# C-G5-TRANS v5 候选 run 1（2026-11-19 23:22–23:25，v114）

**结论：H-B1 判负。** 114/114 条 `TRANSITION SUBGRAPH` BFR 行都读到 `behavior` 的非空 `hkbBehaviorGraph` 指针和正确 vtable，但全部 `behavior-active=0`、`active-nodes=0`、`status=no-active-nodes`；0 条 `NODE`/`SM`/`BLEND`/`CHILD`。`WeapEquipBFR` 的 `behavior` 是 `behaviors\weapequip.hkx` 的模板图，不是运行时 clone；v5 的子图路线不能提供 `State3` 权重。SP-1/SP-5 PASS，SP-2 FAIL，SP-3/SP-4 NOT OBSERVED，SP-6 PASS（10→1 锚点复现）。G4P-7 仍 FAIL，无 `tau_*`。

## 一、身份与工件

| 项 | 读数 |
| --- | --- |
| 构建身份 | `CharacterPanel-scenegraph-v114-g5trans-subgraph-20261119-94003147d5-cl94faaed0c6-20261119T145712Z` |
| 运行档 | `CharacterPanel-g5trans-candidate.ini` 12,449 B `30AA9777…A33E98` |
| 运行窗口 | 23:24:26.512 panel open → 23:25:14.497 panel close；K5 写入 23:24:26.739 |
| MO2 | `AE-Track2`；`+CharacterPanel` 启用；唯一 `[E]` 为装载期 OAR 豁免 `failed to get plugin info for OpenAnimationReplacer` |
| 日志 | [CharacterPanel.log](CharacterPanel.log) 4,623,468 B `082FFBAC…90FC7` |
| K5 bin | [CharacterPanel-g5k5-candidate.bin](CharacterPanel-g5k5-candidate.bin) 1,410,452 B `FA55101C…421534` |
| K5 JSON | [CharacterPanel-g5k5-candidate.json](CharacterPanel-g5k5-candidate.json) 791 B `17D527C1…9908046` |

## 二、K5 与主探针

- K5 249/249；equip 120/120 `fixed-equip-budget`、unequip 129/129 `state-12-to-0`；`identity-failed=1`（SCAR dummy 已知记账）、`drift-records=1`、`missing/dropped/truncated/two-read-bad=0`。
- `TRANSITION PROBE` 747 header（3 phase × 249）全部 `flags=0x0000`；`walked=113 active-nodes=113 state-machines=15 effects=1 blenders=2`（cmd=1）。
- `10→1` 的 12 点姿态由 K5 bin 重算：PC1 **99.8297%**、与时间 Pearson **0.992817**，与 v2/v3/v4 逐位一致。
- `source-drift=none`；equip/unequip 的 `cand-pose-*` 哈希与主探针记录完整。

## 三、子图探针

### 3.1 计数

| 行 | 数量 |
| --- | ---: |
| `TRANSITION SUBGRAPH` BFR 行 | 114 |
| `TRANSITION SUBGRAPH SUMMARY` | 75 |
| `TRANSITION SUBGRAPH NODE` | 0 |
| `SM` / `BLEND` / `CHILD` | 0 / 0 / 0 |

BFR 行分布：`WeapEquipBFR` 39、`1HM_MagicBFR` 36、`Weap_BehaviorBFR` 36、`MT_BehaviorBFR` 3。39 = equip 1–13 × 3 phase；36 = unequip 1–12 × 3 phase；`MT_BehaviorBFR` 只在 equip cmd=1 出现 3 次。

### 3.2 读数

114/114 行均为：`behavior-active=0`、`active-nodes=0`、`active-capacity=0`、`status=no-active-nodes`；无 overflow。代表行：

```text
TRANSITION SUBGRAPH side=candidate segment=equip phase=post-update seq=0 commanded=1 index=8 bfr=0x1c63351c200 bfr-name='WeapEquipBFR' bfr-id=14169 behavior-name='behaviors\weapequip.hkx' behavior=0x1c6332f80c0 behavior-vtable=0x7ff7333e1450 behavior-active=0 active-nodes=0 active-capacity=0 status=no-active-nodes
```

