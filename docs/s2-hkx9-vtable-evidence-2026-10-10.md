# HKX9 实测证据：虚表判类进树成功，66 个 clip 全部卡在运行期 binding 为空（2026-10-10）

- 构建身份：`CharacterPanel-scopy-HKX9-746ffacf13-cl94faaed0c6-20261009T174357Z`，与包内 manifest 一致
  （`source_baseline_dirty=false`，基线 `746ffacf13`）。
- 日志：[CharacterPanel-hkx9-vtable-20261010.log](diagnostics/CharacterPanel-hkx9-vtable-20261010.log)
  （SHA-256 `4d627107ccf13cd8b7c3026c1bffcf27a5b10a71d91aedcc5486ecbc128e316b`）。**无崩溃**，单次捕获
  （CBBE，`matched=90`），绘制与程序化待机、F2/F3/F8 全部正常。

## 1. 根状态机转储：CLib 布局实锤正确

```
SCOPY ANIM play root-sm name='Master_Behavior' readable-0x108=true states.size=11 data=0x29a34062d90 data-readable=true
SCOPY ANIM play root-sm qwords='… 0000029a34062d90 8000000b0000000b 0000029a34062df0 …'
```

- 根状态机真名 `Master_Behavior`（`hkbNode::name`@0x38 读出真名 → 布局链上半段实锤）；
- 转储 word18/19 = `{0x29a34062d90, 0x0000000b, 0x8000000b}` = 教科书 hkArray
  `{_data, _size=11, _capacityAndFlags=11|DontDeallocFlag}` → **CLib 的 `hkbStateMachine::states`@0x90
  完全正确**，HKX8 的"偏移/超上限/不可读"三选一裁决为：偏移没错。
- **HKX8 为什么没进树（复盘）**：旧门槛要求整个 0x108 字节窗口可读——这是会话级脆弱判据（对象落在
  堆页尾部就整段被丢，与 HKX6 的 StateInfo 同机制）；HKX9 改成员窗口（`&states, 0x10`）后当场生效。

## 2. 树进去了，clip generator 被虚表精确识别

```
SCOPY ANIM play search objects=945 clips=0 states=21/100 capped=false first='-'
SCOPY ANIM play classes='other:705, hkbStateMachine$StateInfo:140, hkbClipGenerator:66, hkbStateMachine:34'
SCOPY ANIM play rejections='unreadable:66'
```

- states 边 21 个数组跟进、100 个 StateInfo 入队；945 对象、`capped=false`、队列传空收尾。
- **66 个 `hkbClipGenerator`**（另有 34 状态机、140 StateInfo）被虚表地址精确比对识别，零虚调用零崩溃
  ——HKX7 的崩溃面被彻底移除，HKX8 的"识别不了"被解决。
- 关键推理：拒因 `unreadable` 只可能来自 `read_binding` 的 binding 指针检查——即 66 个 clip **全部
  通过了名字校验**（有真 `.hkx` 名字，顺带实锤 `animationName`@0x48 偏移正确），全部死在
  `hkbClipGenerator::binding`（@0xA0）为空/不可读。

## 3. 机制判读：binding 是运行期惰性链接

`hkbClipGenerator` 的静态链接是 `animationBindingIndex`（@0x70，指向 15203 条 binding set）；运行期
`binding` 指针只在 clip 实例化/激活时填充。暂停菜单里绝大多数 clip 未激活 → 指针为空。这同时回头
修正 HKX3-5 的判读：binding set 元素大多读不到 binding，**不是布局错，是没被填充**（空桩）——
HKX4 看到的"27c8 变体 + 活指针@0x10"很可能就是已加载条目的形态。

## 4. HKX10 的修正

1. **66 个 clip 逐个记录并打印明细**（`clip-seen`：名字、binding-index、binding 指针、control 指针、
   速度、模式；base-idle 名字的必打）；
2. **control 路线**：`animationControl`（@0x88）非空即探测——CLib 的 `hkaAnimationControl` 全类型
   （`binding`@0x38 / `localTime`@0x10 / `weight`@0x14），激活中的 clip 的 control->binding 应当是活的；
   校验通过即直接采纳（`read_clip_generator` 接受"直接 binding 或 control->binding"两条路）；
3. **binding set 定点转储**：用 clip 自带的 `animationBindingIndex` 瞄准一个元素 dump 0x40 字节并
   按同样的校验判 `+0x10` 处的指针——已知索引的定点核对，取代 HKX3-5 的盲扫；
4. 拒因拆分：`binding-null`（指针为空，暂停态的正常形态）与具体 `read_binding` 拒因不再混在一个计数里。
