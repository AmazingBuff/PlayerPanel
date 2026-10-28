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
        // G3-A keeps its own budgets: the graph has 652 word variables today, and a bounded
        // number of lines keeps one equip/unequip pair readable.
        constexpr std::size_t kMaxWords = 1024;
        constexpr std::uint32_t kLineBudget = 200;
        constexpr std::int32_t kNodeHashEntries = 64;
        constexpr std::uint64_t kFnvOffset = 0xcbf29ce484222325ull;
        constexpr std::uint64_t kFnvPrime = 0x100000001b3ull;

        // hkbVariableValueSet stores its word array behind an hkArray whose data pointer and
        // count are at +0x10 / +0x18; the same offsets the candidate probe already reads.
        constexpr std::size_t kWordDataOffset = 0x10;
        constexpr std::size_t kWordCountOffset = 0x18;
        // hkbBehaviorGraph::activeNodes is an opaque NodeList; the data pointer is at +0 and
        // the count at +8, matching the generation-entry reader.
        constexpr std::size_t kNodeDataOffset = 0x0;
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
            std::vector<std::int32_t> words;
            std::vector<std::uint8_t> named;
            std::int32_t node_count = -1;
            std::uint64_t node_hash = 0;
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

        std::uint64_t mix(std::uint64_t a_hash, std::uint64_t a_value)
        {
            for (std::size_t byte = 0; byte < sizeof(a_value); ++byte)
            {
                a_hash ^= (a_value >> (byte * 8)) & 0xFFull;
                a_hash *= kFnvPrime;
            }
            return a_hash;
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

        void sample_variables(Recorder& a_state, RE::hkbBehaviorGraph& a_behavior, std::int32_t const* a_words,
            std::size_t a_count)
        {
            if (a_state.words.size() != a_count)
            {
                a_state.words.assign(a_words, a_words + a_count);
                a_state.named.assign(a_count, 0);
                return;
            }
            for (std::size_t i = 0; i < a_count; ++i)
            {
                std::int32_t const current = a_words[i];
                if (a_state.words[i] == current)
                    continue;
                std::int32_t const previous = a_state.words[i];
                a_state.words[i] = current;
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
            std::uintptr_t const data = nodes ? field_at<std::uintptr_t>(nodes, kNodeDataOffset) : 0;
            std::uint64_t hash = kFnvOffset;
            if (data && count > 0)
            {
                std::int32_t const limit = std::min(count, kNodeHashEntries);
                for (std::int32_t i = 0; i < limit; ++i)
                    hash = mix(hash, field_at<std::uintptr_t>(reinterpret_cast<void const*>(data), sizeof(std::uintptr_t) *
                        static_cast<std::size_t>(i)));
            }
            if (count == a_state.node_count && hash == a_state.node_hash)
                return;
            a_state.node_count = count;
            a_state.node_hash = hash;
            if (take_line(a_state))
                logger::info("SOURCE TIMELINE nodes count={} hash={:016x}", count, hash);
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
            state.armed = true;
            logger::info("SOURCE TIMELINE armed actor={:08x} words={}", actor.GetFormID(), count);
            state.words.assign(words, words + count);
            state.named.assign(static_cast<std::size_t>(count), 0);
            state.node_count = -1;
            state.node_hash = 0;
            return;
        }
        sample_variables(state, *behavior, words, static_cast<std::size_t>(count));
        sample_nodes(state, *behavior);
    }
}
PLUGIN_NAMESPACE_END
