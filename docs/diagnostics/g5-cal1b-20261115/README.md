# C-G5-CAL1-B v1：控制 B 单层姿态有界校准预注册（2026-11-15 草案，待评审）

**状态：草案，待评审。本文只固定方法与范围，不产生任何 Q/N/`tau_*` 数值。**

本预注册是 [CAL1 资格评审](../g5-cal1-qualification-20261115/README.md) §3.2 的落地：完整 CAL1 / 门 3 仍 NOT_READY（K2-C1 PARTIAL、K5-5 缺项、`epsilon_t` 的 TSC 频率/单位未登记），但控制 B 的单层直接采样路径具备独立真值、三进程重复性和明确的解析界，可以单独校准姿态测量容差。

## 一、目标、范围与不覆盖

| 项 | 本协议交付 | 明确不覆盖 |
| --- | --- | --- |
| 姿态容差 | 控制 B 单层姿态的 `Q_p/Q_r/Q_s`、`N_p/N_r/N_s` 与 `tau_p/tau_r/tau_s`，附未舍入值、舍入规则、逐轨/全局分布 | 完整姿态等价、G5 通过、门 3 整体通过 |
| 资源/路径 | `female\mt_idle.hkx`（113,920 B / `0EF8248C…BCD73C7`）；121 时间 × 99 轨 × 10 分量；单层直接 `SamplePartialTracks` | 其他动画、其他 binding、其他层/混合、OAR 替换资源 |
| 运行 | `AE-Track2` 档；3 个独立进程；本机 v104 三进程 CGRD v1 工件 | 新采集、第四进程、源/候选姿态配对 |
| 时间 | 只登记输入工件/日志的 TSC 区间与代序；**不产生 `epsilon_t`** | TSC 秒换算、批量交付时间归属、门 4 完整时间槽位 |
| 层/权重/混合 | 无 | K5-5 的逐骨/通道权重、blend/mask/additive 语义；不得由本协议推断 |
| 装备初态 | 只引用 Save116/panel 运行上下文；控制 B 不依赖装备链的完整初态清单 | K2-C1 的护甲 42 槽/存档身份完整清单 |

**明确不做**：不写源图、不推进图、不新增 hook、不改产品路径；不把控制数据用于 G5 验收；不产生 `epsilon_t`；不把源/候选差或 C2-C 的观测最大比值当作容差。

## 二、输入与身份

本协议只使用已归档材料，不新增游戏采集：

