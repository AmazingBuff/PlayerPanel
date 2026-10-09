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
#include "RE/H/hkbClipGenerator.h"
#include "RE/H/hkbGenerator.h"
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
    // is sampled.
    constexpr int Max_Clip_Depth = 8;
    constexpr size_t Max_Clip_Objects = 512;
    constexpr std::int32_t Max_States = 64;
    constexpr std::int32_t Container_Scan_Bytes = 0x200;
    constexpr size_t Sweep_Steps = 24;
    constexpr size_t Max_Clips_Reported = 6;
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

    // The validation every candidate layout is judged by, and it needs no virtual call: the animation
    // pointer has to look like a live object, its type has to be one the engine names, its clip length
    // has to be plausible, and its own transform-track count has to equal the binding's track table -
    // which is the check a wrapper object cannot pass, because its animation field is not there.
    BindingReading read_binding(RE::hkaAnimationBinding const* binding, std::int32_t bone_count)
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
            return { false, "tracks-unreadable", duration, 0, 0, type };
        if (animation->numberOfTransformTracks != track_count)
            return { false, "track-count-mismatch", duration, static_cast<size_t>(track_count), 0, type };

        const size_t tracks = static_cast<size_t>(track_count);
        if (tracks > static_cast<size_t>(bone_count) || !track_indices_are_valid({ indices, tracks }, bone_count))
            return { false, "track-bones", duration, tracks, 0, type };
        return { true, "valid", duration, tracks, tracks, type };
    }

    // A string we are willing to print: read one byte at a time so an unterminated or non-textual
    // pointer cannot run away, and bounded either way.
    std::string guarded_string(RE::hkStringPtr const& value)
    {
        const char* const text = value.c_str();
        if (!text || !readable(text, 1))
            return "-";
        std::string out;
        for (size_t i = 0; i < Max_String_Read; ++i)
        {
            if (!readable(text + i, 1))
                return "-";
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

    bool equals_ignore_case(std::string_view lhs, std::string_view rhs)
    {
        if (lhs.size() != rhs.size())
            return false;
        for (size_t i = 0; i < lhs.size(); ++i)
        {
            if (std::tolower(static_cast<unsigned char>(lhs[i])) != std::tolower(static_cast<unsigned char>(rhs[i])))
                return false;
        }
        return true;
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

    // The engine names every object's class, which is what tells a clip generator from the rest of the
    // graph. It is a virtual call, so it is only made on an object whose first word already looks like a
    // vtable of the game's own image.
    const char* class_name_of(void const* object)
    {
        if (!looks_like_object(object))
            return nullptr;
        const auto* const referenced = static_cast<const RE::hkReferencedObject*>(object);
        const RE::hkClass* const type = referenced->GetClassType();
        return type && type->name ? type->name : nullptr;
    }

    ClipReading read_clip_generator(void const* candidate, std::int32_t bone_count, bool typed)
    {
        if (!candidate || !readable(candidate, sizeof(RE::hkbClipGenerator)) || !looks_like_object(candidate))
            return { false, "not-object", {}, nullptr };
        const auto* const generator = static_cast<const RE::hkbClipGenerator*>(candidate);
        const std::string name = guarded_string(generator->animationName);

        // The class name is the typed answer to "is this a clip generator", but asking for it is a
        // virtual call, so it is only asked of an object the graph reached through a typed edge. A
        // shape-discovered pointer is judged by its data alone: a word that looks like a vtable is not
        // proof that the object behind it implements the slot this call would jump through.
        if (typed)
        {
            const char* const class_name = class_name_of(candidate);
            if (class_name && std::string_view(class_name) != "hkbClipGenerator")
                return { false, "other-class", {}, nullptr };
        }
        if (!mentions_hkx(name))
            return { false, "no-animation-name", {}, nullptr };

        const BindingReading binding = read_binding(generator->binding, bone_count);
        if (!binding.valid)
            return { false, binding.reason, {}, nullptr };
        if (static_cast<std::uint8_t>(generator->mode.get()) > 3)
            return { false, "playback-mode", {}, nullptr };
        const float speed = generator->playbackSpeed;
        if (!std::isfinite(speed) || speed <= 0.0f || speed > 10.0f)
            return { false, "playback-speed", {}, nullptr };
        return { true, "valid", name, generator->binding };
    }

    struct ClipSearch
    {
        struct Step
        {
            void const* object;
            int depth;
            bool typed;
        };

        std::vector<Step> pending;
        std::unordered_set<void const*> seen;
        std::vector<ClipReading> clips;
        std::unordered_map<std::string, size_t> classes;
        std::unordered_map<std::string, size_t> reasons;
        size_t visited = 0;
        bool capped = false;
    };

    // The graph is a tree of only partly typed nodes, so the search follows only what is worth
    // following - a live object's first word is a vtable, a child pointer is readable - inside a bounded
    // depth and object count. A state machine's states are typed and followed by name; everything else
    // by the shape of its pointers. A node is believed only after validating it as a clip generator, so
    // a wrong step costs a rejection reason rather than a wrong clip.
    void scan_for_clips(void const* root, std::int32_t bone_count, ClipSearch& search)
    {
        search.pending.push_back({ root, 0, true });
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

            if (step.typed)
            {
                if (const char* const class_name = class_name_of(step.object))
                    ++search.classes[class_name];
            }

            const ClipReading clip = read_clip_generator(step.object, bone_count, step.typed);
            if (clip.valid)
            {
                search.clips.push_back(clip);
                continue;
            }
            ++search.reasons[clip.reason];
            if (step.depth == Max_Clip_Depth)
                continue;

            if (readable(step.object, sizeof(RE::hkbStateMachine)))
            {
                const auto* const machine = static_cast<const RE::hkbStateMachine*>(step.object);
                const std::int32_t states = machine->states.size();
                if (states > 0 && states <= Max_States && readable(machine->states.data(), sizeof(void*) * static_cast<size_t>(states)))
                {
                    for (std::int32_t i = 0; i < states; ++i)
                    {
                        if (void const* const state = machine->states[i])
                            search.pending.push_back({ state, step.depth + 1, true });
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
                    search.pending.push_back({ word, step.depth + 1, false });
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

bool prepare_animation_clip(RE::TESObjectREFR& source, RE::NiAVObject& source_root, RE::NiAVObject& copy_root, ClipPlayback& out)
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
    logger::info("SCOPY ANIM play search objects={} clips={} capped={} first='{}'", search.visited, search.clips.size(), search.capped, clip_list.empty() ? "-" : clip_list);
    logger::info("SCOPY ANIM play classes='{}'", summarise(search.classes));
    logger::info("SCOPY ANIM play rejections='{}'", summarise(search.reasons));
    if (search.clips.empty())
    {
        logger::info("SCOPY ANIM play UNAVAILABLE reason=no-clip-generator");
        return false;
    }

    const ClipReading* chosen = &search.clips.front();
    for (const ClipReading& clip : search.clips)
    {
        if (clip_preference(clip.name) < clip_preference(chosen->name))
            chosen = &clip;
    }

    const RE::hkaAnimationBinding* const binding = chosen->binding;
    RE::hkaAnimation* const animation = binding->animation.get();
    const std::int16_t* const track_to_bone = binding->transformTrackToBoneIndices.data();
    const std::int32_t track_count = binding->transformTrackToBoneIndices.size();
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
    playback.animation = animation;
    playback.name = chosen->name;
    playback.duration = animation->duration;
    std::vector<const RE::hkQsTransform*> truth;
    for (std::int32_t track = 0; track < track_count; ++track)
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

    logger::info("SCOPY ANIM play clip='{}' type={} duration={:.2f}s tracks={} copy-resolved={}/{} truth-bones={}",
        playback.name, animation_type_name(static_cast<std::uint32_t>(animation->type.get())), playback.duration,
        track_count, playback.tracks.size(), track_count, pose_by_bone.size());
    if (!playback.valid())
    {
        logger::info("SCOPY ANIM play UNAVAILABLE reason=no-track-resolves-to-the-copy");
        return false;
    }

    // Ground truth: the clip's own pose at every phase against the pose the engine holds. The source is
    // paused, so its phase is frozen and unknown - the sweep finds it, and the spread between the best
    // and the worst phase is what tells a match from a coincidence.
    std::vector<RE::hkQsTransform> sampled(playback.tracks.size());
    float best_position = std::numeric_limits<float>::max();
    float best_rotation = 0.0f;
    float best_time = 0.0f;
    float worst_position = 0.0f;
    size_t compared = 0;
    for (size_t step = 0; step < Sweep_Steps; ++step)
    {
        const float time = playback.duration * static_cast<float>(step) / static_cast<float>(Sweep_Steps);
        animation->SampleIndividualTransformTracks(time, playback.tracks.data(), static_cast<std::uint32_t>(playback.tracks.size()), sampled.data());
        float position = 0.0f;
        float rotation = 0.0f;
        size_t bones = 0;
        for (size_t i = 0; i < sampled.size(); ++i)
        {
            if (!truth[i])
                continue;
            const PoseSample clip_pose = read_pose(sampled[i]);
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
    const bool matched = worst_position > 0.0f && best_position < worst_position * Match_Fraction_Of_Control;
    logger::info("SCOPY ANIM play ground-truth verdict={} best={:.4f} worst={:.3f}", matched ? "match" : "none", best_position, worst_position);

    out = std::move(playback);
    return true;
}

size_t play_animation_clip(ClipPlayback const& clip, float seconds, RE::NiAVObject& copy_root)
{
    if (!clip.valid())
        return 0;

    const float length = clip.duration > 0.05f ? clip.duration : 0.05f;
    float looped = std::fmod(seconds, length);
    if (looped < 0.0f)
        looped += length;

    std::vector<RE::hkQsTransform> sampled(clip.tracks.size());
    clip.animation->SampleIndividualTransformTracks(looped, clip.tracks.data(), static_cast<std::uint32_t>(clip.tracks.size()), sampled.data());

    size_t written = 0;
    for (size_t i = 0; i < sampled.size(); ++i)
    {
        RE::NiAVObject* const node = clip.nodes[i];
        if (!node)
            continue;
        RE::NiTransform local = ni_local_from_pose(read_pose(sampled[i]), false);
        if (!std::isfinite(local.scale) || local.scale <= 0.0f)
            local.scale = node->local.scale;
        node->local = local;
        ++written;
    }
    recompute_subtree_worlds(copy_root, 0);
    return written;
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
