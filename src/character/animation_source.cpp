//
// Created by AmazingBuff on 2026/10/9.
//

#include "character/animation_source.h"

#include "character/snapshot_transform.h"

#include "RE/B/BSAnimationGraphManager.h"
#include "RE/B/BSFadeNode.h"
#include "RE/B/BShkbAnimationGraph.h"
#include "RE/B/BSVisit.h"
#include "RE/H/hkClass.h"
#include "RE/H/hkaAnimation.h"
#include "RE/H/hkaAnimationBinding.h"
#include "RE/H/hkaBone.h"
#include "RE/H/hkaSkeleton.h"
#include "RE/H/hkbAnimationBindingSet.h"
#include "RE/H/hkbBehaviorGraph.h"
#include "RE/H/hkbCharacter.h"
#include "RE/H/hkbCharacterData.h"
#include "RE/H/hkbCharacterSetup.h"
#include "RE/H/hkbCharacterStringData.h"
#include "RE/H/hkbGenerator.h"
#include "RE/T/TESObjectREFR.h"
#include "REL/Module.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// winnt.h defines macros of these names, which would rewrite the REX constants used below as
// `REX::W32::0x1000`; every Windows header this file needs is already parsed at this point.
#undef MEM_COMMIT
#undef PAGE_NOACCESS

PLUGIN_NAMESPACE_BEGIN

namespace
{
    // The log carries a sample of a difference, never all of it: the counts beside it say how many
    // there are.
    constexpr size_t Max_Names_Reported = 8;

    // The control's pose error, applied about each driven bone's local X axis: large enough that no
    // candidate can reproduce the source's pose by accident.
    constexpr float Control_Perturbation_Radians = 0.5f;

    // A candidate reproduces the source when its worst bone lands within this fraction of the
    // control's worst bone. A relative bar, so no absolute tolerance has to be invented for a rig
    // whose units and scale are the game's.
    constexpr float Match_Fraction_Of_Control = 0.01f;

    // Bounds a candidate animation has to stay inside to be believed: no clip the game ships runs for
    // longer than this or animates more tracks than this, so leaving the range means the layout is
    // wrong rather than that a strange clip was found.
    constexpr float Max_Clip_Seconds = 600.0f;
    constexpr std::int32_t Max_Tracks_Read = 4096;

    // How many of the binding set's entries the catalogue samples, and how many names it prints.
    constexpr size_t Catalogue_Sample_Count = 8;
    constexpr size_t Max_Catalogue_Names = 8;

    struct NodeTable
    {
        std::vector<RE::NiAVObject*> objects;
        std::vector<std::string_view> names;
        std::vector<std::int32_t> parents;
    };

    NodeTable collect_nodes(RE::NiAVObject& root)
    {
        std::vector<RE::NiAVObject*> objects;
        RE::BSVisit::TraverseScenegraphObjects(&root, [&](RE::NiAVObject* object)
        {
            objects.push_back(object);
            return RE::BSVisit::BSVisitControl::kContinue;
        });

        std::unordered_map<RE::NiAVObject*, std::int32_t> index_of;
        index_of.reserve(objects.size());
        for (size_t i = 0; i < objects.size(); ++i)
            index_of.emplace(objects[i], static_cast<std::int32_t>(i));

        NodeTable table;
        table.objects = objects;
        table.names.reserve(objects.size());
        table.parents.reserve(objects.size());
        for (RE::NiAVObject* object : objects)
        {
            const char* const name = object->name.c_str();
            table.names.emplace_back(name ? name : "");
            const auto parent = object->parent ? index_of.find(object->parent) : index_of.end();
            table.parents.push_back(parent != index_of.end() ? parent->second : -1);
        }
        return table;
    }

    // The graph whose skeleton resolves the most bones, with what it resolved to.
    struct SelectedGraph
    {
        RE::BShkbAnimationGraph* graph;
        const RE::hkaSkeleton* skeleton;
        SkeletonAlignment alignment;
        size_t index;
    };

