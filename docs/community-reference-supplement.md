# 社区实现参考补充：独立渲染、装备外观与角色展示

日期：2026-09-29  
状态：已核对公开说明与相关源码；尚未完成 PlayerPanel 内的移植和游戏验证。

本文补充 [PlayerPanel PRD](player-panel-prd.md) 的技术调查依据，记录 Dragon’s Eye Minimap、Outfit Preview Selector（OPS）、Apparel Preview 与 SosGui 的参考价值。O 表示世界中的实际玩家，P 表示独立面板角色。

这些案例用于定位可研究的调用链和模块，不改变 PRD 的“世界画面 → 不透明摄影棚面板 → 其他 UI → 最终后处理”顺序，也不代表独立 P、独立动作与渲染模组兼容已得到验证。

## 1. 参考分工

| 项目 | 已核对的实现内容 | 对 PlayerPanel 的主要价值 | 证据边界 |
| --- | --- | --- | --- |
| Dragon’s Eye Minimap | 局部地图上下文、相机、剔除任务、累积器、离屏目标和 HUD 接入 | M0：研究额外视图的完整渲染调用链 | 复用世界及地图渲染能力，不是独立角色摄影棚。 |
| Outfit Preview Selector | 操作实际玩家与第三人称相机，控制展示角度、补光并恢复状态；作者说明包含暂停动画和物理更新 | M1/M2：展示交互、相机构图、暂停更新与状态恢复 | 已核对的公开路径围绕 O；不能证明 O、P 的动作和渲染隔离。 |
| Apparel Preview | 在玩家装备模型装配过程中注入预览护甲，并调整装配用槽位掩码 | M1：装备外观装配、槽位遮挡、刷新与清理 | 修改 O 的可见 3D，未创建独立 P；不改库存不等于不改 O 的外观。 |
| SosGui | 在自定义装备预览窗口调用原生物品 3D 管理器 | 菜单物品展示与 UI 配合 | 单件物品预览，不能证明独立角色或独立离屏场景。 |

## 2. Dragon’s Eye Minimap：M0 的优先渲染参考

### 已确认的源码行为

`Minimap::InitLocalMap()` 创建并初始化局部地图对象，取得其剔除和相机上下文。`RenderOffScreen()` 随后准备场景、执行剔除任务，通过引擎渲染目标管理器绑定深度与 `kLOCAL_MAP_SWAP`，提交绘制并处理结果。HUD 的 `PreDisplay` 路径触发这次额外渲染。

其中 `NiCamera__Accumulate` 使用定位 ID `99789 / 106436`，与当前 CommonLib 中的 `Renderer::SubmitAccumulator` 对应。这提供了完整调用场景，不能简化为切换一个全局累积器指针。

