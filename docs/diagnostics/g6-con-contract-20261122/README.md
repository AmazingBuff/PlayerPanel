# C-G6-CON v1：候选分支域收敛——状态同步修复（预注册契约，2026-11-22）

> **本文在实现前落盘，判据不看运行结果。** 准入依据 [C-G6 v1 §九/§十](../g6-contract-20261122/README.md)：P1 S1–S5 PASS、P2-V1/V2 成立、驱动机制识别完成（§五准入三条全部达成）。修复方向 = **CON-A（状态同步）**，失败回退 = CON-B（发散拒绝回落 OAR，另立）。

## 一、问题与假设

**问题**：保留候选克隆的分支域不随源切换——源在拔刀域（1HM+MLh，114 节点）↔ 收鞘域（MT，106 节点）间切换时，候选恒滞留其注入 replay 末态的域（97 节点），面板显示域错误姿态（C-G6 v1 run1/run2 三连实证）。

**修复假设（H-SYNC，运行前固定）**：候选的状态机分支选择由图变量驱动；把源侧已识别的域切换驱动变量同步进候选自己的 `variableValueSet` 后，候选状态机将自行激活/退出对应分支，其 activeNodes 分支集跟随源。

**对照假设**：
- **H-STRUCT**：分支激活还依赖同步集之外的机制（时间窗、事件、节点间连线），仅变量同步不足以切换 → CON-P1 不成立，转 CON-B 或扩展同步集（另立）。
- **H-CLIP-ONLY**：变量同步使 clip 层跟随，但状态机/blender 层结构性节点（`MT_BehaviorGraph` 等）仍缺 → clip 家族收敛成立而分支节点覆盖不全 → 按预注册的部分收敛记录，不判全过。

## 二、修复设计

### 2.1 同步集（预注册，10 变量）

来自 [C-G6 v1 §十](../g6-contract-20261122/README.md) 的运行证据，全部 BOOL/INT 型（typed setter 已支持）：

| # | 变量 | 类型 | 观测角色 |
| --- | --- | --- | --- |
| 1 | `iState` | INT | 域主开关（9=拔刀域 / 0=收鞘域） |
| 2 | `currentDefaultState` | INT | 默认状态机相位 |
| 3 | `iRightHandEquipped` | INT | 右手装备域（1/0） |
| 4 | `iLeftHandEquipped` | INT | 左手装备域（9/0） |
| 5 | `iMagicEquipped` | INT | 魔法装备域（1/0） |
| 6 | `bMLh_Ready` | BOOL | 左手魔法就绪（MLh 分支门） |
| 7 | `bMRh_Ready` | BOOL | 右手武器就绪 |
| 8 | `bIsInMT` | BOOL | MT 域标志（收鞘 0→1） |
| 9 | `iRightHandType` | INT | 武器类型（SCAR 变体/武器 idle 家族选择） |
| 10 | `iLeftHandType` | INT | 左手类型 |

**显式排除**：`TimeDelta`（每图时钟，永不同步）、`weaponSpeedMult`（REAL，typed-int setter 不支持）、`IsAttackReady`/`bIdleBeforeConversation`（辅助旗标，进观察不进同步集——避免把会话态噪声写进候选）。

### 2.2 同步机制

- **读**：每次 eligible 显示调用开始（驱动步进前），只读源图这 10 个词（按名解析，首用解析后缓存索引），构成 10 元签名。
- **判变**：签名 ≠ 上次已应用签名 → 触发一次同步事件。窗口首次绑定视为签名变化（初同步，覆盖"开包时域不匹配"）。
- **写**：仅写**候选**图，经既有 `write_named_int` 路径（C-G5-CAL D2 验证的 `SetGraphVariableBool/Int` typed setter，按候选侧 m_type 分派；BOOL 用 bool setter、INT 用 int setter）。10 变量逐个写、逐个记回读。
- **源侧只读**：同步全程不写源图、不解引用失效对象（复用 G6-A 的失效链；源图换代 → 签名状态复位为未同步，不跨图携带）。
- **日志**：每次同步事件一行 `G6 SYNC actor=… request={} view={} trigger=initial|change sig-old={:016x} sig-new={:016x} writes=iState:9>0:ok,…`（逐变量 old>new:ok|failed|skipped-unsupported）；另每次窗口绑定一行 `G6 SYNC ARM`。预算：**每进程 ≤32 次同步事件**，超出后记录 `sync-disabled budget` 并停用（重启才恢复）。
- **INI**：`[Experiment] CandidateDomainSync=true`（默认 off）；要求 `IndependentGraph=inject` + `DisplayPose=true`，否则记 `skip=gate` 不启用。重启生效。
- **实现位置**：新 policy 头 `domain_sync_policy.h`（变量名表、签名、判定规则，engine-free 可测）+ 独立图模块内的同步执行体；不改 `DisplaySession` 既有语义。

