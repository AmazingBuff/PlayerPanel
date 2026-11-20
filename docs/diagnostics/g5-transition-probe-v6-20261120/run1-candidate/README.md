# C-G5-TRANS v6 候选 run 1（2026-11-20 22:59，v115）

**结论：父链资格 PASS，内层 `State3` 未进入活动链（契约分支二）。** 26/26 观测点 `status=ok`，父链从 `Master_Behavior` 闭合到 `DG_1hmEquip`（`roots=1`、`unresolved=0`、零普通 Blender、零 overflow）；`State3` 在整个日志中出现 0 次。同一窗口里 clip `local-time` 冻结在 `0x3E555556`、链上无 Blender/过渡效果/活动过渡，但 K5 的 11 帧姿态互不相同（11/11 唯一，86/116 轨变化，最大单轨 3.0968），因此**该链不产生逐帧姿态差异**，v6 §七.2/§七.3 的下一层（采样相位/同步/输出混合）成立。`G4P-7` 仍 **FAIL**，无 `tau_*`。

## 一、身份与工件

| 项 | 读数 |
| --- | --- |
| 构建身份 | `CharacterPanel-scenegraph-v115-g5trans-parent-chain-20261120-087613006d-cl94faaed0c6-20261120T145507Z` |
| 运行档 | 候选 INI 17,121 B `DF18E107…F646981`（`TransitionParentChainProbe=true`、`TransitionSubgraphProbe=false`） |
| 运行窗口 | 22:59:50.014 panel open → 23:01:25.138 panel close；K5 写入 22:59:50.033–22:59:50.138（会话 22:58:01–23:01:25） |
| MO2 | `AE-Track2`；`+CharacterPanel` 启用；唯一 `[E]` 为装载期 OAR 豁免 `failed to get plugin info for OpenAnimationReplacer`（Track 2 设计） |
| 日志 | [CharacterPanel.log](CharacterPanel.log) 4,971,413 B `1035330CA1114E7FF20FA0C2B528F177C17FE7611E75CB7E6581E68EE238AEA1` |
| K5 bin | [CharacterPanel-g5k5-….bin](CharacterPanel-g5k5-CharacterPanel-scenegraph-v115-g5trans-parent-chain-20261120-087613006d-cl94faaed0c6-20261120T145507Z-20261120T145950Z.bin) 1,410,452 B `1015A3D80AFA1009CD08F6B8D75AAB397C2F67C51DB88F076299B8B6458C5A17` |
| K5 JSON | [CharacterPanel-g5k5-….json](CharacterPanel-g5k5-CharacterPanel-scenegraph-v115-g5trans-parent-chain-20261120-087613006d-cl94faaed0c6-20261120T145507Z-20261120T145950Z.json) 799 B `AD7829A7D4B4562384F500B76B5FB1E4F0C11E3B2512E7178ACA928F32376656` |

## 二、K5 与主探针

- K5 249/249；equip 120/120 `fixed-equip-budget`（`max-advances=120`、`active-state-seen=false`）、unequip 129/129 `state-12-to-0`（`max-advances=180`、`first-return-zero=129`）。
- `identity-failed=1`（SCAR dummy 已知记账）、`drift-records=1`、`missing/dropped/truncated/two-read-bad/overflow=0`，与 v114 的账本读数逐项相同。
- `TRANSITION PROBE` 747 header（3 phase × 249）全部 `flags=0x0000`；`SM` 6,780 行零 overflow，`transitions>0` 的 78 行全部属于 `Default_Behavior`。
- 15 条 `[W]` 均为 studio lights / DAC 预览提示；`source-drift=none`（`activated`、`post-drive` 两处）。

## 三、父链

### 3.1 计数

| 行 | 数量 |
| --- | ---: |
| `TRANSITION PARENT CHAIN SUMMARY` | 26 |
| `NODE` | 444 |
| `EDGE` | 418 |
| `SM` | 182 |
| `SELECTOR` | 130 |
| `CLIP` | 26 |
| `BLENDER` | **0** |
| `status=` 非 `ok` 的行 | **0** |

