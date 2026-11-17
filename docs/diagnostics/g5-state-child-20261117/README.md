# C-G5-STATE v4：blender child / internal-state 只读探针预注册（2026-11-17，APPROVED）

**状态：APPROVED，待构建/上机。** v3 已排除 `hkbNodeInfo` 与 `hkbBlenderGenerator` 对象前 0x100；本探针从 blender 对象的指针字段中发现 `hkbBlenderGeneratorChild` / `hkbBlenderGeneratorInternalState`，再读取其 0x100 字节窗口，定位随过渡标量变化的权重/内部状态。

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
