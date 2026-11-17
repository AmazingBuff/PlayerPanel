# C-G5-STATE v3：图内部状态只读探针预注册（2026-11-17，APPROVED）

**状态：COMPLETE（2026-11-17）：SP-1..4 PASS；SP-5 未找到。** C-G5-STATE v1 已证明同一 clip localTime 下的候选姿态沿单标量轴变化（PC1 0.9983–0.999998），C-G5-STATE v2 证明 poseLocal bootstrap 会被首帧 Generate 覆盖。本探针读取候选 `hkbBehaviorGraph::activeNodes` 的已知 `hkbNodeInfo` 布局和 blender 对象前 0x100 字节，寻找与该标量同步变化的字段；结果是**两者都没有逐 step 变化**，标量不在这些已观察区域。

## 一、只读边界

- 新增 `[Experiment] StateProbe=false`，默认 off；只在候选 K5 快照点调用。
- 每个候选 generation 遍历 `activeNodes`（cap 512），读取 `hkbNodeInfo`（CommonLib 已审计 0x90 布局）的 18 个 qword；这是节点**状态条目**，不是未知节点对象布局。对已识别为 `blender`/`blender-child` 的节点，额外读取节点对象前 0x100 字节（32 qword）作为有界对象窗口；该窗口只用于定位候选状态字段，尚未解释为已审计布局。
- 只读：不写 poseLocal、不写图变量、不写节点、不新增 hook、不调用 update/event 入口。
- 仅记录发生变化的状态条目；首次出现记一次；跳过 `clip` 与 `state-machine`（clip time 已由 K5 记录，状态机转移不是 blend 标量）；每 generation 最多 128 条明细 + 一条 `changed/suppressed` 汇总。`STATE OBJECT` 只针对 blender/child，单独变化才记录。

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

## 六、实现与部署身份

| 项 | 值 |
| --- | --- |
| 源码提交 | `705a3b1`（`feat(g5): probe the candidate active-node state`） |
| Build identity | `CharacterPanel-scenegraph-v107-g5state-probe-20261117-705a3b1d18-cl94faaed0c6-20261117T151122Z`（无 `+dirty`） |
| DLL | 2,039,808 B；SHA-256 `810DC008271EC01A9C53D571D11D2F61805C52CD34C11E4F70160019C781524C` |
| PDB | 51,752,960 B；SHA-256 `1AFBE7502C8D0B5ED004A928CD866B31A1EEB5276F2D62743996F87859506DE6` |
| 运行 INI | `dist/g5-state-probe-20261117/CharacterPanel.ini`；SHA-256 `333AECB251221E318E1EF7C4B0829F081E55771C012B0E0D9575ECFE59191351` |
| 部署前备份 | `dist/deploy-backup-20261117-stateprobepre/`（v104 DLL `7D270E7D…` / PDB `B2232180…` / INI `BBFE2B8B…`） |

**部署（2026-11-17，已完成）**：安装位 DLL/PDB/INI 与上表逐位一致；`StateProbe=true`、`BootstrapSourcePose` 默认 off、K5 三键 on。**尚未上机。**

## 七、v108 运行结果（2026-11-17 23:25–23:31，SP-5 未找到）

**运行身份**：v108 DLL/PDB/INI 与 §六逐位一致（v108 build `…6444a21271…`）；日志 `SCENEGRAPH BUILD` = `CharacterPanel-scenegraph-v108-g5state-probe-object-20261117-6444a21271-cl94faaed0c6-20261117T152624Z`。一次 panel 窗口，无崩溃；唯一 `[E]` 为装载期 OAR 豁免行。

| 工件 | 大小 | SHA-256 |
| --- | ---: | --- |
| [CharacterPanel.log](run2/CharacterPanel.log) | 1,488,562 B | `0717E40E38DA9E2CFA0DA6710BD1F767DF698A8FCC6935EE3F4234D699B2886A` |
| [CPK5 bin](run2/CharacterPanel-g5k5-CharacterPanel-scenegraph-v108-g5state-probe-object-20261117-6444a21271-cl94faaed0c6-20261117T152624Z-20261117T153038Z.bin) | 1,410,452 B | `CCB1E75094CFAC5C4FFE9E936594C05EBC5A26378EFFB7283B66F8E6792A23F3` |
| [JSON ledger](run2/CharacterPanel-g5k5-CharacterPanel-scenegraph-v108-g5state-probe-object-20261117-6444a21271-cl94faaed0c6-20261117T152624Z-20261117T153038Z.json) | 771 B | `F9BB4569695478C22C7074DCC323F3CB757537A13718090EEDF91A5778FCD6C7` |

**探针读数**：

- `STATE PROBE` 共 2,729 行（equip 1,391 / unequip 1,338），`STATE OBJECT` 共 12 行（equip 5 / unequip 7）。
- equip 只在 31 个 completed 点出现 nodeInfo 变化；同 localTime 组 `0.208333` 的 completed 2–12 中，只有 1/2/13 等点有状态行，中间 3–12 **没有 nodeInfo 变化**。
- `STATE OBJECT` 的 equip 变化点只有 1、87 等少量栈切换点，没有逐 generation 的权重 ramp。

**结论：SP-5 未找到。** `hkbNodeInfo` 和 `hkbBlenderGenerator` 对象前 0x100 字节都不包含随 C-G5-STATE v1 标量逐 step 变化的字段；该标量应在未观察的 `hkbBlenderGeneratorInternalState`、child 对象权重或生成输出中。该负结果不改变 G4P-7 FAIL/G5 状态。

**下一入口**：C-G5-STATE v4 只读探测 `hkbBlenderGeneratorInternalState`/child 对象，或直接读取生成输出；在拿到状态标量前，不能把它接进配对判据。
