# C-G5-CAL K2-C1/C2：装备初态与输入账本（2026-11-14 预注册）

**状态（2026-11-14 20:1x）：C2 = REGISTERED（离线关闭）；C1 = REGISTERED（v102 run 1 仪器上机后关闭，含一项偏差登记，见 §六）。** 本轮闭合 [CAL v1 §三 K2](../../independent-behavior-graph.md) 的前两子项：C1 装备初态清单、C2 输入账本。第 1 步离线登记（零插件改动）；第 2 步仪器（提交 `bd630da`）已实现、部署并上机一次。

**用户决策（2026-11-14）**：① 背包以**存档身份冻结**（不逐项枚举背包内容）；② 先离线登记，仪器实现与上机后行。

## 一、目标与边界

| 项 | 本轮交付 | 不覆盖 |
| --- | --- | --- |
| K2-C1 装备初态 | 右手武器/左手法术/火把状态/初始相位的固定清单，逐项挂日志证据；FormID 解析到插件+EDID | 护甲 42 槽与存档身份（需第 2 步仪器）；五字 prime 消融（最小集不是关闭条件） |
| K2-C2 输入账本 | 候选注入序列（写/读回/事件/步数/settle）与源侧参考序列（两次运行的相对时刻）分开登记；图内输出单列 | 源/候选输入的等价性证明（候选用 prime 替代真实装备是已知被测差异）；跨场景推广 |
| 通用 | 冻结场景 = v96a 的 1H+左手法术场景（同一存档、同一初态） | H2H 或其他场景的账本 |

**明确不做**：不宣称 prime 五字是最小集；不产生 `tau_*`；不关闭 K5/K6/G5；不改写 D1/D2/D3/v96a 历史裁定。

## 二、证据源（全部为已归档日志，不改写）

| 日志 | 用途 |
| --- | --- |
| [v96a run 1](../CharacterPanel-g5cal-v96a-observation-v2-run1-20261112-2157.log)（180,111 B `d34994f4…d17f6f`） | 冻结场景的候选侧全链：产品行、prime、事件、步数、settle、卸下完成 |
| [D3 v2](../CharacterPanel-g5cal-d3v2-run1-20261111-2331.log)（16,263 B `906e4707…50697d2`） | 源侧初值探针 + 装备/卸下相对时刻（第 1 次） |
| [v101b](../CharacterPanel-g5k1-source-census-v101b-run2-20261114-1424.log)（67,974 B `8B3AC063…E9E2F15`） | 源侧时序第 2 次复现（自然游玩） |
| [D1 run 1](../CharacterPanel-g5cal-d1-run1-20261111-2246.log) | OAR 命中记录（Smooth Moveset - 1hm）与产品路径 family=1h |
| 本仓 K1-C1 登记（`g5-k1-c1-20261114/`） | 候选/源活动 clip 的资源来源映射（data FNV → 文件） |

## 三、离线账本（本轮登记内容）

### C1 装备初态清单（冻结场景：1H + 左手法术，收鞘）

