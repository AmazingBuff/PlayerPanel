# SceneGraph previews

## Purpose

SceneGraph is the sole character-preview backend. It copies a source actor's existing scene graph and owns that preview's observation, animation, framing and drawing. No second Actor is created or controlled.

## Scope and non-goals

The preview shows standing weapon posture and existing equipment/appearance. Animation targets equipped non-combat idles independently of world action, after observed native Smooth goal preparation. Empty hands use the configured loop/direct-base choice. This change adds no movement/attack mirroring, multi-actor UI, physics registration, FaceGen controller, shader/material work or safe graph destruction. The source graph/transforms stay read-only; native `NiObject::Clone` copies its graph. [dac::Session](dac.md) temporarily borrows only controlled original-Actor marker effects and restores them at the UI close boundary.

## Architecture

`CharacterManager` registers source actors, collects per-actor equipment-event revisions and distributes `PreviewFrame` menu/lifetime inputs in the serialized render bracket. Each registered `SceneGraph` retains its source handle, monitor/controller, copied tree and placement state. Its `update` method decides when to observe, request animation, capture, retarget, poll resources and toggle drawing. Input only queues F7/F8 intent on that object.

`SceneGraph` implements graph copying, source/copy audits and retirement privately in `scene_graph_capture.cpp`. `SceneGraph::replace_graph` rebuilds placement/node data and retargets the same controller while the previous tree is still retained, then parks it. Graph replacement preserves animation clocks. The renderer calls manager update and the selected SceneGraph's draw method inside D3D state-capture scopes. It cannot switch to an Actor or puppet backend.

Animation remains a separate module: OAR returns copied query metadata; animation owns annotation clocks, leases, accepted variant history and pose composition. SceneGraph separately owns a native DAC session, whose inventory entry snapshot survives appearance/equipment/source-graph retarget. See [animation previews](animation.md).

```mermaid
flowchart LR
    Menu[Inventory menu] --> Panel[PanelMonitor]
    Panel --> Manager[CharacterManager]
    Equip[Equipment events] --> Manager
    Manager -->|frame and revision| Graph[SceneGraph per actor]
    Input[F7 / F8 intent] --> Graph
    Graph -->|request / retarget / poll / advance| Animation[AnimationController]
    Animation -->|read-only query| OAR[OAR metadata and conditions]
    Animation -->|compose and apply pose| Tree[Owned copied tree]
    Graph -->|private capture / audits| Tree
    Tree --> Draw[Studio draw and composite]
```


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

