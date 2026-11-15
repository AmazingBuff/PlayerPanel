# C-G5-TIME v1：门 4 时间模型与 `epsilon_t` 预注册（2026-11-15 草案，待评审）

**状态：草案，待评审。本文只固定时间模型与判据；不预填 `epsilon_t`，不改变姿态 tau，不实现工具。**

本草案落实 [CAL1-C 时间审计](../g5-cal1c-coverage-timing-20261115/README.md)：本机 TSC invariant 已证、频率约 4.291864 GHz，但控制 B 的读取区间约 24–63 µs，门 4 的 `dt_clock` 是 5–18 ms。**`epsilon_t` 不能直接取读取区间**，必须把时间表示/读取、帧量化/相位归属、`Δt_clock` 处置分开。

## 一、范围与输入

| 项 | 输入 | 范围 |
| --- | --- | --- |
| TSC | 三份 v104 CGRD 的四点 TSC；C2-F 已证单调 | 同机、同 CPU、`AE-Track2`、3 进程 |
| 本机频率 | [TSC 探针输出](../g5-cal1c-coverage-timing-20261115/tsc_probe.txt)：invariant=1、QPC=10 MHz、median≈4.291864 GHz | 机器级；归档运行没有逐进程频率字段（见 §三） |
| 帧量化 | `mt_idle.hkx` frameDuration=0.0166777782；门 4 其余 clip 的 authored frameDuration 需逐一登记 | 帧表完成前不产生 `epsilon_t` |
| 相位差 | 门 4 dry-run 的 `dt_clock` 分布（median 4.7 ms、p95 11 ms、max 17.8 ms） | 只作为待处置量与旁证，不作为 epsilon 数值来源 |

## 二、时间分量定义

### 2.1 `epsilon_read`：TSC 表示 + 读取/采样区间

对每个进程 `r`、代 `k`：

```text
read_cycles(r,k) = tsc_after_compare - tsc_call_start
epsilon_read = round_up_6sig( max_{r,k}(read_cycles)/f_tsc + 1/f_tsc + delta_cal )
```

- `f_tsc` = 机器级 invariant TSC 频率；`delta_cal` = 频率标定的相对不确定度乘最大读取区间（保守取 `1e-6 * max_interval`，再计入一个 TSC tick）。
- 三份归档的 max read_cycles = 162,196 / 102,469 / 268,790；用 4.291864 GHz 换算约 37.8/23.9/62.6 µs。
- `epsilon_read` 只覆盖“输入锚点→完成输出→读取比较”的区间；**不包含帧量化或源/候选相位差**。

### 2.2 `epsilon_grid`：帧量化/相位归属

门 4 的配对单位是同一 clip 相位的两侧完成输出；相位的最小可分辨单位是该 clip 的 authored frameDuration，不是控制 B 的 1/60 网格。定义：

```text
epsilon_grid = max(frameDuration(clip) for clip in paired_clips)
```

- `mt_idle.hkx`：0.0166777782 s（Havok spline frameDuration，已登记）。
- 其余门 4 clip（至少 `1HM_Equip`、`1HM_Unequip`、`1HM_Idle`、`MLh_Idle`）必须逐一从 authored HKX 登记 frameDuration；未登记 → INVALID，不猜 1/60 或 1/30。
- **不使用 dry-run 的观测 `dt_clock` 最大值当 `epsilon_grid`**；观测值只用于事后对账/停点判断。

### 2.3 `epsilon_t` 与 `Δt_clock` 处置

```text
epsilon_t = round_up_6sig(epsilon_read + epsilon_grid)
```

- `epsilon_t` 是门 4 的时间等价界；`gate4_pair.py --tolerances` 的 `dt_exclude=true` 用 `dt_clock > epsilon_t` 排除配对。
- 被排除的配对**保留在账本**，不删帧、不从姿态误差分布中静默消失；排除比例 > 2% 或出现非 edge 对 → 停止并审计（2% 是预注册阈值，不是观测值）。
- `Δt_clock` 不传播为姿态噪声；若要把时间差转成运动误差，须另立速度/插值上界证明（CAL v1 §七），本协议不做。
- 本协议不把 `epsilon_t` 加进 `tau_p/tau_r/tau_s`；两者独立。

