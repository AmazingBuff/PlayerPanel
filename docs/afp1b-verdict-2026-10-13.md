# AFP1b 判读（2026-10-13 22:44–22:48）：泊位是空输出的原因；**暂停又会把它拿走**

- 包：`CharacterPanel-scopy-AFP1-64090de829+dirty-cl94faaed0c6-20261012T144137Z`，DLL `d74f6b46…`。
- 日志：`docs/diagnostics/CharacterPanel-afp1b-held-20261012.log`；零崩溃、零 `[E]`。
- 上机动作：22:45:23 **F4** → `held-pinned`；22:45:50 **F2**（mark 1，世界期）；22:45:5x **F3** → `tick+apply`；
  22:47:50 **F3** → `step`（推图随之关）；22:47:56 **F2**（mark 2，背包暂停）。

## 1. 决定性结论：**泊位就是空输出的原因**（假设成立）

| 状态 | `num-anim-bones` | pose 轨 | 标定 |
| --- | --- | --- | --- |
| 泊深 + 世界期（AFP1 首轮） | 0 | `num-data=0 reason=pose-empty` | 无法标定 |
| **held + 世界期（本轮 mark 1）** | **98** | **`num-data=98 reason=ok`** | **`skeleton 86/92 ratio=0.93 match=1`** |
| held + **暂停**（本轮 mark 2） | **0** | `num-data=0 reason=pose-empty` | `no-candidate` |

- 站到玩家处（fade 钉 0.05）后，引擎**开始为傀儡求值并生成最终输出**：98 个姿态元素、`on-tracks=[0,1,2,3,4,5,8,10,11]`、
  `elem=48`、`cap=116`；动画骨架次序在傀儡上也再次胜出（**0.93** vs boneNodes 0.42），`named=109` 与玩家同源。
- **但一进背包（世界暂停）它就归零**：`num-anim-bones=0`，pose 轨回到 `pose-empty`。
- 也就是说：**这条路线现在的阻塞点不是"姿态没被应用到节点"（HKX42），而是"暂停时引擎根本不生成姿态"**。
  我们的手动推图能让图内部（clip 时间／状态机／OAR 决策）继续走（HKX37/38 已证），但**推导出
  `generatorOutputs` 那一步仍然不发生**。

**注**：mark 2 同时满足"暂停"与"推图关"（F3 刚切到 `step`），两者未分离。**修复后的包已经能在暂停且未标定时
先读姿态再报理由**，所以下一次 F2 会直接把它们分开：`blocked=no-pose-while-paused`（暂停本身拿走了输出）
vs `unverified-world-first`（有姿态、只是没在世界期标定）。

## 2. 六根超差骨全是腿，而且**全是网格真正读的**

mark 1（傀儡）：`outliers='NPC L Thigh','NPC L Calf','NPC L Foot','NPC R Thigh','NPC R Calf','NPC R Foot'`，
`max-rot=7.596°`，`necessary=6 helper=0 palettes=21/44 entries=7382 verdict=classified`。
玩家同轮 4 根超差骨同样在腿部、`necessary=4`。

→ 按 R04 的口径，**necessary≠0 即"样本阻断"**：即使 0.9 的映射门通过了，也不能把这一轮记成"最终姿态正确"。
候选解释（**待验，不作结论**）：腿正是引擎输出之后还有后处理的地方（foot IK／落地贴地），
所以"图的原始输出"与"引擎应用到节点后的结果"在那里本来就该不同。

## 3. 显示侧：累加器没有记录相机

`camera` 行：`ui3d-camera=0x1840907c100 accumulator=0x1840904b400 accumulator-camera=0x0`
→ **`NiAccumulator::camera` 为空**，既不是 UI3D 相机也不是世界相机。所以"把 UI3D 相机挪一挪"这条**没有证据支持**；
AFP2 的取景按设计输入里的退路走：**draw 窗口内临时改根变换并逐项恢复**（PRD §6.6 允许，需写明同步与恢复）。
附带事实：UI3D 相机 `world-t=(0,0,0)`、视线 = −Y、视锥 `l/r/t/b = −0.2/0.2/0.1/−0.1`、`near=15 far=20480`；
傀儡 `world-t=(1106.7,−832.3,−1475.9)`、**离轴 `lateral=1844.7`**、`eye-distance=2023.8`、`fade=0.050`。

## 4. R01 前置：傀儡的蒙皮几何**没有设备缓冲**

`geoms subject=puppet geometries=48 skinned=44 property=40 none=8 effect=1 renderer-data=4 skinned-with-rd=0`
→ **44 个蒙皮几何里没有一个有 `rendererData`**（引擎从未画过这个 Actor）＝ runs 49-52 的条件；
引擎的 `SetupAndDrawPass` 会惰性创建（run 95 已证），所以这是**要管理的风险，不是墙**。
对照玩家：`geometries=117 skinned=63 effect=54 renderer-data=3`。

## 5. 本轮暴露并已修掉的三个缺陷（两个是我自己的）

