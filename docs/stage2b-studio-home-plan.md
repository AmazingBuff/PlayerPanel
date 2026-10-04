# 阶段 2b 验证轮方案:摄影棚即居所(统一之家,route 2 复活)

- 日期:2026-10-04;状态:**工作包收官(v6.60–v6.65,runs 94–99;轮 1
  轮 2 teardown 全部验证通过,代码已提交)**
- 上游:[m0-handoff.md](m0-handoff.md) 阶段 2 + [stage2-p-instance-plan.md](stage2-p-instance-plan.md)
  §0ai(用户 2026-10-03 提议,决策记录:"先验证 v6.25 泊位形态;通过后
  把'摄影棚即家'立项为下一个工作包"——v6.25 已于 run 59 验证通过)
- 读者:实现与验证此轮的 Agent / 开发者
- 前置基线:v6.59(run 93 已验证:面板光照正确、世界无泄漏、高亮/无
  高亮一致;DLL MD5 5ad6742d,提交 743454b)

## H7. Run 99 结果(v6.65:teardown 首演两次全过——工作包收官)

run 99 会话(2026-10-04 20:01,单会话,v6.65,MD5 40eeb15a…):

- **读档 teardown 首演**(20:02:30,kPreLoadGame 路径):`whitelist
  disarmed` → `clone actor killed` → `home released: graph detached from
  CP_StudioHome and freed` 三段完整执行,零崩溃;重出生(20:02:31)→
  重建 → 再迁移(新 root 0x286373b1300)→ 面板照常(TGA 300/301);
- **主菜单 teardown 首演**(20:02:55,v6.34 检查路径):同三段 +
  `Proto P panel closed: main menu open`,零崩溃;
- **v6.65 清扫修正生效**:sweep 噪音 0 条;全场 [W]/[E] 零条;
  6 轮面板 6 张 TGA、153 帧 submitted 全健康。

## 收官结论

**"摄影棚即居所"工作包完成**(v6.60–v6.65,runs 94–99,提交
2412891 / ce0f654):P 的 3D 图常驻菜单场景私有节点
CP_StudioHome(menuObjects[0] 下,NiPointer 强持有),壳 actor 在迁移
同 tick 删除,世界侧从构建窗口结束起结构性零克隆——FR-06 从五层防御
(幽灵化/泊位争霸/簿记争抢/清扫/存活检查)变为"世界不存在这个引用"。
过程中证伪并关闭:设备缓冲 census 仪器(run 54/94)、世界流 pass 门控
(run 95)、"世界必须先渲染"前提(U1,run 95 反证)。

**移交给后续工作包**:
- 正式 CPU/GPU/显存测量(PRD §5.3)——仍独立,未做;
- 灯光与世界光照一致性、亮度/色温微调——非阻塞,用户明示后续;
- 阶段 3(M1):装备与外观同步(FR-01)、临时污染过滤(FR-02)、
  待机动画(FR-03)、面板布局定稿(**需用户输入**)。
- 注:m0-acceptance 开放项 1("开面板状态下存档+读档强制关闭")的
  组成路径已分别验证(run 99 读档 teardown + 各轮 close_panel),精确
  场景(面板开着读档)未单独复跑,风险极低。

## H6. Run 98 结果(v6.64:轮 2 核心命题闭环——壳死图活;遗留 teardown 首演 + 清扫噪音)+ v6.65

run 98 会话(2026-10-04 19:53,单会话,v6.64,MD5 44cd4135…):

- **壳死图活成立**:迁移 + 杀壳同一 tick(19:53:52.428,三条日志:home
  迁移 / clone actor killed / round 2 shell deleted),其后 **13 轮开
  面板、338 帧 submitted=14/14、13 张 TGA** 全部正常(至 19:54:11)——
  图在壳死后 20 秒仍在渲染,TGA 与基线像素级一致。**U4(杀壳安全性)
  关闭:Disable 无 3D 可毁,refcount 持活成立**;
- re-homed 零条;零 crash;**teardown 未演练**(本场无读档/无回主菜
  单,despawn 路径零执行)——用户暂缓,仍是开放项;
- **清扫噪音**:SetDelete 后引擎不立即把 ref 移出 cell 引用表,而
  m_clone 已清空 → 清扫每 300 帧重复"删除"同一壳 ref(0xFF0063BD,全
  场 4 次,唯一 [W] 来源)。无害但须修。

**v6.65(2026-10-04,待游戏验证)**:sweep_stale_clones 跳过
`IsDisabled()/IsDeleted()` 的引用(自家被杀壳体与已扫克隆都是 inert;
遗留存档克隆加载时是 enabled,不受影响)。

**轮 2 验证余项(run 99)**:读档/回主菜单/退出 → despawn teardown
首演(`whitelist disarmed` → `home released` → 图释放;零崩溃)。

## H5. 轮 2:v6.64 = 杀壳 + ghost 层退役(含对 §3 轮 2 清单的范围修正)

**前提变化(run 96/97 教训)**:原 §3 轮 2 清单写"退役泊位全家 + AI 钉
桩 + grace 期早期泊位"。但 run 96 证明构建窗口(place → grace → dress
→ relocate ≈ 1.5 s)内壳 actor 必然活着且其 3D 在世界——早期节点泊位
(718-731)正是 v6.38 修"读档/出生闪现"的手段,泊位与钉桩是该窗口内
防交互/防推挤的唯一杠杆。**修正后清单**:退役 = ghost 层全家;保留 =
泊位、钉桩、早期泊位(它们只服务构建窗口,壳死后自然失活)、清扫、
菜单帧抑制、白名单。

