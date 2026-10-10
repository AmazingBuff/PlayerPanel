# HKX14 实测证据：布局是对的，是轨道表本身没填——且 `hkaAnimationControl` 给出了映射（2026-10-10）

- 构建身份：`CharacterPanel-scopy-HKX14-6b479c8367-cl94faaed0c6-20261010T141937Z`，与 manifest 一致
  （`source_baseline_dirty=false`，基线 `6b479c8367`）。DLL `80644b6a…`、PDB `e16dd718…` 均从最终 zip 部署并校验。
- 日志：[CharacterPanel-hkx14-tracks-20261010.log](diagnostics/CharacterPanel-hkx14-tracks-20261010.log)
  （SHA-256 `a71f8c71…`）。**零崩溃、零 `[E]`**，唯一告警是既有的灯光告警。绘制与程序化待机正常。

## 1. 裁决：不是布局变体，是轨道表没填

```
SCOPY ANIM tracks binding=0x1b243b53a90 array=0x0 size=0 declared=99 readable=false array-at=0x1b243b53ab0
SCOPY ANIM instance anim binding=0x1b243b53a90 animation=0x1b243b1a9c0 probe=valid type=spline duration=30.00s transform-tracks=99 float-tracks=0
```

**`declared=99` 完全合理**（与 `animation->numberOfTransformTracks=99` 自洽），但 `array=0x0`、`size=0`。
按上一轮写好的判读表 → **「惰性未装载」**：binding 对象和动画对象都是活的、布局也对，**只有轨道表还没填**。

## 2. CLib 的 `hkaAnimationBinding` 布局实锤正确（转储逐字段对上）

```
SCOPY ANIM tracks binding qwords='00007ff69ed45430 0000000000000000 000001b243b53ae0 000001b243b1a9c0
                                   0000000000000000 8000000000000000 0000000000000000 8000000000000000
                                   0000000000000000'
```

| 偏移 | 值 | 语义 |
| --- | --- | --- |
| +0x00 | `00007ff69ed45430` | vtable（`hkaAnimationBinding`） |
| +0x08 | 0 | `memSizeAndFlags`\|`refCount` |
| +0x10 | `000001b243b53ae0` | `originalSkeletonName`（堆指针） |
| **+0x18** | **`000001b243b1a9c0`** | **`animation`** ← 与日志自报的 `animation=0x1b243b1a9c0` **逐位相同** |
| +0x20 | `0` | `transformTrackToBoneIndices._data` ← **空** |
| +0x28 | `8000000000000000` | `_size|_capacityAndFlags` ← size=0 |
| +0x30/0x38 | `0` / `8000…` | `floatTrackToFloatSlotIndices`（同样空） |
| +0x40 | 0 | `blendHint`\|pad |

9 qwords = **72 = 0x48 = `sizeof(hkaAnimationBinding)`** ✓，且 `animation@+0x18` 与独立读到的指针逐位吻合 →
**布局零疑问，HKX4/5 的"布局错"判读彻底排除。缺的只是数据。**

## 3. 复现性：两轮都指向同一个结论

| 轮次 | 实例 idx | `cloneState` | binding | 动画 | 轨道表 |
| --- | --- | --- | --- | --- | --- |
| HKX13 | 12066 | kClone | 活 | — | `tracks-unreadable` |
| HKX14 | **12221** | kClone | 活 | spline / 30.00s / 99 轨 | `array=0x0 size=0` |

索引在两轮间变化（12066 → 12221），**符合 OAR 按条件动态替换**的行为——进一步佐证"按名字锁 idx=1022 找的是模板"。

对照组 `weaponAdjustment.hkx`（idx 1629）两轮都 `tracks=7 reason=valid`：
**同一个校验器、同一条写入路径，在另一个 binding 上全通过** → 证明校验与写入没问题，问题只在这一个 binding 的数据。

## 4. 已找到的解法：`hkaAnimationControl` 自带映射访问器

`hkaAnimationControl`（CLib 全类型）声明了两个**非 const 读方法**：

```cpp
virtual std::int32_t        GetNumTransformTrackToBoneIndices() const;   // 0A
virtual const std::int16_t* GetTransformTrackToBoneIndices() const;      // 0B
```

