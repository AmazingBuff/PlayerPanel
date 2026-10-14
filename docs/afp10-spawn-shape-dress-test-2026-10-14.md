# AFP10：出生窗口不可感知、服装镜像、体型同步

日期：2026-10-14。依据：[AFP 汇总 §6](afp-summary-2026-10-13.md) 之后的三条用户报告（2026-10-14）：
①进游戏后前几秒仍能在玩家身边感到克隆体；②面板里只有武器装备上了，服装没有；③克隆的身体形状和原角色不一致。

三个工作包各自独立、各有自己的日志行，可分别判读。

## 1. 工作包 A：出生窗口内不可感知（AFP10-A）

**报告与机制**：装配窗口原本是固定 240 帧，期间傀儡站在玩家身上、fade 被压到 **0.05**——一个 5% 的幽灵，
就在第一人称身体里。窗口之所以存在是引擎约束：**角色 LOD 在引用装载时按距离选定**，
引用必须在玩家身边图才是高模（run 104：出生在泊位深度的引用装配出缺件、蒙皮被剥离的低模图），
所以引用不能提前搬走。

**本轮改动**（`puppet_driver.cpp`）：

1. fade 守卫由 0.05 改为 **0.0**（全透明）。复制图需要非零 fade 是因为它的摘图门在等 `numMatrices`，
   而归零的 BSFadeNode 永远拿不到该值；**本路线没有这个门**——傀儡的绑定是从树里读的，
   设备缓冲由面板自己的绘制创建。
2. 装配窗口由"固定等 240 帧"改为**"装好就走"**：每帧做一次共用普查（`figure_measure_assembly`），
   要求几何非空、skin data 齐全、**每个已分配槽位都已绑定**、且该普查连续 10 帧不变；下限 20 帧确保
   装载时引用确实在玩家身边，上限仍是 240 帧（超时照泊，与旧行为一致，不构成回归）。
3. 判读行：`SCOPY PUPPET dressed …` 与 `SCOPY PUPPET assembly frames=… reason=assembled|assembly-timeout geoms=… bound-nodes=…/… stable=…`。

**游戏内验证**：

| 项 | 期望 |
| --- | --- |
| 读档后前几秒 | 玩家身边**完全看不到**任何人形（不是变淡，是没有） |
| `SCOPY PUPPET assembly` | `reason=assembled`，`frames` 远小于 240（典型几十帧内）；若恒为 `assembly-timeout`，记下该行的 `bound-nodes=/stable=` 回传 |
| 面板 | 反复开关仍完整人物（AFP9 基线不回归） |

## 2. 工作包 B：服装镜像（AFP10-B）

**报告与机制**：`poll_equipment` 只看玩家两只手（`GetEquippedEntryData`），所以面板里只有武器；
盔甲/衣物是 **biped 槽位**，武器不是 `BGSBipedObjectForm`，两条路互不相交。

**本轮改动**：新增共用件 `equipment_mirror.{h,cpp}`——"玩家身上穿了什么"这条规则从此只有一份
（复制图的 `mirror_worn_equipment` 改为调用它，不再自带一份槽位表）。驱动器：

- **穿衣**：装配完成、**引用仍在玩家身边时**把玩家整套 worn body items 穿上（复制图验证过的顺序：
  先穿衣、再让图离开世界）；
- **持续同步**：每次对账都从两边库存现读（无缓存状态），多则补、少则卸；面板暂停时每帧读，
  世界运行时每 30 帧读一次。变化行 `SCOPY PUPPET worn equipped=… removed=… worn=…`。

**游戏内验证**：

| 项 | 期望 |
| --- | --- |
| 开面板 | 人物穿着与玩家一致：衣服、护甲、靴子、首饰都在 |
| 背包内换一件护甲 | ≤1 帧内 `SCOPY PUPPET worn equipped=1`，面板人物随之更新 |
| 脱一件 | `removed=1`，面板上那件消失 |
| 武器 | 与 AFP9 一致（`equipped/right=/left=` 行仍在，draw/sheathe 行为不变） |

## 3. 工作包 C：体型同步（AFP10-C）

