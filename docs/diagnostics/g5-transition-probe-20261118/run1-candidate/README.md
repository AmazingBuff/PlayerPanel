# C-G5-TRANS v1 候选侧 run 1（2026-11-18 22:46–22:51，**INVALID for TP-1/2/3/5**）

**一句话：候选 K5 链本身 249/249、equip 120、unequip 129、state 12→0；但探针运行不满足预注册：OAR 未加载（不是 Track 2）、4 个状态机上限在每条记录都 overflow（`flags=0x0002`）、996 条 SM 行全部 `transitions=0`、没有 EFFECT 行、没有 `msg/State3` Blender 行。该批不能用于支持或否证 H-T1/H-T2。**

## 一、身份与工件

| 项 | 读数 |
| --- | --- |
| 构建身份 | `CharacterPanel-scenegraph-v110-g5trans-probe-20261118-de954f7c39-cl94faaed0c6-20261118T143148Z` |
| 运行档 | `CharacterPanel-g5trans-candidate.ini` 12,419 B `7DEB21E5…D06531` |
| 日志 | [CharacterPanel.log](CharacterPanel.log) 653,789 B `4DAE56F5…C91469` |
| K5 候选 bin | [CharacterPanel-g5k5-candidate.bin](CharacterPanel-g5k5-candidate.bin) 1,410,452 B `AEB357B8…80432D` |
| K5 JSON | [CharacterPanel-g5k5-candidate.json](CharacterPanel-g5k5-candidate.json) 785 B `9241B259…39F8F3` |
| 原始 K5 名 | `…-v110-g5trans-probe-…-20261118T144953Z.bin/.json`（SKSE 日志目录） |

## 二、候选 K5 链（本批唯一有效部分）

- `K5 SNAPSHOT close records=249 attempted=249 overflow=false write-failed=no`。
- `K5 RECON expected=249 attempted=249 recorded=249 missing=0 truncated=0 two-read-bad=0 clip-overflow=0 identity-failed=1 drift-records=1 dropped=0`；`equip-expected=120 equip-outcome=completed`、`unequip-expected=129 unequip-outcome=completed reason=state-12-to-0`。
- 首个 equip 样本：`1HM_Equip.hkx`（binding-index=22，duration=1.4333），`CANDIDATE INPUT default=10`；与 D3/v96a 链一致。

## 三、探针计数

| 行 | 数量 | 说明 |
| --- | ---: | --- |
| `TRANSITION PROBE side=candidate` | 249 | equip 120 + unequip 129，K5 逐条配对成立 |
| `TRANSITION PROBE SM` | 996 | 每条记录恰好 4 个状态机（=上限） |
| `TRANSITION PROBE EFFECT` | **0** | 所有状态机 `transitions=0`；没有 effect 可读 |
| `TRANSITION PROBE BLENDER` | 271 | 仅 body/arms/readied/unequip 等 Blender |
| `TRANSITION PROBE CHILD` | 548 | 上述 Blender 的 child |
| `TRANSITION PROBE INTERNAL` | 271 | 上述 Blender 的 internal 前 4 项 |
| header `flags` | `0x0002` × 249 | `State_Machine_Overflow`，每条记录都置位 |
| SM `transitions` | `0` × 996 | 没有任何 active transition 被采到 |

**状态机名单**：equip 段前 4 个固定为 `StateMachine1181`、`Weap_EquipBehavior`、`WeapEquipBehavior`、`WeapEquipRoot`（state 0/0/0/3）；unequip 段前 4 个为 `StateMachine2531`、`WeapUnequipTypeBehavior`、`Weap_Unequip_Standing_Behavior`、`Weap_UnequipLowerBody`。`Default_State`/`MT_Behavior_State` 未出现在被采的 4 个中，且每条记录均因超过 4 个状态机而 overflow，无法判断它是否有 active transition。

