# C-G5-TRANS v7：clip 时钟判别探针（2026-11-20）

**状态：IMPLEMENTED / BUILT / DEPLOYED（v116）；候选 run 待上机。**

承接 [H-C1 离线审计](../g5-clip-clock-20261120/README.md)：采样时间就是 `clip->localTime (+0xF0)`（`Generate` 写入 `hkaAnimationControl::localTime (+0x10)`，两个采样入口都从 `+0x10` 取时间），不存在第二条时钟。因此 [v6 run 1](../g5-transition-probe-v6-20261120/run1-candidate/README.md) 的「时间冻结但姿态逐帧不同」只能说明差异发生在采样之后。本探针只做一件事：在既有 K5 观测点把 clip 与控制的时钟/权重状态一次读全，用 `advance` 分类把「零步长」「到端」「正常推进」分开，并给出三条后续路线的判别量。

## 一、假设与判别（预注册）

| 编号 | 假设 | 可否证读数 |
| --- | --- | --- |
| H-D1 | v6 的冻结是零步长：`time (+0xF4)` 与 `localTime (+0xF0)` 同时不动、`at-end=0` | `advance=frozen-no-step` |
| H-D2 | 冻结来自到端停写：`time` 推进、`localTime` 不动、`at-end=1` | `advance=frozen-at-end` |
| H-D3 | 时钟其实在推进，只是读到的实例不是采样实例 | `advance=advancing` 且 `clip` 指针与 v6 不同 / `ctl-local-time-bits` 与 `time-bits` 关系异常 |
| H-W1 | 权重斜坡（M1）在冻结期内变化 | `ctl-weight-bits` 或 `ctl-track-weights` 在冻结帧之间变化 |
| H-W2 | 权重恒定，差异来自采样后写入者（M2/M3） | `ctl-weight-bits`、`ctl-track-weights` 全程恒定 |

任一假设被否证，按 §六分叉处理；不靠同一探针重跑「碰」结果。

## 二、采集范围与行格式

- 窗口：`equip` 第 1–13 步、`unequip` 第 1–12 步的 `pre-sample` 与 `post-sample`，共 **50** 个观测点（比 v6 多出 unequip 的 24 点）。
- 键：`[Experiment] TransitionClipClockProbe=false`（默认关）；要求 `TransitionStateProbe=true` 且 `K5FullSample=true`，仅 AE 1.6.1170 且仅候选侧。任一不满足时输出一行 `TRANSITION CLIP CLOCK status=inactive ...`。
- 身份：按**动画族**选种（equip → `1HM_Equip.hkx`、unequip → `1HM_Unequip.hkx`），要求活动节点中恰好一个匹配；0 个记 `missing-seed`，多个记 `ambiguous-seed`。
- 行：

```text
TRANSITION CLIP CLOCK side=candidate segment=equip phase=pre-sample graph=0x.. behavior=0x.. actor=00000014 request=1 commanded=1 completed=0 clip=0x.. animation='Animations\1HM_Equip.hkx' expected='1HM_Equip.hkx' binding-index=22 mode=0 user-controlled=false at-end=0 local-time-bits=3C888889 time-bits=00000000 time-delta=0 advance=unknown control=0x.. ctl-local-time-bits=3C888889 ctl-weight-bits=3F800000 ctl-track-weights=0 ctl-speed-bits=3F800000 ctl-crop-start-bits=00000000 ctl-crop-end-bits=00000000 clip-speed-bits=3F800000 clip-crop-start-bits=00000000 clip-crop-end-bits=00000000 status=ok
```

- `advance` 由同一 `(segment, phase)` 的上一次采样判定；新 `request` 清空该表。`time-delta` 为两次 `time` 的浮点差。
- 只读：所有读取经 `ReadProcessMemory` + `plausible_object_pointer`；不调用图函数、不写图/姿态/变量、不新增 detour；`TransitionProbeRecord`/CPK5/CPK5S/源 ring 全部不变。

## 三、判据（不得事后放宽）

| 编号 | 判据 | 通过条件 | 否则 |
| --- | --- | --- | --- |
| CC-1 身份/环境 | v116 三件套/存档/`AE-Track2`；K5 249/249；747 header `flags=0`；零非豁免 `[E]` | 全满足 | INVALID |
| CC-2 仪器成立 | 50 行全部 `status=ok`；`animation` 族与段一致；`control` 非零且 `ctl-*` 可读 | 全满足 | 观测缺口 |
| CC-3 时钟判别 | equip 2–13 的 `advance` 一致命中 H-D1/H-D2/H-D3 之一，不混用 | 单一分支 | 需先诊断混用原因 |
| CC-4 权重判别 | 冻结帧之间 `ctl-weight-bits`/`ctl-track-weights` 的变/不变给出 H-W1 或 H-W2 | 全满足 | — |
| CC-5 无副作用 | 无新 detour/写入；K5 账本与 v115 逐项相同；`source-drift=none` | 全满足 | INVALID |

## 四、上机步骤

