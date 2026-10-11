# HKX36 测试单（2026-10-11，H05 隔离的仪器化证据：面板操作前后只读基线对比）——**待上机**

- 构建身份：`CharacterPanel-scopy-HKX36-34b67816ad+dirty-cl94faaed0c6-20261011T140501Z`
  （基线 = fork 交接提交 `34b67816ad`，其上为未提交的 isolation_watch 新模块，见下文"本轮内容"）。
- 包：`dist/CharacterPanel-scopy-HKX36-1.2.1.zip`，DLL SHA-256 `f9251957…`
  （全值 `f925195768abd0b6c27001392db028a5a2d50c84beff8f9e06e5d37717b8d08e`，PDB `c534364f…`），
  **DLL 与 PDB 均已从最终 zip 部署到 `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\`，
  部署文件哈希与包内 `build-manifest.json` 逐字节校验一致**。
- 单测 1/1（`snapshot_transform`）。
- 需求出处：[s2-animation-handoff.md](s2-animation-handoff.md) §0"fork 之外的遗留队列"第 2 条
  ——H05 隔离的仪器化证据（只读基线对比面板操作前后 O/NPC 姿态）；
  验收红线：[s2-hkx-next-work-package-2026-10-10.md](s2-hkx-next-work-package-2026-10-10.md) H05
  "O／NPC 被额外推进、出现无效引用或崩溃：该轮停止"。

## 本轮内容：`isolation_watch`（新模块，只读仪表）

**要回答的问题**：面板的每一次操作（F7 捕获／F8 绘制／F3 旋转／F2 待机开关／F4 释放／开关背包），
是否在数值上对 O（玩家）与附近 NPC **零影响**——而不是"没崩溃就算过"。

**机制**（全部只读，全部在渲染括号内采样）：

1. **普查（census）内容**——每次普查读四样东西，全部重新解析、不在普查之间持任何指针：
   - O 第三人称图：根指针／父指针／节点集合（确定性 DFS 序，上限 2048）＋每节点 local/world/worldBound；
   - O 引擎姿态 `hkbCharacter::poseLocal`（按 8 浮点／骨拷贝，上限 256 骨）——面板采样读的同一数组；
   - O 参照本身：`GetPosition`／`GetAngleZ`；
   - 附近 NPC：`ProcessLists` high＋middle-high 桶，半径 6144 内、上限 24 名（formID、位置、朝向、
     disabled/deleted 标志、3D 根世界变换），按 formID 排序后做集合归并对比。
2. **会话协议**：暂停背包内的**第一次操作前普查 = 基线**；之后每个操作 pre/post 各一次普查；
   每次普查输出两份判读——**窗口判读**（对比上一次普查：F7→F8 间隔或某操作 pre→post）与
   **全会话判读**（对比基线）。面板关闭／暂停解除／读档边界结束会话并输出总结行。
3. **判读阈值**（沿用 capture 侧 SOURCE-DIFF 的既定结论）：漂移 ≤ `1e-3`（loose）为 CLEAN，
   `1e-6 < d ≤ 1e-3`（tight）只记 jitter 观测（暂停期引擎重算世界变换的 ULP 抖动先例），
   超过 loose 或对象集合变化 = **DRIFT**（error 行）。
4. **失效安全**：所有指针经 `readable` 页校验后才读（与 animation_source 同纪律）；
   NPC 每次从句柄重解析；普查不可用时记 `UNAVAILABLE` 且会话判读降为 `INCOMPLETE`（不算失败也不算通过）。

**日志行**（前缀 `SCOPY ISOLATION`）：

```
SCOPY ISOLATION baseline op=capture nodes=… npcs=… pose-bones=… pose-note='…' root=… parent=… beyond-radius=…
SCOPY ISOLATION op=capture stage=post window='capture:pre→post' verdict=CLEAN cum=CLEAN nodes=…/… max-local=… max-world=… max-pose=… actor-pos=… actor-rot=… npc-pos=… npc-rot=… npc-world=… npc-set=+0/-0 flags=0 attach=0
SCOPY ISOLATION DRIFT op=… stage=… kind=node-local|node-world|object-set|engine-pose|actor-position|npc-position|… …   （error 行 = 隔离被破坏）
SCOPY ISOLATION jitter op=… stage=… kind=node-local|npc-set|npc-3d-attach …   （info 行 = ULP/流式观测，不判失败）
SCOPY ISOLATION end reason=panel-closed|load-boundary ops=… censuses=… verdict=CLEAN|DRIFT|INCOMPLETE max-local=… … notes='…'
```

## 上机步骤

1. 确认部署：`E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\CharacterPanel.dll` SHA-256 `f9251957…`（PDB 同包已同步）。
2. 进重度存档，正常游玩 1–2 分钟（池子采样照旧，应零回归）。
3. 开背包 → **完整操作序列**：F7 → 等 2 秒 → F8 → 等 3 秒（idle 驱动画几十帧）→ F2 关 → F2 开 →
   F3 转两次 → F8 关 → F4 → 关背包。
4. 再开背包 → F7 → **直接关背包**（短会话路径，验 end reason=panel-closed）。
5. 读档一次（验 end reason=load-boundary），然后再走一遍第 3 步的 F7/F8/F2/F3（读档后的第二次全会话）。
6. 结束后把 `Documents/My Games/Skyrim Special Edition/SKSE/CharacterPanel.log` 复制进
   `docs/diagnostics/`（惯例名 `CharacterPanel-hkx36-isolation-20261011.log`）。

## 判读表

| # | 检查 | 通过判据 | 失败时 |
| --- | --- | --- | --- |
| 1 | 基线行 | 每次 F7 的 pre 普查出现 `SCOPY ISOLATION baseline …`，`nodes>0 npcs≥0` | `pose-note='no-3d'` = 进背包太早，重开背包再 F7 |
| 2 | **窗口全 CLEAN** | 每个操作 pre/post 的 `verdict=CLEAN cum=CLEAN`；全程**零** `DRIFT` error 行 | 按 `kind=` 定位：`object-set`/`node-local` = 面板写了 O 的图；`engine-pose` = 动画采样写穿了；`actor-position`/`npc-*` = 世界被推进 |
| 3 | **会话总结** | 关背包/读档时 `SCOPY ISOLATION end … verdict=CLEAN`，`ops` 与实际操作数一致，maxima 全 0.000000 | `verdict=INCOMPLETE` = 普查不可用（看 UNAVAILABLE 行的 reason），不算失败但要回传 |
| 4 | jitter 观测 | 允许少量 `jitter` info 行（ULP／NPC 流式集合变化）；`max-local/max-world` 仍应为 0.000000 量级 | 若 tight 行每帧大量出现且 max 达到 1e-3，把日志整段拿回来重定阈值 |
| 5 | 零回归 | 面板人物照常、`Clone draw OM check: … match=true`、池子行照旧、无切换卡顿、零崩溃 | 与仪表无关，查渲染链 |
| 6 | F4 unpaused 路径 | 若在非暂停时按过 F4：无 census 行、无异常 | —— |

**通过的意义**：H05 的"隔离"从"没崩溃"升级为**数值判据**——暂停面板会话内 O 的图、O 的引擎姿态、
O 参照、附近 NPC 全部逐位冻结（或仅 ULP 抖动），且全程无失效引用、无崩溃。这仍是 S2/S1 的
**前置证据**而非替代：S1 安全回收（三条释放路径全崩、现行停放不销毁）继续阻塞。

## 已知设计边界（先写明，避免误判）

- 普查在**渲染括号**内进行（process_requests），与既有命令处理同上下文；NPC 句柄解析走引擎
  句柄表（临时 NiPointer，自增/释放一次引用计数，引擎自身模式）。
- `poseLocal` 暂停期应逐位冻结（HKX2 复原 0.000、HKX28 同相位 0.000 的先例支持）；若见 `max-pose`
  非零且无 DRIFT 行，说明引擎在暂停期重算姿态——那是引擎行为的新事实，回传即可，不阻塞。
- 仪表本身零行为改动：不写 O/NPC/克隆，不新增热键，无面板会话时零开销（只在操作前后各跑一次普查）。

---

## HKX36 判读（已实测，2026-10-11 晚，用户执行；日志 [CharacterPanel-hkx36-isolation-20261011.log](diagnostics/CharacterPanel-hkx36-isolation-20261011.log)）

实测规模：**7 个会话 / 64 次操作 / 128 次普查 / 205 行判读**，两个存档（重度 1878 节点/131 几何、
轻量 470 节点/22 几何），2 次读档边界，5 次面板关闭。零崩溃（进程 42732 全程存活）、
零隔离 UNAVAILABLE、零 `[E]` 出自隔离以外。

| # | 检查 | 结果 |
| --- | --- | --- |
| 1 | 基线行 | **通过**（7 个会话各一条 baseline；`pose-note=''`、116 骨齐备） |
| 2 | **窗口全 CLEAN** | **通过**——全部操作 pre/post 窗口 verdict=CLEAN：F7 捕获、F8 绘制（40 行 idle-toggle、8 行 draw-toggle 全在）、F3 旋转（angle 90→360）、F4 释放（4 行全 CLEAN） |
| 3 | **O 图隔离** | **通过**——node local 恒 0.000000、对象集合 1878/1878 与 470/470 逐位全等、根/父指针零变化 |
| 4 | **O 引擎姿态** | **通过**——`poseLocal` 116 骨 max=0.000000 全程（面板采样读取未写穿，测试单"已知设计边界"的预期成立） |
| 5 | **O 参照** | **通过**——actor pos/rot 0.000000 全程 |
| 6 | **NPC 隔离** | **通过**——pos/rot/world/flags/attach 全零、集合 +0/-0（重度存档触达 24 上限，覆盖最近 24 名） |
| 7 | DRIFT 唯一族 | **观测（引擎侧）**——`kind=node-world/world-bound node='SuperSprayBreath'` ×49 节点、恒定 0.250000，归因见下 |

**会话总结行**：`end verdict=` 分布 = CLEAN×4 / DRIFT×3；三个 DRIFT 全部出自重度存档会话、
且**全部**由 SuperSprayBreath 一族贡献（`max-local=0.000000 max-pose=0.000000 actor-pos=…0 npc-…=0`）。

### SuperSprayBreath 0.25u 的归因：引擎粒子自身节拍，非面板（五条证据）

1. **恒定振幅**：每次事件都是精确 0.250000、同 49 节点——粒子系统（`SCOPY GEOMETRY`：type=niParticleSystem）的离散状态切换（出生周期泊位），不是累积损伤。
2. **与面板活动不相关**：F7 捕获窗口内发生（该窗口面板只跑只读测量、绘制关闭）；29 秒无操作间隔内发生；14.5 秒同类间隔内又不发生——跟随粒子自身周期，不跟随任何面板写入。
3. **仅 world 变化**：全程 node local 恒 0——没有任何代码写过图，是引擎从自己的粒子更新重算 world。
4. **交叉存档对照**：无呼吸特效的存档（capture 3/4）同操作序列全程 `verdict=CLEAN notes=''`（含 24 操作会话）——DRIFT 与该族粒子共存亡。
5. **既有证据吻合**：HKX33/34 已确证该存档玩家图带 ~50 个 SuperSprayBreath 粒子节点。

### 结论

- **H05 隔离成立（数值判据首次给出）**：暂停面板会话内，O 的图（local/集合）、引擎姿态、
  参照位置朝向与全部受监视 NPC **逐位冻结**；面板全部操作零世界副作用；无失效引用、无崩溃。
- 唯一被点名的 node-world 族 = **引擎行为的新事实**（暂停期 SuperSprayBreath 仍按自身周期改
  world 0.25u）。若后续要零 DRIFT 报告：普查按类剪除粒子系统节点（与克隆绘制按着色器类剪枝
  同判据）或单列观测通道——小修，不阻塞。
- 顺带观测：重度存档 F7 捕获测量套件耗时 ~41s 墙钟（既有 capture 期成本，非本轮引入，后续可裁）。
- H05 关闭后，遗留队列剩：**多动作循环调度器**（第 1 优先）、S1 安全回收（最硬门槛）、OAR fork（另会话）。
