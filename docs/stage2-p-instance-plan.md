# 阶段 2 验证轮方案：独立展示实例 P（蒙皮过重放路径）

- 日期:2026-10-02;run 27–56;**run 56:v6.20 首验——人物像成型带光照、
  稳态逐帧连续绘制(submitted=14 每帧)!剩取景偏移 + CS 光源钩子崩溃
  (复制的灯光指针失效);v6.21 = bound 动态居中 + 引用式灯光;
  v6.22 = 幽灵化第一层(§0ae);v6.23 = AI 钉桩 + 摄影棚 T-pose(§0af);
  v6.24 = 残留清扫 + T-pose 修复(§0ag);v6.25 = 几何隔离(§0ah);
  v6.28 = 固定视轴锚点 + 菜单限定合成(§0al);v6.29 = 视锥半角标定
  (§0am);v6.30 = 面板矩形右移 + 蒙皮对齐(§0an);v6.31 = CB 分离 +
  v6.30 = 面板矩形右移 + 蒙皮对齐(§0an);v6.31 = CB 分离 +
  退出生命周期(§0ao);v6.32 = 纵横比窗口 + 正面灯光 + 存活检查(§0ap);
  v6.33 = 每 pass 灯光 rig + 世界丢失关面板(§0aq);v6.34 = 朝向翻回 π +
  主菜单即关(§0ar);v6.35 = 朝向定稿 Rz(0)(§0as);
  v6.36 = rig 基向量归一化(§0at);v6.37 = 改 NiLight 节点 + 灯光普查
  (§0au);v6.38 = 面板-高亮解耦 + 读档闪现消除(§0av);
  v6.39 = 面板生命周期绑定背包菜单(§0aw);v6.40 = 合成目标括号出口
  自捕获(§0ax,**已证伪并撤回**);v6.41 = 目标持久化 + 出口 OM 禁区
  (§0ay);v6.42 = kFRAMEBUFFER 直绘(§0az);v6.43 = 摄影棚目标自建
  (§0ba,输入端解耦);v6.44 = 自建块静默跳过诊断(§0bb);
  v6.45 = 模板换 RTV 来源(§0bc);v6.46 = 引擎脏位守卫(§0bd);
  v6.47 = 自建摄影棚灯光(§0be);v6.48 = 半径标定(§0bf,被证伪);
  v6.49 = 引擎正册注册(§0bg);v6.50 = 双队列取壳 + 非粘性重试(§0bh);
  v6.51 = 私有 rig 节点 + 强制级联(§0bi);v6.52 = 主光改方向光(§0bj);
  v6.53 = menu light 覆盖退役 + 壳字段修正(§0bk);
  v6.54 = 回归双点光 + 每次 fetch 重补丁(§0bl);
  v6.55 = 每 pass 壳补丁 + 每灯独立摆位(§0bm);
  v6.56 = 引擎槽位约定修正(§0bn);
  v6.57 = 单绘制路径:replay 窗口退役(§0bo);
  v6.58 = 灯光 rig 泊位制:世界泄漏修复(§0bp);
  v6.59 = 拉回坐标系修正(§0bq)**
- 状态:**v6.59 已验证(run 93 用户确认全部成立:面板光照正确、世界无
  泄漏、高亮/无高亮一致)——摄影棚灯光命题闭环。遗留调优项(非阻
  塞):灯光与世界光照一致性(用户明示后续处理)、亮度/色温微调
  (diffuse/fade/70/50/spread 皆旋钮)**
- 上游:[m0-handoff.md](m0-handoff.md) 阶段 2;PRD M0"独立展示实例
- 上游:[m0-handoff.md](m0-handoff.md) 阶段 2;PRD M0"独立展示实例
  (可先固定姿态)"
- 读者:实现与验证此轮的 Agent / 开发者

## 0bq. Run 92 结果(v6.58 世界泄漏修复✓;面板光变暗 = 拉回后坐标双重计入)+ v6.59 方案

run 92 会话(2026-10-04 14:36,用户截图):

- **世界泄漏修复成立**(用户确认):背包开着的世界场景无光斑;
- **新缺陷:面板内光变暗/位置偏**——人物只有暗淡轮廓光。机制:v6.58
  把 rig 拉回**锚点**后,per-pass 摆位仍写**满锚点系目标**到灯节点
  local——rig 在锚点 + 节点 local 又一个锚点系目标 = **锚点双重计入**
  (rig local anchor × 节点 local ≈ 2×anchor),灯实际落在 ~2×485 = 970
  处,距离翻倍 → 衰减(平方/距离)只剩约 1/4 → 暗淡轮廓。这与截图完
  全一致(光仍在,只是又远又弱)。

**v6.59(2026-10-04 DLL,MD5 5ad6742d…,待游戏验证)= 拉回坐标系修
正**:per-pass 摆位写**锚点相对偏移**(`right×spread + up×50 −
forward×70`,不再含 anchor 项)——rig 停在 anchor、节点 local 是偏
移,合成后灯世界位 = anchor + offset = light_target,与 rig 公式的
意图逐项一致。restore 对称不变。

**判读(run 93)**:①面板光照恢复 run-90 左图水平(双点光正面、亮度
足);②世界场景无泄漏(不回归);③高亮/无高亮一致;④零 crash。若亮
度仍差:offset 的 70/50/spread 与 fade 都是旋钮;普控行 ni_world 可
直接核对灯位。

## 0bp. Run 91/92 结果(v6.57 单路径成立:面板光照正确;新报告 = 灯光泄漏到世界)+ v6.58 方案

run 91/92 会话(2026-10-04 13:45,用户双世界截图):

- **v6.57 单绘制路径成立**:面板内光照正确(无高亮/高亮一致,run 90
  左图质量保持)——replay 窗口退役消除了上下文污染;
- **新报告(用户判读精准)**:背包**开启**时,世界场景(库存面板外的
  背景)石壁上出现**面板同款暖光斑**,且随时间/摆位移动;背包关闭后消
  失。机制链:v6.49 为打通 LLF 把灯**注册进世界灯光总账本**
  (activeLights);灯挂 menuObjects[0] 下、rig 摆位在锚点(世界坐标
  ≈玩家脚下)——引擎灯光 tick 把它们当真灯**渲染进世界**;我们的
  mutate/restore 只保护绘制窗口,窗口外(背包开着的世界背景、面板关
  闭后)灯一直在原位供光 = 泄漏。

**v6.58(2026-10-04 DLL,MD5 e5acc3ef…,待游戏验证)= rig 泊位制**:

1. rig 节点**默认泊位 = 世界外**(Z +100000,远超远平面 20480,任何
   相机都照不到);
2. `draw_p_proactively` 入口(P 将画):rig 拉回**锚点**(menuObjects[0]
   的 world 是菜单空间 identity 级联,local = 累加器空间锚点,灯即到
   位);
3. 所有出口(正常收尾、画布缺失、context 缺失)泊回世界外——灯的
   "世界可见"区间被压缩到**仅绘制窗口内**,窗口外任何 tick 拿到的灯
   都在 100k 外,对世界零贡献;
4. 面板关闭、读档、世界帧同理覆盖(灯常驻账本但永远泊在泊位)。

**判读(run 92)**:①背包开启时世界场景**无光斑**;②面板内光照与
run 90 左图一致(rig 窗口内摆位不变);③高亮/无高亮一致、无回归、零
crash。若①仍有残光:泊位距离不够(改 50000→更高)或引擎对 menuObjects
子树有独立剔除(下一层:窗口外同步把灯的 `flags` 置 kHidden——LLF
IsValidLight 首条即查它)。

## 0bo. Run 90 结果(v6.56 槽位修正:无高亮完美;高亮仍黑 = replay 窗口上下文)+ v6.57 方案(用户架构决策:退役 replay 绘制)

run 90 会话(2026-10-04 03:58,用户双截图对比):

- **槽位修正生效:无高亮窗口完美**——主光(槽 1)首次真正参与照明,
  人物正面双点光打光,与 run 87 左图同级别且更完整;
- **高亮窗口仍黑**。日志定案机制:`composite draw #` 全会话仅 1 次(开
  面板瞬间)——右图面板**也是** end_frame 兜底合成的;两窗口的差异只
  剩**绘制发生的窗口**:高亮帧 menu pass 到达 → `replay_after_original`
  先跑 → `draw_p_proactively` 在 **replay 窗口**绘制(帧闩置位)→
  end_frame 的绘制被闩跳过。replay 窗口的绘制上下文 = 物品 pass 的
  post-original(其灯光常量/严格缓冲刚刚生效),P 的着色在该上下文中
  被污染 → 黑;
- **用户架构决策(提问即方案)**:"既然 end_frame 里能插入
  draw_p_proactively 完成 srv 绘制,为什么还需要 thunk_site?"——盘点
  thunk 职责:菜单灯登记(v6.53 退役)、物品 pass 重放(v6.0 退役)、
  replay 内合成(v4.5,被 end_frame kFRAMEBUFFER 覆盖)、**P/ghost 透
  传抑制(v6.19,仍需)**。结论:绘制与合成职责全部收归 end_frame 单
  路径,thunk 只留抑制。

**v6.57(2026-10-04 DLL,MD5 153358b7…,待游戏验证)= 单绘制路
径**:

1. replay 窗口的 `draw_p_proactively` 调用移除(高亮帧的 P 绘制不再
   发生在物品 pass 上下文);
2. replay 内合成退役(end_frame kFRAMEBUFFER 是唯一合成点);
3. thunk_site 职责 = on_pass 分类 + P/ghost 透传抑制 + FR-06 清理调
   度,不再有任何绘制/合成行为;
4. end_frame 每括号帧:自建画布检查 → P 直驱 → kFRAMEBUFFER 合成——
   一条路径、一个上下文。

**判读(run 91)**:①零高亮与高亮窗口**光照一致**(都应与 run 90 左
图相同);②高亮时物品预览照常显示(其原生透传不受影响);③无回归、
零 crash;④反复开关背包光照稳定。若①达成,槽位 + 单路径两案合并,M0
蒙皮渲染与灯光命题完整闭环,进入 M0 收官清单(存读档验证 + 性能测
量)。

## 0bn. Run 89 结果(v6.55 无高亮仍正确、高亮仍暗;根因定案 = 引擎槽位约定)+ v6.56 方案

run 89 会话(2026-10-04 03:30,用户左右对照"回到 run 87 结果" + 日志):

- v6.55 的两项机制生效但**现象未变**:无高亮正确、高亮黑;
- **结构性发现**:现在有**两条合成路径**并存——无高亮帧走 end_frame
  兜底(合成进 kFRAMEBUFFER),高亮帧走 replay 合成(合成进 format-28
  实例,`composited_this_frame` 闩令 end_frame 跳过)。两路采样同一
  studio target、同一套灯,理应同亮——但高亮帧黑;
- **根因定案(引擎硬约定)**:CS `LightLimitFix.cpp:248` strict 灯光
  从 **`sceneLights[i + 1]`** 开始收集——**`sceneLights[0]` 是环境光
  槽,点光从下标 1 开始**。回查 v6.47 以来的布局:主光一直坐在**槽
  0**——被引擎当环境光无视,真正工作的只有槽 1 的补光;run 87 左图亮
  = 补光恰在正面。高亮帧更暗的机制:replay 窗口里物品 pass 刚用自身
  灯光跑过 SetupGeometry,LLF strict CB 按"后写生效"被物品灯占据;我
  们 P pass 的 strict 数据本来自槽 1 补光,但窗口内壳字段/槽位与物品
  灯数据竞争,呈现为更暗;
- v6.55 的两项(per-pass 补丁、独立摆位)保留——它们修的lodDimmer 重
  置与摆位覆盖是真实 bug,只是被槽位约定这个更大的问题掩盖。

**v6.56(2026-10-04 DLL,MD5 e45e3990…,待游戏验证)= 槽位约定修
正**:

灯光布局重排为引擎约定结构(3 盏,numLights=3):
- **槽 0 = 环境光**:`NiPointLight` + `ambientLight` 语义位由引擎读取
  (diffuse 0.25/0.25/0.28 柔和底光,radius 4096);
- **槽 1 = 主点光**(暖白 1.0/0.96/0.90,rig 摆位锚点前上方);
- **槽 2 = 补点光**(冷调 0.70/0.80/1.0,对侧);
- per-pass 补丁与独立摆位(v6.55)保留;LLF strict 从槽 1/2 取两盏点
  光——首次两条通道都拿到完整灯光。

**判读(run 90)**:①零高亮与高亮窗口光照**一致**(双点光正面打
光);②亮度对比 run 87 左图应显著改善(主光终于生效);③无回归、零
crash。若亮度/色温需微调:diffuse/fade/摆位常量都是旋钮。若槽 0 环
境光在原生路径引发异常亮/灰:ambient 值调低或改用 `ambientLight=true`
语义壳。

