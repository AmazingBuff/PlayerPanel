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
