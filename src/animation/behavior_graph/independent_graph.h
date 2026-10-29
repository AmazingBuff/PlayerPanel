//
// Created by AmazingBuff on 2026/10/24.
//
#pragma once

#include "animation/behavior_graph/display_state_policy.h"
#include "RE/N/NiTransform.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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
        DisplaySourceIdentity source, std::uint64_t request);

    // Invalidates the view only; the loaded candidate remains retained without native teardown.
    void invalidate_display_owner(std::uintptr_t owner);

    class DisplaySession
    {
    public:
        DisplaySession();
        ~DisplaySession();
        void request(DisplaySourceIdentity source, std::uint64_t request);
        void retarget();

        [[nodiscard]] bool held_display_pose(RE::BShkbAnimationGraph const& source_graph,
            std::vector<std::pair<std::string, RE::NiTransform>>& out);
        // Timing waits still return a valid stepped-mode pose; only due ticks run native steps.
        [[nodiscard]] bool step_display_pose(RE::BShkbAnimationGraph const& source_graph,
            std::vector<std::pair<std::string, RE::NiTransform>>& out);
        void record_application(bool stepped, std::size_t output_records, DisplayApplicationCoverage const& coverage);

    private:
        [[nodiscard]] bool read_held_display_pose(RE::BShkbAnimationGraph const& source_graph,
            std::vector<std::pair<std::string, RE::NiTransform>>& out);
        class State;
        std::unique_ptr<State> m_state;
    };
}
PLUGIN_NAMESPACE_END
