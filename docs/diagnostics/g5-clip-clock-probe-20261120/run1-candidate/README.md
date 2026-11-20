# C-G5-TRANS v7 候选 run 1（2026-11-20 23:25–23:27，v116）

**结论：H-D1 + H-W2 成立 → 剩余路线是 M2/M3。** 50/50 行 `status=ok`；全程 `mode=0`、`at-end=0`；clip `time (+0xF4)` 与 `localTime (+0xF0)` 逐行相同，`control+0x10` 亦相同。装备第 3–12 步与卸下第 2–9 步为 `advance=frozen-no-step`（36 行），权重 `0x3F800000`（1.0）与 `track-weights=99` 恒定（H-W1 否证）。同一冻结窗口内 K5 姿态仍 11/11 唯一：3→4 帧有 64 根轨平移、79 根旋转改变，且**不是整体刚性位移**（去均值后残差最大 3.0734）。即该 clip 的采样在冻结期逐帧相同，姿态差异来自采样之后的写入者或捕获点。

## 一、身份与工件

| 项 | 读数 |
| --- | --- |
| 构建身份 | `CharacterPanel-scenegraph-v116-g5trans-clip-clock-20261120-c7ec35441f-cl94faaed0c6-20261120T152153Z` |
| 运行档 | 候选 INI 17,315 B `C9902A42…AC6121EEC`（`TransitionClipClockProbe=true`、父链/子图 false） |
| 运行窗口 | 23:25:27.787 panel open → 23:27:11.587 panel close；K5 写入 23:25:27 |
| MO2 | `AE-Track2`；唯一 `[E]` 为装载期 OAR 豁免 `failed to get plugin info for OpenAnimationReplacer` |
| 日志 | [CharacterPanel.log](CharacterPanel.log) 4,604,908 B `21C6640864A34ED5B9EE384484278213BCFA763BACB258688895C1219D68F9B0` |
| K5 bin | [CharacterPanel-g5k5-….bin](CharacterPanel-g5k5-CharacterPanel-scenegraph-v116-g5trans-clip-clock-20261120-c7ec35441f-cl94faaed0c6-20261120T152153Z-20261120T152527Z.bin) 1,410,452 B `0469806381D0AB3E482930328800BBD95F5C4FF9B3FF0F7DA0568039F12A9015` |
| K5 JSON | [CharacterPanel-g5k5-….json](CharacterPanel-g5k5-CharacterPanel-scenegraph-v116-g5trans-clip-clock-20261120-c7ec35441f-cl94faaed0c6-20261120T152153Z-20261120T152527Z.json) 795 B `2016CC3F33A7692D2B52891E99E6EFB70D11D389FF3A0DBE1F7C7B3095E5987C` |

## 二、K5 与主探针

- K5 249/249；equip 120 `fixed-equip-budget`、unequip 129 `state-12-to-0`；`identity-failed=1`（SCAR dummy）、`drift-records=1`，与 v114/v115 逐项相同。
- `TRANSITION PROBE` 747 header 全 `flags=0x0000`；`TRANSITION CENSUS` 1,276 行。
- `source-drift`：1×`n/a`（pre-activate）+ 5×`none`。
- 15 条 `[W]` 均为 studio lights / DAC 预览提示。

## 三、时钟读数

### 3.1 汇总

| 分布 | 行数 |
| --- | ---: |
| `advance=unknown`（每段首点） | 4 |
| `advance=advancing` | 10 |
| `advance=frozen-no-step` | 36 |
| `advance=frozen-at-end` | **0** |
| `at-end=1` | **0** |
| `mode` ≠ 0 | **0** |
| `ctl-weight-bits` ≠ `3F800000` | **0** |
| `ctl-track-weights` ≠ 99 | **0** |

### 3.2 装备段（`expected='1HM_Equip.hkx'`，pre-sample 与 post-sample 同值）

| 步 | `local-time` = `time` | Δtime | advance |
| ---: | --- | ---: | --- |
| 1 | `0x3C888889`（0.016667） | 0 | `unknown`（首点） |
| 2 | `0x3E555556`（0.208333） | +0.191667 | `advancing` |
| 3–12 | `0x3E555556`（0.208333） | **0** | **`frozen-no-step`** |
| 13 | `0x3EC88888`（0.391667） | +0.183333 | `advancing` |

### 3.3 卸下段（`expected='1HM_Unequip.hkx'`）

| 步 | `local-time` = `time` | Δtime | advance |
| ---: | --- | ---: | --- |
| 1 | `0x00000000` | 0 | `unknown`（首点） |
| 2–9 | `0x00000000` | 0 | `frozen-no-step` |
| 10 | `0x3C88887B`（0.016667） | +0.016667 | `advancing` |
| 11 | `0x3D088882`（0.033333） | +0.016667 | `advancing` |
| 12 | `0x3D4CCCC6`（0.050000） | +0.016667 | `advancing` |

