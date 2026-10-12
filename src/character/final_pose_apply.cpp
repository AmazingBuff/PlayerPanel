//
// Created by AmazingBuff on 2026/10/13.
//

#include "character/final_pose_apply.h"

#include "character/final_pose_probe.h"
#include "character/puppet_driver.h"
#include "character/snapshot_transform.h"

#include "RE/A/Actor.h"
#include "RE/B/BShkbAnimationGraph.h"
#include "RE/H/hkaSkeleton.h"
#include "RE/H/hkbCharacter.h"
#include "RE/H/hkbCharacterSetup.h"
#include "RE/P/PlayerCharacter.h"
#include "RE/T/TESObjectREFR.h"

#include <cmath>
#include <cstring>
#include <string_view>
#include <unordered_map>

// winnt.h defines macros of these names, which would rewrite the REX constants used below as
// `REX::W32::0x1000`; every Windows header this file needs is already parsed at this point.
#undef MEM_COMMIT
#undef PAGE_NOACCESS

PLUGIN_NAMESPACE_BEGIN

namespace
{
    constexpr int Output_Slots = 2;
    // How often a blocked stage repeats itself: a refusal that prints every frame is noise, and one
    // that prints once is a fact the operator can miss.
    constexpr std::uint32_t Block_Log_Period = 120;

    bool readable(void const* address, std::size_t bytes)
    {
        if (!address || bytes == 0)
            return false;
        REX::W32::MEMORY_BASIC_INFORMATION info{};
        if (REX::W32::VirtualQuery(address, &info, sizeof(info)) == 0)
            return false;
        if (info.state != REX::W32::MEM_COMMIT || info.protect == REX::W32::PAGE_NOACCESS)
            return false;
        const auto* const begin = static_cast<const std::uint8_t*>(address);
        const auto* const end = static_cast<const std::uint8_t*>(info.baseAddress) + info.regionSize;
        return begin + bytes <= end;
    }

    // One candidate index order, expressed the way the application needs it: for every pose index,
    // which entry of the engine's live bone table that index drives (-1 when nothing resolved), and
    // what the node held when the mapping was verified.
    struct Mapping
    {
        char const* name = "none";
        std::vector<std::int32_t> targets;
        std::vector<TransformSample> locals;
        std::uint32_t unreachable = 0;
    };

    // The live output pose: the buffer that carries elements, identified by its content and never by
    // its index (AFP1 saw the live one in slot 0 in every mark while the two buffer objects themselves
    // moved). Null with `a_count` = 0 when no buffer carries a pose - a state this route has to report
    // rather than treat as an error: the parked puppet and the paused puppet both measured exactly
    // that, and they mean different things.
    const RE::hkQsTransform* live_pose(RE::BShkbAnimationGraph& a_graph, std::int32_t& a_count)
    {
        a_count = 0;
        for (int slot = 0; slot < Output_Slots; ++slot)
        {
            OutputTrackView view;
            const RE::hkQsTransform* candidate = nullptr;
            std::int32_t candidate_count = 0;
            if (read_pose_track(a_graph, slot, view, candidate, candidate_count) && candidate_count > 0)
            {
                a_count = candidate_count;
                return candidate;
            }
        }
        return nullptr;
    }
}

FinalPoseApply& FinalPoseApply::instance()
{
    static FinalPoseApply s_instance;
    return s_instance;
}

char const* FinalPoseApply::drive_name(Drive a_drive)
{
    switch (a_drive)
    {
    case Drive::e_off:
        return "off";
    case Drive::e_tick:
        return "tick";
    case Drive::e_tick_apply:
        return "tick+apply";
    case Drive::e_tick_apply_force:
        return "tick+apply+force";
    case Drive::e_step:
        return "step";
    }
    return "?";
}

