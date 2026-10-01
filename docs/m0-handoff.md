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

**v4.6 经 run 26 验证:面板首次上屏,工作包 3(合成)完成**(2026-10-01,
用户 RenderDoc 定位 blend 问题后修复;截图与日志见 m0-proto.md run 26
段):不透明矩形精确落在 58–88% × 12–68%,摄影棚画面(铁盾)在其中,世界
正常渲染;`composite draw #3600` 心跳证明整会话逐帧绘制无崩溃;3 张关面板
取证 TGA;引擎混合诊断行 `enable=0 src=2 dest=1` 留档。**M0 面板原型四
大件全部就位**:常驻括号、常驻摄影棚目标、生命周期(FR-06)、屏幕合成。
遗留(M1 打磨,非阻塞):面板目前盖住物品卡右半(卡片先画、面板后画,
§2 顺序的精确排布待做);摄影棚图整幅缩放进矩形(取景属 M1 相机工作)。
**范围修正(2026-10-01,用户决定)**:面板仅在菜单界面显示,世界画面
对照推迟——"下一步 = A01"作废,新规划见下方"下一步规划"三阶段。
**阶段 1 工程面验收通过**(逐项证据见 [m0-acceptance.md](m0-acceptance.md):
7 项中 6 项通过、1 项部分通过,开放项并入阶段 2 首轮;INI 配置用户决定
推迟);**当前阶段 = 2(独立展示实例 P)**。
DLL 已部署 `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\`
(v4.6,MD5 6f00f395…)。热键 F7/F8。v3.1 已提交(770e618);v4 系列
已提交(f15fb1a)。

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

## 下一步规划(2026-10-01 范围修正后,用户决定)

**范围修正**:面板依托菜单场景中的展示模型存在,仅在背包等非世界界面
出现;与"世界画面 + 面板"共存相关的一切(原 A01 等)整体推迟。当前实现
天然满足——重放与合成只在有菜单光照 pass 的帧发生,世界画面内不会出现
面板,无需删改代码。PRD 已升 0.5 记录此修正(§6.1)。

已完成的工作包(不再展开):常驻括号(run 19)、常驻摄影棚目标(run 19)、
屏幕合成(run 26,用户 RenderDoc 定位 blend 问题后 v4.6 修复)。

### 阶段 1:M0 工程收官——**完成**(验收记录:[m0-acceptance.md](m0-acceptance.md))

1. ~~面板矩形 INI 可配置化~~ **用户决定推迟**(2026-10-01"暂时不需要
   配置,这是后面的事");固定矩形 58–88% × 12–68% 与物品卡的避让随
   阶段 3 布局定稿一并处理。
2. ~~菜单内验收清单~~ **完成**:逐项验收与证据映射见
   [m0-acceptance.md](m0-acceptance.md)——7 项中 6 项通过、1 项部分通过
   (读档/新游戏强制关闭的触发路径,并入阶段 2 首轮);性能粗基线留档,
   正式测量推迟(PRD §5.3)。

### 阶段 2:M0 完整收官——独立展示实例 P(**当前阶段**,中风险)

按 PRD M0"可先固定姿态,但必须使用独立展示实例"——当前面板内容是被
重定向的物品预览(证明载体),还不是 P:

1. **P 实例**:克隆玩家 3D(spike 的 clone_actor 机制已验证)挂到
   UI3DSceneManager 空闲 menuObjects root(AttachChild,CLib ID 51859
   已验证);固定姿态,不接动画更新。
2. **摆位与取景**:P 根节点摆到 UI3D 相机前方合适位置,面板内满幅显示
   P(替代现在的整幅缩放);面板内容与原物品预览的并存/替换关系在此
   阶段定稿。
3. **管线自动覆盖**:P 挂在 menuObjects 之下即被祖先识别命中,走同一
   重放与合成路径,无需新机制。**最大未知:蒙皮几何过重放路径的兼容性**
   (物品是静态网格,P 是蒙皮角色;PRD FR-04 本就要求蒙皮验证)——单独
   安排验证轮,失败模式与回退方案先行写清。
4. **首轮验证顺带补两笔**(来自 m0-acceptance.md 开放项):开面板状态下
   存档+读档,确认 kPreLoadGame 触发路径的强制关闭;正式 CPU/GPU 帧耗时
   与显存测量(PRD §5.3)。
   → 产出:PRD M0 要求达成,M0 完整收官。

### 阶段 3:M1——外观与动作闭环(PRD FR-01/02/03)

1. 装备与外观同步(Apparel Preview 的装配逻辑为参考,community-reference-
   supplement.md §4);
2. 临时污染过滤(FR-02);
3. 待机动画与展示动作切换(FR-03;暂停场景的更新入口 = §7 问题 4);
4. 面板布局定稿(**需要用户输入**:P 的位置/大小/与物品卡及后续网格
   背包的关系)。
   → 产出:面板内 P 外观正确、动作独立、换装同步。

### 决策点与风险

- 蒙皮重放兼容性(阶段 2 最大未知,先行单独验证);
- 面板布局需用户参与设计(阶段 3 开工前);
- modded UI 遮挡关系:run 26 截图记录了面板盖住卡片右半的现状,阶段 1
  的 INI 调位解决;
- 世界画面对照(原 A01/A02 的相机部分)推迟,何时恢复由用户决定。

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
