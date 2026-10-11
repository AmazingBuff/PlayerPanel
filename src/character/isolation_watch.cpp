//
// Created by AmazingBuff on 2026/10/11.
//

#include "character/isolation_watch.h"

#include "character/animation_source.h"

#include "RE/B/BSAnimationGraphManager.h"
#include "RE/B/BShkbAnimationGraph.h"
#include "RE/H/hkbCharacter.h"
#include "RE/P/ProcessLists.h"
#include "RE/T/TESObjectREFR.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

// winnt.h defines macros of these names, which would rewrite the REX constants used below as
// `REX::W32::0x1000`; every Windows header this file needs is already parsed at this point.
#undef MEM_COMMIT
#undef PAGE_NOACCESS

PLUGIN_NAMESPACE_BEGIN

namespace
{
    // A census walks the player's whole third-person graph, so the cap doubles as the cycle guard
    // the capture's graph inventory has. Graphs seen so far hold about five hundred nodes.
    constexpr std::uint32_t Max_Census_Nodes = 2048;
    // The pose digest reads hkbCharacter::poseLocal, one hkQsTransform per bone; characters seen
    // so far carry 90-116. The cap keeps a runaway count from growing the census unboundedly.
    constexpr std::uint32_t Max_Pose_Bones = 256;
    // Nearby actors: the high and middle-high process buckets, radius-filtered around the player.
    constexpr std::size_t Max_Watched_Npcs = 24;
    constexpr float Watch_Radius = 6144.0f;
    // The tolerance the capture's SOURCE-DIFF settled on: the paused world still recomputes world
    // transforms with a few ULPs of jitter, so drift under the loose bar is observation, and only
    // drift past it - or an object-set change - is a violation. The pose floats live in Havok
    // units and unit quaternions, so the same bar applies there.
    constexpr float Loose_Epsilon = 0.001f;
    constexpr float Tight_Epsilon = 0.000001f;

    // Same discipline as animation_source.cpp: a read only happens when the pages are committed
    // and readable, so a stale pointer counts as unreadable instead of crashing. Nothing here
    // writes, and nothing here runs engine update code.
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

    float transform_max_delta(RE::NiTransform const& first, RE::NiTransform const& second)
    {
        float worst = 0.0f;
        for (std::size_t row = 0; row < 3; ++row)
            for (std::size_t column = 0; column < 3; ++column)
                worst = std::max(worst, std::abs(first.rotate.entry[row][column] - second.rotate.entry[row][column]));
        worst = std::max(worst, std::abs(first.translate.x - second.translate.x));
        worst = std::max(worst, std::abs(first.translate.y - second.translate.y));
        worst = std::max(worst, std::abs(first.translate.z - second.translate.z));
        worst = std::max(worst, std::abs(first.scale - second.scale));
        return worst;
    }

    float position_max_delta(RE::NiPoint3 const& first, RE::NiPoint3 const& second)
    {
        return std::max({ std::abs(first.x - second.x), std::abs(first.y - second.y), std::abs(first.z - second.z) });
    }

    std::string form_id_text(std::uint32_t form_id)
    {
        return fmt::format("{:08x}", form_id);
    }
}

// One read of the world the panel must not touch: identity and per-node state of O's third-person
// graph, the engine pose behind it, O's reference position, and the nearby actors. Everything it
// holds is a plain copy; no pointer in here outlives the function that filled it.
struct IsolationWatch::Census
{
    // One nearby actor's frozen state: identity by form id, the reference's own position and
    // heading, and the world transform of its 3D root when one is attached.
    struct Npc
    {
        std::uint32_t form_id = 0;
        bool disabled = false;
        bool deleted = false;
        bool has_3d = false;
        RE::NiPoint3 position{};
        float angle_z = 0.0f;
        RE::NiTransform root_world{};
    };

    bool valid = false;
    std::string unavailable;

