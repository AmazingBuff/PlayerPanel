# SceneGraph previews

## Purpose

SceneGraph is the sole character-preview backend. It copies a source actor's existing scene graph and owns that preview's observation, animation, framing and drawing. No second Actor is created or controlled.

## Scope and non-goals

The preview shows standing weapon posture and existing equipment/appearance. It keeps the committed Equip → Start → Loop and phase-continuation behavior. This change adds no movement/attack mirroring, multi-actor UI, physics registration, FaceGen controller, shader/material work or safe graph destruction. The source Actor is a read-only input; native `NiObject::Clone` copies its graph.

## Architecture

`CharacterManager` registers source actors, collects per-actor equipment-event revisions and distributes `PreviewFrame` menu/lifetime inputs in the serialized render bracket. Each registered `SceneGraph` retains its source handle, monitor/controller, copied tree and placement state. Its `update` method decides when to observe, request animation, capture, retarget, poll resources and toggle drawing. Input only queues F7/F8 intent on that object.

`SceneGraph` implements graph copying, source/copy audits and retirement privately in `scene_graph_capture.cpp`. `SceneGraph::replace_graph` rebuilds placement/node data and retargets the same controller while the previous tree is still retained, then parks it. Graph replacement preserves animation clocks. The renderer calls manager update and the selected SceneGraph's draw method inside D3D state-capture scopes. It cannot switch to an Actor or puppet backend.

Animation remains a separate module: OAR returns copied query metadata; animation owns sequences, leases, clocks, variant history and pose composition. See [animation previews](animation.md).

## Code map

| Path | Entry points |
| --- | --- |
| [scene_graph.h](../src/character/scene_graph.h), [scene_graph.cpp](../src/character/scene_graph.cpp) | `SceneGraph`, `PreviewFrame`, `request`, `update`, `replace_graph`, `draw` |
| [character_manager.h](../src/character/character_manager.h), [character_manager.cpp](../src/character/character_manager.cpp) | `register_actors`, `get_scene_graph`, `EquipSink::ProcessEvent`, `update`, `clear_previews` |
| [scene_graph_capture.cpp](../src/character/scene_graph_capture.cpp) | private `SceneGraph::capture_graph`, `retire_graph`; source/copy audits |
| [equipment_state.h](../src/character/equipment_state.h), [equipment_state.cpp](../src/character/equipment_state.cpp) | read-only `equipment_weapon_form`, `equipment_hand_ids` |
| [figure_audit.h](../src/character/figure_audit.h), [figure_audit.cpp](../src/character/figure_audit.cpp) | collision-helper classification and appearance-settle signature |
| [snapshot_transform.h](../src/character/snapshot_transform.h) | `snapshot_delta`, `recompute_subtree_worlds` |
| [input.cpp](../src/input/input.cpp), [main.cpp](../src/main.cpp), [renderer.cpp](../src/render/renderer.cpp) | queued controls, lifetime/installation, update/draw bracket |
| [CMakeLists.txt](../CMakeLists.txt), [CMakePresets.json](../CMakePresets.json), [plugin.h.in](../cmake/plugin.h.in) | single backend and generated build identity |

## Interfaces and controls

- F7 queues recapture of the player's current graph while a paused inventory is open.
- F8 queues display toggle for that same SceneGraph.
- Menu state/revision comes only from `PanelMonitor`; equipment events only increment watched actor revisions.
- `clear_previews` queues release for the next serialized update. Expired source handles unregister; temporarily missing 3D suspends a session while retaining the source record.

There are no Actor spawning, dressing, inventory synchronization, body-morph copying or live-graph diagnostic controls. Removed diagnostic hotkeys have no active handlers.

## Invariants

Native capture, lease release, pose writes and graph replacement stay in the render/update bracket. Input callbacks never clone or animate. Animation requests, Random/variant history and source/display generations stay per source actor. A graph replacement retargets before its old nodes are retired. Source graph audits reject aliased writable state; copying never advances or writes the source actor's graph.

Rendering retains geometry filtering, attachment transforms, standing-pose calibration, material submission and D3D binding restoration. Copied graphs remain detached from engine menu roots. Retired copied trees deliberately remain parked: function-static bookkeeping holds raw pointers to retained native references, so destroying a SceneGraph cannot release them. Only bookkeeping is capped; there is no new native teardown mechanism.

## Failure modes

Unavailable source 3D suspends the session. Failed capture/audit retains the last accepted graph and retries through the existing settle/request flow. Unsupported or failed animation resources follow the animation module's diagnostics and stable-idle handling. Commands outside a usable paused inventory are ignored with an event log. Logs identify actor/request/capture and meaningful transitions; no frame/bone tracing is enabled.

## Dependencies

Existing C++23, VS2022/MSVC 14.44.35207, Windows SDK, CommonLibSSE, SKSE, OAR and the installed vcpkg dependencies remain required. No new dependency or ABI profile was added. Source actor handles and hand/form/clip reads remain necessary inputs. Source Actor creation or mutation is not a dependency.

## Verification

Both presets build exactly the same backend; `Release-scopy` is a compatibility build-directory name. Tests are available in both directories.

```text
cmake --preset Release
cmake --build build --config Release --target CharacterPanel CharacterPanelAnimationTest CharacterPanelCopyMathTest
cmake --preset Release-scopy
cmake --build build-scopy --config Release --target CharacterPanel CharacterPanelAnimationTest CharacterPanelCopyMathTest
ctest --test-dir build -C Release -R "^animation_controller$" --output-on-failure
```

[animation_controller_test.cpp](../tests/animation_controller_test.cpp) retains the two focused cases for Equip phase/descriptor sequences and actor request/continuation/mask isolation. [snapshot_transform_test.cpp](../tests/snapshot_transform_test.cpp) retains current geometry, track, rendering and query consumers; obsolete Actor/probe cases were retired. Its target is compiled, without expanding this change's behavioral test budget.

Local builds and structural checks do not verify game-time hooks, asynchronous loading, visual output, framing or native lifetime behavior. No game runtime is available here. The existing [package runbook](../tools/scene_copy/README.txt) describes manual follow-up.

## Safe modification

Keep registry/event distribution in CharacterManager and per-preview decisions in SceneGraph. Keep OAR query-only and avoid restoring Actor control as a fallback. Retain copy-isolation, source lifetime, latest-request and lease safeguards. Do not free retired native graphs until a separately approved teardown design exists. Rendering/material behavior is maintained outside this consolidation.

## Synchronized files

Synchronize the SceneGraph implementation (including private capture), manager and direct input/main/renderer consumers, [animation guidance](animation.md), [maintenance map](code-maintenance.md), [README](../README.md), [Changelog](../CHANGELOG.md), build presets/generated identity, [packaging script](../cmake/scene_copy_packaging.cmake) and [package instructions](../tools/scene_copy/README.txt).

The generated identity always starts with `CharacterPanel-scenegraph-` and includes the configured label, source/CommonLib revisions, dirty state and configure timestamp. `CharacterPanelSceneCopyPackage` is a compatibility target name for this sole backend; its manifest records `backend: SceneGraph`. Packaging does not deploy to the game.

## Related history

The animation ownership checkpoint is commit `a00745a`. Dated [OAR query](oar-preview-query-2026-10-16.md), [graph-copy validation](scene-graph-copy-validation.md), [AFP](afp-summary-2026-10-13.md) and [M0](m0-handoff.md) reports are historical evidence. Their deleted source paths and old hotkeys describe their recorded revisions, not current controls.
