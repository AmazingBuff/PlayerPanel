# C-G5-CAL K2-C4 控制 B：方案 A 预注册（2026-11-14）

**状态：PRE-REGISTERED，尚未查看任何原生控制输出。** 本目录只登记离线真值、资源身份、承载路径与判据；本轮不产生 `tau_p/tau_r/tau_s/epsilon_t`，不关闭 K1/K2、CAL2 或 G5。

## 一、为什么不是此前的 `male\mt_idle.hkx`

2026-11-13 的资格审计登记的是 `meshes\actors\character\animations\male\mt_idle.hkx`（30,944 B，9.133333 s，30 fps）。v96a 运行日志显示当前玩家实际加载的是：

```
ANIMATION ... index=1022 base=1022 replaced=false duration=25.02 path='Animations\female\mt_idle.hkx'
clip-ref ... name='Animations\male\MT_Idle.HKX' binding-index=1022 map-len=99 mapping=table time=0.0166666675 duration=25.0167 tracks=99
```

因此本轮重新登记运行时真正的 **female** loose 覆盖，男性文件只在历史记录中保留，不再作为控制 B 资源。

## 二、资源身份（查看原生输出前已固定）

| 项 | 值 |
| --- | --- |
| 完整路径 | `meshes\actors\character\animations\female\mt_idle.hkx` |
| 来源 | MO2 `Vanargand Animations II - Female Idle Walk and Run` loose 覆盖 |
| 文件字节 / SHA-256 | 113,920 B / `0EF8248CE2C50A2251FA592F7C65E75E75B5E4DC55E9C654436543180BCD73C7` |
| 容器 | `hkaAnimationContainer`：animations=1、bindings=1、skeletons=0 |
| 动画 | `hkaSplineCompressedAnimation` type=5，duration=25.0166664 s，99 transform + 0 float tracks |
| 帧结构 | numFrames=1501、numBlocks=6、maxFramesPerBlock=256、frameDuration=0.0166777782、blockDuration=4.25283337、blockInverseDuration=0.235137358 |
| 事件 | annotationTracks=99、annotations=4；直接 `SamplePartialTracks` 不派发事件，事件不进入本次采样 |
| binding | skeleton=`NPC Root [Root]`，`transformTrackToBoneIndices` = 0..98（99 项，FNV-1a64 `077954859409395E`），blendHint=0 |
| 压缩数据 | `data` = 82,400 B，FNV-1a64 `F56EE1500EB5A3D0`；`blockOffsets` 6 项，FNV-1a64 `F17EBD9ADA8CF785` |

运行时插件会用 `hkArray::size` + `data()` 重新计算 `data`、`blockOffsets` 与 binding map 的 FNV-1a64。`data`/`blockOffsets` 必须与上表逐位相同才继续采样；binding map 的 count/hash/blendHint 一并记录为 `map_match`，但 `SamplePartialTracks` 只使用动画轨道、不使用该映射，因此 `map_match=no` 不阻断原始轨道读数，只在裁定中说明。

## 三、独立真值（HavokLib，仅本地离线）

真值工具位于 gitignored 的 `build/hkx-audit/HkxTruth`，链接 HavokLib（GPL-3，本地 clone `ef5d5c6`），**不进入仓库、不链接进插件**。本地补丁：

1. **时间映射**：`hkaSplineCompressedAnimation` 的 `GetValue` 使用 `localTime / frameDuration`，而不是上游派生的 `numFrames / duration`。理由：文件只存 `frameDuration`；`blockDuration = 255 × frameDuration`、`duration = 1500 × frameDuration`，而 `numFrames = 1501` 把结束帧计入，派生值 60 会在一整段内漂移约一帧。t=0 端点不受该补丁影响，并且与导出的第 0 个控制点逐位一致。
2. 增加只读 `GetDataSize()` 和 `DumpTrack()`，用于离线哈希与控制点审计；不改变解码逻辑。

工件：[mt_idle_female_truth.txt](mt_idle_female_truth.txt)（106,767 B，SHA-256 `CFC7835C2235C696E3F5C93CBD7234AD098FAE605E6F8CE706662B7EF2024125`），共 891 行 `TRACK`（9 个时间 × 99 轨），每行 10 个原始 float32 bit。文件末尾 `BITS_FNV1A64 8E2E1D12B040807D` 是按“时间主序、轨次序、每轨 t.xyz/r.xyzw/s.xyz 小端字节”累计的 FNV-1a64。

### 固定时间表（原始 binary32 bits）

| # | bits | 十进制 |
| --- | --- | --- |
| 0 | `00000000` | 0 |
| 1 | `3C888889` | 0.0166666675（1/60） |
| 2 | `3F800000` | 1 |
| 3 | `40800000` | 4 |
| 4 | `40A00000` | 5 |
| 5 | `41080000` | 8.5 |
| 6 | `41480000` | 12.5 |
| 7 | `41880000` | 17 |
| 8 | `41C80000` | 25 |

