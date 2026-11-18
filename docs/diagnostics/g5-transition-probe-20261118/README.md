# C-G5-TRANS v1：候选/源过渡与 Blender 状态只读探针预注册（2026-11-18）

**状态：PRE-REGISTERED（草案，待评审）；未实现、未构建、未部署、未上机。** 新键默认 `false`，只读，不触碰产品路径；本契约不改变门 5 的 `G4P-7 FAIL` 裁定，不产生 `tau_*`，不关闭 G5/G6。

## 一、背景与要回答的问题

门 5 正式批次 248 对，`G4P-7 FAIL`（`E_p/E_r/E_s` 超差 3,826/16,755/352）。事后归因把残余分成三个轴：

- 5 个静态附件骨的真实装备状态（H2 已支持，1,240/3,826 次 `E_p`）；
- 同一 clip `local_time` 下仍有多条不同姿态，PC1 占比 0.9983–0.999998，指向一个未采集的 blend/权重标量；
- equip 第一组（`local_time=0.208333`）离源轨迹最远，max 最近源残差约 0.497。

[过渡离线审计](../g5-transition-audit-20261118/README.md) §六已用精确哈希的 `0_Master.hkx`/`weapequip.hkx` 确认：

- 本次 `0→10` 装备边是 `hkbBlendingTransitionEffect`，作者 `duration=0.2 s`、`startFrac=0`、smooth、self-reset；
- `1HM_Equip` 不是独立 transition，而是 `Weap_EquipDefaultState/msg/State3` 的 `hkbBlenderGenerator` 分支；`blendParameter=1`、区间 `[0,1]`，child 0 为 `2HC_Equip.hkx`，child 1 为 `State3/b` 状态机（`startStateId` 绑定 `iLeftHandEquipped_DG`，state id=1 为 `1HM_Equip.hkx`）；
- 两个 dump 内没有 `hkbGeneratorTransitionEffect`。

现有 K5（CPK5/CPK5S）只记录 clip 时钟和姿态，**没有记录**运行时 `timeInTransition`、`endIntervalWeight`、Blender child weight。本探针就是补这四个读数，并把它们与现有 K5 记录逐条对齐。

## 二、假设与可否证条件

| 编号 | 假设 | 可否证读数 |
| --- | --- | --- |
| H-T1 | 候选/源 equip 的 `0→10` 边在 completed/seq 1–13（源为装备起始窗口）实际激活，`timeInTransition` 随步推进并与 0.2 s 对齐 | 13/9 条窗口内没有任何 active blending transition，或 `timeInTransition` 不单调/不推进 |
| H-T2 | `Weap_EquipDefaultState/msg/State3` 的 Blender（`blendParameter`、`endIntervalWeight`、child weight）是同一 `local_time` 组内 PC1 的单标量来源 | 窗口内有 Blender 但三个读数逐 step 全不变，或与 PC1 投影无单调关系 |
| H-T3 | 两侧作者结构相同（哈希一致），差异来自运行时状态/计时，而不是 HKX 字段不同 | 两侧同名字段/结构在探针中系统性不同，且不能用状态/计时解释 |

任一假设被否证，不靠同一探针重跑“碰”结果；按 §八的分叉进入下一层（`allPartitionMemory`/HKX partition 或静态附件轴）。

## 三、范围与只读边界

- 新增 `[Experiment] TransitionStateProbe=false`，默认 off；只在既有 K5 采样点调用，**不新增 detour、不新增 hook、不写图/姿态/变量、不分发事件**。
- 候选侧要求 `K5FullSample=true`；源侧要求 `SamplingLiveness=on` 且 `K5FullSample=true`。任一前置不满足时，只输出一行 `TRANSITION PROBE status=inactive reason=...`，不产生逐代记录。
- 源侧在既有 63856 apply-return detour 内只做固定大小 memcpy 入环；所有字符串格式化和 `logger::info` 都在渲染线程 drain 中完成。候选侧沿用既有 K5 采样点，直接格式化。
- 记录上限固定并有显式 overflow；任何 overflow、ring drop、身份不符、K5 缺代或多次窗口都使该批 INVALID，不静默截断。

## 四、采样点与关联键

