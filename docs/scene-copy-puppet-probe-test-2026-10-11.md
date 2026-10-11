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

---

## HKX37 判读（已实测，2026-10-11 深夜；日志 [CharacterPanel-hkx37-puppet-20261011.log](diagnostics/CharacterPanel-hkx37-puppet-20261011.log)）

实测规模：22:55–23:01 单会话，五按执行了四按（第 5 按清除未按，见 #6）；约 3300+ 次手动推图、
151 条 digest；零崩溃、零 `[E]`、零隔离异常。

| # | 问题 | 结果 |
| --- | --- | --- |
| 1 | 出生+装配 | **通过**——`stage=alive form=ff000c77 base=00000007`；engine-phase digest 显示引擎自己驱动 idle（`MT_Idle.HKX#12221`，t= 2.46→17.03s 随引擎时钟前进）；fade 钉死零可见零交互 |
| 2 | **暂停期手动推图** | **通过（生死判据绿）**——paused=1 的 digest 里 `t=` 严格随我们的 dt 累计前进（n=20 t=0.18 → n=50 t=19.14 → n=151 延续到新家族），3300+ 帧、dt 0.011–0.024s，跨索引切换推图不中断。**`UpdateAnimationGraphManager` 在暂停期对非玩家 Actor 成立——傀儡路线成立的第一根支柱** |
| 3 | OAR/图实时决策 | **通过（索引级）**——运行 clip 的注册索引跨 digest 实时切换：12221→12224（22:56:38）→12220（22:56:59，暂停期手动推图中），与玩家 HKX35 池子已知索引（12220/12224）吻合 = 同一选择机制在傀儡上实时出决策。注：OAR 替换是**索引级**（名字保持基础名），本轮的名字形状探测如预期未命中，判据落在索引上 |
| 4 | 暂停期装备反应 | **字面义未达成，但有重要的部分成立**——EquipObject 后 25s 暂停期 digest 无一次性 equip clip；但装备状态确实进了图：解除暂停后 2 秒内图切到 `MT_IdleHHDRightEnter#1041`（进场 clip）→ `MT_IdleHHDRightIdle#1042`（持物手部 idle 族）。判读：**EquipObject 的图反应挂在引擎 actor 更新上（我们手动推图不泵动画事件）**；产品轮第二杠杆 = 暂停期直接 `SendAnimationEvent`（与手动推图同一原理）。装备 idle 族的实时切换本身已成立 |
| 5 | **采样链在傀儡上成立** | **机制全通 + 映射为已知开放项**——`instance verdict=binding-resolved`（活动 clip #12220 的活 binding + 活 control，localTime=4.109/weight=1.0，HKX13/HKX28 入场券在傀儡上成立）；`anim probe=valid type=spline duration=20.00s transform-tracks=99`；`SamplePartialTracks` 采样 + 真值扫描全程跑通零崩溃。`verdict=none best-t=15.83s max-rot=94.6deg max-pos=1.32u truth-bones=39 copy-resolved=93/99`——与 HKX35 的已知开放项同形（OAR 替换件轨道表为空 → 恒等映射未过 A/B），**不是傀儡特有失败**；映射分级验证本来就是多动作工作包的正戏。观测项：真值骨仅 39（玩家侧 109）——傀儡 poseLocal/boneNodes 的覆盖差需在 HKX38 按骨名桥接复核 |
| 6 | 清理 | **未执行**（用户未按第 5 下 F6，日志止于 manual digest）——傀儡为 ff 运行时 formID，**不进存档**，退出游戏即消散，零残留风险；产品轮仍需自带 kill 路径 |

### 路线判定：**傀儡路线成立**（问 2 绿 + 问 5 机制绿；映射质量是继承的开放项，不阻塞路线）

- **两根支柱都立住了**：暂停期我们自己的时钟能推进一个真 Actor 的行为图（3300+ 帧实测）；
  HKX28 采样链对傀儡按原样参数化即通（活绑定/活 control/采样/对比零崩溃）。
- **OAR 的决策免费到手**：索引在玩家池子已知值之间实时切换——池子回放（历史分布）升级为
  实时决策，而且不需要 fork。
- **手部 idle 分类器缺口被路线本身吸收**：装备后傀儡自动切到持物手部 idle 族
  （`IdleHHDRightEnter→Idle`），比"把族加进分类器"更根本——分类器只剩池子兜底路线还用。
- **HKX38 工程轮范围**（傀儡首选、池子兜底、探测式共存）：
  ①傀儡生命周期产品化（kill 路径 + 读档边界 + 会话复用，ff-formID 不留档已是天然优势）；
  ②调度器 = 采样傀儡当前运行集直驱显示副本（idle 轮换由图自己决定，不再自建加权轮换）；
  ③equip 事件补第二杠杆（暂停期 SendAnimationEvent 直接触发，探针轮已定位原因）；
  ④傀儡 poseLocal 覆盖复核（truth-bones=39 vs 玩家 109，骨名桥接查因）；
  ⑤映射分级验证（绑定集完整表 → 权威；空表 → 恒等 + 结构校验 + 时机性 A/B + 视觉兜底）照
  PRD §6.4 修订执行；⑥H05 仪表加傀儡白名单（面板所有物，不计入 npc-set）。
- 遗留观测：capture 期 A/B 走查慢（1024 对象 ~80s 渲染括号墙钟）——产品轮限频或瘦身。
