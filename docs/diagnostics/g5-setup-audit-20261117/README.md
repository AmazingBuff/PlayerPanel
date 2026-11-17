# C-G5-SETUP v1：候选/源 setup 与静态附件只读审计（2026-11-17，COMPLETE）

**状态：COMPLETE（2026-11-17）：H1 被证伪，H2 得到支持，H3 保留给 body/dynamic 残余。** v105 只读 `SetupAudit` 已实现、构建、部署并完成一次 panel 运行；本审计只回答门 5 G4P-7 失败归因中的 setup/静态附件假设；不改变门 4/门 5 判据，不产生 `tau_*`，不关闭 G5/G6。

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

## 三、只读仪器（已实现）

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
| SA-5 运行完整 | 一次 panel、零非豁免 `[E]`（装载期 OAR 行按 Track 2 §十二）、无崩溃、预算未超 | 全满足 | INVALID |

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
4. ~~一次 panel 运行 + 离线归属报告~~：2026-11-17 21:58:26 完成；H1 证伪、H2 支持，见 §九；G4P-7 仍保持 FAIL，直到新的正式批次通过。

## 八、实现与部署身份

| 项 | 值 |
| --- | --- |
| 源码提交 | `96d60fc`（`feat(g5): add the read-only setup reference-pose audit`） |
| Build identity | `CharacterPanel-scenegraph-v105-g5setup-audit-20261117-96d60fc63e-cl94faaed0c6-20261117T134821Z`（无 `+dirty`） |
| DLL | 2,032,128 B；SHA-256 `1C9E62843E2E73CEDF1AC4E57BDA6A825C10B74AD1545F96FB92F8225D780085` |
| PDB | 51,630,080 B；SHA-256 `04C428B9A8D0B89059E393A04DAE8EBFF384B1B0B7C8E43952B9742597E0739D` |
| 运行 INI | `dist/g5-setup-audit-20261117/CharacterPanel.ini`；SHA-256 `2EF34BEAB695CDFD9907882B5D61F9F8F0928A6D5B9D01EA14589A7157CF5CD3` |
| 部署前备份 | `dist/deploy-backup-20261117-setupauditpre/`（v104 DLL `7D270E7D…` / PDB `B2232180…` / INI `BBFE2B8B…`） |

**部署（2026-11-17，已完成）**：安装位 DLL/PDB/INI 与上表逐位一致；`IndependentGraph=load`、`SetupAudit=true`、K5 三键 off、其余实验键 off。

## 九、v105 运行结果（2026-11-17 21:56–21:59，AUDIT COMPLETE）

**运行身份**：v105 DLL/PDB/INI 与 §八逐位一致；日志 `SCENEGRAPH BUILD` = `CharacterPanel-scenegraph-v105-g5setup-audit-20261117-96d60fc63e-cl94faaed0c6-20261117T134821Z`。一次 panel 窗口（`opening=true` 21:58:26.291，`false` 21:59:00.246），无崩溃，无第二次实验。日志归档：[run1/CharacterPanel.log](run1/CharacterPanel.log)，25,835 B，SHA-256 `B06697AB8FADD02A1FC969AE4C287C9E7B7771496E1C088003E6F9923E04838A`。

**SA 判据**：SA-1 PASS（身份/开关匹配）；SA-2 PASS（源 graph `0x18793161580`、候选 graph `0x186655b1300`；setup `0x182f2ea0600` vs `0x182f28f9380`，不同）；SA-3 PASS（12 骨 ×2 侧 = 24 行；`bones=116 reference-pose=116`）；SA-4 PASS（240 分量 `finite=true`）；SA-5 PASS（一次 panel；唯一 `[E]` 为 Track 2 §十二装载期 OAR 豁免行，运行期零 `[E]`）。

### 9.1 setup/bind 结论

源与候选的 `animationSkeleton` **同址** `0x182d49d6320`，`data` 同址 `0x182d49e9010`，`mirroredSkeleton` 同址 `0x182f20af740`；只有 setup 包装指针不同。12 个固定骨的 `referencePose` 在两侧逐字相同（含 5 个静态附件骨）。**H1（setup/bind 默认位不同）被证伪。**

### 9.2 运行态与 referencePose

门 5 正式数据（候选 pair 0 / 源 seq 137）与本次 `referencePose` 对比：

| 骨 | referencePose t | 候选运行态 t | 源运行态 t | 候选−ref | 源−ref |
| ---: | --- | --- | --- | ---: | ---: |
| 60 `Quiver` | `(-7.278420,-6.048058,-16.815178)` | `(-7.278445,-6.048089,-16.815235)` | `(-7.278442,-10.243851,-16.413286)` | 5.7e-5 | **4.195793** |
| 61 `WeaponAxe` | `(18.991491,-2.993564,-19.048920)` | `(18.991653,-2.993578,-19.049023)` | `(21.756907,-0.882682,-18.779766)` | 1.62e-4 | **2.765415** |
| 63 `WeaponBow` | `(-3.558048,-10.577713,2.465256)` | `(-3.558058,-10.577730,2.465272)` | `(-3.558055,-14.773514,2.867210)` | 1.7e-5 | **4.195801** |
| 64 `WeaponDagger` | `(-8.609213,5.787449,2.991348)` | `(-8.609266,5.787476,2.991362)` | `(-12.687939,9.571548,3.545177)` | 5.3e-5 | **4.078726** |
| 66 `WeaponSword` | `(-11.891476,1.916895,6.666046)` | `(-11.891581,1.916901,6.666102)` | `(-15.598629,2.182207,7.465465)` | 1.05e-4 | **3.707153** |

旋转四元数在两侧与 referencePose 均一致/近一致（差 ≤4.3e-4）；差异只在平移。**H2 得到支持**：源的实际装备/附件对象把静态附件节点从 referencePose 写到装备状态，候选没有真实附件对象，保持 referencePose。

### 9.3 裁定

- **H1 = FALSIFIED**（setup/bind 同源，referencePose 逐字相同）。
- **H2 = SUPPORTED**（5 个静态附件骨：候选运行态 ≈ referencePose；源运行态偏移 2.77–4.20，旋转不变）。
- **H3 = RETAINED**（不解释静态 5 骨；body 1,226 与 dynamic attachment 1,360 的残余仍需层/权重/父级/时钟审计）。
- 本审计不改变 G4P-7 FAIL/G5 未通过；下一版若要消除静态 1,240 次超容差，必须预注册“候选真实附件状态”或“静态附件独立相等轴”，不能把本次结果当作门 5 通过。
