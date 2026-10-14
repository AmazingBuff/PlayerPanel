# 当前代码维护说明

更新日期：2026-10-13（AFP9）。本文是当前构建、模块和开发控制的入口；逐轮实验报告仍保留为历史证据。

## 范围

本轮清理删除已退役的调查工具、骨骼／缓存探针、经验轨道求解器及验证用测试矩阵，保留现有预览行为与 AFP 最终姿态诊断。AFP9 新增两件：应用阶段的覆盖对账审计（点名 unreachable/unprocessed 并按 skin palette 分类，不扩写），以及 R01 同 Actor 直绘实验（F6 开关，傀儡自身 3D 进摄影棚，取景恢复带对账；物理/表情仍不在范围内）。没有实现同一 Actor 直接绘制以外的身体 morph、FaceGen 表情、SMP／CBPC、双人物或安全回收。

## 当前模块与数据流

| 模块 | 职责与入口 |
| --- | --- |
| [main.cpp](../src/main.cpp) | SKSE 初始化、消息、渲染安装和存档会话边界；不再转储引擎映像。 |
| [panel.cpp](../src/panel/panel.cpp) | 菜单状态、预览请求和打开通知。 |
| [renderer.cpp](../src/render/renderer.cpp) | 渲染回调、D3D 状态保护、摄影棚目标及合成；按构建选项选择当前后端。 |
| [character_manager.cpp](../src/character/character_manager.cpp) / [character_clone.cpp](../src/character/character_clone.cpp) | 默认路线通过 Actor 装配装备／3D，再摘出图绘制；`pose()` 负责摄影棚取景。该路线不等于尚待验证的“保留 Actor 与 3D 绑定后直绘”。 |
| [scene_graph_copy.cpp](../src/character/scene_graph_copy.cpp) | F7 捕获、来源／副本审计、隔离观测、旧图停放；`animate_at_base_pose()` 在取景之后驱动显示副本。 |
| [animation_source.cpp](../src/character/animation_source.cpp) | 从 typed `activeNodes` 读取运行 clip；`pick_running_clip()` 选最强 control，`resolve_clip_playback()` 解析现有 binding，`play_animation_clip()` 采样并写节点。捕获时也复用这条路径，不再扫描模板、registry 或任意对象窗口。 |
| [puppet_driver.cpp](../src/character/puppet_driver.cpp) | 存档会话内的傀儡装配、泊位、暂停图推进和装备同步；`drive()` 为显示副本供给 clip 姿态。AFP10：装配窗口改为"普查就绪即泊位"且窗口内 fade 归零（不可感知）；穿着整套 worn body items；`apply_body_shape()` 把玩家体型同步到傀儡。 |
| [equipment_mirror.h/.cpp](../src/character/equipment_mirror.h) | AFP10 共用件：biped 身体槽位的 worn 集合读取、整套穿上、以及无状态对账（多则补、少则卸）。复制图的 `mirror_worn_equipment` 也走这一份，槽位表只有一处。手部武器不在此列（驱动器自己的手部轮询 + draw/sheathe）。 |
| [body_shape.h/.cpp](../src/character/body_shape.h) | AFP10 体型同步：本地镜像 skee64 的 `IBodyMorphInterface`（SKSE 接口交换 `BodyMorph`），先比较后写入，写前清掉 OBody 派给傀儡的随机预设；版本低于镜像布局时拒绝调用。 |
| [idle_driver.cpp](../src/character/idle_driver.cpp) / [snapshot_transform.h](../src/character/snapshot_transform.h) | 现有程序 idle fallback、取景变换和子树更新；不再包含测试用蒙皮缓存写入或距离测量。 |
| [final_pose_probe.cpp](../src/character/final_pose_probe.cpp) / [final_pose_apply.cpp](../src/character/final_pose_apply.cpp) | 最终输出读取、索引校准、独立真值比较、暂停写入和强制生成诊断；当前写入傀儡，而画面默认仍由复制图提供。AFP9：apply 同帧对账（output/attempted/written/unreachable/unprocessed），缺口变化时点名并按 palette 分类（必要骨不跳过）。 |
| [figure_audit.h/.cpp](../src/character/figure_audit.h) | AFP9 共享审计件：skin palette 读取（只收集指针、绝不解引用槽位；引擎从未渲染的图记 unbound-palette）、碰撞辅助体过滤的蒙皮包围盒、取景 placement 解算、槽位必要性分类（palette 成员 → Weapon* 附着 → helper）。AFP10 另收编装配普查 `FigureAssembly`／`figure_measure_assembly()`（几何/skin/已分配槽位/绑定数与稳定签名），复制图的落定门与傀儡的泊位门共用它。 |
| [puppet_draw.h/.cpp](../src/character/puppet_draw.h) | AFP9 R01 直绘：F6 开关；傀儡 `Get3D(false)` 根 + 共用 figure_passes 缝；临时取景（保存/恢复根 local + world/worldBound 对账）；固定骨集 A4/A5 采样；不可用时回退复制图并告警。 |
| [isolation_watch.cpp](../src/character/isolation_watch.cpp) | 来源与 NPC 的观测、装备变更重新设基线及会话判读；不是全骨骼／全部物理隔离已通过的证明。 |
| [figure_passes.cpp](../src/render/studio/figure_passes.cpp) | 共用的引擎 pass 生成、灯光临时替换、提交与状态恢复；移除无人使用的逐 pass／batch 探针 callbacks。 |
| [snapshot_transform_test.cpp](../tests/snapshot_transform_test.cpp) | 两个聚焦回归：取景保持中心与子树相对尺度；最终姿态正确索引与错位索引的判别。 |

