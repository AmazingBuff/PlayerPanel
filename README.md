# CharacterPanel

C++23 SKSE inventory preview using a copied [SceneGraph](docs/scene-graph.md), with equipment-driven non-combat idles independent of source world actions. Native DAC temporarily owns bounded phase effects on the source Actor during inventory and restores the entry effects at close. No second Actor is created.

## Current status

The default product path uses the copied scene tree, OAR clip selection/sampling and native DAC preparation. [Animation](docs/animation.md) owns equipment policy, torch composition, accepted clocks and configuration; [SceneGraph](docs/scene-graph.md) owns capture, placement, attachments and drawing.

The opt-in [independent behavior graph investigation](docs/independent-behavior-graph.md#current-status) has a bounded G2 pass: v33 measured candidate semantic motion 120/120, actual assignments 109/116, unchanged source controls and user-confirmed continuous display motion. Seven misses were counted, four names logged and three remain unknown; control/attachment labels do not prove harmless omissions. V34 verified reset at 61 steps without a summary, reopen, F7 retarget and post-load OAR fallback with no native rows. Its exact rejection cause, same-controller source/generation replacement and frozen-only rejection remain unverified.

G3-A succeeded in capturing a named settled phase trajectory for Equip→Idle→Unequip. The [2026-10-30/31 session](docs/independent-behavior-graph.md#session-20261030) then settled G3-B's first prerequisite: the candidate and the controller's source graph **share one `hkbBehaviorGraphData`** (652 names/types/initial values, alignment 652/0/0), so no by-name type mapping is needed; the earlier "source 463 / candidate 652" framing compared two different graphs (the player's own graph versus the controller's). The same session showed the candidate has **no value set before activation**, is initialised from authored defaults rather than the source's live state, and that **phase transitions are driven by events, not variables**: `WeapEquip` alone moves `currentDefaultState` 10→1 with a node-stack change and 119/120 motion, while `iRightHandEquipped=1` and `IsEquipping=1` — both written and read back successfully — change nothing. `iState=9` stays 0 and most likely needs a real equipped item. The [C-G3B-S3 unequip slice](docs/independent-behavior-graph.md#c-g3b-s3-unequip-event) then ran once on machine (v53, 2026-10-31): the settle gate passed on a v52-identical equip window, `1HM_Unequip` resolved (id=15, `phase=events count=8223` — adjudicating the census's "double zero" as its 8192-cap artifact) and dispatched cleanly, yet the state machine never left the equipped idle — all 121 rows `default=1 nodes=111`, no unequip pulses — so the slice is a **negative result** scoped to that event at that timing; per the anti-fishing rule no second name was tried and the next candidate needs fresh evidence. The [C-G3B-E1 source event-stream capture](docs/independent-behavior-graph.md#c-g3b-e1-event-capture) then ran on machine (v54) and returned a decisive negative: the phase trajectory reproduced v41 exactly, but **zero events** were dispatched through 58377 across both transitions — the real equip/unequip flow drives these phases by writing graph variables directly, not by events. The v41 timing also shows the engine's equipped-state setter writes land 0.85 s **after** the unequip transition starts, so they are not its trigger; the [C-G3B-S4 slice](docs/independent-behavior-graph.md#c-g3b-s4-unequip-variable) therefore walks a pre-registered ladder of the three same-moment unequip-pulse variables (`bEquipUnequip` → `IsUnequipping` → `PitchManualOverride`), one INI-only round per name, writing each from the settled equipped idle. Whether `iState` is a required criterion stays undecided; [G3-B](docs/independent-behavior-graph.md#handoff-20261029) remains a draft on its writer/input-subset/timing clauses. G4–G6 remain open. G6 includes source restoration, request/task invalidation, preview references/callbacks/sinks/history and observable repeated-session convergence. The deliberately retained one-per-process candidate is an explicit exception; loaded teardown remains NO-GO and global engine registry count has no valid measurement anchor.

The latest **recorded installation is v57**, DLL `250abfb1…` (clean identity `…-227d5104a3-…`), with `IndependentGraph=inject`, `CandidateUnequipProbe=true`, `CandidateUnequipPrime=iRightHandEquipped`, `CandidateUnequipVariable=bEquipUnequip` and every other experiment key off ([v57 build](docs/build.md#v57-two-write-2026-10-31)): the S5 two-write replay writes the equipped state then the unequip command at the settle point. The E2 round's v56 DLL (`868e6c35…`) is backed up under `dist/deploy-backup-20261031-v56/`. E2's F7 window caught both real transitions with both player graphs armed and recorded **zero** typed-setter calls — the engine's equipment writes bypass `SetGraphVariableBool/Int` entirely (likely audit27's manager-level path), retiring that route; merged with E1 and S4, the unequip trigger is neither an event, nor a typed-setter call, nor any persistent write. The surviving hypothesis is the equipped-state divergence (`rh=1` in the source's equipped idle versus `rh=0` in the candidate's), which the proposed S5 two-write replay tests. The v55 (`119171e2…`), v54 (`c7e31b78…`) and v53 (`5f4559e1…`) pairs are backed up under `dist/deploy-backup-20261031-v55/`, `-v54/` and `-v53/`. The v54 (`c7e31b78…`) and v53 (`5f4559e1…`) pairs are backed up under `dist/deploy-backup-20261031-v54/` and `-v53/`. The [v46 first game pass](docs/scene-graph.md#v46-manual-regression) was executed on 2026-10-30 against a rebuild of the same source (the 20261029 archive was missing on that machine): source windows, load cancellation, 15 accepted captures with 14 PARK entries, the one-shot threshold warning at 13, F8 and close/reopen all behaved as designed. Repository defaults keep every experiment key off:

```ini
[Experiment]
IndependentGraph=off
DisplayPose=false
DisplayStep=false
SourceTimeline=false
SourceEventCapture=false
CandidateVariableCensus=false
CandidateInputProbe=false
CandidateInputVariable=IsEquipping
CandidateInputValue=1
CandidateGenerationEntry=false
CandidateUnequipProbe=false
```

Use the [verification matrix](docs/independent-behavior-graph.md#verification-matrix), [manual checklist](docs/independent-behavior-graph.md#v32-manual-checklist) and [v42 pending observations](docs/independent-behavior-graph.md#v42-residual-corrections) for evidence limits. Historical contracts, native audits and raw logs remain linked from the canonical document and [history index](docs/history.md).

The [local v44 display clock](docs/independent-behavior-graph.md#v44-display-clock) paces opt-in candidate playback at 60 fixed simulation steps per elapsed second of eligible display time. Each draw observes at most 100 ms and runs at most six steps; zero-due draws retain stepped-mode candidate output. First eligible calls and reset/fallback recovery establish a fresh origin. Two offline timing cases pass; game speed/visual validation remains pending.

The local [v46 retired-copy ledger](docs/scene-graph.md#retired-copy-accounting-v46) keeps every ordinary retired copy handle reachable in one intentionally surviving heap vector and logs its true process-total count. Twelve is a diagnostic warning threshold, with one warning at total 13. Native copies still remain allocated without a memory bound; reclamation and G6 convergence remain unproven. This differs from the experimental one-per-process candidate exception. The 20261030 rebuild of the same source was deployed for the first game pass, where `retained-total` advanced 1→14 and the threshold warning fired exactly once at 13.

[Audit27](docs/behavior-graph-construction-audit.md#audit27) proves three static external integer-setter sites for the equipped variables. It does not establish exclusive writers or a sufficient replay input set. The 2026-10-30 session then showed those variables read as graph outputs: writing two of them left the phase at `0/0/97`, while a single `WeapEquip` event moved `currentDefaultState` 10→1.

The [v46 first-game-pass procedure](docs/scene-graph.md#v46-manual-regression) **was executed on 2026-10-30** against a same-source rebuild (the 20261029 archive was absent, so the run belongs to identity `…-c0f2d81953-…-20261030T150024Z`). Both workflows are now observed rather than pending: independent source windows with fresh baselines, world-reset cancellation followed by an explicit re-arm, a clean 20.03-second window, 15 accepted captures against 14 PARK entries with the single threshold warning at 13, and normal F8/close-reopen behaviour. See the [results table](docs/scene-graph.md#v46-manual-regression).

The [DCMF source notes](docs/community-reference-supplement.md#dcmf-reference) are reference only, with comparison deferred until the returned v46 game evidence is reviewed; no DCMF integration or comparison is underway.

One product-path defect surfaced during that pass and is recorded separately: the idle clip's start segment replays on every loop ([animation](docs/animation.md#idle-start-replay)). It is unrelated to the ledger work and has no contract yet.

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
