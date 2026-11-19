# v6-A 离线布局审计目标（AE 1.6.1170）

目标：从 `WeapEquipRoot` 的 `currentStateID=3` 找到该 state 的 generator，再沿
state machine / ManualSelector 链定位 `State3` Blender。先完成本页的离线审计，
再写运行时探针；在此之前不读 `StateInfo` 未知偏移。

## 一、运行时身份（v114 run 1）

| 对象 | id | 运行时 state | 备注 |
| --- | ---: | ---: | --- |
| `WeapEquipRoot` | 28145 | 3 | 主图 activeNodes index 6；equip 1–13 恒为 3 |
| `WeapEquipBehavior` | 28359 | 0 | 主图 index 5 |
| `Weap_EquipBehavior` | 28761 | 0 | 主图 index 4 |
| `Weap_Equip_MSG` | 29346 | — | manual-selector，index 3 |
| `WeapEquip.hkb` | 28056 | — | `hkbBehaviorGraph`，index 7 |
| `WeapEquipBFR` | 14169 | — | behavior-reference，index 8；v5 已判负 |

候选链（需离线/运行时确认）：

```text
WeapEquipRoot (state 3)
  -> WeapEquipBehavior (state 0)
  -> Weap_EquipBehavior (state 0)
  -> Weap_Equip_MSG (manual selector)
  -> ... -> State3 (hkbBlenderGenerator)
```

## 二、作者树锚点

`weapequip_weap_equip_subtree.txt`：

- `root/WeapEquip`：`hkbStateMachine`，`states=2 startStateId=0`。
- `root/WeapEquip/WeapEquip_StandingState`：`hkbStateMachine`，`states=2 startStateId=0`。
- `root/WeapEquip/WeapEquip_StandingState/Weap_EquipDefaultState`：`hkbManualSelectorGenerator`，`n=13`，`selectedGeneratorIndex` 绑定 `var[12]=iRightHandType`。
- 其 `msg`：第二级 `hkbManualSelectorGenerator`，`n=12`，绑定 `var[13]=iLeftHandType`。
- `msg/State3`：`hkbBlenderGenerator`，`children=2`；child 0 为 `2HC_Equip.hkx`，child 1 为 `State3/b` 状态机（`startStateId` 绑定 `iLeftHandEquipped_DG`；State0=`1HM_Equip.hkx`）。

`0_master_default_state_subtree.txt`：

- `Default_State`：`states=9 wildcard=7`；`MT_Behavior_State` 的 `0→10` 为 `WeapEquip` 事件、0.2 s `hkbBlendingTransitionEffect`。
- `Weap_Unequip_State` 的 `12→0`、`1→12` 和 `10→1` 上下文在子树内。

## 三、引擎布局目标

| 类 | vtable RVA | AE ID | RTTI | 已知/待审计 |
| --- | ---: | ---: | --- | --- |
| `hkbStateMachine` | `0x1964250` | 226812 | `.?AVhkbStateMachine@@` | size `0x108`；`currentStateID +0x80`；`states +0x90` 是 `hkArray<StateInfo*>` |
| `hkbStateMachine::StateInfo` | `0x19633C8` | 226706 | `.?AVStateInfo@hkbStateMachine@@` | size `0x78`；基类 `hkbBindable` 到 `0x30`；`0x30..0x70` 当前在 CommonLibSSE 中为 unknown |
| `hkbManualSelectorGenerator` | `0x195EF88` | 226313 | `.?AVhkbManualSelectorGenerator@@` | CommonLibSSE 无类头；需审计 `selectedGeneratorIndex` 与 generators 数组 |
| `hkbBlenderGenerator` | `0x195EB70` | 226277 | `.?AVhkbBlenderGenerator@@` | 既有字段 `blendParameter +0x4C`、`endIntervalWeight +0x90`、`numActiveChildren +0x94`、begin/end `+0x98/+0x9A`、children `+0x60` |

## 四、必须回答的问题

1. `StateInfo` 的 `stateId` 偏移与类型（int32？）是什么？
2. `StateInfo` 的 generator 指针偏移是什么？它指向 `hkbGenerator` 的哪个子类？
3. `hkbStateMachine::states` 的 `hkArray` data/size/capacity 是否就是 `+0x90/+0x98/+0x9C`？元素是否恒为 `StateInfo*`（0x78 B）？
4. `hkbManualSelectorGenerator` 的 `selectedGeneratorIndex` 与 generators 数组偏移/步长是什么？
5. state 3 的 generator 是 state machine、manual selector 还是 blender？若为 state machine，下一层 state id 是什么？
6. 运行时 `StateInfo` 是否与模板共享，还是每个 clone 一份？是否需要 `nodeTemplateToCloneMap`？

## 五、建议的离线命令

`analysis.json` 已给出 ID→RVA；`pe_tools.Image.rtti_class()` 可核名。`disasm_func.py`
可反汇编 `.pdata` 函数。先从 `hkbStateMachine` 的 `states` 访问函数或 `StateInfo`
构造/克隆路径定位 `stateId`/generator 的写入点，再用 `StateInfo` vtable `0x19633C8`
的构造引用交叉验证。

```powershell
python build/g1-audit-20261023/disasm_func.py 0xab1780  # hkbStateMachine Activate (example)
python build/g1-audit-20261023/disasm_func.py 0xab1890  # hkbStateMachine Update (example)
```

具体函数地址在审计时按 `analysis.json` 的 `VTABLE_hkbStateMachine` 槽位解析；不要
把示例地址当作结论。

## 六、边界

- 只适用于 AE 1.6.1170；不外推 SE/VR。
- 本页是审计计划，不是布局结论；未确认前不得在运行时读 `StateInfo` 未知偏移。
- 不改变 `G4P-7 FAIL` / G5/G6。
