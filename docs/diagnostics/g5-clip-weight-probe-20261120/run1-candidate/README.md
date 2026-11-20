# C-G5-TRANS v9 候选 run 1（2026-11-20 23:51–23:55，v118）

**结论：H-W4 成立（权重恒定）、H-W5 否证（采样是覆盖写入）、三点解恒为 0——捕获姿态在探针时间轴上是分段常量，只在每个命令的采样步进一次。** 于是 v8 的「按 2.8 线性收敛」不是权重斜坡：装备段权重表全程 `255`（`w-min = w-max = 255`、`w-hash` 恒 `67508074`）、`control->weight = 1.0`、`blend-hint = 0`，而 `solve-median = 0`（min = max = 0）说明 `pre(N+1)` 与 `post(N)` 的姿态逐轨相同。

## 一、身份与工件

| 项 | 读数 |
| --- | --- |
| 构建身份 | `CharacterPanel-scenegraph-v118-g5trans-clip-weight-20261120-abb8c969df-cl94faaed0c6-20261120T154338Z` |
| 运行档 | 候选 INI 17,756 B `47FB9EE1…6F7F259`（v7/v8/v9 三探针全开） |
| 运行窗口 | K5 写入 23:51:29；日志 23:55:04 |
| MO2 | `AE-Track2`；唯一 `[E]` 为装载期 OAR 豁免 |
| 日志 | [CharacterPanel.log](CharacterPanel.log) 4,667,286 B `4761BE371B0C55BC23CA88C0E54D1BC3D48DF75302A1605BAEBDB3D631253F55` |
| K5 bin | [CharacterPanel-g5k5-….bin](CharacterPanel-g5k5-CharacterPanel-scenegraph-v118-g5trans-clip-weight-20261120-abb8c969df-cl94faaed0c6-20261120T154338Z-20261120T155129Z.bin) 1,410,452 B `8BC946751A5605F87F655A9D56A2C9352E7AD4781A4C6DFFD49B1CD6168D17A9` |
| K5 JSON | [CharacterPanel-g5k5-….json](CharacterPanel-g5k5-CharacterPanel-scenegraph-v118-g5trans-clip-weight-20261120-abb8c969df-cl94faaed0c6-20261120T154338Z-20261120T155129Z.json) 797 B `27C9B9601143658BAAFFAE57E0C25091A1920CC0B55801549B812F3D238D60B9` |

## 二、计数

| 行 | 数量 |
| --- | ---: |
| `TRANSITION CLIP WEIGHT`（全部 `status=ok`） | 50 |
| `TRANSITION CLIP CAPTURE` | 50 |
| `TRANSITION CLIP CLOCK` | 50 |
| `TRANSITION PROBE` header | 747 |
| `[E]` / `[W]` | 1（OAR 豁免）/ 15 |

K5 249/249（equip 120 `fixed-equip-budget`、unequip 129 `state-12-to-0`）；`identity-failed=1`、`drift-records=1`（与前几轮一致）。

## 三、权重读数

| 项 | 装备段 1–13 | 卸下段 1–12 |
| --- | --- | --- |
| `weights` | 99 | 99 |
| `w-min` / `w-max` | **255 / 255**（全程） | **255 / 255** |
| `w-hash` | **恒 `67508074`** | 恒 `67508074` |
| `w-head` | `255,255,255,255,255,255,255,255` | 同 |
| `ctl-weight-bits` | `3F800000`（1.0） | `3F800000` |
| `blend-hint` | 0 | 0 |
| `pfill-agreed / compared` | 10–16 / 99 | 24–40 / 99 |
| `solved` | 60 → 26 | 7–9 |
| `solve-median / min / max` | **0 / 0 / 0** | **0 / 0 / 0**（cmd 1 跨段例外 0.999948） |

- **H-W4 成立**：逐轨权重表恒定 255，`control->weight` 恒 1.0，`blend-hint` 恒 0。没有任何权重在 ramp。
- **H-W5 否证**：把 `poseLocal` 预填进缓冲再采样，与零填版本（v8 的 `TRANSITION CLIP CAPTURE`）给出同样的 10–16/99 —— `SamplePartialTracks` 是**覆盖写入**，不是与缓冲混合。
- **三点解恒 0**：`w = (current − previous)/(sample − previous)` 在全部轨、全部命令上都为 0（min = max = 0），即 `pre(N+1)` 与 `post(N)` 的 `poseLocal` 逐轨相同。姿态只在 `pre(N)` 与 `post(N)` 之间改变，也就是**每个命令的采样步进一次**——与 K5 的 11 个唯一姿态完全一致（本轮复算：装备 completed 2–12 仍 11/11 唯一）。

## 四、判据

| 判据 | 裁定 |
| --- | --- |
| CC9-1 身份/环境 | **PASS**：v118、1.6.1170、`AE-Track2`、K5 249/249、747 header、唯一 `[E]` 为 Track 2 OAR 豁免 |
| CC9-2 仪器成立 | **PASS**：50/50 `status=ok`；`weights=99` 与 v8 的 `ctl-track-weights` 相同；cmd≥2 起 `solved>0` |
| CC9-3 权重判别 | **PASS（H-W4 单分支）**：权重与 `control->weight` 全程恒定 |
| CC9-4 无副作用 | **PASS**：无新 detour/写入；K5 账本与 v117 逐项相同；`source-drift` 仅 `n/a`/`none` |

## 五、结论与下一步

1. **权重不是自变量**。v8 的等差收敛（32.20 → 1.40，每命令 2.8）既不是权重斜坡，也不是后处理残差：它是**姿态每个命令朝冻结的采样值前进一个固定量**。
2. 结合 CC9-3 与 v7 的 `advance=frozen-no-step`：冻结的是 `clip->localTime (+0xF0)` 与 `control+0x10`，但姿态仍在推进。两种可能：
   - **S1 写入是增量式的**：图把「采样值 − 当前姿态」的一部分或固定量写回 `poseLocal`，而不是整体替换；
   - **S2 有效采样时刻在推进**，只是被读的那两个字段陈旧（图走了不经过 `clip+0xF0` 复制的采样路径）。
3. 判别只需一次**时间扫描**：在每个观测点用 `animation->SamplePartialTracks(t0 + k·δ)`（δ = 1/60 s，k ∈ [−4, +4]）与 `poseLocal` 比较，报最优 `k` 与残差。若最优 `k` 每命令 +1 → S2（有效时刻在走、字段陈旧）；若 `k` 恒 0 而残差仍按 2.8 缩小 → S1（增量写入）。这仍是纯只读、单行输出。

## 六、边界

- 单次运行、单存档、单装备状态；不含源侧。
- 不改变 `G4P-7 FAIL`、G5/G6 与任何 `tau_*`。
- 只对 AE 1.6.1170 + v118 工件成立；「分段常量 + 每命令一步」不外推到其它 clip。
