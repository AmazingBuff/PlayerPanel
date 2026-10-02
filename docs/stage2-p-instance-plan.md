# 阶段 2 验证轮方案：独立展示实例 P（蒙皮过重放路径）

- 日期:2026-10-02;run 27–56;**run 56:v6.20 首验——人物像成型带光照、
  稳态逐帧连续绘制(submitted=14 每帧)!剩取景偏移 + CS 光源钩子崩溃
  (复制的灯光指针失效);v6.21 = bound 动态居中 + 引用式灯光**
- 状态:**v6.21 待游戏验证**
- 上游:[m0-handoff.md](m0-handoff.md) 阶段 2;PRD M0"独立展示实例
  (可先固定姿态)"
- 读者:实现与验证此轮的 Agent / 开发者

## 0ad. Run 56 结果(v6.20 首验:成型带光照 + 稳态连续绘制 + 灯光崩溃)+
  v6.21 方案

v6.20 会话,用户截图(RT 人物像 + VS dump)+ 日志 + crash-2026-10-02-22-24-41:

- **人物像完整成型、带光照**(皮肤/衣物有明暗,弓在背上)——M0 渲染命题
  全面闭环;
- **稳态逐帧连续绘制确认**:`submitted=14 of 14` 每 20ms 连续 40+ 帧
  (v6.19 的引擎侧全抑制 + 灯光画后还原解锁了持续渲染,run 54 的"稳态
  断供"未再出现);
- **标定数据**:`item_scale=0.288`,bound r=25.4、中心在根上方 ≈19.4
  单位——人物从锚点(脚底)向上长,放大 10 倍后头顶出框、整体偏右上;
- **崩溃(crash-2026-10-02-22-24-41)**:CS `GeometrySetupConstantPointLights`
  钩子遍历 P 弓 pass 的 sceneLights 时踩到失效 BSLight——v6.18/19 把
  物品 pass 的灯光**指针复制**进我方成员数组,物品切换后旧光对象销毁,
  连续绘制 19 秒后命中。**复制指针方案证伪**。

**v6.21(2026-10-02 22:3x DLL,MD5 197dd9b0…,待游戏验证)**:

1. **灯光改引用式**:记录物品 pass 的 sceneLights **数组本体指针** +
   numLights(引擎当帧为物品自己的绘制所用的同一份存储),配合"当帧
   有菜单光照 pass 才覆盖"的新鲜度门(begin_frame 清、on_pass 置;日
   志确认 menu_passes=1/帧恒流,门恒开);画后还原保留;
2. **bound 动态居中**:级联后测 body 中心,根平移 `anchor − center` 使
   身体中心精确落在锚点(投影已证正确的位置),再级联一次供蒙皮矩阵
   读取终姿;零相机约定假设,纯向量运算;
3. 其余不动。

**判读**:①人物居中入框 + 退背包/切物品不再崩 → M0 摆位取景收官,进
阶段 2 收尾清单(存读档强制关闭 + 性能基线);②仍偏 → 标定转储
(`studio pose` 行)给出最终 bound,下一轮按常数微调;③若 CS 光源钩
子仍崩 → 引用数组也有生命周期问题,改为自建常量灯光或绕过 CS 钩子。

## 0ac. Run 55 结果(v6.19 首验:人物像成立!)+ v6.20 方案

v6.19 会话,用户 RenderDoc VS 截图 + 目视确认:

- **RenderTarget 上生成了人物像(带光照)**——M0 蒙皮渲染命题闭环:
  几何/设备缓冲/蒙皮/投影/光栅化/光照全链路贯通。"摄影棚灯光"(菜单
  光覆盖)与锚点摆姿同时生效;
- VS 截图:VS Input 为蒙皮局部坐标(单件护甲 0–10 单位),VS Output
  SV_POSITION=(109.4,136.4,485.6,**500.2**)——NDC 双入界,但相机空间
  深度 ~485 + scale=item×0.1 → **人物过远过小**(用户:距离太远);
- 崩溃与稳态断供:v6.19 的全抑制/画后还原是否根治待本轮确认;静默
  kNone 日志(`whitelist root is null` / build aborted)待收。

**v6.20(2026-10-02 22:2x DLL,MD5 fecb84e4…,待游戏验证)= 取景标定
轮 1**:

1. 摆姿 scale 的 0.1 猜值升级为命名常量 `Studio_Figure_Scale = 1.0`
   (人物与物品预览同尺度,10 倍于旧值);
2. `pose_for_studio` 每面板前 3 帧输出标定转储:anchor/item_scale/
   root_scale/world_bound(radius+center)——配合用户下一轮 RenderDoc
   的 NDC 读数,可解析出精确倍率(NDC_h ≈ 2·r/(depth·fTop)),不必再
   猜;
3. 其余不变。**判读**:人物像尺寸合适 → M0 摆位与取景收官;仍小/过大
   → 按标定转储 + NDC 读数解出精确倍率,一轮定值。

## 0ab. Run 54 结果(v6.18 首验:爆发期有画 + 退背包崩溃)+ v6.19 方案

v6.18 会话日志(22:00)+ crash-2026-10-02-22-00-30:

- **爆发期绘制成立**:F7 开面板瞬间的世界帧里 P pass 爆发(66 个/帧、
  全套蒙皮件 discovery),`submitted=14 of 14` 连续三轮——链路是活的;
- **稳态断供**:爆发过后零 P pass、零 P draw 日志,直到关面板;第二次
  开面板时 P 竟然重新出生(状态无声回到 kNone,无任何 despawn 日志
  ——**存在一条未知的静默 kNone 路径**),且此时 P 尚在 grace(面板在
  背包里打开,构建被暂停闸挂起),白名单未武装;
- **心跳关键数据:`P=0/14 player=0/23`**——玩家自己的图(每帧都在渲
  染)也是 0!**init 检查本身测错了对象**(rendererData/vertexBuffer 探
  针对蒙皮几何无意义或偏移有误),run 52 的深度截图才是真相:缓冲一直
  存在。心跳退役,以 RenderDoc/TGA 为准;
- **退背包崩溃(crash-2026-10-02-22-00-30)**:`BSLightingShader::
  SetupGeometry` 空指针解引用(rax=0),崩溃帧链 = 引擎调用点 → 我方
  thunk → CS/EngineFixes → SetupAndDrawPass,RBX = P 的护甲
  BSTriShape——**引擎在画一个"被登记"的 P pass**。机制:v6.17 恢复的
  GetRenderPasses(带 UI3D accumulator)会把 P pass 登记进累加器的持
  久列表,引擎之后(包括退背包的菜单清理绘制)自己画它们;而 v6.18
  把 pass 的 sceneLights 改成我方成员数组且从不还原,菜单切换(开书)
  后光源失效 → 崩溃。同时引擎在菜单帧画 P pass = UI 里的双影。

**v6.19(2026-10-02 22:1x DLL,MD5 699dad2a…,待游戏验证)**:

1. **P pass 引擎侧全抑制**:should_suppress_passthrough 扩为
   `replay && (p_geom || !in_menu_frame)`——世界帧(防双影)与菜单帧
   (防清理绘制崩溃)都不让引擎画 P pass,摄影棚是唯一渲染者;
2. **灯光覆盖画后还原**:mutate → call → restore,被登记的 pass 不再
   带我方暂存数组存活;
3. **静默 kNone 加日志**:tick 的 clone-null 路径(头号嫌疑)+ 无根早
   退一次性告警(`whitelist root is null`)——下一轮钉住稳态断供真因;
4. 心跳退役(探测对象已被证伪),其余防线保留。

**判读**:①爆发期照常有画 + 退背包不再崩 → 崩溃闭环;②稳态帧出现
`whitelist root is null` → 断供真因 = kNone 泄漏,按日志追;③稳态帧
有 `submitted>` 行但 TGA 黑 → 持久化/合成层问题(v4.2 的逐帧合成依
赖重放窗口,稳态无 pass 时合成是否续上待验)。

## 0aa. Run 53 结果(v6.17 首验:深度成形 + RT 近黑)+ v6.18 方案

v6.17 会话,用户 RenderDoc 截图(RT0 + DS 对照):

