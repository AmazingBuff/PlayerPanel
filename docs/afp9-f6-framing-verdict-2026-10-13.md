# AFP9 verdict 3：F6 直绘的“人物在画但在视锥外”——取景写错了层（已修复）

日期：2026-10-13。构建：`CharacterPanel-scopy-AFP9c-ce35ad79e3+dirty-cl94faaed0c6-20261013T150320Z`（DLL SHA-256 `45832724…`）。
现场日志：[AFP9c 会话日志](diagnostics/CharacterPanel-afp9c-f6-offscreen-20261013.log)。
前两轮：[verdict 1 崩溃](afp9-verdict-2026-10-13.md)、[verdict 2 面板全黑](afp9-black-panel-verdict-2026-10-13.md)。

## 1. 会话结果

- **复制图路线（F7→F8）修复确认**：`SCOPY COMPOSITE src=0x2874d916960 bound=0x2874d916960 match=1 draws=1`，用户确认场景图渲染正常。
- **F6 直绘仍不可见**：用户 RenderDoc 判读为位置问题——VS Output `SV_POSITION=(3788.93, 4643.61, 1.02219, 16.02144)`，即 NDC ≈ (236, 290)，人物被投影到视锥外极远处；VS Input 是正常的模型空间顶点。
- 日志互证：R01 每帧 `passes=40 rd-null=36 lights=1 om-match=1`、`A5vsA4 max-rot=0.0000deg`、`restore … ok=1`，且 **`world-d=(0.000000,0.000000,0.000000)`**。

## 2. 根因：`UpdateDownwardPass` 不移动节点自身，我的取景写在了不会生效的层

`world-d` 恒为 0 是决定性证据：**取景从未改变过根的 world**。原因是我的实现写的是

```cpp
root->local = placement;                 // 我写的层
root->UpdateDownwardPass(update_data, 0); // 它只按“对象自身 world”重算子级，不改对象自身
```

对**带父级**的活 Actor 树，根仍停在泊位（玩家下方 8000），子级世界由这个未变的根世界重算 → 整图仍在泊位，横向偏出数千单位、深度极小 → 顶点全被投到视锥外（正是 `w≈16`、NDC≈(236,290) 的形状）。
复制图没这个问题：游离图**没有父级**，`local` 就是 `world`，所以 v6.x 时代写 local 一直有效——这条差异此前没有记录，我把复制图的做法直接套到了活树上。

## 3. 修复（AFP9d）

1. **取景改为世界空间 delta 向下写**：复用快照路线已验证的求解与写入——`figure_world_delta()`（内部就是 `snapshot_delta`：placement ∘ 当前世界逆，并按实测蒙皮包围盒中心重定心）算出 delta，再由 `apply_world_delta_downward()` 把 `world`/`previousWorld` 写给整棵子树。**完全不写 local**，因此姿态（写 local 的那条链）不受影响，父级变换、`UpdateDownwardPass` 语义都不再是前提。
2. **测量顺序对齐复制图**：先 `UpdateWorldBound()` 刷新缓存包围盒，再测蒙皮包围盒（姿态刚改过骨骼世界，缓存 bound 不刷新会测到旧形状）。
3. **补上取景断言行**（这条线的缺失正是本轮漏网的原因）：
   `SCOPY R01 frame frame=N center-d=… world-t-before=(…) world-t-after=(…) anchor=(…)` —— `center-d` 是 `delta × 包围盒中心` 到锚点的距离，必须 ≈0；不符则每帧打印。
4. 恢复同样走 delta 的逆 + 逐字写回根 `worldBound`，判据含 scale；单元测试新增 `r01 framing delta`（虚构泊位世界，断言中心落到锚点、间距按 0.35 缩放）。

## 4. 本轮已验证 / 待验证

已证：复制图路线与合成 src 回读（`match=1`）；R01 的 pass 提交与目标绑定、A4/A5 姿态保全、恢复对账（world/rot/scale 精确还原）。
待验证（AFP9d）：**F6 面板出现人物**、`center-d≈0`、以及人物在面板中的位置/大小是否符合预期（取景常量沿用的是复制图那套 485/0.35）。

## 5. 复测判据（AFP9d）

1. F6 开 → 面板出现人物；日志 `SCOPY R01 frame … center-d=0.0xxx`（>0.5 会每帧刷屏 = 取景仍未生效）。
2. `A5vsA4 max-rot≈0`、`restore … ok=1`（每 120 帧一行）。
3. 若仍不可见：看 `world-t-after` 是否等于 `anchor`——不等即 delta 未落到树上，回传该行；若两者相等而画面仍空，则问题转入投影/视锥（下一层证据是 `rd-null` 与 pass 几何归属）。

## 6. 留档教训

**带父级的节点写 `local` + `UpdateDownwardPass` 不会移动它自己**（只级联子级）；要整体移动活的 Actor 树，必须按世界空间 delta 向下写 `world`/`previousWorld`。游离图（无父级）是例外——这正是复制图路线与同 Actor 路线在取景上不可互换的原因。
