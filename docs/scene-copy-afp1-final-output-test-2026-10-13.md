# AFP1 测试单（2026-10-13，同 Actor 最终输出只读取证）——**待上机**

**两个包，别混**（2026-10-13，游戏正在运行中发现 DLL 被占用）：

| | 上机中的包（**当前部署，正在跑的就是它**） | 下一个包（已构建，**等游戏退出后部署**） |
| --- | --- | --- |
| 构建身份 | `CharacterPanel-scopy-AFP1-6764d4e1d5+dirty-cl94faaed0c6-20261012T134954Z` | `CharacterPanel-scopy-AFP1-8d6037c573+dirty-cl94faaed0c6-20261012T135708Z` |
| DLL SHA-256 | `07bb8ff4…`（全值 `07bb8ff4f43a9590847d738524fe464500cb89550ecb0afbe21e5312a8fb3297`） | `5fa5d1f2…`（全值 `5fa5d1f2cd5d39c3a2ecfca62fa9afd781b992cf2211f40c7225f0949d2a30ea`） |
| 日志格式 | `verdict` 行**不含** `match=`/`reason=`；**没有** `camera`/`figure` 行 | `verdict` 行含 `match=`/`reason=`；新增 `camera`/`figure` 两行 |
| 本轮会话怎么用 | 判读表第 **1–6、9** 条（最终输出能否拿到、次序语义、冻结、零崩溃）——这些它都能答 | 判读表第 **7–8** 条（显示侧取景机制、傀儡在图里的位置）**要它**；部署后不用重开会话，再按几次 F11 即可 |

