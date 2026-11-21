# C-G5-CHAIN v1：idle R 肩链修改层归因探针（预注册，2026-11-21）

> 对应 `C-G5-TRANS` [§八.5](../../g5-transition-window.md) 与 [底噪溯源 README §六](../g5-bone-name-mapping-20261121/README.md)。**本文在实现前落盘，判据不看运行结果。**

## 一、问题与假设

底噪溯源（纯离线）已裁定：settle（idle 22 对）下两侧 pose 的差异不是浮点求值噪声，而是三成分；其中最大成分是 **R 肩链（t30/t31/t58/t59/t45）的恒定旋转差**（t31 30.3°、t58 17.3°、t30 10.2°、t59 8.5°、t45 6.4°，Δq 轴稳定）。同剪辑（data/map FNV 逐位同）、同参考位姿（两骨架 referencePose 逐位同）、同轨映射（map FNV 同）；子链 local 匹配（前臂 0.11°、手 0.77°、武器 0°）但世界系合成后手/武器同差 27.5°（非 IK"保持末端"形状）。**待归因：源侧存在一个候选直驱没有的位姿修改层。**

预注册假设（探针运行前固定）：

| 假设 | 内容 | 判别式 |
| --- | --- | --- |
| **H-AUTH-SRC** | 源侧（live 全管线）在图后/图内对 R 肩链施加修改，候选直驱＝剪辑原值 | 候选 pose ≈ 剪辑采样（控制轨），源 pose 与剪辑采样差 ≥5°（t31/t58/t59/t30/t45） |
| **H-AUTH-CAND** | 候选直驱偏离剪辑原值（直驱缺陷），源侧＝原值 | 反向成立 |
| **H-BOTH-DEV** | 两侧都偏离剪辑采样（如两侧都有不同的修改层） | 两侧都不匹配控制轨 |
| **H-GRAPH**（层定位） | 修改层是图内节点（blender / IK modifier / OAR 注入行为） | 源图 activeNodes 与候选图 activeNodes 的差集中出现非 clip 节点 |
| **H-ENGINE**（层定位） | 修改层在图外（引擎后处理，如握持/瞄准修正） | 两侧 active 集一致（除 clip 状态）且源 pose ≠ 剪辑采样 |

**剪辑采样对照基准**：`hkaAnimation::SamplePartialTracks(control->localTime, tracks, …)` ——与 v120 扫描探针同一条已验证管线（clip→control（`Clip_Control_Offset`）→ `VTABLE_hkaDefaultAnimationControl` 校验→binding→animation）。每个 clip 的对照只在该侧自身的 `localTime` 上做（候选时钟阶梯差 ≤25ms 不进入该对照）。

## 二、探针设计（只读、log-only、无新 detour）

- **挂点**：`DisplaySession::step_display_pose` 的 `call==60` 与 `call==120` 两个既有检查点（每次 F7 显示窗 2 个快照，天然有界；不新增线程/钩子）。
- **INI 键**：`[Experiment] IdleChainProbe=true`（默认 false）。门：`IdleChainProbe` 且 AE 1.6.1170。显示路径自身已要求 `IndependentGraph=inject` + AE 1.6.1170。
- **每快照输出**（对 candidate 与 source 两图对称执行）：
  1. `IDLE CHAIN <side> …`：该图每个 active clip（vtable=`VTABLE_hkbClipGenerator`，上限 8）——clip 名、data-fnv/map-fnv/local-time（`{:08X}` 位型）、`SamplePartialTracks` 后与该图 `poseLocal` 在**兴趣轨**上的逐轨 `drot`（度）与 `dpos`；汇总 `diff-count`/`max-drot`。
  2. `IDLE CHAIN CENSUS <side> …`：该图 activeNodes 全量按 vtable 分类（复用 `classify`），节点名/id，上限 128 行 + SUMMARY（walked/unknown）。
