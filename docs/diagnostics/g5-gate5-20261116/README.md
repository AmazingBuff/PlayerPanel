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
| G4P-1 运行资格 | 两侧 K5-1..8 / 六族 PAIRED | 候选 PASS / 源侧 PENDING |
| G4P-2 账本闭合 | 两侧 ledger 恒等式 | PENDING |
| G4P-3 括号覆盖 | 正式 manifest | PENDING |
| G4P-4 配对确定性 | 同输入两次运行 | PENDING |
| G4P-5 元数据有效 | 正式工件 | PENDING |
| G4P-6 记账完整 | 正式工件 + dry-run 锚点 | PENDING |
| G4P-7 时间与姿态 | P99 tau + `epsilon_t` + clock | PENDING |
| G4P-8 无效值守卫 | 正式 pose 解码 | PENDING |
| G4P-9 工件完整 | manifest/summary/身份清单 | PENDING |

**当前只有候选侧结果，尚无跨侧姿态裁定。** 本文件在首次正式数据归档时追加运行身份、原始日志/CPK5/CPK5S/JSON 哈希与 G4P 判定；不得用 Track 2 dry-run 或旧运行代替。

## 五、候选侧运行记录（2026-11-16 23:48–23:51，PASS）

**运行身份**：v103 DLL `8C3857A3…` + PDB `49581217…` + `k5-inject` INI `76F2BEBB…`；日志 `SCENEGRAPH BUILD` 为 `CharacterPanel-scenegraph-v103-g5k5-fullsample-9e83ff8e8f-cl94faaed0c6-20261114T134329Z`，runtime `1-6-1170-0`。一次 panel 窗口（`opening=true` 23:50:48.253，`opening=false` 23:51:38.183），无崩溃，无重复窗口。

| 工件 | 大小 | SHA-256 |
| --- | ---: | --- |
| [CharacterPanel.log](raw/candidate/CharacterPanel.log) | 243,381 B | `B4C476FF569FAC40705D0F8A0F3A85E5C45F38545C1F6372DB76CDE3D77D4981` |
| [CPK5 bin](raw/candidate/CharacterPanel-g5k5-CharacterPanel-scenegraph-v103-g5k5-fullsample-9e83ff8e8f-cl94faaed0c6-20261114T134329Z-20261116T155048Z.bin) | 1,410,452 B | `5686348BDBEA2F63B3618B7F4136A160227AA4E3014E55EB1B8D1247A3C0C31B` |
| [JSON ledger](raw/candidate/CharacterPanel-g5k5-CharacterPanel-scenegraph-v103-g5k5-fullsample-9e83ff8e8f-cl94faaed0c6-20261114T134329Z-20261116T155048Z.json) | 771 B | `86115F2F5E7490709718CB50FB55CE56E865191B519F13016A1783B6C0619504` |

**冻结场景**：`ANIMATION WORLD … snapshot=paused-source reason='panel-open' hands=(560c2a4c,00043324) drawn=false`，与 K2-C1 登记逐字一致。`K5 RECON`：`expected=249 attempted=249 recorded=249 missing=0 truncated=0 two-read-bad=0 clip-overflow=0 identity-failed=1 drift-records=1 dropped=0`；equip `120/120 fixed-equip-budget`，unequip `129/129 state-12-to-0`。

