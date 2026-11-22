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

## 八、CON v2 run 1 判读（2026-11-22 23:35–23:38，日志 [CharacterPanel-g6conv2-20261122.log](../../CharacterPanel-g6conv2-20261122.log) SHA-256 `DDA1713E…B39AF`；构建 `…v125-g6-domain-dispatch-20261122-740a94b1b9-…` 在位、INI `f8c291f1…`、两键在案）

### 预注册判据判定

| 判据 | 证据 | 判定 |
| --- | --- | --- |
| **CON-P1 clip 家族跟随（主判据）** | **拔刀域（r1/r3 settled）**：候选 clip `{1HM_Idle, MLh_Idle, SCAR_1HM_Ready_Dummy1}` vs 源 `{…Dummy0}`——**归一化后完全相等**；候选 b1hm=18/bmt=0/**bmlh=5**（MLh 与源逐项相等）、源 b1hm=10/bmt=0/bmlh=5。**收鞘域（r2.3+ settled）**：候选 clip `{MT_Idle}` vs 源 `{MT_Idle.HKX}`——归一化相等（与 run1/run2 形态一致）。两域 × ≥2 检查点 | **成立** |
| **CON-P2 旧域退出** | 拔刀后候选 MT 分支 10→**0**（无 MT_Idle clip）；收鞘后候选 bmlh 5→0、无 1HM_Idle clip；r3 反向再次成立——**双向零残留** | **成立** |
| CON-P3 写与派发 | 8 ARM + 8 SYNC：r1 `direction=draw dispatch=WeapEquip`、r2 `sheath dispatch=Unequip`、r3 draw、r4–r8 `none/none`（同域 prime-only，契约 §2.2 预期）；writes 全 `ok`、零 `failed/missing`；签名域语义与取证自洽（`b5a65c60`/`612ce9cc`） | **成立** |
| CON-P4 源安全 | `phase=retained source-drift=none`；DAC **9/9 matched**；park 17 行（16 + 13 处阈值告警 1）== 终值 16；E1 `candidate=retained` 全程、graph `0x25baad49080` 恒定 | **成立** |
| CON-P5 P1 mini 回归 | T5：panel-close ×8 全 matched；F9 ×1 后 `held-source=unbound` + `window=rejected reason=unbound-retained` ×1 + request 重计数 | **成立** |
| CON-P6 稳定性 | `[E]` 零新增；无崩溃 | **成立** |

目视结论未回传（主判据为 clip 家族机械判定，不阻塞）。

### 裁定：CON-A' 成立，**G6-B 收敛达成**（限定：10 变量 + 2 事件、拔刀/收鞘两域、结构差集显式保留）

**三连①②修复实证**：1HM 滞留消除（双向：MT→0、1HM→1 退出）、MLh 分支激活（0→5 与源逐项相等）。派发后转换链在同窗口内完成（r1 call60 时 MT 已退、1HM 未全进的中间态 → call120 settled——状态机确实被事件驱动走了完整转换链，H-SYNC-EVT 实证）。

### 记录的边界（不判 FAIL）

1. **iState 粒度限制（新）**：魔法就绪域（r4：源 b1hm=1、bmlh=2、clip `{MLh_Idle, MRh_Idle}`）与拔刀域共享 iState=9 → 方向规则判 `none`、候选滞留拔刀域形态（clip `{1HM_Idle, MLh_Idle, MRh_Idle, SCAR_Dummy0}`）。契约 §2.2 预注册行为——"相等或其他组合仅 prime 不盲派"。**G6-B 收敛限定为拔刀/收鞘两域**；魔法域收敛需扩展方向判定（未来工作，需新的域区分变量/事件取证）。
2. **候选侧 1stP 超集**：拔刀域候选激活源不激活的 12 节点（`1HM_1stPUpperBody`、1stP 系 modifier、`1stP_MLh_1HM_Idle_Blend`、`WeapIdleMSG` 等第一人称变体）——clip 层不受影响（家族相等），多激活记录在案。
3. **结构差集恒定**：WeaponAdjustment（两域）与收鞘域 10 项源独有保持——C-G5-TOL 已接受的残差不变，修复不触碰。
