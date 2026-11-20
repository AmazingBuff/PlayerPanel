# C-G5-TRANS v10：时间扫描判别探针（2026-11-20）

**状态：IMPLEMENTED / BUILT / DEPLOYED；候选 run 1 完成（2026-11-21 00:05）；CC10-3 FAIL（H-S0）——最佳偏移落在 ±4 窗口边界，半径不足，S1/S2 未定。**

v9 把权重路线关掉（权重表恒 255、`control->weight`=1.0、`blend-hint`=0），并给出一个更硬的事实：三点解恒为 0，`pre(N+1)` 与 `post(N)` 的 `poseLocal` 逐轨相同——姿态在探针时间轴上是**分段常量**，每个命令只在采样步进一次。于是 v8 的「每命令 2.8」只有两种解释：

- **S1 增量写入**：图把差值的一部分写回 `poseLocal`，采样时刻不变；
- **S2 有效采样时刻在推进**，而 `clip->localTime (+0xF0)` 与 `control+0x10` 是陈旧字段。

v10 用一次**时间扫描**直接分开：在每个观测点，用动画层在 `t0 + k/60 s`（k ∈ [−4, +4]，共 9 个候选）采样，与 `poseLocal` 比较，报最优 `k`、该点的 `max-dpos`/`max-drot`，以及整条残差曲线。

## 一、假设与判别（预注册）

| 编号 | 假设 | 可否证读数 |
| --- | --- | --- |
| H-S2 | 有效采样时刻在推进（字段陈旧） | `best-k` 随 `commanded` 每命令 **+1**（或至少单调递增），且 `best-dpos` 明显低于 `k=0` 的残差 |
| H-S1 | 采样时刻不变，写入是增量式 | `best-k` **恒 0**，曲线形状（k=0 为谷）在全部命令上一致，残差却仍按 2.8/命令缩小 |
| H-S0 | 都不是 | `best-k` 无规律、曲线平坦、或 `clamped=1` 频繁出现 → 观测缺口，先诊断 |

## 二、采集范围与行格式

- 窗口与 v7–v9 相同：`equip` 1–13、`unequip` 1–12 的 `pre-sample`/`post-sample`，共 **50** 点（本轮四个只读探针全开，日志约 200 行）。
- 键：`[Experiment] TransitionClipSweepProbe=false`（默认关）；要求 `TransitionStateProbe=true`、`K5FullSample=true`，仅 AE 1.6.1170、仅候选侧。
- 行：

```text
TRANSITION CLIP SWEEP side=candidate segment=equip phase=pre-sample graph=0x.. behavior=0x.. actor=00000014 request=1 commanded=2 completed=1 clip=0x.. animation='Animations\1HM_Equip.hkx' control=0x.. tracks=99 pose=0x.. num-pose=116 local-time-bits=3E555556 duration=1.433300 step=0.016667 best-k=0 best-dpos=1.400070 best-drot=0.018215 best-time=0.208333 clamped=0 curve=-4:12.9000,-3:10.1000,-2:7.3000,-1:4.5000,0:1.4000,1:2.3000,2:5.1000,3:7.9000,4:10.7000 status=ok
```

- `t0` 取 `control->localTime (+0x10)`（与 v7/v8/v9 读的同一字段），`step = 1/60 s`，候选时刻在 `[0, duration]` 内夹取（`clamped` 标记任一候选被夹）。
- 残差口径与 v8/v9 一致：逐轨（按 `binding` 映射）比较平移分量最大值与旋转 `1−|dot|`，取全轨最大。
- 只读：采样写入探针本地缓冲；不写图/姿态/变量、不新增 detour、`TransitionProbeRecord`/CPK5 不变。

## 三、判据（不得事后放宽）

