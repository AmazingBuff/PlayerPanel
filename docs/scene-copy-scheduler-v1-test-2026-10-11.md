# HKX38 测试单（2026-10-11，调度器 v1：傀儡直驱上产品）——**待上机**

- 构建身份：`CharacterPanel-scopy-HKX38-d14af9a166+dirty-cl94faaed0c6-20261011T153149Z61011T153149Z`
  （基线 = HKX37 判读提交 `d14af9a166`，其上为未提交的 puppet_driver 与接线）。
- 包：`dist/CharacterPanel-scopy-HKX38-1.2.1.zip`，DLL SHA-256 `a24605e3…`
  （全值 `a24605e313e6f5d7241ac37e8a054340db0e5187785bde0c106168419e4139d3`），
  **DLL 与 PDB 均已从最终 zip 部署到 `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\` 并与 manifest 校验一致**。
- 单测 1/1。
- 方向出处：[傀儡探针测试单](scene-copy-puppet-probe-test-2026-10-11.md) 判读"路线成立"——
  显示链路不动，动画数据源换成引擎真实 tick 的傀儡 Actor。

## 本轮内容（产品化，scopy 构建默认启用）

1. **`PuppetDriver`（`src/character/puppet_driver.{h,cpp}`，新模块）**——傀儡生命周期产品化：
   - **出生**：kPostLoadGame 请求，渲染括号内第一个**未暂停**帧执行（v6.70 配方：玩家副本
     PlaceObjectAtMe + 钉桩标志 + fade 钉 0.05，装配窗 240 帧结束后**泊到玩家正下方 8000**，
     v6.25 已证深度不可见不可交互；未暂停时每 60 帧重泊，对冲引擎更新链覆盖一次性 SetPosition）。
   - **运行时 formID 不进存档**——中途杀进程/崩零残留；读档边界/主菜单边界释放。
   - **暂停期手动推图**：面板暂停世界时逐帧 `UpdateAnimationGraphManager(自有时钟 dt≤0.1s)`
     （HKX37 实测 3300+ 帧成立的入口），digest 每 ~0.5s 一条（`SCOPY PUPPET digest …`）。
2. **驱动链升级（`SceneGraphCopy::animate_at_base_pose`）**：F2 开时按优先级
   **傀儡 → 捕获时 clip → 程序化 idle**，每帧取傀儡行为图里**混合权重最强的运行 clip**，
   按其 live control 的 `localTime` 采样写副本；clip 切换只触发一次轻量解析（无走查无扫描）。
   每次切换打一行：`SCOPY PUPPET driver clip='…' mapping=table|identity-unverified coverage=…`。
3. **映射分级（PRD §6.4 修订的执行面）**：绑定表非空 → `table`（权威）；空表（OAR 替换件
   的已知形态）→ `identity-unverified`（明确标注未验证，后续 A/B 测量，不静默信任）。
4. **H05 仪表白名单**：傀儡按"面板所有物"排除出 NPC 普查（面板会话内 `npc-set` 不再 +1）。
5. **兜底保留**：傀儡缺位/解析失败 → 旧链（clip → 程序化）自动接管，日志说明；
   `animation_driver()` 相应报 `puppet:<clip>#<idx>` / `clip:…` / `procedural-idle` / `off`。

**本轮不做**（归 HKX39）：equip 第二杠杆（暂停期 SendAnimationEvent）、identity 档的时机性
A/B 与视觉兜底标定、傀儡 poseLocal 覆盖差（39 vs 109）的按骨名桥接深查、capture 期 A/B 走查限频。

## 上机步骤

1. 确认部署：`E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\CharacterPanel.dll` SHA-256 `a24605e3…`。
2. 进重度存档，**进游戏后等 ~6 秒**：日志应出现 `SCOPY PUPPET born form=…` 与 4 秒后的
   `SCOPY PUPPET parked reason=grace-complete offset=-8000`。**装配窗内玩家身边应看不到人形
   （fade 钉死），之后更看不到**。
3. 正常游玩 1 分钟（重泊每秒一次，应无任何可感知；确认傀儡不在身边、E 键打不中任何新增物）。
4. 开背包 → F7 → F8：面板人物应开始**随 OAR 的实时决策动起来**。看日志：
   - `SCOPY PUPPET driver clip='…' mapping=… coverage=…`（每次图内切换一条）；
   - `SCOPY PUPPET digest ticks=… clips=…`（每 ~0.5s 一条，t= 随面板时间推进）。
5. 交互面回归：F2 关（人物冻结）→ F2 开（从图当前状态继续，**不重头播**——图是连续的）；
   F3 旋转、F4 释放、关背包再开（跨面板会话动作连续）；读档一次（日志应见
   `SCOPY PUPPET released reason=load-boundary` → 新存档重新 `born`）。
6. 结束后复制日志进 `docs/diagnostics/`（惯例名 `CharacterPanel-hkx38-scheduler-20261011.log`）。

## 判读表

| # | 检查 | 通过判据 | 失败时 |
| --- | --- | --- | --- |
| 1 | 出生与泊位 | `born` → 4s 内 `parked reason=grace-complete`；全程玩家身边无人形 | `spawn UNAVAILABLE reason=…` 按因回传；可见人形 = fade 守卫失效，回传 |
| 2 | **驱动成立** | F8 后出现 `driver clip='…' mapping=…`，且面板人物**可见地动** | 无 driver 行 = 傀儡解析失败（看 `resolve UNAVAILABLE reason`）；有行但人物不动 = 写入链问题，回传 |
| 3 | digest 推进 | paused 期 digest 的 `t=` 随面板时间前进 | 冻结 = 手动推图回归（HKX37 曾全绿），回传整段 |
| 4 | OAR 实时性 | clip 索引切换出现在**暂停期**（对比 HKX35 池子行索引） | 切换只在未暂停 = 图泵只在引擎更新——回传 |
| 5 | F2 语义 | F2 关即冻结、开即续播 | —— |
| 6 | 读档边界 | `released reason=load-boundary` → `born`（新存档新 formID） | 残留/崩溃 = 回传 |
| 7 | 零回归 | 零崩溃、零 `[E]`；H05 普查 `npc-set=+0/-0`（傀儡已白名单）；渲染照常 | `npc-set` 出现 +1 = 白名单没生效，回传 |
| 8 | 兜底链 | 若傀儡失败：`animation-driver=` 显示 clip/procedural 且画面照常 | 链断裂 = 回传 |

**已知预期**（不作失败）：`mapping=identity-unverified` 的 OAR 替换件上，面板人物姿态可能
**不对位**（恒等映射未过 A/B 的已知开放项，HKX39 标定）；`mapping=table` 的条目应贴得近。
**探针构建说明**：F6 五阶段机仍在（诊断轮可用），HKX38 验收轮**不要按 F6**（会再出生一个
探针傀儡，污染判读）。
