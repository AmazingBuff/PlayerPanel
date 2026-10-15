# UBE 透明材质验证：ube-alpha1

日期：2026-10-15。状态：实现完成，双配置构建和本地回归通过，游戏画面验证待反馈。
首次交付包在基线 `6e5ac99` 的工作区修改之上构建；以下记录对应这个实际交付包。

## 问题与本轮范围

UBE 角色的眉毛、眼周阴影等区域出现与 mesh 轮廓相同的黑块；复制显示图和 Actor
自身显示图都出现。两条路径共用 [figure_passes.cpp](../src/render/studio/figure_passes.cpp)。
本轮按透明材质处理缺口修复，实际黑斑是否消失以游戏画面为准。

## 修改

- 读取每个 mesh 的 `NiAlphaProperty`；alpha test 使用模型的实际开关，取消从
  `passEnum & 0x40` 推断。原材质、阈值、源／目标混合因子均交给引擎处理。
- `SetupAndDrawPass` 的 render flags 改为 `0x204`，开启 `0x4` 对应的原生
  `SetupGeometryAlphaBlending` 路径；保留原有菜单标志 `0x200`。
- 先画不透明和仅 alpha test 的表面，再按摄影棚相机方向对混合表面由远至近稳定排序。
  `No Sorter` 表面保留其在透明阶段的插入位置，其他表面不越过该边界。
- 不透明／镂空阶段的基准深度为测试并写入；混合阶段的基准深度为只测试。
  原生 shader 仍可按材质关闭深度测试或写入。剔除使用各 mesh 的双面标志。
- 每个 pass 开始前同步引擎的 viewport、深度、剔除、混合和裁剪缓存，并强制更新；退出时
  恢复这些缓存值和原有待更新标志，避免直接设置 D3D 状态后引擎误判为“已经绑定”。
- D3D 状态保存覆盖 VS／PS 全部 14 个常量缓冲槽；D3D11.1 同时保存和恢复偏移、长度。
  alpha reference 使用的槽位 `0xB` 包含在其中。退出后标脏原 alpha reference，交给下一次
  原生绘制重新上传，避免只恢复绑定而继续使用预览阈值。

先排序几何，再逐个生成并立即提交其 pass，不跨下一次 `GetRenderPasses` 保存 pass 指针。
显示源 Actor、共享材质和贴图资源均不做持久修改。

依据：[SkyrimSETest 的 BSBatchRenderer](https://github.com/Nukem9/SkyrimSETest/blob/master/skyrim64_test/src/patches/TES/BSBatchRenderer.cpp)
中 `ShaderSetup` 的 `RenderFlags & 4` 分支及原生 alpha 属性读取；
[NifTools AlphaFlags](https://github.com/niftools/nifxml/blob/develop/nif.xml) 的混合／测试／No Sorter 位；
[D3D11.1 常量缓冲范围接口](https://learn.microsoft.com/en-us/windows/win32/api/d3d11_1/nf-d3d11_1-id3d11devicecontext1-vsgetconstantbuffers1)。
本轮没有引入新的游戏函数地址或修改第三方着色器。

## 本地验证与构建

回归覆盖：透明眉毛排在脸之后、仅裁剪表面保留在实体阶段、透明层远近顺序和同深度稳定性、
No Sorter 边界、连续实体／透明绘制时的深度切换、退出后的材质缓存恢复。
另用 D3D11 WARP 离屏设备检查真实 VS／PS 高槽位、完整缓冲和带偏移子范围的恢复。
以上不代替 Skyrim、UBE 和当前 Community Shaders／ENB 环境中的实际绘制。

默认／实验两种 Release 配置构建通过；`snapshot_transform` 中 14 项检查全部通过，
包括本轮新增的排序、缓存恢复和 WARP 常量缓冲范围检查。

实验包名：`CharacterPanel-scopy-ube-alpha1-1.2.1.zip`。构建身份及 DLL／PDB 校验值见包内
manifest；测试时以日志中的 `ube-alpha1` 身份为准。

## 测试包身份

| 项目 | 首次交付包的值 |
| --- | --- |
| 构建身份 | `CharacterPanel-scopy-ube-alpha1-6e5ac99190+dirty-cl94faaed0c6-20261015T105057Z` |
| 打包时间（UTC） | `2026-10-15T10:56:33Z` |
| 目标环境 | Skyrim AE 1.6.1170 / SKSE 2.2.6 |
| 配置 | Release，`CHARACTER_PANEL_SCENE_COPY_EXPERIMENT=ON` |
| DLL SHA-256 | `66c289d7ab536d4343f76d4815b6e33a8e5dc4e35a87772f80b4a6921103936a` |
| PDB SHA-256 | `d445cd63bc48f3d1474c9883fb6a33802049fcbba88462ed469dabddf873c5c5` |
| ZIP SHA-256 | `874b0890b373659f31285e669c3957e723db9b071dbb8ec96a4183413efa082b` |

已检查压缩包内 DLL／PDB 的校验值与构建产物、manifest 一致。`+dirty` 表示打包当时包含
尚未提交的本轮修复；提交源码后仍保留原包记录，不把原包标记成由后续提交重新构建。
以后重新构建的包应记录自己的身份和校验值。

## 复现构建与本地检查

在已配置仓库工具链和依赖的 Windows 环境执行：

```powershell
cmake --preset Release -B build-scopy -DCHARACTER_PANEL_SCENE_COPY_EXPERIMENT=ON -DCHARACTER_PANEL_PROBE_REVISION=ube-alpha1
cmake --build build-scopy --config Release --target CharacterPanel CharacterPanelCopyMathTest
ctest --test-dir build-scopy -C Release -R '^snapshot_transform$' --output-on-failure -V
cmake --build build-scopy --config Release --target CharacterPanelSceneCopyPackage
```

默认配置的共用代码也需要构建检查，命令见 [维护说明](code-maintenance.md#构建控制与打包)。
WARP 检查通过系统的软件图形设备运行，不需要启动游戏，也不证明游戏 shader 已正确处理具体
UBE 材质。其他 11 项既有检查保留，新增 3 项检查的名称为 `figure transparency order`、
`figure material state`、`D3D constant buffer ranges`。

## 游戏内检查

1. 使用原来会出黑块的同一角色、妆容和装备，打开面板，近看眉毛、眼周阴影、睫毛和泪线。
2. 转动面板人物，确认透明边缘、眼球和头发没有变成缺面，也没有前后层次倒置。
3. 在背包内切换头部装备，再关闭、重新打开面板，检查新装配的材质也正常。
4. 退出面板，检查游戏人物、物品预览和界面没有新增透明／遮挡异常。
5. 如仍有黑块，保留本轮 `CharacterPanel.log` 和同角度画面；可用 RenderDoc 检查对应 mesh 的
   diffuse alpha、实际 OM blend state、alpha-test reference 与 depth write。日志中的
   `FIGURE transparency ... blended=... tested=... render-flags=0x204` 可确认新路径已执行。

判据：黑块消失、透明边缘与正常游戏中的角色一致，同时没有缺面、穿透或退出面板后的状态污染。
当前尚无本轮游戏测试日志／画面，不将这个判据记为已通过。
透明 mesh 的内部三角形排序、折射和第三方 OIT 专属流程不由本轮实现；若仍有异常，需结合
具体材质和实际绘制状态继续定位。