场景复制配置的每帧顺序：请求／会话处理 → AFP A0 → 傀儡图推进 → AFP 校准或应用 → AFP mark → 副本 `pose()` → 傀儡 clip／捕获 clip／程序 idle → 引擎蒙皮与 draw → 合成。

AFP 写入与复制图显示是两条不同的路径；不得把 AFP 应用后的节点自比较标记为直绘通过。

## 构建、控制与打包

现有工具链仍为 Windows x64、VS2022 / MSVC 14.44.35207、C++23 和仓库内 CommonLibSSE；依赖由现有 vcpkg manifest 提供。本轮不升级或安装依赖。`ENABLE_SKYRIM_SE/AE/VR` 的选项与支持范围保持原样；当前 DLL 版本声明以 AE 1.6.1170 为准。

默认路线：

```powershell
cmake --preset Release -DCHARACTER_PANEL_SCENE_COPY_EXPERIMENT=OFF
cmake --build build --config Release --target CharacterPanel
```

场景复制与 AFP 开发配置：

```powershell
cmake --preset Release -B build-scopy -DCHARACTER_PANEL_SCENE_COPY_EXPERIMENT=ON -DCHARACTER_PANEL_PROBE_REVISION=cleanup
cmake --build build-scopy --config Release --target CharacterPanel CharacterPanelCopyMathTest
ctest --test-dir build-scopy -C Release -R '^snapshot_transform$' --output-on-failure
cmake --build build-scopy --config Release --target CharacterPanelSceneCopyPackage
```

在本地依赖已安装时，可以传 `-DVCPKG_MANIFEST_INSTALL=OFF -DVCPKG_INSTALLED_DIR=<现有 installed 目录>`，避免配置触发 registry 更新。CommonLib 还使用 FetchContent 获取 hde64；离线验证需另传 `-DFETCHCONTENT_SOURCE_DIR_HDE64=<现有 hde64-src 目录> -DFETCHCONTENT_FULLY_DISCONNECTED=ON`。新建验证树也应保留 Release 配置与已安装的 Release detours 库路径，避免误选 debug 库。禁止将离线配置解释为缺失依赖也已验证通过。

| 控制 | 当前行为 |
| --- | --- |
| F7 | 暂停背包中捕获并审计玩家第三人称 3D；替换前一副本，draw 初始关闭。 |
| F8 | 切换当前已接受副本的显示。 |
| F2 | 一次 AFP mark：输出、校准、来源对照、相机与目标网格状态；A2 行含 AFP9 覆盖对账与审计点名。 |
| F4 | AFP `off → tick → tick+apply → tick+apply+force → step → off`。单步为一次 1/60 秒。 |
| F6 | AFP9 R01 直绘开关（默认关）：开 = 面板画傀儡自身 3D；关 = 复制图。不可用时回退复制图并告警。 |

F3 不再由本项目绑定；旧旋转、释放、idle toggle、S2/PuppetProbe stage 没有控制入口。生命周期边界仍由重捕获／读档／主菜单处理。F4 的冻结／推进针对傀儡图与最终姿态应用，不等于冻结独立时钟运行的显示副本。

