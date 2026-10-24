# 独立行为图验证

更新：2026-10-24。当前结论：**G1 PASSED；G2 三轮 NOT PASSED（判据②），但驱动链已逐级打通——①字符级激活 58453 修复 pose 分配（`pose-local=…/116`，装载链确认填充 `setup.animationSkeleton`）；②探针证明 Update 逐步推进 97 个节点的运行状态（生成链活着）；③采样链完整定位（63581 采样器 + 63589 采样入口，四道门候选全过；`[graph+0x240]` 语义修正为采样变换数 0x74=116）；④v4 镜像 + 每步采样无崩溃、源全程不变——唯 `generatorOutputs` 输出元素 120 步恒定：生成产物未写入**。六轮迭代后战略收束：剩余缺口 = 生成阶段的动画选择机制（管理器驱动，无管理器候选无法复刻——v6 尝试 63587 包装器即崩于此），属功能设计范围；驱动代码已回退到无崩溃的 v5 形式并部署（INI=off）。G2 的推进判据需设计评审重定义。全局注册表计数 UNVERIFIED。

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

`IAnimationGraphManagerHolder::ConstructAnimationGraph` 已声明为虚函数（slot 05），`TESObjectREFR`、`SimpleAnimationGraphManagerHolder`、`WeaponAnimationGraphManagerHolder` 有 override。2026-10-23 的[二进制静态审计](behavior-graph-construction-audit.md)在 AE 1.6.1170 上确认：五种引擎实现共用同一模板（`MemoryManager::Allocate(0x250, 0x10, true)` → `BShkbAnimationGraph` 构造 → `BSTSmartPointer` 赋值），holder 由调用者传入（REFR/Actor 传自身，Simple/Weapon 传空），构造不写 holder/manager、不登记全局表；引用计数经删除析构成对释放。**仍未定位**项目加载、链接、激活/更新与 `rootNode` 建立，所以不能把“可构造”当作独立图可用，也不能直接调用尝试。`BSAnimationGraphManager::QueryAnimations` 的现有 relocation 包含 AE=0，不能作为 AE 1.6.1170 可调用证据。

`hkbNode` / `hkbBehaviorGraph` 的 Activate、Update、Generate、Deactivate 声明只能说明运行接口存在；未完成实例装配时禁止调用。裸 `hkbCharacter` 也不满足 OAR 从其反查 `BShkbAnimationGraph` 容器和 holder 的假设。

OAR 隔离还包括图外状态：图变量和 `IsWeaponDrawn` 可通过 Actor 读取；Random、顺序、PlayOnce 的状态可能按 Actor/ref、子包或剪辑作用域保存；`ActiveClip` 可注册图事件 sink，并沿源 manager 回调。图对象地址不同不能消除这些共享关系。

## 六道关卡

每关保留原始记录、构建身份、配置、初始条件、操作顺序、判据与失败点。前关未通过不得开始后关。遇到未知 ABI、需要写源图/创建 Actor、不能解释的别名或无配对销毁路径，立即停止该实验；保留日志后回到方案评审，不引入猜测调用或隐藏回退。

### G1：构造、链接、所有权与销毁

先确认目标游戏版本下的创建入口、分配器、链接顺序、holder/manager 容器要求、停用与销毁入口，再创建一个离屏候选。逐项记录源与候选的对象身份、运行状态归属、共享资源理由、事件接收者、引用/登记/任务数量与逆序销毁结果。源只读基线只提供之后对照的坐标。