    // O's third-person graph: identity first (root, parent, node set), then per-node state.
    const RE::NiAVObject* root = nullptr;
    RE::NiNode* parent = nullptr;
    std::uint32_t nodes = 0;
    std::vector<const RE::NiAVObject*> node_ids;
    std::vector<std::string> node_names;
    std::vector<RE::NiTransform> locals;
    std::vector<RE::NiTransform> worlds;
    std::vector<RE::NiBound> bounds;
    // O's engine pose: hkbCharacter::poseLocal, the same array the panel's animation sampling reads.
    bool pose_valid = false;
    std::uint32_t pose_bones = 0;
    std::string pose_note;
    std::vector<float> pose;  // 8 floats per bone: position, quaternion, scale
    // O's reference itself.
    bool actor_valid = false;
    RE::NiPoint3 actor_position{};
    float actor_angle_z = 0.0f;
    // The nearby actors, sorted by form id so set comparison is a merge walk.
    std::vector<Npc> npcs;
    std::size_t npc_beyond_radius = 0;
};

namespace
{
    using Census = IsolationWatch::Census;

    void walk_graph(RE::NiAVObject& root, Census& out)
    {
        std::vector<RE::NiAVObject*> pending{ &root };
        while (!pending.empty() && out.node_ids.size() < Max_Census_Nodes)
        {
            RE::NiAVObject* const object = pending.back();
            pending.pop_back();
            out.node_ids.push_back(object);
            const char* const name = object->name.c_str();
            out.node_names.emplace_back(name ? name : "");
            out.locals.push_back(object->local);
            out.worlds.push_back(object->world);
            out.bounds.push_back(object->worldBound);
            if (RE::NiNode* const node = object->AsNode())
                for (const RE::NiPointer<RE::NiAVObject>& child : node->children)
                    if (child)
                        pending.push_back(child.get());
        }
        out.nodes = static_cast<std::uint32_t>(out.node_ids.size());
    }

    void read_pose_digest(RE::PlayerCharacter& player, const RE::NiAVObject* root, Census& out)
    {
        if (!root)
        {
            out.pose_note = "no-3d";
            return;
        }
        RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
        if (!player.GetAnimationGraphManager(manager) || !manager)
        {
            out.pose_note = "no-graph-manager";
            return;
        }
        for (const RE::BSTSmartPointer<RE::BShkbAnimationGraph>& holder : manager->graphs)
        {
            RE::BShkbAnimationGraph* const graph = holder.get();
            if (!graph || graph->rootNode != root)
                continue;
            RE::hkbCharacter& character = graph->characterInstance;
            const RE::hkQsTransform* const pose = character.poseLocal;
            const std::int32_t count = character.numPoseLocal;
            if (!pose || count <= 0)
            {
                out.pose_note = "no-pose-local";
                return;
            }
            if (count > static_cast<std::int32_t>(Max_Pose_Bones))
            {
                out.pose_note = "bone-count-capped";
                return;
            }
            if (!readable(pose, static_cast<std::size_t>(count) * sizeof(RE::hkQsTransform)))
            {
                out.pose_note = "unreadable";
                return;
            }
            out.pose.reserve(static_cast<std::size_t>(count) * 8);
            for (std::int32_t i = 0; i < count; ++i)
            {
                const PoseSample sample = read_pose(pose[i]);
                const float values[8] = {
                    sample.position[0], sample.position[1], sample.position[2],
                    sample.quaternion[0], sample.quaternion[1], sample.quaternion[2], sample.quaternion[3],
                    sample.scale,
                };
                out.pose.insert(out.pose.end(), values, values + 8);
            }
            out.pose_valid = true;
            out.pose_bones = static_cast<std::uint32_t>(count);
            return;
        }
        out.pose_note = "no-third-person-graph";
    }

