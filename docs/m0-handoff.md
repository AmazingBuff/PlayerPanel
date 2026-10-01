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

**v3.1 已实现并经 run 19 验证通过**(2026-10-01,见 m0-proto.md 的 v3 /
run 17 / run 18 / run 19 段):F7 面板开关 + 常驻括号 + 常驻
OffscreenTarget(FR-06 生命周期)+ 关面板自动取证 + F8 可选中途抓帧 +
日志节流。run 19 证据:4 次关面板各出一张 TGA(proto-pass-048–050、052),
F8 中途抓帧出 051,28.5 秒 + 52 秒持续浸泡稳定,单帧 8 几何 cluster 重放
正常(lighting_replayed=8),换 6 种物品重放跟随;目视判读 048(铁盾)/
050(放大盾面)/052(钢甲胸甲)材质、光照、轮廓全部正确;热键已从
F6/F7 移到 F7/F8(F6 与用户游戏内绑定冲突)。**工作包 1+2 就此关闭,
当前目标 = 工作包 3(合成)**。

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
| `tools/m0_proto/proto_passredirect.cpp` | 全部核心(v3.1):PassRedirector(install 三调用点钩子 + 常驻 begin/end_frame 括号 + on_pass 识别[每面板一次发现式日志] + `replay_after_original` 重放)、OffscreenTarget(私有 color+depth+depth-on 状态,常驻、desc 变化重建、FR-06 渲染线程释放)、DrawInterfaceStart Detours detour、关面板取证/F8 一次性 TGA 读回 |
| `tools/m0_proto/proto_main.cpp` | SKSE 导出、运行时门禁、输入 sink(F7 开关面板 / F8 一次性导出)、消息处理(kDataLoaded 安装;kPreLoadGame/kNewGame 强制关面板) |
| `tools/m0_proto/proto.h` | Proto 类接口(面板开关/导出/失效/代数) |
| `tools/m0_probe/` | 探针(F7/F8 抓取),已完成使命,保留作证据工具 |
| `src/` | 旧 spike(pass_hook/clone_actor),pass 钩子机制的出处 |

## 下一工作包:M0 面板原型(拆分与进度)

1. ~~**常驻括号**~~ **完成(run 19 验证)**:F7 面板开关 + 面板开启期间每
   菜单帧跑 begin_frame/original/end_frame;FR-05 输入消费规则保持(只观察
   F7/F8,拒绝按住/重复)。
2. ~~**常驻摄影棚目标**~~ **完成(run 19 验证)**:OffscreenTarget 面板生命
   周期持有;desc 变化自动重建(分辨率变化)、关闭/读档/新游戏渲染线程释放
   (FR-06);创建失败按配置签名粘性化,不逐帧重试。
3. **合成**(当前目标):把摄影棚结果作为不透明矩形合成到世界画面上、
   其他 UI 之前(PRD §2)。候选接入点:世界后处理之后 / `DrawInterfaceStart`
   之前(该 detour 已验证可靠);需要调查与已有 hook 链(CS/ENB/ReShade)的
   相对顺序,这是 §7 问题 2 的正题。
4. **暂停场景**:背包暂停世界时 P 的更新路径(§7 问题 4),M0 先固定姿态。
5. **验收**:先过 A01(世界正常 + 不透明面板 + 下一帧无污染),再 A02/A03。
   记录开面板前后帧耗时/显存(PRD §5.3)。

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

## 未决事项

- `extern/CommonLibSSE` submodule 处于 dirty 状态(历史遗留,未处理,勿
  随意 stash/reset;提交时永远排除)。
- 原版渲染器(无 CS)场景未单独跑过(finding 3 表明低价值,可继续搁置)。
- Culler 真实布局(+0x140/+0x250/+0x3D5/+0x3F4)仅在回头走 accumulator
  路线时才需要,继续搁置。
