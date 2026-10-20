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

The production `IdleTarget` selects family and direct/query mode. Queried idles wait for `Session::prepare_idle` to establish the known Smooth `Smooth_loop` goal through real native casts and actual observed markers. Native DAC owns matching/filters/keyword dispel. Refresh behavior depends on the current goal state:

- An unchanged compatible target whose goal is already ready queues an observation-only check, preserving matching accepted clock/variant without recasting.
- If preparation is still pending, a newer refresh request starts one preparation attempt for its own generation; obsolete queued work cannot cast. Compatible in-flight clip leases remain retained behind the readiness gate, then undergo the current query and generation/effect-revision checks before acceptance.
- Once the controller retains a direct fallback, compatible appearance/F7 refresh keeps that fallback and skips preparation. Equipment changes clear it; a replaced source graph/controller lifetime also starts fresh.

None of these paths reseeds the original inventory entry snapshot.

Missing DAC/Smooth, unsafe tags or a two-second observation timeout do not force the vanilla base: OAR is still queried against the source's actual effects, and a base selection is the last resort. Armed requests use their equipped family's base, never MT_Idle merely because the source was sheathed. Empty-hand queries without a usable replacement, including condition rejection or failed replacement loading, use direct MT_Idle instead of vanilla combat-looking fists. Once retained, the direct fallback is stable across compatible refreshes until equipment changes or the graph/controller lifetime resets. No generic protocol is inferred for other movesets.

## Code map

| Path | Stable entry point |
| --- | --- |
| [action.h](../src/animation/action.h), [idle_policy.h](../src/animation/idle_policy.h) | `weapon_form_from_hands`, `panel_idle_bases`, `IdleTarget`, `idle_target`, `parse_unarmed_idle` |
| [settings.h](../src/animation/settings.h), [settings.cpp](../src/animation/settings.cpp) | `load_settings`, `unarmed_idle_setting` |
| [controller.h](../src/animation/controller.h), [controller.cpp](../src/animation/controller.cpp) | `SourceObservation`, `AnimationController::request`, `poll`, `advance`, `retarget`, `apply_standing_pose` |
| [event_clock.h](../src/animation/event_clock.h), [event_clock.cpp](../src/animation/event_clock.cpp) | `EventClock`, `Annotation`, `clip_annotations` |
| [sequence.h](../src/animation/sequence.h), [variant_state.h](../src/animation/variant_state.h) | `Stage`, pose composition and accepted `SelectionState` |
| [clip.h](../src/animation/clip.h), [clip.cpp](../src/animation/clip.cpp) | `ClipPlayback`, retained binding ownership, track mapping and detached samples |
| [session.h](../src/dac/session.h), [idle_goal.h](../src/dac/idle_goal.h) | `Session::prepare_idle`, `idle_readiness`, `IdlePreparation` |
| [held_equipment.cpp](../src/character/held_equipment.cpp) | `hold_copy_equipment`, owned-copy hand mounting |
| [hooks.cpp](../src/oar/hooks.cpp) | copied OAR metadata and actual source-effect conditions |
| [animation_controller_test.cpp](../tests/animation_controller_test.cpp) | two policy/preparation/transaction cases |

## Interfaces

`ActionKind::e_standing_idle` (0) selects plain standing; `e_weapon_idle` (1) selects an equipped/unarmed family idle. All current targets loop. Movement/combat conditions remain projected inactive; panel-drawn comes from the target. Source equipment and identity remain actual reads.

Logical selection paths remain separate from loading index/binding name. Native interruptibility, replace-on-loop, blend durations and variant/PlayOnce/Random metadata remain copied values. Request generation immediately rejects obsolete queued mutations. Compatible refresh adopts the current generation only for future accepted events; changed equipment does not authorize the old clip's events.

## Event timing and native state

