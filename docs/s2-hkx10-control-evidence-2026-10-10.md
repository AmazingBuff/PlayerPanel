# HKX10 实测证据：control 也全空、正在播放的动画条目也是空桩——标准 Havok binding 层在 Skyrim 运行时是死的（2026-10-10）

- 构建身份：`CharacterPanel-scopy-HKX10-d3c847a3b9-cl94faaed0c6-20261009T180211Z`，与包内 manifest 一致
  （`source_baseline_dirty=false`，基线 `d3c847a3b9`）。
- 日志：[CharacterPanel-hkx10-control-20261010.log](diagnostics/CharacterPanel-hkx10-control-20261010.log)
  （SHA-256 `781ef3a1d82b22d3b6d83106941f23fb15d3cc88f1f5d5319bfdd13cbcdfe147`）。**无崩溃**，绘制与
  程序化待机、F2/F3/F8 正常。

## 1. 现象：66 个 clip 的两条 binding 路全部为空

```
SCOPY ANIM play search objects=893 clips=0 states=21/100 capped=false first='-'
SCOPY ANIM play classes='other:655, hkbStateMachine$StateInfo:138, hkbClipGenerator:66, hkbStateMachine:34'
SCOPY ANIM play rejections='binding-null:66'
SCOPY ANIM play clip-seen i=50 idle=yes name='Animations\male\MT_Idle.HKX' binding-index=1022 binding=0x0 control=0x0 speed=1.00 mode=0
SCOPY ANIM play element index=1022 at=0x2417377cc00 readable-0x40=true
SCOPY ANIM play element qwords='00007ff6ef2649b8 000000000001ffff 0 0 8000000000000000 0 00007ff6ef2649b8 000000000001ffff'
SCOPY ANIM play element binding-at-0x10 at=0x0 valid=false reason=unreadable
```

- 拒因拆分生效：66 个 clip 全部 `binding-null`（直接惰性指针为空）；
- **`clip-seen` 明细全中**：名字可读（`MT_Idle.HKX`、`weaponAdjustment.hkx`、配对击杀动作等真名），
  `binding-index` 与 catalogue 名字表对得上（clip 的 `MT_Idle.HKX` ↔ 索引 1022 ↔ 名字表
  `Animations\female\mt_idle.hkx@1022`——引擎路径与名字表拼写不同但索引一致）；
- **连正在播放的 mt_idle 也是 `binding=0x0 control=0x0`**；
- **定点转储实锤**：binding set 的 1022 号条目（mt_idle 自己的索引）就是空桩
  `{vtable 49b8, 引用头, 0, 0, 0x8000…, 0}`，`+0x10` 指针为 0。HKX4 看到的"27c8 变体 + 活指针"
  被确认为堆邻居噪音，不是已加载形态。

## 2. 判读：路线 A（经引擎已加载动画取 hkaAnimation）的证据链收束为"死路"

三轮定点证据合拢：

1. **HKX3-5**：binding set 元素是 0x30 字节小桩，按布局猜 binding 全部 `valid=0`；
2. **HKX9**：66 个 clip 的运行期 `binding`（@0xA0）全空（当时判读为"未激活"）；
3. **HKX10**：连**正在播放**的 clip 也 `binding=0x0` 且 `animationControl`（@0x88）= 0x0，其
   binding set 条目（索引 1022，定点 dump）也是空桩——"未激活所以为空"的解释被排除。

结论：**Skyrim 的运行时不维护可从类型化成员到达的标准 Havok binding/control 层**——动画按索引经
引擎自有的装载/采样路径在 generate 期解析（`BShkbAnimationGraph` 上仅剩的候选是 `unk190/unk1A8/
unk1C0` 三个无类型数组，继续挖就是又一轮盲钻，与 §9 红线相悖）。

## 3. 顺带收获

- `clip-seen` 证明 clip generator 的名字与索引层完全可用：**虚表判类 + 名字/索引读取这一套仪器是
  靠的**，将来任何需要"枚举图里有什么动画"的场景都能直接用；
- 根状态机转储复现 HKX9 结果（`Master_Behavior`，states=11）——判类仪器跨会话稳定。

## 4. 对路线的重新排序

- **路线 A（引擎已加载动画）**：证据链收束为死路，正式关闭。
- **路线 B（用户提供 HKX 经引擎装载）**：装载后仍要过同一条采样链——被 A 的死路连带否定；但
  "让源角色播任意动画"的需求可由 OAR（本机已装）替代实现。
- **路线 D（新提出）：poseLocal 录制-回放**——引擎每帧自己采样出的姿态就是"真实动画数据"，
  HKX2 已证明 `poseLocal` 全类型可读（按 `boneNodes` 次序）；未暂停时环形录制 N 秒，暂停面板内
  用自己的时钟回放驱动副本。真值与 A/B 协议照旧适用（副本对录制同相位逐骨对比）。代价远低于 C。
- **路线 C（自研 HKX 解析器）**：自由度最强（可播源角色没在播的任意动画），代价 packfile +
  spline 解码自研，留作 D 证明驱动链之后的选择。

S2 的当前验收目标是**待机**（源角色本来就在播待机），D 直接覆盖；"展示动作切换"若将来需要播
源角色没在播的动画，再议 C/B。
