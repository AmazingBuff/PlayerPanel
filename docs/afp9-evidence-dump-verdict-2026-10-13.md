# AFP9 verdict 6：换成"看图说话"——恢复摄影棚取证导出 + 每帧重泊

日期：2026-10-13。构建：`CharacterPanel-scopy-AFP9f-…`（DLL SHA-256 `039fe5f0…`，本轮上机版本）。
现场日志：[AFP9f 会话日志](diagnostics/CharacterPanel-afp9f-park-still-reverted-20261013.log)。
前几轮：[1 崩溃](afp9-verdict-2026-10-13.md)、[2 黑屏](afp9-black-panel-verdict-2026-10-13.md)、[3 取景层](afp9-f6-framing-verdict-2026-10-13.md)、[4 泊位](afp9-park-verdict-2026-10-13.md)、[5 变换层](afp9-transform-layer-verdict-2026-10-13.md)。

## 1. AFP9f 结果：写 local 也没能留住泊位

```
park-3d 23:35:51  world-t-before=(-601.7,-26.5,297.8) → after=(-569.7,-139.9,-7640.5)
park-3d 23:35:52  world-t-before=(-601.7,-26.5,293.4)   ← 一秒后又回到 spawn（X/Y 恒定，Z 缓慢下沉＝原地踩地）
R01 frame 23:36:18 center-d=0.0000 world-t-after=(2.0,-486.2,-22.0) anchor=(-0.0,-485.4,-0.0)
```

即：**写 `local` 同样被引擎每帧重推覆盖**，且用户仍报告"除斧头外位置不对"。取景本身每次都对（`center-d=0.0000`），说明问题不在取景算术，而在"这份变换能不能活到蒙皮计算/绘制"。

## 2. 本轮结论：不再靠转述，改为直接看画面

已连续六轮靠外部判读（RenderDoc 列、日志推断）定位，成本极高。本轮恢复 172984a 之前项目自带的取证导出（`src/render/evidence.cpp`，当时用于 M0 的 `proto-pass-NNN.tga`），并补齐三件硬证据：

1. **摄影棚画面导出**：绘制首帧与此后每 300 帧（上限 20 张）写 `.../SKSE/CharacterPanel/r01-panel-NNN.tga`（同步 GPU→CPU 回读，仅在诊断路径）。**下一轮由我直接读图判断，不再需要用户开 RenderDoc 转述。**
2. **蒙皮普查行**：`R01 census … geoms/skinned/property/passes | first='几何名' skin-frameID=… num-matrices=… alloc=… bone0='骨名' bone0-world-t=(…) bound-c=(…)`——把"身体几何是否进了 pass 列表、skin 是否属于本渲染帧、首骨世界坐标在哪"三件事一次给全。
3. **引用回读**：`park-3d … ref-t=(目标) ref-now=(游戏实际报告的位置) …`——settle `SetPosition` 到底有没有生效（此前只记录了目标，从未回读）。
4. **每帧重泊**：`PuppetDriver::tick` 在未暂停帧**每帧**重写泊位（v6.25 已验证的机制：该路线正是靠每 tick 重泊才让世界侧见不到克隆体），日志每 60 次打一行；`Repark_Frames=60` 的慢节奏退役。

## 3. 复测（AFP9g）

1. 装 AFP9g（DLL+PDB），进游戏 → F6 开 → 面板停 5–10 秒 → 关面板。
2. 无需 RenderDoc：日志里看三行即可——
   - `PUPPET park-3d … ref-now` 是否≈`ref-t`（引用是否真的动了）、`world-t-before` 是否保持泊位；
   - `R01 frame … center-d≈0`；
   - `R01 census … skinned/matrices/frameID`。
3. 把 `CharacterPanel.log` 回传（或直接说跑完了）——**TGA 我自己从 `.../SKSE/CharacterPanel/` 读**，据此判断"面板里到底画出了什么、身体在哪"。
4. 期望：世界侧玩家身边不再有克隆体（每帧重泊）；面板里至少能看到与 TGA 一致的画面。

## 4. 留档教训

诊断这类"变换/蒙皮谁生效"的问题，**图像证据优先于列数值**：同一轮里导出一张 2560×1440 的 TGA，比十行 RenderDoc 顶点列更快给出结论。项目在 172984a 丢掉取证导出后，M0/S2/AFP 各轮都重新付出了这个代价——现已恢复并写进诊断契约。
