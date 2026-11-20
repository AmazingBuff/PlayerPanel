# C-G5-TRANS v9：逐轨权重与三点反解探针（2026-11-20）

**状态：IMPLEMENTED / BUILT / DEPLOYED；候选 run 1 完成（2026-11-20 23:51）；H-W4 成立、H-W5 否证、三点解恒 0（姿态分段常量，只在采样步进一次）。**

v8 把「捕获 ≠ 采样」定死，并留下一条极干净的等差数列：装备段的 `max-dpos` 从 32.20 按**每观测点 2.8** 线性降到 1.40，随后在时钟跳变处重开到 25.20。这种收敛只可能来自随时间线性变化的权重。v9 直接读权重：`hkaAnimationControl::transformTrackWeights`（`+0x18`，每轨 1 字节，size=99）的值、`control->weight`、`binding->blendHint`，并用「上一帧姿态 / 当前姿态 / 当前采样」三点反解每轨实际权重。

## 一、假设与判别（预注册）

| 编号 | 假设 | 可否证读数 |
| --- | --- | --- |
| H-W3 | 逐轨权重表在装备 2–12 段线性 ramp | `w-min`/`w-max`/`w-hash`/`w-head` 逐点变化，且三点解 `solve-median` 从 ≈0.09 升到 ≈0.95 |
| H-W4 | 权重表恒定（全 1 或全同值），线性收敛另有原因 | `w-hash` 全程不变、`w-min = w-max`，`solve-median` 恒定 |
| H-W5 | `SamplePartialTracks` 是「与缓冲混合」而非「覆盖」 | 预填 `poseLocal` 后再采样：`pfill-agreed` 高；零填（v8）时低 |

三类读数互不排斥，按 §六分叉。

## 二、采集范围与行格式

- 窗口与 v7/v8 相同：`equip` 1–13、`unequip` 1–12 的 `pre-sample`/`post-sample`，共 **50** 点；本轮同时开启 v7/v8，日志约 150 行。
- 键：`[Experiment] TransitionClipWeightProbe=false`（默认关）；要求 `TransitionStateProbe=true`、`K5FullSample=true`，仅 AE 1.6.1170、仅候选侧。
- 行：

```text
TRANSITION CLIP WEIGHT side=candidate segment=equip phase=pre-sample graph=0x.. behavior=0x.. actor=00000014 request=1 commanded=2 completed=1 clip=0x.. animation='Animations\1HM_Equip.hkx' control=0x.. tracks=99 pose=0x.. num-pose=116 weights=99 w-min=.. w-max=.. w-head=.. w-hash=.. ctl-weight-bits=3F800000 blend-hint=0 pfill-compared=99 pfill-agreed=.. pfill-max-dpos=.. solved=.. solve-median=.. solve-min=.. solve-max=.. status=ok
```

- `w-*`：`transformTrackWeights` 的 count / 最小 / 最大 / 前 8 项 / 全表 FNV-1a 32 哈希（顺序敏感）。
- `pfill-*`：把 `poseLocal` 预填进采样缓冲后再调 `SamplePartialTracks`，再与 `poseLocal` 比较（判据同 `clip_reference_policy.h`）。零填版本的对照在 v8 的 `TRANSITION CLIP CAPTURE` 行。
- `solve-*`：逐轨取「采样与上一帧分离最大的平移分量」，解 `w = (current − previous) / (sample − previous)`；分离量 ≤ 1e-3 的轨跳过；统计中位数/最小/最大。
- 只读：全部采样写探针本地缓冲，三点解的「上一帧」是探针自己的静态快照；不写图/姿态/变量、不新增 detour。

## 三、判据（不得事后放宽）