    void census_npcs(RE::PlayerCharacter& player, Census& out)
    {
        RE::ProcessLists* const lists = RE::ProcessLists::GetSingleton();
        if (!lists)
            return;
        const RE::NiPoint3 origin = player.GetPosition();
        const std::uint32_t player_id = player.GetFormID();
        for (const RE::BSTArray<RE::ActorHandle>* const bucket : { &lists->highActorHandles, &lists->middleHighActorHandles })
        {
            for (const RE::ActorHandle& handle : *bucket)
            {
                RE::Actor* const actor = handle.get().get();
                if (!actor || actor->GetFormID() == player_id)
                    continue;
                const RE::NiPoint3 position = actor->GetPosition();
                if (position.GetDistance(origin) > Watch_Radius)
                {
                    ++out.npc_beyond_radius;
                    continue;
                }
                if (out.npcs.size() >= Max_Watched_Npcs)
                    continue;
                Census::Npc sample{};
                sample.form_id = actor->GetFormID();
                sample.disabled = actor->IsDisabled();
                sample.deleted = actor->IsMarkedForDeletion();
                sample.position = position;
                sample.angle_z = actor->GetAngleZ();
                if (RE::NiAVObject* const root_3d = actor->Get3D(false))
                {
                    sample.has_3d = true;
                    sample.root_world = root_3d->world;
                }
                out.npcs.push_back(sample);
            }
        }
        std::sort(out.npcs.begin(), out.npcs.end(), [](const Census::Npc& first, const Census::Npc& second) { return first.form_id < second.form_id; });
        out.npcs.erase(std::unique(out.npcs.begin(), out.npcs.end(), [](const Census::Npc& first, const Census::Npc& second) { return first.form_id == second.form_id; }), out.npcs.end());
    }

    // Everything below is worded so a reader can decide H05 from the log alone: each line names
    // the window, the quantity and how far it moved, and the verdict words are CLEAN (nothing
    // past the loose bar) and DRIFT (something did).
    struct Delta
    {
        // Graph identity.
        bool same_set = true;
        bool root_same = true;
        bool parent_same = true;
        std::uint32_t nodes_before = 0;
        std::uint32_t nodes_after = 0;
        // Per-node transform drift, counted by how far past each bar a node landed.
        std::uint32_t local_tight = 0;
        std::uint32_t local_loose = 0;
        std::uint32_t world_tight = 0;
        std::uint32_t world_loose = 0;
        std::uint32_t bound_loose = 0;
        float max_local = 0.0f;
        float max_world = 0.0f;
        std::string first_local;
        std::string first_world;
        // The engine pose behind the graph.
        bool pose_compared = false;
        bool pose_same = true;
        std::uint32_t pose_bones_before = 0;
        std::uint32_t pose_bones_after = 0;
        float max_pose = 0.0f;
        // The player reference itself.
        bool actor_compared = false;
        bool actor_same = true;
        float actor_pos_max = 0.0f;
        float actor_rot_max = 0.0f;
        // The nearby actors. Set changes are streaming, not panel work, so they are counted and
        // named but never fail a window; movement and state changes do.
        std::uint32_t npc_missing = 0;
        std::uint32_t npc_added = 0;
        std::string npc_set_note;
        std::uint32_t npc_flag_changes = 0;
        std::uint32_t npc_3d_changes = 0;
        float npc_pos_max = 0.0f;
        float npc_rot_max = 0.0f;
        float npc_world_max = 0.0f;

        [[nodiscard]] bool clean() const
        {
            return same_set && root_same && parent_same && local_loose == 0 && world_loose == 0 && bound_loose == 0 &&
                   (!pose_compared || (pose_same && max_pose <= Loose_Epsilon)) && actor_same &&
                   npc_pos_max <= Loose_Epsilon && npc_rot_max <= Loose_Epsilon && npc_world_max <= Loose_Epsilon &&
                   npc_flag_changes == 0;
        }
    };