- **人物在深度图完整成形**——剪影比例正确(头/躯干/弓/腿)、深度值合
  理、投影在界内。**v6.17 的链路恢复完全成功**:几何、设备缓冲、蒙皮、
  投影、光栅化全部就位(同时证明 SetupAndDrawPass 侧的几何初始化假设
  成立——rd 门自锁理论闭环);
- **RT 近黑但有极暗剪影**——几何对、光照项≈0 的典型症状。归因:P 的
  pass 携带**地牢世界光**(run 49 首例 `numLights=1`),而顶点被摆进
  UI3D 菜单空间——光源距片元数千单位,衰减归零,只剩环境项;
- 附带确认:剪影偏小 = 摆姿 scale=item×0.1 的结果(取景属 M1)。

**v6.18(2026-10-02 21:3x DLL,MD5 4c48f00c…,待游戏验证)= 摄影棚灯光
+ 锚点摆姿**:

1. **灯光覆盖**:on_pass 记录物品预览 pass 的菜单光(`studio lights
   recorded: N menu lights`),主动绘制前把每个 P pass 的
   numLights/sceneLights/numShadowLights 覆盖为菜单光(pass 是 property
   一帧对象,改动不跨帧存活;世界侧 P pass 本就被透传抑制);
2. **摆姿回归锚点位**(v6.3 实证配置,run 40 NDC 双入界):删除
   v6.7–6.10 的眼点反推/近平面停靠——其平移分量约定有已知漂移 bug
   (run 46),且把 P 停在了离菜单光很远的位置;锚点直接读场景,零推
   导,且是菜单光为之布置的位置;
3. 心跳与其他防线全部保留。

**判读**:①RT 出现有光照的人形 → 命题闭环,M0 蒙皮渲染达成,剩取景
(M1);②仍暗但剪影变亮/局部亮 → 灯光距离仍不匹配(下一层:自建摄影
棚 BSLight 或再校距离);③DS 有形但 RT 全黑无剪影 → 光照覆盖未生效
(查 `studio lights recorded/applied` 两行日志是否出现)。

## 0z. Run 52 结果(v6.16 首验:崩溃消除、缓冲仍缺)+ v6.17 方案

v6.16 会话(用户报告 + 日志):

- **崩溃消除**——撤 Disable 后 pose_for_studio 不再踩空指针,run-51 的
  崩溃签名未再出现;
- **但仍零人物绘制**(日志继续 `drawn=0` 路线,心跳数据待收);
- **用户方向性判断**:"回到 10 轮之前——run 35–42 那时实际上可以绘制
  人物,只是不在视锥内"。该时代 RenderDoc 里有真实的人物 draw call 与
  VS 输出(run 36/38/40/41 的 SV 截图即为证),与 v6.13 后"零 draw"形成
  根本对比。

**统一解释(收口 run 35–51 全部矛盾)**:run 35–42 的人物 pass 是"活水"
(live GetRenderPasses)+ 经引擎自己的 `SetupAndDrawPass` 画的——设备缓
冲由引擎侧惰性创建;v6.13 改手绘 + rd 门后,**rd 门把 pass 挡在
SetupAndDrawPass 之外 = 拆掉了唯一能初始化几何的调用者**,rd=9 从此
自锁。run 48 的"内部拒画"只发生在 recipe 重建 pass(MakeRenderPass 产
物 accumulationHint 不匹配批处理分桶),不适用于 live pass。

**v6.17(2026-10-02 21:2x DLL,MD5 afb4f378…,待游戏验证)= 恢复 run-35
绘制链**:

1. 手绘路径整体退役:对每个收集到的 pass,重绑 studio OM 对(v6.4 防
   线)后直调 `call_site_original(1, pass, passEnum, passEnum&0x40,
   0x200)`——site 1 的原目标即物品预览走的 CS 互插链,技术设置、几何/
   设备缓冲初始化、批处理派发全部交还引擎;
2. run 35 之后仍成立的修复全部保留:近平面停靠(v6.7)+ 强制级联
   (v6.5)+ 不透明混合(v6.5)+ 干净 rasterizer(v6.6)+ 每 pass OM
   重绑(v6.4);配方缓存兜底保留;
3. 手绘专用基础设施(rd/vb/technique/ib 门、stride 日志、SetDirtyStates
   刷洗)随路径一起退役;`P init heartbeat`(v6.16)保留——它现在是
   "惰性初始化假设"的直接探针:若 SetupAndDrawPass 开始处理 P 几何,
   心跳应从 0/14 翻到满员。

**判读**:①心跳翻满 + `submitted=N of N` + RenderDoc 出现人物 draw
call → 链路复活,剩下的是像素落点问题(回到 run 40–42 已知域);②
submitted>0 但心跳仍 0 → SetupAndDrawPass 不做惰性初始化,设备缓冲需
要另外的来源(回 §0y 判读 ②);③submitted=0 → SetupAndDrawPass 在
OM 窗口内连收都不收(新事实,再定)。

## 0y. Run 51 结果(v6.15 首验:自动出生通 + Disable 崩溃)+ v6.16 方案

v6.15 会话日志(20:59)+ crash-2026-10-02-20-59-23:

- **自动出生链路全通**:读档落地后 clone placed(20:59:09.7)→ dressed
  (10.8)→ whitelist armed(11.1),1.4 s 完成,全部在未暂停帧;
- **`renderer init check: 0/14`**——即使未暂停世界渲染了 75 帧(60
  grace + 15 settle),蒙皮几何的设备缓冲仍为零。疑问:缓冲到底会不会
  出现、何时出现,还是检查本身对蒙皮几何无意义——需要对照数据;
- **F7 即崩(crash-2026-10-02-20-59-23)**:`EXCEPTION_ACCESS_VIOLATION,
  RIP=0(空虚表调用)`,崩溃帧 = `pose_for_studio` 第 169 行
  `root->UpdateWorldBound()`,上游 = 手绘窗口重放链(菜单 pass →
  replay_after_original → draw_p_proactively)。**根因 = v6.15 在 attach
  时 `actor->Disable()`:禁用 actor 会摧毁其 3D 图**,白名单里的根指针
  悬空,12 s 后 F7 第一次摆姿即踩空。

**v6.16(2026-10-02 21:0x DLL,MD5 5d37693a…,待游戏验证)= 撤 Disable +
初始化心跳**:

1. **撤掉 Disable**——克隆保持存活(enabled),图永远不会悬空;已知代
   价:克隆站在玩家原位(首人称不可见;第三人称有一个重叠替身,原型
   阶段接受);
2. **初始化心跳**:tick 的 kAttached 分支每 60 活动帧探测一次 P 图与
   玩家图的 `rendererData/vertexBuffer` 蒙皮计数
   (`P init heartbeat: P=a/b player=c/d`),满员即停并打
   `fully renderer-initialized`;20 次(约 20 s)仍不满员则告警收摊;
   玩家图是对照组——它每帧都在渲染,若玩家自己也是 0,说明检查/布局
   假设错了而非时机问题;
3. 手绘路径零改动:白名单持续武装,rd 门在缓冲出现的那一刻自然开始
   放行——**drawn>0 的时刻 = 缓冲就绪的时刻**,心跳日志与 draw 日志
   可直接对表。

**判读**:①心跳 `player=c/d` 满员而 P 迟迟不满 → 世界渲染确实创建了
玩家几何的缓冲,P 的图因某原因未被处理(下一步:查克隆 3D 是否真的
挂进了世界场景图);②player 也是 0 → 检查/布局假设错,回头重验
BSGraphics::TriShape 与 BSGeometry+0x138/0x178 的实测语义;③P 满员后
`drawn>0` → 命题闭环,看 TGA 人形与像素质量。

## 0x. Run 50 结果(v6.14 门诊断轮:元凶点名)+ v6.15 方案

v6.14 会话日志(20:33–20:35):

- **每帧 `drawn=0 of 9 (source=live/recipes) gates: rd=9 vb=0 technique=0
  ib=0`**——全部 pass 死于第一道门:**`rendererData == nullptr`**;首例
  详情 `geom=[Bow_WoodenMesh:0] shaderType=6 passEnum=0x48000037
  skinned=true`;
- 手绘链本身无罪:SetupTechnique 从未被调到,IB 门也未触及——**P 的
  几何从未被引擎做渲染初始化,设备端 VB/IB 根本不存在**;
