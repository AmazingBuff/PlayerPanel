# C-G5-G4 v1：门 4 配对/比较协议预注册（2026-11-15 草案，待评审）

**状态（2026-11-16）：FROZEN（限定范围；见 §十九）。**（2026-11-15 预注册评审通过。） 用户于 2026-11-15 批准本协议（§十三决策 1）与工具实现 + dry-run 授权（§十三决策 2）。本文档冻结 [G5 v2 门 4](../../independent-behavior-graph.md#g5-draft) 要求的**配对/比较协议结构规则**：配对资格、配对键、度量定义、分母、时钟/相位规则、判定/停点/回传格式与预算。**姿态已由 CAL1-B 填充（§十六），`epsilon_t`/`dt_exclude` 已由 TIME v1 填充（§十七），99/17 覆盖已由 COV v1 填充**；时钟轴判据仍 PENDING-CAL，CAL2 已完成、完整 CAL1 未完成，按 [CAL v1 §九](../../independent-behavior-graph.md#c-g5-cal-v1) 的顺序继续；**槽位填充 + 联合证据验收 = 门 4 完整冻结**，冻结必须完成于任何正式数据采集之前（G5 v2 §二门 4/门 5 顺序）。本预注册不授权实现、构建、部署、上机或任何裁定。

**前置阅读**：[Track 2 预注册 §十九/§二十](g5-track2-20261114/README.md)（C 分组账本与收官裁定）、[K6 计划 §三/§六/§十](g5-k6-plan-20261114/README.md)（K5 仪器范围与记录字节布局）、[CAL v1](../../independent-behavior-graph.md#c-g5-cal-v1)（度量/统计/控制/预算 canonical 定义）、[K3 收官](../../independent-behavior-graph.md#g5-cal-k3-closure)（跨侧逐代配对的裁定）、[E3 v2](../../independent-behavior-graph.md#c-g5-e3-v2)（输入序数配对先例）、[2026-11-15 会话总结 §四](../session-summary-20261115-track2.md)（本工作包的入口指定）。

## 一、目标、范围与不覆盖

| 项 | 交付 | 不覆盖 |
| --- | --- | --- |
| 配对协议 | 相位括号配对的完整结构规则（资格/键/钟/边界/记账），两侧对称、独立于 pose 误差 | 同刻逐代配对（[K3 收官](../../independent-behavior-graph.md#g5-cal-k3-closure)裁定其须另立"源图并发运行"契约；本协议明确不开启，见 §3.1） |
| 比较度量 | `E_p`/`E_r`/`E_s`/`E_t` 定义与统计规则（引用 CAL v1 §六/§七，不重定义、不另设 epsilon） | 数值容差（PENDING-CAL，§五） |
| 分母 | 每对 116 轨全量分母 + 记录级对账；reference/default-fill 轨保留；候选缺骨/通道不缩分母 | 层/权重/混合判据（K5-5 缺项阻塞，§六） |
| 正式运行设计 | 门 5 采集的运行协议（零构建、Track 2 档、Run 1/Run 2 协议逐字） | 本协议不授权采集；正式运行在门 4 完整冻结之后 |
| 判定/停点/回传 | G4P 判据、停止规则、回传工件格式 | G5 等价裁定（§六阻塞项未清前不可启动） |

**明确不做**：不产生 `tau_p/tau_r/tau_s/epsilon_t`；不改产品代码、不改 K5 仪器、零构建（v103 复用）；不改写 Track 1/Track 2 任何历史裁定；不把 Track 2 Run 1/2 工件充当正式样本或校准样本（对齐 CAL v1 §二"旧数据只用于识别缺项"）；不做同刻逐代配对；不做姿态等价或 G5 判定；不删帧、不按误差选样、不做跨运行墙钟/TSC 对齐。

## 二、输入材料与资格继承

| 材料 | 用途 | 状态 |
| --- | --- | --- |
| [Track 2 §19 C 分组账本](g5-track2-20261114/README.md#十九c-分组跨侧配对资格账本track-2-收官交付) | 配对资格规则的原型：键 = `data FNV` + 裸名族；同名多实例按进程内 stable-id 消歧；IDUN 记账排除 | 已登记，六族全 PAIRED |
| Track 2 Run 1/Run 2 工件（CPK5 bin / JSON 账本 / 日志） | **协议设计输入材料**：钟族可行性、dry-run 对象（§八） | 已归档（§十七/§十八），姿态分量未被消费，本批继续不消费 |
| [K5 仪器（v103，K6 关闭）](g5-k6-plan-20261114/README.md#十三k6-关闭裁定) | 正式运行的采集仪器，零改动复用 | 资格成立（K5-1..8 两侧 PASS） |
| [K5-5 缺项清单](g5-k6-plan-20261114/README.md#十一v103-run-1候选侧记录2026-11-14-22002201) | 门 1 未闭合子项，随 [Track 2 §五转移声明](g5-track2-20261114/README.md#五层权重混合资格转移声明track-1--track-2)带入 | 阻塞清单（§六） |
| [CAL v1 §六/§七](../../independent-behavior-graph.md#c-g5-cal-v1) | 度量、统计、容差推导的 canonical 定义 | 引用；本协议不新增任何 epsilon 或阈值 |

**资格继承规则**：配对资格（族 PAIRED / IDUN 排除 / 出集处置）在每次正式运行上**由该次运行自己的 K5 对账重导出**；Track 2 §19 账本是规则原型与协议设计输入，不直接转移为正式运行资格。正式运行任一侧 K5-1..8（Track 2 集合，`--track2` 对账）失败 → 该轮 INVALID，不进配对。

## 三、配对设计（核心）

### 3.1 设计决策：相位括号配对，不做同刻逐代配对

[K3 收官](../../independent-behavior-graph.md#g5-cal-k3-closure)已证：`IndependentGraph=inject` 模式下候选驱动期间源图暂停（`source-drift=none`、源姿态前后全等），**不存在同一时刻的源 generation 可供配对**；若做同刻逐代配对须另立"源图并发运行 + 候选驱动/回放"的实验设计契约。

| 备选 | 裁定 | 理由 |
| --- | --- | --- |
| 同刻逐代配对（并发源运行） | **未选，维持另立契约状态** | 需要新 hook/仪器与新的生命周期审计；与"每进程一张保留候选"边界的关系未审；现有仪器不支持 |
| **相位括号配对（本协议）** | **选定** | 配对所需元数据（逐 clip `data FNV`/stable-id/**local_time_bits**/激活窗口）两侧已在 K5 工件落盘（[K6 §10.3](g5-k6-plan-20261114/README.md#103-字节布局pack1全部-static_assert-到位)）；零新代码；规则可完全离线复核 |
| 墙钟/TSC 对齐 | **禁止**（继承 [E3 v2 P5](../../independent-behavior-graph.md#c-g5-e3-v2) 原义） | `tsc`/`elapsed_raw` 只作运行内排序，跨运行不可比 |

配对语义（预先声明）：**配对单位 = "同一已配对动画资源（`data FNV` 两侧相等）在两侧的同一播放相位（clip `localTime`）上各自的完成边界输出"**。与游戏时刻无关；两侧行使同一冻结链、同一受控初态（[K2-C1/C2 登记](../g5-k2c1c2-20261114/README.md)），相位对应即本设计下可得的输入/时间元数据对应。每对的时间偏移 `Δt_clock` 逐对记账，其姿态误差归因不由本协议消化（§五槽位）。

### 3.2 配对资格（记录级）

逐记录（两侧同规则）：

1. 记录所携**全部可读** `(裸名族, data FNV)` ∈ 该次运行重导出的 PAIRED 族账本；任一可读身份出集 → 该记录按 K5-7 现行规则处置（候选出集 = INVALID；源出集且裸名 ∈ 已声明栈名集 = 声明缺口登记），不由配对规则吸收。
2. 记录含任一 **IDUN 目击**（`K5_Flag_Identity_Failed`，FNV 列 = 0）→ **整条排除出配对**，单独记账（继承 [Track 2 §17/§19](g5-track2-20261114/README.md#十七run-1候选侧-k5-姿态采集2026-11-15-0022pass含一项-identity-unreadable-记账) 的 C 规则）。
3. 记录 `two_read_equal != 1` 或 `track_count != 116` → 不进配对（该轮已在 K5-2/K5-3 判 INVALID，此处冗余守卫）。

### 3.3 钟族选择（记录级，两侧同一优先级表）

每条合格记录按以下优先级取**配对钟**（取首个活动族）：

| 优先级 | 钟族 | 覆盖的相位语义 |
| ---: | --- | --- |
| 1 | `1HM_Equip` | 装备过渡（含与 idle 栈的重叠混合带） |
| 2 | `1HM_Unequip` | 卸下过渡 |
| 3 | `1HM_Idle` | 纯 idle 段（首选钟） |
| 4 | `MT_Idle` | idle 回退 |
| 5 | `MLh_Idle` | idle 回退 |
| 6 | `SCAR_1hmReadyDummy` | idle 末位回退（dummy localTime 语义在 dry-run 中核实，见 §八） |
| — | 全部缺失 | 记账 `no-clock`，不配对 |

设计依据：过渡相位由过渡 clip 的 `localTime` 直接对齐（两侧同一资源、同一 clip-start 语义）；纯 idle 段没有共同相位锚，用 idle 族 `localTime`（同一资源的播放相位）对齐——游戏时刻无关，[K3 收官](../../independent-behavior-graph.md#g5-cal-k3-closure)的"无同刻源 generation"不构成障碍。

### 3.4 域内配对（nearest-clock）

- **配对域** = 同侧同钟族的合格记录集合；候选侧域与源侧域一一对应（按钟族）。
- **实例窗**：同侧同名族多实例按 stable-id（node/binding 指针 + binding-index，同侧内可比；指针跨进程不可比，只做同侧分窗）分窗；窗内按记录序数（候选 `completed`、源 `seq`）排序；`local_time_bits` 出现非增点 = 循环回绕边界，切环后环内钟值单调。
- **括号规则**：候选记录钟值 `x` 在同域源钟序列中取 `|c − x|` 最小者配对；平手（等距）取源 `seq` 较早者。**最近钟值是预先声明的元数据规则，与 pose 误差无关**（对齐 [G5 v2 §三](../../independent-behavior-graph.md#g5-draft)"配对只按预先声明、独立于 pose 误差的规则进行"）。
- **方向**：候选 → 源单向（候选是被测对象，每条候选合格记录恰得一个源伙伴）；源记录可承载多个候选伙伴。
- 逐对输出：钟族、`x`、`c_k`、`Δt_clock = |x − c_k|`、双侧活动族集合差（多/少族逐对登记，作为被测差异保留，**不因此排除配对**——候选缺层/多层是应检出的差异，不是删帧理由，[G5 v2 §三](../../independent-behavior-graph.md#g5-draft)）。

### 3.5 边界与记账（全部预先固定）

| 类别 | 处置 |
| --- | --- |
| `no-clock`（全部钟族缺失） | 记账，不配对 |
| `no-partner`（同域源钟序列为空） | 记账 `missing`，**不取邻域、不用最近邻跨域补**、不用另一轮替代（[E3 v2 §三](../../independent-behavior-graph.md#c-g5-e3-v2) 原义） |
| `edge-pair`（`x` 越过全部源钟，取最近端点） | 配对成立 + 逐对 flag 记账 |
| `wrap`（回绕切环） | 环结构逐域登记（环数、每环记录数） |
| `IDUN` | 整条排除（§3.2），记账 |
| 声明缺口（源侧裸名 ∈ 已声明栈名集） | 按 K5-7 现行规则登记；若该记录其余资格完好则仍可配对，缺口事实随对保留 |
| 对账恒等式 | `合格记录 = paired + no-clock + no-partner + IDUN-excluded + 出集处置`，逐侧逐域成立；任何未归属记录 = 账本不完整 |

### 3.6 语义声明与禁止项

1. **`localTime` 是比较语义，不是墙钟对齐**：钟值取自同一已配对资源（`data FNV` 相等）的记录字段，属"输入/时间元数据规则"（[G5 v2 §三](../../independent-behavior-graph.md#g5-draft)）；跨运行 `tsc`/`elapsed_raw` 对齐仍禁止（[E3 v2 P5](../../independent-behavior-graph.md#c-g5-e3-v2) 原义不变）。
2. **`Δt_clock` 不被消化为姿态噪声**：候选侧 dt 网格（1/60 固定驱动）与源侧自然帧率不同，`Δt_clock` 的分布如实报告；其对姿态误差的归因/传播规则是 §五 的 PENDING-CAL 槽位，本协议不得以任何固定偏移或上限吸收它（对齐 [CAL v1 §七](../../independent-behavior-graph.md#c-g5-cal-v1)"时间不自动变成姿态噪声"）。
3. **混合带语义**：过渡钟族记录（E1/U1）天然含 idle 栈重叠；纯 idle 记录可能一侧为纯 clip、另一侧带混合——族集合差逐对登记，差异归被测差异。
4. **禁止**：按 pose 误差挑邻帧/换伙伴；事后收缩钟族或域；删帧/换资源/偏移时间；把配对失败记为零误差。

## 四、比较度量与统计（引用，不重定义）

| 项 | 定义 | 来源 |
| --- | --- | --- |
| 位置 `E_p` | 相同骨父级 parent-local 三分量欧氏距离，double 累加；root 原始轨道与 extracted motion 贡献不剥离 | [CAL v1 §六](../../independent-behavior-graph.md#c-g5-cal-v1) |
| 旋转 `E_r` | 归一化后 `1 − |dot(q_a,q_b)|`（clamp 只处理舍入）；诊断角 `theta = 2·acos(c)` | 同上 |
| 缩放 `E_s` | 原生 XYZ 三分量最大绝对差 | 同上 |
| 时间 `E_t` | `Δt_clock` 分布按域/子段报告（单位秒，钟值 = clip localTime） | 本协议 §3.4（归因规则待 CAL） |
| 统计 | 按 域/骨/通道 分组；count/min/median/P95/P99/max；升序 nearest-rank；全样本保留、不删离群、不取最好一轮 | [CAL v1 §七](../../independent-behavior-graph.md#c-g5-cal-v1) |
| 表示界 Q / 重复性界 N | 由 CAL 控制产出推导；本协议不自行估界 | 同上 |
| 无效值 | 非有限值/零范数单列归属（候选缺陷与采集无效分开），不进误差分布、不计零误差 | [CAL v1 §六](../../independent-behavior-graph.md#c-g5-cal-v1) |

**分母**：每对 116/116 轨（[K5-3 已证](g5-k6-plan-20261114/README.md#六k5-运行协议与预注册判据实现评审后冻结执行细节) skeleton 分母）；reference/default-fill 轨保留；候选缺骨/通道不缩分母（[G5 v2 §三](../../independent-behavior-graph.md#g5-draft)）。逐对逐骨明细全量保留（§十一预算）。

## 五、数值容差槽位与门 4 完整冻结条件

| 槽位 | 内容 | 填充来源 | 当前状态 |
| --- | --- | --- | --- |
| `tau_p` / `tau_r` / `tau_s` | 姿态容差（`Q+N` 推导、旋转角空间合成、向外舍入） | [CAL v1 §七](../../independent-behavior-graph.md#c-g5-cal-v1)（CAL1-B 产出；§十六） | **CAL1-B 已填（限定范围；§十六）** |
| `epsilon_t` | 时间量化界（输入锚点→完成输出→读取延迟区间） | [C-G5-TIME v1](../g5-time-20261115/README.md#time-results) | **`0.0333960` s（TIME-1..7 PASS；§十七）** |
| 时间偏移处置规则 | `Δt_clock` 与配对有效性的判定规则（含 `epsilon_t` 是否/如何传播为运动误差的独立证明） | C-G5-TIME v1 §2.3 | **`dt_exclude=true`，阈值 `epsilon_t`；2% edge-only；dry-run 0/247 排除** |
| 时钟轴判据 | 候选/源 clip 实例速率与相位时长的比较规则（离散判据 + 数值允差） | [C-G5-G4-CLOCK v1](../g5-g4-clock-20261116/README.md#八实现与-dry-run-结果2026-11-16) | **CLOCK_READY（G4C-1..6 PASS；§十八）** |

**门 4 完整冻结条件**（全部满足才称"已冻结"，才授权门 5 正式采集）：

1. 全部槽位由 CAL 输出填充，推导过程可独立复算（CAL v1 §七 CAL6）；
2. [CAL v1 §九](../../independent-behavior-graph.md#c-g5-cal-v1)联合证据验收通过（映射/计数/控制/校准证据齐备）；
3. 本协议 G4P-1..G4P-6、G4P-8、G4P-9 的结构判据在 dry-run 与正式运行资格上成立；
4. §六阻塞清单的处置明确（闭合或显式带缺项冻结，并如实登记其对 G5 裁定的阻塞）。

**当前状态（2026-11-16）：FROZEN（限定范围）。** 姿态 `tau_p/tau_r/tau_s`（CAL1-B）、`epsilon_t=0.0333960` 与 `dt_exclude`（TIME v1）、99/17 覆盖（COV v1）、时钟轴（CLOCK v1）均已填；CAL v1 §九联合验收以 `CALIBRATED (limited)` 通过；K5-5/K2-C1/A17 以显式缺项冻结。**门 5 采集已授权，但未运行；G5 仍因层/权重/混合等阻塞。** 见 [门 4 冻结记录](../g5-gate4-freeze-20261116/README.md)。

## 六、层/权重/混合轴：缺项阻塞登记（不能因读不到便删出 G5）

[K5-5 缺项清单](g5-k6-plan-20261114/README.md#十一v103-run-1候选侧记录2026-11-14-22002201)（`hkbBlendingBehavior` 等无 CommonLibSSE 布局与 VTABLE 常量 → 逐骨/通道权重与 blend/mask/additive 语义不可观测）随 [Track 2 §五转移声明](g5-track2-20261114/README.md#五层权重混合资格转移声明track-1--track-2)带入本协议：

- **层/权重/混合的离散/数值判据在本协议中不冻结**——不可观测字段不得发明偏移补齐（反模式 12 原义），缺项清单之外再发现未声明不可观测 → 协议执行 INVALID。
- 因此 **G5 等价裁定（要求全部有效贡献层/逐骨权重/混合语义）持续阻塞**：即使姿态轴与时间轴全部通过，也只能报告"姿态轴局部证据"，**不得升格为 G5 通过**（[G5 v2 §六](../../independent-behavior-graph.md#g5-draft)裁定条件含层/权重全覆盖）。
- 缺项清单的后续闭合路径（CAL2 原生控制、或另立布局审计范围）独立推进，不影响本协议姿态轴的冻结与执行。

## 七、正式运行设计（门 5 前置；本协议不授权采集）

正式运行（门 5"独立于校准的新保留运行"）在门 4 完整冻结后采集；协议如下，届时逐字执行：

| 侧 | 运行档 | 协议 |
| --- | --- | --- |
| 候选 | k5-inject 档（`76f2bebb…`） | Save116；panel 窗口一次，v96a 链逐字：inject → 同帧 5 写 prime + `WeapEquip` → equip 120/120（`fixed-equip-budget`）→ settle（target 87/settled 101）→ `Unequip` id=144 → unequip（`state-12-to-0`）→ 关窗 |
| 源 | k5-source 档（`21bd938b…`） | Save116；liveness 直采（`SamplingLiveness` + K5；无 F7 窗口，[Track 2 §十八执行偏差登记](g5-track2-20261114/README.md#十八run-2源侧-k5-姿态采集2026-11-15-0031pass) 为准）；用户自然拔剑/收剑 |

- 每侧一次会话一次窗口；重复/重跑须登记原因与新批次，不得静默替换（[CAL v1 §五.1](../../independent-behavior-graph.md#c-g5-cal-v1) 重复规则精神）。
- 每轮先判 K5-1..8（Track 2 集合）；任一失败 → 该轮 INVALID，不进配对。
- 身份登记：DLL/PDB/INI/存档/manifest 哈希；`AE-Track2` 档启用集与 modlist 哈希与 Track 2 登记比对，不符即停。
- **零构建**：正式运行复用 v103（DLL `8c3857a3…`）与现有运行档，不改任何代码/INI 键。

## 八、离线执行者与协议可执行性验证

- **新工具 `tools/gate4_pair.py`**（与 `k5_reconcile.py` 分离：对账与配对是两个关注点，后者不改）。输入 = 候选 CPK5 v1 bin + 源 CPK5S v2 bin（+ 各自 JSON 账本）；输出 = 配对 manifest（JSON）+ 逐对逐骨误差明细 + 文本摘要（§十一预算）。CLI、字节解析、输出 schema 在**实现预注册**中冻结后才实现；工具版本 + SHA-256 随每次使用登记。
- **协议可执行性验证（dry-run，metadata-only）**：工具实现后，先对 Track 2 Run 1/Run 2 工件执行一次只读验证——**只解析元数据（钟族/钟值/实例窗/记账类），不解码、不聚合任何 pose 分量**；输出不含姿态数值。目的：证明 §3 规则可执行（域非空、括号全覆盖、记账闭合）、并在冻结前暴露规则歧义（如 dummy `localTime` 恒定、声明缺口记录的域归属）。
- **dry-run 预期锚点**（自 §十七/§十八已登记元数据派生，供核对，非判据）：候选合格记录 247（E1 96 + idle 22 + U1 129，IDUN 2 条排除）；源侧域记录数上界 E1 ≤ 72 / U1 ≤ 118 / idle ≤ 875（IDUN 1 条落入其一，dry-run 报告确切落点）。
- dry-run 结果只登记"协议可执行/歧义清单"，**不产生任何姿态结论，不预览误差**；修订配对规则须升本协议版本并重新 dry-run（已看元数据不得按新规则回填为预注册样本——对齐 CAL v1 §七"已看过的数据"纪律，此处延伸到元数据）。

## 九、判据（预注册，不得事后放宽）

正式数据上的裁定在门 4 完整冻结后执行；G4P-1..6、8、9 可在每轮资格核验时先判，G4P-7 与最终姿态裁定须 `tau_*`/`epsilon_t` 填充后判。

| # | 判据 | 通过条件 | 否则 |
| --- | --- | --- | --- |
| G4P-1 运行资格 | 两侧各自 K5-1..8 PASS（Track 2 集合）；配对资格账本重导出（六族 PAIRED、IDUN/声明缺口记账） | 全过 | 该轮 INVALID |
| G4P-2 账本闭合 | §3.5 对账恒等式逐侧逐域成立；未归属记录 = 0 | 全过 | 账本不完整，停 |
| G4P-3 括号覆盖 | 每条候选合格记录恰得一伙伴（含 edge-pair flag）；`no-partner` = 0 或逐条解释登记 | 全过 | 停 |
| G4P-4 配对确定性 | 同一输入工件重复执行两次，manifest 逐位一致（规则纯函数性） | 一致 | 工具缺陷，停 |
| G4P-5 元数据有效 | 全部钟值有限、实例窗/环结构无歧义、声明缺口处置符合 K5-7 现行规则 | 全过 | 停 |
| G4P-6 记账完整 | IDUN/no-clock/edge-pair/wrap/族集合差全部入账；dry-run 锚点与正式运行重导出账本一致（计数规则相同） | 全过 | 停 |
| G4P-7 时间与姿态裁定 | 逐对 P99（99 轨）`E_p/E_r/E_s` 按冻结容差判；A17 单列描述、不判；时钟轴按冻结判据；`Δt_clock` 按冻结规则 | 全部槽位已填充且判据通过 | 槽位未填充 = 不得裁；判负 = 被测差异如实报告 |
| G4P-8 无效值守卫 | 非有限值/零范数单列归属；候选缺陷与采集无效分开，均不进误差分布、不计零误差 | 全过 | 停 |
| G4P-9 工件完整 | manifest/明细/摘要可读、SHA-256 匹配、预算未超、工具版本登记 | 全过 | 整轮证据不足 |

**裁定语义**：G4P 全过 = 本协议下的**配对/比较证据成立（限定 Track 2 环境、本协议钟族/括号规则范围）**；仍须与层/权重/混合轴（§六）及 [G5 v2 §六](../../independent-behavior-graph.md#g5-draft)全量条件合并才能形成 G5 级裁定；任何单轴结果不升格为 G5 通过。

## 十、停止规则

任一即停止并保留原因与已收计数，不拼接窗口、不续裁、不降级成"部分通过"：任何 `[E]`、崩溃、需要写源图或未知 ABI；存档/模组/DLL/INI 身份与登记不符；reset/close/PreLoadGame/NewGame、来源或 generation 替换；K5-1..8 任一失败；G4P-2/3/4/5/8/9 任一失败；dry-run 暴露未预先登记的规则歧义。修订须另立预注册版本（本协议升版）；已看数据（含 dry-run 元数据）不按新规则回填。

## 十一、预算与工件

| 工件 | 格式 | 上限 | 超限 |
| --- | --- | --- | --- |
| 配对 manifest | JSON（逐对：钟族/钟值/`Δt_clock`/伙伴序数/记账类/族集合差） | ≤ 1 MiB | 整轮无效，保留已收计数 |
| 逐对逐骨误差明细 | 紧凑二进制或文本（247 预期对 × 116 轨 × E_p/E_r/E_s） | ≤ 8 MiB | 同上 |
| 文本摘要 | `G4P SUMMARY` 行 | ≤ 200 行 | 同上 |
| 身份清单 | DLL/PDB/INI/存档/manifest/工具 SHA-256 | ≤ 0.1 MiB | 同上 |

正式运行的采集预算沿用 K5 既有 cap（候选 384×5,664 B / 源 1,400×5,652 B）与 [CAL v1 §五.7](../../independent-behavior-graph.md#c-g5-cal-v1) 身份/时间/对账桶；本节预算只约束**离线分析工件**。

## 十二、实施顺序与提交拆分

1. **本协议评审批准**（用户）→ 状态改"预注册（评审通过）"，同步主文档指针 / [README](../../README.md) / [history](../../history.md) / [CHANGELOG](../../CHANGELOG.md)。
2. **离线工具实现预注册**（CLI/schema/解析规则冻结；评审后实现）。
3. 实现 + **dry-run**（Track 2 Run 1/2 工件，metadata-only）→ 可执行性登记（含歧义清单与规则修订，如有）。
4. （并行工作包）**CAL2 三进程/120 代协议预注册与运行**（独立于门 4，[会话总结 §四.3](../session-summary-20261115-track2.md)）。
5. **CAL1 原生校准**（`tau_*`/`epsilon_t` 产出；CAL v1 §九顺序）。
6. **门 4 完整冻结**（§五条件逐项核验 + 联合证据验收）。
7. **门 5 正式运行**（§七协议，上机）→ G4P 判定 → 回传。

①–⑥ 零游戏代码、零构建、零上机；⑦ 为上机。提交拆分：`docs(g5-gate4): 预注册门 4 配对/比较协议（评审通过版）` → `feat(tools): gate4_pair 实现预注册与实现` → `docs(g5-gate4): dry-run 可执行性登记` → 后续按 ④–⑦ 各自登记。

## 十三、待用户决策事项

| # | 决策 | 影响 |
| --- | --- | --- |
| 1 | 本协议评审与批准（重点：§3.1 相位括号配对方向、§3.3 钟族优先级、§3.4 nearest-clock、§五槽位结构、§六阻塞登记）——**已批准（2026-11-15）** | 已解锁 §十二 ①–③ |
| 2 | 离线工具实现 + dry-run 授权（`tools/gate4_pair.py` 新工具，零游戏代码）——**已批准（2026-11-15）** | 实现预注册与 dry-run 的前置 |
| 3 | （会话外既挂）78 提交 push 与否；工作区 AGENTS.md CharacterPanel 条目更新与否 | 与本协议无耦合 |

`Documentation Impact: update`：批准后同步主文档账本行与文首状态行、README、history、CHANGELOG；不可回写旧运行结论。本文仅依据本地已审计边界规定研究要求；未来若涉及新 API/ABI/hook（如源图并发契约），必须另行补齐兼容性与来源证据。

## 十四、实现预注册（2026-11-15，批准后冻结；实现前零采集）

本节冻结离线工具 `tools/gate4_pair.py` 及其测试的实现范围。所有变更先于任何采集；采集开始后本节不改（变更 → 本协议升版）。**纯 stdlib、零第三方依赖、零网络、只读输入工件。**

### 14.1 CLI（冻结）

```
python -I tools/gate4_pair.py --candidate <cpk5.bin> --source <cpk5s.bin> --out-dir <dir>
        [--candidate-json <ledger.json>] [--source-json <ledger.json>] [--tolerances <json>]
```

- **默认 = metadata-only（dry-run 语义）**：track 区域字节不进入任何数据结构；manifest 写 `pose_decoded=false`；不产误差明细。
- **`--tolerances` 是 pose 解码的唯一开关**：JSON = `{"tau_p":>0, "tau_r":>0, "tau_s":>0, "epsilon_t":>0, "dt_exclude":bool}`（全有限）。提供时解码 pose 并计算 `E_p/E_r/E_s` 与超容差机械计数；`dt_exclude=true` 时 `dt_clock > epsilon_t` 的对排除出误差分布并记账（§五槽位结构）。**本会话不执行该模式**；仅 `--self-test`（见 14.5）用合成 fixture 覆盖其算术。工具不输出 G5 级裁定语——只产出机械计数与分布，裁定语由运行记录按 G4P-7 引用工具输出书写。
- `--candidate-json`/`--source-json`（可选）：断言 bin `record_count` == JSON 账本 `recorded` 计数，不一致 → 错误退出（G4P-9 交叉核对）。
- 退出码：0 = 成功；2 = 数据判 INVALID（K5-7 出集等）；3 = 解析/守卫失败。

### 14.2 解析（冻结；与 [k5_reconcile.py](../../../tools/k5_reconcile.py) 已验证布局逐字一致）

| 项 | 布局 |
| --- | --- |
| 候选 header | `<4sHHIII>` magic/version/header_size/record_capacity/track_capacity/record_count；identity 文本 @20..header_size |
| 候选记录 5,664 B | meta `<IIIIIIQ>`（segment/completed/commanded/track_count/two_read_equal/clip_count/elapsed_raw，32 B）+ 8×K5_CLIP + 116×40 track |
| 源 header | `<4sHHIIIIII>`；record_size 字段须 == 解析记录尺寸 |
| 源记录 5,652 B | meta `<QIHHBBBB>`（tsc/seq/track_count/clip_count/path/two_read_equal/flags/pad，20 B）+ 8×K5_CLIP + 116×40 track |
| K5_CLIP 124 B | `<QQII4s64sIQQQI>` node/binding/binding_index/**local_time_bits**/mode+pad/name[64]/duration_bits/**data_fnv**/blockoff_fnv/map_fnv/map_count |
| 名字归一化 | `bare_clip_name`（剥路径/扩展名、小写折叠）与 k5_reconcile 逐字一致——比较侧归一化，资源身份仍是 data FNV |

解析守卫（任一失败 → 退出码 3）：magic == `CPK5`；文件尺寸 == header_size + count×record_size（源侧另验 record_size 字段）；每记录 `track_count == 116` 且 `two_read_equal == 1`；钟值 float32 解码非有限 → 退出码 3（G4P-5 守卫）。

### 14.3 资格重导出（冻结）

- Track 2 六族断言集合逐字复制 [k5_reconcile.py `TRACK2_REGISTERED`](../../../tools/k5_reconcile.py)（引用其登记来源 §十五/§十六）；Track 2 模式无声明缺口桶（与 `--track2` 语义一致）。
- 记录级：任一 clip `data_fnv == 0` → 该记录 **IDUN 排除**（§3.2）；可读 `(bare_name, data_fnv)` ∉ 集合 → **候选侧或源侧均判 INVALID**（退出码 2，summary 登记 K5-7 FAIL 行；不产生配对输出）。

### 14.4 配对实现（冻结 = §3.3/§3.4/§3.5）

1. 钟族优先级：`1hm_equip > 1hm_unequip > 1hm_idle > mt_idle > mlh_idle > scar_1hmreadydummy`；记录活动族（可读身份）中按序取首个 ∈ 表者；无 → `no-clock`。
2. 实例窗：域内（同钟族）按侧内序（候选 = 文件记录索引；源 = seq，文件序即 seq 序）分组——相邻目击**文件相邻**（索引差 == 1，即中间无本族缺席记录）且 stable-id（node, binding, binding_index）相同 → 同窗；否则分窗。窗内 `local_time_bits` 出现下降 → wrap 切环（环内钟值单调不减）。
3. nearest-clock：候选钟值 `x` 对域内**全部**源记录钟值（跨窗/跨环）取 `|c−x|` 最小；等距平手取 `seq` 较早；最近者为其端点且 `x` 在源钟序列 `[min,max]` 之外 → `edge-pair` flag（配对仍成立）。域无源记录 → 该候选记录 `no-partner`（逐条记账，不跨域补）。
4. 逐对输出：`domain/cand{record,segment,completed}/src{seq}/clock_x/clock_c/dt_clock/cand_families/src_families/family_set_diff{cand_only,src_only}/flags`。
5. 对账恒等式（逐侧逐域）：`合格记录 = paired + idun_excluded + no_clock + no_partner + edge_pairs(重叠类)`；侧级恒等式 `total = eligible + idun_excluded`，`eligible = paired + no_clock + no_partner`。

### 14.5 输出 schema 与测试清单（冻结）

- **manifest JSON**：`schema/protocol/pose_decoded/inputs{path,bytes,sha256,records,identity}/clock_priority/domains{family: {candidate_records, source_records, pairs, edge_pairs, no_partner, wrap_cycles{cand,src}}}/ledger{side: total/eligible/paired/idun_excluded/no_clock/no_partner/out_of_set}/pairs[]`。domains 按优先级序、pairs 按候选记录索引序、固定键序写出；≤ 1 MiB（超限 → 退出码 3，不截断）。
- **summary 文本** ≤200 行：`G4P SUMMARY` 行集（侧级/域级对账、锚点核对、IDUN 落点、wrap 统计、K5-7 结论）。
- **正式模式 detail**（仅 `--tolerances` 时）：逐对 16 B 头（pair_index u32 / cand_record u32 / src_seq u32 / reserved u32）+ 116×4×f32（E_p、E_r、theta、E_s）= 1,872 B/对；pose track 分量序 T(3)Q(4)S(3) float32 LE；`E_p` double 累加欧氏、`E_r = 1−|dot|`（归一化 + clamp）+ `theta = 2·acos(c)`、`E_s` 三分量 max-abs；非有限/零范数 → 单列 invalid 计数，不进分布。
- **测试 `tools/gate4_pair_test.py`**（纯 stdlib、`python -I` 可跑、显式 sys.path 引导，期望值全部解析计算、不调用被测函数生成答案）：T1 `E_p` 已知值 (0,3,4)→5；T2 `E_r` q/−q 等价→0、60° 轴转→1−cos30°；T3 `E_s` 单 Z 差 0.125→0.125；T4 同记录双族取高优先钟；T5 nearest 等距平手取 seq 早；T6 越界 edge-pair flag；T7 wrap 切环；T8 IDUN 排除 + 恒等式闭合；T9 域空 no-partner；T10 合成 CPK5/CPK5S 解析往返；T11 两次运行 manifest 字节一致；T12 无 `--tolerances` 拒绝 pose 解码（`pose_decoded=false` 且无 detail）。

### 14.6 实施顺序

§十四（本节）先于代码提交冻结 → 实现 + T1–T12 全过 → Track 2 Run 1/2 工件 dry-run（§十五登记，含工具 SHA-256）。**零游戏代码、零构建、零上机贯穿本节。**

## 十五、协议可执行性验证（metadata-only dry-run，2026-11-15 登记）

**裁定：协议在 Track 2 Run 1/2 工件上可执行成立（限定：本批工件、metadata-only、冻结规则 v1）；无规则歧义需要修订；观察清单移交 §五 PENDING-CAL 槽位与门 4 完整冻结阶段。姿态分量全程未解码（`pose_decoded=false`），不产生任何姿态结论。**

### 15.1 执行身份

| 项 | 值 |
| --- | --- |
| 命令 | `python -I tools/gate4_pair.py --candidate …g5k5c-track2-run1-20261115-0024.bin --source …g5k5s-track2-run1-20261115-0033.bin --candidate-json … --source-json … --out-dir docs/diagnostics/g5-gate4-20261115/dry-run` |
| 工具 | [tools/gate4_pair.py](../../../tools/gate4_pair.py) SHA-256 `09e5feab…7d4ed`；[tools/gate4_pair_test.py](../../../tools/gate4_pair_test.py) `adc93307…5f65`（T1..T12 全 PASS，34 断言） |
| 输入工件 | 候选 CPK5 249 记录 1,410,452 B `8c63d5fa…019fa6`（= [Track 2 §17](g5-track2-20261114/README.md#十七run-1候选侧-k5-姿态采集2026-11-15-0022pass含一项-identity-unreadable-记账) 登记值）；源 CPK5S 1065 记录 6,019,504 B `9a9933bc…1ac68` |
| 输出工件 | [manifest](dry-run/gate4-pair-manifest.json) 111,548 B `4a3dd0ae…706`；[summary](dry-run/gate4-pair-summary.txt) `978a58ea…d6`（13 行，预算内） |
| JSON 交叉核对 | 两侧 `recorded` == bin count（249 / 1065）✓ |

### 15.2 对账与锚点（全部判据形态成立）

| 项 | 读数 |
| --- | --- |
| 候选账本 | total=249 idun=2 eligible=247 paired=247 no_clock=0 no_partner=0 edge_pairs=12 out_of_set=0；恒等式闭合 |
| 源账本 | total=1065 idun=1 eligible=1064 hosted_pairs=247 records_hosting=36 out_of_set=0 |
| 域表 | 1hm_equip cand=96/src=71；1hm_unequip cand=129/src=118；1hm_idle cand=22/src=368；mt_idle cand=0/src=507；mlh_idle 与 dummy 两侧 0（总是与更高优先钟并存） |
| dry-run 锚点 | E1 96 / idle 22 / U1 129 **三锚全部 matched=yes** |
| K5-7 | 可读身份全部 ∈ Track 2 六族集合，verdict=PASS（与 k5_reconcile `--track2` 一致） |
| 确定性 | 同参调用两次，manifest + summary SHA-256 逐字节一致（G4P-4 同款检查） |

### 15.3 元数据观察（登记，不在本协议内裁定）

以下为 dry-run 暴露的时间线形态事实——**不构成规则歧义、不触发本协议升版**；其对时钟轴判据与姿态解释的含义由门 4 完整冻结阶段（§五 PENDING-CAL 槽位）处置：

1. **候选过渡 clip localTime 阶梯推进**：相邻记录同值比例 1hm_equip **81/97**、1hm_unequip **108/128**、1hm_idle **28/45**（跳跃幅度 ≈ n×1/60 的追赶式突发）；源侧同族仅 8/71、9/117、**0/387**。阶梯内多条候选记录共享同一相位点，nearest-clock 确定性把同域同一源伙伴分配给它们（many-to-one 集中），机制不受影响。
2. **两侧 clip 总相位跨度端点逐位一致**：1hm_equip 终点同为 `1.4333`、1hm_unequip 终点同为 `2.0000`；候选 1hm_idle 45 步 ↔ 0.7500 s 精确对应（= 45×1/60）。过渡 clip 的候选跨度小于"步数×1/60"（equip 差 11/60、unequip 差 8/60）——候选过渡 clip 时钟相对驱动步呈滞后/追赶形态，是被测时间线事实，不是配对规则缺陷。
3. **dt_clock 全对分布（247 对）**：median 0.0047 / p95 0.0110 / max 0.0178 s（亚帧量级；max 即 unequip 下界 edge 对）。
4. **家族集合差仅 1/247 对**：equip 末点边界对（候选 completed=86 vs 源 seq 173，dt_clock=1.19e-07）——源侧 idle 栈在 equip 尾相对候选提前一步出现；单对登记，不删帧。
5. **edge_pairs=12**：unequip 9 条（候选 local==0.0 越源下界 0.0178）+ equip 1 条（0.0167 < 0.0225）+ idle 1 条；逐对 flag 记账。
6. **零 wrap**：两侧全部实例窗单环（无循环回绕歧义）；`no-clock`/`no-partner` 均为 0。

### 15.4 对门 4 完整冻结的输入

- 时钟轴判据（§五 PENDING-CAL 槽位）的定义必须覆盖观察 1/2（候选过渡 clip 的阶梯/滞后形态与其总跨度端点对称性）——是按 clip 实例速率、还是按跨度端点/形状比较，由 CAL 输出填充时一并冻结；本协议不预设。
- 姿态阶梯（同一相位点多条候选记录的姿态是否相同）属 pose 域问题，dry-run 未解码、正式模式首次给出。
- §15 的全部读数是**协议可执行性证据**，不是姿态比较结果；正式裁定只消费门 5 正式运行的新工件。

<a id="gate4-cal1b-slots"></a>
## 十六、CAL1-B 姿态槽位填充（2026-11-15，限定范围）

[C-G5-CAL1-B v1](../g5-cal1b-20261115/README.md#cal1b-results) 已完成控制 B 单层姿态的有界校准。本节只登记姿态槽位的填充值；**不是门 4 完整冻结**。

| 槽位 | 值 | 来源/范围 |
| --- | --- | --- |
| `tau_p` | `0.000476592`（未舍入 `4.765916457818655e-4`） | CAL1-B；控制 B 单层姿态，`AE-Track2`，3 进程 |
| `tau_r` | `6.03341E-8`（未舍入 `6.033405486327581e-8`；`tau_theta=6.947463158790921e-4`） | 同上 |
| `tau_s` | `0.00000386264`（未舍入 `3.862634230245021e-6`） | 同上 |
| `epsilon_t` | **已由 TIME v1 填充 `0.0333960` s** | T1 invariant TSC + 帧表；见 §十七 |
| `Δt_clock` 处置 | **已由 TIME v1 填充：`dt_exclude=true`，阈值 `epsilon_t`，2% edge-only** | 门 4 §五；不得用姿态容差吸收 |
| 时钟轴判据 | **仍为 PENDING-CAL** | §15.3 的候选阶梯/滞后形态待处置 |
| 轨道覆盖 | **99/116** | CAL1-B 只覆盖控制 B 的 99 个 transform 轨；其余 17 个 default/reference 轨无本控制 |

**边界**：三项姿态值只适用于控制 B 单层、`female\mt_idle.hkx`、121 时间/99 轨、3 进程；不得外推到层/权重/混合、其他资源或其他运行。**门 4 的 116/116 分母中 17 个 item/wing/Belly 轨没有这组 tau 的控制覆盖**，不得把全局 tau 无条件套到 116/116；覆盖/时间审计见 [CAL1-C](../g5-cal1c-coverage-timing-20261115/README.md)，修订见 [C-G5-G4-COV v1](../g5-g4-cov-20261115/README.md) 与 [C-G5-TIME v1](../g5-time-20261115/README.md)。`gate4_pair.py --tolerances` 现在要求 `--coverage`；时钟轴、K5-5、K2-C1 仍阻塞，门 4 仍**未完整冻结**，门 5 仍未授权。

来源工件：[cal1b-calibration.json](../g5-cal1b-20261115/calibration/cal1b-calibration.json)（SHA-256 `8B2077BF63AB68A856148FEF1F0B9CE9F3936334125BF0BAADE91C0B8C740865`）；完整逐轨值见 [cal1b-tracks.csv](../g5-cal1b-20261115/calibration/cal1b-tracks.csv)。

**A17 审计**：[辅助轨审计](../g5-aux-track-audit-20261116/README.md) 显示 A17 在 Track2 窗口内近似静态（span ≤3.8e-6），跨侧差 `E_p≤1.97e-6`，且无活动 clip 控制；COV 方案 A 保留，A17 不进入 CAL1-B 容差，仍不声称 116/116。

## 十七、TIME v1 时间槽位填充（2026-11-16）

[C-G5-TIME v1](../g5-time-20261115/README.md#time-results) 已完成 T1 时间评估：

| 槽位 | 值/规则 |
| --- | --- |
| `f_tsc` | 4,291,863,648.325 Hz（机器级 invariant TSC；非逐进程字段，已披露） |
| `epsilon_read` | `0.0000626282` s |
| `epsilon_grid` | `0.0333333351` s（全局 max frameDuration；30 fps clip） |
| `epsilon_t` | **`0.0333960` s**（未舍入 `0.0333959631725`） |
| `dt_exclude` | `true`，阈值 `epsilon_t`；2% 上限 + 全 edge 对；dry-run **0/247** 排除 |

来源：[cal1c-time.json](../g5-time-20261115/calibration/cal1c-time.json)（SHA-256 `FC060CCA…ACCF5`）。

**边界**：`epsilon_t` 只用于门 4 时间等价/排除纪律，不传播为姿态 tau；时钟轴速率/相位判据已由 CLOCK v1 填（§十八）；门 4 已按 §十九 冻结（限定范围），门 5 采集授权但未运行。

## 十八、CLOCK v1 时钟轴槽位（2026-11-16）

[C-G5-G4-CLOCK v1](../g5-g4-clock-20261116/README.md#八实现与-dry-run-结果2026-11-16) 已完成 metadata-only 时钟轴评估：

| 项 | 值 |
| --- | --- |
| 工具 | `tools/gate4_clock.py`（SHA-256 `6E21BB47…A41780`），9/9 测试 PASS |
| 域 | 3 个有配对域；247 对 |
| rate（E2-C ±0.017） | 0.995882 / 0.991083 / 0.993000 |
| endpoint / dt（≤`epsilon_t=0.0333960`） | 最大 0.017833 / 0.017833 s |
| 阶梯 | candidate 83/95、109/128、19/21 相邻等值；source 唯一 point 0/16、0/20、0/2；只登记不判失败 |
| 判据 | G4C-1..6 **PASS**；`CLOCK_READY` |

**边界**：时钟轴已填；门 4 已按 §十九 冻结（限定范围），门 5 采集授权但未运行；G5 仍因 K5-5/K2-C1/A17 阻塞。

## 十九、门 4 完整冻结记录（2026-11-16）

本协议按 §五条件 1–4 与 §六“显式带缺项冻结”完成：

- 条件 1：姿态/时间/覆盖/时钟轴槽位全部填充，参数 [tolerances.json](../g5-gate4-freeze-20261116/tolerances.json)（SHA-256 `B810698E…710EE`）。
- 条件 2：CAL v1 §九 [联合验收](../g5-cal-joint-acceptance-20261116/README.md) 以 `CALIBRATED (limited: control-B P99 pose + time + clock axes)` 通过。
- 条件 3：G4P-1..6、8、9 结构判据在 metadata dry-run 通过；coverage dry-run 两次逐字节一致（`69EC8AA7…C0E0A` / `CDF6F7A2…A8C13`）。
- 条件 4：K5-5、K2-C1、A17 显式缺项冻结，G5 阻塞如实登记。

**门 5 采集已授权（P99 姿态/时间/时钟轴 + A17 描述性账本），但本记录不表示门 5 已运行；G5 未通过，G6 后置。** 详见 [冻结记录](../g5-gate4-freeze-20261116/README.md)。
