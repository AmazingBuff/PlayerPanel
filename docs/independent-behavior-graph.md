# 独立行为图验证

更新：2026-11-11。先读[最新运行摘要](#status-20261111-session)、[C-G5-CAL v1 校准契约](#c-g5-cal-v1)、[离线 A 结果](#c-g5-cal-a-run1-20261111)与 [K1/K2 资格账本](#c-g5-cal-k1k2)。E1 v3 在 v71 有界通过；E2-B 的 B1/B2/B6 保留，B3/B4 未闭合；E3 输入序数配对 2/2，E4 在 v91/v92 取得四块完整的 116 轨源快照，E2-C 装备链速率已有有界证据。**离线 A 已完成；K1/K2 资格账本已登记；D1 已跑并判负——实际装备为 1H，候选仍选 `H2H_Equip`；D2 pre-event prime 已实现、部署并判负——三项变量写入并保持，片段仍为 H2H；D3 源侧探针已跑，但未覆盖 `iRightHandType` 初始值；D3 v2 已实现、构建为 v94 并部署，待上机；K1–K6、原生 B 与时间校准仍 NOT_READY，数值容差未产生。** G1/G2 有界通过、G3-B 相位回放与 G4 A/B/C 证据保留；完整层/权重/混合、输入/资源/实例身份、同代阶段和后处理边界仍须按契约补齐。G5 未冻结/未通过，G6 后置，装载 teardown NO-GO。最近记录部署为 v94 + D3 源侧探针档（v93 + D3 三件套备份保留）；D2 已上机并判负，D3 已上机但未覆盖初始值，v94 尚未上机。历史记录保留各自日期与限定，不用旧摘要覆盖新运行。

<a id="current-status"></a>

## 当前结论

| 关卡 | 当前结果 | 证据边界 |
| --- | --- | --- |
| G1 | **PASSED，仅按方案 A 修订判据** | 空壳配对销毁、重复装载和所采源对照成立；装载图停用/销毁经 2026-10-28 审计**判为 NO-GO**（[理由](#g1-teardown-audit)）；"全局登记计数"的旧静态结论被 v35 上机读数证伪、判为无锚点（[更正](#g1-teardown-audit)）。 |
| G2 | **PASSED（2026-10-28 v33 窗口 + v34 边界，限定范围）** | 完整窗口 120 次 eligible 调用：候选语义运动 **120/120**（1e-6、符号不变）、实际写骨 **109/116**、源 `drift=none` 且 before=after、画面连续（[v33](#g2-v33-run)）。四条边界全部成立：`reset` 中断（`steps=61` 丢弃）、重开不拼接、F7 `reason=retarget` 后 view 递增并重采基线、读档后旧候选被拒回落 OAR（[v34](#g2-v34-boundaries)）。**未覆盖**：同 controller 来源图/generation 替换、单独 frozen-only 拒绝与旧读档拒绝的确切原因、装载销毁、G3–G6；v26 的 raw 119/120 仍只是 raw 证据。 |
| G3 | **G3-A 成立；G3-B PASSED（相位序列范围）** | [v41 源轨迹](#g3-a-contract)与 [F 候选回放](#c-g3b-f-full-replay)：0→10→1→12→0、两次外部事件、段内 clip-end 自退、源控制不漂移；不等于完整姿态相同。 |
| G4 | **A/B/C 保留（C 已在修复版仪器上重跑关闭，2026-11-05 v69）** | [A 全窗 numeric 类型](#g4-followup-numeric)：v63 运行整窗 72/72 `type=0`、逐窗对账相等；B 限装备族选择稳定；[C 源事件兼容观察](#c-g4-c1-concurrent-events)：v66 的 `events=0` 曾因 [audit30](behavior-graph-construction-audit.md#audit30) 仪器缺陷撤回，**v69 修复版重跑（attempt 2）判据 1–6 全部成立**——`calls=64/other-graph=64` 正控、源图 `events=0`、`ring-dropped=0`、`reason=display-close`，两注入回放第九次逐位复现。零计数仍只限 58377 路由 |
| G5 | **离线 A 已完成；K1/K2 账本已登记；原生校准 NOT_READY，G5 未冻结/未通过** | [E1 v3](#c-g5-e1-v3) 有界通过；[E3](#c-g5-e3-v2) 配对 2/2；[E4/v92](build.md#v92-g5e2c-run1-20261111) 四块 116 轨快照、零丢失，装备链速率可算。[离线 A](#c-g5-cal-a-run1-20261111)仅校准固定控制的采集/格式/解析；[K1/K2 账本](#c-g5-cal-k1k2)把 H2H/1HM 差异归因于候选图缺少源侧真实装备上下文，D1 诊断未运行；[CAL v1](#c-g5-cal-v1) 的 K1–K6、原生 B、时间资格、B3/B4 与完整贡献仍缺，既有证据不证明姿态等价 |
| G6 | **PENDING** | 源恢复、请求/任务失效、引用/回调/sink/历史及重复会话收敛均须验；装载 teardown NO-GO。 |

v27 提供了 116 条输出姿态记录和用户确认的 H2H_Idle 静态站姿，**没有测出 116 个实际 name→node 命中**。v28–v30 显示冻结，但配方不完整、v30 门诊断又读错对象；v31 已修正这些代码问题，不能把旧负结果扩大为完整逐帧驱动不可行。

[v33](#g2-v33-run)（v32 代码的重建）在 2026-10-28 22:28 上机取得第一个非中断窗口：120 次 eligible 显示调用里候选语义运动 120/120（1e-6、符号不变）、实际写骨 109/116（`missing=7`，旧日志只列 4 个控制/挂点标签、另 3 个未知；标签不能证明可忽略，`109+7+0=116` 自洽）、源侧 `drift=none` 且 before=after、**画面连续运动经用户确认**。同一窗口的门读数 `active/children=true`、`numAnim=numData=116`，从运行侧确认 v30 那组 `children=0`/`numData=62016` 是读错对象。G2 因此按原范围判 PASSED；v34 后续补齐四条已观察边界，确切拒绝原因、同 controller 来源替换和 frozen-only 拒绝仍未验，装载销毁及 G3-B/G4–G6 不由此通过。

[v32](#g2-v32-display-session)让冻结和逐步显示共用带 generation/owner 的来源有效性；测量随 request/controller 重置，F7 重捕获重开 view 窗口。相同代码重建为 v33 后，完整窗口与 v34 四条边界均已有运行记录。最近记录部署为 v92 + E4 运行档（见[部署](#deploy-state)，本次未重验安装）；v42/v43 的“本地未部署”只描述当时工件，后续 v46-20261030 已观察正常工作流。默认产品仍走装备驱动 OAR 路径；缓存回放不作为既定方向。



<a id="verification-matrix"></a>
<a id="验证总表2026-10-29含-v33v34v41-证据"></a>
## 验证总表（2026-11-05，含 v69 修复版上机与 audit32 静态收口）

归档结果包括 v33/v34、v41、v46-20261030、G3-B/F、G4 历史裁定、v67/v68 入口活性运行、**v69 修复版四轮上机（E1 两段窗口 ×2、G4-C 正控 ×2）** 与 [audit32](behavior-graph-construction-audit.md#audit32)。**当前更正优先**：G4 A/B/C 保留（C 已在修复版仪器上重关）；E1 的静态四项（完成点/输出范围/寿命/快照顺序）已闭合，**运行侧待增补正控的探针 v3**；`[E1 修复后边界](#g5-e1-evidence-repair)` 的"未部署"表述已由 v69 两轮取代。历史读数、原始日志与工件身份不改写。

<a id="status-20261105"></a>
### 2026-11-05 验证结果汇总（v69 夜）

**本轮做了什么**（一个下游包：干净重建 → 部署 → 四轮上机 → 一次离线静态审计）：

| # | 动作 | 产物 / 身份 |
| --- | --- | --- |
| 1 | 干净树重建修复版 | v69 DLL `2490c38d…da11e` 1,590,272 B、PDB `f69c9790…`、CodeView age 397、身份 `…-v69-g5e1-evidence-repair-a89fa5cba0-cl94faaed0c6-20261104T143255Z` |
| 2 | 部署（concurrent INI，4 键派生 + 20 键断言，拒 BOM） | v68 三件套备份 `dist/deploy-backup-20261104-v68-v69pre/`；运行 INI `c3703b74…` 9,640 B |
| 3 | E1 窗口 run1（面板未关，无转段） | `eligible=recorded=drained=2628`、零缺口、`transport-complete=yes`；Q2 按预注册记"未检验" |
| 4 | E1 窗口 run2（转段完整） | resumed `pinned-engine=408`（全 `caller=0xba3f6e`）、`tracked=paired=788`、六线程反证复现、对账全零缺口 |
| 5 | G4-C attempt 1（INI 漏卸下切片） | 仪器四判据成立（`calls=42/other=42/events=0/display-close`）、形状不完整 → 不判通过 |
| 6 | G4-C attempt 2（六键 INI） | 判据 1–6 全成立：`calls=64/other-graph=64`、源图 `events=0`、`ring-dropped=0`、`display-close`；两注入回放第九次逐位复现 → **G4-C 重关** |
| 7 | audit32 离线静态审计 | 完成点（两捕获窗 + 63856 五个调用点）、输出范围（n 路径三分）、寿命（单点 lazy 分配、无 free）、快照（`[g+0x148]` 四写点顺序）四项闭合 |

**各关卡当前判定**：

| 关卡 | 判定 | 本轮变化 |
| --- | --- | --- |
| G1 | PASSED（方案 A） | 无 |
| G2 | PASSED（v33 窗口 + v34 边界） | 无 |
| G3 | G3-A 成立；G3-B PASSED（相位序列） | 无（v69 两注入回放为其第九次逐位复现） |
| G4 | **A/B/C 三段保留** | **C 由"仪器无效待重跑"改为"修复版重跑关闭"** |
| G5 | **门 1 NOT_READY（未通过）** | **E1 修复版取证缺口闭合 + 静态四项闭合 + [v3 运行通过](#c-g5-e1-v3)（v71：424/424 完成配对、五路径对账 89,437、un=0、count-over-capacity=0、pose-stable=yes、ill==cap==116、in-flight-max=1）**；E2/E3 与联合验收未做 |
| G6 | PENDING | 无（teardown 维持 NO-GO） |

**证据边界（不得外推）**：G4-C 的零事件只限 **58377** 路由；E1 的 `pinned-engine=408` 是**应用入口**计数，不是逐骨写满或输出完成；六工作线程只证明观察到的调用线程不同，不识别调度器；对账恒等式只覆盖已武装窗口内的传送与关闭；audit32 全部为静态可达，不等于运行命中；`fill ≤ cap`（`skeletonWord+0x40` vs `+0x30`）与 63563/63572 两个未列调用点仍是**待运行核验项**。

**下一步排序**（与 [交接](#handoff-20261102-evidence)一致，仅状态更新）：① 按 [audit32 §五](behavior-graph-construction-audit.md#audit32) 增补正控清单 → 探针 v3 实现评审 → 上机取证；② E2 完整贡献（**E2-A 契约已预注册**，见 [C-G5-E2](#c-g5-e2)；含槽填充分点、63563 路径）；③ E3 合格 R2 配对（按 n 路径三分记录）；④ 联合验收 → 独立校准 → 协议冻结 → 新正式验证。G6 后置，typed-setter 窗口按需重跑。

### 关卡

| 关卡 | 判定 | 限定 / 未覆盖 |
| --- | --- | --- |
| G1 | **PASSED**（方案 A 修订） | 装载图停用/销毁、全局登记计数、资源收敛未证 |
| G2 | **PASSED**（v33 窗口 + v34 边界，限定范围） | 完整窗口：候选语义 120/120、实际写骨 109/116、源 before=after、画面连续。边界：`reset` 中断 `steps=61` 丢弃且无摘要、重开不拼接、F7 `reason=retarget`、读档后旧候选被拒回落 OAR。未覆盖：同 controller 来源替换、frozen-only 拒绝、物理/挂载交互、装载销毁与 G3-B/G4–G6；旧读档拒绝的确切原因未记录 |
| G3 | G3-A 成立；**G3-B PASSED（相位序列）** | [F](#c-g3b-f-full-replay)：两事件复现 0→10→1→12→0；完整姿态另验 |
| G4 | **A/B/C 保留；C 已在修复版仪器上重跑关闭（v69 attempt 2，2026-11-05）** | [numeric 全窗覆盖](#g4-followup-numeric)（v63：整窗 72/72 `type=0`、逐窗对账相等）、B 限装备族选择稳定；[源事件兼容观察](#c-g4-c1-concurrent-events) 的 v66 `events=0` 曾由 [audit30](behavior-graph-construction-audit.md#audit30) 判无效（thunk 实参错位），**v69 修复版重跑判据 1–6 全部成立**：`calls=64/other-graph=64`、源图 `events=0`、`ring-dropped=0`、`reason=display-close`，两注入回放第九次逐位复现 |
| G5 | 门 1 NOT_READY；未通过 | [E1 修复后边界](#g5-e1-evidence-repair) + [v69 两轮](#c-g5-e1-v69-run2)：跳板/传送/对账恒等式成立、resumed 段钉住源图 408 次引擎采样、788/788 全配对、六工作线程反证复现——修复版 E1 取证缺口闭合；[audit32](behavior-graph-construction-audit.md#audit32) 静态闭合完成点/范围/寿命/快照四项，**[v3 契约](#c-g5-e1-v3) 已实现（v70 `757b01481b`，未上机）**；E2/E3 未开始，之后仍须仪器→独立校准→完整冻结→新正式验证 |
| G6 | PENDING | 非豁免生命周期、源恢复与重复会话收敛未证 |

### 逐轮实验（本系列）

| 轮次 | 目的 | 结论 |
| --- | --- | --- |
| v17 / v18 | 采样门与帧轨迹 | 空结果，定位采样门 |
| v19 | 眼位 arm | `arm-observed=no`，定位 63864 步长门 |
| v20 | 恢复 `63589` 入口 | guard 通过后、入口前崩溃（越界诊断） |
| v20a | 修诊断链 | 入口到达，首次 pose 变化（120/120） |
| v21 | pose 内容解码 | 命中第 3 条（`source-equal>0`）→ 审计判为误报 |
| v22 | 别名判据修正 | 通过：有限、在变、未别名 |
| v23 | 语义对应 | 单一片段不成立；候选是多片段栈 |
| v24 | identity 映射 + 层普查 | `H2H_Idle` 覆盖 90/99 骨，旋转误差 ≤6.1e-5 |
| v25 | 逐步时钟 | 速率 **1.000×**；v24 的 0.75× 判为采样假象 |
| v26 | 逐步 raw pose | 119/120，按当时判据判 G2 PASSED；当前不能据此单独证明语义连续性 |
| v27 | 静态接驳显示图 | 输出 `bones=116`，用户确认 `H2H_Idle` 站姿；实际骨映射覆盖未计数 |
| v28 | 逐帧（只调 63589） | 三条哈希全同 → 该调用不自推进 |
| v29 | 逐帧（补 63555/63602/63556） | 哈希与 v28 逐位相同；仍遗漏显式 63587(dt/eye) 等成功批量步骤 |
| v30 | 门诊断 | 显示冻结；active/children 对象基址错误、numData 少解引用；批量与显示同线程 |
| v31 | 共用完整单步、修正诊断 | 两个离线回归及 Release 构建通过；该轮无产物留存 |
| v32 | 来源/窗口、语义运动和覆盖 | 两个新离线场景及 Release 构建通过；产物未留存，由 v33 重建后上机 |
| v33 | 重建 v32 代码并上机单窗口 | **显示连续推进成立**：语义 120/120、applied 109/116、源不变、画面连续；门读数修正 v30 结论 |
| v34 | 同一 DLL，只做窗口/来源边界 | 重开不拼接、F7 `steps=84 reason=retarget` 后 view 2→3、读档后 0 条 display 行；补测 `steps=61 reason=reset` 后新窗口从 call 1 独立跑满 120，**四条边界全部成立**。附带：候选轨迹跨会话逐位复现且与源姿态无关 |
| v67 / v68 | E1 最小探针 → v2 两段窗口 | 入口活性成立（`pinned-engine=411`、六工作线程反证、`flag=0`）；旧 `completion` 语义被 [audit31](behavior-graph-construction-audit.md#audit31) 撤回，仪器缺陷（跳板/传输/关闭）修复中 |
| v69 run1 | 修复版首轮（面板未关，无转段） | 对账恒等式全零缺口（`eligible=recorded=drained=2628`）、三处 drop 全 0、`transport-complete=yes`；无 `display-close` → 按预注册记 Q2"未检验" |
| v69 run2 | 修复版两段窗口（转段完整） | resumed `pinned-engine=408`（全 `0xba3f6e`/`num240=116`）、`tracked=paired=788` 零缺口、六线程反证复现、`suppressed-rows=0` → **修复版 E1 取证缺口（入口活性/配对/对账）闭合** |
| v69 C1-a1 | G4-C 修复版首轮（INI 漏卸下切片） | 仪器四判据成立（`calls=42/other-graph=42`、`events=0`、`display-close`），但单注入、收于 readied 姿态 → 形状不完整，**不判通过** |
| v69 C1-a2 | G4-C 修复版完整两注入轮 | 判据 1–6 全部成立：`calls=64/other-graph=64`、源图 `events=0`、`ring-dropped=0`、`reason=display-close`；`drive-done`/`unequip-done` 六哈希第九次逐位复现 → **G4-C 重关** |
| v70 run1 | v3 首轮（三钩子首版） | 返回侧对 81,034 次调用全触发、源图 383/383 配对；但 `apply-paths` 全 unclassified（R2 硬停），另有三处仪器缺陷 → 不通过 |
| v71 run2 | v3 修复版（本文一轮） | **R1–R7 全部满足**：424/424 完成配对、五路径对账 89,437（`un=0`）、`count-over-capacity=0`、`pose-stable=yes`、`fill==cap==116`、`in-flight-max=1`；`p63563=10,600` 首次给出物理回调路径的运行证据 |
| audit32 | 完成点/输出范围/寿命/快照静态审计（离线） | 四项闭合：63856 五个调用点与两处捕获窗、n 按路径三分、poseLocal 单点 lazy 分配且全镜像无 free、`[g+0x148]` 四写点全在拷贝前 |

### 已验证的引擎事实（指令级）

| 事实 | 依据 |
| --- | --- |
| 空 `transformTrackToBoneIndices` = identity | [audit25](behavior-graph-construction-audit.md#audit25)，AE `ConvertTrackPoseToBonePose` @ `0xB03380` |
| 片段时钟只消费 dt：`new = old + dt × ((effDur/enforcedDur 或 1) × playbackSpeed)` | [audit26](behavior-graph-construction-audit.md#audit26)，`0xACDE40` → `0xACFDE0`，速度 `0xAD01F0` |
| 成功批量配方包含辅助管线、子图标记、显式 `63587(dt/eye)`、骨数镜像和生成入口 | 既有批量源码、原生更新审计、v26/v30 批量记录；[v31 共用实现](#g2-v31-correction) |
| v28–v30 显示实现未产生新 pose，但没有测试完整批量配方 | v29/v30 原始显示日志与源码差分；不能据此判定完整逐帧驱动不可行 |

### 已验证的产品行为

- 保留候选提供 **116 条姿态记录**；[v27](#g2-v27-run)确认显示拷贝呈 H2H_Idle 静态站姿，[v33](#g2-v33-run)测出实际写骨 **109/116**（`missing=7`、`skipped-root=0`、`duplicates=0`，不变量 `109+7+0=116` 自洽），已列名的缺失项均为 `x_NPC Rotate/LookNode/Translate` 与 `Weapon` 这类非骨控制/挂点。
- **面板内角色连续运动**（[v33](#g2-v33-run)）：`native-step` 1/60/120 三点哈希互异，`pose-summary semantic-changed=120/120`，画面经用户确认。
- `DisplayPose=false` 时 OAR 路径完全不受影响（默认即此）。

### 未验证 / 开放项

| 项 | 现状 |
| --- | --- |
| 显示窗口边界 | **已验全部四条**：`reset` 中断（`steps=61`，无摘要）、重开不拼接、F7 `reason=retarget` + view 递增 + 重采基线、读档后旧候选被拒且无 `native-step` 残留（v34）。**未验**：同 controller 来源图/generation 替换、单独 frozen-only 拒绝；旧日志未直接记录读档拒绝的确切原因 |
| 场景应用（物理、武器挂载交互） | 未验 |
| 装载图停用/销毁 | **判为 NO-GO（2026-10-28 审计）**：`Deactivate` 对 `activeNodes` 无空检查，且候选与源共享 `rootGenerator`，未证活动节点条目是克隆；污染源图比崩溃更糟。收益只是每进程一张图的登记与内存，不阻塞 G3–G6。见[审计](#g1-teardown-audit) |
| 全局登记计数 | **无锚点、不立项**：v35 探针在 23:16 上机读回 `300474432->300474432`，重审指令确认 `[0x315C950]+0x228` 是不变的奇偶/序列值、`inc` 未写回、`63529` 只是无锁追加而非登记；旧审计那句话判为误读（[更正](#g1-teardown-audit)） |
| G3–G6、独立 OAR 语境 | G3-A、G3-B/F 相位回放保留；G4 A/B/C 保留（C 已在修复版仪器上重跑关闭）；E1 修复版取证缺口已闭合（[v69 第二轮](#c-g5-e1-v69-run2)），输出完成/寿命/同代快照待原生审计，G5/G6 未通过 |
| 缺失项逐名与语义核对 | v33 的 `missing=7` 只按预算列了 4 个名字，其余 3 个未列名，不能逐一断言全部非骨 |
| 帧率无关性 | [v44 本地代码](#v44-display-clock)按 eligible elapsed 时间累计 60 Hz，100 ms/六步上限；30/120 FPS 两个离线场景各在两秒产生 120 步。游戏速率/画面未验；v33/v34 的每调用记录仍为历史 |

<a id="deploy-state"></a>
### 当前部署与开关

- **最近记录部署：v92（G5-E2-C clock ms）**：DLL `fe697ba91ffdd36c0eccd62c4a458405bbab946702a27327140e8ce72d8bcb38`，PDB `f57673ce1a618203e4c278d70138793cc0fd03731bbca88c8256cab8a1e11914`，身份与备份见 [v92 构建/部署](build.md#v92-g5e2c-ms-2026-11-10)。这是历史部署记录，本次文档工作未重新核验安装目录。
- **最近记录 INI：E4 运行档** `8864075e…`（`SourceTimeline=on`、`SourceTimelineGraph=DefaultFemale`、`SourcePoseCapture=true`、`SamplingLiveness=off`）；不是校准配置。仓库样例仍为 dormant 默认；修改设置需重启，不能直接用旧仪器运行 CAL v1。
- **最近上机：v92 2026-11-11 00:26**，四块 116 轨快照、100 条时钟行、零丢失；[运行记录](build.md#v92-g5e2c-run1-20261111)。E3 两段运行已配对 2/2；[离线校准 A 已完成](#c-g5-cal-a-run1-20261111)，[CAL v1](#c-g5-cal-v1) 的原生 B/K1–K6/时间仍 NOT_READY。
- **历史**：v86 E3 concurrent 运行、v88 R/C 运行与回 dormant、v89–v91 捕获迭代分别保留当时配置/工件；[最新摘要](#status-20261111-session)拥有后续顺序。更早的 dormant 记录不代表最近部署状态。
- v41/v42/v43 与 v46-20261029 工件的原身份和当时状态保留；v46 正常工作流实际使用 [20261030 重建](scene-graph.md#v46-manual-regression)，不可混为逐字节相同。
- 2026-11-02 本轮：实现/构建/部署 [C-G4-A4](#c-g4-a4-numeric-types)，并完成一次上机会话（22:32–22:37，[日志](diagnostics/CharacterPanel-g4a4-numeric-types-20261102-2232.log) 已归档）→ A 项关闭；其余仪器范围仍须另立评审/实现范围。

### 测试包清单

| 轮次 | ZIP | 字节 | SHA-256 前 16 位 |
| --- | --- | --- | --- |
| v20 | `CharacterPanel-behavior-graph-v20-generation-entry-20261027.zip` | 9,750,395 | `076cf6f7…` |
| v20a | `…v20a-chain-fix-20261027.zip` | 9,747,510 | `f4586e16…` |
| v21 | `…v21-pose-content-20261027.zip` | 9,928,507 | `4cfa5ac8…` |
| v22 | `…v22-alias-watch-20261027.zip` | 9,921,485 | `be5bd284…` |
| v23 | `…v23-clip-ref-20261027.zip` | 9,928,402 | `05ac4a3e…` |
| v24 | `…v24-layer-census-20261027.zip` | 10,015,469 | `68991ca7…` |
| v25 | `…v25-step-clock-20261027.zip` | 10,029,342 | `15820120…` |
| v26 | `…v26-pose-step-20261028.zip` | 10,029,787 | `d3d7f1b7…` |
| v27 | `…v27-display-pose-20261028.zip` | 10,018,217 | `6a195266…` |
| v28 | `…v28-display-step-20261028.zip` | 10,015,286 | `d2965ca8…` |
| v29 | `…v29-display-pipeline-20261028.zip` | 9,996,292 | `42f92e94…` |
| v30 | `…v30-display-gates-20261028.zip` | 9,986,675 | `1ce2b765…` |

历史包目录约定为 `build/behavior-graph-v2*-2026102*/`，被 `.gitignore` 忽略；完整身份与 manifest 哈希记录在各轮构建段（[build.md](build.md)）。本轮本机未发现 v30 包，没有生成 v31/v32 测试包。

### 复现入口

- **构建**：`cmake --preset Release -DCHARACTER_PANEL_BUILD_LABEL=<轮次>` → `cmake --build --preset Release`；先清掉小写 `http_proxy` 等变量（否则 MSBuild CL 任务报 MSB6001）。
- **当前引擎无关用例**：v42 仅扩展并运行 [behavior_graph_display_session_test.cpp](../tests/behavior_graph_display_session_test.cpp) 中两个连贯场景，MSVC14.44 命令见 [v42 build](build.md#v42-residual-corrections-2026-10-29)。旧五个 policy 用例的结果属于历史记录，本轮未重跑；CMake 仍未注册测试目标。
- **G4 两轮补测的离线用例**（2026-11-02/03）：[behavior_graph_candidate_window_test.cpp](../tests/behavior_graph_candidate_window_test.cpp)（6 例：过预算仍计数、对账恒等、take 清零、scope 外忽略、无跨窗残留、type3 行限额，产物在 `build/behavior-graph-v63-g4a4-numeric-types/`）与 [source_event_capture_test.cpp](../tests/source_event_capture_test.cpp)（新增 `mode_text_maps_to_three_states` + 既有环/预算/取消三例，产物在 `build/behavior-graph-v64-g4c1-concurrent-events/`）；命令均为 `cl /nologo /std:c++latest /EHsc /W4 /WX /utf-8 /I src`。
- **G4 补测的构建标签**：`v63-g4a4-numeric-types`、`v64-g4c1-concurrent-events`、`v65-g4c1-resolver-fix`、`v66-g4c1-pinned-graph`；各组身份/哈希/部署与运行记录见 [Build](build.md)。

- **静态审计**：`C:/Python314/python.exe`（已装 pefile + capstone 5.0.7），解包镜像与脚本在 `build/g1-audit-20261023/`，单函数反汇编用 `disasm_func.py <rva>`。
- **运行日志**：以 `SKSE::log::log_directory()` 返回的目录中 `CharacterPanel.log` 为准（见 [main.cpp](../src/main.cpp) 的 `initialize_log`）；历史机器使用 Documents 下的 SKSE 目录，不应据此猜当前路径。每轮日志与匹配 DLL/PDB、INI 一起保留。

## 目标与边界

验证能否在不创建第二个 Actor、保持原角色行为图只读的条件下，复用原行为项目与状态机，让预览图自行激活动作，再接入 OAR。现有装备驱动播放器继续工作；本实验不替换它，不改变动作入口、DAC、IED、变体、装备或源图状态。

显示场景图副本不包含完整的行为图运行实例。`oar::PreviewContext`、`PreviewClip` 是既有加载/查询操作的载体，也不是独立图。借用 `hkRefPtr`、复制指针、复制对象内存或让剪辑时间前进都不能作为独立实例构造的证明。

先离屏验证，第一批行为实验仅有两个：原版 idle；一种单手武器的 Equip → Idle → Unequip。运行时节点、变量值、状态机、事件队列、时钟和输出必须隔离；静态资源只有在读写范围与生命周期得到证实时才允许共享。

## 来源与历史

1. 本地 CommonLib 基线 `94faaed0c60eddd8347767f2d4d29a97c93bde8c` 及当前维护的声明：`BShkbAnimationGraph`、`hkbCharacter`、`hkbBehaviorGraph`、`hkbCharacterSetup`、`hkbVariableValueSet`、`hkbNode`。本地声明含维护补充，基线提交号不代表所有字段均由上游验证；本轮审计记录保留所读文件哈希。它们提供布局与运行方法声明，不等于完整的原生构造协议。
2. OAR 3.1.6 源码固定在 `0c1c8b38a5323b9e26df41ff9d10ce801b984a45`：[角色到外层图/Actor 的解析](https://github.com/ersh1/OpenAnimationReplacer/blob/0c1c8b38a5323b9e26df41ff9d10ce801b984a45/src/OpenAnimationReplacer.cpp)、[条件读取](https://github.com/ersh1/OpenAnimationReplacer/blob/0c1c8b38a5323b9e26df41ff9d10ce801b984a45/src/Conditions.cpp)、[活动剪辑与回调](https://github.com/ersh1/OpenAnimationReplacer/blob/0c1c8b38a5323b9e26df41ff9d10ce801b984a45/src/ActiveClip.cpp)。本地逐项审计使用 `build/oar-review-316` 缓存；不复制上游实现，不新增依赖。
3. [HKX37 实测判读](history-animation.md#puppet)：第二个真实 Actor 在暂停期被手动推进 3300+ 帧，索引级决策与采样机制成立。暂停期一次性装备动作未出现；完整映射/有效骨覆盖未通过；第 5 按清理未执行。该结果既不是“暂停期推图失败”，也不是无第二 Actor 独立图的证明。
4. HKX37 所引用的 H05 隔离检查属于当时的 Actor/场景实验；新方案仍需重新证明源图状态不漂移。历史总结保留当时判读及后续修正，原件从[来源索引](history-sources.md)恢复，不能把其中“路线成立”升级为本方案 G1 通过。

## G1 静态所有权与 API 审计

以下是需要在未来构造结果中逐项比较的候选清单，不是已确认的所有权说明。字段可见或地址不同不能单独证明深层隔离。

| 对象/成员 | 可写或共享候选 | 必须补充的证据 |
| --- | --- | --- |
| `BShkbAnimationGraph`、内嵌 `characterInstance`、`holder`、`rootNode` | 独立外层容器与实例；源 Actor 身份可按设计借读 | 创建/销毁配对、holder 关系、所有事件接收者以及对源 manager 的影响 |
| `characterInstance.behaviorGraph` / 外层 `behaviorGraph` | 运行实例必须隔离；不能默认二者相同 | 原生链接路径、模板与运行实例对应关系 |
| `rootGeneratorClone`、`activeNodes`、模板/克隆映射、转换数据、伪随机生成器、作业与分区内存 | 可写运行状态，必须独立 | 深克隆范围、递归别名、分配域、线程/作业完成边界 |
| `variableValueSet` | 可写变量值，必须独立 | 值及类型映射来源、合法读写入口；私有数组不得按猜测偏移读取 |
| `eventQueue`、驱动器、world、worldFromModel、poseLocal、generatorOutputs | 事件、物理和输出可写；不得借用源输出 | 输出生命周期、队列隔离、事件路由、IK/物理依赖与释放路径 |
| `boneNodes` / `numAnimBones` | 源骨挂点只读；未来输出挂点属于副本 | 全部有效骨的语义映射；表长度不等于有效覆盖 |
| setup、animationSkeleton、projectData、行为 data、rootGenerator、绑定集、projectDBData | 仅为可共享资源候选 | 哪些字段会被延迟初始化或修改、谁持有资源、加载/卸载顺序 |
| setup 的 mirroredSkeleton、criticalSection、绑定集 | 不可因位于 setup 就假定不可变 | 镜像缓存、锁和运行绑定更新的真实所有权 |
| `hkRefVariant` payload / 未知成员 | 所有权未核实 | 本轮只取已声明包装器里的指针身份，不解引用 payload 或遍历未知结构 |

`IAnimationGraphManagerHolder::ConstructAnimationGraph` 已声明为虚函数（slot 05），`TESObjectREFR`、`SimpleAnimationGraphManagerHolder`、`WeaponAnimationGraphManagerHolder` 有 override。2026-10-23 的[二进制审计](behavior-graph-construction-audit.md)确认五种实现共用分配/构造/赋引用模板；构造本身不加载项目、不登记 manager。后来已定位 63585 装载、字符级激活、63602 生成管线与 63587 更新包装，并按批准实验实际调用，结果见下文各轮记录。不能继续把“加载/激活未定位”当现状，也不能把空壳配对释放推广为已装载图可安全销毁。`BSAnimationGraphManager::QueryAnimations` 的现有 AE relocation 为 0，仍不能作为 AE 1.6.1170 可调用证据；OAR holder/rootNode 与事件路由须按 G4 另验。

`hkbNode` / `hkbBehaviorGraph` 的 Activate、Update、Generate、Deactivate 声明只能说明运行接口存在；未完成实例装配时禁止调用。裸 `hkbCharacter` 也不满足 OAR 从其反查 `BShkbAnimationGraph` 容器和 holder 的假设。

OAR 隔离还包括图外状态：图变量和 `IsWeaponDrawn` 可通过 Actor 读取；Random、顺序、PlayOnce 的状态可能按 Actor/ref、子包或剪辑作用域保存；`ActiveClip` 可注册图事件 sink，并沿源 manager 回调。图对象地址不同不能消除这些共享关系。

## 六道关卡

每关保留原始记录、构建身份、配置、初始条件、操作顺序、判据与失败点。前关未通过不得开始后关。遇到未知 ABI、需要写源图/创建 Actor、不能解释的别名或无配对销毁路径，立即停止该实验；保留日志后回到方案评审，不引入猜测调用或隐藏回退。

### G1：构造、链接、所有权与销毁

先确认目标游戏版本下的创建入口、分配器、链接顺序、holder/manager 容器要求、停用与销毁入口，再创建一个离屏候选。逐项记录源与候选的对象身份、运行状态归属、共享资源理由、事件接收者、引用/登记/任务数量与逆序销毁结果。源只读基线只提供之后对照的坐标。

原始完整门槛（保留为所有权/生命周期验收目标）：原生装配成功、可写状态隔离且共享有依据、源 manager/Actor 无副作用、逆序销毁回到基线；别名跨入可写状态、借用源输出或无配对释放均不能通过完整门槛。**当前 G1 的 PASSED 仅采用 2026-10-24 [方案 A 修订](#2026-10-24-方案-a-修订销毁判据经-g1b-v3-崩溃后评审)**：[G1a](#g1a-运行记录2026-10-24-会话)证明空壳配对销毁；[G1b v4](#g1b-v4-运行记录与-g1-判定2026-10-24-会话)重复装载并保留候选，采集范围内源基线全等、无崩溃。装载图停用/销毁移交后续生命周期验证，尚未通过；全局登记计数、资源收敛和全部运行期写隔离仍 UNVERIFIED。

### G2：原版暂停期单边推进

G1 通过后，在无 OAR 的原版基线只推进候选图。使用固定记录的时间步，采集候选剪辑时钟与输出姿态、源活动节点/变量/时钟/骨骼前后差分。窗口内禁止合法装备操作混入对照；用户换装备后重新建立基线。

通过：候选原版 idle 持续、有意义地前进，必要骨的有效变换变化可验证，源保持对照；时钟、索引、状态机、元素头或原始分量变化单独都不够。**v33 已在限定范围通过**：候选语义运动 120/120、109/116 实际赋值、源对照不变及用户确认画面连续。v26 raw119/120 不单独证明运动；所保留的有限、私有缓冲、层归属、时钟证据仍有效。物理/挂载交互、全部有效骨覆盖、装载图生命周期和 G3–G6 仍未通过。

前置激活/推进审计及 v1–v17 实现记录已保留；[最初 G2 契约](#2026-10-24-g2-契约drive-模式实验设计)的 63586/裸 Update 配方后来迭代为字符级 58453 激活、63602 管线、63587 包装与 v17 的 63581 直调。调用入口已定位不等于输出链可用，当前后续问题以[有界审计清单](#next-audit)为准。

### G3：图原生的 Equip → Idle → Unequip

正常游戏运行时先取得同一单手武器的相位轨迹，再核实外部变量/事件的写入者、角色与运行时类型及其先后时序。G3-A 已取得 settled 观测轨迹，但不是 setter/事件拦截；完整外部输入与时序仍需证据。后续只把已论证的等价外部输入送到候选图，不能手动指定动作族/HKX 或写输出状态来“通过”。输出事件先只收集，不派发到源 Actor。

通过：候选状态机自行完成相同顺序，并能按事件和相位与原版轨迹对应。事件丢失、阶段变量仍读源值或靠播放器强制切片均失败。完整 G3 当前未执行；G2 定位轮已派发一次 WeapEquip 并观察到状态转移，不能计为整段装卸链或原生 OAR 一致性通过。

### G4：OAR 上下文、历史与回调隔离

真实候选节点走 OAR 原生激活、更新、中断及重查流程。逐条件记录来源，用哨兵实验故意让候选阶段变量与源变量不同，确认实际读取谁；身份/装备允许按明确设计借读，阶段状态必须对应候选。记录候选与胜者、变体、播放相位、重查原因、事件 sink、Actor/ref/子包/clip 的状态作用域。

对照运行 A：直接继续世界动作；运行 B：中间运行预览，再继续世界动作。两次保持相同初态与随机/顺序控制，检查预览是否消耗世界 Random、顺序或 PlayOnce 历史。关闭预览只清理自己的登记，不能清空源 Actor 的全部 OAR 状态。DAC 要在事件捕获和隔离证明后另行接入既有受控通道。

通过：读取来源符合预览语境，历史/回调/清理隔离成立。相同替换路径但相位不同仍需解释。当前未执行。

### G5：完整输出姿态对照

按相同初态、事件和播放相位对齐，比较全部有效贡献层、权重、混合参数、播放速度及最终骨骼变换；记录 IK/后处理及坐标空间。实验开始前固定位置、旋转、缩放误差阈值及其依据，不能看结果后放宽。按有效骨名称与语义证明完整覆盖，不能拿骨表长度代替。

通过：完整层/骨覆盖且误差满足预先约定阈值；相同选片而输出不一致仍失败。当前未执行，阈值尚未制定。

### G6：生命周期与资源恢复

记录创建前/运行中/关闭后的源状态、请求/任务代数、预览持有的引用、输出、回调、事件 sink、OAR 登记与历史。重复会话、快速换装、重捕获、关闭、读档时，先使旧请求/任务失效，再解除预览拥有的回调/引用，并验证可观察计数与状态回到基线且不持续累积。

**显式例外**：已装载候选刻意每进程保留一张，不尝试停用/销毁（NO-GO）；此项不作为“全部释放”的通过条件。全局引擎登记计数没有可靠测量锚点，不能当作可测量的通过项。其余可观察的预览引用、回调/sink、历史、请求/任务与重复会话收敛仍须验证，不能借例外省略。

通过：源恢复、旧工作失效、预览自有关系正确解除，重复会话无非例外积累。`DAC restore … result=matched` 仅支持对应效果恢复；无崩溃也不等于 G6。当前 G6 未执行。

## G1 实验契约（2026-10-23 设计，未实现）

> **历史契约标记（2026-10-26）**：标题和原预注册条件保留。实验已于 10-24 实现/运行，并在方案 A 下修订销毁判据；以下“未实现/不得调用”限定该历史阶段，当前结论见[文首](#current-status)。

本契约把[二进制审计](behavior-graph-construction-audit.md)的构造/销毁/加载/共享语义转成可执行的 G1 运行实验：只做"构造 → 装载 → 对照 → 逆序销毁"，不激活、不推进、不写源图、不挂 OAR。实现前不得运行；实现只允许出现在下述边界内。

### 实验分级（前一级通过前不得进入下一级）

| 级 | 动作 | 目的 | 停止条件 |
| --- | --- | --- | --- |
| G1a | 自持 holder（槽位语义见下）+ `holder=nullptr` 构造空壳，然后只经引用计数释放 | 在运行中验证工厂与配对销毁、无源副作用 | 构造返回空/崩溃、源基线漂移、释放后计数不回 |
| G1b | G1a + 按预注册优先级顺序以**完整项目路径 `const char*`** 调用加载 63585（≤4 个候选：`race->behaviorGraphs[sex].model` → `behaviorGraphProjectNames[sex]` → `rootBehaviorGraphNames[sex]` → `stringData` 重建；第三实参 false），首个成功即停；**成功后按[方案 A 修订](#2026-10-24-方案-a-修订销毁判据经-g1b-v3-崩溃后评审)保留候选（不销毁）** | 验证候选能完成原生装配（behaviorGraph/character/setup/rootNode），同时一次性关闭"引擎键来自哪个 race 字段"的取值问题 | 返回空、装配字段缺失、需要写源图、注册表计数不回 |
| G1c | G1b + 以源 Actor 作为 holder 重建一次对照 | 仅在 G1b 通过且后续需要源上下文时量化别名 | G1b 未通过；出现可写状态同址 |

### 候选 holder 与调用边界

- G1a/G1b 不实例化 holder 对象：直接复刻审计模板——`MemoryManager::Allocate(0x250, 0x10, true)` → 原生构造 63555（holder 实参 `nullptr`）→ `BSTSmartPointer` 持有。审计已证明五种 `Construct` 实现共享同一模板，vtable 只是派发点；不派生 `IAnimationGraphManagerHolder`，避免为引擎接口类自定义析构符号（CommonLibSSE 只声明、未定义 `~IAnimationGraphManagerHolder`）。理由：引擎自身以 `holder=null` 构造（Simple/Weapon）且该路径不向源 Actor 注册事件 sink/通道。
- 工厂实现必须复刻审计模板：`MemoryManager::Allocate(0x250, 0x10, true)`（68115）→ 构造 `BShkbAnimationGraph`（63555，签名 `this/holder/bool`，审计所得）→ 引用计数智能指针赋值；不得自行拼装成员。释放只允许丢弃最后一个 `BSTSmartPointer` 引用，禁止手动调用删除析构或 `Deallocate`。
- 加载只调用 63585。**（2026-10-24 续审计二/三修订并实现，v3）第二实参为 NUL 结尾的完整项目路径 `const char*`。候选值来源按预注册优先级：① `holder->GetRace()->behaviorGraphs[sex].model`；② `behaviorGraphProjectNames[sex]`；③ `rootBehaviorGraphNames[sex]`；④ `stringData->behaviorPath + projectName` 重建（69487 拼接规则）。顺序尝试、首个返回 true 即停，最多 4 次；已证实失败加载对空壳是零副作用操作（0xbb21a9 路径全空引用），重复尝试不引入新风险。`sex` 取 `TESNPC::GetSex()`（kFemale=1），非 NPC 按 0。第三实参 false：Actor/PlayerCharacter holder 的 Unk_06（vtable 槽 6）实测返回 `xor al,al`（false）。不得调用 63844/63845/63847 或任何 `hkbCharacter` 成员函数，除非先以只读日志确认；见[审计记录](behavior-graph-construction-audit.md#加载取值与前置链的续审计三2026-10-24g1b-v2-复盘)。**
- 禁止：Activate/Update/Generate/Deactivate、图变量写入、图事件、OAR 求值/`ActiveClip`、DAC、创建 Actor/REFR、写 `+0x1F0/+0x1F8/+0x200/+0x208/+0x218`。
- **保留（2026-10-24 方案 A）**：装载成功的候选不销毁——以堆上副本的 `BSTSmartPointer` 持有且永不释放（泄漏），静态析构与进程退出都不触发引擎析构链；空壳（构造失败、全部候选被拒）仍按引用计数销毁。
- 新设置 `[Experiment] IndependentGraph=off|construct|load`，默认 off，重启生效，独立于 `DetailedAnimationTrace`/`BehaviorGraphProbe`；新模块 `src/animation/behavior_graph/independent_graph.{h,cpp}`，不新增目标/依赖。

### 观测与预算

每级最多 24 行 `INDEPENDENT GRAPH`：候选 `graph/character/setup/behaviorGraph/projectDBData/projectName/rootNode/variableValueSet/activeNodes/generatorOutputs/eventQueue` 的地址与计数，以及 12 项身份比较（shell/load 各一组）；`load` 级另有 1 行 `phase=race-fields`（只读记录 `bg-model`/`root-name`/`project-field`/`string-data-behavior-path`/`source-project-name` 的**内容**）与每候选一行 `phase=load-try`（序号、来源、路径内容）+ `phase=load`（结果与装配字段）——2026-10-24 修订，v2 实测证明"日志不含所传内容"会浪费整个运行。构造前与销毁后各一次完整源基线（复用 `BehaviorGraphProbe` 的 12 行固定字段），构造后使用紧凑身份比较，避免三份完整基线。全局注册表计数（单例 `0x315c950` → 数组 `+0x1F8` → 计数 `+0x30`，AE-only 审计地址）未实现，日志固定输出 `registry-probe=unavailable`，报告中必须注明该侧效应未观测。只读、无指针逃逸、无线程间共享状态。

### 预注册判定（不得事后放宽）

通过（全部满足）：

1. **装配完整**：G1b 后 `behaviorGraph`、`projectDBData`、`projectName` 与 `behaviorGraph->data` 非空；`characterInstance` 与源不同址。且（2026-10-24 修订）候选 `projectName` 的**内容**等于被接受候选路径的文件名段去扩展名（与[审计](behavior-graph-construction-audit.md#第二实参形态与拒绝路径2026-10-24-续审计二)的 69489 写入点一致）。`rootNode`/`setup`/`bindingSet` 为空属预期（无骨架挂接、无场景节点），必须记录但不计失败；G2 需要时另行设计。
2. **可写状态独立**：候选 `graph/character/activeNodes/variableValueSet/generatorOutputs/eventQueue` 与源**不同址**。
3. **共享资源可解释**：五个 hkRefPtr 字段与 `projectDBData`/行为数据与源**同址**（引用计数 +1 属预期，不计漂移）。
4. **源基线不变（2026-10-24 方案 A 修订）**：构造前 ≡ **装载后（候选存活期间）**；装载图的销毁后基线不再适用（销毁移至 G2 生命周期），空壳路径维持"销毁后 ≡ 构造前"。
5. **逆序销毁（2026-10-24 方案 A 修订）**：空壳路径维持引用计数配对释放、无崩溃/断言；**装载图不再销毁**——引擎不支持未激活图的销毁（[续审计四](behavior-graph-construction-audit.md#已装载图析构的崩溃定位2026-10-24-续审计四)），候选按保留流程存活至进程结束，其销毁证据由 G2 生命周期取得。注册表计数未观测（`registry-probe=unavailable`），回落保持 UNVERIFIED。
6. **无侧效应**：源 manager 的图数量不变；OAR 活动查询与 `ActiveClip` 回调未对候选触发；无新增 Actor/REFR。

失败（任一）：可写状态同址、源基线漂移、注册表计数不回落、构造/装载返回空或崩溃、必须写源图才能完成。崩溃未出现不能替代清理证据。

### 停止与回退

任一失败/未知：保留日志与构建身份行，把设置改回 off 重启，不进入下一级，不做"调整判据后重试"。装备变化后重建源基线；F7 重捕获不得叠加构造。G1 通过前 G2–G6 保持锁定，本契约不定义推进/激活。

### 2026-10-24 方案 A 修订（销毁判据，经 G1b v3 崩溃后评审）

[续审计四](behavior-graph-construction-audit.md#已装载图析构的崩溃定位2026-10-24-续审计四)确立引擎事实：`~BShkbAnimationGraph` 在 `behaviorGraph` 非空时调用 `hkbBehaviorGraph::Deactivate`（槽位 7），后者无条件读取 `activeNodes`（Activate 时才分配）——**装载后未激活的图没有安全销毁路径，"不激活而销毁"不在引擎生命周期内**。据此修订：

- **销毁证据重定义**：G1 的配对释放证据由两部分承担——①空壳销毁（G1a 已通过）；②装载图的销毁**推迟至 G2 生命周期**（激活 → 推进 → 停用 → 销毁）内取得。
- **v4 保留流程**：装载成功后，候选以**永不释放的持有智能指针**（堆上副本，刻意泄漏；静态析构与进程退出都不触发引擎析构链）保留至进程结束；每进程 ≤1 张装载图，仅在实验开启时发生，独立图功能化之前不作为可交付路径。
- **v4 预注册判据（不得事后放宽）**：①装载成功复现（首个 `race-bg-model` 候选 `accepted`）；②**装载后源基线**（候选存活期间采集）与构造前逐字全等；③`phase=end retained=true`、无崩溃；④①–③ 全过 → **G1 PASSED（修订判据）**，G1c 跳过（`holder=nullptr` 已证充分，源 holder 别名量化移至 G4 的 OAR 语境）；⑤装载后基线漂移或再次崩溃 → 维持 NOT PASSED，回评审。

### 2026-10-24 G2 契约（drive 模式，实验设计）

> **历史预注册标记（2026-10-26）**：本节保留当时配方、预期和判据，不将后验结果改写为事前预测；后续反例及当前证据边界见[当前结论](#current-status)与 [v17 后验勘误](#g2-v17-运行记录2026-10-26-会话谓词翻假达成与采样门23落定)。

G2 验证：候选图可被激活并以固定时间步推进，且源全程不变。新设置值 `IndependentGraph=drive`，复用 G1b 全流程（构造 → 装载 → 保留，全部观测行不变），装载成功后**同请求、同暂停括号内**继续：

1. **激活**：单次调用引擎激活包装器 **63586（候选）**——[续审计五](behavior-graph-construction-audit.md#激活与推进入口g2-前提2026-10-24-续审计五)确立的引擎入口，零自建结构、零管理器依赖。
2. **驱动**：以 58315 自建 context（`character = 候选 characterInstance`，world/attachmentManager/animationCache 空）直调 `hkbBehaviorGraph::Update`（58329，vtable 槽位 5），`DriveSteps = 120` 次、固定时间步 `1/60 s`（预注册常量，不调 63587——其更新记录依赖管理器，候选无 manager）。
3. **观测（分层，预注册）**：
   - 结构层：`activeNodes(+0x98)` 指针与计数（NodeList+8 dword，[续审计四](behavior-graph-construction-audit.md#已装载图析构的崩溃定位2026-10-24-续审计四)的崩溃指令即读此处）、`+0xA0/+0xA8`、`variableValueSet(+0xD8)`、激活标志(+0x12C)、`generatorOutputs[0/1]`、`poseLocal/numPoseLocal`——激活前/激活后/驱动后各一次；
   - 运动层：FNV-1a 64 姿态哈希——**源侧对照**（激活前 vs 驱动后：暂停括号内源不应被推进，必须全等）与**候选侧**（激活后 / 第 60 步 / 第 120 步：idle 播放应互不全等）；`poseLocal` 空则记 `unavailable`。
4. **预注册判据（不得事后放宽）**：
   - ①激活后 `activeNodes` 非空、激活标志 = 1；
   - ②驱动后候选出现前进证据：候选三级姿态哈希不全等；若姿态不可用，以结构层由空转非空的变化为准并逐项解释，不得默认通过；
   - ③**源全程不变**：激活前（即装载后）与驱动后源基线逐字全等、compact Identity drift = none、源姿态哈希前后全等；
   - ④无崩溃/断言，同会话后续面板开合正常。
   - ①–④ 全过 → **G2 PASSED**；任一失败 → 崩溃/异常日志归因后回评审。
5. **停止与回退**：同 G1（INI 改回 off）；drive 每进程一次；候选保留策略不变（永不释放的持有指针）。`registry-probe=unavailable` 保持。
6. **2026-10-24 v2 修正（[续审计六](behavior-graph-construction-audit.md#姿态写入链与字符级激活2026-10-24-续审计六)）**：首轮判读显示图级激活 63586 绕过了 pose 分配——引擎的激活顺序是"**先 pose 缓冲、后 Activate**"，由**字符级激活 0xabd630**（守卫 `+0x12C==0`，先调 58466 AllocatePose——从 `setup.animationSkeleton` 读骨骼数分配 `poseLocal` 并置 `deletePoseLocal=1`——再 58315 context + Activate）完成。v2 改动：激活改调 0xabd630（地址库无 id，AE RVA）；**预注册守卫**——激活前若 `setup.animationSkeleton == 0` 则记录 `skip=setup-skeleton-missing` 干净结束（不调用、不崩溃，结论直接指向 animationSkeleton 填充链审计）；判据①追加"**`poseLocal` 非空（AllocatePose 生效）**"，判据②的三级姿态哈希在 poseLocal 可用后才有意义。首轮运行记录见[运行记录](#g2-drive-运行记录2026-10-24-会话)。

### 2026-10-24 G2 v7 契约（引擎管线驱动，经续审计七设计）

> **历史预注册标记（2026-10-26）**：本节保留当时配方、预期和判据，不将后验结果改写为事前预测；后续反例及当前证据边界见[当前结论](#current-status)与 [v17 后验勘误](#g2-v17-运行记录2026-10-26-会话谓词翻假达成与采样门23落定)。

[续审计七](behavior-graph-construction-audit.md#管理器每帧管线与记录全解2026-10-24-续审计七)把 v6 时代的"管理器动画选择"结论修正为：记录无动画身份、63575 = LOD 解析器、63583 = 排空+簿记（第三实参 0 时无害）；缺口 = **生成遍历（65237）从未执行**。据此 G2 判据按本契约重定义，v5 的裸 Update 驱动作废：

- **设置不变**：`IndependentGraph=drive`；G1b 装载全流程与 58453 字符级激活（含 `setup-skeleton-missing` 守卫）照旧，全部既有观测行不变。
- **每步驱动（DriveSteps=120 × 1/60 s 不变）**，复刻引擎通知中心槽 5（[graph_helper](behavior-graph-construction-audit.md#管理器每帧管线与记录全解2026-10-24-续审计七)）的配方：
  1. **生成管线**：栈上 `63555(&壳, nullptr, false)` → **`63602(&壳, 候选)`**（事件收集 + 65149/65232/65237 生成遍历写 `generatorOutputs` + `+0x12E=1` + 零 dt 63587 Update + 63583 排空 + 63584）→ `63556(&壳)`——每步一对空壳构造/真析构（引擎自身对空壳的用法，[续审计四](behavior-graph-construction-audit.md#已装载图析构的崩溃定位2026-10-24-续审计四)的崩溃仅在已装载图上）。
  2. **实 dt 推进**：`+0x12E=1` → 58329(behavior, ctx, 1/60)——v5 已证配方（63602 内部 Update 为零 dt，不推进状态）。
  3. **采样**：`候选+0x240 = numPoseLocal` 镜像 → 63589——v4 已证配方（63587 入口会把 `+0x240` 清零，镜像必须在采样前）。
- **观测**：既有三级姿态哈希 + 结构快照不变；`gen0elem` 元素哈希（已在 probe 行）现在应随生成而变化；新增一行 `phase=pipeline steps=120 shell=pairs` 汇总（预算 +1 行）。
- **预注册判据（不得事后放宽）**：
  - ①候选前进证据：三级姿态哈希不全等；**或** `gen0elem`/`gen1` 元素哈希随步变化而姿态仍冻结——后者按"采样链归因"记录（63581 轨道偏移 vs 候选输出布局的新审计），**不得默认通过**；
  - ②源全程不变：三处源基线逐字全等、compact drift=none、源姿态哈希前后全等；
  - ③无崩溃/断言，同会话后续面板开合正常；
  - ①（前进证据成立）+②+③ → **G2 PASSED（v7 判据）**；生成无产物（gen 元素恒定）→ 归因"默认态无激活生成器，idle 需事件注入"，作为 G3 事件机制的入口证据记录，G2 保持 NOT PASSED。
- **失败归因表（预注册）**：gen 元素变化 + 姿态冻结 → 采样链（63581/63589 与候选输出布局）；gen 元素恒定 → 默认态无生成器（转 G3 事件注入设计）；崩溃 → 归因后回评审，INI 回 off。
- **停止与回退**：同 G2（INI 改回 off）；drive 每进程一次；候选保留策略不变。新增调用面 = 63602/63555/63556（引擎自身配方）+ 既有 58453/58315/58329/63589，不新增依赖/目标。

### 2026-10-24 G2 v8 契约（inject 模式：事件注入判别，经续审计八设计）

> **历史预注册标记（2026-10-26）**：本节保留当时配方、预期和判据，不将后验结果改写为事前预测；后续反例及当前证据边界见[当前结论](#current-status)与 [v17 后验勘误](#g2-v17-运行记录2026-10-26-会话谓词翻假达成与采样门23落定)。

[续审计八](behavior-graph-construction-audit.md#事件派发链与槽-2-输出门2026-10-24-续审计八)把事件派发原语定位到 **58377**（hkbBehaviorGraph 槽 6 落体；Update 尾部经槽 6 桩派发队列事件；批量派发器 0xab313e 同构），且证实事件名不在 exe、由 ProjectDBData 名字→id 映射运行期解析。据此设计注入判别实验，回答 v7 遗留判别："65237 无激活生成器" vs "管线到达性/输出绑定门"：

- **设置**：新值 `IndependentGraph=inject`。流程 = v7 全部（装载 → 激活 → 每步 63602 管线 + 实 dt Update + 镜像 + 采样 ×120，观测行不变）+ 两个新增阶段。观测预算修订为 30 行（预注册）。
- **阶段 1 事件表只读查询（注入前，激活后；v8d 修订）**：先保留 `phase=dbdump` 原始 qword 行；新增 `phase=events` 行输出行为图事件表计数与前 16 个真实事件名（证据）；名字→id 解析用 **行为图自带 `stringData->eventNames` 线性扫描**（`hkArray<hkStringPtr>`、掩 managed 位、**id 即下标**——状态机转移映射键控的 id 空间）按**预注册名字表**顺序查询（v8e 修订：按 v8d `phase=events` 证据行重定，原 `MT_Idle`/`MoveStart`/`WeaponDraw`/`SyncForward` 经证非图事件）：`WeapEquip` → `SprintStart` → `FootLeft` → `FootRight`，每名字一行 `event-lookup name='X' id=N source=graph-events`（miss 为 −1）。原 ProjectDBData 哈希表遍历经两轮崩溃 + dbdump 判读证伪（墓碑键不可解引用，引擎只做指针比较），unkD0 判读为剪辑名表，均已移除。
- **阶段 2 注入**：取首个 id≥0 的名字，构栈上 24 B hkbEvent `{id, 0, payload=0, sender=0}`，在 drive 循环第 0 步管线之前**单次直调 58377(behavior, ctx, &event)**（与既有 slot-5 直调同类；不写源图、不触共享资源引用计数）。全部 miss 则 `skip=no-known-event` 干净结束。
- **预注册判据（不得事后放宽）**：
  - ①**前进证据**：注入后候选姿态三级哈希不全等，或 gen 元素哈希相对注入前基线出现变化 → **事件注入即最后缺口，生成链活**；G2 前进证据以"事件驱动"形式成立，注入路径记为 G3 的输入原语；
  - ②注入后 gen 元素仍恒定 → 按预注册归因转 **槽 2 输出绑定门**（[续审计八](behavior-graph-construction-audit.md#事件派发链与槽-2-输出门2026-10-24-续审计八)第 6 条：physicsWorld/ragdollDriver 双空门）→ v9 设计评审（受控借用源 world 需风险登记），G2/G3 维持未过；
  - ③源全程不变（基线逐字全等、源姿态哈希前后全等、drift=none）；④无崩溃/断言，同会话面板正常。
- **停止与回退**：同 v7（INI 回 off）；inject 每进程一次；候选保留策略不变。新增调用面 = 58377 直调（+ 只读 map 遍历），不新增依赖/目标。

### 2026-10-24 G2 v11 契约（ragdoll 实例借用判别，经续审计十设计）

> **历史预注册标记（2026-10-26）**：本节保留当时配方、预期和判据，不将后验结果改写为事前预测；后续反例及当前证据边界见[当前结论](#current-status)与 [v17 后验勘误](#g2-v17-运行记录2026-10-26-会话谓词翻假达成与采样门23落定)。

[续审计十](behavior-graph-construction-audit.md#骨骼提供者真身与填充链2026-10-24-续审计十)把 63630（63602 槽 0xB 出参）的骨骼提供者证认为 **hkaRagdollInstance**（`graph+0xF0` = `characterInstance.ragdollDriver` → `driver+0x88` 实例 → `rigidBodies` 表 `+0x10`、计数 `+0x18`），并把填充方定位到引擎完整装配 63870 的尾链（63952 建实例 → 58285 SetRagdoll）——候选装载链止步 63949，+0x88 恒 null，v10 的 world 借用因此通过全部门而绑定循环计数仍 0。v11 = 在 v10 借用窗口内**同时借用源的 ragdoll 实例指针**，判别"逐骨绑定循环即最后缺口"：

- **设置**：不变（`IndependentGraph=inject`）。流程 = v10 全部 + 驱动窗口内写 `candidate 驱动 +0x88 = 源实例指针`（raw 指针拷贝、不动引用计数——恢复先于任何可能释放者），循环后与 world 同点恢复原值（预期 null）。`phase=driver` 观测行同构建修正读链（`driver+0x88` → 实例 → `{+0x10 表, +0x18 计数}`，并补 `driver+0x80` 的 character 回指；续审计九部署的旧读链读的是 blend-out float 与模式字节，从未上机取证）。`phase=borrow` 行扩为 `world={} ragdoll=borrowed|skipped-no-driver|skipped-no-source-ragdoll saved={}`；`phase=pipeline` 行增 `ragdoll-restored=`。
- **预注册判据（不得事后放宽）**：
  - ①**前进证据**：驱动后候选姿态三级哈希不全等，或 gen 元素哈希 / 元素 `+32/+0x38` 绑定字段相对基线变化 → 逐骨绑定循环即最后缺口，G2 前进证据成立；候选侧正解转入 G3+（63952 实参重构，按候选自身项目数据建实例，不再借用）；
  - ②借用执行（`ragdoll=borrowed`）而输出仍恒定 → 归因转**循环内逐骨元素配对 / 生成遍历到达性**（续审计十第 5 条之外的新缺口），G2/G3 维持未过；
  - ③`skipped-no-source-ragdoll` → 证伪"源 rigidBodies 已填充"前提（与 v9 源元素 `+32=0x74` 证据冲突，需复审 63630 循环体与元素字段来源）；
  - ④源全程不变（基线逐字全等、姿态哈希前后全等、drift=none）；⑤无崩溃/断言，同会话面板正常。
- **风险登记**：63630 逐刚体把体挂进（已借用的源自身）world——对源幂等（其每帧管线本就如此）；逐骨 0x58 输出记录与 `{count|0x80000000, ptr}` 描述符走引擎全局簿记，与任意图的帧管线同型；120 步 × 源骨数（约 116）为上界。
- **停止与回退**：同 v10（INI 回 off）；inject 每进程一次；恢复块与 world 恢复同点（循环后立即）。

### 2026-10-25 G2 v12 契约（槽 2 直调判别，经续审计十一设计）

> **历史预注册标记（2026-10-26）**：本节保留当时配方、预期和判据，不将后验结果改写为事前预测；后续反例及当前证据边界见[当前结论](#current-status)与 [v17 后验勘误](#g2-v17-运行记录2026-10-26-会话谓词翻假达成与采样门23落定)。

[续审计十一](behavior-graph-construction-audit.md#槽-2-真身与元素字段写入链2026-10-25-续审计十一)证认槽 2（63621）= AddRagdollToWorld 且其头部即 +32/+38 的写入者（过三门后无条件 `numData=0`、`onFraction=1.0`），而 63602 只在 63643（帧内当前动画哈希，经槽 0xC 从 projectDBData +0x170/+0x178 取对象）返回 >0 时才调它——候选该链不满足，v10/v11 的 120 次遍历从未触发槽 2，候选 +38=0.0 由此解释。v12 = 直调一次 63621（候选），判别"槽 2 执行是否就是缺口"：

- **设置**：不变（`IndependentGraph=inject`）。v11 的实例借用移除（已证非载荷，候选自有 18 体）；流程 = v10 world 借用 → **直调 63621（候选）**（前置 `phase=slot2` 行：added 返回值 + 调用前后 +32/+38）→ v7/v8 全部驱动 → 驱动后先调 63622（槽 3，RemoveRagdollFromWorld）再恢复 world=null（`phase=pipeline` 行增 `ragdoll-removed=` 与 `tracks-end[]`）。观测预算 30 行（净 +1）。
- **预注册判据（不得事后放宽）**：
  - ①`phase=slot2 added=true` 且 `tracks-after +38=1.000` → 槽 2 在候选可执行、写入者身份证实；若驱动后候选姿态三级哈希不全等或 gen 元素哈希变化 → **槽 2 即最后缺口，G2 前进证据成立**；
  - ②+38=1.0 而姿态/元素仍恒定 → 残余缺口 = **numData=116 的回填路径**（生成/绑定侧；下一审计 = 锚定轨道头写入者的全镜像 store 扫描），G2/G3 维持未过；
  - ③`added=false` → 直调时三门复败（与 v10/v11 门证据冲突，复审 63621 门读）；
  - ④源全程不变（基线逐字全等、姿态哈希前后全等、drift=none）；⑤无崩溃/断言，同会话面板正常；`ragdoll-removed=false` 时登记并暂停后续借用类实验。
- **风险登记**：63621 激活候选自有 18 刚体并挂入（借用的源）world——与引擎在源上的每帧行为同型；驱动结束即以 63622 对称移除，先于 world 恢复 null；单次执行、每进程一次。
- **停止与回退**：同 v10（INI 回 off）。

### 2026-10-25 G2 v13 契约（槽 0xC 直调判别，经续审计十二设计）

> **历史预注册标记（2026-10-26）**：本节保留当时配方、预期和判据，不将后验结果改写为事前预测；后续反例及当前证据边界见[当前结论](#current-status)与 [v17 后验勘误](#g2-v17-运行记录2026-10-26-会话谓词翻假达成与采样门23落定)。

[续审计十二](behavior-graph-construction-audit.md#采样入口五道门与-numdata-写入者2026-10-25-续审计十二)定位 numData 写入者 = 63568（`[tracks+0x32] = word [graph+0x240]`），其唯一调用者 = 每步采样入口 63589——候选在 63589 的**首道门 `[graph+0x110] ≠ 0` 每步直接返回**，从未采样；+0x110 的唯一填充者 = 槽 0xC（63631，从 projectDBData +0x170/+0x178 按 dl 选择），引擎只在同一 63643 门后调用（审计十一与十二是同一缺口的两个切面：候选缺这一次装配）。v13 = 直调一次槽 0xC：

- **设置**：不变（`IndependentGraph=inject`）。流程 = v12 全部（world 借用 → 槽 2 直调 → 驱动 → 槽 3 移除）+ 槽 2 之后**直调 63631（候选， dl=false；+0x110 仍空则 dl=true 重试一次）**，新增 `phase=slot0c` 行（调用前后 `[graph+0x110]`、`[所选+0x20]`、`[graph+0x246]` 分支字节）。观测预算 32 行（净 +1）。
- **预注册判据（不得事后放宽）**：
  - ①`phase=slot0c after` 110 非空 且 `phase=pipeline tracks-end +32=0x0074` 且驱动后姿态三级哈希不全等或 gen 元素哈希变化 → **+0x110 装配即最后缺口，G2 前进证据成立**；
  - ②+0x110 填充而 +32 仍 0x0000 → 转采样内部分支（63568 dl 路径 / [graph+0x246] 分支 / 63581 路径，下一审计）；
  - ③两次调用后 +0x110 仍空 → 复审 63631 前置（projectDBData +0x170/+0x178 内容）；
  - ④源全程不变；⑤无崩溃/断言，同会话面板正常。
- **风险登记**：63631 对所选对象做 AddReference 并读 `[所选+0x28]→[+0x10]` 与 `[ragdoll+0x40]` 骨架——引擎原生簿记，单次执行；+0x110 为候选自有字段，随候选生命周期释放。
- **停止与回退**：同 v12（INI 回 off）。

### 实现状态（2026-10-24）

> 此段为 10-24 构建/部署迭代记录，保留身份和当时待办；当前状态见[文首](#current-status)。

`[Experiment] IndependentGraph=off|construct|load|drive` 已实现（默认 off、重启生效，独立于 `DetailedAnimationTrace`/`BehaviorGraphProbe`）：[independent_graph.cpp](../src/animation/behavior_graph/independent_graph.cpp) 在首次 `panel-open` 请求里构造一个 `holder=nullptr` 的空壳图；`load`/`drive` 模式（v3 取值 + [方案 A 修订](#2026-10-24-方案-a-修订销毁判据经-g1b-v3-崩溃后评审)保留）：先以 `race-fields` 行只读记录源 holder 种族的三个候选字段与 stringData 内容，再按预注册优先级顺序（bg-model → project-name → root-name → stringData 重建）以完整路径 `const char*` 调用 63585，最多 4 次、首个成功即停，第三实参 false；**成功后候选以永不释放的持有智能指针保留（不销毁），仅空壳走引用计数释放**。`drive` 模式当前为 **v7**（[G2 v7 契约](#2026-10-24-g2-v7-契约引擎管线驱动经续审计七设计)，取代 v1–v6 的裸 Update 路线）；新值 **`inject`**（[G2 v8 契约](#2026-10-24-g2-v8-契约inject-模式事件注入判别经续审计八设计)）在 v7 全流程前追加只读事件名查询与单次 58377 派发在装载成功后同括号内：激活前补源基线 → **字符级激活 58453**（先 58466 从 `setup.animationSkeleton` 分配 pose 缓冲、再 Activate；骨架缺失时预注册守卫 `skip=setup-skeleton-missing` 干净跳过）→ 58315 自建 context（raw 零初始化存储——`hkbGeneratorOutputListener` 无完整类型，hkbContext 不可在此侧构造/析构）→ 每步 **63555 栈壳 → 63602(&壳, 候选) → 63556 生成管线 + 58329 实 dt Update + `+0x240` 镜像 + 63589 采样**，共 120 步 ×1/60 s → 三级姿态哈希（源对照 + 候选 0/60/120 步）、gen 元素哈希与结构快照。每进程只跑一次，AE-only 且版本 ≥ 1.6.1170。**G1 已 PASSED（[v4 记录](#g1b-v4-运行记录与-g1-判定2026-10-24-会话)）；G2 v1–v6 六轮 NOT PASSED（判据②，缺口经[续审计七](behavior-graph-construction-audit.md#管理器每帧管线与记录全解2026-10-24-续审计七)重新归因为生成遍历缺失）；v7 已运行（[运行记录](#g2-v7-运行记录2026-10-24-会话)）；v8 五轮运行（两崩、两跳过、v8e 注入+转移成功）+ v9 gates 裁决（槽 2 假设坐实，候选只缺 world）；v10 已运行（限时借用源 world 安全但无效——绑定循环计数 0）；[续审计九](behavior-graph-construction-audit.md#绑定循环的骨骼映射语义与-vtable-澄清2026-10-24-续审计九)归因逐骨绑定、[续审计十](behavior-graph-construction-audit.md#骨骼提供者真身与填充链2026-10-24-续审计十)证认提供者为 hkaRagdollInstance 并定位填充链（63870 → 63952 → SetRagdoll）；**v11（[契约](#2026-10-24-g2-v11-契约ragdoll-实例借用判别经续审计十设计)：修正 phase=driver 读链 + 驱动窗口内同时借用 ragdoll 实例）已实现并运行（构建 `…a32766a080+dirty-cl94faaed0c6-20261024T160418Z`，DLL SHA-256 `E57C19A8…`、PDB `0C5509E7…`，2026-10-25 部署于 MO2，旧构建备份 `dist/deploy-backup-20261025-audit9/`）——判据②落定、审计十"+0x88 恒 null"推论被 driver 行证伪（候选本有 18 体实例），[运行记录](#g2-v11-运行记录2026-10-25-会话判据2落定与审计十推论证伪)；v12（槽 2 直调 + 槽 3 移除，构建 `…20261024T165857Z`）已运行——槽 2 首次执行、onFraction 写到 1.0、移除干净，输出仍恒定（[v12 记录](#g2-v12-运行记录2026-10-25-会话判据2落定与写入者运行期证实)）；v13（槽 0xC 直调填 +0x110）已运行——+0x110 早已填充、246=0 分岔落定（[v13 记录](#g2-v13-运行记录2026-10-25-会话判据2落定与采样分岔取证)）；v14（纯取证 post-pipeline1 行，构建 `…20261024T172336Z`，DLL SHA-256 `33CF02FF…`）已运行并判据②落定——63587 从未到达候选（[v14 记录](#g2-v14-运行记录2026-10-25-会话判据2落定与-63587-到达性取证)）；**v15（[契约](#2026-10-25-g2-v15-契约63587-直调替换裸更新经续审计十三--v14-判据2设计)：63587 直调替换裸 58329 + post-update1 行，构建 `…20261025T075136Z`，DLL SHA-256 `8DED2F93…`、PDB `605A0703…`）已运行并判据②落定——引擎配方首次全活而姿态仍冻结，缺口移入 63587 内部条件（[v15 记录](#g2-v15-运行记录2026-10-25-会话判据2落定与引擎配方首次全活)）。**

## G1a 运行记录（2026-10-24 会话）

构建 `5d4bee1d54`（部署身份见[构建记录](build.md#g1-experiment-implementation-2026-10-24)），目标机 AE 1.6.1170，部署 INI `IndependentGraph=construct`、`DetailedAnimationTrace=false`、`BehaviorGraphProbe=false`；原始日志 [CharacterPanel-g1a-construct-20261024-0000.log](diagnostics/CharacterPanel-g1a-construct-20261024-0000.log)。游戏会话 23:58–00:00+，首个暂停 `panel-open` 请求（request=1、actor 00000014）触发，每进程一次，共 6 条 `INDEPENDENT GRAPH` 行（24 行预算内），无崩溃/断言；面板开合、SCOPY 捕获/装配、DAC 恢复与关闭生命周期全程正常，无新增告警。

按契约观测：

- `phase=begin`：`factory=63555 loader=63585 third-arg=false holder=nullptr runtime=1.6.1170.0`。
- `phase=constructed`：`graph=0x1f2dd736200` 非空；`holder/root/behavior/project-db/setup/project/bindings/behavior-data` 全 0x0、`bones=0/0`——与二进制审计一致（构造不装载项目、不设 rootNode）；唯一的新分配子对象 `event-queue=0x1ed93261e30`，对应审计"构造写两个事件源"。
- `compare-shell graph=different holder=different root=different character=different`：候选与源无同址。
- `phase=destroyed source-drift=none`：仅以引用计数释放完成，无崩溃/断言。
- 源基线：构造前与销毁后各 11 行分组**逐字全等**（已对原始日志独立 diff，不只依赖插件自报的 `source-stable=true`），40+ 地址/计数/标志无一变化；与 2026-10-23 r4 基线同构（pose-count=116、animation-bones=116、world/controller/raycast/bindings/unscaled-skeleton/declared-root-clone/state-machine-templates 为空、ids/jobs/partitions 0/0），源坐标跨会话稳定。

判定（预注册判据，未放宽）：**G1a 通过（可观测判据）**——运行期验证工厂与配对销毁成立、无源副作用；`registry-probe=unavailable`，全局注册表计数回落保持 UNVERIFIED（契约预声明），不作为失败也不计入证据。G1 整体仍未通过：下一步是 G1b——部署 INI 改 `IndependentGraph=load` 重启，检验 63585 装载与预注册装配判据（`behaviorGraph`/`projectDBData`/`projectName`/`behaviorGraph->data` 非空、`characterInstance` 与源不同址、可写状态独立、共享资源同址、源基线不变、逆序销毁）。

## G1b 运行记录（2026-10-24 会话）

同一构建 `5d4bee1d54`，部署 INI 改为 `IndependentGraph=load` 后重启，原始日志 [CharacterPanel-g1b-load-20261024-0011.log](diagnostics/CharacterPanel-g1b-load-20261024-0011.log)。首个暂停 `panel-open` 请求（request=1、actor 00000014）触发，共 8 条 `INDEPENDENT GRAPH` 行（24 行预算内），无崩溃/断言，面板与 DAC 全程正常。

按契约观测：

- 空壳构造阶段与 G1a 完全一致：全 0 字段、`event-queue` 唯一新分配、`compare-shell` 全 different。
- `phase=load outcome=rejected ms=0.166`：以源图 `projectName` 副本（`BSFixedString*`，第三实参 false，见[实现](../src/animation/behavior_graph/independent_graph.cpp)）调用加载 63585，返回 false；候选 `behavior/project-db/behavior-data/active-nodes/variable-values` 全部仍为 0x0，候选 `projectName` 未被写入。0.166 ms 的量级只够一次立即拒绝（查表/解析失败即走错误路径），不像任何真实项目装载。
- 销毁与源基线：仅引用计数释放，无崩溃；构造前/销毁后基线逐字全等（已对原始日志独立 diff），`registry-probe=unavailable` 保持 UNVERIFIED。

判定（预注册判据，未放宽）：**G1b 失败**——命中停止条件"返回空/装配字段缺失"：63585 在早期拒绝，未完成任何原生装配。G1 整体维持 NOT PASSED，G2–G6 保持锁定；已按"停止与回退"把部署 INI 改回 off。

失败点判定（2026-10-24 续审计二已定位根因，见[审计记录](behavior-graph-construction-audit.md#第二实参形态与拒绝路径2026-10-24-续审计二)）：契约预声明的"第二实参形态未定"被实测命中——`BSFixedString*` 形态**错误**：63585 把第二实参原样作为 `const char*` 交给 69488 做路径拆分（按最后一个 `\`/`/` 分出目录与文件名），69487 拼接、69490 规范化（`/`→`\`、解析 `.`/`..`）后对全局注册表单例 0x31b6580 查表；实参误传 BSFixedString 对象地址时，其头 8 字节被当路径内容扫描，截断出的无意义"文件名"查表必失配 → 句柄 0 → 0xbb21a9 拒绝路径（对空壳图全为空操作）返回 false——与本次实测逐项吻合，纯实参形态错误、无源图副作用。另两项审计事实修正了契约认知：**`projectName(+0x1F0)` 由 63844 成功路径写为第二实参的"文件名段"（源图 projectName 不是完整路径，不能原样回传）**；编排者的第三实参来自 `holder->Unk_06()`，随 holder 而异。G1b 已按审计修订契约并重实现（`load-args` 行记录所传内容），待下次运行判读。

## G1b v2 运行记录（2026-10-24 会话）

构建 `646a0f2064+dirty…20261024T051200Z`（v2 修订形态），部署 INI `IndependentGraph=load`，原始日志 [CharacterPanel-g1bv2-load-20261024-1330.log](diagnostics/CharacterPanel-g1bv2-load-20261024-1330.log)。首个暂停 `panel-open` 请求（request=1、actor 00000014）触发，8 条 `INDEPENDENT GRAPH` 行，无崩溃，面板/DAC 全程正常。

按契约观测：

- 空壳构造与 G1a/G1b 首轮一致；`compare-shell` 全 different。
- `phase=load-args behavior-path='' project-name='DefaultFemale' path='DefaultFemale'`——**首轮的内容观测行立即命中诊断盲区**：源项目 `stringData->behaviorPath` 为**空**，`projectName` 为**无扩展名的 `DefaultFemale`**，v2 重组出的键是裸 `DefaultFemale`。
- `phase=load outcome=rejected ms=0.289`：装配字段全空，候选 `projectName` 未被写；构造前/销毁后源基线逐字全等（独立 diff 复核），无源副作用。

判定（预注册判据，未放宽）：**G1b v2 失败**（同一停止条件）。失败点判定（[续审计三](behavior-graph-construction-audit.md#加载取值与前置链的续审计三2026-10-24g1b-v2-复盘)已定位）：① 69489 剥扩展名——`projectName` 是"文件名段去扩展名"，**注册表键含目录段与扩展名**，裸 `DefaultFemale` 必然失配；② 63844 的 `character.setup → +0x40 → +0x98` 检查**不是调用方前置条件**——紧邻其前的 63949 自行为 character 创建 setup（0x58 字节分配、59477 构造、写入 +0x50），引擎自身即以 setup=0 的空壳进入 63844，因此拒绝只能发生在查表；③ 描述符表名字来自 REFR 主 vtable 槽位 114（0x2f63e0）经类型键二分查找（70510，键 `0x3284b30` 为 BSS 运行期数据）取得的组件 +0x18 串，静态无法落到具体 TESRace 字段。**结论：取值来源需要运行期证据，G1b v3 以 `race-fields` 行 dump 三个 TESRace 候选字段并按预注册优先级顺序尝试。**

## G1b v3 运行记录（2026-10-24 会话）

构建 `646a0f2064+dirty…20261024T054649Z`（DLL SHA-256 `A3C2213F…`），部署 INI `IndependentGraph=load`，原始日志 [CharacterPanel-g1bv3-load-crash-20261024-1425.log](diagnostics/CharacterPanel-g1bv3-load-crash-20261024-1425.log) 与 外部未入库原件 `C:/Users/AmazingBuff/Documents/My Games/Skyrim Special Edition/SKSE/crash-2026-10-24-14-25-34.log`（本库不提供该原件；本轮插件日志与续审计仍见本节）（会话 14:24–14:25，面板打开时进程崩溃）。`race-fields` 行全部命中：

- `sex=1 bg-model='Actors\Character\DefaultFemale.hkx' root-name='Actors\Character\Behaviors\0_Master.hkx' project-field='DefaultFemale' string-data-behavior-path='' source-project-name='DefaultFemale'`——**引擎键的来源确证**：描述符表取 `behaviorGraphs[sex].model`（完整路径含扩展名），`projectName` 即 `behaviorGraphProjectNames[sex]`（无扩展名项目名，被 63844 经 69489 剥扩展名写回）。三个字段与 [CommonLib 声明](../extern/CommonLibSSE/include/RE/T/TESRace.h)完全吻合（AE 布局对 0x1E8/0x238/0x248 偏移的读取结果语义正确）。

按契约观测（预注册判据，未放宽）：

1. **装载成功**：`phase=load-try 1/4 source=race-bg-model path='Actors\Character\DefaultFemale.hkx'` → `phase=load try=1 outcome=accepted ms=8.729`（真实装载耗时；后续候选因首个成功未触发）。
2. **装配完整（判据 1 ✓）**：`behavior=0x25df263b2c0`、`project-db=0x25df32c6200`、`project-name=0x25453c00a18`、`behavior-data=0x25db4df7510` 全部非空；候选 `projectName` 内容 = `bg-model` 路径文件名段去扩展名（`DefaultFemale`），与判据修订一致。`active-nodes=0x0`/`variable-values=0x0`——图已装载未激活（激活属 G2）。
3. **共享资源可解释（判据 3 ✓）**：`compare-loaded project-db=same project=same bindings=same behavior-data=same`——候选与源同址（引用计数 +1），与[共享语义审计](behavior-graph-construction-audit.md#加载链共享语义细读2026-10-23-三轮)预测一致；`setup=different`（63949 为候选自建）。
4. **可写状态独立（判据 2）**：候选 graph/character/setup/behavior 与源不同址；`activeNodes`/`variableValueSet` 为空（未激活），深层写隔离属 G2+ 判据。
5. **源基线（判据 4）**：崩溃前未见漂移证据；销毁后基线未能采集（崩溃），本项 UNVERIFIED。
6. **逆序销毁（判据 5 ✗ → 崩溃）**：智能指针释放 → 删除析构 0xbbcf30 → 真析构 0xbad2c0 → `behaviorGraph` 非空 → **`hkbBehaviorGraph::Deactivate`（槽位 7）读空 `activeNodes`（`[behaviorGraph+0x98]==0` → `+0x8` 计数访问越界）**。Crash Logger：`SkyrimSE.exe+0AB1FC3` AV，栈三帧（`sub_AB1FC3 ← sub_BAD324 ← dtor_BBCF4E ← independent_graph.cpp:280`）与[续审计四](behavior-graph-construction-audit.md#已装载图析构的崩溃定位2026-10-24-续审计四)静态反汇编逐帧吻合。`registry-probe=unavailable` 保持 UNVERIFIED。

判定：**G1b v3 = 装载判据成立（决定性正面证据：63585 协议在 `holder=nullptr` 空壳上完整走通），销毁判据失败**——但失败原因不是泄漏、别名或协议错误，而是引擎 `Deactivate` 不支持未激活图：**"装载后未激活直接销毁"不在引擎生命周期内**，G1 契约的销毁一环（不激活而销毁）与引擎设计冲突。G1 维持 NOT PASSED；按"停止与回退"已把部署 INI 改回 off（当前部署 DLL 为 v3 构建，`off` 下实验不触发，面板其余功能不受影响）。**方案评审结论（2026-10-24）：采纳[方案 A 修订](#2026-10-24-方案-a-修订销毁判据经-g1b-v3-崩溃后评审)，销毁判据重定义、v4 保留流程已实现并部署，待运行按修订判据完成 G1 判定。**

<a id="g1b-v4-运行记录2026-10-24-会话"></a>
## G1b v4 运行记录与 G1 判定（2026-10-24 会话）

构建 `646a0f2064+dirty…20261024T065321Z`（DLL SHA-256 `37C255DA…`），部署 INI `IndependentGraph=load`，原始日志 [CharacterPanel-g1bv4-retain-20261024-1457.log](diagnostics/CharacterPanel-g1bv4-retain-20261024-1457.log)（会话 14:56–14:57+，首个暂停 `panel-open` 请求触发）。

按 v4 预注册判据（[方案 A 修订](#2026-10-24-方案-a-修订销毁判据经-g1b-v3-崩溃后评审)，未放宽）：

1. **装载成功复现 ✓**：`load-try 1/4 source=race-bg-model path='Actors\Character\DefaultFemale.hkx'` → `outcome=accepted ms=8.797`；`behavior=0x16ff35ca500 project-db=0x16ff2fe7400 project-name=0x1662f6f0a18 behavior-data=0x16fb4c77510` 全非空；`race-fields` 行与 v3 逐字一致（`sex=1`、三字段内容相同）。
2. **装载后源基线全等 ✓**：`phase=retained source-drift=none`；已对原始日志独立 diff——构造前（基线 11 行分组）与装载后（候选存活期间采集）**逐字全等**，40+ 地址/计数/标志无一变化。
3. **保留无崩溃 ✓**：`phase=end constructed=true loaded=true assembly=true separated=true source-stable=true retained=true`；候选经永不释放的持有指针存活至进程结束；此后同一会话面板又完整开合多轮（request=3、revision=6、DAC 恢复正常），日志零错误。`registry-probe=unavailable` 保持 UNVERIFIED。

**判定：G1 PASSED（2026-10-24，方案 A 修订判据）**——构造（空壳 + 配对销毁）、装载（两次 `accepted`，装配完整、可写状态对象与源不同址、共享资源同址引用计数共享、setup 由 63949 自建）、装载后源基线全等、保留无崩溃。遗留与移交：①全局注册表计数 UNVERIFIED（预声明）；②可写状态的运行期写隔离（`variableValueSet` 等）属 G2+ 判据；③装载图的销毁证据由 G2 生命周期（激活 → 推进 → 停用 → 销毁）取得；④候选图现被保留至进程结束——独立图功能化时该保留机制将被会话生命周期管理替代。G2 解锁，其前提工作（Activate/Update 入口审计）未开始；部署 INI 已收回 `off`。

## G2 drive 运行记录（2026-10-24 会话）

构建 `646a0f2064+dirty…20261024T072214Z`（DLL SHA-256 `A3DEDE5F…`），部署 INI `IndependentGraph=drive`，原始日志 [CharacterPanel-g2-drive-20261024-1531.log](diagnostics/CharacterPanel-g2-drive-20261024-1531.log)（会话 15:25–15:31，首个暂停 `panel-open` 请求触发）。装载照旧成功（`accepted ms=9.359`，首候选），随后按 G2 契约执行。

按预注册判据（未放宽）：

1. **激活（判据① ✓）**：`phase=pre-activate` 全零（`active-nodes=0x0/0 maps=0x0,0x0 variable-values=0x0 flag=0`，`root-generator=0x27134622c70` 非空——装载期即建立）；63586 一次调用后 `phase=activated activated=true`：**`active-nodes=0x271733bfde0/97`（97 个活动节点）、maps 双非空、`variable-values=0x271731cfa00`、`flag=1`**——Activate 自分配与子图激活遍历完全生效。
2. **前进证据（判据② ✗ 未取得）**：120 步驱动完成后 `phase=post-drive` 的结构快照与驱动前**逐字相同**（active-nodes/maps/variable-values/generators 全一致），且候选 `pose-local=0x0/0` 三次快照不变——三级姿态哈希全部 `unavailable`。**没有任何可观测的推进**；按预注册规则不得默认通过。
3. **源不变（判据③ ✓）**：激活前（装载后）与驱动后源基线逐字全等（独立 diff 复核）；`activated`/`post-drive`/`retained` 三处 `source-drift=none`；**源姿态哈希前后全等**（`fdecd6ed563254e9` ×2）——暂停括号内源确实未被触碰，对照控制成立。
4. **无崩溃（判据④ ✓）**：120 步 `Update` 全部返回，同会话面板后续多轮开合正常（revision=4、DAC 恢复），日志零错误。

判定：**G2 NOT PASSED（判据②证据不足）**——激活协议完全成立，但"驱动"未产生可观测前进：候选没有姿态输出（`poseLocal` 从未分配），120 步内结构层零变化（内部时间/变量可能推进但未观测，也可能空转）。已按"停止与回退"把部署 INI 改回 off。

**归因方向（下一轮审计，两项）**：①**pose 写入链**——`character.poseLocal` 的分配/写入点在哪（源为 116 个变换；候选 `setup` 为 63949 新建的壳，可能缺 `hkaSkeleton`/蒙皮变换导致输出阶段跳过 pose 分配；generatorOutputs[0/1] 在装载期即非空，不是驱动产物）；②**Update 短路径细读**（58329）——确认 120 步是否真的推进内部状态（若推进，改进观测：`variableValueSet` 内容哈希、活动节点时间；若空转，定位缺失前置）。G1 的遗留判据"可写状态写隔离"部分成立：激活/驱动期间源结构零漂移。

<a id="g2v2-drive-运行记录2026-10-24-会话"></a>
## G2 v2 drive 运行记录（2026-10-24 会话）

构建 `646a0f2064+dirty…20261024T075048Z`（DLL SHA-256 `99E0B38E…`），INI `IndependentGraph=drive`，原始日志 [CharacterPanel-g2v2-drive-20261024-1557.log](diagnostics/CharacterPanel-g2v2-drive-20261024-1557.log)（会话 15:56–15:57+）。装载照旧成功后按 v2 流程执行：

- **判据① ✓（升级）**：字符级激活 58453 一次调用——`phase=activated activated=true`：`active-nodes=0x25af2f96b10/97`、`flag=1`，且 **`pose-local=0x25af25a6010/116`（AllocatePose 生效）**、`setup-skeleton=0x25ab6f66fd0` 非空——**装载链确实填充了 `setup.animationSkeleton`**（63949 一线成立），pose 缓冲与源同构（116 变换）。
- **判据③ ✓**：`source-drift=none` ×3、两份源基线逐字全等、**源姿态哈希前后全等**（`9c39265b…`×2）。
- **判据④ ✓**：120 步无崩溃，同会话面板多轮正常（revision=10），零错误。
- **判据② ✗ 仍未取得**：候选三级姿态哈希**全等**（`bc160a8c…`×3）——pose 缓冲内容停留在 AllocatePose 填充的骨架绑姿态上，120 步零变化；`children-flag=0`（+0x12E 门未置位）。

**Update 尾部补审**：0xab1ecc..0xab1f27 只有暂存缓冲簿记（57041）与返回——**58329 全函数确认无任何 `poseLocal` 写入**；"生成 → 采样进 pose 缓冲"的采样步骤在引擎其他环节（63587 后段/管理器流），未定位。

**判定：G2 仍 NOT PASSED（判据②）**；INI 已回退 off。

**判别器 v1 结果（gen-hash 行读数失败）**：`numBytes=-898388560` 为垃圾——`generatorOutputs` 指向对象的头部**不是** CommonLib 的 TrackMasterHeader（activeragdoll 移植未按 AE 校验），哈希窗口假设作废。同轮完成 Update 的 context 读取面枚举：**58329 全函数只读 `ctx.eventQueue(+0x18)` 与图字段（activeNodes/+0x98 ×6、+0x12E 门 ×2）**——我们自建的 context 已完备；虚表槽 6（CommonLib `Unk_06`）经 58377 证实为"向活动节点派发事件"类函数，58429 只是单条 float 写入，均非输出请求。**判别器 v2 运行结果（决定性）**：`phase=probe` 三行——**activeNodes 条目数组哈希每步变化**（`33a48b2a…`→`7177d7b3…`→`34ca7983…`）：**Update 确实在逐步推进 97 个节点的运行状态，生成链活着**；变量集 word 数组变一次后冻结（652 词——idle 循环未必每帧写变量，不作数）；gen0/gen1 转储揭示它们是 `{ptr, size=1, cap}` 的一元素数组头（非 TrackMasterHeader）。姿态哈希仍三级全等。**采样器定位（[续审计六补遗](behavior-graph-construction-audit.md#姿态写入链与字符级激活2026-10-24-续审计六)）**：**63581（0xbafda0）= 采样器**——读 `generatorOutputs[0]` 一元素数组的元素、按元素内 16 位偏移取 pose 轨道、以 57148 按 `count×0x30` 拷入 GetOrAllocatePose 返回的 poseLocal（与 AllocatePose 同一填充助手）；其入口 **63589（0xbb2a40）** 的四道门（setup/animationSkeleton/behaviorGraph 非空、+0x12C 已激活）候选全部满足，但 63581 开头 `test r15w` ——**当前动画索引 `[graph+0x240]`（word）为 0 即跳过采样**；该索引由 63587 每帧从管理器更新记录写入，候选无管理器故恒 0。**G2 v4 运行结果（判别器命中索引语义）**：`src-anim=0074`——**0x0074 = 116 = 骨骼变换数**：`[graph+0x240]` 不是动画索引而是**采样变换数**（63587 写它时以 `setup.animationSkeleton` 的骨数钳制，完全吻合）；镜像正确生效（`cand-anim=0074`），63589 每步调用无崩溃，源全程不变。**但 gen 元素内容哈希 120 步恒定**（`68fb5223…`×3）——**生成阶段没有向输出元素写任何东西**，姿态仍停在骨架绑姿态。**判定：G2 仍 NOT PASSED（判据②）；INI 已回退 off**（原始日志 [CharacterPanel-g2v4-drive-20261024-1635.log](diagnostics/CharacterPanel-g2v4-drive-20261024-1635.log)）。**管理器每帧管线审计（续审计六补遗二）——顶石找到**：通知中心单例 0x2022570（vtable 0x1986740；63761 = 把图**订阅**进中心列表，上限 30，0x54df30 为元素释放回调）的**槽 5（0xbacc90）= 每帧图处理**：栈上构临时图（63555）→ **63602(临时图, 订阅图)** → 析构（63556）。**63602 尾部即每帧图管线**：65149(&characterInstance, out) 建帧记录 → **`behaviorGraph->+0x12E = 1`（每帧请求子图重激活！）** → 63587(graph, record) → 清 +0x243。**首轮~v4 的生成无产物根因 = 我们的 Update 前从未置 +0x12E，Update 内的 60079 子图激活遍历每帧被门跳过**（该门由 63132/63602 置位、60079 消费清零）。**G2 v5 运行结果**：`+0x12E=1` 生效（门通过），节点状态仍每步搅动，**但 gen 元素与姿态仍恒定**——子图激活请求不是最后一块。**G2 v6 运行结果：崩溃**（打开背包即崩，[Crash Logger 报告](diagnostics/CharacterPanel-g2v6-crash-report-20261024-1711.log)）。归因逐帧闭合：`63587` 的 post-Update 阶段 **63583** 在 BSFixedString::Set（69162）写 `[r9=0x20]` 崩溃——63583 把动画名注册进一张由 `projectDBData->+0xD0` 派生、按动画索引索引的全局表，**该阶段依赖管理器侧的动画选择上下文，候选不具备**（63602 的记录里相应字段在引擎路径下由管理器状态填充）。**结论：63587 包装器路径拖进了无法在无管理器候选上复刻的名字注册阶段—— 包装器路线到此为止，驱动回退到 v5 形式（裸 58329 + 变换数镜像 + 63589，无崩溃）**。构建 `…20261024T091522Z`（DLL SHA-256 `3733ACFE…`）已部署，INI 已收 off。**战略结论**：v1–v6 六轮迭代把驱动链的每一段都指令级测绘完毕（激活/分配/Update/采样/入口/记录/包装器），剩余缺口收敛为单点：**生成阶段的动画选择机制由管理器驱动（记录携带动画身份、63583 注册名字），无管理器候选无法复刻**——这属于功能设计范围（面板预览本来就要自己选择动画），不再属于"可行性"问题。G2 的"推进"判据需要按此重新定义或与功能设计合并推进。

## G2 v7 运行记录（2026-10-24 会话）

构建 `440e7cf784+dirty…20261024T130326Z`（DLL SHA-256 `12663FFE…`），INI `IndependentGraph=drive`，原始日志 [CharacterPanel-g2v7-pipeline-20261024-2108.log](diagnostics/CharacterPanel-g2v7-pipeline-20261024-2108.log)（会话 21:07–21:09，首个暂停 `panel-open` 请求触发）。装载照旧成功（`accepted ms=8.422`，首候选），字符级激活照旧（97 节点、`pose-local=…/116`、setup 三字段非空），随后按 [G2 v7 契约](#2026-10-24-g2-v7-契约引擎管线驱动经续审计七设计)每步执行 `63555 栈壳 → 63602(&壳, 候选) → 63556` 引擎管线 + 实 dt 58329 + `+0x240` 镜像 + 63589 采样，共 120 步。

按预注册判据（未放宽）：

1. **前进证据（判据① ✗）**：候选三级姿态哈希仍全等（`bc160a8c…`×3，骨架绑姿态），**且 gen0/gen1 元素哈希 120 步恒定**（`6cc73397…`×3）——生成遍历仍未向输出元素写入任何内容。`cand-anim=0000→0074`、`active` 节点哈希每步变化（`3f40…`→`e7a7…`→`f6bb…`）、vars 变一次后冻结（652 词）均与既往轮次一致。按预注册归因表走"gen 元素恒定"分支。
2. **源不变（判据② ✓）**：源姿态哈希前后全等（`63a591dc…`×2）、`source-drift=none` ×3。
3. **无崩溃（判据③ ✓）**：120 步 + 120 对栈壳构造/析构全部返回，日志零错误/断言，无新 crash 报告；同会话面板后续多轮开合正常（request=2、revision=4、DAC restore=matched）。

判定：**G2 v7 NOT PASSED（判据②，gen 元素恒定分支）**；INI 已回退 off。**新增正面事实**：引擎管线驱动配方（63602 ×120 + 栈壳对）被证实可在候选上安全调用——v6 时代的"包装器必崩"只属于手搓记录直调 63587 的路线。**被否假设**："默认态含 idle 生成器、生成即出姿态"不成立——DefaultFemale/0_Master 图在无事件注入时默认态不产出位姿。**战略收束**：位姿生成的最后缺口 = **事件/初始化序列**（哪个事件把状态机带进 idle）——与 G3（Equip→Idle→Unequip 的事件机制）合流，候选事件队列布局（24 B 表项、游标 `+0xC/+0x14`，[续审计七](behavior-graph-construction-audit.md#管理器每帧管线与记录全解2026-10-24-续审计七)）与源图事件流（WORLD/注解观测）是 G3 设计的现成输入。判别遗留（预注册到 G3）：本轮观测无法区分"65237 遍历了但无激活生成器"与"63602 尾部解析未达"——注入已知事件后 gen 元素若变化则生成链活、若不变则先修管线到达性。

## G2 v8 运行记录（2026-10-24 会话，崩溃与修复）

构建 `440e7cf784+dirty…20261024T133638Z`，INI `IndependentGraph=inject`，原始日志 [CharacterPanel-g2v8-inject-crash-20261024-2145.log](diagnostics/CharacterPanel-g2v8-inject-crash-20261024-2145.log) 与 [Crash Logger 报告](diagnostics/CharacterPanel-g2v8-crash-report-20261024-2145.log)（会话 21:45:01，开包即崩）。装载/激活照旧成功（`accepted ms=9.653`、97 节点、`pose-local=…/116`），日志止于 `phase=probe step=activate`——**崩溃发生在首个 event-lookup，注入（58377）从未执行**。

归因（逐帧闭合）：崩溃指令 = ucrtbase `strcmp` 内 `movzx eax,[rcx]`，`rcx=0x4273656C6262694E`（ASCII `"NibbleB"`——字符串字节被当指针）；栈帧 [1] = 插件 `lookup_event_id`（参数 `map`/`name`）、帧 [2] = `run_independent_graph_experiment` 调用点。**根因 = 我方只读遍历的停机条件错误**：桶链尾指向**哨兵**（存于 `map+0x18`，引擎 63639/63773 两处遍历均以 `next == [map+0x18]` 停机，[续审计八](behavior-graph-construction-audit.md#事件派发链与槽-2-输出门2026-10-24-续审计八)已记录该字段），而实现误用 `entry != 0`——空桶即指向哨兵，首个 strcmp 就把哨兵内容当 `char*` 解引用。**非引擎 ABI 问题、非注入风险**：58377 调用面未被触及，v8 契约判据不受影响。

修复一（哨兵停机）后重跑**再次崩溃于同一 strcmp**（[v8b 日志](diagnostics/CharacterPanel-g2v8b-inject-crash-20261024-2154.log) 与 [Crash Logger 报告](diagnostics/CharacterPanel-g2v8b-crash-report-20261024-2154.log)，`rcx=0x6F47646542434C4D` = ASCII `"MLCBedoG"`，同为名字字节）：哨兵停机没错，**但 `db+0xA0` 那张表用 unk70（63639 验证）推导的桶布局根本走不通**——越界后踩进的是名字指针数组。结论：+0xA0 表的内部布局/填充状态与 unk70 不同源假设不成立，盲走结构路线作废。

修复二（路线更换，构建 `…20261024T135805Z`，DLL SHA-256 `5764BCB5…`）：①新增 `phase=dbdump` 行——把 `db+0x60..0xE8` 的 17 个 qword 原始 dump；②名字→id 解析改用 unkD0 名字数组的线性扫描（63583 注册路径布局）。预注册名字表与注入（58377）、判据、归因表沿用原文。

**v8c 运行（同构建，2026-10-24 22:04，[日志](diagnostics/CharacterPanel-g2v8c-inject-skip-20261024-2204.log)）**：无崩溃、源不变、120 步照常——但四个名字全部 `id=-1` → `skip=no-known-event`（预注册干净分支，注入未执行）。**dbdump 解码推翻"布局证伪"结论**：两张哈希表的 CLib 布局完全正确（哨兵 `db+0x88/db+0xB8` 同为模块静态地址、计数 1024/16384 均 2 的幂、桶为堆指针）；**两轮崩溃的真正根因 = 桶链含墓碑/释放态表项，键字段为垃圾字节，而引擎查表只做指针比较、从不解引用键**——按内容 strcmp 的遍历在任何一张表上都不安全。unkD0（9379 项，扫描全程安全）是动画/剪辑名表，不含那四个事件名。

**修复三（v8d，构建 `…20261024T141115Z`，DLL SHA-256 `D86BAA87…`）**：事件→id 的正源改为**行为图自带字符串数据**——`hkbBehaviorGraphData(+0x78 stringData)->eventNames(+0x10)` 是 Havok 原生 `hkArray<hkStringPtr>` 纯指针数组（低位 managed 标志掩 `&~1`），**事件 id 即数组下标**，正是 `eventInfos` 与状态机事件转移映射键控的 id 空间（58377 查的就是它）。新增 `phase=events count=N first='…'` 行（前 16 个真实事件名作证据）；预注册名字表、注入、判据、归因表沿用原文。

**v8d 运行（2026-10-24 22:18，[日志](diagnostics/CharacterPanel-g2v8d-inject-skip-20261024-2218.log)）**：无崩溃、源不变；**事件表首次见光**——`count=8223`，前 16 名：`FootLeft`/`FootRight`/`WeapEquip`/`na_MRh_BeginCast_Event`/`MRh_Cast_State_to_MRh_Readied_State`/`MRh_Standing_Cast_State_to_MRh_Moving_Cast_State`/`MRh_SpellReady_Event`/`MRh_SpellRelease_Event`/`MRh_Equipped_Event`/`MRh_SpellFire_Event`/`MLh_SpellReady_Event`/`MLh_SpellRelease_Event`/`SprintStart`/`SprintStop`/`na_MLH_BeginCast_Event`/`1HM_Unequip`。**判定**：四个预注册名字（`MT_Idle`/`MoveStart`/`WeaponDraw`/`SyncForward`）确非图事件——它们是 Skyrim 层动画事件/剪辑名；图内事件是另一套命名（locomotion 由变量驱动，装备链入口为 `WeapEquip`）。四名字 miss → 预注册干净跳过（58377 仍未执行）。

**v8e 修订（构建 `…20261024T141951Z`，DLL SHA-256 `17364747…`）**：预注册名字表按 v8d 证据重定——`WeapEquip`（装备链入口，正对 G3 首批"单手武器 Equip→Idle→Unequip"）→ `SprintStart`（运动态入口）→ `FootLeft` → `FootRight`（同步点）。注入、判据、归因表沿用原文。

**v8e 运行（2026-10-24 22:28，[日志](diagnostics/CharacterPanel-g2v8e-inject-fired-20261024-2228.log)）——里程碑**：四名字全部命中（`FootLeft=0`、`FootRight=1`、`WeapEquip=2`、`SprintStart=12`），**58377 注入首次执行且无崩溃**，`phase=inject event='WeapEquip' id=2 result=dispatched`。**状态机真实转移**：活动节点 **97 → 92**、变量集哈希变化（`893b33fd…` → `bb1ebb71…`）、转移后图进入稳态（60/120 步 active/vars 哈希相同）——候选图对事件完全响应，引擎侧事件→转移链是活的。但 gen 元素与姿态哈希仍恒定（`2c2893e8…`×3、`bc160a8c…`×3）→ 按预注册判据落**分支②（槽 2 输出绑定门）**；源不变（`02483049…`×2、source-stable=true）、无崩溃 ✓。

**v9 修订（构建 `…20261024T143116Z`，DLL SHA-256 `898C46F0…`）**：分支②存在反证风险——活体 Actor 的 ragdollDriver 很可能同为空（死亡才启用），若源也空却仍出姿态则槽 2 门假设证伪。新增只读 `phase=gates` 行：源/候选的 `physicsWorld(+0x238)`、`ragdollDriver(+0x30)` 与双方 gen0 元素头部 16 字节 + `+0x32/+0x38` 字段对比。**预注册裁决规则**：源 ragdoll-driver=0 → 槽 2 门假设证伪，下一归因 = 生成遍历到达性/63589 第二实参语义复审；源 ragdoll-driver≠0 → v10 设计受控借用（world+driver）评审。注入与判据不变。

**v9 运行（2026-10-24 22:35，[日志](diagnostics/CharacterPanel-g2v9-gates-20261024-2235.log)）——槽 2 假设坐实**：gates 行一次裁决——`src-world≠0`、`src-ragdoll≠0`（活体源双门全开；"死亡才启用 ragdoll"假设错误），`cand-world=0x0`、**`cand-ragdoll≠0`（装载链已为候选自建驱动）——候选只缺 physicsWorld 一门**。**元素绑定铁证**：双方 gen0 元素头 16 字节逐字节相同，但 `src +32=0x0074（=116 变换数）+38=1.000` vs `cand +32=0x0000 +38=0.000`——源元素已绑定、候选未绑定，正是槽 2 门内写的字段。注入/转移照旧复现（97→92），gen 元素与姿态仍恒定，源不变、无崩溃。

**v10 契约（借用源 world，构建 `…20261024T144235Z`，DLL SHA-256 `789C5026…`）**：借用前审计闭合——槽 2 对共享 world 的全部接触 = `bhkWorld vtable 槽 40`（77853，`mov rax,[rcx+0x10]; ret` 纯读 getter）与守卫断言桩 77928（release 直返），其余 body 在候选自己的驱动/控制器对象上（58286 锁内）。实现：drive 循环前 `candidate->physicsWorld = source->physicsWorld`（裸指针借用、不动引用计数），循环后立即恢复 nullptr，`phase=borrow` 行记录（预算 +1）。**判据①修订（预注册）**：前进证据以**候选姿态三级哈希不全等**为准；gen 元素哈希变化单独记录为**绑定门打开的证据**（槽 2 重置会写 +0x32/+0x38，元素哈希必变），**不单独构成通过**。归因表追加：绑定字段被写但姿态仍冻结 → 生成遍历仍未产出（下一归因 = 65237 到达性插桩）；绑定字段未写 → 借用未生效（回 63602 管线审计）。②③④沿用。

**v10 运行（2026-10-24 22:45，[日志](diagnostics/CharacterPanel-g2v10-borrow-20261024-2245.log)）——借用安全但无效，归因精确化**：借用执行/恢复干净（`phase=borrow world=…`、`world-restored=true`）、无崩溃、源不变、注入转移照旧复现（97→92）——但**元素绑定字段仍未被写**（gen0elem 哈希 `d82e8a27…`×3 恒定；槽 2 若运行必写 `+0x38=1.0` 必变哈希）→ 按预注册归因落"借用未生效"分支。**静态追因**：63602 内对图槽 2 的调用位于一个循环体内，循环计数由 ARG2 的 vtable 槽 0x58 以出参填充——候选侧计数为 0，循环零次执行。**结论（修正 v6 战略结论的精确回归）**：绑定循环遍历的是"当前动画/通道"类列表，由管理器侧每帧维护；无管理器候选该列表恒空，world 借用无法解锁。遗留审计疑点：主 vtable（0x19881f8，槽 0=删除析构 0xbbcf30 已确证）的槽 0E/0x11 指向函数内部地址，槽位标注存在歧义；ARG2 槽 0x58 提供者的确切语义与列表填充方未定位。INI 已回退 off。

<a id="g2-v11-运行记录2026-10-25-会话判据2落定与审计十推论证伪"></a>
## G2 v11 运行记录（2026-10-25 会话，判据②落定与审计十推论证伪）

构建 `…a32766a080+dirty…20261024T160418Z`（DLL SHA-256 `E57C19A8…`），部署 INI `inject`，2026-10-25 00:14–00:15 会话，request=1（actor 00000014），共 28 行（30 行预算内），无崩溃/断言，面板生命周期正常，[原始日志](diagnostics/CharacterPanel-g2v11-borrow-20261025-0015.log)。

**driver 行（修正读链首次取证）——审计十静态推论被运行期证伪**：`src[char=0x1f9aaf58ec0 ragdoll=0x1f513a4fc80 bodies=… count=18]`、**`cand[char=0x1fa6358a2c0（= 候选图基址+0xC0 的内嵌 characterInstance，回指精确）ragdoll=0x1f5140a8120 bodies=… count=18]——候选本就持有 18 体 ragdoll 实例**（+0x88 非 null），装载子链某处（63585 下游，待审计十一定位）已为候选填充；"装载链止步 63949 → +0x88 恒 null → 计数 0"不成立，v10"绑定循环零次执行"归因随之失效——**循环计数从来是 18**。另源 rigidBodies=18 而非 116：0x74=numPoseLocal 动画骨数，[续审计九](behavior-graph-construction-audit.md#绑定循环的骨骼映射语义与-vtable-澄清2026-10-24-续审计九)"116=逐骨绑定轨道数=rigidBodies.size"的推断亦错。

**借用与驱动**：`phase=borrow world=0x1f5259940c0 ragdoll=borrowed saved=0x1f5140a8120`（以源实例覆盖候选自有实例，循环后恢复原值）、`phase=pipeline … ragdoll-restored=true`；注入转移照旧复现（active 97→92、vars 哈希 893b33…→bb1ebb…）。

**判读（预注册判据②落定）**：候选姿态三级哈希全等（`bc160a8c2b3a5a58`×3）、**候选 gen0elem 哈希 `e122290bf430e867`×3 恒定**（绑定字段 +32/+38 仍未被写；probe 的 gen0elem 已核实读候选侧）；源姿态前后全等、`source-drift=none`、`source-stable=true`；无崩溃。→ **ragdoll 实例借用不改变输出，G2/G3 维持未过**；归因按预注册转**循环内逐骨元素配对/生成遍历到达性**：计数 18 非零、双门已过而候选元素字段零写入。下一工作包（审计十一）= **定位源元素 `+32=0x74/+38=1.0` 的写入者**（对 element+0x32/+0x38 做 store 扫描），其前置条件（绑定期 vs 每帧、轨道↔刚体映射来源，嫌疑：hkbCharacter.animationBindingSet(+0x68) / hkaRagdollInstance.boneToRigidBodyMap(+0x30)）即真正缺口。INI 已回退 off。

<a id="g2-v12-运行记录2026-10-25-会话判据2落定与写入者运行期证实"></a>
## G2 v12 运行记录（2026-10-25 会话，判据②落定与写入者运行期证实）

构建 `…a32766a080+dirty…20261024T165857Z`（DLL SHA-256 `EA5B3B27…`），部署 INI `inject`，2026-10-25 01:05–01:06 会话，request=1（actor 00000014），共 29 行（30 行预算内），无崩溃/断言，面板正常，[原始日志](diagnostics/CharacterPanel-g2v12-slot2-20261025-0106.log)。

**槽 2 直调首次执行并证实写入者身份**：`phase=slot2 added=true tracks-before[+32=0x0000 +38=0.000] tracks-after[+32=0x0000 +38=1.000]`——63621 在候选上过全部门、头部把 kPose 轨道 onFraction 写到 1.0（+32 保持 0，63621 本就写 0）；驱动后 `phase=pipeline … ragdoll-removed=true tracks-end[+32=0x0000 +38=1.000]`——槽 3 对称移除成功。注入转移照旧复现、源姿态前后全等（`e824ede5…`×2）、无崩溃。

**判读（预注册判据②落定）**：候选姿态三级哈希全等（`bc160a8c2b3a5a58`×3）、gen 元素恒定——**槽 2 可执行但非最后缺口**；残余缺口 = **numData=0x74 的回填路径**。判读后随即的[续审计十二](behavior-graph-construction-audit.md#采样入口五道门与-numdata-写入者2026-10-25-续审计十二)定位：回填者 = 63568（`[tracks+0x32]=word [graph+0x240]`），深藏于每步采样入口 63589 的首道门 `[graph+0x110]` 之后——候选从未真正采样。INI 判读后按惯例回退 off，后为 v13 部署再置 inject。

<a id="g2-v13-运行记录2026-10-25-会话判据2落定与采样分岔取证"></a>
## G2 v13 运行记录（2026-10-25 会话，判据②落定与采样分岔取证）

构建 `…a32766a080+dirty…20261024T171309Z`（DLL SHA-256 `216A003E…`），部署 INI `inject`，2026-10-25 01:17–01:18 会话，request=1（actor 00000014），共 30 行（32 行预算内），无崩溃/断言，面板正常，[原始日志](diagnostics/CharacterPanel-g2v13-slot0c-20261025-0118.log)。

**slot0c 行证伪审计十二的"首门拦截"推断**：`before[110=0x192131b0ea0 sel+20=0x191d70949d0 246=0] after[…同值…]`——**候选的 `[graph+0x110]` 在直调前就非空**（装载子链已填，槽 0xC 直调为无操作），63589 的前四道门其实一直通过。真正的分岔在第五门：**候选 `[graph+0x246]=0` → 每步采样走 63581 分支，从不经过 63568（numData 写入者）**。槽 2 行照旧（onFraction 0→1.0），驱动后 `tracks-end[+32=0x0000 +38=1.000]`、候选姿态三级哈希仍全等（`bc160a8c2b3a5a58`×3）、源不变、无崩溃。

**判读（预注册判据②落定，变体）**：+0x110 早已填充而 +32 仍 0 → 转采样内部分支；[续审计十三](behavior-graph-construction-audit.md#63587-全配方与-0x246-分岔2026-10-25-续审计十三)随即定位 0x245/0x246/0x240 的每帧写入者 = **63587（引擎级更新包装器，完整每帧配方：ctx→preupdate→更新→事件排空→位姿应用→簿记与双缓冲交换）**——插件每步调的裸 58329 绕过了它全部的簿记；且 63602 尾部本就对目标图调 63587，候选是否收到未知。INI 判读后回退 off，后为 v14 部署再置 inject。

### 2026-10-25 G2 v14 契约（纯取证：管线尾部 63587 到达性，经续审计十三设计）

> **历史预注册标记（2026-10-26）**：本节保留当时配方、预期和判据，不将后验结果改写为事前预测；后续反例及当前证据边界见[当前结论](#current-status)与 [v17 后验勘误](#g2-v17-运行记录2026-10-26-会话谓词翻假达成与采样门23落定)。

- **设置**：不变（`IndependentGraph=inject`）。流程 = v13 全部，唯一新增 = 驱动循环第 0 步 `frame_pipeline` 之后**只读记录一行** `phase=post-pipeline1`：`num-anim(0x240)/w245/b246/b249` 与双缓冲两槽 `out0[+0x220]/out1[+0x228]` 的轨道字段。零行为变更。观测预算 33 行（净 +1）。
- **预注册判据（不得事后放宽）**：
  - ①该行 `w245=0x0101` 或 `b246=1`（或 `num-anim` 非零/两槽字段互换）→ **63602 尾部的 63587 确在候选上运行**——缺口在 63587 内部条件（Path A/B 选择、间接更新调用），v15 按内部条件推进；
  - ②该行 `w245=0x0000 b246=0` 且两槽无变化 → 63587 从未在候选上运行（r13 非候选或前置门失败）——v15 以 63602 尾部同款栈记录（dt、footIK=1、+0x2a..+0x2e 模式）直调 63587 替换裸 58329；
  - ③无崩溃/断言，源不变（既有行覆盖）。
- **停止与回退**：同 v13（INI 回 off）。

<a id="g2-v14-运行记录2026-10-25-会话判据2落定与-63587-到达性取证"></a>
## G2 v14 运行记录（2026-10-25 会话，判据②落定与 63587 到达性取证）

构建 `…a32766a080+dirty-cl94faaed0c6-20261024T172336Z`（DLL SHA-256 `33CF02FF…`），部署 INI `inject`，2026-10-25 13:36 会话，request=1（actor 00000014），共 31 行（33 行预算内），无崩溃/断言，源全程稳定，[原始日志](diagnostics/CharacterPanel-g2v14-pipeline1-20261025-1336.log)。

**决定性行（`phase=post-pipeline1`，驱动循环第 0 步管线后）**：`num-anim=0x0 w245=0x0 b246=0 b249=0 out0[+32=0x0000 +38=1.000] out1[+32=0x0000 +38=0.000]`。

**判读（预注册判据②落定）**：全部 63587 簿记为零（0x240/0x245/0x246/0x249 无一被写，两槽字段未互换——out0 的 `+38=1.000` 只是本轮 slot 2 直调的既有写入、out1 仍 0.000），且双缓冲两槽只显示插件自己的槽 2 写入 → **63602 尾部的 63587 从未在候选上运行**（`[r13+0x208]` 非空门失败或其它前置不满足；观测到的 246=0 归因"构造初值"分支）。

支撑事实：装载照旧一次成功（`accepted ms=7.143`，首候选）、字符级激活照旧（97 节点、`pose-local=…/116`）、注入转移照旧复现（`inject event='WeapEquip' id=2 result=dispatched`，active 97→92、vars 哈希 `893b33…`→`bb1ebb…`）、驱动后 `tracks-end[+32=0x0000 +38=1.000]`、候选姿态三级哈希仍全等（`bc160a8c2b3a5a58`×3）、源姿态前后全等（`67117174fdb8e65d`×2）、`source-drift=none`、`source-stable=true`。`cand-anim=0074`（第 60/120 步 probe 行）是**插件自己的每步 `[graph+0x240]` 镜像写**（自 v7 起存在——[v8e](diagnostics/CharacterPanel-g2v8e-inject-fired-20261024-2228.log)/[v10](diagnostics/CharacterPanel-g2v10-borrow-20261024-2245.log) 归档日志中同一 `cand-anim=0074` 可证），不是引擎写入，不构成 63587 簿记证据。INI 判读后按惯例回退 `off`，后为 v15 部署再置 `inject`。

<a id="g2-v15-运行记录2026-10-25-会话判据2落定与引擎配方首次全活"></a>
## G2 v15 运行记录（2026-10-25 会话，判据②落定与引擎配方首次全活）

构建 `…a32766a080+dirty-cl94faaed0c6-20261025T075136Z`（DLL SHA-256 `8DED2F93…`、PDB `605A0703…`），部署 INI `inject`，2026-10-25 19:04 会话，request=1（actor 00000014），共 32 行（34 行预算内），无崩溃/断言，源全程稳定（`source-stable=true`），面板生命周期正常，[原始日志](diagnostics/CharacterPanel-g2v15-wrapper-20261025-1904.log)。

**决定性两行（逐字）**：

- `phase=post-pipeline1 num-anim=0x0 w245=0x0 b246=0 b249=0 out0[+32=0x0000 +38=1.000] out1[+32=0x0000 +38=0.000]`
- `phase=post-update1 num-anim=0x0 w245=0x101 b246=1 b249=1 out0[+32=0x0000 +38=0.000] out1[+32=0x0000 +38=0.000]`

判读：**63587 首次在候选上运行**——序言簿记 `w245=0x101、b249=1` 首次出现；第 0 步走 Path A（num-anim 未被重算；slot 2 直调写入的 +38=1.000 被双缓冲交换与轨道重置消费；post-pipeline1 重现 v14 的全零 → 63602 尾部仍无产出）。

**首次全活事实（相对 v7–v14 均为新）**：

- 活动节点计数随驱动演化：97（激活）→ 125（第 60 步）→ 111（第 120 步），且 `activeNodes` 数组本体被引擎重新分配（`0x1af9385e440` → `0x1af9385e490`）——**新**；
- 变量集哈希逐检查点演化：`893b33fd…`（激活）→ `214e2111…`（60 步）→ `a4bb41b6…`（120 步）——**新**；
- `gen0elem` 哈希驱动期间首次运动：`09335c48760eef8e` → `33583b6274311017`（60 步起稳定至 120 步）——**新**；
- 驱动后 `tracks-end[+32=0x0074 +38=1.000]`——v12–v14 追的 numData=0x74 回填达成——**新**。

**判定（预注册判据②落定，未放宽）**：簿记在场而候选姿态三级哈希仍全等（`bc160a8c2b3a5a58`×3，绑姿）→ 按预注册，缺口移入 **63587 内部条件**（Path A/B 选择、间接更新调用）。锐化后的问题：kPose 轨道头已完全绑定（numData=0x74、onFraction=1.0）且每步元素在运动，还有什么阻止轨道变换数据进入 poseLocal。支撑事实：注入转移照旧复现（`inject event='WeapEquip' id=2 result=dispatched`）、源姿态前后全等（`70b2d213715f49f9`×2）、`source-drift=none`。INI 判读后按惯例回退 `off`（部署的 v15 构建保留在位）。

<a id="2026-10-25-g2-v15-契约63587-直调替换裸更新经续审计十三--v14-判据2设计"></a>
### 2026-10-25 G2 v15 契约（63587 直调替换裸更新，经续审计十三 + v14 判据②设计）

> **历史预注册标记（2026-10-26）**：本节保留当时配方、预期和判据，不将后验结果改写为事前预测；后续反例及当前证据边界见[当前结论](#current-status)与 [v17 后验勘误](#g2-v17-运行记录2026-10-26-会话谓词翻假达成与采样门23落定)。

- **设置**：不变（`IndependentGraph=inject`）。流程 = v14 全部，唯一变更 = 驱动循环每步把裸 58329 更新替换为**一次直调 `63587(candidate, &record, 0)`**：record 为零初始化的 ≥0x30 字节栈结构（第三实参 0），`float dt@+0x00 = 1/60`，字节 `+0x2a=1, +0x2b=1, +0x2c=1, +0x2d=0, +0x2e=1`（footIK=1）——[续审计十三](behavior-graph-construction-audit.md#63587-全配方与-0x246-分岔2026-10-25-续审计十三) 63602 尾部（0xbb63b8）的栈记录布局；record 每步在循环内重建（引擎每次调用即栈上宿主一份）。63602 管线遍历、`[behavior+0x12E]=1` 写、`[graph+0x240]` 镜像与采样入口 63589 照旧。
- **观测**：新增只读 `phase=post-update1` 行——第 0 步首次直调之后立即记录，字段集与 `post-pipeline1` 相同（`num-anim(0x240)/w245/b246/b249` + 双缓冲两槽 `out0[+0x220]/out1[+0x228]` 轨道字段）。观测预算 34 行（净 +1）。
- **预注册判据（不得事后放宽）**：
  - ①候选姿态三级哈希不全等（源稳定、无崩溃）→ **G2 运动证据成立**（直调配方即缺口）；
  - ②`phase=post-update1` 出现任何 63587 簿记（`w245≠0` / `b246=1` / `num-anim` 被重算 / 双缓冲两槽相对 post-pipeline1 互换）而输出仍冻结 → **缺口移入 63587 内部条件**（Path A/B 选择、间接更新调用）→ 下一工作包 = 审计十四；
  - ③`phase=post-update1` 仍 `w245=0x0000 b246=0` → **63587 自身序言门（+0x110 / [[+0x110]+0x20] / +0x208 / [[+0x208]+0x12c]）在候选上拦截** → 下一工作包 = +0x208 门链审计；
  - ④无崩溃/断言，源不变（既有行覆盖）。
- **停止与回退**：同 v14（INI 回 off）。

### 2026-10-25 G2 v16 契约（四门仪器化：63564 谓词直调 + 58423 门变量 + 双槽序列，经续审计十四收束设计）

> **历史预注册标记（2026-10-26）**：本节保留当时配方、预期和判据，不将后验结果改写为事前预测；后续反例及当前证据边界见[当前结论](#current-status)与 [v17 后验勘误](#g2-v17-运行记录2026-10-26-会话谓词翻假达成与采样门23落定)。

- **设置**：不变（`IndependentGraph=inject`）。流程 = v15 全部（每步 63602 管线 → `+0x12E` 写 → 直调 63587 → `[0x240]` 镜像 → 采样入口 63589），**只增观测、零行为变更**：三处新增全部为只读读取，预算内既有行保持原序原字段。
- **观测**（三处新增，[续审计十四](behavior-graph-construction-audit.md#63587-内部条件与位姿最后一公里2026-10-25-续审计十四)收束清单的直接仪器化；观测预算 40 行，较 v15 预算 34 行净 +6）：
  1. **`phase=pred step=N r=<hex>`（步骤 0/60）**：直调一次 **63564**（0xbae3d0，重定位 `PosePredicateID`）——63581 采样块第一条指令就是它（`audit14_branch_63581.txt` 0xbafdda，实参沿用 63581 收到的 rcx/rdx，谓词体内 rdx 可证死参数），返回值在 0xbaff4d `test r13b,r13b` 决定是否整块跳过采样。约定 = `uint8_t(*)(graph const*)`（单活实参 = 候选图）；副作用有界已静态核验：全 0x61 字节体只读 `[graph+0x238]`/`[graph+0xF0]`，唯一被调 58286 = `mov rax,[rcx+0x88]; ret` 纯 getter，无任何写入/引用簿记。调用点 = 每步采样入口 63589 之前（引擎内 63581 的调用时机）。
  2. **`phase=g3-gates`（驱动循环前一次，只读）**：58423（0xab7f30）入口三道门变量——`[graph+0x100]` 对象指针及其 `+0x4A` 门字节、ctx 链 `[ctx+0x50] → [+0x40] → [+0x88]`（0xab7f54..0xab7f89；ctx = 插件 58315 自建的既有 context，与 58377 派发共用），指针空记 `null`。
  3. **`phase=slots step=N p0=<ptr> n0/f0 h0 p1 n1/f1 h1`（步骤 0/1/2/60/120）**：双缓冲两槽 `[graph+0x220]`/`[graph+0x228]` 的逐槽序列——元素指针、首轨道头 `+0x32`（numData，word）/`+0x38`（onFraction，float）、0x100 字节元素头 FNV（复用既有 `fnv1a_64`）。采样路径每步读的是 Path-A 交换后的 `[0x220]` 槽；步骤 0 行取管线遍历前基线、1/2 行取 63587 后、60 行取采样前、120 行取驱动后，足以裁决"哪一槽在动、读槽 numData 何时为 0"。
- **预注册判据（不得事后放宽）**：
  - ① `phase=pred` 在记录步骤返回非零 → **G1 确认**（谓词门：63581 每步在 0xbaff4d 整块跳过采样）→ v17 = 审计 63564 的内部条件及其在候选上为真的原因；
  - ② `pred` 为零且 `g3-gates` 任一变量失败（`[+0x4A]≠0` 或 `+0x88` 链为空）→ **G3 确认** → v17 = 审计/补填该字段（v11 已证候选 driver+0x88 非 null；此门链是另一对象）；
  - ③ `pred` 零 + 门全过 + slots 序列显示采样时刻读槽（交换后的 `[0x220]`）`n=0x0000` 而另一槽数据在动 → **G4 确认**（63587 Path-A 复位在采样器读到之前吃掉生成遍历的 numData）→ v17 = 逐步配方重排（先采样后 63587，或 Path-B 记录）；
  - ④ `pred` 零 + 门全过 + 采样时刻读槽 numData 非零而位姿仍冻结 → **G2 维持**（63666 输出 → 63582 混合基址）→ v17 = 审计 63582 混合内部（暂缓的重循环）；
  - ⑤ 无崩溃、源稳定（既有行覆盖）。
- **停止与回退**：同 v14/v15（INI 回 off）。

<a id="g2-v16-运行记录2026-10-25-会话判据1落定与采样分支配方事实"></a>
## G2 v16 运行记录（2026-10-25 会话，判据①落定与采样分支配方事实）

> 后验范围说明（2026-10-26）：谓词仅在0/60步直接观测；其余“每步”归因是代码配方与静态路径推演。后续v17已改直调63581，当前判定见[文首](#current-status)。

构建 `…a32766a080+dirty-cl94faaed0c6-20261025T2245xx`（DLL SHA-256 `A732AC08…`，v16 契约记录的部署身份），部署 INI `inject`，2026-10-25 22:58 会话，request=1（actor 00000014），无崩溃/断言，`phase=end … source-stable=true`，[原始日志](diagnostics/CharacterPanel-g2v16-gates-20261025-2258.log)。

**决定性行（逐字）**：

- `phase=g3-gates g100=0x27073572de0 flag4a=0x00 ctx+50=null +40=null +88=null`
- `phase=pred step=0 r=0x01`
- `phase=pred step=60 r=0x01`

INDEPENDENT GRAPH 行共 40 条 = **恰在 40 行观测预算上**。

**判读（预注册判据①落定，未放宽）**：**G1 确认（谓词门）**——63564 谓词在两个检查点（步骤 0 与 60）都返回非零，结合静态分支说明该条件可使 63581 跳过采样/混合/位姿应用；没有逐步分支跟踪，不能把两检点写成每步实测；候选姿态三级哈希仍绑姿全等（`bc160a8c2b3a5a58`×3）。[续审计十五](behavior-graph-construction-audit.md#63564-谓词真值条件与-63581-采样入口约定2026-10-25-续审计十五)随即以指令级链收紧该判读：**谓词恰好读借窗口使非空的那两个字段**（`[graph+0x238]` 借来的 physicsWorld、`[graph+0xF0]` 候选自有的 ragdollDriver，往下走 driver+0x88 → `rigidBodies[0]` → 刚体的 `hkpWorldObject::world` 五级非空链，纯非空判定、无数值比较）——契约期提出的"谓词读的恰是借窗口撑活的字段"这一疑点被证实为 v17 要击败的机制。

**采样分支配方事实（新第二门，同轮运行叠加）**：63587 序言每步写 `word[graph+0x245]=0x101`（245=1、246=1）而 Path A 从不复位 0x246 → 采样时刻 `b246=1` 每步成立 → 63589 走 **63568 分支（仅头部回填 `[0x220]` 槽 numData=word[0x240]，dl=0 即返回）**——**即便谓词不拦，63581 也从未被到达**。支撑证据（逐字）：

- `phase=post-update1 num-anim=0x0 w245=0x101 b246=1 b249=1 out0[+32=0x0000 +38=0.000] out1[+32=0x0000 +38=0.000]`（与 v15 一致——Path A 恒走、0x246 不复位）；
- 步骤 120 槽序列 `phase=slots step=120 p0=0x27049d770e0 n=0x0074 f=1.000 h0=48bfefcceccb91f1 p1=0x27049d534c0 n=0x0000 f=0.000 h1=1ec64cb0955cbfc0`——63568 的回填落在读槽 `[0x220]` 且元素头哈希已变（`48bfefcc…`），对照其余各行的全零原始哈希 `1ec64cb0…`。

**g3-gates 记录（v17 设计输入，判据①下不裁决）**：插件的 58315 自建 ctx 链全空（`ctx+50=null +40=null +88=null`）——判据①成立时 58423 门从未被到达，此行不构成归因证据；但 63581 在 0xbafe48 自建 ctx（58315(figure=hkbCharacter, world)）的链内容与之无关，v17 直调 63581 时读的是后者，**g3-gates 空链不能外推为采样链死锁**，仅作为插件 ctx 形态的既有事实记录。

**槽序列事实**：双槽指针每步交替（步骤 0 `p0=0x27049d534c0/p1=0x27049d770e0` → 步骤 1 互换 → 步骤 2 复原——Path A 交换可见）；两槽在 63587 复位块作用范围内保持原始哈希 `1ec64cb0…`，仅步骤 120 的读槽（第 0 步 63568 写过的槽，偶数次交换后回到 `[0x220]`）呈写入态（`48bfefcc…`）——与"63568 仅第 0 步执行 + 每步交换复位"的静态推演完全相容。

**判定与后继**：按预注册判据①，缺口收敛为谓词真值 + 采样分支配方；[续审计十五](behavior-graph-construction-audit.md#63564-谓词真值条件与-63581-采样入口约定2026-10-25-续审计十五)已产出 v17 所需的四问答案（谓词五级非空链、翻假字段 = `rigidBodies[0]->world`、63581 两实参直调约定、管理器每帧记录 +0x2A=0 的 Path B 语义）。INI 判读后按惯例回退 `off`（部署的 v16 构建保留在位）。

### 2026-10-26 G2 v17 契约（引擎原生每帧序：Path B 记录 + 谓词翻假 + 63581 直调，经续审计十五设计）

> **历史预注册标记（2026-10-26）**：本节保留当时配方、预期和判据，不将后验结果改写为事前预测；后续反例及当前证据边界见[当前结论](#current-status)与 [v17 后验勘误](#g2-v17-运行记录2026-10-26-会话谓词翻假达成与采样门23落定)。

- **设置**：不变（`IndependentGraph=inject`）。流程 = v16 全部（每步 63602 管线 → `+0x12E` 写 → 63587 直调 → `[0x240]` 镜像 → 采样；`pred`/`g3-gates`/`slots` 仪器行原样保留），三处行为变更（[续审计十五](behavior-graph-construction-audit.md#63564-谓词真值条件与-63581-采样入口约定2026-10-25-续审计十五)收束清单的直接实现）：
  1. **Path B 记录（审计 15 Q4）**：每步 63587 记录换成管理器每帧生产者 0x54c130 的真实布局——`float dt@+0x00`、`+0x08..+0x27` 全零、`u16 +0x28 id = 0`、`byte +0x2A = 0`（Path B：不交换双缓冲、不复位轨道、`0x246` 清零、`[0x240]` 经 63575 重算）、`byte +0x2B = 1`、`byte +0x2C = 1`、`byte +0x2E = 1`——v15 沿用的 `+0x2A=1` 是 63602 尾部零 dt 管线专用配方，不是引擎每帧序；
  2. **谓词翻假（审计 15 Q1）**：槽 3（63622，RemoveRagdollFromWorld）移到槽 2 直调之后、驱动循环之前——借用窗口内撤销槽 2 的逐刚体 world 注册，采样时刻 `rigidBodies[0]->world == 0`，63564 谓词返回假（活体唯一不破坏链条的翻假出口；v12 已证槽 3 不动轨道头）。`phase=slot2` 行扩为 `added=… removed=…`（不新增行）；循环后的槽 3 调用保留为幂等复检（喂 `phase=pipeline` 的 `ragdoll-removed=` 字段）；
  3. **63581 直调（审计 15 Q2/Q3）**：每步采样入口 63589 退役（其 b246 分岔正是 v16 落定的绕路），改为直调采样分支 **63581（`SamplingBranchID`）**，实参 = `(candidate, word [graph+0x240])`——第二实参读镜像字段（每步 `numPoseLocal` 镜像照旧，镜像写之后读回），与引擎 63589 调用点 0xbb2ac3 的两活实参约定逐字一致。`sampling_entry`/`SamplingEntryID`/`mode_byte` 全部移除（零悬挂引用）。
- **观测预算**：40 行（净不变；全部既有行保持原序原字段，唯一字段级扩展 = `phase=slot2` 行增 `removed=`）。
- **预期新签名（读日志对照，不构成判据）**：`phase=post-update1` 应显示 `w245=0x0000 b246=0 num-anim=0x0074`——Path B 复位 0x245/0x246 且 63575 重算 numAnimBones（0x74）；`phase=slots` 的双槽指针不再每步交替（Path B 无交换）。
- **预注册判据（不得事后放宽）**：
  - ① `phase=pred` 在记录步骤返回 `0x00` 且候选姿态三级哈希不全等（源稳定、无崩溃）→ **G2 运动证据成立（PASSED on the motion criterion）**——下一 = OAR 接入门（G3+），先补一轮稳定复跑；
  - ② `pred` 返回 `0x00`、姿态仍冻结 → 采样块已跑而下游失败——下一 = 审计十六插桩 63581 内部（63666 输出 / 63582 混合 / 58423 门），以 `slots` 行为入口证据；
  - ③ `pred` 仍 `0x01` → 翻假失败（驱动期间刚体被重新注册，或槽 3 未清 `body[0]->world`）——下一 = 审计再注册路径；
  - ④ `post-update1` 不显示 Path B 签名（`w245=0x0000 b246=0`）→ 记录变更未按设计生效——任何后续步骤前先复查；
  - ⑤ 无崩溃、源稳定（既有行覆盖）。停止/回退：按惯例 INI 回 off。**早步门②/③失败预期化**：63581 的门②/③（`[[0x228]]` 槽的 onFraction 在管线填它之前可能仍为 0.0）在早期步骤失败是**预期瞬态**，不是失败模式——`slots` 行记录之。
- **停止与回退**：同 v16（INI 回 off）。

<a id="g2-v17-运行记录2026-10-26-会话谓词翻假达成与采样门23落定"></a>
## G2 v17 运行记录（2026-10-26 会话，谓词翻假达成与采样门②③落定）

构建 `…a32766a080+dirty-cl94faaed0c6-20261025T160248Z`（DLL SHA-256 `904D6B5B…`），2026-10-26 00:21、actor 00000014/request=1；[原始日志](diagnostics/CharacterPanel-g2v17-native-20261026-0021.log)原样保留。该轮 INI 为 inject，判读后回 off；40 行 INDEPENDENT GRAPH 达到既定预算。120×1/60 秒在一次暂停批次内完成，不是 120 个画面帧。本次文档修订未运行新实验。

**实际观察**：装载 accepted=8.289 ms；字符级激活后 poseLocal 有 116 项；WeapEquip id=2 dispatched，activeNodes 97→125→111，变量哈希变化。候选 pose 在 0/60/120 均 `bc160a8c2b3a5a58`，与 v11–v16 相同；源 pose 前后 `7ac13b9170de9d2b`，11 个源快照分组相同。无崩溃/断言，候选 retained=true；不证明全部共享写状态安全或装载图回收成立。

**初始化与重置时序（原始行）**：

- `phase=slot2 added=true removed=true tracks-before[+32=0x0000 +38=0.000] tracks-after[+32=0x0000 +38=1.000]`
- `phase=post-pipeline1 … out0[+32=0x0000 +38=1.000] out1[+32=0x0000 +38=0.000]`
- `phase=post-update1 num-anim=0x0 w245=0x101 b246=1 b249=1 out0[+32=0x0000 +38=0.000] out1[+32=0x0000 +38=0.000]`
- `phase=pred step=0 r=0x00` 与 `phase=pred step=60 r=0x00`：**仅这两个检点**实测谓词为假，不能写成 120 步逐次观测。
- slots 在记录的 0/1/2/60/120 检点双槽 n=0、f=0；`tracks-end +32=0x0000 +38=0.000`。这不是“轨道头从未初始化”：已有权重在首个 update 后清零，尚须解释其重置与重新填充。

**字段勘误（取证范围）**：

| 字段 | 真实读法 | 能说明什么 |
| --- | --- | --- |
| slots n | `uint16(element+0x32)`，kPose numData | 不是整体 numTracks；n=0 不能证明数量子门失败。 |
| slots f | `float(element+0x38)`，kPose onFraction | 记录检点为0，与原生 onFraction>0 门不满足一致；是已观察的首个阻断线索。 |
| 原生整体轨道数门 | `DWORD(element+0x4)>2` | 与 n 不同；驱动前 head `e022000015000000` 的 +4 为 0x15，仅支持当时值，后续实际值/分支待观测。 |
| h0/h1/gen0elem | 元素头 0x100 字节 FNV | 头部/指针内容证据，不是完整动态骨变换载荷。 |

v17 已按配方直调 63581 到入口/门判定；不能称 63666/63582 采样混合主体已执行。f=0 结合静态门说明应优先核对头部/时序，但没有每步分支轨迹，不能从 n=0 断言每步都由数量门退出。插件自建 ctx 的 g3-gates 不等于 63581 内部原生 context，也没有证明 58423 应用链可达。

**Path B 后验勘误**：预注册只凭 record+0x2A=0 预测“不交换/不复位”，遗漏了 `max(t0,t1)∈[0,MAXFLT)` 条件。v17 的 `dt=1/60`、record+0x18=null 与该复位配方仍呈现 Path A 签名和所记录双槽交换。审计对 {0,-MAXFLT} 复位再减正 dt 的推演解释的是**这套记录/状态/顺序**，不是“所有正 dt 原生帧均不能 Path B”；零 dt 调用方也只提供可满足条件的候选，尚无真实 t0/t1 运行序列。旧判据④正确触发了预期复核，但原生字段/状态仍需完整追踪。

**当前归因**：输出头的初始化/重置/重新填充是优先阻断假设，63771/63870 尾链等仍是嫌疑，未确定唯一写入者。后续还须验证生成变换载荷、两个槽的正确写入顺序、63666/63582 返回、63581 原生 ctx 与 58423 下游应用。旧“单点化/最终精确形态”“管线从不写onFraction”“只需播种”结论证据不足，已由本段收窄；v17 契约保留原预测以便追溯。G2 **NOT PASSED**；不能把 gate G1（谓词）的突破与实验 G1、完整 G3 或 OAR 一致性混称。

<a id="g2-v18-frame-trace"></a>
## G2 v18 预注册观测契约（2026-10-26，尚无运行结果）

目的：把[下一步问题](#next-audit)落实为有界时序取证包，保持 v17 驱动配方和验收不变。仅在 `IndependentGraph=inject` 且精确 AE 1.6.1170 时新增读取；其他模式和运行时没有新增 trace 读取/输出。未新增原生入口、REL ID、hook、种子写入或调用顺序调整。原有源对照、事件与保留机制不变；这不是新机器码写入者审计的完成，也不是 G2 修复。

实现入口是[FrameTrace](../src/animation/behavior_graph/frame_trace.h)、[读取适配](../src/animation/behavior_graph/frame_trace.cpp)及[预算/进度策略](../src/animation/behavior_graph/frame_trace_policy.h)，由[既有驱动循环](../src/animation/behavior_graph/independent_graph.cpp)同步调用。状态只含值、掩码与计数，不跨原生调用保留引擎指针；每个快照重新取两槽元素地址。原始 v17 源码序为 **scratch construct → 63602 → scratch destroy → 既有0x12E写 → 63587 → 0x240镜像 → 63581**，新观察点包围该序而不改变它。

### 行模式与预算

新前缀 `GRAPH FRAME schema=v18`，带 actor/request、零基 `step`、`stage` 与 `completed`。`completed`仅表示已经返回的63581调用数；该入口可能在门前退出，因此不代表有效采样、骨骼写入或有意义动作。只选 step **0/1/2/59/60/119**，各至多五个阶段；重复/无效阶段及非选中步不输出，共最多 **30 条阶段行 + 1 条 summary**。原有最多40条 `INDEPENDENT GRAPH` 和源快照预算独立，不计入31条；缺行、缺槽仍是缺证据。

| stage | 时点 | completed |
| --- | --- | --- |
| pre-pipeline | scratch/63602之前 | 此前已返回的63581调用数 |
| post-pipeline | 63602及scratch配对销毁之后 | 同上 |
| post-update | 63587返回之后 | 同上 |
| pre-sample | 原有0x240镜像及既有谓词观测之后，63581之前 | 同上 |
| post-sample | 63581返回且新进度观察完成之后 | 此轮已完成，正常为step+1 |

阶段行字段：`numAnim`为graph+0x240字；`w245/b246/b249`为原有簿记；`t0/t1`为+0x1E8/+0x1EC的float值与原始32位hex（足够往返精度，时间含义依原审计，不当作clip时钟）；slot0/1分别报告实际元素身份、`numTracks=DWORD(+0x4)`、`numData=word(+0x32)`、`onFraction=float(+0x38)`及其原始bits。数目是计数、fraction无量纲，不把numData当整体轨道数。缺output/element明确标missing，不补零；不解析未知dataOffset或原生ctx。

每次实际63581返回后均读候选现有poseLocal原始缓冲指纹（至多120次，无逐次日志），summary给出phase-rows/total-rows、completed、pose-available/missing、raw-hash-changes、first-changed-completed、baseline。变化数比较相邻**可用观察**，缺测间隙保留missing且不能确定变化真实发生时刻；first字段是首次观察到差异的完成步，回到baseline也会被计入。原始缓冲含padding，hash变化不是有意义骨运动，未变化也只限定这些观察。summary明确 `generator-payload=unobserved native-context=unobserved acceptance=unjudged`，没有自动通过判定。

### 预注册判读与测试步骤

1. 用独立包中的匹配DLL/PDB，核对构建身份；合并INI片段启用inject并重启，第一次暂停开包只运行一次。仓库默认仍off，不部署或覆盖旧包。
2. 对齐actor/request/step和物理槽身份，检查**同一物理槽**在五个时点的numTracks/numData/fraction/t值，区分交换与清除。特别核对v17已见的“slot2/post-pipeline1 f=1 → post-update1 f=0”；不能据最终0反推从未初始化。
3. 若数量门已满足而fraction在update后归零，优先追其复位/再填协议及原生正常源帧同阶段对照；若真实numTracks也异常，则把数量字段/元素身份单独交回审计。新数据只能定位观察边界之间的变化，不自动证明某条机器指令是唯一写入者。
4. 若summary观察到raw哈希差异，仍须查有效变换、相位、数值、必要骨及源侧对照；若仍无差异，也不能越过未观测的生成载荷、63581原生ctx、63666/63582与58423下游应用。完整原生帧与写入者审计仍待执行，不能用候选轨迹替代。
5. 保存完整日志与构建身份，确认原有source-stable/source-pose和受控恢复记录，再将INI回off并重启。G2仍需有意义原版idle、有效骨运动、源对照与后续生命周期证据；装载图仍保留，不能计为清理通过。

离线验证仅[两个策略/进度案例](../tests/behavior_graph_frame_trace_test.cpp)和Release构建/包核验，命令与实际结果见[构建记录](build.md#v18-frame-trace-build)。契约设计时没有v18游戏日志；该轮已于2026-10-26回传并判读，见下节。

<a id="g2-v18-run"></a>
## G2 v18 运行记录（2026-10-26 会话，五阶段双槽与 Path A 复位实证）

构建 `CharacterPanel-scenegraph-behavior-graph-v18-frame-trace-20261026-7ee4cce3bf-cl94faaed0c6-20261026T143615Z`（DLL SHA-256 `72c33a0e…`，与包内manifest一致），2026-10-26 23:01 首次暂停开包（actor 00000014、request=1；`Panel menu opening=true` 23:01:42.914 → `opening=false` 23:01:50.205）。[原始日志](diagnostics/CharacterPanel-g2v18-trace-20261026-2301.log)原样归档（40,366字节，SHA-256 `D42DB99D…`）。该轮部署INI为`inject`，判读后已回`off`，v18 DLL/PDB留在原位。全程无`[E]`、无崩溃；15条`[W]`均为既有DAC/studio-light警告。

**预算与行数**：`GRAPH FRAME` 30条阶段行 + 1条summary（step 0/1/2/59/60/119 × 五阶段，恰好用满31行）；`INDEPENDENT GRAPH` 40行（既定预算）；`BEHAVIOR GRAPH` 36行 = 三份12行源基线块（构造前/装载后/结束），三块逐行全等。summary：`phase-rows=30 total-rows=31 completed=120 pose-available=120 pose-missing=0 raw-hash-changes=0 first-changed-completed=none-observed baseline=bc160a8c2b3a5a58`。

**运行流程事实**（与v17同型）：装载 `accepted ms=8.726`，字符级激活后 `pose-local=…/116`、`active-nodes=…/97`；`WeapEquip` id=2 `dispatched`；`phase=borrow world=… ragdoll=own`；`phase=slot2 added=true removed=true tracks-before[+32=0x0000 +38=0.000] tracks-after[+32=0x0000 +38=1.000]`；`phase=slot0c before/after 110=0x23cf2cac900 … 246=0`；`phase=g3-gates … ctx+50=null`；`phase=pred step=0 r=0x00`、`step=60 r=0x00`；`phase=pipeline steps=120 shell=pairs world-restored=true ragdoll-removed=true tracks-end[+32=0x0000 +38=0.000]`；`phase=end constructed=true loaded=true assembly=true separated=true source-stable=true retained=true`。驱动中活动节点 97→125→111、变量哈希 `893b33fd…`→`214e2111…`→`a4bb41b6…`，`gen0elem` 三个检查点恒为 `7ced7f7208732e50`；驱动后 `phase=drive-done … source-pose-before=c58dcc07593b5e75 source-pose-after=c58dcc07593b5e75 candidate-pose-0/60/120=bc160a8c2b3a5a58`。

**五阶段观测**（两槽元素恒为 `E0=0x23cc9fa8510`（slot 2 写过的那个）与 `E1=0x23cc9faa820`；30条阶段行的 `numTracks(+0x4)=21`、`numData(+0x32)=0` 在所有阶段、两个槽上都不变，故不逐行重复；合并行里的 `0 → 116` 表示该步 update 后原生值为 0、随后插件镜像把它写成 116）：

| step | stage | numAnim(+0x240) | w245/b246 | t0/t1 | slot0 | slot1 |
| --- | --- | --- | --- | --- | --- | --- |
| 0 | pre-pipeline | 0 | 0x0/0 | -MAXFLT/-MAXFLT | E0 f=1.000 | E1 f=0.000 |
| 0 | post-pipeline | 0 | 0x0/0 | -MAXFLT/-MAXFLT | E0 f=1.000 | E1 f=0.000 |
| 0 | post-update | 0 | 0x101/1 | 0/-MAXFLT | E1 f=0.000 | E0 f=0.000 |
| 0 | pre-sample | 116 | 0x101/1 | 0/-MAXFLT | E1 f=0.000 | E0 f=0.000 |
| 0 | post-sample | 116 | 0x101/1 | 0/-MAXFLT | E1 f=0.000 | E0 f=0.000 |
| 1 | pre-pipeline | 116 | 0x101/1 | 0/-MAXFLT | E1 f=0.000 | E0 f=0.000 |
| 1 | post-pipeline | 116 | 0x101/1 | 0/-MAXFLT | E1 f=0.000 | E0 f=0.000 |
| 1 | post-update | 0 | 0x101/1 | 0/-MAXFLT | E0 f=0.000 | E1 f=0.000 |
| 1 | pre-sample | 116 | 0x101/1 | 0/-MAXFLT | E0 f=0.000 | E1 f=0.000 |
| 1 | post-sample | 116 | 0x101/1 | 0/-MAXFLT | E0 f=0.000 | E1 f=0.000 |
| 2 | pre-pipeline / post-pipeline | 116 | 0x101/1 | 0/-MAXFLT | E0 f=0.000 | E1 f=0.000 |
| 2 | post-update / pre-sample / post-sample | 0 → 116 | 0x101/1 | 0/-MAXFLT | E1 f=0.000 | E0 f=0.000 |
| 59 | pre-pipeline | 116 | 0x101/1 | 0/-MAXFLT | E1 f=0.000 | E0 f=0.000 |
| 59 | post-update / pre-sample / post-sample | 0 → 116 | 0x101/1 | 0/-MAXFLT | E0 f=0.000 | E1 f=0.000 |
| 60 | pre-pipeline / post-pipeline | 116 | 0x101/1 | 0/-MAXFLT | E0 f=0.000 | E1 f=0.000 |
| 60 | post-update / pre-sample / post-sample | 0 → 116 | 0x101/1 | 0/-MAXFLT | E1 f=0.000 | E0 f=0.000 |
| 119 | pre-pipeline / post-pipeline | 116 | 0x101/1 | 0/-MAXFLT | E1 f=0.000 | E0 f=0.000 |
| 119 | post-update / pre-sample / post-sample | 0 → 116 | 0x101/1 | 0/-MAXFLT | E0 f=0.000 | E1 f=0.000 |

**判读（预注册，不放宽）**：

1. **数量门满足、fraction 门不满足**：`numTracks(+0x4)=21` 在全部阶段成立（审计十四 Q3-2 的 `+4>2` 门过），但 63581 采样时点的双槽 `+0x38` 均为 0，`+0x38>0` 门不过。故落入预注册第3条“数量门已满足而 fraction 在 update 后归零”分支。
2. **复位定位在本步 63602 之后、63587 返回之前**：step 0 的 `pre-pipeline`/`post-pipeline` 两槽为 E0 f=1.000 / E1 f=0.000（slot 2 写入），`post-update` 变为 E1/E0 且两槽 f 均为 0。这排除“63602 管线写坏”和“从未初始化”两种读法；与[审计十四 Q1-3](behavior-graph-construction-audit.md#63587-内部条件与位姿最后一公里2026-10-25-续审计十四)的 Path A 复位块一致——交换后重置写槽的轨道头（逐轨道清 onFraction、flags，尾写 `[tracks+0x32]=0`）。具体指令写入者仍是静态候选，v18 只对齐了观察边界。
3. **每步都是 Path A**：`w245=0x101 b246=1 b249=1` 与“每个 63587 调用恰好交换一次双槽”在全部记录时点成立，`numAnim` 在每次 update 后为 0（Path A 不经 63575 重算），pre-sample 的 116 全部来自插件镜像写。v17 预注册的 Path B 签名（`w245=0 b246=0 numAnim=0x74`）未出现。
4. **t 对被钉在复位值**：step 0 前为 `(-MAXFLT,-MAXFLT)`，首次 update 后恒为 `(0,-MAXFLT)` 且 120 步内不累积。按[审计十六](behavior-graph-construction-audit.md#audit16)修正后的条件 `Path B ⇔ record+0x2A==0 且 fork 时 t_larger ≥ 0 且 t_smaller > -MAXFLT`：序言先 `-= dt`（dt=1/60），fork 时 `t_larger=-dt<0`，因此本配方每步只能 Path A；函数内唯一能推进 t 的眼位 arm 块以 `record+0x18`（`BSAnimationUpdateData::optionalEyePosition`）非空为前提，而本配方该字段恒为 null。**这不是“所有正 dt 原生帧都不能 Path B”的推广**——原生帧由 REFR 的 `ModifyAnimationUpdateData` 填眼位后 arm。
5. **采样主体未执行**：`completed=120` 只表示 120 次 63581 调用返回；候选 pose 原始缓冲在 120 次观察中 0 次变化（`baseline=bc160a8c2b3a5a58`），`numData` 全程 0（63568 的头部回填被绕过），`phase=pred` 两个检查点均为假（`r=0x00`），与“门后 63666/63582/58423 未执行”相容。生成载荷、63581 自建 ctx（非原生）、63666 输出与 58423 下游应用仍未观测。

**判定**：G2 仍 **NOT PASSED**——120 步内没有有意义的原版 idle 位姿运动，候选 pose 与 v11–v17 的同一条 `bc160a8c2b3a5a58` 相同；源侧 `source-stable=true`、三块基线全等、装载图仍保留（不计为清理通过）。

**由此收窄的下一问**：本配方让 t 停在 Path A 的复位值、`record+0x18` 恒空，于是 63575 的采样数解析、`0x246` 清零、63568 回填全部被绕过；下一步属[有界审计问题](#next-audit)（audit16），不是修复或播种。

<a id="v18-handoff"></a>
## v18 测试交接、回传与停点

**v18 已回传并判读（2026-10-26 23:01 会话），G2 仍 NOT PASSED；最新实际运行证据是 v18。** 回传内容：完整 `CharacterPanel.log`，已原样归档为 [v18 原始日志](diagnostics/CharacterPanel-g2v18-trace-20261026-2301.log)（40,366字节，SHA-256 `D42DB99D…`）；其中 `SCENEGRAPH BUILD` 身份与包内 manifest 一致，`GRAPH FRAME` 30条阶段行+1条summary齐备，`INDEPENDENT GRAPH` 40行、`BEHAVIOR GRAPH` 36行，无`[E]`、无崩溃；开包 23:01:42.914、关包 23:01:50.205。判读见[v18 运行记录](#g2-v18-run)。部署的 INI 已按预注册第5步回`off`（v18 DLL/PDB 留在原位，v17 DLL/PDB 备份在`dist/deploy-backup-20261026-v17/`）。

下一步不再等日志，而是[有界审计问题](#next-audit)里的 audit16；在此之前不实施播种、不改驱动配方、不推进 G3+。测试步骤（复盘或重跑用）仍见上节[预注册步骤](#g2-v18-frame-trace)及包内`README-测试步骤.md`。

包文件：`build/behavior-graph-v18-trace-20261026/CharacterPanel-behavior-graph-v18-frame-trace-20261026.zip`（9,763,102字节）；SHA-256 `dad6b72ce17e53ae0d7e823bbe5bec28ad29aa0cf3f98dffa5885b42081c8ae2`。匹配DLL/PDB GUID `{7F586C2E-9F62-4CE8-B33B-D19AFE9C6461}`、age317。完整二进制哈希、源文件身份与限制见包内`manifest.json`和[构建记录](build.md#v18-frame-trace-build)。

首次记录的那份包（身份`…-92a3ed071d+dirty-cl94faaed0c6-20261026T063108Z`、ZIP `cd364569…`、DLL`e24ad2a5…`）在本机已不存在：`build/`当时仍带标签`scenegraph`、只有v17 DLL/PDB。因此按同一提交`7ee4cce`（文件哈希与包内manifest所列一致）重建并重新打包，实际包身份为`CharacterPanel-scenegraph-behavior-graph-v18-frame-trace-20261026-7ee4cce3bf-cl94faaed0c6-20261026T143615Z`；DLL SHA-256 `72c33a0e…`（1,249,280字节）、PDB `1baceb55…`。与旧记录的两处惰性差异：标签带提交后身份而非`92a3ed0+dirty`；原先未跟踪且无人引用的`src/base/util.cpp`/`util.h`已不在检出中，未再编译。旧哈希只作历史，不能当作本次加载证明；分析回传时以实际加载身份和包哈希为准。

回传清单（本轮已按此执行，供后续复用）：

- 完整`CharacterPanel.log`，包含实际`SCENEGRAPH BUILD`身份、原有源对照/恢复行及新`GRAPH FRAME`，同时保留所使用包的manifest。
- 首次开包、关包及异常发生时刻与操作；有崩溃时保留对应报告，不能用最后一条日志推断执行到达点。
- 若新行缺失或不足，保留实际runtime版本、INI配置、是否重启、该进程此前是否已触发过实验，以及实际加载DLL的身份。实验每进程至多一次；缺行仍是不完整证据，不直接判失败原因。

`completed`、头部字段或raw哈希变化都不自动通过G2。待新日志到达后，先按同一步号与物理槽身份比较五阶段，再决定下一个有界问题；原生写入者、生成载荷、内部ctx及应用/生命周期仍按既有判据待证。

<a id="g2-v19-contract"></a>
## G2 v19 契约、实现与运行（2026-10-26，眼位输入因果实验；已运行：分支 6，无 arm）

**目的**：只验证[审计十六](behavior-graph-construction-audit.md#audit16)给出的因果链——`record+0x18`（`BSAnimationUpdateData::optionalEyePosition`）非空 → 0xbb252b 眼位推进块把 `t_smaller` 抬出复位值 → 下一次 63587 分岔进入 Path B → 原生 `0x240`/`0x246` 改变 → 63581 的门是否因此可过。它是**单变量因果实验**，不是修复，也不改变 G2 验收；即使全部命中，G2 仍未通过。

**实现状态（2026-10-26）**：代码已实现并提交 `e17d5d1`；Release 构建身份 `CharacterPanel-scenegraph-behavior-graph-v19-eye-arm-20261026-e17d5d12f6-cl94faaed0c6-20261026T153952Z`（DLL SHA-256 `344d9abb…`，1,249,792字节；PDB `9848cd14…`，35,418,112字节；CodeView `{7F586C2E-…}` age 319），三个 engine-free 案例通过。**已部署（未打包）并运行一次**：v18 的 DLL/PDB 与 `off` INI 备份在 `dist/deploy-backup-20261026-v18/`；23:46 会话落在预注册分支 6（无 arm），部署 INI 已回 `off`，v19 DLL/PDB 留在原位，详见[运行记录](#g2-v19-run)。实现点=[frame_trace_policy.h](../src/animation/behavior_graph/frame_trace_policy.h) 的 `EyePoint`/`make_eye_point`/`arm_observed`、[frame_trace.cpp](../src/animation/behavior_graph/frame_trace.cpp) 的 `schema=v19` 行与 summary `arm-observed`、[independent_graph.cpp](../src/animation/behavior_graph/independent_graph.cpp) 每步记录的 `+0x18`；除 `inject`+精确 1.6.1170 外的模式不改记录。

**依据（审计十六，RVA）**：臂块门 = `record+0x18 != 0`、`record+0x2A == 0`、`record+0x2B != 0`、`[graph+0x248] == 0`，且（`record+0x2C == 0` 或交换前旧读槽 `+4>2` 且 `+0x38>0`）；臂块写 `t_smaller = xmm6 + t_larger`（仅当大于 `1.25*dt`）并跳过复位，`xmm6` 由 rootNode 距离决定，**且 0xbb2603 的 `63864()` 会再归一它**（审计十六原写的“rootNode 为空时固定 0.25”已被 v19 运行期否证，见[运行记录](#g2-v19-run)）；Path B 条件 = `+0x2A==0 && t_larger ≥ 0 && t_smaller > -MAXFLT`，Path B 写 `0x240 = 63575(graph, record+0x18, record+0x2C, &out, record+0x28)`、`[0x246]=0`，不交换、不复位；63581 门 = 双槽 `+4>2`、`+0x38>0`、`t_larger ≥ 0`、`t_smaller < 0`，`f=1.0` 只由 63582 写到 63666 的输出元素。

**唯一改动**：在既有 inject 驱动循环的每步 `update_record`（[independent_graph.cpp](../src/animation/behavior_graph/independent_graph.cpp) 里的 `alignas(8) std::uint8_t update_record[0x30]`）写 `*reinterpret_cast<void**>(update_record + 0x18) = &eye;`，`eye` 是栈上 3×float：

- `[candidate + 0x218]`（rootNode）非空时 `eye = {rootNode[0xA0], rootNode[0xA4], rootNode[0xA8] + 900}`（距离 900 > 800，取满步长 0.25；沿 Z 偏移，不耦合任何轴向语义）。
- rootNode 为空时 `eye = {0,0,0}`（此时臂块固定 0.25，不读内容，但仍要求指针非空）。

指针只在该次 63587 调用期间有效，不保存、不跨调用、不写引擎内存。其余一切不变：调用序（63602 → `[behavior+0x12E]` 写 → 63587 → `0x240` 镜像 → 63581）、40 行 `INDEPENDENT GRAPH` 预算、30+1 行 `GRAPH FRAME` 预算、源对照/保留/恢复逻辑与默认 `off`。

**臂窗口提醒（决定本轮可解释性）**：`+0x2C=1` 时臂块还要求“交换前旧读槽 `+0x38>0`”。v18 只有 step 0 的旧读槽带 slot 2 写入的 1.0，随后 Path A 复位会把它清零，而 `f` 又只能由 63582 写。因此**本实验的臂窗口实际只有第 0 步**：第 0 步若未成功 arm，本轮即落预注册分支 2/6，不得重跑调参。

**观测（不新增行种，预算不变）**：行前缀改为 `GRAPH FRAME schema=v19`，每条既有阶段行末尾追加 `root218=<ptr|null> b248=<byte> eye=<ptr|null>`；`summary` 追加 `arm-observed=yes|no`（判据：post-update 的 `min(t0,t1)` 高于 `-FLT_MAX`，即不再是 `{0,-MAXFLT}` 复位值）。阶段行数仍为 30+1，`phase=post-update1`/`pred`/`slots` 保持原字段、不新增行。

**预注册判读（不得事后放宽）**：

1. `root218` 全程 null 且 `b248 != 0`（门不满足）→ 记“臂块外层门不满足”，**停止**；下一有界问题是 `[graph+0x248]` 语义与 rootNode 来源，不得改记录继续试。
2. `eye != null`、臂块生效（t 被改写）但下一次 fork 仍 Path A → 说明 fork 还有未记录的输入：**停止**回审计，不得加 dt 变体或改 `+0x2A`。
3. 出现 Path B 签名（下一个选中步 `w245=0x0`、`b246=0`；`0x240` 同时记录其原生值，LOD 表可能把它降到 0，不要求非 0）且 63581 仍因双槽 `+0x38=0` 失败 → 记“因果链①成立、②被 f 门挡住”；下一有界问题=63582 的首写条件与双槽 f 初值（新契约），**本轮回写 f 属违规**。
4. 63581 门全过且 63582 运行（`f`/`numData` 被写）→ 记“因果链②成立”；再单独看 pose。pose 不变时结论只说“门在 63666/63582/58423 下游”，不得宣称 G2 通过。
5. 崩溃/断言 → 立即停，保留日志与崩溃报告，部署与 INI 回到 v18 状态（`off`）；同一会话不得重试。
6. `eye` 非空但 t 毫无变化 → 记“臂块内部门/阈值未满足”（`xmm1 > 1.25*dt` 或旧读槽 f 门），**停止**，不调偏移量重试。

**构建与部署**：构建已完成（`CHARACTER_PANEL_BUILD_LABEL=behavior-graph-v19-eye-arm-20261026` 的 Release，离线三例通过）；**已按 v18 流程部署（不打包）并运行一次**：v18 对（DLL `72C33A0E…`、PDB `1BACEB55…`）与 `off` INI 备份在 `dist/deploy-backup-20261026-v18/`，MO2 实例的 `CharacterPanel/SKSE/Plugins/` 现为 v19 DLL `344d9abb…`/PDB `9848cd14…`；本轮 INI 为 `inject`，判读后已回 `off`。判读见[运行记录](#g2-v19-run)。回滚=还原那三份备份并重启。

**验收边界**：本契约只测“眼位 → t → Path B → 63581 门”的因果链；G2 仍需有意义的原版 idle、有效骨运动、源侧对照不变与后续生命周期证据。运行前仓库按 [v18 停点](#v18-handoff) 与[有界问题](#next-audit) 执行，默认 `IndependentGraph=off`（部署副本除外）。

<a id="g2-v19-run"></a>
## G2 v19 运行记录（2026-10-26 会话，无 arm：臂块步长门槛与候选 rootNode 缺失）

构建 `CharacterPanel-scenegraph-behavior-graph-v19-eye-arm-20261026-e17d5d12f6-cl94faaed0c6-20261026T153952Z`（DLL `344d9abb…`，与构建记录一致）。2026-10-26 23:46 会话：`Panel menu opening=true` 23:46:47.728 → `opening=false` 23:46:59.707，实验行 23:46:47.739–47.751。[原始日志](diagnostics/CharacterPanel-g2v19-eye-arm-20261026-2346.log)原样归档（41,470字节，SHA-256 `45AAEE09…`）。部署 INI 判读后已回 `off`，v19 DLL/PDB 留在原位。

**行数与预算**：`GRAPH FRAME schema=v19` 30条阶段行 + 1条 summary；`INDEPENDENT GRAPH` 40行；`BEHAVIOR GRAPH` 36行（三份12行，逐行全等）；无`[E]`、无崩溃，15条`[W]`为既有 DAC/studio-light 警告。summary：`completed=120 pose-available=120 pose-missing=0 raw-hash-changes=0 baseline=bc160a8c2b3a5a58 arm-observed=no`。

**决定性观测**：

- 每条 FRAME 行：`root218=0x0`、`b248=0`、`eye=0x1d6e56bd30`（按契约非空；rootNode 为空故内容为零向量）。
- step 0 pre/post-pipeline：`t0=t1=-MAXFLT`，slot0=E0 `+38=1.000`（slot 2 写入）、slot1=E1 `+38=0.000`，双槽 `+4=21`、`+32=0`。
- step 0 post-update：`t0=0`、`t1=-MAXFLT`，`w245=0x101 b246=1 b249=1`，双槽交换且 `+38` 均为 0；之后所有阶段 t 不变，`arm-observed=no`。
- 120步、`candidate-pose-0/60/120=bc160a8c2b3a5a58`；源 pose 前后 `df87ebc0d3550cda`，`source-stable=true retained=true`；`phase=pred step=0/60 r=0x00`。

**判读（预注册分支 6 落定）**：臂块外层门全部满足——`record+0x18` 非空（`eye` 非空）、`+0x2A=0`、`+0x2B=1`、`[graph+0x248]=0`、step 0 的旧读槽 E0 `+0x38=1.0>0` 且 `+4>2`——所以不是分支 1，也不是旧读槽 f 门。失败在臂块内部：rootNode（`[graph+0x218]`）为 0 时距离分支被跳过（`xmm2=FLT_MAX`），步长候选取 0.25，但 0xbb2603 的 `63864()` 返回值随后覆盖它（`63864()>0.25` 才保留 0.25，否则 `xmm6 = 63864() - 0.5*dt`），最终 `xmm1 = xmm6 + t_larger` 必须大于 `1.25*dt=0.02083` 才写 t。观测到 t 完全未变 ⇒ `xmm1 ≤ 0.02083`；在 `t_larger=-dt=-0.01667` 下即 `xmm6 ≤ 0.0375`（等价于 `63864() ≤ 0.0458`，或返回 NaN）。按契约**停止，不调偏移量、不改 dt 重试**。

**本轮新增/修正的事实**：

1. 候选图 `[graph+0x218] rootNode = 0`——首次直接读到；与审计十四时代“v15 借用期 rootNode 非空”的旧读法冲突，说明候选装载链没有装配 rootNode（与审计十“装载止步 63949、63870 尾链”一致）。rootNode 为空也让 63575 的距离阈值路径不可用。
2. 审计十六“rootNode 为空时臂块步长固定 0.25”被运行期否证：0xbb25fc 之后还有一次 `63864()` 归一（0xbb2603–0xbb261c），小返回值会把步长压到 0.25 以下；本轮的 no-arm 正发生在这里。
3. `b248=0` 使 `[graph+0x248]==0` 门满足；`eye` 按设计非空——v19 的记录字段本身没有传错。

**判定**：G2 仍 **NOT PASSED**——没有 arm、没有 Path B，pose 与 v11–v18 同为 `bc160a8c2b3a5a58`；本轮只证明“眼位非空 + 外层门满足”不足以保证臂块执行。

**下一有界问题（架构决策点）**：[审计二十一](behavior-graph-construction-audit.md#audit21) 给出决定性事实：63602 的生成块整体受第一实参 `behaviorGraph` 守卫（0xbb62ae），块内含 65149 生成与 63602 内部 63587；壳图没有 behaviorGraph，所以**这两次调用在 v7–v19 里从未发生**。同时 63602 的 sink 是**栈子对象指针描述符**（`[rbp+0x140]` 起 12 个槽），与 0xba4e30 的 sink（首字段 BSFixedString）是不同具体类；`65177` 拼 `<obj+0x38>/<buffer+0x38>`，`59268` 只是 `movss [rcx+0xF0], xmm1; ret`。结论：**没有安全可行的“最小生成实验”**——要候选被生成，只能把候选登记进某个 manager（突破 G1 的 no-registration 边界），或接受独立图停在现有状态。这是架构选择，不是下一轮观测；`audit21` 之后不新增原生调用、不改配方。

<a id="g2-v20-generation-entry"></a>
## G2 v20 预注册：受控恢复原生生成入口（2026-10-27，未运行）

> audit23 静态补注：本契约和既有 v20 包保持。63589 的外层职责包含生成及后续场景应用，因而一个入口变量不是一个内部阶段；新结果需按[复制/应用分工](behavior-graph-construction-audit.md#audit23)解释，不能只据变化就归因 Generate。

这是audit22后的新设计，不复活旧fake-sink方案。检验v19直调63581是否绕过原生同步生成/apply；尚未证明manager登记必要，本轮不新增登记；包装器的潜在隐藏依赖仍未排除。依据[完整对齐审计](behavior-graph-construction-audit.md#audit22)，63589两参为candidate与存活mode字节指针，指针同步消费不保存；b246非零走63568，mode0到58343→58376→Generate，ctx/output/额外参数由引擎准备。

**单一行为变量：** 仅现有`IndependentGraph=inject`且精确AE1.6.1170，将每步63581换成63589(candidate,&mode0)。mode0为驱动栈上零字节，调用期间存活；新relocation只在此profile获准驱动时解析。其他模式/版本保持63581。眼位表达式/栈生命期、120×1/60、flags/0x12E/0x240、world借用/add/remove、事件、retention、源对照均保持v19。不登记源/私有manager、不直调Generate、不造buffer/sink或种子。

**一次guard：** 激活后、context/event/world借用前检查候选behavior非空且私有、outer/character一致且active，activeNodes/变量/pose可写存储可用且无明显源别名，双输出wrapper/元素不与任一源槽混用。失败输出一次理由并跳过v20驱动，保留外层源/end观察及retention，不跨活跃借用return、不静默回退。只排除明显别名，不证明深层隔离；双候选槽自别名拒绝是本实验的保守条件，不推广成全部引擎图必须物理分离。

**观测：** `GRAPH FRAME schema=v20 entry=63589 mode=0`只标意图，不证明内部63568/Generate执行；31条预算、选中步、v19眼位/arm记录保持。pre/post-sample兼容名称现包围外层生成/apply；completed只计返回63589，不代表有效采样。旧40条INDEPENDENT GRAPH与源快照另计；guard至多一条独立GRAPH ENTRY，合计新增前缀至多32行（FRAME30+1、ENTRY1）。guard失败仅有ENTRY skip，无FRAME是预期跳过，不是漏采样。

**预注册判读：** guard/原生门失败→跳过并检查真实输入；头部原生变化→定位wrapper边界但不自动通过；raw pose变化→进一步核对有限值/必要骨/相位/源控制；填头而pose不变→限于实际generation/output/apply问题，不播种/登记manager；源漂移/崩溃→立即停止、保留完整日志与匹配PDB，不在同一会话调参重试。G2及G3–G6、装载销毁判据不放宽。

本轮只加[一个参数化生产guard案例](../tests/behavior_graph_generation_entry_test.cpp)，不模拟引擎；旧预算/进度/眼位算法未改、既有证据复用。Release与包身份见[构建](build.md#v20-generation-entry-build)。v20未运行，最新实际运行仍为v19，G2未通过。

<a id="v20-handoff"></a>
## v20 测试交接与当前停点

**v20a 已运行一次并取得首次运动证据：入口 `63589(mode0)` 到达，输出头被填充、Path B 与 arm 出现，候选 raw pose 在 120/120 次调用中变化，源侧控制不变；但逐骨/有限值与视觉证据仍缺，G2 仍 NOT PASSED。** v21 pose 内容解码已于 2026-10-27 22:18 运行一次：三点全有限、`moved=94`、两根抽样骨在动，但 `source-equal=8/21/15` 命中预注册第 3 条；该条经[v21 运行记录](#g2-v21-run)审计为误报（不同址、跨 run 不相关、判据为精确值相等），G2 仍未通过。下一步已由 [v22](#g2-v22-contract) 实现、部署并运行一次：[v22 运行记录](#g2-v22-run)取得 pose 内容证据（三点全有限、94 条在变化、两骨在动、`alias=no`、`source-equal-moving=0`），G2 的剩余缺口是跨批次持续性与语义对应；不新增原生调用、不预先播种或推进后续关卡。

原冻结包（身份 `…-242d0a50c5+dirty-cl94faaed0c6-20261027T070457Z`、ZIP `3f9f9182…`、DLL `6c76c2a7…`）不在本检出中，故 2026-10-27 以同一提交源重建：新身份 `CharacterPanel-scenegraph-behavior-graph-v20-generation-entry-20261027-a83e5229e7-cl94faaed0c6-20261027T134152Z`（干净 `a83e522`，身份无 `+dirty`），DLL `13688a9d…`（1,253,888 字节）、PDB `07a509da…`（35,467,264 字节），CodeView GUID `{7F586C2E-9F62-4CE8-B33B-D19AFE9C6461}` age 320；ZIP 9,750,395 字节、SHA-256 `076cf6f7…`，五个条目重解压复核通过。两次构建不声称字节等同，旧哈希只作历史；判读一律以实际加载的 `SCENEGRAPH BUILD` 身份与包内 manifest 为准，不用较新提交号替代重新构建证据。详见[构建与重建记录](build.md#v20-rebuild-2026-10-27)。

使用 `build/behavior-graph-v20-generation-entry-20261027/CharacterPanel-behavior-graph-v20-generation-entry-20261027.zip`，按包内 `README-测试步骤.md` 和[预注册契约](#g2-v20-generation-entry)操作。已按 v18 流程部署（2026-10-27 21:42）：v19 三件套（DLL `344d9abb…`、PDB `9848cd14…`、`off` INI 1,034 字节）备份在 `dist/deploy-backup-20261026-v19/`，MO2 实例 `CharacterPanel/SKSE/Plugins/` 现为 v20 DLL/PDB，仓库样例仍为 `off`；部署 INI 曾为 `inject`（1,055 字节 `1db104fa…`，其余键未动），首轮崩溃后已按预注册回 `off`。回滚=还原该备份并重启。

回传完整 `CharacterPanel.log`、实际 `SCENEGRAPH BUILD` 身份、所用 manifest，以及首次暂停开包、关包和异常发生的时刻/操作。先核对精确 AE 1.6.1170、匹配 DLL/PDB、合并后的 INI 和重启状态；实验每进程只触发一次。缺行时还要保留该进程此前是否已经运行过实验，不能据缺行猜原因。测试结束把 `IndependentGraph` 恢复为 `off` 并重启；异常或源漂移时立即停止并保留日志/崩溃报告与匹配 PDB，不在同一会话调参重试。

`GRAPH ENTRY` 最多 1 行，`GRAPH FRAME` 最多 30 阶段行加 1 摘要；新增前缀合计最多 32 行，旧 40 条和源快照另计。guard skip 没有 FRAME 是预期跳过。entry/mode 仅标调用意图，completed 只计返回的 63589，raw hash 含 padding，均不等于有效生成或采样。

按[audit23 的分工](behavior-graph-construction-audit.md#audit23)比较生成 → poseLocal 复制 → 场景应用。恢复一个入口变量同时恢复多类内部职责，有变化不能唯一归因 Generate；现包没有真实数值载荷、原生分支或 node 应用 trace。

| 新结果 | 下一步判读 |
| --- | --- |
| guard skip / 门未过 | 核对实际输入边界，不以插件静默换回旧入口、manager 登记或手工种子绕过。 |
| 头部变化 | 先定位 wrapper 边界，不自动认定 pose 有效。 |
| raw pose 变化 | 再核对有限数值、必要骨、相位和源控制，不自动通过 G2。 |
| 头已填、pose 仍不变 | 限定到生成、复制和应用问题，不能直接归咎插值 t 或播种。 |
| 崩溃 / 源漂移 | 停止，保留完整证据；不在同次会话重试。 |

完整 G3–G6、OAR 语境、深层隔离和装载图回收仍未验收。上表"崩溃 / 源漂移"行已经发生了：首轮结果见下节，此处只记录交接与那次运行，不新增诊断代码。

<a id="g2-v20-run"></a>
## G2 v20 运行记录（2026-10-27 21:48 会话，入口前崩溃）

构建 `CharacterPanel-scenegraph-behavior-graph-v20-generation-entry-20261027-a83e5229e7-cl94faaed0c6-20261027T134152Z`（DLL `13688a9d…`、PDB `07a509da…`），部署 INI=`inject`，运行时 `1-6-1170-0`。21:46:12 加载，`SCENEGRAPH BUILD` 身份与包内 manifest 逐字一致；21:48:57.911 `Panel menu opening=true`，实验行 21:48:57.922–57.931，随即崩溃。原始日志原样归档为 [CharacterPanel-g2v20-entry-crash-20261027-2148.log](diagnostics/CharacterPanel-g2v20-entry-crash-20261027-2148.log)（16,920 字节，SHA-256 `2278201CDBCF9090EC1CC3FD038C751D70B66C3FF4614C8CE5B3D63D89E11127`），崩溃报告为 [CharacterPanel-g2v20-crash-report-20261027-2148.log](diagnostics/CharacterPanel-g2v20-crash-report-20261027-2148.log)（119,326 字节，SHA-256 `4074DF23232254922CEBC44E1DEBD187C50DBD4798C53C5161899ACA0788D2F7`）。

**guard 与崩溃位置**：`GRAPH ENTRY schema=v20 … outcome=ready reason=ready entry=63589 mode=0` —— 唯一的 guard 通过（behavior 非空、私有且 outer/character 一致，activeNodes/变量/pose 可写存储可用，四组输出/元素不与源槽或彼此混用）。崩溃是 `EXCEPTION_ACCESS_VIOLATION`，`CharacterPanel.dll+0x28067`，`independent_graph.cpp:955` 的 `mov r8, [rbx+0x88]`，`rbx=0x7BD6CD336F4B8024`（非规范指针），访问目标 `[rbx+0x88]`。该行属于 **v16 起就存在的只读 g3-gates 诊断**，位置在驱动循环之前，因此本轮 **63589 一次都没有被调用**：没有 `GRAPH FRAME`、没有 `completed`、没有 pose 采样，也没有任何关于生成/输出/应用的证据。

**原因（静态声明与运行期一致）**：`RE::hkbContext` 的声明大小正是 `0x50`（[hkbContext.h](../extern/CommonLibSSE/include/RE/H/hkbContext.h) 末尾 `static_assert(sizeof(hkbContext) == 0x50)`），而诊断把 `ctx+0x50` 当作 chain 起点读取 —— 那是 `context_storage[sizeof(RE::hkbContext)]` 数组**末尾之后**的栈槽，属越界读。v16/v18/v19 那次读到 0，链立即短路（三份日志均为 `ctx+50=null +40=null +88=null`）；本轮该槽非空，`[+0x40]` 再读到垃圾，`[垃圾+0x88]` 才崩溃。这也印证 [audit23](behavior-graph-construction-audit.md#audit23) 的纠正：原生 58423 的链是 `[context]`（character）→`+0x50`（setup）→`+0x40`（data）→`+0x88`（footIkDriverInfo），插件那条少了首次解引用。**结论：这是自 v16 起潜伏在观测代码里的越界读，v20 只是改变了栈布局把它暴露出来；没有任何证据表明 63589 会崩溃，也没有证据表明它被调用过。**

**判定**：G2 仍 **NOT PASSED**。按预注册"崩溃→立即停、保留完整日志与匹配 PDB、不在同一次会话重试"，本轮不再上机、不改配方、不调参；部署 INI 已回 `off`（v20 DLL/PDB 留在原位，回滚备份 `dist/deploy-backup-20261026-v19/` 不变）。

**下一有界问题（先修诊断，不新增原生调用）**：
1. 把 g3-gates 的链起点改为 `[context]` 再 `+0x50/+0x40/+0x88`（audit23 的正确链），或直接移除这条 v16 遗留行；两者都不触碰 v20 的单一入口变量。
2. 复查插件内其它 `context + offset` 读取是否越过 `0x50`，以及同类"链首缺一次解引用"的写法。
3. 任一修复都构成新的构建身份（v20a），需重新构建/打包/部署后再取一次上机结果；在那次结果之前，本轮只证明"入口前的诊断越界"，不能用于评价 63589 的行为。

**v20a 修正与部署（2026-10-27 21:56）**：按上列第 1 项修正链起点——`[context]` → `hkbCharacter::setup`（+0x50）→ `hkbCharacterSetup::data`（+0x40）→ `hkbCharacterData::footIkDriverInfo`（+0x88），全部走声明成员，`[graph+0x100]` 改用 `hkbFootIkDriver::disableFootIk`，并在每次解引用前过新增的 [observation_chain_policy.h](../src/animation/behavior_graph/observation_chain_policy.h)（`plausible_object_pointer`，只读声明成员、不再触碰对象之外的栈槽）；行字段改为 `character/setup/data/footik-info`。三个 engine-free 用例全部通过（新增策略 11 行、guard 22 行、frame-trace 3 例，`g++` 15.1 `-Wall -Wextra -Werror`）。身份 `CharacterPanel-scenegraph-behavior-graph-v20a-chain-fix-20261027-7d18a90787-cl94faaed0c6-20261027T135614Z`（干净 `7d18a90`，DLL `691f7592…`、PDB `5b3d8cc8…`、CodeView age 321），ZIP `f4586e16…`。v20 三件套已备份到 `dist/deploy-backup-20261027-v20/`，MO2 现为 v20a DLL/PDB。**构建与部署本身不产生运行证据**；随后的一次运行见下节的 [v20a 运行记录](#g2-v20a-run)。

<a id="g2-v20a-run"></a>
## G2 v20a 运行记录（2026-10-27 22:02 会话，入口到达、pose 首次变化）

构建 `CharacterPanel-scenegraph-behavior-graph-v20a-chain-fix-20261027-7d18a90787-cl94faaed0c6-20261027T135614Z`（DLL `691f7592…`、PDB `5b3d8cc8…`），部署 INI=`inject`，运行时 `1-6-1170-0`：22:01:34 加载（`SCENEGRAPH BUILD` 与包内 manifest 逐字一致），22:02:36.590 开包，实验行 22:02:36.597–36.600，22:02:42.216 关包（`DAC restore … result=matched`）。**无崩溃、无 `[E]`**；15 条 `[W]` 均为既有 DAC/studio-light 警告。原始日志原样归档为 [CharacterPanel-g2v20a-entry-20261027-2202.log](diagnostics/CharacterPanel-g2v20a-entry-20261027-2202.log)（43,330 字节，SHA-256 `E05BFEAF9AAA49E7DE026A17D62F0050BC59B3B388D5B52BFE0E43F35349FAD0`）。

**预算与门**：`GRAPH ENTRY` 1 条、`GRAPH FRAME` 30+1、`INDEPENDENT GRAPH` 40、`BEHAVIOR GRAPH` 36（三块 12 行逐行全等）。guard `outcome=ready`；修正后的 g3-gates 链在候选上四跳全部可读：`g100=0x1d572e9c8a0 flag4a=0x00 character=0x1dab1c853c0 setup=0x1d572e9c9c0 data=0x1d5371d26f0 footik-info=0x1d5371d5b70`（character 与 `phase=driver` 的候选 char 同址，data 与候选 setup-data 同址）。

**首次走到 63589 之后的新事实**：

1. **输出头被真实填充**：step 0 `post-sample` 起 slot0 `numData=116 onFraction=1.0`，step 1 交换后 slot1 同值，step 2 起双槽均 `numData=116 f=1.0`；`tracks-end[+32=0x0074 +38=1.000]`。v7–v19 从未出现。
2. **首次 arm 与 Path B**：step 1 `post-update` 的 t 对为 `t0=-0.0166667 t1=0.175`，step 2 起 `w245=0x0 b246=0`（Path B），summary `arm-observed=yes`。t 对在 Path A 复位后还会再次 arm（step 119 为 `t0=0.05 t1=-0.2`）——因此 v19 契约里"arm 窗口实际只有 step 0"是 **v17 配方（无生成、槽头恒 0）的产物**，不是引擎限制。
3. **生成内容变化**：`gen0elem` 由 `cf37f9140cd1eb83` 变为 `cbd25238b4fbce52`，双槽元素哈希一致；step 60 起原生 `cand-anim=0x0074`（此前恒 0，只有插件 mirror）。
4. **raw pose 首次变化**：`candidate-pose-0=bc160a8c2b3a5a58`（bind 基线）→ `60=63f638a6ac08d955` → `120=8fb4cd336c4395bf`；summary `completed=120 pose-available=120 pose-missing=0 raw-hash-changes=120 first-changed-completed=1`，即**每次返回的 63589 都改变了候选 pose 原始缓冲**。
5. **源侧对照不变**：`source-pose-before=983fc2d4a169fa5b source-pose-after=983fc2d4a169fa5b`、`source-drift=none`×3、三份源基线逐行全等、`source-stable=true retained=true`；`world-restored=true ragdoll-removed=true`。
6. **状态机活着**：active 节点 97→125→111，变量集哈希逐段变化（`893b33fd…`→`214e2111…`→`a4bb41b6…`），`post-drive children-flag=1`（注入前为 0）。

**判读（预注册分支"raw pose 变化"）**：本轮证明 **v17–v19 的空结果来自入口选择**——直调 63581 跳过了 63589 的外层生成/头部回填/arm-apply，所以生成内容、槽头、Path B 与 pose 全都不动；把入口换回 `63589(mode0)`、其余不变后这些环节同时出现，源侧控制保持。这回答了 audit21/22 遗留的"入口是否必要"的运行期问题。

**但 G2 仍 NOT PASSED**：预注册明写 raw pose 变化不等于有效骨骼运动，而本包只有含 padding 的原始缓冲哈希、元素头哈希与计数/分数，没有逐骨或有限值证据，也没有视觉确认；summary 自身标 `acceptance=unjudged fingerprint=raw-buffer-padding-sensitive generator-payload=unobserved native-context=unobserved`。装载图销毁、全局收敛、G3–G6 与独立 OAR 语境同样未验收。

**下一有界问题（pose 内容，不新增原生调用）**：新增一个只读解码契约——在 step 0/60/120 对固定的一组（例如四个）具名骨从 `poseLocal` 解码 translation/quaternion，要求全部有限、至少这些骨在 60/120 步发生变化，并在源图播放同一 `mt_idle` 片段时对同址骨做对照；预算沿用本轮的 30+1。通过这一检查后才谈"有意义的原版 idle"，之后才是装载图销毁与 G3。面板内的视觉确认由观察者另行判断，不能替代数值证据。

<a id="g2-v21-contract"></a>
## G2 v21 契约：pose 内容解码（2026-10-27，已实现/已部署/未运行）

**动机**：v20a 取得首次 raw pose 变化（120/120 次返回的 63589 都改变原始缓冲），但 raw FNV 含 padding，只能证明字节变化。本契约**只增加只读解码**：不改任何原生调用、不写任何 pose 缓冲、不改 guard/眼位/步数/flags/world/事件/保留配方，也不改既有预算。

**观测**：在 `inject` 且精确 1.6.1170 的驱动里，于 step 0/60/120 三个检查点，对候选 `character.poseLocal`（声明 `hkQsTransform`、步长 0x30、count=`numPoseLocal`、上限 256）逐条解码 translation xyz / rotation xyzw / scale xyz；与上一检查点逐条比较（任一分量差 > `1e-6` 记"变化"，NaN 的 delta 记为无穷，绝不会被当成"未变"）；对源图 `poseLocal` 做同样统计；再统计候选与源在**同索引**上完全一致的条目数（别名观察）。四个 `hkVector4` 的第四个 lane 属 padding，不参与判定。纯算术落在 [pose_content_policy.h](../src/animation/behavior_graph/pose_content_policy.h)（`PoseSample`/`compare_pose`/`count_identical`），附一个 20 断言的 engine-free 用例。

**预算**：`pose-content` ≤3 行（每检查点 1 行：`entries/finite/nonfinite/moved/max-delta/source-*`）+ `pose-bone` ≤12 行（每检查点 4 根，索引 `0 / ⅓ / ⅔ / 末`，行内带引擎骨名；骨名读取先过 `plausible_object_pointer` 并截断 48 字符），合计新增 ≤15 行，与 `GRAPH FRAME` 30+1、`INDEPENDENT GRAPH` 40、`BEHAVIOR GRAPH` 36 分账。骨名不可读或 `entries=0` 时按 `unavailable` 记录，不给结论。

**预注册判读（逐条，命中即停）**：

1. 任一检查点 `nonfinite>0` 或任一 `pose-bone` 行 `finite=false` → **pose 内容无效**：停止，不进入 G3；下一问是 audit23 的三条 poseLocal 复制路径中哪一条写入了非有限值。
2. `entries=0` 或骨名 `unreadable` → 记 `unavailable`，不给结论。
3. `source-equal>0` → 候选与源存在同索引完全一致的条目，**别名嫌疑**：停止并审计，不得当作运动证据。
4. 三点全有限、`source-equal=0`，且 step 60 与 120 的 `moved` **均 ≥16（共 116 条）**、四根抽样骨中**至少两根** `moved=yes` → **pose 内容证据成立**（有限、在变化、未别名）。这仍**不是** G2 通过：视觉确认、装载图销毁/全局收敛、G3–G6 与独立 OAR 语境仍各自需要验收。
5. 全有限、未别名，但 `moved<16` → 记"原始缓冲变化主要来自 padding/噪声"，停止；不把 v20a 的 pose 变化升级为有效骨运动，下一问转为输出选择/混合（63666/63582/58423）与场景应用（63856）。
6. 崩溃或源漂移（`source-stable=false`，或源 pose 前后哈希不等）→ 立即停止，保留完整日志与匹配 PDB，不在同一会话调参重试。

**实现与部署（2026-10-27 22:10，未上机）**：身份 `CharacterPanel-scenegraph-behavior-graph-v21-pose-content-20261027-fe7c4d15e1-cl94faaed0c6-20261027T141004Z`（干净 `fe7c4d1`；DLL `e0880651…` 1,267,200 字节、PDB `787b3005…` 35,614,720 字节、CodeView age 322）。首次编译在 `independent_graph.cpp:1080` 因 `hkArray::operator[]` 取 `int32` 触发 C4267（`/WX`），改为索引声明数组 `data()[index]` 后通过；四个 engine-free 用例全过（pose 内容 20 断言、观测链 11 行、guard 22 行、frame-trace 3 例）。包 `build/behavior-graph-v21-pose-content-20261027/test-package/`（manifest `e2873121…`），ZIP 9,928,507 字节、SHA-256 `4cfa5ac8…`，五条目重解压复核通过。v20a 三件套备份到 `dist/deploy-backup-20261027-v20a/`，MO2 现为 v21 DLL/PDB，部署 INI=`inject`（1,075 字节 `11798909…`）；**本轮不上机，等待一次运行**。详见[构建记录](build.md#v21-pose-content-2026-10-27)。



<a id="g2-v21-run"></a>
## G2 v21 运行记录（2026-10-27 22:18 会话，命中第 3 条并审计为误报）

构建 `CharacterPanel-scenegraph-behavior-graph-v21-pose-content-20261027-fe7c4d15e1-cl94faaed0c6-20261027T141004Z`（DLL `e0880651…`），部署 INI=`inject`，运行时 `1-6-1170-0`：22:16:27 加载（`SCENEGRAPH BUILD` 与包内 manifest 逐字一致），22:18:12.436 开包，实验行 22:18:12.446–.458，22:18:23.522 关包（`DAC restore … result=matched`）。**无崩溃、无 `[E]`**；15 条 `[W]` 均为既有 DAC/studio-light 警告。原始日志原样归档为 [CharacterPanel-g2v21-pose-content-20261027-2218.log](diagnostics/CharacterPanel-g2v21-pose-content-20261027-2218.log)（47,393 字节，SHA-256 `601D2B47F9E94F3D7138E040A94BD6CFEC07E84F80F41156E9E2A18F6CF06717`）。

**预算**：`GRAPH ENTRY` 1、`GRAPH FRAME` 30+1、`BEHAVIOR GRAPH` 36；`INDEPENDENT GRAPH` 前缀合计 55 = 既有 40 + 本轮新增 `pose-content` 3 与 `pose-bone` 12（分账未越界）。

**观测**（检查点 0/60/120，均为 116 条）：

| 检查点 | finite | nonfinite | moved | max-delta | source-moved | source-equal |
| --- | --- | --- | --- | --- | --- | --- |
| 0 | 116 | 0 | n/a | n/a | n/a | **8** |
| 60 | 116 | 0 | 94 | 3.430847 | 0 | **21** |
| 120 | 116 | 0 | 94 | 2.313768 | 0 | **15** |

四根抽样骨（索引 0 `NPC Root [Root]` / 38 `NPC L Hand [LHnd]` / 76 `NPC L Finger30 [LF30]` / 115 `Belly`）三点全部 `finite=true`；step 60 与 120 时 38、76 为 `moved=yes`，0、115 为 `moved=no`（两者都是该图的恒等值条目：`p=(0,0,0)`、`q=(0,0,0,1)`、`s=1`，且 `source-equal=true`）。源侧三点 `source-finite=116 source-moved=0`，`source-pose-before=source-pose-after=bd107af608b00103`、`source-stable=true retained=true`、`world-restored=true ragdoll-removed=true`。

**预注册判读（逐条，命中即停）**：

| 条 | 条件 | 本轮结果 |
| --- | --- | --- |
| 1 | 任一检查点 `nonfinite>0` 或骨行 `finite=false` | 未触发（`nonfinite=0`，12 行全有限） |
| 2 | `entries=0` 或骨名不可读 | 未触发（`entries=116`，四骨名可读） |
| 3 | `source-equal>0` → 别名嫌疑 | **命中（8/21/15）→ 停止并审计，不得当作运动证据** |
| 4 | 三点全有限、`source-equal=0`、60/120 `moved≥16`、四骨两动 | 前置不成立，未取得；其余子条件实际已满足（`moved=94`；38/76 两根动） |
| 5 | 全有限未别名但 `moved<16` | 不适用（`moved=94`） |
| 6 | 崩溃或源漂移 | 不适用（无崩溃，源哈希前后相等） |

**判定**：G2 仍 **NOT PASSED**。按预注册不在同一会话调参重试；部署 INI 于 22:20 回 `off`（v21 DLL/PDB 留在原位，回滚备份 `dist/deploy-backup-20261027-v20a/` 不变）。

**审计：第 3 条是误报，别名假设被证伪**（全部取自现有日志，不依赖新运行）：

1. **指针不同址**。本轮候选 `pose-local=0x1d973570010/116`、源 `pose-local=0x1d972f94010 pose-count=116`；v20a 为 `0x1d573d84010` 与 `0x1d573b28010`。两张图各自持有 `poseLocal`，不存在共享缓冲。
2. **跨 run 不相关**。两轮源 pose 哈希不同（`983fc2d4a169fa5b` vs `bd107af608b00103`），而候选三点哈希两轮逐位相同（`bc160a8c2b3a5a58` / `63f638a6ac08d955` / `8fb4cd336c4395bf`）。若候选读取源缓冲，其内容必然随源内容变化；事实相反，并顺带证明该生成路径对同一输入是确定性的（含 padding 的整缓冲逐位可复现）。
3. **根因是判据语义**。[`pose_content_policy.h`](../src/animation/behavior_graph/pose_content_policy.h) 的 `pose_sample_identical` 用 `pose_sample_max_delta(...) == 0.0f`，即 translation xyz / rotation xyzw / scale xyz 十个分量的**精确相等**。`source-equal` 因此统计的是“值相等”而不是“共享内存”：恒等值骨（0、115）天然计入；`moved=94` 意味着约 22 条条目在检查点之间不变，与 `source-equal≈21` 同量级——两张图播放同一片段时，恒定通道骨必然携带同一常量值。**任何同片段图都会命中第 3 条，该条无法区分“常量骨”与“别名”。**

**下一有界问题（判据修正，不新增原生调用、不改配方）**：把别名判据换成两项可判定观测——①两个 `poseLocal` 的**指针同一性**（真别名检测）；②`source-equal` 只统计“与源相等**且**相对 step 0 发生过变化”的条目（别名若成立，该计数必 >0；常量骨则恒为 0）。原始 `source-equal` 保留为统计量、不进门槛。其余入口/guard/眼位/120 步/预算不动；修正构成新身份（v22），需再一次运行，在那之前 G2 仍为 NOT PASSED。

<a id="g2-v22-contract"></a>
## G2 v22 契约：别名判据修正（2026-10-27，已实现/已部署/未运行）

**动机**：v21 唯一的缺陷在第 3 条判据。[v21 运行记录](#g2-v21-run)已用现有日志证明它把“值相等”当成“别名”：候选与源 `poseLocal` 不同址、源哈希两轮不同而候选三点哈希逐位相同、`pose_sample_identical` 又是十分量精确相等——任何两张播同一片段的图都会命中。修正不改配方，只把该判据换成两项可判定观测。

**变更（只读；不加原生调用、不写缓冲、不改行数）**：

- `pose_buffer_alias(candidate, source)`：两个 `character.poseLocal` 的**指针同一性**；任一侧为 null 不算别名。
- `watch_aliasing(current, source, baseline, buffer_alias)` → `PoseAliasWatch{compared, equal, equal_moving, equal_static, has_baseline, buffer_alias}`。`equal_moving` = 同索引与源精确相等**且**相对 step 0 基线某分量变化 >1e-6 的条目数；step 0 基线即该检查点，故该点记 `n/a`；基线长度不足时整项记 `n/a`，不跨骨比较。
- `pose-content` 行追加 `source-equal-moving` / `source-equal-static` / `alias` 三个字段；仍为每检查点 1 条、共 3 条，`pose-bone` 12 条不变，不新增预算。
- 入口 `63589(mode0)`、guard、v19 眼位、120×1/60、world 借用、slot2/3、事件、保留与源侧对照全部不动。

**预注册判读（v22，取代 v21 的第 3、4 条；其余条沿用）**：

1. 任一检查点 `nonfinite>0` 或任一 `pose-bone` 行 `finite=false` → pose 内容无效，停止，转查 audit23 的三条 poseLocal 复制路径。
2. `entries=0` 或骨名不可读 → 记 `unavailable`，不给结论。
3. `alias=yes` → 两图共用同一缓冲，**真别名**：停止并审计。
4. `source-equal-moving>0` → 存在“与源相等且在变化”的条目，**别名嫌疑**：停止并审计。
5. 三点 `finite=entries`、`alias=no`、step 60 与 120 的 `source-equal-moving=0`，且两点 `moved` **均 ≥16（共 116）**、四骨至少两动 → **pose 内容证据成立**（有限、在变化、未别名）。仍**不是** G2 通过。
6. 全有限未别名但 `moved<16` → 记“原始缓冲变化主要来自 padding/噪声”，下一问转输出选择/混合与场景应用。
7. 崩溃或源漂移 → 立即停止，保留完整日志与匹配 PDB，不在同会话重试。

`source-equal` 与 `source-equal-static` **只作对照**：同片段两图在这两项上非零是预期现象，不构成失败。

**实现与部署（2026-10-27 22:56，当轮未上机）**：身份 `CharacterPanel-scenegraph-behavior-graph-v22-alias-watch-20261027-22ef58464f-cl94faaed0c6-20261027T145636Z`（干净 `22ef584`，子模块 `94faaed0`，无 `+dirty`；DLL `5756e0e6…` 1,268,736 字节、PDB `b763d05f…` 35,631,104 字节、CodeView age 323）。四个 engine-free 用例全过（pose content 36 断言、观测链 11 行、generation entry 22 行、frame trace 3 例）。包 `build/behavior-graph-v22-alias-watch-20261027/test-package/`（manifest `c9878cc7…` 7,468 字节），ZIP 9,921,485 字节、SHA-256 `be5bd284…`，五条目重解压复核与 manifest 逐条相符。v21 三件套备份到 `dist/deploy-backup-20261027-v21/`（DLL `e0880651…`、PDB `787b3005…`、`off` INI 1,058 字节 `98a2d082…`），MO2 现为 v22 DLL/PDB，部署 INI=`inject`（1,056 字节 `e7ba0854…`）。**该轮不上机**；随后的一次运行见 [v22 运行记录](#g2-v22-run)。详见[构建记录](build.md#v22-alias-watch-2026-10-27)。

<a id="g2-v22-run"></a>
## G2 v22 运行记录（2026-10-27 23:03 会话，pose 内容证据成立）

构建 `CharacterPanel-scenegraph-behavior-graph-v22-alias-watch-20261027-22ef58464f-cl94faaed0c6-20261027T145636Z`（DLL `5756e0e6…`），部署 INI=`inject`，运行时 `1-6-1170-0`：23:01:47.893 加载（`SCENEGRAPH BUILD` 与部署身份逐字一致），23:03:47.702 开包，实验行 23:03:47.712–.724，23:03:54.845 关包（`DAC restore … result=matched`）。**无崩溃、无 `[E]`**；15 条 `[W]` 与 v20a/v21 同类（DAC/studio-light）。原始日志归档为 [CharacterPanel-g2v22-alias-watch-20261027-2303.log](diagnostics/CharacterPanel-g2v22-alias-watch-20261027-2303.log)（47,564 字节，SHA-256 `6C06C504BAC0CD66FE26BE49F62E15ABC13F192F45320BC8048B0E1361C334CD`）。

**预算**：`GRAPH ENTRY` 1、`GRAPH FRAME` 30+1、`BEHAVIOR GRAPH` 36、`INDEPENDENT GRAPH` 55（40 基础 + `pose-content` 3 + `pose-bone` 12），与 v21 逐项相同，未越界。

**观测**（检查点 0/60/120，均为 116 条）：

| 检查点 | finite | nonfinite | moved | max-delta | source-equal | **source-equal-moving** | source-equal-static | **alias** |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | 116 | 0 | n/a | n/a | 8 | n/a | n/a | no |
| 60 | 116 | 0 | 94 | 3.430847 | 21 | **0** | 21 | no |
| 120 | 116 | 0 | 94 | 2.313768 | 15 | **0** | 15 | no |

四根抽样骨：索引 0 `NPC Root [Root]`、115 `Belly` 三点恒等不动（`p=0`、`q=(0,0,0,1)`、`s=1`）；38 `NPC L Hand [LHnd]`、76 `NPC L Finger30 [LF30]` 在 step 60 与 120 均 `moved=yes` 且 `source-equal=false`。源侧 `source-finite=116 source-moved=0`、`source-pose-before=source-pose-after=d3f3eeb452316397`、`source-stable=true retained=true`、`world-restored=true ragdoll-removed=true`。

**预注册判读（v22，逐条，命中即停）**：

| 条 | 条件 | 本轮结果 |
| --- | --- | --- |
| 1 | 任一检查点 `nonfinite>0` 或骨行 `finite=false` | 未触发（三点 `nonfinite=0`，12 行全有限） |
| 2 | `entries=0` 或骨名不可读 | 未触发（`entries=116`，四骨名可读） |
| 3 | `alias=yes` → 真别名 | 未触发（三点 `no`） |
| 4 | `source-equal-moving>0` → 别名嫌疑 | 未触发（60/120 均为 0） |
| 5 | 三点有限 + `alias=no` + 60/120 `source-equal-moving=0` + `moved≥16` + 四骨两动 | **成立** |
| 6 | 全有限未别名但 `moved<16` | 不适用（`moved=94`） |
| 7 | 崩溃或源漂移 | 不适用（无崩溃，源哈希前后相等） |

**判定**：**v22 判据通过——pose 内容证据成立（有限、在变化、未别名）**。G2 仍 **NOT PASSED**：按上文「G2：原版暂停期单边推进」的原文，通过还要求“候选原版 idle **持续、有意义地**前进”，而本包只覆盖单个 120 步（2 秒模拟）批次内的 `poseLocal` 内容——没有跨批次持续性；没有语义对应证据（这些变化是否对应 `mt_idle` 的原版相位）；也没有场景应用与视觉确认（候选图仍未接到面板显示图）。装载图销毁/全局收敛同样未证。部署 INI 已回 `off`（1,061 字节 `cc0572ca…`），v22 DLL/PDB 留在原位，回滚备份 `dist/deploy-backup-20261027-v21/` 不变。

**这轮同时闭合了 v21 的误报审计**：`source-equal` 与 `source-equal-static` 逐点相等（21=21、15=15），`source-equal-moving=0`——被计入的条目**全部**是自 step 0 起未变化的常量骨。审计当时只能论证“恒定骨必然相等”，现在有了索引级计数证据：不是部分巧合，而是整个计数都由常量骨构成。

**第三份非别名证据**：源 pose 哈希三轮各不相同（`983fc2d4a169fa5b` / `bd107af608b00103` / `d3f3eeb452316397`），候选三点哈希三轮逐位相同（`bc160a8c2b3a5a58` / `63f638a6ac08d955` / `8fb4cd336c4395bf`），四骨数值同样逐位复现（例如索引 38 在 step 60 的 `q=(0.0439,0.1031,0.5867,-0.8021)`），而 step 0 的源 `p` 每轮不同（本轮 `(0.0000,-0.0439,15.6427)`，v21 为 `(0.0000,0.0663,15.9209)`）。候选内容与源内容无关，生成路径对同一输入确定性可复现。

**下一有界问题（2026-10-27 23:07 选定第 2 条）**：语义对应由 [v23](#g2-v23-contract) 预注册并实现。**更正本段原先的说法**：第 2 条并非“不新增原生调用”——它必须调用一次 `aAnimation::SamplePartialTracks` 才能取得参照采样，v23 契约已把这次 const 只读调用单列并说明理由。第 1 条（跨批次持续性）仍待后续。

<a id="g2-v23-contract"></a>
## G2 v23 契约：语义对应（2026-10-27，已实现/已部署/未运行）

**动机**：v22 证明 `poseLocal` 有限、在变化、未别名，但没有证明这些数值有语义——任何生成器写出像样的数字都能过那道门。G2 原文要求“有意义地前进”，本契约把它落成一个可判定的问题：**`poseLocal` 是不是某个活动片段在它自己时间上的采样？**

**方法（只读遍历 + 一次 const 采样调用）**：

1. 取 `candidate.characterInstance.behaviorGraph->activeNodes`（`hkArray<hkbNodeInfo>`，扫描前 256 项；运行期实测 97–125 项）。
2. 只把 `nodeClone` 的 vtable 等于 `RE::VTABLE_hkbClipGenerator[0].address()` 的节点当片段生成器；不匹配记 `no-clip-generator`，**绝不做类型转换**。
3. 读 `animationName` / `localTime` / `mode` / `playbackSpeed` / `binding`；`binding->animation` 与 `transformTrackToBoneIndices` 给出轨道→骨骼映射，越界索引记 `unmapped` 而非钳制。
4. 在 `localTime` 上调用一次 `SamplePartialTracks`，写进本函数的临时 `std::vector`，然后逐条与 `poseLocal[bone]` 比较。
5. 旋转用 `1-|dot|`（q 与 -q 同旋转，避免按分量比较的假失配），平移/缩放用分量差；ε 预注册为位置 1e-4、旋转 1e-5、缩放 1e-4。
6. 每个检查点最多 3 条 `clip-ref` 行（0/60/120 → ≤9 行），`status=unavailable` 单独一行说明原因。

**预注册判读（逐条，命中即停）**：

1. 全部检查点都是 `status=unavailable` → 记 `unavailable`，**不给结论**；下一问改为“pose 从哪来”（生成器输出/层混合），不得据此判失败。
2. 有 `status=compared` 但 `unmapped == tracks`（没有一条能映射到 `poseLocal`）→ 同样记 `unavailable`。
3. `compared>0` 且 **`agreed == compared`** → **完全对应**：`poseLocal` 就是该片段在该 `time` 上的采样；`name`/`time`/`duration` 同时给出片段身份与相位。
4. `agreed / compared >= 0.8` 且 `max-dpos <= 0.05`（模型单位）→ **主体对应**：绝大多数轨道一致，差异集中在少数骨，下一问是哪些骨被 IK/pose modifier 改写。
5. 低于第 4 条 → **不是单一片段的直接采样**：记为分层/混合或后处理结果，下一问转层分解，不得把 v22 的“在变化”升级为“有意义”。
6. 崩溃或源漂移 → 立即停止，保留完整日志与匹配 PDB，不在同会话调参重试。

**这是唯一新增原生调用的轮次**：此前各轮都保持“零新增原生调用”。`SamplePartialTracks` 是 `hkaAnimation` 的 const 虚函数，产品每帧采样显示播放器走的就是这条路径；它只写本函数的临时缓冲，不写 pose、不写图、不改剪辑时间。

**实现与部署（2026-10-27 23:14，当轮未上机）**：身份 `CharacterPanel-scenegraph-behavior-graph-v23-clip-ref-20261027-ddb58bc269-cl94faaed0c6-20261027T151451Z`（干净 `ddb58bc`，子模块 `94faaed0`，无 `+dirty`；DLL `efa90c21…` 1,277,440 字节、PDB `f7e3e2c6…` 35,696,640 字节、CodeView age 327）。新增 `clip_reference_policy.h`（旋转符号无关比较、位置/缩放分量差、聚合计数）与 19 断言 engine-free 用例；五个用例全过。两次编译修正：① `clip_reference_agrees` 触发 cl C4834（`[[nodiscard]]` 返回值被丢弃，`/WX` 下为错误），该函数本就以累积为目的，去掉该属性；② `clip->binding` 原先只判空就解引用——按 v20a 的同一规则补 `plausible_object_pointer` 过滤后才读 `animation`，二进制因此改变，身份以加固提交 `ddb58bc` 重新盖章。包 `build/behavior-graph-v23-clip-ref-20261027/test-package/`（manifest `2f143a77…` 7,373 字节），ZIP 9,928,402 字节、SHA-256 `05ac4a3e…`，五条目重解压复核与 manifest 逐条相符。v22 三件套备份到 `dist/deploy-backup-20261027-v22/`（DLL `5756e0e6…`、PDB `b763d05f…`、`off` INI 1,061 字节 `cc0572ca…`），MO2 现为 v23 DLL/PDB（与包内逐字节相同），部署 INI=`inject`（1,072 字节 `655a9c89…`）。**该轮不上机**；随后的一次运行待记录。

<a id="g2-v23-run"></a>
## G2 v23 运行记录（2026-10-27 23:19 会话，单一片段假设不成立，片段栈被定位）

构建 `CharacterPanel-scenegraph-behavior-graph-v23-clip-ref-20261027-ddb58bc269-cl94faaed0c6-20261027T151451Z`（DLL `efa90c21…`），部署 INI=`inject`，运行时 `1-6-1170-0`：23:18:44.765 加载（`SCENEGRAPH BUILD` 与部署身份逐字一致），23:19:45.963 开包，实验行 45.986–45.989，23:20:03.310 关包（`DAC restore … result=matched`）。**无崩溃、无 `[E]`**；15 条 `[W]` 与既往同类（DAC/studio-light）。原始日志归档为 [CharacterPanel-g2v23-clip-ref-20261027-2319.log](diagnostics/CharacterPanel-g2v23-clip-ref-20261027-2319.log)（50,159 字节，SHA-256 `9452BF96FDED6EEBD2EC7CC3A61D022C6189E970C78B96F15F55CF61D797999B`）。

**预算**：`GRAPH ENTRY` 1、`GRAPH FRAME` 30+1、`BEHAVIOR GRAPH` 36、`INDEPENDENT GRAPH` 63（40 基础 + `pose-content` 3 + `pose-bone` 12 + `clip-ref` 8），`clip-ref` 8 条 ≤ 预注册的 9 条上限，未越界。v22 门槛在本轮逐字复现：三点 `finite=116 nonfinite=0`、`moved=94`、`alias=no`、`source-equal-moving=0`，候选三点哈希与前三轮逐位相同（`bc160a8c` / `63f638a6` / `8fb4cd33`），源哈希第四次不同（`8bdaf4f8…`）。

**观测（全部 8 条 `clip-ref`）**：

| 检查点 | slot | index | 片段 | time | duration | mode | tracks | mapped | compared | agreed | max-dpos | max-drot | max-dscale |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | 0 | 0 | `Animations\H2H_Equip.hkx` | 0.0167 | 0.8333 | 0 | 99 | **0** | 0 | 0 | — | — | — |
| 0 | 1 | 14 | `Animations\male\MT_Idle.HKX` | 0.0167 | 25.0167 | 1 | 99 | 99 | 99 | 22 | 3.983129 | 0.063770 | 0.151933 |
| 60 | 0 | 0 | `Animations\H2H_Idle.hkx` | 0.1833 | 8.0000 | 1 | 99 | **0** | 0 | 0 | — | — | — |
| 60 | 1 | 3 | `Animations\1HM_Idle.hkx` | 0.1833 | 9.0000 | 1 | 99 | **0** | 0 | 0 | — | — | — |
| 60 | 2 | 25 | `Animations\SCAR_1hmReadyDummy.hkx` | 0.1833 | 6.3333 | 0 | 97 | **0** | 0 | 0 | — | — | — |
| 120 | 0 | 0 | `Animations\H2H_Idle.hkx` | 1.2000 | 8.0000 | 1 | 99 | **0** | 0 | 0 | — | — | — |
| 120 | 1 | 3 | `Animations\1HM_Idle.hkx` | 1.2000 | 9.0000 | 1 | 99 | **0** | 0 | 0 | — | — | — |
| 120 | 2 | 25 | `Animations\SCAR_1hmReadyDummy.hkx` | 1.2000 | 6.3333 | 0 | 97 | **0** | 0 | 0 | — | — | — |

**预注册判读（逐条）**：

| 条 | 条件 | 本轮结果 |
| --- | --- | --- |
| 1 | 全部 `unavailable` | 未触发（8 条全是 `status=compared`） |
| 2 | `unmapped == tracks` | **未覆盖**：实际是 `mapped=0` 且 `unmapped=0`（映射数组为空），契约只预注册了“有映射但全越界” |
| 3 | `agreed == compared` | 未触发（唯一 `compared>0` 的点是 22/99） |
| 4 | `>=0.8` 且 `max-dpos<=0.05` | 未触发（0.222、3.983129） |
| 5 | 低于第 4 条 | **命中** |
| 6 | 崩溃/源漂移 | 不适用 |

**判定**：**第 5 条命中——pose 不是单一片段的直接采样**；G2 的“有意义”**仍未成立**，v22 的“在变化”不得升级。同时 60/120 两点因 `mapped=0` 未做成任何比较，属**契约未覆盖状态**，不能当作“不匹配”的证据；唯一做成比较的 step 0 那一刻 pose 还是绑定姿态（`candidate-pose-0=bc160a8c`，生成前），22/99 也不构成对片段的否定。

**本轮真正的收获是结构性的**（不属于预注册判据，只作为下一问的输入）：

1. **候选同时挂着多个片段生成器**。step 0 两个（`H2H_Equip` + `male\MT_Idle.HKX`），step 60/120 三个（`H2H_Idle` + `1HM_Idle` + `SCAR_1hmReadyDummy`）。姿态是栈的产物，不是单片段——这解释了为什么单片段比较必然失败。
2. **集合与装备态相符**。`1HM_Idle` + `SCAR_1hmReadyDummy` 正是单手武器待机 + ready 过渡占位，而 step 0 的 `H2H_Equip` 与 `MT_Idle` 是进面板前的空手姿态。这是第一次看到候选图自己在做装备相关的片段选择。
3. **三条同步**：60 与 120 上三条的 `localTime` 完全相同（0.1833 / 1.2000），而时长各异（8.0 / 9.0 / 6.33）——同一个时钟驱动的层，不是三个独立播放器。
4. **片段时钟与驱动不同步**：0.0167 → 0.1833 → 1.2000。120 步（2.0 s 模拟）片段只走了 1.2 s，且前 60 步只走了 0.1667 s。若 G2 的“持续”以片段时钟衡量，这是需要解释的反例，不能默认它跟随。
5. `mapped=0` 的原因是那些 binding 的 `transformTrackToBoneIndices` 大小为 0。Havok 里空映射通常表示 identity（track i → bone i），引擎自己的采样器（63581/57148）必须能处理它，否则这些片段不会动——**但这只是假设，未审计**；未审计就加 identity 回退等于猜。

**下一有界问题（2026-10-27 23:29 立项为 [v24](#g2-v24-contract)）**：空映射语义已由 [audit25](behavior-graph-construction-audit.md#audit25) 在指令层回答（identity），层普查与时钟曲线进 v24；identity 成立后逐层比较才谈得上“姿态 = 哪几层的组合”，而“有意义”按单层还是加权混合判，必须在新契约里预注册。

<a id="g2-v24-contract"></a>
## G2 v24 契约：空映射语义 + 层普查 + 片段时钟（2026-10-27，已实现/待构建）

**动机**：v23 只给出一个负结论（单一片段不成立）和一个结构事实（多片段栈），而 60/120 两点根本没做成比较——那些 binding 的映射数组为空。v24 一次把三件事做掉：确认空映射语义、拿完整层清单、把片段时钟从三个点变成曲线。

**依据（第一部分已在审计里完成）**：[audit25](behavior-graph-construction-audit.md#audit25) 用 AE 1.6.1170 的 `ConvertTrackPoseToBonePose`（REL 60005，`0xB03380`）指令级确认：`map.size()==0` 时 `bone = track`（`0xB03405 movzx esi, bx`），非空时 `bone = map[track]`（`0xB03411`）。identity 因此不是假设，是引擎在该转 pose 助手里的行为。

**变更（仍只有一次 const 采样调用）**：

1. `clip-ref` 采用该语义：新增 `mapping=identity|table`、`binding-index`（`hkbClipGenerator::animationBindingIndex`）与 `map-len`；空映射时按轨道号比较，不再跳过。
2. 槽位上限 3 → 8，拿到完整层清单；每检查点 ≤8 行。
3. 新增 `clip-clock`：step 0/20/40/60/80/100/120 各 1 行，列出每个活动片段的短名与 `localTime`，把“片段时钟 vs 驱动”变成曲线而不是三点。

**预算**：`clip-ref` ≤8×3 = 24 行；`clip-clock` 7 行；合计新增 ≤31 行，与 `GRAPH FRAME` 30+1、`INDEPENDENT GRAPH` 40、`BEHAVIOR GRAPH` 36 及既有 pose 行分账。

**预注册判读（逐条，命中即停）**：

1. 三个检查点都 `status=unavailable` → 记 `unavailable`，不给结论。
2. 有行为 `mapping=identity` 却仍 `mapped=0` → 与 audit25 冲突（说明这些 binding 运行时并非空，或读取位置不同）：停止并复查读取位置，**不得改判据绕开**。
3. 至少一层 `agreed / compared >= 0.8` → 该层就是这些骨的姿态来源，记名字与 `binding-index`；“有意义”按“层可归属”成立。
4. 多层各自覆盖不同骨且均 ≥0.8 → 记“互补栈”，同样算层可归属。
5. 没有任何一层 ≥0.5 → 姿态是混合/后处理结果，不可按层归属；下一问转向权重与 pose modifier，**不得**把 v22 的“在变化”升级为“有意义”。
6. 崩溃或源漂移 → 停止，保留完整日志与匹配 PDB。

**时钟判读（单列，不参与上面六条）**：把 `clip-clock` 的 `localTime` 与驱动步数对照。若曲线严格单调但斜率 < 1（v23 三点给出的比值约 0.6），记“片段时钟落后于驱动”；若中途停滞，记停滞区间，作为 G2“持续”的明确缺口。**这一条只记录观测，不预先判定谁对。**

<a id="g2-v24-build"></a>
**实现与部署（2026-10-27 23:35，当轮未上机）**：身份 `CharacterPanel-scenegraph-behavior-graph-v24-layer-census-20261027-50c2396d66-cl94faaed0c6-20261027T153521Z`（干净 `50c2396`，子模块 `94faaed0`，无 `+dirty`；DLL `13bc2b12…` 1,282,560 字节、PDB `c4a72f08…` 35,770,368 字节、CodeView age 329）。只改 `independent_graph.cpp`：扫描抽成 `collect_active_clips`、槽位 3→8、`map.empty()` 时 `bone = track`、两行模板各加字段、新增 `log_clip_clock` 并在 `step % 20 == 0` 与 120 处调用；`clip_reference_policy.h` 未改，五个 engine-free 用例全过。包 `build/behavior-graph-v24-layer-census-20261027/test-package/`（manifest `7e47d6fc…` 6,268 字节），ZIP 10,015,469 字节、SHA-256 `68991ca7…`，五条目重解压复核与 manifest 逐条相符。v23 三件套备份到 `dist/deploy-backup-20261027-v23/`（DLL `efa90c21…`、PDB `f7e3e2c6…`、`off` INI 1,077 字节 `02f37ace…`），MO2 现为 v24 DLL/PDB（与包内相同），部署 INI=`inject`（1,062 字节 `ac255097…`）。**该轮不上机**；随后的一次运行待记录。

<a id="g2-v24-run"></a>
## G2 v24 运行记录（2026-10-27 23:48 会话，层可归属成立）

构建 `CharacterPanel-scenegraph-behavior-graph-v24-layer-census-20261027-50c2396d66-cl94faaed0c6-20261027T153521Z`（DLL `13bc2b12…`），部署 INI=`inject`，运行时 `1-6-1170-0`：23:48:02.887 加载（`SCENEGRAPH BUILD` 与部署身份逐字一致）。**无崩溃、无 `[E]`**；15 条 `[W]` 与既往同类。原始日志归档为 [CharacterPanel-g2v24-layer-census-20261027-2348.log](diagnostics/CharacterPanel-g2v24-layer-census-20261027-2348.log)（53,932 字节，SHA-256 `58276FCA6DFD742E58DF410FA4B935C44B3EB8CEF5424A39282B993BDCB568EEA`）。

**预算**：`GRAPH ENTRY` 1、`GRAPH FRAME` 30+1、`BEHAVIOR GRAPH` 36、`INDEPENDENT GRAPH` 71（40 基础 + `pose-content` 3 + `pose-bone` 12 + `clip-ref` 9 + `clip-clock` 7），均在预注册上限内（`clip-ref` ≤24、`clip-clock` 7）。

**层普查（9 条 `clip-ref`，比例 = agreed/compared）**：

| step | slot | 片段 | binding-index | map-len | mapping | tracks | compared | agreed | 比例 | max-dpos | max-drot |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | 0 | `H2H_Equip.hkx` | 11045 | 0 | identity | 99 | 99 | 36 | 0.364 | 2.263 | 0.0531 |
| 0 | 1 | `male\MT_Idle.HKX` | 1022 | 99 | table | 99 | 99 | 22 | 0.222 | 3.983 | 0.0638 |
| 60 | 0 | `H2H_Idle.hkx` | 681 | 0 | identity | 99 | 99 | 46 | 0.465 | 0.497 | 0.00035 |
| 60 | 1 | `1HM_Idle.hkx` | 23 | 0 | identity | 99 | 99 | 19 | 0.192 | 3.107 | 0.2544 |
| 60 | 2 | `SCAR_1hmReadyDummy.hkx` | 15074 | 0 | identity | 97 | 97 | 16 | 0.165 | 17.321 | 0.2544 |
| 60 | 3 | `H2H_Equip.hkx` | 11045 | 0 | identity | 99 | 99 | 19 | 0.192 | 5.477 | 0.0562 |
| 120 | 0 | `H2H_Idle.hkx` | 681 | 0 | identity | 99 | 99 | **90** | **0.909** | 0.187 | **0.000061** |
| 120 | 1 | `1HM_Idle.hkx` | 23 | 0 | identity | 99 | 99 | 23 | 0.232 | 3.378 | 0.2684 |
| 120 | 2 | `SCAR_1hmReadyDummy.hkx` | 15074 | 0 | identity | 97 | 97 | 15 | 0.155 | 100.658 | 0.7345 |

**时钟曲线（7 条 `clip-clock`）**：

| step | found | entries |
| --- | --- | --- |
| 0 | 2 | `H2H_Equip:0.0167`, `MT_Idle:0.0167` |
| 20 | 1 | `H2H_Equip:0.4583` |
| 40 | 1 | `H2H_Equip:0.7083` |
| 60 | 4 | `H2H_Idle:0.1833`, `1HM_Idle:0.1833`, `SCAR:0.1833`, `H2H_Equip:0.8333` |
| 80 | 3 | idle 三条均 `0.7000` |
| 100 | 3 | idle 三条均 `0.9500` |
| 120 | 3 | idle 三条均 `1.2000` |

**预注册判读（逐条）**：

| 条 | 条件 | 本轮结果 |
| --- | --- | --- |
| 1 | 三点全 `unavailable` | 未触发（9 条 `status=compared`） |
| 2 | `mapping=identity` 却 `mapped=0` | **未发生**：所有 identity 行 `mapped=99`（SCAR 97）、`compared` 满额 → [audit25](behavior-graph-construction-audit.md#audit25) 的语义在运行期成立 |
| 3 | 至少一层 `agreed/compared ≥ 0.8` | **命中**：`H2H_Idle` 90/99 = 0.909（binding-index 681） |
| 4 | 多层互补且均 ≥0.8 | 不适用（只有一层达标） |
| 5 | 无任何层 ≥0.5 | 不适用 |
| 6 | 崩溃/源漂移 | 不适用 |

**判定**：**第 3 条命中——层可归属成立，G2 的“有意义”取得数据**。step 120 的 `poseLocal` 有 90/99 条轨道就是 `H2H_Idle` 在该时刻（`time=1.2000`）的采样；而且**全部 99 条的旋转误差 ≤ 6.1e-5**，差异只出现在 9 条的位置分量上（最大 0.187 个模型单位），形态指向这 9 条被额外的平移层或提取运动改写，而不是姿态本身不同。其余层在同一时刻的比例只有 0.155–0.232，`SCAR_1hmReadyDummy` 的 `max-dpos=100.658` 说明它是局部占位片段而非全身片段，不能拿来论证“加权混合”。

G2 的四项要件因此变成：pose 内容（v22 成立）、必要骨变化（v22 成立）、源对照（每轮成立）、**有意义（本轮成立，层可归属）**；**只剩“持续”没有定论**，而本轮第一次给出了它的量化形态：

1. **驱动中途栈发生切换**：step 20/40 只有 `H2H_Equip`（`mode=0` 单次播放），step 60 起 idle 栈接管，`H2H_Equip` 停在自己的 `duration=0.8333`——符合单次播放语义，不是停滞。
2. **idle 栈时钟单调推进，但步长不是常数**：`0.1833`(60) → `0.7000`(80) → `0.9500`(100) → `1.2000`(120)。60→80 是 31 帧 / 20 步（1.55×），80→100 与 100→120 各 15 帧 / 20 步（0.75×）。
3. 也就是说，**每驱动步的推进量在 0.75× 与 1.55× 之间**，与我们喂的 `dt=1/60`（1×）不符。“持续”目前唯一的缺口就是这个比例。

**下一有界问题（2026-10-27 23:53 做掉第 1 条）**：[audit26](behavior-graph-construction-audit.md#audit26) 已把时钟的算式读出来——`new = old + dt × ((实时长/enforcedDuration)×playbackSpeed)`，再按 `[cropStart, cropStart+实时长]` 钳制，**只消费 dt，没有帧计数**。所以 0.75×–1.55× 不可能来自时钟自身（checkpoint 上 `playbackSpeed` 全是 1.000），只能来自“到达 Update 的 dt 不均匀”。剩余问题是**哪些驱动步漏了**，需要逐步记录才能定位；第 2 条（dt 缩放运行）暂时不需要，因为算法已经证明它对 dt 是线性的。

<a id="g2-v25-contract"></a>
## G2 v25 契约：逐步时钟（2026-10-27，已实现/待构建）

**动机**：[audit26](behavior-graph-construction-audit.md#audit26) 已证明时钟只消费 dt——`new = old + dt × speed`，`speed = (effDuration / enforcedDuration 或 1) × playbackSpeed`，再按 `[cropStart, cropStart + effDuration]` 钳制。v24 看到的 0.75×–1.55× 因此只剩两种可能：①部分驱动步没走到该片段的 Update；②那些步的 dt 不同，或 speed 不是 1（`enforcedDuration` 与裁剪量当时没记录，不能排除常数倍率）。v25 把这两条都变成可读数据。

**变更（仍只有一次 const 采样调用）**：

1. `clip-ref` 增三个字段：`enforced=`（`hkbClipGenerator::enforcedDuration`，`+0x68`）、`crop-sum=`（`hkaDefaultAnimationControl::cropStartAmountLocalTime + cropEndAmountLocalTime`，`+0x9C`/`+0xA0`）、`eff-duration=`（`duration − crop-sum`，与 `0xACF340` 同式，向下钳零）。用来排除“常数倍率”。
2. 新增 `clip-steps`：每个片段一行，每个驱动步一个字符，记录该步 `localTime` 的增量（四舍五入到 1/60 帧）。编码 `.`=0、`1`–`9`=1–9 帧、`A`–`Z`=10–35 帧、`N`=回退、`-`=该步不在活动集。行内带 `first=`（首次出现的步号）与 `steps=`（字符数）。
3. `clip-clock`（7 行）保留不变，作为人读对照。

**预算**：`clip-steps` ≤8 行；`clip-ref` 行数与上限不变（≤24）。新增 ≤8 行。

**预注册判读（逐条）**：

1. 某片段位串出现**周期缺口**（例如每 4 步一个 `.`）→ 那些步没有执行该节点的 Update；下一问是 63587 的节点门（`[node+0x30] flags&1`、`[entry+0x85]`）为什么对活动片段翻假。
2. 位串**没有缺口但每步增量不是 1 帧**（如 `0`/`1` 交替、或小数被四舍五入成 0.75 无法整除的模式）→ dt 本身不是 1/60，或 speed ≠ 1；由 `enforced` / `crop-sum` / `eff-duration` 判定是哪一项。
3. 位串出现 `N`（回退）→ 片段被重启或循环回绕；记录回绕周期，不判失败。
4. 位串在激活后**全程 `.`**（零推进）→ 与 v24 的 `clip-clock` 冲突，停止并复查读取位置。
5. 崩溃或源漂移 → 停止，保留完整日志与匹配 PDB。

**判定口径**：本轮只回答“0.75× 从哪来”，**不改变 G2 的通过标准**；“持续”是否成立仍按原句（候选原版 idle 持续、有意义地前进）在拿到位串之后由下一轮判。

<a id="g2-v25-build"></a>
**实现与部署（2026-10-27 23:58，当轮未上机）**：身份 `CharacterPanel-scenegraph-behavior-graph-v25-step-clock-20261027-862cebdf1b-cl94faaed0c6-20261027T155811Z`（干净 `862cebd`，子模块 `94faaed0`，无 `+dirty`；DLL `d8ca80bb…` 1,290,752 字节、PDB `a468bead…` 35,860,480 字节、CodeView age 331）。只改 `independent_graph.cpp`：`clip-ref` 增 `enforced`/`crop-sum`/`eff-duration`（动画控制指针同样先过 `plausible_object_pointer`），新增 `ClipStepTrace` 与 `log_clip_step`/`emit_clip_steps`（每步末尾采样、120 处补一点、循环结束后逐片段输出位串）；`clip_reference_policy.h` 未改，五个 engine-free 用例全过。包 `build/behavior-graph-v25-step-clock-20261027/test-package/`（manifest `ba4bc726…` 5,571 字节），ZIP 10,029,342 字节、SHA-256 `15820120…`，五条目重解压复核与 manifest 逐条相符。v24 三件套备份到 `dist/deploy-backup-20261027-v24/`（DLL `13bc2b12…`、PDB `c4a72f08…`、`off` INI 1,046 字节 `1424473c…`），MO2 现为 v25 DLL/PDB（与包内相同），部署 INI=`inject`（1,057 字节 `4af7e8ee…`）。**该轮不上机**；随后的一次运行待记录。

<a id="g2-v25-run"></a>
## G2 v25 运行记录（2026-10-28 00:03 会话，时钟速率 1.000×，v24 的“落后”是采样假象）

构建 `CharacterPanel-scenegraph-behavior-graph-v25-step-clock-20261027-862cebdf1b-cl94faaed0c6-20261027T155811Z`（DLL `d8ca80bb…`），部署 INI=`inject`，运行时 `1-6-1170-0`：00:03:04.761 加载（`SCENEGRAPH BUILD` 与部署身份逐字一致）。**无崩溃、无 `[E]`**；15 条 `[W]` 与既往同类。原始日志归档为 [CharacterPanel-g2v25-step-clock-20261028-0003.log](diagnostics/CharacterPanel-g2v25-step-clock-20261028-0003.log)（55,345 字节，SHA-256 `FFB672DE109B9DA53B1B74DD889C60CE5AA2733E5BDF1FAE9B9FDBD44E419D35`）。

**预算**：`GRAPH ENTRY` 1、`GRAPH FRAME` 30+1、`BEHAVIOR GRAPH` 36、`INDEPENDENT GRAPH` 76（40 基础 + `pose-content` 3 + `pose-bone` 12 + `clip-ref` 9 + `clip-clock` 7 + `clip-steps` 5），`clip-steps` 5 条 ≤ 上限 8。

**位串解码**（`.`=0 帧，`1`–`9`=1–9 帧，`A`–`Z`=10–35 帧，`-`=缺席）：

| 片段 | first | steps | 总帧数 | 速率 | 游程结构 |
| --- | --- | --- | --- | --- | --- |
| `H2H_Idle.hkx` | 49 | 72 | 72 | **1.000×** | `.`×1, `1`×12, `F`(15)×1, `.`×14, `F`×1, `.`×14, `F`×1, `.`×14, `F`×1, `.`×13 |
| `1HM_Idle.hkx` | 49 | 72 | 72 | **1.000×** | 与上一行逐字符相同 |
| `SCAR_1hmReadyDummy.hkx` | 49 | 72 | 72 | **1.000×** | 与上一行逐字符相同 |
| `H2H_Equip.hkx` | 0 | 61 | 49 | — | `.`×1, `C`(12)×1, `.`×10, `F`×1, `.`×14, `F`×1, `.`×14, `7`(7)×1，其后缺席 |
| `MT_Idle.HKX` | 0 | 1 | 0 | — | 第 0 步之后缺席，全程未推进 |

**判定**：

1. **速率是 1.000×，不是 0.75×。** 三条 idle 片段在 72 个驱动步里正好推进 72 帧。`H2H_Equip` 同样自洽：第 0 步已在第 1 帧，进程内再走 49 帧 = 50 帧 = 它自己的 `duration=0.8333`（50/60），到点即停，符合 `mode=0`。`MT_Idle` 从未推进、第 0 步后就不在活动集里。
2. **v24 的“每步 0.75×–1.55×”是采样假象，本轮推翻。** v24 用 20 步窗口采样，而交付周期是 15 步：窗口里落进 1 个块就是 15/20 = 0.75×，落进 2 个块就是 30/20 = 1.5×（实测 31 帧那次含了首块余下的 1 帧）。**“时钟落后”的结论作废**；v24 的记录按原样保留，不改写当时的读数，只在此处标注后验更正。
3. **但交付是成批的。** 三条 idle 片段先是 12 步逐帧推进，随后改成“每 15 步一次性推进 15 帧”，中间 14 步为 0；三条逐字符相同，说明它们共享同一个批处理时钟。`H2H_Equip` 的块间隔同样是 15 步（首块 12 帧是它起步阶段的半拍）。
4. **预注册第 1、2 条都没有干净命中**：不是“缺口丢帧”（块内把帧补齐了，总量对得上），也不是“无缺口但非 1 帧”。这是第三类：**总量正确、交付成批**。第 1 条推论的“那些步没有执行 Update”因此不成立——帧没有丢，只是被攒起来了。

**对 G2 的影响**：`持续`在**时钟**层面成立（1:1、无停滞、无漂移、无累积误差）；但**可见的持续性未证**——若 `localTime` 只在批边界刷新，`poseLocal` 也可能每 15 步才变一次，那面板里的 idle 就是 4 Hz 的跳帧而不是连续运动。v22 只比了 0/60/120 三个点，没有逐步 pose 证据。G2 的四项要件里，姿态内容（v22）、必要骨变化（v22）、语义归属（v24）、源对照（每轮）、时钟速率（本轮）都有数据，**只剩 pose 级的逐步持续性**。

**下一有界问题（v26，待评审，未预注册）**：per-step pose 哈希——复用同一条位串机制，逐步记录 `poseLocal` 是否变化（以及变化量档位），回答“pose 是每步都在变，还是跟随时钟每 15 步跳一次”。这是 G2“持续”的最后一块，也是判断“这个 idle 能不能直接拿去显示”的依据。

<a id="g2-v26-contract"></a>
## G2 v26 契约：逐步 pose（2026-10-28，已实现/待构建）

**动机**：v25 证明 idle 时钟速率 1.000×，但交付是成批的——每 15 个驱动步一次性推进 15 帧，中间 14 步为 0。如果 `poseLocal` 跟随时钟，面板里的 idle 实际就是 **4 Hz 跳帧**；如果 pose 每步都在变，成批只影响时钟读数、不影响可见连续性。G2 的“持续”只差这一项。

**观测（只读，不新增原生调用）**：在每步末尾（与 `clip-steps` 同一采样点）读一次 `poseLocal`，与上一步逐条比较 translation/rotation/scale 十个有意义分量的最大绝对差，按档位写一个字符：

| 字符 | 最大分量差 |
| --- | --- |
| `.` | 0（没有任何条目变化） |
| `1` | ≤ 1e-5 |
| `2` | ≤ 1e-4 |
| `3` | ≤ 1e-3 |
| `4` | ≤ 1e-2 |
| `5` | > 1e-2 |

行格式：`phase=pose-steps first=<n> steps=<n> changed=<n> max-delta=<x> deltas='…'`，每次请求 1 条。

**预算**：`pose-steps` 1 行；其余行数与上限不变。

**预注册判读（逐条，命中即停）**：

1. `changed / steps ≥ 0.9` 且档位普遍 ≥ `2` → pose **每步都在变**，可见连续性成立；“持续”在 pose 层满足（场景应用与视觉确认仍属他层）。
2. 变化只出现在批次边界（例如每 15 步一个非 `.`、其余为 `.`），`changed/steps ≈ 1/15` → pose 严格跟随时钟的批处理：**可见连续性不成立**，G2 的“持续”得到一个具体阻断项（需要逐帧驱动或改交付粒度），不得算作通过。
3. `changed = 0` → 与 v22（60/120 两点各有 94 条在变）冲突，停止并复查读取位置。
4. 档位 `5`（>1e-2）连续出现 → 记为跳变，单独核对是否与批次边界对齐。
5. 崩溃或源漂移 → 停止，保留完整日志与匹配 PDB。

<a id="g2-v26-build"></a>
**实现与部署（2026-10-28 00:09，当轮未上机）**：身份 `CharacterPanel-scenegraph-behavior-graph-v26-pose-step-20261028-aed15ad6c0-cl94faaed0c6-20261027T160910Z`（干净 `aed15ad`，子模块 `94faaed0`，无 `+dirty`；DLL `6c117d47…` 1,294,848 字节、PDB `9203291b…` 35,909,632 字节、CodeView age 333）。只改 `independent_graph.cpp`：新增 `log_pose_step`/`emit_pose_steps`（每步末尾读 `poseLocal`、与上一步取十个分量的最大绝对差、按六档写一个字符），在每步与 120 处调用、循环结束后输出；`pose-steps` 1 行。包 `build/behavior-graph-v26-pose-step-20261028/test-package/`（manifest `a02e25c1…` 4,824 字节），ZIP 10,029,787 字节、SHA-256 `d3d7f1b7…`，五条目重解压复核与 manifest 逐条相符。v25 三件套备份到 `dist/deploy-backup-20261027-v25/`（DLL `d8ca80bb…`、PDB `a468bead…`、`off` INI 1,063 字节 `e910d4e8…`），MO2 现为 v26 DLL/PDB（与包内相同），部署 INI=`inject`（1,042 字节 `29692657…`）。**该轮不上机**；随后的一次运行待记录。

<a id="g2-v26-run"></a>
## G2 v26 运行记录（2026-10-28 00:12，历史 raw 判据通过；语义连续性待复验）

> v32 后验更正：本节原始读数和当时判定保留。raw 分量的 119/120 可由静止旋转的 q/-q 表示翻转复现；这不证明该次游戏姿态静止，但撤回仅凭 raw 数值确认语义连续性的推论。有限值、私有缓冲、层归属和时钟证据不因此作废。

构建 `CharacterPanel-scenegraph-behavior-graph-v26-pose-step-20261028-aed15ad6c0-cl94faaed0c6-20261027T160910Z`（DLL `6c117d47…`），部署 INI=`inject`，运行时 `1-6-1170-0`：00:12:57.721 加载（`SCENEGRAPH BUILD` 与部署身份逐字一致）。**无崩溃、无 `[E]`**；15 条 `[W]` 与既往同类。原始日志归档为 [CharacterPanel-g2v26-pose-step-20261028-0012.log](diagnostics/CharacterPanel-g2v26-pose-step-20261028-0012.log)（58,751 字节，SHA-256 `D90DCD777A74C5E51FCF96B22147BAFB0AA9011699D72F76EA36DE5D159007D1`）。

**预算**：`GRAPH ENTRY` 1、`GRAPH FRAME` 30+1、`BEHAVIOR GRAPH` 36、`INDEPENDENT GRAPH` 77（40 基础 + `pose-content` 3 + `pose-bone` 12 + `clip-ref` 9 + `clip-clock` 7 + `clip-steps` 5 + `pose-steps` 1）。

**观测**：

```
phase=pose-steps first=0 steps=121 changed=119 max-delta=2.000000
deltas='.55555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555.'
```

同轮其它判据逐字复现（v22 pose 内容：三点 `finite=116 nonfinite=0`、`moved=94`、`alias=no`、`source-equal-moving=0`；候选三点哈希与起前四轮逐位相同 `bc160a8c`/`63f638a6`/`8fb4cd33`；v25 时钟位串与前一轮逐字符相同）。

**预注册判读**：

| 条 | 条件 | 本轮结果 |
| --- | --- | --- |
| 1 | `changed/steps ≥ 0.9` 且档位普遍 ≥2 | **命中**：119/120 = 0.992，除首尾两点外全部是 `5`（>1e-2） |
| 2 | 变化只在批次边界（≈1/15） | 未发生（真实值 0.992，远高于 1/15） |
| 3 | `changed = 0` | 未发生 |
| 4 | 档位 `5` 连续出现 | 出现且连续——但它就是判据 1 所要求的“每步都在变”，不是跳变；`max-delta=2.0` 由 step 0→1 的绑定姿态→首帧过渡贡献 |
| 5 | 崩溃/源漂移 | 未发生 |

**当时按 raw 判据判定第 1 条命中。** v32 复核表明该读数本身不足以证明语义或可见连续性，当前需补符号不变的运动统计与画面。

**当时的机制推断（不作为当前前提）**：v25 的 localTime 成批发布和 v26 raw 分量变化曾被解释为两套采样时间。v32 证明 raw 变化也可来自 q/-q 表示翻转，因此不能凭这两项读数唯一确定姿态实际采样时钟；原时钟/位串保留，候选逐步语义连续性需新统计。

**当时 G2 判定：PASSED（历史 raw 判据，范围如下；当前语义连续性待复验）**

| 要件 | 证据 | 结论 |
| --- | --- | --- |
| 候选原版 idle 持续 | pose 每步变（119/120，档位全 `5`）；时钟速率 1.000× | ✓ |
| 有意义地前进 | `H2H_Idle`（binding-index 681）在 step 120 覆盖 90/99 条轨道、旋转误差 ≤6.1e-5 | ✓ |
| 必要骨的有效变换变化可验证 | LHnd/LF30 在 60 与 120 均 `moved=yes`，90 条轨道在变 | ✓ |
| 源保持对照 | `source-stable=true`、源 pose 前后相等、`source-drift=none` ×3 | ✓ |

**范围限定（不因 G2 通过而解除）**：单进程、单次暂停批次 120 步（2 秒模拟）、AE 1.6.1170、入口 `63589(mode0)`；**不含**场景应用与视觉确认（候选图仍未接到面板显示图）、装载图生命周期/销毁、全局收敛、G3–G6 与独立 OAR 语境。按本文 G2 段落原文“装载图生命周期与后续关卡不由此通过”，这些仍是各自的关口。

**下一有界问题（G3 前置，待评审）**：把候选 pose 接到显示图，或先在候选图上做 Equip→Idle→Unequip 的原生事件链（G3）。两者都需要新的契约；当前部署已回 `off`。

<a id="g2-v27-contract"></a>
## G2 v27 契约：把候选姿态接到显示图（2026-10-28，已实现/待构建）

**动机**：G2 的全部证据都来自缓冲，没有一条来自屏幕。v27 把保留候选的 `poseLocal` 转成（骨名 → 局部变换），交给面板现有的 name→node 映射去应用——复用 `AnimationController::advance()` 已有的落盘循环与 `recompute_subtree_worlds`，不另开渲染路径。

**变更（不新增原生调用）**：

1. 实验结束时把保留候选的 `hkbCharacter*` 记进模块静态（只在 `retained_loaded` 时），因为那是显示路径唯一能拿到的句柄。
2. 新增只读 `held_display_pose(out)`：读 `poseLocal`，按 `setup->animationSkeleton->bones[i].name` 生成 `(骨名, NiTransform)`；骨架缺失、姿态为空或名称为空则跳过该条。
3. 新增 INI 键 `[Experiment] DisplayPose=false`。为 true 且存在保留候选时，`advance()` 用该姿态替代 OAR 采样结果，之后仍走原来的 apply 循环与 world 级联。
4. 首次生效打 1 行：`ANIMATION DISPLAY source=independent-graph bones=<n> note=frozen-at-last-driven-step`。

**范围**：v27 只做**静态接驳**——姿态冻结在驱动结束那一帧，不逐帧推进。目的是先确认“候选姿态能被显示图正确应用”（视觉证据的入口）；逐帧动起来是下一轮的事，需要把逐步驱动搬进渲染路径，风险高一个量级。

**回退**：`DisplayPose=false` 立即恢复原 OAR 路径；候选未保留时该分支直接跳过、零副作用。

**预注册判读（逐条）**：

1. 出现 `ANIMATION DISPLAY … bones≈116`，且画面里的角色是候选姿态（应与 v24 的 `H2H_Idle`、`time=1.2` 一致）→ 接驳成立。
2. 没有该行 → 候选未保留或 `held_display_pose` 返回空，记 `unavailable`，不改判据。
3. 画面与 OAR 姿态看不出差别 → 骨名没命中（`state.nodes` 的键与骨架名不一致），下一问查映射，不算失败。
4. 崩溃 → 回 `off`，保留完整日志与匹配 PDB。

<a id="g2-v27-build"></a>
**实现与部署（2026-10-28 00:20，当轮未上机）**：身份 `CharacterPanel-scenegraph-behavior-graph-v27-display-pose-20261028-c894c956f8-cl94faaed0c6-20261027T162057Z`（干净 `c894c95`，子模块 `94faaed0`，无 `+dirty`；DLL `b973aa5b…` 1,298,944 字节、PDB `75a228f5…` 35,926,016 字节、CodeView age 335）。改动四处：`independent_graph.{h,cpp}` 记录保留候选的 `hkbCharacter*` 并实现 `held_display_pose`、`settings.{h,cpp}` 增 `DisplayPose`、`controller.cpp` 在 `advance()` 开头接驳。首次编译在 `candidate` 越出作用域处失败（`C2065`），改为在泄漏点捕获 `retained_character` 后通过。五个 engine-free 用例全过。包 `build/behavior-graph-v27-display-pose-20261028/test-package/`（manifest `d3545033…` 5,020 字节，包内 INI 1,295 字节 `618b1081…` = `inject` + `DisplayPose=true`），ZIP 10,018,217 字节、SHA-256 `6a195266…`，五条目复核相符。v26 三件套备份到 `dist/deploy-backup-20261028-v26/`，MO2 现为 v27 DLL/PDB，部署 INI=`inject` + `DisplayPose=true`（1,063 字节 `2834670d…`）。**该轮不上机**；随后的一次运行待记录。

<a id="g2-v27-run"></a>
## G2 v27 运行记录（2026-10-28 00:24 会话，静态接驳成立）

构建 `CharacterPanel-scenegraph-behavior-graph-v27-display-pose-20261028-c894c956f8-cl94faaed0c6-20261027T162057Z`（DLL `b973aa5b…`），部署 INI=`inject` + `DisplayPose=true`，运行时 `1-6-1170-0`：00:24:14.761 加载，00:24:59.371 首次生效。**无崩溃、无 `[E]`**。日志归档 [CharacterPanel-g2v27-display-20261028-0025.log](diagnostics/CharacterPanel-g2v27-display-20261028-0025.log)。

**观测**：`EXPERIMENT setting DisplayPose=true`；`ANIMATION DISPLAY source=independent-graph bones=116 note=frozen-at-last-driven-step`（1 条）。用户复核截图：面板里的角色站姿**确实是 `H2H_Idle`**（与 v24 在 step 120 的归属一致），且**一直不动**。

**判定：静态站姿接驳成立，实际覆盖未计数。** 日志中的 116 是候选输出记录数，不能推成 116 个 name→node 命中；用户画面确认只支持 H2H_Idle 站姿。v32 将分别报告实际 assignments/missing/skipped-root。**不动是当轮设计**：只读取冻结缓冲。

这一条同时给了 G2 之外的第一份**屏幕证据**：v24 说"姿态内容属于 `H2H_Idle`"是数值结论，v27 把它变成了肉眼可见的站姿。视觉一致性成立，但**不等于**场景应用全部验收（逐帧播放、物理、武器挂载交互仍未验）。

<a id="g2-v28-contract"></a>
## G2 v28 契约：逐帧推进显示（2026-10-28，已实现/待构建）

**动机**：v27 的静态接驳已确认——日志 `ANIMATION DISPLAY … bones=116`，画面就是 `H2H_Idle` 的站姿，用户复核一致。它不动是设计使然：`held_display_pose` 只读冻结缓冲。v28 让保留图每渲染帧推进一次，再把新姿态接给显示图。

**变更**：

1. 记录保留图指针 `g_held_graph`（与 `g_held_character` 同一保留点）。
2. 新增 `step_display_pose()`：调用一次 `63589(candidate, &mode0)`——与批量驱动完全相同的入口与 mode 字节，不派发事件、不做 guard、不借 world——然后按 v27 的规则读回姿态。
3. 新键 `[Experiment] DisplayStep`（默认 false）。为真时 `advance()` 用 `step_display_pose`；步进返回 false 时回落到冻结姿态。
4. 诊断：第 1/60/120 次调用各打一行 `ANIMATION DISPLAY step=<n> pose-hash=<hex> bones=<n>`，把“图有没有走”和“画面有没有跟”分开判。

**风险与边界**：这是第一次在渲染路径里逐帧调用原生生成入口。批量里 63589 每步一次、共 120 次；这里同样每帧一次。但批量周边还有 world 借用与 slot 操作，逐帧路径没有——63589 的门（setup/animationSkeleton/behaviorGraph 非空 + `+0x12C` 已激活）不依赖它们，预期可跑；若三次哈希相同，则说明孤立调用不自推进，下一问回到 63587 的更新是否必须随步执行。

**回退**：`DisplayStep=false` 回到 v27 的冻结姿态；`DisplayPose=false` 完全回到 OAR 路径。

**预注册判读（逐条）**：

1. 三次 `pose-hash` 互不相同、画面在动 → 逐帧推进成立，场景应用打通。
2. 哈希相同、画面不动 → 63589 在缺 world/缺批量周边时不推进：记“入口孤立调用不自推进”，下一问查更新链。
3. 只有首次那行、没有 step 行 → 走的是回落路径（步进返回 false）。
4. 崩溃 → 回 `off`，保留完整日志与匹配 PDB。

<a id="g2-v28-build"></a>
**实现与部署（2026-10-28 00:27，当轮未上机）**：身份 `CharacterPanel-scenegraph-behavior-graph-v28-display-step-20261028-44dcdf4663-cl94faaed0c6-20261027T162729Z`（干净 `44dcdf4`，无 `+dirty`；DLL `e3f9ade8…` 1,302,016 字节、PDB `3acff4af…` 35,958,784 字节、age 337）。改动：`independent_graph.{h,cpp}` 记录 `g_held_graph` 并新增 `step_display_pose`（每帧一次 `63589`，第 1/60/120 次各打一条 pose-hash）、`settings.{h,cpp}` 增 `DisplayStep`、`controller.cpp` 在 `DisplayPose` 分支里优先步进、失败回落冻结姿态。五个 engine-free 用例全过。包 `build/behavior-graph-v28-display-step-20261028/test-package/`（manifest `46ae35ee…` 4,357 字节，包内 INI 1,338 字节 `327e36a9…` = `inject` + `DisplayPose` + `DisplayStep`），ZIP 10,015,286 字节、SHA-256 `d2965ca8…`，五条目复核相符。v27 三件套备份到 `dist/deploy-backup-20261028-v27/`，MO2 现为 v28 DLL/PDB，部署 INI 三键（1,070 字节 `0866cec7…`）。**该轮不上机**；随后的一次运行待记录。

<a id="g2-v28-run"></a>
## G2 v28 运行记录（2026-10-28 00:47 会话，只调 63589 不推进 —— 负结果）

构建 `CharacterPanel-scenegraph-behavior-graph-v28-display-step-20261028-44dcdf4663-…`（DLL `e3f9ade8…`），INI 三键（`inject` + `DisplayPose` + `DisplayStep`），运行时 `1-6-1170-0`。**无崩溃、无 `[E]`**。

**观测**：

```
00:47:43.867  ANIMATION DISPLAY step=1   pose-hash=7d568c4dee1ac065 bones=116
00:47:43.867  ANIMATION DISPLAY source=independent-graph bones=116 note=stepping-per-frame
00:47:45.607  ANIMATION DISPLAY step=60  pose-hash=7d568c4dee1ac065 bones=116
00:47:47.222  ANIMATION DISPLAY step=120 pose-hash=7d568c4dee1ac065 bones=116
```

三次 `pose-hash` **完全相同**（`7d568c4d…`），画面不动，用户复核一致。120 次调用分布在约 3.35 秒内（约 36 fps），所以调用确实发生了，只是没有产生任何姿态变化。

**判定：预注册第 2 条命中——63589 孤立调用不自推进。**`step_display_pose` 只做了一次 `63589(candidate, &mode0)`，与批量"每步一次"的调用**在参数上完全相同**，但批量每步之前还跑了引擎自己的每帧管线：

```
frame_trace(pre_pipeline)
construct(shell, nullptr, false)      // 63555
frame_pipeline(shell, candidate_base) // 63602
shell_destroy(shell)                  // 63556
frame_trace(post_pipeline)
... 之后才是 63589
```

v28 漏掉的正是这三条。这一条同时解释了为什么批量能推进、逐帧不能：**推进来自 63602 管线（63587 → clip 更新），63589 只是采样入口**。

**代价与收获**：一轮负结果换来一条明确的责任划分——批量的 120 步推进不是 `63589` 的功劳。v29 据此补上管线。

<a id="g2-v29-contract"></a>
## G2 v29 契约：逐帧补齐引擎管线（2026-10-28，已实现/待构建）

**动机**：v28 证明只调 `63589` 不推进。批量每步的真实顺序是 `63555 → 63602 → 63556 → 63589`，其中 63602 才是每帧图管线（63587 → clip 更新 → `0x12E`），63589 只负责采样。v29 把这三条补进 `step_display_pose`。

**变更（只改 `step_display_pose` 内部）**：

1. 每帧用栈上的 `shell_storage` 构造 scratch 壳（63555，`construct(shell, nullptr, false)`），跑 `frame_pipeline(shell, g_held_graph)`（63602），立即 `shell_destroy(shell)`（63556）。
2. 然后照旧一次 `63589(g_held_graph, &mode0)`，再读回 `poseLocal`。
3. 诊断保持 1/60/120 三条 `pose-hash`。

**风险**：渲染路径每帧多三条原生调用（构壳、管线、销毁），与引擎自身每帧所做的工作同量级；批量里这套调用跑过 120 次无崩溃。**这是本实验第一次把 63602 放进渲染路径**，若出现崩溃，回退 `DisplayStep=false` 即回到 v28 的静态显示。

**预注册判读**：

1. 三条 `pose-hash` 互不相同、画面在动 → 逐帧推进成立，场景应用打通。
2. 三条仍相同 → 管线也不足以在渲染路径推进，下一问转向"渲染钩子的调用时机是否在引擎更新括号之外"。
3. 崩溃 → 回 `DisplayStep=false`，保留完整日志与匹配 PDB，不在同会话重试。
4. 动但明显抖动 → 逐帧步长与渲染帧率不匹配，记现象，下一问对齐 dt。

<a id="g2-v29-build"></a>
**实现与部署（2026-10-28 00:50，当轮未上机）**：身份 `CharacterPanel-scenegraph-behavior-graph-v29-display-pipeline-20261028-04903ed263-cl94faaed0c6-20261027T165004Z`（干净 `04903ed`，无 `+dirty`；DLL `ce712e20…` 1,302,016 字节、PDB `6157150e…` 35,958,784 字节、age 339）。只改 `step_display_pose` 内部：每帧 63555 构壳 → 63602 管线 → 63556 销毁 → 63589，壳用栈上 `shell_storage`。五个 engine-free 用例全过。包 `build/behavior-graph-v29-display-pipeline-20261028/test-package/`（manifest `12970d32…` 4,110 字节、INI 1,363 字节 `f5ac5618…`），ZIP 9,996,292 字节、SHA-256 `42f92e94…`，五条目复核相符。v28 三件套备份到 `dist/deploy-backup-20261028-v28/`，MO2 现为 v29，部署 INI 三键（1,095 字节 `468fb691…`）。**该轮不上机**；随后的一次运行待记录。

<a id="g2-v29-run"></a>
## G2 v29 运行记录（2026-10-28 00:54 会话，补管线后仍不动 —— 调用没生效）

构建 `…v29-display-pipeline-20261028-04903ed263-…`（DLL `ce712e20…`）。**无崩溃、无 `[E]`**。

```
00:54:33.682  ANIMATION DISPLAY step=1   pose-hash=7d568c4dee1ac065 bones=116
00:54:35.455  ANIMATION DISPLAY step=60  pose-hash=7d568c4dee1ac065 bones=116
00:54:37.111  ANIMATION DISPLAY step=120 pose-hash=7d568c4dee1ac065 bones=116
```

**三条哈希不只是彼此相同，还与 v28 逐位相同（`7d568c4d…`）。**这就排除了“推进得不够”：补上 63555/63602/63556 之后图状态没有任何变化，说明这几条调用在渲染路径里**根本没有生效**，而不是效果太小。

**判定：预注册第 2 条命中（变体）——管线也不足以推进。**下一步不是换配方，而是先确认门的实际取值：`63587/63589` 的入口门（`+0x110`、`[+0x110]+0x20`、`+0x208`、`[[+0x208]+0x12C]`）与 `+0x12C/+0x12E/+0x245/+0x246/+0x240` 在渲染路径里是不是已经翻假。v30 只加诊断，不改行为。

<a id="g2-v30-contract"></a>
## G2 v30 契约：显示路径的门诊断（2026-10-28，已实现/待构建）

**动机**：v28 与 v29 的 pose 哈希逐位相同，说明渲染路径里的原生调用没有产生任何状态变化。先看门，再谈配方。

**变更（只加日志，不改行为）**：`step_display_pose` 在第 1 次与第 60 次调用时各打一行：

```
ANIMATION DISPLAY gates call=<n> active=<+0x12C> children=<+0x12E> b245=<+0x245> b246=<+0x246>
    w240=<+0x240 字> c110=<+0x110 指针> c208=<+0x208 指针> numData0=<槽0 +0x32> numData1=<槽1 +0x32>
```

槽指针按 `plausible_object_pointer` 过滤，不可读记 `-1`。

**预注册判读（逐条，命中即停）**：

1. `active=0` → 图在渲染路径里已不处于激活态：下一问是什么把它关掉的（批次收尾、面板关闭，还是引擎自己）。
2. `children=0` 或 `b245=0` / `b246=0` → 图没有 arm：下一问是 arm 由谁置位、渲染路径里能否补。
3. `numData0=0` 且 `numData1=0` → 两个输出槽是空的，采样器本来就无内容可采；与 v26 批内 `numData=116` 对照即可定性。
4. 门全部健康（`active=1`、`children=1`、`numData≈116`）而哈希仍不动 → 调用在更深一层被短路，下一问转向 63602 自身的门链（`0x110 → +0x20`）。
5. 崩溃 → 回 `DisplayStep=false`，保留证据。

<a id="g2-v30-build"></a>
**实现与部署（2026-10-28 00:56，当轮未上机）**：身份 `CharacterPanel-scenegraph-behavior-graph-v30-display-gates-20261028-0829b1d26d-cl94faaed0c6-20261027T165604Z`（干净 `0829b1d`；DLL `e5b0e57c…` 1,304,064 字节、PDB `e450627a…` 35,975,168 字节、age 341）。只加诊断：`step_display_pose` 在 call 1/60 各打一行门值，行为仍是 v29 的每帧 63555→63602→63556→63589。五个 engine-free 用例全过。包 `build/behavior-graph-v30-display-gates-20261028/test-package/`（manifest `cdebfff0…` 3,919 字节、INI 1,356 字节 `98ebd960…`），ZIP 9,986,675 字节、SHA-256 `1ce2b765…`，五条目复核相符。v29 三件套备份到 `dist/deploy-backup-20261028-v29/`，MO2 现为 v30，部署 INI 三键（1,068 字节 `50adad10…`）。**该轮不上机**。

<a id="g2-v30-run"></a>
## G2 v30 运行记录（2026-10-28 00:58 会话，门诊断无效的后验更正）

构建 `…v30-display-gates-20261028-0829b1d26d-…`（DLL `e5b0e57c…`）。**无崩溃、无 `[E]`**。`step` 哈希仍三连相同（`7d568c4d…`）。

**门值（call 1 / 60 / 120 三次完全一致）**：

```
active=17  children=0  b245=0x00  b246=0  w240=116
c110=0x211f2bc49c0  c208=0x211f272ca00  numData0=62016  numData1=1
```

**后验更正（2026-10-28，v31 源码复核）**：

1. `active=17` 和 `children=0` **不是行为图的门值**：v30 从外层 `BShkbAnimationGraph+0x12C/+0x12E` 读取；真正字段属于内部 `hkbBehaviorGraph::isActive/updateActiveNodes`。“图没有失活”“没有 arm”两条解释均撤回。
2. `numData0=62016/numData1=1` 无效的原因已定位：少读取输出对象中的 element 指针。正确链是 `generatorOutputs[i] → element → +0x32`，已有 `FrameTrace` 采用这条链。
3. `b245=0/b246=0` 本身也不能判失败；同一轮成功批量在 step 2、119 的 Path B 已有这组值。旧门值又都取自生成之后，不能冒充调用前的前提。
4. 批量与显示同在 `[38632]` 线程、显示哈希三次相同仍是有效观察，但只证明**当时不完整的显示步骤没有推进**。成功批量显式调用了 `63587(dt/eye)`，v29/v30 没有；此前“完整逐帧调用零效果、应转缓存”的推论依据不足，撤回。

原始日志、上面的数值和已归档包身份不变。后续采用下面的完整单步验证，不能把旧负结果扩大为架构不可行。

<a id="g2-v31-correction"></a>
## G2 v31：共用单步与诊断更正（2026-10-28，离线通过，待上机）

> 以下记录 v31 的实现。来源/测量归属、运动判读和覆盖统计已由 [v32](#g2-v32-display-session)更新，不能把这份历史单步修正当成当前完整接口说明。

### 目的与范围

修正显示路径漏更新和诊断读错对象；复用已通过的批量步骤验证跨帧姿态，不实现缓存播放，也不推进 G3–G6 或装载图销毁。没有新原生入口、ABI、依赖、manager/sink 或第二个 Actor。

### 驱动、所有权与接口

`drive_graph_step` 的顺序是辅助管线 → 内部行为图更新标记 → `63587`（固定 dt=1/60、原有眼位/模式记录）→ 骨数镜像 → 原有生成入口。批量的五个 `FrameStage` 观察点及旧日志预算保留，显示通过同一 `drive_candidate_step` 执行这些步骤。

`AnimationController::advance` 从持有的 `BSTSmartPointer` 传入源图 const 引用；`step_display_pose(source_graph,out)` 只接受同一来源、已完成批量的 exact AE 1.6.1170 inject 候选，并重用 `generation_entry_status` 的私有存储守卫。物理世界只在单步中借用，`ScopedGraphWorld` 恢复候选原字段，不保留源 world 裸引用、不逐帧重新注册 ragdoll。

`IndependentGraph=off`、`DisplayPose=false`、`DisplayStep=false` 仍是默认值。显示实验未满足前提则返回 false，沿既有冻结姿态回退；没有可用候选时沿原 OAR 路径。仍是每次显示调用推进一个固定步长，尚未实现按实际 FPS 校准速率。

### 代码与验证入口

| 文件 / 稳定入口 | 职责 |
| --- | --- |
| [independent_graph.cpp](../src/animation/behavior_graph/independent_graph.cpp)：`drive_candidate_step`、`step_display_pose` | 既有 native 调用与资格守卫、显示取证 |
| [drive_step_policy.h](../src/animation/behavior_graph/drive_step_policy.h)：`drive_graph_step`、`ScopedGraphWorld` | 共用步骤与临时 world 恢复 |
| [graph_observation_policy.h](../src/animation/behavior_graph/graph_observation_policy.h)：`observe_graph_activity`、`observe_output_slot` | 内部行为图成员与 output→element 读取 |
| [frame_trace.cpp](../src/animation/behavior_graph/frame_trace.cpp)：`FrameTrace::phase` | 复用正确槽读取，保留既有阶段观察 |
| [controller.cpp](../src/animation/controller.cpp)：`AnimationController::advance` | 源图所有权、开关与显示回退 |
| [behavior_graph_display_step_test.cpp](../tests/behavior_graph_display_step_test.cpp) | 两个 engine-free 用例：有 dt 的单步姿态变化/world 恢复；分离的外层/行为/输出/element 内存读取 |
| [CharacterPanel.ini](../config/CharacterPanel.ini) | 三个实验默认值及重启约束 |

依赖仍为现有 C++23/MSVC、CommonLibSSE、AE1170 地址库与已加载项目；离线测试不加载游戏。构建命令和实际身份见 [v31 build](build.md#v31-display-step-2026-10-28)。

### 失败与上机判读

显示调用 1/60/120 的 `post-pipeline/post-update/post-sample` 各记录内部激活/子图标记和正确 numData，共 9 行；另有 3 行姿态哈希、1 行 120 步有效分量 `pose-summary`、2 行源对照及 controller 的 1 行来源，总上限 **16 行**，120 步后不继续输出。阶段零值仅是观察，不自动判失败。

v31 的显示哈希复用批量 `format_pose_hash`；v28–v30 显示代码的 FNV 初始值不同，**不能用跨版本哈希与旧 `7d568c4d…` 是否相同判断运动**。只比较 v31 内部结果，重点看有限值、有效分量变化和实际画面。

待做一次 exact1170 inject 上机：`DisplayPose=true/DisplayStep=true`，确认 120 个实际显示帧中骨变换有限且持续变化、画面连续运动、源 pose/结构对照保持不变。异常或源漂移时回 off 并保留日志/PDB。当前没有新运行日志，不宣称显示通过；若完整步骤仍停住，依据修正后的阶段取证定位，再决定是否采用缓存备选。

### 修改与同步约束

批量和显示必须继续共用同一记录/步骤；任何 native 前提变化先核对既有引擎审计。同步维护上述实现、两个用例、默认 INI、本节、README 导航和 build 证据。历史 v28–v30 原始数值保留，后验更正与运行结果分开；G2 的限定通过不扩展到全局隔离、完整 OAR 或生命周期。


<a id="g2-v32-display-session"></a>
## G2 v32：来源、测量归属与运动/覆盖证据（2026-10-28，离线通过，待上机）

### 范围与来源有效性

本轮只修四项审计问题，保留 v31 的原生单步、exact1170/inject/private-storage 守卫与临时 world 恢复；没有新 REL ID、ABI、依赖、native 调用、物理功能或装备交接设计。

`DisplaySourceIdentity` 记录 actor、源图地址、`source_graph_generation` 和 controller owner。冻结和逐步读取都先经 `DisplaySession::State::accepts_source`；图地址改变、同地址 generation 改变或 owner 失效时，旧候选不得再参与显示，沿现有 OAR 路径。普通关闭/重开仍可复用同一有效候选；源生命周期改变后不会自动创建新候选。

`AnimationController` 析构只清除它绑定的保留视图资格，防止 PreLoad/NewGame 等销毁后复用地址和从零开始的 generation 再次匹配。已装载候选仍按原实验约束保留，不调用 native 销毁、不再次分配；需要新来源的独立图实验须重启进程。

### 窗口与重捕获

测量由 controller 的 `DisplaySession` 持有，不再使用函数静态计数/基线。新 request、来源失效或 controller reset/关闭会结束或丢弃部分窗口；只有同一有效窗口的 call 1 与 call 120 才比较源控制。关闭后的 60+60 次调用不会拼成一次 120 步结果。

F7 导致显示拷贝 retarget 时，即使来源和 request 未变，也会结束旧 view 窗口并创建新 view epoch，重新采基线和报告映射覆盖。`window=aborted` 不做源前后比较。每个窗口最多 **18 条** display 日志，包含阶段、源对照、覆盖、摘要和中止；没有逐帧日志或无限缺失名字列表。

### raw、候选语义运动与实际写入

- 原 `compare_pose`、`pose_sample_max_delta`、identity/alias-watch 以及 `pose-steps` raw 位串语义不变。raw 119/120 继续是取证数据，不能单独作为运动通过判据。
- `compare_display_motion` 使用 translation XYZ、归一化旋转的符号不变弦长 `sqrt(2*(1-|dot|))` 和显示实际使用的 uniform scale X。阈值是 **1e-6**，保持逐帧小运动敏感度；没有复用片段归属的宽松 1e-5 agreement 容差。q/-q 翻转和仅 scale Y/Z 改变不算候选语义运动。
- 批量增加一行 `phase=pose-motion`（旧 raw 行保留）；显示摘要分别记 `raw-changed/raw-finite` 与 `semantic-changed/semantic-valid`，标明 `scope=candidate acceptance=unjudged`。候选中变化的骨可能根本没有映射到显示图，故仍不等于可见运动。
- `apply_named_display_pose` 是实际赋值路径，返回真正的 assignment 数。日志分别报告 output records、去重 requested、applied、missing、skipped-root 和 duplicates，v32 旧格式最多列出 4 个缺失名字；v42 容量为 8，计数仍为权威总数。根排除来自既有 retarget 不索引显示 root 的策略，以实际 root 名识别，没有硬编码骨名。`advance()` 返回 applied，不再返回候选记录数。

### 代码、用例与同步

| 入口 | 作用 |
| --- | --- |
| [display_state_policy.h](../src/animation/behavior_graph/display_state_policy.h)：`same_display_source`、`invalidate_display_source`、`DisplayWindow` | 来源/owner、请求/view 窗口与日志预算 |
| 同文件：`apply_named_display_pose` | 真实赋值及 matched/missing/root-skipped 计数 |
| [pose_motion_policy.h](../src/animation/behavior_graph/pose_motion_policy.h)：`compare_display_motion` | 独立语义运动；复用既有旋转比较数学 |
| [independent_graph.h](../src/animation/behavior_graph/independent_graph.h)、[independent_graph.cpp](../src/animation/behavior_graph/independent_graph.cpp)：`DisplaySession`、`held_display_pose`、`step_display_pose`、`record_application` | controller 所有的基线、native-step 取证、候选读取和覆盖日志 |
| [controller.cpp](../src/animation/controller.cpp)：`request/reset/retarget/advance` 与析构 | 传递来源身份，重置窗口，析构失效视图，应用真实覆盖 |
| [behavior_graph_display_session_test.cpp](../tests/behavior_graph_display_session_test.cpp) | 两个连贯场景：来源/owner/关闭/重开/F7；q/-q、真实小运动与实际写骨覆盖 |

两个新 engine-free 场景和 Release 构建通过，实际身份/哈希见 [v32 build](build.md#v32-display-session-2026-10-28)。v32 没有修改旧 v31 用例和 native-step/FrameTrace 文件，未扩大旧测试集；本次文档完善没有重跑测试或构建。默认 `IndependentGraph=off/DisplayPose=false/DisplayStep=false`，没有部署或启动游戏。

<a id="v32-manual-checklist"></a>
### AE1170 手工验证清单（v33/v34 已部分执行，剩余项注明）

以下程序的历史执行状态见引用运行记录；剩余项仍待新的有授权游戏会话。v42 新诊断不能替代运行证据。

**v44 当前计数说明**：下方旧 v33/v34 的 call/墙钟记录保持原样。新调度的 `native-step` 与 gates `call` 均为实际原生步序号，120 native steps 不等于 120 rendered calls；零步调用仍显示 stepped 候选姿态。验证应观察原生步 120 与实际 elapsed 时间，并分别记录停顿/100 ms 截断、重开/F7 和不可推进恢复；尚无 v44 游戏数据。

> 执行状态（2026-10-28）：**第 1–6 步已执行**——身份核对改为 [v33 记录](build.md#v33-rebuild-2026-10-28)，首个非中断窗口成立（[v33](#g2-v33-run)），中断（`steps=61 reason=reset`，无摘要）、重开不拼接、F7（`steps=84 reason=retarget`）全部成立（[v34](#g2-v34-boundaries)）。**第 7 步部分成立**：读档后回落 OAR 且无 native 行已验；旧日志没有确切拒绝原因。同 controller 来源图/generation 替换及单独 frozen-only 拒绝未验。

1. **核对输入和身份。** 保留待测 DLL 与匹配 PDB、原 INI 和日志。若使用现有 v32 离线构建，逐项核对 [build 记录](build.md#v32-display-session-2026-10-28)的 SHA-256、字节数及完整 `SCENEGRAPH BUILD` 身份（`f5ec13f2c2+dirty`，配置时间 `20261028T073724Z`）；它是提交前产物，后续提交不会改写身份。若另行重新构建，则记录实际新身份/哈希，不能沿用旧记录。确认运行时为 exact **AE 1.6.1170 / `1-6-1170-0`**。v33 已按上方执行状态部署，核对该对哈希而不是旧 v32 记录。
2. **只调整三个实验键，重启。** 在实际加载的 `[Experiment]` 中设 `IndependentGraph=inject`、`DisplayPose=true`、`DisplayStep=true`，保留其他 INI 设置。设置在 DataLoaded 加载，须完整退出游戏再启动。保留此轮实际 INI；详细 tracing 不是这些 display 行的前提。首次可用的 `panel-open` 执行一次候选批量；确认本轮候选具备 loaded/retained、私有状态守卫与来源身份，再解释后续 display 行。
3. **完成首个连续窗口。** 打开暂停背包，保持同一 actor/source graph/generation/controller、request 和 view；在前 **120 次 eligible display calls** 内不关闭、不 F7、不改变来源/装备。固定步长是每次 eligible 调用 1/60 秒；调用数不等于实际显示 FPS，也不等于同样长的墙钟时间。记录 `native-step=1/60/120` 及各自 `gates` 的 `post-pipeline/post-update/post-sample`。120 后驱动可以继续，但该窗口测量/日志受预算限制，不产生第二份摘要。
4. **分别填写三类结论。** 候选：并列保存批量 `phase=pose-steps` 的 raw 位串与 `phase=pose-motion comparisons/changed/valid/epsilon`，以及显示 `pose-summary native-steps=120 raw-changed/raw-finite/semantic-changed/semantic-valid`。语义阈值为 **1e-6**，translation XYZ、符号不变 quaternion chord、scale X；q/-q 或仅 scale Y/Z 改变不算语义运动。计数是“至少一个候选骨变化”的比较次数，不是变化骨数；批量与显示的起始基线不同，不要求语义值照搬历史 **119/120**。`acceptance=unjudged` 要保留，按 G2 原有范围独立评估逐步语义连续性。应用：保存同窗口 `application mode=stepped` 的 `output-records/requested-unique/applied/missing/skipped-root/duplicates/missing-names`；核对 `requested-unique = applied + missing + skipped-root`、`duplicates = output-records - requested-unique`。v33/v34 旧日志最多列四个名字，v42 新格式最多八个；计数才是总数；不要求恰好 **116** 个 applied。画面：直接观察/记录拷贝角色是否持续运动、是否冻结或跳变；候选语义变化与 applied 数均不能替代视觉确认，也不能证明变化的骨都被映射。
5. **核对同窗口源控制。** 将 `source-control stage=before` 与 call 120 的 `stage=after` 按 request/source-generation/view 配对，必须看到 `source-drift=none` 且 `pose-before=pose-after`。不能跨 request/view 拼源基线。源快照只是所测字段/raw pose 的控制，不能扩大为全部共享状态、原生阶段隔离或全局收敛证明。
6. **观察中断、重开和 F7。** 在后续新窗口中，若在 120 前关闭（例如只到 checkpoint 60），应丢弃旧窗口并在有部分步骤时报告 `window=aborted ... source-comparison=discarded`；重开从新基线开始，旧 60 + 新 60 不能组成摘要。同一有效来源允许复用保留候选。F7 接受新拷贝并 retarget 后，应重新报告 coverage，从新 view/request 窗口的 call 1 采源基线；正常兼容动画时钟不因测量重置而倒带。若 request 同时改变，view 编号可重新从零开始，须以完整身份关联。
7. **最后观察来源/owner 失效。** 来源图替换、同地址新 generation，或 PreLoad/NewGame 等销毁 controller 后，旧候选的冻结与逐步路径都应被拒绝，回到既有 OAR 采样；不能继续把旧 candidate 的行当成新来源。原生候选仍保留、不会自动重建，换来源后若要新的独立候选须重启游戏进程。若没有足够日志/画面区分这些边界，记录为未验证。

**停止与恢复：** 身份/运行时不符、守卫失败、非有限/无效数据、source drift 或源 pose 改变时停止本轮；完整窗口缺少必要摘要/源控制或预算被中断耗尽时也停止判通过，不拼接日志或只凭 raw 哈希补结论。崩溃保留完整日志与匹配 PDB，不在同进程重试。完成观察或出现上述异常后，恢复 `IndependentGraph=off`、`DisplayPose=false`、`DisplayStep=false` 并完整重启，保留其余 INI 设置。候选语义连续性、显示覆盖/视觉结果、G1 遗留和 G3–G6 分别记录；本清单不扩大 G2 的验收范围。

下一次上机要同时记录候选语义运动、实际应用覆盖、同窗口源对照和画面；再检查重开、来源 generation 改变和重捕获后的有效性/窗口边界。**G2 逐步语义连续性当前待复验**：v26 的历史 raw 判据通过保留，不能把符号翻转反例当作历史游戏静止的证据。有限、私有、层归属和时钟证据保留；物理/武器交互、原生阶段隔离、装载销毁及 G3–G6 仍未由本轮验证。

后续改动须保持来源守卫同时覆盖冻结/逐步路径、窗口随 owner/request/view 更新、raw 与语义统计分离，并同步此文、代码/两个场景、README/build/Changelog。原始归档日志不改。

<a id="g2-v33-run"></a>
## G2 v33 运行记录（2026-10-28 22:28 会话，首个连续显示窗口成立）

> v33 只是 v32 代码在 HEAD `d323b35` 上的重建（[构建](build.md#v33-rebuild-2026-10-28)），不是新的实验契约。本轮不新增 native 调用、不改配方，只把 v32 的测量与覆盖统计第一次带到游戏里。

**身份与运行时**：`CharacterPanel-scenegraph-behavior-graph-v33-display-session-20261028-d323b357a8-cl94faaed0c6-20261028T142029Z`，`runtime=1-6-1170-0`（exact AE 1.6.1170）。日志[归档](diagnostics/CharacterPanel-g2v33-display-session-20261028-2228.log)（57,004 字节、`11462f1c…`、246 行）；无 `[E]`、无崩溃，19 条 `[W]` 全为既有 DAC 预检拒绝与 studio 灯光账本。

**窗口**：`request=1`、`source-generation=1`、`view=1`，call 1 在 22:28:02.650、call 120 在 22:28:05.969（约 3.32 s，≈36 次/秒），中间没有关闭、F7 或换装；随后才出现 `stage=after`。没有 `window=aborted`。

### 三类结论（分开记）

| 类别 | 读数 | 判读 |
| --- | --- | --- |
| 候选语义运动 | 显示 `pose-summary native-steps=120 raw-changed=120 raw-finite=true semantic-changed=120 semantic-valid=true epsilon=1e-06 scope=candidate acceptance=unjudged`；批量 `pose-motion comparisons=120 changed=119 valid=true`；`pose-content` 三点 `entries=116 finite=116 nonfinite=0 alias=no` | **成立**：符号不变度量（translation XYZ + quaternion 弦长 + scale X）下 120/120 次比较均变化，q/-q 与仅 scale Y/Z 不计入 |
| 实际应用 | `application mode=stepped output-records=116 requested-unique=116 applied=109 missing=7 skipped-root=0 duplicates=0`；`missing-names='x_NPC Rotate [Rot ],x_NPC LookNode [Look],x_NPC Translate [Pos ],Weapon'` | **成立（有界）**：`109+7+0=116` 与 `duplicates=0` 自洽；旧 cap4 只列这 4/7 个名字，其余 3 个未知。已知控制/挂点标签不证明可忽略，`Weapon` 仍需应用语义核对；若后续遇到 `Shield` 也同样核对，不能因此给历史未知项补名 |
| 源对照 | before=after=`23dd49fed3fbf888`，`source-drift=none`，同 request/generation/view；批量 `drive-done source-pose-before=after=23dd49fed3fbf888` | **成立**：源侧姿态未变 |
| 画面 | 用户确认面板内角色连续运动（截图 2026-10-28 22:28:09） | **成立** |

显式显示哈希 `native-step=1/60/120` 为 `63f161f2cd7502c9` / `7a044b3473f832df` / `2b29331eea5279d9`，三点互异。v31 起显示与批量共用同一哈希函数，因此只在本轮内部比较，不与 v28–v30 的 `7d568c4d…` 对照。

### 门读数：v30 结论的运行期反证

call 1/60/120 × `post-pipeline/post-update/post-sample` 共 9 行，全部为 `active=true children=true numAnim=116 numData0=116 numData1=116`（`post-update` 的 `numAnim=0` 在三点与 v31 一致，属该阶段读数）。v30 记下的 `active=17`、`children=0`、`numData=62016` 因此确认是对象基址与 output→element 间接层读错，**不能再用作"图未 arm"的证据**；v31 的修正读法在真实窗口里全部为非零。

### 收尾与范围

22:28:14 `DAC restore … result=matched`（epoch=2、`spell-groups=3`）、`Panel menu opening=false`、`lifetime reset`；会话结束后三键恢复 off 并完整重启才回到默认产品路径。

本记录只覆盖：单进程、单次批量、单窗口 120 次 eligible 调用、暂停背包、一个 actor/来源/generation/view。**未覆盖**：120 前中断与 `window=aborted`、重开不拼接、F7 重捕获的新 view epoch、来源 generation 改变或 controller 销毁后拒绝旧候选、装载图停用/销毁与全局收敛、物理/武器挂载交互、G3–G6、不同帧率下的速率校准。

<a id="g2-v34-boundaries"></a>
## G2 v34 边界会话（2026-10-28 22:47 + 23:00 两次会话，四条边界全部成立）

同一份 DLL（v33）的第三、四次上机，只做窗口/来源边界，不新增代码。两份日志：22:47 会话[归档](diagnostics/CharacterPanel-g2v34-boundaries-20261028-2247.log)（83,924 字节、`23637075…`）、23:00 补测[归档](diagnostics/CharacterPanel-g2v34-reset-20261028-2300.log)（64,003 字节、`bc7f27c4…`）；均无 `[E]`，各 15 条 `[W]` 同属既有 DAC 预检与灯光账本。

**会话形状**：22:46:33 启动 → 开包 1（request=1/view=1，跑满 120）、开包 2（request=2/view=2）、开包 3（request=3/view=2，中途 F7 → view=3）、读档、读档后开包（新 lifetime）。23:00:12 重启 → 开包 1 在 call 61 关包、开包 2 跑满 120。

| 步骤 | 结果 | 证据 |
| --- | --- | --- |
| ① 120 前关窗（`reason=reset`） | **成立（23:00 补测）** | 第二次会话开包 1 在 call 61（23:00:53.569 关包）中止：`window=aborted … request=1 view=1 steps=61 reason=reset source-comparison=discarded`，**没有 `pose-summary`**；随后开包 2 从 call 1 重采 `97dd7a5b8bbd183e`、独立跑满 120 出摘要——61 步没有被接着用。日志[归档](diagnostics/CharacterPanel-g2v34-reset-20261028-2300.log)（64,003 字节、`bc7f27c4…`，无 `[E]`） |
| ② 重开 | **成立** | 开包 2 重采 `stage=before pose-hash=0af9a6a6f3f1c963`（与开包 1 的 `1ebf9021afd2a1d1` 不同）、call 从 1 起算，独立跑满 120 并出第二份摘要——没有接续开包 1 |
| ③ F7 | **成立** | `window=aborted … request=3 view=2 steps=84 reason=retarget source-comparison=discarded`；随后 view 2→3、重采 before（`f5a9a106bc431094`，源未变故与窗口内相同）、`application` 重报覆盖，新窗口再跑满 120 并出摘要 |
| ④ 来源失效 | **成立（观察到回落形态，确切原因未记录）** | 读档后开包只有 `ANIMATION PANEL accepted` 与 OAR `asset ready`，**没有任何 `ANIMATION DISPLAY` 行**，也没有残留 `native-step`。符合旧候选拒绝/回落的预期形态，实验不在同进程重跑；旧日志没有直接给出 owner/graph/generation 等拒绝原因 |

### 附带读数

- **候选轨迹逐位可复现，且与源姿态无关**：本次开包 1 的 `native-step=1/60/120` 为 `63f161f2cd7502c9` / `7a044b3473f832df` / `2b29331eea5279d9`，与 [v33](#g2-v33-run) 那次**逐位相同**；而两次的源 `stage=before` 哈希分别是 `1ebf9021…` 与 `23dd49fe…`。固定 1/60 步长加候选自身图状态决定轨迹，源姿态不参与——隔离的正面证据。
- **重开会让 view 跳 2**：`request=2` 先 `retarget` 到上一份拷贝（22:49:14.311），再 `SCOPY BEGIN capture=2` 后 `retarget` 到新拷贝（22:49:14.316），基线在两次之后才采。view 编号不连续属设计内，不能当异常。
- **call 1 的门可能是瞬态**：`request=3 view=2` 的 call=1 读到 `children=false w245=0x101 b246=1`，到 call=60 已是 `children=true w245=0x0`，窗口照常推进（哈希在变）。门值要看整窗或至少两个检查点，**单点读数会重犯 v30 的错**。
- `SCOPY PARK … snapshots held alive for the process lifetime: 4`：22:47 会话累积 4 份场景拷贝，归 ②（资源收敛）记账，不属于本步。

**范围**：单进程、暂停背包、一个 actor/来源；四条边界（`reset` 中断、重开不拼接、F7 重捕获、读档后拒绝旧候选）全部成立。**未验**：同 controller 来源图/generation 替换、单独 frozen-only 拒绝与旧读档的确切拒绝原因。装载图停用/销毁仍 NO-GO；非例外资源收敛仍属 G6。

<a id="g3-a-contract"></a>
## G3-A：源图时间线记录（历史 v37 契约与 v41 成功观测）

G3 的前置观测。v41 已在正常游戏里取得同一单手武器 Equip→Idle→Unequip 的命名 settled 序列与相位，作为可用的观测基线；没有手动指定动作族/HKX，没有写变量或给源发事件。它不是完整的外部输入清单。下面 v37 契约/输出为历史记录，后续规则和当前限制优先。

**仪器**：`src/animation/behavior_graph/source_timeline.cpp`，由现有 post-UI 渲染钩子（REL ID 79947/82084）每帧调用一次，`[Experiment] SourceTimeline=true` 才生效。只读、不新增钩子、不新增 native 调用、不碰候选图。

**读点**（均按本仓库已在用的偏移核对过头文件与既有代码）：

| 数据 | 路径 |
| --- | --- |
| 变量值 | `BShkbAnimationGraph::behaviorGraph`(+0x208) → `hkbBehaviorGraph::variableValueSet`(+0xD8) → `hkArray` 数据指针 +0x10、计数 +0x18 |
| 变量名 | `hkbBehaviorGraph::data`(+0x88) → `hkbBehaviorGraphData::stringData`(+0x78) → `hkbBehaviorGraphStringData::variableNames`(+0x30) |
| 活动节点 | `hkbBehaviorGraph::activeNodes`(+0x98) → 数据指针 +0、计数 +8；前 64 项做 FNV 哈希 |

计数与既有运行读数一致（`vars[words=652]`），这是本轮唯一可自证的数据形状。

> **更正（2026-10-29 00:05）**：上一句是错的。`652` 是**候选图**的 word 计数（`phase=probe vars[...]`），源图实测是 **463**。两者本来就不是同一个变量集合，不能拿候选的数字当源图的预期值。这项更正连同"回放必须按名字而非索引"的结论写在下面的 v37 运行记录里。

**输出**（每帧只在变化时写，单进程 ≤200 行，超出后写一行 `budget-exhausted` 并停）：

```
SOURCE TIMELINE armed actor=00000014 words=652
SOURCE TIMELINE variable index=<i> name='<n>' <old>-><new> float=<f>-><f>
SOURCE TIMELINE nodes count=<n> hash=<016x>
```

历史输出同时给出同一 4 字节的 int32/float 解释；这不是类型证据。名字前缀、数值形态、观测转移均不能确认 Bool/Int/Float 或输入/输出角色。运行时类型需核对 `hkbBehaviorGraphData::variableInfos` / `hkbVariableInfo::m_type`；外部写入者/因果角色还需独立证据。

**预注册观察（不得事后放宽）**：

1. 装备瞬间出现一组变量变化；卸下时出现反向变化。两次的变量集合与顺序**不要求对称**，只记录实际顺序。
2. `nodes` 的 `count/hash` 在装备与卸下各变一次或多次，Idle 期间稳定；若 Idle 期间持续变化，说明该哈希包含会抖动的字段，记录为仪器限制而不是状态变化。
3. `armed` 行必须在进游戏后出现且给出该图的 word 计数。**原判据写的是"必须 652"，那是候选图的数字；源图实测 463，故改为：`armed` 存在、`words>0`、且与后续行的索引范围自洽。** 若没有 `armed`，本轮判为未采。
4. 全程源 `pose`/`activeNodes` 不被本仪器写入（只读），源行为与未装插件时一致。

**停止条件**：没有 `armed` 行、`words≤0`、budget 在 equip 前耗尽、或出现非有限 float 且伴随崩溃 → 停止判读，保留日志与匹配 PDB，回审计。

### v37 运行记录（2026-10-29 00:05，判为未采）

身份 `…v37-source-timeline-20261028-81578f5c63…`。日志[归档](diagnostics/CharacterPanel-g3a-v37-timeline-20261029-0005.log)（23,521 字节、`29ffef2a…`）。`armed actor=00000014 words=463 nodes=36`，随后 200 行在 **8.5 秒内**用尽（`budget-exhausted`）。按停止条件第 3 条，**本轮不采**。

三条失败原因，都已定位到仪器而不是引擎：

1. **预注册的 652 不是源图的数**：`652` 来自候选图（`phase=probe vars[words=652]`），源图是 **463**。契约写错，已在上文更正。这条同时暴露一个真事实——**候选与源的变量集合大小不同（652 vs 463），G3-B 的回放必须按变量名映射，不能按索引照搬。**
2. **节点哈希是每帧噪声**：`count=36` 全程稳定，但哈希每约 20 ms 变一次（91 行全是它）。说明我哈希的 64 个指针内容每帧在变，或 `NodeList` 的布局假设不对；该哈希已删除，v38 只记 `count`。
3. **预算被标量吃光**：`index=53 TimeDelta` 一个变量占 76 行，节点占 91 行，装备/卸下发生在预算之后，**目标序列根本没进日志**。

仍然成立的部分：仪器本身是通的——`armed` 出现、109 行变量带着真实名字（`iEquippedItemState`、`fIdleTimer`、`bowZoom`、`bNSV_Teammate_*` 等），名字解析与 463 计数都对得上。坏的只是记录策略。

### v38 规则修订（不是放宽判据，而是换记录规则）

v38 引入 **settle 规则**：新值变化的那个采样将计数清零，之后还须 **3 个稳定采样**才写盘。`TimeDelta`/`Pitch`/`Velocity` 这类每帧都变的标量永远 settle 不了，自然不落盘；`iEquippedItemState` 这类状态量照常记录。节点同样只记 `count` 且要求 settle。其余（预算 200、只读、armed/rearm）不变。

身份 `…v38-source-timeline-settle-20261029-ad15b58ae8-cl94faaed0c6-20261028T160832Z`，DLL `17567d57…`、1,327,616 B；v37 三件套备份到 `dist/deploy-backup-20261029-v37/`，已部署，`SourceTimeline=true` 不变。构建见 [v38](build.md#v38-settle-2026-10-29)。

**v38 的预注册读数**：`armed` 行给出 `words=463 nodes=36`；装备与卸下各出现一组 **settled** 变量变化（预期含 `iEquippedItemState`），且两次之间没有 `TimeDelta` 之类的高频行；`nodes` 若变化则只记 count。若 200 行仍被高频量吃光、或 settle 后一行变量都没有，判仪器失败并重新设计触发方式。

### v38 运行记录（2026-10-29 00:11，仍判未采）+ v39 改成显式窗口

身份 `…v38-source-timeline-settle-20261029-ad15b58ae8…`。日志[归档](diagnostics/CharacterPanel-g3a-v38-timeline-20261029-0011.log)（22,707 字节、`6f7055bb…`）。`armed words=463 nodes=36`，预算撑到 **18.2 秒**（00:12:15.176 → 00:12:33.332）后被 `budget-exhausted` 截断，**装备/卸下仍未进日志**。159 行变量覆盖 47 个索引，`nodes count` 41 行。

这轮暴露的真问题不是预算大小，而是**源图自身有周期性变化**：

```
00:12:31.425  bVoiceReady 1->0 / BlendJump 0.8->0.6 / bIsInMT 1->0 / nodes 27
00:12:31.448  iLeftHandEquipped 0->9
00:12:32.132  bVoiceReady 0->1 / BlendJump 0.6->0.8 / bIsInMT 0->1        ← 约 1.2 s 后重复
00:12:32.623  同上反向
00:12:32.644  iLeftHandEquipped 0->9
```

`bVoiceReady`/`BlendJump`/`bIsInMT`/`iLeftHandEquipped` 与节点数 22↔27↔44↔54 构成一个约 1.2 秒的循环。任何"常开 + 预算"的记录器都会在这个循环里把预算磨光——**这类周期量的存在本身就是 G3-B 的约束**：回放时不能把这些当成装备事件。

**v39 规则**：记录窗口改为**显式热键触发、20 秒自动关闭**，预算提到 800。窗口外完全不采样，所以进游戏后的待机周期不消耗任何预算；按下热键后的 20 秒里，装备/卸下必然落在窗口内。**热键在 v40 改为 F7（面板未开时）**：F9 是游戏自带的快速读档键，不能占用；F7 在面板内仍是重捕获，没有面板时才开时间线窗口。

身份 `…v39-timeline-window-20261029-5f2cf53de6-cl94faaed0c6-20261028T161546Z`，DLL `5f2f660a…`、1,328,640 B；v38 三件套备份到 `dist/deploy-backup-20261029-v38/`，已部署。构建见 [v39](build.md#v39-window-2026-10-29)。

**v39/v40/v41 上机步骤**：进游戏后先站定（**不要开面板**），**按 F7**（日志出现 `window-open seconds=20`），随后 20 秒内：装一把单手武器 → 等约 2 s → 卸下 → 等约 2 s。窗口自动关闭后退出交日志。

> **v41 判据修正**：F7 的分流依据是 `PanelMonitor::is_menu_open()`，不是"场景图是否存在"——00:21 那次按 F7 只触发了面板重捕获并被 `outside paused inventory` 拒绝，说明场景图对象在面板外同样存在。若再出现 `command=1 ignored outside paused inventory`，即判热键未到达时间线窗口。

### v41 运行记录（2026-10-29 00:26，**G3-A 成立**）

身份 `…v41-timeline-panel-gate-20261029-9bc1a4b03a…`。日志[归档](diagnostics/CharacterPanel-g3a-v41-timeline-20261029-0026.log)（15,705 字节、`bc48e764…`、无 `[E]`）。`arm-requested` → `window-open words=463 nodes=36 seconds=20` → 117 行 → `window-closed lines=117`，预算未触顶。

**装备（00:26:46.634 → 48.159，约 1.5 s）**

| 时刻 | settled 变化 |
| --- | --- |
| 46.634 | `IsEquipping` 0→1、`currentDefaultState` 0→10、nodes 51 |
| 46.823 | `bVoiceReady` 1→0、`bIsInMT` 1→0、nodes 32 |
| 46.840 | `iLeftHandEquipped` 0→9 |
| 46.995 | nodes 33 |
| 47.011 | `bAttached` 0→1 |
| 47.132 | `iMagicEquipped` 0→1、`iRightHandEquipped` 0→1 |
| 47.945 | `IsAttackReady` 0→1、`bVoiceReady` 0→1、`bMLh_Ready`/`bMRh_Ready` 0→1、**`iState` 0→9**、`currentDefaultState` 10→1、nodes 81 |
| 47.963 | `bAttached` 1→0 |
| 48.159 | `IsEquipping` 1→0、nodes 66 |

**卸下（00:26:53.652 → 54.933，约 1.3 s）**

| 时刻 | settled 变化 |
| --- | --- |
| 53.652 | `PitchManualOverride` 0→1、`IsUnequipping` 0→1、`currentDefaultState` 1→12、`bEquipUnequip` 0→1、nodes 82 |
| 53.838 | `IsAttackReady` 1→0、`bVoiceReady` 1→0、`bMLh_Ready`/`bMRh_Ready` 1→0、**`iState` 9→0**、nodes 33 |
| 54.504 | `iMagicEquipped` 1→0、`iRightHandEquipped` 1→0 |
| 54.728 | `bVoiceReady` 0→1、`currentDefaultState` 12→0、`bIsInMT` 0→1、nodes 52 |
| 54.744 | `iLeftHandEquipped` 9→0 |
| 54.933 | `PitchManualOverride` 1→0、`IsUnequipping` 1→0、`bEquipUnequip` 1→0、nodes 36 |

判读：

- 两条序列都把**状态量**写进了日志，而 1.2 s 周期量（`BlendJump` 等）没有主导输出——settle + 窗口两条规则合在一起生效。
- `iState` 0→9（装备完成）与 9→0（卸下完成）是最干净的相位标志；`currentDefaultState` 走 0→10→1→12→0，`IsEquipping`/`IsUnequipping` 各是一段 1.5 s / 1.3 s 的矩形脉冲。
- 装备与卸下的变量集合**不对称**：装备有 `bAttached`、`IsAttackReady`、`bMLh_Ready`/`bMRh_Ready`；卸下有 `PitchManualOverride`、`bEquipUnequip`。回放时不能只按"把装备序列取反"来做。
- `TimeDelta` 仍出现约 50 次（settle 拦不住持续抖动），但在 800 预算里不构成问题。
- 00:27:02 起的 `Speed`/`iSyncIdleLocomotion`/`bInJumpState` 等用户移动仍在窗口内：实际关闭为 **00:27:04.193**。它们在所选装备/卸下分析片段之外，不是“窗口关闭后”的行。

**G3-A 判定：成立**——同一把单手武器两段按名字可用的 settled 相位轨迹已取得，未手动指定动作族/HKX、未发事件或写变量。变化后还需三个稳定采样，时间戳反映 settle 后的采样/输出时刻；短暂值可能漏失。同次采样按变量索引遍历，同时间戳行不证明原始 setter 顺序，日志也不提供源事件时间。该成功观测不升级为完整外部输入轨迹；G3-B 先完成下方角色/类型/时序前置核实。

**预注册（v39）**：`window-open` 出现且 `words=463`；装备与卸下在窗口内各出现一组 settled 变化；同一索引在窗口内出现次数不超过其真实状态数（例如 `iLeftHandEquipped` 不应出现 10 次以上）；`window-closed lines=N` 且 N≤800。若窗口内仍只看到 1.2 s 周期循环而没有装备变化，说明装备走的是事件而非变量——那 G3-B 的输入就必须以事件为主，变量只作对照。

**当前运行步骤**：`SourceTimeline=true`、其余三键 off，重启；进入游戏后站定、不开面板，按 **F7** 开 20 秒窗口，随后装备一把单手武器 → 等约 2 s → 卸下 → 等约 2 s；等 `window-closed` 后保留日志。面板内 F7 是重捕获；F9 保持游戏快速读档。旧不按 F7 的常开式说明仅属于历史 v37/v38。

**不做**：不写变量、不发事件、不调 native、不把源图状态复制给候选。G3 的回放与"候选走同一顺序"的判定属下一份契约。

## 当前采集器与操作

`BehaviorGraphProbe`只读源日志仍按每actor/request最多16条预算。另一路`IndependentGraph=off|construct|load|drive|inject`默认off、重启生效，首次暂停panel-open每进程最多一次；v17 inject的INDEPENDENT GRAPH既定预算为40行，另有复用的源分组快照。候选可以被构造/装载/激活/推进/注入，不能用源探针“只读”描述整个实验；源图保持只读。装载候选刻意保留至进程结束，每进程至多一张，不代表安全销毁已实现。

代码入口：[`ownership_probe.h`](../src/animation/behavior_graph/ownership_probe.h) / [`ownership_probe.cpp`](../src/animation/behavior_graph/ownership_probe.cpp) 的 `behavior_graph::observe_source_baseline`；[`AnimationController::request`](../src/animation/controller.cpp) 在持有源图的生命周期内调用。设置由 [`settings.cpp`](../src/animation/settings.cpp) 的 `load_settings` 读取，示例在 [`CharacterPanel.ini`](../config/CharacterPanel.ini)。无新目标和依赖。

只在现有配置合并以下片段，保留其他设置，重启游戏生效。它独立于 `DetailedAnimationTrace`，无需开启后者：

```ini
[Diagnostics]
BehaviorGraphProbe=true
```

用正常开包/换装触发现有请求，收集 `BEHAVIOR GRAPH` 日志及插件构建身份。每个 actor/request 最多一份快照、最多 16 条分组行；读取固定成员的身份/计数/标志，不扫描活动节点内容、动画表、附近 Actor 或骨树。`BehaviorGraphProbe=false` 时仅分支判断，不遍历或格式化。`role=source-baseline candidate=none construction=unverified gate1=not-passed` 是源快照类型的固定标签，表示这条记录不描述候选；它不是当前 G1 判定，也不否定另一条 INDEPENDENT GRAPH 实验流中的候选。

地址转为整数值后写日志，不保存裸指针、owning 引用或跨请求状态。现有 `SceneGraph::schedule_animation` 每递增一次 request 仅调用一次 `AnimationController::request`，由调用点保证采样预算；不会因跨会话请求号重置而被全局去重误抑制。快照依赖所读 SDK 声明，且不是线程间原子快照：请求所处暂停更新括号与源图持有期可减少生命周期风险，不能证明内部数组不变。零地址/`missing` 表示该可选对象不存在；没有采集的字段均为未验证，不能推断独立。

此 API 不调用构造、Activate/Update/Generate/Deactivate、变量写入、图事件、OAR 求值、DAC、效果或 Actor 分配。它只观察源基线，不负责新图创建与清理，也不能单独证明源无漂移。若启用后异常，保留日志/版本信息，改回 false 并重启；不再推进后续关卡。

## G1 二进制静态审计（2026-10-23）

> 以下保留 10-23 初审时点；后续加载、激活和 G1 修订通过见[当前结论](#current-status)，不能把当时“未完成”当当前状态。

零售 1.6.1170 的 `.text` 被 SteamStub 3.1 加密，审计在只读副本的解包镜像上进行（镜像 SHA-256 `843823C115DBC56B5E9740350DB49170CC4EEA013A79513B7FD066DEEC719A2B`；完整方法、地址与命令见[审计记录](behavior-graph-construction-audit.md)）。已确认的只有构造/析构的所有权事实：

- 五种 `ConstructAnimationGraph` 实现共用同一模板：`MemoryManager::Allocate(0x250, 0x10, true)` → `BShkbAnimationGraph::BShkbAnimationGraph(graph, holder, flag)` → `BSTSmartPointer` 赋值。holder 为 `this-0x38`（REFR/Actor/PlayerCharacter）或 `nullptr`（Simple/Weapon）；flag 在 Actor 上由 `race->data.flags` 第 7 位取反（CommonLib 标注 `kFlies`，语义未证），PlayerCharacter 固定为 0。
- 构造只写 vtable、引用计数、两个事件源、`hkbCharacter`（+0xC0）与 holder（+0x210）；`projectName`/`projectDBData`/`behaviorGraph`/`rootNode` 全部置空。不加载项目、不登记 manager、不挂 sink、不设置 `rootNode`。
- 删除析构 `0xbbcf30` → 真正析构 `0xbad2c0` → `MemoryManager::Deallocate(ptr, true)`，与分配参数配对；对已加载的 `behaviorGraph` 有拆除动作（槽位 7 + 释放 `generatorOutputs`/`rootNode`/fade controllers）。
- OAR 从 `graph->holder` 取 REFR，空时退回 `rootNode` 再取；候选图的 holder/rootNode 选择因此直接决定 OAR 读到谁。

当时未完成激活/更新、事件路由与共享范围，G1 未通过；后续已定位加载/激活/推进链并进行 v1–v17 定位实验，见[审计记录](behavior-graph-construction-audit.md)和[当前结论](#current-status)。OAR 语境、全局状态与装载图停用/销毁仍未验收，不能由入口定位推断通过。

<a id="g1-teardown-audit"></a>
## G1 遗留一：装载图停用/销毁（2026-10-28 审计，判定 NO-GO）

> 本节只做静态判定，不新增 native 调用，也未上机。目标是决定"是否值得为一劳永逸的销毁写实验"，不是证明销毁可能。

**`hkbBehaviorGraph::Deactivate` 的真实形态**（vtable 槽 7 = `0xab1f70`，`.pdata` 只记 25 B，实体延伸到 `0xab20b1`）：

- 序言 `cmp qword [rdx+8], 0; jne 0xab20b1`——**当 context+8 非空时直接返回，什么都不做**。是否真的拆除取决于调用方给的 context，而不是图自身状态。
- 否则先 `mov rax,[rcx+0x98]`（`activeNodes`）再 `movsxd r15,[rax+8]`：**对 `activeNodes` 没有任何空检查**。G1b v3 的崩溃就是这一步（从未激活的图 `activeNodes=0`），指令级原因在此确认。
- 遍历 `activeNodes` 时对带 `+0x88` 标记的条目回调节点槽 0，并把 `context+8` 临时改成 `[条目+0x60]`；循环后 `call 0xab7090`，再在 `variableMode==0` 时 `0xa883a0`（Release）释放 `variableValueSet` 并把 `[this+0xD8]` **置空**，最后 `[this+0x12C]=0`（`isActive=false`）。
- 语义后果：一旦 Deactivate，图不能再被驱动，必须重新 Activate；我们保留的候选没有 Activate 的第二条入口。

**引擎自己怎么配对**：`BShkbAnimationGraph` 的真析构 `0xbad2c0` 在 `[+0x208]`（behaviorGraph）非空时，用 `58315`（`0xab0a60`）就地构造 `hkbContext`，然后 `call qword [rax+0x38]`——即槽 7 Deactivate，之后才释放 `generatorOutputs`/`rootNode`/fade controllers。**Deactivate 只在"图即将随容器一起消失"时被调用**，我们的候选从不进入这个路径，于是登记与内存都留到进程结束。

**为什么仍判 NO-GO**：三份日志（22:28 / 22:47 / 23:00）里候选与源的 `root-generator` 是**同一个指针**（`0x1c7b4322c70` / `0x1f134ae2c70` / `0x201343a2c70`），而 `active-nodes` 各自独立（候选 `…/97`、源 `…/111`）。也就是说加载器把项目级生成器树作为共享数据借给候选，激活时才建自己的活动节点数组。Deactivate 的循环正好走 `activeNodes` 条目并对部分条目回调——**若任一条目指向共享模板而非克隆，就会改到源图的状态**。这与崩溃不是一个量级：崩溃可复现、可放弃；源图被污染会跟着存档走。在"活动节点条目是否为克隆"取证之前，正常存档上不做销毁实验。

**收益核算**：候选每进程至多一张（`attempted` 静态 + `retained` 分支），已装载即不再分配。销毁能换回的是"一次进程内的一个图 + 一条登记"，不是可累积的泄漏。因此它是质量项，不是稳定性项，**不阻塞 G3–G6**。

## G1 遗留二：全局登记计数（2026-10-28 审计 → v35 上机证伪 → v36 回滚）

**这一项先前的静态结论是错的，本节保留更正过程。**

原先的读法：`63845`（`0xbc1d70`）尾部用自旋锁包住一段登记，`[r10+0x1F8]+0x30` 是计数、`inc` 后 `call 63529` 插入，因此"每次接受装载都会让全局登记计数 +1"。据此加了 v35 只读探针并在 23:16 上机。

**上机读数**：`phase=retained source-drift=none registry=300474432->300474432`（日志[归档](diagnostics/CharacterPanel-g1v35-registry-probe-20261028-2316.log)，54,198 字节、`0046da43…`）——**前后完全相同，且 3 亿这个量级不可能是计数**。按预注册的停止条件当场判该轮未采，回到审计。

**重读指令后确认三处判错**：

1. `call 0xbabbf0`（id 63529）不是登记插入，而是**把对象推进一条无锁追加缓冲**：`lea rbx,[rcx + (r8&1)*0x18]` 选两条 0x18 字节车道，`lock cmpxchg [rbx],1` 抢槽，`mov rax,[rbx+0x10]` 取尾指针，`[rax]=rdi` 写入对象，`[rbx+0x10]=rdi+0x18` 推进尾指针，最后把锁位清零。**没有数组增长、没有计数存储。**
2. `inc r8d` 只在寄存器里，**63529 全程没有把 `r8d` 写回 `[rcx+0x30]`**；函数体内只写到 `[rbx]`、`[rax]`、`[rbx+0x10]`。所以那个字段是**不变的序列/奇偶值**，不是随装载增长的计数——这正是本次读数前后相同的原因。
3. 该块还有前置门 `test r9,r9; je 0xbc218e`：`r9` 为空时整段跳过。即使跳过，结论也一样——**这里没有可测量的"登记计数"**。

**结论**：`registry-probe` 没有可靠锚点。此前文档里"尾部把完成对象登记进全局注册表、计数自增"这句话是误读，已在本节更正；原始审计记录保留，不追溯改写。**这一项从"待上机"降级为"无锚点、不立项"**，不再占据待办清单。

**v36**：`1ca2ad6` 的探针已 `git revert`（`cee948d`），构建 `…v36-registry-probe-revert-20261028-cee948dcf8…`（DLL `ac01ac95…`、1,319,424 B）并部署，三键全部回 off，见[构建](build.md#v36-revert-2026-10-28)。

**仍然成立的部分**：候选由我们自己持有、从不销毁（`g_held_graph`，每进程至多一张），这一点不依赖上面任何读数。因此"装载图停用/销毁"仍按上一节判 **NO-GO**；而"图是否被引擎登记"目前**未知且不可测**，不再作为结论使用。

## G1 源基线（2026-10-23 会话）

同日晚间会话（[r4 日志](diagnostics/display-issues-2026-10-22.md#r4-session)，构建 `5ef6f0520e`、`BehaviorGraphProbe=true`）取得第一份真实目标游戏源基线：**19/19 请求、228 行、单 actor 00000014、无 missing/异常行**。关键事实（request=1 块）：

- `group=container outer-behavior` 与 `group=character behavior` 为**同一指针**（0x29b73b0d900）——源图上外层容器 `behaviorGraph` 与 `hkbCharacter.behaviorGraph` 确认同址。
- 多个 CommonLib 声明的可选成员在真实源图上**确实为空**：`world`、`character.controllerDriver`、`raycastInterface`、`setup.animationBindingSet`、`setup.unscaledAnimationSkeleton`、`behavior.rootGeneratorClone`、`state-machine-templates`、`uniqueIDPool/jobs/allPartitionMemory`（0/0）。`root=BSFadeNode`、`project-db`、`setup.skeleton/data/mirroredSkeleton` 非空。
- 规模：`pose-count=116`、`animation-bones=116`、`bone entries=116`、`foot-ik=1`、`variable-mode=0`；`flags active=true linked=false update-active=false state-changed=true`；`critical-section` 指向模块外静态地址（仅记录，归属未解析）。

这些是当时取得的源坐标，支持所读字段在该样本中的一致性，不证明整套 SDK 布局或全部共享资源安全。此源基线本身不能通过 G1；10-24 的方案 A 修订与候选记录提供了后续有限通过证据，见[当前结论](#current-status)。

## 执行台账与后续证据

各日期行保留当时结果与待办，不能把旧“尚未运行”投影为当前状态；当前结论以[文首](#current-status)为准。

| 日期/项目 | 实际结果 | 仍缺少 |
| --- | --- | --- |
| 2026-10-23 既有修复提交 | `a9a0a20883803bd5295aff9f2ac4e7edf5474cf8`，先提交再开始本文 | 本实验不重写该提交或旧测试包 |
| G1 静态审计 | 已核对当前声明与固定 OAR 源；候选入口存在但原生协议未核实 | 无新 Actor 的创建/链接/深层所有权/停用销毁证据 |
| G1 二进制审计 | 2026-10-23 在解包后的 1.6.1170 镜像上确认五种构造实现、holder 传参与配对销毁；构造不加载项目、不登记 manager。见[审计记录](behavior-graph-construction-audit.md) | 激活/更新入口、事件路由、无新 Actor 复用、共享资源范围 |
| G1 加载链定位 | 2026-10-23 续审计：编排者 32881/32882/32893/32902 → 加载 63585 → 项目解析 63844 → 行为图装载 63845 → SetupAnimEventSinks → manager 挂接，见[审计记录](behavior-graph-construction-audit.md#项目加载与链接入口2026-10-23-续审计) | 候选 holder 复用条件、运行期隔离、激活与事件路由 |
| G1 共享语义 | 2026-10-23 三轮：BG 实例每次新建（58340/58374）、字段经 hkRefPtr 共享容器数据（57010/57011 加减引用）、`hkbSymbolIdMap` 新建、完成对象登记全局表，见[审计记录](behavior-graph-construction-audit.md#加载链共享语义细读2026-10-23-三轮) | 运行中共享数据的写时隔离（`variableValueSet` 等）仍属 G1 运行判据 |
| G1 实验契约 | 2026-10-23 设计 G1a/G1b/G1c 分级、候选 holder、调用边界、24 行观测预算与预注册判据，见[实验契约](#g1-实验契约2026-10-23-设计未实现) | — |
| G1 实验实现 | 2026-10-24 `[Experiment] IndependentGraph=off|construct|load` 与 [independent_graph.cpp](../src/animation/behavior_graph/independent_graph.cpp)；Release 构建通过并部署，见[构建记录](build.md#g1-experiment-implementation-2026-10-24) | G1b 装载运行（见下行） |
| G1a 运行与判读 | 2026-10-24 会话 `construct` 模式：空壳构造 + 引用计数配对销毁成功、源基线逐行全等、无崩溃，通过可观测判据，见[运行记录](#g1a-运行记录2026-10-24-会话) | 注册表计数 UNVERIFIED |
| G1b 运行与判读 | 2026-10-24 会话 `load` 模式：63585 以 `BSFixedString*` 第二实参调用后 0.166 ms 返回 false，装配字段全空；源基线不变、无崩溃，见[运行记录](#g1b-运行记录2026-10-24-会话)；INI 已回退 off | 第二实参真实形态与拒绝原因未定位；需补 69488/63585 错误路径审计 |
| G1b 根因审计（续审计二） | 2026-10-24 指令级确认：第二实参为 `const char*` 完整项目路径（69488 拆分 → 69487 拼接 → 69490 规范化 → 0x31b6580 注册表查表），拒绝路径 0xbb21a9 对空壳为空操作；`projectName` 由 63844 写为文件名段，见[审计记录](behavior-graph-construction-audit.md#第二实参形态与拒绝路径2026-10-24-续审计二) | 契约修订与 G1b 重实现（改传完整路径）；查表键大小写规则未核实 |
| G1b 契约修订与重实现 | 2026-10-24 `load` 分支改为 `behaviorPath + projectName` 完整路径 `const char*` + 第三实参 false（Actor/PC Unk_06 实测 false），新增 `load-args` 内容观测行；Release 构建通过并部署（身份 `…646a0f2064+dirty…20261024T051200Z`），见[构建记录](build.md#g1b-contract-revision-and-reimplementation-2026-10-24) | G1b v2 运行与判读 |
| G1b v2 运行与判读 | 2026-10-24 会话：`behavior-path='' project-name='DefaultFemale'`、`path='DefaultFemale'`，0.289 ms 被拒；源基线不变、无崩溃，见[v2 运行记录](#g1b-v2-运行记录2026-10-24-会话) | 引擎注册键的真实取值来源未定位 |
| G1b 取值与前置链审计（续审计三） | 2026-10-24 指令级确认：69489 剥扩展名（键含扩展名与目录段）、63949 自建 setup（setup 检查非前置条件）、描述符名字来自 REFR vtable 槽 114 的类型键组件查找（键 0x3284b30 为 BSS 不可静态读取），见[审计记录](behavior-graph-construction-audit.md#加载取值与前置链的续审计三2026-10-24g1b-v2-复盘) | race 三个候选字段的运行期内容与命中结果 |
| G1b v3 顺序取值实验 | 2026-10-24 `race-fields` 只读 dump + 预注册优先级顺序尝试（≤4 次、首个成功即停）；Release 构建通过并部署（身份 `…646a0f2064+dirty…20261024T054649Z`，DLL SHA-256 `A3C2213F…`），见[构建记录](build.md#g1b-v3-顺序取值实验-2026-10-24) | G1b v3 运行与判读 |
| G1b v3 运行与判读 | 2026-10-24 会话：首候选 `race-bg-model='Actors\Character\DefaultFemale.hkx'` **一次装载成功**（8.729 ms，装配字段全非空、共享资源同址、projectName 内容一致）；销毁阶段崩溃于 `Deactivate` 读空 `activeNodes`（[续审计四](behavior-graph-construction-audit.md#已装载图析构的崩溃定位2026-10-24-续审计四)）；INI 已回退 off，见[v3 运行记录](#g1b-v3-运行记录2026-10-24-会话) | 销毁判据方案评审（未激活图不可安全销毁）；激活/更新入口未审计 |
| 方案 A 评审与 v4 实现 | 2026-10-24 采纳方案 A：销毁判据重定义（空壳销毁 + 装载图销毁移至 G2 生命周期）、v4 保留流程（永不释放的持有指针，每进程 ≤1 张泄漏）、v4 预注册判据（装载复现 + 装载后源基线全等 + 保留无崩溃 → G1 PASSED），见[方案 A 修订](#2026-10-24-方案-a-修订销毁判据经-g1b-v3-崩溃后评审)；构建身份 `…20261024T065321Z`（DLL SHA-256 `37C255DA…`）已部署 | **G1b v4 运行与判读（按修订判据完成 G1）**；G2 前提：激活/更新入口审计 |
| G1b v4 运行与判读 | 2026-10-24 会话：装载复现（`accepted ms=8.797`）、装载后源基线独立 diff 全等、`retained=true` 无崩溃，同会话面板多轮开合正常，见[v4 运行记录](#g1b-v4-运行记录与-g1-判定2026-10-24-会话)；INI 已收回 off | — |
| **G1 判定** | **PASSED（2026-10-24，方案 A 修订判据）**——空壳配对销毁 + 两次装载成功（装配完整/共享同址/setup 自建）+ 装载后源基线全等 + 保留无崩溃，见[v4 记录](#g1b-v4-运行记录与-g1-判定2026-10-24-会话) | 注册表计数 UNVERIFIED；可写状态写隔离属 G2+；装载图销毁由 G2 生命周期取得 |
| **G2 判定（历史快照，v20a）** | **NOT PASSED（v20a，2026-10-27 22:02）**：入口 `63589(mode0)` 首次到达，输出头填充（双槽 `numData=116 f=1.0`、`tracks-end +32=0x0074 +38=1.0`）、step 2 起 Path B、`arm-observed=yes`、`gen0elem` 与元素哈希变化、候选 raw pose `bc160a8c2b3a5a58`→`63f638a6ac08d955`→`8fb4cd336c4395bf`（120/120 次变化），源侧 pose 不变、三份源基线全等，见[v20a记录](#g2-v20a-run) | 该行只记录当时的判定；现行判定见[当前结论](#current-status)与 [v33 运行记录](#g2-v33-run) |
| G2 v18 取证运行 | 2026-10-26 23:01 会话：加载身份与包内 manifest 一致；`GRAPH FRAME` 30+1、`INDEPENDENT GRAPH` 40、`BEHAVIOR GRAPH` 36（三块逐行全等）；无`[E]`、无崩溃；`source-stable=true retained=true`；部署 INI 已回 off | Path B 的 t 区间来源、63575 原生数量、63568 回填条件、63666/63582/58423 应用链 |
| 审计十六（机器码） | 2026-10-26：record = `BSAnimationUpdateData`（`+0x18=optionalEyePosition`，唯一写入者=REFR vtable 槽 0x3C8 `ModifyAnimationUpdateData`，Actor 0x6a3750 / PC 0x74e7a0 / REFR 空桩 0x2f71a0）；Path B 真条件改为 `+0x2A==0 && t_larger≥0 && t_smaller>-MAXFLT`；arm 块门与 63581 四道门、63568/63582 的写入分工逐条落定，见[审计十六](behavior-graph-construction-audit.md#audit16) | `[graph+0x248]` 语义、`[graph+0x160]` 回退表、63666 输出选择、63582 混合细节、58423 门值、原生帧双槽 f 初值 |
| G2 v19 实现与部署 | 2026-10-26：单变量眼位实验已实现（提交 `e17d5d1`）并部署——注入分支每步记录 `+0x18` 指向 rootNode 位置 +900 的只读 3×float；FRAME 行改 `schema=v19` 并追加 `root218/b248/eye`，summary 加 `arm-observed`；Release 身份 `…v19-eye-arm-20261026-e17d5d12f6…`（DLL `344d9abb…`），三个 engine-free 案例通过；v18 对备份在 `dist/deploy-backup-20261026-v18/`，部署 INI=`inject`（未打包），见[契约与实现](#g2-v19-contract)、[构建记录](build.md#v19-eye-arm-build) | —（运行见下行） |
| G2 v19 运行与判读 | 2026-10-26 23:46 会话：身份与构建一致；FRAME 30+1、IG 40、BG 36，无`[E]`/崩溃，`source-stable=true retained=true`；`root218=0 b248=0`、`eye` 非空、step 0 旧读槽 f=1.0，但 **`arm-observed=no`**、t 恒 `{0,-MAXFLT}`、无 Path B、pose 仍 `bc160a8c2b3a5a58`；日志归档（41,470字节，`45AAEE09…`），INI 已回 off，见[v19记录](#g2-v19-run) | **audit17**：`63864()` 返回值语义、候选 rootNode 缺失；双槽 f 初值待后续 |
| 审计十七（机器码） | 2026-10-26：臂块步长闸门 = 63864（0xbc3990，唯一调用点 0xbb2603），返回 `hkbBehaviorGraph::activeNodes(+0x98)` 上归一化动画状态量的最小值（默认 FLT_MAX）；候选上 ≤0.04583 故 `-0.5*dt` 后必过不了 `1.25*dt`；候选 rootNode 由构造器清零、审计集内无写入者，见[审计十七](behavior-graph-construction-audit.md#audit17) | 63864 字段语义、`0xacf340`/`0xac3400` 返回值、原生图 rootNode 写入者 |
| 审计十八（机器码） | 2026-10-26/27：63602 的生成/更新/复位只作用于第一实参（`r13=rcx`），引擎壳图助手 0xbacc90 与配方同构而壳图未装载 → **63602 从未在候选上生成**（修正审计十三/十四）；真正入口 0xbc3270/0xbc32d0 → 65232（门 `character.behaviorGraph`、`isActive(+0x12C)`、`numStaticNodes(+0x128)`）→ 65237，配方未调用，见[审计十八](behavior-graph-construction-audit.md#audit18) | `0xbb31b0` 上下文、sink/adapter 布局、第二实参六槽语义、候选 isActive/numStaticNodes 运行值 |
| 审计十九（机器码） | 2026-10-27：生成链 `0x53fd20 → 0xba4e30 → 63595 → 63859 → 65232`；`63595` 门 = `[graph+0x200]`（`ProjectDBData*`），按 request id 表查 `projectDBData+0xc8`；sink = **0x10 字节适配器 {vtable, buffer}**（0x1992da8/0x1989320/0x1989360，slot5 才是输出转发），63602 的 buffer 在 `[rbp+0x140]`（约 0xA0 B），见[审计十九](behavior-graph-construction-audit.md#audit19) | slot5 buffer 布局、`0x53fd20` 上层驱动、`69166` 语义、值数组格式 |
| 审计二十（机器码） | 2026-10-27：slot5（`0xc25ee0`）= `65177([ctx+8], buffer+0x38, …)` 解析名字/状态 → 项目名查 `[[adapter+8]+8]+0x28` 哈希 → `59268(buffer, float)`；当时读到的 buffer 字段（`+0x00` 项目名串、`+0x08` 注册表、`+0x38` 子对象、`+0x88` 指针，总 ≥0xA0）后由审计二十一修正；**伪造 buffer 风险不可接受，v20 暂不立项**，见[审计二十](behavior-graph-construction-audit.md#audit20) | 65177 字符串协议、59268 函数体、注册表来源、buffer 完整大小/对齐 |
| 审计二十一（机器码） | 2026-10-27：63602 生成块受第一实参 `behaviorGraph` 守卫（0xbb62ae），壳图路径上 65149 与内部 63587 **都没被调用**；`65177` 拼 `<obj+0x38>/<buffer+0x38>` tagged string，`59268` = `movss [rcx+0xF0], xmm1; ret`；63602 的 sink 是 12 槽栈子对象描述符、与 0xba4e30 的 sink 不同类。**无安全最小生成实验 → 架构决策**，见[审计二十一](behavior-graph-construction-audit.md#audit21) | 是否允许把候选登记进 manager（突破 G1 边界）；第二实参六槽语义、63584/59441 对真图的作用 |
| G3–G6完整关卡 | 未通过/未执行，前关未过不开展完整验收 | 一次WeapEquip注入不算完整G3；装载图生命周期与全局收敛仍未过 |
| G2 v20 重建与部署 | 2026-10-27：原冻结包不在本检出中，按 v18 先例以干净 `a83e522` 重建/打包/部署（DLL `13688a9d…`、ZIP `076cf6f7…`、manifest `a9ae936d…`），v19 三件套备份在 `dist/deploy-backup-20261026-v19/`，部署 INI=`inject`，见[构建记录](build.md#v20-rebuild-2026-10-27) | —（运行见下行） |
| G2 v20 运行与崩溃 | 2026-10-27 21:48 会话：身份与包内 manifest 一致，`GRAPH ENTRY outcome=ready`（guard 通过），随后在 **63589 之前**的 v16 遗留 g3-gates 只读链崩溃——`context+0x50` 越过 `sizeof(hkbContext)=0x50`，`[+0x40]` 得垃圾后 `[+0x88]` 越界；**入口从未执行**，无 FRAME/completed/pose 证据；日志与崩溃报告已归档（`2278201C…`/`4074DF23…`），INI 回 off，不在同会话重试，见[v20记录](#g2-v20-run) | 先修该越界诊断（按 audit23 正确链或移除该行），复查同类写法，再以新身份（v20a）重测 |
| G2 v20a 修正与部署 | 2026-10-27 21:56：g3-gates 改为 `[context]`→setup(+0x50)→data(+0x40)→footIkDriverInfo(+0x88) 的声明成员链，新增 `observation_chain_policy` guard 与对应 engine-free 用例（三个用例全过）；身份 `…v20a-chain-fix-20261027-7d18a90787…`（DLL `691f7592…`、age 321），v20 三件套备份到 `dist/deploy-backup-20261027-v20/`，部署 INI=`inject`，见[构建记录](build.md#v20a-chain-fix-2026-10-27) | —（运行见下行） |
| G2 v20a 运行与判读 | 2026-10-27 22:02 会话：身份一致、无崩溃无 `[E]`；ENTRY 1、FRAME 30+1、IG 40、BG 36（三块全等）；guard ready、g3-gates 四跳可读；首步回填 `numData=116 f=1.0`，step 2 起 Path B、`arm-observed=yes`，`gen0elem` 变化，raw pose `bc160a8c…`→`63f638a6…`(60)→`8fb4cd33…`(120)、120/120 次变化，源侧 `983fc2d4…` 前后不变、`source-stable=true retained=true`；日志归档（43,330 字节 `E05BFEAF…`），INI 回 off，见[v20a记录](#g2-v20a-run) | pose 内容解码契约（具名骨有限性/变化/源对照）；视觉确认另行判断 |
| G2 v21 契约与实现 | 2026-10-27：只读解码 `poseLocal`（声明 `hkQsTransform` 0x30），三点 0/60/120 全量有限性 + 逐条变化 + 源对照 + 别名计数；新增 `pose_content_policy.h` 与 20 断言 engine-free 用例；预算新增 ≤15 行；身份 `…v21-pose-content-20261027-fe7c4d15e1…`（DLL `e0880651…`、age 322），ZIP `4cfa5ac8…`，v20a 三件套备份到 `dist/deploy-backup-20261027-v20a/`，部署 INI=`inject`，见[契约](#g2-v21-contract)与[构建记录](build.md#v21-pose-content-2026-10-27) | —（待上机；判定门槛 `moved≥16`、四骨至少两动） |
| G2 v21 运行与判读 | 2026-10-27 22:18 会话：身份一致、无崩溃无 `[E]`；ENTRY 1、FRAME 30+1、IG 55（40 基础 + 15 pose 行）、BG 36；三点 `entries=116 finite=116 nonfinite=0`，`moved=94`(60)/`94`(120)、`max-delta` 3.43/2.31，抽样骨 38、76 `moved=yes`、0、115 `moved=no`，源侧 `source-moved=0`、`source-pose-before=after=bd107af6…`；**`source-equal=8/21/15` 命中第 3 条 → 停止**；审计判定为误报（候选/源 `poseLocal` 不同址；源哈希两轮不同而候选三点哈希逐位相同；`pose_sample_identical` 为十分量精确值相等）；日志归档（47,393 字节 `601D2B47…`），INI 回 off，见[v21 运行记录](#g2-v21-run) | 判据改为指针同一性 + “相等且在变化”计数（v22），一次运行后再判第 4 条 |
| G2 v22 契约与实现 | 2026-10-27 22:56：`pose_buffer_alias`（`poseLocal` 指针同一性）+ `watch_aliasing`（相等且相对 step 0 变化的条目数），`pose-content` 加 `source-equal-moving`/`source-equal-static`/`alias` 三字段，行数与预算不变；pose content 用例扩到 36 断言、四例全过；身份 `…v22-alias-watch-20261027-22ef58464f…`（DLL `5756e0e6…`、age 323），ZIP `be5bd284…`、manifest `c9878cc7…`，v21 三件套备份到 `dist/deploy-backup-20261027-v21/`，部署 INI=`inject`（1,056 字节 `e7ba0854…`），见[契约](#g2-v22-contract)与[构建记录](build.md#v22-alias-watch-2026-10-27) | —（运行见下行） |
| G2 v22 运行与判读 | 2026-10-27 23:03 会话：身份一致、无崩溃无 `[E]`；ENTRY 1、FRAME 30+1、IG 55、BG 36；三点 `entries=116 finite=116 nonfinite=0`、`alias=no`，`moved=94`(60/120)，`source-equal=21/15` 而 **`source-equal-moving=0`**、`source-equal-static=21/15`（即计数全由常量骨构成，闭合 v21 误报审计），抽样骨 38/76 两点均在动；源侧 `source-pose-before=after=d3f3eeb4…`、`source-stable=true retained=true`；**判据第 5 条成立 → pose 内容证据成立**，G2 仍 NOT PASSED（持续/语义/场景应用未证）；日志归档（47,564 字节 `6C06C504…`），INI 回 off，见[v22 运行记录](#g2-v22-run) | 下一问二选一（待评审）：跨批次持续性，或用 HKX 参照做语义对应 |
| G2 v23 契约与实现 | 2026-10-27 23:14：只读遍历 `activeNodes`（前 256 项），vtable 等于 `RE::VTABLE_hkbClipGenerator[0].address()` 才当片段生成器；`binding` 先过 `plausible_object_pointer` 再读 `animation`；读 `animationName`/`localTime`/`binding`，用 `SamplePartialTracks` 在片段自身时间采样，按 `transformTrackToBoneIndices` 与 `poseLocal` 逐条比较（旋转用 `1-|dot|`）；新增 `clip_reference_policy.h` 与 19 断言用例，`clip-ref` ≤3 行/检查点；身份 `…v23-clip-ref-20261027-ddb58bc269…`（DLL `efa90c21…`、age 327），ZIP `05ac4a3e…`、manifest `2f143a77…`，v22 三件套备份到 `dist/deploy-backup-20261027-v22/`，部署 INI=`inject`（1,072 字节 `655a9c89…`），见[契约](#g2-v23-contract)与[构建记录](build.md#v23-clip-ref-2026-10-27) | —（运行见下行） |
| G2 v23 运行与判读 | 2026-10-27 23:19 会话：身份一致、无崩溃无 `[E]`；ENTRY 1、FRAME 30+1、IG 63、BG 36（`clip-ref` 8 ≤9）；v22 门槛逐字复现；**第 5 条命中**（`agreed=22/99`、`max-dpos=3.983`，且该点 pose 仍是绑定姿态），60/120 两点 `mapped=0`（binding 的 `transformTrackToBoneIndices` 为空）未做成比较、属契约未覆盖状态；结构收获：候选在跑 `H2H_Idle`+`1HM_Idle`+`SCAR_1hmReadyDummy` 多片段栈，三点同步时钟 0.0167/0.1833/1.2000，片段时钟与驱动不同步；日志归档（50,159 字节 `9452BF96…`），INI 回 off，见[v23 运行记录](#g2-v23-run) | 下一问（待评审）：空映射语义审计 + 完整层清单，之后再重跑 |
| G2 v24 契约与实现 | 2026-10-27 23:35：空映射按 audit25 的 identity 语义比较（`mapping=identity|table`），槽位 3→8 并输出 `binding-index`/`map-len`，新增 `clip-clock`（0/20/40/60/80/100/120）；`clip-ref` ≤24 行、`clip-clock` 7 行；身份 `…v24-layer-census-20261027-50c2396d66…`（DLL `13bc2b12…`、age 329），ZIP `68991ca7…`、manifest `7e47d6fc…`，v23 三件套备份到 `dist/deploy-backup-20261027-v23/`，部署 INI=`inject`（1,062 字节 `ac255097…`），见[契约](#g2-v24-contract)与[构建记录](build.md#v24-layer-census-2026-10-27) | —（待上机；门槛：层 `agreed/compared ≥0.8` 或互补栈） |
| G2 v24 运行与判读 | 2026-10-27 23:48 会话：身份一致、无崩溃无 `[E]`；ENTRY 1、FRAME 30+1、IG 71（40+15+9+7）、BG 36；**identity 行全部 `mapped=99/97`、`compared` 满额**（audit25 语义运行期成立，未触发第 2 条冲突）；**第 3 条命中**：`H2H_Idle`（binding-index 681）在 step 120 达 90/99 = 0.909，且 99 条旋转误差全 ≤6.1e-5，差异只在 9 条的位置分量（≤0.187）；`clip-clock` 显示栈在驱动中途切换（step 20/40 仅 `H2H_Equip`，60 起 idle 栈接管、`H2H_Equip` 停在自身 `duration`），idle 时钟单调但每步 0.75×–1.55× 非恒定；日志归档（53,932 字节 `58276FCA…`），INI 回 off，见[v24 运行记录](#g2-v24-run) | 只剩“持续”：需审计 `localTime` 写入者消费的是 dt 还是内部帧计数 |
| G2 v25 契约与实现 | 2026-10-27 23:58：`audit26` 已证时钟只消费 dt；本轮 `clip-ref` 增 `enforced`/`crop-sum`/`eff-duration` 排除常数倍率，新增 `clip-steps` 逐步位串（`.`=0、`1`–`9`、`A`–`Z`、`N`=回退、`-`=缺席，≤8 行）；只改 `independent_graph.cpp`；身份 `…v25-step-clock-20261027-862cebdf1b…`（DLL `d8ca80bb…`、age 331），ZIP `15820120…`、manifest `ba4bc726…`，v24 三件套备份到 `dist/deploy-backup-20261027-v24/`，部署 INI=`inject`（1,057 字节 `4af7e8ee…`），见[契约](#g2-v25-contract)与[构建记录](build.md#v25-step-clock-2026-10-27) | —（运行见下行） |
| G2 v25 运行与判读 | 2026-10-28 00:03 会话：身份一致、无崩溃无 `[E]`；ENTRY 1、FRAME 30+1、IG 76（40+15+9+7+5）、BG 36；**`clip-steps` 位串证明 idle 时钟速率 1.000×**（三条 idle 片段 first=49、72 步推进 72 帧，`H2H_Equip` 49+1=50 帧=自身 duration 后停住）；交付成批（12 步逐帧后改成每 15 步推进 15 帧、中间 14 步为 0）；**v24 的 0.75×–1.55× 判为采样假象并作废**（20 步窗对 15 步周期）；预注册第 1/2 条均未命中，属“总量正确、交付成批”第三类；日志归档（55,345 字节 `FFB672DE…`），INI 回 off，见[v25 运行记录](#g2-v25-run) | 只剩 pose 级逐步持续性：需 per-step pose 哈希 |
| G2 v26 契约与实现 | 2026-10-28 00:09：新增 `pose-steps`（每步读 `poseLocal`、与上一步取十分量最大差、六档字符：`. 1 2 3 4 5`），1 行；只改 `independent_graph.cpp`；身份 `…v26-pose-step-20261028-aed15ad6c0…`（DLL `6c117d47…`、age 333），ZIP `d3d7f1b7…`、manifest `a02e25c1…`，v25 三件套备份到 `dist/deploy-backup-20261027-v25/`，部署 INI=`inject`（1,042 字节 `29692657…`），见[契约](#g2-v26-contract)与[构建记录](build.md#v26-pose-step-2026-10-28) | —（运行见下行） |
| G2 v26 运行与判读 | 2026-10-28 00:12 会话：身份一致、无崩溃无 `[E]`；ENTRY 1、FRAME 30+1、IG 77、BG 36；`pose-steps` **119/120 步 pose 在变**（除首尾两点外档位全 `5`，>1e-2），预注册第 1 条命中 → 可见连续性成立；同轮 v22/v25 判据逐字复现；同时推翻 v25 的“4 Hz 跳帧”推论（pose 采样时间不是那个成批发布的 `localTime` 字段）；**G2 四项要件全部有数据，判 PASSED**（限定单进程单批 120 步，不含场景应用/装载销毁/G3–G6）；日志归档（58,751 字节 `D90DCD77…`），INI 回 off，见[v26 运行记录](#g2-v26-run) | G3 或场景应用的前置契约 |
| G2 v27 契约与实现 | 2026-10-28 00:20：静态接驳——保留候选的 `hkbCharacter*` 进模块静态，`held_display_pose` 产出（骨名→NiTransform），`advance()` 在 `DisplayPose=true` 时用它替代 OAR 采样；不新增原生调用；身份 `…v27-display-pose-20261028-c894c956f8…`（DLL `b973aa5b…`、age 335），ZIP `6a195266…`、manifest `d3545033…`，v26 三件套备份到 `dist/deploy-backup-20261028-v26/`，部署 INI=`inject`+`DisplayPose=true`（1,063 字节 `2834670d…`），见[契约](#g2-v27-contract)与[构建记录](build.md#v27-display-pose-2026-10-28) | —（运行见下行） |
| G2 v28 契约与实现 | 2026-10-28 00:27：`step_display_pose` 每渲染帧一次 `63589(candidate,&mode0)`，第 1/60/120 次打 `pose-hash`；新键 `DisplayStep`；步进失败回落 v27 冻结姿态；身份 `…v28-display-step-20261028-44dcdf4663…`（DLL `e3f9ade8…`、age 337），ZIP `d2965ca8…`、manifest `46ae35ee…`，v27 备份到 `dist/deploy-backup-20261028-v27/`，部署 INI 三键（1,070 字节 `0866cec7…`），见[契约](#g2-v28-contract)与[构建记录](build.md#v28-display-step-2026-10-28) | —（运行见下行） |
| G2 v28 运行与判读 | 2026-10-28 00:47 会话：三条 `pose-hash` **全同**（`7d568c4d…`），120 次调用约 3.35 s、画面不动 → 预注册第 2 条命中：**只调 63589 不推进**；批量每步的 63555/63602/63556 才是推进来源，日志归档（59,181 字节 `3cde4194…`），见[v28 运行记录](#g2-v28-run) |
| G2 v29 契约与实现 | 2026-10-28 00:50：`step_display_pose` 补上 63555→63602→63556 再 63589；身份 `…v29-display-pipeline-20261028-04903ed263…`（DLL `ce712e20…`、age 339），ZIP `42f92e94…`、manifest `12970d32…`，v28 备份到 `dist/deploy-backup-20261028-v28/`，部署 INI 三键（1,095 字节 `468fb691…`），见[契约](#g2-v29-contract)与[构建记录](build.md#v29-display-pipeline-2026-10-28) | —（运行见下行） |
| G2 v29 运行与判读 | 2026-10-28 00:54 会话：三条 `pose-hash` 仍全同，且与 v28 **逐位相同**（`7d568c4d…`）→ 补上 63555/63602/63556 后图状态零变化，说明渲染路径里的调用根本没生效，不是推进幅度问题；日志归档（59,223 字节 `57eb0b22…`），见[v29 运行记录](#g2-v29-run) |
| G2 v30 契约与实现 | 2026-10-28 00:56：只加门诊断（`active/children/b245/b246/w240/c110/c208/numData0/numData1`，call 1 与 60 各一行），行为不变；身份 `…v30-display-gates-20261028-0829b1d26d…`（DLL `e5b0e57c…`、age 341），ZIP `1ce2b765…`、manifest `cdebfff0…`，v29 备份到 `dist/deploy-backup-20261028-v29/`，部署 INI 三键（1,068 字节 `50adad10…`），见[契约](#g2-v30-contract)与[构建记录](build.md#v30-display-gates-2026-10-28) | —（运行见下行） |
| G2 v30 运行与判读 | 2026-10-28 00:58 会话：显示哈希仍冻结，批量/显示同线程；日志归档 54,987 字节 `0F7B25DE…`，三键回 off。后验复核：active/children 基址错误、numData 少解引用，不能判图未 arm；显示也遗漏 63587 等完整步骤。见[v30 更正](#g2-v30-run) | 优先验证 [v31 共用单步](#g2-v31-correction)，缓存为备选 |
| G2 v31/v32 离线 | 2026-10-28 白天：v31 共用完整单步与修正读法、v32 来源/窗口/语义运动/实际写骨，两个新 engine-free 场景各 2 例通过，Release 构建通过；**产物未留存**（`build/Release` 仍是 v30，检出内无 v31/v32 目录或包） | 重建后上机 |
| G2 v33 重建与部署 | 2026-10-28 22:20–22:23：以 HEAD `d323b35` 重配新 label 构建（干净树，无 `+dirty`），DLL `061d5b8a…` 1,319,424 B / PDB `5924464d…`；同一树重跑两个离线场景各 2 例通过；v30 三件套备份至 `dist/deploy-backup-20261028-v30/` 后部署，三键开。见[构建](build.md#v33-rebuild-2026-10-28)与[部署](#deploy-state) | —（运行见下行） |
| G2 v34 边界会话 | 2026-10-28。22:47 会话：开包 1 跑满 120（未在 120 前关）、开包 2 重采基线且不拼接、开包 3 中途 F7 → `steps=84 reason=retarget source-comparison=discarded` + view 2→3、读档后开包 0 条 display 行；日志 83,924 字节 `23637075…`。**23:00 补测**：开包 1 在 call 61 关包 → `steps=61 reason=reset source-comparison=discarded` 且无摘要，开包 2 从 call 1 独立跑满 120；日志 64,003 字节 `bc7f27c4…`。两次均无 `[E]`。附带：`native-step` 三点哈希与 v33 逐位相同而源 before 不同 → 候选轨迹可复现且与源无关；`SCOPY PARK` 累积 4 份。三键已回 off。见[v34 记录](#g2-v34-boundaries) | 同 controller 来源/generation 替换、frozen-only 拒绝和确切原因未验；新诊断见 v42 `window=rejected`。装载 teardown NO-GO，其余收敛仍属 G6 |
| G3-A 源图时间线 | 2026-10-28 23:41：新增 `source_timeline.cpp`（post-UI 每帧只读源图，diff 652 个 word 变量 + 活动节点 count/hash，≤200 行/进程），`[Experiment] SourceTimeline` 开关，构建 v37（`…v37-source-timeline-20261028-81578f5c63…`，DLL `7f3bd92e…`）并部署。见[契约](#g3-a-contract)、[构建](build.md#v37-source-timeline-2026-10-28) | 未上机；本构建未加离线用例（native glue，无 policy 头可挂） |
| G3-A v37 运行（未采） | 2026-10-29 00:05：`armed words=463 nodes=36`，但 200 行在 8.5 s 内被 `TimeDelta`(76 行) 与每帧节点哈希(91 行) 吃光，装备/卸下未进日志；日志 23,521 字节 `29ffef2a…`。附带真事实：**候选 652 vs 源 463**，回放必须按名字映射。见[v37 记录](#g3-a-contract) | 仪器规则修订 |
| G3-A v38 修订 | 2026-10-29 00:09：settle 规则（连续 3 个采样不变才写）+ 节点只记 count；身份 `…v38-source-timeline-settle-20261029-ad15b58ae8…`，DLL `17567d57…`；v37 三件套备份到 `dist/deploy-backup-20261029-v37/`，已部署，`SourceTimeline=true` | 未上机 |
| G3-A v38 运行（未采） | 2026-10-29 00:11：`armed words=463 nodes=36`，预算撑到 18.2 s 后耗尽，装备未进日志；日志 22,707 字节 `6f7055bb…`。发现源图 **~1.2 s 周期循环**（`bVoiceReady`/`BlendJump`/`bIsInMT`/`iLeftHandEquipped` + 节点数 22↔27↔44↔54），常开预算记录必然被它磨光 | 改为显式窗口 |
| G3-A v39 窗口 | 2026-10-29 00:17：F9 开 20 s 窗口（在 scene-graph 门之前处理，正常游戏可用）、自动关闭、预算 800；身份 `…v39-timeline-window-20261029-5f2cf53de6…`，DLL `5f2f660a…`；v38 备份到 `dist/deploy-backup-20261029-v38/`。**F9 与游戏快速读档冲突，本轮作废** | 换键重发 |
| G3-A v40 换键 | 2026-10-29 00:19：时间线窗口改绑 **F7**（仅在无面板时；面板内 F7 仍为重捕获），构建 `…v40-timeline-f7-20261029-58d25af47b…`（DLL `5f791d2c…`），v39 备份到 `dist/deploy-backup-20261029-v39/`，已部署 | 未上机 |
| G3-A v40 运行（空采） | 2026-10-29 00:21：`SourceTimeline=true` 已加载，但按 F7 只出现 `SceneGraph ... command=1 ignored outside paused inventory`——**场景图对象在面板外也存在**，所以"graph 为空"不能当面板关闭的判据，F7 落进了重捕获分支。日志 4,035 字节 `e6c623a7…` | 改用面板状态判定 |
| G3-A v41 门修正 | 2026-10-29 00:24：改用 `PanelMonitor::is_menu_open()` 判定——面板外 F7 开窗口，面板内 F7 重捕获；构建 `…v41-timeline-panel-gate-20261029-9bc1a4b03a…`（DLL `3b8f8efe…`），v40 备份到 `dist/deploy-backup-20261029-v40/`，已部署 | —（运行见下行） |
| G3-A v41 运行（成立） | 2026-10-29 00:26：`window-open words=463` → 117 行 → `window-closed`；装备（46.634–48.159）与卸下（53.652–54.933）两段所选 settled 相位轨迹已取得（非完整外部输入 trace），`iState` 0→9→0、`currentDefaultState` 0→10→1→12→0、`IsEquipping`/`IsUnequipping` 各一段脉冲；日志 15,705 字节 `bc48e764…`。**G3-A 成立** | G3-B 回放契约 |
| G1 遗留审计 + v35/v36 | 2026-10-28 23:11–23:18：指令级确认 `Deactivate`（`0xab1f70`）对 `activeNodes` 无空检查、`context+8` 非空即早退、释放 `variableValueSet` 并置 `isActive=false`；引擎只在 `BShkbAnimationGraph` 析构里配对调用；候选与源三份日志 `root-generator` 同指针 → **销毁判 NO-GO**。**v35 探针证伪**：23:16 读回 `registry=300474432->300474432`，重审确认 `+0x228` 是不变奇偶值、`inc` 未写回、`63529` 是无锁追加而非登记 → 旧"登记计数"结论作废，判无锚点。`1ca2ad6` 已 revert（`cee948d`），构建 **v36**（`…v36-registry-probe-revert-20261028-cee948dcf8…`，DLL `ac01ac95…`）并部署，三键回 off。见[审计与更正](#g1-teardown-audit)、[v35 构建](build.md#v35-registry-probe-2026-10-28)、[v36 构建](build.md#v36-revert-2026-10-28) | —（该项结清） |
| G2 v33 运行与判读 | 2026-10-28 22:28 会话：单窗口 `request=1/view=1/generation=1`，call 1→120 未中断（3.32 s）；**语义 120/120、applied 109/116（不变量自洽）、源 before=after=`23dd49fed3fbf888`、画面连续**；门 9 行全 `active/children=true numAnim=numData=116`，运行期反证 v30 的读错；`DAC restore matched`，无 `[E]`；日志归档 57,004 字节 `11462f1c…`，三键回 off。见[v33 记录](#g2-v33-run) | 边界（中断/重开/F7/来源失效）、装载销毁、G3–G6 |
| G1 源基线准备 | 默认关闭采集器；Release 构建、16 项有界静态/文档检查、23 个新增相对链接及 DLL/PDB/ZIP 核验通过，见 [Build](build.md#behavior-graph-baseline-checks-2026-10-23) | 实际目标游戏日志，尚未运行 |
| G1 源基线采集 | 2026-10-23 会话 19/19 请求、228 行、无异常，见 [源基线](#g1-源基线2026-10-23-会话)与 [r4 日志](diagnostics/display-issues-2026-10-22.md#r4-session) | —（后续证据见 G1/G2 各行） |
| G1c | NOT STARTED | 按方案 A 修订跳过：`holder=nullptr` 已证充分，源 holder 别名量化移至 G4 |
| C-G4-A4 分型仪器 | 2026-11-02：契约 `e97eb65` → v63 构建/部署（DLL `2c81435c…`）→ 运行：实验窗 `sites=n:72` 对应 `numeric=t0:72/t3:0/un:0/sum:72`，31/31 行对账，65 次命中在 32 行明细预算之外计数 → **A 缺口关闭** | `t3>0` 分支本窗未触发（属未覆盖状态，非否证）；结论限本窗 |
| C-G4-C1 并发观察器 | 2026-11-02/03：评审 + 实现（v64）→ 尝试1 跨解析器图不一致中止 → v65 改共用解析器 + 补诊断 → 尝试2 机制全通但观测错图（463/36 vs 652/106）→ v66 钉住候选源图 → 尝试3 `calls=62/other-graph=55`、源图 `events=0`、`display-close` → **C 缺口关闭** | 58377 之外的 manager 级事件路由未覆盖；两次无效尝试保留为反例 |

<a id="next-audit"></a>
## 下一步有界验证问题

历史 v20 前置问题（当前排序由下方交接覆盖）：[audit23](behavior-graph-construction-audit.md#audit23) 提供输出头、三条 poseLocal 复制和场景应用的静态分工。先比较入口/输出 n/f，再比较 raw pose 与有效载荷，最后查骨数组/场景应用和源控制；f/t 只限制插值，不应在 raw pose 不变时直接认定它是唯一原因。现有 v20 没有载荷/原生分支/node 应用 trace；若继续有界静态工作，关注完整 unwind 组下的 58343/58376 活动生成器前提及 63582 混合/flags，额外诊断另行授权。

历史 v20 选择[v20受控原生入口](#g2-v20-generation-entry)，其机器码依据与manager可选性见[audit22](behavior-graph-construction-audit.md#audit22)。先取得相同v19配方改用63589(&mode0)的头部/pose/源控制证据，再决定实际generation/output/apply问题；不先登记manager或制造sink/种子。旧audit19/20方向已由后验审计更新，原记录保留。

早期归档运行：G3-A v41（2026-10-29 00:26）已取得源图 Equip→Idle→Unequip 的 settled 相位轨迹（非完整外部输入 trace）；G2 的显示窗口与四条边界均已成立。**后续问题不再在这里列**，改由[当前证据交接](#handoff-20261102-evidence)统一排序（G4 A/B 保留、C 待重跑 → [G5 第一门 E1–E3 证据工作](#g5-evidence-plan) → G6）。

<a id="handoff-20261102-evidence"></a>
## 当前证据交接（更新至 2026-11-04）

本节接替下方历史摘要；**当前裁定以 [E1 修复后边界](#g5-e1-evidence-repair) 为准**：G4 A/B 保留，C 待修复仪器重跑；v67/v68 证明采样入口活性，不能关闭 E1 的产物完成、寿命与同代快照缺口；**修复版 v69 两轮已上机（2026-11-04 夜，[第二轮](#c-g5-e1-v69-run2)转段完整）——修复版 E1 取证缺口（入口活性/配对/对账恒等式）已闭合，输出完成/寿命/同代快照仍待原生审计**。门 1 NOT_READY；先完成修复版取证核验与 E1 剩余原生审计，再依赖 E2/E3、联合验收。

> **当前关闭记录（2026-11-04）**：PENDING 1 的 A4/v63 numeric 分型保留关闭；PENDING 2 的 C1/v66 关闭因 [audit30](behavior-graph-construction-audit.md#audit30) 仪器缺陷撤回，仍需修复后重跑。下方历史裁定不覆盖这一更正。

- **已证**：G1 修订方案 A、G2 v33/v34 有界通过；G3-A 有源 settled 相位，G3-B/[F](#c-g3b-f-full-replay)以 `WeapEquip`(2) + `Unequip`(144) 两次外部注入复现 0→10→1→12→0，装备/卸下段分别 step 49/21 自退，`source-drift=none`。S3/S4/S5 的负结果仅否定各自名字/写集/时机，E1 零 58377 命中仅否定该观察路由，不能推出无外部事件。
- **资源与活态分开**：候选与控制器源图共享 `hkbBehaviorGraphData`（652 项、对齐 652/0/0）及 15,203 项绑定集，候选变量活值分储、激活前不存在并从 authored defaults 初始化。共享绑定、候选跨轮 pose 哈希相同只支持资源对应/候选确定性，不证明源候选完整姿态、时钟或全部层/权重一致；`owner=neither` 也不单独识别读值来源。
- **G4 当前证据范围**：A4 整窗 72/72 numeric 全 type0、31/31 行对账保留；C1/v66 的 `events=0/other-graph=55` 仪器无效，仅门前 `calls=62` 保留，修复后待有效正控重跑。B-v2 只证明当前装备族稳定；捕获、EquipSink、DAC restore 也不等于完整生命周期收敛。
- **记录部署**：最新为 [v69](build.md#v69-g5e1-clean-rebuild-2026-11-04)（修复版，DLL 1,590,272 字节 `2490c38d…`，CodeView age 397），运行 INI 用后已恢复 dormant `27f481f5…`（[部署与两轮运行记录](build.md#v69-g5e1-clean-rebuild-2026-11-04)）。v46 正常工作流仍仅对应 20261030 重建。[DCMF](community-reference-supplement.md#dcmf-reference) 仅参考/后置；[idle-start 每圈重放](animation.md#idle-start-replay) 仍是独立未决问题。
- **排序**：~~修复版离线核验 → 匹配工件上机核验入口活性与完整传送/关闭对账~~（**已完成**：[v69 首轮](#c-g5-e1-v69-run1) + [第二轮](#c-g5-e1-v69-run2)）→ ~~E1 剩余完成点/寿命/同代快照证据~~（**静态四项已由 [audit32](behavior-graph-construction-audit.md#audit32) 闭合；运行侧 [v3 契约](#c-g5-e1-v3) 已由 v71 一轮通过**）→ E2 完整贡献、E3 合格 R2 配对 → 联合验收；之后才是完整仪器、独立校准、协议冻结与新正式验证。~~G4-C 与旧 setter 仪器另需有效正控重跑~~（**G4-C 已完成**：[v69 attempt 2](#c-g4-c1-concurrent-events)；typed-setter 窗口 63607/63609 的同类修复正控仍待需要时重跑），G6 后置。

<a id="g4-followup-numeric"></a>
### PENDING 1（**已关闭**，2026-11-02）：A 的全窗口 numeric 类型覆盖

**缺口证据**：[A3 原日志](diagnostics/CharacterPanel-g4a3-scan-anchor-20261031-2345.log)汇总 `sites n=72`，但全窗只发射前 32 条总求值明细，其中 **7 条 numeric**（原行 125/130/132/134/136/138/139）全为 `type=0`。旧“25 行采样/72 次全 type0/零图变量读取”不由日志支持；剩余 65 次 numeric 的类型未知。`numeric` 表示进入 numeric 访问器，只有 type3 才是这里的图变量访问类型。

**目标与入口**：[hooks.cpp](../src/animation/oar/hooks.cpp) 的 `numeric_thunk`、`note_oar_context_hit`（先 tally，后 `evaluations>32` 截明细）与 [candidate_window.h](../src/animation/behavior_graph/candidate_window.h) 的 `CandidateWindowTally`、`note_candidate_window_hit`、`arm_candidate_window`/`take_candidate_window_tally`。现有 tally 只有 site/owner，没有 numeric 类型总数；未来仪器须按窗口统计所有 numeric 类型（含无法分类项），独立于明细行预算，类型总和与 numeric-site 总数对账，arm/take/reset 后不串窗，并可区分 type3 的来源归属。

**前提与有界验收**：另行评审并批准仪器实现范围；不在本次增计数器。未来只选一个能击中盲点的聚焦回归：**在前 32 条总求值明细之后放入 type3 命中**，确认明细被截仍计入全窗总量，并在下一窗口验证清零/对账；随后匹配 DLL/PDB/INI，在完整候选回放及显示窗回传总量。type3 出现时按变量名、REFR、图/调用作用域追溯源/候选/其它来源，**不能自动判污染**；无法归属保留 PENDING。零 type3 的结论也只限定已证明完整的观察窗口。

**停点与回传**：总数不对账、跨窗残留、计数/身份缺失或出现未归属 type3，即停止验收、保留原始记录回评审；源漂移、崩溃或非有限值立即停。回传独立实现契约/差分、上述一个聚焦回归结果、匹配工件/设置身份、带 window/segment 边界的完整日志及分型对账/归属结论；不能只给前 32 行。

**关闭（2026-11-02）**：契约、判据与实现见 [C-G4-A4](#c-g4-a4-numeric-types)。v63 运行（22:32–22:37，[日志](diagnostics/CharacterPanel-g4a4-numeric-types-20261102-2232.log) 567,481 字节 `59fa75ca…`）给出整窗分型：实验窗 `evaluations=2037 sites=n:72` 对应 `numeric=t0:72/t3:0/un:0/sum:72`，**31/31 行 `sum == sites n:`**，其后 12 轮显示窗全 0（无跨窗残留），明细仍精确 32 行 → 65 次 numeric 命中在预算之外被计入。修订解释下的"零图变量读取"因此覆盖完整窗口；缺口的原始证据描述保留如下，不再代表当前状态。

<a id="g4-followup-source-events"></a>
### PENDING 2（**关闭已撤回、待重跑**，2026-11-04）：C 的兼容源事件观察窗口

**缺口证据**：[B+C 原日志](diagnostics/CharacterPanel-g4bc-run-b-20261101-0028.log)原行 14/21 为 `SourceEventCapture=false`、`install-skip reason=off`，没有源事件观察器。并且 [source_event_capture.cpp](../src/animation/behavior_graph/source_event_capture.cpp) 的 `install_source_event_capture` 明确拒绝 `IndependentGraph!=off`，SourceTimeline 另有优先门；F7 的既有 `sample_source_event_capture` 生命周期面向面板外采集，不能假定覆盖并发 inject+display。**不能仅打开一个 INI 键重跑，也不得删守卫规避冲突。**

**第一步与前提**：先另立评审范围，核实兼容观察器的安装/武装/窗口生命周期、线程与 source graph/actor 身份、有效路由、重置/关闭和候选活动共存条件；形成批准的仪器设计后才可实现和运行。AE58377 仅为当前已定位的观察路由，[E1](#c-g3b-e1-event-capture)配合 [A1](#c-g3b-a1-structural-audit)已示其覆盖不到全部 manager 级事件路径；若欲扩大结论须另审对应路由证据，不凭该零计数扩大。

**有界验收**：匹配 build/DLL/PDB/INI，保留实际 installed、armed、actor/graph/window 身份与 scope；用已知经该路由到达的事件作为**观察器存活正控**，正控失败则零记录无效。证明观察窗无缺段、无丢行/溢出，覆盖**两次候选注入、两段 settle、显示与 close/reset**，边界前后可对齐。记录/分类同期无关源事件（例如正常步频），验收关注候选活动导致的源路由泄漏，**不要求全世界事件静默**。任一零计数结论只适用于已证明活跃/完整的观察范围；58377-only 的零不能证明所有 manager 事件均不存在。

**停点与回传**：门冲突未解决、安装/武装不明、正控失败、源身份变更、缺窗/丢行或候选相关源事件无法归因，停止并保持 PENDING；不得放宽守卫、改写旧日志或自动归类污染。回传兼容生命周期评审/实现契约、路由与正控证据、工件/设置身份、含两次注入/settle/display/reset 边界的完整源/候选日志和无关事件分类表，明确路由未覆盖项。

**关闭（2026-11-03）**：评审、判据与三轮实现见 [C-G4-C1](#c-g4-c1-concurrent-events)。第三轮（v66 钉图版，[日志](diagnostics/CharacterPanel-g4c1-attempt3-pinned-graph-20261103-0010.log) 137,368 字节 `d2fa2b03…`）在同一窗口内给出：`window-open` 的 `data=0x1c4b4cb7510` 与 `pre-activate behavior-data` 逐位相同（观测对象确为候选源图）、武装早于两次派发、`calls=62 / other-graph=55`（正控成立）、`events=0 / ring-dropped=0`（源图零事件）、`reason=display-close`（覆盖显示与关闭边界）。前两轮无效（v64 解析器不一致中止、v65 观测错图）与修正过程一并保留在 C1 节。缺口的原始描述保留如下，不再代表当前状态。

四份已替代的 S4/S5 负日志只从工作树退役；逐轮结果保留，CRLF 与 LF 身份、不可变恢复提交及保留理由见[诊断索引](diagnostics/README.md)。

<a id="handoff-20261101-g4"></a>
## 当前状态与后续规划（2026-11-01 G4 收官交接）

> **2026-11-02 前向更正（本节为历史裁定）**：下文 PASSED、“零图变量读取”“世界历史零消耗”“不阻塞 G5”保留当时判读，当前以[新交接](#handoff-20261102-evidence)为准：A 仅 7/72 numeric 有类型明细（该缺口已于同日由 [C-G4-A4](#c-g4-a4-numeric-types) 的 v63 运行补齐）；B 仅当前装备族稳定；C 未启源事件观察器且现有门不兼容 inject。两项证据当时 PENDING，现只剩 C；G5 运行验收后置；共享绑定不证明姿态/时钟/层一致。

> **新会话从这里开始。** 本节接替 [2026-10-31 深夜交接](#handoff-20261031-night)：其排序 G3-B→G4→G5→G6 已推进至 **G4 PASSED（限定范围：A/B/C 三段）**，下一阶段 **G5（完整姿态对照）**。逐轮记录以 [C-G4 契约](#c-g4-context-isolation)节为准，本节为合并摘要。

### 一、本阶段做了什么（2026-10-31 深夜 → 11-01 凌晨，契约 + 三版仪器 + 六次游戏会话）

| 轮 | 内容 | 判定 |
| --- | --- | --- |
| C-G4 契约 | 代码级核实六项静态前提（显示/产品互斥、投影硬编码、候选直调引擎入口、空壳 holder、变量值分储、既有观察面）+ A/B/C 三段实验设计 | 预注册（`85f287b`） |
| v60（运行 1） | 仪器：命中台账 + 窗口守卫 + 抑制计数 + load-bindings 行；回放第四次复现，但 `evaluations=2037`（预期 0）→ A 未决 | 计划外证据类 |
| v61（运行 2，A2） | 分站点计数 + activeNodes 图级归属 + clip 名字 + 四段快照：激活零命中、逐帧绑定非 idle 剪辑（~13/3.4 每步）、idle 期零；clip 名字 = H2H_Equip/Idle；owner 全 neither；numeric 名字空 | 问题①已决、②半 |
| v62（运行 3，A3） | numeric type 值 + 扫描锚 + 活计数：扫描健全（cand 97 / src 106 与 DriveState 互证）、type=0 全部（**零图变量读取**）→ H1（模板级评估）成立 | **G4-A 关闭** |
| B 段 A/A v1 | 拔刀-收刀序列：环境 idle 交替 + 初态漂移 + 拔刀态不可见 → 基线未过 | 按预注册规则重设计 |
| B-v2 预注册 | 热键装备三动画族（弓→剑→法术，弓先行清左手自锚定）；开背包自动收刀语义经用户执行确认 | 重新预注册（`566ade8`） |
| A/A v2 | 4 轮 11 行：法术槽逐项全同、弓/剑族稳定、交替尾缀（Start↔Loop、Loop↔Loop2）在**无预览**下漂移＝与预览无关 | **基线通过**（`401a5c3`） |
| 运行 B + C | 回放+显示 ≥20 秒 → 装备轮三槽逐族吻合；捕获 5/5 accepted、retained-total 2→4 在规则内、修订 10→14 EquipSink 存活、DAC restore matched ×3 | **G4-B/C 过，G4 收官**（`db13af4`） |

### 二、合并裁定（机制增补 + G4 验收）

- **G4 PASSED（限定范围）**：A＝OAR 求值语境隔离；B＝世界历史零消耗；C＝关闭清理与 sink 作用域。判据与逐轮证据见 [C-G4 契约](#c-g4-context-isolation)。
- **候选↔OAR 交联机制（新，替代 A 段未知）**：候选驱动期间，OAR 装载期记录对**共享模板树上 clip 的替换条件逐帧评估**（非 idle 剪辑 ~13/3.4 每步、idle 期零），全部 REFR=null；**零图变量读取**（numeric 72 次全 type=0）、**零源侧接触**（owner=src:0、overlaps=0、源哈希四轮逐位不变）。所触碰对象为 OAR 装载期写入的共享模板级替换数据——不是任一图的运行时变量/激活状态。
- **显示/产品互斥贯穿**：运行 B 的回放+显示共存态下，武装窗口内零产品查询产物行、显示窗口零求值——隔离结论在更难条件成立。
- **开背包＝收刀（游戏语义，用户执行确认）**：拔刀态活不到面板快照——录制型动作必须用持久状态（装备族），已写入 B-v2 协议。
- **环境 idle 交替尾缀（Loop/Loop2/Start）＝本底方差**：A/A v2 证其在无预览下照常漂移；比对判据定为 `(original, oar-mod, oar-submod 族)` 粒度。
- **load-bindings**：候选与源共享同一绑定集对象（15203 项，`setup=different bindings=same`）——**G5 同剪辑对齐前置满足**。

### 三、残余（不阻塞 G5）

1. numeric type=0 的具体语义（已决非图变量型；具体值源未标注）。
2. 模板级评估是否写 OAR 内部状态（Random/变体历史）——间接；G4-B 的世界选择零漂移已部分覆盖，G6 可继承。
3. 652/8223 合并矛盾（未变；共享绑定集事实为其提供了新观察面：OAR 装载期写图内容已有载体证据）。
4. G3 遗留两项：候选侧 iState≡0、currentDefaultState 引擎镜像机制的声明来源。

### 四、下一阶段：G5（完整姿态对照）→ G6

- **G5 第一步仍是预注册**：位置/旋转/缩放误差阈值及依据事先固定（不得看结果后放宽）、逐骨覆盖判据（全部有效贡献层/权重/混合/速度，按有效骨名称语义证明）。确定性锚点：G3-B pose 哈希（装备段 `bc160a8c/63f638a6/c407cb4e`、卸下段 `8fb4cd33/4f2aebde/c407cb4e`，四轮复现）；直接前置：B-v2 装备族序列、load-bindings 共享绑定集。
- G5 后 **G6（生命周期与资源恢复）**，排序不变；装载 teardown 维持 NO-GO。

### 五、反模式（本阶段新增三条，接 10-31 清单）

13. **A/A 基线必须先于 A/B**——B-v1 的拔刀序列栽在自己的基线上（环境交替 + 初态漂移 + 拔刀态不可见）；排除判据的有效性只能由 A/A 证明，不能假定。
14. **派生 INI 也要逐键断言**——dormant 默认值残留 `CandidateInputVariable=IsEquipping` 混过 sed 派生，靠写后打印回读抓住（反模式 11 的派生形态：派生≠写完，两者都要断言）。
15. **运行步骤表里的期望值不是判据**——运行 1 步骤表写了"预期 evaluations=0"而契约判据是 overlaps=0；裁定跟契约不跟期望值，计划外结果以微切片（A2/A3）收口而非放宽。预注册时把"仪器看不见"的分支（负路径）写明。

### 六、部署与备份

- 留装：v62 DLL `cdb8a5ed…`（1,461,760 字节，身份 `…-v62-g4a3-scan-anchor-20261031-2dbe98c785-…`）+ PDB `a7020045…`；部署 INI dormant（2,147 字节 `27f481f5…`，全键 off）。
- 备份链新增：`dist/deploy-backup-20261031-v60/`（v59 三件套 + F 前 dormant）、`-v61/`（v60 三件套）、`-v62/`（v61 pair）。
- 分支 `oar-idle3` 本地领先远端 **55 个提交**未推送（契约 + v60–v62 三版仪器 + 六份运行记录 + 两份交接）；推送由用户决定。
- 日志：[运行 1](diagnostics/CharacterPanel-g4a-oar-context-20261031-2258.log)、[A2](diagnostics/CharacterPanel-g4a2-clip-identity-20261031-2330.log)、[A3](diagnostics/CharacterPanel-g4a3-scan-anchor-20261031-2345.log)、[A/A v1](diagnostics/CharacterPanel-g4b-aa-baseline-20261101-2355.log)、[A/A v2](diagnostics/CharacterPanel-g4b-aa-v2-20261101-2358.log)、[运行 B+C](diagnostics/CharacterPanel-g4bc-run-b-20261101-0028.log)。

<a id="handoff-20261031"></a>
<a id="handoff-20261031-night"></a>
## 当前状态与后续规划（2026-10-31 深夜交接，接替晚间交接）

> **新会话从这里开始。** 本节接替 [2026-10-31 晚间交接](#handoff-20261031)：其"下一阶段唯一工作包 = 图内结构静态审计"已完成并当晚收敛为 G3-B PASSED，结论以本节为准；该节的合并裁定、反模式与部署条款继续有效。测量以[验证总表](#verification-matrix)为准；本轮逐轮记录见 [A1](#c-g3b-a1-structural-audit)、[S6](#c-g3b-s6-unequip-event-retry)、[F](#c-g3b-f-full-replay)。

### 一、本阶段做了什么（当晚三连：A1 → S6 → F，零游戏侧新仪器）

| 轮 | 内容 | 判定 |
| --- | --- | --- |
| A1（离线，无游戏） | 路线 A 准备 + 全量结构提取：HKX2-Enhanced-Library `91fa5b0`（C# net10）+ 自建转储器 `build/hkx-audit/HkxDump`；运行时行为链锁定 Pandora_Output（MO2 冲突解析 + S3 日志 load-try 佐证），引用闭包 23 文件 | 装备机制全景解出（见二） |
| S6（v59 上机） | A1 指认 S3 判负根因 = 注入了错误事件名（`1HM_Unequip`=15 不是任何 Default_State 转移的事件）；新键 `CandidateUnequipEvent` 换名重跑同构切片，注入 `Unequip`(144) | **命中**：注入当帧 1→12，step 21 自发回 0 |
| F（v59 原二进制，纯 INI） | G3-B 本体验收：S1 探针观测模式（空变量名）补装备段轨迹，全序列判据重跑 | **命中**：G3-B PASSED（限定范围） |

### 二、合并裁定（机制全景 + G3-B 关闭）

**装备生命周期机制全景**（全部有静态 + 运行期双重证据）：

- 驱动者 = `0_Master.hkx` 内 `root/Root/mg/msg/msg/msg/Default_State`（9 态机）；运行时 `currentDefaultState` 的 0/10/1/12/2/13 值即其状态 id（0=MT 未装备、10=Weap_Equip、1=Weap_Readied、12=Weap_Unequip、11/13=Magic 两段、2=Magic_Behavior）。
- 外部输入子集（**G3-B 正式定版**）= {`WeapEquip`(2)，`Unequip`(144)}：0→10 与 1→12 各由一次事件注入驱动（转移条目 `cond=null flags=256=FLAG_DISABLE_CONDITION`，无时间窗；1→12 备选边 `BoundWeapUnequip`=982）。
- 段内转移 10→1（step 49 逐点实证）与 12→0（step 21）均由**剪辑尾触发器**（`relEnd=True`：weapequip 的 `WeapEquip_Out`(49)、卸下剪辑的 `Unequip_Out`(50)）图内自发完成——外部只需两次注入。
- 装备/卸下锚的全部脉冲变量（`IsEquipping`(110)、`IsUnequipping`(111)、`bEquipUnequip`(125)、`PitchManualOverride`(65)）= 嵌套机器 `isActive`/`EventDrivenModifier.enable` **绑定输出**；`iState`(94) = 引擎 `BSiStateTaggingGenerator.iStateToSetAs` 维护（源侧 0→9→0，候选侧恒 0）。
- `currentDefaultState`(113) 的 10/1/12 写者静态不可见（全链唯一 currentStateId 绑定在 DragonRider）——判定为引擎装载期镜像机制，声明来源为残余未知；v52/F 运行期证明写者确在图求值内部。
- 事件 144 的真实派发不经 58377（E1 零事件 + A1 证明图内无发射点）→ **存在 manager 级动画事件派发路径**，为 E1 预注册时就记录的残余；候选侧注入用插件自己的 `event_dispatch` 不受影响。
- 运行时合并表 652/8223 无法由静态 23 文件并集（526/9332）重现（2^20 子集穷举无解）——指向 OAR 系运行时改图；不影响本轮结论（装备链全部名字/索引在 0_Master 主表且与 C-VC1 运行时逐项一致）。

**G3-B PASSED（限定范围：相位序列回放）**——F 运行以两次注入在候选自身状态机走完 0→10→1→12→0，两段收尾自发，两窗口 pose 哈希与 S6 完全一致（确定性重放），`source-drift=none` 全程。**姿态对照验收明示归 G5；`iState` 明示不作 G3-B 判据。**

### 三、残余与独立议题（不阻塞 G4，按需立项）

1. `currentDefaultState` 引擎镜像机制的声明来源（project DB / character data 何处声明"哪台机的 currentStateId 镜像到哪个具名变量"）。
2. 候选侧 `iState` 恒 0：`BSiStateTaggingGenerator` 候选侧不触发的原因（装备上下文假设未证）；若 G5 姿态对照需要 iState 语义，需先解决。
3. 652/8223 合并矛盾（运行时表含静态文件之外的注入内容；若 G4 语境隔离涉及 OAR 阶段变量来源，此项可能自然收敛）。
4. 工具遗产：`build/hkx-audit/`（gitignored）内的 HKX2-Enhanced-Library 克隆、HkxDump 转储器与 0_Master 全量转储 `0_master_v2.txt` 可复用于后续结构问题；是否收编进 `tools/` 由用户决定。全局 NuGet.Config 的失效 vcpkg 源（`C:\src
cpkg`）建议用户自行清理。

### 四、下一阶段：G4（语境隔离）→ G5 → G6（排序不变）

- **G4**：确认 OAR 阶段变量来源，验证历史、回调和 sink 的隔离——即候选在面板显示链里被驱动时，OAR 与游戏本体动画状态互不污染。先读 [10-29 交接的 G4 条目](#handoff-20261029)、[scene-graph.md 的显示会话边界](scene-graph.md)与 v33/v34 显示窗口证据（G2 的显示四边界是 G4 的直接基础）。**2026-10-31 收尾补充：G4 预注册契约已立（[C-G4 契约](#c-g4-context-isolation)）——含显示链/OAR 交接面的代码级核实（显示激活期间产品查询不执行、投影硬编码、候选直调引擎入口）与三段实验设计（A 求值可见性哨兵、B 世界历史 A/B 对照、C 关闭清理），待实现与授权上机。**
- **G5**：完整姿态对照——提前固定位置/旋转/缩放阈值及依据，核对全部有效层与骨；G3-B 的 pose 哈希基线（装备段 `bc160a8c/63f638a6/c407cb4e`、卸下段 `8fb4cd33/4f2aebde/c407cb4e`）可作为其确定性锚点。
- **G6**：生命周期与资源恢复（源恢复、请求/任务失效、预览自有引用/回调/sink/历史、重复会话收敛）；装载 teardown 仍 NO-GO，全局登记计数无可测锚点。

### 五、反模式（本阶段新增三条，接 10-31 清单）

10. 转储器去重不能放两处——`WalkGenerator` 与 `WalkMachine` 对同一对象重复 `seen.Add` 会把整棵状态机树静默跳过（A1 首轮转储只有 8.5k 行的事故根因）；去重只留入口一处，"输出行数远小于已知结构量"就是遍历残缺的警报。
11. INI 换装必须逐键断言核对——F 部署时两处键值先错后纠（默认值残留 `CandidateInputVariable=IsEquipping`、自替换 no-op 未改 `CandidateUnequipProbe`）；"replace 成功"不等于"键值正确"，写完必须把全部相关键打印回读。
12. 交接文档里的能力断言要当场核实——"CommonLib 有完整类型"写进了晚间 handoff，实际 pinned v11.0.0 的 `hkbStateMachine` 只有 unk* 骨架、条件类头文件不存在；下一会话按错误断言设计会直接撞墙。

### 六、部署与备份

- 留装：v59 DLL `013e1bdc…`（1,448,960 字节，身份 `…-dcf05cab1e-…`）+ PDB `8b8fd16f…`；部署 INI dormant（2,013 字节 `65e57401…`，全键 off）。
- 备份链新增：`dist/deploy-backup-20261031-v58/`（v58 三件套）、`-v59/`（v59 对 + F 前 dormant INI `1d40a0eb…`）；F 运行 INI `6dfb54ec…`。
- 分支 `oar-idle3` 本地领先远端 10 个提交未推送（A1 审计 + S6 三笔 + F 两笔 + 本交接），推送由用户决定。
- 日志：[S6](diagnostics/CharacterPanel-g3bs6-unequip-event-retry-20261031-2123.log)、[F](diagnostics/CharacterPanel-g3bf-full-replay-20261031-2138.log)。


## 当前状态与后续规划（2026-10-31 交接，接替 10-29 交接）

> **新会话从这里开始。** 本节接替 [2026-10-29 交接](#handoff-20261029)：当日排序里的 G3-B 前置核实（源/候选映射、writer 角色、输入子集、事件时序）已由 v53–v58 六次上机全部收敛或判死，结论与本节为准；10-29 节的 G4–G6 排序与反模式清单仍然有效。测量以[验证总表](#verification-matrix)为准；本轮逐轮记录见 [S3](#c-g3b-s3-unequip-event)、[E1](#c-g3b-e1-event-capture)、[S4](#c-g3b-s4-unequip-variable)、[E2](#c-g3b-e2-write-capture)、[S5](#c-g3b-s5-two-write)、[E3](#c-g3b-e3-transient-watch)。

### 一、本日会话做了什么（v53–v58，7 次有效上机 + 1 次无效会话）

| 轮 | 内容 | 判定 |
| --- | --- | --- |
| S3（v53） | 候选 equipped idle 后单次分发 `1HM_Unequip`（id=15） | 判负：分发被接受但相位不动 |
| E1（v54） | 58377 只读 detour，F7 窗口盯真实装备/卸下 | 判负：两次转移全程零事件 |
| S4 r1–r3（v55） | settle 后分别单写 `bEquipUnequip`/`IsEquipping` 镜像 `IsUnequipping`/`PitchManualOverride` | 三轮全负：值持久（121 行恒 1）、相位恒 1 |
| E2（v56） | Bool/Int setter 只读 detour（双图在门） | 判负：两次转移全程零 setter 调用，**装备锚正控同空** |
| S5（v57） | 两写序列（`iRightHandEquipped=1` 补装备态 → `bEquipUnequip=1` 命令） | 判负：两写均持久、相位不动 |
| E3（v58） | SourceTimeline 瞬态模式（BOOL/INT settle-0） | 判负：卸下锚同帧恰四个已知变量，无任何瞬态伴随写 |

（无效会话：17:01 一次面板未开，零实验行，不构成判定。）

### 二、合并裁定（本阶段的最终结论）

**卸下转移（源侧 `currentDefaultState` 1→12）由图内部逻辑驱动，不由任何外部输入驱动。** 排除表（全部有实验证据）：

| 假设路线 | 证据 |
| --- | --- |
| 58377 事件分发 | E1/E3：转移当刻零事件（双图在门） |
| Bool/Int typed setter 调用 | E2：零调用（双图在门）；装备态写也不走此路径 |
| 三个卸下脉冲变量的持久写 | S4 三轮 + E3：写被接受且持久，状态机不消费 |
| 装备态 + 命令的合取写 | S5：同上 |
| 装备态 setter 作为触发 | v41 时序：发生在转移后 0.85 s |
| 任何持久/瞬态词写 | v41 全量 + E3 瞬态模式（单帧闪烁都能看见，触发点没有） |

同时成立的事实（下一阶段可直接引用）：候选与控制器源图共享 `hkbBehaviorGraphData`（652 名/型/初始值全同）；候选激活前无值集、按 initial 初始化；**装备段输入已解决**——`WeapEquip`（id=2）单次注入可把候选从未装备态推进 equipped idle（v52 起四次复现，settle 门 target-step=49/基线 nodes=111 六次逐点复现）；`PitchManualOverride`=BOOL(65)、`1HM_Unequip`=事件 id=15、`iLeftHandEquipped` 有单帧闪烁行为（9→0→9→0）。

### 三、G3-B 现状重估

**（2026-10-31 深夜终版：图内结构静态审计 [C-G3B-A1](#c-g3b-a1-structural-audit) → 事件名修正切片 [C-G3B-S6](#c-g3b-s6-unequip-event-retry) → 本体验收 [C-G3B-F](#c-g3b-f-full-replay) 当晚三连完成。S6 命中确证 1→12 由事件 `Unequip`(id=144) 驱动；F 运行以 {`WeapEquip`(2)，`Unequip`(144)} 两个外部输入在候选自身状态机上走完 0→10→1→12→0（step 0→49→注入→21），两处段内转移均为图内自发，G3-B PASSED（限定范围：相位序列回放，姿态验收归 G5）。10-31 白天六轮外部输入判死结论维持有效——它们判死的是"变量/58377 事件路线"。下一步：G4（语境隔离）。）**

- **装备段可回放**：`WeapEquip` 注入即可。
- **卸下段不可外部回放**：原 G3-B 草案"在已证明的外部输入子集下回放 Equip→Idle→Unequip"的卸下段前提不成立——外部输入子集为空集。
- **出路**（按预注册排序，下一阶段唯一工作包）：**图内结构静态审计**——解出 equipped idle（`default`=1）状态的出边转移表与条件绑定，回答"1→12 转移的真实条件是什么"。答案决定 G3-B 卸下段的形态：条件若为事件/变量，候选侧照做（尽管真实流程不走，注入仍可能有效——注意 `WeapEquip` 对候选有效恰是先例）；条件若依赖 clip 时间/内部嵌套逻辑，则卸下段回放需重定义范围（如仅装备段回放 + 卸下段作为独立议题）。
- 审计的已知结构锚点：状态机有 `[sm+0xB8]` 事件→转移映射（57195 查询，见审计五/续审计五）；`hkbStateMachine` 的转移 info 数组承载变量条件与通配入口（CommonLib 有完整类型）；源图 project 名可从 `graph->projectName` 读出，对应游戏 `meshes/actors/character` 下的 HKX——**离线 HKX 解析与在内存只读探针两条路线均可**，前者更安全可重复，后者沿用既有仪器风格（新契约须预注册只读范围/预算/判据）。

### 四、反模式（本阶段新增三条，接 10-29 清单）

7. settle-3 规则会滤掉 1–2 帧瞬态写——E3 之前所有"变量没动"的结论都带此盲区（E3 已补上，后续观测结论须注明用的哪种模式）。
8. "正控"要选**必然发生**的事——E2 的装备锚本意是正控，结果同空，反而成了最重要的发现（引擎写法绕过 setter）；正控落空时先怀疑仪器再怀疑世界，但两者都要记录。
9. 共享部署位/F7 的实验必须显式排序（E2 契约的"部署排序"条款避免了与 S4 阶梯互踩）。

### 五、部署与备份

- 留装：v58 DLL `75b13902…`（身份 `…-65c069e1ba-…`），部署 INI 全键 off（dormant，1,795 字节 `86f8bed1…`）。
- 备份链：`dist/deploy-backup-20261031-v57/` 至 `-v53/`（每日一份，含 INI）。
- 仓库样例 `config/CharacterPanel.ini` 全键 off；`PLUGIN_SOURCES` 32 单元；分支 `oar-idle3`，本地领先远端若干提交未推送。

<a id="handoff-20261029"></a>
## 当前状态与后续规划（2026-10-29 交接）

> 新会话从这里开始：先读本节与[验证总表](#verification-matrix)，再读 [G3-A 契约](#g3-a-contract)（含 v41 运行记录）。历史逐轮记录保留在下方，不改写。

### 一、已完成与可复用的结论

| 项 | 状态 | 关键证据 |
| --- | --- | --- |
| G1（构造/链接/所有权） | **PASSED**，仅方案 A 修订判据 | 空壳配对销毁、两次装载、装载后源基线全等、保留无崩溃 |
| G1 遗留·装载图销毁 | **NO-GO**（2026-10-28 审计） | `Deactivate` 对 `activeNodes` 无空检查；候选与源共享 `rootGenerator`，污染源图会跟着存档 |
| G1 遗留·全局登记计数 | **证伪、不立项** | v35 读数 `300474432->300474432`；`inc` 未写回，`63529` 是无锁追加 |
| G2（暂停期单边推进 + 显示） | **PASSED**，限定范围 | 语义 120/120、实际写骨 109/116、源 `drift=none`、画面连续；四条边界（reset/F7/重开/读档）全成立 |
| G3-A（源图 settled 相位轨迹） | **成立**（2026-10-29 00:26） | 同一把单手武器的 Equip→Idle→Unequip 两段 settled 序列，按名字可取 |
| G3-B（候选自行走完同一顺序） | 未开始 | 本节第三节即其契约草案 |
| G4–G6 | 未执行 | — |

### 二、当前部署与开关

- 最新记录安装 DLL `3b8f8efe…`（v41，身份 `…v41-timeline-panel-gate-20261029-9bc1a4b03a…`），MO2 实例 `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\`。
- 部署 INI：`SourceTimeline=true`，`IndependentGraph=off`，`DisplayPose=false`，`DisplayStep=false`。产品动画路径为纯 OAR。
- 备份链：`dist/deploy-backup-20261029-v40/`、`-v39/`、`-v38/`、`-v37/`、`-v36/`、`-v35/`、`-v33/`、`-v30/`。
- 热键：**F7** 在面板外开 20 秒时间线窗口，面板内仍是重捕获；F8 切显示。F9 是游戏快速读档，不可占用。

本地 [v43](#v43-source-sessions)修正源记录窗口，[v44](#v44-display-clock)按 elapsed 时间调度显示，[v45](build.md#v45-source-list-2026-10-29)显式列编译单元，[v46](scene-graph.md#retired-copy-accounting-v46)保留完整退休副本台账。最新本地 DLL/PDB 仍为 [v46](build.md#v46-retained-copy-ledger-2026-10-29)，安装仍是上述 v41；本次 audit27 仅文档，不重编译或部署。不能把新代码/静态证据追认到旧运行。

### 三、G3-B 契约草案（未实现）

**目标**：在已证明的外部输入子集下，让候选自行完成 Equip→Idle→Unequip。v41 是成功的 settled 相位轨迹，不能把其每一项变化直接当成 replay 输入。本节仅草案，未授予 setter/event 写入权限。

| 项 | 当前角色 / 类型 | 证据与前置核实 |
| --- | --- | --- |
| `iState`、`currentDefaultState` | 本草案只作输出/相位观察，禁止回放写入；运行时类型待核对 | v41 分别观测 0→9→0、0→10→1→12→0；不据此推定类型或外部因果角色 |
| `iRightHandEquipped`、`iLeftHandEquipped`、`iMagicEquipped` | 各有至少一个静态外部整数 setter 调用点；仍是待准入的输入候选，实际类型/映射待验 | [audit27](behavior-graph-construction-audit.md#audit27)：AE38821 中 `0x6c70a9` / `0x6c7075` / `0x6c703d` 调 AE32886。不证明独占 writer、v41 实际执行、源/候选 variableInfos 类型/映射、最小充分输入集或事件时序 |
| `IsEquipping`、`IsUnequipping`、`bEquipUnequip` 及其它观测 flag | 拟议/不确定项，不是已授权写表 | 名字/0与1/相位相关性不证明 Bool 或外部输入；也可能是图内输出 |
| `WeapEquip` | 可用的候选单次注入路径；源事件时序未知 | 早期注入证明确有路径；G3-A 没有拦截源事件，不能宣称捕获了事件发生时刻 |
| 节点计数、clip 栈 | 观察输出 | 与相位对应；不作为强制成功的写入目标 |

**写入契约前置**：按变量名核对源与候选的存在性和 `variableInfos`/`m_type`，建立经验证的 Bool/Int/Float 映射。**C-VC1（2026-10-30）已把这一项收敛为“不需要独立映射”**：候选与控制器源图共享同一个 `hkbBehaviorGraphData`（`data`/`string-data`/`infos`/`initial` 四项判定全为 `same`，652 项名字与 `m_type` 逐一对齐，`matched=652 source-only=0 candidate-only=0`）。旧表述“463/652 的槽索引不可照搬”混淆了两张不同的图：**463 是玩家自有图**（SourceTimeline 的 `player_graph()` 取 manager 里最后一个非第一人称图），**652 是控制器 `source_graph()` 选中、并传给候选的那张**；两者变量表不同，不能互相替代。仍需核实外部写入者/输入输出角色及必要事件，论证最小外部输入子集。类型元数据只证明表示类型，不能证明 writer 或因果角色；八个目标变量在该表里的 `role`/`flags` 全为 0，连 output 位都没有，进一步说明元数据不可作为输入/输出判据。瞬态和事件时序若是决策必要条件，须先取得相应证据；不能把 settled 日志时间机械换成原 setter 步号。

**后续机制候选**：只对经验证的外部输入按名字选择既有 typed setter；不直接复制 word 槽、不新增 ABI/REL 入口。驱动仍以已有 `drive_candidate_step` 的 1/60 步为基线。事件/变量顺序不能由同 timestamp 的索引顺序反推；实验的时序容差须在写入契约里预注册。

**观察与判据草案**：候选自己的 `iState`/`currentDefaultState` 相位、Equip→Idle→Unequip 的 clip 栈和节点变化，应由状态机在外部输入下自行产生，并保持源 pose/变量对照。禁止写这两个相位值来满足判据。输入不足或角色/类型/时序未定时，回到证据核实，不放宽为成功；崩溃/非有限值/源漂移立即停止。

**视觉与回收边界**：[audit23](behavior-graph-construction-audit.md#audit23)已证 63856 不读输入 scale 向量、不写目标 scale，而当前转换会取 raw scale X 并写 local.scale；这是写入覆盖差异，不是相位对齐后的视觉根因。r4 的有限相机/混合/缩放样本缺少同步视频时刻与同相位 world/panel 对照，不能唯一定位闪近原因。普通 native 副本回收仍未证；v46 只修复完整台账/进程总数，未回收原生内存，G6 不因此通过。

### 四、之后的排序

下一次实际上机先按 [v46 首轮正常路径步骤](scene-graph.md#v46-manual-regression)核对源窗口/读档取消和普通 OAR 预览；两类结果均 **PENDING**，native candidate display 关闭。这不替代下面另场实验的 G3-B→G4→G5→G6 研究顺序。

1. **G3-B 前置核实 → 新写入契约 → 实现/构建/获授权上机**；当前草案不能直接执行回放。
2. **G4 语境隔离**：确认 OAR 阶段变量来源，验证历史、回调和 sink 的隔离。
3. **G5 完整姿态对照**：提前固定位置/旋转/缩放阈值及依据，核对全部有效层与骨。
4. **G6 生命周期与资源恢复**：源恢复、请求/任务失效、预览自有引用/回调/sink/历史和重复会话收敛；一张进程级保留候选是明确例外，装载 teardown 仍 NO-GO，全局引擎登记计数无可测量锚点。`DAC restore matched` 只是部分证据。

### 五、反模式清单（这几轮实际踩过的）

1. 看到 `inc` 就当计数器——必须确认是否写回内存（v35 得出 3 亿的荒谬值）。
2. 拿候选的计数当源的预期（v37 的 652 vs 463）。
3. 常开式预算记录——会被源图自带的 ~1.2 s 周期磨光（v38）。
4. 用"场景图指针是否存在"判断面板开没开——面板外同样存在（v40）。
5. 单点读门值就下结论（v30 的 `children=0`）。
6. 把周期量或相位相关 flag 直接当外部输入；`BlendJump`/`bVoiceReady`/`bIsInMT` 等循环必须先辨明角色。

### 六、下一次会话的最小任务清单（2026-10-29，v46 首轮待上机）

2026-10-30 补充：[DCMF 源码分析](community-reference-supplement.md#dcmf-reference)只作为后续参考。先完成下列现有 v46 源窗口/读档取消与正常 OAR 预览两类工作流并审阅实际日志/画面证据，再安排 DCMF 对照；两类仍为 **PENDING**，当前步骤、设置和判据不变。

1. 读本节、[验证总表](#verification-matrix)和 [v46 首轮操作步骤](scene-graph.md#v46-manual-regression)。历史 G2/G3-A 结论与本轮待验分开。
2. 用 [v46 归档身份/哈希](build.md#v46-retained-copy-ledger-2026-10-29)核对将测试的 DLL/PDB，先备份已装 pair/INI，只改清单中的既有键并完整重启。最新记录安装仍是 v41；这份文档没有部署文件。
3. 执行源窗口/读档取消，再执行正常 OAR 斧头、火把、三次已接受 F7 重捕获、F8 和关包重开；两类运行结果均 **PENDING**。保持 IndependentGraph=off、DisplayPose/DisplayStep=false，不据此验收 native pacing 或 G3-B。
4. 回传含 build 行的完整 CharacterPanel.log、设置、动作/异常时间、可用视频时间戳，以及接受捕获与 process retained-total 对照。按清单恢复原 INI 并重启；不把台账增长当回收成功。
5. 后续 G3-B 仍须第三节的源/候选名字与实际类型映射、剩余 writer/角色、明确输入子集和预注册时序，再另立写入契约。本次文档整理不重跑构建/测试，也不执行部署或游戏。

<a id="v42-residual-corrections"></a>
## v42：遗留证据校正与有界诊断（2026-10-29，本地）

`DisplayApplicationCoverage::missing_names` 从旧 cap4 提至 cap8。未来七缺失的 application 行可列全七名，超过八时仍以 `missing` 总数为准；requested/applied/missing/skipped-root/duplicates 算式和名称映射未改。v33 历史缺失总数仍七、已列四、未知三；不追加历史名字，不把 Weapon/Shield 控制/挂载语义当作无关。

冻结和逐步入口仍共用 `DisplaySession::State::accepts_source`；`DisplayWindow::rejection` 以原有效性真值分类，失败时 `reject` 消费既有 18 行预算并终止窗口。新行 `ANIMATION DISPLAY window=rejected ... steps=... reason=... source-comparison=discarded` 最多一条，不因 step 后 frozen 回退或后续帧重复输出。原因依次区分 `unbound-retained`、`actor-mismatch`、`source-generation-mismatch`、`source-graph-mismatch`、`controller-owner-mismatch`，并区分调用图与 token 不同的 `call-graph-mismatch`、候选缺失的 `candidate-unavailable`；inactive 窗口不再出行。多重不匹配只报告先遇到的一项。清空 retained token 只能证明 unbound，不能谎报已比较 owner 不同。预算已用完则无新行；有效来源的 frozen 回退与 exact1170/inject/private-storage native 守卫不变。

代码导航：[display_state_policy.h](../src/animation/behavior_graph/display_state_policy.h) 的 `DisplaySourceRejection`、`display_source_rejection`、`DisplayWindow::rejection/reject` 与 `apply_named_display_pose`；[independent_graph.cpp](../src/animation/behavior_graph/independent_graph.cpp) 的 `State::accepts_source`。只扩展 [原两个 session 场景](../tests/behavior_graph_display_session_test.cpp)：生命周期/身份/窗口拒绝，以及运动/七缺失覆盖。实际命令、结果、身份和 DLL/PDB 哈希见 [v42 build](build.md#v42-residual-corrections-2026-10-29)。无新依赖/目标/配置，无 timeline/input/native/运动算法修改。

**待授权游戏复现**：用新 DLL/PDB 核对身份，在有效显示窗口采 application 的全部真实缺失名并解释应用语义；再在同 controller 下实际改变来源图/generation，分别采 stepped 与 frozen-only 路径的 `window=rejected reason=...`/OAR 回落。核对只出一条、终止后无 native-step；若预算耗尽或原因不明保留未验证。v34 的 reset/重开/F7/读档回落四项结果保留；确切旧原因不能由新仪器倒推。**v42 尚无游戏数据**，仍未证明真实七名、同 owner 来源替换或 frozen-only 拒绝。

## 维护约束

此文拥有实验程序和状态；修改时同步[动画](animation.md)、[OAR](oar-hooks.md)、[构建](build.md)、[维护](code-maintenance.md)、README、相关配置和Changelog。区别两条边界：`BehaviorGraphProbe`最多16条源分组、值语义、只读；`IndependentGraph`默认off、每进程一次，当前 inject（v17 配方）为40条自身前缀预算，另有复用源快照，允许的候选原生调用/字段写入仅由对应实验契约授权。两者都不写源图，不能把源探针只读描述套在整个实验上。v18 在旧40条与源快照之外另有≤31条 `GRAPH FRAME`，其运行结果见 [v18 记录](#g2-v18-run)；v19 已实现、构建、部署并运行一次（分支 6，见[契约与实现](#g2-v19-contract)与[v19 记录](#g2-v19-run)），除获批的 eye 指针外不得据此增加调用或写入。v20 只改采样入口（63589），其首轮在入口前崩溃，v20a 仅修正该越界诊断并已部署待测；两者的原生调用许可同一，见[v20 记录](#g2-v20-run)。文档核查不替代引擎所有权、姿态或生命周期证据；构建/离线结果见 [Build](build.md#v19-eye-arm-build) 与 [v20a 构建](build.md#v20a-chain-fix-2026-10-27)。

<a id="v43-source-sessions"></a>
## v43：独立源记录窗口（2026-10-29，本地）

本轮只修正只读记录器的会话边界，保留 v41 的 G3-A 成功观测、v33/v34 的 G2 结论与上面的 v42 诊断改动。**本地构建与两个离线场景通过；未部署、未启动游戏、无新游戏证据。** 最新记录安装仍为 v41，SourceTimeline=true、其余三键 off；仓库四键默认均 off。

当前采集规则：面板外 F7 发布 arm 意图；post-UI sampler 消费后开始固定 20 秒截止时间，并清空变量 logged/candidate/settled/named 数组、节点基线/settle 计数、800 行预算与 exhaustion。重按 F7 同样重新开始，即使上窗已耗尽；原连续三个稳定采样规则及原始 int/float 四字节输出不变。首次有效来源只采新基线，不把上一窗的候选值或 settle 进度带进来。截止时间在任何可选图/数据/存储读取之前检查；等待首个有效来源不延长窗口。

窗口只接受同一 actor、外层 BShkbAnimationGraph、behavior、data、variable-set、word-storage 地址值和 word 数量。任一身份/布局变化（包括同数量替换）或来源不可用，输出 `window-aborted reason=source-lost-or-replaced` 并终止；不会 rebase 或续期，必须显式再按 F7。身份仅为数值快照，不保留原生引用；无法识别未观察到的同址生命周期 ABA，也不使原生数组读取成为原子操作。

F7 输入和 `main.cpp` 的 PreLoadGame/NewGame 回调只发布原子意图，不修改 Recorder 向量。世界失效优先于尚未消费的 arm，sampler 在任何源读取前消费失效、清空 pending/active 记录并返回；随后需新的显式 F7。既有 DAC reset 和 preview clear 不变。渲染先采样、后 manager 更新的顺序不能单靠延迟清理保护，因此取消直接从世界消息发布。此排队边界不承诺与正在进行的引擎数组修改同步。

代码入口：[source_timeline.cpp](../src/animation/behavior_graph/source_timeline.cpp) 的 `sample_source_timeline`、`baseline`、`abort_source`；[source_timeline.h](../src/animation/behavior_graph/source_timeline.h) 的 F7/取消接口；[source_timeline_policy.h](../src/animation/behavior_graph/source_timeline_policy.h) 的 `SourceTimelineWindow::arm/expire/observe_source/take_line`、`SourceTimelineRequests`；[main.cpp](../src/main.cpp) 的 `message_handler`。实际数组只由 sampler 持有，回调只接触原子请求。

[两个 engine-free 回归场景](../tests/source_timeline_session_test.cpp)：一窗耗完 800 行后连续两次重开，各自取得全额预算并要求新基线；另一个连续生命周期场景覆盖同数量图替换、存储迁移、源丢失、未绑定窗口按期结束，以及 pending/active 世界失效优先和后续显式重开。生产直接使用被测 policy；数组和节点 settle 重置由 sampler 的 `Recorder{}` 与 `baseline` 路径保证。MSVC 命令、退出结果、完整构建身份、DLL/PDB 大小及 SHA-256 见 [v43 build](build.md#v43-source-sessions-2026-10-29)。未重跑已保留的 v42 两例或全套。

安全修改：保持请求发布和数组所有权分离；增加来源字段时同步 identity 比较、上述两个场景和本节。此修正不证明变量的外部 writer 角色、运行时类型、确切 setter/event 时刻或原生数组原子性；G3-B 仍须原有前置核实。没有新 native hook/调用/写入、依赖、配置、兼容性范围或装载销毁权限。

<a id="v44-display-clock"></a>
## v44：按 elapsed 时间调度候选显示（2026-10-29，本地）

修正 opt-in `DisplayStep` 原先每次 draw 强制推进一个 1/60 步导致的速率随渲染调用频率变化。当前 [DisplayStepClock](../src/animation/behavior_graph/display_step_clock.h) 只在旧来源/运行时/inject/private-generation 门通过后消费 steady_clock 时间。首个 eligible 调用仅建立 origin、零强制步；后续累计每秒 60 个固定 native 步。一个观测 delta 最多计 100 ms、单次 draw 最多六步；超长停顿的多余时间丢弃，余下不足一步的精度仍保留，不形成无界 backlog。不改变游戏或用户 timescale。

时钟属于 `DisplaySession::State`。request、retarget 和来源拒绝经 `clear_statistics` 清空 origin/余量；不可推进或 public frozen fallback 同样 reset，这些明确 reset 边界后的首次 eligible 调用不追赶此前时间。若只是 F8 隐藏等绘制中断，且没有 request/retarget/reset，时钟不会因此完整暂停；恢复时按原停顿规则最多计 100 ms/六步，不能概括为扣除所有不可见时间。零 due 时通过 private held reader 返回当前有效候选姿态，成功仍是 stepped mode，不递增 native/motion 计数，也不因等待而反复输出 application mode。真正的来源不匹配仍拒绝候选并回 OAR。

每个 actual native 步都单独检查 generation 存储门、运行原共用配方、递增 `DisplayWindow::steps`、做原始/语义运动比较，并分别处理 1/60/120 checkpoint 与 source-control。多个 due 步跨过 60 或 120 时不会漏点。checkpoint 保留原 `output-records` 读数；循环结束后读取/转换最终候选姿态供本次 draw。`native-step` 与 gates `call` 都是实际步序号，pose-summary 的 120 是 native steps，不代表 120 个渲染调用或游戏实时速率已校准。batch 的 120 个固定步和 native IDs/配方均未改变。

每个 view 最多一条 `ANIMATION DISPLAY pacing request=... view=... hz=60 max-delta-ms=100 max-steps=6 first-call=origin`，占原有 18 行预算。保留 v42 的缺失 cap8/一次拒绝诊断，以及 v43 源记录器；没有新配置、依赖、native hook/ABI/写入、候选销毁权限。

代码导航：[independent_graph.cpp](../src/animation/behavior_graph/independent_graph.cpp) 的 `DisplaySession::step_display_pose`、`read_held_display_pose`、`held_display_pose`、`State::clear_statistics`；[接口](../src/animation/behavior_graph/independent_graph.h)；[settings.h](../src/animation/settings.h) 的 `display_step` 注释。安全修改时保持 clock reset 与 request/view/fallback 边界一致，保留逐 actual-step 的门、比较和 checkpoint；零 due 不能被解释为 frozen mode。

[两个 clock 回归](../tests/behavior_graph_display_clock_test.cpp)使用人工时间，不运行引擎：30 FPS 两秒产生 120 步且首调用为零；120 FPS 两秒同为 120 步且包含零 due 调用，随后 reset/不可推进间隔、长停顿六步截断、保留小数步且无残留 backlog。MSVC14.44 assertions、Release 构建与保护哈希/导航检查见 [v44 build](build.md#v44-display-clock-2026-10-29)。未重跑旧套件。

**本地代码/离线验证通过，未部署或启动游戏。** 安装仍是 v41、SourceTimeline=true 其余三键 off；仓库四键默认 off。v33/v34 历史调用频率/120 步和 G2 结论、v41 G3-A settled 轨迹保持原样；本轮不把历史调用耗时追认为 60 Hz 速率证据。游戏时的速率、停顿和画面仍待新的授权观察。

<a id="c-vc1-candidate-census"></a>
## C-VC1：候选变量清单只读探针（2026-10-30，本地）

**目的只有一个**：补齐 G3-B 前置核实的第 1 项——候选侧变量按名字的存在性与 `m_type` 映射。此前只有源侧证据（v41 的 463 字轨迹与此后的按名变化序列），候选侧 652 字的槽索引不能照搬。

**读集**（全部为普通字段读，无原生调用、无写入、无新 hook/REL 入口）：两侧各读 `behavior`/`data`/`stringData`/`variableInfos` 指针用于共享判定；`variableInfos.size()`、`variableNames.size()`、词数组 count（`variableValueSet` 成员私有，仍走 +0x10/+0x18）、`variableBounds.size()`、`variableInitialValues` 的 count；逐项 `m_type`/`m_role`/`m_flags`/名字/当前字/初始字/上下界；`eventInfos` + `eventNames` 的名字与 flags。`m_flags` 的 output 位只作提示记录，不作为输入/输出判据。

**门与触发**：新键 `[Experiment] CandidateVariableCensus=false`；仅在 `IndependentGraph != off` 且候选 `loaded==true` 后每进程一次，位于候选保留之后、任何 activate/drive/inject 之前。

**上界**：条目表以类型表为准；词数组存在时取 `min(infos, words)`，词数组缺失时退回 `min(infos, 8192)` 并把这些行标成 `word=none`。名字表缺失或更短只让对应行 unname，不许吞掉类型/名字证据——名字表是证据，不是长度权威。

**输出**：`phase=compare`（data/string-data/infos/values/bounds/initial 六项 `same|different|unavailable`）、两侧 `phase=table`/`phase=types`/`phase=events`、逐项 `item`、`event`，以及按名字的 `align`/`align-missing` 与 `phase=summary`。对齐只按名字；槽位相同但名字不同不算匹配。

**判据**：能从候选侧读到 `iState`、`currentDefaultState`、`iRightHandEquipped`、`iLeftHandEquipped`、`iMagicEquipped`、`IsEquipping`、`IsUnequipping`、`bEquipUnequip` 的 index 与 `m_type`；`data` 若为 `same`，结论固定为“候选复用源类型表，G3-B 不需要独立映射”并停止推断；三处计数不一致时如实记录，不择一采信。

**首次运行（2026-10-30 23:53，v47，`IndependentGraph=load`）**：`phase=compare` 四项为 `data=same string-data=same infos=same initial=same`（同一 `0x1eeb4d37510`）；两侧 `infos=652 named=652`，`align matched=652 source-only=0 candidate-only=0`。因此**候选与控制器源图共享整张变量表，按名字映射这一步对候选是零工作**。八个目标变量：`iState`(94,INT32)、`IsEquipping`(110,BOOL)、`IsUnequipping`(111,BOOL)、`currentDefaultState`(113,INT32)、`iMagicEquipped`(117,INT32)、`bEquipUnequip`(125,BOOL)、`iRightHandEquipped`(179,INT32)、`iLeftHandEquipped`(180,INT32)，`role`/`flags` 全 0。两条负结果：`bounds` 数组为空（`0x0/0`，项目无 authored 上下界），`eventInfos`/`eventNames` 双 0（该路径拿不到事件名与时序）。关键状态差：源侧 `values` 有效、652 字中 217 个非零；**候选 `values=0x0 words=0`，即激活前不存在值集**，652 行全部 `word=none`。

由此得到 G3-B 的新顺序约束：**变量写入必须发生在 Activate 之后**。为了让这条从推断变成证据，v47 之后追加一次只读复采（见下），其余判定与读集不变。

**激活后复采（v48，待上机）**：`IndependentGraph` 为 `drive`/`inject` 且激活成功后，立即对候选再采一次整表，标记 `stage=post-activate`（`phase=post-activate` 先给指针与词数，随后 `phase=table`/`phase=types`/`item` 复用同一读集）。位置在 `log_drive_state("activated")` 之后、任何 1/60 驱动步之前，因此反映的是 step 0 状态；不新增原生调用、不写变量、不改后续步序。所有普查行新增 `stage=` 字段（`load` / `post-activate`）以区分两次采样。

离线两例（[behavior_graph_candidate_census_test.cpp](../tests/behavior_graph_candidate_census_test.cpp)）：类型分组/名字计数/上界，以及按名字对齐的双向缺失与同名异型。策略头 [candidate_variable_census.h](../src/animation/behavior_graph/candidate_variable_census.h) 不依赖 RE 类型；调用点在 [independent_graph.cpp](../src/animation/behavior_graph/independent_graph.cpp) 的 load 成功分支与激活成功之后；未新增 `.cpp`，`PLUGIN_SOURCES` 仍为 30 项。

**状态**：v47（load 阶段普查）已上机，结论如上；v48（激活后复采）本地构建完成、待上机。本探针不建立外部 writer 角色、不证明 `m_type` 等于实际写入口径、不采事件发生时刻，也不改动源图变量、候选变量、activeNodes 或显示/捕获路径。候选与源共享 `data` 却报出不同词数，属必须保留的矛盾，不得择一采信。

<a id="c-g3b-s1-single-input"></a>
## C-G3B-S1：单整数写入切片（2026-10-31，本地）

**目标**：只证一件事——候选在激活后接受**一个**外部整数输入，能否自行进入装备相位。这是 G3-B 回放契约的最小可判切片，不是 G3-B 本体。

**写集**：仅 `iRightHandEquipped`（C-VC1 实测 index 179、`m_type=INT32`），值 `1`，每进程一次。写前按名字在候选自己的 `stringData->variableNames` 里解析 index，名字缺失即 `outcome=skip reason=name-missing`。

**机制**：`BShkbAnimationGraph::SetGraphVariableInt(BSFixedString, int)`（pinned CommonLib 成员，AE 63609，`this` 即候选图）。不新增 REL 入口、不手改 word 槽、不复制槽位。返回值必须记录；返回 false 不重试、不降级为直接写槽。被否掉的路径：`IAnimationGraphManagerHolder::SetGraphVariableInt` 指向 holder 注册的 manager，写不到未注册的候选。

**时机**：`char_activate` 成功之后、第一个 1/60 步之前——v48 已证激活后 `values` 非空且 `words=652`。

**观测**（只读）：step ∈ {0,1,2,6,60,120} 各一行 `CANDIDATE INPUT … rh=… istate=… default=… nodes=…`；节点数基线 97（v48 记录）。

**判据**：写入返回 true 且 step 0 读回 1；120 步内 `iState` 出现 9 或 `currentDefaultState` 出现 10/1、节点数偏离 97 → 单整数足够，下一片做 Unequip。读回 1 但相位不动 → 单整数不足，下一片加 `bEquipUnequip`。返回 false 或读回 0 → 停在诊断。崩溃、非有限值或 `source-drift` 非 none 立即停止并保留日志。

**非目标**：不写其它变量、不做 Unequip、不做姿态对照、不改显示/捕获/台账，也不宣称 G3-B 通过。

门：`[Experiment] CandidateInputProbe=false`，需 `IndependentGraph=drive|inject`。无离线用例——本轮没有新增纯逻辑，判据只能运行时取；为凑用例写恒真断言没有意义。

**状态：本地 Release 构建通过（`/W4 /WX`），已部署待上机。** 该调用前没有任何本项目的变量写入先例；崩溃即视为该 setter 不适用于未注册的候选图，不得改为直接写槽绕行。

**首轮运行（v49，2026-10-31 00:14，`IndependentGraph=drive`）判为无效，不构成对输入的判定**：写入本身成功（`outcome=write result=true readback=1`），但六个采样点全部 `rh=1 istate=0 default=0 nodes=97`，`phase=pose-motion comparisons=120 changed=0 max-delta=0.000000`，`candidate-pose-0/60/120` 三点同哈希 `bc160a8c2b3a5a58`，`phase=activated … children-flag=0`。原因在驱动路径而非输入：`drive` 的批量驱动未跑引擎生成入口（`generation_profile` 原只在 `inject` 置真），候选图因此从未生成，静态图无法判定任何输入。

**v50 修正**：新增 `[Experiment] CandidateGenerationEntry`（默认 off），仅在 `drive` 下把生成入口打开；`inject` 语义不变。打开的正是 [显示路径](../src/animation/behavior_graph/independent_graph.cpp) 每次 draw 已在调用的同一个入口（v33/v34 已跑过数千次），不是新原语。v49 的写入结果只保留为"setter 对未注册候选图返回 true 且读回一致"这一条局部事实，不再解释为相位证据。

**实施结果（2026-10-31 00:22–00:45，v50–v52 与一次仅改 INI 的组合轮）**

| 轮 | 输入 | 运动 | 相位（121 步采样） |
| --- | --- | --- | --- |
| v50 (drive+生成入口) | `iRightHandEquipped=1` (INT32) | 80/120 | 全 `istate=0 default=0 nodes=97` |
| v51 (drive+生成入口) | `IsEquipping=1` (BOOL) | 80/120 | 全 `istate=0 default=0 nodes=97` |
| v52 (inject，空输入) | `WeapEquip` 事件 | 119/120 | `default` 10→1（step 49），`nodes` 113→96→94→93 → 128→126→125→111 |
| 组合轮 (inject+变量) | `WeapEquip` + `iRightHandEquipped=1` | 119/120 | 与 v52 完全相同，`istate` 仍 0 |

三条结论：**相位由事件推进，不由这两个变量推进**；`SetGraphVariableInt/Bool` 对未注册候选图可用（写入成功、读回一致），但只是把值放进槽里，状态机不因此起步；`iState=9` 在源侧与 `currentDefaultState 10→1`、`IsAttackReady`/`bMRh_Ready`/`bMLh_Ready` 同刻发生，候选侧稳定为 0 —— 最可能是需要角色身上真的有装备（候选没有物品），而不是缺某个变量。

未决：Unequip 段未被驱动；`iState` 是否应作为 G3-B 的必需判据尚未定；v49 的 `children-flag=0` 已被证伪为"运动门"（v50 该值仍 0 却有 80/120 变化）。生成入口本身才是运动门。

<a id="session-20261030"></a>
## 2026-10-30/31 会话总结：从 C-VC1 到事件路线

本会话完成了三件事：v46 台账版本首次上机（[步骤与结果](scene-graph.md#v46-manual-regression)）、C-VC1 候选变量普查、以及 C-G3B-S1/S2 输入切片。全部为 AE 1.6.1170 + MO2 实机，逐轮身份与哈希见 [build](build.md#session-20261030-builds)。

| 轮 | 目的 | 结论 |
| --- | --- | --- |
| v46-20261030 | 台账与正常路径上机 | 源窗口/读档取消、15 捕获/14 PARK、阈值唯一告警、F8/重开全部正常；干净窗口 20.03 s 关闭。详见 [scene-graph](scene-graph.md#v46-manual-regression) |
| v47 (C-VC1, load) | 候选变量表 | `data`/`string-data`/`infos`/`initial` 四项全 `same`（同一对象），652 项名字与类型对齐 `652/0/0`；`bounds` 与事件名表为空；**激活前候选无值集** |
| v48 (C-VC1, drive) | 激活后复采 | `values` 非空、`words=652`；候选按 **initial** 初始化，不继承源活动状态（`Pitch` 源 -26.79 → 候选 0）；八个目标变量 step 0 全 0 |
| v49 (S1, drive) | 写 `iRightHandEquipped=1` | 写入成功读回 1，但候选静态（`changed=0`、三点同哈希）→ **该轮无效**，根因是 drive 未跑生成入口 |
| v50 (S1, drive+入口) | 同上 | 运动 80/120，相位 121 点全 `0/0/97` → 单整数不足 |
| v51 (S2, 逐步采样) | 写 `IsEquipping=1` | 运动 80/120，相位全 `0/0/97` → **变量路线判死** |
| v52 (inject, 空输入) | 只发 `WeapEquip` | 运动 119/120；`default` 10→1（step 49）、`nodes` 113→96→94→93 → 128→126→125→111 → **事件驱动相位** |
| 组合轮 (inject+变量) | 事件 + `iRightHandEquipped=1` | 与 v52 逐项相同，`istate` 仍 0 → 变量对出鞘位无贡献 |

**已确立的三条事实**

1. 候选与**控制器源图**共享同一个 `hkbBehaviorGraphData`，名字/类型/初始值完全一致，按名字映射不是工作项；旧表述"源 463 / 候选 652"实为两张不同的图（`player_graph()` 的 463 与 `source_graph()` 的 652）。
2. 候选值集在激活后才存在，且按 `initial` 初始化——共享 `data` 不等于共享状态，任何输入都必须显式给。
3. 相位由**事件**推进；`iRightHandEquipped`、`IsEquipping` 这类量在被写入后不改变状态机行为，读作图内输出。`iState=9` 与源侧 `IsAttackReady`/`bMRh_Ready`/`bMLh_Ready` 同刻发生，候选侧恒 0，最可能需要角色身上真有装备。

**两条方法论修正**

- `children-flag=0` 不是运动门：v50 该值仍 0 却有 80/120 变化；真正的门是引擎生成入口，`drive` 原本不跑它，v50 用 `CandidateGenerationEntry` 显式打开。
- 六点采样会漏瞬态相位：v51 起改为逐步（0–119 + 120）采样，121 行才足以判"没动"。

**未决**：Unequip 段未被驱动（源侧 `default` 1→12）；`iState` 是否应作为 G3-B 必需判据尚未界定；候选若确需装备上下文，那是独立议题。此前的 [G3-B 契约草案](#handoff-20261029) 中"按变量名建立映射"一项按上述第 1 条收敛为不需要，其余前置（写入者角色、最小输入集、事件时序）仍以本节证据为准。

<a id="c-g3b-s3-unequip-event"></a>
## C-G3B-S3：Unequip 事件切片（草案，2026-10-31，未实现）

**目标**：只证一件事——候选在装备相位完成后接受**一次**卸下事件注入，能否自行进入并走完卸下相位。这是把 v52 的事件路线从 Equip 段延伸到 Unequip 段，是 G3-B 事件输入子集的第二个切片，不是 G3-B 本体。

**输入（事件名预注册）**：`1HM_Unequip`。证据：v8d（2026-10-24）首次读出图事件名表 `count=8223`，前 16 名同时含装备链入口 `WeapEquip` 与 `1HM_Unequip`，这是工作区内唯一有出处的卸下事件名；`WeapUnequip` 在本工作区与 pinned CommonLib 中零出处，禁止臆测。**单一预注册名**：注入后若相位在预注册窗口内不动，按负结果记录并停止，同一轮不试第二个候选名（防 fishing）；第二个名字必须先回到事件名表证据再立项。

**机制**：复用 v52 已验证的同一路径——`lookup_event_id_in_graph` 在图自己的 `stringData->eventNames` 里按名解析 id（上限 `0x8000`、逐名精确比较），以 `InjectedEvent{id,0,nullptr,nullptr}` 走既有 `event_dispatch`（REL ID）分发。区别只在触发时机：这是**第二次**分发，由 settle 门触发，而非激活时的一次性分发；第二次分发本身是新调用，仅由本契约授权。不新增 REL 入口、不写任何变量、不改事件队列结构。

**时机（两段式，每进程一次完整序列）**：

1. 第一段与 v52 完全相同：`IndependentGraph=inject` 激活时单次 `WeapEquip`。
2. settle 门：候选 `currentDefaultState` 到达 1 后连续三次采样保持稳定（v38 settle 规则），同时记录此时的节点数为装备 idle 基线。等待上限预注册 120 步；超限输出 `outcome=no-settle` 并终止，不降级为定时注入。
3. 第二段：注入 `1HM_Unequip`，其后逐步采样 121 步（0–119 + 120），沿用 v51 的逐步采样修正——六点采样会漏瞬态相位。

**观测**（只读）：逐行 `CANDIDATE UNEQUIP step=… default=… istate=… uneq=… bequip=… pitch=… nodes=…`，对照 v41 源侧卸下段（53.652–54.933，约 1.3 s）的变量集 `IsUnequipping`/`bEquipUnequip`/`PitchManualOverride`；`phase=unequip-motion comparisons=…`（勘误：草案误写 121，且行名为 `unequip-motion`——实现为窗口内 120 次 pose 读样、首个作基线，即 119 次相邻比较；采样行 121 含窗外 step 120。首轮实测 119，与本口径一致）、三点 pose 哈希与 `source-drift` 照旧。注入尝试自带的 `phase=events count=` 行照常输出。

**判据**：主判据 `currentDefaultState` 1→12→0；辅判据节点数偏离 settle 门记录的装备 idle 基线并回落、`IsUnequipping`/`bEquipUnequip` 出现脉冲、运动持续。命中主判据 → 事件链足以驱动卸下段，G3-B 输入子集扩为 {equip 事件, unequip 事件}，下一片预注册两事件的时序容差并起草完整回放契约。未命中 → 负结果入档，回到事件名表证据。`iState` 在本切片**只记录不判定**：S1/S2 已证候选侧它可能需要真实装备上下文，是否作为 G3-B 必需判据留给 G3-B 本体契约决定。

**已知矛盾（记录在案，不阻塞本切片）**：C-VC1 v47 在 load 阶段报 `eventInfos`/`eventNames` 双 0，与 v8d 注入阶段读出的 8223 冲突。普查的 `array_size` 对超过 8192 上限的计数返回 0，"空"与"超上限"在其输出中不可区分；v8d 读数发生在注入（激活后）阶段而普查在 load 阶段，时机与上限两种解释并存。该普查结论降级为"未能测定"；S3 运行自带的 `phase=events count=` 行将顺带裁定实际计数。普查代码区分 0 与超上限的修正另立小项，不在 S3 范围内。

**非目标**：不写任何变量、不做姿态对照验收、不改显示/捕获/台账，也不宣称 G3-B 通过；不把源侧 1.3 s 的 `IsUnequipping` 脉冲时长机械换算成候选步数上限。

**停止条件**：崩溃、非有限值或 `source-drift` 非 none 立即停止并保留日志。

**门**：新键 `[Experiment] CandidateUnequipProbe`（默认 off），需 `IndependentGraph=inject`（inject 自带生成入口，无需 `CandidateGenerationEntry`）。settle 门若以 engine-free policy 实现，按仓库惯例须带离线用例：等待超限 `no-settle`、连续三稳定采样触发、中途扰动后重新计数。

**实现与部署（2026-10-31，v53）**：settle 门落在 engine-free 策略头 [unequip_settle_policy.h](../src/animation/behavior_graph/unequip_settle_policy.h)（`UnequipSettleGate`：目标值 1、三连续采样稳定、扰动重计、尾部离开目标即 `no-settle`），三个离线用例在 [behavior_graph_unequip_settle_test.cpp](../tests/behavior_graph_unequip_settle_test.cpp)（`cl /W4 /WX` exit 0）。调用点在 [independent_graph.cpp](../src/animation/behavior_graph/independent_graph.cpp)：门喂入在第一段窗口每步 `e_pre_sample`；判定与二次分发在第一段采样/发射之后、物理世界归还之前（复用同一 borrowed world、`hkbContext` 与生成入口）；`InjectedEvent` 上提为匿名命名空间单一定义，注入块仅改为引用它。观测行按判据所需命名解析：`currentDefaultState` 缺失即整片 skip（`name-missing`），`iState`/`IsUnequipping`/`bEquipUnequip`/`PitchManualOverride`/`iRightHandEquipped` 任一缺失只让该列显示 `none`，不中止切片。预算：`CANDIDATE UNEQUIP` 前缀 ≤ arm 1 行 + settle 1 行 + 121 行采样；第二段新增的 `INDEPENDENT GRAPH` 行 ≤ 4 条（event-lookup、unequip-inject 或 unequip-skip、unequip-motion、unequip-done）；第二段窗口不消耗 `GRAPH FRAME`/clip/slots 预算。构建身份、哈希与部署 INI 见 [v53 build](build.md#v53-unequip-event-2026-10-31)。

**状态：已实现、已构建（干净身份 `…-ecb5869efd-…`）、已部署；首轮已上机，判为负结果（见下）。** 部署 INI 运行后已回 dormant off（v53 DLL 留装），仓库样例保持全 off。运行判据与本节草案一致；结果回填在下方追加，不改写本节既有文字。

**首轮运行（2026-10-31 15:55，v53，AE 1.6.1170，actor 00000014 request=1；[日志](diagnostics/CharacterPanel-g3bs3-unequip-event-20261031-1555.log) 86,409 字节 SHA-256 `5cc72cec…`）：负结果。**

| 段 | 观测 |
| --- | --- |
| 装备段（第一窗口） | 与 v52 行为一致：`phase=settle outcome=settled target-step=49 settled-step=63 baseline-default=1 baseline-nodes=111`（target-step 与 v52 的 step 49、基线与 v52 终态 111 完全一致）；`pose-motion comparisons=120 changed=119`；`candidate-pose-0=bc160a8c… 60=63f638a6… 120=2b29331e…`；源 pose 前后同为 `ec429de0…` |
| 门与事件 | `outcome=arm default=113 istate=94 uneq=111 bequip=125 pitch=65 rh=179`——六名全部解析，`PitchManualOverride` 首证存在于候选 652 表（index 65）；`phase=events count=8223`，前 16 名与 v8d 转储逐项一致 → **普查"双 0"裁定为 8192 上限截断伪影**，普查代码区分 0 与超上限的修正项成立；`1HM_Unequip` 解析 **id=15**，单次分发成功 |
| 卸下段（第二窗口） | 121 行采样全为 `default=1 istate=0 uneq=0 bequip=0 pitch=0 rh=0 nodes=111`；`unequip-motion comparisons=119 changed=119 valid=true`（装备 idle 继续动画），`cand-pose-0=8fb4cd33… 60=7a044b34… 120=2b29331e…`（终点与第一窗口终点同哈希）；无 `[E]`、无崩溃，`source-drift=none` 全程，`phase=end retained=true` |

**裁定**：主判据（`default` 1→12→0）未命中；辅判据未命中（无节点偏离、无卸下脉冲）；`iState` 恒 0 按预注册仅记录。**`1HM_Unequip`（id=15）单独注入对未注册候选图不足以推进卸下相位**——分发被接受但状态机未离开 equipped idle；对照 v52 的 `WeapEquip`（id=2）能推进装备相位，事件路径本身有效，负结果限定于该事件与该时机（settled equipped idle）。按防 fishing 约束不试第二个候选名；"卸下转移需要哪个事件/是否需要伴随输入或装备上下文"须先回到事件名表与引擎证据再立项。本轮顺带产出两条确定证据：候选 652 表含 `PitchManualOverride`(65) 与 `1HM_Unequip`(id=15)；事件表在注入阶段为 8223 项。会话后部署 INI 已回 dormant off（1,585 字节 `1bcd95bc…`）。

<a id="c-g3b-e1-event-capture"></a>
## C-G3B-E1：源图事件流捕获（草案，2026-10-31，未实现）

**背景与目标**：S3 判负后剩下的未决是"真实卸下到底由什么驱动"。v41 只记录了变量的 settled 轨迹，从未拦截源图实际收到的事件。本探针只回答一件事：**真实装备/卸下期间，引擎向控制器源图的状态机派发了哪些事件（id→名字），时序与相位变化如何对应**。它是 G3-B 写入契约"事件时序"前置的证据来源，不触碰候选、不注入任何事件、不写任何变量。

**机制（本实验线首个 detour 类仪器，授权边界见下）**：在引擎事件派发原语 **58377**（`REL::ID(58377)`，hkbBehaviorGraph vtable 槽 6 落体，[审计五/续审计五](behavior-graph-construction-audit.md)）安装只读 trampoline detour。选它的理由：事件到状态机的两条原生路径——"入队 + Update 尾部桩（0xab1f30）逐个派发"与"内部直调 58377"——**最终都经过 58377**；纯轮询事件队列环形缓冲（`[q+0x00]` 数组、`[q+0x8]` 容量、`[q+0xC]` 读游标、`[q+0x14]` 计数、24 B 表项，[队列审计](behavior-graph-construction-audit.md)已闭合）看不到直调路径，存在结构盲区，故不采用。

**回调读集与纪律**（全部只读）：实参 `behavior`、`event->id`（+0x00 u32）、`event->payload` 槽判空（+0x08 指针，**不解引用**）、`ctx->behavior`（+8 判空）。behavior == 当前窗口源图指针时写入无锁 SPSC 环（生产者 = 游戏主线程动画更新，消费者 = 渲染钩子 drain；深度 1024，溢出只计数不阻塞），随后立即转发原函数。热路径 = 一次原子 load 比较，窗口关闭时 gate 为空直接放行。**回调内不解析字符串、不分配内存、不持锁、不调用引擎函数。**

**安全前置（实现时核对，写进构建记录）**：detour 头部 5 字节补丁要求 `0xab58a0` 起始至少 5 字节为完整指令边界——实现前先 dump 前 16 字节反汇编核对，不满足则本探针作罢，不换写法、不退回写队列或改 vtable。SKSE trampoline 常驻不卸载：窗口 gate 清零后残余成本为每帧一次分支；detour 生命周期 = 进程生命周期，安装一次。

**门与窗口**：新键 `[Experiment] SourceEventCapture=false`（默认 off）。要求 `IndependentGraph=off`（纯源观察，候选不存在、注入路径零干扰）。与 `SourceTimeline` 同开属非法配置：加载告警，且 **SourceTimeline 优先、E1 不装 hook**。窗口复用 SourceTimeline 骨架语义：面板外 F7 发布 arm 意图 → 20 秒截止 → 源丢失/替换/读档取消，取消后须显式再按 F7；一次只服务一个捕获器。

**观测**（同窗两路，一次 F7 同时产出）：

1. 事件行：渲染侧每帧 drain 环，逐条 `SOURCE EVENT actor=… request=… seq=… id=… name='…' payload=null|set`。名字在 drain 侧用图自身 `stringData->eventNames[id]`（`&~1` 掩码，与既有 lookup 同款读法）解析；id ≥ count 记 `name=oor`。
2. 相位行：沿用 v37 的按帧 diff 规则（仅变化时出），同窗采样 `currentDefaultState`/`iState`/`IsEquipping`/`IsUnequipping`/`bEquipUnequip`/节点数，供事件与相位对齐。

**预算**：事件行 ≤ 600 行/窗口、相位行 ≤ 200 行/窗口；任一耗尽即 `budget-exhausted` 终止（照 SourceTimeline 惯例），不滚动覆盖。

**判据（预注册）**：run sheet = F7 开窗 → 装备单手武器 → 等约 2 秒 → 卸下 → 等约 2 秒 → 窗口自动关。裁定以相位变化为锚：**装备锚** = `currentDefaultState` 首次离开 0 ±10 帧内的事件序列；**卸下锚** = `currentDefaultState` 首次离开 1 ±10 帧内的事件序列。锚窗口内的 (id, 名字) 集合即"真实装备/卸下的事件签名"，作为下一个注入切片（S5+）的唯一合法候选来源——仍按单名预注册规则逐轮验证。**负结果同样有效**：锚窗口内事件行为空、或只有 FootLeft/FootRight 类步频事件，说明卸下转移不经 58377 可见的事件驱动，届时转回"卸下侧变量从未被写入过"（`bEquipUnequip`/`PitchManualOverride`）这条未测线索，另行立契约。

**非目标**：不注入、不写变量、不触碰候选、不改 58377 行为语义（纯透传）、不做姿态对照、不宣称 G3-B 前置完成。

**停止条件**：崩溃立即停止——detour 崩溃即回退纯观察、INI 回 off 并归档 crash 报告；源丢失/替换/读档取消照 SourceTimeline 身份规则；behavior 指针同址复用（ABA）由"源替换即取消窗口"规则覆盖，残余风险如实入档。

**离线用例**：SPSC 环与窗口状态机进 engine-free 策略头——溢出计数、drain 保序、gate 关闭丢弃、预算耗尽、源取消优先于未消费 arm——`cl /W4 /WX` 全过才算实现完成。

**实现影响面**：新增 hook 载体 `.cpp`，`PLUGIN_SOURCES` 30→31；`SKSE::GetTrampoline()` 为工具链既有能力；按维护约束同步主文档、animation、OAR、build、maintenance、README、config 与 CHANGELOG。

**状态：已实现、已构建（干净身份 `…-995d32347d-…`，[v54 build](build.md#v54-event-capture-2026-10-31)）、已部署；首轮已上机，事件路线判负（见下）。** 安全前置已按本节条款完成并记录在构建节：58377 头部 7 字节 `48 8b c4 48 89 48 08` 从审计用的 Steamless 解包镜像（SHA-256 `843823c1…` 与台账一致）读出，两条指令均无 RIP 相对寻址、边界在 +7，5 字节 jmp + 2 填充可安全落位；桩头 `48 83 ec 48 48 83 7a 08 00` 与审计 `0xab1f30` 记录互证。实现要点：detour 头部字节在**运行时安装前再核对一次**（不匹配则记下实测 16 字节、不装 hook）；thunk 保存全部寄存器与标志、调用记录函数后跳转重定位头；环为"满时丢最新"语义（生产者不得写消费者尾索引，SPSC 单写者约束），逐帧 drain 下该路径实际不可达，dropped 计数是响亮失败信号。离线三例（环序/溢出/drain-discard、双预算独立关窗、cancel 与到期优先）`cl /W4 /WX` 通过。部署 INI 运行后已回 dormant off（v54 DLL 留装），仓库样例全 off。结果回填在下方追加，不改写本节既有文字。

**首轮运行（2026-10-31 16:31，v54，AE 1.6.1170，actor 00000014；[日志](diagnostics/CharacterPanel-g3be1-event-capture-20261031-1631.log) 6,937 字节 SHA-256 `d43ad1b1…`）：事件路线判负，负结果决定性。**

| 流 | 观测 |
| --- | --- |
| 安装 | `installed target=0x7ff7584458a0 stub=0x7ff6d91e0000 original=0x7ff6d91e0050 head=48 8b c4 48 89 48 08`——运行时头部核验通过，detour 在位；16:30:42 一次读档触发 `invalidated reason=world-reset`，取消路径工作，随后显式 F7 开窗成功 |
| 相位流 | 装备 `0→10→1`（`iState 0→9`、`IsEquipping` 脉冲、节点 36→51→33→80→65）；卸下 `1→12→0`（`iState 9→0`、`IsUnequipping`/`bEquipUnequip` 脉冲、节点 65→81→33→52→36）——与 v41 基线逐项一致：监视的 463 词玩家自有图正确，相位流可靠。五个相位名全部解析（113/94/110/111/125，与 C-VC1 索引一致） |
| 事件流 | **`events=0`，`ring-dropped=0`**——两次真实相位转移全程，引擎没有向这张图派发任何经 58377 的事件；`window-closed reason=expired events=0 phases=20` |

**裁定**：装备锚与卸下锚（`currentDefaultState` 首离 0 / 首离 1 ±10 帧）窗口内事件行为空，按预注册判据记负结果——**真实装备/卸下流程不经 58377 可见的事件驱动**。这与全部既有证据自洽：v52 的 `WeapEquip` 注入能推进候选装备相位（事件触发入口存在，但真实流程不走它）；[audit27](behavior-graph-construction-audit.md) 的三个静态外部整数 setter 位点指向真实流程的机制是**引擎代码直接写图变量、状态机按变量条件转移**。剩余未测线索收敛为本节草案预注册的那条：**卸下侧变量从未作为输入写过**——S1/S2 只测过装备侧（`iRightHandEquipped`、`IsEquipping`），`bEquipUnequip`(125) 与 `PitchManualOverride`(65) 从未写过，且两者已证存在于候选 652 表。下一个切片应复用 S3 的两段结构（`WeapEquip` → settle 门），把第二段从事件分发换成对卸下侧变量的按名 typed setter 写入。

**残余限定（如实入档）**：本仪器按设计只对源图指针过滤，无法区分"引擎不向这张图派发事件"与"引擎根本不经 58377 派发任何事件"（后者意味着"两条原生路径都过 58377"的审计覆盖不完备）。闭合它需要一个极小全局计数探针；就当前决策而言不需要——负结果已足以把下一片指向卸下侧变量。部署 INI 已回 dormant off（1,633 字节 `aa49d0f7…`）。

<a id="c-g3b-s4-unequip-variable"></a>
## C-G3B-S4：卸下侧变量写入切片（草案，2026-10-31，未实现）

**背景**：E1 判负后机制图景收敛为"引擎直接写图变量、状态机按变量条件转移"。但 v41 的 settled 行时序给出一个关键反证：引擎对 `iMagicEquipped`/`iRightHandEquipped` 的 setter 写发生在 54.504——**在卸下转移（53.652 `default` 1→12）之后 0.85 秒**，装备态变量不是卸下转移的触发。转移当刻同刻变化的只有三个脉冲加 `default` 本身：`PitchManualOverride` 0→1、`IsUnequipping` 0→1、`bEquipUnequip` 0→1。若其中之一是引擎当刻写入的输入，从 settled equipped idle 写它就应推进相位——本切片逐一检验，预注册顺序防 fishing。

**输入阶梯（每轮一名，换名仅改 INI，不改二进制）**：

1. `bEquipUnequip`（125，BOOL，C-VC1 实测）——名字直接绑定装备/卸下状态机段；
2. `IsUnequipping`（111，BOOL）——`IsEquipping` 的镜像（S2 写 `IsEquipping` 无效，但那是激活后非 equipped-idle 态的写入，时机错配，镜像名本身未测）；
3. `PitchManualOverride`(65)——类型未实测，typed setter 按 `m_type` 自选，REAL/POINTER/quad 跳过。

每轮判负才进下一轮；三轮全负 → 卸下脉冲变量路线判死，触发源既非事件也非已见变量，回到 manager 层路径/写变量监视器另行立项。

**写集**：单个名字、值 1、每进程一次，位置 = S3 的 settle 门通过之后、121 步采样窗之前。typed setter 按 `m_type`（BOOL→`SetGraphVariableBool(true)`，INT8/16/32→`SetGraphVariableInt(1)`），返回值与读回必录；名字在候选自己的 stringData 解析，缺失即 skip（不跑采样窗）。

**机制**：新键 `[Experiment] CandidateUnequipVariable`——非空时第二段从"分发 `1HM_Unequip`"改为"写该变量"，为空保持 S3 事件路径不变。门、settle、121 步采样、观测行、停止条件全部沿用 S3 已验证机制；typed 选择复用 S1 的按型逻辑，无新增策略逻辑、无离线新用例。

**判据**（每轮独立裁定）：主判据 `currentDefaultState` 1→12→0；辅判据节点数偏离基线回落、卸下脉冲出现、运动持续。命中 → 该变量即卸下转移的输入，G3-B 输入子集定为 {equip: `WeapEquip` 事件, unequip: 该变量写入}，下一片预注册两段时序容差并起草完整回放。未命中 → 按阶梯换名重跑。`iState` 照旧只记录不判定。

**非目标**：不写其它变量、不做姿态对照、不改显示/捕获/台账、不宣称 G3-B 通过；第一轮只写 `bEquipUnequip=1`——阶梯是预注册的顺序，不是同轮多写。

**停止条件**：崩溃、非有限值、`source-drift` 非 none 立即停止。

**门**：`CandidateUnequipProbe=true` + `IndependentGraph=inject` + `CandidateUnequipVariable=<名字>`。

**状态：已实现、已构建（干净身份 `…-9f1bbb9e59-…`，[v55 build](build.md#v55-unequip-variable-2026-10-31)）、已部署。** 实现仅涉及 S3 第二段的写法分支与新 INI 键（`CandidateUnequipVariable`，空 = 保持 S3 事件路径），无新增策略逻辑；写入缺名/类型不支持按 S1 惯例 skip（不跑采样窗）。部署 INI 运行后已换为第二轮名单，仓库样例保持新键为空。结果按轮回填在下方追加，不改写本节既有文字。

**第一轮（2026-10-31 16:45，v55，`bEquipUnequip`；[日志](diagnostics/README.md#g3bs4-r1) 72,238 字节 SHA-256 `685ed235…`）：判负。** settle 第三次逐点复现（`target-step=49 settled-step=63 baseline-nodes=111`）；`phase=input outcome=write name='bEquipUnequip' index=125 type=BOOL setter=SetGraphVariableBool result=true readback=1`，且该值在 121 行采样中**全程保持 1**——写入被接受且持久，但 `default` 121 点恒 1、`nodes` 恒 111、`IsUnequipping`/`PitchManualOverride` 恒 0，运动 119/119（装备 idle 照常动画），无崩溃、`source-drift=none`。主判据未命中。负结果含义明确：**值进了槽、状态机不认**——`bEquipUnequip` 读作图内输出（源侧由卸下序列自身清零，候选无序列可清）。按阶梯进第二轮 `IsUnequipping`（INI-only，已部署，v55 二进制不变）。

**第二轮（2026-10-31 17:07，v55，`IsUnequipping`；[日志](diagnostics/README.md#g3bs4-r2) 72,231 字节 SHA-256 `985f210e…`）：判负，形态与第一轮完全一致。** settle 第四次逐点复现（`target-step=49 settled-step=63 baseline-nodes=111`）；`phase=input outcome=write name='IsUnequipping' index=111 type=BOOL setter=SetGraphVariableBool result=true readback=1`，121 行全程保持 1；`default` 恒 1、`nodes` 恒 111、运动 119/119、`source-drift=none`。镜像名假设死亡。按阶梯进第三轮 `PitchManualOverride`（INI-only，已部署；其 `m_type` 未实测，REAL/指针类型将记 `unsupported-type` skip，按预注册不算写入轮）。

（附注：本轮之间有一次无效会话——17:01 的日志只有设置加载与读档，面板未打开、实验未启动，零实验行，按惯例记为无效不构成判定。）

**第三轮（2026-10-31 17:39，v55，`PitchManualOverride`；[日志](diagnostics/README.md#g3bs4-r3) 72,261 字节 SHA-256 `54436ac5…`）：判负——阶梯全负。** settle 第五次逐点复现；`phase=input outcome=write name='PitchManualOverride' index=65 type=BOOL setter=SetGraphVariableBool result=true readback=1`（类型首次落档：BOOL），121 行全程保持 1；`default` 恒 1、`nodes` 恒 111、运动 119/119、`source-drift=none`。

**阶梯裁定（三轮全负，路线判死）**：三个卸下脉冲变量从 equipped idle 单独写入全部呈现同一形态——值进槽、被持久读回、状态机不消费。结合 E1（转移当刻无 58377 事件）与 v41 全量复核（转移前 5.5 s 无持久变量活动），**已排除的输入路线：事件分发（1HM_Unequip 判负）、三个卸下脉冲变量的持久写入、装备态 setter（v41 时序证明发生在转移后 0.85 s，非触发）**。剩余假设收敛为 E2 预注册的那条：引擎在转移当刻经 typed setter 写入**瞬态**命令变量（settle-3 规则滤掉了它）。E2（v56，已构建）按部署排序条款于本轮后部署。

**第三轮后部署（2026-10-31）**：v55 三件套备份至 `dist/deploy-backup-20261031-v55/`，v56 DLL/PDB（`868e6c35…`/`68bebe2e…`，与构建档案一致）装入部署位；部署 INI（1,860 字节 `fd1c43b1…`）为 `SourceWriteCapture=true` + `IndependentGraph=off` + 其余全 off。E2 运行判据见 [E2 契约](#c-g3b-e2-write-capture)。

<a id="c-g3b-s5-two-write"></a>
## C-G3B-S5：装备态补齐 + 卸下命令的两写序列（草案，2026-10-31，未实现）

**假设**：E2 裁定后唯一未被排除且可检验的输入形态是**状态合取**——源侧 equipped idle 与候选 equipped idle 存在已证实的状态差：源在装备段中部（47.132）经非 63609 路径写入 `iRightHandEquipped=1`，候选经 WeapEquip 进入的 equipped idle 里 `rh=0`（S3/S4 各轮 `rh` 列恒 0）。若卸下转移条件是合取（`bEquipUnequip==1 且 iRightHandEquipped==1` 一类），则 S4 三轮单写全负（候选缺 rh=1）与源侧照常触发完全自洽。本切片回放真实序列：**先补齐装备态，再下卸下命令**。

**写集（两写，有序，每进程一次，位置 = settle 门通过后、121 步采样窗前）**：

1. **前置写** `iRightHandEquipped`（179，INT32，C-VC1 实测）值 1——补齐真实装备态；
2. **命令写** `bEquipUnequip`（125，BOOL，C-VC1 实测）值 1——卸下命令。

typed setter 按 `m_type` 自选（沿用 S4 机制），两写的返回值与读回必录；任一名字缺失或类型不支持即 `outcome=skip`，不跑采样窗。两写之间无人工延时——源侧两写相距 6 秒以上（47.132 与 53.652），但合取条件只看值组合与时序窗口；若判负，"需要中间步数间隔"作为残留假设入档（后续可用 INI 在两写间加预注册步数的变体再试，不属本契约）。

**机制**：新键 `[Experiment] CandidateUnequipPrime`——非空时第二段在命令写之前先做前置写，为空保持 S4 单写行为。其余（门、settle、121 步采样、观测行、typed 选择、停止条件）沿用 S4/S3 已验证机制；写入路径抽一个共用 helper（S4 现有写法改为调用它），无新增策略逻辑、无离线新用例。门：`CandidateUnequipProbe=true` + `IndependentGraph=inject` + `CandidateUnequipVariable=bEquipUnequip` + `CandidateUnequipPrime=iRightHandEquipped`。

**判据**：主判据 `currentDefaultState` 1→12→0；辅判据节点数偏离基线回落、卸下脉冲出现、运动持续。命中 → 卸下触发 = 装备态合取，G3-B 输入子集定为 {equip: `WeapEquip` 事件, unequip: rh 补齐 + `bEquipUnequip` 命令}，进入完整回放契约起草（预注册两段时序容差）。未命中 → 状态合取假设判死，按 E2 裁定转向图内结构分析（嵌套状态机/转移条件静态审计），不再有输入侧切片。`iState` 照旧只记录不判定。

**非目标**：不写第三个变量（`iMagicEquipped` 的同款补齐留作判负后的记录性变体）、不做姿态对照、不宣称 G3-B 通过。

**停止条件**：崩溃、非有限值、`source-drift` 非 none 立即停止。

**状态：已实现、已构建（干净身份 `…-227d5104a3-…`，[v57 build](build.md#v57-two-write-2026-10-31)）、已部署待上机。** 实现按契约：S4 的写块抽成共用 `typed_write` lambda（skip 行带 `stage=` 标记），settle 后先 `prime` 后 `command`，任一失败即结束不跑采样窗；`CandidateUnequipPrime` 为空保持 S4 单写行为、`CandidateUnequipVariable` 为空保持 S3 事件路径。部署 INI = `inject` + `CandidateUnequipProbe=true` + `CandidateUnequipPrime=iRightHandEquipped` + `CandidateUnequipVariable=bEquipUnequip`，其余全 off。首轮结果回填时在下方追加，不改写本节既有文字。

**首轮结果（2026-10-31 17:53，v57；[日志退役/恢复](diagnostics/README.md#g3bs5)，原 CRLF 72,647 字节、SHA-256 `c387aa9a…`）：判负。** settle 第六次复现（`target-step=49 settled-step=63 baseline-default=1 baseline-nodes=111`）；先 `stage='prime' iRightHandEquipped`（179，INT32），再 `stage='command' bEquipUnequip`（125，BOOL），两次 setter 均 true、读回均 1，121 行持久；`default=1`、`nodes=111` 全程不变、`source-drift=none`。只否定该 settle 时机的两写合取，输入侧切片按预注册停止并转 E3/A1；其后成功事件路线见 [S6](#c-g3b-s6-unequip-event-retry)/[F](#c-g3b-f-full-replay)，不能把该负结果扩为所有事件或输入均无效。

<a id="c-g3b-e3-transient-watch"></a>
## C-G3B-E3：源图瞬态写监视（草案 → 实现，2026-10-31）

**背景**：S5 判负后输入侧路线按契约退役，但 E2 留下一条已证事实撑起的观察假设：引擎**确实直接写**源图词数组（装备态写不经 63609，E2 `writes=0` 而 v41 词变化可见）——那么卸下触发也可能是**同样的直接写 + 瞬态**（1–2 帧内被图消费/清零），恰好落进 v37 settle-3 规则的盲区。本探针把盲区关掉：**BOOL/INT 类型的词变化逐帧立即落行（settle-0），REAL/quad 保持 settle-3**（滤 TimeDelta 类帧标量洪流）。任何一帧命令写及其回退都无法遁形。

**机制**：`[Experiment] SourceTransientWatch`（默认 off）作为 SourceTimeline 的模式修饰键，非独立窗口。baseline 时按 `variableInfos.m_type` 为每个索引置瞬态标志（BOOL=0/INT8=1/INT16=2/INT32=3）；采样时瞬态索引**与最近落行值比较**，任何变化立即出 `mode=transient` 行（含回退变化），其余类型与全部既有规则（身份、取消、预算 800 行、settle-3）不变。窗口行带 `mode=transient|settled`。门：`SourceTimeline=true` + `IndependentGraph=off`。

**判据（预注册）**：run sheet 同 E1/E2（F7 → 装备单手武器 → ~2 s → 卸下 → ~2 s）。以卸下转移（`default` 首离 1）±10 帧为锚：**锚内出现四个已知变量之外的 `mode=transient` 行** → 该 (名, 值) 即触发候选，S6 沿 S4 机械写它（瞬态变量则需带步数间隔的变体，另行预注册）；**锚内只有四个已知变量** → 词空间彻底排除，转移为图内部逻辑驱动（嵌套状态机/转移条件静态审计立项）。装备锚（首离 0）预计能看到装备态写（v41 见过的 `iMagicEquipped`/`iRightHandEquipped` 等）——它们同时是仪器正控：E2 已证这些写不经 63609，settle-3 能看到它们说明写法是持久的；瞬态模式对它们应产生与 v41 一致的行（不重不漏）。

**非目标**：不写变量、不注入、不触碰候选；REAL 帧标量仍被 settle-3 滤除（若触发恰为 REAL 瞬态写入则本探针盲，如实入档——概率低，已见 REAL 活动仅 TimeDelta）。

**停止条件**：沿用 SourceTimeline 全部规则（源丢失/替换取消、预算耗尽终止、崩溃停止）。

**离线用例**：无新增纯逻辑（settle 阈值分支在既有 recorder 内，行为由窗口数据裁定）；按 S1/E3 前例不凑用例。

**状态：已实现、已构建（干净身份 `…-65c069e1ba-…`，[v58 build](build.md#v58-transient-watch-2026-10-31)）；首轮已上机，词空间按判据彻底排除（见下），部署 INI 已回 dormant（v58 DLL 留装）。

**首轮运行（2026-10-31 17:59，v58，AE 1.6.1170，actor 00000014；[日志](diagnostics/CharacterPanel-g3be3-transient-watch-20261031-1759.log) 9,558 字节 SHA-256 `479e1e4d…`）：词空间按预注册判据彻底排除。** 瞬态模式全程工作：int/bool 行全部 `mode=transient`、REAL/quad 保持 settle（`TimeDelta`/`Pitch` 走老规则）。读档取消与显式重 arm 照常（`invalidated reason=world-reset` → `window-open … mode=transient`）。两次转移完整入窗：

- **装备锚**（`default` 0→10 @17:59:25.866）：`IsEquipping`、`bVoiceReady`/`bIsInMT` 1→0、`iLeftHandEquipped` 0→9、`bAttached` 0→1、`iMagicEquipped`/`iRightHandEquipped` 0→1、ready 组与 `iState 0→9`——正控通过（v41 已知行全部复现），**无新增变量**。
- **卸下锚**（`default` 1→12 @17:59:32.780）：**同帧恰好四个已知变量**（`PitchManualOverride` 0→1、`IsUnequipping` 0→1、`bEquipUnequip` 0→1、`default` 本身），锚前后 ±10 帧亦无四个已知变量之外的任何行。
- **仪器有效性旁证**：`iLeftHandEquipped` 在 17:59:33.649–33.915 出现 **9→0→9→0 单帧闪烁**——settle-3 规则看不见、v41 从未记录的瞬态被当场抓住，证明瞬态确实存在于此图、仪器确实能看见，只是不在触发点上。

**裁定**：按预注册判据——锚内只有四个已知变量（S4/S5 已证单独或 rh 组合写入均不触发）——**源图词空间作为触发源彻底排除（持久与瞬态一并排除）**。合并全部实验：转移触发既不在事件入口、不在 typed setter、不在任何词写（持久或瞬态、单写或合取），剩余唯一假设域是**图内部逻辑**——转移由状态机自身条件驱动（嵌套状态机、clip 时间/注释条件、或对未纳入观测的结构性状态的响应）。下一阶段为**图内结构静态审计**：从源图内存（或 1HM HKX）解出 equipped idle 态的出边转移表及其条件绑定（事件/变量/通配），定位 1→12 转移的真实条件表达式，另行立契约。

<a id="c-g3b-e2-write-capture"></a>
<a id="c-g3b-e2-write-capture"></a>
## C-G3B-E2：源图 setter 写观察（草案，2026-10-31，未实现）

**背景**：v41 原始日志（15,705 字节全量，非选摘）复核给出新事实：**从装备完成（48.159）到卸下转移（53.652）的 5.5 秒里，源图变量空间唯一的活动只有 `TimeDelta`（REAL 帧时长抖动）**——没有任何持久性命令变量先于转移。结合 E1（无 58377 事件）与 S4r1（`bEquipUnequip` 持久写入不触发），剩余假设只有一个可检验形态：引擎在转移当刻经 **typed setter**（`BShkbAnimationGraph::SetGraphVariableBool/Int`）写过某个变量，但该写入是**瞬态**的（写后 1–2 帧内被图自身消费/清零），被 v37 的 settle-3 规则滤掉。本探针在 setter 入口直接观察，瞬态与非瞬态都逃不掉。

**机制**：复用 E1 已验证的只读 detour 机械，两个 hook：`SetGraphVariableBool`（REL 63607，VA `0x140bb6600`）与 `SetGraphVariableInt`（REL 63609，VA `0x140bb6790`；本机 `versionlib-1-6-1170-0.bin` 按 CommonLib IDDB v2 增量格式解析得出，解析器以 58377→`0xab58a0` 与 E1 交叉验证）。两函数头部同为 `48 89 5c 24 10`（`mov [rsp+10h],rbx`，5 字节、RIP 无关、边界恰在 +5）——5 字节 jmp 无需填充、仅重定位这一条指令。运行时安装前照 E1 惯例核对头部字节。**Float setter（63608）不在范围**（值通道为 xmm2，且已知 REAL 活动只有帧时长噪声）；若判据需要再单独立项。

**回调读集与纪律**：`this`（图指针）、`BSFixedString*`（名字）、值（r8）。记录函数在调用现场**立即复制名字 ≤64 字节**进环（调用者的名字对象是栈临时，render 侧延迟读会悬垂——这是与 E1 事件行的关键实现差异），随记录 {图指针, 名字, 值}；无锁 SPSC 环、溢出计数，其余纪律同 E1（不分配、不持锁、不调引擎函数、立即转发）。**门**：源图集合（采样器 baseline 时发现的玩家全部非第一人称图，≤4 指针的原子数组）——E1 只盯一张图，本探针盯玩家所有图，一并闭合"事件/写去别的图"的残余。窗口关闭时 gate 为空直接放行。

**窗口与预算**：新键 `[Experiment] SourceWriteCapture=false`；`IndependentGraph=off`；与 SourceTimeline/SourceEventCapture 三者互斥（同开属非法配置，加载告警，优先级 SourceTimeline > SourceEventCapture > SourceWriteCapture）。F7 开 20 秒窗；采样器同窗记录 settled 相位行（五个装备链变量 + 节点数，settle-3 规则）作对齐锚；写行预算 300 行/窗，相位行 200 行/窗，任一耗尽即关。

**判据（预注册）**：run sheet 同 E1（F7 → 装备单手武器 → ~2 s → 卸下 → ~2 s）。以卸下转移（`default` 首离 1）±10 帧为锚：**锚内有 setter 写行** → 该 (名, 值) 即触发候选——S5 从候选 equipped idle 写它（沿用 S4 机械，INI-only 换名）；**锚内无任何写行** → 与 E1 合并得出"外部调用路线全线判死"（事件 + setter 都不在转移当刻发生），转移由图内部逻辑驱动，路线转向图内结构分析（嵌套状态机/转移条件审计），另行立项。装备锚（首离 0）同样记录作对照——预计能看见 audit27 的装备态写（46.634 段），这同时是仪器正确性的正控。

**非目标**：不写任何变量、不注入事件、不触碰候选、Float setter 不观察、不宣称 G3-B 前置完成。

**停止条件**：崩溃（detour 崩溃即回退纯观察、INI 回 off、归档 crash 报告）；源丢失/替换/读档取消照既有规则。

**离线用例**：名字复制的环（含悬垂防护：调用现场复制）、门数组匹配、双预算——`cl /W4 /WX` 全过。

**实现影响面**：新模块 `source_write_capture.{h,cpp}`，`PLUGIN_SOURCES` 31→32；stub 构造器与 E1 同款复制（记录签名不同，不强抽公共件）；文档同步按维护约束。**部署排序：S4 阶梯（第二/三轮）仍在同一部署 INI 上等待上机，E2 构建完成后不部署，阶梯判死或打平后再部署**——两实验共用 F7 与部署位。

**状态：已实现、已构建（干净身份 `…-a185603f5a-…`，[v56 build](build.md#v56-write-capture-2026-10-31)）、首轮已上机判负后部署 INI 回 dormant（v56 DLL 留装）。**

**首轮运行（2026-10-31 17:43，v56，AE 1.6.1170，actor 00000014；[日志](diagnostics/CharacterPanel-g3be2-write-capture-20261031-1743.log) 7,315 字节 SHA-256 `6c3055fc…`）：typed-setter 假设判负，且正控同空。** 两条 detour 运行时头部核验通过并安装（Bool→`0x7ff758546600`、Int→`0x7ff758546790`，与解析库一致）；F7 开窗 `window-open graphs=2 words=463`——玩家两张图都在门内（E1 的"事件/写去了别的图"残余闭合）；两次真实转移完整发生在窗口内（装备 17:43:00.7–02.2、卸下 17:43:07.6–08.9，与 v41 轨迹逐项一致）；**`writes=0`、`ring-dropped=0`、`window-closed reason=expired writes=0 phases=21`**。

**裁定**：按预注册判据，两个锚（装备/卸下 ±10 帧）内均无写行 → **typed setter 路线判死，"外部调用路线"全线关闭**：转移当刻既无 58377 事件（E1）也无 Bool/Int setter 调用（本探针，双图覆盖）。**正控同空是本裁定最重的部分**——v41 明确观察到装备段中部的 `iMagicEquipped`/`iRightHandEquipped` 0→1，而 63607/63609 上零调用，说明引擎的装备态写**不经** `BShkbAnimationGraph::SetGraphVariableBool/Int`（更可能是 [audit27](behavior-graph-construction-audit.md) 的 manager 层入口 AE32886 直接写字数组）。instrument 判定本身以装备锚为正控的假设被推翻，如实入档。

**全部证据合并后的机制图景与 S5 假设**：已排除 = 事件分发、三个卸下脉冲变量的单独持久写、装备态 setter 作为触发、63607/63609 写入路径、转移前持久命令写。**未被排除**：源侧 equipped idle 与候选 equipped idle 的**状态差**——源在装备段中部（47.132）已有 `iRightHandEquipped=1`（经非 63609 路径写入），候选经 WeapEquip 进入的 equipped idle 里 `rh=0`（S3/S4 各轮 `rh` 恒 0）。若卸下转移条件是合取（如 `bEquipUnequip==1 且 iRightHandEquipped==1`），则 S4 三轮单写全负、源侧照常触发就完全自洽。**S5（草案）**：两写序列回放——settle 门通过后先写 `iRightHandEquipped=1`（补齐真实装备态），再写 `bEquipUnequip=1`（卸下命令），判据不变（`default` 1→12→0）。

<a id="c-g3b-a1-structural-audit"></a>
## C-G3B-A1：图内结构静态审计——离线 HKX 解析（草案 → 实施，2026-10-31）

**背景与目标**：S3–E3 六轮上机已把卸下转移（源图 `currentDefaultState` 1→12）的全部外部输入路线判死（见 [handoff-20261031](#handoff-20261031) 合并裁定），剩余假设域是图内部逻辑。本契约是预注册排序里的下一阶段唯一工作包：**纯离线解析运行时行为链的 HKX 文件，解出 equipped idle 态的出边转移表与条件绑定，回答"1→12 转移的真实条件是什么"**。纯只读：不触碰游戏进程、不部署 DLL、不写任何变量。

**审计对象（运行时实际生效文件，MO2 冲突解析锁定为 Pandora_Output，2026-10-31 哈希）**：

| 文件 | 字节 | SHA-256 |
| --- | --- | --- |
| `meshes/actors/character/Characters Female/DefaultFemale.hkx` | 809,056 | `5f953a3749b431a9444368a43411b904b6c14830ce89f5808e971c822c139cd0` |
| `meshes/actors/character/Behaviors/0_Master.hkx` | 12,951,504 | `466cf1169ee0a0af`（前缀，全量见构建节） |
| `meshes/actors/character/Behaviors/1hm_behavior.hkx` | 1,505,120 | `c56f96bbd2e291e5`（前缀） |
| `meshes/actors/character/Behaviors/weapequip.hkx` | 75,456 | `fd0f0e096eca0c58`（前缀） |

外加 `hkbBehaviorReferenceGenerator.behaviorName` 引用链闭包（引用文件相对 `Behaviors\` 解析）。审计范围限定于此机当前行为链（含 BFCO/Pandora 修改），**不外推原版 1HM**。

**侦察已确立的结构事实（2026-10-31 准备轮，工具链端到端验证通过）**：

1. 运行时 652 变量表 = 0_Master 自有 389 变量 + 引用文件变量**按名统一合并**；0_Master 自有索引与 C-VC1 运行时普查逐项一致（`currentDefaultState=113`、`iState=94`、`iRightHandEquipped=179`、`bEquipUnequip=125`、`PitchManualOverride=65`、`WeapEquip=事件2`、`1HM_Unequip=事件15`）。
2. 全项目链唯一条件类为 `hkbExpressionCondition`（条件=表达式字符串）；无事件/变量/剪辑/姿态匹配条件类实例。触发机制候选域因此收窄为：**表达式条件、转移 `triggerInterval`（enter/exit 事件或本地时间窗口）、嵌套状态机/父机 `fromNestedStateId` 逻辑、表达式脚本层赋值**。
3. `currentDefaultState` 已见的写入机制：一处 `sm[currentStateId] → var[113]` 绑定（DragonRider 机器，5 态）+ 表达式赋值 `currentDefaultState = 0`；**写出 10/1/12 的机制尚未定位**——这正是本审计的主判据对象。
4. 装备链结构锚点：`toForceEquipState` 机器（8 态 id 7–14，startStateId=7=`toWeapEquip`）、`WeapEquipBFR → behaviors/weapequip.hkx`、`1HM_UnequipState`/`1HM_UnequipMSG`/`1HM_Unequip_ModifierGenerator` 节点名。

**工具**：[HKX2-Enhanced-Library](https://github.com/Monitor221hz/HKX2-Enhanced-Library)（`91fa5b0`，C# net10；序列化布局尺寸 `hkbStateMachine=0x108` 与 pinned CommonLib 运行时类一致，交叉验证）+ 本地 `build/hkx-audit/HkxDump`（gitignored，转储工具）。首轮冒烟与运行时索引一致性已核对。若结论存疑，用用户提供的 HKX_Edit 交叉验证。

**读集（全部只读）**：全部状态机 `states[]`/`transitions[]`/`wildcardTransitions`（condition 表达式全文、eventId→名、triggerInterval、toStateId/fromNestedStateId、priority、flags）；StateInfo 的 enter/exit notify 事件；全部 `hkbVariableBindingSet`（memberPath/variableIndex/bitIndex/bindingType，**各文件按自己的变量名表解析**）；全部 `hkbEvaluateExpressionModifier` 的表达式（含 `assignmentVariableIndex`/`assignmentEventIndex`/`eventMode`）；`hkbClipGenerator` 的 clip trigger 数组（localTime+事件）。解析器行为约束：无限深度遍历、对象按引用去重（MSG 扇出不得重复展开）、引用文件跟进加载。

**判据（预注册）**：

1. **主判据**：找到产生 `currentDefaultState` ∈ {1, 10, 12} 的机制集合（绑定 / 表达式赋值 / 转移），并核对它能覆盖运行时实测序列 0→10→1→12→0（v41/E1）。机制不唯一时全部列出并标注证据强度，不择一采信。
2. **1→12 转移的触发条件**：给出其所在状态机的对象路径、状态 id、转移条目全文（条件表达式 / eventId→名 / triggerInterval / 优先级 / flags）。若为表达式条件，引用表达式原文；若为时间窗，给出 enter/exit 事件与时间值。
3. **自洽性核对表**：答案必须逐项解释 E1（转移当刻零 58377 事件）、E2（零 typed setter 调用）、E3（卸下锚同帧恰四个已知变量）、S4/S5（单写与合取写均不触发候选）。任何一项解释不了即如实记录"部分解释"，不得强行裁死。
4. **产出判定**：答案决定 G3-B 卸下段形态——触发若为表达式/事件可注入类 → 候选侧回放可行（`WeapEquip` 先例适用）；若依赖 clip 时间/嵌套结构/引擎直写 → 卸下段回放重定义范围（仅装备段回放 + 卸下段独立议题）。

**非目标**：不写变量、不注入事件、不上机、不部署、不改插件代码与配置；不把本机行为链结论外推到原版；不据此宣称 G3-B 通过。

**停止条件**：解析器对任何目标文件反序列化失败、或解析输出与原始字节字符串扫描矛盾（如类清单缺 `hkbStateMachine`）→ 停止并如实报告，不猜测补全；运行时 652/8223 计数若无法由合并解释，按矛盾记录处理。

**状态：准备完成（本节侦察事实即产物），全量结构提取实施中。** 结果回填在下方追加，不改写本节既有文字。

**实施与结果（2026-10-31，C-G3B-A1 首轮全量结构提取，纯离线）**

工具落地：[HKX2-Enhanced-Library](https://github.com/Monitor221hz/HKX2-Enhanced-Library) `91fa5b0`（C# net10，dotnet 10.0.403 本机构建）+ 转储程序 `build/hkx-audit/HkxDump`（gitignored；能力：反序列化 Skyrim SE hkx、无界深度遍历、对象按引用去重、`hkbBehaviorReferenceGenerator` 引用跟进含 MO2 VFS 回退、修饰符级 `hkbVariableBindingSet` 转储——`hkbNode : hkbBindable`，修饰符也带绑定集）。环境注记：全局 NuGet.Config 存在失效 vcpkg 本地源（`C:\src\vcpkg\scripts\buildsystems` 不存在），已在 `build/hkx-audit/NuGet.config` 用 `<clear/>` 局部绕过，全局配置未改。

**结构事实（全部来自解析，哈希锚定）**：装载链闭包 = 23 个行为文件（Pandora_Output 17 + 模组松散 6：modernstaggerlock/ItemUse/FNIS×3/skyparkour/maxsuBlockHit），主转储 112,143 行。装备链核心 = `0_Master.hkx` 内 `root/Root/mg/msg/msg/msg/Default_State`（9 态，wildcard=7）：`0=MT_Behavior_State`（未装备）、`1=Weap_Readied_State`（装备 idle）、`2=Magic_Behavior_State`、`10=Weap_Equip_State`、`11=Magic_Equip_State`、`12=Weap_Unequip_State`、`13=Magic_Unequip_State`、`14=KillMove_State`、`20=NonLethal_PairedAnimations`。**运行时 `currentDefaultState` 的 0→10→1→12→0 序列即该机状态 id 序列**。

**主判据答案：1→12 的真实条件是事件注入，不是变量、不是时间窗。** `Weap_Readied_State(id=1)` 的出边（全文）：

```
TRANS toState=12 eventId=144(Unequip)           cond=null  trigger=[0,0] flags=256
TRANS toState=10 eventId=2(WeapEquip)           cond=EXPR[IsNPC == 0] flags=0
TRANS toState=12 eventId=982(BoundWeap_Unequip) cond=null  trigger=[0,0] flags=256
```

`flags=256 = FLAG_DISABLE_CONDITION`（条件求值关闭），trigger 区间未启用（无 `FLAG_USE_TRIGGER_INTERVAL`）。**S3 判负的直接原因即注入了错误事件名**：`1HM_Unequip`(id=15) 存在于事件名表但不是任何 Default_State 转移的事件；正确名是 **`Unequip`（0_Master 表 id=144，候选共享同一 stringData）**。Default_State 自身 7 条通配全部指向 10/11/0（force-equip 与 ReanimateExit），父机（68 态机）通配无指向 12 者——**1→12 全链唯一路径就是事件 144（加 982 备选）**。

**完整状态链（装备段 + 卸下段，全部事件驱动、条件关闭、剪辑尾触发器收尾）**：

| 转移 | 事件 | 收尾机制 |
| --- | --- | --- |
| 0→10 | `WeapEquip`(2) 注入/游戏派发 | — |
| 10→1 | `WeapEquip_Out`(49) | weapequip.hkx 剪辑尾触发器 `t=0 relEnd=True`（v52 候选 step-49 自证内部发射） |
| 1→12 | **`Unequip`(144)** 注入/游戏派发 | — |
| 12→0 | `Unequip_Out`(50)（移动中走 76 `Unequip_OutMoving`） | 卸下剪辑尾触发器 `t=0 relEnd=True`（移动变体 `t=-0.25 relEnd=True`） |

**卸下锚四变量签名（E3）的结构解释**：`PitchManualOverride`(65) = `sm[Weap_Unequip_State/mg] isActive` 绑定；`IsUnequipping`(111) = `sm[.../Weap_UnequipRoot/b] isActive` 绑定 + 两个 `hkbEventDrivenModifier.enable` 绑定；`bEquipUnequip`(125) = `sm[.../Weap_UnequipRoot/b] isActive` 绑定；`currentDefaultState`(113) 见下。**三个脉冲全部是卸下子机器激活/去活的绑定输出**——S4 写它们不触发、S5 合取也不触发，与"输出不可作输入"完全自洽。装备段同构：`IsEquipping`(110) = Weap_Equip_State 的 EventDrivenModifier.enable 绑定。

**currentDefaultState 写者（判据 1 的残余未知，如实记录）**：静态 23 文件中 `member=currentStateId` 绑定共 6 处（root→i1HMState(39)、Block_Standing→iBlockState(22)、Magic 两处→testInt(47/48)、Magic_Behavior_State/b→iEquippedItemState(118)、DragonRider→currentDefaultState(113)），**Default_State 及其父机均无绑定**；表达式层无赋值（293 条表达式无一写它；raw 字符串区的 `currentDefaultState = 0`/`weaponSheathe if(1)` 为无引用孤儿串）。但 v52 候选（无 manager、无游戏代码）实测 `default` 跟踪 10→1——**写者在图求值内部，判定为引擎装载期建立的"指定状态机 currentStateId→具名变量"镜像机制**（绑定机构存在性的先例即 DragonRider 绑定）；其建立来源（project DB/character data 何处声明）为残余未知，记录待查，不影响本判据。

**自洽性核对表（判据 3）**：E1（转移当刻零 58377 事件）→ 事件 144 的真实派发不经 58377，属 E1 预注册时已声明的残余（manager 级动画事件派发路径未审计，路径存在性由本审计反证：转移必须由 144 触发且 144 无任何图内发射点——**游戏/引擎在图外发射**）；E2（零 setter）→ 全部变量写为绑定求值，非 setter 调用；E3（锚内恰四变量）→ 三脉冲为 isActive 绑定、第四个是状态镜像本身；S4/S5（单写/合取写不触发）→ 脉冲变量是输出；S3 → 错误事件名。**六轮负结果全部得到结构解释，无遗留矛盾。**

**G3-B 卸下段形态判定（判据 4）**：**事件可注入类——候选侧回放可行**。外部输入子集 = {`WeapEquip`(2)，`Unequip`(144)}，注入机制沿用 S3 已验证的 `lookup_event_id_in_graph`（按名解析）+ `InjectedEvent` + `event_dispatch`；两段收尾（WeapEquip_Out/Unequip_Out）由剪辑尾触发器内部发射，无需注入。`iState` 由引擎 `BSiStateTaggingGenerator.iStateToSetAs` 维护（候选侧恒 0 的现象与真实装备上下文假设仍留 G3-B 本体裁定）。

**判据外矛盾记录**：运行时合并表 652 变量/8223 事件无法由静态链（23 文件，变量名并集 526、事件名并集 9332，2^20 子集穷举无一同时命中）重现——事件方向运行时更少、变量方向更多，指向运行时注入内容（OAR 系改图最可疑）或引擎合并的额外规则。不影响本轮结论：全部装备链变量/事件都在 0_Master 主表（389+8173）且索引与 C-VC1 运行时普查逐项一致（113/94/179/125/110/111/65/117/180/38，事件 2/15/49/50/144/982）。

**关键文件哈希（完整）**：0_Master.hkx `466cf1169ee0a0afad01532d3f1b9a1b1d410dc8dcc3ad3045bb1aae65775e61`；1hm_behavior.hkx `c56f96bbd2e291e5d9ad5ea45907c1e2b147ca4ad50e0d2e03d86e7566ca1d0f`；weapequip.hkx `fd0f0e096eca0c5863b33e854883002ab9e19512aebc2d7161ae0a6ee5bb36e6`；DefaultFemale.hkx（Pandora_Output Characters Female）`5f953a3749b431a9444368a43411b904b6c14830ce89f5808e971c822c139cd0`；运行时实际接受的根路径为 `Actors\Character\DefaultFemale.hkx`（S3 日志 load-try，解析到 BSA 原版根项目，其 behaviorFilename 单值指向 0_master）。

**状态：首轮完成，主判据/自洽性/形态判定全部落档。** 下一步（另立契约）：C-G3B-S6——候选 equipped idle settle 后注入 `Unequip`(144)，判据沿用 S3 骨架（主判据 `default` 1→12→0，辅判据三脉冲 isActive 绑定按序出现 + Unequip_Out 内部发射回 0）。

<a id="c-g3b-s6-unequip-event-retry"></a>
## C-G3B-S6：Unequip(144) 事件注入切片（草案 → 实现，2026-10-31）

**目标**：只证一件事——候选在装备相位 settle 后接受**一次** `Unequip` 事件注入，能否自行进入并走完卸下相位（`currentDefaultState` 1→12→0）。这是把 v52 已验证的事件注入机制从装备段延伸到卸下段，修正 S3 的错误事件名后重跑同构切片，不是 G3-B 本体。

**依据（C-G3B-A1 结构审计）**：`Weap_Readied_State(id=1)` 的唯一 1→12 出边 = `eventId=144(Unequip) cond=null flags=256(FLAG_DISABLE_CONDITION)` 无时间窗（备选 `BoundWeap_Unequip`=982）；S3 判负的直接原因是注入了 `1HM_Unequip`(15)——该名存在于事件表但不是任何 Default_State 转移的事件。卸下锚三脉冲是卸下子机器 isActive/enable 绑定输出，12→0 收尾由卸下剪辑尾触发器 `t=0 relEnd=True id=50(Unequip_Out)` 内部发射。

**输入（单名预注册）**：`Unequip`。按 S3 的防 fishing 规则本轮只注入此一名；判负则负结果入档回 A1 证据，不试第二个候选名。

**机制**：复用 S3 两段结构（`WeapEquip` 激活注入 → settle 门）与 v52 已验证的 `lookup_event_id_in_graph` 按名解析 + `InjectedEvent` + `event_dispatch`。唯一新机制是**事件名来源**：新键 `[Experiment] CandidateUnequipEvent`——非空时第二段分发该名，空时保持 S3 硬编码 `1HM_Unequip`（历史 INI 不变即可逐字复现旧轮）。门：`CandidateUnequipProbe=true` + `IndependentGraph=inject` + `CandidateUnequipEvent=Unequip`，且 `CandidateUnequipVariable`/`CandidateUnequipPrime` 留空（变量路径优先，留空才走事件路径）。

**时机**：settle 门通过后（v38 三连续稳定采样规则，目标 `currentDefaultState`=1），与 S3 相同；等地上限 120 步。

**观测**（只读，沿用 S3）：settle 行 + `phase=event-lookup name='Unequip' id=` + `phase=unequip-inject` + 121 行 `CANDIDATE UNEQUIP step=… default=… istate=… uneq=… bequip=… pitch=… rh=… nodes=…` + `phase=unequip-motion`（119 次相邻 pose 比较）+ 三点 pose 哈希 + `source-drift`。A1 已证三脉冲为绑定输出，预期与转移同帧出现；运行时解析 id 由 event-lookup 行记录——按名解析不依赖预注册 id，若 ≠144 如实记录（合并表差异已由 A1 矛盾条款覆盖），不中止。

**判据（预注册）**：主判据 `currentDefaultState` 1→12→0；辅判据节点数偏离 settle 基线（111）并回落、三脉冲出现、运动持续。命中 → G3-B 外部输入子集正式定为 {`WeapEquip`(2)，`Unequip`(144)}，下一步起草 G3-B 本体完整回放契约（预注册两段时序容差）。未命中 → 负结果入档回 A1 证据。`iState` 只记录不判定（A1：由引擎 `BSiStateTaggingGenerator` 维护；候选恒 0 的装备上下文问题留 G3-B 本体裁定是否作为必需判据）。

**非目标**：不写任何变量、不做姿态对照验收、不触碰显示/捕获/台账、不宣称 G3-B 通过。

**停止条件**：崩溃、非有限值或 `source-drift` 非 none 立即停止并保留日志。

**实现影响面**：`settings.h`/`settings.cpp` 新键 + `independent_graph.cpp` 事件名选择；无新文件（`PLUGIN_SOURCES` 32 不变）、无新增策略逻辑（按名解析机制已有），无离线新用例（判据只能运行时取，凑恒真断言无意义，沿 S1/E3 前例）。

**实现与部署（2026-10-31，v59）**：`CandidateUnequipEvent` 键落在 [settings.h](../src/animation/settings.h)/[settings.cpp](../src/animation/settings.cpp)（解析/日志/访问器，与既有实验键同款），事件名选择在 [independent_graph.cpp](../src/animation/behavior_graph/independent_graph.cpp) 的 S3 事件分支——非空用键值、空保持 `1HM_Unequip` 字面量（历史 INI 逐字复现），S3 注释块补 S6 溯源；无新文件（`PLUGIN_SOURCES` 32 不变）、无新增策略逻辑、无离线新用例。构建身份 `…-dcf05cab1e-…`、DLL/PDB 哈希见 [v59 build](build.md#v59-unequip-event-name-2026-10-31)。部署 INI 运行后回 dormant；仓库样例新键为空。首轮结果回填在下方追加，不改写本节既有文字。

**状态：已实现、已构建、已部署待上机。**

**首轮运行（2026-10-31 21:23，v59，AE 1.6.1170，actor 00000014 request=1；[日志](diagnostics/CharacterPanel-g3bs6-unequip-event-retry-20261031-2123.log) 71,530 字节 SHA-256 `237e3d1a…`）：命中——主判据与全部辅判据通过。**

| 段 | 观测 |
| --- | --- |
| 装备段 | settle 第七次复现：`phase=settle outcome=settled target-step=49 settled-step=63 baseline-default=1 baseline-nodes=111` |
| 门与事件 | `phase=event-lookup name='Unequip' id=144 source=graph-events`（与 A1 静态预测逐字一致）；`phase=unequip-inject event='Unequip' id=144 result=dispatched` |
| 卸下段 | **step=0 即 `default=12 uneq=1 bequip=1 pitch=1 nodes=126`**——转移与三脉冲在注入当帧发生；`default=12` 持续 step 0–20，**step 21 起 `default=0`**（`Unequip_Out` 由剪辑尾触发器内部发射，回到未装备态）；`nodes` 轨迹 126→130→97→115→100→97，偏离 111 基线并回落；三脉冲 step 0–32 保持 1、**step 33 起清零**（卸下子机器去活、isActive 绑定语义）；`rh` 全程 0、`istate` 全程 0（按预注册只记录）；`phase=unequip-motion comparisons=119 changed=92`（运动持续）；三点 pose 哈希 `cand-pose-0=8fb4cd33… 60=4f2aebde… 120=c407cb4e…` 全不相同（起点与 S3 装备 idle 起点同哈希）；`source-drift=none` 全程；无 `[E]`（15 条 `[W]` 均为既有 DAC/Studio 已知告警） |

**裁定**：主判据 `currentDefaultState` 1→12→0 **命中**，辅判据（节点偏离回落、三脉冲出现并按 isActive 语义清零、运动持续）全部命中。**S3 判负根因确证为错误事件名；G3-B 外部输入子集正式定为 {`WeapEquip`(2)，`Unequip`(144)}**——A1 的全部结构性预测（id 解析、注入当帧转移、脉冲绑定语义、剪辑尾触发器收尾）在运行期逐项验证。附带事实：候选卸下段 12→0 用了 21 步（源侧约 1.3 s），按 S3 契约条款不机械换算、不影响裁定。

**G3-B 现状**：装备段（`WeapEquip` 注入）与卸下段（`Unequip` 注入）均已可回放，两段收尾均由图内剪辑尾触发器自发完成。**下一步：G3-B 本体完整回放契约**——一次进程内连贯走完 Equip→Idle→Unequip 全相位（两事件 + 两收尾），预注册两段时序容差与 `iState` 判据取舍，再做姿态对照验收。

<a id="c-g3b-f-full-replay"></a>
## C-G3B-F：G3-B 本体完整回放验收（草案 → 部署，2026-10-31）

**目标**：单一进程、单一候选，仅以已证实的外部输入子集 {`WeapEquip`(2)，`Unequip`(144)} 驱动，验收候选状态机**自行走完 Equip→Idle→Unequip 全相位序列**（`currentDefaultState` 0→10→1→12→0），两处段内转移（10→1、12→0）均由图内剪辑尾触发器自发完成。这是 G3-B 的收官判定：S6 已在单进程中实际走通全链，但其预注册判据只覆盖卸下段——按"不能把后设判据追认到已结束的运行"纪律，本契约以全序列判据重新预注册并在同二进制上重跑。

**源侧对照基线（既有证据，不需新源会话）**：v41 源轨迹 `default` 0→10→1→12→0（装备 46.6–48.2，卸下 53.7–54.9）。候选侧对照同一**顺序**；节点数与时长不要求一致（图内容与装备上下文不同，S6 已证 21 步 vs 源 1.3 s）。

**机制**：**零新代码**——v59 二进制原样（身份 `…-dcf05cab1e-…`），纯 INI 观测配置。装备段轨迹由 S1 探针的观测模式补齐：`CandidateInputProbe=true` + `CandidateInputVariable=`（空 = `observe-only`，在 step 0/1/2/6/60/120 采样、不写任何变量，代码路径 [independent_graph.cpp](../src/animation/behavior_graph/independent_graph.cpp) `arm_candidate_input_probe` 空名分支）+ S6 三键（`CandidateUnequipProbe=true`、`CandidateUnequipEvent=Unequip`、变量两键空）。两探针同窗口共存（S1 激活时 arm、卸下探针独立 arm，共用 e_pre_sample）。

**判据（预注册）**：

1. **主判据（全序列，逐段）**：
   - 0→10：激活 `WeapEquip` 注入当帧——第一窗口 step-0 行 `default=10`（候选初始 0 已由 v48 普查证实）；
   - 10→1：≤120 步内自发完成——六点采样 step-60 行 `default=1` + settle 行 `baseline-default=1`（精确步号预期 ~49，已五次复现，只记录不判定）；
   - 1→12：settle 后 `Unequip` 注入当帧——第二窗口 step-0 行 `default=12`；
   - 12→0：≤121 步窗口内自发完成——`Unequip_Out` 剪辑尾触发器（S6 实测 step 21，只记录不判定）。
2. **辅判据**：两段运动持续（`pose-motion`/`unequip-motion` changed 高）；卸下段三脉冲同帧出现并按 isActive 语义清零；全程恰好两次外部注入（激活 `WeapEquip` + settle `Unequip`），10→1 与 12→0 无对应注入行；无 `[E]`；`source-drift=none` 全程。
3. **iState 裁定**：**不作为 G3-B 判据**（只记录）。v52/S6 已证候选侧恒 0；A1 定位其写者为引擎 `BSiStateTaggingGenerator`，候选侧不触发的原因（装备上下文）列为独立残余，不阻塞本判定。
4. **姿态对照**：**不在 G3-B 判据内**——候选 vs 源的逐骨姿态验收属 G5（预注册排序），G3-B 只验收相位序列、图内自发完成与运动自洽。

**命中** → **G3-B PASSED（限定范围：相位序列回放；姿态验收归 G5）**，按排序进入 G4（语境隔离）。未命中 → 负结果入档，断在哪段记哪段（段内自发转移失败将指向该段触发器的候选侧条件差异，另立切片）。`iState`/姿态不作为判据，故二者异常不构成失败。

**非目标**：不写任何变量（观测模式空名不写入）、不做源姿态对照、不做语境隔离、不触碰显示/捕获/台账、不改二进制。

**停止条件**：崩溃、非有限值或 `source-drift` 非 none 立即停止并保留日志。

**门（全 INI）**：`IndependentGraph=inject` + `CandidateInputProbe=true` + `CandidateInputVariable=`（空）+ `CandidateUnequipProbe=true` + `CandidateUnequipEvent=Unequip`（变量两键空）。观测行预算沿既有两探针上限（S1 六点 + settle/121 行卸下段），无新键。

**实现影响面**：零代码改动，`PLUGIN_SOURCES` 32 不变，无离线用例（无新逻辑）。部署仅换 INI（v59 DLL/PDB 原位不动，先备份当前 dormant INI）。

**部署（2026-10-31）**：v59 DLL/PDB 原位不动（`013e1bdc…`/`8b8fd16f…`），当前 dormant INI 备份至 `dist/deploy-backup-20261031-v59/`（INI 1,834 字节 `1d40a0eb…`），部署 INI 换为本契约配置（1,921 字节，SHA-256 `6dfb54ec28e4086747d18017a6970eb3a4f85778fc33d34b15bac131e0ace9ba`：`inject` + `CandidateInputProbe=true` + `CandidateInputVariable=` 空 + S6 三键）。

**状态：契约已预注册、已部署待上机（v59 二进制原样）。** 运行结果回填在下方追加，不改写本节既有文字。

**首轮运行（2026-10-31 21:38，v59 二进制原样，AE 1.6.1170，actor 00000014 request=1；[日志](diagnostics/CharacterPanel-g3bf-full-replay-20261031-2138.log) 85,915 字节 SHA-256 `a56920ef…`）：命中——主判据四段全过，辅判据全过。G3-B PASSED。**

| 判据 | 观测 |
| --- | --- |
| 0→10 | step-0 行 `default=10 nodes=113`（激活 `WeapEquip` 注入当帧；初始 0 为 v48 普查既证） |
| 10→1 | **`default=10` 持续 step 0–48，step 49 起 `default=1`**（`WeapEquip_Out` 内部发射；settle 行 `target-step=49 settled-step=63 baseline-default=1 baseline-nodes=111` 第八次复现）——观测模式的逐步采样把精确步号从"五次预期"升级为逐点实证 |
| 1→12 | settle 后 `phase=event-lookup name='Unequip' id=144` + `unequip-inject`，第二窗口 step-0 即 `default=12 uneq=1 bequip=1 pitch=1 nodes=126` |
| 12→0 | `default=12` 持续 step 0–20，**step 21 起 `default=0`**（`Unequip_Out` 内部发射） |
| 辅判据 | 第一窗口 `pose-motion comparisons=120 changed=119`、第二窗口 `unequip-motion 119 比较 changed=92`（两段运动持续）；三脉冲 step 0–32 保持、step 33 清零（isActive 语义）；**全程恰好两次 `result=dispatched`**（WeapEquip + Unequip），10→1 与 12→0 无注入行；无 `[E]`；`source-drift=none` 全程 |
| 确定性 | 两窗口 pose 哈希与 S6 完全一致（装备段 `bc160a8c/63f638a6/c407cb4e`，卸下段 `8fb4cd33/4f2aebde/c407cb4e`）——同配置重放确定性的直接证据 |

**裁定：G3-B PASSED（限定范围：相位序列回放）。** 候选仅凭 {`WeapEquip`(2)，`Unequip`(144)} 两个外部输入，在自身状态机上完整走完 Equip→Idle→Unequip，两处段内转移均由图内剪辑尾触发器自发完成，全程无任何变量写入。`iState` 按预注册只记录（全程 0，残余独立立项）；姿态对照按预注册归 G5。

**收尾状态**：部署 INI 已回 dormant（v59 留装）；G3-B 关闭，按排序进入 **G4（语境隔离：OAR 阶段变量来源、历史/回调/sink 隔离）**。

<a id="c-g4-context-isolation"></a>
## C-G4 契约：OAR 上下文、历史与回调隔离（草案 → 实现/部署，2026-10-31）

> **当前更正（2026-11-04）**：本节保留历史 PASSED；A4/v63 的 numeric 缺口关闭保留，C1/v66 的关闭已因 [audit30](behavior-graph-construction-audit.md#audit30) 仪器缺陷撤回、待重跑。完整 E1 的剩余证据见 [当前边界](#g5-e1-evidence-repair)。

**背景与目标**：G3-B 已证候选仅凭 {`WeapEquip`(2)，`Unequip`(144)} 即可在自身状态机走完装卸全相位序列。按预注册排序，本契约验收 **G4 语境隔离**：候选在面板显示链里被驱动时，OAR 与游戏本体动画状态互不污染——OAR 阶段变量的读取来源符合各自语境（[10-29 交接 G4 定义](#handoff-20261029)："身份/装备允许按明确设计借读，阶段状态必须对应候选"）、预览不消耗世界 Random/顺序/PlayOnce 历史、回调与 sink 作用域不跨界、关闭只清预览自有登记。三段实验 **A → B → C** 排序：A 的求值可见性结论是 B/C 的隔离前提，A 判负时 B/C 停止。

### 代码级静态前提（2026-10-31 本会话核实，反模式 12 要求当场断言）

以下六项为本契约的静态地基，全部已按现行代码（oar-idle3 @ `1edc743`）核实；实现或上机前若代码变动须重新核对：

1. **显示激活期间产品 OAR 查询不执行**：`AnimationController::advance` 的 `display_pose()` 分支（[controller.cpp](../src/animation/controller.cpp) 1187）取得候选姿态后提前返回，产品选片/`query_animation`/`ScopedEvaluation` 及 DAC 注记派发（`session->dispatch`，1222）都位于其后。产品查询与候选显示在单次 advance 内互斥——"预览语境的 OAR 求值"在当前设计下不存在，本契约以运行日志坐实该互斥。
2. **产品投影是硬编码静态值**：`preview_state()`（[controller.cpp](../src/animation/controller.cpp) 108）只填常量（Speed=0、Direction=0、IsEquipping=0 等），**不读任何活图变量**。"阶段变量读谁"在产品路径上由设计消解（都不读）；kProjected 模式下非投影条件保持"引擎自己的答案"= 读源 Actor 真实图（设计内借读，仅发生在候选关闭时）。
3. **候选驱动链为插件直调引擎入口**：`drive_candidate_step`（[independent_graph.cpp](../src/animation/behavior_graph/independent_graph.cpp) 213）直调 63602 帧管线 / 63587 更新包装 / 63589 或 63581 采样。OAR 的已知钩子面是装载期 `LoadClips` 与查询期 `GetReplacementAnimation`（[oar-hooks.md](oar-hooks.md)），候选驱动不调用这两类入口；**是否穿过 OAR trampoline 的引擎内部函数属运行期证据，由 A 段求值观察器回答**。
4. **候选为方案 A 空壳装载**：内嵌自有 hkbCharacter（+0xC0，[构造审计](behavior-graph-construction-audit.md)），holder=nullptr。OAR per-actor 上下文以 holder REFR 为键；空 holder 时其兜底取 rootNode（构造审计 129 行："空 holder 的图仍可能被 OAR 求值，取决于 rootNode 的归属"——候选 rootNode 为复制树借用期非空，兜底是否命中由 A 段运行证据回答）。
5. **候选/源变量值分储**：候选与源共享同一 `hkbBehaviorGraphData`（C-VC1 判定），但变量**值**按图实例分储——G3-B 已证对候选按名写入不触碰源。阶段哨兵（A 段）的可区分性由此保证。
6. **既有观察面可直接复用**：`OAR COND actor=… clip-key=…` 求值行（[hooks.cpp](../src/animation/oar/hooks.cpp) 994，详细模式）、`ANIMATION WORLD … clips=` 源侧选中快照（[controller.cpp](../src/animation/controller.cpp) 875，面板打开时自动发射）、S1 探针观测模式（空名 observe-only）、`source-drift` 源对照与 `SourceEventCapture` 源图事件捕获。B 段的世界选中证据采集无需新仪器行。

### 实验设计与判据（预注册，不得事后放宽）

#### A：OAR 求值可见性与来源哨兵（上机）

**哨兵构造**：候选经 G3-B 同款两次注入回放（`inject` + S1 观测 + S6 三键，[C-G3B-F](#c-g3b-f-full-replay) 配置）进入与源可区分的阶段——候选 `IsEquipping=1`/`currentDefaultState=10` vs 源 MT `IsEquipping=0`/`currentDefaultState=0`。窗口全程 `DetailedAnimationTrace=true`。

**仪器（v60 新增，最小面）**：
1. **OAR 求值窗口标记 + 交集检测**：在既有求值观察处（hooks.cpp），当候选活动窗口激活时，对每条 OAR 求值记录求值身份（actor-id、clip-key、可取得的 graph/hkbCharacter 指针）与候选身份（保留图 `g_held_graph`、其 characterInstance、复制树根）的比对；交集非零输出 `OAR CONTEXT overlap=…` 行，窗口结束时输出汇总 `OAR CONTEXT window=… evaluations=N overlaps=M`。新 INI 键 `[Experiment] OarContextWatch`（默认 false，重启生效）。
2. **显示抑制计数**：advance 显示分支每帧累计，窗口关闭时输出一行 `DISPLAY advance display-frames=N product-query-suppressed=N`（[controller.cpp](../src/animation/controller.cpp) 显示分支处，约 5 行）。

**判据（预注册）**：
1. **主判据（隔离正向）**：候选活动窗口内 `overlaps=0`——OAR 求值不触碰候选图/候选 hkbCharacter/复制树；
2. **辅判据（互斥坐实）**：`product-query-suppressed` 计数 > 0，且窗口内无产品查询产物行（`OAR COND`/`OAR bases`/选片行）；
3. **哨兵裁定（仅当 overlaps>0 时启用）**：交集求值所读取的阶段变量值须入档并裁定来源——读到**候选值**（哨兵泄漏）即 G4-A 判负；读到**源值**须解释哪条路径把源图带进该求值语境（预期嫌疑：holder=nullptr 时 rootNode 兜底命中复制树，见静态前提 4）。判负不回滚 G3-B，另立切片定位求值入口。

#### B：世界历史消耗 A/B 对照（上机，需 A 通过）

**A/A 基线（先行）**：同一存档点、同装备，**无预览**直接执行同一动作序列（拔刀-收刀 ×N，N≥3）两次，比对 OAR 选中序列可重复性。A/A 不一致说明该序列含不可控随机（OAR Random 条件或游戏内随机），对照设计无效——须换用可重复序列后**重新预注册**，不得带噪声上机。

**配置（防基线污染）**：A/A 基线与运行 A 用 `IndependentGraph=off`（纯产品路径——面板打开不得触发候选实验）；仅运行 B 用 `inject`。`ANIMATION WORLD` 行在两种配置下都发射（无面板外新仪器）。实验为每进程一次（[independent_graph.h](../src/animation/behavior_graph/independent_graph.h)），B 的后续开面板只做记录性显示请求，不再回放。

**运行 A**：固定初态 → 直接执行动作序列（每步动作后开面板记录、关面板继续）→ `ANIMATION WORLD` 行逐项记录源侧选中。
**运行 B**：读档回同一存档点（同初态）→ 打开面板跑候选显示 ≥20 秒（含两次注入回放）→ 关闭面板 → 执行相同动作序列（同逐项记录法）→ 记录。

**判据**：A/A 与 A/B 的选中序列**逐项一致**（剪辑名/绑定名/变体）→ 预览未消耗世界 Random/顺序/PlayOnce 历史，历史隔离成立；不一致 → 差异点入档，在三嫌疑（候选 native 步进、产品查询、DAC 派发）中定位消耗者后另立切片。OAR 版本、项目集合、加载顺序在两运行间不得改动；手动操作时长差异不算差异，**选中结果**才算。

#### C：关闭清理与 sink 作用域（上机，与 B 同会话可合并执行）

**流程**：候选显示 + 回放结束后关闭面板 → 记录 DisplaySession close/reset 行、DAC 恢复状态（对照 [dac.md](dac.md) 既有 `restore matched` 证据）、EquipSink 存活性（关闭后再做一次换装，预览修订照常触发）。

**判据（预注册）**：
1. 关闭只清预览自有登记：SCOPY 台账遵守既有规则（retained-total 只增不减属"刻意保留"例外；无新增未配对句柄行）；
2. 源 Actor 的 OAR 状态与 sink 存活：关闭后世界动作照常选中（B 运行后半已证），换装事件照常到达 EquipSink；
3. 候选步进全程 `source-drift=none` + `SourceEventCapture` 源图零事件（G3-B 判据复用——候选自发转移产生的事件（`WeapEquip_Out`/`Unequip_Out`）不泄漏进源图事件路由）。

### 仪器影响面（v60）

- `hooks.cpp`：求值观察处加窗口标记 + 交集检测 + `OAR CONTEXT` 行（判据 A1/A3）。
- `controller.cpp`：显示分支抑制计数一行（判据 A2）。
- `settings`：新键 `[Experiment] OarContextWatch`（默认 off）。
- 其余零新代码：S1 观测模式、`ANIMATION WORLD`、`source-drift`、SourceEventCapture 均为 v59 既有仪器。`PLUGIN_SOURCES` 预计不变（按实现定）。

### 门与部署注意

- **门**：v60 DLL/PDB 部署 + INI 按段配置（命名注意区分：**实验 A** = `inject` + S1 观测两键（`CandidateInputProbe=true`、`CandidateInputVariable=` 空）+ S6 卸下两键（`CandidateUnequipProbe=true`、`CandidateUnequipEvent=Unequip`）+ `OarContextWatch=true` + `DetailedAnimationTrace=true`；**B 段的 A/A 基线与运行 A** = `IndependentGraph=off`；**B 段的运行 B** = 实验 A 配置）。上机由用户执行，每次换配置先备份当前 INI。
- **INI 卫生（反模式 11）**：当前部署 INI 中 `CandidateUnequipEvent` 键**重复出现两次**（S6 与 F 两次部署叠加，值相同无行为影响）——v60 部署时必须清理，且部署后逐键打印回读断言。
- **停止条件**：崩溃、非有限值、`source-drift` 非 none 立即停止；A 段 overlaps 非零时停止 B/C（隔离前提不成立）。

### 非目标与残余关联

- **非目标**：不新增 DAC 事件捕获通道（G4 定义原文："DAC 要在事件捕获和隔离证明后另行接入既有受控通道"——本轮只验证 DAC 现状行为与恢复）；装载 teardown 维持 NO-GO；G5 姿态阈值不制定；不做源姿态对照。
- **残余关联**：[652/8223 合并矛盾](#handoff-20261031-night)（运行时变量表含静态文件之外的注入内容，指向 OAR 系运行时改图）——A 段求值观察若记录到 OAR 对候选图 data/变量的触碰即为该项的收敛证据；零交集则该项保持独立残余，不因 G4 通过而关闭。候选侧 `iState` 恒 0 与 `currentDefaultState` 引擎镜像机制两项残余继续独立，不阻塞 G4。

**状态：已实现并部署（v60），待授权上机。** 实现按"草案 → 实现/构建（v60）/获授权上机"推进；运行结果回填在下方追加，不改写本节既有文字。

**实现记录（2026-10-31 收尾，实现 `d2fbc4b` + 会话归属加固 `fdbd414`，v60 部署身份 `…-v60-g4-oar-context-20261031-fdbd414f45-cl94faaed0c6-20261031T143920Z`，DLL `b519b21d…` 1,456,128 字节，PDB `d4aa2f6e…`）**：

- **仪器落点**（与契约仪器清单一致，另加两条补充）：
  1. 新头文件 [candidate_window.h](../src/animation/behavior_graph/candidate_window.h)：thread-local 命中台账（depth/evaluations/overlaps/armed source actor id）+ `ScopedCandidateWindow` 守卫 + `note_candidate_window_hit`/`arm_candidate_window`/`take_candidate_window_tally[_for]`。arm/take 按**候选属主 actor 匹配**（`DisplaySession` 每注册 actor 一个，交错的非候选 session 不得重置或取走候选窗口的计数）——无候选时首个面板打开由实验自身武装。
  2. `drive_graph_step` 步进守卫（[drive_step_policy.h](../src/animation/behavior_graph/drive_step_policy.h)）覆盖全部候选 native 步进；两处预注册事件派发点（激活 `WeapEquip`、settle `Unequip`）同守卫包裹。
  3. OAR 求值命中记录：四个 detour thunk（`evaluate`/`numeric`/`evaluate-set`/`evaluate-replacement`）入口在 scope 检查**之前**记数——原生（非产品）求值同样可观测；命中行 `OAR CONTEXT hit site=… refr=… subject=…` 每窗口上限 8 行防刷屏，汇总计数不受限。overlap 判定 = 命中 REFR 的 form id == 武装窗口登记的源 actor id（无 holder 候选唯一可能的 REFR 回落来源）。
  4. 汇总发射点：实验窗口（`run_independent_graph_experiment` 作用域守卫，`window=experiment`）与显示窗口全部关闭路径（`DisplaySession::State::abort` 漏斗，`window=display-<reason>`）——零命中也发射，作为仪器武装证据。
  5. 显示抑制计数（[controller.cpp](../src/animation/controller.cpp) `advance` 显示分支）：仅统计实际返回候选姿态的帧，显示模式结束时一次发射 `DISPLAY advance display-frames=N product-query-suppressed=N`。
  6. **补充观察（超出契约原清单，实现时加入）**：装载完成后 `phase=load-bindings` 行——候选/源绑定集指针与条目数对比，回答"63585 装载是否经过与源相同的 OAR 替换管线"（无 holder 装载可能跳过 OAR 装载钩子）；条目数不等即 G5 同剪辑对齐的前置问题，等数为前置满足证据。
  7. **运行配置澄清（上机前确认，非判据改动）**：A 段判据 2 的 `product-query-suppressed` 计数依赖显示分支激活，故 A 段 INI 须含 `DisplayPose=true` + `DisplayStep=true`（门节原清单遗漏，现补齐；判据本身不变）。
- **开关**：`[Experiment] OarContextWatch`（默认 false）统一门控全部 OAR CONTEXT 行、汇总行、抑制行与绑定计数行；关断时仅余 thread-local 计数自增与 thunk 入口一次布尔检查（产品路径行为不变）。仓库样例 [config/CharacterPanel.ini](../config/CharacterPanel.ini) 已加该键（false）。
- **验证**：引擎无关用例 `behavior_graph_display_step_test`（编译到改动头 drive_step_policy.h）g++ C++23 `-Wall -Wextra -Werror` 通过（"2 display-step regression cases passed"）；无 settings 解析类测试受新键影响。
- **部署（2026-10-31）**：v59 三件套备份至 `dist/deploy-backup-20261031-v60/`（DLL `013e1bdc…`、PDB `8b8fd16f…`、dormant INI `65e57401…` 2,013 字节）；v60 DLL/PDB 装入 MO2 实例；部署 dormant INI 重建为 2,147 字节 `27f481f5…`——**修复了 v59 部署 INI 中 `CandidateUnequipEvent` 键重复两次的卫生问题**（S6 与 F 两次部署叠加所致，值相同无行为影响），并加入 `OarContextWatch=false`；部署后 20 键逐键打印回读断言全部正确（反模式 11 执行）。
- **上机门（待授权）**：实验 A = `inject` + S1 观测两键（`CandidateInputProbe=true`、`CandidateInputVariable=` 空）+ S6 卸下两键（`CandidateUnequipProbe=true`、`CandidateUnequipEvent=Unequip`）+ `OarContextWatch=true` + `DetailedAnimationTrace=true`；B 段 A/A 基线与运行 A = `IndependentGraph=off`；B 段运行 B = 实验 A 配置。

**首轮运行（2026-10-31 22:58，v60 身份 `fdbd414f45` 活日志确认，AE 1.6.1170，actor 00000014 request=1；[日志](diagnostics/CharacterPanel-g4a-oar-context-20261031-2258.log) 129,822 字节 SHA-256 `a5bb568e…`）：判据 3 命中、判据 2 部分成立、判据 1 出现计划外证据类——G4-A 未决，另立 A2 切片。**

| 判据 | 观测 |
| --- | --- |
| 3（复用） | **命中**：恰好两次 `result=dispatched`（`WeapEquip` id=2 + `Unequip` id=144）；step 0 `default=10`、step 49 `default=1`、settle `target-step=49 settled-step=63 baseline-default=1`（第九次复现）、卸下段 step 21 自发回 0；`drive-done candidate-pose-0/60/120 = bc160a8c/63f638a6/c407cb4e`、`unequip-done = 8fb4cd33/4f2aebde/c407cb4e`——与 S6/F 逐位一致（确定性重放）；`pose-motion changed=119/92`；`source-drift=none` 全程；零 `[E]` |
| 2（辅） | **部分成立**：全程恰好 1 行 `OAR COND`（22:58:01.868，实验窗口结束 01.801 之后、显示步进开始 01.958 之前 = 面板打开的产品提案查询，设计内借读），武装窗口内零产品查询产物行 ✓；但 `DISPLAY advance` 行未发射——该行只在"显示模式结束后下一次 `advance()` 落穿"时输出，而本会话面板关闭后预览不再渲染、无后续 advance 调用（仪器缺口，非隔离失败信号） |
| 1（主） | **未决**：`OAR CONTEXT window=experiment actor=00000014 request=1 evaluations=2037 overlaps=0`——字面判据 `overlaps=0` 满足（零源 actor REFR 命中），但 `evaluations=2037` 远非预期的 0。前 8 行（8 行上限截断）全部 `refr=00000000`（空 REFR），位于 `phase=activated` 之后、`WeapEquip` 注入之前；分布排除逐步求值（显示窗口 120 步稳态 MT idle **evaluations=0**），指向**激活/转移驱动的突发**（初始激活 + 回放 4 次转移的剪辑激活 ≈ 每次 ~400 求值；命中模式 `evaluate-replacement → evaluate-set → evaluate ×2` 为 OAR 单个 replacement 候选树的完整求值流程） |
| 4（记录） | `phase=load-bindings source-set=0x2347380d060 source-count=15203 candidate-set=0x2347380d060 candidate-count=15203`——**候选与源共享同一绑定集对象**（`compare-loaded setup=different bindings=same`：候选自有 setup 引用同一绑定集），装载 9.8 ms 一次接受；G5 同剪辑对齐前置满足，但"共享绑定集"本身列入 A2 观察面 |

**裁定：G4-A 未决（NOT adjudicated）。** 预注册失败签名（`overlaps>0` / 候选值读取）未发生，预注册通过签名（`evaluations=0`）也未发生；字面 `overlaps=0` 的满足不能追认为通过——其解释性陈述"OAR 求值不触碰候选"被 2037 次空 REFR 求值削弱（反模式：不得把判据放宽为成功），也不构成判负（无任何源泄漏签名）。**B/C 暂停**（它们验收的"OAR 与候选互不污染"前提正是当前的未决项）。`load-bindings` 的共享绑定集是新的 G4 相关事实，一并交 A2。

**C-G4-A2 切片（预注册，草案 → 实现/部署，2026-10-31）**：回答三个问题——①2037 求值的触发入口（激活突发假设 vs 其它：把命中行上限改为按事件分段计数，在每次 dispatch/激活前后打窗口快照行）；②**求值读了哪张图**（关键仪器升级：`evaluate_thunk`/`evaluate_set_thunk`/`evaluate_replacement_thunk` 均有 `hkbClipGenerator* clip` 参数，从 clip 反查所属 graph/character 指针，与候选图 `0x239c0779…`/源图做**图级交集**——首轮只有 REFR 级身份，无法区分"候选图内容自评估"与"外部钩子评估候选"）；③求值是否写 OAR 状态（候选未注册 per-character 上下文，null REFR 下状态写落在何处；由 A2 + 后续 B 段世界动作间接回答）。A2 判负（图级交集命中且读候选值）→ G4 隔离前提重建；A2 判"候选图内容自评估且无源读取"→ 判据 1 按修订解释关闭，B/C 解禁。

**A2 实现记录（2026-10-31 收尾，实现 `57fa4af`，v61 部署身份 `…-v61-g4a2-clip-identity-20261031-57fa4affab-cl94faaed0c6-20261031T152526Z`，DLL `d1583a7e…` 1,460,736 字节，PDB `f41d5e74…`；v60 三件套备份至 `dist/deploy-backup-20261031-v61/`，部署 INI `60ff8f27…` 与运行 1 配置相同——A2 仪器全部由既有 `OarContextWatch` 键门控，无新键）**：

- **图级归属分类（问题②的决定性通道）**：命中时把 clip 指针对照候选/源各自的 `hkbBehaviorGraph::activeNodes`（审计依据：NodeList 0x10 字节头 `{entries@+0, count@+8}`，条目 0x90 步长、节点指针 @entry+0x58——58377 事件遍历即此布局；CommonLib `NodeList` 不透明，按审计偏移读）。命中行带 `owner=cand|src|neither`，汇总行带 `owner=cand:N/src:N/neither:N`。**已知局限**：初始激活突发发生在 `char_activate` 期间，此时候选 NodeList 尚未诞生——该段命中只能对照源表分类（`post-activate` 快照前的 owner 归属按此解读）；候选列表在激活返回后立即武装。
- **clip 身份**：命中行新增 `clip={:#x} clip-name='…'`（`hkbClipGenerator::animationName` @+0x48，hkStringPtr 低位掩码后经 `readable` 守卫读 64 字符上限）——clip 名字与分段快照交叉即可判读突发对应哪些剪辑。
- **分站点计数（问题②的判别前提）**：汇总/快照行带 `sites=e:n/s:r` 四站计数——**numeric 站计数 > 0 即有图变量读取发生**（配合命中行的 `name='…'` 变量名）；全程 numeric=0 则"读了谁的变量"问题自然消解。

> **2026-11-02 更正**：numeric 站命中仅表示访问器被调用，type3 才表示这里的图变量访问；site 总数没有记录每次类型。见[全窗口待办](#g4-followup-numeric)。

- **分段快照（问题①）**：`OAR CONTEXT segment=<label>` 累计行 ×4——`post-activate`（激活后）、`post-weapequip`（激活注入后）、`at-settle`（卸下 settle 处）、`post-unequip`（卸下注入后）；相邻行差值定位突发所属事件。命中行上限 8→32。
- **问题③**：仍无直接状态写观测（不变）；由 A2 分类结果 + 后续 B 段世界动作间接回答。
- **验证**：display_step 引擎无关用例 g++ C++23 `-Wall -Wextra -Werror` 通过。**上机步骤与运行 1 完全相同**（同 INI 配置、同操作序列、同判据 3 复用检查）。

**收尾状态：A2 已实现并部署（v61），部署 INI 已切 A2 运行配置（`60ff8f27…`，逐键断言过——含修正一处 dormant 默认值残留 `CandidateInputVariable=IsEquipping` → 空，反模式 11 再执行），待上机。**

**A2 首轮运行（2026-10-31 23:32，v61 身份 `57fa4affab` 活日志确认，actor 00000014 request=1；[日志](diagnostics/CharacterPanel-g4a2-clip-identity-20261031-2330.log) 135,464 字节 SHA-256 `bc93eb75…`）：问题①已决（逐帧、剪辑绑定）；问题②源侧排除（owner=src:0、overlaps=0）但 owner 全 `neither` 与 numeric 变量名未解码，判读未闭——A3 微切片。**

| 项 | 观测 |
| --- | --- |
| 复用守门 | **再次全过**：恰好两次 `result=dispatched`；`drive-done candidate-pose = bc160a8c/63f638a6/c407cb4e`、`unequip-done = 8fb4cd33/4f2aebde/c407cb4e`（与 S6/F/v60 三轮逐位一致）；`source-drift=none`（源 before=after=5025a78c）；零 `[E]`；`OAR COND` 恰 1 行（武装窗口外的产品提案查询） |
| ①触发入口 | **已决**：`post-activate=0`（激活突发假设被否）→ `post-weapequip=23` → `at-settle=1603`（装备窗口 120 步内 +1580，**~13/步**）→ `post-unequip=1628`（卸下注入 +25）→ 窗口总 2037（卸下窗口 120 步 +409，**~3.4/步**）→ 显示窗口（120 步稳态 idle）**0**。求值 = **OAR 逐帧机制，与"正在播放的非 idle 剪辑"绑定**（装备/卸下剪辑活跃期逐帧评估，MT idle 期为零），非一次性突发 |
| ②clip 身份 | 命中行 clip 名字解码成功且**逐帧恒定**：装备窗口全部 `Animations\H2H_Equip.hkx`（单对象 0x25073c00dc0），后续 `Animations\H2H_Idle.hkx`（0x25073c42c80）——本轮玩家徒手（H2H 族），评估对象 = 候选正在播放的剪辑族 |
| ②图归属 | **owner=neither ×2037，src=0、cand=0**。两个候选解释：(H1) 评估收到的是**模板节点**——OAR 装载期记录捕获共享模板树上的 clip generator（root-generator 源/候选同址、activeNodes 持有克隆，模板不入 activeNodes → neither 自洽，且候选 63585 装载过 OAR 装载钩子、REFR=null 与无 holder 自洽）；(H2) 扫描的 entries 基址 @+0 不对。count@+8 读法与既有 DriveState 同源（日志 `/97` 实证），头部读取可信，**H1 为优**，但两说未决 |
| ②变量读取 | `sites n=72`（numeric 命中存在，逐帧伴生）但**变量名全部为空**——名字读取以 `Numeric_Type_Offset==3`（图变量类型）为前置，72 次全空说明这些 numeric 大概率**非图变量型**（或其他值源），也可能名字通道有缺陷——type 值未记录，无法区分。若最终判定非图变量，则"读了谁的变量"问题自然消解 |
| ③状态写 | 维持间接（不变） |

**A2 裁定：失败签名确定排除**（2037 次求值零源归属、零 overlaps、源漂移逐位为零、复放确定性保持）；**通过条件仍未满足**（owner=neither 未解释、numeric 读取性质未知）——**G4-A 仍未决，但危险半边（源侧接触）已以高置信度关闭**。B 段验收的世界侧问题（Random/历史消耗）与 owner 解释相互独立，但按契约纪律维持暂停至判读闭合。

**C-G4-A3 微切片（预注册，3 项仪器增量，同 INI 配置同操作）**：①numeric 命中行记录 `type=N` 原始类型值（区分图变量 vs 其他值源）；②`post-activate` 快照行附带 `scan=cand-entries/cand-count/src-entries/src-count`（与 DriveState 的 `/97` 锚定，验证扫描基址）；③命中行附带当帧 `cand-count`（活计数锚）。判读规则：numeric 全部 type≠3 且扫描锚健全（count≥90 与 DriveState 一致）→ H1 成立（模板级评估、无图变量读取）→ **判据 1 按修订解释关闭，B/C 解禁**；type=3 出现 → 图变量读取存在，按 name/owner 继续裁；扫描锚不健全 → 扫描修正后重跑。

**A3 首轮运行（2026-10-31 23:46，v62 身份 `2dbe98c785` 活日志确认，actor 00000014 request=1；[日志](diagnostics/CharacterPanel-g4a3-scan-anchor-20261031-2345.log) 136,736 字节 SHA-256 `231032cb…`）：双证齐，H1 成立——G4-A 判据 1 按修订解释关闭。**

> **2026-11-02 前向更正**：下表“25 行/72 次全 type0/零图变量读取”属旧判读；实际仅前 32 条总求值明细内的 **7 条 numeric** 显示 type0，未提供全窗 72 次分型。扫描锚/owner/site 总数保留，当前 A 的全窗图变量读取覆盖 **PENDING**，不能由 neither 或源哈希不变替代；见[待办 1](#g4-followup-numeric)。

| 项 | 观测 |
| --- | --- |
| 扫描锚 | `OAR CONTEXT scan cand-list=0x1c8f35ee9c0 cand-entries=0x1c8c6019540 cand-count=97 src-list=0x1c8f41e89f0 src-entries=0x1c8cb9ec120 src-count=106`——双侧 entries/count 全为活值，cand-count=97 与 DriveState `active-nodes=…/97` 同源互证；**扫描在真实数据上行走，布局正确**，neither 分类可信 |
| numeric 类型 | 命中行 `type=0` 全部（25 行采样零 type=3）——**零图变量读取**（type 3 = 图变量访问器；type 0 为其他值源，语义未标注但不属图变量），A2 的名字全空由此解释 |
| owner | `neither ×2043`（`window=experiment evaluations=2043 overlaps=0 sites=e:1382/n:72/s:341/r:248 owner=cand:0/src:0/neither:2043`，与 A2 的 2037 同构）——扫描已证健全，neither = 评估对象**确在两图激活集之外 = 模板节点**（OAR 装载期记录捕获共享模板树上的 clip generator） |
| 复用守门 | **第三次全过**：恰好两次 `result=dispatched`；装备段 `bc160a8c…`、卸下段 `8fb4cd33…` 逐位一致；`source-drift=none`；零 `[E]`；显示窗口（稳态 idle）`evaluations=0` 复现 |

**G4-A 收官裁定：PASSED（限定范围：OAR 求值语境隔离，判据 1 按修订解释关闭）。** 三轮运行（v60/v61/v62）合并证据链：①候选活动窗口内的 OAR 求值 = **OAR 装载期记录的逐帧机制在评估共享模板树上 clip 的替换条件**（激活期零、非 idle 剪辑逐帧 ~13/3.4、稳态 idle 零），全部以 REFR=null（候选无 holder，模板无 REFR 可解析）进行；②**零图变量读取**（72 次 numeric 全为非图变量型 type=0）；③**零源侧接触**（owner=src:0、overlaps=0、源哈希三轮逐位不变）；④产品查询与候选显示的结构互斥坐实（武装窗口内零产品查询产物行）。"OAR 求值不触碰候选图/源图变量"按修订解释成立：触碰的是**共享模板级替换数据**（装载期由 OAR 写入图内容，两图本就共享），不是任一图的运行时变量/激活状态。残余已知项：numeric type=0 的具体语义、模板级评估是否写 OAR 内部状态（③仍间接）——均不构成源/候选运行时污染信号，随 B 段世界动作验收自然覆盖。

**B/C 解禁。** 下一步 = B 段（A/A 基线 ×2 → 运行 A，配置 `IndependentGraph=off`；运行 B，配置 `inject`+显示+回放），判据不变（世界动作选中序列逐项一致）。C 段可与 B 的运行 B 同会话。

**B 段首轮：A/A 基线（2026-11-01 00:00，v62 留装、`IndependentGraph=off`，单会话四开面板；[日志](diagnostics/CharacterPanel-g4b-aa-baseline-20261101-2355.log) 31,571 字节 SHA-256 `1f16d37a…`）：基线未过——按预注册规则换序列重新预注册，未做 A/B。**

| 项 | 观测 |
| --- | --- |
| 协议执行 | 四次面板打开（23:58:13 → 00:00:11，同一会话），全部 `drawn=false` 且左手为火把（00043324）——**核心动作（拔刀-收刀）在任何快照中均未出现**（执行缺口或游戏在开菜单时的收刀语义，待确认） |
| 环境闲置选择 | 行 1–2：`1HM_Idle → '1hm Non-Combat idle Loop'`(9042) 一致；**行 3：同 hands 同 drawn 却翻为 `'1hm Non-Combat idle Loop2'`(9043)**——Smooth Moveset 的双 idle 交替是负载序正常世界行为（循环边界重查交替选择），快照相位不同即落不同答案 |
| 初始态保持 | 行 4 左手变为 3d41d74f 且选择翻至 `'1hm Non-Combat idle Start'`(9044)——火把耗尽或手动更换，初态未持住 |
| 录制通道 | `ANIMATION WORLD` 行本身工作正常：完整 OAR 替换详情（oar-submod/oar-mod/index/t）可用于逐项比对——通道无缺陷，是序列本身不可重复 |

**裁定：A/A 不一致 = 序列含不可控方差（环境 idle 交替 + 初态漂移 + 拔刀态不可见），对照设计无效。** 按预注册规则未执行 A/B，不改窄判据追认。B 段须换用可重复序列后重新预注册：动作改由**条件驱动的装备变更**承担（不同武器类型的装备/卸下，其 OAR 选择由武器类型条件决定，无交替项），比对判据限定动作相关选择项，环境 idle 的 Loop/Loop2 交替明示排除在判据外；火把等初态在两运行间保持恒定。待确认拔刀态语义后预注册 v2。

**B-v2 预注册（2026-11-01，用户确认两事后立）**：①拔刀态语义确认——用户实际执行了拔刀-收刀，而四快照全为 `drawn=false` + 非战斗 idle → **游戏在开背包时已收刀，拔刀态活不到面板快照**（拔刀-收刀作为录制动作从协议中移除）；②序列重设计 = 热键装备不同动画族。

**协议 v2**：
- **动作集**：收藏热键装备 **3 个不同动画族**的物品（按实际库存选：1H 剑＝1HM 族 / 弓＝Bow 族 / 法术或徒手＝Magic/MT 族——族间 OAR 选择由装备类型条件决定，无交替项）。**左手卸下火把保持恒空**（A/A 行 4 的初态漂移根因）。
- **一轮** = [热键装备剑 → 等 settle ≥3 秒 → 开面板记录 → 关] + [热键装备弓 → …] + [热键装备徒手/法术 → …]，共 3 行 `ANIMATION WORLD`。
- **A/A 基线**（配置 `IndependentGraph=off`）：同会话 3 轮 + F9 读档后再 1 轮 = 12 行；**判据**：同槽位（第 N 次记录）的 `(original, oar-mod, oar-submod)` 三元组逐项一致。**明示排除项**：同族内 Loop/Loop2 类交替后缀（A/A 已证其为本底方差，与预览无关）——判据比到族/子模块粒度。
- **运行 B**（配置 `inject`+显示+回放四键+OarContextWatch）：读档回同存档 → 面板跑回放 ≥20 秒 → 关面板 → 同一轮 3 记录；判据同上，逐槽位与 A/A 一致。
- **C 段**并入运行 B（关面板后换装一次看 EquipSink/SCOPY）。
- 全过 → B 段过；C 段过 → **G4 PASSED**，进 G5。

**A/A 基线 v2（2026-11-01 00:xx，v62 留装、`IndependentGraph=off`，单会话 4 轮 11 行——第 1 轮为旧顺序（剑槽带火把作废，弓/法术保留），第 2/3 轮新顺序弓→剑→法术，第 4 轮 F9 后新顺序；[日志](diagnostics/CharacterPanel-g4b-aa-v2-20261101-2358.log) 90,026 字节 SHA-256 `ff6c3b03…`）：基线在预注册粒度通过。**

| 槽位 | hands 锚（逐槽核对） | 选择（族/子模块粒度） | 判定 |
| --- | --- | --- | --- |
| 弓 ×4 | (38052d81,38052d81) 全一致 | 族 `bow Non-Combat idle` 恒定；尾缀 Start(10570)↔Loop(10569) 跨轮交替 | ✓（尾缀属预注册排除的本底方差——本会话**无任何预览**仍交替，证明其独立于预览） |
| 剑 ×3（v2 轮） | (560c2a4c,00000000) 全一致——弓先行的左手清空锚生效 | 族 `1hm Non-Combat idle` 恒定；尾缀 Loop(8986)↔Loop2(8989) 交替 | ✓（同上；第 1 轮带火把的剑槽已按预注册作废） |
| 法术 ×4 | (3d41d74f,3d41d74f) 全一致 | `16031899`(11601) **四轮逐项全同**（含 index） | ✓ 完全稳定，无交替 |
| 附带 | F9 后首开面板记录到读档还原态 (sword,spell-left)——F9 恢复存档装备而非上轮末态；弓首槽的自锚定设计吸收之 | `SCAR_1hmReadyDummy → DumDum` 全行恒定 | ✓ |

**裁定：A/A PASSED（预注册粒度：`(original, oar-mod, oar-submod 族)` 逐槽一致；交替尾缀为本底方差且已证与预览无关）。** 对照设计有效，运行 B 可执行——判据保持族粒度，尾缀排除照旧。运行 B 的读档初态由弓首槽自锚定吸收（F9 还原什么装备都无所谓，弓先行清左手）。

**运行 B + C 段（2026-11-01 00:28–00:32，v62 留装，配置 ② `inject`+显示+回放四键+OarContextWatch，单会话 5 次开面板；[日志](diagnostics/CharacterPanel-g4bc-run-b-20261101-0028.log) 317,842 字节 SHA-256 `f015a1af…`）：B/C 全过——G4 PASSED。**

> **2026-11-02 前向更正**：保留当时 PASSED；B 只证明预注册的装备族选择稳定。C 的 capture/sink/DAC 观测没有源事件通道（配置 false、install-skip off），现有门又拒绝 inject 共存；源事件泄漏验收仍 **PENDING**，先评审兼容观察器，见[待办 2](#g4-followup-source-events)。

| 项 | 观测 |
| --- | --- |
| 回放复现（第四次） | 恰好两次 `result=dispatched`；`drive-done candidate-pose = bc160a8c/…`、`unequip-done = 8fb4cd33/…` 逐位一致；`source-drift=none`；零 `[E]` |
| 隔离结论复现（回放+显示共存态） | `window=experiment evaluations=2037 overlaps=0 owner=cand:0/src:0/neither:2037`——与 A 段三轮**同构同分布**（e:1379/n:72/s:339/r:247 逐项相同）；全部显示窗口（retarget/reset ×6+）`evaluations=0` |
| B：世界选择对照 | 轮序列三槽与 A/A 基线逐族吻合——弓 `(bow,bow)` → `bow Non-Combat idle`（尾缀 Loop，本底方差内）、剑 `(sword,空)` → `1hm Non-Combat idle`（尾缀 Loop）、法术 `(spell,spell)` → `16031899`；预览（回放+显示 ≥20 秒）运行于轮前，**条件驱动的世界选择零漂移** |
| C：关闭清理与 sink | 5 次开面板 5 次 `SCOPY CAPTURE … accepted`；`SCOPY PARK retained-total` 2→3→4 每捕获 +1（刻意保留规则内，远低于 12 阈值，无未配对句柄行）；**EquipSink 存活**——装备修订计数跨面板 10→14 持续递增（热键装备事件照常到达）；DAC restore 每次关面板 `matched`（epoch 6/8/10）；候选步进全程 `source-drift=none` |
| 附带 | 前两行预轮记录（sword+空手）`MT_Idle → 'Idle 1'/'Idle 2'`——MT 族亦见交替尾缀（本底方差类，判据外） |

**G4 收官裁定：PASSED（A/B/C 三段各自限定范围）。** A = OAR 求值语境隔离（模板级评估、零图变量读取、零源接触）；B = 世界历史无消耗（条件驱动选择零漂移）；C = 关闭清理与 sink 作用域成立。按排序进入 **G5（完整姿态对照）**——G3-B 的 pose 哈希基线与 B-v2 的装备族序列是 G5 的直接前置；当前下一步按 [G5 v2](#g5-draft)先审计参照/可比性与采样，完整协议冻结后置。

<a id="c-g4-a4-numeric-types"></a>
## C-G4-A4：numeric 类型全窗分型（草案 → 实现，2026-11-02）

> **状态：已实现、已构建、已部署并已运行（v63）—— PENDING 1 已关闭。** 预注册文字未改写；运行结果回填在本节末尾。构建与部署身份见 [v63 build](build.md#v63-g4a4-numeric-types-2026-11-02)。

**背景与目标**：[待办 1](#g4-followup-numeric) 指出 A3 的 `sites n=72` 只发射了前 32 条总求值明细、其中 7 条 numeric 全为 `type=0`，其余 65 次类型未知，因此"全窗零图变量读取"不被日志支持。本轮只补这一处盲点：按原始访问器类型给 numeric 命中分型计数，**独立于明细行预算**，并与 `sites[n]` 对账。

**预注册判据（不得事后放宽）**：

1. **对账**：每个 `window=experiment|display-<reason>` 与 `segment=*` 行的 `numeric=…/sum:N` 必须等于同一行的 `sites=n:N`；任一窗口不等即仪器无效，判负并按停点上交，不得引用该窗的类型结论。
2. **窗口性**：`arm`/`take` 后全部分型计数归零，上一窗的类型不得进入下一窗；非属主 session 的 take 既不消耗也不清理候选窗（A2 契约沿用）。
3. **明细预算无关**：类型计数在 `Oar_Context_Hit_Row_Cap = 32` 之前累加——第 33 条以后的 numeric 仍须进全窗总量。
4. **可归属**：`type3`（图变量访问器）若落在 32 条明细之后，仍须有界（每窗 ≤8 行）留下 `name`/REFR/`clip`/`owner` 归属行，且不得与前 32 条明细重复记录。
5. **结论边界**：全窗 `t3=0` 只限定该窗口内**已计数**的 numeric 命中；`un`（空或不可读访问器）单独成桶，不并入 `t0`，也不得当作"非图变量"证据。

**判读规则**：`sum==sites[n]` 且全窗 `t3=0` → 修订解释下的"A 零图变量读取"覆盖该完整窗口；出现 `t3>0` → 按 `name`/REFR/图作用域追溯来源，**不自动判污染**，无法归属则保持 PENDING。`overlaps`、`source-drift`、崩溃与非有限值等停点条件与 A 段相同。

**仪器影响面（无新 INI 键、无新源单元、无新原生调用/写入）**：

- [candidate_window.h](../src/animation/behavior_graph/candidate_window.h)：`CandidateWindowTally` 增 `numeric_types[4]`（type 0–3）、`numeric_type_other`（≥4）、`numeric_unclassified`（负值/空访问器）、`numeric_type3_rows`；新增 `note_candidate_numeric_type`、`candidate_window_numeric_sum`、`take_numeric_type3_row`；新增 `clear_candidate_window_counts` 统一 arm / take / 非属主 take 三条路径——原先 `take_candidate_window_tally_for` 用位置式聚合初始化返回零行，任何新增计数器都会在那里漏清，现改为复制元数据后统一清零。
- [hooks.cpp](../src/animation/oar/hooks.cpp) `note_oar_context_hit`：类型计数放在 `OarContextWatch` 门与行预算**之前**；过 32 行后的 type3 走专用归属行 `OAR CONTEXT numeric-type3`（每窗 ≤8 行）。
- [independent_graph.cpp](../src/animation/behavior_graph/independent_graph.cpp)：`window=display-*`、`segment=*`、`window=experiment` 三类行统一追加 `numeric=t0:{}/t1:{}/t2:{}/t3:{}/other:{}/un:{}/sum:{} t3-rows:{}`。
- 沿用 `[Experiment] OarContextWatch`（默认 false）；关断时只剩 thunk 入口一次布尔检查与 thread-local 自增，无行发射。

**离线回归（已跑）**：[behavior_graph_candidate_window_test.cpp](../tests/behavior_graph_candidate_window_test.cpp) 六例——32 条明细之后仍计数并对账、`take` 清零且保留元数据、armed scope 外的命中被忽略、新窗无残留、非属主 take 不消耗不清理、type3 行按窗限额且重开后复位。`cl /nologo /std:c++latest /EHsc /W4 /WX /utf-8 /I src` 编译零警告、运行 "6 candidate-window numeric-type cases passed" 退出 0。**注意：计数需要 `ScopedCandidateWindow` 深度守卫存活**（`arm` 只重置计数），与生产 `drive_graph_step`/两处派发的守卫一致。

**上机门（用户执行）**：配置与 A3 运行相同——`inject` + S1 观测两键（`CandidateInputProbe=true`、`CandidateInputVariable=` 空）+ S6 卸下两键（`CandidateUnequipProbe=true`、`CandidateUnequipEvent=Unequip`）+ `DisplayPose=true` + `DisplayStep=true` + `OarContextWatch=true` + `DetailedAnimationTrace=true`；操作序列与 A3 相同（回放 + 显示 ≥20 秒）。**只做一处聚焦回归**：确认明细被截后 numeric 仍进总量（对账），并在下一窗口验证清零；随后匹配 DLL/PDB/INI 身份回传完整日志与分型对账。**不要求 `t3` 为 0**，只要求总量可对账与来源可归属。

**停点与回传**：总数不对账、跨窗残留、计数或身份缺失、出现未归属 type3，即停止验收并保留原始记录回评审；`source-drift` 非 none、崩溃或非有限值立即停。回传须含独立实现契约/差分、聚焦回归结果、匹配工件与设置身份、带 window/segment 边界的完整日志及分型对账/归属结论，不能只给前 32 行。

**实现与部署记录（2026-11-02，提交 `e97eb65e68`，v63 身份 `…-v63-g4a4-numeric-types-e97eb65e68-cl94faaed0c6-20261102T142727Z`，DLL `2c81435c…` 1,466,368 字节、PDB `c8e361ec…` 38,432,768 字节；完整构建记录见 [v63 build](build.md#v63-g4a4-numeric-types-2026-11-02)）**：

- **计数条件（重要边界）**：`arm_candidate_window` 只重置计数，**真正的计数闸门是 `ScopedCandidateWindow` 的 `depth>0`**——与 A2/A3 相同的语义，`drive_graph_step` 与两处预注册派发包裹的正是这个守卫。因此"窗口已武装但无守卫"的区间不会记任何 numeric 类型，这一点在离线回归第 3 例单独验证。
- **落点**：[candidate_window.h](../src/animation/behavior_graph/candidate_window.h) 的 `note_candidate_numeric_type` 在 `note_candidate_window_hit` 之后由 [hooks.cpp](../src/animation/oar/hooks.cpp) 的 `note_oar_context_hit` 同步调用，二者共同保证"一次 numeric 命中 = site+1 且类型+1"，所以 `sum==sites[n]` 是对账恒等式而非事后推算。
- **复位**：`clear_candidate_window_counts` 替代原先三处重复的清零（arm / take / 非属主 take），并把 `take_candidate_window_tally_for` 的位置式聚合初始化换为"复制元数据后统一清零"，新计数器不可能在其中一处漏清。
- **明细预算与 type3**：`Oar_Context_Hit_Row_Cap = 32` 现在只截总明细；过预算后仅 type3 命中可再发 ≤8 行 `OAR CONTEXT numeric-type3`（`name`/REFR/`clip`/`clip-name`/`owner`/`evaluations`/`cand-count`），前 32 行内的 type3 仍只在总明细行里出现一次，不重复。
- **汇总字段**：三类行统一追加 `numeric=t0:{}/t1:{}/t2:{}/t3:{}/other:{}/un:{}/sum:{} t3-rows:{}`；`other` 覆盖 ≥4 的类型值，`un` 覆盖空/不可读访问器（负值），二者都不并入 `t0`。
- **未改动**：无新 INI 键（仍由 `OarContextWatch` 门控）、无新源单元（`PLUGIN_SOURCES` 32）、无新原生调用或写入、无 settings 解析变化。
- **部署**：v62 dormant 三件套备份至 `dist/deploy-backup-20261102-v62/`（哈希与 v62 记录逐项一致），v63 pair 装入 `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\`，部署 INI 切 A4 运行配置（2,612 字节 `7cb2562f…`，20 键回读断言通过）。**上机由用户执行**：操作与 A3 完全相同的一次会话即可（回放 + 显示 ≥20 秒），判读只看 `sum` 与 `sites n:` 是否相等、以及 `t3` 总量与归属。

**首轮运行（2026-11-02 22:32:00–22:37:02，v63 身份 `e97eb65e68` 活日志确认，AE 1.6.1170，actor 00000014 request=1；[日志](diagnostics/CharacterPanel-g4a4-numeric-types-20261102-2232.log) 567,481 字节 SHA-256 `59fa75ca…`）：31/31 行对账相等、全窗 `t3=0` —— PENDING 1 关闭。**

| 判据 | 观测 |
| --- | --- |
| 1 对账 | 实验窗 `evaluations=2037 overlaps=0 sites=e:1379/n:72/s:339/r:247 owner=cand:0/src:0/neither:2037` 与 `numeric=t0:72/t1:0/t2:0/t3:0/other:0/un:0/sum:72 t3-rows:0` → **72 == 72**；四个分段行（0/7/60/67）与 27 个窗体行（含 2 行 `actor=00000000` 的武装前窗体）共 31 行，`sum` 与 `sites n:` **无一不等** |
| 2 窗口性 | 实验窗取走 72 后，其后 21 个显示窗（跨 request 1..12，含紧跟的 `display-retarget request=1 view=0`）全部 `evaluations=0 sum:0` → 无跨窗残留；`t3-rows` 每窗回到 0 |
| 3 预算无关 | 明细仍是精确 32 行（`evaluations` 1..32，其中 numeric 7 行 `type=0`），而全窗 numeric 计数 72 → **65 次 numeric 命中在明细预算之外仍被计入**；离线第 1 例把该规则钉在 33 条之后 |
| 4 type3 归属 | 全窗 `t3=0`、`un=0`，无需归属行（`t3-rows:0`）；离线第 6 例证明过预算的 type3 会留下 ≤8 行再停 |
| 5 边界 | `t3=0` 只限定本完整窗（2037 求值、两次派发、120 步回放、`source-drift=none`），不外推到未观察窗口或其它会话 |

**复用守门（与 A2/A3 同构）**：恰好两次 `result=dispatched`（`WeapEquip` id=2、`Unequip` id=144）；`drive-done` 姿态 `bc160a8c…/63f638a6…/c407cb4e…`、`unequip-done` `8fb4cd33…/4f2aebde…/c407cb4e…` —— **第五次逐位一致**；`source-drift=none`，源姿态 before=after=`98e34934e40a79ac`；`load-bindings` 两图仍同址 15,203 项；`OAR CONTEXT scan cand-count=97 src-count=106`；零 `[E]`，15 条 `[W]` 全是既有 DAC/台账提示，无仪器告警。

**过程偏差（不影响判据，据实留档）**：显示相位由 21 个被 `retarget`/`request` 打断的窗口组成、累计 364 步（约 6 秒可计显示时间），没有复现 A3 那种单窗 120 步的连续显示；`DISPLAY advance … display-frames=…` 行仍未发射（运行 1 已记录的仪器缺口）。A4 判据不依赖显示时长，故不改判，但**不得把本次描述为"20 秒连续显示"**。

**裁定：PENDING 1 关闭 —— G4-A 的 numeric 全窗覆盖成立。** 完整 2037 求值窗口内 72 次 numeric 命中**全部 type 0、零 type 3、零 unclassified**，且类型总和与 numeric 站点数逐窗相等；"零图变量读取"由此从 A3 的"7/72 条明细"升级为**整窗 72/72 分型**。G4 只剩 [C 段源事件泄漏](#g4-followup-source-events)一项 PENDING，G5 的运行验收仍后置于其关闭；`t3=0` 不构成"模板级评估不写 OAR 内部状态"的证据，残余项维持原状。裁定后部署 INI 回 dormant（2,147 字节 `27f481f5…`），v63 DLL/PDB 留装。

<a id="c-g4-c1-concurrent-events"></a>
## C-G4-C1：兼容的源事件观察窗口（评审 → 实现，2026-11-02）

> **状态：评审定稿、已实现、已运行（v66 关闭 → [audit30](behavior-graph-construction-audit.md#audit30) 撤回 → v69 修复版重跑再次关闭，2026-11-05）。** 本节是 [PENDING 2](#g4-followup-source-events) 要求的"另立评审范围"；预注册判据在实现前固定，运行结果回填在末尾。

**问题陈述（三个结构事实，缺一不可）**：

1. B+C 运行里 `SourceEventCapture=false`、`install-skip reason=off` —— 那次没有源事件通道。
2. 现有安装门 `install_source_event_capture` 在 `IndependentGraph != off` 时直接拒绝；而 F7 武装只走**面板外**路径（[input.cpp](../src/input/input.cpp)）。候选活动只发生在面板内 —— 所以"打开一个 INI 键重跑"在结构上不可能覆盖候选活动，**删守卫也不是设计**。
3. E1 的残余：观察器只对源图指针过滤，无法区分"引擎不向这张图派发事件"与"引擎根本不经 58377 派发"（[E1 残余限定](#c-g3b-e1-event-capture)）。

**设计（关键事实决定正控来源）**：

- **正控不依赖引擎碰巧派发**：插件自身就是直调 `REL::ID(58377)` 派发 `WeapEquip`/`Unequip`（[independent_graph.cpp](../src/animation/behavior_graph/independent_graph.cpp) 的 `event_dispatch`）。每次回放必然在窗口内产生 **≥2 次 58377 调用**；detour 装在函数入口，因此这两次调用就是我们自己的存活正控。
- **三态模式**：`[Experiment] SourceEventCapture` 从 bool 改为 `off|on|concurrent`（`on`/`true` 保留旧语义）。
  - `on`：E1 原样 —— F7 窗口、要求 `IndependentGraph=off`、`SourceTimeline` 优先。
  - `concurrent`：允许 `IndependentGraph=inject`；**仍要求 `SourceTimeline=false`**（两路共用 F7 键与身份规则，不同时开）；窗口由**实验窗口自动武装**（`run_independent_graph_experiment` 入口，同步建立基线与 gate，早于任何注入）、在**显示窗第一次带步数的关闭**（`DisplaySession::State::abort`）或 20 秒到期或世界重置时关闭。
  - 不删任何守卫：`on` 的拒绝分支一字不改，`concurrent` 是新增的、有自己生命周期的模式；`source_timeline()` 冲突仍是 warn+skip。
- **计数器（门之前）**：detour 在 gate 检查**之前**自增 `calls`；gate 已武装但目标是别的图时自增 `other-graph`；只有源图命中才入环并落成事件行。这样一次关闭行同时给出三个量：`calls ≥ 2`（观察器存活）、`other-graph ≥ 2`（分类有效，我们的两次注入被识别为非源图）、`source-events = 0`（隔离结论）。

**预注册判据（不得事后放宽）**：

1. **安装**：`SOURCE EVENTCAPTURE installed … mode=concurrent`，运行时头部字节核验通过；出现 head-mismatch 即停。
2. **武装**：`window-open` 行给出 actor/graph/behavior/data/variable-set/words/count 身份与两条预算，且发生在两次 `result=dispatched` **之前**（同一帧内成立，见设计）。
3. **正控（硬前提）**：关闭行 `calls ≥ 2` **且** `other-graph ≥ 2`。任一不足即"观察器未被证明存活"，本次零记录**无效**，PENDING 保持，不得改判为通过。
4. **隔离**：`source-events=0`、`ring-dropped=0`、无 `budget-exhausted`，且窗口以 `reason=display-close` 关闭（即显示段确有步数）。**若以 `reason=expired` 关闭**（面板在 20 秒内未关），则显示/关闭边界未覆盖：只接受判据 3 与注入段读数，隔离结论限定为"注入期零源事件"，**不得声称覆盖显示与关闭边界**，也不据此关闭 PENDING。
5. **边界**：`source-events=0` 只限定 **58377 这一条路由**；不证明 manager 级其它事件路径不存在（A1/E1 已示覆盖不足），不外推到未观察窗口。
6. **身份**：任一身份变化（`source-lost-or-replaced`）即中止并按停点处理；窗口期间源姿态/变量由既有 `source-drift` 独立复核。

**停点与回传**：头字节不匹配、正控失败、身份变化、缺窗/丢行/预算耗尽 → 停止并保持 PENDING；**不得删守卫、放宽判据或改写旧日志**。回传须含：安装/武装/关闭三行、`calls/other-graph/source-events/ring-dropped` 四项计数、两次 `result=dispatched` 与 `window-open` 的先后关系、匹配的 DLL/PDB/INI 身份，以及窗口内源相位行（若有）。

**非目标**：不注入、不写变量、不触碰候选行为图语义（纯透传，detour 只读三参数）、不做姿态对照、不声称覆盖全部事件路由、不改 F7 既有语义。

**离线回归**：模式解析（`off|false|""`、`on|true`、`concurrent`，含大小写）进 [source_event_capture_test.cpp](../tests/source_event_capture_test.cpp)，`cl /W4 /WX` 通过；环/预算/取消/到期的既有三例不重跑也保持有效。

**实现影响面**：`settings` 三态 + 纯解析函数；`source_event_capture.{h,cpp}` 两个计数器、`begin_capture_window` 共享基线、`arm/close/open` 三个薄 API；`independent_graph.cpp` 的 arm/close 两处调用；文档、配置注释与 CHANGELOG 同步。无新源单元（`PLUGIN_SOURCES` 32）、无新原生调用或写入。

**实现与部署记录（2026-11-02，提交 `02c16e5b65`，v64 身份 `…-v64-g4c1-concurrent-events-02c16e5b65-cl94faaed0c6-20261102T145517Z`，DLL `ff00daad…` 1,470,464 字节、PDB `3364ea18…` 38,481,920 字节；完整记录见 [v64 build](build.md#v64-g4c1-concurrent-events-2026-11-02)）**：

- **模式**：`[Experiment] SourceEventCapture=off|on|concurrent`；`on`/`true`/`1` 保持旧语义，无法识别的字符串解析为 `nullopt` → 落回 off 并 warn（打字错误不能武装 hook），日志打印实际模式。`on` 的 `IndependentGraph != off` 拒绝分支**一字未改**；`concurrent` 的条件是"非 SourceTimeline"，与 legacy 共用同一条 timeline 优先警告。
- **生命周期**：`arm_source_event_capture_concurrent` 在实验入口同步完成 reset→deadline→identity→baseline→gate（`begin_capture_window`，与 F7 路径共用 baseline 代码），因此两次注入发生时 gate 已在位、计数器已从 0 起算；`close` 挂在 `DisplaySession::State::abort` 的"显示段已有步数"分支（零步 abort 不关窗，避免 request 绑定把窗关掉），另有 20 秒到期与世界重置取消。F7 在 concurrent 模式下不再武装。
- **计数器**：`record_source_event` 在 gate 检查**之前**自增 `calls`；gate 在位且目标非源图时自增 `other-graph`；只有源图命中才入环（保持"单一生产者=源图派发者"的 SPSC 约束）。关闭行 `SOURCE EVENTCAPTURE window-closed reason=… events=… phases=… calls=… other-graph=… ring-dropped=…` 一行为正控与隔离签名共用。
- **上机门（用户执行）**：开面板一次 → 回放自动跑完 → 保持面板 2–3 秒让显示段产生步数 → **15 秒内关面板**。判读：`installed mode=concurrent`；`window-open` 早于两条 `result=dispatched`；`window-closed reason=display-close` 且 `calls ≥ 2`、`other-graph ≥ 2`、`events=0`、`ring-dropped=0`。以 `expired` 关闭时只接受注入段读数（见判据 4 的分支），不得据此关闭 PENDING 2。

**首轮尝试（2026-11-02 23:21–23:23，v64；[日志](diagnostics/CharacterPanel-g4c1-attempt1-source-identity-20261102-2321.log) 137,183 字节 SHA-256 `f296c348…`）：无效 —— 窗口武装后第一个采样帧即中止，正控未取得。**

| 项 | 观测 |
| --- | --- |
| 安装 | `installed mode=concurrent target=0x7ff7259958a0 … head=48 8b c4 48 89 48 08`（头字节核验通过） |
| 武装 | `window-open … words=652 nodes=106 tracked …` → `concurrent-armed actor=00000014 behavior=0x221f3875900`，两行都在两条 `result=dispatched` **之前** |
| 回放 | 本体正常：两次 `result=dispatched`、`drive-done` `bc160a8c…/63f638a6…/c407cb4e…`（第六次复现）、`source-drift=none`、零 `[E]` |
| 中止 | `window-aborted reason=source-lost-or-replaced events=0 phases=0` —— 身份规则在第一次采样判定源被替换 |

**根因（代码级，非引擎异常）**：并发武装用实验侧的图（`source_graph`，来自控制器的 `source_graph()` 解析器——优先 `rootNode == actor.Get3D(false)` 的图、回退首个非 1stperson），而采样器每帧用 `player_graph()` 自己解析（要求 `behaviorGraph` 非空、跳过含 `1stperson` 的名字、取**最后一个**匹配）。两个解析器在管理器有多张非 1stperson 图时返回不同对象 → 图指针不同 → 身份不符 → 正确触发中止（规则本身没错，是武装侧用了另一套解析）。

**仪器缺陷（同轮暴露，已修）**：`window-aborted` 行当时不打印 `calls/other-graph`，`identity-mismatch` 也不存在——那一次连正控读数都没留下，只能靠重跑补救。

**修正（v65，判据不变）**：并发武装改为调用与采样器**同一个** `player_graph(*holder)`，并在 `concurrent-armed` 行记录 `graph-match=yes|no`（实验图与解析图是否同一对象）；`window-open` 行补 `graph/behavior/data/set/words-ptr`；新增 `identity-mismatch` 行逐字段对比 prev/now；`window-aborted` 补 `calls/other-graph`。

**第二轮尝试（2026-11-02 23:38–23:40，v65；[日志](diagnostics/CharacterPanel-g4c1-attempt2-wrong-graph-20261102-2338.log) 137,435 字节 SHA-256 `8a478311…`）：仪器机制全通，但观测对象是错的图 —— 不构成 C 判据。**

| 项 | 观测 |
| --- | --- |
| 安装 | `installed mode=concurrent … head=48 8b c4 48 89 48 08` |
| 武装 | 23:40:43.077 `window-open … words=463 nodes=36 graph=0x2039d0f1800 behavior=0x1fef2aee000 data=0x1fec5517960` + `concurrent-armed … graph-match=no` |
| 先后 | 武装在两次派发**之前**：23:40:43.302 `WeapEquip`、23:40:43.422 `Unequip`；`drive-done` `bc160a8c…/63f638a6…/c407cb4e…`（第七次复现）、`source-drift=none`、零 `[E]` |
| 正控 | `window-closed reason=display-close events=0 phases=0 calls=64 other-graph=57 ring-dropped=0` —— `calls≥2`、`other≥2` 成立 |
| **致命问题** | 被观测图是 `words=463/nodes=36/data=0x1fec5517960`，而候选的源图是 `words=652/nodes=106/data=0x1feb4eb7510`（`load-bindings` 的 15203 绑定集与 `pre-activate` 的 `behavior-data` 都指向后者）。两个解析器在同一 actor 上返回**不同图对象**：`player_graph()` 取最后一个非 1stperson；控制器 `source_graph()` 优先 `rootNode == actor.Get3D(false)`。窗口看的是前者，而泄漏判据要的是**候选所由来的那一张** |

**结论：本轮 `events=0` 不能用于 C 判据**（观测对象错，零值无意义），PENDING 2 保持。可用的收获是机制层：窗口先后、正控计数、`display-close` 关窗、零丢弃、无身份中止，全部按预注册工作。

**修正（v66）**：并发窗口改为**钉住实验侧那张图**——`concurrent-armed` 用 `managed_graph(*holder, &a_source_graph)` 只在管理器列表仍持有该对象时才pin，并每帧用同一 `managed_graph` 复核成员关系（只比较指针，不解引用已释放对象）；采样器在 `concurrent_graph` 存在时用钉住的图，legacy F7 路径保持 `player_graph()` 不变。`window-open` 的 `words/nodes/data` 因此应与 `scan src-count`、`load-bindings` 同源，可直接在日志里核对这张图对不对。

**第三轮尝试（2026-11-03 00:10:44–00:10:54，v66 钉图版；[日志](diagnostics/CharacterPanel-g4c1-attempt3-pinned-graph-20261103-0010.log) 137,368 字节 SHA-256 `d2fa2b03…`）：判据 1–6 全部成立 —— PENDING 2 关闭。**

| 判据 | 观测 |
| --- | --- |
| 1 安装 | `installed mode=concurrent target=0x7ff7259958a0 … head=48 8b c4 48 89 48 08` |
| 2 武装 | 00:10:44.638 `window-open actor=00000014 words=652 nodes=106 data=0x1c4b4cb7510` + `concurrent-armed … pinned=yes`，**早于** 44.890 `WeapEquip` 与 45.025 `Unequip` |
| 3 正控 | `calls=62 ≥ 2`、`other-graph=55 ≥ 2` —— 两次注入与其它图的派发都被 detour 看见并正确分类 |
| 4 隔离 | `events=0`、`ring-dropped=0`、无 `budget-exhausted`、`reason=display-close`；窗口跨 10.1 秒，覆盖两次注入、120 步回放与显示段关闭 |
| 5 边界 | 仅限 58377 路由，不外推其它 manager 级事件路径 |
| 6 身份 | 无 `identity-mismatch`、无 `window-aborted`；`nodes=106` 与同日志 `OAR CONTEXT scan src-count=106` 一致，`data=0x1c4b4cb7510` 与 `pre-activate` 的 `behavior-data` **逐位相同** → 观测对象确为候选所由来的那张图 |

**裁定：PENDING 2 关闭 —— 候选活动期间源图事件为零，且观察器存活、分类有效、窗口覆盖完整。** 复用守门同构：恰好两次 `result=dispatched`（`WeapEquip` 2 / `Unequip` 144）；`drive-done` `bc160a8c…/63f638a6…/c407cb4e…`、`unequip-done` `8fb4cd33…/4f2aebde…/c407cb4e…`（**第八次逐位复现**）；`source-drift=none`、源姿态 before=after=`a8f8dabc8439246e`；零 `[E]`，15 条 `[W]` 均为既有 DAC/台账提示。零计数只限定 58377 路由（E1/A1 已示其覆盖不到全部 manager 级事件路径），**不得外推**。裁定后部署 INI 回 dormant（2,147 字节 `27f481f5…`），v66 DLL/PDB 留装。

> **2026-11-04 前向更正**：上一段（v66 第三轮）的裁定因 [audit30](behavior-graph-construction-audit.md#audit30) 仪器缺陷撤回；下节 v69 修复版重跑重新取得同构证据。

**修复版重跑（v69，2026-11-05 00:01–00:01.4，两轮；attempt 1 因 INI 派生漏键只注入一次，仪器判读有效但不构成 C 形状；attempt 2 判据 1–6 全部成立 —— PENDING 2 在修复版上再次关闭）**：

| 判据 | attempt 2 读数（[日志](diagnostics/CharacterPanel-g4c1-v69-attempt2-two-dispatch-20261105-0001.log) 75,607 字节 `06a743c8…`） |
| --- | --- |
| 1 安装 | `installed mode=concurrent target=0x7ff75d4b58a0 … head=48 8b c4 48 89 48 08`（[audit30 修复版](build.md#v69-g5e1-clean-rebuild-2026-11-04)首次过 C1 窗口） |
| 2 武装 | 00:01:10.510 `window-open … words=652 nodes=106 data=0x15734cf7510`（与 `load-try` 的 `behavior-data` 逐位相同）+ `concurrent-armed pinned=yes`，早于 10.521 `WeapEquip` 派发 |
| 3 正控 | `calls=64 ≥ 2`、`other-graph=64 ≥ 2` |
| 4 隔离 | `events=0`、`ring-dropped=0`、无 budget-exhausted、`reason=display-close`（16.2 s 窗口：两注入 + 两段 drive + 显示段 120 步 + 关闭 + `DAC restore matched`） |
| 5 边界 | 仅 58377 路由，不外推 |
| 6 身份 | 无 `identity-mismatch`/`window-aborted`；`source-drift=none`、显示窗 before=after=`cf4474599d2ba8ef` |

回放形状完整：`WeapEquip`(2) → 装备段 settle（step 63，目标 49，`baseline-default=1`）→ `Unequip`(144) → 卸下段 12→0 于 step 21 自发、脉冲变量 step 33 落零；`drive-done` `bc160a8c…/63f638a6…/c407cb4e…` + `unequip-done` `8fb4cd33…/4f2aebde…/c407cb4e…` —— **第九次逐位复现，且为修复版仪器上的第一次**；显示段 `pose-summary semantic-changed=116/116`。attempt 1（[日志](diagnostics/CharacterPanel-g4c1-v69-attempt1-single-dispatch-20261104-2336.log) `0bfa8712…`）的 INI 派生漏掉卸下切片两键（`CandidateUnequipProbe`/`CandidateUnequipEvent`），只注入 `WeapEquip`、收于 readied 姿态——仪器四判据当时已成立（`calls=42/other-graph=42/events=0/display-close`），但不构成两注入形状，不用于关闭。视觉说明：回放在模拟时间 ~5 ms 内完成，面板显示的是卸下完成后的站姿——"面板里只有 idle"是两轮运行的预期外观，证据在日志行里。运行后 INI 恢复 dormant `27f481f5…`，v69 留装。

<a id="g4-closure"></a>
## G4 收官合并（2026-11-03）：三段证据、限定与仪器事实

> **前向更正（2026-11-04）**：本节为历史记录，当前解释与仪器有效性以 [E1 修复后边界](#g5-e1-evidence-repair)及 [audit30](behavior-graph-construction-audit.md#audit30) 为准；旧 `completion` 字段不表示输出完成，G4-C 零事件与旧 setter 零写入无效。

> **前向更正（2026-11-03 晚，[audit30](behavior-graph-construction-audit.md#audit30)）**：本节"三段合并"中的 **C 段证据判为仪器无效**——58377 detour 的 thunk 实参槽位错位，`events=0` 是门不成立的结果。A 段（OAR detour，非手写 thunk）与 B 段不受影响。本节的 C 段文字保留为当时读法，**不得再作为 C 段通过的引用依据**；G4-C 需修复后的仪器重跑。

G4 的验收对象是"候选在面板显示链里被驱动时，OAR 与游戏本体动画状态互不污染"。11-01 的 A/B/C 三段裁定当时证据覆盖不全；11-02（v63）与 11-03（v66）两次补测把 A、C 两项缺口补齐。本节把三段合并为一份可引用的结论，**不改写任何历史判读**。

### 三段证据

| 段 | 判据 | 证据（运行 / 行） | 限定 |
| --- | --- | --- | --- |
| **A** 求值语境隔离 | 候选活动窗口内 OAR 求值不触碰候选/源图变量；产品查询与显示互斥 | v60/v61/v62 三轮 + v63：[C-G4-A4](#c-g4-a4-numeric-types) 实验窗 2,037 求值、`overlaps=0`、`owner=cand:0/src:0/neither:2037`、`numeric=t0:72/t3:0/un:0/sum:72 == sites n:72`（65 次在 32 行明细预算之外计数），显示窗全 0；产品查询在武装窗口内零行 | 触碰的是**共享模板级替换数据**（OAR 装载期写入图内容，两图本就共享），不是任一图的运行时变量/激活状态；`t3>0` 分支未触发 |
| **B** 世界历史零消耗 | 预览前后的世界动作选择逐项一致 | A/A v2（4 轮 11 行、族粒度；交替尾缀在无预览下照常漂移＝本底方差）+ 运行 B（回放+显示 ≥20 s 后同序列三槽逐族吻合） | **只证明预注册的装备族选择稳定**；不普遍证明 Random/sequence/PlayOnce 历史，不覆盖未测序列 |
| **C** 关闭清理与 sink 作用域 | 关闭只清预览自有登记；源 OAR 状态与 sink 存活；候选事件不泄漏进源图路由 | 5/5 `SCOPY CAPTURE accepted`、`retained-total` 2→3→4、EquipSink 修订 10→14、DAC restore matched ×3；[C-G4-C1](#c-g4-c1-concurrent-events) v66：`calls=62/other-graph=55` 正控、源图 `events=0`、`ring-dropped=0`、`reason=display-close` | 事件结论只限 **58377** 路由；`retained-total` 只增不减属刻意保留例外 |

### 仪器与引擎事实（本轮确立，供 G5/G6 复用）

1. **同一 actor 上存在多张非 1stperson 行为图**：本 mod 列表里 `player_graph()`（采样器与旧 E1）解析到 `words=463 nodes=36 data=0x1fec5517960`，控制器 `source_graph()`（OAR/DAC/候选源）解析到 `words=652 nodes=106 data=0x1feb4eb7510`；候选的 15,203 项绑定集与 `pre-activate` 的 `behavior-data` 都属于后者。**任何"读源图"的仪器都必须钉住候选的源图**，否则零计数无意义（C1 前两轮即因此判无效）。
2. **正控可以由我们自己的注入提供**：插件以 `REL::ID(58377)` 直调派发 `WeapEquip`/`Unequip`，所以回放窗内 `calls ≥ 2` 是结构性的；配 `other-graph` 分类即可把"观察器存活/分类有效"与"源图零事件"分成两件事。
3. **窗口生命周期可复用**：并发窗口在实验入口同步建立（reset→deadline→identity→baseline→gate），由显示段首次带步数关闭 / 20 秒到期 / 世界重置结束；`managed_graph()` 每帧只比较指针复核成员关系，不解引用已释放对象。
4. **"零值可信"的格式要求**：`numeric=…/sum:N` 与 `sites=n:N` 同行对账、`t3-rows` 独立预算、`identity-mismatch` 逐字段对比、`window-aborted`/`window-closed` 带 `calls/other-graph`——这些是任何"零结果"被判读前必须同时具备的字段。

### 不证明（明示边界）

- B 不普遍证明历史隔离；A 不证明模板级评估不写 OAR 内部状态（仍为间接）；C 不外推到其它事件路由。
- `652/8223` 合并矛盾（运行时变量表含静态文件之外的注入内容）仍为独立残余。
- 候选侧 `iState≡0` 与 `currentDefaultState` 引擎镜像机制仍未闭合（G3 遗留）。
- 装载图 teardown 维持 NO-GO；每进程一张保留候选是显式例外。

### 后续锚点

G3-B 确定性姿态哈希（装备段 `bc160a8c/63f638a6/c407cb4e`、卸下段 `8fb4cd33/4f2aebde/c407cb4e`，截至 v66 已**八轮逐位复现**）；`load-bindings` 共享绑定集 15,203；B-v2 装备族序列（弓→剑→法术）；并发事件窗口的安装/武装/关闭三行格式与 `calls/other-graph/events/ring-dropped` 四计数。

<a id="g5-draft"></a>
## G5 完整输出姿态对照（执行方案 v2，2026-11-03）

> **状态：G5 执行方案 v2 保留，未冻结/未通过。** [2026-11-11 运行摘要](#status-20261111-session)记录 E1 v3、E3 2/2、E4 四块源快照与装备链速率的有界进展；[CAL v1](#c-g5-cal-v1)已写定校准规则，但输入/资源、完整贡献、同代阶段与后处理边界等前置资格尚缺。不能据此跳过门 1/2，原生校准运行 NOT_READY。v1 历史仍在 `c372600d7d596210ce909c57010e88bfcb280fb5`，旧日志不改写。

### 一、目标与参照职责

保留[原 G5 定义](#g5完整输出姿态对照)的完整目标：在相同受控初态、输入历史与时间线上，检验**全部有效贡献层、逐骨/通道权重、混合参数、播放速度及最终骨骼变换**。G3-B 的相位序列和候选哈希复现只是前置证据，不是姿态等价证明。

| 参照 | 在 v2 中的职责 | 资格与限制 |
| --- | --- | --- |
| **R2 原生源图与候选完整输出** | **正式主参照方向**；同时比较层/权重/混合/时钟与最终输出 | 先证明参照质量、可比输入/时间线和同阶段完整观测；尚未证明当前仪器能取得这些证据 |
| **R1 同剪辑同时间重采样** | 逐层内容、映射、时钟诊断；已知单层控制检查 | `SamplePartialTracks` 的单片段结果不能一般性地与混合 `poseLocal` 判等；authored 骨最多不等于主导层 |
| **R3 按原生语义重建合成参照** | R2 无法提供合格参照时的**另行评审后备方向** | 需独立论证权重、mask、additive、混合与输出语义；本方案不猜 ABI，也不声称只有 R3 才能做完整对照 |

若完整参照、权重或可比性无法建立，结论为**证据不足 / 尚未就绪**，既不能缩成 R1 后宣布 G5 通过，也不自动判插件失败。不得看到正式误差后换参照或改分母。

### 二、按依赖推进的五道门

下表是顺序工作包；前一门退出条件未满足，不进入后一门。每个涉及实现/运行的工作包须有其自身评审和授权，本次文档修改不授权这些操作。

| 门 | 目标与输入 | 交付物 | 退出 / 停止条件 |
| --- | --- | --- | --- |
| **1 参照与可比性审计** | 从既有源码、SDK 声明与保留 trace 出发，查清原生输出边界、字段来源、源/候选身份、输入与时间线、层/权重/混合和覆盖 | 字段/管线证据图、可观测源/候选配对方案、映射/贡献/时钟规则及缺项清单 | 只有能说明合格参照如何取得、被测差异如何保留才退出；若无法建立可观测配对，停止正式路径，另立原生输出/混合证据审计范围 |
| **2 经评审的只读观测实现** | 门 1 证据及获批的仪器范围 | 同代快照与身份/总量/丢失记录；1–2 个有意义的聚焦离线检查及结果 | 阶段、映射、完整计数、取消和失败记录可靠才退出；未知布局/调用或截断不可解释即停，不用猜测读取补齐 |
| **3 独立校准（不验收）** | 合格仪器、独立已知控制与重复性/时间量化观测；先声明统计与推导方法 | 控制来源、噪声/量化分布、容差推导、映射和实际覆盖计数 | 有独立依据才能提出数值；不得把源候选全部差异当噪声；无合格控制或合理容差则保持未就绪 |
| **4 完整协议冻结** | 门 1–3 的参照、映射/计数、控制与校准证据 | 参照/资格/配对、分母、全覆盖、阶段/空间、全部数值容差、时钟规则、判定/停点和回传格式的完整冻结版本 | 缺任何决定性规则不称“已预注册”；冻结在正式数据采集前完成 |
| **5 新正式验证** | 冻结协议、匹配工件与资源、独立于校准的新保留运行 | 完整两段及边界数据、覆盖/误差/源隔离报告和明确裁定 | 按冻结规则裁定；缺项不能通过，修订规则后须另用新正式运行，不回填旧结果 |

### 三、R2 资格与被测差异必须分开

**参照资格**由独立证据建立：源 trace 确实来自候选的控制器源图，输入/资源/骨架与预定时间线可对应，输出阶段可靠且完整。先定义字段意义、来源与读取生命周期，再决定如何观测；同相位或相近 `localTime` 不足以取得资格。

审计与记录必须包含：

- **图身份**：actor、controller 所持 `source_graph`、候选、generation、request/view 与生命周期；按 [G4 钉图事实](#g4-closure)核对归属，不能把任意 `player_graph()` 当源。资源替换/失效后不得沿用旧指针或旧参照。
- **资源与骨架**：project/behavior/clip/binding 的稳定身份及有意义的变体、mirror、additive、mask；骨名、父子关系、reference pose、映射和默认填充规则。共享 binding 指针/数量只是身份线索，不是结果等价证明。
- **受控前提**：初态、相关外部输入的内容/顺序/时刻与相位历史，源的自然游戏推进及候选的预定驱动时间线；不能靠写候选输出状态或手动推进/写源图来强制匹配。
- **完整观测**：源与候选各自的有效贡献层实例、逐骨/通道权重、blend/mask/additive 语义及所有贡献 clip 的时钟/模式/crop/rate；证明字段能解释观察边界，不凭绑定映射推断动态权重或通道归属。

**候选是被测对象**：在合格参照、受控输入和预定时间配对成立后，候选缺层、错权重/混合、错时钟或错输出，都是应按冻结判据检出的差异；**不能要求它们先相同，再把不相同的帧删成“不可比”**。候选缺骨/通道也不缩分母。真正的参照缺失/歧义、错图、未观测阶段或仪器丢失，才归为证据不足。

配对只按**预先声明、独立于 pose 误差及候选内部偏差**的输入/时间元数据规则进行。源自然推进，允许按冻结的时间量化规则取得对应记录；不得挑误差最小的邻帧、临时偏移候选时钟，或在缺失/多义时挑有利样本。候选 pose 哈希仅作重复性警戒，不能当 oracle 或删帧资格。

### 四、同阶段快照与完整覆盖

正式比较使用**Generate 已完成**的同一原生输出边界：候选可审计 `e_post_sample`，源需证明等价边界与作业完成/缓冲生命周期。clip 元数据、权重与 pose 必须属于同次 generation 的一致快照，不能将 Update 后的新时钟配前次 pose。

当前 `log_clip_reference` 在 `e_pre_sample`，即 Update 后、Generate 前；该结果及 step 0/bind-pose 基线只作诊断。`drive_graph_step` 的 pre→Generate→post 顺序提供审计入口，**不等于源端观测已实现**。

覆盖计划必须分开记录：

- **装备段 120 步**与**卸下段 120 步**，各有输入/起点、每步快照、transition/settled 边界与结束记录；另有合格源 trace 对应。两段不能拼作一个 120 步窗口。
- 所有步的覆盖和误差总量均保留；0/60/120 可作明细示例，不能代表完整过渡。批量驱动可能在一次显示调用内交付多步，必须保留各步及实际 dt/时间归属。
- 统计节点/clip/骨/通道的应有、已访问、已记录、遗漏数和各段/边界数量。旧 `collect_active_clips(8)`、256 节点/pose 上限及 leaf 文件名分组不能默认为全量；溢出、漏阶段、丢记录即不能声称完整覆盖。细节限流不得限制总量/最大误差/失败计数。
- 正式分母涵盖**全部有效骨/通道贡献与最终输出**，包括应由 reference/default-fill 提供的通道，不只是 clip-mapped/authored 并集。映射规则可预先固定、再应用于实时元数据并对账，无须在观察前猜实际清单。
- 层重叠是正常混合；active 但零权重不自动算有效贡献。不得要求每层都有独占骨；须用完整层身份、逐骨/通道权重和混合语义证明贡献。

[v24](#g2-v24-run)确实映射/比较了 99 条，其中 9 条位置不同；额外平移/提取运动只是当时解释。binding 不证明这些骨未 author，也不支持全局排除它们或任何有效贡献骨。对同名重复/缺失、未知通道必须保留原因，不以表长 116 或最大 authored 数作完整性证明。

IK/后处理若横跨两侧观测阶段，须先对齐阶段，或将对应范围明确保持开放。只比较 raw `poseLocal` 不能声称最终/渲染姿态一致；缩小的前置检查通过不关闭原 G5。

### 五、误差、时钟与独立校准

| 项 | 待实现/冻结的定义 |
| --- | --- |
| 旋转 | 两个四元数先检查有限值和非零范数并归一化；用符号不变的 `1 - abs(dot(q_ref, q_out))`，点积按浮点舍入边界处理。无效值单列归属，不混入正常误差 |
| 位置 | 在**同一明确的 parent-local 空间**或另有审计证明的共同空间中，算三分量差的欧氏距离；列单位、骨父级、root 与 extracted motion 的包含/剥离规则 |
| 缩放 | 原生输出的 **XYZ 三分量最大绝对差**；在 `clip.h` 显示读取仅留 scaleX / Ni uniform-scale 转换**之前**采集，不能用转换后标量遮住 Y/Z 差异 |
| 层/权重/混合 | 门 1 查清逐骨/通道语义、可观察值和比较规则；门 4 一并冻结数值/离散判据，不能因读不到便删出 G5 |
| 播放速度/时钟 | 按稳定 clip **实例身份**记录生命周期、loop-unwrapping、start/stop、single-play-end、playbackMode/speed、enforcedDuration、crop 与实际 dt；由已审计公式和时间量化规则比较，不要求每个 active clip 每驱动步都恰好推进 1/60 |

既有 `clip_reference_policy.h` 的位置度量是分量最大差，不能直接冒称上述欧氏距离；其归一化、符号不变旋转与 XYZ 缩放可作审计起点。旧 `clip-steps` 的 leaf 名分组与量化字符不能替代实例身份和完整时钟证明。

v1 的 `1e-4 / 1e-3 / 1e-5` 只保留为**历史示意值**，不是本方案的冻结默认值；v24 的混合差异也不是噪声上界。校准前先声明已知控制、重复次数/选样规则、统计量、舍入和容差推导方法；用独立控制的重复性与时序量化估计测量误差，不能用“候选与参照不一致”直接调大容差。

校准不产生 G5 通过/失败。校准后连同实际映射/覆盖分母、各数值容差、时钟允差/配对和批量交付规则一起冻结，再采集新的正式保留运行。控制或容差无独立依据则停在未就绪。

### 六、正式裁定、停点与回传

| 裁定 | 条件与处理 |
| --- | --- |
| **G5 通过** | 原定义的观察范围完整可比，所有要求的层/权重/混合/时钟/骨/通道覆盖且满足冻结容差，有限且同代、无丢失、源无干扰、规则无事后改变；所有必要证据同时成立 |
| **失败** | 合格参照及配对成立，候选差异超冻结容差，或缺少应有贡献/输出；合格观测中的候选非有限值亦须明确判失败并停。不得把被测缺陷改成删帧理由 |
| **证据不足 / 无效** | 源参照/身份/阶段/输入时间线不明、资格歧义、仪器截断丢失或覆盖不可证；保留原因、停止正式验收并回审计，不能宣称零失败或插件必错 |

源异常漂移/污染、崩溃、未知 ABI 或需要写源即停止；源 trace 自然推进须与候选造成的干扰按控制规则区分。reset/close/PreLoadGame/NewGame、来源或 generation 替换取消旧窗口，不拼接新旧记录；stale 记录不可继续解引用。中止必须有原因和已收/缺失总量，非有限/零范数要区分候选缺陷与参照/采集无效。

正式回传至少包括：

1. 冻结协议及参照资格/字段来源审计、独立校准与容差推导，匹配的 build/DLL/PDB/INI、游戏/SDK/OAR 与资源身份。
2. 两段完整 step/输入/边界和源配对表、两图身份/generation、clip 实例/模式/时钟、层/逐骨通道权重与 blend/mask 记录；完整覆盖分母、已观测/遗漏与无丢失对账。
3. 各步/各骨通道的误差总量/极值/失败数及逐名失败详情、未比较原因；明细预算外也须计数并保留可复核记录，**没有失败详情不等于零失败**。
4. 源非干扰控制、取消/关闭记录与裁定，校准数据与正式数据明确分开。诊断或校准成功不得升格为整关通过。

装载 teardown 维持 **NO-GO**；每进程一张保留候选的例外不扩大。G6 仍后置，DCMF 仍仅参考；本次不新增依赖或运行配置。

<a id="g5-audit-pending"></a>
### 七、六项审计处置（已完成；保留旧锚点）

| # | v1 问题 | v2 处置 |
| --- | --- | --- |
| 1 | 单层 R1 被当混合输出主 oracle | R1 限诊断/已知控制；R2 完整原生输出为有资格门的正式方向，R3 只作另审后备 |
| 2 | binding/authored 并集及 9 条差异被用于排除 | 保留 99 条比较事实；不据此排骨，分母覆盖有效贡献及默认填充，预定规则应用于实时元数据 |
| 3 | 权重/混合删出范围、每层要求独占骨 | 原完整目标不变；审计逐骨/通道权重与混合，允许重叠，无证据即未就绪 |
| 4 | pre-Generate 时刻、稀疏采样和身份/时钟不足 | 同代完成输出、装备/卸下各 120 步、全量总数、稳定实例时钟与独立于误差的配对；候选偏差不能删帧 |
| 5 | 度量/空间不清，混合误差被当校准噪声 | 明确归一化旋转、共同空间欧氏位置、转换前 XYZ 缩放；独立控制推导容差，再用新正式运行 |
| 6 | 先冻结、再实现/校准，前置小检查可能代替 G5 | 顺序改为审计→经评审仪器→独立校准→完整冻结→新正式验证；完整证据之前不称整关通过 |

### 八、当前下一步与导航

**门 1/2 仍有未关闭的前置项。** 以 [CAL v1 的 K1–K6](#c-g5-cal-v1)作为当前缺项清单，先核对输入/资源/控制资格，再补齐同阶段同代完整观测和 B3/B4；满足前置后才运行独立校准。E1/E3/E4 的历史通过范围保留，不能扩成完整 G5 或门 3 通过。下面 2026-11-03/04 的校验说明只描述当时状态。

**历史方案校验（2026-11-03）**仅做文档链接/锚点与原件保留检查，当时未构建或运行行为测试。**本轮修复（2026-11-04）**已经执行两个定向离线回归、旧驱动兼容编译和本地 CharacterPanel 构建，结果与限制见 [修复验证](build.md#g5-e1-evidence-repair-20261104)；仍未部署/上机，不产生新的游戏通过结论。

<a id="g5-evidence-plan"></a>
### 九、第一门证据实施与后续验证交接（2026-11-03）

**门 1 = NOT_READY。** 现有 E1 入口探针已经实现并有 v67/v68 三轮历史运行；当前修复版未部署/未上机。按 [更正与剩余工作](#g5-e1-evidence-repair)保留活性事实、补齐真正完成点与快照证明；E2/E3 尚未开始，不再沿用“探针均未实现”的旧状态。

这里的 E1–E3 是第一门的研究证据切片，不是上文 G3-B 历史事件实验 E1。顺序为 **E1 → E2 → E3 → 联合证据验收**；未知 hook/ABI/布局仍须逐项证实并另行评审。E1 的最小探针已按 [C-G5-E1 契约](#c-g5-e1-probe)预注册（观测点、头字节、判据 P1–P5、停点、实现影响面），**当前下一动作是按该契约实现探针**；契约中"不装 32898/63358"与"不叠加其它 detour 探针"是硬约束，改判据或加钩子须另立修订。

| 切片 | 前置依赖与目标 | 必交证据 / 退出条件 |
| --- | --- | --- |
| E1 输出完成 | 先追源调度与完成截面，再实现最小证据探针，最后取得原生命中 | 状态/调用图、字段来源/ABI/线程/寿命表、一致快照与完整计数证明；源生产者确实命中钉住的目标 |
| E2 完整贡献 | 使用 E1 已证的 epoch、输出序列和有效读取寿命 | 字段→消费者→寿命证据；覆盖全部有效实例/骨/通道的贡献账本，含 mask、blend、additive 及缺项 |
| E3 合格 R2 对 | E1/E2 可取得完整同代记录，源控制与资源资格可复核 | 可复现的初态/输入/资源与预定配对 manifest，完整源/候选记录；被测差异保留 |
| 联合验收 | 三份证据能说明同一链上的完成、采集保真、贡献范围及参照资格 | 解释全链且缺项清零，才可关闭门 1 并评审完整观察器；诊断成功不是 G5 通过 |

#### E1：先静态证明，再最小探针，再运行证据

1. 先追自然帧 32898 → 63358 → 63589 到实际输出，并单独核对 63359 → 63589；继续追 63358 跳过本地循环的调度/等待分支及 63568 job 路径。逐一画出 63581 的生成关联、双槽插值、IK、copy/fallback/skip 的生产者、消费者和完成点，不把两个直接调用点当全部路径。
2. 为每个拟读字段记录类型/偏移的原生证据、真实调用 ABI、所属对象、读写线程、起止寿命和同步条件；证明安全捕获点后才评审 hook。SDK 单 context 的 Generate 声明不是实际 ABI 的充分证据，外层 smart pointer 也不保证内部数组快照一致。
3. 区分 **lifetime epoch**（来源/root/graph 替换）、**native call sequence**（调用序号）和 **output completion sequence**（已证产物完成序号）。现有 source generation 只属于第一种；包装返回计数不能冒充第三种。
4. 记录 role、actor/controller/graph/character、request/view、epoch、线程、阶段、调用/完成序列、输出 identity 与实际更新范围；产物来源明确分成 generated / interpolated / reference / reused / unknown。涉及双槽时保留各槽版本、所属完成序列、完整精度时间及插值输入/输出关系。
5. 证明 clip 元数据、pose、完成序列和更新时间属于同一快照：分配容量、reference 初始化、实际写入条数和默认填充分别计数；复制必须在已证寿命内完成，不能在包装返回后解引用失效 scratch。相同字节仍可能重新生成，字节变化也不能独自证明完整生成。
6. 最小探针的原生 liveness 要有安装/武装/关闭和分支计数，且 **source 生产者实际命中钉住的控制器源图**，有能验证该路径产出/捕获的有意义正控；只有候选调用或其它图命中不证明源捕获。静态可达、buffer 非空、包装返回和调用次数都不能代替这项证据。
7. 交付状态/调用图、捕获点证明和完整计数对账。未知分支、来源/输出身份变更、stale、race、截断或 overflow 均保留原因并判未完成；不沿用旧地址/序列继续拼窗，不把诊断控制当完整 G5 样本。

#### E2：按真实消费者建立完整贡献账本

沿 58343 → 58355/58376 及 **58383** 继续核实临时生成状态，再按实际参与输出的 generator/mix 类型追权重、mask 与通道的消费者。58355 的 state+0x60 float 数组语义未证；不得先命名成 weight array，也不得用 pose onFraction 或 NodeInfo+0x80 计数器充作通用权重。

| 账本关系 | 必须证明并保留的内容 |
| --- | --- |
| parent/child 实例 → 输出序列 | 稳定实例身份、父子关系、类型、激活/销毁 epoch，与 E1 同代输出绑定 |
| 实例 → 骨/通道 → blend kind | 动态 mask、有效贡献范围、reference/default-fill、normal/additive 及实际混合规则 |
| weight/mask → 消费者 | 原生存储值的字段意义与读取寿命；没有有效权重字段时，保留经审计的算术输入/规则供离线推导，不发明字段 |
| 完整分母 → 已记录/缺失 | 全部有效贡献及最终输出逐项对账；未知类型/通道、过期 scratch 或漏记录仍算未覆盖 |

重叠层和 additive 按各自语义处理；不要求每层有独占骨，也不套用通用“权重和 = 1”。明细预算不得限制总量，未知项不能从正式分母删除。E2 的退出物是完整贡献账本及其字段/消费者/生命周期证据，不是一个通用 weight reader。

#### E3：自然源轨迹与预定输入/时间配对

在 E1/E2 的完成与贡献证据基础上，先声明自然源控制，再采集源/候选：

- 固定 save、装备、骨架、project/behavior/资源与 OAR 配置，记录相关初态、外部输入内容/顺序/时刻及时间历史。源由正常游戏自然推进；控制运行用来区分正常演化与候选干扰，不能把自然推进本身称污染。
- 保留实际 **bound-resource identity**（含替换/变体及可复核资产身份），不能只记原始 animationName、leaf 名或共享绑定集；所有相关 clip 的时钟、dt、模式与完成阶段/代数用完整精度记录，不能从四舍五入日志补精度。
- manifest 在看 pose 误差前固定输入/时间配对、量化与缺失/多义处理规则，独立于候选内部偏差；禁止挑误差最小邻帧、复制输出变量、临时调时钟或强制更新源图来制造匹配。
- v66 的源 binding **12220 / SVFI / time 13.06** 与候选 **1022 / original / time 1.8667** 来自不同上下文/时间，不是合格对；该会话验 G4，不能追溯改判 G5 失败。
- 合格源/控制及预定配对成立后，候选错资源、缺层、错权重或错时钟均是被测差异，不能据此删帧。缺失/多义源参照、未知阶段或采集丢失才记证据不足。

交付可复现的配对 manifest、资格与控制记录、全部源/候选同代数据和缺失账本。R1 继续只作诊断；尚无合格 R2 对时保持未就绪，不用候选八轮重复哈希代替源 oracle。

#### 第一门联合验收与未来模块边界

**门 1 仅在三份证据可联合解释时关闭**：源完成与捕获保真、完整贡献范围和合格参照协议必须相互对应。静态证据不足以证明 liveness 时，可另行评审最小研究探针；这不豁免门 1，也不把研究切片称作已完成的 G5 观察器或门 2。

| 未来边界（设计方向，未实现） | 责任与限制 |
| --- | --- |
| 窄域 pose_evidence 模块 | 只负责经审计的证据采集/记录，保留现有 controller、显示和产品策略边界；不集中改写 independent_graph 全流程 |
| session / policy | session 持有 identity/epoch、arm/cancel/close 与计数；纯 policy 管配对/完整性/判读，未知资格不转为成功 |
| native callback | 仅在有效寿命内复制完整精度数据为自有记录，不把原生 scratch 指针留给稍后读取；格式化与文件 drain 放回调外 |
| 有界 transport | 先证生产者/线程拓扑再选队列；源可能多生产者，不能直接复用未经证明适用的 SPSC。lost/truncated/overflow 全计数，任一损失使完整性无效 |
| 诊断开关与验证 | 默认关闭、改设置后重启，保留现有控制/设置；每片另审 1–2 个有意义聚焦检查，并另证原生运行 liveness；本次无新安装键、二进制或测试矩阵 |

联合验收只交付进入完整观察器评审所需的依据；之后仍按 **经评审仪器 → 独立校准 → 完整协议冻结 → 新正式验证** 推进。校准/诊断结果不能裁定 G5，通过完整验收前不改其原范围。

#### 稍后验证的前置条件与回传清单

**当前记录安装 v66 / dormant INI 不含新的 E1–E3 探针**。切换既有设置不能生成这些记录；本节没有可运行的新 INI 配方或新验证包。用户稍后验证须先取得对应切片的实现和独立交接，不把未来验证意向算成本次证据。

| 切片验证 | 希望取得的结果 |
| --- | --- |
| E1 | 钉住源的实际 producer liveness、完整阶段/调用/完成序列、一致快照和无损关闭对账 |
| E2 | 与 E1 同代的完整贡献账本及逐骨/通道覆盖，含有效权重/推导依据和全部缺项 |
| E3 | 受控自然源与预定配对 manifest、完整源/候选记录；差异/证据不足分开 |
| 联合 | 三片完整证据可共同复核门 1；不提前请求 G5 通过结论 |

运行前与回传要求：

1. 对应切片已实现，native 兼容性/ABI/线程/寿命已评审，聚焦检查通过；归档 DLL/PDB 与 build identity 成套，实际加载的版本、设置、OAR/骨架/资源与 manifest 一致。不能用本次文档提交重标旧工件。
2. 按未来该片的会话协议重启、显式 arm；保留安装/武装身份与 producer 正控、阶段完整记录和 close/abort 原因，核对 expected/visited/recorded/missing、generation 与 lost/drop/overflow 总量。无源命中或无结束对账不能按零差异判成功。
3. 回传 **完整 CharacterPanel 日志**、完整精度证据文件/配对 manifest、人工操作与动作时间、字段来源/代数/覆盖/遗漏计数、失败详情及实际加载工件/设置/资源身份；不能只截摘要、失败明细的前几行或 pose 哈希。
4. 未知 ABI、源污染/异常、崩溃、非有限值、身份丢失、race、缺阶段或 drop 即停；保留原因和已有/缺失数量。自然源演化按独立控制解释；合格记录中的候选缺陷与源/采集无效分别归类。
5. 依未来会话协议关闭/取消并归档，再恢复记录的设置并重启；不能盲用旧会话开关或继续解引用失效对象。装载 teardown 仍 NO-GO，候选保留例外不扩大，G6 与 DCMF 对照继续后置。

<a id="e1-closure"></a>
<a id="g5-e1-evidence-repair"></a>
## E1 当前边界与仪器修复（2026-11-04）

**裁定：入口活性已有证据，E1 完整证据未闭合。** 本节取代先前“E1 收官”的汇总解释；三轮日志、v1/v2 契约和原始字段名保留为历史，新的取证代码不补写旧结果。

### 已观察到的事实与不能推出的结论

- [首轮 concurrent](#c-g5-e1-run1) 覆盖暂停面板，只有候选入口；[补充 A](#c-g5-e1-run2) 游玩窗口记录 `e:engine=161006`；[v68 resumed](#c-g5-e1-run3) 记录钉住源图 `pinned-engine=411`。调用者 RVA `0xba3f6e` 支持源的自然本地循环入口活性。
- v68 明细显示源采样来自六个 tid，候选明细在另一 tid。**不能据此识别工作线程池、32898 的执行线程或调度器**，也不能从渲染 drain 的日志时间戳推断原生逐帧间隔；记录没有生产时间戳或原生帧序号。
- 队列快照中的 `flag=0/queue-flag-frames=0` 只表示渲染轮询时读到零，不能证明整个窗口或每个原生调用都没有进入分派分支。
- **63856 读取 poseLocal 并写场景节点的 local 旋转/平移**（匹配镜像 RVA `0xbc2fe0..0xbc3237`）；它不是 poseLocal 的最后写入者。旧 recorder 在原函数头之前执行，`completions-pinned=411` 实为 411 次应用**入口**命中；`count=116` 是钳制前的请求数，不是实际写骨数或完整生成证明。原生返回、有效输出序列、默认填充/容量/实际更新范围、寿命和同代快照均仍待证。
- v68 689 次插件入口加 411 次源入口应有 1,100 个可配对入口，账本只纳入 1,099 对。打印预算 2,048 行与此不同：预算后仍应继续记账；旧关闭未做尾部封口，`drop=0` 不等于完整配对。Q3 的“未观察到重入”也不能作为无重入证明。

### 本轮代码修复与接口

- [共享跳板布局](../src/animation/behavior_graph/detour_block_policy.h)：每钩子 256 字节，原指令块在 +128；两钩子池共 512 字节。liveness 与 typed-setter 仪器都使用这一布局，修复旧越界以及 setter 两次 192 字节分配挤入 256 字节池的问题。
- [策略与传送](../src/animation/behavior_graph/probe_liveness_policy.h)：`ProbePushGate` 对 push 只尝试一次原子占用，不等待、不自旋；竞争与满环都丢弃新记录并计数。`ProbeAdmission` 用同一原子状态中的关闭位和在途数量封口，`ProbeRecorderLease` 覆盖已接受记录器。首窗只在状态准备后开放；初始关闭快路拒绝不改计数，避免与首次开放互相覆盖。
- [原生接驳](../src/animation/behavior_graph/probe_liveness.cpp)：仍只钩 63589 与 63856 入口，没有新增完成点 hook。安装不完整时不能 arm。关闭先停止接受，若仍有记录器则留待后续渲染采样；在途归零后通过同一 drain/配对路径处理尾部，再汇总并清身份。读档走相同路径，不在渲染线程阻塞等待。
- 新明细为 `sample-entry` 与 `apply-entry`，`drain-seq` 仅表示排出次序，`requested-count` 为应用入口请求数。`paired-apply-entries` 是同 tid/graph 的两个入口记录配对，不是输出完成。display/resumed 时长及 `SamplingLiveness=off|on|concurrent` 不变；转段行标 `provisional=yes`，是活动中累计快照，不是原子分段封口。
- 最终 close 给出 `eligible-records / recorded-records / drained-records / missing-records`，关系为 `eligible = recorded + ring-dropped`、`recorded = drained + missing`；分别列 `contention-dropped / full-dropped`，另列 `drained-samples / drained-applies / pair-no-slot / pair-mismatch / orphan-applies / unpaired-samples / suppressed-rows`。任何传送丢失或缺账均不得宣称完整覆盖；线程账本失败与窗口边界/合法早退造成的未配对样本单独解释。`transport-complete=yes` 只描述传送，不是 E1 或 G5 通过。

### 离线验证、依赖与下一步

[两个回归场景](../tests/probe_liveness_repair_test.cpp) 使用生产布局/编码器/传送/接收策略：带边界哨兵的双钩子原指令跳转与入口实参；并发生产、竞争拒绝及持有在途记录器时关闭后仍收齐尾部。旧 [测试驱动](../tests/probe_liveness_test.cpp) 仅做新命名的兼容编译。本轮不跑全套、不更换 SDK/依赖或 CMake 配置。离线结果和生产构建限制见 [修复验证](build.md#g5-e1-evidence-repair-20261104)。

依赖仍为 C++23、既有 CommonLib/地址库与固定 AE 1.6.1170 头字节；原子语义依据 [C++ draft atomic flag](https://eel.is/c++draft/atomics.flag)、[release/acquire](https://eel.is/c++draft/atomics.order)，入口实参/栈约定对照 [Microsoft x64 ABI](https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention?view=msvc-170)。这些通用资料不证明 Skyrim 运行兼容性。修改时须同步上述源码/测试、本节、索引及日志格式，保留历史锚点和原件。

**未部署、未上机。** ~~最新记录安装仍为 v68；后续先归档匹配 DLL/PDB/INI 与加载身份，再验证两处安装、唯一窗口、resumed 源命中、无丢失的最终对账及取消路径~~（**已完成**：v69 两轮，[首轮](#c-g5-e1-v69-run1)验证安装/对账、[第二轮](#c-g5-e1-v69-run2)转段完整、resumed 源命中 408、全零缺口对账；INI 已恢复 dormant）。~~不要使用旧日志套新字段。~~ G4-C 已在 v69 上重跑关闭（[attempt 2](#c-g4-c1-concurrent-events)，2026-11-05）；typed-setter 窗口（63607/63609）留待需要时按同款正控重跑。**静态遗留四项已由 [audit32](behavior-graph-construction-audit.md#audit32) 闭合（2026-11-05）**：完成点=63856 返回后的两处捕获窗（63589 尾 + 63568 尾；63856 全镜像共 5 个调用点，其中 63563 物理回调路径与 63572 两处列入探针计数正控）、有效输出范围三计数按路径三分（主路径 n=`[g+0x240]`、fallback n=槽 numData 不写回、63563 n=bp）、poseLocal 单点 lazy 分配无 free（mempool 57056，跨 epoch 复用=同缓冲重写）、`[g+0x148]` 四写点全在 58316 与 57148 之间顺序执行。**运行侧取证已立 [v3 契约](#c-g5-e1-v3)（判据 R1–R7，2026-11-05 预注册）**：下一步 = v3 实现评审 → 上机 → E2/E3。G5 门 1 NOT_READY，G6 与装载 teardown 边界不变。

<a id="c-g5-e1-probe"></a>
## C-G5-E1：源采样 producer liveness 最小探针（契约 v1 + v2 + **[v3](#c-g5-e1-v3)**，2026-11-03/04/05）

> **历史契约与三轮运行，当前解释已被 [E1 后续审计](#g5-e1-evidence-repair)更正。** 下文保留预注册判据和原始读数；`completion` 是旧字段名，实际表示 63856 应用入口，不能推出产物完成、实际写满或完整 E1 关闭。修复后的日志格式与关闭对账见前述更正；没有新运行通过结论。
>
> **实现（v67，2026-11-03）**：`probe_liveness_policy.h`（三态解析、计数器、环、配对账本、窗口、`encode_detour_thunk`）+ `probe_liveness.{h,cpp}`（63589/63856 两处只读 detour、队列快照、关闭行）+ `SamplingLiveness` 键 + `PLUGIN_SOURCES` 32→33 + 主线程接驳（`kDataLoaded` 装、`PreLoadGame/NewGame` 失效、render 每帧 drain、实验 arm、显示关闭 close）。Engine-free [7 例](../tests/probe_liveness_test.cpp) 在 `cl /std:c++latest /W4 /WX` 下通过（产物 `build/behavior-graph-v67-g5e1-liveness-20261103/`）；Release 构建通过，身份/哈希与两条环境修复见 [v67 构建记录](build.md#v67-g5e1-liveness-2026-11-03)。**同一用例发现了旧 detour 的实参槽位缺陷，见 [audit30](behavior-graph-construction-audit.md#audit30)。**

### 一、问题 → 观测点 → 判据

| # | 问题（来自 audit29） | 观测点 | 通过判据 | 反证 / 无效条件 |
| --- | --- | --- | --- | --- |
| Q1 | 32898 的帧线程是哪条，`[q+0x430]` 在实机是否为 0（本地循环是否运行） | 每帧只读快照 `q = *(void**)0x31be0d0`、`byte[q+0x430]`；`63589` 入口的线程 id 与调用者返回地址 | 窗口内 `entries_engine_caller > 0`（返回地址 == 游戏基址 + `0xba3f6e`） | 若 `queue_flag_nonzero_frames > 0`，本地循环结论必须带"该窗口分派分支为活"的限定；若 `entries_engine_caller == 0` 而帧计数增长，判 Q1 负结果并回评审，不加钩子 |
| Q2 | 钉住的控制器源图是否真的被采样（而非只有候选在动） | `63589` 入口的 `graph` 指针 | `pinned_engine_entries > 0` **且** `completions_pinned > 0`，且同窗口 `entries_plugin_caller >= 1`（插件自己的 `s_sample/s_generate` 调用可见，证明返回地址判别有效） | 只有插件调用、没有引擎调用 ⇒ 源未被采样；只有引擎调用、没有插件调用 ⇒ 判别未受检验，正控不成立 |
| Q3 | 队列负载（63359 类消费者）是否在另一线程重入采样 | 全窗口 `63589`/`63856` 的 `tid` 分布 | 钉住图的配对全部落在同一 `tid` | 出现两个 `tid` 且都属于钉住图 ⇒ **报告为反证**（"单帧线程"假设被推翻），不改判成功 |

### 二、观测点（两处只读 detour + 一处每帧只读）

选点依据与头字节均按固定镜像（`843823c1…`）逐字节核对，`no RIP-relative`，边界恰好放得下 5 字节 jmp：

| 目标 | RVA | 预注册头字节 | 边界 | 语义与捕获内容 |
| --- | --- | --- | --- | --- |
| **63589** `BShkbAnimationGraph` 采样入口 | `0xbb2a40` | `40 56 57 41 56`（`push rsi; push rdi; push r14`，5 字节，**无需 pad**） | `+5` 正好是 `sub rsp,0x30` 之前 | rcx = graph；`[rsp]` = 调用者返回地址；`word[g+0x240]`、`byte[g+0x246]` 只读 |
| **63856** poseLocal 末写 | `0xbc2fe0` | `48 81 EC 88 00 00 00`（`sub rsp,0x88`，7 字节；补丁 `E9 rel32 90 90`） | `+7` | rdx = `&g+0x160` ⇒ **graph = rdx − 0x160**；r8d = count；rcx = pose 缓冲 |
| 队列状态（**不装钩子**） | `0x31be0d0` / `+0x430` | — | — | 在既有 post-UI 采样里每帧读一次，`q` 与 flag 写进窗口计数 |

`63856` 的 `rdx−0x160` 还原是**精确字段地址**（两个调用点 63589 `0xbb2af7` 的 `lea rdx,[rdi+0x160]`、63568 `0xbaebd8` 的 `lea rdx,[rbx+0x160]`），不是指针算术猜测；`rcx` 只用于记录 pose 缓冲身份，不用于归属。

**为什么不钩 32898/63358**：`63589` 的调用者返回地址已经能区分"引擎本地循环（`0xba3f6e`）"与"插件直调（模块内）"；`32898`（`4c 8b dc 57 48`，边界 +8）与 `63358`（`40 55 53 56 57`，边界 +5）的头字节虽然同样可打补丁，但**本版不装**——若 Q1 出现"帧计数增长而引擎调用为 0"，按停点回评审，**不得现场加钩子**。

### 三、记录、传送与采集纪律

- **计数器（始终递增，原子）**：`entries_total`、`entries_engine_caller`、`entries_plugin_caller`、`entries_other_caller`、`pinned_engine_entries`、`completions_total`、`completions_pinned`、`unattributed_completions`、`queue_flag_nonzero_frames`、`frames`、`ring_dropped`。
- **明细记录（有界环）**：`{seq, kind(entry|completion), tid, graph, caller_rva 或 count, num240, flag246, ticks}`；仅当 `graph == pinned` 或 `caller` 落在插件模块内才入环，避免别的角色的图刷爆环。环深 4,096；**明细丢失不改写计数器**，但 `pinned` 明细丢失使 Q3 的配对证据无效（记 `detail_dropped` 并判该窗口不可用于 Q3）。
- **纪律**：记录函数内不分配、不加锁、不调用引擎、不读字符串、不打印；`tid` 用 `__readgsqword(0x48)`（TEB，无调用）；`ticks` 用 `__rdtsc()` 且**只作同线程内序关系**，跨核不作时序证据。格式化与落盘在既有 render drain 里做。
- **不写任何引擎字段**：探针只读寄存器/栈与已证偏移（`+0x240`、`+0x246`、`rdx−0x160`、队列全局）；不写槽、poseLocal 或任何图字段。

### 四、窗口生命周期与守卫

- 新键 `[Experiment] SamplingLiveness = off | on | concurrent`（默认 `off`）；未知文本 → `nullopt` → 回退 `off` 并打印原字符串（沿用反模式 14：派生 INI 也要逐键断言）。
- `on`：面板外 F7 打开 20 秒窗口，要求 `IndependentGraph=off`（沿用 E1 的旧语义）。
- `concurrent`：由实验在**自己两次派发之前**同步武装，覆盖两次注入、settle、显示与关闭/重置边界；要求 `IndependentGraph=inject`、`DisplayPose=true`、`SourceTimeline=off`、`SourceEventCapture=off`（与另外两个 detour 探针互斥，**不叠加**）。
- 关闭原因只允许：`display-close`、`expired`、`world-reset`、`abort:<原因>`。一次进程只开一个窗口；关闭后不再重开（重跑要重启）。
- **身份**：`arm` 时用既有 `managed_graph(*holder, &a_source_graph)` 校验并钉住源图，记录 graph/behavior/data 指针与 epoch；每帧只比较指针，不解引用已释放对象。epoch 变化或读档 → `world-reset` 关闭并作废。
- 关闭行必须携带第三节的全部计数器与 `detail_dropped`。

### 五、判据（运行前固定，运行后不放宽）

| 判据 | 内容 |
| --- | --- |
| **P1 安装与存活** | 两处头字节逐字节相符并记录 stub/original 地址；窗口内 `entries_total > 0`、`frames > 0`；任一不符 ⇒ 该窗口无效（不是负结果） |
| **P2 源命中（Q2）** | `pinned_engine_entries > 0` **且** `completions_pinned > 0` **且** `entries_plugin_caller >= 1` |
| **P3 配对完整性（Q3）** | 每条钉住图的 `63589` 记录之后、同一 `tid` 上、在下一条钉住记录之前出现至多一条 `63856`；出现"有 entry 无 completion"只**记录比例**，不作为失败（`63564` 谓词为真、`numAnim==0` 等提前返回是合法形态）；出现 completion 在 entry 之前或跨 `tid` ⇒ 该窗口配对无效 |
| **P4 线程（Q1/Q3）** | 钉住图的所有配对 `tid` 唯一 ⇒ 支持"单帧线程"；两个及以上 `tid` ⇒ 报告反证，禁止改写成通过 |
| **P5 队列状态** | `queue_flag_nonzero_frames` 与 `frames` 一起报出；非零时必须现场读 `[q+0x430]` 的原始字节并复核，不允许用"推测"代替 |

**负结果的定义**：P1 通过而 P2 失败（窗口覆盖完整、正控成立、无 drop），才可写"该窗口未观察到源图采样"。P1 失败、窗口缺失、drop 污染、`tid` 反证都属**无效**或**反证**，不写负结果。

### 六、停点与回传

- 停：头字节不符、安装失败、trampoline 耗尽、窗口未覆盖两次派发、`ring_dropped`/`detail_dropped` 影响判据、epoch/身份变化、世界重置、崩溃、非有限值、任何需要放宽判据的冲动。
- 回传：完整 CharacterPanel 日志、窗口关闭行全部计数器、钉住图的明细（含比例与缺失原因）、实际加载的 build/DLL/PDB/INI 身份与游戏/资源身份、人工操作时序（两次注入的墙钟时刻）、以及"哪些判据通过/失败/无效"的逐条结论。**不得只回传摘要或前若干行明细。**

### 七、实现影响面（评审用，本契约不授权实现）

| 项 | 内容 |
| --- | --- |
| 新文件 | `src/animation/behavior_graph/probe_liveness_policy.h`（解析、计数器、环、窗口）、`probe_liveness.h`、`probe_liveness.cpp` |
| 构建 | `PLUGIN_SOURCES` **32 → 33**（`probe_liveness.cpp`） |
| 设置 | 新增 `SamplingLiveness` 与 `sampling_liveness_mode()`；沿用 `parse_*` 的三态模式 |
| 接线 | `kDataLoaded` 装 detour；`kPreLoadGame`/`kNewGame` 走 `invalidate_*`；`concurrent` 由实验 arm、显示关闭路径 close |
| 互斥 | 与 `SourceEventCapture`/`SourceWriteCapture`/`SourceTimeline` 不叠加；同开时本探针 `install-skip` 并打印原因 |
| 原生调用 | 新增 2 个 detour（63589、63856），**不加任何 native call/write**；头字节不符不安装 |
| 离线回归（engine-free） | ① 三态解析（大小写、`true/1`、拼写错误→`nullopt`）② 计数器/配对账本（有 entry 无 completion、跨 tid、drop→无效）③ `rdx−0x160` 还原的边界（空指针、非 8 字节对齐、`rdx==0`）④ 每线程槽表溢出 ⑤ 关闭行格式化包含全部计数器 |
| 文档同步 | 本节 + README + CHANGELOG + G5 行；不改写 audit28/29 的历史读数 |

### 八、明确不做

- 不做 E2 的层/权重/通道账本，不做 E3 的源/候选配对 manifest，不采集 pose 数值、不比较误差、不冻结容差。
- 不钩 `58343`/`63666`/`63582`/`59303`（槽轨道的最终填充点是 E2 的题目，见 audit29 第十节）。
- 不改产品路径、不写候选/源字段、不增加依赖、不改变既有窗口语义。
- 不把本探针的通过当作门 1 关闭：门 1 仍需 E2、E3 与联合验收。

<a id="c-g5-e1-v2"></a>
### v2 修订（2026-11-04，预注册，先于实现与运行）

**动机**：首轮证明 `concurrent` 窗口结构上覆盖不到引擎的采样期——实验在面板内 arm，而面板是暂停语境（[首轮运行](#c-g5-e1-run1) 995 帧 `e:engine=0`；[补充运行 A](#c-g5-e1-run2) 游玩态 ≈215 次/帧）。v2 只改**窗口语义**，判据 P1–P5 不变。

**变更（四项）**：

| # | 变更 | 理由 |
| --- | --- | --- |
| V1 | 窗口改为两段：`display`（arm→首次 `display-close`）+ `resumed`（该次关闭后 **10 秒**，从转段时刻重新计时）；只有第一次 `display-close` 触发转段，第二次起按原语义关闭 | 让 pin 保留到引擎恢复采样之后 |
| V2 | 计数器**累计**，转段时输出一行 `window=segment from=display to=resumed seconds=10 …` 携带当时全部计数；关闭行仍是累计值 | 两段都可单独读取，又不破坏"一个窗口"的完整性 |
| V3 | 记录器门 `g_gate` 改为原子（`window.active()` 是普通 bool，记录器可能跨线程） | 消除数据竞争 |
| V4 | 插件发起的 `63856` 也入明细（按"最近一次插件 entry 的图"匹配） | 让 P3 对插件/候选路径同样可评 |

**判据（不变，补充读法）**：

- **P2 的"源命中"必须在 resumed 段成立**：`关闭行.pinned-engine − 转段行.pinned-engine > 0`，且 `completions-pinned` 同步增加。display 段的零不算负结果（该段引擎不跑）。
- **P3**：`entry` 与 `completion` 的配对按"同一 tid + 同一 graph"在**全窗口累计**上判定；有 entry 无 completion 只记录比例。
- **P4**：钉住图的所有配对必须同 tid；出现两个 tid 报反证。
- **P5**：`queue-flag-frames` 累计值在两段都报；display 段若已非零，必须在结论里注明分派分支为活。
- **无效条件新增**：若 `display-close` 没有在 display 段内发生（窗口在 display 段到期），则没有 resumed 段，本窗口对 Q2 无效——只能按首轮的方式记"未检验"。

**已声明的限制**：resumed 段**不做**图成员校验（只比较指针，不解引用；避免读到已释放对象），依赖 `PreLoadGame/NewGame` 的 `invalidate` 与 10 秒短窗；若中途读档，按 `world-reset` 关闭并作废。

**不变量**：一次进程一个窗口；不叠加其它 detour 仪器；不新增 native 调用/写入；头字节仍按预注册校验。

<a id="c-g5-e1-run1"></a>
### 首轮运行（2026-11-04 00:10，v67，concurrent）

> **前向更正（2026-11-04）**：本节为历史记录，当前解释与仪器有效性以 [E1 修复后边界](#g5-e1-evidence-repair)及 [audit30](behavior-graph-construction-audit.md#audit30) 为准；旧 `completion` 字段不表示输出完成，G4-C 零事件与旧 setter 零写入无效。

**工件与设置**：DLL `e725066c…3457e`（1,501,696 字节，身份 `…-7dd5ea7458-…`）、PDB `1a4da885…7bc4`；运行 INI 9,465 字节 `ff3c6f34…`（`IndependentGraph=inject`、`DisplayPose=true`、`DisplayStep=true`、`SamplingLiveness=concurrent`，其余仪器全关）；日志 [221,958 字节 `b5318dcb…`](diagnostics/CharacterPanel-g5e1-liveness-first-20261104-0010.log)。部署记录见 [v67 build](build.md#v67-g5e1-liveness-2026-11-03)。

**安装与窗口**：两条 detour 均按预注册头字节安装（63589 `head-size=5`、63856 `head-size=7`），地址 `0x7ff625842a40`/`0x7ff625852fe0`。00:11:34 读档触发 `invalidate-requested reason=world-reset`（窗口未开，无副作用）。00:12:00.314 面板打开（`panel-open`，源快照 `paused-source`），00:12:00.333 实验开始并 `concurrent-armed actor=00000014 epoch=1 graph=0x1ef241c4680 behavior=0x1ea737f5900 pinned=yes`；窗口于 00:12:20.345 到期关闭（20.012 s，995 帧）。

**关闭行（原文）**：
`window=close reason=abort actor=00000014 epoch=1 frames=995 entries=1314 e:engine=0/plugin=1314/other=0 pinned-engine=0 completions=1314 pinned=0 unattributed=1314 tracked-entries=1312 paired-completions=0 pair-mismatch=0 pair-no-slot=0 threads=1 queue-seen=yes queue=0x7ff627e4e0e0 flag=0 queue-flag-frames=0 detail=1312 detail-dropped=0 ring-dropped=0`

**逐条判据**：

| 判据 | 结果 | 依据 |
| --- | --- | --- |
| **P1 安装与存活** | **通过** | 两条 detour 按预注册头字节安装；`frames=995`、`entries=1314`；`reason=abort` 是**标签缺陷**（见下），不是窗口缺失 |
| **P2 源命中（Q2）** | **未被检验（不是负结果）** | `pinned-engine=0`、`completions-pinned=0`、`e:engine=0`——引擎的帧路径在窗口内**一次也没调用 63589**；正控成立（`e:plugin=1314`） |
| **P3 配对完整性** | 插件记录不可评 | 1312 条明细全是候选图 `0x1efc0dc9080`；`63856` 的明细只在钉住图时入环，故插件调用没有对应 completion 行（`paired-completions=0`） |
| **P4 线程唯一性** | 部分 | 明细内只有一个 tid（14696）；钉住图无记录，不能据此断言"引擎单线程" |
| **P5 队列状态** | **通过且关键** | `queue-seen=yes queue=0x7ff627e4e0e0 flag=0 queue-flag-frames=0`：延迟更新队列存在，模式字节**全程为 0**，即分派分支从未成立 |

**两条独立事实（本轮真正收获）**：

1. **面板是暂停语境**：`ANIMATION WORLD … snapshot=paused-source reason='panel-open'`；窗口期间引擎不跑帧路径，所以 `e:engine=0`。这同时解释了为什么历史上"面板内驱动"必须由插件自己调 63589——引擎在这段时间不采样。
2. **`[q+0x430]` 在实机为 0**：995 帧里 `queue-flag-frames=0`，`q` 指针稳定在 `0x7ff627e4e0e0`。audit29 的分派分支在正常游玩状态下**不成立**，本地循环才是路径——与静态审计的"唯一可达采样入口"一致。

**仪器缺陷（已修，未进入本轮二进制）**：`close_probe_liveness_window` 的字符串映射漏了 `"expired"`，导致到期关闭被标成 `reason=abort`。该 bug 只影响标签，不影响计数；修复已进源码（下一次构建生效）。另有一处可改进项：插件调用的 `63856` 不进明细，使 P3 无法评估插件侧配对——列入 v2。

**结论与下一步**：本轮证明安装/窗口/队列快照可用，但**按设计无法回答 Q2**——`concurrent` 窗口由实验 arm，而实验发生在面板暂停期。要取得"钉住源图真被引擎采样"的证据，需要把窗口延伸到**面板关闭后的恢复游玩期**：这属于窗口语义变更，须先预注册 [C-G5-E1 v2](#c-g5-e1-probe)（v2 = 显示关闭后转入"恢复期"，再到期关闭；判据 P1–P5 不变），不得直接改代码后补测。

<a id="c-g5-e1-run2"></a>
#### 补充运行 A（2026-11-04 00:22，v67，`on` 模式，零代码改动）

> **前向更正（2026-11-04）**：本节为历史记录，当前解释与仪器有效性以 [E1 修复后边界](#g5-e1-evidence-repair)及 [audit30](behavior-graph-construction-audit.md#audit30) 为准；旧 `completion` 字段不表示输出完成，G4-C 零事件与旧 setter 零写入无效。

**目的**：用已部署的 v67 二进制回答"引擎在正常游玩中到底调不调 63589"，即 Q1 的正面证据。INI 9,456 字节 `11d94ec2…`（`SamplingLiveness=on`、`IndependentGraph=off`，其余全关）；日志 [5,669 字节 `c0a07a85…`](diagnostics/CharacterPanel-g5e1-liveness-on-mode-20261104-0021.log)。

**过程**：00:21:08 加载时两条 detour 按预注册头字节安装；00:21:41 读档 `invalidate`；00:22:14.820 F7（`arm-requested mode=on`），00:22:14.839 开窗（`epoch=1 pinned=no`），00:22:34.864 到期关闭（19.99 s，748 帧）。

**关闭行（原文）**：
`window=close reason=abort actor=00000000 epoch=1 frames=748 entries=161006 e:engine=161006/plugin=0/other=0 pinned-engine=0 completions=155940 pinned=0 unattributed=155940 tracked-entries=0 paired-completions=0 pair-mismatch=0 pair-no-slot=0 threads=0 queue-seen=yes queue=0x7ff627e4e0e0 flag=0 queue-flag-frames=0 detail=0 detail-dropped=0 ring-dropped=0`

**读数**：

| 项 | 值 | 含义 |
| --- | --- | --- |
| `e:engine` | **161,006** | 748 帧内引擎经本地循环调用 63589 共 16.1 万次，**≈215 次/帧**——每帧把管理器里几乎所有图都采样一遍 |
| `e:plugin` | 0 | 未开面板，无候选步进，符合预期（本模式无 pin，`detail=0`/`threads=0` 也是预期） |
| `flag` / `queue-flag-frames` | 0 / 0 | **真实游玩态下 `[q+0x430]` 仍全程为 0**：延迟更新分派分支在游玩中不成立 |
| `entries`−`completions` | 161,006 − 155,940 = **5,066（3.1%）** | 有 3.1% 的 63589 调用没有走到 63856（提前返回：`+0x240==0`、`63564` 谓词为真或门失败）——这正好支持契约把 P3 定为"存在与顺序"而不是比例 |
| `reason=abort` | 标签缺陷 | 该二进制未含 `expired` 映射修复；按 19.99 s 间隔判定为到期关闭 |

**Q1 由此闭合**：引擎的每帧采样在游玩中**确实运行**，且走的是 63358 的本地循环；audit29 的"唯一可达采样入口"与"分派分支不成立"两条静态结论都得到运行侧支持。**Q2 仍未证**：`on` 模式没有 pin，无法把 16.1 万次调用归属到"候选的源图"，也没有 tid 归属（明细不入环）。这两项正是 v2 的目标。

<a id="c-g5-e1-run3"></a>
#### v2 运行（2026-11-04 00:32，v68，concurrent + resumed）

> **前向更正（2026-11-04）**：本节为历史记录，当前解释与仪器有效性以 [E1 修复后边界](#g5-e1-evidence-repair)及 [audit30](behavior-graph-construction-audit.md#audit30) 为准；旧 `completion` 字段不表示输出完成，G4-C 零事件与旧 setter 零写入无效。

**工件与设置**：DLL `63f2c169…64cec8`（1,503,232 字节，身份 `…-v68-g5e1-resumed-73bff1d32d-…`）、PDB `9ef8305d…8a78ed`；运行 INI 9,465 字节 `ff3c6f34…`（concurrent 配置，与首轮相同）；日志 [291,454 字节 `60311517…`](diagnostics/CharacterPanel-g5e1-liveness-v2-resumed-20261104-0031.log)。零 `[E]`，`source-drift=none` ×4。

**过程**：00:31:27 两条 detour 安装（63589 `head-size=5`、63856 `head-size=7`）；00:31:44 读档 `invalidate`；00:32:05.548 面板打开并 `concurrent-armed … graph=0x1efa8885800 behavior=0x1eaf44c7900 pinned=yes`；**00:32:15.135 转段**（display 段 461 帧、689 条插件 entry，`pinned-engine=0`）；00:32:25.168 到期关闭（resumed 段 10.03 s）。

**关闭行（原文）**：
`window=close reason=expired actor=00000014 epoch=1 frames=871 entries=89967 e:engine=89278/plugin=689/other=0 pinned-engine=411 completions=87279 pinned=411 unattributed=86868 tracked-entries=1099 paired-completions=1099 pair-mismatch=0 pair-no-slot=0 threads=7 queue-seen=yes queue=0x7ff627e4e0e0 flag=0 queue-flag-frames=0 detail=2048 detail-dropped=0 ring-dropped=0`

**逐条判据**：

| 判据 | 结果 | 依据 |
| --- | --- | --- |
| **P1 安装与存活** | **通过** | 头字节按预注册安装；两侧窗口均有帧与记录；零 drop |
| **P2 源命中（Q2）** | **通过** | 转段行 `pinned-engine=0` → 关闭行 `pinned-engine=411`，**resumed 段增量 411 > 0**，`completions-pinned` 同步 411；正控 `entries_plugin=689`（display 段） |
| **P3 配对完整性** | **通过** | `tracked-entries=1099`、`paired-completions=1099`、`pair-mismatch=0`、`pair-no-slot=0`；明细内逐对相邻（entry seq 1379 → completion seq 1380，同 tid、`count=116`） |
| **P4 线程唯一性** | **反证：单线程假设被推翻** | `threads=7`；钉住图的采样来自 **6 个不同工作线程**（9944/11476/11784/22404/26008/32744），约每帧一次（时间戳间隔 ~20 ms）并轮换；插件对候选的调用全部在渲染线程 35440 |
| **P5 队列状态** | **通过** | 两段 `flag=0`、`queue-flag-frames=0`；`q=0x7ff627e4e0e0` 稳定 |

**钉住条目的字段**（335 条明细全是同一形态）：`caller=0xba3f6e`（63358 本地循环）、`num240=116`、`flag246=1`——即每个被引擎采样的帧都走"生成 + 采样"路径，且写满 116 条轨道。

**四条结论**：

1. **Q2 成立**：钉住的源图在引擎恢复运行后**确实被采样**，每帧一次，且每次都配一条同线程的 63856 完成记录。E1 的三个问题（是否采样、哪条路径、完成边界）至此**全部有运行证据**。
2. **采样线程不唯一**：不是一个"帧线程"在跑 32898，而是**至少 6 个工作线程**轮流承担，逐帧轮换。audit29 的"单帧线程"表述据此更正；`[q+0x430]` 分派分支在整个会话里始终为 0，说明多线程采样**不是**那条分支实现的，而是 32898 本身被多个工作线程入口调用（其静态调用者本就有 10 个）。
3. **对 G5/E3 的硬约束**：源的 poseLocal 每帧由不同工作线程写入，而候选由插件在渲染线程驱动——**源与候选天然跨线程、跨时钟**；任何快照/配对规则必须按线程分账，不能用"同线程相邻"来对齐源与候选。
4. **仪器余量**：`threads=7` 对 `Max_Threads=8` 只剩一格；更热闹的场景可能耗尽槽位（`pair-no-slot`）。若后续需要，v3 再扩槽——本轮判据不受影响。

**未覆盖**：明细预算 2048 行，411 条钉住 entry 中只打印了 335 条（计数器完整、明细截断是可预注册的报告限制）；E2/E3 未做；"多线程采样由哪个调度机制派发"留作静态跟进（E2 范围）。

<a id="c-g5-e1-v69-run1"></a>
### v69 首轮（修复版首次上机，2026-11-04 23:05，无转段，Q2 未检验）

**工件与设置**：v69 DLL `2490c38d…da11e`（1,590,272 字节，[干净重建](build.md#v69-g5e1-clean-rebuild-2026-11-04)，身份 `…-v69-g5e1-evidence-repair-a89fa5cba0-…`，CodeView age 397）、PDB `f69c9790…`；运行 INI 9,640 字节 `c3703b74…`（concurrent 配置，20 键派生断言 + 写后回读，[部署记录](build.md#v69-g5e1-clean-rebuild-2026-11-04)）；日志 [323,130 字节 `f00960a7…`](diagnostics/CharacterPanel-g5e1-v69-repair-run1-20261104-2307.log)。全程零 `[E]`。

**过程**：23:04:11.818 两条 detour 按预注册头字节安装（63589 `head-size=5`、63856 `head-size=7`，thunk `0x7ff6de250000/0x7ff6de250100`——新 256 字节布局，original 块在 +128/+0x180）；23:04:43.206 读档 `invalidate-requested reason=world-reset`（窗口未开）；23:05:37.167 `concurrent-armed actor=00000014 epoch=1 graph=0x1e5a3f30680 behavior=0x1e0f4c4d900 pinned=yes`；**23:05:57.167 到期关闭（20.000 s，1040 帧，`reason=expired`）——面板显示窗口从未关闭，没有发生 `display-close`，无 `window=segment` 转段行，直接以到期收尾**。

**关闭行（原文）**：
`window=close reason=expired actor=00000014 epoch=1 frames=1040 entries=1314 e:engine=0/plugin=1314/other=0 pinned-engine=0 apply-entries=1314 pinned-apply-entries=0 other-apply-entries=1314 tracked-sample-entries=1314 paired-apply-entries=1314 pair-mismatch=0 pair-no-slot=0 threads=1 queue-seen=yes queue=0x7ff75fbbe0e0 flag=0 queue-flag-frames=0 detail=2048 detail-dropped=0 ring-dropped=0 contention-dropped=0 full-dropped=0 eligible-records=2628 recorded-records=2628 drained-records=2628 drained-samples=1314 drained-applies=1314 missing-records=0 orphan-applies=0 unpaired-samples=0 suppressed-rows=580 transport-complete=yes`

**逐项判读**：

| 项 | 结果 | 依据 |
| --- | --- | --- |
| **安装（P1 前半）** | **通过** | 两条 detour 头字节相符、thunk/original 地址齐全（修复版共享 256 字节块布局首次实机生效） |
| **新对账恒等式** | **通过（全零缺口）** | `eligible(2628) = recorded(2628) + ring/contention/full-dropped(0)`；`recorded(2628) = drained(2628) + missing(0)`；`drained-samples(1314) + drained-applies(1314)`；`orphan-applies=0`、`unpaired-samples=0`、`pair-mismatch=0`、`pair-no-slot=0`；`transport-complete=yes`——**audit31 要求的传送/关闭对账在其覆盖范围内成立**（注意 `transport-complete` 只描述传送，按裁定不构成 E1 或 G5 通过） |
| **插除旧案** | **不复现** | v68 的 1,100 入口/1,099 对缺口本轮消失：1,314 对 1,314（`tracked-sample-entries = paired-apply-entries = 1314`），关闭前无在途记录残留 |
| **P2 源命中（Q2）** | **未检验（预注册无效分支）** | v2 规则："`display-close` 没有在 display 段内发生 → 没有 resumed 段，本窗口对 Q2 无效——只能按首轮的方式记'未检验'"。`pinned-engine=0`、无转段行、arm 后恰 20.000 s 到期，属设计内覆盖不到，**不是负结果，也不推翻 v68 已取得的 411 命中** |
| **P3 配对** | 通过（全窗） | 1,314 对全部配对、零错配/零孤儿；2048 行明细内 sample→apply 逐对相邻，全部 `graph=0x1e644a5a200`（候选图）、`requested-count=116` |
| **P4 线程** | 与 v67 一致 | 明细单 tid（渲染线程 18560）；钉住源图无引擎记录，不涉 Q3 |
| **P5 队列** | 通过 | `queue-seen=yes`、`flag=0`、`queue-flag-frames=0`（1040/1040 帧） |

**明细字段（新接口首次实机）**：120 条 `caller=0xe6012492` + 904 条 `caller=0xe60120b6`——两个插件内返回地址 = 插件自己的两个直调点（正控成立，插件调用区分有效）；`suppressed-rows=580` = 2,628 条记录 − 2,048 行打印预算，与 `detail=2048` 自洽（计数完整、打印截断，可预注册的报告限制照旧）。

**过程事实**：面板打开于 23:05:37.179（`snapshot=paused-source`），整个 20 秒窗口面板保持打开（`SCOPY COMPOSITE` 持续到 23:07:08，`draws` 累计到 4800+）；`draws=1/600/1200/…/4800` 每 ~11.46 s 累计 600——显示循环稳定。关闭行之后（23:07:14.766）面板关闭，`DAC restore … epoch=2 … result=matched`，`ANIMATION … lifetime reset`——正常关闭路径，零错误。

**结论与下一步**：修复版的跳板布局、原子门、接收封口、尾部 drain 和关闭对账在实机上按设计工作（v68 的配对缺口消失，恒等式全零缺口通过）；但本轮窗口结构未覆盖 resumed 段——**要关闭修复版的 E1 取证缺口，需要一次"开面板 → 关面板（触发转段）→ 留 10 秒游玩 → 等关闭行"的重跑**（同 v69 二进制零改动，只改操作时序；一次进程一个窗口，重跑需重启游戏）。之后再排 G4-C 修复仪器的正控重跑。

<a id="c-g5-e1-v69-run2"></a>
### v69 第二轮（修复版两段窗口完整，2026-11-04 23:15，resumed 段源命中重现）

**工件与设置**：与[首轮](#c-g5-e1-v69-run1)完全相同（v69 DLL `2490c38d…`、PDB `f69c9790…`、INI `c3703b74…`、零代码/INI 改动，仅重启游戏）；日志 [261,457 字节 `ddede89b…`](diagnostics/CharacterPanel-g5e1-v69-repair-run2-resumed-20261104-2315.log)，零 `[E]`。

**过程**：23:14:12.143 两条 detour 安装（头字节相符）；23:14:30.189 读档 `invalidate`；23:15:23.983 arm（`graph=0x257a4832480 behavior=0x252f3ac1900 pinned=yes`，与实验 `INDEPENDENT GRAPH phase=begin mode=inject` 同帧）；**23:15:28.432 转段**（display 段 4.449 s / 225 帧，380 条插件 entry 后首次 `display-close`），`window=segment provisional=yes from=display to=resumed seconds=10`；23:15:38.437 到期关闭（resumed 段 10.005 s）。

**关闭行（原文）**：
`window=close reason=expired actor=00000014 epoch=1 frames=632 entries=89025 e:engine=88645/plugin=380/other=0 pinned-engine=408 apply-entries=85843 pinned-apply-entries=408 other-apply-entries=85435 tracked-sample-entries=788 paired-apply-entries=788 pair-mismatch=0 pair-no-slot=0 threads=7 queue-seen=yes queue=0x7ff75fbbe0e0 flag=0 queue-flag-frames=0 detail=1576 detail-dropped=0 ring-dropped=0 contention-dropped=0 full-dropped=0 eligible-records=1576 recorded-records=1576 drained-records=1576 drained-samples=788 drained-applies=788 missing-records=0 orphan-applies=0 unpaired-samples=0 suppressed-rows=0 transport-complete=yes`

**逐条判据**（按 [C-G5-E1 契约](#c-g5-e1-probe)第五节 + [v2 补充读法](#c-g5-e1-v2)）：

| 判据 | 结果 | 依据 |
| --- | --- | --- |
| **P1 安装与存活** | **通过** | 头字节按预注册安装；两段均有帧（225 + 407）与记录；全零 drop |
| **P2 源命中（Q2，resumed 段）** | **通过** | 转段行 `pinned-engine=0` → 关闭行 `pinned-engine=408`，**resumed 增量 408 > 0**；`pinned-apply-entries` 同步 0→408；正控 `e:plugin=380`（display 段全部插件调用） |
| **P3 配对完整性** | **通过（全窗 + 明细复核）** | 关闭行 `tracked=paired=788`、零错配/零缺槽；明细逐对审计：408 条钉住 sample 之后全部同 tid 相邻一条 apply（`caller=0xba3f6e`、`num240=116`、`requested-count=116`），0 违例 |
| **P4 线程唯一性** | **反证复现（与 v68 一致）** | 钉住图 408 对落在 **6 个工作线程**（5868/33352/31792/30228/34592/8212，各 120–160 条），轮换频率 ~20 ms；"单帧线程"假设维持被推翻 |
| **P5 队列状态** | **通过** | 两段 `flag=0`、`queue-flag-frames=0`，`q=0x7ff75fbbe0e0` 稳定 |
| **对账恒等式（audit31 新判据）** | **通过** | `eligible(1576) = recorded(1576) + dropped(0)`；`recorded(1576) = drained(1576) + missing(0)`；`orphan/unpaired/mismatch/no-slot` 全 0；`suppressed-rows=0`（本轮明细 1,576 行全部打印，预算内）；`transport-complete=yes`（仅传送语义） |

**与 v68 轮的对照**（同一窗口结构，修复前 vs 修复后）：入口/应用计数同量级（89,278→88,645 引擎、411→408 钉住）、六工作线程复现（tid 不同属进程差异）、队列读数相同——**入口活性结论在修复版下完整重现**；差异只在记账侧：v68 的 1,100/1,099 配对缺口与"detail=2048 截断"消失，本轮 `tracked=paired=788` 全配对、明细 1,576 行零抑制（1,314+408 条记录全部可见：380 插件对 + 408 钉住对 = 788 对 = 1,576 行）。v68 当时无法解释的"差一对"，修复版以全零缺口的关闭对账给出答案：记录与传送均完整，缺口源于旧版关闭时未 drain 的在途记录。

**修复版 E1 取证缺口据此闭合**：入口活性（P1/P2）、配对（P3，含新对账恒等式）、线程反证（P4）、队列（P5）在修复仪器上全部成立。仍不构成 E1 完整收官：63856 应用入口 ≠ 输出完成（audit31），完成点/寿命/同代快照仍属后续原生审计；E2/E3 未开始；门 1 维持 NOT_READY。

**下一步**：~~G4-C 修复仪器正控重跑~~（**已完成**：[attempt 2](#c-g4-c1-concurrent-events) 判据 1–6 全成立，2026-11-05）；此后按排序进入 [v3 契约](#c-g5-e1-v3) 的实现评审与 E2/E3。



<a id="c-g5-e1-v3"></a>
### v3 修订（2026-11-05 预注册；v70 首轮 R2 硬停、v71 修复后 **R1–R7 全部满足**）

**动机**：[audit32](behavior-graph-construction-audit.md#audit32) 闭合了 E1 的静态四项，同时改变了三件事的取证方式：① 合法完成捕获窗有**两处**（63589 尾 `0xbb2b06` 与 63568 尾 `0xbaebe7`），不是一处——生成路径在 63568 内完成并直接返回；② 63856 全镜像有 **5 个**直接调用点，除两处主路径外还有 63563（物理回调，经 `VTABLE___GraphPhysicsStepListener` 槽 2）、63572、`0xbae731`（无 .pdata），后三者若在会话内出现必须单独归因；③ 实际写 poseLocal 的 n 按路径三分，`[g+0x240]` 只代表主路径。因此 v1/v2 的"63856 入口命中"只能证明**应用开始**，不能证明完成，也不能把不同路径混入同一次配对。**v3 只扩展观测点与计数，不改窗口语义，也不重判 P1–P5**（它们已由 v69 两轮成立）。

**一、问题 → 观测点 → 通过判据**

| # | 问题 | 观测点 | 通过判据 | 无效 / 反证条件 |
| --- | --- | --- | --- | --- |
| R1 | 完成点：63856 的应用是否在返回时完成；两条路径的配对各成一体 | 63856 入口（既有 `0xbc2fe0`）+ **返回侧插桩**（候选 A：63856 尾部返回点；候选 B：两处调用后指令） | **`63568-tail`（生成路径）在 resumed 段 ≥1 次 `entry→completion` 配对**——源图运行时 `+0x246` 每帧为 1（v68/v69 实机：钉住记录全 `flag246=1`），该路径结构性必然出现；`63589-tail`（非生成路径）**若出现**则配对完整，**若为 0 记"该路径本窗口未触发"，不判失败**（`+0x246` 由 63587 每帧判定，两侧不假定同帧一致）。两者均要求 `completion.tsc − entry.tsc ≥ 0` 且同 tid | `63568-tail` 有 entry 无 completion（同一调用无返回记录）⇒ 该窗口完成证据无效；`completion` 先于 `entry` 或跨 tid ⇒ 该窗口配对无效；`63589-tail` 为 0 不构成无效 |
| R2 | 路径归属：五个调用点是否被正确分类，63563/63572/`0xbae731` 是否触发 | 63856 入口的 caller 返回地址分类（`0xbb2b01`/`0xbaebe2`/`0xbae395`/`0xbaf50f`/`0xbae731`），逐路径计数器 | `calls = Σ 各路径` 对账相等；任一路径 >0 必须给出 graph/tid/时刻与是否与主路径同帧 | `unclassified > 0` ⇒ 停止并上报，不得归入"其它"后继续 |
| R3 | 有效输出范围：`[g+0x240]` 与实际写入的关系是否按路径三分成立 | 63856 入口的 `count` 入参 + `word[g+0x240]` + 槽 `numData` + 节点表容量 `[[g+0x160]+0x10]` | 主路径 `count == word[g+0x240]`；fallback 路径 `count == 槽 numData` 且该路径**不改写** `[g+0x240]`（窗口前后相等）；`count ≤ 节点表容量` 恒成立 | `count > 容量` ⇒ 立即停止（引擎越界发现）；主路径 `count != [g+0x240]` ⇒ 该记录作废并上报 |
| R4 | 寿命：poseLocal 是否单点分配、窗口内指针不变 | 63856 入口的 `pose` 指针与 `[char+0x90]` | 同一 graph 的 `pose` 指针在窗口内恒定，且与 `[char+0x90]` 一致 | 指针变化 ⇒ audit32"单点 lazy 分配"被推翻，停止并上报（不得当作正常重分配） |
| R5 | `fill ≤ cap`：reference 填充计数是否可能超容量（引擎自身越界风险） | 窗口 open/close 读 `skeletonWord+0x30`（cap）与 `+0x40`（fill） | 所有采样点 `fill ≤ cap` | 违反 ⇒ **停止**并作为发现上报（不是本轮通过/失败，是引擎缺陷线索） |
| R6 | 同代快照：跨线程读的时序风险可量化 | 每记录 tsc + tid；render 线程读取时的在途计数 | 报出在途窗口最大值与分布、`in-flight > 0` 的读取比例 | 该比例 >0 **不是失败**（audit29 已声明读者无屏障），但必须报出，且不得据此声称"原子快照" |
| R7 | epoch：跨 epoch 记录是否被隔离 | 沿用 v1 的 `PreLoadGame/NewGame` invalidate + `source generation` | epoch 变化后计数归零、旧记录不得与新记录配对 | 出现跨 epoch 配对 ⇒ 该窗口无效 |

**二、观测点（新增插桩的选点纪律）**

- 入口点不变：63856 `0xbc2fe0`（head `48 81 EC 88 00 00 00`，7 字节补丁）。
- **返回侧两候选**（实现评审二选一并记明选择与核验字节）：
  - **A**：63856 尾部返回点。须先枚举 `[0xbc2fe0,0xbc3237)` 内全部 `ret`/`jmp` 出口并证明应用完成必经同一出口；出口不唯一且无法证明 ⇒ 弃用 A。
  - **B（备选）**：两处调用后指令——63589 `0xbb2b06`（`90` nop，其后为 TLS 恢复）与 63568 `0xbaebe7`（`4c 8d 9c 24 30 01 00 00`，8 字节 lea，需 5 字节 jmp + 3 字节原位）。B 天然区分路径，但必须保证还原精确。
- 五个调用点**不新增 hook**：用 63856 入口的 caller 返回地址分类（v1 已用该手法区分 `0xba3f6e` 引擎本地循环与插件直调）。
- 插桩仍守 v1 §三纪律：记录函数内不分配、不加锁、不调引擎、不读字符串、不打印；tid 用 `__readgsqword(0x48)`；tsc 只作同线程序关系。

**三、记录与计数**

- 计数器（原子递增）：`entries`、`completions`、`pairs`、`unpairable`、`unclassified`、`in_flight_max`，并按路径分列 `main_63589 / main_63568 / p63563 / p63572 / p_bae731` 各一组 entry/completion。
- 明细环：`{seq, slot(entry|completion), tid, graph, pose, path, count, num240, slotNumData, nodeCap, tsc}`；入环条件沿用 v1（pinned 或插件模块内）；环深 4,096；明细丢失不改计数，但使该窗口的 R1/R6 证据无效。
- 关闭行新增：`entries/completions/pairs/unpairable/unclassified/in-flight-max` + 五路径分列 + `pose-stable=yes|no` + `fill<=cap=yes|no|violation`；`transport-complete` 及 v1/v2 的既有对账字段全部沿用。

**四、窗口生命周期**（不变）：`SamplingLiveness=concurrent`，arm 早于两次派发，首次 `display-close` 转段 10 秒，到期/第二次关闭/世界重置收尾；一次进程一个窗口；不与 SourceTimeline/SourceEventCapture/SourceWriteCapture 叠加；头字节不符不安装。

**五、判据（预注册，不得事后放宽）**：R1–R7 如上。任何"通过"必须同时满足：安装字节核验、`calls` 对账相等、`unclassified=0`、`pose-stable=yes`、`fill<=cap ≠ violation`、无跨 epoch 配对，**且 `63568-tail` 至少一次完整 `entry→completion` 配对**。**负结果定义**：窗口完整（两段、无 drop、正控成立）而 `63568-tail` 的 entry 全部无 completion，方可写"该窗口未观察到完成"，且只限所观察路径；`63589-tail` 为 0 只写"该路径本窗口未触发"。

**六、停点与回传**：返回点选点不符、`unclassified>0`、`count>容量`、`pose` 变化、`fill>cap`、跨 epoch 配对、明细丢失影响 R1/R6 ⇒ 停止并保持 PENDING；不得现场放宽判据或改选点。回传须含：选点与字节核验、安装/arm/segment/close 四行、R1–R7 逐项读数、五路径分列、`in-flight` 分布、匹配 DLL/PDB/INI 身份、完整日志（不得只给摘要）。

**七、实现影响面（评审用，本节不授权实现）**

| 项 | 内容 |
| --- | --- |
| 源码 | `probe_liveness_policy.h`（路径枚举、计数器与对账字段）、`probe_liveness.{h,cpp}`（返回侧插桩、caller 分类、关闭行字段） |
| 构建 | `PLUGIN_SOURCES` 不变（33）；无新 target |
| 设置 | 无新 INI 键；沿用 `SamplingLiveness=concurrent` |
| 互斥 | 同 v1 §七（三路 detour 仪器不叠加） |
| 原生调用 | 只新增返回点 detour（或 B 的两处），**不加 native 写、不改产品路径** |
| 离线回归 | 路径分类（五个 caller RVA）、对账恒等式、`pose` 变化判停、`count>cap` 判停、`unclassified` 判停、B 方案的跨指令还原 |
| 文档同步 | 本节 + [audit32 §五](behavior-graph-construction-audit.md#audit32) 交叉引用 + README/CHANGELOG + G5 行 |

**八、明确不做**：不做 E2 贡献账本（层/权重/通道）；不做 E3 配对 manifest；不采集姿态数值、不冻结容差；不钩 58343/63666/63582/59303；不改窗口语义；**不把本契约的通过当作门 1 关闭**（门 1 仍需 E2、E3 与联合验收）。

**九、实现评审前置事实（2026-11-05，离线只读，未实现）**

- **63856 出口唯一**：`[0xbc2fe0,0xbc3237)` 共 599 字节，唯一 `ret` 在 `0xbc3236`；`0xbc3086` 的 `jmp 0x140bc308c` 是函数体内跳转，不是出口。**候选 A 结构可行**，不再需要 B。
- **早退仍经同一 ret**：`0xbc2ffc` 的 `jle 0x140bc322f`（`clamped count ≤ 0` 时跳过全部节点写）直接落到 `0xbc322f: add rsp,0x88` → `0xbc3236: ret`。因此 completion 记录必须携带 **clamped count** 才能区分"应用完成"与"空应用"；该值可在**入口**按 audit31 的同一公式只读复算（`requested<0 ? cap : min(requested, cap)`，`cap = dword[rdx+0x10]`），不必在返回点读易失寄存器。
- **插桩窗口**：`0xbc322f` 的 `48 81 c4 88 00 00 00`（`add rsp,0x88`，7 字节）——5 字节 `jmp` + 其后 2 字节 `nop`，跳板重放该 7 字节指令后 `jmp 0xbc3236`。跳板内调用记录器前须保存/恢复 `rsp`（该点栈帧已收至 63856 入口态，`add rsp,0x88` 会改变 `rsp`），并在跳板尾部保持 `rsp ≡ 0 (mod 16)` 于 `call` 前。
- **补丁窗口安全性已核**：63856 内部全部跳转目标为 `{0xbc2ff7, 0xbc2ffa, 0xbc3050, 0xbc3088, 0xbc308c, 0xbc31f0, 0xbc322f}`，**没有目标落在窗口内部**（唯一跳入点是窗口起点 `0xbc322f`，来自 `0xbc2ffc: jle`）。窗口字节 `48 81 c4 88 00 00 00 c3`：5 字节 `jmp` 覆盖 `0xbc322f..0xbc3233`，残余 `0xbc3234..0xbc3235` 置 `nop nop`。
- **栈序已按实现核实**：返回点 `rsp ≡ 8 (mod 16)`（入口 `sub rsp,0x88` 之后），返回跳板以 `sub rsp,0x28` 对齐后 `call` 记录器，返回后 `add rsp,0x28`、重放 relocated `add rsp,0x88`、再 `jmp 0xbc3236` 的 `ret`；volatile 寄存器与 flags 不保存（返回点之后无人读取），`rbx/rsi/rdi/rbp/r12-r15` 由记录器自身按调用约定保存。

**十、实现与离线回归（2026-11-05，提交 `757b01481b`，v70 身份见 [build.md](build.md#v70-g5e1-v3-2026-11-05)）**

- 三个 detour 块：63589 入口、63856 入口、**63856 返回**（`+0x24f`）。池常量改为 probe 私有的 3×256（共享 `Detour_Hook_Count` 仍为 2，typed-setter 探针不受影响）。
- 应用入口 thunk 改为四参（`rcx/rdx/r8` + `r9` = 调用者返回地址，取 `[rsp+0x68]`，与既有 sample thunk 的返回地址槽同源）；`classify_apply_path` 把五个 caller RVA 分入五条路径，未命中即 `unclassified`（R2 硬停条件）。
- 返回记录器消费 per-tid in-flight 表（`ProbeInFlight`）：同线程配对由结构保证，覆盖与孤儿各自计数；关闭行新增 `in-flight-now/max`、`overwritten-entries`、`orphan-returns` 等字段供 R6 读。
- 范围事实在入口只读采集：`cap = [g+0x170]`、`slot numData = [[g+0x220]+0x32]`、`clamped = requested<0 ? cap : min(requested, cap)`；`pose` 指针窗口内变化即置 `pose-stable=no`（R4 反证）；`skeleton[+0x30/+0x40]` 在 arm 时读一次，`fill>cap` 记 `violation`（R5 停点）。
- **离线回归**：新增 6 例通过（路径分类、clamp 形状、in-flight 配对/覆盖/孤儿、两个新 thunk 以生产字节执行——入口变体断言第四寄存器为调用者返回地址，返回变体断言记录器被调用且 `sub/add rsp,0x88` 栈配对往返正确）；既有 repair 2 例、legacy 8 例不变通过。Release 构建 exit 0。
- **已部署（2026-11-05 21:4x，v70 DLL `cd57895d…`/PDB `b03080ca…`、运行 INI `c3703b74…` 9,640 B，v69 三件套备份 `dist/deploy-backup-20261105-v69-v70pre/`，目的地九门键回读正确），未上机**：一次两段窗口（开面板 → 关面板转段 → 10 秒 → 关闭行）后按 R1–R7 判读；门 1 不变。

**十一、首轮运行（v70，2026-11-05 21:52）——R2 硬停：仪器缺陷四处，判据全部不通过；返回侧完成边界已获零丢失证据**

工件：v70 DLL `cd57895d…`、运行 INI `c3703b74…`、日志 [377,591 字节 `e07e71e8…`](diagnostics/CharacterPanel-g5e1-v70-v3-run1-20261105-2152.log)，零 `[E]`。21:51:38 三处安装成功（63589、63856 入口、**63856 返回 `0x7ff7603f322f` thunk `+0x200`/original `+0x280`**）；21:52:28.309 arm（`pinned=yes`）；21:52:33.692 转段（display 178 帧、436 插件 entry）；21:52:43.712 到期关闭（560 帧）。

关闭行要点：`pinned-engine=383`、`apply-entries=81034`、`pinned-apply-entries=383`、**`apply-returns=81034`（与 apply-entries 逐一对齐）**、**`completions-pinned=383`（与 pinned-apply-entries 相等）**、`pose-stable=yes`、`fill-le-cap=ok`、`skeleton=[cap:116 fill:116]`、零 transport 缺口、`orphan-applies=0`。

| 判据 | 读数 | 裁定 |
| --- | --- | --- |
| R1 完成点 | 返回侧对**每一次** 63856 调用都触发（81,034/81,034），源图 383/383 同线程配对；但路径归属全部未分类 | **不通过（路径未证）**；完成边界本身获得零丢失证据 |
| R2 路径归属 | `apply-paths=…/un:81034`，`unclassified=81034` | **硬停**——按预注册，本轮无效 |
| R3 输出范围 | `count-over-capacity=436`、`clamped-zero=436` 全部来自非钉住图 | 不通过（见缺陷 2） |
| R4 寿命 | `pose-stable=yes`（383 次钉住应用同一 pose 指针） | **通过（限定钉住源图）** |
| R5 fill≤cap | `fill=116 cap=116` | **通过（限定）：reference 填充恰等于容量，未见越界** |
| R6 同线程在途 | `in-flight-max=0`（采样点缺陷） | 无效（见缺陷 4） |
| R7 epoch | 无跨 epoch 配对 | 通过 |

**四处仪器缺陷（全部代码侧，非引擎事实）**：

1. **分类用错地址形态**：`classify_apply_path` 匹配 audit32 表里的 **call 指令地址**（`0xbb2b01`/`0xbaebe2`/…），而 63856 在栈上看到的是**返回地址 = call+5**（`0xbb2b06`/`0xbaebe7`/…）。同日志 `sample-entry caller=0xba3f6e` 正是这种 +5 形式，是直接旁证。修复：改匹配返回地址，并在回归里把 call 地址作为**负例**锁死。
2. **范围事实越界采集**：候选/克隆图的 `[g+0x170]` 为 0、`[g+0x220]` 未初始化，导致 436 条假 `count-over-capacity`/`clamped-zero` 与垃圾 `slot-num-data`（29577/29523）。audit32 的节点表结论只对源图成立。修复：capacity/slot/pose 只在 `is_pinned` 时采集。
3. **orphan 语义**：非钉住图的返回（80651）本不该计 orphan——它们从未登记 in-flight。修复：`ever_tracked(tid)` 分流为 `orphan-returns`（曾持有 tracked entry）与 `untracked-returns`。
4. **in-flight 高水位采样点**：在返回后读取恒为 0。修复：改在 `on_entry` 后更新。

**另加诊断能力**：`apply-entry` 明细行现在打印 `path=` 与 `caller={:#x}`（未分类时可见原始 RVA，不再需要靠推断）；关闭行新增 `apply-completions=63589:…/63568:…/…` 每路径完成数与 `untracked-returns`。

**修复后身份**：v71（`0327e15fa3`，DLL `cbeba8a2…c722f4` 1,633,792 字节，PDB `16a29f59…`，CodeView age 400），6+2+8 例离线通过，[构建记录](build.md#v71-g5e1-v3-fix1-2026-11-06)。**下一轮按同一 R1–R7 判读；本轮不改变门 1。**

**十二、第二轮运行（v71，2026-11-06 22:23）—— R1–R7 全部满足（限定范围）：v3 通过**

工件：v71 DLL `cbeba8a2…`、运行 INI `c3703b74…`、日志 [391,487 字节 `2a64e0a3…`](diagnostics/CharacterPanel-g5e1-v71-v3-run2-20261106-2223.log)，零 `[E]`。22:22:22.733 三钩子安装（返回点 `0x7ff7825f322f`、thunk `+0x200`/original `+0x280`）；22:22:45.996 invalidate；22:23:07.995 arm；22:23:15.151 转段（display 273 帧、543 插件 entry）；22:23:25.155 关闭（696 帧、resumed 10.004 s；显示段 `semantic-changed=120/120`、`source-drift=none`、`DAC restore matched`）。

关闭行（原文）：
`window=close reason=expired actor=00000014 epoch=1 frames=696 entries=92612 e:engine=92069/plugin=543/other=0 pinned-engine=424 apply-entries=89437 pinned-apply-entries=424 other-apply-entries=89013 tracked-sample-entries=967 paired-apply-entries=967 pair-mismatch=0 pair-no-slot=0 threads=7 queue-seen=yes queue=0x7ff784bee0e0 flag=0 queue-flag-frames=0 detail=2048 detail-dropped=0 ring-dropped=0 contention-dropped=0 full-dropped=0 eligible-records=2358 recorded-records=2358 drained-records=2358 drained-samples=967 drained-applies=967 missing-records=0 orphan-applies=0 unpaired-samples=0 suppressed-rows=310 transport-complete=yes apply-paths=63589:1356/63568:77481/p63563:10600/p63572:0/p_bae731:0/un:0 apply-completions=63589:0/63568:424/p63563:0/p63572:0/p_bae731:0/un:0 apply-returns=89437 completions-pinned=424 paired-completions=424 drained-returns=424 in-flight-now=0 in-flight-max=1 orphan-returns=65195 untracked-returns=23818 overwritten-entries=0 count-over-capacity=0 clamped-zero=0 pose-stable=yes fill-le-cap=ok skeleton=[cap:116 fill:116]`

| 判据 | 读数 | 裁定 |
| --- | --- | --- |
| **R1 完成点** | `apply-completions=63568:424` = `completions-pinned` = `pinned-apply-entries` = **424**，`in-flight-now=0`；pinned 的 63589-tail 为 0（源图每帧 `flag246=1`，结构上走生成路径） | **通过（生成路径）**；63589-tail 记"未覆盖"，不失败 |
| **R2 路径归属** | `apply-paths=63589:1356/63568:77481/p63563:10600/p63572:0/p_bae731:0/un:0`，Σ=**89,437** = `apply-entries`；明细 `caller=0xbb2b06/0xbaebe7` 与路径一一对应 | **通过**（`unclassified=0`、对账相等） |
| **R3 有效输出范围** | `count-over-capacity=0`、`clamped-zero=0`；pinned 明细 `requested=116 clamped=116 capacity=116` | **通过（主路径）**；fallback 子判据无 pinned 样本 → 记"未覆盖" |
| **R4 寿命** | `pose-stable=yes`（424 次钉住应用同一 pose 指针） | **通过** |
| **R5 fill≤cap** | `fill-le-cap=ok`、`skeleton=[cap:116 fill:116]` | **通过**（reference 填充恰等于容量，全程未见越界） |
| **R6 同线程在途** | `in-flight-max=1`、`in-flight-now=0`；钉住 424 / 全部 89,437 | **通过**：在途窗口恒为 1，跨线程读取风险已量化而非被否定 |
| **R7 epoch** | 无跨 epoch 配对 | **通过** |

**另有首次运行证据**：`p63563=10,600` —— [audit32](behavior-graph-construction-audit.md#audit32) 归因的**物理回调路径**（63563，仅经 `VTABLE___GraphPhysicsStepListener` 槽 2 进入）在本会话确实触发，且被正确归入自己的路径桶（未混入主路径）；`p63572` 与 `p_bae731` 为 0。

**不证明 / 限定**：

- 完成点证据只覆盖 **pinned 源图的生成路径**（63568-tail，424 次）；非生成路径（63589-tail）在 pinned 层面未出现，**不外推**到"任何图任何路径的完成点"。
- `slot-num-data` 字段对源图读出 29,513 / 17,796（非 count 量级），说明该偏移的读法未对齐 audit29 的槽结构描述；**本字段不参与任何判据**，记为已知读数缺陷，下一版须先核对偏移或删除。
- `orphan-returns=65,195` 是"曾经持有 tracked entry 的线程上、未登记的非 pinned 返回"（in-flight 只对 pinned 登记），**不是丢配**；pinned 配对完整性由 `424/424` 独立证明。
- `count-over-capacity=0` 只覆盖 pinned 图（v71 修复 2 的限定）；其它角色/克隆图的表未初始化，不采集也不外推。
- 门 1 仍 **NOT_READY**：E2/E3 与联合验收未做，本通过只关闭"修复版仪器的完成点/范围/寿命/快照运行侧取证"这一子项。

<a id="c-g5-e2"></a>
## C-G5-E2：完整贡献账本（E2-A 合成恒等式，2026-11-06 预注册）

**范围**：G5 的"完整姿态对照"要求比较**全部有效贡献层、权重、混合参数、播放速度及最终骨骼变换**（[G5 定义](#六道关卡)）。E2 是这一要求的账本部分，分三级：**E2-A** 合成恒等式（本轮）、**E2-B** 层与权重枚举（含 63666 输出选择与 59303 双槽非轨道混合）、**E2-C** 播放速度与时钟账本。本契约只预注册 **E2-A**。

**一、问题**

poseLocal 的每条轨道是否等于"槽1 与槽0 按 `[g+0x230]` 的混合"？若是，则最终姿态的每个分量都可分解到两个槽与一个权重，混合参数这一项即可被逐分量验证；若不是，必须先找出改写者（IK 后处理、`[g+0x148]` 覆盖、fallback、63563 路径）再谈层枚举。

**二、离线核对（2026-11-06，先于契约定稿，防反模式 12）**

- `63582`（`0xbb0320`）读 `n = min(槽1.numData, 槽0.numData, numAnim)`，随后 `64112(count=n, A=槽1.tracks, B=槽0.tracks, _=out, out=out.tracks, alpha=xmm3)`。
- `64112`（`0xbd52d0`）每轨 0x30 字节，只处理**前两段**：`trans(+0x00) = (1−w)·A + w·B`、`rot(+0x10) = normalize((1−w)·A + w·B′)`（`dot(A,B)<0` 时 `B′ = −B`；归一化用 `rsqrtss` + 一次牛顿迭代，零长度时结果为零向量）；**scale(+0x20) 不参与混合**（留给 E2-B）。
- 传入的 `w` 是 `[g+0x230]`（63581 的 `1−alpha`），**不是** 63581 内部的 `alpha = max/(max−min)`。
- 63581 的 `[g+0x148]` 覆盖落在 **payload**（`out+word[out+0x14]`）而 memcpy 源是 **tracks**（`out+word[out+0x34]`），因此主路径下二者的先后不影响 poseLocal。
- fallback 分支（`0xbb0106`）显式把 `[g+0x230]` 置 `1.0f`，于是同一公式退化为 `poseLocal = 槽0`——**无需为 fallback 追加例外**。

**三、观测点（零新增 detour）**

沿用 v3 已有的 **63856 入口** hook（`0xbc2fe0`）：该点 `rcx=poseLocal`、`rdx=&g+0x160`、`r8d=count`，且此时 `[g+0x230]`（w）与 `[g+0x220]/[g+0x228]`（槽容器）仍是本次调用的输入，63587 尚未交换槽。一次只读采集即可拿到全部输入与输出：

- `w = float[g+0x230]`；`槽0 = *[g+0x220]`、`槽1 = *[g+0x228]`；`tracks = 槽 + sx(word[槽+0x34])`；`numData = word[槽+0x32]`（仅当 `dword[槽+4] > 2`，与 63582 同门）。
- `poseLocal` 轨道数组 = `rcx`（63856 的 `r9 = rcx+0x18` 只用于读，轨道基址即 `rcx`）。
- 复算与比对在 detour 内完成（无分配、无锁、无引擎调用），日志只输出**每帧一行汇总**加**前 4 条失败样本**，避免 400 帧 × 116 轨全量转储。

**四、判据（预注册，不得事后放宽）**

| # | 判据 | 通过条件 |
| --- | --- | --- |
| **A1 采样完整** | 逐帧记录 `n`、`w`、两槽 `numData`、两槽与 poseLocal 的有效性；任一读取失败或 `n > 节点表容量` 即该帧无效 |
| **A2 trans 恒等式** | 每条轨道 `i∈[0,n)`：`\|poseLocal.trans.xyz − ((1−w)·槽1.trans.xyz + w·槽0.trans.xyz)\| ≤ 2e-6`（分量绝对值；若任一分量 \|v\|>100 则该分量改用相对 1e-5） |
| **A3 rot 恒等式** | 每条轨道：`\|poseLocal.rot − normalize((1−w)·槽1.rot + w·槽0.rot′)\| ≤ 2e-6`，方向按 `dot<0 ⇒ 槽0 取反`；零长度按 64112 的零向量约定比对 |
| **A4 覆盖** | A2/A3 覆盖该帧**全部** `n` 条轨道，不得抽样；未比对轨道数必须为 0 |
| **A5 权重域** | `w ∈ [0,1]`（含端点）；`w` 越界即停并上报（可能是 63581 的 alpha 计算异常） |
| **A6 分路径报告** | 结果按 v3 的五个 apply 路径分列；主路径（`63589-tail`/`63568-tail`）单独给出比对帧数与失败数，`p63563` 等旁路单独统计，不得混算 |

**五、停点与回传**

非有限值、`n` 超容量、槽指针无效、`w` 越界、明细丢失影响 A4 ⇒ 停止并保持 PENDING；不得放宽阈值、不得抽样、不得把失败轨归入"已知例外"而不给证据。回传：逐帧汇总（帧号、路径、n、w、比对数、失败数、最大分量误差）、失败样本（轨索引、通道、两槽值、复算值、实测值、差值）、匹配 DLL/PDB/INI 身份与完整日志。

**六、实现影响面（评审用，本节不授权实现）**

| 项 | 内容 |
| --- | --- |
| 源码 | `probe_liveness_policy.h`（混元与复算的纯函数、阈值常量）、`probe_liveness.cpp`（63856 入口新增只读采集与每帧汇总行） |
| 构建 | `PLUGIN_SOURCES` 不变（33）；**不新增 detour、不新增原生调用或写入** |
| 设置 | 无新 INI 键；随 `SamplingLiveness=concurrent` 的同一窗口启用 |
| 互斥 | 与 v1/v3 相同（三路 detour 仪器不叠加） |
| 离线回归 | 混元复算（trans/rot、`dot<0` 取反、零长度、`w=0/1` 端点）、阈值边界、覆盖率计数器、`w` 越界判停 |
| 文档同步 | 本节 + audit29/32 交叉引用 + README/CHANGELOG + G5 行 |


**八、实现与部署（2026-11-06，提交 `5dd64a60d0`，v72 `bb27a87f…538577`）**

- 混元落为纯函数：`probe_mix_trans`（`(1−w)·A + w·B`，沿用引擎的结合顺序）、`probe_mix_rot`（`dot<0` 取反 + `rsqrtss` 等价归一化 + 零长度零向量）、`probe_mix_delta`（2e-6 绝对 / >100 单位时 rel 1e-5）、`probe_slot_view`（容器→槽→轨道数组，`+0x34` 有符号偏移，`dword[+4] > 2` 才取计数）、`probe_compare_track`（trans xyz = 通道 0–2、rot xyzw = 4–7）。
- 接线：既有 63856 入口记录器在 pinned 图内读 `[g+0x230]`、两槽轨道数组与 poseLocal，逐轨比对（不抽样），累加 `ProbeMixTally` 并每帧推一条 `k_mix_frame` 记录；渲染 drain 打印 `e2a` 行；关闭行新增 `e2a-frames/tracks/failed/max-delta/ok/w-domain/non-finite/read-failed/samples`。**零新增 detour、零原生写、零新 INI 键。**
- 离线：7 例 E2-A 通过（混元端点、`dot<0` 取反、零长度、100 单位阈值切换、tally 与 4 样本上限、显式手算恒等式与两处单通道篡改归因、槽视图合理性门）；既有 v3 6 例、repair 2 例、legacy 8 例不变通过。Release 构建 exit 0。
- **已部署**（v71 三件套备份 `dist/deploy-backup-20261106-v71-v72pre/`、运行 INI `c3703b74…`、七门键回读正确），未上机；一次两段窗口后按 A1–A6 判读 `e2a` 字段。

**九、首轮运行（v72，2026-11-07 22:24）—— E2-A 未产生比较（n=0），诊断缺失；E1 v3 判据再次复现**

工件：v72 DLL `bb27a87f…`、运行 INI `c3703b74…`、日志 [392,225 字节 `68ec7d18…`](diagnostics/CharacterPanel-g5e2-a-v72-run1-20261107-2224.log)，零 `[E]`。22:20:30 三钩子安装；22:24:23.693 arm；22:24:29.572 转段（display 234 帧、465 插件 entry）；22:24:39.578 关闭（609 帧、resumed 10.006 s）。

**E1 v3 复现（有效）**：`apply-paths=63589:1137/63568:69268/p63563:9400/p63572:0/p_bae731:0/un:0`（Σ=79,805=`apply-entries`）、`apply-completions=63568:376`=`completions-pinned`=`pinned-apply-entries`、`in-flight-max=1`、`count-over-capacity=0`、`pose-stable=yes`、`fill-le-cap=ok`、`skeleton=[cap:116 fill:116]`、`transport-complete=yes`、零错误 —— R1–R7 在第二个二进制上再次成立（`p63563=9,400` 亦复现）。

**E2-A 结果**：`e2a-frames=0 e2a-tracks=0 e2a-failed=0 e2a-ok=yes e2a-w-domain=ok e2a-non-finite=no e2a-read-failed=no e2a-samples=0`，且日志里 **0 条 `e2a` 明细行**。

**判读**：`is_pinned`（376 次）与 `clamped>0`（`clamped-zero=0`）都成立，说明块被执行；`slots_ok`/`weight_ok` 的失败标志均为否，说明 `probe_slot_view` 返回 `valid` 且权重在域内。因此 **`n = min(clamped, slot0.count, slot1.count) = 0`**，即两槽的轨道计数在 `dword[slot+4] > 2` 门下被判为 0。**但 v72 只在 `n>0` 时推 mix 记录，导致零轨迹时没有任何原始值可查**——这正是"零值必须带原始值"的教训在 E2-A 上的重演（与 v70 的 `unclassified` 同型）。

**修复（v73，提交 `5cb55a717e`，DLL `2cc3e365…`，CodeView age 403，[构建记录](build.md#v73-g5e2-a-2026-11-07)）**：

1. **每帧必记**：pinned 应用的 mix 帧无条件推记录，携带 `n`、`clamped`、`slot0.count`、`slot1.count`、`w`、`failed`、`max-delta`；`e2a` 行同时打印这些原始值。
2. **修掉 v1 遗留的一级槽读**：apply-entry 明细的 `slot-num-data` 之前用 `[[g+0x220]+0x32]`（容器 + 偏移，得到 29,513/17,796 之类垃圾），改为一律走 `probe_slot_view` 的两级读（容器 → 槽 → `+0x32`）。
3. 离线：e2a 7 例、v3 6 例、repair 2 例、legacy 8 例全部通过；Release exit 0。
4. 已部署（v72 三件套备份 `dist/deploy-backup-20261107-v72-v73pre/`、INI `c3703b74…`、七门键回读正确）。

**下一步**：一次同样操作的两段窗口，`e2a` 行会给出 `slot0/slot1` 的真实计数与 `n`。若仍为 0，则按原始计数定位（gate 值、`+0x32` 偏移语义或 `[g+0x220]` 的层级），不再靠推断；若 `n>0`，直接读 A1–A6。

**十、第二轮诊断（v73，2026-11-07 22:31）—— 零的原始值到手：全窗口都是 fallback 分支**

工件：v73 DLL `2cc3e365…`、日志 [393,202 字节 `bb22593e…`](diagnostics/CharacterPanel-g5e2-a-v73-run2-20261108.log)，零 `[E]`。E1 v3 再次复现（`apply-paths` Σ 75,940 且 `un=0`、`apply-completions=63568:362`=`completions-pinned`=`pinned-apply-entries`、`p63563=9,102`、`fill==cap==116`、`in-flight-max=1`）。

`e2a` 行给出原始值：**全部 276 条都是 `n=0 clamped=116 slot0=116 slot1=0 w=1.000000`**。据此：

1. **`n=0` 的直接原因**：v73 的实现无条件用 `n = min(clamped, 槽0.count, 槽1.count)`，而槽1 为空（count=0）→ n=0。这是**实现缺陷**：`w=1` 时槽1 不参与混合，不该限制 n。
2. **`w=1.000000` 的意义**：63581 只在 **fallback 分支**（双槽门 `max(tA,tB)≥0 && min(tA,tB)<0` 失败）写 `[g+0x230]=1.0f`（audit32 §二）。因此**整个 resumed 段源图每帧都走 fallback**，64112 的双槽 lerp/nlerp 一次也没有执行。
3. **由此得到的结构性事实**（新）：源图在稳定 idle 采样下**不产生双槽混合帧**；E2-A 的双槽恒等式（A2/A3 的核心）不能靠"正常开面板采样"覆盖，只能在槽交接瞬间（切换/过渡）才可能出现。

**修复（v74，提交 `22e234c2ea`，DLL `9a28389f…26cbfb`，CodeView age 404，[构建记录](build.md#v74-g5e2-a-2026-11-07)）**：

1. `probe_mix_track_count(clamped, w, 槽0.count, 槽1.count)`：**只按参与混合的槽取上限**——`w>0` 才受槽0 约束、`w<1` 才受槽1 约束；fallback（`w=1`）退化为 `min(clamped, 槽0.count)`。
2. `probe_mix_mode(w)`：`w∈{0,1}` 记 `fallback`，严格内部记 `main`；tally 分别累计 `main_frames`/`fallback_frames`，关闭行新增 `e2a-main-frames`/`e2a-fallback-frames`。
3. 槽可读性同样按参与性检查（`need0/need1`），不再因不参与的槽无效而判 `read-failed`。
4. 离线：e2a **8 例**（含权重感知计数与分支分类）+ v3 6 + repair 2 + legacy 8 全过；Release exit 0。
5. 已部署（v73 三件套备份 `dist/deploy-backup-20261108-v73-v74pre/`、INI `c3703b74…`、门键回读正确）。

**下一轮的读法（预注册）**：`e2a-fallback-frames` 应覆盖全部 276 帧左右，验证 "fallback ⇒ `poseLocal == 槽0`"（这是 A2/A3 在单槽分支上的实例）；`e2a-main-frames` 若仍为 0，则**必须在结论里写明"主路径双槽混合本窗口未覆盖"**，并另设计能触发槽交接的操作序列（例如面板内播放 equip/unequip 过渡、或在采样窗口内切换剪辑），不得把 fallback 通过当作双槽混合通过。

**十一、第三轮（v74，2026-11-07 22:39）—— 恒等式首次真正执行：2.04% 分量不符，A2/A3 不通过**

工件：v74 DLL `9a28389f…`、日志 [393,344 字节 `abb82652…`](diagnostics/CharacterPanel-g5e2-a-v74-run3-20261108.log)，零 `[E]`。22:39:21.769 关闭（623 帧、resumed 10.007 s）。E1 v3 第四次复现：`apply-paths` Σ 74,251 且 `un=0`、`apply-completions=63568:352`=`completions-pinned`=`pinned-apply-entries`、`p63563=8,851`、`fill==cap==116`、`in-flight-max=1`。

**E2-A 首次产生比较**：

| 字段 | 值 | 含义 |
| --- | --- | --- |
| `e2a-frames` | **352** | 钉住帧全部比对 |
| `e2a-main-frames` | **0** | 无双槽混合帧（again 全 fallback） |
| `e2a-fallback-frames` | **352** | `w=1.000000`、`slot0=116`、`slot1=0` |
| `e2a-tracks` | **40,832** | = 352 × 116，全覆盖（无抽样） |
| `e2a-failed` | **5,838** | 占 40,832×7 = 285,824 个分量的 **2.04%** |
| `e2a-max-delta` | **2.000** | 个别帧误差极大 |
| `e2a-ok` | **no** | 判据要求 failed=0 |

明细行（234 条可见，其余被 detail 预算截断但计数完整）显示两种帧型：约 **130 帧 `mix-failed=0 max-delta=0`**（完全匹配），其余帧 **20–30 个分量不符、`max-delta` 多在 0.005–0.06**，个别帧 0.09–0.24。

**裁定**：**A2/A3 不通过**——恒等式在超过一半的可见帧上成立，但在约 100 帧上有系统性残余，说明 fallback 分支下 `poseLocal` 并不总是等于槽0 轨道。**主路径双槽混合（`e2a-main-frames=0`）仍未覆盖**，本轮不产生任何双槽结论。

**诊断缺口（本轮）**：`ProbeMixTally` 已保存 4 条失败样本，但关闭行只打印了 `e2a-samples=4`（计数），**样本内容没有落日志**——与 v72 的"零值无原始值"同型，只是这次缺的是"失败内容"。

**下一步（v75 计划，尚未实现）**：① 关闭时追加 4 条样本行（`track/channel/expected/actual/delta`）；② 每个失败分量归类为"更接近槽0 / 更接近槽1 / 都不接近"并计数（`e2a-near-slot0/slot1/neither`）；③ 记录失败帧的 `[g+0x148]` 覆盖门（`[[g+0x110]+0x40]`、`[+0x88]`、`[g+0x100]`）与 `flag246`，以区分"IK/后处理改写"与"out 缓冲不是槽0"两种假设。**`63666` 的输出选择语义（`out` 是否等于某槽）尚未离线确定**，v75 的三项统计能在不改前提的情况下区分它。

**十二、第四轮（v75，2026-11-07 22:56）—— 残差归因：全部来自 IK/后处理，且本会话 354 帧全部门开**

工件：v75 DLL `04ea0b97…`、日志 [394,000 字节 `822c7e8f…`](diagnostics/CharacterPanel-g5e2-a-v75-run4-20261108.log)，零 `[E]`。E1 v3 第五次复现（Σ 74,996 且 `un=0`、`apply-completions=63568:354`、`p63563=8,850`、`fill==cap==116`）。

| 字段 | 值 | 含义 |
| --- | --- | --- |
| `e2a-frames` / `main` / `fallback` | 354 / **0** / 354 | 仍全 fallback |
| `e2a-tracks` | 41,064 | = 354 × 116 |
| `e2a-failed` | 9,223 | 287,448 分量的 **3.21%** |
| `e2a-fail-frames` | 260 | 354 帧中 260 帧有失败 |
| **`e2a-fail-ik`** | **260** | **全部失败帧的 IK 门都开** |
| `e2a-ik-ready` | **354** | **全部 354 帧 IK 门都开** |
| `e2a-near-slot0/slot1/neither` | 0 / 0 / 0 | 归因**未累加**（见缺陷） |
| 每帧 `mix-failed` | 21–26 | 高度稳定 |
| 每帧 `max-delta` | ≈0.1795 | 高度稳定 |

**四条 `e2a-sample`（首次落地）**：全部在 **track=6** —— `ch=2` (trans z) `expected 0 / actual −7.6e-6`；`ch=4` (rot x) `0.0474 → 0.0442`；`ch=5` (rot y) `0.9859 → 0.9704`；`ch=6` (rot z) `0.1558 → 0.2331`。

**判读**：

1. **残差不是噪声**：失败分量数（21–26/帧）与 `max-delta`（≈0.1795）跨帧几乎不变，且集中在少数轨道（样本全在 track 6）与旋转通道 —— 这是**确定性的后处理贡献**。
2. **归因成立**：`fail-frames = fail-ik = 260` 且 `ik-ready = 354`，说明失败帧与 IK 门就绪帧**完全重合**；结合 63581 的顺序（`58423/58427` 后处理 → `memcpy` 到 poseLocal），可以判定 **`poseLocal` 与"纯混合期望"的差 = IK/后处理（58423 路径）的写入**。这正是 E2-B 的题目，不是 E2-A 的恒等式失败。
3. **本会话不可判定**：354 帧 IK 门全开 ⇒ **没有任何一帧能用于严格检验纯 fallback 拷贝**。E2-A 在"IK 全开"配置下**不可判定**（既不能说成立，也不能说失败）。
4. **实现缺陷（v75）**：`e2a-near-*` 三个计数全 0 —— 逐分量归因只累加进了局部 `frame` tally，没有并入窗口级 `s.mix`，因此归因数据丢失（与之前几轮同型：本地 tally 与窗口 tally 的累加遗漏）。

**修复（v76，提交 `92db871ea7`，DLL `e183927d…6e6f0d`，CodeView age 408，[构建记录](build.md#v76-g5e2-a-2026-11-07)）**：

1. 逐分量归因并入窗口级（`s.mix.near_slot0 += frame.near_slot0` 等）。
2. 失败按 IK 门分帧统计：`e2a-ik-frames`、`e2a-failed-ik`、`e2a-failed-no-ik`、`e2a-fail-no-ik`；**判据收紧为「`failed-no-ik == 0` 才算恒等式成立」**，IK 帧的残差转交 E2-B。
3. 离线：e2a 9 + v3 6 + repair 2 + legacy 8 全过；已部署（v75 三件套备份 `dist/deploy-backup-20261108-v75-v76pre/`、INI `c3703b74…`）。

**下一步的两条路**：① 若下一会话仍有 `ik-frames = frames`，E2-A 结论固定为"不可判定（IK 全开）"，需要**让 IK 门关闭**的操作（例如角色离地/无 IK 目标，或在 footIK 关闭的姿势下开面板）才能严格验证；② 或把 E2-A 的判据改为"fallback 拷贝语义 + IK 残差指纹"，直接进入 E2-B（IK/后处理贡献分解），把已测得的 `max-delta ≈0.1795`、track 6 等作为 E2-B 的锚点。

**十三、更正（2026-11-07 晚，反模式 12）：v75/v76 用的「IK 门」是错的，§十二的归因撤回**

离线复核 [63581 dump](../build/g5-e1-audit-20261103/dump_63581.txt) 后确认：

- v75/v76 记录的"门"（`[g+0x110] → [+0x40] → [+0x88]` 且 `[g+0x100]≠0`）实际是 **payload 覆盖门**（0xbaffc3..0xbaffed：`mov rax,[g+0x110]; mov r8,[rax+0x40]; cmp [r8+0x88],0; cmp [g+0x100],0` → 把 `[g+0x148]` 的 0x30 字节写进 out 的 **payload**）。它与 58423 无关。
- 真正的 **58423 IK/后处理门**在 0xbb0040..0xbb0076，四条件：`word[g+0x240]>0 && byte[g+0x249]≠0 && byte[0x2022530]≠0 && [g+0x218]≠nullptr`（真 → `call 58423`，假 → `call 58427`）。
- 因此 §十二 里 "`ik-ready=354` ⇒ 全部帧 IK 门开、`fail-ik=fail-frames` ⇒ 残差 = IK 后处理" 的**推理链不成立**：那个门在 v75 会话里恒定打开，对失败帧没有任何区分度；且 payload 覆盖改的是 payload 不是 tracks，本来就解释不了 poseLocal 的差异。**§十二 的因果归因撤回**，只保留"9,223 个分量不符、每帧 21–26 个、`max-delta≈0.1795`、样本集中 track 6"这些**原始读数**。

**路 A 的正确抓手（离线已证）**：

| 字段 | 位置 | 性质 |
| --- | --- | --- |
| `byte[g+0x249]` | 63587 每帧写：0xbb23b7 `movzx eax,[rdx+0x2e]; mov [rcx+0x249],al`；0xbb28ca `cmp [rsp+0x120],0` → 0/1 | **每帧可变**，footIK 模式 |
| `word[g+0x240]` | 63587 每帧重算 | 每帧可变 |
| `[g+0x218]` | rootNode 指针 | 通常常驻 |
| `byte[0x2022530]` | 镜像静态值 1；全镜像仅 3 处引用（63581 两次、63587 一次） | **引擎全局开关** |

**v77（提交 `236775890a`，DLL `ac96f2a4…262ca` 1,742,336 字节，CodeView age 411，[构建记录](build.md#v77-g5e2-a-2026-11-07)）**：

1. `probe_ik_gate_open(count, mode, global, root)` 四条件纯函数（10 例 e2a + 6/2/8 例既有回归通过）。
2. 每帧 `e2a` 行打印 `ik=[mode:… count:… global:… root:…]` 四个原始字节。
3. `failed-ik / failed-no-ik` 的分流改用真门；判据仍 `failed-no-ik == 0`。

**路 A 的上机协议（用户）**：要拿到 gate-closed 帧，需让 `byte[g+0x249]` 为 0 —— 该字节来自 63587 的 ctx+0x2e，与角色当前 footIK/姿势相关。可行尝试：**角色离地/站到无地面 IK 目标的位置**（例如跳起瞬间、或站在不会触发 footIK 的斜面/物体上）再开面板；或换成 footIK 不参与的姿势（如骑乘/坐姿，若面板允许）。若 `byte[0x2022530]` 在运行期也是恒定 1，则全局开关无法从游戏侧改变，只能靠 `g+0x249` 或 `g+0x240`。**每帧四字节打印后，先看有没有 `ik=[mode:0` 的帧，再决定是否需要改操作序列**；没有 gate-closed 帧就如实记"本配置不可判定"，不得回头把 IK 帧残差算作通过。

**十四、路 A 关闭（v77，2026-11-08 00:30）—— IK 门结构性恒开，但"无修正帧"稳定存在**

工件：v77 DLL `ac96f2a4…`、日志 [345,300 字节 `dc9bf10b…`](diagnostics/CharacterPanel-g5e2-a-v77-run5-20261108.log)，零 `[E]`。00:30:07.613 arm、00:30:13.519 转段（display 321 帧 / 468 插件 entry）；日志在 00:30:18 截断（游戏提前退出，**无关闭行**，故本轮没有 `failed-ik/failed-no-ik` 汇总，但 195 条 `e2a` 明细足够回答四字节问题）。

**静态闭合（三条，已逐字节核实）**：

1. **32898 构造 ctx**（0x54c160..0x54c16f）：`mov dword [rsp+0x50],0x1000000` → `+0x2b=1`；`mov word [rsp+0x54],1` → `+0x2c=1,+0x2d=0`；**`mov byte [rsp+0x56],1` → `+0x2e = 1`（硬编码常量）**。
2. **63602 尾部**（0xbb63a0）：`mov dword [rsp+0x6b],0x1000101` → `+0x2b..+0x2e = 1,1,0,1` → **同样是 1**。
3. **全局字节 `0x2022530`** 全镜像仅 3 处 `cmp`（0xbb0053 / 0xbb017c / 0xbb2882），**无任何写入** → 常量 1。

63587 的 `[g+0x249] = record+0x2e`（0xbb23b7），两个调用者都写 1。**结论：`[g+0x249]` 在正常游玩中恒为 1，58423 门在 `count>0` 时恒开——任何姿势/位置都无法关闭它。** 路 A 的"靠姿势关 IK"协议**结构性不可行**（不是操作问题）。

**运行实证（195 条 `e2a` 明细）**：`ik=[mode:1 count:116 global:1 root:1]` **全部 195 条完全一致**，无一条 `mode:0`。

**但本轮得到一个更有用的事实**——门恒开 ≠ 每帧都改写：

| 会话 | 总可见帧 | `mix-failed=0`（完全匹配） | 有修正 |
| --- | --- | --- | --- |
| v75 | 249 | **93（37%）** | 156 |
| v77 | 195 | **101（52%）** | 94 |

两次会话都有**稳定的"完全匹配帧"子集**。结合 58423 是 footIK 体（audit23 后验注），可以判定：**58423 每帧都被调用，但只在脚需要修正的帧上改变骨变换**；在它不产生修正的帧上，`poseLocal == mix(槽1,槽0,w)` **严格成立**（`mix-failed=0`、`max-delta=0`）。

**裁定**：

1. **路 A 关闭**：不存在 gate-closed 帧，`failed-no-ik == 0` 这条判据在本引擎上**恒为空真**（vacuous），必须废弃——它永远"成立"，没有判别力。
2. **E2-A 不能按"无修正帧"通过**：因为"无修正"目前是用 `failed==0` 定义的，属于同义反复。要把它变成判据，必须**独立证明残差属于 footIK**，即下列二选一：
   - **E2-A′**：加入 `track index → 骨名` 映射，验证**全部失败分量只落在 footIK 会影响的骨**（腿/脚/骨盆链）上；若失败出现在手臂/头等 footIK 不触及的骨，则恒等式仍被推翻。
   - **E2-B**：hook **58423 入口与返回**，在同一帧内取"post-process 输入（= mix 结果）"与"输出"，验证 `poseLocal` 等于输出；这样 footIK 的贡献被显式建模而不是被排除。
3. 已测得的锚点保留：残差规模（1–250 个分量/帧）、`max-delta` 量级、两次会话的匹配帧比例（37% / 52%）。

**下一步建议**：优先 **E2-A′**（改动最小：只需骨名映射，不新增 detour），它能把"残差是否属于 footIK 骨集合"变成可判定的问题；若映射显示残差超出 footIK 骨集合，再上 E2-B 的 58423 观测。

**十五、E2-A′ 实现与部署（v78，2026-11-08 01:2x）—— 残差按骨归属判定**

路 A 证明 58423 门无法关闭（§十四），因此 E2-A 的判据改为**按残差落在哪根骨**来判，而不是按门状态：

- **实现**（提交 `282163ca06`，DLL `237c110e…b636d6` 1,745,408 字节，CodeView age 413，[构建记录](build.md#v78-g5e2-ap-2026-11-08)）：
  1. **arm 时缓存骨名**：从 `pinned->characterInstance.setup.get()->animationSkeleton.get()->bones[i].name` 读到 256 槽静态缓存（渲染线程、对象存活期，唯一安全点）；日志 `bone-names cached=N`。
  2. **detour 内归属**：每条失败分量按 track 索引查缓存名，`probe_bone_is_foot_related` 做大小写不敏感子串匹配（foot/toe/ankle/calf/thigh/knee/leg），命中计 `fail_foot`，否则计 `fail_other` 并把前 4 个"其它骨"名字原文留存。
  3. **输出**：关闭行 `e2a-fail-foot` / `e2a-fail-other`；关闭后逐条打印 `e2a-bone index=… name='…'`。
- **判据（预注册，不得放宽）**：
  - **A7a 通过**：`e2a-fail-other == 0` 且 `e2a-fail-foot > 0` ⇒ 残差全部落在足/腿链上，恒等式"除 foot-IK 修正外成立"。
  - **A7b 推翻**：`e2a-fail-other > 0` ⇒ 出现 foot-IK 触及不到的骨，恒等式被推翻；打印的骨名即证据。
  - **注意**：`fail_foot` 只说明残差落在足链上，**不自动证明**它由 58423 造成；若要归因到 58423 本体，仍需 E2-B 的 post-process 输入/输出观测。
- **离线**：e2a **12 例**（骨名匹配 + 骨归属分类新增）+ v3 6 + repair 2 + legacy 8 全过。
- **已部署**（v77 三件套备份 `dist/deploy-backup-20261108-v77-v78pre/`、INI `c3703b74…`、门键回读正确）。

**上机协议**：与之前相同（开面板 → 10 秒内关 → 保持关闭 10 秒）。这轮**不需要任何特殊姿势**。读法：`e2a-fail-other` 是否为 0，以及若有 `e2a-bone` 行则其名字是什么。

**十六、v78 运行（2026-11-08 01:18）—— 骨名缓存成功，但窗口被提前退出截断；v79 改为逐帧输出**

工件：v78 DLL `237c110e…`、日志 [373,167 字节 `0f3324ed…`](diagnostics/CharacterPanel-g5e2-ap-v78-run6-20261108.log)，零 `[E]`。01:17:16 三钩子安装；**01:17:57.951 `bone-names cached=116`（骨名缓存成功，与骨架 116 骨一致）**、同刻 arm；01:18:04.420 转段（display 321 帧 / 502 插件 entry）；**日志在 01:18:09 截断**（转段后仅 5 秒，游戏提前退出），**无关闭行**，因此 `e2a-fail-foot` / `e2a-fail-other` 未落地（`e2a-bone` 也为 0，因为该行只在 finalize 里打印）。

**这是第二次同类截断**（v77 也是转段后 5–6 秒），说明"等关闭行"的操作要求没有被满足：窗口在**关面板后 10 秒**到期，而退出发生在转段后 5 秒。

**修复（v79，提交 `c1bf46c5b7`，DLL `3e1d2eb2…4578a78` 1,779,200 字节，CodeView age 415，[构建记录](build.md#v79-g5e2-ap-2026-11-08)）**：不再依赖关闭行——把每帧的 `fail_foot` / `fail_other` 直接放进 `e2a` 明细行（`foot={} other={}`）。这样**任意一帧**就足以判 A7a/A7b，即使窗口被截断；关闭行仍保留累计值。

- 离线：e2a 12 例 + v3 6 例全过。
- 已部署（v78 三件套备份 `dist/deploy-backup-20261108-v78-v79pre/`、INI `c3703b74…`）。

**下一轮的读法（不变）**：任一 `e2a` 行的 `other=` 为 0 且 `foot>`0 ⇒ A7a 成立；`other>0` ⇒ A7b 推翻，并给出该帧的 `mix-failed` 与骨名（v79 的 `e2a-bone` 仍只在关闭行输出，但 `other=` 计数已足够判断，骨名可在关闭行出现时获得）。

**十七、v79 崩溃定位与修复（2026-11-08 01:23 / 01:25）—— 探针记录的对齐缺陷**

**现象**：退出面板时进程崩溃，CrashLogger 已装 handler 但**未写出 crash 文件**。

**证据链**：

1. **Windows 事件日志**（`Application` / `Application Error`，Id 1000）给出：`SkyrimSE.exe 1.6.1170.0`、故障模块 **`CharacterPanel.dll`**、异常 **`0xC0000005`**、**错误偏移 `0x53f9c`**；两次崩溃（01:23:02、01:25:51）**偏移完全相同** → 稳定复现。
2. **离线反汇编**该 RVA（DLL 基址 0x180000000，`.text` 起始 0x1000）得到崩溃指令：
   ```
   0x053f67: cmp rax, 0x1000          ; 环深度 4096
   0x053f6d: jb  0x180053f9c
   0x053f9c: movaps xmm0, [rsp]       ; <== FAULT（16 字节对齐要求）
   ...
   0x053fac: and eax, 0xfff           ; head % 4096
   0x053fad: shl rax, 6               ; × 64（ProbeRecord 大小）
   0x053fb1: movups [rax+rbx+0x30] …  ; 写环槽
   ```
   这是 **`ProbeRing::push` 的整记录拷贝**被向量化为 4×xmm + `movaps`。
3. **根因**：`ProbeRecord` 最宽成员是 `std::uintptr_t` → `alignof` 仅 8；`static ProbeState` 因而只保证 8 对齐，`m_slots` 的 16 字节槽也可能只 8 对齐。**v79 给记录加了两个 `uint16` 使 `sizeof` 恰为 64（整 4 个 xmm）**，编译器于是把拷贝从 `movups` 换成要求对齐的 `movaps`，把一直潜伏的对齐缺陷暴露成硬崩溃；v78 的 60 字节记录不会触发。
4. **为何没有 crash log**：`0xC0000005` 由 WER 记录，但 CrashLogger 本轮只写了 `CrashLogger.log`（未生成 `crash-*.log`）——**此路径不可依赖**；Windows 事件日志才是可靠的第一手段。

**修复（v80，提交 `357f3f1644`，DLL `8a830039…d7fb67` 1,779,200 字节，CodeView age 417，[构建记录](build.md#v80-g5e2-ap-2026-11-08)）**：

- `struct alignas(16) ProbeRecord`，使数组存储 16 对齐、`movaps` 安全。
- 两条 `static_assert` 钉死不变量：`sizeof(ProbeRecord) % 16 == 0` 且 `alignof(ProbeRecord) >= 16`，**未来任何字段增删都不能再静默破坏它**。
- 离线四个套件（12/6/2/8）全过；已部署（v79 三件套备份 `dist/deploy-backup-20261108-v79-v80pre/`）。

**新增反模式 16**：记录/结构的布局变更会改变编译器的指令选择（`movups`↔`movaps`）；对会被整块拷贝、且位于静态或堆容器中的 POD，**对齐必须显式声明并 static_assert**，不能靠"以前没崩"推断。另：DLL 崩溃定位的第一手段是 Windows 事件日志的"故障模块 + 偏移"，配 capstone 反汇编即可精确到指令。

**十八、崩溃修复更正（v80 → v81）：alignas 是错的方向**

v80 的 `alignas(16)` **没有消除崩溃**：重编后反汇编**仍在同一 `0x53f9c` 报 `movaps [rsp]`**，且第三次崩溃（01:34:16/19，`0xC0000005`、`CharacterPanel.dll`、偏移仍 `0x53f9c`）确认。

**更正后的根因**：崩溃指令是 `movaps xmm0, [rsp]` —— **读的是栈上的源**（不是环槽，环槽的写一直是 `movups`）。任何"整结构赋值"或 `memcpy` 都会被 MSVC 向量化为 4×xmm 块移动；当**源**位于一个经手写 thunk 进入的栈帧、且该帧不是 16 对齐时，`movaps` 即触发 `0xC0000005`。而 **`alignas(16)` 让编译器"相信"源是对齐的**，从而更倾向于生成 `movaps`——所以 v80 方向相反，反而有害。

**修复（v81，提交 `f7382f59cd`，DLL `2fcb029c…23b7b7` 1,779,200 字节，CodeView age 420）**：

1. 新增 `copy_probe_record`：**volatile 逐字节拷贝**，彻底禁止块向量化；`ProbeRing::push/pop` 均改用它。
2. 移除 v80 的两条 16 字节 static_assert（它们编码的是错误不变量）。
3. **离线验证**：扫描 `.text` 中环模式（`and eax,0xfff` + 块写）附近，**已无任何 `movaps`/`movups`**（此前该处有 2×`movaps` + 6×`movups`）。这是可复核的判据，不依赖"再跑一次看看"。
4. 四套件 12/6/2/8 全过；已部署（v80 三件套备份 `dist/deploy-backup-20261108-v80-v81pre/`）。

**修正后的反模式 16**：**不要用 `alignas` 去"修" `movaps` 崩溃**——`alignas` 是给编译器的**承诺**，一旦实际不满足就会让编译器自信地生成 `movaps`。对经 thunk 进入的栈帧，唯一可靠的写法是让拷贝**不可能被向量化**（volatile 逐字节），或从源头保证 thunk 的栈对齐并**离线反汇编验证**。**"重建后反汇编确认指令已改变"应作为此类修复的必备验收**，而不是只看 `static_assert` 通过。

**十九、v81 运行（2026-11-08 01:44）—— 崩溃已修，E2-A′ 首次完整判读：失败分两类且归因清晰**

工件：v81 DLL `2fcb029c…`、日志 [406,632 字节 `7c6f2353…`](diagnostics/CharacterPanel-g5e2-ap-v81-run7-20261108.log)，零 `[E]`、**无崩溃**（v81 的 volatile 逐字节拷贝生效）。01:44:56.425 关闭（806 帧、resumed 10 s）。E1 v3 判据照常成立：`apply-paths` Σ 61,160 且 `un=0`、`apply-completions=63568:289`=`completions-pinned`=`pinned-apply-entries`、`p63563=7,225`、`fill==cap==116`、`in-flight-max=1`。

**E2-A′ 汇总**：

| 字段 | 值 | 含义 |
| --- | --- | --- |
| `e2a-frames` / `main` / `fallback` | 289 / 0 / 289 | 全 fallback |
| `e2a-tracks` | 33,524 | = 289 × 116 |
| `e2a-failed` | **3,565** / 234,668 = **1.52%** | |
| `e2a-fail-frames` | **86** / 289 | 仅 30% 的帧有失败 |
| `e2a-fail-foot` / `fail-other` | **1,086 / 2,479** | 两类都存在 |
| `e2a-failed-no-ik` | **0** | 全部失败都在 IK 门开帧（与 §十四 一致：门恒开） |
| `e2a-near-slot0` / `slot1` / `neither` | 2,529 / 13 / **1,023** | 多数"接近槽0"= 阈值边界；1,023 为实质改写 |
| `e2a-bone`（4 条） | **全部 `SkirtRBone01`** | 失败骨高度集中 |

**逐帧分类（223 条可见明细）**：

| 帧型 | 数量 | 解读 |
| --- | --- | --- |
| `failed=0` | **148（66%）** | 恒等式**完全成立** |
| 纯 foot（`foot=7…33, other=0`） | 多数失败帧 | **footIK 改写足链** ✅ 预期 |
| 纯 other（`f=66 foot=0 other=66`） | **19 帧** | **全部 SkirtRBone01** —— 布料/物理骨 |
| 混合（`f=199 foot=33 other=166`） | 2 帧 | 两类同时出现 |

四条第 6 轨样本：`ch=0` trans x `expected −6.615067 / actual −6.615061`（delta 6.7e-6，rel 1e-6）；`ch=4/5/6` rot delta 2.9e-5 / 1.4e-4 / 4.6e-4。

**判读**：

1. **A7b 形式上触发，但不构成"恒等式被推翻"**：`fail-other=2,479` 全部来自 **`SkirtRBone01`（裙子骨）**——它是**布料/物理后处理**的目标，与 footIK 无关。也就是说失败骨集合 = {足链（footIK）∪ SkirtRBone01（布料）}，**两者都是已知的后处理写入者**，没有出现"任意骨上的无解释失败"。
2. **恒等式本身在 66% 的帧上逐分量严格成立**（`failed=0`），这是 A2/A3 的正面证据。
3. **阈值偏紧已量化**：`near-slot0=2,529` 说明大量失败分量"与槽0 仅差在容差外一点点"；样本 `ch=0` 的 `|v|=6.6`、`delta=6.7e-6` 即 **rel≈1e-6**，而预注册阈值对 `|v|≤100` 一律用 **abs 2e-6**（等效 rel 3e-7），对中等量级 trans 过严。**不事后放宽**，记为新判据的输入（例如下一版改"abs 2e-6 与 rel 1e-6 取较宽"，须先预注册）。
4. **`fail-foot=1,086` 与 `fail-other=2,479` 同时存在**说明本会话两类后处理都在工作；`e2a-bone` 的 4 个样本槽被同一骨占满，**看不到完整的失败骨集合**——这是 v82 要补的（按骨名聚合，而非"前 4 个失败分量"）。

**下一步（v82）**：把"前 4 个失败分量"改为**按骨名聚合的最多 8 个唯一骨名及各自失败数**，关闭行打印；判据预注册为"失败骨集合 ⊆ {foot/toe/ankle/calf/thigh/knee/leg} ∪ {Skirt*} 且无集合外骨名"，从而把 A7a/A7b 变成**可判定的集合包含**问题，而不是靠单条样本猜。

**二十、v82 运行（2026-11-08 02:03）—— E2-A 收敛：残差全部落在三类可枚举后处理链上，严格恒等式在引擎设计下不可达**

工件：v82 DLL `229547fe…`、日志 [404,515 字节 `c6447fe0…`](diagnostics/CharacterPanel-g5e2-ap-v82-run8-20261108.log)，零 `[E]`、无崩溃。02:03:26.815 关闭（785 帧、resumed 10 s）。E1 v3 照常成立（Σ 91,945 且 `un=0`、436/436 配对、`p63563=10,900`、`fill==cap==116`）。

| 字段 | 值 |
| --- | --- |
| `e2a-frames` / `main` / `fallback` | 436 / 0 / 436 |
| `e2a-tracks` | 50,576（= 436 × 116） |
| `e2a-failed` | **10,421** / 354,032 = **2.94%** |
| `e2a-fail-frames` | **312 / 436（72%）** — 明显高于 v81 的 30% |
| `e2a-fail-foot` | **5,921** |
| `e2a-fail-cloth` | **626** |
| **`e2a-fail-unexplained`** | **3,874** |
| `e2a-near-slot0` / `slot1` / `neither` | 4,705 / 44 / 5,672 |

**`e2a-bone` 的 8 个聚合槽（每个 count=4）全部是手臂链与挂点**：

```
NPC L Forearm [LLar]   NPC R UpperArm [RUar]   NPC R Forearm [RLar]
NPC L Hand [LHnd]      NPC R Hand [RHnd]       AnimObjectL
Shield                 NPC L Pauldron
```

**四条样本**：`track=6 ch=2` `expected 0 / actual −8e-6`（delta 7.6e-6，量级≈0，rel≈1 通过 abs）；`track=7 ch=4` `0.137343/0.137336`（delta 6.6e-6，rel 4.8e-5）；`track=8 ch=1` `−0.073868/−0.073871`（delta 3.2e-6，rel 4.3e-5）；`track=8 ch=4` `−0.545909/−0.545903`（delta 5.6e-6，rel 1.02e-5）。

**判读**：

1. **`fail-unexplained` 的 3,874 个分量不是"无解释的任意骨"**：全部骨名都是**手臂链（Forearm/UpperArm/Hand）、装备挂点（AnimObjectL/Shield）与护肩**。这是 **hand IK 与装备附着**的目标集合，与 v81 观察到的足链/布料链并列，构成**第三类后处理**。
2. **误差量级**：样本 rel 落在 1e-5~5e-5，高于"若干次单精度 lerp 的累积舍入"（约 1e-6 rel），因此**判为真实差异**而不是纯噪声——按预注册判据它们**应当失败**，这一点没有放宽。
3. **阈值论证缺陷已修（v83 代码，提交 `8794c98`，未部署）**：原规则"`|v|≤100` 用 abs 2e-6、>100 用 rel 1e-5"把绝对误差当作与量级无关的判据，但相对误差才是尺度无关的；改为"**abs 或 rel 取宽**"。**该修正不会吃掉上述样本**（3.2e-6 在 0.074 上 rel 4.3e-5，仍超两界），所以 v82 的判读不变，也不是事后放宽。
4. **E2-A 的收敛结论**：
   - **恒等式在后处理链之外的骨上成立**：v81 有 **148/223 帧逐分量完全相等**（`failed=0`）、v82 亦有部分帧完全相等，说明 `mix → poseLocal` 的拷贝链在无后处理介入时**精确**。
   - **严格恒等式（残差为零）在引擎设计下不可达**：`poseLocal` 的写入者是 63581 的 memcpy，而其输入 `out` 已在之前被 **58423（footIK）** 处理；此外 hand IK 与装备附着同样在混合之后改写骨变换。因此"poseLocal == mix"只在三类后处理都不触及的骨/帧上成立。
   - 三类后处理链已枚举：**足链**（`fail-foot`）、**布料链**（`fail-cloth`，`Skirt*`）、**手臂/挂点链**（`fail-unexplained`，Forearm/UpperArm/Hand/AnimObjectL/Shield/Pauldron）。
   - 因此 E2-A 的判据应表述为 **"残差 ⊆ 可枚举后处理骨集合"**，而不是"残差 = 0"；当前实现把第三类命名为 `unexplained` 已不再恰当（它其实已被解释），**下一版应改名为 `fail_limb_attach` 并把判据写成集合包含**。

**下一步（E2-B）**：E2-A 的"严格相等"路线到此为止——要继续就必须**显式观测后处理**，即 hook `58423`（footIK）入口/返回，取得"post-process 输入（= mix 结果）"与"输出"，从而验证 `poseLocal == 58423(mix)` 并量化各链贡献；hand IK 与附着链的入口需另做静态定位。

<a id="status-20261108-session"></a>
## 当前会话摘要（2026-11-08，E2 收口 + E3 立项）

**本会话完成**（v80–v85 六个二进制、七轮上机、E2 全部闭合、E3 契约预注册）：

| 项 | 结果 |
| --- | --- |
| E2-A′ | 恒等式判定收敛：残差 100% 归因三类后处理链（足/布料/手臂-挂点），无后处理帧 148/223 逐分量相等 |
| E2-B | B1/B2/B6 全过：footIK ~2.7 次/帧、平均改写 ~184 分量、`max-delta` 10.2——poseLocal 主导写入者；发现 out 轨道计数 116/98 双值（E3 输入） |
| 崩溃修复 ×2 | v79 环拷贝 `movaps` 对齐（v81 volatile 逐字节）；v83 out 指针少解一层（v84） |
| 反模式 | +16（alignas 是承诺不是修复，必须反汇编验收）；+17（钩子参数布局必须从消费方反推） |
| E3 | 契约预注册（C1–C4 交付物、v66 反例锚点、manifest 索引缺口）；实现未授权 |

**门 1 进度**：E1 ✅ / E2 ✅ / **E3 契约就绪（实现未开始）** / 联合验收待 E3。`strict poseLocal == mix` 不可达是引擎设计（后处理链写入 out），判据应表述为"残差 ⊆ 可枚举后处理骨集合"。

**E3 实现评审的三个待决问题**：① manifest 索引方案（concurrent 窗口内同开 SourceTimeline 还是独立模式）；② 时间主轴对齐（源 settled 快照 vs 候选逐步序列）；③ footIK-seq 锚的双侧记录方式。

**记录安装**：v85 DLL `e5552e89…ba638c`（age 429）；游戏 INI dormant `27f481f5…`。本地/远端同步至 `2dea193`。


<a id="c-g5-e3"></a>
## C-G5-E3：自然源轨迹与预定输入/时间配对（2026-11-08 预注册）

**范围**：G5 门 1 的第三切片。E1 已闭合（源采样活性/完成点/配对/对账），E2 已闭合（贡献账本归因三类后处理链）；E3 要建立**合格的源/候选同代配对**，使"候选姿态 vs 源姿态"的对照有可信的参照。

**一、从 E2-B 带来的新约束**

- **源与候选的 footIK 调用可能不同**：out 块轨道计数有两个值（116/98），footIK 作用域有第二种裁剪机制（LOD/距离衰减）。**配对 manifest 必须记录双方各自的 footIK 调用数与轨道范围**，不得假设同值。
- 源姿态由 6 个工作线程轮换写入（E1 已证），候选由插件在渲染线程驱动——**源与候选天然跨线程/跨时钟**，配对按 manifest 预定的输入/时间轴对齐，不按线程对齐。

**二、E3 的四个交付物**

| # | 交付物 | 内容 |
| --- | --- | --- |
| C1 资格声明 | 源侧资格 | 固定 save、装备、骨架、project/behavior/OAR 配置；记录初态、外部输入内容/顺序/时刻；源由游戏自然推进（**控制运行区分自然演化与候选干扰**） |
| C2 bound-resource 身份 | 资源绑定 | 实际 bound-resource identity（含 OAR 替换/变体），**不能只记 animationName 或共享绑定集**；全部 clip 的时钟/dt/模式/完成阶段用完整精度记录 |
| C3 预定配对 manifest | 量化规则 | **在看 pose 误差前**固定输入/时间配对、量化、缺失/多义处理规则；禁止挑误差最小邻帧、复制输出变量、临时调时钟 |
| C4 完整源/候选记录 + 缺失账本 | 数据 | 同代数据全量记录；候选错资源/缺层/错权重/错时钟 = **被测差异**（保留）；只有源参照缺失/多义、未知阶段、采集丢失才记"证据不足" |

**三、反例锚点（预注册，防止重蹈）**

- **v66 的不合格对**：源 `12220 / SVFI / time 13.06` vs 候选 `1022 / original / time 1.8667`——来自不同上下文/时间，**不是合格对**；该会话验 G4，不追溯改判 G5 失败。
- **R1 只作诊断**：尚无合格 R2 对时保持未就绪，**不用候选八轮重复哈希代替源 oracle**。

**四、离线前置（本轮已完成）**

1. **源侧自然轨迹采集机制已存在**：`SourceTimeline`（F7 窗口，settled 词变量/活动节点计数）与 `SourceEventCapture`（58377 事件观察）可复用为源轨迹记录器；不需要新 detour。
2. **候选侧记录机制已存在**：v81–v85 的 E2-A′/E2-B 探针已在记录候选 `poseLocal`（`e2a` 行）与 footIK 修改（`b-row`）。
3. **缺口**：两侧记录目前**没有共同的时间轴/代数锚点**——E3 的实现工作是把它们纳入同一 manifest 索引（`epoch + request + view + footik-call-seq`）。

**五、实现影响面（评审用，本节不授权实现）**

| 项 | 内容 |
| --- | --- |
| manifest 索引 | `epoch/request/view` 锚 + 双侧 `footik-call-seq`；`SamplingLiveness=concurrent` 窗口内同时记录源（SourceTimeline）与候选（E2 探针） |
| 时间配对 | 以**固定输入/时间历史**（C1 记录）为主轴，不依赖实时钟；源侧 `SourceTimeline` 的 settled 快照 + 候选侧逐步驱动序列 |
| 离线回归 | manifest 索引唯一性、缺失账本计数、不合格对判停（v66 反例形态） |
| 文档同步 | 本节 + 门 1 表 + README/CHANGELOG |

**六、明确不做**：不做 R1 诊断扩展；不采集 pose 数值全文（哈希 + 阈值计数够用）；不在无合格 manifest 时给 G5 通过结论；不改 E1/E2 已闭合判据。

<a id="c-g5-e2-b"></a>
## C-G5-E2-B：后处理观测（2026-11-08 预注册）

**动机**：[E2-A](#c-g5-e2) 收敛结论——`poseLocal` 与 mix 的差全部落在**三类后处理链**（足/布料/手臂-挂点），且 148/223 帧完全匹配帧证明拷贝链本身精确。要继续推进 G5 门 1，必须**显式观测后处理**，而不是靠骨名推断。

**一、离线事实（2026-11-08）**

- **58423 = footIK 体**（0xab7f30..0xab9d63，7,731 B，audit22 链式展开）：55 个被调者，含 57148（memcpy 跳板）×2 处（`0xab80f6`/`0xab9bd7`）。**它是 `out` 的最终写入者之一**。
- **58427 = 清空/复位**（0xab9df0..0xab9e83，147 B）：逐轨道写零 + 清计数，**不是附着写入者**。
- **手臂/挂点链的写入者未定位**——但 E2-B 不需要它：只要 58423 的观测能解释足链残差、并把"非足链残差"从 footIK 观测中排除，剩下的就归手臂/挂点链，交 E2-C。

**二、观测点（预注册）**

1. **58423 入口与返回**——两处窗口已离线核验（2026-11-08）：入口 0xab7f30（4c 8b dc 4d 89 43 18，7 字节头，无跳入）；**返回点唯一**：全函数仅 1 个 ret（0xab9d62），其前 5 字节为 415f 5e 5d 的尾部（pop rdi/rsi/rbp 交错，非 5 字节整指令边界——**补丁窗口取 0xab9d5d（415d 5f 5e 5d c3）不可行，实际可用窗口为 [0xab9d5d, 0xab9d62) 的 5 字节**，逐字节为 41 5d 5f 5e 5d（pop r13/pop rdi/pop rsi/pop rbp 的中段），**有跳入检查：无**），detour 记录：
   - 入口：`rcx=[g+0x100]`（footIkDriver）、`rdx=&ctx`、`r8=&out`（**mix 结果**）、`r9d=0`。
   - **返回：`out` 的 116 轨道**（此刻 footIK 已改写）。
   - 配对：同线程 in-flight（E2-A′ 的机制复用）。
2. **63581 的 memcpy 输入**（`0xbb00ae..0xbb00cc`，57148 调用前）：`rdx` = `out` 轨道数组 → **这就是 poseLocal 的直接源**。与 58423 返回时的 `out` 对比，可判定"memcpy 用的 out 是否就是 58423 的输出"。
3. **63581 的三条 memcpy 路径**（audit23 已列：`0xbb00ae` 主路径、`0xbb01e9` fallback-IK、`0xbb0281` fallback-无 IK）——**v82 已证本会话全走 fallback**（`main-frames=0`），故实际只走 `0xbb0281`（fallback-无 IK）与 `0xbb01e9`（fallback-IK）两条；**58423 门恒开 ⇒ 走 `0xbb01e9`**。

**三、判据（预注册）**

| # | 判据 | 通过条件 |
| --- | --- | --- |
| B1 58423 活性 | 会话内 `58423-entry ≥ 1` | head 字节不符 → 不安装，判据不评 |
| B2 out 改写证据 | 存在帧 `entry.out != return.out`（任意分量差 > 阢值） | 全等 ⇒ footIK 本会话无修正（这本身是结论） |
| B3 与 memcpy 输入一致 | `58423-return.out == 63581-memcpy.src`（逐分量，E2-A′ 阈值） | 不一致 ⇒ 存在 58423 之后的第二写入者（E2-C 的新题目） |
| B4 poseLocal 一致性 | `poseLocal == 63581-memcpy.src`（E2-A′ 阙值） | 不一致 ⇒ memcpy 目标也被改写 |
| B5 归因闭合 | `poseLocal != mix` 的分量 ⊆ `58423` 改写分量 ∪ {手臂/挂点骨} | 不闭合 ⇒ 有未定位写入者 |
| B6 逐帧明细 | 每帧打印 `b-row`：`entry/return/memcpy-src` 三点哈希 + 失败分量数（按链分） | |

**四、实现影响面（本节不授权实现）**

| 项 | 判定 | 理由 |
| --- | --- | --- |
| 新 detour | **58423 入口/返回** 两处 | 记录 out 前后 |
| 63581 memcpy 源 | **不 hook**（在 58423-return 里读 out 即可，等价且无新 detour） | 0xbb00ae/0xbb01e9/0xbb0281 三路径都读同一 out |
| 池大小 | 58423 入口 5 字节、返回点需先离线枚举 ret（7,731 B 体可能有多个 ret；若不唯一则退化为"仅入口 + 下帧入口前状态"） | |
| 离线回归 | 入口实参转发（rcx/rdx/r8/r9d）、返回点 out 读取、三点一致性计数、骨名归因复用 E2-A′ | |

**四、明确不做**：不做 hand-IK/附着写入者的静态定位（E2-C）；不采集源图（源侧无后处理）；不改 E1/v3 判据；不把 B2 全等误读为"footIK 无用"——它只在"本会话无修正"下成立。

**五、实现与部署（v83，2026-11-08，提交 `8dcd91b682`）**

- **六个 detour 块**：63589 入口、63856 入口、63856 返回、**58423 入口**（7 字节链式序言头 `4c 8b dc 4d 89 43 18`）、**58423 返回**（唯一 `ret` 的尾窗 `0xab9d5d..0xab9d62`，`41 5d 5f 5e 5d`）。两处窗口的离线核验已入档（§二/上一提交），探针自有池 3→5。
- **入口记录器**：读 out 快照（`probe_read_footik_out`，policy 纯函数：`+0x32` 计数、`+0x34` 有符号轨道偏移、`dword+4>2` 门），失败则不登记 in-flight；登记 per-tid in-flight。
- **返回记录器**：再读 out，与入口快照**逐分量比对**（E2-A 阈值），差值即 footIK 改写；推 `b-entry`/`b-return` 记录对。
- **关闭行新增**：`b-calls`、`b-compared`、`b-rewritten`、`b-max-delta`、`b-clean`、`b-overwritten`、`b-entries`、`b-returns`、`b-last-rewritten`。
- **离线**：e2b 3 例（in-flight 配对、out 块读取含 gate 门与槽位映射、单分量改写检出）+ e2a 13 + v3 6 + repair 2 + legacy 8 = **42 例全过**；Release exit 0。
- **已部署**（v82 三件套备份 `dist/deploy-backup-20261108-v82-v83pre/`、INI `c3703b74…`）：DLL 1,786,880 字节 `670d52aa…3be3bf`、PDB `c119a7bb…`、CodeView age 426、身份 `…-v83-g5e2-b-footik-8dcd91b682-…`。


**六、v84 运行（2026-11-08 19:31）—— B1/B2 通过（读数超预期），b-rows 缺失暴露两处缺陷**

工件：v84 DLL `aca8a118…cb6a`、日志 [391,283 字节 `c6bd02e2…`](diagnostics/CharacterPanel-g5e2-b-v84-run1-20261108.log)，零 `[E]`、无崩溃（解引用修复生效）。关闭行完整。

| 判据 | 读数 | 裁定 |
| --- | --- | --- |
| **B1 活性** | **`b-calls=2,698`、`b-compared=2,677`、`b-overwritten=0`** | **通过** —— 58423 每帧被调（2,698 次调用 / 910 帧 ≈ 3.0 次/帧），且 2,677 次成功完成入口/返回配对；无一次 in-flight 覆盖 |
| **B2 改写证据** | **`b-rewritten=390,841`、`b-max-delta=36.04`、`b-clean=no`** | **通过（且量级远超预期）** —— footIK 每帧平均改写 390,841/2,677 ≈ **146 个分量/帧**（约 116 轨 × 7 通道），`max-delta` 达 36（位置类量级）——**footIK 是 poseLocal 最大的改写者** |
| B6 明细 | **`b-entries=0`、`b-returns=0`** | **缺陷**：打印分支缺失（见下） |

**三个新事实/缺陷**：

1. **footIK 改写是主流而非例外**：E2-A′ 的 `fail-foot=2,880`（分量）与 B2 的 `b-rewritten=390,841`（分量）量级差 136 倍。**解释**：E2-A′ 只测"poseLocal 与 mix 的差"，而 mix 已是 63582 的输出——**footIK 改写的是 `out`（混合结果）**，其效果已包含在 poseLocal 里；只有改写"恰好落在阈值外"的分量才表现为 E2-A′ 失败。**B2 的 146 分量/帧才是 footIK 的真实工作量**，E2-A′ 的残差只是它的可见子集。
2. **`ring-dropped=121`（`contention-dropped=121`）**：b-row + e2a-row + apply-row 的总产量（8,528 eligible）超过了 4,096 环容量，foot-IK 行（~5.3/帧）在会话前半段就写满环。`transport-complete=no` 是诚实结论。
3. **`b-entries=0 / b-returns=0`**：打印分支缺失——上一次脚本化编辑在中途报错退出时，**drain 的两个 case 与 print 的两个分支没有写入**（与编译/提交之间缺一次"分支存在性复查"）。修复已提交（`7816fd240b`）。

**v85（提交 `7816fd240b`，DLL `e5552e89…ba638c` 1,788,928 字节，CodeView age 429，[构建记录](build.md#v85-g5e2-b-2026-11-08)）**：补上 `b-entry`/`b-return` 两个 drain case 与打印分支；已部署（v84 三件套备份 `dist/deploy-backup-20261108-v84-v85pre/`）。


**七、v85 运行（2026-11-08 20:00）—— B6 补齐，E2-B 全部判据闭合：footIK 逐帧重写 out，最多 533 个分量**

工件：v85 DLL `e5552e89…ba638c`、日志 [369,541 字节 `6d20f0f2…`](diagnostics/CharacterPanel-g5e2-b-v85-run2-20261108.log)，零 `[E]`、无崩溃。20:00:10.137 关闭（1,015 帧、resumed 10 s）。E1 v3 第六次复现（Σ 92,712 且 `un=0`、`apply-completions=63568:439`=`completions-pinned`=`pinned-apply-entries`、`p63563=10,975`、`fill==cap==116`、`in-flight-max=1`）。

| 判据 | 读数 | 裁定 |
| --- | --- | --- |
| B1 活性 | `b-calls=2,736`、`b-compared=2,724`、`b-overwritten=0` | **通过**（每帧 ~2.7 次调用） |
| B2 改写 | **`b-rewritten=501,806`**、`b-max-delta=10.23`、`b-clean=no` | **通过**（平均 ~184 分量/次；比 v84 的 146 再高） |
| B6 明细 | **`b-entries=2,657`、`b-returns=2,659`**（日志内 217/213 条可见，其余被 detail 预算截断） | **通过**（v85 修复生效） |
| 环 | `ring-dropped=164`（contention）—— 与 v84 同型，仍是瓶颈 | 已知限制，不影响计数正确性 |

**逐帧改写分布（213 条可见 b-return）**：两段式——约半数在 **10–33** 之间（脚部微调），其余大量落在 **336–533**（**大幅改写，超过 116 轨 × 3 通道**，即包含旋转+位置全通道）；`b-last-rewritten=27`。`tracks-entry == tracks-return` **213/213 全等**（无一次读取不一致）。

**新事实：`tracks` 有两种取值 —— `116`（188 次）与 `98`（29 次）**。out 块的 `+0x32` 计数随调用变化，说明 footIK 的作用域不只由 `word[g+0x240]` 决定，还有另一条路径（可能是 LOD/距离衰减的轨道子集）。**这对 E3 配对是重要输入**：源与候选的 footIK 调用次数可能不同。

**E2-B 裁定（B1/B2/B6 全部通过）**：

1. **footIK 每帧调用约 2.7 次、平均改写 ~184 个分量、`max-delta` 达 10.2**——它是 poseLocal 的**主导写入者**，"mix → poseLocal"的差**主要由 footIK 贡献**（E2-A′ 的三类链中足链是最大项）。
2. **`tracks` 可变（116/98）**：58423 的作用域有第二种裁剪机制（E2-B 观测的副产品，未深究）。
3. **B3/B4（memcpy 源/poseLocal 一致性）按契约归下一版**；但基于改写量级，"poseLocal ≈ 58423 输出"的预判成立——**E2-A′ 的"严格恒等式失败"已完全归因于 footIK + 布料 + 手臂/挂点三类后处理**。

**E2-A + E2-B 联合结论**：`poseLocal != mix` 的差**100% 归因于后处理链**（足链 footIK 为主导、布料链、手臂/挂点链），`mix → poseLocal` 的拷贝链本身在无后处理时精确（148/223 帧逐分量相等）。**G5 门 1 的 E1/E2 部分已具备闭合条件**；下一步按排序进入 **E3（源/候选合格配对）**，需要先做 E3 契约评审。



**七、明确不做**：不做 scale 归因（E2-B）；不做层权重与 63666/59303 内部（E2-B）；不做播放速度/时钟（E2-C）；不做 IK/后处理的贡献分解（E2-B）；不做源候选配对（E3）；不改窗口语义、不改已有判据、不把 E2-A 通过当作门 1 关闭。

<a id="c-g5-e3-r"></a>
## C-G5-E3-R：实现评审草案（2026-11-09，文档；不授权实现）

**状态**：[C-G5-E3](#c-g5-e3) 已预注册，实现未开始。本节只回答该契约"五、实现影响面"留下的三个待决问题，并记录评审中查到的两条**取证更正**。**本节不授权任何代码、构建、部署或上机**；批准 M1–M3 与 R-1 处置后才进入实现。

### 一、两条先决更正（须先于三个问题，否则会按错误前提设计）

**R-1 `ring-dropped` 是生产者竞争，不是环容量。** 现行记录把 v84/v85 的丢弃写成"8,528 条产量超出 4,096 环容量"。逐字段核对两次运行的关闭行：

| 运行 | `eligible-records` | `drained-records` | `ring-dropped` | `contention-dropped` | `full-dropped` |
| --- | --- | --- | --- | --- | --- |
| v84 | 8,528 | 8,407 | 121 | 121 | **0** |
| v85 | 8,722 | 8,558 | 164 | 164 | **0** |

`full-dropped=0` 说明环从未写满；丢掉的 121/164 条全部来自 [`ProbePushGate::try_enter`](../../src/animation/behavior_graph/probe_liveness_policy.h#L358)：抢不到门就丢，`m_contended` 同步加一。环深 4,096、记录 64 B，容量不是瓶颈。

后果：**扩环不是解**。若沿用"每窗口固定丢约 1.9%"，C4 的"采集丢失 ⇒ 证据不足"会每轮触发，账本永远不完整。处置选项：

| # | 方案 | 代价 |
| --- | --- | --- |
| a | 门改**有界自旋**（≤N 次 `pause`，超限才丢并计数） | detour 内多几十纳秒，最坏仍丢 |
| b | 抢门失败时写 per-tid **二次机会槽**（锁外、单生产者独占） | 一个静态槽/线程 + drain 合并逻辑 |
| c | 预注册"可接受丢失率与归属" | 等于放宽 C4，**不建议** |

建议 a 先做、b 兜底，关闭行保留 `contention/full` 分列，下一轮验证是否真归零。

**R-2 `b-*` 是跨 tid、跨 out 块的合计，尚未建立与候选的归属。** [`record_footik_entry`](../../src/animation/behavior_graph/probe_liveness.cpp#L628) 既不判图也不判块，只把 `a_out` 存入 per-tid in-flight，因此 `b-calls/b-compared/b-rewritten` 统计的是**本进程所有 58423 调用**。v85 日志中 217 条可见 `b-entry` 的分布是 **6 个 tid、18 个不同 `out` 地址**，`tracks` 取 116（188 次）/98（29 次）。

这已足以证明 `b-*` 不是"候选那一块"的读数；至于是同一角色的多条调用路径、还是同场景多个角色，现有数据无法区分。

后果：E2-B 的"footIK 是 poseLocal 的主导写入者"应降级为**量级参照**（改写量确实大，但分母不是候选）；[E2-B 的 B5 归因闭合](#c-g5-e2-b) 因此没有取证基础，B3/B4 也未实现。**E3 不应把 footIK 序当配对锚**——锚必须能归属到候选。

### 二、三个待决问题（M1–M3，均为建议，待批准）

**M1（Q1）manifest 索引：给 `SourceTimeline` 加第三态 `concurrent`，而不是"两个开关同时开"。**

冲突点：[`install_probe_liveness`](../../src/animation/behavior_graph/probe_liveness.cpp#L969) 在 `SourceTimeline`/`SourceEventCapture`/`SourceWriteCapture` 任一开启时拒绝安装。但 `SourceTimeline` **不安装任何 detour**（`source_timeline.cpp` 内无 `safe_write`/`Trampoline`/`REL::ID`），所以这是**策略性**互斥，不是钩子冲突。v64 已为 `SourceEventCapture` 建立同形先例（三态与理由见 [source_event_capture.cpp:412](../../src/animation/behavior_graph/source_event_capture.cpp#L412)）。

建议：
- `SourceTimeline` 解析为三态（`false`/`on` 保持旧义，新增 `concurrent`）；**值域扩展，不新增 INI 键**。
- `concurrent` 由实验在 [independent_graph.cpp:1234](../../src/animation/behavior_graph/independent_graph.cpp#L1234) 一带 arm，**传入 pinned source graph**；在 [independent_graph.cpp:1079](../../src/animation/behavior_graph/independent_graph.cpp#L1079) 的 `display-close` 关闭，`world-reset` 走 invalidate。`on` 的 F7 生命周期不变。
- `install_probe_liveness` 的守卫改为"允许 `SourceTimeline=concurrent`，其余仍拒"。
- `concurrent` 下**不得**沿用 [`player_graph`](../../src/animation/behavior_graph/source_timeline.cpp#L270) 的解析（它挑"最后一个非 1stperson 图"，v64 尝试 1 就是第一帧 `source-lost-or-replaced`）；改为观察传入指针，每帧只做**指针比较**验证 manager 仍持有它，不 deref 陌生指针。

**M2（Q2）时间主轴：TSC 基准 + 输入历史为主轴 + 代数组锚。**

源 settled 是"变化驱动 + settle-3 确认"（`kSettleSamples=3`），候选是 60 Hz 逐步，不能按墙钟插值。建议：
- 两侧都用 `__rdtsc()` 打点（探针已在用，[probe_liveness.cpp:642](../../src/animation/behavior_graph/probe_liveness.cpp#L642)）；源侧在 **settle 确认帧**打点，候选在每个 step 后打点。
- 配对规则（**先于看误差**固定）：`源快照 ↔ settle 确认时刻之后第一个候选 step`；不插值、不取最近邻、不因缺失挑邻帧。源无 settle 或候选无 step ⇒ 记 `missing`，不补。
- 每窗口做**一次** tsc↔steady_clock 比例标定（记录标定值与残差），只用于人读换算，**不参与配对判定**。
- 代数锚用现成字段：`epoch`（探针窗口）、`request`/`actor_id`（arm 处）、`view`（display 窗口）。三者写入**每请求一行** `manifest`；`step` 追加到现有 `e2a` 行，**不新增每帧记录类型**。

**M3（Q3）footIK 序：不做，双侧都不做。** 见 R-2。代价是 manifest 少一个锚；但该锚目前无法归属候选，写进去就是把未定义量当证据。若将来要做，前置条件是"out 块 ↔ 图"的归属映射，那需要另一份契约。

### 三、最小实现面

| 项 | 内容 | 新增 detour | 新增 INI 键 | 新增记录类型 |
| --- | --- | --- | --- | --- |
| `probe_liveness_policy.h` | 有界自旋门 + per-tid 二次机会槽（R-1 a/b） | 否 | 否 | 否 |
| `source_timeline_policy.h` / `.cpp` | 三态解析、pinned-graph 观察、tsc 打点 | 否 | 否（值域扩展） | 否 |
| `independent_graph.cpp` | concurrent arm/close 接线、每请求一行 `manifest` | 否 | 否 | 是（每请求 1 行） |
| `probe_liveness.cpp` | `e2a` 行追加 `epoch/request/step` | 否 | 否 | 否 |
| 离线用例 | 生命周期、配对规则、缺失账本、不合格对判停 | — | — | — |

硬约束：`ProbeRecord` **保持 64 B**——加字段会改变拷贝指令形状，正是 v79→v81 三次 `movaps` 崩溃的成因（反模式 16）；若确需加字段，必须重新反汇编验收。

### 四、离线回归清单（对应 C3"不得事后改规则"）

1. manifest 索引唯一性：同一 `epoch+request` 重复 arm 必须被拒并计数。
2. 缺失账本：源无 settle / 候选无 step / 跨 epoch ⇒ 计 `missing`，不得补。
3. **不合格对判停**：以 v66 反例形态（源 `12220/SVFI/13.06` vs 候选 `1022/original/1.8667`）为输入，必须判 `not-qualified` 并停，不得进入误差比较。
4. `SourceTimeline=concurrent` 生命周期：arm / display-close / expired / world-reset 四条路径与旧 `on` 不交叉；两态同时配置时的优先级要有单测。
5. tsc 标定：单调性、回绕处理、残差上报。

### 五、C1–C4 与现有机制的对照

| 交付物 | 现有机制 | 缺口 |
| --- | --- | --- |
| C1 资格声明 | `pre-activate`/baseline/scan 行 | 初态（装备、骨架、project/behavior/OAR 配置）分散在多行，缺一行汇总 |
| C2 bound-resource 身份 | `log_clip_reference` 只在 drive 循环的 0/60/120 三个检查点调用 | 需要逐 step 的 clip 实例身份/时钟/dt/模式，或明确"C2 只按检查点取证"并由评审接受 |
| C3 配对 manifest | 无 | M1/M2 |
| C4 完整记录 + 缺失账本 | `SourceTimeline` + `e2a`/`b-*` | R-1（丢率）与 R-2（归属）未解前不完整 |

### 六、停点与授权边界

- 本节**不授权实现**；实现前需明确批准 M1–M3 与 R-1 处置。
- 不改 E1/E2 已闭合判据与阈值，不改窗口语义，不对 G5 门 1 下任何结论。
- 若评审认为 C2 必须逐 step 取证，则最小实现面要扩容；该决定须先写回本节，再动代码（避免实现过程中顺手加需求）。
- 该文件的原生字节与行尾已归一（单独提交 `docs: normalize line endings and repair four mangled bytes`）：评审时发现的 3 个 NUL 与 1 个游离 CR 在同一次提交修复，"入口 `0xab7f30`""仅 1 个 `ret`""`0xab9d62`""`0xab9d5d`"恢复前缀与字符；文件现为纯 LF 文本并由 `.gitattributes` 固定，后续可直接用常规编辑工具与 `git diff` 审阅。

### 七、实现（v86，2026-11-09；已构建，未部署、未上机）

评审 M1–M3 与 R-1a 已获批准并落地（提交 `1e645a6d48`，构建记录 [v86](build.md#v86-g5e3-manifest-2026-11-09)）：

| 项 | 落地 |
| --- | --- |
| M1 | `SourceTimeline` 解析为三态（未知值 → off 并警告；**不新增 INI 键**）。`concurrent` 由实验在 [independent_graph.cpp:1234](../../src/animation/behavior_graph/independent_graph.cpp#L1234) 一带以 pinned 图 arm，`display-close` 关闭；采样器用**指针比对**验证 manager 仍持有该图，失配即 `window-aborted reason=source-lost-or-replaced`。probe 的安装守卫只拒绝旧 `on`，两种仪器仍不叠加。旧 `on` 的 F7 生命周期逐字保留。 |
| M2 | 两侧统一 `__rdtsc()`。候选侧保留 2048 步时间线，饱和时 `steps-saturated=yes` 且不覆盖旧步；`e2a` 行新增 `step=`。源侧 settle 行新增 `tsc=`，并把确认时刻发布到 64 槽环；关闭时打印一行 `manifest`，含 `paired/unpaired` 与 `tsc-per-second`（后者只作人读换算，不参与判定）。配对规则落在 `probe_pair_settles`，已有 4 个离线用例。 |
| M3 | 未引入 footIK 锚（见 R-2）。 |
| R-1a | `ProbePushGate::try_enter` 改为最多 64 次 `pause` 的有界自旋，只有最终失败才计一次 `contention-dropped`；`full-dropped` 仍单列。 |
| R-1b | **未实现**。按评审"a 先做、b 兜底"的次序，留给首轮读数决定。 |

**边界**：`ProbeRecord` 仍为 64 B；无新 detour、无新 INI 键、无新每帧记录类型。`SourceTimeline=concurrent` 须与 `SamplingLiveness=concurrent` 同时开启（probe 仍拒绝旧 `on`；`SourceEventCapture`/`SourceWriteCapture` 也仍拒绝与任何 timeline 共存）。

**下一轮的判读顺序**（预注册，按序停）：`manifest steps>0` 且 `tsc-per-second` 数量级合理 → `source-settles>0`（窗口确实覆盖转段）→ `ring-dropped=0`（否则实现 R-1b）→ `unpaired=0` → 才谈 C1–C4 账本。任何一步不成立即停，不得把"manifest 有行"当作配对成立。

**部署（2026-11-09）**：v86 DLL/PDB 已装入 `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\`（哈希与构建产物逐字节一致），v85 三件套备份于 `dist/deploy-backup-20261109-v85-v86pre/`；运行 INI `68be6ee9…`（10,105 字节、无 BOM、全 CRLF）相对上一版只把 `SourceTimeline` 改成 `concurrent` 并补 5 行注释，13 个运行键回读通过。部署未改产品路径：两个开关都只读，`IndependentGraph=inject` 与 v85 时相同。

### 八、首轮运行（v86，2026-11-10 22:30）—— 停在第一分叉：`source-settles=0`

[日志](diagnostics/CharacterPanel-g5e3-run1-20261110-2230.log)（358,712 字节），零 `[E]`。时间线在 22:30:12.762 以 `mode=concurrent request=1` 打开（`words=652 nodes=106`，与 v66 pin 同数量），22:30:20.057 由 `display-close` 关闭，**`lines=0`**：316 个显示帧内 652 个 word 与活动节点计数**一次都没变**。

`manifest steps=377 steps-saturated=no tsc-per-second=4.2918e9 source-settles=0 paired=0 unpaired=0 source-settle-backlog=0`：候选侧健康（377 步、4.29 GHz 比例、首末戳有序），源侧为零。

**裁定（按 §七 预注册顺序）**：**停**。`source-settles=0` 说明"并发窗口内 settle→step 配对"这条轴**结构上为空**——面板开启时源图不推进。本轮直接测量（此前只有 E1 的间接证据：暂停期引擎不跑 63589）。因此不判 C1–C4、不对 G5 门 1 下任何结论。

**对 C3 的后果**（新契约方向，待评审）：配对轴改为**输入历史**，即 ① 源侧一次自然游玩运行（`SourceTimeline=on` + F7）执行固定输入脚本，记录每次 settle 的时刻与变量；② 候选侧一次面板运行注入同一脚本，`e2a` 的 `step` 以注入时刻为 0；③ 配对按**序数**（第 k 次输入的源 settled 快照 ↔ 候选第 k 次注入后的 step 区间），**不比较跨运行的墙钟/TSC**；④ 缺失或多义按 C3 记 `missing`，不补、不挑邻帧。v86 的器件仍有价值：`step=`、`manifest steps/tsc`、settle 行的 `tsc=` 各自都是独立证据，只是不再用并发窗口当锚。

**R-1a 读数**：`ring-dropped=13`（contention）对 v85 的 164，去掉 92% 但仍非零，`transport-complete=no`（另有 `suppressed-rows=5085`）。若后续仍要并发窗口，R-1b 才有必要；改按输入历史后，离线账本不依赖满环。

<a id="c-g5-e3-v2"></a>
## C-G5-E3 v2：输入序数配对（2026-11-10 预注册，文档；不授权实现）

**依据**：[§八](#c-g5-e3-r) 的首轮运行证明并发 settle→step 轴结构为空（面板开启期源图 316 帧 `lines=0`）。契约原文要求的主轴本来就是"固定输入/时间历史"（[C-G5-E3](#c-g5-e3) §五），v2 把配对从并发窗口改回该主轴，并写成可判读的规程。

**范围**：交付一份**有资格的源/候选配对表 + 缺失账本**（C3/C4），**不含姿态数值**、不含 R1 诊断。配对单位是"第 k 次外部输入"，不是时间。

### 一、两次运行

| 运行 | 目的 | `[Experiment]` 键 | 操作 |
| --- | --- | --- | --- |
| **R（源）** | 自然游玩下的源轨迹 | `IndependentGraph=off`、`SourceTimeline=on`、`SourceTransientWatch=false`、`SamplingLiveness=off`，其余全 off | 正常游玩中按 F7 开 20 s 窗口，再执行固定脚本：等约 2 s → **装备单手武器** → 等约 5 s → **卸下** → 等约 5 s |
| **C（候选）** | 同一脚本的注入回放 | `IndependentGraph=inject`、`CandidateInputProbe=true`、`CandidateInputVariable=`（空 ⇒ 只观察不写）、`CandidateUnequipProbe=true`、`CandidateUnequipEvent=Unequip`、`DisplayPose=false`、`DisplayStep=false`、`SamplingLiveness=off`、`SourceTimeline=off`，其余全 off | 打开面板一次；实验注入 `WeapEquip` → 驱动 120 步 → 稳定后派发 `Unequip` → 再驱动 121 步 |

两次运行用同一存档、同一装备与骨架、同一 project/OAR 配置。**同一张源图**的判据：R 的 `SOURCE TIMELINE window-open … words=652 nodes=106` 与 C 的 `scan` 行同值。

**零代码**：R 侧的 settle 行已带 `tsc=`；C 侧 [`arm_candidate_input_probe`](../../src/animation/behavior_graph/independent_graph.cpp#L842) 的空变量分支以 `outcome=observe-only` 武装采样器（不写任何变量），此后每步打印 `CANDIDATE INPUT … step= istate= default= nodes=`，`clip-ref` 在 step 0/60/120 各一行，卸下切片另有 `CANDIDATE UNEQUIP … step=` 行。本契约不新增 detour、不新增 INI 键、不改产品路径。

### 二、资格与签名（C1）

R 运行合格的前提是**两个输入的签名都出现且唯一**：

| 输入 | 签名（源侧 settle 行） | 期望 |
| --- | --- | --- |
| k=1 装备 | `currentDefaultState` | `0→10`（随后 `10→1` 只记录、不作必需） |
| k=1 装备 | `iRightHandEquipped` 或 `iState`，二选一 | `0→1` / `0→9`，必须写明实际命中的是哪一条 |
| k=2 卸下 | `currentDefaultState` | `1→12` |

C 运行合格的前提是：每个输入都有 `step=0` 行，且 `default=` 与上表端点一致（装备窗口 `10`、卸下窗口 `12`）；`CANDIDATE INPUT` 覆盖 `step=0..120` 无缺口。

窗口 `budget-exhausted`、`window-aborted`、任意 `[E]` ⇒ 该运行**不合格**，不得用部分行凑对。

### 三、配对与账本（C3/C4）

- 配对键 = `(k, 输入名, 注入事件 id)`。每条记录含：R 的签名与前后值、R 的窗口内行号与 `tsc=`、C 的 step 区间 `[0, S_k]`、C 在 step 0 与 `S_k` 的 `default/istate/nodes`。
- `S_k` **预先固定**：装备窗口 120、卸下窗口 121；不看误差再定，也不允许按结果收缩。
- **不跨运行比较时间**：R 的 `tsc=` 与 C 的 `step` 分属两个域，`tsc=` 只用于运行内排序；任何按墙钟/TSC 对齐的做法都判为不合格配对。
- 缺失：签名缺失或多义、`step=0` 缺失、行预算耗尽 ⇒ 记 `missing`，不补、不取最近邻、不用另一轮替代。
- 账本逐条列出 `k / 签名 / step 区间 / missing 原因`，并给 `paired`、`missing` 两个计数。

### 四、判据（预注册，不得事后放宽）

| # | 判据 | 通过条件 |
| --- | --- | --- |
| P1 两运行合格 | 各自无 `[E]`、无 abort、无 budget-exhausted | 任一不满足 ⇒ 本轮无效，重跑 |
| P2 签名唯一 | 上表三条签名各出现恰好一次 | 多义 ⇒ 该输入记 `missing` |
| P3 端点覆盖 | 每个输入都有 `step=0` 行，`default` 与签名端点一致 | 不一致 ⇒ 该对不合格 |
| P4 账本完整 | `paired + missing = 输入数`，每条都能指到日志行号 | 有未归属行 ⇒ 账本不完整 |
| P5 无时间对齐 | 记录中不含任何跨运行时间派生量 | 出现即该对不合格 |

**本契约不产生姿态误差，也不对 G5 门 1 下任何结论**：它只交付"这两个端点在状态与资源上可比较"的资格。

### 五、停点

`[E]`、窗口 abort、budget-exhausted、签名多义、`step=0` 缺失、两次运行的源图 `words/nodes` 不同值——任一即停；保留原因与已收行数，不得降级成"部分通过"。

### 六、明确不覆盖（结论里必须写明）

1. **源侧 clip 身份**（C2 的一半）：`SourceTimeline` 不记录 clip 实例/时钟/dt，源侧 C2 只能记"未覆盖"，**不得声称 C2 闭合**。
2. **姿态 oracle**：源侧没有 `poseLocal` 快照；姿态对照需要另一次"源侧姿态捕获"（探针 `on` 生命周期读 63856 的 `poseLocal`），属后续契约。
3. **R1 诊断**：不扩展，也不用同剪辑重采样替代源 oracle。
4. **并发窗口**：v86 的 `SourceTimeline=concurrent` 与 `manifest` 保留为诊断，不再当配对锚。

### 七、实现影响面（评审用）

| 项 | 判定 |
| --- | --- |
| 代码 | **零**：两侧行都已在产（R 的 settle 行、C 的 `CANDIDATE INPUT`/`clip-ref`/`CANDIDATE UNEQUIP` 行） |
| INI | 两套运行配置，均为既有键；`SourceTimeline` 用 `on`（不是 `concurrent`） |
| 离线 | 账本由两份日志手工/脚本汇总；无新增单元测试（没有新纯函数） |
| 风险 | C 侧 120 步内若未到稳定态，属**被测差异**，如实记录，不缩步数 |
### 八、首次尝试（2026-11-10 22:51）—— 图选择不合格

[日志](diagnostics/CharacterPanel-g5e3-r-run1-20261110-2251.log)，零 `[E]`，`lines=174`（800 预算内）。三条签名各出现一次：`currentDefaultState 0→10`、`iRightHandEquipped 0→1`（同帧 `iState 0→9`）、`currentDefaultState 1→12`。脚本与 settle 规则都成立。

但 `window-open … words=463 nodes=36`，而候选的源图是 `words=652 nodes=106`（run 1 的 pinned 窗口，22:30:12.762）。**同一 actor 的两个图**：`player_graph()` 取"最后一个非 1stperson 图"，实验拿到的却是同一 manager 里的另一个对象——这正是 v66 记录过的 `463/36` vs `652/106` 分裂。按 §五 停点"两次运行的源图 `words/nodes` 不同值"，本轮在账本之前终止，**不判 P1–P5**。

**仪器缺口与修法（v87 已实现，2026-11-10）**：`on` 模式在每窗口打开时打印一行**图普查**——`graph-census actor=… found=N shown=M selector='…' origin=last|selector|selector-miss-fallback chosen=i`，随后每个受管非 1stperson 图一行 `index/name/words/nodes/selected`；并用只作用于 `on` 的选择键 `SourceTimelineGraph=<projectName>`（大小写不敏感、长度精确；空值保持"最后一个非 1stperson"）。选择不到时**回落**旧规则并在普查里标 `selector-miss-fallback`，不会静默换图。只读、无 detour、不影响产品路径，`concurrent` 仍观察 pin 住的图且不打印普查。构建 `4324aa19ac` / 身份 `…-v87-g5e3-graph-select-…`（[构建与部署](build.md#v87-g5e3-graph-select-2026-11-10)），离线 39 例通过。

**下一步**：一次**普查运行**（F7 即可，不必走脚本）读出哪个名字对应 `words=652 nodes=106`，填入 `SourceTimelineGraph` 后按 §一 的脚本重跑 R。

**普查结果（v87，2026-11-10 23:05）与根因**：`found=2`——`DefaultFemale` 是 `652/106`（候选的源），`FirstPerson` 是 `463/36`（attempt 1 看的那张）。旧解析器的过滤写的是**小写** `strstr(name, "1stperson")`，而真实项目名是 `FirstPerson`，大小写不匹配 → 过滤从未生效，"最后一个非 1stperson 图"实际取到的是**第一人称图**。`concurrent` 不受影响（它看实验 pin 的指针，run 1 的 `652/106` 即为证）。

**修复（v88，`dae61755fa`）**：`source_graph_is_first_person()` 同时匹 `firstperson`/`1stperson` 且大小写不敏感，时间线采集、事件捕获、写观察三处统一改用它。产品路径不改（`controller.cpp` 的兜底仍用自己的名字判断，但面板主判据是 `rootNode` 匹配）。离线 6 例 e3（含两种拼写、混合大小写、嵌入名、非匹配）与其余 8 套件全过；身份 `…-v88-g5e3-firstperson-…`（[构建与部署](build.md#v88-g5e3-firstperson-2026-11-10)）。运行档已改为 `SourceTimelineGraph=DefaultFemale`。

### 九、更正：历史 `on` 窗口看的是第一人称图

上面的根因不是本轮引入的，它一直存在。凡走 `player_graph()` 的 `on` 窗口，观测对象都是 `FirstPerson`（463/36），因此在结论里必须改写来源：

| 历史切片 | 原表述 | 更正 |
| --- | --- | --- |
| G3-A v37–v41 源轨迹 | "源图时间线" | 观测的是 **`FirstPerson`**（463/36）。相位序列（`currentDefaultState` 0→10→1→12→0、`iState` 0→9→0）在两张图上都有，故**序列结论不变**，但"源图"应写成"第一人称图" |
| G3B-E2 写观察（v58） | `writes=0` | 本就因 audit30 的 thunk 缺陷判仪器无效；现另加一条：它看的也是 `FirstPerson` |
| G4-C1 `on` 模式 | 源事件观察 | 仅早期的 `on` 窗口受影响；v66 起 `concurrent` 用实验 pin 的图（`DefaultFemale`），故 C 段的零事件结论**不受本更正影响** |
| E3-R attempt 1 | 三条签名齐全但图不符 | 现解释为 `FirstPerson`；这也是为什么它的 `words/nodes` 是 463/36 |

**不改写历史读数**：以上只更正"观测的是哪张图"，不重判任何当时的通过/失败；`DefaultFemale` 上的同一脚本尚未观测（v88 的 R 重跑就是为此）。

**R attempt 2（2026-11-10 23:12）已合格**：[日志](diagnostics/CharacterPanel-g5e3-r-run2-20261110-2312.log)，`selector='DefaultFemale' origin=selector chosen=0`、`window-open words=652 nodes=106`——与候选的源同身份；三条签名各出现一次（`currentDefaultState 0→10`、`iRightHandEquipped 0→1`＋`iState 0→9`、`currentDefaultState 1→12`），`lines=282` 预算内、零 `[E]`。P1/P2 与 P3 的源侧成立，**只等 C 跑的 `step=0` 行**。
### 十、账本（R attempt 2 ＋ C run 1，2026-11-10）

配对键 = 序数，不涉时间。两侧日志：[R](diagnostics/CharacterPanel-g5e3-r-run2-20261110-2312.log)、[C](diagnostics/CharacterPanel-g5e3-c-run1-20261110-2317.log)。

| k | 输入 | 源（R，`DefaultFemale` 652/106） | 候选（C，同一源图的克隆） | `S_k` | 状态 |
| --- | --- | --- | --- | --- | --- |
| 1 | `WeapEquip`（id 2） | L62 `currentDefaultState 0→10`；L69 `iRightHandEquipped 0→1`（L80 `iState 0→9`）；L81 稳定 `10→1` | L126 `step=0 default=10 nodes=113`；step 49 起 `default=1`；step 120 `default=1 nodes=111` | 120 | **paired** |
| 2 | `Unequip`（id 144） | L92 `currentDefaultState 1→12`（同帧 `IsUnequipping 0→1`、`bEquipUnequip 0→1`）；L109 `12→0` | L313 `step=0 default=12 nodes=126`；step 20 是最后一个 `12`，step 21 首个 `0`；step 120 `default=0 nodes=97` | 121 | **paired** |

`paired = 2`，`missing = 0`。

| # | 判据 | 结果 | 依据 |
| --- | --- | --- | --- |
| P1 | 两运行合格 | ✅ | R：零 `[E]`、`reason=expired`、`lines=282`；C：零 `[E]`、两个窗口各 121 行无缺口 |
| P2 | 签名唯一 | ✅ | 三条签名在 R 各出现一次 |
| P3 | 端点覆盖 | ✅ | 两个 `step=0` 行的 `default` 分别为 10 / 12，与源端点同值 |
| P4 | 账本完整 | ✅ | `paired + missing = 2`，每条都指到两侧日志行号 |
| P5 | 无时间对齐 | ✅ | 表内只用序数与行号；两侧时间戳只记录、不参与配对 |

**交付边界（不得外推）**：本账本只证明"这两个端点在状态与资源上可比较"，**不含姿态误差**。源侧 clip 身份仍未覆盖（C2 只有候选一半）；姿态 oracle 需要另一次"源侧 `poseLocal` 捕获"（探针 `on` 生命周期读 63856），属下一份契约。候选在卸下段第 21 步即回到 0，而源侧用了 1.26 s（`1→12` 25.073 → `12→0` 26.330）——该时序差异登记给时钟账本（E2-C），不在 E3 判据内。
<a id="c-g5-e2-c"></a>
## C-G5-E2-C：播放速度与时钟账本（2026-11-10 预注册，文档；不授权实现）

**范围**：E2 的第三切片。[G5 §五](#g5-draft) 要求"按稳定 clip 实例身份记录生命周期、loop-unwrapping、start/stop、single-play-end、playbackMode/speed、enforcedDuration、crop 与实际 dt，由已审计公式和时间量化规则比较，**不要求每个 active clip 每驱动步都恰好推进 1/60**"。本节把该要求写成可判读的账本，并明确它与 E2-A（合成恒等式）、E2-B（层与权重）的分工：E2-A/B 管"这一帧的姿态怎么来"，E2-C 只回答"每个 clip 的时钟有没有按已审计公式走"。

### 一、侧别划分（先定，避免又出现"看错对象"）

| 侧 | 现状 | 本节处置 |
| --- | --- | --- |
| 候选（C 运行，drive 循环） | 已有 `clip-ref`（step 0/60/120，含 name/binding-index/map-len/mapping/time/duration/enforced/crop-sum/eff-duration/mode/speed/tracks）、`clip-clock`（每 20 步的每个 active clip 的 `localTime`）、`clip-steps`（每步单字符推进编码） | **本轮可交付账本**，见 §四 |
| 源（R 运行，自然游玩） | E4 已提供 `clock-sample`（每 10 个 post-UI 帧一行，含各活动 clip 的 `localTime`）与四块锚点快照 | **仍阻塞，但原因已精确定位**（[attempt](build.md#v91-g5e4-e2c-source-20261111)）：该行缺"生命周期键 + 缺席标记 + 以引擎步为单位的 Δ"，首末相除会混入多次生命期（`1HM_Idle` 读 0.918、`MT_Idle` 读 0.131 均非时钟结论）。修法：让源侧时钟行带上 `clip-steps` 已有的三件（lifetime key、`-` 缺席、步为单位的 delta） |

### 二、观测与编码（已审计，2026-11-10 复核源码）

`clip-steps` 的每个字符是**该步的推进量**，单位为 1/60 s 帧（[independent_graph.cpp:2108](../../src/animation/behavior_graph/independent_graph.cpp#L2108)）：

| 字符 | 含义 |
| --- | --- |
| `.` | 0 帧（未推进） |
| `1`–`9` | 1–9 帧 |
| `A`–`Z` | 10–35 帧（`'A' + frames - 10`） |
| `?` | > 35 帧（大跳） |
| `N` | 负推进（回退或回绕） |
| `-` | 该步片段不在活动列表 |

`first` 是该片段首次出现于追踪的步号，`steps` 是追踪长度；`localTime` 取自片段生成器，`clip-clock` 每 20 步给一次绝对值。

### 三、规则（R1–R6，预注册）

1. **期望推进**：驱动步的 `dt = 1/60 s`；活跃一步的期望 `ΔlocalTime = dt × speed`（`speed` 取 `clip-ref` 的 `speed`，即 `playbackSpeed`）。
2. **允许批量交付**：单步可以一次推进多帧（历史已观察：12 步逐帧后每 15 步一次交付 15 帧）。**判据看总量与一致性，不看单步是否等于 1/60。**
3. **循环片段**：`mode` 为循环者，`localTime` 在 `eff-duration` 处回绕；回绕表现为一次负推进或一次大跳，两者都必须能被 `Δ = dt × speed` 的整数帧数解释。
4. **单播片段**：`mode` 为单播者，`localTime` 到达 `duration` 后停住（此后每步 0 帧），不得越过 `duration`；`enforced` / `crop-sum` 只通过 `eff-duration` 影响期望值。
5. **缺席**：`-` 表示该步不在活动列表，不产生推进，也不得被计为 0 帧样本。
6. **总量恒等式**：对任一片段、任一连续活跃区间，「帧数总和」必须等于「步数 × 1.000（±1 帧）」，即**平均速率 1.000×**；批量交付不改变总量。

### 四、判据（预注册，不得事后放宽）

| # | 判据 | 通过条件 |
| --- | --- | --- |
| T1 覆盖率 | 每个 `clip-ref` 出现的片段在 `clip-steps` 里都有对应行 | 缺行 ⇒ 账本不完整 |
| T2 单播停住 | 单播片段到达 `duration` 后每步 0 帧，且不越过 | 越过或回退 ⇒ 失败 |
| T3 循环回绕 | 循环片段的每次回绕都能被 `dt × speed` 的整数帧解释 | 不能解释 ⇒ 失败 |
| T4 速率 | 每个片段的**推进区间**（见下）`Σ帧 / 步数 = 1.000 ± 0.017`（±1 帧/60 步） | 越界 ⇒ 失败 |
| T5 编码合法 | 只出现 `. 1-9 A-Z ? N -` | 出现其它字符 ⇒ 日志损坏，账本作废 |

**本契约不比较源与候选的时钟**（源侧无采集），只交付候选侧账本与该侧判据；跨侧比较列入"阻塞"清单，等源侧捕获仪器落地后另立修订。

### 五、首轮候选侧账本（2026-11-10 C run 1，equip 窗口）

来自 [C run 1 日志](diagnostics/CharacterPanel-g5e3-c-run1-20261110-2317.log) 的 `clip-ref` / `clip-clock` / `clip-steps`：

| 片段 | mode / eff-duration | `first` | `steps` | 活跃步 | 帧总数（解码） | 推进区间速率 | 形态 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `H2H_Equip.hkx` | 0（单播）/ 0.8333 s | 0 | 121 | **61** | 49（`C`12 + `F`15 + `F`15 + `7`7） | 49 / 49 = **1.000×** | 12 步逐帧 → 三次批量（15、15、7）→ 到达 `duration` 后 **12 步保持 0 帧**（T2），随后不活跃 |
| `H2H_Idle.hkx` | 1（循环）/ 8.0 s | 49 | 72 | 72 | 12 + 15×4 = 72 | 72 / 72 = **1.000×** | 12 步逐帧后每 15 步交付 15 帧（与 v25 记录的批量规则一致） |
| `1HM_Idle.hkx` | 1 / 9.0 s | 49 | 72 | 72 | 同上 72 | **1.000×** | 同上 |
| `SCAR_1hmReadyDummy.hkx` | 0 / 6.3333 s | 49 | 72 | 72 | 同上 72 | **1.000×** | 同上，但 72 帧 < `eff-duration`×60 = 380，可见窗口内未到末端 |
| `MT_Idle.HKX` | 1 / 25.0167 s | 0 | 121 | 1 | 0 | —（仅第 0 步在场） | 第 0 步后即不在活动列表 |

**口径说明（T4 的"推进区间"）**：单播片段到达 `duration` 后仍可能留在活动列表并每步推进 0 帧，这一段属 T2（保持）而不是 T4（速率）。`H2H_Equip` 的 61 个活跃步里后 12 步是保持，因此速率分母取 49。绝对值交叉核对：`clip-clock` 在 step 60 给出 `H2H_Equip.hkx:0.8333`，等于 `duration`，与"到达末端后保持"一致（解码总量与 `localTime` 存在 ±1 帧的量化差，落在 T4 容差内）。

T1–T5 在 equip 窗口上成立（`N` 与 `?` 均未出现，无非法字符）。**这是候选侧账本，不是跨侧结论**。

### 六、停点

`clip-ref` 缺片段、`clip-steps` 出现非法字符、单播越过 `duration`、循环回绕无法用整数帧解释、活跃区间速率越界——任一即停并保留原始行；不得用"批量交付"解释越界。

### 七、明确不覆盖

1. **源侧时钟**（阻塞，依赖源侧捕获仪器）；
2. **稳定 clip 实例身份**：当前追踪以 `leaf name + first_step` 为键，同名片段在同一窗口重复出现时会合并——这是已知限制，换稳定实例 id 属下一版；
3. **姿态、权重、混合**（属 E2-A/E2-B/E4）；
4. **跨侧速率比较**（等 1 落地）。
<a id="c-g5-e4"></a>
## C-G5-E4：源侧姿态与时钟捕获（2026-11-10 预注册，文档；不授权实现）

**依据**：G5 要求"同阶段快照 + 完整覆盖"，而源侧至今**没有任何姿态或 clip 时钟数据**——`SourceTimeline` 只读 652 个词变量与活动节点计数。这同时卡住两件事：E3 账本只能证明"端点可比较"而给不出参照姿态；[E2-C](#c-g5-e2-c) 的源侧时钟半边也没有采集。E4 就是补这一块：**一次捕获，同时供 E2-C 的源侧速率与后续姿态对照使用**。

### 一、观测点与安全（不用新 detour）

| 项 | 决定 | 理由 |
| --- | --- | --- |
| 观测点 | `SourceTimeline` 的**每帧 post-UI 采样**（已有，`sample_source_timeline`） | 该时刻引擎本帧的 apply 已完成，`poseLocal` 是"已完成输出"的一致快照；不读 apply 中途，避免撕裂 |
| 不叠加 | **不安装 63856 detour**，`SamplingLiveness` 保持 off | E1 的"仪器不叠加"硬约束继续有效；时序代价（比 apply 晚 ≤1 帧）用 §四 的锚点规则抵消 |
| 读取方式 | `graph->characterInstance.poseLocal` / `numPoseLocal`（CommonLib 类型化成员），`hkQsTransform` 0x30/轨 | 不猜偏移；与候选侧 `read_pose_local` 同一套访问器 |
| 写/分配 | 无写入、detour 内无分配；行在渲染线程拼装 | 沿用现有日志纪律 |

**相位声明**（必须写进结论）：源侧快照是"本帧 apply 完成之后"的读数，与 63856 入口的 `poseLocal` 相差 ≤1 帧；每行记录 `frame` 与 `applies-seen` 两个计数，供离线判断相位差。

### 二、捕获内容（两种行）

1. **`pose-snapshot`（锚点快照，每窗口 ≤8 行块）**：`graph/behavior/pose-local` 指针、`n`、以及 `n` 条轨道的 `trans xyz`、`rot xyzw`、`scale xyz`（各 6 位小数）；同一块内附**当时的 clip 表**：最多 8 个活动片段的 `name / localTime / mode / speed / eff-duration`（`node_is_clip_generator` 的 vtable 判定，非片段节点不列）。
2. **`clock-sample`（时钟采样，每 10 帧一行，每窗口 ≤120 行）**：`frame` 与活动片段的 `name:localTime` 列表——用于算源侧速率，不携带姿态。

### 三、预算与丢失（先声明，事后不放宽）

| 项 | 上限 | 超出时 |
| --- | --- | --- |
| 快照块 | 8 / 窗口 | 计 `snapshot-dropped`，**该轮判证据不足** |
| 时钟行 | 120 / 窗口 | 计 `clock-dropped`，速率判据降级为"未覆盖" |
| 行预算 | 1,500 / 窗口 | 计 `line-dropped`，同上 |

### 四、锚点规则（先固定，不看值再定）

1. 时间线把一个 settle 确认写入 settle 环的那一帧记为 `T_k`（现有 `tsc`）。
2. 快照取 **`T_k` 之后第一个 post-UI 采样帧**，并在行内记 `anchor-delta-frames = snapshot_frame - T_k_frame`（应为 0 或 1）。
3. 一个 settle 只允许一个快照；同一输入出现第二个快照即该窗口作废（防止事后挑帧）。
4. 快照**不得**用"姿态看起来最稳的帧"替换——锚点与姿态数值无关。

### 五、判据（预注册）

| # | 判据 | 通过条件 |
| --- | --- | --- |
| S1 锚点完整 | 每个输入的 settle 各产生恰好一个快照 | 缺/多 ⇒ 证据不足 |
| S2 同一代 | 各快照的 `pose-local` 指针与 `n` 完全相同，且窗口内无 `window-aborted` | 变化 ⇒ 该轮作废（图被替换） |
| S3 覆盖完整 | 每块 `n` 条轨道 × 10 个分量齐全，`1 ≤ n ≤ 116`，无截断 | 缺轨 ⇒ 不足 |
| S4 相位可查 | `anchor-delta-frames ∈ {0,1}` 且每块都记 `frame`/`applies-seen` | 越界 ⇒ 记录并停 |
| S5 无丢失 | `snapshot-dropped = clock-dropped = line-dropped = 0` | 任一非零 ⇒ 不足 |
| S6 时钟可算 | 每个活跃片段在窗口内 ≥10 条 `clock-sample`，区间速率可算 | 不足 ⇒ 源侧时钟"未覆盖" |

**S1–S5 只证明"取到了合格快照"，不证明姿态相等**；姿态对照需要独立容差与校准（门 3），不在本节。

### 六、已知限制（结论里必须写明）

1. **clip 身份仍以名字为键**：同名片段重复出现会合并（E2-C §七 同款限制），换稳定实例 id 属后续修订。
2. **源侧权重/层/混合仍未覆盖**（E2-B 只有候选侧）。
3. **相位差 ≤1 帧**：快照是 post-UI，不是 63856 入口；对照时必须用同一锚点规则选取候选侧对应 step，不得按最近邻挑。
4. **`scale` 取存储值**（未做 Ni uniform-scale 转换），与 G5 §五 的"转换前 XYZ"要求一致。

### 七、停点

`[E]`、`window-aborted`、`snapshot-dropped`/`clock-dropped`/`line-dropped` 任一非零、`n` 越界、指针在两块之间变化、`anchor-delta-frames > 1` —— 任一即停，保留已收行数，不降级为"部分通过"。

### 八、实现影响面（评审用）

| 项 | 内容 |
| --- | --- |
| 源码 | `source_timeline.cpp/.h`（快照与采样）、`source_timeline_policy.h`（锚点选择、clip 表格式化、预算计数的纯函数） |
| 设置 | 新键 `SourcePoseCapture=on|off`（默认 off，要求 `SourceTimeline=on`；与 `SamplingLiveness` 互斥，保持仪器不叠加） |
| detour | **零**；不新增原生调用或写入 |
| 离线 | 锚点选择（含"一个 settle 一个快照"）、预算外溢计数、clip 表格式化截断、`n` 越界判停 |
| 预算 | 每窗口 ≤1,500 行；20 s 窗口下 120 条时钟行 + ≤8 块快照（每块约 125 行，合计约 1,120 行） |
| 文档 | 本节 + E2-C §一 的"阻塞"改为"由 E4 解" + README/CHANGELOG |

**首轮运行（v89，2026-11-10 23:43）不合格**：[日志](diagnostics/CharacterPanel-g5e4-capture-run1-20261110-2343.log)。图正确（`DefaultFemale` 652/106）、`n=116`、`pose-local` 指针全程同一、`anchor-delta-frames=1`、`clocks=86`、928 条 track 行与 18 条 clip 行解码正常——但 **`snapshots=8` 且 `snapshot-dropped=1`**，而 S1 要求"每个输入恰好一块"、S5 要求零丢失。根因：锚点规则对六个装备链变量的**每次变化**都开锚（`0→10`、`10→1`、`iState 0→9`、`1→12`、`12→0` …），一次输入产生多块。按停点判**证据不足**，本轮快照不作任何姿态参照。

**E4 v2/v3（v90/v91，已实现）**：锚点认 `currentDefaultState` 的四条边——`0→10`（装备）与 `1→12`（卸下），与 [E3 账本](#c-g5-e3-v2) §十 的两个输入一一对应；其余装备链变量只作确认，不再开锚。v3 补上稳定态两端（`10→1`、`12→0`），每窗口四块——对应 E3 账本每个输入的起点与稳定两个端点，仍在 8 槽预算内。实现见 `eb67deeecd`/`a44b3e932a` 与 [v90](build.md#v90-g5e4-anchor-v2-2026-11-10)、[v91](build.md#v91-g5e4-anchor-v3-2026-11-10) 构建记录。**实现（v89，2026-11-10）**：落在 `SourceTimeline`，零 detour；**锚点只认装备链变量**（`currentDefaultState`/`iRightHandEquipped`/`iState`/`IsEquipping`/`IsUnequipping`/`bEquipUnequip`），节点计数 settle 不锚——否则 117 条 settle 行（首条是 `TimeDelta`）会先把 8 个槽吃光（`5a52136d73` 修）；`poseLocal` 走 `hkbCharacter::poseLocal`/`numPoseLocal`，clip 表按 vtable 过滤，四元组/向量用 `memcpy` 取出（首版直接读 `.x/.y/.z` 编译失败，已改）。锚点为「一个 settle 一个快照」：同帧多变 settle 记一次，悬而未取时再来 settle 记 drop；快照在 settle 之后的第一个采样帧发出（`anchor-delta-frames` 应为 1）。关闭行新增 `snapshots/clocks/capture-lines/snapshot-dropped/clock-dropped/line-dropped`；预算 8/120/1,500 由离线用例钉住。提交 `aaf7b009fd`，[构建记录](build.md#v89-g5e4-source-capture-2026-11-10)，离线 44 例通过。

### 九、明确不做

不做姿态比较与容差（门 3）、不做源侧权重/层、不引入新 detour 或与 `SamplingLiveness` 同跑、不改 E1/E2/E3 已闭合判据、不用快照值反推配对（配对仍由 [C-G5-E3 v2](#c-g5-e3-v2) 的序数规则给出）。
### 八、v2 修订：两条解码规则与源侧读数（2026-11-11 预注册）

v92 给 `clock-sample` 加了 `ms`（墙钟基准）后，源侧速率**首次可算**，[run](build.md#v92-g5e2c-run1-20261111) 的读数是：`1HM_Idle`/`MLh_Idle`/`SCAR_1hmReadyDummy` 各 **1.001×**、`1HM_Unequip` **0.999×**（连续播放），与候选侧 1.000× 对上。低于 1.0 的行全部是**解码假象**，因此新增两条规则，先于任何裁定：

| # | 规则 | 理由 |
| --- | --- | --- |
| **R7 单播剔除保持段** | 单播片段到达 `duration` 之后每步 0 帧的那一段不计入速率分母（速率只在**推进区间**上算），该段由 T2 单独判 | `1HM_Equip` 0.825 就是被末端保持拉低的；候选侧 T4 早已如此 |
| **R8 回退即切段** | `localTime` 任一**下降**视为生命期结束；求和不跨段、跨度不跨段，每段各自算速率 | `MT_Idle` 6 次重置、`MT_RunForward` 3 次；跨段求和会得到 0.402 这类无意义值 |

**判定范围（防止外推）**：本轮 E2-C 的裁定对象是**装备链片段**（`1HM_Equip`、`1HM_Unequip`、`1HM_Idle`、`MLh_Idle`、`SCAR_1hmReadyDummy`）。`MT_*` 属于移动/转向，其重置形态与装备链不同，**不在本轮判据内**，只作为 R8 的例证登记。

**判据补充**：源侧按 R7/R8 逐段算速率后，与候选侧同片段同区间的速率比较，容差沿用 T4（`1.000 ± 0.017`）；任一段无法切分或跨段求和即记 `missing`，不补。
<a id="status-20261111-session"></a>
## 会话摘要（2026-11-09 → 11-11，E3 收口 + E2-C/E4 闭合）

**一句话**：E3 的配对账本闭合（`paired=2 missing=0`），E4 的源侧姿态捕获闭合（四块快照），E2-C 两侧时钟对上（候选 1.000×、源侧 1.001×/0.999×），期间修掉一个潜伏很久的第一人称图过滤缺陷。

| 阶段 | 做了什么 | 结果 |
| --- | --- | --- |
| E3 评审 | [C-G5-E3-R](#c-g5-e3-r)：两条取证更正（`ring-dropped` 是抢门竞争不是环满；`b-*` 是全进程合计）＋ M1–M3 建议 | 通过 |
| E3 实现 | v86：`SourceTimeline=concurrent`、TSC 打点、`manifest` 行、门有界自旋（R-1a） | 构建并部署 |
| E3 run 1 | 面板期源图 316 帧 `lines=0`，`source-settles=0` | **停**：并发轴结构为空 |
| C3 v2 | 改按输入历史的序数配对（R/C 两次运行、`S_k` 先固定、禁止跨运行时钟） | 预注册 |
| 图身份 | v87 普查：`DefaultFemale` 652/106 vs `FirstPerson` 463/36；v88 修掉大小写过滤（三处统一） | 修复并部署 |
| E3 账本 | R attempt 2（三条签名各一次）+ C run 1（两步端点全对） | **`paired=2 missing=0`，P1–P5 全过** |
| E2-C | 契约 + 候选侧账本（`H2H_Equip` 49/49=1.000×，三个 idle 72/72=1.000×）；v92 给时钟行加 `ms` | 候选侧成立 |
| E2-C 源侧 | run 1：`1HM_Idle`/`MLh_Idle`/`SCAR` **1.001×**、`1HM_Unequip` **0.999×**；R7（剔除保持段）/R8（回退即切段）预注册 | 可算、规则就位 |
| E4 | 契约 + v89/v90/v91 三次锚点迭代（全变量 → 两命令边 → 四边）＋ 三次运行 | run 1 不合格（8 块）；run 2 合格（2 块）；**run 3 合格（4 块，零丢弃）** |

**关闭判据**：E3 的 P1–P5 与 E4 的 S1–S6 均有合格运行；E2-C 候选侧 T1–T5 成立、源侧按 R7/R8 可判。**未闭合**：E2-B 的 B3/B4、源侧 clip 身份（C2 只有候选一半）、[CAL v1](#c-g5-cal-v1) 已写定规则，但前置资格未齐、原生校准未运行。

**部署终态**：v92 DLL `fe697ba9…8bcb38` + E4 运行档 `8864075e…`（`SourceTimeline=on`、`SourceTimelineGraph=DefaultFemale`、`SourcePoseCapture=true`、`SamplingLiveness=off`）；备份链 `dist/deploy-backup-20261111-v91-v92pre/` 等。

**下一步排序（2026-11-11 离线结果更新）**：[C-G5-CAL v1](#c-g5-cal-v1) 的[离线 A 已完成](#c-g5-cal-a-run1-20261111)；原生 B/K1–K6/时间仍 NOT_READY。先补输入/资源与控制资格、同代完整采样及 B3/B4 边界，再原生独立校准 → 联合验收 → 完整协议冻结 → 新正式验证。

<a id="c-g5-cal-v1"></a>
## C-G5-CAL v1：姿态测量与时间量化的独立校准契约（2026-11-11）

**状态：研究规则已写定，校准运行 NOT_READY；数值容差未产生，G5 门 3 未通过。** 本契约属于 [G5 v2](#g5-draft) 的仪器准备与独立校准，沿用门 1 → 门 2 → 门 3 → 完整冻结 → 新正式验证的顺序；写成契约不代替前两门退出。本文只登记研究范围、前置证据和执行规则，不授权产品代码修改、原生 hook、构建部署或上机运行。需要补充仪器时另立实施范围；本次没有新二进制、INI 或测试结果。

### 一、目标、范围与退出物

目标是独立确定 **parent-local 位置、旋转、原生 XYZ 缩放的测量误差，以及采样时间的量化界**，交付可复核的容差提案与适用范围。先核对输入/资源与观测阶段，再校准，最后才准备正式协议。

本契约不测定源与候选是否姿态等价，不调节候选来追随源输出，不把单层重采样当混合姿态 oracle，不替代完整层/逐骨通道权重、blend/mask/additive、最终渲染姿态或 G6 验证。层与权重的资格和数值规则继续属于 G5 完整目标，未覆盖项必须带入门 4 的阻塞清单。

退出物为：资格与缺项账本、控制资源清单、完整采样及映射对账、独立控制的误差/时间分布、容差推导表、适用阶段/资源/数值范围和停止原因。**“本契约完成”最多表示限定测量范围已校准，不表示 G5 或整个门 3 通过。**

### 二、证据基线与当前缺项

本契约基于仓库 `a7dbf557f4` 的文档和源码，以及保留的 v88/v91/v92 运行。旧数据只用于识别缺项，不作为本契约的校准样本或未来正式样本。

| 已有证据 | 可以保留 | 本契约不能据此推定 |
| --- | --- | --- |
| [E3 v2 账本](#c-g5-e3-v2)：`paired=2 missing=0` | 两输入的序数、签名和状态端点配对 | 相同装备初态、剪辑实例、采样时刻或逐骨输出 |
| [v91 E4 run 3](build.md#v91-g5e4-run3-20261111) 与 [v92 run 1](build.md#v92-g5e2c-run1-20261111) | 每轮四块、每块 116 轨、零丢失的源姿态；锚点差一个 UI 采样帧 | 完整过渡覆盖，或两侧元数据与 pose 属于同次 Generate；UI 帧差也不是引擎 dt |
| [E2-C](#c-g5-e2-c) 的候选装备段账本及源装备链速率 | 已声明范围内的速率证据与 R7/R8 解码规则 | 同一片段、完整生命周期/循环回绕证明，或可直接沿用的姿态/时间容差 |
| [E2-B](#c-g5-e2-b) 的 B1/B2/B6 | footIK 活性和 out 改写已观察 | B3/B4 的 memcpy 输入与最终 poseLocal 一致性；后处理全部归因已完成 |

当前已出现必须登记的差异：[候选日志](diagnostics/CharacterPanel-g5e3-c-run1-20261110-2317.log) 的装备片段为 `H2H_Equip`，候选输入探针是空变量的 observe-only；[源日志](diagnostics/CharacterPanel-g5e3-r-run2-20261110-2312.log) 有 `iRightHandEquipped 0→1`，[源捕获](diagnostics/CharacterPanel-g5e2c-clock-run1-20261111-0026.log) 是 `1HM_Equip`。卸下段还保留候选第 21 步返回 0 与源约 1.26 s 的阶段时长差。**这些是待解释的输入/行为差异，不是测量噪声。** 资格查明后，候选选错片段或时序偏差必须保留为被测差异，不能通过删帧、换资源或偏移时间消除。

### 三、校准运行的前置资格（K1–K6）

| 编号 | 开跑前必须提供的证据 | 当前状态 |
| --- | --- | --- |
| K1 身份与映射 | 匹配 DLL/PDB/build/INI、runtime/OAR/资源清单；源图确属 controller，源与候选各有 actor/graph/generation/request/view；骨架完整清单、父索引、reference pose、binding/默认填充规则 | 缺源侧完整资源/实例身份与实际映射清单 |
| K2 受控输入与独立控制 | 冻结装备初态及外部输入的内容/顺序/相对时刻，区分外部输入和图内输出；两项控制具备独立真值、资源哈希和可审计承载路径 | H2H/1HM 输入差异待核对；控制资源尚未登记 |
| K3 同阶段与同代快照 | 候选在 Generate 返回的 `e_post_sample` 取得完整快照；源有已审计的等价完成边界、寿命和 generation 关联；pose、clip 元数据及贡献信息在更新前固定为同代快照 | 候选现有 0/60 步细节在 `e_pre_sample`；源 post-UI 行尚不能单凭指针/帧号证明上述关联 |
| K4 后处理边界 | B3/B4 的逐分量证据，或另经审计证明两侧都在同一、明确命名的边界；未验证链保持开放，不能当噪声或排骨 | 未闭合；本契约不批准另选阶段 |
| K5 完整计数与生命周期 | 应有/访问/记录/遗漏逐项对账，generation 失效可取消，完整层/权重/混合资格仍按门 1 审计；所有上限与丢失都可见 | 现有稀疏快照和容量上限不能代表完整仪器 |
| K6 实施与数据计划 | 先有经评审的仪器范围、两个控制的就绪清单、采集/分析版本及预算，再采集本契约的新数据 | 尚无本契约的实现或运行记录 |

K1–K6 未全部满足时，不启动门 3 原生校准；可先做解析控制和仪器离线准备，但须标为前置工作。控制资源名称、哈希、真值来源、承载路径或实际观测边界尚未登记时，状态必须为 NOT_READY，不能用现有装备链输出代替控制。

身份分两层记录：进程内用图 generation、clip 节点身份与激活序号区分实例及复用；跨运行用资源完整相对路径/内容哈希、binding 身份与逻辑激活序号对应。**指针、leaf 名、binding 数量均不能单独充当跨运行身份。** 同名不同实例、资源重载、mirror/additive/mask 或 OAR 变体必须可区分；字段不可观测就登记缺项，不猜偏移。

### 四、预先固定的两个控制

**控制 A：解析已知变换的采集/序列化控制。** 在隔离的离线载体中，把独立编制的已知 `hkQsTransform` 数据送过实际采集、序列化和离线度量路径，不写游戏源图。固定非零位置、非恒等旋转和非均匀 XYZ 缩放；真值和期望误差用解析计算登记，不能调用被测比较函数生成答案。控制必须能检出一个已知非零差：例如位置差 `(0,3,4)` 的欧氏误差为 5，旋转夹角 60° 的误差为 `1-cos(30°)`，单独 Z 缩放差 0.125 的误差为 0.125；相同旋转的 `q/-q` 表示应保持等价。所有数据行是这一个固定控制的数据，不扩展成参数矩阵。

A 区分“度量/通道读取错误”和“表示精度损失”，只证明它实际经过的传输路径；离线通过不能代替源侧完成/线程/寿命证据。

**控制 B：已知单层、固定时间的原生控制。** 先登记一个真值可独立确定的控制资源，证明只有一个有效层、已知权重和 binding/reference/default-fill，且没有未建模的 IK、物理、OAR 变体或随机输入。真值来自固定的 authored 数据及其可审计采样语义；仅重复调用同一个 sampler 只能证明重复性，不能单独证明正确性。现有 `SamplePartialTracks` 可作为实现审计入口，不自动取得独立 oracle 资格；不能把其单层结果直接对照任意混合 `poseLocal`。

B 在隔离控制载体上执行，源 Actor 仍自然运行且只读，不创建第二 Actor、不手动推进源图。若需要控制图，沿用每进程一张保留候选的生命周期边界，不执行 NO-GO 的装载 teardown。**目前没有已登记且具备上述资格的控制资源/路径，因此 B 未就绪。** 资源及承载路径须在任何控制输出值被查看前写入不可变运行清单；无法找到合格控制则停止原生校准。

### 五、采样、配对、覆盖与预算

1. **重复规则**：A 的同一固定数据经过三次独立往返；B 预定三个独立进程，每进程一个窗口，保持同一控制/资源/采样版本。三个尝试全部保留；失败不得静默替换为第四个“好窗口”，重跑须先登记原因和新批次。
2. **B 的时间网格**：独立 baseline 记 `t=0`；之后完成 120 次 `dt=1/60 s` 的控制推进。第 `j=0..119` 次完成输出归属 `t=(j+1)/60 s`，不是旧 `e_pre_sample step=0` 的 bind/前次输出。每代在下一次更新前取得两份读数，以验证读取一致性；不能用相隔两帧的姿态差当读取噪声。
3. **时间配对**：B 以预定控制输入序数、完成代号及累计已交付 dt 为主键；不按姿态、候选状态或最小误差选样。时间戳须有同一进程内的单调基准、频率/单位及采集时刻，记录实际调用前后界；不能直接相减不同运行的 TSC/墙钟。E3 的序数配对保持原义，不由本契约追加跨运行时钟对齐。
4. **原生源采样量化**：另行证明输入锚点到已完成输出的时间区间，以及输出到读取的延迟区间，作为 B 的同一计时控制记录。现有 `anchor-delta-frames=1` 与每 10 UI 帧时钟采样不能代替该证明。无法观测时只交付离线/控制载体结果，原生源计时资格仍 NOT_READY。
5. **全量分母**：应有骨数来自已登记 skeleton；每个 baseline/完成输出保存全部应有轨道的 3+4+3 分量、骨索引/父级和映射原因。116 是当前资源观测值，109/116 是历史场景写骨覆盖，二者都不是全局默认分母。reference/default-fill 轨道不能因未 author 而删除；重名必须以骨架/索引/父路径消歧。
6. **对账**：B 每进程应有 1 个 baseline、120 个完成代、每代 2 份读数；按快照、骨、分量、clip/贡献实例统计 expected/visited/recorded/missing。任何非有限值、重复/缺失主键、无效映射或截断单独计数，不进入正常误差分布，也不能被计为成功或零误差。
7. **预算**：新仪器须在 K6 评审前确定格式；本契约每进程完整变换工件上限 8 MiB、身份/时间/对账工件上限 1 MiB，文本摘要最多 200 行。上限不足时整轮无效，保留已收计数；不能截断后继续裁定。不得复用 E4 的 8 块/120 时钟行/1,500 行预算宣称完成此采样，也不修改其历史预算。

控制数据保留序列化前的原始 float 位值、序列化后的十进制值和分析版本，以便分离真实数值、格式舍入与度量误差。若具体资源/记录格式无法满足预算，必须在采集前修订本契约和清单；不能边看数据边扩容。

### 六、空间、度量与无效值规则

| 项 | 本契约定义 | 边界 |
| --- | --- | --- |
| 位置 `E_p` | 相同骨父级的 parent-local 三分量欧氏距离 `sqrt(dx²+dy²+dz²)`，用 double 累加 | 使用原生存储单位；单位换算须有审计依据。root 的原始轨道保留，extracted motion 的额外贡献另列；不能未经证明剥离 |
| 旋转 `E_r` | 有限、非零范数的四元数先归一化，`c=clamp(abs(dot(q_a,q_b)),0,1)`，`E_r=1-c` | `q/-q` 等价；诊断角 `theta=2*acos(c)`（弧度）。clamp 只处理舍入，不能掩盖非有限值/零范数 |
| 缩放 `E_s` | `max(abs(sx_a-sx_b),abs(sy_a-sy_b),abs(sz_a-sz_b))` | 原生 XYZ 全保留，在 Ni uniform-scale/scaleX 转换前读取 |
| 时间 `E_t` | 已知控制的输入/完成/读取区间与预定累计 dt 的偏差及区间宽度，单位秒 | 区分时钟分辨率、读取延迟、量化与真实行为时钟偏差；未知的代/阶段不能压成一个容差 |

未匹配骨/通道、身份/阶段歧义、非有限值和零范数不是“大误差样本”。控制或仪器存在这些情况就停止校准、报告原因；未来正式运行中，合格观测下的候选无效值仍按正式失败处理，不能借“校准无效”删掉被测缺陷。

位置旧 `pose_component_delta` 是分量最大差，与本契约的欧氏距离不同；`clip_reference_policy.h` 既有 epsilon 和 G5 v1 示例值都不作为本契约的默认阈值。

### 七、统计与容差推导（先固定方法，后产生数值）

- **样本分组**：按控制/运行/阶段/骨/通道及已知时间点保留原始键。报告三次运行各自与合并的 count、min、median、P95、P99、max；分位数用升序 nearest-rank（秩为 `ceil(p*N)`）。记录全部尝试，不删离群值、不取“最好一轮”；本样本量不声称总体置信保证。
- **表示界 Q**：从原始位值到实际序列化/解析/度量路径独立求上界。若两侧均采用已验证的六位小数最近舍入，每侧每分量界为 `5e-7`，则位置对的表示界为 `sqrt(3)*1e-6`、XYZ 缩放为 `1e-6`；不同精度、格式截断或分析算术需重新证明。旋转须计入归一化、原始范数范围及 dot 的舍入，不能照搬分量界。
- **重复性界 N**：来自合格已知控制在相同输入、同一预定时间上的跨运行测量差，取全样本最大值。**同代两次原始读取必须逐位一致**；不一致先判撕裂/生命周期问题，不纳入 N。解析控制的系统偏差不能被 N 吸收；原生控制偏离独立真值而无已知误差来源时，先审计，不能直接增大阈值。
- **姿态提案**：`tau_p=round_up(Q_p+N_p)`，`tau_s=round_up(Q_s+N_s)`。旋转先在角度空间合成：`tau_theta=min(pi,Q_theta+N_theta)`，再换为 `tau_r=1-cos(tau_theta/2)`，避免直接相加 `1-abs(dot)`。`round_up` 表示在报告保留位数向外舍入，不是额外乘安全系数；提案同时保留未舍入值和推导过程。
- **计时提案**：`epsilon_t` 分列已证明的时间戳表示界和合格控制的时间区间上界；读取滞后、输入量化和批量交付各有来源。未知或随阶段改变的延迟不能当固定偏移扣掉；批量交付须先有审计过的代/时间归属，不能以观测到的最大停顿直接放宽到“通过”。
- **时间不自动变成姿态噪声**：若正式比较需要把 `epsilon_t` 传播为运动误差，须在门 4 前独立证明适用资源/骨/通道的速度或插值误差上界及组合方法。当前提案不把源候选差、相邻帧最大差或现有相位时长差加进姿态容差。

上述数值均是限定仪器、精度、资源和范数/运动范围内的**测量容差提案**。控制不覆盖正式条件时必须标为不适用，不能用低运动控制推定所有片段；已有 E2-C 的 `1.000 ± 0.017` 也不自动变成本契约的 `epsilon_t`。

当前数值表应明确登记为 `tau_p/tau_r/tau_s/epsilon_t = 未产生`，不得填 0、沿用历史 epsilon 或从源候选差反算。控制清单和推导方法变更须在新校准批次前升版本；已看过的数据不得按新规则重新充当预注册样本。

### 八、判据与停止规则（CAL1–CAL7）

| 编号 | 退出条件 | 不满足时 |
| --- | --- | --- |
| CAL1 资格 | K1–K6 都有明确证据与实际控制清单 | NOT_READY，不开原生校准 |
| CAL2 已知真值 | A 的解析度量与通道传输正确；B 的单层/真值/时间有独立依据 | 控制/仪器待修复；不推导容差 |
| CAL3 同代与覆盖 | 每代两次读取逐位一致，全部骨/通道和贡献计数对账，映射明确 | 无效，回门 1/2 |
| CAL4 重复性与计时 | 三个预定尝试完整，无未解释偏差；时间区间与批量归属可证 | NOT_READY，保留全部尝试 |
| CAL5 无丢失 | 完整工件可读、哈希匹配，missing/dropped/truncated 均为 0 | 整轮证据不足，不报部分成功 |
| CAL6 容差可追溯 | Q/N、旋转角合成、向外舍入、时间界及适用范围可独立复算；未解决范围显式开放 | 数值不冻结 |
| CAL7 裁定边界 | 只报告本契约的校准结果；完整层/权重/混合、后处理与正式配对缺项另列，未拿校准数据做 G5 验收 | 不允许进入完整冻结 |

任一 `[E]`、崩溃、源污染、需要写源或未知 ABI、采集中资源/graph/generation 失效，以及 reset/close/PreLoadGame/NewGame 都停止当前原生窗口并保留原因、已收与缺失计数；不拼接下一窗口，不继续解引用 stale 记录。已知控制不符合预期时可以修复仪器后另登记新批次，不改变本批次裁定，也不直接宣称产品失败。

### 九、交付、后续顺序与维护

校准报告至少包含：

1. 本契约版本、运行清单与分析版本，DLL/PDB/INI/build/runtime/OAR/资源哈希，独立控制真值及其来源。
2. K1–K6 的证据/缺项与 H2H/1HM 输入差异处置；源/候选/控制载体身份、骨架/binding/实例映射、观测阶段与同代证明。
3. 原始位值和完整变换/计时工件、全部三个尝试及取消记录，expected/visited/recorded/missing/dropped/truncated 对账和工件 SHA-256。
4. 按骨/通道/阶段分布与极值位置，Q/N/时间界的来源、未舍入和向外舍入数值、适用范围、未覆盖项。
5. 唯一裁定：`NOT_READY`、`INVALID`，或 `CALIBRATED（仅列明范围）`；最后一种也必须附 G5/门 3 仍开放的项目，不能写成 G5 PASSED。

**下一步顺序**：先形成 K1/K2 的输入/资源/控制清单并定位 H2H/1HM 差异 → 经评审补齐 K3–K6 仪器及 B3/B4 边界 → 独立校准 → 联合证据验收、完整协议冻结 → 另采新的正式运行。E3/E4 的历史裁定不变，G6 后置，装载 teardown NO-GO。

文档维护为 `Documentation Impact: update`：本节是 canonical 契约，入口同步至 [README](../README.md) 与 [历史索引](history.md)，研究登记同步 [Changelog](../CHANGELOG.md)。改动计量、时序、控制、分母、预算或资格规则时须升契约版本并同步这些入口；不可回写旧运行结论。本文仅依据本地已审计边界规定研究要求，不引入依赖；未来原生实施若涉及新 API/ABI、hook 或未知布局，必须另行补齐兼容性与来源证据。

| 现有代码/验证入口 | 审计用途与限制 |
| --- | --- |
| [source_timeline.cpp](../src/animation/behavior_graph/source_timeline.cpp)：`emit_pose_snapshot` / `emit_clock_sample` / `collect_source_clips` | 源捕获与时钟；核对 116 轨/8 clip/256 node 上限、精度和完整计数，不能默认为本契约就绪 |
| [independent_graph.cpp](../src/animation/behavior_graph/independent_graph.cpp)：`read_pose_local` / `log_pose_content` / `log_clip_reference` | 候选读数、稀疏明细与 pre-Generate 边界 |
| [drive_step_policy.h](../src/animation/behavior_graph/drive_step_policy.h)：`drive_graph_step` | Update → pre-sample → Generate → post-sample 的真实顺序 |
| [clip_reference_policy.h](../src/animation/behavior_graph/clip_reference_policy.h)：`pose_component_delta` / `pose_rotation_agreement_error` | 既有位置/旋转定义与历史 epsilon，不能直接冒称新契约 |
| [clip.cpp](../src/animation/clip.cpp)：`sample_animation_clip` | 单层采样的审计入口，不能自行取得混合输出 oracle 资格 |
| [source_timeline_capture_test.cpp](../tests/source_timeline_capture_test.cpp) / [behavior_graph_clip_reference_test.cpp](../tests/behavior_graph_clip_reference_test.cpp) | 既有预算/度量检查入口；本契约无新增可执行测试，未运行校准或构建 |

<a id="c-g5-cal-a-run1-20261111"></a>
## C-G5-CAL A：离线控制结果（2026-11-11）

**裁定：`CALIBRATED_OFFLINE_A_ONLY`。** 本节追加实际实施/运行证据，不改写上方 CAL v1 的资格、控制、预算、统计或停止规则；上方“尚无实现/运行”描述契约登记时的基线。仅完成控制 A 实际经过的 typed 采集 → 六位小数 track 文本 → 离线解析/度量。**K1–K6、原生控制 B、时间校准和 G5 仍 NOT_READY；`tau_p/tau_r/tau_s/epsilon_t` 未产生，门 3 未通过。**

### 实施边界与预先登记

[pose_capture_policy.h](../src/animation/behavior_graph/pose_capture_policy.h) 的 `capture_pose_sample` 是唯一的 translation XYZ / rotation XYZW / scale XYZ 提取实现，`format_source_pose_track` 统一原有 SOURCE TIMELINE track 消息及 `.6f` 精度。[independent_graph.cpp](../src/animation/behavior_graph/independent_graph.cpp) 的 `pose_sample_from_transform` 和 [source_timeline.cpp](../src/animation/behavior_graph/source_timeline.cpp) 的 `emit_pose_snapshot` 只改为调用共享入口；既有 clip-reference 度量/epsilon、设置、时钟、预算和原生读取边界未改。

[pose_calibration_capture.cpp](../tests/pose_calibration_capture.cpp) 使用明确标为 offline substitute 的 16 字节对齐、0x30 三 quad 载体；没有 SDK/native 调用、未知偏移或游戏源写入。translation/scale 第四 lane 填入各异 NaN/Infinity poison，十个有效分量的 raw bits 与独立清单逐位吻合且无 poison；这不证明 SDK/ABI、原生完成、线程或寿命安全。共享 track 消息经过本控制，实际日志 sink/envelope 未经过。

[calibrate_pose.py](../tools/calibration/calibrate_pose.py) 在第一次调用可执行文件**之前**冻结 fixture `g5-cal-a-analytic-sign-v1`、解析理想值来源、独立 expected binary32 bits、源文件/可执行文件哈希、版本和计划计数。只使用两组固定 pair：解析扰动 `(0,3,4)` / 60° / Z 缩放 `+0.125`，以及相同变换的精确 `q/-q`。double 度量逐侧归一化；独立 Decimal(80) oracle 用 `abs(dot)/sqrt(norm2_a*norm2_b)`，不调用被测 double 度量生成答案。

### 运行、计数与工件

当前分析器的 [attempt-002 report](diagnostics/g5-calibration-a-20261111/attempt-002/result.md)、[预冻结 plan](diagnostics/g5-calibration-a-20261111/attempt-002/plan.json)、[manifest](diagnostics/g5-calibration-a-20261111/attempt-002/manifest.json) 与 [完整 metrics](diagnostics/g5-calibration-a-20261111/attempt-002/metrics.json) 为当前入口；manifest SHA-256 为 `ee3e8d62383567012d5fcf2e82bc7bda5b1c98828a2e8574293c0de4d1b5936d`。原始 [round 1](diagnostics/g5-calibration-a-20261111/attempt-002/round-1.stdout.jsonl)、[round 2](diagnostics/g5-calibration-a-20261111/attempt-002/round-2.stdout.jsonl)、[round 3](diagnostics/g5-calibration-a-20261111/attempt-002/round-3.stdout.jsonl) 与各自空 stderr 全保留。

| 计数（当前批次） | expected | visited | recorded | missing / dropped / truncated |
| --- | --- | --- | --- | --- |
| 独立进程往返 | 3 | 3 | 3（各 exit 0） | 0 / 0 / 0 |
| pair（每次 2） | 6 | 6 | 6 | 0 / 0 / 0 |
| track 行（每次 4） | 12 | 12 | 12 | 0 / 0 / 0 |
| 有效分量（每行 10） | 120 | 120 | 120 | 0 / 0 / 0 |

[首次完整有效 attempt-001](diagnostics/g5-calibration-a-20261111/attempt-001/result.md) 与[首次有效索引](diagnostics/g5-calibration-a-20261111/result.md) 未覆盖。首次通过后，评审要求补明 runner 的完整性失败原因，并允许后续有理由的批次识别“已有原始文件但尚无最终 manifest”的不完整前批次。该小修不改 fixture/数值规则；在 attempt-002 plan 中先登记原因和新源哈希，再做相同两 pair 的三次新往返。**两批各自有效，无失败 capture 被替换，结果一致；不把两批拼成一个新的样本矩阵。**

### 表示、算术与重复性结果

| pair / 度量 | 解析理想值 | raw binary32 的 Decimal oracle | 文本解析后 double | ideal → binary32 | raw → 六位文本的 oracle 差 |
| --- | --- | --- | --- | --- | --- |
| analytic / 位置 L2 | 5 | 5 | 5 | 0 | 0 |
| analytic / 旋转 `1-abs(dot)` | 0.1339745962155613532363 | 0.1339745951678763855369 | 0.133974640478251583 | -1.0476849676993885e-9 | +4.531037500337656e-8 |
| analytic / XYZ scale Linf | 0.125 | 0.125 | 0.125 | 0 | 0 |
| sign / 位置、缩放 | 0、0 | 0、0 | 0、0 | 0、0 | 0、0 |
| sign / 旋转 | 0 | 0 | 3.3306690738754696e-16 | 0 | 0 |

`q/-q` 的 raw/text Decimal oracle 均为 0；double 的 `3.3306690738754696e-16` 是归一化/dot 算术残差，不能填成理想差或原生噪声。全部观测的 raw double 数值残差最大 `7.907067087577111e-17`，text double 最大 `3.3306690738754696e-16`；位置/缩放的本例数值残差为 0。

**pair 误差漂移为 0 不等于逐变换没有舍入损失。** 实际最大分量舍入为 `4.76837158203125e-7`，单个变换的位置 L2 舍入最大 `5.3312014997000448618e-7`，scale 分量最大 `3.5762786865234375e-7`；本 fixture 的共同舍入项在位置/缩放差中抵消。文本 Decimal → binary64 分量解析残差最大 `4.869349368163967e-17`。

六位最近舍入的每侧分量界 `5e-7` 均满足；位置 pair 表示界 `sqrt(3)*1e-6`、XYZ scale pair 界 `1e-6`。旋转仅在本 fixture 的原始范数约 `0.9999999829..0.9999999865` 下检查归一化 chord/dot 表示界，观察到 raw→text 漂移 `4.531037500337656e-8`，不推广成一般四元数界。[plan](diagnostics/g5-calibration-a-20261111/attempt-002/plan.json) 和[数值推导说明](diagnostics/g5-calibration-a-20261111/execution.md#numerical-interpretation) 分列 binary64 `u=2^-53`、`gamma(n)` 的运算计数及输入解析项；text 旋转算术界约 `3.552713678800507e-15`，所有残差满足该精度推导，未使用 G5 阈值。

metrics 保留每轮、每 case 和合并的 count/min/median/P95/P99/max（nearest-rank）。三次 raw bits、文本和 compiler 身份一致，每个固定 case 的三项度量跨轮 range 均为 0；这是有限控制的重复性观察，不是总体保证、原生噪声估计或一般位置/缩放零误差结论。解析理想→binary32、raw→文本和 double→高精度 oracle 三阶段分别保留，不相互吸收。

### 命令、构建与未覆盖项

[构建记录](build.md#g5-calibration-a-20261111)与[完整执行记录](diagnostics/g5-calibration-a-20261111/execution.md#actual-commands-and-exits)保存实际命令/退出码。VS 2022 toolset `14.44.35207` 的 focused `/std:c++latest /W4 /WX` 编译 exit 0；Python 3.12.10 两批分析均 exit 0，每批三个 capture 均 exit 0。使用现有 pinned cache/local dependencies、关闭 vcpkg manifest install 的 configure exit 0；必跑 Release 构建与一次显式 installed-SDK retry 均在 MSBuild SDK discovery 因用户本地 `Microsoft SDKs` 目录拒绝访问而 exit 1，**生产调用者兼容编译仍 UNVERIFIED**，未扩大权限/工具链/依赖。

没有新部署身份、游戏运行、源/候选配对、时间控制或 native control B；现有 v46 cache 标签仅用于本地兼容尝试，未重标或部署。K1/K2 输入/资源资格、K3–K6 同阶段同代完整观测、B3/B4、完整层/权重/mapping 与时间界都保持开放，G6 后置、装载 teardown NO-GO。后续按 CAL v1 顺序补齐前置，离线 A 不能跳过门 1/2 或冻结 G5。

<a id="c-g5-cal-k1k2"></a>
## C-G5-CAL K1/K2：输入、资源与控制资格账本（2026-11-11，文档；不授权实现/上机）

**裁定：K1/K2 NOT_READY。** 本节只登记 [CAL v1](#c-g5-cal-v1) §三 K1/K2 的现有证据、H2H/1HM 差异的初步归因和关闭条件；没有新二进制、INI、部署、上机、控制 B 输出或数值容差。历史 E3/E4 日志只用于识别缺项，不作为校准样本。

### 一、K1 身份与映射

| 项 | 现有证据 | 状态 / 缺项 |
| --- | --- | --- |
| 运行身份 | E3 的源/候选各一次运行均为 v88（[R](diagnostics/CharacterPanel-g5e3-r-run2-20261110-2312.log#L30)、[C](diagnostics/CharacterPanel-g5e3-c-run1-20261110-2317.log#L30)）；E4 源姿态来自 v91/v92（[v92 clock run](diagnostics/CharacterPanel-g5e2c-clock-run1-20261111-0026.log)） | E3 的 v88 只覆盖配对；E4 的源姿态不能与 v88 候选拼成同一次 Generate。K1 要求匹配的 DLL/PDB/build/INI/runtime/OAR/资源哈希，尚缺 |
| 当前安装 | v94 DLL 1,837,056 B SHA-256 `1c3b67ab…6018372`；PDB 49,631,232 B `0fc936bb…c886033`；运行档为 D3 源侧只读探针 `dist/CharacterPanel-g5cal-d3-source-probe.ini` 11,183 B `4c7ad794…13b0400`（`IndependentGraph=off`、`SourceTimeline=on`、`SourceTimelineGraph=DefaultFemale`、`SourceTransientWatch=true`、`SourcePoseCapture=false`） | 这是 **D3 源侧探针档**，不是校准档；v93 + D3 三件套备份在 `dist/deploy-backup-20261111-v93d3-v94pre/`（DLL `638e562f…ef0ac05`、PDB `0b75c57f…45d073f`、INI `4c7ad794…13b0400`）。D1/D2 已判负；D3 已上机但未覆盖初始值；v94 尚未上机 |
| 源图身份 | R 运行 `DefaultFemale` 652 words / 106 nodes；`FirstPerson` 463/36 已被 v88 过滤器排除（[R L58](diagnostics/CharacterPanel-g5e3-r-run2-20261110-2312.log#L58)） | “源图确属 controller”沿用 [G4 钉图](#g4-closure)；本次没有同时打印 controller 持有的 `source_graph`、generation、request、view。C 运行也没有 census 行 |
| 候选图身份 | 候选与控制器源图共享 `data`/`string-data`/`infos`/`initial`（v47，652/652 名字与类型对齐）；本次激活后 97 active nodes、`poseLocal` 116 轨、`request=1`（[C L103](diagnostics/CharacterPanel-g5e3-c-run1-20261110-2317.log#L103)） | 候选 generation/view、候选重载后的实例身份、候选图与源图的同代关系未登记 |
| 骨架与映射 | 116 条姿态轨；G2 的 v33 记录实际写骨 109/116，缺失 7 条，已列名的是 `x_NPC Rotate/LookNode/Translate` 与 `Weapon`（[当前结论](#current-status)） | 完整骨名、父索引、reference pose、binding/默认填充规则未在本次运行中落盘；不能用 116 或 109 当全局分母 |
| Binding 身份 | v47：源与候选共享同一 `hkbBehaviorGraphData`，名字/类型/初始值一致 | 本 C 运行 `OarContextWatch=false`，没有 `load-bindings` 行；源/候选实际 binding set 指针、数量、OAR 替换/变体未登记 |
| 资源清单 | 运行 `1-6-1170-0`、OAR `3.1.6`；候选片段 `H2H_Equip`、`H2H_Idle`、`1HM_Idle`、`SCAR_1hmReadyDummy`、`MT_Idle`；源捕获片段 `1HM_Equip`、`1HM_Unequip`、`1HM_Idle`、`MLh_Idle`、`SCAR_1hmReadyDummy`、`MT_Idle`（[C L134](diagnostics/CharacterPanel-g5e3-c-run1-20261110-2317.log#L134)、[v92 L189](diagnostics/CharacterPanel-g5e2c-clock-run1-20261111-0026.log#L189)） | 资源完整相对路径/内容哈希、project/behavior 文件、OAR 配置与变体、实际命中的 replacement 未登记 |
| Clip 实例身份 | 当前只按 leaf name + first step 记录；同名片段在同一窗口重复出现会合并（[E2-C §七](#c-g5-e2-c)） | 稳定 clip 实例 id、binding 身份、逻辑激活序号未实现；K1 的跨运行身份仍未满足 |
| 控制 A/B | 控制 A 已完成离线三轮往返；控制 B 未登记 | B 缺资源、哈希、独立真值、承载路径；不得用现有装备链输出代替 |

**K1 结论**：身份、映射和资源清单都只有局部证据，不能关闭。尤其不能用“共享 behavior data”或“116 条姿态轨”代替逐资源、逐 binding、逐骨的身份证明。

### 二、K2 受控输入与装备初态

| 项 | 源侧 R（v88） | 候选侧 C（v88） |
| --- | --- | --- |
| 运行模式 | `SourceTimeline=on`，`SourceTimelineGraph=DefaultFemale`，自然游玩（[R L13](diagnostics/CharacterPanel-g5e3-r-run2-20261110-2312.log#L13)） | `IndependentGraph=inject`；`CandidateInputProbe=true` 且变量名为空（observe-only）；`CandidateUnequipProbe=true`、事件 `Unequip`（[C L17–L19](diagnostics/CharacterPanel-g5e3-c-run1-20261110-2317.log#L17)） |
| 装备初态 | 窗口开始时 `currentDefaultState=0`、`iState=0`、`iMagicEquipped/iRightHandEquipped/iLeftHandEquipped=0`；随后真实装备单手武器 | 只注入 `WeapEquip` 图事件（[C L114](diagnostics/CharacterPanel-g5e3-c-run1-20261110-2317.log#L114)）；日志中没有真实装备写入，也没有装备变量的写入行 |
| 输入序列 | `0→10` @19.074；`iMagicEquipped 0→1`、`iRightHandEquipped 0→1` @19.568；`iLeftHandEquipped 0→9` @19.568；`10→1`、`iState 0→9` @20.120；`1→12` @25.073；`12→0` @26.330（[R L61–L109](diagnostics/CharacterPanel-g5e3-r-run2-20261110-2312.log#L61)） | `WeapEquip` → step 0 `default=10`、`H2H_Equip`；step 49 起 `default=1`；`Unequip` → step 0 `default=12`；step 21 起 `default=0`（[C L126–L334](diagnostics/CharacterPanel-g5e3-c-run1-20261110-2317.log#L126)） |
| 装备上下文 | 引擎真实装备写入了三个装备变量；`1HM_Equip`/`1HM_Unequip` 在 v91/v92 源捕获中出现（[v92 L189/L505](diagnostics/CharacterPanel-g5e2c-clock-run1-20261111-0026.log#L189)） | `rh` 在卸下段 121/121 行为 0（[C L313](diagnostics/CharacterPanel-g5e3-c-run1-20261110-2317.log#L313)）；装备段没有 `iMagicEquipped`/`iRightHandEquipped`/`iLeftHandEquipped` 的写入或读数 |
| 外部输入 vs 图内输出 | 三个装备变量由引擎实际装备写入；`currentDefaultState` 的转移是图内结果 | 只有图事件；装备变量没有外部写入。历史 v52/S5 已证：单独写 `iRightHandEquipped=1` 或 `bEquipUnequip=1` 不能推进相位（[§2026-10-30/31](#session-20261030)） |

### 三、H2H/1HM 差异的初步归因

1. **不是测量噪声。** 候选 step 0 的 `H2H_Equip` 与源侧的 `1HM_Equip` 是不同资源、不同状态机路径；E3 的 `paired=2` 只证明状态端点可比，不证明装备上下文相同。
2. **候选图没有收到与源相同的装备上下文。** C 运行只用 `inject` 向候选图派发 `WeapEquip`，没有记录到真实装备写入或装备变量写入；`rh` 全程 0 说明候选图值集里的装备变量没有被外部写入（这不能单独证明角色实际是否持械，但足以证明候选图没有拿到源的装备状态）。候选随后在 step 49 进入 `1HM_Idle`/`SCAR` 栈，说明它并不是完全不会选 1HM，而是**装备过渡的首个片段**按候选图的空手初态选了 `H2H_Equip`。
3. **源侧的片段选择不能只用 `iRightHandEquipped` 变量解释。** 源侧 `1HM_Equip` 所在的 equip 边在 19.074 已出现，而 `iRightHandEquipped 0→1` 的 settled 行在 19.568；时间戳本身受 settle 记录影响，不能单独证明先后，但 v52/S5 的变量写负结果已经排除了“只写这个变量就能复现”的方案。
4. **变量回放不能作为替代方案。** v52 的组合轮虽然得到 `default 10→1`，但 `iState` 仍为 0；S5 的 `iRightHandEquipped=1 + bEquipUnequip=1` 两写序列没有推进卸下相位。两者都没有得到与源相同的完整装备相位；历史结论已写明“候选若确需装备上下文，那是独立议题”。

**处置决定（D1 已准备运行档；其余待评审）**：

| 方案 | 内容 | 当前判断 |
| --- | --- | --- |
| D1 真实装备上下文 | 在同一存档/同一初态下，让候选运行具备与源相同的真实 1H 武器（角色实际装备，而非只写变量），再做 equip/unequip 诊断 | **已运行，判负**：实际装备为 1H，候选仍选 `H2H_Equip`；见下文 D1 首轮运行 |
| D2 变量/事件等价回放 | 设计一个经审计的输入序列，证明它能等价于真实装备上下文 | **下一步候选**：在事件前 prime 三个装备变量；须另立仪器范围，先证明最小充分输入 |
| D3 保留差异 | 不补装备上下文，把 H2H/1HM 作为被测差异保留 | 只能用于“候选缺装备上下文”的结论，不能作为姿态等价证据；G5 继续 NOT_READY |

**D1 的诊断判据（不是校准判据）**：在冻结的同一初态下，候选在 `0→10` 边应选 `1HM_Equip`，在 `1→12` 边应选 `1HM_Unequip`，并同时记录 `iMagicEquipped`/`iRightHandEquipped`/`iLeftHandEquipped`；若仍选 `H2H_*`，停在 K2，不进入姿态比较。该诊断只用于定位输入差异，不产生容差、不关闭 G5。

**D1 诊断运行单（运行档已准备；尚未上机）**：

| 项 | 固定内容 |
| --- | --- |
| 存档与初态 | 同一存档；同一把 1H 武器（记录 FormID、来源模组、左右手）；护甲、火把、左手物品保持不变；打开面板前武器已实际装备 |
| 构建与 INI | 匹配 DLL/PDB/INI；`IndependentGraph=inject`、`CandidateInputProbe=true` 且变量名为空、`CandidateUnequipProbe=true`、`CandidateUnequipEvent=Unequip`、`OarContextWatch=true`（只读）；`SourceTimeline`/`SourcePoseCapture`/`SamplingLiveness` off |
| 操作 | 装备 1H 武器 → 打开面板触发候选实验 → equip 窗口结束 → 卸下 → unequip 窗口结束 → 关闭；一次进程只跑一个窗口 |
| 记录 | `clip-ref` step 0/60/120、`CANDIDATE INPUT`、`CANDIDATE UNEQUIP`、`OAR CONTEXT`/`load-bindings`（若开启）以及实际装备状态 |
| 判据 | `step=0` 为 `1HM_Equip`；settled 栈含 `1HM_Idle`/`SCAR_1hmReadyDummy`；unequip 为 `1HM_Unequip`；仍为 `H2H_*` 即 D1 失败，停在 K2 |
| 停止 | `[E]`、崩溃、`source-drift` 非 `none`、重复面板、实际装备状态未记录 |

该运行单只回答“真实装备上下文是否足以改变首个装备片段”，不比较姿态、不产生容差；正式配对仍需 K3–K6 的仪器和新的同代运行。

#### D1 首轮运行（2026-11-11 22:46，v92，**判负**）

工件：[日志](diagnostics/CharacterPanel-g5cal-d1-run1-20261111-2246.log) 97,027 B，SHA-256 `4e48cd5c…c7be6c`；v92 DLL/PDB；D1 运行档 `98ba7a30…`；一次窗口，零 `[E]`，无崩溃；`post-drive source-drift=none`，`unequip-done steps=120`。

| 判据 | 读数 | 裁定 |
| --- | --- | --- |
| D1-A 首个装备片段 | `clip-ref step=0 slot=0 name='Animations\H2H_Equip.hkx'`，binding-index=11046；`clip-steps` 仍是 `H2H_Equip` 的 `.C...F...` 模式（[L163/L341](diagnostics/CharacterPanel-g5cal-d1-run1-20261111-2246.log#L163)） | **FAIL**：仍是 H2H |
| D1-B settled 栈 | step 49 起 `H2H_Idle`+`1HM_Idle`+`SCAR_1hmReadyDummy`；`default 10→1`；卸下 step 21 起 `default=0` | 部分：后续栈含 1HM，但过渡首选仍是 H2H |
| D1-C 实际装备 | 产品路径在面板开启后记录 `ANIMATION ... goal=non-combat-idle family=1h ... drawn=true`（[L520](diagnostics/CharacterPanel-g5cal-d1-run1-20261111-2246.log#L520)），OAR 命中 `Smooth Moveset - 1hm`（[L523–L524](diagnostics/CharacterPanel-g5cal-d1-run1-20261111-2246.log#L523)） | **实际装备为 1H**；候选仍未选 `1HM_Equip` |
| D1-D 图变量 | `CANDIDATE UNEQUIP` 121/121 行 `rh=0`；装备段没有装备变量写入行；`iMagicEquipped`/`iLeftHandEquipped` 本次未记录 | 候选图的 `iRightHandEquipped` 仍为 0；其余两个变量待 D2 仪器补齐 |
| D1-E 身份 | `load-bindings source-count=15203 candidate-count=15203`，两侧同一 binding set（[L92](diagnostics/CharacterPanel-g5cal-d1-run1-20261111-2246.log#L92)）；但 `H2H_Equip` binding 11046 对 C run 1 的 11040、`SCAR` 15075 对 15076 | 本次两侧同 binding；跨 C run 资源身份不一致，K1 仍不闭合 |
| D1-F 完整性 | 121 个 input step + 121 个 unequip step；一次 activation/inject/unequip；`[E]=0` | 通过 |

**结论：D1 判负。** 真实 1H 装备上下文（产品路径已确认 `family=1h drawn=true`）没有让候选图选 `1HM_Equip`；候选仍按自身空手初态选 `H2H_Equip`。因此“让角色实际装备”不足以替代候选图的装备状态输入。

**为什么 v52 的组合轮没有回答这个问题**：`WeapEquip` 的分发在 [independent_graph.cpp:1666](../src/animation/behavior_graph/independent_graph.cpp#L1666) 附近，而 `arm_candidate_input_probe` 在 [independent_graph.cpp:2188](../src/animation/behavior_graph/independent_graph.cpp#L2188) 附近才执行；v52 的装备变量写发生在事件**之后**，事件分发时 `iRightHandEquipped` 仍是 0。它不能证明“事件前写装备变量”无效。

**D2 草案（待评审/实现）**：在 `WeapEquip` 之前写入源侧观察到的 `iMagicEquipped=1`、`iRightHandEquipped=1`、`iLeftHandEquipped=9`（按变量自身 `m_type` 的 typed setter），再发事件；判据仍是 `clip-ref step=0` 是否为 `1HM_Equip`。若仍为 `H2H_Equip`，D2 判负，转更深的状态机/输入审计；不得写源图，不得改事件顺序之外的现有切片。

#### D2 首轮运行（2026-11-11 23:07，v93，**判负**）

工件：[日志](diagnostics/CharacterPanel-g5cal-d2-run1-20261111-2307.log) 97,402 B，SHA-256 `147bba2a…341e92fd`；v93 DLL/PDB；D2 运行档 `842ed93c…0bf3df`；一次窗口，零 `[E]`，无崩溃；`post-drive source-drift=none`，`unequip-done steps=120`。

| 判据 | 读数 | 裁定 |
| --- | --- | --- |
| D2-A prime 写入 | `iMagicEquipped` index=117 INT32 value=1 readback=1；`iRightHandEquipped` 179 INT32 1/1；`iLeftHandEquipped` 180 INT32 9/9；`outcome=ready`（[L120–L123](diagnostics/CharacterPanel-g5cal-d2-run1-20261111-2307.log#L120)） | **PASS**：三项均 written |
| D2-B 事件分发 | `phase=inject event='WeapEquip' id=2 result=dispatched`（[L147](diagnostics/CharacterPanel-g5cal-d2-run1-20261111-2307.log#L147)） | **PASS**：事件未被跳过 |
| D2-C 首个装备片段 | `clip-ref step=0` 仍是 `Animations\H2H_Equip.hkx`，binding-index=11045；`clip-steps` 仍是 H2H 的 `.C...F...` 模式（[L168](diagnostics/CharacterPanel-g5cal-d2-run1-20261111-2307.log#L168)） | **FAIL**：仍是 H2H |
| D2-D 卸下 | `default=12` step 0，step 21 起 `default=0`；`rh=1` 121/121；未出现 `1HM_Unequip` | **FAIL**：变量持久但片段族未变 |
| D2-E 身份 | `load-bindings` 仍为 source/candidate 同一 15,203 set（[L93](diagnostics/CharacterPanel-g5cal-d2-run1-20261111-2307.log#L93)）；`H2H_Equip` binding 11045 vs D1 11046 vs C run 11040，`SCAR` 15072 vs 15075 vs 15076 | 跨 run 资源身份仍不稳定，K1 未闭合 |
| D2-F 完整性 | 一次 activation/inject/unequip；121 + 121 step；`[E]=0` | **PASS** |

**结论：D2 判负。** 三个装备变量都成功写入并保持（`rh=1` 121/121），但候选仍选 `H2H_Equip`；因此这三个变量不是选择 1HM/H2H 装备分支的充分输入。

**下一步 D3（审计，不是继续盲写变量）**：离线 dump 已显示 `0_Master` 的 `root/Root/mg` manual selector 由 `iRightHandType`(var 35) 选支（`0_master_v2.txt` L8687；同表 L41），`iLeftHandType`(60) 选下一级；`weapequip.hkx` 的 right-hand force-equip 分支同时包含 `1HM_Equip` 与 `H2H_Equip`。D3 应先只读捕获源侧 `iRightHandType`/`iLeftHandType`/`Is1HM`/`i1HMState` 在真实 1H equip 边的值（`SourceTimeline=on` + `SourceTransientWatch=true`，或等价只读探针），再决定是否把 `iRightHandType` 作为下一次 prime；不得猜一个值直接写。

源侧只读探针档 `dist/CharacterPanel-g5cal-d3-source-probe.ini`（11,183 B，SHA-256 `4c7ad794…13b0400`；`IndependentGraph=off`、`SourceTimeline=on`、`SourceTimelineGraph=DefaultFemale`、`SourceTransientWatch=true`、`SourcePoseCapture=false`）**已于 2026-11-11 部署**，v93 + D2 三件套备份在 `dist/deploy-backup-20261111-v93d2-v93d3pre/`。运行单：武器先不装备 → F7 开窗 → 装备 1H → 等 1–2 s → 卸下 → 等 1–2 s；只读记录 `iRightHandType`/`iLeftHandType`/`Is1HM`/`i1HMState` 的变化。若窗口被 800 行预算截断，保留原始行并另立定向探针，不猜值。

#### D3 源侧探针运行（2026-11-11 23:19，v93，**未覆盖初始值**）

工件：[日志](diagnostics/CharacterPanel-g5cal-d3-run1-20261111-2319.log) 21,605 B，SHA-256 `0cbe40c3…2b9b71`；v93 DLL/PDB；D3 源侧探针档 `4c7ad794…13b0400`；一次窗口，零 `[E]`，`window-closed reason=expired lines=166`，`snapshots=0 clocks=0`。

| 项 | 读数 | 裁定 |
| --- | --- | --- |
| 图身份 | `DefaultFemale` 652 words / 106 nodes，`selector='DefaultFemale' origin=selector`；`mode=transient`（[L57–L59](diagnostics/CharacterPanel-g5cal-d3-run1-20261111-2319.log#L57)） | 通过：源图正确 |
| 装备段 | `IsEquipping 0→1` + `currentDefaultState 0→10` @35.389；`iMagicEquipped 0→1`、`iRightHandEquipped 0→1`、`iLeftHandEquipped 0→9` @35.886；`iState 0→9` + `10→1` @36.437（[L65–L82](diagnostics/CharacterPanel-g5cal-d3-run1-20261111-2319.log#L65)） | 通过：相位序列与 R run 一致 |
| 卸下段 | `currentDefaultState 1→12` @43.009；`12→0` @44.262（[L89–L111](diagnostics/CharacterPanel-g5cal-d3-run1-20261111-2319.log#L89)） | 通过 |
| 探测变量 | **没有** `iRightHandType`(35)、`iLeftHandType`(60)、`Is1HM`(76)、`i1HMState`(77) 的 transient 行 | **未覆盖**：transient 只记录变化，初始值不在窗口内 |
| 预算/错误 | 166/800 行，`reason=expired`，零 `[E]` | 通过 |

**结论：D3 运行对相位有效，但没有回答 `iRightHandType` 的初始值。** 该变量在窗口内没有变化，或变化被同帧覆盖；不能据此断言它是 0/1。下一步 D3 v2 需要在 `window-open` 做一次只读初始值探针（四个名字各读一次），再决定候选 prime；不猜值。

### 四、控制 A/B 与 K1/K2 关闭条件

**控制 A**：已完成离线固定两组变换的三轮往返（12/12 track 行、120/120 分量，零 missing/dropped/truncated）；它只证明共享采集、六位小数格式和离线解析路径，不证明原生 SDK/ABI、generation、线程或寿命（[离线 A](#c-g5-cal-a-run1-20261111)）。

**控制 B**：仍未就绪。必须先登记一个单层、固定时间、真值可独立确定的原生控制资源，证明只有一个有效层、已知权重和 binding/reference/default-fill，且没有未建模的 IK、物理、OAR 变体或随机输入；资源名、哈希、真值来源、承载路径须在任何控制输出值被查看前写入不可变清单。`SamplePartialTracks` 只能作实现审计入口，不能自行取得 oracle 资格。

| 关闭条件 | 通过条件 |
| --- | --- |
| K1-C1 运行身份 | 两侧使用匹配的 DLL/PDB/build/INI/runtime/OAR/资源哈希；姿态数据来自同一次或明确可配对的 Generate |
| K1-C2 图身份 | 源图确属 controller，记录 actor/graph/generation/request/view；候选记录同一组身份与生命周期 |
| K1-C3 骨架与映射 | 完整骨名、父索引、reference pose、binding/默认填充规则、应有/访问/记录分母 |
| K1-C4 资源与 clip 实例 | 资源完整路径/哈希、OAR 变体、稳定 clip 实例 id（不能只用 leaf name） |
| K2-C1 装备初态 | 冻结武器 Form、左右手、火把、护甲、背包和初始相位；两侧一致 |
| K2-C2 输入账本 | 外部输入内容/顺序/相对时刻、装备变量写入、图内输出分开记录 |
| K2-C3 片段一致性 | 候选在 equip/unequip 段选中与源同族的 `1HM_Equip`/`1HM_Unequip`；残余时序差保留为被测差异 |
| K2-C4 独立控制 | 控制 A 保持；控制 B 在查看数值前完成资源/真值/路径登记 |

全部满足前，K1/K2 维持 **NOT_READY**；不得启动原生校准、不得产生 `tau_p/tau_r/tau_s/epsilon_t`。

### 五、下一步与明确不做

1. **D1/D2 均已判负；D3 源侧探针已跑但未覆盖初始值；下一步 D3 v2 `window-open` 初始值探针。** 不再盲目增加变量写。
2. **D1 结果**：v92 + D1 运行档，一次窗口，实际装备已由产品路径确认为 `family=1h drawn=true`，候选仍选 `H2H_Equip`；`rh=0` 121/121；零 `[E]`、`source-drift=none`。详见下文 D1 首轮运行。
3. **D2 结果**：v93，三项 prime 全部 written/readback 1/1/9，事件正常分发，候选仍选 `H2H_Equip`，`rh=1` 121/121；D2 判负。详见下文 D2 首轮运行。
4. **D3 v2**：已实现并构建为 v94；在 `window-open` 一次性只读源侧 `iRightHandType`/`iLeftHandType`/`Is1HM`/`i1HMState` 的值（不依赖 transient 变化），待部署/上机后再决定下一次 prime；不得猜值。
5. **控制 B 登记**：与控制 A 一样，先写清单再看值；没有合格资源就保持 NOT_READY。
6. **K3–K6 与 B3/B4**：候选 `e_post_sample` 快照、源侧等价完成边界/generation、完整计数/寿命、`58423 return.out` 对 `63581 memcpy.src`/`poseLocal` 的逐分量对账；经评审后再实现。
7. **之后**：原生控制 B → 时间量化 → 容差推导 → CAL1–CAL7 → 联合验收 → 协议冻结 → 新正式运行。

**明确不做**：不新增 detour、不写源图、不把 D2 实验代码伪装成产品改动、不在未评审时部署新构建、不把旧 E3/E4 日志当校准样本、不从源候选差反算容差、不把 H2H/1HM 说成测量噪声、不用删帧/换资源/偏移时间消除差异。

### 六、D2 仪器范围（预注册，2026-11-11；已实现、部署并运行，判负）

**目标**：只回答“在 `WeapEquip` 之前补齐源侧三个装备变量，是否足以让候选选中 `1HM_Equip`”。不比较姿态，不产生容差，不关闭 K1/K2。

| 项 | 规定 |
| --- | --- |
| 开关 | `[Experiment] CandidateEquipPrime=false`，默认 off；要求 `IndependentGraph=inject`，其他模式忽略 |
| 写入顺序 | 在 `WeapEquip` 分发前，按固定表写 `iMagicEquipped=1`、`iRightHandEquipped=1`、`iLeftHandEquipped=9`；按变量自身 `m_type` 选 `SetGraphVariableBool/Int` |
| 失败 | 任一名字缺失、类型不支持或 setter 失败 → 逐项记 `phase=equip-prime ... result=...` 与 `outcome=skip`，**不分发事件**，该轮判无效 |
| 日志 | 每个变量一行 `name/index/type/value/result/readback` + 一行 summary；无新 detour、无源图写入、无产品路径改动 |
| 成功 | 3/3 `result=written`，事件分发，正常 121 步 equip + 121 步 unequip |
| 判据 | D2-A `clip-ref step=0` 为 `1HM_Equip`；D2-B unequip 为 `1HM_Unequip`；D2-C 3/3 written；D2-D 一次窗口、零 `[E]`、`source-drift=none` |
| 停止 | 任一写入失败、事件未分发、崩溃、`[E]`、drift、多次窗口 |

| 代码入口 | 用途 |
| --- | --- |
| [candidate_equip_prime_policy.h](../src/animation/behavior_graph/candidate_equip_prime_policy.h) | 固定三项 name/value 表；源侧 R run 2 的顺序与值 |
| [independent_graph.cpp](../src/animation/behavior_graph/independent_graph.cpp) | `apply_candidate_equip_prime` 在 `event_dispatch` 之前调用；失败跳过事件 |
| [settings.cpp](../src/animation/settings.cpp) / [settings.h](../src/animation/settings.h) | `CandidateEquipPrime` 解析、日志与访问器；默认 off |
| [behavior_graph_equip_prime_test.cpp](../tests/behavior_graph_equip_prime_test.cpp) | 固定三项顺序/值/唯一性 3 例 |
| [config/CharacterPanel.ini](../config/CharacterPanel.ini) | 默认 off 的样例键与注释 |

**实现状态（2026-11-11）**：策略测试 3/3 通过；Release 构建为 v93，identity `CharacterPanel-scenegraph-v93-g5cal-d2-equip-prime-7e878cc04a-cl94faaed0c6-20261111T150314Z`，DLL 1,835,520 B SHA-256 `638e562f…ef0ac05`，PDB 49,614,848 B `0b75c57f…45d073f`。D2 运行档 `dist/CharacterPanel-g5cal-d2-candidate.ini`（10,689 B，SHA-256 `842ed93c…0bf3df`，`CandidateEquipPrime=true`）已于 2026-11-11 部署，v92 + D1 三件套备份在 `dist/deploy-backup-20261111-v92-v93pre/`；**已上机并判负**，见下文 D2 首轮运行。

**明确不做**：不写源图、不新增 detour、不改事件顺序之外的切片、不把 D2 当产品路径、不产生 `tau_p/tau_r/tau_s/epsilon_t`。

### 七、D3 v2 源侧 `window-open` 初始值探针（2026-11-11，已实现并构建为 v94，待部署/上机）

**目标**：补上 D3 未覆盖的初始值——在源时间线窗口打开时，一次性读取 `iRightHandType`(35)、`iLeftHandType`(60)、`Is1HM`(76)、`i1HMState`(77) 的当前 word；不依赖 transient 变化，不写源图，不新增 detour。

| 项 | 规定 |
| --- | --- |
| 开关 | 复用 `SourceTransientWatch=true`；只在 transient 模式输出，避免改变历史 settled 窗口 |
| 输出 | 一行 `SOURCE TIMELINE probe actor=... values='iRightHandType=... iLeftHandType=... Is1HM=... i1HMState=...'`；缺名记 `missing` |
| 预算 | 该行不消耗 800 行事件预算；窗口仍按原规则关闭 |
| 判据 | D3v2-A 行存在且四名各出现一次；D3v2-B 图仍为 `DefaultFemale` 652/106；D3v2-C 零 `[E]`；D3v2-D 值与随后 equip 边一致（只记录，不反推） |
| 不做 | 不写任何变量、不创建候选图、不比较姿态、不产生容差 |

**实现状态（2026-11-11）**：策略表在 [source_timeline_probe_policy.h](../src/animation/behavior_graph/source_timeline_probe_policy.h)，探针在 [source_timeline.cpp](../src/animation/behavior_graph/source_timeline.cpp)；离线测试 `source_timeline_probe_test` 2/2 通过。Release 构建为 v94，identity `CharacterPanel-scenegraph-v94-g5cal-d3-window-probe-20035abe4a-cl94faaed0c6-20261111T152720Z`，DLL 1,837,056 B SHA-256 `1c3b67ab…6018372`，PDB 49,631,232 B `0fc936bb…c886033`。**已于 2026-11-11 部署**，v93 + D3 三件套备份在 `dist/deploy-backup-20261111-v93d3-v94pre/`；**尚未上机**。

**证据索引**：[E3 v2 账本](#c-g5-e3-v2)、[E4 契约](#c-g5-e4)、[E2-C 契约](#c-g5-e2-c)、[离线 A](#c-g5-cal-a-run1-20261111)、[R run 2](diagnostics/CharacterPanel-g5e3-r-run2-20261110-2312.log)、[C run 1](diagnostics/CharacterPanel-g5e3-c-run1-20261110-2317.log)、[v92 clock run](diagnostics/CharacterPanel-g5e2c-clock-run1-20261111-0026.log)。
