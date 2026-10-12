# AFP1 测试单（2026-10-13，同 Actor 最终输出只读取证）

> **判读已出（首轮，21:54 会话）：见 [AFP1 判读](afp1-verdict-2026-10-13.md)。**
> 三句话：**最终输出读得到**（pose 轨 `reason=ok`、116 元素、0x30 步长），**次序 = 动画骨架**（105/109 骨 ≤1°，
> boneNodes 序只有 45/106），**但泊深傀儡的输出是空的**（`num-data=0`、`num-anim-bones=0`，2103 次手动推图只动了
> 2 根骨）。为此本轮加了 `F4` 放置模式杠杆，下面第 7 条改由它回答。

**三个包，别混**：

| | 已上机（判读已出） | 已构建未上机（**当前部署的就是它**） |
| --- | --- | --- |
| 构建身份 | `CharacterPanel-scopy-AFP1-6764d4e1d5+dirty-cl94faaed0c6-20261012T134954Z` | `CharacterPanel-scopy-AFP1-c15b56ec0f+dirty-cl94faaed0c6-20261012T143622Z` |
| DLL SHA-256 | `07bb8ff4…` | `669c6102…`（全值 `669c61028c3d43dfd4c0073b25d51f30e3a46633d0725fbf70abd24ec02ccd34`） |
| 多了什么 | —— | `camera`/`figure`/`geoms` 三行、`verdict match=/reason=`、`reason=pose-empty` 独立化、`placement=` 与 **F4** 杠杆、`order=` 行补 `beyond`/`max-scale`/`outliers=`、`world` 行、**`A0`/`A0A1`/`A3` 帧内配对**、**`necessary` 行**、**census 行加 `gen=`/`root=`（R02/R06 要的 3D generation）**；直绘缝重构与默认配置编译修复 |

> **判读风险与回滚**：直绘缝重构动了**当前正在用的面板绘制路径**，而它只能在游戏里验证。两点降低风险：
> ①AFP1b 要的那些数（`SCOPY FP` 全部行、`SCOPY PUPPET` 放置）**不经过绘制路径**，即使面板画不出来也不影响本轮判读；
> ②若面板异常，日志里的 `Clone draw: passes=… rd-null=…`／`Clone draw OM check: … match=` 会直接说明是"零 pass"
> 还是"被改道"，回滚 = `git checkout af0aa68 -- src/character/character_clone.cpp` 后重新构建（源在 git，无需旧二进制）。

