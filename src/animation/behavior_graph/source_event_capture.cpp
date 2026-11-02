//
// Created by AmazingBuff on 2026/10/31.
//

#include "animation/behavior_graph/source_event_capture.h"

#include "animation/behavior_graph/source_event_capture_policy.h"

#include "animation/settings.h"

#include <RE/A/Actor.h>
#include <RE/B/BSAnimationGraphManager.h>
#include <RE/B/BShkbAnimationGraph.h>
#include <RE/H/hkbBehaviorGraph.h>
#include <RE/H/hkbBehaviorGraphData.h>
#include <RE/H/hkbBehaviorGraphStringData.h>
#include <RE/H/hkbVariableValueSet.h>
#include <REL/Relocation.h>
#include <SKSE/SKSE.h>
#include <SKSE/Trampoline.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    namespace
    {
        // The phase stream mirrors the G3-A settled rules on the five equipment-chain variables
        // plus the node count, so both streams can be aligned against one F7 window.
        constexpr std::size_t kMaxWords = 1024;
        constexpr std::size_t kWordDataOffset = 0x10;
        constexpr std::size_t kWordCountOffset = 0x18;
        constexpr std::size_t kNodeCountOffset = 0x8;
        constexpr std::uint8_t kSettleSamples = 3;
        constexpr std::size_t kPhaseCount = 5;
        char const* const kPhaseNames[kPhaseCount] = {
            "currentDefaultState", "iState", "IsEquipping", "IsUnequipping", "bEquipUnequip"
        };

        std::atomic<bool> g_installed{ false };
        // The gate the detour reads on every dispatch in the process: non-null exactly while a
        // window is open, holding the source behaviour pointer the filter compares against.
        std::atomic<void const*> g_gate_behavior{ nullptr };
        // C-G4-C1 liveness counters, read by the window-close row. `calls` counts every entry to
        // the detour, before the gate, so the plugin's own direct 58377 dispatches prove the hook
        // is live and reached; `other_graph` counts armed-window dispatches aimed at a different
        // behaviour graph (the candidate's injected events land there). Neither is allowed to
        // gate the ring push: only source-graph dispatches become event rows.
        std::atomic<std::uint64_t> g_calls{ 0 };
        std::atomic<std::uint64_t> g_other_graph{ 0 };

        SourceEventRing& event_ring()
        {
            static SourceEventRing s_ring;
            return s_ring;
        }

        SourceTimelineRequests& capture_requests()
        {
            static SourceTimelineRequests s_requests;
            return s_requests;
        }

        struct CaptureState
        {
            std::size_t index[kPhaseCount] = {};
            bool tracked[kPhaseCount] = {};
            std::int32_t logged[kPhaseCount] = {};
            std::int32_t candidate[kPhaseCount] = {};
            std::uint8_t settled[kPhaseCount] = {};
            std::int32_t node_logged = -1;
            std::int32_t node_candidate = -1;
            std::uint8_t node_settled = 0;
            std::uint64_t seq = 0;
            std::uint64_t dropped_seen = 0;
            std::uint32_t actor_id = 0;
            // C-G4-C1 (v66): pinned observation target. In this mod list the sampler's
            // player_graph() picks a different graph object than the one the candidate descends
            // from (words 463 / nodes 36 vs 652 / 106), so a window armed there cannot answer the
            // leak question. Concurrent windows pin the experiment's graph instead; membership in
            // the actor's manager list is re-checked every frame, so a replaced graph is detected
            // without ever dereferencing the stored pointer.
            RE::BShkbAnimationGraph const* concurrent_graph = nullptr;
            // Identity belongs to the window: re-arming must re-baseline (and re-arm the hook
            // gate) even when the resolved source happens to be pointer-identical.
            SourceTimelineIdentity observed{};
            bool has_observed{ false };
            SourceEventWindow window;
        };

        CaptureState& capture_state()
        {
            static CaptureState s_state;
            return s_state;
        }

        // ---- hook side ----------------------------------------------------------

        // Called for every event dispatch the process makes to any behaviour graph. One relaxed
        // gate load misses immediately outside a window; inside, the record is two flag reads
        // and a ring push. Nothing here may allocate, lock, log, call the engine or fault.
        void record_source_event(void const* a_behavior, void const* a_ctx, void const* a_event) noexcept
        {
            // Counted before the gate so the liveness control sees every dispatch, including the
            // plugin's own direct 58377 calls and any dispatch outside an armed window.
            g_calls.fetch_add(1, std::memory_order_relaxed);
            void const* const gate = g_gate_behavior.load(std::memory_order_relaxed);
            if (!gate || gate != a_behavior)
            {
                // A different graph while armed is the classification evidence: the filter is
                // discriminating, not silently dropping everything.
                if (gate && a_behavior)
                    g_other_graph.fetch_add(1, std::memory_order_relaxed);
                return;
            }
            if (!a_event)
                return;
            std::uint32_t id = 0;
            std::memcpy(&id, a_event, sizeof(id));
            void const* payload = nullptr;
            std::memcpy(&payload, static_cast<std::byte const*>(a_event) + 0x08, sizeof(payload));
            void const* ctx_behavior = nullptr;
            if (a_ctx)
                std::memcpy(&ctx_behavior, static_cast<std::byte const*>(a_ctx) + 0x08, sizeof(ctx_behavior));
            (void)event_ring().push(SourceEventRecord{ id, payload != nullptr ? std::uint8_t{ 1 } :
                std::uint8_t{ 0 }, ctx_behavior != nullptr ? std::uint8_t{ 1 } : std::uint8_t{ 0 } });
        }

        // ---- sampler helpers ----------------------------------------------------

        RE::BShkbAnimationGraph* player_graph(RE::Actor& actor)
        {
            RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
            if (!actor.GetAnimationGraphManager(manager) || !manager)
                return nullptr;
            RE::BShkbAnimationGraph* chosen = nullptr;
            for (RE::BSTSmartPointer<RE::BShkbAnimationGraph> const& graph : manager->graphs)
            {
                if (!graph || !graph->behaviorGraph || graph->holder != &actor)
                    continue;
                char const* const name = graph->projectName.c_str();
                if (name && std::strstr(name, "1stperson"))
                    continue;
                chosen = graph.get();
            }
            return chosen;
        }

        // Membership check for a pinned concurrent target: returns the manager's own pointer when
        // the expected graph is still live, otherwise null. Only pointers are compared, so a freed
        // graph is never dereferenced.
        RE::BShkbAnimationGraph* managed_graph(RE::Actor& a_actor, RE::BShkbAnimationGraph const* a_expected)
        {
            if (!a_expected)
                return nullptr;
            RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
            if (!a_actor.GetAnimationGraphManager(manager) || !manager)
                return nullptr;
            for (RE::BSTSmartPointer<RE::BShkbAnimationGraph> const& graph : manager->graphs)
                if (graph && graph.get() == a_expected)
                    return graph.get();
            return nullptr;
        }

        template <class T>
        T field_at(void const* base, std::size_t offset)
        {
            T value;
            std::memcpy(&value, static_cast<std::byte const*>(base) + offset, sizeof(value));
            return value;
        }

        bool variable_index(RE::hkbBehaviorGraphData const* a_data, char const* a_name, std::size_t& a_out)
        {
            if (!a_data || !a_data->stringData)
                return false;
            auto const& names = a_data->stringData->variableNames;
            std::int32_t const count = names.size();
            if (count <= 0 || static_cast<std::size_t>(count) > kMaxWords)
                return false;
            for (std::int32_t i = 0; i < count; ++i)
            {
                char const* const name = names.data()[i].c_str();
                if (name && std::strcmp(name, a_name) == 0)
                {
                    a_out = static_cast<std::size_t>(i);
                    return true;
                }
            }
            return false;
        }

        std::int32_t read_word(std::int32_t const* a_words, std::size_t a_count, std::size_t a_index)
        {
            return a_index < a_count ? a_words[a_index] : 0;
        }

        char const* event_name(RE::hkbBehaviorGraph const& a_behavior, std::uint32_t a_id)
        {
            RE::hkbBehaviorGraphData const* const data = a_behavior.data.get();
            if (!data || !data->stringData)
                return nullptr;
            auto const& names = data->stringData->eventNames;
            if (a_id >= static_cast<std::uint32_t>(names.size()))
                return nullptr;
            return names[a_id].c_str();
        }

        void close_window(char const* a_reason)
        {
            g_gate_behavior.store(nullptr, std::memory_order_release);
            CaptureState& s = capture_state();
            s.window.cancel();
            // C-G4-C1: the liveness control and the isolation reading share one row. `calls` is
            // every detour entry in the window (>=2 because the replay dispatches twice),
            // `other-graph` is how many of those aimed at a non-source graph, `source-events` is
            // the leak signature.
            logger::info("SOURCE EVENTCAPTURE window-closed reason={} actor={:08x} events={} phases={} calls={} other-graph={} ring-dropped={}",
                a_reason, s.actor_id, s.window.event_lines(), s.window.phase_lines(),
                g_calls.load(std::memory_order_relaxed), g_other_graph.load(std::memory_order_relaxed),
                event_ring().dropped());
        }

        void baseline_capture(CaptureState& a_state, RE::BShkbAnimationGraph const& a_graph,
            RE::hkbBehaviorGraph& a_behavior, std::int32_t const* a_words, std::size_t a_count,
            std::uint32_t a_actor_id)
        {
            RE::hkbBehaviorGraphData const* const data = a_behavior.data.get();
            for (std::size_t i = 0; i < kPhaseCount; ++i)
            {
                a_state.tracked[i] = variable_index(data, kPhaseNames[i], a_state.index[i]);
                if (a_state.tracked[i])
                {
                    a_state.logged[i] = read_word(a_words, a_count, a_state.index[i]);
                    a_state.candidate[i] = a_state.logged[i];
                }
                a_state.settled[i] = 0;
            }
            void* const nodes = a_behavior.activeNodes;
            a_state.node_logged = nodes ? field_at<std::int32_t>(nodes, kNodeCountOffset) : 0;
            a_state.node_candidate = a_state.node_logged;
            a_state.node_settled = 0;
            a_state.seq = 0;
            a_state.dropped_seen = 0;
            a_state.actor_id = a_actor_id;
            event_ring().drain_discard();
            event_ring().reset_dropped();
            // Counters belong to the window: a fresh window starts at zero so its close row can
            // be read without subtracting a previous window.
            g_calls.store(0, std::memory_order_relaxed);
            g_other_graph.store(0, std::memory_order_relaxed);
            // Gate last: from this store on, every dispatch of this graph lands in the ring.
            g_gate_behavior.store(&a_behavior, std::memory_order_release);
            std::string tracked;
            for (std::size_t i = 0; i < kPhaseCount; ++i)
                tracked += fmt::format("{}{}={}", i ? " " : "", kPhaseNames[i],
                    a_state.tracked[i] ? std::to_string(a_state.index[i]) : std::string{ "missing" });
            logger::info("SOURCE EVENTCAPTURE window-open actor={:08x} words={} nodes={} events-budget={} phases-budget={} seconds={} graph={:#x} behavior={:#x} data={:#x} set={:#x} words-ptr={:#x} tracked {}",
                a_actor_id, a_count, a_state.node_logged, Source_Event_Line_Budget,
                Source_Phase_Line_Budget, Source_Event_Window.count(),
                reinterpret_cast<std::uintptr_t>(&a_graph),
                reinterpret_cast<std::uintptr_t>(&a_behavior),
                reinterpret_cast<std::uintptr_t>(a_behavior.data.get()),
                reinterpret_cast<std::uintptr_t>(a_behavior.variableValueSet.get()),
                reinterpret_cast<std::uintptr_t>(a_words), tracked);
        }

        // C-G4-C1: one place that turns a live source graph into an armed window (reset, deadline,
        // identity, baseline, gate). The concurrent arm calls it synchronously from the experiment
        // entry, which is what guarantees the experiment's own dispatches are counted; the F7 path
        // keeps its deferred baseline in the sampler.
        bool begin_capture_window(RE::Actor& a_actor, RE::BShkbAnimationGraph const& a_graph,
            RE::hkbBehaviorGraph& a_behavior)
        {
            RE::hkbBehaviorGraphData* const data = a_behavior.data.get();
            RE::hkbVariableValueSet* const set = a_behavior.variableValueSet.get();
            if (!data || !set)
                return false;
            std::int32_t const count = field_at<std::int32_t>(set, kWordCountOffset);
            std::int32_t const* const words = field_at<std::int32_t const*>(set, kWordDataOffset);
            if (count <= 0 || static_cast<std::size_t>(count) > kMaxWords || !words)
                return false;
            CaptureState& s = capture_state();
            s = CaptureState{};
            s.window.arm(std::chrono::steady_clock::now());
            s.observed = SourceTimelineIdentity{
                reinterpret_cast<std::uintptr_t>(&a_actor), reinterpret_cast<std::uintptr_t>(&a_graph),
                reinterpret_cast<std::uintptr_t>(&a_behavior), reinterpret_cast<std::uintptr_t>(data),
                reinterpret_cast<std::uintptr_t>(set), reinterpret_cast<std::uintptr_t>(words),
                static_cast<std::size_t>(count)
            };
            s.has_observed = true;
            baseline_capture(s, a_graph, a_behavior, words, static_cast<std::size_t>(count), a_actor.GetFormID());
            return true;
        }

        void drain_events(CaptureState& a_state, RE::hkbBehaviorGraph& a_behavior)
        {
            SourceEventRecord record{};
            while (event_ring().pop(record))
            {
                if (!a_state.window.take_event_line())
                {
                    logger::info("SOURCE EVENTCAPTURE budget-exhausted kind=event actor={:08x} lines={}",
                        a_state.actor_id, Source_Event_Line_Budget);
                    close_window("budget-exhausted");
                    return;
                }
                ++a_state.seq;
                char const* const name = event_name(a_behavior, record.id);
                logger::info("SOURCE EVENTCAPTURE event actor={:08x} seq={} id={} name='{}' payload={} ctx-behavior={}",
                    a_state.actor_id, a_state.seq, record.id, name ? name : "oor",
                    record.payload_set ? "set" : "null", record.ctx_behavior_set ? "set" : "null");
            }
            std::uint64_t const dropped = event_ring().dropped();
            if (dropped != a_state.dropped_seen)
            {
                logger::info("SOURCE EVENTCAPTURE ring-dropped actor={:08x} total={} new={}",
                    a_state.actor_id, dropped, dropped - a_state.dropped_seen);
                a_state.dropped_seen = dropped;
            }
        }

        void sample_phase(CaptureState& a_state, RE::hkbBehaviorGraph& a_behavior,
            std::int32_t const* a_words, std::size_t a_count)
        {
            for (std::size_t i = 0; i < kPhaseCount; ++i)
            {
                if (!a_state.tracked[i])
                    continue;
                std::int32_t const current = read_word(a_words, a_count, a_state.index[i]);
                if (current != a_state.candidate[i])
                {
                    a_state.candidate[i] = current;
                    a_state.settled[i] = 0;
                    continue;
                }
                if (a_state.settled[i] < kSettleSamples)
                    ++a_state.settled[i];
                if (a_state.settled[i] < kSettleSamples || current == a_state.logged[i])
                    continue;
                if (!a_state.window.take_phase_line())
                {
                    logger::info("SOURCE EVENTCAPTURE budget-exhausted kind=phase actor={:08x} lines={}",
                        a_state.actor_id, Source_Phase_Line_Budget);
                    close_window("budget-exhausted");
                    return;
                }
                std::int32_t const previous = a_state.logged[i];
                a_state.logged[i] = current;
                logger::info("SOURCE EVENTCAPTURE phase actor={:08x} name='{}' {}->{}",
                    a_state.actor_id, kPhaseNames[i], previous, current);
            }
            void* const nodes = a_behavior.activeNodes;
            std::int32_t const count = nodes ? field_at<std::int32_t>(nodes, kNodeCountOffset) : 0;
            if (count != a_state.node_candidate)
            {
                a_state.node_candidate = count;
                a_state.node_settled = 0;
                return;
            }
            if (a_state.node_settled < kSettleSamples)
                ++a_state.node_settled;
            if (a_state.node_settled < kSettleSamples || count == a_state.node_logged)
                return;
            if (!a_state.window.take_phase_line())
            {
                logger::info("SOURCE EVENTCAPTURE budget-exhausted kind=phase actor={:08x} lines={}",
                    a_state.actor_id, Source_Phase_Line_Budget);
                close_window("budget-exhausted");
                return;
            }
            std::int32_t const previous = a_state.node_logged;
            a_state.node_logged = count;
            logger::info("SOURCE EVENTCAPTURE phase actor={:08x} name='nodes' {}->{}",
                a_state.actor_id, previous, count);
        }

        void abort_capture(char const* a_reason)
        {
            CaptureState& s = capture_state();
            s.window.cancel();
            // C-G4-C1: an abort must still carry the liveness counters - the first v64 attempt lost
            // its positive-control reading exactly here.
            logger::info("SOURCE EVENTCAPTURE window-aborted reason={} actor={:08x} events={} phases={} calls={} other-graph={}",
                a_reason, s.actor_id, s.window.event_lines(), s.window.phase_lines(),
                g_calls.load(std::memory_order_relaxed), g_other_graph.load(std::memory_order_relaxed));
        }
    }

    void install_source_event_capture()
    {
        bool expected = false;
        if (!g_installed.compare_exchange_strong(expected, true))
            return;
        if (!source_event_capture())
        {
            logger::info("SOURCE EVENTCAPTURE install-skip reason=off");
            return;
        }
        if (source_timeline())
        {
            logger::warn("SOURCE EVENTCAPTURE install-skip reason=source-timeline-priority");
            return;
        }
        // C-G4-C1: only the legacy lifecycle requires IndependentGraph=off. The concurrent mode is
        // designed to run beside the candidate experiment, so its guard is the timeline priority
        // above plus the mode itself - never a silent removal of the legacy refusal.
        if (source_event_capture_mode() == SourceEventCaptureMode::e_on &&
            independent_graph_setting() != IndependentGraphMode::off)
        {
            logger::warn("SOURCE EVENTCAPTURE install-skip reason=independent-graph-not-off");
            return;
        }
        REL::Relocation<std::uintptr_t> target{ REL::ID(58377) };
        auto const* const head = reinterpret_cast<std::uint8_t const*>(target.address());
        if (std::memcmp(head, Source_Event_Head_Bytes.data(), Source_Event_Head_Bytes.size()) != 0)
        {
            std::string hex;
            for (std::size_t i = 0; i < 16; ++i)
                hex += fmt::format("{:02x}", head[i]);
            logger::warn("SOURCE EVENTCAPTURE install-failed reason=head-mismatch target={:#x} head={}",
                target.address(), hex);
            return;
        }
        auto& trampoline = SKSE::GetTrampoline();
        trampoline.create(256);
        // 16-aligned stub entry (73 bytes) plus the relocated 7-byte head, a far jump and its
        // 8-byte address (21 bytes).
        auto* const mem = static_cast<std::uint8_t*>(trampoline.allocate(112));
        if (!mem)
        {
            logger::warn("SOURCE EVENTCAPTURE install-failed reason=trampoline-exhausted");
            return;
        }
        std::uint8_t* const stub = mem;
        std::uint8_t* const original = mem + 80;
        std::memcpy(original, Source_Event_Head_Bytes.data(), Source_Event_Head_Bytes.size());
        {
            std::uint8_t const far_jmp[6] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 };
            std::memcpy(original + Source_Event_Head_Bytes.size(), far_jmp, sizeof(far_jmp));
            std::uintptr_t const back = target.address() + Source_Event_Head_Bytes.size();
            std::memcpy(original + Source_Event_Head_Bytes.size() + sizeof(far_jmp), &back, sizeof(back));
        }
        {
            // The thunk preserves every register and the flags, hands (behavior, ctx, event) to
            // record_source_event and jumps to the relocated head. Entry rsp is 8 mod 16 (jmp
            // from a call site), the nine pushes leave it 0 mod 16 for the call.
            std::size_t o = 0;
            auto const emit = [&o, stub](std::initializer_list<std::uint8_t> const& a_bytes) {
                for (std::uint8_t b : a_bytes)
                    stub[o++] = b;
            };
            emit({ 0x50, 0x51, 0x52 });                          // push rax/rcx/rdx
            emit({ 0x41, 0x50, 0x41, 0x51, 0x41, 0x52, 0x41, 0x53 });  // push r8..r11
            emit({ 0x9C });                                      // pushfq
            emit({ 0x48, 0x8B, 0x4C, 0x24, 0x38 });              // mov rcx,[rsp+38h]
            emit({ 0x48, 0x8B, 0x54, 0x24, 0x30 });              // mov rdx,[rsp+30h]
            emit({ 0x4C, 0x8B, 0x44, 0x24, 0x28 });              // mov r8,[rsp+28h]
            emit({ 0x48, 0x83, 0xEC, 0x20 });                    // sub rsp,20h
            emit({ 0x48, 0xB8 });                                // movabs rax, recorder
            auto const recorder = reinterpret_cast<std::uintptr_t>(&record_source_event);
            std::memcpy(stub + o, &recorder, sizeof(recorder));
            o += sizeof(recorder);
            emit({ 0xFF, 0xD0 });                                // call rax
            emit({ 0x48, 0x83, 0xC4, 0x20 });                    // add rsp,20h
            emit({ 0x9D });                                      // popfq
            emit({ 0x41, 0x5B, 0x41, 0x5A, 0x41, 0x59, 0x41, 0x58 });  // pop r11..r8
            emit({ 0x5A, 0x59, 0x58 });                          // pop rdx/rcx/rax
            emit({ 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 });        // jmp qword [rip+0]
            std::memcpy(stub + o, &original, sizeof(original));
            o += sizeof(original);
        }
        std::uint8_t patch[7] = { 0xE9, 0x00, 0x00, 0x00, 0x00, 0x90, 0x90 };
        std::int32_t const rel = static_cast<std::int32_t>(
            reinterpret_cast<std::uintptr_t>(stub) - (target.address() + 5));
        std::memcpy(patch + 1, &rel, sizeof(rel));
        REL::safe_write(target.address(), patch, sizeof(patch));
        logger::info("SOURCE EVENTCAPTURE installed mode={} target={:#x} stub={:#x} original={:#x} head=48 8b c4 48 89 48 08",
            source_event_capture_mode_name(source_event_capture_mode()), target.address(),
            reinterpret_cast<std::uintptr_t>(stub), reinterpret_cast<std::uintptr_t>(original));
    }

    void arm_source_event_capture()
    {
        if (!source_event_capture() || !g_installed.load(std::memory_order_acquire))
            return;
        // C-G4-C1: only the legacy lifecycle listens to F7. The concurrent window is owned by the
        // experiment/display path, so F7 must not open a second window.
        if (source_event_capture_mode() != SourceEventCaptureMode::e_on)
            return;
        capture_requests().arm();
        logger::info("SOURCE EVENTCAPTURE arm-requested");
    }

    void invalidate_source_event_capture()
    {
        if (!source_event_capture() || !g_installed.load(std::memory_order_acquire))
            return;
        capture_requests().invalidate();
    }

    bool arm_source_event_capture_concurrent(RE::BShkbAnimationGraph const& a_source_graph, std::uint32_t a_actor_id)
    {
        if (!source_event_capture_concurrent() || !g_installed.load(std::memory_order_acquire))
            return false;
        RE::Actor* const holder = a_source_graph.holder;
        if (!holder)
        {
            logger::warn("SOURCE EVENTCAPTURE concurrent-arm-skip reason=no-holder actor={:08x}", a_actor_id);
            return false;
        }
        // v66: pin the experiment's graph. v65 armed with player_graph() to satisfy the identity
        // rule, but in this mod list that resolver returns a different graph than the one the
        // candidate descends from, so the window observed the wrong target (see the contract).
        // The manager list is the authority: pin only a graph it still owns.
        RE::BShkbAnimationGraph* const graph = managed_graph(*holder, &a_source_graph);
        if (!graph)
        {
            logger::warn("SOURCE EVENTCAPTURE concurrent-arm-skip reason=source-not-in-manager actor={:08x}", a_actor_id);
            return false;
        }
        RE::hkbBehaviorGraph* const behavior = graph->behaviorGraph;
        if (!behavior)
        {
            logger::warn("SOURCE EVENTCAPTURE concurrent-arm-skip reason=no-behavior actor={:08x}", a_actor_id);
            return false;
        }
        if (capture_state().window.active())
            close_window("rearm");
        if (!begin_capture_window(*holder, *graph, *behavior))
        {
            logger::warn("SOURCE EVENTCAPTURE concurrent-arm-skip reason=source-fields actor={:08x}", a_actor_id);
            return false;
        }
        capture_state().concurrent_graph = graph;
        logger::info("SOURCE EVENTCAPTURE concurrent-armed actor={:08x} graph={:#x} pinned=yes behavior={:#x}",
            a_actor_id, reinterpret_cast<std::uintptr_t>(graph), reinterpret_cast<std::uintptr_t>(behavior));
        return true;
    }

    bool source_event_capture_window_open()
    {
        return capture_state().window.active();
    }

    void close_source_event_capture_window(char const* a_reason)
    {
        if (!capture_state().window.active())
            return;
        close_window(a_reason);
    }

    void sample_source_event_capture(RE::Actor& actor)
    {
        if (!source_event_capture() || !g_installed.load(std::memory_order_acquire))
            return;
        CaptureState& s = capture_state();
        SourceTimelineIntent const intent = capture_requests().take();
        if (intent == SourceTimelineIntent::e_invalidate)
        {
            s = CaptureState{};
            g_gate_behavior.store(nullptr, std::memory_order_release);
            g_calls.store(0, std::memory_order_relaxed);
            g_other_graph.store(0, std::memory_order_relaxed);
            logger::info("SOURCE EVENTCAPTURE invalidated reason=world-reset");
            return;
        }
        if (intent == SourceTimelineIntent::e_arm)
        {
            s = CaptureState{};
            s.window.arm(std::chrono::steady_clock::now());
        }
        if (!s.window.active())
            return;
        if (s.window.expire(std::chrono::steady_clock::now()))
        {
            close_window("expired");
            return;
        }
        // C-G4-C1 (v66): a concurrent window observes the pinned graph - the one the candidate
        // descends from - and re-validates its membership every frame. Legacy F7 windows keep the
        // player_graph() resolution unchanged.
        RE::BShkbAnimationGraph* graph = s.concurrent_graph ? managed_graph(actor, s.concurrent_graph)
                                                            : player_graph(actor);
        if (!graph || !graph->behaviorGraph)
        {
            if (s.actor_id != 0)
                abort_capture("source-lost-or-replaced");
            return;
        }
        RE::hkbBehaviorGraph* const behavior = graph->behaviorGraph;
        RE::hkbBehaviorGraphData* const data = behavior->data.get();
        RE::hkbVariableValueSet* const set = behavior->variableValueSet.get();
        if (!data || !set)
        {
            if (s.actor_id != 0)
                abort_capture("source-lost-or-replaced");
            return;
        }
        std::int32_t const count = field_at<std::int32_t>(set, kWordCountOffset);
        std::int32_t const* const words = field_at<std::int32_t const*>(set, kWordDataOffset);
        if (count <= 0 || static_cast<std::size_t>(count) > kMaxWords || !words)
        {
            if (s.actor_id != 0)
                abort_capture("source-lost-or-replaced");
            return;
        }
        SourceTimelineIdentity const source{
            reinterpret_cast<std::uintptr_t>(&actor), reinterpret_cast<std::uintptr_t>(graph),
            reinterpret_cast<std::uintptr_t>(behavior), reinterpret_cast<std::uintptr_t>(data),
            reinterpret_cast<std::uintptr_t>(set), reinterpret_cast<std::uintptr_t>(words),
            static_cast<std::size_t>(count)
        };
        if (!s.has_observed)
        {
            s.observed = source;
            s.has_observed = true;
            baseline_capture(s, *graph, *behavior, words, source.count, actor.GetFormID());
            drain_events(s, *behavior);
            if (s.window.active())
                sample_phase(s, *behavior, words, source.count);
            return;
        }
        SourceTimelineIdentity const& previous = s.observed;
        if (previous.actor != source.actor || previous.graph != source.graph ||
            previous.behavior != source.behavior || previous.data != source.data ||
            previous.variable_set != source.variable_set || previous.words != source.words ||
            previous.count != source.count)
        {
            // C-G4-C1: name the field that moved. The v64 attempt aborted with no delta and cost a
            // session to diagnose.
            logger::info("SOURCE EVENTCAPTURE identity-mismatch actor={:08x} prev(actor={:#x} graph={:#x} behavior={:#x} data={:#x} set={:#x} words={:#x} count={}) now(actor={:#x} graph={:#x} behavior={:#x} data={:#x} set={:#x} words={:#x} count={})",
                s.actor_id, previous.actor, previous.graph, previous.behavior, previous.data,
                previous.variable_set, previous.words, previous.count, source.actor, source.graph,
                source.behavior, source.data, source.variable_set, source.words, source.count);
            abort_capture("source-lost-or-replaced");
            return;
        }
        drain_events(s, *behavior);
        if (!s.window.active())
            return;
        sample_phase(s, *behavior, words, source.count);
    }
}
PLUGIN_NAMESPACE_END
