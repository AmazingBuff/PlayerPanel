//
// Created by AmazingBuff on 2026/10/24.
//
#include "animation/behavior_graph/independent_graph.h"

#include "animation/behavior_graph/clip_reference_policy.h"
#include "animation/clip.h"
#include "animation/behavior_graph/frame_trace.h"
#include "animation/behavior_graph/generation_entry_policy.h"
#include "animation/behavior_graph/observation_chain_policy.h"
#include "animation/behavior_graph/pose_content_policy.h"

#include "RE/B/BShkbAnimationGraph.h"
#include "RE/H/hkaAnimation.h"
#include "RE/H/hkaAnimationBinding.h"
#include "RE/H/hkaSkeleton.h"
#include "RE/H/hkbBehaviorGraph.h"
#include "RE/H/hkbBehaviorGraphData.h"
#include "RE/H/hkbBehaviorGraphStringData.h"
#include "RE/H/hkbCharacter.h"
#include "RE/H/hkbCharacterData.h"
#include "RE/H/hkbCharacterSetup.h"
#include "RE/H/hkbClipGenerator.h"
#include "RE/H/hkbContext.h"
#include "RE/H/hkbFootIkDriver.h"
#include "RE/H/hkbProjectData.h"
#include "RE/H/hkbProjectStringData.h"
#include "RE/M/MemoryManager.h"
#include "RE/T/TESNPC.h"
#include "RE/T/TESRace.h"
#include "animation/behavior_graph/ownership_probe.h"
#include "animation/settings.h"

#include <array>
#include <chrono>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
namespace
{
    // v27: the retained candidate's character, remembered so the display path can read its pose
    // after the bounded drive has finished. Null until a candidate is retained; never stepped
    // from here.
    RE::hkbCharacter* g_held_character = nullptr;
    // v28: the retained graph itself, so the display path can advance it one step per frame.
    RE::BShkbAnimationGraph* g_held_graph = nullptr;

    // AE address-library ids from the 1.6.1170 audit; the experiment is gated to AE.
    constexpr std::uint64_t GraphConstructorID = 63555;
    constexpr std::uint64_t GraphLoaderID = 63585;
    // G2 drive entries (activation/update audits, 2026-10-24): 58453 is the ENGINE's
    // character-level activator — it allocates the pose buffer from
    // setup.animationSkeleton (58466) BEFORE calling hkbBehaviorGraph::Activate, which
    // the graph-level 63586 wrapper skips; 58315 fills an hkbContext from a character.
    constexpr std::uint64_t CharacterActivatorID = 58453;
    constexpr std::uint64_t ContextBuilderID = 58315;
    // Audit22: 63589 owns native context/output preparation; mode0 takes its synchronous path.
    constexpr std::uint64_t GenerationEntryID = 63589;
    constexpr std::uint64_t SamplingBranchID = 63581;
    // Retain the v19 scratch-shell recipe; its guard does not exclude other generation paths.
    constexpr std::uint64_t GraphDestructorID = 63556;
    constexpr std::uint64_t FramePipelineID = 63602;
    // v8 event injection (dispatch-chain audit, 2026-10-24): 58377 is the ENGINE's
    // event-triggered-transition dispatch — hkbBehaviorGraph vtable slot 6's callee;
    // Update's own tail and the batch dispatcher (0xab313e) route queued events
    // through it. Signature (behavior, ctx, event*) with a 24-byte hkbEvent
    // {u32 id, u32 pad, hkRefPtr payload, hkbNode* sender}; only id/payload are read.
    constexpr std::uint64_t EventDispatchID = 58377;
    // Audit 11: vtable slots 2/3 of the final main vtable (0x19881f8) are
    // BSIRagdollDriver::AddRagdollToWorld / RemoveRagdollFromWorld (63621/63622).
    // 63621's head is the writer of the pose-track evidence fields — it stores
    // [tracks+0x32]=0 and [tracks+0x38]=1.0f right after the world/driver/instance
    // gates — and its body activates the ragdoll's rigid bodies into the graph's
    // physics world. 63602 reaches it only behind a gate whose provider (63643)
    // fills a per-frame current-animations hash, a path the candidate never
    // satisfies; v12 calls the slot directly with the candidate's own instance.
    constexpr std::uint64_t AddRagdollToWorldID = 63621;
    constexpr std::uint64_t RemoveRagdollFromWorldID = 63622;
    // Audit 12: slot 0xC (63631) picks one of the projectDBData +0x170/+0x178
    // objects (dl selects the pair member) and stores it at graph+0x110 — the
    // first gate of the sampling entry 63589, without which it returns before
    // reaching 63568, the writer of the kPose track's numData
    // ([tracks+0x32] = word [graph+0x240]).
    constexpr std::uint64_t AnimationDataProviderID = 63631;
    // Audit 13: 63587 is the engine's complete per-frame update recipe (ctx
    // build 58315, preupdate 63761, the behavior update through indirect
    // calls, event drain 63583, pose apply 58429, and the numAnimBones/
    // 0x245=0x101/0x246 + generatorOutputs-swap bookkeeping). The v14 row
    // (post-pipeline1) adjudicated branch ②: all its bookkeeping stayed zero
    // and both double-buffer slots carried only the plugin's own slot-2 write,
    // so 63602's tail never ran 63587 on the candidate. v15 replaces the bare
    // 58329 with a DIRECT 63587 call using 63602's own stack-record layout
    // (its 0xbb63b8 tail): zero-initialized ≥0x30 bytes, float dt@+0x00 and
    // bytes +0x2a..+0x2e = 1,1,1,0,1 (footIK@+0x2e); the third argument is 0.
    // The engine hosts each record on the stack per call, so v15 builds it
    // fresh inside the drive loop each step. 63587's prologue gates (+0x110 /
    // [[+0x110]+0x20] / +0x208 / [[+0x208]+0x12c]) may still block on the
    // candidate — that outcome is pre-registered as the v15 ③ criterion and
    // points at the +0x208 gate-chain audit.
    // v17 (audit 15 Q4): the v15 record (+0x2a=1) copied 63602's tail — a slot-5
    // zero-dt pipeline special, not the engine's per-frame recipe. The manager's
    // real per-frame record is built by 0x54c130 (32898) as {float dt@+0x00;
    // +0x08..+0x27 zero; u16 +0x28 id = 0; byte +0x2A = 0; byte +0x2B = 1;
    // byte +0x2C = 1; byte +0x2E = 1} (moves: dword [rsp+0x50]=0x1000000,
    // word [rsp+0x54]=1, byte [rsp+0x56]=1) — +0x2A=0 is Path B: no slot swap,
    // no track reset, 0x246 cleared, [0x240] recomputed via 63575. The drive
    // record switches to that byte map.
    constexpr std::uint64_t EngineUpdateWrapperID = 63587;
    // v16 (audit 14, the poseLocal last mile): 63564 is the sampling-block predicate
    // 63581 calls first thing (0xbafdda, with the same rcx/rdx 63581 itself received —
    // rdx = word [graph+0x240], provably dead in the callee) and consumes as
    // `test r13b, r13b` at 0xbaff4d; nonzero skips the whole sampling block. Its
    // 0x61-byte body only reads graph+0x238 (physicsWorld) and +0xF0 (ragdollDriver),
    // walks driver+0x88 (58286 = `mov rax,[rcx+0x88]; ret`, a pure getter) to the
    // instance's rigidBodies first entry and returns a byte in AL — no writes, no
    // bookkeeping. Direct-called observation-only at steps 0/60 to adjudicate gate G1.
    // v17 (audit 15 Q1): the predicate is a pure five-level non-empty chain
    // (physicsWorld → ragdollDriver → driver+0x88 instance → rigidBodies[0] → that
    // body's hkpWorldObject::world at +0x10, no numeric comparison); on a healthy
    // living actor it returns false exactly on `rigidBodies[0]->world == 0` — the
    // only non-chain-breaking flip on the candidate. v17 therefore moves the slot-3
    // call (RemoveRagdollFromWorld) to right after the slot-2 call, before the
    // drive loop: slot 2's per-body world registration is undone while the borrow
    // is still in place, `rigidBodies[0]->world` is null at every sampling moment,
    // and 63564 returns false (v12 proved slot 3 leaves the track headers intact).
    constexpr std::uint64_t PosePredicateID = 63564;
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
    // 63556 is the engine's real BShkbAnimationGraph destructor, applied directly to
    // stack temporaries by the engine's own slot-5 helper; 63602 runs one per-graph
    // frame pipeline pass with the shell as scratch holder.
    using graph_destructor_t = void (*)(RE::BShkbAnimationGraph*);
    using frame_pipeline_t = void (*)(RE::BShkbAnimationGraph* shell, RE::BShkbAnimationGraph* target);
    using ragdoll_world_op_t = bool (*)(RE::BShkbAnimationGraph*);
    using animation_data_provider_t = void (*)(RE::BShkbAnimationGraph*, bool);
    using event_dispatch_t = void (*)(RE::hkbBehaviorGraph*, const RE::hkbContext*, void const*);
    // v15 (audit 13): 63587(graph, record, 0) — the engine's per-frame update
    // wrapper; record is 63602's tail stack layout, at least 0x30 bytes.
    using engine_update_wrapper_t = void (*)(RE::BShkbAnimationGraph*, void*, std::uintptr_t);
    // v16 (audit 14): 63564's full 0x61-byte body only reads the graph
    // (+0x238 physicsWorld, +0xF0 ragdollDriver) and the instance's rigidBodies
    // first entry through the pure 58286 getter; it returns a byte in AL and
    // never writes memory, so the convention carries one alive pointer.
    using pose_predicate_t = std::uint8_t (*)(RE::BShkbAnimationGraph const*);
    // v17 (audit 15 Q2/Q3): 63581 — the sampling block 63589's b246=0 branch
    // reaches. Two live arguments (rcx = graph, edx = word [graph+0x240]); every
    // other register is overwritten before first use, so the call is bounded.
    using sampling_branch_t = void (*)(RE::BShkbAnimationGraph*, std::uint16_t);
    using generation_entry_t = void (*)(RE::BShkbAnimationGraph*, std::uint8_t const*);

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

