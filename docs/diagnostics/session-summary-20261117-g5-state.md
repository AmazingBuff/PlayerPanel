# 2026-11-16/17 会话总结：门 5 → 失败归因 → STATE 探针负结果

**一句话状态：门 5 两侧正式采集完成，但 G4P-7 FAIL；后续 C-G5-ATT/SETUP/CLOCK/STATE v1–v4 已把失败拆成静态附件 H2、候选时钟/状态不足和初始过渡离轨三条轴。G5 未通过，G6 后置。**

## 一、门 5 正式运行

记录：[g5-gate5-20261116/README.md](g5-gate5-20261116/README.md)

| 项 | 结果 |
| --- | --- |
| 候选 | v103 `k5-inject`，249/249，equip 120/120，unequip 129/129，K5-1..8 PASS |
| 源 | v103 `k5-source`，849/849，20.017 s F7 窗口，K5-1..8 PASS |
| 资源 | 六族 data FNV PAIRED；IDUN 候选 1 / 源 1 单列 |
| G4P 配对 | 248 对，`no_partner=0`，edge 12，manifest/summary 确定性成立 |
| G4P-1..6,8,9 | PASS（G4P-6 锚点 97 vs 96 已由 IDUN 2→1 解释） |
| **G4P-7** | **FAIL**：`E_p/E_r/E_s` = 3,826 / 16,755 / 352；P99 `5.5912 / 0.0457227 / 0.000525653`；`dt_excluded_pairs=0` |

工具修复：`gate4_pair.py` 读取 CPK5S identity 少 2 字符；修复后旧/新 manifest 唯一差异是 `/inputs/source/identity`，姿态数值不变。

## 二、失败归因

### 2.1 C-G5-ATT v1（事后）

[g5-attribution-20261117/README.md](g5-attribution-20261117/README.md)

| 组 | 骨数 | `E_p` 超 tau | 其中 static_both |
| --- | ---: | ---: | ---: |
| body | 88 | 1,226 | 0 |
| attachment | 11 | 2,600 | 1,240 |
| auxiliary A17 | 17 | 0 | 0 |

静态 5 骨：`Quiver`、`WeaponAxe`、`WeaponBow`、`WeaponDagger`、`WeaponSword`；两侧 span ≤1.7e-4，但跨侧平移差 3.49–5.59。

### 2.2 C-G5-SETUP v1

[g5-setup-audit-20261117/README.md](g5-setup-audit-20261117/README.md)

- 源/候选 `animationSkeleton`、`data`、`mirroredSkeleton` 同址；12 骨 `referencePose` 逐字相同。
- **H1（setup/bind 不同）证伪。**
- **H2（源真实附件状态）得到支持**：静态 5 骨 source−ref 2.77–4.20，候选−ref ≤1.62e-4。
- v105 构建/运行；唯一 `[E]` 为装载期 OAR 豁免行。

### 2.3 C-G5-CLOCK v2

[g5-clock-step-20261117/README.md](g5-clock-step-20261117/README.md)

| family | 候选唯一 local_time | 多记录组/全姿态唯一 | 源 seq 使用数 | 最大复用 |
| --- | ---: | ---: | ---: | ---: |
| equip | 17 / 98 | 8/8 | 12 / 97 对 | 20 |
| unequip | 21 / 129 | 10/10 | 16 / 129 对 | 15 |
| idle | 3 / 22 | 2/2 | 3 / 22 对 | 15 |

同一 local_time 下的多记录组全部是不同完整姿态（例如 equip 0.208333 的 11 条有 11 个不同姿态），因此 **clip local_time 不是充分相位**；K5-5 层/权重/混合是直接缺口。

### 2.4 C-G5-STATE v1

[g5-state-20261117/README.md](g5-state-20261117/README.md)

- 所有多记录组 PC1 方差占比 0.9983–0.999998，投影随记录顺序单调 → 单标量混合曲线。
- equip 第一组最近源轨迹残差 median 0.324 / max 0.497；后续 equip ≤0.0856；unequip 后段/idle ≤1.5e-5/3.3e-5。
- 解释：equip 第一组是离轨初始过渡；unequip/idle 主要是配对时钟问题。

## 三、STATE 探针与负结果

### 3.1 STATE v2：源 poseLocal bootstrap

[g5-state-bootstrap-20261117/README.md](g5-state-bootstrap-20261117/README.md)

- v106 一次性复制源 poseLocal 116/116，`equal=1`。
- 首帧 Generate 覆盖全部姿态：v106 与门 5 候选的 **288,840/288,840 分量完全相同**；G4P summary 不变。
- **SB-5 FAIL**：初始状态在图内部，不在 pose 缓冲。

### 3.2 STATE v3：nodeInfo / blender 对象前缀

[g5-state-probe-20261117/README.md](g5-state-probe-20261117/README.md)

- v107/v108：`STATE PROBE` 2,729 行、`STATE OBJECT` 12 行。
- 同 localTime 组中间 3–12 步骤没有 nodeInfo 变化；blender 对象前缀只在少数栈切换点变化。
- **SP-5 未找到**：标量不在 `hkbNodeInfo`，也不在 blender 对象前 0x100。

### 3.3 STATE v4：child / internal-state 指针扫描

[g5-state-child-20261117/README.md](g5-state-child-20261117/README.md)

- v109：12 个 blender 节点的 0x200 指针扫描全部 `children=0 internal=0x0`。
- **CP-2 FAIL**：child 不是 blender 对象直接持有的 runtime 对象；可能位于 `allPartitionMemory`/模板映射或 HKX 数据。

## 四、当前裁定

- **门 5/G4P-7 FAIL 不变**；G5 未通过，G6 后置。
- 不能通过删轨、放宽 tau、把 `epsilon_t` 当姿态噪声或 post-hoc 改锚点解决。
- 失败至少分成三条独立轴：
  1. 静态附件 H2（1,240 次）；
  2. 候选时钟/状态不足（K5-5；同 local_time 多姿态）；
  3. equip 第一组初始过渡离轨（max 0.497）。
- 安装位已恢复 v104；源码工作树 clean。

## 五、提交与工件

主要提交（时间顺序）：

- `c01a8aa` fix CPK5S identity
- `819269e` / `8bba6c7` 门 5 候选/失败记录
- `417a01d` / `f24a7af` attribution 工具/报告
- `96d60fc` / `c1759a6` / `4573987` setup 审计
- `f958733` / `c45d87a` / `794d3cb` clock-step 审计
- `070cb4b` state-space 审计
- `ee50e97` / `131832a` / `b9160a1` bootstrap 实现/负结果
- `705a3b1` / `953ec27` / `42941e3` state probe 实现/负结果
- `6444a21` / `8ceb48a` / `4576a15` child/internal 探针/负结果

工具：`tools/gate4_pair.py`、`tools/gate5_attribution.py`、`tools/gate5_clock_step.py`。

## 六、下一入口

1. **离线 HKX 图结构解析**（推荐）：解析 `0_Master.hkx` 的 `hkbBlenderGenerator`/`hkbBlenderGeneratorChild` 权重与层级，建立静态图结构；不依赖再上机。
2. `hkbBehaviorGraph::allPartitionMemory` / 模板映射只读审计，确定 runtime child 数据所在表。
3. 静态附件 H2 轴与 body/K5-5 轴继续独立处理；在状态标量拿到前，不把任何探针结果接入 G4P-7 判据。
