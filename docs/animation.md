# Actor animation previews

## Independent behavior graph validation (2026-10-29)

The default player below continues to select/sample OAR clips on the copied SceneGraph. G1 passes revised Option A. [V33/V34](independent-behavior-graph.md#verification-matrix) establish the bounded G2 display result: semantic motion 120/120, 109/116 actual assignments, unchanged source controls and user-confirmed continuous motion, plus reset/reopen/F7/post-load fallback. The old seven misses have four logged names and three unknown; control/attachment labels do not prove harmless omissions. Exact old rejection cause, same-controller source/generation replacement and frozen-only rejection remain unverified. [V41 G3-A](independent-behavior-graph.md#g3-a-contract) successfully records a named settled phase trajectory; G3-B requires verified role/type mapping and justified external inputs, with `iState`/`currentDefaultState` observation-only. G4–G6 remain open.

The latest recorded install is v41 (`3b8f8efe…`), SourceTimeline=true and IndependentGraph/DisplayPose/DisplayStep off; repository defaults keep all four off. [V42](independent-behavior-graph.md#v42-residual-corrections) is local only: cap8 missing names and one rejection row under the existing 18-row window budget, without new native calls or game results. G6 retains source restoration, request/task invalidation, preview references/callbacks/sinks/history and observable repeated-session convergence. One loaded candidate retained per process is an explicit exception; loaded teardown remains NO-GO and no valid global registry count anchor exists. DAC restore matched is partial evidence.

[V43 source-recorder sessions](independent-behavior-graph.md#v43-source-sessions) are local only: every explicit F7 arm clears baselines/settle state and gets a fresh 20-second/800-line window. Lost or changed actor/graph/data/set/storage identities or count abort the trace; PreLoadGame/NewGame queues cancellation before source reads. A later explicit F7 is required. The original settle3/raw-value rule remains; identity snapshots establish neither writer roles nor exact setter timing nor atomic native arrays. [Two engine-free cases and Release build](build.md#v43-source-sessions-2026-10-29) passed without new game evidence; the recorded install remains v41.

[Audit27](behavior-graph-construction-audit.md#audit27) proves at least one static native integer-setter call site for each of `iMagicEquipped`, `iLeftHandEquipped` and `iRightHandEquipped`. Runtime variableInfos types/source-candidate mapping, exclusive writers, actual v41 execution, event timing and a necessary/sufficient input subset remain unverified. This adds no replay authority; the other equip flags remain unresolved and G3-B remains pending.

`BehaviorGraphProbe=false` is a separate sixteen-row source observer. Its fixed `candidate=none/gate1=not-passed` labels describe that observation, not the current G1 verdict. `IndependentGraph` defaults to `off`; construction/drive runs at most once per process on panel open. Loaded candidates remain retained because teardown is NO-GO under the current shared-root ownership evidence.

With `DisplayPose=true`, `AnimationController::advance` first asks its owned `DisplaySession` for a valid retained candidate pose. `DisplayStep=true` uses the [v44 elapsed clock](independent-behavior-graph.md#v44-display-clock) on exact AE 1.6.1170 inject: after the existing guards pass, the first eligible call establishes an origin; later calls accumulate fixed 1/60-second native steps. Each observed delta is capped at 100 ms and six due steps, dropping excess stall time while preserving fractional steps. Zero-due calls return the current candidate pose as stepped mode without advancing native/motion counters. Request, retarget, rejection, native ineligibility and frozen fallback reset pacing. An F8-only drawing gap without request/retarget/reset does not fully pause the clock; recovery uses the existing 100 ms/six-step stall cap. The shared batch recipe is unchanged. A failed step may use the same valid source's frozen pose; stale actor/graph/generation/controller-owner identity rejects both candidate paths and continues through OAR. Counters/checkpoints 1/60/120 count actual native steps, including multiple due steps in one draw. The two 30/120 FPS elapsed-clock regressions and Release build pass locally ([commands/results](build.md#v44-display-clock-2026-10-29)); game speed and visuals remain unverified, and installation stays v41.

The controller owns request/view measurement windows, not the retained candidate allocation. Closing/resetting or a new request discards partial comparisons; F7 retarget refreshes the copied-node map, view baseline and coverage without rewinding compatible normal playback clocks. A destroyed owner invalidates its candidate view; source changes do not create another candidate in the same process. See the [canonical v32 interfaces and manual checklist](independent-behavior-graph.md#g2-v32-display-session) for semantic motion, actual assignment counts and expected fallback. Native audit/history details remain there rather than serving as current operating instructions.

## Equipment context and torch composition (2026-10-23)

Every selected Unequip uses the previous normalized hand IDs through `transition_hands`; Equip and the next idle use current hands. A proposal retains its typed transition action and preview state across effect-revision refresh. Compatible request refresh also retains an unfinished transition instead of replacing it with idle, and does not prepare a new idle goal over that transition. Finished transitions hand over normally. No source equipment is written. Historical instance-state reads remain unsupported; the [bounded IED rules](oar-hooks.md#historical-hand-and-direction-boundary-2026-10-23) allow only proven dependencies.

The observed right one-handed weapon plus left torch now uses `1hm_idle` for the body and independently queries/loads `Animations\Tor_1HMPose.hkx` on channel 3. `SourceObservation::left_torch` is independent of body family. Torch-only retains `TOR_Idle`; other weapon families retain existing routing. A newly equipped weapon supplies the transition when a left torch returns in the same update. A change to the left torch alone keeps any pending or playing weapon transition and its DAC ticket. Removing the right weapon while retaining the torch keeps the arm layer during the one-handed Unequip, then uses the torch-only idle.

Each private `Proposal` owns its arm selection, load lease and sampling clock alongside body/standing/calibration. Pending-load reuse, effect refresh, accepted-same-asset decisions and graph retarget include that arm. Readiness waits for the requested arm; a missing base or exhausted replacement/base load logs explicit `ANIMATION TORCH ... outcome=body-only`. This fallback is not equivalent torch posture. Cancellation destroys the same owned leases. Acceptance commits arm variant history only when visible playback accepts it. The arm samples on an independent retained clock, synchronized with an identical outgoing arm at acceptance after load. A layer-only handover keeps the clock and annotation cursor of the same body clip, including a playing Equip or Unequip. The main action clock dispatches annotations. `compose_contributor(..., BoneMask::e_left_arm)` covers left clavicle, upper arm, forearm, hand/fingers, excluding root/torso/legs/right arm. Outgoing and incoming contributions each compose standing+body+arm before blending; removal blends back to the current body. The independent zero-time MT_Idle calibration is unchanged.

`panel_torch_arm_bases` enters the existing finite base cache. No new dependency, target, native hook or configuration is added. Modify [controller.cpp](../src/animation/controller.cpp) (`request`, `State::queue`, `poll`, `retarget`, `advance`), [action.h](../src/animation/action.h), [sequence.h](../src/animation/sequence.h), this document, [OAR](oar-hooks.md), [README](../README.md), [Changelog](../CHANGELOG.md) and the [handover](diagnostics/display-issues-2026-10-22.md#10-2026-10-23-装备上下文与火把分层修复待上机) together.

The earlier 2026-10-23 repair verification used two engine-free scenarios in [animation_repair_test.cpp](../tests/animation_repair_test.cpp): `equipment_removal_preserves_query_hands_and_native_ied_boundary` and `one_handed_torch_composes_complete_left_arm_and_removes_cleanly`. They cover shared production policies/composition, not the native loader/controller lifecycle. Transition refresh, layer ownership/retarget/outgoing paths and bounded diagnostics are source-reviewed; runtime readiness, masks against installed assets, phase timing, native IED and visual equality remain UNVERIFIED. See [current build evidence](build.md#animation-repair-checks-2026-10-23).

Single-handed axes in current or removed hands arm the same request-wide probe as two-handed axes. `POSE SCALE` records raw Havok x/y/z; [read_pose and ni_local_from_pose](../src/animation/clip.h) produced a uniform scale in the v130 playback path. Current animation writes apply rotation and translation while retaining the copied node's scale. This covers the standing calibration, composed OAR playback and optional candidate display. The raw scale trace remains available. [r4](diagnostics/display-issues-2026-10-22.md#r4-session) captured nonuniform Root scale `(1.213338,1,1)`. [Audit23](behavior-graph-construction-audit.md#audit23) now proves a narrower native-path fact: the complete 63856 writer normalizes quaternion and writes rotation/translation, without reading the input scale vector or writing the destination scale field. Existing destination scale survives this writer. This is a scoped difference in write coverage, not proof that every native conversion ignores scale, preserves all axes or that scale-x is the sole flash cause. Other writers, phase-aligned world/panel transforms, mapping, placement and composition remain relevant. Four windows/twenty-four snapshots and the fifteen-bone source snapshot remain unchanged; this audit changes no conversion, clamp, asset or code.

## Purpose and scope

Show an independent equipment-driven non-combat idle in InventoryMenu. Equipped weapons are held in the copied graph even when the source Actor is sheathed, attacking, moving, sneaking or in combat. Opening never resumes a world phase or plays Equip/Unequip/entry Start. Inventory-only weapons do not count. Spells, staves, shields and torches retain their corresponding families. No Actor is created or controlled; the bounded source marker transaction belongs to [DAC](dac.md).

## Configuration

Copy [config/CharacterPanel.ini](../config/CharacterPanel.ini) to `Data/SKSE/Plugins/CharacterPanel.ini`:

```ini
[Animation]
UnarmedIdle=non_combat_loop

[Diagnostics]
DetailedAnimationTrace=false
BehaviorGraphProbe=false

[Experiment]
IndependentGraph=off
DisplayPose=false
DisplayStep=false
SourceTimeline=false
```

The accepted values are lowercase and affect empty hands only:

| `UnarmedIdle` | Empty-hand presentation | Native preparation / fallback |
| --- | --- | --- |
| `non_combat_loop` (default) | Query the OAR `h2h_idle.hkx` family with panel-drawn=true. | Observe the known Smooth goal first; use direct MT_Idle if preparation or a usable replacement fails. |
| `mt_idle` | Load vanilla `MT_Idle.hkx` directly with panel-drawn=false. | Bypass visible OAR replacements, preparation and annotation dispatch. |

A missing file uses the default; an invalid value logs one warning and uses the default. Settings load once at DataLoaded, so restart the game after edits. There is no new UI or live reload. Equipped spells, staves, shields and torches keep their corresponding families in either mode; carried inventory items do not count as equipped. Both modes retain the separate zero-time vanilla MT_Idle calibration.

`DetailedAnimationTrace=false` is the normal diagnostic setting. Accepted panel mappings and event-driven WORLD snapshots remain enabled; set it to `true` and restart only when collecting bounded axe probes, annotation information, Smooth marker reports, candidate rejection detail or capture geometry listings. Dispatch, conditions, source/isolation audits and warnings/errors run in either mode. Ordinary information is buffered with periodic flushing about once per second; warnings/errors flush immediately. The flush timer only flushes sinks and performs no engine work.

The current [v46 first-game-pass procedure](scene-graph.md#v46-manual-regression) temporarily enables DetailedAnimationTrace and SourceTimeline while keeping IndependentGraph=off and both candidate display flags false. Its two source-window/normal-OAR workflows remain **PENDING**; they do not validate native display pacing or replay. Preserve other INI keys and restore the pre-test settings/restart afterward.

The four experiment keys above are independent of `DetailedAnimationTrace`. Preserve other INI settings when enabling an experiment and restart. `DisplayPose=false` selects normal OAR playback; `DisplayPose=true/DisplayStep=false` permits a valid frozen candidate; both display flags true request eligible stepping. A stale view always returns to OAR. `SourceTimeline=true` enables the read-only F7 window outside the panel; F7 recaptures inside, while F9 stays game quickload. `IndependentGraph=off` is the safe default; use `inject` only under the [v32 checklist](independent-behavior-graph.md#v32-manual-checklist).

## Architecture

`CharacterManager` distributes equipment revisions and menu/lifetime inputs. Each `SceneGraph` owns an `AnimationController` and `dac::Session`. `SourceObservation` carries equipped family/primary side, independent left-torch presence, hand IDs, request/source-graph generations and reason. It contains no world clip, binding index/time or draw state. The source graph still supplies skeleton/bindings.

The animation module contains both integrations: [src/animation/oar](../src/animation/oar/hooks.h) owns native OAR queries and [src/animation/dac](../src/animation/dac/session.h) owns the marker transaction. This directory ownership retains the existing `oar` and `dac` namespaces and runtime boundaries. Production callers and offline tests include these headers from the configured `src` root, using `animation/oar/...` and `animation/dac/...`; see the [build/include convention](build.md#architecture-and-generated-outputs).

The production `IdleTarget` selects family and direct/query mode. When the preview enters a queried idle directly on panel open or after an equipment change, the controller requests the Smooth `Smooth_loop` goal and waits for the observed effects before querying OAR. An Unequip handoff requests the same goal after its final annotations have been queued, since `weaponSheathe` can clear the loop marker. Equip handoffs follow the Start → Loop phase driven by the clip's annotations. Native DAC owns matching, filters and keyword dispel. [v130 Unequip trace](diagnostics/CharacterPanel-v130-trackdiag-20261127.log#L33571)

Refresh behavior depends on the current goal state.

- An unchanged compatible target whose goal is already ready queues an observation-only check, preserving matching accepted clock/variant without recasting.
- If preparation is still pending, a newer refresh request starts one preparation attempt for its own generation; obsolete queued work cannot cast. Compatible in-flight clip leases remain retained behind the readiness gate, then undergo the current query and generation/effect-revision checks before acceptance.
- Once the controller retains a direct fallback, compatible appearance/F7 refresh keeps that fallback and skips preparation. Equipment changes clear it; a replaced source graph/controller lifetime also starts fresh.

None of these paths reseeds the original inventory entry snapshot.

Missing DAC/Smooth, unsafe tags or a two-second observation timeout do not force the vanilla base: OAR is still queried against the source's actual effects, and a base selection is the last resort. Armed requests use their equipped family's base, never MT_Idle merely because the source was sheathed. Empty-hand queries without a usable replacement, including condition rejection or failed replacement loading, use direct MT_Idle instead of vanilla combat-looking fists. Once retained, the direct fallback is stable across compatible refreshes until equipment changes or the graph/controller lifetime resets. No generic protocol is inferred for other movesets.

## Code map

| Path | Stable entry point |
| --- | --- |
| [action.h](../src/animation/action.h), [idle_policy.h](../src/animation/idle_policy.h) | `weapon_form_from_hands`, `panel_idle_bases`, `panel_transition_bases`, `panel_base_animation_names`, `IdleTarget`, `idle_target`, `parse_unarmed_idle` |
| [base_animation_cache.h](../src/animation/oar/base_animation_cache.h), [query_policy.h](../src/animation/oar/query_policy.h) | `BaseAnimationCache`, `BaseAnimationProject`, project-only sparse metadata and session-only conditions |
| [settings.h](../src/animation/settings.h), [settings.cpp](../src/animation/settings.cpp) | `load_settings`, `unarmed_idle_setting`, `independent_graph_setting`, `display_pose`, `display_step` |
| [controller.h](../src/animation/controller.h), [controller.cpp](../src/animation/controller.cpp) | `SourceObservation`, `AnimationController::request`, `poll`, `advance`, `retarget`, `apply_standing_pose` |
| [independent_graph.h](../src/animation/behavior_graph/independent_graph.h), [independent_graph.cpp](../src/animation/behavior_graph/independent_graph.cpp) | controller-owned `DisplaySession`, `request`, `retarget`, `held_display_pose`, `step_display_pose`, `record_application`, `invalidate_display_owner` |
| [display_state_policy.h](../src/animation/behavior_graph/display_state_policy.h), [pose_motion_policy.h](../src/animation/behavior_graph/pose_motion_policy.h) | `DisplaySourceIdentity`, `DisplayWindow`, `apply_named_display_pose`, `compare_display_motion` |
| [drive_step_policy.h](../src/animation/behavior_graph/drive_step_policy.h), [graph_observation_policy.h](../src/animation/behavior_graph/graph_observation_policy.h) | `drive_graph_step`, `ScopedGraphWorld`, `observe_graph_activity`, `observe_output_slot` |
| [behavior_graph_display_step_test.cpp](../tests/behavior_graph_display_step_test.cpp), [behavior_graph_display_session_test.cpp](../tests/behavior_graph_display_session_test.cpp) | prior v31 shared-step/read regressions and v32 source/window/motion/coverage regressions; [commands and evidence](build.md#v32-display-session-2026-10-28) |
| [event_clock.h](../src/animation/event_clock.h), [event_clock.cpp](../src/animation/event_clock.cpp) | `EventClock`, `Annotation`, `clip_annotations` |
| [sequence.h](../src/animation/sequence.h), [variant_state.h](../src/animation/variant_state.h) | `Stage`, pose composition and accepted `SelectionState` |
| [clip.h](../src/animation/clip.h), [clip.cpp](../src/animation/clip.cpp) | `ClipPlayback`, retained binding ownership, track mapping and detached samples |
| [session.h](../src/animation/dac/session.h), [idle_goal.h](../src/animation/dac/idle_goal.h) | `Session::prepare_idle`, `idle_readiness`, `IdlePreparation` |
| [held_equipment.cpp](../src/character/held_equipment.cpp) | `hold_copy_equipment`, owned-copy hand mounting |
| [hooks.cpp](../src/animation/oar/hooks.cpp) | `prepare_base_animations`, `base_animation`, `animation_binding_name`, native OAR metadata and actual source-effect conditions |
| [base_animation_cache_test.cpp](../tests/base_animation_cache_test.cpp), [base_animation_cache_cases.h](../tests/base_animation_cache_cases.h) | two focused sparse-cache behavior cases shared with the retained driver |
| [animation_controller_test.cpp](../tests/animation_controller_test.cpp) | two policy/preparation/transaction cases |
| [dac_event_match_test.cpp](../tests/dac_event_match_test.cpp) | two native event matching boundary cases; no engine dispatch |
| [torch_shield_policy_test.cpp](../tests/torch_shield_policy_test.cpp) | `weather_flag_keeps_native_source_answer`, `left_shield_removal_keeps_weapon_and_previous_query_hands` |
| [mapping_report.h](../src/animation/mapping_report.h), [animation_mapping_test.cpp](../tests/animation_mapping_test.cpp) | `AcceptedMapping`, `MappingPublication::accept`, accepted alias coalescing and one selected indexed read |
| [diagnostic_trace.h](../src/animation/diagnostic_trace.h), [diagnostic_trace_test.cpp](../tests/diagnostic_trace_test.cpp) | `PoseTraceBudget`, `Trace_Body_Nodes`, one coherent request-budget case |

## Interfaces

`ActionKind::e_standing_idle` (0) selects plain standing; `e_weapon_idle` (1) selects an equipped/unarmed family idle. Idle targets loop; equipment transitions play once. Movement direction 0=None is true while standing, other directions are false, and combat state remains zero; panel-drawn comes from the target. Source equipment and identity remain actual reads.

Logical selection paths remain separate from loading index/binding name. Native interruptibility, replace-on-loop, blend durations and variant/PlayOnce/Random metadata remain copied values. Request generation immediately rejects obsolete queued mutations. Compatible refresh adopts the current generation only for future accepted events; changed equipment does not authorize the old clip's events.

### Torch weather and left-shield transitions (2026-10-22)

`CurrentWeatherHasFlag` is admitted by [forwards_source](../src/animation/oar/policy.h). OAR reads the actual current weather and retains its own false/negation behavior; the panel supplies no constant weather answer. This fixes the confirmed unsupported-condition rejection of TOR_Idle candidates. A candidate can still fail its native conditions; torch/world visual equality is not established by admission alone.

[left_shield_transition](../src/animation/idle_policy.h) handles the narrow unchanged-right-weapon case used by `AnimationController::request`: adding a left shield selects Equip with current equipment context, while removing that shield selects `1HM_Unequip` even though the right weapon remains equipped. The controller limits this policy to the one-handed family and identifies a real `TESObjectARMO::IsShield`. Existing torch-hand routing and other families retain their current transition policy.

All selected equipment Unequip transitions pass former hand IDs through `PreviewState::hand_override` for supported core equipment reads. Pending effect refresh retains every transition action/context; the following idle uses current equipment. SDS settings, explicit gear-node hints and unchanged-hand equipped hints use native provider reads. For a changed weapon hand, Unequip can carry the placement observed while that weapon was equipped. The history expires when Unequip finishes or equipment changes. Historical instance-state predicates remain unsupported. See the current section above for lifetime and torch behavior.

`ANIMATION WORLD` records a **paused source snapshot** at panel-open and equipment requests, including reason, current hand IDs and actual source drawn state. It is not a settled post-change world transition or the panel's projected drawn state. The [current low-overhead mapping format](#accepted-mappings-and-world-snapshots) uses raw authored `original`, direct actual `index/bound`, and the official API's separate replacement fields. It replaces the earlier reverse lookup and `lookup/replaced/original-index` fields. An empty API response remains unknown, not proof of vanilla playback.

`IsMovementDirection` reads its requested direction through the verified numeric component: 0=None answers true while standing, other directions answer false. The canonical disabled/negation path runs once. OAR 3.1.6 binary evidence and the authoritative upstream enum agree that 1=Forward. The previous all-false policy was incorrect and did not change r2 selection. `IsCombatState` retains requested-state evaluation against standing state 0; authored Smooth gates remain enforced.

With detailed tracing enabled, per-query `OAR QUERY` diagnostics publish **one full candidate path per distinct project folder and rejection cause** (first signature, at most forty-eight) followed by one suppressed-count line; the earlier eight-unsupported/two-native-false split is retired, because a single pack's repeated condition could consume the whole budget before a later folder's cause was reached. Unsupported conditions identify provider and condition name; an otherwise ordinary native rejection is labelled as native conditions/eligibility returning false without guessing which condition failed. `first-query-rejection` on a traced proposal belongs to an earlier rejected candidate, not necessarily the selected candidate. Read [the OAR diagnostic boundary](oar-hooks.md#correlated-rejection-evidence-2026-10-22) and the [dated test procedure](diagnostics/display-issues-2026-10-22.md#10-2026-10-23-装备上下文与火把分层修复待上机).

### Accepted mappings and WORLD snapshots

Default `ANIMATION PANEL ... accepted` records describe only ready, accepted playback: final loader fallback at handover, or accepted same-asset reuse after the current query/history checks. A queued proposal is never a playing record. `original/base-index` identify the accepted base, `logical` is the selected logical replacement asset path (empty for base playback), `variant` gives the accepted member index/position or `none`, and `index/bound` name the actual engine binding. Hash-deduplicated logical paths may share that binding. The final bound name is read by index at publication, so loader fallback cannot publish an earlier rejected replacement's stale name.

`MappingPublication` retains just one accepted value per controller. Identical mapping acceptance on repeated loops within a request produces no new mapping line; a new request or a changed accepted action/logical path/variant/binding/status publishes again. Pending/rejected proposals do not update it. No accepted-winner cache is used for selection, and no animation/history policy changes. Proposal path detail is opt-in and explicitly labelled `proposal`; the all-candidate `oar walk` is removed entirely.

WORLD calls the optional official OAR Animations V1 API on each actual active source clip. `original` stays the raw authored clip name, while `index/bound` is a guarded direct selected-entry read. `oar-path`, `oar-project`, `oar-variant`, `oar-submod` and `oar-mod` remain separate OAR-reported fields. `oar-api=available oar-result=replacement` is positive current-replacement information; `empty-unknown` or `unavailable-unknown` preserves the actual bound asset without guessing vanilla or a unique logical alias. Synthetic panel clips are never registered with or queried through this current-world API.

There is no logging-only full-table reverse scan, replacement/variant catalogue copy, deferred reverse scan or full-name cache. The finite original-base prewarm and functional selection metadata remain unchanged. The API may scan members of its current replacement and copy several strings; it is not globally free or a constant-time performance guarantee. `build-ms` measures synchronous snapshot/mapping construction before the log call, excluding sink writes/flushes and later frame rendering. It is not disk time, end-to-end inventory latency or FPS. See [API lifecycle, examples and verification](oar-hooks.md#low-overhead-mapping-and-opt-in-traces-2026-10-22).

## Fixed base indices and project readiness

`panel_base_animation_names` derives the finite registry from the existing `panel_idle_bases` and `panel_transition_bases` aliases for all eleven weapon families. It also includes Tor_1HMPose and includes H2H/MT_Idle, one/two-handed, axe, bow/crossbow, dual wield, magic, staff, torch and shield, with equip/unequip aliases and vanilla MT_Idle calibration/fallback. Numeric binding indices are discovered from each project's actual table. Canonical slash/case matching, exact-before-gender/suffix matching within a candidate, ordered alias priority and left-primary idle order remain intact. OAR/DAR replacement folders cannot supply original bases.

Names identify the original action during initialization; they do not replace the native OAR lookup key at playback. The stages use different inputs:

| Stage | Inputs and result | Implementation |
| --- | --- | --- |
| Initialize project metadata | Fixed original names → that actual project's original binding indices and names. Indices are neither hardcoded numbers nor global IDs; table replacement/world release invalidates them. | [BaseAnimationProject::resolve](../src/animation/oar/base_animation_cache.h) |
| Locate native candidates | Matching string-data project + original binding index → that original action's OAR replacement candidates. | [query_animation](../src/animation/oar/hooks.cpp), `s_get_replacements(project, base->index)` |
| Evaluate and choose | Source Actor, preview clip and scoped preview/condition context → eligible native replacement; accepted local variant/history policy then chooses its member. Project/index alone is insufficient for full evaluation. | `s_evaluate_replacements`, `ScopedEvaluation` in [hooks.cpp](../src/animation/oar/hooks.cpp); `AnimationController::State::select` in [controller.cpp](../src/animation/controller.cpp) |
| Load and report | Selected binding index addresses the engine binding; fetch its actual indexed name separately from the logical replacement path. Names remain in carrier/loading metadata and diagnostics. | `animation_binding_name`; [PreviewClip::initialize_native](../src/animation/oar/preview_clip.h); `LoadedAsset::queue` |

`PreviewClip` retains the name in native carrier fields alongside the binding index; removing the expanded catalogue does not erase all names. Hash-deduplicated replacements can have different logical paths but share one actual binding index/name. The existing [two cache cases](../tests/base_animation_cache_cases.h) verify the sparse mapping, reuse/invalidation and actual selected-index names; [their build entry](build.md#fixed-base-index-verification) remains unchanged.

`AnimationController::prepare_base_indices` reuses the retained third-person `source_graph` resolver during `SceneGraph::update_monitor` outside paused inventory. OAR DLL loading alone is too early: preparation waits for a published source graph with behavior/skeleton and a binding set covering the animation-name table. The [OAR loading boundary](oar-hooks.md#fixed-base-metadata-2026-10-21) explains this check. Unavailable metadata retries on later observations without recording missing originals, advancing a graph, evaluating conditions, changing Random/history or calling DAC/IED. LoadingMenu observations are skipped after lifetime clearing so an old graph cannot refill the cache during a load.

On a cache miss, `BaseAnimationProject::resolve` scans the engine table **once**, borrowing each name for that synchronous pass and using bounded scratch normalization. It copies only finite matched base names and retains missing-key status. No expanded owned catalogue or session copy survives. Up to four projects share sparse metadata keyed by string-data owner, array address and count; a changed owner/table/count rebuilds, and existing world-release invalidation clears all projects even if addresses are reused. Borrowed strings never escape the pass. Warm base/missing lookups do not reread the table.

If a project becomes ready only after opening inventory, the same finite resolver initializes on demand, logging `stage=menu-fallback`; there is no wait loop. Normal preparation logs `OAR bases stage=world-prewarm` once per cached project/table, with table/requested/resolved/missing counts and elapsed milliseconds. Missing family bases retain the existing next-alias/standing-base policy; absent MT_Idle retains the existing unavailable/accepted-playback behavior. OAR absence or an unmatched project still permits the direct base policy.

Native replacement selection remains per query. Descriptors, accepted variants and deduplicated aliases retain the actual binding name read at the requested index through `animation_binding_name`, independently of their logical path; the loader addresses the selected binding by index. This change removes full-table ownership, per-name guarded reads and repeated scans/session copies; it still performs one initialization scan. DAC file preflight, scene-graph Clone/audits and first asset/GPU use remain separate, unchanged costs. No game-time stall reduction has been measured here.

### Custom weapons and new behavior entries

A custom weapon that uses an existing engine weapon family and replaces that family's existing idle/equip entries through OAR uses the same cached originals. [equipment_weapon_form](../src/character/equipment_state.cpp) reads the equipped engine type; [panel_idle_bases and panel_transition_bases](../src/animation/action.h) choose the family aliases. For example, a custom one-handed sword's OAR idle replacements still sit behind the existing one-handed original entry. Native eligibility must still use conditions supported by the preview adapter; an unrelated third-party condition can reject a candidate. This is a compatibility boundary, not a guarantee for every custom moveset or skeleton.

The current **finite canonical original-name keys include aliases and Tor_1HMPose**. They do not limit the number of OAR replacements, variants or custom weapon records behind those entries. A mod introducing an independent behavior/clip entry outside the mapped families requires an explicit family/base mapping before the panel can request it. The old full-table catalogue also used the same action-family policy; storing every name did not discover or schedule new families automatically.

## Event timing and native state

`clip_annotations` reads retained HKX annotations, validates times and stable-sorts them. `EventClock` dispatches crossed occurrences once, including zero/end/loop boundaries. Only accepted queried playback emits native events. Loading, direct base modes/fallbacks, calibration, standing underlay and outgoing fades do not dispatch. Compatible refresh and retarget preserve clock/cursor; frame delta remains clamped to 0-0.1 seconds.

SKSE UI observations publish actual native effect revisions; function return is not readiness. OAR reads real source effects. Pending choices recheck revisions and retain compatible leases. Interruptible idles may requery on changes. Loop boundaries retain native replacement/sequence/PlayOnce/Random scope semantics; same-replacement appearance/interrupt checks preserve accepted variant/history. Equip/start/loop ordering is produced by the clips' own annotations through DAC and OAR's phase conditions: the loop boundary re-queries after the Start clip's `Smooth_loop` annotation has been cast, so the next selection is the Loop replacement. There is no folder-name scheduling or playlist.

## Pose mixing and placement

Outgoing and incoming poses both advance during fades using copied native durations and between-variant policy. This detached composition does not call OAR's ActiveClip blender or implement its lookahead/end-relative seek. Incoming clips start at zero. Calibration independently samples vanilla MT_Idle at zero through `apply_standing_pose`, before the current pose, regardless of the setting.

Generic Weapon, Shield and Quiver tracks map independently to WEAPON, SHIELD and QUIVER, never sheath WeaponSword/MOV nodes. [Capture](scene-graph.md#held-equipment-invariant) remounts only mapped copy equipment and preserves model-local transforms.

### Bounded axe pose and projection evidence (2026-10-22)

The reported axe symptom is a single apparent size change immediately after equipping. Its visual cause remains **UNVERIFIED**. The selected Smooth axe archive has identity scale masks on all 99 tracks and identity first four tracks; track 4 has substantial authored translation. Empty track tables still use the existing `identity-unverified` mapping. These facts do not establish the source bone ordering or the displayed effect. Exact asset hashes, translation bounds and pending game criteria are in the [dated handover](diagnostics/display-issues-2026-10-22.md#5-2026-10-22-实现跟进与待上机标准).

With `DetailedAnimationTrace=true`, `AnimationController::State::begin_pose_probe` and `trace_pose` reuse one read-only probe for axe, torch/shield hands and generic bow/crossbow evidence. One-handed shield transitions and a weapon beside a torch are covered, including the torch request's removal-to-idle context. Resource-specific bow/crossbow analysis and repairs remain deferred. The former `AXE PROBE/MAP/POSE/CAMERA/NODE` prefixes migrate to `POSE PROBE/MAP/SAMPLE/CAMERA/NODE`, with a profile field on each window/sample header.

[PoseTraceBudget](../src/animation/diagnostic_trace.h) limits the entire requested generation to **four windows and twenty-four snapshots**, across accepted handovers and graph retargets. Each window retains twelve nominal wall-second offsets `0, .016, .033, .066, .100, .200, .500, 1, 1.5, 2, 3, 4`, expires after 4.1 seconds and skips missed slots without catch-up output. Only a new request resets the totals. An unchanged accepted loop does not rearm; a denied arm or departure from a supported profile stops the old window so it cannot describe a new graph/clip under stale timing. At most one `POSE BUDGET ... exhausted` notice appears per request. No standing/calibration/outgoing asset starts a separate probe. Flag/profile/time/budget gates precede expensive diagnostic work.

- `POSE PROBE` identifies actor, latest requested `request`, `accepted-request`, probe epoch/profile, copied graph pointer, handover/retarget, clip path/time and immediate root state. An older accepted clip can remain visible while a new request loads; these IDs deliberately differ. Join graph pointers with `SCOPY GRAPH/CAPTURE` for the capture ID; `FRAME` is capture/calibration evidence, not a body time series.
- `POSE MAP` records actual selected track indices, target skeleton indices/raw names and resolved copy names/pointers for tracks 0–4, track 43 and the fixed body/arm targets. Target names come from the asset's retained graph/skeleton, not a newer pending source graph; unavailable/out-of-range names stay explicit. `original-skeleton` is the binding's string, not proof of source ordering. `identity-unverified` remains explicit; track 4/43 labels do not invent source bone semantics.
- `POSE SAMPLE` identifies clip time, wraps since window start, outgoing path/time, blend elapsed/duration and incoming weight. Its `POSE NODE` lines separate incoming sampled local from final composed local/world. Fifteen requested body points cover root/COM/pelvis/spine/head/feet plus both clavicles, upper arms, forearms and hands. The hands use the archived actual `[LHnd]/[RHnd]` names. Copy-root and track 4/43 targets are additional identified observations, not extra source-bone requests. Missing nodes and unmapped incoming samples remain explicit.
- Transform translations are game coordinate units, scales are ratios, and rotations are quaternions ordered x/y/z/w. The pose snapshots are after composition and world recomputation. The immediate begin row can precede placement/calibration after a retarget.
- `POSE CAMERA` reads the same UI3D menu camera used for figure submission: pointer, world transform, raw left/right/top/bottom/near/far frustum fields, orthographic flag and port width/height. `screen-port-xyz` is the engine's `NiCamera::WorldPtToScreenPt3` output for a node's world position; x/y use the camera port coordinates, z is raw projection output, **not linear distance**. Invalid/nonfinite projection is unavailable. The fields are not final composite pixels; compare head/foot projected separation at the same camera/epoch to assess projected body size.

The probes do not change animation sampling, scale, mapping, root/pelvis placement, blending, camera policy or assets. A fresh game log must capture the first equip frames and any recapture, distinguish root scale from body translation/blending and camera projection, and confirm the actual Loop selection after mixed-case Smooth annotations. A repeated Start wrap is a separate phase defect; it is not established as the cause of the one-time immediate flash. Bow and crossbow repairs are deferred.

### Correlating reported issues

Normal accepted PANEL/WORLD mappings remain authoritative and on. Detailed records describe observations and pending decisions, not proof that a proposal is playing. `ANIMATION QUERY` records requested bases/action/reason/target, actual source hands/drawn state, projected drawn/historical hands, existing IsEquipping/IsUnequipping values, effect revision and current accepted clip. Diagnostic request IDs are carried into OAR without changing its channel/clip key/history. [Root false evidence](oar-hooks.md#correlated-rejection-evidence-2026-10-22) observes the original single evaluation result; it never reruns a condition.

`ANIMATION DECISION` records actual pending choices, accepted reuse, refresh/cancel, waiting-goal entry/exit, calibration drop, handover and boundaries. Waiting polls do not repeatedly print waiting records. `asset-request` identifies a loader's immutable creation request; current `request` and `accepted-request` identify later reuse/display separately. `ANIMATION LOAD/REBIND` identify target versus standing/calibration resources, actual fallback/rebind result and existing queue-to-ready duration. `ANIMATION EFFECT-REV/PENDING-REV` show already-read observed/current/proposal revision values. No queue, refresh or historical-equipment policy changes.

`ANIMATION HANDOFF` reports `handoff-seconds` for the action's `weaponDraw` or `weaponSheathe` annotation. Clips without the matching annotation use the midpoint for Equip and the end for Unequip. `ANIMATION ANNOTATION` includes the accepted action, index, path, annotation time, clip time, loop flag and dispatch decision. Follow the DAC dispatch through the observed effect revision and next accepted clip. [DAC event diagnostics](dac.md#opt-in-annotation-and-marker-evidence)

For the immediate axe flash, align capture reason/old-new graph, raw placement, existing calibration measurements, actual mount switches and the first POSE slots. [Scene evidence](scene-graph.md#opt-in-capture-placement-and-mount-evidence) adds one fixed fifteen-bone paused-source snapshot per request during the already-required transform-copy pass, not another traversal. Compare source and panel only after matching IDs, phase and coordinate context. Missing/exhausted data is not evidence that a cause is absent. The [current dated procedure](diagnostics/display-issues-2026-10-22.md#10-2026-10-23-装备上下文与火把分层修复待上机) covers torch/shield, Smooth phases, axe and generic arms/gear separately, with the [section-8 budget](diagnostics/display-issues-2026-10-22.md#8-2026-10-22-已报告问题的关联诊断) defining the pose windows.

## Invariants, failures and compatibility

Only current generations and observed effect revisions accept resources. Accepted playback remains visible during loading. Source graph replacement resets graph-specific resources while preserving the DAC transaction. Snapshots, clocks and history remain per Actor record. Source graph/equipment/draw flags stay read-only; only bounded DAC marker casts/restoration may mutate source state. Rendering, framing and parked graph retirement remain unchanged.

The [Smooth boundary](dac.md#smooth-moveset-compatibility-boundary) documents prerequisites and known protocol. The [three explicitly checked IED/SDS schemas](oar-hooks.md#native-ied-placement-conditions-2026-10-21) use current source equipment/configuration and retain the provider's left/right, comparison, shield setting and negation, including native false. The matched provider must have embedded version 1.0.2.0; the three named schemas also require condition version 1.0.0.0 and their exact components. Other names from that same provider keep native current-source answers without those individual schema/version checks. Historical-hand queries, unknown/missing providers and mismatched checked schemas reject. The shield branch needs its provider-side SDS registration/prerequisites; it is not a universal core non-combat dependency. Provider registration, paused IED hint refresh and game appearance remain unverified. Combat projection stays zero and the panel deliberately clears Smooth combat markers. No complete pack/custom-skeleton support is claimed.

Logs identify setting, actor/request/goal/family, preparation requested/ready/failure, copied-held counts/missing mounts, choices, loads, handovers and restoration. Missing project/base or failed base loading retains accepted playback or loading with diagnostics; no guessed binding/source mutation is a fallback. Unsupported third-party conditions remain rejected, not bypassed.

<a id="idle-start-replay"></a>
## Open issue: idle start segment replay (2026-10-30)

**症状**：面板里的待机会把片段的起始段每圈重放一次，看起来像"起步 → 起身 → 再起步"循环。2026-10-30 的 v46 首轮上机由操作者确认，装备/卸下/F8/重开均无其他异常。

**机制**（代码路径，非推测）：待机以 `loop=true` 入队（[controller.cpp](../src/animation/controller.cpp) 的 `queue_idle` → `queue(..., {kind, form, true}, ...)`）。[event_clock.h](../src/animation/event_clock.h) 在 `loop=true` 时每到一个 duration 边界先调 `boundary()`、再把时间归零；`controller.cpp` 的边界回调置 `idle_due = true; reason = e_boundary`，随后 `queue_idle(source, e_boundary)` 生成**新的 proposal**，时钟从 0 起播。于是只要选中的片段带起手段，起手就每圈重放一次。

**日志旁证**：待机重入队的时间间隔里反复出现 4.37 s / 4.38 s 的成对值（片段时长约 4.4 s），其余 9.79 / 13.90 / 21.08 s 的间隔与捕获时刻重叠，说明边界重入队与捕获重入队叠加，不是等周期。

**边界**：与 v46 无关（v46 只改退休副本台账）；属产品播放路径的既有问题。未修，也未立契约。三个方向取舍不同——边界处不重入队、保留时钟连续、选段时跳过起手段——需要单独定契约后再动。

> **2026-11-23 更正与契约**：已立 [C-IDLE v1 预注册契约](diagnostics/idle-loop-contract-20261123/README.md)，**实现前落盘**。取证经过三轮修正，当前结论：
>
> 1. **`SelectionState::dwell()`/`seek_dwell()` 不是修复**（曾如此判断，已撤回）。引擎无关实验证明 `dwell()` 清 PlayOnce 标记而非钉住位置，会让待机**永久卡在入口段**，比原缺陷更糟。**不得接线**。
> 2. **`_bPlayOnce` 也不是本缺陷的根因**（曾如此判断，同日撤回）。那只描述"若走 OAR 变体序列，PlayOnce 决定是否驻留"，而 **Smooth Moveset 系列的 Start/Loop 由 DAC 裁定**（用户声明）——DAC 放持久魔法效果标记，OAR 条件经 `HasMagicEffect` 读取（[effects.cpp:274-275](../src/animation/dac/effects.cpp) 原文）。面板侧通路正确：`HasMagicEffect` 在 `forwards_source` 白名单（[policy.h:48](../src/animation/oar/policy.h)），[hooks.cpp:566](../src/animation/oar/hooks.cpp) 对源 Actor 求值。
> 3. **已确证的事实边界**：`Smooth_loop` 施法完成（revision 4→5）后，紧接着的边界重查仍返回 `accepted-same-asset`（[r2 日志](diagnostics/CharacterPanel-issue-trace-r2-20261022-2314.log) 23:05:30.667 → 23:05:31.479）。即 **DAC 施法完成 ≠ 标记对 OAR 可见 ≠ OAR 选中 Loop**。这与 [dac.md](dac.md) 和 [display-issues-2026-10-22.md](diagnostics/display-issues-2026-10-22.md#r2-上机步骤detailedanimationtracetrue已完成) 早已登记的开放项一致。
>
> 三个候选断点（标记未激活 / 条件未命中 / 被同资源快路径吃掉）**均未验证**，契约 §7.3–7.4 列出区分它们所需的取证。上文"机制"段的表述保留为当时的记录，不改写。回归护栏：`tests/idle_dwell_policy_test.cpp`（7 例）。
>
> **2026-11-24 上机取证（结论更新）**：[v127 记录](diagnostics/idle-loop-v127-session-20261124/README.md)。两个结论：
>
> 1. **`DisplayPose` 决定面板动画来源**：`true` 走候选图 `poseLocal`——候选图由引擎 `63585`/`63587` 驱动，**不携带 OAR 替换结果**，因此面板显示原版动作；`false` 才走控制器合成（OAR 查询 → Smooth clip → `compose_contributor`）。证据：`reason=boundary` 计数 `true` 为 0、`false` 为 36（边界检测位于 [controller.cpp:1222-1238](../src/animation/controller.cpp) 的提前返回之后）。**这解释了长期存在的"面板动作与游戏世界不一致"**——是两条显示路径二选一，不是动作损坏。
> 2. **Start 驻留缺陷在 v126 上真实复现**：`DisplayPose=false` 下，`2hm`(4/4 边界)、`2hw Axe`(2/2)、`1hm`换装后(2/2) 连续驻留 `* idle Start`；`1hm` 首轮与 `2h` 正常驻留 Loop。`801`（`mag_start on`）在 20/20 次快照中从未被清除（`805` 出现 0 次）。
> 3. **撤回 priority 归因**：同一标记状态（`801=1 802=1 804=0`）下 `1hm` 选 Loop2、`2hm` 选 start——priority 两个方向都无法解释族间差异。新增候选差异：`1hm Start` 含 `OR[IsEquippedType == 1.0, right hand]`，`2hm start` 无此条（未验证）。
>
> 用户实测确认：**正常游玩待机不循环重放**（AE 档）。修复方向三选一（面板侧驻留 / 补派 `sp_start off` / 记录为限制），均未验证。
>
> **2026-11-25 结案：定为已知限制（方向 C）。** [C-WORLD v1](../docs/diagnostics/world-phase-20261125/README.md) 用新增的只读逐帧探针在**面板关闭**状态下采样 722 行：`mag_start on`(801) 在正常游玩中**同样从不被清除**（`phase=both` 401 行、`loop` 0 行），两次拔刀时间线同构（`start` ~3 秒 → `both` 至收刀）。因此缺陷不是"面板少做了一步"——世界也不做那一步。**方向 B（补派 `sp_start off`）已否决**（会清出世界中不存在的状态）；方向 A 依赖命名约定启发式且分歧来源未解释，同样不实施。契约 [§十](../docs/diagnostics/idle-loop-contract-20261123/README.md) 记录了最终事实、不判为面板缺陷的理由、以及重启所需的唯一实质问题（相同 `both` 标记状态下世界与面板的 OAR 选择分歧）与最小取证方案。面板内影响：idle 每轮重放一次起手段，动作正确、无扭曲；`DisplayPose=false` 时面板显示 Smooth 动作（`true` 则显示原版，见 v127）。

## Dependencies and verification

Existing Skyrim/SKSE/CommonLib/Havok, audited OAR and supported original DAC remain. INI parsing uses existing SimpleIni. No new package, DAC rebuild, TOML parser or UI is required. See [build/config setup](build.md).

The default-player evidence below belongs to earlier dated changes; it is not a fresh run for documentation completion. Exactly two direct-compiled [cases](../tests/animation_controller_test.cpp) exercise production policy/parser/preparation/transaction types with a narrow fake Actor boundary: default equipped-to-empty goal convergence/current tickets/entry restoration, and direct MT_Idle rejecting stale work after request/close. The first also checks ready-to-marker-loss invalidation. [Retained graph/math checks](../tests/snapshot_transform_test.cpp) are not executed for this change. Release compilation/static ownership review do not execute native detach/attach or magic APIs.

The separate [two sparse-cache cases](../tests/base_animation_cache_cases.h) exercise production resolution/cache policy amid 15,000 replacement names and project/table/load invalidation, including missing originals and selected-index names. See [direct compilation](build.md#fixed-base-index-verification). The retained graph/math driver shares these case definitions after removal of obsolete catalogue APIs; its other checks were not run for this change.

Earlier offline-only changes had no local runtime. AE 1.6.1170/MO2 sessions are now recorded in [r4](diagnostics/display-issues-2026-10-22.md#r4-session) and [G1/G2](independent-behavior-graph.md#current-status). Those bounded results do not establish complete native timing, dual/bow/custom-skeleton visuals, scabbards or lifetime correctness; this documentation refresh runs no game.

The earlier 2026-10-22 DAC/axe follow-up ran the [two event matching cases](../tests/dac_event_match_test.cpp), using the production comparator shared by `event_scope` and `smooth_idle_goal`; see [DAC matching and verification](dac.md#event-tag-matching). The separate torch/shield follow-up ran [two policy cases](../tests/torch_shield_policy_test.cpp), and low-overhead mapping ran [two mapping cases](../tests/animation_mapping_test.cpp). The issue-trace follow-up runs only [one budget case](../tests/diagnostic_trace_test.cpp), `request_budget_bounds_rearming_sampling_and_stale_windows`, exercising repeated rearm, both request caps, denial stopping stale samples, same-request non-reset, new-request reset, skipped slots and expiration. Earlier drivers are not rerun. Tests are engine-free; Release compilation and source review do not execute native API/conditions/UI tasks, loading, rendering or visuals. Runtime performance and root causes remain game-only evidence.

The current [v42 verification](build.md#v42-residual-corrections-2026-10-29) extends exactly the two session cases; the older v31/v32 results remain historical. Offline results do not prove the new real missing names or rejection branches. The [manual checklist](independent-behavior-graph.md#v32-manual-checklist) distinguishes completed v33/v34 observations from pending game checks.

## Safe modification and synchronized history

Keep Smooth goal derivation/observation and transaction authority in src/animation/dac; equipment policy/settings/leases/history in animation; copy attachment ownership in character. Preserve vanilla calibration, source audits, current tickets and graph parking. Do not add source graph events, virtual effects, parser fallbacks or folder playlists.

Synchronize this document, [DAC](dac.md), [README](../README.md), [Changelog](../CHANGELOG.md), [build](build.md), [OAR hooks](oar-hooks.md), [SceneGraph](scene-graph.md), [maintenance](code-maintenance.md), [runtime notes](../tools/scene_copy/README.txt), sample INI and code/tests. World-phase continuation is superseded by equipment-only idle policy; [historical summaries](history.md) preserve the recorded evidence and corrections, with original reports/logs recoverable through [the source index](history-sources.md).
