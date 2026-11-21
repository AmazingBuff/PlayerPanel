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

## 七、实现记录（实现后追加）

- 构建：`cmake --preset Release -DCHARACTER_PANEL_BUILD_LABEL=v121-g5chain-idle-probe` + `cmake --build --preset Release`（待填身份/DLL/PDB 哈希）。
- 离线测试：`cl /nologo /std:c++latest /EHsc /W4 /WX /utf-8 /I src tests\idle_chain_probe_test.cpp`（待填用例数）。
- 部署：v120 三件套备份 `dist/deploy-backup-…`（待填）。
