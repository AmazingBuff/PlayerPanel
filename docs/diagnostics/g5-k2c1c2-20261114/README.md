# C-G5-CAL K2-C1/C2：装备初态与输入账本（2026-11-14 预注册）

**状态（2026-11-14）：预注册 + 离线登记完成。C2 = REGISTERED（离线关闭）；C1 = 部分（缺护甲/存档身份，待仪器上机，见 §四）。** 本轮闭合 [CAL v1 §三 K2](../../independent-behavior-graph.md) 的前两子项：C1 装备初态清单、C2 输入账本。**纯离线：零游戏运行、零插件代码改动**（第 2 步仪器已预注册但未实现）。

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
| 护甲 42 槽 | **缺**（待第 2 步仪器） | — |
| 存档身份 | **缺**（待第 2 步运行时由用户登记槽位名；背包以存档身份冻结） | — |
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

## 四、第 2 步预注册：装备全态仪器（待实现/待上机）

| 项 | 固定内容 |
| --- | --- |
| 新键 | `[Experiment] EquipmentStateDump=true`，默认 off；只读 |
| 观测点 | 现有 `ANIMATION WORLD` panel-open 快照（[controller.cpp L880](../../src/animation/controller.cpp#L880)）追加 `armor=(slot:formid,…)`：`GetWornArmor(BipedObjectSlot)` 枚举 0..41，非空槽记 slot:FormID；行追加 `save=`（用户在运行记录里登记存档槽位名） |
| 上界 | armor 字段 ≤42 项；不新增 hook、不写图、不采样姿态 |
| 运行协议 | 与 v96a 完全相同的冻结场景（同存档、1H+Candlelight 收鞘、同 INI 基线 + `EquipmentStateDump=on`）；一次 panel 窗口 |
| 判据 K2C1-2 | dump 行存在、42 槽全枚举、零 `[E]`、无崩溃；与 v96a 的 hands/drawn/OAR 命中逐字一致 |
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