**报告与机制**（2026-10-14 两路只读调研的结论）：

- 本机体型**不是**节点缩放：639 个 `.osp` slider set 里没有任何骨骼/缩放定义，滑块全部由 `.osd`
  形状数据（顶点 delta）实现，运行时由 RaceMenu 的 skee64 以 `.tri` morph 方式写进身体网格。
  因此"复制骨骼 scale"这条路在本机等于什么都没复制。
- 傀儡的随机体型来源已实锤：OBody NG 对它**每个会话随机派一个预设**——
  `OBody.log`：`Trying to find and apply preset to CharacterPanel Puppet` →
  `No preset defined for this actor, getting it randomly` → `Preset Mia UBE will be applied …`
  （另一个会话是 `zGranhadd_3BA#009(Jonna)`）。
- 正确入口 = OBody 自己用的那条：skee64 的 **`IBodyMorphInterface`**（SKSE 接口交换，接口名
  `BodyMorph`，本机运行时 `GetVersion()==5`）。

**本轮改动**：新增 `body_shape.{h,cpp}`——本地按上游顺序镜像该接口的虚表（只调用其中四个：`VisitMorphValues`
/ `GetMorph` / `SetMorph` / `ClearBodyMorphKeys` / `ApplyBodyMorphs`），运行时**先查版本**，低于镜像布局就拒绝调用。
`body_shape_sync()` 的语义：

1. 先用 `VisitMorphValues(玩家)` 逐条与傀儡**比较**（`GetMorph`）；完全一致就什么都不做——
   这是它能挂在面板节奏上的原因；
2. 不一致才写：先 `ClearBodyMorphKeys(傀儡, "OBody")` 清掉 OBody 派给它的那个随机预设，
   再把玩家的值逐条 `SetMorph` 上去；
3. `ApplyBodyMorphs(傀儡, deferUpdate=false)` 当场重写顶点缓冲，让变化进入下一次绘制消费的几何。

调用点：穿衣时一次；面板打开期间每 300 个暂停 tick（约 5 s）复核一次——OBody 会在 3D 装载时重新塑形，
玩家自己也可能改体型，所以是"复核后必要时才写"，不是一次性假设。日志行
`SCOPY SHAPE applied reason=… compared=… mismatched=… copied=… sample=[…]`。

**游戏内验证**：

| 项 | 期望 |
| --- | --- |
| OBody 日志 | `skee BodyMorph interface acquired version=5`（获取失败会有 `unavailable` 行） |
| 开面板对比 | 傀儡身材（胸/臀/腿/腰）与玩家一致，不再是随机的另一套 |
| `SCOPY SHAPE applied` | 首次应出现 `mismatched>0`（OBody 已派过随机预设）；之后复核若一致则**不打印**（说明同步稳定） |
| 换一次自己的体型（RaceMenu/OBody 菜单） | 下一次复核 `applied` 再次出现，面板跟上 |
| 回归 | OBody 日志里傀儡仍会被处理，但面板显示以玩家为准；O 本人形状不受影响 |

**红线**：本模块只写傀儡的 morph，从不写玩家的；`ClearBodyMorphKeys` 只清傀儡自己那份。
接口获取失败（无 RaceMenu、版本不符）时**保持原样并打印一次**，不猜布局、不硬编码偏移。

## 4. AFP10b：首测现场的三处修正（2026-10-14 同一会话）

AFP10 上机后用户报告：**clone 身上出现了玩家没有的斧头、盾牌和弓箭**。现场日志（`CharacterPanel.log`）直接给出三处：

1. **`SCOPY SHAPE` 把全部 morph 都搬了**（`compared=193 mismatched=177 copied=193`），sample 里是
   `XPMSEAABase_axeeqp`、`XPMSEAABase_boweqp`、`XPMSEAABase_dageqp`、`XPMSEAABase_1hmeqp`、`XPMSEAABase_2hmeqp`
   ——**XPMSE 自己的"当前装备了哪类武器"状态**。同帧 FP 行的 world 离群节点正是
   `WeaponAxe, WeaponBack, WeaponBow, WeaponDagger, WeaponMace`：傀儡的武器附着节点被告知"这些武器在"。
   **修正**：只拷 **OBody 键**（身体形状就住在那里），其余键只计数（`skipped=`）不写。
