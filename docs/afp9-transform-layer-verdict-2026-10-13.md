# AFP9 verdict 5：变换写错了层——`world` 会被每帧重推，蒙皮吃的是更新期的计算（已修复）

日期：2026-10-13。构建：`CharacterPanel-scopy-AFP9e-…`（DLL SHA-256 `031d180e…`，本轮上机版本）。
现场日志：[AFP9e 会话日志](diagnostics/CharacterPanel-afp9e-world-write-undone-20261013.log)。
前几轮：[1 崩溃](afp9-verdict-2026-10-13.md)、[2 面板全黑](afp9-black-panel-verdict-2026-10-13.md)、[3 取景写错层](afp9-f6-framing-verdict-2026-10-13.md)、[4 泊位只挪引用](afp9-park-verdict-2026-10-13.md)。

## 1. 用户判读与日志

- F6：**除斧头外都不显示**（斧头＝刚体挂件，随节点世界走；身体/脸/头发＝蒙皮几何，没跟）。
- 世界侧：**克隆体仍在玩家身边**。

AFP9e 的两行新证据把机制钉死：

```
park-3d 23:24:32  world-t-before=(-601.7,-26.5,297.5) → after=(-601.7,-26.5,-7697.8)   ← 我的 world 写生效了
park-3d 23:24:33  world-t-before=(-601.7,-26.5,297.6)                                  ← 1.3 秒后又被推回 spawn
R01 frame 23:25:23 center-d=0.0000 world-t-after=(0.0,-485.7,-23.1) anchor=(-0.0,-485.4,-0.0)  ← 取景也算对了
```

两条结论：
1. **该 actor 的子树每帧由根的 `local` 重新推导**：只写 `world` 会被下一次推导抹掉（泊位失效、克隆体可见），`SetPosition` 只动引用、与该推导无关。
2. **蒙皮状态在图更新（`UpdateAnimationGraphManager`）期间算好**：取景如果发生在更新之后，只有刚体挂件（斧头）跟着新世界走，蒙皮几何仍按更新期的矩阵绘制（即泊位处 → 不可见）。

## 2. 修复（AFP9f）

1. **写 `local`，不只写 `world`**：`PuppetDraw::begin_frame()` 现在同时写根的 `local`（父世界逆 × 目标世界）与推导后的 `world`/`previousWorld`，并 `recompute_subtree_worlds` 级联一次；`PuppetDriver::park()` 同样改为写根的 `local`（保留引擎给它的旋转与缩放，只换平移），因此泊位不再被推导抹掉。
2. **取景提前到图更新之前**：`begin_frame()` 在 `PuppetDriver::tick()` 之前调用（渲染器帧序：`begin_frame` → A0 → 傀儡图更新 → 姿态应用 → mark → 面板分支 → `end_frame`），让更新期的蒙皮计算就发生在摄影棚位置；`end_frame()` 在面板分支之后统一恢复（绘制、回退到复制图、面板没开三条路径都恢复）。
3. 恢复判据沿用：`world-d/world-rot/scale` 精确还原 + `worldBound` 逐字写回 + 每 120 帧一行。
4. 证据行语义更新：`park-3d … local-applied=1` 表示本次真的写了 local；`R01 frame … center-d=0.0000` 表示取景落到锚点。**注意**：面板打开期间 mark 的 `FP figure subject=puppet world-t=` 会显示摄影棚坐标（取景已就位），这是预期，不再代表傀儡在世界里的位置。

## 3. 待验证（AFP9f）

1. **世界侧**：玩家身边不再有克隆体；`park-3d` 的 `world-t-before` 第二次起应仍是泊位坐标（不再弹回 spawn）。
2. **F6 面板**：出现**完整人物**（本轮范围＝裸体＋手持武器；护甲镜像属后续关卡）、`center-d≈0`、`restore … ok=1`。
3. F7→F8 复制图路线不得回归（`SCOPY COMPOSITE … match=1`）。
4. 若身体仍缺失而刚体仍在：回传 `A5vsA4` 与 `R01 frame` 两行——那说明蒙皮状态还有第三个来源，需要按 F00 的头部/几何归属流程排查。

## 4. 留档教训

**改活 Actor 的位置要改它 `local`，不是 `world`**：引擎每帧按 `local` 重推 `world`，世界空间写入只在当帧有效（泊位/隐藏因此失效）。**而取景必须在图更新之前就位**：蒙皮状态在更新期计算，更新之后的变换只对刚体挂件有效。两条合并解释了 AFP9c–e 三轮"取景对了却看不见人"的全部现象。
