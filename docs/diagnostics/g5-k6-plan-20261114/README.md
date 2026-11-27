# C-G5-CAL K6：K5 仪器与数据计划（2026-11-14 预注册草案）

**状态（2026-11-14）：预注册（评审通过）。** 用户于 2026-11-14 批准本计划（含 §二两轨制裁定与 Track 2=B 方向），按 §八 顺序实施；实现与运行记录随后在本目录登记。本文闭合 [CAL v1 §三 K6](../../independent-behavior-graph.md#c-g5-cal-v1) 的四项要求——仪器范围、两个控制的就绪清单、采集/分析版本及预算——并裁定 [K1-C4 NOT MATCHED](../g5-k1-census-20261114/README.md#八k1-c4-源候选资源对照与裁定) 的直接后果：OAR 资源策略（§二）。

**前置阅读**：[第二会话总结](../test-results-20261118.md#g5-qualification) §七 + [CAL v1 契约](../../independent-behavior-graph.md#c-g5-cal-v1) 全文。K1/K2/K3/K4/K2-C4 的既有证据不得外推；本计划引用的全部数字均来自已登记文档，不新增事实。

## 一、目标与边界

| 项 | 本计划交付 | 不覆盖 |
| --- | --- | --- |
| OAR 资源策略 | 三选一裁定（§二），决定 K5 采样与姿态对照设计 | 任何 MO2/模组列表实际改动（Track 2 方向须用户批准，§十） |
| K5 仪器范围 | 候选/源两侧完整采样、层/权重/混合审计、寿命与 generation 对账的范围与固定格式（§三、§四） | 仪器实现（评审后另立实现预注册） |
| 控制就绪清单 | 控制 A / 控制 B 的 READY 边界与缺口（§五） | CAL2 三进程/120 代驱动协议（另行预注册） |
| 预算 | 按 CAL v1 §五.7 预先固定每进程工件上限（§四） | — |
| K5 判据 | 预注册判据 K5-1…K5-8（§六） | 姿态等价结论、`tau_*`、G5 关闭 |

**明确不做**：不产生 `tau_p/tau_r/tau_s/epsilon_t`；不关闭 K1/K5/K6/G5；不把 K5 判据通过说成姿态等价；不改写 K1-C4 的 NOT MATCHED；不做任何跨侧姿态比较（Track 1 无跨侧配对）；不改产品代码路径（仪器全部默认 off，off 时输出与 v102 逐字节一致）。

## 二、OAR 资源策略（K1-C4 NOT MATCHED 的裁定）

证据基线（全部为已登记事实）：

1. 两侧 clip **家族**对齐（equip=`1HM_Equip`、unequip=`1HM_Unequip`，K2-C3），但 `data FNV` 六族中仅 SCAR dummy 一族同字节；
2. 源侧 OAR 在窗口内自行切换资源（MT_Idle 20.0 s→18.0 s、MLh_Idle/1HM_Idle 短版→长版，binding-index 同步漂移），且 panel-open 时刻 idle 栈随世界状态变化（SVFI `MT_Idle` 栈 vs Smooth 1HM 栈，两轮不同）；
3. leaf name 与 binding-index 均不能当跨阶段/跨运行身份，只有 `data FNV`（或完整路径+内容哈希）可区分资源；
4. K1-C1 已登记当前模组全量 winner map（24,204 生效路径含 SHA-256）与 14 个观测 FNV 的逐条落位；
5. 行为链闭包 23 文件与 A1 逐位一致——OAR 替换只作用于动画数据，不改图结构；clip 家族选择由图结构决定，不随资源变化。

### 三选项分析

| 选项 | 内容 | 能解决 | 不能解决 | 单独采用后果 |
| --- | --- | --- | --- | --- |
| **A 冻结断言** | 保持现模组；K5 每个相位边界的活动 clip `data FNV` 必须落在 K1-C1/v101/v101b/v96a 已登记集合内（按相位登记**集合**而非单值，容纳源侧合法自切）；出集 FNV 记 drift 行 | 检测 OAR 漂移、防止采样被污染；保持冻结场景（K2-C1/C2、v96a 两次复现链）全部连续性 | 源/候选同名 clip 仍是不同资源——**同资源配对在定义上不成立** | 姿态对照无可比对 |
| **B 关闭动态替换** | 姿态对照批次改用关闭 OAR 替换的 MO2 档（或等效最小配置），两侧加载同一份确定性资源；批次前用 v101 census 仪器重登记资源集 | 使源/候选同资源配对**在定义上成立**（同 `data FNV`）；姿态等价有明确定义 | 改变源侧世界状态（用户可见环境变更）；冻结场景的 OAR 命中登记作废、需新档重验 hands/drawn/armor 逐字一致（K2-C2 的变量/事件账本不受影响） | 触碰用户环境，须批准 |
| **C 分组比较** | 仅在 `data FNV` 相同的组内配对；不同组标 NOT-PAIRED，排除出容差样本；同名不同实例以 stable-id 消歧（census 已证必要：MT_Idle 同节点两实例） | 资源身份记账正确，绝不误配 | 现模组下同字节跨侧组仅 SCAR dummy → 跨侧姿态配对几乎为空 | G5 姿态等价不可证伪 → 按门 1 规则"若无法建立可观测配对，停止正式路径，另立原生输出/混合证据审计范围" |

### 裁定：两轨制

- **Track 1（K5 仪器资格采样，本计划范围）= A + C**：现模组、现冻结场景。K5 的对象是**仪器完整性**（计数/层/权重/混合/寿命对账），不是跨侧姿态等价；保持 v96a/K2/K3/K4 证据链连续。A 的断言集 + C 的分组记账规则进 K5-7 判据。
- **Track 2（姿态对照批次，K5 之后另批）= B + C**：关闭 OAR 替换的新批次，自带 census 重登记与新档 hands/drawn/armor 重验；C 的分组规则继续作为记账防线（任何残余条件性替换按组排除）。层/权重/混合资格结论可从 Track 1 **转移**至 Track 2（依据：行为链闭包逐位一致，混合语义由图结构决定，不随动画数据变化；该转移声明写入 Track 2 预注册并接受复核）。
- **回退**：若用户否决 B（不愿改 MO2 档），Track 2 退化为 C-only——现状下配对组仅 SCAR dummy，G5 姿态等价按门 1 规则**停止正式路径**，另立审计范围；该后果如实登记，不放宽判据、不以 dummy 姿态宣称等价。

**Track 2 不在本计划实施范围**：其 MO2 档、census 重登记与新档重验在 K5 关闭后于 Track 2 预注册中冻结，属用户批准事项（§十）。

## 三、K5 仪器范围

### 3.1 候选侧（inject 模式，冻结场景 = Save116 + 1H + Candlelight 收鞘 + v96a 链）

| 观测 | 内容 | 复用/新增 |
| --- | --- | --- |
| 逐代完整姿态 | 每个 Generate 返回的 `e_post_sample` 全 116 轨 3+4+3 分量快照，无稀疏、无隐式 cap；每代下一次更新前两读，逐位一致才计入（v98/v99b 既证语义：两读等值标志 + 单 pose 存储） | 复用 v98 记录格式骨架（§四）；v98 已是逐代全量（249 条），K5 扩展的是对账、断言与层审计，不是采样密度 |
| 逐代 clip/贡献身份 | 每条记录的活动 clip 集合（`clip_count∈{1..4}` 已证）逐 clip 附 `data FNV`/binding-index/map-FNV/stable-id | 新增列（v98 仅计数量），格式版本号变更 |
| 层/权重/混合审计 | 4 个审计点（post-activate、equip 中部、equip settle、unequip complete）：全活动 generator 节点枚举（复用 census 512 上限 walk）× clip-ref 槽位清单**交叉对账**；可观测字段逐项记（节点类/激活序/clip 槽位/map-FNV/blendHint）；逐骨/通道权重与 blend/mask/additive 语义若无已审计布局 → **登记显式缺项清单**，不猜偏移。门 1 规则保留：active 但零权重不算有效贡献；不要求每层独占骨 | **新增**（现无任何层/权重仪器；v24 槽位清单是唯一前身，cap 3→8） |
| 寿命/generation | generation 计数逐代连续登记；clip 激活/失活窗口；generation 失效→取消语义（本场景若不触发，登记"未触发"，不记"通过"） | 新增对账；K3 的 seq 单调性保留 |
| 资源断言（Track 1） | 每相位边界活动 clip `data FNV` ∈ 登记集合（v101 候选集 ∪ v101b 源集 ∪ v96a/v102 源栈并集，按相位分列） | 新增断言，基于 K1-C1/K1-C4 登记 |

### 3.2 源侧（自然游玩，v99b 基础）

| 观测 | 内容 | 复用/新增 |
| --- | --- | --- |
| 逐 apply 完整姿态 | 等价完成边界上每次 apply 的全 116 轨快照（v99b 记录 5,268 B） | 复用 |
| 逐条 clip 身份 | 每条记录的活动 clip `data FNV`/时长/binding | 新增列（v99b 无逐条 clip 身份），格式版本号变更 |
| 寿命窗口 | clip 激活/失活边界的 FNV 变化登记（源侧 OAR 自切资源的可见形态） | 新增 |
| 漂移断言 | 逐条 FNV ∈ 源侧登记集合；出集 → drift 行，受影响的后续记账（不进姿态样本——Track 1 本无姿态配对，drift 行是 Track 2 配对资格的排除依据） | 新增 |

### 3.3 对账账本（两侧共用）

按 CAL v1 §五.6：每轮输出对账账本，键 = 快照/骨/分量/clip/贡献实例，列 = expected/visited/recorded/missing/dropped/truncated；工件 SHA-256；全部上限与丢失可见；非有限值、重复/缺失主键、无效映射单列，不进任何误差分布，不计为成功或零误差。

## 四、记录格式与预算（按 CAL v1 §五.7 预先固定）

| 工件 | 格式 | 上限/进程 | 超限 |
| --- | --- | ---: | --- |
| 候选姿态工件 | 新格式 v1：v98 布局 + 逐代逐 clip 身份列；header 版本号变更，字节布局在实现预注册冻结 | 预期 1 基线 + 120 equip + 129 unequip ≈ 250 条 × 5,280 B ≈ **1.26 MiB** ≤ 8 MiB | 整轮无效，保留已收计数，不截断续裁 |
| 源姿态工件 | 新格式 v1：v99b 布局 + 逐条 clip 身份列；同上冻结 | **cap 1,400 条 ≈ 7.0 MiB** ≤ 8 MiB；cap 在 unequip 完成前耗尽 → 该轮 INVALID（v101a 判例） | 同上 |
| 对账账本 | JSON，键/列固定 | ≤0.4 MiB | 同上 |
| 层/权重审计工件 | 文本行 `K5 LAYER`，字段顺序实现预注册列全 | 预期 4 点 × ~122 节点 ≈ 490 行，cap 1,024 行 ≤0.5 MiB | 同上 |
| 身份/时间清单 | 现有运行清单模式（DLL/PDB/INI/存档/manifest 哈希） | ≤0.1 MiB；三项合计 ≤ **1 MiB**（CAL v1 身份/时间/对账桶） | 同上 |
| 文本摘要 | `K5 SUMMARY` 行 | ≤200 行 | 同上 |

- 双读规则：每代两读**逐位一致**才计入；不一致先判撕裂/生命周期问题，该代单列并审计，不进正常分布（CAL v1 §七 N 规则）；未解释撕裂 → INVALID。
- 精度：沿用 `pose_capture_policy.h` 六位小数 track 文本与 raw-bits 保留规则；表示界 Q 的适用前提（两侧均六位最近舍入）由实现预注册复证。
- 新 INI 键（名称实现预注册冻结）：`K5FullSample` / `K5LayerAudit` / `K5Reconciliation` 三开关，默认 off；off 时全部输出与 v102 逐字节一致。
- 格式/预算变更只允许发生在任何采集之前；采集后变更 → 新批次另登记（CAL v1 §五.7）。

## 五、两个控制的就绪清单

| 控制 | 当前状态（已登记） | K5/CAL2 前缺口 |
| --- | --- | --- |
| **A 解析采集控制** | `CALIBRATED_OFFLINE_A_ONLY`（`capture_pose_sample` → `.6f` 文本 → 离线度量；raw bits 逐位吻合、无 poison） | 无缺口。约束：K5 不得改动该采集/格式路径；若改，先重跑 A 再采新数据 |
| **B 原生单层控制** | READY（bounded）：`female\mt_idle.hkx` 113,920 B `0EF8248C…BCD73C7`、9 个固定时间、候选图 MT_Idle binding（`binding_index=1022`、map 0..98）、单层直接 `SamplePartialTracks`、v100a 进程；B2 PASS（891 轨 / 8,910 分量 / 0 violation / 最大比值 0.4860），B1 位精确 FAIL 保留在案 | **进程数**：CAL v1 §五.1 要求 B 三个独立进程，现有合格进程 1 个（B2）。缺口 2 个，由 CAL2 协议补齐（另行预注册）。K5 不要求、也不宣称 B 已三进程 |

## 六、K5 运行协议与预注册判据（实现评审后冻结执行细节）

**运行协议（草案）**：两侧各一次窗口、一次会话：

1. 候选侧：v102 式部署（v96a 基线 INI + K5 三键），Save116 加载 → inject → prime（同帧 5 写 + `WeapEquip`）→ equip 120 步 → settle 87/101 → `Unequip` id=144 → unequip 129 步（completed=129 链为既证协议）→ 关窗。
2. 源侧：v101b 式自然游玩（F7 20 s 窗口：装备→稳定→卸下→稳定），记录 cap 1,400。
3. 运行后离线：对账账本生成 + 资源断言核对 + 层审计交叉对账 → 逐判据裁定。

| # | 判据 | 通过条件 | 否则 |
| --- | --- | --- | --- |
| K5-1 完整采样 | expected==visited==recorded；missing=0、dropped=0、truncated=0；全部上限可见且除声明 cap 外无隐式上限 | 该轮 INVALID |
| K5-2 双读一致 | 全部逐代 `two_read_equal=1` | 未解释撕裂 → 该轮 INVALID |
| K5-3 分母完整 | 每条记录 116/116 轨（已登记 skeleton）；reference/default-fill 轨保留；重名以骨架/索引/父路径消歧 | INVALID |
| K5-4 对账闭合 | 账本计数与工件字节数/哈希互洽；全部工件 SHA-256 登记 | INVALID |
| K5-5 层/混合资格 | 审计点活动节点枚举与 clip-ref 清单交叉对账 missing=0；可观测字段逐项登记；不可观测字段列**显式缺项清单**（门 1 未闭合子项如实列明） | 缺项清单之外再发现未声明不可观测 → INVALID |
| K5-6 寿命/generation | generation 逐代连续；clip 激活/失活窗口完整；失效/取消未触发则登记"未触发" | 未声明的失效/取消 → INVALID |
| K5-7 资源断言 | 全部活动 clip `data FNV` ∈ 登记集合；出集 → drift 行 + C 规则记账排除 | 未声明的出集 FNV → INVALID |
| K5-8 运行完整 | 零 `[E]`、无崩溃、DLL/PDB/INI/存档身份匹配、预算未超 | 任一不满足 → 该轮 INVALID |

**裁定语义**：K5-1…K5-8 全过 → **K6 关闭**（K5 仪器资格成立，限定 Track 1 场景）；仍不产生 `tau_*`、不关闭姿态等价/G5、不等于 CAL2 READY。K5-5 的缺项清单决定门 1"层/权重/混合"子项是闭合还是带缺项转 Track 2/CAL2——两种结局都如实登记，不写成已闭合。

## 七、分析版本与工具

- 离线解析脚本入仓（实现预注册定名，如 `tools/k5_reconcile.py`），运行记录登记脚本版本 + SHA-256；分析只读运行工件，不改判据。
- 分位数/度量/容差推导规则全部引用 CAL v1 §六/§七；本计划不新增任何 epsilon 或阈值。
- 工件解析失败 → 对账行登记，不静默跳过。

## 八、评审后的实施顺序与提交拆分

1. **实现预注册**：代码触点、键名、`K5 LAYER` 字段全表、新工件 header 字节布局——先冻结再动代码。
2. K5 仪器实现（默认 off；off 路径逐字节不变断言）。
3. 构建 + 部署 v103 诊断档（v102 三件套备份先行）。
4. 候选侧 + 源侧各一次运行（§六协议）。
5. 离线对账 + 判据裁定 + 登记（含偏差登记，如有）。
6. CAL2 三进程/120 代驱动协议预注册（独立于 K5）。

提交拆分：`docs(g5-k6): 预注册 K6 计划（评审通过版）` → `feat(g5-k5): 仪器实现` → `docs(build): v103 构建部署` → `docs(g5-k5): 运行记录与裁定`。

`Documentation Impact: update`：K6 关闭时同步 [independent-behavior-graph.md](../../independent-behavior-graph.md) 账本行（K6 状态）与文首状态行、[README](../../README.md)、[history](../../history.md)、[CHANGELOG](../../CHANGELOG.md) 入口；不可回写旧运行结论。

## 九、待用户决策事项

| # | 决策 | 影响 |
| --- | --- | --- |
| 1 | 本计划评审（重点：§二两轨制裁定与 Track 2=B 方向确认） | 未评审不实施 |
| 2 | Track 2 关闭 OAR 替换的 MO2 档（K5 之后才需要；否决则姿态等价按门 1 停止规则另立审计范围） | 用户环境变更，需批准 |
| 3 | （会话外既挂）52 提交 push 与否；`build/hkx-audit/`、`build/k1-c1/` 遗产收编与否 | 与 K6 无耦合 |

## 十、实现预注册（2026-11-14，评审通过后冻结；实现前零采集）

本节冻结实现细节：代码触点、键名、字节布局、行格式与预算精确值。所有变更先于任何采集；采集开始后本节不改（变更 → 新批次）。

### 10.1 新增 INI 键（`[Experiment]`，全部默认 off；off 时输出与 v102 逐字节一致）

| 键 | 类型 | 语义 |
| --- | --- | --- |
| `K5FullSample` | bool | 快照写入器切换为 **K5 格式 v1**（magic `CPK5`）。与 `ObservationSnapshot` 共享全部触发点/两读规则/轨道提取；同 on 时 K3 写入器被抑制并输出 `K5 MODE k3-suppressed` 声明行。候选侧逐代 `e_post_sample`、源侧逐 apply（`SamplingLiveness=on` 且 `record_k3s_return` 命中 pinned 时） |
| `K5LayerAudit` | bool | 候选侧 4 个审计点（§10.4）输出 `K5 LAYER`（逐节点）+ `K5 AUDIT`（摘要）行 |
| `K5Reconciliation` | bool | 进程内对账计数器；窗口/实验关闭时输出 `K5 RECON` 行 + JSON 账本工件（≤0.4 MiB）。`K5FullSample` on 时自动生效（写入器 close 行即含对账）；此键独立用于源侧 F7 窗口 |

### 10.2 代码触点（7 处，不触碰产品路径）

| # | 文件 | 改动 |
| --- | --- | --- |
| 1 | [settings.h](../../../src/animation/settings.h) / [settings.cpp](../../../src/animation/settings.cpp) | 3 键 getter + 3 条 `EXPERIMENT setting` 日志行（模式与 EquipmentStateDump 完全一致） |
| 2 | `k5_snapshot_policy.h`（**新文件**，src/animation/behavior_graph/） | K5ClipRecord/K5Header/K5 record 布局 + `K5SnapshotWriter`/`K5SourceWriter` + 身份缓存 + 对账账本结构 |
| 3 | [independent_graph.cpp](../../../src/animation/behavior_graph/independent_graph.cpp) | `record_k3_snapshot` 分支（K5 on → K5 writer，同一两读/轨道提取）；审计点 ×4；关闭行 `K5 SNAPSHOT close` + `K5 RECON` |
| 4 | [probe_liveness.cpp](../../../src/animation/behavior_graph/probe_liveness.cpp) | `record_k3s_return` 分支（K5 on → K5 source record，in-detour 身份缓存）；drain/close 行 `K5 SOURCE close` |
| 5 | [control_sample.h](https://github.com/AmazingBuff/PlayerPanel/blob/b4c8c39b0ebffe2bfdd9e8f04721b14b636ed34b/src/animation/behavior_graph/control_sample.h) / .cpp | **零改动**（复用 `read_animation_resource_identity`；不新增字段不改签名） |
| 6 | [config/CharacterPanel.ini](../../../config/CharacterPanel.ini) | 3 键默认 off + 注释（风格同 EquipmentStateDump 条目） |
| 7 | 文档（实现提交内） | 本节已在文档冻结，无需另改 |

### 10.3 字节布局（pack(1)，全部 static_assert 到位）

**K5ClipRecord = 124 B**（K3ClipRecord 76 B + 身份列）：

```
node_ptr u64 | binding_ptr u64 | binding_index u32 | local_time_bits u32
mode u8 | pad[3] | name[64] | duration_bits u32
data_fnv u64 | blockoff_fnv u64 | map_fnv u64 | map_count u32
```

进程内 stable-id 三组件（node/binding 指针 + binding-index）入记录，离线可重组；`data_fnv` 是 K1-C4 的资源身份；`duration_bits` 是缓存校验字段。

**候选记录 K5Record = 5,664 B** = meta 32（segment/completed/commanded/track_count/two_read_equal/clip_count/elapsed_raw，同 K3 meta）+ 8×124 clips + 116×40 tracks。`K5Header` 布局同 `K3Header`（116 B，magic=CPK5，version=1，track_capacity=116），**record_capacity = 384**。

**源记录 K5SourceRecord = 5,652 B** = meta 20（tsc/seq/track_count/clip_count/path/two_read_equal/flags/pad，同 K3S meta）+ 8×124 clips + 116×40 tracks。`K5SourceHeader` 同 `K3SourceHeader`（124 B，record_size 字段），record_capacity = **1,400**。

**预算精确值（修正 K6 计划 §四 的近似）**：候选 cap 384×5,664 = 2,174,976 B ≈ **2.07 MiB**（预期 250 条 ≈ 1.35 MiB）；源 cap 1,400×5,652 = 7,912,800 B ≈ **7.55 MiB**（计划稿按 5,268 B 估 ≈7.0，精确布局后 7.55，仍在 8 MiB 内——cap 条数不变）。flag 位沿用 `K3S_Flag_Clip_Overflow=1<<0`、`K3S_Flag_No_Pose=1<<1`，新增 `K5_Flag_Identity_Failed=1<<2`（binding/动画不可读，FNV 列=0）。

### 10.4 身份缓存与审计点

**身份缓存**（`k5_snapshot_policy.h` 内，两实例互不共享）：固定 64 项 ring，键 = binding 指针；未命中 → 调 `read_animation_resource_identity` 填入；命中时校验 `duration_bits/data_bytes/map_count` 三字段，不等则重算（覆盖同址 ABA 的常见形态；逐 apply 重哈希不可负担）。distinct bindings > 64 只降性能不错记（逐出后重 sight 重算）。源侧实例在 detour 内使用（`read_animation_resource_identity` 无分配、全部 deref 有 `readable()` 守卫，逐字段核对后可 detour 调用）；候选侧实例在 drive 线程。

**审计点（候选侧 4 处，`K5LayerAudit` on 时）**：

| # | phase | 位置 |
| --- | --- | --- |
| 1 | post-activate | census `post-activate` 同点（independent_graph.cpp ~L1575） |
| 2 | equip-mid | equip 段 `observation.completed()==60`（checkpoint 值） |
| 3 | equip-settle | settle 达成后、`Unequip` 派发前 |
| 4 | unequip-complete | unequip 段 outcome 转 completed 的同一 post-sample |

每点：activeNodes 全走（cap 512，census 风格）逐节点一行；vtable 精确匹配分类：`clip`（VTABLE_hkbClipGenerator）/`state-machine`/`manual-selector`/`modifier-gen`/`pose-match`/`behavior-graph`/`generator-unclassified`（精确 VTABLE_hkbGenerator）/`node-other`（VTABLE_hkbNode）/`unknown`（记 vtable 地址）。clip 节点行附 §10.3 全部身份列。**`hkbBlendingBehavior` 无 CommonLibSSE 布局与 VTABLE 常量 → 只能落 `unknown`，逐骨/通道权重与 blend/mask/additive 语义登记为显式缺项清单（K5-5 的预期缺项，实现不猜偏移）**。摘要行 `K5 AUDIT … walked=… clips=… cross-clips=… missing=… unclassified=… overflow=…`；cross-clips = `collect_active_clips()`（cap 256）同点读数，missing = 审计 walk 中 256-cap 看不到的 clip 数。

### 10.5 K5-7 资源断言的执行者修订

运行时**只记 drift 事实，不做集合裁定**：记录与审计行携带全部 `data FNV`，对账计数器只统计 `identity-failed`（读不出身份）。"`data FNV` ∈ 登记集合"的比对与 drift→INVALID 判定在**离线**执行：`tools/k5_reconcile.py`（实现提交内入仓，版本+SHA-256 随运行记录登记）内置 v101 候选 7 FNV / v101b 源 8 FNV 登记表（按相位分列）+ 已声明源侧栈名集 {MT_Idle, 1HM_Equip, MLh_Idle, 1HM_Idle, 1HM_Unequip, SCAR_1hmReadyDummy}；候选侧出集 → INVALID，源侧出集但 clip 名 ∈ 已声明栈 → 登记为声明缺口（K1-C1 已登记"源侧栈随世界状态变化"），出集且名不在栈名集 → INVALID。修订理由：源侧 Smooth 栈（v96a 时点）的逐 clip FNV 未在 census 登记，运行时内嵌不完整集合会把已声明现象误判为未声明漂移；裁定放离线符合"代码不携带裁定"纪律。**判据文本 K5-7 不变，只明确执行者。**

### 10.6 对账账本

`K5 RECON side=… expected=… visited=… recorded=… missing=… dropped=… truncated=… two-read-bad=… clip-overflow=… identity-failed=… drift=… invalidations=… reasons='…'`；JSON 工件 = 同字段 + 逐 invalidation 原因计数 + cap 表 + 工件 SHA-256，≤0.4 MiB。`reasons` 来自 `observation.invalidate` 调用点的既有 reason 字符串（不改写）。

### 10.7 判据与不变量

- 判据仍为 K5-1…K5-8（§六），本节不改任何通过条件。
- **off 路径字节不变断言**：三键全 off 时，本次改动产生的全部输出行不存在；既有输出（含 `K3 SNAPSHOT`、`K1 CENSUS`、`ANIMATION WORLD`、`INDEPENDENT GRAPH` 各行）逐字节与 v102 同键位行为一致。构建后以 v96a 基线 INI（全 off）+ K5 三键 on 各起一次短窗验证（不采正式数据）。
- K3/K5 互斥：`K5FullSample` on 且 `ObservationSnapshot` on → K3 抑制行；只 K3 on → 行为与 v98 完全一致（K3 代码路径零改动）。

## 十一、v103 run 1（候选侧）记录（2026-11-14 22:00–22:01）

**运行身份**：v103 `CharacterPanel-scenegraph-v103-g5k5-fullsample-9e83ff8e8f-cl94faaed0c6-20261114T134329Z`（DLL `8c3857a3…` / PDB `49581217…` / inject 运行档 `76f2bebb…`，见 [build.md v103 节](../../build.md#v103-g5k5-fullsample-20261114)）。存档 = Save116（Elif #116）；panel 窗口链 = v96a/v102 同链：inject → 同帧 5 写 prime + `WeapEquip` → equip 120/120（`fixed-equip-budget`）→ settle target 87 / settled 101（`baseline-default=1 baseline-nodes=122`）→ `Unequip` id=144 → unequip completed=129（`state-12-to-0`）。`hands=(560c2a4c,00043324) drawn=false` 与 v102 run 1 **逐字一致**；源侧 panel-open OAR 栈本轮 = SVFI（K1-C4 已登记的世界状态漂移现象，第三次观测）。零 `[E]`、无崩溃（clean tail）。

| # | 判据（候选侧） | 读数 | 裁定 |
| --- | --- | --- | --- |
| K5-1 | expected==visited==recorded | `K5 RECON side=candidate expected=249 attempted=249 recorded=249 missing=0 truncated=0 dropped=0`；分段 equip 120 + unequip 129（与 v98 的 249 一致） | **PASS** |
| K5-2 | 双读逐位一致 | `two-read-bad=0`（全 249 代） | **PASS** |
| K5-3 | 分母完整 | 全部记录 `track_count=116`（离线复核 0 失配）；reference/default-fill 轨在 raw 位值中保留 | **PASS** |
| K5-4 | 对账闭合 | 工件大小精确 = 116 + 249×5,664 = 1,410,452 B；JSON 账本与 RECON 行互洽；`clip-overflow=0 write-failed=0 identity-failed=0 drift-records=0` | **PASS** |
| K5-5 | 层/混合资格 | 4 审计点齐全（post-activate/equip-mid/equip-settle/unequip-complete），`missing=0 overflow=0 identity-failed=0` 全部；walked=97/94/122/113。unknown 类 328 行（约 12 个 vtable 地址，主簇 `7ff6a66306d8`×142、`7ff6a6630aa0`×84）= 无 CommonLibSSE VTABLE 常量的类（`hkbBlendingBehavior` 等）——**显式缺项清单按 §10.4 声明成立**，逐骨/通道权重与 blend/mask/additive 语义带缺项转门 1 未闭合项 | **PASS（带缺项清单）** |
| K5-6 | 寿命/generation | 逐代连续无缺口（equip 1..120、unequip 1..129 离线复核）；clip 激活窗口完整可导出（见下表）；失效/取消**未触发**（两段 outcome=completed、零 invalidation）——登记"未触发"，不记"通过" | **PASS（取消未触发，如实登记）** |
| K5-7 | 资源断言 | 离线对账 7 个唯一 (名,bidx,FNV,map_count,map_fnv)，**7/7 data FNV 命中 v101 登记集合**（含 Vanargand MT_Idle map_count=99 + map FNV `077954859409395E`）；出集 = 0 | **PASS** |
| K5-8 | 运行完整 | 零 `[E]`、无崩溃、DLL/PDB/INI 身份匹配、预算未超（2.07 MiB cap 实际 1.35 MiB） | **PASS** |

**clip 激活窗口（离线自 bin 导出；层重叠 = 门 1 预期形态）**：

| 段 | clip（裸名） | data FNV | bidx | 窗口（completed 步） | 条数 |
| --- | --- | --- | ---: | --- | ---: |
| equip | mt_idle（Vanargand） | `F56EE150…` | 1022 | 1..1 | 1 |
| equip | 1hm_equip | `12A77CD1…` | 22 | 1..98 | 98 |
| equip | 1hm_idle / mlh_idle / scar dummy | `C62C233B…` / `DAD1F1C9…` / `8728A54A…` | 23/949/15073 | 87..120 | 各 34 |
| unequip | 1hm_unequip | `4DED5EAF…` | 75 | 1..129 | 129 |
| unequip | 1hm_idle / mlh_idle / scar dummy | 同上 | 同上 | 1..12 | 各 12 |
| unequip | mt_idle（Ashe） | `722C8A1E…` | 11915 | 129..129 | 1 |

equip 段 87–98 为 `1HM_Equip` 与 idle 栈的重叠混合（settle 点前后混合带）；unequip 段 1–12 为 idle 栈 blend-out 残留；step 129 回到 MT_Idle（Ashe）与 state-12→0 完成点一致。

**工件**（归档于 [docs/diagnostics/](../)）：

| 文件 | 字节 | SHA-256 |
| --- | ---: | --- |
| [日志](../CharacterPanel-g5k5c-v103-run1-20261114-2200.log) | 247,907 | `c867daf6939f673552d9cd1b89873e873a5669f4daa4bf6114d3e5d810a12d2b` |
| [CPK5 快照](../CharacterPanel-g5k5c-v103-run1-20261114-2200.bin) | 1,410,452 | `71998840f32b3725fe6944e9721b5fdf772dbbacfde83393be781135979f0be5` |
| [JSON 账本](../CharacterPanel-g5k5c-v103-run1-20261114-2200.json) | 771 | `48b7d295922649f24ab3e8598ce6179198f868e07365a510feb92082bdfe2a97` |

**离线对账工具**：[tools/k5_reconcile.py](../../../tools/k5_reconcile.py)（SHA-256 `c977c9ea1ce67c0c7070884749ec071c781cd95c330a54c917e194f6e978a9af`）。登记两条实现事实：① 运行时 `animationName` 为完整动画路径（`Animations\1HM_Equip.hkx`），节点 `hkbNode::name` 才是裸名（`DG_1hmEquip` 等）——工具以裸名（剥前缀/扩展名、大小写折叠）作匹配键、data FNV 为身份（K1-C4 不变），属比较侧归一化，不判据变更；② SCAR dummy 的 binding-index 本轮 = 15073（v101 = 15078）、MT_Idle = 11915（v101 = 11909）——跨运行 binding-index 漂移再现，K1-C4"binding-index 非身份"结论再次成立。

**裁定（候选侧）**：K5-1/2/3/4/6/7/8 = PASS，K5-5 = PASS（带缺项清单）。**K5 整体仍开放**——源侧（run 2：`SamplingLiveness=on` + source 档，F7 probe 窗口）未运行；`tau_*` 不产生；姿态等价/G5 不关闭。

## 十二、v103 run 2（源侧）记录与 K6 收官（2026-11-14 22:29–22:30）

**运行身份**：v103 同 DLL（`8c3857a3…`）+ source 运行档 `21bd938b…`（`IndependentGraph=off`、`SamplingLiveness=on`、`K5FullSample=true`、`K5Reconciliation=true`）。登记一条过程事实：22:25 的第一次启动仍加载 inject 档（换档未落位），无任何 K5 数据产生，作废重跑；换档后本轮为首个源侧窗口。存档 = Save116；F7 probe 窗口 20 s 正常 `expired`，`pinned-apply-entries=906 = paired-completions=906`（配对零失配）。零 `[E]`、无崩溃。

| # | 判据（源侧） | 读数 | 裁定 |
| --- | --- | --- | --- |
| K5-1 | 完整采样、cap 未在完成前耗尽 | `K5 SOURCE close records=906 attempted=906 dropped=0 overflow=false no-pose=0 write-failed=0 open-failed=false`；`K5 RECON side=source expected=906 recorded=906 missing=0 truncated=0`；cap 1,400 实用 906，**equip（56 条）与 unequip（67 条）及收鞘 idle 全部在窗口内** | **PASS** |
| K5-2 | 双读逐位一致 | `two-read-bad=0`（全 906 apply） | **PASS** |
| K5-3 | 分母完整 | 全部记录 `track_count=116`（离线复核 0 失配） | **PASS** |
| K5-4 | 对账闭合 | 工件精确 = 124 + 906×5,652 = 5,120,836 B（预算 8 MiB 实用 4.88 MiB）；JSON 账本与 RECON 行互洽 | **PASS** |
| K5-6 | 寿命/generation | seq 1..906 连续无缺口；TSC 严格单调；clip 时间线完整（见下表）；失效/取消未触发 | **PASS（取消未触发，如实登记）** |
| K5-7 | 资源断言 | 9 个唯一 (名,bidx,FNV)：**7 个命中 v101b 登记集合**（1HM_Equip `81FD1988…`、1HM_Unequip `4D885DE5…`、1HM_Idle `AE1527A6…`、MLh_Idle `13D0592A…`、短版 idle 对 `22C9C4BF…`、SCAR dummy `8728A54A…`）；**2 个声明缺口** = MT_Idle 新 FNV `08E65508BDEB4443`（bidx 12219）与 `C95CED0358CB90A6`（bidx 12222）——裸名在已声明栈名集内，按 §10.5 登记为声明缺口（K1-C4"源侧栈随世界状态切换"再现：MT_Idle 资源跨窗口第三、四个版本），不是未声明漂移；出集未声明 = 0 | **PASS（2 声明缺口登记）** |
| K5-8 | 运行完整 | 零 `[E]`、无崩溃、DLL/INI 身份匹配、预算未超 | **PASS** |

**clip 时间线（seq 1..906，离线自 bin 导出）**：

| seq 区间 | clip | data FNV | 裁定 |
| --- | --- | --- | --- |
| 1..217 | mt_idle（bidx 12222） | `C95CED03…` | 声明缺口（装备前 idle） |
| 209..264 | 1hm_equip | `81FD1988…` | REG |
| 257..354 | mlh_idle / 1hm_idle（短版，bidx 11611/9040） | `22C9C4BF…` | REG |
| 257..598 | scar_1hmreadydummy | `8728A54A…` | REG |
| 355..598 | mlh_idle / 1hm_idle（长版，bidx 8975/8995） | `13D0592A…` / `AE1527A6…` | REG |
| 592..658 | 1hm_unequip | `4D885DE5…` | REG |
| 649..906 | mt_idle（bidx 12219） | `08E65508…` | 声明缺口（收鞘 idle） |

短版→长版 idle 栈切换（355 处）与 v101b 观察的 OAR 窗口内自切资源现象一致；1HM_Equip/1HM_Unequip 与 idle 栈的重叠区间为层混合带（门 1 预期形态）。**旁注（不改变既有裁定）**：本窗口 `b3-equal=906/906`、`b4-equal=906/906`——B3/B4 在第二会话同链再现，K4 的 v97d 登记范围不变，此为额外复现观察。

**工件**：

| 文件 | 字节 | SHA-256 |
| --- | ---: | --- |
| [日志](../CharacterPanel-g5k5s-v103-run1-20261114-2229.log) | 265,950 | `bdfbd64fec6cc1ea4152c651f1dc232fab8c0e1c2b39b3815a5b6312154dd5e0` |
| [CPK5 源快照](../CharacterPanel-g5k5s-v103-run1-20261114-2229.bin) | 5,120,836 | `44d5c33723346fc4daaa108c62d1fb99b23bb0edb70e7b7176c06fc8304c4c53` |
| [JSON 账本](../CharacterPanel-g5k5s-v103-run1-20261114-2229.json) | 618 | `91849ee6a7aa7fe4e511da8957b0286d6c7dfcc5df3a01e6d582d405246162e4` |

## 十三、K6 关闭裁定

**K5-1…K5-8 两侧全部 PASS → K6 关闭（限定范围）**：

1. **K5 仪器资格成立**：完整计数/层/权重（可观测面）/混合/寿命对账在候选与源两侧各一次会话取得零丢失、零截断、零未声明漂移的完整证据；写骨分母 116/116、双读逐位一致全部成立。
2. **K6 关闭即 CAL v1 §三 K1–K6 全部有证据**：K1-C1/C2/C3/C4 登记（C4 NOT MATCHED）、K2-C1/C2/C3/C4 登记/READY、K3 两侧、K4 本会话边界 + 本轮 b3/b4 复现、K5 收官、K6 本文档。**CAL1 资格门形式上齐备，但 K2-C4 控制 B 仍为 bounded READY（1 个合格进程，3 进程要求归 CAL2）、K1-C4 NOT MATCHED 与 K5-5 缺项清单是姿态对照/校准的实质限制**——姿态等价与门 3 原生校准仍不可启动，下一步按 §二两轨制推进：**Track 2 预注册（关闭 OAR 替换的对照批次，待用户批准 MO2 档）** 与 **CAL2 协议预注册**。
3. **带缺项转移清单**：① 逐骨/通道权重与 blend/mask/additive 语义（`hkbBlendingBehavior` 等无 CommonLibSSE 布局，K5-5 缺项清单）→ Track 2/CAL2 的门 1 未闭合子项；② 源侧 MT_Idle 资源两新 FNV（`08E65508…`/`C95CED03…`）已并入声明清单，Track 2 的资源断言集合须包含或关闭该栈；③ 控制 B 三进程缺口 → CAL2。
4. **不产生** `tau_p/tau_r/tau_s/epsilon_t`；**不关闭** 姿态等价、G5、G6；装载 teardown NO-GO 不变。

**文档维护同步**：[主文档](../../independent-behavior-graph.md) 文首状态行与 [会话总结](../test-results-20261118.md#g5-qualification) 指针、[README](../../README.md) Current status、[CHANGELOG](../../CHANGELOG.md)、[history](../../history.md) 已同步本节。
