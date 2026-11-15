# 会话总结：2026-11-15 会话（门 4 协议全链 + CAL2 至部署，11 个提交）

> **2026-11-15 后续更新**：v104 部署后已完成 **run 1–3/3**；`tools/cal2_adjudicate.py` 已实现/测试，run 1–3 在 [CAL2 §十二](g5-cal2-20261115/README.md#cal2-c2e-oar-addendum) 的 OAR 精确豁免下均 C2-A/B/C/E/F PASS，三进程两两 119,790/119,790 逐位相同；**C2-D PASS，K2-C4 升 grid READY**，CAL1 原生校准为下一阶段（控制 B 资格已就绪；Track 2 已把 K1-C4 反转为六族 PAIRED，K5-5 层/权重/混合缺项仍是剩余资格边界）。本文“部署后未进行任何游戏会话”只描述本会话结束时的状态。

**范围**：本会话从 `34fea79`（Track 2 收官）之后开始，共 **13 个提交**（含 2 个会话总结/修正提交）。分支 `oar-idle3`，结束领先 `origin` **89 提交、未推送**，工作树 clean。完成两条工作包：**门 4 配对/比较协议全链（预注册 → 离线工具 → dry-run）** 与 **CAL2 控制 B 网格协议（预注册 → 实现 → v104 构建 → 真值转储 → 部署）**。部署后**未进行任何游戏会话**——3 进程上机是下一会话的入口动作（用户执行）。

## 一、门 4 轨（C-G5-G4 v1，全部完成至 dry-run）

| 提交 | 内容 |
| --- | --- |
| `472d753` | **C-G5-G4 v1 预注册（评审通过）**：Track 2 同资源环境下的相位括号配对协议——逐记录钟族优先级（`1HM_Equip > 1HM_Unequip > 1HM_Idle > MT_Idle > MLh_Idle > dummy`）+ 域内 nearest-clock（元数据规则、独立于 pose 误差）、IDUN 整条排除、对账恒等式、G4P-1..9 判据、离线工件预算；**不做同刻逐代配对**（K3 收官裁定维持另立契约）；`tau_*/epsilon_t` 与 `Δt_clock` 处置 = PENDING-CAL 槽位；K5-5 缺项清单带入层/权重/混合轴阻塞。四处入口同步 |
| `8c98c77` | **§十四 实现预注册 + [tools/gate4_pair.py](../../tools/gate4_pair.py) 实现**：metadata-only 默认（pose 字节零解码），`--tolerances` 是 pose 解码唯一开关；离线测试 T1–T12 全 PASS（34 断言，期望值解析计算） |
| `570aacb` | **dry-run 登记（§十五）**：Track 2 Run 1/2 工件 metadata-only 执行——候选 **247/247 配对**（E1 96 / idle 22 / U1 129，三锚全中）、两侧对账恒等式闭合、零 wrap、零 no-clock/no-partner、K5-7 PASS、同参调用 manifest/summary **逐字节一致**；工件（manifest 111,548 B `4a3dd0ae…` + summary）入档 |
| `1f2e1d6` | 状态头同步 |

**元数据发现（登记于 §15.3，移交时钟轴 PENDING-CAL 槽位）**：候选过渡 clip localTime **阶梯/滞后形态**（equip 81/97、unequip 108/128 相邻同值 vs 源侧 8/71、9/117），但两侧 clip 总相位跨度端点逐位一致（equip 终点同 1.4333、unequip 同 2.0）；`dt_clock` 全对分布 median 4.7 ms / p95 11 ms / max 17.8 ms；家族集合差仅 1/247 对。**不构成规则歧义、不触发协议升版。**

## 二、CAL2 轨（C-G5-CAL2 v1，完成至部署）

| 提交 | 内容 |
| --- | --- |
| `fd48c7d` | **C-G5-CAL2 v1 预注册（评审通过，环境 = `AE-Track2` 档）**：控制 B 从 B2 的 9 固定时间单发采样（bounded READY，1 进程）重组为 [CAL v1 §五.1/§五.2/§五.6](../../independent-behavior-graph.md#c-g5-cal-v1) 完整形状——每进程 baseline(t=0) + 120 完成代（注入链 equip 段提供每代结构）× 每代 2 读，**预定网格时间 t=k/60 直采（不取图 clip 时间，隔离候选时钟阶梯）**，3 独立进程；判据 C2-A..F；全过 = K2-C4 升网格 READY。四处入口同步 |
| `7db0215` | **§九 实现预注册**：`ControlSampleGrid` 键（off 默认、off 路径与 v103 逐字节一致、armed 抑制九时间采样并出声明行）、6 代码触点、CGRD v1 工件布局（112 B 头 + 121×4,004 B 记录 = 484,596 B）、~135 行日志、测试 G1–G4、HkxTruth 加性 BOUNDS 节、C2-C 界规则（B2 公式逐字 + 跨块保守并集） |
| `d61a6e1` | **网格仪器实现**：121 时间表（float64→float32 规则生成；[1] 与九时间表 1/60 逐位同、[60]=1、[120]=2.0）、每代载体 FNV 重验 + 双读 memcmp + 四点 `__rdtsc` + 首读位值入 CGRD 缓冲（close 时一次写出）；**离线测试 8/8 PASS**（G3 对全部 120 个 k 重算逐位一致） |
| `cb39d4e` | **v104 干净重建 + 网格真值转储登记（§十）**：DLL 2,025,984 B `7d270e7d…`、PDB `b2232180…`、身份 `…-v104-g5cal2-grid-d61a6e19b9-cl94faaed0c6-20261115T073547Z`（提交后重配置，无 `+dirty`）；[网格真值](mt_idle_female_truth_grid121.txt) 121×99 TRACK + TIME 121 + BOUNDS 6 块 × 99 轨 + `BITS_FNV1A64 BE67D994E6042E31`（SHA-256 `5e20fcaa…b4e3`）；量化 pos_q=1（QT_16bit）/ rot_q=3（QT_40bit）与 B2 公式一致；**重叠锚点自检 0 失配**（与 9 时间真值共享的三时间 × 99 轨逐位相同） |
| `2d8003a` | **build.md v104 节 + 运行档派生**：`dist/g5-cal2-20261115/CharacterPanel-g5cal2-grid-inject.ini`（`bbfe2b8b…`，k5-inject 基底 → K5 三键 off + ControlSampleGrid=true；19 键逐键断言） |
| `7f31813` | 状态头同步 |
| `7c396ab` | **部署登记**（见下） |

**本地 HkxTruth 补丁（gitignored，解码逻辑零改动）**：解压器逐轨 `TrackBBOX` 存储（赋值点即既有读取点）+ `DumpBounds` + main.cpp BOUNDS 节（tmpfile 中转回写文件）；RTTI 禁用 → 掩码驱动 static_cast。补丁脚本（`build/g5-cal2-20261115/patch_truth_tool*.py`、`fix_main_doc.py`）为一次性本地工件，不入仓。

## 三、当前 MO2 / 部署状态（**已部署、未上机**）

- 安装位 `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\`：**v104 三件套已装**（DLL `7d270e7d…` / PDB `b2232180…` / INI `bbfe2b8b…` = CAL2 grid 档），装后哈希逐位核对、无旁路文件、INI 键位与 DLL 身份串在安装位复验通过。
- 备份：`dist/deploy-backup-20261115-v103-v104pre/`（v103 DLL `8c3857a3…` + PDB `49581217…` + INI `21bd938b…` k5-source 档，与登记逐位一致）。
- 主档/Track 2 档的 MO2 modlist 未动；本次部署只换插件三件套。

## 四、下一步（下会话入口）

1. **用户执行 3 个独立进程**（协议与判据见 [build.md v104 节](../../build.md#v104-g5cal2-grid-20261115)）：每进程启动游戏 → Save116 → panel 窗口一次走完 v96a 链 → 退出；判据 = `CONTROL GRID phase=manifest`（times=121 baseline=1）+ `phase=carrier`（唯一、三 FNV 逐位）+ 121 行 `phase=gen`（全 `two_read_equal=1 identity_ok=true`）+ `phase=close status=complete generations=121`；零 `[E]`、equip 120/120、unequip completed=129。**任一代 identity_ok=false 或 close=invalid 即停手保留现场。**
2. **`tools/cal2_adjudicate.py` 尚未实现**——契约已在 CAL2 §9.7 冻结（读 CGRD 工件 + 网格真值 + BOUNDS 节 → C2-B/C2-C 逐分量裁定 + 三进程跨进程差 + 对账）。上机后先实现（实现即用前登记 SHA-256）再裁定；C2-A..F 全过 = **K2-C4 升网格 READY**，CAL1 控制 B 资格就绪。
3. **门 4 侧等待**：CAL1 校准产出 `tau_*/epsilon_t` → 填充 PENDING-CAL 槽位 + 联合验收 = 门 4 完整冻结 → 门 5 正式运行（gate4_pair `--tolerances` 解锁 pose 解码）。
4. **既挂（用户决策）**：87 提交 push 与否；工作区 AGENTS.md 的 CharacterPanel 条目仍停留在 2026-10-31 交接（可选更新）。

**不产生** `tau_p/tau_r/tau_s/epsilon_t`；不关闭姿态等价/G5/G6；K5-5 缺项清单（层/权重/混合轴）维持阻塞；装载 teardown NO-GO 不变；Track 1/Track 2 历史裁定全部原样保留。
