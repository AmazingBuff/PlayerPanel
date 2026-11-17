# C-G5-STATE v2：源姿态 bootstrap 预注册（2026-11-17，APPROVED）

**状态：APPROVED / IMPLEMENTED（v106），一次正式运行前。** 本实验检验 [C-G5-STATE v1](../g5-state-20261117/README.md) 的初始状态假设：候选 equip 第一组离轨（最近源残差 max 0.497）是否由候选从不同初始姿态出发造成；若是，一次性把源 `poseLocal` bootstrap 到已激活候选应把第一组拉回源轨迹，并同时消除 H2 静态附件偏移。

## 一、固定语义

- 新增 `[Experiment] BootstrapSourcePose=false`，默认 off；仅 `IndependentGraph=drive|inject` 生效。
- 在候选 58453 激活完成、`poseLocal` 已分配之后，**注入/驱动之前**，一次性复制源 `hkbCharacter::poseLocal` 到候选的同一数组：
  - 两侧 `numPoseLocal` 必须相等（116）且 ≤256；
  - 源 116×12 分量必须全部有限；
  - `std::memcpy` 全部 116 个 `hkQsTransform`；
  - 不写图变量、不写 activeNodes、不新增 hook、不逐帧复制。
- 日志：`BOOTSTRAP POSE actor=… request=… outcome=copied bones=… source-hash=… candidate-before=… candidate-after=… equal=1`；失败记 `outcome=skip reason=…`。
- 这是一次性初始条件，不是每帧姿态注入；不得把它描述为候选图独立生成源姿态。

## 二、运行协议

- 构建 v106；运行档 = `AE-Track2`、Save116、`IndependentGraph=inject`、`CandidateEquipPrime=true`、`CandidateEquipPrimeBranch=true`、`CandidateUnequipProbe=true`、`BootstrapSourcePose=true`、`K5FullSample=true`、`K5Reconciliation=true`，其余实验键关闭。
- 一次会话、一次 panel 窗口，走与门 5 候选侧相同的 v96a 链；不重复/不换档。
- 记录 DLL/PDB/INI/存档身份；日志与 CPK5/JSON 归档哈希。

## 三、判据

| # | 判据 | 通过条件 | 否则 |
| --- | --- | --- | --- |
| SB-1 身份 | v106 DLL/PDB/INI/存档与登记一致；`BootstrapSourcePose=true` 实际加载 | 全满足 | INVALID |
| SB-2 bootstrap | `outcome=copied bones=116 equal=1`；候选 after-hash == source-hash | 全满足 | INVALID |
| SB-3 采样完整 | K5 `249/249`，equip 120/120，unequip 129/129，`missing=0` | 全满足 | INVALID |
| SB-4 运行完整 | 一次 panel；零非豁免 `[E]`；无崩溃；预算未超 | 全满足 | INVALID |
| SB-5 初始状态效果 | 重新计算的 equip 第一组最近源 body 最大残差 ≤ **0.05**；H2 静态 5 骨在 pair 0 的 `E_p` ≤ **0.01** | 全满足 | 如实报告 bootstrap 未解决初始状态；不得改阈值 |
| SB-6 G4P-7 描述 | 用冻结 tau/coverage/epsilon 重跑正式配对；报告计数，不以 SB-5 代替 G4P-7 | 记录 | 不改变 G5 状态 |

**成功语义**：SB-1..5 全过 = 初始状态假设得到支持；SB-6 若仍 FAIL，G5 仍不通过，K5-5/层权重与静态附件轴仍需处理。

## 四、停止规则

- bootstrap skip、计数不符、源非有限、身份不符、`[E]`、崩溃、重复 panel、K5 缺代 → 停止，保留现场；不重跑同一窗口、不换阈值。
- 本实验不产生 `tau_*`、不删除骨、不声称 bootstrap 等于姿态等价。

## 五、实施顺序

1. 实现 + `/W4 /WX` 构建 + 引擎无关检查。
2. 部署 v106 + `BootstrapSourcePose=true` 运行档。
3. 一次 Save116 panel 运行并归档。
4. 重跑 gate4_pair / C-G5-ATT / C-G5-STATE 审计，按 SB-1..6 判定。