    // One behaviour graph's alignment, with its evidence printed when `verbose` asks for it: the
    // report prints every graph, while the later stages select the best one silently. A graph with no
    // usable skeleton comes back with bones == 0.
    SkeletonAlignment report_graph(RE::BShkbAnimationGraph& graph, size_t index, RE::TESObjectREFR& source, RE::NiAVObject& source_root, NodeTable const& copy_nodes, const RE::hkaSkeleton*& skeleton_out, bool verbose)
    {
        RE::hkbCharacter& character = graph.characterInstance;
        RE::hkbCharacterSetup* const setup = character.setup.get();
        RE::hkbBehaviorGraph* const behavior = character.behaviorGraph.get();
        RE::hkbAnimationBindingSet* const bindings = character.animationBindingSet.get();

        const char* generator_class = "none";
        if (behavior)
        {
            RE::hkbGenerator* const generator = behavior->rootGenerator.get();
            if (generator)
            {
                const RE::hkClass* const type = generator->GetClassType();
                if (type && type->name)
                    generator_class = type->name;
            }
        }

        // `holder` and `rootNode` name the character and the graph this behaviour graph drives: the
        // captured third-person graph is one of the graphs a character holds, and knowing which one
        // keeps a first-person or weapon graph from being aligned by accident.
        if (verbose)
        {
            logger::info("SCOPY ANIM graph[{}] project='{}' holder={} root={} bone-nodes={} anim-bones={} behavior-graph={} root-generator='{}' binding-set={} bindings={} pose-local={}",
                index,
                graph.projectName.c_str() ? graph.projectName.c_str() : "",
                graph.holder && static_cast<RE::TESObjectREFR*>(graph.holder) == &source,
                graph.rootNode && static_cast<RE::NiAVObject*>(graph.rootNode) == &source_root,
                graph.boneNodes.size(),
                graph.numAnimBones,
                behavior != nullptr,
                generator_class,
                bindings != nullptr,
                bindings ? bindings->bindings.size() : 0,
                character.numPoseLocal);
        }

        if (!setup)
        {
            if (verbose)
                logger::info("SCOPY ANIM graph[{}] skeleton=UNAVAILABLE reason=null-character-setup", index);
            return SkeletonAlignment{};
        }
        const RE::hkaSkeleton* const skeleton = setup->animationSkeleton.get();
        if (!skeleton)
        {
            if (verbose)
                logger::info("SCOPY ANIM graph[{}] skeleton=UNAVAILABLE reason=null-animation-skeleton", index);
            return SkeletonAlignment{};
        }
        if (skeleton->bones.empty())
        {
            if (verbose)
                logger::info("SCOPY ANIM graph[{}] skeleton=UNAVAILABLE reason=empty-animation-skeleton", index);
            return SkeletonAlignment{};
        }

        std::vector<std::string_view> bone_names;
        bone_names.reserve(skeleton->bones.size());
        for (RE::hkaBone const& bone : skeleton->bones)
        {
            const char* const name = bone.name.c_str();
            bone_names.emplace_back(name ? name : "");
        }
        const std::vector<std::int16_t> bone_parents(skeleton->parentIndices.begin(), skeleton->parentIndices.end());

        const SkeletonAlignment alignment = align_skeleton(bone_names, bone_parents, copy_nodes.names, copy_nodes.parents, {}, Max_Names_Reported);

        // The engine's own bone table should be index-aligned with its animation skeleton. That is
        // what a pointer-based mapping would rest on, so it is measured rather than assumed.
        const uint32_t comparable = std::min<uint32_t>(static_cast<uint32_t>(graph.boneNodes.size()), static_cast<uint32_t>(bone_names.size()));
        size_t agree = 0;
        for (uint32_t i = 0; i < comparable; ++i)
        {
            RE::NiNode* const node = graph.boneNodes[i].node;
            const char* const name = node ? node->name.c_str() : nullptr;
            if (name && std::string_view(name) == bone_names[i])
                ++agree;
        }

        if (verbose)
        {
            logger::info("SCOPY ANIM skeleton graph={} name='{}' bones={} bone-nodes={} bone-node-names-agree={}/{} matched={} ambiguous={} duplicate-nodes={} parent-ancestors={} missing='{}' wrong-parent='{}'",
                index,
                skeleton->name.c_str() ? skeleton->name.c_str() : "",
                alignment.bones,
                graph.boneNodes.size(),
                agree,
                comparable,
                alignment.matched,
                alignment.ambiguous,
                alignment.duplicate_nodes,
                alignment.parent_ancestors,
                alignment.missing.empty() ? "-" : alignment.missing.c_str(),
                alignment.wrong_parent.empty() ? "-" : alignment.wrong_parent.c_str());
        }

        skeleton_out = skeleton;
        return alignment;
    }

