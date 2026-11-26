# 弓 / Axe equip 姿态错位：离线取证（2026-11-26）

> 用户报告：在背包内切换武器时，**弓**和 **Axe** 的 Equip 动画播放期间「一些骨骼出现错误的矩阵变换」。本文只用离线 HKX 取证与既有运行日志，**不含修复代码**。

## 一、现象定位（运行日志）

v128 上机会话（[日志](../CharacterPanel-v128-dispel-20261126.log)）里，所有已加载资产的 `asset ready` 行按 `mapping` 与轨道数分组，只有三个资产的轨道数达到 116：

| mapping | tracks | resolved | 资产 |
| --- | ---: | ---: | --- |
| identity-unverified | **116** | 112 | `Gunslicer Animations OAR\Bow\bow_equip.hkx` |
| identity-unverified | **116** | 112 | `Gunslicer Animations OAR\Sword\1hm_equip.hkx` |
| identity-unverified | 107 | 104 | `WS\Staff\…\staffright_equip.hkx` |

其余全部是 `tracks=99`（Smooth Moveset 的 idle / equip base、`mt_idle`、`1hm_equip` vanilla 等），其中 54 条 `mt_idle` 走 `mapping=table`。

**用户报告的两个场景正是上表前两行。** 法杖（107 轨）用户未报告，但它属于同一族，后续需一并观察。

> 注意：日志里的 `tracks=` 是 `bindings.size()`（[clip.cpp:252](../../../src/animation/clip.cpp)），即经 `min(track_count, bone_count)` 截断后的值，**不是 HKX 的原始轨道数**。

## 二、HKX 取证（离线解包）

使用既有工具链 `build/hkx-audit/HkxAnimDump`（HKX2E）读取原始文件。

| 资产 | HKX `numberOfTransformTracks` | `hkaAnimationBinding.transformTrackToBoneIndices` | 运行时 `tracks` |
| --- | ---: | ---: | ---: |
| **Gunslicer `Bow\bow_equip.hkx`** | **242** | **0（空）** | 116 |
| **Gunslicer `Sword\1hm_equip.hkx`** | **156** | **0（空）** | 116 |
| Smooth Moveset `Bow\…\bow_idleheld.hkx` | 99 | 0（空） | 99 |
| Beam Walking Assist `bow_equip.hkx` | 99 | 0（空） | — |
| Diverse NPC Movesets `bow_equip.hkx` | 99 | 0（空） | — |

三条关键事实：

1. **所有被查资产（包括正常的）绑定表都是空的**，即 `transformTrackToBoneIndices` 长度为 0。所以 `mapping=identity-unverified` 在这套数据里是**普遍状态**，不是异常信号。
2. **只有 Gunslicer 的两个 equip 资产轨道数超过 116**（242 和 156）。其他模组的同名 `bow_equip.hkx` 都是 99。
3. 运行时把 242 / 156 都截断成 116，因为 [clip.h:109](../../../src/animation/clip.h) 用 `min(track_count, bone_count)`。

## 三、错位的机制

`resolve_animation_tracks`（[clip.h:106-120](../../../src/animation/clip.h)）在绑定表为空时走 identity 回退：

```cpp
const std::int32_t bone = table.empty() ? static_cast<std::int32_t>(track) : table[track];
```

即**假定 track 序号 == bone 序号**。这个假定成立的条件是：轨道数 ≤ 骨骼数，且轨道顺序严格等于骨骼顺序。

Gunslicer 的两个资产违反了第一个条件（242 > 116、156 > 116）。截断到 116 之后，代码把**前 116 条轨道**当作**116 根骨骼**逐一写入。因为原始轨道布局里有 126（242−116）/ 40（156−116）条多余轨道，前 116 条与 116 根骨骼之间不存在一一对应，于是骨骼矩阵被写错。

对照：99 轨资产不会触发，因为 99 < 116，且这些资产本身就是按标准骨架的前 99 条轨道排列的（`mt_idle` 用 `mapping=table` 时 `map-len=99` 也印证了 99 是这个骨架的标准轨道数）。

> **未解**：为什么 Gunslicer 的资产会有 242 / 156 条轨道。242/116 = 2.086、156/116 = 1.345，都不是整数倍，所以不是简单的「骨骼重复两遍」。HKX2E 只有反序列化、没有采样 API，因此无法离线解出逐轨的实际变换值来确认多出来的轨道承载什么。这一点需要另一次取证（见 §五）。

## 四、已排除的解释

