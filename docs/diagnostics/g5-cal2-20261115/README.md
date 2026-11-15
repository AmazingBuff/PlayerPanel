# C-G5-CAL2 v1：控制 B 三进程/120 代网格协议预注册（2026-11-15 草案，待评审）

**状态（2026-11-15）：预注册（评审通过）。** 用户于 2026-11-15 批准本协议（§八决策 1）与实现授权（§八决策 2/3：`AE-Track2` 档 + v104 代码改动 + 本地真值转储）。本文档把控制 B 从 [B2 的"9 固定时间单发采样"](../g5-cal-control-b-20261114/README.md)（合格进程 1 个，bounded READY）重组为 [CAL v1 §五.1/§五.2/§五.6](../../independent-behavior-graph.md#c-g5-cal-v1) 要求的完整形状——**每进程 1 个 baseline + 120 个完成代、每代 2 份读数，共 3 个独立进程**——并交付跨进程重复性数据（[CAL v1 §七](../../independent-behavior-graph.md#c-g5-cal-v1) N 的原始材料）与控制自身计时区间（§五.4）。实现范围在 §九 冻结后动代码。`tau_p/tau_r/tau_s/epsilon_t` 的推导属 CAL1 分析，本协议不产生任何数值容差。

> **2026-11-15 追加（run 1 + 工具）**：v104 已部署并完成 **1/3** 个独立进程；离线裁定工具已实现并通过离线测试，见 [§十一](#cal2-run1-tool)。该进程 C2-A/B/C/F 通过，**严格 C2-E 因 Track2 环境缺失 OAR 的确定性 `[E]` 失败**；是否按 Track 2 §十二豁免需用户裁决，run 2/3 尚未运行。

**前置阅读**：[CAL v1](../../independent-behavior-graph.md#c-g5-cal-v1)（§四 控制 B 资格、§五.1–§五.7、§七 统计、§八 CAL1–CAL7）、[K2-C4 控制 B 登记](../g5-cal-control-b-20261114/README.md)（资源身份/独立真值/B1 FAIL/B2 PASS）、[K6 计划 §五](../g5-k6-plan-20261114/README.md)（进程缺口登记）、[门 4 协议 §十五](../g5-gate4-20261115/README.md)（候选时钟阶梯形态登记）、[Track 2 §十六](../g5-track2-20261114/README.md)（资源确定性条件）。

## 一、目标、范围与不覆盖

| 项 | 交付 | 不覆盖 |
| --- | --- | --- |
| 控制网格重组 | 每进程 baseline(t=0) + 120 完成代（dt=1/60 网格，t=(j+1)/60, j=0..119）× 每代 2 读，全部原始 float32 位值落盘 | 不改变 B2 已登记的 9 时间设计及其历史裁定 |
| 三进程重复性 | 3 个独立进程 × 同一控制/资源/采样版本；三尝试全保留 | 不静默替换失败窗口；不把 B2 的 v100a 进程计入（协议版本不同） |
| 解析界覆盖 | B2 同款逐分量解析界应用于全部 121 × 99 × 10 = 119,790 分量/进程 | 不产生 `tau_*`（推导属 CAL1） |
| 计时记录 | 每次采样的输入锚点→完成输出→读取延迟 TSC 区间（§五.4 同一计时控制记录） | 不做跨运行 TSC 相减（§五.3） |
| 对账 | 每进程 expected/visited/recorded/missing 逐项闭合 + 工件 SHA-256 | 不做姿态等价、不关闭 K5-5 缺项、不做源/候选配对（门 4 范围） |

**明确不做**：不改产品路径；off 路径与 v103 逐字节一致断言维持；不改写 B1/B2 历史裁定；不把控制数据用于 G5 验收（[CAL v1 §八 CAL7](../../independent-behavior-graph.md#c-g5-cal-v1)）。

## 二、设计决策（预先声明）

### 2.1 网格实例化：驱动链每代结构 + 预定网格时间直采

- **驱动**：复用既证注入链 equip 段——inject → 同帧 5 写 prime + `WeapEquip` → equip 120 步（`fixed-equip-budget`，每步交付 dt=1/60）。第 k 个完成代（`completed=k`, k=1..120）即网格代 j=k−1，归属 **t=k/60**（j=0..119 → t=1/60..2.0；满足 CAL v1 §五.2 "第 j 次完成输出归属 t=(j+1)/60，不是 bind/前次输出"）。
- **baseline**：t=0 的 2 份读数在 `completed=1` 同点采集（先于 j=0 的 t=1/60 读数），登记为 baseline 代。
- **采样**：每代完成点（候选 e_post_sample 同点，K3/K5 快照既有挂点）对 **held carrier** 的 `hkaAnimation` 直接调用两次 `SamplePartialTracks(t=k/60, 99, out, 0, nullptr, nullptr)`，逐字节比较。不读 poseLocal、不写图、不创建第二 Actor。
- **采样时间是预定网格值，不取图的 clip localTime**。理由：CAL v1 §五.3 的时间键 = 预定控制输入序数 + 完成代号 + 累计已交付 dt；[门 4 dry-run §15.3](../g5-gate4-20261115/README.md) 已登记候选过渡 clip localTime 的阶梯/滞后形态——把候选时钟行为引入控制会把"被测对象的时钟"混入"控制的独立真值"，污染控制资格。图 drive 的职责仅为提供每代完成结构与真实计时条件（§五.2/§五.4）。
- **被否备选（登记）**："每代采样图的 clip localTime 当前值"——时间归属依赖候选时钟形态，违背控制的独立性；"一次性连采 121 个时间"——缺每代完成结构与计时条件，不满足 §五.2/§五.6 形状。

### 2.2 环境 = `AE-Track2` 档（OAR 停用）

- 载体稳定性由构造成立（无替换引擎；[Track 2 §十六](../g5-track2-20261114/README.md)已证确定性资源条件）；控制资源 = 同一 loose 文件 `female\mt_idle.hkx`（113,920 B，`data` FNV `F56EE1500EB5A3D0`）。
- 三进程同档、同 DLL/PDB/INI、同网格真值版本 → 满足 §五.1"同一控制/资源/采样版本"。
- 备选（主 `AE` 档，B2 既有环境连续性）→ §八待用户决策；两案的资源 FNV 逐代重验判据相同。

### 2.3 载体生命周期

- `completed=1` 按 B2 规则选载体（只扫 activeNodes、vtable=`hkbClipGenerator`、**动画字节身份**唯一匹配），缓存 binding/animation 引用。
- **每代重验**：`data`/`blockOffsets` FNV 与登记值逐位比较（K5 身份缓存同款语义）；不可读或不等 → 该代记账 invalid，按 §五.1 批次规则处置（不静默替换、不拼接窗口）。
- equip 段 120 步内 MT_Idle 生成器活性不必逐代成立（步骤 2..86 期间该 clip 不在 activeNodes——[Track 2 Run 1 clip 窗口](../g5-track2-20261114/README.md#十七run-1候选侧-k5-姿态采集2026-11-15-0022pass含一项-identity-unreadable-记账)），但 binding/graph 资源存续；每代 FNV 重验是存续性的运行证据，任一代失验即如实记账。

### 2.4 独立真值扩展（本地离线，零游戏）

- 本地 gitignored `build/hkx-audit/HkxTruth`（HavokLib `ef5d5c6`，GPL-3，不进仓库、不链接插件）对 **121 个网格时间**（t=0 ∪ t=(j+1)/60, j=0..119；全部 ⊂ duration 25.0167 s）重新转储：121 × 99 行 `TRACK`（每行 10 个原始 float32 bits）+ 末行 `BITS_FNV1A64`。时间映射补丁（`localTime / frameDuration`）与 B1 登记逐字一致，不改。
- 新真值工件（命名实现预注册冻结，如 `mt_idle_female_truth_grid121.txt`）SHA-256 登记；**9 时间真值文件保留原状**（B1/B2 历史不改写）。
- 一致性锚点：网格时间 {0, 1/60, 1} 与 9 时间表重叠——网格真值在这三点的位值必须与既有真值逐位相同（转储自检；不齐 → 真值工具回审，不采样）。

## 三、记录格式与预算（按 CAL v1 §五.7 预先固定）

| 工件 | 格式 | 上限/进程 | 超限 |
| --- | --- | --- | --- |
| 网格变换工件 | 二进制：header（magic/version/identity/计数）+ 121 代 × 99 轨 × 10 × float32 原始位值（数据 479,160 B） | ≤ 8 MiB | 整轮无效，保留已收计数 |
| 身份/计时/对账 | JSON 账本（每代：序数/预定时间/两读 equal/载体 FNV 重验/TSC 区间）+ 日志行 | 三项合计 ≤ 1 MiB | 同上 |
| 文本摘要 | `CONTROL GRID` 行集 | ≤ 200 行 | 同上 |

- 每代 2 读**逐位一致**才计入正常样本（CAL v1 §五.2/§七 N 规则）；不一致 → 该代单列审计，不进 N、不计零差。
- 计时（§五.4）：每次采样记 TSC 于（调用前 / 首次采样返回后 / 次次采样返回后 / 比较完成）四点 → 输入锚点→完成输出、输出→读取区间；TSC 频率/单位与单调基准随进程登记，**不跨运行相减**（§五.3）。
- 分母（§五.5）：99 transform 轨 + 0 float 轨 = 载体 binding 全部应有轨道；`transformTrackToBoneIndices` = 0..98（identity map）随进程登记；无 reference/default-fill 剥除。

## 四、运行协议

1. **实现（v104）**：新键 `[Experiment] ControlSampleGrid=true`（默认 off；off 时输出与 v103 逐字节一致）。预期触点：`control_sample.h/.cpp`（网格模式：每代采样 + 载体重验 + 计时）、`settings.h/.cpp`（键 getter + setting 行）、`independent_graph.cpp`（每代 hook，completed=k 分支）、`control_sample_policy.h`（网格时间表/常量）、`tests/control_sample_policy_test.cpp`（网格用例）。**代码触点、键名、字节布局在本文档批准后另行实现预注册冻结。**
2. **真值网格转储**（本地，先于任何控制输出查看）：HkxTruth 121 时间转储 → SHA-256 登记 → 重叠锚点自检。
3. **部署**：v104 三件套（DLL/PDB/INI）+ Track 2 运行档派生（`IndependentGraph=inject` + `ControlSampleGrid=true`，K5 三键全 off；`EquipmentStateDump` 等既有键照 v96a 基线）。部署备份先行。
4. **3 个独立进程（用户执行游戏会话）**：每进程 = Save116 → panel 窗口一次 → inject 链 equip 120 步（网格采样随代）→ 链照常跑完（unequip 段无控制采样）→ 关窗。三尝试全保留；任一失败按 §六停止，不静默替换第四窗。
5. **离线**：真值对账（解析界逐分量）+ 三进程跨进程差异（同预定时间逐分量差，全样本保留）+ 对账闭合 → 判据裁定（§五）→ 登记。

## 五、判据（预注册，不得事后放宽）

| # | 判据 | 通过条件 | 否则 |
| --- | --- | --- | --- |
| C2-A 资源身份 | `completed=1` 载体唯一且三 FNV/时长/轨道字段与登记逐位一致；每代 `data`/`blockOffsets` FNV 重验相等 | 全过 | 该代 invalid；累计 invalid 代 > 0 → 批次按 §五.1 处置（保留原因），不得静默换窗 |
| C2-B 读取完整性 | 每进程 121 代 × 99 轨 × 10 分量全记录；每代两读逐位一致；全部有限 | 全过 | INVALID |
| C2-C 解析界 | 每分量 \|runtime − truth\| ≤ B2 同款解析界（分量类型表逐字复用：rotation 40-bit 步长 / dynamic 16-bit / static float32 / identity） | 0 violation | INVALID；保留全部原始 bits 离线定位，不得用容差吸收 |
| C2-D 三进程 | 3 个独立进程各自 C2-A..C2-C 通过；跨进程同时间逐分量差全样本登记（**只交付数据，不推导 N**） | 缺一 → NOT_READY，保留已收 | |
| C2-E 运行完整性 | 每进程一次窗口/一次会话、零 `[E]`、无崩溃、DLL/PDB/INI/存档身份匹配、预算未超 | 任一失败 → 该轮 INVALID | |
| C2-F 计时 | 每次采样四点 TSC 齐全、区间分布按进程报告 | 缺失/非单调 → 该代 invalid | |

**裁定语义**：C2-A..F 全过 = **CAL v1 §五.1/§五.2/§五.6/§五.4 对控制 B 的要求闭合**（限定范围：该资源 `female\mt_idle.hkx`、该 121 时间网格、该单层直采路径、`AE-Track2` 档、3 进程）→ **K2-C4 从 bounded READY（1 进程）升为网格 READY（3 进程）**，CAL1 原生校准的控制 B 资格就绪；仍不产生 `tau_p/tau_r/tau_s/epsilon_t`（N/Q 推导与容差属 CAL1 分析，[CAL v1 §七](../../independent-behavior-graph.md#c-g5-cal-v1)），不关闭姿态等价/G5/K5-5 缺项，不做 G5 验收。

## 六、停止规则

任一即停止当前窗口并保留原因与已收计数，不拼接、不续裁、不降级：任何 `[E]`、崩溃、需要写源图或未知 ABI、载体 FNV 失验且无法解释、存档/DLL/INI 身份与登记不符、reset/close/PreLoadGame/NewGame、预算超限。三尝试全保留；失败不得静默替换第四窗；重跑须登记原因与新批次。修订须另立预注册版本；已看数据不回填。

## 七、实施顺序与提交拆分

① 本文档评审批准（状态翻转 + 入口同步）→ ② 实现预注册（代码触点/键名/字节布局冻结，含 `CONTROL GRID` 行格式）→ ③ 实现 + 离线测试（`control_sample_policy_test` 网格用例 + off 路径逐字节一致断言）→ ④ 真值网格转储（本地 HkxTruth，SHA-256 登记 + 重叠锚点自检）→ ⑤ v104 构建部署（备份先行）→ ⑥ 3 进程上机（**用户执行**）→ ⑦ 离线裁定登记 + 文档同步。

提交拆分：`docs(g5-cal2): 预注册控制 B 三进程/120 代网格协议（评审通过版）` → `docs(g5-cal2): 实现预注册` → `feat(g5-cal2): 网格仪器实现` → `docs(build): v104 构建部署` → `docs(g5-cal2): 三进程运行记录与裁定`。①–④ 零游戏；⑤–⑦ 涉及部署与上机。

## 八、待用户决策事项

| # | 决策 | 影响 |
| --- | --- | --- |
| 1 | 本协议评审与批准（重点：§2.1 网格实例化 = 驱动链每代结构 + 预定时间直采；§2.2 环境 = `AE-Track2` 档）——**已批准（2026-11-15）** | 已解锁 §七 ①–④ |
| 2 | 实现授权（v104 代码改动；新键默认 off；零产品路径）与真值网格转储授权（本地 gitignored 工具）——**已批准（2026-11-15）** | ②–④ 前置 |
| 3 | 环境备选确认：`AE-Track2`（推荐，载体稳定性构造成立）vs 主 `AE` 档（B2 环境连续性；OAR 在场，靠逐代 FNV 重验守门）——**已确认 `AE-Track2`（2026-11-15）** | §2.2 落定 |
| 4 | （会话外既挂）82 提交 push 与否；AGENTS.md CharacterPanel 条目更新与否 | 与本协议无耦合 |

`Documentation Impact: update`：批准后同步主文档账本行（K2-C4 状态与 CAL2 入口）与文首状态行、[README](../../README.md)、[history](../../history.md)、[CHANGELOG](../../CHANGELOG.md)；不可回写 B1/B2 历史结论。本文仅依据本地已审计边界规定研究要求；实现涉及的新 hook 点（每代采样挂点）沿用既有 e_post_sample 审计入口，不引入新 ABI/hook。

## 九、实现预注册（2026-11-15，批准后冻结；实现前零采集）

本节冻结网格仪器的实现范围。所有变更先于任何采集；采集开始后本节不改（变更 → 本协议升版）。

### 9.1 INI 键

`[Experiment] ControlSampleGrid`（bool，默认 **off**）。要求 `IndependentGraph=inject`（键只在注入链每代 hook 处生效，非注入路径零触点）。`ControlSampleGrid=true` 且 `ControlSample=true` 同时开 → 9 时间单发采样被抑制并输出声明行 `CONTROL GRID ... phase=manifest ninetime=suppressed`（K5/K3 抑制声明同款纪律）；只开 `ControlSample` → 行为与 v100a 完全一致（零改动）。**off 时本次改动产生的全部输出行不存在，既有输出逐字节与 v103 一致。**

### 9.2 代码触点（6 处，不触碰产品路径）

| # | 文件 | 改动 |
| --- | --- | --- |
| 1 | [settings.h](../../../src/animation/settings.h) / [settings.cpp](../../../src/animation/settings.cpp) | `control_sample_grid()` getter + 全局 + `EXPERIMENT setting ControlSampleGrid={}` 行（与 ControlSample 模式逐字同构） |
| 2 | [control_sample_policy.h](../../../src/animation/behavior_graph/control_sample_policy.h) | 网格常量：`Control_Grid_Generation_Count = 120`、**121 项静态 `Control_Grid_Times[]` 原始 bits 表**（0 = baseline；k/60, k=1..120）、记录/头尺寸常量。表由 float64 除法 → float32 舍入一次性生成并逐字粘贴；运行期**不计算任何时间**，只读表 |
| 3 | [control_sample.h](../../../src/animation/behavior_graph/control_sample.h) | 新 API：`run_control_grid_generation(character, actor_id, request, completed)` 与 `close_control_grid(actor_id, request)`；网格状态（held carrier + writer + 计数）封装在 control_sample.cpp 内部，头文件不暴露 |
| 4 | [control_sample.cpp](../../../src/animation/behavior_graph/control_sample.cpp) | 网格实现：completed=1 时按 B2 规则选载体（复用既有 `spline_matches`/`read_binding_identity` 静态助手，9 时间函数体零改动）→ 采集 **baseline(t=0) + j=0(t=1/60)** 两条记录；completed=k(2..120) → 采集 j=k−1(t=k/60)；每代：载体 `data`/`blockOffsets` FNV 重验 → 两次 `SamplePartialTracks` + memcmp → 四点 `__rdtsc` → 首读 99×40 B 原始位值入记录。completed=120 采集后写 close 行并关闭工件 |
| 5 | [independent_graph.cpp](../../../src/animation/behavior_graph/independent_graph.cpp) | `record_output` lambda 内、既有 control_sample 分支之前加网格分支：`segment=="equip" && completed∈[1,120]` → 每代调用；completed==120 → close。9 时间分支原样保留 |
| 6 | [tests/control_sample_policy_test.cpp](../../../tests/control_sample_policy_test.cpp) | 网格用例（§9.5） |

### 9.3 工件字节布局（pack(1)，static_assert 到位）

- **Header（112 B）**：magic 4s `CGRD` | version u16=1 | header_size u16=112 | gen_capacity u16=121 | gen_recorded u16 | track_count u16=99 | reserved u32=0 | identity char[92]（`Plugin_Build_Identity`，NUL 填充）+ pad[2]。
- **Generation record（4,004 B）**：gen_ordinal u16（0=baseline，1..120=网格代 j）| time_bits u32 | completed u16 | two_read_equal u8 | identity_ok u8 | flags u8（bit0=identity_bad、bit1=nonfinite）| pad u8 | `tsc_call_start` u64 | `tsc_after_first` u64 | `tsc_after_second` u64 | `tsc_after_compare` u64 | data 99×40 B（**首读**原始位值，轨主序、每轨 t.xyz/r.xyzw/s.xyz 小端）。
- **总量**：112 + 121 × 4,004 = **484,596 B** ≤ 8 MiB（CAL v1 §五.7）。
- **路径**：`SKSE::log::log_directory()/CharacterPanel-g5cal-grid-<identity>-<UTC>.bin`（K5 路径模式同款）。
- **close 行**：`generations=121 two_read_bad=0 identity_bad=0 nonfinite=0 data_fnv1a64=<全体 data 域 FNV-1a64，序数主序> artifact='...' bytes=484596`；SHA-256 由离线登记，close 行不含。

### 9.4 日志行（~135 行/进程，远低于 200 行摘要预算）

manifest（1，含 `times=121 tracks=99`）+ carrier（1，completed=1，复用 CONTROL B scan 字段）+ 每记录 1 行 `CONTROL GRID ... phase=gen ordinal={} completed={} time_bits={:08X} two_read_equal={} identity_ok={} tsc_first={} tsc_second={} tsc_compare={}`（121+1 baseline 行 = 122）+ close（1）+ 抑制声明（0 或 1）。**原始位值只入工件，不进日志**。

### 9.5 离线测试（`control_sample_policy_test` 追加用例，引擎无关）

G1 网格表：121 项、严格递增、`[0]==0x00000000`、`[1]==0x3C888889`（**与 9 时间表 1/60 逐位相同** = 跨表锚点）、`[60]==0x3F800000`（=1，9 时间表第三点）、`[120]==0x40000000`（=2.0）；G2 预算算术：`112 + 121×4,004 == 484,596 ≤ 8 MiB`；G3 序数映射：baseline+j0 ↔ completed=1、j=k−1 ↔ completed=k、末代 j=120 ↔ t=2.0；G4 off 路径常量：网格常量不影响既有 9 时间常量（两表并存断言）。

### 9.6 真值工具附加输出（本地 gitignored；解码逻辑零改动）

`build/hkx-audit/HkxTruth` 追加 **`BOUNDS` 节**（加性输出，`GetValue`/时间映射补丁逐字不动）：逐轨逐分量输出子轨类型（`STATIC`/`DYNAMIC`/`IDENTITY`）+ 量化位宽（pos/scale 8/16-bit、rotation 位宽）+ **逐块 `TrackBBOX` min/max 原始 bits**（`TrackBBOX` 为块级 authored 元数据，[decompressor 已读](../../../build/hkx-audit/HavokLib/source/hka_spline_decompressor.cpp)）。真值 TRACK 行与 `BITS_FNV1A64` 格式不变。网格真值文件 = `docs/diagnostics/g5-cal2-20261115/mt_idle_female_truth_grid121.txt`（121 × 99 行 TRACK + BITS_FNV1A64 + BOUNDS 节）。

### 9.7 解析界规则（C2-C 的裁定来源，预先固定）

逐分量界（B2 公式逐字复用，分母按 BOUNDS 节实际量化）：rotation（本资源全部 40-bit）= `3.45436e-4`（B2 登记值逐字复用，资源 FNV 未变故量化不变）；dynamic 8/16-bit = `max(divisor·(max−min), 32·eps·(|min|+|max|))`，divisor = `2/255` 或 `2/65535`，(min,max) 取**全体块的保守并集**（块 0 覆盖全部网格时间 t≤2.0，并集仅为保守）；static = `32·eps·|value|`（逐块值取并集最大）；identity = `32·eps`。裁定工具 = `tools/cal2_adjudicate.py`（纯 stdlib，读 CGRD 工件 + 网格真值 + BOUNDS 节 → C2-B/C2-C 逐分量裁定 + 三进程跨进程差 + 对账；SHA-256 随使用登记）。网格时间 {0, 1/60, 1} 与 9 时间真值重叠三点的位值一致性 = 转储自检（§2.4）。

### 9.8 实施顺序

§九（本节）先于代码冻结 → 实现 + G1–G4 全过 → 真值网格转储（§2.4）+ BOUNDS 节 → v104 构建部署 → 3 进程上机（用户）→ `cal2_adjudicate.py` 离线裁定登记。**零产品路径、off 逐字节一致贯穿。**

## 十、v104 构建与网格真值转储登记（2026-11-15）

### 10.1 实现（§九 落地）

| 项 | 值 |
| --- | --- |
| 代码触点 | §9.2 六处全部落地（settings 键、policy 121 时间表 + 布局常量、control_sample 网格状态机/写物器、hook 分支、测试 G1–G4） |
| 离线测试 | [control_sample_policy_test](../../../tests/control_sample_policy_test.cpp) **8/8 PASS**（G1 严格递增 + 三锚点逐位、G2 484,596 B 预算算术、G3 float64→float32 规则对全部 120 个 k 重算逐位一致、G4 九时间常量原状） |
| v104 构建 | 提交 `d61a6e1` 后重配置干净重建：DLL 2,025,984 B，SHA-256 `7d270e7dfc35ccadcca0807472356aa81e3dda3efa912d516479605470d6f9df`；PDB `b2232180b7ec250ac7aebb37fa79536fd0ab7edd8df87321420a88bba31c2ac1`；身份 `CharacterPanel-scenegraph-v104-g5cal2-grid-d61a6e19b9-cl94faaed0c6-20261115T073547Z`（无 `+dirty`） |

### 10.2 网格真值转储（本地 HkxTruth，§2.4/§9.6 落地）

| 项 | 值 |
| --- | --- |
| 工具 | 本地 gitignored `HkxTruth`（HavokLib `ef5d5c6`）加性补丁重建：解压器逐轨 `TrackBBOX` 存储（块级 authored 元数据，赋值点即既有读取点）+ `DumpBounds` + main.cpp 末尾 BOUNDS 节（tmpfile 中转回写文件）；**解码逻辑零改动**，TRACK/TIME/BITS 行格式不变 |
| 输入资源 | `E:\SkyrimAE\mods\Vanargand Animations II - Female Idle Walk and Run\meshes\actors\character\animations\female\mt_idle.hkx`，113,920 B，SHA-256 `0EF8248CE2C50A2251FA592F7C65E75E75B5E4DC55E9C654436543180BCD73C7`（与登记逐位一致） |
| 时间表 | 121 项 = §9.2 同一表（float64 除法 → float32；`build/g5-cal2-20261115/grid_time_bits.txt`） |
| 输出工件 | [mt_idle_female_truth_grid121.txt](mt_idle_female_truth_grid121.txt)：TRACK 11,979（121×99）+ TIME 121 + BOUNDS blocks=6 + BOUND 594（6 块 × 99 轨）行；`BITS_FNV1A64 BE67D994E6042E31`；文件 SHA-256 `5e20fcaabcdf40a70d4c0741fb7768d31896131dad19821e28bc79b8917eb4e3` |
| 量化读数 | 全轨 `pos_q=1`（QT_16bit）、`rot_q=3`（QT_40bit）——与 B2 登记的解析界公式逐字一致（dynamic 16-bit + 40-bit rotation） |
| **重叠锚点自检（§2.4）** | 网格时间 {0, 1, 60}（t=0、1/60、1）对既有 9 时间真值的对应三时间，**99 轨 × 3 时间值域逐位一致、0 失配**（TRACK 行首的 `time=<转储序数>` 字段两侧本就不同序，不参与比较） |

**裁定：真值网格资格成立**——独立真值覆盖全部 121 网格时间（t=0 ∪ k/60），与 9 时间真值自洽，量化元数据齐备（C2-C 的逐分量界由此推导）。`tau_*` 仍未产生。

<a id="cal2-run1-tool"></a>
## 十一、离线裁定工具实现与 run 1 冒烟记录（2026-11-15）

> 本节是工具/运行登记，不修改 §五 判据；正式三进程裁定仍待 run 2/3，以及 C2-E 的 OAR `[E]` 豁免裁决。**不要把本节当作 C2-D 或 K2-C4 的升格依据。**

### 11.1 工具登记

| 项 | 值 |
| --- | --- |
| 工具 | [`tools/cal2_adjudicate.py`](../../../tools/cal2_adjudicate.py)，纯 stdlib；读 CGRD v1 + 121×99 真值 + BOUNDS 节 → C2-A/B/C/E/F 与三进程逐分量差；SHA-256 `F2225EACE4028D9216BEC06C918D6C999078A55ECD13FDF8E1A718303EF6136F` |
| 离线测试 | [`tools/cal2_adjudicate_test.py`](../../../tools/cal2_adjudicate_test.py)，34 项断言全部 PASS（`python -I tools/cal2_adjudicate_test.py`）；SHA-256 `929021A04BAE40470BF88A304142AEB8CEF9F75AE55EAA643063787FFE822DCE` |
| C2-E 默认 | **严格零 `[E]`**；只有显式 `--track2-oar-exemption` 才豁免 Track 2 §十二登记的唯一精确行 `failed to get plugin info for OpenAnimationReplacer`。CAL2 v1 正文尚未写入该豁免，故默认不给 run 1 放行 |
| 输出 | `cal2-adjudication.json`（摘要/逐代统计/计时）、`cal2-adjudication-summary.txt`（≤200 行）、`cal2-cross-process-diff.bin`（≥2 进程时按对保存全部 119,790 个 f64 差）、`cal2-violations.csv`（仅违规时） |

### 11.2 run 1（探索性，不是正式批次裁定）

| 项 | 读数 |
| --- | --- |
| 工件 | [原始文件名工件](CharacterPanel-g5cal-grid-CharacterPanel-scenegraph-v104-g5cal2-grid-d61a6e19b9-cl94faaed0c6-20261115T073547Z-20261115T125913Z.bin)，484,596 B，SHA-256 `CB5587574E5BA5146788A4DBF0873D4C57C802BE4F34356111343D1F8E1D508B`；短名副本 [CharacterPanel-g5cal2-run1-20261115-2059.bin](CharacterPanel-g5cal2-run1-20261115-2059.bin) 内容相同（close 行按原始名对账） |
| 日志 | [CharacterPanel-g5cal2-run1-20261115-2059.log](CharacterPanel-g5cal2-run1-20261115-2059.log)，198,032 B，SHA-256 `F16061B8094E6B078A8BDD7F52DC11AE81F15CF4746A3A4FABE163200017DB2B`；一次 manifest/carrier/close，equip 120 + unequip 129 |
| C2-A | PASS（artifact + log；载体唯一、`binding_index=1022`、三 FNV/时长/轨道字段与登记一致） |
| C2-B | PASS（121/121 代，`two_read_equal` 坏代 0，`identity_ok` 坏代 0，`nonfinite=0`） |
| C2-C | PASS（119,790 分量全部有界；49,391 个非零差；**0 violation**；最大比值 **0.125**） |
| C2-F | PASS（121 代四点 TSC 齐全且单调；首读 median 35,475、max 112,703 cycles） |
| C2-E 严格 | **FAIL**：唯一 `[E]` 是 `20:54:50` 的 OAR 缺失探测行，发生在任何 `CONTROL GRID` 采集之前；CAL2 v1 未登记该豁免，故按字面零 `[E]` 判负 |
| C2-E 带 `--track2-oar-exemption` | PASS（同一行按 Track 2 §十二精确豁免；其余 `[E]` 仍为 0） |
| 批次 | 严格路径：run 1 因 C2-E 为 **INVALID**；带豁免路径：**C2-D NOT_READY（1/3）**。两路径的跨进程差都尚不可计算 |

**待裁决**：① 在 CAL2 内预注册 C2-E 的 Track 2 §十二同款精确豁免（须说明 run 1 是在修订前采集，且只豁免该确定性装载期行，不改任何数值判据），然后把 run 1 计入批次；或 ② 把 run 1 视为修订前冒烟、在修订后再跑 3 个独立进程。两条路径都不产生 `tau_*`，都不把本节的 0 violation 当作 G5 验收。