- **兴趣轨**（按骨名在运行时从 `setup->animationSkeleton->bones[i].name` 解析索引，非硬编码骨号）：`NPC R Clavicle [RClv]`、`NPC R UpperArm [RUar]`、`NPC R UpperarmTwist1 [RUt1]`、`NPC R UpperarmTwist2 [RUt2]`、`NPC R Pauldron`、`NPC R Forearm [RLar]`、`NPC R ForearmTwist1 [RLt1]`、`NPC R ForearmTwist2 [RLt2]`、`NPC R Hand [RHnd]`、`Weapon`、`NPC L Hand [LHnd]`、`Quiver`、**控制轨** `NPC L Thigh [LThg]`、`NPC Spine1 [Spn1]`（后两条预期呈时钟阶梯差形状，用于校准对照灵敏度）。
- **侧间同一性核对**：两侧同族 clip 的 `data-fnv/map-fnv` 逐位对照进日志（离线判定，不改运行行为）。

## 三、判据（预注册）

- **V-P1（H-AUTH-SRC 成立）**：候选侧控制轨（`NPC L Thigh`、`NPC Spine1` 之外的全部非肩链轨，含 `NPC R Forearm/Hand/Weapon`）对自身剪辑采样 `drot < 0.01°`，且源侧 t31/t58/t59/t30/t45 至少 4 轨 `drot ≥ 5°`；同时源侧控制轨 `NPC R Hand/Weapon` 允许小差（世界系已证手 local 差 0.77°）。
- **V-P2（H-AUTH-CAND 成立）**：与 V-P1 反向（候选侧肩链轨 ≥5° 且源侧全匹配）。
- **V-P3（H-BOTH-DEV）**：两侧肩链轨都 ≥5°。
- **V-N1（H-GRAPH）**：CENSUS 两侧行差集（源有候选无）含非 clip 节点 → 按类名/节点名记录候选修改层；离线与 K1 普查/OAR 知识对照命名。
- **V-N2（H-ENGINE）**：两侧 CENSUS 无实质差集且源侧肩链仍 ≥5° → 记录"图外引擎层候选"。
- **不自动判定**：运行时只产出机械计数与快照行；P1/P2/P3 的裁定由离线判读，零计数/匹配结论只限已证明可读的观察范围。

## 四、停点与负路径

- 任一侧 `poseLocal`/`activeNodes`/clip 链不可读 → 该侧 `status=unreadable-…` 行，不判通过；两次快照全 unreadable 则本轮作废回评审。
- 两侧同族 clip 的 data-fnv 不一致 → 不中止（照实记录），但判读时**不得**用 V-P1/P2 归因（资源不同一）。
- 非有限值（pose 或采样）→ 该轨记 `nan` 并计入 status，不中断其余轨。
- 快照期源图 generation 变化（读档/换装）→ 该快照作废（CENSUS SUMMARY 记 `source-drift`）。

## 五、上机协议

1. 延续 gate-5 环境：`AE-Track2`、Save116、同一装备族；DLL 为 v121（构建身份在本文追加节记录，与 PDB/部署 INI 同轮互核哈希——v115 教训）。
2. 进游戏→装备→收刀→**静止站立（idle）≥3 秒**→F7 开显示窗（第一 view）→关→再开（第二 view）。共 ≥2 个 view（≥4 快照）。
3. 不要求注入序列与 gate-5 逐位一致（本探针只取 idle settle 快照）；不装其它 DLL；一次会话一个 DLL。
4. 运行后回传：完整日志、构建身份、部署 INI 哈希；离线判读按 §三。

## 六、边界

- 单运行时 AE 1.6.1170；单存档；不含源侧过渡（§八.3/6 另立）。
- 探针不改门 5 裁定、不改 `tau_*`；采样对照不改任何运行状态（只读 API）。
- `SamplePartialTracks` 的对照基准是"该侧 binding 的 authored 轨数据"；若 OAR 在装载期改写了共享模板树上的 clip 数据（652/8223 残余），则两侧采样的是改写后数据——该可能性由 data-fnv 对照与 K1 普查事实约束，不在此断言。

