# S2 动作探针：诊断证据（2026-10-09）

## S2P3 第三轮（2026-10-09 22:40–22:43）：候选量全部落空 —— 问题在槽的布局，不在候选集

日志存档 [CharacterPanel-s2p3-probe-20261009-2243.log](diagnostics/CharacterPanel-s2p3-probe-20261009-2243.log)
（705627 字节），身份与 S2P3 包 manifest 一致。两次捕获（CBBE 470 节点／UBE 1584 节点）、
566 组 T0–T3、1132 条 `T1c`/`T3c` 分类样本；零 `INVALID`、零崩溃、两次 F4 停放。

**操作者目视配对（R06）**：F2 开启时人物摆动，**关闭后人物复位并保持静止** —— 这是本测试单要求的
"探针关／开"配对观察，**R06 成立**。

**分类结果（1132 样本，最近候选）**：

| 最近候选 | 次数 | 距离范围 |
| --- | --- | --- |
| `bone-captured` | 450 | 2.01–2.48 |
| `bone-captured-T` | 252 | 2.05–2.39 |
| `bind` | 210 | 1.98–2.28 |
| `captured-x-bind` | 82 | 2.00–2.55 |
| `bind-x-world` | 74 | 2.01–2.40 |
| `bind-T` | 24 | 1.97–2.00 |
| `world-x-bind` | 22 | 2.01–2.55 |
| `bone-prev-frame` | 10 | 2.00–2.64 |
| `bone-world` | 8 | 1.99 |

**全部样本的最近距离都在 1.97–2.64 之间**（9 个分量上的 L1，各分量约 0.22）：**没有任何候选接近 0**，
"最近者"在 8 个候选之间随机轮换且距离几乎相同 —— 这说明**不是候选集缺了什么，而是我读槽的方式错了**。
最可能的解释：`boneMatrices` 的 3×4 布局与 `write_bone_matrix`/分类器假定的"行主序、转译在第 3/7/11 位"
不同（例如 GPU 形式的列主序 4×3），于是我从 12 个数里取出来的"旋转块"根本不是旋转；另一种可能同样
无法排除：**矩阵缓冲的下标不是 `bones[]` 的下标**（调色板重映射），那样连槽位本身都是错的。

两个矩阵缓冲的分类分布逐项相同（当前缓冲 221/129/102/45/37/12/11/9，上一缓冲完全相同），
因此"着色器读另一份缓冲"这一解释**不被支持**。

**这一轮的结论**：S2P2 的 50/50 不是"槽里是第三种量"，而是**读数布局错误**导致的噪声；R05 的 CPU 侧
仍未通过，且**继续加候选量没有意义**——必须先直接看到 12 个原始浮点数。

## S2P4 第四轮（2026-10-09 22:52–22:54）：槽的语义定性与 R05 的 CPU 侧结论

日志存档 [CharacterPanel-s2p4-probe-20261009-2254.log](diagnostics/CharacterPanel-s2p4-probe-20261009-2254.log)
（253017 字节），身份与 S2P4 包 manifest 一致。两次捕获（CBBE 470 节点／UBE 1584 节点），
`SCOPY TDUMP`＋102 条 `SCOPY TSLOT`（`clothes` 31 槽、`Dress` 71 槽，均为该皮肤全部槽位），
零 `INVALID`、零崩溃、两次 F4 停放。离线分析脚本：
[槽布局/尺度](diagnostics/analyze_tslot_scale.py)、[槽位映射](diagnostics/analyze_tslot_mapping.py)、
[原始排布](diagnostics/analyze_tslot.py)。

### 结论一：缓冲布局确认，且**带缩放**——这正是前三轮全部落空的原因

以被摆动的骨盆为例（`clothes` 皮肤 slot=10，同一帧的原始值）：

```
world  =(0.9947 -0.1013  0.0186   0.9115 | 0.1028 0.9880 -0.1153 -485.9277 | -0.0067 0.1166 0.9932   2.4928)
matrix =(0.3481 -0.0355  0.0065   0.4620 | 0.0360 0.3458 -0.0404 -483.1459 | -0.0024 0.0408 0.3476 -21.4609)
```

