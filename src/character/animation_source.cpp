//
// Created by AmazingBuff on 2026/10/9.
//

#include "character/animation_source.h"

#include "RE/B/BSAnimationGraphManager.h"
#include "RE/B/BSFadeNode.h"
#include "RE/B/BShkbAnimationGraph.h"
#include "RE/B/BSVisit.h"
#include "RE/H/hkClass.h"
#include "RE/H/hkaBone.h"
#include "RE/H/hkaSkeleton.h"
#include "RE/H/hkbAnimationBindingSet.h"
#include "RE/H/hkbBehaviorGraph.h"
#include "RE/H/hkbCharacter.h"
#include "RE/H/hkbCharacterSetup.h"
#include "RE/H/hkbGenerator.h"
#include "RE/T/TESObjectREFR.h"

#include <algorithm>

PLUGIN_NAMESPACE_BEGIN

namespace
{
    // The log carries a sample of a difference, never all of it: the counts beside it say how many
    // there are.
    constexpr size_t Max_Names_Reported = 8;

    struct NodeTable
    {
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

    // One behaviour graph's evidence and its alignment. A graph with no usable skeleton comes back
    // with bones == 0, having said which pointer was missing.
    SkeletonAlignment report_graph(RE::BShkbAnimationGraph& graph, size_t index, RE::TESObjectREFR& source, RE::NiAVObject& source_root, NodeTable const& copy_nodes)
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

        if (!setup)
        {
            logger::info("SCOPY ANIM graph[{}] skeleton=UNAVAILABLE reason=null-character-setup", index);
            return SkeletonAlignment{};
        }
        const RE::hkaSkeleton* const skeleton = setup->animationSkeleton.get();
        if (!skeleton)
        {
            logger::info("SCOPY ANIM graph[{}] skeleton=UNAVAILABLE reason=null-animation-skeleton", index);
            return SkeletonAlignment{};
        }
        if (skeleton->bones.empty())
        {
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

        const SkeletonAlignment alignment = align_skeleton(bone_names, bone_parents, copy_nodes.names, copy_nodes.parents, Max_Names_Reported);

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

        return alignment;
    }

    const char* verdict_of(SkeletonAlignment const& alignment)
    {
        if (alignment.bones == 0)
            return "UNAVAILABLE";
        if (!alignment_resolves_all_bones(alignment))
            return "INCOMPLETE";
        return alignment.ambiguous == 0 ? "PASS" : "PASS-AMBIGUOUS";
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

    bool have_alignment = false;
    size_t best_graph = 0;
    SkeletonAlignment best{};
    for (uint32_t i = 0; i < graph_count; ++i)
    {
        RE::BShkbAnimationGraph* const graph = manager->graphs[i].get();
        if (!graph)
        {
            logger::info("SCOPY ANIM graph[{}] UNAVAILABLE reason=null-graph", i);
            continue;
        }
        const SkeletonAlignment alignment = report_graph(*graph, i, source, source_root, copy_nodes);
        if (alignment.bones == 0)
            continue;
        if (!have_alignment || alignment.matched > best.matched)
        {
            best = alignment;
            best_graph = i;
            have_alignment = true;
        }
    }

    if (!have_alignment)
    {
        logger::info("SCOPY ANIM gate verdict=UNAVAILABLE reason=no-animation-skeleton");
        return;
    }
    logger::info("SCOPY ANIM gate verdict={} graph={} matched={}/{} ambiguous={}",
        verdict_of(best), best_graph, best.matched, best.bones, best.ambiguous);
}

PLUGIN_NAMESPACE_END
