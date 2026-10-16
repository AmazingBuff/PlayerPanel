# 当前代码维护说明

更新日期：2026-10-16（CS 独立材质视图与引擎路径验证／回退决策；面板站立 idle 的 OAR 主动查询链；此前：copy-primary 显示路由、审查修复、OAR 条件 hook 与投影接入、UBE 透明材质试修、双手武器对账修复）。本文是当前构建、模块和开发控制的入口；逐轮实验报告仍保留为历史证据。

> **当前动作链入口**：[idle 选择与快速换装：oar-idle2](oar-preview-query-2026-10-16.md)。
> 全局窗口和运行 clip 回退已移除。新链路接入站立基础片段、原角色身份、独立 clip／变体／
> 时钟、独立概率／变体状态、各层循环与最新装备打断；外观仍等待装配稳定。旧交接文档保留为历史探针，本轮游戏行为待验证。

## 范围

场景复制配置当前从玩家实际第三人称图取得外观，由独立 OAR 查询及绑定采样提供动作，不创建展示 Actor。默认配置和 AFP 傀儡诊断仍保留；下文 2026-10-15 的 Actor 装配／体型同步修复针对这些旧路线。FaceGen 表情、独立 SMP／CBPC、双人物与安全回收仍未实现。

## 审查修复（2026-10-15）

- 动画 binding 显式区分 `track` 与 `bone`：按 track 采样、按 bone 找节点，采样数量取动画的真实轨道数。非空但不可读的映射表拒绝解析；空表仍保留 `identity-unverified` fallback。
- `PuppetDriver::drive()` 同时检查 generator 与目标 root；F7 重捕获后重新绑定到新树。
- `park()` 直接比较目标泊位和当前 Actor 的引用位置，不再复用上一 Actor 的泊位缓存。
- 动画验证仅在取得足够骨骼并实际评分后锁住本次尝试；缺图、缺姿态或比较数量不足时可在后续运行帧重试，实际映射不匹配仍阻止写入。
- 手部同步先完成双手卸装，再按明确手槽装备；仍被另一手使用的物品保留，双持同款单手武器时补足两件，双手武器保留其原有双手槽。
- 体型同步先快照双方的 OBody 集合，再比较值与条目是否存在；目标多余条目与玩家清空预设都会触发清除和重新应用，其他键不写入。`copied=0` 也可能是有效的清除更新。
- 新游戏也排队创建傀儡，实际创建仍等待未暂停帧。穿衣后重新开始统计稳定帧，满足绑定与稳定条件才泊位；240 帧上限保持为兜底。

本地新增两个回归：带乱序和未映射项的轨道映射，以及空来源对目标残留体型的识别。游戏内仍须验证新游戏／原地重读存档、双持换手、清空 OBody 预设和慢速装备装配；本地测试不证明这些引擎行为已通过。旧图的安全销毁仍未解决。

## UBE 透明材质试修（ube-alpha1）

复制图和傀儡自身 3D 都通过共用的 `draw_figure_passes()` 绘制。本轮针对眉毛、眼周阴影的
mesh 轮廓黑块补齐透明处理；**2026-10-16 用户在另一台电脑确认面板人物渲染完全正常**。

- 从每个 mesh 的 `NiAlphaProperty` 读取 alpha test 开关，使用 `0x204` 让引擎按原材质设置
  混合与裁剪阈值；不再把菜单 pass 的 `0x200` 和 `passEnum & 0x40` 当作通用材质状态。
- 实体／仅裁剪几何先画，混合几何随后按实际 UI3D 相机方向由远至近稳定排序；`No Sorter`
  表面保留透明阶段的插入位置。混合阶段的基准深度只测试不写入，剔除遵循双面标志。
- 每个 pass 更新引擎材质状态缓存；退出时恢复 viewport、深度、剔除、混合、alpha reference
  及原有待更新标志。外层 D3D 捕获负责恢复实际绑定。
- VS／PS 常量缓冲保存覆盖全部 14 槽；D3D11.1 额外保留每个绑定的偏移和长度。
  WARP 离屏回归验证高槽位、完整缓冲与子范围的实际恢复。

