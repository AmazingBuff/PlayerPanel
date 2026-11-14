# 会话总结：2026-11-14 第二会话（K1-C1 完整运行资源清单 → K2-C1/C2 装备初态与输入账本）

**范围**：本次会话从 [第一会话总结](session-summary-20261114.md)（结束于 `62f6a73`，v101b 安装、领先 43 提交）之后开始，到 `47f8139` 结束，共 **6 个提交**。分支 `oar-idle3`，结束时领先 `origin/oar-idle3` **50 提交、未推送**，工作树 clean。两个工作包全部按预注册 → 执行 → 裁定的流程关闭：**K1-C1 = REGISTERED**、**K2-C1 = REGISTERED**、**K2-C2 = REGISTERED**。本文是会话级索引；逐项判据/工件哈希以各专项文档为准。

## 一、工作包 1：K1-C1 完整运行资源清单（REGISTERED）

文档：[g5-k1-c1-20261114/README.md](g5-k1-c1-20261114/README.md)（预注册 + 执行结果 + 两条登记偏差）。提交 `496e68d`（预注册）→ `fb067be`（登记）。

- **行为/工程文件**：从生效 `0_Master.hkx` 走链（`hkbBehaviorReferenceGenerator`）取得闭包 **23 文件**（Pandora_Output 16 含 0_master + 松散 7），38 条引用边零未解析/歧义；与 A1 旧转储**逐文件哈希对照 23/23 逐位一致**（行为链自 10-31 零变化；A1 文本"17+6"为其枚举笔误，成员集即 16+7）。
- **OAR 替换源全量登记**：启用 mod 第三人称角色路径 OAR 树（150 个 OAR 模块 / 1,322 子模块 config / 1,472 行，解析失败 1 条）+ 非 OAR loose 动画覆盖；`providers-all.tsv` 24,375 行 / `winner-map.tsv` 24,204 生效路径（含 SHA-256）。
- **全量解码**：19,973 文件 / 19,821 动画 / 19,062 spline；150 错误全部逐条列明（146 = mnbv 系 XML 假 .hkx、4 = 截断文件）；**锚点 2/2**（SCAR dummy、Vanargand mt_idle，blockOffsets FNV 亦与控制 B 常量一致）。
- **观测对账 14/14**：v101/v101b 全部观测 `data FNV` 落位——源侧 = Smooth Moveset - OAR + Seamless Varied Feminine Idles；候选侧 = SCAR/Elden Rim 同字节家族、Ashe conditional、Vanargand female、原版 BSA 1hm_idle。**K1-C4 NOT MATCHED 的资源来源已具体到文件。**
- **登记偏差两条**：① TSV 预算行失效（预扫 `find -maxdepth 8` 少算深层目录，"8,373"低估；未截断、全保留）；② 102 个 mod 自带 BSA 只计总数未逐一展开（已证观测 FNV 无一需要 mod BSA；工具已具备 BSA 定点提取能力）。

## 二、工作包 2：K2-C1/C2 装备初态与输入账本（REGISTERED）

文档：[g5-k2c1c2-20261114/README.md](g5-k2c1c2-20261114/README.md)。提交 `83a6ff3`（预注册 + 离线账本）→ `bd630da`（仪器）→ `57727b3`（构建/部署登记）→ `47f8139`（运行记录 + 关闭）。