**v6.64(2026-10-04,待游戏验证)落地内容**:

1. **迁移同 tick 杀壳**:`relocate_home` 末尾(data3D 已斩、图已安家、
   `m_home=kMenuHome` 已发布)调 `kill_actor()`(Disable+SetDelete)。
   安全性构造性成立:Disable 无 3D 可毁(run 51 机制失效),图由
   NiPointer + holder 槽位持活。
2. **pump 存活检查再分流**:homed 状态下壳已死(m_clone 空句柄),旧判
   据 `!live → despawn` 会每帧误触发——homed 分支完全跳过 ref/Get3D 探
   针,只做 verify_home;构建窗口(未 homed)判据原样。
3. **ghost 层全家退役**:`ghost_should_suppress`、`m_ghost_warm/
   warmup/first_drop`、`Ghost_Warmup_Passes`、`m_last_pass_ghost`、
   `should_suppress_passthrough` 的 `|| ghost` 子句、on_pass 的 ghost
   分支、spawn 重置块。`is_p_descendant` 保留(is_p_geometry 的祖先链
   半边)。菜单帧 p_geom 抑制(v6.19,run 53 崩溃防线)不动。
4. 轮 1 提交切点:2412891(v6.60–v6.63,runs 94–97)。

**判读(run 98)**:①日志出现 `Proto P round 2: shell actor deleted at
relocation; the world holds no clone` 且其后人物照常绘制(核心命题:
壳死图活);②第三人称全程零替身、E 键无交互、无推挤(泊位退役前的
构建窗口也只发生在读档后 ~1.5 s);③面板/TGA/parent ok 全不回归;④
**读档回归**(despawn teardown 首演):载入存档 → 强制关面板 → 重
spawn → 迁移 → 再开面板,零崩溃——这是 homed 图 teardown 路径的首场
验证;⑤退出游戏零崩溃(kShutdown/kExitGame 路径同样走 despawn)。

## H4. Run 97 结果(v6.63:轮 1 全部判据通过——图迁入菜单场景,验证闭环)

run 97 会话(2026-10-04 19:32,单会话,v6.63,MD5 a394fe14…):

- **乒乓归零**:`re-homed` 警告 0 条(run 96 为 707 条)——data3D 斩断
  生效,壳簿记不再争抢父级;全场 [W]/[E] 零条;
- **迁移照旧成立**:白名单武装后 19 ms 内 home 迁移,
  `home_world=(0.00,0.00,0.00) scale=1.000`(U5 复确认);
- **U2 复确认**:10/10 次开面板 `P home parent ok (per-open check)`;
- **绘制从 home 图**:10 轮开关,378 帧 `submitted=14 of 14`(passes
  回到 14——run 96 的 15 是乒乓期状态污染);
- **取证恢复**:10 次关面板 10 张 TGA(273–282);判读:人物居中
  T-pose,bbox=(1068,311)-(1470,1125) 与 run 95/96 基线像素级一致
  ——**首批从 home 图渲染的面板内容成立**;
