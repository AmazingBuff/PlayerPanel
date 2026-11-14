# C-G5-CAL K1-C1 完整运行资源清单（2026-11-14 预注册）

**状态（2026-11-14）：预注册，未执行。** 本轮闭合 [CAL v1 §三 K1](../../independent-behavior-graph.md) 的 "runtime/OAR/资源清单" 项：[K1 census](../g5-k1-census-20261114/README.md) §八指明 K1-C1 缺 **project/behavior 文件与全部 OAR 替换源的逐一登记**；同时落实其裁定（跨运行身份只按 `data FNV` 或完整路径+内容哈希，不用 leaf 名/binding-index）。**纯离线：零游戏运行、零插件代码改动。**

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