- 包：`dist/CharacterPanel-scopy-AFP1-1.2.1.zip`（打包脚本每次覆盖同名 zip，目录 `dist/CharacterPanel-scopy-AFP1/` 里的就是最新的 `5fa5d1f2`）。
  `07bb8ff4` 的 DLL／PDB 已在 `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\` 并校验一致；**`5fa5d1f2` 尚未部署**（DLL 被运行中的游戏占用），游戏退出后再覆盖并校验。
- 单测 `snapshot_transform` 1/1 通过（含本轮新增的次序判定用例：移位次序被拒、骨数不足被拒、空候选被拒、比率并列取可比更多者）。
- 回滚：HKX44 的 DLL／PDB／manifest 仍在 `dist/CharacterPanel-scopy-HKX44/`（SHA-256 `35108e71…`）。
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
不改显示。F11 的每一次标记都是纯读：输出缓冲、节点 local、相机与取景、计数。一个假设错了就打印
数字，不动骨架。

## 2. 实现（新增／改动）

| 位置 | 内容 |
| --- | --- |
| `src/character/final_pose_probe.{h,cpp}`（新） | 取证仪器：`read_pose_track()` 逐级校验（输出对象 → tracks 主头 → 轨道头 → 元素尺寸 0x30 → 数据跨度），失败给出**具体 reason**；每标记一份姿态副本（普通浮点，`memcpy` 读，避免错位向量读）＋节点 local 摘要；`on-tracks=[…]` 顺带列出该缓冲当前活着的标准轨（pose 之外还看得到 world-from-model／extracted motion，为后面根运动留数据） |
| 同上（纯规则，可离线测） | `score_pose_order()` / `decide_pose_order()` 为**头文件内联的纯函数**：给定姿态副本与两候选表（`NodeLocal`，含 `present` 位——缺席的索引必须留位而不是被丢掉，否则整张表会错位），打分并判定 `match/reason`。门槛 `Pose_Match_Degrees=1.0`、`Pose_Match_Min_Compared=8`、`Pose_Match_Min_Ratio=0.9` 全是发包前定死的常量。**AFP2 的写入闸门就是同一个函数**，不是另写一份 |
| 同上（显示侧取证） | `SCOPY FP camera` / `figure` 两行：累加器启动时用的相机（`NiAccumulator::camera`）与 `ui3d->camera`／`world-camera` 的指针同一性、`NiCamera` 的 `worldToCam` 第 3 行／投影视锥、UI3D 缓存的相机位姿，以及被摄体 3D 根的 `world-t`／沿视轴深度／离轴距离／`fade`／包围球。**全是读**，用来判定 AFP2 的取景该搬相机还是搬根 |
| `src/character/puppet_driver.{h,cpp}` | `actor()`（给仪器用）、`tick_enabled()/toggle_tick()`（R02 的"推图开／关"）、`manual_ticks()`；冻结时保持时钟新鲜（解冻只走一步，不补步） |
| `src/input/input.cpp` | **F11** = 标记；**F12** = 推图开／关（F5 是游戏自己的快速存档，第一版选错，已改） |
| `src/render/renderer.cpp` | 渲染括号内、`PuppetDriver::tick()` 之后消费标记（样本描述的是本帧将要绘制的那一帧） |

标定判据**发包前定死**（R00）：旋转 ≤ **1.0°** 记 reproduces（沿用 HKX28 门槛），位移只作观测不设门。

## 3. 上机步骤

1. 确认部署：DLL SHA-256 `07bb8ff4…`；日志首行 `SCOPY BUILD …-AFP1-…`。
2. 进存档 → **世界期**（未暂停）先按 **F11**：这是参考组，引擎自己驱动傀儡与玩家的图与节点。
   `SCOPY PUPPET born …` 应已出现（HKX38 配方，读档边界出生）。
3. 开背包（暂停）→ **F11** 建立基线 → 等 3 秒（F12 默认 on）→ **F11**：看 `delta slot=… changed=`
   与 `nodes … changed=`。
4. **F12** 关推图 → **F11** → 等 3 秒 → **F11**：两处 delta 应归零（冻结的是图，不是藏起来）。
5. 归档日志 `docs/diagnostics/CharacterPanel-afp1-final-output-20261013.log`（日志非追加，测完立刻复制）。

## 4. 判读表

| # | 检查 | 通过判据 | 失败/信息不足时 |
| --- | --- | --- | --- |
| 1 | **最终输出可读** | 至少一个 slot `reason=ok readable=1 valid=1 type=1 elem=48 num-data≈骨架骨数` | `reason=` 点名断在哪一级；两个 slot 都不可读 = 这条路线的输入不存在，回传全部 slot 行 |
| 2 | **可读但空** | `valid=0 / on=0.000`（轨道 off）或 `num-data` 远小于骨架骨数 | 也是有效答案：说明手动推图没走到求值阶段，转去查求值入口 |
| 3 | **手动推图确实推进输出** | 暂停期两次 F11，`delta slot=… changed>0`（姿态副本在动） | `changed=0` 且 `manual-ticks` 在涨 = HKX42 同源（图在跑、输出不更新），回传 |
| 4 | **F12 冻结成立** | F12 off 后两处 delta 全 0，`manual-ticks` 停涨 | 仍在动 = 有别的写入者在推图，回传 |
| 5 | **索引语义（R03 前置）** | 世界期 F11 的 `verdict` 行 `match=1 reason=matched`、`ratio` 接近 1、`nodes-moved>0` | `nodes-moved=0` = 该 Actor 节点没被驱动（泊深 cull？），标定性无效，回传 |
| 6 | **两个缓冲的关系** | 记录两个 slot 各自的 `changed`／ratio，判出"当前帧用哪个" | 都不动/都不匹配 = 输出缓冲与节点不是同源，本轮结论填此 |
| 7 | **显示侧取景机制（AFP2 前置）**〔下一包〕 | `camera` 行 `accumulator-is-ui3d=1`：生成摄影棚 pass 的累加器就是用 UI3D 相机启动的 → 直绘路线走"相机适配"（`SetCameraPosition/Rotate`），不必每帧搬 Actor | `=0`：记录 `accumulator-camera` 与 `ui3d-camera` 两个指针原值，AFP2 改为在 draw 窗口内临时改根变换并恢复 |
| 8 | **傀儡在图里的实际位置**〔下一包〕 | `figure` 行给出 `world-t`／`view-depth`／`lateral`／`eye-distance`／`fade` | `fade=0` = 引擎会跳过该节点（相机适配前必须先解掉）；`lateral` 很大 = 泊位与取景完全脱节。两者都是 AFP2 的设计输入 |
| 9 | **零崩溃零回归** | 全程无 crash、零新 `[E]` | 立即回传 crash + PDB + 动作序列 |

**这张表决定什么**：第 1/2 条决定路线是否成立（拿不到最终输出就没有"应用"可言）；第 5 条给出
**写骨架时必须用的次序与缓冲**；第 7/8 条给出**显示侧怎么取景**（搬相机还是临时搬根）。AFP2
（手动应用 + 直绘 P 自己的 3D）照抄这三条结论，不再猜。

## 5. 回传

完整 `CharacterPanel.log`、`build-manifest.json`、puppet 出生行与全部 `SCOPY FP` 块（世界期与暂停期
分开标注每次 F11 的时机）、以及第 5 条 `verdict` 行。崩溃则附 CrashLogger 日志与本包 PDB。
