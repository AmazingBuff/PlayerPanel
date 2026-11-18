# C-G5-TRANS v2：过渡/Blender 探针修订预注册（2026-11-18）

**状态：IMPLEMENTED / BUILT / 候选 run 2 完成（2026-11-18，v111）；TP-1/3 PASS，TP-2 FAIL，TP-4 部分，TP-5 NOT OBSERVED。** 本契约取代 [v1](../g5-transition-probe-20261118/README.md) 的采样点与容量，不改变 `G4P-7 FAIL` / G5/G6，不产生 `tau_*`。

## 一、为什么需要 v2

v1 候选 run 1（[记录](../g5-transition-probe-20261118/run1-candidate/README.md)）在正确的 `AE-Track2` 环境（OAR 关闭是该档设计，TP-1 PASS）跑完 K5 链，但探针批次无效：

- 每条记录 `flags=0x0002`：4 个状态机上限在 113 个 active nodes 上 overflow；`Default_State/MT_Behavior_State` 可能被截掉；
- 996 条 SM 全 `transitions=0`、0 条 EFFECT：K5 `e_post_sample` 在 full Generate 后，`activeTransitions` 可能已出队；
- equip 1–13 没有 `msg/State3`/`Weap_EquipDefaultState` Blender，只有 body/arms/readied Blender；
- `identity-failed=1` 是 completed=87 的 `SCAR_1hmReadyDummy`（`data_fnv=0`），与门 5 同形，属 Track 2 已知 dummy 记账，不是环境失败。

v2 的目标不是放宽判据，而是把**同一批 K5 记录**放回“Update 后、Generate 前”的时点，并提高容量/定位装备 Blender。

## 二、环境与边界

- 环境固定 `AE-Track2`：OAR 的 4 个模组保持停用；`oar-api=unavailable`、唯一 OAR `[E]` 与 base `1hm_idle.hkx` 是 Track 2 §十二豁免，**不要启用 OAR、不要切回 `AE`**。
- 只读：不写图/姿态/变量，不新增 detour/hook；源侧复用既有 63589 entry 与 63856 apply-return 两个观察点。
- 新键仍为 `[Experiment] TransitionStateProbe=false`；要求候选 `K5FullSample=true`、源 `SamplingLiveness=on` + `K5FullSample=true`。

## 三、采样点与配对键

| 侧 | phase | 代码点 | sequence | 配对键 |
| --- | --- | --- | --- | --- |
| 候选 | `post_update` | `observe_step` 的 `FrameStage::e_post_update`，在 `observe_stage` 判定之前 | `completed`（上一步） | `segment + commanded + phase` |
| 候选 | `pre_sample` | `observe_step` 的 `FrameStage::e_pre_sample`，在 Generate 前 | `completed`（上一步） | 同上 |
| 候选 | `post_sample` | K5 `e_post_sample` / `record_k3_snapshot` | `completed`（本步） | 同上；与 K5 记录同 `completed` |
| 源 | `pre_sample` | 63589 sampling entry（既有 detour） | 0 | `probe_seq` |
| 源 | `post_sample` | 63856 apply-return / K5S 分支 | `seq` | `probe_seq`；与 K5S 同 `seq` |

- 候选 `commanded` 是当前推进序号（1..120/129），`sequence` 是 K5 `completed`；`post_update`/`pre_sample` 用 `commanded` 对齐，`post_sample` 用 `completed` 对齐 K5。
- 源侧 `probe_seq` 在 63589 entry 分配并存入 `ProbeInFlight::Entry`，63856 return 取同一值；pre/post 探针按 `probe_seq` 配对，post 探针的 `sequence` 与 K5S `seq` 相同。
- `phase` 与 `commanded` 进入 `TransitionProbeRecord` header 和日志；v2 record version = 2。

## 四、容量与记录

| 项 | v1 | v2 |
| --- | ---: | ---: |
| state machine | 4 | **16** |
| effect | 4 | **16** |
| Blender | 4 | **8** |
| child / internal | 4 / 4 | **8 / 8** |
| source ring | 2,048 | **2,560** |