## 0bm. Run 88 结果(v6.54 双窗口全黑:引擎 tick 重置壳字段 + 共享父摆位互覆)+ v6.55 方案

run 88 会话(2026-10-04 03:08,用户双截图"两者全暗,高亮更暗" + 日志):

- **补丁与覆盖的竞态实锤**:每次 fetch 都出现 `raw: lodDimmer=0.000`
  ——补丁在 fetch 时打,但引擎的灯光更新 tick **每帧都会把外来壳的
  lodDimmer 重写回 0**(我们的灯不在它的 LOD 淡入记账里),fetch 时补
  丁永远追不上覆盖;
- **共享父摆位互覆实锤**:多帧 `shell[0]` 与 `shell[1]` 的
  `worldTranslate` 完全同值 `(22.5,-415.4,50)`——两盏灯写的是同一个
  rig 父节点,后者覆盖前者,两盏灯实际叠在一点(单点双亮度仍应有光,
  但 spread 失效);
- run 87 左图能亮 = 那次 fetch 后引擎 tick 恰好未重置(时序运气),说
  明点光配置本身有效。

**v6.55(2026-10-04 DLL,MD5 0de1d262…,待游戏验证)= 补丁进窗口 +
每灯独立摆位**:

1. **每 pass 壳补丁**:override 换入后、draw 前,对本 pass 引用的每盏
   壳强制 `lodDimmer=1 / luminance=1 / frustrumCull=0`——窗口内的状态
   我们说了算,引擎 tick 在窗口外怎么重置都无所谓;
2. **每灯独立摆位**:rig 改为写**灯节点自身**的 local.translate(值 =
   累加器空间目标位;rig 父仅作级联载体保持 identity),两盏灯互不覆
   盖;restore 对称简化(级联重申即可);
3. fetch 时补丁保留(首帧兜底),但主防线移入窗口。

**判读(run 89)**:①零高亮与高亮窗口光照**一致且同 run-87 左图**
(双灯 spread 恢复后应比左图更均匀);②反复开关背包不丢光;③无回
归、零 crash。若仍黑:raw dump 的 lodDimmer 在窗口内是否为 1 是最后
判据——为 1 仍黑则着色器消费点另有其处(RenderDoc PS 常量直接看)。

## 0bl. Run 87 结果(v6.53 左图自建灯首次生效✓;右图黑 = 账本重建后壳错拿)+ v6.54 方案

run 87 会话(2026-10-04 02:48,用户左右对照 + 日志):

- **左图(无高亮)自建灯首次生效**——人物全身均匀受光、面部清晰。判读
  成立:`shell[1] raw: lodDimmer=0.000` 补丁后生效;方向光实验在此帧
  尚未转正,供光的是**点光壳 + rig 摆位** = 被验证的正确配置;
- **右图(高亮)黑**。日志定位:面板开关后引擎**重建账本**
  (activeLights 97→99),重取的壳里 `shell[0]` 变成
  `pointLight=false lum=10081 worldTranslate=(0,0,0)`——是**引擎重建
  的方向光壳**(v6.52 创建的,RTTI 方向光 → pointLight=false 天经地
  义;lum 上万)且 `lodDimmer` 重置为 0;而壳补丁只打过一次,重建后未
  再打。lum 上万 + 方向背对 = 一盏超亮错向的方向光压过两盏正确点光,
  画面全黑;
- 佐证:重取帧的 `worldTranslate=(22.5,-415.4,50)`(我们的 rig 摆位)
  出现在 shell[1](点光)上——点光壳是好的,坏在方向光壳。

**v6.54(2026-10-04 DLL,MD5 7be5d085…,待游戏验证)= 回归双点光 +
每次 fetch 重补丁**:

1. **方向光实验退役**:两盏都改回 NiPointLight(v6.47 配置:暖白主光
   + 冷调补光,4096/2.0)——左图已证明"点光 + rig 摆位"就是正确配
   置;
2. **壳补丁改为每次 fetch 都检查**:引擎重建壳时 lodDimmer 回 0,补丁
   条件从"一次性"改为"lodDimmer!=1 或 lum!=1 即重打",raw dump 只在
   实际补丁时打;
3. 其余(arming 退役、rig 摆位、双队列取壳)不变。

**判读(run 88)**:①零高亮与高亮窗口**光照一致**(同左图);②反复
开关背包后光照不丢(重补丁机制);③无回归、零 crash。若①仍偶发黑:
查 `PATCHED` 行是否在该次 fetch 出现——出现仍黑 = 重建壳的字段不止
lodDimmer(raw dump 会给全部),下一步按 dump 定。

## 0bk. Run 86 结果(v6.52 无变化;判读揪出 menu light 覆盖缺陷)+ v6.53 方案

run 86 会话(2026-10-04 02:30,用户左右对照截图"和之前一样" + 建议
多加 log 钉死加灯逻辑):

- 无高亮 = 黑、高亮 = 亮,与 v6.49-52 完全一致;方向光主光也无效;
- **判读揪出一个贯穿性缺陷**:重读 `on_pass` 菜单分支发现,v6.18 的
  "menu light arming"(`m_studio_light_array = pass->sceneLights`)在
  **每个高亮帧都会覆盖自建灯数组**——也就是说:
  1. run 79-86 的所有"高亮 = 亮"对照,**光全部来自菜单灯**(v6.18 老
     机制),自建灯从未在对照窗口里真正供过光;
  2. "高亮才亮/不高亮就黑"的表象,实为**两种灯光源在争夺同一个
     override 槽位**(menu light 数组 vs 自建数组,谁后写入谁生效);
  3. 自建灯被冤枉了五轮——它从未被单独测试过;
- 另一头,自建灯无高亮时不亮的直接嫌疑浮出:**引擎建的壳字段陈旧**。
  壳经 lightQueueAdd 路径创建,从未经过灯光更新 tick 的处理,
  `lodDimmer`(LOD 淡入系数)很可能停在 0——而**两条消费路径都乘它**:
  LLF `light.fade *= bsLight->lodDimmer`(:451),引擎原生路径同款 LOD
  淡出。lodDimmer=0 ⇒ 灯存在但贡献恰好为零,与"有灯无光"完全吻合。

**v6.53(2026-10-04 DLL,MD5 859ddbec…,待游戏验证)= 覆盖退役 + 壳
修正 + 普查扩展**:

1. **menu light arming 退役**:菜单灯只在 `m_studio_light_array` 为空
   时兜底登记(并打 `SKIPPED` 日志)——自建灯优先,menu light 彻底退
   出面板光照(用户 v6.47 的原始要求这才真正落地);
2. **壳字段修正**:取壳成功后强制 `lodDimmer=1`、`luminance=1`、
   `frustrumCull=0`,并一次性 dump 原始值(lodDimmer/lum/portalStrict/
   dynamic/pointLight/frustrumCull/worldTranslate)——原始值即证据;
3. 普查行追加 `lodDimmer={:.3f}`。

**判读(run 87)**:①零高亮与高亮窗口**光照一致**(都由自建灯供
光);②`shell[N] raw` 行的 lodDimmer 原始值是直接证据:0 = 判读成
立;③若①仍黑:对照 raw dump 的其余字段(portalStrict/dynamic)分
叉;④无回归、零 crash。

## 0bj. Run 85 结果(v6.51 级联已修:ni_world 随 rig 动;点光仍无光)+ v6.52 方案(方向光)

run 85 会话(2026-10-04 02:00,用户"仍然和之前一样" + 日志):

- **级联修复成立**:普查 `parent='CP_StudioLightRig'`、
  `ni_world=(-22.5,-415.4,50.0)` ——NiLight 世界变换随 rig 动了;
- **两个新事实**:
  1. **共享父节点摆位互相覆盖**:light[0] 的 `bs_new=(+22.5,…)` 与
     `ni_world=(-22.5,…)` X 镜像——rig 先为 light[0] 把父节点摆到
     +22.5,又为 light[1] 摆到 -22.5,两次同改一个父节点,最终两盏灯
     叠在同一位置(light[0] 的 local 是 0,跟着父节点落到 light[1] 的
     位置);
  2. **位置全对、距离 ~90 << 4096,却仍然无光**——点光的"位置→着色
     器常量"换算在 LLF strict 路径(世界眼点 `posAdjust.getEye()`)与
     引擎原生路径(accumulator 眼点)之间存在**坐标空间歧义**,三轮
     (v6.49-51)未能收敛;
- **决定性对照**:高亮窗口能照亮人物的菜单位灯 `light[0]` 是
  **point=false 的方向光**(run 69/70 普查)——方向光与位置无关,天然
  免疫上述歧义,"方向光能照亮面板人物"是被验证过的事实。

**v6.52(2026-10-04 DLL,MD5 5fd0cb29…,待游戏验证)= 主光改方向
光**:

1. `light[0]` 改 `NiDirectionalLight::Create()`(CLib 官方工厂,
   0x158/0x180):模型方向 = (1,0,0)(CLib 注释原文:世界方向 =
   world rotate 第一列);`local.rotate = Rz(-90°)` 把 +X 转向视轴 −Y
   = 从相机指向人物;diffuse 暖白 + fade 2.0(effectColor 同步);
2. `light[1]` 保留点光作对照(radius 4096/fade 2.0);
3. 注册/挂图/摆位管线不变(rig 对方向光只改 rotate 的父节点分支
   同样工作;ni_world 普查继续)。

**判读(run 86)**:①零高亮面板人物受光(方向光应立即生效——与菜单
灯同型);②若主光方向不对(侧光/背光):rotate 是唯一旋钮,普控行的
ni_world 不变、需要 RenderDoc 看方向常量,一轮 90° 步进试出;③补光
(点光)若这轮也开始有效,说明此前点光问题在摆位覆盖(共享父),下轮
拆双父;④零 crash 无回归。

## 0bi. Run 84 结果(v6.50 入册/取壳全通但仍无光:级联短路)+ v6.51 方案

run 84 会话(2026-10-04 01:39,用户"仍然和之前一样" + 日志):

- **注册链全通**:`studio lights created and handed to AddLight
  (activeLights.size=97 lightQueueAdd.size=2)` → 下一窗口
  `wrappers fetched (activeLights.size=99 lightQueueAdd.size=0)`——灯
  已入册转正,壳已取回,override 每窗口在跑;
- **仍无光**。至此 pass 数组、LLF 账本两条通道都有我们的灯、override
  都在换入,却照不亮——剩下的唯一解释在**灯光的世界位置**:普查行
  `parent=''`(空名 = menuObjects[0],引擎大根)+ `node_moved=1`,但
  `node_moved` 只说明我们写了 parent 的 local 并调了 `Update`;
  **menuObjects[0] 带 selective-update 标志,Update 的级联被短路**——
  `NiLight::world.translate` 从未真正改变(停在局部原点附近,光照到
  别处去了)。这正是 run 41 在 P 根上的同一坑,当时的解法 =
  `UpdateDownwardPass` 强制级联;
- v6.47 时代(run 71)rig 对菜单灯的移动之所以生效:那时 parent 是物
  品预览自己的小节点,没有选择性标志。

**v6.51(2026-10-04 DLL,MD5 0082d329…,待游戏验证)= 私有 rig 节点 +
强制级联**:

1. 创建一个私有 `NiNode`("CP_StudioLightRig")挂到 menuObjects[0],
   两盏灯挂到 **rig 节点**下(fresh、无标志,rig 现有代码的
   `ni_light->parent` 自动变成它,零逻辑改动);
2. rig/restore 的级联改 `UpdateDownwardPass(kDirty)` 强制(run 41 同
   款);
3. 普查行新增 **`ni_world=(x,y,z)`**——着色器实际消费的灯光世界位置
   直接可见:`bs_new` 动了而 `ni_world` 没动 = 级联仍被短路;两者一致
   且在人物前上方 = 灯光应生效。

**判读(run 85)**:①零高亮面板人物正常受光;②`ni_world` ≈ 人物前上
方(与 bs_new 同域);③高亮无回归、零 crash。若 ni_world 仍不动:强
制级联也推不动(下一层查 flags 细节或改设 world 直写);若 ni_world
对了但仍无光:着色器消费点另有其处(RenderDoc PS 常量直接看灯位)。

## 0bh. Run 83 结果(v6.49:GetPointLight 扑空 → sticky 报废)+ v6.50 方案(双队列取壳)

run 83 会话(2026-10-04 01:16,用户"和之前一样" + 日志):

- **注册本身没报错,但同帧取壳失败**:`wrapper fetch failed
  (GetPointLight)` 一次性告警 → sticky 置位 → **两盏灯全会话报废**,
  rig 掉回地牢方向光(`node_moved=0`)——与截图吻合;
