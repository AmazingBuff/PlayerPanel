# AFP9 verdict 2：面板全黑的根因是同纹理“OM 目标 + PS 采样源”冲突（已修复）

日期：2026-10-13。构建：`CharacterPanel-scopy-AFP9b-ce35ad79e3+dirty-cl94faaed0c6-20261013T143909Z`（DLL SHA-256 `c7eceb61…`）。
现场日志：[AFP9b 会话日志](diagnostics/CharacterPanel-afp9b-black-panel-20261013.log)。
测试单：[AFP9 测试单](afp9-coverage-and-direct-draw-test-2026-10-13.md)；
上一轮：[verdict 1（审计解引用崩溃）](afp9-verdict-2026-10-13.md)。

## 1. 会话结果与用户判读

用户实测：F6 直绘与 F7→F8 复制图两条路线，面板内都没有人物；**RenderDoc 判读为“场景图图像已成功生成，只是这张图作为 src 应用到面板时传错了”**。日志互证：两条路线都确实把 pass 画进了摄影棚目标——`SCOPY R01 A5vsA4 … passes=51 rd-null=37 lights=1 om-match=1`、`Clone draw: passes=48 rd-null=45 lights=3` + `Clone draw OM check: … match=true`。即：**生成端成立，消费端（合成）失败**。

## 2. 根因：同一纹理同时作为 OM 目标与 PS 采样源，D3D11 静默解绑 SRV

AFP9 我把渲染分支重构成“一个 `D3D11StateCapture` 同时包住绘制与合成”。于是在合成绑源那一刻，**摄影棚 RTV 仍绑在 OM slot 0**：

```
bind studio SRV to PS t0        ← 该纹理此刻仍是 OM 目标 → D3D11 静默把 SRV 置空（debug layer 才告警）
OMSetRenderTargets(framebuffer) ← 换目标不会把 SRV 绑回来
Draw(6,0)                       ← PS 从空 SRV 采样 → 面板纯黑
```

改动前的布局不会撞上：绘制的状态捕获在合成前就析构并恢复了引擎目标，摄影棚 RTV 早已解绑。这解释了两条路线同时黑、摄影棚目标却完全正确、以及“之前能显示”的全部现象。

## 3. 修复（AFP9c）

1. **顺序即语义**：`CompositePass::draw` 先 `OMSetRenderTargets` 释放摄影棚纹理，**再**绑它的 SRV；并加一次性回读证据行
   `SCOPY COMPOSITE src=0x… bound=0x… match=1 draws=…`（每 600 次重印）——这条线本该在上一轮就存在，有它就不必开 RenderDoc。
2. **绘制与合成各自独立的状态捕获作用域**，恢复改动前被验证过的布局（双保险，任一顺序出错都不再产生黑屏）。
3. 附带修掉本轮日志暴露的两处 AFP9 判据缺陷：
   - **审计 unreachable 判据错误**：日志出现 `unreachable=6` 而点名列出 61 个（`resolved=55/61`）——负的 bone-table 目标**不等于**不可达，那些槽位正是由名字桥写入的。审计现在复刻写入自身的解析顺序（先名字、后表），并输出 `names=/ reconciles=` 自检。
   - **R01 恢复 `ok=0` 误报**：world 漂移本来就是 0，判据里的 worldBound 是引擎在 pass setup 期间按几何刷新的缓存，恢复时无法复现。现在 bound 逐字写回、`ok` 只看 local/world/rotation，观测值改名 `bound-recompute-drift`。该误报同时造成每帧刷屏（约 2000 行），一并消除。
   - F6 关闭时的 `fallback reason=disabled` 不再发告警（那是正常态，不是故障）。

## 4. 本轮取得的第一份真实覆盖审计数据（部分有效）

18 个 unprocessed 槽位（判据本身正确）**全部为 helper、无一被 skin palette 读取**：
`Camera Control`、`NPC L/R Item01–03`、`LeftWing1–5`、`RightWing1–5`；`palette=21/44 nodes=1483`。
即：这些槽位不写不构成"必要骨缺失"，风险只落在 AFP8 已点名的 `Weapon*` 附着节点上。
**unreachable 的 6 项须在 AFP9c 重新取证**——上一轮那 61 个名字是判据错误的产物。

同会话另证：世界运行中的校准成立（`verify … skeleton 88/92 ratio=0.96 match=1`），R01 身份行同源（`actor=ff006913 root=… graph=… generation=0 verified=1`），A4/A5 与姿态保全数字正常（`max-rot=0.0000–0.0280deg`）。

## 5. AFP9c 复测要点

1. **首要判据**：面板出现人物；日志出现 `SCOPY COMPOSITE … match=1`（若 `match=0` = 同纹理冲突仍在，直接回传该行）。
2. F6 直绘：身份行同源、`A5vsA4 max-rot≈0`、`restore … ok=1`（应只在每 120 帧打印一行）。
3. F7→F8 复制图：`Clone draw OM check … match=true` + 面板人物（这是本轮修复前完全看不到的那条）。
4. V1 覆盖审计：`SCOPY AFP9 audit attempted=98 unreachable=6 unprocessed=18 names=6/18 reconciles=1` 与 A2 行对账恒等式。
5. 回归：F6 关、关面板回世界无副作用。

## 6. 留档教训

**同一纹理不得同时作为输出目标与采样源**：D3D11 不报错，只在绑 SRV 那一刻把该槽位置空，表现为"图生成正确但消费端全黑"。由此得两条工程规则：合成先换 OM 再绑 SRV；绘制与合成各自独立状态捕获作用域，且合成侧必须有 src 回读证据行。
