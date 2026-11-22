//
// C-G6 v1 (2026-11-22): lifecycle ledger policy.
//
// One bounded row at every lifecycle boundary snapshots the process-wide
// observable counters the contract's accounting table tracks (exception E1
// retained candidate, exception E2 parked graphs, DAC world/epoch scope, panel
// revision). The row proves the boundary ran and gives the offline pass the
// before/after values for the pre-registered G6-C1..C6 criteria; it never
// dereferences native state beyond the counters' own modules.
//
#pragma once

#include <cstddef>
#include <cstdint>

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    // Exception E1 candidate state, filled by independent_graph.cpp (the module
    // that owns the retained handles). `attempted` means the one-shot gate has
    // fired this process; no later allocation is legal.
    struct G6CandidateState
    {
        bool attempted{};
        bool retained{};
        bool held_source_bound{};
        bool step_ready{};
        std::uintptr_t held_graph{};
    };

    // Process-wide ledger snapshot. Every field is read from its owning module
    // on the calling thread; nothing here locks or mutates.
    struct G6Ledger
    {
        std::size_t park_total{};      // exception E2: SCOPY PARK retained-total
        G6CandidateState candidate{};  // exception E1
        std::uint64_t dac_world{};     // dac::world_id
        std::size_t dac_sessions{};    // dac::live_session_count (weak-registry live count)
        std::uint64_t panel_revision{};
        bool panel_open{};
    };

    // One row per boundary. The emitter assembles the snapshot itself from the
    // owning modules (candidate state, parked-graph total, DAC world/sessions,
    // panel revision), so callers pass only the boundary's identity. Phase
    // names: controller-reset / panel-close / lifetime-replace / preload-game /
    // newgame. actor/request carry the boundary's owning request when one
    // exists (zero otherwise).
    void log_g6_ledger(char const* a_phase, std::uint32_t a_actor, std::uint64_t a_request) noexcept;
}
PLUGIN_NAMESPACE_END