`clip_annotations` reads retained HKX annotations, validates times and stable-sorts them. `EventClock` dispatches crossed occurrences once, including zero/end/loop boundaries. Only accepted queried playback emits native events. Loading, direct base modes/fallbacks, calibration, standing underlay and outgoing fades do not dispatch. Compatible refresh and retarget preserve clock/cursor; frame delta remains clamped to 0-0.1 seconds.

SKSE UI observations publish actual native effect revisions; function return is not readiness. OAR reads real source effects. Pending choices recheck revisions and retain compatible leases. Interruptible idles may requery on changes. Loop boundaries retain native replacement/sequence/PlayOnce/Random scope semantics; same-replacement appearance/interrupt checks preserve accepted variant/history. A Start folder member is detected by name, played once with the loop flag off, and the following boundary re-queries (so Equip -> Start -> Loop is preserved); there is no folder playlist.

## Pose mixing and placement

Outgoing and incoming poses both advance during fades using copied native durations and between-variant policy. This detached composition does not call OAR's ActiveClip blender or implement its lookahead/end-relative seek. Incoming clips start at zero. Calibration independently samples vanilla MT_Idle at zero through `apply_standing_pose`, before the current pose, regardless of the setting.

Generic Weapon, Shield and Quiver tracks map independently to WEAPON, SHIELD and QUIVER, never sheath WeaponSword/MOV nodes. [Capture](scene-graph.md#held-equipment-invariant) remounts only mapped copy equipment and preserves model-local transforms.

## Invariants, failures and compatibility

Only current generations and observed effect revisions accept resources. Accepted playback remains visible during loading. Source graph replacement resets graph-specific resources while preserving the DAC transaction. Snapshots, clocks and history remain per Actor record. Source graph/equipment/draw flags stay read-only; only bounded DAC marker casts/restoration may mutate source state. Rendering, framing and parked graph retirement remain unchanged.

The [Smooth boundary](dac.md#smooth-moveset-compatibility-boundary) documents prerequisites and known protocol. IED placement queries still require an explicit adapter even with its provider installed. Combat projection stays zero and the panel deliberately clears Smooth combat markers. No complete pack/custom-skeleton support is claimed.

Logs identify setting, actor/request/goal/family, preparation requested/ready/failure, copied-held counts/missing mounts, choices, loads, handovers and restoration. Missing project/base or failed base loading retains accepted playback or loading with diagnostics; no guessed binding/source mutation is a fallback. Unsupported third-party conditions remain rejected, not bypassed.

## Dependencies and verification

Existing Skyrim/SKSE/CommonLib/Havok, audited OAR and supported original DAC remain. INI parsing uses existing SimpleIni. No new package, DAC rebuild, TOML parser or UI is required. See [build/config setup](build.md).

Exactly two direct-compiled [cases](../tests/animation_controller_test.cpp) exercise production policy/parser/preparation/transaction types with a narrow fake Actor boundary: default equipped-to-empty goal convergence/current tickets/entry restoration, and direct MT_Idle rejecting stale work after request/close. The first also checks ready-to-marker-loss invalidation. [Retained graph/math checks](../tests/snapshot_transform_test.cpp) are not executed for this change. Release compilation/static ownership review do not execute native detach/attach or magic APIs.

No Skyrim runtime is installed here. Paused preparation/restore timing, dual/bow/custom-skeleton held geometry, scabbards, visuals and native lifetime remain unverified.

## Safe modification and synchronized history

Keep Smooth goal derivation/observation and transaction authority in src/dac; equipment policy/settings/leases/history in animation; copy attachment ownership in character. Preserve vanilla calibration, source audits, current tickets and graph parking. Do not add source graph events, virtual effects, parser fallbacks or folder playlists.

Synchronize this document, [DAC](dac.md), [README](../README.md), [Changelog](../CHANGELOG.md), [build](build.md), [OAR hooks](oar-hooks.md), [SceneGraph](scene-graph.md), [maintenance](code-maintenance.md), [runtime notes](../tools/scene_copy/README.txt), sample INI and code/tests. World-phase continuation is superseded by equipment-only idle policy; dated [reports](history.md) retain their recorded behavior.
