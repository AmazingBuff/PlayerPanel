# CharacterPanel

C++23 SKSE inventory preview using a copied [SceneGraph](docs/scene-graph.md), with equipment-driven non-combat idles independent of source world actions. Native DAC temporarily owns bounded phase effects on the source Actor during inventory and restores the entry effects at close. No second Actor is created.

## Current status

The default product path uses the copied scene tree, OAR clip selection and native DAC preparation. See [Animation](docs/animation.md), [SceneGraph](docs/scene-graph.md) and the [test results overview](docs/diagnostics/test-results-20261118.md#product).

**2026-11-25（当前）**。独立行为图 **G1–G6 全部有界签收**，验证线收官；下一步转向面板功能开发（PRD §9.5：单角色渲染 → 网格背包 → 环绕装备槽）。

- **G5**（11-22）：冻结容差机械 FAIL（精度记录）+ [C-G5-TOL v1 验收 PASS](docs/diagnostics/g5-tol-contract-20261122/README.md)——逐轨容差表，244/272 干净 + 28 笔预注册机制族超差由用户接受 → **有界签收**。
- **G6**（11-23）：[C-G6 v1](docs/diagnostics/g6-contract-20261122/README.md) G6-A 生命周期 S1–S5 全 PASS（读档拒绝原因落定 `unbound-retained`）；[C-G6-CON v3](docs/diagnostics/g6-con-v3-contract-20261122/README.md) G6-B 收敛：`currentDefaultState` 四域对（0 收鞘 / 1 拔刀 / 2 魔法 / 11 过渡跳过）+ 四事件派发，拔刀/魔法/收鞘三域 clip 家族全归一化相等（v123–v126 六轮）。
- **`idle-start-replay` 结案为已知限制**：C-IDLE → [v127 上机](docs/diagnostics/idle-loop-v127-session-20261124/README.md) → [C-WORLD v1](docs/diagnostics/world-phase-20261125/README.md) 三轮取证。新增只读逐帧探针 `WorldPhaseProbe` 在面板关闭时采样 722 行，证明 `mag_start on`(801) 在正常游玩中**同样从不被清除**（`phase=both` 401 行、`loop` 0 行）——缺陷不是"面板缺一步"，世界也不做那步；方向 B 否决、方向 A 不实施，[契约 §十](docs/diagnostics/idle-loop-contract-20261123/README.md) 记录最终事实与重启所需的最小取证。面板内影响：idle 每轮重放一次起手段，动作正确无扭曲。
- **架构发现（v127）**：`DisplayPose` 决定面板动画来源——`true` 走候选图 `poseLocal`（**不含** OAR 替换，显示原版动作），`false` 走控制器合成（显示 Smooth 动作）。这解释了长期观察到的"面板动作与游戏世界不一致"。

- **2026-11-26（v128 上机完成：Start 驻留不再复现）**。面板的 OAR 查询读取原始角色的 DAC 标记，排除已驱散的实例。同类未驱散且符合条件的实例照常计入。v128 DLL `164eacea…` / PDB `8adc5234…` + 验证档 INI `91acf06f…`（只开 `DetailedAnimationTrace`，保留 `DisplayPose=false`）。**上机结果**：10 个 request / 27 次面板选择中，**8/8 可判定武器族全部完成 Start → Loop**，包含 v127 记录为"卡 Start"的 2hm、2hw Axe、换装后 1hm；法杖族因走空 `logical` 不可判定。**判据修订**：MANUAL 要求的 `cause=effect-correction-change` 本轮 0 次，经代码复核该字段对"修正是否生效"无鉴别力（驱散发生的那一刻必报 `effect-signature-change`），已改用面板选择序列。**未证**：无直接因果证据（修正本身无日志点），`both` 状态下的 OAR 上下文分歧仍未解释。[判读](docs/diagnostics/v128-dac-dispel-20261126/README.md) · [面板效果判定](docs/dac.md#dispelled-effects-in-preview-queries) · [构建记录](docs/build.md#v128-dac-dispel-20261126)

  本轮构建同时完成了工具链迁移：`17b432f` 换成 `Visual Studio 18 2026` 时删掉了 `buildPresets`，`cmake --build --preset Release` 一度失效；`3e61d42` 已补回。新工具链是 CMake 4.3.1-msvc1 + 工具集 v145/14.52.37025（原为 CMake 3.31.4 + v143/14.44.35207），**该差异不出现在嵌入式身份里**。`C:/env/CMake` 的 3.31.4 不支持新生成器，须用 VS 自带的 CMake；旧 `build/` 缓存已迁到 `build/_vs2022-config-stash/`。

仍开放（不阻塞功能线）：K5-5、K2-C1、A17、EQ-C1 补测、装载 teardown NO-GO、`currentDefaultState=11` 瞬态未实测、idle 相位 OAR 上下文分歧（相同 `both` 标记状态下世界转 Loop 而面板继续选 Start）。

**2026-11-18（历史）**。G1–G4 在登记范围内成立。门 5 候选 249、源 849 完整采集，得到 248 对，姿态验收 **G4P-7 FAIL**。G6 待验。过渡离线审计已定位引擎中参与姿态混合的计时字段；字段补审计确认 `0→10` 为 0.2 s 的 `hkbBlendingTransitionEffect`，`1HM_Equip` 走 `Weap_EquipDefaultState/msg/State3` 的 Blender 分支。v110 候选 run 1 的 K5 链 249/249、equip 120、unequip 129、`state-12-to-0`；Track 2 环境正确（OAR 关闭是该档设计，`identity-failed=1` 为已知 SCAR dummy 记账），但探针批次 **INVALID**：4 个状态机上限 overflow、K5 post-sample 全部 `transitions=0`、未见 `State3` Blender；不能支持或否证过渡假设。v111/v2 候选 run 2 已上机：747 探针 header、78 EFFECT（26 个 transition 实例）；`0→10` 在 cmd=1 采到 0.2 s blending transition，`10→1` 在 cmd=87–98 完整推进到 0.183333 且 12 点 PC1 99.83%/Pearson 0.992817；72 条 overflow（equip 87–98、unequip 1–12）使 TP-2 FAIL，census 无 `State3`，TP-5 未目击。v112/v3 候选 run 3 已上机：747 header 全部 flags=0；`0→10` cmd=1 parent transition、`10→1` 12 点 PC1 99.83%/Pearson 0.992817；AUX transition-flags size4 head `0000E9D1`、wildcard 全 0、delayed 0 且 cmd 1–13 恒定；census 仍无 `State3`，TP-5 未目击。v113/v4 候选 run 4 已上机：modifier 绑定路线判负（equip cmd 2–13 活跃 modifier 全 bindings=0，BIND word 恒定），`10→1` PC1 99.83%/Pearson 0.992817 复现；剩余假设域为 `WeapEquip.hkb` 子图、`allPartitionMemory`、`hkbBlendingBehavior`。v5 已实现为 v114（`hkbBehaviorReferenceGenerator` `behavior +0x50` → `hkbBehaviorGraph`；候选专用、log-only；离线测试 8/8、构建身份 `…-v114-g5trans-subgraph-20261119-94003147d5-…`；已部署；候选 run 1 完成，H-B1 判负），见 [C-G5-TRANS v5](docs/diagnostics/g5-transition-probe-v5-20261119/README.md) 与 [run 1](docs/diagnostics/g5-transition-probe-v5-20261119/run1-candidate/README.md)。详见[run 1 记录](docs/diagnostics/g5-transition-probe-20261118/run1-candidate/README.md)、[v2 预注册](docs/diagnostics/g5-transition-probe-v2-20261118/README.md)、[测试结果总览](docs/diagnostics/test-results-20261118.md)和[离线审计](docs/diagnostics/g5-transition-audit-20261118/README.md#六离线字段补审计2026-11-18v3-dump)。

The repository configuration keeps the independent graph experiments dormant by default.

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

The plugin compiles only the 39 explicit `.cpp` units in [CMakeLists.txt](CMakeLists.txt), `PLUGIN_SOURCES`. Add each new intended unit to that sorted list and reconfigure; unlisted local `.cpp` files are excluded. The preserved untracked util files remain untouched; historical v42–v44 builds used the old source glob. [Build overview](docs/build.md) records the current inventory; the [v45 boundary record](docs/build.md#v45-source-list-2026-10-29) preserves its dated 30-unit artifact.

Prepare the pinned checkout/toolchain in [Build](docs/build.md#dependencies-and-checkout), then run from this repository:

```powershell
cmake --preset Release
cmake --build --preset Release
```

Run these with the CMake bundled with Visual Studio 2026 (for example `C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`). That bundled CMake is 4.3.1-msvc1; the older 3.31.4 install in `C:/env/CMake` cannot create the `Visual Studio 18 2026` generator this preset names and fails at configure. A `build/` cache written by the previous VS 2022 generator must be recreated first, because CMake refuses to switch generators in place.

Release is the sole configure/build preset; CharacterPanel is the sole first-party target. Outputs are `build/Release/CharacterPanel.dll` and its matching PDB. Keep that pair with its configure-time `SCENEGRAPH BUILD` identity. The 2026-11-17 test record notes a return to v104 after the state probes. Each artifact retains its own build identity and hashes in [Build](docs/build.md). A later commit does not relabel existing artifacts.

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
