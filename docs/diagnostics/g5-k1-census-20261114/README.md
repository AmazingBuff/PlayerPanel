# C-G5-CAL K1 identity census（2026-11-14 预注册）

**状态（2026-11-14 14:24）：census 仪器 PASS（候选 K1C-1–4、源侧 S1–S4）；K1-C4 资源清单已登记，裁定 NOT MATCHED。** 源/候选 clip 家族对齐但 data FNV 几乎全部不同（仅 SCAR dummy 相同），且源侧 OAR 在窗口内自行切换资源。本目录登记 K1-C2/C3/C4 的只读身份普查：图身份、完整骨架/参考姿态、活动 clip 的资源与实例身份。它不采样姿态、不写图、不推进图、不新增 hook。

## 一、目标与边界

本轮只补 CAL v1 §三 K1 中可由现有只读观察覆盖的三项：

| K1 项 | 本轮交付 | 不覆盖 |
| --- | --- | --- |
| K1-C2 图身份 | 源/候选的 graph、holder、root、project、project-db、behavior、setup、bindings、poseLocal/count、boneNodes、numAnimBones、activeNodes、skeleton | 源/候选的 generation/view 同代关系；controller 所有权证明 |
| K1-C3 骨架与映射 | 完整骨名、父索引、reference pose（10 个 raw float32/骨）、float slots；`recorded==bones`、`invalid-parent=0`、`nonfinite=0` | binding/default-fill 的逐轨语义（另见 K1-C4 的 map hash） |
| K1-C4 资源与 clip 实例 | 每个活动 clip 的 node/binding/animation 指针、binding-index、clip 名、mode/localTime/speed/enforced/crop、动画 header、spline `data`/`blockOffsets` FNV、binding map FNV/blendHint、进程内 stable-id | 跨运行稳定 ID；OAR replacement 的完整配置清单；binding set 全表（只覆盖活动 clip） |

**明确不做**：不读/不比较 `poseLocal`，不调用 `SamplePartialTracks`，不创建第二 Actor，不写源图，不新增 detour，不产生 `tau_*`，不关闭 K1-C1 或 K5/K6/G5。

## 二、触发与采集点

- 开关：`[Experiment] ClipIdentityCensus=true`，默认 off。
- **候选模式**（`IndependentGraph=inject`）：源侧 `observe_source_baseline` 之后 `side=source phase=pre-activate`（骨架 + 活动 clip）；候选 `post-activate`（骨架 + 活动 clip）；随后 equip/unequip 各第一个 completed Generate（`phase=equip-clips` / `unequip-clips`，只 dump 活动 clip）。
- **源侧自然游玩模式**（`IndependentGraph=off` + `SourceTimeline=on`）：F7 窗口打开时 `side=source phase=window-open`（骨架 + 活动 clip）；阶段级变量 settle 时追加 `phase=settle` 的 clip-only dump。触发名：`IsEquipping`、`IsUnequipping`、`currentDefaultState`、`iState`；每窗口最多 24 次，超限打印 `SOURCE TIMELINE census-cap`。
- 次数：候选模式每次实验源 1 次、候选 3 次；源侧自然游玩模式每窗口 1 次 window-open + ≤8 次 settle。

## 三、记录格式

所有行以 `K1 CENSUS` 开头，固定字段顺序：

```
K1 CENSUS actor=... request=... side=source|candidate phase=pre-activate|post-activate|equip-clips|unequip-clips
  graph=... holder=... root=... project='...' project-db=... behavior=... char-behavior=... setup=...
  bindings=... pose-local=.../... bone-nodes=... num-anim-bones=... active-nodes=... skeleton=... bones=...
K1 CENSUS ... phase=skeleton skeleton=... name='...' bones=... parents=... reference=... floats=... slots=...
K1 CENSUS ... phase=bone index=... name='...' parent=... bits=<t.x,t.y,t.z,r.x,r.y,r.z,r.w,s.x,s.y,s.z>
K1 CENSUS ... phase=skeleton-summary bones=... recorded=... invalid-parent=... nonfinite=... overflow=...
K1 CENSUS ... phase=clip slot=... node=... index=... name='...' binding=... animation=... binding-index=...
  mode=... local-time-bits=... speed=... enforced=... crop=... identity=<bool> type=... duration-bits=...
  tracks=... floats=... spline=... frames=... blocks=... max-frames=... block-duration-bits=...
  block-inverse-bits=... frame-duration-bits=... data-bytes=... data-fnv=... blockoff-fnv=...
  map-count=... map-fnv=... blend=... stable-id='<node>:<binding>:<binding-index>:<data-fnv>'
K1 CENSUS ... phase=clips-summary nodes=... clips=... unique=... missing=... overflow=...
```

