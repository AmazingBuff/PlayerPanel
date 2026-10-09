# M0 handoff — state, verified facts, and the next work package

> **S2（动作／FR-03）的下一工作包入口：[S2 动画：HKX／引擎动画路线侦察与下一工作包](s2-animation-handoff.md)。**
> 2026-10-09 方向修正：程序化待机无真值、不可判定，用户要求改用真实动画（HKX）驱动并做数值对比；
> 该文记录了引擎侧可取用的 Havok 对象（含头文件行号）、三条取数路线、HKX1 构建步骤与对比协议，
> 以及"不要继续调 idle 参数"等红线。**下一个会话先读它。**

## 场景图复制实验（2026-10-09 复核：静态显示初步成立，资源回收阻塞）

当前需求与验收依据是 [PRD 0.6 §6.2–6.3](player-panel-prd.md)；下一轮使用 [测试单](scene-graph-copy-next-test-2026-10-09.md)。
S3 公开接口接入受限、独立模拟未验证。
**2026-10-09 当前阶段：S2 的程序化待机已实现（IDLE1 构建），待游戏验证。**
四轮探针（S2P1–S2P4）已确证驱动机制：写副本节点 → 引擎在绘制中重算蒙皮矩阵（缓冲 3×3 = 节点世界
旋转 × 0.35 图形缩放，匹配摆动后姿态，残差 0.0002）→ 着色器消费 → 画面随动；F2 开关配对确认
开＝摆动、关＝复位静止（R06）；朝向变化与设计 ±34.4° 相符。
`IdleDriver`（[src/character/idle_driver.h](src/character/idle_driver.h)）按精确关节名驱动胯／脊柱／
头部，自有 `steady_clock`（暂停时引擎计时器不推进），每帧从捕获姿态重算，**F2 开关、默认开启**
（探针退役后 F2 交还待机；探针改为 `-DCHARACTER_PANEL_S2_PROBE=ON` 的诊断构建、占用 F6），
只写副本节点、不碰源角色与行为图。产物 `dist/CharacterPanel-scopy-IDLE2-1.2.1.zip`。
待验证：动作是否看得出来且自然、F2 关是否立刻复位、退出背包后源角色无变化、CBBE／UBE 各一次
（`SCOPY IDLE bound N/M channels` 会体现两套骨架的关节名差异）。
**新边界**：蒙皮缓冲槽位下标**不等于** `bones[]` 下标（自匹配 5/31、4/71），将来要写缓冲必须先解析
真实映射；GPU 侧绑定/内容未测；HKX 播放与可切换展示动作未实现。

用户授权在现有直绘框架上重新验证直接复制已装配的第三人称角色图。
默认构建仍为 Actor 装配；`CHARACTER_PANEL_SCENE_COPY_EXPERIMENT=ON` 的测试构建关闭旧 Actor 路线，
通过 F7 复制／审计、F8 绘制、F3 旋转、F4 释放（F9/F10 已被其他 mod 占用）。没有 HKX／CBPC／SMP 独立驱动，不能将 S0 当作这些需求通过。
先读 [设计与验证问题](scene-graph-copy-validation.md)，逐条结果见
[2026-10-08 回传记录](scene-graph-copy-results-2026-10-08.md)。
下文历史验证结论仍仅对应原 Actor／游离图路线。

