//
// Created by AmazingBuff on 2026/10/24.
//
#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace RE
{
    class BShkbAnimationGraph;
}

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    enum class IndependentGraphMode : std::uint8_t
    {
        off,
        construct,
        load,
        drive,
        inject
    };

    [[nodiscard]] inline std::optional<IndependentGraphMode> parse_independent_graph(std::string_view value)
    {
        if (value == "off") return IndependentGraphMode::off;
        if (value == "construct") return IndependentGraphMode::construct;
        if (value == "load") return IndependentGraphMode::load;
        if (value == "drive") return IndependentGraphMode::drive;
        if (value == "inject") return IndependentGraphMode::inject;
        return std::nullopt;
    }

    [[nodiscard]] inline std::string_view independent_graph_name(IndependentGraphMode mode)
    {
        switch (mode)
        {
        case IndependentGraphMode::construct: return "construct";
        case IndependentGraphMode::load: return "load";
        case IndependentGraphMode::drive: return "drive";
        case IndependentGraphMode::inject: return "inject";
        default: return "off";
        }
    }

    // Runs at most once per process during a paused panel request. Never activates, drives or
    // writes the source graph; loaded candidates remain retained pending lifecycle proof.
    void run_independent_graph_experiment(RE::BShkbAnimationGraph const& source_graph,
        std::uint32_t actor_id, std::uint64_t request, std::uint64_t source_generation);
}
PLUGIN_NAMESPACE_END
