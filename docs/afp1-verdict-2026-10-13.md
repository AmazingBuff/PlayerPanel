# AFP1 判读（2026-10-13）：最终输出读得到；**但泊深傀儡根本不产出姿态**

- 包：`CharacterPanel-scopy-AFP1-6764d4e1d5+dirty-cl94faaed0c6-20261012T134954Z`，DLL SHA-256 `07bb8ff4…`（上机中的那一版）。
- 日志：`docs/diagnostics/CharacterPanel-afp1-final-output-20261012.log`（21:54:26 启动，228 行；6 次标记、零崩溃、零 `[E]`，
  只有两条已知的灯光账本 warn）。
- 上机动作（按日志顺序）：F2 ×6（mark 1–6），其中 mark 1–2 为世界期、mark 3–4 暂停且推图开、mark 5–6 暂停且 F3 冻结推图。

## 1. 读得到：pose 轨的完整身份（玩家，6 次标记恒定）

```
slot=0 tracks=21 on-tracks=[0,1,2,3,4,5,8,10,11] exists=1 valid=1 type=1 flags=0
       num-data=116 cap=116 elem=48 off=448 on=1.000 data=0x… readable=1 reason=ok
slot=1 tracks=21 on-tracks=[-]                   exists=1 valid=0 type=1 flags=0
       num-data=0   cap=116 elem=48 off=448 on=0.000 data=0x0  readable=0 reason=track-off
```

- **校验链在真机上成立**：`reason=ok` 全绿——输出对象 → tracks 主头（21 轨）→ 轨道头 → 元素尺寸 **0x30** → 数据跨度。
  `num-data = cap = 116` = 骨架骨数，数据在 `tracks + 448`。
- **活缓冲靠内容识别，不能靠下标**：每一标记里 `valid=1/num-data>0` 的那一个都在 slot 0、另一个 `track-off`，
  但**两个缓冲对象的地址每次标记都在变**（0x…380/0x…3b0 → 0x…ae0/0x…b10），活缓冲的数据指针也随之变。
  所以读取规则 = "取 `onFraction>0` 且 `numData>0` 的那一个"，AFP2 不得硬编码 slot。
- `on-tracks` 还顺带确认了其它标准轨：world-from-model(0)、extracted motion(1)、
  rigid-body ragdoll controls(4)/blend time(5)、keyframed ragdoll bones(8)、blend fraction(10)、attributes(11)；
  玩家比傀儡多一条 float slots(3)。**根运动所需的轨都在。**

## 2. 索引语义定案：**动画骨架次序**（不是 boneNodes 次序）

玩家，6 次标记数值不变：

| 候选 | compared | within-1deg | ratio | max-rot | max-pos |
| --- | --- | --- | --- | --- | --- |
| `order=skeleton slot=0` | 109（`named=109`） | **105** | **0.96** | 30.6° | 5.591 |
| `order=bone-nodes slot=0` | 106 | 45 | 0.42 | 176.0° | 75.693 |

→ **写骨架时按动画骨架次序、用精确骨名桥接到节点**（`named=109` 与 HKX1 的 109/116 同源）。
→ 新引擎事实：**`generatorOutputs` 的 pose 轨是骨架序，而 `hkbCharacter::poseLocal` 是 `boneNodes` 序**（HKX2 实测）。
  两个数组、两种次序，都不是"猜"出来的。
→ 残余：4/109 根骨超出 1°（最坏 30.6°），位移最坏 5.591（与 HKX2 当初对骨架序测到的 5.591 同值）。AFP2 应用前
  必须点名这 4 根是谁（大概率根/CME 帮手骨，或带缩放语义的骨），否则按 R04 的要求算"必要骨未覆盖"。

## 3. **阻断项**：泊深傀儡的输出是空的

| 观察 | 傀儡 | 玩家（对照） |
| --- | --- | --- |
| `num-anim-bones` | **0** | 116 |
| pose 轨 | `valid=1 on=1.000` 但 **`num-data=0`** → `reason=pose-empty` | `num-data=116 reason=ok` |
| 节点 local 变化（跨标记） | `changed=2 max-rot=0.040°`（含 2103 次手动推图） | `changed=3…23 max-rot=10.0°` |
| `bound`（骨节点非空） | 106 | 106 |
| `pose-local` | 116 | 116 |

- 傀儡的图**确实生成了输出**（21 轨、8 条 on），但 **pose 轨里一个元素都没有**；`numAnimBones=0` 是同期状态差。
- 这解释并收紧了 HKX42 的"暂停期 node-local 冻结"：不只是"手动推图不把姿态传播到节点"——
  **泊深的傀儡连最终输出都没有**。
- 机制候选（待测，不再靠推断）：泊位把它移出了引擎的动画作用域（culled → 图被停用 → `numAnimBones=0` → 无 generate）。
  v6.25 的泊位是为"世界里看不见/摸不到"设计的，代价现在才被量出来。
- **因此本轮新增杠杆**：`F4` 在 `parked-deep`（现状）与 `held-pinned`（站在玩家处、fade 钉在 0.05、
  引擎动画作用域内）之间切换；标记行新增 `placement=`。下一会话只需按几次 F2，就能判定"空输出"是不是泊位造成的。

## 4. 附带观测（记录，不结论）

- 暂停期间**玩家自己的输出姿态与节点 local 仍在缓慢变化**：mark 3→4 `changed=30 max-rot=0.069°`、
  4→5 `changed=55 max-rot=10.022°`、5→6 `changed=29 max-rot=0.069°`，节点侧同期 `changed=20`/`3`。
  HKX36 的 H05 普查曾判 O 的姿态"逐位冻结"，两者口径不同（那次量的是 `poseLocal` 与固定阈值，
  且以面板操作窗口为单位）。**在把任何一侧称为漂移之前，需要一次专门的对照读数**（同窗口、同数组、同阈值）。
- 冻结（F3 off）后 `manual-ticks` 停在 2103 ✓，语义按设计工作。
- 判读表第 9 条（零崩溃零回归）通过。

## 5. 下一轮（AFP1b）最小动作

1. 部署最新包（`215d3832b3…T140929Z`，DLL `36c97f8a…`）：在首轮的三行之外再加 `geoms` 行（R01 前置：
   Actor 自己的几何有没有 shader property、有没有设备侧 `rendererData`——runs 49-52 正是死在后者为 0），
   `order=` 行补 `beyond`／`max-scale`／`outliers=`（点名超差骨，上限 8），并新增 **`world` 行**（R03 的第二个空间：
   把姿态沿动画骨架自己的父表合成后与节点 world 比对——两级层级不一致才会在这一行露出来）。
2. 会话内：世界期 F2 → F4（held）→ 等装配窗 → F2 ×2；再F4（parked）→ F2 ×2。
3. 看四件事：held 下 `num-anim-bones` 与 `num-data` 是否变成 116；`verdict` 是否 `match=1`；
   `order=` 的 `outliers=` 点名的是哪几根骨、`world` 行是否仍然过；`geoms` 行的 `renderer-data` 是否非零；
   以及 `camera` 行的 `accumulator-is-ui3d`（显示侧取景判据）。
4. held 模式下傀儡站在玩家处（fade 0.05），**只作诊断**，测完切回 parked；这也是 R07 要记录的"世界副作用"样本。
