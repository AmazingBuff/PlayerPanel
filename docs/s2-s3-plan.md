# S2 / S3 方案：副本的独立动作与物理

日期：2026-10-08。前置：S0 已完整通过（测试 A + B，见
[回传记录](scene-graph-copy-results-2026-10-08.md)）。范围对应 PRD 的 FR-03 与
[后续关卡表](scene-graph-copy-validation.md#后续关卡本轮未实现不作为-s0-已通过项) 的 S2（动作）、
S3（CBPC／FSMP 独立注册）。

> **当前状态（2026-10-09）：S2／S3 的"摆姿驱动"路线判定不可行，调查收口。**
>
> 结论建立在一次**有效的分类测量**上（不是猜的）：皮肤缓冲区的克隆期内容与**骨骼裸世界矩阵**
> 的差异为 **1.45**，而与我们写入的复合蒙皮矩阵差 **507.73**、与绑定姿态逆差 **538.08**，
> 也就是缓冲区存的是世界矩阵。据此改为写入世界矩阵后——
> **画面仍然完全不动**，且此前"跳过重建"时画面也完全不变。
>
> 两组事实合起来只能得出一个结论：**CPU 侧的 `NiSkinInstance::boneMatrices` 不是顶点着色器
> 采样的那份数据；对它做任何写入（正确内容、垃圾内容、跳过）都不影响渲染结果。**
> 继续深入需要图形调试器逐次比对（本次调查已用到 RenderDoc 一次），
> 不属于"改一处、进游戏试一次"能推进的范围。
>
> **因此 S2／S3 不再沿"我们直接驱动副本骨骼"这条路推进。**

## 收口记录（2026-10-09，游戏内 9 轮 + 离线修正 4 轮）

### 最终测量（有效的一次）

| 测量 | 值 | 结论 |
| --- | --- | --- |
| `SCOPY ANIM compare` | `vs-written=507.73`、**`vs-world-row0=1.45`**、`vs-bind-inverse=538.08` | 缓冲区内容是**骨骼裸世界矩阵** |
| 按其语义写入世界矩阵后 | **画面不动** | 写入不生效 |
| 跳过重建（`Rebuild_Skinning_Matrices=false`） | **画面完全不变** | 重建本身无副作用，也说明原缓冲区本来就对 |
| 整槽写坏（12 个 float 全改成垃圾） | **画面不动** | 该数组不参与渲染 |

### 因此确立的（头文件 + 实测双重支持）

1. **副本不携带动画状态**：无 `controllers`、无指向原角色的 `userData`，`boneWorldTransforms` 全部指向副本自己的骨骼（`30/30`、`38/38`、`71/71 direct`）；
2. **GPU 侧的骨骼数据与 CPU 侧 `boneMatrices` 不同源**：CPU 侧怎么改都不影响渲染；
3. **皮肤缓冲区布局已明确**：每槽 48 字节（3×4 行主序浮点），平移在索引 3/7/11，与 RenderDoc 抓到的 `BonesBuffer` 一致；
4. **FSMP 在暂停时自行挂起**（`SkyrimPhysicsWorld::ProcessEvent` 里 `if (gamePaused || GameIsPaused()) suspend()`），所以"暂停时副本物理冻结"与世界角色表现一致，不是克隆造成的。

### 这轮调查中我犯的方法性错误（留档以免重犯）

**探针本身出错 5 次**，每次都浪费了一轮游戏验证：

| # | 错误 | 教训 |
| --- | --- | --- |
| 1 | 绕骨骼**局部 Z**（= 骨骼轴向）旋转，等于自转，看不出效果 | 摆姿必须绕垂直于骨骼的轴 |
| 2 | 把子骨骼到父骨骼的**固定距离**当成"位移"读（`world-moved=2.26`） | 单帧数字不足以判断，必须多帧统计 |
| 3 | 每帧重读父世界（被 `pose()` 覆盖）导致摆动被抵消 | 基准必须锚定在探针开启时捕获的值 |
| 4 | 选骨时跨 skin 取"子树最大"，选中了**另一套骨架**的根 | 骨骼必须取自**某一个 skin 自己的 `bones[]`** |
| 5 | 往缓冲区写**复合蒙皮矩阵**（它要的是世界矩阵） | 先分类数据语义，再写 |

**另有两次"以为测出了库的行为"**（欧拉往返丢信息、矩阵乘法不满足结合律），实际是**我用 PowerShell 脚本编辑源码时弄坏了文件**（大括号被注释掉、编码损坏）。**教训：源码只用编辑工具改；对库的假设必须用测量程序验证，而不是靠测试失败反推。**

### 可行的替代方向（未验证，留作后续）

既然 GPU 侧骨骼数据不可触及，若仍要实现独立动作，唯一的自驱路径是**绕开蒙皮**：
每帧把骨骼变换**烘焙进顶点**（读副本顶点缓冲 → 用我们的骨骼矩阵变换 → 写回），
即自行完成 CPU 蒙皮。代价是每帧顶点级写入与性能风险；收益是完全可控、不依赖引擎蒙皮路径。
**在决定走这条路之前，应当先用一次 RenderDoc 会话确认顶点着色器实际采样的是哪个缓冲**
（本次只抓了 `BonesBuffer` 一次），否则可能又是一轮猜测。

## 证伪记录（2026-10-09，游戏内探针 6 轮 + 1 轮离线修正）

探针用途：写一块副本自己的骨骼，看画面是否跟随。6 轮游戏内结论都是"画面不动"，
但**逐轮复查发现其中多轮是探针缺陷造成的假否定**，必须区分对待。

| 轮次 | 假设 | 结果 | 有效性 |
| --- | --- | --- | --- |
| 1 | 绕骨骼局部 Z 轴旋转即可摆动 | 错。Z 是骨骼**轴向**，等于自转（"拧麻花"），在脊柱上看不出来 | **无效**（探针缺陷） |
| 2 | 换局部 X 轴 + 幅度 ±57° | 仍不动 | 受第 1 轮同类缺陷影响，存疑 |
| 3 | `boneWorldTransforms` 仍指向**原角色**的骨骼变换 | **排除**：`30/30 direct`、`38/38 direct`、`71/71 direct` —— 全部指向副本自己的骨骼 | **有效** |
| 4 | 旋转逐帧累积导致摆幅很小 | 改为每帧从原始姿态重算 | 修复本身正确，但当时仍受"父世界被重写"缺陷影响 |
| 5 | 引擎 `UpdateDownwardPass` 级联没运行，改显式重算子树 | 仍不动 | **无效**：读的是被 `pose()` 每帧覆盖的父世界，摆动被抵消 |
| 6 | 上轮 `world-moved=2.26` 表示子骨骼被推动 | **推翻**：多帧统计 `moved-range=[0.00,0.00]`。2.26 是子骨骼到父骨骼的**固定距离** | **有效**（纠正了我自己的误读） |

**离线修正轮（无游戏，ctest）**：把摆动公式剥成 `swing_about_pivot()` 并写单测后，
**测试当场抓到两个真实错误**——角度判据假定"合成是纯 Z 旋转"（实际父级 heading 0.7 +
骨骼 0.35），以及 pivot 判据方向反了（`bone_world * pivot` 恒等于 pivot，等于没测）。
改正后全绿；并做了**变异验证**：把 `delta.translate` 置零（即"绕父级原点"错误）后
12 条断言变红，含 `the bone origin must not move when swinging about it`。**测试本身被证明会失败。**

**连带获得的确证（价值独立于 S2）**：

- 副本**不带**指回原角色的 `userData`（`copy-userData=0x0`）→ 所有"经 userData 取原角色
  动画图"的方案彻底排除（这原本是最大的陷阱：拿了就会驱动世界角色，违反 FR-05）；
- 副本**不带**任何控制器（`controllers=none`）→ KF 路径在克隆上不存在，只能自己造。

**判据一（只读）已拿到的结构性事实**：

| 项 | 值 | 意义 |
| --- | --- | --- |
| 缓冲区布局 | `alloc=1440 / slots=30 = 48` 字节/槽 | `boneMatrices` **确实是 3×4 浮点矩阵数组**，布局假设成立 |
| `rotation-match` | `0/30`、`0/38` | 缓冲区的矩阵**不等于副本任何节点的裸世界变换** → 它们是复合量（骨骼世界 × 绑定逆等），拿裸变换直接比是错的比法 |
| `slot0-row0=0.346` vs 骨骼 `0.994` | 既不等于原始也不等于摆动 | 与节点树无直接同源关系（**但该轮摆动本身无效，故此判读待重测确认**） |

**尚未排除的候选**：`NiSkinInstance::boneMatrices`(0x48) / `prevBoneMatrices`(0x50) /
`skinToWorldWorldToSkinMatrix`(0x58) —— 着色器读的"输出端"，而 `boneWorldTransforms`(0x30)
只是它的输入之一。判据一的下一步判读是 `slot0-follows-swing`：
true → 候选证伪（问题在绘制链别处）；false → 确诊（缓冲区是克隆期快照，须自己重算）。

## 已确证的事实（读头文件得到，非推测）

### 1. 动画系统是"每个 holder 一份"，不在节点树里

`TESObjectREFR` 自己继承 `IAnimationGraphManagerHolder`（`TESObjectREFR.h:104-105`），
接口本体在 `RE/I/IAnimationGraphManagerHolder.h`：

| 方法 | 用途 |
| --- | --- |
| `GetAnimationGraphManagerImpl` / `SetAnimationGraphManagerImpl` | 取／换该 holder 的图管理器 |
| `PopulateGraphNodesToTarget(BSScrapArray<NiAVObject*>&)` | 把骨架节点灌进图 |
| `ConstructAnimationGraph(BSTSmartPointer<BShkbAnimationGraph>& a_out)` | **引擎的图构造入口** |
| `SetupAnimEventSinks` / `CreateAnimationChannels` / `PostCreateAnimationGraphManager` | 建图后的挂钩 |
| `UpdateAnimationGraphManager(const BSAnimationUpdateData&)` | 推进该 holder 的图 |

容器 `RE/B/BSAnimationGraphManager.h`：`graphs` 是
`BSTSmallArray<BSTSmartPointer<BShkbAnimationGraph>>`（引用计数），另有 `boundChannels`、
`variableCache`；`activeGraph` 决定当前生效份。

图本体 `RE/B/BShkbAnimationGraph.h` 的关键成员：

```
hkbCharacter  characterInstance;   // 0C0  行为图状态（hkbCharacter::behaviorGraph @58）
hkbBehaviorGraph* behaviorGraph;   // 208
Actor*        holder;              // 210  ← 图的身份锚点
BSFadeNode*   rootNode;            // 218  ← 驱动的是谁的骨架
BSTArray<BoneNodeEntry> boneNodes; // 160
```

暴露的 API 只有寄存器读写（`Get/SetGraphVariableBool/Float/Int`），**头文件里没有
`BShkbAnimationGraph::Create` 之类的独立构造器**。

### 2. 由此得出的 S2 核心矛盾（必须先证伪）

**2026-10-08 调研补充（头文件级确证，逐条有据）**：

- **节点→图没有任何指针**。`NiAVObject` 的成员只有 `parent` / `collisionObject` / 变换 /
  `worldBound` / `flags` / `TESObjectREFR* userData`(0xF8) / `fadeAmount` /
  `lastUpdatedFrameCounter`；`HasAnimation()` 只是 BSXFlags 的一个位测试，不是查图。
  图→节点则是单向的（`BShkbAnimationGraph::rootNode`、`boneNodes[i].node`）。
  **结论：克隆后，副本上不存在可达的 `hkbCharacter`／`hkbBehaviorGraph`。**
- CommonLibSSE-NG **没有** `BShkbAnimationGraph::Create`、没有 `SetBehaviorGraph`、没有图工厂；
  `ConstructAnimationGraph` / `PopulateGraphNodesToTarget` 是"每个 holder 对自己的 3D"实现的虚函数，
  **对克隆树的行为未验证**。引擎真正的建图路径只以偏移存在（`BSAnimationGraphLoadScrapper` 等，无头文件）。
- **没有找到"给克隆骨架跑第二套动画"的先例**（检索手段受限，作者标注为"未找到"而非"不存在"）。
- 引擎**自己**在暂停菜单里就有非 actor 的图：`BookMenu` / `MistMenu` / `StatsNode` /
  `ModelReferenceEffect` / `SummonPlacementEffect` / `ActorMagicCaster` 均继承
  `SimpleAnimationGraphManagerHolder` —— "暂停 + 非 actor 图"引擎自己做到了；
  **但它们是否真在暂停时 tick，头文件无法证明**（这是唯一悬空的关键假设）。
- 暂停语义：`UI::GameIsPaused()` 只是 `numPausesGame > 0`，标志本身不停止任何东西；
  已确证暂停时游戏时间不推进（SkyrimSoulsRE 文档：暂停菜单开启时 `Utility.Wait()` 失效）、
  装备动画被**推迟**而非播放（`PlayerCharacter::hasQueuedEquipAnim`）；
  UI 有独立计时器 `UI::uiTimer`，而世界 `BSTimer` 在 `pauseCount != 0` 时 no-op。
- **唯一有头文件支持的第二动画入口是 KF 路径**：`NiControllerManager` / `NiControllerSequence`
  **实现了 `CreateClone`**，是节点自带控制器，克隆时会被复制；而 `NiObject::CreateClone` 默认
  `return this`（未覆写的类根本不会被复制）。玩家子树里是否真有 KF 序列，**待运行时确认**。
- **必须运行时排除的陷阱**：`userData` 是裸 `TESObjectREFR*`，克隆可能把它一起带过来 →
  那就指向**原角色**，谁拿它驱动就是驱动世界角色（FR-05 明令禁止）。探针已把它做成显式判据。

S0 的冻结姿态由此完全解释：副本是 `NiObject::Clone()` 的产物，**不携带任何图**，也不在任何
`boneNodes` 列表里。而 `BShkbAnimationGraph::holder` / `rootNode` 都指向原角色，所以
"给副本 `ConstructAnimationGraph` 一份新图"这条路**很可能反而驱动世界角色**——正是
PRD FR-05 所禁止的。顺序因此固定：**先证明"能给副本摆姿"，再谈"用什么驱动"**。

### 3. 本机环境（S3 要面对的两套物理）

| 组件 | 实际文件 | 备注 |
| --- | --- | --- |
| Faster HDT-SMP | `mods\Faster HDT-SMP\SKSE\Plugins\hdtsmp64.dll` | 附带官方 `.pdb`，可反查导出；`SMPFixes.dll`（0.0.3 for 3.0.0-Beta）在同装 |
| CBPC | `mods\CBPC - Physics with Collisions\SKSE\Plugins\cbp.dll` | 闭源，另有大量 CBPC 配置 mod |
| 骨架／身形 | XP32MSSE、CBBE 3BA、UBE 2.0 等多套并存 | S0 已实测两种身形 |

对外接口的可用性由并行调研确认（FSMP 是否提供 SKSE 插件 API、CBPC 是否只认 Papyrus／装备配置），
结论落表后本节补写。

## 第一工作包：摆姿探针（S2-P0，已实现）

一个**最小写入**的探针，回答一个决定性问题，不做任何"正式实现"：

> **副本的蒙皮到底读不读它自己的节点树？** 即：我们手动写一块骨骼，画面会不会跟着变。

这个问题必须先回答，因为前面第 2 节已经确证副本"图不可见"——如果连摆姿都不成立，
S2 不是数据问题而是不可行，应回到架构选择；而如果摆姿成立，S2 与 S3 就同时有了实现形态
（见下文 S3 结论：注册式不可行，只剩自驱式，而自驱式的前提正是"写骨骼有效"）。

判定：

- **摆姿成立**（画面随偏移改变）→ 走**自驱**路线：S2 是"我们自己往骨骼写姿态"，
  接下来才是数据来源问题（KF 序列 vs HKX 采样器 vs 程序化待机）；S3 是同一套手法
  摆动头发／衣物的骨骼链。全程**不碰引擎行为图**，天然满足 FR-05 的独立性要求；
- **摆姿不成立**（写了没反应）→ 说明副本的蒙皮路径还有一层未解（可能是骨骼数组归属
  或 `boneWorldTransforms` 被引擎重算覆盖），先查这一层，不要进入 HKX 或物理工作。

### 探针用法（F2 开关）

F5 是游戏内置的快速保存键，故探针热键定为 **F2**（F9/F10 亦已被其他 mod 占用）。
副本用 F7 捕获、F8 绘制之后，按 **F2** 开关探针。实现要点：

- 目标骨骼从**副本自己的 skin** 里选（子树覆盖节点最多者 = 主关节），不猜名字；
- 每帧对它的局部 Z 轴施加 `sin` 摆动（约 ±34°、周期 320 帧），随后 `UpdateDownwardPass` 级联；
- 开启后约 8 帧会打一条自报告：

```
SCOPY ANIM report bone='…' written-swing=… world-z-row=(…) controllers=…
                    copy-userData=… source-actor=0x… verdict=…
```

`verdict` 两个取值分别对应：`copy-is-graph-invisible`（副本没带 userData，安全，符合预期）
或 `copy-carries-userData-to-source`（副本带着指回原角色的 userData —— **任何基于 userData
取图的方案都会驱动世界角色，直接排除**）。`controllers=present/none` 则是 KF 路径是否存在的线索。

### 为什么先做这个而不是直接上 HKX

- HKX 属于 Havok 序列化格式，读取需要自写采样器或依赖 `hkbClipGenerator` 之类内部对象，
  成本远高于探针；而探针失败时这些成本全部作废。
- S0 已经证明"捕获姿态 + 手动摆根"这条路可行，探针只是把同一手法从**根**下沉到**单根骨骼**，
  风险可控、可快速证伪。

## S3 的路径（调研已完成，结论：注册式不可行）

### 调研结论（2026-10-08，源码级，带 commit 与文件行号）

**FSMP（hdtSMP64，本机 v3.5.0，源码 `dev@e52ad960`）：不能为额外骨架做独立模拟。**

- 它确实有对外接口，但**只给监听**：第三方在 `kPostLoad` 注册 SKSE 监听器，FSMP 随后派发
  `PluginInterface::MSG_STARTUP` 并传 `hdt::PluginInterface*`（**没有** `RequestPluginAPI`，
  也没有名字化接口，必须自带 `src/PluginAPI.h`）。完整方法表只有四个：
  `addListener/removeListener(IPreStepListener*)`、`addListener/removeListener(IPostStepListener*)`，
  事件体是 `{ const btAlignedObjectArray<btCollisionObject*>& objects; float timeStep; }`
  ——**是整个世界的碰撞对象数组，没有骨架/角色句柄**，且头文件写明只能施力、其余只读。
- 骨架进入模拟的唯一路径是**私有** `ActorManager::getSkeletonData()`，由游戏的
  ArmorAttach／HeadSkin 事件喂进来（全仓 `m_skeletons.push_back` 只有一处）。
  **注册、单骨架步进、注销三项都没有对外入口**；步进是世界级的
  `stepSimulation`（`SkyrimPhysicsWorld::doUpdate2ndStep`）。
- 构建**不发布任何 SDK**（只装 DLL/PDB/配置，无头文件、无导入库、无 `WINDOWS_EXPORT_ALL_SYMBOLS`），
  所以连内部事件源都无法链接。
- `FindOrCreateAnonymousSystem` / `AttachAnonymousSystem` / `DetachAnonymousSystem`
  是**注释掉的死代码**，未注册——"挂一个独立物理系统"最接近的形态并不存在于发行版。
- **暂停时它自己会挂起**：`SkyrimPhysicsWorld::ProcessEvent(FrameEvent*)` 里
  `if ((e->gamePaused || mm->GameIsPaused()) && !m_suspended) suspend();`，
  挂起后只 `writeTransform()` 重放上一帧变换。**这解释了一个此前被我误读的现象：
  副本里头发／衣物"冻结"并非我们的克隆所致，而是世界暂停时 FSMP 本来就停**。
  主线程在 `FrameSyncEvent` 里 `m_tasks.wait()` 同步等待 TBB worker，Pre/PostStep 回调
  在 **worker 线程**触发，不在主线程。

**CBPC（`cbp.dll` 1.6.4，闭源，符号/PDB 扫描）：同样不能。**

- 官方 12 个 Papyrus native（`CBPCPluginScript`），其中唯一相关的是
  `StartPhysics(Actor npc, String nodeName)` / `StopPhysics(...)`——**要求真 Actor + 该 actor
  自己的骨骼节点名**，没有接受裸节点/骨架的重载。CBBE 3BA 正是用它做 CBPC↔SMP 切换。
- 其余全是自驱：装备/身体配置 + 自己选 actor（`CBPCSystem.ini` 的
  `ActorAngle`／`ActorDistance`／`InCombatActorCount` 等）。
- 它**没有** SKSE 插件间接口（只导出标准 `SKSEPlugin_Load/Version`）。
- 每帧工作挂在游戏主循环入口（`ProcessTasks` 钩子 + 渲染 detour），内部用 ConcRT 并行。

### 因此 S3 的形态被限定为一条

| 候选 | 结论 |
| --- | --- |
| **注册式**（把副本骨架交给 FSMP／CBPC 独立推进） | **不可行**，两家都没有这个入口；FSMP 连 SDK 都不发 |
| **恢复全局模拟**（解除暂停让世界物理跑起来） | **禁止**。PRD FR-05 明令不得通过推进世界模拟来驱动 P；且验证文档第 130 行写明：若 S3 只能这样实现，必须回到架构选择，不把"副本动了"当成独立物理已通过 |
| **自驱式**（我们自己摆动副本的少数骨骼链） | **唯一可行的形态**。沿用 S0 已证明的"写 `NiTransform` + `UpdateDownwardPass`"手法，对头发／衣物的骨骼链做弹簧／阻尼摆动 |

**红线的措辞要求**：自驱式**不是** FSMP／CBPC 的模拟，必须在文档与验收里明确区分，
不得声称"已支持该物理模组"。它能满足的是 FR-03 里"若支持头发、衣物物理，其状态应独立于 O"
的精神（独立、可控、不碰世界），但**不构成对这两个模组的兼容性声明**。

**另一个由此得到的副产品**：既然 FSMP 在暂停时本就挂起，那么"副本物理冻结"与世界角色
在暂停时的表现是**一致的**——玩家在背包里看到的原角色头发同样是停的。这可以作为一句
准确的用户说明，而不是"我们的克隆冻结了物理"。

## 决策点与风险

| 项 | 说明 |
| --- | --- |
| 摆姿可行性（**最高风险**） | S2-P0 直接判定，失败即回到架构层面讨论 |
| 图身份绑定 | 任何"构造新图"的方案都会碰到 `holder`／`rootNode` 指向原角色，需先证明不会驱动原角色 |
| 暂停语义 | 背包暂停时引擎行为图是否仍在 tick，决定了"自驱式"是否需要每帧在绘制窗口内补写（S0 的姿势就是每帧补写的） |
| 物理注册的注销路径 | 注册进来的骨架必须在 F4／读档／主菜单时**可注销**，否则就是给引擎塞悬空指针（S0 三次崩溃的教训） |
| 动作数据来源 | HKX 采样器是独立工作量；程序化待机只能满足"至少稳定待机"，不足以满足 FR-03 的"可切换展示动作" |
