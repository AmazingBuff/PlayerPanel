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

## 15. HKX22 实测结果（2026-10-11 01:25）：**找到 4 个真实动画**

日志 `CharacterPanel-hkx22-region-20261011.log`。**零崩溃**。

```
SCOPY ANIM load region at=0x… bytes=44944 from-offset=0x10
SCOPY ANIM load region-hit at=0x… offset=0x3ee0 type=spline duration=2.50s  tracks=99 data-present=false
SCOPY ANIM load region at=0x… bytes=7376  from-offset=0x10
SCOPY ANIM load region-hit at=0x… offset=0x270  type=spline duration=2.50s  tracks=97 data-present=false
SCOPY ANIM load region at=0x… bytes=23552 from-offset=0x10
SCOPY ANIM load region-hit at=0x… offset=0x270  type=spline duration=3.00s  tracks=99 data-present=false
SCOPY ANIM load region at=0x… bytes=60656 from-offset=0x10
SCOPY ANIM load region-hit at=0x… offset=0x270  type=spline duration=13.50s tracks=99 data-present=false
SCOPY ANIM load verdict=no-new-animation anims-before=4 anims-after=4
```

**这是本轮之前从未达到的位置**：`anims=4` —— 装载存储里**确实存在活的动画对象**，
且**不依赖任何布局假设**就能找到（内容扫描）。

| 观测 | 判读 |
| --- | --- |
| `region` 行 `from-offset=0x10` | **我推断的「+0x10 指针 + 0x18 字节数」结构在 4 个资源上全部成立** |
| 时长 2.50/2.50/3.00/13.50s、轨数 99/97/99/99 | 都是合理值 → **是真动画，不是巧合命中的字节** |
| `anims-before=4 = anims-after=4` | 这 4 个**本来就在**，与本次 Load 无关（走了 8 条记录，全表 7872 条） |
| 全部 `data-present=false` | **唯一未解**：对象是真的，数据仍未就绪 |

## 16. 社区搜索（按用户指示）

### 16.1 最佳社区参考就在本地：OAR 的完整配方（生产验证）

