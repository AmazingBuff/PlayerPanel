//
// Created by AmazingBuff on 2026/10/28.
//

#include "animation/behavior_graph/source_timeline.h"

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
        constexpr std::uint32_t kLineBudget = 200;
        constexpr std::uint8_t kSettleSamples = 3;
        constexpr std::uint64_t kFnvOffset = 0xcbf29ce484222325ull;

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
            std::uint32_t lines = 0;
            bool armed = false;
            bool exhausted = false;
        };

        Recorder& recorder()
        {
            static Recorder s_recorder;
            return s_recorder;
        }

        bool take_line(Recorder& state)
        {
            if (state.exhausted)
                return false;
            if (state.lines < kLineBudget)
            {
                ++state.lines;
                return true;
            }
            state.exhausted = true;
            logger::info("SOURCE TIMELINE budget-exhausted lines={}", kLineBudget);
            return false;
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

        void arm(Recorder& a_state, RE::hkbBehaviorGraph& a_behavior, std::int32_t const* a_words,
            std::size_t a_count, std::uint32_t a_actor_id)
        {
            a_state.armed = true;
            a_state.logged.assign(a_words, a_words + a_count);
            a_state.candidate.assign(a_words, a_words + a_count);
            a_state.settled.assign(a_count, 0);
            a_state.named.assign(a_count, 0);
            void* const nodes = a_behavior.activeNodes;
            a_state.node_count = nodes ? field_at<std::int32_t>(nodes, kNodeCountOffset) : 0;
            a_state.node_candidate = a_state.node_count;
            a_state.node_settled = 0;
            logger::info("SOURCE TIMELINE armed actor={:08x} words={} nodes={}", a_actor_id, a_count, a_state.node_count);
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

    void sample_source_timeline(RE::Actor& actor)
    {
        if (!source_timeline())
            return;
        RE::BShkbAnimationGraph* const graph = player_graph(actor);
        if (!graph)
            return;
        RE::hkbBehaviorGraph* const behavior = graph->behaviorGraph;
        if (!behavior)
            return;
        RE::hkbVariableValueSet* const set = behavior->variableValueSet.get();
        if (!set)
            return;
        std::int32_t const count = field_at<std::int32_t>(set, kWordCountOffset);
        std::int32_t const* const words = field_at<std::int32_t const*>(set, kWordDataOffset);
        if (count <= 0 || static_cast<std::size_t>(count) > kMaxWords || !words)
            return;

        Recorder& state = recorder();
        if (!state.armed)
        {
            arm(state, *behavior, words, static_cast<std::size_t>(count), actor.GetFormID());
            return;
        }
        if (state.logged.size() != static_cast<std::size_t>(count))
        {
            // The graph was replaced or reloaded; start a fresh baseline instead of diffing
            // two different variable layouts.
            logger::info("SOURCE TIMELINE rearm actor={:08x} words={}->{}", actor.GetFormID(), state.logged.size(), count);
            arm(state, *behavior, words, static_cast<std::size_t>(count), actor.GetFormID());
            return;
        }
        sample_variables(state, *behavior, words, static_cast<std::size_t>(count));
        sample_nodes(state, *behavior);
    }
}
PLUGIN_NAMESPACE_END