- 机制确认:F7 在背包菜单(暂停)里按下,clone 的 grace/dress/settle
  整个构建过程都发生在暂停帧——**世界渲染器停帧,引擎从未画过 P 一
  次**,rendererData 自然从未创建;run 29/30 已证伪"菜单场景代渲染",
  故初始化只能来自真实世界渲染。

**v6.15(2026-10-02 20:49 DLL,MD5 fdd0a67a…,待游戏验证)= 未暂停世界
帧渲染初始化**:

1. **自动出生**:kDataLoaded/kPostLoadGame 置 world-ready;pump 在
   kNone + world-ready 时每帧尝试——游戏线程校验 `!GameIsPaused()` 且
   玩家 3D 存在才 spawn。P 在读档后玩家一落地就出生,世界渲染器自然
   初始化其全部几何(玩家始终在视锥内,克隆与玩家重叠,首人称不可见);
2. **构建暂停闸**:tick() 在暂停时直接返回(计数器也不走)——构建只会
   在未暂停帧推进,杜绝"菜单里建好但零初始化"的回归;F7 在菜单里按
   下的 spawn 也因此安全(构建挂起直到玩家退出菜单);
3. **attach 时验证 + 禁用停泊**:白名单武装后遍历图统计
   `rendererData/vertexBuffer` 完好的蒙皮几何数并打日志
   (`renderer init check: N/M`),然后 `actor->Disable()`——克隆退出
   世界画面与剔除器,设备缓冲持久存在,绘制时摆姿(根 local + 强制级
   联)在停泊图上照常工作(spike 时代先例);
4. **面板开关与 P 生命周期解绑**:F7 关闭不再 despawn——构建好的实例
   保留整个会话,重开面板零重建;despawn 只挂在 kPreLoadGame/kNewGame
   (读档销毁,下次落地自动重建)。

**判读**:日志顺序应为 `clone placed (world-render init route…)` →
`renderer init check: N/M skinned geometries…`(N=M 期望满员)→
`parked disabled` → F7 后 `P draw (manual): drawn>0`。若 N<M:缺的
几何是 dress 后新出现的装备网格且 settle 帧内未被渲染——加长
Settle_Frames;若 N=0:世界渲染初始化路径整体未发生(查 spawn 是否
真的发生在未暂停帧)。drawn>0 后像素质量归于 v6.14 已就位的状态防线
(刷洗/混合/光栅化/近平面停靠)。

## 0w. Run 49 结果(v6.13 首验:手绘循环全员拒画)+ v6.14 方案

v6.13 会话日志(19:56–19:59)+ 用户 RenderDoc 观察:

- **每帧 `P draw (manual): drawn=0 of 10~14 passes (source=live/recipes)`,
  共 749 帧、零帧 drawn>0**——pass 收集正常(live 生成 10~14 个/帧,
  配方兜底未触发),循环每帧进入,但 **DrawIndexed(proto_passredirect.cpp
  1589 行)从未执行**,与用户 RenderDoc"零人物 draw call"互证;
- v6.13 循环里 DrawIndexed 之前有**四道静默早退门**:①`rendererData`
  空 ②`vertexBuffer` 空 ③`SetupTechnique` 返假 ④`indexBuffer` 空——
  哪一道拒的,日志无痕。

**交叉证据(CLib 头文件 + Community Shaders 源码)**:

- `GetGeometryRuntimeData().rendererData` → `BSGraphics::TriShape{VB, IB,
  VertexDesc, raw*}` 布局与用法在 CS 实战验证(GrassOptimizations 直取
  VB/IB,LightLimitFix 读 vertexDesc/rawVertexData)——v6.13 字段访问
  无误;
- **CS 的 BeginTechnique 钩子(Hooks.cpp:159)证明 `SetupTechnique` 返
  false 是真实存在的路径**:vanilla 查找失败时 CS 只对**非 Effect**
  shader 用自家缓存兜底(`shaderType != Effect` 分支),Effect 系直接
  false——而 run 32 已证 P 身体是 Effect shader(头发才是 Lighting);
- CS GrassOptimizations 手绘路径(963–974)揭示 v6.13 缺的最后一块:
  引擎的 VS/PS/常量绑定走**延迟 shadow state**,派发时由 SetDirtyStates
  (ID 75580/77386)刷进 context;裸 DrawIndexed 前必须补
  vertexDesc/topology 记账 + 刷洗,否则画在上一笔引擎状态上。

**v6.14(2026-10-02 20:31 DLL,MD5 e03a297d…,待游戏验证)= 门诊断 +
  状态刷洗**:

1. 手绘循环四道门逐 pass 计数,帧摘要扩为
   `P draw (manual): drawn=… gates: rd= vb= technique= ib=`;每门每面板
   开启首次命中打详情行(geom 名/shaderType/passEnum hex/numLights/
   skinned);
2. stride 不再假设 0x30,取 `vertexDesc.GetSize()`(LLF 同款),首帧记录
   实际值;
3. DrawIndexed 前补 shadow state 记账(vertexDesc/topology + DIRTY 位)+
   SetDirtyStates 刷洗(CS 手绘路径同款)。

**判读表(下一轮日志直接点名元凶)**:rd/vb 吞掉全部 pass → P 几何从未
被渲染初始化(disabled-park 路线嫌疑),修法 = 让引擎对 P 执行一次渲染
初始化;technique 吞掉全部 → CS Effect 缺口实锤,修法 = 绕 CS 预热技术
或改走直调;ib 吞掉全部 → TriShape 布局质疑(概率最低)。

## 0v. Run 48 结果(v6.12 诊断轮:真相到墙前一步)

v6.12 会话日志(15:48)+ TGA 232-234(活动期抓帧):

- **每帧 `P draw: source=live/recipes passes=8~15`,共 1040 次调用**
  ——绘制管线每帧执行、pass 数稳定(live 生成 10~15 个 pass,配方缓存
  兜底);绑定检查 held;
- **但活动期抓的 TGA 整帧零内容**——四态防线全开、入锥投影、held 绑
  定,像素仍不落地。**结论:SetupAndDrawPass(ID 107644)内部在拒画**
  ——它的批处理逻辑按 pass 的 accumulationHint / 当前 accumulator 状
  态做分桶调度,我们的"带外"pass 不属于任何活跃桶,Draw 调用被内部
  吞掉(RenderDoc 里也看不到像素级 draw 交互)。

**v6.13 = 手绘(绕过批渲染器)**:
1. 不再调 `SetupAndDrawPass`;直接走 BSShader 虚链:
   `SetupTechnique(passEnum)`(绑 technique 对应 VS/PS/布局)→
   `SetupGeometry(pass, renderFlags)`(绑常量/纹理/顶点声明,蒙皮矩阵
   在此重算)→ 裸 D3D:`IASetVertexBuffers`(rendererData->
   vertexBuffer,stride 0x30)+ `IASetIndexBuffer` + TRIANGLELIST +
   `DrawIndexed(indexBuffer 大小/2)` → `RestoreGeometry`;
2. BSGraphics::TriShape(VB/IB/VertexDesc)布局在 CLib
   NiSkinPartition.h 完整可得,零偏移猜测;
3. 配方缓存/近平面停靠/强制级联/OM 重绑/不透明混合/干净 rasterizer
   全保留——手绘路径同样受这些防线保护;
4. 已知简化(标定项):stride 0x30 为蒙皮顶点假设,若人形出现但顶点错
   位,从 vertexDesc.GetSize() 精确取 stride 即可。

## 0u. Run 47 结果(v6.11:日志闩误导判读,真图景两好一疑)

v6.11 会话日志(15:28-15:29,两次开面板):

- **好 1**:两次 arm 帧都成功画(62/37 passes,`binding held`);
- **好 2**:第二次开面板后**每帧 49 个 P pass 从世界流到达 thunk**,
  `on_pass` 白名单全命中、replay=true、透传抑制生效——触发链全活;
- **疑**:747 次 depth 日志 = `draw_p_proactively` 进入 747 次,但
  RenderDoc 零 draw——**旧 `P drawn` 日志被 `m_p_draw_logged` 永久闩
  掩盖了真实路径**(只记会话第一次),无法区分"每帧画了"与"每帧早退";
