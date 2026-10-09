# HKX11 实测证据：管理器表是项目级注册表，条目指向哈希记录而非动画（2026-10-10）

- 部署身份：`CharacterPanel-scopy-HKX11-a93c0051e3+dirty-…182823Z`——**提交前的中间构建**（animmgr
  代码与最终包完全一致，仅横幅身份串不同）；下轮起从最终 zip 部署，保证日志横幅可与 manifest 对表。
- 日志：[CharacterPanel-hkx11-animmgr-20261010.log](diagnostics/CharacterPanel-hkx11-animmgr-20261010.log)
  （SHA-256 `75ab8440ffab26209e6a4c96770b8bd108997726d6adb266e739a3d147fc29a7`）。**无崩溃**，绘制与
  程序化待机正常。

## 1. 现象

```
SCOPY ANIM animmgr queued=0 loaded=7872
SCOPY ANIM animmgr rejected-dump i=0 at=0x1a8c30dffd0 qwords='00786b68ad6f4173 3000000273e5c210 0 0 000001a705930d30 000001a6f3e407b0 0000000000004000 0'
SCOPY ANIM animmgr loaded i=0..15 file='-' crc=0xad6f4173/0x02777ff6/0xdb5c1156 … probe=not-object …
```

- **7872 条已装载**（≈15203/2，单项目动画数量级）——`loadedAnimations` 不是"最近使用"小表，而是
  项目级注册表（与 OAR 的"预载全部替换动画 + Havok 堆扩容补丁"互证）；
- 7872 条**全部** `probe=not-object`：`unk00` 不是 `hkaAnimation*`；
- CRC 反查（名字表 15203 条自算标准 CRC32）**全部 `-`**：这些 CRC 不是（或不只是）动画文件名的
  标准 CRC32——疑似行为文件名或另一变体。

## 2. 被拒转储的解码：记录形状与 OAR 的 `HashedBehaviorData` 精确吻合

```
+0x00 00786b68ad6f4173  = { crc32Filename=0xad6f4173, extension="hkx\0" }   ← AnimationFileInfo
+0x08 3000000273e5c210  = { crc32Path=0x73e5c210,    unk0C=0x30 }
+0x10..0x1F 0 0         = HashedData.unk10/unk14/unk18/unk1C = 0
+0x20 000001a705930d30  = stream?                                          ← OAR: stream @+0x20
+0x28 000001a6f3e407b0  = DBData*?                                         ← OAR: data @+0x28
+0x30 0000000000004000  = 0x4000（后续字段，未定性）
```

与 OAR Havok.h 的 `BShkbHkxDB::HashedBehaviorData{HashedData(0x20), stream@0x20, DBData*@0x28}`
（0x30 字节）逐字段吻合，且条目自身 fileInfo 与记录 fileInfo 同 CRC。多条目共享 CRC、data 指针以
0x30 步进连续排布——**`loadedAnimations[i].unk00` 指向项目 DB 的连续哈希记录，不是动画本体**；
已装载内容在再下一跳：`记录+0x28 DBData* → DBData : hkLoader { loadedData: hkArray<hkResource*> }`。

## 3. 判读与下一步

- `AnimationFileManagerSingleton` 的表是**项目级哈希注册表的指针阵列**，动画本体在
  `DBData.loadedData`（hkResource 包装）里——还差一到两跳；
- CRC 反查失败**不阻塞**：引擎自己的解析函数 `GetHashedAnimFromAnimIndex`（AE ID 63600，OAR 补丁
  点名，内部读 graph+0x200 的 ProjectDBData）按 binding 索引直接给出条目——引擎权威映射，无需定名；
- HKX12 = 调该解析器（索引取自 clip-seen 的 mt_idle binding-index 1022）→ 返回值落盘 → 按记录形状
  走 `+0x28 DBData*` → `loadedData` 有界两级遍历，凡过裸动画校验（type/duration/轨道数）者即
  mt_idle——链是按索引从引擎拿的，无需 CRC 定名。
