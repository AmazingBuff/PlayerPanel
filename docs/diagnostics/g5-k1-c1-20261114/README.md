# C-G5-CAL K1-C1 完整运行资源清单（2026-11-14 预注册）

**状态（2026-11-14 15:47）：已执行，C1-1..C1-5 全部成立 → K1-C1 = REGISTERED（限定范围，含两条登记偏差，见 §六）。**

本轮闭合 [CAL v1 §三 K1](../../independent-behavior-graph.md) 的 "runtime/OAR/资源清单" 项：[K1 census](../g5-k1-census-20261114/README.md) §八指明 K1-C1 缺 **project/behavior 文件与全部 OAR 替换源的逐一登记**；同时落实其裁定（跨运行身份只按 `data FNV` 或完整路径+内容哈希，不用 leaf 名/binding-index）。**纯离线：零游戏运行、零插件代码改动。**

## 一、目标与边界

| 项 | 本轮交付 | 不覆盖 |
| --- | --- | --- |
| K1-C1 行为/工程文件 | 生效行为闭包（起点 `0_Master.hkx`）逐一登记：路径、字节、SHA-256，并与 [A1 的 23 文件闭包](../../independent-behavior-graph.md#handoff-20261031-night)交叉核对 | 闭包之外无关行为文件的内容审计 |
| K1-C1 OAR 替换源 | 启用 mod 第三人称角色路径 `meshes/actors/character/animations/OpenAnimationReplacer/**` 全量：OAR 模块→子模块（priority/replaceOnLoop/conditions 摘要）→每动画文件（数据相对路径、字节、SHA-256、提供 mod、MO2 winner 标记） | `_1stperson`/dragon/draugr 路径只计总数不逐一登记；不改 OAR 配置 |
| K1-C1 非 OAR loose 覆盖 | 启用 mod + 游戏 Data 的 `meshes/actors/character/animations/**/*.hkx`（排除 OAR 目录）同格式登记 | 非 HKX 动画资源（.kf 等） |
| FNV ↔ 文件映射 | 对 B2/B3 全量解码，逐动画 FNV-1a64(spline `data`)（与插件 `control_sample_fnv1a64` 同算法）；与 v101/v101b 观测 `data-fnv` 对账 | binding map FNV（运行时对象，文件无对应物）；非 spline 动画只记类型/时长不产 FNV |

**明确不做**：不冻结 OAR 配置（冻结是用户决策，本清单是其前提）；不含 K2-C1/C2 账本（另包）；不产生 `tau_*`；不关闭 K5/K6/G5；不改写 v101/v101b 历史裁定。

## 二、登记范围（用户已确认）

1. **B2 = 全量**：启用 mod（`profiles/AE/modlist.txt` 的 `+` 项，981 个）第三人称角色路径 OAR 树全量逐一登记（磁盘预扫 8,373 个 HKX）。
2. **TSV 直接提交仓库**（预估 ~1.3 MiB 文本）。
3. 游戏根目录 `E:\SteamLibrary\steamapps\common\Skyrim Special Edition`（BSA 定点提取用）。

## 三、方法

- **生效视图**：数据相对路径按 Windows 大小写不敏感归一；提供者 = 启用 mod（按 modlist.txt 顺序，**靠后者优先**）+ `overwrite/` + 游戏 Data loose（最后）。每路径记 `winner` 标记；OAR config.json 同视图解析（被遮蔽的 config 也登记，标 winner）。
- **FNV 解码**：本地工具 `HkxInventory`（net10，引用 HKX2-Enhanced-Library `91fa5b0`，与 HkxAnimScan 同构；源码在 gitignored `build/hkx-audit/HkxInventory/`，**不进仓库**）。spline 动画取 `data`/`blockOffsets` 字节做 FNV-1a64（offset `0xcbf29ce484222325`，prime `0x100000001b3`，逐字节）。
- **闭包走链**：从生效 `meshes/actors/character/Behaviors/0_Master.hkx`（Pandora_Output）出发，沿 `hkbBehaviorReferenceGenerator.behaviorName` 递归（复用 HkxDump 解析规则），登记闭包内全部文件哈希，与 A1 的 23 文件（Pandora 17 + loose 6：modernstaggerlock/ItemUse/FNIS×3/skyparkour/maxsuBlockHit）交叉核对。
- **锚点校验（先于对账）**：`SCAR_1hmReadyDummy.hkx`（Dummy Animation SCAR - OAR）必须出 `8728A54A50D85C22`；`mods/Vanargand Animations II - Female Idle Walk and Run/.../female/mt_idle.hkx` 必须出 `F56EE1500EB5A3D0` 且 SHA-256 = `0EF8248C…BCD73C7`（控制 B 登记，兼验 MO2 状态未变）。**任一不过 → 映射方法 INVALID，本轮不裁 PASS。**
- **观测对账**：从 v101/v101b 日志原文提取全部观测 `data-fnv`（不用 README 转述），逐个裁决 `matched=<file>` / `not-found`。not-found 者先查 B3 之外的提供者，再查原版 BSA（`Skyrim - Animations.bsa` 等定点提取）；BSA 内文件登记其归档内路径 + 解码哈希。
- **OAR conditions 摘要**：记录条件类名与原语字段（字符串/数值），单条 ≤200 字符；解析失败单独计数，不猜。

## 四、预注册判据

| # | 判据 | 通过条件 |
| --- | --- | --- |
| C1-1 行为闭包 | 闭包走链完成、零解析错误；A1 的 23 文件全部在登记内，逐条核对哈希；差异（多/少/哈希变）逐条列出并解释 | 否则 C1-1 INVALID |
| C1-2 OAR 枚举 | 启用 mod 角色路径 OAR 目录全部枚举、零文件读取错误；config 解析失败数 ≤ 总 config 的 1% 且逐条列出；dragon/draugr/1stperson 只计总数 | 否则 INVALID |
| C1-3 解码健全 | B2+B3 全量文件解码错误计数 = 0 或逐条列出（错误文件仍登记哈希）；锚点 2/2 匹配 | 否则 INVALID |
| C1-4 观测对账 | 全部观测 FNV 每个有裁决（matched/not-found+原因），无未审视项；matched 文件必须为该路径的 winner | 否则 INVALID |
| C1-5 运行完整性 | 纯离线、零游戏运行、插件源码零改动；全部工件（TSV/日志/工具输出）哈希登记；工具源码留在 gitignored 目录 | 否则本轮无效 |

**裁定语义**：C1-1..C1-5 全过 = **K1-C1 REGISTERED**（本机、本 modlist、此时点的运行资源清单可复核且无丢失）。不表示 OAR 已冻结、不证明跨运行资源未变（后续运行须重跑清单并 diff）、不产生容差、不关闭 K5/K6/G5。

## 五、预算

| 项 | 上限 | 超限处理 |
| --- | ---: | --- |
| 行为文件 TSV | 100 行 | 超限即停，不截断 |
| OAR 清单 TSV | 20,000 行（预扫 8,373 HKX + config 行） | 超限即停 |
| FNV 映射 TSV | 20,000 行 | 超限即停 |
| 本 README | 200 行 | 分轮追加，不改写历史 |
| conditions 摘要 | 200 字符/条 | 截断标记 `…` |

## 六、执行结果与裁定（2026-11-14 15:47）

### 执行轨迹（离线，零游戏运行）

| 步 | 内容 | 结果 |
| --- | --- | --- |
| 1 | 预注册提交 `496e68d` | 判据 C1-1..C1-5 预先固定 |
| 2 | 锚点校验 | SCAR dummy → `8728A54A50D85C22` ✓、Vanargand female mt_idle → `F56EE1500EB5A3D0` ✓（blockOffsets FNV `F17EBD9ADA8CF785` 亦与控制 B 登记常量一致；SHA-256 `0EF8248C…BCD73C7` ✓）→ **映射方法有效** |
| 3 | 全量枚举（`build/k1-c1/enum_k1c1.py`，本地 gitignored） | 24,375 provider 行 / 24,204 生效路径；MO2 优先方向以 A1 运行时证据校准（**modlist.txt 先者赢**，BFCO/Pandora 冲突裁定与 S3 load-try 佐证一致） |
| 4 | 全量解码（`build/hkx-audit/HkxInventory`，本地 gitignored） | 19,973 文件 / 19,821 动画 / 19,062 spline；错误 150 全部逐条列明（146 ERR-XML = mnbv 系 Havok XML 假 .hkx，4 EndOfStream = jumpfallfix×3 + Left Hand Equipment Swap×1 截断文件） |
| 5 | 行为闭包走链（生效 `0_Master.hkx`，38 REF） | **23 文件**（Pandora_Output 16 含 0_master + 松散 7：modernstaggerlock/ItemUse/FNIS×3/skyparkour/maxsuBlockHit），0 未解析、0 歧义、0 加载错误；**与 A1 旧转储逐文件哈希对照 23/23 逐位一致**（A1 文本"17+6"为其枚举笔误，其成员集即 16+7） |
| 6 | 观测 FNV 对账（v101 候选 8 + v101b 源 8，并集 14） | **14/14 落位**：13 个 loose winner + 1 个原版 BSA（`Skyrim - Animations.bsa` 内 `meshes\actors\character\animations\1hm_idle.hkx`，LZ4 帧解压 20,576 B，spline data 14,448 B、9.0 s，FNV `C62C233B06FAE9A9` = 候选 1HM_Idle） |

### 判据裁定

| # | 判据 | 裁定 |
| --- | --- | --- |
| C1-1 | 闭包 23 文件全部在 B1 登记内、哈希与 A1 一致 | **成立**（missing=0 hash-diff=0） |
| C1-2 | OAR 枚举完整、config 解析失败 ≤1% | **成立**（150 个 OAR 模块 / 1,322 子模块 config / 1,472 行；解析失败 1 条 = `ashedialogueanims02/idle_barter`，0.07%；零文件读取错误） |
| C1-3 | 解码错误逐条列明 + 锚点 2/2 | **成立** |
| C1-4 | 14 个观测 FNV 全部有裁决 | **成立**（`fnv-reconcile.tsv`，matched 均为 winner 或原版 BSA 唯一来源） |
| C1-5 | 纯离线、插件零改动、工件哈希登记 | **成立** |

### 登记偏差（不隐藏，两项）

1. **预算行失效**：预注册 TSV 上限 20,000 行按预扫 8,373 估算，该预扫受 find `-maxdepth 8` 截断；全量深扫实际 providers-all 24,375 行、fnv-decode 39,794 行。**未截断、全部保留**；判据本身无行数条款，此为估算基数过时的登记失误，如实记录。
2. **mod 自带 BSA 只计总数**：磁盘存在 102 个 mod BSA，本轮未逐一展开内容。已证 14 个观测 FNV 全部由 loose winner 或原版 BSA 覆盖，无需 mod BSA 资源；工具已具备 BSA 列举/定点提取能力（LZ4 帧解码），未来对账未命中时可直接扩展。

### 对账要点（解释 K1-C4 NOT MATCHED 的具体来源）

- **源侧（v101b 自然游玩）全部来自 Smooth Moveset - OAR 与 Seamless Varied Feminine Idles OAR**：1HM_Equip `81FD1988…` = Smooth 1hm equip base（1.07 s）；1HM_Unequip `4D885DE5…` = Smooth equip base（1.47 s）；equip 稳定 MLh/1HM_Idle `22C9C4BF…` = Smooth non-combat idle start（3.33 s）；unequip MLh_Idle `13D0592A…` = Smooth non-combat idle loop2（22.9 s）；1HM_Idle `AE1527A6…` = Smooth combat idle loop（19.8 s）；MT_Idle 20.0 s `9F6236C9…` 与 18.0 s `95E20CF7…` = SVFI（svfi 1 base idle / svfi 3 right hand hip，female 路径但 clip 名走 `male\MT_Idle` 引用）。
- **候选侧来自另一组资源**：1HM_Equip `12A77CD1…` / 1HM_Unequip `4DED5EAF…` = Elden Rim/SCAR 同字节家族（1.43 s / 2.0 s，多路径同内容）；baseline MT_Idle `722C8A1E…` = Ashe conditional `i_no_mv`（11.67 s）；equip MT_Idle `F56EE150…` = Vanargand female（25.02 s）；MLh_Idle `DAD1F1C9…` = Diverse Magic Casting / Pretty Combat Spell Master（2.53 s）；1HM_Idle `C62C233B…` = **原版 BSA**（9.0 s）。
- 同一 FNV 可命中多个 winner 路径（同字节复制）：`data FNV` 是内容身份，不是文件唯一性；跨运行登记以 FNV+字节为准，文件归属记"多选一"。

### 工件（SHA-256）

| 工件 | 字节 | SHA-256 |
| --- | ---: | --- |
| providers-all.tsv | 3,567,290 | `c7a74461…dcd5cb` |
| winner-map.tsv | 8,734,740 | `c0285fa0…7298642` |
| oar-config.tsv | 487,477 | `839090d6…6d05538` |
| b1-behavior.tsv | 7,376 | `ea4da70d…6e6c6bf3` |
| b2-oar-winners.tsv | 3,683,759 | `496bfde5…794304e6` |
| b3-loose-winners.tsv | 3,551,547 | `bbc25bb9…71c89330` |
| fnv-decode.tsv | 4,593,225 | `acf53ceb…16783d` |
| fnv-reconcile.tsv | 44,256 | `9c7ec167…ff1d6bbe` |
| behavior-closure.txt | 35,372 | `0b7bb404…2db30198` |
| closure-files.txt | 3,396 | `58ef0174…7054c512` |

工具与脚本留在 gitignored `build/k1-c1/enum_k1c1.py` 与 `build/hkx-audit/HkxInventory/`（net10，HKX2-Enhanced-Library `91fa5b0`），不进仓库。

**裁定：K1-C1 = REGISTERED（限定范围）。** 本机、本 modlist、此时点的运行资源清单可复核且无丢失；不表示 OAR 已冻结、不证明跨运行资源未变（后续运行须重跑本清单并 diff）、不产生 `tau_*`、不关闭 K5/K6/G5。
