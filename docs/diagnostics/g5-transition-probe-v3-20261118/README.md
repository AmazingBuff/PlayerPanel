# C-G5-TRANS v3：overflow 修复与 0→10 辅助数组观测（2026-11-18）

**状态：IMPLEMENTED / BUILT / 候选 run 1 完成（2026-11-18，v112）。** TP-1/2/3 PASS；TP-4 的 `10→1` PASS、`0→10` 只有起点；TP-5 仍 NOT OBSERVED。本契约接续 [v2](../g5-transition-probe-v2-20261118/README.md) 的 run 2；不改变 `G4P-7 FAIL` / G5/G6，不产生 `tau_*`。

## 一、v2 run 2 的输入

v2 候选 run 2（[记录](../g5-transition-probe-v2-20261118/run2-candidate/README.md)）已取得：

- `0→10` 在 cmd=1 采到 `hkbBlendingTransitionEffect`（duration 0.2、time-in 0.016667）；cmd≥2 后 `activeTransitions` 为空；
- `10→1` 在 cmd=87–98 完整推进到 0.183333，12 点姿态 PC1 99.83%、Pearson 0.992817；
- 72 条 header `flags=0x0002`：equip 87–98、unequip 1–12；equip 1–13 clean；
- census 无 `State3`；active equipment 节点为 `WeapEquip.hkb` / `WeapEquipBFR` / `WeapEquipModifierGenerator` 等。

v3 只修两个可执行缺口：**overflow** 与 **0→10 效果移出 activeTransitions 后的去向**。TP-5 的 `State3` 不在 activeNodes，另立布局/父链审计，不在本契约内声称通过。

## 二、变更

### 2.1 容量

- state machine / effect **16→32**；Blender 8、child/internal 8 不变；source ring 2,560 不变。
- 目标：消除 equip 87–98 / unequip 1–12 的 `0x0002`；仍保留 overflow 即 INVALID。

### 2.2 日志

- 每条 `SM`/`EFFECT`/`BLENDER`/`CHILD`/`INTERNAL` 行增加 `segment=`；不再依赖 header 顺序消歧。
- 对 `Default_Behavior` 状态机，每条 SM 行后输出 `TRANSITION AUX`：
  - `transitionFlags` 头 `+0xC0`（data/size/capacity）；
  - `wildcardTransitionFlags` 头 `+0xD0`；
  - `delayedTransitions` 头 `+0xE0`；
  - 每个数组最多读 0x10 字节（delayed 0x40）原始 hex，**不解释元素类型/步长**；
  - 目标：确认 `0→10` 是否在 `delayedTransitions`/flags 中，或确认它只在 `activeTransitions` 一帧。

### 2.3 Census 分类

- 增加 `hkbBehaviorReferenceGenerator`（`WeapEquip.hkb`）与 `hkbModifierGenerator`（`WeapEquipModifierGenerator`）的 vtable 分类，census 只记 class/name/id/vtable；不读未知偏移。
- 不尝试通过 `hkbBehaviorReferenceGenerator` 的运行时 behavior 指针跳转（布局未审计）；TP-5 仍 NOT OBSERVED。

## 三、判据

| 编号 | 判据 | 通过条件 |
| --- | --- | --- |
| TP-1 身份/环境 | v112 + AE-Track2 + Track 2 OAR 豁免；dummy identity 记账 | 全满足 |
| TP-2 记录配对 | 候选 K5 249/249；每步 3 phase；零 overflow/drop | 全满足 |
| TP-3 0→10 目击 | cmd=1 `Default_Behavior state=10` + blending transition | 全满足 |
| TP-4 计时推进 | `10→1` 12 点单调到 ≥duration−1/60；`0→10` 若 aux 解释推进则另记 | 10→1 全满足；0→10 只记录 |
| TP-5 Blender | `State3` 仍不在 activeNodes → NOT OBSERVED；census 分类改进只作诊断 | 不设通过 |
| TP-6 相关性 | `10→1` 的 12 点 PC1/Pearson 复算；0→10 仍不足 | 只报告 |
| TP-7 无副作用 | 无图/姿态/变量写入；`source-drift=none` | 全满足 |

## 四、运行协议

