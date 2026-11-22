# C-G6 v1：生命周期与资源恢复 + 克隆分支收敛（预注册契约，2026-11-22）

> **本文在实现前落盘，判据不看运行结果。** 对应主文档 [G6 定义](../../independent-behavior-graph.md)（「G6：生命周期与资源恢复」节）与 [C-G5-CHAIN run1 判读](../g5-idle-chain-probe-20261121/README.md) 的克隆收敛缺口三连（1HM 分支滞留、缺 MLh 分支、缺 WeaponAdjustment 节点）。
> 契约分四段：**P0** 例外与对账清单（静态，源自 2026-11-22 代码盘点）；**P1** 生命周期全场景验证（原 G6 契约主体）；**P2** 分支收敛探针（取证，不修复）；**P3** 收敛修复立项条件（另立 C-G6-CON，本契约只立准入，不预注册修复判据）。

## 一、范围与例外

### 1.1 G6 验证目标（原契约原文展开）

记录创建前/运行中/关闭后的源状态、请求/任务代数、预览持有的引用、输出、回调、事件 sink、OAR 登记与历史。在**重复会话、快速换装、重捕获、关闭、读档**时验证：旧请求/任务失效、预览自有关系正确解除、可观察计数与状态回到基线且不持续累积。`DAC restore … result=matched` 仅支持对应效果恢复；无崩溃不等于 G6。

### 1.2 显式例外（不计失败，但必须对账）

