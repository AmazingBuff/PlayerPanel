# 动画、OAR 与暂停期行为图实验总结

更新：2026-10-23。本文把已过期的动作报告、测试单和旧日志提炼为可复用证据；所有原件从固定提交 `719c90bfe115a67ac64b80271dc8afced780290f` 恢复，逐文件归属见[来源表](history-sources.md)。[渲染总结](history-rendering.md)拥有取景、灯光、材质及显示生命周期历史。

当前规范是[动画](animation.md)、[OAR 适配](oar-hooks.md)、[DAC](dac.md)、[SceneGraph](scene-graph.md)与[独立行为图验证](independent-behavior-graph.md)。本文的旧热键、源码路径、部署包与方案不是当前操作接口。本次不运行游戏、不改变播放器或实验状态。

<a id="evidence"></a>
## 证据分层与时间线

**历史运行观察**、**静态证据/本地检查**、**未执行方案**、**已被后续修正的判断**分别记录。某份“测试单”可能前半是预登记、后半追加实测；以具体构建和日志段判定，不能整体算待测，也不能整体算通过。几个归档只有启动或灯光登记几行，无法支撑文件名暗示的完整会话。

| 阶段 | 实际推进与不能外推的边界 |
| --- | --- |
| S2P1–4，10-09 | 节点摆动与画面随动成立，CPU/GPU 蒙皮缓存语义未因此全定；程序化 idle 无真实动作真值。 |
| HKX1–28，10-10/11 | 名字桥、活实例、原生加载、连续轨采样与一个替换件的数值配对逐步成立，通用映射/回收未过。 |
| HKX35–44，10-11/12 | 观察池、源隔离、第二 Actor 暂停推图与选片有正结果；装备泵、完整层输出及映射暴露新缺口。 |
| AFP1–9，10-13 | 强制 generate 后可获得暂停最终输出并写回同 Actor，随后才接通直绘；生成、应用、上屏分别验收。 |
| OAR preview/idle3，10-15–18 | 按预览状态主动查询，曾试世界相位接续与文件夹序列，后被现行装备目标/原生 DAC 标记路线取代。 |
| DAC/IED 与问题追踪，10-20–23 | 修正原名、标记预检、历史手部、火把层和诊断；r1/r2 仍留下选片/姿态问题。 |
| 无第二 Actor 独立行为图，10-23 | 只做 G1 静态审计与默认关闭源基线探针；G1 未通过，G2–G6 未开始。 |

<a id="pose-probes"></a>
## 节点、蒙皮缓存和“动作看起来在动”的不同证明

早期 `s2-anim-probe-evidence-2026-10-09.md`、`s2-s3-plan.md` 及四轮原始日志记录了仪器反转：原探针在 `pose()` 之前写节点/矩阵，随后捕获姿态恢复覆盖它；写后读回只证明写入，不证明 GPU 消费，不能据此否定动画路线。`Hands` 的槽号等于槽数表示未找到，不能把返回零当采样成功；见证点若落在旋转轴上，也不能靠位置不变证明节点没动。

S2P1 把探针放到“基础摆位 → 节点修改 → pass 准备 → draw”之后，CBBE/UBE 均有用户画面变化，141/141 见证点有效。朝向只看第 0 行却绕 X 轴转，读数恒零是仪器失明；重捕后仍缓存旧目标，曾有 136 行描述停放旧图而非当前显示图。S2P2 改用相对旋转并清缓存，241/241 有效；骨盆与脊柱位于不同 CME 分支，影响上下半身不同有父链证据，不能先怪取景锚点。

两轮 T2→T3 缓存变化证明引擎绘制期会重算，但 `captured/swung` 分类在大角度处仍约各半，没有识别具体矩阵语义。S2P3 的 566 组、1132 条分类和 S2P4 TSLOT 数据帮助排除若干候选，但没有把未知槽位变成可安全写入的协议。保留的三个 `analyze_tslot*.py` 是历史分析工具，输入可从 Git 恢复；本轮不修改或执行它们。

