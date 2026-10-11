# HKX43 测试单（2026-10-12，求解器修复：引擎期观测 + 真值源时机 + 缓存）——**待上机**

- 构建身份：`CharacterPanel-scopy-HKX43-c6b9a4dfc8+dirty-cl94faaed0c6-20261011T172214Z`
  （基线 = HKX42 判读提交 `c6b9a4dfc8`，其上为未提交的求解器重构）。
- 包：`dist/CharacterPanel-scopy-HKX43-1.2.1.zip`，DLL SHA-256 `2c743cfd…`
  （全值 `2c743cfd54a08bc1faf8d1b1a63e082dd2f5b8916bf2240bd90a1c1deb5a8f80`），
  **DLL 与 PDB 均已从最终 zip 部署到 `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\` 并校验一致**。单测 1/1。
- 出处：[求解器测试单](scene-copy-solver-test-2026-10-12.md) HKX42 判读的三项修复。

## 修复内容（对 HKX42 判读逐项）

1. **retry 死循环修复**：求解器结构重构——结构表（候选骨，一次构建）与真值选择（显式路径、
   可重入）分离，`frames=0` 不再重建结构表 → pose-local 回退真正可执行（HKX42 的死循环根因）。
2. **真值源换时机**：node-locals 冻结只在暂停期成立（HKX42 的决定性事实）→ **求解观测改挂
   引擎期 tick**（未暂停分支；此时傀儡节点由世界驱动，93 骨全宽真值），面板期只消费结果；
   暂停期捕获到求解中段则按 pose-local（39 骨）继续，已测死的 node 路径不再重试。两相观测
   不混积（相位切换即重置累积）。
3. **垃圾映射防御**：冻结判死的路径绝不写映射（HKX42 的 `max-pos=inf` 根因）；motion 打 3 位
   小数（0.05 边界假象消除）；**求解映射按 animation 指针缓存**（上限 48，指针仅在对象存活时
   被比较）——resolve 的第三档：绑定表空且缓存命中 → `mapping=solved-cached`，跨面板会话
   免等 1.5s。

## 上机步骤

1. 确认部署：DLL SHA-256 `2c743cfd…`。
2. 进存档后**先在世界里待 30 秒以上**（求解在引擎期观测；期间不应有任何感知异常）。
3. 开背包 F7/F8：首次切换应直接 `mapping=solved-cached`（引擎期已解完）或
   `driver … solving=1` 后短时间内 `solve clip='…' tracks=… solved=… identity-agrees=… truth=node-locals motion=…deg`。
4. ab 行判 `mapping=solved|solved-cached` 的 calm/compared——**与 HKX40 的 38–62% 对照**。
5. 跨族换武器 2–3 次（每族一轮 solve+ab）；F2/F3/F4、读档照旧回归。
6. 日志归档 `docs/diagnostics/CharacterPanel-hkx43-solver2-20261012.log`。

## 判读表

| # | 检查 | 通过判据 | 失败时 |
| --- | --- | --- | --- |
| 1 | 引擎期求解 | `truth=node-locals motion=` **≥ 数度**（节点活了）；`solve clip=… solved=90+` 出现 | `truth-frozen` = 引擎期节点也不动（新事实，泊深 cull 问题），回传 |
| 2 | **ab 判 solved** | calm/compared 相比 HKX40 显著上升（理想 ≥90%） | 不升 = 轨道序不是主因（混合层污染论上桌），回传 ab 全行 |
| 3 | **目视** | 求解后面板人物姿态明显对位 | 无改善 = 同上，两论对质的数据都在 ab 里 |
| 4 | 缓存生效 | 第二次开背包同一 clip 直接 `solved-cached`、无 solving=1 | 未命中 = 缓存键没对上，回传 |
| 5 | 零回归 | 零崩溃、无新 `[E]`、equip/F2/读档照常 | —— |

**路线含义**：#2+#3 全绿 = 显示映射实测化闭环，动画线收敛 → S1 安全回收；#2 红线 =
"混合层污染"假设上桌（poseLocal/节点都是混合输出，单 clip 采样永远追不上）→ 显示端改为
"追混合输出"（求解器照旧、判读阈值重定），仍是本轮数据可判。