`matrix` 的 3×3 旋转块 = **0.3500 × world 的旋转块**（9 个分量逐项拟合，残差 **0.0002**）。
0.35 正是 `Studio_Figure_Scale`：蒙皮矩阵把节点变换的 **scale 烘进 3×3**（`[s·R | t]`），
而 S2P3 的十个候选量全部用的是 `NiTransform::rotate`（单位长度）——**每个分量差 0.65 倍**，
9 个分量 L1≈2.0，正好解释了那轮"最近距离恒在 1.97–2.64、最近者在 8 个候选间随机轮换"的现象。
**布局本身没问题**：3×4 行主序、旋转在 [0,1,2]/[4,5,6]/[8,9,10]、平移在 [3,7,11]。

### 结论二：引擎确实**按节点重算**了蒙皮矩阵（R05 的 CPU 侧成立）

同一帧里，`matrix` 匹配的是节点**当前**（摆动中）的世界变换，而不是捕获姿态：该帧骨盆已离开
捕获姿态（`bone-row1` 由捕获的 `(0.100, 0.993, 0.070)` 变为 `(0.103, 0.974, -0.201)`，15.6°），
而缓冲与之的贴合度是 0.0002 —— 若缓冲停在捕获姿态或克隆期数值，误差至少是 0.2 量级。配合
S2P2/S2P3 已测到的 `changed-since-T2=true`（241/241、566/566，绘制中必被重写），链条闭合：

**我们写节点 → 引擎在绘制中按节点重建该槽 → 着色器消费 → 画面随摆动（S2P3 目视配对）**。

### 结论三（新的、必须遵守的边界）：槽位下标 **不** 等于 `bones[]` 下标

把每个槽的 3×3 与**同一皮肤所有骨骼**（含尺度）逐一比对，只有少数槽匹配到自己的 `bones[i]`：
`clothes` 5/31、`Dress` 4/71；而 `Dress` 的 71 个槽里只有 **16 个互不相同的矩阵块**（前 12 个槽完全
相同，都等于骨盆矩阵）。因此**不能假定 slot i 就是 bones[i]**——这同时解释了更早几轮
"把 `bones[i]->world` 写进 slot i 却毫无效果"的失败：写入打偏了。真实映射（调色板顺序、或分区的
vertex-bone 索引序）**本轮未解析**，属于将来真要写缓冲时必须先解决的前置问题。

### 本轮同时确认

- **R06 成立**（S2P3 目视配对：F2 开＝摆动，F2 关＝复位并静止）。
- 一次 `session-boundary` 释放与两次 F4 停放均正常，无泄漏路径异常、无崩溃。
- 测量自始至终**只读**：四轮探针从未写入过蒙皮矩阵缓冲。

## S2P4 设计留档

相对 S2P3 只加一次性的原始转储（不改摆动、不写缓冲）：每次 F2 开启后第一帧，把目标皮肤**全部
槽位**逐个打印为 `SCOPY TSLOT i=… bone='…' world=(骨骼自身世界变换) matrix=(缓冲内容) previous=(第二缓冲)`。
有了同一帧的两列原始数字，布局与槽位映射都能离线直接读出——这也是本轮两个结论的来源。

## S2P3 设计留档：待测的槽语义定性轮

1. **两个缓冲都采样**：`NiSkinInstance::boneMatrices`(0x48) 与 `prevBoneMatrices`(0x50)，检验
   "着色器在某些帧读另一份"这一最可能的 50/50 解释；
2. **十个候选量**：骨骼世界／捕获姿态／上一帧摆动三者的旋转与其转置、bind（skinToBone）变换
   及其转置、world×bind 与 bind×world 与 captured×bind 乘积；每帧在 `SCOPY T1c`／`T3c` 打印
   两个缓冲各自的"最近候选(距离) + 次近候选(距离)"。
   候选全部来自引擎侧的节点与皮肤数据，**没有任何一项来自本代码写入的值**。
3. 单测新增"旋转与其转置必须可区分"断言（否则候选集内部就会打平）。

判读约定：某个候选的距离**稳定接近 0** → 槽的语义被定性，再看它是否随摆动更新即可回答 R05 的
CPU 侧；所有候选距离都很大 → "槽存放的是这套候选之外的量"本身即为结论，下一步只能靠图形调试器。
本轮仍不写缓冲，画面侧请顺带做一次 F2 关/开的目视配对（R06）。

## S2P2 第二轮（2026-10-09 22:27–22:30，CBBE 一次 + UBE 两次捕获）

日志存档 [CharacterPanel-s2p2-probe-20261009-2230.log](diagnostics/CharacterPanel-s2p2-probe-20261009-2230.log)
（221455 字节）。首行身份 `CharacterPanel-scopy-S2P2-a7073d58b8+dirty-cl94faaed0c6-20261009T141902Z`
与 S2P2 包 manifest 一致。操作：捕获 → F8 → F2 开 → F2 关 → F2 开 → F3 旋转 12 次 → F4；
随后换存档重捕两次、再开 F2。共 **241 组 T0–T3（964 行）**，零 `INVALID`、零崩溃、`AUDIT` 全 0。

