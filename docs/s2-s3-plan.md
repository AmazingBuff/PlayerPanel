# S2 / S3 方案：副本的独立动作与物理

日期：2026-10-08。前置：S0 已完整通过（测试 A + B，见
[回传记录](scene-graph-copy-results-2026-10-08.md)）。范围对应 PRD 的 FR-03 与
[后续关卡表](scene-graph-copy-validation.md#后续关卡本轮未实现不作为-s0-已通过项) 的 S2（动作）、
S3（CBPC／FSMP 独立注册）。

> **当前状态（2026-10-09）：S2／S3 判定为"在当前绘制路径下不可实现"，暂停实现尝试。**
> 游戏内 A/B 测试共 6 轮（探针 F2），逐一证伪了全部可控假设，见文末"证伪记录"。
> 继续推进需要附进程调试或渲染侧断言，属于独立专项；在此之前不要重复"改一处再进游戏试"的循环。

## 证伪记录（2026-10-09，游戏内探针 6 轮）

探针用途：写一块副本自己的骨骼，看画面是否跟随。**全部 6 轮的结论都是"画面不动"**，
每一轮排除一个假设，最终没有任何可控假设存活。

| 轮次 | 假设 | 结果 |
| --- | --- | --- |
| 1 | 绕骨骼局部 Z 轴旋转即可摆动 | 错。Z 是骨骼**轴向**，等于自转（"拧麻花"），在脊柱上看不出来。**是我的设计缺陷，不构成证伪** |
| 2 | 换局部 X 轴 + 幅度 ±57° | 仍不动 |
| 3 | `boneWorldTransforms` 仍指向**原角色**的骨骼变换（克隆浅拷贝的漏网） | **排除**：`skins: [Body [Ovl0]: 30/30 direct] [Hands [Ovl0]: 38/38 direct] [Dress: 71/71 direct]` —— 全部指向副本自己的骨骼 |
| 4 | 旋转逐帧累积，摆幅实际很小 | 改为每帧从探针开启时的原始姿态重算。仍不动 |
| 5 | 引擎的 `UpdateDownwardPass` 级联没有运行；改为**显式重算整棵子树**（`world = parent_world × local`） | 仍不动 |
| 6 | 上一轮看到的 `world-moved=2.26` 表示子骨骼被推动了 | **推翻**：加多帧统计后 `moved-range=[0.00,0.00]`，15 帧 `max-of-max=0`。那个 2.26 是子骨骼到父骨骼的**固定距离**，不是位移 |

**连带排除的（同属确证，价值独立于 S2）**：

- 副本**不带**指回原角色的 `userData`（`copy-userData=0x0`，`verdict=copy-is-graph-invisible`）
  → 所有"经 userData 取原角色动画图"的方案彻底排除（这原本是最大的陷阱）；
- 副本**不带**任何控制器（`controllers=none`）→ KF 路径（`NiControllerManager` /
  `NiControllerSequence`，唯一有头文件支持的第二动画入口）在副本上不存在，只能自己造。

**最终判读**：即使我们自己把子骨骼的 `world` 逐个重算（不依赖引擎级联），
被采样的骨骼仍然纹丝不动，画面也不跟随。也就是说
**这条直绘路径下，节点变换的写入不会传导到 GPU 实际采样的骨骼数据**。

**唯一未被排除的候选**：`NiSkinInstance::boneMatrices`(0x48) /
`prevBoneMatrices`(0x50) / `skinToWorldWorldToSkinMatrix`(0x58) —— 这三个不透明缓冲区才是
着色器读的"输出端"，而 `boneWorldTransforms`(0x30) 只是它的输入之一。
需要附进程调试或渲染侧断言才能查清它们何时、由谁写入。**这是下一个专项的入口，
不是继续进游戏试错的题目。**

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
