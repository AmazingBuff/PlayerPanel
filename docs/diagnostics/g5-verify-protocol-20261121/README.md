# C-G5-VER v1：新正式验证协议（2026-11-21 预注册；2026-11-22 冻结）

> **状态**：**v1 已冻结（2026-11-22，用户评审通过）**。整合 C-G5-TRANS §八 全六项、C-G5-CHAIN v1、C-G5-EQ v1 与三项开放项决断（O-1/O-2/O-3，2026-11-22）。冻结时批准的四项判断：① 比较单元 = burst 末步 + clip 末段饱和窗（O-1）；② 源侧 = 独立源侧会话，两次会话结构（O-2）；③ K2-C1/K5-5/A17 为限定条件继承，裁定语带 limited 资格（O-3，见 [O-3 决断](../g5-o3-threshold-chain-20261122/README.md)；O-2 见 [O-2 决断](../g5-o2-modality-20261122/README.md)，O-1 见 [O-1 分析](../g5-o1-pairing-granularity-20261121/README.md)）；④ 阈值冻结继承（`tau_p/tau_r/tau_s/epsilon_t` 原样，不放宽不重推）。**预期产出形态**：机械 FAIL + 全归因分账（tau_r 位精确级，时钟阶梯残差必然超差），不是干净 PASS；残差量级的产品接受度与容差契约属后续，不在本协议内。§九 实现已授权（离线）；上机待运行档就绪后另行安排。

## 一、目的与主张形状

新正式验证 = 门 5 姿态对照的重新执行（G4P-7 的历史 FAIL 不改写；本协议是新裁定）。主张形状：

> **在 EQ-C1..C5 全部成立、声明差异集与排除窗按本协议处理后，候选回放姿态与源姿态在等值主张集上的逐分量误差 ≤ CAL 推导阈值，且 residual 集的差异如实计入。**

| 集合 | 定义 | 处置 |
| --- | --- | --- |
| **等值主张集** | 87 条主轨全通道 + EQ-DECL-1 五轨的 translation（t45 除外）+ EQ-DECL-2 七轨的 rotation | 超阈值 ⇒ **FAIL** |
| **声明差异集** | EQ-DECL-1（t30/31/58/59/45 rotation）、EQ-DECL-2（t60–66 translation） | 不计 FAIL；逐轨实测值必须记录并比对基准量级 |
| **residual 集** | t44 恒定平移、手指微差、C″（unequip 域右臂差）、MLh 分支克隆缺口、时钟阶梯 | **照常计 FAIL**；逐会话归因记录 |

## 二、资格链与前置（引用既有账本，不重裁）

1. CAL 阈值链资格——**已决断（O-3，2026-11-22）**：`tau_*`（CAL1-B）/`epsilon_t`（TIME v1，0.0333960 s，11-16）/99-17 覆盖结构（G4-COV）/时钟轴（G4-CLOCK）齐备；K2-C1 PARTIAL 与 K5-5 缺项为**显式传递的限定条件**（不阻塞本有界主张，裁定语带 limited 资格），继续阻塞完整 CAL1/门 3 与全量等价主张；见 [O-3 决断](../g5-o3-threshold-chain-20261122/README.md)。
2. K3（同代快照）两侧各自通过；K5-5 未闭。
3. G6 后置不变（克隆收敛缺口三连为其输入证据：1HM 分支滞留、缺 MLh 分支、缺 WeaponAdjustment 节点——[C-G5-CHAIN run1/run2](../g5-idle-chain-probe-20261121/README.md)）。
4. 本协议运行前，C-G5-EQ v1 必须已评审通过（其 §八"实现影响面"含 v121 探针复用，无新 detour）。

## 三、比较协议（整合项）

### 3.1 配对（2026-11-22 O-1 采纳修订；依据 [O-1 决断分析](../g5-o1-pairing-granularity-20261121/README.md)）

