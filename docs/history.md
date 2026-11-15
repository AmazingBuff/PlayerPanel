# 历史研究总结

更新：2026-11-15（[CAL2 网格协议预注册](diagnostics/g5-cal2-20261115/README.md)）。**C-G5-CAL2 v1 评审通过（环境 = `AE-Track2` 档）**：控制 B 从 B2 的 9 固定时间单发采样（bounded READY，1 进程）重组为 [CAL v1 §五.1/§五.2/§五.6](independent-behavior-graph.md#c-g5-cal-v1) 完整形状——每进程 baseline(t=0) + 120 完成代（预定网格时间 t=k/60 直采，不取图 clip 时间以隔离[候选时钟阶梯形态](diagnostics/g5-gate4-20261115/README.md)）× 每代 2 读，共 3 独立进程；判据 C2-A..F，全过 = K2-C4 升网格 READY、CAL1 控制 B 资格就绪；`tau_*` 推导属 CAL1。同日[门 4 协议预注册](diagnostics/g5-gate4-20261115/README.md)：C-G5-G4 v1 评审通过且实现预注册/dry-run 完成（候选 247/247 配对、三锚全中、同参调用逐字节确定；候选过渡 clip localTime 阶梯/滞后形态移交时钟轴 PENDING-CAL 槽位）。CAL2 同日推进至构建完成：§九实现预注册 → v104 仪器（DLL `7d270e7d…`，测试 8/8，身份 `…-v104-g5cal2-grid-d61a6e19b9-…`）→ 网格真值 121×99 + BOUNDS 节（重叠锚点 0 失配）→ 运行档派生（`bbfe2b8b…`，逐键断言）；**部署完成；run 1–3/3 已上机并在 [CAL2 §十二](diagnostics/g5-cal2-20261115/README.md#cal2-c2e-oar-addendum) 的 OAR 精确豁免下通过 C2-A/B/C/E/F；三进程两两 119,790/119,790 逐位相同，C2-D PASS，K2-C4 升 grid READY；CAL1-B 已完成 CALIBRATED_BOUNDED（控制 B 单层姿态；`tau_p=0.000476592`、`tau_r=6.03341E-8`、`tau_s=0.00000386264`）；门 4 姿态槽位已填；完整 CAL1 仍 NOT_READY（K2-C1 PARTIAL、K5-5、`epsilon_t` TSC 频率/单位未登记）；** cal2_adjudicate.py 已实现并登记于 §十一；CAL1-C 审计：17 条 item/wing/Belly 轨无 CAL1-B 控制（99/116）；TSC invariant 已证但读取区间 ≠ `dt_clock`，`epsilon_t` 不能直接取读取区间；门 4 仅姿态槽位填充，`epsilon_t`/`Δt_clock` 仍 PENDING；G5/G6 不关闭，装载 teardown NO-GO 不变。

更新：2026-11-15（[门 4 协议预注册](diagnostics/g5-gate4-20261115/README.md)）。**C-G5-G4 v1 评审通过**：Track 2 同资源环境下的门 4 配对/比较协议结构冻结——相位括号配对（不做同刻逐代配对，[K3 收官](independent-behavior-graph.md#g5-cal-k3-closure)裁定的另立契约状态维持）、逐记录钟族优先级 + 域内 nearest-clock（元数据规则、独立于 pose 误差）、IDUN 整条排除与对账恒等式、G4P-1..9 判据、离线工件预算；`tau_*`/`epsilon_t` 与 `Δt_clock` 处置为 PENDING-CAL 槽位（门 4 完整冻结待 CAL1/CAL2 后填充，先于门 5 正式采集）；K5-5 缺项清单带入层/权重/混合轴阻塞，G5 等价裁定持续阻塞。下一步：（并行）CAL2 三进程/120 代协议预注册 → CAL1 校准 → 槽位填充 + 门 4 完整冻结 → 门 5 正式运行。**同日后续：实现预注册（§十四）与 dry-run（§十五）完成**——`tools/gate4_pair.py`（T1–T12 全 PASS）在 Track 2 Run 1/2 工件上 metadata-only 执行：候选 247/247 配对（E1 96 / idle 22 / U1 129 三锚全中）、恒等式闭合、零 wrap、K5-7 PASS、同参调用逐字节确定；登记候选过渡 clip localTime 阶梯/滞后形态（81/97、108/128 相邻同值 vs 源侧 8/71、9/117；两侧跨度端点逐位一致），移交时钟轴 PENDING-CAL 槽位。`tau_*` 未产生，G5/G6 不关闭，装载 teardown NO-GO 不变。

更新：2026-11-14 深夜（会话总结见 [diagnostics/session-summary-20261114-k6.md](diagnostics/session-summary-20261114-k6.md)）。本次会话（8 个提交）完成 **K6 计划评审通过（OAR 两轨制）→ K5 三仪器实现（v103）→ 候选/源两侧各一次运行 → K6 关闭（限定范围）**。候选侧 249/249 记录（7/7 data FNV 命中 v101 登记集）、源侧 906/906（7 命中 + 2 个 MT_Idle 声明缺口 `08E65508…`/`C95CED03…`），双读逐位全等、116/116 轨、零丢失零截断；K5-5 带缺项清单（`hkbBlendingBehavior` 等无 CommonLibSSE 布局 → 逐骨/通道权重与 blend 语义转 Track 2/CAL2）。CAL v1 §三 K1–K6 形式齐备，姿态等价与门 3 校准仍不可启动；下一步 **Track 2 与 CAL2 预注册**。`tau_*` 未产生，G5/G6 不关闭，装载 teardown NO-GO 不变。逐项判据与工件见 [K6 计划 §十一/§十二/§十三](diagnostics/g5-k6-plan-20261114/README.md)。

更新：2026-11-14（会话总结见 [diagnostics/session-summary-20261114.md](diagnostics/session-summary-20261114.md)）。本次会话（11 个提交）完成 **K2-C4 控制 B 方案 A**（= READY bounded）与 **K1 身份普查**（census 仪器 PASS，K1-C4 资源裁定 NOT MATCHED）。[C-G5-OBS v2](independent-behavior-graph.md#c-g5-observation-v2-20261112)的两场景离线验证已完成；v96a 首次[原生运行](build.md#g5-cal-v96a-observation-v2-run1-20261112)取得 clip-family PASS 与 `state 12→0` PASS（completed=129）。源图后处理边界 **B3/B4 已在 v97d 单窗口 739/739 PASS（K4 本会话边界）**；K3 两侧已闭合（候选 K3-C v98 249 条 + 源 K3-S v99b 1,151 条，双读一致、116 轨、零溢出；跨侧逐代配对不在 K3）。**K2-C4 控制 B**：把运行时资源更正为 female loose `mt_idle.hkx`，用本地 HavokLib 产出独立 spline 真值；B1 位精确 FAIL 保留（8910 分量 60.81% 逐位一致、最大差 1.7643e-05，归因 float32 实现顺序），B2 用 authored 解析界在新进程 0/8910 violation PASS → **READY（bounded）**。**K1 身份普查**：v101 候选侧 K1C-1–4 与 v101b 源侧自然游玩 S1–S4 均 PASS（116 骨骨架/reference pose 齐全、活动 clip 身份无丢失），但源/候选除 SCAR dummy 外 `data FNV` 全不同、且源侧 OAR 窗口内自行替换资源 → **K1-C4 裁定 NOT MATCHED**（clip 家族仍对齐，leaf name/binding-index 不可作跨阶段身份）。K1-C1 完整清单、K5/K6、时间量化、姿态等价仍待验证，`tau_*` 未产生，G5 未通过。

- [渲染、展示实例与外观](history-rendering.md)：M0状态/合成、Actor与StudioHome、灯光、native Clone、AFP9取景/蒙皮、UBE，以及不同的回收边界。
- [动画、OAR与暂停行为图](history-animation.md)：HKX取数/采样/映射反转、第二Actor暂停推图、最终输出、OAR/DAC/IED、r1/r2遗留与无Actor独立图关卡。
- [144份来源与恢复索引](history-sources.md)：每份被清理原件的归属、证据限制、固定Git恢复方法与审计位置。

## 当前权威入口

| 要了解的内容 | 文档 |
| --- | --- |
| **当前状态、部署与后续规划（新会话先读这个）** | [11-12 OBS v2 离线结果与手工包](independent-behavior-graph.md#c-g5-observation-v2-20261112)优先；以下是 11-11 日期状态：[本轮 D1→D3 摘要](independent-behavior-graph.md#status-20261111-d3-session)、[最新运行摘要](independent-behavior-graph.md#status-20261111-session)、[当前部署记录](independent-behavior-graph.md#deploy-state)、[CAL v1](independent-behavior-graph.md#c-g5-cal-v1)与 [K1/K2 资格账本](independent-behavior-graph.md#c-g5-cal-k1k2)：安装 v95 + D3 候选 prime（v94 + D3 三件套备份）；D1/D2 判负，D3 v2 命中 `iRightHandType=1`/`iLeftHandType=9`，D3 候选 prime 取得 clip-family PASS（`1HM_Equip`/`1HM_Unequip`），1HM 卸下完成点待覆盖，K1–K6/原生 B/时间资格待补，数值容差未产生。**本轮暂停。** 历史交接与原日志保留 |
| **2026-11-14 会话总结（最新）** | [diagnostics/session-summary-20261114.md](diagnostics/session-summary-20261114.md)：控制 B 方案 A（B1 FAIL / B2 bounded PASS → K2-C4 READY）与 K1 身份普查（census PASS / K1-C4 NOT MATCHED）的完整轨迹、5 个版本部署哈希、11 个提交清单、当前 MO2 安装 v101b 与剩余前置；[控制 B 登记](diagnostics/g5-cal-control-b-20261114/README.md)、[K1 census 登记](diagnostics/g5-k1-census-20261114/README.md) |
| **全部测试与验证状态** | [最新运行摘要](independent-behavior-graph.md#status-20261111-session)与[验证总表](independent-behavior-graph.md#verification-matrix)：总表中的日期条目保留当时裁定；新增[离线 A 结果](diagnostics/g5-calibration-a-20261111/attempt-002/result.md)，仅限固定控制的采集/格式/解析与算术，不是原生或 G5 通过记录 |
| 构建、依赖和验证入口 | [Build](build.md) |
| 复制显示图、取景、装备与生命周期 | [SceneGraph](scene-graph.md) |
| 播放器、OAR查询、DAC原生事件 | [Animation](animation.md)、[OAR](oar-hooks.md)、[DAC](dac.md) |
| 默认产品的未决外观/装备问题 | [诊断交接第10节](diagnostics/display-issues-2026-10-22.md#10-2026-10-23-装备上下文与火把分层修复待上机)；[r1原始日志](diagnostics/CharacterPanel-issue-trace-r1-20261022-2231.log)、[r2原始日志](diagnostics/CharacterPanel-issue-trace-r2-20261022-2314.log) |
| **v46 正常路径上机（OBSERVED 2026-10-30）** | [操作与结果](scene-graph.md#v46-manual-regression)：实际使用 20261030 重建身份，不是 20261029 归档的逐字节验证；源窗口/读档取消、OAR 装备/火把/F7/F8/重开已观察，retained-total 1→14、13 时一次警告；未证明原生内存回收 |
| DCMF 参考（2026-10-30，后置） | [固定提交源码笔记](community-reference-supplement.md#dcmf-reference)仅参考、对照后置；v46 正常工作流已观察，尚无 DCMF 对照执行记录 |
| 后续独立图实验上机 | [v32 手工清单](independent-behavior-graph.md#v32-manual-checklist)：匹配身份与三键重启、连续 120 调用、候选语义/实际应用/画面分开、同窗口源对照、中断/重开/F7/来源失效及停止恢复规则；**第 1–5 步已于 2026-10-28 22:28 执行一次**（[v33 记录](independent-behavior-graph.md#g2-v33-run)），**第 6 步与第 7 步读档回落已由 v34 观察；同 controller 来源/generation 变更、确切拒绝原因和 frozen-only 路径待新数据**，部署状态见[当前部署与开关](independent-behavior-graph.md#deploy-state) |
| 独立图可行性与实际进度 | [当前六关结论](independent-behavior-graph.md#current-status)：G1/G2 有界通过，G3-B 相位回放及 G4 A/B/C 保留；E1 v3、E3 2/2、E4 四块快照、装备链速率及离线 A 不等于完整姿态一致。原生校准 NOT_READY，G5 未冻结/未通过，G6 待验，装载 teardown NO-GO |
| 工程约束、产品要求和研究来源 | [维护](code-maintenance.md)、[PRD](player-panel-prd.md)、[社区参考](community-reference-supplement.md) |

当前保留日志与恢复元数据见[诊断索引](diagnostics/README.md)；engine-text 与 TSLOT 工具未变。

## 2026-10-29 历史 checkpoint（以下部署/本地状态仅指当时）

原始记录按日期保留。最新记录安装为 [v41](build.md#v41-panel-gate-2026-10-29)（SourceTimeline=true、其余三键 off）；[v42](build.md#v42-residual-corrections-2026-10-29) 是新的本地构建，未部署。新 missing cap8 与一次拒绝原因只改进下一次取证，不补写旧日志中的三未知名或拒绝原因。G3-B 先核实类型/角色/外部输入，G6 保持源恢复及完整可观察生命周期范围。

[v43 源记录窗口](independent-behavior-graph.md#v43-source-sessions)：每次 F7 重置 20 秒/800 行及 settle 基线；同数量来源/存储替换、源丢失与 PreLoadGame/NewGame 失效终止旧窗，需显式重开。[两个回归与 Release 构建](build.md#v43-source-sessions-2026-10-29)通过；未部署/上机，安装仍为 v41。身份快照不提供 writer 角色、确切 setter 时序或原生数组原子性。v41 历史日志、G3-A 和 G2 结论不变。

[v44 显示时钟](independent-behavior-graph.md#v44-display-clock)：按 eligible 显示墙钟时间累计 60 Hz 固定步，100 ms/最多六步截断停顿；零步帧保持 stepped 候选姿态，request/retarget/rejection/不可推进与 frozen 回落清时钟。[两个 30/120 FPS 场景及 Release 构建](build.md#v44-display-clock-2026-10-29)通过，未部署/上机；旧 v33/v34 的每调用观测与 G2/G3-A 结论不变，最新记录安装仍为 v41。

[v46 退休副本台账](scene-graph.md#retired-copy-accounting-v46)：单个进程存活堆 vector 保留全部 NiPointer 值，不再擦除旧句柄；`retained-total` 为进程累计句柄数，12 仅诊断阈值、13 首次警告，之后继续增长。原生内存未回收、仍无上界，区别于每进程一张独立候选的例外；G6 未通过。旧 SCOPY PARK 原文/计数和 r4 证据不改写。[Release 构建与哈希](build.md#v46-retained-copy-ledger-2026-10-29)仅本地，无部署/上机，最新记录安装仍为 v41。

[audit27：三条静态外部整数 setter](behavior-graph-construction-audit.md#audit27)核实 AE38821 内的 iMagicEquipped / iLeftHandEquipped / iRightHandEquipped 调用与名字初始化。仅补部分 writer 证据；运行时类型/源候选映射、独占性、v41 实际执行、事件时序与输入集充分性未证，iState/currentDefaultState 仍只观察，其余 equip flag 未决。G3-B 未实现/通过；视觉根因与 native 回收未决。此轮仅文档，不构建/测试/部署，最新本地仍 [v46](build.md#v46-retained-copy-ledger-2026-10-29)、记录安装仍 v41。v44 补充说明：F8-only 绘制间隔没有 reset 时按 100 ms 停顿上限恢复，不是完整暂停。