`CHARACTER_PANEL_SCENE_COPY_EXPERIMENT` 默认为 OFF。删除 `CHARACTER_PANEL_BUILD_PROBE`、`CHARACTER_PANEL_S2_PROBE`、M0 probe/proto targets；旧 cache 可能仍列出历史变量，但当前构建已不消费它们。`CHARACTER_PANEL_PROBE_REVISION` 保留为兼容的构建身份／包 manifest 标记，与已删除探针无关。

场景复制包仍包含 DLL、PDB、身份与 SHA-256 manifest、当前包 README，输出在 `dist`；打包不部署。最近一轮 AFP10 的包与部署见其测试单与 Changelog。

## 保留诊断及理由

- 图隔离审计、来源漂移观测、内存／vtable／数据范围检查：对象来自游戏及第三方插件，指针与 ABI 生命周期仍是真实边界。
- AFP 两个输出槽位、A0/A1 和 poseLocal／节点对照、索引次序校准：双缓冲选择与最终姿态来源尚未完成验证。
- 必需骨／palette、local/world/scale、覆盖计数：标定 98 项而强制生成 116 项的问题尚未关闭。不能删掉这些证据或调整通过阈值掩盖缺口。
- 相机、几何、蒙皮与目标回读：同一 Actor 的实际 draw 尚待接入，仍需要区分应用到正确树、取景和渲染目标问题。
- force-generation、freeze 和 step：暂停动画链路的必要操作控制；不修改原有算法与阈值。

已移除 AFP 未命名字节的 raw hex dump；保留 typed 状态、输出数量和插值信息。S2 swing／T0–T3 蒙皮缓存探针、capture-time perturb/replay、catalogue/loader/registry/vtable 扫描、idle 池统计、HKX42–44 求解与延迟 A/B、程序 idle 位移日志，以及无调用者的容器／尸体／库存／按键／hash 模板 helpers 均已退役。

## 不变量、生命周期和未关闭事项