- 零 crash。轮 1 判据全过,代码提交 2412891。
- 遗留:despawn(读档/退出)对 homed 图的 teardown 路径本场未演练
  (用户暂缓存档项)——与轮 2 验证/存档回归并验。

## H3. Run 96 结果(v6.62:迁移成功 + U5 恒等成立;壳簿记每帧抢图乒乓;TGA 被既有门跳过)+ v6.63 方案

run 96 会话(2026-10-04 19:10,单会话,v6.62,MD5 31f87dce…):

- **迁移成功且时机符合预期**:白名单武装(19:10:32.386)后 19 ms 内
  `Proto P home: graph relocated under CP_StudioHome — root=0x…(unchanged)
  old_parent=[] home_world=(0.00,0.00,0.00) scale=1.000`——**U5 判定通
  过**(menuObjects[0] 父链恒等,pose 数学原样成立)。无门控直迁策略成
  立:不依赖面板、不依赖任何传感器。
- **新缺陷:父级乒乓**——`P home parent was detached; re-homed` 每帧一
  条,全场 707 次。机制:壳 actor 仍活着,其 3D 簿记(被 tick 的每帧
  warp 泊位喂着)每帧把图重新挂回世界——引擎的"活体 actor 的 3D 属于
  世界场景"不变量在逐帧强制执行。绘制不受影响(draw 与父级无关,
  submitted 356 帧、composite 每帧在跑[日志节流]),但这是逐帧场景图
  拔河,必须斩断。
- **TGA 仍缺失(第二个门)**:dump 路径走了(`studio target released
  (panel closed)` 证明 a_dump_evidence 生效),但 `dump_and_release` 的
  既有门槛 `m_session_replays > 0` 在无高亮的 P-only 会话恒为 0 → 跳
  过。复合正常(节流日志,无异常)。

**v6.63(2026-10-04,待游戏验证)= 斩断簿记 + dump 补门**:

1. **relocate_home 摘图前置空壳的 `loadedData->data3D`**(纯数据写,与
   v6.38 grace 代码同款手法,零虚函数):引擎簿记失去图的引用,无从争
   抢;壳变成"正常的远处无 3D actor"(引擎对未加载 3D 的 NPC 原生维
   持的状态)。**附带收益:轮 2 的杀壳由此变得构造性安全**——Disable
   再无 3D 可毁,run 51 机制失效。
2. **verify_home 警告加父诊断**(parent 指针/名字):若乒乓仍发,日志
   直接指认抢走方。
3. **dump 门加 `m_p_drew_this_open`**:主动绘制提交过 pass 即视为摄影
   棚有内容,与 `m_session_replays > 0` 并联——P-only 会话也写取证。

**判读(run 97)**:①迁移行照旧(恒等);②**零条 re-homed 警告**(乒乓
消失 = data3D 斩断生效;若仍发,父诊断指认新抢主);③关面板写 TGA
(日志 dump 行 + 目录新文件),TGA 判读:人物居中 T-pose 与基线一致;
④面板/开关/多类菜单零回归;⑤零 crash;⑥第三人称无替身(数据被斩断
后壳无 3D,更应不可见)。

## H2. Run 95 结果(v6.61:零回归再证;ghost 门控零武装;U1 被正面回答)+ v6.62 方案

run 95 会话(2026-10-04 18:55,单会话,v6.61,MD5 1430be37…):

- **零回归再证**:16 轮开/关面板,411 帧 `submitted=14 of 14
  (source=live)`,零 crash,全场唯一 [W] 仍是迁移放弃行。
- **ghost 门控零武装**:`relocation gave up after 600 frames (ghost
  warmup never armed)`——整场**零条** ghost 行(run 94 里 0.2 s 即武装)。
  世界流 P pass 的到达依赖玩家视角对泊位的剔除收录:run 94 的菜单帧恰
  好收,run 95 全场没收。**世界 pass 流是视角依赖信号,不可作门控**。