通过：完整原生装配成功，可写状态独立，获准共享资源有证据；没有源 manager/Actor 副作用；逆序销毁回到基线。失败：别名跨入可写状态、必须借用源输出、无可核实配对释放。**PASSED（2026-10-24，方案 A 修订判据）：[G1a](#g1a-运行记录2026-10-24-会话)空壳构造 + 引用计数配对销毁 + 源基线全等；[G1b](#g1b-v4-运行记录2026-10-24-会话)装载两次成立（装配完整、共享资源同址、setup 自建、projectName 内容一致），v4 装载后源基线（候选存活）逐字全等、保留无崩溃；装载图销毁证据按修订移至 G2 生命周期（引擎不支持未激活图销毁，[续审计四](behavior-graph-construction-audit.md#已装载图析构的崩溃定位2026-10-24-续审计四)）。遗留 UNVERIFIED：全局注册表计数；可写状态的运行期写隔离属 G2+。**

### G2：原版暂停期单边推进

G1 通过后，在无 OAR 的原版基线只推进候选图。使用固定记录的时间步，采集候选剪辑时钟与输出姿态、源活动节点/变量/时钟/骨骼前后差分。窗口内禁止合法装备操作混入对照；用户换装备后重新建立基线。

通过：候选原版 idle 持续前进；源保持不变。只有时钟变化或某个索引变化不够。**当前 NOT PASSED（v2 仍未取得前进证据）：v2 判据①升级成立（字符级激活 58453 → AllocatePose 生效，`pose-local=…/116`，`setup.animationSkeleton` 确认由装载链填充）；③源不变、④无崩溃成立；但候选三级姿态哈希全等（停留在骨架绑姿态）——120 步 Update 的生成产物未进入 pose 缓冲。决定性判别（generatorOutputs 内容哈希）已预注册并实现，见[运行记录](#g2v2-drive-运行记录2026-10-24-会话)。**

**前置审计已完成（2026-10-24 [续审计五](behavior-graph-construction-audit.md#激活与推进入口g2-前提2026-10-24-续审计五)）**：激活 = 对候选单次调用引擎自己的 63586（`behaviorGraph` 非空即成立，context 由图自身字段经 58315 构造，`activeNodes` 由 Activate 自分配）；推进需在"58315 自建 context + 直调槽位 5（58329）传 timestep"与"构造 63587 的管理器更新记录"之间做设计选择。G2 契约见[契约章节](#2026-10-24-g2-契约drive-模式实验设计)。

### G3：图原生的 Equip → Idle → Unequip

正常游戏运行时先记录同一单手武器的变量写入与事件先后顺序、节点激活/退出。把等价输入送到候选图；不能手动指定动作族/HKX 来“通过”。输出事件先只收集，不派发到源 Actor。

通过：候选状态机自行完成相同顺序，并能按事件和相位与原版轨迹对应。事件丢失、阶段变量仍读源值或靠播放器强制切片均失败。当前未执行。

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

### 实现状态（2026-10-24）

`[Experiment] IndependentGraph=off|construct|load|drive` 已实现（默认 off、重启生效，独立于 `DetailedAnimationTrace`/`BehaviorGraphProbe`）：[independent_graph.cpp](../src/animation/behavior_graph/independent_graph.cpp) 在首次 `panel-open` 请求里构造一个 `holder=nullptr` 的空壳图；`load`/`drive` 模式（v3 取值 + [方案 A 修订](#2026-10-24-方案-a-修订销毁判据经-g1b-v3-崩溃后评审)保留）：先以 `race-fields` 行只读记录源 holder 种族的三个候选字段与 stringData 内容，再按预注册优先级顺序（bg-model → project-name → root-name → stringData 重建）以完整路径 `const char*` 调用 63585，最多 4 次、首个成功即停，第三实参 false；**成功后候选以永不释放的持有智能指针保留（不销毁），仅空壳走引用计数释放**。`drive` 模式（[G2 契约](#2026-10-24-g2-契约drive-模式实验设计) + [v2 修正](behavior-graph-construction-audit.md#姿态写入链与字符级激活2026-10-24-续审计六)）在装载成功后同括号内：激活前补源基线 → **字符级激活 58453**（先 58466 从 `setup.animationSkeleton` 分配 pose 缓冲、再 Activate；骨架缺失时预注册守卫 `skip=setup-skeleton-missing` 干净跳过——首轮的图级 63586 绕过了 pose 分配，是无可观测前进的根因）→ 58315 自建 context（raw 零初始化存储——`hkbGeneratorOutputListener` 无完整类型，hkbContext 不可在此侧构造/析构）→ 58329 直调 `Update` ×120 步 ×1/60 s → 三级姿态哈希（源对照 + 候选 0/60/120 步）与结构快照（含 setup 三字段、+0x12E 门标志）。每进程只跑一次，AE-only 且版本 ≥ 1.6.1170。**G1 已 PASSED（[v4 记录](#g1b-v4-运行记录与-g1-判定2026-10-24-会话)）；G2 首轮 NOT PASSED（[运行记录](#g2-drive-运行记录2026-10-24-会话)）；G2 v2 构建已部署（身份 `…20261024T075048Z`，DLL SHA-256 `99E0B38E…`），INI 已设 `drive`，未运行**。

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

构建 `646a0f2064+dirty…20261024T054649Z`（DLL SHA-256 `A3C2213F…`），部署 INI `IndependentGraph=load`，原始日志 [CharacterPanel-g1bv3-load-crash-20261024-1425.log](diagnostics/CharacterPanel-g1bv3-load-crash-20261024-1425.log) 与 [Crash Logger 报告](../../../../../Users/AmazingBuff/Documents/My%20Games/Skyrim%20Special%20Edition/SKSE/crash-2026-10-24-14-25-34.log)（会话 14:24–14:25，面板打开时进程崩溃）。`race-fields` 行全部命中：

- `sex=1 bg-model='Actors\Character\DefaultFemale.hkx' root-name='Actors\Character\Behaviors\0_Master.hkx' project-field='DefaultFemale' string-data-behavior-path='' source-project-name='DefaultFemale'`——**引擎键的来源确证**：描述符表取 `behaviorGraphs[sex].model`（完整路径含扩展名），`projectName` 即 `behaviorGraphProjectNames[sex]`（无扩展名项目名，被 63844 经 69489 剥扩展名写回）。三个字段与 [CommonLib 声明](../extern/CommonLibSSE/include/RE/T/TESRace.h)完全吻合（AE 布局对 0x1E8/0x238/0x248 偏移的读取结果语义正确）。

按契约观测（预注册判据，未放宽）：

1. **装载成功**：`phase=load-try 1/4 source=race-bg-model path='Actors\Character\DefaultFemale.hkx'` → `phase=load try=1 outcome=accepted ms=8.729`（真实装载耗时；后续候选因首个成功未触发）。
2. **装配完整（判据 1 ✓）**：`behavior=0x25df263b2c0`、`project-db=0x25df32c6200`、`project-name=0x25453c00a18`、`behavior-data=0x25db4df7510` 全部非空；候选 `projectName` 内容 = `bg-model` 路径文件名段去扩展名（`DefaultFemale`），与判据修订一致。`active-nodes=0x0`/`variable-values=0x0`——图已装载未激活（激活属 G2）。
3. **共享资源可解释（判据 3 ✓）**：`compare-loaded project-db=same project=same bindings=same behavior-data=same`——候选与源同址（引用计数 +1），与[共享语义审计](behavior-graph-construction-audit.md#加载链共享语义细读2026-10-23-三轮)预测一致；`setup=different`（63949 为候选自建）。
4. **可写状态独立（判据 2）**：候选 graph/character/setup/behavior 与源不同址；`activeNodes`/`variableValueSet` 为空（未激活），深层写隔离属 G2+ 判据。
5. **源基线（判据 4）**：崩溃前未见漂移证据；销毁后基线未能采集（崩溃），本项 UNVERIFIED。
6. **逆序销毁（判据 5 ✗ → 崩溃）**：智能指针释放 → 删除析构 0xbbcf30 → 真析构 0xbad2c0 → `behaviorGraph` 非空 → **`hkbBehaviorGraph::Deactivate`（槽位 7）读空 `activeNodes`（`[behaviorGraph+0x98]==0` → `+0x8` 计数访问越界）**。Crash Logger：`SkyrimSE.exe+0AB1FC3` AV，栈三帧（`sub_AB1FC3 ← sub_BAD324 ← dtor_BBCF4E ← independent_graph.cpp:280`）与[续审计四](behavior-graph-construction-audit.md#已装载图析构的崩溃定位2026-10-24-续审计四)静态反汇编逐帧吻合。`registry-probe=unavailable` 保持 UNVERIFIED。

判定：**G1b v3 = 装载判据成立（决定性正面证据：63585 协议在 `holder=nullptr` 空壳上完整走通），销毁判据失败**——但失败原因不是泄漏、别名或协议错误，而是引擎 `Deactivate` 不支持未激活图：**"装载后未激活直接销毁"不在引擎生命周期内**，G1 契约的销毁一环（不激活而销毁）与引擎设计冲突。G1 维持 NOT PASSED；按"停止与回退"已把部署 INI 改回 off（当前部署 DLL 为 v3 构建，`off` 下实验不触发，面板其余功能不受影响）。**方案评审结论（2026-10-24）：采纳[方案 A 修订](#2026-10-24-方案-a-修订销毁判据经-g1b-v3-崩溃后评审)，销毁判据重定义、v4 保留流程已实现并部署，待运行按修订判据完成 G1 判定。**

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

## G2 v2 drive 运行记录（2026-10-24 会话）

构建 `646a0f2064+dirty…20261024T075048Z`（DLL SHA-256 `99E0B38E…`），INI `IndependentGraph=drive`，原始日志 [CharacterPanel-g2v2-drive-20261024-1557.log](diagnostics/CharacterPanel-g2v2-drive-20261024-1557.log)（会话 15:56–15:57+）。装载照旧成功后按 v2 流程执行：

- **判据① ✓（升级）**：字符级激活 58453 一次调用——`phase=activated activated=true`：`active-nodes=0x25af2f96b10/97`、`flag=1`，且 **`pose-local=0x25af25a6010/116`（AllocatePose 生效）**、`setup-skeleton=0x25ab6f66fd0` 非空——**装载链确实填充了 `setup.animationSkeleton`**（63949 一线成立），pose 缓冲与源同构（116 变换）。
- **判据③ ✓**：`source-drift=none` ×3、两份源基线逐字全等、**源姿态哈希前后全等**（`9c39265b…`×2）。
- **判据④ ✓**：120 步无崩溃，同会话面板多轮正常（revision=10），零错误。
- **判据② ✗ 仍未取得**：候选三级姿态哈希**全等**（`bc160a8c…`×3）——pose 缓冲内容停留在 AllocatePose 填充的骨架绑姿态上，120 步零变化；`children-flag=0`（+0x12E 门未置位）。

**Update 尾部补审**：0xab1ecc..0xab1f27 只有暂存缓冲簿记（57041）与返回——**58329 全函数确认无任何 `poseLocal` 写入**；"生成 → 采样进 pose 缓冲"的采样步骤在引擎其他环节（63587 后段/管理器流），未定位。

**判定：G2 仍 NOT PASSED（判据②）**；INI 已回退 off。

**判别器 v1 结果（gen-hash 行读数失败）**：`numBytes=-898388560` 为垃圾——`generatorOutputs` 指向对象的头部**不是** CommonLib 的 TrackMasterHeader（activeragdoll 移植未按 AE 校验），哈希窗口假设作废。同轮完成 Update 的 context 读取面枚举：**58329 全函数只读 `ctx.eventQueue(+0x18)` 与图字段（activeNodes/+0x98 ×6、+0x12E 门 ×2）**——我们自建的 context 已完备；虚表槽 6（CommonLib `Unk_06`）经 58377 证实为"向活动节点派发事件"类函数，58429 只是单条 float 写入，均非输出请求。**判别器 v2 运行结果（决定性）**：`phase=probe` 三行——**activeNodes 条目数组哈希每步变化**（`33a48b2a…`→`7177d7b3…`→`34ca7983…`）：**Update 确实在逐步推进 97 个节点的运行状态，生成链活着**；变量集 word 数组变一次后冻结（652 词——idle 循环未必每帧写变量，不作数）；gen0/gen1 转储揭示它们是 `{ptr, size=1, cap}` 的一元素数组头（非 TrackMasterHeader）。姿态哈希仍三级全等。**采样器定位（[续审计六补遗](behavior-graph-construction-audit.md#姿态写入链与字符级激活2026-10-24-续审计六)）**：**63581（0xbafda0）= 采样器**——读 `generatorOutputs[0]` 一元素数组的元素、按元素内 16 位偏移取 pose 轨道、以 57148 按 `count×0x30` 拷入 GetOrAllocatePose 返回的 poseLocal（与 AllocatePose 同一填充助手）；其入口 **63589（0xbb2a40）** 的四道门（setup/animationSkeleton/behaviorGraph 非空、+0x12C 已激活）候选全部满足，但 63581 开头 `test r15w` ——**当前动画索引 `[graph+0x240]`（word）为 0 即跳过采样**；该索引由 63587 每帧从管理器更新记录写入，候选无管理器故恒 0。**G2 v4 运行结果（判别器命中索引语义）**：`src-anim=0074`——**0x0074 = 116 = 骨骼变换数**：`[graph+0x240]` 不是动画索引而是**采样变换数**（63587 写它时以 `setup.animationSkeleton` 的骨数钳制，完全吻合）；镜像正确生效（`cand-anim=0074`），63589 每步调用无崩溃，源全程不变。**但 gen 元素内容哈希 120 步恒定**（`68fb5223…`×3）——**生成阶段没有向输出元素写任何东西**，姿态仍停在骨架绑姿态。**判定：G2 仍 NOT PASSED（判据②）；INI 已回退 off**（原始日志 [CharacterPanel-g2v4-drive-20261024-1635.log](diagnostics/CharacterPanel-g2v4-drive-20261024-1635.log)）。**管理器每帧管线审计（续审计六补遗二）——顶石找到**：通知中心单例 0x2022570（vtable 0x1986740；63761 = 把图**订阅**进中心列表，上限 30，0x54df30 为元素释放回调）的**槽 5（0xbacc90）= 每帧图处理**：栈上构临时图（63555）→ **63602(临时图, 订阅图)** → 析构（63556）。**63602 尾部即每帧图管线**：65149(&characterInstance, out) 建帧记录 → **`behaviorGraph->+0x12E = 1`（每帧请求子图重激活！）** → 63587(graph, record) → 清 +0x243。**首轮~v4 的生成无产物根因 = 我们的 Update 前从未置 +0x12E，Update 内的 60079 子图激活遍历每帧被门跳过**（该门由 63132/63602 置位、60079 消费清零）。**G2 v5 运行结果**：`+0x12E=1` 生效（门通过），节点状态仍每步搅动，**但 gen 元素与姿态仍恒定**——子图激活请求不是最后一块。**G2 v6 运行结果：崩溃**（打开背包即崩，[Crash Logger 报告](diagnostics/CharacterPanel-g2v6-crash-report-20261024-1711.log)）。归因逐帧闭合：`63587` 的 post-Update 阶段 **63583** 在 BSFixedString::Set（69162）写 `[r9=0x20]` 崩溃——63583 把动画名注册进一张由 `projectDBData->+0xD0` 派生、按动画索引索引的全局表，**该阶段依赖管理器侧的动画选择上下文，候选不具备**（63602 的记录里相应字段在引擎路径下由管理器状态填充）。**结论：63587 包装器路径拖进了无法在无管理器候选上复刻的名字注册阶段—— 包装器路线到此为止，驱动回退到 v5 形式（裸 58329 + 变换数镜像 + 63589，无崩溃）**。构建 `…20261024T091522Z`（DLL SHA-256 `3733ACFE…`）已部署，INI 已收 off。**战略结论**：v1–v6 六轮迭代把驱动链的每一段都指令级测绘完毕（激活/分配/Update/采样/入口/记录/包装器），剩余缺口收敛为单点：**生成阶段的动画选择机制由管理器驱动（记录携带动画身份、63583 注册名字），无管理器候选无法复刻**——这属于功能设计范围（面板预览本来就要自己选择动画），不再属于"可行性"问题。G2 的"推进"判据需要按此重新定义或与功能设计合并推进。

## 当前采集器与操作

代码入口：[`ownership_probe.h`](../src/animation/behavior_graph/ownership_probe.h) / [`ownership_probe.cpp`](../src/animation/behavior_graph/ownership_probe.cpp) 的 `behavior_graph::observe_source_baseline`；[`AnimationController::request`](../src/animation/controller.cpp) 在持有源图的生命周期内调用。设置由 [`settings.cpp`](../src/animation/settings.cpp) 的 `load_settings` 读取，示例在 [`CharacterPanel.ini`](../config/CharacterPanel.ini)。无新目标和依赖。

只在现有配置合并以下片段，保留其他设置，重启游戏生效。它独立于 `DetailedAnimationTrace`，无需开启后者：

```ini
[Diagnostics]
BehaviorGraphProbe=true
```

用正常开包/换装触发现有请求，收集 `BEHAVIOR GRAPH` 日志及插件构建身份。每个 actor/request 最多一份快照、最多 16 条分组行；读取固定成员的身份/计数/标志，不扫描活动节点内容、动画表、附近 Actor 或骨树。`BehaviorGraphProbe=false` 时仅分支判断，不遍历或格式化。`role=source-baseline candidate=none construction=unverified gate1=not-passed` 明确表示候选不存在。

地址转为整数值后写日志，不保存裸指针、owning 引用或跨请求状态。现有 `SceneGraph::schedule_animation` 每递增一次 request 仅调用一次 `AnimationController::request`，由调用点保证采样预算；不会因跨会话请求号重置而被全局去重误抑制。快照依赖所读 SDK 声明，且不是线程间原子快照：请求所处暂停更新括号与源图持有期可减少生命周期风险，不能证明内部数组不变。零地址/`missing` 表示该可选对象不存在；没有采集的字段均为未验证，不能推断独立。

此 API 不调用构造、Activate/Update/Generate/Deactivate、变量写入、图事件、OAR 求值、DAC、效果或 Actor 分配。它只观察源基线，不负责新图创建与清理，也不能单独证明源无漂移。若启用后异常，保留日志/版本信息，改回 false 并重启；不再推进后续关卡。

## G1 二进制静态审计（2026-10-23）

零售 1.6.1170 的 `.text` 被 SteamStub 3.1 加密，审计在只读副本的解包镜像上进行（镜像 SHA-256 `843823C115DBC56B5E9740350DB49170CC4EEA013A79513B7FD066DEEC719A2B`；完整方法、地址与命令见[审计记录](behavior-graph-construction-audit.md)）。已确认的只有构造/析构的所有权事实：

- 五种 `ConstructAnimationGraph` 实现共用同一模板：`MemoryManager::Allocate(0x250, 0x10, true)` → `BShkbAnimationGraph::BShkbAnimationGraph(graph, holder, flag)` → `BSTSmartPointer` 赋值。holder 为 `this-0x38`（REFR/Actor/PlayerCharacter）或 `nullptr`（Simple/Weapon）；flag 在 Actor 上由 `race->data.flags` 第 7 位取反（CommonLib 标注 `kFlies`，语义未证），PlayerCharacter 固定为 0。
- 构造只写 vtable、引用计数、两个事件源、`hkbCharacter`（+0xC0）与 holder（+0x210）；`projectName`/`projectDBData`/`behaviorGraph`/`rootNode` 全部置空。不加载项目、不登记 manager、不挂 sink、不设置 `rootNode`。
- 删除析构 `0xbbcf30` → 真正析构 `0xbad2c0` → `MemoryManager::Deallocate(ptr, true)`，与分配参数配对；对已加载的 `behaviorGraph` 有拆除动作（槽位 7 + 释放 `generatorOutputs`/`rootNode`/fade controllers）。
- OAR 从 `graph->holder` 取 REFR，空时退回 `rootNode` 再取；候选图的 holder/rootNode 选择因此直接决定 OAR 读到谁。

仍未完成：激活/更新入口、事件路由、"无新 Actor 复用"与共享资源范围；项目加载/链接入口已在续审计中定位（见[审计记录](behavior-graph-construction-audit.md#项目加载与链接入口2026-10-23-续审计)）。G1 判定保持 **NOT PASSED**；下一步是评估该链路在候选 holder 上的复用条件并取得运行证据，而不是直接调用。

## G1 源基线（2026-10-23 会话）

同日晚间会话（[r4 日志](diagnostics/display-issues-2026-10-22.md#r4-session)，构建 `5ef6f0520e`、`BehaviorGraphProbe=true`）取得第一份真实目标游戏源基线：**19/19 请求、228 行、单 actor 00000014、无 missing/异常行**。关键事实（request=1 块）：

- `group=container outer-behavior` 与 `group=character behavior` 为**同一指针**（0x29b73b0d900）——源图上外层容器 `behaviorGraph` 与 `hkbCharacter.behaviorGraph` 确认同址。
- 多个 CommonLib 声明的可选成员在真实源图上**确实为空**：`world`、`character.controllerDriver`、`raycastInterface`、`setup.animationBindingSet`、`setup.unscaledAnimationSkeleton`、`behavior.rootGeneratorClone`、`state-machine-templates`、`uniqueIDPool/jobs/allPartitionMemory`（0/0）。`root=BSFadeNode`、`project-db`、`setup.skeleton/data/mirroredSkeleton` 非空。
- 规模：`pose-count=116`、`animation-bones=116`、`bone entries=116`、`foot-ik=1`、`variable-mode=0`；`flags active=true linked=false update-active=false state-changed=true`；`critical-section` 指向模块外静态地址（仅记录，归属未解析）。

这些是未来候选图必须逐项对照的**源坐标**，同时证实二进制审计的声明来源可靠（声明为可选的字段在真实图上可为空）。它仍只描述源图：候选构造与隔离证据仍缺，G1 维持 NOT PASSED。

## 执行台账与后续证据

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
| **G2** | **UNLOCKED（G1 已过）**；四轮 drive 迭代：①v1 pose 缓冲缺失（根因=图级 63586 绕过分配，[续审计六](behavior-graph-construction-audit.md#姿态写入链与字符级激活2026-10-24-续审计六)）→ v2 字符级激活 58453 修复（`pose-local=…/116`、`setup.animationSkeleton` 确认非空）；②v3 探针证明 **Update 逐步推进 97 节点状态**（生成链活着）；③采样链完整定位（63581 采样器 + 63589 入口；`[graph+0x240]` 语义修正为采样变换数 0x74=116）；④v4 镜像+每步采样：无崩溃、源全程不变，**但输出元素 120 步恒定——生成产物未写入**，见[v2 运行记录](#g2v2-drive-运行记录2026-10-24-会话)与[v4 结果](#g2v2-drive-运行记录2026-10-24-会话)；INI 已回退 off | 管理器每帧管线审计：①事件处理 63602/0xba4800（+0x12E 写入者）→ ②63587 pre-Update（63761/77924，输出缓冲绑定？）→ 补齐后重跑 |
| G2–G6 其余 | NOT STARTED / LOCKED | 逐关推进 |
| G1 源基线准备 | 默认关闭采集器；Release 构建、16 项有界静态/文档检查、23 个新增相对链接及 DLL/PDB/ZIP 核验通过，见 [Build](build.md#behavior-graph-baseline-checks-2026-10-23) | 实际目标游戏日志，尚未运行 |
| G1 源基线采集 | 2026-10-23 会话 19/19 请求、228 行、无异常，见 [源基线](#g1-源基线2026-10-23-会话)与 [r4 日志](diagnostics/display-issues-2026-10-22.md#r4-session) | —（后续证据见 G1/G2 各行） |
| G1c | NOT STARTED | 按方案 A 修订跳过：`holder=nullptr` 已证充分，源 holder 别名量化移至 G4 |

**下一阶段入口（供新会话）**：当前停点与全部证据见文首结论与[G2 运行记录](#g2v2-drive-运行记录2026-10-24-会话)；驱动链六轮迭代的指令级测绘在[续审计六及其补遗](behavior-graph-construction-audit.md#姿态写入链与字符级激活2026-10-24-续审计六)。下一个工作包 = **动画选择机制设计**（功能设计：面板预览自行选择动画——源图的 `[graph+0x240]` 采样变换数、共享绑定集与插件既有的动画索引知识是起点），其后 G2 判据随之重定义。环境：AE 1.6.1170（`E:\SteamLibrary\steamapps\common\Skyrim Special Edition`）、MO2 实例 `E:\SkyrimAE`（部署点 `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\`）；可复现审计记录在 `build/g1-audit-20261023/`（解包镜像 + 脚本 + listings）。

## 维护约束

此文拥有实验程序和状态，其他文档只链接摘要。修改采集字段时同步本文、[动画](animation.md)、[OAR](oar-hooks.md)、[构建](build.md)、[维护](code-maintenance.md)、README、配置和 Changelog；保持默认关闭、16 行预算、单 actor/request、值语义与只读边界。只运行批准的构建、静态/导航检查；本轮不新增模拟日志的单元测试，编译器检查不替代引擎所有权验证。