- 可见的静默早退点:live 生成空转(与 run 46 同)→ 配方重建路径的
  实际产出未被记录。

**v6.12 = 每帧绘制摘要(诊断轮)**:`draw_p_proactively` 末尾打
`P draw: source=live/recipes-failed/empty passes=N`——每帧一戳,彻底
消除闩盲区;`empty-live` 首次发生也单独记一笔。**先跑这轮拿数据再定
下一修**(若 source=recipes 且 passes>0 而 RenderDoc 仍零 draw,则
MakeRenderPass 重建的 pass 状态有问题——下一层)。

## 0t. Run 46 结果(v6.10:depth 日志 = draw 在跑的活证;生成链后段断了)

v6.10 会话日志(14:36-14:37):

- **`anchor depth` 日志 74 次 = `pose_for_studio` 执行 74 次 = 主动绘制
  74 次都进入了**——v6.8 的 end_frame 直驱完全工作;**但 `P drawn`
  日志只有 arm 帧 1 次**:绘制在 `GetRenderPasses` 处拿到**空链**静默
  早退(`passes.empty()` 无声);
- 深度两个值:arm 帧 **485.1**(菜单相机活动、物品在位)→ 后续 **-15.0**
  (反推的"眼点"落在锚点后方 15 处——worldToCam 平移分量的行列/符号
  约定还有一处不匹配,单独标定项);
- **核心事实:暂停后引擎不再为该物品生成 pass,`GetRenderPasses` 恒
  空**——与 run 43"不重画同一物品"同源,连显式调用也不产生。

**v6.11 = pass 配方缓存 + MakeRenderPass 重建**:
1. 活动生成成功时(arm 帧等)记录**配方**:{shader, property, geometry,
   technique=passEnum, numLights, sceneLights[4]}(全在 BSRenderPass 上
   现成可读;引用对象在面板打开期间全部存活:actor 图/单例 shader/UI3D
   菜单灯);按全键去重,关面板清空;
2. 生成空转的帧:逐配方 `shader->MakeRenderPass(property, geometry,
   technique, numLights, lights)`(ID 107497)重建 pass →
   `SetupAndDrawPass`;
3. 其余防线不变。**注意**:MakeRenderPass 创建的 pass 挂进 property 的
   RenderPassArray(引擎在下一 accumulator 周期清理),单帧生命周期。

## 0s. Run 45 结果(v6.9:取反只镜像了 x——列假设证伪)

v6.9 会话(用户 RenderDoc):

- **SV = (-126.4, 31.3, -15.5, -0.47)**:w 仍负、yz 与 run 44 完全相
  同、仅 x 翻号——world.rotate 第 3 列 ± 都不是视线轴;在相机系里我
  们摆出的两个点关于视线轴对称错位;
- **结论:放弃从 world.rotate 猜轴向**,改用权威变换
  `NiCamera::worldToCam`(相机空间投影矩阵的前身,引擎每帧自己维护)。

**v6.10 = 从 worldToCam 正推视线/眼点**:
1. 视线(相机系 +Z 的世界向量)= worldToCam **旋转块第 3 行**
   (行主序约定,`w2c[2][0..2]`);
2. 眼点 = `-R^T × 平移分量`(`w2c[i][3]`,i=0..2);
3. 锚点深度 = dot(anchor − eye, view);不在 [20,200] → P 放
   `eye + view × 20`;
4. 深度日志更新为 v6.10 标签——本轮日志的 depth 值与 run 43 的 17.3
   对比即可验证新轴向是否自洽。

## 0r. Run 44 结果(v6.8:触发链通了,方向反了)

v6.8 会话(用户 RenderDoc VS 截图):

- **draw call 每帧出现**——end_frame 直驱修复生效,触发链闭合;
- **SV_Position = (124.5, 31.3, -15.5, -0.47)**:w 为负 = 顶点在**相机
  后方**;z=-15.5 恰为 near=15 量级——P 确实在"距相机 20 单位"处,只
  是方向反了(v6.2 起"局部 +Y=视线"的假设与 NiCamera 约定相反:NiCamera
  沿局部 **-Y** 观察);

**v6.9 = 视线取反**:view = world rotate 第 3 列的**负值**;depth 测量
与 near+5 摆位都用新方向。其余不动(end_frame 直驱、近平面停靠、强制
级联、OM 重绑、不透明混合、干净 rasterizer)。

## 0q. Run 43 结果(v6.7:近平面停靠生效但 P 只画一次——触发链断)

v6.7 会话日志(13:57):

- `anchor depth: 17.3 (near=15)`——**物品锚点深度实测 17.3,本就在近平
  面旁**;深度在 [20,200] 外→P 放视轴 near+5(=20),停靠逻辑生效;
- **但 `P drawn proactively` 只有 arm 帧 1 次,之后 74 个括号帧零绘制、
  零 menu pass**——**背包暂停时 Inventory3DManager 不重画同一物品**
  (loadedModels 常驻,只有切物品才发新 pass),物品 pass 不来 →
  `replay_after_original` 不触发 → OM 窗口不开 → P 不画。`anchor
  depth` 日志每帧刷是 tick 的姿势计算,与窗口无关;
- RenderDoc"完全没有人物 draw call"与此一致。

**v6.8 = end_frame 直驱(触发与物品彻底解耦)**:
1. `draw_p_proactively` 自含 OM 窗口:自绑 studio RTV/DSV/深度/视口 →
   清屏(每帧一次)→ 摆姿 → 逐 pass 画(每 pass 前重绑)→ 全状态恢复;
   不再依赖任何引擎 pass 先到;
2. `end_frame` 每括号帧直调它(括号帧必然到达 end_frame);物品 pass
   窗口路径保留为冗余(帧闩防重);
3. 前沿防线全保留:近平面停靠(near+5)、强制级联、不透明混合、干净
   rasterizer、绑定漂移检测。

## 0p. Run 43 结果(v6.6 仍无人形;用户指示改近平面)

v6.6 四道状态防线(OM 重绑/强制级联/不透明混合/干净 rasterizer)仍无
人形——用户判断"最后的坐标太远",指示改到近平面附近。

**v6.7 = 近平面停靠 + 深度实测**:
1. `pose_for_studio` 实测锚点深度:`depth = dot(anchor − cameraPos,
   viewDir)`(NiCamera 局部 +Y=视线),日志打
   `Proto v6.7 anchor depth along the studio view: N (near=15)`;
2. 锚点深度不在 [20, 200] 区间时,P 放到**视轴上 near+5(=20)**处:
   `cameraPos + viewDir × 20`——彻底移除"锚点深度未知"变量;
3. 其余防线不变(强制级联/OM 重绑/不透明混合/干净 rasterizer)。

## 0o. Run 42 结果(v6.5 会话,用户 RenderDoc rasterizer 状态)

- 用户在 RenderDoc 里发现主动绘制时的 **rasterizer state 携带
  depthBiasClamp = -100**(物品 call site 的遗留状态):z/w≈0.97 的贴
  远平面深度经 bias 偏移后整块推出深度范围——光栅化全灭。与"TGA 全
  黑、SV 入锥、draw 在跑"三证完全自洽;
- 我们的 OM 窗口只压回了 RTV/DSV/深度状态——**rasterizer state 从未
  管控**。

**v6.6 = RasterGuard(第三道状态压制)**:主动绘制期间强制换上合成器
的干净 rasterizer(无 bias、无 scissor、CULL_NONE——v4.4 为合成四边
形自建的那套),绘制完恢复引擎状态。现有防线:OM 重绑(v6.4)+ 不透
明混合(v6.5)+ 强制级联(v6.5)+ 干净 rasterizer(v6.6)。

## 0n. Run 41 结果(v6.4:投影完全正确,骨骼没跟上)

v6.4 会话(用户 RenderDoc 完整 SV 四列截图):

- **SV_Position = (110.7, 144.4, 486.4, 501.0)**:NDC x/w=0.221、
  y/w=0.288、z/w=0.971——**全部入界,投影完全正确,必然光栅化**;