- **决定性交叉证据(U1 正面回答)**:ghost 零计数 = 世界整场从未画过
  克隆体;而摄影棚对同一张图成功绘制 411 帧(引擎 SetupAndDrawPass 链,
  人物像正常——用户两轮实测无异常报告)。**结论:引擎
  SetupAndDrawPass(即摄影棚的 call_site_original)自己惰性初始化设备
  缓冲,"世界必须先渲染一遍"的前提死亡**。此前支持该前提的 run 50 判
  读依赖的正是已证伪的 census 仪器 + 手绘路径自身的 rd 门(v6.13 时代,
  与 SetupAndDrawPass 无关)。
- **附 Bug:关面板取证 TGA 自 v6.39 失效**:close_panel 从不设
  m_dump_on_close(F7 路径才有),MenuSink 的用户关闭走静默释放——今
  天两场零 TGA 即此因(F8 手动抓帧不受影响)。

**v6.62(2026-10-04,待游戏验证)= 删门控直迁 + 取证恢复**:

1. **迁移门控整个删除**:`try_relocate` 在 kAttached 后第一个未暂停
   tick 直接迁移(白名单已武装、根非空即足够)。保留的 fail-open:宿主
   不可用/节点创建失败 → kStuckParked。重试计数器与 Relocate_Retry_
   Frames 一并退役。
2. **close_panel 增 a_dump_evidence 参数**:MenuSink 用户关闭传 true
   (恢复 v3.1"关面板=取证"契约);强关(读档/主菜单/世界丢失)保持
   静默——与渲染端既有语义(user close writes one evidence TGA, a
   force-close releases silently)对齐。

**判读(run 96)**:①进存档等自动出生,**不依赖面板**,attach 后约一
两秒内出现 `Proto P home: graph relocated…` 且 `home_world=
(0.00,0.00,0.00) scale=1.000`(U5 判定);②第一次开背包面板:人物像
与 v6.59/run 94 基线一致,`P home parent ok (per-open check)` 出现;
③关背包后日志出现 `dump_and_release`/TGA 写出行(取证恢复验证);
④第三人称:替身不可见;⑤开关面板 ×5 + 容器/地图/技能菜单回归;⑥
零 crash。TGA 判读:人物居中、T-pose、non-black bbox 与 run 95 时代
量级一致。

## H1. Run 94 结果(v6.60 首验:零回归成立;迁移被证伪的 census 探针挡住)+ v6.61 方案

run 94 会话(2026-10-04 16:26,单会话,v6.60,MD5 4d7a427a…):

- **零回归成立**:spawn→attach 正常;背包面板开/关 8 轮,387 帧
  `submitted=14 of 14(source=live)`、composite 正常、ghost 预热武装
  (128 passes)、关面板干净释放;全场唯一 [W] 是迁移放弃行,零 crash。
- **迁移未发生**:`relocation gave up after 600 frames (device buffers
  0/14)`——census 门控整场不满足,fail-open 按设计兜住(面板全程从
  泊位图绘制,与 v6.59 行为一致)。
- **根因 = census 是坏仪器**:心跳 `P=0/14 player=0/23`——玩家图显然
  每帧在渲染也是 0,run 54 的证伪结论原样复现(rendererData/
  vertexBuffer 普查不反映真实初始化);而 `submitted=14/14` 每帧成功证
  明绘制链全通。门控逻辑正确,传感器错了。

**v6.61(2026-10-04,待游戏验证)= 门控换信号**:迁移门控从 census
换成 **ghost 预热窗武装(`m_ghost_warm`)**——run 50 语义:每个到达的
世界流 pass = 一次完成的引擎真实绘制,真实绘制是 rendererData/VB/IB
唯一创建者;武装 = ≥128 次真实绘制 = 缓冲就绪的项目自证信号(run 94
实测:面板打开后 0.2 s 内武装)。census 降级为 give-up 时的诊断行,
attach 时的 `renderer init check` 保留为参考行;**init_heartbeat(同
一坏仪器)顺带退役**(方案 §2 表格既定项)。注意:tick 暂停不推进,
所以迁移落在武装后的**第一个未暂停 kAttached tick**——预期流程 =
开面板(照常,泊位图)→ 关面板 → 迁移发生 → 再开面板(从 home 绘
制)。

**判读(run 95)**:①首个面板关闭后日志出现 `Proto P home: graph
relocated…` 且 `home_world=(0.00,0.00,0.00) scale=1.000`(U5);②第二
次开面板出现 `P home parent ok (per-open check)`,人物像与 run 93/94
基线一致;③`source=live` 保持;④关面板/读档/退出零崩溃;⑤迁移帧
`relocating` 闩跳过绘制无可见异常。

