# 会话总结：2026-11-15 CAL2 三进程 → CAL1-B → CAL1-C 审计（26 个提交）

> **2026-11-16 后续**：COV/TIME 草案已评审通过并实现。COV：`gate4_pair.py --coverage` 的 metadata-only dry-run 为 247 对、P99=99/A17=17、`pose_decoded=False`；TIME：T1 机器级 invariant TSC + 全局 max frameDuration 产出 **`epsilon_t=0.0333960` s**（`epsilon_read=0.0000626282`、`epsilon_grid=0.0333333351`），`dt_exclude` dry-run **0/247**；门 4 姿态/时间/时钟轴槽位已填（CLOCK_READY）；A17 审计为近似静态共享状态（span ≤3.8e-6，跨侧 `E_p≤1.97e-6`），无独立 clip 控制；CAL v1 §九联合验收以 `CALIBRATED (limited)` 通过，**门 4 FROZEN（限定范围）**，门 5 采集授权但未运行；K5-5/K2-C1/G5 仍开放。

**范围**：从 `90d7f24`（上一份会话总结的修正提交）之后开始，至 `d81d7b9`，共 **26 个提交**；本总结为第 27 个提交。分支 `oar-idle3`，远端 `origin`（PlayerPanel）。本会话完成 CAL2 三进程裁定、CAL1 资格评审、CAL1-B 有界校准、真值 BOUNDS 混合轨修复、CAL1-C 覆盖/时间审计，并起草两份门 4 修订草案。

## 一、CAL2：三进程完成，K2-C4 升 grid READY

