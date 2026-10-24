//
// Created by AmazingBuff on 2026/10/24.
//
#include "animation/behavior_graph/independent_graph.h"

#include "RE/B/BShkbAnimationGraph.h"
#include "RE/H/hkbBehaviorGraph.h"
#include "RE/H/hkbCharacter.h"
#include "RE/H/hkbCharacterSetup.h"
#include "RE/H/hkbContext.h"
#include "RE/H/hkbProjectData.h"
#include "RE/H/hkbProjectStringData.h"
#include "RE/M/MemoryManager.h"
#include "RE/T/TESNPC.h"
#include "RE/T/TESRace.h"
#include "animation/behavior_graph/ownership_probe.h"
#include "animation/settings.h"

#include <chrono>
#include <string>
#include <vector>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
namespace
{
    // AE address-library ids from the 1.6.1170 audit; the experiment is gated to AE.
    constexpr std::uint64_t GraphConstructorID = 63555;
    constexpr std::uint64_t GraphLoaderID = 63585;
    // G2 drive entries (activation/update audit, 2026-10-24): 58453 is the ENGINE's
    // character-level activator — it allocates the pose buffer from
    // setup.animationSkeleton (58466) BEFORE calling hkbBehaviorGraph::Activate, which
    // the graph-level 63586 wrapper skips; 58329 is hkbBehaviorGraph::Update
    // (vtable slot 5); 58315 fills an hkbContext from a character; 63589 is the
    // engine's post-update sampling entry (gates: setup/animationSkeleton/behaviorGraph
    // non-null, graph activated; reads the current-animation word at graph+0x240 and
    // calls the 63581 sampler that copies the generated pose track into poseLocal).
    constexpr std::uint64_t CharacterActivatorID = 58453;
    constexpr std::uint64_t BehaviorGraphUpdateID = 58329;
    constexpr std::uint64_t ContextBuilderID = 58315;
    constexpr std::uint64_t SamplingEntryID = 63589;
    // Pre-registered G2 drive budget: 120 fixed steps of 1/60 s (2 simulated seconds).
    constexpr std::uint32_t DriveSteps = 120;
    constexpr float DriveTimestep = 1.0f / 60.0f;
    constexpr std::int32_t GraphAlignment = 0x10;

    using constructor_t = RE::BShkbAnimationGraph* (*)(RE::BShkbAnimationGraph*, RE::Actor*, bool);
    // 63585 takes a NUL-terminated full project path (const char*): helper 69488 splits
    // it on the last separator, 63844 canonicalizes dir+file and looks the combined path
    // up in the global hkbProjectData registry (2026-10-24 second loader audit).
    using loader_t = bool (*)(RE::BShkbAnimationGraph*, const char*, bool);
    using character_activator_t = void (*)(RE::hkbCharacter*);
    using context_builder_t = void (*)(RE::hkbContext*, RE::hkbCharacter*, void* world, void* attachment_manager,
        void* animation_cache);
    using behavior_updater_t = void (*)(RE::hkbBehaviorGraph*, const RE::hkbContext*, float);
    using sampling_entry_t = void (*)(RE::BShkbAnimationGraph*, std::uint8_t* mode_byte);
    using update_wrapper_t = bool (*)(RE::BShkbAnimationGraph*, void* record);

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

    // G2 drive observations. Offsets 0xA0/0xA8/0x12C/0x12E are audit-verified fields
    // without CommonLib accessors (activation/update audits, 2026-10-24);
    // active_nodes count is the NodeList dword at +8 that the audited Deactivate
    // crash instruction reads.
    struct DriveState
    {
        std::uintptr_t active_nodes{};
        std::int32_t active_node_count{};
        std::uintptr_t active_map_a{};
        std::uintptr_t active_map_b{};
        std::uintptr_t variable_values{};
        std::uint8_t active_flag{};
        std::uint8_t children_flag{};
        std::uintptr_t root_generator{};
        std::uintptr_t behavior_data{};
        std::uintptr_t generator0{};
        std::uintptr_t generator1{};
        std::uintptr_t pose_local{};
        std::int32_t pose_count{};
        std::uintptr_t setup_animation_skeleton{};
        std::uintptr_t setup_data{};
        std::uintptr_t setup_mirrored{};
    };

