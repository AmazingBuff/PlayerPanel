# Code maintenance

The sole current preview backend is [SceneGraph](scene-graph.md); animation ownership and phase rules are documented in [animation previews](animation.md). Dated reports linked below remain historical evidence for their recorded revisions.

## Current ownership and source map

| Module | Responsibility |
| --- | --- |
| [scene_graph.h](../src/character/scene_graph.h), [scene_graph.cpp](../src/character/scene_graph.cpp) | persistent per-source-actor observation/controller, F7/F8 intents, capture/retarget decisions, placement/calibration, animation and draw |
| [character_manager.cpp](../src/character/character_manager.cpp) | source registry, equipment-event revisions and menu/frame/lifetime distribution |
| [scene_graph_capture.cpp](../src/character/scene_graph_capture.cpp) | private SceneGraph native copy, writable-isolation audits and process-lifetime retirement |
| [equipment_state.cpp](../src/character/equipment_state.cpp) | read-only hand/form observations |
| [figure_audit.cpp](../src/character/figure_audit.cpp), [snapshot_transform.h](../src/character/snapshot_transform.h) | appearance-settle signature, collision filtering, placement and subtree math |
| [animation/controller.cpp](../src/animation/controller.cpp), [sequence.h](../src/animation/sequence.h) | isolated requests/leases, current-state choices, accepted continuation, event clocks and composed poses |
| [animation/event_clock.h](../src/animation/event_clock.h), [event_clock.cpp](../src/animation/event_clock.cpp) | retained HKX annotation timing; no DAC rules or effect emulation |
| [dac/session.cpp](../src/dac/session.cpp), [native.cpp](../src/dac/native.cpp), [effects.cpp](../src/dac/effects.cpp) | UI task/menu epochs, original native DAC handler/loaded metadata, marker-scope preflight and engine snapshot restoration |
| [animation/clip.cpp](../src/animation/clip.cpp) | equipment-independent binding resolution, track→bone mapping and detached pose samples |
| [oar/hooks.cpp](../src/oar/hooks.cpp) | verified query-only OAR condition/candidate/variant boundary |
| [panel_monitor.cpp](../src/panel/panel_monitor.cpp) | InventoryMenu state and revision only |
| [input.cpp](../src/input/input.cpp), [main.cpp](../src/main.cpp), [renderer.cpp](../src/render/renderer.cpp) | queued intent, installation/lifetime signals, serialized update and drawing |
| [figure_passes.cpp](../src/render/studio/figure_passes.cpp), [figure_draw_order.h](../src/render/studio/figure_draw_order.h), [figure_pipeline_state.h](../src/render/studio/figure_pipeline_state.h) | existing material/pass submission and transparency/state restoration |
| [d3d11_util.cpp](../src/render/dx11/d3d11_util.cpp) | render-target and device binding protection |

## Runtime hash composition

Custom process-local keys and fingerprints use [base/def.h](../src/base/def.h) `hash_combine`, starting from `Amazing_Hash`. The seed remains derived from `Plugin::Plugin_Author` by the supplied `hash_str`; do not duplicate the algorithm or hardcode a second seed. Combine logical fields individually in a fixed order, never raw struct bytes.

- [StateKeyHash and condition RandomKeyHash](../src/oar/selection_state.h): owner → group → scope, then condition for the random key.
- [Animation RandomKeyHash](../src/animation/variant_state.h): extend the same StateKeyHash with condition.
- [query_animation](../src/oar/hooks.cpp): project/wanted pointer → base index → channel for `clip_key`.
- [FigureAssembly::signature](../src/character/figure_audit.h): geometries → skinned → bound_nodes → alloc_slots, retaining the full `size_t` result.

The [animation test source](../tests/animation_controller_test.cpp) compiles the production idle policy, preparation progress and transaction core directly, without the generated author metadata or a CMake target. Production retains generated Plugin metadata from `PROJECT_AUTHOR`. Packaging SHA-256 checksums in historical records, Git/native identifiers, bit masks, PRNG seeds and the helper/bootstrap implementation keep their distinct semantics.

## Build, verification and controls

[Build guidance](build.md) owns the pinned checkout/toolchain, one Release preset, installed-dependency reuse, outputs and configure-time identity. The supported commands are `cmake --preset Release` followed by `cmake --build --preset Release`; the output is `build/Release/CharacterPanel.dll` with a matching PDB.

