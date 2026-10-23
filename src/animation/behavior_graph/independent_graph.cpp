//
// Created by AmazingBuff on 2026/10/24.
//
#include "animation/behavior_graph/independent_graph.h"

#include "RE/B/BShkbAnimationGraph.h"
#include "RE/H/hkbBehaviorGraph.h"
#include "RE/H/hkbCharacter.h"
#include "RE/H/hkbCharacterSetup.h"
#include "RE/M/MemoryManager.h"
#include "animation/behavior_graph/ownership_probe.h"
#include "animation/settings.h"

#include <chrono>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
namespace
{
    // AE address-library ids from the 1.6.1170 audit; the experiment is gated to AE.
    constexpr std::uint64_t GraphConstructorID = 63555;
    constexpr std::uint64_t GraphLoaderID = 63585;
    constexpr std::int32_t GraphAlignment = 0x10;

    using constructor_t = RE::BShkbAnimationGraph* (*)(RE::BShkbAnimationGraph*, RE::Actor*, bool);
    using loader_t = bool (*)(RE::BShkbAnimationGraph*, RE::BSFixedString*, bool);

    struct Identity
    {
        std::uintptr_t graph{};
        std::uintptr_t holder{};
        std::uintptr_t root{};
        std::uintptr_t behavior{};
        std::uintptr_t project_db{};
        std::uintptr_t project_name{};
        std::uintptr_t setup{};
        std::uintptr_t project{};
        std::uintptr_t bindings{};
        std::uintptr_t event_queue{};
        std::uintptr_t active_nodes{};
        std::uintptr_t variable_values{};
        std::uintptr_t generator0{};
        std::uintptr_t generator1{};
        std::uintptr_t behavior_data{};
        std::uintptr_t bones{};
        std::size_t bone_count{};
    };

    std::uintptr_t address(void const* pointer)
    {
        return reinterpret_cast<std::uintptr_t>(pointer);
    }

    Identity identify(RE::BShkbAnimationGraph const& graph)
    {
        RE::hkbCharacter const& character = graph.characterInstance;
        Identity identity;
        identity.graph = address(&graph);
        identity.holder = address(graph.holder);
        identity.root = address(graph.rootNode);
        identity.behavior = address(character.behaviorGraph.get());
        identity.project_db = address(graph.projectDBData);
        identity.project_name = address(graph.projectName.c_str());
        identity.setup = address(character.setup.get());
        identity.project = address(character.projectData.get());
        identity.bindings = address(character.animationBindingSet.get());
        identity.event_queue = address(character.eventQueue.get());
        if (RE::hkbBehaviorGraph const* const behavior = character.behaviorGraph.get())
        {
            identity.active_nodes = address(behavior->activeNodes);
            identity.variable_values = address(behavior->variableValueSet.get());
            identity.behavior_data = address(behavior->data.get());
        }
        identity.generator0 = address(graph.generatorOutputs[0]);
        identity.generator1 = address(graph.generatorOutputs[1]);
        identity.bones = address(graph.boneNodes.data());
        identity.bone_count = graph.boneNodes.size();
        return identity;
    }

    char const* same_or_different(std::uintptr_t const source, std::uintptr_t const candidate)
    {
        return source && source == candidate ? "same" : "different";
    }

    char const* first_drift(Identity const& before, Identity const& after)
    {
        if (before.graph != after.graph) return "graph";
        if (before.behavior != after.behavior) return "behavior";
        if (before.project_db != after.project_db) return "project-db";
        if (before.project_name != after.project_name) return "project-name";
        if (before.setup != after.setup) return "setup";
        if (before.project != after.project) return "project";
        if (before.bindings != after.bindings) return "bindings";
        if (before.event_queue != after.event_queue) return "event-queue";
        if (before.active_nodes != after.active_nodes) return "active-nodes";
        if (before.variable_values != after.variable_values) return "variable-values";
        if (before.generator0 != after.generator0 || before.generator1 != after.generator1) return "generators";
        if (before.bones != after.bones || before.bone_count != after.bone_count) return "bones";
        return "none";
    }

    bool assembled(Identity const& candidate)
    {
        return candidate.behavior && candidate.project_db && candidate.project_name && candidate.behavior_data;
    }

    bool separated(Identity const& source, Identity const& candidate)
    {
        return candidate.graph && candidate.graph != source.graph &&
            candidate.behavior && candidate.behavior != source.behavior &&
            candidate.active_nodes != source.active_nodes &&
            candidate.variable_values != source.variable_values &&
            candidate.generator0 != source.generator0 &&
            candidate.generator1 != source.generator1;
    }
}

