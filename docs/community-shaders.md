# Community Shaders 独立材质视图（cs-view1）

本轮将面板摄影棚接入 **Community Shaders 1.9.1** 的材质流程。目标运行时仍为 Skyrim AE 1.6.1170 / SKSE 2.2.6。人物来源仍可为复制图或 Actor 装配图；不新增展示 Actor，也不改变已提交的 OAR 选择与播放逻辑。

这是待游戏验证的实现。编译、WARP 和官方着色器验证不能代替游戏内 hook、相机和最终画面的验证。

后续优先验证能否复用引擎已有的渲染阶段；未通过就回到本方案，验证通过前保留本实现。具体对照组、GPU 证据、停止条件和恢复步骤见 [引擎渲染路径验证与回退](engine-render-path-validation-2026-10-16.md)。当前 DLL 尚未实现该候选路径或其诊断开关。

## 修复对象

原路径直接向一张 RGBA8 颜色纹理绘制，再合成到界面。它已经可能经过 CS 的全局材质 hook，不能将问题描述成“完全绕过 CS”；但它缺少独立的材质缓冲、散射、材质合成和完整视图上下文。仅覆盖 `pass.sceneLights` 也不足以改变 LLF 对“当前累加器”的判断，世界光源表可能与这组三盏摄影棚灯不一致。

原有状态捕获只保存 PS 的三个资源槽和一个采样器，也不足以保护 CS 使用的高槽资源及计算阶段。

UI 累加器若仍指向世界节点，视图会在绘制窗口内借用一个已初始化的非世界 ShadowSceneNode，再恢复原指针。没有合适节点时回退并记录原因，不把面板误判成世界渲染。

## 当前流程

1. 在绘制窗口内设置 UI 累加器，调用引擎的菜单相机设置；关闭该窗口的 TAA，使用独立的全尺寸视图常量、环境光和灯光参数。
2. 不透明及镂空的 Lighting 材质进入 CS 的 deferred 变体，输出独立的八张材质目标。保留原来的 alpha 属性与 `0x204` 提交标志。
3. 使用 CS 自己的 DiffuseExtraction、Separable 或 Burley 计算着色器，读取面板的漫反射、深度、法线、反照率和 SSS mask。核、配置、散射模式与人类／兽族区分来自当前 CS 设置；源 Actor 仅用于皮肤类型分类，不重新挂回复制图的 userData。
4. 调用 CS 的原生室内材质合成着色器。漫反射、高光及材质反射按 CS 的公式合成；环境反射使用摄影棚自己的常量立方体与固定环境光。SSGI 输入为中性值，不启动世界 SSGI、动态立方体更新或其他历史更新。
5. 根据 Linear Lighting 设置，转换为 CS Lighting 菜单 forward 分支的输出编码，再绘制其他不透明材质与排序后的透明材质。透明阶段使用只读深度视图，同时允许采样该深度。
6. 退出时恢复 CS 的描述符、常量缓冲包装器、LLF 灯光缓存、帧缓存、原生 frame buffer 指针、TAA、相机与引擎 shadow state。外层 D3D 捕获恢复设备绑定。

计算阶段还有自己的 D3D 捕获：临时解除纹理绑定后必须恢复原来的材质纹理，否则后续透明 pass 的材质缓存可能仍认为纹理已经绑定。

摄影棚方向光不注册到世界场景。保留原有非阴影灯壳，在每个 pass 内临时为第 0 槽提供真正的 NiDirectionalLight，随后恢复灯壳；第 1、2 槽用于主光、补光。这样既符合 native Lighting / LLF 对第 0 槽的约定，也避免替换世界太阳。

## 隔离边界

