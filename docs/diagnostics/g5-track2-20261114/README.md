# C-G5 Track 2：同资源姿态对照批次预注册（2026-11-14 草案，待评审）

**状态（2026-11-14）：预注册（评审通过）。** 用户于 2026-11-14 批准 §九决策 1：新建 `AE-Track2`（复制 `AE`）、停用 4 个 OAR 模组（主插件 + RaySense + IED conditions + Math Plugin），其余 977 项不动。运行档已派生（[build.md Track 2 节](../../build.md#track-2-运行档派生2026-11-14纯-ini零代码零构建)，三个逐字节复制档与既有登记哈希一致）；实施按 §八进行：用户建档 → 离线 winner-map 重 diff + 闭包复跑 → Run 0 前置门 → Run 1/2。本文档冻结批次设计、判据、预算与停止规则。

**前置阅读**：[K6 计划 §二/§十三](../g5-k6-plan-20261114/README.md)、[CAL v1](../../independent-behavior-graph.md#c-g5-cal-v1)、[K1 census §八](../g5-k1-census-20261114/README.md#八k1-c4-源候选资源对照与裁定)、[K1-C1 登记](../g5-k1-c1-20261114/README.md)、[K2-C1/C2 登记](../g5-k2c1c2-20261114/README.md)、[K3 收官](../../independent-behavior-graph.md#g5-cal-k3-closure)、[第三会话总结 §六](../session-summary-20261114-k6.md)。

## 一、目标与边界

| 项 | 交付 | 不覆盖 |
| --- | --- | --- |
| 确定性资源条件（B） | 停用动态替换的新 MO2 档；两侧同资源（逐族 `data FNV` 相等）有运行证据 | 主档 `AE` 不动；不改任何模组内容，只改新档启用集 |
| 新档重登记 | census 重登记（Run 0a/0b）+ 新档资源断言集合（按相位分列）+ winner-map 重 diff + 闭包走链复跑 | 不改写 v101/v101b/K1-C1 的 Track 1 登记 |
| 新档重验 | hands/drawn/armor 与 K2-C1 登记比对；clip 家族逐相位两侧相等；装备/输入冻结不受影响的确认 | 五字 prime 消融不做（K2-C1 既有登记不变） |
| 两侧完整姿态/身份证据 | 复用 v103 K5 仪器，Run 1（候选）/ Run 2（源）各一次窗口 | 配对/比较协议（门 4 冻结）与等价裁定不在本批 |
| C 分组配对资格账本 | 跨侧族对照表：每族两侧 FNV 集合 / 交集 / PAIRED 资格 | 同刻逐代配对（[K3 收官](../../independent-behavior-graph.md#g5-cal-k3-closure)已裁定须另立契约） |

**明确不做**：不产生 `tau_p/tau_r/tau_s/epsilon_t`；不做姿态等价或 G5 判定；不做同刻逐代配对；不改产品代码（**零构建**——v103 已含全部所需仪器键）；不改写 Track 1 任何历史裁定；Run 0 失败后不采姿态数据；不删帧/换资源/偏移时间。

## 二、MO2 档方案（B 路线，待用户批准）

### 2.1 方案依据（本轮从已登记数据派生的事实，可复核）

| # | 事实 | 来源 / 推导 |
| --- | --- | --- |
| 1 | 生效视图内 **113 个模组**提供 OAR/DAR 内容，共 **16,663 条**生效路径 | `winner-map.tsv`（K1-C1 登记，SHA-256 `c0285fa0…7298642`）；推导命令 `awk -F'\t' 'tolower($1) ~ /openanimationreplacer\|dynamicanimationreplacer/ {print $2}' … \| sort -u \| wc -l`，本轮执行读数 113 / 16,663。逐一停用内容不可行且无法证完备 |
| 2 | 动态替换引擎只有 **Open Animation Replacer**（modlist 内无独立 DAR 引擎；`dynamicanimationreplacer/_customconditions/` 路径由 OAR 兼容扫描承载） | `profiles/AE/modlist.txt`（K1-C1 §二登记：`+` 项 981 个）grep：OAR 主插件 + RaySense / IED conditions / Math Plugin 三个扩展；DAR 主插件无命中 |
| 3 | `MaxsuBlockOverhaul` 同时提供行为闭包成员（maxsuBlockHit）与 OAR 内容 → **内容级停用会破坏闭包**；引擎级停用不触碰磁盘行为文件 | winner-map `Behaviors/` 提供者 ∩ OAR 提供者交叉 |
| 4 | `SCAR_1hmReadyDummy` 由 OAR 模组承载（Dummy Animation SCAR - OAR / BFCO Elden Rim Dual Wield）→ 引擎停用后 dummy 不再出现；新档 census 须**自相对**重登记，不套用 Track 1 名集合 | `fnv-reconcile.tsv`（K1-C1 登记）第 5 行 |

### 2.2 方案

**新 MO2 档 = 复制 `AE` → `AE-Track2`，仅停用以下 4 个模组，其余 977 项不动**：

| 停用 | 理由 |
| --- | --- |
| Open Animation Replacer | 唯一动态替换引擎；停用即"零动态替换"可由运行时证据证明（全日志无 `oar-result=replacement`、无 OAR 命中行） |
| Open Animation Replacer - RaySense | 引擎 API 扩展，随主插件一并停用 |
| Open Animation Replacer - IED conditions | 同上 |
| Open Animation Replacer - Math Plugin | 同上 |

**保留不动**：Pandora_Output 与行为闭包 23 文件全部提供者（Moddern.Stagger.Lock.Framework、MaxsuBlockOverhaul、SkyParkour V3、FNIS 系、ItemUse 系、A-Pose Fix 等）；SCAR / BFCO / Maxsu 系 SKSE 插件；Vanargand / Smooth / SVFI / Ashe / Diverse 等全部内容模组（其 OAR 内容在新档下无引擎承载，成为死数据）；Save116 与装备/护甲相关模组。存档跨档通用，Save116 原槽加载。

### 2.3 风险登记（判据守门，不预判通过）

| 风险 | 观察点 | 失败处置 |
| --- | --- | --- |
| 依赖 OAR API 的插件（SCAR 系 / BFCO 等）在无引擎时出错或崩溃 | Run 0 零 `[E]`、无崩溃 | 停止；登记原因；方案修订后另立预注册版本 |
| 基图 authored clip 路径在无替换下缺文件 | census K1C-3 自相对 `missing>0` | 同上 |
| clip 家族名 / 相位序列与 Track 1 不同（OAR 子图贡献的段消失） | T2-C3 逐相位两侧家族相等 + 相位序列完成 | 同上 |
| 无条件 loose 覆盖（如 Vanargand `female/mt_idle.hkx`）仍生效 | 两侧同样生效 → 同资源成立即不阻断；登记进新档断言集合 | —（只登记） |

### 2.4 备选与后果

- **内容级停用（备选）**：需停用约 113 个模组，且 MaxsuBlockOverhaul 冲突会破坏闭包 23/23 → 不可证完备、破坏 §五转移声明前提，**不推荐**。
- **否决 B（用户不愿改 MO2 档）**：Track 2 退化为 C-only——现状下跨侧同字节组仅 SCAR dummy，G5 姿态等价按门 1 规则**停止正式路径**、另立审计范围（K6 计划 §二已登记后果，如实执行，不放宽判据）。

## 三、Run 0：新档 census 重登记与重验（姿态采集前置门）

**顺序**：① 用户建 `AE-Track2` 并停用 4 模组，登记新 modlist → ② 离线 winner-map 重 diff + 闭包走链复跑（复用 gitignored `build/k1-c1/enum_k1c1.py` 与 `build/hkx-audit/HkxInventory`，零游戏运行；锚点校验先行：SCAR dummy `8728A54A50D85C22`、Vanargand female `F56EE1500EB5A3D0` 在新档磁盘视图仍须命中——两模组未停用，仅验证映射方法）→ ③ 派生 Track 2 运行档（§六）→ ④ Run 0a / Run 0b 上机 → ⑤ 新档断言集合登记进 `tools/k5_reconcile.py` Track 2 表。

**协议**：Run 0a = v101 候选协议逐字（inject 档 + `ClipIdentityCensus=true` + `EquipmentStateDump=true`，K5 三键 off；Save116）；Run 0b = v101b 源协议逐字（source 档 + census，自然游玩 F7 窗口，触发收敛与 cap 沿 v101b 现行为）。

**判据（全部预先固定；任一失败 → 停止）**：

| # | 判据 | 通过条件 |
| --- | --- | --- |
| T2-C1 census 完整 | 两侧 K1C-1/2/4 自相对成立：四段图身份齐全、`bones==parents==reference`、`recorded==bones`、`invalid-parent=0`、`nonfinite=0`、`overflow=0`、一次实验零 `[E]` | 全过 |
| T2-C2 clip 身份自相对 | 每活动 clip `identity=true`、`unique==clips`、`missing=0`、`overflow=0`（**不要求** Track 1 的 `1HM_Equip`/`1HM_Unequip` 名集合） | 全过 |
| T2-C3 家族逐相位两侧相等 | equip 相位与 unequip 相位两侧活动 clip 裸名族一致；相位序列完成（equip 预算跑完、unequip 完成事件，或如实登记差异） | 全过 |
| T2-C4 跨侧资源相等（B 路线核心） | 两侧同族 clip `data FNV` **逐族相等**；任一族不等 → 停止 | 全过 |
| T2-C5 装备初态重验 | dump 行 hands/drawn/armor 与 [K2-C1 登记](../g5-k2c1c2-20261114/README.md#三离线账本本轮登记内容) 逐字一致（armor 31 槽 9 件全表）；OAR 相关字段（oar-api/oar-project/命中）按新档实际输出**单独登记**、不参与逐字比对；另判全日志零 `oar-result=replacement` | 全过 |
| T2-C6 行为闭包不变 | 新档闭包走链 23/23 与 A1 / K1-C1 登记逐位一致 | 全过 |
| T2-C7 落位 | census 报出的全部 (data FNV, 字节) 在新档 winner-map diff 中有唯一或显式"多选一"落位 | 全过 |

**裁定语义**：T2-C1..C7 全过 = **确定性资源条件成立**，新档断言集合（按相位分列的 FNV 集合 + 已声明栈名集）登记进 reconcile 工具 Track 2 表；任一失败 = 批次停止，修订方案 → 预注册修订版 → 重跑 Run 0，**已收数据不进入姿态批次**。

## 四、Run 1 / Run 2：姿态采集与 C 分组账本

**协议复用**：Run 1 = v103 run 1 协议逐字（k5-inject 档，Save116，v96a 链：inject → 同帧 5 写 prime + `WeapEquip` → equip 120/120 → settle → `Unequip` id=144 → unequip → 关窗）；Run 2 = v103 run 2 协议逐字（k5-source 档，SamplingLiveness + K5，F7 20 s 窗口）。判据 = **K5-1..K5-8 逐字复用**，唯一修订：K5-7 断言集合换为新档集合（候选侧出集 → INVALID；源侧出集但裸名 ∈ 新档已声明栈名集 → 声明缺口登记，规则同 K6 §10.5）。

**C 分组账本（离线，`tools/k5_reconcile.py` Track 2 版本）**：跨侧族对照表——每族输出 {裸名, 候选 FNV 集, 源 FNV 集, 交集, 配对资格 PAIRED/NOT-PAIRED, 同名多实例的 stable-id 消歧计数}；Run 0 之后出现的新 FNV 按 C 规则排除出配对资格并登记。**预期：T2-C4 已证逐族相等，账本应全族 PAIRED；任何 NOT-PAIRED 如实登记并解释。**

**交付**：两侧完整姿态/身份工件（K5 格式 v1）+ 跨侧族对照表 + 配对资格账本。这批数据是门 4（完整协议冻结）设计配对规则时的输入材料；**本批不消费它做任何等价判断**。

## 五、层/权重/混合资格转移声明（Track 1 → Track 2）

- **声明**：v103 两侧的 K5-5 层/混合资格结论（含 `hkbBlendingBehavior` 等显式缺项清单）转移至 Track 2 批次。
- **依据**：行为闭包 23 文件逐位一致（T2-C6 复验）；OAR 替换只作用于动画数据、不改图结构（K6 计划 §二已登记事实）；混合语义由图结构决定，不随动画数据变化。
- **复核**：T2-C6 失败 → 转移声明失效，K5-5 结论退回 Track 1 限定范围，Track 2 层/混合资格须重采；缺项清单随转移，仍是门 1 未闭合子项。

## 六、预算、运行档与工件

| 项 | 值 | 来源 |
| --- | --- | --- |
| 候选姿态工件 | cap 384 × 5,664 B ≈ 2.07 MiB（预期 ~250 条） | K6 §10.3 |
| 源姿态工件 | cap 1,400 × 5,652 B ≈ 7.55 MiB | K6 §10.3 |
| 对账账本 | ≤0.4 MiB JSON | K6 §10.6 |
| census | 512 节点 / 64 clip / 512 骨；行数 ~450/侧，超限即失败 | K1 census §四 |
| 身份/时间/对账桶 | 三项合计 ≤1 MiB；文本摘要 ≤200 行 | CAL v1 §五.7 |
| winner-map 重 diff | **不设行上限、全量保留**（吸取 K1-C1 预算行失效教训），登记总行数与哈希 | 本文档修订 |

**运行档（纯 INI 派生，零代码、零构建）**：四个新档文件入 `dist/g5-track2-20261114/`（census-inject / census-source / k5-inject / k5-source），键位 = v103 既有键组合（`IndependentGraph`、`ClipIdentityCensus`、`EquipmentStateDump`、`SamplingLiveness`、`K5FullSample`/`K5LayerAudit`/`K5Reconciliation` 等），逐键值在派生登记中列全；未启用键全部保持默认 off，off 路径与 v103 逐字节一致。

**零构建声明**：v103（DLL `8c3857a3…`）已含全部所需仪器；Track 2 不新增任何代码、hook、INI 键。

## 七、停止规则

任一 Run 0 判据失败、任何 `[E]`、崩溃、需要写源图或未知 ABI、存档/模组身份与登记不符、reset/close/PreLoadGame/NewGame → 停止当前窗口并保留已收计数与原因；不拼接窗口、不续裁。修订须另立预注册版本；已看数据不按新规则回填。

## 八、实施顺序与提交拆分（评审通过后）

1. 用户建 `AE-Track2` 档并停用 4 模组；登记新 modlist（`+`/`-` 差异清单）。
2. 离线：winner-map 重 diff + 闭包走链复跑（锚点校验先行；工件哈希登记）→ `docs(g5-track2): register AE-Track2 winner-map diff and closure re-walk`。
3. 派生 4 个运行档 → `docs(build): derive Track 2 run profiles (INI only)`。
4. Run 0a / Run 0b 上机 → 新档断言集合登记 → `docs(g5-track2): record Run 0 census and assertion set`。
5. Run 1 / Run 2 上机 → K5 判据 + C 分组账本 → `docs(g5-track2): record Run 1/2 and pairability ledger`。
6. 收官裁定 + 文档同步（主文档状态行、README、history、CHANGELOG）。

## 九、待用户决策事项

| # | 决策 | 影响 |
| --- | --- | --- |
| 1 | **§二档方案批准**：新建 `AE-Track2`（复制 `AE`），停用 4 个 OAR 模组（主插件 + RaySense + IED conditions + Math Plugin），其余 977 项不动 | **已批准（2026-11-14）**，按 §八实施 |
| 2 | 档名 `AE-Track2` 确认（可改名，登记以实际为准） | 工件命名 |
| 3 | （会话外既挂）60 提交 push 与否 | 与本批次无耦合 |

## 十、执行登记 ①：AE-Track2 建档与离线资源身份登记（2026-11-14）

**状态：§八步骤 ① ② 完成（用户建档 + 离线 winner-map 身份 + 闭包走链复跑 + 锚点校验）。Run 0 未运行；本节不采样任何游戏内数据。**

登记偏差（过程事实，如实记录）：用户最初把 4 个 OAR 模组停在了主档 `AE` 内（批准方案的偏差），随后按更正顺序完成——先复制当前 `AE`（继承 4 项停用）为 `AE-Track2`，再把主档 4 项重新启用。终态经核验符合 §2.2。

### 10.1 档案与 modlist diff（步骤 ①）

| 核验项 | 读数 |
| --- | --- |
| `AE`（主档） | `+` 项 981，4 个 OAR 模组全部重新启用（恢复与 Track 1 证据链一致） |
| `AE-Track2` | `+` 项 977；`-Open Animation Replacer` / `-RaySense` / `-IED conditions` / `-Math Plugin` 恰为第 788/790/791/792 行 |
| modlist diff | **恰好 4 行**（`+`→`-`），无其他差异 |
| settings.ini | 与主档逐字一致（`LocalSaves=true`） |
| 存档 | `profiles/AE-Track2/saves/` 含 `Save116_61F78427_0_456C6966_Folgunthur01_…ess` + `.skse` 双文件 |

### 10.2 winner-map 身份登记（步骤 ②-1，离线零游戏运行）

工具：`build/k1-c1-track2/enum_track2.py`（SHA-256 `493c704c33c38595…`，= enum_k1c1.py 仅改 PROFILE→`AE-Track2\modlist.txt` 与 OUT_DIR 两常量；gitignored 本地工具，方法与 K1-C1 登记相同）。输出在 gitignored `build/k1-c1-track2/out/`，按哈希登记：

| 工件 | Track 2 重生成 SHA-256（前 16） | 与 K1-C1 登记工件对比 |
| --- | --- | --- |
| winner-map.tsv（24,204 路径） | `786f31934e9ceb76…` | **winner 赋值逐行一致**（路径→提供者→字节→SHA-256，忽略 `pos` 列 diff 为零）；`pos` 序号差异 = 4 个引擎模组移除后其余模组序号前移（见下） |
| providers-all.tsv（24,375 行） | `b9051ec71b48c448…` | 同上：赋值逐行一致，仅 `pos` 漂移 |
| oar-config.tsv（1,472 行 / 1 解析失败，同 K1-C1） | `839090d66d7d2628…` | **逐字节一致**（`839090d6…6d05538` 全哈希相同） |

**pos 漂移的完备解释**：4 个引擎模组在两张 providers-all 中都贡献 **0 行**（逐模组计数 `0/0`）——它们是纯 SKSE DLL 模组，不含任何 `meshes/actors/character/**` 内容。移除 4 行只使后续模组的枚举序号整体前移 ≤4，不改变任何相对顺序与 winner 裁定。**结论：AE-Track2 的磁盘生效视图与 K1-C1 登记视图在语义上完全相同；动态替换的移除只发生在运行时引擎层，磁盘无任何内容变化。**

**锚点校验（预注册 §三步骤 ②）**：两条锚点行在 Track 2 winner map 中逐字一致——SCAR dummy（`…/openanimationreplacer/050 ermoveset_crossnaginata/base/scar_1hmreadydummy.hkx`，BFCO Elden Rim Dual Wield，29,008 B，`4061ce1b…`）与 Vanargand female mt_idle（`meshes/actors/character/animations/female/mt_idle.hkx`，113,920 B，`0EF8248C…BCD73C7` = 控制 B 登记文件哈希）。映射方法有效。

### 10.3 行为闭包走链复跑（T2-C6，步骤 ②-2）

生效 `0_Master.hkx` = `E:\SkyrimAE\mods\Pandora_Output\…\0_Master.hkx`，SHA-256 `466cf116…`（与 [closure-files.txt](../../diagnostics/g5-k1-c1-20261114/closure-files.txt) START 行一致）。HkxDump（`build/hkx-audit/HkxDump/bin/Release/net10.0/`，同一 net10 工具）重跑走链：`LOADED-FILES 23`。

**裁定：T2-C6 成立。** 闭包 23/23 与登记集**路径集合一致、逐文件 bytes+SHA-256 一致**、23 路径全部在 Track 2 winner map 内。走链工件：`0_master_track2_dump.txt`（SHA-256 `0d1308dd1f6afc1a…`）与提取表 `closure-track2.txt`（23 行，SHA-256 `c058d36ef1b663c6…`，均在 gitignored out/ 目录）。

**转移声明（§五）前提成立**：行为闭包逐位一致复验通过；层/权重/混合资格转移可按声明生效（最终裁定在 Run 0 全判据后作出）。

### 10.4 下一步（Run 0 上机，等待用户）

1. 换档：MO2 切 `AE-Track2`；安装位 INI 换为 `dist/g5-track2-20261114/CharacterPanel-track2-census-inject.ini`（换档纪律见 [build.md Track 2 节](../../build.md#track-2-运行档派生2026-11-14纯-ini零代码零构建)：先备份、后核对安装位哈希与日志身份）。
2. **Run 0a**（候选侧 census + T2-C5 装备重验）：Save116 → panel 窗口（inject 链同 v96a）→ 关窗；判据 T2-C1/C2/C3（候选侧）、T2-C5、T2-C7。
3. **Run 0b**（源侧 census）：换 `CharacterPanel-track2-census-source.ini`，Save116 → F7 20 s 窗口自然装备/卸下；判据 T2-C1/C2/C3（源侧）+ T2-C4（跨侧逐族 `data FNV` 相等，离线裁定）。
4. 全部通过 → 新档断言集合登记进 reconcile 工具 → Run 1/2 姿态采集。

## 十一、Run 0a run 1（2026-11-14 23:29，**INVALID——两项判据失败；仪器与解析全部成功，产出三项设计级发现**）

**运行身份**：v103 DLL（`8c3857a3…`）+ track2-census-inject 档（`c05b9b91…`；首次部署未落位——安装位仍是 v103 source 档，安装位哈希核验抓出后由 Agent 补部署，备份 `dist/deploy-backup-20261114-v103-run0apre/`）。档位 `AE-Track2`。日志 [归档](../CharacterPanel-g5track2-census-inject-run1-20261114-2329.log)（202,711 B，SHA-256 `830ccc3356c94f6f…`）。

### 判据裁定

| # | 判据 | 读数 | 裁定 |
| --- | --- | --- | --- |
| T2-C1 | census 骨架/图身份 | 两侧 `bones=116 recorded=116 invalid-parent=0 nonfinite=0`；四段齐全（source pre-activate、candidate post-activate/equip-clips/unequip-clips）；overflow=0 | PASS |
| T2-C2 | clip 身份自相对 | equip-clips `clips=2 missing=0`；unequip-clips `clips=4 unique=4 **missing=1**`——`SCAR_1hmReadyDummy` 行 `identity=false`、`binding=0x0 animation=0x0 data-fnv=0`（裸名 `Animations\SCAR_1hmReadyDummy.hkx` 无任何磁盘来源，见 §11.2） | **FAIL** |
| T2-C3 | 家族逐相位 | equip-clips 含 `1HM_Equip`、unequip-clips 含 `1HM_Unequip`；相位序列完成（equip 120/120、unequip `state-12-to-0` completed=120 first-return-zero=120）；H2H=0；drift=none ×5 | PASS |
| T2-C4 | 跨侧资源相等 | Run 0a 不裁（源侧 equip/unequip 相位在 Run 0b）；旁证：source pre-activate 的 MT_Idle `data-fnv=F56EE150…` 与候选一致 | 待 Run 0b |
| T2-C5 | 装备初态重验 | dump 行 `hands=(00000000,00000000) drawn=false armor=slots=31 worn=2 2:0003c9fe,7:0003ca00` —— **与 K2-C1 冻结场景（`560c2a4c,00043324`、worn=9）不符：本次会话未处于冻结场景** | **FAIL** |
| 完整性 | 零 `[E]` | **1 条**：L4 `failed to get plugin info for OpenAnimationReplacer`（23:28:44，SKSE 装载期、任何采集之前）——OAR 缺失时插件 OAR API 探测的确定性伴生行（伴生 `[I]` 行 `OAR hooks unavailable: Open Animation Replacer is not loaded`）；运行中零 `[E]` | 按字面 FAIL（修订见 §十二） |

**裁定：Run 0a run 1 = INVALID**（T2-C2、T2-C5、完整性判据），按 §七停止规则不进入 Run 1/2，已收数据不进入姿态批次。但仪器链路全部成功（census/装备 dump/inject 链/schema=2 观察全链无缺陷），且产出三项设计级发现（§11.1–11.3），修订后重跑。

### 11.1 发现一：候选侧资源与 OAR 无关（K1-C1 归属更正）

OAR 关闭后候选 FNV **与 Track 1 逐位不变**：MT_Idle=`F56EE150…`（`Animations\female\mt_idle.hkx` 普通路径 loose，Vanargand）、MLh_Idle=`DAD1F1C9…`（`mlh_idle.hkx` 普通路径 loose，Pretty Combat——与 Diverse Magic Casting 的 DAR 文件同字节）、1HM_Equip=`12A77CD1…`、1HM_Unequip=`4DED5EAF…`、1HM_Idle=`C62C233B…`。

本轮从原版 `Skyrim - Animations.bsa` 定点提取并解码（`build/k1-c1-track2/out/vanilla-fnv.tsv`）：**原版第三人称 `1hm_equip.hkx`/`1hm_unequip.hkx`/`1hm_idle.hkx` 的 data FNV 就是 `12A77CD1…`/`4DED5EAF…`/`C62C233B…`**（1.4333 s / 2.0 s / 9.0 s 全对齐）。即这三个 clip 的运行时来源是**原版 BSA**（普通路径无 loose 赢家 → 引擎回落 BSA）；[K1-C1 §六](../g5-k1-c1-20261114/README.md#六执行结果与裁定) 把 `12A77CD1…`/`4DED5EAF…` 归属到 SCAR DAR / Elden Rim OAR 路径属"多选一"归属偏差——那些模组复制了原版同字节文件。**候选图从未被 OAR 替换过；源/候选资源差异的全部来源是源图被 OAR 替换。**

### 11.2 发现二：SCAR dummy 是唯一无来源 clip

`Animations\SCAR_1hmReadyDummy.hkx` 在普通路径、gender 变体、原版 BSA、全部 102 个模组 BSA 中均不存在（本轮穷举）；唯一磁盘来源是 OAR 内容目录（Dummy Animation SCAR - OAR / BFCO Elden Rim Dual Wield），须 OAR DLL 才会被装载。K1-C4 中它"两侧字节相同"的原因正是两侧都经 OAR 拿到同一文件；OAR 关闭后两侧都 `identity=false`（对称缺失）。这是预注册 §2.3 风险 2 的命中形态，处置修订见 §十二。

### 11.3 发现三：OAR 停用的运行时证明齐全

`OAR hooks unavailable`（启动 `[I]`）+ 启动期唯一 `[E]`（OAR plugin info 探测失败）+ ANIMATION WORLD 行 `oar-api=unavailable oar-result=unavailable-unknown oar-path='' oar-project='' oar-variant='' oar-submod='' oar-mod=''` + 全日志零 `oar-result=replacement`。产品路径 unarmed 回落行为按预期出现（`unarmed replacement unavailable; direct MT_Idle fallback`——非冻结场景手部为空所致；冻结场景 1H 在手不会出现）。

## 十二、预注册修订 v2（2026-11-14 批准生效）

用户于 2026-11-14 批准：§12.1 `[E]` 伴生行豁免、§12.2 **方案 b**（环境修复）、§12.3 冻结场景重跑要求。Run 0a run 1 的三项发现需要修订批次设计。修订只发生在任何姿态采集之前；已收 run 1 数据保持 INVALID 原判，不回填。

### 12.1 启动期 `[E]` 伴生行容忍（设计后果，收窄到精确字符串）

**修订**：T2-C1/K1C-4 与 T2-C7 的"零 `[E]`"判据豁免唯一精确行——`[E] failed to get plugin info for OpenAnimationReplacer`（SKSE 装载期、任何采集之前、OAR 缺失的确定性探测伴生；其存在与 §11.3 的 `[I]` 伴生行同为 T2-C5 的停用证明材料）。其余任何 `[E]`（含运行期）仍然停止。**理由**：该行是批准设计（移除 OAR 引擎）的必然后果，与被测数据无关；不豁免则任何 Track 2 会话都无法满足判据，判据将失去检出力。

### 12.2 SCAR dummy 处置（二选一，用户决策）

| 方案 | 内容 | 优点 | 代价 |
| --- | --- | --- | --- |
| **b（已批准）** | 新建本地模组 `CharacterPanel-Track2Dummy`：把 BFCO 的 dummy 文件（29,008 B，`4061ce1b…`）逐字节复制到普通路径 `meshes\actors\character\animations\SCAR_1hmReadyDummy.hkx`，**仅在 `AE-Track2` 启用**（modlist 追加 1 行；主档 `AE` 不动） | census 判据不动（missing=0 恢复）；两侧同字节（`data FNV 8728A54A…`）→ dummy 族跨侧配对成立（Track 1 唯一配对族的连续性）；姿态对照覆盖全部 4 clip 槽位 | 磁盘视图新增 1 行 winner（须重跑 enum 重登记；行为闭包不受影响——behaviors/ 目录无变化）；触碰用户环境（新模组文件夹） |
| c | 修订 T2-C2：声明 dummy 槽为设计性对称缺失（两侧 `identity=false` 对称、data-fnv=0 → C 规则排除出配对资格），census 其余判据不变 | 零环境改动 | 判据修订（缺项合法化）；姿态对照少 1 个 clip 槽位（两侧对称，对照仍自洽）；Track 1 唯一配对族消失 |

### 12.3 冻结场景重跑要求（T2-C5）

Run 0a 重跑（方案 b 或 c 落地后）必须：加载 **Save116**（`Save116_61F78427_0_456C6966_Folgunthur01_011659_20261112151836_26_1.ess`，AE-Track2 档内 saves/ 已有）、panel-open dump 行逐字等于 K2-C1 登记（`hands=(560c2a4c,00043324) drawn=false`、`armor=slots=31 worn=9 1:0201989e,2:fe030801,3:5f006643,5:000f1b33,6:fe032823,7:fe030800,12:0201989e,17:fe01280b,20:fe0fe81b`）。不满足即再次 INVALID，不采姿态数据。

### 12.4 方案 b 的重登记义务

方案 b 落地记录：模组文件夹 `E:\SkyrimAE\mods\CharacterPanel-Track2Dummy\` 已由 Agent 创建（含 `README.txt` 与逐字节副本文件，源 = BFCO `…/OpenAnimationReplacer/050 ermoveset_crossnaginata/base/SCAR_1hmReadyDummy.hkx`，29,008 B，副本 SHA-256 `4061ce1b…` 与源一致）；**待用户在 MO2 的 `AE-Track2` 档中勾选启用**。之后：① 建模组后重跑 `enum_track2.py`（modlist 977→978），winner-map diff 须为**恰好 1 行新增**（dummy 普通路径）且其余逐行不变；② 闭包走链复跑仍须 23/23；③ census 断言集合登记时 dummy 以新普通路径落位。**方案 b 落地前 Run 0b 不上机。**

### 12.5 方案 b 落地登记（2026-11-14，§12.4 义务完成）

| 项 | 读数 |
| --- | --- |
| 模组 | `E:\SkyrimAE\mods\CharacterPanel-Track2Dummy\`（Agent 创建：`meshes\actors\character\animations\SCAR_1hmReadyDummy.hkx` 逐字节副本 29,008 B `4061ce1b…` 与源一致 + `README.txt` 用途说明） |
| modlist | `AE-Track2` `+` 项 978（第 2 行 `+CharacterPanel-Track2Dummy`）；主档 `AE` 零引用 ✓ |
| winner-map 重跑 | 24,205 路径（+1）；**与 Track 1 登记的赋值 diff 恰好 1 行新增**：`meshes/actors/character/animations/scar_1hmreadydummy.hkx → CharacterPanel-Track2Dummy, 29,008 B, 4061ce1b…`，其余逐行不变（`pos` 序号漂移同 §10.2 解释）。SHA-256：winner-map `005bf3e6…600cc2`、providers-all `261a79c2…5a2d78`、oar-config `839090d6…6d05538`（仍与 K1-C1 逐字节一致） |
| 闭包复跑 | HkxInventory `closure` 模式（K1-C1 原装走链）对新 winner map：`files=23`，与登记集**路径集合 + 逐文件 bytes+SHA-256 一致**（`closure-track2-v2.txt` SHA-256 `ffc398d0…26973`）——T2-C6 在 +1 行视图下复验成立 |
| 安装位 | census-inject 档 `c05b9b91…` / v103 DLL `8c3857a3…` / PDB `49581217…` 复核一致（Run 0a 重跑无需换档） |

**裁定：方案 b 落地完成，Run 0a 重跑资格就绪。** 预期：census 的 dummy 槽 `identity=true`、`data-fnv=8728A54A50D85C22`（两侧同字节，跨侧配对族恢复）；T2-C2 `missing=0` 恢复。