1. **标定每帧重试**：`generation=59 / verifications=58`（10 秒内）——把"尝试次数"当成了"身份代数"，
   并刷了 70 行日志。**已修**：代数的定义 = 3D 根或图换身份；一次失败不再每帧重试，等身份变化或重新武装。
2. **world 比较没有对齐**：两个 subject 都是 `world-within-1deg=0/N`、最坏位移 2044.9——因为我的合成从根骨的
   **local** 起算，而节点 world 还带着 Actor 自身的位置（约 2000 单位）。**已修**：每条链先按"根骨的节点 world"
   对齐成一个 delta 再比；**已加单测**（有/无对齐分别为满分与全灭，且对齐后无位移残差）。
3. **应用阶段的名字桥接面太窄**：它只从 `boneNodes` 表建名字表（傀儡仅 **37/116** 命中），而仪器是走整棵树
   （**109/116**）。于是应用判 `0.84 below-ratio` **拒绝写入**，而同一时刻仪器判 `0.93 match=1`——
   **应用拒绝的原因是我的桥，不是姿态**。**已修**：桥走整棵树，再反查回引擎活骨表的索引（每帧解析仍只读表，
   不跨帧持指针；表里没有的节点记 `unreachable`）。

## 7. 追加判读（2026-10-13 22:56–22:58 会话）：**是暂停拿走了输出，不是推图状态**

- 包：`…AFP1-ee54611706…T145401Z`（含三个修复）；日志 `docs/diagnostics/CharacterPanel-afp1c-paused-20261012.log`。
- 状态：`placement=held-pinned`、`drive=tick+apply`（**推图开**）、mark `paused=1`。

| 观察 | 傀儡 | 玩家（同帧对照） |
| --- | --- | --- |
| `num-anim-bones` | **0** | 116 |
| pose 轨 | 空（`no-candidate`） | `num-data=116` |
| 标定 | —— | **`match=1 ratio=1.00 within-1deg=109/109`** |
| 应用阶段 | 重复 `blocked=no-pose-while-paused` | —— |

→ **之前两个变量（暂停、推图关）同时动过，这次分开了：推图是开的，姿态仍然没有。暂停本身就是原因。**
应用阶段现在"先看有没有姿态再报理由"，这条新的 block 就是它给出的答案。

**三个修复在同一次上机里得到验证**：

- `verifications=1 generation=0`（上一轮是 `58/59`）→ 标定不再每帧重试，代数也不再是"尝试次数"。
- 玩家侧 `world-within-1deg=88/109`（上一轮 `0/109`）→ world 对齐修复成立；残余 21 根与 local 侧的
  腿部/CME 家族一致，不是对齐问题。
- 新的 `names skeleton=116 tree=109 table=37` → **引擎的骨表只寻址 116 根动画骨里的 37 根**，
  而树里有 109 根同名节点。这条决定了应用阶段的**写入覆盖面只能到 37 根**（我改成树桥 + 反查骨表之后仍受此限），
  也解释了两个数字的差别：仪器走树是 `86/92=0.93`，应用走树再反查骨表是 `31/37=0.84`，
  失败的 6 根都是同一批腿骨。**新开问题**：傀儡的骨表是"缩减集"，还是它指向的节点根本不在傀儡自己的树里？
  （廉价判据：把骨表节点与傀儡树、玩家树各比一次，看哪边命中。）

## 8. 下一步（本轮已备好探针）

暂停期既没有 `generatorOutputs` 的姿态，那就问第二个源：`hkbCharacter::poseLocal`（HKX2 实测为 **boneNodes 次序**，
与图输出是两套数组）。**本轮已把 poseLocal 纳入 A0/A1 配对**（`pose-local=N/N pose-local-moved=K pose-local-max-rot=X`），
下次会话按 F2 即可回答：

- **poseLocal 在暂停期仍然随我们的时钟变化** → 路线有源：把姿态来源从 `generatorOutputs` 换成 `poseLocal`
  （次序按 HKX2 的 boneNodes），应用阶段照现状即可工作；
- **poseLocal 也冻结** → 缺的是"引擎的求值"本身，下一步锁定 `hkbBehaviorGraph::Generate`
  （OAR 钩的那一层，`OpenAnimationReplacer/src/Hooks.cpp:211`）能否由我们按帧调用，而不是继续在应用阶段打转。

## 9. 上机步骤（下一次）

1. 进存档 → **F4** 到 `held-pinned` → 等装配窗 → **F3** 循环到 `tick+apply`（**世界期**）→
   看 `SCOPY AFP2 verify … match=1` 与 `names skeleton=116 tree=109 table=?`。
2. **开背包** → **F2** ×2（间隔几秒）：看 `A0A1` 的 **`pose-local-moved`** 是 0 还是非 0——
   这一条决定姿态来源换不换。
3. 同一对标记里 `A2` 行会说明应用是否还在 `blocked=no-pose-while-paused`。