透明轮的默认与实验两种 Release 配置均构建通过；透明顺序、材质缓存恢复和 WARP 缓冲范围
三项为该轮新增。后续 OAR 与手部修复继续扩展同一个测试入口，当前共 31 项。透明包身份、校验值、复现命令和游戏
判据见 [UBE 透明材质验证](ube-transparency-test-2026-10-15.md)。透明 mesh 内部三角形排序、
折射和第三方 OIT 专属流程不属于本轮实现。
游戏内结果：眉毛／眼周黑块消失、妆容正常（用户确认）——见该测试单的"游戏内结果"节。

## 当前模块与数据流

CS 1.9.1 材质视图试验（`cs-view1`）在两种来源共用的提交阶段接入独立材质缓冲、原生 SSS 和原生材质合成；相机、摄影棚灯、反射环境及深度属于面板。完整版本身份、恢复边界、显存开销和游戏检查见 [Community Shaders](community-shaders.md)。本地验证通过，游戏结果待回传。

后续渲染路线按 [引擎路径验证与回退](engine-render-path-validation-2026-10-16.md) 执行：先关闭手动 CS 补偿做原生入口对照，必要阶段、资源归属与世界恢复全部通过后再考虑替换；不满足或证据不足就保留当前 `cs-view1`。本次只记录验证计划，没有切换运行后端或新增诊断开关。