1. 复制图仍先审计，再清除其 controller、碰撞和 userData；不改写来源图，不为动画写入 `NiSkinInstance::boneMatrices`。
2. 先恢复副本取景，再写入动画，禁止恢复捕获姿态覆盖本帧动作。引擎继续负责蒙皮提交。
3. 傀儡保持 Actor／3D 关联用于行为决策；在暂停期间只手动推进它的图。默认 Actor 装配后摘图路线仍单独保留。
4. AFP 每帧重新查当前树，保留已验证映射和必要的 bone-table fallback；不跨帧缓存目标节点。当前映射覆盖和双缓冲策略没有在本轮修复。
5. typed clip resolver 保留现有 table 与 `identity-unverified` fallback。空表轨道映射、非 identity 表解释、clip 混合与任意 OAR 动作的正确性仍需游戏证明；删除经验求解器不等于这些问题已解决。
6. 旧图仍通过 `snapshot.release()` 停放，原有释放崩溃没有修复。12 项仅限制 bookkeeping；更早指针被移除时内存仍泄漏。不要进行无限重捕获压力测试，也不要把无崩溃称为回收通过。
7. 物理、脸部表情、身体 morph、尸体卸装刷新及双人物后端路由仍是后续工作，不能通过清理顺带实现。
8. 傀儡 clip 绑定目前只在 generator 切换时重建。F7 换成新 root 而 generator 不变时，仍可能写入旧停放图；重复捕获必须上机复核，后续需要按 root 与 generation 失效绑定。本轮没有修复该既有缓存边界，也没有证明所有重捕获后的显示持续性。
9. AFP9 红线：覆盖审计只点名分类、不扩写 98 槽之外的姿态（`Weapon*` 槽盲写会把武器写出正确位置）；R01 直绘不改写根 local 以外的树数据、不移动 Actor 引用、不动相机、不解除暂停，取景失败与关闭出口同样恢复。
10. **`NiSkinInstance::bones` 的槽位只能比较指针**：不保证是可读节点（AFP9 首测崩溃：`figure_audit.cpp` 读槽位名字，`cmp [rcx], r12b` @ 0x1010，见 [AFP9 verdict 1](afp9-verdict-2026-10-13.md)）。遍历 palettes 的代码不得读名字、不得解引用、不得调用虚函数。
11. **同一纹理不得同时作为 OM 目标与 PS 采样源**：D3D11 不报错，只在绑 SRV 时静默置空该槽位，表现为"摄影棚图正确、面板全黑"（AFP9b，见 [black-panel verdict](afp9-black-panel-verdict-2026-10-13.md)）。合成必须先换 OM 再绑 SRV；绘制与合成各自使用独立的状态捕获作用域；合成侧保留 `SCOPY COMPOSITE src/bound/match` 回读证据行。
12. **带父级的节点写 `local` + `UpdateDownwardPass` 不移动它自己**（只按对象自身 world 重算子级）：活 Actor 树的整体取景必须按世界空间 delta 向下写 `world`/`previousWorld`（`figure_world_delta` + `apply_world_delta_downward`），并打印 `center-d` 断言；游离图（无父级，local 即 world）是例外，这是复制图路线与同 Actor 路线在取景上不可互换的原因（AFP9c，见 [F6 framing verdict](afp9-f6-framing-verdict-2026-10-13.md)）。
13. **泊位/隐藏必须同时管引用与 3D**：`SetPosition` 只挪引用，该 actor 的 3D 不会从新引用重新推导（AFP9c 实测：引用泊位 20 秒未暂停帧，3D 根世界仍在玩家处；fade 恢复 1.0 即成可见克隆体）。`park()` 现在按目标绝对位置与当前世界的差做世界空间下移（幂等），并打印 `ref-t / world-t-before / world-t-after / moved-z` 对照行（见 [park verdict](afp9-park-verdict-2026-10-13.md)）。
14. **活 Actor 的位置要写 `local`，不是 `world`**：引擎每帧按根的 `local` 重推该子树的 `world`，世界空间写入只当帧有效（AFP9e 实测：泊位 world 写生效后 1.3 秒被推回 spawn）。**且取景必须在傀儡图更新之前就位**（`PuppetDraw::begin_frame` → 图更新 → 姿态应用 → 绘制 → `end_frame`）：蒙皮状态在更新期计算，更新之后的变换只对刚体挂件生效（AFP9e：斧头跟随、蒙皮身体不跟）。见 [transform-layer verdict](afp9-transform-layer-verdict-2026-10-13.md)。
15. **诊断"变换/蒙皮谁生效"优先取图像证据**：摄影棚目标导出（`dump_studio_target` → `.../SKSE/CharacterPanel/r01-panel-NNN.tga`，首帧与每 300 帧、上限 20 张）比外部帧捕获转述快一个数量级；配套 `R01 census`（geoms/skinned/skin-frameID/num-matrices/首骨世界坐标）与 `park-3d ref-now` 回读。泊位需**每帧**重写（引擎每帧重推，慢节奏会被推回）。见 [evidence-dump verdict](afp9-evidence-dump-verdict-2026-10-13.md)。
16. **取景不得用引擎缓存量做参照**：几何 `worldBound` 是引擎按自己节奏刷新的缓存，可停在泊位之前（AFP9g 实测差 8000 单位，导致人物偏深 2820、变小变暗）；定心必须用本帧自己写下的活数据（骨骼世界坐标，`figure_skeleton_bound`），palette 槽位先经本树节点集合过滤。`center-d=0` 只证明"delta 把所测中心摆到了锚点"，不证明测的是对的对象。暂停帧也要重泊（菜单会把世界画在背景里）。见 [stale-bound verdict](afp9-stale-bound-verdict-2026-10-13.md)。
17. **`NiSkinInstance::frameID` 是"本渲染帧已算过蒙皮"的门**：若引擎在取景写入之前已算过，`SetupGeometry` 会保留旧矩阵——蒙皮身体按旧骨骼世界（泊位）绘制而不可见，刚体挂件照常可见（AFP9h 实测：同一会话完整人物与孤斧交替）。直绘前必须 `invalidate_skin_cache()`（置 `frameID` 为无效），让引擎按本帧骨骼世界重算；**不得写 `boneMatrices`**（缓存归引擎所有），顺带消除姿态的一帧延迟。见 [skin-cache verdict](afp9-skin-cache-verdict-2026-10-13.md)。
18. **`Actor::SetPosition(pos, a_updateCharController)` 的第二参必须按需传 true**：只挪引用不搬控制器时，碰撞/激活目标留在原地（玩家身边仍能感到克隆体），且引擎的 3D 推导源（控制器）也停住——这就是"world/local 写入被每帧推回 spawn"的底层原因。引用确实移动时传 true（`park-3d warp=1`），未动时传 false 以免每帧 warp。见 [controller-park verdict](afp9-controller-park-verdict-2026-10-13.md)。
19. **装配窗口内的 fade 必须是 0，且窗口按"就绪即走"关闭**：0.05 是玩家身体里一个可见的幽灵（2026-10-14 用户报告"前几秒仍在玩家附近"）；引用又不能提前搬走，因为**角色 LOD 在引用装载时按距离选定**（run 104：泊位深度出生的引用装配出缺件、蒙皮剥离的低模图）。因此窗口里做三件事：fade 归零、每帧普查、**几何非空 + skin data 齐全 + 每个已分配槽位已绑定 + 连续 10 帧不变**就立刻泊位；240 帧只是从不就绪时的兜底（超时照泊 = 旧行为）。门用**已分配**槽位而非 NIF 声明容量（复制图落定门实测 47/131 渲染正常，等 131 白等 600 帧）。见 [AFP10 测试单](afp10-spawn-shape-dress-test-2026-10-14.md)。
20. **本机体型 = 网格 morph，不是节点缩放**：639 个 `.osp` slider set 零骨骼/缩放定义，BodySlide 预设由 `.tri` 顶点 delta 实现；复制 `local.scale` 等于什么都没复制。且**新放置的 NPC 会被 OBody NG 随机派预设**（`OBody.log`：`No preset defined for this actor, getting it randomly` → `Preset … will be applied to CharacterPanel Puppet`），所以"傀儡体型 = 玩家"必须显式同步，且写前先 `ClearBodyMorphKeys(傀儡, "OBody")` 清掉那份随机预设。
21. **skee64 的 `BodyMorph` 接口只能按镜像虚表 + 版本门调用**：接口经 SKSE 接口交换（消息 `0x9E3779B9` → `IInterfaceMap::QueryInterface("BodyMorph")`）取得；本地镜像必须保持上游**虚表顺序**，并在首次调用前查 `GetVersion()`（本机为 5），低于镜像布局一律拒绝——不猜偏移、不硬编码槽位。获取失败（无 RaceMenu/版本不符）时保持原样并只打印一次。见 [AFP10 测试单](afp10-spawn-shape-dress-test-2026-10-14.md)。

