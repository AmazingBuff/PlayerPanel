# C-G5-TRANS v2 候选侧 run 2（2026-11-18 23:09–23:15）

**结论：v2 首次采到 active blending transition。** 候选 K5 链 249/249、equip 120、unequip 129、`state-12-to-0`；747 条探针 header（3 phase × 249）、78 条 EFFECT（26 个 transition 实例）、6,672 条 SM、813 条 BLENDER。`Default_Behavior` 的 `0→10` 在 cmd=1 被采到（`hkbBlendingTransitionEffect`，duration 0.2 s，time-in 0.016667），`10→1` 在 cmd=87–98 完整推进到 0.183333（= duration − 1/60）；该 12 点姿态 PC1 占 99.83%、与 time-in 的 Pearson 0.992817。但 72 条 header 仍 `flags=0x0002`（settle/unequip 的 SM >16），且 census 证明 `State3`/`Weap_EquipDefaultState` 不在 activeNodes，TP-5 未目击。

## 一、身份与工件

| 项 | 读数 |
| --- | --- |
| 构建身份 | `CharacterPanel-scenegraph-v111-g5trans-probe-v2-20261118-15f75ac27b-cl94faaed0c6-20261118T150516Z` |
| 运行档 | `CharacterPanel-g5trans-candidate.ini` 12,419 B `7DEB21E5…D06531` |
| 日志 | [CharacterPanel.log](CharacterPanel.log) 2,750,057 B `3AABAC43…BB587F` |
| K5 bin | [CharacterPanel-g5k5-candidate.bin](CharacterPanel-g5k5-candidate.bin) 1,410,452 B `BB5B8B68…FA31FB` |
| K5 JSON | [CharacterPanel-g5k5-candidate.json](CharacterPanel-g5k5-candidate.json) 791 B `02F6BF5B…E4C234D` |

## 二、K5 链（有效）

- `K5 SNAPSHOT close records=249 attempted=249 overflow=false write-failed=no`。
- `K5 RECON expected=249 attempted=249 recorded=249 missing=0 truncated=0 two-read-bad=0 clip-overflow=0 identity-failed=1 drift-records=1 dropped=0`；equip 120/120 `fixed-equip-budget`、unequip 129/129 `state-12-to-0`。
- `identity-failed=1` 仍为 completed=87 的 `SCAR_1hmReadyDummy`（`data_fnv=0`），与门 5 同形，属 Track 2 已知 dummy 记账。

## 三、探针计数

| 行 | 数量 |
| --- | ---: |
| `TRANSITION PROBE side=candidate` | **747**（equip 120 + unequip 129，各 3 phase） |
| `SM` | 6,672 |
| `EFFECT` | **78**（26 个 transition 实例 × 3 phase） |
| `BLENDER` / `CHILD` / `INTERNAL` | 813 / 1,644 / 813 |
| `TRANSITION CENSUS` | 1,276（equip cmd 1–13） |
| header flags | `0x0000` × 675；`0x0002` × 72 |

## 四、过渡观测

### 4.1 `0→10`（equip）

`Default_Behavior`（sm-index=12）在 cmd=1：`state=10 transitions=1`；effect 为 `hkbBlendingTransitionEffect`，`duration=0.2`、`time-in=0.016667`、`time-remaining=0.183333`、`blend-curve=0`、`self=2`。三个 phase 各一条；cmd≥2 后该 state machine 的 `transitions=0`，`activeTransitions` 不再包含该 effect。

**TP-3 PASS**：`0→10` 边确实由 blending transition 驱动。**TP-4 只得到起点**（0.016667），未采到 0.2 s 的完整推进；`0.208333` 平台仍不能用本数据完整解释。

### 4.2 `10→1`（equip settle）

`Default_Behavior`（sm-index=14）在 cmd=87–98：`state=1 transitions=1`；effect `duration=0.2`，`time-in` 从 `0.0` 单调推进到 `0.183333`（`duration − 1/60`）。**TP-4 对该边 PASS**。

### 4.3 unequip

- `0→12`（sm-index=15）在 cmd=1–12：`state=12 transitions=1`；time-in 前 9 步为 0，随后 0.016667/0.033333/0.050000；只到 0.05，未到 0.2。
- `12→0`（sm-index=13）只在 cmd=129 出现，time-in=0；仅完成边界的一帧。

### 4.4 TP-6：`10→1` 的标量与姿态（离线）

对 equip completed 87–98 的 12 条 CPK5 记录（116×10 维）做 PCA，并与同一记录的 `timeInTransition`（0.0→0.183333）相关：