    // The graph whose skeleton resolves the most bones: the report prints every candidate, and the
    // later stages work on the winner.
    bool select_graph(RE::BSAnimationGraphManager& manager, RE::TESObjectREFR& source, RE::NiAVObject& source_root, NodeTable const& copy_nodes, SelectedGraph& out, bool verbose)
    {
        bool found = false;
        const uint32_t graph_count = manager.graphs.size();
        for (uint32_t i = 0; i < graph_count; ++i)
        {
            RE::BShkbAnimationGraph* const graph = manager.graphs[i].get();
            if (!graph)
            {
                if (verbose)
                    logger::info("SCOPY ANIM graph[{}] UNAVAILABLE reason=null-graph", i);
                continue;
            }
            const RE::hkaSkeleton* skeleton = nullptr;
            const SkeletonAlignment alignment = report_graph(*graph, i, source, source_root, copy_nodes, skeleton, verbose);
            if (alignment.bones == 0 || !skeleton)
                continue;
            if (!found || alignment.matched > out.alignment.matched)
            {
                out.graph = graph;
                out.skeleton = skeleton;
                out.alignment = alignment;
                out.index = i;
                found = true;
            }
        }
        return found;
    }

    const char* verdict_of(SkeletonAlignment const& alignment)
    {
        if (alignment.bones == 0)
            return "UNAVAILABLE";
        if (!alignment_resolves_all_bones(alignment))
            return "INCOMPLETE";
        return alignment.ambiguous == 0 ? "PASS" : "PASS-AMBIGUOUS";
    }

    // What one candidate drives, indexed like the pose: the copy's node and the source's node whose
    // world is the truth for it.
    struct ReplayTargets
    {
        std::vector<RE::NiAVObject*> copy;
        std::vector<RE::NiAVObject*> source;
    };

    struct WorldDelta
    {
        size_t compared;
        float max_position;
        float max_rotation_degrees;
    };

    using NameIndex = std::unordered_map<std::string_view, RE::NiAVObject*>;

    NameIndex index_by_name(NodeTable const& table)
    {
        NameIndex index;
        index.reserve(table.names.size());
        for (size_t i = 0; i < table.names.size(); ++i)
            index.emplace(table.names[i], table.objects[i]);
        return index;
    }

    // Candidate one: pose entry i belongs to skeleton bone i, which names the node it drives.
    ReplayTargets map_by_skeleton_names(RE::hkaSkeleton const& skeleton, NameIndex const& copy_by_name, NameIndex const& source_by_name)
    {
        ReplayTargets targets;
        targets.copy.resize(skeleton.bones.size(), nullptr);
        targets.source.resize(skeleton.bones.size(), nullptr);
        for (std::int32_t i = 0; i < skeleton.bones.size(); ++i)
        {
            const char* const name = skeleton.bones[i].name.c_str();
            if (!name)
                continue;
            const auto copy = copy_by_name.find(name);
            if (copy == copy_by_name.end())
                continue;
            targets.copy[static_cast<size_t>(i)] = copy->second;
            const auto source = source_by_name.find(name);
            targets.source[static_cast<size_t>(i)] = source != source_by_name.end() ? source->second : nullptr;
        }
        return targets;
    }

