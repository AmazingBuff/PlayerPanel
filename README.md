# CharacterPanel

C++23 SKSE inventory preview using a copied [SceneGraph](docs/scene-graph.md), with equipment-driven non-combat idles independent of source world actions. Native DAC temporarily owns bounded phase effects on the source Actor during inventory and restores the entry effects at close. No second Actor is created.

## Current status

> **2026-11-17 最新状态**：门 5 两侧正式采集已完成并归档（候选 249/249、源 849/849，K5-1..8 两侧 PASS）。正式 G4P 批次 248 对；G4P-1/2/3/4/5/8/9 PASS，G4P-6 PASS（锚点 `1hm_equip` 97 vs 96，完全由 IDUN 2→1 解释），**G4P-7 FAIL**：`E_p/E_r/E_s` 分别 3,826 / 16,755 / 352 次超冻结 tau，P99 `5.5912 / 0.0457227 / 0.000525653`。失败集中在 `Weapon`、`WeaponDagger`、`Quiver`、`WeaponBow`、`WeaponAxe`、`WeaponSword`、`WeaponBack`、`WeaponMace`、`Pauldron`、`NPC COM`；[事后归因审计](docs/diagnostics/g5-attribution-20261117/README.md) 显示 5 个静态跨侧偏移附件骨贡献 1,240/3,826，body 88 轨仍贡献 1,226，A17 为 0。[C-G5-SETUP v1](docs/diagnostics/g5-setup-audit-20261117/README.md) 已完成：两侧 `animationSkeleton`/`data`/`referencePose` 同源，**H1（setup/bind 不同）证伪；H2（源真实附件状态）得到支持**（静态 5 骨 source−ref 2.77–4.20，候选−ref ≤1.62e-4）。[C-G5-CLOCK v2](docs/diagnostics/g5-clock-step-20261117/README.md) 显示候选 equip/unequip/idle 只有 17/21/3 个唯一 local_time，正式配对只用 12/16/3 个源 seq；同一 local_time 的多记录组全部是不同完整姿态（8/8、10/10、2/2 全唯一）。[C-G5-STATE v1](docs/diagnostics/g5-state-20261117/README.md) 完成：同组姿态 PC1 0.9983–0.999998 且单调，是**单标量混合曲线**；equip 第一组最近源轨迹残差 max 0.497（离轨初始状态），后续 equip ≤0.0856，unequip 后段/idle ≤1.5e-5/3.3e-5。[C-G5-STATE v2 bootstrap](docs/diagnostics/g5-state-bootstrap-20261117/README.md) 为**负结果**：复制 116/116 源 poseLocal 后，首帧 Generate 覆盖全部姿态，v106 与门 5 的 288,840/288,840 分量完全相同，SB-5 FAIL。[C-G5-STATE v3](docs/diagnostics/g5-state-probe-20261117/README.md) 也未找到：`hkbNodeInfo` 2,729 行、blender 对象 0x100 窗口 12 行均无逐 step 标量变化（SP-5 未找到）。**G5 未通过，G6 后置**；下一版须探测 `hkbBlenderGeneratorInternalState`/child 权重或生成输出，并处理候选真实附件状态、K5-5 与 body 残余，不得删轨或放宽 tau。