## 0. 一句话目标

把克隆 actor 的 3D 图从世界摘出、挂进 UI3D 菜单场景的专用根、杀掉壳
actor,让"克隆体不可察觉"从五层防御(幽灵化/泊位/钉桩/标记清扫/存活
检查)变成**结构性成立**:世界侧根本不存在这个引用,自然零像素、零
pass、零交互、零存档残留。

## 1. 目标与通过判据

1. **面板零回归**:人物像与 run 93 基线一致(光照/取景/朝向/比例),
   高亮/无高亮一致,稳态逐帧连续绘制;
2. **世界侧零痕迹**:第三人称全程看不见替身、E 键无交互、无推挤碰撞
   (轮 2 后 = 壳已死,结构性成立;轮 1 期间由既有泊位兜底);
3. **图驻留菜单场景**:跨背包开关/容器菜单/其他菜单,图不丢、不重复
   ——每次开面板有 parent 校验日志(`P home parent ok/detached`);
4. **生命周期零崩溃**:读档/新游戏/回主菜单/退出全路径干净,无
   use-after-free,无残留克隆新增(清扫计数不涨);
5. **设备缓冲初始化无回归**:迁移时 census `renderer init check: N/N`,
   无 rd=9 复发(F7 人物成立 = 缓冲健在);
6. **防线退役落地**:泊位/幽灵化世界帧抑制/AI 钉桩代码删除后全部判据
   不回归,FR-06 从逐项防御变结构性成立。

## 2. 现状(代码地图,2026-10-04 工作区行号)

当前 P = 活的世界 actor:玩家 base `CreateDuplicateForm` 副本经
`PlaceObjectAtMe` 放置,60 帧 grace(节点级早期下移 8000 防读档闪现)
→ AI 钉桩 + 泊位 → 穿着镜像 → 15 帧 settle → `attach_graph` 白名单登记
`clone->Get3D()` 裸地址(`m_active_root`)→ kAttached 每 tick 重泊。

| 机制 | 位置 | 本轮命运 |
| --- | --- | --- |
| 状态机 5 态 kNone/kWaitingGrace/kWaiting3D/kAttached/kKillPending | proto_pinstance.h:122-129 | 骨架保留,插入迁移步 |
| 自动出生(未暂停门 + 玩家 3D 存在) | proto_pinstance.cpp:552-561,630-631;proto_main.cpp:145-156 | **保留**(缓冲初始化前提) |
| 装配链 CreateDuplicateForm→PlaceObjectAtMe→穿着镜像 | proto_pinstance.cpp:421-469,579-609 | 保留(轮 2 前段不变) |
| 泊位(玩家正下 8000,4 处调用) | proto_pinstance.cpp:115,120-129,646-647,718-731,764 | 轮 2 退役 |
| AI 钉桩(kMovementBlocked 等 + SetActivationBlocked + StopCombat) | proto_pinstance.cpp:746-759 | 轮 2 退役 |
| 幽灵化(is_p_descendant + 128 pass 预热窗 + fail-open) | proto_pinstance.cpp:94,995-1058;proto_passredirect.cpp:1312-1323,1493-1504 | 轮 2 退役(仅世界帧部分) |
| 残留清扫(sweep_stale_clones,"CharacterPanel_Clone") | proto_pinstance.cpp:951-993 | **保留**(历史存档清理) |
| 存活检查(pump 读 clone->Get3D 判图丢失) | proto_pinstance.cpp:529-546 | **轮 1 必须改写**(摘图后 Get3D 失效会误判 despawn) |
| init_heartbeat(v6.16 探针,已证伪) | proto_pinstance.cpp:796-840 | 顺带退役(run 54 已判死) |
| is_p_geometry 白名单(祖先链比对 m_active_root) | proto_pinstance.cpp:1010-1021;proto_passredirect.cpp:1301,1976,2041 | **保留**(根节点地址在重挂时不变) |
| 菜单帧 P pass 抑制(v6.19,防引擎画登记进 UI3D 累加器的 pass) | proto_passredirect.cpp:1493-1504 | **保留**(主动 GetRenderPasses 的登记副作用与宿主无关) |
| 绘制链 draw_p_proactively(活动 pass 生成 + 配方兜底 + call_site_original) | proto_passredirect.cpp:1810-2368 | **不动**(唯一耦合 = root() 与 is_p_geometry) |
| pose_for_studio(根 local 写入 + 锚点 + 强制级联 + bound 居中) | proto_pinstance.cpp:266-412 | **必改点**(见 §4-U5) |
| 灯光 rig(私有节点挂 menuObjects[0] + ShadowSceneNode 注册 + 泊位制) | proto_passredirect.cpp:1621-1808 | 不动(独立于克隆体宿主) |
| end_frame 唯一绘制合成点 + kFRAMEBUFFER 自建 | proto_passredirect.cpp:1077-1276 | 不动 |
| despawn(白名单 exchange(0) + kill_actor Disable+SetDelete) | proto_pinstance.cpp:907-947 | 轮 1 扩展(图所有权),轮 2 简化 |
| 退役列表(retire list) | drain_retired 空函数 cpp:949 | 已不存在;轮 1 重挂竞态防护用轻量闩替代 |