| 模块 | 职责与入口 |
| --- | --- |
| [oar/hooks.h/.cpp](../src/oar/hooks.h) | 对 3.1.6／3.2.1 核对 DLL 身份和入口；主路径按精确 project、站立基础索引主动查询，在独立 clip 内审计及投影条件，候选／变体指针不越过原生读锁。全局窗口已删除。`kProjected` 及原答案差异统计仅供旧傀儡诊断。见[当前证据与限制](oar-preview-query-2026-10-16.md)。 |
| [oar/query_policy.h](../src/oar/query_policy.h) / [preview_clip.h](../src/oar/preview_clip.h) | 原版动作目录匹配、目录缓存、面板变体状态和独立原生 clip／context 载体；不依赖历史播放样本。 |
| [oar/selection_state.h](../src/oar/selection_state.h) | 面板内 Random 缓存、变体权重／序列／PlayOnce、作用域共享与循环重评规则；选择提议和播放提交分开，不写世界角色状态。 |
| [main.cpp](../src/main.cpp) | SKSE 初始化、消息、渲染安装和存档会话边界；不再转储引擎映像。 |
| [panel.cpp](../src/panel/panel.cpp) | 菜单状态、预览请求、打开通知和玩家装备事件修订号。 |
| [renderer.cpp](../src/render/renderer.cpp) | 渲染回调、D3D 状态保护、摄影棚目标及合成；按构建选项选择当前后端。 |
| [character_manager.cpp](../src/character/character_manager.cpp) / [character_clone.cpp](../src/character/character_clone.cpp) | 默认路线通过 Actor 装配装备／3D，再摘出图绘制；`pose()` 负责摄影棚取景。该路线不等于尚待验证的“保留 Actor 与 3D 绑定后直绘”。 |
| [scene_graph_copy.cpp](../src/character/scene_graph_copy.cpp) | 打开及装备稳定后自动捕获、F7 手动捕获、来源／副本审计和旧图停放。失败保留上一接受图；`animate_at_base_pose()` 在取景后调用独立播放，不使用世界当前 clip 回退。 |
| [panel_animation.cpp](../src/character/panel_animation.cpp) / [panel_animation_policy.h](../src/character/panel_animation_policy.h) | 各 idle 层独立循环、有限切换、当前姿态混合和最新请求代次。装备变化立即作废旧提议，稳定后换外观；重捕获重绑活动／待装载节点，保留同一动作的进度。 |
| [animation_source.cpp](../src/character/animation_source.cpp) | `resolve_binding_playback()` 持有 binding 强引用，按 track→bone 采样到当前显示树；ClipPlayer 支持循环及单次播放。activeNodes／运行 clip 读取保留给旧诊断，主显示路径不调用。 |
| [puppet_driver.cpp](../src/character/puppet_driver.cpp) | 存档会话内的傀儡装配、泊位、暂停图推进和装备同步；`drive()` 为显示副本供给 clip 姿态。AFP10：装配窗口改为"普查就绪即泊位"且窗口内 fade 归零（不可感知）；穿着整套 worn body items；`apply_body_shape()` 把玩家体型同步到傀儡。AFP10g：spawn 处断掉副本记录的自带套装（容器清空 + 双 outfit 置空），prune/worn 各持独立节奏计数器，箭袋对账到玩家当前弹药。OAR 投影：暂停帧的手动推图包在 `ScopedEvaluation(kProjected)` 里，OAR 对傀儡的整轮替换决策按站立预览求值（证据行 `SCOPY OAR project`）。手部对账按"物品是否被持有"（双手武器归一化）并带 churn 刹车，见不变量 32／33。 |
| [equipment_mirror.h/.cpp](../src/character/equipment_mirror.h) | AFP10 共用件：biped 身体槽位的 worn 集合读取、整套穿上、以及无状态对账（多则补、少则卸）。复制图的 `mirror_worn_equipment` 也走这一份，槽位表只有一处。手部武器不在此列（驱动器自己的手部轮询 + draw/sheathe）。AFP10g：槽位表补全（kHair/kModBack/kModMisc1/2/kModPelvis*，kShield 归手部轮询、FX01 排除），`equipment_sync_ammo` 把傀儡当前弹药对账到玩家的，prune 的弹药豁免收窄为"玩家的当前弹药"。双手武器：`hand_two_handed_weapon()`／`hand_item_is_wanted()`／`hand_object_equipped()` 是手部对账的单一判据来源，strip 对两处都报的同一件物品只卸一次。 |
| [body_shape.h/.cpp](../src/character/body_shape.h) | AFP10 体型同步：本地镜像 skee64 的 `IBodyMorphInterface`（SKSE 接口交换 `BodyMorph`），先比较后写入，写前清掉 OBody 派给傀儡的随机预设；版本低于镜像布局时拒绝调用。 |
| [idle_driver.cpp](../src/character/idle_driver.cpp) / [snapshot_transform.h](../src/character/snapshot_transform.h) | 旧 Actor 路线的程序 idle、取景变换和子树更新；独立 OAR 主路径不回退程序 idle。 |
| [final_pose_probe.cpp](../src/character/final_pose_probe.cpp) / [final_pose_apply.cpp](../src/character/final_pose_apply.cpp) | 保留最终输出读取、索引校准、独立真值比较、暂停写入和强制生成诊断。F6 默认关闭且当前不创建傀儡；AFP9 的 output/attempted/written/unreachable/unprocessed 对账只适用于启用旧诊断的情形。 |
| [figure_audit.h/.cpp](../src/character/figure_audit.h) | AFP9 共享审计件：skin palette 读取（只收集指针、绝不解引用槽位；引擎从未渲染的图记 unbound-palette）、碰撞辅助体过滤的蒙皮包围盒、取景 placement 解算、槽位必要性分类（palette 成员 → Weapon* 附着 → helper）。AFP10 另收编装配普查 `FigureAssembly`／`figure_measure_assembly()`（几何/skin/已分配槽位/绑定数与稳定签名），复制图的落定门与傀儡的泊位门共用它。 |
| [puppet_draw.h/.cpp](../src/character/puppet_draw.h) | AFP9 R01 直绘：F6 开关；傀儡 `Get3D(false)` 根 + 共用 figure_passes 缝；临时取景（保存/恢复根 local + world/worldBound 对账）；固定骨集 A4/A5 采样；不可用时回退复制图并告警。 |
| [isolation_watch.cpp](../src/character/isolation_watch.cpp) | 来源与 NPC 的观测、装备变更重新设基线及会话判读；不是全骨骼／全部物理隔离已通过的证明。 |
| [figure_passes.cpp](../src/render/studio/figure_passes.cpp) | 两条来源共用的按材质分类、引擎 pass 生成、灯光临时替换与提交；读取原 alpha 属性，原生混合设置使用 `0x204`。 |
| [figure_draw_order.h](../src/render/studio/figure_draw_order.h) / [figure_pipeline_state.h](../src/render/studio/figure_pipeline_state.h) | 实体／透明阶段排序及 No Sorter 边界；逐 pass 的引擎材质状态准备和作用域退出恢复。 |
| [community_shaders/view.cpp](../src/render/community_shaders/view.cpp) / [bridge.cpp](../src/render/community_shaders/bridge.cpp) | CS 1.9.1 独立材质视图、受版本保护的 State::Draw hook、私有 GBuffer／SSS／材质合成和 CPU/GPU 上下文恢复。 |
| [community_shaders/binary_profile.h](../src/render/community_shaders/binary_profile.h) / [gpu_data.h](../src/render/community_shaders/gpu_data.h) | 官方 DLL/PDB 的入口、全局符号及 GPU 布局；更新上游前必须重新核对，不可只改版本号。 |
| [d3d11_util.h/.cpp](../src/render/dx11/d3d11_util.h) | VS／PS／CS 保存全部常量缓冲、资源和采样器槽，另存计算 shader／UAV；D3D11.1 保留缓冲子范围。离屏目标支持浮点 UAV、可采样深度与只读 DSV。 |
| [snapshot_transform_test.cpp](../tests/snapshot_transform_test.cpp) | `snapshot_transform` 内共 31 项本地检查，新增 Random 缓存、权重与共享状态、PlayOnce 提交、循环决策、最新请求、各层周期及中断混合。不执行游戏内 detour 或装卸。 |
| [render_state_test.cpp](../tests/render_state_test.cpp) / [native_sss_test.cpp](../tests/native_sss_test.cpp) / [native_material_test.cpp](../tests/native_material_test.cpp) | 五项离屏资源／绑定／编码／版本检查；可用官方包的实际 HLSL 执行另外十六项 SSS 与材质合成案例，不装载 CS DLL。 |

