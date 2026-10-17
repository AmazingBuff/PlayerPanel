# 面板姿态与动作修复：oar-idle3

日期：2026-10-16。基线：`cdb5afe`（oar-idle2），修复提交 `3275efa`。
本轮修复 oar-idle2 上机实测暴露的五个问题；游戏行为仍待本轮验证。

## 用户报告与根因（日志 `CharacterPanel.log` 22:02–22:14 会话）

| # | 报告 | 根因（证据） |
| --- | --- | --- |
| 1 | 跳跃状态进背包，面板人物向下偏移 | 取景中心 = capture 瞬间的蒙皮包围盒中心；跳跃蜷腿姿态的中心偏高，对齐后面板人物整体下沉（每次捕获的 `SCOPY FRAME` center Z 各不相同，姿态相关） |
| 2 | 持械攻击状态进背包，面板不播该武器 idle | 引擎开背包时已把武器收起；全会话 168 次查询 `drawn` 恒 false、0 次武器族查询——事后读 `IsWeaponDrawn()` 永远迟到 |
| 3 | 背包内换武器不播切换动作 | transition 只在 `desired.drawn || active.drawn` 时触发；收鞘状态换武器两边的 drawn 都是 false，永不触发 |
| 4 | 弓、爪等武器使面板人物位置偏移 | 取景测量只排除了碰撞帮手；弓/箭袋几何挂在 `Weapon*`/`Quiver*` 挂点子树下，把中心拖离身体 |
| 5 | 打开背包瞬间很卡 | `QuerySession` 的动画名表缓存随 `PanelAnimation::reset()` 丢弃；开包帧在主线程重建 ~15k 条名字（每条一次 `VirtualQuery` + 拷贝）。日志：面板冷启动 `SCOPY READY → query` 空档 5.0–6.9 秒，同会话内换装查询只要 6–10ms |

## 修复

1. **姿态就绪后一次性重居中**（问题 1）：`CharacterClone` 增加 `m_recentre_pending`；
   `SceneGraphCopy::animate_at_base_pose` 在 idle 首帧真正写入节点（`advance` 返回写入数 >0，
   接口从 void 改为 `std::size_t`）后置位；下一次 `pose()` 在快照定位后按实测蒙皮包围盒对
   anchor 重居中一次并打 `SCOPY FRAME recentre offset=…` 行。一次收敛，不做每帧闭环
   （避免包围盒逐帧变化的抖动）。
2. **进包姿态快照**（问题 2）：`PanelMonitor::MenuSink` 在 InventoryMenu opening 事件上升沿
   采样 `IsWeaponDrawn()`（菜单事件先于引擎的菜单时收鞘生效，读到的是进入姿态）；
   `panel_animation` 侧所有装备读数统一走 `panel_equipment()`（= 实时读数 + 快照 drawn +
   空手否决：form 为 kNone 时强制 not drawn）。玩家在背包内主动出鞘/收鞘（装备事件）仍由
   实时读数覆盖。
3. **收鞘换武器播 equip**（问题 3）：transition 触发条件放宽为 `drawn 变化 || form 变化`；
   收鞘状态下的武器族切换（sword→sword）按**新武器的 equip** 处理（引擎在世界侧播的就是
   equip），真正的收鞘语义只保留给"武器消失"（卸下/清空）。
4. **取景排除武器挂点**（问题 4）：`measure_skinned_bound` 增加 `is_framing_attachment`——
   沿父链命中 `Weapon`/`Quiver`/`Ammo` 名字的子树不进取景（AFP8 已测得 `Weapon*` 五节点是
   动画不驱动的挂点离群者）。只影响测量；挂点几何照常绘制。
5. **动画名表目录级缓存**（问题 5）：`QuerySession::names` 的重建结果按 `{owner, data, count}`
   存进程级缓存（每 rig 一条，新指针自动置换旧条目）；面板会话重置后冷查询直接命中，
   开包帧不再重建。选择/变体/随机状态仍按会话隔离（设计不变）。

## 验证与包

默认（Actor 路线）与实验（scene-copy）两个 Release 配置构建通过；`snapshot_transform`
33 项全过，其中新增两项：`stance and transition contract`（收鞘换武器 = equip 契约）、
`project catalogue reuse`（跨会话目录复用 + 换表重建）。