| BFR | behavior-name | behavior-vtable | active-nodes | status |
| --- | --- | --- | ---: | --- |
| `WeapEquipBFR` | `behaviors\weapequip.hkx` | `0x7ff7333e1450` | 0 | `no-active-nodes` |
| `1HM_MagicBFR` | `behaviors\magicbehavior.hkx` | `0x7ff7333e1450` | 0 | `no-active-nodes` |
| `Weap_BehaviorBFR` | `Behaviors\1HM_Behavior.hkx` | `0x7ff7333e1450` | 0 | `no-active-nodes` |
| `MT_BehaviorBFR` | `Behaviors\MT_Behavior.hkx` | `0x7ff7333e1450` | 0 | `no-active-nodes` |

### 3.3 主图对照

同一窗口的 `TRANSITION CENSUS`（equip cmd=1，pre-sample）仍给出：

- `WeapEquip.hkb` index 7，node `0x1c6332f80c0`，vtable `0x7ff7333e1450`，class=`unknown`（`hkbBehaviorGraph` 未在 v3 分类表中单独命名）；
- `WeapEquipBFR` index 8，node `0x1c63351c200`，class=`behavior-reference`；
- `WeapEquipRoot` index 6，class=`state-machine`，`currentStateID=3`（equip cmd 1–13 的 pre-sample 恒为 3）；
- 主图 `activeNodes` 无 `State3` / `Weap_EquipDefaultState` Blender。

这说明：BFR 的 `behavior` 指针指向 `weapequip.hkx` 的模板图（`activeNodes` 为空），而运行时 `WeapEquipRoot` 状态机的 state 3 才是 `State3` 分支的父对象；`State3` 不是顶层 active node。

## 四、判据

| 判据 | 裁定 |
| --- | --- |
| SP-1 身份/环境 | **PASS（Track 2 豁免内）**：身份/存档/`AE-Track2`；K5 249/249；747 header `flags=0`；唯一 `[E]` 为装载期 OAR 豁免 |
| SP-2 仪器成立 | **FAIL**：BFR/behavior/vtable 均正确，但 `active-nodes=0`（114/114） |
| SP-3 目标定位 | **NOT OBSERVED**：无 `NODE`/`SM`/`BLEND`，未找到 `State3`/`Weap_EquipDefaultState` |
| SP-4 标量推进 | **NOT OBSERVED**：没有可读的 Blender 字段 |
| SP-5 无副作用 | **PASS**：无写入/新 detour；K5 249/249；`source-drift=none` |
| SP-6 相关性 | **PASS（描述性）**：`10→1` 12 点 PC1 99.8297%、Pearson 0.992817 |

## 五、结论与 v6 入口

1. **BFR 的 `behavior` 是模板图，不是运行时 clone。** 114/114 BFR 行都在 `+0x50` 读到正确的 `hkbBehaviorGraph` vtable，但 `activeNodes` 为空；这不是仪器失败，而是路线否证。
2. **`State3` 的父对象已在主图中可见。** `WeapEquipRoot` 状态机在 equip 1–13 的 `currentStateID=3`，它是下一步最直接的入口。
3. **v6 首选（A）**：离线审计 `hkbStateMachine::StateInfo` 的 `stateId`/`generator` 偏移（`hkbStateMachine::states +0x90`，`StateInfo` 0x78 B），运行时读 `states` 中 state 3 的 generator 指针，再按既有 Blender 布局读 `State3` 权重。先做离线布局审计，再写只读探针。
4. **v6 备选（B）**：`hkbBehaviorGraph::allPartitionMemory +0x118` / `nodePartitionInfo +0xF8` 的独立预注册。
5. **v6 备选（C）**：`nodeTemplateToCloneMap +0xE0` / `nodeCloneToTemplateMap +0xE8`，把 `weapequip.hkx` 模板 `State3` 映射到运行时 clone。
6. **不重复** BFR `+0x50` 路线；不重复 v109 的 `0x200` 直接指针扫描。

v6-A 的离线输入包已放在 [`../hkx/`](../hkx/README.md)：`weapequip` 的 `root/WeapEquip` 子树、`0_Master` 的 `Default_State` 子树、v114 运行时证据，以及 `StateInfo`/ManualSelector 的布局审计目标。

2026-11-20 已核实主图子索引表和上述字段。v6 先从 `DG_1hmEquip` 反查活动父边，再核对 StateInfo 与选择器，具体路径见 [离线审计结果](../hkx/stateinfo_layout_audit.md)。

**边界**：G4P-7 仍 FAIL；G5/G6 未改变；无 `tau_*`；源侧未运行。v5 的负结果只否定“BFR `behavior->activeNodes` 是运行时子图”的解释，不否定 `State3` Blender 本身。