- 主颜色为 RGBA16F，可供计算阶段读写；深度为 R32 typeless，分别提供 D32 DSV、只读 DSV 和 R32 SRV。
- 八张材质目标、SSS 临时纹理、合成输出与法线／运动输出均由面板持有。分辨率或设备变化后重建。
- 视图常量与 LLF strict-light 常量使用面板自己的 GPU 缓冲；不把摄影棚灯写入世界的聚类光源表。
- 每次 native dirty-state 更新后重新应用独立的视图常量，避免引擎再次写入世界动态分辨率比例。原 frame buffer 另有 GPU 备份，退出时连同内容一起恢复。
- VS / PS / CS 捕获全部 128 个资源槽、16 个采样器及 14 个常量缓冲槽；支持 D3D11.1 时保留缓冲子范围。另捕获 CS shader、实例与八个计算 UAV，恢复前解除潜在读写冲突。
- 世界时域效果、景深、动态天气光照和世界环境贴图不参与这个摄影棚。CS 的材质算法与摄影棚的相机／光照是两个不同的层次；这里不承诺面板与某一世界位置具有相同亮度。
- ENB 没有在本轮接入独立材质流程。未知 CS 二进制和关闭 CS 的情形使用 forward 回退。

当前私有纹理与窗口分辨率相同，按约 76 字节／像素预留：1080p 约 150 MiB，4K 约 601 MiB，另有驱动开销。纹理在关闭面板后保留以供重用。后续若需降低开销，应单独调整摄影棚分辨率／取景契约，不能让 CS 仍读取世界分辨率。

最终界面合成不再附加一个任意的 Reinhard tone map。HDR Display 的最终输出位置、其他图像空间效果与实际安装组合仍须在游戏里检查，本轮没有声称复制全部世界后处理。

## 版本与维护

