# C-G5-CAL K1 identity census（2026-11-14 预注册）

**状态：PRE-REGISTERED，尚未运行。** 本目录登记 K1-C2/C3/C4 的只读身份普查：图身份、完整骨架/参考姿态、活动 clip 的资源与实例身份。它不采样姿态、不写图、不推进图、不新增 hook。

## 一、目标与边界

本轮只补 CAL v1 §三 K1 中可由现有只读观察覆盖的三项：

| K1 项 | 本轮交付 | 不覆盖 |
| --- | --- | --- |
| K1-C2 图身份 | 源/候选的 graph、holder、root、project、project-db、behavior、setup、bindings、poseLocal/count、boneNodes、numAnimBones、activeNodes、skeleton | 源/候选的 generation/view 同代关系；controller 所有权证明 |
| K1-C3 骨架与映射 | 完整骨名、父索引、reference pose（10 个 raw float32/骨）、float slots；`recorded==bones`、`invalid-parent=0`、`nonfinite=0` | binding/default-fill 的逐轨语义（另见 K1-C4 的 map hash） |
| K1-C4 资源与 clip 实例 | 每个活动 clip 的 node/binding/animation 指针、binding-index、clip 名、mode/localTime/speed/enforced/crop、动画 header、spline `data`/`blockOffsets` FNV、binding map FNV/blendHint、进程内 stable-id | 跨运行稳定 ID；OAR replacement 的完整配置清单；binding set 全表（只覆盖活动 clip） |

**明确不做**：不读/不比较 `poseLocal`，不调用 `SamplePartialTracks`，不创建第二 Actor，不写源图，不新增 detour，不产生 `tau_*`，不关闭 K1-C1 或 K5/K6/G5。

## 二、触发与采集点

- 开关：`[Experiment] ClipIdentityCensus=true`，默认 off；要求 `IndependentGraph=inject`。
- 源侧：`observe_source_baseline` 之后，`side=source phase=pre-activate`，dump 骨架 + 活动 clip。
- 候选侧：`post-activate`，dump 骨架 + 活动 clip；随后在 equip 段第一个 completed Generate（`completed=1`）与 unequip 段第一个 completed Generate 各 dump 一次活动 clip（`phase=equip-clips` / `unequip-clips`，不重复骨架）。
- 次数：每次实验源 1 次、候选 3 次（post-activate + equip-clips + unequip-clips）；同一进程只跑一次实验。

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