| K5 判据 | 读数 | 结果 |
| --- | --- | --- |
| K5-1 完整采样 | 249/249；equip 120 + unequip 129；`missing=0 dropped=0 truncated=0` | PASS |
| K5-2 双读一致 | 249/249 记录 `two_read_equal=1`；`two-read-bad=0` | PASS |
| K5-3 分母完整 | 249/249 记录 `track_count=116`；离线复核 0 失配 | PASS |
| K5-4 对账闭合 | 文件 1,410,452 B = 116 + 249×5,664；JSON 与 RECON 互洽；`write-failed=0` | PASS |
| K5-5 层/混合资格 | 四审计点齐全：`walked=97/94/122/113`，`missing=0 overflow=0 identity-failed=0`；unknown 77/78/89/84，沿用 Track 1/K6 的显式缺项清单（`hkbBlendingBehavior` 等无 CommonLibSSE 布局；不猜偏移） | PASS（带缺项清单） |
| K5-6 寿命/generation | equip completed 1..120、unequip 1..129 连续无缺口；clip 窗口完整；失效/取消未触发（如实登记） | PASS（取消未触发） |
| K5-7 资源断言 | 离线 `k5_reconcile.py --track2`：7 个唯一可读 sighting 全 REG（含 MT_Idle map_count=99、map FNV `077954859409395E`）；1 个 IDUN `SCAR_1hmReadyDummy` 单条排除；出集 0 | PASS |
| K5-8 运行完整 | 身份匹配；Save116 hands/drawn 匹配；预算未超；`[E]` 唯一 = Track 2 §十二已登记的装载期 `failed to get plugin info for OpenAnimationReplacer`（任何采集之前；运行期零 `[E]`） | PASS（继承精确豁免） |

**离线对账**：`[VERDICT] candidate-side K5-1/2/3/4/7: PASS`；`identity_failed_clips=1` 按 IDUN 记账，不进入配对/误差分布。

**裁定：候选侧有效（VALID），进入源侧采集。** 此裁定只覆盖候选侧 K5 资格；G4P-1 仍需源侧 K5-1..8，六族 PAIRED 与 G4P-7 均未判。

## 六、源侧运行记录（2026-11-16 23:56–23:59，PASS）

**运行身份**：同一 v103 DLL/PDB + `k5-source` INI `21BD938B…`；日志 `SCENEGRAPH BUILD` 匹配 `CharacterPanel-scenegraph-v103-g5k5-fullsample-9e83ff8e8f-cl94faaed0c6-20261114T134329Z`，runtime `1-6-1170-0`。一次 F7 liveness 窗口：`window=open` 23:58:40.323，`window=close reason=expired` 23:59:00.348（20.017 s，`pinned-engine=849`）。

| 工件 | 大小 | SHA-256 |
| --- | ---: | --- |
| [CharacterPanel.log](raw/source/CharacterPanel.log) | 265,374 B | `A8FF0E37A12C779569994FB19A1A31883F93B643E7A7FA97EA3CAAA6C4F49F21` |
| [CPK5S bin](raw/source/CharacterPanel-g5k5s-CharacterPanel-scenegraph-v103-g5k5-fullsample-9e83ff8e8f-cl94faaed0c6-20261114T134329Z-20261116T155900Z.bin) | 4,798,672 B | `4187890D7FF33E6B4F45E0D9319ABF98FCCB3F8D84254BD7FE15646F8985CF7C` |
| [JSON ledger](raw/source/CharacterPanel-g5k5s-CharacterPanel-scenegraph-v103-g5k5-fullsample-9e83ff8e8f-cl94faaed0c6-20261114T134329Z-20261116T155900Z.json) | 618 B | `513C36F142E34DD161EDDFB03BC3DF6BC051DA982CBB414FA02C5751945B3D78` |

`K5 RECON side=source expected=849 attempted=849 recorded=849 missing=0 truncated=0 two-read-bad=0 clip-overflow=0 identity-failed=1 drift-records=1 dropped=0 no-pose=0`；文件大小精确 `124 + 849×5,652 = 4,798,672 B`；`PROBE LIVENESS` manifest `steps=849 steps-saturated=no`，`b3b4-window verdict=B3+B4-PASS`。