- 超过上限仍显式 overflow；任何 overflow/drop 使该批 INVALID，不升限回填。
- 源侧每个 apply 产生 pre+post 两条记录；门 5 源窗口 849 applies → 1,698 条 < 2,560。若窗口超过 1,280 applies，ring drop 会如实使批次 INVALID。

## 五、Blender 定位（census）

候选 `pre_sample`、`segment=equip`、`commanded=1..13` 时，额外输出有界 `TRANSITION CENSUS`：active nodes（cap 256）的 `index/node/vtable/class/name/id`，每步最多 256 行。目的：

1. 若 `State3`/`Weap_EquipDefaultState` 以 `VTABLE_hkbBlenderGenerator` 出现在 activeNodes，v2 的 8 个 Blender 槽应直接捕获；
2. 若它以另一个 vtable 出现（如 `hkbBlendingBehavior` 的 unknown 类），只登记 vtable/name，**不读未知偏移**；TP-5 仍 NOT OBSERVED，下一版另立布局审计；
3. 若它完全不在 activeNodes，census 给出负结果，下一版转父链/`activeNodesChildrenIndices` 或 `allPartitionMemory`，不再重复对象头扫描。

census 是诊断行，不是 TP-5 判据；没有 census 行本身不使批次 INVALID（但必须记录 `census=0`）。

## 六、判据（预注册）

| 编号 | 判据 | 通过条件 |
| --- | --- | --- |
| TP-1 身份/环境 | v111 三件套/存档/`AE-Track2`；Track 2 OAR 豁免行；`identity-failed=1` 若为 dummy 则按已知记账；零非豁免 `[E]` | 全满足 |
| TP-2 记录配对 | 候选 K5 249/249；每步有 `post_update`、`pre_sample`、`post_sample` 三条探针；源每 K5S 有 pre/post 两条；overflow=0、drop=0 | 全满足 |
| TP-3 0→10 目击 | 候选 equip `commanded=1..13` 至少一条 `post_update`/`pre_sample` 的 SM 记录 `currentStateID=10` 且 active transition 的 effect vtable 为 blending；源对应窗口同样 | 全满足 |
| TP-4 计时推进 | 同一 SM/effect 在连续 `pre_sample` 中 `timeInTransition` 非递减；最后 active 点 `timeInTransition >= duration - 1/60`；post_sample 仅作对照（可为空） | 全满足 |
| TP-5 Blender 目击 | census 定位到 `State3` 等价 Blender；其 `blendParameter`/`endIntervalWeight`/child weight 在 equip 1–13 或同 localTime 组间至少一项变化 | 全满足；若 census 显示 unknown 类则 NOT OBSERVED |
| TP-6 相关性 | 对同 localTime 多记录组，标量与 C-G5-STATE v1 PC1 的 Pearson/Spearman 与单调性；只报告 | 不设阈值 |
| TP-7 无副作用 | 无图/姿态/变量写入；无新 detour/hook；`source-drift=none`；产品路径 off | 全满足 |

## 七、运行协议

1. 构建 v111，离线测试；备份当前 v110 三件套后安装 v111 + `k5-transition-candidate` 档（`TransitionStateProbe=true`、`K5FullSample=true`、`K5LayerAudit=false`、`K5Reconciliation=false`）。
2. 候选：MO2 `AE-Track2`（OAR 保持停用）→ Save116 → 一次 panel → equip 120/120 → settle → `Unequip` 144 → unequip 129；关闭退出。
3. 候选通过 TP-2 后，换 `k5-transition-source` 档，同一 DLL/PDB；F7 20 秒自然拔/收剑；源侧分开进程。
4. 归档日志/K5/probe 工件与哈希；TP-1..7 判读。

## 八、停止规则与明确不做

- 任一 overflow/drop、K5 缺代、非豁免 `[E]`、崩溃、多次窗口 → 停止并保留现场；不重跑同一窗口。
- 不启用 OAR/不切 `AE`；不改 `tau`/`epsilon_t`；不删轨；不把 census 或 TP-6 当 G5 通过条件。
- 不把 v1 的 0 EFFECT 读成“transition 不存在”；v2 的 pre-sample 结果才是判据。

## 九、实现影响面