使用 [Nexus 主文件](https://www.nexusmods.com/skyrimspecialedition/mods/86492?tab=files)对应的正式版 **1.9.1**；核对材料为 [官方 v1.9.1 发布包](https://github.com/community-shaders/skyrim-community-shaders/releases/tag/v1.9.1) 的 `CommunityShaders-2026-09-24T12-32Z.zip`，Windows 文件版本 1.9.1.0。没有使用 GitHub PR 预发布版，也没有以 main 分支的布局替代此 DLL。

| 项目 | 已核对值 |
| --- | --- |
| DLL SHA-256 | `10ba95e6b1cd6b675ce74d1d681fae41e667bdd8b7b5ba8c362e657511097827` |
| PDB SHA-256 | `1764229a27a01ae91eb1abb5bf092e8da1c869bd69c378d7b96435b1fc15c1ce` |
| PE timestamp / SizeOfImage | `0x6AB51A9F` / `0x144B000` |
| CodeView GUID / age | `240F7D4A-CB53-466A-B55C-6BCF3E723338` / `1` |
| State / Deferred 大小 | `0x320` / `0x1700` |
| Shared / Feature / Frame 常量大小 | `0x2A0` / `0x510` / `0x2D0` |
| SSS / strict-light 常量大小 | `0x2F0` / `0x4C0` |

[binary_profile.h](../src/render/community_shaders/binary_profile.h) 固定所有调用入口 RVA、入口前 16 字节和全局符号；[bridge.cpp](../src/render/community_shaders/bridge.cpp) 在安装前验证 PE、CodeView 和全部入口字节，只 detour `State::Draw`。未处于本线程预览作用域时只执行原函数。

| 原生函数 | RVA |
| --- | --- |
| State::Draw | `0x5E2790` |
| GetFeatureBufferData | `0x3A6820` |
| SSS prepass / horizontal / vertical / Burley getter | `0x4CDC90` / `0x4CDB40` / `0x4CDA20` / `0x4CD8D0` |
| Deferred::OverrideBlendStates / ResetBlendStates | `0x3A0000` / `0x39F990` |
| ShaderCache::IsEnabled / GetPixelShader | `0x5D9990` / `0x5D5AD0` |
| HDRDisplay::GetSharedDataHDR | `0x4554A0` |
| Deferred::GetComputeMainCompositeInterior | `0x39EA80` |

布局通过配套 PDB 和 [v1.9.1 源码](https://github.com/community-shaders/skyrim-community-shaders/tree/v1.9.1) 对照；[gpu_data.h](../src/render/community_shaders/gpu_data.h) 用大小及偏移断言约束 GPU 数据。HDR getter 按 MSVC 成员函数的隐藏返回参数调用（RCX=this、RDX=输出地址），不能转换成一个返回 Vector4 的普通自由函数指针。

上游更新后，先核对新 DLL/PDB、函数签名、GPU 布局及 HLSL 编码，再增加 profile。不能只改版本号、跳过字节验证或扫描一个相似函数地址。

## 本地验证

```powershell
cmake --preset Release -DCHARACTER_PANEL_SCENE_COPY_EXPERIMENT=ON -DCHARACTER_PANEL_PROBE_REVISION=cs-view1
cmake --build build --config Release --target CharacterPanel CharacterPanelCopyMathTest CharacterPanelRenderTest
ctest --test-dir build -C Release -R '^(snapshot_transform|render_state)$' --output-on-failure
build/Release/CharacterPanelRenderTest.exe --cs-shaders <官方1.9.1包内的Shaders目录>
cmake --build build --config Release --target CharacterPanelSceneCopyPackage
```

已有 31 项 snapshot 检查，以及新增五项 WARP／profile 检查通过。使用官方包的实际 HLSL 另行编译和执行：八项 SSS 案例覆盖两种算法、Linear Lighting 开关、常量与混合 mask；八项材质合成案例覆盖 Linear Lighting、环境反射与中性 SSGI 输入。这些验证不装载或执行 CommunityShaders.dll。

SSS 案例检查无散射材质不被改写、反照率去除／恢复正确、可分离散射实际改变皮肤像素；材质案例检查原生 diffuse/specular/reflection 的数值结果。GPU 数据布局的 C++ 断言随插件及测试一起构建。

## 游戏检查与日志

测试包为 `dist/CharacterPanel-scopy-cs-view1-1.2.1.zip`，不包含 CS。安装对应 CS 1.9.1 后检查：

本轮包内 `build-manifest.json` 的身份为 `CharacterPanel-scopy-cs-view1-cdb5afebf1+dirty-cl94faaed0c6-20261016T110744Z`，源码基线 `cdb5afebf1219773e7c7a0afdae963abec6c2eec`，标记有未提交修改；CommonLib 基线 `94faaed0c60eddd8347767f2d4d29a97c93bde8c` 且工作树干净。压缩包内 DLL/PDB 已逐字节对照最终构建产物。

| 产物 | SHA-256 |
| --- | --- |
| ZIP | `b472344a22f4f89987b3cfb30a3b2831d5a41362897a651d9704da5f3fc4c59f` |
| CharacterPanel.dll | `784b58e977211cec943c0a43b4ae5fced6d965530f04663a206f0685920a92dc` |
| CharacterPanel.pdb | `470079cf03b6ba813781f03f1bf49a824c3c6be06083f1691d794d8f36e44289` |

1. 打开背包，确认日志先出现 `CP-CS 1.9.1 verified`，随后出现 `CP-CS private view` 和 `CP-CS material view`。只有第一条表示 hook 已安装，不表示材质视图已运行。
2. 对 UBE 面部、眼影、眉毛、头发及换头部装备检查透明度；确认两种 SSS 模式和 Linear Lighting 开关都可用。
3. 对金属武器和皮肤高光检查摄影棚反射；室内／室外及天气变化后再次打开背包，核对固定摄影棚光照。
4. 快速切换武器、转动预览，再关闭背包；世界人物、太阳、后处理和下一帧渲染应正常。
5. 若启用 HDR Display，核对最终 UI 与面板亮度、色彩及图像空间效果的插入位置。

遇到未完成的 deferred shader 编译，会清除本帧的部分材质结果并重绘 forward，下一帧重试；不会把部分 GBuffer 当作完整结果合成。`CP-CS refused` 表示版本身份／入口不匹配。请保留完整 `CharacterPanel.log` 和测试包 manifest，便于区分版本拒绝、资源未就绪、着色器等待及游戏内画面问题。
