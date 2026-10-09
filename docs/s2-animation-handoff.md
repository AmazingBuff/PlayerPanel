# S2 动画：HKX／引擎动画路线侦察与下一工作包（下一个会话从这里开始）

- 日期：2026-10-09（本轮以文档收尾，未再改代码）
- 读者：下一个会话的 Agent / 开发者。本文是 S2（FR-03 动作）的**当前入口**；机制证据在
  [探针证据](s2-anim-probe-evidence-2026-10-09.md)，历史方案在 [S2/S3 方案](s2-s3-plan.md)，
  测试单在 [下一轮测试单](scene-graph-copy-next-test-2026-10-09.md)。

## 0. 一句话状态

副本**写骨骼能驱动画面**这一点已被四轮游戏内实测确证；但当前**程序化待机没有真值、无法判定对错**，
用户已明确要求改为**用真实动画数据（HKX）驱动并可对比验证**。本工作包 = 用引擎自己的采样器播放
真实动画并驱动副本，附**数值 A/B 对比协议**；程序化待机降级为"没有可用动画时的兜底"，**不再调参**。

## 1. 方向修正（用户批评，已接受）

用户原话要点：既然已经知道骨骼能移动，为什么不直接用简单的 HKX 测试？HKX 还能对比；硬编码的
idle 完全没法测试。

**接受。** 复盘：程序化待机的正弦通道**没有基准**——它只能证明"能动"（这正是前面四轮已证明的），
不能证明"动对了"；任何判断都退化成观感，于是出现了"腰部好像没动"这种无法界定的反馈。真实动画有
作者意图、有可对比参照（源角色播放同一动画），因此**可判定**。结论：
**停止在 idle 参数上迭代；把数据源换成引擎播放／采样的真实动画。**

（附：idle 那轮暴露的问题本身是真实的设计缺口，已修并留档——骨盆与躯干在本骨架里是**兄弟分支**
（`CME LBody` / `CME UBody`），且绕骨骼自身原点的旋转不移动该骨骼本身，所以胯部必须带**平移**通道。
改动画驱动后这个缺口自然消失：我们将驱动**全部**骨骼而不是几个关节。）

## 2. 已确证的机制（不要重复验证）

| 结论 | 证据 |
| --- | --- |
| 写副本节点 → 画面随动（两个身形、两套骨架） | S2P1/S2P2 日志与用户目视；`probe-effective` 全部 true |
| 执行顺序必须固定为：基础摆位 → 写骨骼 → pass 准备 → draw | PRD §6.3；探针移入绘制窗口后成立 |
| 引擎在**绘制中**按我们写的节点重算蒙皮矩阵（缓冲 3×3 = 节点世界旋转 × 0.35 图形缩放，匹配摆动后姿态，残差 0.0002） | S2P4 原始转储 + [analyze_tslot_scale.py](diagnostics/analyze_tslot_scale.py) |
| F2 开关配对：开＝动、关＝复位静止 | S2P3 用户目视（R06） |
| **警示**：蒙皮缓冲槽位下标 **≠** `bones[]` 下标（自匹配 5/31、4/71） | S2P4 + [analyze_tslot_mapping.py](diagnostics/analyze_tslot_mapping.py) |

## 3. 侦察结果：引擎侧可取用的对象（本次读的是本仓 CommonLibSSE checkout）

**结论：不需要自己写 HKX 解析器。** 引擎已把"加载→解码→采样"做完，CommonLibSSE 暴露了类型与偏移，
我们只需**选一个动画、用自己的时钟推进、调用引擎的采样、按轨道→骨骼映射写进副本**。

