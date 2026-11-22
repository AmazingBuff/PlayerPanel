# C-G5-VER 正式验证：两次会话与冻结规则判定（2026-11-22）

> **状态**：正式验证运行完成（候选会话 13:39 + 源侧会话 13:47，冻结工具 `gate5_ver.py` 全模式判定）。**机械判定：等值主张集 FAIL（tau_p=117 / tau_r=1,278 / tau_s=0 / invalid=0）**——与冻结时的预期形态一致（tau_r 位精确级判据 vs 时钟阶梯残差），每笔超差归因到预注册机制，无新增机制、无 EQ-C5 停点触发。裁定语带 limited 资格（§三.4）。G4P-7 历史裁定与工件不改写。

## 一、会话与工件

| 项 | 候选会话（13:39） | 源侧会话（13:47） |
| --- | --- | --- |
| 构建身份 | `…-v122-g5ver-candidate-20261122-6cc96b6418-cl94faaed0c6-20261122T051227Z`（两会话同 DLL） | 同左 |
| 键面 | inject + 注入五键 + K5FullSample + IdleChainProbe（17 键断言） | off + SamplingLiveness=on + K5FullSample + K5Reconciliation（12 键断言） |
| 捕获 | 背包内 F7 重捕获 ×1（view1/2 快照）+ 注入 WeapEquip ×1 | 背包关 F7 → 20 s 源侧窗（frames=1145，expired 正常关窗） |
| K5 批次 | **249/249 recorded**，零丢失/溢出/双读坏 | **1145/1145 recorded**，零丢失；`identity_failed_clips=1, drift_records=1`（与 gate-5 源批同形） |
| 段完成 | equip completed（预算收尾，last-state=1）；unequip completed（**state-12-to-0 @129**，last-state=0） | 实机序列：armed idle → 真收刀（seq 133）→ mt_idle 205 帧 → 真掏刀（seq 462）→ armed idle 594 帧 |
| 漂移 | `source-drift=none` 两段 | —— |
| [E] | 仅 OAR-not-loaded 一条（Track 2 正常） | 同左 |
| 工件 | [raw/candidate](raw/candidate/)（log `c64ae456…`、bin `500a0b23…`、ledger `75b50502…`） | [raw/source](raw/source/)（log、bin、ledger 待列哈希见 git） |

两次会话为**独立会话**（O-2 决断结构）；候选 bin 家族序列与 gate-5 正式批次逐段同构（1hm_equip → equip+idle 双存 → 1hm_idle+mlh+scar settle → unequip 注入双存 → 1hm_unequip → unequip+mt）。

## 二、分账结果（冻结规则 §三，[verdict](verdict/gate5-ver-summary.txt)）

| 层 | 数 |
| --- | ---: |
| 候选记录 → burst | 249 → **41**（equip 17 / idle 3 / unequip 21，与 O-1/dry-run 同构） |
| 排除：clip 末段饱和 | 3 bursts（端点 c85/c98/c128） |
| 排除：EQ-W1b | 9 bursts（burst 级规则） |
| 端点 → 配对 | 29 → **29 paired**（0 no-partner、**0 edge**、0 dt-excluded） |
| 排除：EQ-W1a | 2（源侧双主族边界对） |
| **等值主张集** | **27 对 × 272 分量** |
| **机械判定** | **tau_p=117 / tau_r=1,278 / tau_s=0 / invalid_components=0** |
| dt 门 | 0 排除（median 3.8 ms / max 9.2 ms ≤ ε_t=0.0333960） |
| 账本闭合 | closure OK |

**判定（机械）：FAIL**——等值主张集存在超差，正如冻结时预期（tau_r ≈0.04° 位精确级 vs 阶梯残差 ≈0.5–1.2°，20–30×）。这是"每笔超差有名字"的最终账本，不是未经归因的失败。

## 三、DECL 集对账（全部在基准量级内，不进 FAIL）

| 轨 | 本会话 max | C-G5-EQ v1 基准 | 判读 |
| --- | ---: | --- | --- |
| t31（R UpperArm）rot | 30.08° | 30.3 / 30.32 | ✓ |
| t58（RUt1）rot | 17.30° | 17.3 / 17.0 | ✓ |
| t30（RClv）rot | 10.18° | 10.2 / 10.2 | ✓ |
| t59（RUt2）rot | 8.60° | 8.5 / 8.5 | ✓ |
| t45（R Pauldron）rot | 6.37° | 6.4 / 6.6 | ✓ |
| t60–66（武器槽）pos | 3.5–5.6 | Quiver 4.1958 量级 | ✓ |

**WeaponAdjustment 与挂点定位两层在本次会话中以同等量级在场**——声明集的机制前提（EQ-C2 方向）成立。

## 四、残差集（无新机制）