## 八、run 1 判读（2026-11-21 13:01，日志 [CharacterPanel-g5chain-idle-run1-20261121-1301.log](../CharacterPanel-g5chain-idle-run1-20261121-1301.log) 281,816 字节 `3718aad7…`）

**对预注册判据：V-P1/V-P2/V-P3 全部 NOT REACHED（不判定）**——快照状态与判据前提失配：协议的「装备→收刀→静止」取到的是**徒手 idle**（源图已切 MT 分支），而预注册三假设隐含两侧处于 gate-5 idle domain 的**持武器 1hm_idle 状态**。失配本身不是预注册停点，但前提不满足即不得套用判据；以下按实际证据记录，不改写预期。

**状态失配的直接证据**（4 组快照 × 两视图一致）：
- 源图 active clip 仅 `MT_Idle.HKX`（data-fnv `F56EE1500EB5A3D0`，与 Track2 注册表逐位同），census 独有 **MT 分支 26 节点**（MT_BehaviorGraph、IdleBFR/MT_BehaviorBFR、ArmsBlend/BodyWeightBlend blender、Carry 左右臂行为等）。
- 候选图 active clip 3 个：`H2H_Idle` + `1HM_Idle`（`C62C233B06FAE9A9`）+ `SCAR_1HM_Ready_Dummy0`（`8728A54A50D85C22`），census 独有 **1HM 分支 31 节点**（1HM_Behavior、1HM_Readied_BehaviorGraph、1HM_1stPUpperBody、1stP 系 modifier 等）。候选 pose ≈ H2H_Idle 采样（t31 0.40°–4.68° 变动残差），与 1HM_Idle 采样差 60–70°。
- 面板打开期间源图 clip 时钟冻结（view 内 call 60/120 的 `local-time-bits` 逐位同：view1 `41284B17`=10.636 s、view2 `4159082C`=13.565 s），view 之间推进——F7 检查点的源侧对照是冻结帧等式。

**有效新证据（有界，不改门 5 裁定）**：
1. **B 成分方向裁定（回答 §八.4 机制方向）**：源图 poseLocal 对**它自己的** MT_Idle 采样：身体/头/脊/双臂/Weapon 全部逐位一致（drot ≤3e-6°），**唯 t60（Quiver）dpos=4.1958 且 drot=0**——恒定槽位平移差存在于**源侧相对 authored 剪辑数据**，即 live 引擎在图后重定位挂点轨；候选按剪辑采样输出，与源差恰为该常量 → gate-5 B 成分（t60–66 ≈4.21–4.22）= **引擎挂点定位层**，不是候选缺陷。量级一致（4.1958 vs 4.21–4.22）但非逐位同值（不同剪辑/状态）。
2. **源侧腿部修改层线索**：同一源图对照里 t6（L Thigh）drot=2.19°（其余身体轨逐位同）——MT idle 下腿部对剪辑采样的独有偏差，foot-IK 类引擎层候选，未定名，只限该状态。
3. **克隆收敛缺口（G6 范围新证据）**：装备→收刀→徒手站立后，候选克隆仍保留整个 1HM 分支 active（31 节点）且 pose 由 H2H_Idle 驱动，不随世界切换到 MT 分支——「源恢复/重复会话收敛」未成立的直接运行时证据。
4. **仪器局限**：`IdleChainSideSummary.observe` 逐 clip 覆写同一槽——SUMMARY 行只反映最后一个被比较 clip；逐 clip 行完整且为判读权威。修复推迟（行数据足够，避免本轮改身份）。

