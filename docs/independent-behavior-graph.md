# 独立行为图验证

更新：2026-10-23。当前结论：**G1 未通过，G2–G6 未开始且锁定**。本轮只完成可执行的静态所有权审计，并提供默认关闭的源图基线采集器；没有创建、链接、推进或销毁独立图。构建通过和源地址日志都不能证明隔离成立。

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

通过：完整原生装配成功，可写状态独立，获准共享资源有证据；没有源 manager/Actor 副作用；逆序销毁回到基线。失败：别名跨入可写状态、必须借用源输出、无可核实配对释放。**当前未通过；离线审计（构造/销毁/加载/共享语义）与源基线采集已完成，运行期装配/隔离与激活未证实；[G1 实验契约](#g1-实验契约2026-10-23-设计未实现)已设计、未实现、未运行。**

### G2：原版暂停期单边推进

G1 通过后，在无 OAR 的原版基线只推进候选图。使用固定记录的时间步，采集候选剪辑时钟与输出姿态、源活动节点/变量/时钟/骨骼前后差分。窗口内禁止合法装备操作混入对照；用户换装备后重新建立基线。

通过：候选原版 idle 持续前进；源保持不变。只有时钟变化或某个索引变化不够。当前未执行。

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
| G1b | G1a + 调用加载 63585（项目名取源图 `projectName` +0x1F0） | 验证候选能完成原生装配（behaviorGraph/character/setup/rootNode） | 返回空、装配字段缺失、需要写源图、注册表计数不回 |
| G1c | G1b + 以源 Actor 作为 holder 重建一次对照 | 仅在 G1b 通过且后续需要源上下文时量化别名 | G1b 未通过；出现可写状态同址 |

### 候选 holder 与调用边界

- G1a/G1b 使用插件自有的 `IAnimationGraphManagerHolder` 子类（vtable 自持；槽位 02/03 返回 false、05 执行原生工厂、06/07 返回 true、08 返回 true、0A 返回 0、0B 空）——语义对齐审计中 `Simple` 路径的默认实现，holder 字段传 `nullptr`。理由：审计证明引擎自身以 `holder=null` 构造（Simple/Weapon）且该路径不向源 Actor 注册事件 sink/通道。
- 工厂实现必须复刻审计模板：`MemoryManager::Allocate(0x250, 0x10, true)`（68115）→ 构造 `BShkbAnimationGraph`（63555，签名 `this/holder/bool`，审计所得）→ 引用计数智能指针赋值；不得自行拼装成员。释放只允许丢弃最后一个 `BSTSmartPointer` 引用，禁止手动调用删除析构或 `Deallocate`。
- 加载只调用 63585（参数按审计推断 `graph/项目名/bool`；第二实参在 63585 内先交给 69488 解析，形态未定——若 `BSFixedString*` 不成立，改用静态空对象 `0x1acbcc0` 走默认解析，与编排者 0x54ba10 的 cmov 缺省一致）；不得调用 63844/63845/63847 或任何 `hkbCharacter` 成员函数，除非先以只读日志确认。
- 禁止：Activate/Update/Generate/Deactivate、图变量写入、图事件、OAR 求值/`ActiveClip`、DAC、创建 Actor/REFR、写 `+0x1F0/+0x1F8/+0x200/+0x208/+0x218`。
- 新设置 `[Experiment] IndependentGraph=off|construct|load`，默认 off，重启生效，独立于 `DetailedAnimationTrace`/`BehaviorGraphProbe`；新模块 `src/animation/behavior_graph/independent_graph.{h,cpp}`，不新增目标/依赖。

### 观测与预算

每级最多 24 行 `INDEPENDENT GRAPH`：候选 `graph/character/setup/behaviorGraph/projectDBData/projectName/rootNode/variableValueSet/activeNodes/generatorOutputs/eventQueue` 的地址、计数与标志；构造前、构造后、销毁后的源基线各一次（复用 `BehaviorGraphProbe` 的固定字段）；全局注册表计数（单例 `0x315c950` → 数组 `+0x1F8` → 计数 `+0x30`，AE-only 审计地址）前后各一次——该探测为可选，若实现风险高可跳过，但报告必须注明未观测。只读、无指针逃逸、无线程间共享状态。

### 预注册判定（不得事后放宽）

通过（全部满足）：

1. **装配完整**：G1b 后 `behaviorGraph`、`projectDBData`、`projectName`、`rootNode`、`character.setup` 均非空。
2. **可写状态独立**：候选 `graph/character/activeNodes/variableValueSet/generatorOutputs/eventQueue` 与源**不同址**。
3. **共享资源可解释**：五个 hkRefPtr 字段与 `projectDBData`/行为数据与源**同址**（引用计数 +1 属预期，不计漂移）。
4. **源基线不变**：构造前 ≡ 构造后 ≡ 销毁后（12 行字段地址/计数/标志全等）。
5. **逆序销毁**：候选指针全部释放，注册表计数回落到构造前，无崩溃/断言。
6. **无侧效应**：源 manager 的图数量不变；OAR 活动查询与 `ActiveClip` 回调未对候选触发；无新增 Actor/REFR。

失败（任一）：可写状态同址、源基线漂移、注册表计数不回落、构造/装载返回空或崩溃、必须写源图才能完成。崩溃未出现不能替代清理证据。

### 停止与回退

任一失败/未知：保留日志与构建身份行，把设置改回 off 重启，不进入下一级，不做"调整判据后重试"。装备变化后重建源基线；F7 重捕获不得叠加构造。G1 通过前 G2–G6 保持锁定，本契约不定义推进/激活。

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
| G1 实验契约 | 2026-10-23 设计 G1a/G1b/G1c 分级、候选 holder、调用边界、24 行观测预算与预注册判据，见[实验契约](#g1-实验契约2026-10-23-设计未实现) | 实现、`[Experiment] IndependentGraph` 设置与真机运行 |
| G1 源基线准备 | 默认关闭采集器；Release 构建、16 项有界静态/文档检查、23 个新增相对链接及 DLL/PDB/ZIP 核验通过，见 [Build](build.md#behavior-graph-baseline-checks-2026-10-23) | 实际目标游戏日志，尚未运行 |
| G1 源基线采集 | 2026-10-23 会话 19/19 请求、228 行、无异常，见 [源基线](#g1-源基线2026-10-23-会话)与 [r4 日志](diagnostics/display-issues-2026-10-22.md#r4-session) | 候选构造/加载/隔离证据仍未执行 |
| G1 判定 | **NOT PASSED** | 无独立候选；只有源基线不能过关 |
| G2–G6 | **NOT STARTED / LOCKED** | 必须先逐关通过；两个行为实验均未执行 |

本轮按“不启动游戏”执行：未部署、未启动、未注入任何进程。实测本机存在 AE 1.6.1170 安装（`E:\SteamLibrary\steamapps\common\Skyrim Special Edition`）与 MO2 实例 `E:\SkyrimAE`，但二进制审计只读取 exe 副本并对其解包；同日晚间的部署会话另取得源基线（见台账），候选构造/加载证据仍缺。可复现审计记录存于 `build/g1-audit-20261023/` 及任务记录。下一步先在镜像上定位项目加载/链接入口（或用运行基准确认），取得充分证据并通过第一关评审、修订实现契约后，再谈原生构造/驱动验证；当前停点是加载/链接协议未知，而不是“能否分配”。

## 维护约束

此文拥有实验程序和状态，其他文档只链接摘要。修改采集字段时同步本文、[动画](animation.md)、[OAR](oar-hooks.md)、[构建](build.md)、[维护](code-maintenance.md)、README、配置和 Changelog；保持默认关闭、16 行预算、单 actor/request、值语义与只读边界。只运行批准的构建、静态/导航检查；本轮不新增模拟日志的单元测试，编译器检查不替代引擎所有权验证。