- **用户判断(LLF 改了灯光添加逻辑)方向正确**:钩子本身(ValidLight1-3)
  只在引擎原生 portal 过滤上叠加条件,不拦我们的灯;真正的细节在
  **引擎 AddLight 的队列语义**:`ShadowSceneNode` 有 `lightQueueAdd`
  (0x160)——新灯先进队列,**引擎下一次灯光更新才转正 activeLights**。
  我们同帧 `AddLight → GetPointLight(扫 activeLights)` 必扑空;而
  sticky 失败让这次扑空升级成全会话报废;
- LLF 的 strict 缓冲(UpdateLights :492)也只在**下一次 UpdateLights**
  才会扫到转正后的灯——同帧创建 + 同帧消费的时序对两条通道都太早。

**v6.50(2026-10-04 DLL,MD5 059cf22b…,待游戏验证)= 双队列取壳 +
非粘性重试**:

1. `ensure_studio_lights` 拆两相:**创建相**(一次性,AddLight 入队)
   + **取壳相**(每窗口重试):取壳扫 `activeLights` **和**
   `lightQueueAdd` 双队列,两盏都拿到才点亮 override;
2. sticky 失败退役:取壳失败只是下一窗口再试(转正最多晚一帧),不再
   报废全会话;
3. 日志:创建行带 `activeLights.size/lightQueueAdd.size`,取壳成功行
   同样带——队列语义直接可见。

**判读(run 84)**:①开背包 1-2 帧内 `wrappers fetched from the
ledger` 行出现,面板人物正常受光;②若取壳行持续不出现:日志的双队列
尺寸直接说明灯在哪(队列不转正 = 引擎灯光更新点不在菜单帧——届时改
为我们手动把壳从队列搬进 activeLights 或直接用参数版 AddLight 重试);
③零 crash 无回归。

## 0bg. Run 82 结果(v6.48 半径假设证伪;真供光通道 = ShadowSceneNode::activeLights)+ v6.49 方案

run 82 会话(2026-10-04 00:53,用户:"结果和上次一样,但高亮物品就有光"):

- **v6.48 已生效**:`radius.x=4096 fade=2.000` 普查确认、rig 照常
  (node_moved=1),画面**零变化**——上轮"灯距 415 > 半径 400"假设被
  **证伪**:若半径是因,4096 必然改变画面;
- **"高亮物品就有光"才是判据**:高亮时 figure 被点亮 = 物品自身灯光
  经由某通道到达着色器;无高亮时我们的自建灯虽在 pass->sceneLights
  override 里却无光 = **着色器消费的不是(只有)pass 数组**;
- **源码定案**:CS `LightLimitFix::UpdateLights` 遍历的是
  **`smState->shadowSceneNode[0]->GetRuntimeData().activeLights`**
  (:492)——世界场景的灯光总账本——填进 strict 常量缓冲供所有
  BSLighting 着色器消费。我们的 NiPointLight 从未注册进该账本(LLF 的
  IsValidLight 之外,根本不在列表里),strict 缓冲里自然没有它们;
  pass->sceneLights 的 override 只影响引擎原生路径的一部分,LLF 启用
  时 strict 缓冲才是主供光源;
- 引擎现成注册 API:`ShadowSceneNode::AddLight(NiLight*)`(CLib REL
  99691/106325,便捷重载)——引擎自己建 BSLight 包装、填全字段、
  入册 activeLights。

**v6.49(2026-10-04 DLL,MD5 583f6ba8…,待游戏验证)= 引擎正册注
册**:

1. 创建流程不变(NiPointLight::Create ×2,4096/2.0,挂 menuObjects[0]);
2. **手工 BSLight 壳退役**:`BSShaderManager::State::GetSingleton()
   .shadowSceneNode[0]->AddLight(ni)` 注册;`GetPointLight(ni)` 取回
   引擎建的壳,存入 `m_studio_lights` 供 per-pass override 继续使用;
   取壳失败则 sticky 失败;
3. 两通道同时供光:LLF strict 缓冲(activeLights)+ 引擎原生 pass 分
   配;rig(v6.37)继续移节点。

**判读(run 83)**:①零高亮面板人物正常受光;②普控行 rtti 仍为
NiPointLight 且 radius.x=4096(壳现在是引擎建的,字段应一致);③高亮
无回归、零 crash。若仍无光:查 activeLights 是否真的收下
(GetLight(ni) 非空可证),下一嫌疑是 LLF 对 menu 帧的 inWorld 判定把
strict 缓冲清零(NumStrictLights = inWorld ? 0 : ...)——那时灯光该从
"引擎原生路径"出,方向转查 SetupGeometry 的原生灯光常量装配。

## 0bf. Run 81 结果(v6.47 灯链全通但亮度近零:radius 400 < 灯距 415)+ v6.48 方案

run 81 会话(2026-10-04 00:28,用户截图"可能有灯但没生效" + 日志):

- **自建灯光全链验证通过**:`studio lights created: 2` → 换入
  (`applied to P passes: 2 SELF-BUILT`)→ 普查行完美
  (`point=true dynamic=true lum=1.000 rtti=NiPointLight radius.x=400
  fade=1.000 parent='' bs_new=(±22.5,-415.4,50.0) node_moved=1`)——
  创建/壳/挂图/换入/节点摆位全活;
- **但画面人物极暗轮廓**(用户:"可能有灯但没生效");
- **根因(算术实锤)**:灯到人物距离 = 锚点深度 485 − 前移 70 =
  **415 单位**,而 `radius=400 < 415`——代入 CS InverseSquare 衰减公式
  (`GetAttenuation` 的 smoothstep 项),**贡献恰好 0.0000**:人物在灯
  光作用区外。灯一切正常,唯独够不着人。

**v6.48(2026-10-04 DLL,MD5 cce11909…,待游戏验证)= 半径/亮度标
定**:radius 400 → **4096**(任意摆位都覆盖;2000 已够,4096 留余
量),fade 1.0 → **2.0**(亮度余量),`SetLightAttenuation(4096)` 同
步。其余零改动。

**判读(run 82)**:①零高亮面板人物**正常受光**(暖白主光 + 冷调补
光);②若过亮/过暗/偏色,fade 与 diffuse 即旋钮(一轮定值);③高亮
后灯光不变(解耦);④零 crash 无回归。注意普控行 radius.x=4096 可作
为生效确认。

## 0be. Run 80 结果(v6.46 弓箭修复✓)+ v6.47 方案(自建摄影棚灯光,用户提案)

run 80 会话(2026-10-03 晚,用户确认):

- **弓箭/飞出几何修复成立**(脏位守卫生效,引擎不再把 P 重应用到
  kFRAMEBUFFER);
- **剩灯光缺陷**(run 79 判读的缺陷 2):零高亮窗口 rig 只有地牢方向
  光可动(无 parent 移不动),人物近黑。用户提案:**面板内移除 menu
  light,改用自建灯光**。

**v6.47(2026-10-03 DLL,MD5 b655294f…,待游戏验证)= 自建灯光**:

1. **创建**(会话首次直驱窗口,渲染线程):两盏
   `NiPointLight::Create()`(CLib 官方工厂,malloc_runtime + Ctor)——
   主光 CP_StudioKey(暖白 1.0/0.96/0.90)+ 补光 CP_StudioFill(冷调
   0.70/0.80/1.0),radius 400 / fade 1.0 /
   SetLightAttenuation(400);`AttachChild` 挂到 UI3D
   **menuObjects[0]**(括号每帧验证的常驻根)——节点从此有父,v6.37
   的 rig 移位路径天然生效;
2. **BSLight 壳**:引擎堆 0x140 + 真 `VTABLE_BSLight`(CS TruePBR 的
   fake-object 模式;窗口内引擎只读字段与 IsShadowLight 虚函数),
   pointLight=true、dynamic=true、lum=1,`light` NiPointer 指向自建
   NiLight;
3. **换入**:直驱窗口 per-pass override **无条件**指向自建壳数组
   (numShadowLights=0 不变)——`m_studio_lights_fresh` 新鲜度门退役,
   menu light 依赖整体移除;rig(v6.37)继续每 pass 摆两盏灯的节点
   (主光锚点前上 70/50,补光对侧 spread=-45 的低角度位),mutate→draw
   →restore 管线零改动;
4. 所有权:NiLight 由场景图 + 本地 NiPointer 双持(会话级,proto 可接
   受);壳会话级存活。

**判读(run 81)**:①零高亮开背包,面板人物**正常受光**(面部/胸前
亮,与右图高亮时同级);②主光暖白补光冷调(色温可从截图分辨);③
高亮物品后灯光**不变**(已与 menu light 解耦——这是新行为,报"正常"
即可);④零 crash、屏幕无泄漏、读档零闪现。若亮度/角度不理想,rig 的
70/50/45 与 radius/fade 是旋钮;若引擎对壳对象崩(异常虚表调用),回
报签名——下一层改为借真实 BSLight 换 NiLight 指针。

## 0bd. Run 79 结果(v6.45 部分验证:面板立现✓;弓箭上屏 + 无光照)+ v6.46 方案

run 79 会话(2026-10-03 21:51,用户双截图 + 日志):

- **解耦主目标达成**:零高亮开背包,`self-created` → `P draw: passes=15`
  → 合成,面板立即出现(用户左图右上的黑面板内有人物轮廓);
- **缺陷 1(左图):弓箭 + 拉伸几何画在屏幕左中(面板外)**。日志:
  `T-pose: 0 bind bones over 3 skin root(s)`(骨架未重绑,蒙皮矩阵垃
  圾)+ `menu-lights=0`。机制:引擎 `SetupAndDrawPass` 在
  `ShaderFlags::DIRTY_RENDERTARGET` 置位时**重应用账本里的目标**
  (CS Deferred.cpp 就是这个机制的用户)——无高亮窗口里没有任何东西
  经引擎状态绑过我们的自建目标,账本仍指 kFRAMEBUFFER,P 的 15 个
  pass(含骨头挂点上的弓)被画上屏幕;高亮窗口(右图)干净,正因为
  物品 call-site 刚用引擎状态绑过 format-28 实例,账本一致;
- **缺陷 2(左图):面板内人物无光照**。普查行:
  `rtti=NiDirectionalLight ... parent='<no parent>' node_moved=0`——无
  菜单灯时 rig 只能拿 pass 自带的地牢方向光;它**没有父节点**,v6.37
  的节点路径无处级联,灯没动,方向也不对。**此缺陷本轮不修**(需要
  给方向光造方向或自建灯光,单独一轮),先修缺陷 1。

**v6.46(2026-10-03 DLL,MD5 43d114ce…,待游戏验证)= 引擎脏位守卫**:

`draw_p_proactively` 裸绑自建 OM 后,
`RendererShadowState::GetSingleton()->GetRuntimeData().stateUpdateFlags
.reset(DIRTY_RENDERTARGET)`——pass 内部的状态应用把 OM 视为最新,我们
的绑定存活;窗口恢复 OM 后 `set(DIRTY_RENDERTARGET)` 置回,引擎下次
应用状态时重建它自己的绑定(CS Deferred 每帧同一握手,共存已被验证)。
其余零改动。

**判读(run 80)**:①零高亮面板内人物正常(蒙皮、取景同右图),屏幕
上无弓箭/飞出几何;②高亮后一切照旧;③零 crash、读档零闪现。若①屏
幕上仍有泄漏:脏位不止一个在作祟(RenderDoc 看泄漏 draw 的 OM 目标即可
点名);若面板内蒙皮仍乱(0 bind bones)——那是骨架重绑时序问题,与
本修正交,另轮处理。

## 0bc. Run 78 结果(v6.44 trace 实锤:fb.texture 空而 fb.RTV 活)+ v6.45 方案

run 78 会话(2026-10-03 21:47,日志):