- **比较单元 = burst 末步**：burst = 文件相邻、同钟族、同实例（stable_id）、同观测 clip localTime 的候选记录。burst 内候选 pose 以 1/60 步推进而报告钟值保持（正式批次实测：249 记录 → 41 burst，尺寸 1–15；稳态 idle 每步 ~0.5°、装备/卸下快速段每步最高 ~10°）。**burst 末步是与配对源帧相位对齐的成员**——实证：正常播放 burst 逐成员 vs 源误差单调递减、argmin=末步 15/15；idle 非 C′ 漂移 7.5°→1.25° 单调衰减至末步；burst 边界相位连续恰差 1 步。mid-burst 成员相位滞后可达 (n−1)/60 ≈ 0.23 s，与源帧比较属配对伪影（正式批次 mid-burst 188 对承载 tau_p 76% / tau_r 79% / tau_s 72%），不进等值判定。每对等值判定在"末步 pose"与源帧之间做；多对一记账保留（burst 内其余步照记不判）。等值集实测规模：equip 17 / unequip 21 / idle 3 = 41 端点（叠加排除窗后 38，见 3.2）。
- **相位残差界定**：端点相位不确定性由配对 `dt_clock ≤ ε_t` 界定（本批 ≤0.0178 s）；**不引入 rigid 时钟锁存模型**（逐 burst ΔL 簿记混合：≈n_next/60 拟合 20、≈n_prev/60 拟合 15、半帧残差 16——机制留开放，不影响末步选择的实证结论）。残差集"时钟阶梯"条目按此界定读取（端点量化 ≤dt_clock，mid-burst 相位滞后不进等值集）。
- **过渡敏感段**（两侧任一处于混合窗）：由 EQ-W1a/W1b 排除窗覆盖（见 3.2），窗内数据照记不判；原"过渡敏感段 clip 相位锚定"机械不再单列，由 burst 末步单元 + 排除窗承担。

### 3.2 排除窗（从 FAIL 计数排除，数据照记）

| 窗 | 界定 | 依据 |
| --- | --- | --- |
| **EQ-W1a 源侧混合窗** | 每命令**双 clip 共存帧集合**（按贡献 clip 出现/消失判定，帧率波动下按 clip 时钟而非记录数） | [源侧过渡语义](../g5-source-blend-20261121/README.md)：四窗实测 ≈0.2 s、钟形路径 |
| **EQ-W1b 候选侧混合窗** | 每命令 injected 事件后 **completed 1–24**（尾界经 O-1 复核**维持 24**：端点集 comp>12 → 439/1,792/0 vs comp>24 → 407/1,661/0，12→24 仅移除 ~30/~130）；**另含每贡献 clip 的末段饱和段**（提案，随冻结评审确认）：burst 尺寸 ≥2 且报告钟值 ≥ duration − 1.5 帧的段——该段为 A/B 混合机制（钟值冻结、pose 推进，与注入 ramp 同机制），末步锚定失效（unequip 末段 argmin=m12 而非末步；equip 末段端点 66.8°）；本批命中 equip lt=1.4250/1.4333、unequip lt=1.9833 三个 burst（约 3 端点/会话，等值集 41→38） | 同上 §二 + 重判数据 + [O-1 分析 §六.3](../g5-o1-pairing-granularity-20261121/README.md) |
| **EQ-W2 声明差异集** | EQ-DECL-1/2 的轨×通道 | C-G5-EQ v1 §三 |

排除 = 不进 FAIL 计数；**窗口内逐对误差、逐轨值全部保留在明细里**（rejudge 模式），排除不等于删除。

### 3.3 会话适用性判据（每会话先复测，任一失败 ⇒ 本会话对声明集不适用）

EQ-C1 状态同域 / EQ-C1a MLh 家族集一致 / EQ-C2 机制在场 / EQ-C3 候选忠实 / EQ-C4 节点在场性 / EQ-C5 无新恒差——定义见 [C-G5-EQ v1 §四](../g5-eq-contract-20261121/README.md)；测量点复用 v121 IDLE CHAIN 探针模式（census + 三方对照）。