    DriveState observe_drive(RE::BShkbAnimationGraph const& graph)
    {
        DriveState state;
        state.generator0 = address(graph.generatorOutputs[0]);
        state.generator1 = address(graph.generatorOutputs[1]);
        RE::hkbCharacter const& character = graph.characterInstance;
        state.pose_local = address(character.poseLocal);
        state.pose_count = character.numPoseLocal;
        if (RE::hkbCharacterSetup const* const setup = character.setup.get())
        {
            state.setup_animation_skeleton = address(setup->animationSkeleton.get());
            state.setup_data = address(setup->data.get());
            state.setup_mirrored = address(setup->mirroredSkeleton.get());
        }
        RE::hkbBehaviorGraph const* const behavior = character.behaviorGraph.get();
        if (!behavior)
            return state;
        const std::uintptr_t base = address(behavior);
        state.root_generator = address(behavior->rootGenerator.get());
        state.behavior_data = address(behavior->data.get());
        state.active_nodes = address(behavior->activeNodes);
        if (state.active_nodes)
            state.active_node_count = *reinterpret_cast<const std::int32_t*>(state.active_nodes + 8);
        state.active_map_a = *reinterpret_cast<const std::uintptr_t*>(base + 0xA0);
        state.active_map_b = *reinterpret_cast<const std::uintptr_t*>(base + 0xA8);
        state.variable_values = address(behavior->variableValueSet.get());
        state.active_flag = *reinterpret_cast<const std::uint8_t*>(base + 0x12C);
        state.children_flag = *reinterpret_cast<const std::uint8_t*>(base + 0x12E);
        return state;
    }

    void log_drive_state(std::uint32_t const actor_id, std::uint64_t const request, char const* phase,
        bool const activated, char const* drift, DriveState const& state)
    {
        logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase={} activated={} source-drift={} active-nodes={:#x}/{} maps={:#x},{:#x} variable-values={:#x} flag={} children-flag={} root-generator={:#x} behavior-data={:#x} generator0={:#x} generator1={:#x} pose-local={:#x}/{} setup-skeleton={:#x} setup-data={:#x} setup-mirrored={:#x}",
            actor_id, request, phase, activated, drift, state.active_nodes, state.active_node_count,
            state.active_map_a, state.active_map_b, state.variable_values, state.active_flag, state.children_flag,
            state.root_generator, state.behavior_data, state.generator0, state.generator1, state.pose_local,
            state.pose_count, state.setup_animation_skeleton, state.setup_data, state.setup_mirrored);
    }

    std::uint64_t fnv1a_64(void const* data, std::size_t const size)
    {
        std::uint64_t hash = 0xcbf29ce484222325ull;
        const auto* const bytes = static_cast<std::uint8_t const*>(data);
        for (std::size_t i = 0; i < size; ++i)
        {
            hash ^= bytes[i];
            hash *= 0x100000001b3ull;
        }
        return hash;
    }

    // Pose motion hash for the G2 motion tier; returns false when the character has no
    // local pose buffer (poses are written by the update's output stage at best).
    bool pose_hash(RE::hkbCharacter const& character, std::uint64_t& out)
    {
        if (!character.poseLocal || character.numPoseLocal <= 0)
            return false;
        out = fnv1a_64(character.poseLocal, static_cast<std::size_t>(character.numPoseLocal) * 0x30);
        return true;
    }

    std::string format_pose_hash(RE::hkbCharacter const& character)
    {
        std::uint64_t hash = 0;
        if (pose_hash(character, hash))
            return fmt::format("{:016x}", hash);
        return "unavailable";
    }

    // Decisive G2 discriminator (v3): did the update change ANY node/variable state?
    // The activeNodes array entries are 0x90-byte per-node runtime states; the variable
    // set's word array is self-describing ({ptr, count} at +0x10/+0x18). Generator
    // outputs are dumped as raw leading bytes because their header layout did not
    // match the CommonLib port (the byte-count read was garbage).
    std::string format_gen_dump(RE::hkbGeneratorOutput const* output)
    {
        if (!output)
            return "unavailable";
        const auto* const bytes = reinterpret_cast<const std::uint8_t*>(output);
        std::string dump;
        for (std::size_t i = 0; i < 16; ++i)
            dump += fmt::format("{:02x}", bytes[i]);
        return dump;
    }

