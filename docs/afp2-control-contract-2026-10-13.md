# AFP2 控制与观测契约（2026-10-13）

- 性质：**R00 要求的那份"发包前的实现证据与控制清单"**（[测试单 §2](actor-final-pose-validation-2026-10-12.md)、PRD 0.9 §6.6）。
  本文是 AFP2 包**必须实现的行为契约**，不是测试步骤的复述；键位与行名以本文为准，实现完成后写进包内 runbook 与 manifest。
- 前提状态：`已定` = 已有实测支撑；`待判` = 由 AFP1b 会话（NUM1 放置实验 + `camera` 行）决定，未定之前不实现对应分支。

## 1. 控制清单（AFP2 包）

一个游戏会话只装一个 DLL；AFP2 包**关闭**旧复制路线的 F7/F8/F3/F4/F2 语义（复制图不再是显示体），键位重排如下。

| 行为 | 键 | 精确语义（R00 要求逐条对应） | 日志 | 状态 |
| --- | --- | --- | --- | --- |
| 准备／显示 P | 无（自动）+ **F8** | 出生：读档边界在玩家处装配（v6.70 配方），随后按放置模式处置；**F8 只切换"是否把 P 的 3D 画进摄影棚目标"**，不改变 P 的世界状态、不摘图、不 Disable | `SCOPY AFP2 figure draw=on|off root=0x… gen=…` | 待判(取景) |
| 推图 开／关 | **F12** | 只控制面板对 P 行为图的 `UpdateAnimationGraphManager` 调用；不动 UI 暂停；关时保持时钟新鲜（解冻只走一步） | `SCOPY PUPPET tick on|off` | 已实现 |
| 放置模式 | **NUM1** | `parked-deep` ↔ `held-pinned`（站玩家处、fade 钉 0.05、留在引擎动画作用域内）；切换必经一次装配窗 | `SCOPY PUPPET placement=…` | 已实现 |
| 最终姿态应用 开／关 | **NUM2** | 控制"图输出 → P 骨架节点 local"的写入。**关**：继续推图但冻结在最后一次节点结果，不恢复快照/绑定姿态；**开**：每帧按 `decide_pose_order` 的结论写入或阻断 | `SCOPY AFP2 apply on|off gated=<reason>` ＋ A2 行 | 待判(placement) |
| 冻结全部 | **NUM3** | 推图关 + 应用关（两者同时），不写新姿态；手动推图计数停止 | `SCOPY AFP2 freeze all` | 待判 |
| 单步 | **NUM4** | 只推进**一个已声明 dt**（`AFP2_Step_Seconds = 1/60`）并执行**一次**应用；连续按即连续单步 | `SCOPY AFP2 step dt=0.0167 index=K` ＋ A1/A2 行 | 待判 |
| 继续 | **NUM5** | 解除冻结（推图开 + 应用开），时钟从当前时刻续，不补冻结期间的时间 | `SCOPY AFP2 resume` | 待判 |
| 标记／逐阶段采样 | **F11** | 一次只读取证（现有 AFP1 全套行）＋ A0–A5 阶段行；每按一次一个 mark 号 | `SCOPY FP mark=…` | 已实现（阶段行待补） |
| 相机取景／旋转 | **NUM6** | **只改相机**：在 UI3D 相机上循环"距离 × 角度"预设（近/中/远 × 正面/侧 45°），每帧在 draw 窗口内应用、出口恢复；**不改变 P 的模拟坐标与骨架尺度** | `SCOPY AFP2 camera preset=near|mid|far angle=0|45` | 待判(取景) |
| 隐藏／实际销毁 | **NUM0** | 两段反馈：第一次 = 停止 draw（隐藏，P 仍在世界里受管）；第二次 = 请求停止 tick／应用／物理，等任务完成后按引擎合法顺序解除引用（R09） | `SCOPY AFP2 hide` / `SCOPY AFP2 destroy requested|done` | 待 R09 |
| 状态／装备请求 | **NUM7** | 只操作 P 的展示状态；镜像 O 的实际装备时**只读** O，由游戏正常装备装配 P，不写 O 的图 | `SCOPY AFP2 equip request=… pushed=…` | 部分（HKX39 杠杆已有） |
| 物理开关 | 后续（R08） | 分别控制 P 的 CBPC／SMP 与碰撞，不解除全局暂停 | `SCOPY AFP2 physics cbpc=… smp=…` | 未实现 |

