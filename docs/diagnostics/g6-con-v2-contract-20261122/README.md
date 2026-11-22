# C-G6-CON v2：候选分支域收敛——变量同步 + 定向事件注入（预注册契约，2026-11-22）

> **本文在实现前落盘，判据不看运行结果。** 前置：[C-G6-CON v1 run1](../g6-con-contract-20261122/README.md) 判停（H-STRUCT：状态机域转换事件驱动，仅变量同步不触发）+ [事件取证会话](../../CharacterPanel-g6con-evidence-20261122.log) 落定注入事件名。修复方向 = **CON-A'**（CON-A 机制保留 + 定向事件注入）；本契约是 v1 的修订轮，v1 的判停不改写。

## 一、机制假设（H-SYNC-EVT，运行前固定）

候选状态机的域转换由 **58377 事件派发**驱动（G3-B replay 已在候选上驱动成功这两个事件），变量是转换的伴随态。把源侧驱动变量 prime 进候选自己的变量集后，**按方向派发对应域切换事件**，候选状态机将走完转换链，其 activeNodes 分支集跟随源。

## 二、注入集与方向判定（预注册，来自取证会话 2026-11-22 23:13，日志 `1cd9bcec…`）

### 2.1 注入事件

| 方向 | 事件名 | id（源图事件表） | 取证证据 |
| --- | --- | --- | --- |
| 拔刀（MT→1HM+MLh） | `WeapEquip` | 2 | 窗口 1 seq=1 + 二次拔刀 seq 重现（相位 `currentDefaultState 0→10→1`、`iState 0→9`、nodes→114） |
| 收鞘（1HM+MLh→MT） | `Unequip` | 144 | 窗口 1 seq=38（相位 `1→12→0`、`iState 9→0`、nodes→106）；与 C-G3B-S6 已解析名一致 |

辅证事件（**不注入**，仅记录对照）：`BeginWeaponDraw`(169)/`weaponDraw`(36)/`WeapEquip_Out`(49)/`WeapEquip_OutMoving`(75)；`BeginWeaponSheathe`(170)/`weaponSheathe`(37)/`Unequip_Out`(50)/`Unequip_OutMoving`(76)/`MTState`(1098)/`IdleStop`(96)。事件 id 从**候选侧**事件表按名解析（同项目应同 id；解析失败 → 该方向记 `dispatch=skip-unresolved`，不猜 id）。

### 2.2 方向判定（预注册，替换 v1 的 initial 规则）

用 **(候选 iState, 源 iState)** 对，每次签名变化或窗口初绑定时判定：

| 候选 iState | 源 iState | 动作 |
| --- | --- | --- |
| 0 | 9 | **拔刀方向**：prime 10 变量（新值）→ dispatch `WeapEquip` |
| 9 | 0 | **收鞘方向**：prime 10 变量 → dispatch `Unequip` |
| 相等或其他组合 | — | 仅 prime，`dispatch=none`（初始同域/未知域，不盲派） |

变量 prime 顺序在 dispatch **之前**（与 C-G5-CAL D2 的 "prime before dispatch" 纪律一致——取证显示转换链上 iState 翻转发生在事件后数帧，先 prime 保证转换条件读到终值）。`currentDefaultState`/`bIsInMT` 等其余 9 词与 v1 同集同序。

### 2.3 机制

- 变量同步执行体（v124 `domain_sync_tick`）原样保留：签名检测、`G6 SYNC ARM`/`G6 SYNC` 行、失败变量停用、32 事件预算、源侧只读。
- 事件派发复用 G3-B 路径：`event_dispatch_t`（58377）+ `InjectedEvent{id, pad, payload=null, sender=null}` 栈记录；context 沿用候选驱动路径既有的 58315 自建 context 纪律（实现细节在实现评审时定，不得新开写源路径）。
- **新 INI 键** `CandidateDomainDispatch=true`（默认 off；语义升级不换回旧键——v1 的 `CandidateDomainSync=true` 仍是 prime 前提：dispatch 只在 sync 开启时生效）。重启生效。
- `G6 SYNC` 行扩展字段 `dispatch=<WeapEquip|Unequip|none|skip-unresolved>`。

## 三、判据（预注册，继承 v1 §三并修订）

