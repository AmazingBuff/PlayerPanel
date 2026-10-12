# AFP2 控制与观测契约（2026-10-13）

- 性质：**R00 要求的那份"发包前的实现证据与控制清单"**（[测试单 §2](actor-final-pose-validation-2026-10-12.md)、PRD 0.9 §6.6）。
  本文是 AFP2 包**必须实现的行为契约**，不是测试步骤的复述；键位与行名以本文为准，实现完成后写进包内 runbook 与 manifest。
- 前提状态：`已定` = 已有实测支撑；`待判` = 由 AFP1b 会话（F4 放置实验 + `camera` 行）决定，未定之前不实现对应分支。

## 1. 控制清单（AFP2 包）

**键位预算 = 三个：F2／F3／F4**（用户定：本机没有更多可用键）。一个游戏会话只装一个 DLL，AFP 路线占用这三个键；
旧复制路线的 F7（重新捕获，顺带释放旧副本）与 F8（绘制副本）保留，其旧的 F2 待机／F3 旋转／F4 释放绑定**让位**。

| 行为 | 键／形态 | 精确语义（R00 要求逐条对应） | 日志 | 状态 |
| --- | --- | --- | --- | --- |
| 标记／逐阶段采样 | **F2** | 一次只读取证：AFP1 全套行 ＋ A0／A0A1／A3 阶段行；每按一次一个 mark 号。**不改变任何状态** | `SCOPY FP mark=…` | 已实现 |
| 推图 开／关 | **F3**（循环态 `tick`） | 只控制面板对 P 行为图的 `UpdateAnimationGraphManager` 调用；不动 UI 暂停；关时保持时钟新鲜（解冻只走一步） | `SCOPY PUPPET tick on|off` | 已实现 |
| 最终姿态应用 开／关 | **F3**（循环态 `tick+apply`） | 控制"图输出 → P 骨架节点 local"的写入。**关**（`tick` 态）：继续推图但冻结在最后一次节点结果，不恢复快照/绑定姿态；**开**：每帧按 `decide_pose_order` 的结论写入或阻断并点名理由 | `SCOPY AFP2 apply on|off gated=<reason>` ＋ A2 行 | 待判(placement) |
| 冻结全部／继续 | **F3**（循环态 `off`／退出 `off`） | `off` = 推图关 + 应用关，不写新姿态、手动推图计数停止；离开 `off` 即继续，时钟从当前时刻续，**不补**冻结期间的时间 | `SCOPY AFP2 drive=off|tick|tick+apply|step` | 部分（off/tick 已有） |
| 单步 | **F3**（循环态 `step`，一次性） | 推进**一个已声明 dt**（`AFP2_Step_Seconds = 1/60`）并执行**一次**应用，然后自动回到 `off`；连续按即连续单步 | `SCOPY AFP2 step dt=0.0167 index=K` ＋ A1/A2 行 | 待判(placement) |
| 放置模式 | **F4** | `parked-deep` ↔ `held-pinned`（站玩家处、fade 钉 0.05、留在引擎动画作用域内）；切换必经一次装配窗 | `SCOPY PUPPET placement=…` | 已实现 |
| 准备／显示 P | 自动（**不占键**） | 出生：读档边界在玩家处装配（v6.70 配方），随后按放置模式处置；显示：面板打开即绘制 P 的 3D（关闭背包即隐藏），**不改变 P 的世界状态、不摘图、不 Disable** | `SCOPY AFP2 figure draw=on|off root=0x… gen=…` | 待判(取景) |
| 相机取景／旋转 | **构建期常量预设**（不占键） | 取景是常量而非常按键：`AFP2_View_Preset`（近/中/远 × 正面/侧 45°）。R07 要对照取景时改预设重编并记进 manifest，**不为此再占一个键** | `SCOPY AFP2 camera preset=… angle=…` | 待判(取景) |
| 状态／装备请求 | 自动镜像（不占键） | 只操作 P 的展示状态；镜像 O 的实际装备时**只读** O，由游戏正常装备装配 P，不写 O 的图（HKX39 的轮询杠杆） | `SCOPY PUPPET equip …` | 部分已实现 |
| 隐藏／实际销毁 | 关卡自带（不占键） | R09 要的不是按键而是**停止请求与回收**：停止 tick／应用／物理 → 等任务完成 → 按引擎合法顺序解除引用；"隐藏"在 AFP2 里等于关闭面板（不绘制） | `SCOPY AFP2 destroy requested|done` | 待 R09 |
| 物理开关 | 关卡自带（不占键） | R08 分别控制 P 的 CBPC／SMP 与碰撞，不解除全局暂停 | `SCOPY AFP2 physics cbpc=… smp=…` | 未实现 |