长期规则是写副本节点，让引擎准备蒙皮；不继续猜 boneMatrices 布局。程序化摆动能验证执行顺序，不能证明真实 HKX、混合、物理或动作选择正确。当前代码和诊断入口见[动画](animation.md)。

<a id="hkx-discovery"></a>
## HKX1–15：找对活实例，再谈 binding

HKX1（`cd646aefae`）在第三人称 `DefaultFemale` 找到骨架 116 项、核心名字桥 109 命中；第一人称是另一个项目。`boneNodes[i]` 与动画骨架同名只有 37/116，表长相同绝非同序证明。缺 helper/装备附着项、body/装束改变命中数，都需逐项记录。

HKX2（`a0eaef50cd`）验证了直接四元数约定及姿态写入灵敏性：0.5 rad 扰动对照差约 125–199 单位，恢复后 0.000；但以 0.529u vs 5.591u 的平移残差宣判 `poseLocal=boneNodes` 次序，**后来被 AFP1e 的直接逐骨旋转比较推翻**。这两个数可能含平移/三轴缩放语义差，不能拿“哪个位置更小”替代排序证据。

HKX3–10 读出 15,203 个动画名字与 binding 集，经历错误偏移、扫描窗口跳过小对象、类名虚调用崩溃等问题。某次扫到 66 个 template clip，binding/control 全空，旧报告遂宣判“标准 Havok 层死路、连带自研加载也不行”；这是过度推断。HKX13 静态复核只提出强假设，随后实测（`47725682fa`）通过 `activeNodes` 找到 kClone 的 MT_Idle，binding/control 都活，正式推翻此前判决。模板树、活动克隆、`rootGeneratorClone` 和真实 ActiveClip 不能混用；在该样本 `rootGeneratorClone` 为空不等于没有活动实例。

当时 binding 包装对象的 `+0x10` 与 clip binding 双路指针一致；相邻分配观察到 0x30 间隔，但 OAR 类型声明 sizeof 0x28，不能据邻居排列改变数组步进或发布通用 ABI。应使用绑定集现有元素指针。HKX14 的 hkaAnimationBinding 转储确认 animation 在 `+0x18`，transform mapping 的 data=0/size=0；是表缺失，不是头部布局全错。HKX15 control 指向同一个空表，99 项 `transformTrackWeights` 是逐轨权重字节，不能当骨索引；虚调用其访问器不会凭空补出映射。

HKX11/12 的文件管理器地图仍有价值：loadedAnimations 指向文件/哈希注册层，不能直接当 animation；按索引解析器返回的 HashedData 是 0x20 文件信息值，读 +0x28 会落入下一条。项目注册、原始动画索引、OAR 替换索引和真实已装载对象是四个概念。源码/指针形状合理只提供候选，生命周期和资源持有另证。

<a id="hkx-loading"></a>
## HKX16–25：原生装载与采样入口的两次反转

HKX16 用错误采样入口撞上空数组；HKX17 防护后把它解释为“数据未装载”。HKX18（及其后续合集）记录 `Queue=1 Load=true`，但扫描 loader arena 没找到新动画；这说明返回成功与取得可采样数据不同。HKX20–22 的资源区扫描找到四个合理动画，仍无法证明是这次 Load 产生，before/after 相同必须保留。

转用 OAR FakeClipGenerator 的已存在路径：按已注册 binding index Queue/Load，再读回同一索引 binding-set 元素，而非继续扫描 arena。HKX23 实测 binding/animation 被引擎填充。这个加载载体只负责资源请求，**不等于独立 behavior graph**；借用 source character/setup 也不意味着推进了源图。当前载体/lease 的配对卸载仍遵守[动画规范](animation.md)。

