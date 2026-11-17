# C-G5-CLOCK v2：候选时钟阶梯与门 5 配对审计（2026-11-17，POST-HOC）

**状态：POST-HOC 描述性审计，已完成；不改变门 4/门 5 判据，不改变 G4P-7 FAIL，不产生 `tau_*`。** 本审计解释 body/dynamic 残余中“候选时钟阶梯 + nearest-clock 复用源记录”所占比例；任何新配对规则都必须另行预注册并在新批次验证。

## 一、背景

[门 5 正式运行](../g5-gate5-20261116/README.md) G4P-7 FAIL：`E_p/E_r/E_s` 3,826 / 16,755 / 352 超冻结 tau。归因审计显示 body 1,226、attachment 2,600、A17 0；其中 5 个静态附件骨 1,240 次可用 H2（源真实附件状态）解释（[C-G5-SETUP](../g5-setup-audit-20261117/README.md)）。剩下的 body 1,226 与动态 attachment 1,360 需要配对机制审计。

## 二、方法

工具：[`tools/gate5_clock_step.py`](../../../tools/gate5_clock_step.py)，纯 stdlib、只读输入、固定排序；两次同参调用逐字节一致。

- 对每个 clip family（`1hm_equip`、`1hm_unequip`、`1hm_idle`），统计候选记录的唯一 clip `local_time` 数、每个时间的记录组大小、源侧唯一时间数。
- 将每条正式配对标记为 `stair`（候选 local_time 组大小 > 1）或 `unique`，并按 body/attachment/A17 统计 `E_p` 超 tau。
- 统计实际用到的源 `seq` 数、最大复用次数；输出 [clock-step.json](results/clock-step.json) 与 [clock-step.txt](results/clock-step.txt)。
- 工具 SHA-256 `C2AC1BFDF4B79FF37F22C5795C2A05FE3EA7CCFB5DBA04AE97A068FEAE9C1C4A`；JSON `F39F16049802A185B431756CF805F79CC979C5B8C5997B04AFC020D501ED8A3E`；summary `E99466FBA3D7869193BB9B5C4EBFCD18471E5B2811CC297B33F369DF8A1B9D7E`。

## 三、结果

| family | 候选记录 | 候选唯一时间 | 最大组 | 源记录 | 源唯一时间 | 配对数 | stair 对 | 实际源 seq 数 | 最大复用 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1hm_equip | 98 | **17** | 15 | 68 | 61 | 97 | **88** | **12** | **20** |
| 1hm_unequip | 129 | **21** | 15 | 92 | 84 | 129 | **118** | **16** | 15 |
| 1hm_idle | 22 | **3** | 15 | 198 | 198 | 22 | **21** | **3** | 15 |

`E_p` 超 tau 按 stair/unique 拆分：

| family | body stair | body unique | attachment stair | attachment unique | A17 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1hm_equip | 1,032/7,744 | 94/792 | 928/968 | 96/99 | 0 |
| 1hm_unequip | 92/10,384 | 8/968 | 1,286/1,298 | 114/121 | 0 |
| 1hm_idle | 0/1,848 | 0/88 | 168/231 | 8/11 | 0 |

**关键读数**：

- 候选 clip local_time 不是逐帧唯一：equip 98 条只有 17 个时间值（最大 15 条同值），unequip 21 个，idle 3 个；源侧对应为 61/84/198 个。
- nearest-clock 因此反复复用少数源记录：97 个 equip 对只用到 **12 个源 seq**（最大复用 20），129 个 unequip 对只用到 **16 个源 seq**。
- body 1,226 次超差中，1,032 次出现在 equip 的 stair 对，92 次出现在 unequip 的 stair 对；idle body 为 0。attachment 2,600 次中绝大多数也在 stair 对。
- 但 unique 对仍非零：equip body 94/792、unequip body 8/968；因此不能把全部残余归给时钟阶梯，也不能用一个“去掉 stair”规则直接通过。

## 四、解释

1. 候选图在过渡 clip 上的 localTime 呈阶梯/滞后；同一 localTime 下仍可能有多条候选姿态。nearest-clock 把这些姿态全部配到同一个源姿态，产生大 `E_p`（例如 equip 第 2–12 帧斜率下降），这是**配对时间分辨率**问题，不是候选图与源动画在该时刻的真实姿态差。
2. 冻结协议只排除了 `dt_clock > epsilon_t`；stair 对的 `dt_clock` 可以很小（如 0.0055 s），但候选姿态在阶梯组内推进更大，`epsilon_t` 未能识别。`epsilon_t` 是时间读取/帧表界，不是姿态速度界；本审计再次确认不能把它当姿态噪声吸收。
3. 静态附件 H2 与动态/身体残余是两个不同轴：即使把 stair 对全部单列，静态 1,240 与 unique body 102 次仍存在；因此下一步需要同时处理“配对时间轴”和“真实附件状态轴”，不能只改一个。

## 五、裁定与下一入口

**本审计不改变 G4P-7 FAIL / G5 未通过。** 下一版只能在**新预注册**下选择以下之一：

- 以候选唯一 local_time 为配对单位（stair 组取代表/聚合），并预注册代表选择规则；
- 以源侧为基准，对候选姿态做时间重采样/插值（需证明插值语义）；
- 显式把 stair 对列为独立“过渡时间轴”，不与稳态姿态等价混算；
- 先修候选时钟阶梯本身（若它是产品缺陷），再重采正式数据。

任何方案都不得用本批数据回填为 G4P-7 通过；静态附件轴仍需单独预注册（H2 已支持，但尚未设计相等判据）。
