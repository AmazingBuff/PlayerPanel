# HKX2 实测证据：把引擎自己的姿态写进副本（2026-10-10）

- 构建身份：`CharacterPanel-scopy-HKX2-a0eaef50cd-cl94faaed0c6-20261009T161113Z`，与
  `dist/CharacterPanel-scopy-HKX2-1.2.1.zip` 内 `build-manifest.json` 一致（`source_baseline_dirty=false`，基线 `a0eaef50cd`）。
- 日志：[CharacterPanel-hkx2-replay-20261010-0016.log](diagnostics/CharacterPanel-hkx2-replay-20261010-0016.log)
  （SHA-256 `ecd76d88ef94983a3a88532539b75e206f2b756e5dbbeaace7769e084bb3fb82`）。**无崩溃**（`crash-*` 最新仍是 2026-10-08）。
- 两次捕获：capture=1（470 节点、CBBE、90 命中），capture=2（1851 节点、109 命中）。绘制正常
  （`passes=23`、`passes=47`），只有既有的 `Studio lights …` 警告。

## 1. 四元数约定：直接式（本轮把假设变成了事实）

| 候选 | capture 1 位置 / 旋转 | capture 2 位置 / 旋转 |
| --- | --- | --- |
| `order=skeleton quat=direct` | 3.743u / **0.08°** | 5.591u / **0.10°** |
| `order=skeleton quat=transposed` | 56.438u / 179.19° | 146.048u / 179.77° |
| `order=bone-nodes quat=direct` | **1.614u** / 0.03° | **0.529u** / 0.06° |
| `order=bone-nodes quat=transposed` | 56.439u / 179.19° | 58.999u / 165.22° |

`transposed` 给出 165–180°：那正是**逆旋转**，说明它错；`direct` 把姿态复现到 **0.03–0.10°**。
→ 单测里锁定的标准四元数公式就是引擎的约定（`ni_matrix_from_quaternion`，见 `animation_source.h`）。

## 2. 负结果二：`poseLocal` 按 `boneNodes` 次序索引，不是骨架次序

两次捕获里 `bone-nodes/direct` 都优于 `skeleton/direct`（0.529 vs 5.591；1.614 vs 3.743），且它是唯一被
`verdict` 点名的候选（capture 2 `match=bone-nodes/direct`；capture 1 `match=none`，因为 `best/control=1.29%`
略高于我设的 1% 相对门槛——**看排名而不是看这个词**）。

→ 结论：**`poseLocal[i]` 属于 `boneNodes[i].node`**。HKX4 若要拿 `poseLocal` 当真值，必须按 `boneNodes` 索引；
而动画轨道的骨索引是**动画骨架**索引，两者只能靠**骨名**桥接（HKX1 已证明骨名 109/116 精确可用）。

## 3. 写路径成立，且测量本身是灵敏的（这是本轮最关键的一条）

- **对照**：把副本先摆偏 0.5 rad 后，"副本 vs 源"的最差骨距离 = **124.740u**（capture 1）／**198.992u**（capture 2），
  旋转差 171.80°／178.79°——测量确实灵敏，不是"怎么写都接近 0"。
- **复原**：验证结束后重测 = **0.000u**（两次都是），说明这次验证把面板**逐字恢复**了，也让"0.529u"这种数字
  有了意义：仪器有能力报 0，残差是真差异而非噪声。
- 写路径 = 按名解析骨→节点 + 四元数→`NiMatrix3` + 写 local + 一次自顶向下世界重算，**全部走通**。

## 4. 残差是"位置、且很小"：0.5–1.6 单位、旋转 0.03–0.10°

位置有残差而旋转几乎为零，且 `scale-off-from-one=4`（capture 1）／`7`（capture 2）——最可能是**缩放语义**：
`hkQsTransform` 带三个缩放分量，`NiTransform` 只有一个；本轮写的是姿态的缩放（分量 0），引擎可能保留节点自己的缩放
或按分量处理。下一轮把"保留节点缩放"加为第二个缩放候选即可（同一套测量，不需要新仪器）。

## 5. 附带观测

- `map … other-unresolved=23`（capture 1，CBBE 极简体型）与 `4`（capture 2）：缺的骨随装备/体型变化，
  所以跳过清单必须是逐骨的运行时状态，不能写死。
- `bone-nodes` 候选只写到 `written=106`（表里 116 项）：10 项在该副本里没有同名节点（帮手骨/附着骨），
  与 HKX1 的分类一致。
- capture 2 的绘制里 `rd-null=44/47`（3 个 pass 有 render-data），与之前几轮"全 rd-null"不同；本轮代码不碰 render-data，
  面板显示正常，**未解释，留档观察**。

## 6. 判定与下一步

- HKX2 的目标（"引擎自己的姿态能否驱动副本、能否定量比较"）**成立**：旋转 0.03–0.10°、位置残差 ≤1.6u、
  对照 125–199u、复原 0.000u。
- 下一步 HKX3（已打包待测）：动画目录（typed 名字表 → 可直接采样的 Idle 索引）与绑定集元素布局的结构化核对；
  之后 HKX4 用 `binding->animation->SampleTracks(t, …)` 采样，真值取同一相位下的 `poseLocal`（**按 `boneNodes` 索引**）。
