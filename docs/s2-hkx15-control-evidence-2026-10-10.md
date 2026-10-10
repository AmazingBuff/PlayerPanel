# HKX15 实测证据：选项 D 裁定为无用；control 自带 99 元素权重数组（新线索）（2026-10-10）

- 构建身份：`CharacterPanel-scopy-HKX15-5a1af365b0-cl94faaed0c6-20261010T143103Z`，与 manifest 一致
  （`source_baseline_dirty=false`）。DLL `a8dc0e90…`、PDB 均从最终 zip 部署并校验。
- 日志：[CharacterPanel-hkx15-control-20261010.log](diagnostics/CharacterPanel-hkx15-control-20261010.log)
  （SHA-256 `211a5522…`）。**零崩溃、零 `[E]`、零 `[W]`**。绘制与程序化待机正常。

## 1. 裁定：选项 D（虚调用 control 的两个访问器）无用

```
SCOPY ANIM instance control at=0x155734decc0 binding@0x38=0x15543c18d20 same-as-clip-binding=true
        localTime=1.094 weight=1.000 ttw-size=99 ftw-size=0
```

**`same-as-clip-binding=true`** —— control 的 `binding`（@0x38）**就是** clip 那个表为空的 binding
（`0x15543c18d20`，与 `instance clip` 行的 `binding=` 逐位相同）。

按上一轮写好的判读表：访问器读的是同一张空表 → **返回同样的空结果，D 作废**。
**结论：不必为它破"不虚调用"红线。** 这是本轮最有价值的产出——**省掉了一次红线例外**。

## 2. CLib 的 `hkaAnimationControl` 布局同样实锤正确

```
control qwords='00007ff69ed4a2b8 … 3f8000003f8c0837 00000155731ba280 0000006300000063
                0000015572c16000 0000001000000000 0000015543c18d20 0000000000000000
                8000000000000000 000000003f800000'
```

| 偏移 | 解出 | 语义 | 校验 |
| --- | --- | --- | --- |
| +0x10 | `localTime=1.0940`、`weight=1.0` | 与日志自报 `localTime=1.094 weight=1.000` **一致** | ✓ |
| +0x18 | `0x155731ba280` | `transformTrackWeights._data` | 堆指针 |
| +0x20 | size=99, cap=99 | `transformTrackWeights` | ✓ 与 `ttw-size=99` 一致 |
| +0x28/0x30 | `0x15572c16000` / size=16 | `floatTrackWeights` | — |
| **+0x38** | **`0x15543c18d20`** | **`binding`** | ✓ 与 clip binding 逐位相同 |
| +0x50 | `motionTrackWeight=1.0` | — | ✓ |

11 qwords = 0x58 = `sizeof(hkaAnimationControl)` ✓，三个字段独立自洽 → 布局零疑问。

## 3. 新线索：control 自带 99 元素数组（不是映射表，但是重要旁证）

`transformTrackWeights`：`_data=0x155731ba280`、**size=99**，与 `animation->numberOfTransformTracks=99` **数字相同**。

- **它不是映射表**：`transformTrackWeights` 的类型是 `hkArray<std::uint8_t>`（CLib 声明），逐轨道一个字节的
  **权重**，不是 `std::int16_t` 的骨索引。所以不能直接当 `transformTrackToBoneIndices` 用。
- **但它是强旁证**：**control 已被正确构造到 99 轨规模、localTime 在推进（1.094）、weight=1.0** ——
  说明这个 clip **确实在播、control 是活的、数据完整**，唯独 binding 的 `transformTrackToBoneIndices` 没填。
- 这进一步收窄：**不是"对象没构造"，而是"这一张表没填"**。

## 4. 三轮复现（同一结论、索引每次不同）

| 轮次 | idx | cloneState | binding | control | 轨道表 |
| --- | --- | --- | --- | --- | --- |
| HKX13 | 12066 | kClone | 活 | 活 | 不可读 |
| HKX14 | 12221 | kClone | 活 | 活 | `array=0x0 size=0` |
| HKX15 | **12219** | kClone | 活 | 活 | `array=0x0 size=0` |

