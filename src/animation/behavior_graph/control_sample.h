//
// C-G5-CAL K2-C4 control B carrier (plan A, 2026-11-14).
//
// Read-only: finds the active MT_Idle clip whose animation bytes match the
// pre-registered authored resource, then samples that animation directly at the
// fixed registered times. It never writes the graph, never advances it and never
// reads poseLocal as an oracle.
#pragma once

#include <cstddef>
#include <cstdint>

namespace RE
{
    class hkbCharacter;
}

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    // Returns true only when exactly one qualified carrier was found and every
    // registered sample was read twice with bitwise-equal results.
    [[nodiscard]] bool run_control_sample(RE::hkbCharacter& a_character, std::uint32_t a_actor_id,
        std::uint64_t a_request, std::size_t a_completed);
}
PLUGIN_NAMESPACE_END
