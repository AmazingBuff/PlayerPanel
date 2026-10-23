# 渲染、展示实例与外观实验总结

更新：2026-10-23。本文整理 2026-09-27 至 10-16 的历史研究；原始文件在本轮清理前已提交到 `719c90bfe115a67ac64b80271dc8afced780290f`，逐文件归属、证据限制和恢复方法见[来源索引](history-sources.md)。历史键位、旧源码路径、部署目录和测试包不再作为操作说明。

当前实现与约束以 [SceneGraph](scene-graph.md)、[维护说明](code-maintenance.md)、[构建](build.md)为准；动作与最终姿态研究见[动画总结](history-animation.md)。本次只整理文档，不重新执行历史实验。

<a id="evidence"></a>
## 证据口径与阶段边界

本文区分四类材料：**历史运行观察**指报告明确对应的用户游戏会话、日志或抓帧；**静态证据**指源码、声明、二进制或本地检查；**未执行方案**只有设计与判据；**已被修正的判断**保留其失败原因与后来结论。报告标题中的“已修复”“收官”不自动代表其全部复测项完成。

早期 M0 是物品 pass 重放和面板合成；之后 P-instance/Studio 使用第二个真实 Actor 装配网格，曾验证迁图、杀壳和特定 teardown；10-08 的 S0 改为 native Clone 已有场景图；AFP 又短暂研究同 Actor 最终姿态直绘。它们的对象、宿主和销毁路径不同，不能互相借用通过结论。今天产品只有复制场景图展示，旧 Actor 热键和生命周期已经退役。

历史原件还包含不完整日志与未入库图片。来源表将其标为“启动片段”或“报告观察”，没有原件的截图不补造链接；报告写明的用户观察可以保留，但不能升级成此次重新检查过的像素证据。

<a id="m0"></a>
## M0：先证明渲染状态和合成链

`stage0-spike.md` 记录了 AE 1.6.1170 / SKSE 2.2.6 下的早期失败：从 SKSE 共享 branch pool 申请 64 KiB 会触发分配断言，改用私有 trampoline；这不是 CommonLib 9.1.0 与 SKSE 存在结构性版本冲突的证据。该 spike 的大部分游戏验证清单当时尚未执行。

`m0-capture-report-2026-09-28.md` 的用户采集覆盖 Community Shaders 开启、暂停背包、2560×1440。十五个目标例程解析到游戏映像，Get/SetCurrentAccumulator 分别在该版模块偏移 `0x1480B20/0x1480B30`；这只证明槽位与入口可读，不证明菜单实际通过该全局槽绘制。无 CS 的对照没有做。Culler 扩展字段与运行时观察冲突，不能拿编译期 sizeof/static_assert 代替目标版本布局证据。

`m0-proto.md` 的 runs 1–8 最终否定了当时的 accumulator-swap 实现：菜单路径没有按设想消费全局 current accumulator，声明的 culler 布局/入队语义也不成立；主、次 accumulator 互换和直接喂几何都没有获得可用图像。早期“全局指针能换，所以独立 cull/render 可直接做”的推断被后续实测覆盖。它不等于所有独立渲染器方案均不可能。

随后走菜单帧内 pass 观察/重放，得到可复用的因果链：

- 几何祖先链与 UI3D `menuObjects` 可以识别当时的菜单 pass；必须保留被其他插件改写过的调用链，不能覆盖 Community Shaders 的前置钩子。
- 原调用之前重放，会被引擎仍待应用的 OM、SRV、常量缓冲影子状态反复覆盖。run 12 记录原调用前后的 PS 纹理槽集合不同；逐类状态补丁只把故障移到下一类状态。
- 改为原调用先完成，再重放同一 pass，run 14 的六次捕获得到材质和颜色正确的完整铁盾。这是该物品样本的闭环，不是整个人物或任意 shader 兼容证明。
- 没有独立深度目标时，盾后把手错误覆盖盾面；typed DSV 格式与显式深度状态修复后，run 16 的遮挡观察成立。资源的 typeless 格式不能直接当默认 DSV 格式使用。
- 屏幕上的实际合成目标是会轮换的 RTV 实例，不能缓存一个看起来同格式的旧目标。格式、绑定、调用时点和最后消费位置必须同时对应。