| 编号 | 判据 | 通过条件 | 否则 |
| --- | --- | --- | --- |
| CC10-1 身份/环境 | v119 三件套/存档/`AE-Track2`；K5 249/249；747 header `flags=0`；零非豁免 `[E]` | 全满足 | INVALID |
| CC10-2 仪器成立 | 50 行 `TRANSITION CLIP SWEEP` 全 `status=ok`；`duration` 与 v8 的 `eff-duration` 一致；`best-time` 落在 `[0, duration]` | 全满足 | 观测缺口 |
| CC10-3 判别成立 | H-S2 或 H-S1 单分支命中；`clamped` 不得在装备 2–13 连续出现 | 单一分支 | 先诊断 |
| CC10-4 无副作用 | 无新 detour/写入；K5 账本与 v118 相同；`source-drift=none` | 全满足 | INVALID |

## 四、上机步骤

1. 关闭 Skyrim/SKSE；MO2 选 `AE-Track2`（OAR 保持停用）。v118 三件套已备份到 `dist/deploy-backup-20261120-v118-v119pre/`，v119 三件套已装入 CharacterPanel 模组。
2. 确认 `IndependentGraph=inject`、`CandidateEquipPrime=true`、`CandidateEquipPrimeBranch=true`、`CandidateInputProbe=true`、`CandidateUnequipProbe=true`、`CandidateUnequipEvent=Unequip`、`K5FullSample=true`、`TransitionStateProbe=true`、`TransitionClipClockProbe=true`、`TransitionClipCaptureProbe=true`、`TransitionClipWeightProbe=true`、`TransitionClipSweepProbe=true`；`TransitionParentChainProbe=false`、`TransitionSubgraphProbe=false`、`SamplingLiveness=off`。
3. 载入 Save116 → 打开一次背包 → 等 K5 写入 → 关闭面板并退出。
4. 保留完整 `CharacterPanel.log` 与同 session 的 `CharacterPanel-g5k5-*.bin`、`.json`。

## 五、实现、构建与部署

**实现（提交 `7956ca2`）**

| 文件 | 变更 |
| --- | --- |
| `src/animation/behavior_graph/transition_clip_sweep_policy.h` | 新增：`Clip_Sweep_Radius=4`、`Clip_Sweep_Step=1/60`、`clip_sweep_time`（含 `[0,duration]` 夹取与 `clamped` 标记）、`clip_sweep_best`（相等时取离中心最近者） |
| `src/animation/behavior_graph/transition_probe.h/.cpp` | 新增 `log_transition_clip_sweep`：9 点扫描、逐点 `max-dpos`/`max-drot`、最优 `k` 与整条曲线 |
| `src/animation/settings.h/.cpp` | 新键 `TransitionClipSweepProbe` 解析、日志、getter；默认 false |
| `src/animation/behavior_graph/independent_graph.cpp` | 同一窗口条件调用 + `status=inactive` 提示 |
| `tests/transition_probe_test.cpp` | 新增 2 例（夹取、最优索引含中心优先与空输入）；总计 18/18 |
| `config/CharacterPanel.ini` | 样例键 `TransitionClipSweepProbe=false` + 注释 |

**离线测试**：`cl /nologo /std:c++latest /EHsc /W4 /WX /utf-8 /I src tests\transition_probe_test.cpp` → **18/18、exit 0**。

**构建（2026-11-20 23:59）**：干净树（`7956ca2`）用 `cmake --preset Release -DCHARACTER_PANEL_BUILD_LABEL=v119-g5trans-clip-sweep-20261120` + `cmake --build --preset Release`，VS 2022 v143、vcpkg `x64-windows-static-md`，exit 0；身份 `CharacterPanel-scenegraph-v119-g5trans-clip-sweep-20261120-7956ca2687-cl94faaed0c6-20261120T155935Z`（无 `+dirty`）。