HKX24 逐字段转储进一步修正“数据 absent”：spline 有 150 帧、99 轨、20,320 字节压缩 data 可读，空的是 `transformOffsets/floatOffsets`；早期判断要求所有表存在，误把采样器不需要的东西当就绪条件。非 spline 分支未经检查却返回 true，也已改为 `unchecked-non-spline`，不能把“不检查”写成通过。

| 入口 | 历史 AE 实验结果与适用条件 |
| --- | --- |
| `SampleIndividualTransformTracks`，vtable 槽 06 | 按任意轨号索引 transformOffsets；该表在样本为空，HKX16 崩溃。不能套用对象头有效就调用。 |
| `SamplePartialTracks`，槽 04 | 从 0 起连续 N 轨，OAR 预览使用同一路径；HKX25 起采样成立，未要求上述空表。 |

这些槽号和布局是目标版本/对象证据，不能移植到未核对 runtime。Queue/Load 有资源副作用，animation/control 裸指针页可读不是长期所有权；必须连资源持有、binding 与数据一起保证。有限只读 clip 采样成功不能证明行为图工厂、状态机或销毁协议。

<a id="hkx-mapping"></a>
## 数值 A/B、轨道映射与参考的有效性

HKX28（`38e18ae5f0`）在一个 30 秒替换件上得到同相位位置 0.000、最大旋转约 0.15°、37/37 比较骨在 1°内，副本目视与数值同时成立；binding 轨道表仍空，使用 identity。**只对这段动画、当时骨域有效**，不能将其升级为所有 OAR 替换件的权威映射，更不能拿解析 90/99 轨代替 99 根有效骨都验证。

后续 HKX35 另一个变体未过 A/B；HKX39 记录 37/99、最大旋转 109.16°；HKX40 多武器族 calm/compared 约 38%–62%。它们足以证明当时未验证 identity/参考链不能通用，却不能只凭残差唯一断定“真实轨序全错”：后来 poseLocal 误序、单 clip 对最终混合输出、暂停输出冻结都成了混杂因素。当前仍标 `identity-unverified`，未猜一张新映射表。

HKX42 经验匹配尝试在多个帧累积轨道与真值旋转误差。暂停节点几乎冻结，回退初始化还覆盖路径选择，曾输出 solved 后 `max-pos=inf`；这不是有效标定。HKX43 转到正常引擎帧观测，缓存裸 NiNode 跨帧后在 `rotation_angle_degrees → solve_tracks_step` 崩溃；HKX44 修为逐帧解析和值累积，**用户后来决定不再上机该包**，不补写求解成功。

正确的验证需相同资产、相位、初态、坐标、贡献层/IK阶段；比较位置/旋转/缩放各自误差，列明必要骨、缺失与未处理项。单 clip 不应被强行要求等于包含多层、混合、IK或物理的最终姿态。反过来，写后读回一致与节点名命中也不证明真实采样正确。三轴 Havok scale 与单标量 NiTransform 的转换从早期便是未解变量，今天弓/弩仍需以原始样本定位。

<a id="isolation"></a>
## 源隔离、缓存失效与时间边界

HKX36 的 H05 有真实有限证据：7 会话、64 操作、128 普查、两个存档；当时源 local/对象集、116 项 poseLocal、引用位置朝向及受监视 NPC 保持基线，NPC 最多覆盖最近 24 名。SuperSprayBreath 的 0.25u world/bound 周期变化在无操作窗口也存在，记录为引擎粒子观测，不能归入面板写入。该口径不覆盖所有粒子/物理内部状态，AFP 后来又测到未被驱动玩家也有 0.069°姿态微抖，不能把某轮“逐位冻结”变为永恒前提。

用户装备变化会合法替换源对象集。HKX39 64 条 DRIFT 是整会话基线没有再建，窗口 pre/post 仍 CLEAN；HKX40 在实际装备边界重建后消掉重复误报。**只能在合法变化之后重建，用它遮盖无装备操作期间的漂移是错误的。** 原角色图推进、变量写入、world transform 改写始终不在普通复制路线权限里。

