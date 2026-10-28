# 历史研究总结

更新：2026-10-28。旧逐轮报告按日期保留；当前状态由[验证总表](independent-behavior-graph.md#verification-matrix)与 [v32](independent-behavior-graph.md#g2-v32-display-session)拥有。v26 的 raw 判据通过是历史记录，当前语义连续性待复验；旧原件仍可从清理前提交 `719c90bfe115a67ac64b80271dc8afced780290f` 恢复。

- [渲染、展示实例与外观](history-rendering.md)：M0状态/合成、Actor与StudioHome、灯光、native Clone、AFP9取景/蒙皮、UBE，以及不同的回收边界。
- [动画、OAR与暂停行为图](history-animation.md)：HKX取数/采样/映射反转、第二Actor暂停推图、最终输出、OAR/DAC/IED、r1/r2遗留与无Actor独立图关卡。
- [144份来源与恢复索引](history-sources.md)：每份被清理原件的归属、证据限制、固定Git恢复方法与审计位置。

## 当前权威入口

| 要了解的内容 | 文档 |
| --- | --- |
| **全部测试与验证状态（先读这个）** | [验证总表](independent-behavior-graph.md#verification-matrix)：v17–v30 归档运行、v31/v32 离线修正、raw 与语义连续性边界、实际应用覆盖及复现入口 |
| 构建、依赖和验证入口 | [Build](build.md) |
| 复制显示图、取景、装备与生命周期 | [SceneGraph](scene-graph.md) |
| 播放器、OAR查询、DAC原生事件 | [Animation](animation.md)、[OAR](oar-hooks.md)、[DAC](dac.md) |
| 默认产品的未决外观/装备问题 | [诊断交接第10节](diagnostics/display-issues-2026-10-22.md#10-2026-10-23-装备上下文与火把分层修复待上机)；[r1原始日志](diagnostics/CharacterPanel-issue-trace-r1-20261022-2231.log)、[r2原始日志](diagnostics/CharacterPanel-issue-trace-r2-20261022-2314.log) |
| 独立图下一场上机 | [v32 手工清单](independent-behavior-graph.md#v32-manual-checklist)：匹配身份与三键重启、连续 120 调用、候选语义/实际应用/画面分开、同窗口源对照、中断/重开/F7/来源失效及停止恢复规则；**v33 已部署（三键开）**，尚未执行，部署状态见[当前部署与开关](independent-behavior-graph.md#deploy-state) |
| 独立图可行性与实际进度 | [当前六关结论](independent-behavior-graph.md#current-status)：G1 按方案 A 修订通过；G2 当前待补逐步候选语义连续性，v26 的历史 raw 判据通过保留。v27 只有 116 条输出和站姿确认；实际覆盖另验。v31 共用单步/修正读取，v32 绑定来源与窗口、分离语义运动和实际 assignments，只有离线成功。装载销毁、全局收敛及 G3–G6 未验收 |
| 工程约束、产品要求和研究来源 | [维护](code-maintenance.md)、[PRD](player-panel-prd.md)、[社区参考](community-reference-supplement.md) |

两份当前raw日志、engine-text元数据和三个TSLOT分析工具保留原位。它们分别是未决问题原始证据、历史引擎dump信息与分析入口。当前行为与操作以在用文档为准，本次文档完善没有重跑测试/构建、部署或执行游戏实验；现有 v32 DLL/PDB 保留 [提交前 f5ec13f+dirty 身份](build.md#v32-display-session-2026-10-28)，后续提交不会改写它们。