    Delta diff(Census const& before, Census const& after)
    {
        Delta delta;
        delta.nodes_before = before.nodes;
        delta.nodes_after = after.nodes;
        delta.root_same = before.root == after.root;
        delta.parent_same = before.parent == after.parent;

        delta.same_set = before.nodes == after.nodes;
        if (delta.same_set)
            for (std::uint32_t i = 0; i < before.nodes; ++i)
                if (before.node_ids[i] != after.node_ids[i])
                {
                    delta.same_set = false;
                    break;
                }

        if (delta.same_set)
        {
            for (std::uint32_t i = 0; i < before.nodes; ++i)
            {
                const float local = transform_max_delta(before.locals[i], after.locals[i]);
                const float world = transform_max_delta(before.worlds[i], after.worlds[i]);
                if (local > delta.max_local)
                {
                    delta.max_local = local;
                    if (local > Tight_Epsilon)
                        delta.first_local = before.node_names[i];
                }
                if (world > delta.max_world)
                {
                    delta.max_world = world;
                    if (world > Tight_Epsilon)
                        delta.first_world = before.node_names[i];
                }
                delta.local_tight += local > Tight_Epsilon ? 1u : 0u;
                delta.local_loose += local > Loose_Epsilon ? 1u : 0u;
                delta.world_tight += world > Tight_Epsilon ? 1u : 0u;
                delta.world_loose += world > Loose_Epsilon ? 1u : 0u;
                if (before.bounds[i].center.GetDistance(after.bounds[i].center) > Loose_Epsilon ||
                    std::abs(before.bounds[i].radius - after.bounds[i].radius) > Loose_Epsilon)
                    ++delta.bound_loose;
            }
        }

        delta.pose_compared = before.pose_valid && after.pose_valid;
        if (before.pose_valid != after.pose_valid)
            delta.pose_same = false;
        else if (delta.pose_compared)
        {
            delta.pose_bones_before = before.pose_bones;
            delta.pose_bones_after = after.pose_bones;
            if (before.pose.size() != after.pose.size())
                delta.pose_same = false;
            else
            {
                for (std::size_t i = 0; i < before.pose.size(); ++i)
                    delta.max_pose = std::max(delta.max_pose, std::abs(before.pose[i] - after.pose[i]));
                delta.pose_same = delta.max_pose <= Loose_Epsilon;
            }
        }

        if (before.actor_valid && after.actor_valid)
        {
            delta.actor_compared = true;
            delta.actor_pos_max = position_max_delta(before.actor_position, after.actor_position);
            delta.actor_rot_max = std::abs(before.actor_angle_z - after.actor_angle_z);
            delta.actor_same = delta.actor_pos_max <= Loose_Epsilon && delta.actor_rot_max <= Loose_Epsilon;
        }

        std::size_t i = 0;
        std::size_t j = 0;
        while (i < before.npcs.size() && j < after.npcs.size())
        {
            const Census::Npc& first = before.npcs[i];
            const Census::Npc& second = after.npcs[j];
            if (first.form_id == second.form_id)
            {
                delta.npc_pos_max = std::max(delta.npc_pos_max, position_max_delta(first.position, second.position));
                delta.npc_rot_max = std::max(delta.npc_rot_max, std::abs(first.angle_z - second.angle_z));
                delta.npc_flag_changes += (first.disabled != second.disabled ? 1u : 0u) + (first.deleted != second.deleted ? 1u : 0u);
                if (first.has_3d != second.has_3d)
                    ++delta.npc_3d_changes;
                else if (first.has_3d)
                    delta.npc_world_max = std::max(delta.npc_world_max, transform_max_delta(first.root_world, second.root_world));
                ++i;
                ++j;
            }
            else if (first.form_id < second.form_id)
            {
                ++delta.npc_missing;
                if (delta.npc_missing <= 3)
                    delta.npc_set_note += fmt::format("{}-missing ", form_id_text(first.form_id));
                ++i;
            }
            else
            {
                ++delta.npc_added;
                if (delta.npc_added <= 3)
                    delta.npc_set_note += fmt::format("{}-added ", form_id_text(second.form_id));
                ++j;
            }
        }
        for (; i < before.npcs.size(); ++i)
        {
            ++delta.npc_missing;
            if (delta.npc_missing <= 3)
                delta.npc_set_note += fmt::format("{}-missing ", form_id_text(before.npcs[i].form_id));
        }
        for (; j < after.npcs.size(); ++j)
        {
            ++delta.npc_added;
            if (delta.npc_added <= 3)
                delta.npc_set_note += fmt::format("{}-added ", form_id_text(after.npcs[j].form_id));
        }
        return delta;
    }

