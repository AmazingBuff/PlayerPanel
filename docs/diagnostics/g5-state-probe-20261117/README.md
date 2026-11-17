# C-G5-STATE v3：图内部状态只读探针预注册（2026-11-17，APPROVED）

**状态：APPROVED，待实现/构建/上机。** C-G5-STATE v1 已证明同一 clip localTime 下的候选姿态沿单标量轴变化（PC1 0.9983–0.999998），C-G5-STATE v2 证明 poseLocal bootstrap 会被首帧 Generate 覆盖。本探针读取候选 `hkbBehaviorGraph::activeNodes` 的已知 `hkbNodeInfo` 布局，寻找与该标量同步变化的节点字段。

## 一、只读边界

- 新增 `[Experiment] StateProbe=false`，默认 off；只在候选 K5 快照点调用。
- 每个候选 generation 遍历 `activeNodes`（cap 512），读取 `hkbNodeInfo`（CommonLib 已审计 0x90 布局）的 18 个 qword；这是节点**状态条目**，不是未知节点对象布局。
- 只读：不写 poseLocal、不写图变量、不写节点、不新增 hook、不调用 update/event 入口。
- 仅记录发生变化的状态条目；首次出现记一次；跳过 `clip` 与 `state-machine`（clip time 已由 K5 记录，状态机转移不是 blend 标量）；每 generation 最多 32 条明细 + 一条 `changed/suppressed` 汇总，防止日志爆炸。

## 二、运行协议

- 构建 v107；运行档 = `AE-Track2`、Save116、`IndependentGraph=inject`、`CandidateEquipPrime=true`、`CandidateEquipPrimeBranch=true`、`CandidateUnequipProbe=true`、`BootstrapSourcePose=false`、`K5FullSample=true`、`K5Reconciliation=true`、`StateProbe=true`，其余实验键关闭。
- 一次会话、一次 panel 窗口；不改变门 5 的链和停止规则。
- 记录 DLL/PDB/INI/存档身份；日志/CPK5 归档哈希。

## 三、判据

| # | 判据 | 通过条件 | 否则 |
| --- | --- | --- | --- |
| SP-1 身份 | v107 三件套/存档匹配；`StateProbe=true` 实际加载 | 全满足 | INVALID |
| SP-2 探针覆盖 | 至少一个 `STATE PROBE` 条目；`activeNodes` 可读；无溢出/不可读 | 全满足 | INVALID |
| SP-3 采样完整 | K5 249/249，equip 120/120，unequip 129/129 | 全满足 | INVALID |
| SP-4 运行完整 | 一次 panel；零非豁免 `[E]`；无崩溃；预算未超 | 全满足 | INVALID |
| SP-5 标量候选 | 离线统计中至少一个 qword/字段在同一 localTime 组内随记录顺序单调变化，且与 C-G5-STATE v1 的 PC1 投影显著相关 | 记录结论 | 报告未找到；不改变 G4P-7 |

**注意**：SP-5 是探索性判据，不是 G5 通过条件；即使找到标量，仍需新的配对/状态协议。

## 四、停止规则

- 任一 `[E]`、崩溃、身份不符、K5 缺代、`activeNodes` 不可读 → 停止并保留现场。
- 不删除/重排 activeNodes，不写节点字段，不扩大探针到未知对象布局。

## 五、下一步

若找到单调 qword/字段：将其登记为候选状态标量，下一版预注册“clip 时钟 + 状态标量”的配对规则；若未找到，则记录该布局盲区，转 runtime hook/官方布局审计。