| 文件 | 变更 |
| --- | --- |
| `transition_probe.h/.cpp` | v2 record（phase/commanded/probe_seq）、容量 16/16/8/8/8、ring 2,560、census |
| `independent_graph.cpp` | `e_post_update`/`e_pre_sample` 探针 + equip 1–13 census；`e_post_sample` 复用 |
| `probe_liveness.cpp` / `probe_liveness_policy.h` | `ProbeInFlight::Entry::probe_seq`；63589 pre-sample 入环；63856 post-sample |
| `tests/transition_probe_test.cpp` | v2 容量/预算/phase/census 纯函数 |
| `docs/build.md` | v111 构建/部署/运行记录 |

## 十、实现与构建记录（2026-11-18）

**源码已实现；离线测试 5/5；Release 构建通过；未部署、未上机。**

- record version 2：header 增加 `phase`/`commanded`/`probe_seq`；容量 16/16/8/8/8；source ring 2,560。
- 候选：`e_post_update`/`e_pre_sample` 每步记录，`e_post_sample` 保留；equip `commanded=1..13` 的 `pre_sample` 额外输出 `TRANSITION CENSUS`。
- 源：63589 entry 记录 `pre_sample`（`probe_seq` 存入 `ProbeInFlight::Entry`），63856 return 记录 `post_sample`；两次记录按 `probe_seq` 配对。

**离线测试**：

```powershell
cl /nologo /std:c++latest /EHsc /W4 /WX /utf-8 /I src tests\transition_probe_test.cpp /Fo:build\transition_probe_test.obj /Fe:build\transition_probe_test.exe
build\transition_probe_test.exe
```

结果：5/5，exit 0，无告警。

**构建**：提交 `15f75ac` 后干净树 `cmake --preset Release -DCHARACTER_PANEL_BUILD_LABEL=v111-g5trans-probe-v2-20261118` + `cmake --build --preset Release`，exit 0；身份 `CharacterPanel-scenegraph-v111-g5trans-probe-v2-20261118-15f75ac27b-cl94faaed0c6-20261118T150516Z`（无 `+dirty`）。

| 工件 | 字节 | SHA-256 |
| --- | ---: | --- |
| CharacterPanel.dll | 2,065,920 | `dc89538521072489b90d2f1e144cac6f8a644c0e1f488b2a85458753dd7ddb6b` |
| CharacterPanel.pdb | 52,113,408 | `fe269ce44463675f8206b0023787182fd7600c84bda50dbd3463faf06898bb28` |

**部署（2026-11-18）**：v110 三件套已备份到 `dist/deploy-backup-20261118-v110-v111pre/`（DLL `393421ea…`、PDB `466f4beb…`、候选 INI `7deb21e5…`）。v111 DLL/PDB 与候选档 `dist/g5-transition-v2-20261118/CharacterPanel-g5trans-candidate.ini`（`7deb21e5…`）已安装，安装位哈希/身份逐位核对通过（DLL `dc895385…`、PDB `fe269ce4…`、身份 `…-v111-g5trans-probe-v2-…`）。源档 `CharacterPanel-g5trans-source.ini`（`a94a3bb9…`）已复制但未安装。**候选 run 2 已完成**（见 §十一）；源侧尚未运行。构建记录见 [`docs/build.md#v111-g5trans-probe-v2-20261118`](../../build.md#v111-g5trans-probe-v2-20261118)。

## 十一、候选 run 2（2026-11-18 23:09–23:15，v111）

候选 K5 249/249、equip 120、unequip 129、`state-12-to-0`；探针 747 header（3 phase × 249）、6,672 SM、78 EFFECT（26 个 transition 实例）、813 BLENDER、1,276 census；flags `0x0000`×675、`0x0002`×72。`Default_Behavior` 的 `0→10` 在 cmd=1 采到 blending transition（duration 0.2、time-in 0.016667），`10→1` 在 cmd=87–98 完整推进到 0.183333（= duration−1/60），12 点姿态 PC1 99.83%、与 time-in 的 Pearson 0.992817。72 条 overflow 位于 equip cmd 87–98 与 unequip cmd 1–12；equip 1–13 clean。census 没有 `State3`/`Weap_EquipDefaultState`，active Blender 只有 body/arms/readied/unequip，TP-5 未目击。完整读数与 v3 入口见 [run2-candidate](run2-candidate/README.md)。