**S0 状态**：CBBE 与 UBE 的静态复制／显示、旋转及手动换装刷新有通过观察。
停放构建的有限会话没有再触发已知析构崩溃，但安全释放与资源回收没有通过；Q09／Q10 必须区分显示切换与生命周期。
Q01–Q12 见回传记录；副本内物理头发／衣物不继续模拟
属 S0 既定范围（无独立驱动），不作为失败。
配置**没有** `Release-scopy` 预设（`CMakePresets.json` 只有 `Release`），实际命令是
`cmake -S . -B build-scopy -DCHARACTER_PANEL_SCENE_COPY_EXPERIMENT=ON -DCHARACTER_PANEL_BUILD_PROBE=OFF`
（见 [验证文档](scene-graph-copy-validation.md#构建和传输)）；再构建 `--target CharacterPanelSceneCopyPackage`，
产出 `dist/CharacterPanel-scopy-<轮次>-1.2.1.zip`（示例 `CharacterPanel-scopy-S2P1-1.2.1.zip`）。
本机 `E:\SkyrimAE\mods\CharacterPanel` 的现役 DLL 在验证期间已被换成实验 DLL；
**PDB 也要换成同一个包里的**，否则崩溃日志无法符号化（当前部署目录里的 PDB 早于 DLL）。

**动手前必读的三条结论**（都是这轮拿崩溃换来的）：

1. **三种已测试的释放路径均崩溃，根因未定位**。`capture()` 内就地析构、延迟 4 帧的退役队列、
   读档／主菜单边界释放——三条路全崩，签名一致：`~CharacterClone` → `BSFadeNode` 析构 →
   tbbmalloc `rsi=0`（crashes 23-01-11 / 23-07-35 / 23-12-02）。现行做法 = 停放不销毁
   （`SceneGraphCopy::m_parked`，cap 12 只丢记账条目），实际泄漏没有这个上限。
   C++ 析构体不显式 reset 不能阻止 NiPointer 成员自动析构；停放对象没被析构，与“析构不会释放图”不同。
   不能从三次失败外推“任何时机都不可销毁”，也未证明与 Actor 路线同源。
2. **玩家 3D 图不等于人物**。重度 mod 环境下它挂着法术特效、骨骼驱动的碰撞体
   （3BCA_*/VirtualGround/CollisionStopper）、脚本标记与法术光源，`passes=106` 而身体几何
   从未被世界渲染器画过。`prune_effect_objects()` 剪枝后 `geoms 123→51`、`passes 106→47`，
   人物才完整显示。**判据不是 `rd-null`**：引擎在 `SetupAndDrawPass` 内部惰性建缓冲，
   能正常显示的 CBBE 图同样是 23/23 全 `rd-null`。
3. **源图比较不能用精确浮点相等**。`NiTransform` 含 3×3 旋转矩阵，暂停帧内几个 ULP 的漂移
   会让 CBBE 永远 `BLOCKED`。`SOURCE-DIFF` 只把对象集合变化当失败，变换漂移仅作观测。

**S1 仍阻塞；S2 先补执行链的有效证据**：把“有意泄漏”换成干净销毁需要查清引擎为何在
`BSFadeNode` 析构里踩空指针——三次崩溃现场 `RSP` 上都留着 `BSFlattenedBoneTree "NPC Root [Root]"`、
`NiNode "NPC"`、`NiNode "skeleton_female.nif"`；这是定位线索，不证明全局注册根因。
当前停放只限制记账条目最多 12；未释放的图持续累积，不做长时间压力循环来冒充生命周期通过。
S2 必须先确认基础 pose 后的探针变化能到达真实 draw，再判 GPU 数据链。S2–S4（HKX 驱动、CBPC／FSMP 独立注册、持续换装同步）见
[后续关卡表](scene-graph-copy-validation.md#后续关卡本轮未实现不作为-s0-已通过项)。

- 日期:2026-10-01(run 19 收官:工作包 1+2 完成并经游戏验证)
- 读者:下一个会话的 Agent / 开发者。本文是"从这里继续"的入口;运行级细节在
  [m0-proto.md](m0-proto.md),探针证据在
  [m0-capture-report-2026-09-28.md](m0-capture-report-2026-09-28.md),
  需求基线在 [player-panel-prd.md](player-panel-prd.md)。

## 当前状态：游离图架构验证轮（2026-10-07 晚间会话，历史留档，最新状态见上节）

**架构已换代**：pass-redirect 管线（run 1–105 的重放/合成路线）已退役，提交
`172984a "Replace pass-redirect pipeline with direct clone drawing"` 起，
克隆的 3D 图改为**游离图**——摘出世界场景后**不挂任何引擎场景**（重点：
不再挂 `menuObjects[0]`），由 `CharacterClone` 的 NiPointer 独占持有，
每帧在渲染括号内手动 `GetRenderPasses` + `SetupAndDrawPass` 直绘进私有
离屏目标，合成管线照旧。**三次崩溃（17:20/17:37/18:01）的根因——
`menuObjects[0]` 的引擎重建窗口——在架构上已不存在**；插件对
`menuObjects[0]` 现在只剩审计读取（`Studio host changed` 日志）。

### 游离图架构要点（动手前必读）

- **线程模型**：克隆出生（`CharacterClone` ctor：PlaceObjectAtMe + 镜像
  装备）与摘图/直绘都在**渲染括号**（`RenderHook` 的 DrawInterface 钩子，
  与世界渲染 job 序列化）。**SKSE 任务队列已退役**——实测它 flush 在
  独立 worker 线程（日志线程号 18840，非主线程/渲染括号），引用创建在
  worker 线程是 18:01 崩溃的土壤。`clear_clones` 同步跑在主线程消息里。
- **摘图判据**（`detach_graph` 阶段机，全部是"必然事件"或附加层信号，
  不可逆序）：
  1. `facegen_attached`：BSFaceGenNiNode 树附加完成（引擎异步）；
  2. `rendered=true`：`numMatrices>0`——引擎至少渲染过克隆一次
     （SetupGeometry 写入，boneMatrices 缓冲已初始化）。**fade=0 会被
     引擎渲染器跳过 → numMatrices 永远 0**，故 fade 守卫压 **0.05**
     （近隐形但被渲染）；两者都有 1200 帧超时兜底（warn 后照摘）。
  3. 穿衣（mirror_worn_equipment）在摘图**前**（壳存活期，m0 顺序）；
  4. **全绑定判据**：`bound_nodes ≥ alloc_slots`——注意基准是
     `min(bone_count, allocatedSize)`（引擎按需分配骨骼指针数组，3BA 实测
     47 槽 vs skinData 声明 131），不是声明容量；
  5. 摘图三步：`data3D=null` 先斩（run 96 拉锯机制）→
     `old_parent->DetachChild`（world cell room，壳活着时必活）→ **不挂宿主**。
- **活壳**：摘图后壳 **Disable 但不 SetDelete**——`SetDelete` 触发
  biped 拆解，会把 worn-armor 蒙皮从骨架树摘走（23:48–50 的
  "只剩头+手"：geoms 12→8）。Disabled + data3D 斩 = 引擎原生维护状态
  （无重装配、无 tick、不可见）。壳引用随克隆持有到下次读档边界。
- **灯光 rig 也游离**：`StudioLight` 的 NiLight 树不挂任何场景
  （`init` 只 AddLight 入 ShadowSceneNode 账本），`place(anchor)` 在
  绘制窗口内手动级联（着色读 `NiLight::world.translate`，v6.37），
  `park()` 在**每个** draw 出口把 rig 泊回 Z+100000（账本壳在游戏空间
  附近会被引擎画进世界 = 漏光，v6.58 机制）。**壳 fetch 扫双队列**
  （activeLights + lightQueueAdd）——游离 rig 无场景 tick，壳永不晋升，
  直接从 lightQueueAdd 取（v6.50 实证）。
- **灯光注入**：per-pass mutate→draw→restore（`submit_pass`），
  槽 0=环境、1/2=key/fill（v6.56 约定），窗口内每 pass 补壳
  （lodDimmer/luminance/frustrumCull）。注入仅在三槽都被账本/队列
  服务时启用。`SetupAndDrawPass` 直调 CLib 绑定（CS 共存待观察，
  run-11 症状预案 = pre-patch 目标解析）。

### 本会话崩溃/缺陷判读链（全部留档）

1. crash-17-20-42 / 17-37-26：`menuObjects[0]` 悬空 root，303 行
   `host->AttachChild` 虚调用跳 0x000100000000（反汇编+CLib 布局确认）。
2. crash-18-01-37：AddTask 版死在任务对象析构——lambda 按值捕获的
   NiPointer 副本对死对象 Release→DeleteThis 虚调用读垃圾（RIP=0）。
   同根：死指针上 IncRef 无效。→ 游离图 + 撤 AddTask 双修。
3. "0 bind bones"四轮排除（深度/原语/地下遮挡/渲染绑定）→ 真因：
   **T-pose 重推导门自己死锁**（绑定发生在摘图后的 SetupAndDrawPass，
   门把图挡在管线外）+ **numMatrices 语义误读**（它是渲染活动指标，
   摘图前恒 0 是正常的）→ 门降级为观测。
4. UBE 头身分离：**facegen 树挂接差异**——CBBE facegen 树挂 Head 骨骼下
   （跟随 T-pose），UBE 挂骨架 root（bind 是骨架绝对坐标，facegen 组
   209 骨骼含 ~207 根 hdtSSE 头发物理骨骼）——T-pose 重推导错误重置
   物理骨骼与 SMP 拉锯 → 修法 = **facegen 组跳过** + **头/颈骨骼豁免**
   （`[Head]`/`Neck` 名字过滤）。
5. "只剩头+手"（跨读档随机）：`SetDelete` 的 biped 拆解摘走 worn-armor
   蒙皮（geoms 12→8 实锤）→ **活壳修法**（撤 SetDelete 保 Disable）。
6. 漏光：park 缺失（place 后不泊回）→ 每 draw 出口 `park()`。
7. 第一次进存档无人像：fade=0 饿死渲染门 → fade 0.05。

## 当前状态：游离图架构验证收官（2026-10-08 会话，落定门全判据通过）

**活壳 + 落定门验证全部通过（用户确认"以上所有问题都没问题了"）**：
落定门（v1.2.1，MD5 28303210）后连续读档验证——面板人物完整着装、
`detached` 行 geoms 与 dressed 一致不再掉、UBE 头身分离消失（此前判读
确被 armor 摘除污染）、漏光保持已修、零崩溃零回归。**"只剩头+手"问题
正式关闭**（根因链：穿衣摘旧几何 + 护甲异步装载 vs 摘图过早 → 落定门
解决；SetDelete 判读为同窗误判，活壳 Disable 本身无罪）。

**forwarder 待修项关闭（误报）**：CLib `RE/R/Renderer.h:95` 中
`forwarder` 就声明为 `ID3D11Device*`（offset 0x50），CS 仓库同款用法
（`Globals.cpp:228` reinterpret_cast）；昨晚日志 `All shaders compiled`
+ 合成全程正常证明运行时成立。撤下该"已知待修"。

**当前无待验证代码。** 下一阶段 = 阶段 3（M1，PRD FR-01/02/03）：
①装备与外观同步（出生时一次性镜像 → 持续同步，Apparel Preview 装配
逻辑为参考，community-reference-supplement.md §4）；②临时污染过滤
（FR-02）；③待机动画与展示动作（FR-03，含头部姿态冻结的取舍——头/颈
骨骼豁免已落，头姿态=出生瞬间引擎驱动的姿态）；④面板布局定稿
（**需要用户输入**：P 的位置/大小/与物品卡及后续网格背包的关系）。
遗留：正式性能测量（PRD §5.3）、灯光与世界光照一致性微调。

## 上一轮：几何掉落根因改判（2026-10-08 会话，已关闭，历史留档）

**"只剩头+手"根因改判：不是 SetDelete，是穿衣本身**。活壳修法
（MD5 972263fa）下的验证轮（23:54–57，日志已存档
`log-archive/CharacterPanel-20261007-2357.log`）：三轮克隆**全部**
`dressed`(geoms=12, num-matrices=118, bound=118) 后 **14 ms 内**
`detached`(geoms=8, num-matrices=49)——裸体图当时已 118/118 全绑定，
阶段 3 门立即放行；掉的是被替换的 4 个裸体部位几何（biped 换装在
EquipObject 当帧即刻摘除旧几何），而护甲 nif 是**异步**装载、几帧后
才挂上——data3D 已斩、图已摘出世界，护甲永远进不了图。23:48 截图
（脸+手发光、身体全黑）= 幸存的 8 个几何里脸/手无护甲覆盖 + 无绑定
部位退化为恒等蒙皮。22:22 那轮全链路正常只是竞态赢了（护甲恰好先
挂上）。**SetDelete 判读的修正**：23:48–50 的 geoms 12→8 与 SetDelete
时间上同窗，但机制是穿衣摘旧几何；活壳 Disable 本身无罪（本轮 Disable
后 geoms 保持 8 不再掉）。

**修复（v1.2.1，MD5 28303210，已验证通过）**：阶段 3
门升级为"全绑定 + 落定"双门——①几何 census（geoms/skinned）连续
`Clone_Settle_Stable_Frames`(20) 帧不变才允许摘图，任何几何变化
（护甲挂上）重置稳定计数并打 `Clone dressing geometry change` 行；
②穿衣后最短等待 `Clone_Settle_Wait_Frames`(30) 帧；③600 帧上限兜底
（超限 warn 后照摘）；④摘图时一次性打 `Clone geometry census (N):`
几何名清单（判读一锤定音：幸存了哪些 12、挂上了哪些护甲）。

### 已验证 / 待验证

- **已验证（全绿，无待验证项）**：落定门全判据（完整着装/geoms 不掉/
  UBE 分离消失/漏光已修/零崩溃）；活壳修法（Disable 后 geoms 稳定）；
  CBBE 裸体全链路（118/118 全绑定）；facegen 门；灯光双队列 fetch
  自愈；宿主审计日志（读档不重建 root，主菜单转换重建——runs
  100-102 补充数据）。
- **FR-03 范畴**：头部姿态冻结（头/颈豁免的取舍——头姿态=出生瞬间
  引擎驱动的姿态）。
- **流程**：`CharacterPanel.log` 非追加模式，**每次测试后立刻复制**
  （已建立 `log-archive/` 惯例，本会话已存档一份）。

### 部署与构建

- 当前 DLL：MD5 `28303210`（v1.2.1，`build/Release/CharacterPanel.dll` →
  `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\`，PDB 同步部署）。
- 构建：`cmake --build build --config Release --target CharacterPanel`。
- 出生原语：**PlaceObjectAtMe**（玩家处出生；CreateReferenceAtLocation
  产出无绑定蒙皮——run 104 的"深度选 LOD"判读已修正为"原语差异"）。

- 反汇编记录：crash 17-20-42 的 303 行虚调用（vfunc 0x35=AttachChild）、
  18-01-37 的 `_Delete_this`（析构捕获的 NiPointer）、skse64+0x189E8 的
  Run/Dispose 循环。
- m0 档案参考：`tools/m0_proto/proto_passredirect.cpp`（per-pass 灯光注入
  :2199-2248、双队列 fetch :1608-1626、泊位 :1879-1900）、
  `proto_pinstance.cpp`（relocate 顺序 :936-1040、facegen 组结构）。
- 本会话对话记录含每次判读的完整证据链（census 数字表、反汇编片段）。

## 当前状态：M0 gate 已通过（历史，2026-10-01）

pass 重定向路线的核心命题已由 16 轮运行证实:**菜单场景的 pass 可以被识别、
拦截,并在原调用之后 1:1 重放进私有离屏目标,着色正确、物品内部遮挡正确、
对游戏画面零破坏**(run 9–16,完整证伪链见 m0-proto.md)。
accumulator 交换路线(旧路线)已在 run 1–8 被证伪并搁置,勿再回头。

下一阶段目标是 **M0 面板原型**:把 F6 单帧证据工程化为常驻的摄影棚渲染,
按 PRD 第 2 节合成顺序接入,首个验收场景是 **A01**。

**v4.6 经 run 26 验证:面板首次上屏,工作包 3(合成)完成**(2026-10-01,
用户 RenderDoc 定位 blend 问题后修复;截图与日志见 m0-proto.md run 26
段):不透明矩形精确落在 58–88% × 12–68%,摄影棚画面(铁盾)在其中,世界
正常渲染;`composite draw #3600` 心跳证明整会话逐帧绘制无崩溃;3 张关面板
取证 TGA;引擎混合诊断行 `enable=0 src=2 dest=1` 留档。**M0 面板原型四
大件全部就位**:常驻括号、常驻摄影棚目标、生命周期(FR-06)、屏幕合成。
遗留(M1 打磨,非阻塞):面板目前盖住物品卡右半(卡片先画、面板后画,
§2 顺序的精确排布待做);摄影棚图整幅缩放进矩形(取景属 M1 相机工作)。
**范围修正(2026-10-01,用户决定)**:面板仅在菜单界面显示,世界画面
对照推迟——"下一步 = A01"作废,新规划见下方"下一步规划"三阶段。
**阶段 1 工程面验收通过**(逐项证据见 [m0-acceptance.md](m0-acceptance.md):
7 项中 6 项通过、1 项部分通过,开放项并入阶段 2 首轮;INI 配置用户决定
推迟);**当前阶段 = 3(M1)待启动;阶段 2/2b 均收官**。
DLL 已部署 `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\`
(v4.6,MD5 6f00f395…)。热键 F7/F8。v3.1 已提交(770e618);v4 系列
已提交(f15fb1a)。

**阶段 2b 完成(2026-10-04,"摄影棚即居所"工作包收官,run 94–103,
v6.60–v6.68)**:P 的 3D 图常驻菜单场景私有节点 CP_StudioHome
(menuObjects[0] 下、NiPointer 强持有),壳 actor 迁移同 tick 删除
(data3D 先斩,run 51 崩溃机制构造性失效),ghost 层/泊位争霸/簿记
争抢全部退役,世界侧结构性零克隆,FR-06 成立。teardown(读档/主菜
单)双路径验证干净。**跨会话转换防线(v6.66–v6.68,runs 100–103)
也闭环**:切角色(A 存档→主菜单→B 存档)曾击穿灯光生命周期——
账本 free 灯壳(use-after-free,栈实锤)+ rig 被剥离收集路径(指针
全对但 0/3,指针比对路线证伪);最终形态 = 壳指针每关面板清空 +
节点/宿主变更检测 + **fetch 停滞 30 窗自愈重建**(对任何失联形态通
用,~0.5 s 恢复)。过程中另证伪:census 仪器(run 54/94)、世界流门
控(run 95)、"世界先渲染"前提(U1,run 95)、**出生即泊位(v6.70,
run 104:引擎按引用距离在装载时选角色 LOD,出生深度装配出零绑骨的
低精度图)**。**run 105 已验证 v6.70:面板人物完整着装 + 进游戏无可见
克隆体,flash 问题正式收官;当前无待验证代码,最新 DLL v6.70(MD5
aa58ac99…)**。下一阶段 = 3(M1:装
备同步 FR-01、污染过滤 FR-02、待机动画 FR-03、面板布局定稿——布局
需用户输入);遗留:正式性能测量、灯光与世界光照一致性微调。判读
全史见 stage2b-studio-home-plan.md §H1–§H11。DLL 已部署
`E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\`。热键 F7/F8。

**阶段 2 运行史(2026-10-02,run 27–33 + v5.7 build 起)**:
- 证伪/修复链:run 27 深拷贝(F5)→ run 28 假帧 bug(SKSE 任务队列同
  帧排空)→ run 29/30 摘图路线(菜单剔除器不收外来图,摘图后零剔除链
  到达,pass 根本不存在)→ **run 31 route 3 首轮:P 保世界 actor + 白
  名单重放 + 透传抑制,面板第一次显示人形**——蒙皮过重放路径的核心命
  题初步成立(用户截图 + TGA proto-pass-150 双证:头/躯干/手臂轮廓)。
- **Run 32(v5.5 会话,用户 RenderDoc 双观察)**:①背包内零 P——
  p_passes 只在面板打开瞬间的世界流帧爆发(69/66),之后菜单帧全零:
  **菜单打开 → 暂停 → 世界渲染器停帧 → 世界 pass 流消失**,P 无处发
  pass;②残影——清屏只在首帧,重放逐帧叠画;③深度事实:P 的 type 6
  只有头发(0Anto92/HAIRLINE/Brows),身体(CBBE/clothes)是 Effect
  shader(type 8)+ 3BA 体,纯 BSLighting 过滤只剩头发。
- **Run 33(v5.6 会话,用户 RenderDoc 判定)**:快照在跑(snapshot
  started 8 次、每帧 lighting_replayed=1)但**画进不可见目标**——
  end_frame 裸调 call_site_original 无 OM 绑定;run 31 能看到人形正因
  走了 replay_after_original 的完整 OM 绑定窗口。CLib 调研确认:①蒙
  皮矩阵 frameID 机制每渲染帧重算(暂停冻结姿势不冻结重算),逐帧快
  照重放安全;②背包物品 pass 每显示帧经
  InventoryMenu::PostDisplay → Inventory3DManager::Begin3D/Render/
  End3D → UI3D accumulator → BSBatchRenderer::SetupAndDrawPass
  (100854/107644),site 每帧触发、OM 窗口每帧存在。
- **v5.7(2026-10-02 02:05 DLL,待游戏验证)**:快照重放迁入
  `replay_after_original` 内部 OM 窗口(本 pass 重放完成、摄影棚绑定
  仍生效处);pass 身份校验(geometry 非空 + is_p_geometry 仍真,失效
  条目惰性淘汰);计入取证闸门;首次重放打
  `P snapshot replayed in the studio OM window: N passes`。
- **Run 34(v5.7 会话 02:11 + 日志)**:OM 窗口修复在世界帧内成立
  (`snapshot replayed… 64 passes` + composite draw #1),但**背包内
  arm 零 P pass、快照空**——进背包即暂停,世界流在 F7 之前已死,捕获
  供给端在目标场景天然为零(结构死结)。用户路线决策:**主动构造绘制**。
- **v5.8(2026-10-02 02:29 DLL)= 主动构造绘制**:OM 窗口内遍历 P 蒙
  皮几何 → 直调 `BSShaderProperty::GetRenderPasses(geom, kNormal,
  UI3D::unk10 accumulator)`(引擎原生 pass 生成,自选
  technique/灯光/MakeRenderPass)→ 返回链逐条
  `call_site_original(pass, passEnum, passEnum&0x40, 0x200)`;生成 pass
  属 property 一帧对象不手动释放;世界流透传抑制保留;快照机制移除。
- **Run 35(v5.8 会话,用户 RenderDoc)**:**draw call 真实发生(pass
  生成链路全通)但零像素落进面板**——P 顶点在玩家下方 8k 的世界坐标,
  经 UI3D 相机投影后视锥外,GPU 执行 draw 但零光栅化(run 31 世界流
  能见人形正因当时用世界相机);且每物品 pass 重复绘制一次(无帧闩)。
- **Run 36(v5.9 会话,用户 RenderDoc VS 截图)**:VS Input 人体局部坐
  标正常(蒙皮正确),**VS Output SV_Position ≈ (-40386,3221,-1418)**——
  物品锚点是世界坐标,UI3D 相机 worldToCam 无世界偏移(translate 在原
  点),P 在视锥外零光栅化。**用户新验收**:面板不依赖高亮物品,面板
  内不显示高亮物品。
- **v6.0(2026-10-02 03:02 DLL)= 相机系停靠 + 面板只显示
  P**:每帧 `SetPosition(camera->world.translate + view*40)` +
  `SetHeading` 面向相机(NiCamera 局部 +Y=视线,world rotate 列 2);
  脱离 menuObjects[1]——无高亮物品也可开面板;**物品 pass 不再重放进
  摄影棚**(`replay_after_original` 只作 OM 窗口触发器,窗口内仅
  `draw_p_proactively` 帧闩一次),面板内容 = 纯 P,物品在原位正常预
  览;`Park_Depth`/`item_anchor`/`m_has_anchor` 退役。
- **Run 37(crash-2026-10-02-03-05-37 判读)**:载存档 crash,但**面板
  从未打开**(日志仅安装行,零 clone placed)——v6.0 新路径一行未跑;
  crash 签名 = HUDMenu 消息链读已释放 HUDData(r14 未对齐,+0x38 =
  quest/wordOfPower 槽),**po3_FloatingSubtitles 与 FasterLoadscreens
  在同一栈**;9 月历史 crash 全为不同签名。**定性:环境性(读档 × HUD
  消息竞态,第三方钩子在链),与本插件无代码路径因果**。
- **v6.1(2026-10-02 03:16 DLL)= 防御加固**:kPreLoadGame/
  kNewGame 在 force-close 后追加 `PInstance::despawn()`(读档边界杀
  克隆,不留半建状态);安装横幅 v6。
- **Run 38(v6.1 会话,用户 RenderDoc VS 截图)**:`P drawn proactively:
  8 passes`(pass 生成链路全通)但 **SV_Position 仍 ≈ 世界洞窟坐标**——
  actor 级 `SetPosition(camera)` 被引擎更新链覆盖(spike 时代有效的前提
  是克隆已 Disable,本路线克隆是活的)。**结论:actor 位置不可作姿势权
  威**。
- **v6.2(2026-10-02 03:31 DLL,待游戏验证)= 绘制时直摆根变换**:
  `PInstance::pose_for_studio()` 在 OM 窗口内、pass 生成前,直接设 P 根
  的 local transform(父级为世界 cell 根,local=世界目标):
  `camera->world.translate + view*40`,Rz 面向相机,`Update()` 级联——
  SetupGeometry 的蒙皮矩阵重算(帧号机制)读到的就是相机帧内骨骼
  world;actor 级停靠全面退役,姿势唯一权威 = 绘制窗口。
- **Run 39(v6.2 会话,用户 RenderDoc VS 截图)**:**SV_Position 巨幅改
  善**(-40386 → (353,1087,-20),根变换生效、顶点入相机邻域)但仍视锥
  外——v6.2 对 NiCamera 视线方向(局部 +Y / world rotate 列)的语义假
  设与实际不符。**可靠参照系只有物品预览**:manager 把它摆在 UI3D 相
  机正确投影的空间里。
- **v6.3(2026-10-02 03:39 DLL,待游戏验证)= 物品参照系标定**:
  `pose_for_studio` 锚点 = menuObjects[1] 首物品几何 world.translate、
  缩放 = world.scale×0.1、朝向 Rz(180°);无物品回退原点。**不恢复"
  面板依赖物品"**——物品 pass 仍不进摄影棚(面板纯 P),物品只是标定
  参照。
- **Run 40(v6.3 会话,用户 RenderDoc VS 截图 + TGA 判读)**:
  **SV_Position = (107.7,137.5,484.7),NDC x/w≈0.22 y/w≈0.28 双入界**
  ——物品参照系标定成功,顶点投影正确必然光栅化;**但 TGA 213 全帧零
  内容、alpha 恒 255**——像素未达摄影棚目标。归因收窄:`SetupAndDrawPass`
  内部 batch 逻辑按 pass 的 accumulationHint 自行重绑 OM 对。
- **v6.4(2026-10-02 13:09 DLL,待游戏验证)= 每 pass 前重绑 OM**:
  逐 pass 画,每个 pass 前重设 studio OM 对 + 深度状态;批量后检测绑定
  (日志 `binding after SetupAndDrawPass batch: held/stolen`);若 held
  而 TGA 仍黑 → 下怀疑对象是 pass 状态丢失(证据已备)。
- **Run 41(v6.4 会话,用户 RenderDoc 完整 SV 四列)**:
  **SV=(110.7,144.4,486.4,501.0),NDC 全入界,投影完全正确**;用户双嫌
  疑(z≈远平面/无混合)深挖:z/w=0.971 ≈ 洞窟→原点视距——**Update(kDirty)
  级联被 selective-update 标志短路,蒙皮骨骼 world 未随根迁移**;TGA
  全黑+alpha 满与未知混合因子压黑 RGB 相容。
- **v6.5(2026-10-02 13:31 DLL,待游戏验证)= 强制级联 + 混合压制**:
  `pose_for_studio` 改 `UpdateDownwardPass` 强制整树级联(骨骼 world 终
  于随根);主动绘制加 OpaqueGuard(blend disable 全写,完事恢复);
  v6.4 的逐 pass OM 重绑与漂移检测保留。
- **Run 42(v6.5 会话,用户 RenderDoc rasterizer 状态)**:主动绘制时的
  **rasterizer state 携带 depthBiasClamp=-100**(物品 call site 遗
  留)——贴远平面深度的像素被 bias 整块推出深度范围,光栅化全灭;与
  SV 入锥/TGA 全黑/draw 在跑三证自洽。OM 窗口此前从未管控 rasterizer。
- **v6.6(2026-10-02 13:40 DLL,待游戏验证)= RasterGuard**:主动绘制
  期间强制干净 rasterizer(无 bias/无 scissor/CULL_NONE),完事恢复。
  现有防线:逐 pass OM 重绑 + 不透明混合 + 强制级联 + 干净 rasterizer。
- **Run 43(v6.6 会话)**:四道状态防线仍无人形;用户判断坐标太远,指示
  改近平面。
- **v6.7(2026-10-02 13:52 DLL,待游戏验证)= 近平面停靠 + 深度实测**:
  `pose_for_studio` 实测锚点沿视线深度(日志 `anchor depth along the
  studio view: N (near=15)`),不在 [20,200] 时 P 放视轴 near+5 处——
  移除锚点深度未知变量;其余防线不变。
- **Run 43(v6.7 会话)**:锚点深度实测 **17.3(本就贴近平面)**,near+5
  停靠生效;但 **P 只画 arm 帧 1 次,之后 74 括号帧零绘制零 menu pass**
  ——背包暂停时 Inventory3DManager 不重画同一物品(loadedModels 常驻),
  物品 pass 不来 → replay 窗口不开(用户 RenderDoc"零人物 draw call"
  吻合)。
- **v6.8(2026-10-02 14:14 DLL,待游戏验证)= end_frame 直驱**:
  `draw_p_proactively` 自含 OM 窗口(自绑 studio RTV/DSV/深度/视口 →
  清屏 → 摆姿 → 逐 pass 画(每 pass 前重绑)→ 全恢复),`end_frame` 每
  括号帧直调——**与引擎 pass 彻底解耦**;物品 pass 窗口路径保留为冗余
  (帧闩防重);近平面停靠/强制级联/不透明混合/干净 rasterizer/绑定检
  测全保留。
- **Run 44(v6.8 会话,用户 RenderDoc VS 截图)**:**draw call 每帧出现
  (end_frame 直驱成功)**;但 **SV w=-0.47(负)= 顶点在相机后方**——
  NiCamera 沿局部 **-Y** 观察,v6.2 起的"+Y=视线"假设方向反了(z=-15.5
  恰为 near 量级,P 在背后 20 处)。
- **v6.9(2026-10-02 14:22 DLL,待游戏验证)= 视线取反**:view = world
  rotate 第 3 列取负;其余全部保留(end_frame 直驱/近平面停靠/强制级联/
  OM 重绑/不透明混合/干净 rasterizer)。
- **Run 45(v6.9 会话,用户 RenderDoc)**:取反后 **w 仍负、yz 不变、仅 x
  镜像**——world.rotate 第 3 列 ± 都不是视线轴(两次实验证伪列假设)。
- **v6.10(2026-10-02 14:32 DLL,待游戏验证)= 从 worldToCam 正推**:
  视线 = worldToCam 旋转块第 3 行(行主序),眼点 = -R^T×平移列;锚点深
  度 = dot(anchor−eye, view),不在 [20,200] → P 放 eye+view×20。深度
  日志换 v6.10 标签,与 run 43 的 17.3 对照即验证新轴。
- **Run 46(v6.10 会话)**:**74 次 depth 日志 = 主动绘制 74 次都在跑**
  (pose_for_studio 在 draw 内),但 `GetRenderPasses` 拿到**空链**静默
  早退——**暂停后引擎连显式调用都不再生成 pass**(与 run 43 同源);
  另:worldToCam 反推的眼点在 arm 帧(485.1)与后续(-15.0)漂移,平移
  分量约定留待标定。
- **v6.11(2026-10-02 14:49 DLL,待游戏验证)= pass 配方缓存 + 重建**:
  活动生成成功时记录配方{shader/property/geometry/technique/numLights/
  sceneLights}(全键去重,面板期存活),生成空转的帧用
  `BSShader::MakeRenderPass`(ID 107497)重建 pass 再画;其余防线不变。
- **Run 47(v6.11 会话)**:两次 arm 帧成功画(62/37 passes,held);
  **第二次开面板后每帧 49 个 P pass 从世界流到 thunk,白名单/抑制/重放
  链全活**;但 RenderDoc 零 draw——**旧 `P drawn` 日志被永久闩掩盖真实
  路径**(`m_p_draw_logged` 只记会话首次),747 次 depth 日志证明
  draw_p_proactively 每帧进入,但走哪条出口不可见。
- **v6.12(2026-10-02 15:36 DLL,诊断轮)= 每帧绘制摘要**:
  `P draw: source=live/recipes-failed/empty passes=N` 每帧一戳 +
  `empty-live` 首次单独记录——先拿数据再定下一修(若 source=recipes 且
  passes>0 而 RenderDoc 仍零 draw → MakeRenderPass 重建 pass 状态问题,
  下一层)。
- **Run 48(v6.12 诊断轮)**:**每帧 `P draw: passes=8~15`,共 1040 次**
  ——绘制管线每帧执行;但活动期 TGA 232-234 整帧零内容。**结论:
  SetupAndDrawPass(107644)内部拒画**——批处理按 accumulationHint/
  当前 accumulator 分桶,带外 pass 被内部吞掉。
- **v6.13(2026-10-02 16:10 DLL,run 49 已验证——拒画)= 手绘绕行**:不再调
  SetupAndDrawPass;`SetupTechnique→SetupGeometry→IA 绑
  rendererData(VB/IB)→DrawIndexed→RestoreGeometry`(BSGraphics::
  TriShape 布局 CLib 全可得);配方缓存/近平面停靠/强制级联/OM 重绑/
  不透明混合/干净 rasterizer 全保留。标定项:stride 0x30 蒙皮假设。
- **Run 49(v6.13 会话 19:59 + 日志判读 + 用户 RenderDoc)**:手绘循环
  **每帧 `drawn=0 of 10~14`(749 帧,零 DrawIndexed,RenderDoc 零人物
  draw 互证)**——pass 收集正常(live 10~14 个/帧),但循环内四道静默门
  (rendererData 空/vertexBuffer 空/SetupTechnique 返假/indexBuffer 空)
  之一拒画全部 pass。CS 交叉证据:①`SetupTechnique` 返假是真实路径
  (其 BeginTechnique 钩子只对非 Effect shader 兜底,而 P 身体是
  Effect);②手绘还需补 shadow-state 刷洗(SetDirtyStates 75580/77386)。
- **v6.14(2026-10-02 20:31 DLL,run 50 已验证——rd=9 元凶点名)= 门诊断 +
  状态刷洗**:四门逐 pass 计数进帧摘要(`gates: rd= vb= technique=
  ib=`),每门每面板开启首例详情行(geom/shaderType/passEnum/skinned);
  stride 改 `vertexDesc.GetSize()`(0x30 假设退役);DrawIndexed 前补
  vertexDesc/topology 记账 + SetDirtyStates 刷洗(CS 手绘路径同款)。
  判读表见 [stage2-p-instance-plan.md](stage2-p-instance-plan.md) §0w。
- **Run 50(v6.14 会话 20:35 + 日志判读)**:门诊断点名——**`gates:
  rd=9 vb=0 technique=0 ib=0`,全部 pass 死于 `rendererData=null`**,手
  绘链本身无罪;机制 = F7 在暂停的背包里按下,构建全程暂停帧,世界渲
  染器从未画过 P,设备端 VB/IB 从未创建(与 v6.14 新增的两张关面板
  TGA 235/236 全黑一致)。
- **v6.15(2026-10-02 20:49 DLL,run 51 已验证——自动出生通但 Disable
  崩溃)= 未暂停世界帧渲染初始化**:①kDataLoaded/kPostLoadGame 置
  ready,pump 空闲帧
  自动 spawn(游戏线程校验未暂停 + 玩家 3D 存在);②tick 暂停闸——构
  建只在未暂停帧推进;③attach 时统计 `renderer init check: N/M` 后
  `Disable()` 停泊(设备缓冲持久,绘制时摆姿照常);④面板关闭不再
  despawn,实例保留全会话,despawn 只挂读档/新游戏。判读表见
  [stage2-p-instance-plan.md](stage2-p-instance-plan.md) §0x。
- **Run 51(v6.15 会话 20:59 + crash-2026-10-02-20-59-23 判读)**:自动
  出生链路全通(未暂停 1.4 s 建成),但 **`renderer init check: 0/14`**
  且 **F7 即崩**——v6.15 的 attach 时 `Disable()` 摧毁了克隆 3D 图,
  白名单根悬空,pose_for_studio:169 空虚表调用(RIP=0)。**引擎事实
  升级:禁用 actor = 销毁其 3D;且 75 帧未暂停渲染后设备缓冲仍为 0**。
- **v6.16(2026-10-02 21:0x DLL,run 52 已验证——崩溃消除但仍零绘制)=
  撤 Disable + 初始化心跳**:克隆保持 enabled 存活;tick 的 kAttached 分支
  每 60 活动帧打 `P init heartbeat: P=a/b player=c/d`(玩家图为对照
  组),满员即停;手绘零改动,rd 门随缓冲出现自动放行。判读见
  [stage2-p-instance-plan.md](stage2-p-instance-plan.md) §0y。
- **Run 52(v6.16 会话 + 用户判读)**:崩溃消除,但仍零人物绘制。**用
  户指路:"回到 10 轮之前——run 35–42 那时实际上可以绘制人物,只是不在
  视锥内"**。统一解释收口:run 35–42 的人物 pass 是 live 生成 + 引擎
  SetupAndDrawPass 绘制(设备缓冲由引擎侧惰性创建);v6.13 的手绘 rd 门
  把 pass 挡在 SetupAndDrawPass 之外 = 拆掉了唯一能初始化几何的调用者,
  rd=9 自锁(run 48 的"内部拒画"只适用于 recipe 重建 pass)。
- **v6.17(2026-10-02 21:2x DLL,run 53 已验证——深度成形,链路复活)=
  恢复 run-35 绘制链**:手绘路径退役,每 pass 重绑 studio OM 后直调
  `call_site_original(1, pass, passEnum, passEnum&0x40, 0x200)`(引擎
  SetupAndDrawPass,site 1 = 物品预览同款 CS 互插链);近平面停靠/强制
  级联/不透明混合/干净 rasterizer/OM 重绑/配方兜底全保留;心跳保留作
  "惰性初始化假设"探针。判读见 [stage2-p-instance-plan.md]
  (stage2-p-instance-plan.md) §0z。
- **Run 53(v6.17 会话,用户 RenderDoc RT0+DS 截图)**:**人物在深度
  图完整成形**——v6.17 链路恢复成功(几何/缓冲/蒙皮/投影/光栅化全
  通,"SetupAndDrawPass 惰性初始化"假设闭环);RT 近黑有极暗剪影 =
  **P 的 pass 带地牢世界光,顶点在菜单空间,光照项塌缩只剩环境项**。
- **v6.18(2026-10-02 21:3x DLL,run 54 已验证——爆发期有画但稳态断供
  + 退背包崩溃)= 摄影棚灯光 + 锚点摆姿**:①on_pass 记录物品预览菜单
  光,主动绘制前覆盖 P pass 的 numLights/sceneLights/numShadowLights
  (v6.19 起改为画后还原);②摆姿回归物品锚点位(v6.3 实证,run 40
  NDC 双入界),删除眼点反推/近平面停靠(run 46 漂移 bug 弃用)。判读
  见 [stage2-p-instance-plan.md](stage2-p-instance-plan.md) §0aa。
- **Run 54(v6.18 会话 + crash-2026-10-02-22-00-30 判读)**:爆发期确实
  画了(P pass 66/帧,`submitted=14+14+1`),但稳态无声断供、第二次开
  面板时 P 无声回到 kNone(未知静默路径),退背包崩溃 = **引擎画被登
  记进 UI3D 累加器的 P pass,踩到 v6.18 未还原的光源改写**。另:心跳
  `player=0/23` 证伪 init 探针(玩家图也是 0 但显然在渲染)——心跳退
  役,缓冲真相以 RenderDoc/TGA 为准。
- **v6.19(2026-10-02 22:1x DLL,run 55 已验证——人物像成立)= 引擎侧
  全抑制 + 灯光画后还原 + 静默 kNone 日志**:①P pass 世界与菜单帧都
  抑制引擎透传(摄影棚唯一渲染者,清退背包崩溃 + UI 双影);②灯光
  mutate→call→restore;③tick 的 clone-null 路径与无根早退加日志。
  判读见 [stage2-p-instance-plan.md](stage2-p-instance-plan.md) §0ab。
- **Run 55(v6.19 会话,用户 RenderDoc VS 截图 + 目视)**:**RenderTarget
  上人物像成立(带光照)——M0 蒙皮渲染命题闭环**(几何/缓冲/蒙皮/投
  影/光栅化/光照全链路贯通)。剩取景:人物在相机深度 ~485、scale=item
  ×0.1 → 过远过小。
- **v6.20(2026-10-02 22:2x DLL,run 56 已验证——成型带光照、稳态连续绘
  制,剩取景偏移 + 灯光崩溃)= 取景标定轮 1**:0.1 猜值升级为常量
  `Studio_Figure_Scale = 1.0`(10 倍);
  pose 每面板前 3 帧输出标定转储(anchor/item_scale/root_scale/
  world_bound)——配合下一轮 NDC 读数解析精确倍率。判读见
  [stage2-p-instance-plan.md](stage2-p-instance-plan.md) §0ac。
- **Run 56(v6.20 会话,用户截图 + crash-2026-10-02-22-24-41)**:**人物
  像成型带光照 + 稳态逐帧连续绘制**(submitted=14/帧,持续渲染解锁)。
  崩溃 = CS 光源钩子遍历我方**复制的**灯光指针,物品切换后失效——复制
  方案证伪;取景偏移由标定数据锚定(bound 中心在根上方 19.4 单位)。
- **v6.21(2026-10-02 22:3x DLL,MD5 197dd9b0…,待游戏验证)= bound 动
  态居中 + 引用式灯光**:①灯光改引用物品 pass 当帧 sceneLights 数组本
  体 + 当帧新鲜度门;②级联后按 bound 中心把根平移 `anchor − center`
  居中,再级联供蒙皮读取。判读见
  [stage2-p-instance-plan.md](stage2-p-instance-plan.md) §0ad。
- **v6.22(2026-10-02 晚 DLL,MD5 b38bc09a…,run 57 已验证——替身消失)=
  幽灵化第一层(用户插入优先项)**:克隆体世界像素全抑制——透传抑制从"面
  板开着"扩展到全会话:新 `is_p_descendant`(纯祖先链,无 skin 门,连背上
  武器/箭袋挂件一起隐藏)+ 预热窗(世界流 P pass 攒满 128 个才武装,
  每个到达都证明引擎刚画过它、缓冲在初始化,run 50 教训)+ 仅
  kAttached 参与(fail-open:预热不达标则替身保持可见但摄影棚供给无损);
  v6.19 面板内条款不动;spawn 重置预热,面板关闭的菜单帧不参与。
- **v6.23(2026-10-02 晚 DLL,MD5 3cc24da1…,run 57 验证——钉桩执行但
  世界侧仍可交互,T-pose 折叠)= AI 钉桩 + 摄影棚 T-pose**:钉桩
  (kMovementBlocked/kAttackingDisabled/kCastingDisabled +
  SetActivationBlocked + StopCombat)在宽限转换点执行(日志为证)但用户
  仍遇到可交互克隆;SetCollision 证伪(只改 formFlags 记录标志,零运行
  时效果);T-pose 头在地面(74 骨骼 / 3 skin root 暴露跨参照系污染 +
  root 覆写两个错误)。**主嫌疑:存档残留克隆**——PlaceObjectAtMe 的引
  用随存档持久化,旧会话存档把未钉桩的旧克隆体带回(可见、可交互、自
  由行动)。
- **v6.24(2026-10-03 凌晨 DLL,MD5 5ebe188a…,run 58 验证——T-pose 姿
  势修复成功但朝向背对;交互目标是现役克隆,标志位实证无效)= 残留清扫
  + T-pose 修复**:①T-pose 按 skin root 分组重置生效(人物直立);②交
  互目标名字 = 标记名 "CharacterPanel_Clone" = 现役克隆体本体,
  SetActivationBlocked/BOOL_FLAGS 在该 actor 上无效;幽灵化正常(不可
  见);清扫零删除(旧残留不在到访 cell),保留;③面板开关零
  despawn——面板状态不控制克隆体世界存在。
- **v6.25(2026-10-03 DLL,MD5 3b25cbbb…,run 59 验证通过——世界中不再
  遇见克隆体)= 几何隔离**:AI 标志压不住,换距离杠杆——每帧把克隆体
  泊到玩家正下方 8000 单位(run-31 已验证配置:该深度世界仍收它、pass
  照发、缓冲照常初始化),交互/对话/碰撞(数百单位射程)天然不可达;
  kAttached 每 tick 重泊(引擎更新链会覆盖一次性 SetPosition,run 38);
  摄影棚绘制时重摆根,与泊位解耦;幽灵化/标志/标记名/清扫保留为次要防
  线。另:用户实测 + 代码链路证实"面板打开时克隆体完全消失"的机制 =
  pose_for_studio 的根 local 写入(锚点)对静止 actor 持续存在,世界侧
  交互/碰撞读到的 3D 边界被整体拖离(非透明)。
- **v6.26(2026-10-03 DLL,MD5 c78b2028…,run 60 验证——取景失败:人物
  出画面板全黑)= T-pose 朝向 + 取景解算**:朝向 Rz(180°)→0°(T-pose
  授权朝向与图驱动相反);取景按 目标占比 × 相机深度 × 视锥 tanHalfY ÷
  body_r 解算——但深度用了 camera 节点 world.translate,与真实视变换脱
  节(run 36:矩阵平移在原点),深度失真 → 出画;标定转储随游戏重启被
  日志截断吞掉。
- **v6.27(2026-10-03 DLL,MD5 f3efed14…,run 61 验证——人物入画、朝向
  成立,但位置随物品移动 + 面板泄漏到世界)= 取景深度换源**:深度 = 锚
  点在 worldToCam 第 3 行上的投影取模长(矩阵平移在原点 → 无平移项约
  定可错、符号免疫,与引擎投影同源)。
- **v6.28(2026-10-03 DLL,MD5 3854fcbf…,run 62 验证——固定锚点/菜单限
  定合成生效,但 viewFrustum 读数崩掉尺寸)= 固定视轴锚点 + 菜单限定合
  成**:①锚点 = 视轴上 Studio_Depth=485 处(眼在原点 + worldToCam 第 3
  行为视线;合成四边形把整幅 studio 目标挤压进面板矩形 → 视轴点
  NDC(0,0) = 面板正中)——人物位置/尺寸与所选物品彻底解耦,物品扫描
  从 pose 退役;②合成加 `m_in_frame` 门控(世界帧被抑制的 P pass 曾把
  面板画到世界 HDR 目标上);用户确认两项均解决。
- **v6.29(2026-10-03 DLL,MD5 f640ba0c…,run 63 验证——物品解耦/菜单限
  定/不可达全部成立,唯人物过大偏低只见腿)= 视锥半角标定**:
  `Studio_TanHalfY=0.605` 渲染证据标定;有效标定入档:body_r=138.4
  (T-pose scale=1 含武器)、w2c_t=-15(眼在原点后方 15)。
- **v6.30(2026-10-03 DLL,MD5 4dd619fc…,run 64 验证——面板位置正确但只
  有一半高度;开面板退出游戏崩溃)= 面板右移 + 蒙皮对齐(用户红框决
  策)**:①面板矩形 58-88% × 12-68% → **73-99% × 5-96%**(右侧竖条);
  ②对齐基准改为**仅蒙皮网格包围盒**(`measure_skinned_bound`,武器/箭
  袋不再拖偏中心);③尺寸改直接标定常量 `Studio_Figure_Scale = 0.70`
  (run 63 渲染证据:1.80 时视野只装下 45% 身体),公式链退役。
- **v6.31(2026-10-03 DLL,MD5 c0eaf4cb…,run 65 验证——半面板修复成立;
  人物拉长 + 灯光偏侧 + 回主菜单仍崩)= CB 分离 + 退出生命周期**:①合
  成 CB 的 y 分量身兼两职(VS 的矩形底边 / PS 的 HDR 标志),每帧被
  `out[1]=hdr` 覆盖 → LDR 面板从屏幕中线画起(半面板;此 bug 自 v4.6 存
  在)——CB 扩为 8 浮点,`g_flags.x` 载 HDR 标志;②消息处理器新增 case
  10/11/12(SKSE kShutdown/kExitGame/kQuitGame)→ 强制关面板 + despawn。
- **v6.32(2026-10-03 DLL,MD5 58b98262…,run 66 验证——比例/崩溃修复成
  立;灯光仍偏 + 主菜单残留面板)= 纵横比窗口 + 正面灯光 + 存活检查**:
  ①合成 PS 采样 x 范围缩至 (MaxX-MinX)/(MaxY-MinY)(g_flags.y)——消各
  向异性拉长;②scale 重标定 0.35(T-pose 臂展在窄窗的宽度约束);③正
  面灯光 rig(与新鲜度门共闸——run 66 证实该闸稳态常闭,rig 不生效);
  ④pump 游戏线程存活检查(kAttached 时克隆 ref/3D 丢失即 disarm)——
  run 65 crash 的渲染侧防线,生效。
- **v6.33(2026-10-03 DLL,MD5 b03c0319…,run 67 验证——rig 触发(2 灯移
  位,menu-lights=1 新鲜度门竟 armed)但人物背光;主菜单残留一帧;SKSE
  消息 9-15 零条 = 退出消息值不可靠)= 每 pass 灯光 rig + 世界丢失关面
  板**:灯光位置突变作用于 pass 实际灯光,同索引保存/恢复。
- **v6.34(2026-10-03 DLL,MD5 8995f508…,run 68 验证——正面受光成立、主
  菜单零帧成立;人物背对玩家)= 朝向翻回 π + 主菜单即关 + 灯光诊断**:
  ①灯光 rig 位置已验证(日志),人物仍背光 ⇒ **bind pose 面朝 away**
  (该判断后经 run 68 截图证伪:Rz π 时发辫+背弓可见 = 面朝 away,朝向
  应为 Rz 0,见 v6.35);②pump 增加主菜单检查:
  `IsMenuOpen(MainMenu::MENU_NAME="Main Menu")` 即 despawn +
  close_panel——主菜单出现的当帧关面板(克隆 3D 检查晚一帧,run 67 残
  留的那一帧由此消除);③灯光诊断:rig 一次性记录每盏灯的
  point/ambient 标志、luminance、新旧位置。
- **v6.35(2026-10-03 DLL,MD5 274e5a8b…,run 69 已验证——朝向已对,
  真缺陷是顶光)= 朝向定稿 Rz(0)**:run 68 截图(发辫+背弓可见)证实
  Rz(π) 面朝 away;综合两轮观察(Rz 0 时用户看到的正面当时灯尚在背后,
  Rz π 灯已修正到玩家侧),**正确朝向 = Rz(0)**——`Studio_Facing_Z_Rad`
  翻回 0,与已验证的正面灯光 rig 配合。判读见
  [stage2-p-instance-plan.md](stage2-p-instance-plan.md) §0as。
- **v6.36(2026-10-03 DLL,MD5 a7d86888…,run 70 已验证——灯位精确到位
  画面零变化)= rig 基向量归一化**:run 69 增强截图判读**朝向已对**
  (五官可见,正面朝相机)——"方向不对"的观感实为正面近黑;rig 日志
  算术反解实锤根因:**worldToCam 三行非单位长**(|row0|=6.27、
  |row1|=11.15,携带菜单相机缩放),`up×50` 实落 +557 Z、`right×±22.5`
  实落 ±141 X → 两盏灯 74° 仰角头顶上方 = 顶光(头发/肩亮、脸黑)。
  修复:rig 块读 w2c 三行先归一化再进偏移乘积,朝向保持 Rz(0),日志
  追加 row_lengths 监测。判读见
  [stage2-p-instance-plan.md](stage2-p-instance-plan.md) §0at。
- **v6.37(2026-10-03 DLL,MD5 56b86c98…,run 71 已验证——灯光成立)=
  rig 改走 NiLight 节点 + 灯光普查(用户方法论:先定类型→再定可调字
  段→最后处理)**:run 70 归一化生效、灯位落点精确到设计意图,**但画
  面零变化 = 决定性证据:着色器不消费 BSLight::worldTranslate(剔除器
  副本)**。源码调研(CS 仓库):灯光进着色的唯一位置来源 =
  `NiLight::world.translate`(LLF :275、GetLuminance :109);CS
  LightEditor 移灯唯一写法 = `niLight->parent->local.translate = pos`
  + `parent->Update`。v6.37:rig mutate 改 NiLight 节点(parent local
  += R^T·delta + Update 级联,恢复同路);一次性灯光普查。**run 71 用
  户确认正面受光成立**;遗留:摄影棚光照与世界内效果有差异(用户明示
  暂不考虑,后续重点)。判读见
  [stage2-p-instance-plan.md](stage2-p-instance-plan.md) §0au。
- **v6.38(2026-10-03 DLL,MD5 82bb9648…,被 v6.39 取代未单独验证)=
  面板-高亮解耦 + 读档闪现消除(用户两新 bug)**:日志定量实锤合成断
  供——整段会话 `composite draw` 仅 1 次(开面板瞬间的物品爆发 pass),
  稳态菜单帧 `in_menu_frame=1` 的 P pass = 0 条:合成挂在
  replay_after_original,暂停背包 UI3D 不逐帧重画(run 43),无高亮 =
  零 pass = 面板零合成。修复:①end_frame 括号关闭前补合成
  (`m_composited_this_frame` 闩 + 画进持久捕获的 format-28 实例,日志
  `end_frame composite #N`);②宽限期内经 `clone->loadedData->data3D`
  (纯数据,0x68,虚函数禁区外)一建图就把 3D 根平移到玩家下方 8k,
  玩家位零可见窗口。判读见
  [stage2-p-instance-plan.md](stage2-p-instance-plan.md) §0av。
- **v6.39(2026-10-03 DLL,MD5 7e7fac4e…,待游戏验证)= 面板生命周期
  绑定背包菜单(用户重申:面板只依赖背包开启,随背包开而渲染、随背包
  关而关闭)**:新增 MenuSink(BSTEventSink<MenuOpenCloseEvent>)——
  InventoryMenu opening → open_panel("inventory opened"),closing →
  close_panel("inventory closed");open_panel 幂等(F7 先开不重复),
  **F7 降级为手动备用**。v6.38 的 end_frame 补合成(无高亮帧可见性)与
  读档闪现修复(data3D 非虚提前泊位)原样保留。注意:P 构建只在未暂
  停帧推进(run 49),首会话第一次开背包 P 可能未建成(黑矩形),关背
  包后 pump 自动出生,第二次开背包即有人物——既有全会话存活设计的预
  期行为。判读见
  [stage2-p-instance-plan.md](stage2-p-instance-plan.md) §0aw。
- **v6.40(2026-10-03 DLL,MD5 c9db5882…,run 74 已证伪并撤回——两次同
  签名 crash)= 合成目标括号出口自捕获**:run 73 日志证明 MenuSink 生
  命周期与 end_frame 补合成都已工作,残余绑定 = s_panel_rtv 只能在
  replay 内捕获。v6.40 尝试在括号出口读/绑 OM 自捕获(format==28 门
  控),**crash ×2**(crash-2026-10-03-20-31-41/20-33-27):CS
  `HDRDisplay::SetUIBuffer` 读空指针(cmp [rsi+0x16D], rsi=0),调用链
  经 CS 的 MenuManagerDrawInterfaceStartHook(与我们共享 DrawInterface-
  Start 入口)。机制 = 括号出口 OM 属于引擎+CS 的 UI 合成状态机,零
  pass 帧第一次被我们碰就踩进 CS 自建版的未初始化分支。**教训:括号
  出口的 OM 是禁区;OM 相邻操作只做在 replay 窗口内**。
- **v6.41(2026-10-03 DLL,MD5 48d3bb6f…,run 75 部分验证——零 crash
  成立,但解耦未达成)= 目标持久化 + 出口 OM 禁区**:撤回出口自捕获;
  s_panel_rtv 跨面板开关持久。**用户 run-75 决定性实证:背包打开根本
  不进 thunk_site(进入需要高亮物品)——pass 路径在无高亮下整体不存
  在,解耦必须绕开 thunk_site(用户拍板)**。
- **v6.42(2026-10-03 DLL,MD5 fe802ae0…,run 76 部分验证——输出端通,
  输入端仍断)= kFRAMEBUFFER 直绘**:兜底合成目标改引擎常驻表
  kFRAMEBUFFER.RTV(纯内存读,零 OM 探测)。**run 76 用户 RenderDoc:
  draw 在 !target.srv 守卫早退**——摄影棚离屏目标只在 replay 内创建,
  零 pass = 无模板无目标,P 直驱也在 !target.rtv 早退:输出端通了,
  输入端(画布)仍绑 pass 路径。
- **v6.43(2026-10-03 DLL,MD5 1e45279d…,run 77 验证——自建块静默跳
  过)= 摄影棚目标自建(输入端解耦)**:end_frame 里、P 直驱之前,目
  标不存在时从 kFRAMEBUFFER desc 自建。**run 77:自建块零执行零告警**
  (self-created 0 条/失败 0 条,面板窗口内 P draw 0 条),跳过路径静
  默不可辨。
- **v6.44(2026-10-03 DLL,MD5 ac874fe9…,run 78 验证——trace 一轮定
  因)= 诊断轮**:自建块入口一次性 trace。**run 78 实锤:`fb.texture=
  0x0 而 fb.RTV 非空`**——CLib `RenderTargetData.texture/textureCopy`
  裸指针在 1.6.1170 运行时引擎不填充,模板源恒空;管线其余(高亮后
  capture→P draw→composite)无恙。
- **v6.45(2026-10-03 DLL,MD5 6320edbf…,run 79 部分验证——面板立现
  ✓,弓箭上屏 + 无光照)= 模板换 RTV 来源**:自建 desc 改从 fb.RTV 走
  GetResource/GetDesc。run 79:零高亮面板立现;两缺陷——①P 的弓+蒙皮
  垃圾几何画上屏幕(引擎 SetupAndDrawPass 在 DIRTY_RENDERTARGET 置位时
  重应用账本目标 kFRAMEBUFFER;高亮窗口干净因物品 call-site 刚用引擎
  状态绑过 format-28);②无光照(地牢方向光无 parent,节点 rig 移不
  动,defer)。
- **v6.46(2026-10-03 DLL,MD5 43d114ce…,run 80 已验证——弓箭修复成
  立)= 引擎脏位守卫**:直驱窗口裸绑后 reset DIRTY_RENDERTARGET、恢复
  后 set 回(CS Deferred 同款握手)。run 80 用户确认屏幕无泄漏。
- **v6.47(2026-10-03 DLL,MD5 b655294f…,run 81 验证——灯链全通但亮
  度近零)= 自建摄影棚灯光**:NiPointLight::Create ×2 挂 menuObjects[0]
  + BSLight 壳(引擎堆+真虚表)+ 窗口内无条件换入,新鲜度门退役。
  run 81 普查:创建/换入/rig 移动(node_moved=1)全活,画面仍极暗。
- **v6.48(2026-10-04 DLL,MD5 cce11909…,run 82 验证——半径假设证
  伪)= 半径/亮度标定**:radius 4096/fade 2.0 生效但画面零变化。
- **v6.49(2026-10-04 DLL,MD5 583f6ba8…,run 83 验证——同帧取壳扑空
  粘性报废)= 引擎正册注册**:AddLight 入册方向正确,但引擎 AddLight
  把新灯**先入 lightQueueAdd 队列、下次灯光更新才转正 activeLights**,
  同帧 GetPointLight 必扑空;sticky 失败让两盏灯全会话报废(rig 掉回
  地牢方向光)。
- **v6.50(2026-10-04 DLL,MD5 059cf22b…,run 84 验证——入册/取壳全通
  仍无光)= 双队列取壳 + 非粘性重试**:activeLights 97→99 确认入册、
  壳取回、override 在跑,但无光 → 剩余解释 = 灯的世界位置没动。
- **v6.51(2026-10-04 DLL,MD5 0082d329…,run 85 验证——级联已修但点
  光仍无光)= 私有 rig 节点 + 强制级联**:普查 `ni_world` 随 rig 动
  (级联修复成立),但发现共享父摆位互相覆盖(两灯叠一点)+ 点光位置
  数学在 LLF/引擎路径的坐标空间歧义,三轮不收敛。
- **v6.52(2026-10-04 DLL,MD5 5fd0cb29…,run 86 无变化)= 主光改方向
  光**:方向光也无效——但判读揪出贯穿性缺陷:v6.18 的 menu light
  arming 每个高亮帧覆盖自建灯数组,**run 79-86 的"高亮=亮"全部是菜
  单灯的光,自建灯从未被真测**。
- **v6.53(2026-10-04 DLL,MD5 859ddbec…,run 87 部分验证——左图自建
  灯首次生效✓)= 覆盖退役 + 壳修正**:①menu light arming 退役;②壳
  lodDimmer/lum/frustrumCull 补丁。run 87 左图(无高亮)人物均匀受光
  ——**点光 + rig 摆位 = 被验证的正确配置**;右图(高亮)黑的根因 =
  面板开关后引擎重建账本(activeLights 97→99),重取的壳里混入引擎重
  建的方向光壳(lum=10081、方向背对、lodDimmer 重置 0),补丁只打过
  一次。
- **v6.54(2026-10-04 DLL,MD5 7be5d085…,run 88 双窗口全黑)= 回归双
  点光 + 每次 fetch 重补丁**:判读实锤两个机制——①引擎灯光 tick **每
  帧**把外来壳的 lodDimmer 重写回 0(每帧 raw: lodDimmer=0.000),fetch
  时补丁永远追不上;②两灯共享 rig 父、摆位互相覆盖(双灯
  worldTranslate 同值)。run 87 左图亮 = 时序运气,点光配置本身有效。
- **v6.55(2026-10-04 DLL,MD5 0de1d262…,run 89 无变化)= 补丁进窗口 +
  每灯独立摆位**:现象不变,但确认了两条合成路径并存(无高亮 =
  kFRAMEBUFFER 兜底,高亮 = replay 实例)。
- **v6.56(2026-10-04 DLL,MD5 e45e3990…,run 90 部分验证——无高亮完
  美,高亮仍黑)= 引擎槽位约定修正**:**sceneLights[0] 是环境光槽,
  点光从 [1] 起**(LLF cpp:248 `sceneLights[i+1]` 实锤)——主光自
  v6.47 起一直坐在环境槽被无视。新布局:槽 0 环境底光 + 槽 1/2
  key/fill 双点光(numLights=3)。run 90 左图(无高亮)完美 = 主光首
  次生效;右图(高亮)黑 = replay 窗口绘制上下文污染(物品 pass 的
  post-original 状态)。
- **v6.57(2026-10-04 DLL,MD5 153358b7…,run 91/92 验证——面板光照正
  确)= 单绘制路径**:replay 窗口 draw/合成退役;end_frame 唯一绘制
  合成点。run 91/92:面板内光照正确且高亮/无高亮一致。
- **v6.58(2026-10-04 DLL,MD5 e5acc3ef…,run 92 验证——世界泄漏修复
  ✓,面板光变暗)= 灯光 rig 泊位制**:rig 默认泊位 Z+100000,仅绘制窗
  口拉回锚点,所有出口泊回。run 92:世界场景光斑消失;但面板光变暗 =
  拉回后 per-pass 摆位仍写满锚点系目标 → 锚点双重计入,灯距翻倍。
- **v6.59(2026-10-04 DLL,MD5 5ad6742d…,run 93 已验证——全部成立)=
  拉回坐标系修正**:per-pass 摆位改为锚点相对偏移(spread/up/forward,
  不含 anchor 项)——rig 停锚点 + 节点偏移 = 灯落 light_target。
  **run 93 用户确认:面板光照正确(双点光正面)、世界无泄漏、高亮/无
  高亮一致。摄影棚灯光命题完整闭环**。当前 DLL MD5 5ad6742d,已提交
  aeba6da(v6.47–v6.59,runs 81–93)。
- **阶段 2b 完成(2026-10-04,run 94–99,"摄影棚即居所"工作包收官,
  方案与判读全史见 stage2b-studio-home-plan.md §H1–§H7)**:
  - **轮 1 验证通过(run 94–97,提交 2412891)**:3D 图从世界摘出、
    挂 menuObjects[0] 下私有节点 CP_StudioHome、NiPointer 强持有
    (v6.60–v6.63)。U1 关闭(run 95 实锤引擎 SetupAndDrawPass 惰性建
    缓冲,无需等世界渲染);U5 恒等成立(home_world=0,pose 数学不
    变);U2 十轮 parent ok;乒乓(data3D 未斩前壳簿记每帧抢图 707
    次)v6.63 斩断归零;TGA 取证恢复(v6.39 起 close_panel 丢 dump
    标志 + m_session_replays 门,均修)。
  - **轮 2 验证通过(run 98–99,提交 ce0f654)**:迁移同 tick 杀壳
    (data3D 先斩,Disable 无 3D 可毁,run 51 机制构造性失效)+ ghost
    层全家退役 + pump 存活检查 homed 分流。壳死图活(run 98:杀壳后
    13 轮面板 338 帧照常);teardown 双路径首演干净(run 99:读档与
    回主菜单的 disarmed→killed→released 链零崩溃);清扫噪音 v6.65
    修(跳过已删/已禁用 ref)。泊位/钉桩/早期泊位保留(只服务 ~1.5 s
    构建窗口,对原 §3 清单的范围修正见 §H5)。
- **v6.69(2026-10-04,run 104 已判读)= 出生即泊位 + grace fade 守卫(用户
  报告:进游戏前几帧,玩家身边出现一个透明、可交互的克隆体,之后才被
  移到别处)**:
  - 机制:PlaceObjectAtMe 只能把引用锚在玩家身上——grace 期引用位置留
    在玩家处,与 v6.38 的节点级泊位逐帧打架(引擎更新链按引用位置重推
    3D 变换),排序输掉的帧渲染出渐显中的替身("透明"即 BSFadeNode 渐
    显);激活封锁因 grace 期虚函数禁区只能等 grace 结束,proto_pinsta
    nce 的钉桩块就在 grace-over 段——可交互窗口与可见窗口同源。
  - 修复 1(治本):出生原语换 `TESDataHandler::CreateReferenceAtLocation`
    (CLib 头文件在册、src 有 REL 绑定;单次调用、发生在 grace 之前,不
    踩 grace 期虚函数禁区)——引用直接出生在泊位深度(玩家
    data.location − 8000 Z、玩家 cell/worldspace/朝向),引擎每帧重推的
    就是泊位本身,竞争消失;8000 深度天然不可交互。**核心未知 = 出生
    深度是否触发完整角色 3D 装配(加载距离门)**:现有证据只覆盖"近处
    装配 → 泊到深处"(run 31/59),没覆盖"出生即在深处"。null 句柄回
    退 PlaceObjectAtMe,日志区分两条路径。
  - 修复 2(回退路径止血):grace 期每 tick 把 data3D 根(BSFadeNode)
    的 fade 三元组压 0(0x130 currentFade 为 CLib 命名字段;0x128/0x12C
    语义未证实、按 target/rate 处理——全压 0 在任一语义下都收敛于不可
    见;纯数据写,AsFadeNode 与既有 UpdateDownwardPass 同安全级),
    grace 结束恢复 target=1/rate=0/current=1(不恢复 target 会被引擎
    渐变拉回黑)。
  - 判读(run 104):①进游戏全程玩家身边任何时刻无克隆体(**外域与室
    内各验一次**);②日志出现 `born at park depth` 且后续链照常;③若见
    `falling back` = 原语失败走回退;④零崩溃零回归。
- **Run 104(v6.69 会话,用户截图 + 日志 + TGA 376–381 判读)= 出生即泊
  位成立✓✓,渲染残缺 → born-at-depth 证伪**:
  - **成立的部分**:六轮读档(两种存档)全部走 CreateReferenceAtLocation
    (零回退),链路照常(出生 → 1.2 s 武装 → 迁移),**进游戏全程玩家身
    边零克隆体**(用户确认)——原 flash 问题在出生层被消灭;
  - **残缺的部分**:面板人物散落——铁盔+脸+长弓+手套可见,躯干/双腿/所
    有衣物缺失;主存档(6 件衣物)45 蒙皮几何只出 37 pass 的零星像素;
    **囚徒存档(10 几何)的轮次竟渲染出主存档角色的铁盔/长弓/手套**;
    唯一本轮出全裸人形的轮次与其余轮次同存档不同渲染。
  - **判读依据**:①`studio T-pose: 0 bind bones over 3 skin root(s)`——
    v6.23 时代同机制是 74 bind bones;numMatrices/GetBoneCount()=0 =
    **nif 级蒙皮数据被剥离**(高模不可能),空绑骨在渲染器退化为恒等蒙
    皮 → 几何按原始模型空间位置散落;②同存档周期间渲染不一致 = 装配/
    换装的异步时序竞争;③镜像衣物全轮不可见。
  - **根因定性(推断,与全部证据自洽):引擎按引用距离在装载时刻选择
    角色 LOD**——引用出生即在 8000 深度 → 装配出低精度图(部件缺失、
    蒙皮剥离、morph 未应用)。与既有证据自洽:v6.25 时代"玩家处装配 →
    泊 8000"的图 13 轮保持高模 = **降级只发生在装载时刻,装配后泊远不
    降级**——与 run 51(Disable 毁 3D)、run 96(活体 3D 属世界)同级
    的引擎硬事实:**装配窗口内引用必须贴近玩家**。
  - 结论:born-at-depth 路线证伪留档;flash 修复从"放置层"转到"可见
    性层"(v6.70)。
- **v6.70(2026-10-04,待游戏验证)= 回退出生原语 + fade 守卫转正为
  flash 主修复**:①spawn 回退 PlaceObjectAtMe(引用在玩家处装配,保高
  模);②v6.38 节点泊位保留;③v6.69 的 grace fade 守卫转正——装配期
  每 tick 把根 BSFadeNode 的 fade 三元组(0x130 currentFade 为 CLib 命
  名字段;0x128/0x12C 按 target/rate 处理)压 0,grace 结束、穿衣之前恢
  复 target=1/rate=0/current=1。**排序输掉的帧渲染全透明**(渐变在
  target=0 时无法爬升),透明 + 位移的图使激活射线无物可击——v6.68 的
  "透明可交互替身"两个症状都压在可见性层解决。纯数据写,AsFadeNode 与
  既有 UpdateDownwardPass 同安全级。
  - 判读(run 105):①面板人物**完整着装**(与 runs 94–103 基线一致;
    T-pose 行应回到 ~74 bind bones);②**进游戏前几秒玩家身边无可见克
    隆体**(fade 守卫的效果验证)——若仍见透明替身 = 守卫也输排序,下
    一层方案 = 把 pin 挪进渲染括号(end_frame,渲染线程,排序必赢),
    已留档;③无交互(E 键打不中);④零崩溃、既有判据零回归。
- **Run 105(v6.70 会话,用户确认)= flash 收官**:面板人物完整着装✓
  (高模装配恢复);进游戏玩家身边无可见克隆体✓(fade 守卫在游戏线程
  层已够——渲染括号升级方案无需启用);零崩溃零回归。**"进游戏闪现透
  明可交互克隆体"问题关闭**(出生层证伪留档,可见性层修复成立)。
  v6.69–v6.70 全部提交。
- 方案与证据链:[stage2-p-instance-plan.md](stage2-p-instance-plan.md)
  (§0–§0ad 三十一轮留档、§2 路线、§3 失败模式)、
  [stage2b-studio-home-plan.md](stage2b-studio-home-plan.md)(§H1–§H7)。

**源码结构迁移(2026-10-05,零行为改动,游戏内首验通过)**:proto 的三个
大文件(passredirect 2916 行 / pinstance 1236 行 / main 435 行)按职责
拆为 `src/` 下 23 个小文件,采用 skse-plugin-template
布局(main/panel/hooks + pinstance/ + render/),用按行范围断言的抽取脚本
字节级迁移。主构建目标 `CharacterPanelProto` 现从 `src/` 构建;`src/` 旧
spike 代码已清除,`tools/m0_proto` 目标退役(目录保留作档案参考)。**运行
时身份不变**:DLL 名、日志 `CharacterPanelProto.log`、TGA 目录、F7/F8 全
部照旧;Release 构建通过,导出表(SKSEPlugin_Load/Query/Version)与部署中
的 v6.70 完全一致。用户游戏内确认读档 → 面板照常。代码地图见下文更新后
的表格。

**skee64 脸部变形崩溃判读(2026-10-05,迁移后首测,非迁移引入)**:换用拆分
版 DLL 后首次读档即崩(crash-2026-10-05-00-45-29):崩点
`skee64.dll+00BF79D`(`and rdx,[rcx+0x20]`,rcx=0),经 skse64 任务调度的
RaceMenu `SKSETaskApplyMorphs` 在克隆 facegen 的发型 HeadPart
(BSDynamicTriShape "N16",Dint999 BDOr)上空指针;CharacterPanelProto.dll
不在任何栈上(仅模块列表)。**同签名两次见于迁移前**(crash-2026-10-02-22-45-20
/ crash-2026-10-03-13-01-22,v6.20/v6.30 时代,同为玩家 NPC formID 0x7 +
BSFaceGenNiNodeSkinned + BGSHeadPart BSDynamicTriShape +
BSFaceGenModelMap::Entry)。判读:**既有间歇性竞态**——克隆 faceNPC=玩家,
PlaceObjectAtMe 触发 facegen 异步装配,skee64 对新装配的脸部网格沿 faceNPC
链应用变形时 per-model morph map 尚未挂上;105+ 次放置输 3 次(两次在旧代
码时代)。重试即恢复(用户确认可进)。复发频繁时的候选缓解:自动出生推迟
2-3 s 避开读档爆发期;结构性消除需先立 skee64 morph 挂接侦察轮。

**身份转正(2026-10-05,用户决定:原型已验证可行)**:插件身份由
CharacterPanelProto 更名为 **CharacterPanel**(C++ 命名空间、CMake 目标、
DLL 名、版本资源、SKSE 插件名一并改)。随之变化的证据路径:日志
`Documents/My Games/Skyrim Special Edition/SKSE/CharacterPanel.log`、
TGA 目录 `...\SKSE\CharacterPanel\`、构建命令 `--target CharacterPanel`。
**部署注意:MO2 里必须删除旧的 CharacterPanelProto.dll,只装
CharacterPanel.dll——两个 DLL 的 SKSE 导出同名,同时加载 = 钩子双装。**
更早 run 记录里引用的旧路径按历史留档对待;日志文本前缀("Proto v3…" 等)
未随身份改名,历史判读行不受影响。

**模板规范归一(2026-10-05)**:代码与构建全面对齐 skse-plugin-template——
①全部源码命名空间迁入 `PLUGIN_NAMESPACE` 宏(展开为
`MyPlugins::CharacterPanel`,由根 CMake `PROJECT_NAMESPACE` 生成,
`PLUGIN_NAMESPACE_BEGIN/END` 包裹);
②SKSE 导出与横幅改从模板元数据取值:`Plugin::Plugin_Name / Plugin_Author /
Plugin_Version`(含 `Plugin_Build_Identity` 扩展项),均由根 CMake
`PROJECT_NAME/AUTHOR/VERSION` 生成,代码不再硬编码身份字符串;
③`PROJECT_VERSION` 1.0.0 → 1.2.0(SKSE PluginVersionData、版本资源、横幅
随之 1.2.0.0,与 v6.70 时代 SKSE 版本保持连续);AuthorName 由误写的
"CharacterPanel" 修正为 "AmazingBuff"。运行时身份(DLL 名/日志/TGA 路径)
不变。已有代码的 east-const 化按 small-project-cpp-rules 的"绿地默认值"
原则不回溯。

## 已验证事实(动手前必读,都是实测结论,不是推测)

1. **菜单 3D pass 走三个 `RenderPassImmediately` 调用点**。物品预览只走
   site 1:`RelocationID(100877,107673)+0x1EE`、`(100852,107642)+0x28F`、
   `(100871,107667)+0xED`,调用目标 `SetupAndDrawPass` =
   `RelocationID(100854,107644)`。thunk 签名
   `(BSRenderPass*, uint32 technique, bool alphaTest, uint32 renderFlags)`。
2. **Community Shaders(LightLimitFix)先钩了 site 1**。安装时必须先解析
   E8 rel32 取 pre-patch 目标存下来,透传和重放都调它("interposed, chain
   restored")。漏掉这步 = 静默旁路 CS + 垃圾纹理(run 11)。
3. **shadow-state 重应用发生在 `SetupAndDrawPass` 内部**:脏标记未消耗时,
   函数内部会把引擎自己的 OM、PS 纹理槽(以及推定 CB)重新应用回去。任何
   "先绑私有目标再调用"的顺序都要跟它逐族搏斗(run 9–13 的教训)。
   **正解是 post-original 顺序(v2.5)**:透传先画完(状态就位、脏标记耗尽),
   再绑私有目标调一次,1:1 复制,零状态捕获/恢复。
4. **调用点渲染目标是 UI 合成图,`R8G8B8A8_UNORM`(format 28)**,不是
   kMAIN 的 R11G11B10;引擎深度资源是 TYPELESS 格式,取 desc 建新纹理会
   E_INVALIDARG,必须归一化(R24G8_TYPELESS→D24_UNORM_S8_UINT 等,
   `normalize_depth_format()`,run 15 的教训)。
5. **调用点继承的 depth-stencil 状态 depthEnable=0**;重放要绑私有深度缓冲
   + 自建 depth-on 状态(LESS_EQUAL/全写),用完恢复原状态(run 16 验证
   把手遮挡正确)。
6. **菜单 pass 识别**:pass 的 geometry 沿 parent 链(≤32 层)比对
   `UI3DSceneManager::menuObjects[8]` 根指针,整帧零误报。物品在 root[1]
   (run 6 census)。只重放 shaderType==6(BSLightingShader);其余计数记录。
7. CS 1.9.1 不重定向受监控例程(capture report finding 3),在 CS 开启下
   取得的证据对原版管线同样成立。
8. 运行时门禁:仅 AE 1.6.1170 + skse64 2.2.6(`supported_runtime()`);
   一次游戏会话**绝不**同时加载 CharacterPanel(spike)+ Probe + Proto。

## 代码地图(2026-10-05 拆分迁移后)

| 文件 | 内容 |
| --- | --- |
| `src/main.cpp` | SKSE 导出(Load/Version/Query)、消息处理(kDataLoaded/kPostLoadGame/kPreLoadGame/kNewGame/退出 10-12)、日志初始化、运行时门禁 supported_runtime |
| `src/panel/panel.h/.cpp` | Proto 面板生命周期单例(open/toggle/close/dump/代数)+ MenuSink(面板随背包开关,v6.39) |
| `src/input/input.h/.cpp` | InputManager:F7/F8 热键 sink(FR-05,只观察/拒绝按住重复);InputHandler 匿名 sink,HLC 模式 |
| `src/render/ui_render_hook.h/.cpp` | DrawInterfaceStart Detours detour(帧括号驱动器)+ `install_ui_render_hooks()`(三调用点钩子 + 帧括号,顺序保持) |
| `src/pinstance/pinstance.h/.cpp` | 独立展示实例 P:状态机(spawn→grace(节点泊位+fade 守卫)→穿着→attach)、pump 节奏、despawn/kill、残留清扫、is_p_* 白名单匹配 |
| `src/pinstance/pinstance_home.cpp` | 阶段 2b 的家:白名单武装、CP_StudioHome 迁移(data3D 先斩 + 杀壳)、release/verify/note_panel_open(U2) |
| `src/pinstance/pose.cpp` | 摄影棚摆姿:固定视轴锚点、蒙皮 T-pose 重推导、强制级联、蒙皮包围盒居中 |
| `src/pinstance/dress.cpp` | 构建窗口穿着镜像(身体槽位,EquipItemEx 顺序) |
| `src/render/pass_redirector.h/.cpp` | 三调用点钩子安装(E8 rel32 解析取 pre-patch 目标)、begin/end_frame 括号(目标自建 + kFRAMEBUFFER 兜底合成)、on_pass 分类、目标生命周期、透传抑制 thunk |
| `src/render/p_draw.cpp` | draw_p_proactively:主动 pass 生成(GetRenderPasses) + 配方缓存兜底 + 每 pass 灯光改写 + call_site_original(1) 绘制 + 脏位守卫 |
| `src/render/replay.cpp` | 重放窗口(post-original OM 捕获、目标 sizing、绑定存活校验;v6.57 起不再绘制/合成)+ 深度格式归一化 + 状态取证日志 |
| `src/render/studio_lights.cpp` | 自建摄影棚灯光 rig(环境+key/fill 双点光、ShadowSceneNode 入册、壳补丁、泊位制、fetch 停滞自愈) |
| `src/render/composite.h/.cpp` | 屏幕合成:SV_VertexID 全屏四边形 + 固定管线状态 + 面板矩形常量;HLSL 从 `shaders/` 嵌入消费 |
| `src/render/shaders/composite_vs.hlsl` `composite_ps.hlsl` | 合成四边形 HLSL 源文件;构建期经 `cmake/embed_shaders.cmake` 生成 `shader_sources.h`(`render_shaders::CompositeVS/CompositePS`)——HLC 参考项目同款机制 |
| `src/render/offscreen_target.h/.cpp` | 私有离屏摄影棚目标(color+depth+depth-on 状态)、TargetSig、s_panel_rtv 捕获指针 |
| `src/render/evidence.h/.cpp` | 取证:同步 TGA 导出(F8 与关面板取证契约) |
| `src/render/render_internal.h` | 渲染层共享内部件:typedef、pre-patch 目标表、call_site_original、心跳/灯数常量 |
| `tools/m0_proto/` | 迁移前的原型源码(构建目标已退役,保留作档案;运行史见 m0-proto.md) |
| `tools/m0_probe/` | 探针(F7/F8 抓取),已完成使命,保留作证据工具 |
| `docs/stage2-p-instance-plan.md` | 阶段 2 验证轮方案:技术路线、失败模式 F1–F5 与回退阶梯 R1–R3、验证步骤 |

## 下一步规划(2026-10-01 范围修正后,用户决定)

**范围修正**:面板依托菜单场景中的展示模型存在,仅在背包等非世界界面
出现;与"世界画面 + 面板"共存相关的一切(原 A01 等)整体推迟。当前实现
天然满足——重放与合成只在有菜单光照 pass 的帧发生,世界画面内不会出现
面板,无需删改代码。PRD 已升 0.5 记录此修正(§6.1)。

已完成的工作包(不再展开):常驻括号(run 19)、常驻摄影棚目标(run 19)、
屏幕合成(run 26,用户 RenderDoc 定位 blend 问题后 v4.6 修复)。

### 阶段 1:M0 工程收官——**完成**(验收记录:[m0-acceptance.md](m0-acceptance.md))

1. ~~面板矩形 INI 可配置化~~ **用户决定推迟**(2026-10-01"暂时不需要
   配置,这是后面的事");固定矩形 58–88% × 12–68% 与物品卡的避让随
   阶段 3 布局定稿一并处理。
2. ~~菜单内验收清单~~ **完成**:逐项验收与证据映射见
   [m0-acceptance.md](m0-acceptance.md)——7 项中 6 项通过、1 项部分通过
   (读档/新游戏强制关闭的触发路径,并入阶段 2 首轮);性能粗基线留档,
   正式测量推迟(PRD §5.3)。

### 阶段 2:M0 完整收官——独立展示实例 P(**当前阶段**,中风险)

按 PRD M0"可先固定姿态,但必须使用独立展示实例"——当前面板内容是被
重定向的物品预览(证明载体),还不是 P:

1. **P 实例**:克隆玩家 3D(spike 的 clone_actor 机制已验证)挂到
   UI3DSceneManager 空闲 menuObjects root(AttachChild,CLib ID 51859
   已验证);固定姿态,不接动画更新。
2. **摆位与取景**:P 根节点摆到 UI3D 相机前方合适位置,面板内满幅显示
   P(替代现在的整幅缩放);面板内容与原物品预览的并存/替换关系在此
   阶段定稿。
3. **管线自动覆盖**:P 挂在 menuObjects 之下即被祖先识别命中,走同一
   重放与合成路径,无需新机制。**最大未知:蒙皮几何过重放路径的兼容性**
   (物品是静态网格,P 是蒙皮角色;PRD FR-04 本就要求蒙皮验证)——单独
   安排验证轮,失败模式与回退方案先行写清。
4. **首轮验证顺带补两笔**(来自 m0-acceptance.md 开放项):开面板状态下
   存档+读档,确认 kPreLoadGame 触发路径的强制关闭;正式 CPU/GPU 帧耗时
   与显存测量(PRD §5.3)。
   → 产出:PRD M0 要求达成,M0 完整收官。

### 阶段 3:M1——外观与动作闭环(PRD FR-01/02/03)

1. 装备与外观同步(Apparel Preview 的装配逻辑为参考,community-reference-
   supplement.md §4);
2. 临时污染过滤(FR-02);
3. 待机动画与展示动作切换(FR-03;暂停场景的更新入口 = §7 问题 4);
4. 面板布局定稿(**需要用户输入**:P 的位置/大小/与物品卡及后续网格
   背包的关系)。
   → 产出:面板内 P 外观正确、动作独立、换装同步。

### 决策点与风险

- 蒙皮重放兼容性(阶段 2 最大未知,先行单独验证);
- 面板布局需用户参与设计(阶段 3 开工前);
- modded UI 遮挡关系:run 26 截图记录了面板盖住卡片右半的现状,阶段 1
  的 INI 调位解决;
- 世界画面对照(原 A01/A02 的相机部分)推迟,何时恢复由用户决定。

## 测试 runbook(每轮流程,已跑熟)

```powershell
# 构建(vcpkg 锁被占会卡在 "Running vcpkg install",先清掉残留 vcpkg 进程;
# 工具链走 VCPKG_ROOT 环境变量——vcpkg 已迁至 C:\env\vcpkg,旧 D: 路径作废;
# 2026-10-05 起代码主体在 src/,无需 CHARACTER_PANEL_BUILD_PROTO)
cmake -S . -B build `
  -DCHARACTER_PANEL_BUILD_PROBE=ON `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows-static-md
cmake --build build --config Release --target CharacterPanel
```

- **并行度**:不要写 `--parallel 4` 之类的固定值——不传该选项时 MSBuild 会用全部
  逻辑核(本机 32),而 `CMakePresets.json` 的 `/MP` 已经让 MSVC 自己也多进程编译;
  写死小数字会把构建卡在 4 核。

- 产物 `build/Release/CharacterPanel.dll`,**手动**装入 MO2
  (本机 `E:\SkyrimAE\mods\...`,MO2 profile `AE`;直接放真实 Data 会被清)。
  **2026-10-05 身份转正:同目录下旧的 CharacterPanelProto.dll 必须删除,
  只装 CharacterPanel.dll(两个 DLL 的 SKSE 导出同名,同时加载 = 钩子双装)。**
- 游戏内(v3.1 面板流程,详见 tools/m0_proto/README.txt):**取证动作就是
  关面板**——进存档 → 背包高亮有 3D 模型的物品 → F7 开面板 → 停一两秒 →
  F7 关面板,渲染线程自动写一张 `proto-pass-NNN.tga`(先导出后释放)。
  可选:F8 在面板开着时中途抓帧(关着按无效并告警);普通画面开面板几分钟
  等心跳行(约 30 秒一条);开面板状态下存档/读档确认强制关闭(无 dump 行)。
- 采集:`Documents/My Games/Skyrim Special Edition/SKSE/CharacterPanel.log`
  + `...\SKSE\CharacterPanel\proto-pass-NNN.tga`(2560x1440 RGBA TGA)。
- 判读:用 python 解 TGA 头(18 字节,type 2,32bpp,top-down)转 PNG + 统计
  非黑像素/包围盒/最亮值,裁剪后直接看图;判据见 m0-proto.md 各 run 段落。
- 回报物:log 全文 + TGA 序号;异常先查 `[W]` 行。
- **阶段 2(v5)新增判据**:日志应出现 `Proto P attached…`(root/ 
  skinned_geoms 计数)与 `Proto P skin rebind`(bones_rebound 计数),
  P 几何的 `menu pass` 发现行带 `p=1`;关面板出现 `Proto P detached` +
  `Proto P retired graphs released`。TGA 中预期人形(蒙皮重放命题);
  失败模式对照 [stage2-p-instance-plan.md](stage2-p-instance-plan.md) §3
  的 F1–F5 判读。

## 未决事项

- `extern/CommonLibSSE` submodule 处于 dirty 状态(历史遗留,未处理,勿
  随意 stash/reset;提交时永远排除)。
- 原版渲染器(无 CS)场景未单独跑过(finding 3 表明低价值,可继续搁置)。
- Culler 真实布局(+0x140/+0x250/+0x3D5/+0x3F4)仅在回头走 accumulator
  路线时才需要,继续搁置。