旧缓存问题横跨探针、求解器和显示：对象仍活着可能已经退休；地址可读可能已是另一个对象；根相同也可能子节点/登记关系重建。当前按 graph/root/request/session 代次失效，跨帧保留值与受控持有，不能把 `VirtualQuery` 当所有权协议。

渲染回调并非“暂停不执行”；历史文内已更正其每渲染帧带去重执行。菜单生命周期仍需菜单事件的权威边界。自己的时钟会跨关包延续，旧版本重开不重置曾导致续播；何时续接、重置由现行请求规则决定，不复用旧热键约定。

<a id="puppet"></a>
## 第二个真实 Actor：暂停推图有正证据

源角色暂停不产生新 equip pick，与“任何图暂停都不能运行”是两件事。`scene-copy-probe-menu-equip-2026-10-11.md` 记录法术→收鞘匕首：几何增减确认装备生效，但两次活动集仍为 MLh/MRh Idle、同 binding。它只证明这个样本暂停图没响应；不能外推所有操作，也不能把捕获自身 Load 填充 MT_Idle 误认作装备事件。

HKX37（`3b581ad8e1+dirty`）创建**第二个 Actor**，在暂停期用自有 dt 推 `UpdateAnimationGraphManager` 约 3300+ 帧，最后 manual-ticks=3360，clip 时间前进，OAR 替换索引 12221→12224→12220 改变；活 binding/control 与槽04采样机制成立。暂停的 EquipObject 后 25 秒没有一次性 equip，解除暂停后才进入持物 idle，说明当时仅推图没有泵足 Actor 动画事件。数值映射最大旋转约94.6°未过；第5按清理未执行。旧文“ff运行时ID所以零残留风险”不成立。

HKX38 再得到 4890 次暂停推图、图选 Pickaxe idle 且显示跟进的运行观察；深泊 Actor 恢复世界时会下坠，后来 motion 固定处理。HKX39/40 增加暂停内 draw 事件杠杆，HKX40 7 次装备 push 后数秒内出现新武器族，属于**第二 Actor 的事件/图决策链正证据**。它不等于无第二 Actor 的新独立图构造成功，也不等于双 Actor 条件身份、完整姿态、视觉、任务清理全部通过。

HKX41 还定位显示端一次性动作循环：图早已从 Equip 切 Idle，播放器却对超过 duration 的淡出窗口一律 fmod，重放了 Equip。修复按真实 mode 处理的包是待测方案，不能把其预期全写成实测。它说明“图选得对”和“播放器采样得对”需分别检查。

<a id="final-output"></a>
## AFP1–8：暂停最终输出和写入的准确结论

AFP1 读到 generator-output 标准 pose 轨，48 字节元素、116 项；两个输出 buffer 地址/有效性可变，不能硬编码 slot0。AFP1b 最初把空输出归因深泊位，但同一标记还切了 drive off，结论混杂。AFP1c 在 drive on 下复现暂停空 pose，之后 AFP3 才找到更具体机制。

AFP1e（`128ffaa2f4`）直接比较：玩家 poseLocal 按 skeleton 命中109/109，boneNodes仅50/106；傀儡99/109 vs49/106，正式更正 HKX2 次序。未推玩家与推了约500次的傀儡都只有0.069°双态微抖，不能归因手动时钟；不能继续把“有变化”当积累的动画。

AFP3/3b（`44f6ab6103+dirty`）同一次暂停、同一 parked-deep位置，force off→on→off得到 **0→116→0** 输出，109/109旋转配对；`is-active=1`始终成立，推翻“图被停用”的判断。BSAnimationUpdateData `unk2A` 与目标版外层图 `+0x242` force痕迹解释generate skip。**这是历史第二 Actor 更新协议的证据，不是今天写源角色字节的授权，也不是通用ABI。** 当时不必手造hkbContext直接调Generate。

