# C-G5-G4-COV v1：门 4 覆盖分层预注册（2026-11-16，评审通过）

**状态：评审通过（2026-11-16，用户授权按推荐方案 A 执行）；实现与 metadata-only dry-run 完成。** 本文定义 99 主分母 + 17 辅助账本的覆盖结构；不改变姿态判据、不产生 tau/epsilon。

**批准记录**：用户于 2026-11-16 授权按方案 A 执行：门 4 的 CAL1-B 姿态判定使用 P99（99 轨）主分母，A17（17 轨）单列描述性账本，不声称 116/116；工具实现预注册随后落地。批准不改变 §三判据或 CAL1-B 数值。

本草案落实 [CAL1-C 覆盖审计](../g5-cal1c-coverage-timing-20261115/README.md) 的结论：控制 B 只覆盖 `mt_idle.hkx` binding map 0..98 的 99 条 transform 轨；门 4 的 116 轨分母中 99..115 是 6 个 item、10 个 wing、1 个 Belly 轨，CAL1-B 的 tau 不能覆盖它们。

## 一、覆盖定义

| 集合 | 轨 | 身份 | 门 4 处置 |
| --- | --- | --- | --- |
| **P99 主集** | 0..98 | `mt_idle.hkx` binding map identity 0..98；CAL1-B 的 99 条控制轨 | 99/99 主分母；可套 CAL1-B `tau_p/tau_r/tau_s`；G4P-7 只对 P99 判定 |
| **A17 辅助集** | 99..115 | 6 item（`NPC L/R Item01..03`）+ 10 wing（`Left/RightWing1..5`）+ `Belly` | 17/17 单列描述性账本；**不套 CAL1-B tau、不判 PASS/FAIL、不计入 116/116** |

**分母规则**：本修订后，门 4 的 CAL1-B 姿态判定分母为 **P99 的 99 轨**；A17 单列报告。K5 的 116 轨清单仍保留为仪器/库存分母，但不再与“CAL1-B 姿态等价”混用。**不得再把 116/116 写成 CAL1-B 的通过条件。**

**A17 的可见性**：A17 的 `E_p/E_r/E_s` 逐轨/逐对报告，不删帧、不归零、不当作测量噪声；它们是否需要独立控制由后续协议决定。

## 二、门 4 输出与工具变更

门 4 的输出必须分成两个账本：

| 账本 | 内容 | 判据 |
| --- | --- | --- |
| `primary-p99` | 99 轨的逐对 `E_p/E_r/E_s`、超容差计数、分布、G4P-1..9 | 可用 CAL1-B tau；G4P-7 仅在此账本判 |
| `aux-a17` | 17 轨的逐对 `E_p/E_r/E_s`、分布、源/候选身份 | 描述性；`no_tolerance=true`；不判 PASS/FAIL |

工具预注册（批准后才实现）：

- `gate4_pair.py` 增加 `--coverage <json>`（或等价冻结参数），JSON 定义 `primary`/`auxiliary` 轨索引与名称；默认无 coverage 时保持旧的 116 轨行为并标 `legacy-116`，但 **legacy 模式不得用于 CAL1-B 裁定**。
- 输出 manifest 增加 `coverage` 节：`primary_count=99`、`auxiliary_count=17`、集合哈希、`legacy_116=false`；summary 分别输出 `G4P PRIMARY` 与 `G4P AUX` 行。
- `--tolerances` 仍要求 `epsilon_t`；覆盖分层不改变时间槽位，也不解锁正式模式。

## 三、判据（预注册，不得事后放宽）