## 四、原生承载路径

- 开关：`[Experiment] ControlSample=true`，默认 off；要求 `IndependentGraph=inject`。
- 时点：candidate 的 **第一个 completed Generate**（`completed=1`）。v96a 证明此时 OAR 已把 MT_Idle 的 `animationBindingIndex` 从 11916 切到 1022，且动画为 25.0167 s / 99 轨；baseline（completed=0）仍是 11.6667 s 的另一资源，因此不在 baseline 采样。
- 选择规则：只扫描候选图 `behaviorGraph->activeNodes`（≤512），只认 vtable 为 `hkbClipGenerator` 的节点；按 **动画字节身份**（duration/track/float/spline 字段 + data/blockOffsets/map 哈希）选唯一载体，名称只作 `MT_Idle` 过滤，不把 binding-index 当跨运行身份。
- 采样：对载体 binding 的 `hkaAnimation` 直接调用两次 `SamplePartialTracks(t, 99, out, 0, nullptr, nullptr)`；不读 poseLocal、不写图、不推进图、不创建第二 Actor。每次两份 `hkQsTransform[99]` 逐字节比较。
- 记录：9 × 99 = 891 行 `CONTROL B ... phase=track`，每行 10 个原始 float32 bit；summary 行给出 `two_read_equal`、`nonfinite`、`bits_fnv1a64`、`expected`、`hash_match`、`map_match`。

## 五、预注册判据（不得事后放宽）

| # | 判据 | 通过条件 |
| --- | --- | --- |
| B-A 资源身份 | 唯一载体，`data`/`blockOffsets` 两个 FNV 与登记值相同，duration/spline 字段逐位相同；binding map 可读并记录 `map_match` | 否则 NOT_READY，不采样 |
| B-B 读取完整性 | 9 个时间 × 99 轨 × 10 分量 = 8,910 个值；每个时间两份读数逐字节相同；全部有限 | 任一失败 ⇒ INVALID |
| B-C 独立真值 | `bits_fnv1a64 == 8E2E1D12B040807D`（与离线 HavokLib 真值逐位一致） | 否则记录为差异，不得称 PASS |
| B-D 运行完整性 | 一次窗口/一次进程，零 `[E]`、无崩溃；日志身份与 DLL/PDB/INI 匹配 | 任一失败 ⇒ 本轮无效 |

**边界**：即使 B-A–B-D 全通过，也只证明“该资源、该 9 个固定时间、该单层直接采样路径”在本次会话逐位一致；它不证明 120 代驱动协议、三次独立进程、时间量化、层/权重/混合、K1 资源清单或 G5。`tau_*`/`epsilon_t` 仍为未产生。

**若 B-C 不通过**：保留全部 8910 个分量的原始 bits，离线逐分量定位差异；不得用“容差”或“采样噪声”直接吸收，也不得回改本清单的时间/资源判据。下一步只能是审计差异来源（时间映射、解码顺序、资源被 OAR 替换或 binding 重建），再另立批次。

## 六、离线验证

- `tests/control_sample_policy_test.cpp`：4/4 通过（时间表/规模、资源登记格式、FNV 参考向量、frame/block 结构关系）。
- 真值文件 891 行；`t=0` 的 track 4 为 `C012B5B4,4068755E,42897BE4`，与 HavokLib 导出的第 0 个控制点 `(-2.29234028, 3.63216352, 68.7419739)` 逐位一致。

## 七、B1 首次原生运行（2026-11-14 13:49；**bit-exact FAILED**）

**运行身份**：v100 `CharacterPanel-scenegraph-v100-g5cal-control-b-3378889e91-cl94faaed0c6-20261114T054321Z`，runtime `1-6-1170-0`，OAR `3.1.6`；DLL `409C2684975886E4C5AA093FB2232289D04FC127423A1423448ADCE85D0E476F`，PDB `0EE6284249B1C46EFD5DF815FF6DA2A3CB178607BA42B63371A84EC4D06B801D`，INI `D8B6B7A7CA857BFE06E6ED15E9520158AA3C0E3581EC9614C572E96EE502B752`。日志归档 [CharacterPanel-g5cal-control-b-v100-run1-20261114-1349.log](../CharacterPanel-g5cal-control-b-v100-run1-20261114-1349.log)，345,735 B，SHA-256 `DD3008CBA6AFB4DA38A3DDC7A13F6BEF8A1437706B1F9275501C0E643321D901`；零 `[E]`，120 equip + 129 unequip，`source-drift=none`。

### 承载与读取