**Blender 名单**：equip seq=1 只有 `BodyWeightBlend`、`ArmsBlend`；seq=2–13 没有 Blender 行；seq=87 起为 `1stP_MLh_1HM_Idle_Blend`、`SCAR_1HM_Ready_Blend`、`Weap_Readied_Lean_Blend`；unequip 段为 `Weap_UnequipBlend`。**没有任何 `Weap_EquipDefaultState/msg/State3` 或 `State3` 行。**

## 四、环境阻断

- 22:47:04：`[E] failed to get plugin info for OpenAnimationReplacer`，紧接着 `OAR hooks unavailable: Open Animation Replacer is not loaded`。
- 22:49:53：`ANIMATION WORLD … oar-api=unavailable oar-result=unavailable-unknown … bound='Animations\female\mt_idle.hkx'`。
- 22:49:38 虽有一行 `OAR bases stage=world-prewarm … missing=7`，但 OAR hooks 未安装，`oar-api=unavailable`；候选使用 base `Animations\1hm_idle.hkx`，不是 Track 2 的 Smooth Moveset OAR 资源。
- `K5 RECON identity-failed=1 drift-records=1`；与 Track 2 冻结环境不一致。

## 五、按预注册判据

| 判据 | 裁定 | 理由 |
| --- | --- | --- |
| TP-1 身份与环境 | **INVALID** | v110 身份/档位匹配，但 OAR 未加载；存在非豁免 `[E]`；不是 `AE-Track2` 冻结环境 |
| TP-2 记录配对 | **INVALID** | 249/249 配对成立，但 `flags=0x0002` 在全部记录置位；状态机 overflow |
| TP-3 0→10 目击 | **NOT OBSERVED** | 0 条 EFFECT；996 条 SM 全部 `transitions=0`；相关 `Default_State` 可能被 4 上限截掉 |
| TP-4 计时推进 | **N/A** | 没有 effect 读数 |
| TP-5 Blender 目击 | **NOT OBSERVED** | 未见 `State3`/`Weap_EquipDefaultState` Blender；只有 body/arms/readied/unequip Blender |
| TP-6 相关性 | **N/A** | 没有过渡/目标 Blender 标量 |
| TP-7 无副作用 | **PASS（本批范围内）** | 候选链无崩溃、`source-drift=none`、无图/姿态/变量写入；但环境/探针资格仍 INVALID |

**裁定：本批对 H-T1/H-T2 不作支持或否证；不得写成“transition 不存在”或“Blender 无变化”。**

## 六、根因与 v2 方向

1. **状态机上限太小（已证）**：每条记录 4 个 SM 仍 overflow；`Default_State/MT_Behavior_State` 可能在第 5 个以后。v2 至少 16 个 SM/16 个 effect，并按 `currentStateID`/active transition 优先，而不是只取 activeNodes 前 4 个。
2. **K5 post-sample 太晚（强假设）**：`activeTransitions` 在全部 996 条上为 0，且 `timeInTransition` 从未出现；过渡可能在 `Generate` 后已出队。v2 必须在**候选 `e_post_update` / `e_pre_sample`** 以及源侧 **63589 采样入口**增加同代采样点，`e_post_sample` 只作对照；header 增加 `phase`。
3. **`msg/State3` 不在 active `hkbBlenderGenerator` 集合（已证）**：equip 1–13 的 active Blender 只有 body/arms/readied，非装备 State3；可能它是另一种运行时类（`hkbBlendingBehavior`，无 CommonLib 布局）或不在 activeNodes。v2 需在 1–13 做有界 vtable/name census，或从 active `1HM_Equip` clip 的父链/`activeNodesChildrenIndices` 向上定位；不得直接猜偏移读未知类。
4. **OAR 环境（已证）**：必须通过 MO2 `AE-Track2` 启动并确认 OAR loaded；日志应出现 `oar-api=available`、Track 2 OAR 资源路径，且没有 22:47:04 的 OAR `[E]`。否则新批次仍 INVALID。

## 七、边界

- 不改变 `G4P-7 FAIL`/G5/G6；不产生 `tau_*`；不删除/放宽任何判据。
- 本批 K5 链结果可保留为候选链健康证据；transition/Blender 结论只能来自 v2 新批次。