| 编号 | 判据 | 通过条件 | 否则 |
| --- | --- | --- | --- |
| CC9-1 身份/环境 | v118 三件套/存档/`AE-Track2`；K5 249/249；747 header `flags=0`；零非豁免 `[E]` | 全满足 | INVALID |
| CC9-2 仪器成立 | 50 行 `TRANSITION CLIP WEIGHT` 全 `status=ok`；`weights` 与 v8 的 `ctl-track-weights` 相同；`solved>0`（cmd≥2 起） | 全满足 | 观测缺口 |
| CC9-3 权重判别 | H-W3 或 H-W4 单分支命中；`w-hash`/`solve-median` 给出明确走势 | 单一分支 | 先诊断混用 |
| CC9-4 无副作用 | 无新 detour/写入；K5 账本与 v117 相同；`source-drift=none` | 全满足 | INVALID |

## 四、上机步骤

1. 关闭 Skyrim/SKSE；MO2 选 `AE-Track2`（OAR 保持停用）。v117 三件套已备份到 `dist/deploy-backup-20261120-v117-v118pre/`，v118 三件套已装入 CharacterPanel 模组。
2. 确认 `IndependentGraph=inject`、`CandidateEquipPrime=true`、`CandidateEquipPrimeBranch=true`、`CandidateInputProbe=true`、`CandidateUnequipProbe=true`、`CandidateUnequipEvent=Unequip`、`K5FullSample=true`、`TransitionStateProbe=true`、`TransitionClipClockProbe=true`、`TransitionClipCaptureProbe=true`、`TransitionClipWeightProbe=true`；`TransitionParentChainProbe=false`、`TransitionSubgraphProbe=false`、`SamplingLiveness=off`。
3. 载入 Save116 → 打开一次背包 → 等 K5 写入 → 关闭面板并退出。
4. 保留完整 `CharacterPanel.log` 与同 session 的 `CharacterPanel-g5k5-*.bin`、`.json`。

## 五、实现、构建与部署

**实现（提交 `abb8c96`）**

| 文件 | 变更 |
| --- | --- |
| `src/animation/behavior_graph/transition_clip_weight_policy.h` | 新增：`clip_weight_summary`（count/min/max/FNV-1a 32）、`clip_weight_solve_sample`（单分量反解，含分母门限与非有限值拒绝）、`clip_weight_solve_summary`（count/min/max/中位数） |
| `src/animation/behavior_graph/transition_probe.h/.cpp` | 新增 `log_transition_clip_weight`：权重表读数、预填采样对照、三点解（静态上一帧快照，按 request 重置） |
| `src/animation/settings.h/.cpp` | 新键 `TransitionClipWeightProbe` 解析、日志、getter；默认 false |
| `src/animation/behavior_graph/independent_graph.cpp` | 同一窗口条件调用 + `status=inactive` 提示 |
| `tests/transition_probe_test.cpp` | 新增 2 例（权重汇总与哈希顺序敏感、反解与中位数）；总计 16/16 |
| `config/CharacterPanel.ini` | 样例键 `TransitionClipWeightProbe=false` + 注释 |

**离线测试**：`cl /nologo /std:c++latest /EHsc /W4 /WX /utf-8 /I src tests\transition_probe_test.cpp` → **16/16、exit 0**。

**构建（2026-11-20 23:44）**：干净树（`abb8c96`）用 `cmake --preset Release -DCHARACTER_PANEL_BUILD_LABEL=v118-g5trans-clip-weight-20261120` + `cmake --build --preset Release`，VS 2022 v143、vcpkg `x64-windows-static-md`，exit 0；身份 `CharacterPanel-scenegraph-v118-g5trans-clip-weight-20261120-abb8c969df-cl94faaed0c6-20261120T154338Z`（无 `+dirty`）。

