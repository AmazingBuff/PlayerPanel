# 会话总结：2026-11-14（G5-CAL 控制 B 方案 A → K1 身份普查）

**范围**：本次会话从 13:08 `d8ed9fc`（K3 收官）之后开始，到 14:42 `ac30fb1`（K1-C4 NOT MATCHED）结束，共 **11 个提交**（`dce898e`…`ac30fb1`）。分支 `oar-idle3`，会话结束时领先 `origin/oar-idle3` **43 提交、未推送**，工作树 clean。本文是会话级索引，逐轮判据/工件哈希以各专项文档为准，不改写历史判定。

## 一、会话起点状态（承接 11-13）

- K3 两侧各自闭合：候选 K3-C v98（249 条）与源 K3-S v99b（1,151 条）双读一致、116 轨、零溢出；跨侧逐代配对（K3-P）不在 CAL v1 K3 范围，详见 [独立行为图验证](../independent-behavior-graph.md)。
- K4 B3/B4：v97d 单窗口 739/739 PASS（本会话边界）。
- **K2-C4 控制 B**：11-13 资格审计判 NOT_READY（运行时 `mt_idle` 被 Vanargand loose 覆盖 + OAR 引用、缺独立 spline 真值解码器与任意 HKX loader）。
- K1-C1–C4、K2-C1/C2、K5/K6、时间量化、姿态等价与 G5 均未闭合；`tau_p/tau_r/tau_s/epsilon_t` 未产生。
- 当前安装：v99b（`SamplingLiveness=on`，`IndependentGraph=off`）。

## 二、本会话完成的两大块

### 1. K2-C4 控制 B（方案 A）— READY（bounded）

目标：为“已知单层、固定时间的原生控制”建立独立真值与承载路径。

| 步骤 | 结果 |
| --- | --- |
| 资源更正 | 运行时实际加载 `meshes\actors\character\animations\female\mt_idle.hkx`（Vanargand female loose，113,920 B，SHA-256 `0EF8248C…BCD73C7`），**不是** 11-13 审计的 male BSA 文件（30,944 B）。 |
| 离线独立真值 | 本地 gitignored HavokLib（`ef5d5c6`，GPL-3，未进仓库/未链进插件）解码；修正时间映射为 `localTime/frameDuration`。产出 [mt_idle_female_truth.txt](g5-cal-control-b-20261114/mt_idle_female_truth.txt)（106,767 B，`CFC7835C…F2024125`，9 时间 × 99 轨，`BITS_FNV1A64 8E2E1D12B040807D`）。 |
| 原生承载 | 新键 `[Experiment] ControlSample`（默认 off，`IndependentGraph=inject`），按**动画字节身份**（data / blockOffsets / binding map FNV）选唯一 MT_Idle 载体，直接两次 `SamplePartialTracks`，不读 poseLocal、不写图。 |
| B1（v100，13:49） | 资源/读取/运行三项 PASS，`bits_fnv1a64=0FAB995615D7BFB2` vs `8E2E1D12B040807D` → **位精确 FAIL**；8910 分量中 5418 逐位一致（60.81%），最大差 1.7643e-05。归因 float32 实现顺序（非资源/时间/映射错误）。日志 [v100 run1](CharacterPanel-g5cal-control-b-v100-run1-20261114-1349.log)（345,735 B `DD3008CB…21D901`）。 |
| B2（v100a，13:59） | 预注册 authored bbox/量化步长 + float32 解析界（rotation 40-bit 步长 3.45436e-4；dynamic-16 `max(2·(max−min)/65535, 32·eps·(|min|+|max|))`；static/identity `32·eps·|value|`），**新进程** 8910 分量 **0 violation**、最大比值 0.4860 → **PASS**。B1 FAIL 保留，B2 是新批次不是放宽。日志 [v100a run2](CharacterPanel-g5cal-control-b-v100a-run2-20261114-1359.log)（343,700 B `655E9C52…1618B4`）。 |

**裁定：K2-C4 控制 B = READY（bounded）**，严格限 `female\mt_idle.hkx` / 9 固定时间 / 单层直接 `SamplePartialTracks` / 单进程。不产生 `tau_*`，不证明 CAL2 三进程/120 代协议，不关闭 K1/K5/K6/G5。详见 [control B 登记](g5-cal-control-b-20261114/README.md)。