`stable-id` 是**进程内**身份：node 指针 + binding 指针 + binding-index + `data` FNV。它不能替代跨运行资源哈希，但能把同一进程内同名不同实例的 clip 区分开。对非 spline 动画，`spline=0`、`data/blockoff` 字段为 0，仍保留 header/map 身份。

## 四、预算与上限

| 项 | 上限 | 超限处理 |
| --- | ---: | --- |
| activeNodes 扫描 | 512 | `clips-summary overflow=1`，只记录前 512 个节点 |
| 活动 clip | 64 | `overflow=1`，只记录前 64 个 |
| 骨架骨 | 512 | `skeleton-summary overflow=1`，只记录前 512 个 |
| 日志行 | ~450 | 每侧 116 骨 + ≤64 clip × 3 + 摘要；不设截断，超限即失败 |

## 五、预注册判据

| # | 判据 | 通过条件 |
| --- | --- | --- |
| K1C-1 图身份 | 源 `pre-activate` 与候选 `post-activate`/`equip-clips`/`unequip-clips` 四段均存在；graph/behavior/setup/bindings/pose-local/active-nodes 字段非零或明确 0 | 否则 INVALID |
| K1C-2 骨架完整 | 两侧 `bones == parents == reference`，`recorded == bones`，`invalid-parent=0`，`nonfinite=0`，`overflow=0` | 否则 INVALID |
| K1C-3 clip 身份 | 每个活动 clip `identity=true`；`unique == clips`；`missing=0`；`overflow=0`；equip-clips 含 `1HM_Equip`，unequip-clips 含 `1HM_Unequip`（按 clip 名） | 否则 INVALID |
| K1C-4 运行完整性 | 一次实验、零 `[E]`、无崩溃、DLL/PDB/INI 身份匹配 | 否则本轮无效 |

**裁定边界**：K1C-1–4 全通过只表示本进程的图/骨架/活动 clip 身份**可复核且无丢失**；不表示 K1-C1 的完整运行/资源清单已闭合，不表示跨运行 ID 稳定，不表示 K5 的层/权重/混合计数已完成，也不产生容差或关闭 G5。

## 六、v101 首次运行（2026-11-14 14:11；**census PASS，K1-C4 资源配对 NOT READY**）

**运行身份**：v101 `CharacterPanel-scenegraph-v101-g5k1-census-9d0d2219cb-cl94faaed0c6-20261114T060928Z`，runtime `1-6-1170-0`，OAR `3.1.6`；DLL `C38C179C77D3C0B22BE26A60BE0A0399FDAD9F24D95C087DC79A35233622FCE4`，PDB `F1DC3957CB3D3B47A916E6A96BEDCBBA3C21FA9C54BBD35DB9EB73519F038075`，INI `B94DCE8F0D8141470CC642F65833233692E82F1D79BA4715E6CDEE18B17C93A5`。日志 [归档](../CharacterPanel-g5k1-census-v101-run1-20261114-1411.log)，209,572 B，SHA-256 `AF9433E951FF15998647B42DE781357E1744A2D7FDD1B68025D50EBB6455F379`；零 `[E]`，120 equip + 129 unequip，`source-drift=none`。

### 判据

| # | 读数 | 裁定 |
| --- | --- | --- |
| K1C-1 图身份 | 四段齐全：source `pre-activate`、candidate `post-activate`/`equip-clips`/`unequip-clips`；`project='DefaultFemale'`，图/behavior/setup/bindings/pose/active-nodes 字段全部可读。candidate `post-activate` 的 `holder=0`、`root=0`、`bone-nodes=0`、`num-anim-bones=0` 为显式 0（尚未挂显示节点；equip/unequip 时 `num-anim-bones=116`） | **PASS（带显式 0 说明）** |
| K1C-2 骨架 | 两侧同一 skeleton `0x21036bf3ff0`：`bones=116 parents=116 reference=116 floats=8 slots=8`，`recorded=116`，`invalid-parent=0`，`nonfinite=0`，`overflow=false` | **PASS** |
| K1C-3 clip 身份 | source `clips=1 unique=1`；candidate post `1/1`、equip `2/2`、unequip `4/4`；全部 `identity=true`、`missing=0`、`overflow=0`；equip 含 `1HM_Equip`，unequip 含 `1HM_Unequip` | **PASS** |
| K1C-4 运行完整性 | 一次实验、零 `[E]`、DLL/PDB/INI 身份匹配 | **PASS** |

