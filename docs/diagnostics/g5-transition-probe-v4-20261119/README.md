# C-G5-TRANS v4：目标状态 Modifier 权重观测（2026-11-19）

**状态：PRE-REGISTERED（2026-11-19）；实现 v113；未上机。** 本契约接续 [v3](../g5-transition-probe-v3-20261118/README.md) 的候选 run 1；不改变 `G4P-7 FAIL` / G5/G6，不产生 `tau_*`。

## 一、v3 run 1 的输入

- overflow 已消除：747 条 header 全 `flags=0x0000`；K5 249/249。
- `0→10` parent `hkbBlendingTransitionEffect` 只在 cmd=1 采到（duration 0.2、time-in 0.016667），cmd≥2 移出 `activeTransitions`；AUX `transitionFlags` size 4 head `0000E9D1`、`wildcard-flags` 全 0、`delayed` size 0，cmd 1–13 恒定。
- `10→1` 在 cmd=87–98 完整推进到 0.183333；12 点姿态 PC1 99.83%、Pearson 0.992817。
- census 无 `State3`；active equipment 节点含 `WeapEquipModifierGenerator`、`MT_Default_MG`、`BodyWeightBlendModifier`、`RootModifierGenerator`（均 `hkbModifierGenerator`）与 `WeapEquipBFR`/`MT_BehaviorBFR`（behavior-reference）。

v4 的目标：**在目标状态内部找 0→10 的逐帧标量**，优先观察 active `hkbModifierGenerator` 的 modifier 与变量绑定；不继续读 parent `activeTransitions`/AUX，也不声称 `State3`。

## 二、观测设计

### 2.1 Modifier 结构

对每个 active node，vtable 精确等于 `RE::VTABLE_hkbModifierGenerator[0].address()` 时读取（受保护的运行时结构；布局来源为 HKX2 `hkbModifierGenerator` 的 `modifier +0x48` / `generator +0x50`，本契约标记为 `layout=hkx2`，不是门 5 判据）：

- generator 节点：`node`、`vtable`、`name`、`id`；
- `modifier +0x48`：指针、vtable、`hkbNode::name +0x38`、`id +0x40`、`hkbModifier::enable +0x48`；
- `generator +0x50`：指针、vtable、class；
- `modifier +0x10` 的 `hkbVariableBindingSet*`：bindings 数组 `+0x10` data / `+0x18` size；每个 Binding `0x28`，读 `memberOffset +0x10`、`variableIndex +0x1C`、`bindingMode +0x21`、`memberType +0x22`；
- 变量名/值：`behavior->data->stringData->variableNames[var]` 与 `behavior->variableValueSet` 的 word 数组（`+0x10` data / `+0x18` count）；类型从 `behavior->data->variableInfos[var].m_type` 读。

所有指针先经 `plausible_object_pointer`；任一读取失败只记 `status=unavailable`，不 deref。

### 2.2 日志

- `TRANSITION MODIFIER side=candidate segment=... phase=... seq=... commanded=... index=... node=... name='...' modifier=... modifier-vtable=... modifier-name='...' modifier-id=... enable=... generator=... generator-vtable=... generator-class=... bindings=...`
- `TRANSITION MODIFIER BIND ... binding=... member-offset=... var=... var-name='...' type=... mode=... word=... float=...`
- 最多 8 个 modifier / 8 个 binding；超过记 `overflow=1` 到 summary 行，不静默截断。
- 只在候选三个 phase（post-update/pre-sample/post-sample）调用；log-only，不进入 `TransitionProbeRecord`，不增加 source ring 内存。

## 三、假设与判据

| 编号 | 假设/判据 | 通过/记录 |
| --- | --- | --- |
| H-M1 | `BodyWeightBlendModifier`/`WeapEquipModifierGenerator`/`MT_Default_MG` 的绑定变量是 0→10 组的单标量 | 至少一个 modifier binding 的 word/float 在 equip cmd 1–13 变化；与 0.208333 组 PC1 相关（描述性） |
| TP-1 | 身份/环境（AE-Track2、OAR 关闭、dummy 记账） | 与 v3 相同 |
| TP-2 | K5 249/249、747 header、overflow 0 | 与 v3 相同 |
| TP-3 | cmd=1 `Default_Behavior state=10` + blending transition | 与 v3 相同 |
| TP-4 | `10→1` 12 点到 duration−1/60；0→10 只记录起点 | 与 v3 相同 |
| TP-5 | `State3` 仍不在 activeNodes | **NOT OBSERVED**，不在 v4 声称 |
| TP-6 | `10→1` PC1/Pearson 复算 | 与 v3 相同 |
| TP-7 | 无写入/新 hook；`source-drift=none` | 与 v3 相同 |
| TP-8 | modifier 观测资格 | 至少一条 `TRANSITION MODIFIER`；binding 读取失败可接受但需记 `unavailable`；若所有 modifier 都 `bindings=0`，H-M1 未目击 |
| TP-9 | modifier 标量 vs 0→10 PC1 | 只报告相关/单调性，不设阈值 |

**不把 TP-8/TP-9 写成 G5 通过条件**；它们只决定是否进入 v5 的“clip 时钟 + modifier 标量”配对协议。

## 四、运行协议

1. 构建 v113，离线测试；备份 v112 三件套，安装 v113 + 候选档（同一 `TransitionStateProbe=true` 档）。
2. MO2 `AE-Track2`（OAR 停用是设计）→ Save116 → 一次 panel → equip 120/120 → settle → `Unequip` 144 → unequip 129；关闭退出。
3. 判读：K5 249/249；747 header 全 flags 0；`TRANSITION MODIFIER` 是否出现 `BodyWeightBlendModifier`/`WeapEquipModifierGenerator`/`MT_Default_MG` 及其绑定；0→10 组 cmd 1–13 的 modifier word/float 是否变化；`10→1` 12 点相关复算。
4. 源侧不在本轮；TP-5 另立布局/父链审计。

## 五、明确不做

- 不启用 OAR、不切 `AE`；不改 `tau`/`epsilon_t`；不删轨。
- 不读 `State3`/`hkbBlendingBehavior` 未审计偏移；modifier 布局标记为 HKX2，不是引擎审计结论。
- 不把 modifier 相关写成 G5 通过；G4P-7 仍 FAIL。

## 六、实现影响面

| 文件 | 变更 |
| --- | --- |
| `transition_probe.h/.cpp` | `log_transition_modifiers`（log-only；modifier/binding 读取；guarded） |
| `independent_graph.cpp` | 在 `log_transition_stage` 后调用 modifier 日志；不改采样点 |
| `probe_liveness.cpp` | 无变更（source ring 不变） |
| `tests/transition_probe_test.cpp` | v4 版本/容量/预算不变；modifier 日志为引擎绑定，不做假对象测试 |
| `docs/build.md` | v113 构建/部署/运行记录 |