## 验证与安全修改

本地验收只构建 CharacterPanel 的两个配置与上述聚焦 regression target，再检查 `git diff --check`、删除符号引用和当前链接。最多两条有意义行为案例，不恢复已验证的测试矩阵。

游戏 PC 仍需 smoke：F7/F8 捕获及显示；确认 driver 为傀儡 clip、捕获 clip 或原 fallback；在运行世界完成 AFP 校准，再暂停使用 F2/F4 比较输出及 apply；验证没有改变来源与 NPC。AFP9 增判据：A2 行对账恒等式（written+unreachable+unprocessed=output-slots）与审计点名；F6 开时 R01 身份行（root/graph/generation 同源）、A5vsA4 max-rot≈0、restore 对账 ok=1、关面板回世界无副作用。本地构建与 CTest 不能代替这些结果。后续直绘、过渡和覆盖工作依据 [AFP 后续验证单](afp-next-validation-and-paused-gaps-2026-10-13.md) 与 [AFP9 测试单](afp9-coverage-and-direct-draw-test-2026-10-13.md)；场景后端规则依据 [路由文档](display-backend-routing-2026-10-13.md)。

修改 engine 入口、ABI、钩子、所有权或物理接口需要独立设计与游戏验证。本轮没有引入新 hook 或改变图回收策略。维护代码时同步本文、README、CMake、包 README 与 Changelog。

## 历史证据

[M0 交接](m0-handoff.md)、[S2 探针证据](s2-anim-probe-evidence-2026-10-09.md)、[AFP 汇总](afp-summary-2026-10-13.md) 和 `docs/diagnostics` 保持不变。旧文档的源码／工具链接和按键描述针对其记录的 Git revision；被删除源码请从 Git 历史查看，不为了让归档链接可点击而恢复死代码。既有 `dist`、原始日志、测试记录未删除。
