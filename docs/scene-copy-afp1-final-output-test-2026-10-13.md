# AFP1 测试单（2026-10-13，同 Actor 最终输出只读取证）——**待上机**

- 构建身份：`CharacterPanel-scopy-AFP1-6764d4e1d5+dirty-cl94faaed0c6-20261012T134954Z`
  （基线 = 提交 `6764d4e1d5` "actor clone reback"，其上为未提交的本轮改动）。
- 包：`dist/CharacterPanel-scopy-AFP1-1.2.1.zip`，DLL SHA-256 `07bb8ff4…`
  （全值 `07bb8ff4f43a9590847d738524fe464500cb89550ecb0afbe21e5312a8fb3297`），
  **DLL 与 PDB 已从最终 zip 部署到 `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\` 并校验一致**。
  单测 `snapshot_transform` 1/1 通过。
- 回滚：HKX44 的 DLL／PDB／manifest 仍在 `dist/CharacterPanel-scopy-HKX44/`（SHA-256 `35108e71…`）。
- 依据：[PRD 0.9 §6.6](player-panel-prd.md)、[同 Actor 最终姿态测试单](actor-final-pose-validation-2026-10-12.md) §2 R00。

## 1. 本轮是什么、不是什么

**是**：§6.6 路线的第一个仪器轮。R00 要求"解释不了就先调查，不发盲写骨架的上机包"，而这条路线
押在两件从未测过的事上：

1. 行为图自己的输出缓冲（`BShkbAnimationGraph::generatorOutputs[0]/[1]`）在**我们手动推图**之后，
   到底有没有可读的**最终姿态**，两个缓冲哪个是当前帧的；
2. 该姿态的**索引语义**——引擎自己的 `boneNodes` 次序，还是动画骨架次序。正常游玩时引擎把它应用
   到了节点上，手动路线必须复现同一件事。

**不是**：本轮不写骨架、不摘图、不 Disable、不重置 T-pose、不取 clip 重采样、不跑经验求解器、
不改显示。F11 的每一次标记都是纯读：输出缓冲、节点 local、计数。一个假设错了就打印数字，不动骨架。

## 2. 实现（新增／改动）

| 位置 | 内容 |
| --- | --- |
| `src/character/final_pose_probe.{h,cpp}`（新） | 取证仪器：`read_pose_track()` 逐级校验（输出对象 → tracks 主头 → 轨道头 → 元素尺寸 0x30 → 数据跨度），失败给出**具体 reason**；每标记一份姿态副本（普通浮点，`memcpy` 读，避免错位向量读）＋节点 local 摘要＋两候选次序标定；`on-tracks=[…]` 顺带列出该缓冲当前活着的标准轨（pose 之外还看得到 world-from-model／extracted motion，为后面根运动留数据） |
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
| 5 | **索引语义（R03 前置）** | 世界期 F11 的 `verdict best=<order>/slot<N>` 且 `ratio` 接近 1、`nodes-moved>0` | `nodes-moved=0` = 该 Actor 节点没被驱动（泊深 cull？），标定性无效，回传 |
| 6 | **两个缓冲的关系** | 记录两个 slot 各自的 `changed`／ratio，判出"当前帧用哪个" | 都不动/都不匹配 = 输出缓冲与节点不是同源，本轮结论填此 |
| 7 | **零崩溃零回归** | 全程无 crash、零新 `[E]` | 立即回传 crash + PDB + 动作序列 |

**这张表决定什么**：第 1/2 条决定路线是否成立（拿不到最终输出就没有"应用"可言）；第 5 条给出
**写骨架时必须用的次序与缓冲**——下一轮（AFP2：手动应用 + 直绘 P 自己的 3D）直接照抄它，
不再猜。

## 5. 回传

完整 `CharacterPanel.log`、`build-manifest.json`、puppet 出生行与全部 `SCOPY FP` 块（世界期与暂停期
分开标注每次 F11 的时机）、以及第 5 条 `verdict` 行。崩溃则附 CrashLogger 日志与本包 PDB。