    // generatorOutputs[i] is a one-element array header; its element carries the track
    // data the 63581 sampler reads (pose offset at +0x32). Hash a fixed window of the
    // element to detect generation churn without assuming the element layout.
    std::string format_gen_element(RE::hkbGeneratorOutput const* output)
    {
        if (!output)
            return "unavailable";
        const void* const element = *reinterpret_cast<void* const*>(output);
        if (!element)
            return "element=0";
        return fmt::format("hash={:016x}", fnv1a_64(element, 0x100));
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

            if (mode != IndependentGraphMode::construct)
            {
                // 63585 takes a NUL-terminated full project path; the registry keys are
                // canonicalized dir+file strings with the extension intact (the resolver
                // strips it only when writing projectName back). The 2026-10-24 second
                // run showed the source project's stringData paths are empty and
                // projectName is the extension-less "DefaultFemale", so the value is now
                // taken from the holder's race data and candidates are tried in a
                // pre-registered order, stopping at the first accepted load. Failed
                // loads unwind to a bare shell without touching source or registry.
                const RE::Actor* const holder = source_graph.holder;
                const RE::TESRace* race = nullptr;
                std::size_t sex = 0;
                if (holder && holder->GetActorBase())
                {
                    if (const RE::TESNPC* const npc = holder->GetActorBase()->As<RE::TESNPC>())
                        sex = npc->GetSex() == RE::SEX::kFemale ? 1u : 0u;
                    race = holder->GetRace();
                }
                const char* const bg_model = race && *race->behaviorGraphs[sex].model.c_str() ? race->behaviorGraphs[sex].model.c_str() : "";
                const char* const root_name = race && *race->rootBehaviorGraphNames[sex].c_str() ? race->rootBehaviorGraphNames[sex].c_str() : "";
                const char* const project_field = race && *race->behaviorGraphProjectNames[sex].c_str() ? race->behaviorGraphProjectNames[sex].c_str() : "";
                const RE::hkbProjectData* const project = source_graph.characterInstance.projectData.get();
                const RE::hkbProjectStringData* const strings = project ? project->stringData.get() : nullptr;
                const char* const behavior_path = strings && !strings->behaviorPath.empty() ? strings->behaviorPath.c_str() : "";
                const char* const source_project_name = source_graph.projectName.c_str();
                logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase=race-fields sex={} bg-model='{}' root-name='{}' project-field='{}' string-data-behavior-path='{}' source-project-name='{}'",
                    actor_id, request, sex, bg_model, root_name, project_field, behavior_path, source_project_name);

                struct PathTry
                {
                    const char* source;
                    std::string path;
                };
                std::vector<PathTry> tries;
                const auto push_try = [&tries](const char* source, const char* value) {
                    if (value && *value && tries.size() < 4)
                        tries.push_back(PathTry{ source, value });
                };
                push_try("race-bg-model", bg_model);
                push_try("race-project-name", project_field);
                push_try("race-root-name", root_name);
                std::string rebuilt(behavior_path);
                if (!rebuilt.empty() && rebuilt.back() != '\\' && rebuilt.back() != '/')
                    rebuilt += '\\';
                rebuilt += source_project_name;
                if (!rebuilt.empty())
                    push_try("string-data-rebuilt", rebuilt.c_str());

                static REL::Relocation<loader_t> load{ REL::ID(GraphLoaderID) };
                for (std::size_t index = 0; index < tries.size() && !loaded; ++index)
                {
                    logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase=load-try {}/{} source={} path='{}'",
                        actor_id, request, index + 1, tries.size(), tries[index].source, tries[index].path);
                    const auto started = std::chrono::steady_clock::now();
                    loaded = load(candidate.get(), tries[index].path.c_str(), false);
                    const double load_ms = std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - started).count();
                    const Identity loaded_identity = identify(*candidate);
                    logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase=load try={} outcome={} ms={:.3f} behavior={:#x} project-db={:#x} project-name={:#x} behavior-data={:#x} active-nodes={:#x} variable-values={:#x}",
                        actor_id, request, index + 1, loaded ? "accepted" : "rejected", load_ms, loaded_identity.behavior,
                        loaded_identity.project_db, loaded_identity.project_name, loaded_identity.behavior_data,
                        loaded_identity.active_nodes, loaded_identity.variable_values);
                    candidate_final = loaded_identity;
                }
                const Identity loaded_identity = identify(*candidate);
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
                if (loaded)
                {
                    // Deliberate leak (contract revision of 2026-10-24): a loaded graph
                    // cannot be destroyed while unactivated — the destructor's
                    // hkbBehaviorGraph::Deactivate dereferences the null activeNodes —
                    // so the assembled candidate is kept alive by an owning smart
                    // pointer that is never released, not even at static destruction;
                    // the G2 lifecycle owns its teardown.
                    new RE::BSTSmartPointer<RE::BShkbAnimationGraph>(candidate);
                }

                if (loaded && mode == IndependentGraphMode::drive)
                {
                    // G2 drive contract (v2): the ENGINE's character-level activator
                    // 58453 — it allocates the pose buffer from setup.animationSkeleton
                    // first (58466) and then runs hkbBehaviorGraph::Activate; the
                    // graph-level 63586 wrapper used in the first run skipped the pose
                    // allocation, which is why the candidate produced no motion. The
                    // pose allocator dereferences setup.animationSkeleton unchecked, so
                    // a missing skeleton is a pre-registered clean skip, not a call.
                    observe_source_baseline(source_graph, actor_id, request, source_generation);
                    const std::string source_pose_before = format_pose_hash(source_graph.characterInstance);
                    const DriveState before_activate = observe_drive(*candidate);
                    log_drive_state(actor_id, request, "pre-activate", false, "n/a", before_activate);
                    bool activated = false;
                    if (before_activate.setup_animation_skeleton == 0)
                    {
                        logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase=skip reason=setup-skeleton-missing",
                            actor_id, request);
                    }
                    else
                    {
                        static REL::Relocation<character_activator_t> char_activate{ REL::ID(CharacterActivatorID) };
                        char_activate(&candidate->characterInstance);
                        activated = true;
                        const DriveState after_activate = observe_drive(*candidate);
                        log_drive_state(actor_id, request, "activated", activated,
                            first_drift(source_before, identify(source_graph)), after_activate);
                        const std::string candidate_pose_before = format_pose_hash(candidate->characterInstance);
                        std::string candidate_pose_mid = "not-run";
                        std::string candidate_pose_end = "not-run";

                        // Raw zeroed storage instead of an hkbContext object: the
                        // context's hkRefPtr<hkbGeneratorOutputListener> member has no
                        // complete type on this side, so neither construction nor
                        // destruction of hkbContext can be instantiated; 58315 writes
                        // every field as raw stores anyway.
                        alignas(8) std::uint8_t context_storage[sizeof(RE::hkbContext)]{};
                        RE::hkbContext* const context = reinterpret_cast<RE::hkbContext*>(context_storage);
                        static REL::Relocation<context_builder_t> init_context{ REL::ID(ContextBuilderID) };
                        init_context(context, &candidate->characterInstance, nullptr, nullptr, nullptr);
                        RE::hkbBehaviorGraph* const behavior = candidate->characterInstance.behaviorGraph.get();
                        auto* const candidate_base = candidate.get();
                        const auto* const source_base = &source_graph;
                        const auto source_anim_index = [source_base]() {
                            return *reinterpret_cast<const std::uint16_t*>(reinterpret_cast<std::uintptr_t>(source_base) + 0x240);
                        };
                        const auto log_state_probe = [&](char const* step) {
                            const RE::hkbBehaviorGraph* const behavior_state =
                                candidate->characterInstance.behaviorGraph.get();
                            std::string active = "unavailable";
                            std::string word_vars = "unavailable";
                            if (behavior_state && behavior_state->activeNodes)
                            {
                                const std::uintptr_t nodes = address(behavior_state->activeNodes);
                                const void* const base =
                                    reinterpret_cast<const void*>(*reinterpret_cast<const std::uintptr_t*>(nodes));
                                const std::int32_t count = *reinterpret_cast<const std::int32_t*>(nodes + 8);
                                if (base && count > 0 && count < 0x10000)
                                    active = fmt::format("count={} hash={:016x}", count,
                                        fnv1a_64(base, static_cast<std::size_t>(count) * 0x90));
                                else
                                    active = fmt::format("count={}", count);
                            }
                            if (behavior_state && behavior_state->variableValueSet)
                            {
                                const std::uintptr_t set = address(behavior_state->variableValueSet.get());
                                const void* const word_data =
                                    reinterpret_cast<const void*>(*reinterpret_cast<const std::uintptr_t*>(set + 0x10));
                                const std::int32_t word_count = *reinterpret_cast<const std::int32_t*>(set + 0x18);
                                if (word_data && word_count > 0 && word_count < 0x10000)
                                    word_vars = fmt::format("words={} hash={:016x}", word_count,
                                        fnv1a_64(word_data, static_cast<std::size_t>(word_count) * 4));
                                else
                                    word_vars = fmt::format("words={}", word_count);
                            }
                            const unsigned src_anim = source_anim_index();
                            const unsigned cand_anim =
                                *reinterpret_cast<const std::uint16_t*>(reinterpret_cast<std::uintptr_t>(candidate_base) + 0x240);
                            logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase=probe step={} src-anim={:04x} cand-anim={:04x} active[{}] vars[{}] gen0={} gen0elem={} gen1={}",
                                actor_id, request, step, src_anim, cand_anim, active, word_vars,
                                format_gen_dump(candidate->generatorOutputs[0]),
                                format_gen_element(candidate->generatorOutputs[0]),
                                format_gen_dump(candidate->generatorOutputs[1]));
                        };
                        log_state_probe("activate");
                        const std::uint8_t mode_byte = 0;
                        static REL::Relocation<sampling_entry_t> sampling_entry{ REL::ID(SamplingEntryID) };
                        static REL::Relocation<behavior_updater_t> behavior_update{ REL::ID(BehaviorGraphUpdateID) };
                        for (std::uint32_t step = 0; behavior && step < DriveSteps; ++step)
                        {
                            // The engine requests per-frame children re-activation by
                            // setting graph+0x12E = 1 before every update (63602's tail
                            // does exactly this for each subscribed graph); Update's
                            // internal gate skips its 60079 activation walk otherwise,
                            // which is why the first drive rounds produced no output.
                            *reinterpret_cast<std::uint8_t*>(address(behavior) + 0x12E) = 1;
                            behavior_update(behavior, context, DriveTimestep);
                            // The sampling entry reads the sample-transform count at
                            // graph+0x240 (the engine writes it per frame from the
                            // manager record; for the same skeleton it is always
                            // numPoseLocal).
                            *reinterpret_cast<std::uint16_t*>(reinterpret_cast<std::uintptr_t>(candidate_base) + 0x240) =
                                static_cast<std::uint16_t>(candidate->characterInstance.numPoseLocal);
                            sampling_entry(candidate_base, const_cast<std::uint8_t*>(&mode_byte));
                            if (step + 1 == DriveSteps / 2)
                            {
                                candidate_pose_mid = format_pose_hash(candidate->characterInstance);
                                log_state_probe("60");
                            }
                        }
                        candidate_pose_end = format_pose_hash(candidate->characterInstance);
                        log_state_probe("120");

                        const std::string source_pose_after = format_pose_hash(source_graph.characterInstance);
                        logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase=drive-done steps={} dt={:.6f} source-pose-before={} source-pose-after={} candidate-pose-0={} candidate-pose-60={} candidate-pose-120={}",
                            actor_id, request, DriveSteps, DriveTimestep, source_pose_before, source_pose_after,
                            candidate_pose_before, candidate_pose_mid, candidate_pose_end);
                        log_drive_state(actor_id, request, "post-drive", activated,
                            first_drift(source_before, identify(source_graph)), observe_drive(*candidate));
                    }
                }
            }
            else
                candidate_final = constructed_identity;
        }
    }

    const bool retained_loaded = mode != IndependentGraphMode::off && mode != IndependentGraphMode::construct && loaded;
    const Identity source_after = identify(source_graph);
    logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase={} source-drift={} registry-probe=unavailable",
        actor_id, request, retained_loaded ? "retained" : "destroyed", first_drift(source_before, source_after));
    observe_source_baseline(source_graph, actor_id, request, source_generation);
    logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase=end constructed={} loaded={} assembly={} separated={} source-stable={} retained={}",
        actor_id, request, constructed, loaded, assembled(candidate_final), separated(source_before, candidate_final),
        std::string_view(first_drift(source_before, source_after)) == "none", retained_loaded);
}
}
PLUGIN_NAMESPACE_END
