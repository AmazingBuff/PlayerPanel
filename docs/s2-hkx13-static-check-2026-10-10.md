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

## 5. 尚未完成（PRD 第 1 条未闭环）

- **未固定 OAR commit，未核对其真实播放路径**。PRD §6.4 第 1 条与测试单第 12 行要求固定 OAR commit 并核对
  `ActiveClip.cpp` 的真实播放／混合路径（"仅参考源码，不调用其运行时接口"）。本轮该项**未完成**。
- 未验证克隆链在运行期确实非空（需上机）。
- 未涉及资源持有（PRD §6.4 "资源持有必须覆盖压缩数据、绑定与轨道表依赖的底层资源"）——留待取得对象后。

## 6. 下一轮上机的建议范围（待用户同意后实施）

若按 §4 路线做一轮只读探针，建议只问一个问题：**活跃克隆上的 `binding`／`animationControl` 是否非空**。
最小日志面：

```
SCOPY ANIM instance root-clone=0x… active-nodes=N clone-states='kTemplate:…,kClone:…,other:…'
SCOPY ANIM instance clip i=… name='…' clone-state=… binding=0x… control=0x… reason=…
```

仍须遵守：只读、不写源角色、不虚调用候选对象、从最终 zip 部署（HKX11/HKX12 两轮曾部署提交前中间构建，
日志横幅与 manifest 对不上表，本轮起须纠正）。