2. **手部镜像只装不卸**：`equip right=… pushed=sheathe` 在玩家空手时只推收起动作，
   **从不卸下傀儡手里的旧武器**——本会话画过的斧头/盾/弓于是全部留着。
   **修正**：双向镜像，先卸掉与玩家不同的那一件再装玩家的；日志行补 `equipped=/removed=`。
3. **装配行每帧刷屏**：提前泊位后 `m_grace_frames` 仍小于上限，窗口块下一帧再次进入，
   于是 `assembly` 行与 `park-3d` 每帧重复到 240 帧。**修正**：泊位后一律关闭窗口。

同时记录两条本轮实测事实（不是失败，是判读输入）：

- 装配门**能提前放行**：第二个会话 `dressed equipped=2 frames=20 reason=assembled`、
  `assembly frames=30 reason=assembled bound-nodes=47/47 stable=8` —— 从 240 帧降到 30 帧。
- 但**重载存档后的第一次出生可能等满 240**：第一个会话 `bound-nodes=0/0 · stable=237`，
  即该壳的几何都在（48/44）却一个 skin data 都没就绪，且 240 帧内毫无变化；
  超时兜底按旧行为泊位（面板照常）。这条要留观：若稳定复现，下一轮把门从"帧数上限"改为
  "skin data 就绪 + 时间上限"。

**AFP10b 复测项**：① 反复切武器（斧→盾→弓→空手），面板武器数量**始终**与玩家一致，
`equip` 行出现 `removed=`；② 面板不再出现玩家没有的武器；③ 装配行**只出现一次**。

## 5. AFP10c：装备叠加与"新 Actor 自带装备"（2026-10-14 第二轮报告）

第二轮报告两条：**装备叠加**；**初始阶段**同步仍有问题——傀儡会带上"默认新 Actor 的弓箭、盾牌、斧头"。

根因（同一件事的两半）：

- **新放置的 Actor 不是空的**。它的基础记录自带作者放进去的东西，而傀儡的基础记录是**玩家 NPC 记录的副本**
  （`CreateDuplicateForm`），于是壳一生出来就穿着自己那套（弓/盾/斧），叠在镜像穿的装备之上 = 叠加。
- **装配窗口内开面板就会看到它**：穿衣发生在窗口末尾（`dressed` 行），在第一轮日志里是 `born`
  之后约 10 秒（`reason=assembly-timeout`，等满 240 帧）。这 10 秒里壳是"自带装备 + 未镜像"的状态。

**修正（AFP10c）**：

1. 新增 `equipment_strip()`：把每个 biped 槽位的 worn 项、双手、以及**全部库存条目**脱下并移除；
2. **出生时剥一次**（并把 `inherited=[名字(formID)]` 打进日志，自带的到底是什么由日志点名），
   **穿衣前再剥一次**（壳的 3D 装载会按记录再挂一次默认装备）；
3. 装 worn 项前先 `GetItemCount`，已有就不再 `AddObjectToContainer`（重复添加是叠加的另一半）。

AFP10b 会话已确认修好的部分（同一次日志）：

```
SCOPY SHAPE applied reason=dressing compared=170 mismatched=161 copied=170 skipped=46
SCOPY PUPPET equip right=000be19c left=00000000 pushed=draw    equipped=1 removed=1
SCOPY PUPPET equip right=00000000 left=00000000 pushed=sheathe equipped=0 removed=1
```

即：XPMSE 的 46 条状态 morph 已被排除（`skipped=46`），手部现在**双向**（玩家放下时 `removed=1`）。

**AFP10c 复测项**：① 出生后立刻开面板：壳应是**裸的**或已穿玩家装备，**绝不出现**弓/盾/斧这类自带件；
② `SCOPY PUPPET stripped items=… removed=… inherited=[…]` 行点名了自带件（把它们的形式 ID 记下来，
可定位是哪个 mod 给玩家 NPC 记录加的）；③ 面板上任何一件装备都**只有一份**（不再叠加）；
④ `equip` 行仍然 `equipped=/removed=` 成对。

