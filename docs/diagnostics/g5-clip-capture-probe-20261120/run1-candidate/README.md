# C-G5-TRANS v8 候选 run 1（2026-11-20 23:37–23:39，v117）

**结论：H-CAP1 否证、H-CAP2 否证、H-CAP3 成立——`poseLocal` 不是控制在该时刻的采样。** 50/50 行 `status=ok`；`tracks=99`、`num-pose=116`、`map-len=0`（identity）、`unmapped=0`；`ctl-local-time-bits` 与同点 v7 时钟行逐位相同。装备段一致率只有 **10–24/99**、`max-dpos` 1.40–32.20；卸下段一致率 37–40/99、`max-dpos` 恒 1.617。**但装备段的差距不是噪声：它按每命令 2.8 精确线性收敛（32.20 → 1.40），随后在 cmd 13 时钟跳变时重新张开到 25.20**——这是权重斜坡的特征，不是后处理残差。

## 一、身份与工件

| 项 | 读数 |
| --- | --- |
| 构建身份 | `CharacterPanel-scenegraph-v117-g5trans-clip-capture-20261120-282fb3eedc-cl94faaed0c6-20261120T153353Z` |
| 运行档 | 候选 INI 17,508 B `2D65A3A1…E4BB13`（v7 时钟 + v8 捕获同时开） |
| 运行窗口 | K5 写入 23:37:50；日志 23:39:18 |
| MO2 | `AE-Track2`；唯一 `[E]` 为装载期 OAR 豁免 |
| 日志 | [CharacterPanel.log](CharacterPanel.log) 4,635,688 B `5544AE2815ADC095A4383CD4DA1D5768511ABABA9EE9F684D8E86C2DAE055899` |
| K5 bin | [CharacterPanel-g5k5-….bin](CharacterPanel-g5k5-CharacterPanel-scenegraph-v117-g5trans-clip-capture-20261120-282fb3eedc-cl94faaed0c6-20261120T153353Z-20261120T153750Z.bin) 1,410,452 B `76F49447993DC8A764F6C8792BE91ABC4BEB065A6E19B8BFF38362DE94CDFB73` |
| K5 JSON | [CharacterPanel-g5k5-….json](CharacterPanel-g5k5-CharacterPanel-scenegraph-v117-g5trans-clip-capture-20261120-282fb3eedc-cl94faaed0c6-20261120T153353Z-20261120T153750Z.json) 799 B `C11CFA88B58CD45BC9CE0D9C25F548A6DE70C9D685057974EACBBA46B6850B32` |

## 二、K5 与主探针

- K5 249/249；equip 120 `fixed-equip-budget`、unequip 129 `state-12-to-0`；`identity-failed=1`、`drift-records=1`（与前两轮相同）。
- `TRANSITION PROBE` 747 header；本轮 `TRANSITION CLIP CLOCK` 50 行与 v116 完全同构（cmd 3–12 仍 `frozen-no-step`）。
- 15 条 `[W]` 均为 studio lights / DAC 预览。

## 三、逐点读数

`max-dpos` 为分量的最大值，`resid-max` 为「去均值后的欧氏残差」，两者不是同一度量，只做同量级比较。

### 3.1 装备段

| 命令 | pre-sample `max-dpos` / agreed | post-sample `max-dpos` / agreed |
| ---: | --- | --- |
| 1 | 28.330582 / 15 | 27.216974 / 10 |
| 2 | 32.201603 / 10 | 29.401466 / 11 |
| 3 | 29.401466 / 11 | 26.601326 / 12 |
| 4 | 26.601326 / 12 | 23.801186 / 12 |
| 5 | 23.801186 / 12 | 21.001047 / 13 |
| 6 | 21.001047 / 13 | 18.200909 / 13 |
| 7 | 18.200909 / 13 | 15.400766 / 14 |
| 8 | 15.400766 / 14 | 12.600628 / 14 |
| 9 | 12.600628 / 14 | 9.800489 / 14 |
| 10 | 9.800489 / 14 | 7.000349 / 14 |
| 11 | 7.000349 / 14 | 4.200211 / 16 |
| 12 | 4.200211 / 16 | **1.400070 / 24** |
| 13 | 25.200270 / 14 | 25.391233 / 29 |

