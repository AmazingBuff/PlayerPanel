# C-G5-TRANS v6 候选父链探针

v115 在 AE 1.6.1170 的既有 K5 候选采样点读取主图的活动节点和子索引表。首轮只回答一个问题：装备第 2–12 步实际经过哪些生成器，作者树的内层 `State3` 是否在这条链上。 [v6 离线审计](../g5-transition-probe-v5-20261119/hkx/stateinfo_layout_audit.md)

**状态：IMPLEMENTED / BUILT / DEPLOYED；候选 run 1 完成（2026-11-20 22:59）；父链资格 PASS，内层 `State3` 未观测（分支二）。**

## 采集范围

- 只采候选装备段第 1–13 步的 `pre-sample` 和 `post-sample`，共 26 个观测点。锚点为已实见的 `DG_1hmEquip`，用同代 clip 指针、绑定与时钟和 K5 对账。前后读数用 `commanded` 对齐，`post-sample` 的 `completed` 与 CPK5 记录对齐。
- 反向记录活动图中所有可达父边，保留原始 edge slot 和必要的输出转发关系。路径节点记录模板、运行实例和所属图。状态机记录当前 ID、StateInfo 下标及 generator，选择器记录实际使用的索引和选中 generator。
- 与已有 `TRANSITION PROBE`、`TRANSITION CENSUS` 和 `K5 RECON` 一起判读。v6 的新读取在现有候选观察阶段进行，CPK5 格式沿用 v114。
- 新行前缀为 `TRANSITION PARENT CHAIN`。`SUMMARY` 登记数量、溢出和读数状态，`CLIP` 与 CPK5 对账，`NODE`/`EDGE` 还原原始边，`SM`/`SELECTOR` 校验实际选择，`BLENDER` 报告 flags 与权重模式。权重读数按同代 clone 与既有 `TRANSITION PROBE` 行关联。每相位最多读取 256 个节点、4,096 个子索引，最多报告父链中的 64 个节点和 128 条边。

## 上机步骤

1. 关闭 Skyrim/SKSE。在 MO2 的 `AE-Track2` 使用之前的 Save116 和装备状态。v114 三件套已备份到 `dist/deploy-backup-20261120-v114-v115pre/`，v115 三件套已装入 CharacterPanel 模组（安装位哈希与身份见「本地构建」）。
2. 确认 `IndependentGraph=inject`、`CandidateEquipPrime=true`、`CandidateEquipPrimeBranch=true`、`CandidateUnequipProbe=true`、`CandidateUnequipEvent=Unequip`、`K5FullSample=true`、`TransitionStateProbe=true`、`TransitionParentChainProbe=true`。旧 `TransitionSubgraphProbe=false`，`SamplingLiveness=off`。Track 2 的 OAR 状态沿用 v114。
3. 启动游戏并加载 Save116。按 v114 的操作打开一次背包，候选回放会自动执行装备与卸下段。等 K5 写入后关闭面板并退出游戏。每次重跑用新的 Skyrim 进程。
4. 保留完整 `CharacterPanel.log` 和同一 session 的 `CharacterPanel-g5k5-*.bin`、`.json`。同时记录意外退出、画面异常或操作偏差。

## 判读

| 项 | 首轮核对 |
| --- | --- |
| 构建及环境 | 日志识别为 v115，运行时为 1.6.1170，`AE-Track2` 和 Save116 与 v114 同场景。 |
| K5 完整性 | 候选 249/249，装备 120、卸下 129，以 `state-12-to-0` 结束。已登记的 SCAR dummy 单条 IDUN 单列。 |
| 正控与覆盖 | 第 1 步的既有 census 预计看见两个 clip 和两个普通 Blender。第 1–13 步各有前后观测，共 26 个 summary，按装备步和相位与 K5 对照。 |
| 父链资格 | 同代 clip 身份可对账，父链完整回溯，范围错误、读取失败、模糊锚点、截断和环均判为观测缺口。多父边是要保留的正常证据。 |
| 分叉 | 链路经过实际 `State3` 时对照运行时 flags、活动子边和有效权重。若完整链路无普通 Blender，转查引擎实际采样相位、同步和输出混合。链路未闭合时先诊断缺口。 |

`SUMMARY status=ok` 表示原始父链范围和采集到的身份检查成立。判读分支时还要核对 `SM`/`SELECTOR` 的 `on-path`。`no-parent`、`incomplete-identity`、读失败、溢出、索引异常或环都须保留原始日志。

首轮为候选侧诊断，正式 `G4P-7` 裁定仍是 **FAIL**。

## 本地构建

2026-11-20 使用仓库预设 `Release`（Visual Studio 17 2022 / v143 14.44.35207、vcpkg `x64-windows-static-md`）在干净树 `0876130` 上重建，标签 `v115-g5trans-parent-chain-20261120`，`cmake --build --preset Release` exit 0。构建身份从 DLL 提取为 `CharacterPanel-scenegraph-v115-g5trans-parent-chain-20261120-087613006d-cl94faaed0c6-20261120T145507Z`（无 `+dirty`）。离线 C++ 测试 10/10 通过（`cl /std:c++latest /EHsc /W4 /WX /utf-8 /I src tests\transition_probe_test.cpp`，exit 0），其中新增两组分别覆盖共享父边与空槽、无效范围与带有效根的环路。

