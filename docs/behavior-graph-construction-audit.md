# 行为图构造与销毁的二进制审计（2026-10-23）

更新：2026-10-23（含续审计）；2026-10-24 续审计二确定加载函数 63585 的第二实参形态与拒绝路径，续审计三确定扩展名剥离（69489）、setup 自建（63949）与描述符名字来源链，续审计四定位已装载图析构的崩溃（Deactivate 读空 activeNodes），续审计五（G2 前提）确定激活/推进入口。对零售 SkyrimSE 1.6.1170 的**已解包副本**做静态审计，核实 `ConstructAnimationGraph` 的分配、构造、holder 关系与配对销毁，并定位其后的项目加载/链接链路。结论：**构造/所有权/销毁配对、项目加载/链接链路、第二实参形态、激活入口（引擎配方 63586 + Activate 自分配 activeNodes）均已取得二进制证据；Update 推进细节与事件路由仍未逐条核实，G1 已 PASSED（修订判据），G2 设计中。**

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

## 第二实参形态与拒绝路径（2026-10-24 续审计二）

G1b 运行实测 63585 返回 false（0.166 ms、无装配、无源漂移）后，对同一解包镜像续审计，回答两个问题：**第二实参的真实形态**与**拒绝发生在哪、有什么副作用**。全部结论为指令级事实：

