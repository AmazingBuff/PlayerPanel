# v128 · DAC 驱散修正上机判读（2026-11-26）

> 本文按部署时 [MANUAL](../../dist/v128-dac-dispel-20261126/MANUAL.md) 的操作执行。**判据在本文第一节被修订**，修订理由与影响一并记录，不改写原 MANUAL 文本。

## 一、会话身份

| 项 | 值 |
| --- | --- |
| 日期 | 2026-11-26 22:32:00–22:34:26 |
| 构建 | `CharacterPanel-scenegraph-v128-dac-dispel-20261126-3e61d42b61-cl94faaed0c6-20261126T142302Z`（干净树 HEAD `3e61d42`） |
| DLL / PDB | `164EACEA…2BBD` / `8ADC5234…AFA` |
| INI | `91ACF06F…`（`DetailedAnimationTrace=true`，`DisplayPose=false`） |
| 日志 | [CharacterPanel-v128-dispel-20261126.log](../CharacterPanel-v128-dispel-20261126.log) 3,027,659 B，SHA-256 `5b3aac23f73bcd1ab715920836950e80a73f0f0e3a05352d75f967f2dbb0e354` |
| 载入身份核对 | 日志首行 `SCENEGRAPH BUILD` 与本表逐字符一致 → 本轮确实跑的是 v128 |
| 工具链 | **首次以 VS 2026 / v145 / CMake 4.3.1 构建的产物上机**（v126/v127 为 VS 2022 / v143） |

## 二、判据修订：`cause=effect-correction-change` 不能作为判据

MANUAL 原文要求"必须出现至少一行 `DAC OBSERVED ... cause=effect-correction-change`"。**本轮该行出现 0 次**（26 行 `DAC OBSERVED` 中 25 行 `effect-signature-change`、1 行 `native-dispatch-no-revision-change`）。经代码复核，这是**判据设计错误**，不是修正未生效：

[effects.cpp:154-176](../../../src/animation/dac/effects.cpp) 里 `signature` **只**由 `managed && !dispelled` 的效果计算：

```cpp
const bool dispelled = active->flags.any(RE::ActiveEffect::Flag::kDispelled);
presence.push_back({ effect, managed, dispelled, inactive });
if (managed && !dispelled) { signature = hash_combine(...); }
```

而 `corrections` 的**存在前提**正是一个 managed 效果处于 `dispelled` 状态（[effect_presence.h:33](../../../src/animation/dac/effect_presence.h)：`none_of(managed && dispelled)` 时直接返回空）。因此：

- **一个 managed 效果被驱散的那一刻**，它退出 signature，同时新进入 corrections → `signature_changed` 必为 true → 打印 `effect-signature-change`。
- 此后 corrections 已经稳定；只要它不再变化，两轮之间就没有新行（`session.cpp:108` 提前返回）。

所以 `cause=effect-correction-change` **不是不可达的死分支**——当已被驱散的条目数量或 inactive 状态变化、而 signature 不变时它确实会打印。问题在于**它永远赶不上"一个效果刚被驱散"这个关键过渡**：那一步一定报 `effect-signature-change`。

**结论：`cause` 字段对"修正是否生效"没有鉴别力。** 修正生效的必要前提是 corrections 非空，而 corrections 从空变为非空的那一次观测必然被 signature 变化掩盖。MANUAL 把该字段当作必要判据是设计错误。

> 附注：本节初稿曾写"逻辑上不可达的死分支"，复核后收窄为上述表述——`effect-correction-change` 本身可达，只是不可用于判定修正是否被应用。

**可靠判据改为**：`ANIMATION PANEL` 的 `accepted` 选择序列——Start 是否真的过渡到 Loop。这是修正的**目标行为**，且不依赖任何诊断字段。

## 三、主结果：8/8 可判定武器族全部 Start → Loop

按 `request` 分组的 idle 选择序列（完整 27 行见日志）：

| req | 武器族 | 选择序列 | 判定 |
| ---: | --- | --- | --- |
| 1 | 1hm | `Loop` | 进入即 Loop（无 Start 段） |
| 2 | **2hm** | `Equip → Start → Loop` | **Start → Loop** |
| 3 | **1hm（换装后）** | `Equip → Start → Loop` | **Start → Loop** |
| 4 | **2hw Axe** | `Equip → Start → Loop` | **Start → Loop** |
| 5 | Bow | `Gunslicer equip → Start → Loop` | **Start → Loop** |
| 6 | Dagger | `（无 equip 行）→ Start → Loop` | **Start → Loop** |
| 7 | Dagger | `（无 equip 行）→ Start → Loop` | **Start → Loop** |
| 8 | Axe | `Gunslicer Sword equip → Start → Loop` | **Start → Loop** |
| 9 | （法杖） | `staffright equip → 空 logical` | **不可判定**（见 §五） |
| 10 | Bow（重复） | `Gunslicer equip → Start → Loop` | **Start → Loop** |

**关键对照**：req 2（2hm）、req 4（2hw Axe）、req 3（换装后 1hm）正是 [v127](../idle-loop-v127-session-20261124/README.md) 记录为"连续驻留 `* idle Start`"的三个武器族。本轮三者全部落到 Loop。

## 四、这一步证明了什么、没证明什么

### 4.1 已证明

