//
// C-G5-CAL K1 identity census (2026-11-14).
//
// Read-only: dumps the graph/skeleton identity and every active clip's resource
// identity (animation header + spline payload hashes + binding map) and a
// process-local stable clip id. No pose sampling, no graph writes, no new hooks.
#pragma once

#include <cstdint>

namespace RE
{
    class BShkbAnimationGraph;
}

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    // a_dump_skeleton is used once per side (source/candidate post-activate);
    // clip-only phases keep the log bounded.
    void log_k1_census(RE::BShkbAnimationGraph const& a_graph, char const* a_side,
        std::uint32_t a_actor_id, std::uint64_t a_request, char const* a_phase,
        bool a_dump_skeleton);
}
PLUGIN_NAMESPACE_END
