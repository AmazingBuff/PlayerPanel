# C-G5-TRANS v5：WeapEquipBFR 子图与 State3 Blender 只读探针预注册（2026-11-19）

**状态：IMPLEMENTED / BUILT / DEPLOYED（v114，2026-11-19）；未上机。** 本契约接续 [v4](../g5-transition-probe-v4-20261119/README.md) 的候选 run 1；不改变 `G4P-7 FAIL` / G5/G6，不产生 `tau_*`。

## 一、v4 run 1 的输入与问题

v4 候选 run 1（[记录](../g5-transition-probe-v4-20261119/run1-candidate/README.md)）已经给出：

- K5 249/249；747 条探针 header 全 `flags=0x0000`；`TRANSITION MODIFIER` 2,379、`BIND` 537、summary overflow 全 false。
- `WeapEquipModifierGenerator` 的 child generator 就是 census 中的 `WeapEquipBFR`：两者指针同为 `0x1c23320dd40`（v4 日志 `TRANSITION MODIFIER` 行与 `TRANSITION CENSUS` 行）。
- equip cmd=1 的 4 个 active modifier 到 cmd≥2 只剩 `RootModifierList` / `bAttachedModifierList`，且全部 `bindings=0`；`TurnDelta`/`Speed` 恒定 → **H-M1 判负**，modifier 的 graph-variable 绑定不是 0→10 的标量。
- 剩余假设域：`WeapEquip.hkb` 子图（`WeapEquipBFR`）、`allPartitionMemory`/`nodePartitionInfo`、`hkbBlendingBehavior`。

v5 只回答第一个域：**`WeapEquipBFR` 的 `behavior` 指针是否指向一个可枚举的 `hkbBehaviorGraph`，其 `activeNodes` 是否包含 `Weap_EquipDefaultState`/`msg/State3`，以及该 Blender 的权重是否在 equip cmd 1–13 随步推进。** 不在本契约内实现 `allPartitionMemory` 或 `hkbBlendingBehavior`。

## 二、离线布局审计（2026-11-19）

### 2.1 输入与身份

| 输入 | 值 |
| --- | --- |
| 解包镜像 | `build/g1-audit-20261023/SkyrimSE.original.exe.unpacked.exe`，36,950,016 B，SHA-256 `843823C115DBC56B5E9740350DB49170CC4EEA013A79513B7FD066DEEC719A2B` |
| 地址库 | `extern/CommonLibSSE/tests/REL/versionlib-1-6-1170-0.bin`，SHA-256 `C4093C569A3C83B26587F4B9EA4C55DE9AE6E73B84A2AF9FB3FBD30E2FE0D452` |
| CommonLibSSE | `94faaed0c60eddd8347767f2d4d29a97c93bde8c`（`ng`） |
| 运行时 | AE 1.6.1170；下文所有地址均为镜像 RVA，运行时地址 = 模块基址 + RVA |
| HKX 审计工件 | `build/hkx-audit/0_master_v3.txt` `277C4670…6D81`；`build/hkx-audit/weapequip_v3_full.txt` `E10E12F2…B2BA` |

v4 日志的模块基址可由三条 vtable 交叉确定为 `0x7ff68c910000`：BFR `0x7ff68e26ea20 - 0x195ea20`、Blender `0x7ff68e26eb70 - 0x195eb70`、ModifierList `0x7ff68e271860 - 0x1961860` 均得到同一基址。

### 2.2 `hkbBehaviorReferenceGenerator` 运行时布局

| 项 | 读数 | 证据 |
| --- | --- | --- |
| vtable | RVA `0x195EA20`（AE ID 226271） | RTTI `.?AVhkbBehaviorReferenceGenerator@@`；`Offsets_VTABLE.h` 的 `VTABLE_hkbBehaviorReferenceGenerator` |
| 对象大小 | `0x58` | 析构 `0xAC7860` 在 `0xAC78C2` 以 `mov edx, 0x58` 调 `operator delete`（`0xFB7E00`） |
| `behaviorName` | `+0x48`，`hkStringPtr`（8 B） | 构造函数 `0xACCCE0` 对 `+0x48` 和 `+0x38` 调 `0xA8D400`（`hkStringPtr` 置空/赋值路径；`+0x38` 是已知的 `hkbNode::name`）；对象大小 0x58 = 0x48 + 8 + 8 |
| `behavior` | `+0x50`，`hkbBehaviorGraph*` | 校验 `0xACCDF0` 比较 `[rcx+0x50]`，为零时报 `"You must provide a behavior."`；序列化 `0xACCF10` 以 `[rdi+0x50]` 和类名 `"hkbBehaviorGraph"` 调用；构造函数 `0xACCCE0` 对 `[rcx+0x50]` 调 `0xA883A0`（`hkReferencedObject` 引用释放）；slot 9 `0xACCD40` 把 `[rcx+0x50]` 写入输出数组 |

