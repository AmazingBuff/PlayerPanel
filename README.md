# CharacterPanel

C++23 SKSE inventory preview using a copied [SceneGraph](docs/scene-graph.md), with equipment-driven non-combat idles independent of source world actions. Native DAC temporarily owns bounded phase effects on the source Actor during inventory and restores the entry effects at close. No second Actor is created.

## Current status

The default product path uses the copied scene tree, OAR clip selection/sampling and native DAC preparation. [Animation](docs/animation.md) owns equipment policy, torch composition, accepted clocks and configuration; [SceneGraph](docs/scene-graph.md) owns capture, placement, attachments and drawing.

The opt-in [independent behavior graph investigation](docs/independent-behavior-graph.md#current-status) is at v33. G1 passes only the revised Option A criterion. **G2 now passes within a bounded scope**: one uninterrupted 120-call display window measured 120/120 sign-invariant candidate motion, 109/116 actual node assignments (`109+7+0=116` self-consistent, the listed misses being non-bone control/attachment nodes), unchanged source pose and continuous on-screen motion ([v33 run](docs/independent-behavior-graph.md#g2-v33-run)). Reopen without splicing, F7 discarding a partial window and a load refusing the stale candidate are also verified ([v34](docs/independent-behavior-graph.md#g2-v34-boundaries)); closing before 120 calls is not. Loaded teardown, global convergence, G3–G6 and independent OAR parity remain unverified.

The committed code was rebuilt under the `v33` label because no v31/v32 artifact survived the checkout; that pair ([Build](docs/build.md#v33-rebuild-2026-10-28)) is deployed, has run once, and the three experiment keys are back to off.

All experiment keys default off:

```ini
[Experiment]
IndependentGraph=off
DisplayPose=false
DisplayStep=false
```

Use the [verification matrix](docs/independent-behavior-graph.md#verification-matrix) for measured results and the [v32 manual checklist](docs/independent-behavior-graph.md#v32-manual-checklist) for the next game session; steps 1–5 have run once, steps 6–7 (boundaries) have not. Candidate motion, applied coverage and visual motion stay separate evidence. Historical contracts, native audits and raw logs remain linked from the canonical document and [history index](docs/history.md).

## Build and controls

Prepare the pinned checkout/toolchain in [Build](docs/build.md#dependencies-and-checkout), then run from this repository:

```powershell
cmake --preset Release
cmake --build --preset Release
```

Release is the sole configure/build preset; CharacterPanel is the sole first-party target. Outputs are `build/Release/CharacterPanel.dll` and its matching PDB. Keep that pair with its `SCENEGRAPH BUILD` identity. Identity is captured at configure time; committing documentation does not relabel an existing DLL. The current deployable pair is a rebuild of the committed v32 code under a new label, recorded in the [v33 build record](docs/build.md#v33-rebuild-2026-10-28); the earlier [v32 pre-commit binary record](docs/build.md#v32-display-session-2026-10-28) no longer has an artifact in the checkout.

Preserve existing settings when updating [CharacterPanel.ini](config/CharacterPanel.ini). Settings load once at DataLoaded; restart after edits. In a paused inventory, **F7** queues recapture and **F8** toggles display. Compatible recapture retains normal playback clocks; the experimental measurement window and coverage refresh when the copied view changes.

Accepted panel mappings and paused-source WORLD snapshots are logged normally. `[Diagnostics] DetailedAnimationTrace=false` keeps verbose probes off; enable it and restart only for detailed investigation. Use the runtime-selected SKSE log directory and match the build identity before comparing results. [Maintenance](docs/code-maintenance.md#debugging-navigation) maps the log fields and evidence limits.

## Code and documentation navigation

| Area | Entry points |
| --- | --- |
| Default animation | [Animation](docs/animation.md), [controller.cpp](src/animation/controller.cpp): `AnimationController::request`, `retarget`, `advance`; [OAR](docs/oar-hooks.md), [DAC](docs/dac.md) |
| Experimental session | [v32](docs/independent-behavior-graph.md#g2-v32-display-session), [independent_graph.h](src/animation/behavior_graph/independent_graph.h): `DisplaySession`; [display_state_policy.h](src/animation/behavior_graph/display_state_policy.h): `DisplaySourceIdentity`, `DisplayWindow`, `apply_named_display_pose` |
| Motion and native step | [pose_motion_policy.h](src/animation/behavior_graph/pose_motion_policy.h): `compare_display_motion`; [drive_step_policy.h](src/animation/behavior_graph/drive_step_policy.h): `drive_graph_step`, `ScopedGraphWorld` |
| Focused offline regressions | [display session](tests/behavior_graph_display_session_test.cpp), [shared display step](tests/behavior_graph_display_step_test.cpp); prior results/commands in [v32 build](docs/build.md#v32-display-session-2026-10-28) and [v31 build](docs/build.md#v31-display-step-2026-10-28) |
| Scene capture and lifetime | [SceneGraph](docs/scene-graph.md), [scene_graph.cpp](src/character/scene_graph.cpp), [scene_graph_capture.cpp](src/character/scene_graph_capture.cpp) |
| Safe changes and evidence | [Maintenance](docs/code-maintenance.md), [native audit boundaries](docs/behavior-graph-construction-audit.md#audit-current), [Changelog](CHANGELOG.md), [history](docs/history.md) |

Offline tests/builds do not establish native timing, installed-asset visuals or safe retirement. Existing product symptom evidence and remaining equipment/attachment checks are recorded in the [2026-10-23 handoff](docs/diagnostics/display-issues-2026-10-22.md#10-2026-10-23-装备上下文与火把分层修复待上机).