**run 2 修正协议**（对齐 gate-5 idle domain 前提，部署/身份不变）：**装备（持武器）→ 原地站立 idle ≥3 秒（不收刀）→ F7 ×2 视图**。两侧应同处 1hm_idle 状态（源侧 1hm_idle 家族 active），此时三方对照才测量 C′。

## 九、run 2 判读（2026-11-21 13:14，持武器 idle；日志 [CharacterPanel-g5chain-idle-run2-20261121-drawn.log](../CharacterPanel-g5chain-idle-run2-20261121-drawn.log) 292,041 字节，v121 同身份）

**状态达成**：两侧同处 1hm idle（源/候选各 3 clip；源 = `1HM_Idle`+`MLh_Idle`+`SCAR_1HM_Ready_Dummy1`，data-fnv 与 Track2 注册表逐位同）。

### 9.1 源侧半边（V-P1 决定性证据，与离线 C′ 常量逐项对账）

源图 poseLocal 对**它自己的** `1HM_Idle` 采样（4 组快照 = 两个冻结时刻 × 两检查点，度）：

| 轨 | 骨 | run2 实测 | 离线 C′（gate-5 idle 域） | 判读 |
| --- | --- | --- | --- | --- |
| t30 | NPC R Clavicle | 10.23 / 10.33 | 10.2 | 对账 ✓ |
| t31 | NPC R UpperArm | 29.64 / 29.97 | 30.3 | 对账 ✓ |
| t58 | RUAR twist1 | 16.97 / 17.47 | 17.3 | 对账 ✓ |
| t59 | RUAR twist2 | 8.48 / 8.62 | 8.5 | 对账 ✓ |
| t45 | R Pauldron | 6.57 / 6.37（dpos 0.39） | 6.4 | 对账 ✓ |
| t32/56/57 | R Forearm + twists | **0.00（逐位）** | — | 子链未动 ✓ |
| t39/43 | R Hand / Weapon | **0.00（逐位）** | — | 末端未动 ✓ |
| t38/6/25 | L Hand / L Thigh / Spine1 | 62.9 / 15.3–11.6 / 9.5 | — | **非肩链**：MLh 分支混合贡献（见 9.2） |

偏差跨两个冻结时刻稳定（±1% 内）＝状态常量；修改**精确限定在右肩链五轨**。这就是 gate-5 C′ 的源侧成分：**live 图的 pose 在持武器 idle 下 ≠ 自身 1HM_Idle authored 采样，偏差即 C′ 常量**。

### 9.2 层定位（V-N1/H-GRAPH 成立：层是图内节点）

CENSUS 名集差（源 113 / 候选 112，**源独有 21 节点**）：

- **`WeaponAdjustment_OffsetAnimationGen`**（class=unknown，在源 activeNodes 内）——引擎武器握持校正节点，右肩链五轨修改层的唯一针对性候选。候选克隆**没有**该节点。
- **整个 MLh/魔法分支**（`1HM_MLh_Behavior`、`MagicBehavior`、`MagicReady*` 六节点、`MLh_1HM_Idle_Blend`、`MLh_Idle` 等 19 节点）——源图左手法师就绪分支 active，解释 t38/t6/t25 的混合贡献（`MLh_1HM_Idle_Blend` 与 1HM_Idle 混合；前臂/手在臂链上逐位等值说明该混合对右臂链权重为 0）。

### 9.3 候选侧（V-P1 字面的候选半边未在目标状态测得）

克隆在 F7 重捕获时新建并注入 WeapEquip（13:14:15.341 dispatch），但快照时其 pose 由 **`H2H_Idle`** 驱动（对自身 H2H 采样 0.01–1.13°，对 1HM_Idle 采样 60–70°＝跨状态无效对照）——克隆的 idle 状态选择不随世界的持武器/左手状态（run 1 的分支滞留 + 本轮缺 MLh 分支/WeaponAdjustment，为 G6「源恢复/收敛」的第三项运行时证据）。候选对**自身贡献 clip** 的忠实性在已测状态有界成立（≤1.1°，残差为混合/时钟阶梯）。

