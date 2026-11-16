# C-G5-G5 v1：门 5 正式采集运行记录（2026-11-16，PREFLIGHT）

**状态：PREFLIGHT（尚未上机、尚无门 5 数据）。** 本记录只登记门 5 的冻结身份、运行前安装态与逐字执行清单；任何姿态/时间/配对结论必须在两侧正式运行完成并归档新工件后另节追加。门 4 冻结记录见 [g5-gate4-freeze-20261116](../g5-gate4-freeze-20261116/README.md)，正式运行设计见 [门 4 §七](../g5-gate4-20261115/README.md#七正式运行设计门-5-前置本协议不授权采集)。

## 一、授权与边界

- 门 4 已 FROZEN（限定范围：P99 姿态 + 时间 + 时钟轴 + A17 描述性账本），门 5 **采集**已授权。
- 本批不构建、不改代码、不改 INI 键；唯一允许的运行时变更是在 `AE-Track2` 档内换装已冻结的 v103 DLL/PDB 与两份既有 k5 INI。
- 候选侧与源侧各一次会话/窗口；重复或重跑必须登记原因与新批次，不得静默替换。
- 每轮先判 K5-1..8；任一失败即 INVALID，不进配对。任一 `[E]`、崩溃、未知 ABI 或身份不符即按门 4 §十停止。
- 本批只产生 G4P-7 的输入；G5 仍因 K5-5/K2-C1/A17 阻塞，不得写成 G5 PASS。

## 二、冻结身份（运行前实测）

| 项 | 值 |
| --- | --- |
| 游戏 | `SkyrimSE.exe` 1.6.1170.0；SHA-256 `C434208894F07F604B852F29B8EDC3A58C4DE63DE783373733E72B2B73F33BE9` |
| SKSE loader | 0.2.2.6；SHA-256 `730C2743F6871FBAEB8606C1D3B7A55FECA045C3D74858A41B0C6D03CD989FBC` |
| MO2 profile | `AE-Track2`（`selected_profile` 实测）；modlist `+` 979 项（含分隔符；MO2 统计 954 个启用模组） |
| modlist.txt | SHA-256 `E5DF72DF98B658EAEB538B1140F1DA7EAB2215C9D809C43E179ED46A56E3FE33` |
| plugins.txt | SHA-256 `215BE1B47DC761B2DC8FA0E54ACC8095483FC9929CA45D143559EA7C3FE1915F` |
| loadorder.txt | SHA-256 `D4E7C168A8D5428155D95597C9F153748DE1B65F8BE4F697FF4C1125CA9C79A5` |
| archives.txt | SHA-256 `F62369D080C5F8C1ED752377EFB670E2A19BF121B34D4DB45E27821AFA9AD338` |
| Save116 `.ess` | 8,259,439 B；SHA-256 `F5B2145BC49DC8E916E381A5666802EF91990C87E2424512141D9C65C5B119CB` |
| Save116 `.skse` | 1,601,157 B；SHA-256 `17CEF3BB133CE16CE0F8A573D8355F16A6BDBAE9535F4BCB7FD7E6BDE247D665` |
| v103 DLL | 2,007,552 B；SHA-256 `8C3857A39479C172F894C73449677ADF3BB19DBD6BAF6718609D0D85EA58E5EF` |
| v103 PDB | 51,367,936 B；SHA-256 `49581217B75FF7A93646C8FF106A045584071EAEA54F214CFDC4627F83045566` |
| k5-inject INI | SHA-256 `76F2BEBB03DEAED6C9B5E0987DFA8E3BBBD00AC1E54323D2AF2C5EDB1BE0B885` |
| k5-source INI | SHA-256 `21BD938B94268C3DA7A81EBAA6BDEE75AD4D49E9C4C36114C052142FC626EC2B` |
| `gate4_pair.py` | SHA-256 `51ECBD673A9DD059A38A9E7FCF3F7D09B3DCF60B4D4EDA5B7E3D657B55B9D35D` |
| `gate4_clock.py` | SHA-256 `6E21BB47DF36A17ABDBFBFBC98323A237D5EF75774339CDD3665E1CEC4A41780` |
| `k5_reconcile.py` | SHA-256 `14797A23FB8A6162E6CCB54A7B13A9CF1B16D0A2BFB8A6E08B62142053214EA7` |

### 2.1 运行前安装态（将被替换，登记以便回退）

| 文件 | 当前值 |
| --- | --- |
| `CharacterPanel.dll`（v104） | 2,025,984 B；SHA-256 `7D270E7DFC35CCADCCA0807472356AA81E3DDA3EFA912D516479605470D6F9DF` |
| `CharacterPanel.pdb`（v104） | SHA-256 `B2232180B7EC250AC7AEBB37FA79536FD0AB7EDD8DF87321420A88BBA31C2AC1` |
| `CharacterPanel.ini`（v104/CAL2） | 12,413 B；SHA-256 `BBFE2B8BFAD11FA97CFB151BCDCD3E6460184A9487354928067CAC59F1EAFBB2` |

运行前安装态先整体备份到 `dist/deploy-backup-20261116-gate5pre/`，不得覆盖既有备份。

## 三、逐字执行清单

### 3.1 候选侧（k5-inject）

1. 安装 v103 DLL/PDB（哈希见 §二），部署 `k5-inject` INI，复核安装位三元组哈希。
2. 启动 MO2 `AE-Track2` → SKSE；日志中 `SCENEGRAPH BUILD` 必须匹配 v103 身份。
3. 载入 **Save116**；不改装备、不存档、不读档。
4. 打开一次 panel；等待注入链自动完成：prime 5/5 写 + `WeapEquip` 一次 → equip 120/120 `fixed-equip-budget` → settle（target 87 / settled 101）→ `Unequip` id=144 → unequip `state-12-to-0`。
5. 关闭 panel；退出游戏。记录窗口起止、`[E]` 行、K5 RECON 行与三件工件哈希。

### 3.2 源侧（k5-source）

1. 安装同一 v103 DLL/PDB，部署 `k5-source` INI，复核三元组哈希。
2. 启动 MO2 `AE-Track2` → SKSE；日志身份匹配。
3. 载入 **Save116**；按 v103 的实际实现，`SamplingLiveness=on` 由 **F7 武装 20 秒窗口**（`arm_probe_liveness`），随后在窗口内自然拔剑/收剑。门 4 §七的“无 F7 窗口”按“无 panel 窗口/直采”理解；文本与实现的这一差异在此登记。若不按 F7，源侧不会产出 K5 快照。
4. 自然拔剑 → 保持 → 收剑；覆盖六个可配对 clip 族。
5. 退出游戏。记录窗口起止、`[E]` 行、K5 RECON 行与工件哈希。

## 四、运行后必判

| 判据 | 输入 | 结果 |
| --- | --- | --- |
| G4P-1 运行资格 | 两侧 K5-1..8 / 六族 PAIRED | PENDING |
| G4P-2 账本闭合 | 两侧 ledger 恒等式 | PENDING |
| G4P-3 括号覆盖 | 正式 manifest | PENDING |
| G4P-4 配对确定性 | 同输入两次运行 | PENDING |
| G4P-5 元数据有效 | 正式工件 | PENDING |
| G4P-6 记账完整 | 正式工件 + dry-run 锚点 | PENDING |
| G4P-7 时间与姿态 | P99 tau + `epsilon_t` + clock | PENDING |
| G4P-8 无效值守卫 | 正式 pose 解码 | PENDING |
| G4P-9 工件完整 | manifest/summary/身份清单 | PENDING |

**当前没有任何门 5 结果。** 本文件在首次正式数据归档时追加运行身份、原始日志/CPK5/CPK5S/JSON 哈希与 G4P 判定；不得用 Track 2 dry-run 或旧运行代替。
