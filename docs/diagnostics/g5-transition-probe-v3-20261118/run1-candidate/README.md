# C-G5-TRANS v3 候选 run 1（2026-11-18 23:49–23:54，v112）

**结论：v3 修复了 overflow，并复现了 `10→1` 的强标量相关。** 747 条 header 全部 `flags=0x0000`；K5 249/249；`0→10` 在 cmd=1 仍为 blending transition（duration 0.2、time-in 0.016667）；`10→1` 在 cmd=87–98 到 0.183333，12 点姿态 PC1 99.83%、Pearson 0.992817。AUX 的 `transitionFlags`/`wildcard-flags`/`delayed` 在 cmd 1–13 恒定，`delayed` size=0；`State3` 仍不在 activeNodes，TP-5 未目击。

## 一、身份与工件

| 项 | 读数 |
| --- | --- |
| 构建身份 | `CharacterPanel-scenegraph-v112-g5trans-probe-v3-20261118-3e273e68e6-cl94faaed0c6-20261118T154505Z` |
| 运行档 | `CharacterPanel-g5trans-candidate.ini` 12,419 B `7DEB21E5…D06531` |
| 日志 | [CharacterPanel.log](CharacterPanel.log) 3,376,673 B `EEC842FE…B849C` |
| K5 bin | [CharacterPanel-g5k5-candidate.bin](CharacterPanel-g5k5-candidate.bin) 1,410,452 B `967F5AED…0ED631B` |
| K5 JSON | [CharacterPanel-g5k5-candidate.json](CharacterPanel-g5k5-candidate.json) 791 B `4E727856…ED198721` |

## 二、K5 链

- 249/249；equip 120/120 `fixed-equip-budget`、unequip 129/129 `state-12-to-0`；`identity-failed=1` 为 completed=87 的 `SCAR_1hmReadyDummy` 已知记账。
- `K5 SNAPSHOT close records=249 attempted=249 overflow=false write-failed=no`。

## 三、探针计数

| 行 | 数量 |
| --- | ---: |
| `TRANSITION PROBE side=candidate` | **747**（3 phase × 249） |
| `SM` | 6,780 |
| `EFFECT` | **78**（26 个 transition 实例） |
| `BLENDER` / `CHILD` / `INTERNAL` | 813 / 1,644 / 813 |
| `TRANSITION AUX` | **2,241** |
| `TRANSITION CENSUS` | 1,276 |
| header flags | **`0x0000` × 747（overflow 0）** |

## 四、过渡观测

### 4.1 `0→10`

`Default_Behavior` state=10，cmd=1：`hkbBlendingTransitionEffect`，duration 0.2、time-in 0.016667、time-remaining 0.183333、self=2、curve 0。cmd≥2 后 `transitions=0`，`activeTransitions` 不再含该 effect。

**TP-3 PASS；TP-4 只得到起点**。`0.208333` 平台（cmd 2–12）的 11 个不同姿态不能由这条 parent transition 的 time-in 解释——该 transition 在 cmd=1 后已离开 `activeTransitions`，而 0.2 s 的起点与平台时长同量级，说明平台来自目标状态内部，而不是 parent transition 的逐帧权重。

### 4.2 `10→1`

`Default_Behavior` state=1，cmd=87–98：duration 0.2，time-in 0.0→0.183333（= duration − 1/60）。**TP-4 PASS**。

对 completed 87–98 的 12 条 CPK5 记录（116×10）做 PCA：

- PC1 方差占比 **0.998297**；投影随 completed 严格单调上升；
- PC1 vs time-in 的 Pearson **0.992817**。

**TP-6 强描述性**；与 v2 run 2 的读数逐位复现。

### 4.3 AUX

`Default_Behavior` 的 `TRANSITION AUX` 在 equip seq 0–12（cmd 1–13）恒定：

- `transition-flags`: size 4、capacity 4、head `0000E9D1`；
- `wildcard-flags`: size 7、capacity 7、head 全 0；
- `delayed`: size 0、data 0。

没有 delayed effect，也没有随 cmd 变化的 flags/head。**AUX 未能解释 0→10 的逐帧推进**；它只证明 0→10 不在 `delayedTransitions`，且 flags 数组在窗口内不变。

## 五、Census 与 TP-5

- `Default_Behavior` 在 cmd 1–13 有 `weap` 子图节点：`Weap_Equip_MSG`（manual-selector）、`Weap_EquipBehavior`/`WeapEquipBehavior`/`WeapEquipRoot`（state-machine）、`WeapEquipModifierGenerator`/`MT_Default_MG`/`BodyWeightBlendModifier`/`RootModifierGenerator`（modifier-generator）、`WeapEquipBFR`/`MT_BehaviorBFR`（behavior-reference）。
- **仍无 `State3`/`Weap_EquipDefaultState`**；active Blender 只有 `BodyWeightBlend`、`ArmsBlend`、`Weap_UnequipBlend`、`1stP_MLh_1HM_Idle_Blend`、`SCAR_1HM_Ready_Blend`、`Weap_Readied_Lean_Blend`。
- **TP-5 NOT OBSERVED**：`State3` 不是 activeNodes 直接成员；v3 的 behavior-reference 分类只改进了 census，没有遍历 `WeapEquipBFR` 的引用图，也没有读其未审计的 behavior 指针。

## 六、判据汇总

| 判据 | 裁定 |
| --- | --- |
| TP-1 身份/环境 | **PASS（Track 2 豁免内）** |
| TP-2 记录配对 | **PASS**：747 header / 249 K5 配对；overflow=0 |
| TP-3 0→10 目击 | **PASS**：cmd=1 parent blending transition |
| TP-4 计时推进 | **PASS（10→1）**；0→10 只有起点 |
| TP-5 Blender 目击 | **NOT OBSERVED**：无 State3；active Blender 无装备 State3 |
| TP-6 相关性 | **强描述性（10→1）**：PC1 99.83%、Pearson 0.992817 |
| TP-7 无副作用 | **PASS** |

## 七、v4 入口

1. **0→10 的目标状态内部标量**：parent transition 在 cmd=1 后离开 `activeTransitions`；0.208333 平台的 11 个姿态变化应来自目标状态（`MT_Default_MG`/`RootModifierGenerator`/`BodyWeightBlendModifier`/`WeapEquipBFR`）内部的 blend/weight。v4 应优先在 `Default_Behavior` 的 **目标状态** `MT_Default_Behavior`/`Master_Behavior` 或 `WeapEquipBFR` 的引用图上做有界状态 census，而不是继续读 parent `activeTransitions`。
2. **TP-5**：`State3` 不在 activeNodes；下一步是离线/运行时审计 `hkbBehaviorReferenceGenerator` 的 behavior 指针或 `WeapEquip.hkb` 的引用图，或 `allPartitionMemory`/父链；未审计偏移不得读。
3. **AUX** 已证明 0→10 不在 `delayedTransitions`，flags 不变；v4 不需要重复相同 AUX。
4. **10→1** 的标量相关已复现；可作为后续配对协议的锚点，但仍不是 G5 通过。

**边界**：G4P-7 仍 FAIL；G5/G6 未改变；无 `tau_*`。
