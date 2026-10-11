# HKX39 测试单（2026-10-11，调度器 v2：identity 档数值 A/B + equip 杠杆 + 泊位运动钉）——**待上机**

- 构建身份：`CharacterPanel-scopy-HKX39-96e2a5fc3b+dirty-cl94faaed0c6-20261011T160133Z`
  （基线 = HKX38 判读提交 `96e2a5fc3b`，其上为未提交的 HKX39 四项改动）。
- 包：`dist/CharacterPanel-scopy-HKX39-1.2.1.zip`，DLL SHA-256 `d8694577…`
  （全值 `d8694577bcd97297bea474a7d47f97cc2c8e95eb8bc5e286e63c91a7412f4eae`），
  **DLL 与 PDB 均已从最终 zip 部署到 `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\` 并与 manifest 校验一致**。
- 单测 1/1。
- 出处：[调度器 v1 测试单](scene-copy-scheduler-v1-test-2026-10-11.md) 判读的三项后续 + F2 日志化妆。

## 本轮内容

1. **identity 档时机性 A/B（主判据，`measure_clip_against_puppet`）**：每次 clip 切换后延迟 60 帧
   （约 1s，避开混合期），对当前 clip 的**当前相位**做一次单相 A/B——采样 clip 轨道 vs 傀儡
   `poseLocal`（按骨名桥接，HKX28 协议单相版），输出
   `SCOPY PUPPET ab clip='…' mapping=… bones=K/N unnamed=… max-rot=…deg max-pos=…u weight=… t=…s verdict=match|none`。
   判定 = 比较骨 ≥24 且最大旋转差 ≤1°（HKX28 关卡）→ `match`。**这是恒等映射的常驻体检**：
   每个 OAR 替换件都被测量，`match` = 显示忠实，`none` = 该替换件轨道序与骨架序不合（记档，
   视觉标定的对象清单自动生成）。
2. **equip 第二杠杆（`poll_equipment`）**：暂停期逐 tick 轮询玩家双手装备；变化即镜像到傀儡
   （HKX37 已验证配方）并**同 tick 推 `DrawWeaponMagicHands(draw|sheathe)`**——暂停期装备
   反应不再等那个永远不来的引擎更新。每次反应打
   `SCOPY PUPPET equip right=… left=… pushed=draw|sheathe|none reactions=N`。
3. **泊位运动钉**：泊位时 `SetMotionType(kFixed)`——HKX38 观测到的"泊深无地面 → 引擎期
   JumpFall 循环"就此消除，OAR 持续评估一个站立 Actor。park 行加 `motion=fixed` 标注。
4. **F2 日志化妆**：开关行 `driver=` 改打真实驱动链（`animation_driver()`），不再显示旧
   m_clip 状态。

## 上机步骤

1. 确认部署：DLL SHA-256 `d8694577…`。
2. 进重度存档等 ~6s：`born` → `parked … motion=fixed`；**游玩 1–2 分钟**后看 digest——
   引擎期不应再出现 JumpFall/JumpLand（HKX38 的下坠循环应消失）。
3. 开背包 F7/F8：看每个切换后 ~1s 的 **ab 行**——`verdict=match|none`、max-rot/max-pos 数值。
4. **equip 杠杆实测**：面板开着时**换一件右手武器**（收起→换装→再装备）——日志应出现
   `SCOPY PUPPET equip right=… pushed=draw`，且 digest 里的 clip 应在数秒内切到装备/持握
   相关族（IdlePickaxe 风味件或武器 idle）；F8 下看面板人物是否**当场做了拔武器动作**。
   再收起武器（pushed=sheathe）。
5. 读档一次（补 HKX38 未测的边界）：`released reason=load-boundary` → `born`（新 formID）。
6. F2 关/开各一次：`driver=` 行应显示真实链（off / puppet:…）。
7. 日志复制进 `docs/diagnostics/`（惯例名 `CharacterPanel-hkx39-calibration-20261011.log`）。

## 判读表

| # | 检查 | 通过判据 | 失败时 |
| --- | --- | --- | --- |
| 1 | 运动钉 | parked 行带 `motion=fixed`；游玩期 digest 无 JumpFall/JumpLand | 下坠照旧 = 物理钉没压住，回传 |
| 2 | **A/B 常驻体检** | 每次切换后 ~1s 一条 ab 行；`mapping=identity-unverified` 的替换件里出现 `verdict=match`（max-rot ≤1°） | 全部 `none` 且 max-rot 大 = 恒等映射对本机替换件系统性失效——**视觉标定升级为换轨道序映射**，把全部 ab 行回传 |
| 3 | match 与观感一致 | ab=match 的 clip 上面板姿态看起来**对**；ab=none 的 clip 上可能不对 | 不一致 = A/B 阈值或混合期干扰，回传对应 ab 行 |
| 4 | **equip 杠杆** | 换装时 `equip … pushed=draw` 且 digest/驱动行随后出现装备相关 clip；面板人物当场有拔/收动作 | equip 行出现但无 clip 变化 = 杠杆没推到图（诚实记档：暂停期装备反应退回"池子回放"）；面板无动作但 clip 切了 = 显示链问题 |
| 5 | 读档边界 | `released` → `born` 新 formID；零残留零崩溃 | 残留 = 回传 |
| 6 | 零回归 | 零崩溃、零新 `[E]`、H05 `npc-set=+0/-0`、驱动链照常 | —— |

**路线含义**：#2 全绿 = 恒等映射在本机替换件上数值成立，显示保真有了常驻数值背书，S2 的
FR-03 数值验收基本闭环；#2 全红 = 需要"轨道序映射"（骨架哈希对表）或视觉标定文档化，仍是
S2 内工作。#4 红不影响路线（equip 已有池子回放兜底）。

---

## HKX39 判读（已实测，2026-10-12 凌晨；日志 [CharacterPanel-hkx39-calibration-20261011.log](diagnostics/CharacterPanel-hkx39-calibration-20261011.log)）

实测规模：00:04–00:10 单会话；傀儡 00:05:04 泊位（motion 钉生效）；暂停面板时间约 4 分钟；
1 次 driver 切换 + 1 条 ab；2 次 equip 反应；零崩溃。

| # | 检查 | 结果 |
| --- | --- | --- |
| 1 | 运动钉 | **通过**——全日志零 JumpFall/JumpLand（HKX38 的下坠循环消除）；park 行缺 `motion=fixed` 字样 = 纯日志化妆漏改（SetMotionType 在源码 257 行实装生效），HKX40 补 |
| 2 | **A/B 常驻体检** | **仪器成立，并抓到决定性事实**——`MT_Idle#12219 mapping=identity-unverified bones=37/99 unnamed=62 max-rot=109.16deg verdict=none`：恒等映射对该 OAR 替换件**数值失败**（HKX28 的 0.15° 成立只对个别变体，如 #12220；#12219 这类就错位）——恒等档的"逐变体运气"从此有常驻测量 |
| 3 | match 与观感 | **暴露第二问题**——可比骨只有 37/116：泊深处 culler 不渲染傀儡 → boneNodes 未绑槽位 `node=null` → 姿态桥半盲（HKX38 的 coverage 行只数了表长 116/116，掩盖了这一点）。ab 的可比面与显示保真都被泊深卡住 |
| 4 | **equip 杠杆** | **机制成立**——两次 `equip … pushed=draw reactions=N` 正确触发；图最终切到 **1HM 族**（`1HM_Idle.hkx#8970 + SCAR_1hmReadyDummy#15075`，SCAR 是武器 idle 包 = OAR 对持武傀儡的实时决策），且族内变体 #8970→#8975 的重选发生在**暂停期**（00:10:11，换武器后 7s）。"第一次 draw 推送是否当场生效"因 F2/开关包时序穿插日志无法定论——下次测试面板内盯着看一次即可 |
| 5 | 读档边界 | **再次未测**（全程未读档；连续两轮欠账，HKX40 测试单放第一步） |
| 6 | 零回归 | **通过但有一个新观测**——64 行 `[E]` 全部出自**同一根因**：00:08:24 用户换装后，O 的图对象集 1584→1589（武器网格挂进玩家骨架）→ 隔离仪器的全会话基线被合法用户动作污染，后续每次普查都重报 DRIFT。操作窗口本身全 CLEAN（`idle-toggle pre→post verdict=CLEAN`）——仪器没错，是"用户装备动作 = 合法世界变化"需要再基线 |

### 结论与 HKX40 范围（按优先序）

1. **泊位回退"玩家身边 + fade 0.05 + kFixed + 钉桩"**（HKX37 已实跑 7 分钟无副作用）——
   culler 持续渲染 → boneNodes 全绑定 → 姿态桥 116/116 → ab 全骨可比。泊深退役为备选。
   （HKX40 第 1 项，改动 ~10 行，解锁第 2 项的全部判读力。）
2. **轨道序映射（骨架哈希对表）或逐变体视觉标定**——ab 已证明恒等档逐变体运气（#12220 过、
   #12219 砍 109°）。先在泊位回退后重测：全骨可比时若仍 none → 上映射；若过 → 泊深半盲
   是主因，视觉标定收尾。
3. **隔离仪器的装备再基线**——`poll_equipment` 检测到变化时通知隔离会话重设基线（消除 64 行
   合法动作噪音），操作窗口 CLEAN 的语义保持不变。
4. equip 杠杆"当场性"目视确认（测试动作改为：面板内换武器、眼睛盯着面板人物）。
5. park 行补 `motion=fixed` 字样（化妆）。
遗留队列：HKX40（上述 1–3）→ S1 安全回收 → OAR fork（免立项趋势不变）。
