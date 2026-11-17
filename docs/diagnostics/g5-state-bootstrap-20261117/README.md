# C-G5-STATE v2：源姿态 bootstrap 预注册（2026-11-17，APPROVED）

**状态：COMPLETE（2026-11-17）：SB-1..4 PASS，SB-5 FAIL；bootstrap 被首帧 Generate 覆盖，G4P-7 与门 5 逐位相同。** 本实验检验 [C-G5-STATE v1](../g5-state-20261117/README.md) 的初始状态假设：候选 equip 第一组离轨（最近源残差 max 0.497）是否由候选从不同初始姿态出发造成；若是，一次性把源 `poseLocal` bootstrap 到已激活候选应把第一组拉回源轨迹，并同时消除 H2 静态附件偏移。

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

## 六、实现与部署身份

| 项 | 值 |
| --- | --- |
| 源码提交 | `ee50e97`（`feat(g5): bootstrap the candidate from the source pose`） |
| Build identity | `CharacterPanel-scenegraph-v106-g5state-bootstrap-20261117-ee50e97bf4-cl94faaed0c6-20261117T142733Z`（无 `+dirty`） |
| DLL | 2,035,712 B；SHA-256 `B4BB754B78F5E69B7879CF3FFB1380C14BDC1F7EC8E39D32F204FEB9DC72528E` |
| PDB | 51,712,000 B；SHA-256 `00BE2671D6376CCB04E2BE2C108D7C5E3D7BF6F0A1AB277A75095C35EE3A3CD0` |
| 运行 INI | `dist/g5-state-bootstrap-20261117/CharacterPanel.ini`；SHA-256 `DD7A5E1A52CB4A4A19EA4760CC1DB7179C0E1F2DFA6AAFCB1E9F83C9741347E9` |
| 部署前备份 | `dist/deploy-backup-20261117-bootstrappre/`（v104 DLL `7D270E7D…` / PDB `B2232180…` / INI `BBFE2B8B…`） |

**部署（2026-11-17，已完成）**：安装位 DLL/PDB/INI 与上表逐位一致；`IndependentGraph=inject`、`BootstrapSourcePose=true`、K5 三键 on、其余实验键按 k5-inject 档。**尚未上机。**

## 七、v106 运行结果（2026-11-17 22:31–22:34，SB-5 FAIL）

**运行身份**：v106 DLL/PDB/INI 与 §六逐位一致；日志 `SCENEGRAPH BUILD` = `CharacterPanel-scenegraph-v106-g5state-bootstrap-20261117-ee50e97bf4-cl94faaed0c6-20261117T142733Z`。一次 panel 窗口（`opening=true` 22:32:16.999，`false` 22:34:08.304），无崩溃。

| 工件 | 大小 | SHA-256 |
| --- | ---: | --- |
| [CharacterPanel.log](run1/CharacterPanel.log) | 245,141 B | `E619417AEFF7B92BFF6D7F4772AB838D0F34F0EFACF5C2951701C3F43D03E0CC` |
| [CPK5 bin](run1/CharacterPanel-g5k5-CharacterPanel-scenegraph-v106-g5state-bootstrap-20261117-ee50e97bf4-cl94faaed0c6-20261117T142733Z-20261117T143217Z.bin) | 1,410,452 B | `183664CEA0F076227434C90EE23B9088C1BDE7B40B86EC3C268C862F8CCDFD14` |
| [JSON ledger](run1/CharacterPanel-g5k5-CharacterPanel-scenegraph-v106-g5state-bootstrap-20261117-ee50e97bf4-cl94faaed0c6-20261117T142733Z-20261117T143217Z.json) | 771 B | `0BC25F89A7A3A4B5EF27EF2DDDAFB62946FFA62ECD4DD41390F97110758D9F26` |

**bootstrap 读数**：

```text
BOOTSTRAP POSE actor=00000014 request=1 outcome=copied bones=116
source-hash=cffe2735eb028f8f
candidate-before=bc160a8c2b3a5a58
candidate-after=cffe2735eb028f8f equal=1
```

`K5 RECON side=candidate expected=249 attempted=249 recorded=249 missing=0 ... equip=120/120 unequip=129/129`；唯一 `[E]` 为装载期 OAR 豁免行。

### 7.1 SB 判据

| # | 结果 |
| --- | --- |
| SB-1 身份 | PASS |
| SB-2 bootstrap | PASS（copied 116/116，after==source） |
| SB-3 采样完整 | PASS（249/249，120/120，129/129） |
| SB-4 运行完整 | PASS（一次 panel，零非豁免 `[E]`，无崩溃） |
| SB-5 初始状态效果 | **FAIL** |
| SB-6 G4P-7 | **FAIL**（与门 5 相同） |

### 7.2 关键负结果

正式 G4P summary 与门 5 **逐字节一致**（manifest 因 candidate identity/路径不同而哈希不同，summary 相同）：`exceed tau_p/r/s = 3,826 / 16,755 / 352`，P99 `5.5912 / 0.0457227 / 0.000525653`。

逐位比较 v106 与门 5 候选 CPK5 的 249 条记录、116×10 分量：**288,840/288,840 分量完全相同（0 diff，max-abs=0）**。bootstrap 的源姿态副本在首帧 Generate 后被候选图完全重新生成，未进入任何 K5 记录。

因此 SB-5 FAIL：equip 第一组仍是离轨初始状态，静态附件仍是 H2 偏移；一次性 poseLocal 复制不能改变候选图的内部初始状态。

**裁定：bootstrap 负结果成立。** 初始状态位于图内部（active nodes / blend state / weights），不只是 poseLocal 缓冲；下一版必须初始化图状态或采集单标量 blend/权重，不能继续在 pose 缓冲上打补丁。
