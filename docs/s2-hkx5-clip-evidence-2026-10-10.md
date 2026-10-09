# HKX5 实测证据：绑定集元素是 0x30 字节的桩，路线改走 `hkbClipGenerator`（2026-10-10）

- 构建身份：`CharacterPanel-scopy-HKX5-3cd816a1eb-cl94faaed0c6-20261009T163707Z`，与
  `dist/CharacterPanel-scopy-HKX5-1.2.1.zip` 内 `build-manifest.json` 一致（`source_baseline_dirty=false`，基线 `3cd816a1eb`）。
- 日志：[CharacterPanel-hkx5-clip-20261010-0041.log](diagnostics/CharacterPanel-hkx5-clip-20261010-0041.log)
  （SHA-256 `1c68d223415e9f2fb09afa1cfbfa2cbf9fef074d3d1fa935cff022202df6d1cc`）。**无崩溃**；两次捕获，绘制正常，
  只有既有的 `Studio lights …` 警告。

## 1. 0x80 字节转储：vtable 每 0x30 字节重复

```
raw index=7601 qwords='00007ff6ef2649b8 000000000001ffff 0 0 8000000000000000 0
                     00007ff6ef2649b8 000000000001ffff 0 0 8000000000000000 0
                     00007ff6ef2649b8 000000000001ffff 0 0'
```

| 偏移 | 内容 |
| --- | --- |
| +0x00 / +0x30 / +0x60 | 同一个 vtable `…49b8`，每 0x30 重复一次 |
| 每段 +0x08 | `memSizeAndFlags=0xffff` + `referenceCount=1`（`hkReferencedObject` 头） |
| 每段 +0x10..+0x2F | 0、0、`0x8000000000000000`、0 |

判读：**元素是 0x30 字节的小对象**，三个一段挨着排（分配器把同类对象连续放置），
其中**没有任何 `hkaAnimationBinding` 的字段**——`hkaAnimationBinding` 光自身就有 0x48 字节，
不可能塞进 0x30。所以 HKX4/HKX5 的候选（+0x00、+0x30 两处 vtable）全部 `no-animation`，与转储完全一致。

## 2. 结论：绑定集这条路性价比已经不对

- 想从这条路线拿到 `hkaAnimation*`，还要继续猜这个 0x30 字节桩的含义（它既不是 binding，也不含指向 binding 的指针）；
- 而 **`hkbClipGenerator` 在 CommonLibSSE 里全类型**：`animationName`（+0x48）、`binding`（+0xA0）、
  `animationControl`（+0x88）、`playbackSpeed`（+0x64）、`mode`（+0x72）都有确定偏移。
  我们只需要**找到一个实例**，剩下的读取全部类型化。

## 3. 顺带确认（沿用未变）

- `names == bindings == 15203`、`character='DefaultFemale'`、`rig='Character Assets Female\skeleton_female.hkx'`、
  `behavior='Behaviors\0_Master.hkx'` 再次成立；
- `idle-base` 找到 `Animations\horse_rider\idle.hkx@705` 与 `Animations\female\mt_idle.hkx@1022`，
  后面 11909+ 全是 **OpenAnimationReplacer** 的替换件（路径以 `data\meshes\...` 开头）——
  所以 HKX6 的选择规则里，**引擎自己那份（路径不以 `data\` 开头）优先**，mod 替换件排后。
- `mt_idle.hkx` 就是角色站立待机（MT = movement type），这也是 HKX6 要播的那一段。

## 4. 下一步（HKX6，已实现并打包）

1. **发现**：从 `behaviorGraph->rootGenerator` 起做**有界**遍历；状态机的 `states` 是类型化的按名跟进，其余节点只跟
   "首字像 vtable"的指针；候选必须通过 clip generator 校验（名字像 `.hkx`、binding 结构性通过、`mode ≤ 3`、
   `playbackSpeed` 合理）才被采信。
2. **采样**：`SampleIndividualTransformTracks(time, tracks, n, out)` —— **没有 chunk cache 参数**，
   于是"spline 动画是否需要 `hkaChunkCache`"这条悬案不必先解决就能采样。
3. **真值**：把 clip 在每个相位采样一次，与引擎当前姿态（`poseLocal`，**按 `boneNodes` 索引**）逐骨比较，
   打印最佳/最差相位——`match` 即"某个相位能复现引擎姿态到 1% 以内"。
4. **驱动**：F2 由**我们自己的时钟**推进（暂停时引擎不推进），每帧采样→写 local→重算子树世界；
   没有 clip 时回退程序化待机并在日志里写明。