### 3.4 判定输出（每会话）

机械计数 + 分组明细，不做文字裁定：等值集超差计数与逐轨排名、DECL 集逐轨实测值与基准比对、residual 集逐轨计数、排除窗内统计（单独列示）、EQ-C1..C5 各自结果、配对账本（对数、多对一结构、相位差分布）。裁定措辞由运行记录写（与 gate-5 惯例同），**且必须携带限定范围资格与未闭合项清单**（完整 CAL1/门 3、K5-5、K2-C1、A17 独立控制、整体姿态等价——`CALIBRATED (limited)` 同形式；见 [O-3 决断 §三](../g5-o3-threshold-chain-20261122/README.md)）。

## 四、会话流程（草案）

| 阶段 | 内容 | 仪器 |
| --- | --- | --- |
| 0 | 环境与身份：干净树构建（标签含提交哈希）、部署断言（逐键）、DLL/PDB/INI 哈希互核 | 既有流程 |
| 1 | census 差集核对（EQ-C1/C1a/C4） | IDLE CHAIN CENSUS |
| 2 | 三方对照快照（EQ-C2/C3；持武器 idle + 徒手 idle 各 ≥2 view） | IDLE CHAIN 探针 |
| 3 | 回放 + 实机装备序列（与 gate-5 同构）+ K5 双侧捕获 = **两次独立会话**（O-2 已决断）：候选 inject 会话（面板开，`IndependentGraph=inject` + `K5FullSample=true` + `SamplingLiveness=off`）与源侧独立会话（**面板关**，`IndependentGraph=off` + `SamplingLiveness=on` + `K5FullSample=true` + `K5Reconciliation=true`，实机装备序列在 20 s 窗口内执行）；注入序与实机序分别记 | K5FullSample |
| 4 | 离线配对与判定（3.1–3.4；EQ-C5 逐轨归因复核） | gate4_pair 扩展 + rejudge 模式 |
| 5 | 回传：完整日志、K5 工件、配对账本、分组明细、身份链 | 既有惯例 |

## 五、冻结前必须解决的三个开放项

| # | 问题 | 现状 | 选项 |
| --- | --- | --- | --- |
| **O-1 配对粒度与驱动突发** | 候选回放按突发推进（~11–12 步/墙帧；clock_x/cand_lt 突发内恒定），突发内候选 pose 逐步变化而 clip 相位不可分辨——过渡窗内的候选步没有可配对的源相位；burst 末步 pose 才有源对应 | 2026-11-21 实测（clock_x 突发结构） | (a) 比较单元 = **burst 末步**（等值集收缩到 ~22 相位/段）；(b) 每步比较 + 把突发内 ramp 步声明为回放自身粒度（新 EQ-DECL 项，须评审）；(c) 驱动语义改造（去突发，1 步/墙帧）——行为实现改动，另立契约 |
| **O-2 模态与暂停语义** | **已决断（2026-11-22）**：原"两次会话语义未统一"是会话结构误读——gate-5 源侧 849 条来自独立源侧会话（面板关），与 run2 面板开显示窗的冻结读数分属两种模态；正式验证源侧 = 独立源侧会话（gate-5 结构沿用，更新语义已被其记录证明） | [O-2 决断](../g5-o2-modality-20261122/README.md)（离线，无需探测） | 面板开显示窗内源图冻结为边界事实；单会话双捕获不可行 |
| **O-3 阈值链** | **已决断（2026-11-22）**：阈值链完整——`epsilon_t` 已由 TIME v1（11-16）关闭（本行原引用 CHANGELOG 11-15 为陈旧状态）；K2-C1/K5-5/A17 = 限定条件继承 | [O-3 决断](../g5-o3-threshold-chain-20261122/README.md) | 数值冻结继承不旁路；裁定语带 limited 资格 |

