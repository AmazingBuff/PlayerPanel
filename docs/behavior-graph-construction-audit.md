# 行为图构造与销毁的二进制审计（2026-10-23）

更新：2026-10-23（含续审计）。对零售 SkyrimSE 1.6.1170 的**已解包副本**做静态审计，核实 `ConstructAnimationGraph` 的分配、构造、holder 关系与配对销毁，并定位其后的项目加载/链接链路。结论：**构造/所有权/销毁配对与项目加载/链接链路均已取得二进制证据；激活/更新、事件路由与"无新 Actor 复用"仍未证实，G1 仍未通过。**

本文只记录已读出的指令事实和可复现步骤，不把地址相等、函数可反汇编或构造成功当作独立图可用。G1 的通过/停止判据仍由[六关验证](independent-behavior-graph.md#六道关卡)拥有。

## 目标与边界

- 目标：`E:\SteamLibrary\steamapps\common\Skyrim Special Edition\SkyrimSE.exe`，FileVersion 1.6.1170.0，37,157,144 字节，SHA-256 `C434208894F07F604B852F29B8EDC3A58C4DE63DE783373733E72B2B73F33BE9`。
- 目标文件为 SteamStub 3.1 x64 打包：入口点位于 `.bind`，整个 `.text` 熵约 7.997。直接反汇编得到的是密文，不能作为证据。
- 审计使用**副本**解包后的镜像：`SkyrimSE.original.exe.unpacked.exe`，36,950,016 字节，SHA-256 `843823C115DBC56B5E9740350DB49170CC4EEA013A79513B7FD066DEEC719A2B`；入口点回到 `.text`（`0x153bc64`），`.rdata` 与打包文件一致。原始 exe 未被修改。
- 地址库：CommonLibSSE-NG 的 `tests/REL/versionlib-1-6-1170-0.bin`（format 2，version 1.6.1170.0，428,973 条记录、428,461 个唯一 ID，SHA-256 `C4093C569A3C83B26587F4B9EA4C55DE9AE6E73B84A2AF9FB3FBD30E2FE0D452`），与目标文件逐项对齐。
- 本审计不运行游戏、不注入进程、不修改原始游戏文件，也不加入任何运行时代码。

## 方法与校验

1. 用 Steamless 3.1.0.5（`Steamless.CLI.exe`，官方 release zip，SHA-256 `E3E2D22E098FF3FB359B2876AA2BED9596F0501E6FF588CBFFAE90A76D2DC4F5`）对 exe 副本解包；日志确认 `SteamStub Variant 3.1 (x64)`、`.bind` 移除、`.text` 解密。工具与解包镜像只在本地 `build/g1-audit-20261023/`，不随仓库提交。
2. 解析 versionlib 得到 `id → RVA`，再用 PE 段表映射到文件偏移；读取 `.rdata` 的 vtable 与 RTTI（Complete Object Locator 的类名和子对象偏移），并对每个 vtable 槽位检查其目标是否是 `.pdata` 的函数起点。
3. 反汇编使用 capstone 5.0.7；函数边界取自 `.pdata` 的 `RUNTIME_FUNCTION`。每条指令的 RIP 相对数据引用与直接调用目标都以 RVA 标注，调用目标再反查地址库 ID。CommonLibSSE 头文件中的 `RELOCATION_ID` 与函数名仅用作**声明来源的标签**，不作为行为证据。
4. 校验样例：`VTABLE_TESObjectREFR` 的四个 vtable 分别解析为 `.?AVTESObjectREFR@@`、子对象偏移 `0x0/0x20/0x30/0x38`；`VTABLE_hkbCharacter` 解析为 `.?AVhkbCharacter@@`；`VTABLE_BShkbAnimationGraph` 解析为 `.?AVBShkbAnimationGraph@@`，槽位 0–13 全部落在 `.text` 且是 `.pdata` 函数起点（槽位 14 起为相邻数据）。这些一致性与地址库版本共同支持“RVA 对齐正确”的结论。

## vtable 与槽位

`IAnimationGraphManagerHolder` 的 19 个虚函数中，`GetAnimationGraphManagerImpl`、`SetAnimationGraphManagerImpl`、`ConstructAnimationGraph` 为纯虚（槽位 02/03/05）。在 `VTABLE_IAnimationGraphManagerHolder` 中这三槽指向同一个 `_purecall` 目标，而槽位 01/04/06/07 指向默认实现，验证了**槽位 05 就是 ConstructAnimationGraph**，且 MSVC 的虚析构只占槽位 0（不是两个）。

| 名称（CommonLib 声明） | AE 地址库 ID | vtable RVA | RTTI 类 / 子对象 | 槽位 05 目标 |
| --- | --- | --- | --- | --- |
| `IAnimationGraphManagerHolder` | 205174 | `0x1871160` | `.?AVIAnimationGraphManagerHolder@@` / 0 | `_purecall` |
| `SimpleAnimationGraphManagerHolder` | 205182 | `0x18713c0` | `.?AVSimpleAnimationGraphManagerHolder@@` / 0 | `0x54e8f0` |
| `WeaponAnimationGraphManagerHolder` | 205189 | `0x1871678` | `.?AVWeaponAnimationGraphManagerHolder@@` / 0 | `0x54eea0` |
| `TESObjectREFR` 第 4 个 vtable | 190265 | `0x17ae6c8` | `.?AVTESObjectREFR@@` / 0x38 | `0x2f6f70` |
| `Actor` 第 4 个 vtable | 207517 | `0x189e1e8` | `.?AVActor@@` / 0x38 | `0x69d520` |
| `PlayerCharacter` 第 4 个 vtable | 208046 | `0x18ac370` | `.?AVPlayerCharacter@@` / 0x38 | `0x74e1f0` |
| `BShkbAnimationGraph` | 232870 | `0x19881f8` | `.?AVBShkbAnimationGraph@@` / 0 | `0xbbcf30`（删除析构） |
| `BSAnimationGraphManager` | 232742 | `0x1985c18` | `.?AVBSAnimationGraphManager@@` / 0 | `0xba7200`（删除析构） |

`BSAnimationGraphManager::QueryAnimations` 的两个 `RELOCATION_ID(62432, 0)`/`(62431, 0)` 在 AE 为 0，本次审计没有把它当作可调用入口，也没有执行。

## 构造路径

`ConstructAnimationGraph` 的每个实现都是**同一台机器码模板**（`Actor` 只多出第三实参的取位计算，`PlayerCharacter` 把该实参固定为 0）：

```
lea  rcx, [MemoryManager singleton]      ; 0x35f11e0
call MemoryManager::Allocate(rcx, 0x250, 0x10, true)
test rax, rax
je   <fail>
lea  rdx, <holder>                       ; 见下表
mov  r8d, <flag>
mov  rcx, rax
call BShkbAnimationGraph::BShkbAnimationGraph  ; 0xbad070
<BSTSmartPointer<BShkbAnimationGraph> 赋值：inc new/dec old，必要时调用旧对象槽位 0>
return *a_out != nullptr
```

`0x250` 与 `BShkbAnimationGraph` 的 `sizeof` 一致（[头文件](../extern/CommonLibSSE/include/RE/B/BShkbAnimationGraph.h) `static_assert(sizeof(BShkbAnimationGraph) == 0x250)`），`MemoryManager::Allocate` 的 AE ID 为 68115（`0xcc40c0`；[MemoryManager.h](../extern/CommonLibSSE/include/RE/M/MemoryManager.h)）。每个实现前还有一段以 `.data` 状态字 `0x35f11d0 == 2` 为条件的一次性初始化调用（`0xcc4680`，ID 68121，语义未解析）；它不接触图对象。

五个实现与调用者：

| 实现 | RVA | 大小 | holder 实参 | 第三实参 |
| --- | --- | --- | --- | --- |
| `TESObjectREFR::ConstructAnimationGraph` | `0x2f6f70` | 0xAE | `this - 0x38`（REFR 本身） | 0 |
| `Actor::ConstructAnimationGraph` | `0x69d520` | 0xC4 | `this - 0x38`（Actor 本身） | `!(*(uint32*)(race + 0x108) & 0x80)` |
| `PlayerCharacter::ConstructAnimationGraph` | `0x74e1f0` | 0xAE | `this - 0x38`（玩家本身） | 0 |
| `SimpleAnimationGraphManagerHolder::ConstructAnimationGraph` | `0x54e8f0` | 0x9F | `nullptr` | 0 |
| `WeaponAnimationGraphManagerHolder::ConstructAnimationGraph` | `0x54eea0` | 0x9F | `nullptr` | 0 |

`Actor` 的第三实参推导：`this` 是 `IAnimationGraphManagerHolder` 子对象（Actor+0x38）；`[this+0x1C0]` = Actor+0x1F8；AE 1.6.1170 的 `ACTOR_RUNTIME_DATA` 基址为 0xE8（[Actor.h](../extern/CommonLibSSE/include/RE/A/Actor.h) 的 `RUNTIME_DATA_ACCESSOR_VERSIONED_EX(..., 0xE0, 0xE8)`），故该指针是 `race`；`race+0x108` = `RACE_DATA`（0xE8）+0x20 = `flags`（[TESRace.h](../extern/CommonLibSSE/include/RE/T/TESRace.h)）。CommonLib 把该位标为 `kFlies`，**原因和运行语义未证实**，本文只记录“第 7 位取反”这一事实。`PlayerCharacter` 显式改为 0，说明它并非 Actor 实现的机械继承。

`slot 05` 的类覆盖（按 RTTI 与 vtable 反查）：

- `0x2f6f70`：`TESObjectREFR`、`Projectile`、`ArrowProjectile`、`BarrierProjectile`、`BeamProjectile`、`ConeProjectile`、`FlameProjectile`、`GrenadeProjectile`、`MissileProjectile`、`Explosion`、`Hazard`、`ChainExplosion`。
- `0x69d520`：`Actor`、`Character`。
- `0x74e1f0`：`PlayerCharacter`。
- `0x54e8f0`：`SimpleAnimationGraphManagerHolder`、`ActorMagicCaster`、`BookMenu`、`LineNode`、`MistMenu`、`ModelReferenceEffect`、`StatsNode`、`SummonPlacementEffect`。
- `0x54eea0`：`WeaponAnimationGraphManagerHolder`、`TailAnimationGraphManagerHolder`。

## 构造函数做了什么

`0xbad070`（571 字节；CommonLib 未声明构造函数，按 vtable 写入、成员初始化与六个直接调用点判定为 `BShkbAnimationGraph` 构造函数）的已读指令：

1. 写 `BShkbAnimationGraph` vtable `0x19881f8`；引用计数（+8）置 0。
2. 原位构造两个 `BSTEventSource` 基类（+0x10、+0x68）与 `hkbCharacter`（+0xC0，调用 `0xabcf20`，写 `hkbCharacter` vtable `0x1941a18`）。
3. 清零 `fadeControllers`（+0x160 起）、`unk190`、两个 `BSTSmallArray`（+0x1A8、+0x1C0），并把 `projectName`(+0x1F0)、`projectDBData`(+0x200)、`behaviorGraph`(+0x208)、`rootNode`(+0x218)、`generatorOutputs[2]`(+0x220/+0x228)、`physicsWorld`(+0x238) 全部置空。
4. `holder`(+0x210) 写入第二实参；第三实参写入 `+0x247`（CommonLib 未命名，`unk247`）。
5. `numAnimBones`(+0x240) 置 0，两个 `interpolationTimeOffsets` 写 `0xff7fffff`。

构造**没有**做：加载 `projectDBData`/`behaviorGraph`、写 `projectName`、创建或登记 `BSAnimationGraphManager`、把图加入任何全局表、注册事件 sink、设置 `rootNode`、激活/更新图。也就是说 `ConstructAnimationGraph` 返回的是一枚**带 holder 的引用计数空壳**；要驱动它，必须另外加载行为项目并完成链接，而这条协议在本次审计中**没有定位到**（详见下文“未完成的部分”）。

## 销毁路径

引用计数的释放端（`BSTSmartPointer` 赋值/析构）在计数降到 1 时执行 `call qword ptr [vtable]`、`edx = 1`：

- `0xbbcf30`（删除析构，133 字节）：先调用 `0xbad2c0`（785 字节；按删除析构的调用与成员拆除判定为真正的 `~BShkbAnimationGraph`）；`flags & 1` 时以 `r8b = 1` 调 `MemoryManager::Deallocate(ptr, true)`（AE ID 68117，`0xcc4510`）。
- 分配端的 `Allocate(0x250, 0x10, true)` 与释放端的 `Deallocate(ptr, aligned=true)` 成对，大小/对齐参数一致。`flags & 4` 的路径改为调用 ID 82484 的带 size（0x250）释放，引用计数路径不会走到。
- 真正的析构在 `behaviorGraph`(+0x208) 非空时，先构造一个栈上临时对象并调用 `hkbBehaviorGraph` vtable 槽位 7（`+0x38`），随后释放 `generatorOutputs[0/1]`、`rootNode`(+0x218)、`behaviorGraph`、`fadeControllers`（`0xbaf460` 逐个释放并清空数组）和 `hkbCharacter`。这说明“加载后的图”有对应的拆除动作，但不证明加载协议本身。

`hkbCharacter` 与 `BShkbAnimationGraph` 的地址是编译期固定偏移（+0xC0），与 OAR 的 `adjust_pointer<BShkbAnimationGraph>(character, -0xC0)` 一致（OAR 3.x 源码）；这解释了三方（graph、character、OAR）为何能按固定偏移互推。

## 对 G1 的影响

已取得二进制证据的：

1. **无需新建 Actor 也能构造**：`Simple`/`Weapon` holder 的引擎实现就以 `holder = nullptr` 构造图，说明构造 API 本身不要求 REFR/Actor；没有“创建 Actor”的副作用。
2. **构造不改写 holder/manager**：五个实现只分配、构造、给 `a_out` 赋值；不触碰 holder 的 `animationGraphManager`，不写全局管理器。
3. **配对销毁成立**：引用计数 → 删除析构 → 真正析构 → 对齐释放，参数与分配端一致；析构对已加载的 `behaviorGraph` 有拆除路径。
4. **holder 是可选上下文**：REFR/Actor/PlayerCharacter 传自身，Simple/Weapon 传空。OAR 从 `graph->holder` 取 REFR，为空时退回 `rootNode` 再取 REFR（OAR `ActiveClip.cpp`），所以空 holder 的图仍可能被 OAR 求值，但取决于 `rootNode` 的归属。

仍未证实、因此 G1 不能通过的：

1. ~~项目加载~~ → 已定位（见下节"项目加载与链接入口"），但**复用到无新 Actor 的候选图**仍是运行问题。
2. **链接与激活**：`hkbBehaviorGraph` 的 Activate/Update/Generate/Deactivate 入口、`hkbCharacter` setup/绑定集的建立、事件 sink 的注册，均未证明可在“无新 Actor”的条件下完成。
3. **候选 holder 的选择**：若传源 Actor，OAR/IED 条件会读到源角色（别名风险）；若传空，`rootNode` 仍是空的，OAR 的兜底会失效；若给复制 `rootNode` 一个 userData 指向源 REFR，等于把源身份带进候选图。三种选择都需要运行证据。
4. **共享资源**：`.text` 层只能证明分配/释放配对；行为项目、`hkbCharacterSetup`、绑定集、`hkbBehaviorGraph` 是共享还是深拷贝，仍需逐项核对。

## 项目加载与链接入口（2026-10-23 续审计）

续审计定位到构造之后的加载/链接协议（同一解包镜像，方法相同）。编排者与加载函数：

- **编排者**：`0x54ba10`（地址库 id 32893），与 `0x54b390`（32881）、`0x54b460`（32882）、`0x54c4d0`（32902）同属 holder 辅助簇（Simple 的 Construct 实现在 0x54e8f0）。它遍历一张行为图描述符表（第二实参 `r15`：`+0x18` 条数、`+0x8` 数组基址、每项 `0x118` 字节、`+0x108` 可选项目名指针），对每项执行：
  1. `call qword ptr [holderVtbl + 0x28]` —— **虚 slot 5 ConstructAnimationGraph** 得到空壳图（rcx=holder、rdx=&out）；
  2. `call [vtbl+0x30]`（Unk_06）取 bool 作为第三实参；
  3. 项目名 = 表项 `+0x108`，缺省指向 `.rdata` 静态对象 `0x1acbcc0`（值为 0）；
  4. `call 0xbb0800`（**id 63585**，通用 `BShkbAnimationGraph` 加载函数，6,752 字节）；
  5. `call [vtbl+0x38]`（Unk_07）、`call [vtbl+0x40]`（**SetupAnimEventSinks**，rdx=新图）；失败走 `[vtbl+0x48]`（Unk_09）。
  循环结束后 `MemoryManager::Allocate(0xB8)` 分配 `BSAnimationGraphManager`（AE 尺寸），调 `0xba3690`（id 63354）构造，再 `CreateAnimationChannels`（slot 0x50）、`0xba40f0`（63360）、`PostCreateAnimationGraphManager`（slot 0x58）、`0x54c290`（32900）、`0xba3a50`（63356）；图与 manager 的挂接落在 63360/32900/63356 之一，未逐条区分。
- **加载函数 63585（0xbb0800）** 的填写顺序（`r13`=图、第二实参=项目描述、第三实参=bool）：
  1. `call 0xbc1b10`（**id 63844**，593 字节；内部先 `call 63889` 构造解析器对象再走其虚 `+0x18`）——解析项目，实参含 `&graph->characterInstance`(+0xC0)、`&graph->unk1F8`(+0x1F8)、`&graph->projectName`(+0x1F0)；返回句柄（空则跳错误路径）。
  2. `graph->projectDBData (+0x200) = graph->unk1F8->field28->虚fn11()`（两级解引用后调 vtable 槽 11）。
  3. `call 0xba8360`（id 63455，返回 bool）校验/初始化 projectDBData。
  4. 栈上构造 `hkbCharacter`（0xabcf20），`call 0xabdbd0`（id 58464）把项目句柄绑到该 character。
  5. `call 0xbc1d70`（**id 63845**，1,126 字节）——装载并返回 **hkbBehaviorGraph\***，经智能指针赋值；
  6. 主路径 `mov [graph+0x208], rdi` 写入 `behaviorGraph`（0xbb0b36；0xbb216f/0xbb21f0 为失败/重载路径的写入）。
  `projectName`(+0x1F0)、`rootNode`(+0x218) 的具体填写点在 63585 后半段，未逐条核实；`boneNodes`/骨架绑定在 63845 内部。

**对 G1 的影响**：链接协议已定位到指令级——"构造空壳 → 63585 按项目名装载 → SetupAnimEventSinks → manager 挂接"。因此"谁加载"不再是未知数；未决问题收窄为：① 该链路在**无新 Actor 的候选 holder** 上能否复用（`holder=nullptr` 走 Simple 路径时 `unk_06/SetupAnimEventSinks` 等虚调用全部打在 Simple holder 上）；② 63844/63845 是否共享或复制行为项目（G1 共享资源判据）；③ 激活/更新/事件路由。地址库 ID：编排者 32881/32882/32893/32902，加载 63585，项目解析 63844，行为图装载 63845，校验 63455，character 绑定 58464，manager 构造 63354/63356/63360。

### 加载链共享语义细读（2026-10-23 三轮）

对 63845/63847/58340/58374 的细读回答了"共享还是复制"：

- **`hkbBehaviorGraph` 实例每次加载都新建**：63585 两条路径（主路径 `bb0b18`、失败/重载路径 `bb2151`）都调 `0xbc2390`（id 63847，仅此两个调用者）；63847 调 `0xab2770`（id 58340）从 hkMemoryRouter（`[0x174f440]→[+0x58]→vtbl[1]`）分配 **0x130 字节**（`[new+8]=0x130`，hkReferencedObject 尺寸模式），再由 `0xab4ca0`（id 58374）完成构造链——先装基类 vtable `0x1941260`、`[obj+8]=0x1ffff`，最后装 `hkbBehaviorGraph` 最终 vtable `0x1941450`。不从缓存取共享实例。
- **实例内容引用共享容器/源数据**：58340 以第二实参（为 0 时回退 `[容器+0x80]`）为初始化源，调 `0xab4ca0` 完成主体；随后五个 0x40 字节辅助（58350/58351/58352/58353/58367）把容器字段（`[容器+0xB0/0xB8/0xC0/0xC8/0xD0]`）以 **hkRefPtr 赋值**写入新 BG（以 58350 为例：`if (src) call 57010`（AddRef，0x14 字节）→ `if (old) call 57011`（Release）→ `[dst+0xB8]=src`）；另有 `[容器+0x60]` **裸指针直接复制**（无计数）。即新实例的字段指向被容器持有的同一批行为数据对象（引用计数 +1），**不是深拷贝**。
- **63845 的映射构建**：63845 循环里对描述符数组每个条目的三个 (指针,大小) 句柄对调用 64460/64459——64460 用**帧分配器**（`[0x174f440]` 单例、bump 指针 `[rax+0x18]` 推进/回退）暂存句柄数组并逐项 `& ~1` 剥离标志位，64459 新建 **`hkbSymbolIdMap`**（vtable `0x1966138`，0x30 字节，`0x1ffff` 初始化）并从句柄数组填充。映射实例新建，句柄内容引自共享数据。
- **返回值与登记**：63845 的返回值取自 `hkbCharacter+0x50 → +0x40 → +0x98` 链上槽位的 64460 调用结果；尾部把完成对象经自旋锁（`[r9+0xC]` 状态 `0x30000001/0x40000000`）登记进全局注册表（单例 `[0x315c950]`、数组 `+0x1F8`、计数 `+0x30` 自增、`call 63529`）。63847 还把性能计数字段写进 `[bg+0x78]` 指向的对象。
- **按用法判定的语义**：57010（0xa882e0，0x14 字节）= AddRef、57011（0xa883a0，0x31 字节）= Release——依据是"新值先加、旧值后减"的赋值模式与析构中的释放用法，未逐字节核对计数偏移。

**结论（静态层面）**：引擎自身加载第二个行为图时的语义正是"实例新建、行为数据引用共享"——与独立图设计想要的资源模型一致。但"共享数据在运行中是否安全（写时隔离、variableValueSet 独立性）"仍属 G1 运行判据，静态证据不能替代。

## 对设计契约的影响

- 构造 API **不是**“独立图可用”的证据；它只证明“可以分配并初始化空壳，且能按引用计数成对释放”。
- 若继续走“无第二 Actor”的路线，必须解决**无 REFR 的 holder/rootNode 语义**：要么证明空 holder 下 OAR/IED 仍能按明确规则求值，要么引入一个轻量、可销毁的 holder 代理，并证明它不会被 OAR 当作源 Actor 的权威状态。
- 在加载/激活协议被证明前，不应在插件里调用 `ConstructAnimationGraph`、`~BShkbAnimationGraph`、`Activate`/`Update`/`Generate` 或任何写 `+0x1F0/+0x200/+0x208` 的路径；也不应把本次地址写入代码作为可调用保证。

## 复现

以下步骤需要**本机已解包的镜像**；零售 exe 的 `.text` 是密文，直接反汇编无效。所有产物在 `build/g1-audit-20261023/`（被 `.gitignore` 忽略）：

```powershell
# 1. 目标身份
Get-FileHash 'E:\SteamLibrary\steamapps\common\Skyrim Special Edition\SkyrimSE.exe' -Algorithm SHA256

# 2. 只对副本解包（Steamless 3.1.0.5）
Copy-Item '<game>\SkyrimSE.exe' build/g1-audit-20261023/SkyrimSE.original.exe
build/g1-audit-20261023/tools/Steamless/Steamless.CLI.exe --recalcchecksum build/g1-audit-20261023/SkyrimSE.original.exe
# 输出：SkyrimSE.original.exe.unpacked.exe

# 3. 地址库/RVA/vtable 证认
python build/g1-audit-20261023/build_maps.py
python build/g1-audit-20261023/structure_scan.py
python build/g1-audit-20261023/vtable_slot_scan.py 0x2f6f70 0x69d520 0x74e1f0 0x54e8f0 0x54eea0

# 4. 构造/销毁/加载实现
python build/g1-audit-20261023/disasm_func.py 0x2f6f70
python build/g1-audit-20261023/disasm_func.py 0xbad070
python build/g1-audit-20261023/disasm_func.py 0xbad2c0
python build/g1-audit-20261023/disasm_func.py 0xbbcf30
python build/g1-audit-20261023/disasm_func.py 0x54ba10   # 编排者 32893
python build/g1-audit-20261023/disasm_func.py 0xbb0800   # 加载 63585
python build/g1-audit-20261023/disasm_func.py 0xbc1b10   # 项目解析 63844
python build/g1-audit-20261023/disasm_func.py 0xbc1d70   # 行为图装载 63845
```

脚本只读 PE/versionlib，不加载模块、不调用引擎；`analysis.json` 由 `build_maps.py` 从 versionlib 生成。反汇编注释中的函数名来自 CommonLibSSE 头文件的 `RELOCATION_ID` 标签，未标名者保持 ID/地址。

## 限制

- 静态审计不能替代运行：没有构造、加载、激活、绘制或销毁任何真实图，也没有验证 OAR/IED 在空 holder 下的行为。
- `.pdata` 只覆盖有展开信息的函数；叶函数可能没有条目，`_purecall` 之类的目标无法用 `.pdata` 反查（其槽位一致性由三个纯虚槽同址证明）。
- 地址库记录的是**未打包布局**的 RVA；本次已用 vtable/RTTI/`.pdata` 对齐验证，但不代表所有 ID 都经过逐条核对。
- 第三实参的 `RACE_DATA::flags` 第 7 位语义、`+0x247` 的用途、`0xcc4680` 的一次性初始化身份均未解析。
- 本次没有修改运行代码，没有新增依赖、目标或配置；`BehaviorGraphProbe` 的采集字段和 G1 采集预算不变。