### 2.3 明确不做

- **不修** WeaponAdjustment 结构缺口（克隆无该节点，激活期结构差，C-G5-TOL 已接受其 5 轨残差）。
- **不修** 收鞘域源独有 10 项中的结构节点（Behavior00/02、IdleBFR、bIdleBeforeConversation 系等）——若同步后它们出现在候选（状态机自行激活），照实记录为正收益，不作为判据。
- **不扩** E1/E2 例外、不碰装载 teardown NO-GO、不新增源图写入。
- **不改** C-G6 v1 的对账仪器（ledger/BRANCHES/VARS 原样保留，作为本契约的验证工具）。

## 三、判据（预注册）

以下全部在上机日志离线判读；名称归一化规则：clip 名剥离 `.HKX` 后缀、`SCAR_1HM_Ready_Dummy\d+` 记为同族（两侧 Dummy 序号差异为已知形态）。

- **CON-P1（clip 家族跟随，主判据）**：同步开启下，**每个域状态 ≥2 个检查点、共 ≥2 个域**：候选 active clip 集合（归一化后）== 源 active clip 集合（归一化后）。即拔刀域候选出现 `1HM_Idle`+`MLh_Idle`+`SCAR_1HM_Ready_Dummy*` 家族且无 `MT_Idle`；收鞘域候选为 `MT_Idle` 且无 1HM/MLh clip。
- **CON-P2（旧域退出）**：每次域切换后的首个快照，候选不含**前一域**的任何 clip 类节点（滞留零容忍——这正是三连之①②的失败形态）。
- **CON-P3（写证据）**：`G6 SYNC` 行逐变量 `ok`；任何 `failed/skipped-unsupported` 逐条记录并归因（变量缺失/类型不支持/setter 失败），不静默。
- **CON-P4（源安全）**：会话内 `source-drift=none` 保持；G6-A 判据仍成立（ledger/DAC matched/park 恒等式）；同步只写候选（由实现评审静态确认 + 运行期无源侧异常佐证）。
- **CON-P5（P1 mini 回归）**：同会话以同步开启重跑 S1 mini（开合 ×2）+ S5 mini（F9 ×1）：`DAC restore matched`、park 恒等式、读档后 `unbound-retained` 语义不变。
- **CON-P6（稳定性）**：无 `[E]`（除既有 OAR-not-loaded）、无崩溃；崩溃按停点处理。
- **附加记录（不判过）**：分支节点覆盖比（新域内候选新获得的 state-machine/blender 节点数 / 源该域节点数）；WeaponAdjustment 与结构差集残差；用户对显示姿态域正确性的目视确认。

**判定**：CON-P1 + P2 + P4 + P5 + P6 全过 → **CON-A 成立**，G6-B 收敛达成（限定：同步集 10 变量、两域对照、结构差集残差显式保留）；P1 成立而分支节点覆盖不全 → H-CLIP-ONLY，记部分收敛，主文档 G6-B 行按"clip 家族收敛成立"登记；P1 不成立 → H-STRUCT，停（转 CON-B 评审，不在本契约内放宽判据）。

## 四、停点与负路径

- setter 失败的变量本进程停用（不重试、不换写法）；同步预算耗尽 → 停用并记录。
- 源图换代/读档：签名状态复位，不携带；失效链沿用 G6-A。
- 源变量表不可读：该调用跳过同步（记 `unavailable`），不猜。
- 崩溃：保留完整日志与匹配 PDB，不同进程重试。
- 任何判据不满足不做"调参重试"；修复参数（同步集/时机）变更须另立修订。

## 五、上机协议（CON run 1）

1. 环境：AE 1.6.1170、Save116、`AE-Track2` 装备族；一次会话一个 DLL；构建标签建议 `v124-g6-domain-sync`；DLL/PDB/INI 三方哈希互核（v115 教训）。
2. INI：在 v123 候选档基础上只加 `CandidateDomainSync=true`（其余键不动）。
3. 场景序列（全程背包开着按 F7）：
   - **T1 拔刀域初同步**：拔刀持武器 idle ≥3 s → 开包 → F7 ×2 → 关包。
   - **T2 收鞘切换**：收鞘 idle ≥3 s → 开包 → F7 ×2 → 关包。
   - **T3 再拔刀**：拔刀 idle ≥3 s → 开包 → F7 ×2 → 关包。
   - **T4（可选）魔法域**：装备魔法并持出 idle ≥3 s → F7 ×2。
   - **T5 P1 mini 回归**：开合 ×2 + F9 ×1（读档后开包一次）。
