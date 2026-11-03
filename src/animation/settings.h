//
// Created by AmazingBuff on 2026/10/20.
//
#pragma once
#include "animation/behavior_graph/independent_graph.h"
#include "animation/behavior_graph/probe_liveness_policy.h"
#include "animation/behavior_graph/source_event_capture_policy.h"
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
    // G3-A: log the player graph's variable/active-node changes during normal play.
    [[nodiscard]] bool source_timeline();
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
    [[nodiscard]] behavior_graph::ProbeLivenessMode sampling_liveness_mode();
}
PLUGIN_NAMESPACE_END
