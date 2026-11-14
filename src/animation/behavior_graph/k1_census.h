//
// C-G5-CAL K1 identity census (2026-11-14).
//
// Read-only: dumps the graph/skeleton identity and every active clip's resource
// identity (animation header + spline payload hashes + binding map) and a
// process-local stable clip id. No pose sampling, no graph writes, no new hooks.
#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace RE
{
    class BShkbAnimationGraph;
}

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    // Settled equipment-chain variables that trigger a source-side clip census during an
    // F7 window. The list is deliberately narrow: it is the same chain used by D2/D3.
    // Phase-level triggers only. The per-word equipment flags settle in bursts and
    // exhausted the first v101a budget before the unequip phase; these four cover the
    // phase start and the clip-family transitions without redundant snapshots.
    inline constexpr std::array<std::string_view, 4> K1_Census_Trigger_Names{ {
        "IsEquipping",
        "IsUnequipping",
        "currentDefaultState",
        "iState",
    } };

    [[nodiscard]] inline bool k1_census_trigger(std::string_view a_name) noexcept
    {
        for (std::string_view const name : K1_Census_Trigger_Names)
        {
            if (name == a_name)
                return true;
        }
        return false;
    }

    // a_dump_skeleton is used once per side (source/candidate post-activate);
    // clip-only phases keep the log bounded.
    void log_k1_census(RE::BShkbAnimationGraph const& a_graph, char const* a_side,
        std::uint32_t a_actor_id, std::uint64_t a_request, char const* a_phase,
        bool a_dump_skeleton);
}
PLUGIN_NAMESPACE_END