- **trace 行**:`target.rtv=0x0 targetFailed=false fb.texture=0x0
  fb.RTV=0x1c67eca0f78`——跳过原因定位:**CLib `RenderTargetData` 的
  `texture/textureCopy` 裸指针在 1.6.1170 运行时不被引擎填充**(引擎只
  通过视图使用这些目标),`fb.texture` 恒空 → v6.43 的模板源永远拿不
  到;`fb.RTV` 有效(v6.42 合成用它画出了 #1);
- 第二窗口(21:47:11 高亮后)capture→P draw→composite 全链照旧——管
  线其余部分无恙,唯一断点就是模板源;
- v6.44 的诊断方法论再次生效:一次性 trace 一轮定因(run 46/53/77 同
  款)。

**v6.45(2026-10-03 DLL,MD5 6320edbf…,待游戏验证)= 模板换 RTV 来
源**:自建块的 desc 改从 `fb.RTV` 走 `GetResource → GetDesc`(v4.1 起
replay 路径对 format-28 实例的同款两步,零新机制);`texture==null`
告警改写为 RTV 判空。其余零改动。

**判读(run 79)**:①进背包零高亮:日志三连 `self-created from
kFRAMEBUFFER` → `P draw: passes=N` → `end_frame composite` 持续,面板
**立现且有人物**(首会话 P 未建成时黑矩形,次会话起有人物——设备缓
冲仍需世界帧初始化,这是自动出生机制的老边界);②高亮后 replay 路径
无回归;③零 crash、读档零闪现。若①面板黑(合成跑、P 未画):查
`P draw` 行的 passes 与 `rd=` 门(v6.14 老朋友),回报即可。

## 0bb. Run 77 结果(v6.43 自建块静默跳过)+ v6.44 诊断轮

run 77 会话(2026-10-03 21:34,用户报告 draw 仍中断 + 日志):

- **自建块零执行**:`self-created` 日志 0 条、失败 warn 0 条;面板窗口
  (21:34:11–18,7.6 s 零高亮)内 **`P draw` 零条**——P 直驱因目标缺失
  早退,与 v6.43 前一致;
- `end_frame composite #1` 出现(计数器每帧递增,日志只打 #1 与
  %1800)——合成块的 `fb.RTV` 非空且每帧在调用 draw,但 `!target.srv`
  守卫照旧早退(目标没建成);
- **矛盾**:合成块与自建块同在 end_frame、同一 `rt->context`,后者成
  功前者静默跳过 → 跳过条件必在 `!target.rtv && !m_target_failed` 与
  `fb.texture` 之间,且**无日志不可辨**——这正是 run 46/run 53 教训的
  重演(静默路径吞事件),修法也一样:先让每条路径出声。

**v6.44(2026-10-03 DLL,MD5 ac874fe9…,诊断轮)= 跳过路径全留痕**:

- 自建块入口打一次性 trace(renderer/rt/context/forwarder/target.rtv/
  targetFailed/fb.texture/fb.RTV 八个指针/标志);
- `fb.texture == null` 单独一次性告警;
- 失败/成功日志(v6.43 已有)不变。

**判读(run 78)**:开背包后把 `self-create trace` 行发回。分叉:①
`targetFailed=1` → 开面板前 replay 失败置位(查 21:34:11 前的日志);
②`target.rtv` 非空 → 目标其实已存在,断点在别处( srv 专项查);③
`fb.texture=0` → CLib 的 kFRAMEBUFFER 布局/运行时数据访问不对(换
RendererData 直读);④trace 行根本不出现 → end_frame 在该帧未走到自
建块(括号路径分叉)。

## 0ba. Run 76 结果(v6.42:draw 在 !target.srv 守卫早退——输入端仍绑 pass)+ v6.43 方案

run 76 会话(2026-10-03 21:15,用户 RenderDoc + 日志):

- 日志:`composite ready` + `end_frame composite #1` 出现,但 RenderDoc
  **零面板 draw call**;面板窗口(21:15:48–53)内 **零 `P draw` 日志**
  ——`draw_p_proactively` 也早退了;
- **根因(用户指认)**:`CompositeRenderer::draw` 开头守卫
  `!target.srv` 拦下;`draw_p_proactively` 开头 `!target.rtv` 拦下。
  摄影棚离屏目标(color/srv/rtv/dsv)**只在 `replay_after_original` 内
  从 call-site 模板创建**——零 pass = 零模板 = 零目标 = P 没地方画、
  合成没有输入。v6.42 只解了**输出端**(合成目标 → kFRAMEBUFFER),
  **输入端**(摄影棚画布)仍绑在 pass 路径上;
- v6.42 的输出端本身工作正常(守卫前的路径走到了)。

**v6.43(2026-10-03 DLL,MD5 1e45279d…,待游戏验证)= 摄影棚目标自
建**:end_frame 里、`draw_p_proactively` **之前**,若目标不存在且非
sticky 失败,从引擎常驻 `kFRAMEBUFFER` 的纹理 desc 推导模板自建:

- 分辨率 = 屏幕(与 format-28 实例一致);颜色格式**钉死
  R8G8B8A8_UNORM(28)**——物品 call-site 目标一直就是这个表示,replay
  与合成采样端所见格式不变;深度归一化到 D24_UNORM_S8_UINT;
- sticky 失败复用 `m_target_failed/m_failed_sig`(sig 含自建格式,与
  replay 路径的 sig 空间不冲突);分辨率变化时 replay 路径的 sig 重建
  仍然接管(两路共享 target_sig,无抖动);
- 创建后 `m_cleared=false` 保证新表面首帧清屏。

**判读(run 77)**:①进背包零高亮,面板立即出现**且有人物**(日志:
`studio target self-created from kFRAMEBUFFER` → `P draw: source=live
passes=N` → `end_frame composite #N` 三连);②高亮物品后 replay 路径
接管无回归;③读档零闪现、生命周期开关无回归;④零 crash。若①面板出
现但全黑(合成跑、P 没画):查 `P draw` 行——空 passes = P 几何未就
绪(等自动出生,第二次开背包判定);若仍 crash/异常,回报签名。

## 0az. Run 75 结果(用户实证:背包打开不进 thunk_site)+ v6.42 方案(kFRAMEBUFFER 直绘)

用户 run-75 测试结论(决定性架构事实):**背包打开时 thunk_site 三个
钩子一个都不进;进入需要高亮 3D 物品**。这意味着 pass 路径在"无高亮"
状态下整体不存在——合成目标捕获(replay 内)、兜底合成的门控事件
(菜单 pass)全部断供。**结论(用户拍板):面板显示与高亮的解耦必须
绕开 thunk_site/pass 路径**。

**v6.42(2026-10-03 DLL,MD5 fe802ae0…,待游戏验证)= kFRAMEBUFFER
直绘**:end_frame 兜底合成的目标从"replay 捕获的 format-28 实例"换为
**引擎常驻渲染目标表**:
`Renderer::GetRuntimeData().renderTargets[RENDER_TARGET::kFRAMEBUFFER].RTV`。

- **纯内存读引擎账本**:无出口 OM 探测(v6.40 crash 教训)、无捕获生
  命周期、渲染器存活期间永不为空(CS 的 SetUIBuffer 读的就是同一字段
  ——引擎自己的 UI 合成终点);
- **位置正确**:原版 UI 合成直接画进 kFRAMEBUFFER(CS 源码注释佐证);
  括号出口 = 菜单画完,四边形画上去随帧上屏;
- **状态安全**:`CompositeRenderer::draw` 自带全套 OM/状态保存恢复
  (v4.6 起每个会话验证),出口状态在返回前还原;
- 分辨率自适应:draw 从目标自身 desc 读尺寸定视口,kFRAMEBUFFER = 屏
  幕分辨率,与此前 format-28 实例相同;
- v6.41 的 s_panel_rtv 持久化保留(replay 路径仍在,两个目标并存不冲
  突:replay 合成画 format-28 实例、兜底画 framebuffer,同一帧内
  `m_composited_this_frame` 闩保证只走一条)。

**判读(run 76)**:①进背包面板**立即出现**(P 未建成时黑矩形;第
二会话起有人物),零高亮零 F7;②进出背包/读档零 crash;③读档零克隆
体闪现;④面板内容与高亮物品完全无关(物品卡照常)。若①失败但无
crash:看 `end_frame composite` 心跳是否出现——出现但不可见 = 菜单帧
的可见像素不在 kFRAMEBUFFER(CS 的 Upscaling/HDR 把 UI 重定向到了自
己的 uiTexture,下一层与 CS 的 UI 缓冲对齐);未出现 = RTV 空(几乎
不可能)。若②crash:对照签名定位。

## 0ay. Run 74 结果(v6.40 证伪:括号出口自捕获 crash)+ v6.41 方案

run 74 会话(2026-10-03 20:33,两次 crash:crash-2026-10-03-20-31-41 /
crash-2026-10-03-20-33-27,签名完全相同):

- **crash 点**:`d3d11.dll cmp [rsi+0x16D], rsi=0`——**Community
  Shaders `HDRDisplay::SetUIBuffer`** 内读空指针成员;调用链 =
  CS `Upscaling::MenuManagerDrawInterfaceStartHook::thunk` →
  SKSE Menu Framework → Main::Update——**与我们钩同一个 DrawInterfaceStart
  入口(ID 79947/82084)**;
- **时序证据**:日志 20:33:25.673 首次开背包 → v6.40 出口自捕获成功
  (`captured at bracket exit: format=28 (no pass needed)`)+ end_frame
  合成 #1 → 约 1.5 s 后 crash;上一帧刚有 clothesGND menu pass + format
  10/28 捕获交替(replay 路径);
- **机制**:v6.40 在**括号出口**读/绑 OM(探测 + 捕获 + 合成绘制)。
  该时刻的 OM 属于引擎 + CS 的 UI 合成状态机:CS 的 Upscaling 在下一帧
  同一入口先跑 `PostDisplay`/`SetUIBuffer`(把 kFRAMEBUFFER.RTV 重定向
  到 UI 纹理),其 D3D12/FG 分支对某些延迟初始化对象判空不全(用户自建
  CS 版,与本地 dev 源码行号错位)。v6.39 之前零 pass 帧我们在出口**从
  不碰 OM**,状态机从未被打扰;v6.40 让"无高亮背包帧"第一次进入该路径,
  恰好踩进 CS 未初始化分支。**教训:括号出口的 OM 是引擎/CS 的禁区,
  我们只在 replay 窗口内做 OM 相邻操作(v4.5 起已验证稳定)**;
- v6.40 的目标(切断高亮绑定)本身仍成立,换安全路线。

**v6.41(2026-10-03 DLL,MD5 48d3bb6f…,待游戏验证)= 目标持久化 +
出口 OM 禁区**:

1. **整体撤回 v6.40 出口自捕获**(探测/捕获/告警全删);end_frame 兜
   底合成回到 v6.38 形态(仅当 s_panel_rtv 已被 replay 路径捕获);
2. **高亮解耦换杠杆:s_panel_rtv 跨面板开关持久**——release_target 不
   再释放它。首次捕获(背包打开的默认高亮帧必然提供)之后,后续开关
   面板即时合成,不再等新一轮 pass 爆发。悬垂安全性:COM 引用计数保证
   纹理不销毁;分辨率变化时引擎换新实例,replay 路径的指针变化检查在
   下一次 pass 到达时自动重捕获——最坏情形 = 面板短暂消失到下一次
   pass,永不崩溃;
3. MenuSink 生命周期、读档闪现修复(v6.38)原样保留。

**判读(run 75)**:①游戏正常进出背包、读档,**零 crash**;②第一次开
背包仍需默认高亮那一轮(面板随首条 menu pass 出现——此时序与原版一致
且不可避),**第二次及以后开背包面板立即出现**;③读档零克隆体闪现;
④若②不成立,查 `captured` 行是否在会话早期出现过 + 关闭时是否意外释
放。若①再 crash:对照签名,若仍是 SetUIBuffer → 下一嫌疑是 end_frame
合成本身与 CS 时序的交互(进一步回退到 v6.37 形态)。

## 0ax. Run 73 结果(v6.39:MenuSink/补合成已通;残余绑定 = 合成目标只能从 pass 捕获)+ v6.40 方案

run 73 会话(2026-10-03 14:57,用户截图 + 日志定量判读):

- **v6.39 的两件事都工作了**:MenuSink 开火(`Panel opened (inventory
  opened)` 与 force-closed (inventory closed) 成对出现,零 F7 依赖);
  end_frame 补合成稳定运行(`composite draw #1800/#3600/#5400` 心跳,
  `end_frame composite #1` 后每帧都在画);
- **但用户截图:面板呈现仍依赖高亮**。日志定位残余绑定:整个会话
  `P pass:` = 0 条,`menu pass:` 只在高亮物品的帧出现,而
  **`s_panel_rtv`(合成目标指针)只能在 `replay_after_original` 内捕获
  ——没有 pass 到达就没有捕获**。开背包瞬间 UI 未高亮任何物品 → 零
  menu pass → 零捕获 → end_frame 兜底因 `s_panel_rtv==null` 跳过 →
  面板不出现;用户高亮物品(书/clothesGND)→ pass 爆发 → 捕获 + 合成
  → 面板出现。这就是"红框依赖黄框"的最后一环:**不是合成的执行,而
  是合成目标的取得绑在 pass 上**;
- 佐证:`composite draw #1` 恰好出现在第一次 menu pass(clothesGND,默
  认高亮帧)同一毫秒(14:57:27.283)。

**v6.40(2026-10-03 DLL,MD5 c9db5882…,待游戏验证)= 括号出口自捕
获**:end_frame 兜底块改为——`s_panel_rtv` 为空时,读
DrawInterfaceStart **出口**的 OM 绑定(此刻菜单绘制刚结束,绑的正是
菜单合成链终点);其 format == 28(R8G8B8A8_UNORM,UI 合成图,replay
路径一直捕获的同一 desc)则就地捕获(移入引用)并立即合成;非 28 则
跳过;零 OM 绑定打一次性告警。replay 路径的捕获保留(更精确,指针变
化自动重捕获)。**合成执行、生命周期(MenuSink)、读档闪现修复均不变
——只切断"目标取得绑 pass"这最后一环**。

**判读(run 74)**:①进背包**第一帧**面板即出现(黑矩形或有人物均可
——P 未建成时黑矩形也算"呈现");②无需高亮、无需 F7;③高亮物品后
面板内容不消失/不重置(replay 捕获与出口捕获指向同一实例);④读档闪
现修复、生命周期开关、灯光取景无回归。日志预期:`composite target
captured at bracket exit … format=28` 在开面板后第一时间出现(而非
第一个 menu pass 时)。若①仍失败:看该行是否出现——出现但面板不可见
= 出口 OM 不是被 merge 的实例(下一层:审计 UI 合成链);未出现 =
出口 format ≠ 28(按告警行换门控格式)。

## 0aw. v6.38 修正(用户重申逻辑:面板生命周期 = 背包菜单)+ v6.39 方案

用户明确:**面板只依赖背包的开启**——渲染逻辑直接插在背包开启之后、
随背包开而渲染、随背包关而关闭;不是"F7 框架内补合成"。

**v6.39(2026-10-03 DLL,MD5 7e7fac4e…,待游戏验证)= 生命周期重绑定**:

1. **MenuSink**(BSTEventSink<MenuOpenCloseEvent>,UI 单例注册):仅响
   应 InventoryMenu——`opening → open_panel("inventory opened")`,
   `closing → close_panel("inventory closed")`。事件在游戏线程、菜单
   状态一致点触发,首个背包帧前面板状态已就位(第一帧即括号);
2. **open_panel(reason)** 新增(幂等 CAS,F7 先开则不重复),与
   toggle 共享生成位/释放请求复位 + P spawn 尝试;**F7 降级为手动备用**
   (日志标注 "F7 fallback");
3. **v6.38 的 end_frame 补合成保留**——它解决的是"无高亮帧面板可见
   性"(暂停背包 UI3D 不逐帧重画,合成必须有 pass 触发的老链路断),
   与生命周期正交;读档闪现修复(data3D 非虚提前泊位)原样保留;
4. **P 的构建时序注意**:背包打开 = 游戏暂停,spawn 放置可在 sink 里
   做,但构建(tick)只在未暂停帧推进(run 49 教训)——首会话第一次
   开背包时 P 可能未建成,面板显示黑矩形;关背包后 pump 自动出生链
   (v6.15)建成,第二次开背包即有人物。此为既有全会话存活设计的预期
   行为,验收时注意区分。

**判读(run 73)**:①进背包面板立即出现(无需高亮、无需 F7),关背包
面板消失;②读档零克隆体闪现;③F7 仍可手动开关(备用);④读档/主菜
单/退出的强制关路径不回归(kPreLoadGame/kNewGame/kQuitGame/MainMenu
即关);⑤灯光/朝向/取景无回归。

## 0av. Run 71 结果(v6.37 灯光成立;用户报两新 bug:面板依赖高亮 + 读档克隆体闪现)+ v6.38 方案

v6.37 会话(2026-10-03 下午,用户确认):

- **灯光成立**——NiLight 节点路径生效,人物正面受光正常。遗留:摄影棚
  光照与世界内效果有差异(用户明示暂不考虑,后续重点);
- **新 bug 1:面板依赖高亮**——背包中按 F7,只有先高亮 3D 物品后面板
  才出现;应解耦(按 F7 立现);
- **新 bug 2:读档克隆体闪现**——载入存档的开头几帧,玩家附近出现克
  隆体,几帧后才消失。

**Run 70/71 日志取证(合成断供的定量实锤)**:整段面板会话
`composite draw` 仅 **1 次**(面板打开瞬间的物品爆发 pass 触发);
`in_menu_frame=1` 的 P pass = **0 条**——稳态菜单帧零 pass 到达 thunk,
`replay_after_original` 的合成路径根本不跑。机制:合成挂在
replay_after_original,而暂停背包的 UI3D accumulator 不逐帧重画
(run 43);无高亮 = 连爆发 pass 都没有 = 面板零合成。高亮的本质作用 =
提供那一轮菜单 pass,让合成跑一次。

**v6.38(2026-10-03 DLL,MD5 82bb9648…,待游戏验证)= 两修**:

1. **面板-高亮解耦**:end_frame 括号关闭前,若本帧 replay 相位未合成
   (`m_composited_this_frame` 闩,begin_frame 复位、replay 合成置位)
   且 `s_panel_rtv` 在,把四边形补画进最后捕获的 format-28 实例。与
   run 24 证伪的差别:v4.5 画的是当帧 replay 的 prev_rtv(当时每帧有
   item pass、实例刚被 merge 消费);现在画的是持久捕获实例,且稳态帧
   零 pass = 引擎未重画 = 该实例很可能就是引擎持续 merge 的那个
   (run 70 composite #1 后面板持续可见佐证 merge 读同一实例)。日志
   `end_frame composite #N` 判读;
2. **读档闪现消除**:宽限期(60 帧)内不能对 actor 调虚函数(spike
   [rax+0x38] 铁证,Get3D 也是虚的),改走**纯数据路径**:
   `clone->loadedData->data3D`(0x68,NiPointer)非空即图已建——立刻把
   3D 根 local.translate 平移到玩家 3D 下方 8k + `UpdateDownwardPass`
   级联,每个宽限 tick 重做;`park_below_player`(actor SetPosition)
   在宽限结束照旧作为引擎权威泊位。效果:3D 出现的那一帧就在地底,
   玩家位零可见窗口。

**判读(run 72)**:①背包开箱按 F7(不碰任何物品)面板立即出现;
②读档全程玩家旁无克隆体闪现;③灯光/朝向/取景无回归;④若①不成立,
`end_frame composite` 行出现与否分叉:出现但不显示 = 实例选择错
(下一层:钩 UI 合成点/自建实例);未出现 = 闩逻辑 bug。若②仍闪,
检查 data3D 时序(闪现帧是否在 graph 建立前)。

## 0au. Run 70 结果(v6.36:灯位落点精确到位但画面零变化 = BSLight::worldTranslate 不是着色器输入)+ v6.37 方案(先查类型与消费字段再处理,用户方法论)

v6.36 会话(2026-10-03 13:47,用户截图 + 日志):

- **归一化生效**:`row_lengths=(6.27,11.15,1.00)` 与 run 69 反解完全一
  致;灯位落点 `new=(±22.5,-415.4,50.0)` = 设计意图(锚点 (0,-485.4,0)
  前方 70、上方 50、左右 ±22.5)精确到位;
- **但画面零变化**(用户截图与 run 69 同形态:正面近黑、顶光轮廓)——
  这本身就是决定性证据:**改 BSLight::worldTranslate 不影响着色**;
- 用户方法论插入:**先确定灯光类型(方向光/点光)、再确认哪些字段可调
  且被消费、最后才决定处理方式**——停止盲调。

**源码调研(本轮完成,证据全在本地仓库)**:

1. **着色器读哪**:`skyrim-community-shaders` 的 LightLimitFix 填着色
   器常量用 `SetLightPosition(light, niLight->world.translate, inWorld)`
   (LightLimitFix.cpp:275→341);InverseSquare 的 `BSLight_GetLuminance`
   衰减距离也用 `niLight->world.translate`(:109)。**灯光位置进入着色
   的唯一来源 = NiLight 场景图节点的 world 变换**(NiLight : NiAVObject,
   world 在 0x7C);
2. **怎么改才对**:CS LightEditor 移动灯的唯一写法 = 
   `niLight->parent->local.translate = pos; ni_light->parent->Update(...)`
   (LightEditor.cpp:2088-2092)——改 parent 的 local 再级联,让
   NiLight::world 重算;
3. **BSLight::worldTranslate 的真实角色**:剔除/登记副本(CS 全源码零
   着色消费;Engine 是 culling process 把 BSLight 登记进 accumulator),
   run 70 实证写着色器不消费;
4. **灯光类型判别**:`BSLight::pointLight/ambientLight` 标志 +
   NiLight 的 NiRTTI 链(NiDirectionalLight vs NiPointLight);强度/
   半径在 `NiLight::GetLightRuntimeData()`(AE 0x110:ambient/diffuse/
   radius/fade)。run 69/70 的 `lum=0.000` 但受光可见 = 亮度另有来源
   (原生路径不走 LLF 的 luminance),普查日志一并核实。

**v6.37(2026-10-03 DLL,MD5 56b86c98…,待游戏验证)= rig 改走节点
路径 + 灯光普查**:

1. rig mutate 目标改为 **NiLight 节点**:`parent->local.translate +=
   R^T(target − parent.world.translate)` + `parent->Update(kDirty)` 
   (LightEditor 同款,world.rotate 是 local→world,转置即逆);恢复同
   路(local.translate 还原 + Update);BSLight::worldTranslate 写保留
   (零成本,万一原生剔除读它);
2. **一次性灯光普查**(首例详情行升级):每盏灯的 NiRTTI 名/基名、
   point/ambient/dynamic 标志、radius.x、fade、parent 节点名、
   node_moved 实际生效位——为下一轮决策(继续借灯 vs 建自己的摄影棚
   灯)提供类型事实;
3. 朝向保持 Rz(0)(run 69 已定),其余零改动。

**判读(run 71)**:①人物正面受光(脸/胸亮)——节点路径生效的直接
证据;②普查行出现 `NiPointLight`(或方向光)与 radius/fade 数值,记录
备用;③若节点路径仍不生效(灯 parent 为空/不可级联),下一层 = 放弃
借灯,直接**建两盏自己的 NiPointLight 挂进 UI3D 场景**(类型事实已备,
LightEditor 已示范引擎接受运行时灯);④其余不回归。

## 0at. Run 69 结果(v6.35:朝向已对但被顶光误判;rig 基向量算术实锤非单位长)+ v6.36 方案

v6.35 会话(2026-10-03 13:04,用户截图 + 日志):

- **朝向判定反转:人物实际已面向相机**——用户报告"方向不对",但截图
  增强(2.2×)判读五官清晰可见(眉/鼻/唇),发辫在脑后 = 正面朝相机。
  v6.35 的 Rz(0) 修复本身生效;
- **真正缺陷:正面近黑**——只有头发、肩/臂上缘、衣服褶皱被照亮,是
  典型顶光形态(灯在头顶上方,打亮上表面,面部处于掠射角);
- **rig 日志算术实锤根因**:`light[0] new=(141.1,-415.3,557.6)`、
  `light[1] new=(-141.1,-415.3,557.6)`,锚点 (0,-485.4,0)。按 rig 公式
  `anchor − fwd×70 + up×50 + right×±22.5` 反解:**|right 行|=6.27、
  |up 行|=11.15、|fwd 行|=1.00**——worldToCam 三行方向全对(+X/+Z/−Y)
  但**不是单位向量**,携带菜单相机缩放。`up×50` 实落 +557.6 Z、
  `right×±22.5` 实落 ±141.1 X:两盏灯在 74° 仰角头顶上方(意图 34°)。
  v6.33/v6.34/v6.35 三轮"背光→翻朝向"摇摆同源于此:每次都动朝向、
  rig 落点从未对过;
- 附证:两盏灯 `lum=0.000` 但 point/ambient 标志齐,受光可见 = 灯光
  强度另有字段(缺省亮度),不影响本轮判断。

**v6.36(2026-10-03 DLL,MD5 a7d86888…,待游戏验证)= rig 基向量归一
化**:`pose` 的 rig 块把 w2c 三行读出后先 `Length()` 归一化再进偏移
乘积(零向量/退化保护 1e-6);朝向保持 Rz(0)(run 69 判定已对);
rig 一次性日志追加 `row_lengths=(r,u,f)` 持续监测缩放行为。其余零改
动。

**判读(run 70)**:①人物正面受光(脸/胸亮,面部不再黑);②灯位落
点回到意图量级(new ≈ ±22.5 / −555 / +50 邻域);③朝向无回归(正面
仍朝相机);④其余(主菜单零帧、物品解耦、克隆体不可达)不回归。若
亮度/角度仍不理想,rig 的 70/50/45 三个偏移常量即旋钮。

## 0ah. Run 58 结果(v6.24:T-pose 姿势正确但朝向背对;交互目标是现役克隆,标志位实证无效)+ v6.25 方案

v6.24 会话(2026-10-03 00:27–00:29,用户双截图 + 日志):

- **T-pose 姿势修复成功**(分组重置生效,人物直立)——剩朝向问题(背
  向相机,根 Rz(180°) 与 T-pose 正向约定不符),用户指示搁置;
- **决定性证据:交互目标名字 = "CharacterPanel_Clone"**(v6.24 标记
  名)——用户对话的就是我们被钉桩的现役克隆体,不是残留。钉桩日志
  `pinned (ref=0xFF0063BE)` 在先,交互在后:**SetActivationBlocked 与
  BOOL_FLAGS 位在该 actor 上实证无效**;幽灵化正常(克隆体不可见,用
  户确认图中人物是玩家角色);
- 残留清扫跑通(会话内 ~24 次扫描)但零删除——旧 "3BA" 克隆不在玩家
  到访的 cell(或已不存在);清扫保留(标记名对未来存档残留精确匹
  配);
- **面板开关零 despawn**(opened 00:29:11 / closed 00:29:32,无
  disarmed 行)——回答用户问题:面板状态不控制克隆体的世界存在,"保
  持面板常开"不是规避手段,也不必要。

**v6.25(2026-10-03 DLL,MD5 3b25cbbb…,待游戏验证)= 几何隔离**:

run 58 实证 AI 标志压不住这个 actor,换成距离这个压得住的杠杆:每帧
把克隆体泊到玩家正下方 8000 单位(= run-31 route-3 的已验证配置——该
深度世界剔除器仍收它、pass 照发、设备缓冲照常初始化)。交互/对话/碰
撞的游戏距离仅数百单位,泊位后**天然不可达**。要点:

1. 引擎更新链会覆盖一次性 SetPosition(run 38),故 kAttached 每 tick
   重泊(route 3 同款,`Actor::SetPosition(pos, true)` 连控制器一起搬);
2. 摄影棚路径绘制时重摆根 local,与泊位完全解耦(v6.2 已确立);
3. 幽灵化照旧(双保险,且 fail-open 方向安全:即便抑制失效,8k 深处
   也不可见、不可达);
4. AI 标志、标记名、残留清扫全部保留为次要防线;
5. spawn 宽限期(约 1 s)内克隆体仍在玩家位置——虚函数安全窗口之前
   不能泊,维持既有可接受窗口。

**判读(run 59)**:①世界中无法再交互/对话/碰撞克隆体(第三人称走近
无提示、无推挤);②面板人物照常直立(T-pose 朝向问题已知、搁置中);
③幽灵化/残留清扫无回归;④若泊位导致世界不再为它生成 pass(与 run 31
矛盾),预热不会武装、缓冲缺失 → 面板零绘制(rd=9 复发)——届时回退
泊深(如 2000)或另想办法,判据是面板人物是否照常出现。

## 0aj. Run 59 结果(v6.25 验证通过:世界中不再遇见克隆体)+ v6.26 方案(朝向 + 取景)

用户验证(2026-10-03):**v6.25 几何隔离生效**——世界里再也碰不到克隆体。
用户决策:先做 T-pose 朝向与取景微调,"摄影棚即家"(§0ai route 2 复活)
后置。

**v6.26(2026-10-03 DLL,MD5 c78b2028…,待游戏验证)= 两项**:

1. **T-pose 朝向**:Rz(180°) 是给图驱动态校准的(run 55),T-pose 骨架的
   **授权朝向**与之相反——run 58 截图背向相机。改为命名常量
   `Studio_Facing_Z_Rad = 0°`(身份旋转)。若一轮后仍不对,该常量即唯
   一旋钮;
2. **取景解算(取代 v6.20 的固定猜值)**:每帧按
   `figure_scale = Framing_Target_Height_Fraction × depth × tanHalfY ÷ body_r`
   解出缩放——depth = |锚点 − 相机世界坐标| 欧氏距离(零矩阵约定风险,
   锚点近视轴,轴向 ≈ 欧氏),tanHalfY = 引擎相机自己的
   `viewFrustum.fTop / fNear`(UI3DSceneManager::camera,CLib
   GetRuntimeData2),body_r = scale=1 时 T-pose 世界包围盒半径(确定性
   姿势,稳定输入)。目标占比 `Framing_Target_Height_Fraction = 0.85`
   (人物约占摄影棚视高 85%)是**唯一旋钮**;尺寸不再跟随高亮物品。旧
   `Studio_Figure_Scale = 1.0` 降级为相机/视锥读取失败时的回退。标定转
   储升级为 v6.26 字段(anchor/item_scale/cam_depth/tanHalfY/body_r/
   figure_scale/居中后 bound)。

**判读(run 60)**:①面板人物面向相机(T-pose 正面);②人物体高约占
面板 85%、居中(过小/过大 → 按转储的 cam_depth/tanHalfY/body_r 与截图
实测占比换算 Framing_Target_Height_Fraction 修正,一轮定值);③若
`cam_depth=0`/`tanHalfY=0`(相机读取失败)则回退 scale=1.0 并观察转储;
④克隆体世界不可达(v6.25)与幽灵化无回归。

## 0as. Run 68 结果(v6.34:正面受光成立、主菜单零帧;唯人物背对玩家)+ v6.35 方案

v6.34 会话(2026-10-03 12:49,用户截图):

- **正面受光成立**(灯光 rig 生效,人物正常受光不再昏暗)、**主菜单零帧
  面板成立**(IsMenuOpen 当帧关闭);
- **缺陷:人物背对玩家**(截图可见发辫与背弓)——Rz(π) 面朝 away。综合
  两轮观察:Rz(0) 时用户看到的是正面但当时(v6.33)灯光尚在背后,故报
  "灯光在背部";Rz(π) 灯光已修正到玩家侧,用户看到的是背面。**正确朝向
  = Rz(0)**;
- 用户验收口径:灯光方向 ✓、面板位置/高度 ✓、比例 ✓、主菜单 ✓。

**v6.35(2026-10-03 DLL,MD5 274e5a8b…,待游戏验证)**:

`Studio_Facing_Z_Rad` 翻回 **Rz(0)**(正面朝向玩家,与已验证的正面灯光
rig 配合)。其余零改动。

**判读(run 69)**:①人物正面朝向玩家(可见面部/胸前);②正面受光保持;
③其余(主菜单零帧、物品解耦、克隆体不可达)不回归。若朝向仍不对,转
储与截图一轮定角度(常量即旋钮)。

## 0ar. Run 67 结果(v6.33:rig 确认触发但人物背光;主菜单残留一帧)+ v6.34 方案

v6.33 会话(2026-10-03 12:06,用户截图 + 日志):

- **日志实锤:rig 已触发**——`frontal light rig: 2 light(s) repositioned
  (menu-lights=1)`(新鲜度门竟处于 armed,菜单灯数组生效),2 盏灯已移到
  锚点前上方;且**无任何 9-15 区间 SKSE 消息**——退出消息确实不存在,
  pump 路径是唯一可靠机制;
- **缺陷 1:人物背光**——灯光位置已验证在相机与人物之间,人物仍背光 ⇒
  **人物背对相机**:v6.26 把朝向翻到 Rz(0) 的依据是 run 58 的"背向"判
  断,而那次判断是在骨架折叠(头在地上)的坏渲染上做出的,作废;朝向翻
  回 Rz(π);
- **缺陷 2:主菜单残留一帧**——存活检查以"克隆 3D 消失"为触发,而世界
  卸载滞后菜单加载一帧 → 首个菜单帧仍走括号路径合成一次。修复:以
  `IsMenuOpen("Main Menu")` 为触发(菜单出现的当帧即关,早于卸载)。

**v6.34(2026-10-03 DLL,MD5 8995f508…,待游戏验证)**:

1. **朝向翻回 Rz(π)**——灯光 rig 已验证正面,背光即面朝 away;
2. **主菜单即关**:pump(kAttached 时)检查
   `UI::IsMenuOpen(MainMenu::MENU_NAME="Main Menu")`,命中即 despawn +
   close_panel——首个菜单帧前生效,面板零帧出现在主菜单;
3. **灯光诊断升级**:一次性记录每盏灯的 point/ambient 标志、luminance、
   新旧位置——若朝向翻转后仍背光,即证据链指向"worldTranslate 非渲染
   实际读取字段",下一轮改突变 NiLight 节点变换。

**判读(run 68)**:①人物正面受光(脸/胸亮);②主菜单零帧面板;③若仍
背光 → 读转储的灯具新旧位置与标志,转 (b) 路线(NiLight 节点突变);
④其余不回归。


## 0aq. Run 66 结果(v6.32:比例/崩溃修复成立;灯光仍偏侧 + 主菜单残留面板)+ v6.33 方案

v6.32 会话(2026-10-03 03:25,用户双截图):

- **比例修复成立**(人物全身、正确比例、不拉长)、**崩溃消失**(pump 存
  活检查生效);
- **缺陷 1:灯光仍偏侧**——v6.32 的正面 rig 与菜单灯数组共用新鲜度门,
  而稳态菜单里物品 pass 不来(run 43 的老发现)→ 门关闭 → rig 不动、数
  组交换也不发生 → 人物落在自身 pass 的**地牢灯**上(暗、偏侧),与截图
  吻合;
- **缺陷 2:主菜单残留面板**——pump 存活检查只 disarm 了 P、**没关面
  板**:面板状态仍开 → 主菜单帧继续走括号路径 → 合成把最后一幅 studio
  图画上主菜单。世界已卸载 = FR-06 的"无有效游戏会话"。

**v6.33(2026-10-03 DLL,MD5 b03c0319…,待游戏验证)**:

1. **每 pass 正面灯光 rig(新鲜度门无关)**:灯光位置突变移入 per-pass
   覆盖块,作用于 pass 实际使用的灯光(新鲜时 = 菜单灯;否则 = pass 自
   带的地牢灯——每帧都有,稳态可用);锚点/相机轴系从 studio_anchor +
   worldToCam 现取;保存/恢复按同一索引空间;一次性日志
   `frontal light rig: N light(s) (menu-lights=?)` 顺带校准新鲜度门的真
   实行为;
2. **世界丢失关面板**:pump 存活检查在 despawn 之外追加
   `close_panel("world unloaded")`——pump 位于 DrawInterfaceStart 入口,
   同帧的 panel_frame_active 读数即走非括号路径,主菜单不再合成;
3. **SKSE 消息诊断**:handler 的 default 分支有界记录 type 9-15(退出消
   息 10/11/12 两轮未生效,下轮按真实值校准)。

**判读(run 67)**:①人物正面受光(脸/胸亮,转储日志
`frontal light rig: N (menu-lights=?)` 出现);②退出到主菜单面板消失、
不再残留;③`SKSE message type=` 行出现(记录退出时打出的真实值,校准
v6.31 的 case);④比例/崩溃/物品解耦不回归。


## 0ap. Run 65 结果(v6.31:半面板修复;人物拉长 + 灯光偏侧 + 回主菜单仍崩)+ v6.32 方案

v6.31 会话(2026-10-03 03:00,用户游戏截图 + crash-2026-10-03-03-01-16):

- **半面板修复成立**(面板占满红框全高)、CB 分离生效;
- **缺陷 1:人物明显拉长**——合成把 16:9 目标整幅挤进 0.51 宽高比的面
  板,横向压缩 ~3.5×(各向异性)。用户要求"以世界空间的 scale 渲染";
- **缺陷 2:灯光不在正面**——记录的菜单灯光为物品预览位置布置,人物站
  到视轴锚点后打光偏侧;
- **缺陷 3:回主菜单仍崩**——crash 日志点名:**TraverseScenegraph-
  Geometries 遍历悬空的 P 根**(MistMenu/MainMenu 已在场,世界卸载摧毁
  克隆体 3D 图,渲染线程的摄影棚绘制还在跑)。v6.31 的消息 case(10/11/
  12)未能阻止——时序或值不可靠,渲染侧需要自己的存活防线。

**v6.32(2026-10-03 DLL,MD5 58b98262…,待游戏验证)**:

1. **纵横比正确采样窗**:合成 PS 把采样 x 范围缩到
   `(MaxX-MinX)/(MaxY-MinY)` = 0.286(经 g_flags.y 传入,CB 常量与分辨率
   无关)——目标中央 16:9→0.51 窗口,人物保持世界比例(消拉长);
2. **scale 重标定 0.35**:窗口变窄后 T-pose 臂展(≈身高)约束宽度——
   0.70 时臂展超窗 ~2×;0.35 全身入框、比例正确、约占面板高一半
   (T-pose 1:1 剪影在 0.51 窄面板的几何上限;M1 手臂下垂姿态可更高);
3. **正面灯光 rig**:绘制前把记录灯光的 `BSLight::worldTranslate`(0x50
   可写)移到锚点前上方(沿相机 right 轴展开、up 抬高、−forward 拉向相
   机),pass 循环后恢复——与灯光数组交换同一 mutate→call→restore 契
   约,物品自身的绘制已在 call site 用原灯完成,互不干扰;
4. **pump 存活检查(崩態试剂)**:游戏线程 pump 在 kAttached 时校验克
   隆 ref 与 3D 存活,丢失即 disarm——pump 位于 DrawInterfaceStart,先于
   本帧渲染工作,把"悬空根窗口"从"整个退出过程"压到"一帧以内";
   消息 case(10/11/12)保留作次级防线。

**判读(run 66)**:①人物比例正常(不再拉长)、全身可见、约占面板高一
半;②灯光从正面打(脸/胸亮);③开面板退出到主菜单/桌面不崩(若仍崩,
新 crash 日志点名);④面板位置/高度、物品解耦、克隆体不可达均不回归。


## 0ao. Run 64 结果(v6.30:面板位置正确但只有一半高度;开面板退出游戏崩溃)+ v6.31 方案

v6.30 会话(2026-10-03 02:2x,用户截图 + 退出崩溃报告):

- **缺陷 1:面板只有红框的上半部分**——合成 CB 的 y 分量**身兼两职**:
  VS 把 `g_ndcRect.y` 当矩形底边(yBottom),PS 把它当 HDR 标志
  (`>0.5`),而绘制路径每帧 `out[1] = hdr` 覆盖之 → LDR 时 yBottom=0
  (NDC)→ 四边形从屏幕中线画起 = 恰好半面板。**回看旧截图证实此 bug
  从 v4.6 就存在**(旧面板实渲染 12-50% 而非常量声称的 12-68%),矩形
  改高后才暴露;
- **缺陷 2:开面板退出游戏 crash**——退出路径无生命周期处理:摄影棚绘
  制与合成继续跑进正在拆除的渲染器,泊位克隆体的图随世界卸载悬空。

**v6.31(2026-10-03 DLL,MD5 c0eaf4cb…,待游戏验证)**:

1. **CB 分离**:扩展到 8 浮点(两个 float4)——`g_ndcRect`(完整矩形)
   + `g_flags`(x = HDR 标志);VS/PS 同步;每帧写入完整四边矩形 +
   g_flags.x = hdr,几何与状态彻底解耦;
2. **退出生命周期**:消息处理器新增 case 10/11/12(SKSE kShutdown /
   kExitGame / kQuitGame——CLib 枚举止于 kDataLoaded,续值按 SKSE API
   表)→ `set_world_ready(false)` + `close_panel("game exit")` +
   `despawn()`——与读档路径同一套防御;
3. 其余不动(红框矩形、蒙皮对齐、scale 0.70)。

**判读(run 65)**:①面板占满红框全高(5-96%);②T-pose 全身入框居中
(半面板修复后首次真正可验);③开面板退出游戏/回主菜单不崩;④其余不
回归。


## 0an. Run 63 结果(v6.29:物品解耦/菜单限定/不可达全部成立;人物过大偏低只见腿)+ v6.30 方案

v6.29 会话(2026-10-03 02:27,用户游戏截图 + 红框标注):

- **三项验收通过**:物品切换不影响面板、世界空间 F7 无面板、克隆体不
  可达——v6.28 的两项修复与 v6.25 几何隔离全部成立;
- **缺陷:面板内只见腿**——figure_scale 1.80 过大(视高只装下约 45% 身
  体),且身体偏低。两个原因:①尺寸公式链的 tanHalf 标定(0.605)仍偏
  大 5×;②居中基准用根节点 worldBound,**随身武器(背弓)把包围盒中
  心顶高**,身体视觉中心下垂 → 视野里只剩下半身;
- 用户决策:**面板矩形移到红框**(右侧竖条,73%..99% × 5%..96%),人物
  全身对齐。

**v6.30(2026-10-03 DLL,MD5 4dd619fc…,待游戏验证)**:

1. **面板矩形**:58-88% × 12-68% → **73-99% × 5-96%**(用户红框;窄高
   条,合成把 16:9 目标挤进 0.29 宽高比矩形,横向压扁是映射固有特性);
2. **对齐基准 = 仅蒙皮网格包围盒**(新 `measure_skinned_bound`:遍历蒙
   皮几何,球体联合,排除武器/箭袋等非蒙皮挂件)——居中与标定都用它,
   武器不再拖偏中心;
3. **尺寸 = 直接标定常量 `Studio_Figure_Scale = 0.70`**:run 63 渲染证
   据——scale 1.80 时视野只装下约 45% 身体 → 视高 ≈ 0.45×125×1.8 ≈
   101 单位 → 全身(~125 单位)85% 占比需 0.85×101/125 ≈ 0.69 → 0.70。
   公式链(v6.26-29)退役:三个输入里两个(相机节点变换、viewFrustum)
   被证不可靠,直接常量是唯一诚实的选择;转储保留标定字段供一轮微调。

**判读(run 64)**:①面板位于右侧竖条(红框位置);②T-pose 全身(头
到脚)完整入框、居中;③若偏差,转储的 skinned_body_r/figure_scale +
实测占比一轮修死(改 Studio_Figure_Scale 一个常量);④窄条横压扁属映
射固有,视觉不可接受时 M1 阶段调摄影棚相机纵横比。


## 0am. Run 62 结果(v6.28:固定锚点/菜单限定合成生效;viewFrustum 读数崩掉尺寸)+ v6.29 方案

v6.28 会话(2026-10-03 02:13,用户 RenderDoc VS 截图 + 转储):

- **固定视轴锚点生效**(dump:anchor=(0,-485.4,0),人物投影 NDC ≈
  (0,0.04) 居中;用户确认位置跟随物品/世界面板泄漏两项均已解决);
- **缺陷:人物缩成微尘**——转储点名:**`tanHalfY=0.0060`**(等效 0.7°
  FOV)——`viewFrustum.fTop/fNear` 不是实际生效的投影(陈旧/单位不
  符),尺寸解算崩成 figure_scale=0.018(bound r=2.5);
- **有效标定数据**:`body_r=138.4`(T-pose scale=1 真实包围盒,含展开手
  臂与随身武器)、`w2c_t=-15.0`(眼点在原点后方 15 = 近平面偏移,固定
  锚点方案成立)、`depth=485`。

**v6.29(2026-10-03 DLL,MD5 f640ba0c…,待游戏验证)= 半角标定常量**:

`Studio_TanHalfY = 0.605` 由渲染证据反解:v6.24 时代 root scale=1.0 时
人物约占视高 30%(用户截图),配合 run 56 的 bound 数据(r=25.4 @
root scale 0.288 → 88 @ 1.0)→ tanHalf = 88/(485×0.30) ≈ 0.605(~62°
FOV,数值可信)。viewFrustum 读取退役。解算:
`figure_scale = 0.85 × 485 × 0.605 ÷ 138.4 ≈ 1.80`。

**判读(run 63)**:①人物以正常尺寸居中显示(注意 body_r 含武器/手臂,
视觉体高可能略小于 85%——可接受则微调
`Framing_Target_Height_Fraction` 一次定值);②朝向正面;③物品切换不
再影响面板;④其余(克隆体不可达/菜单限定合成)无回归。


## 0al. Run 61 结果(v6.27:人物入画、朝向成立;位置随物品移动 + 面板泄漏到世界)+ v6.28 方案

v6.27 会话(2026-10-03 01:4x-01:56,用户 RenderDoc VS 截图 + 游戏截图):

- **人物入画**(SV 入界,深度 ~485 与 run 55 同源——worldToCam 深度换源
  生效),**朝向无再报**(Rz 0° 成立);
- **缺陷 1:面板内人物位置随所选 3D 物品变化**——锚点 = 物品预览
  world.translate(v6.3 起设计),而管理器为每个物品重新摆位,人物跟着
  跳。用户验收:人物位置不应随物品改变;
- **缺陷 2:F7 在世界空间开启时,特定角度下面板仍可见**——合成在
  replay_after_original 内,而世界帧被抑制的 P pass 仍触发它(replay
  为真与抑制无关);特定角度下泊位克隆体进视锥 → pass 到达 → 面板画
  到世界 HDR 目标上。与"面板仅菜单可见"(PRD §6.1)相悖。

**v6.28(2026-10-03 DLL,MD5 3854fcbf…,待游戏验证)**:

1. **固定视轴锚点(物品彻底解耦)**:眼睛在世界原点(run 36 + w2c_t 转
   储佐证)、视线方向 = worldToCam 第 3 行,锚点 = 视轴上
   `Studio_Depth = 485`(run 55 实证的入界深度)处。关键合成事实:合成
   四边形把**整幅 studio 目标挤压进面板矩形**(UV=[0,1],v4.6 实现)——
   视轴点 NDC (0,0) 恰好落在**面板正中**。人物位置/尺寸与物品完全无关,
   无物品时面板照常(回退逻辑可删);物品扫描从 pose 中整体退役;
2. **尺寸解算简化**:深度 = 常量 Studio_Depth,`figure_scale = 0.85 ×
   485 × tanHalfY ÷ body_r` ——稳定实测值,唯一旋钮不变;
3. **合成加菜单帧门控**:`m_in_frame` 才画合成。世界帧被抑制的 P pass
   不再触发面板;稳态菜单的合成路径(UI3D 累加器登记的 P pass,在括号
   内到达)不受影响。摄影棚绘制(draw_p_proactively)在世界帧继续运行
   ——保留"面板开启时克隆体从世界消失"的已验证行为。

**判读(run 62)**:①切换不同物品,面板人物位置/尺寸纹丝不动、居中;
②无高亮物品时开面板人物照常;③世界空间 F7 任意角度不再出现面板;④
面板人物朝向正面、体高 ~85%(v6.27 已达成项不回归);⑤克隆体不可达、
幽灵化无回归。


## 0ak. Run 60 结果(v6.26 取景失败:人物出画,标定数据随日志截断丢失)+ v6.27 方案

v6.26 会话(2026-10-03 01:36,用户截图):面板全黑、人物不可见("又到远
处去了")——取景解算产出了错误缩放。**失败会话的标定转储随游戏重启被
日志截断吞掉**(spdlog 每会话清空),无法事后判读。

**根因分析(代码推理)**:v6.26 的深度用 `camera->world.translate`(节
点世界坐标)到锚点的欧氏距离,而 run 36 早已证明 **UI3D 相机的真实视
变换(worldToCam 矩阵)平移在原点**——节点上的 world.translate 是与视
变换脱节的陈旧值(引擎把它摆到别处),深度 = |锚点 − 错误眼点| 完全失
真(过大 → 巨型出画;过小 → 微型),两种方向都与"面板全黑"相容。朝向
修正(Rz 0°)与本次失败无关(旋转不影响可见性)。

**v6.27(2026-10-03 DLL,MD5 f3efed14…,待游戏验证)= 深度换源**:

1. **深度 = 锚点在 worldToCam 第 3 行(视图行)上的投影取模长**:
   `|anchor · w2c[2][0..2]|`。因矩阵平移在原点(run 36),**没有平移项
   约定可错**,取模长连符号约定都免疫;该矩阵就是引擎投影实际使用的视
   变换,深度与 run 55 的 w≈500 同源;
2. 节点距离与矩阵平移项(w2c[2][3],应 ≈0)降级为对照诊断项,进转储;
3. tanHalfY(viewFrustum.fTop/fNear)、body_r(scale=1 T-pose 包围盒)、
   `Framing_Target_Height_Fraction = 0.85` 全部保留;朝向 Rz 0° 保留;
4. 转储升级 v6.27 字段(w2c_depth / w2c_t / node_dist 双深度对照),每
   会话前 6 帧。

**判读(run 61)**:①面板人物出现、朝向正面、体高约 85%;②若仍出画
→ 转储的 w2c_depth 与 node_dist 对照 + figure_scale 直接点名哪一项失
真,一轮修死;③若 w2c_t ≫ 0(矩阵平移不在原点)→ 深度公式需补平移
项,按转储处理;④克隆体不可达/幽灵化无回归。

## 0ai. 架构方向(用户提议,2026-10-03):统一之家——摄影棚即居所

用户提议:克隆体应常驻一个"玩家永远不会到达的统一位置",面板需要显示
时才显示,其他时候跳过渲染——而非在面板关闭时聚在玩家附近。

**现状映射**:三点中两点已成立——①面板关闭时整条摄影棚路径直通跳过
(draw_interface_start_thunk 的 panel_frame_active 早退),只在面板开着
时渲染;②世界侧像素被幽灵化永久抑制(GPU 零绘制,仅剩廉价 pass 生成)。
v6.25 的泊位已把克隆体放进"不可达居所"。

**引擎硬约束(cell 加载)**:世界 actor 的 3D 图只在所在 cell 加载期间
存在——固定的远距离世界坐标会导致 cell 卸载时 3D 被销毁、白名单根悬空
(run 51 崩溃模式的泛化)。故克隆体 x/y 必须跟随玩家 cell,引擎只留下
深度一个自由维度——"玩家正下方 8k"是"统一不可达居所"在现有路线下的
唯一形态。

**字面意义的统一之家 = route 2 复活(前提已变)**:run 29/30 证伪
route 2 的理由(菜单剔除器不收外来图 → 零 pass)在 v6.8+ 主动生成 +
v6.17+ 引擎链自绘的架构下不再成立。复活形态:

1. spawn(未暂停)→ 世界渲染初始化缓冲(~2 s,保留)→ 3D 图摘出世界、
   挂进 UI3D 菜单场景专用根 → 杀壳 actor → 世界无克隆体痕迹;缓冲随
   几何体走;
2. 摄影棚绘制不变(主动生成 + call_site_original),锚点即居所;
3. 可退役:幽灵化预热/抑制、泊位与重泊、残留清扫大半、白名单世界帧匹
   配——FR-06 从逐项防御变结构性成立;
4. 未知数:①SetupAndDrawPass 对零渲染几何的缓冲初始化(run 53 有"世
   界已渲染"混淆——按上面顺序用世界渲染先初始化可绕开);②UI3D 场景
   跨菜单切换是否清除外来挂接(loadedModels 常驻证据偏乐观);③摘挂与
   在途 pass 竞态(route 2 时代退役列表机制需请回,F4)。

**决策**:先验证 v6.25(泊位形态,同时是复活路线的安全网);通过后把
"摄影棚即家"立项为下一个工作包,T-pose 朝向与取景微调排在其后(摆姿
数学届时可能随菜单场景坐标一并调整)。

## 0ag. Run 57 结果(v6.23 首验:钉桩执行但世界侧仍可交互;T-pose 骨架折叠)+ v6.24 方案


v6.23 会话(2026-10-02 23:58–23:59,用户双截图 + 日志):

- **钉桩代码确实执行**(`Proto P pinned` 23:58:47.325),幽灵化正常
  (`ghost armed` + `first world-stream pass dropped (geom=[shoes])`);
  但用户在世界中遇到的克隆体(交互提示名 "3BA")**仍可交互、发声、碰
  撞、自由行动**;
- **T-pose 严重变形**——面板人物头在地面(用户截图);T-pose 日志
  `74 bind bones over 3 skin root(s)` 暴露两个实现错误(见归因 B);
- **SetCollision 证伪**:CLib 的 `TESObjectREFR::SetCollision` 只改
  formFlags 记录标志(加载期语义),对运行时碰撞零效果(实现为纯位运
  算,src/TESObjectREFR.cpp:1018)——v6.24 移除该调用。

**归因 A(世界侧行为,主嫌疑 = 存档残留)**:克隆体是 PlaceObjectAtMe
放置的引用,**会随存档持久化**;此前会话在克隆体存活时存过档 → 本次读
档把旧克隆体一并带回。它不受白名单(可见)、不受钉桩、不受幽灵化——
交互/发声/碰撞/自由行动全部正常。次要可能:钉桩手段本身无效——若清扫
零删除而行为依旧,则升级 Papyrus VM 调用(SetGhost/SetRestrained,按名
字分发,无需地址,免猜 ID)。

**归因 B(T-pose 折叠,两个实现错误)**:
1. **跨参照系污染**:绑定世界变换以**各自的 skin root** 为参照;v6.23
   用一张全局共享映射驱动 3 次不同 root 的遍历——任一骨骼有 2/3 概率
   被错误参照系重写(74 骨骼 / 3 root 即证据),骨架折叠;
2. **skin root 自身被覆写**:root 在自己的绑定表里时,其 local 被覆盖
   成近似单位变换,装配基准被抹掉。

**v6.24(2026-10-03 凌晨 DLL,MD5 5ebe188a…,待游戏验证)**:

1. **T-pose 修复**:绑定表按 skin root 分组,每组只在自己的参照系内重
   置;遍历从 root 的**子节点**起步(累积起点 = 单位变换),永不触碰
   skin root 自身的 local;嵌套 root 由各组分治自然覆盖;
2. **残留清扫**(游戏线程 pump,每 300 帧 ≈ 5 s):扫玩家所在 cell 的
   引用,删除"运行时创建基座(formID ≥ 0xFF000000)且名字 = 标记名
   `CharacterPanel_Clone` 或 = 玩家名"的 actor(Disable + SetDelete),
   每笔删除单独记日志;本次 spawn 起克隆体基座打标记名,未来残留精确
   匹配(玩家本人与现役克隆跳过;mod 内容的基座非运行时创建,天然豁
   免)。旧残留可能在别的 cell——玩家走近其所在 cell 后 5 s 内被清;
3. **钉桩修正**:移除 SetCollision;钉桩日志附 ref formID——用户控制台
   点击可疑克隆比对 formID 即可判定它是不是我们的克隆;
4. 其余不动(幽灵化、预热窗、v6.19 面板条款、居中、引用式灯光)。

**判读(run 58)**:①日志出现 `residue sweep: deleted stale clone …`
→ 残留实锤;世界中不再有可交互克隆体(旧残留已删,新克隆被钉桩+幽灵
化);②若零删除而仍遇到可交互克隆 → 控制台点击它对比 formID 与
`pinned (ref=…)` 行:不同 = 非本插件的克隆(其他 mod 的玩偶,另行排查);
相同 = 钉桩手段无效,下一轮升级 Papyrus VM 调用;③面板人物 T-pose 正
常直立、居中;④v6.21 取景/灯光判据照旧(引用式灯光无崩溃)。

## 0af. v6.23 方案:AI 钉桩 + 摄影棚 T-pose(克隆体行为非自发)



用户决策(2026-10-02,v6.22 游戏验证通过——"替身确实消失"——后):第二
层处理,要求 ①克隆体动作非自发、暂设原地 T 字形、无法移动;②世界侧完
全不可察觉(交互、对话、碰撞等)。
**v6.23 已构建并部署(2026-10-02 晚,MD5 3cc24da1…,待游戏验证)**。

**需求归因(幽灵化之后的再分析)**:v6.22 之后克隆体的*画面*已不可见,
"察觉"只能来自行为副作用——推挤(碰撞)、对话/交互(E 键)、战斗反应、
声音。钉桩的目标是消副作用,而 T-pose 的唯一消费者是摄影棚。

**A. 行为钉桩(tick 的 kWaitingGrace→kWaiting3D 转换点,宽限后虚函数安
全,spike 先例;kMovementBlocked 原本就在此处设置)**:

1. `kMovementBlocked`(既有)+ `kAttackingDisabled` + `kCastingDisabled`
   (BOOL_FLAGS 直写,CLib 已验证);
2. `SetActivationBlocked(true)`(TESObjectREFR::478,CLib 已验证)——
   E 键交互/偷窃/对话发起全部失效(交互 + 对话);
3. `SetCollision(false)`(TESObjectREFR::480,CLib 已验证)——物理碰撞
   关闭(碰撞)。**已知代价与判读项**:角色的角色控制器若失去地面接触
   可能下坠——下坠无害(幽灵化不可见;摄影棚路径 v6.8 起与引擎完全解
   耦,pose_for_studio 每帧重设根 local),但需游戏内确认不下坠则更好;
4. `StopCombat()` 兜底。**本轮不做**:SetGhost(CLib 无声明,地址未验
   证,禁止猜 ID)、SetRestrained 生命态(语义有副作用风险,且非自发
   行为的可见渠道已被 2/3 封死);AI 钉桩若游戏内仍有漏网行为再升级。

**B. 摄影棚 T-pose(pose_for_studio 内,根 local 设置之后、首次级联之
前)**:

1. **绑定姿势来源**:每个蒙皮几何的 `NiSkinInstance` 带 `skinData` 与
   `bones[]`;`NiSkinData::GetBoneDataSkinToBone(i)`(CLib 已验证)给出
   skin→bone 逆绑定变换,其逆 = 该骨骼在 skin 根(rootParent)空间内的
   **绑定世界变换**;
2. **重置算法**:自 rootParent 向下递归,维护"重置后父世界"(skin 根空
   间,起点单位变换);命中绑定骨骼时 `local = parentWorld⁻¹ ∘ bindWorld`,
   非骨骼节点保留 local(NiTransform 的 `Invert()`/`operator*` CLib 已
   验证);
3. **每帧重施**:动画图在世界侧仍可能改写骨骼 local(不可见,无妨),摄
   影棚每帧绘制前重置,蒙皮矩阵(frameID 重算)读到的一定是绑定姿势;
   级联(UpdateDownwardPass)与 v6.21 动态居中照旧——bound 变成确定的
   T-pose 包围盒,**取景也随之确定化**;
4. 多 skinInstance 共享同一骨架(身体/盔甲/头发),按节点指针建映射,
   重复插入幂等;开销 ~百节点/帧,可忽略。

**判读(run 57)**:①面板人物为 T-pose、静止、居中(v6.21 取景判据在
确定姿势下复核);②第三人称走近克隆体:无推挤、E 键无交互提示、无对
话;③克隆体不下坠(若下坠但面板正常 = 可接受,记录在案);④幽灵化判
据(v6.22)不回归。

## 0ae. v6.22 方案:幽灵化第一层——透传抑制常态化(克隆体世界像素全抑制)



用户决策(2026-10-02,run 57 之前插入):先做"克隆体不被玩家察觉"的
第一层处理,取景(v6.21)与灯光验证随后同轮进行。
**v6.22 已构建并部署(2026-10-02 晚,MD5 b38bc09a…,待游戏验证)**。

**背景**:v6.16 撤 Disable 换取 3D 图存活的已知代价——克隆体是引擎 AI
体系的完全体公民,面板关闭时引擎正常画它,第三人称有一个重叠替身。抑制
机制(v6.19)在面板打开期间已完整工作,但其身份门 `p_geom` 以
`panel_open` 为前置(on_pass),面板一关即失效。

**方案(不碰 v6.19 面板内行为,只加"面板关闭"分支)**:

1. **幽灵身份 = 纯祖先链匹配(新 `is_p_descendant`,无 skinInstance
   门)**。skin 门只服务摄影棚内容筛选(run 31:道具进摄影棚是巨型色
   块);对"隐藏"而言,克隆根下的一切——蒙皮身体、盔甲、背上武器/箭
   袋挂件——都该从世界画面消失。
2. **预热窗(设备缓冲初始化保护,run 50 教训)**:引擎真实绘制是
   rendererData/VB/IB 的唯一创建者,开局即抑制会重现 rd=9 死锁。世界流
   P pass 逐个计数(到达即证明世界剔除器在收克隆体、引擎刚画过它),
   攒满 `Ghost_Warmup_Passes = 128`(实测 60–75 pass/帧,约 2 帧)才武
   装抑制。预热期替身照常绘制,叠加在既有 spawn 构建期(~1.3 s)的可见
   窗口上,首人称不可见,可接受。
3. **门控与 fail-open**:`m_state == kAttached` 才参与(spawn 构建期不
   抑制);预热不达标(克隆体未被世界渲染)→ 抑制不武装 → 替身保持可
   见,但摄影棚供给无损。despawn(读档/新游戏)经 kNone 关门;重
   spawn 重置预热(新图新几何,必须重新初始化)。
4. **抑制接线**:on_pass 对世界帧(非菜单括号)在既有早退前算
   `m_last_pass_ghost`(panel 打开且 p_geom 已命中则跳过,避免对同一
   pass 双走祖先链);`should_suppress_passthrough` 扩为
   `(replay && (p_geom || !in_menu_frame)) || ghost`;三个调用点 thunk
   透传新参。幽灵 pass 在 on_pass 内提前返回(不进面板侧计数,摄影棚
   目标语义不变)。
5. **面板关闭的菜单帧不参与**:P 无菜单流 pass(runs 29/30);面板关闭
   后 UI3D 累加器当帧不再有 P pass(登记是摄影棚绘制的逐帧副产物),
   无清理绘制残留可画。
6. **日志**:武装一次性 `Proto P ghost armed…`,首次拦截一次性
   `first world-stream pass dropped…`。阴影通道(阴影 pass 可能走未监
   控调用点)列为本轮游戏判读项;若"无体之影"残留,下一轮处理(阴影
   路径研究或 alpha 兜底)。

**判读(run 57,与 v6.21 取景/灯光同轮)**:①读档落地约 2 s 后第三人称
替身消失(spawn 后短暂可见属预期);②全程无 rd=9 复发(F7 开面板人物
成立 = 缓冲初始化未被幽灵化破坏);③影子核查——地面无无体之影;④世界
完整性不受影响(与 run 26 同判据);⑤切物品/退背包不崩(v6.21 灯光判
据照旧)。

**边界留档**:M1 换装同步(re-dress)落地时必须重置预热窗(新几何需要
重新初始化),契约记入 proto_pinstance.h;AI 钉桩与泊位(第 2–4 层隐
身手段)不在本轮。

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
