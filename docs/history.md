# 历史研究总结

更新：2026-11-11。[最新运行摘要](independent-behavior-graph.md#status-20261111-session)记录 E3 配对 2/2、E4 四块源快照与装备链速率；[CAL v1](independent-behavior-graph.md#c-g5-cal-v1)的[离线 A 已完成](independent-behavior-graph.md#c-g5-cal-a-run1-20261111)，当前分析器三次往返共 12/12 行、120/120 分量，[工件完整保留](diagnostics/g5-calibration-a-20261111/attempt-002/result.md)。[K1/K2 资格账本](independent-behavior-graph.md#c-g5-cal-k1k2)的 D1 判负（实际 1H，候选仍 H2H），D2 判负（三项装备变量写入并保持，片段仍 H2H）；D3 v2 源侧初始值命中 `iRightHandType=1`、`iLeftHandType=9`，D3 候选 prime 已实现并构建为 v95、待部署/上机。K1–K6、原生 B、时间校准仍 NOT_READY，数值容差未产生。G1/G2 有界通过、G3-B 相位回放和 G4 A/B/C 证据保留；G5 未冻结/未通过，G6 后置，装载 teardown NO-GO。历史条目的日期/限定保留。

- [渲染、展示实例与外观](history-rendering.md)：M0状态/合成、Actor与StudioHome、灯光、native Clone、AFP9取景/蒙皮、UBE，以及不同的回收边界。
- [动画、OAR与暂停行为图](history-animation.md)：HKX取数/采样/映射反转、第二Actor暂停推图、最终输出、OAR/DAC/IED、r1/r2遗留与无Actor独立图关卡。
- [144份来源与恢复索引](history-sources.md)：每份被清理原件的归属、证据限制、固定Git恢复方法与审计位置。

## 当前权威入口

| 要了解的内容 | 文档 |
| --- | --- |
| **当前状态、部署与后续规划（新会话先读这个）** | [最新运行摘要](independent-behavior-graph.md#status-20261111-session)、[当前部署记录](independent-behavior-graph.md#deploy-state)、[CAL v1](independent-behavior-graph.md#c-g5-cal-v1)与 [K1/K2 资格账本](independent-behavior-graph.md#c-g5-cal-k1k2)：安装 v94 + D3 源侧只读探针（v93 + D3 三件套备份）；[离线 A 已完成](independent-behavior-graph.md#c-g5-cal-a-run1-20261111)，D1 判负，D2 判负（变量写入并保持，片段仍 H2H），D3 v2 命中 `iRightHandType=1`/`iLeftHandType=9`，D3 候选 prime 已构建为 v95、待部署/上机，K1–K6/原生 B/时间资格待补，数值容差未产生。历史交接与原日志保留 |
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
