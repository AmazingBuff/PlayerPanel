# HKX4 实测证据：元素形态与 binding 的落点（2026-10-10）

- 构建身份：`CharacterPanel-scopy-HKX4-215cea8805-cl94faaed0c6-20261009T162845Z`，与
  `dist/CharacterPanel-scopy-HKX4-1.2.1.zip` 内 `build-manifest.json` 一致（`source_baseline_dirty=false`，基线 `215cea8805`）。
- 日志：[CharacterPanel-hkx4-layout-20261010-0033.log](diagnostics/CharacterPanel-hkx4-layout-20261010-0033.log)
  （SHA-256 `dd427b94f7267f23008ffddb64b98b2b0be38c0f3eca9e5b240b8c3bc7719ad6`）。**无崩溃**；两次捕获，绘制正常。

## 1. 原始转储把元素形态写得很清楚

```
SCOPY ANIM catalogue elements=8 object-like=8/8
SCOPY ANIM catalogue raw index=0    qwords='00007ff6ef2649b8 000000000001ffff 0000000000000000 0000000000000000 8000000000000000 0000000000000000 00007ff6ef2649d8 000000000001ffff'
SCOPY ANIM catalogue raw index=7601 qwords='00007ff6ef2649b8 000000000001ffff 0000000000000000 0000000000000000 8000000000000000 0000000000000000 00007ff6ef2649b8 000000000001ffff'
```

按 qword 读出来：

| 偏移 | 内容 | 判读 |
| --- | --- | --- |
| +0x00 | `00007ff6ef2649b8` | **vtable**（落在游戏镜像里） |
| +0x08 | `000000000001ffff` | `memSizeAndFlags=0xffff` + `referenceCount=1` —— 正是 `hkReferencedObject` 的两个字段 |
| +0x10 / +0x18 | 0 / 0 | 一个**空 `hkArray`**（data/size/capacity 全 0） |
| +0x20 / +0x28 | `8000000000000000` / 0 | 非指针的字段（未定名，不参与判读） |
| **+0x30** | `00007ff6ef2649d8` | **第二个 vtable**：又一个 `hkReferencedObject` 从这里开始 |
| +0x38 | `000000000001ffff` | 它的 `memSizeAndFlags` + `referenceCount=1` |

→ 元素本身是一个 `hkReferencedObject` 派生的**包装对象**（+0x00），而 **`hkaAnimationBinding` 是它的成员、按值放在 +0x30**
（`hkaAnimationBinding` 自己也派生自 `hkReferencedObject`，所以它从这里带自己的 vtable 与引用计数）。
**HKX4 的候选表只走到 +0x20，差一格**——这就是那一轮"全部 `valid=0`"的原因。

## 2. 两个取值方式的失败原因与形态完全一致

- `as=value` 的每一行都是 `reasons='no-animation,…'`：把元素/中间偏移当 binding 时，+0x18（binding 的 `animation` 字段位置）
  读到的是 0 → `no-animation` ✓ 与转储一致。
- `as=pointer` 的每一行都是 `reasons='unreadable,…'`：这些偏移处的 qword 是 0 或非指针 → 目标不可读 ✓ 也一致。

**结论**：失败不是"数组不对"（8/8 元素非空可读、`object-like=8/8`），而是**偏移差一格**。

## 3. 顺带确认的名字表

`names == bindings == 15203` 再次成立；`idle-plain` 打出来的前几个是动作/物件待机
（`Animations\horse_rider\idle.hkx@705`、`Animations\idleblessingkneel_loop.hkx@711`…），
说明"文件名以 idle 开头"并不等于"角色自己的站立待机"，所以 HKX5 另加一组**基础待机**
（`idle.hkx` / `idleforcedefaultstate.hkx` / `mt_idle.hkx`）。

## 4. 下一步（HKX5，已实现并打包）

- **候选不再靠猜**：用**一个元素的形态**自动生成候选——某处 qword 像 vtable ⇒ 那里就是一个按值对象；某处 qword 是可读堆指针
  且其目标像对象 ⇒ 那里是一个指针成员。然后仍由 8 个采样点判定哪个候选是真的。
- **校验更强**（全部静态读取，不调用任何虚函数）：`hkaAnimation::type` 必须是引擎命名的类型之一、
  `duration` 合理、**`animation->numberOfTransformTracks` 必须等于 binding 的轨道表长度**（包装对象过不了这一条）、
  每条轨道索引 ≤ 骨数。
- **诊断更完整**：`type=` 直接说明是 spline 还是 interleaved（决定 HKX6 采样时要不要 chunk cache）、`frames=`、
  `skeleton-name=`、前 4 条轨道对应的**骨名**，以及 **`copy-resolved=K/N`**——这段动画有多少轨道能真正落到副本节点上
  （HKX6 的写入覆盖率，不需要采样就能算）。