| # | 例外 | 实现位置 | 对账要求 |
| --- | --- | --- | --- |
| E1 | 已装载候选每进程保留一张、永不释放（方案 A） | `independent_graph.cpp:1821-1831`（`new BSTSmartPointer` 泄漏持有）+ `:1608-1612`（`static bool attempted` 闸门） | 每进程恰好 ≤1 张；`retained` 行有记录；读档/换 owner 后 `g_held_source`/`g_held_step_ready` 失效（`independent_graph.cpp:1597-1601`）而图本体留存 |
| E2 | SCOPY PARK：退役克隆树刻意 park（函数静态 `NiPointer` vector 存活到进程结束） | `scene_graph_capture.cpp:271-284`（`s_parked_graphs` 永不 clear） | 每次 retire 必有 `SCOPY PARK … retained-total=N` 行；`retained-total` 增量 == retire 事件数（对账恒等式）；**不作为内存回收证据**（[scene-graph.md §74](../../scene-graph.md)） |
| E3 | 装载候选停用/销毁 NO-GO | [G1 遗留一审计](../../independent-behavior-graph.md#g1-teardown-audit) | G6 不做候选销毁实验；例外不扩大 |
| E4 | 全局引擎登记计数无锚点 | [G1 遗留二更正](../../independent-behavior-graph.md#g1-teardown-audit) | 不立项、不测量 |
| E5 | 进程级 sink/hook 一次性安装、无移除路径 | 盘点 §2：SKSE listener、装备/菜单/输入 sink、Present hook、OAR 4 detour、SKSE trampoline 3 组 | 它们不随预览会话创建/销毁，**不是** G6「预览拥有的回调解除」的对账对象；G6 对账对象是 per-session/per-window 状态（§二）。若未来出现会话级 sink，才纳入对账 |

### 1.3 非目标（不属 G6，判据不覆盖）

- K5-5 层/权重/混合语义、K2-C1 完整装备清单、A17 独立控制、EQ-C1 快照级状态同域（见 [C-G5-VER 未闭合清单](../g5-ver-formal-20261122/README.md)）。
- 门 4 冻结 `tau_*` 与整体姿态等价主张。
- 场景应用（物理、武器挂载交互）——仍属未验开放项，不在本契约。
- 原生内存字节回收（native 分配无界，teardown 未解决——只验可观察计数，不验字节数）。

## 二、可观察状态对账清单（P0，代码盘点基线）

每一项标注：创建时机 / 失效或销毁时机 / 对账读取点。P1 各场景的前后对账按此表执行。

### 2.1 预览会话级（创建于 panel-open，销毁于 close/reset）

| 状态 | 创建 | 失效/销毁 | 读取点 |
| --- | --- | --- | --- |
| SceneGraph 克隆体 | `capture_graph`（`scene_graph_capture.cpp:286`，`NiObject::Clone` :355） | `replace_graph` :272 退役旧体 → E2 park | SCOPY PARK 行计数 |
| `AnimationController::State`（含 Proposal/history、variants、源图 `BSTSmartPointer`） | `AnimationController::reset()` 重建 | 换装 `new_lifetime` 检测（`controller.cpp:851-859`）/ 面板关闭 :834-838 | State 重建行（若有） |
| `DisplaySession`（含 `DisplayWindow` + `DisplayStepClock`） | `State` 构造 | 析构 → `abort("reset")`（`independent_graph.cpp:1234`） | `ANIMATION DISPLAY window=…` 行、`abort` 行 |
| DAC `Session` + `SessionCore::m_epoch` | `SceneGraph` 构造 :195-199 | `close()`（epoch++，`session_core.h:62-74`）/ `reset_world()`（:177-191） | `DAC restore … result=` 行、epoch 数值 |
| OAR 基动画缓存 `s_projects` | 首次查询预热 | `release_previews()` → `invalidate()`（`character_manager.cpp:130`） | （读档场景核对失效调用发生） |

### 2.2 窗口/请求/任务代数

| 状态 | 语义 | 读取点 |
| --- | --- | --- |
| `DisplayWindow` request/view/steps/rows、`DisplaySourceIdentity{actor,generation,graph,owner}` | `bind()` 重置、`retarget()` view++、`reject()` 消费 18 行预算 | `ANIMATION DISPLAY window=rejected reason=…`（7 种原因，`display_state_policy.h:23-34`）——**v42 仪器，尚无游戏数据** |
| 三路并发采集窗口（source_event / source_timeline / probe_liveness） | `abort()` 关闭三窗（`independent_graph.cpp:1160-1197`） | 各窗口 close 行 |
| 读档四件套 invalidate | `kPreLoadGame/kNewGame`（`main.cpp:63-70`）：`invalidate_source_timeline/_event_capture/_write_capture/_probe_liveness` + `dac::reset_world()` + `clear_previews()` | 日志时序行 |
| `PanelMonitor::m_revision`、DAC `state.revision`、`g_candidate_window` tally | 各自递增 | 对账表逐项抄录 |

### 2.3 源侧恢复关注点

- `AnimationController::State` 持有**源图** `BSTSmartPointer<BShkbAnimationGraph>`（`controller.cpp:388`）→ 源图引用计数 +1；State 重建/销毁时释放。对账：源图被销毁（如读档后）时预览不得残留可解引用引用——由 E1 的 `g_held_source` 失效 + State reset 保证，P1-S5 场景核对。
- 源基线 before=after：复用 G2 的源对照判据（`drift=none`）。

## 三、P1：生命周期全场景验证（预注册判据）

### 3.1 场景矩阵

| 场景 | 操作序列 | 目的 |
| --- | --- | --- |
| S1 重复会话 | 面板开→关 ×5（含 F8 显隐穿插） | 计数回基线、无例外外累积、DAC epoch 递增、DisplaySession `abort("reset")` |
| S2 快速换装 | 装备↔卸下连续 ≥4 次（settle 后再下一次） | `new_lifetime` 失效链、旧 DisplaySession 重建、PARK 增量对账 |
| S3 重捕获 | F7 连续 ≥3 次 | retarget 语义（已验）+ 资源侧对账（tally re-arm、无新分配泄漏） |
| S4 显示关闭 | F8 / 关包 | `reason=display-close` 路径（G4-C 已验的仪器侧复用）+ 资源对账 |
| S5 读档 | 游戏内快速读档 ×2 | 四件套 invalidate 时序、旧候选回落 OAR（v34 已验）+ **确切拒绝原因首采**（`window=rejected`） |
| S6 同 controller 来源替换 | 同 controller 下实际改变来源图/generation（按 [v42 协议](../../independent-behavior-graph.md#v42-residual-corrections)） | `source-generation-mismatch`/`source-graph-mismatch` 首采 + stepped 与 frozen-only 两路径 |
| S7 frozen-only 单独拒绝 | 仅令 frozen 回退失效（同 S6 协议第二段） | `reject` 原因分类首采 |

### 3.2 预注册判据（全部满足 → P1 对应场景 PASS）

对每场景 S1–S7，前后各抄一张对账表（§2 全项）：

1. **G6-C1 计数收敛**：声明为"会话级"的状态（DisplaySession、Proposal/history、DAC epoch 之外的 ring/tally）在场景后回到场景前基线或明确失效态；允许变化项必须逐项对账（+1 且有对应日志行）。
2. **G6-C2 例外对账**：E1 候选 ≤1 且 `attempted` 后不再分配；E2 `retained-total` 增量 == retire 行数；两例外外**无任何新增持久分配路径**（以对账表空项证明）。
3. **G6-C3 源恢复**：源基线 before=after（`drift=none`）；读档场景源图销毁后 `g_held_*` 失效、无 native-step 残留。
4. **G6-C4 失效链**：旧请求/任务代数（DisplayWindow identity、DAC epoch、采集窗口）在 S2/S5/S6/S7 中按 §2.2 时序失效；`window=rejected reason=` 首采行与预期原因一一对应（S5 读档 = `unbound-retained` 或具体原因、S6 = `source-generation-mismatch`/`source-graph-mismatch`）。
5. **G6-C5 DAC**：每次面板关闭 `result=matched|MISMATCH` 有行；重复会话 epoch 严格递增且旧 ticket 不复活。
6. **G6-C6 稳定性**：全程无 `[E]`、无崩溃；崩溃按停点处理，不判 PASS。

**部分通过允许**：各场景独立判定；S6/S7 首采若原因不符预期，记"原因分类与预注册不符"而不是 FAIL——因为这两项是 v42 仪器的首次真实观测，其价值是建立基线。但**不得**因不符而放宽原因枚举。

### 3.3 P1 通过 → G6-A 判定

S1–S5 全 PASS 且 S6/S7 完成首采（含不符项记录）→ **G6 原契约的"源恢复、旧工作失效、预览自有关系正确解除，重复会话无非例外积累"在该构建上有界成立**；主文档 G6 行更新为"G6-A PASSED（限定：本契约场景矩阵 + E1–E5 例外）"。

## 四、P2：克隆分支收敛探针（取证，不修复）

复用 C-G5-CHAIN 仪器（`IdleChainProbe` 的 CENSUS + clip 对照）扩展为**分支集时间线**：

- **挂点**：F7 view 快照点（call 60/120 既有检查点）+ 每次源分支切换边界（装备/拔刀/收刀 settle 后首个 view）。
- **每快照新增行**：两侧 activeNodes 按 census 顶层分支分类（1HM 分支 / MT 分支 / MLh 分支 / WeaponAdjustment 节点存在性）——四元组布尔 + 各分支节点计数。
- **判据（预注册）**：
  - **P2-V1（现状基线，预期 FAIL）**：源分支集合随世界事件切换时，候选分支集合不跟随（复现 C-G5-CHAIN run1 的 1HM 滞留 + run2 的 MLh 缺失 + WeaponAdjustment 缺失）。此判据**预期不通过**——它把缺口三连从"两次会话的证据"固化为时间线基线。
  - **P2-V2（驱动变量识别）**：源分支切换前后对照源图变量集（走 `CandidateVariableCensus` 路径），记录翻转的变量名/值与事件名。判据：至少识别出 1HM↔MT 切换的驱动变量（如 WeaponDrawn/iState 族）并记入本 README 追加节。
- P2 只取证；**修复方案与判据另立 C-G6-CON v1**（准入条件见 §五）。

## 五、P3：收敛修复立项条件（C-G6-CON 准入）

修复方向三选一，由 P2 数据裁决后另立契约（本契约不预注册修复判据）：

| 方向 | 内容 | 适用条件 |
| --- | --- | --- |
| CON-A 状态同步 | 把 P2-V2 识别的驱动变量/事件经既有 prime 路径（`CandidateEquipPrime`/`CandidateUnequipPrime/Event` 已有实验键）同步给候选 | 驱动变量 ≤8 个且同步后分支集收敛 |
| CON-B 发散拒绝 | 分支集对照不匹配 → `window=rejected reason=branch-divergence` 新原因 → 回落 OAR | 同步路径不可行/风险高 |
| CON-C 自动重捕获 | 分支切换触发一次受控重捕获（扩展 F7 retarget 语义） | CON-A/B 均不可行 |

任一方向的准入条件（全部满足才立项实现）：

1. P1 在当前构建上 S1–S5 PASS（修复不得叠加在未验生命周期上）。
2. P2-V2 已识别驱动机制（CON-A）或确认不可同步（CON-B/C）。
3. 修复不新增源图写入、不触碰 NO-GO 边界（E3）、不扩大 E1/E2 例外。

## 六、停点与负路径（沿用项目惯例）

- 崩溃：保留完整日志与匹配 PDB，不在同进程重试；保留构建身份行。
- 源漂移/源 pose 改变：停止该场景，记 `source-drift`，不拼接。
- 对账表预算耗尽/缺行：该场景记"对账不完整"，不判 PASS，不事后补行。
- 任一场景判 FAIL 后不做"调整判据重试"；下一场景继续（场景独立判定）。
- 运行后 INI 全部实验键回默认 off 并重启验证。

## 七、上机协议

1. 运行时 AE 1.6.1170、单存档（Save116）、`AE-Track2` 装备族（与 G5 环境一致）；一次游戏会话一个 DLL。
2. 构建/部署互核：DLL/PDB/部署 INI 哈希三方核对（v115 教训）；新构建标签建议 `v123-g6-lifecycle`（含 P1 仪器 + P2 分支集行）。
3. 场景按 S1→S7 顺序，每场景前抄对账表前值；S6/S7 按 v42 协议操作序列。
4. 运行后：完整日志归档 `diagnostics/CharacterPanel-g6-<场景>-<日期时间>.log`、构建身份、INI 哈希回传；离线判读按 §三/§四。
5. INI 键组合建议：P1 场景 `IndependentGraph=inject` + `DisplayPose=true`（S3/S4 另开 `DisplayStep`）；P2 场景加 `IdleChainProbe=true` + 新分支集行键（若需新键，命名 `[Experiment] BranchCensus=true`）。

## 八、边界

- 单运行时 AE 1.6.1170；本契约不证明整体姿态等价、不触碰门 4 `tau_*`、不关闭 K5-5/K2-C1/A17。
- S6/S7 的首采价值是建立 `window=rejected` 分类基线；判据不符只记录不放宽枚举。
- P2 只在 idle/装备域取证，不含过渡窗（§八.3/6 另立）。
- 对账表覆盖声明路径；未列路径（如未来新增模块的静态状态）在实现评审时补入 §二后才有效。

## 变更记录

- 2026-11-22：契约落盘（P0 盘点基线来自当日代码盘点；判据不看运行结果）。

## 九、run 1 判读（2026-11-22 22:30–22:37，日志 [CharacterPanel-g6-candidate-20261122-2236.log](../CharacterPanel-g6-candidate-20261122-2236.log) 1,695,086 B，SHA-256 `2F7BD373…51F5F1`；构建 `…behavior-graph-v123-g6-ledger-20261122-8c339cc744-…` 在位，INI `3a6d9e22…`）

身份与键面：`SCENEGRAPH BUILD …v123-g6-ledger-…` 在位；`EXPERIMENT setting G6Ledger=true`、`IdleChainProbe=true` 在案（[E] 仅 OAR-not-loaded 1 条既有项）。17 行 `G6 LEDGER`（14 panel-close + 3 preload-game）、28 行 `SCOPY PARK`（含 1 条阈值告警）、14 行 `DAC restore`、67 行 native-step、33 组 `IDLE CHAIN BRANCHES`/`IDLE CHAIN VARS` 快照（16 views）。

### P1 场景判定（预注册 G6-C1..C6）

| 场景 | 证据 | 判定 |
| --- | --- | --- |
| S1 重复会话 ×5+ | 8 次 panel-close 边界（22:31:00–22:33:34）：每次 `ANIMATION … lifetime reset` + `G6 LEDGER phase=panel-close` + `DAC restore … result=matched`；`dac-sessions=1` 恒定、`panel-rev` 每开合 +2 | **PASS**（C1/C5） |
| S2 快速换装 ×12 | `ANIMATION reason='equipment'` ×12；park 增量区间（+6、+4）对账 OK。**更正**：`lifetime-replace` 0 条不是缺陷——装备变化不改 source-generation（12 行全部 `generation=1`），`new_lifetime` 不触发，MANUAL 预期写错，照实更正 | **PASS**（C1/C2） |
| S3 重捕获 ×5 | `window=aborted reason=retarget` ×5（steps=2）、view 递增（request=12 view 2→3→4→5）、native-step 67 行 | **PASS** |
| S4 显示关闭 | `window=aborted reason=reset`（steps=89，中途关包）；`reason=display-close` 0 条 = `SourceEventCapture=off` 下无 capture 窗口（预期内，该行属于 capture 窗口关闭） | **PASS** |
| S5 读档 ×2 | `preload-game` ×3（含首载）；读档后 `held-source=unbound step-ready=0`（E1 失效路径运行证据）、request 重计数 1→…；**`window=rejected reason=unbound-retained` ×7（22:33:58–22:36:48）——v34 遗留"读档后旧候选被拒的确切原因"落定 = unbound-retained**；epoch 新 session 从 2 重开不复活 | **PASS**（C3/C4） |
| S6 同 controller 来源替换 | 无 `source-generation-mismatch`/`source-graph-mismatch` 行。首采基线：装备替换不改 source-generation（恒 1）、不改图指针 → `new_lifetime`/拒绝分类不可达；读档拒绝走 `unbound-retained`。**该分类在游戏内的可达路径比 v42 预期窄**（需图指针或 generation 变化） | 首采记录（不判 FAIL，契约 §3.2 允许） |
| S7 frozen-only | 未观测到分类行 | not-reached 记录 |

**对账恒等式（C2）**：park 增量 vs `SCOPY PARK` 行数 **16/16 区间精确相等**（28 行 == 终值 park=28）；E1 `candidate=retained` 全程、candidate-graph `0x2f2b735ce00` 恒定、无再分配；两例外外无新增持久分配（ledger 各字段收敛）。DAC epoch 序列干净（每开合 +2，读档后重开）。`INDEPENDENT GRAPH phase=retained source-drift=none`（C3）。

### P1 总判定

S1–S5 全 PASS + S6/S7 首采基线在案 + C1..C6 全过 → **G6-A（源恢复、旧工作失效、预览自有关系解除、重复会话无非例外积累）在本构建上有界 PASSED**。限定：本契约场景矩阵 + E1–E5 例外；S6/S7 分类行的可达性结论是本次首采的观察，不是完备性证明。

### P2 判定（预注册 P2-V1/V2）

- **P2-V1 NOT REACHED（前提未发生）**：33 快照（16 views，22:30:54–22:33:29）两侧分支集**各自恒定**——源未发生分支切换事件（玩家全程收鞘 MT 域，`iRightHandType` 只在武器类型间换 1→5→1→6→7→6，从未拔刀/徒手）。
- **静态收敛缺口基线（比预期更强的证据）**：两侧分支集恒定不相等。末对 census（req13 view2 call120）差集：**源独有 10 项** = `WeaponAdjustment_OffsetAnimationGen`、`MT_Idle_3rdPersonBehavior`、`MT_Idle_TurnBehavior`、`Behavior00`、`Behavior02`、`IdleBFR`、`IdleBehavior.hkb`、`MT_Idle.HKX`（命名对，见下）、`bIdleBeforeConversationIsActive`、`bIdleBeforeConversationModifier`；**候选独有 1 项** = `MT_Idle`（同资源无 `.HKX` 后缀的命名差）。`common=96`、cand=97、src=106。**缺口三连之"缺 WeaponAdjustment"在 33 快照一致复现**；1HM 滞留、缺 MLh 本次无域切换/魔法对照（NOT REACHED，未被反驳）。
- **域同构正面证据**：两侧 active clip 同为 MT_Idle（源名 `MT_Idle.HKX`、候选名 `MT_Idle`——候选经 unequip replay 后与源同在收鞘 MT 域，96/97 节点共同）——与 C-G5-CHAIN run1"候选滞留 1HM"形态不同：本次域未切换，滞留未触发。
- **P2-V2 NOT REACHED + 附加发现**：1HM↔MT 切换驱动变量未取得（无切换）；实际识别出**装备变化驱动变量** = `iRightHandType`(idx35, INT)、`iLeftHandType`(idx60, INT)、`weaponSpeedMult`(idx47, REAL)、`TimeDelta`(idx53, REAL 时钟)；检查点间无其他词翻转（`truncated=0` 全程、单快照 changed ≤2）。

### 对 C-G6-CON 立项的影响（契约 §五）

准入条件 2（驱动机制识别）**未达成**——CON-A 需要的分支切换驱动变量仍缺一次"拔刀↔收鞘（或装备↔徒手）"的对照快照。下次会话：持武器拔刀 idle ≥3 s → F7 ×2，再收鞘 idle ≥3 s → F7 ×2，即可在同一 INI 下补齐 V1 对照 + V2 切换变量。WeaponAdjustment 缺口（本次已固化）与 MT 状态机缺失可先进入收敛设计讨论。

## 十、run 2 判读（2026-11-22 22:45–22:47 补课会话，日志 [CharacterPanel-g6-candidate-20261122-2nd.log](../CharacterPanel-g6-candidate-20261122-2nd.log) 746,140 B，SHA-256 `0831CB2F…3A3D`；同构建 v123、新进程 candidate-graph `0x2122ed53100`）

协议达成：拔刀域 3 views（22:46:40–22:46:51）→ 收鞘域 3 views（22:47:07–22:47:19），同一 request 系列内域切换完成；ledger 干净（preload ×1 + panel-close ×2、DAC 2/2 matched、park 5 行 == 终值、`source-drift=none`、[E] 仅既有 1 条）。

### P2-V1 成立（预期 FAIL 复现，三连全部实证）

源分支集随拔刀/收鞘切换，候选恒定不跟随：

| 域 | 源 activeNodes | 候选 activeNodes |
| --- | --- | --- |
| 拔刀（1HM+MLh 魔法域） | 114 节点：b1hm=10、bmt=0、**bmlh=5**、weapon-adj=1 | 97 节点：b1hm=1、**bmt=10**、bmlh=0、weapon-adj=0（**滞留 MT 域**） |
| 收鞘（MT 域） | 106 节点：b1hm=1、bmt=12、bmlh=0、weapon-adj=1 | 97 节点：MT 域（跟随） |

拔刀域差集（common=80）：**源独有 33 项** = 整个 1HM 武器分支（1HM_Behavior/1HM_Idle/SCAR_1HM_Ready 系）+ **整个 MLh 魔法分支**（MLh_Idle、MLh_Ready_MSG、MagicBehavior/MagicRoot/MagicReady 系、DefaultMagicReadyBehavior）+ WeaponAdjustment_OffsetAnimationGen + Weap_BehaviorBFR/Weap_Readied_Lean_Blend；**候选独有 17 项** = 整个 MT 分支（MT_BehaviorGraph/MT_Idle/MT_Default/MT_Root/MTIdleTurnType/Carry 左右臂/ArmsBlend/BodyWeightBlend/DefaultOrDrunkBehavior 等）。收鞘域差集与 run1 相同（源独有 10 + 命名差 1）。

**三连闭环**：① 1HM 分支滞留——run1 是"源 MT/候选 1HM"，本次镜像为"源 1HM/候选 MT"（滞留方向跟随候选的 replay 末态，与源无关）；② 缺 MLh 分支——源拔刀域 5 个 MLh 节点候选全无；③ 缺 WeaponAdjustment——两域 57 组快照（run1 33 + run2 24）一致复现。

### P2-V2 成立（域切换驱动变量清单完整）

拔刀→收鞘的跨视图 diff（req2 view2 call60）捕获 **11 词**，其中核心驱动 8 个（≤8，CON-A 可同步集）：

`iState`(94, INT 9→0)、`currentDefaultState`(113, INT 1→0)、`iRightHandEquipped`(179, INT 1→0)、`iLeftHandEquipped`(180, INT 9→0)、`iMagicEquipped`(117, INT 1→0)、`bMLh_Ready`(92, BOOL 1→0)、`bMRh_Ready`(93, BOOL 1→0)、`bIsInMT`(200, BOOL 0→1)；辅助：`IsAttackReady`(38, BOOL 1→0)、`bIdleBeforeConversation`(589, BOOL 0→1)、`TimeDelta`(53, REAL 时钟)。加 run1 的装备变化变量（`iRightHandType`(35)/`iLeftHandType`(60)/`weaponSpeedMult`(47)）即完整输入账本。全程 `truncated=0`。

### 对 C-G6-CON 立项的裁定

契约 §五准入三条全部达成：① P1 S1–S5 PASS（run1）；② 驱动机制识别完成（8 变量 ≤8 上限）；③ 设计约束（不写源图、不碰 NO-GO、不扩例外）为待评审项。**C-G6-CON v1 可以立项**：CON-A（状态同步）为首选方向——把上述 8 变量经既有 typed-setter 路径（`write_named_int`，C-G5-CAL D2 已验证的写法）在域切换时同步给候选；收敛判据 = 候选分支集跟随源（BRANCHES 四元组逐域相等，除 WeaponAdjustment 与 10 项结构差集——见 §九）。失败回退 = CON-B（发散拒绝回落 OAR）。
