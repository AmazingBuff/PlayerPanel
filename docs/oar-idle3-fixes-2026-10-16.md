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