**日志字段缺陷（不影响 stable-id）**：v101 的 `phase=clip` 行把 `node` 与 `index` 两个参数的顺序写反（`node=0x0/0xe/0xf`，`index=<指针>`）；`stable-id='<node>:<binding>:<binding-index>:<data-fnv>'` 使用的是正确的 node 指针，故 K1C-3 的唯一性判定不受影响。v101a 修复标签顺序，数据字段不变。

### 活动 clip 资源身份

| 阶段 | clip | binding-index | duration | frames/blocks | data bytes | data FNV | map |
| --- | --- | ---: | ---: | --- | ---: | --- | ---: |
| source pre-activate | MT_Idle | 12219 | 20.0000 s | 1000 / 4 | 211,328 | `9F6236C9E2D68F7D` | 0 |
| candidate post-activate | MT_Idle | 11909 | 11.6667 s | 351 / 2 | 16,992 | `722C8A1E46CE297E` | 0 |
| candidate equip-clips | 1HM_Equip | 22 | 1.4333 s | 87 / 1 | 15,728 | `12A77CD14ECC3688` | 0 |
| candidate equip-clips | MT_Idle | 1022 | 25.0167 s | 1501 / 6 | 82,400 | `F56EE1500EB5A3D0` | 99 |
| candidate unequip-clips | 1HM_Unequip | 75 | 2.0000 s | 61 / 1 | 13,744 | `4DED5EAF23DF26E3` | 0 |
| candidate unequip-clips | MLh_Idle | 949 | 2.5333 s | 77 / 1 | 17,328 | `DAD1F1C997C0F0C6` | 0 |
| candidate unequip-clips | 1HM_Idle | 23 | 9.0000 s | 271 / 2 | 14,448 | `C62C233B06FAE9A9` | 0 |
| candidate unequip-clips | SCAR_1hmReadyDummy | 15078 | 6.3333 s | 190 / 1 | 25,168 | `8728A54A50D85C22` | 0 |

### 关键发现：源/候选的 MT_Idle 不是同一资源

1. **源侧** pre-activate 的 MT_Idle 是 20.0 s / 1000 帧 / 4 block / 211,328 B（FNV `9F6236C9E2D68F7D`），binding-index 12219；这不是候选 equip-clips 的 Vanargand 25.0167 s 资源。
2. **候选侧**同一 MT_Idle 节点 `0x21072841900` 在 post-activate 使用 11.6667 s / 351 帧 / 16,992 B（binding `0x2103efe0080`，index 11909），在 equip-clips 使用 25.0167 s / 1501 帧 / 82,400 B（binding `0x21045141f50`，index 1022）。同一节点指针、不同 binding + 不同 data FNV，正是 OAR 替换在进程内的可见形态；`stable-id` 成功区分了这两个实例。
3. 因此本轮的 **census 仪器 PASS**，但 **K1-C4 的源/候选资源配对仍 NOT_READY**：源侧在 equip/unequip 阶段的活动 clip 没有在候选驱动时自然出现（源图暂停，`source-drift=none`），源侧 MT_Idle 20.0 s 与候选 25.0167 s 也不能直接当作同一资源。需要一次**源侧自然游玩窗口**（F7 + 固定装备/卸下脚本）来捕获源侧 equip/unequip 的活动 clip 资源哈希，再与候选逐阶段比较。

**裁定：K1C-1–4 PASS（census 仪器）；K1-C4 资源配对 NOT READY。** 不产生容差，不关闭 K1/K5/K6 或 G5。

## 七、源侧自然游玩普查（v101a 预注册；待运行）

**目的**：在源图自然运行时捕获源侧 equip/unequip 阶段的活动 clip 资源哈希，解释并配对源侧 MT_Idle 20.0 s 与候选 25.0167 s 的差异。