### 2. K1 身份普查 — census 仪器 PASS，K1-C4 裁定 NOT MATCHED

目标：补齐 K1-C2 图身份、K1-C3 完整骨架/参考姿态、K1-C4 活动 clip 资源与实例身份。

- 实现：新键 `[Experiment] ClipIdentityCensus`（默认 off），新增 `src/animation/behavior_graph/k1_census.{h,cpp}` 与 `control_sample.{h,cpp}` 中的只读 `read_animation_resource_identity`。记录图身份、116 骨名/父索引/reference pose raw bits、每个活动 clip 的动画 header + spline data/blockOffsets FNV + binding map FNV + 进程内 `stable-id`。不采样姿态、不写图、不推进图、不新增 hook。
- 两种模式：候选（`IndependentGraph=inject`，源 pre-activate + 候选 post-activate/equip/unequip 首个 completed）、源自然游玩（`IndependentGraph=off` + `SourceTimeline=on` + F7，window-open 全量 + 阶段级 settle clip-only）。

| 运行 | 模式 | 结果 |
| --- | --- | --- |
| [v101](CharacterPanel-g5k1-census-v101-run1-20261114-1411.log)（209,572 B `AF9433E9…6455F379`） | 候选 | K1C-1–4 **PASS**：四段图身份齐全；两侧同一 116 骨 skeleton（`bones=parents=reference=116`、`recorded=116`、`invalid-parent=0`、`nonfinite=0`）；活动 clip `identity=true`/`unique==clips`/`missing=0`/`overflow=0`。发现源侧 MT_Idle 20.0 s 与候选 25.0167 s 不同、候选同一节点在 post→equip 之间被 OAR 换 binding。 |
| [v101a](CharacterPanel-g5k1-source-census-v101a-run1-20261114-1420.log)（89,988 B `75BC8C3B…E459C843`） | 源自然游玩 | **INVALID**：10 个逐字触发名在 equip 阶段耗尽 8 次 cap，`1HM_Unequip` 未覆盖。修复：触发收敛为 `IsEquipping`/`IsUnequipping`/`currentDefaultState`/`iState`，cap 8→24。 |
| [v101b](CharacterPanel-g5k1-source-census-v101b-run2-20261114-1424.log)（67,974 B `8B3AC063…E9E2F15`） | 源自然游玩 | S1–S4 **PASS**：window-open 全量 + 10 次 settle，`1HM_Equip`/`1HM_Unequip` 均覆盖、无 `census-cap`、零 `[E]`、20 s 正常 expired；28 条 clip 全 `identity=true`。 |

**源/候选资源对照（K1-C4 关键结论）**：两侧 **clip 家族对齐**（equip→`1HM_Equip`、unequip→`1HM_Unequip`，K2-C3 保持），但 `data FNV` 除 SCAR_1hmReadyDummy（`8728A54A…`）外 **无一对齐**：`1HM_Equip` 源 `81FD1988…` vs 候选 `12A77CD1…`、`1HM_Unequip` 源 `4D885DE5…` vs 候选 `4DED5EAF…`、MT_Idle 源 20.0/18.0 s vs 候选 11.6667/25.0167 s、1HM_Idle/MLh_Idle 也各不同。源侧 OAR 还在窗口内把 MLh_Idle/1HM_Idle 从 3.3333 s 短版切到 22.9/19.8 s 长版、MT_Idle 20.0→18.0 s，binding-index 同步漂移 → **leaf name 与 binding-index 都不能当跨阶段身份，只有 `data FNV` 可以**。

**裁定：K1-C4 资源清单已登记，但源/候选资源不一致（NOT MATCHED）。** 不能把两侧 `poseLocal` 当作同一动画资源的两次采样比较或反推容差；未来姿态对照须先冻结 OAR 配置 / 关闭动态替换，或按 `data FNV` 分组。详见 [K1 census 登记](g5-k1-census-20261114/README.md)。

## 三、构建与部署轨迹（本会话）

| 版本 | 身份后缀 | 用途 | DLL SHA-256（前 8） |
| --- | --- | --- | --- |
| v100 | `…3378889e91…` | 控制 B 方案 A 首轮（B1） | `409C2684` |
| v100a | `…9db4adfd5e…` | B1 结果 + 单次门控 + B2（PASS） | `12B39444` |
| v101 | `…9d0d2219cb…` | 候选侧 K1 census（PASS） | `C38C179C` |
| v101a | `…4f42e9615f…` | 源自然游玩 census（首轮 INVALID） | `07EB7633` |
| v101b | `…4d92f4ad8c…` | 触发收敛 + cap 24（源 PASS） | `87D1B417` |