实验配置使用复制图显示时的主顺序：请求／会话与装备稳定处理 → 查询及装载轮询 → 副本 `pose()` → 独立绑定采样／切换与过渡 → 引擎蒙皮与 draw → 合成。AFP 及傀儡推图保留为旁路诊断。

AFP 写入与复制图显示是两条不同的路径；不得把 AFP 应用后的节点自比较标记为直绘通过。
两种显示来源的提交阶段共用“实体／镂空 → CS 材质处理（匹配版本可用时）→ 排序透明 → 恢复状态 → 合成”的顺序。

## 构建、控制与打包

现有工具链仍为 Windows x64、VS2022 / MSVC 14.44.35207、C++23 和仓库内 CommonLibSSE；依赖由现有 vcpkg manifest 提供。本轮不升级或安装依赖。`ENABLE_SKYRIM_SE/AE/VR` 的选项与支持范围保持原样；当前 DLL 版本声明以 AE 1.6.1170 为准。

默认路线：

```powershell
cmake --preset Release -DCHARACTER_PANEL_SCENE_COPY_EXPERIMENT=OFF
cmake --build build --config Release --target CharacterPanel
```

场景复制与 AFP 开发配置：

```powershell
cmake --preset Release -B build-scopy -DCHARACTER_PANEL_SCENE_COPY_EXPERIMENT=ON -DCHARACTER_PANEL_PROBE_REVISION=cs-view1
cmake --build build-scopy --config Release --target CharacterPanel CharacterPanelCopyMathTest CharacterPanelRenderTest
ctest --test-dir build-scopy -C Release -R '^(snapshot_transform|render_state)$' --output-on-failure
cmake --build build-scopy --config Release --target CharacterPanelSceneCopyPackage
```

在本地依赖已安装时，可以传 `-DVCPKG_MANIFEST_INSTALL=OFF -DVCPKG_INSTALLED_DIR=<现有 installed 目录>`，避免配置触发 registry 更新。CommonLib 还使用 FetchContent 获取 hde64；离线验证需另传 `-DFETCHCONTENT_SOURCE_DIR_HDE64=<现有 hde64-src 目录> -DFETCHCONTENT_FULLY_DISCONNECTED=ON`。新建验证树也应保留 Release 配置与已安装的 Release detours 库路径，避免误选 debug 库。禁止将离线配置解释为缺失依赖也已验证通过。