AFP4在正常、无force世界帧获得108/109≈0.99的映射参考；一旦强制生成却仍与旧引擎apply比较，分数降至约0.67，测的是两个阶段偏差。于是把“无force正常帧验证”与“暂停force写入”分开，身份变更才失效映射证明，不能每次切驱动档清掉有效结果。

| 轮次 | 有证据的结果 | 仍不能声称 |
| --- | --- | --- |
| AFP5，`e970256356` | 暂停1帧明确写36项，unreachable32；68项输出，两buffer都可读，观察到interp值。 | 完整覆盖、两buffer插值规则、上屏已通过。旧标题“route closed”已由后续汇总纠正。 |
| AFP6/7 | 分离证明锁存、树名字解析与有限骨表；原始日志还出现unverified/未取足参考。 | 任一提交/单测自动使下一轮通过。 |
| AFP8，`ac27f6e83a` | 无force90/92≈0.98；暂停written92/unreachable6；另有109/109、max-rot≈0.048°的观察比较。 | 109根全被本阶段写入，或者它是唯一写入者。直接draw当时仍未接通。 |
| AFP9c及之后 | 某次实际109写/7不可达/116尝试；另一些装束仍98尝试、18未处理或缺裙骨。 | 单一骨数适用于所有会话，或者palette未命中便证明任意配置都无必要骨。 |

