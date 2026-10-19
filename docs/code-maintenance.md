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
| [animation/controller.cpp](../src/animation/controller.cpp), [sequence.h](../src/animation/sequence.h) | isolated requests/leases, sequences, accepted continuation, visible-stage clocks and composed poses |
| [animation/clip.cpp](../src/animation/clip.cpp) | source clip observation, track→bone mapping and detached pose samples |
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

The preserved [animation test source](../tests/animation_controller_test.cpp) retains its test-only `CHARACTER_PANEL_TEST_AUTHOR` metadata bridge as reference material, but is no longer configured or built by CMake. Production retains generated Plugin metadata from the existing `PROJECT_AUTHOR`. Packaging SHA-256 checksums in historical records, Git/native identifiers, bit masks, PRNG seeds and the helper/bootstrap implementation keep their distinct semantics.

## Build, verification and controls

[Build guidance](build.md) owns the pinned checkout/toolchain, one Release preset, installed-dependency reuse, outputs and configure-time identity. The supported commands are `cmake --preset Release` followed by `cmake --build --preset Release`; the output is `build/Release/CharacterPanel.dll` with a matching PDB.

F7 queues recapture and F8 toggles display in a paused inventory. Current CMake does not expose test, package, install or compatibility entry points. Preserved [animation](../tests/animation_controller_test.cpp) and [graph/math](../tests/snapshot_transform_test.cpp) sources are reference material. Prior native results and their limits are recorded in [Build verification](build.md#verification-and-limits); build evidence does not establish game-time correctness.

## Debugging navigation

[main.cpp](../src/main.cpp), `initialize_log`, writes `CharacterPanel.log` under the directory returned by `SKSE::log::log_directory()`. Use that runtime-selected location rather than a guessed machine path. The current info-level log includes timestamps and thread IDs. Keep a returned log with the exact DLL/PDB pair and start with its `SCENEGRAPH BUILD` identity; that identity is generated at configure time, so a later commit alone does not update it.

| Log prefix/fields | Owner and purpose |
| --- | --- |
| `SCENEGRAPH BUILD` with identity/runtime/backend | [main.cpp](../src/main.cpp): match the configured binary before comparing results |
| `CharacterManager watch` / `CharacterManager lifetime boundary` | [character_manager.cpp](../src/character/character_manager.cpp): delimit the source record whose revisions/requests are being followed |
| `CharacterManager equipment actor=… revision=… equipped=…` | [character_manager.cpp](../src/character/character_manager.cpp), `EquipSink::ProcessEvent`: verify the source actor's equipment event reached the registry |
| `ANIMATION actor=… request=… reason=… source-generation=…` | [controller.cpp](../src/animation/controller.cpp), `AnimationController::request`: identify the per-actor request and source generation |
| `ANIMATION` asset ready/rejected, path/index, stage/action, cancel/discard stale proposal, graph retarget/time | [controller.cpp](../src/animation/controller.cpp), `poll`, `advance`, `retarget`: follow selection, accepted loads, handovers and phase preservation; group by actor/request |
| `SCOPY BEGIN`, `SOURCE-DIFF`, `AUDIT`, `CENSUS`, `READY`, `FAILED`, `BLOCKED` | [scene_graph_capture.cpp](../src/character/scene_graph_capture.cpp), private `capture_graph`: follow capture ID, correspondence/isolation and rejection reasons |
| `SCOPY PARK` with actor/count or cap warning | private `retire_graph`: tracks retained allocations; the cap limits bookkeeping only and does not bound retained native memory |
| `SCOPY FRAME`, `SceneGraph draw`, `SceneGraph actor=… draw unavailable` | [scene_graph.cpp](../src/character/scene_graph.cpp): framing/calibration, toggle state, target binding and absence of render passes |
| `OAR hooks unavailable/refused`, `OAR … hooks installed` | [hooks.cpp](../src/oar/hooks.cpp), `install_hooks`: distinguish verified query availability from a rejected/unavailable native profile |

In query lines, `action` is the caller's query channel (initial standing query 1, weapon idle query 2, transition query 4; loop rechecks also use 2). In stage lines it is the numeric [ActionKind](../src/animation/action.h); use the [action table](animation.md#interfaces) to interpret the stage.

Game follow-up remains two representative categories, not a new test matrix:

1. **Entry/equipment continuation:** inventory entry during Equip/Start/Loop, sheathe to standing and rapid equipment changes; correlate actor/request/stage logs and confirm the source actor remains unchanged.
2. **Copied-graph presentation/lifetime:** F7 without phase rewind, F8, close/reopen/load boundaries, appearance, attachments, framing and transparency; correlate capture/audit/park and draw logs.

These categories remain unverified without a Skyrim runtime. No new diagnostic code or per-frame tracing is implied.

## Safety and rendering invariants

- Source actors are read-only. Native `NiObject::Clone` copies a scene graph, not an Actor or base record.
- Event/input callbacks record revisions/intents. Capture, resource release, retarget and pose writes run in the serialized bracket.
- Replacing a graph keeps its SceneGraph/controller record and playback phase. Retarget precedes retirement of old nodes.
- Retired graph allocations stay parked. Safe native teardown remains unresolved.
- Preserve effect/collision filtering and attachment drawing, standing-idle calibration and detached roots.
- Preserve material alpha handling, opaque/cutout then ordered transparency, per-pass state restoration and all constant-buffer bindings/subranges. No shader/material change belongs to this cleanup.
- Local tests/builds do not establish game-time native loader, hook, visual or lifetime correctness.

## Historical evidence

The [complete historical index](history.md) keeps past controls, outcomes and proposals separate from current guidance.

[UBE transparency](ube-transparency-test-2026-10-15.md), [OAR ABI](oar-hooks.md), [OAR query experiments](oar-preview-query-2026-10-16.md), [AFP summary](afp-summary-2026-10-13.md), [S2 probes](s2-anim-probe-evidence-2026-10-09.md), [M0 handoff](m0-handoff.md) and [graph-copy validation](scene-graph-copy-validation.md) retain their recorded results. Their former source paths and hotkeys are historical; removed product code remains available through Git history. Evidence archives and old packages were not deleted.