| 假设 | 排除依据 |
| --- | --- |
| `Belly` / 控制骨解析失败导致 | `Belly` 只是 `tracks=116` 资产里 4 个未解析项之一（index 115）；99 轨资产同样有 3 个未解析项（`x_NPC Look/Pos/Rot`）却工作正常。未解析项在 [clip.cpp:239-245](../../../src/animation/clip.cpp) 里是**跳过**（`nodes[i]` 留空，采样时 `continue`），不会写出错误矩阵。 |
| 名字映射错 | `POSE MAP` 行显示 track 8 → 骨骼 8 → `NPC L Foot [Lft ]` 等**自洽**，说明按名解析这一环没有问题。 |
| `compose_contributor` 过滤 | `BoneMask::e_full_body` 直接放行（[sequence.h:39](../../../src/animation/sequence.h)），无过滤。 |
| `recompute_subtree_worlds` 层级错误 | 标准 `world * local` 递归（[snapshot_transform.h:22-38](../../../src/character/snapshot_transform.h)），逻辑正确；它只是把上游错误逐级放大。 |
| 只是显示路径（`DisplayPose=true`）的问题 | 用户会话是 `DisplayPose=false`，走的是 `sample_animation_clip` + `compose_contributor`（[controller.cpp:1283-1285](../../../src/animation/controller.cpp)）。 |

## 五、取证缺口（修复前需要补）

1. **242 / 156 条轨道的实际语义**。需要带采样能力的工具（HKX2E 之外的 HavokLib 或自写解析）解出逐轨 `hkQsTransform`，判断：
   - 多出的轨道是否对应额外骨骼（例如物理骨、附加挂点）；
   - 前 116 条是否**恰好**就是 116 根骨骼的姿态（若是，则截断其实是安全的，错位另有原因）。
   这一条不解决，任何修复都是猜。
2. **法杖（107 轨）**是否也复现。用户在场上未报告，但同属超 99 的少数族。
3. ~~**运行时真实轨道数**~~ —— **已实现（v130）**。`out_coverage` 现在额外输出 `hkx-tracks`（原始 `numberOfTransformTracks`）与 `skeleton-bones`，`tracks` 字段含义不变。构建身份 `…-v130-track-diag-20261126-d1278da4d9-…`（DLL `bd1246e5…`），已部署，上机步骤见 [v130-run](v130-run.md)。判读要点：弓应显示 `hkx-tracks=242`、Axe（`1hm_equip`）应显示 `156`；若显示 116，说明引擎自身已规整轨道数，错位另有原因。

## 六、候选修复方向（未选定、未实现）

按风险从低到高：

| 方向 | 做法 | 优点 | 风险 |
| --- | --- | --- | --- |
| A. 拒绝不可信 identity | 当 `track_count > bone_count` 时判定 mapping 不可信，保留上一有效姿态 | 绝不写错矩阵 | equip 动画不播 |
| B. 只排除非骨骼轨道 | 跳过已知非骨骼（`x_NPC *`、`Belly`），其余照旧 | 动画仍可见 | 仍依赖「序号等于骨骼」 |
| C. 离线确认后按真实对应修 | 先补 §五.1 取证，再决定映射规则 | 根治 | 工作量最大 |

用户当前选择：**先补 §五.1 的离线取证，再定方案**。

## 七、取证环境

| 工具 | 路径 | 状态 |
| --- | --- | --- |
| `HkxAnimDump` | `build/hkx-audit/HkxAnimDump`（HKX2E，net10.0） | 已存在，可直接跑 |
| `DumpTracks` | `build/hkx-audit/DumpTracks` | 本次新建，打印绑定表与注释轨 |
| `DumpSpline` | `build/hkx-audit/DumpSpline` | 本次新建，打印 `numberOfTransformTracks` / 轨数比 |

三者都在 `build/`（被 `.gitignore` 排除），不入库。

## 八、变更记录

- 2026-11-26（v130）：补上运行时原始轨道数诊断（`hkx-tracks` / `skeleton-bones`），见 [v130-run](v130-run.md)；已构建部署，待上机。
- 2026-11-26：首次离线取证。确认只有 Gunslicer 的 `Bow\bow_equip.hkx`（242 轨）与 `Sword\1hm_equip.hkx`（156 轨）超出 116 骨骼数，与其他模组同名资产（99 轨）形成对照；绑定表全空说明 identity 回退是常态而非异常；排除名字映射与 mask 过滤两类假设；记录三项取证缺口与三个候选方向。