void FinalPoseApply::cycle()
{
    const Drive next = [current = drive()]
    {
        switch (current)
        {
        case Drive::e_off:
            return Drive::e_tick;
        case Drive::e_tick:
            return Drive::e_tick_apply;
        case Drive::e_tick_apply:
            return Drive::e_tick_apply_force;
        case Drive::e_tick_apply_force:
            return Drive::e_step;
        case Drive::e_step:
            return Drive::e_off;
        }
        return Drive::e_off;
    }();

    m_drive.store(next, std::memory_order_release);

    // The gate the driver obeys. `step` leaves nothing running: it is a one-shot, so the driver stays
    // closed and the next paused frame advances exactly the declared dt.
    PuppetDriver& driver = PuppetDriver::instance();
    const bool applying = next == Drive::e_tick_apply || next == Drive::e_tick_apply_force;
    driver.set_tick_enabled(next == Drive::e_tick || applying);
    driver.set_force_generate(next == Drive::e_tick_apply_force);
    if (next == Drive::e_step)
        driver.request_step();

    // A drive cycle does NOT re-verify. It used to, and AFP6 showed what that costs: cycling from
    // tick+apply into tick+apply+force threw away a proof taken seconds earlier at 0.98, then the
    // paused forced frame found itself unverified and wrote nothing
    // (`blocked=unverified-world-first` with the pose sitting right there).
    //
    // The mapping is a property of the Actor and its graph generation, and the only event that
    // invalidates it wholesale is that identity changing - which the per-frame identity check below
    // handles. Force changes what the paused panel generates but not what the running engine applies,
    // and the mapping is proved against the latter, so moving the flag is not a reason to lose it.
    m_verify_attempted = false;
    m_blocked = applying ? "verifying" : "not-armed";
    m_step_pending = next == Drive::e_step;

    logger::info("SCOPY AFP2 drive={} apply={} force-generate={} tick={} ({})",
        drive_name(next),
        applying ? "on" : "off",
        next == Drive::e_tick_apply_force ? "on" : "off",
        driver.tick_enabled() ? "on" : "off",
        next == Drive::e_step
            ? "one declared step, then frozen"
            : "F4 cycles off -> tick -> tick+apply -> tick+apply+force -> step");
}

void FinalPoseApply::block(char const* a_reason)
{
    m_blocked = a_reason;
    if (m_log_countdown != 0)
    {
        --m_log_countdown;
        return;
    }
    m_log_countdown = Block_Log_Period;
    logger::info("SCOPY AFP2 apply blocked reason={} mapping={} verified={} generation={}",
        a_reason, m_mapping, m_verified ? 1 : 0, m_generation);
}

