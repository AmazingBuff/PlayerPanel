//
// C-WORLD v1 (2026-11-25): normal-play Smooth phase sampling.
//
// The panel-side `smooth markers` row only exists inside queue_idle, so nothing
// samples the Smooth phase effects while the panel is closed. This probe answers
// one question only: does normal play ever clear mag_start on (801)?
//
// Contract: docs/diagnostics/world-phase-probe-contract-20261125/README.md
//
#pragma once

namespace RE { class Actor; }
PLUGIN_NAMESPACE_BEGIN
namespace animation
{
    // Per-frame entry. Off by default; when off this is a single settings read and returns.
    // Rate-limited internally to one row per World_Phase_Sample_Ms.
    void sample_world_phase(RE::Actor& actor);
}
PLUGIN_NAMESPACE_END