- 用户指出两嫌疑:①z≈远平面被裁;②无混合;
- **①的深挖**:z/w=0.971 ≈ 视距 0.971×20480 ≈ 19890 单位——恰是洞窟
  到 UI3D 相机原点的量级!P 根和 geometry 的 world 已被 v6.2/6.3 摆
  对(xy 入锥证明了),但**蒙皮顶点由骨骼矩阵驱动,骨骼 world 还留在
  引擎摆的旧世界位置**——`Update(kDirty)` 的级联被克隆携带的
  selective-update 标志短路(动画控制器节点常见),`Update` 只处理"标
  记为需更新"的子树;
- **②成立可能**:引擎遗留混合状态未知,TGA 全黑+alpha 满 255 与"混合
  因子把 RGB 压到 0"相容。

**v6.5 = 强制级联 + 不透明混合压制**:
1. `pose_for_studio` 改用 `UpdateDownwardPass` 强制整树变换级联(不问
   selective-update 标志)+ `UpdateWorldBound`——蒙皮矩阵(frameID 重
   算)读到的骨骼 world 终于与根一致;
2. 主动绘制加 `OpaqueGuard`:逐 pass 绘制期间强制 blend disable(全部
   通道写入),绘制完恢复——studio 目标独占,不透明覆盖即正确语义;
   v6.4 的逐 pass OM 重绑与绑定漂移检测保留。

## 0m. Run 40 结果(v6.3:投影胜利,目标失守)

v6.3 会话(用户 RenderDoc VS 截图 + TGA 判读):

- **SV_Position = (107.7, 137.5, 484.7)**:第三列即 w,NDC x/w≈0.22、
  y/w≈0.28——**双双入界,顶点投影正确,必然光栅化**(物品参照系标定
  成功);
- **但 TGA 213 全帧零内容、alpha 恒 255**——P 的像素没落进摄影棚目标
  (若画了但透明,alpha 会变;全黑+满 alpha = 清屏后无绘制);
- **归因收窄到 OM 绑定**:draw 执行 + 入锥 + 我们窗口绑了 studio
  RTV,像素却不在——`SetupAndDrawPass` 内部的 batch 逻辑按 pass 的
  accumulationHint 自行重绑 OM 对,把绑定从 studio RTV 抢走。

**v6.4 = 每 pass 前重绑 OM + 漂移检测**:
1. 逐 pass 画,每个 pass 前重设 `OMSetRenderTargets(studio)` +
   深度状态——batch 抢绑多少次就压回多少次;
2. 批量绘制后检查绑定并一次性记日志
   (`proactive draw binding after SetupAndDrawPass batch: held/stolen`)
   ——若仍被抢,下一轮把 P 绘制移到窗口最末并做最终重绑;若 held 而
   TGA 仍黑,则问题在 pass 的状态丢失(下一个怀疑对象,证据已备)。

## 0l. Run 39 结果(v6.2:SV 巨幅改善,相机旋转语义错)

v6.2 会话(用户 RenderDoc VS 截图):

- **SV_Position 从 (-40386,3221,-1418) → (353,1087,-20)**——根变换
  生效,顶点落到相机邻域;但除以 w 后仍在视锥外,面板无人形;
- **归因**:v6.2 假设"NiCamera 局部 +Y=视线,world rotate 列 3 为世
  界视线方向"与 UI3D 相机的实际朝向语义不符(或相机 world 语义与
  worldToCam 的组合方式不同)——相机自标定用的旋转是错的;
- **可靠参照系只有物品预览**:Inventory3DManager 把物品摆在 UI3D 相
  机正确投影的空间里(menuObjects[1] 下),它的 world translate/scale
  就是"这个相机系里能显示"的实测答案。

**v6.3 = 物品参照系标定**:`pose_for_studio` 改为——锚点 =
`menuObjects[1]` 下首个物品几何的 `world.translate`,缩放 = 其
`world.scale × 0.1`(人物/物品尺度比,首轮标定),朝向 Rz(180°)(面向
管理器约定的相机方向);无物品时回退原点单位变换。**注**:这是标定参
照,不恢复"面板依赖物品"——无物品时面板仍能开(P 站回退位),物品存
在时用它的参照(更准)。用户验收的"面板不显示物品"不变(物品 pass 仍
不进摄影棚)。

## 0k. Run 38 结果(v6.1:相机系 SetPosition 无效,绘制时直摆根)

v6.1 会话(用户 RenderDoc VS 截图):

- `P drawn proactively: 8 passes`(8 个蒙皮件全部生成 pass),但
  **SV_Position 仍 ≈ (-40255, 3200, -1446)**——与 run 36 的世界坐标
  几乎相同(仅差 130 单位)。tick 的 `SetPosition(camera)` 没有生效或
  被引擎的更新链覆盖(引擎从自己的记账重推 actor 3D 根的 world 变
  换);spike 时代 SetPosition 有效的前提是克隆**已 Disable**(无 AI/
  移动更新覆盖),本路线克隆是活的,每帧被引擎重摆回世界位置。
- 另证:P 绘制只发生在 arm 的同一帧(8 passes),后续帧因无窗口触发
  …实际每帧都有物品 pass 触发窗口,帧闩逐帧重置——**绘制每帧在跑,
  只是画的位置错**。

**v6.2 = 绘制时直摆根变换(最终姿势权)**
1. `PInstance::pose_for_studio()`:OM 窗口内、pass 生成前,直接设 P 根
   的 **local transform**(父级是世界 cell 根,local 即世界目标):
   `camera->world.translate + view*Studio_Standoff`,Rz 面向相机,然后
   `Update()` 级联——蒙皮矩阵(SetupGeometry 的 frameID 重算)读到的
   就是相机帧内的骨骼 world;
2. actor 级停靠全面退役(tick 的 SetPosition 分支、attach_graph 的初
   始 SetPosition、Park_Depth);姿势的唯一权威 = 绘制窗口;
3. 世界侧:P 根的 local 在面板打开期间被持续改写——世界渲染(若有)
   读同一变换,但透传抑制在杀它的像素,且 anchor 是相机原点(世界地
   板下),双保险。

## 0j. Run 37 结果(crash-2026-10-02-03-05-37 判读)

- crash 时刻 03:05:37,日志显示 **03:05:09 安装钩子后面板从未打开**
  (零 `clone placed`/`Panel opened`)——v6.0 的新代码路径一行都没跑过;
- **crash 签名**:HUDMenu 消息链(`MenuManager::Update → HUDMenu::
  ProcessMessage → GFxValue::Invoke`)读已释放对象
  (`mov r15, [r14+0x38]`,r14=0x…1CBF 未对齐,+0x38 = HUDData 的
  quest/wordOfPower 槽);**po3_FloatingSubtitles 与 FasterLoadscreens
  在同一栈上**(两者也钩 HUD 消息/读档流);
- 历史对照:9 月的 crash 全是 spike 克隆代码/renderdoc/d3d11 签名,
  此签名**首次出现**,且在 DLL 完全被动的会话;
- **定性:环境性 crash(读档 × HUD 消息竞态,第三方钩子在链上),与本
  插件无代码路径因果**;钩子存在只弱相关(改变消息时序)。

**v6.1 防御加固**(降被动面,不改功能):
1. kPreLoadGame/kNewGame 在 force-close 后追加 `PInstance::despawn()`
   ——读档也杀克隆 actor,任何半建状态不留过读档边界;
2. 安装横幅更新 v6。

## 0i. Run 36 结果(v5.9:VS 实测钉死投影原因 + 用户验收新要求)

v5.9 会话(用户 RenderDoc VS 输入/输出截图):

- **VS Input POSITION ≈ (1.4, 0..10, 3.1)**——模型局部坐标,人体正常
  尺寸,蒙皮正确;
- **VS Output SV_Position ≈ (-40386, 3221, -1418)**——NDC = xy/w 远超
  [-1,1],零光栅化实锤。物品锚点是**世界坐标**(洞窟内),而 UI3D 相机
  的 worldToCam **不含世界偏移**(translate 在原点)——它期望物体在
  相机局部空间;物品预览能显示正因 manager 把它摆进相机局部系。
- **用户新验收**:①人物位置有巨大问题(即上述投影错位);②面板不应
  依赖高亮物品——背包内随时可开,且面板内**不需要显示高亮物品**。

