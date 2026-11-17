# C-G5-SETUP v1：候选/源 setup 与静态附件只读审计（2026-11-17，DRAFT）

**状态：IMPLEMENTED / DEPLOYED（2026-11-17），尚未上机采集。** v105 只读 `SetupAudit` 已实现、构建并部署；本预注册只回答门 5 G4P-7 失败归因中的 setup/静态附件假设；不改变门 4/门 5 判据，不产生 `tau_*`，不关闭 G5/G6。

## 一、背景

[门 5 正式运行](../g5-gate5-20261116/README.md) 的 G4P-7 FAIL；[事后归因](../g5-attribution-20261117/README.md) 显示：

| 组 | 骨数 | `E_p` 超 tau | 其中 static_both |
| --- | ---: | ---: | ---: |
| body | 88 | 1,226 | 0 |
| attachment | 11 | 2,600 | 1,240 |
| auxiliary A17 | 17 | 0 | 0 |

5 个静态附件骨在两侧几乎不动（span ≤ 1.7e-4），但跨侧平移相差 3.49–5.59：`Quiver`(60)、`WeaponAxe`(61)、`WeaponBow`(63)、`WeaponDagger`(64)、`WeaponSword`(66)。它们贡献 1,240/3,826 次 `E_p` 超容差。

现有事实：`compare-loaded setup=different bindings=same behavior-data=same`；候选由 63949 自建 setup，源使用自身 actor setup。该差异是已知设计事实，但“setup 不同”是否直接造成上述 5 骨的常量偏移，尚未用 setup 自己的 skeleton/bind 数据证明。

## 二、假设

- **H1（setup/bind 默认位）**：候选与源的 `hkbCharacterSetup::animationSkeleton` 或 bind pose 在这 5 骨上不同，导致候选静态节点保持候选默认值，而源静态节点保持源默认值。
- **H2（真实装备状态）**：两侧 setup/bind 相同或近似，但源的实际装备/附件对象把这 5 个节点写到装备状态；候选只注入装备变量，没有真实附件对象，故保持 bind。
- **H3（层/时钟残余）**：上述都不是常量偏移的主因，差异来自动态层/父级/时钟；该分支保留 body 1,226 与 dynamic attachment 1,360 的现有缺口。

三者不互斥；本审计只做归属，不预设结论。

## 三、只读仪器（待批准后实现）

新增 `[Experiment] SetupAudit=false`，默认 off；只在候选装载完成后、任何注入/驱动之前记录一次。

每个运行记录：

1. 源/候选 `BShkbAnimationGraph`、`hkbCharacter`、`hkbCharacterSetup` 指针；`setup` 的 `animationSkeleton`、`unscaledAnimationSkeleton`、`data`、`mirroredSkeleton` 指针；骨架骨数。
2. 固定观测骨集：`{0,4,43,44,45,60,61,63,64,66}`（root、COM、Weapon、Pauldron、5 个静态附件骨）以及两个对照 `{1,35}`（Look/Neck）。每骨从 `hkaSkeleton::referencePose` 记录 10 个 float32（translation xyz、rotation xyzw、scale xyz；`hkQsTransform` 的 scale.w 忽略）与有限性检查；`referencePose.size()` 必须等于骨架骨数，否则 INVALID。
3. 只读；不写 poseLocal、不写图变量、不新增 hook、不调用事件/驱动入口。

界：每骨 10 分量 × 12 骨 × 2 侧 = 240 个 float；单行 ≤ 8 KiB；超界即 INVALID。

## 四、运行协议

- 构建带 `SetupAudit` 的新 DLL；运行档 = `AE-Track2`、Save116、`IndependentGraph=load`、K5 三键 off、`SetupAudit=true`，其余实验键关闭。`load` 只装载候选、不激活/不驱动；审计在装载后、激活前完成，因此不需要把动态链混入 setup 读数（这是对 DRAFT 中 `inject` 的最小化修订，发生在任何采集之前）。
- 一次会话、一次 panel 窗口；不采姿态、不跑 equip/unequip 链（避免把动态差异混入 setup 读数）。
- 记录 DLL/PDB/INI/存档身份；日志/工件归档哈希。

