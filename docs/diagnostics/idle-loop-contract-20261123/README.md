# C-IDLE v1：待机序列驻留（Start 段每圈重放）收敛契约（预注册，2026-11-23）

> **本文在实现前落盘，判据不看运行结果。** 对应产品缺陷 [Animation · idle start segment replay](../../animation.md#idle-start-replay)（2026-10-30 v46 首轮上机由操作者确认："待机会把片段的起始段每圈重放一次，看起来像起步 → 起身 → 再起步"）。该缺陷当时记为"未修，也未立契约；三个方向取舍不同，需要单独定契约后再动"——本文即该契约。

## 一、取证结论（先于契约的静态与运行证据）

### 1.1 运行证据（`docs/diagnostics/CharacterPanel-animation-repair-20261023-2253.log`，`DetailedAnimationTrace=true`）

31 次 idle 时钟边界的 `ANIMATION DECISION` 分布：

| decision | 次数 | 含义 |
| --- | ---: | --- |
| `accepted-same-asset` | 20 | 边界重查结果与当前 clip 相同 → 走同资源快路径（[controller.cpp:712-745](../../../src/animation/controller.cpp)），**不重启时钟**，仅接受 history |
| `accepted-handover` | 11 | 边界重查结果不同 → 重建 asset，执行 `clock.start(…, 0.0, false)`（[:1165-1166](../../../src/animation/controller.cpp)），**时钟归零** |
| `pending-choice` | 31 | 每次边界都先走完整 OAR 查询 |

11 次 handover 的路径全部落在 OAR 的 `Non-Combat idle Start` 变体（例：`Smooth Moveset - 1hm Shield\1hm Shield Non-Combat idle Start\1hm_idle.hkx`，index 9013 / duration 3.33 s），而同属一个 request 的后续边界则切到 `1hm Non-Combat idle Loop2\1hm_idle.hkx`（index 9041）。**同一 request 内出现 `Start → Start → Loop2` 的重复**，与"起手段每圈重放"的操作者描述一致。

### 1.2 代码机制（三步因果链）

1. **边界触发重查**（[controller.cpp:1267-1278](../../../src/animation/controller.cpp)）：`EventClock` 到 `duration` 边界时调 `boundary()`，置 `idle_due = true; reason = e_boundary`；随后 `queue_idle(source, e_boundary)` 生成新 proposal。
2. **`native_boundary` 挡不住变体序列**（[controller.cpp:648-649](../../../src/animation/controller.cpp)）：
   ```cpp
   const bool native_boundary = reason == ChoiceReason::e_boundary && active && active->stage.action.loop &&
       (!active->stage.clip.reevaluate_on_loop || continuing_sequence);
   ```
   OAR 的 `Start` 段 `reevaluate_on_loop = true`（A 组语义：作者要求每圈重算），因此 `native_boundary = false`（除非 `continuing_sequence`），落回完整 `select()` 查询。
3. **`select()` 从历史里重新选段**（[controller.cpp:589-601](../../../src/animation/controller.cpp)）→ `proposal.history.variant(*result.variants, result.clip_key)`（[:590](../../../src/animation/controller.cpp)）→ 取 `history.next` 位置的成员。接受时 `variants.commit(...)`（[:730](../../../src/animation/controller.cpp) / [:1151](../../../src/animation/controller.cpp)）把 `history.next` 推进到下一成员。

### 1.3 已被实现但**从未接线**的修复（本轮新发现，是本契约的核心依据）

`SelectionState` 里两个方法精确描述了本缺陷，且都带注释：

- [`seek_dwell()`](../../../src/animation/variant_state.h)（`:131-138`）："Landing mid-sequence … pin the clip's sequence history to the picked dwell position, so the NEXT loop recheck stays here instead of resuming the entry moves the world already played."
- [`dwell()`](../../../src/animation/variant_state.h)（`:140-155`）："Accepting a dwell pick must NOT advance the sequence … `commit()` would move `next` past it and replay the entry member on the following cycle (round 16)."

`commit()` 自身也已实现正确语义（[:110-128](../../../src/animation/variant_state.h)）："A sequence that ends on a non-once member (the usual Start → Loop authoring) DWELLS there in OAR: looping re-checks keep picking the loop body, never the entry move again."

**但 `dwell()` 与 `seek_dwell()` 在全仓库零调用点**（`grep -rn "\.dwell(\|seek_dwell(" src/` 无结果，除定义处）。控制器在所有接受路径上只调 `commit()`。因此"驻留"语义从未生效——历史里被引用的 "round 16" 修复（提交 `a00745a`，2026-10-19）**只落地了 `commit()` 的驻留分支与两个辅助方法，没有接线**。

### 1.4 机制归因（与历史记载的差异，必须显式记录）

[animation.md](../../animation.md#idle-start-replay) 原记载为"边界重入队 → 时钟从 0 起播 → 起手每圈重放一次"。取证后更精确的表述是：

- 时钟归零确实发生（`clock.start(..., 0.0, ...)`），但**它只是表现层**；
- 真正的根因是**变体序列历史被过度推进**：`commit()` 在每个边界把 `next` 推进一格，而"驻留"语义（`dwell()`）没有接线，于是经过 `Start → Loop1 → Loop2 …` 走完一圈后回到 `Start`，形成"每圈重放起手段"。
- 这也解释了为什么 20 次边界走 `accepted-same-asset`（不归零）却仍能观察到重放：重放不只在归零时发生，**在序列回卷时同样发生**。

**因此"保留时钟连续"不是本契约的修复方向**——原 [animation.md](../../animation.md#idle-start-replay) 列的三个方向里，方向二（保留时钟连续）针对的是表现而非根因，实施它会掩盖问题而不解决它。

## 二、修复方向（预注册，唯一方向）

**接线 `dwell()` / `seek_dwell()` 到正确的接受边界**，使"接受一个驻留成员不推进序列"：

| 接线点 | 调用 | 语义 |
| --- | --- | --- |
| 边界重查后接受、且新选择为**序列末尾的非 once 成员**（即 OAR 的驻留位） | `dwell(*selected.variants, selected.variant, selected.clip_key)` | 不推进 `next`，清掉该成员的 PlayOnce 标记，下一次边界重查仍选它 |
| 面板在序列中途打开（`activation`，即 panel-open / 装备切换后的首次选段） | `seek_dwell(...)` | 把 `next` 钉到当前驻留位，避免后续回卷到起手段 |

明确**不做**：

- 不改 `EventClock`（时钟归零本身是合法行为，非驻留时段的归零是正确语义）。
- 不改 `native_boundary` 判定（`reevaluate_on_loop` 是 A 组作者的显式语义，不得绕过）。
- 不改 `commit()`（其驻留分支已正确，缺的只是调用方是否该用 `dwell` 还是 `commit` 的区分）。
- 不动 `idle_loop_decision()`（[variant_state.h:18-23](../../../src/animation/variant_state.h)，引擎无关策略，保持原样）。

## 三、判据（预注册，实现前固定）

### 3.1 主判据（离线，必须全部满足）

CI-P1 起手段不重放
: 构造一个 `Start(once) → Loop1(循环) → Loop2(循环)` 的三元序列，让时钟连续跨过 ≥3 个边界；断言 `Start` 在第一个边界之后**不再被选中**（`dwell()` 接线后 `next` 稳定停在最后一次驻留成员）。

CI-P2 序列仍能正常推进
: 首次推进必须完整走 `Start → Loop1`（若作者把它标为 once/mid 段），不得因接线而跳过作者声明的入口动作。

CI-P3 `once` 全消耗后回到循环体
: 当序列尾部是循环成员时，边界重查必须停在循环成员上（对应 `commit()` 的 dwell 分支语义）；仅当整条序列全为 once 时，才允许从顶部重启。

CI-P4 非驻留选择的既有行为不变
: 对 `accepted-same-asset`（20/31 次边界）路径，时钟与 history 的行为**逐位保持**当前实现——接线只影响"该用 dwell 而非 commit"的判定，不得改变同资源快路径。

CI-P5 `seek_dwell` 中途落地
: 模拟面板在序列中途打开（`activation=true` 且当前驻留位为 Loop2），断言随后 ≥3 个边界都停在 Loop2，不回卷到 Start。

### 3.2 保护判据（不得退化）

CI-P6 装备/卸下过渡不受影响
: `ActionKind::e_equip` / `e_unequip` 的选择路径不变；`transition_preview`、`handoff_seconds`（`weaponDraw`/`weaponSheathe` 标注优先）不被触碰。

CI-P7 火把层不受影响
: `channel == 3` 的 `torch_clip` 序列推进使用同一 `SelectionState`；接线后火把臂的 `commit` 语义保持当前行为（若火把序列也需要驻留，另立契约，不夹带）。

CI-P8 门 4 / G5 冻结判据不触碰
: 本契约不产生、不修改任何 `tau_p`/`tau_r`/`tau_s`/`epsilon_t`；姿态对照结论不变。

### 3.3 上机判据（一次会话，可选但建议）

CI-R1 操作者目视：待机连续 ≥2 分钟不再出现"起步 → 起身"循环，且不同武器族的 idle 仍有正常变化（目视一句话）。

> 判据不看结果调整。CI-P1..P5 任一不成立 → 本方向证伪，判停，不在本契约内调参重试。

## 四、实现边界

- 只改 `controller.cpp` 的接受路径与必要的 `variant_state.h` 调用接线；**不新增 INI 键**（缺陷修复，不是实验开关）。
- `dwell()` / `seek_dwell()` 已是死代码，接线不新增 API 面积。
- 不触碰 `src/animation/oar/` 的查询层、不改 OAR 替换语义。
- 变更后必须补/扩展 `tests/animation_controller_test.cpp` 或新增引擎无关用例覆盖 CI-P1..P5，并按 [build.md](../../build.md) 记录构建身份。

## 五、停点与回退

- 若 CI-P2 与 CI-P1 无法同时满足（即"不重放起手段"必须牺牲"播放作者的入口动作"），说明 OAR 序列语义与面板的常驻 idle 目标存在真实冲突 → 判停，转 CON 式方案讨论（例如：面板只在首次进入时播放一次 Start，之后强制锁到循环成员），不在本契约内放宽 CI-P1。
- 崩溃/构建失败：保留日志与匹配 PDB，不在同进程重试。
- 本契约**不**声称解决 `idle-start-replay` 之外任何动画问题；`axe flicker`、`torch layer` 等开放项不受影响。

## 六、变更记录

- 2026-11-23：契约落盘。取证基于 `CharacterPanel-animation-repair-20261023-2253.log`（31 次边界决策）与 `variant_state.h` 静态审计（`dwell`/`seek_dwell` 零调用点）。判据不看运行结果。

## 七、方向证伪与根因落定（2026-11-23，实现前离线实验）

> **§二 的修复方向已撤回。** 本节记录证伪过程与真正的根因。§一至§五的预注册文本保持原样，不改写。

### 7.1 证伪实验（引擎无关，直接编译运行 `SelectionState`）

序列 `Start(once) → Loop1 → Loop2`，连续 7 次"选段 + 接受"：

| 接受方法 | 选段序列 | 判定 |
| --- | --- | --- |
| `commit()`（**当前接线**） | `Start → Loop1 → Loop2 → Loop1 → Loop2 → …` | 正确驻留 |
| `dwell()`（§二 拟接线） | `Start → Start → Start → …` | **永不出段** |

`dwell()` 的 `flags[pick.position] = false`（[variant_state.h:151-152](../../../src/animation/variant_state.h)）清掉了 PlayOnce 标记，于是 `variant()` 的 `if (!once || !played(...))`（[:63](../../../src/animation/variant_state.h)）每轮重新选中入口成员。**它从未实现过"驻留"**，零调用点反而保护了产品。

**结论**：`dwell()`/`seek_dwell()` 不是"已实现未接线的修复"（§1.3 的判断错误），而是从未正确的死代码。§二 若实施会引入比原缺陷更严重的回归。**不接线。**

### 7.2 `_bPlayOnce` 是 OAR 变体序列的驻留条件（但**不是本缺陷的根因**）

> **2026-11-23 追加更正（用户声明后）**：本节原先写作"真正的根因是入口变体的 `_bPlayOnce` 标志"。**该归因已撤回**——用户声明 Smooth Moveset 系列的 Start/Loop 判决由 **DAC（Dynamic Animation Casting）** 裁定，不走 OAR 变体序列的 PlayOnce 机制。见 §7.3。以下实验事实仍然成立，但只描述"若走 OAR 变体序列，`_bPlayOnce` 决定是否驻留"这一机制，不是本缺陷的成因。

同一序列在 `commit()` 下,行为完全由入口变体的 once 标志决定：

| 入口 `_bPlayOnce` | 选段序列 | 现象 |
| --- | --- | --- |
| `true` | `Start → L1 → L2 → L1 → L2 → …` | 正常驻留 |
| `false` | `Start → L1 → L2 → Start → L1 → L2 → …` | 每圈重放起手段 |

`once` 来自 OAR 元数据：[hooks.cpp:776](../../../src/animation/oar/hooks.cpp) 读 `read_at<std::uint8_t>(variant, 0x29) != 0`，而 `Variant::_bPlayOnce=0x29` 有 PDB 审计证据（[oar-hooks.md:152](../../oar-hooks.md)：`_bDisabled=0x28`, `_bPlayOnce=0x29`, size 56）。字段与语义均正确。

### 7.3 真正的根因：Smooth Moveset 的 Start/Loop 由 DAC 标记裁定，而边界重查未得到 Loop 胜者

Smooth Moveset 系列不使用 OAR 变体序列来区分 Start 与 Loop。它用 **DAC 的持久魔法效果标记**作为相位协议，并由 OAR 条件通过 `HasMagicEffect` 读取这些标记来判决——[effects.cpp:274-275](../../../src/animation/dac/effects.cpp) 原文："Smooth's phase protocol lives in these markers: the OAR branches read them through `HasMagicEffect`"。

通路本身在面板侧是正确的：

1. `HasMagicEffect` 在 [policy.h:48](../../../src/animation/oar/policy.h) 的 `forwards_source` 白名单里；
2. [hooks.cpp:566](../../../src/animation/oar/hooks.cpp) 对白名单条件走 `s_evaluate(condition, source, …)`，即**直接对源 Actor 求值**——DAC 在源 Actor 上放的标记因此可被看见。

**但运行日志显示判决没有翻转**（[CharacterPanel-issue-trace-r2-20261022-2314.log](../CharacterPanel-issue-trace-r2-20261022-2314.log)）：

| 时刻 | 事件 |
| --- | --- |
| 23:05:30.653 | `ANIMATION ANNOTATION … event='Smooth_loop'`（Start 段标注触发） |
| 23:05:30.667 | `DAC … event='Smooth_loop' native call completed revision=5`（施法调用完成） |
| 23:05:30.683 | `decision=accepted-same-asset reason=interrupt`（**仍选中原 clip**） |
| 23:05:31.469 | `decision=boundary`（边界重查） |
| 23:05:31.479 | `decision=accepted-same-asset reason=boundary`（**边界重查仍不变**） |

这与 [dac.md:54](../../dac.md) 已登记的开放项一致："the `Smooth_loop` annotation is cast and the loop marker is observed, **yet boundary re-queries keep returning `accepted-same-asset`**"；以及 [display-issues-2026-10-22.md:228](../display-issues-2026-10-22.md)："axe Start→Loop 不切换（`Smooth_loop` 已施法、边界重查仍 `accepted-same-asset`）仍是独立待查项"。

**因此缺陷是**：DAC 施法完成 ≠ 标记对 OAR 可见 ≠ OAR 选中 Loop。三者之间至少断了一处。候选断点（均未验证）：

| 断点 | 含义 | 面板侧可否观测 |
| --- | --- | --- |
| B1 标记未真正激活 | `native call completed` 只证明函数返回，不证明暂停引擎里效果已激活（[dac.md](../../dac.md) 反复强调） | `smooth_markers()` 报告可读，但"未激活"与"未存在"需分辨 |
| B2 标记存在但 OAR 条件未命中 | Smooth 的 OAR 条件可能还有别的合取项（如 `Smooth_Start` 未清除） | 需完整条件求值追踪 |
| B3 OAR 已正确返回 Loop，但被 `accepted-same-asset` 快路径吃掉 | 若 Loop 与 Start 同 `replacement_key`，[:712-745](../../../src/animation/controller.cpp) 会判"同资源"而不切换 | 现有 trace 可区分（`selected.index`），日志显示 index 未变 |

**归因未落定前，本契约不提出修法。** 上一轮的 D1/D2/D3 三选项建立在错误归因上（把 `_bPlayOnce` 当根因），**全部作废**。

### 7.4 修正后的取证需求（下一步，尚未执行）

要区分 B1/B2/B3，需要在一次 `DetailedAnimationTrace=true` 会话中同时取得：

1. `Smooth_loop` 施法前后的 `smooth_markers()` 逐次差异（现有 `DAC OBSERVED` 行已含 markers 列表，需比对 `Smooth_Start` 是否仍活跃）；
2. 边界重查的 OAR 查询结果（`selected.index` / `replacement_key`）与 `accepted-same-asset` 判定所比对的字段；
3. Smooth Moveset 的 OAR 条件集合（离线读其 json，判定是否有除 `HasMagicEffect` 之外的合取项）——**这一项可纯离线完成，且应最先做**。

### 7.5 离线取证结果：Smooth Moveset 的四标记相位协议（2026-11-23，第 3 项已完成）

读了本机 `E:\SkyrimAE\mods\Smooth Moveset - OAR`（92 个非战斗 idle 配置：Start 36 / Loop+Loop2 41）。

**相位协议**（全系列一致，无例外）：

| 变体 | 优先级区间 | 相位条件（OAR json） |
| --- | --- | --- |
| `* Non-Combat idle Start` | `…001` | `HasMagicEffect 801` 存在 |
| `* Non-Combat idle Loop` | `…002` | `HasMagicEffect 804` **negated**，且 `HasMagicEffect 802` 存在 |
| `* Non-Combat idle Loop2`（仅 1hm / 1hm Shield / 1hm Pistol / Katana 有） | `…003` | 同 Loop，另加 `Random < 0.5` |

**DAC 侧**（`SKSE/Plugins/_DynamicAnimationCasting/SmoothMoveset.toml`）：

| 事件 | 施放的 spell 集合 |
| --- | --- |
| `Smooth_loop` | `[0x80A, 0x810, 0x82E]` |
| `Smooth_Loop_off` | `[0x80E, 0x80F, 0x82A]` |

**运行日志的标记序列**（[r2 日志](../CharacterPanel-issue-trace-r2-20261022-2314.log)，token 形如 `24`+local id）：

| 时刻 | markers | 解读 |
| --- | --- | --- |
| 23:05:27.560 | `3801, 3804, 3806, 3807, 3827, 382C, 3831` | 有 3804（Start 相位） |
| 23:05:28.256 | `3801, 3806, 3807, 3808, 3827, 382C, 3831` | 3804 消失 |
| **23:05:30.667** | `3801, **3802**, 3807, 3808, 3827, 382C, 3831` | **`Smooth_loop` 施法后 3802 出现** |
| 23:05:31.595 | 同上 | 边界重查在此时刻之后 |

**关键观察**：`Smooth_loop` 施法后 `3802` 确实出现、`3804` 确实消失——**这两条正是 Loop 相位条件所要求的**。但同一时刻之后的边界重查仍返回 `accepted-same-asset`。

### 7.6 离线取证第二阶段：ID 空间已闭合，根因是 `mag_start on` 未被清除（2026-11-23）

工具：[`tools/esp_mgef_probe.py`](../../../tools/esp_mgef_probe.py)（纯 stdlib，`--self-test` 4 例通过）。

**ID 空间**：`Smooth Moveset.esp` 有 19 条 MGEF，raw FormID `0x01000801`–`0x01000831`。日志 token 是 6 位十六进制，其**低 3 位**与 ESP 的 low24 精确对应：

| 日志 token | 低 3 位 | ESP low24 | EDID |
| --- | --- | --- | --- |
| `243801` | `801` | `0x000801` | `mag_start on` |
| `243802` | `802` | `0x000802` | `mag_loop on` |
| `243804` | `804` | `0x000804` | `mag_combat on` |
| `243806` | `806` | `0x000806` | `mag_loop off` |
| `243807` | `807` | `0x000807` | `mag_move off` |
| `243827` | `827` | `0x000827` | `mag_non-combat on` |
| `24382C` | `82C` | `0x00082C` | `mag_combat_loop off` |
| `243831` | `831` | `0x000831` | `mag_move_equip` |

前缀 `243…` 是运行时 `GetFormID()` 的加载序相关位；`smooth_marker_report` 用 `& 0xFFFFFF` 取 low24 后再 `{:02X}` 输出，因此 token 的**后 3 位**才是可比的 ID。（**推测残留**：前缀 `243` 的确切构成未验证；不影响本节结论，因为比对只用后 3 位，而 EDID 命名独立佐证了映射。）

**条件判定**（OAR json × 实测标记）：

| 时刻 | 相关标记 | Start 条件（801 存在） | Loop 条件（804 不存在 + 802 存在） |
| --- | --- | --- | --- |
| 23:05:27.560 | 有 `mag_start on`、`mag_combat on`；无 `mag_loop on` | **满足** | 不满足（804 存在且 802 缺失） |
| 23:05:30.667 | 有 `mag_start on`、`mag_loop on`；无 `mag_combat on` | **满足** | **也满足** |
| 23:05:31.595 | 同上 | **满足** | **也满足** |

**根因**：`mag_start on` 在整个窗口的 idle 阶段**始终存在**，因此 Start 变体的条件永远成立。Start 的 `priority=83030011` 小于 Loop 的 `83030012`（OAR 数值小的优先），于是即使 Loop 条件在 23:05:30.667 之后已经满足，**Start 仍然胜出**。

**`sp_start on` / `sp_start off` 是互斥对，协议本身自洽**（本节依据 §7.9 的完整时间线验证）：

- `sp_start on`（`0x809` → `MGEF 0x801`）只由 **`weaponDraw`** 携带（`[0x809, 0x80F, 0x80E, 0x82E, 0x80C]`）。
- `sp_start off`（`0x80D` → `MGEF 0x805`）只由 **`weaponSheathe`** 携带（`[0x80E, 0x80D, 0x810, 0x82E]`）。
- 二者在日志中严格互斥：`weaponSheathe` 之后 `801` 消失、`805` 出现；`weaponDraw` 之后反过来。**没有同时持有的记录**。

| 事件 | spells | 净效果（对 801/802/804/805） |
| --- | --- | --- |
| `weaponDraw` | `0x809, 0x80F, 0x80E, 0x82E, 0x80C` | 加 **start on(801)**、清 move/loop、加 combat on(804) |
| `weaponSheathe` | `0x80E, 0x80D, 0x810, 0x82E` | 清 loop/move/combat、加 **start off(805)** |
| `Smooth_loop` | `0x80A, 0x810, 0x82E` | 加 **loop on(802)**、清 combat on、清 combat_loop on |
| `Smooth_Loop_off` | `0x80E, 0x80F, 0x82A` | 清 loop on、清 move on、清 non-combat on |
| `Smooth_NonCombat` | `0x810, 0x80F, 0x828, 0x82E` | 清 combat on、清 move on、加 non-combat on |

**HKX 侧确认严格配对**（§7.9）：152 个带 `weaponSheathe` 的 clip **全部**是 `*_unequip.hkx`；80 个带 `weaponDraw` 的 clip **全部**是 `*_equip.hkx`。零例外，没有任何 idle clip 携带二者之一。

**因此**：Smooth 的相位机假设 `start on` 在进入 idle 前已被 `weaponSheathe` 清掉。面板的装备驱动 idle 在 `weaponDraw` 之后直接进入 idle 循环，**不再经过 Sheathe**，`start on` 就一直留着，Start 相位永不退出。

**这不是面板代码错误**：面板正确派发了 clip 标注里存在的 `Smooth_loop`（三份日志共 110 次）；`Smooth_Loop_off` 在标注集合里从未出现（资源事实）。缺口是**面板的 idle 场景天然缺少 Sheathe 这一步**。

### 7.7 由此产生的三个具体待验问题（不再推测）

1. **面板是否应补派 `sp_start off`**：若在 idle 首次选段后主动清 `start on`，Start 相位就会退出、Loop 胜出。风险是面板替 Smooth 补相位可能干扰正常游玩的相位机（尤其面板开/关切换时）。
2. **`HasMagicEffect` 的活跃语义**：`"Active effects only": false` 在 OAR 里的确切含义（是否等于"含 inactive"）。本节结论只用存在性，不依赖该区分。
3. **正常游玩时 `start on` 的清除时点**：本机日志显示 sheathe 会清它，但正常游玩中 `mag_start on` 何时被清、以及它是否本来就该长期存在，需要另取一份非面板会话的日志对照——**尚未做**。

### 7.9 HKX 标注普查（2026-11-23，纯离线）

扫描 `Smooth Moveset - OAR/meshes` 下 616 个 HKX 的明文标注名：

| 标注 | 出现于多少个 HKX |
| --- | ---: |
| `weaponSheathe` | 152 |
| `weaponDraw` | 80 |
| `Smooth_loop` | 95 |
| `Smooth_Combat` | 39 |
| `Smooth_Loop` | 23 |
| `Smooth_NonCombat` | 19 |
| `Smooth_Stop` | 5 |
| `Smooth_Loop_off` | **0** |

文件名分布（按出现次数）：`weaponSheathe` → `1hm_unequip.hkx` 52、`axe_unequip.hkx` 23、`mac_unequip.hkx` 22、`dag_unequip.hkx` 20 等，**全部是 `*_unequip.hkx`**；`weaponDraw` → `1hm_equip.hkx` 26、`axe_equip.hkx` 11 等，**全部是 `*_equip.hkx`**。

**`Smooth_Loop_off` 在 616 个 HKX 中零出现**——它不是由动画标注派发的，只出现在 DAC toml 的规则表里（可能由本 mod 之外的路径或未使用的规则触发）。

### 7.10 运行时间线与互斥验证（2026-11-23）

[r2 日志](../CharacterPanel-issue-trace-r2-20261022-2314.log) 的全部 `DAC OBSERVED` 快照，按标记解析：

| 时刻 | 触发事件 | start on(801) | start off(805) | loop on(802) | combat on(804) |
| --- | --- | :---: | :---: | :---: | :---: |
| 23:05:21.935 | `Smooth_loop` | Y | – | Y | – |
| 23:05:27.560 | `weaponDraw` | Y | – | – | Y |
| 23:05:30.667 | `Smooth_loop` | Y | – | **Y** | – |
| **23:05:33.245** | **`weaponSheathe`** | **–** | **Y** | – | – |
| 23:05:36.189 | `weaponDraw` | **Y** | – | – | Y |
| 23:05:39.298 | `Smooth_loop` | Y | – | **Y** | – |
| **23:05:40.715** | **`weaponSheathe`** | **–** | **Y** | – | – |
| 23:06:40.457 | `weaponDraw` | **Y** | – | – | Y |
| 23:06:44.870 | `Smooth_loop` | Y | – | **Y** | – |
| 23:07:38.883 | `weaponDraw` | Y | – | – | Y |
| 23:07:49.486 | `Smooth_loop` | Y | – | **Y** | – |

**两条硬性观察**：

1. `801` 与 `805` **从未同时出现**，严格互斥——协议自洽，不是"两个独立累加的效果"。
2. **每次 `Smooth_loop` 之后 `start on` 都是 Y**，因此每次边界重查都只满足 Start。`weaponSheathe` 确实能清零（23:05:33 / 23:05:40），但面板的 idle 循环里紧接着的 `weaponDraw` 又把它加回来。

### 7.8 已排除的方向

| 方向 | 排除依据 |
| --- | --- |
| 接线 `dwell()`/`seek_dwell()` | §7.1 引擎无关实验：`dwell()` 永久卡在入口段 |
| `_bPlayOnce` 未标记 | §7.2 追加更正：36 个 Start 是独立 `config.json` + priority 区分，不走 OAR 变体序列 |
| 面板漏读 `HasMagicEffect` | [policy.h:48](../../../src/animation/oar/policy.h) 白名单 + [hooks.cpp:566](../../../src/animation/oar/hooks.cpp) 源 Actor 求值，通路完整 |
| 面板写错相位变量 | Smooth 用**魔法效果标记**，不是行为图变量；`preview_state` 不参与判决 |
| Loop 条件永不满足（ID 空间错配） | §7.6：ID 映射已闭合，Loop 条件在 23:05:30.667 后确实满足 |
| DAC 施法失败 | §7.6：`Smooth_loop` 施法后 802 出现、804 消失，标记变化与预期一致 |

### 7.4 已落地的离线产物

`tests/idle_dwell_policy_test.cpp`（7 例，`/W4 /WX` 通过）把 §7.1–7.2 的行为固化为回归护栏：

- `entry_flagged_play_once_dwells_on_the_loop_body` — 正确形态
- `entry_without_play_once_replays_every_cycle` — 缺陷形态（精确逐拍断言）
- `authored_order_is_preserved_in_both_shapes` — 修复不得跳过入口段
- `all_once_sequence_restarts` — 全 once 序列允许重启
- `dwell_pins_the_entry_member_forever` — **证伪的可执行记录**
- `seek_dwell_moves_the_recheck_start` — 该方法的真实作用
- `clip_without_variants_yields_no_pick` — 回退路径

**生产代码零改动**（本契约到此为止只涉及文档与测试）。

## 八、C 项取证：正常游玩是否同样重放（2026-11-23，离线部分完成、结论受限）

**问题**：若正常游玩也有同样循环，则方案 A（面板补派 `sp_start off`）就等于修复一个并不存在的差异，风险判断失去依据。

### 8.1 离线能确定的事实

| 事实 | 依据 |
| --- | --- |
| `mag_start on`(801) 只被 **18 个 `Non-Combat idle Start` 变体**读取 | 遍历 226 个 `config.json` |
| 每个 Start 的 priority 都紧邻其 Loop（`…001` vs `…002`），OAR 数值小者优先 | 同上 |
| `mag_loop on`(802) 被 76 个变体读取 | 同上 |
| **所有 `off` 效果（805/806/807/808）不被任何条件读取** | 同上，0 命中 |
| `replaceOnLoop` 在 226 个配置里只有 **6 个显式声明**，全部为 `false`，且都不是 idle 变体 | 同上 |
| HKX 中 `weaponSheathe` 只出现在 `*_unequip.hkx`（152/152），`weaponDraw` 只在 `*_equip.hkx`（80/80） | §7.9 |

### 8.2 离线无法确定的部分（必须承认）

**正常游玩时 `mag_start on` 的生命周期没有被观测过。** 原因：

- `smooth_marker_report` 只在 `queue_idle`（面板 idle 通路）里调用（[controller.cpp:819](../../../src/animation/controller.cpp)），面板关闭时探针不运行；
- 三份归档日志的全部 `DAC OBSERVED` 行都落在面板打开窗口内；`DAC restore` 之后没有任何标记快照。

因此"正常游玩不重放"这一前提**在本仓库现有证据里是缺失的**，只能作为外部常识（该 mod 广泛使用且未闻此缺陷）而非实测结论。

### 8.3 逻辑推演给出的三种情形（均未验证）

| 情形 | 内容 | 若成立，方案 A 的风险 |
| --- | --- | --- |
| N1 | 正常游玩时 `start on` 确实长期存在，但玩家不感知（第三人称远景、动作快、或引擎不走 OAR 的 Start） | A 无效或引入相位错乱 |
| N2 | 正常游玩在 `weaponDraw` 之后还有一次 `weaponSheathe`（收刀动作）清掉 801 | A 安全（只是补了缺失的那一步） |
| N3 | 正常游玩根本不进入 `Non-Combat idle Start`（引擎直接给 loop 基动画） | A 无意义（面板的问题另有来源） |

**三者假设互斥，现有离线证据不能排除任何一个。**

### 8.4 C 项的结论

**C 无法纯离线完成。** 要把 N1/N2/N3 区分开，最小代价是**一次非面板会话**：正常游玩中拔刀 → 待机 ≥10 s → 收刀，期间用现有 `SourceTimeline` 或新增一个轻量标记快照读取 `smooth_markers()`（面板关闭状态下也要能采样）。

在此之前，**方案 A 不做**——它的安全性完全依赖 N2 成立，而 N2 未被证实。

### 8.5 若用户确认"正常游玩无此现象"

如果用户能直接确认正常游玩待机不循环重放（即 N1/N3 已排除、N2 成立），则：

- 方案 A 的安全性成立（面板只需补正常游玩本来就有的那一步）；
- 但**补派发的实现仍需先立判据**：必须证明在面板关闭/切换时不会把 `start off` 带进正常游玩的相位机（例如面板 close 时应恢复基线而非留下 `start off`）；
- 现有 `DAC restore ... result=matched` 机制（[dac.md](../../dac.md)）看起来正好覆盖这一点，但**未针对 `sp_start off` 验证过**。

## 九、上机取证结果（2026-11-24，v126 + `DisplayPose=false`）

**完整记录见 [idle-loop-v127-session-20261124](../idle-loop-v127-session-20261124/README.md)**；日志 [CharacterPanel-g6idle-displaypose-off-20261124.log](../CharacterPanel-g6idle-displaypose-off-20261124.log)（1,424,942 B，`a2c6cebb…`）。

### 9.1 用户确认的前提（§8.5 所问）

用户实测：**正常游玩（AE 档）待机不循环重放，稳定后循环 loop 段**。因此 §8 的 N1/N3 被排除，**N2 成立**——正常游玩存在清除 `801` 的那一步，面板缺了它。

### 9.2 两个独立结论

1. **`DisplayPose` 决定面板动画来源**：`true` → 候选图 `poseLocal`（不含 OAR 替换，显示原版动作）；`false` → 控制器合成（含 OAR 替换，显示 Smooth 动作）。证据：`reason=boundary` 计数 `true` 为 0、`false` 为 36（边界检测代码位于提前返回点之后）。
2. **Start 驻留缺陷在 v126 上真实复现**：5 个武器族 36 次边界，`2hm`(4/4)、`2hw Axe`(2/2)、`1hm`换装后(2/2) 连续驻留 `* idle Start`；`1hm` 首轮与 `2h` 正常驻留 Loop。用户目视一致。

### 9.3 必须撤回的归因

**§7.6 的"Start 的 priority 数值更小所以胜出"已撤回。** 同一标记状态（`801=1 802=1 804=0`）下，`1hm` 选 `Loop2`、`2hm` 选 `start`——priority 的两个方向都无法解释族间差异。

**保留的**：`801`（`mag_start on`）在该会话 20/20 次快照中从未被清除（`805` 出现 0 次）。

**新增的候选差异**（未验证）：`1hm Start` 含 `OR[IsEquippedType == 1.0, right hand]`，`2hm start` 无此条且相位条件包了一层 `OR[801, OR[804, IsInCombat negated, 801]]`。这是目前唯一发现的、能区分"1hm 正常 / 2hm 卡住"的结构差异。

### 9.4 修复方向（三选一，均未验证）

见 [v127 记录 §五](../idle-loop-v127-session-20261124/README.md)：A 面板侧强制驻留 / B 补派 `sp_start off` 或 `weaponSheathe` / C 记录为已知限制。A 与 B 的共同前置：证明修复动作不会在面板关闭/切换时漏进正常游玩的相位机。

## 十、收尾裁定：定为已知限制（方向 C，2026-11-25）

> **本契约到此关闭。** 依据 [C-WORLD v1 run 1](../world-phase-20261125/README.md)：正常游玩 722 行采样中 `801`(`mag_start on`) 同样从不被清除（`phase=both` 401 行、`loop` 0 行），两次拔刀时间线同构（`start` ~3 s → `both` 至收刀）。

### 10.1 最终事实

| # | 事实 | 证据 |
| --- | --- | --- |
| 1 | 面板与正常游玩在 `801` 状态上**完全一致**（都常驻） | C-WORLD run 1（722 行）＋ v127（20/20 快照） |
| 2 | 清 `801` 的唯一路径是 `weaponSheathe`（toml 严格二选一，HKX 中只在 `*_unequip.hkx`） | 离线取证（§7.9） |
| 3 | idle 相位不经过 Sheathe，故两种场景都不清 `801` | 由 1+2 推出 |
| 4 | 给定相同的 `both` 标记状态，世界转 Loop 而面板继续选 Start | v127 边界对照 + 用户目视 |
| 5 | 面板内动画来源由 `DisplayPose` 决定（`false` 才走 OAR 合成路径） | v127（`reason=boundary` 0 vs 36） |

### 10.2 为什么不定为"面板缺陷"

- 缺陷不是"面板少做了一步"——世界也不做那一步（事实 1–3）。
- 无法在面板侧安全修复：方向 B 会清出一个世界中不存在的状态（违背事实 1）；方向 A 依赖命名约定启发式，而事实 4 的分歧来源未解释，硬切可能掩盖真实机制。
- 影响面有限：面板内 idle 每轮重放一次起手段，动作本身正确、无扭曲（v127 目视）。

### 10.3 保留的开放项（如需重启）

**事实 4 的分歧来源**（相同标记状态下 OAR 选择不同）是本契约未解释的唯一实质问题。重启条件与最小取证：新增探针同时记录「面板调用 `s_evaluate_replacements` 的返回」与「引擎自身调用同一函数的返回」，比对同一标记状态下的差异。在此之前，本契约的修复方向 A/B 均不得实施。

### 10.4 边界（不得外推）

- 本裁定不声称面板动画与游戏世界完全一致；`both` 状态下的选择分歧仍在。
- 不声称 Smooth Moveset 无缺陷；只声称本项目的面板侧未引入可归因的偏差。
- 不改变 G1–G6 任何已签收结论。

### 10.5 变更记录

- 2026-11-25：契约关闭，定级为已知限制（方向 C）。撤回 §二 的 `dwell()` 方向（§7.1）、§7.2 的 `_bPlayOnce` 归因（§7.6 更正）、§7.6 的 priority 归因（§9.3 撤回）。G1–G6 的既有签收不受影响。
