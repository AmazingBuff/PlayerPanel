# CharacterPanel

C++23 SKSE inventory preview using a copied [SceneGraph](docs/scene-graph.md), with equipment-driven non-combat idles independent of source world actions. Native DAC temporarily owns bounded phase effects on the source Actor during inventory and restores the entry effects at close. No second Actor is created.

## Current status

The default product path uses the copied scene tree, OAR clip selection/sampling and native DAC preparation. [Animation](docs/animation.md) owns equipment policy, torch composition, accepted clocks and configuration; [SceneGraph](docs/scene-graph.md) owns capture, placement, attachments and drawing.

The opt-in [independent behavior graph investigation](docs/independent-behavior-graph.md#current-status) has a bounded G2 pass: v33 measured candidate semantic motion 120/120, actual assignments 109/116, unchanged source controls and user-confirmed continuous display motion. Seven misses were counted, four names logged and three remain unknown; control/attachment labels do not prove harmless omissions. V34 verified reset at 61 steps without a summary, reopen, F7 retarget and post-load OAR fallback with no native rows. Its exact rejection cause, same-controller source/generation replacement and frozen-only rejection remain unverified.

G3-A succeeded in capturing a named settled phase trajectory for Equip→Idle→Unequip. It does not establish a complete external input trace, setter order or event timestamps. [G3-B remains a draft](docs/independent-behavior-graph.md#handoff-20261029), pending source/candidate runtime type mapping and a justified external input subset with event timing; `iState` and `currentDefaultState` are observations only. G4–G6 remain open. G6 includes source restoration, request/task invalidation, preview references/callbacks/sinks/history and observable repeated-session convergence. The deliberately retained one-per-process candidate is an explicit exception; loaded teardown remains NO-GO and global engine registry count has no valid measurement anchor.

The latest **recorded installation is v41**, DLL `3b8f8efe…`, with `SourceTimeline=true` and `IndependentGraph=off`, `DisplayPose=false`, `DisplayStep=false` ([build](docs/build.md#v41-panel-gate-2026-10-29)). The [local v42 correction](docs/build.md#v42-residual-corrections-2026-10-29) adds up to eight missing names and one bounded rejection reason; it has no new game results and is not deployed. The [local v43 recorder correction](docs/independent-behavior-graph.md#v43-source-sessions) gives each F7 arm a fresh 20-second/800-line allowance and discards replaced or lost sources and world-reset traces. V43 is also local only, with no game results. Repository defaults keep all four experiment keys off:

```ini
[Experiment]
IndependentGraph=off
DisplayPose=false
DisplayStep=false
SourceTimeline=false
```

Use the [verification matrix](docs/independent-behavior-graph.md#verification-matrix), [manual checklist](docs/independent-behavior-graph.md#v32-manual-checklist) and [v42 pending observations](docs/independent-behavior-graph.md#v42-residual-corrections) for evidence limits. Historical contracts, native audits and raw logs remain linked from the canonical document and [history index](docs/history.md).

The [local v44 display clock](docs/independent-behavior-graph.md#v44-display-clock) paces opt-in candidate playback at 60 fixed simulation steps per elapsed second of eligible display time. Each draw observes at most 100 ms and runs at most six steps; zero-due draws retain stepped-mode candidate output. First eligible calls and reset/fallback recovery establish a fresh origin. Two offline timing cases pass; game speed/visual validation remains pending and the recorded installation stays v41.

The local [v46 retired-copy ledger](docs/scene-graph.md#retired-copy-accounting-v46) keeps every ordinary retired copy handle reachable in one intentionally surviving heap vector and logs its true process-total count. Twelve is a diagnostic warning threshold, with one warning at total 13. Native copies still remain allocated without a memory bound; reclamation and G6 convergence remain unproven. This differs from the experimental one-per-process candidate exception. No deployment/game result is claimed.

[Audit27](docs/behavior-graph-construction-audit.md#audit27) now proves three static external integer-setter sites for the equipped variables. It does not establish exclusive writers, runtime variable types/mapping, execution in v41 or a sufficient replay input set. This documentation update leaves the latest local v46 binary and recorded v41 installation unchanged.

The next operator task is the [v46 first-game-pass procedure](docs/scene-graph.md#v46-manual-regression): two **PENDING** workflows covering fresh source windows/load cancellation and normal OAR axe/torch/recapture behavior. It keeps native candidate display off, backs up the matched files/settings, and returns the full log plus visual timing notes. The procedure has not been executed by this documentation update.

The [DCMF source notes](docs/community-reference-supplement.md#dcmf-reference) are reference only, with comparison deferred until the unchanged v46 workflows are completed and their returned game evidence reviewed. Both workflows remain PENDING; no DCMF integration or comparison is underway.

## Build and controls

The plugin compiles only the 30 explicit `.cpp` units in [CMakeLists.txt](CMakeLists.txt), `PLUGIN_SOURCES`. Add each new intended unit to that sorted list and reconfigure; unlisted local `.cpp` files are excluded. The preserved untracked util files remain untouched; historical v42–v44 builds used the old source glob. [Build boundary and verification](docs/build.md#v45-source-list-2026-10-29) record the current inventory and local-only artifact.

Prepare the pinned checkout/toolchain in [Build](docs/build.md#dependencies-and-checkout), then run from this repository:

```powershell
cmake --preset Release
cmake --build --preset Release
```

Release is the sole configure/build preset; CharacterPanel is the sole first-party target. Outputs are `build/Release/CharacterPanel.dll` and its matching PDB. Keep that pair with its configure-time `SCENEGRAPH BUILD` identity. The latest recorded installed pair is [v41](docs/build.md#v41-panel-gate-2026-10-29); the local [v42 build](docs/build.md#v42-residual-corrections-2026-10-29), [v43 build](docs/build.md#v43-source-sessions-2026-10-29), [v44 build](docs/build.md#v44-display-clock-2026-10-29), [v45 build](docs/build.md#v45-source-list-2026-10-29) and current [v46 build](docs/build.md#v46-retained-copy-ledger-2026-10-29) each have their own identity and hashes. A later commit does not relabel existing artifacts.

Preserve existing settings when updating [CharacterPanel.ini](config/CharacterPanel.ini). Settings load once at DataLoaded; restart after edits. Outside the panel, **F7** arms a 20-second source timeline when enabled; inside a paused inventory it queues recapture. In the v43 local code, re-arm resets baselines/settle counters; source loss/replacement or PreLoadGame/NewGame cancellation requires a later explicit F7. **F8** toggles display; **F9** remains the game quickload key. Compatible recapture retains normal playback clocks; the experimental measurement window and coverage refresh when the copied view changes.

Accepted panel mappings and paused-source WORLD snapshots are logged normally. `[Diagnostics] DetailedAnimationTrace=false` keeps verbose probes off; enable it and restart only for detailed investigation. Use the runtime-selected SKSE log directory and match the build identity before comparing results. [Maintenance](docs/code-maintenance.md#debugging-navigation) maps the log fields and evidence limits.

## Code and documentation navigation

| Area | Entry points |
| --- | --- |
| Default animation | [Animation](docs/animation.md), [controller.cpp](src/animation/controller.cpp): `AnimationController::request`, `retarget`, `advance`; [OAR](docs/oar-hooks.md), [DAC](docs/dac.md) |
| Experimental session | [v32](docs/independent-behavior-graph.md#g2-v32-display-session), [independent_graph.h](src/animation/behavior_graph/independent_graph.h): `DisplaySession`; [display_state_policy.h](src/animation/behavior_graph/display_state_policy.h): `DisplaySourceIdentity`, `DisplaySourceRejection`, `DisplayWindow`, `apply_named_display_pose` |
| Display pacing | [display_step_clock.h](src/animation/behavior_graph/display_step_clock.h): `DisplayStepClock`; [independent_graph.cpp](src/animation/behavior_graph/independent_graph.cpp): `DisplaySession::step_display_pose`; [two clock cases](tests/behavior_graph_display_clock_test.cpp), [commands/results](docs/build.md#v44-display-clock-2026-10-29) |
| Source timeline | [v43 invariants](docs/independent-behavior-graph.md#v43-source-sessions), [source_timeline.cpp](src/animation/behavior_graph/source_timeline.cpp): `sample_source_timeline`, `invalidate_source_timeline`; [policy](src/animation/behavior_graph/source_timeline_policy.h): `SourceTimelineWindow`, `SourceTimelineRequests`; [two regressions](tests/source_timeline_session_test.cpp), [commands/results](docs/build.md#v43-source-sessions-2026-10-29) |
| Motion and native step | [pose_motion_policy.h](src/animation/behavior_graph/pose_motion_policy.h): `compare_display_motion`; [drive_step_policy.h](src/animation/behavior_graph/drive_step_policy.h): `drive_graph_step`, `ScopedGraphWorld` |
| Focused offline regressions | [display session](tests/behavior_graph_display_session_test.cpp), [shared display step](tests/behavior_graph_display_step_test.cpp); current two-case results/commands in [v42 build](docs/build.md#v42-residual-corrections-2026-10-29); shared-step results remain historical |
| Scene capture and lifetime | [SceneGraph](docs/scene-graph.md), [scene_graph.cpp](src/character/scene_graph.cpp), [scene_graph_capture.cpp](src/character/scene_graph_capture.cpp) |
| Safe changes and evidence | [Maintenance](docs/code-maintenance.md), [native audit boundaries](docs/behavior-graph-construction-audit.md#audit-current), [Changelog](CHANGELOG.md), [history](docs/history.md) |

Offline tests/builds do not establish native timing, installed-asset visuals or safe retirement. Existing product symptom evidence and remaining equipment/attachment checks are recorded in the [2026-10-23 handoff](docs/diagnostics/display-issues-2026-10-22.md#10-2026-10-23-装备上下文与火把分层修复待上机).
