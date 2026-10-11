//
// Created by AmazingBuff on 2026/10/9.
//

#include "character/animation_source.h"

#include "character/snapshot_transform.h"

#include "RE/A/AnimationFileManagerSingleton.h"
#include "RE/B/BSAnimationGraphManager.h"
#include "RE/B/BSFadeNode.h"
#include "RE/B/BShkbAnimationGraph.h"
#include "RE/B/BSVisit.h"
#include "RE/H/hkClass.h"
#include "RE/H/hkaAnimation.h"
#include "RE/H/hkaAnimationBinding.h"
#include "RE/H/hkaAnimationControl.h"
#include "RE/H/hkaDefaultAnimationControl.h"
#include "RE/H/hkaBone.h"
#include "RE/H/hkaSkeleton.h"
#include "RE/H/hkaSplineCompressedAnimation.h"
#include "RE/H/hkbAnimationBindingSet.h"
#include "RE/H/hkbBehaviorGraph.h"
#include "RE/H/hkbCharacter.h"
#include "RE/H/hkbCharacterData.h"
#include "RE/H/hkbCharacterSetup.h"
#include "RE/H/hkbCharacterStringData.h"
#include "RE/H/hkbClipGenerator.h"
#include "RE/H/hkbGenerator.h"
#include "RE/H/hkbNode.h"
#include "RE/H/hkbStateMachine.h"
#include "RE/T/TESObjectREFR.h"
#include "REL/Module.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
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

    // How far into an element the scan looks for an object, how much of it is dumped raw, and how long
    // a string read is allowed to run.
    constexpr std::int32_t Scan_Bytes = 0x80;
    constexpr size_t Raw_Dump_Bytes = 0x80;
    // How far the clip search walks, how much of each object it scans, and how the ground-truth sweep
    // is sampled. The bounds are generous because vtable matching made each object cheap: the real
    // generator tree is far wider than anything the old shape-only walk ever reached.
    constexpr int Max_Clip_Depth = 12;
    constexpr size_t Max_Clip_Objects = 1024;
    constexpr std::int32_t Max_States = 64;
    constexpr std::int32_t Container_Scan_Bytes = 0x200;
    constexpr size_t Sweep_Steps = 24;
    constexpr size_t Max_Clips_Reported = 6;
    // Every clip generator the walk meets is recorded up to this many, and this many of them printed:
    // HKX9 met 66 and none of their direct bindings were live, so the record of who carries what is
    // the evidence the next route is chosen from.
    constexpr size_t Max_Clips_Tracked = 128;
    constexpr size_t Max_Clips_Printed = 12;
    // How much of one binding-set element is dumped when the dump is aimed by a clip's own index.
    constexpr size_t Element_Dump_Bytes = 0x40;
    // How many of the file manager's loaded animations are printed, and how wide a raw dump a rejected
    // one gets.
    constexpr size_t Max_Loaded_Printed = 16;
    // The engine's own index-to-animation resolver is followed by a short walk over the loaded
    // resource: this many objects at most, and this many validated animations recorded from one.
    constexpr size_t Max_Hashed_Visited = 96;
    constexpr size_t Max_Hashed_Anims = 8;
    // How many animations are collected from the engine's loaded storage when walking it, how many
    // resources one loader record is allowed to hold before the count is treated as a misread, and how
    // many records the walk looks at (the table holds thousands, and the scan is bounded on purpose).
    constexpr size_t Max_Loaded_Anims = 8;
    constexpr std::int32_t Max_Loaded_Resources = 64;
    // Records the walk looks at, and objects it may visit inside one resource. The table holds thousands
    // of records and each resource costs a window scan, so both are bounded and the record count is small
    // enough to afford a walk that is deep enough to reach a nested animation.
    constexpr std::uint32_t Max_Walked_Records = 8;
    constexpr size_t Max_Resource_Objects = 24;
    constexpr size_t Resource_Dump_Bytes = 0x60;
    constexpr size_t Max_Tree_Lines = 16;
    // How far the content scan reaches into one byte range, and how many ranges one resource may offer.
    constexpr size_t Max_Region_Scan_Bytes = 0x10000;
    constexpr size_t Max_Scanned_Regions = 4;
    // How far the instance-side walk goes and how many of its clips are printed. Small on purpose: this
    // stage only answers whether a binding resolves, so it should cost one bounded pass, not a search.
    constexpr size_t Max_Instance_Objects = 256;
    constexpr size_t Max_Instance_Printed = 12;
    // How many tracks a skeleton has to resolve before it is worth scoring against the engine's pose.
    constexpr size_t Min_Tracks_For_Skeleton_Test = 8;
    constexpr size_t Progress_Every = 32;
    constexpr double Max_Clip_Step_Seconds = 0.1;

    // How long a string read is allowed to run, how many bone names a probe prints, and which file
    // names count as the character's own standing idle rather than a weapon or object one.
    constexpr size_t Max_String_Read = 128;
    constexpr size_t Max_Bones_Reported = 4;
    constexpr std::string_view Base_Idle_Names[] = { "idle.hkx", "idleforcedefaultstate.hkx", "mt_idle.hkx" };

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

    // What one candidate binding says, or which structural check rejected it. The reason is carried
    // because "valid=0" alone cannot tell a wrong offset from a wrong assumption about the element.
    struct BindingReading
    {
        bool valid;
        const char* reason;
        float duration;
        size_t tracks;
        size_t tracks_in_range;
        std::uint32_t type;
    };

    const char* animation_type_name(std::uint32_t type)
    {
        using Type = RE::hkaAnimation::AnimationType;
        switch (static_cast<Type>(type))
        {
        case Type::kInterleavedAnimation:
            return "interleaved";
        case Type::kDeltaCompressedAnimation:
            return "delta";
        case Type::kWaveletCompressedAnimation:
            return "wavelet";
        case Type::kMirroredAnimation:
            return "mirrored";
        case Type::kSplineCompressedAnimation:
            return "spline";
        case Type::kQuantizedCompressedAnimation:
            return "quantized";
        default:
            return "unknown";
        }
    }

    // What the track table of one binding says when it does not validate, printed once per binding.
    // HKX13 reached a live binding on the clip being played and still could not map it, and the two
    // remaining explanations differ in what they predict: a track table the engine has not filled in
    // yet reads as a null or garbage array, while a layout variant puts the array somewhere other than
    // the offset this checkout declares. The array's own words and the animation's declared track
    // count tell the two apart without a guess.
    void report_track_table(RE::hkaAnimationBinding const* binding, RE::hkaAnimation const* animation, std::int32_t track_count, std::int16_t const* indices)
    {
        const std::int32_t declared = animation ? animation->numberOfTransformTracks : 0;
        const bool array_readable = indices != nullptr && track_count > 0 &&
                                    readable(indices, sizeof(std::int16_t) * static_cast<size_t>(track_count));
        logger::info("SCOPY ANIM tracks binding=0x{:x} array=0x{:x} size={} declared={} readable={} array-at=0x{:x}",
            reinterpret_cast<std::uintptr_t>(binding), reinterpret_cast<std::uintptr_t>(indices), track_count,
            declared, array_readable, reinterpret_cast<std::uintptr_t>(&binding->transformTrackToBoneIndices));

        if (!readable(binding, sizeof(RE::hkaAnimationBinding)))
            return;
        const auto* const words = static_cast<const std::uint64_t*>(static_cast<const void*>(binding));
        std::string bytes;
        for (size_t word = 0; word < sizeof(RE::hkaAnimationBinding) / sizeof(std::uint64_t); ++word)
            bytes += fmt::format("{}{:016x}", word ? " " : "", words[word]);
        logger::info("SCOPY ANIM tracks binding qwords='{}'", bytes);
    }

    // The validation every candidate layout is judged by, and it needs no virtual call: the animation
    // pointer has to look like a live object, its type has to be one the engine names, its clip length
    // has to be plausible, and its own transform-track count has to equal the binding's track table -
    // which is the check a wrapper object cannot pass, because its animation field is not there.
    BindingReading read_binding(RE::hkaAnimationBinding const* binding, std::int32_t bone_count, bool detail_on_failure = false)
    {
        if (!binding || !readable(binding, sizeof(RE::hkaAnimationBinding)))
            return { false, "unreadable", 0.0f, 0, 0, 0 };
        const RE::hkaAnimation* const animation = binding->animation.get();
        if (!animation || !readable(animation, sizeof(RE::hkaAnimation)) || !looks_like_object(animation))
            return { false, "no-animation", 0.0f, 0, 0, 0 };

        const std::uint32_t type = static_cast<std::uint32_t>(animation->type.get());
        if (type == 0 || type > static_cast<std::uint32_t>(RE::hkaAnimation::AnimationType::kQuantizedCompressedAnimation))
            return { false, "animation-type", 0.0f, 0, 0, type };

        const float duration = animation->duration;
        if (!std::isfinite(duration) || duration <= 0.05f || duration >= Max_Clip_Seconds)
            return { false, "duration", duration, 0, 0, type };

        const std::int32_t track_count = binding->transformTrackToBoneIndices.size();
        const std::int16_t* const indices = binding->transformTrackToBoneIndices.data();
        if (track_count <= 0 || track_count > Max_Tracks_Read || !readable(indices, sizeof(std::int16_t) * static_cast<size_t>(track_count)))
        {
            // The one number that separates the two explanations for an unreadable table: if the
            // animation itself declares a sane track count, the binding is a live object and the array
            // is either not filled in yet or not where this checkout reads it.
            if (detail_on_failure)
                report_track_table(binding, animation, track_count, indices);
            return { false, "tracks-unreadable", duration, 0, 0, type };
        }
        if (animation->numberOfTransformTracks != track_count)
            return { false, "track-count-mismatch", duration, static_cast<size_t>(track_count), 0, type };

        const size_t tracks = static_cast<size_t>(track_count);
        if (tracks > static_cast<size_t>(bone_count) || !track_indices_are_valid({ indices, tracks }, bone_count))
            return { false, "track-bones", duration, tracks, 0, type };
        return { true, "valid", duration, tracks, tracks, type };
    }

    // Whether the animation's own data is actually present, so that sampling it is safe. Every route
    // here reads the object's header first, and the header can be complete while the data is not: a
    // spline animation de-references its transformOffsets table per track, and HKX16 crashed inside
    // hkaSplineCompressedAnimation sampling on `mov r9d,[rax+rcx*4]` - an index into that table - after
    // the header had passed every check. A null or unreadable table therefore has to be caught before
    // the first sample, never after.
    bool animation_data_is_present(RE::hkaAnimation const* animation, [[maybe_unused]] std::int32_t track_count)
    {
        if (!animation || !readable(animation, sizeof(RE::hkaAnimation)) || !looks_like_object(animation))
            return false;
        if (static_cast<std::uint32_t>(animation->type.get()) !=
            static_cast<std::uint32_t>(RE::hkaAnimation::AnimationType::kSplineCompressedAnimation))
            return true;

        const auto* const spline = static_cast<const RE::hkaSplineCompressedAnimation*>(animation);
        if (!readable(spline, sizeof(RE::hkaSplineCompressedAnimation)))
            return false;

        // What this checks is what the sampler this project actually calls needs: the block table and the
        // compressed bytes. transformOffsets is deliberately not required - the dump shows this engine
        // leaves it empty at size zero on animations that plainly do have their data, and that is not an
        // accident: SampleIndividualTransformTracks indexes it per track, while the SamplePartialTracks
        // entry point OAR uses takes a contiguous prefix and never needs it. Requiring it here is what
        // made every animation read as absent while the bytes sat there readable.
        if (spline->numFrames <= 0 || spline->numBlocks <= 0)
            return false;
        if (spline->blockOffsets.size() <= 0 || !spline->blockOffsets.data() ||
            !readable(spline->blockOffsets.data(), sizeof(std::uint32_t) * static_cast<size_t>(spline->blockOffsets.size())))
            return false;
        const std::int32_t bytes = spline->data.size();
        if (bytes <= 0 || !spline->data.data() || !readable(spline->data.data(), static_cast<size_t>(bytes)))
            return false;
        return true;
    }

    // What the spline check found, said out loud instead of collapsed into one boolean. "absent" has
    // several causes that need different fixes - an array that was never filled reads as a null pointer
    // with size zero, while a layout that does not match this build reads as a plausible pointer or a
    // count that cannot be true - and the caller cannot tell them apart from the boolean alone.
    const char* animation_data_state(RE::hkaAnimation const* animation, std::int32_t track_count)
    {
        if (!animation || !readable(animation, sizeof(RE::hkaAnimation)) || !looks_like_object(animation))
            return "no-object";
        if (static_cast<std::uint32_t>(animation->type.get()) !=
            static_cast<std::uint32_t>(RE::hkaAnimation::AnimationType::kSplineCompressedAnimation))
            return "unchecked-non-spline";
        return animation_data_is_present(animation, track_count) ? "present" : "absent";
    }

    // Every field the spline layout declares, so a mismatch is visible as a value rather than inferred
    // from a failed check. The raw words come too: the scalars say whether the object is populated at
    // all, and the words say where the arrays actually sit if these offsets are not the right ones.
    void report_spline_fields(RE::hkaAnimation const* animation, std::int32_t track_count)
    {
        if (!animation || !readable(animation, sizeof(RE::hkaSplineCompressedAnimation)))
        {
            logger::info("SCOPY ANIM load spline UNAVAILABLE reason=window-unreadable");
            return;
        }
        const auto* const spline = static_cast<const RE::hkaSplineCompressedAnimation*>(animation);
        logger::info("SCOPY ANIM load spline frames={} blocks={} max-per-block={} mask-size={} block-dur={:.3f} frame-dur={:.4f} track-count={}",
            spline->numFrames, spline->numBlocks, spline->maxFramesPerBlock, spline->maskAndQuantizationSize,
            spline->blockDuration, spline->frameDuration, track_count);

        struct ArrayView
        {
            const char* name;
            const void* data;
            std::int32_t size;
        };
        const ArrayView arrays[] = {
            { "block-offsets", spline->blockOffsets.data(), spline->blockOffsets.size() },
            { "float-block-offsets", spline->floatBlockOffsets.data(), spline->floatBlockOffsets.size() },
            { "transform-offsets", spline->transformOffsets.data(), spline->transformOffsets.size() },
            { "float-offsets", spline->floatOffsets.data(), spline->floatOffsets.size() },
            { "data", spline->data.data(), spline->data.size() },
        };
        for (const ArrayView& array : arrays)
        {
            const bool ok = array.size > 0 && array.data &&
                            readable(array.data, static_cast<size_t>(array.size));
            logger::info("SCOPY ANIM load spline array={} size={} at=0x{:x} readable={}",
                array.name, array.size, reinterpret_cast<std::uintptr_t>(array.data), ok);
        }

        std::string bytes;
        const auto* const words = static_cast<const std::uint64_t*>(static_cast<const void*>(spline));
        for (size_t word = 0; word < sizeof(RE::hkaSplineCompressedAnimation) / sizeof(std::uint64_t); ++word)
            bytes += fmt::format("{}{:016x}", word ? " " : "", words[word]);
        logger::info("SCOPY ANIM load spline qwords='{}'", bytes);
    }


    // The sampling entry point this project uses, taken from the one piece of working community code
    // that does this exact job: OAR's preview samples a loaded animation through SamplePartialTracks
    // with a null chunk cache (ActiveAnimationPreview.cpp:38). It takes a contiguous run of tracks from
    // zero, which is what an empty track-to-bone table amounts to anyway - Havok treats a null table as
    // the identity mapping - and it never indexes the per-track offset table that this engine leaves at
    // size zero and that SampleIndividualTransformTracks walks straight into, which is what crashed the
    // first attempt to sample and what made every animation read as having no data.
    void sample_tracks(RE::hkaAnimation const& animation, float time, std::uint32_t count, RE::hkQsTransform* out, float* float_out, std::uint32_t float_count)
    {
        animation.SamplePartialTracks(time, count, out, float_count, float_out, nullptr);
    }

    // The two binding sets the engine keeps, in the precedence OAR's source uses
    // (FakeClipGenerator.cpp:108-111): the character's own first, the setup's as the fallback. They are
    // distinct objects, and only one of them may have had its entries filled in.
    struct BindingSetPair
    {
        RE::hkbAnimationBindingSet* character;
        RE::hkbAnimationBindingSet* setup;
    };

    BindingSetPair binding_sets(RE::hkbCharacter& character)
    {
        BindingSetPair pair{ character.animationBindingSet.get(), nullptr };
        if (const RE::hkbCharacterSetup* const setup = character.setup.get())
            pair.setup = setup->animationBindingSet.get();
        return pair;
    }

    // Which of the two sets an index is read from, or nullptr when neither holds it.
    RE::hkbAnimationBindingSet* set_holding(BindingSetPair const& pair, std::int32_t index)
    {
        for (RE::hkbAnimationBindingSet* const set : { pair.character, pair.setup })
        {
            if (set && index >= 0 && index < set->bindings.size())
                return set;
        }
        return nullptr;
    }

    // One binding-set entry as OAR's source lays it out (Havok.h:200-203, static_asserted there):
    // hkbAnimationBindingWithTriggers is 0x28 bytes and the hkaAnimationBinding pointer sits at +0x10.
    // The earlier rounds read this at 0x30-wide spacing, which never lands on the field.
    const RE::hkaAnimationBinding* binding_at_offset_0x10(void const* element)
    {
        if (!element || !readable(element, 0x18))
            return nullptr;
        const void* const candidate = *reinterpret_cast<void* const*>(static_cast<const std::uint8_t*>(element) + 0x10);
        return static_cast<const RE::hkaAnimationBinding*>(candidate);
    }

    // A string we are willing to print: read through one guarded window at a time so an
    // unterminated or non-textual pointer cannot run away, and bounded either way. The window
    // is what keeps the page queries affordable at a per-sample call site — one query per
    // String_Guard_Window bytes instead of one per byte — with the same guarantee that no
    // scan ever touches an address the query did not cover.
    constexpr size_t String_Guard_Window = 16;
    std::string guarded_string(RE::hkStringPtr const& value)
    {
        const char* const text = value.c_str();
        if (!text)
            return "-";
        std::string out;
        size_t verified = 0;
        for (size_t i = 0; i < Max_String_Read; ++i)
        {
            if (i >= verified)
            {
                const size_t span = std::min(String_Guard_Window, Max_String_Read - i);
                if (readable(text + i, span))
                {
                    verified = i + span;
                }
                else if (readable(text + i, 1))
                {
                    verified = i + 1;  // a string can legitimately end at a page edge; keep going byte-wise
                }
                else
                {
                    return "-";
                }
            }
            const char character = text[i];
            if (character == '\0')
                return out.empty() ? "-" : out;
            if (static_cast<unsigned char>(character) < 0x20 || static_cast<unsigned char>(character) > 0x7e)
                return "-";
            out += character;
        }
        return out.empty() ? "-" : out + "...";
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

    bool starts_with_idle(std::string_view name)
    {
        constexpr std::string_view Idle = "idle";
        if (name.size() < Idle.size())
            return false;
        for (size_t i = 0; i < Idle.size(); ++i)
        {
            if (std::tolower(static_cast<unsigned char>(name[i])) != Idle[i])
                return false;
        }
        return true;
    }

    bool equals_ignore_case(std::string_view lhs, std::string_view rhs)    {
        if (lhs.size() != rhs.size())
            return false;
        for (size_t i = 0; i < lhs.size(); ++i)
        {
            if (std::tolower(static_cast<unsigned char>(lhs[i])) != std::tolower(static_cast<unsigned char>(rhs[i])))
                return false;
        }
        return true;
    }

    // The equip class by name. The panel needs exactly two classes of animation - the idle it plays while
    // nothing is happening and the equip animation it plays when equipment changes - so this is the whole
    // of the second classifier. Names are the only handle available before an animation is loaded, and the
    // log prints the names that fell into neither class so a wrong guess here is visible rather than silent.
    bool mentions_equip(std::string_view name)
    {
        constexpr std::string_view Equip = "equip";
        if (name.size() < Equip.size())
            return false;
        for (size_t start = 0; start + Equip.size() <= name.size(); ++start)
        {
            size_t matched = 0;
            while (matched < Equip.size() && std::tolower(static_cast<unsigned char>(name[start + matched])) == Equip[matched])
                ++matched;
            if (matched == Equip.size())
                return true;
        }
        return false;
    }

    bool is_base_idle(std::string_view file_name)
    {
        for (const std::string_view candidate : Base_Idle_Names)
        {
            if (equals_ignore_case(file_name, candidate))
                return true;
        }
        return false;
    }

    std::string_view base_name_of(std::string_view path)
    {
        const size_t slash = path.find_last_of("\\/");
        return slash == std::string_view::npos ? path : path.substr(slash + 1);
    }

    bool mentions_hkx(std::string_view name)
    {
        constexpr std::string_view Suffix = ".hkx";
        return name.size() >= Suffix.size() && equals_ignore_case(name.substr(name.size() - Suffix.size()), Suffix);
    }

    // The file manager keys a loaded animation by the game's own CRC of the lowercased file name
    // without its extension, so the name table can name what the manager only numbers.
    std::uint32_t bscrc32(std::string_view text)
    {
        std::uint32_t crc = 0xFFFFFFFFu;
        for (const char character : text)
        {
            crc ^= static_cast<std::uint8_t>(character);
            for (int bit = 0; bit < 8; ++bit)
                crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
        return ~crc;
    }

    std::string lowercased(std::string_view text)
    {
        std::string out;
        out.reserve(text.size());
        for (const char character : text)
            out += static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        return out;
    }

    std::string_view strip_extension(std::string_view name)
    {
        const size_t dot = name.find_last_of('.');
        return dot == std::string_view::npos ? name : name.substr(0, dot);
    }

    // A raw animation pointer from the file manager is believed only when everything that makes it an
    // animation reads plausibly - the same validation a binding's animation must pass, minus the
    // binding: an engine-named type, a plausible length, a track count inside bounds.
    struct RawAnimationReading
    {
        bool valid;
        const char* reason;
        std::uint32_t type;
        float duration;
        std::int32_t tracks;
    };

    RawAnimationReading read_raw_animation(void const* candidate)
    {
        if (!candidate || !readable(candidate, sizeof(RE::hkaAnimation)) || !looks_like_object(candidate))
            return { false, "not-object", 0, 0.0f, 0 };
        const auto* const animation = static_cast<const RE::hkaAnimation*>(candidate);
        const std::uint32_t type = static_cast<std::uint32_t>(animation->type.get());
        if (type == 0 || type > static_cast<std::uint32_t>(RE::hkaAnimation::AnimationType::kQuantizedCompressedAnimation))
            return { false, "animation-type", type, 0.0f, 0 };
        const float duration = animation->duration;
        if (!std::isfinite(duration) || duration <= 0.05f || duration >= Max_Clip_Seconds)
            return { false, "duration", type, duration, 0 };
        const std::int32_t tracks = animation->numberOfTransformTracks;
        if (tracks <= 0 || tracks > Max_Tracks_Read)
            return { false, "tracks", type, duration, tracks };
        return { true, "valid", type, duration, tracks };
    }

    // A short walk from a rooted pointer over the loaded resource's own references: every object that
    // passes the raw-animation validation is recorded. Visits and finds are bounded, nothing virtual is
    // called, and a wrong step costs an unvalidated candidate rather than a wrong animation.
    void collect_raw_animations(void const* root, std::vector<const RE::hkaAnimation*>& out)
    {
        std::unordered_set<void const*> seen{ root };
        std::vector<const void*> pending{ root };
        size_t visited = 0;
        while (!pending.empty() && visited < Max_Hashed_Visited && out.size() < Max_Hashed_Anims)
        {
            const void* const object = pending.back();
            pending.pop_back();
            ++visited;
            if (const RawAnimationReading reading = read_raw_animation(object); reading.valid)
                out.push_back(static_cast<const RE::hkaAnimation*>(object));
            for (std::int32_t offset = 0; offset + static_cast<std::int32_t>(sizeof(void*)) <= Container_Scan_Bytes; offset += static_cast<std::int32_t>(sizeof(void*)))
            {
                const auto* const word_address = static_cast<const std::uint8_t*>(object) + offset;
                if (!readable(word_address, sizeof(void*)))
                    break;
                const void* const word = *reinterpret_cast<void* const*>(word_address);
                if (word && readable(word, sizeof(void*)) && looks_like_object(word) && seen.insert(word).second)
                    pending.push_back(word);
            }
        }
    }

    // CLib exposes only RTTI and VTABLE for these two classes, so their layout is taken from OAR's
    // Havok.h, which static_asserts both sizes against the engine it runs in.
    class hkLoaderLayout : public RE::hkReferencedObject
    {
    public:
        RE::hkArray<void*> loadedData;  // 10
    };
    static_assert(sizeof(hkLoaderLayout) == 0x20);

    // How far the loaded-storage walk reached, so a miss can be attributed to the step it died at
    // instead of to the whole route.
    struct LoadedWalk
    {
        size_t records = 0;         // records looked at
        size_t with_loader = 0;     // records whose +0x28 read as an object
        size_t with_resources = 0;  // of those, how many held a non-empty resource array
        size_t resources = 0;       // resources seen in total
        size_t animations = 0;      // animations validated out of them
        std::string first_loader;   // the first loader's own words, for layout confirmation
        std::string first_resource; // the first resource's own words
        const char* resource_type = "-";  // what that resource's vtable says it is
        size_t tree_lines = 0;      // objects described so far, so the tree dump stays bounded
        size_t region_hits = 0;     // aligned addresses in a scanned region that validated as animations
        size_t regions_scanned = 0; // byte ranges the content scan has covered
    };

    void consider_animation(void const* candidate, LoadedWalk& walk, std::vector<const RE::hkaAnimation*>& out)
    {
        if (out.size() >= Max_Loaded_Anims || !candidate ||
            !readable(candidate, sizeof(RE::hkaAnimation)) || !looks_like_object(candidate))
            return;
        if (!read_raw_animation(candidate).valid)
            return;
        const auto* const animation = static_cast<const RE::hkaAnimation*>(candidate);
        if (std::find(out.begin(), out.end(), animation) == out.end())
        {
            out.push_back(animation);
            ++walk.animations;
        }
    }

    // Which of the engine's resource classes a pointer actually is. The five names are all declared in
    // the address library, so equality with a known vtable answers the question with one guarded read
    // and no call - the same technique that made HKX9's clip classification safe.
    const char* resource_type_name(void const* resource)
    {
        if (!resource || !readable(resource, sizeof(void*)))
            return "unreadable";
        const std::uintptr_t vtable = *static_cast<const std::uintptr_t*>(resource);
        struct Entry
        {
            std::uintptr_t address;
            const char* name;
        };
        static const Entry entries[] = {
            { RE::VTABLE_hkResourceBase[0].address(), "hkResourceBase" },
            { RE::VTABLE_hkResourceHandle[0].address(), "hkResourceHandle" },
            { RE::VTABLE_hkResourceContainer[0].address(), "hkResourceContainer" },
            { RE::VTABLE_hkResourceMap[0].address(), "hkResourceMap" },
            { RE::VTABLE_hkResource[0].address(), "hkResource" },
        };
        for (const Entry& entry : entries)
        {
            if (vtable == entry.address)
                return entry.name;
        }
        return "other";
    }

    // A linear scan over bytes a resource owns, so an animation is found by what it is rather than by
    // following a pointer graph whose shape has to be guessed first. The resource holds its objects in
    // one arena - the dump shows a pointer and a byte count at +0x10 and +0x18, with a second pointer
    // 0x1A0 into that arena - and a graph walk that only follows object-shaped words never descends
    // into it. Offering every aligned address to the same validation an animation must pass needs no
    // layout at all, and a wrong hit costs one rejected candidate.
    void scan_region_for_animations(void const* region, size_t bytes, LoadedWalk& walk, std::vector<const RE::hkaAnimation*>& out)
    {
        const size_t limit = std::min(bytes, Max_Region_Scan_Bytes);
        if (!region || limit < sizeof(RE::hkaAnimation) || !readable(region, sizeof(RE::hkaAnimation)))
            return;
        for (size_t offset = 0; offset + sizeof(RE::hkaAnimation) <= limit; offset += sizeof(void*))
        {
            const auto* const candidate = static_cast<const std::uint8_t*>(region) + offset;
            if (!readable(candidate, sizeof(RE::hkaAnimation)))
                break;
            const RawAnimationReading raw = read_raw_animation(candidate);
            if (!raw.valid)
                continue;
            ++walk.region_hits;
            consider_animation(candidate, walk, out);
            if (walk.region_hits <= Max_Tree_Lines)
            {
                logger::info("SCOPY ANIM load region-hit at=0x{:x} offset=0x{:x} type={} duration={:.2f}s tracks={} data={}",
                    reinterpret_cast<std::uintptr_t>(candidate), offset, animation_type_name(raw.type),
                    raw.duration, raw.tracks,
                    animation_data_state(reinterpret_cast<const RE::hkaAnimation*>(candidate), raw.tracks));
                if (raw.type == static_cast<std::uint32_t>(RE::hkaAnimation::AnimationType::kSplineCompressedAnimation))
                    report_spline_fields(reinterpret_cast<const RE::hkaAnimation*>(candidate), raw.tracks);
            }
        }
    }

    // Every byte range a resource's own header points at, found without assuming which field is which:
    // any adjacent pair that reads as {heap pointer, plausible byte count} is treated as one region.
    void scan_resource_regions(void const* resource, LoadedWalk& walk, std::vector<const RE::hkaAnimation*>& out)
    {
        if (out.size() >= Max_Loaded_Anims || !resource || !readable(resource, Resource_Dump_Bytes))
            return;
        scan_region_for_animations(resource, Resource_Dump_Bytes, walk, out);

        const auto* const words = static_cast<const std::uint64_t*>(resource);
        for (size_t word = 0; word + 1 < Resource_Dump_Bytes / sizeof(std::uint64_t); ++word)
        {
            const void* const pointer = reinterpret_cast<void const*>(words[word]);
            const std::int32_t size = static_cast<std::int32_t>(words[word + 1] & 0xFFFFFFFF);
            if (!pointer || reinterpret_cast<std::uintptr_t>(pointer) < 0x10000)
                continue;
            if (size < static_cast<std::int32_t>(sizeof(RE::hkaAnimation)) || size > static_cast<std::int32_t>(Max_Region_Scan_Bytes))
                continue;
            if (!readable(pointer, 0x20))
                continue;
            if (walk.regions_scanned >= Max_Scanned_Regions)
                return;
            ++walk.regions_scanned;
            logger::info("SCOPY ANIM load region at=0x{:x} bytes={} from-offset=0x{:x}",
                reinterpret_cast<std::uintptr_t>(pointer), size, word * sizeof(std::uint64_t));
            scan_region_for_animations(pointer, static_cast<size_t>(size), walk, out);
        }
    }

    // an animation file loads to is a container, and the animation sits some levels inside it, so one
    // level of pointers is not enough to reach it. Every candidate is validated before it is believed,
    // and nothing virtual is called on any of them.
    void collect_resource_animations(void const* resource, LoadedWalk& walk, std::vector<const RE::hkaAnimation*>& out)
    {
        if (out.size() >= Max_Loaded_Anims || !resource || !readable(resource, sizeof(void*)))
            return;

        if (walk.first_resource.empty())
        {
            walk.resource_type = resource_type_name(resource);
            const auto* const words = static_cast<const std::uint64_t*>(resource);
            for (size_t word = 0; word < Resource_Dump_Bytes / sizeof(std::uint64_t); ++word)
                walk.first_resource += fmt::format("{}{:016x}", word ? " " : "", words[word]);
        }

        std::unordered_set<void const*> seen{ resource };
        std::vector<const void*> pending{ resource };
        size_t visited = 0;
        while (!pending.empty() && visited < Max_Resource_Objects && out.size() < Max_Loaded_Anims)
        {
            const void* const object = pending.back();
            pending.pop_back();
            ++visited;
            consider_animation(object, walk, out);

            // One line per object, so the shape of what a loaded animation resource actually holds is
            // visible instead of inferred. Each line carries the object's own vtable and header words
            // plus how the animation validation judged it, which is what says whether the animation is
            // here at all or whether this walk is still looking in the wrong place.
            if (walk.tree_lines < Max_Tree_Lines && readable(object, 0x20))
            {
                ++walk.tree_lines;
                const auto* const words = static_cast<const std::uint64_t*>(object);
                std::string qwords;
                for (size_t word = 0; word < 4; ++word)
                    qwords += fmt::format("{}{:016x}", word ? " " : "", words[word]);
                const RawAnimationReading raw = read_raw_animation(object);
                const std::uintptr_t vtable = *static_cast<const std::uintptr_t*>(object);
                logger::info("SCOPY ANIM load tree obj=0x{:x} vtable=0x{:x} animation={} reason={} type={} tracks={} qwords='{}'",
                    reinterpret_cast<std::uintptr_t>(object), vtable, raw.valid, raw.reason,
                    animation_type_name(raw.type), raw.tracks, qwords);
            }
            for (std::int32_t offset = 0; offset + static_cast<std::int32_t>(sizeof(void*)) <= Container_Scan_Bytes; offset += static_cast<std::int32_t>(sizeof(void*)))
            {
                const auto* const at = static_cast<const std::uint8_t*>(object) + offset;
                if (!readable(at, sizeof(void*)))
                    break;
                // The address itself may be where an inline object begins, and the word there may point
                // at one; both are tried, and each is validated before it is recorded.
                consider_animation(at, walk, out);
                const void* const word = *reinterpret_cast<void* const*>(at);
                if (word && readable(word, sizeof(void*)) && looks_like_object(word) && seen.insert(word).second)
                    pending.push_back(word);
            }
        }

        // The graph walk cannot leave the pointer-shaped objects, and the arena holds plain bytes, so it
        // is scanned by content afterwards rather than reached by following anything.
        scan_resource_regions(resource, walk, out);
    }

    void walk_loaded_record(void const* record, LoadedWalk& walk, std::vector<const RE::hkaAnimation*>& out)
    {
        if (out.size() >= Max_Loaded_Anims || !record || !readable(record, 0x30))
            return;
        ++walk.records;
        void const* const db_data = *reinterpret_cast<void* const*>(static_cast<const std::uint8_t*>(record) + 0x28);
        if (!db_data || !readable(db_data, sizeof(hkLoaderLayout)) || !looks_like_object(db_data))
            return;
        ++walk.with_loader;

        if (walk.first_loader.empty())
        {
            const auto* const words = static_cast<const std::uint64_t*>(db_data);
            for (size_t word = 0; word < sizeof(hkLoaderLayout) / sizeof(std::uint64_t); ++word)
                walk.first_loader += fmt::format("{}{:016x}", word ? " " : "", words[word]);
        }

        const auto* const loader = static_cast<const hkLoaderLayout*>(db_data);
        const std::int32_t count = loader->loadedData.size();
        void* const* const data = loader->loadedData.data();
        if (count <= 0 || count > Max_Loaded_Resources || !data ||
            !readable(data, sizeof(void*) * static_cast<size_t>(count)))
            return;
        ++walk.with_resources;
        walk.resources += static_cast<size_t>(count);
        for (std::int32_t i = 0; i < count; ++i)
            collect_resource_animations(data[i], walk, out);
    }

    // The manager's own "current animation" slot is the record a load has just touched, so it is walked
    // first and on its own: whatever the load produced is behind this pointer even when no other record
    // has been filled in yet.
    void walk_loaded_storage(RE::AnimationFileManagerSingleton& manager, LoadedWalk& walk, std::vector<const RE::hkaAnimation*>& out)
    {
        walk_loaded_record(manager.currentAnimation.unk00, walk, out);
        const std::uint32_t limit = std::min<std::uint32_t>(manager.loadedAnimations.size(), Max_Walked_Records);
        for (std::uint32_t i = 0; i < limit; ++i)
            walk_loaded_record(manager.loadedAnimations[i].unk00, walk, out);
    }

    // A clip generator is fully typed, so a pointer to one is believed only when everything that makes
    // it one reads plausibly: a name that looks like an animation file, a binding that validates, a
    // playback mode the engine defines and a speed the engine could be running at.
    struct ClipReading
    {
        bool valid;
        const char* reason;
        std::string name;
        const RE::hkaAnimationBinding* binding;
    };

    // The engine's own vtable addresses, resolved once through the address library. HKX7 died asking
    // GetClassType about pointers it had merely guessed at; equality with a known vtable answers the
    // same question with one guarded read and no call, so it is safe on anything the walk can read.
    std::uintptr_t clip_generator_vtable()
    {
        static const std::uintptr_t vtable = RE::hkbClipGenerator::VTABLE[0].address();
        return vtable;
    }

    std::uintptr_t state_machine_vtable()
    {
        static const std::uintptr_t vtable = RE::hkbStateMachine::VTABLE[0].address();
        return vtable;
    }

    std::uintptr_t state_info_vtable()
    {
        static const std::uintptr_t vtable = RE::hkbStateMachine::StateInfo::VTABLE[0].address();
        return vtable;
    }

    // What the walk believes an object to be, decided by its first word alone. "Other" is the honest
    // verdict for everything whose vtable is none of the three the search cares about.
    enum class GraphKind
    {
        Other,
        ClipGenerator,
        StateMachine,
        StateInfo
    };

    GraphKind classify(void const* object)
    {
        if (!readable(object, sizeof(void*)))
            return GraphKind::Other;
        const std::uintptr_t vtable = *static_cast<const std::uintptr_t*>(object);
        if (vtable == clip_generator_vtable())
            return GraphKind::ClipGenerator;
        if (vtable == state_machine_vtable())
            return GraphKind::StateMachine;
        if (vtable == state_info_vtable())
            return GraphKind::StateInfo;
        return GraphKind::Other;
    }

    const char* kind_name(GraphKind kind)
    {
        switch (kind)
        {
        case GraphKind::ClipGenerator:
            return "hkbClipGenerator";
        case GraphKind::StateMachine:
            return "hkbStateMachine";
        case GraphKind::StateInfo:
            return "hkbStateMachine$StateInfo";
        default:
            return "other";
        }
    }

    // Whether a generator is the shared template or a per-character instance. hkbNode::cloneState is a
    // typed byte at 0x42 (hkbNode.h:59); the archived root-machine dumps of HKX9 and HKX10 both decode
    // to 1 (kTemplate) with isActive false, which is why the walk from rootGenerator only ever met
    // templates. OAR never walks rootGenerator - it hooks Activate and receives the instance.
    const char* clone_state_name(std::uint8_t state)
    {
        switch (static_cast<RE::hkbNode::CloneState>(state))
        {
        case RE::hkbNode::CloneState::kDefault:
            return "kDefault";
        case RE::hkbNode::CloneState::kTemplate:
            return "kTemplate";
        case RE::hkbNode::CloneState::kClone:
            return "kClone";
        case RE::hkbNode::CloneState::kShareable:
            return "kShareable";
        default:
            return "unknown";
        }
    }

    bool is_clone_instance(void const* object)
    {
        if (!object || !readable(object, sizeof(RE::hkbNode)))
            return false;
        const auto* const node = static_cast<const RE::hkbNode*>(object);
        return static_cast<RE::hkbNode::CloneState>(static_cast<std::uint8_t>(node->cloneState.get())) ==
               RE::hkbNode::CloneState::kClone;
    }

    // What one met clip generator carries, valid or not: the name, the binding-set index it was born
    // with, and which of its two binding holders - the clip's own lazy link or the instance control -
    // points anywhere. HKX9's 66 clips all read valid names and empty bindings; this record is how the
    // next route finds a live one instead of guessing again.
    struct ClipSeen
    {
        std::string name;
        std::uint16_t binding_index = 0;
        const RE::hkaAnimationBinding* binding = nullptr;
        const RE::hkaDefaultAnimationControl* control = nullptr;
        float speed = 0.0f;
        std::uint8_t mode = 0;
    };

    ClipReading read_clip_generator(void const* candidate, std::int32_t bone_count)
    {
        if (!candidate || !readable(candidate, sizeof(RE::hkbClipGenerator)) || !looks_like_object(candidate))
            return { false, "not-object", {}, nullptr };
        const auto* const generator = static_cast<const RE::hkbClipGenerator*>(candidate);
        const std::string name = guarded_string(generator->animationName);

        // The caller matched this object's first word against hkbClipGenerator's own vtable, so the
        // name read above is not a guess: a name that fails here means the field offset is wrong,
        // which is exactly the alarm this reason is kept to raise.
        if (!mentions_hkx(name))
            return { false, "no-animation-name", {}, nullptr };
        if (static_cast<std::uint8_t>(generator->mode.get()) > 3)
            return { false, "playback-mode", {}, nullptr };
        const float speed = generator->playbackSpeed;
        if (!std::isfinite(speed) || speed <= 0.0f || speed > 10.0f)
            return { false, "playback-speed", {}, nullptr };

        // The runtime binding is a lazy link: an inactive clip carries only animationBindingIndex into
        // the binding set, so an empty pointer is the normal state of a paused menu rather than a
        // broken graph (HKX9: all 66 clips read valid names and empty bindings). The control is created
        // with the clip's instance, and the source is playing its idle as we capture - the active
        // clip's control is the one binding holder the graph has already filled in.
        const char* reason = "binding-null";
        if (generator->binding)
        {
            const BindingReading direct = read_binding(generator->binding, bone_count);
            if (direct.valid)
                return { true, "valid", name, generator->binding };
            reason = direct.reason;
        }
        if (const RE::hkaDefaultAnimationControl* const control = generator->animationControl.get();
            control && readable(control, sizeof(RE::hkaAnimationControl)) && looks_like_object(control))
        {
            if (const RE::hkaAnimationBinding* const via_control = control->binding)
            {
                const BindingReading indirect = read_binding(via_control, bone_count);
                if (indirect.valid)
                    return { true, "valid", name, via_control };
                if (std::string_view(reason) == "binding-null")
                    reason = "control-binding-invalid";
            }
        }
        return { false, reason, {}, nullptr };
    }

    struct ClipSearch
    {
        struct Step
        {
            void const* object;
            int depth;
        };

        std::vector<Step> pending;
        std::unordered_set<void const*> seen;
        std::vector<ClipReading> clips;
        std::vector<ClipSeen> met;
        std::unordered_map<std::string, size_t> classes;
        std::unordered_map<std::string, size_t> reasons;
        size_t visited = 0;
        size_t state_arrays = 0;
        size_t state_entries = 0;
        bool capped = false;
        bool root_reported = false;
    };

    // The first state machine the walk reaches is the graph root, and its raw words are printed once.
    // HKX8 ended with clips=0 and only the root ever got a class name, so whether the states array
    // reads empty, unreadable or at the wrong offset has to come from the object itself, not from a
    // third guess: the dump makes the layout visible whatever the answer turns out to be.
    void report_root_machine(void const* object, ClipSearch& search)
    {
        search.root_reported = true;
        const auto* const machine = static_cast<const RE::hkbStateMachine*>(object);
        const bool window = readable(object, sizeof(RE::hkbStateMachine));
        std::string name = "-";
        std::int32_t states = -1;
        std::uintptr_t data = 0;
        bool data_readable = false;
        if (window)
        {
            name = guarded_string(machine->name);
            states = machine->states.size();
            data = reinterpret_cast<std::uintptr_t>(machine->states.data());
            data_readable = machine->states.data() && states > 0 &&
                            readable(machine->states.data(), sizeof(void*) * static_cast<size_t>(states));
        }
        logger::info("SCOPY ANIM play root-sm name='{}' readable-0x108={} states.size={} data=0x{:x} data-readable={}",
            name, window, states, data, data_readable);

        std::string bytes;
        for (size_t word = 0; word * sizeof(std::uint64_t) < sizeof(RE::hkbStateMachine); ++word)
        {
            const auto* const address = static_cast<const std::uint8_t*>(object) + word * sizeof(std::uint64_t);
            if (!readable(address, sizeof(std::uint64_t)))
                break;
            bytes += fmt::format("{}{:016x}", word ? " " : "", *reinterpret_cast<const std::uint64_t*>(address));
        }
        logger::info("SCOPY ANIM play root-sm qwords='{}'", bytes);
    }

    // The graph is a tree the engine names by vtable: a state machine's states are the typed edge into
    // the tree, a state info carries its state's generator, and a clip generator is what the search is
    // for. Class comes first now, so every step is decided by a fact about the object rather than by
    // what its bytes could also be; a wrong step costs one histogram entry, never a call.
    void scan_for_clips(void const* root, std::int32_t bone_count, ClipSearch& search)
    {
        search.pending.push_back({ root, 0 });
        while (!search.pending.empty())
        {
            if (search.visited >= Max_Clip_Objects)
            {
                search.capped = true;
                break;
            }
            const ClipSearch::Step step = search.pending.back();
            search.pending.pop_back();
            if (!step.object || step.depth > Max_Clip_Depth)
                continue;
            if (!search.seen.insert(step.object).second || !looks_like_object(step.object))
                continue;
            ++search.visited;

            // A breadcrumb every so many objects: if a build ever dies in here again, the log says how
            // far the walk got instead of leaving nothing behind.
            if (search.visited % Progress_Every == 0)
                logger::info("SCOPY ANIM play progress objects={} pending={} clips={}", search.visited, search.pending.size(), search.clips.size());

            const GraphKind kind = classify(step.object);
            ++search.classes[kind_name(kind)];

            if (kind == GraphKind::ClipGenerator)
            {
                if (search.met.size() < Max_Clips_Tracked && readable(step.object, sizeof(RE::hkbClipGenerator)))
                {
                    const auto* const generator = static_cast<const RE::hkbClipGenerator*>(step.object);
                    ClipSeen record;
                    record.name = guarded_string(generator->animationName);
                    record.binding_index = generator->animationBindingIndex;
                    record.binding = generator->binding;
                    record.control = generator->animationControl.get();
                    record.speed = generator->playbackSpeed;
                    record.mode = static_cast<std::uint8_t>(generator->mode.get());
                    search.met.push_back(std::move(record));
                }
                const ClipReading clip = read_clip_generator(step.object, bone_count);
                if (clip.valid)
                    search.clips.push_back(clip);
                else
                    ++search.reasons[clip.reason];
                continue;  // a clip's fields are data, not graph nodes worth walking
            }

            if (step.depth == Max_Clip_Depth)
                continue;

            if (kind == GraphKind::StateMachine)
            {
                if (!search.root_reported)
                    report_root_machine(step.object, search);
                const auto* const machine = static_cast<const RE::hkbStateMachine*>(step.object);
                if (readable(&machine->states, sizeof(machine->states)))
                {
                    const std::int32_t states = machine->states.size();
                    if (states > 0 && states <= Max_States && readable(machine->states.data(), sizeof(void*) * static_cast<size_t>(states)))
                    {
                        ++search.state_arrays;
                        for (std::int32_t i = 0; i < states; ++i)
                        {
                            if (void const* const state = machine->states[i])
                            {
                                ++search.state_entries;
                                search.pending.push_back({ state, step.depth + 1 });
                            }
                        }
                    }
                }
            }

            // Scan as far as this object is readable rather than demanding the whole window: a state
            // info is 0x78 bytes and holds the generator of its state, so requiring 0x200 skipped
            // exactly the node the clips hang from.
            for (std::int32_t offset = 0; offset + static_cast<std::int32_t>(sizeof(void*)) <= Container_Scan_Bytes; offset += static_cast<std::int32_t>(sizeof(void*)))
            {
                const auto* const base = static_cast<const std::uint8_t*>(step.object) + offset;
                if (!readable(base, sizeof(void*)))
                    break;
                const void* const word = *reinterpret_cast<void* const*>(base);
                if (word && readable(word, sizeof(void*)) && looks_like_object(word))
                    search.pending.push_back({ word, step.depth + 1 });
            }
        }
    }

    // The character's own standing idle first, and the engine's own copy of it before a mod's
    // replacement of the same name.
    int clip_preference(std::string_view name)
    {
        if (is_base_idle(base_name_of(name)))
            return name.starts_with("data\\") || name.starts_with("data/") ? 1 : 0;
        return mentions_idle(base_name_of(name)) ? 2 : 3;
    }

    // A histogram as one log field: the classes a walk met, or the reasons it rejected candidates, most
    // frequent first. Which of the two is empty is what says whether the walk got lost or the clips are
    // somewhere else entirely.
    std::string summarise(std::unordered_map<std::string, size_t> const& counts)
    {
        std::vector<std::pair<std::string, size_t>> ordered(counts.begin(), counts.end());
        std::sort(ordered.begin(), ordered.end(), [](auto const& lhs, auto const& rhs) { return lhs.second > rhs.second; });
        std::string out;
        for (size_t i = 0; i < ordered.size() && i < Max_Clips_Reported; ++i)
        {
            if (!out.empty())
                out += ", ";
            out += fmt::format("{}:{}", ordered[i].first, ordered[i].second);
        }
        return out.empty() ? "-" : out;
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
    std::vector<std::pair<std::int32_t, std::string>> plain_idles;
    std::vector<std::pair<std::int32_t, std::string>> base_idles;
    std::int32_t idle_count = 0;
    for (std::int32_t i = 0; i < strings->animationNames.size(); ++i)
    {
        const char* const name = strings->animationNames[i].c_str();
        if (!name || !mentions_idle(name))
            continue;
        ++idle_count;
        if (idle_names.size() < Max_Catalogue_Names)
            idle_names.emplace_back(i, name);

        // The list is sorted by path, so the first matches are weapon and object idles. The plain ones
        // are those whose file name starts with "idle", and the character's own standing idle is one of
        // a few known file names - that is the animation a panel would play.
        const std::string_view view(name);
        const size_t slash = view.find_last_of("\\/");
        const std::string_view base = slash == std::string_view::npos ? view : view.substr(slash + 1);
        if (starts_with_idle(base) && plain_idles.size() < Max_Catalogue_Names)
            plain_idles.emplace_back(i, name);
        if (is_base_idle(base) && base_idles.size() < Max_Catalogue_Names)
            base_idles.emplace_back(i, name);
    }

    std::string idle_list;
    for (const auto& [index, name] : idle_names)
    {
        if (!idle_list.empty())
            idle_list += ", ";
        idle_list += fmt::format("{}@{}", name, index);
    }
    std::string plain_list;
    for (const auto& [index, name] : plain_idles)
    {
        if (!plain_list.empty())
            plain_list += ", ";
        plain_list += fmt::format("{}@{}", name, index);
    }
    std::string base_list;
    for (const auto& [index, name] : base_idles)
    {
        if (!base_list.empty())
            base_list += ", ";
        base_list += fmt::format("{}@{}", name, index);
    }
    logger::info("SCOPY ANIM catalogue idle-names={}/{} first='{}'", idle_count, strings->animationNames.size(), idle_list.empty() ? "-" : idle_list);
    logger::info("SCOPY ANIM catalogue idle-plain first='{}'", plain_list.empty() ? "-" : plain_list);
    logger::info("SCOPY ANIM catalogue idle-base first='{}'", base_list.empty() ? "-" : base_list);

    if (!set || !looks_like_object(set) || binding_count <= 0)
    {
        logger::info("SCOPY ANIM catalogue layout UNAVAILABLE reason=binding-set");
        return;
    }

    // No candidate layout is typed in this checkout, so the offsets are not guessed: one element's own
    // shape picks them - a vtable pointer means an object begins there, a heap pointer to such an
    // object means one is pointed at - and the spread of samples decides which candidate is real.
    const std::int32_t bone_count = selected.skeleton->bones.size();
    const NameIndex copy_by_name = index_by_name(copy_nodes);
    std::vector<std::int32_t> sample_indices;
    for (size_t step = 0; step < Catalogue_Sample_Count; ++step)
    {
        const std::int32_t index = static_cast<std::int32_t>(static_cast<size_t>(binding_count) * step / Catalogue_Sample_Count);
        if (index < binding_count)
            sample_indices.push_back(index);
    }

    // The element's first word says whether it is an object itself (a vtable) or a plain struct, which
    // is what decides whether the binding can be the element at all.
    size_t elements = 0;
    size_t object_like = 0;
    for (const std::int32_t index : sample_indices)
    {
        const void* const element = set->bindings[index];
        if (!element || !readable(element, sizeof(void*)))
            continue;
        ++elements;
        object_like += looks_like_object(element) ? 1 : 0;
    }
    logger::info("SCOPY ANIM catalogue elements={} object-like={}/{} scan=0x{:x}", elements, object_like, elements, Scan_Bytes);

    struct Candidate
    {
        std::int32_t offset;
        bool pointer;
    };

    std::vector<Candidate> candidates;
    const void* const shape = set->bindings[sample_indices.front()];
    if (readable(shape, Scan_Bytes))
    {
        for (std::int32_t offset = 0; offset + static_cast<std::int32_t>(sizeof(RE::hkaAnimationBinding)) <= Scan_Bytes; offset += static_cast<std::int32_t>(sizeof(void*)))
        {
            const auto* const base = static_cast<const std::uint8_t*>(shape) + offset;
            if (looks_like_object(base))
                candidates.push_back({ offset, false });
            else if (const void* const pointed = *reinterpret_cast<void* const*>(base); readable(pointed, sizeof(RE::hkaAnimationBinding)) && looks_like_object(pointed))
                candidates.push_back({ offset, true });
        }
    }
    if (candidates.empty())
    {
        logger::info("SCOPY ANIM catalogue layout UNAVAILABLE reason=no-object-in-element");
        return;
    }

    struct LayoutRow
    {
        std::int32_t offset;
        bool pointer;
        size_t valid;
        std::string reasons;
    };

    std::vector<LayoutRow> rows;
    for (const Candidate& candidate : candidates)
    {
        const size_t width = candidate.pointer ? sizeof(void*) : sizeof(RE::hkaAnimationBinding);
        LayoutRow row{ candidate.offset, candidate.pointer, 0, {} };
        for (const std::int32_t index : sample_indices)
        {
            const void* const element = set->bindings[index];
            if (!element || !readable(element, static_cast<size_t>(candidate.offset) + width))
                continue;
            const auto* const base = static_cast<const std::uint8_t*>(element) + candidate.offset;
            const RE::hkaAnimationBinding* binding = candidate.pointer
                ? *reinterpret_cast<RE::hkaAnimationBinding* const*>(base)
                : reinterpret_cast<const RE::hkaAnimationBinding*>(base);

            const BindingReading reading = read_binding(binding, bone_count);
            if (reading.valid)
                ++row.valid;
            else
            {
                if (!row.reasons.empty())
                    row.reasons += ",";
                row.reasons += reading.reason;
            }
        }
        rows.push_back(std::move(row));
    }

    for (const LayoutRow& row : rows)
    {
        logger::info("SCOPY ANIM catalogue candidate offset=0x{:02x} as={} valid={}/{} reasons='{}'",
            row.offset, row.pointer ? "pointer" : "value", row.valid, sample_indices.size(), row.reasons.empty() ? "-" : row.reasons);
    }

    // The raw bytes of two elements, so a layout no row matched can still be read off by hand.
    for (size_t pick = 0; pick < 2 && pick < sample_indices.size(); ++pick)
    {
        const std::int32_t index = sample_indices[pick ? sample_indices.size() / 2 : 0];
        const void* const element = set->bindings[index];
        if (!element || !readable(element, Raw_Dump_Bytes))
            continue;
        const auto* const words = static_cast<const std::uint64_t*>(element);
        std::string bytes;
        for (size_t word = 0; word < Raw_Dump_Bytes / sizeof(std::uint64_t); ++word)
            bytes += fmt::format("{}{:016x}", word ? " " : "", words[word]);
        logger::info("SCOPY ANIM catalogue raw index={} qwords='{}'", index, bytes);
    }

    const auto best = std::max_element(rows.begin(), rows.end(), [](LayoutRow const& lhs, LayoutRow const& rhs) { return lhs.valid < rhs.valid; });
    if (best == rows.end() || best->valid == 0)
    {
        logger::info("SCOPY ANIM catalogue layout UNAVAILABLE reason=no-candidate (read the raw dump)");
        return;
    }
    logger::info("SCOPY ANIM catalogue layout best=offset=0x{:02x}/{} valid={}/{}",
        best->offset, best->pointer ? "pointer" : "value", best->valid, sample_indices.size());

    const auto binding_at = [&](void const* element) -> const RE::hkaAnimationBinding*
    {
        const size_t width = best->pointer ? sizeof(void*) : sizeof(RE::hkaAnimationBinding);
        if (!element || !readable(element, static_cast<size_t>(best->offset) + width))
            return nullptr;
        const auto* const base = static_cast<const std::uint8_t*>(element) + best->offset;
        return best->pointer
            ? *reinterpret_cast<RE::hkaAnimationBinding* const*>(base)
            : reinterpret_cast<const RE::hkaAnimationBinding*>(base);
    };

    // The idle indices are where a name and a duration can be read side by side: the layout is right
    // when a name that says idle comes back with a plausible clip, and the probe line also says how
    // much of that animation the copy can actually be driven with.
    std::vector<std::pair<std::int32_t, std::string>> probes = base_idles;
    for (const auto& entry : plain_idles)
    {
        if (probes.size() >= Max_Catalogue_Names)
            break;
        probes.push_back(entry);
    }
    for (const auto& entry : idle_names)
    {
        if (probes.size() >= Max_Catalogue_Names)
            break;
        probes.push_back(entry);
    }

    for (const auto& [index, name] : probes)
    {
        if (index >= binding_count)
            continue;
        const RE::hkaAnimationBinding* const binding = binding_at(set->bindings[index]);
        const BindingReading reading = read_binding(binding, bone_count);
        if (!reading.valid)
        {
            logger::info("SCOPY ANIM catalogue probe index={} name='{}' valid=false reason={}", index, name, reading.reason);
            continue;
        }

        const std::int16_t* const tracks = binding->transformTrackToBoneIndices.data();
        const std::int32_t track_count = binding->transformTrackToBoneIndices.size();
        size_t resolved = 0;
        std::string bones;
        for (std::int32_t track = 0; track < track_count; ++track)
        {
            const std::int16_t bone = tracks[track];
            if (bone < 0 || bone >= bone_count)
                continue;
            const char* const bone_name = selected.skeleton->bones[bone].name.c_str();
            if (!bone_name)
                continue;
            if (copy_by_name.find(bone_name) != copy_by_name.end())
                ++resolved;
            if (track < static_cast<std::int32_t>(Max_Bones_Reported))
            {
                if (!bones.empty())
                    bones += ", ";
                bones += bone_name;
            }
        }
        logger::info("SCOPY ANIM catalogue probe index={} name='{}' type={} duration={:.2f}s frames={} tracks={} animation-tracks={} skeleton-name='{}' bones='{}' copy-resolved={}/{}",
            index, name, animation_type_name(reading.type), reading.duration,
            binding->animation->GetNumOriginalFrames(),
            reading.tracks, binding->animation->numberOfTransformTracks,
            guarded_string(binding->originalSkeletonName), bones,
            resolved, reading.tracks);
    }
}

bool prepare_animation_clip(RE::TESObjectREFR& source, RE::NiAVObject& source_root, RE::NiAVObject& copy_root, ClipPlayback& out, const RE::hkaAnimation* a_instance_animation, const RE::hkaAnimationBinding* a_loaded_binding, float a_local_time)
{
    out = ClipPlayback{};
    const NodeTable copy_nodes = collect_nodes(copy_root);
    RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
    if (copy_nodes.names.empty() || !source.GetAnimationGraphManager(manager) || !manager)
    {
        logger::info("SCOPY ANIM play UNAVAILABLE reason=no-animation-graph-manager");
        return false;
    }

    SelectedGraph selected{};
    if (!select_graph(*manager, source, source_root, copy_nodes, selected, false))
    {
        logger::info("SCOPY ANIM play UNAVAILABLE reason=no-animation-skeleton");
        return false;
    }

    RE::hkbBehaviorGraph* const behavior = selected.graph->characterInstance.behaviorGraph.get();
    RE::hkbGenerator* const root_generator = behavior ? behavior->rootGenerator.get() : nullptr;
    if (!root_generator)
    {
        logger::info("SCOPY ANIM play UNAVAILABLE reason=no-root-generator");
        return false;
    }

    const std::int32_t bone_count = selected.skeleton->bones.size();
    ClipSearch search;
    scan_for_clips(root_generator, bone_count, search);
    std::string clip_list;
    for (size_t i = 0; i < search.clips.size() && i < Max_Clips_Reported; ++i)
    {
        if (!clip_list.empty())
            clip_list += ", ";
        clip_list += fmt::format("{}@{:.2f}s", search.clips[i].name, search.clips[i].binding->animation->duration);
    }
    logger::info("SCOPY ANIM play search objects={} clips={} states={}/{} capped={} first='{}'", search.visited, search.clips.size(), search.state_arrays, search.state_entries, search.capped, clip_list.empty() ? "-" : clip_list);
    logger::info("SCOPY ANIM play classes='{}'", summarise(search.classes));
    logger::info("SCOPY ANIM play rejections='{}'", summarise(search.reasons));

    // What the walk met, valid or not: the first ones in discovery order, then every one whose file
    // name is one of the character's own standing idles - the clip that is playing as we capture is
    // the likeliest holder of a live binding, and its line says where that binding lives.
    for (size_t i = 0; i < search.met.size(); ++i)
    {
        const ClipSeen& record = search.met[i];
        const bool idle = is_base_idle(base_name_of(record.name));
        if (i >= Max_Clips_Printed && !idle)
            continue;
        logger::info("SCOPY ANIM play clip-seen i={} idle={} name='{}' binding-index={} binding=0x{:x} control=0x{:x} speed={:.2f} mode={}",
            i, idle ? "yes" : "no", record.name, record.binding_index,
            reinterpret_cast<std::uintptr_t>(record.binding), reinterpret_cast<std::uintptr_t>(record.control),
            record.speed, record.mode);
    }

    // The engine lazy-loads animation files: a clip's activation queues its file and the manager keeps
    // what it loaded, keyed by the game's own CRC of the lowercased file name. OAR's source pointed
    // here after the binding set proved to be a registry of empty stubs; every step is CLib-typed, and
    // a candidate is believed only after the validation any binding's animation must pass.
    struct LoadedRecord
    {
        size_t index;
        std::uint32_t crc_name;
        std::uint32_t crc_path;
        std::uint16_t counter;
        const void* data;
        const RE::hkaAnimation* animation;
        RawAnimationReading reading;
        std::string name;
    };
    std::vector<LoadedRecord> loaded_records;

    RE::AnimationFileManagerSingleton* const file_manager = RE::AnimationFileManagerSingleton::GetSingleton();
    if (!file_manager)
    {
        logger::info("SCOPY ANIM animmgr UNAVAILABLE reason=no-manager");
    }
    else
    {
        logger::info("SCOPY ANIM animmgr queued={} loaded={}", file_manager->queuedAnimations.size(), file_manager->loadedAnimations.size());

        // What each CRC would be called: the name table spells out every animation the project knows,
        // so a matching CRC names a loaded entry without any guessing.
        std::unordered_map<std::uint32_t, std::string> name_by_crc;
        if (const auto setup = selected.graph->characterInstance.setup.get())
            if (const auto data = setup->data.get())
                if (const auto strings = data->stringData.get())
                {
                    for (const RE::hkStringPtr& animation_name : strings->animationNames)
                    {
                        const char* const text = animation_name.c_str();
                        if (!text)
                            continue;
                        const std::string_view view(text);
                        const size_t slash = view.find_last_of("\\/");
                        const std::string_view file = slash == std::string_view::npos ? view : view.substr(slash + 1);
                        name_by_crc.emplace(bscrc32(lowercased(strip_extension(file))), std::string(view));
                    }
                }

        bool dumped_rejected = false;
        for (std::uint32_t i = 0; i < file_manager->loadedAnimations.size(); ++i)
        {
            const RE::AnimationFileManagerSingleton::LoadedAnimation& loaded = file_manager->loadedAnimations[i];
            LoadedRecord record;
            record.index = i;
            record.crc_name = loaded.fileInfo.crc32Filename;
            record.crc_path = loaded.fileInfo.crc32Path;
            record.counter = loaded.counter;
            record.data = loaded.unk00;
            record.reading = read_raw_animation(loaded.unk00);
            record.animation = record.reading.valid ? static_cast<const RE::hkaAnimation*>(loaded.unk00) : nullptr;
            const auto named = name_by_crc.find(record.crc_name);
            record.name = named != name_by_crc.end() ? named->second : "-";
            loaded_records.push_back(std::move(record));

            // The first rejected pointer is dumped raw: if the manager stores a wrapper rather than
            // the animation itself, the dump says so without a second round.
            if (!record.reading.valid && !dumped_rejected && record.data && readable(record.data, Element_Dump_Bytes))
            {
                dumped_rejected = true;
                const auto* const words = static_cast<const std::uint64_t*>(record.data);
                std::string bytes;
                for (size_t word = 0; word < Element_Dump_Bytes / sizeof(std::uint64_t); ++word)
                    bytes += fmt::format("{}{:016x}", word ? " " : "", words[word]);
                logger::info("SCOPY ANIM animmgr rejected-dump i={} at=0x{:x} qwords='{}'",
                    record.index, reinterpret_cast<std::uintptr_t>(record.data), bytes);
            }
        }

        size_t printed = 0;
        for (const LoadedRecord& record : loaded_records)
        {
            const bool idle = is_base_idle(base_name_of(record.name));
            if (printed >= Max_Loaded_Printed && !record.reading.valid && !idle)
                continue;
            logger::info("SCOPY ANIM animmgr loaded i={} idle={} file='{}' crc=0x{:08x} counter={} data=0x{:x} probe={} type={} duration={:.2f}s tracks={}",
                record.index, idle ? "yes" : "no", record.name, record.crc_name, record.counter,
                reinterpret_cast<std::uintptr_t>(record.data), record.reading.reason,
                animation_type_name(record.reading.type), record.reading.duration, record.reading.tracks);
            ++printed;
        }
    }

    // The engine's own index-to-animation resolver, named by OAR's patch (GetHashedAnimFromAnimIndex
    // walks the project DB off the graph): whatever it returns for the standing idle's binding index is
    // the entry the engine itself uses, so the loaded contents behind it are that animation - no CRC
    // matching required. The return is logged raw and then walked: a record shaped like the hashed
    // entries carries the loaded data one hop further, and every pointer in reach is validated before
    // it is believed.
    const RE::hkaAnimation* hashed_animation = nullptr;
    std::string hashed_name;
    std::int32_t hashed_index = -1;
    for (const ClipSeen& record : search.met)
    {
        if (is_base_idle(base_name_of(record.name)))
        {
            hashed_index = static_cast<std::int32_t>(record.binding_index);
            break;
        }
    }
    if (hashed_index < 0 && !search.met.empty())
        hashed_index = static_cast<std::int32_t>(search.met.front().binding_index);

    if (hashed_index < 0)
    {
        logger::info("SCOPY ANIM hashed UNAVAILABLE reason=no-binding-index");
    }
    else
    {
        using GetHashedAnim_t = void* (*)(RE::BShkbAnimationGraph*, std::int32_t);
        static REL::Relocation<GetHashedAnim_t> get_hashed_anim{ REL::VariantID(62655, 63600, 0xB2BAA0) };
        void* const hashed = get_hashed_anim(selected.graph, hashed_index);
        logger::info("SCOPY ANIM hashed index={} at=0x{:x}", hashed_index, reinterpret_cast<std::uintptr_t>(hashed));

        const void* walked_root = hashed;
        if (hashed && readable(hashed, 0x30) &&
            std::string_view(reinterpret_cast<const char*>(hashed) + 4, 3) == "hkx")
        {
            const void* const db_data = *reinterpret_cast<void* const*>(static_cast<const std::uint8_t*>(hashed) + 0x28);
            logger::info("SCOPY ANIM hashed record db-data=0x{:x}", reinterpret_cast<std::uintptr_t>(db_data));
            if (db_data && looks_like_object(db_data))
                walked_root = db_data;
        }

        std::vector<const RE::hkaAnimation*> found;
        collect_raw_animations(walked_root, found);
        if (!found.empty())
        {
            hashed_animation = found.front();
            for (size_t i = 0; i < found.size(); ++i)
            {
                const RawAnimationReading reading = read_raw_animation(found[i]);
                logger::info("SCOPY ANIM hashed anim i={} at=0x{:x} type={} duration={:.2f}s tracks={}",
                    i, reinterpret_cast<std::uintptr_t>(found[i]), animation_type_name(reading.type),
                    reading.duration, reading.tracks);
            }
            hashed_name = fmt::format("hashed:{}", hashed_index);
        }
        else
        {
            logger::info("SCOPY ANIM hashed anims found=0");
        }
    }

    const NameIndex copy_by_name = index_by_name(copy_nodes);

    // The engine's own pose is indexed like its bone table, not like the animation skeleton (measured
    // in HKX2), so the ground truth is looked up by bone name.
    const RE::hkQsTransform* const pose = selected.graph->characterInstance.poseLocal;
    const std::int32_t pose_count = selected.graph->characterInstance.numPoseLocal;
    std::unordered_map<std::string_view, const RE::hkQsTransform*> pose_by_bone;
    if (pose && pose_count > 0)
    {
        for (std::int32_t i = 0; i < static_cast<std::int32_t>(selected.graph->boneNodes.size()) && i < pose_count; ++i)
        {
            RE::NiNode* const node = selected.graph->boneNodes[i].node;
            const char* const name = node ? node->name.c_str() : nullptr;
            if (name)
                pose_by_bone.emplace(name, pose + i);
        }
    }

    ClipPlayback playback;
    std::vector<const RE::hkQsTransform*> truth;
    std::int32_t total_tracks = 0;

    // An engine-loaded binding is used first when there is one: it belongs to an index this character
    // registers, and its track-to-bone table is the authoritative mapping - the same entry OAR reads
    // (FakeClipGenerator.cpp:113-125). A clip the walk validated is the fallback.
    const RE::hkaAnimationBinding* chosen_binding = a_loaded_binding;
    std::string chosen_name = a_loaded_binding ? "engine-loaded" : std::string{};
    if (!search.clips.empty())
    {
        // The graph's own link: a validated clip binding carries the authoritative track-to-bone table.
        const ClipReading* chosen = &search.clips.front();
        for (const ClipReading& clip : search.clips)
        {
            if (clip_preference(clip.name) < clip_preference(chosen->name))
                chosen = &clip;
        }
        chosen_binding = chosen->binding;
        chosen_name = chosen->name;
    }

    if (chosen_binding)
    {
        const RE::hkaAnimationBinding* const binding = chosen_binding;
        playback.animation = binding->animation.get();
        playback.name = chosen_name;
        playback.duration = playback.animation->duration;
        total_tracks = binding->transformTrackToBoneIndices.size();
        const std::int16_t* const track_to_bone = binding->transformTrackToBoneIndices.data();
        for (std::int32_t track = 0; track < total_tracks; ++track)
        {
            const std::int16_t bone = track_to_bone[track];
            if (bone < 0 || bone >= bone_count)
                continue;
            const char* const bone_name = selected.skeleton->bones[bone].name.c_str();
            if (!bone_name)
                continue;
            const auto node = copy_by_name.find(bone_name);
            if (node == copy_by_name.end())
                continue;
            playback.tracks.push_back(static_cast<std::uint16_t>(track));
            playback.nodes.push_back(node->second);
            const auto entry = pose_by_bone.find(bone_name);
            truth.push_back(entry != pose_by_bone.end() ? entry->second : nullptr);
        }
    }
    else
    {
        // Two engine storages, one assumption: the animation shares the rig's skeleton, so track i
        // drives bone i - the ground-truth sweep below is what verifies it. The engine's own resolution
        // of the standing idle's binding index comes first; a validated entry of the file manager's
        // loaded table is the fallback.
        const RE::hkaAnimation* candidate = hashed_animation;
        std::string candidate_name = hashed_name;
        if (!candidate)
        {
            const LoadedRecord* best = nullptr;
            for (const LoadedRecord& record : loaded_records)
            {
                if (!record.reading.valid || !record.animation)
                    continue;
                if (is_base_idle(base_name_of(record.name)))
                {
                    best = &record;
                    break;
                }
                if (!best && mentions_idle(base_name_of(record.name)))
                    best = &record;
            }
            if (!best)
            {
                for (const LoadedRecord& record : loaded_records)
                {
                    if (record.reading.valid && record.animation)
                    {
                        best = &record;
                        break;
                    }
                }
            }
            if (best)
            {
                candidate = best->animation;
                candidate_name = best->name == "-" ? fmt::format("crc:{:08x}", best->crc_name) : best->name;
            }
        }
        // The animation the instance side handed over, when the binding route had none. Its track table
        // is unfilled, so the mapping is not taken from the binding - it is measured here.
        if (!candidate && a_instance_animation)
        {
            candidate = a_instance_animation;
            candidate_name = "instance-binding";
        }
        if (!candidate)
        {
            logger::info("SCOPY ANIM play UNAVAILABLE reason=no-clip-generator");
            return false;
        }

        // Sampling reads through the pointer but writes nothing; the const is dropped only here, at
        // the single hand-off into the playback structure.
        playback.animation = const_cast<RE::hkaAnimation*>(candidate);
        playback.name = candidate_name;
        playback.duration = candidate->duration;
        total_tracks = std::min(read_raw_animation(candidate).tracks, bone_count);

        // No binding means no track-to-bone table, and the animation's own track count need not match
        // the skeleton the copy was aligned against - this character reports 99 tracks against 116
        // bones on one graph and 99 against 99 on the other. Rather than assume which skeleton the
        // tracks are indexed by, every graph's skeleton is tried and the one that reproduces the
        // engine's own pose wins. The mapping is thereby measured against an independent reference
        // (hkbCharacter::poseLocal, read in HKX2) instead of assumed, which is what the PRD requires
        // before a mapping may be used.
        const RE::hkaSkeleton* best_skeleton = selected.skeleton;
        float best_score = std::numeric_limits<float>::max();
        std::vector<std::pair<RE::hkaSkeleton const*, float>> skeleton_scores;
        for (uint32_t i = 0; i < manager->graphs.size(); ++i)
        {
            RE::BShkbAnimationGraph* const graph = manager->graphs[i].get();
            if (!graph)
                continue;
            const RE::hkaSkeleton* const skeleton = graph->characterInstance.setup.get()
                ? graph->characterInstance.setup->animationSkeleton.get()
                : nullptr;
            if (!skeleton || !looks_like_object(skeleton) || skeleton->bones.empty())
                continue;

            const std::int32_t available = std::min(total_tracks, static_cast<std::int32_t>(skeleton->bones.size()));
            std::vector<std::uint16_t> tracks;
            std::vector<const RE::hkQsTransform*> poses;
            for (std::int32_t track = 0; track < available; ++track)
            {
                const char* const bone_name = skeleton->bones[track].name.c_str();
                if (!bone_name)
                    continue;
                const auto entry = pose_by_bone.find(bone_name);
                if (entry == pose_by_bone.end())
                    continue;
                tracks.push_back(static_cast<std::uint16_t>(track));
                poses.push_back(entry->second);
            }
            if (tracks.size() < Min_Tracks_For_Skeleton_Test)
                continue;
            if (!animation_data_is_present(candidate, total_tracks))
            {
                logger::info("SCOPY ANIM mapping UNAVAILABLE reason=animation-data-not-present");
                break;
            }

            // One sample at an arbitrary phase is enough to compare skeletons against each other: the
            // wrong skeleton pairs unrelated bones and cannot produce a small distance. The sampler takes
            // a contiguous run from track zero, so the whole run is sampled and each pair is then read
            // out by its own track number - the collected list can have gaps where a bone did not
            // resolve, and indexing the sample by position would shift every pair after such a gap.
            std::vector<RE::hkQsTransform> sampled(static_cast<size_t>(available));
            sample_tracks(*candidate, 0.0f, static_cast<std::uint32_t>(available), sampled.data(), nullptr, 0);
            float worst = 0.0f;
            for (size_t k = 0; k < tracks.size(); ++k)
            {
                if (!poses[k] || static_cast<size_t>(tracks[k]) >= sampled.size())
                    continue;
                const PoseSample clip_pose = read_pose(sampled[tracks[k]]);
                const PoseSample engine_pose = read_pose(*poses[k]);
                worst = std::max(worst, rotation_angle_degrees(
                    ni_matrix_from_quaternion(engine_pose.quaternion[0], engine_pose.quaternion[1], engine_pose.quaternion[2], engine_pose.quaternion[3]),
                    ni_matrix_from_quaternion(clip_pose.quaternion[0], clip_pose.quaternion[1], clip_pose.quaternion[2], clip_pose.quaternion[3])));
            }
            skeleton_scores.emplace_back(skeleton, worst);
            if (worst < best_score)
            {
                best_score = worst;
                best_skeleton = skeleton;
            }
        }

        for (const auto& entry : skeleton_scores)
        {
            logger::info("SCOPY ANIM mapping skeleton='{}' bones={} worst-rot-delta={:.2f}deg{}",
                entry.first->name.c_str() ? entry.first->name.c_str() : "-", entry.first->bones.size(),
                entry.second, entry.first == best_skeleton ? " best" : "");
        }

        for (std::int32_t track = 0; track < total_tracks; ++track)
        {
            if (track >= static_cast<std::int32_t>(best_skeleton->bones.size()))
                break;
            const char* const bone_name = best_skeleton->bones[track].name.c_str();
            if (!bone_name)
                continue;
            const auto node = copy_by_name.find(bone_name);
            if (node == copy_by_name.end())
                continue;
            playback.tracks.push_back(static_cast<std::uint16_t>(track));
            playback.nodes.push_back(node->second);
            const auto entry = pose_by_bone.find(bone_name);
            truth.push_back(entry != pose_by_bone.end() ? entry->second : nullptr);
        }
    }

    logger::info("SCOPY ANIM play clip='{}' type={} duration={:.2f}s tracks={} copy-resolved={}/{} truth-bones={}",
        playback.name, animation_type_name(static_cast<std::uint32_t>(playback.animation->type.get())), playback.duration,
        total_tracks, playback.tracks.size(), total_tracks, pose_by_bone.size());
    if (!playback.valid())
    {
        logger::info("SCOPY ANIM play UNAVAILABLE reason=no-track-resolves-to-the-copy");
        return false;
    }

    // Sampling de-references the animation's own per-track tables, and a header that validates says
    // nothing about those (HKX16 crashed here on a spline whose transformOffsets was absent). Refuse
    // before the first sample rather than inside it.
    if (!animation_data_is_present(playback.animation, total_tracks))
    {
        logger::info("SCOPY ANIM play UNAVAILABLE reason=animation-data-not-present");
        return false;
    }

    // Ground truth: the clip's own pose at every phase against the pose the engine holds. The source is
    // paused, so its phase is frozen and unknown - the sweep finds it, and the spread between the best
    // and the worst phase is what tells a match from a coincidence.
    // The sampler covers a contiguous run from track zero, and the collected list can have gaps where a
    // bone did not resolve, so the sample is taken over the whole run and each pair is read out by its
    // own track number - indexing by position would shift every pair after a gap.
    const size_t sample_count = playback.tracks.empty() ? 0 : static_cast<size_t>(playback.tracks.back()) + 1;
    std::vector<RE::hkQsTransform> sampled(sample_count);
    float best_position = std::numeric_limits<float>::max();
    float best_rotation = 0.0f;
    float best_time = 0.0f;
    float worst_position = 0.0f;
    size_t compared = 0;
    for (size_t step = 0; step < Sweep_Steps; ++step)
    {
        const float time = playback.duration * static_cast<float>(step) / static_cast<float>(Sweep_Steps);
        sample_tracks(*playback.animation, time, static_cast<std::uint32_t>(sample_count), sampled.data(), nullptr, 0);
        float position = 0.0f;
        float rotation = 0.0f;
        size_t bones = 0;
        for (size_t i = 0; i < playback.tracks.size(); ++i)
        {
            if (!truth[i] || static_cast<size_t>(playback.tracks[i]) >= sampled.size())
                continue;
            const PoseSample clip_pose = read_pose(sampled[playback.tracks[i]]);
            const PoseSample engine_pose = read_pose(*truth[i]);
            position = std::max(position, (RE::NiPoint3{ clip_pose.position[0], clip_pose.position[1], clip_pose.position[2] } -
                                              RE::NiPoint3{ engine_pose.position[0], engine_pose.position[1], engine_pose.position[2] })
                                                 .Length());
            const RE::NiMatrix3 clip_rotation = ni_matrix_from_quaternion(clip_pose.quaternion[0], clip_pose.quaternion[1], clip_pose.quaternion[2], clip_pose.quaternion[3]);
            const RE::NiMatrix3 engine_rotation = ni_matrix_from_quaternion(engine_pose.quaternion[0], engine_pose.quaternion[1], engine_pose.quaternion[2], engine_pose.quaternion[3]);
            rotation = std::max(rotation, rotation_angle_degrees(engine_rotation, clip_rotation));
            ++bones;
        }
        compared = std::max(compared, bones);
        worst_position = std::max(worst_position, position);
        if (position < best_position)
        {
            best_position = position;
            best_rotation = rotation;
            best_time = time;
        }
    }

    logger::info("SCOPY ANIM play ground-truth best-t={:.2f}s max-pos-delta={:.3f} max-rot-delta={:.2f}deg worst-pos-delta={:.3f} bones={}",
        best_time, best_position, best_rotation, worst_position, compared);

    // The same comparison at the phase the engine itself is at, taken from the control rather than found
    // by search. The sweep above is coarse - 24 steps across a clip of tens of seconds is over a second
    // per step, which on a moving skeleton is tens of frames of drift - so its best phase is a bound, not
    // the answer. This is the answer when the animation being sampled is the one being played: any
    // remaining difference is mapping error, not phase error, and that is what decides the mapping.
    if (a_local_time >= 0.0f && !sampled.empty())
    {
        sample_tracks(*playback.animation, a_local_time, static_cast<std::uint32_t>(sample_count), sampled.data(), nullptr, 0);
        float position = 0.0f;
        float rotation = 0.0f;
        size_t bones = 0;
        for (size_t i = 0; i < playback.tracks.size(); ++i)
        {
            if (!truth[i] || static_cast<size_t>(playback.tracks[i]) >= sampled.size())
                continue;
            const PoseSample clip_pose = read_pose(sampled[playback.tracks[i]]);
            const PoseSample engine_pose = read_pose(*truth[i]);
            position = std::max(position, (RE::NiPoint3{ clip_pose.position[0], clip_pose.position[1], clip_pose.position[2] } -
                                           RE::NiPoint3{ engine_pose.position[0], engine_pose.position[1], engine_pose.position[2] })
                                              .Length());
            const RE::NiMatrix3 clip_rotation = ni_matrix_from_quaternion(clip_pose.quaternion[0], clip_pose.quaternion[1], clip_pose.quaternion[2], clip_pose.quaternion[3]);
            const RE::NiMatrix3 engine_rotation = ni_matrix_from_quaternion(engine_pose.quaternion[0], engine_pose.quaternion[1], engine_pose.quaternion[2], engine_pose.quaternion[3]);
            rotation = std::max(rotation, rotation_angle_degrees(engine_rotation, clip_rotation));
            ++bones;
        }
        logger::info("SCOPY ANIM play phase-check t={:.3f}s max-pos-delta={:.3f} max-rot-delta={:.2f}deg bones={}",
            a_local_time, position, rotation, bones);

        // Which bones carry the rotation that is left, and how many are close to exact. A few named bones
        // far from the rest is the engine correcting those bones - blending, look-at, IK, physics - and the
        // PRD says a single clip cannot be required to equal that final pose. A spread across many bones
        // means the mapping itself is off in rotation, which needs a different fix. The two look identical
        // in one max value, so the distribution is what decides between them.
        struct BoneDelta
        {
            float rotation;
            float position;
            const char* name;
        };
        std::vector<BoneDelta> deltas;
        size_t within_one_degree = 0;
        for (size_t i = 0; i < playback.tracks.size(); ++i)
        {
            if (!truth[i] || static_cast<size_t>(playback.tracks[i]) >= sampled.size())
                continue;
            const PoseSample clip_pose = read_pose(sampled[playback.tracks[i]]);
            const PoseSample engine_pose = read_pose(*truth[i]);
            const RE::NiMatrix3 clip_rotation = ni_matrix_from_quaternion(clip_pose.quaternion[0], clip_pose.quaternion[1], clip_pose.quaternion[2], clip_pose.quaternion[3]);
            const RE::NiMatrix3 engine_rotation = ni_matrix_from_quaternion(engine_pose.quaternion[0], engine_pose.quaternion[1], engine_pose.quaternion[2], engine_pose.quaternion[3]);
            const float bone_rotation = rotation_angle_degrees(engine_rotation, clip_rotation);
            const float bone_position = (RE::NiPoint3{ clip_pose.position[0], clip_pose.position[1], clip_pose.position[2] } -
                                         RE::NiPoint3{ engine_pose.position[0], engine_pose.position[1], engine_pose.position[2] })
                                            .Length();
            if (bone_rotation <= 1.0f)
                ++within_one_degree;
            RE::NiAVObject* const node = i < playback.nodes.size() ? playback.nodes[i] : nullptr;
            const char* const node_name = node && node->name.c_str() ? node->name.c_str() : "?";
            deltas.push_back({ bone_rotation, bone_position, node_name });
        }
        std::sort(deltas.begin(), deltas.end(), [](const BoneDelta& lhs, const BoneDelta& rhs) { return lhs.rotation > rhs.rotation; });
        logger::info("SCOPY ANIM play phase-check spread bones={} within-1deg={} worst-bones='{}'",
            deltas.size(), within_one_degree, [&deltas]() {
                std::string text;
                for (size_t i = 0; i < deltas.size() && i < Max_Bones_Reported * 2; ++i)
                    text += fmt::format("{}{}:{:.2f}deg/{:.3f}u", i ? ", " : "", deltas[i].name, deltas[i].rotation, deltas[i].position);
                return text.empty() ? std::string("-") : text;
            }());
    }
    const bool matched = worst_position > 0.0f && best_position < worst_position * Match_Fraction_Of_Control;
    logger::info("SCOPY ANIM play ground-truth verdict={} best={:.4f} worst={:.3f}", matched ? "match" : "none", best_position, worst_position);

    out = std::move(playback);
    return true;
}

// Answers one question, and nothing else: on the side of the graph that actually plays - the
// per-character clone instances, not the shared template tree the earlier rounds walked - does a
// binding resolve at all, and if so does it carry a sampleable animation?
//
// Everything here is read-only and typed. The instance side is reached through the two members of
// hkbBehaviorGraph that the template walk never touched; every object is classified by its vtable
// address rather than by a virtual call; and a candidate entry is believed only once the structural
// checks in read_binding pass. Nothing is written and no clip is activated, so a wrong step costs a
// rejection reason in the log instead of a crash.
void report_animation_instance(RE::TESObjectREFR& source, RE::NiAVObject& source_root, RE::NiAVObject& copy_root, const RE::hkaAnimation*& out_animation, const RE::hkaAnimationBinding*& out_binding, float& out_local_time)
{
    out_animation = nullptr;
    out_binding = nullptr;
    out_local_time = -1.0f;
    const NodeTable copy_nodes = collect_nodes(copy_root);
    RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
    if (copy_nodes.names.empty() || !source.GetAnimationGraphManager(manager) || !manager)
    {
        logger::info("SCOPY ANIM instance UNAVAILABLE reason=no-animation-graph-manager");
        return;
    }

    SelectedGraph selected{};
    if (!select_graph(*manager, source, source_root, copy_nodes, selected, false))
    {
        logger::info("SCOPY ANIM instance UNAVAILABLE reason=no-animation-skeleton");
        return;
    }

    RE::hkbCharacter& character = selected.graph->characterInstance;
    RE::hkbBehaviorGraph* const behavior = character.behaviorGraph.get();
    if (!behavior)
    {
        logger::info("SCOPY ANIM instance UNAVAILABLE reason=no-behavior-graph");
        return;
    }

    const BindingSetPair sets = binding_sets(character);
    const std::int32_t bone_count = selected.skeleton->bones.size();
    logger::info("SCOPY ANIM instance graph={} char-bindingset=0x{:x}/{} setup-bindingset=0x{:x}/{} root-generator=0x{:x} root-clone=0x{:x} active-nodes={}",
        selected.index,
        reinterpret_cast<std::uintptr_t>(sets.character), sets.character ? sets.character->bindings.size() : 0,
        reinterpret_cast<std::uintptr_t>(sets.setup), sets.setup ? sets.setup->bindings.size() : 0,
        reinterpret_cast<std::uintptr_t>(behavior->rootGenerator.get()),
        reinterpret_cast<std::uintptr_t>(behavior->rootGeneratorClone.get()),
        behavior->activeNodes ? behavior->activeNodes->size() : 0);

    // Which side of the graph each start point belongs to. The template root is what HKX6-12 walked;
    // the clone root and the active-node list are what the engine updates.
    logger::info("SCOPY ANIM instance roots generator={} clone={}",
        is_clone_instance(behavior->rootGenerator.get()) ? "kClone" : "template",
        is_clone_instance(behavior->rootGeneratorClone.get()) ? "kClone" : "template");

    // Instance-side clip generators: the clone root, then every node the engine holds as active.
    std::vector<const RE::hkbClipGenerator*> instances;
    std::unordered_set<void const*> pending;
    std::unordered_map<std::string, size_t> clone_states;
    auto consider = [&](void const* object)
    {
        if (!object || !readable(object, sizeof(RE::hkbNode)) || !looks_like_object(object))
            return;
        const auto* const node = static_cast<const RE::hkbNode*>(object);
        const char* const state = clone_state_name(static_cast<std::uint8_t>(node->cloneState.get()));
        ++clone_states[state];
        if (classify(object) == GraphKind::ClipGenerator)
            instances.push_back(static_cast<const RE::hkbClipGenerator*>(object));
    };

    // rootGeneratorClone is an hkRefVariant in this checkout, so it is read as a pointer rather than
    // through the typed member.
    const void* const clone_root = behavior->rootGeneratorClone.get();
    if (clone_root)
    {
        pending.insert(clone_root);
        consider(clone_root);
    }
    if (behavior->activeNodes)
    {
        for (const RE::hkbNodeInfo& info : *behavior->activeNodes)
        {
            if (info.nodeClone && pending.insert(info.nodeClone).second)
                consider(info.nodeClone);
        }
    }

    // One hop down the clone root so a clip nested in a state machine is still met. Bounded, vtable
    // classified, and it never calls anything.
    for (void const* const object : pending)
    {
        for (std::int32_t offset = 0; offset + static_cast<std::int32_t>(sizeof(void*)) <= Container_Scan_Bytes; offset += static_cast<std::int32_t>(sizeof(void*)))
        {
            const auto* const word_address = static_cast<const std::uint8_t*>(object) + offset;
            if (!readable(word_address, sizeof(void*)))
                break;
            const void* const word = *reinterpret_cast<void* const*>(word_address);
            if (!word || !looks_like_object(word) || !pending.insert(word).second)
                continue;
            consider(word);
            if (pending.size() >= Max_Instance_Objects)
                break;
        }
    }

    std::string states;
    for (const auto& entry : clone_states)
    {
        if (!states.empty())
            states += ", ";
        states += fmt::format("{}:{}", entry.first, entry.second);
    }
    logger::info("SCOPY ANIM instance objects={} clips={} clone-states='{}'",
        pending.size(), instances.size(), states.empty() ? "-" : states);

    if (instances.empty())
    {
        logger::info("SCOPY ANIM instance UNAVAILABLE reason=no-clip-on-the-instance-side");
        return;
    }

    // Every instance clip whose name is the character's own standing idle, plus the first few others:
    // the idle is the one the source is playing as we capture, so it is the one whose binding the
    // engine has had to fill in.
    size_t printed = 0;
    bool found_live = false;
    for (const RE::hkbClipGenerator* const generator : instances)
    {
        const std::string name = guarded_string(generator->animationName);
        const bool idle = is_base_idle(base_name_of(name));
        if (printed >= Max_Instance_Printed && !idle)
            continue;
        ++printed;

        const std::int32_t index = static_cast<std::int32_t>(generator->animationBindingIndex);
        const char* reason = "binding-null";
        const RE::hkaAnimationBinding* binding = generator->binding;
        if (!binding)
        {
            // OAR's fallback: the clip carries only an index, and the engine's own entry for that index
            // is where the binding comes from (FakeClipGenerator.cpp:113-126).
            if (RE::hkbAnimationBindingSet* const set = set_holding(sets, index))
                if (const void* const element = set->bindings[index])
                    binding = binding_at_offset_0x10(element);
            if (!binding)
                reason = "no-binding-in-set";
        }

        // Detail is printed only where the answer is still open: a binding that resolved but whose
        // track table did not, which is the one thing standing between this round and sampling.
        const BindingReading reading = read_binding(binding, bone_count, idle || printed <= 2);
        if (reading.valid)
            found_live = true;

        // The object the whole exercise exists for. The binding's track table is what maps a track to a
        // bone, and it is unfilled here, but that is only one way to learn the mapping - the animation
        // itself can be sampled directly and the mapping measured against the pose the engine holds, so
        // the animation is what this stage hands forward.
        if (!out_animation && binding && readable(binding, sizeof(RE::hkaAnimationBinding)))
        {
            const RE::hkaAnimation* const animation = binding->animation.get();
            if (read_raw_animation(animation).valid)
                out_animation = animation;
        }
        logger::info("SCOPY ANIM instance clip name='{}' idx={} clone-state={} binding=0x{:x} control=0x{:x} bind-src={} tracks={} reason={}",
            name, index, clone_state_name(static_cast<std::uint8_t>(generator->cloneState.get())),
            reinterpret_cast<std::uintptr_t>(binding), reinterpret_cast<std::uintptr_t>(generator->animationControl.get()),
            generator->binding ? "clip" : "bindingset", reading.tracks,
            reading.valid ? "valid" : std::string_view(reason) == "binding-null" ? reading.reason : reason);

        // The animation a live binding points at, described by its own typed fields. A sane type,
        // duration and track count here while the binding's array reads unreadable says the animation
        // object is real and it is the mapping - not the animation - that is missing.
        if (binding && readable(binding, sizeof(RE::hkaAnimationBinding)))
        {
            const RE::hkaAnimation* const animation = binding->animation.get();
            const RawAnimationReading raw = read_raw_animation(animation);
            logger::info("SCOPY ANIM instance anim binding=0x{:x} animation=0x{:x} probe={} type={} duration={:.2f}s transform-tracks={} float-tracks={}",
                reinterpret_cast<std::uintptr_t>(binding), reinterpret_cast<std::uintptr_t>(animation),
                raw.reason, animation_type_name(raw.type), raw.duration, raw.tracks,
                animation && readable(animation, sizeof(RE::hkaAnimation)) ? animation->numberOfFloatTracks : 0);
        }

        // Would the control's typed accessors help? They are virtual, so calling them is a decision for
        // the user, not for this probe. The question can be answered without calling them: the
        // accessors read through hkaAnimationControl::binding (@0x38), so if that is the very same
        // object whose track table is empty, they return the same empty table and are worth nothing.
        // A read-only dump of the control's own fields says which, and also shows whether the control
        // carries a track table of its own anywhere in its window.
        const RE::hkaDefaultAnimationControl* const control = generator->animationControl.get();
        if (control && readable(control, sizeof(RE::hkaAnimationControl)))
        {
            const RE::hkaAnimationBinding* const control_binding = control->binding;
            logger::info("SCOPY ANIM instance control at=0x{:x} binding@0x38=0x{:x} same-as-clip-binding={} localTime={:.3f} weight={:.3f} ttw-size={} ftw-size={}",
                reinterpret_cast<std::uintptr_t>(control), reinterpret_cast<std::uintptr_t>(control_binding),
                control_binding == binding, control->localTime, control->weight,
                control->transformTrackWeights.size(), control->floatTrackWeights.size());

            // The phase the engine is at in the animation it is playing. The sweep below finds a phase by
            // search, but this one is not searched for - it is the truth, and it is what the sample should
            // be compared against when the animation being sampled is the one being played.
            if (idle && out_local_time < 0.0f && std::isfinite(control->localTime))
                out_local_time = control->localTime;

            if (control_binding && control_binding != binding && readable(control_binding, sizeof(RE::hkaAnimationBinding)))
            {
                const std::int32_t control_tracks = control_binding->transformTrackToBoneIndices.size();
                const std::int16_t* const control_indices = control_binding->transformTrackToBoneIndices.data();
                logger::info("SCOPY ANIM instance control tracks=0x{:x} size={} readable={} animation=0x{:x}",
                    reinterpret_cast<std::uintptr_t>(control_binding), control_tracks,
                    control_indices != nullptr && control_tracks > 0 &&
                        readable(control_indices, sizeof(std::int16_t) * static_cast<size_t>(control_tracks)),
                    reinterpret_cast<std::uintptr_t>(control_binding->animation.get()));
            }

            // A wider look into the control itself: if it holds a mapping the binding does not, it will
            // be a heap pointer followed by a small count somewhere in this window.
            std::string bytes;
            for (size_t word = 0; word < sizeof(RE::hkaAnimationControl) / sizeof(std::uint64_t); ++word)
            {
                const auto* const address = static_cast<const std::uint8_t*>(static_cast<const void*>(control)) + word * sizeof(std::uint64_t);
                if (!readable(address, sizeof(std::uint64_t)))
                    break;
                bytes += fmt::format("{}{:016x}", word ? " " : "", *reinterpret_cast<const std::uint64_t*>(address));
            }
            logger::info("SCOPY ANIM instance control qwords='{}'", bytes);
        }
    }

    // The entry the engine itself resolves for the standing idle's index, dumped at OAR's 0x28 spacing
    // so the layout can be confirmed from a live sample rather than inferred.
    std::int32_t idle_index = -1;
    for (const RE::hkbClipGenerator* const generator : instances)
    {
        if (is_base_idle(base_name_of(guarded_string(generator->animationName))))
        {
            idle_index = static_cast<std::int32_t>(generator->animationBindingIndex);
            break;
        }
    }
    if (idle_index >= 0)
    {
        if (RE::hkbAnimationBindingSet* const set = set_holding(sets, idle_index))
        {
            const void* const element = set->bindings[idle_index];
            const RE::hkaAnimationBinding* const binding = binding_at_offset_0x10(element);
            const BindingReading reading = read_binding(binding, bone_count, true);
            logger::info("SCOPY ANIM instance element idx={} at=0x{:x} set={} binding@0x10=0x{:x} valid={} reason={}",
                idle_index, reinterpret_cast<std::uintptr_t>(element),
                set == sets.character ? "character" : "setup",
                reinterpret_cast<std::uintptr_t>(binding), reading.valid, reading.reason);

            if (element && readable(element, Element_Dump_Bytes))
            {
                const auto* const words = static_cast<const std::uint64_t*>(element);
                std::string bytes;
                for (size_t word = 0; word < Element_Dump_Bytes / sizeof(std::uint64_t); ++word)
                    bytes += fmt::format("{}{:016x}", word ? " " : "", words[word]);
                logger::info("SCOPY ANIM instance element qwords='{}'", bytes);
            }
        }
    }

    logger::info("SCOPY ANIM instance verdict={}", found_live ? "binding-resolved" : "no-live-binding");

    // Ask the engine to load the animation behind one of its own registered indices, then walk the
    // storage the engine loaded it into. The panel must play animations the game already knows - vanilla
    // or OAR-registered - so this never reads a file we supplied; it only ever names an index from the
    // character's own animation list and lets the engine fetch it.
    //
    // OAR's LoadAnimation (OpenAnimationReplacer.cpp:772-802) shows the loading functions need only the
    // binding index, carried in an object shaped like a clip generator. A real context is built here
    // instead of OAR's reinterpret cast, because that one hands the loader a character pointer where a
    // context is expected.
    //
    // Two read-only passes come first, so the effect of the load itself can be told apart from what was
    // already there.
    RE::AnimationFileManagerSingleton* const file_manager = RE::AnimationFileManagerSingleton::GetSingleton();
    if (!file_manager)
    {
        logger::info("SCOPY ANIM load UNAVAILABLE reason=no-manager");
        return;
    }

    LoadedWalk before_walk;
    std::vector<const RE::hkaAnimation*> before;
    walk_loaded_storage(*file_manager, before_walk, before);
    // The addresses the classification compares against, printed so a "type=other" can be told apart
    // from a wrong table: if the resource's own vtable equals one of these, the comparison is at fault;
    // if it equals none, the object is simply not one of these five classes.
    logger::info("SCOPY ANIM load vtables base=0x{:x} handle=0x{:x} container=0x{:x} map=0x{:x} resource=0x{:x}",
        RE::VTABLE_hkResourceBase[0].address(), RE::VTABLE_hkResourceHandle[0].address(),
        RE::VTABLE_hkResourceContainer[0].address(), RE::VTABLE_hkResourceMap[0].address(),
        RE::VTABLE_hkResource[0].address());
    logger::info("SCOPY ANIM load before records={} current=0x{:x} current-crc=0x{:08x} records-walked={} with-loader={} with-resources={} resources={} anims={}",
        file_manager->loadedAnimations.size(), reinterpret_cast<std::uintptr_t>(file_manager->currentAnimation.unk00),
        file_manager->currentAnimation.fileInfo.crc32Filename,
        before_walk.records, before_walk.with_loader, before_walk.with_resources, before_walk.resources, before.size());
    if (!before_walk.first_loader.empty())
        logger::info("SCOPY ANIM load loader qwords='{}'", before_walk.first_loader);
    if (!before_walk.first_resource.empty())
        logger::info("SCOPY ANIM load resource type={} qwords='{}'", before_walk.resource_type, before_walk.first_resource);

    const std::int32_t load_index = idle_index >= 0 ? idle_index : 1022;

    // A stand-in for a clip generator, sized like a real one so that a write past the fields the loader
    // reads cannot run off the end of the object. A plain buffer is used rather than a real
    // hkbClipGenerator because constructing one would run the engine's reference-counted members over
    // uninitialised bytes; nothing virtual is called on this and no destructor is involved.
    alignas(16) std::uint8_t carrier[sizeof(RE::hkbClipGenerator)]{};
    *reinterpret_cast<std::uint16_t*>(carrier + 0x70) = static_cast<std::uint16_t>(load_index);

    // hkbContext cannot be default-constructed here: two of its members are hkRefPtr/hkRefVariant whose
    // pointee types are only forward-declared in this checkout, so their destructors have nothing to
    // compile against. A zeroed buffer of its asserted size, with the two fields the loader reads filled
    // in, is what the engine is handed instead - the same shape OAR passes, except that everything past
    // those two fields is zero rather than whatever happened to be on the stack.
    struct ContextLayout
    {
        RE::hkbCharacter* character;     // 00
        RE::hkbBehaviorGraph* behavior;  // 08
        std::uint64_t pad10;
        std::uint64_t pad18;
        std::uint64_t pad20;
        std::uint64_t pad28;
        std::uint64_t pad30;
        std::uint64_t pad38;
        std::uint64_t pad40;
        std::uint64_t pad48;
    };
    static_assert(sizeof(ContextLayout) == 0x50);

    ContextLayout context{};
    context.character = &character;
    context.behavior = behavior;
    const RE::hkbContext& context_ref = reinterpret_cast<const RE::hkbContext&>(context);

    const std::int32_t queued = file_manager->Queue(context_ref, reinterpret_cast<RE::hkbClipGenerator*>(&carrier), nullptr);
    const bool loaded = file_manager->Load(context_ref, reinterpret_cast<RE::hkbClipGenerator*>(&carrier), nullptr);
    logger::info("SCOPY ANIM load index={} queued={} loaded={} loaded-records={}",
        load_index, queued, loaded, file_manager->loadedAnimations.size());

    LoadedWalk after_walk;
    std::vector<const RE::hkaAnimation*> after;
    walk_loaded_storage(*file_manager, after_walk, after);
    logger::info("SCOPY ANIM load after records={} current=0x{:x} current-crc=0x{:08x} records-walked={} with-loader={} with-resources={} resources={} anims={}",
        file_manager->loadedAnimations.size(), reinterpret_cast<std::uintptr_t>(file_manager->currentAnimation.unk00),
        file_manager->currentAnimation.fileInfo.crc32Filename,
        after_walk.records, after_walk.with_loader, after_walk.with_resources, after_walk.resources, after.size());
    if (after_walk.first_loader.empty() && !before_walk.first_loader.empty())
        logger::info("SCOPY ANIM load loader-gone-after-load");
    for (const RE::hkaAnimation* const animation : after)
    {
        const RawAnimationReading raw = read_raw_animation(animation);
        logger::info("SCOPY ANIM load anim at=0x{:x} type={} duration={:.2f}s tracks={} data-present={}",
            reinterpret_cast<std::uintptr_t>(animation), animation_type_name(raw.type), raw.duration, raw.tracks,
            animation_data_is_present(animation, raw.tracks));
    }
    logger::info("SCOPY ANIM load verdict={} anims-before={} anims-after={}",
        after.size() > before.size() ? "load-produced-animation" : "no-new-animation", before.size(), after.size());

    // The step OAR's own source takes and this round had missed: after the engine has loaded an index,
    // the binding for it is in the character's binding set - that is where OAR reads it from
    // (FakeClipGenerator.cpp:106-126) rather than from anything the loader keeps. Reading it here is
    // what turns a completed load into an animation, and the binding carries the track-to-bone table
    // the playing clip's own binding was missing.
    {
        RE::hkbAnimationBindingSet* const set = set_holding(sets, load_index);
        const void* element = set ? set->bindings[load_index] : nullptr;
        const RE::hkaAnimationBinding* binding = binding_at_offset_0x10(element);
        const BindingReading reading = read_binding(binding, bone_count, true);
        const RE::hkaAnimation* animation = binding ? binding->animation.get() : nullptr;
        const RawAnimationReading raw = read_raw_animation(animation);
        logger::info("SCOPY ANIM load bindingset idx={} set={} element=0x{:x} binding=0x{:x} anim=0x{:x} anim-type={} duration={:.2f}s tracks={} binding-valid={} reason={} data={}",
            load_index, set == sets.character ? "character" : set == sets.setup ? "setup" : "none",
            reinterpret_cast<std::uintptr_t>(element), reinterpret_cast<std::uintptr_t>(binding),
            reinterpret_cast<std::uintptr_t>(animation), animation_type_name(raw.type), raw.duration, raw.tracks,
            reading.valid, reading.reason,
            animation_data_state(animation, raw.tracks));

        if (animation && animation_data_is_present(animation, raw.tracks))
        {
            out_animation = animation;
            if (reading.valid)
                out_binding = binding;
        }
    }
}

ClipClass classify_clip_name(std::string_view file_name)
{
    if (is_base_idle(file_name))
        return ClipClass::e_idle;
    if (mentions_equip(file_name))
        return ClipClass::e_equip;
    return ClipClass::e_other;
}

bool clip_generator_is_live(void const* object)
{
    // One query for the common rejection first: almost every node offered to this check is not
    // a clip generator, and the exact vtable compare needs only the first word readable — it is
    // also strictly stronger than a "looks like an object" segment check. The full-struct
    // readability that the field reads rely on is paid only by the few survivors.
    if (!object || !readable(object, sizeof(void*)) ||
        *static_cast<const std::uintptr_t*>(object) != clip_generator_vtable())
        return false;
    return readable(object, sizeof(RE::hkbClipGenerator));
}

void tally_clip_generator(RE::hkbClipGenerator const* generator, ClipClass kind, ActiveAnimationIndices& out)
{
    if (!generator)
        return;
    // A live control means the engine is running this clip: that is the one OAR has chosen and activated,
    // so its index is OAR's answer for this character right now. The index is re-read every time because
    // OAR rewrites it when it picks a different replacement; the name is not, because it cannot change.
    if (!generator->animationControl.get())
        return;
    switch (kind)
    {
    case ClipClass::e_idle:
        out.idles.push_back(generator->animationBindingIndex);
        break;
    case ClipClass::e_equip:
        out.equips.push_back(generator->animationBindingIndex);
        break;
    default:
        ++out.others;
        break;
    }
}

// The clip generators the engine lists as active right now, read from the engine's own active-node
// record instead of from any scan of object memory. The engine appends a node when its Activate runs
// and removes it on Deactivate (HKX13 reached a live, playing clip exactly this way), so the list is
// the definition of "currently running" and an animation switch is just the next snapshot listing
// different nodes - there is no discovery step to pay for, and no window scan to trigger.
//
// This runs inside ordinary gameplay, so the HKX32/33 cost model governs it: a name is read exactly
// once per clip (it cannot change while the pointer is alive) and the identity check is one page
// query for the non-clip entries that make up almost the whole list. HKX34 paid a name read for every
// running clip on every sample - OAR's replacement paths are over a hundred characters - and that was
// the stutter the player felt on every action change, the exact cost "stop reading names every
// sample" had already removed once.
void collect_running_clip_generators(RE::TESObjectREFR& source, std::vector<ActiveClipEntry>& out)
{
    struct ClipIdentity
    {
        ClipClass kind;
        std::string name;
    };
    // Mutated on the render thread only (process_requests is the one caller). A clip that stops
    // running is swept below, so a session of switching cannot grow it without bound, and a freed
    // pointer that gets recycled simply re-pays one fresh name read.
    static std::unordered_map<const RE::hkbClipGenerator*, ClipIdentity> cache;

    RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
    if (!source.GetAnimationGraphManager(manager) || !manager)
        return;

    for (std::uint32_t i = 0; i < manager->graphs.size(); ++i)
    {
        RE::BShkbAnimationGraph* const graph = manager->graphs[i].get();
        if (!graph || !graph->holder || static_cast<RE::TESObjectREFR*>(graph->holder) != &source)
            continue;
        RE::hkbBehaviorGraph* const behavior = graph->characterInstance.behaviorGraph.get();
        if (!behavior || !behavior->activeNodes)
            continue;

        // The list is typed end to end (hkbNodeInfo::nodeClone), but it is the engine's live data:
        // entries are re-validated with the same one-guarded-read identity check every other route
        // uses, so a stale or misread entry is skipped rather than trusted.
        for (const RE::hkbNodeInfo& info : *behavior->activeNodes)
        {
            auto* const node = static_cast<const void*>(info.nodeClone);
            if (!clip_generator_is_live(node))
                continue;
            const auto* const generator = static_cast<const RE::hkbClipGenerator*>(node);
            auto known = cache.find(generator);
            if (known == cache.end())
            {
                const std::string name = guarded_string(generator->animationName);
                known = cache.emplace(generator, ClipIdentity{ classify_clip_name(base_name_of(name)), name }).first;
            }
            out.push_back({ generator, known->second.kind, known->second.name });
        }
    }

    if (!out.empty())
    {
        std::unordered_set<const RE::hkbClipGenerator*> running;
        running.reserve(out.size());
        for (const ActiveClipEntry& entry : out)
            running.insert(entry.generator);
        std::erase_if(cache, [&](const auto& entry) { return running.find(entry.first) == running.end(); });
    }
    else
    {
        cache.clear();
    }
}

size_t play_animation_clip(ClipPlayback const& clip, float seconds, RE::NiAVObject& copy_root, bool loop)
{
    if (!clip.valid())
        return 0;
    // The one place the clip is sampled every frame, so the same guard applies here: a clip that was
    // accepted at capture can have had its data unloaded by the time a later frame draws it.
    if (!animation_data_is_present(clip.animation, static_cast<std::int32_t>(clip.tracks.size())))
        return 0;

    const float length = clip.duration > 0.05f ? clip.duration : 0.05f;
    float looped = seconds;
    if (loop)
    {
        looped = std::fmod(seconds, length);
        if (looped < 0.0f)
            looped += length;
    }
    else if (looped > length)
        looped = length;  // single-play: the engine blends the final frame out; hold it, never replay

    const size_t sample_count = clip.tracks.empty() ? 0 : static_cast<size_t>(clip.tracks.back()) + 1;
    if (sample_count == 0)
        return 0;
    std::vector<RE::hkQsTransform> sampled(sample_count);
    sample_tracks(*clip.animation, looped, static_cast<std::uint32_t>(sample_count), sampled.data(), nullptr, 0);

    size_t written = 0;
    for (size_t i = 0; i < clip.tracks.size(); ++i)
    {
        RE::NiAVObject* const node = clip.nodes[i];
        if (!node || static_cast<size_t>(clip.tracks[i]) >= sampled.size())
            continue;
        RE::NiTransform local = ni_local_from_pose(read_pose(sampled[clip.tracks[i]]), false);
        if (!std::isfinite(local.scale) || local.scale <= 0.0f)
            local.scale = node->local.scale;
        node->local = local;
        ++written;
    }
    recompute_subtree_worlds(copy_root, 0);
    return written;
}

RE::hkbClipGenerator const* pick_running_clip(RE::TESObjectREFR& source)
{
    std::vector<ActiveClipEntry> running;
    collect_running_clip_generators(source, running);
    RE::hkbClipGenerator const* best = nullptr;
    float best_weight = 0.0f;
    for (const ActiveClipEntry& entry : running)
    {
        RE::hkbClipGenerator const* const generator = entry.generator;
        if (!generator || !readable(generator, sizeof(RE::hkbClipGenerator)) || !looks_like_object(generator))
            continue;
        const RE::hkaDefaultAnimationControl* const control = generator->animationControl.get();
        if (!control || !readable(control, sizeof(RE::hkaAnimationControl)) || !looks_like_object(control))
            continue;
        if (control->weight > best_weight)
        {
            best_weight = control->weight;
            best = generator;
        }
    }
    return best;
}

namespace
{
    // Solved mappings keyed by animation. Pointers are only compared while the same animation
    // object is live in the caller's hands, so a freed entry can never be dereferenced - a stale
    // key merely misses and the clip solves again. Lives above the resolver: the resolver consumes
    // it (tier 3) and the solver below writes it.
    constexpr size_t Solved_Mapping_Cap = 48;

    std::unordered_map<const RE::hkaAnimation*, std::vector<std::uint16_t>>& solved_mappings()
    {
        static std::unordered_map<const RE::hkaAnimation*, std::vector<std::uint16_t>> s_cache;
        if (s_cache.size() > Solved_Mapping_Cap)
            s_cache.clear();
        return s_cache;
    }
}

bool resolve_clip_playback(RE::TESObjectREFR& source, RE::NiAVObject& copy_root, RE::hkbClipGenerator const* generator,
    ClipPlayback& out, std::string& out_mapping, std::string& out_coverage)
{
    if (!generator || !readable(generator, sizeof(RE::hkbClipGenerator)) || !looks_like_object(generator))
    {
        logger::info("SCOPY PUPPET resolve UNAVAILABLE reason=generator-lost");
        return false;
    }

    const char* const raw_name = generator->animationName.c_str();
    const std::string clip_name = raw_name ? raw_name : "";
    const RE::hkaDefaultAnimationControl* const control = generator->animationControl.get();
    if (!control || !readable(control, sizeof(RE::hkaAnimationControl)) || !looks_like_object(control))
    {
        logger::info("SCOPY PUPPET resolve UNAVAILABLE reason=no-live-control clip='{}'", clip_name);
        return false;
    }

    const RE::hkaAnimationBinding* const binding = generator->binding;
    if (!binding || !readable(binding, sizeof(RE::hkaAnimationBinding)) || !looks_like_object(binding))
    {
        logger::info("SCOPY PUPPET resolve UNAVAILABLE reason=no-live-binding clip='{}'", clip_name);
        return false;
    }
    RE::hkaAnimation* const animation = binding->animation.get();
    if (!animation || !readable(animation, sizeof(RE::hkaAnimation)) || !looks_like_object(animation))
    {
        logger::info("SCOPY PUPPET resolve UNAVAILABLE reason=no-animation clip='{}'", clip_name);
        return false;
    }
    const std::int32_t track_count = animation->numberOfTransformTracks;
    if (track_count <= 0 || track_count > Max_Tracks_Read || !animation_data_is_present(animation, track_count))
    {
        logger::info("SCOPY PUPPET resolve UNAVAILABLE reason=animation-data clip='{}' tracks={}", clip_name, track_count);
        return false;
    }

    // The animation skeleton names the bones the tracks are indexed against; the graph that holds
    // this character is the third-person one (holder matches). Its per-bone state also gives the
    // coverage note the log uses to watch the pose bridge.
    RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
    const RE::hkaSkeleton* skeleton = nullptr;
    std::uint32_t bone_nodes = 0;
    std::uint32_t pose_count = 0;
    if (source.GetAnimationGraphManager(manager) && manager)
    {
        for (const RE::BSTSmartPointer<RE::BShkbAnimationGraph>& holder : manager->graphs)
        {
            RE::BShkbAnimationGraph* const graph = holder.get();
            if (!graph || !graph->holder || !graph->rootNode)
                continue;
            if (static_cast<const RE::TESObjectREFR*>(graph->holder) != &source)
                continue;
            skeleton = graph->characterInstance.setup.get()
                ? graph->characterInstance.setup->animationSkeleton.get()
                : nullptr;
            bone_nodes = static_cast<std::uint32_t>(graph->boneNodes.size());
            pose_count = static_cast<std::uint32_t>(std::max(graph->characterInstance.numPoseLocal, 0));
            break;
        }
    }
    if (!skeleton || skeleton->bones.empty())
    {
        logger::info("SCOPY PUPPET resolve UNAVAILABLE reason=no-animation-skeleton clip='{}'", clip_name);
        return false;
    }

    // Tier 1: the engine-filled table is the authority. Tier 2: the known-empty OAR shape falls
    // back to identity over the skeleton; marked unverified and left to the occasional A/B.
    const RE::hkArray<std::int16_t>& table = binding->transformTrackToBoneIndices;
    std::vector<std::uint16_t> tracks;
    if (!table.empty() && readable(table.begin(), static_cast<std::size_t>(table.size()) * sizeof(std::int16_t)))
    {
        const std::int32_t count = std::min<std::int32_t>(track_count, static_cast<std::int32_t>(table.size()));
        tracks.reserve(static_cast<std::size_t>(count));
        for (std::int32_t i = 0; i < count; ++i)
        {
            const std::int16_t bone = table[static_cast<std::size_t>(i)];
            if (bone < 0 || static_cast<std::uint32_t>(bone) >= static_cast<std::uint32_t>(skeleton->bones.size()))
                continue;
            tracks.push_back(static_cast<std::uint16_t>(bone));
        }
        out_mapping = "table";
    }
    else
    {
        const std::int32_t count = std::min<std::int32_t>(track_count, static_cast<std::int32_t>(skeleton->bones.size()));
        tracks.reserve(static_cast<std::size_t>(count));
        for (std::int32_t i = 0; i < count; ++i)
            tracks.push_back(static_cast<std::uint16_t>(i));
        out_mapping = "identity-unverified";
    }

    // Copy nodes by bone name, one walk per clip switch (switches are seconds apart). The
    // skeleton's own root never drives: the studio owns the copy's root placement.
    const NodeTable copy_nodes = collect_nodes(copy_root);
    const NameIndex copy_by_name = index_by_name(copy_nodes);
    out = ClipPlayback{};
    out.animation = animation;
    out.name = clip_name;
    out.duration = animation->duration;
    out.tracks = tracks;
    out.nodes.assign(tracks.size(), nullptr);
    size_t resolved = 0;
    for (size_t i = 0; i < tracks.size(); ++i)
    {
        const char* const bone_name = skeleton->bones[tracks[i]].name.c_str();
        if (!bone_name)
            continue;
        const auto found = copy_by_name.find(bone_name);
        if (found == copy_by_name.end() || found->second == &copy_root)
            continue;
        out.nodes[i] = found->second;
        ++resolved;
    }
    if (resolved == 0)
    {
        logger::info("SCOPY PUPPET resolve UNAVAILABLE reason=no-copy-nodes clip='{}' tracks={}", clip_name, tracks.size());
        return false;
    }
    // Tier 3: the empirical solve cached for this animation - the measured order beats the
    // empty-table identity, and every later panel session picks it up without waiting.
    if (out_mapping == "identity-unverified")
    {
        const auto cached = solved_mappings().find(animation);
        if (cached != solved_mappings().end() && cached->second.size() == out.tracks.size())
        {
            out.tracks = cached->second;
            resolved = 0;
            for (size_t i = 0; i < out.tracks.size(); ++i)
            {
                if (static_cast<std::size_t>(out.tracks[i]) >= static_cast<std::size_t>(skeleton->bones.size()))
                    continue;
                const char* const bone_name = skeleton->bones[static_cast<std::int32_t>(out.tracks[i])].name.c_str();
                if (!bone_name)
                    continue;
                const auto mapped = copy_by_name.find(bone_name);
                if (mapped == copy_by_name.end() || mapped->second == &copy_root)
                    continue;
                out.nodes[i] = mapped->second;
                ++resolved;
            }
            if (resolved > 0)
                out_mapping = "solved-cached";
        }
    }
    out_coverage = fmt::format("tracks={} resolved={} bone-nodes={} pose={}", tracks.size(), resolved, bone_nodes, pose_count);
    return true;
}

float live_clip_time(RE::hkbClipGenerator const* generator)
{
    if (!generator || !readable(generator, sizeof(RE::hkbClipGenerator)) || !looks_like_object(generator))
        return -1.0f;
    const RE::hkaDefaultAnimationControl* const control = generator->animationControl.get();
    if (!control || !readable(control, sizeof(RE::hkaAnimationControl)) || !looks_like_object(control))
        return -1.0f;
    const float time = control->localTime;
    return std::isfinite(time) ? time : -1.0f;
}

namespace
{
    float quat_angle_degrees(float const* first, float const* second)
    {
        const float dot = std::abs(first[0] * second[0] + first[1] * second[1] + first[2] * second[2] + first[3] * second[3]);
        return 2.0f * std::acos(std::clamp(dot, 0.0f, 1.0f)) * 57.2957795131f;
    }
}

std::string measure_clip_against_puppet(RE::TESObjectREFR& source, RE::hkbClipGenerator const* generator,
    ClipPlayback const& playback, std::string const& mapping)
{
    if (!generator || !readable(generator, sizeof(RE::hkbClipGenerator)) || !looks_like_object(generator))
        return "SCOPY PUPPET ab UNAVAILABLE reason=generator-lost";
    const RE::hkaDefaultAnimationControl* const control = generator->animationControl.get();
    if (!control || !readable(control, sizeof(RE::hkaAnimationControl)) || !looks_like_object(control))
        return "SCOPY PUPPET ab UNAVAILABLE reason=no-live-control";
    if (!playback.valid() || !animation_data_is_present(playback.animation, static_cast<std::int32_t>(playback.tracks.size())))
        return "SCOPY PUPPET ab UNAVAILABLE reason=playback-invalid";

    // The puppet's own per-bone state, indexed by bone name: poseLocal follows boneNodes order
    // (the HKX2 protocol), and the names come from the graph's own bone table.
    RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
    const RE::hkaSkeleton* skeleton = nullptr;
    const RE::hkQsTransform* pose = nullptr;
    std::int32_t pose_count = 0;
    std::unordered_map<std::string_view, const RE::hkQsTransform*> pose_by_name;
    if (source.GetAnimationGraphManager(manager) && manager)
    {
        for (const RE::BSTSmartPointer<RE::BShkbAnimationGraph>& holder : manager->graphs)
        {
            RE::BShkbAnimationGraph* const graph = holder.get();
            if (!graph || !graph->holder || !graph->rootNode)
                continue;
            if (static_cast<const RE::TESObjectREFR*>(graph->holder) != &source)
                continue;
            skeleton = graph->characterInstance.setup.get()
                ? graph->characterInstance.setup->animationSkeleton.get()
                : nullptr;
            pose = graph->characterInstance.poseLocal;
            pose_count = graph->characterInstance.numPoseLocal;
            if (pose && pose_count > 0 &&
                readable(pose, static_cast<std::size_t>(pose_count) * sizeof(RE::hkQsTransform)))
            {
                const std::int32_t named = std::min<std::int32_t>(pose_count, static_cast<std::int32_t>(graph->boneNodes.size()));
                pose_by_name.reserve(static_cast<std::size_t>(named));
                for (std::int32_t i = 0; i < named; ++i)
                {
                    RE::NiNode* const node = graph->boneNodes[static_cast<std::uint32_t>(i)].node;
                    const char* const name = node ? node->name.c_str() : nullptr;
                    if (name)
                        pose_by_name.emplace(name, pose + i);
                }
            }
            break;
        }
    }
    if (!skeleton || skeleton->bones.empty() || pose_by_name.empty())
        return "SCOPY PUPPET ab UNAVAILABLE reason=no-pose-state";

    // Sample the clip once, at the phase the graph is playing right now.
    const std::uint32_t sample_count = playback.tracks.empty() ? 0 : static_cast<std::uint32_t>(playback.tracks.back()) + 1;
    if (sample_count == 0)
        return "SCOPY PUPPET ab UNAVAILABLE reason=no-tracks";
    std::vector<RE::hkQsTransform> sampled(sample_count);
    sample_tracks(*playback.animation, control->localTime, sample_count, sampled.data(), nullptr, 0);

    // Compare bone by bone: the clip's track for a skeleton bone versus the graph's pose for the
    // bone of the same name. Bones the pose state does not name are counted out, not failed. The
    // named/calm pair reads the result: `named` is how many bones the name bridge reaches at all
    // (the vanilla-named core inside a CBBE/UBE skeleton - HKX1 measured the same 37/116), and
    // `calm` is how many of the compared bones sit inside the 1-degree bar, so a single wild bone
    // no longer hides a mostly-correct mapping.
    std::uint32_t compared = 0;
    std::uint32_t unnamed = 0;
    std::uint32_t calm = 0;
    float worst_rotation = 0.0f;
    float worst_position = 0.0f;
    for (size_t i = 0; i < playback.tracks.size(); ++i)
    {
        if (static_cast<std::uint32_t>(playback.tracks[i]) >= static_cast<std::uint32_t>(skeleton->bones.size()))
            continue;
        const char* const bone_name = skeleton->bones[playback.tracks[i]].name.c_str();
        const auto found = bone_name ? pose_by_name.find(bone_name) : pose_by_name.end();
        if (found == pose_by_name.end())
        {
            ++unnamed;
            continue;
        }
        const PoseSample clip_pose = read_pose(sampled[playback.tracks[i]]);
        const PoseSample graph_pose = read_pose(*found->second);
        const float rotation = quat_angle_degrees(clip_pose.quaternion, graph_pose.quaternion);
        worst_rotation = std::max(worst_rotation, rotation);
        calm += rotation <= 1.0f ? 1u : 0u;
        const float dx = clip_pose.position[0] - graph_pose.position[0];
        const float dy = clip_pose.position[1] - graph_pose.position[1];
        const float dz = clip_pose.position[2] - graph_pose.position[2];
        worst_position = std::max(worst_position, std::sqrt(dx * dx + dy * dy + dz * dz));
        ++compared;
    }
    const bool match = compared >= 24 && worst_rotation <= 1.0f;
    return fmt::format("SCOPY PUPPET ab clip='{}' mapping={} bones={}/{} unnamed={} named={} calm={} max-rot={:.2f}deg max-pos={:.3f}u weight={:.2f} t={:.2f}s verdict={}",
        playback.name, mapping, compared, playback.tracks.size(), unnamed, pose_by_name.size(), calm, worst_rotation, worst_position,
        control->weight, control->localTime, match ? "match" : "none");
}

namespace
{
    // The solver: one observation per paused tick, ~1.5 seconds of manual ticks to a verdict.
    // The solver: observations accumulate while the ENGINE ticks the puppet - its node locals only
    // move then (the HKX42 log proved the paused manual tick never reaches them) - and the panel
    // consumes the finished mapping. One observation per tick of the owning phase, ~1.5 seconds of
    // engine time to a verdict. Solved orders are cached per animation, so every later panel
    // session picks the mapping up without waiting.
    constexpr float Frozen_Truth_Degrees = 0.05f;

    struct TrackSolveState
    {
        RE::hkbClipGenerator const* generator = nullptr;
        bool built = false;                              // structural tables survive truth retries
        bool node_path = false;                          // current truth: node locals vs pose locals
        bool paused_phase = false;                       // a phase change invalidates the accumulation
        std::uint32_t frames = 0;
        std::vector<bool> reachable;                     // per skeleton bone: a truth source exists
        std::vector<RE::NiNode*> node_candidates;        // node-locals path, per skeleton bone
        std::vector<const RE::hkQsTransform*> pose_candidates;  // pose path, per skeleton bone
        std::vector<RE::NiMatrix3> previous_truth;       // last frame's truth (frozen check)
        float truth_motion = 0.0f;
        std::vector<float> error;                        // accumulated: track x skeleton bone
    };

    TrackSolveState& solver_state()
    {
        static TrackSolveState s_state;
        return s_state;
    }

    // Truth rotations indexed by skeleton bone; unreachable bones carry identity and are excluded
    // through `reachable` everywhere.
    std::vector<RE::NiMatrix3> read_truth_rotations(const TrackSolveState& state)
    {
        std::vector<RE::NiMatrix3> rotations(state.reachable.size());
        for (size_t i = 0; i < state.reachable.size(); ++i)
        {
            if (!state.reachable[i])
                continue;
            if (state.node_path)
            {
                if (RE::NiNode* const node = state.node_candidates[i])
                    rotations[i] = node->local.rotate;
            }
            else if (const RE::hkQsTransform* const pose = state.pose_candidates[i])
            {
                const PoseSample sample = read_pose(*pose);
                rotations[i] = ni_matrix_from_quaternion(sample.quaternion[0], sample.quaternion[1], sample.quaternion[2], sample.quaternion[3]);
            }
        }
        return rotations;
    }

    // Re-entrant truth selection with an explicit path: the structural tables survive retries,
    // only the pointer and the reachability flip. The HKX42 round tripped over an inferred
    // selection - frames==0 rebuilt the tables, forced node-locals back on, and the pose fallback
    // never ran.
    void select_truth(TrackSolveState& state, bool node_path)
    {
        state.node_path = node_path;
        state.reachable.assign(state.reachable.size(), false);
        if (node_path)
            for (size_t i = 0; i < state.node_candidates.size(); ++i)
                state.reachable[i] = state.node_candidates[i] != nullptr;
        else
            for (size_t i = 0; i < state.pose_candidates.size(); ++i)
                state.reachable[i] = state.pose_candidates[i] != nullptr;
        state.previous_truth = read_truth_rotations(state);
    }
}

void reset_track_solver()
{
    solver_state() = TrackSolveState{};
}

bool solve_tracks_step(RE::TESObjectREFR& source, RE::NiAVObject& copy_root,
    RE::hkbClipGenerator const* generator, bool paused_phase, ClipPlayback& in_out_playback, std::string& out_note)
{
    TrackSolveState& state = solver_state();
    if (state.generator != generator || state.paused_phase != paused_phase)
    {
        // A new clip or a phase change invalidates the accumulation - the two truth spaces do not
        // mix. The structural tables are rebuilt with the state.
        state = TrackSolveState{};
        state.generator = generator;
        state.paused_phase = paused_phase;
    }

    if (!generator || !readable(generator, sizeof(RE::hkbClipGenerator)) || !looks_like_object(generator))
    {
        out_note = "SCOPY PUPPET solve UNAVAILABLE reason=generator-lost";
        reset_track_solver();
        return false;
    }
    if (!in_out_playback.valid() || !animation_data_is_present(in_out_playback.animation, static_cast<std::int32_t>(in_out_playback.tracks.size())))
    {
        out_note = "SCOPY PUPPET solve UNAVAILABLE reason=playback-invalid";
        reset_track_solver();
        return false;
    }

    // Structural build, once per clip. Truth selection is the re-entrant part and runs after it.
    if (!state.built)
    {
        RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
        const RE::hkaSkeleton* skeleton = nullptr;
        RE::hkbCharacter const* character = nullptr;
        RE::BShkbAnimationGraph* third_person = nullptr;
        if (source.GetAnimationGraphManager(manager) && manager)
        {
            for (const RE::BSTSmartPointer<RE::BShkbAnimationGraph>& holder : manager->graphs)
            {
                RE::BShkbAnimationGraph* const graph = holder.get();
                if (!graph || !graph->holder || !graph->rootNode)
                    continue;
                if (static_cast<const RE::TESObjectREFR*>(graph->holder) != &source)
                    continue;
                third_person = graph;
                skeleton = graph->characterInstance.setup.get()
                    ? graph->characterInstance.setup->animationSkeleton.get()
                    : nullptr;
                character = &graph->characterInstance;
                break;
            }
        }
        if (!skeleton || skeleton->bones.empty() || !third_person)
        {
            out_note = "SCOPY PUPPET solve UNAVAILABLE reason=no-skeleton";
            reset_track_solver();
            return false;
        }
        state.node_candidates.assign(static_cast<std::size_t>(skeleton->bones.size()), nullptr);
        std::unordered_map<std::string_view, RE::NiAVObject*> puppet_nodes;
        if (RE::NiAVObject* const puppet_root = source.Get3D(false))
        {
            std::vector<RE::NiAVObject*> pending{ puppet_root };
            while (!pending.empty() && puppet_nodes.size() < 512)
            {
                RE::NiAVObject* const object = pending.back();
                pending.pop_back();
                if (const char* const name = object->name.c_str())
                    puppet_nodes.emplace(name, object);
                if (RE::NiNode* const node = object->AsNode())
                    for (const RE::NiPointer<RE::NiAVObject>& child : node->children)
                        if (child)
                            pending.push_back(child.get());
            }
            for (size_t i = 0; i < static_cast<std::size_t>(skeleton->bones.size()); ++i)
            {
                const char* const bone_name = skeleton->bones[static_cast<std::int32_t>(i)].name.c_str();
                const auto found = bone_name ? puppet_nodes.find(bone_name) : puppet_nodes.end();
                state.node_candidates[i] = found != puppet_nodes.end() ? found->second->AsNode() : nullptr;
            }
        }
        state.pose_candidates.assign(static_cast<std::size_t>(skeleton->bones.size()), nullptr);
        if (character && character->poseLocal && character->numPoseLocal > 0 &&
            readable(character->poseLocal, static_cast<std::size_t>(character->numPoseLocal) * sizeof(RE::hkQsTransform)))
        {
            const std::int32_t named = std::min<std::int32_t>(character->numPoseLocal, static_cast<std::int32_t>(third_person->boneNodes.size()));
            std::unordered_map<std::string_view, const RE::hkQsTransform*> pose_by_name;
            for (std::int32_t i = 0; i < named; ++i)
            {
                RE::NiNode* const node = third_person->boneNodes[static_cast<std::uint32_t>(i)].node;
                const char* const name = node ? node->name.c_str() : nullptr;
                if (name)
                    pose_by_name.emplace(name, character->poseLocal + i);
            }
            for (size_t i = 0; i < static_cast<std::size_t>(skeleton->bones.size()); ++i)
            {
                const char* const bone_name = skeleton->bones[static_cast<std::int32_t>(i)].name.c_str();
                const auto found = bone_name ? pose_by_name.find(bone_name) : pose_by_name.end();
                state.pose_candidates[i] = found != pose_by_name.end() ? found->second : nullptr;
            }
        }
        state.built = true;
        select_truth(state, !paused_phase);
        state.error.assign(in_out_playback.tracks.size() * state.reachable.size(), 0.0f);
        if (std::none_of(state.reachable.begin(), state.reachable.end(), [](const bool flag) { return flag; }))
        {
            select_truth(state, false);  // the node table came out empty; pose locals or nothing
            state.error.assign(in_out_playback.tracks.size() * state.reachable.size(), 0.0f);
            if (std::none_of(state.reachable.begin(), state.reachable.end(), [](const bool flag) { return flag; }))
            {
                out_note = "SCOPY PUPPET solve UNAVAILABLE reason=no-truth-candidates";
                reset_track_solver();
                return false;
            }
        }
    }

    const RE::hkaDefaultAnimationControl* const control = generator->animationControl.get();
    if (!control || !readable(control, sizeof(RE::hkaAnimationControl)) || !looks_like_object(control))
    {
        out_note = "SCOPY PUPPET solve UNAVAILABLE reason=no-live-control";
        reset_track_solver();
        return false;
    }
    const float time = control->localTime;
    if (!std::isfinite(time) || time < 0.0f)
    {
        out_note = "SCOPY PUPPET solve UNAVAILABLE reason=no-live-time";
        reset_track_solver();
        return false;
    }
    const size_t track_count = in_out_playback.tracks.size();
    const std::uint32_t sample_count = track_count == 0 ? 0 : static_cast<std::uint32_t>(in_out_playback.tracks.back()) + 1;
    if (sample_count == 0)
    {
        out_note = "SCOPY PUPPET solve UNAVAILABLE reason=no-tracks";
        reset_track_solver();
        return false;
    }
    std::vector<RE::hkQsTransform> sampled(sample_count);
    sample_tracks(*in_out_playback.animation, time, sample_count, sampled.data(), nullptr, 0);

    const std::vector<RE::NiMatrix3> truth = read_truth_rotations(state);
    const size_t bones = state.reachable.size();
    for (size_t i = 0; i < track_count; ++i)
    {
        if (static_cast<std::uint32_t>(in_out_playback.tracks[i]) >= sample_count)
            continue;
        const PoseSample track_pose = read_pose(sampled[in_out_playback.tracks[i]]);
        const RE::NiMatrix3 track_rotation = ni_matrix_from_quaternion(
            track_pose.quaternion[0], track_pose.quaternion[1], track_pose.quaternion[2], track_pose.quaternion[3]);
        for (size_t j = 0; j < bones; ++j)
        {
            if (!state.reachable[j])
                continue;
            state.error[i * bones + j] += rotation_angle_degrees(track_rotation, truth[j]);
        }
    }

    float frame_motion = 0.0f;
    for (size_t j = 0; j < std::min(truth.size(), state.previous_truth.size()); ++j)
        if (state.reachable[j])
            frame_motion = std::max(frame_motion, rotation_angle_degrees(truth[j], state.previous_truth[j]));
    state.truth_motion = std::max(state.truth_motion, frame_motion);
    state.previous_truth = truth;
    ++state.frames;

    if (state.frames < Track_Solve_Frames)
    {
        out_note = fmt::format("SCOPY PUPPET solve frames={}/{} truth={} motion={:.3f}deg",
            state.frames, Track_Solve_Frames, state.node_path ? "node-locals" : "pose-local", state.truth_motion);
        return false;
    }

    // A frozen truth convicts the source, never the mapping: fall back to the other path, or give
    // up with the playback untouched. No frozen observation ever writes a mapping.
    if (state.truth_motion < Frozen_Truth_Degrees)
    {
        if (state.node_path)
        {
            out_note = fmt::format("SCOPY PUPPET solve retry truth=node-locals-frozen motion={:.3f}deg (falling back to pose-local)", state.truth_motion);
            select_truth(state, false);
            std::fill(state.error.begin(), state.error.end(), 0.0f);
            state.frames = 0;
            state.truth_motion = 0.0f;
            return false;
        }
        out_note = fmt::format("SCOPY PUPPET solve UNAVAILABLE reason=truth-frozen motion={:.3f}deg (both sources)", state.truth_motion);
        reset_track_solver();
        return false;
    }

    // Verdict: per track the least-error reachable bone wins; greedy claims keep two tracks from
    // driving one bone. identity-agrees counts the tracks the engine maps the way the old
    // assumption did - the number that quantifies OAR's track reordering.
    std::vector<std::uint8_t> taken(bones, 0);
    std::uint32_t solved = 0;
    std::uint32_t identity_agrees = 0;
    std::vector<std::uint16_t> solved_tracks(track_count, 0);
    for (size_t i = 0; i < track_count; ++i)
    {
        size_t best = bones;
        float best_error = std::numeric_limits<float>::max();
        for (size_t j = 0; j < bones; ++j)
        {
            if (taken[j] || !state.reachable[j])
                continue;
            const float error = state.error[i * bones + j];
            if (error < best_error)
            {
                best_error = error;
                best = j;
            }
        }
        if (best == bones)
            continue;
        taken[best] = 1;
        solved_tracks[i] = static_cast<std::uint16_t>(best);
        ++solved;
        if (best == in_out_playback.tracks[i])
            ++identity_agrees;
    }

    RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
    const RE::hkaSkeleton* skeleton = nullptr;
    if (source.GetAnimationGraphManager(manager) && manager)
    {
        for (const RE::BSTSmartPointer<RE::BShkbAnimationGraph>& holder : manager->graphs)
        {
            RE::BShkbAnimationGraph* const graph = holder.get();
            if (!graph || !graph->holder || static_cast<const RE::TESObjectREFR*>(graph->holder) != &source)
                continue;
            skeleton = graph->characterInstance.setup.get()
                ? graph->characterInstance.setup->animationSkeleton.get()
                : nullptr;
            break;
        }
    }
    if (!skeleton)
    {
        out_note = "SCOPY PUPPET solve UNAVAILABLE reason=skeleton-lost-at-write";
        reset_track_solver();
        return false;
    }
    const NodeTable copy_nodes = collect_nodes(copy_root);
    const NameIndex copy_by_name = index_by_name(copy_nodes);
    in_out_playback.tracks = solved_tracks;
    in_out_playback.nodes.assign(track_count, nullptr);
    size_t resolved = 0;
    for (size_t i = 0; i < track_count; ++i)
    {
        if (static_cast<std::size_t>(solved_tracks[i]) >= static_cast<std::size_t>(skeleton->bones.size()))
            continue;
        const char* const bone_name = skeleton->bones[static_cast<std::int32_t>(solved_tracks[i])].name.c_str();
        if (!bone_name)
            continue;
        const auto found = copy_by_name.find(bone_name);
        if (found == copy_by_name.end() || found->second == &copy_root)
            continue;
        in_out_playback.nodes[i] = found->second;
        ++resolved;
    }
    if (resolved == 0)
    {
        out_note = "SCOPY PUPPET solve UNAVAILABLE reason=no-copy-nodes-after-solve";
        reset_track_solver();
        return false;
    }
    solved_mappings()[in_out_playback.animation] = solved_tracks;
    out_note = fmt::format("SCOPY PUPPET solve clip='{}' tracks={} solved={} identity-agrees={} truth={} motion={:.3f}deg",
        in_out_playback.name, track_count, solved, identity_agrees, state.node_path ? "node-locals" : "pose-local", state.truth_motion);
    reset_track_solver();
    return true;
}

void ClipPlayer::reset()
{
    m_clip = ClipPlayback{};
    m_time = 0.0;
    m_last_tick = std::chrono::steady_clock::now();
}

void ClipPlayer::set_clip(ClipPlayback clip)
{
    m_clip = std::move(clip);
    m_time = 0.0;
    m_last_tick = std::chrono::steady_clock::now();
}

void ClipPlayer::restart()
{
    m_time = 0.0;
    m_last_tick = std::chrono::steady_clock::now();
}

size_t ClipPlayer::advance(RE::NiAVObject& copy_root)
{
    if (!m_clip.valid())
        return 0;

    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    const double step = std::min(std::chrono::duration<double>(now - m_last_tick).count(), Max_Clip_Step_Seconds);
    m_last_tick = now;
    m_time += std::max(step, 0.0);
    return play_animation_clip(m_clip, static_cast<float>(m_time), copy_root);
}

PLUGIN_NAMESPACE_END