**v6.0 = 相机系停靠 + 面板只显示 P**:
1. **停靠改 UI3D 相机帧**:每帧 `SetPosition(camera->world.translate +
   view_dir * Studio_Standoff)`(NiCamera 局部 +Y 为视线,取 world
   rotate 列 2),`SetHeading(atan2(-view.x, view.y))` 面向相机;脱离
   menuObjects[1] 依赖——**无高亮物品也能开面板**(面板只画 P,物品
   pass 不再重放进摄影棚,物品在原位正常预览);
2. **物品 pass 不再重放**:`replay_after_original` 只作为 OM 窗口触发
   器,窗口内只做 `draw_p_proactively`(帧闩一次)——面板内容 = 纯 P;
3. `Studio_Standoff = 40`(相机前方 40 单位,首轮标定值,取景细调归
   M1);`Park_Depth`/`item_anchor`/`m_has_anchor` 全部退役。

## 0h. Run 35 结果(v5.8:draw call 通了,像素没出来)

v5.8 会话(用户 RenderDoc):

- **`GetRenderPasses` 主动构造成功**:P 的 draw call 真实发生(用户
  RenderDoc 确认),pass 生成链路全通;
- **两个缺陷**:①draw call 无像素落进面板(面板仍是物品)——**P 的
  顶点在玩家下方 8k 的世界坐标,经 UI3D 相机 worldToCam 投影后在视锥
  外,GPU 执行 draw 但零光栅化**(run 31 世界流能看到人形,正因当时用
  世界相机看世界坐标的 P);②draw call 重复 3 遍——
  `replay_after_original` 每物品 pass 触发一次(每帧 3–8 个物品
  pass),无帧闩。

**v5.9 = 锚点停靠 + 帧闩**:
1. **重停目标改为 UI3D 物品锚点**(`menuObjects[1]` 下首个几何的
   world translate):UI3D 相机构图看的就是这个点,P 站这里顶点必然落
   进摄影棚视锥;世界相机看不到它(锚点在世界原点附近,游玩发生在远
   处;面板开着时透传抑制也在杀它的像素);tick 每帧维持,换物品时持
   最后锚点;
2. `m_p_drawn_this_frame` 帧闩(begin_frame 重置):P 每摄影棚帧只画
   一次,与清屏节律对齐。

## 0g. Run 34/35 结果(v5.7 世界帧内有效、背包帧内空转 → 捕获时机死结)

v5.7 会话(2026-10-02 02:11 + 日志 + 用户 RenderDoc):

- **v5.7 的 OM 窗口修复在世界帧内成立**:02:11:00/05 两次世界内 arm,
  P burst(75/74 pass)→ `P snapshot replayed in the studio OM window:
  64 passes` → composite draw #1——机制正确;
- **但 02:11:16/33 两次背包内 arm:零 P pass、快照为空、无重放**——
  用户进背包后世界流已死(暂停),F7 按下时白名单 arm 了,P 却永远等
  不到 pass。**结构死结:pass 捕获依赖"面板开着时世界流还活着",而
  背包内恰恰死了**。快照机制的供给端在目标场景里天然为零。
- 用户路线决策:**主动构造绘制**(弃预捕获)。

**v5.8 = 主动构造绘制**:
1. **pass 生成本体可直调**:`BSShaderProperty::GetRenderPasses(geometry,
   renderMode, accumulator)`(纯虚,vtable 2A)——引擎原生 pass 生成
   逻辑,内部自己选 technique、收灯光、经 `BSShader::MakeRenderPass`
   (ID 107497) EmplacePass;不需要我们拼任何参数;
2. `replay_after_original` OM 窗口内:遍历 P root 的蒙皮几何 → 对每
   个几何调其 property 的 `GetRenderPasses(geom, kNormal, 菜单
   accumulator)` → 对返回 RenderPassArray 链逐条
   `call_site_original(pass, passEnum, alphaTest, 0x200)`——
   technique==passEnum、alphaTest=passEnum bit 6(0x40)均从 run-31
   日志实证;renderFlags 0x200 为菜单流实测值;
3. accumulator 用 `UI3DSceneManager::unk10`(菜单场景自己的,灯光状态
   由本帧 Begin3D 备好,与物品预览同一光照);
4. 生成的 pass 是 property RenderPassArray 的一帧对象(下一个
   accumulator 周期被 DoClearRenderPasses 清理),不手动释放;
5. 世界流透传抑制保留(P pass 出现在世界帧时仍不画进世界);快照机制
   整体移除(死代码)。

## 0f. Run 33 结果(v5.6:快照在跑但画进不可见目标,用户 RenderDoc 判定)

v5.6 会话(2026-10-02 00:52 + 用户 RenderDoc):

- 快照机制本身在工作:`P snapshot started` 8 次,burst 后每菜单帧
  `lighting_replayed=1` 持续 = `replay_p_snapshot()` 逐帧在调用;
- **但面板仍无人形,RenderDoc 证实摄影棚目标没有 P 的 pass**。
- **根因**:`replay_p_snapshot` 在 end_frame 裸调 `call_site_original`
  (引擎 SetupAndDrawPass 只画不管绑目标)——此刻管线上绑的是环境
  OM 状态,不是摄影棚 RTV/DSV/深度/视口。run 31 世界流那次能看到人
  形,恰因为那条路走 `replay_after_original` 的完整 OM 绑定窗口。
- **蒙皮矩阵前提已确认**(CLib 调研):frameID 机制保证每个渲染帧重
  放蒙皮 pass 都重算蒙皮矩阵(实时读骨骼 world transform);暂停只冻
  结姿势不冻结重算,逐帧快照重放安全。
- **管线前提已确认**(CLib 调研):背包内高亮物品 3D 每显示帧走
  `InventoryMenu::PostDisplay → Inventory3DManager::Begin3D/Render/
  End3D → UI3DSceneManager accumulator → BSBatchRenderer::
  SetupAndDrawPass(100854/107644)`——site 每帧都为物品 pass 触发,
  OM 窗口每帧都存在。

**v5.7 = 快照重放迁入 OM 窗口**:
1. `replay_p_snapshot_in_studio_window()` 在 `replay_after_original`
   内部调用——本 pass 重放完成、`OMSetRenderTargets(studio)` 仍生效
   的窗口内;每个菜单帧任一物品 pass 重放都会携带 P 快照重画;
2. **pass 身份校验**(引擎池化 BSRenderPass 跨帧可能回收):逐条校验
   `pass->geometry` 非空且 `is_p_geometry` 仍真;失效条目惰性淘汰
   (写指针压缩,不 panic);
3. 计入 `m_menu_lighting_replayed`(关面板取证闸门);首次重放打一条
   `P snapshot replayed in the studio OM window: N passes`。

## 0e. Run 32 结果(v5.5:背包内零 P;残影确认,双归因)

v5.5 会话(2026-10-02 00:30,用户 RenderDoc 观察 + 日志):

- **背包内零人形,RenderDoc 证实无 P 渲染逻辑**。日志:p_passes 只在
  面板打开瞬间的**世界流帧**爆发(passes_seen=69/66,p_passes=69/66),
  之后菜单帧 p_passes=0。**归因:菜单打开 → 游戏暂停 → 世界渲染器
  停止产出帧 → 世界 pass 流整个消失 → P 无处发 pass**。run 31 的
  "每帧重停"修复无关紧要——不是 P 被剔除,是流本身没了。
- **残影(用户 RenderDoc 发现)**:清屏只在每帧首个重放发生,且 v5.5
  把 `m_cleared=false` 挪到了开面板一次——菜单帧的物品重放 + P 世界
  帧重放全部叠画在同一目标上。确认。
- **深度事实**:P 的 type 6 pass 只有头发(0Anto92/0Anto92HL/
  HAIRLINE/Brows);身体(CBBE/clothes)是 Effect shader(type 8)或
  3BA 体——纯 BSLighting 过滤会只剩头发。Effect pass 不可弃。

**v5.6 = P 快照回放**:
1. 白名单 P pass(type 6 **与** Effect type 8 全收,回退 0d 的过滤)
   记录调用快照(pass 指针 + technique + alphaTest + renderFlags +
   site)——快照按面板代数重取,关面板清空(几何随 actor 销毁);