**取舍说明（写进 manifest，不装作有）**：R00 要求"测试包必须提供的行为"，在三个键的预算下，本包提供的**手动**控制
只有三个（F2 标记／F3 驱动循环／F4 放置），其余行为要么**自动**（出生、显示、装备镜像）、要么是**构建期常量**
（取景预设、dt、阈值）、要么**属于各自关卡**（R08 物理、R09 回收）。R00 的要求是"能解释清楚且不静默"，
不是"每个行为一个键"——少而写明的控制面胜过装作齐全的控制面。

## 2. A0–A5 观测点（每标记一组，行名固定）

| 点 | 记录内容 | 实现位置 |
| --- | --- | --- |
| **A0 推图前** | frame／update 序号、P 与根／generation、UI paused、累计时间、输出摘要（`generatorOutputs` 两槽的身份与 `num-data`）、节点当前姿态摘要 | **已实现**：渲染括号内、驱动 tick **之前**（`FinalPoseProbe::sample_before_tick`），打 `SCOPY FP A0` |
| **A1 求值后** | 最终输出阶段／数组身份、骨表身份、内容副本与摘要、活动 clip／权重／相位（**仅诊断，clip 不是显示输入**） | **已实现**：tick 之后的全套 `SCOPY FP` 报告 ＋ **`A0A1` 帧内配对**（同一帧 A0 vs A1；`pose0/1`、`nodes-moved`、`nodes-rebound`） |
| **A2 应用后** | 逐骨 local／world、helper／根处理、应用与缺失骨数量、必要后代与包围盒、`decide` 结论 | apply 阶段内部（写入前后各一读） | 
| **A3 物理后** | 后端状态／任务完成、物理输出节点；首轮物理关闭时明确写 `skipped` | **已实现**：每个标记打 `SCOPY FP A3 physics=skipped reason=first-round-physics-off` |
| **A4 pass 前** | 同一节点与实际 geometry／skin／palette、图与 generation 是否仍有效、覆盖与路由 | `draw_figure_passes` 的 `before_pass` 回调（缝已就位，等 Actor 直绘接线） |
| **A5 draw 后** | 实际 draw 数、蒙皮准备后的观测、画面时间标记、图与渲染状态恢复结果 | `draw_figure_passes` 的 `after_batch` 回调（同上） |

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
| 泊深傀儡为何输出为空 | AFP1b：F4 → `held-pinned` 后 `num-anim-bones`／`num-data` 是否变 116 | 应用阶段的目标与前提；若 held 仍为 0，先查"自己驱动 generate"再谈应用 |
| 投影读哪个相机 | AFP1b：`camera` 行 `accumulator-is-ui3d` | 取景走"相机适配"（— 方案）还是"draw 窗口内临时根变换并恢复" |
| P 的世界隐藏用什么 | AFP2 设计输入 §6.0 三条出路 | — 的"隐藏"语义、R07 的世界副作用证据 |
| P 的几何是否有设备缓冲 | AFP1b：`geoms` 行 `renderer-data` | 直绘前是否需要先让引擎渲染过 P（runs 49-52 的死因） |