来源：[初始化](https://github.com/alexsylex/DragonsEyeMinimap/blob/main/source/MiniMap.cpp)、[离屏渲染](https://github.com/alexsylex/DragonsEyeMinimap/blob/main/source/WorldRendering.cpp)、[HUD 接入](https://github.com/alexsylex/DragonsEyeMinimap/blob/main/source/Hooks.cpp)。

### 应研究的部分与限制

- 对照初始化、剔除、目标绑定、提交和清理的先后关系，核对当前原型遗漏的前置状态。
- 区分地图专用处理与通用场景提交，不能把地图的渲染模式直接用于角色皮肤和装备。
- 该实现使用世界场景及其照明相关状态，不能直接证明独立摄影棚灯光或任意 CS 效果可用。
- 其创建局部地图上下文的方式，也不等于 CommonLib 已提供通用独立剔除器工厂。

## 3. Outfit Preview Selector：展示控制与暂停更新参考

### 已确认的源码行为

公开 `MenuCamera.cpp` 直接取得实际玩家与玩家相机，调整第三人称视角；旋转展示时修改玩家朝向，退出时恢复角度、相机和相关设置。该路径展示的是 O。

来源：[MenuCamera.cpp](https://github.com/fatalCMD/Outfit-Preview-Selector/blob/main/native/src/MenuCamera.cpp)。

作者说明还描述了暂停期间推进玩家动画和受支持的 FSMP 更新。其物理更新可能同时推进附近的活跃物理对象，因此“世界暂停而角色能动”不能直接等同于独立 P 的动画／物理隔离。

来源：[作者功能与兼容说明](https://www.nexusmods.com/skyrimspecialedition/mods/184943)。

### 对本项目的用途

- 借鉴构图、旋转、缩放、补光和鼠标／手柄交互。
- 借鉴进入展示、换装重建、暂停更新、退出恢复的生命周期安排。
- 移植时将控制对象限定为 P 的展示状态，不沿用修改 O 朝向或游戏主相机的行为。
- 暂停动画和物理路径需在目标运行时重新验证；不要直接继承旧 FSMP 布局或更新频率假设。

### 版本边界

本轮看到的公开仓库 README 标示版本为 1.3.0，目标环境是 Skyrim 1.5.97 / SKSE 2.0.20；Nexus 发布页标示版本为 1.5。公开代码与新版全部功能的对应关系尚未确认，不能据此断言新版 Portrait Mode 的所有内部行为。

来源：[公开仓库与版本说明](https://github.com/fatalCMD/Outfit-Preview-Selector)。

## 4. Apparel Preview：装备外观装配的重点参考

### 已确认的源码行为

`BipedHooks.cpp` 在已穿装备处理过程中识别实际玩家，在原生装配之后注入预览护甲，并调整外观装配使用的槽位掩码，使头盔等预览物品能够参与头发和身体遮挡。装配使用玩家第三人称 Biped 上下文。

来源：[BipedHooks.cpp](https://github.com/maartenharms/apparel-preview/blob/main/src/BipedHooks.cpp)。

`PreviewSession` 管理预览集合、种族适配检查及结束／装备变化后的刷新。作者说明明确把功能限定为外观预览，并要求其他模组提供玩家的菜单取景。

来源：[PreviewSession.cpp](https://github.com/maartenharms/apparel-preview/blob/main/src/PreviewSession.cpp)、[功能说明](https://www.nexusmods.com/skyrimspecialedition/mods/185334)、[依赖说明](https://github.com/maartenharms/apparel-preview)。

### 对本项目的用途

- 研究如何借助引擎装配装备模型，同时避免为了展示而改动实际库存和装备状态。
- 研究槽位冲突、身体／头发遮挡、种族与装备模型不匹配时的处理。
- 研究换装、模型重建和关闭预览时的刷新及清理边界。
- 若 P 采用独立 Actor，需要将玩家身份判断、Biped 获取和刷新逻辑改为 P 的受控上下文。
- 若 P 仅为场景图快照，需要另行解决装配上下文及蒙皮绑定；不能只替换玩家指针。

本项目当前要求同步实际生效的装备。Apparel Preview 的“未装备物品试穿”仅提供技术参考，不因此加入首版产品范围。其公开路径也没有证明独立 P 或摄影棚离屏渲染。

## 5. SosGui：原生物品预览参考

`OutfitEditPanel` 的装备预览调用原生 `Inventory3DManager` 加载并绘制物品，再结合菜单相机参数调整展示位置。它可用于研究物品预览与自定义界面的配合，但没有提供独立角色摄影棚的完整依据。

来源：[OutfitEditPanel.cpp](https://github.com/cyfewlp/SosGui/blob/main/src/gui/OutfitEditPanel.cpp)。

## 6. 对后续实施的约束

1. M0 优先核对 Dragon’s Eye Minimap 的完整离屏调用链，再决定如何适配独立角色；不要把全局累积器交换当作完整渲染入口。
2. M1 优先研究 Apparel Preview 的装配逻辑，同时保持 O 的外观和游戏状态不受 P 操作影响。
3. M1/M2 参考 OPS 的展示交互和暂停更新，但独立动画、物理和生命周期仍须另行验证。
4. 各案例支持的是不同模块，组合后的 PlayerPanel 仍需完成 PRD 的双视图和状态隔离验收。

区分三个证据层级：CommonLib 声明证明“有接口线索”；社区源码证明“有具体应用方式”；本插件在目标环境中的实测才证明“适用于 PlayerPanel”。参考项目的成功不能替代最后一层。

本轮引用的是公开网页可见的分支源码，未固定提交哈希。正式复用前需固定版本、核对接口与许可，并区分源码和发布资产；本文没有复制第三方实现，也没有认定这些项目已在本项目目标组合下兼容。

相关文档：[PRD](player-panel-prd.md)、[M0 诊断](m0-diagnostics.md)、[用户采集报告](m0-capture-report-2026-09-28.md)、[累积器原型试验](m0-proto.md)。后两者保留试验过程，本文不把其中未经独立核实的推断升级为已验证接口。
