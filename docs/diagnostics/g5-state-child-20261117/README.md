# C-G5-STATE v4：blender child / internal-state 只读探针预注册（2026-11-17，APPROVED）

**状态：运行完成（2026-11-17）。CP-1/4/5 PASS，CP-2 FAIL，CP-3 未满足。** v3 在已读范围内未找到逐步变化的标量，其范围为 `hkbNodeInfo` 与 `hkbBlenderGenerator` 对象前 0x100。本轮从 blender 对象的指针字段尝试定位 child/internal-state，首轮 0x200 指针扫描零命中。

## 一、只读边界

- 新增 `[Experiment] StateChildProbe=false`，默认 off；只在候选 K5 快照点调用。
- 仅对 vtable == `VTABLE_hkbBlenderGenerator` 的活动节点扫描对象前 0x200 字节的 qword；每个 qword 只要通过用户态指针过滤，就用 `ReadProcessMemory(GetCurrentProcess(), …)` 安全读取目标首 8 字节，命中 child/internal-state vtable 才缓存。
- 每代对缓存的 child/internal 对象读取 **0x100 字节**（32 qword），逐行记录；读取失败即跳过，不 deref 未验证指针。
- 不写节点/姿态/图变量、不新增 hook、不调用 update/event 入口。

## 二、运行协议

- 构建 v109；运行档 = `AE-Track2`、Save116、`IndependentGraph=inject`、`CandidateEquipPrime=true`、`CandidateEquipPrimeBranch=true`、`CandidateUnequipProbe=true`、`StateChildProbe=true`、`K5FullSample=true`、`K5Reconciliation=true`，其余实验键关闭。
- 一次会话、一次 panel 窗口，走与门 5 候选侧相同的 v96a 链。
- 记录 DLL/PDB/INI/存档身份；日志/CPK5 归档哈希。

## 三、判据

| # | 判据 | 通过条件 | 否则 |
| --- | --- | --- | --- |
| CP-1 身份 | v109 三件套/存档匹配；`StateChildProbe=true` | 全满足 | INVALID |
| CP-2 发现 | 至少一个 `STATE CHILD DISCOVER`，且 children+internal 总数 > 0 | 全满足 | INVALID |
| CP-3 覆盖 | 至少一个 `STATE CHILD` 或 `STATE INTERNAL` 有逐代记录；读取失败单独计数 | 全满足 | INVALID |
| CP-4 采样完整 | K5 249/249，equip 120/120，unequip 129/129 | 全满足 | INVALID |
| CP-5 运行完整 | 一次 panel；零非豁免 `[E]`；无崩溃；预算未超 | 全满足 | INVALID |

**分析目标（非判据）**：在 32 qword 窗口中找与 C-G5-STATE v1 PC1 投影单调相关、且在多个同 localTime 组重复出现的字段；不得用任何字段直接改门 5 判据。

## 四、停止规则与下一步

- 任一 `[E]`、崩溃、身份不符、K5 缺代、发现阶段读取异常 → 停止并保留现场；不重跑同一窗口。
- 若找到单调字段，下一版才可设计“clip 时钟 + 状态标量”的配对预注册；若未找到，登记该对象窗口盲区并转生成输出/官方布局审计。

## 五、v109 运行结果（2026-11-17 23:41–23:44，CP-2 FAIL）

**运行身份**：v109 DLL/PDB/INI 匹配；K5 `249/249`，equip 120/120，unequip 129/129；一次 panel，唯一 `[E]` 为装载期 OAR 豁免行。

| 工件 | 大小 | SHA-256 |
| --- | ---: | --- |
| [CharacterPanel.log](run1/CharacterPanel.log) | 248,085 B | `F0F2DCCCD453CB1D7AD50B8988099F383AEFA831D4C5C22655E7A226F7BFE590` |
| [CPK5 bin](run1/CharacterPanel-g5k5-CharacterPanel-scenegraph-v109-g5state-child-20261117-8ceb48af57-cl94faaed0c6-20261117T153907Z-20261117T154236Z.bin) | 1,410,452 B | `BB0C4A57FCB8C4F46799A9A1645CE04DF0E5C9680930396C5312AEB77D985830` |
| [JSON ledger](run1/CharacterPanel-g5k5-CharacterPanel-scenegraph-v109-g5state-child-20261117-8ceb48af57-cl94faaed0c6-20261117T153907Z-20261117T154236Z.json) | 771 B | `B6D8F53CB7FCCEA28AFF5967D88EBD2F02BDF0ADE9B8C559F339D890527615C5` |

**发现读数**：12 个 `STATE CHILD DISCOVER` 全部 `children=0 internal=0x0`；没有任何 `STATE CHILD`/`STATE INTERNAL` 窗口行。

**结论**：blender 对象前 0x200 字节中的指针没有直接指向 `hkbBlenderGeneratorChild` 或 `hkbBlenderGeneratorInternalState` 的对象；可能 child 是 HKX 数据而不是 runtime 对象，或 internal state 由 `hkbBehaviorGraph` 的分区内存/其他表持有。CP-2 FAIL，本探针没有取得状态标量。

**下一入口**：改查 `hkbBehaviorGraph::allPartitionMemory` / `idToStateMachineTemplateMap` 或直接解析 HKX 的 `hkbBlenderGeneratorChild` 数据；不要重复同一 0x200 指针扫描。

**2026-11-18 复核**。按 §三，CP-3 需要逐代 child/internal 行，而 §五 记录两类行都未产生。0x200 扫描仅匹配窗口中的直接指针。引擎审计确认 Blender 的 children 是指针数组，childrenInternalStates 是内联元素数组，这一轮扫描没有进入两类数组。 [离线审计](../g5-transition-audit-20261118/README.md)