void run_independent_graph_experiment(RE::BShkbAnimationGraph const& source_graph,
    std::uint32_t const actor_id, std::uint64_t const request, std::uint64_t const source_generation)
{
    static bool attempted = false;
    const IndependentGraphMode mode = independent_graph_setting();
    if (mode == IndependentGraphMode::off || attempted)
        return;
    attempted = true;

    if (!REL::Module::IsAE())
    {
        logger::warn("INDEPENDENT GRAPH actor={:08x} request={} skip=runtime-only-ae", actor_id, request);
        return;
    }
    const REL::Version version = REL::Module::get().version();
    if (version < SKSE::RUNTIME_SSE_1_6_1170)
    {
        logger::warn("INDEPENDENT GRAPH actor={:08x} request={} skip=runtime-version", actor_id, request);
        return;
    }
    logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase=begin mode={} runtime={}.{}.{}.{} holder=nullptr factory={} loader={} third-arg=false",
        actor_id, request, independent_graph_name(mode), version.major(), version.minor(), version.patch(), version.build(),
        GraphConstructorID, GraphLoaderID);

    const Identity source_before = identify(source_graph);
    observe_source_baseline(source_graph, actor_id, request, source_generation);

    Identity candidate_final;
    bool constructed = false;
    bool loaded = false;
    {
        void* const memory = RE::MemoryManager::GetSingleton()->Allocate(
            sizeof(RE::BShkbAnimationGraph), GraphAlignment, true);
        if (!memory)
        {
            logger::error("INDEPENDENT GRAPH actor={:08x} request={} outcome=allocate-failed", actor_id, request);
            return;
        }
        static REL::Relocation<constructor_t> construct{ REL::ID(GraphConstructorID) };
        RE::BShkbAnimationGraph* const raw = construct(static_cast<RE::BShkbAnimationGraph*>(memory), nullptr, false);
        RE::BSTSmartPointer<RE::BShkbAnimationGraph> candidate;
        if (raw)
        {
            candidate.reset(raw);
            constructed = true;
        }
        else
        {
            logger::error("INDEPENDENT GRAPH actor={:08x} request={} outcome=construct-null", actor_id, request);
            RE::MemoryManager::GetSingleton()->Deallocate(memory, true);
        }

        if (constructed)
        {
            const Identity constructed_identity = identify(*candidate);
            logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase=constructed graph={:#x} holder={:#x} root={:#x} behavior={:#x} project-db={:#x} project-name={:#x}",
                actor_id, request, constructed_identity.graph, constructed_identity.holder, constructed_identity.root,
                constructed_identity.behavior, constructed_identity.project_db, constructed_identity.project_name);
            logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase=constructed setup={:#x} project={:#x} bindings={:#x} event-queue={:#x} behavior-data={:#x} bones={:#x}/{}",
                actor_id, request, constructed_identity.setup, constructed_identity.project, constructed_identity.bindings,
                constructed_identity.event_queue, constructed_identity.behavior_data, constructed_identity.bones,
                constructed_identity.bone_count);
            logger::info("INDEPENDENT GRAPH actor={:08x} request={} compare-shell graph={} holder={} root={} character={}",
                actor_id, request,
                same_or_different(source_before.graph, constructed_identity.graph),
                same_or_different(source_before.holder, constructed_identity.holder),
                same_or_different(source_before.root, constructed_identity.root),
                same_or_different(address(&source_graph.characterInstance), address(&candidate->characterInstance)));

            if (mode == IndependentGraphMode::load)
            {
                RE::BSFixedString project_name = source_graph.projectName;
                static REL::Relocation<loader_t> load{ REL::ID(GraphLoaderID) };
                const auto started = std::chrono::steady_clock::now();
                loaded = load(candidate.get(), &project_name, false);
                const double load_ms = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - started).count();
                const Identity loaded_identity = identify(*candidate);
                logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase=load outcome={} ms={:.3f} behavior={:#x} project-db={:#x} project-name={:#x} behavior-data={:#x} active-nodes={:#x} variable-values={:#x}",
                    actor_id, request, loaded ? "accepted" : "rejected", load_ms, loaded_identity.behavior,
                    loaded_identity.project_db, loaded_identity.project_name, loaded_identity.behavior_data,
                    loaded_identity.active_nodes, loaded_identity.variable_values);
                logger::info("INDEPENDENT GRAPH actor={:08x} request={} compare-loaded project-db={} project={} setup={} bindings={} behavior-data={} active-nodes={} variable-values={}",
                    actor_id, request,
                    same_or_different(source_before.project_db, loaded_identity.project_db),
                    same_or_different(source_before.project, loaded_identity.project),
                    same_or_different(source_before.setup, loaded_identity.setup),
                    same_or_different(source_before.bindings, loaded_identity.bindings),
                    same_or_different(source_before.behavior_data, loaded_identity.behavior_data),
                    same_or_different(source_before.active_nodes, loaded_identity.active_nodes),
                    same_or_different(source_before.variable_values, loaded_identity.variable_values));
                candidate_final = loaded_identity;
            }
            else
                candidate_final = constructed_identity;
        }
    }

    const Identity source_after = identify(source_graph);
    logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase=destroyed source-drift={} registry-probe=unavailable",
        actor_id, request, first_drift(source_before, source_after));
    observe_source_baseline(source_graph, actor_id, request, source_generation);
    logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase=end constructed={} loaded={} assembly={} separated={} source-stable={}",
        actor_id, request, constructed, loaded, assembled(candidate_final), separated(source_before, candidate_final),
        std::string_view(first_drift(source_before, source_after)) == "none");
}
}
PLUGIN_NAMESPACE_END