| 输入 | 身份 |
| --- | --- |
| run 1 CGRD | [原始文件](../CharacterPanel-g5cal-grid-CharacterPanel-scenegraph-v104-g5cal2-grid-d61a6e19b9-cl94faaed0c6-20261115T073547Z-20261115T125913Z.bin)，484,596 B，SHA-256 `CB5587574E5BA5146788A4DBF0873D4C57C802BE4F34356111343D1F8E1D508B` |
| run 2 CGRD | [原始文件](../CharacterPanel-g5cal-grid-CharacterPanel-scenegraph-v104-g5cal2-grid-d61a6e19b9-cl94faaed0c6-20261115T073547Z-20261115T132909Z.bin)，484,596 B，SHA-256 `F57D6251DF699E70CC8C2B888716BA05814C53583EB5BEE1B6A5E110A5522221` |
| run 3 CGRD | [原始文件](../CharacterPanel-g5cal-grid-CharacterPanel-scenegraph-v104-g5cal2-grid-d61a6e19b9-cl94faaed0c6-20261115T073547Z-20261115T133459Z.bin)，484,596 B，SHA-256 `60B373DB93BA05C7528768A2F31C298085B6153C30CD86B6197BD85B36BF9008` |
| run 1/2/3 日志 | `F16061B8…DB2B` / `7FD033D3…2230` / `90675EE7…ED04`（完整值见 [CAL2 §十一](../g5-cal2-20261115/README.md#cal2-run3-final)） |
| 121 网格真值 + BOUNDS | [mt_idle_female_truth_grid121.txt](../g5-cal2-20261115/mt_idle_female_truth_grid121.txt)，SHA-256 `5e20fcaabcdf40a70d4c0741fb7768d31896131dad19821e28bc79b8917eb4e3` |
| 三进程 C2-A..F 裁定 | [batch-3of3 summary](../g5-cal2-20261115/batch-3of3-adjudication/cal2-adjudication-summary.txt)，`VERDICT READY`；JSON SHA-256 `82FCD7E5…C555C` |
| 裁定工具 | [`tools/cal2_adjudicate.py`](../../../tools/cal2_adjudicate.py)，SHA-256 `F2225EACE4028D9216BEC06C918D6C999078A55ECD13FDF8E1A718303EF6136F` |

任何身份不符、工件缺代、日志 `[E]` 超出 CAL2 §十二 的精确豁免、或 C2 裁定不是 READY → 本协议 INVALID，停止，不产生数值。

## 三、测量定义（固定，来自 CAL v1 §六）

对每个进程 `r`、时间 `k=0..120`、轨 `t=0..98`：

| 量 | 定义 | 备注 |
| --- | --- | --- |
| 位置 `E_p` | `sqrt(dx²+dy²+dz²)`，double 累加 | parent-local，原生存储单位；不剥离 extracted motion |
| 旋转 `E_r` | 两四元数先归一化；`c=clamp(abs(dot(q_a,q_b)),0,1)`；`E_r=1-c` | `q/-q` 等价；诊断角 `theta=2*acos(c)` |
| 缩放 `E_s` | `max(|sx_a-sx_b|,|sy_a-sy_b|,|sz_a-sz_b|)` | 原生 XYZ 全保留 |

无效值规则：非有限、零范数四元数、缺轨/缺分量不是“大误差”；出现即 INVALID，停止校准，不用容差吸收。

## 四、Q：解析表示/解析界

Q 从 121 网格真值的 BOUNDS 节与 CAL2 §9.7 的 B2 公式逐字推导，**不使用观测最大差**。

### 4.1 逐分量界

对每轨 `t`：

| 分量 | 界 `q` |
| --- | --- |
| position x/y/z | dynamic 16-bit：`max(2*(max-min)/65535, 32*eps*(|min|+|max|))`；dynamic 8-bit：`max(2*(max-min)/255, 32*eps*(|min|+|max|))`；static：`32*eps*|value|`；identity：`32*eps` |
| rotation（4 分量） | 本资源全部 40-bit：固定 `3.45436e-4`（B2 登记值；资源 FNV 未变） |
| scale x/y/z | 同 position 的 dynamic/static/identity 规则 |

`eps=2^-24`；dynamic `(min,max)` 取全体块的保守并集（CAL2 §9.7）。

### 4.2 合成为 CAL 度量

对每轨 `t`：

| 量 | 合成界 | 推导 |
| --- | --- | --- |
| `Q_p(t)` | `sqrt(q_tx²+q_ty²+q_tz²)` | 三角不等式；double |
| `Q_s(t)` | `max(q_sx,q_sy,q_sz)` | max-abs 度量 |
| `Q_theta(t)` | `2*asin(min(1, q_rot + 32*eps))` | 四分量的逐分量界 → 单位四元数角差上界；`32*eps` 是 float32 归一化/点积舍入的保守余量 |
| `Q_r(t)` | `1-cos(Q_theta(t)/2)` | CAL v1 的旋转度量；报告同时保留 `Q_theta` |

全局值：`Q_p=max_t Q_p(t)`、`Q_s=max_t Q_s(t)`、`Q_theta=max_t Q_theta(t)`、`Q_r=1-cos(Q_theta/2)`；逐轨值全部保留，不用全局值替代逐轨报告。

**不把 C2-C 的观测最大比值当作 Q**；它只是输入可行性的旁证，Q 必须由上述解析式独立复算。

## 五、N：跨进程重复性

对每个进程对 `(a,b)`（1-2、1-3、2-3）、每个时间/轨：

| 量 | 定义 |
| --- | --- |
| `N_p(a,b,k,t)` | 两个进程在同一 `k,t` 的 `E_p` |
| `N_r(a,b,k,t)` | 同一 `k,t` 的 `E_r`（附 `theta`） |
| `N_theta(a,b,k,t)` | 同一 `k,t` 的归一化四元数诊断角 `theta=2*acos(c)` |
| `N_s(a,b,k,t)` | 同一 `k,t` 的 `E_s` |

全局值：`N_p=max N_p(a,b,k,t)`、`N_r=max N_r(a,b,k,t)`、`N_theta=max N_theta(a,b,k,t)`、`N_s=max N_s(a,b,k,t)`。

- **不删除任何进程对、时间、轨或离群值**；全部逐样本由三份 CGRD 原始工件保留并可复算，CAL1-B 的 JSON/CSV 只保存逐轨/逐对聚合与计数，不重复展开全部样本。
- 同一进程内两次读取必须逐位一致（C2-B 已由 CAL2 裁定）；本协议不再重新裁定 C2，只消费其 READY 工件。
- 不同进程的 TSC 数值**不相减**；N 只比较同一预定时间/轨的变换位值。
- 若某分量非有限或缺失 → INVALID，不把该样本排除后继续。

## 六、tau 推导与舍入

预注册公式：

```text
tau_p = round_up_6sig(Q_p + N_p)
tau_s = round_up_6sig(Q_s + N_s)
tau_theta = min(pi, Q_theta + N_theta)
tau_r = round_up_6sig(1 - cos(tau_theta / 2))
```

- `round_up_6sig(x)`：正值保留 6 位十进制有效数字，在最后保留位**向上**舍入；0 保持 0。用 `decimal.Decimal` + `ROUND_CEILING` 实现，同时保留未舍入 double。
- 旋转先在角度空间合成，再换回 `1-cos`；不直接把 `Q_r+N_r` 相加。
- 逐轨 `tau_p(t)/tau_s(t)/tau_r(t)` 与全局 `tau_p/tau_r/tau_s` 都报告；判定使用全局值，逐轨分布用于适用范围说明。
- **本协议不产生 `epsilon_t`**；时间槽位保持 `NOT_PRODUCED`，不得用 TSC 区间或采样次数替代。

## 七、判据（预注册，不得事后放宽）

| # | 判据 | 通过条件 | 否则 |
| --- | --- | --- | --- |
| C1B-A 输入身份 | 3 个 CGRD/日志/真值/裁定 JSON 的 SHA-256 与登记逐位一致；C2-A..F = READY；121×99×10 完整 | 全部满足 | INVALID，停止 |
| C1B-B Q 复算 | BOUNDS 解析、逐分量界、度量合成与本文 §四逐项一致；合成公式有独立离线测试 | 全部满足 | INVALID |
| C1B-C N 完整 | 3 个进程对 × 121 × 99 × 3 主度量（p/r/s；`theta` 为 r 的诊断）全部有限；无删除/截断/缺样本；全部样本可从三份 CGRD 原始工件复算，聚合输出保留逐轨/逐对计数 | 全部满足 | INVALID |
| C1B-D tau 合成 | 6 位有效数字向上舍入；旋转角空间合成；未舍入值保留；逐轨与全局值一致 | 全部满足 | INVALID |
| C1B-E 可复现 | 相同输入重复运行，JSON/CSV/summary 逐字节一致；输出不含墙钟时间或随机量 | 全部满足 | INVALID |
| C1B-F 范围纪律 | 所有输出标记 `CAL1-B/control-B-single-layer`；无 `epsilon_t`、K5-5、G5 或姿态等价字段/裁定 | 全部满足 | INVALID |

**裁定语义**：C1B-A..F 全过 → `CALIBRATED_BOUNDED`（仅控制 B 单层姿态）；可把 `tau_p/tau_r/tau_s` 填入门 4 的姿态槽位（限定范围），`epsilon_t` 仍为 `NOT_PRODUCED`。否则 `INVALID`/`NOT_READY`，不产生数值。

## 八、实现与工件

| 项 | 预注册 |
| --- | --- |
| 工具 | `tools/cal1b_calibrate.py`，纯 stdlib；读 3×CGRD + 121 网格真值 + BOUNDS + C2 batch JSON/log；不读游戏、不改插件 |
| 输出 | `cal1b-calibration.json`（Q/N/tau/逐轨/逐进程对聚合/输入哈希）、`cal1b-tracks.csv`（逐轨聚合）、`cal1b-summary.txt`（≤200 行）；原始样本以三份 CGRD 工件为准，不重复展开；只用输入 basename+SHA-256 标识，不含绝对路径或墙钟时间，保证同输入逐字节可复现 |
| 测试 | `tools/cal1b_calibrate_test.py`：合成 truth/CGRD 的 Q 合成、旋转角界、N 全样本、6 位向上舍入、可复现、范围纪律 |
| 身份 | 工具/测试 SHA-256 在首次使用前登记；输出工件 SHA-256 随后登记 |
| 预算 | JSON ≤ 4 MiB；CSV ≤ 4 MiB（逐进程对×时间×轨聚合，原始样本不重复）；summary ≤ 200 行；超限即停 |

实现只增加离线工具与测试，不改 `src/`、不改 INI、不改部署。

输出排序固定为 `pair(1-2, 1-3, 2-3) → time 0..120 → track 0..98`；CSV 使用同一顺序与固定列顺序，确保同输入逐字节可复现。

## 九、时点、停止与边界

- 本文是预注册草案；**评审通过前不实现、不运行、不产生任何数值**。
- 已看数据不回填：本协议的 Q/N/tau 规则不得根据运行结果调整；若要改，升版本并另立批次。
- 停止规则：身份不符、缺代、非有限、`[E]` 未豁免、C2 非 READY、预算超限、输出不可复现 → 立即停止并保留现场。
- 本协议不产生 `tau_p/tau_r/tau_s/epsilon_t` 之外的任何 G5 结论；不关闭 K5-5、K2-C1、完整 CAL1 或 G6。

## 十、后续顺序

1. 本文评审/批准 → 冻结 §四–§七。
2. 实现 `cal1b_calibrate.py` + 测试，登记 SHA-256。
3. 对三份归档工件运行，登记 Q/N/tau 工件与哈希。
4. 只填门 4 的**姿态槽位**；`epsilon_t` 槽位继续 PENDING-CAL。
5. 并行关闭完整 CAL1 的 K2-C1、K5-5 与 TSC 频率/单位缺口；之后才谈完整门 4 冻结/门 5。
