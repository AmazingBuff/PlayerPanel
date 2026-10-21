# Actor animation previews

## Purpose and scope

Show an independent equipment-driven non-combat idle in InventoryMenu. Equipped weapons are held in the copied graph even when the source Actor is sheathed, attacking, moving, sneaking or in combat. Opening never resumes a world phase or plays Equip/Unequip/entry Start. Inventory-only weapons do not count. Spells, staves, shields and torches retain their corresponding families. No Actor is created or controlled; the bounded source marker transaction belongs to [DAC](dac.md).

## Configuration

Copy [config/CharacterPanel.ini](../config/CharacterPanel.ini) to `Data/SKSE/Plugins/CharacterPanel.ini`:

```ini
[Animation]
UnarmedIdle=non_combat_loop
```

The accepted values are lowercase and affect empty hands only:

| `UnarmedIdle` | Empty-hand presentation | Native preparation / fallback |
| --- | --- | --- |
| `non_combat_loop` (default) | Query the OAR `h2h_idle.hkx` family with panel-drawn=true. | Observe the known Smooth goal first; use direct MT_Idle if preparation or a usable replacement fails. |
| `mt_idle` | Load vanilla `MT_Idle.hkx` directly with panel-drawn=false. | Bypass visible OAR replacements, preparation and annotation dispatch. |

A missing file uses the default; an invalid value logs one warning and uses the default. Settings load once at DataLoaded, so restart the game after edits. There is no new UI or live reload. Equipped spells, staves, shields and torches keep their corresponding families in either mode; carried inventory items do not count as equipped. Both modes retain the separate zero-time vanilla MT_Idle calibration.

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

## Interfaces

`ActionKind::e_standing_idle` (0) selects plain standing; `e_weapon_idle` (1) selects an equipped/unarmed family idle. All current targets loop. Movement/combat conditions remain projected inactive; panel-drawn comes from the target. Source equipment and identity remain actual reads.

Logical selection paths remain separate from loading index/binding name. Native interruptibility, replace-on-loop, blend durations and variant/PlayOnce/Random metadata remain copied values. Request generation immediately rejects obsolete queued mutations. Compatible refresh adopts the current generation only for future accepted events; changed equipment does not authorize the old clip's events.

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

## Invariants, failures and compatibility

Only current generations and observed effect revisions accept resources. Accepted playback remains visible during loading. Source graph replacement resets graph-specific resources while preserving the DAC transaction. Snapshots, clocks and history remain per Actor record. Source graph/equipment/draw flags stay read-only; only bounded DAC marker casts/restoration may mutate source state. Rendering, framing and parked graph retirement remain unchanged.

The [Smooth boundary](dac.md#smooth-moveset-compatibility-boundary) documents prerequisites and known protocol. The [three native IED/SDS queries](oar-hooks.md#native-ied-placement-conditions-2026-10-21) use current source equipment/configuration and retain the provider's left/right, comparison, shield setting and negation, including native false. The supplied provider's embedded 1.0.2.0 and exact condition 1.0.0.0 schemas are required (three components for placement, zero for `SDS_IsShieldOnBackEnabled`); historical-hand queries and unknown/missing providers reject. The shield branch needs its provider-side SDS registration/prerequisites; it is not a universal core non-combat dependency. Provider registration, paused IED hint refresh and game appearance remain unverified. Combat projection stays zero and the panel deliberately clears Smooth combat markers. No complete pack/custom-skeleton support is claimed.

Logs identify setting, actor/request/goal/family, preparation requested/ready/failure, copied-held counts/missing mounts, choices, loads, handovers and restoration. Missing project/base or failed base loading retains accepted playback or loading with diagnostics; no guessed binding/source mutation is a fallback. Unsupported third-party conditions remain rejected, not bypassed.

## Dependencies and verification

Existing Skyrim/SKSE/CommonLib/Havok, audited OAR and supported original DAC remain. INI parsing uses existing SimpleIni. No new package, DAC rebuild, TOML parser or UI is required. See [build/config setup](build.md).

Exactly two direct-compiled [cases](../tests/animation_controller_test.cpp) exercise production policy/parser/preparation/transaction types with a narrow fake Actor boundary: default equipped-to-empty goal convergence/current tickets/entry restoration, and direct MT_Idle rejecting stale work after request/close. The first also checks ready-to-marker-loss invalidation. [Retained graph/math checks](../tests/snapshot_transform_test.cpp) are not executed for this change. Release compilation/static ownership review do not execute native detach/attach or magic APIs.

The separate [two sparse-cache cases](../tests/base_animation_cache_cases.h) exercise production resolution/cache policy amid 15,000 replacement names and project/table/load invalidation, including missing originals and selected-index names. See [direct compilation](build.md#fixed-base-index-verification). The retained graph/math driver shares these case definitions after removal of obsolete catalogue APIs; its other checks were not run for this change.

No Skyrim runtime is installed here. Paused preparation/restore timing, dual/bow/custom-skeleton held geometry, scabbards, visuals and native lifetime remain unverified.

## Safe modification and synchronized history

Keep Smooth goal derivation/observation and transaction authority in src/animation/dac; equipment policy/settings/leases/history in animation; copy attachment ownership in character. Preserve vanilla calibration, source audits, current tickets and graph parking. Do not add source graph events, virtual effects, parser fallbacks or folder playlists.

Synchronize this document, [DAC](dac.md), [README](../README.md), [Changelog](../CHANGELOG.md), [build](build.md), [OAR hooks](oar-hooks.md), [SceneGraph](scene-graph.md), [maintenance](code-maintenance.md), [runtime notes](../tools/scene_copy/README.txt), sample INI and code/tests. World-phase continuation is superseded by equipment-only idle policy; dated [reports](history.md) retain their recorded behavior.
