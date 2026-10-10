# HKX13 实测证据：实例侧确实存在活 binding，正在播放的 idle 已能取到动画指针（2026-10-10）

- 构建身份：`CharacterPanel-scopy-HKX13-47725682fa-cl94faaed0c6-20261010T135149Z`，与包内 manifest 一致
  （`source_baseline_dirty=false`，基线 `47725682fa`）。**DLL 与 PDB 均从最终 zip 部署**
  （DLL SHA-256 `b2869467…`、PDB `ca632f3d…`，已校验一致）——HKX11/12 部署中间构建的问题已纠正。
- 日志：[CharacterPanel-hkx13-instance-20261010.log](diagnostics/CharacterPanel-hkx13-instance-20261010.log)
  （SHA-256 `dd77200c…`，24666 字节）。**零崩溃、零 `[E]`**；唯一告警是既有的
  `Studio lights not fully served by the ledger yet`。绘制与程序化待机正常（`SCOPY IDLE bound 8/8`、
  `SCOPY DRAW frame=1 passes=23`）。

## 1. 核心结论：实例侧有活 binding

```
SCOPY ANIM instance clip name='Animations\male\MT_Idle.HKX' idx=12066 clone-state=kClone
        binding=0x28b4433a770 control=0x28b730fab40 bind-src=clip tracks=0 reason=tracks-unreadable
SCOPY ANIM instance clip name='Animations\male\weaponAdjustment.hkx' idx=1629 clone-state=kDefault
        binding=0x28b7333ded0 control=0x0 bind-src=bindingset tracks=7 reason=valid
SCOPY ANIM instance verdict=binding-resolved
```

**这是 HKX1 以来第一次在引擎对象上同时拿到非空的 `binding` 与 `control`。** 关键定性：

| 对象 | `cloneState` | `binding` | `control` | 判读 |
| --- | --- | --- | --- | --- |
| `MT_Idle.HKX`（idx **12066**） | **`kClone`** | **0x28b4433a770** | **0x28b730fab40** | **活跃实例**，两者都活 |
| `weaponAdjustment.hkx`（idx 1629） | `kDefault` | 0x28b7333ded0 | 0x0 | binding 活，过全部校验（`tracks=7 reason=valid`） |
| `MT_Idle.HKX`（idx 1022） | `kTemplate` / `kDefault` | 0x0 | 0x0 | 模板，空 |

→ **模板空、克隆活，与静态假设完全吻合。** 且注意 `idx=12066` 而非 HKX10 一直盯着的 1022——
12066 是 OAR 替换后的实际索引（日志里 `idle-base` 列表含大量
`OpenAnimationReplacer\...\mt_idle.hkx@11908/11915/11917…`），**说明我们此前按名字锁定 1022 找的是模板，
而真正在播的是 OAR 替换件**。

## 2. OAR 布局实锤：`binding@+0x10`、`0x28` 步进

```
SCOPY ANIM instance element qwords='00007ff69ed249b8 000000000001ffff 0000028b4433a770 0000000000000000
                                     8000000000000000 0000000000000000 00007ff69ed249b8 0000000000000100'
```

| 偏移 | 值 | 语义 |
| --- | --- | --- |
| +0x00 | `00007ff69ed249b8` | vtable（`hkbAnimationBindingWithTriggers`） |
| +0x08 | `000000000001ffff` | `memSizeAndFlags`\|`refCount`=1 |
| **+0x10** | **`0000028b4433a770`** | **`binding`** ← 与 clip 自己读到的 `0x28b4433a770` **完全一致** |
| +0x18–0x28 | 0 / `8000000000000000` / 0 | `triggers` hkArray（空） |
| +0x30 | `00007ff69ed249b8` | **下一个元素的 vtable → 步进 = 0x30** |

两条独立路径（clip 的 `binding` 成员 vs 绑定集元素 +0x10）**读到同一个指针**，互相印证。

