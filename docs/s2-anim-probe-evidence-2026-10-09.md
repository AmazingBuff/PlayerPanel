# S2 动作探针：诊断证据（2026-10-09）

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
