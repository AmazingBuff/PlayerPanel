# HKX12 实测证据：解析器返回文件注册表条目，已装载数据在更深的未探层（2026-10-10）

- 部署身份：`CharacterPanel-scopy-HKX12-2e22e8f38f+dirty-…184151Z`——提交前中间构建（代码与最终包
  一致，仅横幅身份串不同；连续两轮如此，下轮起从最终 zip 部署）。
- 日志：[CharacterPanel-hkx12-hashed-20261010.log](diagnostics/CharacterPanel-hkx12-hashed-20261010.log)
  （SHA-256 `e1c584908e1a2255ab3b0100288b4b40f0b79eec3bd3521758c95f4371c95fda`）。**无崩溃**，绘制与
  程序化待机正常。

## 1. 现象

```
SCOPY ANIM hashed index=1022 at=0x27164df4f00        ← 解析器调用成功，返回堆指针
SCOPY ANIM hashed record db-data=0xb6aaba92           ← "hkx"记录模式匹配，但 +0x28 不是指针
SCOPY ANIM hashed anims found=0
SCOPY ANIM play UNAVAILABLE reason=no-clip-generator
```

## 2. 判读：解析器返回的是"文件注册表条目"，不是动画对象

- `GetHashedAnimFromAnimIndex(graph, 1022)` 返回堆指针，记录开头 = {crc32Filename, `"hkx"`,
  crc32Path}——结构检查通过；
- **+0x28 = `0xb6aaba92` 是 32 位值（下一条目的 CRC/ID），不是 DBData 指针**。对照 OAR 类型表：
  `hashedAnimations` 是 **`BSTArray<HashedData>` 按值数组（0x20/条，只有文件信息、没有任何指针）**，
  `+0x28` 正好溢出到元素 1023 的字段。**解析器返回 `&hashedAnimations[1022]`**；
- 结合 HKX11：**"名字/索引注册"与"已装载数据"是分离的**——binding set（空桩）、hashedAnimations
  （纯文件信息）都不载数据。

## 3. 引擎动画存储的已探明地图（三轮合拢）

```
clip.animationBindingIndex ──► hashedAnimations[index]（HashedData，0x20，纯文件信息）   ← HKX12，到此为止
binding set 元素（hkbAnimationBindingWithTriggers，binding@+0x10）＝空桩                 ← HKX10
loadedAnimations[i].unk00 ──► 哈希记录{AnimationFileInfo, …, stream*@+0x20, ptr*@+0x28}  ← HKX11，ptr 未跟进
管理器还有 unk68/unk88 两个 void* 数组未探
```

已装载动画数据只剩两个未探去处：**管理器记录的 ptr1/ptr2**（HKX11 转储中 +0x20/+0x28 的两个堆
指针，疑似 stream 与已载数据）与**管理器的 unk68/unk88**。hkResource 内部布局（若走到那）仍未知。

## 4. 下一步的两个选项（用户暂缓拍板）

- **HKX13（最后一跳探针）**：跟进管理器记录的 ptr1/ptr2（校验 + 0x40 转储 + 裸动画校验）+
  用 CRC32("mt_idle") 在 7872 条里点名（若 CRC 变体恰好标准即可精准定位）；拿到即接已铺好的
  ground-truth + F2 驱动。**检查点：若仍拿不到动画对象，转路线 D**——hkResource 内部布局是
  再下一个未知层，继续挖的边际收益递减。
- **路线 D（poseLocal 录制-回放）**：机制全部已验证（HKX2 读 poseLocal、写入路径、F2 开关、
  ground-truth 协议），小时级工程，直接交付 S2 待机验收；A″ 三轮的结构图成果（虚表判类仪器、
  binding set/管理器/注册表布局）留作"展示动作切换"的地图。
