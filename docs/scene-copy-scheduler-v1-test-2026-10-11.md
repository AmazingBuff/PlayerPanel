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

---

## HKX38 判读（已实测，2026-10-11 深夜；日志 [CharacterPanel-hkx38-scheduler-20261011.log](diagnostics/CharacterPanel-hkx38-scheduler-20261011.log)）

实测规模：23:38–23:46 单会话（无读档穿越）；傀儡 23:39:17 出生（ff006918）、23:39:28 泊位、
此后每 ~1.1s 重泊共 ~7 分钟；暂停面板时间 ~5.5 分钟（4890 次手动推图、163 条 digest）；
8 次 clip 切换解析；零崩溃、零 `[E]`（仅 2 行已知 SuperSprayBreath 隔离观测）。

| # | 检查 | 结果 |
| --- | --- | --- |
| 1 | 出生与泊位 | **通过**——kPostLoadGame 后首个未暂停帧出生，240 帧装配窗后泊 -8000，重泊节奏正常；玩家身边全程无人形 |
| 2 | **驱动成立** | **通过**——`SCOPY DRAW … animation-driver=puppet:Animations\IdlePickaxe_Floor.hkx#11096`；8 次切换解析全部成功，coverage=**bone-nodes=116 pose=116**（IdlePickaxe 116 轨解析 109 / mt_idle 族 99 轨解析 93）——**HKX37 的真值骨 39 异常在产品读取上不复现，姿态桥健康** |
| 3 | digest 推进 | **通过**——t= 随面板时间前进并按 clip 周期回绕；step 0.012–0.026 常态、一次 0.1 钳制（顿帧保护生效） |
| 4 | **OAR 实时性** | **通过（决定性）**——暂停面板内图把傀儡从 `MT_Idle#12222` 切到 `IdlePickaxe_Floor#11096`（OAR 风味 idle 决策），驱动链同帧跟进；未暂停期引擎又切 JumpFall/JumpLand（见下）。**运行中切换 + 切换只重解析一次 + 零卡顿** |
| 5 | F2 语义 | **通过**——F2 关 `driver=off` 即冻结；F2 开**从图当前状态继续**（同一 clip 不重头）——图连续性红利兑现 |
| 6 | 读档边界 | **未测**（本轮未穿越读档；释放/重生路径与出生同机制 + HKX37 已验引用丢失软复位，残险可接受） |
| 7 | 零回归 | **通过**——H05 普查 `npc-set=+0/-0` 全部 8 行（白名单生效）；面板会话 2 的 end verdict=CLEAN；已知粒子观测照旧 |
| 8 | 兜底链 | 未触发（傀儡全程健康；路径与 HKX37 软复位同源） |

### 新确证的引擎事实与观测（记档，HKX39 处理）

1. **泊深 -8000 的傀儡在引擎时间里会下坠**：未暂停期图切 JumpFall/JumpLand（该深度无地面），
   循环落地起跳。对面板无害（暂停时间归我们，恢复时从图当前状态续播），HKX39 泊位时加
   `SetMotionType(Settled)` 一并钉死。
2. **OAR 在本机的 idle 全量替换**：傀儡运行 clip 全部走 `identity-unverified` 档（12xxx 索引 +
   IdlePickaxe 风味件都是替换件、表为空）——identity 档的视觉保真就是主路径的常态，HKX39 的
   时机性 A/B + 视觉标定从"兜底"升级为**主判据**。
3. F2 开关行的 `driver=` 字段还打旧 m_clip 状态（化妆性 bug，HKX39 顺手改）。

### 结论

**HKX38 通过：调度器 v1 成立**——面板显示副本由傀儡实时驱动（真 OAR 决策、图连续、零回归），
池子回放/程序化 idle 降为兜底。**HKX39 范围**：identity 档时机性 A/B 与视觉标定（主判据）、
equip 第二杠杆（暂停期 SendAnimationEvent）、泊深运动钉、F2 日志化妆。遗留队列：HKX39 →
S1 安全回收 → OAR fork（大概率免立项）。
