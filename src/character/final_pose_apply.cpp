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
    driver.set_tick_enabled(next == Drive::e_tick || next == Drive::e_tick_apply);
    if (next == Drive::e_step)
        driver.request_step();

    // A new drive state re-verifies: the mapping is a property of the Actor and its graph generation,
    // and an operator who has just changed the state is exactly who should see it checked again.
    m_verified = false;
    m_blocked = next == Drive::e_tick_apply ? "verifying" : "not-armed";
    m_step_pending = next == Drive::e_step;

    logger::info("SCOPY AFP2 drive={} apply={} tick={} ({})",
        drive_name(next),
        next == Drive::e_tick_apply ? "on" : "off",
        driver.tick_enabled() ? "on" : "off",
        next == Drive::e_step ? "one declared step, then frozen" : "F3 cycles off -> tick -> tick+apply -> step");
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

    // The generation this verification belongs to: a new root or graph invalidates it wholesale, so
    // the same event the instrument numbers is the event that forces a re-verify here.
    const std::uint32_t generation = m_generation + 1;
    m_generation = generation;

    // The live output pose. The buffer is identified by its content, never by its index: AFP1 saw the
    // live one in slot 0 every time while the two buffer objects themselves moved.
    const RE::hkQsTransform* pose = nullptr;
    std::int32_t count = 0;
    for (int slot = 0; slot < Output_Slots; ++slot)
    {
        OutputTrackView view;
        const RE::hkQsTransform* candidate = nullptr;
        std::int32_t candidate_count = 0;
        if (read_pose_track(*graph, slot, view, candidate, candidate_count) && candidate_count > 0)
        {
            pose = candidate;
            count = candidate_count;
            break;
        }
    }
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

    // Candidate two: pose index i drives the bone whose NAME the animation skeleton gives that index,
    // resolved through the engine's own bone table. A name bridge is required rather than assumed -
    // HKX1 measured the two orders as different, and AFP1 measured this output pose as the skeleton's.
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
    if (skeleton && !skeleton->bones.empty())
    {
        std::unordered_map<std::string_view, std::int32_t> table_by_name;
        table_by_name.reserve(by_bone_nodes.locals.size());
        for (std::int32_t i = 0; i < static_cast<std::int32_t>(by_bone_nodes.locals.size()); ++i)
        {
            RE::NiNode* const node = graph->boneNodes[i].node;
            if (!node || !readable(node, sizeof(RE::NiAVObject)))
                continue;
            const char* const name = node->name.c_str();
            if (name && *name)
                table_by_name.try_emplace(std::string_view(name), i);
        }

        for (std::int32_t i = 0; i < static_cast<std::int32_t>(samples.size()); ++i)
        {
            const std::size_t index = static_cast<std::size_t>(i);
            const char* const name = i < static_cast<std::int32_t>(skeleton->bones.size())
                ? skeleton->bones[i].name.c_str()
                : nullptr;
            const auto found = name && *name ? table_by_name.find(std::string_view(name)) : table_by_name.end();
            if (found == table_by_name.end())
                continue;
            by_skeleton.targets[index] = found->second;
            by_skeleton.locals[index] = by_bone_nodes.locals[static_cast<std::size_t>(found->second)];
        }
    }

    const OrderScore bone_score = score_pose_order(samples, by_bone_nodes.locals, Pose_Match_Degrees);
    const OrderScore skeleton_score = score_pose_order(samples, by_skeleton.locals, Pose_Match_Degrees);
    const OrderScore scores[2] = { bone_score, skeleton_score };
    const OrderDecision decision = decide_pose_order(scores);
    ++m_stats.verifications;

    logger::info("SCOPY AFP2 verify generation={} samples={} bone-nodes within-1deg={}/{} ratio={:.2f} | skeleton within-1deg={}/{} ratio={:.2f} | match={} reason={}",
        generation, samples.size(),
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

    const RE::hkQsTransform* pose = nullptr;
    std::int32_t count = 0;
    for (int slot = 0; slot < Output_Slots; ++slot)
    {
        OutputTrackView view;
        const RE::hkQsTransform* candidate = nullptr;
        std::int32_t candidate_count = 0;
        if (read_pose_track(*graph, slot, view, candidate, candidate_count) && candidate_count > 0)
        {
            pose = candidate;
            count = candidate_count;
            break;
        }
    }
    if (!pose)
    {
        block("no-pose");
        return;
    }

    Stats stats;
    const std::size_t writable = std::min(m_targets.size(), static_cast<std::size_t>(count));
    // The reference for "what the write changed" is the node as the engine left it this frame.
    for (std::size_t i = 0; i < writable; ++i)
    {
        const std::int32_t target = m_targets[i];
        if (target < 0)
        {
            ++stats.unreachable;
            continue;
        }
        if (target >= static_cast<std::int32_t>(graph->boneNodes.size()))
        {
            ++stats.unreachable;
            continue;
        }
        RE::NiNode* const node = graph->boneNodes[static_cast<std::size_t>(target)].node;
        if (!node)
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
    const Drive drive = this->drive();
    // The step state runs exactly one frame of work; the continuous state runs every frame.
    const bool step_frame = m_step_pending;
    if (drive != Drive::e_tick_apply && !step_frame)
        return;

    RE::Actor* const puppet = PuppetDriver::instance().actor();
    if (!puppet || !readable(puppet, sizeof(RE::Actor)))
    {
        block("no-puppet");
        return;
    }

    // R03 then R04: verify while the engine still applies the pose (its locals are then the truth to
    // compare against), write while it does not. After the first write the node locals are ours, so a
    // re-score would compare the pose with itself - which is why verification is not repeated per
    // frame. A single step is a write frame too, so it obeys the same rule.
    if (!m_verified)
    {
        if (a_paused)
            block("unverified-world-first");
        else
            verify(*puppet);
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