## 五、判据

| # | 判据 | 通过条件 | 否则 |
| --- | --- | --- | --- |
| SA-1 身份 | DLL/PDB/INI/存档与登记逐位一致；`SetupAudit=true` 实际加载 | 全满足 | INVALID |
| SA-2 指针 | 源/候选 graph、character、setup 各自可读；setup 指针不同（与既有 G4 事实一致） | 全满足 | INVALID |
| SA-3 覆盖 | 12 骨 × 2 侧全部记录；骨数=116；无 missing | 全满足 | INVALID |
| SA-4 有限性 | 240 分量全有限；无零范数/不可读 | 全满足 | INVALID |
| SA-5 运行完整 | 一次 panel、零 `[E]`、无崩溃、预算未超 | 全满足 | INVALID |

**归属规则（描述性，不在本审计内改 G4P-7）**：

- 若源 setup bind 与候选 setup bind 在该 5 骨上存在与 3.49–5.59 同量级的常量差，且源 bind 与运行态静态值一致 → 支持 H1。
- 若两侧 bind 一致/近一致，而源运行态静态值与 bind 不同、候选运行态与 bind 一致 → 支持 H2。
- 若两侧 bind 与运行态都不能解释常量差 → 转 H3，保留层/时钟审计。

## 六、停止规则与边界

- 任一 `[E]`、崩溃、写图/写 pose/新增 hook、身份不符、骨覆盖缺失 → 停止并保留原因；不“补跑”同一窗口。
- 本审计不产生 `tau_*`、不改变 G4P-1..9、不删除任何骨、不声称 setup 相等即姿态等价。
- 若 H1/H2 成立，下一版才可设计候选 setup/静态附件对齐；任何对齐必须另立预注册并在新采集上验证。

## 七、实施顺序

1. ~~用户评审本 DRAFT~~（2026-11-17 用户批准直接开始；`load` 最小化修订见 §四）。
2. ~~实现 `SetupAudit`~~：v105，源码提交 `96d60fc`；编译 `/W4 /WX` 通过。
3. ~~构建/部署~~：身份 `CharacterPanel-scenegraph-v105-g5setup-audit-20261117-96d60fc63e-cl94faaed0c6-20261117T134821Z`；DLL `1C9E6284…780085`、PDB `04C428B9…E0739D`、运行 INI `2EF34BEA…7CF5CD3`；安装位与备份见 §八。
4. 待一次 panel 运行 → 离线归属报告 → 决定 H1/H2/H3 下一工作包；G4P-7 仍保持 FAIL，直到新的正式批次通过。

## 八、实现与部署身份

| 项 | 值 |
| --- | --- |
| 源码提交 | `96d60fc`（`feat(g5): add the read-only setup reference-pose audit`） |
| Build identity | `CharacterPanel-scenegraph-v105-g5setup-audit-20261117-96d60fc63e-cl94faaed0c6-20261117T134821Z`（无 `+dirty`） |
| DLL | 2,032,128 B；SHA-256 `1C9E62843E2E73CEDF1AC4E57BDA6A825C10B74AD1545F96FB92F8225D780085` |
| PDB | 51,630,080 B；SHA-256 `04C428B9A8D0B89059E393A04DAE8EBFF384B1B0B7C8E43952B9742597E0739D` |
| 运行 INI | `dist/g5-setup-audit-20261117/CharacterPanel.ini`；SHA-256 `2EF34BEAB695CDFD9907882B5D61F9F8F0928A6D5B9D01EA14589A7157CF5CD3` |
| 部署前备份 | `dist/deploy-backup-20261117-setupauditpre/`（v104 DLL `7D270E7D…` / PDB `B2232180…` / INI `BBFE2B8B…`） |

**部署（2026-11-17，已完成）**：安装位 DLL/PDB/INI 与上表逐位一致；`IndependentGraph=load`、`SetupAudit=true`、K5 三键 off、其余实验键 off。**尚未上机，尚无 `SETUP AUDIT` 读数。**