runs 20–25 中，“绘制调用发生”多次未形成可见矩形；某轮新 draw 根本未被调用，不能计作对其方案的有效否定。run 24 的自有 CullNone 又推翻了上一轮背面剔除归因。run 25 改用 over blend，run 26 用户截图首次出现持久摄影棚矩形。引擎当时日志仍报 ONE/ZERO，不足以完整解释 UI layer alpha 语义；应保留“经验修复有效、底层 layer 语义未定”，不能把假设写成已证明的通用规则。

`m0-acceptance.md` 只验收当时菜单内工程面：7 项中 6 完全通过、读档/新游戏消息触发路径部分通过。20+ 次开关和 release 记录属于当时 RT/面板生命周期，不能证明后来 native Clone 图安全析构。世界画面与面板共存、正式 CPU/GPU/显存测量、布局与 modded item card 遮挡仍被推迟。旧帧计数换算不构成受控 FPS 基准。

<a id="live-actor"></a>
## P-instance：装配、隐藏和显示不是一个所有权问题

`stage2-p-instance-plan.md`（runs 27–93）与 `m0-handoff.md` 保存了多次路线更换。run 27 的 `CreateDeepCopy` 没得到有效根，只否定该次入口/参数；后来 `NiObject::Clone` 的成功不能被这次失败预先否定。

第二 Actor 装配后摘图/挂菜单根的早期版本，也遇到“物品菜单从不替外来人物生成 pass”“图被摘出世界后没有观察回调”“缓存 pass 被池化回收”等问题。渲染循环最终直接遍历当前几何，调用引擎原生 `GetRenderPasses` 与 `SetupAndDrawPass`；pass 是当次生成、当次使用的借用对象，不能跨下一次生成或会话保留。

曾把“所有 `rendererData==nullptr`”当作不能画的门，导致初始化被挡在初始化所需的绘制之前。Studio run 95 证实引擎提交时可惰性创建设备缓冲，推翻了“必须先让世界渲染一次”的通用前提。可读头、非零矩阵数、有效 pass、GPU 消费与正确像素仍是不同的证据层。

真实 Actor 由引擎控制引用位置、3D 根、AI、碰撞/角色控制器和装配；只隐藏 pixels 或只 `SetPosition` 不会消除交互。在早期 run 51，Disable 会销毁其 3D，白名单裸指针随后失效；run 59 的深泊位仅在该配方下得到世界不可感知观察，不能成为后来所有隐藏方法的保证。出生深度还影响装配 LOD：后续 run 104/AFP10 记录深处直接出生得到缺件低模，故“出生后再移动”与“在远处出生”不等价。

取景必须核对实际相机和坐标约定。世界坐标、Actor 引用、根 local、派生 world、UI3D 相机视轴、近远面和面板裁剪并非同一空间。早期几轮只翻矩阵行列或猜比例，直到 VS 输出和实测 frustum 才定位。按物品相机位置摆人物会令取景随高亮旋转/缩放，固定摄影棚锚点才解除依赖；CB、viewport、矩形纵横比混用又造成半幅、拉长与离屏。旧常量只属于原构建，现行标定入口见 [SceneGraph](scene-graph.md)。

<a id="lighting"></a>
## 灯光：账本、窗口和宿主生命周期

这批实验留下的灯光结论比亮度参数更重要：