1. 构建 v112，离线测试；备份 v111 三件套，安装 v112 + 候选档（同一 `TransitionStateProbe=true` 档）。
2. MO2 `AE-Track2`（OAR 停用是设计）→ Save116 → 一次 panel → equip 120/120 → settle → `Unequip` 144 → unequip 129；关闭退出。
3. 判读：`flags=0x0002` 应为 0；`segment=` 每行齐全；`TRANSITION AUX` 只对 `Default_Behavior` 出现；`10→1` 12 点相关复算；`0→10` aux 是否给出 delayed/flags 线索。
4. 源侧不在本轮；TP-5 另立布局/父链审计。

## 五、明确不做

- 不启用 OAR、不切 `AE`；不改 `tau`/`epsilon_t`；不删轨。
- 不读 `hkbBehaviorReferenceGenerator`/`hkbBlendingBehavior` 未审计偏移；不把 AUX 原始 hex 当语义。
- 不把 v3 的 overflow 修复或 aux 结果写成 G5 通过；G4P-7 仍 FAIL。

## 六、实现影响面

| 文件 | 变更 |
| --- | --- |
| `transition_probe.h/.cpp` | v3 version、容量 32/32、segment 每行、AUX 头/raw hex、behavior-reference/modifier 分类 |
| `independent_graph.cpp` | 无需改采样点；复用 v2 三相位 |
| `probe_liveness.cpp` | 无需改；source ring 不变 |
| `tests/transition_probe_test.cpp` | v3 容量/版本/class 名称 |
| `docs/build.md` | v112 构建/部署/运行记录 |

## 七、实现与构建记录（2026-11-18）

**源码已实现；离线测试 5/5；Release 构建通过；候选档已部署，未上机。**

- record version 3；SM/effect 32；`segment=` 每行；`Default_Behavior` 的 AUX 头/raw hex；behavior-reference/modifier 分类。
- 测试：`transition_probe_test` 5/5，exit 0。
- 构建：提交 `3e273e6` 后干净树，身份 `CharacterPanel-scenegraph-v112-g5trans-probe-v3-20261118-3e273e68e6-cl94faaed0c6-20261118T154505Z`（无 `+dirty`）。

| 工件 | 字节 | SHA-256 |
| --- | ---: | --- |
| CharacterPanel.dll | 2,070,016 | `57e02ed6cb9f9f96d0fb8098c32fd2f258ff83985bf3cda63a1fc538d2dd4340` |
| CharacterPanel.pdb | 52,203,520 | `e099237dc51dc4a2fa553e031e5b47db792b7229a330846f5edda8fa0efff7ad` |

**部署（2026-11-18）**：v111 三件套备份到 `dist/deploy-backup-20261118-v111-v112pre/`（DLL `dc895385…`、PDB `fe269ce4…`、候选 INI `7deb21e5…`）。v112 DLL/PDB + 候选档 `dist/g5-transition-v3-20261118/CharacterPanel-g5trans-candidate.ini`（`7deb21e5…`）安装，安装位哈希/身份逐位核对通过（DLL `57e02ed6…`、PDB `e099237d…`）。源档 `a94a3bb9…` 未安装。候选 run 1 已完成（见 §八）；源侧尚未运行。构建记录见 [`docs/build.md#v112-g5trans-probe-v3-20261118`](../../build.md#v112-g5trans-probe-v3-20261118)。

## 八、候选 run 1（2026-11-18 23:49–23:54，v112）

K5 249/249、equip 120、unequip 129、`state-12-to-0`；747 探针 header、6,780 SM、78 EFFECT、813 BLENDER、2,241 AUX、1,276 census；**headers 全部 `flags=0x0000`（overflow 修复）**。`0→10` cmd=1 parent blending transition（duration 0.2、time-in 0.016667）；`10→1` cmd=87–98 到 0.183333，12 点 PC1 99.83%、Pearson 0.992817（与 v2 run 2 逐位复现）。`transitionFlags` size 4 head `0000E9D1`、wildcard 7 零、delayed size 0，cmd 1–13 恒定；0→10 不在 delayed。census 仍无 `State3`，TP-5 NOT OBSERVED。完整记录见 [run1-candidate](run1-candidate/README.md)。
