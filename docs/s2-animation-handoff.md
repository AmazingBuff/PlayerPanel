# S2 动画：HKX／引擎动画路线侦察与下一工作包（下一个会话从这里开始）

- 日期：2026-10-09（本轮以文档收尾，未再改代码）
- 读者：下一个会话的 Agent / 开发者。本文是 S2（FR-03 动作）的**当前入口**；机制证据在
  [探针证据](s2-anim-probe-evidence-2026-10-09.md)，历史方案在 [S2/S3 方案](s2-s3-plan.md)，
  测试单在 [下一轮测试单](scene-graph-copy-next-test-2026-10-09.md)。

## 0. 一句话状态

副本**写骨骼能驱动画面**这一点已被四轮游戏内实测确证；但当前**程序化待机没有真值、无法判定对错**，
用户已明确要求改为**用真实动画数据（HKX）驱动并可对比验证**。本工作包 = 用引擎自己的采样器播放
真实动画并驱动副本，附**数值 A/B 对比协议**；程序化待机降级为"没有可用动画时的兜底"，**不再调参**。

**进展（2026-10-10）**：HKX1 的**第 1 步（骨架对齐）已实测通过**——引擎的动画数据能叫出副本的节点：第三人称图
`DefaultFemale`（`root=true`）的骨架 **109/116 精确命中、零歧义**，缺的 7 个全是 Havok 帮手骨（`x_` 前缀）与装备
附着骨（`Shield`/`Weapon`/`Quiver`/`Belly`）；骨架父子关系与 NIF 层级一致，写 local 由层级合成成立。**负结果**：
`boneNodes` 的次序 ≠ 动画骨架的次序（同名率 37/116），"用 `boneNodes[i]` 指针对应"因此作废，按名匹配是必需的
（[HKX1 实测证据](s2-hkx1-alignment-evidence-2026-10-10.md)）。

**HKX2 也已实测通过**：把引擎自己的 `poseLocal` 写进副本，旋转复现到 **0.03–0.10°**、位置残差 **≤1.6 单位**、
对照（先摆偏 0.5 rad）**125–199 单位**、复原 **0.000** —— 四元数约定 = 直接式；**第二个负结果**：`poseLocal` 按
**`boneNodes` 次序**索引，不是骨架次序（[HKX2 实测证据](s2-hkx2-replay-evidence-2026-10-10.md)）。

**最新（2026-10-10 深夜）**：HKX12 已实测——`GetHashedAnimFromAnimIndex(graph, 1022)` 调用成功，
但返回的是 **`&hashedAnimations[1022]`（`HashedData` 按值条目，0x20、纯文件信息、无指针）**，`+0x28`
读到的是下一条目的 CRC——**"名字/索引注册"与"已装载数据"分离实锤**（见
[HKX12 实测证据](s2-hkx12-hashed-evidence-2026-10-10.md)）。三轮（HKX10-12）已把注册表层完全看清：
binding set = 空桩、hashedAnimations = 文件信息、已装载数据只在管理器记录的 ptr1/ptr2（未跟进）或
管理器 unk68/unk88（未探）。**当前状态：无待验证代码；下一步待用户拍板——HKX13（最后一跳探针：
跟进 ptr1/ptr2 + CRC 点名，检查点：拿不到即转 D）还是直接转路线 D（poseLocal 录制-回放，机制全已
验证）**。

## 1. 方向修正（用户批评，已接受）

用户原话要点：既然已经知道骨骼能移动，为什么不直接用简单的 HKX 测试？HKX 还能对比；硬编码的
idle 完全没法测试。

**接受。** 复盘：程序化待机的正弦通道**没有基准**——它只能证明"能动"（这正是前面四轮已证明的），
不能证明"动对了"；任何判断都退化成观感，于是出现了"腰部好像没动"这种无法界定的反馈。真实动画有
作者意图、有可对比参照（源角色播放同一动画），因此**可判定**。结论：
**停止在 idle 参数上迭代；把数据源换成引擎播放／采样的真实动画。**

（附：idle 那轮暴露的问题本身是真实的设计缺口，已修并留档——骨盆与躯干在本骨架里是**兄弟分支**
（`CME LBody` / `CME UBody`），且绕骨骼自身原点的旋转不移动该骨骼本身，所以胯部必须带**平移**通道。
改动画驱动后这个缺口自然消失：我们将驱动**全部**骨骼而不是几个关节。）

## 2. 已确证的机制（不要重复验证）

| 结论 | 证据 |
| --- | --- |
| 写副本节点 → 画面随动（两个身形、两套骨架） | S2P1/S2P2 日志与用户目视；`probe-effective` 全部 true |
| 执行顺序必须固定为：基础摆位 → 写骨骼 → pass 准备 → draw | PRD §6.3；探针移入绘制窗口后成立 |
| 引擎在**绘制中**按我们写的节点重算蒙皮矩阵（缓冲 3×3 = 节点世界旋转 × 0.35 图形缩放，匹配摆动后姿态，残差 0.0002） | S2P4 原始转储 + [analyze_tslot_scale.py](diagnostics/analyze_tslot_scale.py) |
| F2 开关配对：开＝动、关＝复位静止 | S2P3 用户目视（R06） |
| **警示**：蒙皮缓冲槽位下标 **≠** `bones[]` 下标（自匹配 5/31、4/71） | S2P4 + [analyze_tslot_mapping.py](diagnostics/analyze_tslot_mapping.py) |

## 3. 侦察结果：引擎侧可取用的对象（本次读的是本仓 CommonLibSSE checkout）

**结论：不需要自己写 HKX 解析器。** 引擎已把"加载→解码→采样"做完，CommonLibSSE 暴露了类型与偏移，
我们只需**选一个动画、用自己的时钟推进、调用引擎的采样、按轨道→骨骼映射写进副本**。