1. 复制物品 pass 的灯光裸指针会跨物品切换失效，曾在 Community Shaders 常量上传中崩溃。引用当前有效来源、缩短借用窗口，不能以一次画对为由缓存引擎壳。
2. 自建 NiLight 不等于 shader 可用灯。该路径需要 ShadowSceneNode 登记及其 BSLight wrapper；AddLight 先放 `lightQueueAdd`，之后才进入 `activeLights`，只查一边会误判创建失败。
3. 引擎更新会重写外来 wrapper 的淡入/亮度字段；fetch 时补一次不等于 draw 时仍有效。两盏灯若都写同一 rig 父节点，后者覆盖前者。坐标需写到各自节点，窗口内临时处理后对称恢复。
4. 当时 CS strict light 列表从 `sceneLights[1]` 读取点光，槽 0 具有环境光语义。错误地把主光放槽 0，以及物品 replay 上下文污染，都表现为“没有光”。v6.56 只修槽位仍不能解决高亮差异；v6.57 把人物 draw/composite 收归 end_frame 单一路径，runs 91/92 才验证高亮/无高亮一致。
5. 灯注册进世界账本后，即使挂菜单节点也可能照亮背包背景世界。v6.58 默认把 rig 泊到 Z+100000，仅在 draw 窗口拉回，用户确认漏光消失；但把 anchor 同时加到 rig 和子灯 local，造成双重位移与变暗。v6.59 改成 anchor 相对偏移，run 93 再确认面板光照、无泄漏及两种高亮状态一致。

这些是指定 AE/CS 和历史显示路线下的观察，不证明全部渲染框架或模组兼容，也不证明当前所有光照与世界场景完全一致。

<a id="studio-home"></a>
## StudioHome：历史成功与不能外推的清理结论

`stage2b-studio-home-plan.md` 的 v6.60–v6.68 是一条具体的历史成功路线：图迁入 `menuObjects[0]` 下私有 `CP_StudioHome`，先 detach 并用 NiPointer 持有，再删 Actor 壳。run 98 在杀壳后 20 秒内，13 轮开面板、338 帧提交保持正常；run 99 对读档和主菜单两条路径观察到 `whitelist disarmed → actor killed → home released`，6 轮面板和 TGA 正常。它证明该装配图、引用和宿主顺序下的机制，不证明今日 native Clone 图、行为运行实例或任何销毁时机都安全。

同系列还有后续反例：run 100 跨角色加载后，缓存灯壳被世界账本重建释放，非空裸指针继续被补写而 UAF。v6.66 清壳指针/重新取账本壳消除了该崩溃，但灯光仍丢失；v6.67 比较宿主、场景节点、NiLight 指针未触发重建。**指针相同不证明成员关系和收集路径仍然存在。** v6.68 用连续 fetch 不完整为信号重建 rig，run 103 用户确认切角色后恢复，普通单存档流程无额外 reset。约 0.5 秒是旧窗口计数下的观察，不作为今后的性能承诺。