2. **菜单括号内 end_frame 逐帧重放快照**:暂停后世界流虽死,快照里
   的 geometry 指针仍有效(actor 存活),引擎 SetupAndDrawPass 会重读
   蒙皮矩阵——P 每菜单帧重画进摄影棚,与物品预览同屏;
3. **每菜单帧清屏**(begin_frame 重置 m_cleared):世界主目标
   (format 10)与菜单 UI 合成目标(format 28)是**不同资源**,清菜单
   目标不会抹世界相位像素;不清才是残影根源(用户发现)。
4. 透传抑制与白名单仍限世界流(菜单流物品不变);合成 HDR tone-map
   保留(HDR 世界目标采样仍需)。

## 0d. Run 31 结果(route 3 首轮:核心命题初步成立 + 三缺陷)

v5.4 会话(2026-10-01 23:54,用户截图 + 日志 + TGA 三证):

- **正面:面板第一次显示人形**——用户截图(世界画面中按 F7)与 TGA
  proto-pass-150 判读一致:头/躯干/手臂/手持武器的完整轮廓,蒙皮件
  (CBBE/clothes/FemaleHeadNord/Feet/0Anto92 头发等,BSLightingShader
  type 6)确实过重放路径,**M0 核心命题(蒙皮过重放)初步成立**;
- **缺陷 1(花屏噪声)**:摄影棚目标跟随调用点格式,世界流 = HDR
  主目标(format 10 R11G11B10),合成四边形原样采样输出,且 P 的
  Effect-shader pass(衣服/眼睛/武器,type 8)与 BSLighting pass 互相
  覆盖 → 噪声。**修复**:只重放 type 6(与菜单侧同一过滤器);合成
  PS 对 HDR 目标做 Reinhard tone-map(CB y 位携带 HDR 标志,动态 CB);
- **缺陷 2(只在世界中第一次有效)**:`p_passes` 只出现在世界窗口会
  话,背包内会话为 0——停靠点固定在世界一处,玩家转身/移动后 P 出视
  锥被剔除,零 pass。**修复**:kAttached 状态每帧重新停靠(pump 保持
  给 kAttached 供步,P 停到玩家正下方 Park_Depth)——玩家在视锥中心,
  其正下方必然在视锥内;
- **缺陷 3(画面里的巨大杂物)**:P pass 列表含手持件(Scb 箭袋/
  Torch/武器),静态网格按挂点世界变换重放 → 满幅大块。**修复**:
  白名单收紧为**蒙皮件**(`skinInstance` 非空才收)——身体/盔甲/头发
  正是面板要的角色,箭袋火把等全数排除。
- 另证:`mirrored: 2`(vs 首轮 6)说明玩家装备状态在会话中不同,属
  正常(镜像的是当下穿着)。

## 0c. Run 30 结果(route 2 + R3 白名单 → **证伪**,引擎事实升级)

v5.3 首轮(2026-10-01 23:39 会话):attach 反复成功、生命周期零残留,
但全程**零 `P pass` 行、`p_passes=0`**——白名单也没接住。**P 的 pass
根本不存在**:图被摘出世界后,世界剔除链看不见它;挂在 menuObjects 下,
菜单剔除器私有队列也不收它(它按自己的收集列表工作,run 4–8 的
accumulator 证伪早已暗示这条链的封闭性)。两个剔除链都到不了的图 = 零
pass。另:run 29 判读中"P 在世界流被渲染"系误判——用户更正截图右上角
人形由 Show Player In Menus 绘制,与本插件无关。

**引擎事实链(三轮修正后)**:
1. 菜单场景几何收集走 culler 私有队列,不走场景图挂接(§0b);
2. 被世界摘除的图没有任何剔除链到达(§0c,本轮);
3. **世界剔除链是唯一已知会为 P 产生 pass 的链**(spike 时代 + 本轮
   推断),而世界 pass 与菜单 pass 共用同三个 `RenderPassImmediately`
   调用点。

**Route 3(v5.4)**:P 保持**世界 actor**不摘图——着装后停到玩家下方
8000 单位(远平面 20480 之内,世界剔除器持续为它发 pass;普通游玩相机
俯仰远达不到看见它),白名单(v5.3 已就位)接世界流 pass,**透传抑制**
(白名单命中且括号外 → 不调 original):世界画面不画 P(PRD 0.5 零
污染),pass 只进摄影棚。退役列表机制随摘图路线一并退役(route 3 无
场景图手术)。

## 0b. Run 29 结果(route 2 全链路走通,但 P 的 pass 在世界流)

v5.2 首轮(2026-10-01 23:27 会话):完整成功序列反复出现(clone placed
→ mirrored: 6 → dressed and parked → **attached**,detach/kill/release
生命周期零残留),**但**:

- 日志零 `p=1`、`menu_passes` 不因 P 上涨——P 的 pass 从未进入菜单
  括号;
- TGA 面板矩形内容与物品预览完全同位(bbox 1484,412–1676,648),零
  人形;
- 用户截图:**P 被渲染了——在屏幕右上角的世界画面区域**(半身、蒙皮
  件无盔甲),不在面板里。

**结论:菜单场景的几何收集不走场景图遍历**,走 culler 私有队列
(capture report 附录 2 的 AppendVirtual +0x140 队列早已证实);把外来
图挂进 menuObjects 不会让它产生菜单 pass——它仍被引擎当世界对象渲染
(actor 渲染注册未摘)。方案 §3 的 F3 变体命中,且比预想深一层。

**修复 = 回退 R3 转正(v5.3)**:不追求 P 进菜单收集列表——P 的世界
pass 本来就经过同三个 `RenderPassImmediately` 调用点(thunk 都看得
见),只是被 `m_in_frame`(菜单括号)拦住。v5.3 改为:

1. `on_pass` 双路接收:括号内菜单 pass(原有)+ **括号外 P 白名单
   pass**(root 祖先匹配 + 面板开着闸门;面板关 → P 不存在 → 零额外
   重放,普通世界 pass 永不匹配 P root → 世界流零污染);
2. **清屏节律重排**:引擎每帧先世界后 UI,P 的世界 pass 先于菜单括号
   到达——`begin_frame` 重置 `m_cleared` 会把已画的 P 每帧清掉。改为
   帧尾(end_frame)重置:世界阶段首个 P pass 清屏,菜单阶段物品叠画
   不清,一帧一图(P + 物品);
3. `p_passes` 帧计数 + `P pass` 发现行(in_menu_frame 标记)+ 
   `m_p_total_replays` 并入关面板取证闸门(P-only 会话也出 TGA)。

## 0a. Run 28 结果(route 2 首轮:前半走通,节奏 bug,F8 兜底生效)

v5.1 首轮(2026-10-01 22:53 会话)日志:
`clone placed` → `body-worn items mirrored: 6` → `dressed and disabled`
→ 紧接着 `build timeout (1201 frames)`——超时在着装后 **~1 ms** 触发。
两个发现:

1. **假帧 bug(实现错误,非引擎问题)**:状态机用 SKSE TaskInterface
   自续任务当"逐帧"节拍,但任务队列在**同一游戏帧内排空**——1201 个
   tick 约 4 ms 连跑,假帧数撑爆超时,引擎从未得到真实时间异步加载
   biped 3D。**修复:节拍改由渲染线程泵**——DrawInterfaceStart detour
   每真实帧 pump 一次、至多排一个游戏线程步骤任务;超时按真实帧重算
   (1200 帧 ≈ 60 fps 下 20 s)。
2. **Disable 疑似拆 3D**:着装后立即 Disable 可能导致引擎拆掉蒙皮图,
   Get3D 永远拿不到。**修复:不 Disable**,停靠位 z=-100000(远超相机
   far plane 20480)保证不可见;图摘走后壳 actor 直接删除。

宽限期、穿着镜像(6 件)、停车、F8 兜底杀壳全部按设计工作。

## 0. Run 27 结果(route 1:场景图深拷贝 → **证伪**,F5)

