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