| K5 判据 | 读数 | 结果 |
| --- | --- | --- |
| K5-1 完整采样 | 849/849；cap 1,400 未耗尽；`missing=0 dropped=0 truncated=0` | PASS |
| K5-2 双读一致 | 849/849 `two_read_equal=1`；`two-read-bad=0` | PASS |
| K5-3 分母完整 | 849/849 `track_count=116`；离线复核 0 失配 | PASS |
| K5-4 对账闭合 | 4,798,672 B 精确；JSON 与 RECON 互洽；`no-pose=0 write-failed=0` | PASS |
| K5-5 层/混合资格 | 源侧 `K5LayerAudit=false`，沿用 Track 2/K6 的源侧资格与显式缺项清单；本批不新增层偏移猜测 | PASS（继承，带缺项） |
| K5-6 寿命/generation | `seq=1..849` 连续无缺口；TSC 严格单调；六个 clip 窗口完整；失效/取消未触发（如实登记） | PASS（取消未触发） |
| K5-7 资源断言 | 离线 `k5_reconcile.py --track2`：6 族 7 个可读 sighting 全 REG（MT_Idle map_count=99、map FNV `077954859409395E`）；1 个 IDUN `SCAR_1hmReadyDummy` 排除；出集 0 | PASS |
| K5-8 运行完整 | 身份匹配；窗口一次且正常 expired；预算未超；`[E]` 唯一 = Track 2 §十二装载期 OAR 豁免行，运行期零 `[E]` | PASS（继承精确豁免） |

**clip 窗口（seq）**：1HM_Equip 137–204（68）、1HM_Unequip 403–494（92）、1HM_Idle/MLh_Idle/SCAR dummy 197–410（各 214）、MT_Idle 1–849（508）。

**裁定：源侧有效（VALID）。** `K5 RECON` 与离线对账均闭合；G4P-1 两侧 K5 资格成立。

## 七、正式配对与 G4P 判定（2026-11-17）

### 7.1 工具身份与已修复的解析缺陷

首次正式调用使用门 4 冻结版 `gate4_pair.py` `51ECBD67…`，产出 [formal-preidentity-fix](formal-preidentity-fix/gate4-pair-manifest.json)（manifest `2C9C5C2E…C0980`，summary `30834EFB…D86E9`）。该次输出暴露一个**只影响源身份显示**的解析缺陷：源 CPK5S 头实际为 28 B 固定字段 + 96 B identity；旧解析器按 `header_size-92` 读取，少读了开头 2 个字符，manifest 写成 `acterPanel…`。

修复：`K5_SOURCE_HEADER` 改为 `<4sHHIIIII`，identity 读取改为 96 B；测试 fixture 同步改为真实头布局并增加 `T10 source identity`。`gate4_pair_test.py` T1..T13 全 PASS。**逐字段比较确认：旧/新 manifest 的唯一差异是 `/inputs/source/identity`；配对、钟、账本与全部姿态数值不变。** 因此该修复不改变 G4P-7 读数，只修复 G4P-9 的身份记录。

修复后工具 `gate4_pair.py` SHA-256 `873AEC16A4E9D202CCDFD8B6F98BAD47266FD94A49DFE5EB118C96E17E0472AC`；测试 `5519F19961114823ED30A060FE5D2389AC2A69276EC64F4BE2100CE05FD0C7BD`。正式工件使用修复版。

### 7.2 正式工件

| 工件 | 大小 | SHA-256 |
| --- | ---: | --- |
| [manifest](formal/gate4-pair-manifest.json) | 114,299 B | `8D61E0F41C56FEC66A44DB93502F31B250414262FC0FB4B40A312594CA78E144` |
| [detail](formal/gate4-pair-detail.bin) | 464,256 B | `51A44CEBF6D572A28BF5DC3D43458DEB8D30A7D96E2863FEA4C0B5D9A7D78D04` |
| [summary](formal/gate4-pair-summary.txt) | 1,939 B / 27 行 | `30834EFB25608297566E407FD95E8C16AA07B09A097F728CC0892C5DE20D86E9` |
| [repeat manifest](formal-repeat/gate4-pair-manifest.json) | 114,299 B | `8D61E0F4…8E144`（逐字节同） |
| [repeat summary](formal-repeat/gate4-pair-summary.txt) | 1,939 B | `30834EFB…D86E9`（逐字节同） |

### 7.3 G4P 判定