## 3. 技术路线(两轮制,每轮一个大问题)

### 轮 1:图迁移(壳保留,全部防线原样在岗)

**v6.60 已实现(2026-10-04,MD5 4d7a427a…,待游戏验证)**。落地差异对
方案的映射:迁移步挂在 `tick()` kAttached 分支(泊位+心跳之后), census
门控不满足则逐帧重试(上限 600 帧,超限 fail-open 停留现架构);pump
存活检查按迁移状态分流——已迁移实例跳过 `Get3D` 探针(图已自持有,不
可能悬垂),改为 U2 宿主校验 + 自动 re-home;未迁移实例判据原样保留。
`note_panel_open()` 在 open_panel/toggle_panel 两条开面板路径接入(每开
一次校验日志)。draw_p_proactively 入口检查 `relocating()` 闩。

spawn 与构建链**一字不动**(未暂停世界帧里建成、世界渲染初始化设备缓冲
——run 49/50 教训的顺序前提)。`attach_graph` 末尾(census 之后)新增
**迁移步**(游戏线程,pump AddTask 同一线程,与既有场景图操作同语义):

1. **探针**:记录 `menuObjects[0]->world`(恒等校验)与迁移前 P 根
   world,日志一行;
2. **摘图**:P 根 parent(世界 cell 侧)→ `DetachChild`;引擎先例:actor
   3D 卸载本就是引擎日常操作;route 2 时代 AttachChild/DetachChild(CLib
   ID 51859/51861)已验证可用;
3. **安家**:新建专用持有节点 `CP_StudioHome`(恒等变换)AttachChild 到
   `menuObjects[0]`,P 根挂其下。选 menuObjects[0] 的依据 = 灯光 rig 节
   点 `CP_StudioLightRig` 自 v6.47 挂在同处,跨 runs 81–93 十三轮菜单开
   关未丢(未知数 U2 的既有乐观证据);专用持有层隔离引擎对 root[0] 子
   节点的任何操作,且 P 根地址不变 → 白名单/发现日志/配方缓存全数免改;
4. **所有权**:PInstance 新增 `NiPointer<NiAVObject> m_home_graph` 强持有
   ——引擎侧再无任何挂接也保图不死;这是与 run 51(白名单裸地址悬垂
   崩溃)的本质区别;
5. **级联**:迁移后对持有节点 `UpdateDownwardPass(kDirty)` 一次,之后
   pose_for_studio 每帧的三次强制级联照常工作;
6. **竞态闩**:`m_relocating` 原子标志,迁移窗口内置位,end_frame 的
   draw_p_proactively 入口检查到置位即跳过当帧(非破坏性搬家,实际
   风险远低于既有 kill_actor 路径,闩是双保险);
7. **门控**:迁移仅当 census `renderer init check: N/N` 满员才执行;
   不满员则不迁、下一帧重试(带上限),超限保持现架构并告警
   (fail-open 回现架构)。

轮 1 中壳 actor 保留泊位 + 钉桩 + 幽灵化 + 清扫:全部防线语义不变,唯一
区别是图搬走后世界流不再有 P pass → 幽灵化预热不再武装(fail-open,
无可抑制之物,无害)。壳不可见(图已不在世界),泊位此时只剩防交互
意义。

