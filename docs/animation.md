# Actor animation previews

## Purpose and scope

Show an independent equipment-driven non-combat idle in InventoryMenu. Equipped weapons are held in the copied graph even when the source Actor is sheathed, attacking, moving, sneaking or in combat. Opening never resumes a world phase or plays Equip/Unequip/entry Start. Inventory-only weapons do not count. Spells, staves, shields and torches retain their corresponding families. No Actor is created or controlled; the bounded source marker transaction belongs to [DAC](dac.md).

## Configuration

Copy [config/CharacterPanel.ini](../config/CharacterPanel.ini) to `Data/SKSE/Plugins/CharacterPanel.ini`:

```ini
[Animation]
UnarmedIdle=non_combat_loop

[Diagnostics]
DetailedAnimationTrace=false
```

The accepted values are lowercase and affect empty hands only:

| `UnarmedIdle` | Empty-hand presentation | Native preparation / fallback |
| --- | --- | --- |
| `non_combat_loop` (default) | Query the OAR `h2h_idle.hkx` family with panel-drawn=true. | Observe the known Smooth goal first; use direct MT_Idle if preparation or a usable replacement fails. |
| `mt_idle` | Load vanilla `MT_Idle.hkx` directly with panel-drawn=false. | Bypass visible OAR replacements, preparation and annotation dispatch. |

A missing file uses the default; an invalid value logs one warning and uses the default. Settings load once at DataLoaded, so restart the game after edits. There is no new UI or live reload. Equipped spells, staves, shields and torches keep their corresponding families in either mode; carried inventory items do not count as equipped. Both modes retain the separate zero-time vanilla MT_Idle calibration.

`DetailedAnimationTrace=false` is the normal diagnostic setting. Accepted panel mappings and event-driven WORLD snapshots remain enabled; set it to `true` and restart only when collecting bounded axe probes, annotation information, Smooth marker reports, candidate rejection detail or capture geometry listings. Dispatch, conditions, source/isolation audits and warnings/errors run in either mode. Ordinary information is buffered with periodic flushing about once per second; warnings/errors flush immediately. The flush timer only flushes sinks and performs no engine work.

## Architecture

`CharacterManager` distributes equipment revisions and menu/lifetime inputs. Each `SceneGraph` owns an `AnimationController` and `dac::Session`. `SourceObservation` carries equipped family/primary side, hand IDs, request/source-graph generations and reason. It contains no world clip, binding index/time or draw state. The source graph still supplies skeleton/bindings.