26 个 summary 全部 `seeds=1 roots=1 unresolved=0 empty-slots=0`，`status=ok`。

### 3.2 唯一链路（26/26 观测点相同）

```text
Master_Behavior(state Root=0)
 → RootModifierGenerator(1)
 → RootBoneSwitch_MSG(17, sel 1/13)
 → DualWieldShieldBoneRootMSG(32, sel 9/12)
 → 1st3rdPerson_MSG(108, sel 1/2)
 → RootBehaviorGraph(state Default_State=14)
 → Default_Behavior(state Weap_Equip_State=10)
   [仅装备第 1 步：DefaultBlendResetTransition(35518, time-in 0.016667)]
 → WeapEquipModifierGenerator(305)
 → WeapEquipBFR(14169)
 → WeapEquip.hkb(28056)
 → WeapEquipRoot(state WeapEquip=3)
 → WeapEquipBehavior(state WeapEquip_StandingState=0)
 → Weap_EquipBehavior(state Weap_EquipDefaultState=0)
 → Weap_Equip_MSG(29346, sel 1/13)
 → StateMachine1181(state State0=0)
 → ManualSelectorGenerator418(30892, sel 9/12)
 → DG_1hmEquip(31506, clip 'Animations\1HM_Equip.hkx', binding-index=22)
```

| 层 | class | name | id | 读出 |
| --- | --- | --- | ---: | --- |
| 1 | state-machine | `Master_Behavior` | 0 | state 0 `Root` |
| 2 | modifier-generator | `RootModifierGenerator` | 1 | 2 个子槽 |
| 3 | manual-selector | `RootBoneSwitch_MSG` | 17 | 13 generators，current 1 |
| 4 | manual-selector | `DualWieldShieldBoneRootMSG` | 32 | 12，current 9 |
| 5 | manual-selector | `1st3rdPerson_MSG` | 108 | 2，current 1 |
| 6 | state-machine | `RootBehaviorGraph` | 113 | state 14 `Default_State` |
| 7 | state-machine | `Default_Behavior` | 172 | state 10 `Weap_Equip_State` |
| — | blending-transition | `DefaultBlendResetTransition` | 35518 | 仅第 1 步；`duration=0.2`、`time-in=0.016667` |
| 8 | modifier-generator | `WeapEquipModifierGenerator` | 305 | 2 个子槽 |
| 9 | behavior-reference | `WeapEquipBFR` | 14169 | `behaviors\weapequip.hkx` 模板图（v114 已判模板） |
| 10 | unknown（图） | `WeapEquip.hkb` | 28056 | template=clone |
| 11 | state-machine | `WeapEquipRoot` | 28145 | state 3 `WeapEquip` |
| 12 | state-machine | `WeapEquipBehavior` | 28359 | state 0 `WeapEquip_StandingState` |
| 13 | state-machine | `Weap_EquipBehavior` | 28761 | state 0 `Weap_EquipDefaultState` |
| 14 | manual-selector | `Weap_Equip_MSG` | 29346 | 13，current 1 |
| 15 | state-machine | `StateMachine1181` | 30245 | state 0 `State0` |
| 16 | manual-selector | `ManualSelectorGenerator418` | 30892 | 12，current 9 |
| 17 | clip | `DG_1hmEquip` | 31506 | `1HM_Equip.hkx`，binding 22 |

所有 `SM`/`SELECTOR` 行 `on-path=true`、`match=template`；`EDGE` 418 条全部保留原始 slot，`empty-slots=0`，多父边未出现（每个节点 `count=1`，两个 modifier-generator 的 `count=2` 中修饰符槽不在路径上）。

### 3.3 逐步覆盖