| 侧 | 代码点 | 关联键 | 现有行为 |
| --- | --- | --- | --- |
| 候选 | [`independent_graph.cpp`](../../../src/animation/behavior_graph/independent_graph.cpp) K5 `e_post_sample` 分支，`k5_writer.append` 之后、`log_node_state_probe` 之前 | `side=candidate, segment, completed` | 与 CPK5 候选记录同代；不改推进/事件顺序 |
| 源 | [`probe_liveness.cpp`](../../../src/animation/behavior_graph/probe_liveness.cpp) `record_k3s_return` 的 K5 分支，`K5SourceRecord` 填充之后 | `side=source, seq` | 与 CPK5S 源记录同代；入独立 ring，渲染 drain 输出 |

候选段名沿用 `equip`/`unequip`；源侧沿用 K5S 的 `seq` 与 `tsc`。探针记录必须与同名 K5 记录一一配对；缺一条即 TP-2 失败。

## 五、观测 schema

### 5.1 枚举与安全

从 `behavior->activeNodes` 枚举（cap 512，和 K5 审计一致）。每个节点先通过用户态指针过滤，再读 vtable；只有精确匹配下列 vtable 才读布局：

| 类 | 识别 | 依据 |
| --- | --- | --- |
| `hkbStateMachine` | `RE::VTABLE_hkbStateMachine[0].address()` | CommonLibSSE 现有常量 |
| `hkbBlendingTransitionEffect` | `RE::VTABLE_hkbBlendingTransitionEffect[0].address()` | CommonLibSSE 现有常量；引擎审计 `0x19637E8` |
| `hkbBlenderGenerator` | `REL::Offset(0x195EB70).address()`（static） | AE 1.6.1170 RTTI：`hkbBlenderGenerator` TD `0x20b9818`、COL `0x1b9d760`、vtable `0x195eb70`；v103 日志 `0x7ff6a662eb70` 与推测基址 `0x7ff6a64d0000` 差值正好 `0x195eb70` |

`hkbBlenderGenerator` 没有 CommonLibSSE 头/`VTABLE` 常量，故用 `REL::Offset`；这是 AE 1.6.1170 专用偏移，构建身份必须带 runtime 1-6-1170-0。不得把该偏移外推到其他运行时。

### 5.2 状态机

对每个 `hkbStateMachine`：

- `currentStateID` `+0x80`（int32）；
- `activeTransitions` `+0xB0` 是 `hkArray` 头：data `+0x00`、size `+0x08`、capacity `+0x0C`；
- **实际元素步长 0x28**（CommonLibSSE 把元素声明为 `hkRefVariant`/8 B，不能按该声明索引第二条及以后）；
- 每个元素首 qword `data[i*0x28+0x00]` 视为 effect 指针；指针过滤后读 vtable。

只有 vtable 精确等于 `RE::VTABLE_hkbBlendingTransitionEffect[0].address()` 时才读：

| 字段 | 偏移 | 记录 |
| --- | ---: | --- |
| `duration` | `+0x50` | float bits + `.6f` |
| `toGeneratorStartTimeFraction` | `+0x54` | float bits |
| `flags` | `+0x58` | uint16 |
| `endMode` | `+0x5A` | int8 |
| `blendCurve` | `+0x5B` | int8 |
| `selfTransitionMode` | `+0x48` | int8 |
| `eventMode` | `+0x49` | int8 |
| `timeRemaining` | `+0x80` | float bits + `.6f` |
| `timeInTransition` | `+0x84` | float bits + `.6f` |

`fromGenerator`/`toGenerator` 是 `SERIALIZE_IGNORED` 的运行时私有字段，本探针不读、不猜；用状态机 `currentStateID` + 节点指针/名称做关联。

### 5.3 Blender

对每个 `hkbBlenderGenerator`：

| 字段 | 偏移 | 记录 |
| --- | ---: | --- |
| `blendParameter` | `+0x4C` | float bits + `.6f` |
| `children` 数组头 | `+0x60` | data/size/capacity |
| `childrenInternalStates` 数组头 | `+0x70` | data/size/capacity；前 4 项按 uint16 原样记录，不解释 |
| `endIntervalWeight` | `+0x90` | float bits + `.6f` |
| `numActiveChildren` | `+0x94` | int32 |
| `beginIntervalIndex` | `+0x98` | int16 |
| `endIntervalIndex` | `+0x9A` | int16 |