| 控制 | 当前行为 |
| --- | --- |
| 面板打开 | 自动重捕获玩家第三人称 3D；站立基础片段准备好才显示。动作由独立 OAR 查询与播放提供，换装稳定后刷新外观并有限过渡回 idle，不再 spawn 展示傀儡。 |
| F7 | 暂停背包中手动重新捕获并审计玩家第三人称 3D；替换前一副本（面板打开时已自动捕获，此键保留为手动覆盖）。 |
| F8 | 切换当前已接受副本的显示。 |
| F2 | 一次 AFP mark：输出、校准、来源对照、相机与目标网格状态；A2 行含 AFP9 覆盖对账与审计点名。 |
| F4 | AFP 驱动档位：`auto → off → tick → tick+apply → tick+apply+force → step`。**默认 `auto`**：暂停帧强制 generate（面板才有姿态可写），运行世界不强制（映射证明需要未被引擎强制过的帧）。单步为一次 1/60 秒。 |
| F6 | R01 直绘诊断开关（**默认关**，copy-primary 起傀儡不再 spawn）：有傀儡时开 = 面板画傀儡自身 3D；关 = 复制图。不可用时回退复制图并告警。 |
| F11 | 旧傀儡诊断的 OAR 投影开关（默认开），保留 A/B clip 与原答案差异统计；不关闭独立查询的预览隔离。 |

F3 不再由本项目绑定；旧旋转、释放、idle toggle、S2/PuppetProbe stage 没有控制入口。生命周期边界仍由重捕获／读档／主菜单处理。F4 的冻结／推进针对傀儡图与最终姿态应用，不等于冻结独立时钟运行的显示副本。

`CHARACTER_PANEL_SCENE_COPY_EXPERIMENT` 默认为 OFF。删除 `CHARACTER_PANEL_BUILD_PROBE`、`CHARACTER_PANEL_S2_PROBE`、M0 probe/proto targets；旧 cache 可能仍列出历史变量，但当前构建已不消费它们。`CHARACTER_PANEL_PROBE_REVISION` 保留为兼容的构建身份／包 manifest 标记，与已删除探针无关。