`SceneGraph::observe_source` also records left torch presence independently of the body family for the [one-handed body/arm composition](animation.md#equipment-context-and-torch-composition-2026-10-23). Retarget rebinds the owned arm layer alongside body/standing/calibration for pending, active and outgoing proposals. Source equipment/transform/framing policy does not change.

Outside paused inventory, `SceneGraph::update_monitor` also asks `AnimationController::prepare_base_indices` to prepare the [finite base-action cache](animation.md#fixed-base-indices-and-project-readiness) through the existing third-person graph resolver. This observes metadata only: no clip/graph advancement, condition/Random/history evaluation, DAC/IED call or source mutation. Preparation waits for published graph/behavior/skeleton and covering binding data, then scans the engine names once while copying only matched finite originals. LoadingMenu updates return before observing old source graphs. Existing `CharacterManager::release_previews` clears project metadata at world boundaries; table identity/count changes also rebuild. A late-ready project may initialize on the menu path with an explicit diagnostic. Capture/Clone and first-use resource costs remain separate and unchanged.

## Invariants

Native capture, lease release, pose writes and graph replacement stay in the render/update bracket. Input callbacks never clone or animate. Animation requests, Random/variant history and source/display generations stay per source actor. A graph replacement retargets before its old nodes are retired. Source graph audits reject aliased writable state; copying never advances or writes the source actor's graph.

Rendering retains geometry filtering, standing-pose calibration, material submission and D3D binding restoration. Copied graphs remain detached from engine menu roots. Retired copied trees deliberately remain parked: function-static bookkeeping holds raw pointers to retained native references, so destroying a SceneGraph cannot release them. The twelve-entry cap limits bookkeeping only; retained native allocations are not bounded by it and may accumulate across captures and preview lifetimes. There is no safe native teardown mechanism here.

Placement calibration normally uses the vanilla MT_Idle standard: `AnimationController::apply_standing_pose` samples the independent base calibration asset at time zero, and `SceneGraph::draw` calls `calibrate_from_posed_bound` before writing the current action on the same frame. Calibration waits while that asset is pending. If its lease fails, the standing underlay is also the vanilla base; action poses never supply calibration. [Event timing and dynamic selection](animation.md#event-timing-and-native-state) describes the separate animation policy.

## Held equipment invariant

After source/copy correspondence and isolation pass, [hold_copy_equipment](../src/character/held_equipment.cpp) maps actual equipped biped `item`/`partClone` identities to owned copy objects. A weapon's `TESObjectWEAP::GetNodeName` provides the exact root identity when needed; shield lookup uses `GetShieldObject`. Missing or ambiguous roots log actor/form/side rather than selecting an arbitrary model. Inventory-only weapons and unrelated quivers stay untouched.

Mapped equipment retains a `NiPointer` across detach/attach and preserves its model-local rotation, translation and scale. Right-hand weapons/staves mount at copied WEAPON, left-hand items and shields at SHIELD; bows/crossbows explicitly use SHIELD. Two-handed weapons are handled once. During an equip transition the mapped model is parked on its captured sheath parent until the clip's `weaponDraw` annotation, then reattached to the hand; unequip reverses while the copy still holds the model. The copied model/mount becomes visible; copied Scb/ScbLeft geometry is hidden without unhiding unrelated child effects/LOD/blood objects. Generic HKX Weapon/Shield/Quiver tracks map independently to WEAPON/SHIELD/QUIVER; sheath WeaponSword/MOV chains are never held targets.

No source node, equipment/draw flag or Actor graph is mutated. The helper receives capture-local ownership correspondence; native graph retirement and source/isolation audits remain intact. This source-backed parenting design is not an in-game visibility result: identical-form dual roots, custom skeletons, bow/crossbow/scabbard visibility and paused presentation still require game verification.

## Opt-in capture, placement and mount evidence

`DetailedAnimationTrace=true` adds observations around existing decisions, with actor/latest requested request/accepted request/capture ID and graph pointers. `SCOPY CAPTURE-DUE` records the already-computed pending/settled/manual flags, current revision and request reason; it does not remeasure assembly. `SCOPY CAPTURE` records actual acceptance/rejection and old/new graph, and `SCOPY GRAPH` identifies the accepted graph's calibration request. These are capture events, not per-poll waiting messages. A newer requested generation can coexist with an older visible accepted clip.

At most once per request, `SCOPY SOURCE-SNAPSHOT/SOURCE-NODE` records the shared fixed fifteen body/arm names from [Trace_Body_Nodes](../src/animation/diagnostic_trace.h). Matching occurs during the **already-required** source local/world/bound copy pass, and the log uses those saved local/world transforms before Clone. It adds no full source traversal. The header says `paused-source phase=before-clone`; this is not a settled world-animation reference or an atomic all-bone measurement. Node/parent names and pointers are actual values; absent requested nodes say missing, unnamed objects say `<unnamed>`, and absent parents say `<none>`. Local/world translation units are game units, rotation is x/y/z/w quaternion, scale is a ratio.

`SCOPY PLACEMENT` records raw placement once per accepted capture before standing calibration. `SCOPY CALIBRATION` reuses the existing measured anchor/offset/radius and applied roots, or identifies the unmeasurable/raw-kept outcome. There is no additional bound measurement, root solve or camera change. `SCOPY MOUNT-MAP` reports successful existing helper choices: form/side, mapped source/copy model, prior/target parent and retained local transform. Immutable `HeldDiagnosticIds` carry request/accepted-request/capture correlation. `SCOPY MOUNT-SWITCH` records only an actual parent change made by the existing draw/sheathe policy, preserving the same local transform and operation order.

Join graph pointers/capture IDs to `POSE PROBE/SAMPLE` and [ANIMATION HANDOFF](animation.md#correlating-reported-issues), whose source is an observed weaponDraw/weaponSheathe annotation or the unchanged duration midpoint. The common [request-budgeted probe](animation.md#bounded-axe-pose-and-projection-evidence-2026-10-22) uses post-composition copy transforms/projection; the source snapshot and copy can represent different phases and coordinate contexts. No layer weight/mask or source-track bone semantic is inferred. Missing names, budget exhaustion or a missed transient must remain inconclusive.

Default-off tracing skips source-name matching and all new formatting/projection/successful mount detail. Capture isolation/source audits and their handling remain unconditional, as do actual mounting, calibration, animation dispatch and normal accepted mappings. Native parent operations, render timing and all reported visual causes remain **UNVERIFIED**; see the [current symptom procedure](diagnostics/display-issues-2026-10-22.md#10-2026-10-23-装备上下文与火把分层修复待上机).

## Failure modes

Unavailable source 3D suspends the session. Failed capture/audit retains the last accepted graph and retries through the existing settle/request flow. Unsupported or failed animation resources follow the animation module's diagnostics and stable-idle handling. Commands outside a usable paused inventory are ignored with an event log. Default logs retain accepted mappings, event-driven paused-source snapshots and required audits; issue traces, capture geometry lists and common pose probes require `[Diagnostics] DetailedAnimationTrace=true` and a restart. Pose evidence is limited to four windows/twenty-four snapshots per request, each window expiring after 4.1 seconds with the existing twelve slots. Ordinary info flushes periodically about once per second; warnings/errors flush immediately.

## Dependencies

Existing C++23, VS2022/MSVC 14.44.35207, Windows SDK, CommonLibSSE, SKSE, OAR and the installed vcpkg dependencies remain required. The existing gated OAR metadata getters remain. Native phase processing uses the supported original DAC binary and UI-session transaction described in [DAC compatibility](dac.md#original-binary-compatibility-and-provenance). No DAC rebuild or tomlplusplus dependency remains. Source actor handles and hand/form reads remain necessary inputs. No source Actor is created; only the documented DAC marker transaction may temporarily mutate its effects.

## Verification

[Build](build.md) documents the single Release target, pinned setup, generated outputs and configure-time identity. Prior native evidence and its limits are recorded in [Build verification](build.md#verification-and-limits). The two animation cases compile directly as documented there; no CMake test target is added.

Build/source checks establish compilation and structural consistency, not game-time hook, loader, framing, visual or lifetime behavior. [Maintenance](code-maintenance.md#debugging-navigation) maps actual logs and the two existing representative runtime follow-up categories. No Skyrim runtime is available here.

## Safe modification

Keep registry/event distribution in CharacterManager and per-preview decisions in SceneGraph. Keep OAR query-only and avoid restoring Actor control as a fallback. Retain copy-isolation, source lifetime, latest-request and lease safeguards. Do not free retired native graphs until a separately approved teardown design exists. Rendering/material behavior is maintained outside this consolidation.

## Synchronized files

Synchronize the SceneGraph implementation (including private capture), manager and direct input/main/renderer consumers, [animation guidance](animation.md), [maintenance map](code-maintenance.md), [README](../README.md), [Changelog](../CHANGELOG.md), [build guidance](build.md), [CMake presets](../CMakePresets.json), generated identity, [shader embedding](../cmake/embed_shaders.cmake), [version resource](../cmake/version.rc.in) and [build/runtime notes](../tools/scene_copy/README.txt).

The generated identity always starts with `CharacterPanel-scenegraph-` and includes the configured label, source/CommonLib revisions, dirty state and configure timestamp. The DLL target/output name remains CharacterPanel / CharacterPanel.dll. No package or install target is generated.

## Related history

The animation ownership checkpoint is commit `a00745a`. Historical summaries cover [OAR queries](history-animation.md#oar), [graph-copy validation](history-rendering.md#scene-copy), [AFP](history-animation.md#final-output) and [M0](history-rendering.md#m0). The superseded reports/logs are recoverable through [the source index](history-sources.md); their former code paths and hotkeys describe recorded revisions, not current controls.