### 9.4 裁定

- **C′ 归因闭环**：gate-5 idle 域的 R 肩链 30° 级差异 = **源图在持武器状态下由 `WeaponAdjustment_OffsetAnimationGen` 施加的肩链偏移**；候选克隆无此节点、按 authored 剪辑输出。三条独立证据链互相咬合：gate-5 离线 cand-vs-src 常量 → run2 源-vs-authored 同常量 → census 该节点仅在源侧。
- **V-P1 = PARTIAL**：源侧半边成立且状态匹配；候选半边因克隆状态不随世界未在持武器状态下实测（其"clean"由已测状态有界支持）。
- **C′ 契约方向**（供下一步预注册）：姿态相等性主张应**显式排除右肩链五轨（引擎 WeaponAdjustment 辖区）**并声明该节点存在性差异，或要求克隆宿主同节点（引擎依赖，不作为缺省方向）；左手法师分支混合（MLh）在两侧家族集一致时可对消，克隆缺该分支时对 L Hand/腿/脊另立声明。
- **G6 新增证据**：克隆分支收敛缺口三连（run1 1HM 分支滞留、run2 缺 MLh 分支、缺 WeaponAdjustment 节点）。

## 十、实现记录（2026-11-21，与构建同轮落盘）

- **提交**：实现 `b5259d1da1e`（feat(g5): add the idle chain layer probe for v121；首笔 `94a412d` 因 view 参数 uint64 修正已 amend 并作废其构建身份），分析文档 `6e135d1`。
- **构建**：`cmake --preset Release -DCHARACTER_PANEL_BUILD_LABEL=v121-g5chain-idle-probe-20261121` + `cmake --build --preset Release`，干净 HEAD `b5259d1da1e`、submodule `cl94faaed0c6`（configure 与 build 之间无源改动）。身份 `CharacterPanel-scenegraph-v121-g5chain-idle-probe-20261121-b5259d1da1-cl94faaed0c6-20261121T045052Z`（无 `+dirty`）；CodeView GUID `{7F586C2E-9F62-4CE8-B33B-D19AFE9C6461}` age 530；DLL 2,167,296 字节 `23a785ce2e9f1934413505680fff27381998b9dbc9d7e08870fcb76f491ae535`；PDB 53,088,256 字节 `c14e4241fa2f25c84cbb9663765df8a63c9d11c7282bb0302712cbc7be50935f`。构建仅既有 D9025 `/Ob2`→`/Ob3` 警告。
- **离线测试**：`cl /nologo /std:c++latest /EHsc /W4 /WX /utf-8 /I src tests\idle_chain_probe_test.cpp`（VS2022 Community vcvars64），5 个用例函数全过（名称表/裁剪/解析/肩链计数/非 Shoulder 豁免）。
- **部署**（2026-11-21，无 Skyrim 进程；`CompiledPluginsPath` 未设）：v120 三件套备份 `dist/deploy-backup-20261121-v120-v121pre/`（DLL 2,148,864 `d2cd3b79…`、PDB 52,924,416 `01278f4d…`、INI 17,980 `80c4572b…`，均与安装位逐位一致）→ 安装 v121 DLL+PDB 至 `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\`（目标副本哈希比对一致）→ 运行 INI 派生（模板行级替换 4 键、保留注释与行尾）`dist/run-ini-v121-g5chain-idle-probe-20261121.ini`（18,364 字节 `6071f137…`）→ 安装位写后回读逐键断言 **21 键全过**：`IndependentGraph=inject`、`DisplayPose=true`、`DisplayStep=true`、`IdleChainProbe=true`（均 [Experiment] 节）+ 17 个其它探针键休眠。首版断言脚本误用 [Animation] 节名（四键实际全在 [Experiment]），INI 本体未变，脚本更正后通过——反模式 14 的又一次当场印证。