4. 目视：T1/T2 的显示姿态域是否正确（持武器待机 vs 收鞘站立）——一句话记录。
5. 回传：完整日志改名 `CharacterPanel-g6con-<日期时间>.log` + INI 哈希 + `[E]`/异常一句话。
6. 判读按 §三；上机不做判定。

## 六、边界

- 单运行时 AE 1.6.1170；本契约只验**分支域收敛**（clip 家族层），不验逐骨姿态等价、不触碰门 4 `tau_*`、不关闭 K5-5/K2-C1/A17。
- 同步集变更（加变量/去变量）= 契约修订，不做 INI 侧临场改判。
- MLh 域（T4）为可选对照：若玩家无魔法条件，CON-P1 在两域（拔刀/收鞘）成立即判过，MLh 记 not-covered。
- `G6 SYNC` 行与既有行系列互不挤占预算（独立前缀，无共享行预算）。

## 变更记录

- 2026-11-22：契约落盘（同步集与判据来自 C-G6 v1 run1/run2 的运行证据；实现前，判据不看运行结果）。

## 七、run 1 判读（2026-11-22 23:03–23:07，日志 [CharacterPanel-g6con-20261122.log](../CharacterPanel-g6con-20261122.log) SHA-256 `E427897F…6DDD61`；构建 `…v124-g6-domain-sync-20261122-32432c179e-…` 在位、INI `c062dc0d…`、键面 `CandidateDomainSync=true` 在案）

### 预注册判据判定

| 判据 | 证据 | 判定 |
| --- | --- | --- |
| CON-P3 写证据 | 6 × `G6 SYNC ARM` + 6 × `G6 SYNC`（全部 `trigger=initial`——每 request 的首个步进调用即同步），**30/30 变量写 `ok`**、零 `failed/missing/unsupported`；签名域语义自洽（r1/r3 拔刀 `b5a65c60…`、r2 收鞘 `612ce9cc…`、r4–r6 `2937e9ca…`） | **成立** |
| CON-P1 clip 家族跟随（主判据） | 候选 12 快照（r1 ×3 views、r2 ×3、r3 ×2+）BRANCHES 四元组**全程恒定** `b1hm=1 bmt=10 bmlh=0 walked=97`（MT 域）；源正确切换 拔刀 114（b1hm=10、bmt=0、**bmlh=5**）↔ 收鞘 106。**0/3 域跟随** | **不成立** |
| CON-P2 旧域退出 | T1/T3（源拔刀域）时候选仍含 `MT_Idle` clip（滞留即残留） | **不成立** |
| CON-P4 源安全 | `phase=retained source-drift=none`；DAC **7/7 matched**；park 恒等式 **ALL OK**（14 行==终值 14，13 处阈值告警 1 行符合设计）；E1 `candidate=retained` 全程、graph `0x25547b2ce00` 恒定；同步只写候选 | **成立** |
| CON-P5 P1 mini 回归 | T5：panel-close ×6 全 matched、F9 ×1 后 `held-source=unbound` + `window=rejected reason=unbound-retained` ×1 + request 重计数——G6-A 判据在同步开启下全部保持 | **成立** |
| CON-P6 稳定性 | `[E]` 仅既有 OAR-not-loaded 1 条；无崩溃 | **成立** |

目视结论未回传（不影响机械判定路径——P1 已 FAIL）。

### 裁定：H-STRUCT 成立，CON-A（仅变量同步）不成立，本契约按 §三判停

变量写成功（P3）但候选状态机不响应：12 快照零域跟随、写后 120 步内分支集不变。**机制指向事件驱动**——与项目既有知识自洽：C-G5-CAL D2 注释明示候选的装备状态转换由 58377 事件派发驱动、变量是伴随态（"the event alone still selects H2H_Equip"）；G3-B 的全部状态机推进（equip/unequip 段）均为事件注入取得。仅同步终值变量不触发转换链。

按契约 §三与 §四：不放宽判据、不调参重试；CON-B 评审或修订另立。**本次非空跑**：CON-P3 证明同步机制（写路径/签名检测/预算/ARM 语义）工作正常可复用；CON-P4/P5 证明同步写候选对 G6-A 生命周期判据零干扰；机制归因从"变量驱动"收敛到"事件驱动"，为下一轮修复（变量+事件同步，或 CON-B）提供直接依据。

### 下一轮选项（用户裁定）

1. **CON-A'**：变量同步（本契约机制原样保留）+ **域切换事件注入**（复用 G3-B 的 resolve+dispatch 路径，拔刀/收鞘各一个预注册事件名——需先取证源图拔刀/收鞘瞬间实际接收的事件名，SourceEventCapture=on 一次窗口即可）——新契约 C-G6-CON v2。
2. **CON-B**：分支集发散检测 → `window=rejected reason=branch-divergence` 新原因 → 回落 OAR（保守，显示不撒谎但不修复）。