    // Event-name lookup over the behavior graph's OWN string data
    // (hkbBehaviorGraphData::stringData->eventNames): a plain Havok hkArray of
    // hkStringPtr where the EVENT ID IS THE ARRAY INDEX — the same id space the
    // parallel eventInfos array and the state-machine event-transition maps key
    // on. hkStringPtr carries a managed flag in the pointer's low bit, so it is
    // masked before use. The ProjectDBData hash maps stay unwalked: the dbdump
    // proved their layout, but their bucket chains hold tombstone entries whose
    // key bytes are garbage — the engine only ever pointer-compares keys there.
    std::int32_t lookup_event_id_in_graph(const RE::hkbBehaviorGraphData& data, char const* const name)
    {
        const RE::hkbBehaviorGraphStringData* const strings = data.stringData.get();
        if (!strings)
            return -1;
        const auto& event_names = strings->eventNames;
        const std::int32_t count = static_cast<std::int32_t>(event_names.size());
        if (!event_names.data() || count <= 0 || count > 0x8000)
            return -1;
        for (std::int32_t id = 0; id < count; ++id)
        {
            const std::uintptr_t ptr =
                reinterpret_cast<std::uintptr_t>(event_names[static_cast<std::size_t>(id)].data()) & ~1ull;
            const char* const entry = reinterpret_cast<const char*>(ptr);
            if (entry && std::strcmp(entry, name) == 0)
                return id;
        }
        return -1;
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

    // v21: bounded, plausibility-checked read of a skeleton bone name. The string lives
    // in engine memory, so an implausible pointer is reported instead of dereferenced,
    // and the scan stops at 48 characters so a malformed name cannot walk far.
    std::string read_string(RE::hkStringPtr const& text, std::size_t const max_length)
    {
        const char* const raw = text.c_str();
        if (!plausible_object_pointer(reinterpret_cast<std::uintptr_t>(raw)))
            return std::string("unreadable");
        std::size_t length = 0;
        while (length < max_length && raw[length] != '\0')
            ++length;
        return std::string(raw, length);
    }

    std::string read_bone_name(RE::hkStringPtr const& name)
    {
        // 48 keeps a bone name intact; a clip path is longer and gets its own bound.
        return read_string(name, 48);
    }

    // v23 semantic-correspondence helpers. A live graph node counts as a clip generator
    // only when its vtable is the one REL resolves for hkbClipGenerator; anything else is
    // reported as "no clip generator" rather than cast to an assumed type.
    bool node_is_clip_generator(RE::hkbNode const* const node)
    {
        if (!node)
            return false;
        return *reinterpret_cast<const std::uintptr_t*>(node) == RE::VTABLE_hkbClipGenerator[0].address();
    }

    PoseSample pose_sample_from_transform(RE::hkQsTransform const& entry)
    {
        float translation[4]{};
        float rotation[4]{};
        float scale[4]{};
        std::memcpy(translation, &entry.translation, sizeof(translation));
        std::memcpy(rotation, &entry.rotation, sizeof(rotation));
        std::memcpy(scale, &entry.scale, sizeof(scale));
        PoseSample sample;
        sample.translation = { translation[0], translation[1], translation[2] };
        sample.rotation = { rotation[0], rotation[1], rotation[2], rotation[3] };
        sample.scale = { scale[0], scale[1], scale[2] };
        return sample;
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
    template <class T>
    T generation_field(void const* base, std::size_t offset)
    {
        T value;
        std::memcpy(&value, static_cast<std::byte const*>(base) + offset, sizeof(value));
        return value;
    }

    GenerationEntryView generation_entry_view(RE::BShkbAnimationGraph const& graph)
    {
        GenerationEntryView view{};
        RE::hkbBehaviorGraph const* const behavior = graph.characterInstance.behaviorGraph.get();
        view.outer_behavior = address(graph.behaviorGraph);
        view.character_behavior = address(behavior);
        if (behavior)
        {
            view.active = behavior->isActive;
            view.active_nodes = address(behavior->activeNodes);
            if (behavior->activeNodes)
            {
                view.active_data = generation_field<std::uintptr_t>(behavior->activeNodes, 0);
                view.active_count = generation_field<std::int32_t>(behavior->activeNodes, 8);
            }
            view.variable_values = address(behavior->variableValueSet.get());
            if (behavior->variableValueSet)
            {
                view.variable_data = generation_field<std::uintptr_t>(behavior->variableValueSet.get(), 0x10);
                view.variable_count = generation_field<std::int32_t>(behavior->variableValueSet.get(), 0x18);
            }
        }
        view.pose = address(graph.characterInstance.poseLocal);
        view.pose_count = graph.characterInstance.numPoseLocal;
        for (std::size_t i = 0; i < view.outputs.size(); ++i)
        {
            view.outputs[i] = address(graph.generatorOutputs[i]);
            if (graph.generatorOutputs[i])
                view.elements[i] = generation_field<std::uintptr_t>(graph.generatorOutputs[i], 0);
        }
        return view;
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
    // v27: filled where the candidate is still in scope; the display path reads it later.
    RE::hkbCharacter* retained_character = nullptr;
    RE::BShkbAnimationGraph* retained_graph = nullptr;
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
                    retained_character = &candidate->characterInstance;
                    retained_graph = candidate.get();
                }

                if (loaded && (mode == IndependentGraphMode::drive || mode == IndependentGraphMode::inject))
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

                        const bool generation_profile = mode == IndependentGraphMode::inject && version == SKSE::RUNTIME_SSE_1_6_1170;
                        GenerationEntryStatus generation_guard = GenerationEntryStatus::e_ready;
                        if (generation_profile)
                        {
                            generation_guard = generation_entry_status(generation_entry_view(*candidate), generation_entry_view(source_graph));
                            logger::info("GRAPH ENTRY schema=v20 actor={:08x} request={} outcome={} reason={} entry=63589 mode=0 intent=controlled-native-wrapper",
                                actor_id, request, generation_guard == GenerationEntryStatus::e_ready ? "ready" : "skip",
                                generation_entry_reason(generation_guard));
                        }
                        if (generation_guard == GenerationEntryStatus::e_ready)
                        {
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
                            if (mode == IndependentGraphMode::inject)
                            {
                                // G2 v8 contract (v8d revision): resolve the pre-registered
                                // names through the behavior graph's OWN event-name array
                                // (id = index, the state-machine transition id space), log a
                                // bounded sample of real event names as evidence, then ONE
                                // dispatch of the first resolved name through the engine's
                                // event-triggered-transition entry (58377) before the first
                                // pipeline step.
                                const std::uintptr_t project_db = reinterpret_cast<std::uintptr_t>(candidate->projectDBData);
                                std::string db_dump;
                                for (std::size_t offset = 0x60; project_db && offset < 0xE8; offset += 8)
                                    db_dump += fmt::format("{:016x}", *reinterpret_cast<const std::uint64_t*>(project_db + offset));
                                logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase=dbdump db={:#x} qwords[60..e8]={}",
                                    actor_id, request, project_db, db_dump);
                                const RE::hkbBehaviorGraphData* const graph_data = behavior ? behavior->data.get() : nullptr;
                                const RE::hkbBehaviorGraphStringData* const graph_strings =
                                    graph_data ? graph_data->stringData.get() : nullptr;
                                std::string sample;
                                if (graph_strings && graph_strings->eventNames.data())
                                {
                                    const std::int32_t total = static_cast<std::int32_t>(graph_strings->eventNames.size());
                                    for (std::int32_t i = 0; i < total && i < 16; ++i)
                                    {
                                        const char* const entry = graph_strings->eventNames[static_cast<std::size_t>(i)].c_str();
                                        if (entry && *entry)
                                            sample += fmt::format("'{}',", entry);
                                    }
                                    logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase=events count={} first={}",
                                        actor_id, request, total, sample);
                                }
                                else
                                {
                                    logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase=events count=0 first=",
                                        actor_id, request);
                                }
                                // v8e: names pre-registered from the v8d phase=events evidence row (the graph's
                                // own event table — Skyrim-level anim-event names like MT_Idle/MoveStart/
                                // WeaponDraw are NOT graph events). WeapEquip leads: it is the equip-chain
                                // entry the G3 first batch (one-handed Equip->Idle->Unequip) needs.
                                // v9 gates row (read-only): the slot-2 output-reset gates
                                // (physicsWorld at graph+0x238 and the character's ragdoll
                                // driver at +0x30) decide the v8e attribution — a LIVING
                                // source actor most likely has no ragdoll driver either, and
                                // if the source shows the same empty gates while still
                                // producing poses, the slot-2 hypothesis is falsified. The
                                // element head comparison shows what a bound output element
                                // looks like on the source versus the untouched candidate.
                                {
                                    const auto element_head = [](RE::hkbGeneratorOutput const* output) {
                                        if (!output)
                                            return std::string("null");
                                        const void* const element = *reinterpret_cast<void* const*>(output);
                                        if (!element)
                                            return std::string("elem0");
                                        const auto* const bytes = static_cast<const std::uint8_t*>(element);
                                        std::string dump;
                                        for (std::size_t i = 0; i < 16; ++i)
                                            dump += fmt::format("{:02x}", bytes[i]);
                                        const std::uint16_t track = *reinterpret_cast<const std::uint16_t*>(bytes + 0x32);
                                        const float weight = *reinterpret_cast<const float*>(bytes + 0x38);
                                        return fmt::format("{} +32={:#06x} +38={:.3f}", dump, track, weight);
                                    };
                                    // Audit 10 corrected the provider object: 63630 (graph
                                    // vtable slot 0xB) takes the driver at [graph+0xF0]
                                    // (characterInstance.ragdollDriver), returns its
                                    // hkaRagdollInstance from +0x88 (the 58286 getter), and
                                    // counts rigidBodies there — hkArray at instance+0x10:
                                    // {data +0x10, size +0x18, capacity +0x1C}. The v9-era
                                    // reads at driver+0x10/+0x18 hit the blend-out float and
                                    // the world-from-model mode bytes and proved nothing.
                                    const auto driver_state = [](RE::hkbRagdollDriver const* driver) {
                                        if (!driver)
                                            return std::string("null");
                                        const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(driver);
                                        const std::uintptr_t character = *reinterpret_cast<const std::uintptr_t*>(base + 0x80);
                                        const std::uintptr_t instance = *reinterpret_cast<const std::uintptr_t*>(base + 0x88);
                                        if (!instance)
                                            return fmt::format("{:#x} char={:#x} ragdoll=null", base, character);
                                        const std::uintptr_t bodies = *reinterpret_cast<const std::uintptr_t*>(instance + 0x10);
                                        const std::int32_t count = *reinterpret_cast<const std::int32_t*>(instance + 0x18);
                                        return fmt::format("{:#x} char={:#x} ragdoll={:#x} bodies={:#x} count={}", base, character, instance, bodies, count);
                                    };
                                    logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase=gates src-world={:#x} src-ragdoll={:#x} cand-world={:#x} cand-ragdoll={:#x} src-gen0[{}] cand-gen0[{}]",
                                        actor_id, request,
                                        address(source_graph.physicsWorld),
                                        address(source_graph.characterInstance.ragdollDriver.get()),
                                        address(candidate->physicsWorld),
                                        address(candidate->characterInstance.ragdollDriver.get()),
                                        element_head(source_graph.generatorOutputs[0]),
                                        element_head(candidate->generatorOutputs[0]));
                                    logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase=driver src[{}] cand[{}]",
                                        actor_id, request,
                                        driver_state(source_graph.characterInstance.ragdollDriver.get()),
                                        driver_state(candidate->characterInstance.ragdollDriver.get()));
                                }
                                char const* const inject_candidates[] = { "WeapEquip", "SprintStart", "FootLeft", "FootRight" };
                                std::int32_t chosen_id = -1;
                                char const* chosen_name = nullptr;
                                for (char const* const name : inject_candidates)
                                {
                                    const std::int32_t resolved = graph_data ? lookup_event_id_in_graph(*graph_data, name) : -1;
                                    logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase=event-lookup name='{}' id={} source=graph-events",
                                        actor_id, request, name, resolved);
                                    if (resolved >= 0 && !chosen_name)
                                    {
                                        chosen_id = resolved;
                                        chosen_name = name;
                                    }
                                }
                                if (chosen_name)
                                {
                                    struct InjectedEvent
                                    {
                                        std::uint32_t id;
                                        std::uint32_t pad;
                                        void* payload;
                                        void* sender;
                                    };
                                    InjectedEvent event{ static_cast<std::uint32_t>(chosen_id), 0, nullptr, nullptr };
                                    static REL::Relocation<event_dispatch_t> event_dispatch{ REL::ID(EventDispatchID) };
                                    event_dispatch(behavior, context, &event);
                                    logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase=inject event='{}' id={} result=dispatched",
                                        actor_id, request, chosen_name, chosen_id);
                                }
                                else
                                {
                                    logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase=skip reason=no-known-event",
                                        actor_id, request);
                                }
                            }
                            // v15: 63602's tail calls 63587(target, &record, 0) with the
                            // stack record layout below; the direct call mirrors it (audit
                            // 13, the 0xbb63b8 tail) in place of the bare slot-5 update.
                            static REL::Relocation<engine_update_wrapper_t> engine_update{ REL::ID(EngineUpdateWrapperID) };
                            // v16: 63564's convention carries one alive argument — the
                            // candidate graph (audit14_branch_63581.txt calls it with the
                            // exact rcx/rdx 63581 received and the callee never reads dx) —
                            // and its whole 0x61-byte body is read-only, so the direct call
                            // is bounded (R-01).
                            static REL::Relocation<pose_predicate_t> pose_predicate{ REL::ID(PosePredicateID) };
                            // v17: the per-step sampling goes through 63581 directly
                            // (audit 15 Q2's two-live-argument convention), bypassing the
                            // 63589 fork that routed every step to the 63568 branch.
                            static REL::Relocation<sampling_branch_t> sampling_branch{ REL::ID(SamplingBranchID) };
                            static REL::Relocation<frame_pipeline_t> frame_pipeline{ REL::ID(FramePipelineID) };
                            static REL::Relocation<graph_destructor_t> shell_destroy{ REL::ID(GraphDestructorID) };
                            // Stack storage for the per-pass scratch shell, exactly like the
                            // engine's slot-5 helper hosts its 63602 pass on a stack graph.
                            alignas(0x10) std::uint8_t shell_storage[sizeof(RE::BShkbAnimationGraph)]{};
                            // v10: borrow the source's physics world for the drive duration
                            // only — the gates row proved the candidate lacks exactly this
                            // one field (its ragdoll driver already exists) and slot 2's
                            // shared-world touch is just the [vtable 40] getter (a pure
                            // read) plus a release-disabled assertion stub; everything else
                            // in the unlocked body is candidate-local. Restored to null
                            // right after the loop.
                            // v11 dropped: the ragdoll-instance borrow proved non-load-bearing
                            // — the corrected driver row showed both sides own an 18-body
                            // instance, and borrowed or own the outputs stayed frozen.
                            // v12 (audit 11): slot 2 (63621) is itself the writer of the
                            // pose-track evidence fields ([tracks+0x32]=0, [tracks+0x38]=1.0f
                            // right after its three gates) and its body is
                            // BSIRagdollDriver::AddRagdollToWorld — per-body
                            // hkpEntity::Activate plus 64157's add-to-world. The engine
                            // reaches it inside 63602 only behind a gate whose provider
                            // (63643) fills a per-frame current-animations hash from the
                            // projectDBData pair (+0x170/+0x178 via slot 0xC), a path the
                            // candidate never satisfies — which is why +38 stayed 0.0
                            // through v10/v11 despite every gate of 63621 itself passing.
                            // Bypass the internal gate with one direct slot-2 call (same
                            // direct-call discipline as 58453/58377): candidate-own
                            // ragdoll instance, borrowed world, engine-native body. Since
                            // v17 the paired slot-3 removal follows immediately (see the
                            // slot-2 call below) so the sampling-time predicate reads
                            // false, and the post-loop call is an idempotent re-check.
                            const auto pose_track_fields = [](RE::BShkbAnimationGraph const* graph) {
                                const void* const output = graph->generatorOutputs[0];
                                if (!output)
                                    return std::string("no-output");
                                const auto* const tracks = *reinterpret_cast<void* const*>(output);
                                if (!tracks)
                                    return std::string("no-tracks");
                                const auto* const bytes = static_cast<const std::uint8_t*>(tracks);
                                const std::uint16_t num_data = *reinterpret_cast<const std::uint16_t*>(bytes + 0x32);
                                const float on_fraction = *reinterpret_cast<const float*>(bytes + 0x38);
                                return fmt::format("+32={:#06x} +38={:.3f}", num_data, on_fraction);
                            };
                            candidate->physicsWorld = source_graph.physicsWorld;
                            logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase=borrow world={:#x} ragdoll=own",
                                actor_id, request, address(source_graph.physicsWorld));
                            static REL::Relocation<ragdoll_world_op_t> add_ragdoll{ REL::ID(AddRagdollToWorldID) };
                            static REL::Relocation<ragdoll_world_op_t> remove_ragdoll{ REL::ID(RemoveRagdollFromWorldID) };
                            const std::string track_before = pose_track_fields(candidate.get());
                            const bool slot2_added = add_ragdoll(candidate.get());
                            // v17 (audit 15 Q1): the slot-3 removal follows slot 2
                            // immediately, before the drive loop — slot 2's per-body world
                            // registration is undone while the borrow is still in place, so
                            // `rigidBodies[0]->world` is null at every later sampling moment
                            // and the 63564 predicate returns false (v12 proved slot 3
                            // leaves the track headers intact). The post-loop call below
                            // stays as an idempotent re-check feeding the pipeline row.
                            const bool slot3_removed = remove_ragdoll(candidate.get());
                            logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase=slot2 added={} removed={} tracks-before[{}] tracks-after[{}]",
                                actor_id, request, slot2_added, slot3_removed, track_before, pose_track_fields(candidate.get()));
                            // v13 (audit 12): the per-step sampling entry 63589 gates on
                            // [graph+0x110] first; without it every step returned before
                            // reaching 63568, the kPose numData writer. Fill it with one
                            // direct slot-0xC call (dl=false picks +0x178; retry with true
                            // for the +0x170 member only if the first leaves +0x110 null),
                            // then the existing per-step sampling entry takes its real path.
                            // 63631 also AddReference's the picked object and touches the
                            // ragdoll skeleton — engine-native bookkeeping, called once.
                            const auto animation_data_state = [&]() {
                                const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(candidate_base);
                                const std::uintptr_t selected = *reinterpret_cast<const std::uintptr_t*>(base + 0x110);
                                const std::uintptr_t sub = selected ? *reinterpret_cast<const std::uintptr_t*>(selected + 0x20) : 0;
                                const unsigned branch = *reinterpret_cast<const std::uint8_t*>(base + 0x246);
                                return fmt::format("110={:#x} sel+20={:#x} 246={}", selected, sub, branch);
                            };
                            static REL::Relocation<animation_data_provider_t> fill_animation_data{ REL::ID(AnimationDataProviderID) };
                            const std::string data_before = animation_data_state();
                            fill_animation_data(candidate.get(), false);
                            if (!*reinterpret_cast<const std::uintptr_t*>(reinterpret_cast<std::uintptr_t>(candidate_base) + 0x110))
                                fill_animation_data(candidate.get(), true);
                            logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase=slot0c before[{}] after[{}]",
                                actor_id, request, data_before, animation_data_state());
                            auto* const shell = reinterpret_cast<RE::BShkbAnimationGraph*>(shell_storage);
                            // v15: the v14 post-pipeline1 fork evidence and the new
                            // post-update1 row share one emitter (same field set:
                            // 0x240/0x245/0x246/0x249 plus both double-buffer slots' track
                            // fields); the phase name selects the row.
                            const auto log_bookkeeping = [&](char const* phase) {
                                const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(candidate_base);
                                const auto output_tracks = [&](std::size_t slot_index) {
                                    const void* const output = candidate->generatorOutputs[slot_index];
                                    if (!output)
                                        return std::string("no-output");
                                    const auto* const tracks = *reinterpret_cast<void* const*>(output);
                                    if (!tracks)
                                        return std::string("no-tracks");
                                    const auto* const bytes = static_cast<const std::uint8_t*>(tracks);
                                    return fmt::format("+32={:#06x} +38={:.3f}",
                                        *reinterpret_cast<const std::uint16_t*>(bytes + 0x32),
                                        *reinterpret_cast<const float*>(bytes + 0x38));
                                };
                                logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase={} num-anim={:#x} w245={:#x} b246={} b249={} out0[{}] out1[{}]",
                                    actor_id, request, phase,
                                    *reinterpret_cast<const std::uint16_t*>(base + 0x240),
                                    *reinterpret_cast<const std::uint16_t*>(base + 0x245),
                                    *reinterpret_cast<const std::uint8_t*>(base + 0x246),
                                    *reinterpret_cast<const std::uint8_t*>(base + 0x249),
                                    output_tracks(0), output_tracks(1));
                            };
                            // v16 gate-G3 evidence, one read-only row before the drive loop:
                            // 58423's entry gates are byte [footIkDriver+0x4A] (rcx =
                            // [graph+0x100]), [[out]]+4 > 0xB and the context chain
                            // (0xab7f54..0xab7f89). v20a (audit24): the old row read
                            // `[ctx+0x50]` although `sizeof(RE::hkbContext)` is 0x50 - one
                            // slot past the object - and v20 crashed there; audit23's chain
                            // is [context] -> character, then setup (+0x50) -> data (+0x40)
                            // -> footIkDriverInfo (+0x88). Every hop is now a declared member
                            // and every intermediate is checked before it is dereferenced.
                            // No writes.
                            const std::uintptr_t graph_base = reinterpret_cast<std::uintptr_t>(candidate_base);
                            const std::uintptr_t gate_owner = *reinterpret_cast<const std::uintptr_t*>(graph_base + 0x100);
                            const auto readable = [](void const* const value) {
                                return plausible_object_pointer(reinterpret_cast<std::uintptr_t>(value));
                            };
                            const auto* const gate_driver = readable(reinterpret_cast<void const*>(gate_owner))
                                ? reinterpret_cast<const RE::hkbFootIkDriver*>(gate_owner)
                                : nullptr;
                            const std::uint8_t gate_flag = gate_driver ? gate_driver->disableFootIk : 0;
                            const RE::hkbCharacter* const gate_character =
                                readable(context) ? context->character : nullptr;
                            const RE::hkbCharacterSetup* const gate_setup =
                                readable(gate_character) ? gate_character->setup.get() : nullptr;
                            const RE::hkbCharacterData* const gate_data =
                                readable(gate_setup) ? gate_setup->data.get() : nullptr;
                            const RE::hkbFootIkDriverInfo* const gate_foot_ik_info =
                                readable(gate_data) ? gate_data->footIkDriverInfo.get() : nullptr;
                            const auto ptr_field = [](void const* const value) {
                                const auto address = reinterpret_cast<std::uintptr_t>(value);
                                return address ? fmt::format("{:#x}", address) : std::string("null");
                            };
                            logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase=g3-gates g100={} flag4a={:#04x} character={} setup={} data={} footik-info={}",
                                actor_id, request, ptr_field(reinterpret_cast<void const*>(gate_owner)), gate_flag,
                                ptr_field(gate_character), ptr_field(gate_setup), ptr_field(gate_data),
                                ptr_field(gate_foot_ik_info));
                            // v16 gate-G4 evidence: the per-slot pointer/numData/onFraction
                            // sequence over the double-buffer pair ([graph+0x220]/[0x228]).
                            // n/f are the read slot's first output track header (+0x32 word,
                            // +0x38 float), h is the 0x100-byte element-head FNV per slot;
                            // the sampling path reads [0x220] after each Path-A swap.
                            const auto log_slots = [&](std::uint32_t const step) {
                                const auto slot_fields = [&](std::size_t const slot_index) {
                                    const void* const output = candidate->generatorOutputs[slot_index];
                                    if (!output)
                                        return std::make_pair(std::string("null"), std::uint64_t{ 0 });
                                    const void* const element = *reinterpret_cast<void* const*>(output);
                                    if (!element)
                                        return std::make_pair(std::string("null"), std::uint64_t{ 0 });
                                    const auto* const bytes = static_cast<const std::uint8_t*>(element);
                                    return std::make_pair(fmt::format("{:#x} n={:#06x} f={:.3f}",
                                        reinterpret_cast<std::uintptr_t>(element),
                                        *reinterpret_cast<const std::uint16_t*>(bytes + 0x32),
                                        *reinterpret_cast<const float*>(bytes + 0x38)),
                                        fnv1a_64(element, 0x100));
                                };
                                const auto [ptr0, hash0] = slot_fields(0);
                                const auto [ptr1, hash1] = slot_fields(1);
                                logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase=slots step={} p0={} h0={:016x} p1={} h1={:016x}",
                                    actor_id, request, step, ptr0, hash0, ptr1, hash1);
                            };
                            // v21 pose-content contract (2026-10-27): a raw FNV over
                            // poseLocal is padding-sensitive, so it proves bytes changed, not
                            // that a bone carries a finite, changing transform. At three
                            // checkpoints this decodes every entry through the declared
                            // hkQsTransform (0x30 stride), compares each entry with the
                            // previous checkpoint, compares with the source's own poseLocal and
                            // counts candidate/source identical entries (aliasing watch). The
                            // pure arithmetic lives in pose_content_policy.h and is covered
                            // engine-free. Read-only: no native call and no buffer write.
                            std::vector<PoseSample> previous_candidate_pose;
                            std::vector<PoseSample> previous_source_pose;
                            // v22: the step-0 candidate pose, so "moving" can be decided
                            // against the bind baseline instead of the previous checkpoint.
                            std::vector<PoseSample> baseline_candidate_pose;
                            const auto read_pose_local = [](RE::hkbCharacter const& character, std::vector<PoseSample>& out) {
                                out.clear();
                                if (!character.poseLocal || character.numPoseLocal <= 0)
                                    return;
                                const std::size_t count = static_cast<std::size_t>(character.numPoseLocal);
                                const std::size_t limit = count < kMaxPoseEntries ? count : kMaxPoseEntries;
                                out.reserve(limit);
                                for (std::size_t i = 0; i < limit; ++i)
                                {
                                    out.push_back(pose_sample_from_transform(character.poseLocal[i]));
                                }
                            };
                            const auto or_missing = [](bool const known, std::string const& value) {
                                return known ? value : std::string("n/a");
                            };
                            const auto log_pose_content = [&](std::uint32_t const step) {
                                std::vector<PoseSample> current_candidate;
                                std::vector<PoseSample> current_source;
                                read_pose_local(candidate->characterInstance, current_candidate);
                                read_pose_local(source_base->characterInstance, current_source);
                                const PoseComparison comparison = compare_pose(current_candidate, previous_candidate_pose);
                                const PoseComparison source_comparison = compare_pose(current_source, previous_source_pose);
                                // v22: aliasing is decided by buffer identity plus "equal and
                                // still moving"; the raw equal count stays a statistic only.
                                const bool buffer_alias = pose_buffer_alias(candidate->characterInstance.poseLocal,
                                    source_base->characterInstance.poseLocal);
                                const PoseAliasWatch watch =
                                    watch_aliasing(current_candidate, current_source, baseline_candidate_pose, buffer_alias);
                                logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase=pose-content step={} entries={} finite={} nonfinite={} moved={} max-delta={} source-entries={} source-finite={} source-moved={} source-equal={} source-equal-moving={} source-equal-static={} alias={}",
                                    actor_id, request, step, comparison.entries, comparison.finite, comparison.non_finite,
                                    or_missing(comparison.has_previous, fmt::format("{}", comparison.moved)),
                                    or_missing(comparison.has_previous, fmt::format("{:.6f}", comparison.max_delta)),
                                    source_comparison.entries, source_comparison.finite,
                                    or_missing(source_comparison.has_previous, fmt::format("{}", source_comparison.moved)),
                                    watch.equal,
                                    or_missing(watch.has_baseline, fmt::format("{}", watch.equal_moving)),
                                    or_missing(watch.has_baseline, fmt::format("{}", watch.equal_static)),
                                    watch.buffer_alias ? "yes" : "no");
                                const RE::hkaSkeleton* const skeleton =
                                    candidate->characterInstance.setup ? candidate->characterInstance.setup->animationSkeleton.get() : nullptr;
                                const std::size_t bone_count = skeleton ? static_cast<std::size_t>(skeleton->bones.size()) : 0;
                                for (std::size_t slot = 0; slot < 4 && bone_count > 0; ++slot)
                                {
                                    const std::size_t index = (slot * (bone_count - 1)) / 3;
                                    if (index >= current_candidate.size() || index >= current_source.size())
                                        continue;
                                    const PoseSample& sample = current_candidate[index];
                                    const std::string name = read_bone_name(skeleton->bones.data()[index].name);
                                    const std::string moved = index < previous_candidate_pose.size()
                                        ? (pose_sample_differs(sample, previous_candidate_pose[index]) ? "yes" : "no")
                                        : "n/a";
                                    const bool matches_source = pose_sample_identical(sample, current_source[index]);
                                    logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase=pose-bone step={} index={} name='{}' p=({:.4f},{:.4f},{:.4f}) q=({:.4f},{:.4f},{:.4f},{:.4f}) s=({:.3f},{:.3f},{:.3f}) finite={} moved={} source-equal={} source-p=({:.4f},{:.4f},{:.4f})",
                                        actor_id, request, step, index, name,
                                        sample.translation[0], sample.translation[1], sample.translation[2],
                                        sample.rotation[0], sample.rotation[1], sample.rotation[2], sample.rotation[3],
                                        sample.scale[0], sample.scale[1], sample.scale[2],
                                        pose_sample_finite(sample), moved, matches_source,
                                        current_source[index].translation[0], current_source[index].translation[1],
                                        current_source[index].translation[2]);
                                }
                                if (step == 0)
                                    baseline_candidate_pose = current_candidate;
                                previous_candidate_pose = current_candidate;
                                previous_source_pose = current_source;
                            };
                            // v23 semantic correspondence (read-only): is poseLocal the sample
                            // of an active clip at that clip's own time? The active node list is
                            // walked without writing anything; a node is only treated as a clip
                            // generator when its vtable matches REL's hkbClipGenerator. The single
                            // native call is SamplePartialTracks - the same API the display player
                            // already uses every frame - and it samples a copy, not the live clip.
                            // v24: one scan shared by the comparison and the clock rows. A node
                            // counts as a clip generator only when its vtable matches REL's
                            // hkbClipGenerator; the walk is bounded at 256 active entries and at
                            // the caller's slot limit so a malformed list cannot drive it.
                            const auto collect_active_clips = [&](std::size_t const limit) {
                                std::vector<std::pair<std::size_t, const RE::hkbClipGenerator*>> found;
                                RE::hkbBehaviorGraph* const graph = candidate->characterInstance.behaviorGraph.get();
                                if (!graph || !graph->activeNodes)
                                    return found;
                                RE::NodeList const& nodes = *graph->activeNodes;
                                const std::size_t active = static_cast<std::size_t>(nodes.size());
                                const std::size_t count = active < 256 ? active : 256;
                                for (std::size_t i = 0; i < count && found.size() < limit; ++i)
                                {
                                    RE::hkbNode* const node = nodes.data()[i].nodeClone;
                                    if (!node_is_clip_generator(node))
                                        continue;
                                    found.emplace_back(i, static_cast<const RE::hkbClipGenerator*>(node));
                                }
                                return found;
                            };
                            // v23/v24 semantic correspondence (read-only): is poseLocal the sample
                            // of an active clip at that clip's own time? The only native call is
                            // SamplePartialTracks - const, the same API the display player uses
                            // every frame - and it writes a local copy, never the live clip.
                            const auto log_clip_reference = [&](std::uint32_t const step) {
                                std::vector<PoseSample> pose;
                                read_pose_local(candidate->characterInstance, pose);
                                if (pose.empty())
                                {
                                    logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase=clip-ref step={} status=unavailable reason=no-pose",
                                        actor_id, request, step);
                                    return;
                                }
                                const auto clips = collect_active_clips(8);
                                if (clips.empty())
                                {
                                    logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase=clip-ref step={} status=unavailable reason=no-clip-generator",
                                        actor_id, request, step);
                                    return;
                                }
                                std::size_t slot = 0;
                                for (auto const& [index, clip] : clips)
                                {
                                    // Same rule as the v20a chain: never dereference an
                                    // intermediate that has not been filtered first.
                                    const RE::hkaAnimationBinding* const binding =
                                        plausible_object_pointer(reinterpret_cast<std::uintptr_t>(clip->binding))
                                        ? clip->binding
                                        : nullptr;
                                    const RE::hkaAnimation* const animation = binding ? binding->animation.get() : nullptr;
                                    const bool animation_plausible = animation != nullptr &&
                                        plausible_object_pointer(reinterpret_cast<std::uintptr_t>(animation));
                                    const std::int32_t tracks = animation_plausible ? animation->numberOfTransformTracks : 0;
                                    const float local_time = clip->localTime;
                                    const std::string name = read_string(clip->animationName, 96);
                                    if (!binding || !animation_plausible || tracks <= 0 || tracks > 512 || !std::isfinite(local_time))
                                    {
                                        logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase=clip-ref step={} slot={} index={} status=unavailable reason=bad-binding name='{}' tracks={} time={:.4f}",
                                            actor_id, request, step, slot, index, name, tracks, local_time);
                                        ++slot;
                                        continue;
                                    }
                                    std::vector<RE::hkQsTransform> sampled(static_cast<std::size_t>(tracks));
                                    animation->SamplePartialTracks(local_time, static_cast<std::uint32_t>(tracks),
                                        sampled.data(), 0, nullptr, nullptr);
                                    // audit25: the engine's own ConvertTrackPoseToBonePose
                                    // (REL 60005, 0xB03380) sets bone = track when the map is
                                    // empty, so the identity reading here is the engine's.
                                    const auto& map = binding->transformTrackToBoneIndices;
                                    const bool identity = map.empty();
                                    const std::size_t mapped = identity
                                        ? static_cast<std::size_t>(tracks)
                                        : std::min<std::size_t>(static_cast<std::size_t>(tracks), static_cast<std::size_t>(map.size()));
                                    ClipReferenceMatch match;
                                    for (std::size_t track = 0; track < mapped; ++track)
                                    {
                                        const std::int32_t bone = identity ? static_cast<std::int32_t>(track) : map.data()[track];
                                        if (bone < 0 || static_cast<std::size_t>(bone) >= pose.size())
                                        {
                                            ++match.unmapped;
                                            continue;
                                        }
                                        clip_reference_agrees(match, pose_sample_from_transform(sampled[track]),
                                            pose[static_cast<std::size_t>(bone)]);
                                    }
                                    // v25: audit26 showed the advance is
                                    // dt x ((effDuration/enforcedDuration or 1) x playbackSpeed),
                                    // so a constant multiplier must be ruled out by reading the
                                    // enforced duration and the crop amounts, not by assuming 1.0.
                                    const RE::hkaDefaultAnimationControl* const control =
                                        plausible_object_pointer(reinterpret_cast<std::uintptr_t>(clip->animationControl.get()))
                                        ? clip->animationControl.get()
                                        : nullptr;
                                    const float crop_sum = control
                                        ? control->cropStartAmountLocalTime + control->cropEndAmountLocalTime
                                        : 0.0f;
                                    const float effective_duration = control
                                        ? std::max(animation->duration - crop_sum, 0.0f)
                                        : animation->duration;
                                    logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase=clip-ref step={} slot={} index={} status=compared name='{}' binding-index={} map-len={} mapping={} time={:.4f} duration={:.4f} enforced={:.4f} crop-sum={:.4f} eff-duration={:.4f} mode={} speed={:.3f} tracks={} mapped={} compared={} agreed={} unmapped={} max-dpos={:.6f} max-drot={:.6f} max-dscale={:.6f}",
                                        actor_id, request, step, slot, index, name,
                                        static_cast<std::uint32_t>(clip->animationBindingIndex),
                                        static_cast<std::uint32_t>(map.size()),
                                        identity ? "identity" : "table",
                                        local_time, animation->duration, clip->enforcedDuration, crop_sum,
                                        effective_duration,
                                        static_cast<std::uint32_t>(clip->mode.get()), clip->playbackSpeed,
                                        tracks, mapped, match.compared, match.agreed, match.unmapped,
                                        match.max_position, match.max_rotation, match.max_scale);
                                    ++slot;
                                }
                            };
                            // v24 clock row: every active clip's own localTime on one line, so the
                            // clip clock can be compared with the drive budget instead of inferred.
                            const auto log_clip_clock = [&](std::uint32_t const step) {
                                const auto clips = collect_active_clips(8);
                                if (clips.empty())
                                {
                                    logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase=clip-clock step={} status=unavailable",
                                        actor_id, request, step);
                                    return;
                                }
                                std::string entries;
                                for (auto const& [index, clip] : clips)
                                {
                                    const std::string full = read_string(clip->animationName, 96);
                                    const std::size_t slash = full.find_last_of("\\/");
                                    const std::string leaf = slash == std::string::npos ? full : full.substr(slash + 1);
                                    if (!entries.empty())
                                        entries.push_back('|');
                                    entries += fmt::format("{}:{:.4f}", leaf, clip->localTime);
                                }
                                logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase=clip-clock step={} found={} entries='{}'",
                                    actor_id, request, step, clips.size(), entries);
                            };
                            // v25: one character per drive step for each clip's localTime delta.
                            // audit26 established that the clock consumes dt and nothing else, so
                            // an uneven advance can only come from how often that Update is
                            // reached; the bitstring shows the pattern without 121 log rows.
                            // Code: '.' = 0 frames, '1'..'9' = frames, 'A'..'Z' = 10..35 frames,
                            // 'N' = time went backwards (wrap or restart), '-' = clip absent.
                            struct ClipStepTrace
                            {
                                std::string name;
                                std::string deltas;
                                std::uint32_t first_step{};
                                float last_time{};
                            };
                            std::vector<ClipStepTrace> clip_step_traces;
                            const auto log_clip_step = [&](std::uint32_t const step) {
                                const auto clips = collect_active_clips(8);
                                std::vector<bool> seen(clip_step_traces.size(), false);
                                for (auto const& [index, clip] : clips)
                                {
                                    const std::string full = read_string(clip->animationName, 96);
                                    const std::size_t slash = full.find_last_of("\\/");
                                    const std::string leaf = slash == std::string::npos ? full : full.substr(slash + 1);
                                    std::size_t at = 0;
                                    for (; at < clip_step_traces.size(); ++at)
                                    {
                                        if (clip_step_traces[at].name == leaf)
                                            break;
                                    }
                                    if (at == clip_step_traces.size())
                                    {
                                        ClipStepTrace fresh;
                                        fresh.name = leaf;
                                        fresh.first_step = step;
                                        fresh.last_time = clip->localTime;
                                        fresh.deltas.push_back('.');
                                        clip_step_traces.push_back(fresh);
                                        seen.push_back(true);
                                        continue;
                                    }
                                    seen[at] = true;
                                    ClipStepTrace& trace = clip_step_traces[at];
                                    const float now = clip->localTime;
                                    const long frames = std::lround((now - trace.last_time) * 60.0f);
                                    trace.last_time = now;
                                    char code = '.';
                                    if (frames < 0)
                                        code = 'N';
                                    else if (frames > 35)
                                        code = '?';
                                    else if (frames > 9)
                                        code = static_cast<char>('A' + (frames - 10));
                                    else if (frames > 0)
                                        code = static_cast<char>('0' + frames);
                                    trace.deltas.push_back(code);
                                }
                                for (std::size_t i = 0; i < clip_step_traces.size(); ++i)
                                {
                                    if (!seen[i])
                                        clip_step_traces[i].deltas.push_back('-');
                                }
                            };
                            const auto emit_clip_steps = [&] {
                                for (auto const& trace : clip_step_traces)
                                {
                                    logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase=clip-steps name='{}' first={} steps={} deltas='{}'",
                                        actor_id, request, trace.name, trace.first_step, trace.deltas.size(), trace.deltas);
                                }
                            };
                            // v26: does poseLocal follow the clock every step, or only at the batch
                            // boundary v25 found? One character per drive step, bucketed by the
                            // largest single-component change since the previous step:
                            // '.' = 0, '1' <= 1e-5, '2' <= 1e-4, '3' <= 1e-3, '4' <= 1e-2, '5' > 1e-2.
                            std::vector<PoseSample> pose_step_previous;
                            std::string pose_step_deltas;
                            std::size_t pose_step_changed = 0;
                            std::uint32_t pose_step_first = 0;
                            bool pose_step_started = false;
                            float pose_step_max = 0.0f;
                            const auto log_pose_step = [&](std::uint32_t const step) {
                                std::vector<PoseSample> current;
                                read_pose_local(candidate->characterInstance, current);
                                if (current.empty())
                                    return;
                                if (!pose_step_started)
                                {
                                    pose_step_started = true;
                                    pose_step_first = step;
                                    pose_step_previous = std::move(current);
                                    pose_step_deltas.push_back('.');
                                    return;
                                }
                                float worst = 0.0f;
                                const std::size_t limit = std::min(current.size(), pose_step_previous.size());
                                for (std::size_t i = 0; i < limit; ++i)
                                    worst = std::max(worst, pose_sample_max_delta(current[i], pose_step_previous[i]));
                                pose_step_max = std::max(pose_step_max, worst);
                                char code = '.';
                                if (worst > 1.0e-2f)
                                    code = '5';
                                else if (worst > 1.0e-3f)
                                    code = '4';
                                else if (worst > 1.0e-4f)
                                    code = '3';
                                else if (worst > 1.0e-5f)
                                    code = '2';
                                else if (worst > 0.0f)
                                    code = '1';
                                if (code != '.')
                                    ++pose_step_changed;
                                pose_step_deltas.push_back(code);
                                pose_step_previous = std::move(current);
                            };
                            const auto emit_pose_steps = [&] {
                                if (!pose_step_started)
                                    return;
                                logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase=pose-steps first={} steps={} changed={} max-delta={:.6f} deltas='{}'",
                                    actor_id, request, pose_step_first, pose_step_deltas.size(), pose_step_changed,
                                    pose_step_max, pose_step_deltas);
                            };
                            // v19 contract: the inject recipe supplies the audited
                            // optionalEyePosition (record+0x18) so 63587's arm block can run.
                            // The point is the candidate root's position shifted along Z past
                            // the arm block's 800-unit cap; a null root keeps a non-null zero
                            // point (the arm step is then fixed and the content is unread).
                            // Other modes keep the v18 record (null pointer) unchanged.
                            const bool eye_arm = mode == IndependentGraphMode::inject && version == SKSE::RUNTIME_SSE_1_6_1170;
                            std::optional<std::array<float, 3>> root_position;
                            if (eye_arm)
                            {
                                if (std::uintptr_t const root_node = *reinterpret_cast<const std::uintptr_t*>(graph_base + 0x218))
                                {
                                    const auto* const position = reinterpret_cast<const float*>(root_node + 0xA0);
                                    root_position = std::array<float, 3>{ position[0], position[1], position[2] };
                                }
                            }
                            EyePoint eye = make_eye_point(root_position);
                            FrameTrace frame_trace(mode == IndependentGraphMode::inject && version == SKSE::RUNTIME_SSE_1_6_1170,
                                actor_id, request, candidate->characterInstance, eye_arm ? &eye : nullptr);
                            std::uint8_t const generation_mode = 0;
                            for (std::uint32_t step = 0; behavior && step < DriveSteps; ++step)
                            {
                                // The existing scratch helper pair stays unchanged.
                                // Audit22 tests generation through the later outer entry instead.
                                frame_trace.phase(*candidate_base, step, FrameStage::e_pre_pipeline);
                                construct(shell, nullptr, false);
                                frame_pipeline(shell, candidate_base);
                                shell_destroy(shell);
                                frame_trace.phase(*candidate_base, step, FrameStage::e_post_pipeline);
                                if (step == 0)
                                {
                                    // v14 (audit 13): read-only fork evidence — did 63602's
                                    // internal 63587 tail run on the candidate? Its prologue
                                    // writes word 0x245 = 0x101 (245=1, 246=1) and Path B
                                    // recomputes numAnimBones and resets 246; the outputs are
                                    // double-buffered (+0x220/+0x228 swapped by 63587), so
                                    // both slots' track fields are logged. (Adjudicated ②:
                                    // all zero, slots unchanged → 63587 never reached the
                                    // candidate, which is why v15 calls it directly.)
                                    log_bookkeeping("post-pipeline1");
                                }
                                // The engine requests per-frame children re-activation by
                                // setting graph+0x12E = 1 before every update (63602's tail
                                // does exactly this for each subscribed graph, and its own
                                // zero-dt update consumed the flag again); the update's
                                // internal gate skips its 60079 activation walk otherwise.
                                *reinterpret_cast<std::uint8_t*>(address(behavior) + 0x12E) = 1;
                                // v15: replace the bare 58329 update with the ENGINE's
                                // per-frame wrapper 63587 — 63602's tail calls it exactly
                                // like this (target graph, stack record, third argument 0)
                                // and the v14 run proved the candidate never receives that
                                // call. The record is 63602's tail layout (audit 13):
                                // ≥0x30 bytes, zero-initialized, float dt at +0x00 and the
                                // byte modes +0x2a..+0x2e = 1,1,1,0,1 (footIK at +0x2e);
                                // the engine hosts each record on the stack per call, so it
                                // is rebuilt fresh every step, with the drive's fixed 1/60 s
                                // step as dt instead of 63602's zero.
                                // This audited manager byte map is preserved from v17.
                                // Path B also depends on t0/t1; v17 observed Path A despite +0x2A=0.
                                alignas(8) std::uint8_t update_record[0x30]{};
                                *reinterpret_cast<float*>(update_record) = DriveTimestep;
                                update_record[0x2A] = 0;
                                update_record[0x2B] = 1;
                                update_record[0x2C] = 1;
                                update_record[0x2E] = 1;
                                // v19 only: the audited BSAnimationUpdateData::optionalEyePosition
                                // slot; every other mode keeps the v18 null record field.
                                if (eye_arm)
                                    *reinterpret_cast<void**>(update_record + 0x18) = &eye;
                                engine_update(candidate_base, update_record, 0);
                                frame_trace.phase(*candidate_base, step, FrameStage::e_post_update);
                                if (step == 1 || step == 2)
                                    log_slots(step);
                                if (step == 0)
                                {
                                    // v15: the same field set immediately after the first
                                    // direct call — any 63587 bookkeeping (w245 ≠ 0, b246=1,
                                    // num-anim recomputed, slots swapped vs post-pipeline1)
                                    // proves the wrapper reached the candidate; all zero
                                    // means its prologue gates (+0x110 / [[+0x110]+0x20] /
                                    // +0x208 / [[+0x208]+0x12c]) blocked — pre-registered ③,
                                    // next audit = the +0x208 gate chain.
                                    log_bookkeeping("post-update1");
                                }
                                // v17: the mirror that feeds the sampler. The engine's
                                // per-frame 63587 writes word [graph+0x240] from the
                                // manager record via 63575 (Path B recomputes it); on the
                                // recordless candidate the drive mirrors numPoseLocal
                                // (0x74 for the same skeleton) into it each step, exactly
                                // as since v7, and reads it back as the direct 63581
                                // call's second argument (audit 15 Q2: edx = word
                                // [graph+0x240]).
                                *reinterpret_cast<std::uint16_t*>(reinterpret_cast<std::uintptr_t>(candidate_base) + 0x240) =
                                    static_cast<std::uint16_t>(candidate->characterInstance.numPoseLocal);
                                // v16: direct 63564 predicate observation at steps 0/60,
                                // immediately before the sampling block — the same call
                                // 63581 makes first thing inside the block (same rcx the
                                // sampler receives, rdx provably dead); a nonzero return
                                // means 63581 skips the whole block at 0xbaff4d. The
                                // read-only body keeps the call observation-only.
                                if (step == 0 || step == 60)
                                {
                                    const std::uint8_t predicate = pose_predicate(candidate_base);
                                    logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase=pred step={} r={:#04x}",
                                        actor_id, request, step, predicate);
                                    log_slots(step);
                                    log_pose_content(step);
                                    log_clip_reference(step);
                                }
                                if (step % 20 == 0)
                                    log_clip_clock(step);
                                // V20 uses the audited outer wrapper; other profiles retain 63581.
                                // The mode byte lives across this synchronous call.
                                frame_trace.phase(*candidate_base, step, FrameStage::e_pre_sample);
                                if (generation_profile)
                                {
                                    static REL::Relocation<generation_entry_t> generation_entry{ REL::ID(GenerationEntryID) };
                                    generation_entry(candidate_base, &generation_mode);
                                }
                                else
                                {
                                    sampling_branch(candidate_base,
                                        *reinterpret_cast<const std::uint16_t*>(reinterpret_cast<std::uintptr_t>(candidate_base) + 0x240));
                                }
                                frame_trace.sampled_pose(candidate->characterInstance);
                                frame_trace.phase(*candidate_base, step, FrameStage::e_post_sample);
                                if (step + 1 == DriveSteps / 2)
                                {
                                    candidate_pose_mid = format_pose_hash(candidate->characterInstance);
                                    log_state_probe("60");
                                }
                                log_clip_step(step);
                                log_pose_step(step);
                            }
                            log_clip_step(120);
                            log_pose_step(120);
                            emit_clip_steps();
                            emit_pose_steps();
                            // v17: idempotent slot-3 re-check after the drive — the world
                            // borrow is released right below, so nothing re-registers the
                            // bodies afterwards; the call feeds the pipeline row only.
                            const bool slot2_removed = remove_ragdoll(candidate.get());
                            candidate->physicsWorld = nullptr;
                            candidate_pose_end = format_pose_hash(candidate->characterInstance);
                            log_state_probe("120");
                            log_slots(120);
                            log_pose_content(120);
                            log_clip_reference(120);
                            log_clip_clock(120);
                            logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase=pipeline steps={} shell=pairs world-restored=true ragdoll-removed={} tracks-end[{}]",
                                actor_id, request, DriveSteps, slot2_removed, pose_track_fields(candidate.get()));

                            const std::string source_pose_after = format_pose_hash(source_graph.characterInstance);
                            logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase=drive-done steps={} dt={:.6f} source-pose-before={} source-pose-after={} candidate-pose-0={} candidate-pose-60={} candidate-pose-120={}",
                                actor_id, request, DriveSteps, DriveTimestep, source_pose_before, source_pose_after,
                                candidate_pose_before, candidate_pose_mid, candidate_pose_end);
                            log_drive_state(actor_id, request, "post-drive", activated,
                                first_drift(source_before, identify(source_graph)), observe_drive(*candidate));
                            frame_trace.summary();
                        }
                    }
                }
            }
            else
                candidate_final = constructed_identity;
        }
    }

    const bool retained_loaded = mode != IndependentGraphMode::off && mode != IndependentGraphMode::construct && loaded;
    // v27: the retained candidate is the only handle the display path can reach later, so its
    // character is remembered here. Nothing is stepped from this pointer here.
    if (retained_loaded)
    {
        g_held_character = retained_character;
        g_held_graph = retained_graph;
    }
    const Identity source_after = identify(source_graph);
    logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase={} source-drift={} registry-probe=unavailable",
        actor_id, request, retained_loaded ? "retained" : "destroyed", first_drift(source_before, source_after));
    observe_source_baseline(source_graph, actor_id, request, source_generation);
    logger::info("INDEPENDENT GRAPH actor={:08x} request={} phase=end constructed={} loaded={} assembly={} separated={} source-stable={} retained={}",
        actor_id, request, constructed, loaded, assembled(candidate_final), separated(source_before, candidate_final),
        std::string_view(first_drift(source_before, source_after)) == "none", retained_loaded);
}

