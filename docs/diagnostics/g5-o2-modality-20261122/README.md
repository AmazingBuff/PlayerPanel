# O-2 模态决断：源侧捕获的会话结构与面板开启冻结语义（2026-11-22，纯离线）

> **状态**：**已决断（无需上机探测）**。C-G5-VER [§五 O-2](../g5-verify-protocol-20261121/README.md) 预设的"两次会话暂停/前台语义未统一"是**会话结构误读**：gate-5 正式批次的源侧来自独立源侧会话（面板关闭），与 run2 面板开启显示窗的冻结读数分属两种模态、各自行为正确。既有工件已完整回答 O-2 的问题；本稿只做证据核对、误读更正与协议文本澄清。零采集、零构建、零上机。

## 一、结论（TL;DR）

1. **gate-5 正式批次是两次独立会话**：[正式 README §一](../g5-gate5-20261116/README.md) 明示"候选侧与源侧各一次会话/窗口"；工件侧印证——候选 bin flush `20261116T155048Z`、源 bin `20261116T155900Z`（相隔 52 s）、各自独立 `CharacterPanel.log`。
2. **源侧 849 条记录来自面板关闭的独立源侧会话**：源会话日志第 9 行 `EXPERIMENT setting IndependentGraph=off`、第 70 行 `PROBE LIVENESS window=open mode=on epoch=1 pinned=yes`（SamplingLiveness=on 在 `IndependentGraph=off` 下经 F7 开 20 秒源侧窗口）、**零条 `ANIMATION DISPLAY` 行**（面板从未打开）。候选会话日志 `SamplingLiveness=off`——无双侧同会话捕获。
3. **run2 的"面板打开期间源图时钟冻结"是真实行为**（非同帧读数伪影）：view 1 的 call 60（13:14:16.392）→ call 120（13:14:17.368）相隔 **0.976 s**，源三 clip（1HM_Idle `410D58C4`≈8.84 s、MLh_Idle `3F9DF944`≈1.23 s、SCAR dummy `401F53FA`≈2.49 s）位型**逐位不变**——三个 clip 均不在时长终点，无 end-hold 借口。同窗口内候选照常推进（call 60→120 走完 60 步）⇒ 游戏未暂停 ⇒ **显示会话接管角色期间源图不被 tick**（机制为推断，行为为实测）。
4. **面板关闭后源图正常更新**：跨 view（32.6 s，期间面板关）源时钟按墙钟推进回绕——1HM_Idle `410D58C4`→`4069110E`（8.84→3.65，模 9.0 s）、MLh_Idle `3F9DF944`→`3FB00035`（1.23→1.38，模 2.53 s），与 ~30.5–30.8 s 的 tick 时长一致（32.6 s 减去面板开启时段）。
5. **正式验证的源侧模态由此确定**：沿用 gate-5 结构——源侧捕获在**独立源侧会话**进行（面板关 + `IndependentGraph=off` + `SamplingLiveness=on` + `K5FullSample=true` + `K5Reconciliation=true`；实机装备序列在 20 s 窗口内执行）。该模态下"图在更新"已被 gate-5 记录直接证明（四个混合窗实时推进、idle 域唯一相位点 0 相邻等值）。
6. **O-2 无需上机探测**：其行内预设的"面板开/关各 10 s 探测"所问的问题（源图在哪种模态下更新）已由既有工件完全回答——面板开显示窗冻结（run2 实测）、面板关源侧会话更新（gate-5 实测）。

## 二、证据对照表

| 观察 | 会话/模态 | 证据 | 源图时钟 |
| --- | --- | --- | --- |
| gate-5 源侧 849 条 | **独立源侧会话**（面板关；`IndependentGraph=off`、`SamplingLiveness=on`） | [源会话日志](../g5-gate5-20261116/raw/source/CharacterPanel.log) 第 9/70 行；0 条 DISPLAY 行；独立 bin | **推进**（四混合窗实时、idle 域 0 相邻等值） |
| gate-5 候选侧 249 条 | inject 会话（面板开；`SamplingLiveness=off`） | [候选会话日志](../g5-gate5-20261116/raw/candidate/CharacterPanel.log) 第 22 行 | ——（无源捕获） |
| v121 run2 view 1 | 面板开显示窗 | [run2 日志](../g5chain-idle-run2-20261121-drawn.log) 306–313/549–551 行：call 60→120 相隔 0.976 s、三 clip 位型不变 | **冻结** |
| v121 run2 view 1→2 | 面板关时段（32.6 s） | 826–828 行：1HM_Idle 8.84→3.65、MLh 1.23→1.38 | **推进（回绕）** |

## 三、协议文本修订

1. [§四 stage 3](../g5-verify-protocol-20261121/README.md) 澄清："K5 双侧捕获" = **两次独立会话**（候选 inject 会话 + 源侧独立会话），与 gate-5 同构；源侧会话键面为 `IndependentGraph=off` + `SamplingLiveness=on` + `K5FullSample=true`（+ `K5Reconciliation=true`）。
2. §五 O-2 行标记已决断，指向本稿。
3. §六 停点移除 O-2 前提（三项开放项全部解决后，停点只剩会话内判据）。
4. 新增边界注记：**面板开启显示窗内源图时钟冻结**——任何未来在面板开模态下的源侧观测/捕获必须绕开此语义；单会话双捕获在当前显示机制下不可行。

## 四、边界

- 本稿不改写 gate-5 正式批次、run1/run2、[源侧过渡语义](../g5-source-blend-20261121/README.md)的任何裁定；源会话 INI 的逐键值（SamplingLiveness 等）按日志行为读出（源记录存在 ⇒ `sampling_liveness != off` 且 `K5FullSample=true`，代码路径 [probe_liveness.cpp `record_k3s_return`](../../../src/animation/behavior_graph/probe_liveness.cpp) 门控）。
- "显示会话接管期间源图不被 tick"是行为层实测 + 机制推断；引擎级机制（retarget 后源图 update 门控的具体路径）未取证，如未来需要面板开模态的源侧数据，须先另行取证。
- run2 的跨 view 回绕算术（~30.5–30.8 s tick）与 32.6 s 墙钟的差值（~1.8–2.1 s 面板开启时段）只作一致性旁证，不作正式推导。
- 纯离线；未启动游戏、未部署任何文件。