- **CON-P1（clip 家族跟随，主判据）**：同步+派发开启下，每个域状态 ≥2 检查点、共 ≥2 域：候选 active clip 集（归一化后）== 源 active clip 集（归一化后）。
- **CON-P2（旧域退出）**：每次方向切换后的首个快照，候选不含前一域任何 clip 节点。
- **CON-P3（写与派发证据）**：`G6 SYNC` 行逐变量 `ok` 且 `dispatch=` 与方向判定一致（拔刀方向 → `WeapEquip`）；派发失败/跳过逐条归因。
- **CON-P4（源安全）**：`source-drift=none` 保持、G6-A 判据（ledger/DAC matched/park 恒等式）保持、事件只派发给候选。
- **CON-P5（P1 mini 回归）**：开合 ×2 + F9 ×1，读档后 `unbound-retained` 语义不变。
- **CON-P6（稳定性）**：无 `[E]`（除既有）、无崩溃。
- **附加记录（不判过）**：分支节点覆盖比；WeaponAdjustment 与结构差集残差；目视域正确性；派发后候选相位（`currentDefaultState`/`iState`）跟随时间线。

**判定**：P1+P2+P3+P4+P5+P6 全过 → **CON-A' 成立，G6-B 收敛达成**（限定：10 变量 + 2 事件、两域对照、结构差集显式保留）。P1 仍不成立 → H-SYNC-EVT 证伪，判停转 CON-B 评审（不调参重试）。部分域跟随（如拔刀跟随、收鞘不跟随）→ 逐方向记录，不整体判过。

## 四、停点与负路径

- 事件解析失败 → 该方向停用派发（记 `skip-unresolved`），不猜 id。
- 派发后预算耗尽 → 同步整体停用（沿用 v1 语义）。
- 源图换代/读档 → 签名与方向状态复位。
- 崩溃 → 保留日志与 PDB，不同进程重试；判据不满足不调参重试。

## 五、上机协议（CON v2 run 1）

1. 环境：AE 1.6.1170、Save116、`AE-Track2`；一次会话一个 DLL；构建标签建议 `v125-g6-domain-dispatch`；三方哈希互核。
2. INI：v124 CON 档基础上加 `CandidateDomainDispatch=true`（`CandidateDomainSync=true` 保持）。
3. 场景：**T1** 拔刀 idle ≥3 s → 开包 F7 ×2 → 关包；**T2** 收鞘 idle ≥3 s → 开包 F7 ×2 → 关包；**T3** 再拔刀 → F7 ×2；**T4**（可选）魔法域 → F7 ×2；**T5** P1 mini 回归（开合 ×2 + F9 ×1）。
4. 目视：T1/T2 显示姿态域正确性（一句话）。
5. 回传：日志改名 `CharacterPanel-g6conv2-<日期时间>.log` + 目视结论 + `[E]` 一句话；判读按 §三。

## 六、边界

- 单运行时 AE 1.6.1170；只验分支域收敛（clip 家族层），不验逐骨姿态等价、不碰 `tau_*`/K5-5/K2-C1/A17。
- 注入集变更 = 契约修订；魔法域（MLh）为可选对照，无魔法条件时两域成立即判过（同 v1）。
- `G6 SYNC` 行带 `dispatch` 字段后仍为独立行系列，不挤占其他预算。

## 变更记录

- 2026-11-22：契约落盘（注入集来自当日取证会话；实现前，判据不看运行结果）。

## 七、取证会话判读（2026-11-22 23:13，日志 [CharacterPanel-g6con-evidence-20261122.log](../../CharacterPanel-g6con-evidence-20261122.log) 211 行，SHA-256 `1CD9BCEC…F33578`；同 v124 DLL、取证档 INI `60ecb8aa…`）

窗口 1（20 秒满额，`events=40+ phases=30`、`ring-dropped=0`）完整覆盖两次拔刀 + 一次收鞘：

- **拔刀序列（×2，相位 currentDefaultState 0→10→1、iState 0→9、nodes 106→…→114）**：`WeapEquip`(2) → `AnimObjectUnequip`(543) → … → `BeginWeaponDraw`(169) → `weaponDraw`(36) → `WeapEquip_Out`(49)/`WeapEquip_OutMoving`(75)。**首事件 = `WeapEquip`，两次拔刀一致。**
- **收鞘序列（相位 1→12→0、iState 9→0、nodes 114→…→106）**：`Unequip`(144) → … → `BeginWeaponSheathe`(170) → `weaponSheathe`(37) → `Unequip_OutMoving`(76)/`Unequip_Out`(50) → `MTState`(1098)/`IdleStop`(96)/`tailMTIdle`(1186)。**首事件 = `Unequip`，与 C-G3B-S6 解析名一致。**
- 注入集落定：拔刀 = `WeapEquip`(2)、收鞘 = `Unequip`(144)；辅证事件表见 §2.1。`ring-dropped=0`、无 `[E]`。
