//
// Created by AmazingBuff on 2026/10/28.
//
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    struct GraphActivity
    {
        bool active;
        bool update_active_nodes;
    };

    struct OutputSlotObservation
    {
        void const* element;
        std::uint32_t num_tracks;
        std::uint16_t num_data;
        float on_fraction;
    };

    template <class Graph>
    [[nodiscard]] GraphActivity observe_graph_activity(Graph const& graph)
    {
        if (!graph.behaviorGraph) return { false, false };
        return { graph.behaviorGraph->isActive, graph.behaviorGraph->updateActiveNodes };
    }

    template <class T>
    [[nodiscard]] T observation_scalar(void const* base, std::size_t offset)
    {
        T value;
        std::memcpy(&value, static_cast<std::byte const*>(base) + offset, sizeof(value));
        return value;
    }

    [[nodiscard]] inline std::optional<OutputSlotObservation> observe_output_slot(void const* output)
    {
        if (!output) return std::nullopt;
        void const* const element = observation_scalar<void const*>(output, 0);
        if (!element) return std::nullopt;
        return OutputSlotObservation{
            element,
            observation_scalar<std::uint32_t>(element, 0x4),
            observation_scalar<std::uint16_t>(element, 0x32),
            observation_scalar<float>(element, 0x38)
        };
    }
}
PLUGIN_NAMESPACE_END
