# 独立行为图验证

更新：2026-10-27。本文是独立行为图实验的当前状态权威；旧契约、构建身份和逐轮记录在下文保留，不能把当时的预期或判读当成现状。

<a id="current-status"></a>

## 当前结论

| 关卡 | 截至 2026-10-27 的结果 | 证据边界 |
| --- | --- | --- |
| G1 | **PASSED，仅按方案 A 修订判据** | 空壳配对销毁、重复装载、采集范围内源基线全等、装载图保留无崩溃；[G1b v4](#g1b-v4-运行记录与-g1-判定2026-10-24-会话)。装载图停用/销毁、全局登记计数和资源收敛仍未验证。 |
| G2 | **NOT PASSED（但 2026-10-27 22:02 的 v20a 首次取得运动证据）** | v18 五阶段取证：双槽 `numTracks(+0x4)=21`（数量门满足）、`numData(+0x32)` 全程 0、onFraction 仅 step 0 pre-pipeline 为 1.0（slot 2 写入）后被首次 63587 复位为 0；t 对复位为 `{0,-MAXFLT}` 且 120 步内不累积，每步 Path A（`w245=0x101 b246=1`、双槽每步交换一次）。v19 眼位实验落在预注册分支 6：`root218=0`、`eye` 非空、`[graph+0x248]=0`、step 0 旧读槽 `f=1.0`，外层门全过，但 `63864()` 把臂块步长压到 `1.25*dt` 以下，t 未被改写（`arm-observed=no`），无 Path B；120 次 63581 全部返回而候选 pose 仍为 `bc160a8c2b3a5a58`、原始缓冲 0 次变化。v20 首轮 guard 通过后在入口前崩溃（越界诊断，已修为 v20a）。**v20a 运行**：`63589(mode0)` 到达（guard `ready`、修正链四跳可读），首步即回填 `numData=116 f=1.0`，step 2 起 Path B 且 `arm-observed=yes`，`gen0elem` 与双槽元素哈希变化，候选 raw pose `bc160a8c2b3a5a58`→`63f638a6ac08d955`(60)→`8fb4cd336c4395bf`(120)、120/120 次变化，同时源侧 pose 不变、三份源基线逐行全等。缺逐骨/有限值证据与视觉确认，故仍不放行；**v21 运行（22:18）**：三点 `entries=116 finite=116 nonfinite=0`、`moved=94`（60/120，门槛 16）、抽样骨 38/76 `moved=yes`，但 `source-equal=8/21/15` 命中第 3 条 → 判据停止；审计证明第 3 条为**误报**（候选/源 `poseLocal` 不同址；源哈希两轮不同而候选三点哈希逐位相同；`source-equal` 是十分量精确值相等，恒等/常量骨必然计入），见 [v20 记录](#g2-v20-run)、[v20a 记录](#g2-v20a-run)、[v21 契约](#g2-v21-contract) 与 [v21 运行记录](#g2-v21-run)；v22 已把该判据改为指针同一性 + “相等且在变化”计数，并于 23:03 运行通过：三点全有限、`alias=no`、60/120 `source-equal-moving=0`、`moved=94`、LHnd/LF30 在动 → **pose 内容证据成立**；G2 仍 NOT PASSED（跨批次持续性、语义对应与场景应用/视觉确认未证），见 [v22 契约](#g2-v22-contract) 与 [v22 运行记录](#g2-v22-run)；v23 已把“语义对应”做成可判定观测（活动 clip generator + 在其自身时间上采样对照）并部署待测（[v23 契约](#g2-v23-contract)）。 |
| G3–G6 | **完整关卡未通过/未执行** | 单次事件注入不等于 Equip→Idle→Unequip；OAR 上下文/历史/sink、完整有效骨输出与生命周期均未验收。 |

[v18 原始日志](diagnostics/CharacterPanel-g2v18-trace-20261026-2301.log)中源 pose 前后为 `c58dcc07593b5e75`，三份 12 行源基线块逐行全等；它同样只支持所采结构/姿态哈希不变，不证明全部共享可写资源、回调或全局状态都隔离。120 步×1/60 秒是**同一次暂停括号内的 2 秒模拟时间批次**，不是 120 个渲染帧或持续面板播放。实验默认 `IndependentGraph=off`，该轮 deploy 的 INI 已回 off；产品仍使用装备驱动播放器。

当前按[audit22](behavior-graph-construction-audit.md#audit22)修订生成链分类：63374/63595/63859/65232/65237提供clip元数据/visitor，不能推出manager登记必要；audit21壳图守卫只限制该路径。真实Generate槽有额外活参数，不直调SDK声明。63589→63568的mode0实际经58343/58376派发Generate，63568自身还写f=1，纠正旧跳转及唯一写入者读法。新[v20契约](#g2-v20-generation-entry)仅恢复63589(&mode0)，保持v19其余配方、off默认和源manager不写边界；不创建manager、fake sink或播种。最新实际运行为 v21（2026-10-27 22:18）：入口 63589 到达、pose 三点全有限且 94/116 条在变化，但 `source-equal>0` 命中第 3 条并已审计为判据误报；修正后的别名判据（指针同一性 + “相等且在变化”）待 v22 构建与一次运行，G2 未通过。

[audit23](behavior-graph-construction-audit.md#audit23) 已静态拆开三段职责：原生生成、63581 内的三条 poseLocal memcpy、外层 63856 的场景应用。双槽 fraction/t 门失败不等于整函数无复制；58423 的短函数/context 旧判读也已纠正。63856 在该写入者中不更新 scale，与手动采样转 local 的写入范围不同，但视觉因果仍未证明。v20 恢复一个外层入口变量，同时恢复多类内部职责，后续变化不能唯一归因 Generate。此次不改冻结代码/测试/包，不新增运行结果。

## 目标与边界

验证能否在不创建第二个 Actor、保持原角色行为图只读的条件下，复用原行为项目与状态机，让预览图自行激活动作，再接入 OAR。现有装备驱动播放器继续工作；本实验不替换它，不改变动作入口、DAC、IED、变体、装备或源图状态。

显示场景图副本不包含完整的行为图运行实例。`oar::PreviewContext`、`PreviewClip` 是既有加载/查询操作的载体，也不是独立图。借用 `hkRefPtr`、复制指针、复制对象内存或让剪辑时间前进都不能作为独立实例构造的证明。

先离屏验证，第一批行为实验仅有两个：原版 idle；一种单手武器的 Equip → Idle → Unequip。运行时节点、变量值、状态机、事件队列、时钟和输出必须隔离；静态资源只有在读写范围与生命周期得到证实时才允许共享。

## 来源与历史

1. 本地 CommonLib 基线 `94faaed0c60eddd8347767f2d4d29a97c93bde8c` 及当前维护的声明：`BShkbAnimationGraph`、`hkbCharacter`、`hkbBehaviorGraph`、`hkbCharacterSetup`、`hkbVariableValueSet`、`hkbNode`。本地声明含维护补充，基线提交号不代表所有字段均由上游验证；本轮审计记录保留所读文件哈希。它们提供布局与运行方法声明，不等于完整的原生构造协议。
2. OAR 3.1.6 源码固定在 `0c1c8b38a5323b9e26df41ff9d10ce801b984a45`：[角色到外层图/Actor 的解析](https://github.com/ersh1/OpenAnimationReplacer/blob/0c1c8b38a5323b9e26df41ff9d10ce801b984a45/src/OpenAnimationReplacer.cpp)、[条件读取](https://github.com/ersh1/OpenAnimationReplacer/blob/0c1c8b38a5323b9e26df41ff9d10ce801b984a45/src/Conditions.cpp)、[活动剪辑与回调](https://github.com/ersh1/OpenAnimationReplacer/blob/0c1c8b38a5323b9e26df41ff9d10ce801b984a45/src/ActiveClip.cpp)。本地逐项审计使用 `build/oar-review-316` 缓存；不复制上游实现，不新增依赖。
3. [HKX37 实测判读](history-animation.md#puppet)：第二个真实 Actor 在暂停期被手动推进 3300+ 帧，索引级决策与采样机制成立。暂停期一次性装备动作未出现；完整映射/有效骨覆盖未通过；第 5 按清理未执行。该结果既不是“暂停期推图失败”，也不是无第二 Actor 独立图的证明。
4. HKX37 所引用的 H05 隔离检查属于当时的 Actor/场景实验；新方案仍需重新证明源图状态不漂移。历史总结保留当时判读及后续修正，原件从[来源索引](history-sources.md)恢复，不能把其中“路线成立”升级为本方案 G1 通过。

## G1 静态所有权与 API 审计

以下是需要在未来构造结果中逐项比较的候选清单，不是已确认的所有权说明。字段可见或地址不同不能单独证明深层隔离。

| 对象/成员 | 可写或共享候选 | 必须补充的证据 |
| --- | --- | --- |
| `BShkbAnimationGraph`、内嵌 `characterInstance`、`holder`、`rootNode` | 独立外层容器与实例；源 Actor 身份可按设计借读 | 创建/销毁配对、holder 关系、所有事件接收者以及对源 manager 的影响 |
| `characterInstance.behaviorGraph` / 外层 `behaviorGraph` | 运行实例必须隔离；不能默认二者相同 | 原生链接路径、模板与运行实例对应关系 |
| `rootGeneratorClone`、`activeNodes`、模板/克隆映射、转换数据、伪随机生成器、作业与分区内存 | 可写运行状态，必须独立 | 深克隆范围、递归别名、分配域、线程/作业完成边界 |
| `variableValueSet` | 可写变量值，必须独立 | 值及类型映射来源、合法读写入口；私有数组不得按猜测偏移读取 |
| `eventQueue`、驱动器、world、worldFromModel、poseLocal、generatorOutputs | 事件、物理和输出可写；不得借用源输出 | 输出生命周期、队列隔离、事件路由、IK/物理依赖与释放路径 |
| `boneNodes` / `numAnimBones` | 源骨挂点只读；未来输出挂点属于副本 | 全部有效骨的语义映射；表长度不等于有效覆盖 |
| setup、animationSkeleton、projectData、行为 data、rootGenerator、绑定集、projectDBData | 仅为可共享资源候选 | 哪些字段会被延迟初始化或修改、谁持有资源、加载/卸载顺序 |
| setup 的 mirroredSkeleton、criticalSection、绑定集 | 不可因位于 setup 就假定不可变 | 镜像缓存、锁和运行绑定更新的真实所有权 |
| `hkRefVariant` payload / 未知成员 | 所有权未核实 | 本轮只取已声明包装器里的指针身份，不解引用 payload 或遍历未知结构 |

`IAnimationGraphManagerHolder::ConstructAnimationGraph` 已声明为虚函数（slot 05），`TESObjectREFR`、`SimpleAnimationGraphManagerHolder`、`WeaponAnimationGraphManagerHolder` 有 override。2026-10-23 的[二进制审计](behavior-graph-construction-audit.md)确认五种实现共用分配/构造/赋引用模板；构造本身不加载项目、不登记 manager。后来已定位 63585 装载、字符级激活、63602 生成管线与 63587 更新包装，并按批准实验实际调用，结果见下文各轮记录。不能继续把“加载/激活未定位”当现状，也不能把空壳配对释放推广为已装载图可安全销毁。`BSAnimationGraphManager::QueryAnimations` 的现有 AE relocation 为 0，仍不能作为 AE 1.6.1170 可调用证据；OAR holder/rootNode 与事件路由须按 G4 另验。

`hkbNode` / `hkbBehaviorGraph` 的 Activate、Update、Generate、Deactivate 声明只能说明运行接口存在；未完成实例装配时禁止调用。裸 `hkbCharacter` 也不满足 OAR 从其反查 `BShkbAnimationGraph` 容器和 holder 的假设。

OAR 隔离还包括图外状态：图变量和 `IsWeaponDrawn` 可通过 Actor 读取；Random、顺序、PlayOnce 的状态可能按 Actor/ref、子包或剪辑作用域保存；`ActiveClip` 可注册图事件 sink，并沿源 manager 回调。图对象地址不同不能消除这些共享关系。

## 六道关卡

每关保留原始记录、构建身份、配置、初始条件、操作顺序、判据与失败点。前关未通过不得开始后关。遇到未知 ABI、需要写源图/创建 Actor、不能解释的别名或无配对销毁路径，立即停止该实验；保留日志后回到方案评审，不引入猜测调用或隐藏回退。

### G1：构造、链接、所有权与销毁

先确认目标游戏版本下的创建入口、分配器、链接顺序、holder/manager 容器要求、停用与销毁入口，再创建一个离屏候选。逐项记录源与候选的对象身份、运行状态归属、共享资源理由、事件接收者、引用/登记/任务数量与逆序销毁结果。源只读基线只提供之后对照的坐标。

原始完整门槛（保留为所有权/生命周期验收目标）：原生装配成功、可写状态隔离且共享有依据、源 manager/Actor 无副作用、逆序销毁回到基线；别名跨入可写状态、借用源输出或无配对释放均不能通过完整门槛。**当前 G1 的 PASSED 仅采用 2026-10-24 [方案 A 修订](#2026-10-24-方案-a-修订销毁判据经-g1b-v3-崩溃后评审)**：[G1a](#g1a-运行记录2026-10-24-会话)证明空壳配对销毁；[G1b v4](#g1b-v4-运行记录与-g1-判定2026-10-24-会话)重复装载并保留候选，采集范围内源基线全等、无崩溃。装载图停用/销毁移交后续生命周期验证，尚未通过；全局登记计数、资源收敛和全部运行期写隔离仍 UNVERIFIED。

### G2：原版暂停期单边推进

G1 通过后，在无 OAR 的原版基线只推进候选图。使用固定记录的时间步，采集候选剪辑时钟与输出姿态、源活动节点/变量/时钟/骨骼前后差分。窗口内禁止合法装备操作混入对照；用户换装备后重新建立基线。

通过：候选原版 idle 持续、有意义地前进，必要骨的有效变换变化可验证，源保持对照；时钟、索引、状态机、元素头或姿态哈希变化单独都不够。**当前 G2 NOT PASSED，最新 v22（2026-10-27 23:03）**：`poseLocal` 层已取得 pose 内容证据——116/116 条在三点全有限、94 条在变化、LHnd/LF30 两根必要骨在动、`alias=no` 且 `source-equal-moving=0`、源侧全程不变。缺口收敛为“持续”（单批 120 步＝2 秒模拟，无跨批次证据）、“有意义”（未与 HKX/原版相位对应）与场景应用/视觉确认；见 [v22 运行记录](#g2-v22-run)。v23 已实现“有意义”的可判定版本并部署待测：[v23 契约](#g2-v23-contract)。旧 v2/v17 只代表当时进度，源不变结论限于已采结构和 pose 内容。装载图生命周期与后续关卡不由此通过。

前置激活/推进审计及 v1–v17 实现记录已保留；[最初 G2 契约](#2026-10-24-g2-契约drive-模式实验设计)的 63586/裸 Update 配方后来迭代为字符级 58453 激活、63602 管线、63587 包装与 v17 的 63581 直调。调用入口已定位不等于输出链可用，当前后续问题以[有界审计清单](#next-audit)为准。

### G3：图原生的 Equip → Idle → Unequip

正常游戏运行时先记录同一单手武器的变量写入与事件先后顺序、节点激活/退出。把等价输入送到候选图；不能手动指定动作族/HKX 来“通过”。输出事件先只收集，不派发到源 Actor。

通过：候选状态机自行完成相同顺序，并能按事件和相位与原版轨迹对应。事件丢失、阶段变量仍读源值或靠播放器强制切片均失败。完整 G3 当前未执行；G2 定位轮已派发一次 WeapEquip 并观察到状态转移，不能计为整段装卸链或原生 OAR 一致性通过。

### G4：OAR 上下文、历史与回调隔离

真实候选节点走 OAR 原生激活、更新、中断及重查流程。逐条件记录来源，用哨兵实验故意让候选阶段变量与源变量不同，确认实际读取谁；身份/装备允许按明确设计借读，阶段状态必须对应候选。记录候选与胜者、变体、播放相位、重查原因、事件 sink、Actor/ref/子包/clip 的状态作用域。

对照运行 A：直接继续世界动作；运行 B：中间运行预览，再继续世界动作。两次保持相同初态与随机/顺序控制，检查预览是否消耗世界 Random、顺序或 PlayOnce 历史。关闭预览只清理自己的登记，不能清空源 Actor 的全部 OAR 状态。DAC 要在事件捕获和隔离证明后另行接入既有受控通道。

通过：读取来源符合预览语境，历史/回调/清理隔离成立。相同替换路径但相位不同仍需解释。当前未执行。

### G5：完整输出姿态对照

按相同初态、事件和播放相位对齐，比较全部有效贡献层、权重、混合参数、播放速度及最终骨骼变换；记录 IK/后处理及坐标空间。实验开始前固定位置、旋转、缩放误差阈值及其依据，不能看结果后放宽。按有效骨名称与语义证明完整覆盖，不能拿骨表长度代替。

通过：完整层/骨覆盖且误差满足预先约定阈值；相同选片而输出不一致仍失败。当前未执行，阈值尚未制定。

### G6：生命周期与资源恢复

记录创建前/运行中/销毁后的实例、运行节点、资源引用、事件 sink、OAR 登记、任务和输出数量。重复会话、快速换装、重捕获、关闭背包、读档；旧任务带会话代数失效，先停止工作再逆序解除回调和资源。

通过：计数回到基线，没有源状态漂移或持续积累。崩溃未出现不能代替清理证据。当前未执行。

## G1 实验契约（2026-10-23 设计，未实现）

> **历史契约标记（2026-10-26）**：标题和原预注册条件保留。实验已于 10-24 实现/运行，并在方案 A 下修订销毁判据；以下“未实现/不得调用”限定该历史阶段，当前结论见[文首](#current-status)。

本契约把[二进制审计](behavior-graph-construction-audit.md)的构造/销毁/加载/共享语义转成可执行的 G1 运行实验：只做"构造 → 装载 → 对照 → 逆序销毁"，不激活、不推进、不写源图、不挂 OAR。实现前不得运行；实现只允许出现在下述边界内。

### 实验分级（前一级通过前不得进入下一级）

| 级 | 动作 | 目的 | 停止条件 |
| --- | --- | --- | --- |
| G1a | 自持 holder（槽位语义见下）+ `holder=nullptr` 构造空壳，然后只经引用计数释放 | 在运行中验证工厂与配对销毁、无源副作用 | 构造返回空/崩溃、源基线漂移、释放后计数不回 |
| G1b | G1a + 按预注册优先级顺序以**完整项目路径 `const char*`** 调用加载 63585（≤4 个候选：`race->behaviorGraphs[sex].model` → `behaviorGraphProjectNames[sex]` → `rootBehaviorGraphNames[sex]` → `stringData` 重建；第三实参 false），首个成功即停；**成功后按[方案 A 修订](#2026-10-24-方案-a-修订销毁判据经-g1b-v3-崩溃后评审)保留候选（不销毁）** | 验证候选能完成原生装配（behaviorGraph/character/setup/rootNode），同时一次性关闭"引擎键来自哪个 race 字段"的取值问题 | 返回空、装配字段缺失、需要写源图、注册表计数不回 |
| G1c | G1b + 以源 Actor 作为 holder 重建一次对照 | 仅在 G1b 通过且后续需要源上下文时量化别名 | G1b 未通过；出现可写状态同址 |

### 候选 holder 与调用边界

- G1a/G1b 不实例化 holder 对象：直接复刻审计模板——`MemoryManager::Allocate(0x250, 0x10, true)` → 原生构造 63555（holder 实参 `nullptr`）→ `BSTSmartPointer` 持有。审计已证明五种 `Construct` 实现共享同一模板，vtable 只是派发点；不派生 `IAnimationGraphManagerHolder`，避免为引擎接口类自定义析构符号（CommonLibSSE 只声明、未定义 `~IAnimationGraphManagerHolder`）。理由：引擎自身以 `holder=null` 构造（Simple/Weapon）且该路径不向源 Actor 注册事件 sink/通道。
- 工厂实现必须复刻审计模板：`MemoryManager::Allocate(0x250, 0x10, true)`（68115）→ 构造 `BShkbAnimationGraph`（63555，签名 `this/holder/bool`，审计所得）→ 引用计数智能指针赋值；不得自行拼装成员。释放只允许丢弃最后一个 `BSTSmartPointer` 引用，禁止手动调用删除析构或 `Deallocate`。
- 加载只调用 63585。**（2026-10-24 续审计二/三修订并实现，v3）第二实参为 NUL 结尾的完整项目路径 `const char*`。候选值来源按预注册优先级：① `holder->GetRace()->behaviorGraphs[sex].model`；② `behaviorGraphProjectNames[sex]`；③ `rootBehaviorGraphNames[sex]`；④ `stringData->behaviorPath + projectName` 重建（69487 拼接规则）。顺序尝试、首个返回 true 即停，最多 4 次；已证实失败加载对空壳是零副作用操作（0xbb21a9 路径全空引用），重复尝试不引入新风险。`sex` 取 `TESNPC::GetSex()`（kFemale=1），非 NPC 按 0。第三实参 false：Actor/PlayerCharacter holder 的 Unk_06（vtable 槽 6）实测返回 `xor al,al`（false）。不得调用 63844/63845/63847 或任何 `hkbCharacter` 成员函数，除非先以只读日志确认；见[审计记录](behavior-graph-construction-audit.md#加载取值与前置链的续审计三2026-10-24g1b-v2-复盘)。**
- 禁止：Activate/Update/Generate/Deactivate、图变量写入、图事件、OAR 求值/`ActiveClip`、DAC、创建 Actor/REFR、写 `+0x1F0/+0x1F8/+0x200/+0x208/+0x218`。
- **保留（2026-10-24 方案 A）**：装载成功的候选不销毁——以堆上副本的 `BSTSmartPointer` 持有且永不释放（泄漏），静态析构与进程退出都不触发引擎析构链；空壳（构造失败、全部候选被拒）仍按引用计数销毁。
- 新设置 `[Experiment] IndependentGraph=off|construct|load`，默认 off，重启生效，独立于 `DetailedAnimationTrace`/`BehaviorGraphProbe`；新模块 `src/animation/behavior_graph/independent_graph.{h,cpp}`，不新增目标/依赖。

### 观测与预算

每级最多 24 行 `INDEPENDENT GRAPH`：候选 `graph/character/setup/behaviorGraph/projectDBData/projectName/rootNode/variableValueSet/activeNodes/generatorOutputs/eventQueue` 的地址与计数，以及 12 项身份比较（shell/load 各一组）；`load` 级另有 1 行 `phase=race-fields`（只读记录 `bg-model`/`root-name`/`project-field`/`string-data-behavior-path`/`source-project-name` 的**内容**）与每候选一行 `phase=load-try`（序号、来源、路径内容）+ `phase=load`（结果与装配字段）——2026-10-24 修订，v2 实测证明"日志不含所传内容"会浪费整个运行。构造前与销毁后各一次完整源基线（复用 `BehaviorGraphProbe` 的 12 行固定字段），构造后使用紧凑身份比较，避免三份完整基线。全局注册表计数（单例 `0x315c950` → 数组 `+0x1F8` → 计数 `+0x30`，AE-only 审计地址）未实现，日志固定输出 `registry-probe=unavailable`，报告中必须注明该侧效应未观测。只读、无指针逃逸、无线程间共享状态。

### 预注册判定（不得事后放宽）

通过（全部满足）：

1. **装配完整**：G1b 后 `behaviorGraph`、`projectDBData`、`projectName` 与 `behaviorGraph->data` 非空；`characterInstance` 与源不同址。且（2026-10-24 修订）候选 `projectName` 的**内容**等于被接受候选路径的文件名段去扩展名（与[审计](behavior-graph-construction-audit.md#第二实参形态与拒绝路径2026-10-24-续审计二)的 69489 写入点一致）。`rootNode`/`setup`/`bindingSet` 为空属预期（无骨架挂接、无场景节点），必须记录但不计失败；G2 需要时另行设计。
2. **可写状态独立**：候选 `graph/character/activeNodes/variableValueSet/generatorOutputs/eventQueue` 与源**不同址**。
3. **共享资源可解释**：五个 hkRefPtr 字段与 `projectDBData`/行为数据与源**同址**（引用计数 +1 属预期，不计漂移）。
4. **源基线不变（2026-10-24 方案 A 修订）**：构造前 ≡ **装载后（候选存活期间）**；装载图的销毁后基线不再适用（销毁移至 G2 生命周期），空壳路径维持"销毁后 ≡ 构造前"。
5. **逆序销毁（2026-10-24 方案 A 修订）**：空壳路径维持引用计数配对释放、无崩溃/断言；**装载图不再销毁**——引擎不支持未激活图的销毁（[续审计四](behavior-graph-construction-audit.md#已装载图析构的崩溃定位2026-10-24-续审计四)），候选按保留流程存活至进程结束，其销毁证据由 G2 生命周期取得。注册表计数未观测（`registry-probe=unavailable`），回落保持 UNVERIFIED。
6. **无侧效应**：源 manager 的图数量不变；OAR 活动查询与 `ActiveClip` 回调未对候选触发；无新增 Actor/REFR。

失败（任一）：可写状态同址、源基线漂移、注册表计数不回落、构造/装载返回空或崩溃、必须写源图才能完成。崩溃未出现不能替代清理证据。

### 停止与回退

任一失败/未知：保留日志与构建身份行，把设置改回 off 重启，不进入下一级，不做"调整判据后重试"。装备变化后重建源基线；F7 重捕获不得叠加构造。G1 通过前 G2–G6 保持锁定，本契约不定义推进/激活。

### 2026-10-24 方案 A 修订（销毁判据，经 G1b v3 崩溃后评审）

[续审计四](behavior-graph-construction-audit.md#已装载图析构的崩溃定位2026-10-24-续审计四)确立引擎事实：`~BShkbAnimationGraph` 在 `behaviorGraph` 非空时调用 `hkbBehaviorGraph::Deactivate`（槽位 7），后者无条件读取 `activeNodes`（Activate 时才分配）——**装载后未激活的图没有安全销毁路径，"不激活而销毁"不在引擎生命周期内**。据此修订：

- **销毁证据重定义**：G1 的配对释放证据由两部分承担——①空壳销毁（G1a 已通过）；②装载图的销毁**推迟至 G2 生命周期**（激活 → 推进 → 停用 → 销毁）内取得。
- **v4 保留流程**：装载成功后，候选以**永不释放的持有智能指针**（堆上副本，刻意泄漏；静态析构与进程退出都不触发引擎析构链）保留至进程结束；每进程 ≤1 张装载图，仅在实验开启时发生，独立图功能化之前不作为可交付路径。
- **v4 预注册判据（不得事后放宽）**：①装载成功复现（首个 `race-bg-model` 候选 `accepted`）；②**装载后源基线**（候选存活期间采集）与构造前逐字全等；③`phase=end retained=true`、无崩溃；④①–③ 全过 → **G1 PASSED（修订判据）**，G1c 跳过（`holder=nullptr` 已证充分，源 holder 别名量化移至 G4 的 OAR 语境）；⑤装载后基线漂移或再次崩溃 → 维持 NOT PASSED，回评审。

### 2026-10-24 G2 契约（drive 模式，实验设计）

> **历史预注册标记（2026-10-26）**：本节保留当时配方、预期和判据，不将后验结果改写为事前预测；后续反例及当前证据边界见[当前结论](#current-status)与 [v17 后验勘误](#g2-v17-运行记录2026-10-26-会话谓词翻假达成与采样门23落定)。

G2 验证：候选图可被激活并以固定时间步推进，且源全程不变。新设置值 `IndependentGraph=drive`，复用 G1b 全流程（构造 → 装载 → 保留，全部观测行不变），装载成功后**同请求、同暂停括号内**继续：

1. **激活**：单次调用引擎激活包装器 **63586（候选）**——[续审计五](behavior-graph-construction-audit.md#激活与推进入口g2-前提2026-10-24-续审计五)确立的引擎入口，零自建结构、零管理器依赖。
2. **驱动**：以 58315 自建 context（`character = 候选 characterInstance`，world/attachmentManager/animationCache 空）直调 `hkbBehaviorGraph::Update`（58329，vtable 槽位 5），`DriveSteps = 120` 次、固定时间步 `1/60 s`（预注册常量，不调 63587——其更新记录依赖管理器，候选无 manager）。
3. **观测（分层，预注册）**：
   - 结构层：`activeNodes(+0x98)` 指针与计数（NodeList+8 dword，[续审计四](behavior-graph-construction-audit.md#已装载图析构的崩溃定位2026-10-24-续审计四)的崩溃指令即读此处）、`+0xA0/+0xA8`、`variableValueSet(+0xD8)`、激活标志(+0x12C)、`generatorOutputs[0/1]`、`poseLocal/numPoseLocal`——激活前/激活后/驱动后各一次；
   - 运动层：FNV-1a 64 姿态哈希——**源侧对照**（激活前 vs 驱动后：暂停括号内源不应被推进，必须全等）与**候选侧**（激活后 / 第 60 步 / 第 120 步：idle 播放应互不全等）；`poseLocal` 空则记 `unavailable`。
4. **预注册判据（不得事后放宽）**：
   - ①激活后 `activeNodes` 非空、激活标志 = 1；
   - ②驱动后候选出现前进证据：候选三级姿态哈希不全等；若姿态不可用，以结构层由空转非空的变化为准并逐项解释，不得默认通过；
   - ③**源全程不变**：激活前（即装载后）与驱动后源基线逐字全等、compact Identity drift = none、源姿态哈希前后全等；
   - ④无崩溃/断言，同会话后续面板开合正常。
   - ①–④ 全过 → **G2 PASSED**；任一失败 → 崩溃/异常日志归因后回评审。
5. **停止与回退**：同 G1（INI 改回 off）；drive 每进程一次；候选保留策略不变（永不释放的持有指针）。`registry-probe=unavailable` 保持。
6. **2026-10-24 v2 修正（[续审计六](behavior-graph-construction-audit.md#姿态写入链与字符级激活2026-10-24-续审计六)）**：首轮判读显示图级激活 63586 绕过了 pose 分配——引擎的激活顺序是"**先 pose 缓冲、后 Activate**"，由**字符级激活 0xabd630**（守卫 `+0x12C==0`，先调 58466 AllocatePose——从 `setup.animationSkeleton` 读骨骼数分配 `poseLocal` 并置 `deletePoseLocal=1`——再 58315 context + Activate）完成。v2 改动：激活改调 0xabd630（地址库无 id，AE RVA）；**预注册守卫**——激活前若 `setup.animationSkeleton == 0` 则记录 `skip=setup-skeleton-missing` 干净结束（不调用、不崩溃，结论直接指向 animationSkeleton 填充链审计）；判据①追加"**`poseLocal` 非空（AllocatePose 生效）**"，判据②的三级姿态哈希在 poseLocal 可用后才有意义。首轮运行记录见[运行记录](#g2-drive-运行记录2026-10-24-会话)。

### 2026-10-24 G2 v7 契约（引擎管线驱动，经续审计七设计）

> **历史预注册标记（2026-10-26）**：本节保留当时配方、预期和判据，不将后验结果改写为事前预测；后续反例及当前证据边界见[当前结论](#current-status)与 [v17 后验勘误](#g2-v17-运行记录2026-10-26-会话谓词翻假达成与采样门23落定)。

[续审计七](behavior-graph-construction-audit.md#管理器每帧管线与记录全解2026-10-24-续审计七)把 v6 时代的"管理器动画选择"结论修正为：记录无动画身份、63575 = LOD 解析器、63583 = 排空+簿记（第三实参 0 时无害）；缺口 = **生成遍历（65237）从未执行**。据此 G2 判据按本契约重定义，v5 的裸 Update 驱动作废：

- **设置不变**：`IndependentGraph=drive`；G1b 装载全流程与 58453 字符级激活（含 `setup-skeleton-missing` 守卫）照旧，全部既有观测行不变。
- **每步驱动（DriveSteps=120 × 1/60 s 不变）**，复刻引擎通知中心槽 5（[graph_helper](behavior-graph-construction-audit.md#管理器每帧管线与记录全解2026-10-24-续审计七)）的配方：
  1. **生成管线**：栈上 `63555(&壳, nullptr, false)` → **`63602(&壳, 候选)`**（事件收集 + 65149/65232/65237 生成遍历写 `generatorOutputs` + `+0x12E=1` + 零 dt 63587 Update + 63583 排空 + 63584）→ `63556(&壳)`——每步一对空壳构造/真析构（引擎自身对空壳的用法，[续审计四](behavior-graph-construction-audit.md#已装载图析构的崩溃定位2026-10-24-续审计四)的崩溃仅在已装载图上）。
  2. **实 dt 推进**：`+0x12E=1` → 58329(behavior, ctx, 1/60)——v5 已证配方（63602 内部 Update 为零 dt，不推进状态）。
  3. **采样**：`候选+0x240 = numPoseLocal` 镜像 → 63589——v4 已证配方（63587 入口会把 `+0x240` 清零，镜像必须在采样前）。
- **观测**：既有三级姿态哈希 + 结构快照不变；`gen0elem` 元素哈希（已在 probe 行）现在应随生成而变化；新增一行 `phase=pipeline steps=120 shell=pairs` 汇总（预算 +1 行）。
- **预注册判据（不得事后放宽）**：
  - ①候选前进证据：三级姿态哈希不全等；**或** `gen0elem`/`gen1` 元素哈希随步变化而姿态仍冻结——后者按"采样链归因"记录（63581 轨道偏移 vs 候选输出布局的新审计），**不得默认通过**；
  - ②源全程不变：三处源基线逐字全等、compact drift=none、源姿态哈希前后全等；
  - ③无崩溃/断言，同会话后续面板开合正常；
  - ①（前进证据成立）+②+③ → **G2 PASSED（v7 判据）**；生成无产物（gen 元素恒定）→ 归因"默认态无激活生成器，idle 需事件注入"，作为 G3 事件机制的入口证据记录，G2 保持 NOT PASSED。
- **失败归因表（预注册）**：gen 元素变化 + 姿态冻结 → 采样链（63581/63589 与候选输出布局）；gen 元素恒定 → 默认态无生成器（转 G3 事件注入设计）；崩溃 → 归因后回评审，INI 回 off。
- **停止与回退**：同 G2（INI 改回 off）；drive 每进程一次；候选保留策略不变。新增调用面 = 63602/63555/63556（引擎自身配方）+ 既有 58453/58315/58329/63589，不新增依赖/目标。

### 2026-10-24 G2 v8 契约（inject 模式：事件注入判别，经续审计八设计）

> **历史预注册标记（2026-10-26）**：本节保留当时配方、预期和判据，不将后验结果改写为事前预测；后续反例及当前证据边界见[当前结论](#current-status)与 [v17 后验勘误](#g2-v17-运行记录2026-10-26-会话谓词翻假达成与采样门23落定)。

[续审计八](behavior-graph-construction-audit.md#事件派发链与槽-2-输出门2026-10-24-续审计八)把事件派发原语定位到 **58377**（hkbBehaviorGraph 槽 6 落体；Update 尾部经槽 6 桩派发队列事件；批量派发器 0xab313e 同构），且证实事件名不在 exe、由 ProjectDBData 名字→id 映射运行期解析。据此设计注入判别实验，回答 v7 遗留判别："65237 无激活生成器" vs "管线到达性/输出绑定门"：

- **设置**：新值 `IndependentGraph=inject`。流程 = v7 全部（装载 → 激活 → 每步 63602 管线 + 实 dt Update + 镜像 + 采样 ×120，观测行不变）+ 两个新增阶段。观测预算修订为 30 行（预注册）。
- **阶段 1 事件表只读查询（注入前，激活后；v8d 修订）**：先保留 `phase=dbdump` 原始 qword 行；新增 `phase=events` 行输出行为图事件表计数与前 16 个真实事件名（证据）；名字→id 解析用 **行为图自带 `stringData->eventNames` 线性扫描**（`hkArray<hkStringPtr>`、掩 managed 位、**id 即下标**——状态机转移映射键控的 id 空间）按**预注册名字表**顺序查询（v8e 修订：按 v8d `phase=events` 证据行重定，原 `MT_Idle`/`MoveStart`/`WeaponDraw`/`SyncForward` 经证非图事件）：`WeapEquip` → `SprintStart` → `FootLeft` → `FootRight`，每名字一行 `event-lookup name='X' id=N source=graph-events`（miss 为 −1）。原 ProjectDBData 哈希表遍历经两轮崩溃 + dbdump 判读证伪（墓碑键不可解引用，引擎只做指针比较），unkD0 判读为剪辑名表，均已移除。
- **阶段 2 注入**：取首个 id≥0 的名字，构栈上 24 B hkbEvent `{id, 0, payload=0, sender=0}`，在 drive 循环第 0 步管线之前**单次直调 58377(behavior, ctx, &event)**（与既有 slot-5 直调同类；不写源图、不触共享资源引用计数）。全部 miss 则 `skip=no-known-event` 干净结束。
- **预注册判据（不得事后放宽）**：
  - ①**前进证据**：注入后候选姿态三级哈希不全等，或 gen 元素哈希相对注入前基线出现变化 → **事件注入即最后缺口，生成链活**；G2 前进证据以"事件驱动"形式成立，注入路径记为 G3 的输入原语；
  - ②注入后 gen 元素仍恒定 → 按预注册归因转 **槽 2 输出绑定门**（[续审计八](behavior-graph-construction-audit.md#事件派发链与槽-2-输出门2026-10-24-续审计八)第 6 条：physicsWorld/ragdollDriver 双空门）→ v9 设计评审（受控借用源 world 需风险登记），G2/G3 维持未过；
  - ③源全程不变（基线逐字全等、源姿态哈希前后全等、drift=none）；④无崩溃/断言，同会话面板正常。
- **停止与回退**：同 v7（INI 回 off）；inject 每进程一次；候选保留策略不变。新增调用面 = 58377 直调（+ 只读 map 遍历），不新增依赖/目标。

### 2026-10-24 G2 v11 契约（ragdoll 实例借用判别，经续审计十设计）

> **历史预注册标记（2026-10-26）**：本节保留当时配方、预期和判据，不将后验结果改写为事前预测；后续反例及当前证据边界见[当前结论](#current-status)与 [v17 后验勘误](#g2-v17-运行记录2026-10-26-会话谓词翻假达成与采样门23落定)。

[续审计十](behavior-graph-construction-audit.md#骨骼提供者真身与填充链2026-10-24-续审计十)把 63630（63602 槽 0xB 出参）的骨骼提供者证认为 **hkaRagdollInstance**（`graph+0xF0` = `characterInstance.ragdollDriver` → `driver+0x88` 实例 → `rigidBodies` 表 `+0x10`、计数 `+0x18`），并把填充方定位到引擎完整装配 63870 的尾链（63952 建实例 → 58285 SetRagdoll）——候选装载链止步 63949，+0x88 恒 null，v10 的 world 借用因此通过全部门而绑定循环计数仍 0。v11 = 在 v10 借用窗口内**同时借用源的 ragdoll 实例指针**，判别"逐骨绑定循环即最后缺口"：

- **设置**：不变（`IndependentGraph=inject`）。流程 = v10 全部 + 驱动窗口内写 `candidate 驱动 +0x88 = 源实例指针`（raw 指针拷贝、不动引用计数——恢复先于任何可能释放者），循环后与 world 同点恢复原值（预期 null）。`phase=driver` 观测行同构建修正读链（`driver+0x88` → 实例 → `{+0x10 表, +0x18 计数}`，并补 `driver+0x80` 的 character 回指；续审计九部署的旧读链读的是 blend-out float 与模式字节，从未上机取证）。`phase=borrow` 行扩为 `world={} ragdoll=borrowed|skipped-no-driver|skipped-no-source-ragdoll saved={}`；`phase=pipeline` 行增 `ragdoll-restored=`。
- **预注册判据（不得事后放宽）**：
  - ①**前进证据**：驱动后候选姿态三级哈希不全等，或 gen 元素哈希 / 元素 `+32/+0x38` 绑定字段相对基线变化 → 逐骨绑定循环即最后缺口，G2 前进证据成立；候选侧正解转入 G3+（63952 实参重构，按候选自身项目数据建实例，不再借用）；
  - ②借用执行（`ragdoll=borrowed`）而输出仍恒定 → 归因转**循环内逐骨元素配对 / 生成遍历到达性**（续审计十第 5 条之外的新缺口），G2/G3 维持未过；
  - ③`skipped-no-source-ragdoll` → 证伪"源 rigidBodies 已填充"前提（与 v9 源元素 `+32=0x74` 证据冲突，需复审 63630 循环体与元素字段来源）；
  - ④源全程不变（基线逐字全等、姿态哈希前后全等、drift=none）；⑤无崩溃/断言，同会话面板正常。
- **风险登记**：63630 逐刚体把体挂进（已借用的源自身）world——对源幂等（其每帧管线本就如此）；逐骨 0x58 输出记录与 `{count|0x80000000, ptr}` 描述符走引擎全局簿记，与任意图的帧管线同型；120 步 × 源骨数（约 116）为上界。
- **停止与回退**：同 v10（INI 回 off）；inject 每进程一次；恢复块与 world 恢复同点（循环后立即）。

### 2026-10-25 G2 v12 契约（槽 2 直调判别，经续审计十一设计）

> **历史预注册标记（2026-10-26）**：本节保留当时配方、预期和判据，不将后验结果改写为事前预测；后续反例及当前证据边界见[当前结论](#current-status)与 [v17 后验勘误](#g2-v17-运行记录2026-10-26-会话谓词翻假达成与采样门23落定)。

[续审计十一](behavior-graph-construction-audit.md#槽-2-真身与元素字段写入链2026-10-25-续审计十一)证认槽 2（63621）= AddRagdollToWorld 且其头部即 +32/+38 的写入者（过三门后无条件 `numData=0`、`onFraction=1.0`），而 63602 只在 63643（帧内当前动画哈希，经槽 0xC 从 projectDBData +0x170/+0x178 取对象）返回 >0 时才调它——候选该链不满足，v10/v11 的 120 次遍历从未触发槽 2，候选 +38=0.0 由此解释。v12 = 直调一次 63621（候选），判别"槽 2 执行是否就是缺口"：

- **设置**：不变（`IndependentGraph=inject`）。v11 的实例借用移除（已证非载荷，候选自有 18 体）；流程 = v10 world 借用 → **直调 63621（候选）**（前置 `phase=slot2` 行：added 返回值 + 调用前后 +32/+38）→ v7/v8 全部驱动 → 驱动后先调 63622（槽 3，RemoveRagdollFromWorld）再恢复 world=null（`phase=pipeline` 行增 `ragdoll-removed=` 与 `tracks-end[]`）。观测预算 30 行（净 +1）。
- **预注册判据（不得事后放宽）**：
  - ①`phase=slot2 added=true` 且 `tracks-after +38=1.000` → 槽 2 在候选可执行、写入者身份证实；若驱动后候选姿态三级哈希不全等或 gen 元素哈希变化 → **槽 2 即最后缺口，G2 前进证据成立**；
  - ②+38=1.0 而姿态/元素仍恒定 → 残余缺口 = **numData=116 的回填路径**（生成/绑定侧；下一审计 = 锚定轨道头写入者的全镜像 store 扫描），G2/G3 维持未过；
  - ③`added=false` → 直调时三门复败（与 v10/v11 门证据冲突，复审 63621 门读）；
  - ④源全程不变（基线逐字全等、姿态哈希前后全等、drift=none）；⑤无崩溃/断言，同会话面板正常；`ragdoll-removed=false` 时登记并暂停后续借用类实验。
- **风险登记**：63621 激活候选自有 18 刚体并挂入（借用的源）world——与引擎在源上的每帧行为同型；驱动结束即以 63622 对称移除，先于 world 恢复 null；单次执行、每进程一次。
- **停止与回退**：同 v10（INI 回 off）。

### 2026-10-25 G2 v13 契约（槽 0xC 直调判别，经续审计十二设计）

> **历史预注册标记（2026-10-26）**：本节保留当时配方、预期和判据，不将后验结果改写为事前预测；后续反例及当前证据边界见[当前结论](#current-status)与 [v17 后验勘误](#g2-v17-运行记录2026-10-26-会话谓词翻假达成与采样门23落定)。

[续审计十二](behavior-graph-construction-audit.md#采样入口五道门与-numdata-写入者2026-10-25-续审计十二)定位 numData 写入者 = 63568（`[tracks+0x32] = word [graph+0x240]`），其唯一调用者 = 每步采样入口 63589——候选在 63589 的**首道门 `[graph+0x110] ≠ 0` 每步直接返回**，从未采样；+0x110 的唯一填充者 = 槽 0xC（63631，从 projectDBData +0x170/+0x178 按 dl 选择），引擎只在同一 63643 门后调用（审计十一与十二是同一缺口的两个切面：候选缺这一次装配）。v13 = 直调一次槽 0xC：

- **设置**：不变（`IndependentGraph=inject`）。流程 = v12 全部（world 借用 → 槽 2 直调 → 驱动 → 槽 3 移除）+ 槽 2 之后**直调 63631（候选， dl=false；+0x110 仍空则 dl=true 重试一次）**，新增 `phase=slot0c` 行（调用前后 `[graph+0x110]`、`[所选+0x20]`、`[graph+0x246]` 分支字节）。观测预算 32 行（净 +1）。
- **预注册判据（不得事后放宽）**：
  - ①`phase=slot0c after` 110 非空 且 `phase=pipeline tracks-end +32=0x0074` 且驱动后姿态三级哈希不全等或 gen 元素哈希变化 → **+0x110 装配即最后缺口，G2 前进证据成立**；
  - ②+0x110 填充而 +32 仍 0x0000 → 转采样内部分支（63568 dl 路径 / [graph+0x246] 分支 / 63581 路径，下一审计）；
  - ③两次调用后 +0x110 仍空 → 复审 63631 前置（projectDBData +0x170/+0x178 内容）；
  - ④源全程不变；⑤无崩溃/断言，同会话面板正常。
- **风险登记**：63631 对所选对象做 AddReference 并读 `[所选+0x28]→[+0x10]` 与 `[ragdoll+0x40]` 骨架——引擎原生簿记，单次执行；+0x110 为候选自有字段，随候选生命周期释放。
- **停止与回退**：同 v12（INI 回 off）。

### 实现状态（2026-10-24）

> 此段为 10-24 构建/部署迭代记录，保留身份和当时待办；当前状态见[文首](#current-status)。

`[Experiment] IndependentGraph=off|construct|load|drive` 已实现（默认 off、重启生效，独立于 `DetailedAnimationTrace`/`BehaviorGraphProbe`）：[independent_graph.cpp](../src/animation/behavior_graph/independent_graph.cpp) 在首次 `panel-open` 请求里构造一个 `holder=nullptr` 的空壳图；`load`/`drive` 模式（v3 取值 + [方案 A 修订](#2026-10-24-方案-a-修订销毁判据经-g1b-v3-崩溃后评审)保留）：先以 `race-fields` 行只读记录源 holder 种族的三个候选字段与 stringData 内容，再按预注册优先级顺序（bg-model → project-name → root-name → stringData 重建）以完整路径 `const char*` 调用 63585，最多 4 次、首个成功即停，第三实参 false；**成功后候选以永不释放的持有智能指针保留（不销毁），仅空壳走引用计数释放**。`drive` 模式当前为 **v7**（[G2 v7 契约](#2026-10-24-g2-v7-契约引擎管线驱动经续审计七设计)，取代 v1–v6 的裸 Update 路线）；新值 **`inject`**（[G2 v8 契约](#2026-10-24-g2-v8-契约inject-模式事件注入判别经续审计八设计)）在 v7 全流程前追加只读事件名查询与单次 58377 派发在装载成功后同括号内：激活前补源基线 → **字符级激活 58453**（先 58466 从 `setup.animationSkeleton` 分配 pose 缓冲、再 Activate；骨架缺失时预注册守卫 `skip=setup-skeleton-missing` 干净跳过）→ 58315 自建 context（raw 零初始化存储——`hkbGeneratorOutputListener` 无完整类型，hkbContext 不可在此侧构造/析构）→ 每步 **63555 栈壳 → 63602(&壳, 候选) → 63556 生成管线 + 58329 实 dt Update + `+0x240` 镜像 + 63589 采样**，共 120 步 ×1/60 s → 三级姿态哈希（源对照 + 候选 0/60/120 步）、gen 元素哈希与结构快照。每进程只跑一次，AE-only 且版本 ≥ 1.6.1170。**G1 已 PASSED（[v4 记录](#g1b-v4-运行记录与-g1-判定2026-10-24-会话)）；G2 v1–v6 六轮 NOT PASSED（判据②，缺口经[续审计七](behavior-graph-construction-audit.md#管理器每帧管线与记录全解2026-10-24-续审计七)重新归因为生成遍历缺失）；v7 已运行（[运行记录](#g2-v7-运行记录2026-10-24-会话)）；v8 五轮运行（两崩、两跳过、v8e 注入+转移成功）+ v9 gates 裁决（槽 2 假设坐实，候选只缺 world）；v10 已运行（限时借用源 world 安全但无效——绑定循环计数 0）；[续审计九](behavior-graph-construction-audit.md#绑定循环的骨骼映射语义与-vtable-澄清2026-10-24-续审计九)归因逐骨绑定、[续审计十](behavior-graph-construction-audit.md#骨骼提供者真身与填充链2026-10-24-续审计十)证认提供者为 hkaRagdollInstance 并定位填充链（63870 → 63952 → SetRagdoll）；**v11（[契约](#2026-10-24-g2-v11-契约ragdoll-实例借用判别经续审计十设计)：修正 phase=driver 读链 + 驱动窗口内同时借用 ragdoll 实例）已实现并运行（构建 `…a32766a080+dirty-cl94faaed0c6-20261024T160418Z`，DLL SHA-256 `E57C19A8…`、PDB `0C5509E7…`，2026-10-25 部署于 MO2，旧构建备份 `dist/deploy-backup-20261025-audit9/`）——判据②落定、审计十"+0x88 恒 null"推论被 driver 行证伪（候选本有 18 体实例），[运行记录](#g2-v11-运行记录2026-10-25-会话判据2落定与审计十推论证伪)；v12（槽 2 直调 + 槽 3 移除，构建 `…20261024T165857Z`）已运行——槽 2 首次执行、onFraction 写到 1.0、移除干净，输出仍恒定（[v12 记录](#g2-v12-运行记录2026-10-25-会话判据2落定与写入者运行期证实)）；v13（槽 0xC 直调填 +0x110）已运行——+0x110 早已填充、246=0 分岔落定（[v13 记录](#g2-v13-运行记录2026-10-25-会话判据2落定与采样分岔取证)）；v14（纯取证 post-pipeline1 行，构建 `…20261024T172336Z`，DLL SHA-256 `33CF02FF…`）已运行并判据②落定——63587 从未到达候选（[v14 记录](#g2-v14-运行记录2026-10-25-会话判据2落定与-63587-到达性取证)）；**v15（[契约](#2026-10-25-g2-v15-契约63587-直调替换裸更新经续审计十三--v14-判据2设计)：63587 直调替换裸 58329 + post-update1 行，构建 `…20261025T075136Z`，DLL SHA-256 `8DED2F93…`、PDB `605A0703…`）已运行并判据②落定——引擎配方首次全活而姿态仍冻结，缺口移入 63587 内部条件（[v15 记录](#g2-v15-运行记录2026-10-25-会话判据2落定与引擎配方首次全活)）。**

## G1a 运行记录（2026-10-24 会话）

构建 `5d4bee1d54`（部署身份见[构建记录](build.md#g1-experiment-implementation-2026-10-24)），目标机 AE 1.6.1170，部署 INI `IndependentGraph=construct`、`DetailedAnimationTrace=false`、`BehaviorGraphProbe=false`；原始日志 [CharacterPanel-g1a-construct-20261024-0000.log](diagnostics/CharacterPanel-g1a-construct-20261024-0000.log)。游戏会话 23:58–00:00+，首个暂停 `panel-open` 请求（request=1、actor 00000014）触发，每进程一次，共 6 条 `INDEPENDENT GRAPH` 行（24 行预算内），无崩溃/断言；面板开合、SCOPY 捕获/装配、DAC 恢复与关闭生命周期全程正常，无新增告警。

按契约观测：

- `phase=begin`：`factory=63555 loader=63585 third-arg=false holder=nullptr runtime=1.6.1170.0`。
- `phase=constructed`：`graph=0x1f2dd736200` 非空；`holder/root/behavior/project-db/setup/project/bindings/behavior-data` 全 0x0、`bones=0/0`——与二进制审计一致（构造不装载项目、不设 rootNode）；唯一的新分配子对象 `event-queue=0x1ed93261e30`，对应审计"构造写两个事件源"。
- `compare-shell graph=different holder=different root=different character=different`：候选与源无同址。
- `phase=destroyed source-drift=none`：仅以引用计数释放完成，无崩溃/断言。
- 源基线：构造前与销毁后各 11 行分组**逐字全等**（已对原始日志独立 diff，不只依赖插件自报的 `source-stable=true`），40+ 地址/计数/标志无一变化；与 2026-10-23 r4 基线同构（pose-count=116、animation-bones=116、world/controller/raycast/bindings/unscaled-skeleton/declared-root-clone/state-machine-templates 为空、ids/jobs/partitions 0/0），源坐标跨会话稳定。

判定（预注册判据，未放宽）：**G1a 通过（可观测判据）**——运行期验证工厂与配对销毁成立、无源副作用；`registry-probe=unavailable`，全局注册表计数回落保持 UNVERIFIED（契约预声明），不作为失败也不计入证据。G1 整体仍未通过：下一步是 G1b——部署 INI 改 `IndependentGraph=load` 重启，检验 63585 装载与预注册装配判据（`behaviorGraph`/`projectDBData`/`projectName`/`behaviorGraph->data` 非空、`characterInstance` 与源不同址、可写状态独立、共享资源同址、源基线不变、逆序销毁）。

## G1b 运行记录（2026-10-24 会话）

同一构建 `5d4bee1d54`，部署 INI 改为 `IndependentGraph=load` 后重启，原始日志 [CharacterPanel-g1b-load-20261024-0011.log](diagnostics/CharacterPanel-g1b-load-20261024-0011.log)。首个暂停 `panel-open` 请求（request=1、actor 00000014）触发，共 8 条 `INDEPENDENT GRAPH` 行（24 行预算内），无崩溃/断言，面板与 DAC 全程正常。

按契约观测：

- 空壳构造阶段与 G1a 完全一致：全 0 字段、`event-queue` 唯一新分配、`compare-shell` 全 different。
- `phase=load outcome=rejected ms=0.166`：以源图 `projectName` 副本（`BSFixedString*`，第三实参 false，见[实现](../src/animation/behavior_graph/independent_graph.cpp)）调用加载 63585，返回 false；候选 `behavior/project-db/behavior-data/active-nodes/variable-values` 全部仍为 0x0，候选 `projectName` 未被写入。0.166 ms 的量级只够一次立即拒绝（查表/解析失败即走错误路径），不像任何真实项目装载。
- 销毁与源基线：仅引用计数释放，无崩溃；构造前/销毁后基线逐字全等（已对原始日志独立 diff），`registry-probe=unavailable` 保持 UNVERIFIED。

判定（预注册判据，未放宽）：**G1b 失败**——命中停止条件"返回空/装配字段缺失"：63585 在早期拒绝，未完成任何原生装配。G1 整体维持 NOT PASSED，G2–G6 保持锁定；已按"停止与回退"把部署 INI 改回 off。

失败点判定（2026-10-24 续审计二已定位根因，见[审计记录](behavior-graph-construction-audit.md#第二实参形态与拒绝路径2026-10-24-续审计二)）：契约预声明的"第二实参形态未定"被实测命中——`BSFixedString*` 形态**错误**：63585 把第二实参原样作为 `const char*` 交给 69488 做路径拆分（按最后一个 `\`/`/` 分出目录与文件名），69487 拼接、69490 规范化（`/`→`\`、解析 `.`/`..`）后对全局注册表单例 0x31b6580 查表；实参误传 BSFixedString 对象地址时，其头 8 字节被当路径内容扫描，截断出的无意义"文件名"查表必失配 → 句柄 0 → 0xbb21a9 拒绝路径（对空壳图全为空操作）返回 false——与本次实测逐项吻合，纯实参形态错误、无源图副作用。另两项审计事实修正了契约认知：**`projectName(+0x1F0)` 由 63844 成功路径写为第二实参的"文件名段"（源图 projectName 不是完整路径，不能原样回传）**；编排者的第三实参来自 `holder->Unk_06()`，随 holder 而异。G1b 已按审计修订契约并重实现（`load-args` 行记录所传内容），待下次运行判读。

## G1b v2 运行记录（2026-10-24 会话）

构建 `646a0f2064+dirty…20261024T051200Z`（v2 修订形态），部署 INI `IndependentGraph=load`，原始日志 [CharacterPanel-g1bv2-load-20261024-1330.log](diagnostics/CharacterPanel-g1bv2-load-20261024-1330.log)。首个暂停 `panel-open` 请求（request=1、actor 00000014）触发，8 条 `INDEPENDENT GRAPH` 行，无崩溃，面板/DAC 全程正常。

按契约观测：

- 空壳构造与 G1a/G1b 首轮一致；`compare-shell` 全 different。
- `phase=load-args behavior-path='' project-name='DefaultFemale' path='DefaultFemale'`——**首轮的内容观测行立即命中诊断盲区**：源项目 `stringData->behaviorPath` 为**空**，`projectName` 为**无扩展名的 `DefaultFemale`**，v2 重组出的键是裸 `DefaultFemale`。
- `phase=load outcome=rejected ms=0.289`：装配字段全空，候选 `projectName` 未被写；构造前/销毁后源基线逐字全等（独立 diff 复核），无源副作用。

判定（预注册判据，未放宽）：**G1b v2 失败**（同一停止条件）。失败点判定（[续审计三](behavior-graph-construction-audit.md#加载取值与前置链的续审计三2026-10-24g1b-v2-复盘)已定位）：① 69489 剥扩展名——`projectName` 是"文件名段去扩展名"，**注册表键含目录段与扩展名**，裸 `DefaultFemale` 必然失配；② 63844 的 `character.setup → +0x40 → +0x98` 检查**不是调用方前置条件**——紧邻其前的 63949 自行为 character 创建 setup（0x58 字节分配、59477 构造、写入 +0x50），引擎自身即以 setup=0 的空壳进入 63844，因此拒绝只能发生在查表；③ 描述符表名字来自 REFR 主 vtable 槽位 114（0x2f63e0）经类型键二分查找（70510，键 `0x3284b30` 为 BSS 运行期数据）取得的组件 +0x18 串，静态无法落到具体 TESRace 字段。**结论：取值来源需要运行期证据，G1b v3 以 `race-fields` 行 dump 三个 TESRace 候选字段并按预注册优先级顺序尝试。**

## G1b v3 运行记录（2026-10-24 会话）

构建 `646a0f2064+dirty…20261024T054649Z`（DLL SHA-256 `A3C2213F…`），部署 INI `IndependentGraph=load`，原始日志 [CharacterPanel-g1bv3-load-crash-20261024-1425.log](diagnostics/CharacterPanel-g1bv3-load-crash-20261024-1425.log) 与 外部未入库原件 `C:/Users/AmazingBuff/Documents/My Games/Skyrim Special Edition/SKSE/crash-2026-10-24-14-25-34.log`（本库不提供该原件；本轮插件日志与续审计仍见本节）（会话 14:24–14:25，面板打开时进程崩溃）。`race-fields` 行全部命中：

- `sex=1 bg-model='Actors\Character\DefaultFemale.hkx' root-name='Actors\Character\Behaviors\0_Master.hkx' project-field='DefaultFemale' string-data-behavior-path='' source-project-name='DefaultFemale'`——**引擎键的来源确证**：描述符表取 `behaviorGraphs[sex].model`（完整路径含扩展名），`projectName` 即 `behaviorGraphProjectNames[sex]`（无扩展名项目名，被 63844 经 69489 剥扩展名写回）。三个字段与 [CommonLib 声明](../extern/CommonLibSSE/include/RE/T/TESRace.h)完全吻合（AE 布局对 0x1E8/0x238/0x248 偏移的读取结果语义正确）。

按契约观测（预注册判据，未放宽）：

1. **装载成功**：`phase=load-try 1/4 source=race-bg-model path='Actors\Character\DefaultFemale.hkx'` → `phase=load try=1 outcome=accepted ms=8.729`（真实装载耗时；后续候选因首个成功未触发）。
2. **装配完整（判据 1 ✓）**：`behavior=0x25df263b2c0`、`project-db=0x25df32c6200`、`project-name=0x25453c00a18`、`behavior-data=0x25db4df7510` 全部非空；候选 `projectName` 内容 = `bg-model` 路径文件名段去扩展名（`DefaultFemale`），与判据修订一致。`active-nodes=0x0`/`variable-values=0x0`——图已装载未激活（激活属 G2）。
3. **共享资源可解释（判据 3 ✓）**：`compare-loaded project-db=same project=same bindings=same behavior-data=same`——候选与源同址（引用计数 +1），与[共享语义审计](behavior-graph-construction-audit.md#加载链共享语义细读2026-10-23-三轮)预测一致；`setup=different`（63949 为候选自建）。
4. **可写状态独立（判据 2）**：候选 graph/character/setup/behavior 与源不同址；`activeNodes`/`variableValueSet` 为空（未激活），深层写隔离属 G2+ 判据。
5. **源基线（判据 4）**：崩溃前未见漂移证据；销毁后基线未能采集（崩溃），本项 UNVERIFIED。
6. **逆序销毁（判据 5 ✗ → 崩溃）**：智能指针释放 → 删除析构 0xbbcf30 → 真析构 0xbad2c0 → `behaviorGraph` 非空 → **`hkbBehaviorGraph::Deactivate`（槽位 7）读空 `activeNodes`（`[behaviorGraph+0x98]==0` → `+0x8` 计数访问越界）**。Crash Logger：`SkyrimSE.exe+0AB1FC3` AV，栈三帧（`sub_AB1FC3 ← sub_BAD324 ← dtor_BBCF4E ← independent_graph.cpp:280`）与[续审计四](behavior-graph-construction-audit.md#已装载图析构的崩溃定位2026-10-24-续审计四)静态反汇编逐帧吻合。`registry-probe=unavailable` 保持 UNVERIFIED。

判定：**G1b v3 = 装载判据成立（决定性正面证据：63585 协议在 `holder=nullptr` 空壳上完整走通），销毁判据失败**——但失败原因不是泄漏、别名或协议错误，而是引擎 `Deactivate` 不支持未激活图：**"装载后未激活直接销毁"不在引擎生命周期内**，G1 契约的销毁一环（不激活而销毁）与引擎设计冲突。G1 维持 NOT PASSED；按"停止与回退"已把部署 INI 改回 off（当前部署 DLL 为 v3 构建，`off` 下实验不触发，面板其余功能不受影响）。**方案评审结论（2026-10-24）：采纳[方案 A 修订](#2026-10-24-方案-a-修订销毁判据经-g1b-v3-崩溃后评审)，销毁判据重定义、v4 保留流程已实现并部署，待运行按修订判据完成 G1 判定。**

<a id="g1b-v4-运行记录2026-10-24-会话"></a>
## G1b v4 运行记录与 G1 判定（2026-10-24 会话）

构建 `646a0f2064+dirty…20261024T065321Z`（DLL SHA-256 `37C255DA…`），部署 INI `IndependentGraph=load`，原始日志 [CharacterPanel-g1bv4-retain-20261024-1457.log](diagnostics/CharacterPanel-g1bv4-retain-20261024-1457.log)（会话 14:56–14:57+，首个暂停 `panel-open` 请求触发）。

按 v4 预注册判据（[方案 A 修订](#2026-10-24-方案-a-修订销毁判据经-g1b-v3-崩溃后评审)，未放宽）：

1. **装载成功复现 ✓**：`load-try 1/4 source=race-bg-model path='Actors\Character\DefaultFemale.hkx'` → `outcome=accepted ms=8.797`；`behavior=0x16ff35ca500 project-db=0x16ff2fe7400 project-name=0x1662f6f0a18 behavior-data=0x16fb4c77510` 全非空；`race-fields` 行与 v3 逐字一致（`sex=1`、三字段内容相同）。
2. **装载后源基线全等 ✓**：`phase=retained source-drift=none`；已对原始日志独立 diff——构造前（基线 11 行分组）与装载后（候选存活期间采集）**逐字全等**，40+ 地址/计数/标志无一变化。
3. **保留无崩溃 ✓**：`phase=end constructed=true loaded=true assembly=true separated=true source-stable=true retained=true`；候选经永不释放的持有指针存活至进程结束；此后同一会话面板又完整开合多轮（request=3、revision=6、DAC 恢复正常），日志零错误。`registry-probe=unavailable` 保持 UNVERIFIED。

**判定：G1 PASSED（2026-10-24，方案 A 修订判据）**——构造（空壳 + 配对销毁）、装载（两次 `accepted`，装配完整、可写状态对象与源不同址、共享资源同址引用计数共享、setup 由 63949 自建）、装载后源基线全等、保留无崩溃。遗留与移交：①全局注册表计数 UNVERIFIED（预声明）；②可写状态的运行期写隔离（`variableValueSet` 等）属 G2+ 判据；③装载图的销毁证据由 G2 生命周期（激活 → 推进 → 停用 → 销毁）取得；④候选图现被保留至进程结束——独立图功能化时该保留机制将被会话生命周期管理替代。G2 解锁，其前提工作（Activate/Update 入口审计）未开始；部署 INI 已收回 `off`。

## G2 drive 运行记录（2026-10-24 会话）

构建 `646a0f2064+dirty…20261024T072214Z`（DLL SHA-256 `A3DEDE5F…`），部署 INI `IndependentGraph=drive`，原始日志 [CharacterPanel-g2-drive-20261024-1531.log](diagnostics/CharacterPanel-g2-drive-20261024-1531.log)（会话 15:25–15:31，首个暂停 `panel-open` 请求触发）。装载照旧成功（`accepted ms=9.359`，首候选），随后按 G2 契约执行。

按预注册判据（未放宽）：

1. **激活（判据① ✓）**：`phase=pre-activate` 全零（`active-nodes=0x0/0 maps=0x0,0x0 variable-values=0x0 flag=0`，`root-generator=0x27134622c70` 非空——装载期即建立）；63586 一次调用后 `phase=activated activated=true`：**`active-nodes=0x271733bfde0/97`（97 个活动节点）、maps 双非空、`variable-values=0x271731cfa00`、`flag=1`**——Activate 自分配与子图激活遍历完全生效。
2. **前进证据（判据② ✗ 未取得）**：120 步驱动完成后 `phase=post-drive` 的结构快照与驱动前**逐字相同**（active-nodes/maps/variable-values/generators 全一致），且候选 `pose-local=0x0/0` 三次快照不变——三级姿态哈希全部 `unavailable`。**没有任何可观测的推进**；按预注册规则不得默认通过。
3. **源不变（判据③ ✓）**：激活前（装载后）与驱动后源基线逐字全等（独立 diff 复核）；`activated`/`post-drive`/`retained` 三处 `source-drift=none`；**源姿态哈希前后全等**（`fdecd6ed563254e9` ×2）——暂停括号内源确实未被触碰，对照控制成立。
4. **无崩溃（判据④ ✓）**：120 步 `Update` 全部返回，同会话面板后续多轮开合正常（revision=4、DAC 恢复），日志零错误。

判定：**G2 NOT PASSED（判据②证据不足）**——激活协议完全成立，但"驱动"未产生可观测前进：候选没有姿态输出（`poseLocal` 从未分配），120 步内结构层零变化（内部时间/变量可能推进但未观测，也可能空转）。已按"停止与回退"把部署 INI 改回 off。

**归因方向（下一轮审计，两项）**：①**pose 写入链**——`character.poseLocal` 的分配/写入点在哪（源为 116 个变换；候选 `setup` 为 63949 新建的壳，可能缺 `hkaSkeleton`/蒙皮变换导致输出阶段跳过 pose 分配；generatorOutputs[0/1] 在装载期即非空，不是驱动产物）；②**Update 短路径细读**（58329）——确认 120 步是否真的推进内部状态（若推进，改进观测：`variableValueSet` 内容哈希、活动节点时间；若空转，定位缺失前置）。G1 的遗留判据"可写状态写隔离"部分成立：激活/驱动期间源结构零漂移。

<a id="g2v2-drive-运行记录2026-10-24-会话"></a>
## G2 v2 drive 运行记录（2026-10-24 会话）

构建 `646a0f2064+dirty…20261024T075048Z`（DLL SHA-256 `99E0B38E…`），INI `IndependentGraph=drive`，原始日志 [CharacterPanel-g2v2-drive-20261024-1557.log](diagnostics/CharacterPanel-g2v2-drive-20261024-1557.log)（会话 15:56–15:57+）。装载照旧成功后按 v2 流程执行：

- **判据① ✓（升级）**：字符级激活 58453 一次调用——`phase=activated activated=true`：`active-nodes=0x25af2f96b10/97`、`flag=1`，且 **`pose-local=0x25af25a6010/116`（AllocatePose 生效）**、`setup-skeleton=0x25ab6f66fd0` 非空——**装载链确实填充了 `setup.animationSkeleton`**（63949 一线成立），pose 缓冲与源同构（116 变换）。
- **判据③ ✓**：`source-drift=none` ×3、两份源基线逐字全等、**源姿态哈希前后全等**（`9c39265b…`×2）。
- **判据④ ✓**：120 步无崩溃，同会话面板多轮正常（revision=10），零错误。
- **判据② ✗ 仍未取得**：候选三级姿态哈希**全等**（`bc160a8c…`×3）——pose 缓冲内容停留在 AllocatePose 填充的骨架绑姿态上，120 步零变化；`children-flag=0`（+0x12E 门未置位）。

**Update 尾部补审**：0xab1ecc..0xab1f27 只有暂存缓冲簿记（57041）与返回——**58329 全函数确认无任何 `poseLocal` 写入**；"生成 → 采样进 pose 缓冲"的采样步骤在引擎其他环节（63587 后段/管理器流），未定位。

**判定：G2 仍 NOT PASSED（判据②）**；INI 已回退 off。

**判别器 v1 结果（gen-hash 行读数失败）**：`numBytes=-898388560` 为垃圾——`generatorOutputs` 指向对象的头部**不是** CommonLib 的 TrackMasterHeader（activeragdoll 移植未按 AE 校验），哈希窗口假设作废。同轮完成 Update 的 context 读取面枚举：**58329 全函数只读 `ctx.eventQueue(+0x18)` 与图字段（activeNodes/+0x98 ×6、+0x12E 门 ×2）**——我们自建的 context 已完备；虚表槽 6（CommonLib `Unk_06`）经 58377 证实为"向活动节点派发事件"类函数，58429 只是单条 float 写入，均非输出请求。**判别器 v2 运行结果（决定性）**：`phase=probe` 三行——**activeNodes 条目数组哈希每步变化**（`33a48b2a…`→`7177d7b3…`→`34ca7983…`）：**Update 确实在逐步推进 97 个节点的运行状态，生成链活着**；变量集 word 数组变一次后冻结（652 词——idle 循环未必每帧写变量，不作数）；gen0/gen1 转储揭示它们是 `{ptr, size=1, cap}` 的一元素数组头（非 TrackMasterHeader）。姿态哈希仍三级全等。**采样器定位（[续审计六补遗](behavior-graph-construction-audit.md#姿态写入链与字符级激活2026-10-24-续审计六)）**：**63581（0xbafda0）= 采样器**——读 `generatorOutputs[0]` 一元素数组的元素、按元素内 16 位偏移取 pose 轨道、以 57148 按 `count×0x30` 拷入 GetOrAllocatePose 返回的 poseLocal（与 AllocatePose 同一填充助手）；其入口 **63589（0xbb2a40）** 的四道门（setup/animationSkeleton/behaviorGraph 非空、+0x12C 已激活）候选全部满足，但 63581 开头 `test r15w` ——**当前动画索引 `[graph+0x240]`（word）为 0 即跳过采样**；该索引由 63587 每帧从管理器更新记录写入，候选无管理器故恒 0。**G2 v4 运行结果（判别器命中索引语义）**：`src-anim=0074`——**0x0074 = 116 = 骨骼变换数**：`[graph+0x240]` 不是动画索引而是**采样变换数**（63587 写它时以 `setup.animationSkeleton` 的骨数钳制，完全吻合）；镜像正确生效（`cand-anim=0074`），63589 每步调用无崩溃，源全程不变。**但 gen 元素内容哈希 120 步恒定**（`68fb5223…`×3）——**生成阶段没有向输出元素写任何东西**，姿态仍停在骨架绑姿态。**判定：G2 仍 NOT PASSED（判据②）；INI 已回退 off**（原始日志 [CharacterPanel-g2v4-drive-20261024-1635.log](diagnostics/CharacterPanel-g2v4-drive-20261024-1635.log)）。**管理器每帧管线审计（续审计六补遗二）——顶石找到**：通知中心单例 0x2022570（vtable 0x1986740；63761 = 把图**订阅**进中心列表，上限 30，0x54df30 为元素释放回调）的**槽 5（0xbacc90）= 每帧图处理**：栈上构临时图（63555）→ **63602(临时图, 订阅图)** → 析构（63556）。**63602 尾部即每帧图管线**：65149(&characterInstance, out) 建帧记录 → **`behaviorGraph->+0x12E = 1`（每帧请求子图重激活！）** → 63587(graph, record) → 清 +0x243。**首轮~v4 的生成无产物根因 = 我们的 Update 前从未置 +0x12E，Update 内的 60079 子图激活遍历每帧被门跳过**（该门由 63132/63602 置位、60079 消费清零）。**G2 v5 运行结果**：`+0x12E=1` 生效（门通过），节点状态仍每步搅动，**但 gen 元素与姿态仍恒定**——子图激活请求不是最后一块。**G2 v6 运行结果：崩溃**（打开背包即崩，[Crash Logger 报告](diagnostics/CharacterPanel-g2v6-crash-report-20261024-1711.log)）。归因逐帧闭合：`63587` 的 post-Update 阶段 **63583** 在 BSFixedString::Set（69162）写 `[r9=0x20]` 崩溃——63583 把动画名注册进一张由 `projectDBData->+0xD0` 派生、按动画索引索引的全局表，**该阶段依赖管理器侧的动画选择上下文，候选不具备**（63602 的记录里相应字段在引擎路径下由管理器状态填充）。**结论：63587 包装器路径拖进了无法在无管理器候选上复刻的名字注册阶段—— 包装器路线到此为止，驱动回退到 v5 形式（裸 58329 + 变换数镜像 + 63589，无崩溃）**。构建 `…20261024T091522Z`（DLL SHA-256 `3733ACFE…`）已部署，INI 已收 off。**战略结论**：v1–v6 六轮迭代把驱动链的每一段都指令级测绘完毕（激活/分配/Update/采样/入口/记录/包装器），剩余缺口收敛为单点：**生成阶段的动画选择机制由管理器驱动（记录携带动画身份、63583 注册名字），无管理器候选无法复刻**——这属于功能设计范围（面板预览本来就要自己选择动画），不再属于"可行性"问题。G2 的"推进"判据需要按此重新定义或与功能设计合并推进。

## G2 v7 运行记录（2026-10-24 会话）

构建 `440e7cf784+dirty…20261024T130326Z`（DLL SHA-256 `12663FFE…`），INI `IndependentGraph=drive`，原始日志 [CharacterPanel-g2v7-pipeline-20261024-2108.log](diagnostics/CharacterPanel-g2v7-pipeline-20261024-2108.log)（会话 21:07–21:09，首个暂停 `panel-open` 请求触发）。装载照旧成功（`accepted ms=8.422`，首候选），字符级激活照旧（97 节点、`pose-local=…/116`、setup 三字段非空），随后按 [G2 v7 契约](#2026-10-24-g2-v7-契约引擎管线驱动经续审计七设计)每步执行 `63555 栈壳 → 63602(&壳, 候选) → 63556` 引擎管线 + 实 dt 58329 + `+0x240` 镜像 + 63589 采样，共 120 步。

按预注册判据（未放宽）：

1. **前进证据（判据① ✗）**：候选三级姿态哈希仍全等（`bc160a8c…`×3，骨架绑姿态），**且 gen0/gen1 元素哈希 120 步恒定**（`6cc73397…`×3）——生成遍历仍未向输出元素写入任何内容。`cand-anim=0000→0074`、`active` 节点哈希每步变化（`3f40…`→`e7a7…`→`f6bb…`）、vars 变一次后冻结（652 词）均与既往轮次一致。按预注册归因表走"gen 元素恒定"分支。
2. **源不变（判据② ✓）**：源姿态哈希前后全等（`63a591dc…`×2）、`source-drift=none` ×3。
3. **无崩溃（判据③ ✓）**：120 步 + 120 对栈壳构造/析构全部返回，日志零错误/断言，无新 crash 报告；同会话面板后续多轮开合正常（request=2、revision=4、DAC restore=matched）。

判定：**G2 v7 NOT PASSED（判据②，gen 元素恒定分支）**；INI 已回退 off。**新增正面事实**：引擎管线驱动配方（63602 ×120 + 栈壳对）被证实可在候选上安全调用——v6 时代的"包装器必崩"只属于手搓记录直调 63587 的路线。**被否假设**："默认态含 idle 生成器、生成即出姿态"不成立——DefaultFemale/0_Master 图在无事件注入时默认态不产出位姿。**战略收束**：位姿生成的最后缺口 = **事件/初始化序列**（哪个事件把状态机带进 idle）——与 G3（Equip→Idle→Unequip 的事件机制）合流，候选事件队列布局（24 B 表项、游标 `+0xC/+0x14`，[续审计七](behavior-graph-construction-audit.md#管理器每帧管线与记录全解2026-10-24-续审计七)）与源图事件流（WORLD/注解观测）是 G3 设计的现成输入。判别遗留（预注册到 G3）：本轮观测无法区分"65237 遍历了但无激活生成器"与"63602 尾部解析未达"——注入已知事件后 gen 元素若变化则生成链活、若不变则先修管线到达性。

## G2 v8 运行记录（2026-10-24 会话，崩溃与修复）

构建 `440e7cf784+dirty…20261024T133638Z`，INI `IndependentGraph=inject`，原始日志 [CharacterPanel-g2v8-inject-crash-20261024-2145.log](diagnostics/CharacterPanel-g2v8-inject-crash-20261024-2145.log) 与 [Crash Logger 报告](diagnostics/CharacterPanel-g2v8-crash-report-20261024-2145.log)（会话 21:45:01，开包即崩）。装载/激活照旧成功（`accepted ms=9.653`、97 节点、`pose-local=…/116`），日志止于 `phase=probe step=activate`——**崩溃发生在首个 event-lookup，注入（58377）从未执行**。

归因（逐帧闭合）：崩溃指令 = ucrtbase `strcmp` 内 `movzx eax,[rcx]`，`rcx=0x4273656C6262694E`（ASCII `"NibbleB"`——字符串字节被当指针）；栈帧 [1] = 插件 `lookup_event_id`（参数 `map`/`name`）、帧 [2] = `run_independent_graph_experiment` 调用点。**根因 = 我方只读遍历的停机条件错误**：桶链尾指向**哨兵**（存于 `map+0x18`，引擎 63639/63773 两处遍历均以 `next == [map+0x18]` 停机，[续审计八](behavior-graph-construction-audit.md#事件派发链与槽-2-输出门2026-10-24-续审计八)已记录该字段），而实现误用 `entry != 0`——空桶即指向哨兵，首个 strcmp 就把哨兵内容当 `char*` 解引用。**非引擎 ABI 问题、非注入风险**：58377 调用面未被触及，v8 契约判据不受影响。

修复一（哨兵停机）后重跑**再次崩溃于同一 strcmp**（[v8b 日志](diagnostics/CharacterPanel-g2v8b-inject-crash-20261024-2154.log) 与 [Crash Logger 报告](diagnostics/CharacterPanel-g2v8b-crash-report-20261024-2154.log)，`rcx=0x6F47646542434C4D` = ASCII `"MLCBedoG"`，同为名字字节）：哨兵停机没错，**但 `db+0xA0` 那张表用 unk70（63639 验证）推导的桶布局根本走不通**——越界后踩进的是名字指针数组。结论：+0xA0 表的内部布局/填充状态与 unk70 不同源假设不成立，盲走结构路线作废。

修复二（路线更换，构建 `…20261024T135805Z`，DLL SHA-256 `5764BCB5…`）：①新增 `phase=dbdump` 行——把 `db+0x60..0xE8` 的 17 个 qword 原始 dump；②名字→id 解析改用 unkD0 名字数组的线性扫描（63583 注册路径布局）。预注册名字表与注入（58377）、判据、归因表沿用原文。

**v8c 运行（同构建，2026-10-24 22:04，[日志](diagnostics/CharacterPanel-g2v8c-inject-skip-20261024-2204.log)）**：无崩溃、源不变、120 步照常——但四个名字全部 `id=-1` → `skip=no-known-event`（预注册干净分支，注入未执行）。**dbdump 解码推翻"布局证伪"结论**：两张哈希表的 CLib 布局完全正确（哨兵 `db+0x88/db+0xB8` 同为模块静态地址、计数 1024/16384 均 2 的幂、桶为堆指针）；**两轮崩溃的真正根因 = 桶链含墓碑/释放态表项，键字段为垃圾字节，而引擎查表只做指针比较、从不解引用键**——按内容 strcmp 的遍历在任何一张表上都不安全。unkD0（9379 项，扫描全程安全）是动画/剪辑名表，不含那四个事件名。

**修复三（v8d，构建 `…20261024T141115Z`，DLL SHA-256 `D86BAA87…`）**：事件→id 的正源改为**行为图自带字符串数据**——`hkbBehaviorGraphData(+0x78 stringData)->eventNames(+0x10)` 是 Havok 原生 `hkArray<hkStringPtr>` 纯指针数组（低位 managed 标志掩 `&~1`），**事件 id 即数组下标**，正是 `eventInfos` 与状态机事件转移映射键控的 id 空间（58377 查的就是它）。新增 `phase=events count=N first='…'` 行（前 16 个真实事件名作证据）；预注册名字表、注入、判据、归因表沿用原文。

**v8d 运行（2026-10-24 22:18，[日志](diagnostics/CharacterPanel-g2v8d-inject-skip-20261024-2218.log)）**：无崩溃、源不变；**事件表首次见光**——`count=8223`，前 16 名：`FootLeft`/`FootRight`/`WeapEquip`/`na_MRh_BeginCast_Event`/`MRh_Cast_State_to_MRh_Readied_State`/`MRh_Standing_Cast_State_to_MRh_Moving_Cast_State`/`MRh_SpellReady_Event`/`MRh_SpellRelease_Event`/`MRh_Equipped_Event`/`MRh_SpellFire_Event`/`MLh_SpellReady_Event`/`MLh_SpellRelease_Event`/`SprintStart`/`SprintStop`/`na_MLH_BeginCast_Event`/`1HM_Unequip`。**判定**：四个预注册名字（`MT_Idle`/`MoveStart`/`WeaponDraw`/`SyncForward`）确非图事件——它们是 Skyrim 层动画事件/剪辑名；图内事件是另一套命名（locomotion 由变量驱动，装备链入口为 `WeapEquip`）。四名字 miss → 预注册干净跳过（58377 仍未执行）。

**v8e 修订（构建 `…20261024T141951Z`，DLL SHA-256 `17364747…`）**：预注册名字表按 v8d 证据重定——`WeapEquip`（装备链入口，正对 G3 首批"单手武器 Equip→Idle→Unequip"）→ `SprintStart`（运动态入口）→ `FootLeft` → `FootRight`（同步点）。注入、判据、归因表沿用原文。

**v8e 运行（2026-10-24 22:28，[日志](diagnostics/CharacterPanel-g2v8e-inject-fired-20261024-2228.log)）——里程碑**：四名字全部命中（`FootLeft=0`、`FootRight=1`、`WeapEquip=2`、`SprintStart=12`），**58377 注入首次执行且无崩溃**，`phase=inject event='WeapEquip' id=2 result=dispatched`。**状态机真实转移**：活动节点 **97 → 92**、变量集哈希变化（`893b33fd…` → `bb1ebb71…`）、转移后图进入稳态（60/120 步 active/vars 哈希相同）——候选图对事件完全响应，引擎侧事件→转移链是活的。但 gen 元素与姿态哈希仍恒定（`2c2893e8…`×3、`bc160a8c…`×3）→ 按预注册判据落**分支②（槽 2 输出绑定门）**；源不变（`02483049…`×2、source-stable=true）、无崩溃 ✓。

**v9 修订（构建 `…20261024T143116Z`，DLL SHA-256 `898C46F0…`）**：分支②存在反证风险——活体 Actor 的 ragdollDriver 很可能同为空（死亡才启用），若源也空却仍出姿态则槽 2 门假设证伪。新增只读 `phase=gates` 行：源/候选的 `physicsWorld(+0x238)`、`ragdollDriver(+0x30)` 与双方 gen0 元素头部 16 字节 + `+0x32/+0x38` 字段对比。**预注册裁决规则**：源 ragdoll-driver=0 → 槽 2 门假设证伪，下一归因 = 生成遍历到达性/63589 第二实参语义复审；源 ragdoll-driver≠0 → v10 设计受控借用（world+driver）评审。注入与判据不变。

**v9 运行（2026-10-24 22:35，[日志](diagnostics/CharacterPanel-g2v9-gates-20261024-2235.log)）——槽 2 假设坐实**：gates 行一次裁决——`src-world≠0`、`src-ragdoll≠0`（活体源双门全开；"死亡才启用 ragdoll"假设错误），`cand-world=0x0`、**`cand-ragdoll≠0`（装载链已为候选自建驱动）——候选只缺 physicsWorld 一门**。**元素绑定铁证**：双方 gen0 元素头 16 字节逐字节相同，但 `src +32=0x0074（=116 变换数）+38=1.000` vs `cand +32=0x0000 +38=0.000`——源元素已绑定、候选未绑定，正是槽 2 门内写的字段。注入/转移照旧复现（97→92），gen 元素与姿态仍恒定，源不变、无崩溃。

**v10 契约（借用源 world，构建 `…20261024T144235Z`，DLL SHA-256 `789C5026…`）**：借用前审计闭合——槽 2 对共享 world 的全部接触 = `bhkWorld vtable 槽 40`（77853，`mov rax,[rcx+0x10]; ret` 纯读 getter）与守卫断言桩 77928（release 直返），其余 body 在候选自己的驱动/控制器对象上（58286 锁内）。实现：drive 循环前 `candidate->physicsWorld = source->physicsWorld`（裸指针借用、不动引用计数），循环后立即恢复 nullptr，`phase=borrow` 行记录（预算 +1）。**判据①修订（预注册）**：前进证据以**候选姿态三级哈希不全等**为准；gen 元素哈希变化单独记录为**绑定门打开的证据**（槽 2 重置会写 +0x32/+0x38，元素哈希必变），**不单独构成通过**。归因表追加：绑定字段被写但姿态仍冻结 → 生成遍历仍未产出（下一归因 = 65237 到达性插桩）；绑定字段未写 → 借用未生效（回 63602 管线审计）。②③④沿用。

**v10 运行（2026-10-24 22:45，[日志](diagnostics/CharacterPanel-g2v10-borrow-20261024-2245.log)）——借用安全但无效，归因精确化**：借用执行/恢复干净（`phase=borrow world=…`、`world-restored=true`）、无崩溃、源不变、注入转移照旧复现（97→92）——但**元素绑定字段仍未被写**（gen0elem 哈希 `d82e8a27…`×3 恒定；槽 2 若运行必写 `+0x38=1.0` 必变哈希）→ 按预注册归因落"借用未生效"分支。**静态追因**：63602 内对图槽 2 的调用位于一个循环体内，循环计数由 ARG2 的 vtable 槽 0x58 以出参填充——候选侧计数为 0，循环零次执行。**结论（修正 v6 战略结论的精确回归）**：绑定循环遍历的是"当前动画/通道"类列表，由管理器侧每帧维护；无管理器候选该列表恒空，world 借用无法解锁。遗留审计疑点：主 vtable（0x19881f8，槽 0=删除析构 0xbbcf30 已确证）的槽 0E/0x11 指向函数内部地址，槽位标注存在歧义；ARG2 槽 0x58 提供者的确切语义与列表填充方未定位。INI 已回退 off。

<a id="g2-v11-运行记录2026-10-25-会话判据2落定与审计十推论证伪"></a>
## G2 v11 运行记录（2026-10-25 会话，判据②落定与审计十推论证伪）

构建 `…a32766a080+dirty…20261024T160418Z`（DLL SHA-256 `E57C19A8…`），部署 INI `inject`，2026-10-25 00:14–00:15 会话，request=1（actor 00000014），共 28 行（30 行预算内），无崩溃/断言，面板生命周期正常，[原始日志](diagnostics/CharacterPanel-g2v11-borrow-20261025-0015.log)。

**driver 行（修正读链首次取证）——审计十静态推论被运行期证伪**：`src[char=0x1f9aaf58ec0 ragdoll=0x1f513a4fc80 bodies=… count=18]`、**`cand[char=0x1fa6358a2c0（= 候选图基址+0xC0 的内嵌 characterInstance，回指精确）ragdoll=0x1f5140a8120 bodies=… count=18]——候选本就持有 18 体 ragdoll 实例**（+0x88 非 null），装载子链某处（63585 下游，待审计十一定位）已为候选填充；"装载链止步 63949 → +0x88 恒 null → 计数 0"不成立，v10"绑定循环零次执行"归因随之失效——**循环计数从来是 18**。另源 rigidBodies=18 而非 116：0x74=numPoseLocal 动画骨数，[续审计九](behavior-graph-construction-audit.md#绑定循环的骨骼映射语义与-vtable-澄清2026-10-24-续审计九)"116=逐骨绑定轨道数=rigidBodies.size"的推断亦错。

**借用与驱动**：`phase=borrow world=0x1f5259940c0 ragdoll=borrowed saved=0x1f5140a8120`（以源实例覆盖候选自有实例，循环后恢复原值）、`phase=pipeline … ragdoll-restored=true`；注入转移照旧复现（active 97→92、vars 哈希 893b33…→bb1ebb…）。

**判读（预注册判据②落定）**：候选姿态三级哈希全等（`bc160a8c2b3a5a58`×3）、**候选 gen0elem 哈希 `e122290bf430e867`×3 恒定**（绑定字段 +32/+38 仍未被写；probe 的 gen0elem 已核实读候选侧）；源姿态前后全等、`source-drift=none`、`source-stable=true`；无崩溃。→ **ragdoll 实例借用不改变输出，G2/G3 维持未过**；归因按预注册转**循环内逐骨元素配对/生成遍历到达性**：计数 18 非零、双门已过而候选元素字段零写入。下一工作包（审计十一）= **定位源元素 `+32=0x74/+38=1.0` 的写入者**（对 element+0x32/+0x38 做 store 扫描），其前置条件（绑定期 vs 每帧、轨道↔刚体映射来源，嫌疑：hkbCharacter.animationBindingSet(+0x68) / hkaRagdollInstance.boneToRigidBodyMap(+0x30)）即真正缺口。INI 已回退 off。

<a id="g2-v12-运行记录2026-10-25-会话判据2落定与写入者运行期证实"></a>
## G2 v12 运行记录（2026-10-25 会话，判据②落定与写入者运行期证实）

构建 `…a32766a080+dirty…20261024T165857Z`（DLL SHA-256 `EA5B3B27…`），部署 INI `inject`，2026-10-25 01:05–01:06 会话，request=1（actor 00000014），共 29 行（30 行预算内），无崩溃/断言，面板正常，[原始日志](diagnostics/CharacterPanel-g2v12-slot2-20261025-0106.log)。

**槽 2 直调首次执行并证实写入者身份**：`phase=slot2 added=true tracks-before[+32=0x0000 +38=0.000] tracks-after[+32=0x0000 +38=1.000]`——63621 在候选上过全部门、头部把 kPose 轨道 onFraction 写到 1.0（+32 保持 0，63621 本就写 0）；驱动后 `phase=pipeline … ragdoll-removed=true tracks-end[+32=0x0000 +38=1.000]`——槽 3 对称移除成功。注入转移照旧复现、源姿态前后全等（`e824ede5…`×2）、无崩溃。

**判读（预注册判据②落定）**：候选姿态三级哈希全等（`bc160a8c2b3a5a58`×3）、gen 元素恒定——**槽 2 可执行但非最后缺口**；残余缺口 = **numData=0x74 的回填路径**。判读后随即的[续审计十二](behavior-graph-construction-audit.md#采样入口五道门与-numdata-写入者2026-10-25-续审计十二)定位：回填者 = 63568（`[tracks+0x32]=word [graph+0x240]`），深藏于每步采样入口 63589 的首道门 `[graph+0x110]` 之后——候选从未真正采样。INI 判读后按惯例回退 off，后为 v13 部署再置 inject。

<a id="g2-v13-运行记录2026-10-25-会话判据2落定与采样分岔取证"></a>
## G2 v13 运行记录（2026-10-25 会话，判据②落定与采样分岔取证）

构建 `…a32766a080+dirty…20261024T171309Z`（DLL SHA-256 `216A003E…`），部署 INI `inject`，2026-10-25 01:17–01:18 会话，request=1（actor 00000014），共 30 行（32 行预算内），无崩溃/断言，面板正常，[原始日志](diagnostics/CharacterPanel-g2v13-slot0c-20261025-0118.log)。

**slot0c 行证伪审计十二的"首门拦截"推断**：`before[110=0x192131b0ea0 sel+20=0x191d70949d0 246=0] after[…同值…]`——**候选的 `[graph+0x110]` 在直调前就非空**（装载子链已填，槽 0xC 直调为无操作），63589 的前四道门其实一直通过。真正的分岔在第五门：**候选 `[graph+0x246]=0` → 每步采样走 63581 分支，从不经过 63568（numData 写入者）**。槽 2 行照旧（onFraction 0→1.0），驱动后 `tracks-end[+32=0x0000 +38=1.000]`、候选姿态三级哈希仍全等（`bc160a8c2b3a5a58`×3）、源不变、无崩溃。

**判读（预注册判据②落定，变体）**：+0x110 早已填充而 +32 仍 0 → 转采样内部分支；[续审计十三](behavior-graph-construction-audit.md#63587-全配方与-0x246-分岔2026-10-25-续审计十三)随即定位 0x245/0x246/0x240 的每帧写入者 = **63587（引擎级更新包装器，完整每帧配方：ctx→preupdate→更新→事件排空→位姿应用→簿记与双缓冲交换）**——插件每步调的裸 58329 绕过了它全部的簿记；且 63602 尾部本就对目标图调 63587，候选是否收到未知。INI 判读后回退 off，后为 v14 部署再置 inject。

### 2026-10-25 G2 v14 契约（纯取证：管线尾部 63587 到达性，经续审计十三设计）

> **历史预注册标记（2026-10-26）**：本节保留当时配方、预期和判据，不将后验结果改写为事前预测；后续反例及当前证据边界见[当前结论](#current-status)与 [v17 后验勘误](#g2-v17-运行记录2026-10-26-会话谓词翻假达成与采样门23落定)。

- **设置**：不变（`IndependentGraph=inject`）。流程 = v13 全部，唯一新增 = 驱动循环第 0 步 `frame_pipeline` 之后**只读记录一行** `phase=post-pipeline1`：`num-anim(0x240)/w245/b246/b249` 与双缓冲两槽 `out0[+0x220]/out1[+0x228]` 的轨道字段。零行为变更。观测预算 33 行（净 +1）。
- **预注册判据（不得事后放宽）**：
  - ①该行 `w245=0x0101` 或 `b246=1`（或 `num-anim` 非零/两槽字段互换）→ **63602 尾部的 63587 确在候选上运行**——缺口在 63587 内部条件（Path A/B 选择、间接更新调用），v15 按内部条件推进；
  - ②该行 `w245=0x0000 b246=0` 且两槽无变化 → 63587 从未在候选上运行（r13 非候选或前置门失败）——v15 以 63602 尾部同款栈记录（dt、footIK=1、+0x2a..+0x2e 模式）直调 63587 替换裸 58329；
  - ③无崩溃/断言，源不变（既有行覆盖）。
- **停止与回退**：同 v13（INI 回 off）。

<a id="g2-v14-运行记录2026-10-25-会话判据2落定与-63587-到达性取证"></a>
## G2 v14 运行记录（2026-10-25 会话，判据②落定与 63587 到达性取证）

构建 `…a32766a080+dirty-cl94faaed0c6-20261024T172336Z`（DLL SHA-256 `33CF02FF…`），部署 INI `inject`，2026-10-25 13:36 会话，request=1（actor 00000014），共 31 行（33 行预算内），无崩溃/断言，源全程稳定，[原始日志](diagnostics/CharacterPanel-g2v14-pipeline1-20261025-1336.log)。

**决定性行（`phase=post-pipeline1`，驱动循环第 0 步管线后）**：`num-anim=0x0 w245=0x0 b246=0 b249=0 out0[+32=0x0000 +38=1.000] out1[+32=0x0000 +38=0.000]`。

**判读（预注册判据②落定）**：全部 63587 簿记为零（0x240/0x245/0x246/0x249 无一被写，两槽字段未互换——out0 的 `+38=1.000` 只是本轮 slot 2 直调的既有写入、out1 仍 0.000），且双缓冲两槽只显示插件自己的槽 2 写入 → **63602 尾部的 63587 从未在候选上运行**（`[r13+0x208]` 非空门失败或其它前置不满足；观测到的 246=0 归因"构造初值"分支）。

支撑事实：装载照旧一次成功（`accepted ms=7.143`，首候选）、字符级激活照旧（97 节点、`pose-local=…/116`）、注入转移照旧复现（`inject event='WeapEquip' id=2 result=dispatched`，active 97→92、vars 哈希 `893b33…`→`bb1ebb…`）、驱动后 `tracks-end[+32=0x0000 +38=1.000]`、候选姿态三级哈希仍全等（`bc160a8c2b3a5a58`×3）、源姿态前后全等（`67117174fdb8e65d`×2）、`source-drift=none`、`source-stable=true`。`cand-anim=0074`（第 60/120 步 probe 行）是**插件自己的每步 `[graph+0x240]` 镜像写**（自 v7 起存在——[v8e](diagnostics/CharacterPanel-g2v8e-inject-fired-20261024-2228.log)/[v10](diagnostics/CharacterPanel-g2v10-borrow-20261024-2245.log) 归档日志中同一 `cand-anim=0074` 可证），不是引擎写入，不构成 63587 簿记证据。INI 判读后按惯例回退 `off`，后为 v15 部署再置 `inject`。

<a id="g2-v15-运行记录2026-10-25-会话判据2落定与引擎配方首次全活"></a>
## G2 v15 运行记录（2026-10-25 会话，判据②落定与引擎配方首次全活）

构建 `…a32766a080+dirty-cl94faaed0c6-20261025T075136Z`（DLL SHA-256 `8DED2F93…`、PDB `605A0703…`），部署 INI `inject`，2026-10-25 19:04 会话，request=1（actor 00000014），共 32 行（34 行预算内），无崩溃/断言，源全程稳定（`source-stable=true`），面板生命周期正常，[原始日志](diagnostics/CharacterPanel-g2v15-wrapper-20261025-1904.log)。

**决定性两行（逐字）**：

- `phase=post-pipeline1 num-anim=0x0 w245=0x0 b246=0 b249=0 out0[+32=0x0000 +38=1.000] out1[+32=0x0000 +38=0.000]`
- `phase=post-update1 num-anim=0x0 w245=0x101 b246=1 b249=1 out0[+32=0x0000 +38=0.000] out1[+32=0x0000 +38=0.000]`

判读：**63587 首次在候选上运行**——序言簿记 `w245=0x101、b249=1` 首次出现；第 0 步走 Path A（num-anim 未被重算；slot 2 直调写入的 +38=1.000 被双缓冲交换与轨道重置消费；post-pipeline1 重现 v14 的全零 → 63602 尾部仍无产出）。

**首次全活事实（相对 v7–v14 均为新）**：

- 活动节点计数随驱动演化：97（激活）→ 125（第 60 步）→ 111（第 120 步），且 `activeNodes` 数组本体被引擎重新分配（`0x1af9385e440` → `0x1af9385e490`）——**新**；
- 变量集哈希逐检查点演化：`893b33fd…`（激活）→ `214e2111…`（60 步）→ `a4bb41b6…`（120 步）——**新**；
- `gen0elem` 哈希驱动期间首次运动：`09335c48760eef8e` → `33583b6274311017`（60 步起稳定至 120 步）——**新**；
- 驱动后 `tracks-end[+32=0x0074 +38=1.000]`——v12–v14 追的 numData=0x74 回填达成——**新**。

**判定（预注册判据②落定，未放宽）**：簿记在场而候选姿态三级哈希仍全等（`bc160a8c2b3a5a58`×3，绑姿）→ 按预注册，缺口移入 **63587 内部条件**（Path A/B 选择、间接更新调用）。锐化后的问题：kPose 轨道头已完全绑定（numData=0x74、onFraction=1.0）且每步元素在运动，还有什么阻止轨道变换数据进入 poseLocal。支撑事实：注入转移照旧复现（`inject event='WeapEquip' id=2 result=dispatched`）、源姿态前后全等（`70b2d213715f49f9`×2）、`source-drift=none`。INI 判读后按惯例回退 `off`（部署的 v15 构建保留在位）。

<a id="2026-10-25-g2-v15-契约63587-直调替换裸更新经续审计十三--v14-判据2设计"></a>
### 2026-10-25 G2 v15 契约（63587 直调替换裸更新，经续审计十三 + v14 判据②设计）

> **历史预注册标记（2026-10-26）**：本节保留当时配方、预期和判据，不将后验结果改写为事前预测；后续反例及当前证据边界见[当前结论](#current-status)与 [v17 后验勘误](#g2-v17-运行记录2026-10-26-会话谓词翻假达成与采样门23落定)。

- **设置**：不变（`IndependentGraph=inject`）。流程 = v14 全部，唯一变更 = 驱动循环每步把裸 58329 更新替换为**一次直调 `63587(candidate, &record, 0)`**：record 为零初始化的 ≥0x30 字节栈结构（第三实参 0），`float dt@+0x00 = 1/60`，字节 `+0x2a=1, +0x2b=1, +0x2c=1, +0x2d=0, +0x2e=1`（footIK=1）——[续审计十三](behavior-graph-construction-audit.md#63587-全配方与-0x246-分岔2026-10-25-续审计十三) 63602 尾部（0xbb63b8）的栈记录布局；record 每步在循环内重建（引擎每次调用即栈上宿主一份）。63602 管线遍历、`[behavior+0x12E]=1` 写、`[graph+0x240]` 镜像与采样入口 63589 照旧。
- **观测**：新增只读 `phase=post-update1` 行——第 0 步首次直调之后立即记录，字段集与 `post-pipeline1` 相同（`num-anim(0x240)/w245/b246/b249` + 双缓冲两槽 `out0[+0x220]/out1[+0x228]` 轨道字段）。观测预算 34 行（净 +1）。
- **预注册判据（不得事后放宽）**：
  - ①候选姿态三级哈希不全等（源稳定、无崩溃）→ **G2 运动证据成立**（直调配方即缺口）；
  - ②`phase=post-update1` 出现任何 63587 簿记（`w245≠0` / `b246=1` / `num-anim` 被重算 / 双缓冲两槽相对 post-pipeline1 互换）而输出仍冻结 → **缺口移入 63587 内部条件**（Path A/B 选择、间接更新调用）→ 下一工作包 = 审计十四；
  - ③`phase=post-update1` 仍 `w245=0x0000 b246=0` → **63587 自身序言门（+0x110 / [[+0x110]+0x20] / +0x208 / [[+0x208]+0x12c]）在候选上拦截** → 下一工作包 = +0x208 门链审计；
  - ④无崩溃/断言，源不变（既有行覆盖）。
- **停止与回退**：同 v14（INI 回 off）。

### 2026-10-25 G2 v16 契约（四门仪器化：63564 谓词直调 + 58423 门变量 + 双槽序列，经续审计十四收束设计）

> **历史预注册标记（2026-10-26）**：本节保留当时配方、预期和判据，不将后验结果改写为事前预测；后续反例及当前证据边界见[当前结论](#current-status)与 [v17 后验勘误](#g2-v17-运行记录2026-10-26-会话谓词翻假达成与采样门23落定)。

- **设置**：不变（`IndependentGraph=inject`）。流程 = v15 全部（每步 63602 管线 → `+0x12E` 写 → 直调 63587 → `[0x240]` 镜像 → 采样入口 63589），**只增观测、零行为变更**：三处新增全部为只读读取，预算内既有行保持原序原字段。
- **观测**（三处新增，[续审计十四](behavior-graph-construction-audit.md#63587-内部条件与位姿最后一公里2026-10-25-续审计十四)收束清单的直接仪器化；观测预算 40 行，较 v15 预算 34 行净 +6）：
  1. **`phase=pred step=N r=<hex>`（步骤 0/60）**：直调一次 **63564**（0xbae3d0，重定位 `PosePredicateID`）——63581 采样块第一条指令就是它（`audit14_branch_63581.txt` 0xbafdda，实参沿用 63581 收到的 rcx/rdx，谓词体内 rdx 可证死参数），返回值在 0xbaff4d `test r13b,r13b` 决定是否整块跳过采样。约定 = `uint8_t(*)(graph const*)`（单活实参 = 候选图）；副作用有界已静态核验：全 0x61 字节体只读 `[graph+0x238]`/`[graph+0xF0]`，唯一被调 58286 = `mov rax,[rcx+0x88]; ret` 纯 getter，无任何写入/引用簿记。调用点 = 每步采样入口 63589 之前（引擎内 63581 的调用时机）。
  2. **`phase=g3-gates`（驱动循环前一次，只读）**：58423（0xab7f30）入口三道门变量——`[graph+0x100]` 对象指针及其 `+0x4A` 门字节、ctx 链 `[ctx+0x50] → [+0x40] → [+0x88]`（0xab7f54..0xab7f89；ctx = 插件 58315 自建的既有 context，与 58377 派发共用），指针空记 `null`。
  3. **`phase=slots step=N p0=<ptr> n0/f0 h0 p1 n1/f1 h1`（步骤 0/1/2/60/120）**：双缓冲两槽 `[graph+0x220]`/`[graph+0x228]` 的逐槽序列——元素指针、首轨道头 `+0x32`（numData，word）/`+0x38`（onFraction，float）、0x100 字节元素头 FNV（复用既有 `fnv1a_64`）。采样路径每步读的是 Path-A 交换后的 `[0x220]` 槽；步骤 0 行取管线遍历前基线、1/2 行取 63587 后、60 行取采样前、120 行取驱动后，足以裁决"哪一槽在动、读槽 numData 何时为 0"。
- **预注册判据（不得事后放宽）**：
  - ① `phase=pred` 在记录步骤返回非零 → **G1 确认**（谓词门：63581 每步在 0xbaff4d 整块跳过采样）→ v17 = 审计 63564 的内部条件及其在候选上为真的原因；
  - ② `pred` 为零且 `g3-gates` 任一变量失败（`[+0x4A]≠0` 或 `+0x88` 链为空）→ **G3 确认** → v17 = 审计/补填该字段（v11 已证候选 driver+0x88 非 null；此门链是另一对象）；
  - ③ `pred` 零 + 门全过 + slots 序列显示采样时刻读槽（交换后的 `[0x220]`）`n=0x0000` 而另一槽数据在动 → **G4 确认**（63587 Path-A 复位在采样器读到之前吃掉生成遍历的 numData）→ v17 = 逐步配方重排（先采样后 63587，或 Path-B 记录）；
  - ④ `pred` 零 + 门全过 + 采样时刻读槽 numData 非零而位姿仍冻结 → **G2 维持**（63666 输出 → 63582 混合基址）→ v17 = 审计 63582 混合内部（暂缓的重循环）；
  - ⑤ 无崩溃、源稳定（既有行覆盖）。
- **停止与回退**：同 v14/v15（INI 回 off）。

<a id="g2-v16-运行记录2026-10-25-会话判据1落定与采样分支配方事实"></a>
## G2 v16 运行记录（2026-10-25 会话，判据①落定与采样分支配方事实）

> 后验范围说明（2026-10-26）：谓词仅在0/60步直接观测；其余“每步”归因是代码配方与静态路径推演。后续v17已改直调63581，当前判定见[文首](#current-status)。

构建 `…a32766a080+dirty-cl94faaed0c6-20261025T2245xx`（DLL SHA-256 `A732AC08…`，v16 契约记录的部署身份），部署 INI `inject`，2026-10-25 22:58 会话，request=1（actor 00000014），无崩溃/断言，`phase=end … source-stable=true`，[原始日志](diagnostics/CharacterPanel-g2v16-gates-20261025-2258.log)。

**决定性行（逐字）**：

- `phase=g3-gates g100=0x27073572de0 flag4a=0x00 ctx+50=null +40=null +88=null`
- `phase=pred step=0 r=0x01`
- `phase=pred step=60 r=0x01`

INDEPENDENT GRAPH 行共 40 条 = **恰在 40 行观测预算上**。

**判读（预注册判据①落定，未放宽）**：**G1 确认（谓词门）**——63564 谓词在两个检查点（步骤 0 与 60）都返回非零，结合静态分支说明该条件可使 63581 跳过采样/混合/位姿应用；没有逐步分支跟踪，不能把两检点写成每步实测；候选姿态三级哈希仍绑姿全等（`bc160a8c2b3a5a58`×3）。[续审计十五](behavior-graph-construction-audit.md#63564-谓词真值条件与-63581-采样入口约定2026-10-25-续审计十五)随即以指令级链收紧该判读：**谓词恰好读借窗口使非空的那两个字段**（`[graph+0x238]` 借来的 physicsWorld、`[graph+0xF0]` 候选自有的 ragdollDriver，往下走 driver+0x88 → `rigidBodies[0]` → 刚体的 `hkpWorldObject::world` 五级非空链，纯非空判定、无数值比较）——契约期提出的"谓词读的恰是借窗口撑活的字段"这一疑点被证实为 v17 要击败的机制。

**采样分支配方事实（新第二门，同轮运行叠加）**：63587 序言每步写 `word[graph+0x245]=0x101`（245=1、246=1）而 Path A 从不复位 0x246 → 采样时刻 `b246=1` 每步成立 → 63589 走 **63568 分支（仅头部回填 `[0x220]` 槽 numData=word[0x240]，dl=0 即返回）**——**即便谓词不拦，63581 也从未被到达**。支撑证据（逐字）：

- `phase=post-update1 num-anim=0x0 w245=0x101 b246=1 b249=1 out0[+32=0x0000 +38=0.000] out1[+32=0x0000 +38=0.000]`（与 v15 一致——Path A 恒走、0x246 不复位）；
- 步骤 120 槽序列 `phase=slots step=120 p0=0x27049d770e0 n=0x0074 f=1.000 h0=48bfefcceccb91f1 p1=0x27049d534c0 n=0x0000 f=0.000 h1=1ec64cb0955cbfc0`——63568 的回填落在读槽 `[0x220]` 且元素头哈希已变（`48bfefcc…`），对照其余各行的全零原始哈希 `1ec64cb0…`。

**g3-gates 记录（v17 设计输入，判据①下不裁决）**：插件的 58315 自建 ctx 链全空（`ctx+50=null +40=null +88=null`）——判据①成立时 58423 门从未被到达，此行不构成归因证据；但 63581 在 0xbafe48 自建 ctx（58315(figure=hkbCharacter, world)）的链内容与之无关，v17 直调 63581 时读的是后者，**g3-gates 空链不能外推为采样链死锁**，仅作为插件 ctx 形态的既有事实记录。

**槽序列事实**：双槽指针每步交替（步骤 0 `p0=0x27049d534c0/p1=0x27049d770e0` → 步骤 1 互换 → 步骤 2 复原——Path A 交换可见）；两槽在 63587 复位块作用范围内保持原始哈希 `1ec64cb0…`，仅步骤 120 的读槽（第 0 步 63568 写过的槽，偶数次交换后回到 `[0x220]`）呈写入态（`48bfefcc…`）——与"63568 仅第 0 步执行 + 每步交换复位"的静态推演完全相容。

**判定与后继**：按预注册判据①，缺口收敛为谓词真值 + 采样分支配方；[续审计十五](behavior-graph-construction-audit.md#63564-谓词真值条件与-63581-采样入口约定2026-10-25-续审计十五)已产出 v17 所需的四问答案（谓词五级非空链、翻假字段 = `rigidBodies[0]->world`、63581 两实参直调约定、管理器每帧记录 +0x2A=0 的 Path B 语义）。INI 判读后按惯例回退 `off`（部署的 v16 构建保留在位）。

### 2026-10-26 G2 v17 契约（引擎原生每帧序：Path B 记录 + 谓词翻假 + 63581 直调，经续审计十五设计）

> **历史预注册标记（2026-10-26）**：本节保留当时配方、预期和判据，不将后验结果改写为事前预测；后续反例及当前证据边界见[当前结论](#current-status)与 [v17 后验勘误](#g2-v17-运行记录2026-10-26-会话谓词翻假达成与采样门23落定)。

- **设置**：不变（`IndependentGraph=inject`）。流程 = v16 全部（每步 63602 管线 → `+0x12E` 写 → 63587 直调 → `[0x240]` 镜像 → 采样；`pred`/`g3-gates`/`slots` 仪器行原样保留），三处行为变更（[续审计十五](behavior-graph-construction-audit.md#63564-谓词真值条件与-63581-采样入口约定2026-10-25-续审计十五)收束清单的直接实现）：
  1. **Path B 记录（审计 15 Q4）**：每步 63587 记录换成管理器每帧生产者 0x54c130 的真实布局——`float dt@+0x00`、`+0x08..+0x27` 全零、`u16 +0x28 id = 0`、`byte +0x2A = 0`（Path B：不交换双缓冲、不复位轨道、`0x246` 清零、`[0x240]` 经 63575 重算）、`byte +0x2B = 1`、`byte +0x2C = 1`、`byte +0x2E = 1`——v15 沿用的 `+0x2A=1` 是 63602 尾部零 dt 管线专用配方，不是引擎每帧序；
  2. **谓词翻假（审计 15 Q1）**：槽 3（63622，RemoveRagdollFromWorld）移到槽 2 直调之后、驱动循环之前——借用窗口内撤销槽 2 的逐刚体 world 注册，采样时刻 `rigidBodies[0]->world == 0`，63564 谓词返回假（活体唯一不破坏链条的翻假出口；v12 已证槽 3 不动轨道头）。`phase=slot2` 行扩为 `added=… removed=…`（不新增行）；循环后的槽 3 调用保留为幂等复检（喂 `phase=pipeline` 的 `ragdoll-removed=` 字段）；
  3. **63581 直调（审计 15 Q2/Q3）**：每步采样入口 63589 退役（其 b246 分岔正是 v16 落定的绕路），改为直调采样分支 **63581（`SamplingBranchID`）**，实参 = `(candidate, word [graph+0x240])`——第二实参读镜像字段（每步 `numPoseLocal` 镜像照旧，镜像写之后读回），与引擎 63589 调用点 0xbb2ac3 的两活实参约定逐字一致。`sampling_entry`/`SamplingEntryID`/`mode_byte` 全部移除（零悬挂引用）。
- **观测预算**：40 行（净不变；全部既有行保持原序原字段，唯一字段级扩展 = `phase=slot2` 行增 `removed=`）。
- **预期新签名（读日志对照，不构成判据）**：`phase=post-update1` 应显示 `w245=0x0000 b246=0 num-anim=0x0074`——Path B 复位 0x245/0x246 且 63575 重算 numAnimBones（0x74）；`phase=slots` 的双槽指针不再每步交替（Path B 无交换）。
- **预注册判据（不得事后放宽）**：
  - ① `phase=pred` 在记录步骤返回 `0x00` 且候选姿态三级哈希不全等（源稳定、无崩溃）→ **G2 运动证据成立（PASSED on the motion criterion）**——下一 = OAR 接入门（G3+），先补一轮稳定复跑；
  - ② `pred` 返回 `0x00`、姿态仍冻结 → 采样块已跑而下游失败——下一 = 审计十六插桩 63581 内部（63666 输出 / 63582 混合 / 58423 门），以 `slots` 行为入口证据；
  - ③ `pred` 仍 `0x01` → 翻假失败（驱动期间刚体被重新注册，或槽 3 未清 `body[0]->world`）——下一 = 审计再注册路径；
  - ④ `post-update1` 不显示 Path B 签名（`w245=0x0000 b246=0`）→ 记录变更未按设计生效——任何后续步骤前先复查；
  - ⑤ 无崩溃、源稳定（既有行覆盖）。停止/回退：按惯例 INI 回 off。**早步门②/③失败预期化**：63581 的门②/③（`[[0x228]]` 槽的 onFraction 在管线填它之前可能仍为 0.0）在早期步骤失败是**预期瞬态**，不是失败模式——`slots` 行记录之。
- **停止与回退**：同 v16（INI 回 off）。

<a id="g2-v17-运行记录2026-10-26-会话谓词翻假达成与采样门23落定"></a>
## G2 v17 运行记录（2026-10-26 会话，谓词翻假达成与采样门②③落定）

构建 `…a32766a080+dirty-cl94faaed0c6-20261025T160248Z`（DLL SHA-256 `904D6B5B…`），2026-10-26 00:21、actor 00000014/request=1；[原始日志](diagnostics/CharacterPanel-g2v17-native-20261026-0021.log)原样保留。该轮 INI 为 inject，判读后回 off；40 行 INDEPENDENT GRAPH 达到既定预算。120×1/60 秒在一次暂停批次内完成，不是 120 个画面帧。本次文档修订未运行新实验。

**实际观察**：装载 accepted=8.289 ms；字符级激活后 poseLocal 有 116 项；WeapEquip id=2 dispatched，activeNodes 97→125→111，变量哈希变化。候选 pose 在 0/60/120 均 `bc160a8c2b3a5a58`，与 v11–v16 相同；源 pose 前后 `7ac13b9170de9d2b`，11 个源快照分组相同。无崩溃/断言，候选 retained=true；不证明全部共享写状态安全或装载图回收成立。

**初始化与重置时序（原始行）**：

- `phase=slot2 added=true removed=true tracks-before[+32=0x0000 +38=0.000] tracks-after[+32=0x0000 +38=1.000]`
- `phase=post-pipeline1 … out0[+32=0x0000 +38=1.000] out1[+32=0x0000 +38=0.000]`
- `phase=post-update1 num-anim=0x0 w245=0x101 b246=1 b249=1 out0[+32=0x0000 +38=0.000] out1[+32=0x0000 +38=0.000]`
- `phase=pred step=0 r=0x00` 与 `phase=pred step=60 r=0x00`：**仅这两个检点**实测谓词为假，不能写成 120 步逐次观测。
- slots 在记录的 0/1/2/60/120 检点双槽 n=0、f=0；`tracks-end +32=0x0000 +38=0.000`。这不是“轨道头从未初始化”：已有权重在首个 update 后清零，尚须解释其重置与重新填充。

**字段勘误（取证范围）**：

| 字段 | 真实读法 | 能说明什么 |
| --- | --- | --- |
| slots n | `uint16(element+0x32)`，kPose numData | 不是整体 numTracks；n=0 不能证明数量子门失败。 |
| slots f | `float(element+0x38)`，kPose onFraction | 记录检点为0，与原生 onFraction>0 门不满足一致；是已观察的首个阻断线索。 |
| 原生整体轨道数门 | `DWORD(element+0x4)>2` | 与 n 不同；驱动前 head `e022000015000000` 的 +4 为 0x15，仅支持当时值，后续实际值/分支待观测。 |
| h0/h1/gen0elem | 元素头 0x100 字节 FNV | 头部/指针内容证据，不是完整动态骨变换载荷。 |

v17 已按配方直调 63581 到入口/门判定；不能称 63666/63582 采样混合主体已执行。f=0 结合静态门说明应优先核对头部/时序，但没有每步分支轨迹，不能从 n=0 断言每步都由数量门退出。插件自建 ctx 的 g3-gates 不等于 63581 内部原生 context，也没有证明 58423 应用链可达。

**Path B 后验勘误**：预注册只凭 record+0x2A=0 预测“不交换/不复位”，遗漏了 `max(t0,t1)∈[0,MAXFLT)` 条件。v17 的 `dt=1/60`、record+0x18=null 与该复位配方仍呈现 Path A 签名和所记录双槽交换。审计对 {0,-MAXFLT} 复位再减正 dt 的推演解释的是**这套记录/状态/顺序**，不是“所有正 dt 原生帧均不能 Path B”；零 dt 调用方也只提供可满足条件的候选，尚无真实 t0/t1 运行序列。旧判据④正确触发了预期复核，但原生字段/状态仍需完整追踪。

**当前归因**：输出头的初始化/重置/重新填充是优先阻断假设，63771/63870 尾链等仍是嫌疑，未确定唯一写入者。后续还须验证生成变换载荷、两个槽的正确写入顺序、63666/63582 返回、63581 原生 ctx 与 58423 下游应用。旧“单点化/最终精确形态”“管线从不写onFraction”“只需播种”结论证据不足，已由本段收窄；v17 契约保留原预测以便追溯。G2 **NOT PASSED**；不能把 gate G1（谓词）的突破与实验 G1、完整 G3 或 OAR 一致性混称。

<a id="g2-v18-frame-trace"></a>
## G2 v18 预注册观测契约（2026-10-26，尚无运行结果）

目的：把[下一步问题](#next-audit)落实为有界时序取证包，保持 v17 驱动配方和验收不变。仅在 `IndependentGraph=inject` 且精确 AE 1.6.1170 时新增读取；其他模式和运行时没有新增 trace 读取/输出。未新增原生入口、REL ID、hook、种子写入或调用顺序调整。原有源对照、事件与保留机制不变；这不是新机器码写入者审计的完成，也不是 G2 修复。

实现入口是[FrameTrace](../src/animation/behavior_graph/frame_trace.h)、[读取适配](../src/animation/behavior_graph/frame_trace.cpp)及[预算/进度策略](../src/animation/behavior_graph/frame_trace_policy.h)，由[既有驱动循环](../src/animation/behavior_graph/independent_graph.cpp)同步调用。状态只含值、掩码与计数，不跨原生调用保留引擎指针；每个快照重新取两槽元素地址。原始 v17 源码序为 **scratch construct → 63602 → scratch destroy → 既有0x12E写 → 63587 → 0x240镜像 → 63581**，新观察点包围该序而不改变它。

### 行模式与预算

新前缀 `GRAPH FRAME schema=v18`，带 actor/request、零基 `step`、`stage` 与 `completed`。`completed`仅表示已经返回的63581调用数；该入口可能在门前退出，因此不代表有效采样、骨骼写入或有意义动作。只选 step **0/1/2/59/60/119**，各至多五个阶段；重复/无效阶段及非选中步不输出，共最多 **30 条阶段行 + 1 条 summary**。原有最多40条 `INDEPENDENT GRAPH` 和源快照预算独立，不计入31条；缺行、缺槽仍是缺证据。

| stage | 时点 | completed |
| --- | --- | --- |
| pre-pipeline | scratch/63602之前 | 此前已返回的63581调用数 |
| post-pipeline | 63602及scratch配对销毁之后 | 同上 |
| post-update | 63587返回之后 | 同上 |
| pre-sample | 原有0x240镜像及既有谓词观测之后，63581之前 | 同上 |
| post-sample | 63581返回且新进度观察完成之后 | 此轮已完成，正常为step+1 |

阶段行字段：`numAnim`为graph+0x240字；`w245/b246/b249`为原有簿记；`t0/t1`为+0x1E8/+0x1EC的float值与原始32位hex（足够往返精度，时间含义依原审计，不当作clip时钟）；slot0/1分别报告实际元素身份、`numTracks=DWORD(+0x4)`、`numData=word(+0x32)`、`onFraction=float(+0x38)`及其原始bits。数目是计数、fraction无量纲，不把numData当整体轨道数。缺output/element明确标missing，不补零；不解析未知dataOffset或原生ctx。

每次实际63581返回后均读候选现有poseLocal原始缓冲指纹（至多120次，无逐次日志），summary给出phase-rows/total-rows、completed、pose-available/missing、raw-hash-changes、first-changed-completed、baseline。变化数比较相邻**可用观察**，缺测间隙保留missing且不能确定变化真实发生时刻；first字段是首次观察到差异的完成步，回到baseline也会被计入。原始缓冲含padding，hash变化不是有意义骨运动，未变化也只限定这些观察。summary明确 `generator-payload=unobserved native-context=unobserved acceptance=unjudged`，没有自动通过判定。

### 预注册判读与测试步骤

1. 用独立包中的匹配DLL/PDB，核对构建身份；合并INI片段启用inject并重启，第一次暂停开包只运行一次。仓库默认仍off，不部署或覆盖旧包。
2. 对齐actor/request/step和物理槽身份，检查**同一物理槽**在五个时点的numTracks/numData/fraction/t值，区分交换与清除。特别核对v17已见的“slot2/post-pipeline1 f=1 → post-update1 f=0”；不能据最终0反推从未初始化。
3. 若数量门已满足而fraction在update后归零，优先追其复位/再填协议及原生正常源帧同阶段对照；若真实numTracks也异常，则把数量字段/元素身份单独交回审计。新数据只能定位观察边界之间的变化，不自动证明某条机器指令是唯一写入者。
4. 若summary观察到raw哈希差异，仍须查有效变换、相位、数值、必要骨及源侧对照；若仍无差异，也不能越过未观测的生成载荷、63581原生ctx、63666/63582与58423下游应用。完整原生帧与写入者审计仍待执行，不能用候选轨迹替代。
5. 保存完整日志与构建身份，确认原有source-stable/source-pose和受控恢复记录，再将INI回off并重启。G2仍需有意义原版idle、有效骨运动、源对照与后续生命周期证据；装载图仍保留，不能计为清理通过。

离线验证仅[两个策略/进度案例](../tests/behavior_graph_frame_trace_test.cpp)和Release构建/包核验，命令与实际结果见[构建记录](build.md#v18-frame-trace-build)。契约设计时没有v18游戏日志；该轮已于2026-10-26回传并判读，见下节。

<a id="g2-v18-run"></a>
## G2 v18 运行记录（2026-10-26 会话，五阶段双槽与 Path A 复位实证）

构建 `CharacterPanel-scenegraph-behavior-graph-v18-frame-trace-20261026-7ee4cce3bf-cl94faaed0c6-20261026T143615Z`（DLL SHA-256 `72c33a0e…`，与包内manifest一致），2026-10-26 23:01 首次暂停开包（actor 00000014、request=1；`Panel menu opening=true` 23:01:42.914 → `opening=false` 23:01:50.205）。[原始日志](diagnostics/CharacterPanel-g2v18-trace-20261026-2301.log)原样归档（40,366字节，SHA-256 `D42DB99D…`）。该轮部署INI为`inject`，判读后已回`off`，v18 DLL/PDB留在原位。全程无`[E]`、无崩溃；15条`[W]`均为既有DAC/studio-light警告。

**预算与行数**：`GRAPH FRAME` 30条阶段行 + 1条summary（step 0/1/2/59/60/119 × 五阶段，恰好用满31行）；`INDEPENDENT GRAPH` 40行（既定预算）；`BEHAVIOR GRAPH` 36行 = 三份12行源基线块（构造前/装载后/结束），三块逐行全等。summary：`phase-rows=30 total-rows=31 completed=120 pose-available=120 pose-missing=0 raw-hash-changes=0 first-changed-completed=none-observed baseline=bc160a8c2b3a5a58`。

**运行流程事实**（与v17同型）：装载 `accepted ms=8.726`，字符级激活后 `pose-local=…/116`、`active-nodes=…/97`；`WeapEquip` id=2 `dispatched`；`phase=borrow world=… ragdoll=own`；`phase=slot2 added=true removed=true tracks-before[+32=0x0000 +38=0.000] tracks-after[+32=0x0000 +38=1.000]`；`phase=slot0c before/after 110=0x23cf2cac900 … 246=0`；`phase=g3-gates … ctx+50=null`；`phase=pred step=0 r=0x00`、`step=60 r=0x00`；`phase=pipeline steps=120 shell=pairs world-restored=true ragdoll-removed=true tracks-end[+32=0x0000 +38=0.000]`；`phase=end constructed=true loaded=true assembly=true separated=true source-stable=true retained=true`。驱动中活动节点 97→125→111、变量哈希 `893b33fd…`→`214e2111…`→`a4bb41b6…`，`gen0elem` 三个检查点恒为 `7ced7f7208732e50`；驱动后 `phase=drive-done … source-pose-before=c58dcc07593b5e75 source-pose-after=c58dcc07593b5e75 candidate-pose-0/60/120=bc160a8c2b3a5a58`。

**五阶段观测**（两槽元素恒为 `E0=0x23cc9fa8510`（slot 2 写过的那个）与 `E1=0x23cc9faa820`；30条阶段行的 `numTracks(+0x4)=21`、`numData(+0x32)=0` 在所有阶段、两个槽上都不变，故不逐行重复；合并行里的 `0 → 116` 表示该步 update 后原生值为 0、随后插件镜像把它写成 116）：

| step | stage | numAnim(+0x240) | w245/b246 | t0/t1 | slot0 | slot1 |
| --- | --- | --- | --- | --- | --- | --- |
| 0 | pre-pipeline | 0 | 0x0/0 | -MAXFLT/-MAXFLT | E0 f=1.000 | E1 f=0.000 |
| 0 | post-pipeline | 0 | 0x0/0 | -MAXFLT/-MAXFLT | E0 f=1.000 | E1 f=0.000 |
| 0 | post-update | 0 | 0x101/1 | 0/-MAXFLT | E1 f=0.000 | E0 f=0.000 |
| 0 | pre-sample | 116 | 0x101/1 | 0/-MAXFLT | E1 f=0.000 | E0 f=0.000 |
| 0 | post-sample | 116 | 0x101/1 | 0/-MAXFLT | E1 f=0.000 | E0 f=0.000 |
| 1 | pre-pipeline | 116 | 0x101/1 | 0/-MAXFLT | E1 f=0.000 | E0 f=0.000 |
| 1 | post-pipeline | 116 | 0x101/1 | 0/-MAXFLT | E1 f=0.000 | E0 f=0.000 |
| 1 | post-update | 0 | 0x101/1 | 0/-MAXFLT | E0 f=0.000 | E1 f=0.000 |
| 1 | pre-sample | 116 | 0x101/1 | 0/-MAXFLT | E0 f=0.000 | E1 f=0.000 |
| 1 | post-sample | 116 | 0x101/1 | 0/-MAXFLT | E0 f=0.000 | E1 f=0.000 |
| 2 | pre-pipeline / post-pipeline | 116 | 0x101/1 | 0/-MAXFLT | E0 f=0.000 | E1 f=0.000 |
| 2 | post-update / pre-sample / post-sample | 0 → 116 | 0x101/1 | 0/-MAXFLT | E1 f=0.000 | E0 f=0.000 |
| 59 | pre-pipeline | 116 | 0x101/1 | 0/-MAXFLT | E1 f=0.000 | E0 f=0.000 |
| 59 | post-update / pre-sample / post-sample | 0 → 116 | 0x101/1 | 0/-MAXFLT | E0 f=0.000 | E1 f=0.000 |
| 60 | pre-pipeline / post-pipeline | 116 | 0x101/1 | 0/-MAXFLT | E0 f=0.000 | E1 f=0.000 |
| 60 | post-update / pre-sample / post-sample | 0 → 116 | 0x101/1 | 0/-MAXFLT | E1 f=0.000 | E0 f=0.000 |
| 119 | pre-pipeline / post-pipeline | 116 | 0x101/1 | 0/-MAXFLT | E1 f=0.000 | E0 f=0.000 |
| 119 | post-update / pre-sample / post-sample | 0 → 116 | 0x101/1 | 0/-MAXFLT | E0 f=0.000 | E1 f=0.000 |

**判读（预注册，不放宽）**：

1. **数量门满足、fraction 门不满足**：`numTracks(+0x4)=21` 在全部阶段成立（审计十四 Q3-2 的 `+4>2` 门过），但 63581 采样时点的双槽 `+0x38` 均为 0，`+0x38>0` 门不过。故落入预注册第3条“数量门已满足而 fraction 在 update 后归零”分支。
2. **复位定位在本步 63602 之后、63587 返回之前**：step 0 的 `pre-pipeline`/`post-pipeline` 两槽为 E0 f=1.000 / E1 f=0.000（slot 2 写入），`post-update` 变为 E1/E0 且两槽 f 均为 0。这排除“63602 管线写坏”和“从未初始化”两种读法；与[审计十四 Q1-3](behavior-graph-construction-audit.md#63587-内部条件与位姿最后一公里2026-10-25-续审计十四)的 Path A 复位块一致——交换后重置写槽的轨道头（逐轨道清 onFraction、flags，尾写 `[tracks+0x32]=0`）。具体指令写入者仍是静态候选，v18 只对齐了观察边界。
3. **每步都是 Path A**：`w245=0x101 b246=1 b249=1` 与“每个 63587 调用恰好交换一次双槽”在全部记录时点成立，`numAnim` 在每次 update 后为 0（Path A 不经 63575 重算），pre-sample 的 116 全部来自插件镜像写。v17 预注册的 Path B 签名（`w245=0 b246=0 numAnim=0x74`）未出现。
4. **t 对被钉在复位值**：step 0 前为 `(-MAXFLT,-MAXFLT)`，首次 update 后恒为 `(0,-MAXFLT)` 且 120 步内不累积。按[审计十六](behavior-graph-construction-audit.md#audit16)修正后的条件 `Path B ⇔ record+0x2A==0 且 fork 时 t_larger ≥ 0 且 t_smaller > -MAXFLT`：序言先 `-= dt`（dt=1/60），fork 时 `t_larger=-dt<0`，因此本配方每步只能 Path A；函数内唯一能推进 t 的眼位 arm 块以 `record+0x18`（`BSAnimationUpdateData::optionalEyePosition`）非空为前提，而本配方该字段恒为 null。**这不是“所有正 dt 原生帧都不能 Path B”的推广**——原生帧由 REFR 的 `ModifyAnimationUpdateData` 填眼位后 arm。
5. **采样主体未执行**：`completed=120` 只表示 120 次 63581 调用返回；候选 pose 原始缓冲在 120 次观察中 0 次变化（`baseline=bc160a8c2b3a5a58`），`numData` 全程 0（63568 的头部回填被绕过），`phase=pred` 两个检查点均为假（`r=0x00`），与“门后 63666/63582/58423 未执行”相容。生成载荷、63581 自建 ctx（非原生）、63666 输出与 58423 下游应用仍未观测。

**判定**：G2 仍 **NOT PASSED**——120 步内没有有意义的原版 idle 位姿运动，候选 pose 与 v11–v17 的同一条 `bc160a8c2b3a5a58` 相同；源侧 `source-stable=true`、三块基线全等、装载图仍保留（不计为清理通过）。

**由此收窄的下一问**：本配方让 t 停在 Path A 的复位值、`record+0x18` 恒空，于是 63575 的采样数解析、`0x246` 清零、63568 回填全部被绕过；下一步属[有界审计问题](#next-audit)（audit16），不是修复或播种。

<a id="v18-handoff"></a>
## v18 测试交接、回传与停点

**v18 已回传并判读（2026-10-26 23:01 会话），G2 仍 NOT PASSED；最新实际运行证据是 v18。** 回传内容：完整 `CharacterPanel.log`，已原样归档为 [v18 原始日志](diagnostics/CharacterPanel-g2v18-trace-20261026-2301.log)（40,366字节，SHA-256 `D42DB99D…`）；其中 `SCENEGRAPH BUILD` 身份与包内 manifest 一致，`GRAPH FRAME` 30条阶段行+1条summary齐备，`INDEPENDENT GRAPH` 40行、`BEHAVIOR GRAPH` 36行，无`[E]`、无崩溃；开包 23:01:42.914、关包 23:01:50.205。判读见[v18 运行记录](#g2-v18-run)。部署的 INI 已按预注册第5步回`off`（v18 DLL/PDB 留在原位，v17 DLL/PDB 备份在`dist/deploy-backup-20261026-v17/`）。

下一步不再等日志，而是[有界审计问题](#next-audit)里的 audit16；在此之前不实施播种、不改驱动配方、不推进 G3+。测试步骤（复盘或重跑用）仍见上节[预注册步骤](#g2-v18-frame-trace)及包内`README-测试步骤.md`。

包文件：`build/behavior-graph-v18-trace-20261026/CharacterPanel-behavior-graph-v18-frame-trace-20261026.zip`（9,763,102字节）；SHA-256 `dad6b72ce17e53ae0d7e823bbe5bec28ad29aa0cf3f98dffa5885b42081c8ae2`。匹配DLL/PDB GUID `{7F586C2E-9F62-4CE8-B33B-D19AFE9C6461}`、age317。完整二进制哈希、源文件身份与限制见包内`manifest.json`和[构建记录](build.md#v18-frame-trace-build)。

首次记录的那份包（身份`…-92a3ed071d+dirty-cl94faaed0c6-20261026T063108Z`、ZIP `cd364569…`、DLL`e24ad2a5…`）在本机已不存在：`build/`当时仍带标签`scenegraph`、只有v17 DLL/PDB。因此按同一提交`7ee4cce`（文件哈希与包内manifest所列一致）重建并重新打包，实际包身份为`CharacterPanel-scenegraph-behavior-graph-v18-frame-trace-20261026-7ee4cce3bf-cl94faaed0c6-20261026T143615Z`；DLL SHA-256 `72c33a0e…`（1,249,280字节）、PDB `1baceb55…`。与旧记录的两处惰性差异：标签带提交后身份而非`92a3ed0+dirty`；原先未跟踪且无人引用的`src/base/util.cpp`/`util.h`已不在检出中，未再编译。旧哈希只作历史，不能当作本次加载证明；分析回传时以实际加载身份和包哈希为准。

回传清单（本轮已按此执行，供后续复用）：

- 完整`CharacterPanel.log`，包含实际`SCENEGRAPH BUILD`身份、原有源对照/恢复行及新`GRAPH FRAME`，同时保留所使用包的manifest。
- 首次开包、关包及异常发生时刻与操作；有崩溃时保留对应报告，不能用最后一条日志推断执行到达点。
- 若新行缺失或不足，保留实际runtime版本、INI配置、是否重启、该进程此前是否已触发过实验，以及实际加载DLL的身份。实验每进程至多一次；缺行仍是不完整证据，不直接判失败原因。

`completed`、头部字段或raw哈希变化都不自动通过G2。待新日志到达后，先按同一步号与物理槽身份比较五阶段，再决定下一个有界问题；原生写入者、生成载荷、内部ctx及应用/生命周期仍按既有判据待证。

<a id="g2-v19-contract"></a>
## G2 v19 契约、实现与运行（2026-10-26，眼位输入因果实验；已运行：分支 6，无 arm）

**目的**：只验证[审计十六](behavior-graph-construction-audit.md#audit16)给出的因果链——`record+0x18`（`BSAnimationUpdateData::optionalEyePosition`）非空 → 0xbb252b 眼位推进块把 `t_smaller` 抬出复位值 → 下一次 63587 分岔进入 Path B → 原生 `0x240`/`0x246` 改变 → 63581 的门是否因此可过。它是**单变量因果实验**，不是修复，也不改变 G2 验收；即使全部命中，G2 仍未通过。

**实现状态（2026-10-26）**：代码已实现并提交 `e17d5d1`；Release 构建身份 `CharacterPanel-scenegraph-behavior-graph-v19-eye-arm-20261026-e17d5d12f6-cl94faaed0c6-20261026T153952Z`（DLL SHA-256 `344d9abb…`，1,249,792字节；PDB `9848cd14…`，35,418,112字节；CodeView `{7F586C2E-…}` age 319），三个 engine-free 案例通过。**已部署（未打包）并运行一次**：v18 的 DLL/PDB 与 `off` INI 备份在 `dist/deploy-backup-20261026-v18/`；23:46 会话落在预注册分支 6（无 arm），部署 INI 已回 `off`，v19 DLL/PDB 留在原位，详见[运行记录](#g2-v19-run)。实现点=[frame_trace_policy.h](../src/animation/behavior_graph/frame_trace_policy.h) 的 `EyePoint`/`make_eye_point`/`arm_observed`、[frame_trace.cpp](../src/animation/behavior_graph/frame_trace.cpp) 的 `schema=v19` 行与 summary `arm-observed`、[independent_graph.cpp](../src/animation/behavior_graph/independent_graph.cpp) 每步记录的 `+0x18`；除 `inject`+精确 1.6.1170 外的模式不改记录。

**依据（审计十六，RVA）**：臂块门 = `record+0x18 != 0`、`record+0x2A == 0`、`record+0x2B != 0`、`[graph+0x248] == 0`，且（`record+0x2C == 0` 或交换前旧读槽 `+4>2` 且 `+0x38>0`）；臂块写 `t_smaller = xmm6 + t_larger`（仅当大于 `1.25*dt`）并跳过复位，`xmm6` 由 rootNode 距离决定，**且 0xbb2603 的 `63864()` 会再归一它**（审计十六原写的“rootNode 为空时固定 0.25”已被 v19 运行期否证，见[运行记录](#g2-v19-run)）；Path B 条件 = `+0x2A==0 && t_larger ≥ 0 && t_smaller > -MAXFLT`，Path B 写 `0x240 = 63575(graph, record+0x18, record+0x2C, &out, record+0x28)`、`[0x246]=0`，不交换、不复位；63581 门 = 双槽 `+4>2`、`+0x38>0`、`t_larger ≥ 0`、`t_smaller < 0`，`f=1.0` 只由 63582 写到 63666 的输出元素。

**唯一改动**：在既有 inject 驱动循环的每步 `update_record`（[independent_graph.cpp](../src/animation/behavior_graph/independent_graph.cpp) 里的 `alignas(8) std::uint8_t update_record[0x30]`）写 `*reinterpret_cast<void**>(update_record + 0x18) = &eye;`，`eye` 是栈上 3×float：

- `[candidate + 0x218]`（rootNode）非空时 `eye = {rootNode[0xA0], rootNode[0xA4], rootNode[0xA8] + 900}`（距离 900 > 800，取满步长 0.25；沿 Z 偏移，不耦合任何轴向语义）。
- rootNode 为空时 `eye = {0,0,0}`（此时臂块固定 0.25，不读内容，但仍要求指针非空）。

指针只在该次 63587 调用期间有效，不保存、不跨调用、不写引擎内存。其余一切不变：调用序（63602 → `[behavior+0x12E]` 写 → 63587 → `0x240` 镜像 → 63581）、40 行 `INDEPENDENT GRAPH` 预算、30+1 行 `GRAPH FRAME` 预算、源对照/保留/恢复逻辑与默认 `off`。

**臂窗口提醒（决定本轮可解释性）**：`+0x2C=1` 时臂块还要求“交换前旧读槽 `+0x38>0`”。v18 只有 step 0 的旧读槽带 slot 2 写入的 1.0，随后 Path A 复位会把它清零，而 `f` 又只能由 63582 写。因此**本实验的臂窗口实际只有第 0 步**：第 0 步若未成功 arm，本轮即落预注册分支 2/6，不得重跑调参。

**观测（不新增行种，预算不变）**：行前缀改为 `GRAPH FRAME schema=v19`，每条既有阶段行末尾追加 `root218=<ptr|null> b248=<byte> eye=<ptr|null>`；`summary` 追加 `arm-observed=yes|no`（判据：post-update 的 `min(t0,t1)` 高于 `-FLT_MAX`，即不再是 `{0,-MAXFLT}` 复位值）。阶段行数仍为 30+1，`phase=post-update1`/`pred`/`slots` 保持原字段、不新增行。

**预注册判读（不得事后放宽）**：

1. `root218` 全程 null 且 `b248 != 0`（门不满足）→ 记“臂块外层门不满足”，**停止**；下一有界问题是 `[graph+0x248]` 语义与 rootNode 来源，不得改记录继续试。
2. `eye != null`、臂块生效（t 被改写）但下一次 fork 仍 Path A → 说明 fork 还有未记录的输入：**停止**回审计，不得加 dt 变体或改 `+0x2A`。
3. 出现 Path B 签名（下一个选中步 `w245=0x0`、`b246=0`；`0x240` 同时记录其原生值，LOD 表可能把它降到 0，不要求非 0）且 63581 仍因双槽 `+0x38=0` 失败 → 记“因果链①成立、②被 f 门挡住”；下一有界问题=63582 的首写条件与双槽 f 初值（新契约），**本轮回写 f 属违规**。
4. 63581 门全过且 63582 运行（`f`/`numData` 被写）→ 记“因果链②成立”；再单独看 pose。pose 不变时结论只说“门在 63666/63582/58423 下游”，不得宣称 G2 通过。
5. 崩溃/断言 → 立即停，保留日志与崩溃报告，部署与 INI 回到 v18 状态（`off`）；同一会话不得重试。
6. `eye` 非空但 t 毫无变化 → 记“臂块内部门/阈值未满足”（`xmm1 > 1.25*dt` 或旧读槽 f 门），**停止**，不调偏移量重试。

**构建与部署**：构建已完成（`CHARACTER_PANEL_BUILD_LABEL=behavior-graph-v19-eye-arm-20261026` 的 Release，离线三例通过）；**已按 v18 流程部署（不打包）并运行一次**：v18 对（DLL `72C33A0E…`、PDB `1BACEB55…`）与 `off` INI 备份在 `dist/deploy-backup-20261026-v18/`，MO2 实例的 `CharacterPanel/SKSE/Plugins/` 现为 v19 DLL `344d9abb…`/PDB `9848cd14…`；本轮 INI 为 `inject`，判读后已回 `off`。判读见[运行记录](#g2-v19-run)。回滚=还原那三份备份并重启。

**验收边界**：本契约只测“眼位 → t → Path B → 63581 门”的因果链；G2 仍需有意义的原版 idle、有效骨运动、源侧对照不变与后续生命周期证据。运行前仓库按 [v18 停点](#v18-handoff) 与[有界问题](#next-audit) 执行，默认 `IndependentGraph=off`（部署副本除外）。

<a id="g2-v19-run"></a>
## G2 v19 运行记录（2026-10-26 会话，无 arm：臂块步长门槛与候选 rootNode 缺失）

构建 `CharacterPanel-scenegraph-behavior-graph-v19-eye-arm-20261026-e17d5d12f6-cl94faaed0c6-20261026T153952Z`（DLL `344d9abb…`，与构建记录一致）。2026-10-26 23:46 会话：`Panel menu opening=true` 23:46:47.728 → `opening=false` 23:46:59.707，实验行 23:46:47.739–47.751。[原始日志](diagnostics/CharacterPanel-g2v19-eye-arm-20261026-2346.log)原样归档（41,470字节，SHA-256 `45AAEE09…`）。部署 INI 判读后已回 `off`，v19 DLL/PDB 留在原位。

**行数与预算**：`GRAPH FRAME schema=v19` 30条阶段行 + 1条 summary；`INDEPENDENT GRAPH` 40行；`BEHAVIOR GRAPH` 36行（三份12行，逐行全等）；无`[E]`、无崩溃，15条`[W]`为既有 DAC/studio-light 警告。summary：`completed=120 pose-available=120 pose-missing=0 raw-hash-changes=0 baseline=bc160a8c2b3a5a58 arm-observed=no`。

**决定性观测**：

- 每条 FRAME 行：`root218=0x0`、`b248=0`、`eye=0x1d6e56bd30`（按契约非空；rootNode 为空故内容为零向量）。
- step 0 pre/post-pipeline：`t0=t1=-MAXFLT`，slot0=E0 `+38=1.000`（slot 2 写入）、slot1=E1 `+38=0.000`，双槽 `+4=21`、`+32=0`。
- step 0 post-update：`t0=0`、`t1=-MAXFLT`，`w245=0x101 b246=1 b249=1`，双槽交换且 `+38` 均为 0；之后所有阶段 t 不变，`arm-observed=no`。
- 120步、`candidate-pose-0/60/120=bc160a8c2b3a5a58`；源 pose 前后 `df87ebc0d3550cda`，`source-stable=true retained=true`；`phase=pred step=0/60 r=0x00`。

**判读（预注册分支 6 落定）**：臂块外层门全部满足——`record+0x18` 非空（`eye` 非空）、`+0x2A=0`、`+0x2B=1`、`[graph+0x248]=0`、step 0 的旧读槽 E0 `+0x38=1.0>0` 且 `+4>2`——所以不是分支 1，也不是旧读槽 f 门。失败在臂块内部：rootNode（`[graph+0x218]`）为 0 时距离分支被跳过（`xmm2=FLT_MAX`），步长候选取 0.25，但 0xbb2603 的 `63864()` 返回值随后覆盖它（`63864()>0.25` 才保留 0.25，否则 `xmm6 = 63864() - 0.5*dt`），最终 `xmm1 = xmm6 + t_larger` 必须大于 `1.25*dt=0.02083` 才写 t。观测到 t 完全未变 ⇒ `xmm1 ≤ 0.02083`；在 `t_larger=-dt=-0.01667` 下即 `xmm6 ≤ 0.0375`（等价于 `63864() ≤ 0.0458`，或返回 NaN）。按契约**停止，不调偏移量、不改 dt 重试**。

**本轮新增/修正的事实**：

1. 候选图 `[graph+0x218] rootNode = 0`——首次直接读到；与审计十四时代“v15 借用期 rootNode 非空”的旧读法冲突，说明候选装载链没有装配 rootNode（与审计十“装载止步 63949、63870 尾链”一致）。rootNode 为空也让 63575 的距离阈值路径不可用。
2. 审计十六“rootNode 为空时臂块步长固定 0.25”被运行期否证：0xbb25fc 之后还有一次 `63864()` 归一（0xbb2603–0xbb261c），小返回值会把步长压到 0.25 以下；本轮的 no-arm 正发生在这里。
3. `b248=0` 使 `[graph+0x248]==0` 门满足；`eye` 按设计非空——v19 的记录字段本身没有传错。

**判定**：G2 仍 **NOT PASSED**——没有 arm、没有 Path B，pose 与 v11–v18 同为 `bc160a8c2b3a5a58`；本轮只证明“眼位非空 + 外层门满足”不足以保证臂块执行。

**下一有界问题（架构决策点）**：[审计二十一](behavior-graph-construction-audit.md#audit21) 给出决定性事实：63602 的生成块整体受第一实参 `behaviorGraph` 守卫（0xbb62ae），块内含 65149 生成与 63602 内部 63587；壳图没有 behaviorGraph，所以**这两次调用在 v7–v19 里从未发生**。同时 63602 的 sink 是**栈子对象指针描述符**（`[rbp+0x140]` 起 12 个槽），与 0xba4e30 的 sink（首字段 BSFixedString）是不同具体类；`65177` 拼 `<obj+0x38>/<buffer+0x38>`，`59268` 只是 `movss [rcx+0xF0], xmm1; ret`。结论：**没有安全可行的“最小生成实验”**——要候选被生成，只能把候选登记进某个 manager（突破 G1 的 no-registration 边界），或接受独立图停在现有状态。这是架构选择，不是下一轮观测；`audit21` 之后不新增原生调用、不改配方。

<a id="g2-v20-generation-entry"></a>
## G2 v20 预注册：受控恢复原生生成入口（2026-10-27，未运行）

> audit23 静态补注：本契约和既有 v20 包保持。63589 的外层职责包含生成及后续场景应用，因而一个入口变量不是一个内部阶段；新结果需按[复制/应用分工](behavior-graph-construction-audit.md#audit23)解释，不能只据变化就归因 Generate。

这是audit22后的新设计，不复活旧fake-sink方案。检验v19直调63581是否绕过原生同步生成/apply；尚未证明manager登记必要，本轮不新增登记；包装器的潜在隐藏依赖仍未排除。依据[完整对齐审计](behavior-graph-construction-audit.md#audit22)，63589两参为candidate与存活mode字节指针，指针同步消费不保存；b246非零走63568，mode0到58343→58376→Generate，ctx/output/额外参数由引擎准备。

**单一行为变量：** 仅现有`IndependentGraph=inject`且精确AE1.6.1170，将每步63581换成63589(candidate,&mode0)。mode0为驱动栈上零字节，调用期间存活；新relocation只在此profile获准驱动时解析。其他模式/版本保持63581。眼位表达式/栈生命期、120×1/60、flags/0x12E/0x240、world借用/add/remove、事件、retention、源对照均保持v19。不登记源/私有manager、不直调Generate、不造buffer/sink或种子。

**一次guard：** 激活后、context/event/world借用前检查候选behavior非空且私有、outer/character一致且active，activeNodes/变量/pose可写存储可用且无明显源别名，双输出wrapper/元素不与任一源槽混用。失败输出一次理由并跳过v20驱动，保留外层源/end观察及retention，不跨活跃借用return、不静默回退。只排除明显别名，不证明深层隔离；双候选槽自别名拒绝是本实验的保守条件，不推广成全部引擎图必须物理分离。

**观测：** `GRAPH FRAME schema=v20 entry=63589 mode=0`只标意图，不证明内部63568/Generate执行；31条预算、选中步、v19眼位/arm记录保持。pre/post-sample兼容名称现包围外层生成/apply；completed只计返回63589，不代表有效采样。旧40条INDEPENDENT GRAPH与源快照另计；guard至多一条独立GRAPH ENTRY，合计新增前缀至多32行（FRAME30+1、ENTRY1）。guard失败仅有ENTRY skip，无FRAME是预期跳过，不是漏采样。

**预注册判读：** guard/原生门失败→跳过并检查真实输入；头部原生变化→定位wrapper边界但不自动通过；raw pose变化→进一步核对有限值/必要骨/相位/源控制；填头而pose不变→限于实际generation/output/apply问题，不播种/登记manager；源漂移/崩溃→立即停止、保留完整日志与匹配PDB，不在同一会话调参重试。G2及G3–G6、装载销毁判据不放宽。

本轮只加[一个参数化生产guard案例](../tests/behavior_graph_generation_entry_test.cpp)，不模拟引擎；旧预算/进度/眼位算法未改、既有证据复用。Release与包身份见[构建](build.md#v20-generation-entry-build)。v20未运行，最新实际运行仍为v19，G2未通过。

<a id="v20-handoff"></a>
## v20 测试交接与当前停点

**v20a 已运行一次并取得首次运动证据：入口 `63589(mode0)` 到达，输出头被填充、Path B 与 arm 出现，候选 raw pose 在 120/120 次调用中变化，源侧控制不变；但逐骨/有限值与视觉证据仍缺，G2 仍 NOT PASSED。** v21 pose 内容解码已于 2026-10-27 22:18 运行一次：三点全有限、`moved=94`、两根抽样骨在动，但 `source-equal=8/21/15` 命中预注册第 3 条；该条经[v21 运行记录](#g2-v21-run)审计为误报（不同址、跨 run 不相关、判据为精确值相等），G2 仍未通过。下一步已由 [v22](#g2-v22-contract) 实现、部署并运行一次：[v22 运行记录](#g2-v22-run)取得 pose 内容证据（三点全有限、94 条在变化、两骨在动、`alias=no`、`source-equal-moving=0`），G2 的剩余缺口是跨批次持续性与语义对应；不新增原生调用、不预先播种或推进后续关卡。

原冻结包（身份 `…-242d0a50c5+dirty-cl94faaed0c6-20261027T070457Z`、ZIP `3f9f9182…`、DLL `6c76c2a7…`）不在本检出中，故 2026-10-27 以同一提交源重建：新身份 `CharacterPanel-scenegraph-behavior-graph-v20-generation-entry-20261027-a83e5229e7-cl94faaed0c6-20261027T134152Z`（干净 `a83e522`，身份无 `+dirty`），DLL `13688a9d…`（1,253,888 字节）、PDB `07a509da…`（35,467,264 字节），CodeView GUID `{7F586C2E-9F62-4CE8-B33B-D19AFE9C6461}` age 320；ZIP 9,750,395 字节、SHA-256 `076cf6f7…`，五个条目重解压复核通过。两次构建不声称字节等同，旧哈希只作历史；判读一律以实际加载的 `SCENEGRAPH BUILD` 身份与包内 manifest 为准，不用较新提交号替代重新构建证据。详见[构建与重建记录](build.md#v20-rebuild-2026-10-27)。

使用 `build/behavior-graph-v20-generation-entry-20261027/CharacterPanel-behavior-graph-v20-generation-entry-20261027.zip`，按包内 `README-测试步骤.md` 和[预注册契约](#g2-v20-generation-entry)操作。已按 v18 流程部署（2026-10-27 21:42）：v19 三件套（DLL `344d9abb…`、PDB `9848cd14…`、`off` INI 1,034 字节）备份在 `dist/deploy-backup-20261026-v19/`，MO2 实例 `CharacterPanel/SKSE/Plugins/` 现为 v20 DLL/PDB，仓库样例仍为 `off`；部署 INI 曾为 `inject`（1,055 字节 `1db104fa…`，其余键未动），首轮崩溃后已按预注册回 `off`。回滚=还原该备份并重启。

回传完整 `CharacterPanel.log`、实际 `SCENEGRAPH BUILD` 身份、所用 manifest，以及首次暂停开包、关包和异常发生的时刻/操作。先核对精确 AE 1.6.1170、匹配 DLL/PDB、合并后的 INI 和重启状态；实验每进程只触发一次。缺行时还要保留该进程此前是否已经运行过实验，不能据缺行猜原因。测试结束把 `IndependentGraph` 恢复为 `off` 并重启；异常或源漂移时立即停止并保留日志/崩溃报告与匹配 PDB，不在同一会话调参重试。

`GRAPH ENTRY` 最多 1 行，`GRAPH FRAME` 最多 30 阶段行加 1 摘要；新增前缀合计最多 32 行，旧 40 条和源快照另计。guard skip 没有 FRAME 是预期跳过。entry/mode 仅标调用意图，completed 只计返回的 63589，raw hash 含 padding，均不等于有效生成或采样。

按[audit23 的分工](behavior-graph-construction-audit.md#audit23)比较生成 → poseLocal 复制 → 场景应用。恢复一个入口变量同时恢复多类内部职责，有变化不能唯一归因 Generate；现包没有真实数值载荷、原生分支或 node 应用 trace。

| 新结果 | 下一步判读 |
| --- | --- |
| guard skip / 门未过 | 核对实际输入边界，不以插件静默换回旧入口、manager 登记或手工种子绕过。 |
| 头部变化 | 先定位 wrapper 边界，不自动认定 pose 有效。 |
| raw pose 变化 | 再核对有限数值、必要骨、相位和源控制，不自动通过 G2。 |
| 头已填、pose 仍不变 | 限定到生成、复制和应用问题，不能直接归咎插值 t 或播种。 |
| 崩溃 / 源漂移 | 停止，保留完整证据；不在同次会话重试。 |

完整 G3–G6、OAR 语境、深层隔离和装载图回收仍未验收。上表"崩溃 / 源漂移"行已经发生了：首轮结果见下节，此处只记录交接与那次运行，不新增诊断代码。

<a id="g2-v20-run"></a>
## G2 v20 运行记录（2026-10-27 21:48 会话，入口前崩溃）

构建 `CharacterPanel-scenegraph-behavior-graph-v20-generation-entry-20261027-a83e5229e7-cl94faaed0c6-20261027T134152Z`（DLL `13688a9d…`、PDB `07a509da…`），部署 INI=`inject`，运行时 `1-6-1170-0`。21:46:12 加载，`SCENEGRAPH BUILD` 身份与包内 manifest 逐字一致；21:48:57.911 `Panel menu opening=true`，实验行 21:48:57.922–57.931，随即崩溃。原始日志原样归档为 [CharacterPanel-g2v20-entry-crash-20261027-2148.log](diagnostics/CharacterPanel-g2v20-entry-crash-20261027-2148.log)（16,920 字节，SHA-256 `2278201CDBCF9090EC1CC3FD038C751D70B66C3FF4614C8CE5B3D63D89E11127`），崩溃报告为 [CharacterPanel-g2v20-crash-report-20261027-2148.log](diagnostics/CharacterPanel-g2v20-crash-report-20261027-2148.log)（119,326 字节，SHA-256 `4074DF23232254922CEBC44E1DEBD187C50DBD4798C53C5161899ACA0788D2F7`）。

**guard 与崩溃位置**：`GRAPH ENTRY schema=v20 … outcome=ready reason=ready entry=63589 mode=0` —— 唯一的 guard 通过（behavior 非空、私有且 outer/character 一致，activeNodes/变量/pose 可写存储可用，四组输出/元素不与源槽或彼此混用）。崩溃是 `EXCEPTION_ACCESS_VIOLATION`，`CharacterPanel.dll+0x28067`，`independent_graph.cpp:955` 的 `mov r8, [rbx+0x88]`，`rbx=0x7BD6CD336F4B8024`（非规范指针），访问目标 `[rbx+0x88]`。该行属于 **v16 起就存在的只读 g3-gates 诊断**，位置在驱动循环之前，因此本轮 **63589 一次都没有被调用**：没有 `GRAPH FRAME`、没有 `completed`、没有 pose 采样，也没有任何关于生成/输出/应用的证据。

**原因（静态声明与运行期一致）**：`RE::hkbContext` 的声明大小正是 `0x50`（[hkbContext.h](../extern/CommonLibSSE/include/RE/H/hkbContext.h) 末尾 `static_assert(sizeof(hkbContext) == 0x50)`），而诊断把 `ctx+0x50` 当作 chain 起点读取 —— 那是 `context_storage[sizeof(RE::hkbContext)]` 数组**末尾之后**的栈槽，属越界读。v16/v18/v19 那次读到 0，链立即短路（三份日志均为 `ctx+50=null +40=null +88=null`）；本轮该槽非空，`[+0x40]` 再读到垃圾，`[垃圾+0x88]` 才崩溃。这也印证 [audit23](behavior-graph-construction-audit.md#audit23) 的纠正：原生 58423 的链是 `[context]`（character）→`+0x50`（setup）→`+0x40`（data）→`+0x88`（footIkDriverInfo），插件那条少了首次解引用。**结论：这是自 v16 起潜伏在观测代码里的越界读，v20 只是改变了栈布局把它暴露出来；没有任何证据表明 63589 会崩溃，也没有证据表明它被调用过。**

**判定**：G2 仍 **NOT PASSED**。按预注册"崩溃→立即停、保留完整日志与匹配 PDB、不在同一次会话重试"，本轮不再上机、不改配方、不调参；部署 INI 已回 `off`（v20 DLL/PDB 留在原位，回滚备份 `dist/deploy-backup-20261026-v19/` 不变）。

**下一有界问题（先修诊断，不新增原生调用）**：
1. 把 g3-gates 的链起点改为 `[context]` 再 `+0x50/+0x40/+0x88`（audit23 的正确链），或直接移除这条 v16 遗留行；两者都不触碰 v20 的单一入口变量。
2. 复查插件内其它 `context + offset` 读取是否越过 `0x50`，以及同类"链首缺一次解引用"的写法。
3. 任一修复都构成新的构建身份（v20a），需重新构建/打包/部署后再取一次上机结果；在那次结果之前，本轮只证明"入口前的诊断越界"，不能用于评价 63589 的行为。

**v20a 修正与部署（2026-10-27 21:56）**：按上列第 1 项修正链起点——`[context]` → `hkbCharacter::setup`（+0x50）→ `hkbCharacterSetup::data`（+0x40）→ `hkbCharacterData::footIkDriverInfo`（+0x88），全部走声明成员，`[graph+0x100]` 改用 `hkbFootIkDriver::disableFootIk`，并在每次解引用前过新增的 [observation_chain_policy.h](../src/animation/behavior_graph/observation_chain_policy.h)（`plausible_object_pointer`，只读声明成员、不再触碰对象之外的栈槽）；行字段改为 `character/setup/data/footik-info`。三个 engine-free 用例全部通过（新增策略 11 行、guard 22 行、frame-trace 3 例，`g++` 15.1 `-Wall -Wextra -Werror`）。身份 `CharacterPanel-scenegraph-behavior-graph-v20a-chain-fix-20261027-7d18a90787-cl94faaed0c6-20261027T135614Z`（干净 `7d18a90`，DLL `691f7592…`、PDB `5b3d8cc8…`、CodeView age 321），ZIP `f4586e16…`。v20 三件套已备份到 `dist/deploy-backup-20261027-v20/`，MO2 现为 v20a DLL/PDB。**构建与部署本身不产生运行证据**；随后的一次运行见下节的 [v20a 运行记录](#g2-v20a-run)。

<a id="g2-v20a-run"></a>
## G2 v20a 运行记录（2026-10-27 22:02 会话，入口到达、pose 首次变化）

构建 `CharacterPanel-scenegraph-behavior-graph-v20a-chain-fix-20261027-7d18a90787-cl94faaed0c6-20261027T135614Z`（DLL `691f7592…`、PDB `5b3d8cc8…`），部署 INI=`inject`，运行时 `1-6-1170-0`：22:01:34 加载（`SCENEGRAPH BUILD` 与包内 manifest 逐字一致），22:02:36.590 开包，实验行 22:02:36.597–36.600，22:02:42.216 关包（`DAC restore … result=matched`）。**无崩溃、无 `[E]`**；15 条 `[W]` 均为既有 DAC/studio-light 警告。原始日志原样归档为 [CharacterPanel-g2v20a-entry-20261027-2202.log](diagnostics/CharacterPanel-g2v20a-entry-20261027-2202.log)（43,330 字节，SHA-256 `E05BFEAF9AAA49E7DE026A17D62F0050BC59B3B388D5B52BFE0E43F35349FAD0`）。

**预算与门**：`GRAPH ENTRY` 1 条、`GRAPH FRAME` 30+1、`INDEPENDENT GRAPH` 40、`BEHAVIOR GRAPH` 36（三块 12 行逐行全等）。guard `outcome=ready`；修正后的 g3-gates 链在候选上四跳全部可读：`g100=0x1d572e9c8a0 flag4a=0x00 character=0x1dab1c853c0 setup=0x1d572e9c9c0 data=0x1d5371d26f0 footik-info=0x1d5371d5b70`（character 与 `phase=driver` 的候选 char 同址，data 与候选 setup-data 同址）。

**首次走到 63589 之后的新事实**：

1. **输出头被真实填充**：step 0 `post-sample` 起 slot0 `numData=116 onFraction=1.0`，step 1 交换后 slot1 同值，step 2 起双槽均 `numData=116 f=1.0`；`tracks-end[+32=0x0074 +38=1.000]`。v7–v19 从未出现。
2. **首次 arm 与 Path B**：step 1 `post-update` 的 t 对为 `t0=-0.0166667 t1=0.175`，step 2 起 `w245=0x0 b246=0`（Path B），summary `arm-observed=yes`。t 对在 Path A 复位后还会再次 arm（step 119 为 `t0=0.05 t1=-0.2`）——因此 v19 契约里"arm 窗口实际只有 step 0"是 **v17 配方（无生成、槽头恒 0）的产物**，不是引擎限制。
3. **生成内容变化**：`gen0elem` 由 `cf37f9140cd1eb83` 变为 `cbd25238b4fbce52`，双槽元素哈希一致；step 60 起原生 `cand-anim=0x0074`（此前恒 0，只有插件 mirror）。
4. **raw pose 首次变化**：`candidate-pose-0=bc160a8c2b3a5a58`（bind 基线）→ `60=63f638a6ac08d955` → `120=8fb4cd336c4395bf`；summary `completed=120 pose-available=120 pose-missing=0 raw-hash-changes=120 first-changed-completed=1`，即**每次返回的 63589 都改变了候选 pose 原始缓冲**。
5. **源侧对照不变**：`source-pose-before=983fc2d4a169fa5b source-pose-after=983fc2d4a169fa5b`、`source-drift=none`×3、三份源基线逐行全等、`source-stable=true retained=true`；`world-restored=true ragdoll-removed=true`。
6. **状态机活着**：active 节点 97→125→111，变量集哈希逐段变化（`893b33fd…`→`214e2111…`→`a4bb41b6…`），`post-drive children-flag=1`（注入前为 0）。

**判读（预注册分支"raw pose 变化"）**：本轮证明 **v17–v19 的空结果来自入口选择**——直调 63581 跳过了 63589 的外层生成/头部回填/arm-apply，所以生成内容、槽头、Path B 与 pose 全都不动；把入口换回 `63589(mode0)`、其余不变后这些环节同时出现，源侧控制保持。这回答了 audit21/22 遗留的"入口是否必要"的运行期问题。

**但 G2 仍 NOT PASSED**：预注册明写 raw pose 变化不等于有效骨骼运动，而本包只有含 padding 的原始缓冲哈希、元素头哈希与计数/分数，没有逐骨或有限值证据，也没有视觉确认；summary 自身标 `acceptance=unjudged fingerprint=raw-buffer-padding-sensitive generator-payload=unobserved native-context=unobserved`。装载图销毁、全局收敛、G3–G6 与独立 OAR 语境同样未验收。

**下一有界问题（pose 内容，不新增原生调用）**：新增一个只读解码契约——在 step 0/60/120 对固定的一组（例如四个）具名骨从 `poseLocal` 解码 translation/quaternion，要求全部有限、至少这些骨在 60/120 步发生变化，并在源图播放同一 `mt_idle` 片段时对同址骨做对照；预算沿用本轮的 30+1。通过这一检查后才谈"有意义的原版 idle"，之后才是装载图销毁与 G3。面板内的视觉确认由观察者另行判断，不能替代数值证据。

<a id="g2-v21-contract"></a>
## G2 v21 契约：pose 内容解码（2026-10-27，已实现/已部署/未运行）

**动机**：v20a 取得首次 raw pose 变化（120/120 次返回的 63589 都改变原始缓冲），但 raw FNV 含 padding，只能证明字节变化。本契约**只增加只读解码**：不改任何原生调用、不写任何 pose 缓冲、不改 guard/眼位/步数/flags/world/事件/保留配方，也不改既有预算。

**观测**：在 `inject` 且精确 1.6.1170 的驱动里，于 step 0/60/120 三个检查点，对候选 `character.poseLocal`（声明 `hkQsTransform`、步长 0x30、count=`numPoseLocal`、上限 256）逐条解码 translation xyz / rotation xyzw / scale xyz；与上一检查点逐条比较（任一分量差 > `1e-6` 记"变化"，NaN 的 delta 记为无穷，绝不会被当成"未变"）；对源图 `poseLocal` 做同样统计；再统计候选与源在**同索引**上完全一致的条目数（别名观察）。四个 `hkVector4` 的第四个 lane 属 padding，不参与判定。纯算术落在 [pose_content_policy.h](../src/animation/behavior_graph/pose_content_policy.h)（`PoseSample`/`compare_pose`/`count_identical`），附一个 20 断言的 engine-free 用例。

**预算**：`pose-content` ≤3 行（每检查点 1 行：`entries/finite/nonfinite/moved/max-delta/source-*`）+ `pose-bone` ≤12 行（每检查点 4 根，索引 `0 / ⅓ / ⅔ / 末`，行内带引擎骨名；骨名读取先过 `plausible_object_pointer` 并截断 48 字符），合计新增 ≤15 行，与 `GRAPH FRAME` 30+1、`INDEPENDENT GRAPH` 40、`BEHAVIOR GRAPH` 36 分账。骨名不可读或 `entries=0` 时按 `unavailable` 记录，不给结论。

**预注册判读（逐条，命中即停）**：

1. 任一检查点 `nonfinite>0` 或任一 `pose-bone` 行 `finite=false` → **pose 内容无效**：停止，不进入 G3；下一问是 audit23 的三条 poseLocal 复制路径中哪一条写入了非有限值。
2. `entries=0` 或骨名 `unreadable` → 记 `unavailable`，不给结论。
3. `source-equal>0` → 候选与源存在同索引完全一致的条目，**别名嫌疑**：停止并审计，不得当作运动证据。
4. 三点全有限、`source-equal=0`，且 step 60 与 120 的 `moved` **均 ≥16（共 116 条）**、四根抽样骨中**至少两根** `moved=yes` → **pose 内容证据成立**（有限、在变化、未别名）。这仍**不是** G2 通过：视觉确认、装载图销毁/全局收敛、G3–G6 与独立 OAR 语境仍各自需要验收。
5. 全有限、未别名，但 `moved<16` → 记"原始缓冲变化主要来自 padding/噪声"，停止；不把 v20a 的 pose 变化升级为有效骨运动，下一问转为输出选择/混合（63666/63582/58423）与场景应用（63856）。
6. 崩溃或源漂移（`source-stable=false`，或源 pose 前后哈希不等）→ 立即停止，保留完整日志与匹配 PDB，不在同一会话调参重试。

**实现与部署（2026-10-27 22:10，未上机）**：身份 `CharacterPanel-scenegraph-behavior-graph-v21-pose-content-20261027-fe7c4d15e1-cl94faaed0c6-20261027T141004Z`（干净 `fe7c4d1`；DLL `e0880651…` 1,267,200 字节、PDB `787b3005…` 35,614,720 字节、CodeView age 322）。首次编译在 `independent_graph.cpp:1080` 因 `hkArray::operator[]` 取 `int32` 触发 C4267（`/WX`），改为索引声明数组 `data()[index]` 后通过；四个 engine-free 用例全过（pose 内容 20 断言、观测链 11 行、guard 22 行、frame-trace 3 例）。包 `build/behavior-graph-v21-pose-content-20261027/test-package/`（manifest `e2873121…`），ZIP 9,928,507 字节、SHA-256 `4cfa5ac8…`，五条目重解压复核通过。v20a 三件套备份到 `dist/deploy-backup-20261027-v20a/`，MO2 现为 v21 DLL/PDB，部署 INI=`inject`（1,075 字节 `11798909…`）；**本轮不上机，等待一次运行**。详见[构建记录](build.md#v21-pose-content-2026-10-27)。



<a id="g2-v21-run"></a>
## G2 v21 运行记录（2026-10-27 22:18 会话，命中第 3 条并审计为误报）

构建 `CharacterPanel-scenegraph-behavior-graph-v21-pose-content-20261027-fe7c4d15e1-cl94faaed0c6-20261027T141004Z`（DLL `e0880651…`），部署 INI=`inject`，运行时 `1-6-1170-0`：22:16:27 加载（`SCENEGRAPH BUILD` 与包内 manifest 逐字一致），22:18:12.436 开包，实验行 22:18:12.446–.458，22:18:23.522 关包（`DAC restore … result=matched`）。**无崩溃、无 `[E]`**；15 条 `[W]` 均为既有 DAC/studio-light 警告。原始日志原样归档为 [CharacterPanel-g2v21-pose-content-20261027-2218.log](diagnostics/CharacterPanel-g2v21-pose-content-20261027-2218.log)（47,393 字节，SHA-256 `601D2B47F9E94F3D7138E040A94BD6CFEC07E84F80F41156E9E2A18F6CF06717`）。

**预算**：`GRAPH ENTRY` 1、`GRAPH FRAME` 30+1、`BEHAVIOR GRAPH` 36；`INDEPENDENT GRAPH` 前缀合计 55 = 既有 40 + 本轮新增 `pose-content` 3 与 `pose-bone` 12（分账未越界）。

**观测**（检查点 0/60/120，均为 116 条）：

| 检查点 | finite | nonfinite | moved | max-delta | source-moved | source-equal |
| --- | --- | --- | --- | --- | --- | --- |
| 0 | 116 | 0 | n/a | n/a | n/a | **8** |
| 60 | 116 | 0 | 94 | 3.430847 | 0 | **21** |
| 120 | 116 | 0 | 94 | 2.313768 | 0 | **15** |

四根抽样骨（索引 0 `NPC Root [Root]` / 38 `NPC L Hand [LHnd]` / 76 `NPC L Finger30 [LF30]` / 115 `Belly`）三点全部 `finite=true`；step 60 与 120 时 38、76 为 `moved=yes`，0、115 为 `moved=no`（两者都是该图的恒等值条目：`p=(0,0,0)`、`q=(0,0,0,1)`、`s=1`，且 `source-equal=true`）。源侧三点 `source-finite=116 source-moved=0`，`source-pose-before=source-pose-after=bd107af608b00103`、`source-stable=true retained=true`、`world-restored=true ragdoll-removed=true`。

**预注册判读（逐条，命中即停）**：

| 条 | 条件 | 本轮结果 |
| --- | --- | --- |
| 1 | 任一检查点 `nonfinite>0` 或骨行 `finite=false` | 未触发（`nonfinite=0`，12 行全有限） |
| 2 | `entries=0` 或骨名不可读 | 未触发（`entries=116`，四骨名可读） |
| 3 | `source-equal>0` → 别名嫌疑 | **命中（8/21/15）→ 停止并审计，不得当作运动证据** |
| 4 | 三点全有限、`source-equal=0`、60/120 `moved≥16`、四骨两动 | 前置不成立，未取得；其余子条件实际已满足（`moved=94`；38/76 两根动） |
| 5 | 全有限未别名但 `moved<16` | 不适用（`moved=94`） |
| 6 | 崩溃或源漂移 | 不适用（无崩溃，源哈希前后相等） |

**判定**：G2 仍 **NOT PASSED**。按预注册不在同一会话调参重试；部署 INI 于 22:20 回 `off`（v21 DLL/PDB 留在原位，回滚备份 `dist/deploy-backup-20261027-v20a/` 不变）。

**审计：第 3 条是误报，别名假设被证伪**（全部取自现有日志，不依赖新运行）：

1. **指针不同址**。本轮候选 `pose-local=0x1d973570010/116`、源 `pose-local=0x1d972f94010 pose-count=116`；v20a 为 `0x1d573d84010` 与 `0x1d573b28010`。两张图各自持有 `poseLocal`，不存在共享缓冲。
2. **跨 run 不相关**。两轮源 pose 哈希不同（`983fc2d4a169fa5b` vs `bd107af608b00103`），而候选三点哈希两轮逐位相同（`bc160a8c2b3a5a58` / `63f638a6ac08d955` / `8fb4cd336c4395bf`）。若候选读取源缓冲，其内容必然随源内容变化；事实相反，并顺带证明该生成路径对同一输入是确定性的（含 padding 的整缓冲逐位可复现）。
3. **根因是判据语义**。[`pose_content_policy.h`](../src/animation/behavior_graph/pose_content_policy.h) 的 `pose_sample_identical` 用 `pose_sample_max_delta(...) == 0.0f`，即 translation xyz / rotation xyzw / scale xyz 十个分量的**精确相等**。`source-equal` 因此统计的是“值相等”而不是“共享内存”：恒等值骨（0、115）天然计入；`moved=94` 意味着约 22 条条目在检查点之间不变，与 `source-equal≈21` 同量级——两张图播放同一片段时，恒定通道骨必然携带同一常量值。**任何同片段图都会命中第 3 条，该条无法区分“常量骨”与“别名”。**

**下一有界问题（判据修正，不新增原生调用、不改配方）**：把别名判据换成两项可判定观测——①两个 `poseLocal` 的**指针同一性**（真别名检测）；②`source-equal` 只统计“与源相等**且**相对 step 0 发生过变化”的条目（别名若成立，该计数必 >0；常量骨则恒为 0）。原始 `source-equal` 保留为统计量、不进门槛。其余入口/guard/眼位/120 步/预算不动；修正构成新身份（v22），需再一次运行，在那之前 G2 仍为 NOT PASSED。

<a id="g2-v22-contract"></a>
## G2 v22 契约：别名判据修正（2026-10-27，已实现/已部署/未运行）

**动机**：v21 唯一的缺陷在第 3 条判据。[v21 运行记录](#g2-v21-run)已用现有日志证明它把“值相等”当成“别名”：候选与源 `poseLocal` 不同址、源哈希两轮不同而候选三点哈希逐位相同、`pose_sample_identical` 又是十分量精确相等——任何两张播同一片段的图都会命中。修正不改配方，只把该判据换成两项可判定观测。

**变更（只读；不加原生调用、不写缓冲、不改行数）**：

- `pose_buffer_alias(candidate, source)`：两个 `character.poseLocal` 的**指针同一性**；任一侧为 null 不算别名。
- `watch_aliasing(current, source, baseline, buffer_alias)` → `PoseAliasWatch{compared, equal, equal_moving, equal_static, has_baseline, buffer_alias}`。`equal_moving` = 同索引与源精确相等**且**相对 step 0 基线某分量变化 >1e-6 的条目数；step 0 基线即该检查点，故该点记 `n/a`；基线长度不足时整项记 `n/a`，不跨骨比较。
- `pose-content` 行追加 `source-equal-moving` / `source-equal-static` / `alias` 三个字段；仍为每检查点 1 条、共 3 条，`pose-bone` 12 条不变，不新增预算。
- 入口 `63589(mode0)`、guard、v19 眼位、120×1/60、world 借用、slot2/3、事件、保留与源侧对照全部不动。

**预注册判读（v22，取代 v21 的第 3、4 条；其余条沿用）**：

1. 任一检查点 `nonfinite>0` 或任一 `pose-bone` 行 `finite=false` → pose 内容无效，停止，转查 audit23 的三条 poseLocal 复制路径。
2. `entries=0` 或骨名不可读 → 记 `unavailable`，不给结论。
3. `alias=yes` → 两图共用同一缓冲，**真别名**：停止并审计。
4. `source-equal-moving>0` → 存在“与源相等且在变化”的条目，**别名嫌疑**：停止并审计。
5. 三点 `finite=entries`、`alias=no`、step 60 与 120 的 `source-equal-moving=0`，且两点 `moved` **均 ≥16（共 116）**、四骨至少两动 → **pose 内容证据成立**（有限、在变化、未别名）。仍**不是** G2 通过。
6. 全有限未别名但 `moved<16` → 记“原始缓冲变化主要来自 padding/噪声”，下一问转输出选择/混合与场景应用。
7. 崩溃或源漂移 → 立即停止，保留完整日志与匹配 PDB，不在同会话重试。

`source-equal` 与 `source-equal-static` **只作对照**：同片段两图在这两项上非零是预期现象，不构成失败。

**实现与部署（2026-10-27 22:56，当轮未上机）**：身份 `CharacterPanel-scenegraph-behavior-graph-v22-alias-watch-20261027-22ef58464f-cl94faaed0c6-20261027T145636Z`（干净 `22ef584`，子模块 `94faaed0`，无 `+dirty`；DLL `5756e0e6…` 1,268,736 字节、PDB `b763d05f…` 35,631,104 字节、CodeView age 323）。四个 engine-free 用例全过（pose content 36 断言、观测链 11 行、generation entry 22 行、frame trace 3 例）。包 `build/behavior-graph-v22-alias-watch-20261027/test-package/`（manifest `c9878cc7…` 7,468 字节），ZIP 9,921,485 字节、SHA-256 `be5bd284…`，五条目重解压复核与 manifest 逐条相符。v21 三件套备份到 `dist/deploy-backup-20261027-v21/`（DLL `e0880651…`、PDB `787b3005…`、`off` INI 1,058 字节 `98a2d082…`），MO2 现为 v22 DLL/PDB，部署 INI=`inject`（1,056 字节 `e7ba0854…`）。**该轮不上机**；随后的一次运行见 [v22 运行记录](#g2-v22-run)。详见[构建记录](build.md#v22-alias-watch-2026-10-27)。

<a id="g2-v22-run"></a>
## G2 v22 运行记录（2026-10-27 23:03 会话，pose 内容证据成立）

构建 `CharacterPanel-scenegraph-behavior-graph-v22-alias-watch-20261027-22ef58464f-cl94faaed0c6-20261027T145636Z`（DLL `5756e0e6…`），部署 INI=`inject`，运行时 `1-6-1170-0`：23:01:47.893 加载（`SCENEGRAPH BUILD` 与部署身份逐字一致），23:03:47.702 开包，实验行 23:03:47.712–.724，23:03:54.845 关包（`DAC restore … result=matched`）。**无崩溃、无 `[E]`**；15 条 `[W]` 与 v20a/v21 同类（DAC/studio-light）。原始日志归档为 [CharacterPanel-g2v22-alias-watch-20261027-2303.log](diagnostics/CharacterPanel-g2v22-alias-watch-20261027-2303.log)（47,564 字节，SHA-256 `6C06C504BAC0CD66FE26BE49F62E15ABC13F192F45320BC8048B0E1361C334CD`）。

**预算**：`GRAPH ENTRY` 1、`GRAPH FRAME` 30+1、`BEHAVIOR GRAPH` 36、`INDEPENDENT GRAPH` 55（40 基础 + `pose-content` 3 + `pose-bone` 12），与 v21 逐项相同，未越界。

**观测**（检查点 0/60/120，均为 116 条）：

| 检查点 | finite | nonfinite | moved | max-delta | source-equal | **source-equal-moving** | source-equal-static | **alias** |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | 116 | 0 | n/a | n/a | 8 | n/a | n/a | no |
| 60 | 116 | 0 | 94 | 3.430847 | 21 | **0** | 21 | no |
| 120 | 116 | 0 | 94 | 2.313768 | 15 | **0** | 15 | no |

四根抽样骨：索引 0 `NPC Root [Root]`、115 `Belly` 三点恒等不动（`p=0`、`q=(0,0,0,1)`、`s=1`）；38 `NPC L Hand [LHnd]`、76 `NPC L Finger30 [LF30]` 在 step 60 与 120 均 `moved=yes` 且 `source-equal=false`。源侧 `source-finite=116 source-moved=0`、`source-pose-before=source-pose-after=d3f3eeb452316397`、`source-stable=true retained=true`、`world-restored=true ragdoll-removed=true`。

**预注册判读（v22，逐条，命中即停）**：

| 条 | 条件 | 本轮结果 |
| --- | --- | --- |
| 1 | 任一检查点 `nonfinite>0` 或骨行 `finite=false` | 未触发（三点 `nonfinite=0`，12 行全有限） |
| 2 | `entries=0` 或骨名不可读 | 未触发（`entries=116`，四骨名可读） |
| 3 | `alias=yes` → 真别名 | 未触发（三点 `no`） |
| 4 | `source-equal-moving>0` → 别名嫌疑 | 未触发（60/120 均为 0） |
| 5 | 三点有限 + `alias=no` + 60/120 `source-equal-moving=0` + `moved≥16` + 四骨两动 | **成立** |
| 6 | 全有限未别名但 `moved<16` | 不适用（`moved=94`） |
| 7 | 崩溃或源漂移 | 不适用（无崩溃，源哈希前后相等） |

**判定**：**v22 判据通过——pose 内容证据成立（有限、在变化、未别名）**。G2 仍 **NOT PASSED**：按上文「G2：原版暂停期单边推进」的原文，通过还要求“候选原版 idle **持续、有意义地**前进”，而本包只覆盖单个 120 步（2 秒模拟）批次内的 `poseLocal` 内容——没有跨批次持续性；没有语义对应证据（这些变化是否对应 `mt_idle` 的原版相位）；也没有场景应用与视觉确认（候选图仍未接到面板显示图）。装载图销毁/全局收敛同样未证。部署 INI 已回 `off`（1,061 字节 `cc0572ca…`），v22 DLL/PDB 留在原位，回滚备份 `dist/deploy-backup-20261027-v21/` 不变。

**这轮同时闭合了 v21 的误报审计**：`source-equal` 与 `source-equal-static` 逐点相等（21=21、15=15），`source-equal-moving=0`——被计入的条目**全部**是自 step 0 起未变化的常量骨。审计当时只能论证“恒定骨必然相等”，现在有了索引级计数证据：不是部分巧合，而是整个计数都由常量骨构成。

**第三份非别名证据**：源 pose 哈希三轮各不相同（`983fc2d4a169fa5b` / `bd107af608b00103` / `d3f3eeb452316397`），候选三点哈希三轮逐位相同（`bc160a8c2b3a5a58` / `63f638a6ac08d955` / `8fb4cd336c4395bf`），四骨数值同样逐位复现（例如索引 38 在 step 60 的 `q=(0.0439,0.1031,0.5867,-0.8021)`），而 step 0 的源 `p` 每轮不同（本轮 `(0.0000,-0.0439,15.6427)`，v21 为 `(0.0000,0.0663,15.9209)`）。候选内容与源内容无关，生成路径对同一输入确定性可复现。

**下一有界问题（2026-10-27 23:07 选定第 2 条）**：语义对应由 [v23](#g2-v23-contract) 预注册并实现。**更正本段原先的说法**：第 2 条并非“不新增原生调用”——它必须调用一次 `aAnimation::SamplePartialTracks` 才能取得参照采样，v23 契约已把这次 const 只读调用单列并说明理由。第 1 条（跨批次持续性）仍待后续。

<a id="g2-v23-contract"></a>
## G2 v23 契约：语义对应（2026-10-27，已实现/已部署/未运行）

**动机**：v22 证明 `poseLocal` 有限、在变化、未别名，但没有证明这些数值有语义——任何生成器写出像样的数字都能过那道门。G2 原文要求“有意义地前进”，本契约把它落成一个可判定的问题：**`poseLocal` 是不是某个活动片段在它自己时间上的采样？**

**方法（只读遍历 + 一次 const 采样调用）**：

1. 取 `candidate.characterInstance.behaviorGraph->activeNodes`（`hkArray<hkbNodeInfo>`，扫描前 256 项；运行期实测 97–125 项）。
2. 只把 `nodeClone` 的 vtable 等于 `RE::VTABLE_hkbClipGenerator[0].address()` 的节点当片段生成器；不匹配记 `no-clip-generator`，**绝不做类型转换**。
3. 读 `animationName` / `localTime` / `mode` / `playbackSpeed` / `binding`；`binding->animation` 与 `transformTrackToBoneIndices` 给出轨道→骨骼映射，越界索引记 `unmapped` 而非钳制。
4. 在 `localTime` 上调用一次 `SamplePartialTracks`，写进本函数的临时 `std::vector`，然后逐条与 `poseLocal[bone]` 比较。
5. 旋转用 `1-|dot|`（q 与 -q 同旋转，避免按分量比较的假失配），平移/缩放用分量差；ε 预注册为位置 1e-4、旋转 1e-5、缩放 1e-4。
6. 每个检查点最多 3 条 `clip-ref` 行（0/60/120 → ≤9 行），`status=unavailable` 单独一行说明原因。

**预注册判读（逐条，命中即停）**：

1. 全部检查点都是 `status=unavailable` → 记 `unavailable`，**不给结论**；下一问改为“pose 从哪来”（生成器输出/层混合），不得据此判失败。
2. 有 `status=compared` 但 `unmapped == tracks`（没有一条能映射到 `poseLocal`）→ 同样记 `unavailable`。
3. `compared>0` 且 **`agreed == compared`** → **完全对应**：`poseLocal` 就是该片段在该 `time` 上的采样；`name`/`time`/`duration` 同时给出片段身份与相位。
4. `agreed / compared >= 0.8` 且 `max-dpos <= 0.05`（模型单位）→ **主体对应**：绝大多数轨道一致，差异集中在少数骨，下一问是哪些骨被 IK/pose modifier 改写。
5. 低于第 4 条 → **不是单一片段的直接采样**：记为分层/混合或后处理结果，下一问转层分解，不得把 v22 的“在变化”升级为“有意义”。
6. 崩溃或源漂移 → 立即停止，保留完整日志与匹配 PDB，不在同会话调参重试。

**这是唯一新增原生调用的轮次**：此前各轮都保持“零新增原生调用”。`SamplePartialTracks` 是 `hkaAnimation` 的 const 虚函数，产品每帧采样显示播放器走的就是这条路径；它只写本函数的临时缓冲，不写 pose、不写图、不改剪辑时间。

**实现与部署（2026-10-27 23:14，当轮未上机）**：身份 `CharacterPanel-scenegraph-behavior-graph-v23-clip-ref-20261027-ddb58bc269-cl94faaed0c6-20261027T151451Z`（干净 `ddb58bc`，子模块 `94faaed0`，无 `+dirty`；DLL `efa90c21…` 1,277,440 字节、PDB `f7e3e2c6…` 35,696,640 字节、CodeView age 327）。新增 `clip_reference_policy.h`（旋转符号无关比较、位置/缩放分量差、聚合计数）与 19 断言 engine-free 用例；五个用例全过。两次编译修正：① `clip_reference_agrees` 触发 cl C4834（`[[nodiscard]]` 返回值被丢弃，`/WX` 下为错误），该函数本就以累积为目的，去掉该属性；② `clip->binding` 原先只判空就解引用——按 v20a 的同一规则补 `plausible_object_pointer` 过滤后才读 `animation`，二进制因此改变，身份以加固提交 `ddb58bc` 重新盖章。包 `build/behavior-graph-v23-clip-ref-20261027/test-package/`（manifest `2f143a77…` 7,373 字节），ZIP 9,928,402 字节、SHA-256 `05ac4a3e…`，五条目重解压复核与 manifest 逐条相符。v22 三件套备份到 `dist/deploy-backup-20261027-v22/`（DLL `5756e0e6…`、PDB `b763d05f…`、`off` INI 1,061 字节 `cc0572ca…`），MO2 现为 v23 DLL/PDB（与包内逐字节相同），部署 INI=`inject`（1,072 字节 `88dec6d8…`）。**该轮不上机**；随后的一次运行待记录。

## 当前采集器与操作

`BehaviorGraphProbe`只读源日志仍按每actor/request最多16条预算。另一路`IndependentGraph=off|construct|load|drive|inject`默认off、重启生效，首次暂停panel-open每进程最多一次；v17 inject的INDEPENDENT GRAPH既定预算为40行，另有复用的源分组快照。候选可以被构造/装载/激活/推进/注入，不能用源探针“只读”描述整个实验；源图保持只读。装载候选刻意保留至进程结束，每进程至多一张，不代表安全销毁已实现。

代码入口：[`ownership_probe.h`](../src/animation/behavior_graph/ownership_probe.h) / [`ownership_probe.cpp`](../src/animation/behavior_graph/ownership_probe.cpp) 的 `behavior_graph::observe_source_baseline`；[`AnimationController::request`](../src/animation/controller.cpp) 在持有源图的生命周期内调用。设置由 [`settings.cpp`](../src/animation/settings.cpp) 的 `load_settings` 读取，示例在 [`CharacterPanel.ini`](../config/CharacterPanel.ini)。无新目标和依赖。

只在现有配置合并以下片段，保留其他设置，重启游戏生效。它独立于 `DetailedAnimationTrace`，无需开启后者：

```ini
[Diagnostics]
BehaviorGraphProbe=true
```

用正常开包/换装触发现有请求，收集 `BEHAVIOR GRAPH` 日志及插件构建身份。每个 actor/request 最多一份快照、最多 16 条分组行；读取固定成员的身份/计数/标志，不扫描活动节点内容、动画表、附近 Actor 或骨树。`BehaviorGraphProbe=false` 时仅分支判断，不遍历或格式化。`role=source-baseline candidate=none construction=unverified gate1=not-passed` 是源快照类型的固定标签，表示这条记录不描述候选；它不是当前 G1 判定，也不否定另一条 INDEPENDENT GRAPH 实验流中的候选。

地址转为整数值后写日志，不保存裸指针、owning 引用或跨请求状态。现有 `SceneGraph::schedule_animation` 每递增一次 request 仅调用一次 `AnimationController::request`，由调用点保证采样预算；不会因跨会话请求号重置而被全局去重误抑制。快照依赖所读 SDK 声明，且不是线程间原子快照：请求所处暂停更新括号与源图持有期可减少生命周期风险，不能证明内部数组不变。零地址/`missing` 表示该可选对象不存在；没有采集的字段均为未验证，不能推断独立。

此 API 不调用构造、Activate/Update/Generate/Deactivate、变量写入、图事件、OAR 求值、DAC、效果或 Actor 分配。它只观察源基线，不负责新图创建与清理，也不能单独证明源无漂移。若启用后异常，保留日志/版本信息，改回 false 并重启；不再推进后续关卡。

## G1 二进制静态审计（2026-10-23）

> 以下保留 10-23 初审时点；后续加载、激活和 G1 修订通过见[当前结论](#current-status)，不能把当时“未完成”当当前状态。

零售 1.6.1170 的 `.text` 被 SteamStub 3.1 加密，审计在只读副本的解包镜像上进行（镜像 SHA-256 `843823C115DBC56B5E9740350DB49170CC4EEA013A79513B7FD066DEEC719A2B`；完整方法、地址与命令见[审计记录](behavior-graph-construction-audit.md)）。已确认的只有构造/析构的所有权事实：

- 五种 `ConstructAnimationGraph` 实现共用同一模板：`MemoryManager::Allocate(0x250, 0x10, true)` → `BShkbAnimationGraph::BShkbAnimationGraph(graph, holder, flag)` → `BSTSmartPointer` 赋值。holder 为 `this-0x38`（REFR/Actor/PlayerCharacter）或 `nullptr`（Simple/Weapon）；flag 在 Actor 上由 `race->data.flags` 第 7 位取反（CommonLib 标注 `kFlies`，语义未证），PlayerCharacter 固定为 0。
- 构造只写 vtable、引用计数、两个事件源、`hkbCharacter`（+0xC0）与 holder（+0x210）；`projectName`/`projectDBData`/`behaviorGraph`/`rootNode` 全部置空。不加载项目、不登记 manager、不挂 sink、不设置 `rootNode`。
- 删除析构 `0xbbcf30` → 真正析构 `0xbad2c0` → `MemoryManager::Deallocate(ptr, true)`，与分配参数配对；对已加载的 `behaviorGraph` 有拆除动作（槽位 7 + 释放 `generatorOutputs`/`rootNode`/fade controllers）。
- OAR 从 `graph->holder` 取 REFR，空时退回 `rootNode` 再取；候选图的 holder/rootNode 选择因此直接决定 OAR 读到谁。

当时未完成激活/更新、事件路由与共享范围，G1 未通过；后续已定位加载/激活/推进链并进行 v1–v17 定位实验，见[审计记录](behavior-graph-construction-audit.md)和[当前结论](#current-status)。OAR 语境、全局状态与装载图停用/销毁仍未验收，不能由入口定位推断通过。

## G1 源基线（2026-10-23 会话）

同日晚间会话（[r4 日志](diagnostics/display-issues-2026-10-22.md#r4-session)，构建 `5ef6f0520e`、`BehaviorGraphProbe=true`）取得第一份真实目标游戏源基线：**19/19 请求、228 行、单 actor 00000014、无 missing/异常行**。关键事实（request=1 块）：

- `group=container outer-behavior` 与 `group=character behavior` 为**同一指针**（0x29b73b0d900）——源图上外层容器 `behaviorGraph` 与 `hkbCharacter.behaviorGraph` 确认同址。
- 多个 CommonLib 声明的可选成员在真实源图上**确实为空**：`world`、`character.controllerDriver`、`raycastInterface`、`setup.animationBindingSet`、`setup.unscaledAnimationSkeleton`、`behavior.rootGeneratorClone`、`state-machine-templates`、`uniqueIDPool/jobs/allPartitionMemory`（0/0）。`root=BSFadeNode`、`project-db`、`setup.skeleton/data/mirroredSkeleton` 非空。
- 规模：`pose-count=116`、`animation-bones=116`、`bone entries=116`、`foot-ik=1`、`variable-mode=0`；`flags active=true linked=false update-active=false state-changed=true`；`critical-section` 指向模块外静态地址（仅记录，归属未解析）。

这些是当时取得的源坐标，支持所读字段在该样本中的一致性，不证明整套 SDK 布局或全部共享资源安全。此源基线本身不能通过 G1；10-24 的方案 A 修订与候选记录提供了后续有限通过证据，见[当前结论](#current-status)。

## 执行台账与后续证据

各日期行保留当时结果与待办，不能把旧“尚未运行”投影为当前状态；当前结论以[文首](#current-status)为准。

| 日期/项目 | 实际结果 | 仍缺少 |
| --- | --- | --- |
| 2026-10-23 既有修复提交 | `a9a0a20883803bd5295aff9f2ac4e7edf5474cf8`，先提交再开始本文 | 本实验不重写该提交或旧测试包 |
| G1 静态审计 | 已核对当前声明与固定 OAR 源；候选入口存在但原生协议未核实 | 无新 Actor 的创建/链接/深层所有权/停用销毁证据 |
| G1 二进制审计 | 2026-10-23 在解包后的 1.6.1170 镜像上确认五种构造实现、holder 传参与配对销毁；构造不加载项目、不登记 manager。见[审计记录](behavior-graph-construction-audit.md) | 激活/更新入口、事件路由、无新 Actor 复用、共享资源范围 |
| G1 加载链定位 | 2026-10-23 续审计：编排者 32881/32882/32893/32902 → 加载 63585 → 项目解析 63844 → 行为图装载 63845 → SetupAnimEventSinks → manager 挂接，见[审计记录](behavior-graph-construction-audit.md#项目加载与链接入口2026-10-23-续审计) | 候选 holder 复用条件、运行期隔离、激活与事件路由 |
| G1 共享语义 | 2026-10-23 三轮：BG 实例每次新建（58340/58374）、字段经 hkRefPtr 共享容器数据（57010/57011 加减引用）、`hkbSymbolIdMap` 新建、完成对象登记全局表，见[审计记录](behavior-graph-construction-audit.md#加载链共享语义细读2026-10-23-三轮) | 运行中共享数据的写时隔离（`variableValueSet` 等）仍属 G1 运行判据 |
| G1 实验契约 | 2026-10-23 设计 G1a/G1b/G1c 分级、候选 holder、调用边界、24 行观测预算与预注册判据，见[实验契约](#g1-实验契约2026-10-23-设计未实现) | — |
| G1 实验实现 | 2026-10-24 `[Experiment] IndependentGraph=off|construct|load` 与 [independent_graph.cpp](../src/animation/behavior_graph/independent_graph.cpp)；Release 构建通过并部署，见[构建记录](build.md#g1-experiment-implementation-2026-10-24) | G1b 装载运行（见下行） |
| G1a 运行与判读 | 2026-10-24 会话 `construct` 模式：空壳构造 + 引用计数配对销毁成功、源基线逐行全等、无崩溃，通过可观测判据，见[运行记录](#g1a-运行记录2026-10-24-会话) | 注册表计数 UNVERIFIED |
| G1b 运行与判读 | 2026-10-24 会话 `load` 模式：63585 以 `BSFixedString*` 第二实参调用后 0.166 ms 返回 false，装配字段全空；源基线不变、无崩溃，见[运行记录](#g1b-运行记录2026-10-24-会话)；INI 已回退 off | 第二实参真实形态与拒绝原因未定位；需补 69488/63585 错误路径审计 |
| G1b 根因审计（续审计二） | 2026-10-24 指令级确认：第二实参为 `const char*` 完整项目路径（69488 拆分 → 69487 拼接 → 69490 规范化 → 0x31b6580 注册表查表），拒绝路径 0xbb21a9 对空壳为空操作；`projectName` 由 63844 写为文件名段，见[审计记录](behavior-graph-construction-audit.md#第二实参形态与拒绝路径2026-10-24-续审计二) | 契约修订与 G1b 重实现（改传完整路径）；查表键大小写规则未核实 |
| G1b 契约修订与重实现 | 2026-10-24 `load` 分支改为 `behaviorPath + projectName` 完整路径 `const char*` + 第三实参 false（Actor/PC Unk_06 实测 false），新增 `load-args` 内容观测行；Release 构建通过并部署（身份 `…646a0f2064+dirty…20261024T051200Z`），见[构建记录](build.md#g1b-contract-revision-and-reimplementation-2026-10-24) | G1b v2 运行与判读 |
| G1b v2 运行与判读 | 2026-10-24 会话：`behavior-path='' project-name='DefaultFemale'`、`path='DefaultFemale'`，0.289 ms 被拒；源基线不变、无崩溃，见[v2 运行记录](#g1b-v2-运行记录2026-10-24-会话) | 引擎注册键的真实取值来源未定位 |
| G1b 取值与前置链审计（续审计三） | 2026-10-24 指令级确认：69489 剥扩展名（键含扩展名与目录段）、63949 自建 setup（setup 检查非前置条件）、描述符名字来自 REFR vtable 槽 114 的类型键组件查找（键 0x3284b30 为 BSS 不可静态读取），见[审计记录](behavior-graph-construction-audit.md#加载取值与前置链的续审计三2026-10-24g1b-v2-复盘) | race 三个候选字段的运行期内容与命中结果 |
| G1b v3 顺序取值实验 | 2026-10-24 `race-fields` 只读 dump + 预注册优先级顺序尝试（≤4 次、首个成功即停）；Release 构建通过并部署（身份 `…646a0f2064+dirty…20261024T054649Z`，DLL SHA-256 `A3C2213F…`），见[构建记录](build.md#g1b-v3-顺序取值实验-2026-10-24) | G1b v3 运行与判读 |
| G1b v3 运行与判读 | 2026-10-24 会话：首候选 `race-bg-model='Actors\Character\DefaultFemale.hkx'` **一次装载成功**（8.729 ms，装配字段全非空、共享资源同址、projectName 内容一致）；销毁阶段崩溃于 `Deactivate` 读空 `activeNodes`（[续审计四](behavior-graph-construction-audit.md#已装载图析构的崩溃定位2026-10-24-续审计四)）；INI 已回退 off，见[v3 运行记录](#g1b-v3-运行记录2026-10-24-会话) | 销毁判据方案评审（未激活图不可安全销毁）；激活/更新入口未审计 |
| 方案 A 评审与 v4 实现 | 2026-10-24 采纳方案 A：销毁判据重定义（空壳销毁 + 装载图销毁移至 G2 生命周期）、v4 保留流程（永不释放的持有指针，每进程 ≤1 张泄漏）、v4 预注册判据（装载复现 + 装载后源基线全等 + 保留无崩溃 → G1 PASSED），见[方案 A 修订](#2026-10-24-方案-a-修订销毁判据经-g1b-v3-崩溃后评审)；构建身份 `…20261024T065321Z`（DLL SHA-256 `37C255DA…`）已部署 | **G1b v4 运行与判读（按修订判据完成 G1）**；G2 前提：激活/更新入口审计 |
| G1b v4 运行与判读 | 2026-10-24 会话：装载复现（`accepted ms=8.797`）、装载后源基线独立 diff 全等、`retained=true` 无崩溃，同会话面板多轮开合正常，见[v4 运行记录](#g1b-v4-运行记录与-g1-判定2026-10-24-会话)；INI 已收回 off | — |
| **G1 判定** | **PASSED（2026-10-24，方案 A 修订判据）**——空壳配对销毁 + 两次装载成功（装配完整/共享同址/setup 自建）+ 装载后源基线全等 + 保留无崩溃，见[v4 记录](#g1b-v4-运行记录与-g1-判定2026-10-24-会话) | 注册表计数 UNVERIFIED；可写状态写隔离属 G2+；装载图销毁由 G2 生命周期取得 |
| **G2 当前判定** | **NOT PASSED（v20a，2026-10-27 22:02）**：入口 `63589(mode0)` 首次到达，输出头填充（双槽 `numData=116 f=1.0`、`tracks-end +32=0x0074 +38=1.0`）、step 2 起 Path B、`arm-observed=yes`、`gen0elem` 与元素哈希变化、候选 raw pose `bc160a8c2b3a5a58`→`63f638a6ac08d955`→`8fb4cd336c4395bf`（120/120 次变化），源侧 pose 不变、三份源基线全等，见[v20a记录](#g2-v20a-run) | 只差逐骨/有限值与视觉证据：需新增 pose 内容解码契约（具名骨 translation/quaternion 有限性、60/120 步变化、与源同片段对照），之后才是装载图销毁与 G3 |
| G2 v18 取证运行 | 2026-10-26 23:01 会话：加载身份与包内 manifest 一致；`GRAPH FRAME` 30+1、`INDEPENDENT GRAPH` 40、`BEHAVIOR GRAPH` 36（三块逐行全等）；无`[E]`、无崩溃；`source-stable=true retained=true`；部署 INI 已回 off | Path B 的 t 区间来源、63575 原生数量、63568 回填条件、63666/63582/58423 应用链 |
| 审计十六（机器码） | 2026-10-26：record = `BSAnimationUpdateData`（`+0x18=optionalEyePosition`，唯一写入者=REFR vtable 槽 0x3C8 `ModifyAnimationUpdateData`，Actor 0x6a3750 / PC 0x74e7a0 / REFR 空桩 0x2f71a0）；Path B 真条件改为 `+0x2A==0 && t_larger≥0 && t_smaller>-MAXFLT`；arm 块门与 63581 四道门、63568/63582 的写入分工逐条落定，见[审计十六](behavior-graph-construction-audit.md#audit16) | `[graph+0x248]` 语义、`[graph+0x160]` 回退表、63666 输出选择、63582 混合细节、58423 门值、原生帧双槽 f 初值 |
| G2 v19 实现与部署 | 2026-10-26：单变量眼位实验已实现（提交 `e17d5d1`）并部署——注入分支每步记录 `+0x18` 指向 rootNode 位置 +900 的只读 3×float；FRAME 行改 `schema=v19` 并追加 `root218/b248/eye`，summary 加 `arm-observed`；Release 身份 `…v19-eye-arm-20261026-e17d5d12f6…`（DLL `344d9abb…`），三个 engine-free 案例通过；v18 对备份在 `dist/deploy-backup-20261026-v18/`，部署 INI=`inject`（未打包），见[契约与实现](#g2-v19-contract)、[构建记录](build.md#v19-eye-arm-build) | —（运行见下行） |
| G2 v19 运行与判读 | 2026-10-26 23:46 会话：身份与构建一致；FRAME 30+1、IG 40、BG 36，无`[E]`/崩溃，`source-stable=true retained=true`；`root218=0 b248=0`、`eye` 非空、step 0 旧读槽 f=1.0，但 **`arm-observed=no`**、t 恒 `{0,-MAXFLT}`、无 Path B、pose 仍 `bc160a8c2b3a5a58`；日志归档（41,470字节，`45AAEE09…`），INI 已回 off，见[v19记录](#g2-v19-run) | **audit17**：`63864()` 返回值语义、候选 rootNode 缺失；双槽 f 初值待后续 |
| 审计十七（机器码） | 2026-10-26：臂块步长闸门 = 63864（0xbc3990，唯一调用点 0xbb2603），返回 `hkbBehaviorGraph::activeNodes(+0x98)` 上归一化动画状态量的最小值（默认 FLT_MAX）；候选上 ≤0.04583 故 `-0.5*dt` 后必过不了 `1.25*dt`；候选 rootNode 由构造器清零、审计集内无写入者，见[审计十七](behavior-graph-construction-audit.md#audit17) | 63864 字段语义、`0xacf340`/`0xac3400` 返回值、原生图 rootNode 写入者 |
| 审计十八（机器码） | 2026-10-26/27：63602 的生成/更新/复位只作用于第一实参（`r13=rcx`），引擎壳图助手 0xbacc90 与配方同构而壳图未装载 → **63602 从未在候选上生成**（修正审计十三/十四）；真正入口 0xbc3270/0xbc32d0 → 65232（门 `character.behaviorGraph`、`isActive(+0x12C)`、`numStaticNodes(+0x128)`）→ 65237，配方未调用，见[审计十八](behavior-graph-construction-audit.md#audit18) | `0xbb31b0` 上下文、sink/adapter 布局、第二实参六槽语义、候选 isActive/numStaticNodes 运行值 |
| 审计十九（机器码） | 2026-10-27：生成链 `0x53fd20 → 0xba4e30 → 63595 → 63859 → 65232`；`63595` 门 = `[graph+0x200]`（`ProjectDBData*`），按 request id 表查 `projectDBData+0xc8`；sink = **0x10 字节适配器 {vtable, buffer}**（0x1992da8/0x1989320/0x1989360，slot5 才是输出转发），63602 的 buffer 在 `[rbp+0x140]`（约 0xA0 B），见[审计十九](behavior-graph-construction-audit.md#audit19) | slot5 buffer 布局、`0x53fd20` 上层驱动、`69166` 语义、值数组格式 |
| 审计二十（机器码） | 2026-10-27：slot5（`0xc25ee0`）= `65177([ctx+8], buffer+0x38, …)` 解析名字/状态 → 项目名查 `[[adapter+8]+8]+0x28` 哈希 → `59268(buffer, float)`；当时读到的 buffer 字段（`+0x00` 项目名串、`+0x08` 注册表、`+0x38` 子对象、`+0x88` 指针，总 ≥0xA0）后由审计二十一修正；**伪造 buffer 风险不可接受，v20 暂不立项**，见[审计二十](behavior-graph-construction-audit.md#audit20) | 65177 字符串协议、59268 函数体、注册表来源、buffer 完整大小/对齐 |
| 审计二十一（机器码） | 2026-10-27：63602 生成块受第一实参 `behaviorGraph` 守卫（0xbb62ae），壳图路径上 65149 与内部 63587 **都没被调用**；`65177` 拼 `<obj+0x38>/<buffer+0x38>` tagged string，`59268` = `movss [rcx+0xF0], xmm1; ret`；63602 的 sink 是 12 槽栈子对象描述符、与 0xba4e30 的 sink 不同类。**无安全最小生成实验 → 架构决策**，见[审计二十一](behavior-graph-construction-audit.md#audit21) | 是否允许把候选登记进 manager（突破 G1 边界）；第二实参六槽语义、63584/59441 对真图的作用 |
| G3–G6完整关卡 | 未通过/未执行，前关未过不开展完整验收 | 一次WeapEquip注入不算完整G3；装载图生命周期与全局收敛仍未过 |
| G2 v20 重建与部署 | 2026-10-27：原冻结包不在本检出中，按 v18 先例以干净 `a83e522` 重建/打包/部署（DLL `13688a9d…`、ZIP `076cf6f7…`、manifest `a9ae936d…`），v19 三件套备份在 `dist/deploy-backup-20261026-v19/`，部署 INI=`inject`，见[构建记录](build.md#v20-rebuild-2026-10-27) | —（运行见下行） |
| G2 v20 运行与崩溃 | 2026-10-27 21:48 会话：身份与包内 manifest 一致，`GRAPH ENTRY outcome=ready`（guard 通过），随后在 **63589 之前**的 v16 遗留 g3-gates 只读链崩溃——`context+0x50` 越过 `sizeof(hkbContext)=0x50`，`[+0x40]` 得垃圾后 `[+0x88]` 越界；**入口从未执行**，无 FRAME/completed/pose 证据；日志与崩溃报告已归档（`2278201C…`/`4074DF23…`），INI 回 off，不在同会话重试，见[v20记录](#g2-v20-run) | 先修该越界诊断（按 audit23 正确链或移除该行），复查同类写法，再以新身份（v20a）重测 |
| G2 v20a 修正与部署 | 2026-10-27 21:56：g3-gates 改为 `[context]`→setup(+0x50)→data(+0x40)→footIkDriverInfo(+0x88) 的声明成员链，新增 `observation_chain_policy` guard 与对应 engine-free 用例（三个用例全过）；身份 `…v20a-chain-fix-20261027-7d18a90787…`（DLL `691f7592…`、age 321），v20 三件套备份到 `dist/deploy-backup-20261027-v20/`，部署 INI=`inject`，见[构建记录](build.md#v20a-chain-fix-2026-10-27) | —（运行见下行） |
| G2 v20a 运行与判读 | 2026-10-27 22:02 会话：身份一致、无崩溃无 `[E]`；ENTRY 1、FRAME 30+1、IG 40、BG 36（三块全等）；guard ready、g3-gates 四跳可读；首步回填 `numData=116 f=1.0`，step 2 起 Path B、`arm-observed=yes`，`gen0elem` 变化，raw pose `bc160a8c…`→`63f638a6…`(60)→`8fb4cd33…`(120)、120/120 次变化，源侧 `983fc2d4…` 前后不变、`source-stable=true retained=true`；日志归档（43,330 字节 `E05BFEAF…`），INI 回 off，见[v20a记录](#g2-v20a-run) | pose 内容解码契约（具名骨有限性/变化/源对照）；视觉确认另行判断 |
| G2 v21 契约与实现 | 2026-10-27：只读解码 `poseLocal`（声明 `hkQsTransform` 0x30），三点 0/60/120 全量有限性 + 逐条变化 + 源对照 + 别名计数；新增 `pose_content_policy.h` 与 20 断言 engine-free 用例；预算新增 ≤15 行；身份 `…v21-pose-content-20261027-fe7c4d15e1…`（DLL `e0880651…`、age 322），ZIP `4cfa5ac8…`，v20a 三件套备份到 `dist/deploy-backup-20261027-v20a/`，部署 INI=`inject`，见[契约](#g2-v21-contract)与[构建记录](build.md#v21-pose-content-2026-10-27) | —（待上机；判定门槛 `moved≥16`、四骨至少两动） |
| G2 v21 运行与判读 | 2026-10-27 22:18 会话：身份一致、无崩溃无 `[E]`；ENTRY 1、FRAME 30+1、IG 55（40 基础 + 15 pose 行）、BG 36；三点 `entries=116 finite=116 nonfinite=0`，`moved=94`(60)/`94`(120)、`max-delta` 3.43/2.31，抽样骨 38、76 `moved=yes`、0、115 `moved=no`，源侧 `source-moved=0`、`source-pose-before=after=bd107af6…`；**`source-equal=8/21/15` 命中第 3 条 → 停止**；审计判定为误报（候选/源 `poseLocal` 不同址；源哈希两轮不同而候选三点哈希逐位相同；`pose_sample_identical` 为十分量精确值相等）；日志归档（47,393 字节 `601D2B47…`），INI 回 off，见[v21 运行记录](#g2-v21-run) | 判据改为指针同一性 + “相等且在变化”计数（v22），一次运行后再判第 4 条 |
| G2 v22 契约与实现 | 2026-10-27 22:56：`pose_buffer_alias`（`poseLocal` 指针同一性）+ `watch_aliasing`（相等且相对 step 0 变化的条目数），`pose-content` 加 `source-equal-moving`/`source-equal-static`/`alias` 三字段，行数与预算不变；pose content 用例扩到 36 断言、四例全过；身份 `…v22-alias-watch-20261027-22ef58464f…`（DLL `5756e0e6…`、age 323），ZIP `be5bd284…`、manifest `c9878cc7…`，v21 三件套备份到 `dist/deploy-backup-20261027-v21/`，部署 INI=`inject`（1,056 字节 `e7ba0854…`），见[契约](#g2-v22-contract)与[构建记录](build.md#v22-alias-watch-2026-10-27) | —（运行见下行） |
| G2 v22 运行与判读 | 2026-10-27 23:03 会话：身份一致、无崩溃无 `[E]`；ENTRY 1、FRAME 30+1、IG 55、BG 36；三点 `entries=116 finite=116 nonfinite=0`、`alias=no`，`moved=94`(60/120)，`source-equal=21/15` 而 **`source-equal-moving=0`**、`source-equal-static=21/15`（即计数全由常量骨构成，闭合 v21 误报审计），抽样骨 38/76 两点均在动；源侧 `source-pose-before=after=d3f3eeb4…`、`source-stable=true retained=true`；**判据第 5 条成立 → pose 内容证据成立**，G2 仍 NOT PASSED（持续/语义/场景应用未证）；日志归档（47,564 字节 `6C06C504…`），INI 回 off，见[v22 运行记录](#g2-v22-run) | 下一问二选一（待评审）：跨批次持续性，或用 HKX 参照做语义对应 |
| G2 v23 契约与实现 | 2026-10-27 23:14：只读遍历 `activeNodes`（前 256 项），vtable 等于 `RE::VTABLE_hkbClipGenerator[0].address()` 才当片段生成器；`binding` 先过 `plausible_object_pointer` 再读 `animation`；读 `animationName`/`localTime`/`binding`，用 `SamplePartialTracks` 在片段自身时间采样，按 `transformTrackToBoneIndices` 与 `poseLocal` 逐条比较（旋转用 `1-|dot|`）；新增 `clip_reference_policy.h` 与 19 断言用例，`clip-ref` ≤3 行/检查点；身份 `…v23-clip-ref-20261027-ddb58bc269…`（DLL `efa90c21…`、age 327），ZIP `05ac4a3e…`、manifest `2f143a77…`，v22 三件套备份到 `dist/deploy-backup-20261027-v22/`，部署 INI=`inject`（1,072 字节 `88dec6d8…`），见[契约](#g2-v23-contract)与[构建记录](build.md#v23-clip-ref-2026-10-27) | —（待上机；门槛 `agreed=compared` 或 ≥0.8 且 `max-dpos≤0.05`） |
| G1 源基线准备 | 默认关闭采集器；Release 构建、16 项有界静态/文档检查、23 个新增相对链接及 DLL/PDB/ZIP 核验通过，见 [Build](build.md#behavior-graph-baseline-checks-2026-10-23) | 实际目标游戏日志，尚未运行 |
| G1 源基线采集 | 2026-10-23 会话 19/19 请求、228 行、无异常，见 [源基线](#g1-源基线2026-10-23-会话)与 [r4 日志](diagnostics/display-issues-2026-10-22.md#r4-session) | —（后续证据见 G1/G2 各行） |
| G1c | NOT STARTED | 按方案 A 修订跳过：`holder=nullptr` 已证充分，源 holder 别名量化移至 G4 |

<a id="next-audit"></a>
## 下一步有界验证问题

[audit23](behavior-graph-construction-audit.md#audit23) 提供输出头、三条 poseLocal 复制和场景应用的静态分工。先比较入口/输出 n/f，再比较 raw pose 与有效载荷，最后查骨数组/场景应用和源控制；f/t 只限制插值，不应在 raw pose 不变时直接认定它是唯一原因。现有 v20 没有载荷/原生分支/node 应用 trace；若继续有界静态工作，关注完整 unwind 组下的 58343/58376 活动生成器前提及 63582 混合/flags，额外诊断另行授权。

当前选择[v20受控原生入口](#g2-v20-generation-entry)，其机器码依据与manager可选性见[audit22](behavior-graph-construction-audit.md#audit22)。先取得相同v19配方改用63589(&mode0)的头部/pose/源控制证据，再决定实际generation/output/apply问题；不先登记manager或制造sink/种子。旧audit19/20方向已由后验审计更新，原记录保留。

最新实际运行为 v22（2026-10-27 23:03）：pose 内容证据成立，G2 的剩余缺口是持续性与场景应用；语义对应已由 v23 做成可判定观测并部署待测（[v23 契约](#g2-v23-contract)）。正常源帧、装载销毁与全局收敛仍待证，恢复入口或填头不会放宽验收。

## 维护约束

此文拥有实验程序和状态；修改时同步[动画](animation.md)、[OAR](oar-hooks.md)、[构建](build.md)、[维护](code-maintenance.md)、README、相关配置和Changelog。区别两条边界：`BehaviorGraphProbe`最多16条源分组、值语义、只读；`IndependentGraph`默认off、每进程一次，当前 inject（v17 配方）为40条自身前缀预算，另有复用源快照，允许的候选原生调用/字段写入仅由对应实验契约授权。两者都不写源图，不能把源探针只读描述套在整个实验上。v18 在旧40条与源快照之外另有≤31条 `GRAPH FRAME`，其运行结果见 [v18 记录](#g2-v18-run)；v19 已实现、构建、部署并运行一次（分支 6，见[契约与实现](#g2-v19-contract)与[v19 记录](#g2-v19-run)），除获批的 eye 指针外不得据此增加调用或写入。v20 只改采样入口（63589），其首轮在入口前崩溃，v20a 仅修正该越界诊断并已部署待测；两者的原生调用许可同一，见[v20 记录](#g2-v20-run)。文档核查不替代引擎所有权、姿态或生命周期证据；构建/离线结果见 [Build](build.md#v19-eye-arm-build) 与 [v20a 构建](build.md#v20a-chain-fix-2026-10-27)。
