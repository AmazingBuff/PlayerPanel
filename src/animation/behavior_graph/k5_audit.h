//
// C-G5-CAL K5 layer audit (2026-11-14 implementation pre-registration §10.4).
//
// Read-only: walks the candidate's active-node list at the four pre-registered
// audit points, classifies every node by exact vtable match, logs one K5 LAYER
// row per node (clip rows carry the full resource identity through the shared
// K5IdentityCache) and one K5 AUDIT summary row. No graph writes, no
// advancement, no pose sampling. hkbBlendingBehavior has no CommonLibSSE layout
// or VTABLE constant, so its weights surface as `unknown` and are registered as
// the explicit K5-5 gap, not guessed offsets.
#pragma once

#include "animation/behavior_graph/k5_snapshot_policy.h"

#include <cstdint>

namespace RE
{
    class BShkbAnimationGraph;
}

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    inline constexpr std::size_t K5_Audit_Node_Limit = 512;
    inline constexpr std::size_t K5_Audit_Cross_Limit = 256;

    // a_point is the pre-registered audit point number 0..3; a_phase is the row label
    // (post-activate / equip-mid / equip-settle / unequip-complete). Each call owns a
    // fresh K5IdentityCache: at most a handful of clips per point, so the per-record
    // cache (which lives beside the snapshot writer) is not needed here. The log_ prefix
    // keeps the settings getter animation::k5_layer_audit() unambiguous.
    void log_k5_layer_audit(RE::BShkbAnimationGraph const& a_graph, std::uint32_t a_actor_id,
        std::uint64_t a_request, char const* a_phase, std::uint32_t a_point);
}
PLUGIN_NAMESPACE_END