部署备份（均在 `dist/`，哈希已核对）：`deploy-backup-20261114-v99b-v100pre/`、`…-v100a-v101pre/`、`…-v101-v101apre/`、`…-v101a-v101bpre/`；`g5-cal-control-b-v100a-20261114/` 含三件套 + MANUAL.md。

**会话结束时的 MO2 安装状态**（`E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\`）：

| 文件 | 字节 | SHA-256 |
| --- | ---: | --- |
| CharacterPanel.dll | 1,956,864 | `87D1B4178C4198772AF0C60E3937D7E35D41D01AF3C0F04A2694F884456E8D72` |
| CharacterPanel.pdb | 50,974,720 | `E3CB4DF02FCA612819DE974B297315DA4F47E6F63277A1808AF35A19AF6F17D1` |
| CharacterPanel.ini | 13,176 | `2964009C49EF55FF3BEFD1F648059DBC13BA97E3FA0769D8F6EC15D2A91A854E` |

即 **v101b 源侧自然游玩档**（`IndependentGraph=off`、`SourceTimeline=on`、`SourceTimelineGraph=DefaultFemale`、`ClipIdentityCensus=true`、其余 off）。这是最后一次上机用的档，不是默认产品档；`config/CharacterPanel.ini` 仍保持全键默认 off。

## 四、提交清单（11 个，按时间）

| 提交 | 内容 |
| --- | --- |
| `d8ed9fc` | K3 收官 + K3-P 范围更正（会话前基线） |
| `dce898e` | 控制 B 资格登记 NOT_READY |
| `3378889` | 方案 A：控制 B 载体 + 离线真值 + `ControlSample` 键 + policy 测试 |
| `9db4adf` | B1 位精确 FAIL + B2 预注册 + 单次门控 |
| `7ba9206` | B2 bounded PASS + K2-C4 READY（build/CHANGELOG/history 同步） |
| `9d0d221` | K1 census 实现（graph/skeleton/clip 身份） |
| `3ebcd2e` | v101 结果 + 资源发现 + node/index 标签修复 |
| `4f42e96` | 源侧 window-open/settle census 实现 |
| `53a8df5` | 源侧自然游玩普查预注册 |
| `4d92f4a` | 触发收敛 + cap 24 + v101a 归档 |
| `ac30fb1` | v101b PASS + K1-C4 NOT MATCHED 裁定 + 文档同步 |

## 五、当前状态与明确边界

| 前置 | 状态 |
| --- | --- |
| K1-C1 运行/资源完整清单 | **NOT_READY**（project/behavior 文件与全部 OAR 替换源未逐一登记） |
| K1-C2 图身份 / K1-C3 骨架映射 | census 已登记（116 骨、reference pose、无丢失）；跨运行身份未证 |
| K1-C4 资源与 clip 实例 | 清单已登记，裁定 **NOT MATCHED** |
| K2-C1/C2 装备初态/输入账本 | 部分（v96a 有 1H+左手 9 场景证据，未成完整清单） |
| K2-C3 片段一致性 | PASS（冻结 1H+左手 9 场景） |
| K2-C4 独立控制 | **READY（bounded）**（B2 v100a） |
| K3 同代快照 / K4 后处理边界 | 两侧各自 PASS（单会话） |
| K5/K6 完整计数、层/权重/混合、寿命/丢失、仪器/数据计划 | **未闭合** |
| 时间量化、`tau_*`、姿态等价、G5、G6 | **未产生 / 未通过 / 后置**，装载 teardown NO-GO |

**下一步（按 CAL v1 顺序）**：① K5/K6 完整计数/层/权重/混合/寿命；② K1-C1 完整运行资源清单与 K2-C1/C2 账本；③ 按 `data FNV` 冻结 OAR 配置或关闭动态替换后再设计姿态对照；④ CAL2 三进程/120 代驱动协议另行预注册。改资源/时钟/判据需升契约版本并同步入口，不回写旧运行。

**不产生** `tau_p/tau_r/tau_s/epsilon_t`；不把旧日志当校准样本；不把 60.81% 逐位一致或 0 violation 说成 G5 通过；不从源候选差反算容差；不自动 push。