    // Candidate two: pose entry i belongs to the engine's own bone table, boneNodes[i], whose node is
    // on the source's side - the alignment round measured that table to be in a different order than
    // the animation skeleton, so the two candidates really are different.
    ReplayTargets map_by_bone_nodes(RE::BShkbAnimationGraph& graph, NameIndex const& copy_by_name)
    {
        ReplayTargets targets;
        targets.copy.resize(graph.boneNodes.size(), nullptr);
        targets.source.resize(graph.boneNodes.size(), nullptr);
        for (uint32_t i = 0; i < graph.boneNodes.size(); ++i)
        {
            RE::NiNode* const node = graph.boneNodes[i].node;
            if (!node)
                continue;
            targets.source[i] = node;
            const char* const name = node->name.c_str();
            if (!name)
                continue;
            const auto copy = copy_by_name.find(name);
            if (copy != copy_by_name.end())
                targets.copy[i] = copy->second;
        }
        return targets;
    }

    // Writes one candidate's pose into the copy: every target's local transform, then one downward
    // world recompute, because the engine's dirty-update cascade does not run inside the draw window
    // and the skeleton's parent indices are not guaranteed to precede their children.
    size_t write_pose(ReplayTargets const& targets, RE::hkQsTransform const* pose, std::int32_t pose_count, bool transposed, RE::NiAVObject& copy_root)
    {
        const size_t count = std::min(targets.copy.size(), static_cast<size_t>(pose_count));
        size_t written = 0;
        for (size_t i = 0; i < count; ++i)
        {
            RE::NiAVObject* const node = targets.copy[i];
            if (!node)
                continue;
            const PoseSample sample = read_pose(pose[i]);
            RE::NiTransform local = ni_local_from_pose(sample, transposed);
            // A pose without a usable scale keeps the node's own: NIF holds one scale, the pose holds
            // three, and clobbering a real NIF scale with a degenerate value would distort the figure
            // rather than report anything.
            if (!std::isfinite(local.scale) || local.scale <= 0.0f)
                local.scale = node->local.scale;
            node->local = local;
            ++written;
        }
        recompute_subtree_worlds(copy_root, 0);
        return written;
    }

    // How far the copy's driven nodes sit from the source's: one instrument for every candidate, so
    // their numbers can be compared.
    WorldDelta measure_worlds(ReplayTargets const& targets)
    {
        WorldDelta delta{ 0, 0.0f, 0.0f };
        for (size_t i = 0; i < targets.copy.size() && i < targets.source.size(); ++i)
        {
            if (!targets.copy[i] || !targets.source[i])
                continue;
            ++delta.compared;
            delta.max_position = std::max(delta.max_position, (targets.copy[i]->world.translate - targets.source[i]->world.translate).Length());
            delta.max_rotation_degrees = std::max(delta.max_rotation_degrees,
                rotation_angle_degrees(targets.source[i]->world.rotate, targets.copy[i]->world.rotate));
        }
        return delta;
    }

    void perturb_targets(ReplayTargets const& targets, RE::NiAVObject& copy_root)
    {
        RE::NiMatrix3 rotation;
        rotation.MakeXRotation(Control_Perturbation_Radians);
        for (RE::NiAVObject* node : targets.copy)
        {
            if (node)
                node->local.rotate = node->local.rotate * rotation;
        }
        recompute_subtree_worlds(copy_root, 0);
    }

    // The copy's captured locals, so the verification can put the figure back exactly as captured.
    struct LocalSnapshot
    {
        std::vector<RE::NiAVObject*> nodes;
        std::vector<RE::NiTransform> locals;
    };

    LocalSnapshot snapshot_locals(NodeTable const& table)
    {
        LocalSnapshot snapshot;
        snapshot.nodes = table.objects;
        snapshot.locals.reserve(table.objects.size());
        for (RE::NiAVObject* object : table.objects)
            snapshot.locals.push_back(object->local);
        return snapshot;
    }

    void restore_locals(LocalSnapshot const& snapshot, RE::NiAVObject& copy_root)
    {
        for (size_t i = 0; i < snapshot.nodes.size(); ++i)
            snapshot.nodes[i]->local = snapshot.locals[i];
        recompute_subtree_worlds(copy_root, 0);
    }

