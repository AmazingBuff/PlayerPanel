# 历史研究总结

更新：2026-10-26。过期逐轮报告与旧日志归并为下面两篇研究总结，保留后续验证的结论、被推翻的假设、失败路径和仍开放的验收约束。旧原件从清理前提交`719c90bfe115a67ac64b80271dc8afced780290f`恢复，不能把旧“通过/收官”当作当前实现的通用保证。

- [渲染、展示实例与外观](history-rendering.md)：M0状态/合成、Actor与StudioHome、灯光、native Clone、AFP9取景/蒙皮、UBE，以及不同的回收边界。
- [动画、OAR与暂停行为图](history-animation.md)：HKX取数/采样/映射反转、第二Actor暂停推图、最终输出、OAR/DAC/IED、r1/r2遗留与无Actor独立图关卡。
- [144份来源与恢复索引](history-sources.md)：每份被清理原件的归属、证据限制、固定Git恢复方法与审计位置。

## 当前权威入口

| 要了解的内容 | 文档 |
| --- | --- |
| 构建、依赖和验证入口 | [Build](build.md) |
| 复制显示图、取景、装备与生命周期 | [SceneGraph](scene-graph.md) |
| 播放器、OAR查询、DAC原生事件 | [Animation](animation.md)、[OAR](oar-hooks.md)、[DAC](dac.md) |
| 在用问题与下一场上机 | [诊断交接第10节](diagnostics/display-issues-2026-10-22.md#10-2026-10-23-装备上下文与火把分层修复待上机)；[r1原始日志](diagnostics/CharacterPanel-issue-trace-r1-20261022-2231.log)、[r2原始日志](diagnostics/CharacterPanel-issue-trace-r2-20261022-2314.log) |
| 独立图可行性与实际进度 | [当前六关结论](independent-behavior-graph.md#current-status)：G1仅按方案A修订通过；G2 仍未通过但只剩“持续”（v20a 运动证据、v21 判据误报、v22 pose 内容、v23 定位多片段栈、v24 层可归属：`H2H_Idle` 覆盖 90/99 骨；idle 时钟每步 0.75×–1.55× 待解释），G3–G6完整验收未执行。加载图销毁/资源收敛未证；[审计后验边界](behavior-graph-construction-audit.md#audit-current)区分指令事实、运行观察与假设 |
| 工程约束、产品要求和研究来源 | [维护](code-maintenance.md)、[PRD](player-panel-prd.md)、[社区参考](community-reference-supplement.md) |

两份当前raw日志、engine-text元数据和三个TSLOT分析工具保留原位。它们分别是未决问题原始证据、历史引擎dump信息与分析入口。当前行为与操作以在用文档为准，本轮未执行游戏实验。
