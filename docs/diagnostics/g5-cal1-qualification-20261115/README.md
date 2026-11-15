# C-G5-CAL1 资格评审（2026-11-15）

**状态：评审完成。裁定：完整 CAL1 / 门 3 原生校准 NOT_READY；控制 B 单层姿态的有界 CAL1-B 已完成 **`CALIBRATED_BOUNDED`**（`tau_p=0.000476592`、`tau_r=6.03341E-8`、`tau_s=0.00000386264`），`epsilon_t` 未产生。**

本轮只做资格评审，不采集新数据、不改判据、不产生容差。输入为 [CAL v1 §七/§八](../../independent-behavior-graph.md#c-g5-cal-v1)、K1–K6 的既有登记、[CAL2 三进程裁定](../g5-cal2-20261115/README.md#cal2-run3-final) 与三份 CGRD v1 工件。

## 一、评审问题与判据

[CAL v1 §八](../../independent-behavior-graph.md#c-g5-cal-v1) 的 CAL1 资格要求 **K1–K6 都有明确证据与实际控制清单**；不满足则 NOT_READY，不开原生校准。CAL6 还要求 Q/N、旋转角合成、向外舍入、时间界与适用范围可独立复算；CAL7 要求只报告本契约范围，不得用校准数据做 G5 验收。

本轮回答两个问题：

1. 现有 K1–K6 证据是否足以开启**完整** CAL1 / 门 3 原生校准？
2. 若不能，是否存在一个可预注册、范围明确、不偷渡 K5-5 的**有界** CAL1 子集？

## 二、K1–K6 证据矩阵

### K1 身份、资源与清单

| 项 | 最新证据 | 状态 | 对 CAL1 的含义 |
| --- | --- | --- | --- |
| K1-C1 完整运行资源清单 | [K1-C1 登记](../g5-k1-c1-20261114/README.md)：行为闭包 23 文件、全量 OAR/loose 路径、24,204 生效路径、锚点 2/2、观测 FNV 全部裁决 | **REGISTERED**（本机/本 modlist/此时点） | 资源清单可复核；不表示 OAR 已冻结或跨运行不变 |
| K1-C2 图身份 | [v101/v101b census](../g5-k1-census-20261114/README.md)：graph/holder/root/project/behavior/setup/bindings/poseLocal 等 | **PASS**（census 仪器） | 运行实例身份可复核；不证明 controller 所有权 |
| K1-C3 骨架与映射 | 116 骨、父索引、reference pose、float slots；`recorded==bones`、`invalid-parent=0`、`nonfinite=0` | **PASS** | 骨映射可用于同资源控制；不覆盖逐轨 default-fill 语义 |
| K1-C4 资源与 clip 实例 | Track 1：六族中五族 `data FNV` 不同 → **NOT MATCHED**；Track 2（OAR 关闭）：[六族全 PAIRED](../g5-track2-20261114/README.md) | **Track 1 历史 NOT MATCHED；`AE-Track2` 环境 PASS（六族 PAIRED）** | 控制 B 在 `AE-Track2` 下可用同一资源对照；Track 1 结论不回写 |

### K2 输入、控制与初态

| 项 | 最新证据 | 状态 | 对 CAL1 的含义 |
| --- | --- | --- | --- |
| K2-C1 装备初态 | [K2-C1/C2 登记](../g5-k2c1c2-20261114/README.md)：武器/手/火把/初始相位已记录；**护甲 42 槽与存档身份仍缺** | **PARTIAL** | 完整 CAL1 的“实际控制清单”仍不完整；控制 B 单层采样不依赖该装备链 |
| K2-C2 输入账本 | 候选注入序列（写/读回/事件/步数/settle）与源侧参考序列分开登记；冻结场景可复核 | **REGISTERED** | 输入账本可复核；源/候选输入等价性仍是已知被测差异 |
| K2-C3 片段一致性 | 冻结 1H + 左手 9 场景：`1HM_Equip` / `1HM_Unequip`，无 `H2H_*` | **PASS** | 片段族对齐；不证明完整姿态 |
| K2-C4 独立控制 | [CAL2 三进程](../g5-cal2-20261115/README.md#cal2-run3-final)：C2-A..F 全过，三进程两两 119,790/119,790 逐位相同 | **grid READY** | 控制 B 资格闭合；不产生 `tau_*` |

### K3–K6

| 项 | 最新证据 | 状态 | 对 CAL1 的含义 |
| --- | --- | --- | --- |
| K3 同代快照 | 候选 [v98](../../build.md#g5-k3c-v98-run1-20261113) 249 条；源 [v99b](../../build.md#g5-k3s-v99b-run1-20261113) 1,151 条；双读一致、116 轨、零溢出 | **PASS（两侧各自）** | 不覆盖跨侧逐代配对（K3 收官已裁定另立契约） |
| K4 源后处理边界 | [v97d](../../build.md#g5-e2b-b3b4-v97d-run1-20261113) B3/B4 739/739、`max-delta=0` | **PASS（单窗口/单会话边界）** | 不覆盖多窗口/多装备重复 |
| K5-1..K5-8 完整计数 | [K6 §十一](../g5-k6-plan-20261114/README.md) Track 1 两侧全 PASS；[Track 2](../g5-track2-20261114/README.md) 两侧 K5-1..8 全 PASS | **PASS（Track 2，含声明缺口）** | 计数/双读/预算/运行完整性可复核 |
| K5-5 层/混合资格 | 4 审计点齐、`missing=0 overflow=0`；`hkbBlendingBehavior` 等无 CommonLibSSE 布局 → **逐骨/通道权重、blend/mask/additive 语义为显式缺项** | **PASS（带缺项清单）** | **完整姿态等价/混合语义仍不可裁**；控制 B 单层直采不受该缺项影响 |
| K6 仪器资格 | [K6 关闭（限定范围）](../g5-k6-plan-20261114/README.md)：仪器范围、控制清单、采集/分析版本、预算 | **CLOSED（限定范围）** | 形式资格齐备；K6 §13 明示 K5-5 与 K1-C4 是实质限制 |

## 三、裁定

### 3.1 完整 CAL1 / 门 3 原生校准：NOT_READY

三项实质缺口，任一未关闭都不能把完整 CAL1 说成 READY：

1. **K2-C1 PARTIAL**：装备初态仍缺护甲 42 槽与存档身份；完整 CAL1 的“实际控制清单”不完整。
2. **K5-5 显式缺项**：`hkbBlendingBehavior` 等无 CommonLibSSE 布局，逐骨/通道权重与 blend/mask/additive 语义不可观测；完整姿态/混合等价不能裁。
3. **`epsilon_t` 的 TSC 单位/频率未登记**：CGRD v1 只存四点原始 `__rdtsc` 值与日志差值；[CAL2 §五.4](../g5-cal2-20261115/README.md) 要求频率/单位随进程登记，但 v104 工件与日志没有该字段，因此不能把 cycles 换算为秒，也不能完成 CAL6 的时间界独立复算。

[K6 §13](../g5-k6-plan-20261114/README.md) 所称“CAL1 资格门形式上齐备”只表示 K1–K6 均有登记，不等于实质 READY；该节本身同时把 K1-C4 与 K5-5 列为实质限制。Track 2 已反转 K1-C4，K5-5 仍开放。

**因此：不产生 `tau_p/tau_r/tau_s/epsilon_t`；不冻结门 3；不进入门 4 完整冻结或门 5。**

### 3.2 有界 CAL1-B（控制 B 单层姿态）：已完成

控制 B 的数据形状与缺口是分离的：

| 项 | 现有材料 | 结论 |
| --- | --- | --- |
| 资源/路径 | `female\mt_idle.hkx`；121 时间 × 99 轨 × 10 分量；单层直接 `SamplePartialTracks`；`AE-Track2` | 范围明确 |
| 重复性 N | 三进程两两 119,790/119,790 逐位相同；`max_abs=0`、`max_ulp=0` | N 的原始材料 = 0（仍需预注册确认 N 的取法与 Q 的组合） |
| 表示/解析界 Q | B2 逐分量解析界表（rotation 40-bit / dynamic 16-bit / static / identity）；C2-C 最大比值 0.125、0 violation | Q 可复算；0.125 只是可行性读数，不是容差 |
| 姿态度量 | CAL v1 §六：位置欧氏距离、旋转 `1-|dot|` + 角度空间、缩放 max-abs | 可在预注册中固定 |
| `epsilon_t` | 只有原始 cycles；无频率/单位 | **有界 CAL1-B 暂不含 `epsilon_t`**；时间槽位保持开放 |
| K5-5 | 层/权重/混合缺项 | **明确排除**；不得用控制 B 推断混合/层语义 |

**结论：[`C-G5-CAL1-B v1`](../g5-cal1b-20261115/README.md) 已于 2026-11-15 评审通过并完成正式运行**：只推导控制 B 单层姿态的 `Q_p/Q_r/Q_s` 与 `N_p/N_r/N_s`，再按 CAL v1 §七 的 `round_up` 与旋转角合成给出 `tau_p/tau_r/tau_s`；`epsilon_t`、K5-5、K2-C1 完整清单与 G5 等价裁定明确排除。结果 `CALIBRATED_BOUNDED`：`tau_p=0.000476592`、`tau_r=6.03341E-8`、`tau_s=0.00000386264`；门 4 姿态槽位已按 [§十六](../g5-gate4-20261115/README.md#gate4-cal1b-slots) 填充。

## 四、建议顺序

1. **已完成**：[`C-G5-CAL1-B v1`](../g5-cal1b-20261115/README.md) 已产出控制 B 单层姿态的 `tau_p/tau_r/tau_s` 并填入姿态槽位；`epsilon_t` 仍开放。
2. 并行关闭完整 CAL1 的剩余缺口：K2-C1 护甲/存档身份清单；K5-5 的可观测替代或显式范围裁定；TSC 频率/单位登记（新仪器批次或运行记录扩展）。
3. `epsilon_t` 与 K5-5 未闭合前，门 4 不得完整冻结，门 5 不得启动；`tau_*` 不得被写成 G5 通过。

## 五、边界

- 本评审不采集新数据、不改 K1–K6 判据、不改 CAL2 工件。
- 三进程逐位相同只证明该资源/网格/单层路径的重复性；不证明其他资源、层/权重/混合、时间量化或完整姿态等价。
- Track 2 的 K1-C4 反转只在 `AE-Track2` 环境成立；Track 1 的 NOT MATCHED 保留为历史。
- `tau_p/tau_r/tau_s/epsilon_t = 未产生`；G5 未冻结/未通过，G6 后置。