| # | 判据 | 通过条件 | 否则 |
| --- | --- | --- | --- |
| COV-1 主集身份 | P99 与 MT_Idle binding map 0..98 一致；99 个唯一索引、无重复/越界 | 全部满足 | INVALID |
| COV-2 辅助集身份 | A17 = 99..115，名称/父索引与 K1-C3 登记一致；6 item + 10 wing + Belly 分组计数 = 6/10/1 | 全部满足 | INVALID |
| COV-3 不重叠 | P99 ∩ A17 = ∅；并集 = 0..115 | 全部满足 | INVALID |
| COV-4 双账本 | 输出同时含 `primary-p99` 与 `aux-a17`；A17 `no_tolerance=true`；无 116/116 PASS | 全部满足 | INVALID |
| COV-5 无删除 | A17 全部逐对保留；不得因误差/缺失删轨；缺失即缺项登记 | 全部满足 | INVALID |
| COV-6 历史不改写 | K5 116 轨清单、既有 dry-run/历史裁定保留；本修订只改门 4 姿态判定分母 | 全部满足 | INVALID |

**裁定语义**：COV-1..6 全过 = 门 4 可执行 **P99 主集 + A17 辅助账本** 的覆盖结构；这不等于门 4 完整冻结（`epsilon_t`/`Δt_clock` 仍待 [C-G5-TIME v1](../g5-time-20261115/README.md)）。

## 四、边界与后续

- 不产生 `tau_p/tau_r/tau_s/epsilon_t`；不改变 CAL1-B 的 99 轨数值。
- A17 不是“已证明可忽略”：item 随装备、wing 随翼骨、Belly 有变化；它们只是**不在 CAL1-B 控制内**。
- 若未来为 A17 建立独立控制，另立 `COV v2` 提升其判据；不得在本协议里预填。
- 门 4 完整冻结仍要求 [C-G5-TIME v1](../g5-time-20261115/README.md) 的时间模型与 `epsilon_t`；门 5 仍不授权。

## 五、实施顺序与提交拆分

1. 本文评审/批准 → 冻结覆盖集合与双账本语义。**已完成。**
2. 实现预注册（CLI/schema/测试清单）→ 实现 + 测试。**已完成。**
3. 用 Track 2 dry-run 工件重跑 metadata-only，确认双账本可生成；不解码 pose。**已完成，见 §六。**
4. 门 4 完整冻结时联合 [C-G5-TIME v1](../g5-time-20261115/README.md) 验收。

## 六、实现与 metadata-only dry-run（2026-11-16）

| 项 | 值 |
| --- | --- |
| 覆盖集合 | [coverage.json](coverage.json)，SHA-256 `3DF45251748EF7C7F80381A57A196901298C48A0C43E782E378B83030DD0A52C`；P99=0..98、A17=99..115，item/wing/belly=6/10/1 |
| 工具 | `tools/gate4_pair.py` 增加 `--coverage`；SHA-256 `51ECBD673A9DD059A38A9E7FCF3F7D09B3DCF60B4D4EDA5B7E3D657B55B9D35D`；测试 T1..T13 PASS，SHA-256 `32E67A122856492F5F92400F3F8A2DEB7D0A11D1DBEFEAC7B8020DA30F03C4E1` |
| 正式模式门 | `--tolerances` 现在要求 `--coverage`；legacy-116 仍可用于 metadata，但不得作为 CAL1-B 判定分母 |
| dry-run 输入 | Track 2 candidate 249 条 / source 1,065 条；247 对；`pose_decoded=False` |
| dry-run 输出 | [manifest](dry-run-coverage/gate4-pair-manifest.json)（112,782 B，`69EC8AA7…C0E0A`）与 [summary](dry-run-coverage/gate4-pair-summary.txt)（1,158 B，`CDF6F7A2…A8C13`）；`COVERAGE primary=99 auxiliary=17 legacy_116=no`，三锚 matched=yes |

**边界**：本 dry-run 只证明覆盖双账本可执行；未解码任何 pose，不产生 tau/epsilon，不改变门 4 完整冻结状态。

**A17 后续审计（2026-11-16）**：[辅助轨审计](../g5-aux-track-audit-20261116/README.md) 显示 A17 在 Track2 窗口内近似静态（跨记录分量 span ≤3.8e-6），源/候选跨侧差 `E_p≤1.97e-6`、`E_r≤~1e-15`、`E_s=0`；没有活动 clip 控制它们。故保留 COV 方案 A（A17 描述性账本），仍不声称 116/116。
