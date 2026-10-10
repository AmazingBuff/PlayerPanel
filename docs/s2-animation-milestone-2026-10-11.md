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

## 7. 用户反馈（2026-10-11 02:1x）

### 7.1 H05：用户游戏内观察 —— 未观察到对 O 与 NPC 的影响

**记录如实**：用户目视**未观察到**面板操作对 O 或 NPC 造成影响。
**但这不等于 H05 通过**：PRD §6.4 要求的是**只读仪器化基线**
（面板操作前后 O／NPC 的姿态与动画进度读数对比），目视只是补充证据。
**H05 状态 = 用户目视无异常，仪器化证据仍缺。**

### 7.2 缺陷（用户发现）：重开背包时动画从上次位置续播，而非重新开始

**根因**：背包打开时引擎是暂停的，**工作室自己的时钟是唯一在动的东西**
（`ClipPlayer` 的 `steady_clock`、`IdleDriver` 的时钟），而它们的 `m_time` **跨"关闭→重开"被保留**。

**修复（HKX29，提交 `4d552aa5`，DLL `aef8a67a…` 已部署校验）**：

- `ClipPlayer::restart()` / `IdleDriver::restart()` —— **只把时钟归零，不动已绑定的关节**
  （区别于 `reset()`，后者会清掉绑定、强制重绑）；
- `PanelMonitor` 在背包**打开时置标志**，`SceneGraphCopy` **每次访问消费一次**并重启两个时钟；
- 日志：`SCOPY ANIM restart at panel open capture=… driver=…`。

**为什么用菜单事件而不是在渲染钩子里看边沿**：`process_requests()` 跑在渲染侧钩子中，
**并非每帧都执行** —— 背包关闭时看不到下降沿，重开也就不成其为边沿。

### 7.3 OAR 适配：调查结果（这是本轮的重要结论）

用户要求：**动作切换应与 OAR 适配** —— 每个动作播放完成后、需要播下一个动作时，
**按面板人物当前状态走 OAR 的逻辑**筛选下一个动作，利用 OAR 的随机/变体机制。

**调查结论：OAR 的公开 API 做不到这件事。**

| API | 暴露的能力 | 能否用于"求值选择" |
| --- | --- | --- |
| **Animations** | `GetCurrentReplacementAnimationInfo(clipGenerator)`、两个 `ClearConditionStateData` | **否** —— 只能查**已存在 clip** 的当前替换信息 |
| **Conditions** | `AddCustomCondition` 等 | **否** —— 只能**注册**自定义条件（扩展 OAR），**不能求值** |
| **Functions** | `AddCustomFunction` 等 | **否** —— 同上 |
| **UI** | ImGui/菜单开关 | 否 |

`ICondition::Evaluate(refr, clipGenerator, subMod)` 确实存在，但它属于**接口类型定义**
（供自定义条件实现），**没有"给我一组条件、我替你求值并选动作"的入口**。

**另有两条硬约束：**

1. **面板人物不是 actor**。OAR 的条件以 `TESObjectReREF*` 为求值对象，
   而副本是复制的场景图，**没有 FormID、没有 actor 状态** —— OAR 无法对它求值。
2. **暂停期间 OAR 不在跑**。OAR 在 clip 的 `Activate`/`Update` 期改写 `animationBindingIndex`，
   而背包打开即暂停 → **面板开着的整段时间里 OAR 的判定不会前进**。

**因此"面板状态驱动 OAR 判定"在当前架构下无法直接实现**，只能选下面之一：

| 方案 | 做法 | 代价 | 代价的诚实说明 |
| --- | --- | --- | --- |
| **A（推荐）跟随玩家的 OAR 选择** | 未暂停时记录玩家 clip 的 `animationBindingIndex`（**那就是 OAR 当前的选择**）与其变体，暂停时面板从**记录下来的池子**里按序/随机播放 | 低 | **跟随的是玩家状态，不是面板独立状态**；但面板人物本就是玩家的副本（同装备/同身形），二者状态高度一致 |
| **B 重写 OAR 的选择逻辑** | 自己读 OAR 的 JSON 条件配置并对面板状态求值 | **极高** | 等于复刻 OAR（条件类型繁多），且 OAR 升级即失效 |
| **C 让 OAR 直接管面板** | 给面板一个引擎内的 clip generator | 不可行 | 面板不是 actor，没有行为图，OAR 的钩子看不到它 |

**方案 A 还可用 OAR 的 Animations API 增补**：`GetCurrentReplacementAnimationInfo` 能给出
当前替换件的**路径/子模组/变体名**，可用于日志与去重。

**这一步需要用户拍板**（涉及"跟随玩家状态"是否是可接受的产品语义），我不默认推进。