1. **第二实参是 `const char*`（完整项目路径），不是 `BSFixedString*`**。63585 序言把 rdx 存入 rdi（bb0837），随后原样作为第一实参传给 69488：`mov rcx, rdi; call 0xcfc5b0`（bb08a9/bb08ac）。
2. **69488（0xcfc5b0，354 B）= 路径拆分**。69492（0xcfcba0，25 B）实参为空返回 -1，非空尾落进紧随其后的扫描器（0xcfcbb9 起，.pdata 只记 7 字节）：在实参的本地副本里**从后向前**找最后一个 `\`（0x5c）或 `/`（0x2f），返回其下标或 -1。69488 按该下标把路径拆成（目录，文件名）写入两个 SSO 字符串：**0x118 字节对象，内联 260 B（0x104）缓冲在对象基址、缓冲指针 +0x108、长度 +0x110（word）、容量 +0x112（word）**——正是编排者描述符表表项（0x118 B、+0x108 "名字指针"）的同一类型。无分隔符 → 目录为空、文件名=整串；返回 bool。
3. **63844（0xbc1b10，593 B）的实参布局**：rcx=目录段、rdx=文件名段（均为 SSO 串缓冲指针，bb08bf–bb08da 取自 69488 的两个栈对象，空时用静态空串 0x1acbcc0）、r8=`&characterInstance`(+0xC0)、r9=`&unk1F8`(+0x1F8)、arg5=`&projectName`(+0x1F0)（=63844 帧内 [rbp+0x2B0]）、arg6=16 字节清零栈结构（=[rbp+0x2B8]，bc1c43 的读取与之互证）。
4. **解析与查表**：63844 构造解析器对象（63889，vtable `0x19892a8`），走槽位 3（0xbc4d30，1142 B）：69487 把实参拼为 `目录 + [目录自身不以分隔符结尾时补 '\'] + 文件名`（0xcfc4cc–0xcfc598；目录 ≤1 字符或以分隔符结尾则不补）；69490（0xcfc8c0，357 B）规范化——`/`→`\`（cfc96a）、解析 `.\` 与 `..\` 段（cfc8f8–cfc957）、**无大小写折叠**；然后对全局注册表单例 `0x31b6580` 查表。返回句柄；`句柄==0` 或 `[句柄+0x20]==0` → 63844 返回 0（bc1b9f/bc1ba9 → bc1d34）。
5. **63844 成功路径**：句柄经 hkRefPtr 存入 `character+0x60`（= graph+0x120；bc1bb7–bc1bc5，57010/57011 配对）；调 63884（&unk1F8）、63949；读 `character+0x50 → +0x40 → +0x98` 链（任一为空 → 返回 0）；69489 处理文件名段后，**69161 从文件名段构造 `BSFixedString`，69167 赋给 `graph->projectName`(+0x1F0)（bc1cd7–bc1d01）**，69164 析构临时串，63951 收尾。即 **projectName 的值 = 第二实参路径的文件名段**；63585 本体从不写 +0x1F0（全函数唯一引用是 bb08de 取地址）。⇒ 源图的 projectName 是"文件名段"（如 `0_master.hkx` 形态），**不是完整路径，不能原样回传**。
6. **拒绝路径（63844 返回 0 → 0xbb21a9）**：释放 graph+0x120（若非空，57011）并置空 → `58464(&characterInstance, 0)` 解绑 → `58285([graph+0xF0], 0)` → 释放 graph+0x208 behaviorGraph（若非空）→ [rsp+0x70] 句柄若非空续处理 → 置空两个栈上 SSO 串、对象析构、恢复线程局部守护字（gs 段变址 +0x768；序言保存旧值、写 0x3a，尾声恢复，bb0840–bb0863/bb2235–bb223e）→ 返回 r14b=0（序言 bb083d 置 0）。**对空壳图上述引用全为 null，整条路径是空操作、只返回 false——与 G1b 实测（source-drift=none、0.166 ms、无崩溃、projectName 未被写）逐项吻合。** 63455 失败的第二错误路径（0xbb2100）走 63456/59484/58460/63847 的重载清理，未逐条展开。
7. **编排者实参来源**（0x54ba10 @54bb11–54bb2c）：rdx = 表项 +0x108 缓冲指针，缺省静态空串 0x1acbcc0；**r8d = holder->Unk_06()（vtable +0x30）的返回值——第三实参随 holder 而异，不是常量**；rcx = 新构造的图。

**对 G1b 失败的解释**：实验传的是 `BSFixedString*`（对象地址）。69488 把 BSFixedString 头 8 字节（指向引用计数串数据的指针）当路径内容扫描，得到在首个 0 字节截断的无意义"文件名"、目录为空；合并、规范化后查表必然失配 → 句柄 0 → 拒绝路径返回 false。0.166 ms 与"拆分+查表即返"一致。**这是纯粹的实参形态错误，不涉及未定义行为或源图副作用。**

**修正后的调用契约（供实验契约修订）**：第二实参应为 NUL 结尾、含分隔符的**完整项目路径** `const char*`（`/` 或 `\` 皆可，规范化统一为 `\`）；成功后 projectName=文件名段。候选值来源：源图 character 的 projectData 链内项目路径串（hkbProjectStringData 一类）为首选；仅传 `源.projectName`（文件名段、无目录）时合并结果为裸文件名，能否命中注册表**未证**。69487 补分隔符所用静态（0x17ABFA8 间接对象）未逐字节解析，不影响上述修正。

**仍未决**：解析器槽位 3 的查表键比较细节（大小写规则、是否要求完整路径）未逐条核实；激活/更新/事件路由仍未定位；G1 判定不变，G2–G6 保持锁定。

## 加载取值与前置链的续审计三（2026-10-24，G1b v2 复盘）

G1b v2（以 `stringData->behaviorPath + projectName` 重建路径调用 63585）仍被拒（0.289 ms）。`load-args` 行首次记录所传内容，暴露三项事实并引出下列指令级结论：

1. **源项目的 `stringData->behaviorPath` 为空，`projectName` = `DefaultFemale`**（无扩展名、无目录）——v2 传出的合成键就是裸 `DefaultFemale`，仍查表失配。
2. **69489（0xcfc720，397 B）剥离扩展名**：对文件名段从后向前找最后一个 `.`（cfc7bd 的 0x2e 比较），只把前缀写入 `projectName`（cfc7de 的 13274 长度截断）。因此 `projectName` 是"文件名段去扩展名"，**注册表键本身仍含扩展名与目录段**——裸 `DefaultFemale` 与引擎键必然不同。
3. **63844 的 `character.setup → +0x40 → +0x98` 检查不是调用方前置条件**：紧邻其前的 63949（0xbc91e0，296 B）自行为 character 创建 setup——经 hkMemoryRouter 分配 0x58 字节、59477 构造、写入 `[&characterInstance+0x50]`（bc92c1–bc92cf 的 Release/AddRef 配对），再调 60112 初始化。即引擎自己的"先 Construct 后 63585"流程同样以 setup=0 的空壳进入 63844，检查在绑定后必然通过。**G1b v2 的拒绝因此收窄为纯粹查表失配**（若句柄命中，后续链会被 63949 自行建立）。
4. **描述符表名字的来源**：编排者 0x54ba10 的调用者之一 0x2f69f0（REFR 簇）在栈上构造 2 个 0x118 表项（68162，表项尺寸实参 0x118），再由 REFR 主 vtable（0x17ae180）槽位 114（0x2f63e0，340 B）填充：经类型键 `0x3284b30`（BSS，运行期初始化，静态不可读）二分查找（70510，0xd26490）取得组件，BSFixedString 拷贝其 `+0x18` 串，按 13274 写入表项（先置空 +0x108/+0x110 再赋值）。名字原文来自该组件，**无前缀拼接**；`TESModel::model` 在 +0x08、`BGSBehaviorGraphModel` 尺寸 0x28，`found+0x18` 与二者均不吻合，静态层面无法把该组件落到具体 TESRace 字段。CommonLibSSE 声明的 TESRace 候选字段：`behaviorGraphs[SEXES::kTotal]`(+0x1E8，BGSBehaviorGraphModel)、`rootBehaviorGraphNames[2]`(+0x238)、`behaviorGraphProjectNames[2]`(+0x248)。

**结论与下一步**：G1b v2 拒绝 = 注册表键失配（键含目录段与扩展名，而 `stringData->behaviorPath` 为空、`projectName` 已被剥扩展名，二者都无法重组出引擎键）。取值来源需运行期证据：G1b v3 在 `race-fields` 行只读记录三个 TESRace 候选字段与 stringData 的内容，并按预注册优先级顺序尝试（≤4 次、首次成功即停、失败尝试对空壳为已证实的无副作用操作）。G1 判定不变，G2–G6 保持锁定。

## 已装载图析构的崩溃定位（2026-10-24 续审计四）

G1b v3 运行（构建 `…20261024T054649Z`）：**第一个候选 `race->behaviorGraphs[sex].model` = `Actors\Character\DefaultFemale.hkx` 一次装载成功**（`outcome=accepted ms=8.729`，装配字段全非空，`project-db/project/bindings/behavior-data` 与源同址），但智能指针释放（析构链）崩溃。Crash Logger（`crash-2026-10-24-14-25-34.log`）与解包镜像反汇编把崩溃逐帧归因：

1. **崩溃指令**：`SkyrimSE.exe+0xAB1FC3`：`movsxd r15, dword ptr [rax+0x08]`，`rax=0`（读 `0x8`）。位于 0xab1f9c..0xab1fd0 叶函数，其前置指令 `mov rax, [rcx+0x98]`——**rcx = 已装载的 hkbBehaviorGraph（"0_Master.hkb"），+0x98 = `activeNodes`（NodeList\*），本次为 NULL**（与实验日志 `active-nodes=0x0` 一致），随后读 NodeList 计数（+0x8）越界。
2. **调用链**（Crash Logger 三帧，与静态审计互证）：智能指针作用域退出（independent_graph.cpp:280）→ 删除析构 0xbbcf30 → 真析构 0xbad2c0 → `behaviorGraph(+0x208)` 非空时构造 `hkbContext`（58315，以 `&characterInstance` 为源）并**调用 `hkbBehaviorGraph::Deactivate`（vtable 槽位 7 = 0xab1f70，CommonLib 声明槽 7 为 Deactivate）** → 0xab1fb4 读空 `activeNodes` → 0xab1fc3 崩溃。
3. **引擎语义**：`hkbBehaviorGraph::Deactivate` 无条件读取 `activeNodes` 的计数——引擎自身生命周期里，被销毁的图都先经过激活（`activeNodes` 在 Activate 时分配），**"装载后未激活直接销毁"不在引擎支持的生命周期内**。这不是我们的调用协议错误，而是 G1 契约"装载 → 对照 → 逆序销毁"里销毁一环的前提（不激活而销毁）与引擎设计冲突。

**对 G1 的影响**：装载判据（装配完整、共享资源同址、setup 由 63949 自建）已在运行期成立；销毁判据失败的原因不是泄漏或别名，而是"未激活图无安全销毁路径"。G1 仍 NOT PASSED；销毁证据的取得方式需要方案评审（激活后销毁属 G2 及之后的生命周期，或修订 G1 的销毁判据定义）。激活/更新入口本身仍未审计。

## 激活与推进入口（G2 前提，2026-10-24 续审计五）

G1 通过后为 G2（暂停期单边推进）做的前提审计。全部指令级事实（同一解包镜像）：

1. **`hkbContext`（0x50 B，[CommonLib 声明](../extern/CommonLibSSE/include/RE/H/hkbContext.h)）**：+0x00 `character`、+0x08 `behavior`、+0x10 `nodeToIndexMap`、+0x18 `eventQueue`、+0x20 `sharedEventQueue`、+0x28 `generatorOutputListener`、+0x30 `eventTriggeredTransition`、+0x38 `world`、+0x40 `attachmentManager`、+0x48 `animationCache`。
2. **context 构造助手 58315（0xab0a60，无 .pdata）**：`InitContext(ctx, character, world, attachmentManager, animationCache)`——只写 `ctx.character = character`、四个入参字段（world/attachmentManager/animationCache）并把 **`behavior`/nodeToIndexMap/eventQueue/sharedEventQueue/generatorOutputListener 全部置空**。因此经它构造的 context 走 Activate/Deactivate/Update 的**短路径**（三个虚函数的桩都以 `cmp [ctx+8], 0` 分流）。
3. **`hkbBehaviorGraph::Activate` = vtable `0x1941450` 槽位 4 = 58328（0xab1780，短桩 + 落体 0xab17a3..0xab187a）**。短路径（`ctx.behavior == 0`）依次：`data(+0x88)` 非空时调 58346（0xab32b0）；经 hkMemoryRouter 分配三个 0x10 字节 NodeList 并初始化——**`activeNodes`(+0x98)**{0,0,0x80000000}、**`activeNodeTemplateToIndexMap`(+0xA0)**{0,0,0xffffffff}、**`activeNodesChildrenIndices`(+0xA8)**{0,0,0x80000000}（CommonLib 命名吻合）；置 `[this+0x12C] = 1`（激活标志）；调 **60079（0xb0fb70，子图激活遍历）**(this, ctx, 0)；最后 `ctx.behavior = 0` 收尾。**G1b 崩溃缺失的 `activeNodes` 正是 Activate 自行分配的**；前置仅要求 `data` 非空（候选已满足）。`variableValueSet`(+0xD8) 的分配点未在短路径出现（在 60079 内部或长路径 0xab18c0，留待 G2 运行期/细读确认）。长路径（`ctx.behavior` 非空，0xab187a → 0xab18c0，0x4F8 B）调用同一次 60079，语义差异未逐条展开。
4. **引擎激活配方 = 63586（0xbb2270，236 B）**：单参 `graph`。检查 `behaviorGraph(+0x208)` 非空（空则返回 false）→ 置 `[graph+0x244] = 1` → `physicsWorld(+0x238)` 非空时取其 vtable 槽位 40 作为 world 实参（空则 0）→ **58315(ctx, `&graph->characterInstance`, world, 0)** → `behaviorGraph->vtbl[4](ctx)`（Activate）→ 复位 character `eventQueue` 的游标字段（`[eq+0xC]=0`、`[eq+0x14]=0`）→ `[graph+0x244] = 0` → 返回 true。**零自建结构**：context 完全来自图自身字段。唯一调用者 = **63356（0xba3a50）**——编排者 manager 挂接尾链的最后一步；候选图绕过了该链，这就是它未激活的原因。
5. **`hkbBehaviorGraph::Update` = 槽位 5 = 58329（0xab1890）**，签名 `(this, ctx, xmm2 = timestep)`；短路径取 `ctx.eventQueue(+0x18)`，为空则回退 `character.eventQueue(+0x80)`。引擎级图更新包装器 **63587（0xbb2370，1444 B）** 签名为 `(graph, 更新记录, ?)`——第二实参是管理器更新记录（读 `[rec+8]/[rec+0x10]/[rec+0x28/0x2a/0x2c]` 等），不是裸 float；候选图无 manager，直接复用 63587 需要先构造该记录。63625（0xbb7130）为图级同步辅助（先调 BShkbAnimationGraph vtable 槽 5），非干净推进入口。
6. **G2 可行性结论**：**激活 = 对候选单次调用引擎自己的 63586**——无自建 context、无管理器依赖（physicsWorld 为空有显式分支），是激活的既定引擎入口。推进：候选无 manager 更新记录，G2 需在"自建 context（58315）+ 直调槽位 5 传 timestep"与"构造 63587 的更新记录"之间做设计选择（前者调用面最小，倾向前者）；Update 对 `variableValueSet`/rootGenerator 的具体消费路径在 G2 契约设计时以 60079/58329 细读补充。G2 仍需先按六关程序设计其预注册判据（固定时间步、源差分采集、禁混装备操作）。

## 姿态写入链与字符级激活（2026-10-24 续审计六）

G2 首轮"激活成功但无可观测前进"的归因审计。全部指令级事实：

1. **`hkbBehaviorGraph::Update`（58329）短路径**（0xab1890..0xab1f19，含长路径跳板 0xab1f19）：取 `ctx.eventQueue(+0x18)`（空则回退 `character.eventQueue(+0x80)`）→ 一段 hkMemoryRouter/rdtsc 性能打点 → **`cmp byte [graph+0x12E],0`**：非 0 才调 60079（子图重激活遍历；Activate 只写了 +0x12C，+0x12E 语义未定）→ 读 `activeNodes(+0x98)` 计数 → **58383（0xab6ad0）**(this, ctx, xmm1=timestep, r8=帧缓冲) → 逐活动节点更新循环 → 尾部再次 +0x12E 检查/60079 与逐节点收尾。**Update 内没有任何 `character.poseLocal(+0x90)` 写入**。
2. **pose 分配器 = 58466（0xabdc90，0x76 B）**：守卫 `character.poseLocal == 0`；读 `setup(+0x50)->animationSkeleton(+0x20)`，以 `[skeleton+0x30]` 为骨骼数写 `numPoseLocal(+0x98)`，按 `count×0x30` 经 hkMemoryRouter 分配缓冲写 `poseLocal(+0x90)`，用 `skeleton+0x38/+0x40` 的绑姿态数据填充（57148），最后置 `deletePoseLocal(+0x9D) = 1`（与源基线 `delete-pose=true` 吻合）。**`setup.animationSkeleton` 为空时该函数会空指针崩溃——调用方必须保证前置**。
3. **字符级激活 = 0xabd630（119 B，地址库无 id）**：守卫 `behaviorGraph(+0x58)` 非空且 **`behaviorGraph->+0x12C == 0`（未激活）** → **先调 58466 分配 pose 缓冲** → 以 58315 建 context（world/attachment 取自 `character.raycastInterface(+0x78)` 的 +0x130/+0x138，为空则 0）→ `behaviorGraph vtbl[4]` Activate。0xabd6b0 为带附加实参的变体。0xabd930 = `GetOrAllocatePose`（返回 poseLocal，空则分配）。**引擎的激活顺序是"先 pose 缓冲、后 Activate"——G2 首轮用的图级 63586 绕过了 pose 分配，这就是候选无姿态输出的根因。**
4. **绑定链辨析**：58464（0xabdbd0）实为"置 `character.behaviorGraph(+0x58)`"（AddRef/Release 配对）+ raycast 接口循环（0xabdc1d..0xabdc68，`raycastInterface` 为空直接返回）——**不含 pose 分配**。其调用者：63585（栈上加载用 character ×2）、**63847（0xbc2390：从容器建 hkbBehaviorGraph 实例后把实例绑到 ARG1 character）**、0xaea12b（×2，hkbCharacter 簇）。63847 的 ARG1 是否为嵌入 character、以及候选 `setup.animationSkeleton` 的填充点（63846→63950→63947 链）未逐条核实——留待运行期判读。
5. **G2 v2 修正**：激活改调**字符级激活 58453**（0xabd630，运行期守卫 `setup.animationSkeleton == 0` 时记录 `skip=setup-skeleton-missing` 干净结束；v2 运行证实骨架非空、AllocatePose 生效）。**续审计六补遗（G2 v2/v3 判读后）**：① Update（58329）全函数确认无任何 poseLocal 写入（位移枚举完整覆盖：图字段仅 activeNodes 与 +0x12E 门）；② 其 per-node 主循环有 `[node+0x30] flags&1` 与 `[entry+0x85]` 两道门；③ **采样器 = 63581（0xbafda0）**：读 `generatorOutputs[0]`（一元素数组头）的元素、按元素 +0x32/+0x34 的 pose 轨道偏移以 57148 按 `count×0x30` 拷入 GetOrAllocatePose（58458）返回的 poseLocal；④ **采样入口 = 63589（0xbb2a40）**：门为 setup/animationSkeleton/behaviorGraph 非空 + `+0x12C` 已激活（候选全过），但 63581 以 `[graph+0x240]`（当前动画索引 word，63587 每帧从管理器记录写入）为 0 时跳过；⑤ 虚表槽 6 = 向活动节点派发事件（58377），58429 = 单 float 写入，均非输出请求。**G2 v4**：驱动循环每步 Update 后将 `[source+0x240]` 镜像到候选（同一张图 + 共享绑定集下引擎本会写同值）并调 63589 完成采样。

## 管理器每帧管线与记录全解（2026-10-24 续审计七）

G2 六轮迭代后为“动画选择机制设计”做的定向补审。全部指令级事实（同一解包镜像，listing 已存 `listings/`）：

1. **63587（0xbb2370，1444 B）更新包装器全解**，签名 `(graph, record, ?)`，record 实际布局：
   - `+0x00` **f32 timestep**（驱动两处：渐变计时器递减、Update 的 xmm 实参）；
   - `+0x08` 回调函数指针、`+0x10` 回调实参——Update 后且 `graph+0x243 == 0` 时调用一次；
   - `+0x18` 世界变换/位置指针（移动距离→渐变计时缩放；也传入 63575）；
   - `+0x28` **u16 id**、`+0x2a` u8（非 0 = 跳过选择）、`+0x2b/+0x2c` u8、`+0x2e` u8（直写 `graph+0x249`）。**记录里没有任何动画身份字段。**
   - 入口即 `graph+0x240 = 0`、`+0x245/+0x246 = 1`；双计时器 `+0x1e8/+0x1ec -= dt`；选择路径（`+0x2a==0` 且活动计时器到期且 ≠ −FLT_MAX）调 **63575** 写 `+0x240` 并清 `+0x245/+0x246`（该帧**跳过 Update**）；渐变块管理 `+0x220/+0x228`（当前/上一渐变控制器）与渐变控制器数组（元素 0x10 B，含 `+0x12` word、`+0x18` dword、`+0x1c` 标志）；
   - **Update 仅在 `+0x245==1 && +0x246==1` 时执行**：58315 自建 ctx（character=`graph+0xC0`，world=`physicsWorld(+0x238)` vtable 槽 40 或 0）→ `behaviorGraph` vtable 槽 5（xmm=dt）→ 回调 → **63583(graph, ctx, 第三实参)** → TLS 槽 0x768 换入 0x3a 再还原（分配器域）→ 尾部 58429（多字段非空门）。
2. **63575（0xbaf880，487 B）= LOD 采样数解析器，不是动画选择器**：`out` 字节 + 返回 word；基数 = `min(record+0x28 id, [graph+0x170])`；`rootNode(+0x218)` 与 record 变换的距离² 按 `1/[root+0x78]²` 缩放后查阈值表 `[graph+0x190]`（计数 `+0x1a0`，元素 `{f32 阈值, u16 采样数, u8 标志}` 步长 8，最后一个命中项生效）；rootNode 为空走 `[graph+0x160]` 表的 0x80 步长回退路径。**`[graph+0x240]` 的语义至此完全闭合：按角色到相机距离衰减的采样变换数（近满 116、远衰减），与动画身份无关。**
3. **63583（0xbb0460，678 B）= 事件排空 + 名字簿记**：队列 = `ctx.eventQueue(+0x18)`，空则回退 `character.eventQueue(+0x80)`；`graph+0x243 != 0` 时仅清游标（`[eq+0xC]/[eq+0x14]=0`）；否则每调用**最多排空 5 个事件**（59324 取 24 B 表项）；**第三实参为 0 时整个注册块被跳过（静态证实无害路径）**；每个事件经重映射表（`[r9+0x10]` 表或 57195）得动画索引，越界（≥ `[projectDBData(+0x200)+0xE0]`）即跳过；名字取自 `[projectDBData+0xD0][idx]`（步长 8），经 `BSFixedString::Set`（69162）写进第三实参的 16 B 名字对数组（基址 `+0x08`）——纯簿记，v6 崩溃的唯一原因是第三实参非法（`+0x10` 为 0 时 `[r9+0x10]` 解引用崩）。
4. **63602（0xbb4c50，6280 B）= 每帧图管线**，签名 `(临时壳, 目标图)`（graph_helper 的调用形态；文首 r13=rcx=临时壳，bb5a7f 重绑定 `r13=[rbp+0x3b0]`=解析出的目标图——此前"63602(临时图, 订阅图)"的实参方向记录以本节为准）：头部 TLS 换域 + `临时壳+0x243=1`；从**目标图 vtable**（槽 2/0x10/0x30/0x38/0x58）收集事件；栈上构造 **12 指针 scratch 记录**（`rbp+0x140..0x198` → 本帧局部容器对象）；尾序：**65149(&目标图->characterInstance(+0xC0), 记录) → `目标behaviorGraph+0x12E = 1` → 63587(目标图, 栈记录, 0) → `目标图+0x243 = 0` → 63584(目标图)** → 容器析构收尾（63689/63696–63701/32600）。
5. **65149（0xc24c50，95 B）**：门 = `characterInstance.behaviorGraph(+0x58)` 非空；栈上构 `{dt=0 f32; 字节 1,1,1; +0x08=1; +0x09 word=0; +0x0B=1; +0x10 vtable=0x1992da8; +0x18 记录}` 后调 **65232**。
6. **65232（0xc28af0，443 B）**：门 = behaviorGraph 非空且（`+0x12C` 已激活或 `[struct+9]` 强制位）；速率 = 强制 ? 1.0 : **0.04**；**scratch 计数 = `[behaviorGraph+0x128]`（word）× 速率向上取 2 的幂**；scrap heap（68088）分配 计数×0x18 并清零首 8 字节；调 **65237（0xc28f30，4250 B）**（局部 ctx、struct、`{vtable, 记录}`、behaviorGraph，栈上另传 `{1.0, 1.0, 强制位, 0}` 与 `{计数, 魔数 0xefbeadde/0x41993508, heap, 数组}`）。
7. **65237 = 生成遍历引擎**：以 behaviorGraph vtable 槽 1 身份 + 哈希表 + RTTI 常量单例（58554/58639/58650/58666/58672/58675/58901/58927/62942/62956/62959，各为 `lea rax,[rip+X]; ret` 型身份取值）分派存储域；沿 `[bg+0x58]` 子数组 / `[bg+0x60]` 子数递归，按节点权重（`+0x80/+0x84` 混合时长、`+0x5a` 标志、`+0x5b`→58448 缓动）累计；引用生成器分支经 **65238** 解析子图（58554→`[bg+0x80]`；58901→59405/59372 索引；58666→`[bg+0x48][字节+0x58]` 钳制；58636→`[bg+0x50]` 等）；clip 生成器分支走 `[rbx+0x60]` 子生成器权重循环；生成结果经接口 vtable（`{0x1992da8, 12 指针记录}`：`+0x20` 报告节点输出尺寸→16 B 对齐 scratch、`+0x40` 查询、`+0x48` 写出 `{ptr,count}`）写进本帧 scratch；缓冲经 TLS arena（`[0x31b14d4]`→0x174f440）分配、57041/0x2019838 vtable 槽 4 释放。**裸 Update（58329）完全不含该遍历——G2 v1–v6“零生成产物”的最终根因即候选从未执行任何生成遍历，而非缺“管理器动画选择”。**
8. **引擎自建 63587 记录全零**（r14d 于 bb60ac 清零后至尾未再赋值）：`{dt=0, 回调=0, 实参=0, 变换=0, id=0, 标志 +0x2a=1/+0x2b=1/+0x2c=1/+0x2e=1}`，第三实参 = 0——slot-5 管线是**零 dt 再生成节拍**：65237 生成本帧输出、63587 零推进 Update、63583 排空事件。
9. **graph_helper（0xbacc90，70 B）= 通知中心槽 5 的每图处理**：栈上 `63555(&壳, 0, 0)` → `63602(&壳, 订阅图)` → `63556(&壳)`——空壳构造/真析构直接配对（不经智能指针），即 v7 要复刻的引擎配方。

## 事件派发链与槽 2 输出门（2026-10-24 续审计八）

G2 v7 判读后为事件注入判别（G3 前提）做的定向补审。全部指令级事实（同一解包镜像）：

1. **事件派发原语 = 58377（0xab58a0，hkbBehaviorGraph vtable 槽 6 的落体）**，签名 `(behavior, ctx, event*)`，event 为 hkbEvent（24 B：`+0x00` u32 id、`+0x08` hkRefPtr payload；58377 只读这两项）。流程：暂存并清零 `ctx+0x30` 字节 → 取 `ctx.eventQueue(+0x18)`，空则回退 `character.eventQueue(+0x80)`（与 Update/63583 同一队列）→ 遍历 activeNodes（0x90 步长）：跳过 `[entry+0x84]` 置位项；取 `[entry+0x60]` 状态机与 `[entry+0x58]` 节点 → 写队列游标（`[eq+0x18]=[sm+0xB8]`、`[eq+0x20]=1`）→ 以 57195 在 `[sm+0xB8]` 的事件→转移映射中查 id，命中（≠−1）则调节点 vtable `+0x30` 或 59310 **触发状态转移（激活新状态/生成器）**。
2. **槽 6 桩 = 0xab1f30（0x33 B）**：`ctx.behavior(+8) == 0` 时把 24 B 事件拷栈转调 58377，非空直接返回——[续审计五](#激活与推进入口g2-前提2026-10-24-续审计五)的"短路径"模式又一例；58315 自建的裸 context（behavior=0）恰好走派发路径。**Update（58329）尾部即经此桩派发队列事件**（完整反汇编 0xab1f59 落在 58329 函数体内）。
3. **批量派发器 = 0xab313e（0xC8 B，无直接调用者——经虚表/内部跳转进入）**：从环形队列逐项 pop（`[q+0xC]` 读游标、`[q+0x8]` 容量回绕、`[q+0x14]` 计数递减——与 59324 同一 ring 读取），经 `[q+0x18]` 映射表重映射 id（`[q+0x20]` 标志选择 57195 查 `[q+0x20]` 表或 `[q+0x10]` dword 表），逐个调 58377(behavior, ctx, &event)。**"入队 + Update"与"直调 58377"都是引擎原生路径。**
4. **59324（0xad60a0，.pdata 截断为 16 B）= 队列批量搬移**：锁内分配临时数组、按 `[q+0xC]`/`[q+0x8]` 环形拷出、逐项递减 `[q+0x14]`——63583 的取件器。队列环形布局至此闭合：`[q+0x00]` 表项数组、`[q+0x08]` 容量、`[q+0xC]` 读游标、`[q+0x14]` 待处理计数。
5. **事件名不在 exe**：`WeaponDraw`/`MT_Idle` 等字符串在全部节区零命中——事件名来自行为项目数据，运行期由 ProjectDBData 的名字→id 映射解析（[CommonLib 声明](../extern/CommonLibSSE/include/RE/B/BShkbAnimationGraph.h)：`unk70`(+0x70) 与 `unkA0`(+0xA0) 两张 `BSTHashMap<char*, i32>`，注释明确 `unkA0` 为 anim event 处理所读；`unkD0`(+0xD0) 全部事件名表，与续审计七 63583 的名字来源吻合）。**BSTHashMap 内联布局**（63639 实测）：表项数 `+0xC`（u32）、**链尾哨兵 `+0x18`（桶与链尾均指向哨兵而非空——v8 首轮崩溃即误用空停机所致）**、桶数组 `+0x28`，表项 0x18 步长 `{char* name, i32 id, next*}`。**63773（图 vtable 槽 13，0xbbd6b0）= 名字→id 查询**（哈希 + 桶链内联遍历，miss 返回 −1）。**运行期事实记录（v8 三轮）**：dbdump 解码确证两张表的 CLib 布局正确（哨兵 `+0x18` 同为模块静态地址、计数 1024/16384 均 2 的幂、桶为堆指针）——但桶链含**墓碑/释放态表项**，其键字段是垃圾字节；引擎查表一律**指针比较**（与驻留串指针相等，从不解引用键），CommonLib 侧按内容 strcmp 的遍历必崩（两轮崩溃的真正根因，非布局错误）。`db+0xD0` 数组（9379 项，全程安全）是动画/剪辑名表，不含 `MT_Idle`/`MoveStart` 等事件名。**插件侧事件→id 的正源 = 行为图自带字符串数据**：`hkbBehaviorGraphData(+0x78 stringData)->eventNames(+0x10)`，Havok 原生 `hkArray<hkStringPtr>`（指针低位为 managed 标志，读取需掩 `&~1`），**事件 id 即数组下标**，平行数组 `eventInfos(+0x40)` 与状态机事件转移映射同键控此 id 空间（[CommonLib 声明](../extern/CommonLibSSE/include/RE/H/hkbBehaviorGraphData.h)）。
6. **槽 2（63621，0xbb6c60，857 B）= 每帧输出重置/ragdoll 更新**，开头两道门：`physicsWorld(+0x238)` 为空 **或** `character.ragdollDriver(+0x30)`（即 `graph+0xF0`，[CommonLib hkbCharacter 声明](../extern/CommonLibSSE/include/RE/H/hkbCharacter.h)）为空 → 整体跳过。门内：63584 → **重置 `generatorOutputs[0]` 元素（`+0x32` word=0、`+0x38` f32=1.0，即 63581 采样器读的轨道偏移/权重）** → physicsWorld vtable 槽 40 → 控制器链表遍历。**候选两门皆空 → 连输出元素重置都未发生**——v7 元素哈希冻结与此吻合，构成注入判别失败时的下一假设（输出绑定门）。

## 绑定循环的骨骼映射语义与 vtable 澄清（2026-10-24 续审计九）

v10 判读后对"绑定循环计数来源"的定向补审。全部指令级事实：

1. **主 vtable 槽位澄清**：0x19881f8（最终主 vtable；构造序 = 0x1988180 基类先写、0x19881f8 于 bad16b 覆盖，槽 0 = 删除析构 0xbbcf30 与 G1 审计吻合）的有效槽位为 00–0D；此前引用的"槽 0E/0x11 = 0xbaad80/0xbaae00"是 dump 越界垃圾，**槽 +0x58 = 槽 0B = 63630（0xbb7e30，0x436 B）**。
2. **63630 = 驱动的骨骼映射提供者**：门 = `ragdollDriver(character+0x30)` 非空（58286 锁）+ `physicsWorld` 经其 vtable 槽 40 getter 得 hkpWorld 非空 + `[driver+0x10]→[rax]→[+0x10]` 非空；**计数 = `[driver+0x18]`（hkArray 计数）**，表项 **0x58 步长**（hkbBone 类结构）；每次调用以 count×8 构建暂存与 `{count|0x80000000, ptr}` 描述符。63602 对图槽 2（63621）的调用循环遍历的就是这些**逐骨骼条目**——是骨骼级输出绑定路径，不是"当前动画"列表（v10 归因再修正）。
3. **与 v9 证据自洽**：源元素 `+32=0x0074`（=116 骨数）即逐骨骼绑定写入的轨道变换数；候选驱动对象存在（装载链自建）但 `[driver+0x10]` 骨数组为空 → 63630 计数 0 → 绑定循环零次 → 元素字段从未被写。**v10 的 world 借用让 63630 的两道门全部通过,但计数仍来自空的骨数组——借用无效的完整解释**。
4. **骨数组填充方未定位**：装载链（63949 setup 自建 → 63950 → 63946 → 63846 绑定）listing 内无 `character+0x30` 驱动骨数组的初始化；嫌疑范围 = 字符级激活簇（0xabd630 一带）、编排尾链 63356（0xba3a50）、hkbCharacter 簇（0xaea12b）。骨数组初始化大概率以 `setup.animationSkeleton` 为输入（hkbAnimatedSkeleton::init 类语义）——待续审计十。

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

# 5. 第二实参形态与拒绝路径（2026-10-24 续审计二）
python build/g1-audit-20261023/disasm_func.py 0xcfc5b0   # 69488 路径拆分
python build/g1-audit-20261023/disasm_func.py 0xcfcba0   # 69492 分隔符扫描桩（尾落入 0xcfcbb9）
python build/g1-audit-20261023/disasm_func.py 0x199640   # 13274 SSO 串 assign
python build/g1-audit-20261023/disasm_func.py 0xbc1b10   # 63844 项目解析
python build/g1-audit-20261023/disasm_func.py 0xbc6310   # 63889 解析器构造（vtable 0x19892a8）
python build/g1-audit-20261023/disasm_func.py 0xbc4d30   # 解析器槽位 3 查表
python build/g1-audit-20261023/disasm_func.py 0xcfc8c0   # 69490 路径规范化
# 0xcfcbb9 扫描器与 0xcfc490(69487) 拼接的函数体超出 .pdata 记录，用 capstone 从 RVA 原始反汇编（见任务记录脚本）。

# 6. 加载取值与前置链（2026-10-24 续审计三）
python build/g1-audit-20261023/disasm_func.py 0xcfc720   # 69489 扩展名剥离
python build/g1-audit-20261023/disasm_func.py 0xbc91e0   # 63949 project→character 绑定 + setup 自建
python build/g1-audit-20261023/disasm_func.py 0x2f63e0   # REFR vtable 槽位 114 描述符填充
python build/g1-audit-20261023/disasm_func.py 0x5418a0   # 32715 名字取值（类型键 0x3284b30 二分查找 70510）
python build/g1-audit-20261023/disasm_func.py 0x2f69f0   # 编排者调用者：栈上 2×0x118 表项构造

# 7. 已装载图析构的崩溃定位（2026-10-24 续审计四）
python build/g1-audit-20261023/disasm_func.py 0xbad2c0   # 真析构（behaviorGraph 非空 → Deactivate）
python build/g1-audit-20261023/disasm_func.py 0xab1f70   # hkbBehaviorGraph::Deactivate（vtable 槽位 7）
# 0xab1f9c 叶函数与 0xab0a60(58315 hkbContext 构造) 超出 .pdata，用 capstone 从 RVA 原始反汇编。

# 8. 激活与推进入口（2026-10-24 续审计五，G2 前提）
python build/g1-audit-20261023/disasm_func.py 0xab1780   # Activate 桩（真身 0xab17a3 落体，raw）
python build/g1-audit-20261023/disasm_func.py 0xbb2270   # 63586 激活包装器
python build/g1-audit-20261023/disasm_func.py 0xbb2370   # 63587 引擎级更新包装器
python build/g1-audit-20261023/disasm_func.py 0xab1890   # Update 桩 58329
# 0xab0a60(58315 InitContext) 无 .pdata，raw 反汇编。

# 9. 管理器每帧管线与记录全解（2026-10-24 续审计七）
python build/g1-audit-20261023/disasm_func.py 0xbb2370   # 63587 更新包装器（记录布局全解）
python build/g1-audit-20261023/disasm_func.py 0xbaf880   # 63575 LOD 采样数解析器
python build/g1-audit-20261023/disasm_func.py 0xbb0460   # 63583 事件排空 + 名字簿记
python build/g1-audit-20261023/disasm_func.py 0xbb4c50   # 63602 每帧图管线
python build/g1-audit-20261023/disasm_func.py 0xc24c50   # 65149 帧记录薄包装
python build/g1-audit-20261023/disasm_func.py 0xc28af0   # 65232 生成准备（scratch 分配）
python build/g1-audit-20261023/disasm_func.py 0xc28f30   # 65237 生成遍历引擎
python build/g1-audit-20261023/disasm_func.py 0xc29fd0   # 65238 引用生成器解析
python build/g1-audit-20261023/disasm_func.py 0xbacc90   # 通知中心槽 5 每图处理
# 58554/58639 等 RTTI 身份单例无 .pdata，raw 反汇编（lea rax,[rip+X]; ret）。

# 10. 事件派发链与槽 2 输出门（2026-10-24 续审计八）
python build/g1-audit-20261023/disasm_func.py 0xab58a0   # 58377 事件→转移派发原语
python build/g1-audit-20261023/disasm_func.py 0xab1f30   # 槽 6 桩（ctx.behavior==0 转调 58377）
python build/g1-audit-20261023/disasm_func.py 0xab313e   # 队列批量派发器
python build/g1-audit-20261023/disasm_func.py 0xad60a0   # 59324 队列批量搬移（.pdata 截断，raw 补充）
python build/g1-audit-20261023/disasm_func.py 0xbb6c60   # 槽 2（63621）输出重置/ragdoll 更新（双门）
python build/g1-audit-20261023/disasm_func.py 0xbb8c70   # 63639 按名读变量（unk70 布局证据）
python build/g1-audit-20261023/disasm_func.py 0xbbd6b0   # 63773 名字→id 查询（vtable 槽 13）
```

脚本只读 PE/versionlib，不加载模块、不调用引擎；`analysis.json` 由 `build_maps.py` 从 versionlib 生成。反汇编注释中的函数名来自 CommonLibSSE 头文件的 `RELOCATION_ID` 标签，未标名者保持 ID/地址。

## 限制

- 静态审计不能替代运行：没有构造、加载、激活、绘制或销毁任何真实图，也没有验证 OAR/IED 在空 holder 下的行为。
- `.pdata` 只覆盖有展开信息的函数；叶函数可能没有条目，`_purecall` 之类的目标无法用 `.pdata` 反查（其槽位一致性由三个纯虚槽同址证明）。
- 地址库记录的是**未打包布局**的 RVA；本次已用 vtable/RTTI/`.pdata` 对齐验证，但不代表所有 ID 都经过逐条核对。
- 第三实参的 `RACE_DATA::flags` 第 7 位语义、`+0x247` 的用途、`0xcc4680` 的一次性初始化身份均未解析。
- 本次没有修改运行代码，没有新增依赖、目标或配置；`BehaviorGraphProbe` 的采集字段和 G1 采集预算不变。