| 工件 | 字节 | SHA-256 |
| --- | ---: | --- |
| CharacterPanel.dll | 2,148,864 | `DD599DCF9FD4E554C4D47C89CF161DE47E1D9ABF01DBE24695A9ECB6883198EE` |
| CharacterPanel.pdb | 52,924,416 | `29B6CE401E0F83D4F35F28381500DC80E15223E166714AFC10D0D5F1A33A5440` |
| 候选 INI | 17,980 | `80C4572B6F1CA4AF586661401CFD96BFDD1E77953A37E4640E5230C203CCF948` |
| 运行包 ZIP `CharacterPanel-v119-AE-Track2.zip` | 14,439,097 | `ADEA28F7B210C4AA2ACCC658E0589C230BA8EE2FA22537BD22146EAB0D960846` |

**部署（2026-11-20 23:59；Skyrim/SKSE 未运行，MO2 打开）**：安装位 v118 三件套先备份到 `dist/deploy-backup-20261120-v118-v119pre/`（DLL 2,139,648 B `c040533b…`、PDB 52,858,880 B `6922dd6e…`、候选 INI 17,756 B `47fb9ee1…`，逐位核对）。v119 DLL/PDB + 候选档装入 `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\`，安装位哈希 `dd599dcf…`/`29b6ce40…`/`80c4572b…`；DLL 内身份与 `TRANSITION CLIP SWEEP` 模板在位。**未启动游戏**。

## 六、判读与分叉

1. **H-S2 成立**（`best-k` 随命令推进）：有效采样时刻在走、被读字段陈旧。这会改写「时钟」这一整条结论：G5 的时钟轴必须改用「有效采样时刻」，并且要重新检查 `clip->localTime` 的写者（`0xACDFA3` 是否真的每帧执行）。
2. **H-S1 成立**（`best-k` 恒 0、曲线不变、残差仍按 2.8 缩小）：姿态是被增量写入的。下一层读 `hkbClipGenerator::Generate` 之后的输出缓冲写入（含 `extractedMotion` 累加、`0xACFFF0` 的写出目标），并核对 E2-A 的 58423/63581 写入路径。
3. **H-S0**：先诊断 `clamped`、`status` 与曲线形状，再决定是否换 `δ`（1/60 → 1/120）或扩大半径。

## 七、候选 run 1（2026-11-21 00:05–00:06）

50/50 `TRANSITION CLIP SWEEP` `status=ok`、`clamped=0`；`k=0` 残差与 v8 `max-dpos` 逐点一致（装备 post-sample cmd 2 `29.4015`、cmd 12 `1.4001`），两条探针互相对上。**但 50 行里有 15 行的 `best-k` 落在 ±4 边界**：装备 cmd 2–7 全在 `+4` 且曲线到边界仍单调下降，cmd 13 在 `−4`。

- 装备：`best-k` 走 `+4(边界) → 3 → 2 → 2 → 1 → 0 → −4(边界)`，`k=0` 残差 29.4 → 1.4。
- 卸下：时钟冻结在 `0` 时 9 个点全 `k=0`、残差恒 1.617；时钟每步 +1 帧后 `best-k` 依次 −1/−2/−3（姿态落后于记录时刻）。

**CC10-1/2/4 PASS，CC10-3 FAIL（H-S0）**：S1 的强形式（`best-k` 恒 0）不成立，对齐点出现在时钟台阶末尾（cmd 12），台阶跳变后立刻落后；但真实谷底在窗口外，无法分辨 S1/S2。下一步 v11 只把 `Clip_Sweep_Radius` 4 → 16（25 个候选，覆盖 0.267 s），其余不变，改读「谷底轨迹」。完整表格、曲线与分叉见 [run1-candidate](run1-candidate/README.md)。`G4P-7` 仍 **FAIL**。

## 八、边界

- 单次运行、单存档、单装备状态；不含源侧。
- 不改变 `G4P-7 FAIL`、G5/G6 与任何 `tau_*`。
- 只对 AE 1.6.1170 + v119 工件成立；扫描半径与步长是预注册值，判读时不得临时放宽。
