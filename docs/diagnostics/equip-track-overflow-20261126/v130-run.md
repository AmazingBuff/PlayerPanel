# v130 轨道数诊断 · 上机取证步骤

## 目的

确认 Skyrim 运行时读到的 `numberOfTransformTracks` 原始值，验证 242 / 156 是否为引擎实际所见（而不是只在离线 HKX 里）。

## 身份

| 件 | 字节 | SHA-256 |
| --- | ---: | --- |
| CharacterPanel.dll | 2,193,408 | `bd1246e531138b455169c3c6f8ff189f207dd92e6e53fe0fdf9fe864866feddd` |
| CharacterPanel.pdb | 57,192,448 | `80b56b00915ac33ffc5440683242a6354c0abc93118acc2c533a4cd036284c76` |
| 运行 INI | 20,242 | `91acf06f7ea751ae6b5c442cc9d9d22230b4ca507f16ad3d028dbcf0d8c7a3ff`（沿用 v128 验证档，`DetailedAnimationTrace=true` + `DisplayPose=false`） |

构建身份：`CharacterPanel-scenegraph-v130-track-diag-20261126-d1278da4d9-cl94faaed0c6-20261126T153724Z`（干净树，HEAD `d1278da`）。

## 回退

`dist/v130-track-diag-20261126/CharacterPanel-pre.{dll,pdb,ini}` = v128（DLL `164EACEA…` / PDB `8ADC5234…` / INI `91ACF06F…`）。

## 操作

只需要**复现一次弓和 Axe 的换装**即可，不必跑完整流程：

1. 启动游戏，读档，确认日志首行 `SCENEGRAPH BUILD` 与本表一致。
2. 收刀待机。
3. 打开背包，**装备弓**，停留几秒。
4. 装备 **Axe**，停留几秒。
5. 关闭背包，退出。

## 判读

`asset ready` 行现在多两个字段：

```
... mapping=identity-unverified tracks=116 ... hkx-tracks=242 skeleton-bones=116 ...
                              ^^^^^^^^^          ^^^^^^^^^^^^  ^^^^^^^^^^^^^^^
                              截断后             HKX 原始      骨架骨骼数
```

**关键问题**：`hkx-tracks` 对弓是否显示 242、对 Axe 是否显示 156。

| 观察 | 含义 |
| --- | --- |
| `hkx-tracks=242` / `156` | 引擎确实看到超骨骼数的轨道 → 截断假设成立，错位机制确认 |
| `hkx-tracks=116` | 引擎自身已把轨道规整到 116 → 离线 HKX 与运行时不同，错位另有原因，需转向别处 |

同时留意 `skeleton-bones` 是否为 116。

## 边界

- 本次只加诊断，**不改任何行为**：`tracks`（截断值）字段含义不变，只是旁边多了原始值。
- 不改姿态、选择或分支。
- 与 v129 的 `OAR CORRECTION` 日志点**互不干扰**（两个独立文件）。