等值集超差 top（27 对内）：t4（COM）p24/r24、t44（L Pauldron）p~22/r22、t43（Weapon）p19/r21、t25/26（Spine1/2）r26–27、t35/36（Neck/Head）r27、t9/t28/t39/t6/t10 r24–25——**全部为 [重判 §四/§五](../g5-transition-window-rejudge-20261121/README.md) 与 [落名分析](../g5-bone-name-mapping-20261121/README.md) 已登记的残差轨**；形状不变（平移集中少数轨 + 旋转弥散阶梯）。**无新恒差 → EQ-C5 不触发停点。**

## 五、EQ-C 适用性复测

| 判据 | 结果 | 证据 |
| --- | --- | --- |
| EQ-C2 机制在场 | **成立** | 源侧 1HM_Idle 行 max-drot **63.15°**（= 已知 t38 MLh 混合 62.9° + C′ 家族量级），全部快照一致 |
| EQ-C3 候选忠实 | **成立** | 候选 MT_Idle 行 max-drot ≤3.11°（候选 poseLocal ≈ 自身 clip 采样，无源式修改层） |
| EQ-C1a/C4 家族/节点 | **成立（已知差）** | census：候选 97 节点 vs 源 114 节点，差 17 = 已知 WeaponAdjustment+MLh 分支克隆缺口（EV-4 同形） |
| EQ-C5 无新恒差 | **成立** | §四 |
| EQ-C1 状态同域 | **未达成（快照级）** | 快照态错配：候选全部为卸下后徒手（MT_Idle），源侧武装（1HM_Idle）。机制：**注入序列在单墙帧内 15 ms 全部完成**（equip 段 32.370→32.377、unequip 段 32.377→32.382），call 60/120 快照必落在序列之后；这是突发驱动的结构性后果，非数据缺陷 |

**影响与处置**：快照级状态配对的三方对照（EQ-C1/C1a 的逐状态对）在本会话形态下不可得；正式比较不受影响——`gate5_ver` 的 nearest-clock 相位配对跨域成立（dt ≤9.2 ms），C2/C3 的机制证据取自两侧各自的行 + census。EQ-C1 的成立留待后续会话形态修正（如需，另立小型探针），不阻塞本次判定记录。

## 六、新结构观察：单帧完成

本次 249 驱动步在**单墙帧 ~15 ms** 内完成（gate-5 形态为 ~17 帧 × 11–15 步突发）。尽管如此，时钟阶梯仍分为 41 个 burst 且端点配对 dt ≤9.2 ms——**O-1 的经验规则（末步相位对齐，以 dt_clock ≤ ε_t 为操作保证）在单帧形态下依然成立**；时钟锁存的引擎机制维持开放（O-1 已记）。此形态说明 burst 结构由驱动批次而非墙帧时长决定——对后续会话设计是重要输入。

## 七、裁定（limited）

> **C-G5-VER v1 正式验证判定（2026-11-22，限定范围）**：在两次独立会话工件上，按冻结规则执行 burst 末步配对与排除窗处理后，**等值主张集机械 FAIL**（tau_p=117 / tau_r=1,278 / tau_s=0；27 对 × 272 分量），**全部超差归因到预注册机制**——声明差异集（WeaponAdjustment 五轨 rot、挂点七轨 pos）实测与基准咬合、不计 FAIL；残差集（时钟阶梯、手指微差、t4/t44/t43 平移等）无新增机制。scale 轴在等值集上干净（tau_s=0）。**本裁定不证明整体姿态等价**；未闭合项清单：完整 CAL1/门 3、K5-5 层/混合语义、K2-C1 完整清单、A17 独立控制、EQ-C1 快照级状态同域、G6 全部、装载 teardown（NO-GO 不变）。

**下游决策输入（产品层，非本协议范围）**：
1. 残差量级：动画骨旋转阶梯 ≈0.5–1.2°（max 1.2°）、平移残差（t4/t44/t43）为已知波动形。**若产品判定"肖像预览可接受"，须另立容差契约**（tau_r' ≈ 1–2° 级）方可出 PASS 形裁定——这是最后一次判定形态转换的路径。
2. 若不接受残差：时钟阶梯为端点相位量化固有（O-1 已界定），修复路径 = 驱动/时钟语义改造（行为契约，(c) 路线）。
3. G6（生命周期/源恢复/重复会话）仍未验——预览的长期稳定性主张在 G6 之前不成立。

## 八、边界

- 单运行时 AE 1.6.1170、`AE-Track2`、DefaultFemale、单存档族、单会话对；换骨架/装备族须重测。
- EQ-C1 未达成按"未达成"记录，不升格为失败也不豁免；其补测路径已给出。
- `gate5_ver` 的 DECL/残差计数为机械输出；本裁定语由本运行记录按 §三.4 书写。
- G4P-7、重判、C-G5-TRANS §八、C-G5-EQ/C-G5-CHAIN 的既有裁定与工件不改写。
- 本会话的源侧窗内 `ring-dropped=44`（E1 式 detail 环，容量 2048）——K5S 批次本身 1145/1145 完整（账本 dropped=0）；detail 环为辅助通道，不影响比较输入。
