# HKX18：由引擎装载游戏已注册的动作，再取其数据（2026-10-10）

## 1. 设计原则（用户澄清，已接受并记为产品要求）

面板人物的动作**不必与游戏角色当前在播的动作一致**，但**必须是游戏已注册的动作**——
原版自带或经 OAR 注册的，**不是我们额外引入的资源**。

因此取数流程固定为：

```
需要某个动作（例如 female idle）
  → 从角色自己的动画名表里命名一个索引（原版／OAR 注册）
  → 由引擎装载它
  → 从引擎的已装载存储里取出动画数据
  → 用自己的时钟采样，作用于面板人物
```

**推论（要一并记住）**：放在 `SKSE\Plugins\animations\` 下的两个 hkx 文件（`mt_idle0_d.hkx`、
`mt_idle0_j.hkx`）**不满足"游戏已注册"**——它们不在引擎的动画名表里，引擎不会装载它们。
若要使用，必须放到引擎／OAR 会扫描的路径使其被注册。**本轮实现完全不读这两个文件。**

## 2. 本轮实现（HKX18）

- 提交 `874bbc32`，包 `dist/CharacterPanel-scopy-HKX18-1.2.1.zip`，身份
  `CharacterPanel-scopy-HKX18-874bbc3230-cl94faaed0c6-20261010T162851Z`（`source_baseline_dirty=false`）。
- DLL `69877a3b…`、PDB **均已从最终 zip 部署并校验一致**；单测 1/1 通过。

装载调用参照 OAR 的 `LoadAnimation`（[OpenAnimationReplacer.cpp:772-802](../OpenAnimationReplacer/src/OpenAnimationReplacer.cpp#L772)）：
装载入口只需要一个**带 `animationBindingIndex` 的类 clip-generator 对象**。两处与 OAR 不同，
都是为了安全：

| 项 | OAR 做法 | 本轮做法 |
| --- | --- | --- |
| carrier 大小 | 0x72 字节的简化结构 | **按 `sizeof(hkbClipGenerator)`=0x110 的定长缓冲** —— 引擎若写入更远的字段不会踩栈 |
| context | `*reinterpret_cast<hkbContext*>(&character)`（栈垃圾） | **0x50 定长零缓冲**，只填 `character`@0x00 与 `behavior`@0x08 |

取数走的是 **HKX11 找到、HKX12 走错**的那条链：

```
loadedAnimations[i].unk00 → +0x28 hkLoader*(DBData) → loadedData(hkArray) → hkResource → hkaAnimation
```

HKX12 之所以 `found=0`：它调 `GetHashedAnimFromAnimIndex`，返回的是 `hashedAnimations[]`
（0x20 字节、纯文件信息、**无指针**），读 +0x28 只会溢出到下一条目。指针在 `hashedBehaviors` 侧。

**日志面**（新增）：

```
SCOPY ANIM load before records=N anims=M
SCOPY ANIM load index=… queued=… loaded=… loaded-records=N
SCOPY ANIM load anim at=0x… type=spline duration=…s tracks=… data-present=true|false
SCOPY ANIM load verdict=load-produced-animation|no-new-animation anims-before=… anims-after=…
```

装载前后各扫一遍已装载存储 —— **把"这次装载产生的"与"本来就在的"分开**，这是判读的关键。

## 3. 判读表

| 观测 | 含义 | 下一步 |
| --- | --- | --- |
| `verdict=load-produced-animation` 且 `data-present=true` | **装载成功且数据完整** | 接 ground-truth 相位扫描 → F2 播放 |
| `anims-after` 有动画但全部 `data-present=false` | 装到了对象，数据仍未就绪 | 改在**未暂停帧**做装载（选项 B） |
| `loaded=false`、`anims-after=0` | 装载调用未生效（索引或 carrier 形状问题） | 看 `queued`/`loaded` 返回值定因 |
| `db-data` 行全无 | `+0x28` 不是 loader 指针 | 回看 HKX11 转储重新定偏移 |

## 4. 风险（必须写明）

`Queue` / `Load` 是**有副作用的引擎调用**（会真的装载并常驻资源），与前几轮"纯只读"不同。
本轮已做的缓解：carrier 与 context 都是定长零缓冲；每次调用前后都有日志，若崩溃日志会指到具体一步。
**不敢断言绝对安全**——这是用户明确要求的方向，按"有界实验"处理。

## 5. 关卡状态

| 关卡 | 状态 |
| --- | --- |
| H01 构建／实例身份 | 通过 |
| H02 类型／持有 | 部分 —— 装载链本轮才第一次走；持有仍待验证（引擎装载的资源由引擎持有） |
| H03 采样 | 未测（需先有 data-present=true 的动画） |
| H04 副本驱动 | 未测 |
| H05 隔离／生命周期 | 未测 |
| S1 安全回收 | 仍阻塞 |

## 6. 上机

进游戏 → 打开背包（暂停）→ **F7 捕获** → 看 `SCOPY ANIM load …` 系列 → 测完**立刻复制**
`Documents\My Games\Skyrim Special Edition\SKSE\CharacterPanel.log`。
（mod 目录下那份是 10/05 的旧日志，非活动。）

## 7. HKX18 实测结果（2026-10-11 00:45）

日志 `CharacterPanel-hkx18-load-20261011-0048.log`。**零崩溃**（最新 crash 仍是 22:54 的 HKX16 那次）。

```
SCOPY ANIM load before records=7872 anims=0
SCOPY ANIM load index=12221 queued=1 loaded=true loaded-records=7871
SCOPY ANIM load verdict=no-new-animation anims-before=0 anims-after=0
```

**最重要的正结果**：`queued=1`、`loaded=true` —— **引擎接受了按索引的装载调用并返回成功**。
用户要的机制（命名游戏已注册的索引 → 引擎装载）**本身是通的**。

**没成立的一半**：遍历仍 `anims=0`，且 `db-data` 行**从未打印**（该行在 HKX18 里被我遗漏，
是诊断缺陷）。`loaded-records` 由 7872 变为 **7871**（少 1），说明 `Load` 确实动过表。

## 8. HKX19：把失败归因到具体一步，并修一个真 bug

- 提交 `8282e897`，包 `dist/CharacterPanel-scopy-HKX19-1.2.1.zip`，身份
  `CharacterPanel-scopy-HKX19-8282e8977d-cl94faaed0c6-20261010T165014Z`（`source_baseline_dirty=false`）。
- DLL `44b7b8c0…`、PDB **均已从最终 zip 部署并校验一致**；单测 1/1。

**修的真 bug**：HKX18 把 `hkResource` 的内容当成 `+0x00` 处的指针读——**那是 vtable**。
OAR 断言 `sizeof(hkResource) == 0x10`（只有引用计数头、**无内容成员**），
所以对象要么在**头之后（+0x10）**，要么在资源自身窗口里的某个指针后面。
HKX19 两个位置都试，且每个候选都先过校验，**表头再也不会被当成对象**。

**新增诊断**（分级计数，一次定位死在哪一步）：

```
SCOPY ANIM load before records=… current=0x… current-crc=0x… records-walked=… with-loader=… with-resources=… resources=… anims=…
SCOPY ANIM load loader qwords='…'      （首个 loader 的原始 0x20 字节，供对布局）
SCOPY ANIM load after  records=… current=0x… current-crc=0x… records-walked=… with-loader=… with-resources=… resources=… anims=…
```

`currentAnimation` 槽（管理器 @0x20）单独打 —— 那是**装载刚触碰的那条记录**。
记录扫描**上界 64 条**（表里几千条，每条一次窗口扫描）。

**判读表**：

| 观测 | 含义 | 下一步 |
| --- | --- | --- |
| `with-loader=0` | `+0x28` 不是 loader 指针 | 看 `loader qwords` 与 `current` 定真偏移 |
| `with-loader>0` 但 `with-resources=0` | loader 的 `loadedData` 为空 | loader 布局或时机问题 |
| `with-resources>0` 但 `anims=0` | 资源里没找到可校验动画 | 看资源窗口结构，改扫描范围 |
| `anims>0` 且 `data-present=true` | **取数成功** | 接 ground-truth → F2 播放 |

## 9. HKX19 实测结果（2026-10-11 00:51）：链路实锤正确，卡在资源内部

日志 `CharacterPanel-hkx19-load-20261011.log`。**零崩溃**。

```
SCOPY ANIM load before records=7872 current=0x0 current-crc=0xb89be175 records-walked=64 with-loader=64 with-resources=64 resources=64 anims=0
SCOPY ANIM load loader qwords='00007ff69ed463f0 000000000001ffff 000001fcf3f4c030 0000000100000001'
SCOPY ANIM load index=12219 queued=1 loaded=true loaded-records=7871
SCOPY ANIM load after  … records-walked=64 with-loader=64 with-resources=64 resources=64 anims=0
```

**这是本轮最重要的正结果 —— 那条链完全正确：**

| 计数 | 值 | 含义 |
| --- | --- | --- |
| `records-walked` | 64 | 看了 64 条记录 |
| **`with-loader`** | **64/64** | **`+0x28` 在每一条上都是可读的 loader 对象** |
| **`with-resources`** | **64/64** | **每个 loader 的 `loadedData` 都非空** |
| `resources` | 64 | 共 64 个资源（**每个 loader 恰好 1 个**） |
| `anims` | **0** | 但这 64 个资源里没有一个能校验成动画 |

`loader qwords` 逐字段实锤 **hkLoader 布局正确**：

| 偏移 | 值 | 语义 |
| --- | --- | --- |
| +0x00 | `00007ff69ed463f0` | vtable（hkLoader） |
| +0x08 | `000000000001ffff` | memSizeAndFlags\|refCount |
| **+0x10** | `000001fcf3f4c030` | **`loadedData._data`**（堆指针） |
| **+0x18** | `0000000100000001` | **`_size=1`、`_capacityAndFlags=1`** ✓ |

→ **HKX11 的地图被证实**：`loadedAnimations[i].unk00 → +0x28 hkLoader → loadedData`，
且 HKX12 走错的确实是数组（它读 `hashedAnimations`，本应用 `hashedBehaviors` 侧）。

另外：`current=0x0`（装载前后都是空）→ 管理器的 `currentAnimation` 槽在本流程下不使用；
`loaded-records` 7872 → 7871。

**失败被精确归因到"资源内部"**：HKX18 的单层指针扫描够不到它。

## 10. HKX20：给资源判类 + 转储 + 多层有界遍历

- 提交 `9c20596e`，包 `dist/CharacterPanel-scopy-HKX20-1.2.1.zip`，身份
  `CharacterPanel-scopy-HKX20-9c20596ed2-cl94faaed0c6-20261010T165605Z`（`source_baseline_dirty=false`）。
- DLL `88335d6d…`、PDB **均已从最终部署并校验一致**；单测 1/1。

三处改动：

1. **判类**：把资源的虚表与地址库声明的五个 `hkResource` 类逐一比对
   （`hkResourceBase`／`hkResourceHandle`／`hkResourceContainer`／`hkResourceMap`／`hkResource`）——
   一次受保护读、**零虚调用**，与 HKX9 让 clip 判类安全的同一手法；
2. **转储**：首个资源的原始 `0x60` 字节 + 类名；
3. **深层遍历**：把单层指针扫描换成**每个资源最多 24 个对象的有界遍历**
   （地址本身与字指针都当候选，逐个校验后才采信）。记录数降到 8 以负担遍历成本。

**新增日志**：

```
SCOPY ANIM load resource type=hkResource|hkResourceContainer|… qwords='…'
```

**判读表**：

| 观测 | 含义 | 下一步 |
| --- | --- | --- |
| `type=hkResourceContainer` | 容器：动画在命名变体里 | 按容器布局取变体（OAR 的 `hkRootLevelContainer` 有型） |
| `type=hkResourceHandle` | 句柄：真资源在另一处 | 跟句柄指针到目标资源 |
| `type=hkResource` 且转储里出现「堆指针 + 小整数」 | 内容内联或经由该指针 | 按转储定偏移 |
| `anims>0` 且 `data-present=true` | **取数成功** | 接 ground-truth → F2 播放 |

**未变**：`Queue`/`Load` 仍有副作用；S1 安全回收仍阻塞；H03–H05 未测。

## 11. HKX20 实测结果（2026-10-11 00:56）：资源不是那五个类

日志 `CharacterPanel-hkx20-resource-20261011.log`。**零崩溃**。

```
SCOPY ANIM load before records=7872 records-walked=8 with-loader=8 with-resources=8 resources=8 anims=0
SCOPY ANIM load loader qwords='00007ff69ed463f0 000000000001ffff 000001f3f3fcc030 0000000100000001'
SCOPY ANIM load resource type=other qwords='00007ff69ed527c8 000000000001ffff 000001f3b5351f00 00004b1000004b10
                                           000001f3b53520a0 00007ff69ed26d08 000001f3f3c0cb40 0000000100000000
                                           000001f3f3bf1650 0000000200000001 0000000000000000 0000000000000000'