- 相邻观测点（含跨命令）差值恒为 **2.8**：32.20 → 29.40 → 26.60 → … → 4.20 → 1.40。
- 失配轨集合稳定：`mismatch-head=1,2,3,4,6,7,8,9`（track 0 与 5 一致），全程不变。
- cmd 13 时钟跳到 `0x3EC88888` 后差距重新张开到 25.2。

### 3.2 卸下段

| 命令 | `max-dpos` / agreed | 说明 |
| ---: | --- | --- |
| 1–9 | 1.617165 / 40→37 | 时钟冻结在 `0`，差距**恒定** |
| 10–12 | 1.620416 → 1.511719 / 24–26 | 时钟开始每步 +0.016667，差距略降 |

### 3.3 对照

- `map-len=0`（identity），与 v7 `clip-ref` 的 `map-len=0`、`mapping=identity` 一致。
- `ctl-local-time-bits` 与 v7 时钟行逐点相同（装备 0.2083 冻结、卸下 0 → 0.0333）。
- 两段的差异量级相差 20 倍（32 vs 1.6），说明「捕获 ≠ 采样」不是全局的空间/映射错误：若是坐标系问题，两段应同量级。

## 四、判据

| 判据 | 裁定 |
| --- | --- |
| CC8-1 身份/环境 | **PASS**：v117、1.6.1170、`AE-Track2`、K5 249/249、747 header、唯一 `[E]` 为 Track 2 OAR 豁免 |
| CC8-2 仪器成立 | **PASS**：50/50 `status=ok`；`tracks=99`、`num-pose=116`、`map-len=0`、`unmapped=0`；`ctl-local-time-bits` 与 v7 一致 |
| CC8-3 比较成立 | **PASS（单一分支）**：H-CAP1 否证（10–24/99）、H-CAP2 否证（`mean-shift` 0.04–0.31，远小于 30+ 的残差）、**H-CAP3 成立** |
| CC8-4 无副作用 | **PASS**：无新 detour/写入；K5 账本与 v116 逐项相同；`source-drift` 仅 `n/a`/`none` |

## 五、结论与下一步

1. **捕获缓冲不是控制采样的结果**（H-CAP3）。这不是空间/映射问题：同一 `binding`、`map-len=0`、同一 `control`，而两段的偏差量级相差 20 倍，且失配轨集合恒定。
2. **装备段的偏差按固定步长 2.8 线性收敛到 1.4，随后在时钟跳变处重开**。线性收敛只可能来自一条随时间线性变化的权重：要么 `control` 的逐轨权重（`transformTrackWeights +0x18`，本轮只记了 size=99）在 ramp，要么图输出合成在 ramp。后处理写入者（E2 系列）产生的是非系统性残差，不会给出这样干净的等差数列。
3. 因此下一轮（v9，只读）应直接读**权重值**：`transformTrackWeights` 的 min/max/前 8 项/字节哈希、`control->weight (+0x14)`、`binding->blendHint`，并用三点解（上一帧 pose、当前 pose、采样）反解每轨实际权重。若权重线性 ramp，则 v7 的「冻结时间下姿态逐帧不同」被解释为**权重斜坡**，G5 的时钟/姿态配对协议需要显式包含权重，而不是假设权重恒 1。
4. 若权重恒定（值全 1），则误差必须由采样语义解释（例如 `SamplePartialTracks` 对权重表非空时的部分采样语义），改用「预填 poseLocal 后再采样」的对照来分离。

## 六、边界

- 单次运行、单存档、单装备状态；不含源侧。
- 不改变 `G4P-7 FAIL`、G5/G6 与任何 `tau_*`。
- 只对 AE 1.6.1170 + v117 工件成立。