| 工件 | 字节 | SHA-256 |
| --- | ---: | --- |
| CharacterPanel.dll | 2,139,648 | `C040533B2DF6C6DB5EA0F6F598C92FE7E2B3C967F833DD46F295FAE40F2C3F8D` |
| CharacterPanel.pdb | 52,858,880 | `6922DD6E681135F1079444064298C85A9C0926EC2BE65432157FFD479983DC60` |
| 候选 INI | 17,756 | `47FB9EE16B4D01DED47805442D132E8B63D88FE9873AB4389EE93B9326F7F259` |
| 运行包 ZIP `CharacterPanel-v118-AE-Track2.zip` | 14,433,524 | `30856088A9E6ABAD12C8F9161CCA675812BED27735C5BBEE0B9503498AA6EA1E` |

**部署（2026-11-20 23:45；Skyrim/SKSE 未运行，MO2 打开）**：安装位 v117 三件套先备份到 `dist/deploy-backup-20261120-v117-v118pre/`（DLL 2,128,896 B `286affef…`、PDB 52,776,960 B `1985e4e0…`、候选 INI 17,508 B `2d65a3a1…`，逐位核对）。v118 DLL/PDB + 候选档装入 `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\`，安装位哈希 `c040533b…`/`6922dd6e…`/`47fb9ee1…`；DLL 内身份与 `TRANSITION CLIP WEIGHT` 模板在位。**未启动游戏**。

## 六、判读与分叉

1. **H-W3 成立**（权重 ramp）：v8 的等差收敛就是权重斜坡；G5 的时钟↔姿态配对必须显式包含权重——`poseLocal` 在冻结的 clip 时间下仍逐帧变化，因为混合权重在变。下一轮把权重作为已知量重算配对，不再假设权重恒 1。
2. **H-W4 成立**（权重恒定）：线性收敛另有来源。此时先看 H-W5 的预填对照：若 `pfill-agreed ≈ compared`，说明控制是「与缓冲混合」；否则需要转为审计 `hkbClipGenerator::Generate` 的写入路径（是否只写部分轨）。
3. **H-W5 单独成立**：采样语义确认为混合式，v8 的「捕获 ≠ 采样」需按此重述（比较的是混合结果而非纯 clip 输出）。
4. 任何 `status=no-control`/`invalid-*`、`solved=0`（cmd≥2）或 `[E]` 都判观测缺口，保留原始日志。

## 七、候选 run 1（2026-11-20 23:51–23:55）

50/50 `TRANSITION CLIP WEIGHT` `status=ok`；K5 249/249、747 header、唯一 `[E]` 为 Track 2 OAR 豁免。权重表 `weights=99`、`w-min = w-max = 255`、`w-hash` 恒 `67508074`、`w-head` 全 `255`，`ctl-weight-bits=3F800000`、`blend-hint=0`——**H-W4 成立**（没有任何权重在 ramp）。预填 `poseLocal` 后再采样与零填版本给出同样的 `pfill-agreed` 10–16/99——**H-W5 否证**（`SamplePartialTracks` 覆盖写入）。

三点解 `solve-median / min / max` 在全部轨、全部命令上恒为 **0**：`pre(N+1)` 与 `post(N)` 的 `poseLocal` 逐轨相同，姿态只在每个命令的采样步进一次（与 K5 装备 completed 2–12 的 11/11 唯一姿态一致）。**CC9-1/2/3/4 全 PASS。**

因此 v8 的等差收敛（32.20 → 1.40，每命令 2.8）不是权重斜坡，而是姿态每命令朝冻结采样前进一个固定量；结合 v7 的 `frozen-no-step`，只剩两种解释：**S1 增量写入**（图把差值的一部分写回 `poseLocal`）或 **S2 有效采样时刻在推进而字段陈旧**。判别只需一次时间扫描（`t0 + k·δ`，k ∈ [−4,4]），见 [run1-candidate](run1-candidate/README.md) §五。`G4P-7` 仍 **FAIL**。

## 八、边界

- 单次运行、单存档、单装备状态；不含源侧。
- 不改变 `G4P-7 FAIL`、G5/G6 与任何 `tau_*`。
- 只对 AE 1.6.1170 + v118 工件成立；权重语义的结论不外推到其它 clip 或装备族。
