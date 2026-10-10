# S2 里程碑：面板由游戏已注册的动画驱动，且与引擎姿态定量吻合（2026-10-11）

- 构建身份：`CharacterPanel-scopy-HKX28-38e18ae5f0-cl94faaed0c6-20261010T180735Z`，
  与包内 manifest 一致（`source_baseline_dirty=false`）。
- 日志：[CharacterPanel-hkx28-spread-20261011.log](diagnostics/CharacterPanel-hkx28-spread-20261011.log)
  （SHA-256 `a68de8fd…`）。**零崩溃、零 `[E]`**。

## 1. 结论

```
SCOPY ANIM load bindingset idx=12219 binding=0x1c843972be0 anim=0x1c843939b10
        anim-type=spline duration=30.00s tracks=99 binding-valid=false reason=tracks-unreadable data=present
SCOPY ANIM play clip='instance-binding' type=spline duration=30.00s tracks=99 copy-resolved=90/99
SCOPY ANIM play ground-truth best-t=22.50s max-pos-delta=0.102 max-rot-delta=1.40deg worst-pos-delta=10.722
SCOPY ANIM play phase-check  t=4.438s max-pos-delta=0.000 max-rot-delta=0.15deg bones=37
SCOPY ANIM play phase-check spread bones=37 within-1deg=37
        worst-bones='NPC Head [Head]:0.15deg/0.000u, NPC L UpperarmTwist1 [LUt1]:0.08deg/0.000u,
                     NPC L Thigh [LThg]:0.06deg/0.000u, NPC R Foot [Rft ]:0.04deg/0.000u,
                     SkirtBBone01:0.03deg/0.000u, NPC Pelvis [Pelv]:0.03deg/0.000u,
                     NPC Spine1 [Spn1]:0.03deg/0.000u, NPC L Calf [LClf]:0.00deg/0.000u'
SCOPY ANIM play ground-truth verdict=match best=0.1021 worst=10.722
SCOPY DRAW frame=1 … animation-driver=instance-binding
```

**四个数同时成立：**

| 观测 | 值 | 含义 |
| --- | --- | --- |
| `phase-check max-pos-delta` | **0.000** | 在引擎自己的相位上，位置**完全一致** |
| `phase-check max-rot-delta` | **0.15°** | 旋转残差 0.15 度 |
| `phase-check within-1deg` | **37 / 37** | **全部参与骨都在 1° 以内** |
| `ground-truth verdict` | **`match`** | **首次通过**（0.1021 < 10.722×1%） |

`SCOPY DRAW` 全程 `animation-driver=instance-binding` —— **驱动者是引擎装载的动画**。

## 2. 4.65° → 0.15°：是精度，不是映射错

上一轮（HKX27）残差 4.65°，本轮 0.15°。**按骨拆分回答了成因**：

- **不是少数骨被引擎修正**（若如此会有具名离群骨）——实际是**全部骨在一个平滑衰减的极小残差上**；
- 最差骨 `Head 0.15°` → `LUt1 0.08°` → `LThg 0.06°` → `Rft 0.04°` → `SkirtBBone01/Pelv/Spn1 0.03°` → `LClf 0.00°`；
- **所有位置差值都是 0.000u**。

**这是数值精度的特征，不是映射问题，也不是引擎修正。** 上一轮较大的那个值属于另一段动画（时长 20.00s vs 本轮 30.00s，是 OAR 的不同替换件）。

## 3. 恒等映射：由"假设"变为"实测"

本轮 `binding-valid=false`（该 OAR 索引的轨道表仍空），所以采样走的是
**"轨道 i → 骨骼 i"** 的恒等映射 —— 而 `phase-check` 以 **0.000 / 0.15° / 37 骨全在 1° 内** 证实了它。

**PRD §6.4 禁止的是"未经验证就使用恒等映射"，不是禁止恒等映射本身。**
本轮满足的正是它要求的验证方式：**用引擎自己的 `poseLocal` 做同相位 A/B**。

## 4. 这条链最终是怎么通的（十轮的可复用结论）

| 环节 | 结论 | 依据 |
| --- | --- | --- |
| 动画从哪来 | **命名角色名表里的索引 → 引擎 `Queue`/`Load`** | HKX18 `queued=1 loaded=true` |
| 绑定从哪读 | **`characterInstance.animationBindingSet`，空则回退 `setup`** | OAR `FakeClipGenerator.cpp:106-126` |
| 绑定集条目 | **引擎在 Load 后填充它**（原版索引给出**完整表**；OAR 替换件可能为空表） | HKX23/26 |
| 采样入口 | **`SamplePartialTracks`（槽 04），不是 `SampleIndividualTransformTracks`（槽 06）** | OAR `ActiveAnimationPreview.cpp:38` |
| 为什么槽 06 崩 | 槽 06 按轨道号索引 `transformOffsets`，而**本引擎故意把它留空** | HKX24 转储 + HKX16 崩溃 |
| 轨道表空怎么办 | **恒等映射，并由 `poseLocal` A/B 验证** | 本轮 |
| 位置写入 | **按骨名解析 → 写 local → 一次自顶向下重算** | HKX1/HKX2 |

**关键教训（写给下一个会话）**：HKX16–HKX24 共九轮都在"数据是否装载"上找原因，
而真因是**采样入口选错**（槽 06 vs 04）。**社区源码里 OAR 调哪个函数，是最省时间的证据。**

## 5. 关卡状态

| 关卡 | 状态 | 依据 |
| --- | --- | --- |
| H01 构建／实例身份 | **通过** | 横幅与 manifest 一致；kClone 实例与索引定位 |
| H02 类型／持有 | **通过** | 引擎装载、`data=present`、绑定集条目由引擎填充 |
| H03 采样 | **通过** | 同相位位置 **0.000**、旋转 **0.15°**、**37/37 骨在 1° 内**、`verdict=match` |
| H04 副本驱动 | **通过** | `animation-driver=instance-binding` + 用户目视 + F2 可开关 |
| H05 隔离／生命周期 | **未测** | O/NPC 是否被额外推进、失效停止、切会话清理 |
| **S1 安全回收** | **仍阻塞** | 反复开关／持续换装／长会话的前置门槛 |

**不要据此宣称 FR-03 全部通过**：H05 与 S1 未闭环，展示动作切换（播指定的另一个动画）也未做。

## 6. 下一步

1. **H05 隔离**：本轮只读观测 O 与附近 NPC 的姿态/位置在面板操作前后是否不变（PRD §6.4 红线）；
2. **S1 安全回收**：它是反复开关与换装的前置门槛，仍是最硬的阻塞项；
3. **展示动作切换**：同一套链换一个索引即可（例如从待机切到某个指定动作），验证"可切换"。