## 三、TSC 来源：T1 / T2

| 路径 | 定义 | 边界 |
| --- | --- | --- |
| **T1（推荐先评审）** | 机器级 invariant TSC 探针 + 三进程 C2-F 单调性 + 日志斜率交叉核对；登记 `f_tsc`、探针输出哈希、非逐进程字段披露 | 只适用于同机、invariant TSC、归档 v104 批次；未来批次须重新登记 |
| T2 | v105 仪器在进程启动/关闭登记 `f_tsc`、单位、单调基准；重采三进程 | 最严格；需代码/构建/部署/上机与新批次 |

T1 的披露必须写明：归档运行没有 CAL2 §五.4 要求的逐进程频率字段；T1 用机器级 invariant TSC 补证，不把日志毫秒斜率当正式频率。若评审不接受 T1，则本协议不产生 `epsilon_t`，改走 T2。

## 四、判据（预注册，不得事后放宽）

| # | 判据 | 通过条件 | 否则 |
| --- | --- | --- | --- |
| TIME-1 TSC 资格 | CPUID invariant=1；QPC 频率可读；探针 3 次频率相对跨度 ≤1e-6；三进程 C2-F TSC 单调 | 全部满足 | INVALID |
| TIME-2 频率登记 | `f_tsc`、单位、探针输出/源码/运行命令哈希登记；T1 披露逐进程缺失 | 全部满足 | INVALID |
| TIME-3 读取区间 | 3×121 个 `read_cycles` 完整、有限、非负；`epsilon_read` 按 §2.1 复算 | 全部满足 | INVALID |
| TIME-4 帧表 | 门 4 全部配对 clip 的 authored frameDuration 逐一登记；无猜测/缺项 | 全部满足 | INVALID |
| TIME-5 合成 | `epsilon_t=round_up_6sig(epsilon_read+epsilon_grid)`；未舍入值保留；不把 dt_clock 观测当输入 | 全部满足 | INVALID |
| TIME-6 排除纪律 | 被排除配对全部保留/记账；排除比例 ≤2%；无非 edge 排除 | 全部满足 | INVALID |
| TIME-7 范围 | 只用于门 4 时间等价；不传播为姿态 tau、不做 G5 姿态等价 | 全部满足 | INVALID |

**裁定语义**：TIME-1..7 全过 → 可把 `epsilon_t` 填入门 4 时间槽位（限定范围）；仍不表示门 4 完整冻结，直至 [C-G5-G4-COV v1](../g5-g4-cov-20261115/README.md) 的覆盖结构同时成立。

## 五、实现与工件

| 项 | 预注册 |
| --- | --- |
| 工具 | `tools/cal1c_time.py` 或 `gate4_pair.py` 的冻结扩展；纯 stdlib；读 CGRD + 帧表 + 探针输出 |
| 输出 | `cal1c-time.json`（f_tsc、epsilon_read、frame table、epsilon_t、排除账本）、summary ≤200 行 |
| 测试 | 合成 TSC/帧表测试；边界：TSC 非单调、帧表缺项、dt 排除 >2%、非 edge 排除 |
| 预算 | JSON ≤2 MiB；summary ≤200 行；超限即停 |
| 身份 | 工具/测试/输出 SHA-256 使用前登记 |

## 六、待评审决策

1. TSC 路径：T1（机器级 invariant + 披露）还是 T2（v105 逐进程）？
2. 帧表来源：从 authored HKX 逐一解码 frameDuration，是否接受 `mt_idle` 已有值 + 其余 clip 新审计？
3. `epsilon_grid` 用全局 max frameDuration 是否足够保守；是否要按 clip 分列？
4. 2% 的 `dt_exclude` 上限是否合适；是否要求所有排除都是 edge 对？
5. 是否与 `C-G5-G4-COV v1` 联合冻结，还是先各自评审？

**本协议不产生 `epsilon_t`；批准前不实现、不填值。**