- 运行档：`IndependentGraph=off`、`SourceTimeline=on`、`SourceTimelineGraph=DefaultFemale`、`SourceTransientWatch=false`、`ClipIdentityCensus=true`、其余实验键 off。
- 操作：加载同一存档 → 按 **F7** 开 20 秒窗口 → 等约 2 s → **装备单手武器** → 等约 5 s → **卸下** → 等约 5 s。
- 记录：`side=source phase=window-open` 全量（图+骨架+活动 clip）；阶段级变量 settle 时 `phase=settle` 的 clip-only dump（≤24 次；触发名见 §二）。
- 判据：S1 `window-open` + ≥1 次 `settle` 存在；S2 至少一次 settle 出现 `1HM_Equip`，至少一次出现 `1HM_Unequip`（按 clip 名）；S3 每个活动 clip `identity=true`、`unique==clips`、`missing=0`、`overflow=0`，无 `census-cap`；S4 一次窗口、20 s 内正常关闭、零 `[E]`、无崩溃。
- 不覆盖：不写图、不推进图、不采样姿态、不产生容差；只补 K1-C4 的源侧逐阶段资源清单。

### v101a 首轮（14:20，**INVALID — unequip 未覆盖**）

v101a 的 10 个逐字触发名在 equip 阶段产生 8 次 census，14:20:10.173 触发 `census-cap calls=8 limit=8`；14:20:16.192 的 `IsUnequipping 0->1` / `currentDefaultState 1->12` 未能再采样，`1HM_Unequip` 未出现，S2 失败。该轮只保留 equip 侧发现：源侧 `MT_Idle` 20.0 s / 1000 帧 / 211,328 B（`9F6236C9E2D68F7D`）、`1HM_Equip` 1.4333 s / 33 帧 / 12,512 B（`81FD19882A9ED2B1`）、`1HM_Idle` 9.0 s / 271 帧 / 22,656 B（`C714568869986D08`）、`SCAR_1hmReadyDummy` 25,168 B（`8728A54A50D85C22`）。候选侧对应为 `1HM_Equip` 87 帧 / 15,728 B（`12A77CD14ECC3688`）、`1HM_Idle` 14,448 B（`C62C233B06FAE9A9`），即 **1HM_Equip/1HM_Idle 也不是同一资源**；只有 SCAR dummy 的 data FNV 相同。v101b 把触发收敛为 `IsEquipping`/`IsUnequipping`/`currentDefaultState`/`iState` 并提高上限到 24，重跑后再判 S1–S4。

### v101b 第二轮（14:24，**S1–S4 PASS；源侧资源清单完整**）

**运行身份**：v101b `CharacterPanel-scenegraph-v101b-g5k1-source-census2-4d92f4ad8c-cl94faaed0c6-20261114T062156Z`，DLL `87D1B4178C4198772AF0C60E3937D7E35D41D01AF3C0F04A2694F884456E8D72`，PDB `E3CB4DF02FCA612819DE974B297315DA4F47E6F63277A1808AF35A19AF6F17D1`，INI `2964009C…1A854E`。日志 [归档](../CharacterPanel-g5k1-source-census-v101b-run2-20261114-1424.log)，67,974 B，SHA-256 `8B3AC063674457671A880D6CA1795FB4623DCACE59E7F27F4510BFBA6E9E2F15`。`window-open 20 s` → 14:24:58.141 `window-closed reason=expired lines=93`，零 `[E]`、无 `census-cap`、无崩溃。

| # | 判据 | 读数 | 裁定 |
| --- | --- | --- | --- |
| S1 | window-open + settle | `K1 CENSUS phase=window-open` 1 次（图身份 `DefaultFemale` 652/106，skeleton 116 骨 `recorded=116 invalid-parent=0 nonfinite=0`）+ `phase=settle` 10 次 | **PASS** |
| S2 | equip/unequip clip 覆盖 | settle 0–4 含 `1HM_Equip`（4 次），settle 5–9 含 `1HM_Unequip`（4 次）；触发链 `IsEquipping 0->1` @39.698 → `currentDefaultState 0->10` → `iState 0->9` → `IsUnequipping 0->1` @45.807 | **PASS** |
| S3 | clip 身份完整 | 10 次 `clips-summary`：clips 1/2/2/4/4/3/4/4/1/2/2/1，`unique==clips` 全部，`missing=0`、`overflow=0` 全部；28 条 clip 行全部 `identity=true` | **PASS** |
| S4 | 运行完整 | 一次 F7 窗口、20 s 正常 expired、零 `[E]`、无 `census-cap`、DLL/PDB/INI 匹配 | **PASS** |

**源侧逐阶段资源清单（v101b，9 个唯一资源）**：

