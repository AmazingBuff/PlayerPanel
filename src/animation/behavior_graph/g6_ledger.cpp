//
// C-G6 v1 (2026-11-22): lifecycle ledger row emitter. See g6_ledger.h.
//

#include "animation/behavior_graph/g6_ledger.h"

#include "animation/dac/session.h"
#include "animation/settings.h"
#include "character/scene_graph.h"
#include "panel/panel_monitor.h"

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
    void log_g6_ledger(char const* const a_phase, std::uint32_t const a_actor,
        std::uint64_t const a_request) noexcept
    {
        if (!g6_ledger_enabled()) return;
        G6Ledger ledger;
        ledger.park_total = parked_graph_total();
        ledger.candidate = g6_candidate_state();
        ledger.dac_world = dac::world_id();
        ledger.dac_sessions = dac::live_session_count();
        ledger.panel_revision = PanelMonitor::instance().revision();
        ledger.panel_open = PanelMonitor::instance().is_menu_open();
        logger::info("G6 LEDGER phase={} actor={:08x} request={} park={} candidate={} held-source={} "
            "step-ready={} candidate-graph={:#x} dac-world={} dac-sessions={} panel-rev={} panel-open={}",
            a_phase, a_actor, a_request, ledger.park_total,
            ledger.candidate.attempted ? (ledger.candidate.retained ? "retained" : "attempted") : "unattempted",
            ledger.candidate.held_source_bound ? "bound" : "unbound",
            ledger.candidate.step_ready ? 1 : 0, ledger.candidate.held_graph,
            ledger.dac_world, ledger.dac_sessions, ledger.panel_revision,
            ledger.panel_open ? 1 : 0);
    }
}
PLUGIN_NAMESPACE_END