    // The copy's root is the frame everything below it composes onto, so it is never a target itself.
    void clear_root_target(ReplayTargets& targets, RE::NiAVObject& copy_root)
    {
        for (RE::NiAVObject*& node : targets.copy)
        {
            if (node == &copy_root)
                node = nullptr;
        }
    }

    std::string log_candidate(char const* order, bool transposed, size_t written, WorldDelta const& delta)
    {
        return fmt::format("SCOPY ANIM replay order={} quat={} written={} max-pos-delta={:.3f} max-rot-delta={:.2f}deg bones={}",
            order, transposed ? "transposed" : "direct", written, delta.max_position, delta.max_rotation_degrees, delta.compared);
    }

    // A read is only attempted when the pages are committed and readable, and an object is only
    // believed when its first word looks like a vtable of the game's own image. Nothing here calls a
    // virtual function or trusts a layout, so a misread pointer reports nonsense instead of crashing.
    bool readable(void const* address, size_t bytes)
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

    bool looks_like_object(void const* object)
    {
        if (!readable(object, sizeof(void*)))
            return false;
        const void* const vtable = *static_cast<void* const*>(object);
        if (!vtable)
            return false;
        const std::uintptr_t value = reinterpret_cast<std::uintptr_t>(vtable);
        for (const REL::Segment::Name name : { REL::Segment::rdata, REL::Segment::data })
        {
            const REL::Segment segment = REL::Module::get().segment(name);
            if (value >= segment.address() && value < segment.address() + segment.size())
                return true;
        }
        return false;
    }

    // What one candidate binding says, or that it is not worth believing.
    struct BindingReading
    {
        bool valid;
        float duration;
        size_t tracks;
        size_t tracks_in_range;
    };

    // The validation the two candidate layouts are judged by: the animation pointer has to look like
    // a live object with a plausible clip length, and its track-to-bone table has to name only bones
    // this skeleton has. A pointer into the wrong object fails on all three.
    BindingReading read_binding(RE::hkaAnimationBinding const* binding, std::int32_t bone_count)
    {
        if (!binding || !readable(binding, sizeof(RE::hkaAnimationBinding)))
            return { false, 0.0f, 0, 0 };
        const RE::hkaAnimation* const animation = binding->animation.get();
        if (!animation || !readable(animation, sizeof(RE::hkaAnimation)) || !looks_like_object(animation))
            return { false, 0.0f, 0, 0 };

        const std::int32_t track_count = binding->transformTrackToBoneIndices.size();
        const std::int16_t* const indices = binding->transformTrackToBoneIndices.data();
        if (track_count <= 0 || track_count > Max_Tracks_Read || !readable(indices, sizeof(std::int16_t) * static_cast<size_t>(track_count)))
            return { false, 0.0f, 0, 0 };

        const float duration = animation->duration;
        const bool plausible = std::isfinite(duration) && duration > 0.05f && duration < Max_Clip_Seconds;
        const size_t tracks = static_cast<size_t>(track_count);
        const bool in_range = track_indices_are_valid({ indices, tracks }, bone_count);
        return { plausible && in_range, duration, tracks, in_range ? tracks : 0 };
    }

    // Case-insensitive "idle": the animation list is where the game spells out what a binding index
    // means, and an idle is the first animation the panel needs.
    bool mentions_idle(std::string_view name)
    {
        constexpr std::string_view Idle = "idle";
        if (name.size() < Idle.size())
            return false;
        for (size_t start = 0; start + Idle.size() <= name.size(); ++start)
        {
            size_t matched = 0;
            while (matched < Idle.size() && std::tolower(static_cast<unsigned char>(name[start + matched])) == Idle[matched])
                ++matched;
            if (matched == Idle.size())
                return true;
        }
        return false;
    }
}

