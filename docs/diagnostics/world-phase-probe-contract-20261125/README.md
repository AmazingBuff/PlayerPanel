# C-WORLD v1：正常游玩（面板外）的 Smooth 相位采样探针（预注册契约，2026-11-25）

> **本文在实现前落盘，判据不看运行结果。**

## 一、为什么需要

[C-IDLE v1](../idle-loop-contract-20261123/README.md) 与 [v127 上机记录](../idle-loop-v127-session-20261124/README.md) 已确认：**面板内** `mag_start on`(`801`) 在整场会话中从未被清除，`* idle Start` 变体因此在多个武器族上被反复选中。

**但"正常游玩是否也会卡在 Start"从未被实测。** 用户目视确认"Start 只播一段就转 Loop"，然而 [v127 §三](../idle-loop-v127-session-20261124/README.md) 的全部离线证据都显示清 `801` 只有 `weaponSheathe` 一条路，而 idle 相位不经过 Sheathe。两者矛盾，必须用数据裁决。

**现有仪器的盲区**：`smooth markers` 行的采样点在 [controller.cpp:818](../../../src/animation/controller.cpp) 的 `queue_idle` 内——只有面板打开、面板自己的 idle 通路被调用时才输出。面板关闭时**没有任何标记采样**。三份归档日志的全部 `DAC OBSERVED` / `ANIMATION` 行都落在面板窗口内。

## 二、探针定义（预注册）

**新键**：`[Experiment] WorldPhaseProbe`，默认 `off`。

**采样点**：`RE::PlayerCharacter::Update` 之后（由 SKSE 每帧 sink 或现有 tick 位置驱动），**每 250 ms 至多一行**，与面板是否打开无关。

**每行字段**（只读，不写任何游戏状态）：

```
WORLD PHASE t=<seconds> actor=<formid> markers='<smooth_marker_report 原样>' idle-clip=<当前 idle clip 名或 '<none>'> phase=<start|loop|loop2|unknown>
```

- `markers`：直接复用 [`smooth_marker_report()`](../../../src/animation/dac/effects.cpp)（已有的只读实现，含 `!` inactive 后缀）。
- `idle-clip`：从源图的当前激活 clip 读；不可得时记 `<none>`。
- `phase`：**不推断**——由 `markers` 的 801/802 组合直接映射：`801→start`、`802→loop`、两者都有→`both`、都没有→`unknown`。

**不做**：不查询 OAR、不调用 DAC、不写变量、不碰候选图。

## 三、判据（预注册，实现前固定）

一次 AE 档会话，纯正常游玩（**不开面板**）：

| # | 场景 | 判据 |
| --- | --- | --- |
| **CW-P1** | 收刀待机 ≥5 s | `801` 应为 `absent`（或 `805` 在场）。若 `801` 常驻 → 收鞘域也卡相位，推翻当前理解。 |
| **CW-P2** | 拔刀 → 站定不动 ≥15 s | 采到 ≥3 个时间点的 `markers`。**关键判据**：`801` 是否在 Start 播完后消失。 |
| **CW-P3** | 拔刀后走动 ≥5 s → 停下 ≥10 s | `801` 与 `802` 的变化时点，用于判断是否由移动/停下驱动。 |
| **CW-P4** | 收刀 → 拔刀 ×2 循环 | 每次拔刀后 `801` 是否重新出现；两次行为是否一致。 |

**判读规则**：

- 若 CW-P2 采到 `801` **消失** → 世界确实会清 `801`，存在面板未复用的机制；继续定位该机制（下一步：比对同一时刻的 OAR 查询结果）。
- 若 CW-P2 采到 `801` **常驻** → **世界与面板行为一致，"卡 Start"不是面板缺陷**，而是 Smooth Moveset 的相位设计在两种场景下相同。此时 C-IDLE 的修复方向应关闭（选项 C），缺陷降级为"上游资源配置特征"。

**判据不放宽；两种结果都有明确的下游动作。**

## 四、实现边界

- 只新增：`settings.h`/`.cpp` 一个 bool 键 + 一个采样函数 + 一处每帧调用点。
- 复用 `smooth_marker_report()`（[effects.cpp:272](../../../src/animation/dac/effects.cpp)），不新增效果遍历逻辑。
- 键 off 时输出与当前逐字节一致（不新增任何行）。
- 采样间隔硬编码 250 ms，不新增配置。
- 不改 OAR/DAC/候选图任何路径；不写游戏状态。

## 五、停点

- 探针行长超过 4 KB / 会话超过 5000 行 → 停用并记录（防止日志失控）。
- 游戏崩溃：保留日志与匹配 PDB，不在同进程重试。
- 本契约只解决"世界是否卡 Start"这一个问题，不延伸到修复。

## 六、变更记录

- 2026-11-25：契约落盘。触发原因：v127 的离线结论（清 `801` 只有 `weaponSheathe` 一条路）与用户目视（世界 Start→Loop 正常）矛盾，且现有仪器在面板外无采样能力。

## 七、run 1 判读（2026-11-25 22:40–22:43）

**完整记录见 [world-phase-20261125](../world-phase-20261125/README.md)**；日志 [CharacterPanel-worldphase-run1-20261125.log](../CharacterPanel-worldphase-run1-20261125.log)（103,124 B，`8dcfe5f7…`），722 行采样。

**CW-P2 命中"`801` 常驻"分支**：`phase=both` 401 行、`unknown` 297、`start` 24、**`loop` 0**。时间线两次拔刀同构——`start` 约 3 秒（Start 段时长）→ `both` 持续到收刀。

**按 §三 预注册规则裁定**：世界与面板在 `801` 状态上**一致**，因此：

- **方向 B（补派 `sp_start off`）否决**——世界不靠清 `801` 离开 Start，补清会创造世界不存在的状态。
- 方向 A 仍可作候选，但依据变为"修正 OAR 在面板上下文中的判定差异"，需先解释差异来源。
- 方向 C 依据增强。

**本契约范围外**：探针只测标记、不测 OAR 选择结果，因此不能据此断言"面板行为正确"；世界与面板在 `both` 状态下 OAR 选择不同这一点仍待解释。

**探针缺陷**：首轮实现对 token 用 `0xFFFFFF` 掩码比较，六位运行时值永不匹配，导致全部行报 `start-on=false`。修正为低三位十六进制（提交 `34d099e`）；原始 `markers` 字符串已保留，无需重跑。