v5 首轮(2026-10-01 22:28 会话):每次开面板
`Proto P spawn failed: CreateDeepCopy returned no NiNode (plan F5)`——
对玩家 3D 图调用引擎通用深拷贝(`NiObject::CreateDeepCopy`,ID
68839/70191)返回的不是可用节点:角色图的动态类(BSFadeNode/
SkinnedMesh 一族)对通用 `NiCloningProcess` 没有可用的 Clone 重写。
非崩溃、无残留(处置路径按设计工作);F5 的回退是"摘碰撞重试,仍崩→
R2",这里不是崩溃而是**静默失败**,直接按 ladder 升级。

**Route 2 = R1 的 spike clone-actor 变体,已实现(v5.1)**:

- `CreateDuplicateForm` 复制玩家 base(faceNPC 指回玩家)→
  `PlaceObjectAtMe` 放置克隆 actor → 宽限 60 帧(spike 实测的次级基
  窗口)→ 停移动+穿着镜像(spike 顺序:先容器后 EquipManager)→
  `Disable`;
- 取克隆 actor 的第三人称 3D **整图** `DetachChild` 出世界、
  `AttachChild(graph, kInventory)` 挂进 menuObjects[1]:蒙皮骨骼引用
  随图整体迁移,**无需重绑**;世界不再渲染它(不在任何剔除集)、
  动画更新不再到达它(不在世界更新链)——固定姿态即 M0 要求;
- 摆位自标定不变(物品几何世界坐标 + 朝 UI3D 相机);
- 生命周期新增 F8:构建中途关面板(despawn 早于宽限期结束)时,杀
  actor 推迟到状态机走完宽限期(kKillPending),防未graced虚函数崩溃;
  构建总超时 1200 帧兜底。退役列表(F4)不变。
- game-thread tick 用 spike FrameTick 模式(TaskInterface 自续任务)。

## 1. 目标与通过判据(不变)

把玩家 3D 场景图的一个独立副本（P）挂进菜单 3D 场景，使面板显示的是
完整角色而非物品预览。PRD M0 要求"可先固定姿态，但必须使用独立展示
实例"，核心风险是**蒙皮几何能否走通既有 pass 重放路径**。

通过判据（全部满足 = M0 完整收官的蒙皮风险关闭）：

1. 面板内容是 P（完整角色模型），不是物品预览——TGA 取证可见人形；
2. 蒙皮几何（身体/盔甲）着色正确、无 T-pose 碎裂/坐标飞出/崩溃；
3. P 的 pass 出现在重放日志中（`geom=…` 发现行），且**只**被重放进
   私有目标，原画面零改动（与 run 26 一致的世界完整性）；
4. 反复开关面板 + 读档，无残留、无崩溃（FR-06 与生命周期复验）。

## 2a. 技术路线 1(已证伪,留档):场景图深拷贝

**不采用** spike 的真实世界 Actor 克隆(PlaceObjectAtMe):当时判断
世界场景的 pass 不发生在菜单帧的 DrawInterfaceStart 括号内,且带
AI/物理全套初始化风险——这个判断对"直接用世界 actor"是对的,但
route 2 证明 actor 的 **3D 图**可以摘进菜单场景,actor 本身只需作为
装配机。深拷贝路线(玩家 3D → `CreateDeepCopy` → 挂 menuObjects)在
run 27 被证伪:F5,见 §0。

## 2. 技术路线 2(当前):clone-actor 装配 + 整图挂接

见 §0 的 run-27 修订。CLib/引擎事实依据:
`CreateDuplicateForm`(spike 已验证)、`PlaceObjectAtMe`(spike 已验证)、
`AttachChild(obj, kInventory)`(CLib ID 51859/52731,挂进
menuObjects[1] = 物品预览 root,run 6 census 证实该 root 的几何被祖先
识别命中)、`DetachChild`(ID 51861/52733)。

## 3. 失败模式与回退（handoff 硬性要求）

| # | 失败模式 | 症状（日志/TGA 可辨） | 回退 |
| --- | --- | --- | --- |
| F1 | **深拷贝后 skinInstance 骨骼指针仍指向 O 的骨骼**（bones 是 `NiAVObject**` 裸指针数组；CreateDeepCopy 的 cloneMap 对骨骼节点重映射失败或部分失败） | P 不跟随摆位变换（贴在 O 身上渲染）、T-pose、或渲染线程读悬垂骨骼崩溃 | 实现显式骨骼重绑：遍历 P 的每个 `BSGeometry`，`GetGeometryRuntimeData().skinInstance`，把 `bones[i]` 重指向 P 图内同名节点（`GetObjectByName`），`rootParent` 重指向 P 根；重绑后仍失败 → 回退 R1 |
| F2 | **蒙皮 pass 重放崩溃或输出错乱**（蒙皮顶点着色依赖 per-draw 骨骼矩阵寄存器，重放时引擎 SetupGeometry 重算依赖 frameID/脏标记） | 重放调用点崩溃；或 TGA 中蒙皮件消失/黑块/碎面 | 先确认非 F1；再关闭蒙皮件重放（on_pass 按 geometry 是否带 skinInstance 分流），P 只出非蒙皮件 → 证据降级记录，回退 R2 |
| F3 | **P 的 pass 不被识别**（挂接的 root 不是重放匹配的 menuObjects 指针，或 AttachChild 实际挂进别的节点） | `menu pass` 发现行里没有 P 的几何名；面板仍是物品预览 | 日志比对 root 指针与 begin_frame 快照；改用 `AttachChild(obj, kInventory)` 显式 scheme（若初版用的无 scheme 重载）；仍不行 → 回退 R3 |
| F4 | **生命周期悬挂**（渲染线程在途 pass 持 P 几何指针时游戏线程销毁） | 关面板/读档后偶发崩溃 | 摘除（DetachChild）与销毁（延迟数帧 + 在渲染线程 non-bracketed 帧确认无 in-flight）分离；ClosePanel 只摘不删 |
| F5 | **深拷贝本身不稳定**（玩家 3D 带控制器/碰撞对象，克隆期崩溃） | 开面板即崩，栈在 CreateDeepCopy/ProcessClone | 拷贝前摘 collisionObject（克隆副本上置空）；仍崩 → 回退 R2 |

## 4. 风险与首轮裁剪

- **R1（回退方案 1）**：改用**原生装备装配路径**重建 P——新建哑元
  actor（不 PlaceObjectAtMe，或最低限度放置后立即 Disable 停 AI），用
  engine 的 biped 装配（Apparel Preview 参考，community-reference-
  supplement §4）生成 3D，再深拷贝其图。成本高，仅当 F1/F5 无法
  修时启动。
- **R2（回退方案 2，最保守）**：P 的蒙皮件不重放（F2 分流常开），
  面板先显示非蒙皮件 + 蒙皮件占位；蒙皮兼容性作为独立后续验证轮。
  M0"独立展示实例"仍达成（P 存在且走重放），蒙皮完备性延后。
- **R3（回退方案 3）**：放弃场景图挂接，P 的 pass 靠名字/用户数据
  白名单识别（is_menu_geometry 之外加一条 P 根判定），不依赖祖先
  匹配。仅在 F3 且挂接方式无法修正时使用。
- 蒙皮重放每帧 CPU 成本未知；正式性能测量本就在阶段 2 首轮一起做
  （PRD §5.3），首轮记录帧耗时粗值即可。

## 5. 验证轮步骤（游戏内，按 runbook）

1. 构建部署后进存档，背包菜单开面板（F7）——预期：面板出现 P 人形
   （或按 F2 分流的非蒙皮件），日志出现 P 几何的发现行（`p=1` 标记）；
2. 关面板（F7）——预期：摘除日志 + 证据 TGA；重复开关 ≥5 次；
3. F8 中途抓帧 + 关面板 TGA 判读（python 解 TGA，判据同 §1）；
4. 开面板状态下存档 + 读档——预期 kPreLoadGame 强制关闭（顺带补
   m0-acceptance 开放项 1）；
5. 记录本轮 CPU/GPU 粗值（心跳帧率行），正式测量另行安排；
6. 全部判据通过 → 更新 m0-handoff/m0-acceptance，M0 完整收官评估；
   任何失败模式命中 → 按 §3 回退列执行并记录。

## 6. 当前决定记录

- 用户已决定：INI 配置推迟；面板仅在菜单界面显示（世界对照推迟）。
- 本轮实现遵循"先方案后动手"：本文档先于实现提交。
