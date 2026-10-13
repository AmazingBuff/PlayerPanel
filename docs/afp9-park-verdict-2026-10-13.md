# AFP9 verdict 4：傀儡的 3D 从未离开玩家——泊位只挪了引用（已修复）

日期：2026-10-13。构建：`CharacterPanel-scopy-AFP9c-…`（DLL SHA-256 `45832724…`，本轮上机版本；AFP9d/AFP9e 未上机）。
现场日志：[AFP9c 会话日志](diagnostics/CharacterPanel-afp9c-f6-offscreen-20261013.log)。
前几轮：[1 崩溃](afp9-verdict-2026-10-13.md)、[2 面板全黑](afp9-black-panel-verdict-2026-10-13.md)、[3 F6 取景写错层](afp9-f6-framing-verdict-2026-10-13.md)。

## 1. 用户两项判读与日志互证

1. RenderDoc：**只有斧头与眼影位置正确，其余部位全错**（`SV_POSITION` 一处 NDC≈(-0.07,0.10) 且 z/w=0.97，另一处 NDC≈(123,134)）。
2. 游戏内：**克隆体一直在玩家身边，没有被 park 到 -8000**。

日志钉死了同一个机制：

```
SCOPY PUPPET parked reason=grace-complete offset=-8000      ← 引用位置确实被挪了
SCOPY PUPPET parked reason=keep-parked offset=-8000         ← 且每 60 帧重复
SCOPY FP figure subject=puppet world-t=(-601.7,-26.5,297.1) fade=1.000   ← 但 3D 根世界坐标仍在玩家脚下
```

**引用被挪到 -8000，3D 根却 20 秒（多次未暂停帧）都没跟过去**：该 actor 的 3D 不会从泊位引用重新推导。`park()` 随后把 fade 恢复成 1.0，于是玩家身边出现一个**完全可见**的克隆体。这同时解释了 RenderDoc 的散乱——整图从未离开玩家处，各部位在世界空间里被菜单相机随意投影，位置自然无规律。

## 2. 修复（AFP9e）

1. `PuppetDriver::park()`：除 `SetPosition` 外，**把 3D 子树按同一世界偏移下移**（`translate_subtree(root, 2, dz)`，世界空间 delta 向下写，与取景同一条已被验证的机制）。`dz` 由**目标绝对位置减当前世界位置**算出，因此每 60 帧的重复泊位不会叠加（自动幂等，也能自愈引擎后续可能的挪动）。
2. 新增证据行，一眼判读引用与 3D 是否一致：
   `SCOPY PUPPET park-3d reason=… ref-t=(…) world-t-before=(…) world-t-after=(…) moved-z=-8000.0`；第二次起应为 `moved-z=0.0`。
3. AFP9d 已完成的取景修复（世界空间 delta + `center-d` 断言）对"3D 在玩家处"或"3D 在泊位"两种状态都成立——它按当前世界算相对 delta；两者合并后：世界中傀儡在泊位（不可见），面板里傀儡被搬到摄影棚锚点。

## 3. 待验证（AFP9e 上机）

1. **世界侧**：进游戏后玩家身边不再有克隆体；`park-3d … moved-z=-8000`（首次）与后续 `moved-z=0.0`。
2. **F6 面板**：出现人物、`SCOPY R01 frame … center-d≈0`、`world-t-before` 应≈泊位坐标（-8000 量级）而 `world-t-after`≈`anchor`。
3. `A5vsA4 max-rot≈0`、`restore … ok=1`、关面板回世界无残留。
4. F7→F8 复制图路线不得回归（`SCOPY COMPOSITE … match=1`）。

## 4. 留档教训

**泊位/隐藏必须同时管引用与 3D**：只挪引用不会让图离开世界（引擎不会为这个被钉桩的 actor 重推 3D），而 fade 一旦恢复成 1.0 就变成可见克隆体。凡"把 actor 移出视野"的实现，都要打印引用位置与 3D 世界的对照行，并保证重复调用幂等。