关键反汇编（`disasm_func.py`，镜像 RVA）：

```text
0xACCCE0  mov rcx, [rcx+0x50]        ; behavior
0xACCCF7  test rcx, rcx
0xACCD08  lea rcx, [rbx+0x48]        ; behaviorName
0xACCD0C  call 0xA8D400              ; hkStringPtr reset (name +0x38, behaviorName +0x48)
0xACCDF0  cmp qword ptr [rcx+0x50], 0
0xACCE08  call 0xA8D420              ; "You must provide a behavior."
0xACCF10  mov r8, [rdi+0x50]         ; behavior
0xACCF32  lea rdx, [0x195E1D0]       ; "hkbBehaviorGraph"
0xACCF43  call qword ptr [rax+0x18]  ; serialize as hkbBehaviorGraph
```

`hkbBehaviorReferenceGenerator` 在 CommonLibSSE `94faaed0c` 中没有类头文件，因此实现不得 `reinterpret_cast` 成某个 `RE::hkbBehaviorReferenceGenerator` 类型；只能以 `RE::hkbNode*` 取得 vtable，再用受保护的 `read_field<std::uintptr_t>(node, 0x50)` 读取 behavior，并通过 `VTABLE_hkbBehaviorGraph[0].address()` 校验后再转成 `RE::hkbBehaviorGraph*`。

### 2.3 `hkbModifierGenerator` 交叉核对

v4 的 HKX2 布局（`modifier +0x48` / `generator +0x50`）也在引擎侧得到确认：

| 项 | 读数 | 证据 |
| --- | --- | --- |
| vtable | RVA `0x195F1F8`（AE ID 226323） | RTTI `.?AVhkbModifierGenerator@@` |
| child generator | `+0x50` | `0xAE983D` `mov rax, [rbp+0x50]`；校验 `0xAE98E0` 对 `+0x50` 报 `"You must specify a child generator."` |
| modifier | `+0x48` | `0xAE9892` `mov rax, [rbp+0x48]`；非空时作为第二个 child 加入 |

v4 日志中 `WeapEquipModifierGenerator` 的 `modifier` 是 `bAttachedModifierList`（vtable `0x1961860`，RTTI `.?AVhkbModifierList@@`，AE ID 226532），`generator` 是 vtable `0x195EA20` 的 `WeapEquipBFR`。这与 HKX2 的 `hkbModifierGenerator` 布局一致，也把 v5 的目标 BFR 锁定为 `WeapEquipModifierGenerator` 的 child。

### 2.4 `hkbBehaviorGraph` 与 `hkbNodeInfo`

| 项 | 偏移/读数 | 证据 |
| --- | --- | --- |
| vtable | RVA `0x1941450`（AE ID 220287） | RTTI `.?AVhkbBehaviorGraph@@`；`VTABLE_hkbBehaviorGraph` |
| 对象大小 | `0x130` | CommonLibSSE `hkbBehaviorGraph.h` `static_assert` |
| `activeNodes` | `+0x98`，`NodeList*` | CommonLibSSE 头；`hkbBehaviorGraph::Update` `0xAB1890` 在 `0xAB196F`/`0xAB1A4B`/`0xAB1AD6`/`0xAB1AEB`/`0xAB1E2D`/`0xAB1E68` 读取 `[rbp+0x98]`；`0xAB197C` 读取 `[rax+0x08]` 作为 size |
| `isActive` | `+0x12C`，bool | CommonLibSSE 头；只报告，不作硬门槛 |
| `NodeList` | `hkArray<hkbNodeInfo>`：data `+0x00`、size `+0x08`、capacity `+0x0C` | CommonLibSSE `hkArray.h` / 既有 v1 用法 |
| `hkbNodeInfo` | size `0x90`；`nodeTemplate +0x50`、`nodeClone +0x58`、`behavior +0x60` | CommonLibSSE `hkbNodeInfo.h` |

