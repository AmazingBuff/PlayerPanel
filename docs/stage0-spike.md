# Stage-0 spike：游戏内验证清单与标定结果

状态：DLL 已构建并部署（2026-09-27，`build/Release/CharacterPanel.dll` → 游戏目录 `Data/SKSE/Plugins/CharacterPanel.dll`），游戏内验证 **未执行**（需要真人跑一次游戏）。

## 本 spike 回答的三个问题

1. **函数同一性**：`BSBatchRenderer::SetupAndDrawPass`（RELOCATION_ID(100854, 107644)，CommonLibSSE-NG v9.1.0 声明）是否就是社区所称的 `RenderPassImmediately`（Community Shaders 三个钩子点的调用目标）？
2. **pass 形态标定**：克隆 actor 的身体/头部几何在正常渲染中，引擎实际使用的 `technique` / `passEnum` / `numLights` / `numShadowLights` / shaderType 各是什么？（替代原计划的 RenderDoc 人工标定）
3. **核心赌注（Stage-0 gate）**：在渲染线程上，把引擎当前渲染目标换成插件自建离屏 RT 后，用原函数重放（replay）同一个 `BSRenderPass`，目标里能否出现**正确着色**（皮肤 tint 正确、非灰模）的几何体？

静态反汇编路线的说明：本机 SkyrimSE.exe（1.6.1170）带 Steam DRM（`.text` 熵 8.00、无 RTTI 字符串、入口为解壳 stub），静态分析不可行；故同一性问题改为运行时验证（钩子点即调用点，钩子生效本身 + 落盘结果共同证明）。

## 游戏内操作步骤

前置：正常加载一个室外存档（人物全身可见、不穿披风斗篷类重物理装备，减少干扰）。

1. 启动 skse64_loader 进入游戏，读档。
2. 查看日志 `Documents/My Games/Skyrim Special Edition/SKSE/CharacterPanel.log`：
   - 期望出现 `pass hook 0/1/2 installed at 0x...`（三个钩子点全部装上；若游戏加载时 SKSE 报错/闪退，先记录崩溃时日志最后一行）。
3. 按 **F7**：玩家面前约 120 单位处应出现玩家的克隆（穿同样的装备，面向玩家，站着不动）。
   - 日志期望：`[spike] clone spawned at (x, y, z)`，随后一帧内 `[spike] collected N geometries from the clone 3D`（N 应为两位数，含身体/头/手/脚/装备）。
   - 若无 `collected` 行：等待几秒再按 F8（3D 加载有延迟）。
4. 按 **F8**：标定武装。下一帧引擎渲染克隆的任意 pass 流经钩子时触发：
   - 日志期望一串 `[spike] clone pass: geometry=... technique=0x... passEnum=0x... ...`（每个克隆几何一条）；
   - 随后 `[spike] dumped replay_000.tga (...)`；
   - 目录 `Data/SKSE/Plugins/CharacterPanelSpike/`（相对游戏根）出现 `replay_000.tga`。
5. 再按几次 F8 可多次采样（文件序号递增；每按一次落一张）。
6. 按 **F7** 销毁克隆（日志 `clone despawned`）；再按 F7/F8 循环几遍，确认无泄漏、无崩溃。
7. 存档/读档后克隆应自动销毁（`kPreLoadGame` → request_despawn）。

## 判读标准（gate）

- **过 gate**：`replay_000.tga` 中能看出克隆的某件几何（大概率是身体或衣服）形状，且**颜色正确**——布料/皮革是本色，皮肤是肤色（不是纯灰、不是纯剪影）。这证明"引擎 pass + 自建 RT 重放"闭环成立，Stage 1（面板原型）开工。
- **部分过**：形状在但颜色错（如全灰、纯黑、tint 丢失）——记录 TGA 与日志，问题定位到"重放时引擎状态不自洽"（常量缓冲/technique 栈在帧中段被我们打断），Stage 0 加一条"挂帧内调用点重放"的对照实验再判。
- **不过**：TGA 是纯清屏色（0x0D0D14）或崩溃——说明帧外/换目标重放不成立，**停下与用户重新决策**（含备选路线：克隆放世界 + 相机取景），不静默降级。

## 日志字段速查

| 字段 | 含义 |
|---|---|
| `identity: SetupAndDrawPass resolves to 0x...` | 函数地址（与钩子安装地址对照可确认调用链） |
| `technique=0x...` | 该 pass 的 technique 参数（传给绘制函数的第二个参数） |
| `passEnum=0x...` | pass 结构体里的 passEnum（材质特征位编码；BSLightingShader 的 kTechniqueIDBase 见 `RE/B/BSLightingShader.h`） |
| `numLights` / `numShadowLights` | 该 pass 引用的灯数量形态 |
| `shaderType` | BSShader::Type（Lighting=6 族、FaceGen 等） |

## 已知设计取舍（spike 专用，不带入正式实现）

- 克隆故意**不** AppCulled——它必须被引擎正常渲染，pass 才会流经钩子。
- 重放发生在帧中段（钩子点内），当前渲染目标被短暂换掉再还原；若引擎对该目标有中间状态假设，可能造成一帧花屏/闪烁——spike 可接受，正式实现要挂更安全的时点。
- `m_originals[3]` 成员目前未使用（保留的接口位）；同一性结论来自"钩子生效 + 重放出图"的联合证明。
- TGA dump 在渲染线程做 Map/读回，会有一次卡顿——spike 一次性操作，可接受。
