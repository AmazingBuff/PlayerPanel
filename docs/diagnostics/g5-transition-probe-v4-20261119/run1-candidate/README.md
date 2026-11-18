# C-G5-TRANS v4 候选 run 1（2026-11-19 00:35–00:37，v113）

**结论：modifier 绑定路线判负。** K5 链 249/249、equip 120、unequip 129、`state-12-to-0`；747 header 全 `flags=0x0000`；2,379 条 `TRANSITION MODIFIER`、537 条 `BIND`、747 条 summary（overflow 全 false）。`BodyWeightBlendModifier`/`MT_Default_MG` 只在 cmd=1 active，cmd≥2 的 active modifier 只有 `RootModifierList`/`bAttachedModifierList`，**全部 bindings=0**；唯一的 BIND word（`TurnDelta`、`Speed`）在 equip cmd 1–13 恒定。H-M1 未目击：modifier 绑定变量不是 0→10 组的标量。

## 一、身份与工件

| 项 | 读数 |
| --- | --- |
| 构建身份 | `CharacterPanel-scenegraph-v113-g5trans-probe-v4-20261119-114f877e8d-cl94faaed0c6-20261118T161425Z` |
| 运行档 | `CharacterPanel-g5trans-candidate.ini` 12,419 B `7DEB21E5…D06531` |
| 日志 | [CharacterPanel.log](CharacterPanel.log) 4,570,261 B `6AEB793C…79CF93` |
| K5 bin | [CharacterPanel-g5k5-candidate.bin](CharacterPanel-g5k5-candidate.bin) 1,410,452 B `56E89AAE…486FB3` |
| K5 JSON | [CharacterPanel-g5k5-candidate.json](CharacterPanel-g5k5-candidate.json) 791 B `EA2F2512…067CBBC` |

## 二、K5 链与探针

- K5 249/249；equip 120/120 `fixed-equip-budget`、unequip 129/129 `state-12-to-0`；`identity-failed=1`（SCAR dummy 已知记账）。
- 747 header（3 phase × 249）全 `flags=0x0000`；SM 6,780；EFFECT 78（26 个 transition）；BLENDER 813；AUX 2,241；CENSUS 1,276。
- `TRANSITION MODIFIER` 2,379；`BIND` 537；`SUMMARY` 747，overflow 全 `false`。

## 三、modifier 观测

### 3.1 equip

- cmd=1：4 个 active modifier——`WeapEquipModifierGenerator`（`bAttachedModifierList`，behavior-reference，bindings=0）、`MT_Default_MG`（`BeginShout_OnFalseToTrueMod`，state-machine，bindings=1）、`BodyWeightBlendModifier`（`BodyMorphWeightExpressionModifier`，blender，bindings=1）、`RootModifierGenerator`（`RootModifierList`，manual-selector，bindings=0）。
- cmd=2–13：只剩 `RootModifierList`（bindings=0）与 `bAttachedModifierList`（bindings=0）。
- BIND word（equip cmd 1–13，全 phase）：`TurnDelta`=5.0 恒定（仅在 unequip 窗口的 `1HM_Unequip_ModifierGenerator`/`Weap_UnequipModifierGenerator` 出现，equip 无）；`Speed`=1.0 恒定（cmd=1，`BodyMorphWeightExpressionModifier`）；index 19 的匿名 bool 恒 0。**没有任何 modifier 绑定 word 随 cmd 变化。**

### 3.2 unequip

`1HM_Unequip_ModifierGenerator`/`Weap_UnequipModifierGenerator` 的 `TurnDelta` 绑定（mode 1）在 unequip 窗口 387 条；word=5.0 恒定——这是 `TurnDelta` 的运行值，不是过渡权重。

## 四、判据

| 判据 | 裁定 |
| --- | --- |
| TP-1 身份/环境 | **PASS（Track 2 豁免内）** |
| TP-2 记录配对 | **PASS**：249 K5、747 header、flags 全 0、summary overflow 全 false |
| TP-3 0→10 目击 | **PASS**（与 v3 相同：cmd=1 parent blending transition） |
| TP-4 计时推进 | **PASS（10→1）**：cmd 87–98 time-in 0.0→0.183333；0→10 只有起点 |
| TP-5 Blender 目击 | **NOT OBSERVED**（同 v3） |
| TP-6 相关性 | **10→1 复现**：12 点 PC1 99.8297%、Pearson 0.992817、单调 |
| TP-7 无副作用 | **PASS** |
| TP-8 modifier 观测资格 | **PASS（仪器成立）**：行/summary/BIND 均按 schema 输出 |
| TP-9 / H-M1 | **判负**：无 modifier 绑定 word 随 0→10 组变化；活跃 modifier 在 cmd≥2 全 bindings=0 |

## 五、结论与 v5 入口

1. **modifier 绑定变量排除**：`BodyWeightBlendModifier`（`BodyMorphWeightExpressionModifier`）、`MT_Default_MG`、`RootModifierList`、`bAttachedModifierList` 的 graph-variable 绑定都不承载 0→10 的逐帧标量；`TurnDelta`/`Speed` 与过渡无关。
2. **0→10 的标量不在：**parent `activeTransitions`（v3）、`delayedTransitions`/flags（v3 AUX）、active `hkbBlenderGenerator` 字段（v109/v2）、active modifier graph-variable 绑定（v4）。剩余假设域：
   - `WeapEquipBFR` 引用的 `WeapEquip.hkb` 子图内部（activeNodes 只显示 `WeapEquipBFR` 一行，其子图节点不在 active 列表）；
   - `hkbBehaviorGraph::allPartitionMemory` / `nodePartitionInfo`（v109 的下一入口，尚未查）；
   - `hkbBlendingBehavior`（无 CommonLib 布局，K5-5 缺项清单）。其中 `State3`/`Weap_EquipDefaultState` 极可能在 `WeapEquip.hkb` 的子图里。
3. **v5 建议方向（需另立预注册）**：审计 `hkbBehaviorReferenceGenerator` 的运行时 behavior 指针布局（HKX2 `behavior +0x50`）并在 guarded 读取下枚举 `WeapEquip.hkb` 子图的 activeNodes/blender；或读 `allPartitionMemory +0x118`。每一步都先做离线布局审计，再上运行时探针；不猜偏移。
4. **`10→1` 标量**已两次复现（PC1 99.83%、Pearson 0.992817），可作为后续配对协议锚点；仍不是 G5 通过。

**边界**：G4P-7 仍 FAIL；G5/G6 未改变；无 `tau_*`；TP-5 未目击。