`hkbBehaviorGraph::activeNodes` 是 v5 的新入口；`hkbNodeInfo::behavior +0x60` 只作为诊断指针记录，不递归跟随。

### 2.5 审计结论与限制

1. **布局成立**：`WeapEquipBFR +0x50` 是 `hkbBehaviorGraph*`，`+0x48` 是 `behaviorName`；`hkbBehaviorGraph::activeNodes` 在 `+0x98`。
2. **运行时未证**：本次只读离线审计，不证明该 behavior 指针非空、`isActive` 为真、`activeNodes` 非空，或其中包含 `State3`。这些正是 v5 的运行时假设。
3. **范围**：所有偏移只适用于 AE 1.6.1170 + 当前解包镜像；不外推 SE/VR/其他版本。
4. **不猜偏移**：如果运行时 vtable 不是 `VTABLE_hkbBehaviorGraph[0].address()`，探针只记 vtable/class，不读 `+0x98/+0x12C`。

## 三、假设与可否证条件

| 编号 | 假设 | 可否证读数 |
| --- | --- | --- |
| H-B1 | `WeapEquipBFR` 的 `+0x50` 非空、vtable 为 `hkbBehaviorGraph`，且 `activeNodes` 在 equip cmd 1–13 非空 | BFR 不出现、behavior 为空、vtable 不匹配、`activeNodes` 为空/无效 |
| H-B2 | 子图内存在 `Weap_EquipDefaultState`/`msg/State3` 或等价 Blender；其 `blendParameter`/`endIntervalWeight`/child weight 至少一项在 cmd 1–13 变化 | 找不到目标 Blender，或所有字段逐 step 恒定 |
| H-B3 | 该标量与同 `local_time` 组内 PC1 投影相关，并解释 v4 的 0→10 初始离轨 | 标量变化但与 PC1 无单调/相关关系 |

任一假设被否证，不靠同一探针重跑“碰”结果；按 §七进入下一层（`allPartitionMemory`/`nodePartitionInfo`，最后才是 `hkbBlendingBehavior`）。

## 四、探针设计

### 4.1 键与门控

- 新键 `[Experiment] TransitionSubgraphProbe=false`，默认 off。
- 要求候选 `TransitionStateProbe=true` 且 `K5FullSample=true`；任一不满足时不产生子图行，候选侧在首次调用时输出一行 `TRANSITION SUBGRAPH status=inactive reason=...`。
- 候选专用；源侧不调用该函数，不进入 source ring，不改 CPK5S 布局。
- 不新增 detour/hook；不写图/姿态/变量；不分发事件；`TransitionProbeRecord` 版本/容量/源 ring 均不变。

### 4.2 采样点与窗口

复用 [`independent_graph.cpp`](../../../src/animation/behavior_graph/independent_graph.cpp) 的 `log_transition_stage` 三个既有相位；在 `log_transition_modifiers` 之后调用 `log_transition_subgraph`。

| 侧 | 相位 | 窗口 | 输出 |
| --- | --- | --- | --- |
| 候选 | `post_update` | equip 1–13；unequip 1–12 | BFR summary + 目标 Blender/SM/child 值 |
| 候选 | `pre_sample` | equip 1–13；unequip 1–12 | 同上 + 子图 `activeNodes` census（每 commanded 一次） |
| 候选 | `post_sample` | equip 1–13；unequip 1–12 | BFR summary + 目标 Blender/SM/child 值 |

窗口外的记录不输出子图行（避免 249 条 × 3 相位 × 全节点造成日志膨胀）；v4 的 `TRANSITION MODIFIER` 与主探针行为不变。

### 4.3 枚举与上限

1. 从主图 `behavior->activeNodes`（cap 512）枚举，找 vtable 精确等于 `VTABLE_hkbBehaviorReferenceGenerator[0].address()` 的节点；cap 8 个 BFR。
2. 对每个 BFR：读 `+0x48` 字符串（cap 96）、`+0x50` behavior 指针。
3. behavior 先过滤指针，再读 vtable；只有精确等于 `VTABLE_hkbBehaviorGraph[0].address()` 才读 `+0x98`/`+0x12C`。
4. `activeNodes` 的 `NodeList`：data/size/capacity 先校验；size ≤ 0 或 data 不可信时记 `status=no-active-nodes`/`invalid-array` 并停止该 BFR。
5. 子图节点 cap 256；记录 `nodeClone +0x58`、vtable、class、name、id；不递归跟随子图里的 BFR（只记 `nodeInfo.behavior +0x60` 指针）。
6. 子图状态机 cap 8、Blender cap 8、每个 Blender 的 child cap 8；超过置 overflow 并计数。

