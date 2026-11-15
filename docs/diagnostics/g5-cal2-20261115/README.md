# C-G5-CAL2 v1：控制 B 三进程/120 代网格协议预注册（2026-11-15 草案，待评审）

**状态（2026-11-15）：预注册（评审通过）。** 用户于 2026-11-15 批准本协议（§八决策 1）与实现授权（§八决策 2/3：`AE-Track2` 档 + v104 代码改动 + 本地真值转储）。本文档把控制 B 从 [B2 的"9 固定时间单发采样"](../g5-cal-control-b-20261114/README.md)（合格进程 1 个，bounded READY）重组为 [CAL v1 §五.1/§五.2/§五.6](../../independent-behavior-graph.md#c-g5-cal-v1) 要求的完整形状——**每进程 1 个 baseline + 120 个完成代、每代 2 份读数，共 3 个独立进程**——并交付跨进程重复性数据（[CAL v1 §七](../../independent-behavior-graph.md#c-g5-cal-v1) N 的原始材料）与控制自身计时区间（§五.4）。实现范围在 §九 冻结后动代码。`tau_p/tau_r/tau_s/epsilon_t` 的推导属 CAL1 分析，本协议不产生任何数值容差。

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