| clip | 阶段 | duration | frames/blocks | data B | data FNV |
| --- | --- | ---: | --- | ---: | --- |
| MT_Idle | window-open / equip 起点 | 20.000 s | 1000 / 4 | 211,328 | `9F6236C9E2D68F7D` |
| 1HM_Equip | equip | 1.0667 s | 33 / 1 | 12,512 | `81FD19882A9ED2B1` |
| MLh_Idle / 1HM_Idle | equip 稳定 | 3.3333 s | 101 / 1 | 27,216 | `22C9C4BF59C71D25`（两者同 bytes） |
| SCAR_1hmReadyDummy | equip 稳定 | 6.3333 s | 190 / 1 | 25,168 | `8728A54A50D85C22` |
| 1HM_Unequip | unequip | 1.4667 s | 45 / 1 | 14,896 | `4D885DE532138FE5` |
| MLh_Idle | unequip | 22.9 s | 688 / 3 | 100,064 | `13D0592AF75A0886` |
| 1HM_Idle | unequip | 19.8 s | 595 / 3 | 103,104 | `AE1527A6189CD799` |
| MT_Idle | unequip 末 | 18.0 s | 900 / 4 | 168,640 | `95E20CF7492CAE83` |

**源侧 OAR 在窗口内自身切换资源**：equip 稳定期的 MLh_Idle/1HM_Idle 是 3.3333 s 短版（FNV `22C9C4BF`），unequip 阶段变成 22.9 s / 19.8 s 长版（`13D0592A` / `AE1527A6`）；MT_Idle 从 20.0 s（`9F6236C9`）切到 18.0 s（`95E20CF7`）。binding-index 也随 OAR 变化（MLh_Idle 11604→9037、1HM_Idle 8973→8972、MT_Idle 12218→12220），证明 **leaf name 与 binding-index 都不能当跨阶段身份**，只有 `data FNV` 可区分资源。

## 八、K1-C4 源/候选资源对照与裁定

把 v101 候选侧（`IndependentGraph=inject`）与 v101b 源侧（自然游玩）按 clip 名对照：

| clip | 源侧（v101b） | 候选侧（v101） | 字节相同？ |
| --- | --- | --- | --- |
| 1HM_Equip | 1.0667 s / 33 帧 / 12,512 B `81FD1988…` | 1.4333 s / 87 帧 / 15,728 B `12A77CD1…` | **否** |
| 1HM_Unequip | 1.4667 s / 45 帧 / 14,896 B `4D885DE5…` | 2.0000 s / 61 帧 / 13,744 B `4DED5EAF…` | **否** |
| MT_Idle | 20.0 s / 18.0 s（两版，OAR 切换） | 11.6667 s（baseline）/ 25.0167 s（equip，Vanargand） | **否** |
| 1HM_Idle | 3.3333 s / 19.8 s（两版） | 9.0 s / 271 帧 / 14,448 B `C62C233B…` | **否** |
| MLh_Idle | 3.3333 s / 22.9 s（两版） | 2.5333 s / 77 帧 / 17,328 B `DAD1F1C9…` | **否** |
| SCAR_1hmReadyDummy | 6.3333 s / 190 帧 / 25,168 B `8728A54A…` | 6.3333 s / 190 帧 / 25,168 B `8728A54A…` | **是（唯一相同）** |

**裁定：K1-C4 完成资源清单登记，但判定为资源不一致（NOT MATCHED）。** 两侧在**行为 clip 家族**上对齐（equip 都选 `1HM_Equip`、unequip 都选 `1HM_Unequip`，K2-C3 的 clip-family 结论保持），但除 SCAR dummy 外，`data FNV` 无一对齐，且同名 clip 的 duration/frames 都不同；源侧 OAR 还在窗口内自行替换资源。因此：

1. 不能把源/候选 `poseLocal` 当作“同一动画资源的两次采样”来比较或反推容差；
2. 不能用 `leaf name` 或 `binding-index` 做跨运行身份，必须按 `data FNV`（或完整路径+内容哈希）登记；
3. 未来的姿态对照必须先冻结 OAR 配置 / 关闭动态替换，或按资源哈希分组，否则同名不同资源会被误配。

**边界**：K1-C4 登记了本进程、本存档、本 OAR 配置下的活动 clip 资源清单；不闭合 K1-C1 的完整运行资源清单（project/behavior 文件与全部 OAR 替换源未逐一登记），不证明跨运行 ID 稳定，不完成 K5 的层/权重/混合计数，不产生 `tau_*`，不关闭 G5。