### 4.4 日志 schema

```text
TRANSITION SUBGRAPH side=candidate segment=equip phase=pre-sample seq=<completed> commanded=<n> index=<i> bfr=<addr> bfr-name='<name>' behavior-name='<name>' behavior=<addr> behavior-vtable=<addr> behavior-active=<0|1> active-nodes=<n> status=<ok|unavailable|unknown-behavior-class|no-active-nodes|invalid-array>
TRANSITION SUBGRAPH NODE side=candidate segment=... phase=... seq=... commanded=... bfr-index=... index=... node=<addr> vtable=<addr> class=<clip|state-machine|blender|manual-selector|behavior-reference|...> name='...' id=<n>
TRANSITION SUBGRAPH SM side=candidate segment=... phase=... seq=... commanded=... bfr-index=... index=... node=<addr> name='...' current-state=<n> transitions=<n>
TRANSITION SUBGRAPH BLEND side=candidate segment=... phase=... seq=... commanded=... bfr-index=... index=... node=<addr> name='...' blend-param=<bits> end-weight=<bits> num-active=<n> begin=<n> end=<n> children=<n>
TRANSITION SUBGRAPH CHILD side=candidate segment=... phase=... seq=... commanded=... bfr-index=... blend-index=... child=<n> generator=<addr> vtable=<addr> class=<...> weight=<bits> wfm=<bits>
TRANSITION SUBGRAPH SUMMARY side=candidate segment=... phase=... seq=... commanded=... bfrs=<n> behaviors=<n> nodes=<n> sm=<n> blenders=<n> overflow=<bool>
```

浮点字段同时保留 raw bits 与 `.6f`（与 v1/v4 一致）；`blend-param`/`end-weight`/`weight`/`wfm` 都按 `std::bit_cast<std::uint32_t>` 记录。

### 4.5 安全与预算

- 所有指针先经 `plausible_object_pointer`；任何不可信指针只记 `status`，不 deref。
- 不对 behavior 调任何函数、不 addref/release、不写任何字段。
- 只在 `pre_sample` 输出子图 census；其余相位只输出 BFR summary + 目标字段，最大约 25 × 3 × 20 行，远低于常规日志预算。
- 任何 overflow、K5 缺代、身份不符或多次窗口使该批 INVALID，不静默截断。

## 五、判据（预注册，不得事后放宽）

| 编号 | 判据 | 通过条件 | 否则 |
| --- | --- | --- | --- |
| SP-1 身份/环境 | v114 三件套/存档/`AE-Track2`；K5 249/249；747 header `flags=0`；零非豁免 `[E]` | 全满足 | INVALID |
| SP-2 仪器成立 | equip 1–13 至少一条 `TRANSITION SUBGRAPH` 行对应 `WeapEquipBFR`；behavior 非空且 vtable 匹配；`active-nodes > 0`；overflow=0 | 全满足 | H-B1 未目击 |
| SP-3 目标定位 | 子图 census 找到 `Weap_EquipDefaultState`/`msg/State3` 或等价 Blender（名称 + child 身份） | 全满足 | H-B1 未目击/需换层 |
| SP-4 标量推进 | 目标 Blender 的 `blendParameter`/`endIntervalWeight`/任一 child weight 在 cmd 1–13 或同 `local_time` 组间至少一项变化 | 全满足 | H-B2 否证 |
| SP-5 无副作用 | 无图/姿态/变量写入；无新 detour/hook；`source-drift=none`；K5/CPK5 不变 | 全满足 | INVALID |
| SP-6 相关性（分析目标） | `10→1` 12 点 PC1 99.83%/Pearson 0.992817 复算；0→10 标量与 PC1 的 Pearson/Spearman/单调性只报告 | 不设阈值 | — |

SP-3/SP-4 未目击时，不把“探针没找到”写成候选失败或 G5 失败；按 §七进入下一层。SP-6 只支持下一版配对协议，不回填本门 5。