The animation module contains both integrations: [src/animation/oar](../src/animation/oar/hooks.h) owns native OAR queries and [src/animation/dac](../src/animation/dac/session.h) owns the marker transaction. This directory ownership retains the existing `oar` and `dac` namespaces and runtime boundaries. Production callers and offline tests include these headers from the configured `src` root, using `animation/oar/...` and `animation/dac/...`; see the [build/include convention](build.md#architecture-and-generated-outputs).

The production `IdleTarget` selects family and direct/query mode. Queried idles wait for `Session::prepare_idle` to establish the known Smooth `Smooth_loop` goal through real native casts and actual observed markers. Native DAC owns matching/filters/keyword dispel. Refresh behavior depends on the current goal state:

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
| [settings.h](../src/animation/settings.h), [settings.cpp](../src/animation/settings.cpp) | `load_settings`, `unarmed_idle_setting` |
| [controller.h](../src/animation/controller.h), [controller.cpp](../src/animation/controller.cpp) | `SourceObservation`, `AnimationController::request`, `poll`, `advance`, `retarget`, `apply_standing_pose` |
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

`ActionKind::e_standing_idle` (0) selects plain standing; `e_weapon_idle` (1) selects an equipped/unarmed family idle. All current targets loop. Movement/combat conditions remain projected inactive; panel-drawn comes from the target. Source equipment and identity remain actual reads.

Logical selection paths remain separate from loading index/binding name. Native interruptibility, replace-on-loop, blend durations and variant/PlayOnce/Random metadata remain copied values. Request generation immediately rejects obsolete queued mutations. Compatible refresh adopts the current generation only for future accepted events; changed equipment does not authorize the old clip's events.

### Torch weather and left-shield transitions (2026-10-22)

`CurrentWeatherHasFlag` is admitted by [forwards_source](../src/animation/oar/policy.h). OAR reads the actual current weather and retains its own false/negation behavior; the panel supplies no constant weather answer. This fixes the confirmed unsupported-condition rejection of TOR_Idle candidates. A candidate can still fail its native conditions; torch/world visual equality is not established by admission alone.

[left_shield_transition](../src/animation/idle_policy.h) handles the narrow unchanged-right-weapon case used by `AnimationController::request`: adding a left shield selects Equip with current equipment context, while removing that shield selects `1HM_Unequip` even though the right weapon remains equipped. The controller limits this policy to the one-handed family and identifies a real `TESObjectARMO::IsShield`. Existing torch-hand routing and other families retain their current transition policy.

Shield removal passes the former hand IDs through `PreviewState::hand_override` for the existing core `IsEquipped`, `IsEquippedType` and `IsEquippedHasKeyword` reads. The private proposal retains the shield action and preview context if an effect revision changes during loading; `poll` requeues that same intent rather than replacing it with an idle. The following idle receives a fresh current-equipment context. No source equipment, Actor or graph state changes. Historical instance-state predicates and historical IED placement remain unsupported, so an IED-dependent shield Unequip can still fall to a basic move. This does not guarantee every shield pack's equip/unequip appearance.

`ANIMATION WORLD` records a **paused source snapshot** at panel-open and equipment requests, including reason, current hand IDs and actual source drawn state. It is not a settled post-change world transition or the panel's projected drawn state. The [current low-overhead mapping format](#accepted-mappings-and-world-snapshots) uses raw authored `original`, direct actual `index/bound`, and the official API's separate replacement fields. It replaces the earlier reverse lookup and `lookup/replaced/original-index` fields. An empty API response remains unknown, not proof of vanilla playback.

`IsMovementDirection` is a standing predicate in [policy.h](../src/animation/oar/policy.h): the preview figure stands still, so every direction answers false and the negated arm of Smooth's non-combat gate — `OR( HasMagicEffect(loop marker), NOT IsMovementDirection(0) )` — is true before the loop marker is cast. The previous implementation read the condition's numeric component, which carries the *requested* direction rather than the actor's motion, and therefore answered true for direction 0; every `Non-Combat Equip/Unequip Base` variant was rejected during a transition and the combat variant (or another pack's clip) was displayed instead. `IsCombatState` keeps its numeric read because its component holds the requested state and the standing figure is out of combat (state 0). This is admission of a standing answer, not a new lenient unknown-condition path.