旧文把动态 `ff` FormID、SetDelete 或“不崩”写成零残留保证，证据不足。动态表、任务、引擎登记和资源释放要分别证明；历史 teardown 成功也不替代资源分配计数收敛。当前 [SceneGraph 的停放约束](scene-graph.md#invariants)仍保留。

<a id="scene-copy"></a>
## S0：native Clone 静态显示成立，安全回收未通过

`scene-graph-copy-results-2026-10-08.md` 对应 AE 1.6.1170 + SKSE 2.2.6 + CS，在 CBBE、UBE 两个存档验证：复制根独立；源根/父/对象集保持；多类可写引用审计无别名；主体、脸、头发、装备、妆容/纹身显示；副本旋转不带动源；换装后手动重捕可刷新外观。它验证有限静态样本，不是任意骨架、动态物理或自动换装的全面证明。副本头发/衣物保持捕获形状属于当时预期。

复制只读静态资源可共享，节点、skin instance、shader property、动态缓冲和 pass 等可写对象必须审计；native Clone 返回不等于未知回调、GPU 或引擎全局登记已隔离。控制器/碰撞/userData 清理只作用副本，不能清源。特效/粒子/辅助体混入人物树时，会得到很多 pass 却没有有效身体；UBE 一轮 123 几何剪至 51、106 pass 降至 47 后显示恢复，不应按名字无限扩排除名单。

实际回收有三个已测失败点：捕获内就地析构、延迟四帧退休队列、读档/主菜单边界析构，均出现 `~CharacterClone → BSFadeNode → tbbmalloc` 相似崩溃；根因未定，不能宣布“任何时机都不能销毁”，也不能把栈上某骨名当根因。之后采用停放，14 次捕获无崩溃只是避免触发析构。容器 cap 12 只删记账，原生引用仍保活，实际分配持续积累；C++ 析构体不主动 reset 也不能阻止成员自动析构。

因此 S1 的安全释放仍未闭环，反复捕获/压力测试不应拿来掩盖资源积累。当前限制和所有权规则由 [SceneGraph](scene-graph.md)与[维护说明](code-maintenance.md)维护，历史表格的 Q09/Q10 不再笼统写“通过”。

<a id="afp-rendering"></a>
## AFP9：生成、取景、蒙皮与合成逐层拆开

AFP1–8 的最终姿态生成/写入是[动画证据](history-animation.md#final-output)。AFP8 当时仍画复制图，不能把节点写入成功称为同 Actor 直绘已闭环。AFP9 才接入真实 Actor 根，且连续暴露彼此不同的故障：

| 现象 | 实测/定位与后续边界 |
| --- | --- |
| 覆盖审计先崩溃 | `afp9-verdict` 与匹配 crash 指向直接解引用 skin palette 项读名字。后改指针集合与本树解析节点成员比对；palette 项非空不能当可信 NiAVObject。 |
| 摄影棚有图、面板全黑 | 同纹理仍绑 RTV 时先绑 PS SRV，D3D11 将采样槽静默解绑；先切输出目标再绑源，并分开 draw/composite 状态捕获。后续 `COMPOSITE match=1` 与用户画面确认复制路线恢复。 |
| pass 在画、人物远离视锥 | 写根 local 再向下更新没有移动该有父根自身；需要核对实际 world 与投影。临时 world delta 修正当帧取景，不能由此外推长期 Actor 泊位只写 world 即可。 |
| 引用已泊，3D/交互仍在玩家处 | Actor 引用、3D root、角色控制器分离；world 写入稍后会被引擎重推，补 local 也仍被后续引擎 Actor 更新覆盖。需读回真实根和控制器状态，不能只信调用日志。 |
| `center-d=0`，人物仍小且暗 | 几何 worldBound 是旧缓存，落在旧 spawn/泊位空间，错误把图推深约 2820 单位。改以本帧骨骼 world 测量，缓存 bound 仅作对照。零“自比较误差”不能证明参照物正确。 |
| 部分帧完整、部分只剩斧头 | 刚体武器跟新 world，蒙皮矩阵却已按当帧 frameID 在泊位计算。AFP9i 在提交前使 skin frameID 失效，让引擎重算，**没有直接写 boneMatrices**；后续用户确认反复开关面板稳定。 |

`afp9-controller-park-verdict` 又定位 SetPosition 的角色控制器更新参数，AFP9j 加 warp；**该文末尾的“无交互提示”复测仍待执行**，不能因标题“已修复”直接记上机通过。AFP9i 的归档本身只有启动/灯登记片段，稳定画面结论来自报告所载用户确认。先生成、再应用、再被 skin/draw 消费、最后合成，每层都要有自己的证据。

覆盖也随样本变动：AFP9c 原始日志曾对 `attempted=116` 明确报告 `written=109 unreachable=7 unprocessed=0`，不可将 AFP8 的 92 写入固定为全系列上限；另一些样本仍是 98 尝试/18 未处理或缺裙骨。早期“61 个 unreachable”是审计解析顺序错，后改为与写入一致先名字后表。18 个未处理 helper 在当次 palette 中不被读取，不是任意配置都可忽略的证明。

<a id="ube"></a>
## UBE 透明与外观：实测范围必须细分

`ube-transparency-test-2026-10-15.md` 对应 `6e5ac99` 工作区交付与干净 `f63f41c` 重建：眉毛/眼周 mesh 轮廓黑块在 Actor、复制两路共有绘制缝出现。修复读真实 NiAlphaProperty，启用原生 alpha blending 标志（历史 flags `0x204`），不透明/alpha-test 先画，混合表面按摄影棚深度稳定排序；No Sorter 保留分段边界，不让其他表面越过。透明阶段基准不写深度，实际材质仍可覆盖。

引擎影子缓存与 D3D 状态都必须恢复；VS/PS 14 个 CB 槽及 D3D11.1 range 需要一起保存，alpha reference 的高槽 `0xB` 不能遗漏。先排几何再当次生成/提交 pass，不能跨生成保存 pooled pass。WARP 与本地排序检查验证的是状态恢复与算法，不能单独证明游戏 shader 的 UBE 结果。

10-15 用户明确确认黑块消失；报告记载 `blended=12 tested=22`、合成正常的历史日志，但该外部 log-archive 文件未随本仓保留。转动、换头部装备再重开、退回游戏三项当轮没有逐项确认。10-16 用户又在另一台机器表示“面板人物渲染完全没问题”，**无新增日志/抓帧**，保留为整体视觉反馈，不补造测试矩阵。

AFP10 的体型/服装属于当时新 Actor 路线：服装是 biped 槽，武器手部不是同一装配表；OBody 给新 Actor 随机预设，顶点 morph 不会靠骨骼 scale 同步。曾接 skee BodyMorph v5 比较/复制傀儡 morph、清其 OBody key、重应用；没有写源角色。后来默认套装、容器记录、prune cadence、弹药与漏掉的装甲槽继续暴露问题，AFP10g 明确是代码轮待上机。不要把这些修复清单写成全部外观验收通过，也不要从 Actor 体型不同推断静态 Clone 必然丢 morph。

<a id="future"></a>
## 未执行的产品方向与不能丢的验收条件

`display-backend-routing-2026-10-13.md` 提过交易双方、玩家/随从/商人、尸体/基础容器的分源后端；`afp-next-validation-and-paused-gaps`、`s3-independent-physics-validation` 提过独立 SMP/CBPC、FaceGen 表情、身体变形。它们是历史设计/测试协议，未形成本文可证明的完整实现。当前唯一后端仍以 [SceneGraph](scene-graph.md)为准，产品要求留在 [PRD](player-panel-prd.md)，社区入口保留在[参考调查](community-reference-supplement.md)。

这些方案中仍有持久价值的要求：双视图必须有独立状态/输出，不能第二次 batch 清掉第一次；尸体卸装等实际装配完成后重捕，保留死亡姿态而非站立模板；连续 A→B→C 只接受最新代次；身份切换令旧任务失效。物理要证明定向实例、真实求解、有效碰撞、基础输入冻结后继续响应及 pass 前未被动画抹除；表情要分别验证输入、权重、形变和实际绘制。恢复全局模拟、程序化摆动或“画面动了”不替代这些门槛。

<a id="remaining"></a>
## 当前仍开放的事项

安全销毁复制图与资源收敛、未知骨架/装备和渲染框架兼容、实际独立物理/FaceGen、正式性能测量均不能从旧“收官”外推。当前火把/弓弩/单手斧问题由[在用诊断交接](diagnostics/display-issues-2026-10-22.md#10-2026-10-23-装备上下文与火把分层修复待上机)维护；独立行为图由[六关验证方案](independent-behavior-graph.md)维护，G1 未通过。

<a id="maintenance"></a>
## 后续维护与验证方法

保留一条与实际构建、阶段、Actor/root/request 对齐的证据链，先定位首次分叉，再修改对应机制。字节可读、地址相等、表长相等、成功返回、写后读回、无崩溃与视觉通过各有范围，不能互相代替。源装备变化是合法操作，隔离比较要按操作窗口重建基线。图像和节点证据应来自同一有效对象，清理必须证明任务和引用结束。

当前代码入口、控制、依赖和测试命令链接到[维护](code-maintenance.md)与[构建](build.md)，本文没有新增运行依赖或测试入口。变更历史判断时同步[动画总结](history-animation.md)、[索引](history.md)与[来源表](history-sources.md)；新实验只记录已发生结果，未测试项继续明确标注。所有已清理原件均可从固定 Git 基线只读恢复，不需要保留重复日志包。
