# HKX17 实测证据：崩溃已防住，且坐实"暂停捕获时动画数据未装载"（2026-10-10）

- 构建身份：`CharacterPanel-scopy-HKX17-882f3121ff-cl94faaed0c6-20261010T150155Z`，与 manifest 一致
  （`source_baseline_dirty=false`）。DLL `f5f03788…`、PDB 均从最终 zip 部署并校验。
- 日志：[CharacterPanel-hkx17-guard-20261010.log](diagnostics/CharacterPanel-hkx17-guard-20261010.log)
  （SHA-256 `a33ed46a…`）。**零崩溃**（最新 crash 仍是 22:54 的 HKX16 那次）、**零 `[E]`、零 `[W]`**。
  绘制与程序化待机正常（`IDLE bound 8/8`、`DRAW frame=1 passes=23`）。

## 1. 闸生效，且给出了明确答案

```
SCOPY ANIM mapping UNAVAILABLE reason=animation-data-not-present
SCOPY ANIM play clip='instance-binding' type=spline duration=20.00s tracks=99 copy-resolved=90/99 truth-bones=39
SCOPY ANIM play UNAVAILABLE reason=animation-data-not-present
```

两处采样点都被拦住 → **HKX16 的崩溃面已消除**（零崩溃实证）。
且答案不是"数据可用、只是我闸加错了"，而是**数据确实不存在**。

## 2. 结论：暂停捕获时，正在播放的 idle 的动画数据未装载

这与 binding 的 `transformTrackToBoneIndices` 未填是**同一模式**：

| 层 | 头部 | 数据 |
| --- | --- | --- |
| `hkaAnimationBinding` | vtable／`animation`@+0x18 全对，布局实锤 | `transformTrackToBoneIndices` = `array=0x0 size=0` |
| `hkaAnimation`（spline） | `type=spline`／`duration=20.00s`／99 轨 全对 | `transformOffsets`／`blockOffsets`／`data` 之一不可读 |

**该引擎分阶段填充对象，头部合法不构成"其余已就位"的证据。** 这是我此前所有校验的**系统性盲区**
——`read_raw_animation()` 只读 +0x10~0x1C 三个头部字段，HKX16 第一次真正调用采样器才撞上。

## 3. 关键对照：同为 spline 的 `weaponAdjustment.hkx` 全通过

```
weaponAdjustment.hkx  idx=1629  binding=0x24ef37a26d0  animation=0x24ef37a22d0
                      probe=valid  type=spline  duration=3.33s  transform-tracks=7  →  reason=valid
MT_Idle.HKX(实例)      idx=12218  binding=0x24ec419fbd0  animation=0x24ec4166f10
                      probe=valid  type=spline  duration=20.00s transform-tracks=99  →  animation-data-not-present
```

**两者都是 spline、都过了头部校验**，差别在轨道数（7 vs 99）与是否正在被实例播放。
→ 说明**问题不是"spline 不能采样"**，而是**这一个动画对象的数据区没装载**。
7 轨的小动画反而完整，提示装载与其使用状态/规模有关（**未确证，仅记录**）。

## 4. 附带数据

- `copy-resolved=90/99`、`truth-bones=39` —— 副本能解析 90 轨、真值表只有 39 项，说明骨架侧映射本有基础，
  但采样被拦在数据层之前，这些数字**不构成映射已成立的证据**。
- 实例 idx 第四轮仍在变（12066／12221／12219／**12218**），皆 `kClone` —— OAR 动态替换行为稳定复现。

## 5. 关卡状态

| 关卡 | 状态 |
| --- | --- |
| H01 构建／实例身份 | **通过**（四轮复现） |
| H02 类型／持有 | 部分 —— 对象与布局已确证；**数据未装载，映射与采样均不可进行** |
| H03–H05 | 未测 |
| S1 安全回收 | 仍阻塞 |

## 6. 下一步：只剩选项 B（未暂停帧观察），需改探针时机

三条静态/实例路线都已排除：binding 表未填（HKX13-15）、control 访问器读同一张空表（HKX15）、
**动画数据本身未装载（本轮）**。PRD §6.4 第 2 条明确允许的剩余路线是：
**在 O 正常世界更新（未暂停）时被动观察同一实例**，把数据快照下来供暂停时回放。

**实施要点（动工前需用户确认，因为要改探针时机）**：
- 在世界帧（未暂停、玩家 3D 存在）记录一次实例侧 binding + 动画数据可用性；
- **只读**：不写源角色、不推进世界模拟（FR-05 红线）；
- 结果缓存，暂停捕获时对照 —— 回答"**同一实例在未暂停时数据是否已装载**"。

**如果 B 仍显示未装载**：则"经引擎已装载动画取数"这条路线在本版本应判为不可行，
届时应转向 `poseLocal` 录制回放（HKX2 已验证可读、按 `boneNodes` 索引）或自研映射来源。
**这是方向性决策，需用户拍板**，不由我默认推进——PRD §6.4 禁止因 HKX 取数失败就静默算作 FR-03 完成。