OAR **从不遍历装载器的 arena**。[FakeClipGenerator.cpp:97-126](../OpenAnimationReplacer/src/FakeClipGenerator.cpp#L97)：

```cpp
animationFileManagerSingleton->Queue(a_context, fakeClipGeneratorPtr, nullptr);
…
const auto animationBindingSet = graph->characterInstance.animationBindingSet.get()
                              ?: setup->animationBindingSet.get();
const auto animationBindingWithTriggers = animationBindingSet->bindings[animationBindingIndex];
binding = animationBindingWithTriggers->binding.get();      // ← 引擎 Load 之后，这里就有了
```

→ **我们漏掉的一步**：调了 `Queue`/`Load`（`loaded=true`）却**从未读回该索引的绑定集条目**；
此前读过的绑定集条目都属于*正在播放*的 clip，不是我们刚装载的那个。

### 16.2 自研解析：社区已有成熟先例，但无 C++ 可直接嵌入

| 项目 | 语言 | 用途 |
| --- | --- | --- |
| [HKX2-Enhanced-Library](https://github.com/Monitor221hz/HKX2-Enhanced-Library) | C# | patch／import／export Skyrim SE/AE 的 hkx |
| [soulstruct-havok](https://github.com/Grimrukh/soulstruct-havok) | Python | Havok packfile 类与工具 |
| [serde-hkx](https://github.com/SARDONYX-sard/serde-hkx) | Rust | HKX 反序列化 |
| [PyNifly hkx 格式文档](https://raw.githubusercontent.com/BadDogSkyrim/PyNifly/V25.14/docs/hkx_skeleton_format.md) | 文档 | 骨架／格式说明 |

→ 自研解析**在社区是已解决问题**，可作为格式参考；但没有可直接嵌入本插件的 C++ 库。

## 17. HKX23：走 OAR 验证过的路径 —— 读回刚装载索引的绑定

- 提交 `9e907551`，包 `dist/CharacterPanel-scopy-HKX23-1.2.1.zip`，身份
  `CharacterPanel-scopy-HKX23-9e90755173-cl94faaed0c6-20261010T173332Z`（`source_baseline_dirty=false`）。
- DLL `154631c7…`、PDB **均已从最终 zip 部署并校验一致**；单测 1/1。

**新增日志**：

```
SCOPY ANIM load bindingset idx=… set=character|setup|none element=0x… binding=0x… anim=0x…
        anim-type=spline duration=…s tracks=… binding-valid=… reason=… data-present=true|false
```

**并且**：`data-present=true` 且 binding 校验通过时，**binding 与 animation 一起交给播放阶段**；
`prepare_animation_clip` 里**引擎装载的 binding 优先**（其 `transformTrackToBoneIndices` 是权威映射，
正是 OAR 读的那一份），走 walked-clip 路径作为后备。

**判读表**：

| 观测 | 含义 | 下一步 |
| --- | --- | --- |
| `binding-valid=true` + `data-present=true` | **动画与映射都到手** | ground-truth 相位扫描 → F2 播放 |
| `binding=0x0` | 该索引的绑定集条目仍空 | 装载未落到绑定集 → 改在未暂停帧装载 |
| `binding≠0` 但 `binding-valid=false` | 绑定在、轨道表仍空 | 看 `reason`；必要时用 `animation` + 实测映射 |
| `data-present=false` | 与 HKX22 同 | 数据仍在更深处 |

## 18. HKX23 实测结果（2026-10-11 01:34）：**引擎确实填了绑定集；但我的判据在说谎**

日志 `CharacterPanel-hkx23-bindingset-20261011.log`。**零崩溃**。

```
SCOPY ANIM load bindingset idx=12219 set=character element=0x22b7318dda0
        binding=0x22b43d62bc0 anim=0x22b43d29f00 anim-type=spline duration=20.00s tracks=99
        binding-valid=false reason=tracks-unreadable data-present=false
```

**① OAR 的路径被证实。** `Queue`/`Load` 之后，**该索引的绑定集条目被引擎填上了**：
`binding` 非空、`animation` 头部有效（spline／20.00s／99 轨）。
**读回绑定集条目是正确的地方** —— HKX20–22 往装载器 arena 里挖的方向本就是弯路。

**② 判据有一个说谎点**：全日志唯一的 `true` 是

```
region-hit type=wavelet duration=0.93s tracks=107 data-present=true
```

而我的判据对**非 spline 类型直接返回 true**（只实现了 spline 布局的检查）——
**这个 true 代表"没检查"，不是"数据在"**。真正有信号的是 spline，**全部 spline 都是 false**。

**③ 两个 `hkArray` 同时读空**：binding 的 `transformTrackToBoneIndices`（`tracks-unreadable`）
与 animation 的 `data`（`absent`）。HKX14 的原始转储已确认前者是 `data=0, size=0` —— **真空，不是读错**。

### 一个待验证的假设（记录在案，未采信）

Havok 的 `ConvertTrackPoseToBonePose` 在 `transformTrackToBoneIndices` 为 **NULL** 时
**按恒等映射处理（轨道 i → 骨骼 i）**。Skyrim 的动画与骨架同源，**可能正因此故意不填这张表**——
这能解释引擎为何从不填它。旁证：OAR 只在**带前缀的同步动画**里才用
`hkaSkeleton_CreateSynchronizedClipBinding` 新建填好的表，其余情况直接用原表（**可能是空表**）。

**但这仍是假设**：PRD §6.4 明文禁止未验证就用"轨道 i = 骨骼 i"。
唯一可接受的验证 = 用 `poseLocal` 做真值 A/B（HKX2 已验证该参考可读）。
**前置条件仍是数据可采样**，故本轮先解决 `absent` 的成因。

## 19. HKX24：说清 `absent` 属哪一种成因

- 提交 `23070b6e`，包 `dist/CharacterPanel-scopy-HKX24-1.2.1.zip`，身份
  `CharacterPanel-scopy-HKX24-23070b6ec7-cl94faaed0c6-20261010T173933Z`（`source_baseline_dirty=false`）。
- DLL `fce0638a…`、PDB **均已从最终 zip 部署并校验一致**；单测 1/1。

**`data-present` 改为四态命名**，只有 spline 给真实答案：

| 取值 | 含义 |
| --- | --- |
| `present` | spline 且五个数组全部可读 |
| `absent` | spline 但检查未通过（成因见下表） |
| **`unchecked-non-spline`** | **本检查不认识该类型 → 不给答案**（原先谎报 true） |
| `no-object` | 不是对象 |

**spline 逐字段转储**（`absent` 时打印）：

```
SCOPY ANIM load spline frames=… blocks=… max-per-block=… mask-size=… block-dur=… frame-dur=… track-count=…
SCOPY ANIM load spline array=block-offsets|float-block-offsets|transform-offsets|float-offsets|data  size=… at=0x… readable=…
SCOPY ANIM load spline qwords='…'   （对象原始 0xB0 字节）
```

**两种成因的修法完全不同，必须分开：**

| 观测 | 成因 | 修法 |
| --- | --- | --- |
| 数组 `size=0`、`at=0x0` | **从未填充**（惰性装载） | 改在未暂停帧装载；或该动画确实无数据 |
| `size` 合理但 `at` 不可读 | 指针未 patch | 说明这是**序列化镜像**而非活对象 |
| 数组值像**偏移量**（远小于指针） | 同上，packfile 未 fixup | 需按 packfile 规则自行 fixup |
| `frames/blocks` 为 0 或不可能值 | **布局与本版本不符** | 用 `qwords` 定真偏移 |

## 20. HKX24 实测结果（2026-10-11 01:43）：**布局对、数据在，只有一张表真空**

日志 `CharacterPanel-hkx24-spline-20261011.log`。**零崩溃**。

```
spline frames=150 blocks=1 max-per-block=256 mask-size=396 block-dur=4.279 frame-dur=0.0168 track-count=99
spline array=block-offsets        size=1     at=0x…83e0 readable=true
spline array=float-block-offsets  size=1     at=0x…83f0 readable=true
spline array=transform-offsets    size=0     at=0x0
spline array=float-offsets        size=0     at=0x0
spline array=data                 size=20320 at=0x…8400 readable=true
spline qwords='00007ff69ed4c008 0000000000000000 4020000000000005 0000000000000063 …'
```

**原始 22 个字（0xB0）逐字段解码，与 CLib 的 `hkaSplineCompressedAnimation` 完全吻合：**

| 偏移 | 值 | 语义 |
| --- | --- | --- |
| +0x00 | `00007ff69ed4c008` | vtable |
| +0x10 | `40200000`\|`00000005` | `duration=2.5`、`type=5`(spline) |
| +0x18 | `00000063` | `numberOfTransformTracks=99` |
| +0x38 | `0x96`\|`0x1` | `numFrames=150`、`numBlocks=1` |
| +0x40 | `0x100`\|`0x18c` | `maxFramesPerBlock=256`、`maskAndQuantizationSize=396` |
| +0x48 | `0x4088e9aa`\|`0x3e6f55bc` | `blockDuration=4.279`、`blockInverseDuration` |
| +0x58 | 堆指针 + size=1 | `blockOffsets` ✓ |
| +0x68 | 堆指针 + size=1 | `floatBlockOffsets` ✓ |
| **+0x78** | **`0` + size=0** | **`transformOffsets` 真空** |
| **+0x88** | **`0` + size=0** | **`floatOffsets` 真空** |
| +0x98 | 堆指针 + size=**20320** | **`data` 在且可读** ✓ |

**结论：不是布局错、不是数据缺 —— 是这张表本来就没填。**

### 这一条解释了此前两个看似无关的现象

1. **HKX16 的崩溃**：`mov r9d,[rax+rcx*4]` = 按轨道号索引 `transformOffsets`
   → **索引一张 size=0 的表**。崩溃不是"数据没装载"，是**入口选错了**。
2. **所有 spline 都报 `absent`**：我的判据**要求 `transformOffsets.size() >= 轨道数`** ——
   而这个引擎**故意不填它**。**判据在要求采样器根本不需要的东西。**

### 关键对照：OAR 用的不是这个入口

[ActiveAnimationPreview.cpp:38](../OpenAnimationReplacer/src/ActiveAnimationPreview.cpp#L38)：

```cpp
previewAnimation->SamplePartialTracks(localTime, numTracks, transforms, numFloat, floats, nullptr);
```

**槽 04，不是槽 06。** 差别是本质的：

| 入口 | 槽 | 轨道选择 | 需要 `transformOffsets`？ |
| --- | --- | --- | --- |
| `SampleIndividualTransformTracks` | 06 | **任意**轨道号 | **需要**（按号索引）→ 空表必崩 |
| **`SamplePartialTracks`** | **04** | **从 0 起连续 N 轨** | **不需要** |

**这就是引擎为什么不填那张表** —— 它走的是 04。

## 21. HKX25：改用 OAR 的入口（槽 04）

- 提交 `b580bfba`，包 `dist/CharacterPanel-scopy-HKX25-1.2.1.zip`，身份
  `CharacterPanel-scopy-HKX25-b580bfba19-cl94faaed0c6-20261010T174644Z`（`source_baseline_dirty=false`）。
- DLL `c594e7bb…`、PDB **均已从最终 zip 部署并校验一致**；单测 1/1。

三处采样点全部改走 `SamplePartialTracks(t, count, out, 0, nullptr, nullptr)`；
判据改为**只要求该调用真正触碰的东西**（`numFrames`/`numBlocks` 合理、`blockOffsets` 可读、`data` 可读）。

**同时修掉一个正确性缺陷**：槽 04 取的是**从 0 起的连续段**，而收集到的轨道列表**可能因骨未解析而有空洞**
—— 所以**按整段采样、按轨道号取值**，而不是按位置索引（后者会让空洞之后的每一对骨全部错位）。

**判读表**：

| 观测 | 含义 | 下一步 |
| --- | --- | --- |
| `ground-truth verdict=match` | **采样链闭环** | F2 播放 |
| 有 `best-t` 但 `verdict=none` | 采样成功、映射不成立 | 恒等映射假设不成立 → 需真映射 |
| 零崩溃且无 `mapping UNAVAILABLE` | 槽 04 不再碰空表 | 同上 |
| 仍崩 | 槽 04 也依赖该表 | 回退并改在未暂停帧观察 |

**未验证的假设（仍不采信）**：轨道表为空 ⇒ Havok 按**恒等映射**处理。本轮**没有假设它**——
代码按轨道号取值，**是否正确由 `ground-truth` 相位扫描裁决**（PRD 要求的正是这种 A/B）。