索引每轮都变（12066 / 12221 / 12219），**符合 OAR 按条件动态替换**；三轮都是 `kClone`、binding/control 都活、
都是同一张表缺数据 → **结论稳定，不是偶发**。

## 5. 关卡状态

| 关卡 | 状态 |
| --- | --- |
| H01 构建／实例身份 | **通过**（三轮复现） |
| H02 类型／持有 | 部分 —— 对象已得、布局实锤、**映射仍缺**；D 已裁定无用 |
| H03–H05 | 未测 |
| S1 安全回收 | 仍阻塞 |

## 6. 下一步：只剩选项 B（未暂停帧观察）

D 已排除，C（轨道 i=骨骼 i）被 PRD §6.4 明确禁止（除非先拿真值 A/B 验证）。
剩下 **B：在 O 正常世界更新（未暂停）时被动观察同一实例**，看轨道表是否被填上。
PRD §6.4 第 2 条明确允许这条路。

**实施要点（需改动探针时机，动工前先确认）**：
当前 `report_animation_instance()` 只在 `capture()` 内、即**暂停的背包里**调用一次。B 需要：
- 在世界帧（未暂停）也记录一帧实例侧的 binding 信息；
- 只读、不写源角色、不推进世界模拟（FR-05 红线）；
- 结果缓存下来，供暂停捕获时对照 —— 即回答"**同一实例在未暂停时表是否已填**"。

## 7. 方向修正（用户指出，已接受）：不该继续追 mapping 的取得途径

用户原话要点：现有逻辑**已经能控制面板内人物的骨骼矩阵**、从而控制动作；所以取 HKX 的目的
**只是拿到每个时刻的矩阵数据**。应当关注"如何用现有解析方式拿到这些矩阵"，而不是查 OAR 动态替换。

**接受。** 复盘错误：binding 的 `transformTrackToBoneIndices` 只是**取得映射的一种途径**，
我却把它当成唯一途径，为此追了三轮 OAR 动态替换（HKX13/14/15）。
**正确的目标函数是"每时刻每根骨的变换"**——它可以直接对 `hkaAnimation` 采样得到，
映射则用引擎自己的 `poseLocal`（HKX2 已验证可读、按 `boneNodes` 索引）**当独立参考去测量**，
而不必等引擎填某张表。

### HKX16 已按此实现并发包

- 提交 `8e903c5d45`，包 `dist/CharacterPanel-scopy-HKX16-1.2.1.zip`，身份
  `CharacterPanel-scopy-HKX16-8e903c5d45-cl94faaed0c6-20261010T145114Z`（`source_baseline_dirty=false`）。
- DLL `236afd32…`、PDB **均已从最终 zip 部署并校验一致**；单测 1/1 通过。
- ①实例侧把它拿到的 `hkaAnimation` 交给播放阶段（不再因 binding 缺表而丢弃）；
  ②**不再假设"轨道 i = 骨骼 i"**——动画自报 99 轨，而该角色一个图 116 骨、另一个 99 骨，
  故对**每个图的骨架各试一遍**，以"能否复现引擎当前 `poseLocal`"打分，取最优者（日志
  `SCOPY ANIM mapping skeleton=… bones=… worst-rot-delta=…deg best`）。
  映射因此是**测出来的**，符合 PRD §6.4 禁止盲配的要求。

**判读表**：

| 观测 | 含义 |
| --- | --- |
| 某个骨架 `worst-rot-delta` 明显低于其他（如 <30°） | **映射已测得**，接 ground-truth 相位扫描 |
| 各骨架分数接近、都很高 | 轨道编号与两套骨架都不是简单同序 → 需另找映射来源 |
| `ground-truth verdict=match` | **S2 驱动链闭环**，F2 可用自有时钟播放 |

**未变**：仍只读、零虚调用、从最终 zip 部署；S1 安全回收仍阻塞。