With detailed tracing enabled, per-query `OAR QUERY` diagnostics publish **one full candidate path per distinct project folder and rejection cause** (first signature, at most forty-eight) followed by one suppressed-count line; the earlier eight-unsupported/two-native-false split is retired, because a single pack's repeated condition could consume the whole budget before a later folder's cause was reached. Unsupported conditions identify provider and condition name; an otherwise ordinary native rejection is labelled as native conditions/eligibility returning false without guessing which condition failed. `first-query-rejection` on a traced proposal belongs to an earlier rejected candidate, not necessarily the selected candidate. Read [the OAR diagnostic boundary](oar-hooks.md#correlated-rejection-evidence-2026-10-22) and the [dated test procedure](diagnostics/display-issues-2026-10-22.md#9-2026-10-22-换装片源与站立谓词修复).

### Accepted mappings and WORLD snapshots

Default `ANIMATION PANEL ... accepted` records describe only ready, accepted playback: final loader fallback at handover, or accepted same-asset reuse after the current query/history checks. A queued proposal is never a playing record. `original/base-index` identify the accepted base, `logical` is the selected logical replacement asset path (empty for base playback), `variant` gives the accepted member index/position or `none`, and `index/bound` name the actual engine binding. Hash-deduplicated logical paths may share that binding. The final bound name is read by index at publication, so loader fallback cannot publish an earlier rejected replacement's stale name.

`MappingPublication` retains just one accepted value per controller. Identical mapping acceptance on repeated loops within a request produces no new mapping line; a new request or a changed accepted action/logical path/variant/binding/status publishes again. Pending/rejected proposals do not update it. No accepted-winner cache is used for selection, and no animation/history policy changes. Proposal path detail is opt-in and explicitly labelled `proposal`; the all-candidate `oar walk` is removed entirely.

WORLD calls the optional official OAR Animations V1 API on each actual active source clip. `original` stays the raw authored clip name, while `index/bound` is a guarded direct selected-entry read. `oar-path`, `oar-project`, `oar-variant`, `oar-submod` and `oar-mod` remain separate OAR-reported fields. `oar-api=available oar-result=replacement` is positive current-replacement information; `empty-unknown` or `unavailable-unknown` preserves the actual bound asset without guessing vanilla or a unique logical alias. Synthetic panel clips are never registered with or queried through this current-world API.

There is no logging-only full-table reverse scan, replacement/variant catalogue copy, deferred reverse scan or full-name cache. The finite original-base prewarm and functional selection metadata remain unchanged. The API may scan members of its current replacement and copy several strings; it is not globally free or a constant-time performance guarantee. `build-ms` measures synchronous snapshot/mapping construction before the log call, excluding sink writes/flushes and later frame rendering. It is not disk time, end-to-end inventory latency or FPS. See [API lifecycle, examples and verification](oar-hooks.md#low-overhead-mapping-and-opt-in-traces-2026-10-22).

## Fixed base indices and project readiness

`panel_base_animation_names` derives the finite registry from the existing `panel_idle_bases` and `panel_transition_bases` aliases for all eleven weapon families. It includes H2H/MT_Idle, one/two-handed, axe, bow/crossbow, dual wield, magic, staff, torch and shield, with equip/unequip aliases and vanilla MT_Idle calibration/fallback. Numeric binding indices are discovered from each project's actual table. Canonical slash/case matching, exact-before-gender/suffix matching within a candidate, ordered alias priority and left-primary idle order remain intact. OAR/DAR replacement folders cannot supply original bases.

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

The current **33 canonical original-name keys include aliases**. They do not limit the number of OAR replacements, variants or custom weapon records behind those entries. A mod introducing an independent behavior/clip entry outside the mapped families requires an explicit family/base mapping before the panel can request it. The old full-table catalogue also used the same action-family policy; storing every name did not discover or schedule new families automatically.

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

`ANIMATION HANDOFF` reports the existing `handoff-seconds` and whether the first weaponDraw/weaponSheathe annotation supplied it or the duration midpoint remained. `ANIMATION ANNOTATION` includes accepted action/index/path, annotation/clip time, loop and queue-native-DAC versus direct-no-dispatch. Follow it into the [DAC queued/skipped/completed/observed chain](dac.md#opt-in-annotation-and-marker-evidence), then the next query/acceptance; completion alone is not readiness.

For the immediate axe flash, align capture reason/old-new graph, raw placement, existing calibration measurements, actual mount switches and the first POSE slots. [Scene evidence](scene-graph.md#opt-in-capture-placement-and-mount-evidence) adds one fixed fifteen-bone paused-source snapshot per request during the already-required transform-copy pass, not another traversal. Compare source and panel only after matching IDs, phase and coordinate context. Missing/exhausted data is not evidence that a cause is absent. The [current dated procedure](diagnostics/display-issues-2026-10-22.md#9-2026-10-22-换装片源与站立谓词修复) covers torch/shield, Smooth phases, axe and generic arms/gear separately, with the [section-8 budget](diagnostics/display-issues-2026-10-22.md#8-2026-10-22-已报告问题的关联诊断) defining the pose windows.

## Invariants, failures and compatibility

Only current generations and observed effect revisions accept resources. Accepted playback remains visible during loading. Source graph replacement resets graph-specific resources while preserving the DAC transaction. Snapshots, clocks and history remain per Actor record. Source graph/equipment/draw flags stay read-only; only bounded DAC marker casts/restoration may mutate source state. Rendering, framing and parked graph retirement remain unchanged.

The [Smooth boundary](dac.md#smooth-moveset-compatibility-boundary) documents prerequisites and known protocol. The [three explicitly checked IED/SDS schemas](oar-hooks.md#native-ied-placement-conditions-2026-10-21) use current source equipment/configuration and retain the provider's left/right, comparison, shield setting and negation, including native false. The matched provider must have embedded version 1.0.2.0; the three named schemas also require condition version 1.0.0.0 and their exact components. Other names from that same provider keep native current-source answers without those individual schema/version checks. Historical-hand queries, unknown/missing providers and mismatched checked schemas reject. The shield branch needs its provider-side SDS registration/prerequisites; it is not a universal core non-combat dependency. Provider registration, paused IED hint refresh and game appearance remain unverified. Combat projection stays zero and the panel deliberately clears Smooth combat markers. No complete pack/custom-skeleton support is claimed.

Logs identify setting, actor/request/goal/family, preparation requested/ready/failure, copied-held counts/missing mounts, choices, loads, handovers and restoration. Missing project/base or failed base loading retains accepted playback or loading with diagnostics; no guessed binding/source mutation is a fallback. Unsupported third-party conditions remain rejected, not bypassed.

## Dependencies and verification

Existing Skyrim/SKSE/CommonLib/Havok, audited OAR and supported original DAC remain. INI parsing uses existing SimpleIni. No new package, DAC rebuild, TOML parser or UI is required. See [build/config setup](build.md).

Exactly two direct-compiled [cases](../tests/animation_controller_test.cpp) exercise production policy/parser/preparation/transaction types with a narrow fake Actor boundary: default equipped-to-empty goal convergence/current tickets/entry restoration, and direct MT_Idle rejecting stale work after request/close. The first also checks ready-to-marker-loss invalidation. [Retained graph/math checks](../tests/snapshot_transform_test.cpp) are not executed for this change. Release compilation/static ownership review do not execute native detach/attach or magic APIs.

The separate [two sparse-cache cases](../tests/base_animation_cache_cases.h) exercise production resolution/cache policy amid 15,000 replacement names and project/table/load invalidation, including missing originals and selected-index names. See [direct compilation](build.md#fixed-base-index-verification). The retained graph/math driver shares these case definitions after removal of obsolete catalogue APIs; its other checks were not run for this change.

No Skyrim runtime is installed here. Paused preparation/restore timing, dual/bow/custom-skeleton held geometry, scabbards, visuals and native lifetime remain unverified.

The earlier 2026-10-22 DAC/axe follow-up ran the [two event matching cases](../tests/dac_event_match_test.cpp), using the production comparator shared by `event_scope` and `smooth_idle_goal`; see [DAC matching and verification](dac.md#event-tag-matching). The separate torch/shield follow-up ran [two policy cases](../tests/torch_shield_policy_test.cpp), and low-overhead mapping ran [two mapping cases](../tests/animation_mapping_test.cpp). The issue-trace follow-up runs only [one budget case](../tests/diagnostic_trace_test.cpp), `request_budget_bounds_rearming_sampling_and_stale_windows`, exercising repeated rearm, both request caps, denial stopping stale samples, same-request non-reset, new-request reset, skipped slots and expiration. Earlier drivers are not rerun. Tests are engine-free; Release compilation and source review do not execute native API/conditions/UI tasks, loading, rendering or visuals. Runtime performance and root causes remain game-only evidence.

## Safe modification and synchronized history

Keep Smooth goal derivation/observation and transaction authority in src/animation/dac; equipment policy/settings/leases/history in animation; copy attachment ownership in character. Preserve vanilla calibration, source audits, current tickets and graph parking. Do not add source graph events, virtual effects, parser fallbacks or folder playlists.

Synchronize this document, [DAC](dac.md), [README](../README.md), [Changelog](../CHANGELOG.md), [build](build.md), [OAR hooks](oar-hooks.md), [SceneGraph](scene-graph.md), [maintenance](code-maintenance.md), [runtime notes](../tools/scene_copy/README.txt), sample INI and code/tests. World-phase continuation is superseded by equipment-only idle policy; dated [reports](history.md) retain their recorded behavior.
