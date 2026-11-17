//
// Created by AmazingBuff on 2026/10/20.
//
#pragma once
#include "animation/behavior_graph/independent_graph.h"
#include "animation/behavior_graph/probe_liveness_policy.h"
#include "animation/behavior_graph/source_event_capture_policy.h"
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
    // C-G3B-E1 / C-G4-C1: install a read-only 58377 observer. `on` logs the source graph's
    // dispatched events during an F7 window (requires IndependentGraph=off; SourceTimeline wins
    // when both are on). `concurrent` is armed and closed by the candidate experiment/display
    // path instead, and may coexist with IndependentGraph=inject.
    [[nodiscard]] bool source_event_capture();
    // C-G4-C1: the parsed three-state mode and the concurrent predicate. `source_event_capture()`
    // is true for both `on` and `concurrent`.
    [[nodiscard]] behavior_graph::SourceEventCaptureMode source_event_capture_mode();
    [[nodiscard]] bool source_event_capture_concurrent();
    // C-G3B-E2: install read-only observers on the typed graph setters (Bool/Int) and log every
    // write to the player's graphs during an F7 window. Requires IndependentGraph=off; the
    // other source instruments win when both are on.
    [[nodiscard]] bool source_write_capture();
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
    // C-G3B-S3: once the equip phase settles (currentDefaultState==1 held for three consecutive
    // samples inside the first 120 driven steps), dispatch 1HM_Unequip once through the same
    // event path as the activation injection and sample the unequip phase for another 121 steps.
    // Requires IndependentGraph=inject.
    [[nodiscard]] bool candidate_unequip_probe();
    // C-G3B-S4: the unequip slice's second-stage input. Non-empty selects a by-name typed
    // variable write at the settle point (the pre-registered ladder bEquipUnequip ->
    // IsUnequipping -> PitchManualOverride, one INI-only round per name); empty keeps the S3
    // 1HM_Unequip event dispatch.
    [[nodiscard]] std::string const& candidate_unequip_variable();
    // C-G3B-S5: an optional prime write before the unequip command — the pre-registered
    // sequence writes iRightHandEquipped=1 first (completing the real equipped state the
    // WeapEquip path skips), then the CandidateUnequipVariable command. Empty keeps the S4
    // single-write behaviour.
    [[nodiscard]] std::string const& candidate_unequip_prime();
    // C-G3B-S6: the unequip slice's second-stage event name when CandidateUnequipVariable is
    // empty. The C-G3B-A1 structural audit resolved the Weap_Readied_State 1->12 transition to
    // the event "Unequip" (id 144 in the 0_Master table) with its condition flag disabled; S3's
    // hard-coded "1HM_Unequip" stays as the empty-key default so historical INI files reproduce
    // their rounds verbatim.
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
    // C-G5-CAL K2-C4 control B: read-only, fixed-time sample of the registered MT_Idle
    // animation through the candidate's loaded binding. Requires IndependentGraph != off;
    // defaults off and never writes or advances the graph.
    [[nodiscard]] bool control_sample();
    // C-G5-CAL2 control grid (2026-11-15): per-generation sampling of the held
    // carrier at the predetermined grid times during the equip phase. Requires
    // IndependentGraph=inject; defaults off and never writes or advances the graph.
    [[nodiscard]] bool control_sample_grid();
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
    // C-G5-STATE v2 (2026-11-17, default off): one-time source poseLocal bootstrap into
    // the activated candidate, before any injection/drive. Copies all candidate tracks
    // once; no per-frame write. Requires IndependentGraph=drive|inject.
    [[nodiscard]] bool bootstrap_source_pose();
    [[nodiscard]] behavior_graph::ProbeLivenessMode sampling_liveness_mode();
}
PLUGIN_NAMESPACE_END