| 项 | 结果 |
| --- | --- |
| run 1/2/3 | 2026-11-15 20:59 / 21:29 / 21:35（UTC 工件 125913Z / 132909Z / 133459Z）；均 121 代 `close=complete` |
| C2-A..F | 三进程全部 PASS；C2-E 唯一 `[E]` 为 [§十二](../diagnostics/g5-cal2-20261115/README.md#cal2-c2e-oar-addendum) 的 OAR 装载期精确豁免 |
| 跨进程 | 三对均 119,790/119,790 分量逐位相同；`nonzero=0`、`max_abs=0`、`max_ulp=0` |
| C2-D | **PASS（3/3）**；总体 **READY** |
| 结果 | **K2-C4 从 bounded READY 升为 grid READY**；CAL1 控制 B 资格就绪 |

工件：[三进程裁定](../diagnostics/g5-cal2-20261115/batch-3of3-adjudication/cal2-adjudication-summary.txt)（JSON SHA-256 `B670782A…C54A7`）。

## 二、CAL1 资格评审：完整 CAL1 仍 NOT_READY

[评审记录](../diagnostics/g5-cal1-qualification-20261115/README.md)：K1–K6 形式证据齐备，但完整 CAL1 / 门 3 不能开：

- K2-C1 仍 PARTIAL（护甲 42 槽/存档身份缺失）；
- K5-5 层/权重/混合有显式缺项（`hkbBlendingBehavior` 等无 CommonLibSSE 布局）；
- `epsilon_t` 的 TSC 频率/单位未在归档运行中逐进程登记。

Track 2 已把 K1-C4 反转为六族 PAIRED；Track 1 的 NOT MATCHED 保留为历史。

## 三、CAL1-B：控制 B 单层姿态 CALIBRATED_BOUNDED

[预注册与结果](../diagnostics/g5-cal1b-20261115/README.md)：预注册经用户批准，工具 `tools/cal1b_calibrate.py` + 32/32 离线测试通过；对三份归档 CGRD 正式运行。

### 3.1 真值 BOUNDS 修复（本会话关键发现）

第一次 CAL1-B 试跑得到 `Q_p=4.14e16`/`Q_s=2.94e16`。原因是 HkxTruth `DumpBounds` 把混合轨（部分 dynamic、部分 static）的 `SplineDynamicTrackVector` 强转成 `SplineStaticTrack`，读到了向量/指针内部值。修复后：

- 只改本地 gitignored 真值工具；解码/时间映射未改，插件零改动；
- TRACK/TIME/`BITS_FNV1A64 BE67D994E6042E31` 与旧真值逐行相同；仅 BOUNDS 变化；
- 真值 SHA-256 从 `5e20fcaa…b4e3` 变为 **`de26eceb…c387`**；
- 三进程 C2 用修复后真值重裁：C2-C 仍 **0 violation**、最大比值 **0.125**，C2-D PASS；C2 JSON/summary 重发。详见 [CAL2 §十三](../diagnostics/g5-cal2-20261115/README.md#cal2-bounds-fix)。

### 3.2 CAL1-B 结果

| 量 | Q | N | tau（6 位向上） |
| --- | --- | --- | --- |
| 位置 | `4.765916457818655e-4` | `0` | **`0.000476592`** |
| 旋转 | `Q_theta=6.946867112343167e-4`；`Q_r=6.0323702699705e-8` | `N_theta=5.960464477539063e-8`；`N_r=4.440892098500626e-16` | **`tau_r=6.03341E-8`**；`tau_theta=6.947463158790921e-4` |
| 缩放 | `3.862634230245021e-6` | `0` | **`0.00000386264`** |

逐轨范围：`tau_p` `3.30363e-6..4.76592e-4`、`tau_r` `6.03238E-8..6.03341E-8`、`tau_s` `1.90735e-6..3.86264e-6`。N 的原始变换位值三进程逐位相同；旋转的 `N_r/N_theta` 是归一化/点积浮点残差，按定义保守保留。

门 4 姿态槽位已按 [§十六](../diagnostics/g5-gate4-20261115/README.md#gate4-cal1b-slots) 填充，但只覆盖 99 条 MT_Idle 轨；`epsilon_t`/`Δt_clock` 仍 PENDING，门 4 未完整冻结。

## 四、CAL1-C：17 辅助轨与时间模型审计

[审计记录](../diagnostics/g5-cal1c-coverage-timing-20261115/README.md)：

### 4.1 17 条额外轨

99..115 = 6 item（`NPC L/R Item01..03`）+ 10 wing（`Left/RightWing1..5`）+ `Belly`。K5 Track 2 显示它们随装备/翼骨变化，且 wing 与 K1-C3 reference 零匹配；**不是 CAL1-B 的 99 条控制轨**，不能套 CAL1-B tau。完整审计：[aux_tracks.txt](../diagnostics/g5-cal1c-coverage-timing-20261115/aux_tracks.txt)。

### 4.2 TSC 与 `epsilon_t`

- 本机 TSC **invariant=1**；QPC 10 MHz；TSC 中位 **4,291,863,648 Hz**（约 4.291864 GHz）；日志斜率交叉核对 <0.2%。
- 三份 CGRD 的最大读取区间 37.8/23.9/62.6 µs；门 4 `dt_clock` median 4.7 ms、p95 11 ms、max 17.8 ms。
- 因此 **`epsilon_t` 不能直接取读取区间**；需要 `epsilon_read` + 帧量化/相位归属的独立时间模型。
- 归档运行没有逐进程 TSC 频率字段；T1（机器级 invariant TSC + 披露）与 T2（v105 逐进程登记/重采）待选。

## 五、两份门 4 修订草案（待评审）

1. [C-G5-G4-COV v1](../diagnostics/g5-g4-cov-20261115/README.md)：99 主分母 + 17 辅助账本；A17 不套 CAL1-B tau、不判 PASS/FAIL、不声称 116/116；`gate4_pair.py` 需双账本输出。
2. [C-G5-TIME v1](../diagnostics/g5-time-20261115/README.md)：`epsilon_read` + `epsilon_grid`（所有配对 clip 的 authored frameDuration max）+ `epsilon_t`；T1/T2 待选；`dt_exclude` 上限 2% 待评审；不把观测 `dt_clock` 当输入。

**两项均未实现、未填 `epsilon_t`。**

## 六、当前状态与下一步

- 分支 `oar-idle3`，本总结提交前领先 `origin` **116 个提交**；远端 `github.com/AmazingBuff/PlayerPanel.git`；本总结提交并 push 后应同步。
- 工作树 clean；未 force push、未 rebase。
- 未产生 `epsilon_t`；完整 CAL1/门 3、G5、G6 仍未关闭；K5-5/K2-C1 仍开放。
- 待用户决策：COV 方案 A；TIME 的 T1/T2；frameDuration 全局 max；2% `dt_exclude` 上限。
- 若批准 COV/TIME：实现覆盖双账本 + 帧表/时间工具，重跑 metadata-only dry-run，联合冻结门 4；之后才有门 5。

## 七、提交与工件

- 提交范围：`5a9f2e2`（CAL2 裁定工具）… `d81d7b9`（COV/TIME 草案），共 26 个提交；本总结为第 27 个。
- 关键工件：
  - [CAL2 三进程 summary](../diagnostics/g5-cal2-20261115/batch-3of3-adjudication/cal2-adjudication-summary.txt)
  - [CAL1-B JSON](../diagnostics/g5-cal1b-20261115/calibration/cal1b-calibration.json)（SHA-256 `8B2077BF…C740865`）
  - [CAL1-B CSV](../diagnostics/g5-cal1b-20261115/calibration/cal1b-tracks.csv)
  - [CAL1-C TSC 探针](../diagnostics/g5-cal1c-coverage-timing-20261115/tsc_probe.txt)（SHA-256 `50544EFC…E479E`）
  - [CAL1-C 辅助轨审计](../diagnostics/g5-cal1c-coverage-timing-20261115/aux_tracks.txt)（SHA-256 `F950E378…784A6BBD`）
- 工具身份：`cal2_adjudicate.py` `F2225EAC…`；`cal1b_calibrate.py` `981CB741…`；`cal1b_calibrate_test.py` `A2542FAF…`。
