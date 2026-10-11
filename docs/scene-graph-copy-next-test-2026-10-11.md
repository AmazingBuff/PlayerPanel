# HKX34 测试单（2026-10-11，修复 HKX33 实测暴露的面板黑屏与物品卡泄漏）

- 构建身份：`CharacterPanel-scopy-HKX34-99696da5b2+dirty-cl94faaed0c6-20261011T082733Z`
  （`+dirty` = 基线 `99696da5b2` 之上含三处未提交改动，见下文"本轮内容"）。
- 包：`dist/CharacterPanel-scopy-HKX34-1.2.1.zip`，DLL SHA-256 `bacdd76e…`，
  **DLL 与 PDB 均已从最终 zip 部署到 `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\` 并校验一致**。
- 单测 1/1（`snapshot_transform`）。
- 上一轮判读（HKX33 首次上机）：[本轮实测判读](#hkx33-首次上机判读2026-10-11-1602)。

## HKX33 首次上机判读（2026-10-11 16:02）

**先更正一件事**：本轮上机的是**已提交的 HKX33 包**（日志 `SCOPY BUILD …-HKX33-9bd674656d-20261010T190021Z`），
**工作区里那份 activeNodes 改动从未上机**。卡顿消失 = HKX33 的墙钟限速已生效。

| 症状 | 判读 |
| --- | --- |
| 正常游玩不再卡顿 | **通过**（限速生效） |
| 面板内无人物（纯黑矩形） | **回归**，根因见下 |
| 物品卡左侧出现特效/眼影小垃圾 | **同一根因的另一面**，见下 |
| idle 未应用（F2 关后依然黑） | 说明与动画写入无关，绘制目标层问题 |

### 根因链（日志 + 几何普查 + 截图三方互证）

1. **本轮存档与 HKX28 里程碑存档不同**：本轮克隆图 54 几何 / 49 蒙皮 / 908 矩阵，
   且带着**约 50 个 `SuperSprayBreath` 粒子系统 + 一批特效网格**（`Circle01/02/04`、
   `GeoSphere01`、`Tube01`、`EmitterArea`、`Object03`……）；HKX28 那轮是 23/23 纯蒙皮、
   **零特效网格**。
2. **特效网格逃过了剪枝**：`is_effect_object()` 按名字 token（Blood/Flash/Wisps/SuperSpray/…）
   和粒子类型剪枝，`Circle*`/`GeoSphere*`/`Tube*` 等名字族不命中、又不是粒子类型 → 全部留在克隆图里。
3. **泄漏物与截图对上**：红圈里的紫色光环 = `Circle02`，金色小件 = `GeoSphere01`/`Object03` 类，
   深色小块 = `00UBE_FemaleEyesHuman_shadows`（眼影，facegen 动态 TriShape，克隆时共享了源角色的
   设备缓冲——49 个提交 pass 里恰好 4 个 `rd` 非空）。
4. **机制（v6.45 机制类）**：绘制窗口内对特效几何调 `GetRenderPasses(…, UI3D accumulator)` 是窗口里
   除 `SetupAndDrawPass` 外唯一的引擎代码；特效 pass 走 accumulator 的特效桶，会把
   `DIRTY_RENDERTARGET` 置位 → 后续所有 `SetupAndDrawPass` **重应用引擎账本目标 kFRAMEBUFFER**
   （= UI 合成缓冲）而不是摄影棚离屏目标。结果：①摄影棚目标只被清屏一次 → 合成出面板块纯黑；
   ②克隆 pass 画进 UI 缓冲 → 以 UI3D 相机投影出现在屏幕中心 = 物品卡位置（锚点 = 视轴 485 → NDC(0,0)
   = 屏幕中心）；③45 个 `rd-null` 的身体 pass 被分桶机制吞掉（run 48 同款），4 个共享缓冲的
   face/特效片画了出来 = 用户看到的垃圾。
5. HKX28 之所以正常：那个存档的克隆图没有特效网格，窗口内没人翻脏位。

### HKX34 的三处改动（对应修复 + 裁决仪器）

1. **`is_effect_object()` 补着色器属性类型判据**（`character_clone.cpp`）：几何的 shaderProperty
   RTTI 含 `EffectShader` 一律剪掉——按类不按名，`Circle`/`GeoSphere`/`Tube` 全族一网打尽；
   蒙皮身体/护甲/facegen（BSLightingShaderProperty 系）不受影响。
2. **`draw()` 一次性 OM 绑定回读**（`character_clone.cpp`）：首个有 pass 的帧，提交完成后读回
   上下文实际绑定的 RTV，与摄影棚 RTV 比对，打一行
   `Clone draw OM check: bound=… studio=… match=true|false`——**这行是裁决仪器**：
   若剪枝后仍黑屏，`match=false` = 路由仍被翻（找下一个翻脏位者）；`match=true` = 路由没翻、
   是别的环节没画。
3. **（已在工作区的）activeNodes 池子采样**（`animation_source.{h,cpp}`、`scene_graph_copy.{h,cpp}`）：
   `collect_running_clip_generators()` 直接读引擎自己的 `behavior->activeNodes`（全类型、每条目一次
   守卫读），替代 HKX32/33 的"发现遍历 + 缓存 + 限速"——零窗口扫描、动作切换零成本、
   不再有 3 秒覆盖率缺口。**这是卡顿修复的终态，但本轮才第一次上机。**

## 上机步骤

1. 确认部署：`E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\CharacterPanel.dll` SHA-256
   `bacdd76e…`（PDB 同包已同步）。
2. 进存档（就用本轮这个有特效的重度存档，它是复现现场），等日志横幅出现
   `SCOPY BUILD …-HKX34-99696da5b2+dirty-…`。
3. 正常游玩 1–2 分钟，做几次动作切换（跑/走/拔武器）——**确认不卡顿**（activeNodes 采样首测）。
4. 开背包 → **高亮 Anonymous Letter（无 3D 模型的笔记，复现本轮条件）** → F7 捕获 → F8 绘制。
5. 判据见下表；面板开着时 F3 转一圈、F2 关开各一次。
6. 结束后把 `Documents/My Games/Skyrim Special Edition/SKSE/CharacterPanel.log` 复制进
   `docs/diagnostics/`（惯例名 `CharacterPanel-hkx34-<主题>-<日期>.log`）。

## 判读表

| # | 检查 | 通过判据 | 失败时看哪里 |
| --- | --- | --- | --- |
| 1 | 游玩期卡顿 | 动作切换无卡顿 | 若卡顿回归：`SCOPY ANIM pool` 行是否还在，间隔是否规律 |
| 2 | 池子采样存活 | 捕获时 `SCOPY ANIM pool at-capture samples≥1` | 全零 = activeNodes 路径没采到东西 |
| 3 | **面板人物** | 面板内出现完整人物（黑屏消失） | 看第 4 行的 OM check |
| 4 | **OM 路由裁决** | `Clone draw OM check: … match=true` | `match=false` = 还有东西翻脏位（把日志整段拿回来） |
| 5 | 物品卡垃圾 | 红圈位置无特效/眼影泄漏 | 若人物已现但垃圾仍在 = 剪枝漏了别的族（报几何名） |
| 6 | 几何普查 | capture 日志里不再出现 `Circle`/`GeoSphere`/`Tube`/`EmitterArea`/`Object03` | `SCOPY GEOMETRY` 清单即证据 |
| 7 | 零回归 | 零崩溃、零 `[E]`；F2 开关、F3 旋转、关面板取证照常 | —— |

**已知预期变化**：剪掉特效网格后，`Clone draw` 的 `geoms`/`passes` 会从 54/49 降到 ≈49/49
（只剩蒙皮件）；`ground-truth verdict` 本轮大概率仍是 `none`（活动 clip 是 MLh/MRh 魔法手 idle，
恒等映射未被验证）——**这不阻塞本轮判读**，驱动选择问题归"多动作循环"工作包处理。