    char const* verdict_word(bool clean)
    {
        return clean ? "CLEAN" : "DRIFT";
    }

    void log_window(char const* operation, char const* stage, std::string const& window, Delta const& window_delta, Delta const& session_delta)
    {
        const bool clean_now = window_delta.clean();
        const bool clean_session = session_delta.clean();
        const std::string line = fmt::format(
            "SCOPY ISOLATION op={} stage={} window='{}' verdict={} cum={} nodes={}/{} max-local={:.6f} max-world={:.6f} max-pose={:.6f} actor-pos={:.6f} actor-rot={:.6f} npc-pos={:.6f} npc-rot={:.6f} npc-world={:.6f} npc-set=+{}/-{} flags={} attach={}",
            operation, stage, window, verdict_word(clean_now), verdict_word(clean_session),
            window_delta.nodes_before, window_delta.nodes_after, window_delta.max_local, window_delta.max_world, window_delta.max_pose,
            window_delta.actor_pos_max, window_delta.actor_rot_max, window_delta.npc_pos_max, window_delta.npc_rot_max, window_delta.npc_world_max,
            window_delta.npc_added, window_delta.npc_missing, window_delta.npc_flag_changes, window_delta.npc_3d_changes);
        if (clean_now && clean_session)
            logger::info("{}", line);
        else
            logger::error("{}", line);
    }

    void report_drift(char const* operation, char const* stage, Delta const& delta)
    {
        if (delta.local_loose)
            logger::error("SCOPY ISOLATION DRIFT op={} stage={} kind=node-local node='{}' max={:.6f} loose-count={}",
                operation, stage, delta.first_local, delta.max_local, delta.local_loose);
        else if (delta.local_tight)
            logger::info("SCOPY ISOLATION jitter op={} stage={} kind=node-local node='{}' max={:.6f} tight-count={}",
                operation, stage, delta.first_local, delta.max_local, delta.local_tight);
        if (delta.world_loose)
            logger::error("SCOPY ISOLATION DRIFT op={} stage={} kind=node-world node='{}' max={:.6f} loose-count={}",
                operation, stage, delta.first_world, delta.max_world, delta.world_loose);
        else if (delta.world_tight)
            logger::info("SCOPY ISOLATION jitter op={} stage={} kind=node-world node='{}' max={:.6f} tight-count={}",
                operation, stage, delta.first_world, delta.max_world, delta.world_tight);
        if (delta.bound_loose)
            logger::error("SCOPY ISOLATION DRIFT op={} stage={} kind=world-bound count={}", operation, stage, delta.bound_loose);
        if (!delta.same_set)
            logger::error("SCOPY ISOLATION DRIFT op={} stage={} kind=object-set nodes={}/{}", operation, stage, delta.nodes_before, delta.nodes_after);
        if (!delta.root_same)
            logger::error("SCOPY ISOLATION DRIFT op={} stage={} kind=root changed", operation, stage);
        if (!delta.parent_same)
            logger::error("SCOPY ISOLATION DRIFT op={} stage={} kind=parent changed", operation, stage);
        if (delta.pose_compared && !delta.pose_same)
            logger::error("SCOPY ISOLATION DRIFT op={} stage={} kind=engine-pose bones={}/{} max={:.6f}",
                operation, stage, delta.pose_bones_before, delta.pose_bones_after, delta.max_pose);
        if (!delta.actor_same)
            logger::error("SCOPY ISOLATION DRIFT op={} stage={} kind=actor-position pos-max={:.6f} rot-max={:.6f}",
                operation, stage, delta.actor_pos_max, delta.actor_rot_max);
        if (delta.npc_pos_max > Loose_Epsilon || delta.npc_rot_max > Loose_Epsilon)
            logger::error("SCOPY ISOLATION DRIFT op={} stage={} kind=npc-position pos-max={:.6f} rot-max={:.6f}",
                operation, stage, delta.npc_pos_max, delta.npc_rot_max);
        if (delta.npc_world_max > Loose_Epsilon)
            logger::error("SCOPY ISOLATION DRIFT op={} stage={} kind=npc-root-world max={:.6f}", operation, stage, delta.npc_world_max);
        if (delta.npc_flag_changes)
            logger::error("SCOPY ISOLATION DRIFT op={} stage={} kind=npc-state count={}", operation, stage, delta.npc_flag_changes);
        if (delta.npc_3d_changes)
            logger::info("SCOPY ISOLATION jitter op={} stage={} kind=npc-3d-attach count={}", operation, stage, delta.npc_3d_changes);
        if (!delta.npc_set_note.empty())
            logger::info("SCOPY ISOLATION jitter op={} stage={} kind=npc-set {}", operation, stage, delta.npc_set_note);
    }
}

