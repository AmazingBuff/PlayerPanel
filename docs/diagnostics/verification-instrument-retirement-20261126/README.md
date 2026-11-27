# 独立行为图探针退役

## 2026-11-27 实施

本次将行为图源码从 18,637 行减到 14,548 行，插件编译单元从 39 个减到 36 个。

- 移除源图事件与变量写入的诊断钩子和旧逐帧跟踪。F7 仍可用 `SourceTimeline` 采集源图的相位变化。
- 移除固定时间的 MT_Idle 采样和网格实验、候选姿势预填、原始内存状态扫描，以及四个已完成的片段时钟、采样、权重和时间搜索探针。
- 移除卸下实验的变量写入分支。开启 `CandidateUnequipProbe` 时，留空或省略 `CandidateUnequipEvent` 即可使用已上机验证的 `Unequip`。明确填写的事件名仍有效。
- 保留候选图的创建、原生步进、姿势显示和域同步，以及 `IdleChainProbe`、源图与候选图的 K5 采样、资源身份和过渡结构诊断。K5 的源侧采样仍由 `SamplingLiveness` 提供。

旧 INI 中已移除的键会被忽略。历史日志、判据与当时的配置仍可查阅。复现旧探针可以使用清理前的源码版本 `b4c8c39`。

15 个离线测试程序通过。使用 MSVC 14.52 和同版本的 CommonLibSSE Release 库完成插件编译与链接。游戏内行为待复测。

## 2026-11-26 调查原文

本节记录当时建议的退役范围和预计行数。

## 一、总览

| 项 | 数值 |
| --- | --- |
| `behavior_graph/` 总行数 | **18,637**（.cpp 9,000+ / .h 6,959） |
| `dac/` 总行数（对照） | 1,528 |
| `oar/` 总行数（对照） | 2,273 |
| `controller.cpp`（对照） | 1,373 |
| 建议退役合计 | **8 个编译单元、11,094 行**（.cpp 6,886 + .h 4,208） |
| 退役后 `PLUGIN_SOURCES` | 39 → 31 |

## 二、依赖树

```
independent_graph (3,917) ──┬─ k1_census (271)
                            ├─ k5_audit (158)
                            ├─ frame_trace (91)
                            └─ control_sample (733)

probe_liveness (2,216) ────── transition_probe (2,248)

main.cpp 直接安装：probe_liveness / source_event_capture / source_write_capture
controller.cpp 直接调用：g6_ledger / ownership_probe / independent_graph / DisplaySession
```

两组仪器彼此独立（`independent_graph` 调 `probe_liveness`，反向不调），但都只在 `IndependentGraph != off` 时进入。

## 三、建议退役（8 单元）

| 模块 | .cpp | 关联头文件 | 服务的契约 | 退役注意 |
| --- | ---: | --- | --- | --- |
| `probe_liveness` | 2,216 | `probe_liveness_policy.h` (1,541) | G5-E1/E3 取证 | 需断开 `independent_graph` 两处调用（`arm_probe_liveness_concurrent` / `close_probe_liveness_window`） |
| `transition_probe` | 2,248 | `transition_probe.h` (377)、`transition_parent_chain_policy.h` (176) | C-G5-TRANS | 由 `probe_liveness` 调用，须一并去掉 |
| `control_sample` | 733 | `control_sample_policy.h` (153)、`observation_chain_policy.h` (33) | C-G5-CAL2 | 由 `independent_graph` 调用 |
| `source_event_capture` | 656 | `source_event_capture_policy.h` (200) | C-G4-C1 | **`main.cpp:51` 直接安装**，需删装 |
| `source_write_capture` | 513 | `source_write_capture_policy.h` (197) | C-G3B-S4/S5 | **`main.cpp:52` 直接安装** |
| `k1_census` | 271 | `k5_snapshot_policy.h` (441)、`k3_snapshot_policy.h` (270) | C-G5-CAL K1/K5 | 与 `k5_audit` 共享 `k5_snapshot_policy.h`，见 §五 |
| `k5_audit` | 158 | 见上 | **K5-5（仍是开放项）** | ⚠️ 见 §四 |
| `frame_trace` | 91 | `frame_trace_policy.h` (128) | G2 v18 | 由 `independent_graph` 调用 |

## 四、必须保留

| 模块 | 行数 | 理由 |
| --- | ---: | --- |
| `g6_ledger` | 36 | 每个存档边界一行，是生命周期回归的唯一**持续**观测，成本极低 |
| `independent_graph` | 3,917 | `DisplayPose` 是唯一解释"面板动作 ≠ 世界动作"的机制；删除后面板永远显示原版动画（[v127 结论](../idle-loop-v127-session-20261124/README.md)） |
| `source_timeline` | 758 | 绑着 F7 热键工作流，日常在用 |
| `ownership_probe` | 81 | 被 `controller.cpp:8` 调用，开销极小 |
| `k5_audit` | 158 | ⚠️ K5-5 仍在开放项中；虽已签收，但它是该结论的唯一在役观测。若要彻底而非"有界"收口才可退役 |

## 五、两点风险

1. **审计追溯链会断**。G1–G6 的签收记录引用了具体行号与函数名（如 `probe_liveness.cpp:1917`、`independent_graph.cpp:2857`）。退役后这些引用指向不存在的文件。结论仍然有效，但以后复查无法直接跳转。
2. **`k1_census` 与 `k5_audit` 共享 `k5_snapshot_policy.h`**（441 行）。若退役 `k1_census` 而保留 `k5_audit`，该头文件须留下——实施时需单独核对。

## 六、预期收益与边界

- 源码 30,656 → 约 19,500 行（−36%），DLL 体积同步下降。
- **运行时开销本就是零**：这些仪器在未启用时于 `install_*` 首行 `install-skip` 并返回（见 `probe_liveness.cpp:1922`、`source_event_capture.cpp:402`），不安装任何 detour。
- 所以真实收益是**维护与认知负担**，不是性能。这也意味着"留着的代价"仅是编译时间与代码可读性。

## 七、DAC 侧结论（同批调查）

DAC 不能退役，且几乎无可收窄空间：

- 它是外部插件 **Dynamic Animation Casting NG Plus 3.2.4** 的调用封装，做两件生产路径必需的事：暂停世界里推引擎事件（`native_dispatch`）、注入/恢复标记效果（`capture_effects`/`restore_effects`）。
- 生产调用 5 处：`controller.cpp:573`（驱散修正喂给 OAR）、`619`（effect_revision）、`794`（idle_readiness）、`923`（prepare_idle）、`1266`（dispatch）。最后一条是 HKX 注解派发，**v128 修正能起作用的前提**。
- `effect_corrections` 那条链**既是功能也是观测**（v128 依赖它），不可为"清理观测"而简化。
- 唯一可退役的观测是 `world_phase_probe`（C-WORLD 已结案，约 120 行 + 一次每帧 bool 判断）与 `Session::smooth_markers()` 一层薄转发。

## 八、变更记录

- 2026-11-26：首次调查，产出本清单。**未执行任何代码改动。**