| 工件 | 字节数 | SHA-256 |
| --- | ---: | --- |
| CharacterPanel.dll | 2,113,024 | `5484030D80A20C77196D4AAF7A949C9E9EB406ED6A185AC3E4F37D4E0361A712` |
| CharacterPanel.pdb | 52,613,120 | `B1071FDDBC63CEAE3115E38BBB145A281C4517DABB3171BBB367604E9FDEC9DB` |
| CharacterPanel.ini（候选档） | 17,121 | `DF18E107CD0CC303C47BB645BB2604DB6A558406F5414C3B459493F363646981` |

候选档相对 `config/CharacterPanel.ini` 只改 9 行（见上机步骤第 2 条），`CandidateInputVariable` 保持默认 `IsEquipping`。

**更正（2026-11-20 22:56）**：本节早先登记的 v115 构建（身份 `…-b9bd29fad6+dirty-…`、DLL 2,107,392 B `e1f2c69c…`、PDB 35,106,816 B `69401e8a…`、INI 17,110 B `a19c8d95…`）与其声明的 `dist/g5-transition-v6-20261120/CharacterPanel-v115-AE-Track2.zip` 在仓库、`dist/` 与安装位均无对应工件（当时 `build/` 与安装位仍是 v114），无法复核；已由上面的重建工件取代，早先哈希作废。

**部署（2026-11-20 22:56；Skyrim/SKSE 未运行，MO2 打开）**：安装位 v114 三件套先备份到 `dist/deploy-backup-20261120-v114-v115pre/`（DLL 2,091,520 B `523b097d…`、PDB 52,383,744 B `d954ee51…`、候选 INI 12,449 B `30aa9777…`，与安装位逐位一致）。v115 DLL/PDB + 候选档装入 `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\`，安装位哈希 `5484030d…`/`b1071fdd…`/`df18e107…`，DLL 内身份 `…-v115-g5trans-parent-chain-20261120-087613006d-…` 与 `TRANSITION PARENT CHAIN` 模板在位。运行包 `dist/g5-transition-v6-20261120/CharacterPanel-v115-AE-Track2.zip`（14,413,417 B `6e087fe0…`）三件套逐条哈希与安装位一致。**未启动游戏**；候选 run 待上机。

## 候选 run 1（2026-11-20 22:59–23:01）

K5 249/249（equip 120 `fixed-equip-budget`、unequip 129 `state-12-to-0`）、747 header 全 `flags=0x0000`、唯一 `[E]` 为 Track 2 OAR 豁免、零 `[W]` 相关异常。`TRANSITION PARENT CHAIN SUMMARY` **26/26 `status=ok`**（`seeds=1 roots=1 unresolved=0 empty-slots=0`），`NODE` 444、`EDGE` 418、`SM` 182、`SELECTOR` 130、`CLIP` 26、**`BLENDER` 0**；26 个观测点的链路完全相同：`Master_Behavior` → `RootModifierGenerator` → `RootBoneSwitch_MSG` → `DualWieldShieldBoneRootMSG` → `1st3rdPerson_MSG` → `RootBehaviorGraph`(`Default_State`) → `Default_Behavior`(`Weap_Equip_State`=10) → `WeapEquipModifierGenerator` → `WeapEquipBFR` → `WeapEquip.hkb` → `WeapEquipRoot`(`WeapEquip`=3) → `WeapEquipBehavior`(`WeapEquip_StandingState`) → `Weap_EquipBehavior`(`Weap_EquipDefaultState`) → `Weap_Equip_MSG`(sel 1) → `StateMachine1181`(`State0`) → `ManualSelectorGenerator418`(sel 9) → `DG_1hmEquip`；仅第 1 步多一个 `DefaultBlendResetTransition`(35518, `time-in=0.016667`)。整份日志 `State3` 出现 **0 次**。

同一窗口的装备 2–12 步：clip `local-time` 恒为 `0x3E555556`、7 个 state-machine 全 `transitions=0`、`effects=0`、`blenders=0`；而 K5 的 11 帧姿态 **11/11 唯一**（86/116 轨变化 >1e-4，最大单轨 3.0968）。即该链上没有能逐帧改变姿态的元素。随后的 [H-C1 离线审计](../g5-clip-clock-20261120/README.md) 已否证「第二条采样时钟」：采样时间就是 `clip->localTime (+0xF0)`（`mode=0`、夹点 1.4333），因此冻结的时间等于该 clip 采样输出逐帧相同，差异必在采样之后。**V6-1/2/5 PASS，V6-3 NOT OBSERVED，V6-4 N/A；分叉为分支二（链路完整、无普通 Blender）**，下一层按 §七.2/§七.3 转入采样之后的写入者/输出混合。完整读数、工件哈希与候选假设 H-C1–C3 见 [run1-candidate](run1-candidate/README.md)。`G4P-7` 仍 **FAIL**。
