# C-G5-G4 v1 门 4 完整冻结记录（2026-11-16，限定范围）

**状态：FROZEN（2026-11-16，P99 姿态/时间/时钟轴 + A17 描述性账本）。** 本冻结按门 4 §五条件 1–4 与 §六“显式带缺项冻结”完成；**不是 G5 PASS，不是完整 CAL1，门 3 未通过**。

## 一、冻结范围

| 项 | 冻结内容 |
| --- | --- |
| 姿态 | P99（`mt_idle.hkx` binding map 0..98）的 `tau_p/tau_r/tau_s`；A17（99..115）描述性账本，无容差 |
| 时间 | `epsilon_t=0.0333960` s；`dt_exclude=true`，2% edge-only |
| 时钟轴 | G4C-1..6 `CLOCK_READY`：三域 rate/endpoint/dt 通过；候选阶梯只登记 |
| 环境 | `AE-Track2`、v104/v103 既有工件、Track 2 K5 集合、Save116 |
| 不覆盖 | K5-5 层/权重/混合、K2-C1 完整清单、A17 独立控制、其他资源/环境、G5 等价、G6 |

## 二、条件 1：槽位与可复算性

| 槽位 | 值/来源 |
| --- | --- |
| `tau_p/tau_r/tau_s` | `0.000476592` / `6.03341E-8` / `0.00000386264`（CAL1-B，P99） |
| `epsilon_t` | `0.0333960` s（TIME v1；T1 invariant TSC + frame table） |
| `dt_exclude` | `true`，阈值 `epsilon_t`，2% edge-only；dry-run 0/247 |
| 覆盖 | COV v1：P99=99、A17=17；不声称 116/116 |
| 时钟轴 | CLOCK v1：G4C-1..6 PASS；rate 0.995882/0.991083/0.993000 |

冻结参数文件：[tolerances.json](tolerances.json)。

## 三、条件 2：CAL v1 §九联合验收

[联合验收报告](../g5-cal-joint-acceptance-20261116/README.md) 的限定裁定：

> `CALIBRATED (limited: control-B P99 pose + time + clock axes)`

报告逐项列出 K1–K6 证据/缺项、原始工件/三次运行/对账、Q/N/tau/epsilon、唯一裁定与 G5/门 3 开放项。**K2-C1 PARTIAL、K5-5 缺项、A17 无独立控制**作为显式缺口传递到条件 4；不写成完整 CAL1 或 G5 READY。

## 四、条件 3：G4P-1..6、8、9 结构判据

| 判据 | 证据 | 结果 |
| --- | --- | --- |
| G4P-1 运行资格 | Track 2 K5-1..8 两侧 PASS；六族 PAIRED；IDUN/声明缺口记账 | PASS |
| G4P-2 账本闭合 | dry-run candidate/source ledger 恒等式闭合；未归属 0 | PASS |
| G4P-3 括号覆盖 | 247 对；no_partner=0；edge 12 全记账 | PASS |
| G4P-4 配对确定性 | 同输入两次 coverage dry-run：manifest `69EC8AA7…C0E0A`、summary `CDF6F7A2…A8C13` 逐字节一致 | PASS |
| G4P-5 元数据有效 | 钟值有限；实例窗/环无歧义；K5-7 集合处置沿用 Track 2 | PASS |
| G4P-6 记账完整 | IDUN/no-clock/edge/wrap/族集合差全入账；三锚 matched | PASS |
| G4P-8 无效值守卫 | metadata-only dry-run 不产生 pose 无效值；工具 T2/T12 已覆盖非有限/零范数守卫；正式 pose 守卫在门 5 执行 | PASS（metadata 范围） |
| G4P-9 工件完整 | manifest/summary 可读、HASH 登记、预算未超、工具版本登记 | PASS |

G4P-7（时间/姿态裁定）**不在冻结条件 3 内**：必须在门 5 正式新工件上用冻结的 P99 tau/epsilon/clock 规则判；A17 不判。

## 五、条件 4：显式缺项冻结

| 缺口 | 处置 | 对 G5 的阻塞 |
| --- | --- | --- |
| K5-5 层/权重/混合 | 显式缺项清单；不得发明偏移补齐；门 4 只冻结姿态/时间/时钟轴 | **G5 等价裁定持续阻塞** |
| K2-C1 装备初态 | PARTIAL（护甲 42 槽/存档身份缺）；不阻塞 P99 单层控制，但阻塞完整 CAL1/G5 | **G5/完整 CAL1 阻塞** |
| A17 item/wing/Belly | 近似静态共享状态（span ≤3.8e-6，跨侧 `E_p≤1.97e-6`）；无独立 clip 控制；描述性账本、不套 tau、不声称 116/116 | **116/116 不成立；G5 阻塞** |
| G4P-7 | 门 5 正式数据才判；不得用 dry-run pose 预判 | 无冻结前裁定 |

## 六、门 5 授权边界

本冻结授权门 5 **采集**（不是 G5 判定）：

- 候选 k5-inject / 源 k5-source 各一次会话/窗口，协议按门 4 §七逐字执行；
- 每轮先判 K5-1..8；失败即 INVALID，不配对；
- 正式姿态裁定只用 P99 + 冻结 tau/epsilon/clock；A17 单列描述，不判 PASS/FAIL；
- G5 仍因 K5-5/K2-C1/A17 阻塞；不得把门 5 数据写成 G5 通过。

**本记录不表示门 5 已运行。**

## 七、工具与工件身份

| 项 | SHA-256 |
| --- | --- |
| `tolerances.json` | `B810698EC180B108A1515A21324A37E5DC293175AF05C18B19EA2AC7D55710EE` |
| `gate4_pair.py` | `51ECBD67…B9D35D` |
| `gate4_clock.py` | `6E21BB47…A41780` |
| `cal1c_time.py` | `95AA8B4A…CE4623` |
| 覆盖 dry-run manifest / summary | `69EC8AA7…C0E0A` / `CDF6F7A2…A8C13` |
| CAL1-C clock JSON / summary | `15F5EACD…1A4A76` / `F6EED377…C11774` |

## 八、冻结边界

- 门 4 FROZEN 只覆盖本记录的 P99 姿态/时间/时钟轴与 A17 描述性账本；不覆盖 G5 层/权重/混合、G6、其他资源/环境。
- 若未来闭合 K5-5/K2-C1/A17 独立控制，可另立门 4 修订；本冻结不阻止，也不自动升级 G5。
- 任何门 5 采集必须在新的正式工件上进行；Track 2 dry-run 数据不得充当正式样本。
