//
// Created by AmazingBuff on 2026/10/28.
//

#include "animation/behavior_graph/source_timeline.h"

#include "animation/behavior_graph/pose_capture_policy.h"
#include "animation/behavior_graph/source_timeline_probe_policy.h"
#include "animation/behavior_graph/source_timeline_policy.h"

#include "animation/settings.h"

#include <RE/A/Actor.h>
#include <RE/B/BSAnimationGraphManager.h>
#include <RE/B/BShkbAnimationGraph.h>
#include <RE/H/hkbBehaviorGraph.h>
#include <RE/H/hkbBehaviorGraphData.h>
#include <RE/H/hkbBehaviorGraphStringData.h>
#include <RE/H/hkbClipGenerator.h>
#include <RE/H/hkbVariableValueSet.h>
#include <RE/H/hkStringPtr.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
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
            // C-G5-E3 v2 §八: the `on` census prints once per window, before the first sample.
            bool census_logged{ false };
            // C-G5-E4: source-side pose/clock capture state. `snapshot_armed` means a settle is
            // waiting for its anchor sample; the anchor rule is "first post-UI sample after it".
            bool pose_capture{ false };
            SourceCaptureBudget capture;
            std::uint32_t frame{ 0 };
            bool snapshot_armed{ false };
            std::uint64_t anchor_tsc{ 0 };
            std::uint32_t anchor_frame{ 0 };
            std::chrono::steady_clock::time_point armed_at{};
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

        // C-G5-E3 v2 §八: one actor can own several managed non-1stperson graphs. Collect them
        // once so the census and the selector see the same list; `found` stays exact even when the
        // stored list is truncated.
        inline constexpr std::size_t kMaxManagedGraphs = 8;

        struct ManagedGraphs
        {
            std::array<RE::BShkbAnimationGraph*, kMaxManagedGraphs> graphs{};
            std::array<std::string_view, kMaxManagedGraphs> names{};
            std::size_t found{ 0 };
            std::size_t stored{ 0 };
        };

        ManagedGraphs collect_managed_graphs(RE::Actor& actor)
        {
            ManagedGraphs out{};
            RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
            if (!actor.GetAnimationGraphManager(manager) || !manager)
                return out;
            for (RE::BSTSmartPointer<RE::BShkbAnimationGraph> const& graph : manager->graphs)
            {
                if (!graph || !graph->behaviorGraph || graph->holder != &actor)
                    continue;
                char const* const name = graph->projectName.c_str();
                if (name && source_graph_is_first_person(std::string_view{ name }))
                    continue;
                if (out.stored < kMaxManagedGraphs)
                {
                    out.graphs[out.stored] = graph.get();
                    out.names[out.stored] = name ? std::string_view{ name } : std::string_view{};
                    ++out.stored;
                }
                ++out.found;
            }
            return out;
        }

        // Read-only census: every managed non-1stperson graph with the counts the pairing contract
        // compares (`words` from the variable set, `nodes` from the active-node list), plus which
        // one the selector picks and through which rule.
        void log_graph_census(RE::Actor& actor, std::uint32_t a_actor_id, std::string_view a_selector)
        {
            ManagedGraphs const managed = collect_managed_graphs(actor);
            std::size_t const picked = source_timeline_pick(managed.names.data(), managed.stored, a_selector);
            char const* const origin = a_selector.empty() ? "last" :
                (picked == Source_Timeline_No_Pick ? "selector-miss-fallback" : "selector");
            std::size_t const chosen = picked != Source_Timeline_No_Pick ? picked :
                (managed.stored != 0 ? managed.stored - 1 : Source_Timeline_No_Pick);
            logger::info("SOURCE TIMELINE graph-census actor={:08x} found={} shown={} selector='{}' origin={} chosen={}",
                a_actor_id, managed.found, managed.stored, a_selector, origin, chosen);
            for (std::size_t i = 0; i < managed.stored; ++i)
            {
                RE::hkbBehaviorGraph const* const behavior = managed.graphs[i] ? managed.graphs[i]->behaviorGraph : nullptr;
                RE::hkbVariableValueSet const* const set = behavior ? behavior->variableValueSet.get() : nullptr;
                std::int32_t const words = set ? field_at<std::int32_t>(set, kWordCountOffset) : -1;
                void* const nodes = behavior ? behavior->activeNodes : nullptr;
                std::int32_t const node_count = nodes ? field_at<std::int32_t>(nodes, kNodeCountOffset) : -1;
                logger::info("SOURCE TIMELINE graph-census index={} name='{}' words={} nodes={} selected={}",
                    i, managed.names[i], words, node_count, i == chosen ? "yes" : "no");
            }
        }

        // Legacy rule: the last managed non-1stperson graph. With a selector, the matching graph
        // wins; a selector that matches nothing falls back to the legacy rule, so a typo can never
        // silently swap the observed graph (the census reports which rule fired).
        RE::BShkbAnimationGraph* player_graph(RE::Actor& actor, std::string_view a_selector)
        {
            ManagedGraphs const managed = collect_managed_graphs(actor);
            if (managed.stored == 0)
                return nullptr;
            std::size_t const picked = source_timeline_pick(managed.names.data(), managed.stored, a_selector);
            if (picked != Source_Timeline_No_Pick)
                return managed.graphs[picked];
            return managed.graphs[managed.stored - 1];
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

        // C-G5-E4: the active clip table, typed access only (NodeList -> nodeClone -> vtable gate),
        // bounded at 256 entries and at the caller's slot count.
        std::size_t collect_source_clips(RE::BShkbAnimationGraph& a_graph, RE::hkbClipGenerator** a_out,
            std::size_t a_capacity)
        {
            if (a_out == nullptr || a_capacity == 0)
                return 0;
            RE::hkbBehaviorGraph* const behavior = a_graph.behaviorGraph;
            if (!behavior || !behavior->activeNodes)
                return 0;
            RE::NodeList const& nodes = *behavior->activeNodes;
            std::size_t const active = static_cast<std::size_t>(nodes.size());
            std::size_t const count = active < 256 ? active : 256;
            std::size_t found = 0;
            for (std::size_t i = 0; i < count; ++i)
            {
                RE::hkbNode* const node = nodes.data()[i].nodeClone;
                if (!node)
                    continue;
                if (*reinterpret_cast<std::uintptr_t const*>(node) != RE::VTABLE_hkbClipGenerator[0].address())
                    continue;
                if (found < a_capacity)
                    a_out[found] = static_cast<RE::hkbClipGenerator*>(node);
                ++found;
            }
            return found;
        }

        std::string_view clip_leaf_name(RE::hkbClipGenerator const& a_clip)
        {
            char const* const name = a_clip.animationName.c_str();
            if (!name)
                return {};
            std::string_view view{ name };
            std::size_t const slash = view.find_last_of("\\/");
            return slash == std::string_view::npos ? view : view.substr(slash + 1);
        }

        // One anchor per settle. Several variables settle on the same frame for one input, so a
        // same-frame repeat is the same anchor; a settle while an anchor is still pending breaks the
        // one-snapshot-per-settle rule and is counted.
        void arm_source_snapshot(Recorder& a_state, std::uint64_t a_tsc)
        {
            if (!a_state.pose_capture)
                return;
            if (a_state.snapshot_armed)
            {
                if (a_state.anchor_frame != a_state.frame)
                    a_state.capture.drop_snapshot();
                return;
            }
            a_state.snapshot_armed = true;
            a_state.anchor_tsc = a_tsc;
            a_state.anchor_frame = a_state.frame;
        }

        void emit_pose_snapshot(Recorder& a_state, RE::BShkbAnimationGraph& a_graph, std::uint32_t a_actor_id)
        {
            a_state.snapshot_armed = false;
            RE::hkbCharacter& character = a_graph.characterInstance;
            RE::hkQsTransform const* const pose = character.poseLocal;
            std::int32_t const n = character.numPoseLocal;
            std::uint32_t const delta = a_state.frame >= a_state.anchor_frame ? a_state.frame - a_state.anchor_frame : 0;
            if (pose == nullptr || n <= 0 || n > static_cast<std::int32_t>(Source_Capture_Max_Tracks))
            {
                a_state.capture.drop_snapshot();
                logger::info("SOURCE TIMELINE pose-snapshot actor={:08x} status=unavailable reason=bad-pose pose-local={:#x} n={}",
                    a_actor_id, reinterpret_cast<std::uintptr_t>(pose), n);
                return;
            }
            RE::hkbClipGenerator* clips[Source_Capture_Max_Clips]{};
            std::size_t const found = collect_source_clips(a_graph, clips, Source_Capture_Max_Clips);
            std::size_t const shown = found < Source_Capture_Max_Clips ? found : Source_Capture_Max_Clips;
            std::uint32_t const lines = 2u + static_cast<std::uint32_t>(n) + static_cast<std::uint32_t>(shown);
            if (!a_state.capture.take_snapshot(lines))
            {
                logger::info("SOURCE TIMELINE pose-snapshot actor={:08x} status=dropped frame={} snapshots={} lines={} line-dropped={}",
                    a_actor_id, a_state.frame, a_state.capture.snapshots(), a_state.capture.lines(), a_state.capture.line_dropped());
                return;
            }
            logger::info("SOURCE TIMELINE pose-snapshot actor={:08x} frame={} anchor-delta-frames={} anchor-tsc={} pose-local={:#x} n={} clips={}/{} lines={}",
                a_actor_id, a_state.frame, delta, a_state.anchor_tsc, reinterpret_cast<std::uintptr_t>(pose), n, shown, found,
                a_state.capture.lines());
            for (std::int32_t i = 0; i < n; ++i)
            {
                logger::info("{}", format_source_pose_track(i, capture_pose_sample(pose[i])));
            }
            for (std::size_t i = 0; i < shown; ++i)
            {
                RE::hkbClipGenerator const& clip = *clips[i];
                logger::info("SOURCE TIMELINE pose-snapshot clip name='{}' time={:.4f} mode={} speed={:.3f} enforced={:.4f}",
                    clip_leaf_name(clip), clip.localTime, static_cast<std::uint32_t>(clip.mode.get()),
                    clip.playbackSpeed, clip.enforcedDuration);
            }
        }

        void emit_clock_sample(Recorder& a_state, RE::BShkbAnimationGraph& a_graph, std::uint32_t a_actor_id)
        {
            RE::hkbClipGenerator* clips[Source_Capture_Max_Clips]{};
            std::size_t const found = collect_source_clips(a_graph, clips, Source_Capture_Max_Clips);
            std::size_t const shown = found < Source_Capture_Max_Clips ? found : Source_Capture_Max_Clips;
            if (!a_state.capture.take_clock(1))
            {
                logger::info("SOURCE TIMELINE clock-sample actor={:08x} status=dropped frame={} clocks={}",
                    a_actor_id, a_state.frame, a_state.capture.clocks());
                return;
            }
            std::string entries;
            for (std::size_t i = 0; i < shown; ++i)
            {
                if (i != 0)
                    entries += '|';
                char buffer[96]{};
                (void)std::snprintf(buffer, sizeof(buffer), "%.*s:%.4f",
                    static_cast<int>(clip_leaf_name(*clips[i]).size()), clip_leaf_name(*clips[i]).data(),
                    clips[i]->localTime);
                entries += buffer;
            }
            std::uint64_t const elapsed_ms = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - a_state.armed_at).count());
            logger::info("SOURCE TIMELINE clock-sample actor={:08x} frame={} ms={} found={} entries='{}'",
                a_actor_id, a_state.frame, elapsed_ms, found, entries);
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

        // C-G5-CAL D3 v2: transient mode records changes only, so read the four branch-selection
        // words once at window-open. This is read-only, runs before the baseline changes anything,
        // and adds one line that does not consume the event budget.
        void log_initial_probe(RE::hkbBehaviorGraph const& a_behavior, std::int32_t const* a_words,
            std::size_t a_count, std::uint32_t a_actor_id)
        {
            std::string values;
            for (std::string_view const wanted : Source_Timeline_Probe_Names)
            {
                std::size_t index = a_count;
                for (std::size_t i = 0; i < a_count; ++i)
                {
                    char const* const name = variable_name(a_behavior, i);
                    if (name && wanted == name)
                    {
                        index = i;
                        break;
                    }
                }
                if (index < a_count)
                    values += fmt::format("{}={} ", wanted, a_words[index]);
                else
                    values += fmt::format("{}=missing ", wanted);
            }
            logger::info("SOURCE TIMELINE probe actor={:08x} values='{}'", a_actor_id, values);
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
            if (source_transient_watch())
                log_initial_probe(a_behavior, a_words, a_count, a_actor_id);
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
                char const* const anchor_name = variable_name(a_behavior, i);
                if (anchor_name != nullptr && source_settle_is_anchor(std::string_view{ anchor_name }, previous, current))
                    arm_source_snapshot(a_state, settle_tsc);
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
            state.pose_capture = source_pose_capture();
            state.armed_at = std::chrono::steady_clock::now();
            state.window.arm(state.armed_at);
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
            state.pose_capture = source_pose_capture();
            state.armed_at = std::chrono::steady_clock::now();
            state.window.arm(state.armed_at);
            logger::info("SOURCE TIMELINE concurrent-arm actor={:08x} request={} graph={:#x}",
                arm.actor.load(std::memory_order_relaxed), state.request, state.pinned_graph);
        }
        if (g_close_pending.exchange(false, std::memory_order_acquire) && state.window.active())
        {
            std::uint32_t const lines = state.window.lines();
            state.window.cancel();
            logger::info("SOURCE TIMELINE window-closed reason={} lines={} settles-pending={} settles-dropped={} snapshots={} clocks={} capture-lines={} snapshot-dropped={} clock-dropped={} line-dropped={}",
                g_close_reason, lines, settle_ring().size(), settle_ring().dropped(), state.capture.snapshots(),
                state.capture.clocks(), state.capture.lines(), state.capture.snapshot_dropped(),
                state.capture.clock_dropped(), state.capture.line_dropped());
        }
        if (!state.window.active())
            return;
        if (state.window.expire(std::chrono::steady_clock::now()))
        {
            logger::info("SOURCE TIMELINE window-closed reason=expired lines={} snapshots={} clocks={} capture-lines={} snapshot-dropped={} clock-dropped={} line-dropped={}",
                state.window.lines(), state.capture.snapshots(), state.capture.clocks(), state.capture.lines(),
                state.capture.snapshot_dropped(), state.capture.clock_dropped(), state.capture.line_dropped());
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
            // C-G5-E3 v2 §八: the `on` lifecycle owns the graph choice, so the census prints once
            // before the first sample and the selector decides which managed graph is observed.
            if (!state.census_logged)
            {
                log_graph_census(actor, actor.GetFormID(), source_timeline_graph());
                state.census_logged = true;
            }
            graph = player_graph(actor, source_timeline_graph());
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

        // C-G5-E4: the capture runs after this frame's apply, so `poseLocal` already holds the
        // completed output. The snapshot goes out on the first sample after a settle was armed;
        // clock samples are taken on a fixed stride.
        ++state.frame;
        if (state.pose_capture)
        {
            if (state.snapshot_armed)
                emit_pose_snapshot(state, *graph, actor.GetFormID());
            else if (source_clock_sample_due(state.frame))
                emit_clock_sample(state, *graph, actor.GetFormID());
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