## 六、运行协议

1. 实现 v114，离线测试；备份 v113 三件套，安装 v114 + 候选档（`TransitionStateProbe=true`、`TransitionSubgraphProbe=true`、`K5FullSample=true`、`K5LayerAudit=false`、`K5Reconciliation=false`）。
2. MO2 `AE-Track2`（OAR 保持停用）→ Save116 → 一次 panel → equip 120/120 → settle → `Unequip` 144 → unequip 129；关闭退出。
3. 判读 SP-1..6；归档日志/CPK5/子图行/哈希。
4. 源侧不在 v5；若候选路线成立，再另立源侧预注册（同 BFR 布局，源 K5 apply-return 点）。

## 七、分析与分叉

1. **SP-3 + SP-4 成立**：0→10 的标量在 `State3` Blender；起草 `C-G5-PAIR v2`（clip 时钟 + 子图标量）作为新预注册，用新批次验证；本批 G4P-7 仍 FAIL。
2. **SP-3 成立但 SP-4 不成立**：Blender 存在但字段不是姿态轴；检查 `endIntervalWeight`/child weight 的运行时语义，或转 `hkbBlendingBehavior`/`nodePartitionInfo`；不重复同一读取。
3. **SP-2/SP-3 不成立**：BFR 的 behavior 可能是模板或无 activeNodes；转 `allPartitionMemory +0x118`/`nodePartitionInfo +0xF8` 的独立预注册；不要重复 v109 的 `0x200` 指针扫描。
4. **静态附件轴独立**：5 骨 1,240 次 `E_p` 超差仍需单独的“真实附件状态/静态附件相等”预注册；不能靠删骨或放宽 `tau`。

## 八、停止规则与明确不做

- 任一 `[E]`、崩溃、身份不符、K5 缺代、overflow/drop、多次窗口 → 停止并保留现场；同一会话不重跑同一窗口。
- 不启用 OAR、不切 `AE`；不改 `tau`/`epsilon_t`；不删轨；不读未审计偏移；不写源/候选图。
- 不把 SP-3/SP-4 写成 G5 通过；不把 SP-6 相关性当门 5 判据；不开始 G6。
- 不把本探针结果外推到其他装备族/运行时/存档。

## 九、实现影响面（评审用）

| 文件 | 变更 |
| --- | --- |
| `src/animation/settings.h` / `settings.cpp` | 新键 `TransitionSubgraphProbe` 解析、日志、getter；默认 false |
| `src/animation/behavior_graph/transition_probe.h/.cpp` | 新 `log_transition_subgraph`；BFR/behavior/NodeList/Blender 偏移常量；cap 与 overflow；全部 guarded read |
| `src/animation/behavior_graph/independent_graph.cpp` | 在 `log_transition_modifiers` 之后调用；仅在 `TransitionSubgraphProbe` 与窗口条件满足时输出 |
| `tests/transition_probe_test.cpp` | 纯函数窗口/上限/状态名用例；不依赖游戏对象 |
| `config/CharacterPanel.ini` | 样例键 + 注释，默认 false |
| `CMakeLists.txt` | 不新增 `.cpp`（复用 `transition_probe.cpp`） |
| `docs/build.md` | v114 构建/部署/候选 run 记录 |

## 十、本契约不证明什么

- 不证明 0→10 的标量一定在 `State3` Blender；只证明布局与入口可审计。
- 不证明候选/源姿态等价；不改变 `G4P-7 FAIL` / G5/G6。
- 不证明 `WeapEquipBFR` 的 behavior 是运行时 clone 而非模板；该问题由 SP-2 的 `active-nodes` 读数回答。
- 不涵盖 `allPartitionMemory`、`hkbBlendingBehavior`、静态附件轴、K2-C1/A17 或源侧。

## 十一、离线审计复现命令

从仓库根目录执行；`disasm_func.py` 是本地 gitignored 审计工具（`build/g1-audit-20261023`）。

```powershell
$exe = 'build/g1-audit-20261023/SkyrimSE.original.exe.unpacked.exe'
python build/g1-audit-20261023/disasm_func.py 0xaccce0   # BFR ctor: +0x48 string, +0x50 behavior
python build/g1-audit-20261023/disasm_func.py 0xaccdf0   # BFR validate: "You must provide a behavior."
python build/g1-audit-20261023/disasm_func.py 0xaccf10   # BFR serialize: [rdi+0x50] as hkbBehaviorGraph
python build/g1-audit-20261023/disasm_func.py 0xae97e0   # ModifierGenerator: +0x50 child, +0x48 modifier
python build/g1-audit-20261023/disasm_func.py 0xae98e0   # ModifierGenerator: "You must specify a child generator."
```

