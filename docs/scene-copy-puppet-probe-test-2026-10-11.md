# HKX37 探针测试单（2026-10-11，傀儡 Actor：面板动画数据源换真 Actor 的可行性探针）——**待上机**

- 构建身份：`CharacterPanel-scopy-HKX37-3b581ad8e1+dirty-cl94faaed0c6-20261011T144702Z`
  （基线 = H05 判读提交 `3b581ad8e1`，其上为未提交的 puppet_probe 新模块）。
- 包：`dist/CharacterPanel-scopy-HKX37-1.2.1.zip`，DLL SHA-256 `42351be7…`
  （全值 `42351be7fb66c1fac35801589738424e8490da3f6d991afda3ac70cb38d82cc5`），
  **DLL 与 PDB 均已从最终 zip 部署到 `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\` 并与 manifest 校验一致**。
- 单测 1/1。
- 方向出处：用户拍板的"傀儡 Actor"调度路线——显示仍是场景图副本直绘，动画数据从引擎真实
  tick 的傀儡（玩家副本 Actor）提取，OAR 对傀儡做实时决策。本探针只回答**可行性五问**，
  不动显示链路一寸代码。

## 前提须知

- 这是**诊断构建**（`CHARACTER_PANEL_S2_PROBE=ON`）：F6 = 傀儡探针（旧 S2 摆动探针已退役、
  不再绑定）；F6 与另一个插件的占用冲突照旧存在于诊断轮。F7/F8/F3/F4/F2 行为不变。
- **本轮全程不需要 F7**——五问全部围绕傀儡本体与数值 A/B，不涉及面板捕获与显示。
- 探针构建里 isolation_watch 照常运行：傀儡出生会让 H05 普查多出 `npc-set=+1` 类 jitter 行
  ——探针轮预期内，不作判读对象。
- 日志前缀 `SCOPY PUPPET`（阶段机与 digest）+ `SCOPY ANIM`（A/B 复用既有链，source form id =
  傀儡的 formID）。

## F6 阶段机（每按一次推进一步）

| 按次 | 阶段 | 动作 | 回答 |
| --- | --- | --- | --- |
| 1 | alive | 玩家副本出生（v6.70 配方：PlaceObjectAtMe 于玩家处 + 钉桩标志 + fade 钉 0.05），**不泊位**（泊位是产品轮问题，本轮不引入） | 问 1：出生 + 装配。游戏未暂停时 digest 每 ~2s 一条，看引擎自己是否驱动 clips（localTime 前进） |
| 2 | manual | 开启手动推图：暂停背包内每帧 `UpdateAnimationGraphManager(自有时钟 dt)` | 问 2：暂停期图是否被我们推进（digest 的 t= 随我们的时钟走）；问 3：OAR 是否替换（clip 名出现 OAR 式长路径） |
| 3 | equipped | 把玩家穿着（含双手持物）镜像上傀儡（ActorEquipManager::EquipObject，既验证配方） | 问 4：暂停期装备事件是否让 equip 类 clip 在面板会话内激活 |
| 4 | sampled | 数值 A/B：`report_animation_instance` + `prepare_animation_clip` 以傀儡为 source（HKX28 链原样参数化） | 问 5：`SCOPY ANIM play ground-truth verdict=match`？——采样链在傀儡上成立 |
| 5 | off | SetDelete 清除傀儡 + 总结行（ticks/digests/oar-shaped-names/ab-verdict） | 清理与证据汇总 |

## 上机步骤

1. 确认部署：DLL SHA-256 `42351be7…`。
2. 进重度存档，**游戏未暂停**时按 F6（第 1 次）→ 等 5–10 秒：看 2–4 条 `SCOPY PUPPET digest n=… engine-phase paused=0 clips=…`。
   **确认玩家身边无可见傀儡**（fade 钉死）、无碰撞/交互异常（已钉桩+封锁激活）。
3. **开背包（暂停）** → 按 F6（第 2 次）→ 等 5–10 秒：看 2–4 条 `digest n=… manual-ticks=… paused=1 dt=…`。
4. 按 F6（第 3 次）→ 等 5–10 秒：日志出现 `stage=equipped items=N`，digest 里留意 equip 类条目。
5. 按 F6（第 4 次）→ 等 2 秒：`SCOPY PUPPET ab begin/end` + `SCOPY ANIM play …` 块。
6. 按 F6（第 5 次）→ `SCOPY PUPPET stage=off reason=f6-final-press …` 总结行。
7. 关背包确认：傀儡已删（玩家身边无残留）；再游玩 1 分钟零异常。
8. 日志复制进 `docs/diagnostics/`（惯例名 `CharacterPanel-hkx37-puppet-20261011.log`）。

## 判读表

| # | 问题 | 通过判据 | 失败判读 |
| --- | --- | --- | --- |
| 1 | 出生+装配 | `stage=alive form=…`；engine-phase digest 出现 clips≥1 且 t= 前进 | `UNAVAILABLE reason=…` 按因判读；clips 恒空 = 引擎不给出生即站立的角色配图——回传 |
| 2 | **暂停期手动推图** | paused=1 的 digest 里 **t= 随 dt 累计前进**（跨 digest 比对） | t= 冻结 = `UpdateAnimationGraphManager` 在暂停期不推进——**傀儡路线的生死判据**，回退池子路线等 fork |
| 3 | OAR 实时替换 | digest 的 clip 名出现 OAR 式长路径（会记入总结行 `oar-shaped-names`）；或索引与玩家池子行吻合 | 全部原版名 = OAR 未对傀儡求值（条件不合或钩子未触）；不影响问 2/4/5 的独立判读 |
| 4 | 暂停期装备反应 | equipped 后的 digest 出现 equip 类条目（t= 前进若干秒后回落/消失） | 无 equip 条目 = 暂停期装备事件不激活 clip——菜单内实时反应不成立，equip 回退"事件+池子回放" |
| 5 | **采样链在傀儡上成立** | `SCOPY ANIM play ground-truth verdict=match`（或 best 已到 1% 量级） | verdict=none 且 max 很大 = 绑定/映射在傀儡上未成立——回传 `SCOPY ANIM` 整块判读 |
| 6 | 清理 | 总结行出现；关背包后玩家身边零残留；无崩溃零 `[E]`（隔离仪表的傀儡行除外） | SetDelete 后残留 = 回传 |

**路线判定**：问 2+5 全绿 = 傀儡路线成立，进入 HKX38 工程轮（傀儡驱动为首选、池子回放降级
兜底，探测式共存）；问 2 红而问 5 绿 = 采样可但暂停期不可驱，傀儡只在非暂停时间线有价值
——回到池子路线，fork 重新成为必要项；问 1 红则全盘回退。