| 判据 | 读数 | 结果 |
| --- | --- | --- |
| G4P-1 运行资格 | 候选 K5-1..8 PASS；源侧 K5-1..8 PASS；六族资源 FNV PAIRED；IDUN 候选 1 / 源 1 单独记账 | PASS |
| G4P-2 账本闭合 | 候选 `249 = 1 IDUN + 248 eligible`、`paired=248`；源 `849 = 1 IDUN + 848 eligible`、`hosted_pairs=248`；`out_of_set=0` | PASS |
| G4P-3 括号覆盖 | 248 对；`no_partner=0`；edge 12（equip 1 / unequip 10 / idle 1）；无 wrap | PASS |
| G4P-4 配对确定性 | 同输入两次调用：manifest + summary 逐字节一致 | PASS |
| G4P-5 元数据有效 | 全部钟值有限；无 wrap/实例歧义；K5-7 出集 0 | PASS |
| G4P-6 记账完整 | IDUN/no-clock/edge/wrap/族集合差全部入账。**工具锚点 `1hm_equip expected=96 actual=97 matched=no`**：候选 equip clip 98 条，IDUN 1 条，故 eligible 97；Track 2 dry-run 时 IDUN 2 条，故为 96。计数规则未变，差异完全由 IDUN 数解释；idle/un equip 锚点均匹配 | PASS（带锚点说明，非静默通过） |
| G4P-7 时间与姿态 | **FAIL**：P99 主分母 24,552 个 `E_p` 中 3,826 超 `tau_p`；`E_r` 16,755 超；`E_s` 352 超。P99 分布 `E_p=5.5912`、`E_r=0.0457227`、`E_s=0.000525653`（冻结 tau `0.000476592 / 6.03341e-8 / 0.00000386264`）。`dt_excluded_pairs=0`，非时间排除造成 | **FAIL** |
| G4P-8 无效值守卫 | `invalid_bones=0`；无零范数/非有限进入误差分布；A17 单列描述 | PASS |
| G4P-9 工件完整 | manifest/detail/summary 可读；输入与工具 SHA 登记；manifest < 1 MiB、detail < 8 MiB、summary 27 行 | PASS |

**批次裁定：门 5 采集与结构判据完成，但 G4P-7 FAIL；G5 未通过，G6 后置。** 这不是 G5 PASS，也不是“部分通过”。

### 7.4 失败归因（描述性，不改变判据）

逐域 P99 `E_p`：equip `5.5912`、unequip `4.2150`、idle `4.2150`；三域的 P50 分别为 `0 / 2.46e-7 / 0`，大量分量逐位相同，配对结构并非整体错位。

按骨架名排序的最大误差集中在静态装备/附件轨：

| 索引 | 名称 | max E_p | 超 tau 次数 |
| ---: | --- | ---: | ---: |
| 43 | `Weapon` | 31.3159 | 170 |
| 4 | `NPC COM [COM ]` | 6.8615 | 225 |
| 45 | `NPC R Pauldron` | 5.6214 | 246 |
| 64 | `WeaponDagger` | 5.5912 | 248 |
| 62 | `WeaponBack` | 4.2921 | 248 |
| 60 | `Quiver` | 4.2150 | 248 |
| 63 | `WeaponBow` | 4.2150 | 248 |
| 65 | `WeaponMace` | 4.0797 | 248 |
| 66 | `WeaponSword` | 3.8015 | 248 |
| 61 | `WeaponAxe` | 3.4893 | 248 |

代表样本（pair 0，bone 64 `WeaponDagger`）：候选与源的旋转四元数逐位相同（`0.395178,-0.623511,0.651826,-0.173754`），但平移相差约 `4.08`（候选 `-8.6093,5.7875,2.9914`，源 `-12.6879,9.5715,3.5452`）。这说明失败主轴是**候选静态装备/骨架默认位与源实际装备状态不一致**，不是旋转采样或配对整体错位；与 K2-C1 PARTIAL、K5-5 层/权重缺项、A17 静态 item/wing/Belly 缺项一致。

