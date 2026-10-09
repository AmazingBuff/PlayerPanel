# HKX8 实测证据：遍历走完但 clips=0，typed states 边从未生效（2026-10-10）

- 构建身份：`CharacterPanel-scopy-HKX8-7656223071-cl94faaed0c6-20261009T170438Z`，与包内 manifest 一致
  （`source_baseline_dirty=false`，基线 `7656223071`）。
- 日志：[CharacterPanel-hkx8-clipsearch2-20261010-0107.log](diagnostics/CharacterPanel-hkx8-clipsearch2-20261010-0107.log)
  （SHA-256 `af2d7a156e8a3b424f876b3046847c4e8c0ff6bf601dba7b4b889f8d0ec01af3`）。**无崩溃**（HKX7 的崩溃已消除），
  单次捕获（CBBE，`matched=90`），绘制正常（`passes=23`、`bound=331`），程序化待机回退照常工作。

## 1. 现象：面包屑走完全程，一个 clip 也没有

```
SCOPY ANIM play progress objects=32 … 448 pending=21 clips=0   （每 32 对象一条，一路到尾）
SCOPY ANIM play search objects=460 clips=0 capped=false first='-'
SCOPY ANIM play classes='hkbStateMachine:1'
SCOPY ANIM play rejections='no-animation-name:426, unreadable:33, other-class:1'
SCOPY ANIM play UNAVAILABLE reason=no-clip-generator
```

对照 HKX6（`objects=96` 恰好等于上限、被截断）与 HKX7（日志停在 catalogue 行、崩在遍历里）：

- **不崩了**：HKX7 的教训成立——不再对来历不明的指针做虚调用后，遍历全程平安。
- **走完了**：`objects=460 < 512` 且 `capped=false`、队列传空收尾——前沿被**耗尽**而不是被截断。
- **但 clips=0**。

## 2. 根因收窄：460 个对象里没有一个真实 clip generator

判类依据是数据形状（在 `hkbClipGenerator::animationName` 的偏移上找 `.hkx` 名字），而这条校验
**不依赖对象是怎么被发现的**——只要真踩到一个 clip generator，名字必然读得出来。460 个对象全部
`no-animation-name`，所以结论不是"踩到了但认错了"，而是**根本没踩到**：遍历从未进入生成器树。

进入生成器树的唯一类型化路径是 `hkbStateMachine::states[]` → `StateInfo`（其字段持有该状态的
generator），而 `classes='hkbStateMachine:1'` + `rejections='other-class:1'` 证明**全程只有根一个
typed 对象被访问**——states 边一次都没有跟出去。形状扫描补不上这条路：`states.data()` 指向的是
指针数组（堆地址，不像模块 vtable，形状判据不收），`StateInfo` 只能经类型化边到达。

states 边没生效的可能原因（CLib 布局链 `hkbBindable=0x30 → hkbNode=0x48 → states@0x90` 自洽，
`sizeof(hkbStateMachine)=0x108`；根节点 `readable(0x110)` 在 HKX8 已成立，故整窗可读性不是问题）：

1. `states.size()` 读到 0 / 垃圾——**CLib 的 `states` 偏移 0x90 与实际引擎不符**；
2. 根状态机 states 数 > `Max_States`(64) 被静默跳过；
3. `states.data()` 不可读 / 条目全空（可能性最低）。

## 3. 附带观察

- 每对象 ~37 ms 的耗时主要来自对每个对象都做 `guarded_string`（逐字节 `readable`）——判类先行后
  只有真候选才需要，这一开销自然消失。
- 本轮 replay A/B：`best=1.6161u / control=125.391u = 1.29%`，仍是 HKX2 捕获 1 的老样子（略高于
  1% 相对门槛、残差纯位置——缩放语义候选项，未变）。
- 形状扫描会扫过小对象的**末尾之外**（`Container_Scan_Bytes=0x200` 大于多数对象），460 个对象里
  有多少是堆邻居住户无从分辨——判类先行后这不再是判据，只是噪音。

## 4. HKX9 的修正

1. **判类改为与引擎自己的虚表地址精确比对**（`VTABLE_hkbStateMachine` / `…__StateInfo` /
   `VTABLE_hkbClipGenerator`，CLib AE ID 226812 / 226706 / 226785）：一次受保护的内存读、零虚调用，
   对任何可读对象都安全——HKX7 的崩溃面被彻底移除，分类也不再依赖"怎么到的这个对象"。
2. **states 边只在虚表确证的状态机上触发**，成员窗口（`&states, 0x10`）可读即可，不再要求整 0x108；
   子项照旧按 typed 信任。
3. **根状态机一次性转储**（`SCOPY ANIM play root-sm`）：名字、`readable-0x108`、`states.size`、
   data 指针与可读性、外加 0x108 字节的 qword 转储——上一节三个可能原因由这一行直接裁决。
4. `search` 行新增 `states=数组数/条目数`：`states=0` = 边仍没生效（看转储）；`states>0` 而
   `clips=0` = 已进树、问题在识别（看 classes/rejections）。
5. 深度 8→12、对象 512→1024（确证路径使每对象变廉价；真实生成器树比旧边界宽）。