| 项 | 读数 | 裁定 |
| --- | --- | --- |
| 载体 | `phase=scan`：`clip='Animations\male\MT_Idle.HKX'`，`index=14`，binding `0x1e845f832a0`，`binding_index=1022` | 唯一 |
| 资源身份 | `data_fnv1a64=F56EE1500EB5A3D0`、`block_offsets_fnv1a64=F17EBD9ADA8CF785`、`map_fnv1a64=077954859409395E`、duration/tracks/spline 字段全部等于登记值 | **PASS（逐位）** |
| 读取完整性 | 891 条 `phase=track`（9×99×10），`two_read_equal=9/9`，`nonfinite=0` | **PASS** |
| 运行完整性 | 一次候选实验、零 `[E]`、身份匹配 | **PASS** |
| B-C 独立真值 | `bits_fnv1a64=0FAB995615D7BFB2` vs `expected=8E2E1D12B040807D`，`hash_match=false` | **FAIL（位精确）** |

### 数值差异（8910 个分量，runtime − offline）

| 通道 | 总数 | 逐位相同 | 不同 | 最大绝对差 |
| --- | --- | --- | --- | --- |
| translation | 2673 | 1970 | 703 | 1.76429749e-05（t=25，track 4，t.x：`C01B766A` vs `C01B76B4`） |
| rotation | 3564 | 1095 | 2469 | 3.16649675e-06 |
| scale | 2673 | 2353 | 320 | 3.57827869e-07 |
| 合计 | 8910 | 5418（60.81%） | 3492 | 1.76429749e-05 |

**结论：差异是 float32 实现顺序，不是资源/时间/映射错误。** 证据：

1. 资源三个 FNV、duration、tracks、spline 字段全部逐位匹配；binding map 也是 authored 0..98。
2. t=0 的 translation 有 272/297 逐位相同，剩余为 1 ULP（最大 9.54e-7）；若时间映射或 binding 选错，端点不可能只有 1 ULP。
3. runtime 与 offline 的四元数范数误差几乎相同（t=25 最大 |q|−1 分别为 3.9807e-4 与 3.9814e-4），说明双方都没有额外归一化；差异来自解码/求值的运算顺序，而不是语义（normalize/sign）差异。
4. 差异随 bbox 绝对量级增长（最大 1.76e-5 出现在 |x|≈2.43、bbox 跨度≈5.99 的轨道），符合仿射解码 `min + (max−min)·q` 与三次 B-spline 求值的 float32 舍入。
5. HavokLib 使用 `localTime / frameDuration`；若引擎误用派生 `60 fps`，t=25 会差约 1 帧，远大于实测 1.76e-5，因此时间映射也被排除。

**裁定：B1 的位精确判据 B-C FAILED，K2-C4 仍 NOT_READY。** 不产生容差，不把 60.81% 的逐位一致当作 PASS，也不回改 B1 的时间/资源判据。

## 八、B2 有界判据（2026-11-14 预注册；待新进程运行）

B1 证明了两件事：资源/载体/读取路径合格，且两个独立实现只在 float32 末位上分歧。B2 不使用“观测到的最大差”当阈值，而是从 **authored bbox + 量化类型 + float32 运算界** 解析地给出逐分量上界：

| 分量类型 | B2 上界 | 依据 |
| --- | --- | --- |
| rotation（本资源全部 40-bit，dynamic/static） | `3.45436e-4` | 40-bit 四元数分量的 authored 量化步长 |
| position/scale，dynamic 16-bit | `max(2·(max−min)/65535, 32·eps·(|min|+|max|))` | 两个 authored 量化步长 + 仿射解码/三次求值的 float32 舍入界；`eps=2^-24` |
| position/scale，static float32 | `32·eps·|value|` | 直接 float32 读取 + 求值舍入 |
| identity（0 或 1） | `32·eps` | 常量通道的浮点舍入 |

**判据**：8910 个分量全部满足 `|runtime − offline| ≤ bound`；同时 B-A/B-B/B-D 保持通过。否则 B2 FAILED，控制 B 仍 NOT_READY。**边界**：B2 只证明“该资源、该 9 个固定时间、该单层直接采样路径”在 float32 解析界内一致；不证明 120 代驱动协议、三次独立进程（CAL2）、时间量化、层/权重/混合或 G5。

**可行性核对（不是 B2 裁定）**：把 B1 的 8910 个分量离线代入上述界，最大比值 `0.4860`（t=25、track 92、t.x；diff 1.718e-7 / bound 3.536e-7）。这只说明界不是空集，B2 仍需新进程重新采集。

**安全系数的时点说明（不得隐去）**：上界的形式来自 authored 数据与 float32 误差分析；`2×step` 和 `32×eps` 的具体系数是在看到 B1 的 0.97×step 最坏比值后确定的，因此 B2 明确是**新批次**，不是对 B1 位精确判据的重判或放宽。B2 必须用新进程重新采集，且结果不得回写 B1。