## 2. A0–A5 观测点（每标记一组，行名固定）

| 点 | 记录内容 | 实现位置 |
| --- | --- | --- |
| **A0 推图前** | frame／update 序号、P 与根／generation、UI paused、累计时间、输出摘要（`generatorOutputs` 两槽的身份与 `num-data`）、节点当前姿态摘要 | 标记处理的开头（`PuppetDriver::tick` 之前） |
| **A1 求值后** | 最终输出阶段／数组身份、骨表身份、内容副本与摘要、活动 clip／权重／相位（**仅诊断，clip 不是显示输入**） | 手动 tick 之后、应用之前 |
| **A2 应用后** | 逐骨 local／world、helper／根处理、应用与缺失骨数量、必要后代与包围盒、`decide` 结论 | apply 阶段内部（写入前后各一读） |
| **A3 物理后** | 后端状态／任务完成、物理输出节点；首轮物理关闭时明确写 `skipped` | R08 之前固定打 `skipped` |
| **A4 pass 前** | 同一节点与实际 geometry／skin／palette、图与 generation 是否仍有效、覆盖与路由 | `draw_figure_passes` 的 `before_pass` 回调 |
| **A5 draw 后** | 实际 draw 数、蒙皮准备后的观测、画面时间标记、图与渲染状态恢复结果 | `draw_figure_passes` 的 `after_batch` 回调 ＋ 收尾 |

**A2→A4 的一致性只证明"输出没被覆盖"**，不证明动画来源正确——动画正确性另由 R03 的正常引擎参考与 R05 的状态／混合验收（测试单原文），这条不许在本包里被简化。

## 3. 声明常量（发包前定死，不事后改阈值）

| 常量 | 值 | 含义 |
| --- | --- | --- |
| `Pose_Match_Degrees` | 1.0° | 一根骨"被复现"的旋转门槛（HKX28 沿用） |
| `Pose_Match_Min_Compared` | 8 | 至少多少根骨可比才允许写入 |
| `Pose_Match_Min_Ratio` | 0.9 | 可比骨中至少多少比例落在门槛内 |
| `AFP2_Step_Seconds` | 1/60 s | 单步声明的 dt（与补步上限一起写进 manifest） |
| `AFP2_Max_Catchup_Steps` | 4 | 失焦／重新打开后的补步上限 |
| 位移／缩放 | 只记录不设门 | 姿态三缩放 vs NIF 单缩放，系统性偏移属空间事实（AFP1 已确立） |

## 4. 未决项（决定哪些分支能实现）

| 未决 | 由什么回答 | 决定什么 |
| --- | --- | --- |
| 泊深傀儡为何输出为空 | AFP1b：NUM1 → `held-pinned` 后 `num-anim-bones`／`num-data` 是否变 116 | 应用阶段的目标与前提；若 held 仍为 0，先查"自己驱动 generate"再谈应用 |
| 投影读哪个相机 | AFP1b：`camera` 行 `accumulator-is-ui3d` | 取景走"相机适配"（NUM6 方案）还是"draw 窗口内临时根变换并恢复" |
| P 的世界隐藏用什么 | AFP2 设计输入 §6.0 三条出路 | NUM0 的"隐藏"语义、R07 的世界副作用证据 |
| P 的几何是否有设备缓冲 | AFP1b：`geoms` 行 `renderer-data` | 直绘前是否需要先让引擎渲染过 P（runs 49-52 的死因） |