bool held_display_pose(std::vector<std::pair<std::string, RE::NiTransform>>& out)
{
    out.clear();
    if (!g_held_character || !g_held_character->poseLocal || g_held_character->numPoseLocal <= 0)
        return false;
    const RE::hkaSkeleton* const skeleton =
        g_held_character->setup ? g_held_character->setup->animationSkeleton.get() : nullptr;
    if (!skeleton || skeleton->bones.empty())
        return false;
    const std::size_t bones = static_cast<std::size_t>(skeleton->bones.size());
    const std::size_t entries = std::min<std::size_t>(static_cast<std::size_t>(g_held_character->numPoseLocal), bones);
    out.reserve(entries);
    for (std::size_t i = 0; i < entries; ++i)
    {
        const char* const raw = skeleton->bones.data()[i].name.c_str();
        if (!raw || raw[0] == '\0')
            continue;
        out.emplace_back(std::string(raw), ni_local_from_pose(read_pose(g_held_character->poseLocal[i])));
    }
    return !out.empty();
}

bool step_display_pose(std::vector<std::pair<std::string, RE::NiTransform>>& out)
{
    out.clear();
    if (!g_held_graph || !g_held_character)
        return false;
    // v29: the batch runs the engine's per-frame pipeline (63555 shell -> 63602 -> 63556) before
    // each generation entry. v28 called only 63589, and the three identical pose hashes showed the
    // entry does not advance anything on its own; this reproduces the batch's per-step core.
    static std::uint8_t mode = 0;
    static REL::Relocation<constructor_t> construct{ REL::ID(GraphConstructorID) };
    static REL::Relocation<frame_pipeline_t> frame_pipeline{ REL::ID(FramePipelineID) };
    static REL::Relocation<graph_destructor_t> shell_destroy{ REL::ID(GraphDestructorID) };
    static REL::Relocation<generation_entry_t> entry{ REL::ID(GenerationEntryID) };
    alignas(0x10) std::uint8_t shell_storage[sizeof(RE::BShkbAnimationGraph)]{};
    auto* const shell = reinterpret_cast<RE::BShkbAnimationGraph*>(shell_storage);
    construct(shell, nullptr, false);
    frame_pipeline(shell, g_held_graph);
    shell_destroy(shell);
    entry(g_held_graph, &mode);
    static std::uint64_t calls = 0;
    ++calls;
    const bool converted = held_display_pose(out);
    if (calls == 1 || calls == 60 || calls == 120)
    {
        std::uint64_t hash = 1469598103934665603ull;
        if (g_held_character->poseLocal && g_held_character->numPoseLocal > 0)
        {
            const auto* const bytes = reinterpret_cast<const std::uint8_t*>(g_held_character->poseLocal);
            const std::size_t count = static_cast<std::size_t>(g_held_character->numPoseLocal) * 0x30;
            for (std::size_t i = 0; i < count; ++i)
            {
                hash ^= bytes[i];
                hash *= 1099511628211ull;
            }
        }
        logger::info("ANIMATION DISPLAY step={} pose-hash={:016x} bones={}", calls, hash, out.size());
        // v30: the pose hash never moved (v28 and v29 both reported 7d568c4d...), so the calls
        // are reaching the graph but changing nothing. These are the gates 63587/63589 read
        // (audit16/audit18) plus the two output slots' numData, which says whether the frame
        // pipeline is even eligible to run here.
        const auto* const fields = reinterpret_cast<const std::uint8_t*>(g_held_graph);
        const auto slot_num_data = [](std::uintptr_t const slot) -> int
        {
            if (!plausible_object_pointer(slot))
                return -1;
            return static_cast<int>(*reinterpret_cast<const std::uint16_t*>(slot + 0x32));
        };
        const std::uintptr_t slot0 = *reinterpret_cast<const std::uintptr_t*>(fields + 0x220);
        const std::uintptr_t slot1 = *reinterpret_cast<const std::uintptr_t*>(fields + 0x228);
        logger::info("ANIMATION DISPLAY gates call={} active={} children={} b245={:#04x} b246={} w240={} c110={:#x} c208={:#x} numData0={} numData1={}",
            calls, static_cast<int>(fields[0x12C]), static_cast<int>(fields[0x12E]),
            static_cast<int>(fields[0x245]), static_cast<int>(fields[0x246]),
            static_cast<int>(*reinterpret_cast<const std::uint16_t*>(fields + 0x240)),
            *reinterpret_cast<const std::uintptr_t*>(fields + 0x110),
            *reinterpret_cast<const std::uintptr_t*>(fields + 0x208),
            slot_num_data(slot0), slot_num_data(slot1));
    }
    return converted;
}
}
PLUGIN_NAMESPACE_END