void FinalPoseApply::verify(RE::Actor& a_puppet)
{
    m_verified = false;
    m_targets.clear();
    m_mapping = "none";

    RE::BShkbAnimationGraph* const graph = find_third_person_graph(a_puppet);
    if (!graph || !readable(graph, sizeof(RE::BShkbAnimationGraph)))
    {
        block("no-graph");
        return;
    }

    RE::NiAVObject* const root = a_puppet.Get3D(false);
    if (!root || !readable(root, sizeof(RE::NiAVObject)))
    {
        block("no-3d");
        return;
    }

    // The generation is the identity of the Actor's 3D root and its graph - the event that
    // invalidates a mapping wholesale - and not a count of attempts. A failed verification is
    // therefore not retried every frame: it waits for that identity to change or for a new arm, both
    // of which reset the latch that got us here.
    if (m_root != root || m_graph != graph)
    {
        m_root = root;
        m_graph = graph;
        if (m_seen_identity)
            ++m_generation;
        m_seen_identity = true;
    }

    // The live output pose, identified by content (see `live_pose`).
    std::int32_t count = 0;
    const RE::hkQsTransform* const pose = live_pose(*graph, count);
    if (!pose)
    {
        // A live-but-empty pose track is the parked puppet's measured state, and it is a block, not a
        // reason to write something else.
        block("no-pose");
        return;
    }

    std::vector<PoseSample> samples(static_cast<std::size_t>(count));
    for (std::int32_t i = 0; i < count; ++i)
        samples[static_cast<std::size_t>(i)] = read_pose_unaligned(pose[i]);

    // Candidate one: pose index i drives bone table entry i.
    Mapping by_bone_nodes;
    by_bone_nodes.name = "bone-nodes";
    by_bone_nodes.targets.resize(samples.size(), -1);
    by_bone_nodes.locals.resize(graph->boneNodes.size());
    for (std::int32_t i = 0; i < static_cast<std::int32_t>(by_bone_nodes.locals.size()); ++i)
    {
        RE::NiNode* const node = graph->boneNodes[i].node;
        if (!node || !readable(node, sizeof(RE::NiAVObject)))
            continue;
        by_bone_nodes.locals[static_cast<std::size_t>(i)].rotate = node->local.rotate;
        by_bone_nodes.locals[static_cast<std::size_t>(i)].translate = node->local.translate;
        by_bone_nodes.locals[static_cast<std::size_t>(i)].scale = node->local.scale;
        by_bone_nodes.locals[static_cast<std::size_t>(i)].present = true;
        if (static_cast<std::size_t>(i) < by_bone_nodes.targets.size())
            by_bone_nodes.targets[static_cast<std::size_t>(i)] = i;
    }

    // Candidate two: pose index i drives the bone whose NAME the animation skeleton gives that index.
    //
    // The bridge runs over the Actor's own tree, because the engine's bone table does not reliably
    // carry the animation skeleton's names - the 2026-10-13 session measured 37 of 116 on the puppet's
    // reduced graph (numAnimBones 98) against 109 of 116 on the player, on the same tree walk. The
    // resolved node is then mapped back to the table index the per-frame write uses, so the bridge can
    // be the tree's while nothing is held across frames: the table is re-read every frame, and a node
    // the table does not address is counted as unreachable rather than written through a stale pointer.
    const RE::hkaSkeleton* skeleton = nullptr;
    if (readable(&graph->characterInstance, sizeof(RE::hkbCharacter)))
    {
        const RE::hkbCharacter& character = graph->characterInstance;
        if (character.setup && readable(character.setup.get(), sizeof(RE::hkbCharacterSetup)))
        {
            const RE::hkaSkeleton* const candidate = character.setup->animationSkeleton.get();
            if (candidate && readable(candidate, sizeof(RE::hkaSkeleton)))
                skeleton = candidate;
        }
    }

    Mapping by_skeleton;
    by_skeleton.name = "skeleton";
    by_skeleton.targets.resize(samples.size(), -1);
    by_skeleton.locals.resize(samples.size());
    std::uint32_t tree_matched = 0;
    std::uint32_t table_matched = 0;
    if (skeleton && !skeleton->bones.empty())
    {
        std::unordered_map<std::string_view, RE::NiAVObject*> tree_by_name = nodes_by_name(*root);
        std::unordered_map<RE::NiAVObject const*, std::int32_t> table_index;
        table_index.reserve(by_bone_nodes.locals.size());
        for (std::int32_t i = 0; i < static_cast<std::int32_t>(by_bone_nodes.locals.size()); ++i)
        {
            RE::NiNode* const node = graph->boneNodes[i].node;
            if (node)
                table_index.try_emplace(node, i);
        }

        for (std::int32_t i = 0; i < static_cast<std::int32_t>(samples.size()); ++i)
        {
            const char* const name = i < static_cast<std::int32_t>(skeleton->bones.size())
                ? skeleton->bones[i].name.c_str()
                : nullptr;
            const auto found = name && *name ? tree_by_name.find(std::string_view(name)) : tree_by_name.end();
            if (found == tree_by_name.end())
                continue;
            ++tree_matched;

            const std::size_t index = static_cast<std::size_t>(i);
            // The proof of the order is the node's OWN local, read from the tree - which is what the
            // instrument scores (109 of 109) and what this stage used to throw away by insisting the
            // bone also be addressable in the engine's table. The table holds 37 of 116 here, so that
            // insistence turned a full-order proof into a 37-bone one that landed at 0.89 and blocked.
            // The table is still consulted, but for the WRITE: a bone it cannot address is counted as
            // unreachable there rather than excluded from the evidence here.
            if (!found->second || !readable(found->second, sizeof(RE::NiAVObject)))
                continue;
            by_skeleton.locals[index].rotate = found->second->local.rotate;
            by_skeleton.locals[index].translate = found->second->local.translate;
            by_skeleton.locals[index].scale = found->second->local.scale;
            by_skeleton.locals[index].present = true;

            const auto slot = table_index.find(found->second);
            if (slot == table_index.end())
                continue;
            ++table_matched;
            by_skeleton.targets[index] = slot->second;
        }
    }

    const OrderScore bone_score = score_pose_order(samples, by_bone_nodes.locals, Pose_Match_Degrees);
    const OrderScore skeleton_score = score_pose_order(samples, by_skeleton.locals, Pose_Match_Degrees);
    const OrderScore scores[2] = { bone_score, skeleton_score };
    const OrderDecision decision = decide_pose_order(scores);
    ++m_stats.verifications;

    logger::info("SCOPY AFP2 verify generation={} samples={} names skeleton={} tree={} table={} | bone-nodes within-1deg={}/{} ratio={:.2f} | skeleton within-1deg={}/{} ratio={:.2f} | match={} reason={}",
        m_generation, samples.size(),
        skeleton ? skeleton->bones.size() : 0, tree_matched, table_matched,
        bone_score.within, bone_score.compared, bone_score.ratio(),
        skeleton_score.within, skeleton_score.compared, skeleton_score.ratio(),
        decision.match ? 1 : 0, decision.reason);

    if (!decision.match)
    {
        m_blocked = decision.reason;
        return;
    }

    const Mapping& winner = decision.best == 1 ? by_skeleton : by_bone_nodes;
    m_targets = winner.targets;
    m_mapping = winner.name;
    // The per-frame write resolves nodes by name in the tree, so the names are captured once - here,
    // where the skeleton that defines the index order is already in hand. A name is stable across
    // the frame boundary in a way a node pointer is not (HKX43's dangling read came from keeping
    // one), and it reaches the bones the engine's bone table does not address.
    m_bone_names.assign(samples.size(), std::string{});
    if (skeleton && decision.best == 1)
    {
        for (std::size_t i = 0; i < m_bone_names.size(); ++i)
        {
            if (i >= static_cast<std::size_t>(skeleton->bones.size()))
                continue;
            const char* const name = skeleton->bones[static_cast<std::int32_t>(i)].name.c_str();
            if (name && *name)
                m_bone_names[i] = name;
        }
    }
    m_verified = true;
    m_blocked = "verified";
}