struct IsolationWatch::State
{
    bool baseline_taken = false;
    bool has_previous = false;
    std::string last_op = "session-start";
    std::uint32_t operations = 0;
    std::uint32_t censuses = 0;
    bool incomplete = false;
    bool session_clean = true;
    // Session maxima; drift and jitter both land here, and the summary reports them as numbers.
    float max_local = 0.0f;
    float max_world = 0.0f;
    float max_pose = 0.0f;
    float max_actor_pos = 0.0f;
    float max_actor_rot = 0.0f;
    float max_npc_pos = 0.0f;
    float max_npc_rot = 0.0f;
    float max_npc_world = 0.0f;
    std::uint32_t npc_missing_total = 0;
    std::uint32_t npc_added_total = 0;
    std::uint32_t npc_3d_changes_total = 0;
    std::string notes;
    Census baseline;
    Census previous;
};

IsolationWatch::IsolationWatch() : m_state(std::make_unique<State>()) {}

IsolationWatch::~IsolationWatch() = default;

bool IsolationWatch::armed() const
{
    return m_armed.load(std::memory_order_acquire);
}

void IsolationWatch::request_end()
{
    m_end_requested.store(true, std::memory_order_release);
}

bool IsolationWatch::take_end_request()
{
    return m_end_requested.exchange(false, std::memory_order_acq_rel);
}

void IsolationWatch::census_pre(char const* operation)
{
    record(operation, "pre");
}

void IsolationWatch::census_post(char const* operation)
{
    record(operation, "post");
}

void IsolationWatch::take(Census& out)
{
    RE::PlayerCharacter* const player = RE::PlayerCharacter::GetSingleton();
    if (!player)
    {
        out.unavailable = "no-player";
        return;
    }
    out.actor_valid = true;
    out.actor_position = player->GetPosition();
    out.actor_angle_z = player->GetAngleZ();
    if (RE::NiAVObject* const root = player->Get3D(false))
    {
        out.root = root;
        out.parent = root->parent;
        walk_graph(*root, out);
        if (out.nodes >= Max_Census_Nodes)
        {
            out.unavailable = "graph-capped";
            return;
        }
        read_pose_digest(*player, root, out);
    }
    else
        out.pose_note = "no-3d";
    census_npcs(*player, out);
    out.valid = true;
}