void report_animation_source(RE::TESObjectREFR& source, RE::NiAVObject& source_root, RE::NiAVObject& copy_root)
{
    const NodeTable copy_nodes = collect_nodes(copy_root);
    if (copy_nodes.names.empty())
    {
        logger::warn("SCOPY ANIM UNAVAILABLE reason=copy-node-table-empty");
        return;
    }

    RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
    if (!source.GetAnimationGraphManager(manager) || !manager)
    {
        logger::info("SCOPY ANIM UNAVAILABLE reason=no-animation-graph-manager source={:08x}", source.GetFormID());
        return;
    }

    const uint32_t graph_count = manager->graphs.size();
    logger::info("SCOPY ANIM source form={:08x} graphs={} copy-nodes={}", source.GetFormID(), graph_count, copy_nodes.names.size());
    if (graph_count == 0)
    {
        logger::info("SCOPY ANIM UNAVAILABLE reason=no-animation-graphs");
        return;
    }

    SelectedGraph selected{};
    if (!select_graph(*manager, source, source_root, copy_nodes, selected, true))
    {
        logger::info("SCOPY ANIM gate verdict=UNAVAILABLE reason=no-animation-skeleton");
        return;
    }
    logger::info("SCOPY ANIM gate verdict={} graph={} matched={}/{} ambiguous={}",
        verdict_of(selected.alignment), selected.index, selected.alignment.matched, selected.alignment.bones, selected.alignment.ambiguous);
}

void verify_pose_replay(RE::TESObjectREFR& source, RE::NiAVObject& source_root, RE::NiAVObject& copy_root)
{
    const NodeTable copy_nodes = collect_nodes(copy_root);
    if (copy_nodes.names.empty())
    {
        logger::info("SCOPY ANIM replay UNAVAILABLE reason=copy-node-table-empty");
        return;
    }

    RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
    if (!source.GetAnimationGraphManager(manager) || !manager)
    {
        logger::info("SCOPY ANIM replay UNAVAILABLE reason=no-animation-graph-manager");
        return;
    }

    SelectedGraph selected{};
    if (!select_graph(*manager, source, source_root, copy_nodes, selected, false))
    {
        logger::info("SCOPY ANIM replay UNAVAILABLE reason=no-animation-skeleton");
        return;
    }

    RE::hkbCharacter& character = selected.graph->characterInstance;
    const RE::hkQsTransform* const pose = character.poseLocal;
    const std::int32_t pose_count = character.numPoseLocal;
    if (!pose || pose_count <= 0)
    {
        logger::info("SCOPY ANIM replay UNAVAILABLE reason=no-pose-local graph={}", selected.index);
        return;
    }

    // The unresolved bones, by class: Havok's own helper bones (x_ prefixed) never had a nif node,
    // while the rest are attachment or mod bones this outfit does not carry. Neither is a failure of
    // the mapping, and the replay below is what decides whether the resolved bones are enough.
    const NodeTable source_nodes = collect_nodes(source_root);
    const NameIndex copy_by_name = index_by_name(copy_nodes);
    const NameIndex source_by_name = index_by_name(source_nodes);
    size_t helper_unresolved = 0;
    size_t other_unresolved = 0;
    size_t scale_off = 0;
    for (std::int32_t i = 0; i < selected.skeleton->bones.size(); ++i)
    {
        const char* const name = selected.skeleton->bones[i].name.c_str();
        if (!name || copy_by_name.find(name) == copy_by_name.end())
        {
            if (name && std::string_view(name).starts_with("x_"))
                ++helper_unresolved;
            else
                ++other_unresolved;
        }
    }
    for (std::int32_t i = 0; i < pose_count; ++i)
    {
        const float scale = read_pose(pose[i]).scale;
        if (!std::isfinite(scale) || std::abs(scale - 1.0f) > 0.01f)
            ++scale_off;
    }
    logger::info("SCOPY ANIM map graph={} bones={} matched={} helper-unresolved={} other-unresolved={}",
        selected.index, selected.alignment.bones, selected.alignment.matched, helper_unresolved, other_unresolved);
    logger::info("SCOPY ANIM replay pose entries={} scale-off-from-one={}", pose_count, scale_off);

    ReplayTargets skeleton_targets = map_by_skeleton_names(*selected.skeleton, copy_by_name, source_by_name);
    ReplayTargets bone_node_targets = map_by_bone_nodes(*selected.graph, copy_by_name);
    clear_root_target(skeleton_targets, copy_root);
    clear_root_target(bone_node_targets, copy_root);

    const LocalSnapshot snapshot = snapshot_locals(copy_nodes);

    // The control first: the same measurement on a pose the copy cannot reproduce by accident. If it
    // came out small, the candidates' numbers would mean nothing.
    perturb_targets(skeleton_targets, copy_root);
    const WorldDelta control = measure_worlds(skeleton_targets);
    logger::info("SCOPY ANIM replay control max-pos-delta={:.3f} max-rot-delta={:.2f}deg bones={}", control.max_position, control.max_rotation_degrees, control.compared);
    restore_locals(snapshot, copy_root);

    struct Candidate
    {
        char const* order;
        bool transposed;
        WorldDelta delta;
    };
    const char* const order_names[2] = { "skeleton", "bone-nodes" };
    ReplayTargets* const orders[2] = { &skeleton_targets, &bone_node_targets };

    Candidate best{ "none", false, { 0, std::numeric_limits<float>::max(), 0.0f } };
    for (size_t order = 0; order < 2; ++order)
    {
        for (int conversion = 0; conversion < 2; ++conversion)
        {
            const bool transposed = conversion != 0;
            const size_t written = write_pose(*orders[order], pose, pose_count, transposed, copy_root);
            const WorldDelta delta = measure_worlds(*orders[order]);
            logger::info("{}", log_candidate(order_names[order], transposed, written, delta));
            if (delta.compared != 0 && delta.max_position < best.delta.max_position)
                best = Candidate{ order_names[order], transposed, delta };
            restore_locals(snapshot, copy_root);
        }
    }

    // And the restored state, because the verification must leave the panel's figure as it found it.
    const WorldDelta restored = measure_worlds(skeleton_targets);
    logger::info("SCOPY ANIM replay restored max-pos-delta={:.3f} max-rot-delta={:.2f}deg bones={}", restored.max_position, restored.max_rotation_degrees, restored.compared);

    const bool matched = control.max_position > 0.0f && best.delta.max_position < control.max_position * Match_Fraction_Of_Control;
    logger::info("SCOPY ANIM replay verdict match={} control={:.3f}u best={:.4f}u",
        matched ? fmt::format("{}/{}", best.order, best.transposed ? "transposed" : "direct") : std::string("none"),
        control.max_position, best.delta.max_position);
}