> **O-1 已决断（2026-11-22）：采纳选项 (a)**，修订并入 §3.1/§3.2。依据：[配对粒度决断分析](../g5-o1-pairing-granularity-20261121/README.md)——比较单元 = burst 末步（正常播放 burst argmin=末步 15/15 实证）；mid-burst 相位伪影占正式超差 72–79%；(a)+EQ-W1b 端点集 33 对剩余超差与既有 DECL/残差结构一致（DECL2 占 tau_p 57%、残差 tau_r ≤1.2° 阶梯形）。同日补充验证：equip 末段（c87–98）已被 EQ-W1a 双 clip 共存覆盖；unequip 末段（c114–128）为单 clip A/B 混合保持，末步锚定退化 → EQ-W1b 增加结构性"clip 末段饱和段"判据（§3.2，随冻结评审确认）。残差集数值（C″ 剖面、手指微差）在新协议判定步骤改用端点集重推导（O-1 分析 §六.5）。

## 六、停点与无效性

EQ-C1..C5 任一失败、非有限值/读取失败影响覆盖、源漂移/崩溃、声明集外新恒差（EQ-C5）——停止并保留记录回评审。不得：事后扩声明集、把 residual 升格豁免、放宽/绕过 CAL 阈值链、改写 G4P-7 与既有裁定。

## 七、实现影响面（2026-11-22 冻结后授权实现；范围以 §九 为准）

1. 配对工具扩展：clip 相位锚定 + burst 末步判定（O-1 选型后）。
2. IDLE CHAIN 探针 SUMMARY 聚合修复（逐 clip 覆写缺陷；行数据已完整，修复为可选）。
3. 源侧模态探测（O-2）：复用现有日志键，无新 detour。
4. 批次清单/账本模板按 §三.4 扩展。
5. 无新增 INI 键、不改 CPK5/CPK5S 布局。

## 八、边界

- 单运行时 AE 1.6.1170、`AE-Track2`、DefaultFemale 骨架、单存档族；换骨架/装备族须重解析轨集与基准。
- 本协议不关闭 G6、K5-5、K1/K2 剩余项；不证明整体姿态等价；G4P-7 历史裁定与其工件不改写。
- EQ-DECL 基准值绑定 2 状态 × 1 装备族会话；其它武器/左手配置按会话重测。

## 九、实现预注册（2026-11-22，随冻结生效；实现前零采集）

离线工具 `tools/gate5_ver.py`（新建；[gate4_pair.py](../../../tools/gate4_pair.py) 本体冻结不动，只 import 复用其解析与度量）。**纯 stdlib、零第三方依赖、零网络、只读输入。**

### 9.1 CLI（冻结）

```
python -I tools/gate5_ver.py --candidate <cpk5.bin> --source <cpk5s.bin> --out-dir <dir>
        [--tolerances <json>] [--self-test]
```

- 默认 = metadata-only（dry-run 语义）：不解码 pose、不产误差明细。
- `--tolerances`（姿态判定的唯一开关）= `{"tau_p":>0,"tau_r":>0,"tau_s":>0,"epsilon_t":>0}`（全有限），即门 4 冻结槽位值。
- 退出码：0 成功；2 数据 INVALID；3 解析/守卫失败。

### 9.2 规则（全部来自冻结的 §三，此处只定操作化）

