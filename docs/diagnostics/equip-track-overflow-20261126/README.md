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

## 四之二、v130 上机结果（2026-11-26/27）

日志 10,788,879 B，SHA-256 `6c9f44fe58236aa7b3cbc57be22219147420eed415309a3f6609c769b61ed3c5`（会话 23:42 起）。首行 `SCENEGRAPH BUILD` = `…-v130-track-diag-20261126-d1278da4d9-…`，与本轮部署身份一致。

**核心判据命中**：引擎读到的**就是**原始轨道数，不是 116。

| `tracks`（截断） | `hkx-tracks`（原始） | `skeleton-bones` | 行数 |
| ---: | ---: | ---: | ---: |
| 99 | 99 | 116 | 312 |
| 116 | **156** | 116 | 15 |
| 116 | **242** | 116 | 8 |

即 `min(track_count, bone_count)` 确实把 242 / 156 截成了 116，**离线推断成立**。

**溢出范围比用户报告更广**（新信息）：

| `hkx-tracks` | 资产 |
| ---: | --- |
| 242 | `Gunslicer/Bow/bow_equip.hkx` |
| 242 | `Gunslicer/Bow/bow_unequip.hkx` |
| 156 | `Gunslicer/Sword/1hm_equip.hkx` |
| 156 | `Gunslicer/Sword/1hm_unequip.hkx` |
| 156 | `Gunslicer/Sword/1hm_idle.hkx` |

用户只报告了 `bow_equip` 与 `1hm_equip`。`bow_unequip`、`1hm_unequip`、`1hm_idle` 同属溢出族——**`1hm_idle` 尤其值得注意**，因为它是持续播放的待机而非一次性过渡，若也错位，观感影响比 equip 更大。建议下次一并目视确认。

全部溢出资产都属于 **Gunslicer OAR Animations Pack**；同目录其他模组的同名资产仍是 99。

## 四之三、更正：HavokLib 的类型判定不可信

本轮尝试用 `build/hkx-audit/HavokLib`（带 `GetTransformOffsets` / `DumpTrack` 本地补丁）解逐轨数据，发现：

- HavokLib 把 `bow_equip.hkx` 报成 `HK_WAVELET_COMPRESSED_ANIMATION`，把 `bow_idleheld.hkx` 报成 `HK_SPLINE_COMPRESSED_ANIMATION`；
- 但两个文件的字符串表里**都只有 `hkaSplineCompressedAnimation`，wavelet 出现 0 次**；
- 因此 **HKX2E 的判定才是对的**，HavokLib 对该 packfile 的 layout 识别有偏差。

连带影响：HavokLib 报出的 `transformOffsets=0` **不能采信**（两个文件都报 0，包括已知正常的 99 轨资产）。先前若据此推断「per-track 表缺失」会得出错误结论。

`TrackTableProbe` 工具已留在 `build/hkx-audit/HavokLib/trackprobe/`（`build/` 被 gitignore），但它当前的 spline 数据不可用，**逐轨语义仍需另找可信路径**。

## 五、取证缺口

1. ~~242 / 156 条轨道的实际语义~~ —— **已由 §五之二关闭**：分量展开布局，多出的轨道是 scale 分量段。
2. **`1hm_idle.hkx`（156 轨）是否也可见错位** —— 它是持续待机而非一次性过渡，用户尚未报告，但同属溢出族，需要一个目视确认。
3. ~~运行时真实轨道数~~ —— 已由 v130 关闭。

## 五之二、逐轨 mask 解码结论（2026-11-27）

用扩展后的 mask 探针（`SplineMaskProbe`，见 §七）逐轨解码三个资产，得到决定性结构：

| 事实 | 数值 / 内容 |
| --- | --- |
| 旋转动画止于 | **track 96**（bow 242 轨与 idle 99 轨**相同**） |
| 前 97 条与标准布局 | 完全一致（rot 分量逐位吻合） |
| track 97–98 | 标准 = `rot=STA`；异常 = `rot=ID` + track 98 引入 `scale=DYN` |
| track 99 之后 | 无结构边界，STA/ID/DYN 混合延续到文件尾 |
| `scale=DYN` 轨道数 | bow 107（头 41 + 尾 66）、axe 12；**idle 99 轨资产 = 0** |
| 头 99 一致率 | axe 75/99、bow 44/99（差异几乎全在 scale 分量） |

**结论：多出的轨道不是"附加骨骼"，而是把骨骼的 scale 分量展开成了独立轨道。**

- 这份资产采用"分量展开"（per-component）的轨道布局：每骨的 pos/rot/scale 各自占轨，超出 116 的部分是 scale 段的延续，不是新骨骼。
- 标准 Skyrim HKX（含 `mt_idle`、Smooth Moveset 全部资产）用紧凑布局：一轨一骨、scale 走绑定默认，所以 99 轨就够。
- 游戏与我们插件的 `hkaAnimationBinding.transformTrackToBoneIndices` 都为空，双方都按"一轨=一骨"假设。**前 97 条恰好对得上**（此时两套布局的骨骼序恰好一致），track 97 之后进入附加骨/挂点/物理骨区（Weapon/Quiver/Shield/Belly 等），错位从这里开始并逐轨放大——与"头部骨骼尚可、越往后越错"的用户观感吻合。

> **此结论基于 HavokLib 的 mask 解码**。其类型枚举对该 packfile 不可信（§四之三），但 mask 解码走的是同一份 spline block 数据 + `hka_spline_decompressor.cpp` 的格式规则，且解码出的前 97 条与已知正确的 99 轨布局逐位吻合——这是格式正确性的直接证据。若要完全消除疑虑，可用 CK/3ds Max 打开资产复核，但前缀吻合已经构成强对照。

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

- 2026-11-27（逐轨解码）：**多出轨道定性为 scale 分量段**——前 97 条与标准布局逐位一致、旋转动画止于 track 96、异常资产的 `scale=DYN` 轨道（bow 107 / axe 12）是健康资产（0）没有的，见 §五之二。取证缺口 1 关闭，修复方向可以进入选择。
- 2026-11-27（v130 上机）：**引擎确认读到 hkx-tracks=242 / 156**，截断假设成立；溢出范围扩至 5 个 Gunslicer 资产（含 unequip 与 1hm_idle）。同日发现 HavokLib 类型判定错误，逐轨语义仍未解，见 §四之三。
- 2026-11-26（v130）：补上运行时原始轨道数诊断（`hkx-tracks` / `skeleton-bones`），见 [v130-run](v130-run.md)；已构建部署，待上机。
- 2026-11-26：首次离线取证。确认只有 Gunslicer 的 `Bow\bow_equip.hkx`（242 轨）与 `Sword\1hm_equip.hkx`（156 轨）超出 116 骨骼数，与其他模组同名资产（99 轨）形成对照；绑定表全空说明 identity 回退是常态而非异常；排除名字映射与 mask 过滤两类假设；记录三项取证缺口与三个候选方向。