void report_animation_catalogue(RE::TESObjectREFR& source, RE::NiAVObject& source_root, RE::NiAVObject& copy_root)
{
    const NodeTable copy_nodes = collect_nodes(copy_root);
    RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
    if (copy_nodes.names.empty() || !source.GetAnimationGraphManager(manager) || !manager)
    {
        logger::info("SCOPY ANIM catalogue UNAVAILABLE reason=no-animation-graph-manager");
        return;
    }

    SelectedGraph selected{};
    if (!select_graph(*manager, source, source_root, copy_nodes, selected, false))
    {
        logger::info("SCOPY ANIM catalogue UNAVAILABLE reason=no-animation-skeleton");
        return;
    }

    RE::hkbCharacter& character = selected.graph->characterInstance;
    RE::hkbAnimationBindingSet* const set = character.animationBindingSet.get();
    RE::hkbCharacterSetup* const setup = character.setup.get();
    RE::hkbCharacterData* const data = setup ? setup->data.get() : nullptr;
    RE::hkbCharacterStringData* const strings = data ? data->stringData.get() : nullptr;

    const std::int32_t binding_count = (set && looks_like_object(set)) ? set->bindings.size() : 0;
    if (!strings || !looks_like_object(strings))
    {
        logger::info("SCOPY ANIM catalogue UNAVAILABLE reason=character-string-data data={} string-data={} bindings={}",
            data != nullptr, strings != nullptr, binding_count);
        return;
    }

    logger::info("SCOPY ANIM catalogue character='{}' rig='{}' behavior='{}' names={} bindings={}",
        strings->name.c_str() ? strings->name.c_str() : "",
        strings->rigName.c_str() ? strings->rigName.c_str() : "",
        strings->behaviorFilename.c_str() ? strings->behaviorFilename.c_str() : "",
        strings->animationNames.size(), binding_count);

    // The names are the only place the game spells out what a binding index means, so the idle hunt is
    // what turns an index into "the animation the panel wants". Their indices are kept to be checked
    // against the binding set below.
    std::vector<std::pair<std::int32_t, std::string>> idle_names;
    std::int32_t idle_count = 0;
    for (std::int32_t i = 0; i < strings->animationNames.size(); ++i)
    {
        const char* const name = strings->animationNames[i].c_str();
        if (!name || !mentions_idle(name))
            continue;
        ++idle_count;
        if (idle_names.size() < Max_Catalogue_Names)
            idle_names.emplace_back(i, name);
    }

    std::string idle_list;
    for (const auto& [index, name] : idle_names)
    {
        if (!idle_list.empty())
            idle_list += ", ";
        idle_list += fmt::format("{}@{}", name, index);
    }
    logger::info("SCOPY ANIM catalogue idle-names={}/{} first='{}'", idle_count, strings->animationNames.size(), idle_list.empty() ? "-" : idle_list);

    if (!set || !looks_like_object(set) || binding_count <= 0)
    {
        logger::info("SCOPY ANIM catalogue layout UNAVAILABLE reason=binding-set");
        return;
    }

    // Neither candidate layout is typed in this checkout, so both are judged structurally on a spread
    // of indices: a real binding's animation looks like a live object with a plausible clip length,
    // and every one of its tracks names a bone this skeleton has.
    const std::int32_t bone_count = selected.skeleton->bones.size();
    size_t tried = 0;
    size_t as_binding = 0;
    size_t holds_binding = 0;
    float min_duration = std::numeric_limits<float>::max();
    float max_duration = 0.0f;
    for (size_t step = 0; step < Catalogue_Sample_Count; ++step)
    {
        const std::int32_t index = static_cast<std::int32_t>(static_cast<size_t>(binding_count) * step / Catalogue_Sample_Count);
        if (index >= binding_count)
            continue;
        const void* const element = set->bindings[index];
        if (!element || !readable(element, sizeof(void*)))
            continue;
        ++tried;

        const BindingReading direct = read_binding(static_cast<const RE::hkaAnimationBinding*>(element), bone_count);
        if (direct.valid)
        {
            ++as_binding;
            min_duration = std::min(min_duration, direct.duration);
            max_duration = std::max(max_duration, direct.duration);
        }
        const auto* const pointed = *static_cast<RE::hkaAnimationBinding* const*>(element);
        if (read_binding(pointed, bone_count).valid)
            ++holds_binding;
    }

    logger::info("SCOPY ANIM catalogue layout candidate=element-as-binding valid={}/{} duration=({:.2f}..{:.2f})s",
        as_binding, tried, as_binding ? min_duration : 0.0f, max_duration);
    logger::info("SCOPY ANIM catalogue layout candidate=element-holds-binding-pointer valid={}/{}", holds_binding, tried);

    // The idle indices are where a name and a duration can be read side by side: "Idle" with a few
    // seconds and a full track table says the name list and the binding set really are the same order.
    for (const auto& [index, name] : idle_names)
    {
        if (index >= binding_count)
            continue;
        const void* const element = set->bindings[index];
        if (!element || !readable(element, sizeof(void*)))
            continue;
        const BindingReading direct = read_binding(static_cast<const RE::hkaAnimationBinding*>(element), bone_count);
        const auto* const pointed = *static_cast<RE::hkaAnimationBinding* const*>(element);
        const BindingReading indirect = read_binding(pointed, bone_count);
        logger::info("SCOPY ANIM catalogue idle index={} name='{}' element-as-binding={} duration={:.2f}s tracks={}/{} element-holds-binding={} duration={:.2f}s",
            index, name, direct.valid, direct.duration, direct.tracks_in_range, direct.tracks, indirect.valid, indirect.duration);
    }
}

PLUGIN_NAMESPACE_END
