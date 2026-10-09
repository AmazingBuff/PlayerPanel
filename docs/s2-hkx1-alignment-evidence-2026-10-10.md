# HKX1 第 1 步实测证据：动画骨架 ↔ 副本节点（2026-10-10）

- 构建身份：`CharacterPanel-scopy-HKX1-cd646aefae-cl94faaed0c6-20261009T155444Z`，与
  `dist/CharacterPanel-scopy-HKX1-1.2.1.zip` 内 `build-manifest.json` 逐字一致（`source_baseline_dirty=false`，
  基线 `cd646aefae`）。日志首行 `SCOPY BUILD …` 与它相符 → 下面的结论只属于这个包。
- 日志：[CharacterPanel-hkx1-align-20261010-0001.log](diagnostics/CharacterPanel-hkx1-align-20261010-0001.log)
  （306 行，SHA-256 `79b0bcfb38ef7e6c2ec08216fc93f4e179dac9461d061a44f52e35e74b19aa4a`）。
  **无崩溃**：`crash-*` 最新仍是 2026-10-08（探针时代）。
- 三次捕获都是玩家自己的第三人称图（`actor=00000014`），只在暂停的背包里发生。

| 捕获 | 时刻 | 节点/几何 | 剪枝 | 场景 |
| --- | --- | --- | --- | --- |
| 1 | 23:59:12 | 470 / 23 | 0 | 会话 A、CBBE 体型（与 S2P1 的 470 节点一致）；00:00:45 `RELEASE reason=session-boundary` |
| 2 | 00:01:27 | 1584 / 63 | 14 | 会话 B、UBE 体型（与 S2P2 的 1584 节点一致） |
| 3 | 00:02:17 | 1842 / 117 | 67 | 同一会话、外观更复杂（重名节点 147 个） |

## 1. 取图路径在 AE 1.6.1170 上成立

```
SCOPY ANIM source form=00000014 graphs=2 copy-nodes=1570
SCOPY ANIM graph[0] project='DefaultFemale' holder=true root=true bone-nodes=116 anim-bones=116 behavior-graph=true root-generator='hkbStateMachine' binding-set=true bindings=15203 pose-local=116
SCOPY ANIM graph[1] project='FirstPerson' holder=true root=false bone-nodes=99 anim-bones=99 behavior-graph=true root-generator='hkbStateMachine' binding-set=true bindings=2275 pose-local=99
```

- `TESObjectREFR::GetAnimationGraphManager` 在 AE 上可用：它走的是引擎自己的虚函数，**不需要地址库 id**，
  因此没有 `QueryAnimations` / `AnimationSystemUtils` 那类"AE 地址为 0、调用即跳到 0"的风险。
- 角色持有**两个**图：`DefaultFemale`（第三人称，`root=true` 指回被捕获的那张图）与 `FirstPerson`（`root=false`）。
  `holder`/`root` 判据按设计把两者分开，选图因此是**测出来的**而不是猜的。
- 第 2 步的侦察数据：女性行为工程已加载 **15203 条 binding**（第一人称 2275 条）；根生成器是 `hkbStateMachine`；
  `poseLocal` 有 116 项（= 骨架骨数）。

## 2. 骨架 ↔ 节点：核心全中，缺的都是帮手骨与装备附着骨

```
SCOPY ANIM skeleton graph=0 name='NPC Root [Root]' bones=116 bone-nodes=116 bone-node-names-agree=37/116 matched=109 ambiguous=0 duplicate-nodes=0 parent-ancestors=109 missing='x_NPC LookNode [Look], x_NPC Translate [Pos ], x_NPC Rotate [Rot ], Shield, Weapon, Quiver, Belly' wrong-parent='-'
```

- **109/116 精确命中**，`ambiguous=0`：没有哪个骨名对应多个节点，按名匹配在这一轮是安全的。
- 缺的 7 个分两类，都不是核心关节：
  - `x_NPC LookNode [Look]`、`x_NPC Translate [Pos ]`、`x_NPC Rotate [Rot ]` —— Havok 的**辅助骨**（`x_` 前缀，
    瞄准/位移控制），NIF 里本来就没有对应节点；
  - `Shield`、`Weapon`、`Quiver`、`Belly` —— **装备/身形附着节点**，这套装备下不存在。
  - 会话 A 的 CBBE 体型只有 **90/116**（差集被打印上限截成 8 个，实际 26 个）：那是极简体型，裙骨与 SMP 链都不在。
- `parent-ancestors=109`（= matched）且 `wrong-parent='-'`：**动画骨架的父子关系与 NIF 层级一致**——每个骨的骨架
  父骨都是该节点的祖先。所以"写 local 变换、由节点层级合成"这条路成立。（"父骨未解析"的骨也计入该数，
  故最多有 7 个未真正校验。）
- 剪枝解释了走路数与 CENSUS 的差：`pruned-nodes=14`（1584−1570）与 `pruned-nodes=67`（1842−1721，67 是被剪
  子树的根、121 含其子孙）。**对齐量的是真正被绘制的副本**，这正是要对齐的对象。

## 3. 负结果：`boneNodes` 的次序 ≠ 动画骨架的次序

`bone-node-names-agree=37/116`（第三人称）、`78/99`（第一人称）：引擎自己的 `boneNodes[i].node` 与
`animationSkeleton->bones[i]` 同名率只有约三分之一，而两张表长度都等于骨数（116 / 99）。

→ "用 `boneNodes[i]` 做指针对应"这条路**作废**，按名匹配是必需的；并且**任何以 `poseLocal[i]` 为输入的
路线都不能假设它与 `boneNodes` 同序**。HKX2 会把两种次序假设都测一遍（§5），由数值决定哪个是引擎的映射。

## 4. 其余观测

- 面板功能未受影响：F8 绘制正常（CBBE `passes=23`、UBE `passes=45`，`rd-null` 全亮，`bound=331/908`），
  `SCOPY IDLE bound 8/8 channels`，骨盆横移 0.78 单位；日志里只有 4 条既有的
  `Studio lights not fully served by the ledger yet` 警告，没有 `[E]`。
- 捕获 3 的 `duplicate-nodes=147`（其中重名几何 63 组）说明该外观下重名节点很多，但 `ambiguous=0`：
  骨架骨名一个都没撞上重名。

## 5. 判定与下一步

- 第 1 步要回答的问题（"引擎的动画数据能不能叫出副本的节点"）**答案是能**：核心 109/116、零歧义、层级一致。
- `verdict=INCOMPLETE` 来自我把门槛定成"每根骨都必须有节点"，而实测缺的全是**帮手骨与装备附着骨**：
  门槛过严。下一轮改为按类计数（`x_` 帮手骨 / 附着骨），只要缺的骨里没有核心骨就算通过，并打印跳过清单。
- 下一步 **HKX2：把引擎自己的 `poseLocal` 写进副本，做带对照的数值 A/B**（全类型，无未定型布局风险）：
  1. **对照**：先把副本扰动到明显不同的姿态（现成的程序化待机即可），量一次"副本 vs 源角色"的世界差——
     证明这套测量是灵敏的，否则后面的 0 什么也证明不了；
  2. **两次假设**：按"骨架次序"与"`boneNodes` 次序"各写一次 `poseLocal`，各量一次同样的差——**小的那个就是
     引擎的映射**，同时验证四元数→`NiMatrix3` 的约定、按名映射与写路径；
  3. **恢复**捕获姿态，面板会话不受影响。
- HKX2 通过之后才进 HKX3（取一段 clip、用我们自己的时钟采样）——那时的真值就是同一个 `poseLocal`：
  暂停相位下采样结果应当与它相等。