| 项 | 捕获 1（CBBE） | 捕获 3（UBE） |
| --- | --- | --- |
| 规模 | `nodes=470 geoms=23 skins=13`，passes=23 | `nodes=1585 geoms=64 skins=62`，passes=46，`pruned-nodes=14` |
| 探针骨骼 | `NPC Pelvis [Pelv]`（候选 74，子树 219） | `NPC Spine [Spn0]`（候选 361，子树 968） |
| 骨骼父链 | `Pelv < CME LBody < MJF CME Body < CME Body < NPC COM < NPC Root < NPC < skeleton_female.nif` | `Spn0 < CME UBody < MJF CME Body < CME Body < NPC COM < NPC Root < NPC < skeleton_female.nif` |
| 采样皮肤 | `clothes`（31 骨），slot=10 | `Dress`（71 骨） |
| 影响面 | `affected-geoms=10/23` | `affected-geoms=43/46` |
| 见证点 | `NPC L Toe0 [LToe]`，半径 23.95 | `RightWing5`，半径 23.68 |

### 上一轮问题的答案：两身形摆动部位不同 = 骨架本身就是分开的两支

父链实锤：**骨盆挂在 `CME LBody` 下、脊柱挂在 `CME UBody` 下**，两支都从 `CME Body` / `NPC COM`
分叉——骨盆不是脊柱的祖先。所以摆骨盆只带动下半身、摆脊柱只带动上半身，是骨架结构决定的，
与锚点无关；`affected-geoms` 把这件事量化了（10/23 vs 43/46）。S2P2 的全局选骨（在所有蒙皮里选
子树最大者）在 CBBE 上选中骨盆、在 UBE 上选中脊柱，是因为两套装束挂的物理/布料链不同、
两支的节点数不同，属于预期差异而非缺陷。

### 证据统计（241 组）

| 字段 | 结果 | 判读 |
| --- | --- | --- |
| `orientation-delta-deg` | 1.3–**34.4**（设计值 ±0.6 rad = ±34.4） | 朝向读数修复生效；摆动确实以设计幅度施加到节点树（**R03 成立**） |
| `witness-moved` / `probe-effective` | 最大 13.77；**241/241 true** | 见证点随摆动位移（R03 的位移判据） |
| `changed-since-T1` | 241/241 false | T1→T2 之间没人改缓冲（我们也不写） |
| `changed-since-T2` | **241/241 true** | 绘制过程中引擎自己重写了该槽（R05 的"引擎侧重算"环节存在） |
| `pre/submit/final-rel` | `captured` 118 / `swung` 117 / `neither` 6（final 为 117/124） | **仍不能判读槽的语义**，见下 |
| 重捕后的陈旧记录 | **0 行**（S2P1 为 136 行） | S2P2 的"重捕即清目标"修复生效 |

### `*-rel` 为什么仍不能判读（本轮没有拿到 R05 的 CPU 侧结论）

按摆动幅度分层统计 `final-rel`，四个区间都是约 50/50：

| 摆动幅度 | 组数 | swung | captured |
| --- | --- | --- | --- |
| <10° | 45 | 23 | 22 |
| 10–20° | 48 | 24 | 24 |
| 20–30° | 67 | 38 | 29 |
| >30° | 81 | 39 | 42 |

**在最大幅度处（±34.4°，前后帧姿态几乎相同）标签仍然对半分**，这排除了"槽跟随我们的节点写入
（哪怕带一帧延迟）"这一解释：若跟随，极值区应压倒性地出现 `swung`。结论是**我们仍不知道这个槽
存的是什么量**——可能是复合蒙皮矩阵、转置存储，或另一个骨架空间的量；`relation()` 只证明了
"槽在动"，没有证明"槽等于我们写的骨骼"。这正是 PRD §6.3 禁止"用写后读回给自己分类"的同一条边界：
要给它定性，必须把**引擎自己写的内容**与多个候选量（原始世界矩阵、其转置、world×bindInverse 等）
并列比较，或用图形调试器直接看 GPU 绑定。**因此 R05 的 CPU 侧仍未通过，GPU 侧仍未测。**

## S2P1 游戏内第一轮（2026-10-09 22:11–22:13，CBBE + UBE 各一次捕获）

