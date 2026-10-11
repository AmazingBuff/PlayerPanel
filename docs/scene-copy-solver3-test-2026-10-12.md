# HKX44 测试单（2026-10-12，求解器崩溃修复：真值表不再跨帧持指针）——**待上机**

- 构建身份：`CharacterPanel-scopy-HKX44-fbd718c05e+dirty-cl94faaed0c6-20261011T173331Z`
  （基线 = HKX43 提交 `fbd718c05e`，其上为未提交的崩溃修复）。
- 包：`dist/CharacterPanel-scopy-HKX44-1.2.1.zip`，DLL SHA-256 `35108e71…`
  （全值 `35108e7164c57ad16478564e0381c9a126dbb8da5dd8f85d7e230a46d5bcd042`），
  **DLL 与 PDB 已从最终 zip 部署到 `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\` 并校验一致**。单测 1/1。
- crash 证据：[crash-2026-10-12-01-27-26.log](diagnostics/crash-2026-10-12-01-27-26.log)。

## 崩点与根因（crash 日志实锤）

```
Unhandled exception EXCEPTION_ACCESS_VIOLATION at CharacterPanel.dll+4A261  mov eax,[r9]
  animation_source.cpp:3451 rotation_angle_degrees(NiMatrix3 const&, NiMatrix3 const&)
  ← solve_tracks_step (animation_source.cpp:3562)
  ← PuppetDriver::tick (puppet_driver.cpp:190)   ← renderer draw bracket
```
**根因**：求解观测表（node_candidates）缓存裸 `NiNode*` 跨观测帧——引擎期傀儡 3D 是引擎财产
（装备挂接/facegen/换树随时重建），两帧之间节点被释放 → 真值旋转读到悬空地址。与 HKX42 的
"暂停期节点冻结"叠加：冻结真值 + 悬空指针都在这张跨帧表上。

## 修复（HKX44）

1. **真值表逐帧重建**：node/pose 候选表不再跨帧持有——每个观测帧从傀儡活树/graph 重新解析；
   跨帧只留误差累积矩阵、frozen 检查的上一帧真值（按索引对位，不持指针）与判定状态。
2. **每帧页守卫**：节点真值读前 `readable(node, sizeof(NiAVObject))`，不可读即本帧视为不可达；
   pose 真值同理。
3. **副本节点同帧查找**：写映射时的副本节点改为 `find_descendant(copy_root, name)` 当帧查找，
   不再走解析期缓存表。
4. 观测语义修正：误差矩阵只在（重）启动时清零，观测帧只累加（HKX43 首版曾误为逐帧清零，
   累积无意义——本轮一并修正，solve 需要 90 帧里姿态在动的证据）。

## 上机步骤

1. 确认部署：DLL SHA-256 `35108e71…`。
2. 进存档 → **世界期 30 秒以上**（引擎期求解观测，无感知）→ 开背包 F7/F8：
   `mapping=solved-cached` 或 `driver … solving=1` → 引擎期 `solve clip=… solved=… truth=node-locals motion=…deg`。
3. 换装 2–3 次跨族；**做几次进出室内/门转换**（上次崩溃窗口就是引擎期节点变动）——零崩溃。
4. ab 行（`mapping=solved`）calm/compared 对照 HKX40 的 38–62%。
5. 日志归档 `docs/diagnostics/CharacterPanel-hkx44-solver3-20261012.log`。

## 判读表

| # | 检查 | 通过判据 | 失败时 |
| --- | --- | --- | --- |
| 1 | **零崩溃**（含门转换/换装/读档） | 全程无 crash | 再崩 = 立即回传 crash 文件 + 复现动作序列 |
| 2 | 引擎期求解完成 | `solve clip='…' solved=90+ truth=node-locals motion=` 数值可观（≥0.1deg） | `truth-frozen` = 引擎期节点也没动（泊深 cull 新事实），回传 |
| 3 | ab 判 solved | calm/compared 较 38–62% 明显上升 | 不升 = 混合层污染论，回传 |
| 4 | 目视 | 求解后面板人物姿态对位改善 | 无改善 = 同上 |
| 5 | 零回归 | 零新 `[E]`、equip/idle/F2/读档照常 | —— |
