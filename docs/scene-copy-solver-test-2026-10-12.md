# HKX42 测试单（2026-10-12，经验映射求解器：恒等档判死后的实测映射）——**待上机**

- 构建身份：`CharacterPanel-scopy-HKX42-515d62f7d6+dirty-cl94faaed0c6-20261011T165746Z`
  （基线 = HKX41 提交 `515d62f7d6`，其上为未提交的求解器与驱动接线）。
- 包：`dist/CharacterPanel-scopy-HKX42-1.2.1.zip`，DLL SHA-256 `4e32ea27…`
  （全值 `4e32ea274daf67da71e02471fa41957f52c51b8066c50de9a7308328dd1bed9a`），
  **DLL 与 PDB 均已从最终 zip 部署到 `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\` 并校验一致**。单测 1/1。
- 出处：[诊断轮测试单](scene-copy-diagnose-test-2026-10-12.md) 的路线判定——恒等映射对本机
  OAR 替换件系统性错位（calm/compared = 38–62%），改为**实测映射**。

## 本轮内容：求解器（`solve_tracks_step`）

**原理**：绑定表为空的替换件，其真实轨道序不再猜——每个暂停 tick 采一帧观测（99 条轨道在
live control 时间采样 × 每个候选真值骨的当前旋转比对），累积 90 帧（约 1.5s）后逐轨道取
**最小累积误差**的候选骨（贪心占位，两条轨道不写同一骨），就地重写 `m_playback` 的映射
（`mapping=solved`），显示端立即生效。ab 判读顺延到求解完成后，判的就是求解映射。

**双真值 + 冻结检测**（本轮要回答的新问题）：
1. 首选真值 = **傀儡 3D 节点 local**（按骨名，覆盖 ~93 骨，与显示映射同名空间）；
2. 节点旋转在累积期位移 <0.05° = 冻结（手动推图可能不把姿态传播到节点）→ 自动**回落
   poseLocal**（39 骨 vanilla 名核心）重跑；
3. 双真值都冻结 → `solve UNAVAILABLE reason=truth-frozen`，保持恒等，如实报告。

**日志行**：
```
SCOPY PUPPET driver clip='…' mode=… mapping=identity-unverified solving=1 coverage=…
SCOPY PUPPET solve frames=45/90 truth=node-locals motion=12.31deg        （累积中，每 45 tick 一条）
SCOPY PUPPET solve clip='…' tracks=99 solved=93 identity-agrees=41 truth=node-locals motion=…deg
SCOPY PUPPET ab clip='…' mapping=solved bones=… calm=… verdict=…          （判的就是 solved）
```
`identity-agrees=J` = 多少轨道引擎本来就按恒等排——OAR 替换件轨道重排规律的第一个数。

## 上机步骤

1. 确认部署：DLL SHA-256 `4e32ea27…`。
2. 进重度存档 → 开背包 F7/F8 → 等首次切换：看 `driver … solving=1` → 90 tick 后 `solve …`
   行（**truth= 是哪条路径、motion 多大**）→ 之后 ab 行（`mapping=solved`）。
3. 换 2–3 次武器跨族（1HM/2HW/Bow）：每族一次 solve + 一次 ab。
4. **目视主线**：求解完成后面板人物姿态是否**明显更对**（对照 HKX40 的错位感）。
5. F2/F3/F4、读档各一次照旧回归。
6. 日志归档 `docs/diagnostics/CharacterPanel-hkx42-solver-20261012.log`。

## 判读表

| # | 检查 | 通过判据 | 失败时 |
| --- | --- | --- | --- |
| 1 | 真值路径 | `truth=node-locals` 且 `motion` 数秒内 ≥ 数度（图在推节点） | 落到 pose-local 也行；`truth-frozen` = 手动推图不传播姿态（新引擎事实），ab/显示照旧但求解搁置，回传 |
| 2 | **求解成立** | `solve … solved=90+ identity-agrees=J`（J 免疫多大都行，是数据）且后续 ab `mapping=solved` | solve 行缺席 = 累积被打断（切 clip 太快），回传 |
| 3 | **ab 判 solved** | `calm/compared` 相比 HKX40 的 38–62% **显著上升**（理想 ≥90%） | 不升 = 节点 local 与轨道采样不相关（混合层污染），看 ab 的 max-rot 分布回传 |
| 4 | **目视** | 求解后面板人物姿态与真机对照明显改善 | 无改善 = 映射不是主因（混合/缩放层），回传 |
| 5 | 零回归 | 零崩溃、无新 `[E]`、equip/idle/F2 照常 | —— |

**路线含义**：#2+#3+#4 全绿 = FR-03 的"面板人物播对 OAR 替换件"闭环（映射实测化），S2 的
动画线收敛，回 S1 安全回收主线；#1 红线 = 手动推图传播链的新事实，求解器转 poseLocal 路线
再测（39 骨也能显著改善核心骨错位）。
