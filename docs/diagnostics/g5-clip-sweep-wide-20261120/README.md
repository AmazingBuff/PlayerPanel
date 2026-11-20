# C-G5-TRANS v11：时间扫描窗口加宽（2026-11-21）

**状态：IMPLEMENTED / BUILT / DEPLOYED（v120）；候选 run 待上机。**

v10 run 1 的判据 CC10-3 未通过（H-S0）：50 行里有 **15 行**的 `best-k` 落在 ±4 帧边界（装备 cmd 2–7 全为 `+4` 且曲线到边界仍单调下降），真实谷底在窗口外。本轮不改探针逻辑、不改口径，只把预注册的半径加宽。

## 一、唯一变更

| 项 | v10 | v11 |
| --- | --- | --- |
| `Clip_Sweep_Radius` | 4（9 个候选，±0.067 s） | **16（33 个候选，±0.267 s）** |
| `Clip_Sweep_Step` | 1/60 s | 1/60 s（不变） |
| 采样/比较/输出 | 动画层 `SamplePartialTracks`、逐轨分量最大差 + 旋转 `1−|dot|`、`best-k`/`best-dpos`/`best-drot`/`best-time`/`clamped`/`curve` | 同（`curve` 变 33 项） |
| 窗口 | equip 1–13、unequip 1–12 的 pre/post（50 点） | 同 |

半径 16 帧的选取：v6/v7 的时钟台阶约 11–11.5 帧，±16 足以把台阶两侧都包住；若仍贴边，按预注册下一步把 δ 改为 1/120 再扫，而不是继续扩半径。

## 二、判据

CC10-1（身份/环境）、CC10-2（仪器成立）、CC10-4（无副作用）沿用 v10 契约；**CC10-3 用更宽窗口重判**，判读规则不变：

- 谷底随 `commanded` **递减到 0** → 姿态朝记录采样收敛（S1 的具体形式）；
- 谷底随 `commanded` **递增** → 有效采样时刻在推进而字段陈旧（S2），「时钟冻结」结论需改写；
- 谷底**仍贴 ±16 边界** → 判 H-S0，转入 δ = 1/120 的下一轮，不再扩半径。

## 三、上机步骤

1. 关闭 Skyrim/SKSE；MO2 选 `AE-Track2`（OAR 保持停用）。v119 三件套已备份到 `dist/deploy-backup-20261121-v119-v120pre/`，v120 三件套已装入 CharacterPanel 模组。
2. 候选档与上一轮相同（v7–v10 四个只读探针全开；`TransitionParentChainProbe=false`、`TransitionSubgraphProbe=false`、`SamplingLiveness=off`），哈希未变：`80C4572B…C203CCF948`。
3. 载入 Save116 → 打开一次背包 → 等 K5 写入 → 关闭面板并退出。
4. 保留完整 `CharacterPanel.log` 与同 session 的 `CharacterPanel-g5k5-*.bin`、`.json`。

## 四、实现、构建与部署

**实现（提交 `cd9817f`）**：只改 `src/animation/behavior_graph/transition_clip_sweep_policy.h` 的半径常量与注释；离线用例仍 18/18（`clip_sweep_best` 与 `clip_sweep_time` 与半径无关）。

**构建（2026-11-21 00:09）**：干净树（`cd9817f`）用 `cmake --preset Release -DCHARACTER_PANEL_BUILD_LABEL=v120-g5trans-clip-sweep-wide-20261120` + `cmake --build --preset Release`，exit 0；身份 `CharacterPanel-scenegraph-v120-g5trans-clip-sweep-wide-20261120-cd9817f21a-cl94faaed0c6-20261120T160923Z`（无 `+dirty`）。

| 工件 | 字节 | SHA-256 |
| --- | ---: | --- |
| CharacterPanel.dll | 2,148,864 | `D2CD3B79D39C3D34099B676DF01C5B080223EB89166C2957EEA34E7F0C3961C3` |
| CharacterPanel.pdb | 52,924,416 | `01278F4D075EC1D601A7DDC5AAB168B01E9CF375EAD7A2042686A6C2308B6DFB` |
| 候选 INI（与 v119 相同） | 17,980 | `80C4572B6F1CA4AF586661401CFD96BFDD1E77953A37E4640E5230C203CCF948` |
| 运行包 ZIP `CharacterPanel-v120-AE-Track2.zip` | 14,439,916 | `5A0042937155B514B8C9CFA334401682B530DA48C64582E5A2B93CD5A583B893` |

**部署（2026-11-21 00:09；Skyrim/SKSE 未运行，MO2 打开）**：安装位 v119 三件套先备份到 `dist/deploy-backup-20261121-v119-v120pre/`（DLL 2,148,864 B `dd599dcf…`、PDB 52,924,416 B `29b6ce40…`、候选 INI 17,980 B `80c4572b…`，逐位核对）。v120 DLL/PDB + 同一候选档装入 `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\`，安装位哈希 `d2cd3b79…`/`01278f4d…`/`80c4572b…`；DLL 内身份 `…-v120-g5trans-clip-sweep-wide-20261120-cd9817f21a-…` 在位。**未启动游戏**。

## 五、边界

- 只改窗口，不改口径、不改探针读取的字段、不改 CPK5 与 `TransitionProbeRecord`。
- 不改变 `G4P-7 FAIL`、G5/G6 与任何 `tau_*`。
- 半径 16 是本轮预注册值：判读时不得临时扩大；若仍贴边，按 §二进入 δ = 1/120 的下一轮。