`children` data 是 child 指针数组（每项 8 B）。每个 child 指针过滤后读：`generator +0x30`、`weight +0x40`、`worldFromModelWeight +0x44`。child 的 generator 只记指针、vtable 与 class（clip/state-machine/blender/manual-selector/unknown）；不递归展开其子树，避免在图内成环。运行时没有 HKX 路径，目标 `msg/State3` 用节点 `name`、child 0 的 `2HC_Equip` clip 身份、child 1 的 state-machine + `1HM_Equip` 子节点身份三者对齐。

### 5.4 记录与预算

建议固定记录 `TransitionProbeRecord`（实现预注册时冻结字节布局与 `static_assert`）：

- 头：magic/version、side、segment、flags、seq/completed、tsc、三个计数、overflow 位；
- 最多 **4** 个 state machine，每个：node、vtable、`currentStateID`、transition 数、name[96]；
- 最多 **4** 个 blending effect，每个：effect、vtable、duration/startFrac/timeRemaining/timeInTransition bits、flags/endMode/blendCurve/self/event；
- 最多 **4** 个 Blender，每个：node、vtable、blendParameter/endIntervalWeight bits、numActiveChildren、begin/end interval、children 数、name[96]、前 4 个 internal uint16；
- 每个 Blender 最多 **4** 个 child，每个：generator、vtable、weight/wfm bits、class。

超过 4 个 state machine / 4 个 effect / 4 个 Blender / 4 个 child 时置对应 overflow 位并计数；该记录 INVALID。源 ring 容量 **2,048** 条，drop 必须为 0；候选直接输出，记录数必须等于 K5 记录数。日志每记录最多约 20 行；候选 249、源 849 的预算下总行数约 2–3 万行，留在常规日志预算内。

## 六、运行协议

### 6.1 候选侧

1. 构建 v110 三件套并做离线测试；按惯例备份当前安装（v104 + CAL2 grid 档），安装 v110 DLL/PDB 和 `k5-transition-candidate` INI。
2. 启动 MO2 `AE-Track2` → SKSE；`SCENEGRAPH BUILD` 必须含 v110 身份。载入 **Save116**，不改装备、不存档、不读档。
3. 键位：`IndependentGraph=inject`、`CandidateEquipPrime=true`、`CandidateEquipPrimeBranch=true`、`CandidateUnequipProbe=true`、`K5FullSample=true`、`TransitionStateProbe=true`；`K5LayerAudit/K5Reconciliation=false`，`DisplayPose/DisplayStep/SourceTimeline/SamplingLiveness/OarContextWatch/ObservationSnapshot/ControlSampleGrid=false`。
4. 打开一次 panel，等待 equip 120/120 → settle → `Unequip` 144 → unequip 129；关闭 panel，退出。

### 6.2 源侧

1. 同一 v110 DLL/PDB，部署 `k5-transition-source` INI；`IndependentGraph=off`、`SamplingLiveness=on`、`K5FullSample=true`、`TransitionStateProbe=true`，其余实验键关闭。
2. 载入 Save116；按 F7 武装 20 秒窗口；自然拔剑 → 保持 → 收剑，覆盖六个可配对 clip 族；退出。
3. 两侧必须分开进程/会话；不把候选 inject 与源自然游玩混在同一会话（源在 inject 期间被暂停，混合会产生错误关联）。

## 七、判据（预注册，不得事后放宽）

| 编号 | 判据 | 通过条件 | 否则 |
| --- | --- | --- | --- |
| TP-1 身份与覆盖 | v110 DLL/PDB/INI/存档匹配；两侧各一次窗口；候选 249/249、源按 K5 close 行全量；零非豁免 `[E]` | 全满足 | INVALID |
| TP-2 记录配对 | 每条 K5 记录恰有一条同 side/segment/seq 的探针记录；ring drop=0、overflow=0 | 全满足 | INVALID |
| TP-3 0→10 目击 | 候选 equip completed 1–13 至少一条 state machine 记录含 `currentStateID=10` 且 active transition 的 effect vtable 为 blending；源装备起始窗口（seq 137–145）同样至少一条 | 全满足 | 该假设未目击 |
| TP-4 计时推进 | 同一 state machine/effect 连续出现时 `timeInTransition` 非递减；最后一个 active 记录 `timeInTransition >= duration - 1/60`；`timeRemaining` 仅作交叉核对；保留 bits | 全满足 | H-T1 否证/需换层 |
| TP-5 Blender 目击 | 候选 equip 1–13 至少一条与 `msg/State3` 对齐的 Blender 记录 `children >= 2`、`blendParameter∈[0,1]`；`blendParameter`/`endIntervalWeight`/child weight 至少一项在 1–13 或同 localTime 组间变化；源窗口同样 | 全满足 | H-T2 未目击 |
| TP-6 相关性（分析目标，不是门 5 判据） | 对每个同 localTime 多记录组，计算标量与 C-G5-STATE v1 PC1 投影的 Pearson/Spearman 与单调性；两侧分别报告 | 只报告，不设通过阈值 | — |
| TP-7 无副作用 | 无图/姿态/变量写入；无新 detour/hook；`source-drift=none`；产品路径 off | 全满足 | INVALID |

