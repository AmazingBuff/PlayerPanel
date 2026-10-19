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

The dependency-light [animation test](../tests/animation_controller_test.cpp) receives `CHARACTER_PANEL_TEST_AUTHOR` from the existing `PROJECT_AUTHOR` in [CMake](../CMakeLists.txt), then uses the real helper. Production/math consumers retain generated Plugin metadata. Packaging SHA-256 checksums, Git/native identifiers, bit masks, PRNG seeds and the helper/bootstrap implementation keep their distinct semantics.

## Build, tests and controls

Use the commands in [SceneGraph verification](scene-graph.md#verification). `Release` and compatibility `Release-scopy` compile the same source/backend; there is no runtime-route option. The installed VS2022/MSVC 14.44.35207 toolset and existing vcpkg dependencies remain unchanged. Native builds may need SDK access outside the sandbox and `VCPKG_MANIFEST_INSTALL=OFF` with the existing installed-package directory.

F7 queues recapture and F8 toggles display while a paused inventory is open. [Package instructions](../tools/scene_copy/README.txt) describe identity and manual validation. The compatibility `CharacterPanelSceneCopyPackage` target creates a package; it does not install it into a game directory.

`CharacterPanelAnimationTest` / `animation_controller` contains the two selected behavioral cases. `CharacterPanelCopyMathTest` is rebuilt to check retained geometry/track/render/query consumers; its whole suite is not run for this consolidation. Old Actor/final-output/body-morph test consumers are removed with their product APIs.

## Safety and rendering invariants

- Source actors are read-only. Native `NiObject::Clone` copies a scene graph, not an Actor or base record.
- Event/input callbacks record revisions/intents. Capture, resource release, retarget and pose writes run in the serialized bracket.
- Replacing a graph keeps its SceneGraph/controller record and playback phase. Retarget precedes retirement of old nodes.
- Retired graph allocations stay parked. Safe native teardown remains unresolved.
- Preserve effect/collision filtering and attachment drawing, standing-idle calibration and detached roots.
- Preserve material alpha handling, opaque/cutout then ordered transparency, per-pass state restoration and all constant-buffer bindings/subranges. No shader/material change belongs to this cleanup.
- Local tests/builds do not establish game-time native loader, hook, visual or lifetime correctness.

## Historical evidence

[UBE transparency](ube-transparency-test-2026-10-15.md), [OAR ABI](oar-hooks.md), [OAR query experiments](oar-preview-query-2026-10-16.md), [AFP summary](afp-summary-2026-10-13.md), [S2 probes](s2-anim-probe-evidence-2026-10-09.md), [M0 handoff](m0-handoff.md) and [graph-copy validation](scene-graph-copy-validation.md) retain their recorded results. Their former source paths and hotkeys are historical; removed product code remains available through Git history. Evidence archives and old packages were not deleted.