void FinalPoseApply::apply(RE::Actor& a_puppet)
{
    // The mapping was verified; the write resolves every node from the live table on THIS frame.
    // Nothing is cached across frames on purpose (HKX43), and nothing is page-queried per bone: the
    // bracket is serialized with the engine's own update, so a pointer read from the live table is
    // valid for the rest of this frame.
    RE::BShkbAnimationGraph* const graph = find_third_person_graph(a_puppet);
    RE::NiAVObject* const root = a_puppet.Get3D(false);
    if (!graph || !root || !readable(graph, sizeof(RE::BShkbAnimationGraph)))
    {
        block("no-graph");
        return;
    }

    std::int32_t count = 0;
    const RE::hkQsTransform* const pose = live_pose(*graph, count);
    if (!pose)
    {
        block("no-pose");
        return;
    }

    Stats stats;
    const std::size_t writable = std::min(m_targets.size(), static_cast<std::size_t>(count));
    // Every node is resolved from the tree on THIS frame, by the name the verification captured:
    // the engine's bone table reaches only 36 of 68 bones here (AFP5: unreachable=32), and a bone
    // the table does not address is still a bone the pose drives. Nothing is held across frames -
    // no node pointer (HKX43) and no per-frame page query (HKX35) - and a name that resolves to
    // nothing is counted, never skipped.
    std::unordered_map<std::string_view, RE::NiAVObject*> tree_by_name;
    if (root && readable(root, sizeof(RE::NiAVObject)))
        tree_by_name = nodes_by_name(*root);

    // The reference for "what the write changed" is the node as the engine left it this frame.
    for (std::size_t i = 0; i < writable; ++i)
    {
        const std::int32_t target = m_targets[i];

        RE::NiAVObject* node = nullptr;
        if (i < m_bone_names.size() && !m_bone_names[i].empty())
        {
            const auto found = tree_by_name.find(std::string_view(m_bone_names[i]));
            if (found != tree_by_name.end())
                node = found->second;
        }
        if (!node && target >= 0 && target < static_cast<std::int32_t>(graph->boneNodes.size()))
        {
            // The table is the fallback, not the gate: it is still index-aligned with the pose and
            // still the cheapest resolution when the tree walk found nothing for that name.
            node = graph->boneNodes[static_cast<std::size_t>(target)].node;
        }
        if (!node || !readable(node, sizeof(RE::NiAVObject)))
        {
            ++stats.unreachable;
            continue;
        }

        const PoseSample sample = read_pose_unaligned(pose[i]);
        RE::NiTransform local = ni_local_from_pose(sample, false);
        // A pose without a usable scale keeps the node's own: a NIF node carries one scale where the
        // pose carries three, and clobbering a real scale would distort the figure rather than report
        // anything (the same rule the measured replay path uses).
        if (!std::isfinite(local.scale) || local.scale <= 0.0f)
            local.scale = node->local.scale;

        stats.max_rotation = std::max(stats.max_rotation, rotation_angle_degrees(node->local.rotate, local.rotate));
        stats.max_position = std::max(stats.max_position, (node->local.translate - local.translate).Length());
        node->local = local;
        ++stats.written;
    }

    if (stats.written != 0)
    {
        // One downward recompute for the whole write: the engine's dirty cascade does not run inside
        // the draw window, and the skeleton's parent indices need not precede their children.
        recompute_subtree_worlds(*root, 0);
        stats.applied = true;
    }

    // The per-frame stats replace the last frame's, but the verification count is about the mapping
    // and survives: a reader needs both "what did this frame write" and "how often was it checked".
    const std::uint32_t verifications = m_stats.verifications;
    m_stats = stats;
    m_stats.verifications = verifications;
    ++m_stats.frames;
}