- **C2 输入账本（离线关闭）**：候选注入序列 10 步逐行挂 v96a 行号（同帧 5 写 readback 1/1/9/1/9 → `WeapEquip` → equip 120 步 → settle 87/101 → `Unequip` → completed=129 state-12-to-0）；源侧参考序列两次带相对时刻（D3v2：事件 t=0 → +0.500s 变量写入 → +1.056s iState→9 → +7.803s 卸下 → +9.054s 12→0；v101b 复现）；外部输入与图内输出分栏。
- **C1 装备初态（仪器上机后关闭）**：新键 `[Experiment] EquipmentStateDump`（默认 off，`ANIMATION WORLD` 行尾追加 `armor=slots=31 worn=N <slot>:<formid>`，`Actor::GetWornArmor` 逐掩码槽枚举；off 时行与历史逐字节一致）。**护甲 9 件全解析**：
  - 1+12 = Dawnguard.esm `DLC1ArmorDawnguardHelmetLight`（BOD2=0x1002 = hair+circlet，双槽自洽）
  - 2/7 = **CC Blades Armor** Cuirass/Boots（ccbgssse055，local 0x801/0x800）
  - 3 = DIS_RealisticArmor `DIS_SteelScaledGauntlets`；5 = USSEP `MG05RewardAmulet`
  - 6 = **CC Daedric Mail Ring**（ccbgssse051，local 0x823）
  - 17 = **CC Survival Mage Backpack（ccfsvsse001，local 0x80b）——"背包"即此已装备 CC 背包**
  - 20 = 3BBB `SMP3BONObjectP50`（3BA 身体填充）；火把未装备（左手 = Candlelight）
  - 右手 = `DBM_CC_WeaponCrimsonTideSword`（DBM_CC_HallOfWonders.esp，收鞘）；左手 = `Candlelight`（Skyrim.esm `0x00043324`，即 `iLeftHandType=9` 的语义）
- **运行存档身份（K2C1-3）**：`Save116_61F78427_0_456C6966_Folgunthur01_011659_20261112151836_26_1.ess`（角色 Elif，#116，Folgunthur01，playtime 040.59.48）——5 处独立 FormID 交叉解析全部经该存档表命中。
- **登记偏差一条**：K2C1-2 的 OAR 命中子项 FAIL——本次 panel-open 源侧栈 = SVFI `MT_Idle`，v96a = Smooth Moveset 1HM 栈（源侧 idle 栈随世界状态变化，K1-C4 同类现象）；hands/drawn/armor/相位冻结项不受影响，按 FAIL 登记未放宽。

## 三、方法学新事实（下会话可复用）

1. **MO2 modlist.txt 先者赢**（顶部=高优先级）：以 A1 运行时证据（S3 load-try 链 = Pandora_Output @行26 胜 BFCO @行754）校准；静态重建冲突方向前先拿一个已知运行时事实对表。
2. **运行时 FormID 表 = 存档表**：AE v12（version=12）存档头的插件表是 **LZ4 压缩块**（Save116 起始 @245879，解压 3,998,360 B：常规 133 项 + 光表 766 项，u16 计数 + u16 长度名）。静态 plugins.txt 推算会漂移（ESL/常规分类与文件副本解析是坑）；**跨运行 FormID 解析一律走存档表**。解压产物与两表已留 `build/k1-c1/save_header_plugintable.bin`、`save_regular_plugins.txt`、`save_light_plugins.txt`（gitignored）。
3. **ESL FormID 映射**：`0xFE + (表序<<12) + local12`，表序 = 存档光表 0-based；双锚点验证法（语义锚：`fe0bb800` → SCAR Exhaustion State/Check/Dispell；内容锚：CC Blades local 0x800/0x801）。
4. **工具资产**（全部 gitignored，本地）：`build/hkx-audit/HkxInventory/`（decode/closure/bsa-list/bsa-extract 四模式，含 SSE BSA v105 解析 + LZ4 帧解码 + 原版 Animations.bsa 定点提取）；`build/k1-c1/enum_k1c1.py`（winner map/OAR config 枚举）；ESM/ESP/ESS 解析脚本均在会话内联，未持久化。
5. 游戏侧已证事实：v96a 场景在本轮**原样复现**（equip 120/120 + settle 87/101 + unequip 129 state-12-to-0、clip 族 1HM、零 H2H、零 `[E]`）——冻结场景的可复现性得到第二次独立确认。

## 四、构建与部署轨迹

| 版本 | 身份 | 用途 | DLL SHA-256（前 8） |
| --- | --- | --- | --- |
| v102 | `…-v102-g5k2c1-equipdump-bd630daacc-cl94faaed0c6-20261114T082611Z` | K2-C1 装备全态仪器（EquipmentStateDump） | `32ff383e` |

- 首个构建（+dirty 身份 `9d55bf63…`）未部署；提交 `bd630da` 后干净重建部署。
- 运行档 = v96a 候选档派生 + `EquipmentStateDump=true`（逐键断言通过），`dist/g5-k2c1c2-20261114/CharacterPanel.ini` 11,767 B `0e789f8a…57cf35`。
- 部署备份：`dist/deploy-backup-20261114-v101b-v102pre/`（v101b 三件套）。

