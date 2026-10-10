# HKX13 静态核对：66 个 clip 是模板树，从未走到活跃克隆实例（2026-10-10）

- 性质：**纯静态核对，未发上机包、未改代码、未构建新 DLL**。依据 [PRD 0.8 §6.4](player-panel-prd.md) 首选实施顺序第 1 条
  （先做有界静态核对，无法解释来源与持有关系时不增加上机盲探轮次）。
- 输入：HKX9／HKX10 的**已归档日志**（两次独立会话）+ 本仓 CommonLibSSE checkout（`ng`，commit `94faaed0c6`）
  的**类型化头文件**。无新游戏数据。
- 结论等级：**由已有日志与类型化布局交叉验证得出的强假设**，不是游戏实测结论。它改变的是"路线 A 已死"这条判读的
  依据，不等于已取得可采样动画。

## 1. 核对了什么

PRD §6.4 第 1 条要求核对真实 `Activate`／`Load`／`Update`／采样调用的**对象类型、签名、字段与执行时点**。
本次核对的落点是：`hkbClipGenerator::binding`（@0xA0）与 `animationControl`（@0x88）**在什么对象上、什么时候**被填充。

CLib 给出的生命周期声明（[AnimationFileManagerSingleton.h:47-49](../extern/CommonLibSSE/include/RE/A/AnimationFileManagerSingleton.h#L47)）：

| 虚函数 | CLib 注释所述调用点 |
| --- | --- |
| `Queue` | "called in hkbClipGenerator::**Activate**, possibly loads animation" |
| `Load` | "called in hkbClipGenerator::**Update**, possibly loads animation" |
| `Unload` | "called in hkbClipGenerator::**Deactivate**" |

即 binding／animationControl 属**激活后**的运行期状态，不是静态装配期字段。

## 2. 决定性证据：被扫的根状态机是模板

HKX9 与 HKX10 两次独立会话的 `root-sm` 原始转储（各 33 qwords = `sizeof(hkbStateMachine)`=0x108），
按 [hkbNode.h:56-61](../extern/CommonLibSSE/include/RE/H/hkbNode.h#L56) 与
[hkbStateMachine.h:110](../extern/CommonLibSSE/include/RE/H/hkbStateMachine.h#L110) 解码：

| 字段 | 偏移 | HKX9 | HKX10 | 判读 |
| --- | --- | --- | --- | --- |
| `hkbNode::name` | 0x38 | 0x29a34062d80 | 0x24134462d80 | 堆指针，读出真名 `Master_Behavior` |
| `hkbNode::id` | 0x40 | 0x0000 | 0x0000 | — |
| **`hkbNode::cloneState`** | **0x42** | **0x01** | **0x01** | **`kTemplate`（1）** |
| **`hkbStateMachine::isActive`** | **0x88** | **0x00** | **0x00** | **`false`** |
| `states._data` | 0x90 | 0x…62d90 | 0x…62d90 | — |
| `states._size` | 0x98 | 11 | 11 | 与日志 `states.size=11` 一致 |
| `states._capacityAndFlags` | 0x9C | 0x0b + DontDealloc | 同 | 教科书 hkArray |

枚举值取自 `hkbNode::CloneState`（`kDefault=0 / kTemplate=1 / kClone=2 / kShareable=3`）。

**三重自校验**（防止偏移解错）：

1. `states._size`（0x98）解出 **11**，与日志自己打印的 `states.size=11` **逐字相同**；
2. `states._capacityAndFlags`（0x9C）低 4 字节同为 11、高位带 `kDontDeallocFlag`，是合法 hkArray；
3. `name`（0x38）解出的指针在两轮中都与日志打印的 `data=0x…62d90` 邻近（同一次分配内）。

三条互相独立地吻合 → **偏移基准正确，`cloneState=1`、`isActive=0` 不是解码错误。**

## 3. 由此改写的判读

HKX6–HKX12 全程扫描的是 `hkbBehaviorGraph::rootGenerator`（@0x80）——**模板树的根**：

| 成员 | 偏移 | 本项目状态 |
| --- | --- | --- |
| `rootGenerator` | 0x080 | **HKX6–HKX12 一路扫的就是它**（`animation_source.cpp:1492`） |
| `rootGeneratorClone` | 0x090 | **从未读过** |
| `activeNodes`（`hkArray<hkbNodeInfo>`） | 0x098 | **从未读过** |
| `nodeCloneToTemplateMap` / `nodeTemplateToCloneMap` | 0x0E8 / 0x0E0 | 从未读过 |

（`hkbBehaviorGraph` 布局见 [hkbBehaviorGraph.h:57-72](../extern/CommonLibSSE/include/RE/H/hkbBehaviorGraph.h#L57)；
`hkbNodeInfo::{nodeTemplate@0x50, nodeClone@0x58}` 见 [hkbNodeInfo.h:15-16](../extern/CommonLibSSE/include/RE/H/hkbNodeInfo.h#L15)）

按 Havok 的模板／克隆语义，模板是**共享资源对象**，`binding`／`animationControl` 只在每个角色的**克隆**上于
`Activate`／`Update` 时填充。这解释了 HKX10 中一直无法解释的组合——**名字全对、索引全对、指针全空**：
`animationName`（@0x48）与 `animationBindingIndex`（@0x70）是**静态字段，克隆时原样复制**，而 binding/control 不是。

### 对既有结论的影响

- HKX10 判读「标准 Havok binding/control 层在 Skyrim 运行时是死的」→ **依据不成立**：它扫的是模板树，
  未走到活跃实例。PRD §6.4 第 284 行已预警「某批字段为空不能连带判定引擎标准 binding/control 层普遍无效」。
- 路线 A（经引擎已加载动画取 `hkaAnimation`）的"死路"结论 → **应撤回重判**，但**不等于路线 A 已通过**：
  仍未取得任何可采样对象（H01/H02 均未满足）。
- HKX11/12 的注册表／管理器地图仍然有效（它们描述的是另一层），但不构成对活跃实例的否定。

## 4. 通往活跃实例的类型化路线（给下一轮上机用）

全 CLib 类型化、零虚调用、无需猜布局：

```
hkbBehaviorGraph::rootGeneratorClone (0x90)            → hkbGenerator*
hkbBehaviorGraph::activeNodes (0x98)                   → hkArray<hkbNodeInfo>
        └─ hkbNodeInfo::nodeTemplate (0x50) / nodeClone (0x58)
每个候选对象用 hkbNode::cloneState (0x42) 判定：kClone(2) 才是活跃实例
```

判据可交叉验证：`nodeTemplateToCloneMap`（0xE0）应能把已扫到的模板 clip 映射到对应克隆。

## 5. OAR 源码核对（PRD §6.4 第 1 条，已闭环）

**固定 commit：`f4e7688b065175aff70aa523073857911e15aca3`（2026-08-31，工作区干净）**，源码置于 `../OpenAnimationReplacer`。
仅参考源码，未引入运行时交互（PRD §6.4 红线）。

### 5.1 OAR 怎么拿到活跃 clip：**不遍历 `rootGenerator`，而是钩住 `Activate`**

[Hooks.cpp:40-60](../OpenAnimationReplacer/src/Hooks.cpp#L40)：OAR 钩 `hkbClipGenerator::Activate`，
由引擎把**正在被激活的那个 clip generator（`a_this`）**递进来，再 `AddOrGetActiveClip` 建 `ActiveClip`。
`Update`（:62）与 `Deactivate`（:72）同法。**OAR 从头到尾没有遍历 `rootGenerator` 找 clip。**

→ 佐证 §3：`rootGenerator` 是模板，`Activate` 收到的才是实例。我们的扫描路线与 OAR 完全不同。

### 5.2 替换机制：写 `animationBindingIndex`，**不是读 binding 对象**

[ActiveClip.cpp:120](../OpenAnimationReplacer/src/ActiveClip.cpp#L120) 注释明确：

```
_clipGenerator->animationBindingIndex = newBindingIndex;  // this is the most important part - this is what actually replaces the animation
                                                          // the animation binding index is the index of an entry inside hkbCharacterStringData->animationNames
```

→ 说明 **OAR 不需要 `hkaAnimation` 对象**：它只改索引，引擎自己按索引去装载与绑定。
这与 HKX3 的 `names == bindings == 15203` 同序结论一致。**OAR 的成功不构成"能取到 hkaAnimation 对象"的证据。**

### 5.3 但 OAR 确实从 binding set 取到了有效 binding——且是从**角色自己的** binding set

[FakeClipGenerator.cpp:106-126](../OpenAnimationReplacer/src/FakeClipGenerator.cpp#L106)：

```cpp
const RE::BShkbAnimationGraph* graph = reinterpret_cast<RE::BShkbAnimationGraph*>(behaviorGraph->userData);
const auto& setup = graph->characterInstance.setup;
auto animationBindingSet = static_cast<RE::hkbAnimationBindingSet*>(graph->characterInstance.animationBindingSet.get());
if (!animationBindingSet) {
    animationBindingSet = setup->animationBindingSet.get();      // ← 回退到 setup 的绑定集
}
const auto animationBindingWithTriggers = animationBindingSet->bindings[animationBindingIndex];
binding = animationBindingWithTriggers->binding.get();           // ← hkbAnimationBindingWithTriggers +0x10
```

三处直接推翻我们的既有判读：

1. **`hkbAnimationBindingWithTriggers` 是 0x28 字节、`binding` 在 +0x10**
   （[Havok/Havok.h:200-203](../OpenAnimationReplacer/src/Havok/Havok.h#L200)，带 `static_assert`）。
   我们 HKX4/5 判读的"0x30 字节小对象/空桩"**尺寸就错了**（0x30 vs 0x28）；HKX10 读 +0x10 位置虽对，
   但**读的是 `hkbCharacter::animationBindingSet`（`hkbCharacter.h`@0x68）**，而 OAR **优先用
   `graph->characterInstance.animationBindingSet`、为空才回退 `setup->animationBindingSet`（@0x38）**——
   两个不同的绑定集，我们只读了后者，从未试过回退。
2. **`binding` 为 null 时 OAR 自己造一个**：[FakeClipGenerator.cpp:128-134](../OpenAnimationReplacer/src/FakeClipGenerator.cpp#L128)
   `hkHeapAlloc` + `hkaDefaultAnimationControl_ctor(animControl, binding, true, 2)`。
3. **装载入口**：`AnimationFileManagerSingleton::Load(a_context, fakeClipGeneratorPtr, nullptr)`
   （[FakeClipGenerator.cpp:144](../OpenAnimationReplacer/src/FakeClipGenerator.cpp#L144)）——与 CLib 注释的
   "Load called in Update" 吻合，说明 **binding 的填充发生在 Update 期**。

### 5.4 采样入口与 cache 参数（HKX6 悬案的实测答案）

[ActiveAnimationPreview.cpp:38](../OpenAnimationReplacer/src/ActiveAnimationPreview.cpp#L38)：

```cpp
previewAnimation->SamplePartialTracks(_previewClipGenerator->localTime, numTransformTracks,
                                      sampledTransformTracks.data(), previewAnimation->numberOfFloatTracks,
                                      sampledFloatTracks.data(), nullptr);   // ← cache = nullptr
```

**OAR 传 `nullptr` 作为 `hkaChunkCache*`**，且注释未提 spline 例外。→ 支持 HKX6 选择
`SampleIndividualTransformTracks`（无 cache 参数）的路线；"spline 是否需要 cache"不再是阻塞项。
（未验证：OAR 走的是 `SamplePartialTracks`，与 `SampleIndividualTransformTracks` 是不同虚函数槽，
两者对 cache 的要求未必完全等价——仍以我们自己上机实测为准。）

轨道→骨骼映射取自 `binding->transformTrackToBoneIndices`（同上 :63），
**不是**"轨道 i = 骨骼 i"——PRD §6.4 明确禁止用后者替代映射。

### 5.5 持有方式

OAR 用 `RE::hkRefPtr`（引用计数）持有 `animationControl`／`binding`
（[FakeClipGenerator.h:43,46](../OpenAnimationReplacer/src/FakeClipGenerator.h#L43)：
`hkRefPtr<hkaDefaultAnimationControl> animationControl;`）——而非裸指针。
**注意**：OAR 全程在引擎的 Activate/Update/Generate 窗口内操作，生命周期由引擎的 clip 生命周期兜底，
它**没有**证明"跨帧长期持有、暂停后仍有效"。PRD §6.4 要求的"资源持有必须覆盖压缩数据、绑定与轨道表依赖的
底层资源"**OAR 未给出答案**——这是我们自己的待办。

## 6. 结论与下一步

### 构建与测试单（HKX13 包，已部署）

- 提交 `47725682fa`，包 `dist/CharacterPanel-scopy-HKX13-1.2.1.zip`，身份
  `CharacterPanel-scopy-HKX13-47725682fa-cl94faaed0c6-20261010T135149Z`（`source_baseline_dirty=false`）。
- DLL SHA-256 `b2869467…`、PDB `ca632f3d…`，**两者均已从最终 zip 部署并校验一致**（纠正 HKX11/12 部署中间构建的问题）；
  上一份游戏日志已存档为 `docs/diagnostics/CharacterPanel-pre-hkx13-20261010-215222.log`。
- 新增 `src/character/animation_source.cpp` 的 `report_animation_instance()`，在 `capture()` 里于 HKX6 之前调用。
  单元测试 `snapshot_transform` 通过（1/1）。
- **上机**：进游戏 → 打开背包（暂停）→ **F7 捕获** → 看 `SCOPY ANIM instance …` 系列 → F8 绘制 → 测完**立刻复制**
  `CharacterPanel.log`（非追加模式）到 `docs/diagnostics/`。

判读表：

| 日志 | 含义 | 下一步 |
| --- | --- | --- |
| `roots generator=template clone=kClone` | 假设成立：两侧确实不同 | 看 instance clip 行 |
| `roots generator=template clone=template` | 克隆根未建/不在该成员 | 转 `activeNodes` 行与 `clone-states` |
| `instance clip … reason=valid` | **binding 已取得** | 接 ground-truth + F2 驱动（HKX6 协议） |
| `instance clip … binding=0x0 reason=no-binding-in-set` | 实例侧也是空 | 判读"标准 binding 层确为惰性/未填"，转路线 D |
| `UNAVAILABLE reason=no-clip-on-the-instance-side` | 未走到实例 | 看 `objects`/`clone-states` 定遍历问题 |

### 已可确定的（静态）

- 我们扫的是模板树（`cloneState=kTemplate`、`isActive=false`，两轮日志三重自校验）→ **路线 A 的"死路"结论撤回**。
- 通往实例的路线：优先 `graph->characterInstance.animationBindingSet`、**为空才回退 `setup->animationBindingSet`**，
  元素按 **0x28 字节** `hkbAnimationBindingWithTriggers`，`binding` 在 **+0x10**（OAR 带 static_assert）。
- 采样可传 `cache = nullptr`；映射必须走 `transformTrackToBoneIndices`。

### 仍未闭环

- **活跃克隆上的 `binding` 是否非空，需上机实测**（HKX9/10 的 66 空是在模板上读的，不构成否定）。
- OAR 未解决"长期安全持有"——这是我们自己的待办，不因 OAR 成功而消失。
- 未验证 `SamplePartialTracks` 与 `SampleIndividualTransformTracks` 对 cache 的要求等价。

### 下一轮上机的最小范围（待用户同意后实施）

只问一个问题：**实例侧 binding 是否可取得**。建议日志面：

```
SCOPY ANIM instance graph=i char-bindingset=0x…/N setup-bindingset=0x…/N root-clone=0x… active-nodes=N
SCOPY ANIM instance clone-states='kTemplate:…,kClone:…,other:…'
SCOPY ANIM instance clip i=… name='…' idx=… clone-state=… binding=0x… control=0x… tracks=… reason=…
SCOPY ANIM instance element idx=… at=0x… size=0x28 binding@0x10=0x… valid=… reason=…
```

仍须遵守：只读、不写源角色、不虚调用候选对象、**从最终 zip 部署**（HKX11/HKX12 曾部署提交前中间构建，
日志横幅对不上 manifest，本轮起须纠正）。

## 6. 下一轮上机的建议范围（待用户同意后实施）

若按 §4 路线做一轮只读探针，建议只问一个问题：**活跃克隆上的 `binding`／`animationControl` 是否非空**。
最小日志面：

```
SCOPY ANIM instance root-clone=0x… active-nodes=N clone-states='kTemplate:…,kClone:…,other:…'
SCOPY ANIM instance clip i=… name='…' clone-state=… binding=0x… control=0x… reason=…
```

仍须遵守：只读、不写源角色、不虚调用候选对象、从最终 zip 部署（HKX11/HKX12 两轮曾部署提交前中间构建，
日志横幅与 manifest 对不上表，本轮起须纠正）。