void FinalPoseApply::tick(bool a_paused)
{
    // The step state runs exactly one frame of work; the continuous states run every frame.
    const bool step_frame = m_step_pending;
    if (!apply_enabled() && !step_frame)
        return;

    RE::Actor* const puppet = PuppetDriver::instance().actor();
    if (!puppet || !readable(puppet, sizeof(RE::Actor)))
    {
        block("no-puppet");
        return;
    }

    // The identity check runs before anything else: a new 3D root or a new graph invalidates the
    // mapping whatever the drive state is, and it is also the event that re-arms a verification that
    // failed (the alternative - retrying every frame - made the 2026-10-13 session's `generation`
    // count 59 in ten seconds, which is a counter of attempts and not an identity).
    RE::NiAVObject* const root = puppet->Get3D(false);
    RE::BShkbAnimationGraph* const graph = find_third_person_graph(*puppet);
    if (m_root != root || m_graph != graph)
    {
        m_root = root;
        m_graph = graph;
        if (m_seen_identity)
            ++m_generation;
        m_seen_identity = true;
        m_verified = false;
        m_verify_attempted = false;
        m_targets.clear();
        m_bone_names.clear();
        m_mapping = "none";
    }

    // R03 then R04: verify while the engine still applies the pose (its locals are then the truth to
    // compare against), write while it does not. After the first write the node locals are ours, so a
    // re-score would compare the pose with itself - which is why verification is not repeated per
    // frame.
    if (!m_verified)
    {
        if (a_paused)
        {
            // Ask whether there is a pose at all BEFORE reporting why nothing was written: "the graph
            // produced nothing" and "the mapping was never verified" are different answers, and the
            // 2026-10-13 session could not tell them apart because the stage refused without looking.
            std::int32_t count = 0;
            live_pose(*graph, count);
            block(count > 0 ? "unverified-world-first" : "no-pose-while-paused");
        }
        else if (!m_verify_attempted)
        {
            // The proof needs the pose and the locals to share an origin, and only the unforced
            // running world gives them one: the engine generated the pose AND applied it. With force
            // on, the panel generates one the engine did not apply, so the score measures that drift
            // instead of the index order - AFP4 watched 0.99 become 0.67 on exactly that frame. The
            // latch therefore stays closed through a force flip; force is a property of the write.
            if (force_generate())
            {
                block("verification-waits-for-unforced-world");
            }
            else
            {
                m_verify_attempted = true;
                verify(*puppet);
            }
        }
        return;
    }

    if (!a_paused)
    {
        // The engine is applying the pose itself; a write here would fight it.
        m_stats.applied = false;
        return;
    }

    apply(*puppet);
    if (step_frame)
        m_step_pending = false;
}

PLUGIN_NAMESPACE_END