场景复制包包含 DLL、PDB、身份与 SHA-256 manifest、当前包 README，输出在 `dist`；打包不部署。
当前动作验证包为 `CharacterPanel-scopy-oar-idle2-1.2.1.zip`，入口与检查见[本轮记录](oar-preview-query-2026-10-16.md)。
上一轮 `oar-preview1` 包保留，不覆盖。
透明轮的历史包 `CharacterPanel-scopy-ube-alpha1-1.2.1.zip` 身份保留于[其测试单](ube-transparency-test-2026-10-15.md#测试包身份)。
提交源码不会追写既有包身份，核对时以各包清单和记录为准。
如果为了复用依赖而传入 MSBuild 的 `BuildProjectReferences=false`，须先显式构建 `CharacterPanel`
再运行打包目标，避免只打包旧 DLL。AFP10 的既有部署记录保留在对应测试单与 Changelog。

## 保留诊断及理由

- 图隔离审计、来源漂移观测、内存／vtable／数据范围检查：对象来自游戏及第三方插件，指针与 ABI 生命周期仍是真实边界。
- AFP 两个输出槽位、A0/A1 和 poseLocal／节点对照、索引次序校准：双缓冲选择与最终姿态来源尚未完成验证。
- 必需骨／palette、local/world/scale、覆盖计数：标定 98 项而强制生成 116 项的问题尚未关闭。不能删掉这些证据或调整通过阈值掩盖缺口。
- 相机、几何、蒙皮与目标回读：同一 Actor 的直绘已通过 F6 接入，仍需要区分应用到正确树、取景、材质和渲染目标问题。
- force-generation、freeze 和 step：暂停动画链路的必要操作控制；不修改原有算法与阈值。

已移除 AFP 未命名字节的 raw hex dump；保留 typed 状态、输出数量和插值信息。S2 swing／T0–T3 蒙皮缓存探针、capture-time perturb/replay、catalogue/loader/registry/vtable 扫描、idle 池统计、HKX42–44 求解与延迟 A/B、程序 idle 位移日志，以及无调用者的容器／尸体／库存／按键／hash 模板 helpers 均已退役。

## 不变量、生命周期和未关闭事项

1. 复制图仍先审计，再清除其 controller、碰撞和 userData；不改写来源图，不为动画写入 `NiSkinInstance::boneMatrices`。
2. 先恢复副本取景，再写入动画，禁止恢复捕获姿态覆盖本帧动作。引擎继续负责蒙皮提交。
3. 傀儡保持 Actor／3D 关联用于行为决策；在暂停期间只手动推进它的图。默认 Actor 装配后摘图路线仍单独保留。
4. AFP 每帧重新查当前树，保留已验证映射和必要的 bone-table fallback；不跨帧缓存目标节点。当前映射覆盖和双缓冲策略没有在本轮修复。
5. typed clip resolver 保留 table 与 `identity-unverified` fallback；table 的轨道编号和骨骼编号现已分开，非 identity 的索引关系有本地回归。空表映射、clip 混合与任意 OAR 动作仍需游戏证明。
6. 旧图仍通过 `snapshot.release()` 停放，原有释放崩溃没有修复。12 项仅限制 bookkeeping；更早指针被移除时内存仍泄漏。不要进行无限重捕获压力测试，也不要把无崩溃称为回收通过。
7. 独立物理、脸部表情、傀儡路线 OBody 以外的体型适配、尸体卸装刷新及双人物后端路由仍是后续工作；当前复制图直接取得来源已有外观。
8. 傀儡 clip 绑定在 generator 或目标 root 切换时重建；重捕获不再沿用旧 root 的节点缓存。重复捕获后的显示持续性仍需游戏验证，不等于旧图安全回收已通过。
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
22. **morph 存储是多方共用的，只许写自己那把键**：AFP10 首测把玩家的全部 morph（193 条）照搬到傀儡，结果傀儡"凭空"多出弓与匕首——因为随行搬过去的还有 **XPMSE 自己的"当前装备了哪类武器"状态**（`XPMSEAABase_axeeqp/_boweqp/_dageqp/_1hmeqp/_2hmeqp`），傀儡的武器附着节点于是相信那些武器在那里。身体形状只住在 **OBody 键**下（OBody 写预设用的就是这把键），所以同步只拷该键，其余键只计数（`skipped=`）绝不写入。见 [AFP10 测试单](afp10-spawn-shape-dress-test-2026-10-14.md) §3。
23. **镜像必须双向，只装不卸等于累积**：手部轮询原先只在"玩家手上有东西且换过"时装备，玩家空手时只推 sheathe——于是本会话里画过的每把武器都永久留在傀儡身上（2026-10-14 用户报告：玩家身上没有的斧头/盾/弓出现在 clone 上）。同步语义 = **先卸掉傀儡手里与玩家不同的那一件，再装玩家的**；日志行必须同时给 `equipped=` 与 `removed=`，否则下一轮无法区分"没装"与"没卸"。见 [AFP10 测试单](afp10-spawn-shape-dress-test-2026-10-14.md) §2。
24. **新放置的 Actor 不是空的**：它的基础记录自带作者放进去的东西，而傀儡的基础记录是**玩家 NPC 的副本**——2026-10-14 用户报告的"默认新 Actor 的弓箭/盾牌/斧头"就是它自带的，且在装配窗口内开面板（穿衣之前）就会看到；叠在镜像装备之上就是"装备叠加"。所以傀儡**出生时剥一次、穿衣前再剥一次**（每个 biped 槽位的 worn 项 + 双手 + 全部库存条目），并把 `items=/removed=/unequipped=/inherited=[名字(formID)]` 打进日志——自带的到底是什么由日志点名，不靠图猜。另外：装 worn 项前先查 `GetItemCount`，已有就不再 `AddObjectToContainer`（重复添加是叠加的另一半）。见 [AFP10 测试单](afp10-spawn-shape-dress-test-2026-10-14.md) §6。
25. **蒙皮 palette 是"画过才分配"的，取景不能等它**：引擎在**绘制**几何时才分配 `NiSkinInstance` 的骨骼指针数组，所以从未被画过的图每个 skin 都是 `allocatedSize==0`——此时 `bound-nodes=0/0` 且 `skin-data-null=0 bones-null=0`（AFP10d 实测：壳 44 个蒙皮几何全为 0）。`figure_skeleton_bound` 只能从 palette 取点，因此恒为 invalid → `begin_frame` 不取景 → `draw` 报 `not-framed` → 回退复制图（没有 F7 捕获时面板就是空白）：**"等 palette 才能画"与"画了才有 palette"是死锁**。取景必须回退到**树自身的节点云**（`figure_node_cloud_bound`，活数据、排除碰撞辅助体），画过一次后自动回到 palette 活值；日志行 `R01 frame bound-from=palette|nodes` 指出用的是哪一个。fade 归零让世界渲染器永不触碰壳，正是这条死锁的起点——若要再动 fade 守卫，先确认 palette 已由别的路径分配。见 [AFP10 测试单](afp10-spawn-shape-dress-test-2026-10-14.md) §7。
26. **镜像的"变了没"必须对着目标现状问，不能对着缓存问**：手部轮询原先按"玩家手上一次的 formID"判断是否需要装备。而剥离（AFP10c）会在**玩家双手毫无变化**的情况下清空傀儡的手——于是刚被剥掉的武器再也装不回来，直到玩家手动换一次武器（2026-10-14 用户报告：昨天武器还能换，今天初始装备出问题）。规则：**装备决策 = 傀儡此刻手里是什么 vs 玩家手里是什么**（无状态），缓存只用于"要不要推 draw/sheathe"与日志；任何剥离之后必须清缓存。见 [AFP10 测试单](afp10-spawn-shape-dress-test-2026-10-14.md) §9。
27. **force-generate 与映射证明在时间上互斥**：暂停的面板若不强 generate，输出就是空的（`pose-empty`，AFP3），没有姿态可写；而映射证明要求"引擎生成并应用了同一帧姿态"的运行世界帧（AFP4：强制过的帧拿 0.99 打成 0.67）。所以**没有一个固定档位能同时满足两者**：默认档必须按暂停状态逐帧解析（`Drive::e_auto`，驱动侧 `force_when_paused`），F4 的五个固定档保留为诊断仪器。见 [AFP10 测试单](afp10-spawn-shape-dress-test-2026-10-14.md) §9。
28. **"剥"必须把物品从库存里删掉，且读库存要用 `GetInventoryChanges(false)`**：`(true)` 是"不初始化"，刚放置的 Actor 容器还没建 → 返回空 → 剥离报 `items=0`，而实测该壳手上**戴着**铁战斧 00013790（记录自带的起始套装）；套装随后进库存，**引擎的 best-fit 自动装备会把它再装回去**（傀儡没有 AI 去否决）。同理，"卸下"不等于"移出库存"：只卸不删，被卸的武器仍被携带，引擎会再装回来——这就是"武器装上就不再被替换"（2026-10-14）。现行规则 = 剥离 + **持续修剪**（凡玩家未穿且未持的携带物一律卸下并删除）+ 手部替换时把旧武器删出库存；计数报告**实际删除**而非 equip API 的返回值（同一会话里 `removed=0` 而武器仍在手上）。见 [AFP10 测试单](afp10-spawn-shape-dress-test-2026-10-14.md) §10。
29. **自带套装要在源头断掉，运行期剥离只是护栏**：套装的入口是 `CreateDuplicateForm` 副本记录自带的容器——引擎按它初始化新 Actor 的库存并 best-fit 自动装备。AFP10b–f 的"叠加/自带/装回"整条链，都是副本容器这个源头漏进来之后三层运行期补救（出生剥、穿衣前剥、持续修剪）在追。spawn 处对副本 `TESContainer::ClearDataComponent()` + `defaultOutfit/sleepOutfit` 双置空（复制图路线 2026-10-05 起就在用、从未出过此类报告的配方）之后，剥离只负责挡脚本/mod 的注入并点名日志。见 [AFP10 测试单](afp10-spawn-shape-dress-test-2026-10-14.md) §11。
30. **箭袋 = 玩家当前弹药，必须独立对账**：拉弓时背上的箭袋模型由 **worn ammo**（当前弹药）渲染，既非 biped 槽也非手槽，两套镜像都不经过它；prune 的"全部弹药豁免"会让残留箭（记录自带的）成为引擎 best-fit 的唯一候选。规则 = `equipment_sync_ammo` 把傀儡当前弹药对账到玩家的（先装后卸、替换即删出库存），prune 的豁免收窄为"玩家的当前弹药进 keep 集"。见 [AFP10 测试单](afp10-spawn-shape-dress-test-2026-10-14.md) §11。
31. **同一节奏的多个对账器不得共用一个"由别人递增、由别人清零"的计数器**：prune 的门读 `m_worn_poll_frames >= 30`，而该计数器只被 worn 块递增、且到达 30 的同一帧清零——门条件在世界运行期不可达，prune 从未在世界上跑过（`pruned` 行只在暂停期出现即判据）。规则：每个门自己递增自己清零（`a_every_frame || ++counter >= period` + 块内归零）。见 [AFP10 测试单](afp10-spawn-shape-dress-test-2026-10-14.md) §11。
32. **双手武器占两只手，且引擎轮流把它报在任一手条目里**：`GetEquippedEntryData(false/true)` 对一把双手武器会在两次轮询间翻面，因此换装对账**不能逐条目比较**。按条目比较的后果实测过：另一只手的期望是"空" → 每帧 `UnequipObject` + 再 `EquipObject`（日志 `equipped=1 removed=1` 每 20 ms、4 秒 104 次反应），同时 prune 在读数瞬空的帧把这把武器从傀儡库存里删掉，最终引擎在 `BipedAnim` 读到空 biped 对象崩溃（[crash 2026-10-15 22:45:27](diagnostics/crash-2026-10-15-22-45-27.log)，BiPolar Blade / LegacyoftheDragonborn.esm）。现行规则：`hand_two_handed_weapon()` 归一出"哪把是双手武器"，`hand_item_is_wanted()`（卸装侧）与 `hand_object_equipped()`（装备侧）按"这件物品是否被持有"判断，手部签名用双手武器归一化以免翻面被读成换装，prune 在**来源手部读数刚变化的帧**不执行，`equipment_strip` 对两处都报的同一件物品只卸一次。
33. **不收敛的对账必须有刹车**：这次崩溃的直接成因不是"比较写错"，而是**每帧重发引擎调用**（约 50 次/秒）把引擎不变量打崩。手部对账现在带 churn 门：10 帧窗口内超过 4 次装备/卸装动作即判不收敛，停手 90 帧并打 `hand reconcile churning actions=… in … polls`。语义修复负责让这条门保持不可达，门本身负责让下一个逻辑缺口退化成一行日志而不是一次崩溃。见 [本轮日志](../log-archive/CharacterPanel-20261015-2245-oar-ab-2hcrash.log)。

## 验证与安全修改

本地验证构建 CharacterPanel 的两个配置与上述聚焦 regression target，再检查 `git diff --check`、
当前符号引用和文档链接。仅为本轮真实风险补充行为案例：透明排序、材质缓存恢复、D3D11.1
常量缓冲范围、OAR 两种作用域的匹配面与差值计数、双手武器手部模型；不恢复已退役的重型探针矩阵。
16 项本地检查通过不等于游戏验收通过。

游戏 PC 当前 smoke 以[独立 OAR 测试单](oar-preview-query-2026-10-16.md#验证与上机)为准：不同世界行为下开包仍站立、各种武器 idle、有限换装动作及最终 idle、关闭／读档和来源无副作用。AFP9 的旧对账、F2/F4/F6 判据仍见 [AFP 后续验证单](afp-next-validation-and-paused-gaps-2026-10-13.md) 与 [AFP9 测试单](afp9-coverage-and-direct-draw-test-2026-10-13.md)；它们不替代本轮链路验证。

修改 engine 入口、ABI、钩子、所有权或物理接口需要二进制证据与游戏验证。本轮在原有条件和
数字值 hook 之外新增 OAR ConditionSet／ReplacementAnimation 守卫，两版入口独立核实；
独立资源租约与节点重绑也已接入。本地检查不等于实际 detour／引擎装卸验证，限制见[本轮记录](oar-preview-query-2026-10-16.md)。
旧图回收策略未改变。维护代码时同步本文、README、包 README 与 Changelog。
UBE 渲染已由用户在另一台电脑确认正常；本轮仍需确认新动作链下的换装、开关面板和读档。

## 历史证据

[M0 交接](m0-handoff.md)、[S2 探针证据](s2-anim-probe-evidence-2026-10-09.md)、[AFP 汇总](afp-summary-2026-10-13.md) 和 `docs/diagnostics` 保持不变。旧文档的源码／工具链接和按键描述针对其记录的 Git revision；被删除源码请从 Git 历史查看，不为了让归档链接可点击而恢复死代码。既有 `dist`、原始日志、测试记录未删除。
