# C-G5-TRANS v8：捕获点与映射判别探针（2026-11-20）

**状态：IMPLEMENTED / BUILT / DEPLOYED（v117）；候选 run 待上机。**

v7 已把「时钟冻结 + 权重恒定 + 姿态逐帧变化」定死（[run 1](../g5-clip-clock-probe-20261120/run1-candidate/README.md)），剩余路线只有 M2（采样后写入者）与 M3（捕获点/映射）。v8 在**同一批观测点**把控制自身的采样与 `hkbCharacter::poseLocal` 逐轨比较，并记录 binding 的轨→骨映射与位移统计，用一条行给出 M2/M3 的判别量。

## 一、假设与判别（预注册）

| 编号 | 假设 | 可否证读数 |
| --- | --- | --- |
| H-CAP1 | 捕获缓冲就是该 clip 经控制采样的输出 | `agreed ≈ compared`（99/99 量级）、`max-dpos ≈ 0` |
| H-CAP2 | 两者只差一个整体平移（坐标系/根轨道问题） | `agreed` 低，但 `resid-max` 小（去均值后残差≈0），`mean-shift` 明显非零 |
| H-CAP3 | 两者内容不同（采样后被改写，或另一张图写同一缓冲） | `agreed` 低且 `resid-max` 大，`mismatch-head` 覆盖多数轨 |

任一假设被否证即按 §六分叉，不用同一探针重跑「碰」结果。

## 二、采集范围与行格式

- 窗口与 v7 相同：`equip` 1–13、`unequip` 1–12 的 `pre-sample`/`post-sample`，共 **50** 点（本轮同时开启 v7 时钟探针，日志约 100 行）。
- 键：`[Experiment] TransitionClipCaptureProbe=false`（默认关）；要求 `TransitionStateProbe=true`、`K5FullSample=true`，仅 AE 1.6.1170、仅候选侧。
- 身份：与 v7 同一选种规则（动画族：`1HM_Equip.hkx` / `1HM_Unequip.hkx`）。
- 行：

```text
TRANSITION CLIP CAPTURE side=candidate segment=equip phase=pre-sample graph=0x.. behavior=0x.. actor=00000014 request=1 commanded=1 completed=0 clip=0x.. animation='Animations\1HM_Equip.hkx' control=0x.. binding=0x.. animation-data=0x.. tracks=99 pose=0x.. num-pose=116 map-len=0 map-head=identity ctl-local-time-bits=3C888889 compared=99 agreed=16 unmapped=0 max-dpos=2.344827 max-drot=0.024934 max-dscale=0.148320 mean-shift=0.0,0.0,0.0 resid-max=3.07 moved=64 mismatch-head=1,2,3,4,5,6,7,8 status=ok
```

- `map-len`/`map-head`：`hkaAnimationBinding::transformTrackToBoneIndices`（`+0x20`）；空表时输出 `identity`（引擎 `0xB03380` 的同一规则）。
- `ctl-local-time-bits`：`hkaAnimationControl::localTime`（`+0x10`），与 v7 的 `clip->localTime` 对账。
- `compared/agreed/max-*`：沿用既有 `clip_reference_policy.h` 的判据（位移 `1e-4`、旋转 `1e-5`、缩放 `1e-4`），与历史 `clip-ref` 行可逐项对照。
- `mean-shift`/`resid-max`/`moved`：把每轨位移（捕获 − 采样）先求均值再算残差，用来区分「整体平移」与「内容不同」。
- 只读：控制采样写入探针本地缓冲（`SamplePartialTracks`，与既有 `clip-ref` 的直采样同性质）；vtable 校验后才转换控制类；不写图/姿态/变量、不新增 detour、`TransitionProbeRecord`/CPK5 不变。

## 三、判据（不得事后放宽）

| 编号 | 判据 | 通过条件 | 否则 |
| --- | --- | --- | --- |
| CC8-1 身份/环境 | v117 三件套/存档/`AE-Track2`；K5 249/249；747 header `flags=0`；零非豁免 `[E]` | 全满足 | INVALID |
| CC8-2 仪器成立 | 50 行 `TRANSITION CLIP CAPTURE` 全 `status=ok`；`tracks=99`、`num-pose=116`、`map-len` 与 v7 一致；`ctl-local-time-bits` 与同点 v7 行相同 | 全满足 | 观测缺口 |
| CC8-3 比较成立 | `compared>0`、`unmapped=0`，且三条假设中恰好一条被命中 | 单一分支 | 先诊断混用 |
| CC8-4 无副作用 | 无新 detour/写入；K5 账本与 v116 相同；`source-drift=none` | 全满足 | INVALID |

## 四、上机步骤