**一处与 OAR 不符**：步进实测 **0x30**，OAR 的 `static_assert(sizeof==0x28)` 给的是 0x28。
字段位置（+0x10）没错、两条路互证也没错；差异在**元素间距**（可能是分配对齐）。
→ 结论：**+0x10 的字段偏移可用，但不要用 sizeof 去步进索引元素**（我们本来就用 `bindings[index]` 指针数组，不受影响）。

## 3. 新暴露的阻塞点：`tracks-unreadable`

正在播放的那个 idle（`idx=12066`）binding 指针活、但 `read_binding` 报 **`tracks-unreadable`**：
`transformTrackToBoneIndices` 的数组不可读。而 `weaponAdjustment`（7 轨）能过全部校验。

两个可能（**未裁定**）：

1. 该 binding 的轨道表尚未装载完成（惰性），数组指针/大小还是垃圾；
2. `hkaAnimationBinding` 在该实例上是**另一种布局**（例如引擎自有的派生类型），`transformTrackToBoneIndices`
   不在 CLib 声明的 +0x20。

区分方法明确：下一轮把 `read_binding` 的失败面加细——打印 `tracks.size`、`tracks.data()`、
以及 binding 对象自身的 `animation->numberOfTransformTracks`，再对 binding 做一次 0x48 字节原始转储。

## 4. 与既有判读的关系

- **HKX10「标准 Havok binding/control 层在运行时是死的」→ 正式推翻。** 它扫的是模板树
  （`root-sm` 转储 `cloneState=kTemplate`），本轮在同一张图上找到 `kClone` 且 binding/control 都活。
- **`root-clone=0x0`、`roots clone=template`**：`rootGeneratorClone` 成员为空，实例是经
  **`activeNodes`（106 个）** 到达的——`activeNodes` 才是本运行时可用的入口，不是 `rootGeneratorClone`。
- `setup-bindingset=0x0/0`：**该角色只有 `character` 那份**（15203 条），OAR 的回退分支在本样本上未触发。
- `clone-states='kDefault:156, kClone:95, unknown:167, kShareable:12, kTemplate:37'`——
  `unknown` 占多数提醒：`cloneState` 只在**类型化对象**上有意义，对扫描到的裸指针读它是噪声（无害，未参与判定）。
- HKX11/12 的管理器/注册表地图仍然有效，但它们描述的是**文件注册层**，与这里的**实例层**是两回事；
  本轮证明动画对象要在实例层取，不必再往下挖注册表。

## 5. 关卡状态

| 关卡 | 状态 | 说明 |
| --- | --- | --- |
| H01 构建／实例身份 | **通过** | 横幅与 manifest 一致；`kClone` 实例 + OAR 替换件索引 12066 已定位 |
| H02 类型／持有 | **部分** | binding 指针已取得且两条路互证；**轨道表不可读**，映射未成立；长期持有未验证 |
| H03 采样 | 未测 | 需先解决 `tracks-unreadable` |
| H04 副本驱动 | 未测 | — |
| H05 隔离／生命周期 | 未测 | 本轮零崩溃、零 `[E]`，但未做换会话/读档 |
| S1 安全回收 | 仍阻塞 | 未涉及 |

## 6. 下一轮的最小范围

只解决 `tracks-unreadable`，其余不动：

1. `read_binding` 失败时打印 `tracks.size`、`tracks.data()`、`animation->numberOfTransformTracks`；
2. 对 binding 对象 dump 0x48 字节，看 `transformTrackToBoneIndices`（CLib +0x20）是否为合法 hkArray；
3. 若确定为布局变体 → 用 `animation->numberOfTransformTracks` 反推，或按 `binding->animation` 的类型另选映射来源；
4. 若确定为惰性未装载 → 改在未暂停帧观察（PRD §6.4 第 2 条允许：O 正常世界更新中被动观察）。

仍须遵守：只读、不虚调用、**从最终 zip 部署**、测完立刻复制日志。