void IsolationWatch::record(char const* operation, char const* stage)
{
    // Arming happens here rather than at menu-open: the first census of a session is the baseline,
    // and until an operation runs there is nothing the instrument is responsible for. end() clears
    // the flag through its own exchange, so a queued end request cannot resurrect the session.
    m_armed.store(true, std::memory_order_release);
    State& state = *m_state;
    Census now;
    take(now);
    if (!now.valid)
    {
        state.incomplete = true;
        state.has_previous = false;
        logger::warn("SCOPY ISOLATION op={} stage={} UNAVAILABLE reason={} (the session verdict becomes INCOMPLETE)", operation, stage, now.unavailable);
        return;
    }
    ++state.censuses;
    if (!state.baseline_taken)
    {
        state.baseline = std::move(now);
        state.baseline_taken = true;
        state.previous = state.baseline;
        state.has_previous = true;
        state.last_op = operation;
        logger::info("SCOPY ISOLATION baseline op={} nodes={} npcs={} pose-bones={} pose-note='{}' root={} parent={} beyond-radius={}",
            operation, state.baseline.nodes, state.baseline.npcs.size(),
            state.baseline.pose_valid ? state.baseline.pose_bones : 0, state.baseline.pose_note,
            static_cast<void const*>(state.baseline.root), static_cast<void const*>(state.baseline.parent), state.baseline.npc_beyond_radius);
        return;
    }

    const Delta window_delta = state.has_previous ? diff(state.previous, now) : diff(state.baseline, now);
    const Delta session_delta = diff(state.baseline, now);
    const std::string window = std::string_view(stage) == "pre" ? fmt::format("{}→{}", state.last_op, operation) : fmt::format("{}:pre→post", operation);
    log_window(operation, stage, window, window_delta, session_delta);
    if (!window_delta.clean() || !session_delta.clean())
    {
        state.session_clean = false;
        report_drift(operation, stage, !window_delta.clean() ? window_delta : session_delta);
        if (state.notes.size() < 220)
            state.notes += fmt::format("[{} {} local={:.4f} world={:.4f}] ", window, stage, window_delta.max_local, window_delta.max_world);
    }
    state.max_local = std::max({ state.max_local, window_delta.max_local, session_delta.max_local });
    state.max_world = std::max({ state.max_world, window_delta.max_world, session_delta.max_world });
    state.max_pose = std::max({ state.max_pose, window_delta.max_pose, session_delta.max_pose });
    state.max_actor_pos = std::max({ state.max_actor_pos, window_delta.actor_pos_max, session_delta.actor_pos_max });
    state.max_actor_rot = std::max({ state.max_actor_rot, window_delta.actor_rot_max, session_delta.actor_rot_max });
    state.max_npc_pos = std::max({ state.max_npc_pos, window_delta.npc_pos_max, session_delta.npc_pos_max });
    state.max_npc_rot = std::max({ state.max_npc_rot, window_delta.npc_rot_max, session_delta.npc_rot_max });
    state.max_npc_world = std::max({ state.max_npc_world, window_delta.npc_world_max, session_delta.npc_world_max });
    state.npc_missing_total += window_delta.npc_missing;
    state.npc_added_total += window_delta.npc_added;
    state.npc_3d_changes_total += window_delta.npc_3d_changes;
    if (std::string_view(stage) == "post")
    {
        ++state.operations;
        state.last_op = operation;
    }
    state.previous = std::move(now);
    state.has_previous = true;
}

void IsolationWatch::end(char const* reason)
{
    if (!m_armed.exchange(false, std::memory_order_acq_rel))
        return;
    m_end_requested.store(false, std::memory_order_release);
    State& state = *m_state;
    if (!state.baseline_taken)
    {
        logger::info("SCOPY ISOLATION end reason={} (no census was taken)", reason);
        return;
    }
    const char* const verdict = state.incomplete ? "INCOMPLETE" : (state.session_clean ? "CLEAN" : "DRIFT");
    logger::info("SCOPY ISOLATION end reason={} ops={} censuses={} verdict={} max-local={:.6f} max-world={:.6f} max-pose={:.6f} actor-pos={:.6f} actor-rot={:.6f} npc-pos={:.6f} npc-rot={:.6f} npc-world={:.6f} npc-set=+{}/-{} attach={} notes='{}'",
        reason, state.operations, state.censuses, verdict, state.max_local, state.max_world, state.max_pose,
        state.max_actor_pos, state.max_actor_rot, state.max_npc_pos, state.max_npc_rot, state.max_npc_world,
        state.npc_added_total, state.npc_missing_total, state.npc_3d_changes_total, state.notes);
    // The next session starts from an empty state no matter how this one ended.
    *m_state = State{};
}

PLUGIN_NAMESPACE_END
