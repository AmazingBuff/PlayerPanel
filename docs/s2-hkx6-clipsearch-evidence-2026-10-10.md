# HKX6 实测证据：clip 搜索被自己的窗口判据挡住（2026-10-10）

- 构建身份：`CharacterPanel-scopy-HKX6-733dfd5f17-cl94faaed0c6-20261009T165300Z`，与包内 manifest 一致
  （`source_baseline_dirty=false`，基线 `733dfd5f17`）。
- 日志：[CharacterPanel-hkx6-clipsearch-20261010-0056.log](diagnostics/CharacterPanel-hkx6-clipsearch-20261010-0056.log)
  （SHA-256 `d3dee918e63f7ba93465cf37990c8ff8ffe26470142c5a4bd49fe6f19d4b1526`）。**无崩溃**，两次捕获，绘制正常（`passes=23`）。

## 1. 现象：搜索走满上限，一个 clip 也没找到

```
SCOPY ANIM play search objects=96 clips=0 first='-'
SCOPY ANIM play UNAVAILABLE reason=no-clip-generator
SCOPY IDLE bound 8/8 channels
SCOPY IDLE disabled (F2); driver=procedural-idle; …
```

`objects=96` **恰好等于上限** `Max_Clip_Objects`：搜索是被**截断**的，不是走完了整张图。
回退逻辑本身正确（`driver=procedural-idle`，程序化待机照常工作，面板没受影响）。

## 2. 根因（读代码即得，不需要再跑一轮）

扫描一个对象的指针前，我要求它**整个 0x200 字节窗口都可读**：

```cpp
if (!readable(object, Container_Scan_Bytes))
    continue;                       // ← 0x78 字节的 StateInfo 在这里被整段跳过
```

而 clip generator 正是挂在 `hkbStateMachine::states[i]` → `StateInfo`（0x78 字节）里的：
状态机本身能进（100+ 字节可读），**但它的每个 state 都被这一行丢掉**，于是遍历只能去啃状态机里
其它指针指向的杂项对象，96 个上限很快被这类对象吃满。

## 3. HKX7 的三处修正

1. **按可读范围扫描**，不再要求整窗：逐 8 字节前进，遇到不可读就停（上限仍是 0x200）。
   这一条直接让 0x78 字节的 StateInfo 可以进队。
2. **用引擎自己的类名判类**：`hkReferencedObject::GetClassType()->name`（只在首字像 vtable 的对象上调用），
   类名等于 `hkbClipGenerator` 才算候选；类名拿不到时才退回"名字像 `.hkx`"的启发式。
   这样 `animationName` 是否会因为实例化而变空，不再决定能不能找到 clip。
3. **上限放宽 + 直方图**：深度 4→8、对象 96→512；并新增两行诊断
   `play classes='…'`（走过的对象类名，按频次）与 `play rejections='…'`（候选被拒的原因，按频次），
   `search` 行也带上 `capped=` —— 下一轮若还不中，这两行会直接说明"遍历走丢了"还是"clip 根本不在图里"。