### 轮 2:杀壳 + 防线退役(轮 1 判据全过后)

1. 迁移步末尾追加**杀壳**:`kill_actor`(既有 Disable+SetDelete)。
   已知风险 = run 51 "Disable 销毁 3D 图"——但那次图的引用仅在白名单
   裸地址;本轮图已被 DetachChild 摘出 + NiPointer 强持有,引擎销毁路径
   即使触达也只能是引用计数递减(持有节点持引用 → 图不死)。**此假设
   是轮 2 的核心实验命题**(U4),探针与回退见 §4/§5;
2. 退役代码删除:泊位全家(常量 + 函数 + 4 调用点)、幽灵化世界帧层
   (字段 + on_pass 分支 + suppress 子句)、AI 钉桩、init_heartbeat、
   grace 期节点级早期泊位(718-731,迁移后闪现窗口天然消失);
3. 保留:残留清扫(历史存档里还有旧克隆)、菜单帧 p_geom 抑制、白名单、
   自动出生未暂停门、存活检查(轮 1 已改写为图持有语义)。

### 存档语义变化

- 轮 1:与现状相同(壳存活,可入存档);
- 轮 2:壳在 spawn 后 ~2 秒内 SetDelete,存档窗口极窄;kPreLoadGame/
  kNewGame 的 despawn 覆盖在途实例;历史存档残留由清扫兜底。**判读项**:
  存档后读档,Confirm 无克隆体、无 "CharacterPanel_Clone" 基 form 泄漏
  增长(存档体积粗判)。

## 4. 未知数与探针(对应 §0ai 三未知数 + 侦察新增两条)

- **U1 设备缓冲初始化(谁创建 rendererData/VB/IB)**:run 49/50 证明
  暂停帧构建 → rd=9 全拒;run 53 证明世界渲染过的几何过
  SetupAndDrawPass 全通。迁移不碰几何本体,缓冲按理随几何走。
  **探针**:迁移时 census N/N(满员才迁)+ 迁移后首轮绘制帧摘要
  `source=live passes=N`(非 recipes/empty)。
- **U2 UI3D 跨菜单是否清除外来挂接**:loadedModels 常驻 + rig 节点
  十三轮存活,均偏乐观;无反证。**探针**:每次 open_panel 校验
  `p_root->parent == 持有节点`(日志 `P home parent ok`,脱离则
  `detached` + 自动 re-home:从 NiPointer 重新 AttachChild,re-home 后
  绘制正常 = 可接受降级;若引擎连对象都销毁,NiPointer 仍保活,同样
  re-home)。多类菜单实测:背包/容器/炼金/地图/技能。
- **U3 摘挂与在途 pass 竞态**:F4 历史教训。迁移是游戏线程 pump 任务,
  与既有 spawn/despawn 场景图操作同线程同语义;非破坏性(只改 parent
  链,几何/属性指针全部不变)+ `m_relocating` 闩双保险。**探针**:反复
  开关面板 ×20 + 迁移与面板开关交叠,零崩溃。
- **U4 杀壳安全性(轮 2 核心,侦察 F17 实锤的对立证据)**:代码库全部
  记录行为是"图随壳亡"(run 51 Disable 崩溃);本轮命题 = "先摘先持
  后杀"打破该耦合。**探针**:轮 2 首轮专门盯迁移+杀壳后 60 秒内崩溃
  与日志 `P shell killed, graph alive`(杀壳后下一帧绘制照常 = 命题成
  立);失败模式 H1。
- **U5 pose 坐标域(pose_for_studio 的父级假设)**:现假设根父级 = 世界
  cell 根(local==世界目标,锚点 = UI3D 相机 w2c 第 3 行 ×
  Studio_Depth)。迁移后父级 = 恒等持有节点 → **数学不变**,但假设需
  实证。**探针**:迁移探针记录持有节点 world;首帧绘制后比对面板构图
  与 run 93 基线(人物位置/大小逐项对照)。若持有节点非恒等 →
  `local = parentWorld⁻¹ × anchor` 修正(run 40 物品参照系先例)。

## 5. 失败模式与回退

