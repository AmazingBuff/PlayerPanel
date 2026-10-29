# 历史研究总结

更新：2026-10-29。旧逐轮报告按日期保留；当前状态由[验证总表](independent-behavior-graph.md#verification-matrix)与[交接](independent-behavior-graph.md#handoff-20261029)拥有。v33 完整显示窗口、v34 四条已观察边界、v41 G3-A settled 相位轨迹均保留；v42–v46 为本地诊断、源窗口、显示时钟、构建边界和退休台账修正，audit27 仅补部分静态 writer 证据，均无新游戏结果。旧原件可从清理前提交 `719c90bfe115a67ac64b80271dc8afced780290f` 恢复。

- [渲染、展示实例与外观](history-rendering.md)：M0状态/合成、Actor与StudioHome、灯光、native Clone、AFP9取景/蒙皮、UBE，以及不同的回收边界。
- [动画、OAR与暂停行为图](history-animation.md)：HKX取数/采样/映射反转、第二Actor暂停推图、最终输出、OAR/DAC/IED、r1/r2遗留与无Actor独立图关卡。
- [144份来源与恢复索引](history-sources.md)：每份被清理原件的归属、证据限制、固定Git恢复方法与审计位置。

## 当前权威入口

| 要了解的内容 | 文档 |
| --- | --- |
| **当前状态、部署与后续规划（新会话先读这个）** | [当前状态与后续规划](independent-behavior-graph.md#handoff-20261029)：已完成项、DLL/开关、G3-B 契约草案、G3-B→G4→G5→G6 排序、反模式清单 |
| **全部测试与验证状态** | [验证总表](independent-behavior-graph.md#verification-matrix)：各关卡判定、逐轮结论、引擎事实、未验证项 |
| 构建、依赖和验证入口 | [Build](build.md) |
| 复制显示图、取景、装备与生命周期 | [SceneGraph](scene-graph.md) |
| 播放器、OAR查询、DAC原生事件 | [Animation](animation.md)、[OAR](oar-hooks.md)、[DAC](dac.md) |
| 默认产品的未决外观/装备问题 | [诊断交接第10节](diagnostics/display-issues-2026-10-22.md#10-2026-10-23-装备上下文与火把分层修复待上机)；[r1原始日志](diagnostics/CharacterPanel-issue-trace-r1-20261022-2231.log)、[r2原始日志](diagnostics/CharacterPanel-issue-trace-r2-20261022-2314.log) |
| **v46 首轮正常路径上机（PENDING）** | [首轮操作步骤](scene-graph.md#v46-manual-regression)：匹配归档 DLL/PDB、备份/仅改既有 INI 键并重启；两类工作流为源窗口/读档取消，以及正常 OAR 斧头/火把/F7/F8/重开。DisplayPose/DisplayStep 保持 false；完整日志、画面时间与 process retained-total 待回传。尚未执行 |
| 后续独立图实验上机 | [v32 手工清单](independent-behavior-graph.md#v32-manual-checklist)：匹配身份与三键重启、连续 120 调用、候选语义/实际应用/画面分开、同窗口源对照、中断/重开/F7/来源失效及停止恢复规则；**第 1–5 步已于 2026-10-28 22:28 执行一次**（[v33 记录](independent-behavior-graph.md#g2-v33-run)），**第 6 步与第 7 步读档回落已由 v34 观察；同 controller 来源/generation 变更、确切拒绝原因和 frozen-only 路径待新数据**，部署状态见[当前部署与开关](independent-behavior-graph.md#deploy-state) |
| 独立图可行性与实际进度 | [当前六关结论](independent-behavior-graph.md#current-status)：G1 按方案 A 修订通过；**G2 按原范围 PASSED（v33 单窗口）**——语义 120/120、applied 109/116、源不变、画面连续；v26 的 raw 判据与 v27 的静态接姿保留为历史。v31/v32 的离线修正由 v33 重建后上机验证。四条边界由 v34 成立但原因取证有限；G3-A 轨迹由 v41 成立，G3-B/G4–G6 未验。装载销毁 NO-GO，进程保留候选为显式例外；其余生命周期收敛仍待验 |
| 工程约束、产品要求和研究来源 | [维护](code-maintenance.md)、[PRD](player-panel-prd.md)、[社区参考](community-reference-supplement.md) |

原始日志、engine-text 元数据和 TSLOT 分析工具保持原位。最新记录安装为 [v41](build.md#v41-panel-gate-2026-10-29)（SourceTimeline=true、其余三键 off）；[v42](build.md#v42-residual-corrections-2026-10-29) 是新的本地构建，未部署。新 missing cap8 与一次拒绝原因只改进下一次取证，不补写旧日志中的三未知名或拒绝原因。G3-B 先核实类型/角色/外部输入，G6 保持源恢复及完整可观察生命周期范围。

[v43 源记录窗口](independent-behavior-graph.md#v43-source-sessions)：每次 F7 重置 20 秒/800 行及 settle 基线；同数量来源/存储替换、源丢失与 PreLoadGame/NewGame 失效终止旧窗，需显式重开。[两个回归与 Release 构建](build.md#v43-source-sessions-2026-10-29)通过；未部署/上机，安装仍为 v41。身份快照不提供 writer 角色、确切 setter 时序或原生数组原子性。v41 历史日志、G3-A 和 G2 结论不变。

[v44 显示时钟](independent-behavior-graph.md#v44-display-clock)：按 eligible 显示墙钟时间累计 60 Hz 固定步，100 ms/最多六步截断停顿；零步帧保持 stepped 候选姿态，request/retarget/rejection/不可推进与 frozen 回落清时钟。[两个 30/120 FPS 场景及 Release 构建](build.md#v44-display-clock-2026-10-29)通过，未部署/上机；旧 v33/v34 的每调用观测与 G2/G3-A 结论不变，最新记录安装仍为 v41。

[v46 退休副本台账](scene-graph.md#retired-copy-accounting-v46)：单个进程存活堆 vector 保留全部 NiPointer 值，不再擦除旧句柄；`retained-total` 为进程累计句柄数，12 仅诊断阈值、13 首次警告，之后继续增长。原生内存未回收、仍无上界，区别于每进程一张独立候选的例外；G6 未通过。旧 SCOPY PARK 原文/计数和 r4 证据不改写。[Release 构建与哈希](build.md#v46-retained-copy-ledger-2026-10-29)仅本地，无部署/上机，最新记录安装仍为 v41。

[audit27：三条静态外部整数 setter](behavior-graph-construction-audit.md#audit27)核实 AE38821 内的 iMagicEquipped / iLeftHandEquipped / iRightHandEquipped 调用与名字初始化。仅补部分 writer 证据；运行时类型/源候选映射、独占性、v41 实际执行、事件时序与输入集充分性未证，iState/currentDefaultState 仍只观察，其余 equip flag 未决。G3-B 未实现/通过；视觉根因与 native 回收未决。此轮仅文档，不构建/测试/部署，最新本地仍 [v46](build.md#v46-retained-copy-ledger-2026-10-29)、记录安装仍 v41。v44 补充说明：F8-only 绘制间隔没有 reset 时按 100 ms 停顿上限恢复，不是完整暂停。