| 能力 | 位置 |
| --- | --- |
| 引擎自己的采样函数（任意时间点取姿态） | `hkaAnimation::SampleTracks(time, hkQsTransform* out, float* floatTracks, hkaChunkCache*)` — [hkaAnimation.h:44](extern/CommonLibSSE/include/RE/H/hkaAnimation.h#L44) |
| 轨道 → 骨骼索引映射 | `hkaAnimationBinding::transformTrackToBoneIndices` — [hkaAnimationBinding.h:26](extern/CommonLibSSE/include/RE/H/hkaAnimationBinding.h#L26) |
| 动画骨架（骨名＋父索引，用于与副本节点对齐） | `hkbCharacterSetup::animationSkeleton` — [hkbCharacterSetup.h:25](extern/CommonLibSSE/include/RE/H/hkbCharacterSetup.h#L25)；`hkaSkeleton::{bones,parentIndices}` — [hkaSkeleton.h:33](extern/CommonLibSSE/include/RE/H/hkaSkeleton.h#L33) |
| 可自驱的播放控制（公开 `localTime`） | `hkaAnimationControl`：`localTime`／`binding`／`SampleTracks(...)` — [hkaAnimationControl.h:23-38](extern/CommonLibSSE/include/RE/H/hkaAnimationControl.h#L23) |
| 引擎的 clip 对象（含动画名与播放模式） | `hkbClipGenerator`：`animationName`／`binding`／`animationControl`／`localTime`／`time`／`playbackSpeed`／`mode` — [hkbClipGenerator.h:66-96](extern/CommonLibSSE/include/RE/H/hkbClipGenerator.h#L66) |
| 骨索引 → NiNode 表（引擎自带映射） | `BShkbAnimationGraph::boneNodes`（`BoneNodeEntry{ NiNode* node; … }`）— [BShkbAnimationGraph.h:38-43](extern/CommonLibSSE/include/RE/B/BShkbAnimationGraph.h#L38)、成员在 0x160 |
| 引擎每帧生成的姿态缓冲 | `BShkbAnimationGraph::generatorOutputs[2]`（0x220）；结构见 [hkbGeneratorOutput.h](extern/CommonLibSSE/include/RE/H/hkbGeneratorOutput.h) |
| 角色的当前 local 姿态 | `hkbCharacter::poseLocal`／`numPoseLocal` — [hkbCharacter.h:53-54](extern/CommonLibSSE/include/RE/H/hkbCharacter.h#L53) |
| 取图路径 | `BShkbAnimationGraph::characterInstance`（0xC0，`hkbCharacter`）；`hkbCharacter::{setup(0x50), animationBindingSet(0x68), behaviorGraph(0x58)}` — [hkbCharacter.h:45-48](extern/CommonLibSSE/include/RE/H/hkbCharacter.h#L45) |
| 手动推进动画图（可选） | `IAnimationGraphManagerHolder::UpdateAnimationGraphManager(const BSAnimationUpdateData&)` — [IAnimationGraphManagerHolder.h:55](extern/CommonLibSSE/include/RE/I/IAnimationGraphManagerHolder.h#L55) |

### 必须验证的未知项（不要当成已知）

1. **`hkbAnimationBindingWithTriggers` 在 CommonLibSSE 里只有前向声明、没有头文件**
   （[hkbAnimationBindingSet.h:11](extern/CommonLibSSE/include/RE/H/hkbAnimationBindingSet.h#L11)），
   而 `hkbAnimationBindingSet::bindings` 是它的指针数组（[:21](extern/CommonLibSSE/include/RE/H/hkbAnimationBindingSet.h#L21)）。
   → 走这条路前必须**运行时核对布局**（例如用两个实例交叉验证），否则优先走有类型的
   `hkbClipGenerator::binding` 或 `hkaAnimationControl`。
2. **暂停时行为图是否 tick**：本项目的既有结论是暂停时引擎计时器不推进、世界动画不前进；
   "我们自己调 `SampleTracks`"因此不需要图在跑（这是首选路径的原因），但"读引擎每帧生成的姿态
   （`generatorOutputs`／`poseLocal`）"在暂停时是冻结的——若要那条路，需要记录（未暂停时）＋回放（暂停时）。
3. `hkaChunkCache`：`SampleTracks` 的最后一个参数可为空的假设**未验证**（spline 压缩动画可能需要它）。
   若需要，得看 `hkaChunkCache` 是否可以自行分配。
4. 轨道数 vs 骨骼数：动画轨数可能少于骨架骨数（未动画的骨保持参考姿态），映射后必须**只写有轨道的骨**，
   其余保持捕获姿态。

## 4. 三条取数路线

| 路线 | 形态 | 成本／风险 | 真值与对比 |
| --- | --- | --- | --- |
| **A. 引擎已加载的动画（推荐先做）** | 从角色取 `hkbClipGenerator`／`hkaAnimationControl`，用我们的时钟设 `localTime`，`SampleTracks` 后写副本骨骼 | 中；未知项 1/3 需在游戏内确认 | 同一动画、同一采样函数；可与源角色的同一动画做**数值 A/B** |
| **B. 用户提供的 HKX 文件** | 文件由**引擎**加载（动画 mod 常规路径：替换/注册到行为工程或动画事件），我们仍走 A 的采样链 | 中＋素材流程；需要用户确认文件放哪、如何让引擎加载 | 同上；文件可换、可对比 |
| **C. 自研 HKX 解析器** | 自己读 packfile（`hkaSplineCompressedAnimation` 等）并采样，不依赖引擎加载 | 高（packfile＋spline 解码自研） | 最强自由；但真值仍需与引擎播放同一文件对比 |

**推荐：先 A（一次会话即可验证映射与采样链是否成立），随后把 B 的同一个文件接进同一条链。**
C 只在 A/B 无法满足需求时再考虑。

## 5. 下一工作包：HKX1 构建（一次构建、一次会话，要么播起来、要么明确缺哪一环）

目标产物：`CHARACTER_PANEL_SCENE_COPY_EXPERIMENT=ON` 的测试包，新增 `SCOPY ANIM …` 系列日志。

1. **骨架对齐（先做，失败即停）**：从 `BShkbAnimationGraph::characterInstance → setup->animationSkeleton`
   取骨名表，与副本节点名逐条比对，打印 `SCOPY ANIM skeleton bones=N matched=M/N missing='…'`。
   映射不成立 → 不进入下一步（并打印差集）。
2. **取动画**，按优先级并各自打印证据：
   ① `hkbClipGenerator`：在行为图里按 `animationName` 找（优先 Idle）；打印 `source=clip:'…' duration=… tracks=… mode=…`；
   ② `hkaAnimationControl`：直接用其 `localTime`＋`SampleTracks`；
   ③ 最后才尝试 `hkbAnimationBindingWithTriggers`——**碰之前先做布局交叉核对**并在日志里写明核对结果。
3. **采样与写入**：`localTime = f(steady_clock)`（循环模式，周期 = `animation->duration`／`playbackSpeed`）
   → `SampleTracks` 得 `hkQsTransform[]` → 按 `transformTrackToBoneIndices` 取骨索引 → 骨名 → 副本节点
   → 写 **local** 变换（`NiTransform`），随后复用已验证的子树重算（`apply_world_delta_downward`）。
   只写有轨道的骨，其余保持捕获姿态。
4. **热键语义**：**F2 = 播放/停止动画驱动**（默认开启）；`F6` 探针仅在
   `-DCHARACTER_PANEL_S2_PROBE=ON` 的诊断构建里存在（本机 F6 被其他插件占用）。
   程序化待机保留为**兜底**：找不到动画或映射失败时自动回退，并在日志里说明回退原因（不再调参）。
5. **失败面必须可读**：任何一环失败都要打印**具体**原因（哪个指针为空、布局核对结果、匹配率、缺哪些骨），
   不允许静默跳过——下一轮的依据必须是数据而不是猜测。

## 6. 验收与对比协议（这是"可测试"的核心）

1. **数值 A/B（主判据）**：同一动画、同一相位下，逐骨比较**副本的 local 变换**与**源角色的 local 变换**，
   输出 `SCOPY ANIM compare bones=N/N max-rot-delta=…deg max-pos-delta=…u`；
   差异应在浮点精度量级（插值/相位对齐误差另计并写明）。
2. **目视 A/B（辅助）**：同一个动画在源角色（世界侧播放）与副本（面板内）同相位对照截图。
3. **独立性（FR-03/FR-05 红线）**：退出背包后，O 与附近 NPC 的姿态、位置、物理无任何变化；
   面板操作不改变 O；不通过推进世界模拟来驱动 P。
4. **动作连续性**：切换动作、循环接缝、根运动不移动世界角色（根变换由摄影棚接管）。
5. 每轮单独日志与构建身份（`build-manifest.json` 的身份串必须与日志首行一致）。

## 7. 需要用户决定的一件事

**动画数据来源选 A / B / C？** 若选 B，请一并说明：文件放哪里、用什么方式让引擎加载它
（替换某个 vanilla 动画／注册到行为工程／用 `NotifyAnimationGraph` 触发的自定义事件）。

## 8. 工程状态（下一个会话直接可用）

- 构建（实验包）：
  ```powershell
  cmake -S . -B build-scopy -DCHARACTER_PANEL_SCENE_COPY_EXPERIMENT=ON -DCHARACTER_PANEL_BUILD_PROBE=OFF -DCHARACTER_PANEL_PROBE_REVISION=<轮次>
  cmake --build build-scopy --config Release --target CharacterPanel CharacterPanelCopyMathTest
  ctest --test-dir build-scopy -C Release --output-on-failure
  cmake --build build-scopy --config Release --target CharacterPanelSceneCopyPackage
  ```
  诊断探针构建另加 `-DCHARACTER_PANEL_S2_PROBE=ON`（探针落在 F6）。
  **注意**：新增 `src/*.cpp` 后必须重新 configure（GLOB 在 configure 期求值）。
- 默认（Actor 路线）构建同理用 `build/`，`cmake --build build --config Release --target CharacterPanel`。
- 当前产物：`dist/CharacterPanel-scopy-IDLE3-1.2.1.zip`（身份 `…-IDLE3-…-20261009T152323Z`，
  DLL SHA-256 `b01eee73…`）；此前 S2P1–S2P4 四个探针包与日志、分析脚本都在 `dist/` 与
  [docs/diagnostics](diagnostics/)。
- **未提交**：工作区仍是 dirty（本轮全部改动 + 四轮日志 + 三个分析脚本都未提交）；
  `build-manifest.json` 里 `source_baseline_dirty: true`。建议先把现状提交，再开 HKX 工作。
- 热键现状（IDLE3 产品构建）：`F7` 捕获、`F8` 绘制、`F3` 旋转、`F4` 释放、`F2` 待机开关（默认开）；
  探针已退出产品构建。

## 9. 明确不要做的事

- **不要继续调 idle 的振幅／周期**来"让它像待机"——没有真值，无法判定。
- **不要在解析出真实调色板映射之前写蒙皮矩阵缓冲**（槽位下标 ≠ `bones[]`，写多少都打偏）。
- **不要用推进世界模拟／推进 O 的动画来驱动副本**（FR-03/FR-05 红线）。
- **不要把"静默回退到程序化待机"当成动画链路通过**——回退必须写进日志并计入未通过。