- PC1 方差占比 **0.998297**；PC1 投影随 completed **严格单调上升**；
- PC1 投影 vs `timeInTransition` 的 Pearson **0.992817**；
- 12 个点全部来自同一 `Default_Behavior` state=1 transition，`duration=0.2`。

这是描述性结果，不是门 5 判据；它支持“过渡计时是单个标量轴”的模型，但只覆盖 `10→1` settle 边。`0→10` 只有 cmd=1 一个点，不能做同类相关。

## 五、Overflow 与 TP-2

- 72 条 `flags=0x0002`：equip cmd 87–98（36 条，3 phase）与 unequip cmd 1–12（36 条）。两条窗口正是 `10→1` / `0→12` transition 活跃期；active SM 数超过 16。
- **equip cmd 1–13 全部 `flags=0x0000`**，目标 `0→10` 窗口没有 overflow。
- 预注册要求整批零 overflow，因此 **TP-2 仍 FAIL**；不能把 settle/unequip 的 overflow 静默裁掉。

## 六、Census 与 TP-5

equip cmd 1–13 的 1,276 条 census 中**没有 `State3`/`Weap_EquipDefaultState`**。active equipment 节点为：

- `Weap_Equip_MSG`（manual-selector）
- `Weap_EquipBehavior` / `WeapEquipBehavior` / `WeapEquipRoot`（state-machine）
- `WeapEquip.hkb` / `WeapEquipBFR` / `WeapEquipModifierGenerator`（unknown vtable：`0x7ff654191450` / `0x7ff6541aea20` / `0x7ff6541af1f8`）
- `MT_Default_Behavior` / `Default_Behavior`（state-machine）

active `hkbBlenderGenerator` 只有 `BodyWeightBlend`、`ArmsBlend`、`Weap_UnequipBlend`、`1stP_MLh_1HM_Idle_Blend`、`SCAR_1HM_Ready_Blend`、`Weap_Readied_Lean_Blend`；**没有 `State3`**。因此 **TP-5 NOT OBSERVED**：`State3` 不是 activeNodes 的直接成员，不能继续用该路径读它的 `blendParameter`/`endWeight`。

## 七、判据汇总

| 判据 | 裁定 |
| --- | --- |
| TP-1 身份/环境 | **PASS（Track 2 豁免内）**：AE-Track2、OAR 关闭为设计、唯一 OAR `[E]` 豁免、dummy identity 记账与门 5 同形 |
| TP-2 记录配对 | **FAIL**：747 header 配对成立，但 72 条 SM overflow |
| TP-3 0→10 目击 | **PASS**：cmd=1 `Default_Behavior state=10` + blending transition |
| TP-4 计时推进 | **PARTIAL/PASS**：`10→1` 完整到 0.183333；`0→10` 只有起点；unequip `0→12` 只到 0.05 |
| TP-5 Blender 目击 | **NOT OBSERVED**：census 无 `State3`；active Blender 无装备 State3 |
| TP-6 相关性 | **描述性强相关（10→1）**：12 点 PC1 99.83%、投影单调、Pearson 0.992817；0→10 仅 1 点未计算 |
| TP-7 无副作用 | **PASS**：零崩溃、`source-drift=none`、无图/姿态/变量写入 |

**本批不能判 G5**；G4P-7 仍 FAIL，不产生 `tau_*`。

## 八、v3 入口

1. **overflow**：SM/effect 上限 16→**24/32**；仍保留 overflow 即 INVALID，不静默截断。
2. **日志可读性**：per-line `SM/EFFECT/BLENDER` 增加 `segment=`；当前只能靠 header 顺序消歧，分析成本高。
3. **0→10 推进**：`activeTransitions` 在 cmd≥2 为空，但 cmd=1 确证 duration 0.2。v3 需读 `hkbStateMachine::transitionFlags(+0xC0)`、`delayedTransitions(+0xE0)`、`wildcardTransitionFlags(+0xD0)`，或对 `MT_Default_Behavior`/目标状态机做同点观察，确认 0→10 是“已移出 activeTransitions”还是“0.208333 平台来自目标 clip”。
4. **TP-5**：`State3` 不在 activeNodes；改查 `WeapEquip.hkb`/`WeapEquipBFR` 的引用图、父链/`activeNodesChildrenIndices` 或 `allPartitionMemory`；对 unknown vtable 只登记，不猜偏移。
5. **TP-6**：先用 `10→1` 的 12 个 time-in 点与 settle 组 PC1 做离线相关，确认该标量是否解释姿态；0→10 起点不足，暂不计算。