**会话结束时的 MO2 安装状态**（`E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\`）：

| 文件 | 字节 | SHA-256 |
| --- | ---: | --- |
| CharacterPanel.dll | 1,969,152 | `32ff383e4edffd2de43e380eb3ffc75366f02b3944220b961b7ffa13aec4185c` |
| CharacterPanel.pdb | 50,991,104 | `c3a7ed4579bb20ccfa8709e5e182386715471f3f22db71d39774bc92d6ba85b1` |
| CharacterPanel.ini | 11,767 | `0e789f8a0673bac822173e2353f90bca1b0ba32905dbfde3fcbf50474457cf35` |

即 **v102 诊断档**（`IndependentGraph=inject`、prime/unequip-probe on、`EquipmentStateDump=true`），**不是默认产品档**；`config/CharacterPanel.ini` 保持全键默认 off。下会话如需产品档或其它实验档，按需派生并重部署。

## 五、提交清单（6 个，按时间）

| 提交 | 内容 |
| --- | --- |
| `496e68d` | docs(g5-k1c1): 预注册完整运行资源清单 |
| `fb067be` | docs(g5-k1c1): 登记 REGISTERED（11 个 TSV/文本工件） |
| `83a6ff3` | docs(g5-k2c1c2): 预注册装备初态/输入账本 + 离线登记 |
| `bd630da` | feat(g5-k2c1): EquipmentStateDump 仪器（settings×2/controller/INI/README 修订） |
| `57727b3` | docs(build): v102 构建与部署登记 |
| `47f8139` | docs(g5-k2c1c2): v102 run 1 记录 + K2-C1 关闭 |

## 六、当前账本状态与明确边界

| 前置 | 状态 |
| --- | --- |
| K1-C1 完整运行资源清单 | **REGISTERED**（限定范围：mod BSA 只计总数；跨运行重跑清单须 diff） |
| K1-C2/C3 图身份/骨架 | census 登记（v101/v101b）；跨运行身份未证 |
| K1-C4 资源与 clip 实例 | 清单登记，**NOT MATCHED**（来源已具体到文件：源=Smooth/SVFI，候选=SCAR/Elden Rim/Ashe/Vanargand/原版） |
| K2-C1/C2 | **REGISTERED**（本轮） |
| K2-C3 / K2-C4 | PASS（冻结 1H+左手法术场景）/ READY（bounded，B2 v100a） |
| K3 同代快照 | 两侧各自闭合（v98 + v99b，单会话）；K3-P 跨侧配对另批 |
| K4 后处理边界 | PASS（v97d 739/739，单会话） |
| K5 完整计数/层/权重/混合 | **未闭合** |
| K6 实施与数据计划 | **未闭合**（须先有经评审的仪器范围/数据计划） |
| 时间量化、`tau_*`、姿态等价、G5 | 未产生 / 未通过 / 后置 |
| G6 | 后置；装载 teardown NO-GO |

## 七、下一步（下会话入口）

1. **先读本文 + [CAL v1 契约](../independent-behavior-graph.md#c-g5-cal-v1)**；K1/K2/K3/K4 与 K2-C4 的既有证据不得外推。
2. **K6 仪器与数据计划（下一个工作包）**：先写经评审的计划稿——须一并裁定 **"按 `data FNV` 冻结 OAR 配置 or 关闭动态替换 or 分组比较"**（K1-C4 NOT MATCHED 的直接后果，决定 K5 采样与姿态对照设计）；计划评审通过后才实现 K5 仪器。
3. K5 实现与上机（完整计数/层/权重/混合/寿命对账，预算按 CAL v1 §五）。
4. CAL2 三进程/120 代驱动协议另行预注册；`tau_*` 在全部前置关闭前不产生。
5. 可选清理：44→50 提交是否 push 由用户决定；`build/hkx-audit/`、`build/k1-c1/` 工具遗产收编与否由用户决定；全局 NuGet.Config 失效源建议用户自清（历史遗留）。

**不产生** `tau_p/tau_r/tau_s/epsilon_t`；不把 K1/K2 的 REGISTERED 说成 K1/K2 整体 READY（K1-C2/C3/C4 跨运行身份与 K5/K6 仍开放）；不自动 push。