1. 关闭 Skyrim/SKSE；MO2 选 `AE-Track2`（OAR 保持停用）。v115 三件套已备份到 `dist/deploy-backup-20261120-v115-v116pre/`，v116 三件套已装入 CharacterPanel 模组。
2. 确认 `IndependentGraph=inject`、`CandidateEquipPrime=true`、`CandidateEquipPrimeBranch=true`、`CandidateInputProbe=true`、`CandidateUnequipProbe=true`、`CandidateUnequipEvent=Unequip`、`K5FullSample=true`、`TransitionStateProbe=true`、`TransitionClipClockProbe=true`；`TransitionParentChainProbe=false`、`TransitionSubgraphProbe=false`、`SamplingLiveness=off`。
3. 载入 Save116 → 打开一次背包（候选回放自动跑 equip 120 / unequip 129）→ 等 K5 写入 → 关闭面板并退出。每次重跑用新的 Skyrim 进程。
4. 保留完整 `CharacterPanel.log` 与同 session 的 `CharacterPanel-g5k5-*.bin`、`.json`。

## 五、构建、部署与实现记录

**实现（提交 `c7ec354`）**

| 文件 | 变更 |
| --- | --- |
| `src/animation/behavior_graph/transition_clip_clock_policy.h` | 新增：时钟字段偏移表、`ClipClockAdvance` 分类器、模式判定（纯函数） |
| `src/animation/behavior_graph/transition_probe.h/.cpp` | 新增 `log_transition_clip_clock`：按动画族选种、读 clip/control 时钟与权重、`advance` 分类、50 点窗口 |
| `src/animation/settings.h/.cpp` | 新键 `TransitionClipClockProbe` 解析、日志行、getter；默认 false |
| `src/animation/behavior_graph/independent_graph.cpp` | 在父链探针之后的条件调用 + `status=inactive` 提示 |
| `tests/transition_probe_test.cpp` | 新增 2 例（偏移表、advance 分类与名字）；总计 12/12 |
| `config/CharacterPanel.ini` | 样例键 `TransitionClipClockProbe=false` + 注释 |

**离线测试**：`cl /nologo /std:c++latest /EHsc /W4 /WX /utf-8 /I src tests\transition_probe_test.cpp` 编译并运行，**12/12、exit 0**。

**构建（2026-11-20 23:22）**：干净树（`c7ec354`）用 `cmake --preset Release -DCHARACTER_PANEL_BUILD_LABEL=v116-g5trans-clip-clock-20261120` + `cmake --build --preset Release`，VS 2022 v143、vcpkg `x64-windows-static-md`，exit 0；身份 `CharacterPanel-scenegraph-v116-g5trans-clip-clock-20261120-c7ec35441f-cl94faaed0c6-20261120T152153Z`（无 `+dirty`）。

| 工件 | 字节 | SHA-256 |
| --- | ---: | --- |
| CharacterPanel.dll | 2,120,704 | `43654AEDAFF9764D41EE872D451B0B48E24AAAC2C177985CE996ABA3E2BA76AE` |
| CharacterPanel.pdb | 52,695,040 | `5370FDFEDF47C760C63899097BF2E0F8A0B06B8A2C86AB93AB603DF7DA68DB30` |
| 候选 INI | 17,315 | `C9902A42D61EB66806D4C6C396DE853C9571B4160678FDBE2CBF618AC6121EEC` |
| 运行包 ZIP `CharacterPanel-v116-AE-Track2.zip` | 14,375,877 | `AFDEC0A39709F0E48B5DDDD07B6AB95F51D4431E289FFB3700CF198DEFF6F239` |

**部署（2026-11-20 23:22；Skyrim/SKSE 未运行，MO2 打开）**：安装位 v115 三件套先备份到 `dist/deploy-backup-20261120-v115-v116pre/`（DLL 2,113,024 B `5484030d…`、PDB 52,613,120 B `b1071fdd…`、候选 INI 17,121 B `df18e107…`，逐位核对）。v116 DLL/PDB + 候选档装入 `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\`，安装位哈希 `43654aed…`/`5370fdfe…`/`c9902a42…`；DLL 内身份 `…-v116-g5trans-clip-clock-20261120-c7ec35441f-…` 与 `TRANSITION CLIP CLOCK` 模板在位。**未启动游戏**。

## 六、判读与分叉

1. **H-D1 成立**（`frozen-no-step`）：clip 确实没拿到步长。接着看 H-W1/H-W2：权重变 → 转 M1（下一轮读 `control+0x18` 逐轨权重内容，只读）；权重不变 → 转 M2/M3（接既有 E2 探针定位采样后写入者）。
2. **H-D2 成立**（`frozen-at-end`）：到端停写是既有语义，冻结本身不是缺陷；同样按 H-W1/H-W2 分流。
3. **H-D3 成立**（`advancing`）：v6 的 `localTime` 冻结与外层读数矛盾，先核对 `clip` 指针与活动节点表是否同一实例，再决定是否修 v6 判据。
4. 任何 `status=no-control`、`missing-seed`、`ambiguous-seed`、`overflow` 或 `[E]` 都判观测缺口，保留原始日志，不静默截断。

## 七、边界

- 单次运行、单存档、单装备状态；不含源侧。
- 不改变 `G4P-7 FAIL`、G5/G6 与任何 `tau_*`；不据此修改姿态判据。
- 只对 AE 1.6.1170 的这批字段负责；偏移表见 [H-C1 审计](../g5-clip-clock-20261120/README.md#二字段与代码核对)。
- 不外推到其他装备族、运行时或存档。
