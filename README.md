# CharacterPanel

C++23 SKSE inventory preview using a copied [SceneGraph](docs/scene-graph.md), with equipment-driven non-combat idles independent of source world actions. Native DAC temporarily owns bounded phase effects on the source Actor during inventory and restores the entry effects at close. No second Actor is created.

## Current status

The default product path uses the copied scene tree, OAR clip selection/sampling and native DAC preparation. [Animation](docs/animation.md) owns equipment policy, torch composition, accepted clocks and configuration; [SceneGraph](docs/scene-graph.md) owns capture, placement, attachments and drawing.

更新：2026-11-11。[最新运行摘要](docs/independent-behavior-graph.md#status-20261111-session)记录 E3 配对 2/2、E4 四块完整源姿态快照与装备链速率的有界证据。[C-G5-CAL v1](docs/independent-behavior-graph.md#c-g5-cal-v1)的[离线控制 A 已完成](docs/independent-behavior-graph.md#c-g5-cal-a-run1-20261111)：固定两组变换经共享采集/六位小数格式/解析，当前分析器三次往返为 12/12 行、120/120 分量。[K1/K2 资格账本](docs/independent-behavior-graph.md#c-g5-cal-k1k2)的 D1 判负（实际 1H，候选仍 `H2H_Equip`），D2 三项装备变量写入并保持、片段仍为 H2H，D2 判负；D3 v2 源侧初始值命中 `iRightHandType=1`、`iLeftHandType=9`，下一步 D3 候选 prime 写这两个分支字。输入/资源/控制资格、同代完整采样和 B3/B4 后处理边界尚缺，**K1–K6、原生控制 B 与时间校准仍 NOT_READY，数值容差未产生**。G1/G2 有界通过、G3-B 相位回放及 G4 A/B/C 保留；G5 未冻结/未通过，G6 后置，装载 teardown NO-GO。

G4 的历史裁定及限定见 [G4 收官](docs/independent-behavior-graph.md#g4-closure)；[G5 v2](docs/independent-behavior-graph.md#g5-draft)拥有完整验证目标，[CAL v1](docs/independent-behavior-graph.md#c-g5-cal-v1)拥有当前校准要求和缺项。E1–E4 的日期记录与原始日志保留各自边界，不能用旧“待运行”摘要覆盖后续证据。


候选与控制器源图共享行为 data 和绑定集，变量活值分储、从 authored defaults 初始化；共享资源与重复候选 pose 哈希均不能证明与源完整姿态、时钟或层混合一致。G2 的实际写骨 109/116、三条缺失名未知，以及同 controller 来源替换/frozen-only 等边界限制继续保留。

最近**记录部署为 v94**（[D3 v2 构建/部署](docs/build.md#v94-g5cal-d3-window-probe-20261111)），当前运行档为 D3 源侧只读探针 `4c7ad794…`（`IndependentGraph=off`、`SourceTimeline=on`、`SourceTransientWatch=true`；v93 + D3 三件套备份在 `dist/deploy-backup-20261111-v93d3-v94pre/`），D3 v2 已上机并命中；该档不是校准配置。下面列出仓库 dormant 样例的主要关闭键；完整状态见[当前部署与开关](docs/independent-behavior-graph.md#deploy-state)。

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

Use the [verification matrix](docs/independent-behavior-graph.md#verification-matrix), [manual checklist](docs/independent-behavior-graph.md#v32-manual-checklist) and [v42 pending observations](docs/independent-behavior-graph.md#v42-residual-corrections) for evidence limits. Historical contracts, native audits and retained logs remain linked from the canonical document and [history index](docs/history.md). Four redundant negative-run logs are retired from the worktree with full provenance and Git recovery in the [diagnostics index](docs/diagnostics/README.md).

The [local v44 display clock](docs/independent-behavior-graph.md#v44-display-clock) paces opt-in candidate playback at 60 fixed simulation steps per elapsed second of eligible display time. Each draw observes at most 100 ms and runs at most six steps; zero-due draws retain stepped-mode candidate output. First eligible calls and reset/fallback recovery establish a fresh origin. Two offline timing cases pass; game speed/visual validation remains pending.

The local [v46 retired-copy ledger](docs/scene-graph.md#retired-copy-accounting-v46) keeps every ordinary retired copy handle reachable in one intentionally surviving heap vector and logs its true process-total count. Twelve is a diagnostic warning threshold, with one warning at total 13. Native copies still remain allocated without a memory bound; reclamation and G6 convergence remain unproven. This differs from the experimental one-per-process candidate exception. The 20261030 rebuild of the same source was deployed for the first game pass, where `retained-total` advanced 1→14 and the threshold warning fired exactly once at 13.

[Audit27](docs/behavior-graph-construction-audit.md#audit27) proves three static external integer-setter sites for the equipped variables. It does not establish exclusive writers or a sufficient replay input set. The 2026-10-30 session then showed those variables read as graph outputs: writing two of them left the phase at `0/0/97`, while a single `WeapEquip` event moved `currentDefaultState` 10→1.

The [v46 first-game-pass procedure](docs/scene-graph.md#v46-manual-regression) **was executed on 2026-10-30** against a same-source rebuild (the 20261029 archive was absent, so the run belongs to identity `…-c0f2d81953-…-20261030T150024Z`). Both workflows are now observed rather than pending: independent source windows with fresh baselines, world-reset cancellation followed by an explicit re-arm, a clean 20.03-second window, 15 accepted captures against 14 PARK entries with the single threshold warning at 13, and normal F8/close-reopen behaviour. See the [results table](docs/scene-graph.md#v46-manual-regression).

The [DCMF source notes](docs/community-reference-supplement.md#dcmf-reference) are reference only, with comparison deferred; the v46-20261030 normal workflows are observed, while no DCMF integration or comparison has been executed.

One product-path defect surfaced during that pass and is recorded separately: the idle clip's start segment replays on every loop ([animation](docs/animation.md#idle-start-replay)). It is unrelated to the ledger work and has no contract yet.

## Build and controls

The plugin compiles only the 32 explicit `.cpp` units in [CMakeLists.txt](CMakeLists.txt), `PLUGIN_SOURCES`. Add each new intended unit to that sorted list and reconfigure; unlisted local `.cpp` files are excluded. The preserved untracked util files remain untouched; historical v42–v44 builds used the old source glob. [Build overview](docs/build.md) records the current inventory; the [v45 boundary record](docs/build.md#v45-source-list-2026-10-29) preserves its dated 30-unit artifact.

Prepare the pinned checkout/toolchain in [Build](docs/build.md#dependencies-and-checkout), then run from this repository:

```powershell
cmake --preset Release
cmake --build --preset Release
```

Release is the sole configure/build preset; CharacterPanel is the sole first-party target. Outputs are `build/Release/CharacterPanel.dll` and its matching PDB. Keep that pair with its configure-time `SCENEGRAPH BUILD` identity. The latest recorded deployed pair is [v92](docs/build.md#v92-g5e2c-ms-2026-11-10); earlier artifacts retain their own identities, hashes and build-time limits. A later commit does not relabel existing artifacts.

Preserve existing settings when updating [CharacterPanel.ini](config/CharacterPanel.ini). Settings load once at DataLoaded; restart after edits. Outside the panel, **F7** arms a 20-second source timeline when enabled; inside a paused inventory it queues recapture. In the v43 local code, re-arm resets baselines/settle counters; source loss/replacement or PreLoadGame/NewGame cancellation requires a later explicit F7. **F8** toggles display; **F9** remains the game quickload key. Compatible recapture retains normal playback clocks; the experimental measurement window and coverage refresh when the copied view changes.

Accepted panel mappings and paused-source WORLD snapshots are logged normally. `[Diagnostics] DetailedAnimationTrace=false` keeps verbose probes off; enable it and restart only for detailed investigation. Use the runtime-selected SKSE log directory and match the build identity before comparing results. [Maintenance](docs/code-maintenance.md#debugging-navigation) maps the log fields and evidence limits.

## Code and documentation navigation

| Area | Entry points |
| --- | --- |
| Default animation | [Animation](docs/animation.md), [controller.cpp](src/animation/controller.cpp): `AnimationController::request`, `retarget`, `advance`; [OAR](docs/oar-hooks.md), [DAC](docs/dac.md) |
| G5 calibration | [C-G5-CAL v1](docs/independent-behavior-graph.md#c-g5-cal-v1) and [offline A result](docs/independent-behavior-graph.md#c-g5-cal-a-run1-20261111): shared capture/format/parser passes three fixed-control round trips; [current raw/metrics](docs/diagnostics/g5-calibration-a-20261111/attempt-002/result.md). K1–K6, native B, time calibration and G5 remain NOT_READY |
| Experimental session | [v32](docs/independent-behavior-graph.md#g2-v32-display-session), [independent_graph.h](src/animation/behavior_graph/independent_graph.h): `DisplaySession`; [display_state_policy.h](src/animation/behavior_graph/display_state_policy.h): `DisplaySourceIdentity`, `DisplaySourceRejection`, `DisplayWindow`, `apply_named_display_pose` |
| Display pacing | [display_step_clock.h](src/animation/behavior_graph/display_step_clock.h): `DisplayStepClock`; [independent_graph.cpp](src/animation/behavior_graph/independent_graph.cpp): `DisplaySession::step_display_pose`; [two clock cases](tests/behavior_graph_display_clock_test.cpp), [commands/results](docs/build.md#v44-display-clock-2026-10-29) |
| Source timeline | [v43 invariants](docs/independent-behavior-graph.md#v43-source-sessions), [source_timeline.cpp](src/animation/behavior_graph/source_timeline.cpp): `sample_source_timeline`, `invalidate_source_timeline`; [policy](src/animation/behavior_graph/source_timeline_policy.h): `SourceTimelineWindow`, `SourceTimelineRequests`; [two regressions](tests/source_timeline_session_test.cpp), [commands/results](docs/build.md#v43-source-sessions-2026-10-29) |
| Motion and native step | [pose_motion_policy.h](src/animation/behavior_graph/pose_motion_policy.h): `compare_display_motion`; [drive_step_policy.h](src/animation/behavior_graph/drive_step_policy.h): `drive_graph_step`, `ScopedGraphWorld` |
| Focused offline regressions | [display session](tests/behavior_graph_display_session_test.cpp), [shared display step](tests/behavior_graph_display_step_test.cpp); current two-case results/commands in [v42 build](docs/build.md#v42-residual-corrections-2026-10-29); shared-step results remain historical |
| Scene capture and lifetime | [SceneGraph](docs/scene-graph.md), [scene_graph.cpp](src/character/scene_graph.cpp), [scene_graph_capture.cpp](src/character/scene_graph_capture.cpp) |
| Safe changes and evidence | [Maintenance](docs/code-maintenance.md), [native audit boundaries](docs/behavior-graph-construction-audit.md#audit-current), [Changelog](CHANGELOG.md), [history](docs/history.md) |

Offline tests/builds do not establish native timing, installed-asset visuals or safe retirement. Existing product symptom evidence and remaining equipment/attachment checks are recorded in the [2026-10-23 handoff](docs/diagnostics/display-issues-2026-10-22.md#10-2026-10-23-装备上下文与火把分层修复待上机).
