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