TP-3/TP-4/TP-5 任一未目击时，不得用“探针没找到”直接判候选/源失败；按 §八进入下一层。TP-6 的强单调关系只支持下一版配对协议，不回填本门 5。

## 八、分析与分叉

1. **TP-3/TP-4 成立且 TP-6 单调**：0.2 s transition 是首段计时轴；起草 `C-G5-PAIR v1`（clip 时钟 + transition/Blender 标量）作为**新**预注册，用新批次验证；本批 G4P-7 仍 FAIL。
2. **TP-3 成立但 TP-4 不成立**：transition 存在但不是姿态计时轴；检查 effect 是否在采样前已结束、`timeRemaining` 语义、target generator 的 start/lifetime，或换另一层效果。
3. **TP-5 成立但 TP-6 不成立**：Blender 标量存在但不解释姿态；转 `hkbBehaviorGraph::allPartitionMemory` / `idToStateMachineTemplateMap`、父级/COM 或 HKX partition。
4. **TP-3/TP-5 都未目击**：不再重复 0x200 直接指针扫描；按 v109 的下一入口查 partition memory/模板映射或直接解析 HKX 的 `hkbBlenderGeneratorChild` 数据。
5. **静态附件轴独立**：5 骨的 1,240 次 `E_p` 超差另行预注册“候选真实附件状态”或“静态附件独立相等轴”；不能靠删骨/放宽 `tau` 通过。

## 九、停止规则与明确不做

- 任一 `[E]`、崩溃、身份不符、K5 缺代、探针 overflow/drop、多次窗口 → 停止并保留现场；同一会话不重跑同一窗口。
- 不写图/姿态/变量；不新增 detour/hook；不改事件顺序/推进预算；不改产品路径。
- 不产生 `tau_*`、不修改 `epsilon_t`、不删轨、不把 TP-6 相关性当作 G5 通过条件。
- 不把本探针结果外推到其他装备族/其他运行时/其他存档；源与候选必须分开进程。
- G6（源恢复、请求/回调失效、重复会话资源收敛）仍后置；装载 teardown 仍 NO-GO。

## 十、实现影响面（评审用）

| 文件 | 变更 |
| --- | --- |
| `src/animation/settings.h` / `settings.cpp` | 新键 `TransitionStateProbe` 解析、日志、getter；默认 false |
| 新 `src/animation/behavior_graph/transition_probe.h/.cpp` | 固定记录、vtable/offset 常量、`fill_transition_probe`（无分配）、`format_transition_probe`（只在 drain 调用） |
| `src/animation/behavior_graph/independent_graph.cpp` | 候选 K5 点调用 fill/format；不改变 K5 append 与推进顺序 |
| `src/animation/behavior_graph/probe_liveness.cpp` | 源侧独立 ring（2,048 条）+ 渲染 drain + close 行计数；不改 CPK5/CPK5S 记录布局 |
| `CMakeLists.txt` | 加入新 `.cpp`（显式 sources 列表） |
| `tests/transition_probe_test.cpp` | 记录布局/overflow/空数组/名称截断的离线用例；不依赖游戏 |
| `config/CharacterPanel.ini` | 样例键 + 注释，默认 false |
| `docs/build.md` / 测试记录 | v110 构建、部署、两次运行与哈希 |

## 十一、本契约不证明什么

- 不证明候选/源姿态等价；不改变 `G4P-7 FAIL` 或 G5/G6 状态。
- 不证明 0.2 s transition 是唯一原因；不证明 Blender 标量是唯一自由度。
- 不证明静态附件、K2-C1、A17 独立控制或 CAL1 完整资格。
- 不把作者 HKX 字段当作运行时读数；所有运行时结论只来自本探针的新采集。