1. 解析与守卫：复用 `gate4_pair.parse_candidate/parse_source`（`track_count==116`、`two_read_equal==1`、有限钟值守卫随解析器继承）；记录级 IDUN（任一 clip `data_fnv==0`）排除出配对、照记账本。
2. 域归属：按 `CLOCK_PRIORITY` 取首个身份可读 clip（同 gate4）。
3. burst 分组：文件相邻 + 同域 + 同 `stable_id` + 同观测 `local_time`；比较单元 = **burst 末步**（单记录 burst 自身即单元）。
4. clip 末段饱和排除：`len(burst) ≥ 2` 且 `local_time ≥ duration − 1.5/60`（duration 取该 burst 优先 clip 的 `duration_bits` 解码；`duration ≤ 0` 或不可读 → 不触发排除）。整 burst 出等值集、留账本。
5. EQ-W1b 排除：burst 内任一成员 `completed ≤ 24` → 整 burst 出等值集、留账本。
6. EQ-W1a 源侧排除：被配对的源记录含 **2+ 个身份可读 clip 族**（双 clip 共存）→ 该对出等值集、留账本。
7. 端点配对：域内全部源记录钟值取 `|c−x|` 最小（等距取 `seq` 较早；端点越源钟域 → `edge-pair` flag，配对成立）——同 gate4 §3.4 nearest-clock 语义；不继承实例窗/wrap 机械（本批零 wrap；如出现 wrap 判 INVALID）。
8. dt 门：`dt_clock > epsilon_t` 的对排除出误差分布并记账（沿用 TIME v1 的 `dt_exclude=true` + 2% edge-only 上限，超限 → INVALID）。
9. 等值主张集（按 §一 冻结算术，99 主轨 = 轨号 0–98；A17 99–115 描述性照记）：
   - 87 条轨（99 − DECL1 五轨 − DECL2 七轨）：translation/rotation/scale 三通道全判；
   - DECL1 = {30,31,58,59,45}：rotation 声明（不判）；其中 {30,31,58,59} 的 translation 判，t45 的 translation 声明；其 scale 不进等值集；
   - DECL2 = {60..66}：translation 声明（不判）；rotation 判；scale 判；
   - 超差计数 = 等值分量上 `E_p > tau_p` / `E_r > tau_r` / `E_s > tau_s`；逐轨排名输出。
10. DECL 集记账：被声明分量的逐轨实测值（`E_r` 换算角度 / `E_p`）全量输出，与 [C-G5-EQ v1 §三](../g5-eq-contract-20261121/README.md) 基准量级比对——**不进 FAIL**。
11. 残差集：不做工具级归类（t44/手指/C″/阶梯的归因由运行记录按 §三.4 写）；工具输出逐轨超差与实测值，机械计数只含等值分量。
12. 输出：`gate5-ver-manifest.json`（对账恒等式、burst 清单、端点/排除/记账分类、配对账本：对数、多对一 host 结构、`dt_clock` 分布）≤1 MiB；`gate5-ver-summary.txt` ≤200 行；`gate5-ver-detail.bin`（仅 `--tolerances`）：每对 16 B 头（pair_index/cand_record/src_seq/reserved）+ 116×4×f32（E_p/E_r/theta/E_s，轨序 0–115）= 1,872 B/对。
13. 测试（`tools/gate5_ver_test.py`，纯 stdlib、`python -I`、期望值解析计算）：T1 burst 分组（相邻等钟聚合 + stable_id 变更切分）；T2 末步单元选择；T3 饱和判据边界（duration−1.5 帧±）；T4 W1b completed 边界；T5 双 clip 共存源对排除；T6 nearest-clock 等距平手取 seq 早；T7 DECL 分量映射（t45/t60–66 各通道）；T8 dt 排除记账与 2% 上限；T9 IDUN 排除与恒等式闭合；T10 合成 CPK5/CPK5S 解析往返；T11 同参两次运行字节一致；T12 无 `--tolerances` 拒绝 pose 解码。
14. dry-run：冻结后先对**已归档的 gate-5 正式批次**（v103 两 bin + 冻结容差）执行全模式，登记结果与 [O-1 分析](../g5-o1-pairing-granularity-20261121/README.md) 的端点读数交叉核对——可执行性证据，非新裁定。

### 9.3 不做的事

- 不改 `gate4_pair.py`/`gate4_clock.py`/`cal1c_time.py` 本体；不改 CPK5/CPK5S 布局；不新增 INI 键；不实现 IDLE CHAIN SUMMARY 修复（§七.2 可选项，若做另立小节登记）。
- 工具不输出 G5 级裁定语——只产出机械计数与分布；裁定语由运行记录按 §三.4 书写（带 limited 资格）。