| 装备步 | census 行 | active-nodes | state-machines | effects | blenders | 父链 nodes/edges |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| 1 | 114 | 113 | 15 | 1 | 2 | 18 / 17 |
| 2–12 | 97 | 96 | 7 | 0 | 0 | 17 / 16 |
| 13 | 95 | 94 | 7 | 0 | 0 | 17 / 16 |

（census 行数 = 活动节点数 + 1 条 summary 行。）

**正控**：第 1 步 census 仍见 `BodyWeightBlend`(29757) 与 `ArmsBlend`(28686) 两个普通 Blender、`BodyWeightBlendModifier`(28995) 与 `DefaultBlendResetTransition`(35518)；这两个 Blender **不在** `DG_1hmEquip` 的父链上，第 2–13 步整图 `blenders=0`。

## 四、与姿态读数的矛盾（本轮新增）

| 项 | 装备 completed 2–12（11 帧） |
| --- | --- |
| clip 身份 | 同一 clone `0x17dd1523540`、`1HM_Equip.hkx`、binding 22 |
| clip `local-time` | 全部 `0x3E555556` = 0.2083333432674408（第 1 步 `0x3C888889`、第 13 步 `0x3EC88888`） |
| 链上混合元素 | 7 个 state-machine 全 `transitions=0`（`Default_Behavior` 自 seq=2 起 `transitions=0`）、`effects=0`、`blenders=0` |
| K5 姿态 | **11/11 唯一** track-block 哈希；86/116 轨变化 >1e-4；最大单轨合并 delta 3.0968（track 43）、0.3667（4）、0.2074（3） |

读数只说明：这条链上没有任何能逐帧改变姿态的元素，而姿态确实逐帧不同；因此「报告的 clip 时间」与「实际用于采样/输出的时钟」至少有一个不是同一条链路。**不据此断言原因**。

## 五、判据

| 判据 | 裁定 |
| --- | --- |
| V6-1 身份/环境 | **PASS**：v115 身份、1.6.1170、`AE-Track2` + Save116、K5 249/249、747 header `flags=0`、唯一 `[E]` 为 Track 2 OAR 豁免 |
| V6-2 仪器成立 | **PASS**：26/26 `status=ok`，`roots=1`、`unresolved=0`，零范围错误/读取失败/截断/环 |
| V6-3 目标定位 | **NOT OBSERVED**：`State3` 未进入活动链，链上普通 Blender 计数为 0 |
| V6-4 标量推进 | **N/A**：没有可读的目标 Blender |
| V6-5 无副作用 | **PASS**：无新增 detour/写入；K5 账本与 v114 逐项相同；`source-drift=none` |
| 分叉 | **分支二**：链路完整但经过其他分支（`State0` + 两个 manual selector 直取 `DG_1hmEquip`） |

## 六、下一步（候选假设，未预注册）

1. **H-C1**：`hkbClipGenerator` 的实际采样时钟与探针读到的 `localTime` 字段不同（字段只在 activate/sync 刷新）。先做离线审计：AE 1.6.1170 里谁在 update 中写该字段、采样读哪个字段。
2. **H-C2**：该链只写部分轨，其余轨沿用上一帧姿态或由其它写入者保留（86/116 的变化分布与单轨量级已留档）。
3. **H-C3**：图输出合成发生在采样相位之外（`allPartitionMemory +0x118` / `nodePartitionInfo +0xF8`），需独立预注册。

顺序：先离线审计 H-C1（沿用 v6-A 的镜像/地址库方法），再决定是否写 v7 只读探针；不重复父链读取，不重复 BFR `+0x50`、modifier 绑定与 `0x200` 指针扫描。

## 七、边界

- 单次运行、单存档、单装备状态；源侧未运行；不含 `tau_*`；`G4P-7` 维持 **FAIL**，G5/G6 不因此改变。
- 本文只回答「装备第 1–13 步的父链是什么、`State3` 是否在链上」，不证明姿态等价、不证明 `State3` 存在与否的权重语义。
- 不外推到其他装备族、运行时或存档。