| # | 失败模式 | 症状(日志/TGA 可辨) | 回退 |
| --- | --- | --- | --- |
| H1 | 杀壳破坏图(引擎销毁路径穿透持有引用) | 轮 2 杀壳后崩溃,或下一帧绘制 rd 门拒绝/TGA 黑 | 停在轮 1 形态立项收尾(壳常驻泊位 + 图驻菜单场景,世界零 pass 已结构性达成),杀壳列为"证伪留档" |
| H2 | 菜单场景清除外来挂接甚至销毁图 | `P home parent detached` 频发,re-home 后仍异常 | re-home 自动化已兜底;若 re-home 后绘制坏 → 持有节点改挂更稳定宿主或改纯无父持有(根无父级,local==world 恒成立,pose 数学反而更简) |
| H3 | 菜单剔除器意外收录外来图(版本差异) | 引擎画 P pass 的计数告警行/世界侧重影 | 保留的菜单帧 p_geom 抑制本来就在拦;确认抑制覆盖即记录,无需新机制 |
| H4 | 缓冲初始化不足(世界渲染窗口不够) | census < N/N 持续,迁移被门控拦住 | 门控 fail-open = 自动停留现架构;延长 settle 帧数为下一轮参数 |
| H5 | 持有节点非恒等变换 → 人物出画/错位 | 迁移探针 world 非恒等;面板构图偏离 run 93 基线 | `local = parentWorld⁻¹ × anchor` 修正(一次标定) |
| H6 | 迁移与在途 pass 竞态 | 交叠压力测试偶发崩溃 | `m_relocating` 闩升级为"迁移只发生在面板关 + 括号外帧"(pump 已具备该判别) |
| H7 | 存档残留恶化(DuplicateForm 基泄漏) | 存档体积异常增长/清扫计数上涨 | 清扫保留兜底;SetDelete 时机提前到迁移同帧 |
| H8 | BSFadeNode 在菜单场景被 fade 更新 → 人物淡出 | TGA 人物半透明/黑 | fade 值为节点状态,菜单场景无人更新它,预期不变;若出现,绘制窗内强制 fade 满值(一行) |

**回退阶梯**:R1 = 轮 1 形态(图迁移 + 壳保留,防线全在)——收益已含
"世界流零 P pass"的结构性 half;R2 = 全退回 v6.59 现架构,方案证伪
留档。任何时点现架构都是安全网(§0ai 决策原话)。

## 6. 验证轮步骤(游戏内,按 runbook)

**轮 1(迁移)**:构建部署 → 进存档等待自动出生 → 开背包面板:①人物
像与 run 93 基线一致;②日志含迁移探针行(持有节点 world / census
N/N / `P home parent ok`);③绘制帧摘要 `source=live`。开关面板 ×5、
容器菜单/地图/技能各开一次再回背包(图不丢)。第三人称看脚下:壳不可
见(图已搬走)。**顺带补验**:开面板状态下存档+读档(m0-acceptance
开放项 1 的回归——despawn 路径本轮动过)。零崩溃 + 判据全过 → 进轮 2。

**轮 2(杀壳 + 退役)**:同流程,另加:①杀壳探针行
(`P shell killed, graph alive`)后人物照常;②第三人称全程零替身零交
互(不再依赖泊位);③存档→读档→无残留克隆、清扫零命中;④开关面板
×20 压力;⑤对照 crash 目录零新增。全过 → 更新 m0-handoff/m0-acceptance,
M0 阶段 2 完整收官评估,防线退役清单核对。

产物:本轮 DLL + 两轮日志/TGA + 本文档追加 run 判读段(沿用
stage2-p-instance-plan.md 的 §0 编号惯例,新增 §H 序列)。

## 7. 决定记录

- 2026-10-04 用户拍板:启动"摄影棚即居所"工作包(§0ai 决策的执行)。
- 两轮制为本方案裁剪:轮 1 只迁移不杀壳(壳防线全保留 = 零行为风险),
  轮 2 杀壳 + 退役;两轮判据独立,轮 1 全过才进轮 2。
- 灯光与世界光照一致性、亮度/色温微调、面板-物品卡排布:不在本工作包
  (既有非阻塞遗留)。
- 正式 CPU/GPU/显存测量(PRD §5.3):仍为独立事项,不并入本轮。
