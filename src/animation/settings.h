//
// Created by AmazingBuff on 2026/10/20.
//
#pragma once
#include "animation/behavior_graph/independent_graph.h"
#include "animation/behavior_graph/probe_liveness_policy.h"
#include "animation/behavior_graph/source_timeline_policy.h"
#include "animation/idle_policy.h"

#include <cstdint>
#include <string>
PLUGIN_NAMESPACE_BEGIN
namespace animation
{
    void load_settings();
    [[nodiscard]] UnarmedIdle unarmed_idle_setting();
    [[nodiscard]] bool detailed_animation_trace();
    [[nodiscard]] bool behavior_graph_probe();
    [[nodiscard]] behavior_graph::IndependentGraphMode independent_graph_setting();
    // v27: when true and a candidate is retained, the panel's display pose comes from that
    // candidate instead of the OAR clip sampler.
    [[nodiscard]] bool display_pose();
    // Pace fixed 1/60 steps by eligible elapsed time, capped at 100 ms / 6 steps per display call.
    [[nodiscard]] bool display_step();
    // G3-A / C-G5-E3: log the source graph's variable/active-node changes. `on` keeps the F7
    // lifecycle outside the panel; `concurrent` is armed by the candidate experiment with the
    // pinned source graph and closed at the display boundary. `source_timeline()` is true for
    // both states so the existing callers keep their meaning.
    [[nodiscard]] bool source_timeline();
    [[nodiscard]] behavior_graph::SourceTimelineMode source_timeline_mode();
    [[nodiscard]] bool source_timeline_concurrent();
    // C-G5-E3 v2: `on`-only graph selector by project name. Empty keeps the legacy "last managed
    // non-1stperson graph" rule; a value that matches nothing is logged and falls back to it.
    [[nodiscard]] std::string const& source_timeline_graph();
    // C-G5-E4: capture the source graph's poseLocal and clip clocks at settle anchors. Requires
    // SourceTimeline; the probe instruments stay off in the same process (no stacking).
    [[nodiscard]] bool source_pose_capture();
    // C-G3B-E3: transient mode for the source timeline - BOOL/INT-typed variables log every
    // observed word change immediately (no settle rule), catching one-frame command writes
    // the settle-3 rule filters out. Requires SourceTimeline.
    [[nodiscard]] bool source_transient_watch();
    // C-VC1: one-shot read-only dump of the candidate's variable table, aligned by name against
    // the source. Requires IndependentGraph != off; writes nothing and calls no native function.
    [[nodiscard]] bool candidate_variable_census();
    // C-G3B-S1: after a successful candidate activation, set iRightHandEquipped=1 once through the
    // graph's own typed setter and sample the phase variables. Requires IndependentGraph=drive|inject.
    [[nodiscard]] bool candidate_input_probe();
    // C-G3B-S2: the single probe input. The name selects the variable; the setter follows that
    // variable's own m_type (BOOL -> bool setter, INT8/16/32 -> int setter).
    [[nodiscard]] std::string const& candidate_input_variable();
    [[nodiscard]] std::int32_t candidate_input_value();
    // C-G5-CAL D2: before the inject event, prime the three equipment words the source observed
    // during a real 1H equip (iMagicEquipped=1, iRightHandEquipped=1, iLeftHandEquipped=9).
    // Requires IndependentGraph=inject; a missing/unsupported/failed write skips the event.
    [[nodiscard]] bool candidate_equip_prime();
    // C-G5-CAL D3: additionally prime iRightHandType=1 and iLeftHandType=9, the source values
    // captured at window-open on 2026-11-11. Kept separate so D2's three-word profile remains
    // reproducible; requires CandidateEquipPrime=true.
    [[nodiscard]] bool candidate_equip_prime_branch();
    // C-G3B-S1: let drive mode run the engine generation entry so the candidate actually produces
    // output. inject already implies it; keeping it explicit and off by default leaves historical
    // drive runs comparable.
    [[nodiscard]] bool candidate_generation_entry();
    // After equip settles, dispatch the unequip event once and observe the resulting state.
    [[nodiscard]] bool candidate_unequip_probe();
    [[nodiscard]] std::string const& candidate_unequip_event();
    // C-G4-A: while native candidate activity is armed, count and log OAR condition-evaluation
    // detour hits (OAR CONTEXT rows) and emit the display-branch suppression row. Default off;
    // restart after changing.
    [[nodiscard]] bool oar_context_watch();
    // C-G5-E1: three-state minimal liveness probe on the sampling entry (63589) and the poseLocal
    // last writer (63856). `on` opens an F7 window outside the panel; `concurrent` is armed by the
    // candidate experiment before its own dispatches and closed at the display boundary.
    [[nodiscard]] bool sampling_liveness();
    // C-G5-CAL K3-C: write the per-generation full candidate snapshot artifact.
    [[nodiscard]] bool observation_snapshot();
    // C-G5-CAL K1 (2026-11-14): one-shot read-only graph/skeleton/clip identity census.
    // Requires IndependentGraph != off; defaults off and never writes or advances the graph.
    [[nodiscard]] bool clip_identity_census();
    // C-G5-CAL K2-C1 (2026-11-14): read-only worn-armor snapshot appended to the
    // ANIMATION WORLD rows. `armor=slots=31 worn=N <slot>:<formid>,…` enumerates every
    // BipedObjectSlot mask bit via Actor::GetWornArmor; defaults off. With the key off the
    // row is byte-identical to the historical format, so past runs stay comparable.
    [[nodiscard]] bool equipment_state_dump();
    // C-G5-CAL K5 (2026-11-14, K6 plan §10): switch the per-generation candidate snapshot
    // (and the per-apply source snapshot under SamplingLiveness) to the CPK5 format v1,
    // which adds per-clip resource identity columns (stable-id pointers, duration bits,
    // data/blockOffsets/map hashes) to the K3 layout. Shares every trigger point and the
    // two-read rule with ObservationSnapshot; when both are on the K3 writer is suppressed
    // with a logged declaration row. Defaults off; with the key off the output is
    // byte-identical to the historical behaviour.
    [[nodiscard]] bool k5_full_sample();
    // C-G5-CAL K5: at the four pre-registered audit points (post-activate, equip mid,
    // equip settle, unequip complete) walk the candidate's active-node list, classify
    // every node by exact vtable match and log one K5 LAYER row per node plus one K5
    // AUDIT summary row. Read-only; defaults off.
    [[nodiscard]] bool k5_layer_audit();
    // C-G5-CAL K5: emit the K5 RECON reconciliation row and JSON ledger artifact when the
    // source-side F7 window closes. The candidate side always reconciles when K5FullSample
    // is on, so this key only governs the source window. Defaults off.
    [[nodiscard]] bool k5_reconciliation();
    // C-G5-SETUP v1 (2026-11-17, default off): one-shot read-only setup/skeleton
    // reference-pose audit after the candidate loads and before activation. Logs the
    // source and candidate setup/skeleton pointers and the 12 fixed bone bind transforms;
    // writes no graph/pose state and installs no hook.
    [[nodiscard]] bool setup_audit();
    // C-G5-TRANS v1 (2026-11-18, default off): read-only transition/blender state probe at the
    // existing K5 candidate/source points. Requires K5FullSample; adds no detour and writes no
    // graph, pose or variable.
    [[nodiscard]] bool transition_state_probe();
    // C-G5-TRANS v5 (2026-11-19, default off): candidate-only, log-only follow of
    // hkbBehaviorReferenceGenerator::behavior (+0x50) into the referenced hkbBehaviorGraph.
    // Requires TransitionStateProbe and K5FullSample; no writes and no new hooks.
    [[nodiscard]] bool transition_subgraph_probe();
    // C-G5-TRANS v6: candidate-only equipment parent chain, default off.
    [[nodiscard]] bool transition_parent_chain_probe();
    // C-G5-CHAIN v1 (2026-11-21, default off): idle-settle three-way comparison at the two F7
    // display checkpoints — each side's poseLocal against a direct SamplePartialTracks of that
    // side's active clip at that side's own localTime, plus a bounded active-node census of
    // both graphs. Read-only; no writes, no hooks, no new threads.
    [[nodiscard]] bool idle_chain_probe();
    // C-G6 v1 (2026-11-22, default off): one bounded lifecycle ledger row at every
    // session/save boundary — process-wide counters only (parked-graph total,
    // retained-candidate state, DAC world/sessions, panel revision). Read-only.
    [[nodiscard]] bool g6_ledger_enabled();
    // C-G6-CON v1 (2026-11-22, default off): CON-A domain sync — read the ten
    // pre-registered driver words from the source and write them into the
    // candidate's own variable set through the typed setters when the signature
    // changes. Writes candidate only; requires IndependentGraph=inject.
    [[nodiscard]] bool domain_sync_enabled();
    // C-G6-CON v2 (2026-11-22, default off): CON-A' directional event dispatch —
    // when the domain signature changes, prime first then dispatch the
    // pre-registered direction event (WeapEquip/Unequip) into the candidate via
    // the 58377 entry. Requires domain_sync_enabled; writes candidate only.
    [[nodiscard]] bool domain_dispatch_enabled();
    // C-WORLD v1 (2026-11-25, default off): sample the source actor's Smooth phase
    // markers from the per-frame hook, so normal play is observable while the panel
    // is closed. Read-only; answers whether normal play ever clears mag_start on.
    // Contract: docs/diagnostics/world-phase-probe-contract-20261125/README.md
    [[nodiscard]] bool world_phase_probe();
    [[nodiscard]] behavior_graph::ProbeLivenessMode sampling_liveness_mode();
}
PLUGIN_NAMESPACE_END