RTTI/vtable 交叉核对（Python，本地 `pe_tools.py`）：

```python
from pe_tools import Image
img = Image(r'build/g1-audit-20261023/SkyrimSE.original.exe.unpacked.exe')
for name, rva in [('BFR', 0x195EA20), ('ModGen', 0x195F1F8),
                  ('Blender', 0x195EB70), ('BehaviorGraph', 0x1941450)]:
    print(name, img.rtti_class(rva))
```

预期：`BFR` 为 `.?AVhkbBehaviorReferenceGenerator@@`，`BehaviorGraph` 为 `.?AVhkbBehaviorGraph@@`。

## 十二、实现与构建记录（2026-11-19）

**源码已实现；离线测试 8/8；Release 构建通过；未部署、未上机。**

| 文件 | 实现 |
| --- | --- |
| `src/animation/settings.h` / `settings.cpp` | `TransitionSubgraphProbe` 解析、日志行、getter；默认 false |
| `src/animation/behavior_graph/transition_probe.h` | v5 容量/窗口常量、`TransitionSubgraphStatus` 名称、`transition_subgraph_window_matches`、`log_transition_subgraph` 声明 |
| `src/animation/behavior_graph/transition_probe.cpp` | BFR `behavior +0x50`、`behaviorName +0x48`、`hkbBehaviorGraph::activeNodes +0x98`/`isActive +0x12C`、`hkbNodeInfo` 0x90 步长；受保护的 `TRANSITION SUBGRAPH`/`NODE`/`SM`/`BLEND`/`CHILD`/`SUMMARY` 输出；cap 8/256/8/8/8 |
| `src/animation/behavior_graph/independent_graph.cpp` | 在 `log_transition_modifiers` 后调用；仅当 `TransitionSubgraphProbe` 且 `K5FullSample` 时输出；`TransitionStateProbe`/`K5FullSample` 缺失时只输出一次 `status=inactive` 提示 |
| `tests/transition_probe_test.cpp` | 新增容量、窗口、状态名 3 例；总计 8/8 |
| `config/CharacterPanel.ini` | 样例键 `TransitionSubgraphProbe=false` + 注释 |

**离线测试**：`transition_probe_test` 8/8，exit 0。**构建**：提交 `9400314` 后干净树 `cmake --preset Release -DCHARACTER_PANEL_BUILD_LABEL=v114-g5trans-subgraph-20261119` + `cmake --build --preset Release`，exit 0；身份 `CharacterPanel-scenegraph-v114-g5trans-subgraph-20261119-94003147d5-cl94faaed0c6-20261119T145712Z`（无 `+dirty`）。

| 工件 | 字节 | SHA-256 |
| --- | ---: | --- |
| CharacterPanel.dll | 2,091,520 | `523b097dfc65ea10becbf4d506b49038b12d1742ab1cb55fc22e3e105dd6fe6e` |
| CharacterPanel.pdb | 52,383,744 | `d954ee51368ae07624793d3c7201297428ca849ca13edb24e89506ba1a250c8f` |

**部署（2026-11-19 23:10）**：v113 三件套备份到 `dist/deploy-backup-20261119-v113-v114pre/`（DLL `228614da…`、PDB `69ab68ad…`、候选 INI `7deb21e5…`，逐位核对）。v114 DLL/PDB + 候选档 `dist/g5-transition-v5-20261119/CharacterPanel-g5trans-candidate.ini`（12,449 B `30aa9777…`，`TransitionStateProbe=true`、`TransitionSubgraphProbe=true`）安装到 `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\`；安装位哈希/身份逐位核对通过（DLL `523b097d…`、PDB `d954ee51…`、INI `30aa9777…`、身份 `…-v114-g5trans-subgraph-20261119-94003147d5-…`）。Skyrim/SKSE 未运行，MO2 打开；未启动游戏。构建记录见 [`docs/build.md#v114-g5trans-subgraph-20261119`](../../build.md#v114-g5trans-subgraph-20261119)。