### 3.4 时钟是「突发式」推进

两段都显示同一种形态：多数 K5 步 `Δtime=0`，少数步一次跳约 11–11.5 帧（0.1833–0.1917 s）。这与 v103 的「249 帧只有 17 个唯一 clip 时间」一致，说明候选驱动只在部分步上执行了带步长的图更新，其余步只重采样。

## 四、同一冻结窗口内的姿态

| 项 | 读数 |
| --- | --- |
| 装备 completed 2–12 | 11 帧，**11/11 唯一** track-block 哈希 |
| 卸下 129 帧 | 129/129 唯一 |
| 冻结帧 3→4 | 64 根轨平移变化、79 根旋转变化；去均值残差最大 **3.0734**（非刚性整体位移） |
| 最大单轨 | track 43，Δ=(2.8001, −0.4118, 1.2568)，|Δ|=3.0968 |
| `clip-ref`（同 `localTime` 重采样 vs 捕获姿态） | baseline `16/99`、`max-dpos=2.344827`；`time=0.0167` 时 `10/99`、`max-dpos=27.216974`；`time=1.1` 时 `30/99`、`max-dpos=0.832284` |

## 五、判据

| 判据 | 裁定 |
| --- | --- |
| CC-1 身份/环境 | **PASS**：v116、1.6.1170、`AE-Track2` + Save116、K5 249/249、747 header `flags=0`、唯一 `[E]` 为 Track 2 OAR 豁免 |
| CC-2 仪器成立 | **PASS**：50/50 `status=ok`；26 行 `1HM_Equip.hkx` / 24 行 `1HM_Unequip.hkx`；`control` 与 `ctl-*` 全部可读 |
| CC-3 时钟判别 | **PASS — H-D1 单分支**（`frozen-no-step`）。H-D2 否证（`at-end` 全程 0）；H-D3 否证（`time` 与 `localTime` 同步、`ctl-local-time` 与之相同） |
| CC-4 权重判别 | **PASS — H-W2**。H-W1 否证：`ctl-weight=1.0`、`ctl-track-weights=99` 在冻结帧之间恒定 |
| CC-5 无副作用 | **PASS**：无新 detour/写入；K5 账本与 v115 逐项相同；`source-drift` 只有 `n/a`/`none` |

## 六、结论与下一步

1. **采样在冻结期逐帧相同**：`localTime`、`control+0x10`、`time` 三处一致且不动，权重与轨道权重表大小恒定。因此姿态差异**不是** clip 采样、也不是权重斜坡产生的。
2. 剩余两条路线：
   - **M2 采样后写入者**：生成之后仍有写入者改这些轨（E2-A/E2-B 列过的 58423 足部 IK、布料、手部 IK/附件）。但 E2-A 的残差量级是 `1e-5` 相对，而本轮 `clip-ref` 在 `time=0.0167` 时 `max-dpos=27.2`，量级不匹配。
   - **M3 捕获点/映射**：K5 捕获的缓冲或轨道→骨骼映射不是该图的输出。`27.2` 与 `2.34` 的偏差、以及 116 轨对 99 轨的差额，都指向这里。
3. 建议 v8（只读、一条行）：同时记录 ① 捕获缓冲指针与 `hkbCharacter::poseLocal` 的同一性，② `binding->transformTrackToBoneIndices`（`map-len`/前若干项），③ 同一帧用 `control+0x10` 走 `SamplePartialTracks` 与捕获姿态逐轨比较（`clip-ref` 已有，补上 control 路径与指针身份）。先判 M3，再决定是否回到 M2 的 58423/63581 写入计数。

## 七、记录缺陷（已修源码，未重建）

本轮的 50 行都把 `segment=equip` 硬编码在行键里（照抄了父链探针的键），包括卸下的 24 行。`advance` 的状态表按真实 `segment` 索引，因此分类不受影响；行可用 `expected='1HM_Equip.hkx'` / `'1HM_Unequip.hkx'`（26/24）与命令行顺序分离。源码已改为输出真实段名（提交见 `docs` 记录），**未重建**：安装位与本轮归档必须保持同一二进制。下一版（v117）起行键正确。

## 八、边界

- 单次运行、单存档、单装备状态；不含源侧。
- 不改变 `G4P-7 FAIL`、G5/G6 与任何 `tau_*`。
- 只对 AE 1.6.1170 + v116 工件成立；不外推到其他装备族/运行时/存档。
