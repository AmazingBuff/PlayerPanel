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
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

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

        struct Recorder
        {
            std::vector<std::int32_t> logged;
            std::vector<std::int32_t> candidate;
            std::vector<std::uint8_t> settled;
            std::vector<std::uint8_t> named;
            std::int32_t node_count = -1;
            std::int32_t node_candidate = -1;
            std::uint8_t node_settled = 0;
            SourceTimelineWindow window;
        };

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
            void* const nodes = a_behavior.activeNodes;
            a_state.node_count = nodes ? field_at<std::int32_t>(nodes, kNodeCountOffset) : 0;
            a_state.node_candidate = a_state.node_count;
            a_state.node_settled = 0;
            logger::info("SOURCE TIMELINE window-open actor={:08x} words={} nodes={} seconds={}", a_actor_id, a_count,
                a_state.node_count, Source_Timeline_Window.count());
        }

        void sample_variables(Recorder& a_state, RE::hkbBehaviorGraph& a_behavior, std::int32_t const* a_words,
            std::size_t a_count)
        {
            for (std::size_t i = 0; i < a_count; ++i)
            {
                std::int32_t const current = a_words[i];
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
                float previous_float = 0.0f;
                float current_float = 0.0f;
                std::memcpy(&previous_float, &previous, sizeof(float));
                std::memcpy(&current_float, &current, sizeof(float));
                char const* name = a_state.named[i] ? nullptr : variable_name(a_behavior, i);
                if (name)
                    a_state.named[i] = 1;
                if (name)
                    logger::info("SOURCE TIMELINE variable index={} name='{}' {}->{} float={:.4f}->{:.4f}", i, name,
                        previous, current, previous_float, current_float);
                else
                    logger::info("SOURCE TIMELINE variable index={} {}->{} float={:.4f}->{:.4f}", i, previous, current,
                        previous_float, current_float);
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
                logger::info("SOURCE TIMELINE nodes count={}", count);
        }
    }

    void arm_source_timeline()
    {
        if (!source_timeline())
            return;
        requests().arm();
        logger::info("SOURCE TIMELINE arm-requested");
    }

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
            logger::info("SOURCE TIMELINE invalidated reason=world-reset");
            return;
        }
        if (intent == SourceTimelineIntent::e_arm)
        {
            state = Recorder{};
            state.window.arm(std::chrono::steady_clock::now());
        }
        if (!state.window.active())
            return;
        if (state.window.expire(std::chrono::steady_clock::now()))
        {
            logger::info("SOURCE TIMELINE window-closed lines={}", state.window.lines());
            return;
        }
        RE::BShkbAnimationGraph* const graph = player_graph(actor);
        if (!graph)
        {
            abort_source(state);
            return;
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