- 包：`dist/CharacterPanel-scopy-AFP1-1.2.1.zip`（每次打包覆盖同名 zip）。`c6d5e6f6` 的 DLL／PDB 已在
  `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\` 并校验一致。
- 单测 `snapshot_transform` 1/1 通过（含次序判定用例：移位次序被拒、骨数不足被拒、空候选被拒、比率并列取更多者）。
- 回滚：HKX44（`35108e71…`）与已上机的 AFP1 首版（`07bb8ff4…`）的 DLL／PDB／manifest 都在 `dist/` 下。
- 依据：[PRD 0.9 §6.6](player-panel-prd.md)、[同 Actor 最终姿态测试单](actor-final-pose-validation-2026-10-12.md) §2 R00。

## 1. 本轮是什么、不是什么

**是**：§6.6 路线的第一个仪器轮。R00 要求"解释不了就先调查，不发盲写骨架的上机包"，而这条路线
押在三件从未测过的事上：

1. 行为图自己的输出缓冲（`BShkbAnimationGraph::generatorOutputs[0]/[1]`）在**我们手动推图**之后，
   到底有没有可读的**最终姿态**，两个缓冲哪个是当前帧的；
2. 该姿态的**索引语义**——引擎自己的 `boneNodes` 次序，还是动画骨架次序。正常游玩时引擎把它应用
   到了节点上，手动路线必须复现同一件事；
3. **显示侧取景靠什么**：pass 提交（`SetupAndDrawPass`）不带相机也不写投影，所以"不许每帧搬 Actor"
   的路线只能去改那个真正生效的相机——先读清楚累加器是用哪个相机启动的（`camera` 行），
   以及被摄体现在在图里的什么位置（`figure` 行）。

**不是**：本轮不写骨架、不摘图、不 Disable、不重置 T-pose、不取 clip 重采样、不跑经验求解器、
不改显示。F2 的每一次标记都是纯读：输出缓冲、节点 local、相机与取景、计数。一个假设错了就打印
数字，不动骨架。

## 2. 实现（新增／改动）

| 位置 | 内容 |
| --- | --- |
| `src/character/final_pose_probe.{h,cpp}`（新） | 取证仪器：`read_pose_track()` 逐级校验（输出对象 → tracks 主头 → 轨道头 → 元素尺寸 0x30 → 数据跨度），失败给出**具体 reason**（`pose-empty` 单独一条：轨在、元素为 0）；每标记一份姿态副本（普通浮点，`memcpy` 读，避免错位向量读）＋节点 local 摘要；`on-tracks=[…]` 顺带列出该缓冲当前活着的标准轨 |
| 同上（纯规则，可离线测） | `score_pose_order()` / `decide_pose_order()` 为**头文件内联的纯函数**：给定姿态副本与两候选表（`NodeLocal`，含 `present` 位——缺席的索引必须留位而不是被丢掉，否则整张表会错位），打分并判定 `match/reason`。门槛 `Pose_Match_Degrees=1.0`、`Pose_Match_Min_Compared=8`、`Pose_Match_Min_Ratio=0.9` 全是发包前定死的常量。**AFP2 的写入闸门就是同一个函数**，不是另写一份 |
| 同上（显示侧取证） | `SCOPY FP camera` / `figure` 两行：累加器启动时用的相机（`NiAccumulator::camera`）与 `ui3d->camera`／`world-camera` 的指针同一性、`NiCamera` 的 `worldToCam` 第 3 行／投影视锥、UI3D 缓存的相机位姿，以及被摄体 3D 根的 `world-t`／沿视轴深度／离轴距离／`fade`／包围球。**全是读** |
| `src/character/puppet_driver.{h,cpp}` | `actor()`、`tick_enabled()/toggle_tick()`（R02 的"推图开／关"）、`manual_ticks()`；**`cycle_placement()`/`placement()`**——`parked-deep` ↔ `held-pinned`（站在玩家处、fade 钉 0.05、留在引擎动画作用域内），这是首轮判读逼出来的唯一变量 |
| `src/input/input.cpp` | **F2** = 标记；**F3** = 推图开／关；**F4** = 放置模式切换（F5 是游戏自己的快速存档，第一版选错，已改） |
| `src/render/renderer.cpp` | 渲染括号内、`PuppetDriver::tick()` 之后消费标记（样本描述的是本帧将要绘制的那一帧） |

标定判据**发包前定死**（R00）：旋转 ≤ **1.0°** 记 reproduces（沿用 HKX28 门槛），位移只作观测不设门。

## 3. 上机步骤（第二轮：回答"空输出是不是泊位造成的"）

1. 确认部署：DLL SHA-256 `c6d5e6f6…`；日志首行 `SCOPY BUILD …-AFP1-…`。
2. 进存档 → **世界期**先按 **F2**（参考组）→ 再按 **F4** 切到 `held-pinned` → 等装配窗（约 4 秒）→ **F2** ×2。
3. 开背包（暂停）→ **F2** ×2（`tick=on`）→ **F3** → **F2** ×2（冻结对照）。
4. 按 **F4** 切回 `parked-deep` → **F2** ×2（同会话内的直接对照）。
5. 归档日志 `docs/diagnostics/CharacterPanel-afp1b-placement-<日期>.log`（日志非追加，测完立刻复制）。

## 4. 判读表

| # | 检查 | 首轮结果 | 失败/信息不足时 |
| --- | --- | --- | --- |
| 1 | **最终输出可读** | **通过**（玩家）：`reason=ok valid=1 type=1 elem=48 num-data=116 cap=116 off=448` | 已通过，保留 |
| 2 | **活缓冲识别** | **通过**：活的那个 `num-data>0`，但两个缓冲对象地址每标记都在变 → **不得按下标读** | —— |
| 3 | **索引语义（R03 前置）** | **通过**：`order=skeleton slot=0 within-1deg=105/109 ratio=0.96` vs `bone-nodes 45/106 ratio=0.42` → **骨架序** | 本轮已加 `outliers=`（点名超差骨，上限 8）与 `world` 行（R03 的第二个空间）——下一会话直接给出"那 4 根是谁"以及骨架序在 world 空间是否仍然成立 |
| 4 | **手动推图推进输出** | **部分**：玩家侧暂停期仍 `changed=29…55`；傀儡侧 `changed=0`（轨空） | 本轮新增 **`A0A1` 帧内配对**：它把"我们的 tick 是否就是动它的原因"从跨标记比较里分出来——暂停世界里只有一方能推动。`pose0/1` 是两槽元素数（A0 → A1），`nodes-moved`/`nodes-rebound` 是本帧节点变化（重建的树记 rebound，不记幽灵位移） |
| 5 | **F3 冻结成立** | **通过**：`manual-ticks` 停在 2103、两处 delta 归零 | —— |
| 6 | **泊深傀儡产出最终输出（阻断项）** | **不通过**：`num-anim-bones=0`、pose 轨 `num-data=0 reason=pose-empty`（2103 次推图只动 2 根骨） | **本轮 F4 → `held-pinned` 就是回答它的实验**：若 `num-data` 变 116、`num-anim-bones` 变 116 → 空输出是泊位（cull 停用图）造成的；若仍为 0 → 傀儡的图根本不产出，路线需换前提 |
| 7 | **显示侧取景机制（AFP2 前置）** | 未测（首版无该行） | `accumulator-is-ui3d=1` → 走"相机适配"；`=0` → draw 窗口内临时改根变换并恢复 |
| 8 | **傀儡在图里的实际位置** | 未测（首版无该行） | `fade`／`lateral`／`view-depth` 是 AFP2 取景设计输入 |
| 9 | **零崩溃零回归** | **通过**（6 标记、零 crash、零 `[E]`，两条已知灯光 warn） | —— |
| 10 | **R01 前置：Actor 自己的几何能不能画**〔新增〕 | `geoms` 行：`skinned>0`、`property≈skinned`、**`renderer-data>0`** | `renderer-data=0` = 设备侧几何从未创建（runs 49-52 的死因），直绘前必须先让引擎渲染过它（`held-pinned` 的作用域内装配应给出非零值）；`effect` 非零 = HKX34 那类特效几何，直绘前需按类剪枝 |
| 11 | **R04 前置：超差骨是否阻断**〔新增〕 | `necessary` 行：`necessary=0`（超差的全是 helper，可跳过）或 `necessary>0` 且能逐根解释 | `verdict=unbound-palette`（`palettes=0/N`）= 该 Actor 的调色板从未绑定（引擎没画过它）→ 分类不可用，**不能**当作"没有必要骨"；`necessary>0` = 按 R04 必须阻断样本并点名 |

**这张表决定什么**：第 1–3 条已给出"读得到 + 按骨架序"；第 6 条是下一轮唯一要翻的石头（空输出是泊位还是图的本质），
第 7/8 条给显示侧取景。AFP2（手动应用 + 直绘 P 自己的 3D）照抄这三条结论，不再猜。

## 5. 回传

完整 `CharacterPanel.log`、`build-manifest.json`、`SCOPY PUPPET` 出生/放置行与全部 `SCOPY FP` 块
（标出每次 F2／F4／F3 的时机）。崩溃则附 CrashLogger 日志与本包 PDB。
