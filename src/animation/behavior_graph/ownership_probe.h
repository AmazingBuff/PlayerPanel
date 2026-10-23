//
// Created by AmazingBuff on 2026/10/23.
//
#pragma once

#include <cstdint>

namespace RE
{
    class BShkbAnimationGraph;
}

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    // Caller retains the source graph and invokes once per newly scheduled request.
    void observe_source_baseline(RE::BShkbAnimationGraph const& graph, std::uint32_t actor_id,
        std::uint64_t request, std::uint64_t source_generation);
}
PLUGIN_NAMESPACE_END