1. 关闭 Skyrim/SKSE；MO2 选 `AE-Track2`（OAR 保持停用）。v116 三件套已备份到 `dist/deploy-backup-20261120-v116-v117pre/`，v117 三件套已装入 CharacterPanel 模组。
2. 确认 `IndependentGraph=inject`、`CandidateEquipPrime=true`、`CandidateEquipPrimeBranch=true`、`CandidateInputProbe=true`、`CandidateUnequipProbe=true`、`CandidateUnequipEvent=Unequip`、`K5FullSample=true`、`TransitionStateProbe=true`、`TransitionClipClockProbe=true`、`TransitionClipCaptureProbe=true`；`TransitionParentChainProbe=false`、`TransitionSubgraphProbe=false`、`SamplingLiveness=off`。
3. 载入 Save116 → 打开一次背包 → 等 K5 写入 → 关闭面板并退出。
4. 保留完整 `CharacterPanel.log` 与同 session 的 `CharacterPanel-g5k5-*.bin`、`.json`。

## 五、实现、构建与部署

**实现（提交 `282fb3e`）**

| 文件 | 变更 |
| --- | --- |
| `src/animation/behavior_graph/transition_clip_capture_policy.h` | 新增：容量/上限、`clip_capture_bone`（轨→骨，含 identity 规则）、`clip_capture_shift`（均值位移与去均值残差） |
| `src/animation/behavior_graph/transition_probe.h/.cpp` | 新增 `log_transition_clip_capture`；抽出共享的 `find_family_clip` 选种（v7 时钟探针改用同一实现） |
| `src/animation/settings.h/.cpp` | 新键 `TransitionClipCaptureProbe` 解析、日志、getter；默认 false |
| `src/animation/behavior_graph/independent_graph.cpp` | 在同一窗口条件调用 + `status=inactive` 提示 |
| `tests/transition_probe_test.cpp` | 新增 2 例（轨→骨映射、均值/残差汇总）；总计 14/14 |
| `config/CharacterPanel.ini` | 样例键 `TransitionClipCaptureProbe=false` + 注释 |

**离线测试**：`cl /nologo /std:c++latest /EHsc /W4 /WX /utf-8 /I src tests\transition_probe_test.cpp` → **14/14、exit 0**。

**构建（2026-11-20 23:34）**：干净树（`282fb3e`）用 `cmake --preset Release -DCHARACTER_PANEL_BUILD_LABEL=v117-g5trans-clip-capture-20261120` + `cmake --build --preset Release`，VS 2022 v143、vcpkg `x64-windows-static-md`，exit 0；身份 `CharacterPanel-scenegraph-v117-g5trans-clip-capture-20261120-282fb3eedc-cl94faaed0c6-20261120T153353Z`（无 `+dirty`）。

| 工件 | 字节 | SHA-256 |
| --- | ---: | --- |
| CharacterPanel.dll | 2,128,896 | `286AFFEF5A4DAD4EE6ABB010874085CA1E40D6D604A279718529466BADAB4CED` |
| CharacterPanel.pdb | 52,776,960 | `1985E4E07A52E85752559C363D3A840410F2B914B5FDDA4B839EE2348368A8E9` |
| 候选 INI | 17,508 | `2D65A3A12E59DA58EECFD663A52A5410B323BE844A57D36A7E30D75ABBE4BB13` |
| 运行包 ZIP `CharacterPanel-v117-AE-Track2.zip` | 14,375,241 | `72450CDBD8C3A359B3DB6AD0DE4836952A7EAF9F7A8F1E7706601533609BAB27` |

**部署（2026-11-20 23:35；Skyrim/SKSE 未运行，MO2 打开）**：安装位 v116 三件套先备份到 `dist/deploy-backup-20261120-v116-v117pre/`（DLL 2,120,704 B `43654aed…`、PDB 52,695,040 B `5370fdfe…`、候选 INI 17,315 B `c9902a42…`，逐位核对）。v117 DLL/PDB + 候选档装入 `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\`，安装位哈希 `286affef…`/`1985e4e0…`/`2d65a3a1…`；DLL 内身份与 `TRANSITION CLIP CAPTURE` 模板在位。**未启动游戏**。

## 六、判读与分叉

1. **H-CAP1 成立**（高一致）：捕获就是该 clip 的采样输出。此时必须复核 v7 的冻结读数与采样时刻是否同一瞬（`phase` 顺序、`completed` 与 `commanded` 的错位），必要时另立「采样时刻对齐」预注册。
2. **H-CAP2 成立**（一致率低但 `resid-max` 小、`mean-shift` 非零）：两者只差整体平移，属坐标系/根轨道层级问题；下一轮读 `hkbCharacter::poseLocal` 的根轨与 `setup->animationSkeleton` 的空间关系。
3. **H-CAP3 成立**（`resid-max` 大）：`poseLocal` 不是该 clip 的采样。按 E2 系列既有仪器查采样后写入者（58423 足部 IK、63581 混合、附件/手部 IK），沿用其读法，不新开偏移。
4. 任何 `status=no-control`/`no-binding`/`no-animation`/`invalid-*` 或 `[E]` 都判观测缺口，保留原始日志。

## 七、边界

- 单次运行、单存档、单装备状态；不含源侧。
- 不改变 `G4P-7 FAIL`、G5/G6 与任何 `tau_*`。
- 只对 AE 1.6.1170 + v117 工件成立；不外推到其他装备族/运行时/存档。