1. **在 v128 上，"卡 Start"现象不再复现。** 5 个武器族（8 次换装）全部完成 Start → Loop，包含 v127 的三个失败族。
2. **修正代码路径确实被执行。** `session->effect_corrections(...)` → `preview.effect_corrections` → [hooks.cpp:501-518](../../../src/animation/oar/hooks.cpp) 的 `HasMagicEffect` 分支在每次预览查询都会求值；`DisplayPose=false` 保证走该路径（本轮 `reason=boundary` 30 次，v127 为 36 次，同一量级，说明合成路径一致启用）。
3. **构建环境迁移没有破坏渲染或动画。** VS 2026 / v145 产物在 2 分 26 秒会话内无崩溃、无渲染异常，27 次面板选择全部正常返回。

### 4.2 没有证明（不得外推）

1. **没有直接观测到"某个驱散条目被排除"这一瞬间。** 修正没有独立日志点，`cause` 字段又是死分支（§二）。因此**"Start→Loop 的改善由这次修正导致"是相关性推断，不是直接因果**。存在其他解释：DAC 的每次换装都会重建效果集（`DAC snapshot`→多轮 `effect-signature-change`），而本轮换装节奏与 v127 不同，OAR 在同一标记状态下的选择本身可能就对时序敏感。
2. **`both` 状态下的 OAR 上下文分歧仍未解释。** [C-WORLD v1](../world-phase-20261125/README.md) 证明世界与面板的 `801` 状态相同；本轮标记序列与 v127 **逐位相同**（`243801` 依旧常驻），但选择结果不同。这说明分歧可能比"驱散条目被误计入"更深——但本轮无法排除是会话间随机性或换装时序差异。
3. **法杖（req 9）不可判定**，因其 idle 走空 `logical`（直连原版 `staffright_idle.hkx`），不在 Smooth 域内。
4. 不改变 G1–G6 任何已签收结论。

### 4.3 若要闭合因果，需要的最小取证

修正分支目前**没有任何日志**。最小改动是在 [hooks.cpp](../../../src/animation/oar/hooks.cpp) 的 `value = false` 处加一行受 `DetailedAnimationTrace` 门控的记录（效果 FormID + `all_effects`/`active_only`）。下一次会话若出现该行且 Start→Loop 仍成立，因果链才闭合。这是下一轮的候选工作包，**本轮不做**。

## 五、判据与边界汇总

| 项 | 结果 |
| --- | --- |
| MANUAL 原判据 `cause=effect-correction-change` | **0 次 → 判据作废**（§二：无鉴别力，非修正失败） |
| 替代判据：Start → Loop | **8/8 可判定族成立** |
| v127 三个失败族是否复现 | **未复现**（req 2 / 4 / 3 全部落 Loop） |
| 法杖族 | 不可判定（空 logical） |
| 直接因果证据 | **缺**（§4.2.1） |
| 构建环境迁移 | 无崩溃、无渲染异常；首次上机成立 |
| 会话时长 | 2 分 26 秒；10 个 request；27 次面板选择 |
| G1–G6 | 不受影响 |

## 六、部署与回退

安装位现为 v128（DLL `164EACEA…` / PDB `8ADC5234…` / INI `91ACF06F…`），上机后逐位复核未变。

> **回退备份已丢失（2026-11-26 收尾时发现）**：部署前写入 `dist/v128-dac-dispel-20261126/` 的 v126 三件套（DLL `55DAF91B…` / PDB `FB6E52E9…` / INI `ECB2F912…`）连同整个 `dist/` 目录在本轮会话之后消失，`$Recycle.Bin` 无对应记录。`/dist/` 被 [.gitignore](../../../.gitignore) 排除，**git 无法恢复这份备份**。
>
> 实际损失有限：
> - **当前无需回退**——v128 运行正常（本轮 2 分 26 秒无崩溃、无渲染异常）。
> - v126 的原始构建产物可由提交 `e437cc2`（源码 `20261125T143400Z` 身份）重新构建，只是**字节不等价**（工具链已换成 VS 2026 / v145），且需重新走一次 configure/build。
> - 已从安装位把 v128 的 DLL 与验证档 INI 重新归档到 `dist/v128-dac-dispel-20261126/`（`CharacterPanel-v128-deployed.dll` `164EACEA…`、`CharacterPanel-dispel-verify.ini` `91ACF06F…`），PDB 因体积未复制。
>
> 教训：`dist/` 不是持久存储。需要长期保留的回退档应放进仓库内（如 `docs/diagnostics/` 或专门的 `backups/`），或至少在 `dist/` 之外另存一份。

**验证档收尾建议**：本轮判据已满足，`DetailedAnimationTrace=true` 可以关回 `false`（常态档），除非下一轮要做 §4.3 的日志取证。

> **`build/Release/` 现状与部署物不一致**：部署时记录的产物是 `164EACEA…`（label `v128-dac-dispel-20261126`）。收尾检查发现 `build/Release/CharacterPanel.dll` 现为 `9736d324…`、2,191,872 B、22:37 生成、身份 `…-scenegraph-scenegraph-b8cd985b7a-…-20261126T143100Z`（默认 label，非本轮的 v128 label），**不是**本轮的构建产物，也未被部署。本轮部署的 `164EACEA…` 只保留在安装位与 `dist/` 归档中。

## 七、变更记录

- 2026-11-26：首次运行与判读。**修正 MANUAL 的判据**——`cause=effect-correction-change` 经代码复核对"修正是否生效"无鉴别力（§二），改用 `ANIMATION PANEL` 选择序列；主结果 8/8 族 Start→Loop，v127 三个失败族全部不复现；直接因果证据仍缺，记录最小取证方案。同日收尾发现 `dist/` 目录丢失，回退备份不可恢复，事实与影响见 §六。
