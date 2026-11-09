//
// Created by AmazingBuff on 2026/10/28.
//

#include "animation/behavior_graph/source_timeline.h"

#include "animation/behavior_graph/source_timeline_policy.h"

#include "animation/settings.h"

#include <RE/A/Actor.h>
#include <RE/B/BSAnimationGraphManager.h>
#include <RE/B/BShkbAnimationGraph.h>
#include <RE/H/hkbBehaviorGraph.h>
#include <RE/H/hkbBehaviorGraphData.h>
#include <RE/H/hkbBehaviorGraphStringData.h>
#include <RE/H/hkbVariableValueSet.h>
#include <RE/H/hkStringPtr.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    namespace
    {
        // G3-A budgets and rules. The point of the settle rule is to ignore per-frame scalars
        // (TimeDelta, Pitch, velocity...) and keep only values that actually hold: a change is
        // written only after the new value has survived kSettleSamples consecutive samples.
        constexpr std::size_t kMaxWords = 1024;
        constexpr std::uint8_t kSettleSamples = 3;

        // hkbVariableValueSet stores its word array behind an hkArray whose data pointer and
        // count are at +0x10 / +0x18; both offsets are the ones the candidate probe already reads.
        constexpr std::size_t kWordDataOffset = 0x10;
        constexpr std::size_t kWordCountOffset = 0x18;
        // hkbBehaviorGraph::activeNodes is an opaque NodeList: data pointer at +0, count at +8.
        constexpr std::size_t kNodeCountOffset = 0x8;

        template <class T>
        T field_at(void const* base, std::size_t offset)
        {
            T value;
            std::memcpy(&value, static_cast<std::byte const*>(base) + offset, sizeof(value));
            return value;
        }

        // C-G5-E3 (M2): one TSC basis for both sides. The probe already stamps its steps with
        // __rdtsc(), so the source publishes the same unit; the per-window ratio to steady_clock is
        // printed for readability only and never takes part in the pairing decision.
        std::uint64_t tsc_now()
        {
#if defined(_MSC_VER)
            return static_cast<std::uint64_t>(__rdtsc());
#else
            return static_cast<std::uint64_t>(
                std::chrono::steady_clock::now().time_since_epoch().count());
#endif
        }

        // C-G5-E3 (M1): the experiment publishes one arm request with its pinned source graph.
        // Only `concurrent` consumes it; `on` keeps the F7 path untouched.
        struct ConcurrentArm
        {
            std::atomic<bool> pending{ false };
            std::atomic<void*> graph{ nullptr };
            std::atomic<std::uint32_t> actor{ 0 };
            std::atomic<std::uint64_t> request{ 0 };
        };

        ConcurrentArm& concurrent_arm()
        {
            static ConcurrentArm s_arm;
            return s_arm;
        }

        std::atomic<bool> g_close_pending{ false };
        char const* g_close_reason = "none";

        struct Recorder
        {
            std::vector<std::int32_t> logged;
            std::vector<std::int32_t> candidate;
            std::vector<std::uint8_t> settled;
            std::vector<std::uint8_t> named;
            // C-G3B-E3: per-index transient mode - BOOL/INT-typed variables log every observed
            // word change immediately (settle-0), catching one-frame command writes the
            // settle-3 rule filters out. REAL/quad types keep settle-3.
            std::vector<std::uint8_t> transient;
            std::int32_t node_count = -1;
            std::int32_t node_candidate = -1;
            std::uint8_t node_settled = 0;
            SourceTimelineWindow window;
            // C-G5-E3 (M1): concurrent-mode pinning. The pointer is only ever compared against
            // the manager's graph list; an unowned graph is never dereferenced.
            std::uintptr_t pinned_graph{ 0 };
            std::uint64_t request{ 0 };
            bool concurrent{ false };
        };

        // The settle ledger lives outside the Recorder because it holds atomics and the Recorder is
        // rebuilt by assignment at arm/invalidate. It is reset explicitly at the same points.
        SourceSettleRing& settle_ring()
        {
            static SourceSettleRing s_ring;
            return s_ring;
        }

        Recorder& recorder()
        {
            static Recorder s_recorder;
            return s_recorder;
        }

        SourceTimelineRequests& requests()
        {
            static SourceTimelineRequests s_requests;
            return s_requests;
        }

        bool take_line(Recorder& state)
        {
            if (state.window.exhausted())
                return false;
            if (state.window.take_line())
                return true;
            logger::info("SOURCE TIMELINE budget-exhausted lines={}", Source_Timeline_Line_Budget);
            return false;
        }

        void abort_source(Recorder& state)
        {
            state.window.cancel();
            logger::info("SOURCE TIMELINE window-aborted reason=source-lost-or-replaced lines={}", state.window.lines());
        }

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

        // C-G5-E3 (M1): resolve the pinned source graph by pointer comparison against the manager's
        // list. A graph that is no longer owned (replaced, freed, world reset) resolves to nullptr,
        // so the window aborts instead of dereferencing a stale pointer.
        RE::BShkbAnimationGraph* pinned_source_graph(RE::Actor& actor, std::uintptr_t a_pinned)
        {
            if (a_pinned == 0)
                return nullptr;
            RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
            if (!actor.GetAnimationGraphManager(manager) || !manager)
                return nullptr;
            auto const* const wanted = reinterpret_cast<RE::BShkbAnimationGraph*>(a_pinned);
            for (RE::BSTSmartPointer<RE::BShkbAnimationGraph> const& graph : manager->graphs)
            {
                if (graph && graph.get() == wanted)
                    return graph.get();
            }
            return nullptr;
        }

        char const* variable_name(RE::hkbBehaviorGraph const& a_graph, std::size_t a_index)
        {
            RE::hkbBehaviorGraphData const* const data = a_graph.data.get();
            if (!data || !data->stringData)
                return nullptr;
            auto const& names = data->stringData->variableNames;
            if (a_index >= static_cast<std::size_t>(names.size()))
                return nullptr;
            return names[static_cast<std::uint32_t>(a_index)].c_str();
        }

        void baseline(Recorder& a_state, RE::hkbBehaviorGraph& a_behavior, std::int32_t const* a_words,
            std::size_t a_count, std::uint32_t a_actor_id)
        {
            a_state.logged.assign(a_words, a_words + a_count);
            a_state.candidate.assign(a_words, a_words + a_count);
            a_state.settled.assign(a_count, 0);
            a_state.named.assign(a_count, 0);
            a_state.transient.assign(a_count, 0);
            if (source_transient_watch())
            {
                RE::hkbBehaviorGraphData const* const data = a_behavior.data.get();
                if (data && data->variableInfos.size() >= a_count)
                {
                    for (std::size_t i = 0; i < a_count; ++i)
                    {
                        std::int8_t const type = static_cast<std::int8_t>(
                            data->variableInfos.data()[i].m_type.get());
                        a_state.transient[i] = type >= 0 && type <= 3 ? std::uint8_t{ 1 } :
                            std::uint8_t{ 0 };
                    }
                }
            }
            void* const nodes = a_behavior.activeNodes;
            a_state.node_count = nodes ? field_at<std::int32_t>(nodes, kNodeCountOffset) : 0;
            a_state.node_candidate = a_state.node_count;
            a_state.node_settled = 0;
            logger::info("SOURCE TIMELINE window-open actor={:08x} words={} nodes={} seconds={} mode={} request={}",
                a_actor_id, a_count, a_state.node_count, Source_Timeline_Window.count(),
                a_state.concurrent ? "concurrent" : (source_transient_watch() ? "transient" : "settled"),
                a_state.request);
        }

        void sample_variables(Recorder& a_state, RE::hkbBehaviorGraph& a_behavior, std::int32_t const* a_words,
            std::size_t a_count)
        {
            for (std::size_t i = 0; i < a_count; ++i)
            {
                std::int32_t const current = a_words[i];
                if (a_state.transient.size() > i && a_state.transient[i] && current != a_state.logged[i])
                {
                    // Transient mode: a one-frame write (and its revert) must be visible, so
                    // every observed change against the last logged value logs immediately.
                    if (!take_line(a_state))
                        return;
                    std::int32_t const previous = a_state.logged[i];
                    a_state.logged[i] = current;
                    a_state.candidate[i] = current;
                    a_state.settled[i] = 0;
                    char const* name = a_state.named[i] ? nullptr : variable_name(a_behavior, i);
                    if (name)
                        a_state.named[i] = 1;
                    if (name)
                        logger::info("SOURCE TIMELINE variable index={} name='{}' {}->{} mode=transient", i, name,
                            previous, current);
                    else
                        logger::info("SOURCE TIMELINE variable index={} {}->{} mode=transient", i, previous, current);
                    continue;
                }
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
                std::int32_t const previous = a_state.logged[i];
                a_state.logged[i] = current;
                if (!take_line(a_state))
                    return;
                // C-G5-E3 (M2): the confirmation instant is the pairing anchor, so it is published
                // with the log line in one unit for both sides.
                std::uint64_t const settle_tsc = tsc_now();
                (void)settle_ring().push(settle_tsc);
                float previous_float = 0.0f;
                float current_float = 0.0f;
                std::memcpy(&previous_float, &previous, sizeof(float));
                std::memcpy(&current_float, &current, sizeof(float));
                char const* name = a_state.named[i] ? nullptr : variable_name(a_behavior, i);
                if (name)
                    a_state.named[i] = 1;
                if (name)
                    logger::info("SOURCE TIMELINE variable index={} name='{}' {}->{} float={:.4f}->{:.4f} tsc={}", i, name,
                        previous, current, previous_float, current_float, settle_tsc);
                else
                    logger::info("SOURCE TIMELINE variable index={} {}->{} float={:.4f}->{:.4f} tsc={}", i, previous, current,
                        previous_float, current_float, settle_tsc);
            }
        }

        void sample_nodes(Recorder& a_state, RE::hkbBehaviorGraph& a_behavior)
        {
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
            if (a_state.node_settled < kSettleSamples || count == a_state.node_count)
                return;
            a_state.node_count = count;
            if (take_line(a_state))
            {
                std::uint64_t const settle_tsc = tsc_now();
                (void)settle_ring().push(settle_tsc);
                logger::info("SOURCE TIMELINE nodes count={} tsc={}", count, settle_tsc);
            }
        }
    }

    void arm_source_timeline()
    {
        // F7 belongs to the `on` lifecycle only; `concurrent` is armed by the experiment.
        if (source_timeline_mode() != SourceTimelineMode::e_on)
            return;
        requests().arm();
        logger::info("SOURCE TIMELINE arm-requested");
    }

    bool arm_source_timeline_concurrent(RE::BShkbAnimationGraph const& a_source_graph, std::uint32_t a_actor_id,
        std::uint64_t a_request)
    {
        if (source_timeline_mode() != SourceTimelineMode::e_concurrent)
            return false;
        if (recorder().window.ever_armed())
        {
            logger::warn("SOURCE TIMELINE concurrent-arm-skip reason=window-already-used actor={:08x}", a_actor_id);
            return false;
        }
        ConcurrentArm& arm = concurrent_arm();
        arm.graph.store(const_cast<RE::BShkbAnimationGraph*>(&a_source_graph), std::memory_order_release);
        arm.actor.store(a_actor_id, std::memory_order_release);
        arm.request.store(a_request, std::memory_order_release);
        arm.pending.store(true, std::memory_order_release);
        logger::info("SOURCE TIMELINE concurrent-arm-requested actor={:08x} request={}", a_actor_id, a_request);
        return true;
    }

    void close_source_timeline_window(char const* a_reason)
    {
        g_close_reason = a_reason ? a_reason : "none";
        g_close_pending.store(true, std::memory_order_release);
    }

    std::size_t take_source_settles(std::uint64_t* a_out, std::size_t a_capacity)
    {
        if (a_out == nullptr || a_capacity == 0)
            return 0;
        std::size_t taken = 0;
        while (taken < a_capacity)
        {
            std::uint64_t settle = 0;
            if (!settle_ring().pop(settle))
                break;
            a_out[taken++] = settle;
        }
        return taken;
    }

    std::size_t source_settle_backlog() { return settle_ring().size(); }

    void invalidate_source_timeline()
    {
        requests().invalidate();
    }

    void sample_source_timeline(RE::Actor& actor)
    {
        if (!source_timeline())
            return;
        Recorder& state = recorder();
        SourceTimelineIntent const intent = requests().take();
        if (intent == SourceTimelineIntent::e_invalidate)
        {
            state = Recorder{};
            settle_ring().reset();
            concurrent_arm().pending.store(false, std::memory_order_release);
            g_close_pending.store(false, std::memory_order_release);
            logger::info("SOURCE TIMELINE invalidated reason=world-reset");
            return;
        }
        if (intent == SourceTimelineIntent::e_arm)
        {
            state = Recorder{};
            settle_ring().reset();
            state.window.arm(std::chrono::steady_clock::now());
        }
        // C-G5-E3 (M1): the experiment's arm carries the pinned graph; it is applied here so the
        // window opens on the sampler thread with the source identity already pinned.
        ConcurrentArm& arm = concurrent_arm();
        if (arm.pending.exchange(false, std::memory_order_acquire))
        {
            state = Recorder{};
            settle_ring().reset();
            state.concurrent = true;
            state.pinned_graph = reinterpret_cast<std::uintptr_t>(arm.graph.load(std::memory_order_acquire));
            state.request = arm.request.load(std::memory_order_acquire);
            state.window.arm(std::chrono::steady_clock::now());
            logger::info("SOURCE TIMELINE concurrent-arm actor={:08x} request={} graph={:#x}",
                arm.actor.load(std::memory_order_relaxed), state.request, state.pinned_graph);
        }
        if (g_close_pending.exchange(false, std::memory_order_acquire) && state.window.active())
        {
            std::uint32_t const lines = state.window.lines();
            state.window.cancel();
            logger::info("SOURCE TIMELINE window-closed reason={} lines={} settles-pending={} settles-dropped={}",
                g_close_reason, lines, settle_ring().size(), settle_ring().dropped());
        }
        if (!state.window.active())
            return;
        if (state.window.expire(std::chrono::steady_clock::now()))
        {
            logger::info("SOURCE TIMELINE window-closed reason=expired lines={}", state.window.lines());
            return;
        }
        RE::BShkbAnimationGraph* graph = nullptr;
        if (state.concurrent)
        {
            // C-G5-E3: pointer comparison only. An unowned graph is never dereferenced, so a
            // replaced or freed source aborts the window instead of reading freed memory.
            graph = pinned_source_graph(actor, state.pinned_graph);
            if (!graph)
            {
                abort_source(state);
                return;
            }
        }
        else
        {
            graph = player_graph(actor);
            if (!graph)
            {
                abort_source(state);
                return;
            }
        }
        RE::hkbBehaviorGraph* const behavior = graph->behaviorGraph;
        if (!behavior)
        {
            abort_source(state);
            return;
        }
        RE::hkbBehaviorGraphData* const data = behavior->data.get();
        RE::hkbVariableValueSet* const set = behavior->variableValueSet.get();
        if (!data || !set)
        {
            abort_source(state);
            return;
        }
        std::int32_t const count = field_at<std::int32_t>(set, kWordCountOffset);
        std::int32_t const* const words = field_at<std::int32_t const*>(set, kWordDataOffset);
        if (count <= 0 || static_cast<std::size_t>(count) > kMaxWords || !words)
        {
            abort_source(state);
            return;
        }

        SourceTimelineIdentity const source{
            reinterpret_cast<std::uintptr_t>(&actor), reinterpret_cast<std::uintptr_t>(graph),
            reinterpret_cast<std::uintptr_t>(behavior), reinterpret_cast<std::uintptr_t>(data),
            reinterpret_cast<std::uintptr_t>(set), reinterpret_cast<std::uintptr_t>(words),
            static_cast<std::size_t>(count)
        };
        SourceTimelineSample const sample = state.window.observe_source(source);
        if (sample == SourceTimelineSample::e_aborted)
        {
            abort_source(state);
            return;
        }
        if (sample == SourceTimelineSample::e_baseline)
        {
            baseline(state, *behavior, words, source.count, actor.GetFormID());
            return;
        }
        if (state.window.exhausted())
            return;
        sample_variables(state, *behavior, words, static_cast<std::size_t>(count));
        sample_nodes(state, *behavior);
    }
}
PLUGIN_NAMESPACE_END