AFP9的取景、蒙皮frameID和黑面板见[渲染总结](history-rendering.md#afp-rendering)。生成输出、写到节点、skin消费和正确合成是四个独立证明层；旧Actor/studio清理成功不能替今天复制图或独立行为实例配对销毁背书。AFP关于SMP/CBPC、FaceGen和morph的测试单未因身体动画成功而通过。

<a id="oar"></a>
## OAR：主动查询成立，复刻原生流程并未等价

10-11 的 `oar-fork-handoff.md` 提议新增公开Query接口，未实施fork。当时公开Animations API只能读已有clip的替换，Conditions/Functions用于注册，不是任意状态求值；“只有fork才可能”只是当时已调查公开接口的边界，后来二进制适配已绕过这项阻塞，不能继续列为当前不可行证明。

`oar-standing-idle-handover.md` 的旧主动查询误把3.1.6 RVA `0x1A5720`当Evaluate；它实际是按character/index查replacements。遍历成功、前缀匹配和with-replacements=0并没有执行那一跳，因此“唯一阻塞是索引”不成立。`oar-preview-query-2026-10-16.md` 依据匹配DLL/PDB与反汇编分别核对3.1.6/3.2.1入口、布局；两份profile不套用地址。精确二进制身份、CodeView、字段与入口验证是当前兼容边界，其他版本不靠版本号猜ABI。

当前query必须从实际project的原版binding找候选，不能拿OAR已改写的替换index当original key；哈希去重可让不同logical path共享实际binding index/name，日志须分别保存。缺名曾导致弓/弩/火把/双持/空手无提案、面板消失，10-20改为真实引擎原名与有限fallback；名字表长度一致本身不是索引语义的唯一证明。

主动预览使用独立clip载体和作用域，保留原生候选优先级、disabled/negation、变体权重与元数据；Random、顺序、PlayOnce由面板自有历史控制，**被接受的播放**才提交消费，pending失败不能耗掉历史。关闭重开/不同Actor的scope和循环reset语义仍由规范限定，不能擅自清源Actor全部状态。

但选片链仍有已知差异：

- 原生行为图先决定哪个节点/哪些层激活，面板按装备家族确定固定original入口；入口已可能不同。
- 预览临时clip统一loop载体，装卸图变量仍为0，站立投影与源Actor实时状态也不同；依赖节点、阶段、剪辑的条件可能分叉。
- 面板先审核整棵条件树，原生AND/OR可短路；即使受支持的OR第一支已true，未执行的另一支不支持仍可能让面板拒绝整候选。这是准入语义差，不是单个条件值错误。
- 原生ActiveClip的更新、中断、循环前瞻、结束相对seek和混合相位，没有被“复制混合秒数+自有clock从0开始”完整保留。
- 行为图全部贡献层/权重、后处理与面板有限pose覆盖不同；同path/index也不保证同相位、同最终姿态。

旧oar-ab在历史傀儡上实测投影臂/原生臂clip集合不等，差值主要是IsWeaponDrawn；证明hook影响了选择，不能证明全部条件/分层/外观正确。10-15三份双手武器crash还分别保留：两份是BipedAnim空指针，经IED/UAPNG与傀儡Equip链；23:02是poll_equipment空对象读。它们约束旧Actor装备路径与调用顺序，**不能仅因栈中出现某插件就将其判作该插件缺陷或当前OAR适配根因**。

<a id="scheduling"></a>
## 调度与选片历史：为什么目录名和主层日志不够

`oar-idle3-fixes`汇集18轮，部分逆序追加且未上机。它先修跳跃/潜行捕获中心、进包时源已收鞘、骨名空格、装备挂点与淡入中断，后来又尝试记录世界序列、按Start/Loop兄弟目录接力、跨开包播放列表。跨族路径、哈希alias、keep_current早退、Start驻留和进包前后drawn时机让这些补偿越来越复杂；后续被现行装备目标+原生DAC相位替代，不恢复旧global playlist或“所有效果无条件true”。

一个关键的历史反例：01:53会话日志报告主身体已到19.80秒Loop，画面却仍像3.33秒Start；报告按文件/track数量与写序定位到mlh手部层是整骨架导出、排在主层之后，把约95根骨覆盖。它解释该轮主层日志与画面冲突，**不是今日斧头Start重选问题的既定原因**。手部层恢复需明确骨域/真实binding，不能凭动画名认定仅含左手。

校准也经历明确回归：重居中每帧重新武装、将捕获world再次覆盖到已写动画local，第二帧缩成白色剪影。改为站立idle的稳定标定、动作随后共享固定placement；现行更明确用原版MT_Idle而非当前替换动作作标准。武器generic轨应落手持WEAPON/SHIELD/QUIVER，不能用WeaponSword等鞘挂点替代。

性能证据同样有边界：旧游玩期全表/逐字符可读性查询造成周期或换动作卡顿，HKX35用户确认改善；近来的WORLD逻辑反查一度每clip约7.3秒，已改实际index/name与可选官方API，不重新扫全表。一次预热扫描、Clone/审计、DAC文件预检、首次GPU资源各是不同成本；这些诊断数字和用户感知不能合成为测量过的FPS提升。Info约1秒flush、trace预算/截断/条件门控，也使“未看到日志”不能直接证明未执行。

<a id="dac-ied"></a>
## DAC、Smooth 与 IED：真实状态、整事件与恢复

10-20–22测试记录保留过虚拟marker/TOML/文件夹方案，当前均被原始DAC二进制通道取代。不能修改DAC原包或合成OAR效果值冒充原生。真实annotation经UI任务dispatch，图/菜单/世界/请求票据作废后不执行；close先封epoch，再同步恢复不可变entry snapshot。兼容刷新、重捕、换装备不重写最初快照。唯一允许的源状态写入是受控惰性自目标marker效果，不是源图、draw flags或装备。

`TESForm::HasVMAD()`只检查VM句柄，不能证明记录定义带脚本；旧66规则全拒的门槛因此误判。只移除它又不能证明无脚本。现在核对每个MGEF的最后贡献插件定义，absent/合法空VMAD才放行；有脚本、贡献源不可见、格式/ID映射不明、压缩记录等均拒绝。全/轻插件/master映射与mod管理器VFS仍需目标游戏确认。一个事件涉及的全部效果和冲突范围须都可恢复，不能只放安全子集造成半事件。原文件19个SPEL/19个MGEF无VMAD的静态结果不替覆盖顺序背书。

Smooth匹配要复用原生大小写不敏感的全tag语义：`Smooth_Loop`可匹配`Smooth_loop`，`Smooth_Loop_off`仍不同，dispatch保留原拼写。native call completed只证明返回；实际marker revision/active状态才说明目标就绪，Start/Loop on/off与combat-clearing需按各自语义观察。早期用off短暂共存否定on、或把等10秒仍Start直接判“施法失败”，都被后续观测细分。恢复matched只比较受控snapshot字段，不代表Papyrus或任意外部副作用回滚。

IED的原生false必须保留。提供方压缩包标签1.0.3而嵌入版本1.0.2.0，三个显式schema按条件1.0.0.0及components核对；同一匹配provider其他当前源条件保留原生回答，不意味着任意第三方条件通行。SDS分支还需provider自身注册/依赖，不能说整个非战斗路线一律需要SDS/TDM。

历史手部范围已经细化：设置与显式node可在核对schema下借读；equipped-placement hint仅手部未变化时可原生读取，**变化那只手的历史hint仍不支持**，未知历史谓词拒绝。10-23补齐卸下old-hand上下文与过渡保留，只修适配边界；provider注册、暂停hint刷新、原生选片和视觉仍待上机，不能绕过条件获得“命中”。

<a id="recent"></a>
## r1/r2与10-23修复：仍在用的未决证据

最新诊断交接与两份raw保留原处：[r1](diagnostics/CharacterPanel-issue-trace-r1-20261022-2231.log)、[r2](diagnostics/CharacterPanel-issue-trace-r2-20261022-2314.log)、[当前步骤第10节](diagnostics/display-issues-2026-10-22.md#10-2026-10-23-装备上下文与火把分层修复待上机)。r1关闭恢复5次，r2为2次；r1盾/卸盾index8940/9503，r2为8938/9496。数字是每次项目的局部索引，按logical path、request、阶段比较，不能跨会话硬对ID。WORLD是**暂停源图快照**，不是正常世界换装后稳定行为，也没有记录完整事件链。

| 问题 | 已知事实与当前边界 |
| --- | --- |
| 火把 | 世界源同时有武器身体idle和`Tor_1HMPose`，旧面板只查整身TOR_Idle，属于结构性缺层。10-23为右一手武器+左火把加入独立左臂层及显式失败回退；编译/离线policy通过，视觉未验证。其他火把组合不由此自动覆盖。 |
| 卸装 | 旧普通全卸仅保留类型而非旧手ID；单卸盾即使传old-hand仍可能被IED历史hint限制。10-23补请求上下文和加载中的过渡意图，不宣称已解决所有包的卸装选择。 |
| 斧头Start→Loop | r2确有Smooth_loop派发、真实loop marker、效果变化/边界重查，仍返回Start/accepted-same-asset。剩余需Loop候选自己的拒绝链，不继续把大小写或“没事件”当唯一原因。 |
| 单手斧闪近 | 原axe探针仅认双手斧，单手归1h错过窗口；10-23扩覆盖。视觉原因仍未知，需采样、混合、placement、相机投影同ID/相位对照，不能用重复Start直接解释瞬时闪近。 |
| 弓/弩 | r2弓左手incoming scale13.462，混合结束Root0.004102、COM17.625725，外层0.35；异常已出现在输入链，不能仅归因旧动作混合。三轴scale日志新增，转换仍取X、空表仍identity-unverified，未做猜测clamp。给定Smooth Bow包没有equip资源/弩project，所以选Gunslicer过渡本身不是选片失败证据。 |
| movement | OAR3.1.6的direction0=None、1=Forward；静止0应true，先前全false投影语义错误已修。r3日志只在命中时发，缺seen=no不足以证明hook漏装；10-23补参数化诊断，尚无对应新会话。 |

`CharacterPanel-oar-probe-20261022.log`是更早00:42–00:54会话，归并其DAC拒绝/restore和WORLD/PANEL观察；它不替最新r1/r2。10-23修复提交`a9a0a20`与后续行为基线提交`719c90b`的Release/离线结果都不是新的游戏运行结果。当前问题不能因删除老日志而被标成已解决。

<a id="graph-validation"></a>
## 独立行为图：当前可行性关卡

无第二Actor、复用行为项目的候选图，目的在于让原状态机自己激活节点，而非继续手写所有入口。`ConstructAnimationGraph`已有声明（holder虚函数slot05及多个override）。2026-10-23的[二进制审计](behavior-graph-construction-audit.md)在AE 1.6.1170上确认了五种实现、holder传参（REFR自身或空）与配对销毁，但项目加载、链接、`hkbCharacter` setup及激活入口仍未定位；不能直接拷内存或调用试错。

OAR会从hkbCharacter反查BShkbAnimationGraph/Actor；图变量、drawn等可能经Actor读取，Random/顺序/PlayOnce状态和事件sink还在图外。图指针不同不等于条件上下文、历史、回调独立。候选阶段变量须用哨兵验证实际读谁；清理不能抹源Actor历史；输出事件先只收集，DAC隔离后再接。

目前只有静态对象归属审计、1.6.1170构造/销毁二进制审计及默认关闭`BehaviorGraphProbe`，每actor/request最多16分组行，role=source-baseline/candidate=none；没有创建、链接、Activate/Update/Generate、图变量写入或事件。**源基线不能证明隔离。G1 NOT PASSED，G2–G6 NOT STARTED/LOCKED。** 本机实际存在AE 1.6.1170安装与MO2实例，但本轮未部署、未启动游戏；保留的`CharacterPanel-engine-text.txt`是历史dump元数据，不是游戏可执行文件或已验证工厂协议。

下一步门槛与台账仅由[独立行为图文档](independent-behavior-graph.md)维护：先构造/所有权/销毁，再原版单边推进，再图原生Equip→Idle→Unequip，再OAR上下文/历史隔离，之后完整贡献层姿态，最后重复生命周期。首批只两场景，前关未过后关不开；历史第二Actor正证据既不能忽略，也不能替新G1过关。

<a id="future"></a>
## 未执行方向与验收约束

独立SMP/CBPC实例、FaceGen表情、完整身体变形、双人物交易/混合后端与广泛兼容仍是各自关卡；原测试单存在不等于当前提供接口。FSMP/CBPC注册公开接口不足只说明已调查路径受限，不能宣布“唯一可能是自制弹簧”或“CBPC闭源”。真实求解、碰撞、任务同步和逆序销毁需要单独证据，不能解除世界暂停或推源角色来完成预览。

最终pick一致性应比较正常世界链和预览链的首次分叉：同初态与事件下的行为入口/变量 → 候选逐条件 → 替换/变体/phase → load/accept → 有效层/权重/骨骼 → 输出事件/marker重查。只对齐主path而忽略层、时序和被采样对象，无法解释或保证一致。产品要求与支持范围继续在[PRD](player-panel-prd.md)维护，不把旧愿景写成已实现功能。

<a id="maintenance"></a>
## 保持结论可验证

后续新证据优先修正本总结对应段，并同步[历史入口](history.md)、[来源表](history-sources.md)与拥有当前规范的文档；不重新堆积逐轮完整日志在长期docs索引。保留运行构建、二进制/依赖身份、实际操作、请求/阶段、原件和未测范围，不复活过时命令。当前[构建](build.md)与[维护](code-maintenance.md)拥有代码/测试入口，本总结不引入依赖、不新增行为测试，也不把离线通过当原生引擎验证。每个清理来源在来源表有唯一行，Git恢复身份与清理前原始字节哈希分开记录。
