# PlayerPanel 测试结果总览（2026-11-18）

范围为 `oar-idle3` 截至 `f937d350c518092bb5d4efd8b4759d335ffecb67` 的已记录测试，以及 11 月 18 日的过渡离线审计、字段补审计和归档复核。本轮没有新的游戏动作样本。产品运行证据、独立行为图关卡和离线校准按各自判据报告。

**总裁定：G1–G4 在登记范围内成立，G5 姿态验收失败（G4P-7 FAIL），G6 待验。** 门 5 采样完整，失败来自姿态超容差。最近的离线审计已定位混合计时的引擎读取路径，尚未观测实际装备边的运行时计时。

<a id="product"></a>
## 默认面板路径

| 验证 | 已有结果与边界 |
| --- | --- |
| SceneGraph 复制与显示 | v46 同源码重建的实机轮观察到正常面板、换装/重捕、独立源窗口、读档取消与重新武装。保留副本账本可记录进程累计量，原生回收与重复会话资源收敛未验。 [v46 实机记录](../scene-graph.md#v46-manual-regression) |
| OAR 与装备预览 | r4 覆盖部分换装、天气与火把路径，6 个请求出现左臂火把层，单独装备火把未重播右手武器动作。单独卸火把、部分历史 IED 条件和视觉一致性仍缺验证。斧头闪动观察到非均匀根缩放，原因待 A/B。idle 循环重复开头段另有开放记录。 [r4 实机记录](display-issues-2026-10-22.md#r4-session) · [idle 循环问题](../animation.md#idle-start-replay) |
| DAC | 配对 DLL/PDB 能复现兼容性档案，错配 PDB 被拒。策略、准备与 VMAD 入口有离线用例。r4 有 5 次受控效果恢复匹配，原生 API、任意副作用和所有 mod 条件仍各有适用边界。 [DAC 验证](../dac.md#verification-and-limits) |
| 较早渲染与动画实验 | M0、AFP、直接绘制、UBe、骨骼映射和旧显示路径已有专题归档。早期局部运行与后来纠正的读数保留各自版本。 [动画历史](../history-animation.md) · [渲染历史](../history-rendering.md) · [原始来源索引](../history-sources.md) |

<a id="gates"></a>
## 独立行为图关卡

| 关卡 | 已有结果与边界 |
| --- | --- |
| G1 构造与所有权 | 方案 A 有界通过。空壳配对释放和候选装载已观察，装载图停用/销毁为 **NO-GO**。v35 已撤回旧全局登记计数推断。 [G1 裁定](../independent-behavior-graph.md#current-status) |
| G2 暂停期推进 | v33 候选语义运动 120/120、实际写骨 109/116、所采源对照无漂移。v34 覆盖重置丢弃、重开独立、F7 重采和读档回退。相同 controller 的来源更换、单独 frozen-only 边界仍待验。 [v33](../independent-behavior-graph.md#g2-v33-run) · [v34](../independent-behavior-graph.md#g2-v34-boundaries) |
| G3 装备相位 | v41 源与候选 F 重放取得 `0→10→1→12→0`，由 `WeapEquip(2)`、`Unequip(144)` 触发，段内由 clip 结束信号收尾。结论限相位顺序。 [G3 源轨迹](../independent-behavior-graph.md#g3-a-contract) · [候选 F](../independent-behavior-graph.md#c-g3b-f-full-replay) |
| G4 OAR 与事件隔离 | A 的 numeric 整窗 72/72 已核对，B 证明装备族选择稳定。修复后的 v69 双派发跑通 C 的 1–6 项，正控 64/64，钉住源图事件与丢失均为 0，范围限 58377 观察路径。 [G4 裁定](../independent-behavior-graph.md#current-status) · [有效重跑日志](CharacterPanel-g4c1-v69-attempt2-two-dispatch-20261105-0001.log) |
| G5 完整姿态 | E1–E4 和采集准备有阶段结果，正式门 5 为 **G4P-7 FAIL**。 [门 5 正式报告](g5-gate5-20261116/README.md) |
| G6 生命周期 | **PENDING**。源恢复、请求与回调失效、引用及重复会话资源收敛尚待验。 [G6 判据](../independent-behavior-graph.md#六道关卡) |

<a id="g5-qualification"></a>
## G5 采集与资源资格

| 阶段 | 结果 |
| --- | --- |
| E1–E4 取证 | E1 v71 有界通过，424/424 完成配对。E2 区分原始混合和后处理，v97d 的 B3/B4 在 739/739 样本通过，后续窗口 906/906 再现。E3 输入序数配对 2/2，E4 取得四块完整 116 轨快照。 [E1–E4 总账](../independent-behavior-graph.md#current-status) · [B3/B4 原生记录](../build.md#g5-e2b-b3b4-v97d-run1-20261113) |
| K1 资源与身份 | 生效行为文件闭包 23/23 与旧审计哈希一致，登记的 14/14 个观测 FNV 均已落位。v101/v101b 骨架和资源普查通过，Track 1 的 OAR 动态资源差异使 K1-C4 为 NOT MATCHED。按运行时 data FNV 核对内容身份，binding-index 仅供进程内跟踪。 [资源清单](g5-k1-c1-20261114/README.md) · [K1 普查](g5-k1-census-20261114/README.md) |
| K2 装备初态与输入 | v102 登记 Save116、31 槽与 9 条已穿戴槽记录，另记源 OAR idle 栈差异。输入账本区分候选五字 prime 与源实际装备写入。D1/D2 未选到预期族，D3 补分支字后命中 1HM，v96a 的卸下于第 129 步回到状态 0。场景清单已登记，源和候选实际附件/内部初态等价待证。 [初态与输入账本](g5-k2c1c2-20261114/README.md) · [v96a 原生结果](../build.md#g5-cal-v96a-observation-v2-run1-20261112) |
| K3–K6 与 Track 1 | K3 候选 249、源 1,151，双读一致、116 轨、零溢出；K4 已取得上述后处理边界。v103 K5/K6 仪器资格中候选 249/249、源 906/906，源有两个 MT_Idle 资源变体待登记。K5-5 有明确缺项，逐骨/通道权重仍待测。 [K3 收官](../independent-behavior-graph.md#g5-cal-k3-closure) · [K6 裁定](g5-k6-plan-20261114/README.md) |
| Track 2 同资源资格 | `AE-Track2` 下 Run 0a/0b 通过，六族 data FNV PAIRED。正式门 5 之前的独立资格批次为候选 249/249、源 1,065/1,065，IDUN 候选 2、源 1 单列。Run 0a 的两次无效结果与占位资源、冻结场景修订过程见专项记录。 [Track 2](g5-track2-20261114/README.md) |

<a id="calibration"></a>
## 校准与门 4

- **离线 A** 的采集格式与分析度量通过，三个固定控制采集、每轮 120/120 分量，原生姿态容差来自后续控制 B。 [离线 A](g5-calibration-a-20261111/README.md)
- **控制 B1** 位精确比较失败。独立新进程 **B2** 在预注册量化与浮点边界内通过，8,910 分量零超界，最大占界比 0.4860。**CAL2** 扩展至三个独立进程，每个进程 baseline + 120 代，三对跨进程比较均为 119,790/119,790 分量逐位相同，K2-C4 为 grid READY。 [B1/B2](g5-cal-control-b-20261114/README.md) · [CAL2](g5-cal2-20261115/README.md)
- **CAL1-B / C** 的联合验收为 **CALIBRATED（限定范围）**。冻结阈值 `tau_p=0.000476592`、`tau_r=6.03341e-8`、`tau_s=0.00000386264`，`epsilon_t=0.0333960 s`，时钟轴 CLOCK_READY。99 条主轨进入姿态裁定，17 条 item/wing/Belly 轨单列描述，独立控制待证。完整 CAL1/门 3 仍未通过。 [联合验收](g5-cal-joint-acceptance-20261116/README.md) · [A17](g5-aux-track-audit-20261116/README.md)
- **门 4** 的元数据 dry-run 有 247/247 对、三锚通过、配对确定；11 月 16 日冻结范围为 99 主轨 P99 姿态、时间、时钟轴和 A17 描述账本。 [配对协议与 dry-run](g5-gate4-20261115/README.md) · [冻结裁定](g5-gate4-freeze-20261116/README.md)

<a id="formal"></a>
## 门 5 正式结果与失败归因

候选 v103 249/249（装备 120、卸下 129），源 v103 849/849（F7 窗口约 20.017 秒）。两侧各 1 条 IDUN，得到 **248 对**，缺伙伴 0，边缘配对 12。G4P-1–6、8、9 成立，G4P-6 的装备锚点 97 vs 96 由 IDUN 数变化解释。本地用归档重新执行同一度量，清单、二进制明细和汇总均匹配。 [正式运行](g5-gate5-20261116/README.md) · [误差汇总](g5-gate5-20261116/formal/gate4-pair-summary.txt)

| G4P-7 主轨误差 | 超阈值次数 | P99 |
| --- | ---: | ---: |
| 平移 `E_p` | 3,826 | 5.5912 |
| 旋转 `E_r` | 16,755 | 0.0457227 |
| 缩放 `E_s` | 352 | 0.000525653 |

时间排除 0 对，无效骨值 0。平移超差分布为 body 1,226 次、附件 2,600 次；其中 `Quiver`、`WeaponAxe`、`WeaponBow`、`WeaponDagger`、`WeaponSword` 这 5 根静态附件占 1,240 次。两侧 skeleton/data 与 12 根审计骨的 referencePose 一致，支持源实际附件状态造成静态偏移。 [归因](g5-attribution-20261117/README.md) · [v105 setup 审计](g5-setup-audit-20261117/README.md)

候选 equip/unequip/idle 仅有 17/21/3 个唯一 clip 时间，多记录组均有不同姿态，组内 PC1 占比 0.9983–0.999998。第一装备组对已采源轨迹的最大最近平移残差约 0.497，卸下后段和 idle 的同类残差分别不超过约 1.5e-5、3.3e-5。由此区分初始装备过渡偏离、混合状态欠观测与静态附件偏移，现有 clip 时间配对不足以解释三者。 [时钟阶梯](g5-clock-step-20261117/README.md) · [状态空间](g5-state-20261117/README.md)

<a id="state-probes"></a>
## 状态探针与剩余验证

| 轮次 | 结果 |
| --- | --- |
| v106 poseLocal bootstrap | 复制源姿态 116/116，首帧 Generate 覆盖。相对于正式 v103，288,840 个候选分量逐位相同，SB-5 FAIL。 [记录](g5-state-bootstrap-20261117/README.md) |
| v107/v108 节点与对象前缀 | 运行报告中 2,729 条 nodeInfo、12 条 Blender 对象行。已观测窗口内找不到伴随同时间组逐步变化的字段，SP-5 未命中。 [记录](g5-state-probe-20261117/README.md) |
| v109 child/internal 扫描 | 运行报告 12/12 零发现，CP-2 FAIL。没有逐代 child/internal 行，按判据 CP-3 未满足。候选归档与 v103 的 249 帧姿态逐字节相同。原始运行日志未随当前检出，扫描计数来自运行报告。 [记录](g5-state-child-20261117/README.md) |
| 11 月 18 日离线审计 | AE 1.6.1170 指令证实 `activeTransitions` 逐条步长为 `0x28`，`timeInTransition` 随步长累加并用于混合。v109 的单层 vtable 扫描未遍历 Blender 的数组元素与内联状态。候选第 2–12 步的 clip 时间固定，11 帧全部配到源 seq 145。 [离线审计](g5-transition-audit-20261118/README.md) |

2026-11-18 复核：与测试哈希一致的 `0_Master.hkx`、`weapequip.hkx` 和转储均在本机，字段补审计已确认 `0→10` 为 `hkbBlendingTransitionEffect`（作者 `duration=0.2 s`、smooth、self-reset），`1HM_Equip` 由 `Weap_EquipDefaultState/msg/State3` 的 Blender 分支选择，不是独立 transition。下一步是在两侧同代采样点采集运行时 `timeInTransition`/`endIntervalWeight`/child weight 与姿态的相关性；[C-G5-TRANS v1](g5-transition-probe-20261118/README.md) 已预注册，尚未实现/上机。G4P-7 维持正式失败裁定，K5-5、实际附件状态和 A17 独立控制仍有缺项。 [字段补审计](g5-transition-audit-20261118/README.md#六离线字段补审计2026-11-18v3-dump) · [闭包与哈希](g5-k1-c1-20261114/closure-files.txt)

## 本轮复核

- 仓库顶层 `tools/*_test.py` 的 5 组离线测试全部通过，覆盖 CAL1-B、CAL1-C、CAL2、门 4 时钟和正式配对（T1–T13）。游戏内读数来自各版本原有归档，原生 C++ 构建与实机采集未在本轮重跑。
- 历史会话的中间状态已归并到本页，逐项试验、构建身份和哈希仍见专项记录。已退役文件的固定 Git 版本、路径及恢复方法见诊断索引。 [退役与恢复索引](README.md#retired-20261118)