**仍待观察（已加日志字段）**：`assembly-timeout` 仍在重载后的第一次出生出现，且
`bound-nodes=0/0`、`stable=237`——壳的几何都在（48/44）却一个 skin data 都没就绪且 240 帧不变。
AFP10c 的该行新增 `skin-data-null=` / `bones-null=` 两个字段用于区分"skin data 没装载"与
"bones 数组为空"。若稳定复现，下一轮把门从"帧数上限"改为"skin data 就绪 + 时间上限"。

## 7. AFP10d：面板空白（"F6 按不出面板"）与默认直绘

**报告**：用户要求"直接在背包内显示 clone 面板、不需要额外操作"，并报告"现在 F6 按不出面板了"。

**现场日志**（AFP10c 会话）：

```
SCOPY R01 fallback to the copy reason=not-framed        ← 每一帧
SCOPY PUPPET dressed equipped=6 frames=240 reason=assembly-timeout geoms=44 skinned=44 bound-nodes=0/0 skin-data-null=0 bones-null=0 stable=237
SCOPY COMPOSITE src=… match=1 draws=1                    ← 整局只合成过一次
```

**机制（本轮引入的死锁，非 F6 被别的 mod 占用）**：`SCOPY R01 direct-draw on/off` 在日志里
成对出现，说明 **F6 事件本身是收到的**；问题在取景：

1. 引擎**只在绘制几何时**分配 `NiSkinInstance` 的骨骼指针数组。壳从未被世界渲染器画过
   （fade 归零 + 泊位深度），所以每个 skin 都是 `allocatedSize == 0`
   → `bound-nodes=0/0`，且 `skin-data-null=0 / bones-null=0` 正说明"不是缺数据，是没分配"。
2. 取景用的活数据 `figure_skeleton_bound` 只能从 palette 取点 → 恒 invalid。
3. `begin_frame()` 因此每帧直接返回 → `draw()` 报 `not-framed` → 回退复制图；
   而这一局没有 F7 捕获 → **面板空白**。

即 **"等 palette 才敢画"与"画了才会分配 palette"互相等待**。上一轮 fade=0.05 能工作，正是因为
它让引擎在装配期画过一次——fade 归零顺手掐断了这条供给。

**修正（AFP10d）**：

1. 取景回退：palette 活值不可用时改用**树自身的节点云**（新增 `figure_node_cloud_bound`，
   活数据、排除碰撞辅助体），并打印 `bound-from=palette|nodes`；画过一次后 palette 被分配，
   自动回到活值。
2. **R01 直绘默认开**：开背包即出人，F7 捕获与 F6 都不需要；F6 仍可切回复制图。
3. 补两处取证：穿衣时打印壳的几何名（`SCOPY PUPPET shell names=[…]`），剥离时打印被脱下的
   worn 项名字与形式 ID（`unequipped=[名字(formID)]`）——上一轮的 `unequipped=1` 只给了数量，
   无法点名，这正是"面板上的弓/盾/斧到底是谁的"至今只能猜的原因。

**复测项**：① 开背包**直接**出人（不按任何键）；② `R01 frame … bound-from=` 首次可能是
`nodes`，面板出人后应转为 `palette`；③ `R01 fallback … reason=` 不再逐帧出现；
④ 回传 `shell names=[…]` 与 `unequipped=[…]` 两行——前者点名傀儡树里的网格，后者点名它自带的东西。

## 8. 判读回传

| 项 | 读数 |
| --- | --- |
| A：读档前几秒画面 + `assembly` 行（reason/frames/bound-nodes/stable） | |
| B：`dressed equipped=` 行 + 换装/脱装的 `worn` 行 + 面板截图 | |
| C：`SHAPE applied` 行（compared/mismatched/copied/sample）+ 面板与玩家并排截图 | |
| 回归：F6 直绘、F2 mark、F4 档位、反复开关面板 | |
| 崩溃（如有）：CrashLogger + 本包 PDB | |

出现非有限值、来源（玩家）被写、旧树引用或崩溃时立即停止并回传原件。