| 项目 | 值 |
| --- | --- |
| 文件 | `dist/CharacterPanel-scopy-oar-idle3-1.2.1.zip` |
| 构建身份 | `CharacterPanel-scopy-oar-idle3-3275efaf77-cl94faaed0c6-20261016T153112Z` |
| DLL SHA-256 | `a36b8dd1…` |
| PDB SHA-256 | `ad4974f3…` |
| 部署 | `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\`（DLL+PDB 已同步校验） |

## 第十二轮：驻留位=序列最后一个成员（2026-10-17，提交 `cde4418`，部署待游戏退出）

用户实测（15:56 会话，构建 `aa3bea861c`）：Start 段进包仍播 Start。第十一轮的历史反向
读取**已生效**（drawn=true ✓、世界首选命中 `1hm_idle` replacement ✓），但 seek_dwell 仍落
Start——**该包所有成员都勾了 PlayOnce**："从尾部找第一个非 once"的搜索一无所获，落回
原始 pick（position 0 = Start）。

修复：驻留位定义简化为**序列最后一个成员**（Start→Loop 编排的循环体必在尾部；与第四轮
commit 驻留规则完全一致）。channel=3（手部 idle `MLh_Idle` 层）也补了 seek_dwell——它同路
落在了 `Start\mlh_idle`。

包身份 `CharacterPanel-scopy-oar-idle3-cde4418816-cl94faaed0c6-20261017T080655Z`。
判读：Start/Equip/Unequip 段进包 → asset ready 名字应含 `**Loop**`（channel=2 与 3 都是）；
换装接力（Equip→Start→Loop）不变。

## 第十一轮：进包姿态读整段历史（2026-10-17，提交 `aa3bea8`，部署待游戏退出）

用户报告：Equip/Unequip/attack 段进包，面板播 MT_Idle。14:56 会话三个场景全部复现：
capture=4（attack 后）=5（1HM_Equip 中）=6（1HM_Unequip 中）全 `drawn=false`。

根因：按名规则只看**最后一个** clip，而这三段的最后 clip（`1HM_Equip`/`1HM_Unequip`/
`BFCO_Attack1`）名字里都没有 "idle"——姿态提升与世界 clip 首选双双不触发，回落站立
idle。weaponState 位在开包帧已被引擎清掉，救不了。

修复：记录器保留**最近 clip 历史**（8 条环形），两个读取都**从尾部反向**走：
`stance_at_open()` 找到第一个 idle 命名 clip——武器 idle 提升 drawn=true、unarmed 基础
族（MT_Idle/走跑跳/Mag_）确定收鞘侧并停止回溯；`last_idle_clip()` 返回同一个名字供
面板首选装载（Equip 中进包 → 装载进包前世界在播的那个武器 idle）。查无 idle 才落回
weaponState 位。

包身份 `CharacterPanel-scopy-oar-idle3-aa3bea861c-cl94faaed0c6-20261017T070239Z`。
判读：Equip/Unequip/attack 段进包 → `phase=idle form=… drawn=true` 且 driver=进包前世界
在播的武器 idle；收鞘链（…→Unequip→MT_Idle 后进包）仍站立 idle（反向走先碰到 MT_Idle
即停止）。

## 第十轮：seek_dwell 从尾部找驻留位（2026-10-17，提交 `cef8a66`，部署待游戏退出）

用户实测（14:47 会话，构建 `12c38e7ee1`）：Start 段开包，面板仍播 Start（index 9038 =
`1hm Non-Combat idle Start`）。根因：seek_dwell 从 pick 位置**向前**找第一个非 once 成员，
但该包的作者**没有勾 PlayOnce**（纯 sequential 顺序推进）——位置 0 的 Start 本身就是
"非 once"，搜索原地停止。

修复：**从序列尾部反向**找第一个非 once 成员——Start→Loop 编排的驻留位在尾部，与第四轮
`commit` 驻留规则同一定位。once 标志齐备的包（尾部 Loop 勾了循环、Start 勾 PlayOnce）与
不勾的包都落在 Loop。

包身份 `CharacterPanel-scopy-oar-idle3-cef8a668a0-cl94faaed0c6-20261017T065302Z`。
判读：Start 段/Equip 段/Unequip 段开包 → `asset ready` 的名字应含 `idle **Loop**`
（不再是 Start）；换装后接力（e_activate 从 Start 起）不受影响。

## 第九轮：idle 序列完整接力 + 中段进包直接驻留（2026-10-17，提交 `12c38e7`，已部署 `fff8d090…`）

用户对记录序列的理论解读：1hm 拔刀是**三段接力**——`1HM_Equip`（过渡）→ `1HM_Idle` 的
**Start** 成员（入口，消化过渡残留）→ `1HM_Idle` 的 **Loop** 成员（驻留循环）。这正是 OAR
sequence 机制的用途。两项实现：

1. **transition 播完 → 序列重开**：Equip/Unequip 播完后 `restart_idle_after_transition()`
   以 `e_activate` 重查各 idle 层（该 clip 的序列进度重置），拿到 Start 成员；Start 播到
   循环边界走既有 `e_loop` 重查进 Loop 驻留。完整复刻游戏内 Equip→Start→Loop 顺序。
2. **中段进包 seek_dwell**：拔刀/换装发生在开包**之前**时，世界的接力已进行到中段——
   进包的 idle 查询带 `seek_dwell`（`QueryRequest` 新字段），replacement 的序列跳过
   once 成员直接落在尾部循环体，`SelectionState::seek_dwell` 把历史钉在该位（下次循环
   重评继续驻留，不会回头补播 Start）。Equip/Start/Unequip 任意段进包，面板都表现为
   Loop 段 idle——与游戏内观察者视角一致。

包身份 `CharacterPanel-scopy-oar-idle3-12c38e7ee1-cl94faaed0c6-20261017T063955Z`，DLL
SHA-256 `fff8d090…`，已部署。

### 上机判读（第九轮）

1. 拔刀后进包（Equip 播放中）→ 面板**直接 Loop 段 idle**（无 Start 重播）；
2. 背包内换武器 → equip 播完 → 日志出现 idle 层 e_activate 重查（Start 成员 index）→
   Start 播一个循环边界 → e_loop 换 Loop 成员驻留；
3. 收刀进包 → 站立 idle 驻留位；
4. 无 Start 重播、无回卷循环（第四轮 dwell 修复的回归判据）。

## 第八轮：按名判定挪到读取侧（2026-10-17 傍晚，提交 `b5ebd0d`，已部署 `8cf6ca84…`）

用户报告：持械进包面板仍站立 idle（第七轮的按名规则没生效）。13:33 会话日志：13:33:21
拔刀 → 13:33:23 `MLh_Idle` → 13:33:32/35/43/45 四次开包全部 `drawn=false`。

**根因是写入侧时序竞态，无法在写入侧赢**：开包的同一输入帧里引擎既清了 weaponState 又
发了菜单事件；渲染钩子在 menu_open 置位**之前**还跑得到一次观测——把 false 写了进去。
之前"解决过"的各轮（快照、三源、手槽、世界帧观测）全是在换写入源，而这个竞态写入侧
必输。

修复（贴合"名称即接口"的最终形态）：**判定挪到读取侧**——`stance_at_open()` 直接检查
记录的世界 clip 名：idle 命名但非 unarmed 基础族（MT_Idle/潜行/走跑跳/Mag_ 移动）=
武器在手，返回 drawn=true；位读数只负责 false 方向。读取时名表早已稳定，竞态不存在。
`observe_stance_from_clip` 写侧版本退役（留注释说明竞态）。

### 上机判读（第八轮）

1. 拔刀 → 持械站立任意时长 → 开包：`phase=idle form=… drawn=true`（读取侧按名命中）；
2. 收鞘状态开包：仍 drawn=false 站立 idle（位读数未被破坏）；
3. 移动/跳跃后开包：Mag_/MT_Walk 等移动 clip 不触发提升（base_idle 名单覆盖）。

## 第七轮：演示序列到手，面板切换按序实现（2026-10-17 下午，提交 `0872061`）

序列记录仪首战告捷。用户实测（13:19–13:22）记录到游戏内完整的拔刀循环：

```
MT_Idle（收鞘站立）
1HM_Equip.hkx（拔刀，约 1.1 s 一次性）
MLh_Idle.hkx（持械 idle——左手法术持械姿态，循环）
1HM_Unequip.hkx（收刀，约 1.1 s 一次性）
MT_Idle（回收鞘站立）
```

两条直接产出：

1. **进包姿态按名判定**：13:21:48.3 `1HM_Equip` → 13:21:48.8 开包 capture——**拔刀动画
   播放中 weaponState 位还是 false**（kDrawing 不算 drawn），采样窗口比想象窄。新增
   `observe_stance_from_clip()`：世界序列**最后稳定 idle** 是武器/手部 idle（含 "idle" 且
   非 unarmed 基础族 MT_Idle/sneak/走跑跳）→ promoted drawn=true；false 方向仍由
   weaponState 位独占。13:22:01–06 世界在播 MLh_Idle 时开包 → 面板 drawn=true。
2. **切换动作换真名**：transition 基础名序列以实测拼写领头——`1HM_Equip`/`1HM_Unequip`
   （原版大写 HM；此前一直猜写小写 `1hm_equip`，靠 find_base_animation 的大小写归一才
   命中）。2HM/2HW/Bow/CrossBow 同步按原版命名惯例领头，小写保留同序列回退。

包身份 `CharacterPanel-scopy-oar-idle3-0872061069-cl94faaed0c6-20261017T052809Z`，DLL
SHA-256 见 manifest。**部署待游戏退出**（本会话末尾游戏仍在运行）。

### 上机判读（第七轮）

1. 拔刀后 1–2 秒内开包 → `phase=idle form=1h drawn=true`，driver=世界侧同款武器 idle
   （`MLh_Idle`/对应族）；
2. 背包内换武器 → transition 查询命中的名字应为 `1HM_Equip`（日志 asset ready 行）；
3. 收鞘状态开包 → 仍站立 idle（false 方向未被破坏）。

## 第六轮：序列记录器，名称即接口（2026-10-17 下午，提交 `a3788b4`+`97d3ff49`）

用户演示了一次拔刀→收刀全程，但日志里没有留下痕迹——世界帧观测只把"当前 idle 名"存在
内存、只在测过渡时长时打一行。无法凭空知道游戏内拔刀的动作顺序，面板的"按游戏内顺序
播放"无从谈起。

本轮交付**序列记录仪**（用户方向：按 OAR 语义，**名称是唯一接口**——OAR 查询按名拿
index，替换/变体/权重决策是 OAR 内部的事，我们只按名取动作）：

- 世界帧观测改为记录**权重最高的任意名字 clip**（拔刀/收刀/攻击这些一次性动作名字不含
  "idle"，此前全被过滤），**每次 clip 变化**打一行 `SCOPY WORLD clip '<名字>'`；
- weight 只保留一个用途：crossfade 实测（引擎混合时长的诚实来源），不参与任何决策；
- 面板装载同一接口：世界 clip 名 → `find_base_animation` 名表解析 → OAR 查询拿 index →
  OAR 自己的替换决策。名→index→动作，与站立基础查询完全同路。

包身份 `CharacterPanel-scopy-oar-idle3-97d3ff4955-cl94faaed0c6-20261017T051306Z`，DLL
SHA-256 `86c2a2cc…`，**已部署**。

### 上机流程（下一轮数据采集）

1. 进游戏（新 DLL 已装）；
2. 完整演示：拔刀 → 持械站立几秒 → 收刀（建议单手剑与弓各一遍）；
3. 退出或把日志发我——`SCOPY WORLD` 行的先后顺序就是游戏内动作序列的真容。

下一轮按该序列把面板的装备切换做成同序播放：切换动作用序列里的真实 clip 名（替代
猜写的 `1hm_equip`），起止衔接与过渡时长按实测。

## 第五轮上机判读与"武器跟手"的架构收口（2026-10-17 中午）

上机（12:34 会话，构建 `3d18908b3a` 部署版）判读：

- **过渡实测成立**：`measured world crossfade 0.07–0.30s`（跟 clip 相关——MT_Idle 0.30s、
  MLh_Idle 0.07–0.12s），面板混合时长来源改为实测值 ✓；
- **收鞘开包全轮 drawn=false**：本轮用户全程收鞘测试，武器 idle 从未查询（channel=2 零次）
  ——收鞘 → 站立 idle，符合第五轮问题 1 的新规则 ✓；
- **世界 clip 首选**：本轮收鞘场景不触发武器 idle，判据待出鞘测试（`panel-idle:<世界 clip>`
  行）；
- **武器跟手**：`SCOPY WEAPON` 行显示 `WeaponSword` world 仅小幅摆动（±1 单位）——hint 命中
  后挂点轨道**确实在驱动**，但 equip 动画的 `Weapon` 轨道本就是小幅动画。

**"武器跟手"的最终定性（架构收口）**：原版"武器从鞘到手的完整轨迹"由 behavior graph 的
weapon-draw 逻辑完成（biped 把武器模型从鞘挂点 reparent 到手挂点 + 动画），不是单个 hkx
的挂点轨道。隔离快照副本没有 behavior，这一轨迹**无法从 equip 动画得到**。在快照架构内：

1. **出鞘状态 capture**（武器在手进包）：武器模型已在手骨子树下，手骨被 idle 驱动 →
   **武器跟手自动成立**——这是快照架构内"跟手"的完整形态；
2. **收鞘状态换武器**：新武器模型随重捕获进入快照（capture=12→16 在跑），但 equip 动画
   播放期间模型保持在鞘位——这是快照架构的边界，非 bug。要突破需要 behavior 集成或
   手工 reparent 动画（独立立项评估）。

判读表中"武器跟手"从此按 1 的场景验收（出鞘状态进包后武器随手部 idle 移动）。
部署版 DLL `185d9d98…` 已由用户自行安装（游戏占用，本会话未复制）。

## 第五轮：世界侧观测主导（2026-10-17，提交 `3d18908`，待部署——游戏占用，退出后重试 cp）

上机（11:47 会话，构建 `e10a459f4a`）判读，三项证据改写规则：

1. **trim 生效、变体选错**：`unresolved=''`（空）、`resolved=95/96/103`——挂点轨道已全部
   落点；但 `SCOPY WEAPON` 显示武器模型（`Weapon (formID)`）仍挂在 `WeaponSword` ←
   `MOV WeaponSwordDefault`（收鞘 MOV 链）上不动——**最长前缀把 `Weapon` 骨解析到了错误
   变体**（`WeaponSwordLeft`/`Weapon L Calf` 这类）。修复：`panel_weapon_node(form)` 按武器
   形态给出**精确手部挂点名**（1h/2h→WeaponSword、2hw→WeaponAxe、bow→WeaponBow、
   staff→WeaponStaff、crossbow→WeaponCrossbow），`resolve_binding_playback` 的 `Weapon` 骨
   hint 优先、前缀回退；hint 经 `LoadedAsset` 贯通到 idle 与 transition 两路。
2. **收鞘误播持械 idle**（用户报告 1）：手槽规则矫枉过正——挂腰/背的武器也在手槽。drawn
   回归**世界帧观测**（`observe_stance`，出鞘进包=true、收鞘进包=false），非战斗挂武器
   → 普通站立 idle。
3. **最终 idle 应为游戏内真实动作**（用户报告 2）：面板武器 idle **首选世界侧实际在播的
   idle clip**——渲染钩子每 10 个未暂停帧用 `strongest_idle_clip` 记录玩家最高权重 idle 的
   名字，面板查到该名字直接装载（`find_base_animation` 同项目解析），OAR sequence 查询仅
   回退；日志 `SCOPY PANEL world idle clip not resolvable` 标记回退路径。
4. **过渡时长实测**（用户报告 3）：不再自选常数——观测玩家 idle 混合权重**低于 1.0 的持续
   时间**（引擎 crossfade 的直接痕迹），面板混合用最近实测值（无数据回退 0.4s）；每次测得
   打 `SCOPY PANEL measured world crossfade X.XXs`。

包身份 `CharacterPanel-scopy-oar-idle3-3d18908b3a-cl94faaed0c6-20261017T041708Z`，DLL
SHA-256 `185d9d98…`，PDB `cd65c166…`。**部署未完成：游戏运行中 DLL 被占用**——退出游戏后：

```powershell
Copy-Item dist\CharacterPanel-scopy-oar-idle3\SKSE\Plugins\CharacterPanel.dll,
          dist\CharacterPanel-scopy-oar-idle3\SKSE\Plugins\CharacterPanel.pdb
        E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\