日志存档 [CharacterPanel-s2p1-probe-20261009-2213.log](diagnostics/CharacterPanel-s2p1-probe-20261009-2213.log)
（134298 字节，一次会话）。构建身份 `CharacterPanel-scopy-S2P1-a7073d58b8+dirty-cl94faaed0c6-20261009T140032Z`
与包 `dist/CharacterPanel-scopy-S2P1-1.2.1.zip` 的 `build-manifest.json` 逐字一致 → **测试单 R01 的"manifest 与日志匹配"成立**。

**用户实测（本轮最重要的一条）：探针开启后画面确实随摆动变化，CBBE 与 UBE 两个身形都成立**，
CBBE 视觉上是下半身在动、UBE 是上半身在动。日志给出了两者不同的原因，见下。

| 项 | 捕获 1（CBBE 存档） | 捕获 2（UBE 存档） |
| --- | --- | --- |
| 规模 | `nodes=470 geoms=23 skins=13`，passes=23 | `nodes=1584 geoms=63 skins=62`，passes=45，`pruned-nodes=14` |
| 探针骨骼 | `NPC Pelvis [Pelv]`（来自 `Body [Ovl0]`，subtree=219） | `NPC Spine2 [Spn2]`（来自 `Face [Ovl0]`，subtree=906） |
| 见证点 | `NPC R Toe0 [RToe]`，半径 23.93 | `RightWing5`，半径 16.01 |
| 帧组数 | 188 行（47 组） | 240 行（60 组） |

**两身形摆动位置不同 = 目标骨骼不同，不是锚点问题**：探针此前只在"遍历到的第一个蒙皮几何"的
骨骼表里选目标，CBBE 命中 `Body` 皮肤 → 选了骨盆，UBE 命中 `Face` 皮肤 → 选了 Spine2；
支点高度与见证点因此都不同（骨盆→脚趾、Spine2→头发/翅膀骨）。取景锚点是捕获时算好的，
`pose()` 每帧都在探针之前恢复捕获世界变换，日志里 `SCOPY FRAME skinned-bound` 也没有随摆动变化，
所以锚点不参与这个差异。

**证据统计（141 组 T0–T3）**：

- `probe-effective=true`：**141/141**；见证点位移 `moved` 最大 14.10（CBBE）、14.10 级（UBE 亦同量级），
  `moved-range` 非零 → 探针确实在摆、且摆动到达了节点树（R03 的位移判据成立）。
- `changed-since-T2=true`：**141/141** —— 我们从不写缓冲，所以绘制过程中**引擎自己重写了该矩阵槽**，
  即 `SetupGeometry`/`SetupAndDrawPass` 路径每绘制帧重建蒙皮矩阵；这正是 PRD §6.3 要求定位的
  "引擎侧重算"环节。
- `changed-since-T1`：34/141 为 true（摆动之后、pass 提交之前槽内容已变）。
- **`orientation-delta-deg=0.0` 与 `*-rel=neither`：141/141 —— 这是本轮的仪器缺陷，不是结果**（见下）。

**本轮暴露的三个缺陷（都出在探针自身，S2P2 已修）**：

1. **朝向读数对 X 轴摆动失明**：T1/T2/T3 用矩阵**第 0 行**做朝向判据，而 S2P1 起摆动轴就是世界 X
   —— 第 0 行正是 X 基向量，摆动不改它，于是 `orientation-delta-deg` 恒为 0。同一原因让
   `relation()`（把槽内容判给"摆动后/捕获时"）永远输出 `neither`，**R05 的 CPU→GPU 判据因此无法读取**。
2. **F7 重捕后探针仍在摆上一份图**：目标骨骼/几何缓存不随 `capture()` 失效。捕获 2 之后、F2 未重新
   开关之前（22:12:50–22:12:54，**136 行 T 记录**）探针摆的是已被停放的旧图，面板画的是新图——
   那一段数据与画面无对应关系（旧图仍活着所以没崩，属于"停放不销毁"掩盖的又一处）。
3. **`pass-geometry` 会打印陈旧字符串**：T2 的几何名只在指针匹配时赋值、不按帧清空，未匹配的帧
   沿用上一帧的名字，"这一帧有没有提交该 pass"变得不可辨。

**结论（本轮能下的）**：S2 的前置命题——"副本的蒙皮读它自己的节点树，写节点就能动"——**已由
游戏内观察与日志同时成立**（两个身形、两种骨架、141 组判据全部 `probe-effective`）。
**仍不能下的**：CPU 矩阵槽是否跟随我们写入的骨骼（本轮读数失明，需要 S2P2 的配对数据），
以及 GPU 侧绑定/内容（仍需 RenderDoc 配对抓帧）。S2 的 HKX 驱动、S3 物理均未开始。

