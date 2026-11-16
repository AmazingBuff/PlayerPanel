# C-G5-G4-CLOCK v1：门 4 时钟轴判据预注册（2026-11-16，评审通过）

**状态：评审通过（2026-11-16，用户授权按推荐结构执行）；metadata-only 实现与 dry-run 完成（CLOCK_READY）。** 本文只定义门 4 时钟轴判据；不产生 tau/epsilon、不解码 pose。

门 4 §五的“时钟轴判据”是最后一个 PENDING-CAL 结构槽位。本文落实其“逐实例/逐域、离散判据 + 数值允差”结构，使用 dry-run 已登记的 candidate/source clip `localTime` 与 `dt_clock`，不使用姿态数据。

## 一、范围与输入

| 项 | 输入 |
| --- | --- |
| 配对元数据 | `gate4-pair-manifest.json`（Track 2 dry-run；247 对） |
| 时间界 | [C-G5-TIME v1](../g5-time-20261115/README.md#time-results)：`epsilon_t=0.0333960 s` |
| 速率证据 | [E2-C](../independent-behavior-graph.md#c-g5-e2-c)：源/候选 clip 速率 `1.000 ± 0.017` |
| 域 | `1hm_equip`、`1hm_unequip`、`1hm_idle`（`mt_idle/mlh_idle/dummy` 在 Track2 无配对域） |

## 二、逐域定义

对每个有配对的域 `f`：

```text
span_c = max(clock_c) - min(clock_c)
span_s = max(clock_x) - min(clock_x)
rate = span_c / span_s
endpoint_diff = max(|min(clock_c)-min(clock_x)|, |max(clock_c)-max(clock_x)|)
dt_max = max |clock_c - clock_x|
```

- 候选/源分别按 `cand.record` / `src.seq` 排序；检查 `localTime` 非递减。
- 源侧是一对多（同一源记录可服务多条候选），等值计数按**唯一源 seq**统计，不按配对行重复计数。
- 候选阶梯（相邻 candidate record 的 `localTime` 相等）**只登记，不判失败**；它反映 1/60 驱动下的追赶式采样，不是速率差。
- 端点/`dt` 使用 `epsilon_t`；速率使用 E2-C 的 `±0.017`。

## 三、判据（预注册，不得事后放宽）

| # | 判据 | 通过条件 | 否则 |
| --- | --- | --- | --- |
| G4C-1 域完整性 | 247 对全部有有限 `clock_c/clock_x/dt_clock`；每个有配对域都被分析；无 no-clock/no-partner/未声明域 | 全部满足 | INVALID |
| G4C-2 速率 | 每个域 `|rate-1| ≤ 0.017` | 全部满足 | FAIL（该域） |
| G4C-3 端点 | 每个域 `endpoint_diff ≤ epsilon_t` | 全部满足 | FAIL（该域） |
| G4C-4 `dt` | 每个域 `dt_max ≤ epsilon_t`；全部配对保留；edge 对全部记账 | 全部满足 | FAIL/INVALID |
| G4C-5 单调/回绕 | 候选/源唯一记录 `localTime` 非递减；域 `wrap_cycles` 全 0 | 全部满足 | INVALID |
| G4C-6 阶梯登记 | 候选相邻等值计数逐域登记；不删除/不归零；源唯一 seq 等值计数单列 | 全部满足 | INVALID |

**裁定语义**：G4C-1..6 全过 = 门 4 时钟轴结构可执行且本 dry-run 域通过；这只填时钟轴判据，不等于门 4 完整冻结（K5-5/K2-C1/17 辅助轨仍开放）。

## 四、实现与输出

| 项 | 预注册 |
| --- | --- |
| 工具 | `tools/gate4_clock.py`，纯 stdlib；读 manifest + TIME result；不解码 pose |
| 输出 | `gate4-clock.json`（逐域 span/rate/endpoint/dt/阶梯/判据）、`gate4-clock-summary.txt`（≤200 行） |
| 测试 | 合成 manifest：速率越界、端点越界、dt 越界、单调回退、源一对多等值计数、阶梯不判失败 |
| 身份 | 工具/测试/输出 SHA-256 使用前登记 |
| 预算 | JSON ≤1 MiB；summary ≤200 行；超限即停 |

## 五、执行顺序

1. 本文评审/批准。**已批准。**
2. 实现 `gate4_clock.py` + 测试，登记 SHA-256。**已完成。**
3. 对 Track 2 dry-run manifest 执行 metadata-only，登记逐域结果。**已完成，见 §八。**
4. 通过后填门 4 §五时钟轴槽位；仍不冻结门 4，门 5 不授权。**已填时钟轴；门 4 仍因 K5-5/K2-C1/17 辅助轨未冻结。**

## 六、边界

- 不使用 pose；不做同刻逐代配对；不改变 247 对或 99/17 覆盖。
- 候选阶梯与源一对多等值不是失败；速率/端点/dt 才是判据。
- 不产生 `tau_*`/`epsilon_t`；不关闭 K5-5/K2-C1/G5/G6。

## 七、dry-run 预期读数（执行前登记，不是裁定）

`1hm_equip` rate 0.995882、`1hm_unequip` 0.991083、`1hm_idle` 0.993000；三域 `dt_max` 最大 0.017833 s；端点最大差 ≤0.017833 s。所有值预期落在本判据内；正式裁定由工具输出，不使用本节手算值。

## 八、实现与 dry-run 结果（2026-11-16）

| 项 | 值 |
| --- | --- |
| 工具 | `tools/gate4_clock.py`，SHA-256 `6E21BB47DF36A17ABDBFBFBC98323A237D5EF75774339CDD3665E1CEC4A41780`；测试 9/9 PASS，SHA-256 `D9630C58C41171328C1C0603AEC94B9B1F43922B711CA51EB3224F25D84D3380` |
| dry-run | 247 对、3 个有配对域；`pose_decoded=false`；全部判据 **G4C-1..6 PASS**，**VERDICT CLOCK_READY** |
| 逐域 | `1hm_equip` rate 0.995882、endpoint 0.005833、dt 0.011000；`1hm_unequip` 0.991083/0.017833/0.017833；`1hm_idle` 0.993000/0.005667/0.005667 |
| 阶梯 | candidate 相邻等值 83/95、109/128、19/21；source 唯一 point 等值 0/16、0/20、0/2；按预注册只登记、不判失败 |
| 工件 | [gate4-clock.json](dry-run/gate4-clock.json)（2,330 B，SHA-256 `15F5EACD…1A4A76`）与 [summary](dry-run/gate4-clock-summary.txt)（706 B，SHA-256 `F6EED377…C11774`） |

**裁定**：时钟轴结构可执行且本 dry-run 域通过；只填门 4 时钟轴槽位，不改变姿态/时间数值，不关闭 K5-5/K2-C1/17 辅助轨/G5。