| 能力 | 位置 |
| --- | --- |
| 引擎自己的采样函数（任意时间点取姿态） | `hkaAnimation::SampleTracks(time, hkQsTransform* out, float* floatTracks, hkaChunkCache*)` — [hkaAnimation.h:44](extern/CommonLibSSE/include/RE/H/hkaAnimation.h#L44) |
| 轨道 → 骨骼索引映射 | `hkaAnimationBinding::transformTrackToBoneIndices` — [hkaAnimationBinding.h:26](extern/CommonLibSSE/include/RE/H/hkaAnimationBinding.h#L26) |
| 动画骨架（骨名＋父索引，用于与副本节点对齐） | `hkbCharacterSetup::animationSkeleton` — [hkbCharacterSetup.h:25](extern/CommonLibSSE/include/RE/H/hkbCharacterSetup.h#L25)；`hkaSkeleton::{bones,parentIndices}` — [hkaSkeleton.h:33](extern/CommonLibSSE/include/RE/H/hkaSkeleton.h#L33) |
| 可自驱的播放控制（公开 `localTime`） | `hkaAnimationControl`：`localTime`／`binding`／`SampleTracks(...)` — [hkaAnimationControl.h:23-38](extern/CommonLibSSE/include/RE/H/hkaAnimationControl.h#L23) |
| 引擎的 clip 对象（含动画名与播放模式） | `hkbClipGenerator`：`animationName`／`binding`／`animationControl`／`localTime`／`time`／`playbackSpeed`／`mode` — [hkbClipGenerator.h:66-96](extern/CommonLibSSE/include/RE/H/hkbClipGenerator.h#L66) |
| 骨索引 → NiNode 表（引擎自带映射） | `BShkbAnimationGraph::boneNodes`（`BoneNodeEntry{ NiNode* node; … }`）— [BShkbAnimationGraph.h:38-43](extern/CommonLibSSE/include/RE/B/BShkbAnimationGraph.h#L38)、成员在 0x160 |
| 引擎每帧生成的姿态缓冲 | `BShkbAnimationGraph::generatorOutputs[2]`（0x220）；结构见 [hkbGeneratorOutput.h](extern/CommonLibSSE/include/RE/H/hkbGeneratorOutput.h) |
| 角色的当前 local 姿态 | `hkbCharacter::poseLocal`／`numPoseLocal` — [hkbCharacter.h:53-54](extern/CommonLibSSE/include/RE/H/hkbCharacter.h#L53) |
| 取图路径 | `BShkbAnimationGraph::characterInstance`（0xC0，`hkbCharacter`）；`hkbCharacter::{setup(0x50), animationBindingSet(0x68), behaviorGraph(0x58)}` — [hkbCharacter.h:45-48](extern/CommonLibSSE/include/RE/H/hkbCharacter.h#L45) |
| 手动推进动画图（可选） | `IAnimationGraphManagerHolder::UpdateAnimationGraphManager(const BSAnimationUpdateData&)` — [IAnimationGraphManagerHolder.h:55](extern/CommonLibSSE/include/RE/I/IAnimationGraphManagerHolder.h#L55) |

### 已实测（2026-10-10，逐条证据见 [HKX1 实测证据](s2-hkx1-alignment-evidence-2026-10-10.md)）

- 取图路径在 AE 1.6.1170 上成立：`TESObjectREFR::GetAnimationGraphManager` → 两个图（`DefaultFemale` root=true /
  `FirstPerson` root=false，`holder`/`root` 判据可用），女性工程已加载 **15203** 条 binding，`poseLocal` 116 项。
- 骨架 ↔ 副本节点：**109/116** 精确命中、`ambiguous=0`；缺的是 `x_` 帮手骨与装备附着骨（`Shield`/`Weapon`/`Quiver`/`Belly`），
  CBBE 极简体型 90/116。
- **`boneNodes[i]` 与 `bones[i]` 不同序**（同名率 37/116、78/99）→ 指针对应作废；**`poseLocal[i]` 的对应次序也必须实测**，不能假设。
- 骨架父子关系与 NIF 层级一致（`parent-ancestors=matched`、`wrong-parent` 空）→ 写 local、由节点层级合成成立。
- **HKX2（2026-10-10）**：`quat=direct` 把姿态复现到 0.03–0.10°，`quat=transposed` 给出 165–180°（即逆旋转）→ 四元数约定确定。
- **`poseLocal` 按 `boneNodes` 次序索引**：`bone-nodes/direct` 0.529u 优于 `skeleton/direct` 5.591u → 拿它当真值时必须按
  该表索引；动画轨道的骨索引是**骨架**索引，两者只能靠**骨名**桥接。

### 在 AE 上不可用的两条现成 API（2026-10-09 复核，规划时排除）

- `BSAnimationGraphManager::QueryAnimations`（两个重载）与 `RE::AnimationSystemUtils` 的**全部函数**在这个
  CommonLibSSE checkout 里 AE 地址都是 `0`（`RELOCATION_ID(62432, 0)`、`RELOCATION_ID(31942, 0)` …）。
  `REL::RelocationID` 在 AE 构建下取 `_aeID`，为 0 时 `address()` 返回 0（[REL/ID.h:71-81](extern/CommonLibSSE/include/REL/ID.h#L71)），
  **调用即跳到 0**。本项目只有 AE 1.6.1170，所以"用引擎现成 API 列出 clip 名／clip 信息"这条路不可规划。
- `hkbStateMachine::StateInfo` 的成员全是 `unk30…unk70`，`hkbAnimationBindingWithTriggers` 只有前向声明：
  遍历 generator 树找 `hkbClipGenerator`、或读 `animationBindingSet::bindings` 的元素，**都要先做运行时布局核对**。
  有类型的只有 `hkbBehaviorGraph::rootGenerator`／`hkbStateMachine::states`／`hkbClipGenerator` 的字段／
  `hkaAnimationBinding` 的字段／`hkaAnimationControl`／`hkaSkeleton`／`BShkbAnimationGraph::{characterInstance,boneNodes}`／
  `BSAnimationGraphManager::graphs`。

### 必须验证的未知项（不要当成已知）

1. **`hkbAnimationBindingWithTriggers` 在 CommonLibSSE 里只有前向声明、没有头文件**
   （[hkbAnimationBindingSet.h:11](extern/CommonLibSSE/include/RE/H/hkbAnimationBindingSet.h#L11)），
   而 `hkbAnimationBindingSet::bindings` 是它的指针数组（[:21](extern/CommonLibSSE/include/RE/H/hkbAnimationBindingSet.h#L21)）。
   → 走这条路前必须**运行时核对布局**（例如用两个实例交叉验证），否则优先走有类型的
   `hkbClipGenerator::binding` 或 `hkaAnimationControl`。
2. **暂停时行为图是否 tick**：本项目的既有结论是暂停时引擎计时器不推进、世界动画不前进；
   "我们自己调 `SampleTracks`"因此不需要图在跑（这是首选路径的原因），但"读引擎每帧生成的姿态
   （`generatorOutputs`／`poseLocal`）"在暂停时是冻结的——若要那条路，需要记录（未暂停时）＋回放（暂停时）。
3. `hkaChunkCache`：`SampleTracks` 的最后一个参数可为空的假设**未验证**（spline 压缩动画可能需要它）。
   若需要，得看 `hkaChunkCache` 是否可以自行分配。
4. 轨道数 vs 骨骼数：动画轨数可能少于骨架骨数（未动画的骨保持参考姿态），映射后必须**只写有轨道的骨**，
   其余保持捕获姿态。

## 4. 三条取数路线

| 路线 | 形态 | 成本／风险 | 真值与对比 |
| --- | --- | --- | --- |
| **A. 引擎已加载的动画（推荐先做）** | 从角色取 `hkbClipGenerator`／`hkaAnimationControl`，用我们的时钟设 `localTime`，`SampleTracks` 后写副本骨骼 | 中；未知项 1/3 需在游戏内确认 | 同一动画、同一采样函数；可与源角色的同一动画做**数值 A/B** |
| **B. 用户提供的 HKX 文件** | 文件由**引擎**加载（动画 mod 常规路径：替换/注册到行为工程或动画事件），我们仍走 A 的采样链 | 中＋素材流程；需要用户确认文件放哪、如何让引擎加载 | 同上；文件可换、可对比 |
| **C. 自研 HKX 解析器** | 自己读 packfile（`hkaSplineCompressedAnimation` 等）并采样，不依赖引擎加载 | 高（packfile＋spline 解码自研） | 最强自由；但真值仍需与引擎播放同一文件对比 |

**推荐：先 A（一次会话即可验证映射与采样链是否成立），随后把 B 的同一个文件接进同一条链。**
C 只在 A/B 无法满足需求时再考虑。

## 5. 下一工作包：HKX1 构建（一次构建、一次会话，要么播起来、要么明确缺哪一环）

目标产物：`CHARACTER_PANEL_SCENE_COPY_EXPERIMENT=ON` 的测试包，新增 `SCOPY ANIM …` 系列日志。

1. **骨架对齐（先做，失败即停；已实现并实测，见下"实施与实测结果"）**：从 `BShkbAnimationGraph::characterInstance → setup->animationSkeleton`
   取骨名表，与副本节点名**精确**逐条比对，打印 `SCOPY ANIM skeleton …` 与 `SCOPY ANIM gate verdict=…`。
   映射不成立 → 不进入下一步（差集与计数在同一行里）。
2. **取动画**，按优先级并各自打印证据：
   ① `hkbClipGenerator`：在行为图里按 `animationName` 找（优先 Idle）；打印 `source=clip:'…' duration=… tracks=… mode=…`；
   ② `hkaAnimationControl`：直接用其 `localTime`＋`SampleTracks`；
   ③ 最后才尝试 `hkbAnimationBindingWithTriggers`——**碰之前先做布局交叉核对**并在日志里写明核对结果。
3. **采样与写入**：`localTime = f(steady_clock)`（循环模式，周期 = `animation->duration`／`playbackSpeed`）
   → `SampleTracks` 得 `hkQsTransform[]` → 按 `transformTrackToBoneIndices` 取骨索引 → 骨名 → 副本节点
   → 写 **local** 变换（`NiTransform`），随后复用已验证的子树重算（`apply_world_delta_downward`）。
   只写有轨道的骨，其余保持捕获姿态。
4. **热键语义**：**F2 = 播放/停止动画驱动**（默认开启）；`F6` 探针仅在
   `-DCHARACTER_PANEL_S2_PROBE=ON` 的诊断构建里存在（本机 F6 被其他插件占用）。
   程序化待机保留为**兜底**：找不到动画或映射失败时自动回退，并在日志里说明回退原因（不再调参）。
5. **失败面必须可读**：任何一环失败都要打印**具体**原因（哪个指针为空、布局核对结果、匹配率、缺哪些骨），
   不允许静默跳过——下一轮的依据必须是数据而不是猜测。

### 实施状态：第 1 步已落地（2026-10-09，待游戏内一轮）

`src/character/animation_source.{h,cpp}`，由 `SceneGraphCopy::capture()` 在审计通过后调用一次（只在暂停的背包里发生）：

- **只读**：不写源角色、不写副本、不写引擎的图。特别地**不碰** `hkbClipGenerator::animationControl`——那是
  **源角色**的对象，写它的 `localTime` 等于写世界角色的引擎状态（FR-05）；将来采样要走
  `binding->animation->SampleTracks(t, …)` 这种**带显式时间**的 const 调用（`hkaAnimationControl::SampleTracks`
  没有时间参数、按自身 `localTime` 采样，两者得到的结果在全身单 clip 上一致，但后者要先改源对象的状态）。
- 日志（每次 F7 打印一组）：

```
SCOPY ANIM source form=<id> graphs=N copy-nodes=N
SCOPY ANIM graph[i] project='…' holder=… root=… bone-nodes=N anim-bones=N behavior-graph=… root-generator='…' binding-set=… bindings=N pose-local=N
SCOPY ANIM skeleton graph=i name='…' bones=N bone-nodes=N bone-node-names-agree=K/N matched=N ambiguous=N duplicate-nodes=N parent-ancestors=N missing='…' wrong-parent='…'
SCOPY ANIM gate verdict=… graph=i matched=N/N ambiguous=N
```

- 判读：`matched` = 动画骨架里**精确**命中副本节点名的骨数；`missing` 是差集样本（最多 8 个名字，计数才是全量）。
  `verdict=PASS`（全部命中且无重名歧义）／`PASS-AMBIGUOUS`（全部命中，但有骨名对应多个节点——改装头发/衣物的
  链常常重名）／`INCOMPLETE`（有骨找不到节点）／`UNAVAILABLE`（点名缺的是哪个指针）。
  `bone-node-names-agree` 量的是"引擎自己的 `boneNodes` 是否与动画骨架同序"——将来用**指针对应**替代按名匹配
  就靠它。`parent-ancestors`／`wrong-parent` **只作观测、不参与 gate**。实现时曾推测"脊柱挂 `CME UBody`、骨盆挂
  `CME LBody`，所以骨架与节点两套层级必然不一致"——**实测推翻了这个推测**：`wrong-parent` 为空，动画骨架的父子
  关系与 NIF 层级一致（每个骨的骨架父骨都是该节点的祖先），所以写 local 变换、由节点层级合成成立。

### 实施与实测结果（2026-10-09 实现，2026-10-10 实测）

`verdict=INCOMPLETE`，但差集全是 `x_` 帮手骨与装备附着骨——**核心 109/116 全中、零歧义**，说明这个门槛定得过严
（它要求"每根骨都有节点"）。下一轮的判据改为按类计数（`x_` 帮手骨 / 附着骨 / 核心骨）：**核心骨缺一根才算失败**，
其余打印跳过清单。`bone-node-names-agree=37/116` 这条负结果已记入 §3。

### HKX2（已实测通过，2026-10-10）：用引擎自己的 `poseLocal` 做带对照的数值 A/B

引擎已经把当前姿态放在 `hkbCharacter::poseLocal`（116 项 `hkQsTransform`，实测非空），这是**全类型**的输入，
不必碰任何未定型布局。每次 F7 在 HKX1 的对齐报告之后多打印一组：

```
SCOPY ANIM map graph=0 bones=116 matched=109 helper-unresolved=3 other-unresolved=4
SCOPY ANIM replay pose entries=116 scale-off-from-one=N
SCOPY ANIM replay control max-pos-delta=… max-rot-delta=…deg bones=109
SCOPY ANIM replay order=skeleton quat=direct written=109 max-pos-delta=… max-rot-delta=…deg bones=109
SCOPY ANIM replay order=skeleton quat=transposed written=109 max-pos-delta=… max-rot-delta=…deg bones=109
SCOPY ANIM replay order=bone-nodes quat=direct written=109 max-pos-delta=… max-rot-delta=…deg bones=109
SCOPY ANIM replay order=bone-nodes quat=transposed written=109 max-pos-delta=… max-rot-delta=…deg bones=109
SCOPY ANIM replay restored max-pos-delta=… max-rot-delta=…deg bones=109
SCOPY ANIM replay verdict match=skeleton/direct control=…u best=…u
```

- **实测（2026-10-10，两次捕获：CBBE 90 命中、UBE 109 命中）**：`quat=transposed` 两次都给出 165–180°（逆旋转）
  → 约定 = `direct`，旋转复现到 **0.03–0.10°**；`bone-nodes/direct` 位置残差 **0.529u** 优于 `skeleton/direct` 的
  5.591u → **`poseLocal` 按 `boneNodes` 次序**；`control` 124.7/199.0u、`restored` **0.000u** → 测量灵敏、面板被逐字复原。
  capture 1 的 `match=none` 只是 `best/control=1.29%` 略高于 1% 相对门槛，**看排名即可**。残差是位置、且很小
  （≤1.6u、旋转≈0），最可能是缩放语义（`scale-off-from-one=4/7`；`NiTransform` 只有一个缩放，姿态有三个）——
  下一轮把"保留节点缩放"加为第二个缩放候选即可。逐条见
  [HKX2 实测证据](s2-hkx2-replay-evidence-2026-10-10.md)。
- **判读规则（仍然适用）**：`control` 必须明显大，否则这套测量不灵敏、后面的 0 什么也不证明；四个候选里 `match=`
  命名的是引擎的映射，其余应当很大；`best/control < 1%` 才算"命中"（相对判据，不发明绝对容差）；`restored` 证明面板
  被复原；`helper-unresolved`/`other-unresolved` 按类计数（`x_` 帮手骨 vs 其他），`scale-off-from-one` 统计姿态里
  非 1 的缩放。
### HKX3（已实测，2026-10-10）：动画目录成立，元素布局被否定

- **成立的一半**：`hkbCharacterSetup::data → hkbCharacterData::stringData → animationNames` 读通了
  （`character='DefaultFemale' rig='Character Assets Female\skeleton_female.hkx' behavior='Behaviors\0_Master.hkx'`），
  且 **`names == bindings == 15203`** → 名字表与绑定集**同序**，"索引 → 动画名"可直接用；名字是 HKX 路径，
  1526 个含 "idle"。下一步单测/实现按已核对过的 `stringData` 读，不再怀疑它。
- **失败的一半**：`element-as-binding` 与 `element-holds-binding-pointer` **都是 `valid=0/8`**，而 8 个元素都可读 →
  元素既不是 binding 本身、也不以指向它的指针开头。最可能是"元素是 `hkReferencedObject`、binding 是成员（偏移 ≥ 0x10）"。
  上一版 `read_binding` 失败时只返回全零、看不出卡在哪一步（仪器缺陷，已修）。逐条见
  [HKX3 实测证据](s2-hkx3-catalogue-evidence-2026-10-10.md)。

### HKX4（已实测，2026-10-10）：原始转储说明元素形态，binding 落在 +0x30

```
SCOPY ANIM catalogue raw index=0 qwords='00007ff6ef2649b8 000000000001ffff 0 0 8000000000000000 0 00007ff6ef2649d8 000000000001ffff'
```

按 qword 读：+0x00 = vtable、+0x08 = `memSizeAndFlags=0xffff`+`refCount=1`（`hkReferencedObject` 头）、+0x10/+0x18 = 一个空数组、
**+0x30 = 第二个 vtable、+0x38 = 它自己的引用计数** —— 即"元素是包装对象，`hkaAnimationBinding` **按值**放在 +0x30"。
HKX4 的候选表只走到 +0x20，**差一格**；`as=value` 全部 `no-animation`、`as=pointer` 全部 `unreadable`，与转储完全一致。
逐条见 [HKX4 实测证据](s2-hkx4-layout-evidence-2026-10-10.md)。

### HKX5（已实现并打包，待游戏内一轮）：候选由元素形态生成 + 用动画自身字段确认

```
SCOPY ANIM catalogue elements=8 object-like=8/8 scan=0x80
SCOPY ANIM catalogue candidate offset=0x30 as=value valid=8/8 reasons='-'
SCOPY ANIM catalogue raw index=0 qwords='…'（0x80 字节）
SCOPY ANIM catalogue layout best=offset=0x30/value valid=8/8
SCOPY ANIM catalogue idle-base first='Animations\Idle.hkx@…'
SCOPY ANIM catalogue probe index=… name='Animations\Idle.hkx' type=spline duration=…s frames=… tracks=116 animation-tracks=116 skeleton-name='…' bones='NPC Pelvis [Pelv], …' copy-resolved=109/116
```

- **候选不再猜**：由一个元素的形态决定——某处 qword 像 vtable ⇒ 按值对象从这里开始；某处 qword 是可读堆指针且目标像对象 ⇒ 指针成员。
- **校验更强且全静态**：`hkaAnimation::type` ∈ 引擎命名类型、`duration` 合理、**`numberOfTransformTracks` == binding 轨道表长度**、
  每条轨道索引 ≤ 骨数。包装对象过不了"轨道数一致"这一条。
- **判读**：`best=` 那一行给出布局；`type=spline` 说明 HKX6 采样时**可能需要 chunk cache**（`interleaved` 则不需要）；
  `frames`/`duration` 给出采样率；`bones=` 是轨道指向的真实骨名（顺便验证轨道→骨映射）；
  **`copy-resolved=K/N` 是这段动画在副本上的写入覆盖率**——不需要采样就能算出来，HKX6 的预期上限就是它。
- 通过后 HKX6：`SampleTracks(t, out, nullptr, cache)`（带显式时间、const，不碰源 control）→ 写副本（HKX2 已验证的路径）
  → 用自己的时钟循环播放，并按 `poseLocal`（`boneNodes` 次序）做同相位 A/B。

### HKX5（已实测，2026-10-10）：绑定集元素是 0x30 字节的桩，路线改走 `hkbClipGenerator`

0x80 字节转储显示元素里 **vtable 每 0x30 字节重复一次**（`[vtable][memSize 0xffff|refCount 1][0][0][0x8000…][0]`），
即"元素是 0x30 字节的小对象"，其中**没有** `hkaAnimationBinding` 的字段；候选（0x00/0x30 两处 vtable）全部 `no-animation`。
这条路线要继续挖下去只会越挖越深。逐条见 [HKX5 实测证据](s2-hkx5-clip-evidence-2026-10-10.md)。

### HKX6（已实现并打包，待游戏内一轮）：走 `hkbClipGenerator`，采样用无 cache 的接口

`hkbClipGenerator` 在 CommonLibSSE 里**全类型**，只需找到实例；采样改用
`SampleIndividualTransformTracks(time, tracks, n, out)`——**它没有 chunk cache 参数**，
所以"spline 动画是否需要 cache"这条悬案不必先答。

```
SCOPY ANIM play search objects=N clips=K first='Animations\female\mt_idle.hkx@3.40s, …'
SCOPY ANIM play clip='Animations\female\mt_idle.hkx' type=spline duration=3.40s tracks=116 copy-resolved=109/116 truth-bones=116
SCOPY ANIM play ground-truth best-t=…s max-pos-delta=… max-rot-delta=…deg worst-pos-delta=… bones=109
SCOPY ANIM play ground-truth verdict=match|none best=… worst=…
SCOPY IDLE enabled (F2); driver=Animations\female\mt_idle.hkx
```

- **发现**：从 `behaviorGraph->rootGenerator` 起做**有界**遍历（深度 ≤4、对象 ≤96）；状态机的 `states` 是**类型化**的按名跟进，
  其余节点只跟"首字像 vtable"的指针；每个候选必须通过**clip generator 校验**（名字像 `.hkx`、binding 结构性通过、
  `mode ≤ 3`、`playbackSpeed` 合理）才被采信——走错一步只会多一条拒绝理由，不会选错动画。
- **选择**：优先**角色自己的站立待机**（`mt_idle.hkx` / `idle.hkx` / `idleforcedefaultstate.hkx` 的文件名），
  且优先引擎自己那份（路径不以 `data\` 开头的 mod 替换件排在后面）。
- **真值**：把该 clip 在**每个相位**采样一次，与引擎当前持有的姿态（`poseLocal`，**按 `boneNodes` 索引**）逐骨比较，
  打印最佳相位与最差相位。`verdict=match` 表示某个相位能复现引擎姿态到 1% 以内——这就是"引擎的动画数据能被我们正确采样"的证据。
- **驱动**：F2 打开后由**我们自己的时钟**推进（暂停时引擎不推进），每帧采样→写 local→重算子树世界；
  找不到 clip 时自动回退程序化待机并在日志里说明。

### HKX6（已实测，2026-10-10）：搜索被自己的窗口判据挡住，已修

`play search objects=96 clips=0`、`objects` **恰好等于上限** → 搜索被**截断**而非走完。回退逻辑正确
（`driver=procedural-idle`，程序化待机照常）。**根因**：扫描指针前要求整个 0x200 字节窗口可读，
于是 0x78 字节的 `StateInfo`（clip 就挂在它的字段里）被整段跳过，遍历只能去啃状态机里的杂项对象。
逐条见 [HKX6 实测证据](s2-hkx6-clipsearch-evidence-2026-10-10.md)。

### HKX7（已实现并打包，待游戏内一轮）：按可读范围扫描 + 用类名判类 + 直方图

1. **按可读范围扫描**（逐 8 字节前进，不可读即停，上限仍 0x200）——让 `StateInfo` 能进队；
2. **用 `hkReferencedObject::GetClassType()->name` 判类**（只在首字像 vtable 的对象上调用），
   类名 == `hkbClipGenerator` 才算候选；类名拿不到时才退回"名字像 `.hkx`"；
3. 深度 4→8、对象 96→512，新增 `play classes='…'` 与 `play rejections='…'` 两行直方图，
   `search` 行带 `capped=`——下一轮若还不中，这两行直接区分"遍历走丢"与"clip 不在图里"。

### HKX8（已实测，2026-10-10）：不再虚调用、遍历走完，但 clips=0 —— typed states 边从未生效

- **成立的一半**：HKX7 的崩溃消失（判类不再虚调用）；面包屑每 32 对象一条走到底，
  `objects=460 capped=false`、队列传空收尾——HKX6 的截断与 HKX7 的崩溃都已消除。
- **没成立的一半**：`clips=0`；`classes='hkbStateMachine:1'` + `rejections='other-class:1'` 证明全程
  只有根一个 typed 对象被访问。名字校验不依赖对象来历（`animationName` 在固定偏移上），460 个候选
  全部 `no-animation-name` = **根本没踩到 clip generator**，遍历从未进入生成器树——states 边一次都没
  跟出去（`states.data()` 指向指针数组，堆地址过不了形状判据，`StateInfo` 只能经类型化边到达）。
  逐条见 [HKX8 实测证据](s2-hkx8-clipsearch2-evidence-2026-10-10.md)、
  [日志](diagnostics/CharacterPanel-hkx8-clipsearch2-20261010-0107.log)。
- 三个候选根因（CLib 布局链 `hkbBindable=0x30 → hkbNode=0x48 → states@0x90` 自洽，根节点
  `readable(0x110)` 在 HKX8 已成立，整窗可读性不是问题）：①CLib 的 `states` 偏移与实际引擎不符；
  ②根状态机 states 数 > `Max_States`(64) 被静默跳过；③data 不可读（最不可能）——从外部分辨不了，
  需要根状态机的原始转储裁决。

### HKX9（已实现并打包，待游戏内一轮）：判类改虚表精确比对 + 根状态机转储

```
SCOPY ANIM play root-sm name='…' readable-0x108=… states.size=N data=0x… data-readable=…
SCOPY ANIM play root-sm qwords='…'（0x108 字节）
SCOPY ANIM play search objects=… clips=… states=数组/条目 capped=… first='…'
SCOPY ANIM play classes='hkbStateMachine:…,hkbStateMachine$StateInfo:…,hkbClipGenerator:…,other:…'
SCOPY ANIM play clip='…' …（选中后与 HKX6 协议相同）
```

- **判类 = 与引擎自己的虚表地址精确比对**（`VTABLE_hkbStateMachine` / `…__StateInfo` /
  `VTABLE_hkbClipGenerator`，CLib AE ID 226812 / 226706 / 226785）：一次受保护的内存读、零虚调用
  ——HKX7 的崩溃面被彻底移除，分类不再依赖"这个对象是怎么被发现的"。
- **states 边**只在虚表确证的状态机上触发；成员窗口（`&states, 0x10`）可读即可，不再要求整 0x108。
- **`root-sm` 转储**直接裁决 HKX8 留下的三选一：`states.size` 为 0 或垃圾大数 = 偏移问题（在转储里
  找 {堆指针, 小整数, 小整数} 三元组定真实偏移）；正常小值但 `states=0/…` = data 可读性或上限问题。
- **判读**：`states=0/0` = 边仍没生效（看 root-sm 转储）；`states>0` 而 `clips=0` = 已进树、识别问题
  （看 classes/rejections）；`classes` 出现 `hkbClipGenerator` 计数而 `rejections` 有具体拒因 =
  偏移级警报（名字读不出 = `animationName` 偏移错）。
- 通过后照旧：ground-truth 相位扫描（HKX6 协议）→ F2 用自己的时钟驱动。
- 深度 8→12、对象 512→1024（确证路径使每对象变廉价；真实生成器树比旧边界宽）。

### HKX9（已实测，2026-10-10）：虚表判类进树成功，66 个 clip 全卡在运行期 binding 为空

- **成立**：根状态机转储实锤 CLib 布局（`Master_Behavior`，`states.size=11`，转储 word18/19 =
  教科书 hkArray）——HKX8 的三选一裁决为"偏移没错，旧整窗可读门槛是会话级脆弱判据"；states 边
  21/100 生效；**66 个 `hkbClipGenerator` 被虚表精确识别**（34 状态机、140 StateInfo），零虚调用
  零崩溃，名字校验全过（`animationName`@0x48 偏移实锤正确）。
- **没成立**：66 个 clip 全部死于 `read_binding` 的 binding 指针检查（拒因 `unreadable:66`）——
  运行期 `binding`（@0xA0）是**惰性链接**，暂停菜单里未激活的 clip 就是空指针；静态链接是
  `animationBindingIndex`（@0x70）。这同时回头修正 HKX3-5 的判读：binding set 的空桩不是布局错，
  是**没被填充**。逐条见 [HKX9 实测证据](s2-hkx9-vtable-evidence-2026-10-10.md)、
  [日志](diagnostics/CharacterPanel-hkx9-vtable-20261010.log)。

### HKX10（已实现并打包，待游戏内一轮）：clip 明细 + control 路线 + binding set 定点转储

```
SCOPY ANIM play clip-seen i=… idle=yes name='…mt_idle.hkx' binding-index=1022 binding=0x0 control=0x… speed=1.00 mode=0
SCOPY ANIM play element index=1022 at=0x… readable-0x40=true
SCOPY ANIM play element qwords='…'（0x40 字节）
SCOPY ANIM play element binding-at-0x10 at=0x… valid=true reason=valid
```

- **clip-seen 明细**：每个被虚表识别的 clip 一行（名字、binding-index、binding 指针、control 指针、
  速度、模式；base-idle 名字的必打）——谁的指针活着，一目了然。
- **control 路线**：源角色此刻就在播 idle，激活中 clip 的 `animationControl`（@0x88）应当是活的，
  CLib 的 `hkaAnimationControl` 全类型（`binding`@0x38 / `localTime`@0x10 / `weight`@0x14）——
  `read_clip_generator` 现在接受两条路：直接 binding 有效，或 control->binding 有效。
- **定点元素转储**：用 clip 自带的 binding-index 瞄准 binding set 的一个元素（优先 base-idle 名字、
  次 control 非空、末首个），dump 0x40 字节并按统一校验判 `+0x10` 处的指针——已知索引的定点核对，
  取代 HKX3-5 的盲扫；HKX4 看到的 27c8 变体若真是"已加载"形态，这次会在已知索引上复现。
- **判读**：`clip-seen` 里若有 `binding` 或 `control` 非零的 base-idle 行，且 `element binding-at-0x10
  valid=true` → 采样链的入场券到手，下一轮直接接 ground-truth 与 F2 驱动；若全空 → 转储指认
  "已加载条目"的真实形态，布局从活样本上学。

### HKX10（已实测，2026-10-10）：control 也全空、正在播放的条目也是空桩——路线 A 正式关闭

- `clip-seen` 全中：66 个 clip 名字/索引全可读（`MT_Idle.HKX`↔1022↔名字表对上），**虚表判类 +
  名字/索引读取这套仪器跨会话稳定**（root-sm 转储复现 `Master_Behavior`/states=11）。
- 但连正在播放的 mt_idle 也 `binding=0x0 control=0x0`，其 binding set 条目（定点 dump 索引 1022）
  也是空桩——**"未激活所以为空"被排除；标准 Havok binding/control 层在 Skyrim 运行时是死的**，
  HKX4 的"27c8 已加载变体"确认为堆邻居噪音。路线 A 收束为死路关闭；`BShkbAnimationGraph` 仅剩的
  `unk190/unk1A8/unk1C0` 无类型数组不再挖（§9 红线）。逐条见
  [HKX10 实测证据](s2-hkx10-control-evidence-2026-10-10.md)、
  [日志](diagnostics/CharacterPanel-hkx10-control-20261010.log)。

### 路线重排（HKX10 之后，待用户拍板 D/C）

- **A（引擎已加载动画）**：死路，关闭（HKX3–10 的定点证据链）。
- **B（用户提供 HKX 经引擎装载）**：被 A 连带否定（装载后仍要过同一条采样链）；"让源播任意动画"
  由本机已装的 OAR 替代实现。
- **D（新，推荐）：poseLocal 录制-回放**——未暂停时环形录制引擎每帧自己采样出的 `poseLocal`
  （全类型、按 `boneNodes` 次序，HKX2 已证明可读），暂停面板内用自己的时钟回放驱动副本（HKX2 的
  写入路径 + F2 开关都是已验证机制）。真值 = 录制本身（引擎真实输出），A/B 协议照旧：副本对录制
  同相位逐骨对比应到浮点量级。代价小，直接覆盖 S2 当前验收目标（待机）。
- **C（自研 HKX 解析器）**：自由度最强（可播源没在播的动画，"展示动作切换"终局方案），代价
  packfile + spline 解码自研；留作 D 证明驱动链之后的选择。

### 拍板与 HKX11（已实现并打包，待游戏内一轮）：借 OAR 的地图，直接读引擎的动画文件管理器

**用户拍板（2026-10-10）**：只借用 OAR 的解析逻辑（它开源、已把引擎路径标好），从引擎拿动画，
**不与 OAR 运行时交互**。OAR 源码给出的决定性事实：

- `hkbAnimationBindingWithTriggers` 全布局（OAR Havok.h，带 static_assert）：`binding`
  （`hkaAnimationBinding*`）在 **+0x10**——HKX10 探针位置正确，空 = 未装载；
- 引擎动画注册表：`BShkbAnimationGraph::projectDBData`（+0x200）→ `BShkbHkxDB::ProjectDBData`
  （`hashedBehaviors`/`hashedAnimations`/事件 map/`bindings`@0x150）；
- 引擎解析函数被 OAR 点名：`GetHashedAnimFromAnimIndex`（AE ID 63600，内部读 graph+0x200）；
- **`AnimationFileManagerSingleton` 在 CLib 全类型**：`Queue/Load/Unload` 在 clip 的
  Activate/Update/Deactivate 期调用；**`loadedAnimations[]` = `LoadedAnimation{void* 已载数据;
  AnimationFileInfo{crc32文件名, 扩展名, crc32路径}; counter}`**；单例可达；CRC = 小写文件名去
  扩展名的标准 CRC32。

**HKX11 内容**（`SCOPY ANIM animmgr` 系列）：

```
SCOPY ANIM animmgr queued=N loaded=M
SCOPY ANIM animmgr loaded i=… idle=yes file='Animations\female\mt_idle.hkx' crc=0x… counter=… data=0x… probe=valid type=spline duration=3.40s tracks=116
SCOPY ANIM animmgr rejected-dump i=… at=0x… qwords='…'   （首个被拒指针的原始转储）
SCOPY ANIM play clip='…' type=… duration=… tracks=… copy-resolved=…/… truth-bones=…   （两条取数路共用）
```

- 探针：`AnimationFileManagerSingleton::GetSingleton()` → `loadedAnimations[]` 逐条 CRC 反查名字表
  （CRC32("mt_idle") 等自算）→ 每条 data 指针按裸动画校验（引擎命名 type / 时长合理 / 轨道数在界内
  ——与 binding 校验同门，零虚调用）；
- **条件驱动**：若存在校验通过且文件名匹配 base-idle 的条目 → 按"**轨道 i = 骨骼 i**"假设（动画与
  骨架同源 skeleton_female.hkx，binding 的轨道表缺位时的替代）建 playback → **共享 ground-truth
  扫描验证该假设**（verdict=match ⇔ 轨道映射与采样都正确）→ 交 F2 驱动；binding 路仍优先，两路共用
  同一段真值扫描代码。
- **判读**：`animmgr loaded` 出现 `probe=valid` 且 file=base-idle → 引擎动画到手，看 `ground-truth
  verdict`；全部 `probe=not-object` 且 `rejected-dump` 显示包装结构 → 按 dump 定下一层；CRC 反查
  全 `-` → CRC 变体与标准不同，dump 的原始 CRC 留作人工对表。

### HKX11（已实测，2026-10-10）：管理器表是项目级注册表，条目指向哈希记录而非动画

- `animmgr queued=0 loaded=7872`——**项目级注册表**（≈15203/2），不是"最近使用"小表；
- 7872 条全部 `probe=not-object`，但首个被拒转储解码后**与 OAR 的 `HashedBehaviorData`（0x30）
  逐字段吻合**：{AnimationFileInfo(crc,"hkx",crcPath), …, stream*@+0x20, `DBData*`@+0x28}——
  `loadedAnimations[i].unk00` 指向项目 DB 的连续哈希记录（多记录共享 CRC、0x30 步进），**动画本体
  在再下一跳 `DBData : hkLoader { loadedData: hkArray<hkResource*> }` 里**；
- CRC 反查（标准 CRC32）全 `-`——疑似行为文件名或另一变体，**不阻塞**：改用引擎自己的解析器按
  索引取条目。逐条见 [HKX11 实测证据](s2-hkx11-animmgr-evidence-2026-10-10.md)、
  [日志](diagnostics/CharacterPanel-hkx11-animmgr-20261010.log)。

### HKX12（已实现并打包，待游戏内一轮）：调引擎自己的解析器，从已装载资源里走两跳

```
SCOPY ANIM hashed index=1022 at=0x…
SCOPY ANIM hashed record db-data=0x…        （返回值带 AnimationFileInfo+"hkx" 模式时）
SCOPY ANIM hashed anim i=0 at=0x… type=spline duration=…s tracks=116
SCOPY ANIM play clip='hashed:1022' … copy-resolved=…/… …   （与 binding 路共用 ground-truth）
```

- **调 `GetHashedAnimFromAnimIndex(graph, 1022)`**（AE ID 63600，OAR 补丁点名；索引 = clip-seen 的
  mt_idle binding-index）——引擎权威映射，**无需 CRC 定名**；
- 返回值若带 AnimationFileInfo+"hkx" 记录模式 → 取 **+0x28 `DBData*`** → `loadedData`
  （hkArray@+0x10）有界两级遍历，凡过裸动画校验者即 mt_idle（链按索引取自引擎）；
- 校验通过 → 按"轨道 i = 骨骼 i"建 playback → **ground-truth 扫描验证**（verdict=match ⇔ 映射与
  采样都正确）→ F2 驱动；binding 路仍优先，三路共用同一段真值扫描；
- **判读**：`hashed anim` 出现且 `ground-truth verdict=match` → **S2 驱动链闭环**；`found=0` → 资源
  结构更深，按日志里的指针继续；`at=0x0` 或小整数 → 返回值语义不是指针，转 ProjectDBData 布局
  核对（OAR 布局带 static_assert，`bindings`@0x150 应等于角色的 animationBindingSet）。

### HKX12（已实测，2026-10-10）：解析器返回文件注册表条目，已装载数据在更深的未探层

- `GetHashedAnimFromAnimIndex(graph, 1022)` 调用成功、返回堆指针、记录开头 = AnimationFileInfo+
  `"hkx"` 模式——但 **+0x28 = `0xb6aaba92` 是 32 位值，不是指针**：返回的是
  **`&hashedAnimations[1022]`**（`HashedData` 按值条目 0x20、纯文件信息），`+0x28` 溢出到元素 1023；
- **"名字/索引注册"与"已装载数据"分离实锤**。三轮合拢的地图：binding set = 空桩（HKX10）、
  hashedAnimations = 纯文件信息（HKX12）、已装载数据只剩管理器记录 ptr1/ptr2（HKX11 转储中
  +0x20/+0x28，未跟进）与管理器 unk68/unk88（未探）两处未探层；
- 逐条见 [HKX12 实测证据](s2-hkx12-hashed-evidence-2026-10-10.md)、
  [日志](diagnostics/CharacterPanel-hkx12-hashed-20261010.log)。

### 当前决策点（HKX12 之后，用户暂缓拍板）

- **HKX13（最后一跳探针）**：跟进管理器记录 ptr1/ptr2 + CRC32("mt_idle") 在 7872 条里点名，拿到即
  接已铺好的驱动；**检查点：仍拿不到动画对象就转 D**（hkResource 内部布局是再一个未知层，边际收益
  递减）。
- **路线 D（poseLocal 录制-回放）**：机制全部已验证，小时级工程，直接交付 S2 待机验收；A″ 三轮的
  结构图成果（虚表判类仪器、binding set/管理器/注册表布局）留作"展示动作切换"的地图。

## 6. 验收与对比协议（这是"可测试"的核心）

1. **数值 A/B（主判据）**：同一动画、同一相位下，逐骨比较**副本的 local 变换**与**源角色的 local 变换**，
   输出 `SCOPY ANIM compare bones=N/N max-rot-delta=…deg max-pos-delta=…u`；
   差异应在浮点精度量级（插值/相位对齐误差另计并写明）。
2. **目视 A/B（辅助）**：同一个动画在源角色（世界侧播放）与副本（面板内）同相位对照截图。
3. **独立性（FR-03/FR-05 红线）**：退出背包后，O 与附近 NPC 的姿态、位置、物理无任何变化；
   面板操作不改变 O；不通过推进世界模拟来驱动 P。
4. **动作连续性**：切换动作、循环接缝、根运动不移动世界角色（根变换由摄影棚接管）。
5. 每轮单独日志与构建身份（`build-manifest.json` 的身份串必须与日志首行一致）。

## 7. 需要用户决定的一件事

**动画数据来源选 A / B / C？** 若选 B，请一并说明：文件放哪里、用什么方式让引擎加载它
（替换某个 vanilla 动画／注册到行为工程／用 `NotifyAnimationGraph` 触发的自定义事件）。

## 8. 工程状态（下一个会话直接可用）

- 构建（实验包）：
  ```powershell
  cmake -S . -B build-scopy -DCHARACTER_PANEL_SCENE_COPY_EXPERIMENT=ON -DCHARACTER_PANEL_BUILD_PROBE=OFF -DCHARACTER_PANEL_PROBE_REVISION=<轮次>
  cmake --build build-scopy --config Release --target CharacterPanel CharacterPanelCopyMathTest
  ctest --test-dir build-scopy -C Release --output-on-failure
  cmake --build build-scopy --config Release --target CharacterPanelSceneCopyPackage
  ```
  诊断探针构建另加 `-DCHARACTER_PANEL_S2_PROBE=ON`（探针落在 F6）。
  **注意**：新增 `src/*.cpp` 后必须重新 configure（GLOB 在 configure 期求值）。
- 默认（Actor 路线）构建同理用 `build/`，`cmake --build build --config Release --target CharacterPanel`。
- 当前产物：`dist/CharacterPanel-scopy-HKX12-1.2.1.zip`，身份
  `CharacterPanel-scopy-HKX12-4165d734ea-cl94faaed0c6-20261009T184849Z`（`source_baseline_dirty=false`，基线
  `4165d734ea`），DLL SHA-256 `25d98ef7…`——**已实测**（判读见 §5 HKX12 节；实际部署的是提交前中间
  构建 `2e22e8f+dirty`，代码一致）。**当前无待验证代码；下一步待用户拍板（HKX13 最后一跳 / 路线 D），
  见 §5 决策点。****已实测的十二轮**：HKX12（[证据](s2-hkx12-hashed-evidence-2026-10-10.md)、
  [日志](diagnostics/CharacterPanel-hkx12-hashed-20261010.log)）、HKX11
  （[证据](s2-hkx11-animmgr-evidence-2026-10-10.md)、[日志](diagnostics/CharacterPanel-hkx11-animmgr-20261010.log)）、
  HKX10（`…-HKX10-d3c847a3b9-…`，[证据](s2-hkx10-control-evidence-2026-10-10.md)、[日志](diagnostics/CharacterPanel-hkx10-control-20261010.log)）、
  HKX9（`…-HKX9-746ffacf13-…`，[证据](s2-hkx9-vtable-evidence-2026-10-10.md)、
  [日志](diagnostics/CharacterPanel-hkx9-vtable-20261010.log)）、HKX8（`…-HKX8-7656223071-…`，
  [证据](s2-hkx8-clipsearch2-evidence-2026-10-10.md)、
  [日志](diagnostics/CharacterPanel-hkx8-clipsearch2-20261010-0107.log)）、HKX7 崩溃轮
  （[崩溃日志](diagnostics/CharacterPanel-hkx7-crash-20261010-0103.log)）、HKX6（`…-HKX6-733dfd5f17-…`，
  [证据](s2-hkx6-clipsearch-evidence-2026-10-10.md)、[日志](diagnostics/CharacterPanel-hkx6-clipsearch-20261010-0056.log)）、
  HKX5（`…-HKX5-3cd816a1eb-…`，[证据](s2-hkx5-clip-evidence-2026-10-10.md)、
  [日志](diagnostics/CharacterPanel-hkx5-clip-20261010-0041.log)）、HKX4、HKX3、HKX2、HKX1（各自的证据与日志见
  [docs/diagnostics](diagnostics/)）。操作说明见 [tools/scene_copy/README.txt](../tools/scene_copy/README.txt)。
  此前 IDLE1–IDLE3、S2P1–S2P4 的包、日志与分析脚本都在 `dist/` 与 [docs/diagnostics](diagnostics/)。
- 提交状态：`d9ad6b0`（归档探针与待机）→ HKX1–HKX6 每轮实现 + 实测 + 身份 → `7984bbda89`/`7656223`/
  `746ffac`/`d3c847a`/`a93c005`/`f00041f`/`b715354`（HKX7 崩溃修复、HKX8 判读收窄、HKX9 虚表判类、
  HKX10 clip 明细、HKX11 管理器探针、HKX12 引擎解析器）；
  都在本地 `master`，`extern/CommonLibSSE` 的历史 dirty 状态照旧排除。
- 热键现状（HKX3 产品构建）：`F7` 捕获（审计通过后打印对齐报告 + 姿态重放测量 + 动画目录）、`F8` 绘制、`F3` 旋转、
  `F4` 释放、`F2` 待机开关（默认开）；探针只在 `-DCHARACTER_PANEL_S2_PROBE=ON` 的诊断构建里占 F6。

## 9. 明确不要做的事

- **不要继续调 idle 的振幅／周期**来"让它像待机"——没有真值，无法判定。
- **不要在解析出真实调色板映射之前写蒙皮矩阵缓冲**（槽位下标 ≠ `bones[]`，写多少都打偏）。
- **不要用推进世界模拟／推进 O 的动画来驱动副本**（FR-03/FR-05 红线）。
- **不要把"静默回退到程序化待机"当成动画链路通过**——回退必须写进日志并计入未通过。