描述性排除 11 个装备/附件骨（4、43–45、60–66）后，身体 89 轨仍有 `E_p` P99 `0.0776481`、1,226 次超 tau——因此**不能**把失败全部归给附件骨，也不能事后删骨通过判据；身体轨仍有残余差异，需与时间对齐/源场景一起审计。

## 八、结论与下一入口

- 门 5 的两侧正式采集已完成并归档；G4P-1..6、8、9 成立（G4P-6 带上述 IDUN 锚点说明），**G4P-7 FAIL**。
- G5 仍 **未通过**；G6 后置；不得把本记录写成 G5 PASS 或“仅差一点”。
- 失败归因见 [C-G5-ATT v1 事后审计](../g5-attribution-20261117/README.md)：5 个静态跨侧偏移附件骨贡献 1,240/3,826 次 `E_p` 超容差，body 88 轨仍贡献 1,226；该审计不改变本裁定。
- 只读确认性审计 [C-G5-SETUP v1](../g5-setup-audit-20261117/README.md) 已完成：**H1（setup/bind 不同）证伪；H2（源真实附件状态）得到支持**；静态 5 骨的 source−ref 为 2.77–4.20，候选−ref ≤1.62e-4。该结果不改变 G4P-7 FAIL；下一版需另行预注册静态附件轴/真实附件状态。
- 时钟阶梯审计 [C-G5-CLOCK v2](../g5-clock-step-20261117/README.md) 已完成（描述性）：候选 equip/unequip/idle 只有 **17/21/3** 个唯一 local_time，正式配对只用到 **12/16/3** 个源 seq（最大复用 20）；body 超差 1,032/1,126（equip）与 92/100（unequip）落在 stair 对，但 unique 对仍有 94/8 次。**同一 local_time 的多记录组全部是不同完整姿态（8/8、10/10、2/2 全唯一）**，说明 clip time 不是充分相位，K5-5 层/权重是直接缺口。该结果不改变 G4P-7 FAIL；下一版须另行预注册时间/状态轴。
- 状态空间审计 [C-G5-STATE v1](../g5-state-20261117/README.md) 已完成（描述性）：同组候选姿态 PC1 方差占比 **0.9983–0.999998** 且投影随记录顺序单调——是**单标量混合曲线**。equip 第一组最近源轨迹残差 median 0.324 / max 0.497，后续 equip 组 ≤0.0856，末段 ≤0.0076；unequip 后段与 idle 最近源残差 ≤1.5e-5/3.3e-5。结论：equip 起始是离轨的初始状态，unequip/idle 主要是配对时钟问题，K5-5 需要单标量 blend/权重状态。G4P-7 仍 FAIL。
- 初始姿态 bootstrap [C-G5-STATE v2](../g5-state-bootstrap-20261117/README.md) 已完成：v106 一次性复制源 poseLocal 116/116（`equal=1`），但首帧 Generate 后候选姿态与门 5 **288,840/288,840 分量完全相同**；G4P-7 计数/P99 不变，SB-5 FAIL。初始状态在图内部（active nodes/blend/weights），不是 poseLocal 缓冲；下一版须初始化图状态或采集单标量 blend。
- 图内部状态探针 [C-G5-STATE v3](../g5-state-probe-20261117/README.md) 已完成：`hkbNodeInfo` 2,729 行与 blender 对象 0x100 窗口 12 行均**没有**随同 localTime 组逐 step 变化的字段；同组中间步骤无 nodeInfo 变化，SP-5 未找到。标量应在 `hkbBlenderGeneratorInternalState`/child 权重或生成输出；该负结果不改变 G4P-7 FAIL。
- 下一步只能走新的预注册修订：优先闭合 K2-C1 的装备/骨架初态与 K5-5 层/权重，或为静态附件轨建立独立控制/相等判据；任何删轨、放宽 tau、把 `epsilon_t` 当姿态噪声吸收都必须先升版预注册，不得用本批数据回填。
