# M0 handoff — state, verified facts, and the next work package

- 日期:2026-10-01(run 19 收官:工作包 1+2 完成并经游戏验证)
- 读者:下一个会话的 Agent / 开发者。本文是"从这里继续"的入口;运行级细节在
  [m0-proto.md](m0-proto.md),探针证据在
  [m0-capture-report-2026-09-28.md](m0-capture-report-2026-09-28.md),
  需求基线在 [player-panel-prd.md](player-panel-prd.md)。

## 当前状态:M0 gate 已通过

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
推迟);**当前阶段 = 2(独立展示实例 P)**。
DLL 已部署 `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\`
(v4.6,MD5 6f00f395…)。热键 F7/F8。v3.1 已提交(770e618);v4 系列
已提交(f15fb1a)。

**阶段 2 进行中(2026-10-02,run 27–33 + v5.7 build)**:
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
- 方案与证据链:[stage2-p-instance-plan.md](stage2-p-instance-plan.md)
  (§0–§0ad 三十一轮留档、§2 路线、§3 失败模式)。

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

## 代码地图

| 文件 | 内容 |
| --- | --- |
| `tools/m0_proto/proto_passredirect.cpp` | 全部核心(v3.1):PassRedirector(install 三调用点钩子 + 常驻 begin/end_frame 括号 + on_pass 识别[每面板一次发现式日志,含 `p=` P 标记] + `replay_after_original` 重放)、OffscreenTarget(私有 color+depth+depth-on 状态,常驻、desc 变化重建、FR-06 渲染线程释放)、DrawInterfaceStart Detours detour、关面板取证/F8 一次性 TGA 读回 |
| `tools/m0_proto/proto_pinstance.cpp` | 阶段 2 独立展示实例 P(v5.1,route 2):clone-actor 装配(CreateDuplicateForm→PlaceObjectAtMe→60 帧宽限→穿着镜像→Disable→整图摘挂 menuObjects[kInventory]);状态机 tick(kWaitingGrace/kWaiting3D/kAttached/kKillPending)+1200 帧超时;退役列表(游戏线程只摘除,渲染线程非括号帧释放,F4);is_p_geometry 供重放日志标记 |
| `tools/m0_proto/proto_main.cpp` | SKSE 导出、运行时门禁、输入 sink(F7 开关面板 / F8 一次性导出)、消息处理(kDataLoaded 安装;kPreLoadGame/kNewGame 强制关面板);F7 开关路径同步驱动 P 的 spawn/despawn |
| `tools/m0_proto/proto.h` | Proto 类接口(面板开关/导出/失效/代数/P 开关) |
| `docs/stage2-p-instance-plan.md` | 阶段 2 验证轮方案:技术路线、失败模式 F1–F5 与回退阶梯 R1–R3、验证步骤 |
| `tools/m0_probe/` | 探针(F7/F8 抓取),已完成使命,保留作证据工具 |
| `src/` | 旧 spike(pass_hook/clone_actor),pass 钩子机制的出处 |

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
# 构建(vcpkg 锁被占会卡在 "Running vcpkg install",先清掉残留 vcpkg 进程)
cmake -S . -B build `
  -DCHARACTER_PANEL_BUILD_PROBE=ON -DCHARACTER_PANEL_BUILD_PROTO=ON `
  -DCMAKE_TOOLCHAIN_FILE="D:/Microsoft Visual Studio/2022/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows-static-md
cmake --build build --config Release --target CharacterPanelProto --parallel 4
```

- 产物 `build/Release/CharacterPanelProto.dll`,**手动**装入 MO2
  (本机 `E:\SkyrimAE\mods\...`,MO2 profile `AE`;直接放真实 Data 会被清)。
- 游戏内(v3.1 面板流程,详见 tools/m0_proto/README.txt):**取证动作就是
  关面板**——进存档 → 背包高亮有 3D 模型的物品 → F7 开面板 → 停一两秒 →
  F7 关面板,渲染线程自动写一张 `proto-pass-NNN.tga`(先导出后释放)。
  可选:F8 在面板开着时中途抓帧(关着按无效并告警);普通画面开面板几分钟
  等心跳行(约 30 秒一条);开面板状态下存档/读档确认强制关闭(无 dump 行)。
- 采集:`Documents/My Games/Skyrim Special Edition/SKSE/CharacterPanelProto.log`
  + `...\SKSE\CharacterPanelProto\proto-pass-NNN.tga`(2560x1440 RGBA TGA)。
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