```

### 上机判读（第五轮）

1. `SCOPY PANEL measured world crossfade X.XXs` 行出现（过渡时长来源）；
2. 出鞘状态开包：`phase=idle form=… drawn=true` 且 driver=**世界侧同款 idle**（日志
   `panel-idle:<世界 clip 名>`）；收鞘开包：drawn=false + MT_Idle；
3. equip/idle 期间武器跟手（hint 命中后 `Weapon` 轨道驱动正确变体；若仍不动，
   `SCOPY WEAPON` 行的 node world 应随 parent 变化——把日志发我）。

## 第四轮：骨名 trim、sequence 驻留、crossfade 0.4s（2026-10-17，提交 `e10a459`）

上机（01:45 会话，构建 `cb556cf390`）判读：**别名回退没有命中**（resolved 仍 93、
unresolved 仍含 `Weapon`）——代码在部署包里，唯一解释是**骨名带隐藏空白**：差集里
`x_NPC Translate [Pos ]` 实锤 Havok 命名惯例，`Weapon` 大概率同病，`== "Weapon"` 精确比较
永远假。另两个新报告同轮定位：

| # | 报告 | 根因 | 修复 |
| --- | --- | --- | --- |
| 1 | 武器仍不跟手 | 骨名隐藏空白使 `bone_name == "Weapon"` 恒假，别名分支从未执行 | 解析前 trim 骨名；差集打印加引号（`'Weapon '` 形态），本轮结果一锤定音 |
| 2 | 面板播"攻击动作的循环" | OAR sequence（Start→Loop→Loop2）全部 once 播完后 `commit` 清历史回 0——面板每轮重播 Start（起手动作，观感即攻击）；OAR 世界侧语义是**驻留**尾部的循环体 | `commit` 全播完后 `next` 指向最后一个非 once 成员；仅纯 once 序列才回 0 |
| 3 | equip→idle 几乎无过渡 | crossfade 0.15s 太短（引擎自己的 hand-off 在 0.3–0.5s 量级） | 0.15s → 0.4s |

包身份 `CharacterPanel-scopy-oar-idle3-e10a459f4a-cl94faaed0c6-20261017T033230Z`，DLL
SHA-256 `e171232c…`，已部署校验。

### 上机判读（第四轮）

1. `asset ready` 行：`resolved` 应 ≥96（Weapon/Quiver/Shield 三条挂点轨道落点）；若仍 93，
   引号化的 unresolved 名字会直接暴露骨名真实形态——按它修下一层；
2. 武器在 equip/idle 期间跟手移动；
3. 面板 idle 循环不再回到起手/攻击类动作（Start 只在进包/换装后播一次）；
4. equip 播完后 0.4s 平滑过渡到持械站立。

## 第三轮：手槽即姿态 + 挂点别名匹配（2026-10-17，用户澄清"战斗状态=武器在手"，提交 `cb556cf`）

第二轮证据的两个决定性事实：

1. `weaponState` 族信号**全部退役**：每帧未暂停观测写出的仍是 false——引擎在世界帧就该
   是 drawn 的状态下读数依然失败（用户实测三连败）。用户的定义本就不需要它：
   **"战斗状态 = 手槽装备着武器"**——`form=1h/2hw/bow` 行一直是对的，手槽读数从没错过。
   最终规则：**手槽有武器 → drawn=true（对应武器族 idle）；手槽空 → 普通站立 idle**。
2. `unresolved='x_…|Shield|Weapon|Quiver'` 实锤 **equip 动画确实 key 了挂点骨**——武器不跟
   手的原因是**名字系统差一层**：动画骨架用通用名（`Weapon`），NIF 层用变体名
   （`WeaponSword`/`WeaponAxe`/…），按名解析从未落点 → 挂点轨道被丢弃 → 武器钉死。
   修复：这三个通用名解析失败时回退**最长前缀变体节点**（每槽实际只带一把，无歧义风险）。

包身份 `CharacterPanel-scopy-oar-idle3-cb556cf390-cl94faaed0c6-20261016T173859Z`，DLL
SHA-256 `7c52c779…`，已部署校验。

### 上机判读（第三轮）

1. 手槽有武器开包（不管此前出没出鞘）→ `phase=idle form=… drawn=true` + channel=2 武器族
   idle；手槽空 → drawn=false + MT_Idle；
2. equip 播放时武器跟手（挂点轨道落点后动画自己驱动 `Weapon` 骨）；`asset ready` 行
   `resolved` 应从 93 升到 96（+Weapon/+Quiver/+Shield）；
3. transition 播完回对应武器族 idle（drawn 正确后的连带效果）。

## 第二轮判读与信号换源（2026-10-17 凌晨，提交 `8671eba`）

上机（01:13 会话，构建 `5567507d40`）判读：三问题仍在。两个决定性证据：

1. **`weaponState` 在菜单事件前已被引擎清除**——用户"武器在手开包"的全会话 drawn 仍 0 次
   true，三源判定里的实时/快照两个源都死于同一冻结；上一轮"事件先于收鞘"的时序假设**证伪**。
2. **`SCOPY WEAPON` 行揭示挂点结构**：武器模型节点挂在 `WeaponSword` 下、其父是
   `MOV WeaponSwordDefault`（收鞘动画挂点骨架）——武器被钉在**收鞘挂点**上跟着收鞘骨动。
   引擎的"武器到手"是 biped 的 hand-attach 切换，隔离副本不跑这套。

修复（信号换源，提交 `8671eba`）：

- **进入姿态 = 最后一个未暂停世界帧的 weaponState**：`PanelMonitor::observe_stance()` 由
  渲染钩子每帧（非暂停）写入；菜单事件不再采样任何东西（时序上已经输了）。用户定义的
  "战斗状态 = 武器已在手上"由此诚实回答——出鞘状态进包，最后一个世界帧的读数就是 drawn=true。
- **武器跟随的证据先行**：`asset ready` 行的 coverage 现在列出**未解析轨道骨名**（最多 8 个）
  ——决定 MOV 手部挂点骨是否随 equip 动画被 key。三种可能：在轨道里且已解析（动画自己会
  移挂点，问题 2 应自愈）；在轨道里但解析失败（修名字匹配）；根本不在轨道里（下一轮做
  hand-attach 重挂）。

包身份 `CharacterPanel-scopy-oar-idle3-8671ebacaf-cl94faaed0c6-20261016T172604Z`，DLL
SHA-256 `385695d1…`，已部署校验。

### 上机判读（第二轮）

1. 出鞘状态开包 → `phase=idle form=… drawn=true`（`observe_stance` 生效）；
2. `asset ready ... unresolved='…'` 行——若出现 `MOV Weapon`/`WeaponSword` 字样即挂点骨随
   动画，看解析是否命中；若差集只有 `x_`/`NPC` 杂骨 → 挂点不在动画里，问题 2 走 hand-attach 重挂；
3. transition 后回武器 idle 的判据同前一轮。

## 战斗姿态与武器跟随（2026-10-17，用户三问，提交 `5567507`）

上机判读（00:45–00:48 会话）：①校准摆位成立（`calibrated` 每捕获 1 条、radius 27.8 稳定）；
②全会话 `drawn=true` **0 次**；③equip transition 正常播放但武器几何钉在捕获位置。

| # | 用户报告 | 根因 | 修复 |
| --- | --- | --- | --- |
| 1 | 战斗状态开包不播持械 idle（含空手战斗） | `weaponState` 位在背包菜单会话内**冻结**——进入快照只覆盖"出鞘状态进包"，战斗中收鞘开包、或菜单内换装后的玩家，三个 drawn 来源（实时/快照）全是 false | drawn = `weaponState ‖ 进入快照 ‖ (战斗中 && 手上有武器)`；空手 + 战斗保持 drawn=true（form kNone + drawn 查 h2h_idle 族 = 空手战斗姿态） |
| 2 | equip 动作播放时武器不动 | 稳态 `pose()` 每帧把手写世界（`delta × 捕获世界`）盖回全树，然后动画才级联骨架——挂点节点每帧先被钉回捕获位置 | 稳态分支删掉手写世界循环，只设校准根 + `UpdateDownwardPass` 级联——级联对全部节点权威，武器作为被驱动手骨的子树自然跟随；新增 `SCOPY WEAPON` 观测行（挂点 world vs 父骨 world，120 帧节拍） |
| 3 | transition 后直接回站立 idle | drawn=false 使武器族 idle 层（channel=2）从未装载，transition 完成后没有武器 idle 可回 | 由 ①连带解决：drawn 正确后武器 idle 随集合装载，transition 完成即回武器 idle |

包身份 `CharacterPanel-scopy-oar-idle3-5567507d40-cl94faaed0c6-20261016T170950Z`，DLL
SHA-256 `a2f23db9…`，已部署校验。单测 33/33。

### 上机判读

1. 问题 1：战斗中持单手剑+法术开包 → `phase=idle form=… drawn=true`，channel=2/3 出现
   `1hm_idle`/`MRh_Idle` 族查询；空手战斗 → form=none + drawn=true 查 h2h_idle；
2. 问题 2：equip 播放期间武器跟随手部移动；`SCOPY WEAPON` 行的 node world 应随 parent
   world 变化（差值恒定 = 跟随成立；node world 恒定 = 仍被钉死，需下一层）；
3. 问题 3：transition 完成后 `phase=idle form=<武器族>` 且 driver 是武器 idle 而非 MT_Idle。

## 校准摆位：以站立 idle 为定位标准（2026-10-17，用户方案，提交 `2f5ed1f`）

用户指出此前 recentre 方向对但机制弱：定位基准仍是"捕获姿态的包围盒中心"——跳跃捕获
中心偏高（面板人物偏下）、潜行捕获偏低（偏上），recentre 只是事后补偿。正确做法：
**先用站立 idle 自身算一次包围盒作为标准，把根冻结到这个标准上，之后所有动作共用。**

实现（快照路线 `pose()` 两阶段）：

1. **校准期**（新快照的最初几帧）：`m_recentre_pending` 置位（快照构造函数默认 armed），
   `pose()` 只做 raw 摆放（捕获姿态进相机系，`m_calibrated_delta_base` 记下这个 delta），
   不做任何以捕获中心为基准的求解；
2. **校准**：动画桥（`animate_at_base_pose`）在 idle **首次写入关节**的当帧调
   `calibrate_from_posed_bound()`——实测此刻（站立 idle）的蒙皮包围盒，把
   `锚点 − 校准中心` 折进根，冻结为 `m_calibrated_root` / `m_calibrated_delta`。
   日志 `SCOPY FRAME calibrated offset=… radius=…`，每次捕获恰一条；
3. **稳态**：之后每帧 `pose()` 直接套用冻结根，零测量零求解——跳跃、潜行、换装过渡、
   后续捕获都在同一条标准上播放，位置与进入姿态彻底解耦。

工程细节：校准当帧用引擎级联（`UpdateDownwardPass`）从新根重摆——校准时动画已写入
local，手动重写捕获世界会把 idle 砸掉（白剪影教训在同一窗口复现过）；校准失败（裸骨架
竞态无可测包围盒）时丢弃请求、保留捕获基准，不冻结垃圾修正。包身份
`CharacterPanel-scopy-oar-idle3-2f5ed1fd19-cl94faaed0c6-20261016T164203Z`，DLL SHA-256
`4bf88ab7…`，已部署校验。

判读：跳跃/潜行/游泳进包人物位置一致（与站立进包完全相同）；`calibrated` 行每次捕获
1 条，`radius` ≈ 28（站立 idle 的量级，不再是捕获姿态的 67–94 波动）。

## hotfix：首帧正常随后缩成白色剪影（2026-10-16 深夜，提交 `9276a40`）

上机发现：面板人物首帧大小正常，第二帧起缩成小的白色剪影。根因在 oar-idle3 的重居中块：

1. **重复武装**：`animate_at_base_pose` 每帧动画写入后都置位重居中请求（日志 2873 次），
   不是设计的一次；
2. **手写世界循环砸烂蒙皮**：重居中块里用 `root_world * node.local` 逐节点重写世界——
   该式只对根的直接子节点成立，**孙子节点丢失中间骨骼变换**，所有非动画后代（衣甲）的
   蒙皮矩阵坍缩 → 几何缩成一簇（变小）+ 法线垃圾（白剪影）。首帧正常是因为该块尚未跑过。

修复：删掉手写循环（`UpdateDownwardPass` 的级联本来就正确）；武装改为每次 capture 严格
一次（`m_recentre_armed`，capture 时复位）。包身份
`CharacterPanel-scopy-oar-idle3-9276a408c7-cl94faaed0c6-20261016T160914Z`，DLL SHA-256
`966d8bd8…`，已部署校验。判读：`recentre` 行每次捕获恰好出现 1 次，人物不再缩小/发白。

## 上机判读表

1. **问题 1**：跳跃/潜行进包 → 日志出现 `SCOPY FRAME recentre offset=…`，面板人物站位居中；
   offset 数值应在几十单位内（原偏移量级）。
2. **问题 2**：持械（任意武器族、攻击后）开包 → `SCOPY PANEL query` 出现 channel=2 的武器族
   基础查询（`1hm_idle`/`bow_idle` 等），`phase=idle form=… drawn=true`。
3. **问题 3**：背包内换同类武器 → `SCOPY PANEL query channel=4`（`1hm_equip` 等）+ `phase=transition`
   + 播完 `transition complete -> idle`；收鞘状态换不同族武器同样应播新族 equip。
4. **问题 4**：装备弓/爪开包 → 人物仍居中；`SCOPY FRAME` 的 radius 不再因挂点几何跳变
   （对照 idle2 会话 radius 67–94 的波动）。
5. **问题 5**：开包瞬间卡顿消失 → `SCOPY READY → SCOPY PANEL query` 空档降到 ~0.1s 量级
   （首次会话仍需建一次目录；之后每次开包命中缓存）。
6. 回归：站立/潜行/腾空/游泳/骑乘进包人物仍站立；随机/权重/顺序/PlayOnce idle 包行为不变；
   快速 A→B→C 换装最终回到最新装备 idle；读档/主菜单边界零崩溃。

回报物：`CharacterPanel.log` + 画面。若问题 2 仍未出现武器族查询，先看
`SCOPY PANEL phase=… form=… drawn=…` 行的 form/drawn 值——form 对而 drawn 错 = 快照未生效
（菜单事件时序问题），form 错 = 手槽读数问题，两者修复路径不同。
