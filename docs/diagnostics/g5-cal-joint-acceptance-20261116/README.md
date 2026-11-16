# C-G5 CAL v1 §九联合证据验收（2026-11-16，限定范围）

**状态：限定范围联合验收通过；裁决 `CALIBRATED (limited: control-B P99 pose + time + clock axes)`。完整 CAL1/门 3、K5-5、K2-C1、A17 独立控制与 G5 仍未通过。**

本文按 [CAL v1 §九](../../independent-behavior-graph.md#c-g5-cal-v1) 为门 4 的条件 2 形成联合证据报告。它不把受限校准写成完整 CAL1 或 G5 PASSED；G5 级裁定仍按 [G5 v2 §六](../../independent-behavior-graph.md#g5-draft) 等待层/权重/混合。

## 一、版本、运行清单与身份

| 项 | 值 |
| --- | --- |
| 插件 | v104 `CharacterPanel-scenegraph-v104-g5cal2-grid-d61a6e19b9-cl94faaed0c6-20261115T073547Z` |
| DLL / PDB / INI | `7d270e7d…d6f9df` / `b2232180…c2ac1` / `bbfe2b8b…eafbb2` |
| runtime / 环境 | `1-6-1170-0` / `AE-Track2`（OAR 关闭） |
| 资源 | `female\mt_idle.hkx`，113,920 B，SHA-256 `0EF8248C…BCD73C7` |
| 控制真值 | `mt_idle_female_truth_grid121.txt`，SHA-256 `DE26ECEB…C387`（BOUNDS 混合轨修复后） |
| 三进程 CGRD | run1 `CB558757…D508B`、run2 `F57D6251…22221`、run3 `60B373DB…F9008` |
| CAL2 三进程裁定 | `cal2-adjudication.json`，SHA-256 `B670782A…C54A7`，C2-D PASS/READY |

## 二、K1–K6 证据与缺项

| 项 | 证据 | 状态/缺项 |
| --- | --- | --- |
| K1-C1 | 行为闭包/OAR/loose 全量清单、锚点 | REGISTERED（本机/modlist/时点） |
| K1-C2/C3 | v101/v101b census；116 骨/reference 完整 | PASS |
| K1-C4 | Track 2 六族 PAIRED；Track 1 NOT MATCHED 保留 | `AE-Track2` 通过；Track 1 历史不改 |
| K2-C1 | 武器/手/火把/初始相位已记；护甲 42 槽/存档身份缺 | **PARTIAL** |
| K2-C2 | 候选输入账本/源侧参考序列分开登记 | REGISTERED |
| K2-C3 | 冻结 1H+左手 9：1HM_Equip/Unequip，无 H2H | PASS |
| K2-C4 | CAL2 三进程 C2-A..F | **grid READY** |
| K3 | v98 249 条候选 + v99b 1,151 条源；双读/116 轨/零溢出 | PASS（两侧各自） |
| K4 | v97d B3/B4 739/739 | PASS（单窗口/单会话边界） |
| K5-1..8 | Track 2 v103 两侧全 PASS；声明缺口登记 | PASS（Track 2） |
| K5-5 | 4 审计点齐；`hkbBlendingBehavior` 等无可观测布局 | **PASS 带缺项清单** |
| K6 | 仪器范围/控制清单/版本/预算 | CLOSED（限定范围） |

**K1–K6 结论**：形式证据齐备；K2-C1 PARTIAL、K5-5、A17 无独立控制是实质缺口，不能写成完整 CAL1/G5 READY。

## 三、原始工件、尝试与对账

- CAL2 三次独立进程（2026-11-15 20:59/21:29/21:35）：每进程 121 代、121/121 双读一致、`close=complete`；三对跨进程 119,790/119,790 逐位相同；C2-D PASS。
- CAL1-B：三进程 CGRD + 121×99 truth/BOUNDS；C1B-A..F PASS；C2-C 119,790 分量 0 violation、最大比值 0.125；N 的原始变换位值三进程逐位相同（旋转归一化残差 N_theta≈5.96e-8 rad）。
- 初次 CAL1-B 使用修复前 BOUNDS，Q 出现 4e16，按停止规则作废；修复后重转/重发 C2/重跑，结果见 [CAL2 §十三](../g5-cal2-20261115/README.md#cal2-bounds-fix)。
- CAL1-C：COV 247 对、99/17 双账本；CLOCK 247 对、G4C-1..6 PASS；TIME 247 对、dt_exclude 0/247。
- 所有工件哈希在对应 §十/§十一/§十六/§十七/§十八登记；预算未超。

## 四、Q/N/tau/epsilon 与范围

| 量 | 未舍入 | 6 位向上 | 范围 |
| --- | --- | --- | --- |
| `tau_p` | `4.765916457818655e-4` | **`0.000476592`** | 控制 B 单层 P99 |
| `tau_r` | `6.033405486327581e-8` | **`6.03341E-8`** | 同上；`tau_theta=6.947463158790921e-4` |
| `tau_s` | `3.862634230245021e-6` | **`0.00000386264`** | 同上 |
| `epsilon_t` | `0.0333959631725` | **`0.0333960`** | T1 invariant TSC + 全局 max frameDuration |
| `dt_exclude` | — | `true`，阈值 `epsilon_t`，2% edge-only | 0/247 dry-run |

**不覆盖**：K5-5 层/权重/混合、K2-C1 完整清单、A17 独立控制、非 `female\mt_idle.hkx` 资源、其他环境、G5 等价。

## 五、唯一裁定

### `CALIBRATED (limited: control-B P99 pose + time + clock axes)`

- 通过：映射/身份/计数/控制/校准证据齐备（对上述范围）；Q/N/tau/epsilon/clock 可独立复算。
- 不通过/仍开放：完整 CAL1、门 3、K5-5、K2-C1、A17 独立控制、G5、G6。
- 本裁定不是 `CALIBRATED` 全量，也不是 `G5 PASSED`；门 4 可据此按 §六“显式带缺项冻结”处置 §六阻塞。

## 六、对门 4 条件 2 的意义

CAL v1 §九允许 `CALIBRATED（仅列明范围）` 并必须列出 G5/门 3 未闭合项。本文满足该形式：P99 姿态/时间/时钟轴的联合证据通过；K5-5/K2-C1/A17 作为显式缺口传递到门 4 §六，不阻塞门 4 的 P99 轴冻结，但阻塞 G5 级裁定。