## S2P1 归档：旧版探针的诊断证据

> **日志行换代说明**：以下记录的是 S2P1 之前那一版探针（在 `pose()` 之前运行、绕骨骼局部 Z 轴、
> 并包含写回自比较的 `buffer/written/compare/slot0-raw` 行）。自 `CharacterPanel-scopy-S2P1` 起，
> 探针改在绘制窗口内按 PRD §6.3 顺序执行，只改节点变换，日志换成 `SCOPY ANIM target` 加每 8 帧的
> `SCOPY T0`–`SCOPY T3` 四行（见 [S2/S3 方案](s2-s3-plan.md#探针用法f2-开关) 与包内 README），
> S2P2 起朝向判据改用相对旋转的迹、槽位判据改用整个 3×3 块。下文原样保留为历史证据。

本文件是 [S2/S3 方案](s2-s3-plan.md) 的原始证据索引。2026-10-09 复核后，路线否定结论撤销，下一轮依据 [PRD 0.6](player-panel-prd.md) 和 [测试单](scene-graph-copy-next-test-2026-10-09.md)。全文日志存档在
[CharacterPanel-s2-anim-probe-20261009-0133.log](diagnostics/CharacterPanel-s2-anim-probe-20261009-0133.log)
（61446 字节，一次会话，含 341 条 `SCOPY ANIM` 行）。

## 环境与身份

```
[01:30:42] SCOPY BUILD CharacterPanel-scopy-S0-2026-10-08 runtime=1-6-1170-0
           mode=manual-scene-copy legacy-actor-route=disabled animation=not-implemented
           cbpc=not-registered smp=not-registered
           hotkeys=F7-copy F8-draw F3-rotate F4-release F2-anim-probe
```

构建身份带 `hotkeys=` 字段，用于一眼确认部署的是哪一版 DLL——这一条是为"装的还是旧 DLL"
这类误判加的（此前 F3 无效就是旧 DLL 造成的）。

## 实验序列

F7 复制（`BEGIN`/`READY` 各一条）→ F8 绘制 → 按 **F2** 打开动作探针 → 探针每帧摆动
`NPC Pelvis [Pelv]` 并重算 23 个 skin 共 331 个矩阵槽 → 按 F2 关闭。

探针骨骼的选择方式：**取自某一个 skin 自己的 `bones[]` 数组**（保证被蒙皮引用），
再在该 skin 内取子树覆盖节点最多者。

## 关键行与判读

### 1. 清理后的副本没有已接入动画驱动

```
SCOPY ANIM report … controllers=none verdict=copy-is-graph-invisible
   skins: [Body [Ovl0]: 30/30 direct] [Body [SOvl0]: 30/30 direct]
          [Hands [Ovl0]: 38/38 direct] [Hands [Ovl1]: 38/38 direct]
```

- 日志同时记录 `controllers-removed=14`。当前实验主动清理了控制器，`controllers=none` 仅证明清理后的状态，不能证明原生复制没有控制器或不能建立独立上下文；
- 副本**不带指回原角色的 `userData`** → 所有"经 userData 取原角色动画图"的方案排除
  （这是最大的陷阱：取了就会驱动世界角色，违反 PRD FR-05）；
- `boneWorldTransforms` **全部指向副本自己的骨骼**（`30/30`、`38/38`），排除"蒙皮读的是
  原角色骨骼"这一假设。

### 2. 骨骼确实被摆动，缓冲区确实被写入

```
SCOPY ANIM probe bone='NPC Pelvis [Pelv]' candidates=30 subtree-nodes=219
   child='CME Pelvis [Pelv]' bone-row0-original=(0.996,-0.080,0.038)

SCOPY ANIM bone-pose now=(0.651,0.754,0.087) original=(0.996,-0.080,0.038)

SCOPY ANIM matrices skin='Body [Ovl0]' slots=30 probe-slot=9
   slot-row0=0.651 bone-now=0.651 bone-original=0.996
   follows-swing=true rebuilt-skins=23 rebuilt-slots=331
```

- 探针骨骼找到了它在 skin 中的**真实槽位**（`probe-slot=9`，不再假设第 0 槽）；
- 骨骼姿态逐帧变化（`now` 与 `original` 不同）；
- 缓冲区槽位**跟随该变化**（`slot-row0` 与 `bone-now` 相等）；
- 单帧内 `rebuilt-slots=331`、`rebuilt-skins=23`，即全部蒙皮都被重算。

整个会话中 `slot-row0` 出现过 **25 种不同取值**（0.474 … 0.996，另有 `0.000` 属于
未找到槽位的 `Hands [Ovl0]`），证明写入是逐帧生效的、不是一次性快照。

### 3. 画面依然不动 —— 观察成立，机制未判定

```
SCOPY ANIM report … probe-effective=false tracked='CME Pelvis [Pelv]'
   moved=0.00 moved-range=[0.00,0.00]
```

归档支持“探针执行期间骨骼／CPU 数组变化，用户观察画面不动”。当前调用顺序为
`apply_animation_probe → draw → pose → GetRenderPasses → SetupAndDrawPass`；其中 pose 恢复捕获的节点世界变换。
日志发生在这个恢复与引擎准备之前，不能证明修改到达实际 draw／GPU。
整槽写坏与跳过重建是其他轮次的报告，需要各自构建和原件；未连接上传／消费链前，同样不能用于排除整个路线。

> **当前结论**：CPU 写入、后续覆盖／重建、GPU 上传及最终消费关系尚未确定；S2 保持未判定。

## 我必须纠正的一处判读（同一份证据里的教训）

本轮日志里有一个容易读错的字段：

```
SCOPY ANIM buffer slot=9 R=(0.651 0.754 0.087 | …)     ← 注意：打印的是"写入之后"的内容
SCOPY ANIM written R=(0.699 0.710 0.086 | …)
SCOPY ANIM compare vs-written=0.19 vs-world-row0=0.00 vs-bind-inverse=563.96
```

`vs-world-row0=0.00` 看着像"缓冲区存的就是世界矩阵"的铁证，其实**是循环论证**——
`buffer` 那一行读的是**我们自己刚写进去的值**，所以它当然匹配我们自己。
上一轮我据这个数字推断"缓冲区语义是世界矩阵、之前写错了"，**那个判读不成立**，此处更正。

写入后读回只能验证写入。还需测量实际 pass 前后及 GPU 内容；“写入格式正确”也须独立证明，不能由当前自比较推出。
旧方案的 `507.73／1.45` 分类值未在本归档中找到，相关原件待补；归档有 52 条 compare 和 26 条 `probe-effective=false` 报告，没有 true。

## 仍未排除的假设

1. 顶点着色器采样的是**另一份**骨骼数据（GPU 侧独立缓冲，不经 CPU 侧数组）；
2. 或者采样的是本数组的**另一份副本**（例如 `prevBoneMatrices` 或按帧号切换的双缓冲），
   当前写入落在非活动的那个。
3. 修改在后续 pose／引擎准备中被覆盖或重建，日志读取的位置未处于实际消费边界。
4. 目标几何不是真正可见的身体 draw，或所改槽位没有有效顶点权重／palette 映射；矩阵布局、坐标空间与缩放也未完整分类。

两者都需要图形调试器逐次比对才能区分（本次调查只抓过一次 `BonesBuffer`），
**不适合再用"改一处、进游戏试一次"的方式推进**。

## 探针自身遗留的缺陷（影响否定结论有效性）

- `probe-effective=false` 的原因未确证。见证点必须在同一受控子树，且离旋转轴有非零半径；点与 pivot 重合／位于转轴上时，位置不变可能正确。
  当前代码从目标骨骼的直接子节点中选见证点，不能单凭名字或两个 skin root 推出“属于另一套骨架”。需记录父链、身份和朝向来判别。
- `Hands [Ovl0]` 一行出现 `probe-slot=38`（= 槽位总数，即**未找到**）与 `slot-row0=0.000`，
  说明探针骨骼不在该 skin 的骨骼列表里——这是预期的（它属于另一套骨架），不是缺陷。

## 相关离线验证

摆动与矩阵写入的数学有离线断言覆盖（`tests/snapshot_transform_test.cpp`，`ctest` 1/1）：
摆动绕骨骼自身原点、原点不动、朝向精确前进请求角度、子关节弧长 `2r·sin(θ/2)`、
绕父级原点必定移动骨骼（回归锁）、写入的矩阵复现骨骼自身变换且 48 字节步长不互相覆盖。
每条都做过变异验证（改错即变红）。

这些是离线数学／写入测试的报告；不覆盖实际调用顺序、引擎缓存更新、GPU 上传和着色器消费，不能单独证明矩阵语义或动作驱动可行性。