正在播放的实例 `control=0x1b27305d380` **非空**，且 `hkaAnimationControl::binding`（@0x38）正是同一个 binding。
→ **下一轮可直接经 control 取轨道映射，不必等引擎填 binding 的表**。

**代价与风险（必须写明）**：这两个是**虚函数**，与本项目"不虚调用候选对象"的红线冲突。
差别在于——HKX7 的崩溃是**在未确证对象上**虚调用判类；这里的对象是**虚表已确证、且 binding 指针已两条路互证**的活实例。
是否破例**需用户拍板**，不由我单方面决定。
更保守的替代：若 `control->binding` 与 clip 的 binding 是同一对象（几乎必然），
那么 control 侧的表同样是空的，虚调用也救不了——**这一点上机即可判**。

## 5. 关卡状态

| 关卡 | 状态 | 说明 |
| --- | --- | --- |
| H01 构建／实例身份 | **通过** | 两轮复现；kClone 实例 + 动态 OAR 索引 |
| H02 类型／持有 | 部分 | binding＋animation 对象已取得且布局实锤；**映射未成立**（表未填） |
| H03 采样 | 未测 | 需先有映射 |
| H04／H05 | 未测 | 零崩溃但未做换会话 |
| S1 安全回收 | 仍阻塞 | 未涉及 |

**本轮交付的是"排除了布局错"这一整类假设**，把问题从"我们读错了"收敛到"引擎还没填"。

## 6. 下一步（待用户拍板）

1. **A（推荐先判，零风险）**：对活实例的 `control` 做**只读**转储 —— 打印 `control->binding`（@0x38）是否为同一个
   `0x1b243b53a90`，以及 `control` 自身 `GetNumTransformTrackToBoneIndices` 之外的**字段**（不虚调用）。
   若 control 侧表同样为空 → 虚调用无用，转 B。
2. **B（换时点）**：PRD §6.4 第 2 条允许 —— 在 **O 正常世界更新（未暂停）** 时被动观察同一实例，
   看轨道表是否被填上。这需要把探针从"仅在暂停背包捕获时"改为"世界帧也可记录一帧"。
3. **C（用 animation 自报轨道数 + 骨架同名假设）**：`declared=99` 且动画与骨架同源，
   但 PRD §6.4 **明确禁止**用"轨道 i = 骨骼 i"替代映射 —— **仅在拿到真值 A/B 验证后才可接受**，不作为首选。
4. **D（虚调用 control 的两个访问器）**：最快，但破红线，需用户明确批准。

### 选项 A 已发包（HKX15）：只读转储 control，判定 D 是否值得破例

- 提交 `5a1af365b0`，包 `dist/CharacterPanel-scopy-HKX15-1.2.1.zip`，身份
  `CharacterPanel-scopy-HKX15-5a1af365b0-cl94faaed0c6-20261010T143103Z`（`source_baseline_dirty=false`）。
- DLL `a8dc0e90…`、PDB 均从最终 zip 部署并校验一致。
- 新增日志：`SCOPY ANIM instance control …`（`binding@0x38`、**是否与 clip 的 binding 同一对象**、
  `localTime`/`weight`、两个权重数组 size、control 的 0x58 字节转储），以及当 control 的 binding
  **是另一个对象**时的 `instance control tracks …`。
- **全程只读、零虚调用**——不碰红线。

**判读表**：

| 观测 | 结论 | 下一步 |
| --- | --- | --- |
| `same-as-clip-binding=true` | 访问器读的是同一张空表 → **D 无用** | 转 B（未暂停帧观察） |
| `same-as-clip-binding=false` 且 `control tracks readable=true` | control 侧有真映射 → **D 值得，或直接用该表** | 拍板是否虚调用；或先试直接读该数组 |
| `binding@0x38=0x0` | control 还没绑 | D 无用，转 B |
| control 转储里出现「堆指针 + 小整数」且有 99 量级整数 | control 自带映射表（非 binding） | 用转储定偏移 |

上机：进游戏 → 打开背包（暂停）→ **F7 捕获** → 看 `instance control …` →
测完**立刻复制** `Documents\My Games\Skyrim Special Edition\SKSE\CharacterPanel.log`
（mod 目录下那份是 10/05 的旧日志，非活动）。