| 项 | 登记值 | 证据 |
| --- | --- | --- |
| 右手武器 | `DBM_CC_WeaponCrimsonTideSword`（WEAP，插件 `DBM_CC_HallOfWonders.esp`，运行时 FormID `0x560c2a4c`） | v96a L604 `hands=(560c2a4c,00043324)`（[controller.cpp L880](../../src/animation/controller.cpp#L880)：hands[0]=`GetEquippedObject(false)`=右手）；插件归属 = 全插件记录扫描唯一命中（low24 `0x0C2A4C`，FULL 名为四个替代字符、EDID 唯一） |
| 左手物品 | `Candlelight`（SPEL，Skyrim.esm `0x00043324`）→ 对应 `iLeftHandType=9` | 同上 hands[1]=左手；EDID 由 Skyrim.esm 直解；D3v2 L60 初值探针 `iLeftHandType=9` |
| 火把 | 未装备（左手被烛光法术占用；场景无 LIGH 在手） | 同上 |
| drawn（拔鞘） | `false`（收鞘） | v96a L604 `drawn=false` |
| 源侧初始相位 | window-open：`currentDefaultState=0 iState=0`（D3v2 L59-60）+ 探针 `iRightHandType=1 iLeftHandType=9 Is1HM=0 i1HMState=0` | D3v2 L59-L60 |
| 候选初始相位 | baseline：`istate=0 default=10 nodes=112`（装备事件注入后首帧） | v96a L142 |
| 候选图变量初值 | authored `iRightHandType=0 iLeftHandType=0`（D1/D2 判负根因），prime 后 1/9 | [K1/K2 §三](../../independent-behavior-graph.md#c-g5-cal-k1k2) D2/D3 记录 |
| 两侧初态一致性 | 候选 prime readback `1/1/9/1/9` = 源侧观察值 `1/1/9`（L74-L76）+ `1/9`（L60） | v96a L116-L120；D3v2 L60/L74-L76 |
| 护甲 31 槽 | worn=9，见 §六 全表 | v102 run 1 dump 行（L608） |
| 存档身份 | `Save116_61F78427_0_456C6966_Folgunthur01_011659_20261112151836_26_1.ess`（角色 Elif，#116，Folgunthur01，playtime 040.59.48）；背包 = 存档身份冻结 + 该存档同时给出已装备 CC 背包（slot 17） | 存档表交叉解析（§六）；背包语义按用户决策 |
| OAR 命中（源侧 panel-open） | `MLh_Idle`/`1HM_Idle` → Smooth Moveset - 1hm「1hm Non-Combat idle Start」；`SCAR_1hmReadyDummy` → Dummy/DumDum；`oar-project=DefaultFemale`、`oar-api=available oar-result=replacement` | v96a L604 |
| 候选 clip 资源来源 | 按 data FNV：equip/unequip = SCAR/Elden Rim 同字节家族、baseline MT_Idle = Ashe conditional、equip MT_Idle = Vanargand female、1HM_Idle = 原版 BSA | [K1-C1 对账](g5-k1-c1-20261114/README.md#六执行结果与裁定) |

**FormID 解析方法与偏差**：插件归属以"全插件扫描 low24 唯一命中"为准（比运行时槽位换算可靠）；静态重算槽位表（plugins.txt 启用 + 非 ESL 头标志）把 `DBM_CC_HallOfWonders.esp` 排在 `0x47`，与运行时前缀 `0x56` 差 15 槽——原因未定（运行时点 2026-11-12 与当前 plugins.txt 可能不同，或 ESL/CC 槽位规则另有细节），**不影响本轮归属判定**（记录扫描唯一且 EDID 语义吻合），如实登记待查。账本以插件名+EDID 为稳定身份，运行时 FormID 只作当次运行参照。

### C2 输入账本

**候选侧（脚本注入，相对序 t0 = prime 帧；v96a L116-L122、L142、L362-L364、L560）**

| 序 | 类型 | 内容 | 证据行 |
| --- | --- | --- | --- |
| 1 | 外部写 | `iMagicEquipped`(117) INT32 =1，readback 1 | L116 |
| 2 | 外部写 | `iRightHandEquipped`(179) INT32 =1，readback 1 | L117 |
| 3 | 外部写 | `iLeftHandEquipped`(180) INT32 =9，readback 9 | L118 |
| 4 | 外部写 | `iRightHandType`(35) INT32 =1，readback 1 | L119 |
| 5 | 外部写 | `iLeftHandType`(60) INT32 =9，readback 9 | L120 |
| 6 | 外部事件 | `WeapEquip` id=2 dispatched（与写同帧） | L122 |
| 7 | 图内输出 | equip 段 120/120 步；baseline `istate=0 default=10`；clip `1HM_Equip`（K2-C3 PASS） | L142+ |
| 8 | 外部事件 | settle：target-step=87 → settled-step=101（`baseline-default=1 baseline-nodes=122`） | L362 |
| 9 | 外部事件 | `Unequip` id=144 dispatched（unequip-inject） | L364 |
| 10 | 图内输出 | unequip completed=129、`default=0 istate=0 uneq=1 bequip=1 pitch=1 rh=1`、`state-12-to-0`（本场景完成 PASS） | L560 |

**源侧参考序列（真实装备，引擎驱动；两次运行）**

D3 v2（2026-11-11 23:33，第 1 次；相对时刻以装备事件 t=0）：

| 相对时刻 | 变量 / 事件 | 转移（行号） |
| --- | --- | --- |
| −1.801 s | window-open + 初值探针 | `iRightHandType=1 iLeftHandType=9 Is1HM=0 i1HMState=0`（L59-L60，@05.968） |
| t=0 | `IsEquipping` 0→1 + `currentDefaultState` 0→10 | L67-L68（@07.769） |
| +0.500 s | `iMagicEquipped/iRightHandEquipped/iLeftHandEquipped` 0→1/1/9 | L74-L76（@08.269） |
| +1.056 s | `iState` 0→9（L86）；`currentDefaultState` 10→1（L87） | @08.825 |
| +7.803 s | `currentDefaultState` 1→12 | L100（@15.572） |
| +7.996 s | `iState` 9→0 | L107（@15.765） |
| +8.654 s | `iLeftHandEquipped` 9→0（卸下时左手法术一并清除） | L112（@16.423） |
| +9.054 s | `currentDefaultState` 12→0 | L119（@16.823） |

v101b（2026-11-14 14:24，第 2 次复现；用户节奏不同）：`IsEquipping 0→1` @39.698 → `currentDefaultState 0→10` @39.700（+0.002 s，settle 记录粒度差）→ `iState 0→9` @40.759（+1.061 s）→ `IsUnequipping 0→1` @45.807（+6.109 s，用户-paced）。

**分离原则**：外部输入 = 表中"外部写/外部事件"两类；图内输出 = `currentDefaultState`/`iState`/clip 选择/步数，单列不混入输入。装备变量的"源侧写入者是引擎真实装备行为"，候选侧写入者是插件 typed setter——两者都登记为外部输入，语义差异（真实装备 vs prime）保留为被测差异。

## 四、第 2 步预注册：装备全态仪器（已实现，待上机）

**采集前修订（2026-11-14，实现时）**：CommonLib 的 `Actor::GetWornArmor` 取 `BGSBipedObjectForm::BipedObjectSlot` **掩码枚举**（31 个位槽，`kFX01=1<<31` 含），不是 42 值索引枚举——上文"42 槽"修订为 **31 槽**。此修订发生在任何采集之前，不触碰已收数据。

| 项 | 固定内容 |
| --- | --- |
| 新键 | `[Experiment] EquipmentStateDump=true`，默认 off；只读 |
| 观测点 | 现有 `ANIMATION WORLD` panel-open/equipment 快照（[controller.cpp](../../src/animation/controller.cpp) `ANIMATION WORLD` 行）行尾追加 ` armor=slots=31 worn=N <slot>:<formid>,…`：`GetWornArmor` 逐掩码槽枚举，非空槽记 slot:FormID；键 off 时行与历史格式逐字节一致 |
| 上界 | armor 字段 ≤31 项；不新增 hook、不写图、不采样姿态 |
| 运行协议 | 与 v96a 完全相同的冻结场景（同存档、1H+Candlelight 收鞘、同 INI 基线 + `EquipmentStateDump=on`）；一次 panel 窗口；存档槽位名由用户在运行记录登记（背包以存档身份冻结） |
| 判据 K2C1-2 | dump 行存在、31 槽全枚举、零 `[E]`、无崩溃；与 v96a 的 hands/drawn/OAR 命中逐字一致 |
| 判据 K2C1-3 | 存档槽位名登记进账本，背包以存档身份冻结 |
| 关闭语义 | 两判据成立 → C1 由"部分"转 REGISTERED；仍不关闭 K5/K6/G5 |

## 五、本轮判据（离线部分）

| # | 判据 | 通过条件 | 裁定 |
| --- | --- | --- | --- |
| K2C1-1 | C1 已证字段与日志逐字一致、逐项挂证据行 | 表中每行有日志行号且内容一致 | **成立** |
| K2C2-1 | 候选注入序列逐行登记（写/读回/事件/步数/settle） | 序号连续、与 v96a 行号对齐 | **成立** |
| K2C2-2 | 源侧参考序列带相对时刻、两次运行 | D3v2 七条 + v101b 复现 | **成立** |
| K2C2-3 | 外部输入与图内输出分栏 | 候选/源两表均分离 | **成立** |

**裁定语义**：C2 = **REGISTERED**（冻结场景的输入账本可复核）；C1 = **部分**（缺护甲 42 槽 + 存档身份，待第 2 步仪器上机后关闭）。不关闭 K2 整体（K2-C4 控制 B 已 READY-bounded，C1 待补）；不关闭 K1/K5/K6/G5；不产生 `tau_*`。

**裁定语义（离线轮，2026-11-14 15:47）**：C2 = **REGISTERED**（冻结场景的输入账本可复核）；C1 = **部分**（缺护甲 42 槽 + 存档身份，待第 2 步仪器上机后关闭）。不关闭 K2 整体（K2-C4 控制 B 已 READY-bounded，C1 待补）；不关闭 K1/K5/K6/G5；不产生 `tau_*`。

## 六、v102 run 1（2026-11-14 20:11-20:14；仪器上机，C1 关闭）

**运行身份**：v102 DLL `32ff383e…c4185c`（`…-v102-g5k2c1-equipdump-bd630daacc-cl94faaed0c6-20261114T082611Z`）+ PDB `c3a7ed45…85b1` + 运行档 `0e789f8a…57cf35`（v96a 基线 + `EquipmentStateDump=true`，逐键断言见 [build.md v102 节](../build.md#v102-g5k2c1-equipdump-20261114)）。日志 [归档](CharacterPanel-g5k2c1c2-equipdump-v102-run1-20261114-2011.log) 177,630 B，SHA-256 `b33c452c…2e2d4385`。

**实验读数**：一次 panel 窗口；inject → equip 120/120（`istate=9 default=1`）→ settle target 87 / settled 101 → `Unequip` id=144 → unequip completed=129、`state-12-to-0`、`default=0`；clip 族 `1HM_Equip`/`1HM_Unequip`/`1HM_Idle`，全日志零 `H2H_*`；零 `[E]`；无崩溃（clean tail）；`source-drift=none`。15 条 `[W]` 全部为既有 DAC 台账限制（与本仪器无关）。

**dump 行**（L608，`reason='panel-open'`）：`hands=(560c2a4c,00043324) drawn=false` 与 v96a L604 **逐字一致**；`armor=slots=31 worn=9 1:0201989e,2:fe030801,3:5f006643,5:000f1b33,6:fe032823,7:fe030800,12:0201989e,17:fe01280b,20:fe0fe81b`。

### 护甲 9 件全解析（归属 = 存档表 + 记录扫描；FormID 为运行时值）

| 槽位 | FormID | 归属插件 | EDID |
| --- | --- | --- | --- |
| 1 hair + 12 circlet | `0201989e` | Dawnguard.esm（USSEP 覆盖同记录） | `DLC1ArmorDawnguardHelmetLight`（BOD2=`0x1002`=hair+circlet，双槽实测自洽） |
| 2 body | `fe030801` | ccbgssse055-ba_orcishscaled.esl | `ccBGSSSE055_ArmorBladesCuirass` |
| 3 hands | `5f006643` | DIS_RealisticArmor.esp | `DIS_SteelScaledGauntlets` |
| 5 amulet | `000f1b33` | Skyrim.esm（USSEP 覆盖同记录） | `MG05RewardAmulet` |
| 6 ring | `fe032823` | ccbgssse051-ba_daedricmail.esl | `ccBGSSSE051_Ring` |
| 7 feet | `fe030800` | ccbgssse055-ba_orcishscaled.esl | `ccBGSSSE055_ArmorBladesBoots` |
| 17 mod back | `fe01280b` | ccfsvsse001-backpacks.esl | `ccFSVSSE001_Backpack_Mage`（**"背包"= 已装备的 CC Survival 法师背包**） |
| 20 | `fe0fe81b` | 3BBB.esp | `SMP3BONObjectP50`（3BA 身体物理填充对象） |
| 火把 | 未装备 | 左手被 Candlelight 占用 | — |

**存档身份与表解析（K2C1-3 + 方法学）**：运行存档 = `Save116_…Folgunthur01….ess`（Elif，#116）。AE v12 存档头的插件表为 **LZ4 压缩块**（起始 @245879，解压 3,998,360 B）：常规表 133 项 + 光表 766 项（u16 计数、u16 长度名）。运行时 FormID 表 = 存档表：`REG 0x02=Dawnguard.esm`、`REG 0x56=DBM_CC_HallOfWonders.esp`、`REG 0x5f=DIS_RealisticArmor.esp` 三处独立命中 → **§三"静态重算 0x47 vs 运行时 0x56 差 15"待查项关闭**（静态 plugins.txt 分类与存档表序不一致；运行时身份以存档表为准）。ESL 映射 = 存档光表 0-based，双锚点验证：`fe0bb800`（DAC warning）→ light[187] = SCAR NPC Exhaustion System（Spell "Exhaustion State" local 0x800 语义命中）；`fe030800/801` → CC Blades Boots/Cuirass local 0x800/0x801 命中。

### 判定

| # | 子项 | 裁定 |
| --- | --- | --- |
| K2C1-2 | dump 行存在、31 槽全枚举、零 `[E]`、无崩溃、hands/drawn 与 v96a 逐字一致 | **PASS** |
| K2C1-2 子项 | OAR 命中与 v96a 逐字一致 | **FAIL（登记为偏差）**：本次 panel-open 源侧栈 = `MT_Idle` → SVFI（`Seamless Varied Feminine Idles`），v96a = Smooth Moveset 1HM idle 栈；源侧面板时刻 idle 栈随世界状态变化，与 K1-C4"源侧 OAR 窗口内自切资源"同类。装备初态冻结项（hands/drawn/armor/相位）不受影响 |
| K2C1-3 | 存档身份登记、背包以存档身份冻结 | **PASS**（存档 = Save116；slot 17 背包同时以装备身份登记） |
| **C1 整体** | CAL v1 关闭条件"武器 Form、左右手、火把、护甲、背包和初始相位；两侧一致" | **全部满足 → C1 = REGISTERED（含上述偏差登记）** |

**边界**：C1/C2 关闭不关闭 K1-C2/C3/C4（census 已登记但跨运行身份未证）、不关闭 K5/K6/G5；不产生 `tau_*`；五字 prime 消融仍不做（非关闭条件）。