SCOPY ANIM load index=11921 queued=1 loaded=true loaded-records=7871
```

链路依旧 8/8 全通（loader 与非空 `loadedData`），**失败仍在资源内部**。两个意外：

1. **`type=other`** —— 资源的虚表 `0x7ff69ed527c8` 与地址库声明的五个 `hkResource` 类**都不相等**；
   要么这对象不是那五类之一，要么我的对照表本身不对（HKX21 会把五个地址打出来，一次分辨）；
2. **结构比假设复杂** —— 资源里是一个**堆后端、size≈19216 的表**，后面还有更多表和一个
   **第二个虚表形态的指针**（+0x28 = `0x7ff69ed26d08`）。**部分转储已不足以靠读推出来。**

## 12. HKX21：逐对象转储整棵树

- 提交 `eaea7ed6`，包 `dist/CharacterPanel-scopy-HKX21-1.2.1.zip`，身份
  `CharacterPanel-scopy-HKX21-eaea7ed69a-cl94faaed0c6-20261010T170019Z`（`source_baseline_dirty=false`）。
- DLL `17f50a5f…`、PDB **均已从最终 zip 部署并校验一致**；单测 1/1。

**新增日志** —— 遍历到的**每个对象一行**，让结构从日志里读出来而不是再猜一次：

```
SCOPY ANIM load vtables base=0x… handle=0x… container=0x… map=0x… resource=0x…
SCOPY ANIM load tree obj=0x… vtable=0x… animation=false reason=not-object type=unknown tracks=0 qwords='…'
```

`vtables` 行一次分辨"对照表错"还是"对象确实不属于这五类"：
**资源虚表等于其中某个 → 比较有问题；都不等 → 对象确实不是这五类。**

`tree` 行给出每个对象的**自身虚表 + 前 4 个字 + 校验判定与拒因**，
所以"动画到底在不在里面"与"遍历是不是还在找错地方"可以分开判读。上限 16 行。

**判读表**：

| 观测 | 含义 | 下一步 |
| --- | --- | --- |
| `tree` 里出现 `animation=true` | 找到了可校验动画 | 看 `data-present` 是否也过 → 接 ground-truth |
| `tree` 里所有对象都 `reason=not-object` | 遍历到的都不是对象头（偏移错） | 用 `qwords` 重新定偏移 |
| `tree` 里出现大对象但无动画 | 动画在更深层或不在该资源 | 加大遍历深度／换记录 |
| `vtables` 行与资源虚表相等 | **我的判类有问题**，不是对象的问题 | 修对照表即可 |

## 13. HKX21 实测结果（2026-10-11 01:01）：判类不可依赖，且遍历从未进入 arena

日志 `CharacterPanel-hkx21-tree-20261011.log`。**零崩溃**。

```
SCOPY ANIM load vtables base=0x7ff69ed26f68 handle=0x… container=0x… map=0x… resource=0x7ff69ed52780
SCOPY ANIM load tree obj=0x21af4000720 vtable=0x7ff69ed527c8 animation=false reason=animation-type …
SCOPY ANIM load tree obj=0x21af4015800 vtable=0x7ff69eda1910 …
… （共 16 行，后段 obj 地址已是模块静态区 0x7ff69ed05xxx / 0x7ff69ecfxxx）
SCOPY ANIM load verdict=no-new-animation anims-before=0 anims-after=0
```

**两个决定性结论：**

**① 判类不可依赖。** 资源虚表 `0x7ff69ed527c8` 比地址库给的 `hkResource` 虚表 `0x7ff69ed52780`
**正好高 0x48（9 个槽位）**。要么这是另一个类，要么**这份地址库对 AE 1.6.1170 已陈旧**。
两种可能都无法用"比对虚表"区分 → **HKX22 起不再依赖任何类名判定**。

**② 遍历从未进入 arena。** 16 行全部 `reason=animation-type`（说明它们是对象形态、只是不是动画），
且后段 `obj` 地址落在**模块静态区**（`0x7ff69ed05xxx`／`0x7ff69ecfxxx`，即引擎自带的
`hkClass` 之类元数据）—— **指针图走出了堆、走进了静态表，而资源真正的数据一个字节都没碰到。**

**资源结构（由转储手算确认）**：

| 偏移 | 值 | 语义 |
| --- | --- | --- |
| +0x10 | 堆指针 | **arena 基址** |
| +0x18 | `0x4570` = **17776** | **arena 字节数** |
| +0x20 | arena 基址 **+ 0x1A0** | arena 内第二个指针 |

arena 里是**纯字节**，而指针遍历只走"字形状像对象"的字 —— **所以它结构上就到不了 arena 内部**。

## 14. HKX22：按内容扫描，不按图走

- 提交 `f458d268`，包 `dist/CharacterPanel-scopy-HKX22-1.2.1.zip`，身份
  `CharacterPanel-scopy-HKX22-f458d268e9-cl94faaed0c6-20261010T170639Z`（`source_baseline_dirty=false`）。
- DLL `5bb74b2d…`、PDB **均已从最终 zip 部署并校验一致**；单测 1/1。

**做法**：资源头部指向的每个字节区间（任何相邻两个字读作「堆指针 + 合理字节数」即视为一个区间）
都**逐 8 字节对齐扫描**，每个候选地址交给**动画必须通过的那套校验**。
**不假设任何布局**；错一次只损失一个被拒候选，不会得到错误答案。

**新增日志**：

```
SCOPY ANIM load region at=0x… bytes=… from-offset=0x…
SCOPY ANIM load region-hit at=0x… offset=0x… type=spline duration=…s tracks=… data-present=true|false
```

**判读表**：

| 观测 | 含义 | 下一步 |
| --- | --- | --- |
| `region-hit` 且 `data-present=true` | **动画与数据都找到了** | 取映射（同法扫 binding）→ 接 ground-truth → F2 播放 |
| `region-hit` 但 `data-present=false` | 找到动画对象，数据仍未就绪 | 数据在别的区间 → 看 `region` 行覆盖了哪些 |
| 只有 `region` 行、无 `region-hit` | 该区间内没有动画对象 | 换记录（首 8 条未必含目标）或扩区间 |

**成本**：17776 字节 ÷ 8 = 2222 个候选/区间，区间数上限 4 —— 一次性开销，可接受。
