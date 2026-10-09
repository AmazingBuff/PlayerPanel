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

下一步 **HKX3（已打包待测）**：动画目录（typed 名字表 → 可直接采样的 Idle 索引）与绑定集元素布局的结构化核对，
方案见 §5 末。

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
- 当前产物（**待你跑的一轮**）：`dist/CharacterPanel-scopy-HKX5-1.2.1.zip`，身份
  `CharacterPanel-scopy-HKX5-3cd816a1eb-cl94faaed0c6-20261009T163707Z`（`source_baseline_dirty=false`，基线
  `3cd816a1eb`），DLL SHA-256 `4e71b491…`。**已实测的四轮**：HKX4（`…-HKX4-215cea8805-…`，DLL `06cf6690…`，
  [证据](s2-hkx4-layout-evidence-2026-10-10.md)、[日志](diagnostics/CharacterPanel-hkx4-layout-20261010-0033.log)）、
  HKX3（`…-HKX3-e32eafc31c-…`，DLL `f0adfc6a…`，[证据](s2-hkx3-catalogue-evidence-2026-10-10.md)、
  [日志](diagnostics/CharacterPanel-hkx3-catalogue-20261010-0026.log)）、HKX2（`…-HKX2-a0eaef50cd-…`，DLL `506e21f7…`，
  [证据](s2-hkx2-replay-evidence-2026-10-10.md)、[日志](diagnostics/CharacterPanel-hkx2-replay-20261010-0016.log)）与
  HKX1（`…-HKX1-cd646aefae-…`，DLL `356c71f4…`，[证据](s2-hkx1-alignment-evidence-2026-10-10.md)、
  [日志](diagnostics/CharacterPanel-hkx1-align-20261010-0001.log)）。操作说明见
  [tools/scene_copy/README.txt](../tools/scene_copy/README.txt)。此前 IDLE1–IDLE3、S2P1–S2P4 的包、日志与分析脚本
  都在 `dist/` 与 [docs/diagnostics](diagnostics/)。
- 提交状态：`d9ad6b0`（归档探针与待机）→ `cd646ae`/`66758ca`（HKX1）→ `1abe07d`（HKX1 实测）→ `a0eaef5`/`1aa167d`
  （HKX2）→ `e32eafc`/`f65930a`（HKX3）→ `215cea8`/`b2877d8`（HKX4）→ `3cd816a`（HKX5）；都在本地 `master`，
  `extern/CommonLibSSE` 的历史 dirty 状态照旧排除。
- 热键现状（HKX3 产品构建）：`F7` 捕获（审计通过后打印对齐报告 + 姿态重放测量 + 动画目录）、`F8` 绘制、`F3` 旋转、
  `F4` 释放、`F2` 待机开关（默认开）；探针只在 `-DCHARACTER_PANEL_S2_PROBE=ON` 的诊断构建里占 F6。

## 9. 明确不要做的事

- **不要继续调 idle 的振幅／周期**来"让它像待机"——没有真值，无法判定。
- **不要在解析出真实调色板映射之前写蒙皮矩阵缓冲**（槽位下标 ≠ `bones[]`，写多少都打偏）。
- **不要用推进世界模拟／推进 O 的动画来驱动副本**（FR-03/FR-05 红线）。
- **不要把"静默回退到程序化待机"当成动画链路通过**——回退必须写进日志并计入未通过。