> **2026-11-15 最新状态**（详见 [CAL2 三进程/120 代网格协议预注册](docs/diagnostics/g5-cal2-20261115/README.md) 与 [门 4 配对/比较协议预注册](docs/diagnostics/g5-gate4-20261115/README.md)）：**C-G5-CAL2 v1 评审通过（环境 = `AE-Track2`）**——控制 B 重组为 CAL v1 §五.1/§五.2/§五.6 完整形状（每进程 baseline + 120 完成代 × 2 读，预定网格时间直采，3 独立进程），全过 = K2-C4 升网格 READY、CAL1 控制 B 资格就绪；`tau_*` 推导属 CAL1。同日门 4：C-G5-G4 v1 协议/实现预注册/dry-run 完成（候选 247/247 配对、三锚全中）。当前：v104 已部署，run 1–3/3 已完成并在 [CAL2 §十二](docs/diagnostics/g5-cal2-20261115/README.md#cal2-c2e-oar-addendum) 的 OAR 精确豁免下通过 C2-A/B/C/E/F；三进程两两 119,790/119,790 逐位相同，**C2-D PASS，K2-C4 升 grid READY**；CAL1-B 已完成：控制 B 单层姿态 **CALIBRATED_BOUNDED**，`tau_p=0.000476592`、`tau_r=6.03341E-8`、`tau_s=0.00000386264`；门 4 姿态槽位已按 [§十六](docs/diagnostics/g5-gate4-20261115/README.md#gate4-cal1b-slots) 填充。完整 CAL1 / 门 3 仍 NOT_READY（K2-C1 PARTIAL、K5-5、`epsilon_t` TSC 频率/单位未登记）；结果见 [CAL1-B §十一](docs/diagnostics/g5-cal1b-20261115/README.md#cal1b-results)，资格评审见 [评审](docs/diagnostics/g5-cal1-qualification-20261115/README.md)。CAL1-C 审计：17 条 item/wing/Belly 轨不在 MT_Idle 控制内（99/116）；TSC invariant 已证、频率约 4.291864 GHz，但读取区间 24–63 µs ≠ `dt_clock` 5–18 ms，`epsilon_t` 不能直接取读取区间；覆盖/时间修订 [C-G5-G4-COV v1](docs/diagnostics/g5-g4-cov-20261115/README.md) 与 [C-G5-TIME v1](docs/diagnostics/g5-time-20261115/README.md) 已实现并完成评估：COV 的 99/17 覆盖 dry-run 通过；TIME 产出 `epsilon_t=0.0333960` s。审计见 [CAL1-C](docs/diagnostics/g5-cal1c-coverage-timing-20261115/README.md)。**C-G5-G4 v1 评审通过，实现预注册与 Track 2 工件 metadata-only dry-run 已完成（§十四/§十五）**——协议可执行成立：候选 247/247 配对（E1 96 / idle 22 / U1 129 三锚全中）、对账恒等式闭合、零 wrap、K5-7 PASS、同参调用逐字节确定；登记候选过渡 clip localTime 阶梯/滞后形态（移交时钟轴 PENDING-CAL 槽位）。协议结构：逐记录钟族优先级 + nearest-clock（元数据规则、独立于 pose 误差）、IDUN 排除、G4P-1..9 判据；不做同刻逐代配对。`epsilon_t` 与 `dt_exclude` 处置已由 TIME v1 填充（`epsilon_t=0.0333960`；dry-run 0/247 排除），时钟轴已由 [C-G5-G4-CLOCK v1](docs/diagnostics/g5-g4-clock-20261116/README.md) 填充（CLOCK_READY；姿态 `tau_p/tau_r/tau_s` 已由 CAL1-B 填充）；A17 审计：item/wing/Belly 近似静态共享状态（span ≤3.8e-6，跨侧 `E_p≤1.97e-6`），无独立 clip 控制；保留 COV A，不声称 116/116。CAL v1 §九联合验收以 `CALIBRATED (limited)` 通过；**门 4 于 2026-11-16 FROZEN（P99 姿态/时间/时钟轴 + A17 描述性账本）**，门 5 采集授权但未运行。K5-5、K2-C1 与 A17 独立控制仍阻塞 G5/完整 CAL1；不声称 116/116。G5 未冻结/未通过。

> **2026-11-14 最新状态**（详见[第三会话总结](docs/diagnostics/session-summary-20261114-k6.md) 与 [独立行为图验证](docs/independent-behavior-graph.md) 顶部）：**K6 关闭（限定范围）**——[K6 计划](docs/diagnostics/g5-k6-plan-20261114/README.md)评审通过（OAR 两轨制），v103 三仪器（CPK5 v1 快照 + 层审计 + RECON 对账）候选/源两侧各一次运行**全判据 PASS**（候选 249/249、7/7 data FNV 命中 v101 集合；源 906/906、7 命中 + 2 个 MT_Idle 声明缺口），K5 仪器资格成立；CAL v1 §三 K1–K6 形式齐备，但姿态等价与门 3 校准仍不可启动（控制 B bounded、K1-C4 NOT MATCHED、K5-5 权重缺项），下一步 **Track 2 与 CAL2 预注册**。`tau_*` 未产生，G5/G6 不关闭。当前 MO2 安装为 v103 source 运行档（非默认产品档）。下方 11-14 前段与 11-12 段落为历史状态，保留作时间线。

The default product path uses the copied scene tree, OAR clip selection/sampling and native DAC preparation. [Animation](docs/animation.md) owns equipment policy, torch composition, accepted clocks and configuration; [SceneGraph](docs/scene-graph.md) owns capture, placement, attachments and drawing.

更新：2026-11-12。[C-G5-OBS v2](docs/independent-behavior-graph.md#c-g5-observation-v2-20261112)已完成候选诊断的离线修复：精确 float32 clocks、equip/unequip 分段、Generate 后 checkpoint、120/最多 180 推进与真实 completed/timeout/invalid。两个 focused 场景通过；原 v96 包不在本检出中，已从同一提交源码重建为 [v96a 手工包](dist/g5-observation-v2a-20261112/MANUAL.md)（[重建记录](docs/diagnostics/g5-observation-v2a-20261112/result.md)），并已于 2026-11-12 [部署](docs/build.md#deploy-20261112-v96a)。首次原生运行（[日志](docs/diagnostics/CharacterPanel-g5cal-v96a-observation-v2-run1-20261112-2157.log)）取得 clip-family PASS 与本场景 `state 12→0` PASS（completed=129，`default=0`）；[K1/K2 账本](docs/independent-behavior-graph.md#c-g5-cal-k1k2)随之更新，K2-C3 PASS，K2-C1/C2 仅部分、K1-C1–C4 与 K2-C4 仍 NOT_READY。[D1→D3 历史](docs/independent-behavior-graph.md#status-20261111-d3-session)保留 clip-family 正结果，但“再一帧完成”和五字最小性/writer 角色仍未证。[离线 A](docs/independent-behavior-graph.md#c-g5-cal-a-run1-20261111)保持原工件与边界；时钟速率/量化、K3、K5/K6、原生 B、时间资格与姿态等价仍待补；源图 **B3/B4 后处理边界已在 v97d 单窗口 739/739 PASS（K4 本会话边界）**（[记录](docs/build.md#g5-e2b-b3b4-v97d-run1-20261113)），K3-C 候选侧全量同代快照（v98）已 PASS（249 条记录、双读一致、116 轨、零溢出），K3-S 源侧完成边界快照已实现；K3 两侧已闭合：候选 K3-C（v98，249 条）与源 K3-S（v99b，1,151 条）各自双读一致、116 轨、零溢出；跨侧逐代配对不在 K3（inject 期间源图暂停），属后续联合验收；K2-C4 控制 B 已做资格审计，仍 **NOT_READY**（运行时 `mt_idle` 被 Vanargand loose 覆盖 + OAR 引用；缺独立 spline 真值解码器与任意 HKX loader）；K1 与 K2-C4 控制 B 仍未闭合，数值容差未产生，G5 未冻结/未通过，G6 后置，装载 teardown NO-GO。

G4 的历史裁定及限定见 [G4 收官](docs/independent-behavior-graph.md#g4-closure)；[G5 v2](docs/independent-behavior-graph.md#g5-draft)拥有完整验证目标，[CAL v1](docs/independent-behavior-graph.md#c-g5-cal-v1)拥有当前校准要求和缺项。E1–E4 的日期记录与原始日志保留各自边界，不能用旧“待运行”摘要覆盖后续证据。


候选与控制器源图共享行为 data 和绑定集，变量活值分储、从 authored defaults 初始化；共享资源与重复候选 pose 哈希均不能证明与源完整姿态、时钟或层混合一致。G2 的实际写骨 109/116、三条缺失名未知，以及同 controller 来源替换/frozen-only 等边界限制继续保留。

最近**记录部署为 v95**（[D3 候选 prime](docs/build.md#v95-g5cal-d3-candidate-prime-20261111)），当前运行档为 D3 候选 prime `84e45be3…`（`IndependentGraph=inject`、`CandidateEquipPrime=true`、`CandidateEquipPrimeBranch=true`；v94 + D3 三件套备份在 `dist/deploy-backup-20261111-v94d3-v95pre/`），v95 已上机并取得 clip-family PASS；该档不是校准配置。下面列出仓库 dormant 样例的主要关闭键；完整状态见[当前部署与开关](docs/independent-behavior-graph.md#deploy-state)。

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