[DAC maintenance](dac.md#dependencies-and-offline-inspection) uses `src/dac/inspect_binary.py` against untouched original DLL/PDB inputs. Python/LLVM are offline tooling only; keep the original DAC installation and replace only CharacterPanel.dll.

F7 queues recapture and F8 toggles display in a paused inventory. Current CMake does not expose test, package, install or compatibility entry points. The two [animation](../tests/animation_controller_test.cpp) cases compile directly through [the build guide](build.md#verification-and-limits); retained [graph/math](../tests/snapshot_transform_test.cpp) checks have no CMake target. Prior native results and their limits are recorded in [Build verification](build.md#verification-and-limits); build evidence does not establish game-time correctness.

## Debugging navigation

[main.cpp](../src/main.cpp), `initialize_log`, writes `CharacterPanel.log` under the directory returned by `SKSE::log::log_directory()`. Use that runtime-selected location rather than a guessed machine path. The current info-level log includes timestamps and thread IDs. Keep a returned log with the exact DLL/PDB pair and start with its `SCENEGRAPH BUILD` identity; that identity is generated at configure time, so a later commit alone does not update it.

| Log prefix/fields | Owner and purpose |
| --- | --- |
| `SCENEGRAPH BUILD` with identity/runtime/backend | [main.cpp](../src/main.cpp): match the configured binary before comparing results |
| `CharacterManager watch` / `CharacterManager lifetime boundary` | [character_manager.cpp](../src/character/character_manager.cpp): delimit the source record whose revisions/requests are being followed |
| `CharacterManager equipment actor=… revision=… equipped=…` | [character_manager.cpp](../src/character/character_manager.cpp), `EquipSink::ProcessEvent`: verify the source actor's equipment event reached the registry |
| `ANIMATION actor=… request=… reason=… source-generation=…` | [controller.cpp](../src/animation/controller.cpp), `AnimationController::request`: identify the per-actor request and source generation |
| `ANIMATION` asset ready/rejected, path/index, choice/state revision, action/handover/blend, stale proposal, graph retarget/time | [controller.cpp](../src/animation/controller.cpp), `poll`, `advance`, `retarget`: follow target selection, accepted loads, handovers and compatible clock preservation; group by actor/request |
| `SCOPY BEGIN`, `SOURCE-DIFF`, `AUDIT`, `CENSUS`, `READY`, `FAILED`, `BLOCKED` | [scene_graph_capture.cpp](../src/character/scene_graph_capture.cpp), private `capture_graph`: follow capture ID, correspondence/isolation and rejection reasons |
| `SCOPY PARK` with actor/count or cap warning | private `retire_graph`: tracks retained allocations; the cap limits bookkeeping only and does not bound retained native memory |
| `SCOPY FRAME`, `SceneGraph draw`, `SceneGraph actor=… draw unavailable` | [scene_graph.cpp](../src/character/scene_graph.cpp): framing/calibration, toggle state, target binding and absence of render passes |
| `OAR hooks unavailable/refused`, `OAR … hooks installed` | [hooks.cpp](../src/oar/hooks.cpp), `install_hooks`: distinguish verified query availability from a rejected/unavailable native profile |
| `DAC snapshot`, native event/revision, rejection, `DAC restore ... result=matched|MISMATCH` | [session.cpp](../src/dac/session.cpp), [effects.cpp](../src/dac/effects.cpp): follow immutable inventory entry, native state visibility and checked close restoration |

`ANIMATION setting` identifies the loaded [sample option](../config/CharacterPanel.ini); goal/family/direct/drawn logs identify equipment policy. `DAC ... Smooth_loop preparation requested/ready/failed` separates UI submission from observed marker readiness. `SCOPY HELD` reports attached/missing roots and concrete side/form/mount problems. See [idle policy](animation.md) and [held attachment invariant](scene-graph.md#held-equipment-invariant). Ready compatible refresh observes without recasting; pending refresh creates one current-generation preparation attempt, while an already retained direct fallback skips preparation. Check the [precise refresh contract](dac.md#non-combat-idle-goal-preparation) before interpreting repeated request logs.

`choice` records request/interrupt/boundary/appearance-refresh reasons (0/1/2/3). Handover `action` is the numeric [ActionKind](../src/animation/action.h); see [the action values](animation.md#interfaces). Native DAC diagnostics report exact profile compatibility, unsupported event scope and snapshot/restore result. Annotation logs include event/time; native call completion and actual observed effect revisions are separate. See `native_rules`, `Session::dispatch`, `capture_effects`, `restore_effects` and `AnimationController::advance`.

The [IED/SDS adapter](oar-hooks.md#native-ied-placement-conditions-2026-10-21) covers exactly the equipped-weapon placement, gear-node placement and zero-component `SDS_IsShieldOnBackEnabled` predicates for current source equipment/configuration. `OAR IED placement adapter` reports provider metadata availability/version at hook installation; it does not prove OAR factory or SDS interface registration. `ied_condition_rejection` in [ied_conditions.h](../src/oar/ied_conditions.h) distinguishes missing/version-mismatched provider, unsupported name/schema/version and historical-hand scope. `audit` in [hooks.cpp](../src/oar/hooks.cpp) also covers custom virtual `Evaluate` before dispatch; `Frame` preserves scoped numeric reads. Native false is a valid false, not a compatibility error. The two [behavior cases](../tests/ied_condition_adapter_test.cpp) use the [direct build command](build.md#ied-condition-verification); the native-preservation scenario includes the SDS false result and negation.

`third-party condition requires an explicit preview adapter` still identifies unrelated third-party providers; consult the [Smooth support boundary](dac.md#smooth-moveset-compatibility-boundary). Do not diagnose it as missing DAC state or bypass it. Confirm the exact provider (archive 1.0.3, embedded 1.0.2.0), its own IED/SDS registration logs and actual hints/settings when checking paused-game behavior. The SDS branch needs its provider-side prerequisites; it is not universally required by the core non-combat goal. Combat configs retain the TDM-effect OR combat-state requirement and current combat-zero projection.

Game follow-up remains two representative categories, not a new test matrix:

1. **Equipment idle policy:** verify the same non-combat equipped target during world draw/attack/move/sheathe, both empty-hand INI choices and rapid equipment changes; correlate actor/request/event/effect/handover logs and confirm controlled source effects restore and unrelated equipment/buffs remain intact.
2. **Copied-graph presentation/lifetime:** F7 without compatible-loop rewind, F8, close/reopen/load boundaries, appearance, attachments, framing and transparency; correlate capture/audit/park and draw logs.

These categories remain unverified without a Skyrim runtime. No new diagnostic code or per-frame tracing is implied. The current round's concrete checklist, expected log lines and result table are in the [equipment idle/DAC test sheet](equipment-idle-dac-test-2026-10-20.md).

## Safety and rendering invariants

- Source graphs/transforms remain read-only. Native `NiObject::Clone` copies a scene graph, not an Actor. The scoped [native DAC transaction](dac.md) temporarily changes only controlled source effects and restores them at close.
- Event/input callbacks record revisions/intents. Capture, resource release, retarget and pose writes run in the serialized bracket.
- Replacing a graph keeps its SceneGraph/controller record and playback phase. Retarget precedes retirement of old nodes.
- Retired graph allocations stay parked. Safe native teardown remains unresolved.
- Preserve effect/collision filtering and attachment drawing, standing-idle calibration and detached roots.
- Preserve material alpha handling, opaque/cutout then ordered transparency, per-pass state restoration and all constant-buffer bindings/subranges. No shader/material change belongs to this cleanup.
- Local tests/builds do not establish game-time native loader, hook, visual or lifetime correctness.

## Historical evidence

The [complete historical index](history.md) keeps past controls, outcomes and proposals separate from current guidance.

[UBE transparency](ube-transparency-test-2026-10-15.md), [OAR ABI](oar-hooks.md), [OAR query experiments](oar-preview-query-2026-10-16.md), [AFP summary](afp-summary-2026-10-13.md), [S2 probes](s2-anim-probe-evidence-2026-10-09.md), [M0 handoff](m0-handoff.md) and [graph-copy validation](scene-graph-copy-validation.md) retain their recorded results. Their former source paths and hotkeys are historical; removed product code remains available through Git history. Evidence archives and old packages were not deleted.
