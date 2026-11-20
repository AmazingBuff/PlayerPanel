# v6-A 离线审计：装备节点父链与 StateInfo（2026-11-20）

**裁定：主图父链的读取布局已核实，可以设计 v6 只读探针。** 先从 v114 实见的 `DG_1hmEquip` 反查主图父边，再用 `StateInfo` 和 ManualSelector 核对分支。归档覆盖第 2–13 步的完整活动节点清单，其中普通 `hkbBlenderGenerator` 实例计数为 0。具体父链、图别名和混合量仍需采集。

## 输入

- 已核哈希的 AE 1.6.1170 解包镜像 `843823C115DBC56B5E9740350DB49170CC4EEA013A79513B7FD066DEEC719A2B`，地址库 `C4093C569A3C83B26587F4B9EA4C55DE9AE6E73B84A2AF9FB3FBD30E2FE0D452`。CommonLibSSE 固定为 `94faaed0c`。下文地址均为 RVA。 [既有镜像与地址库登记](../../g5-transition-audit-20261118/README.md)
- v114 [原始日志](../run1-candidate/CharacterPanel.log)（SHA-256 `082FFBAC7E082FEACACF4E6F34F744F5CFCD50994936A0FA8CD4771737490FC7`）、[候选 CPK5](../run1-candidate/CharacterPanel-g5k5-candidate.bin)、[正式 v103 CPK5](../../g5-gate5-20261116/README.md) 和 [HKX 子树](README.md)。三个 HKX 输入摘录按 LF 规范化后与登记的字节数及 SHA-256 一致。

<a id="active-edges"></a>
## 一、主图父子边

| 字段 | AE 1.6.1170 实证 |
| --- | --- |
| 图 `+0x98`，`activeNodes` | `0xAB17E3` 写入数组头指针。`0xB0FD3A` / `0xB0FD6D` 同时取活动节点表和子索引表，交给构图函数 `59319`。节点记录为 **0x90 B**，`nodeTemplate +0x50`、`nodeClone +0x58`、所属行为图 `+0x60`。 |
| 图 `+0xA8`，`activeNodesChildrenIndices` | `0xAB1843–0xAB1855` 初始化后挂到图上。表头 data/size/capacityAndFlags 为 `+0x00/+0x08/+0x0C`，元素为 **int32**。`0xAD7307–0xAD7336` 扩容使用 4 B 步长并写入索引。 |
| 节点 `+0x70/+0x74`，子项数量与首项下标 | 构图函数 `59327` 在 `0xAD71F1` 写数量，`0xAD72D5` 以当时的子索引表 size 写首项下标。消费端在 `0xAD56D6–0xAD5719` 以数量为循环边界，从首项起每次取 4 B，作为 `activeNodes` 下标。 |
| 空槽、共享子项与输出转发 | `0xAD70C2` 写入空槽 `-1`。构图可复用已有子节点下标（`0xAD70D9–0xAD713D`），并在 `0xAD714C` 累加子节点的传入引用数。另有 `0xAD9373–0xAD93B9` 取第一个 child 并将解析出的输出下标写入 `nodeInfo+0x68`、置 `+0x89`。两字段用于该路径上的输出转发，父边应从子索引表反查。 |

在同一主图、同一采样点读取两张表。逐条验证数组头、容量、`first + count` 和每个非负子下标，保留 `-1` 空槽，区分无子项与无效范围。以 clip 的运行实例和资源身份为锚，反查 **所有** 父边，记录同一路径中的模板、运行实例、所属图和原始边序号。共享子节点可有多个父边。输出转发另列，避免把转发目标当成直接父边。
[CommonLibSSE 行为图](../../../../extern/CommonLibSSE/include/RE/H/hkbBehaviorGraph.h) · [节点记录](../../../../extern/CommonLibSSE/include/RE/H/hkbNodeInfo.h)

<a id="branch-layout"></a>
## 二、状态机与选择器

| 对象 | 已核布局与用途 |
| --- | --- |
| `hkbStateMachine` | `currentStateID +0x80` 为 int32。`states` 的 data/size/capacityAndFlags 在 `+0x90/+0x98/+0x9C`，数组内容为 8 B 的 `StateInfo*`，每个对象为 `0x78` B。`0xAD776A–0xAD77AC` 初始化数组，`0xAD87B5–0xAD87CC` 以指针步长遍历。 |
| `StateInfo` | `generator +0x58`、`name +0x60`、`stateId +0x68`（int32），反射记录分别位于 `0x19506A0/0x19506C8/0x19506F0`。`0xADA660` 对比 ID，`0xADE8C2–0xADE8DB` 用 `uint16(+0x102) >> 1` 取当前数组下标并读 generator。有活动过渡时，同一引擎入口改取最后一条过渡效果（`0xADE8A0–0xADE8C1`）。应同时记录逻辑 ID、下标和实际父子边。 |
| `StateInfo` 与运行实例 | 机器复制路径 `0xAD79F1–0xAD7A34` 在无绑定集、无已解析的 listener clone 时复用 `StateInfo*`。另一分支 `0xAD7A87–0xAD7ADE` 分配新 `0x78` 对象，`59352` 在 `0xAD9526–0xAD9540` 引用并复制原 generator 指针。即便 StateInfo 本身已复制，仍需用活动记录的 `nodeTemplate`/`nodeClone` 与所属图验证 generator 身份。 |
| `hkbManualSelectorGenerator` | `generators +0x48` 为 8 B 指针数组，size 在 `+0x50`。`selectedGeneratorIndex +0x58` 和 `currentGeneratorIndex +0x59` 均为有符号 8 位。反射 `0x194F620/0x194F648/0x194F670` 与 `0xAE9189–0xAE9199`、`0xAE9288–0xAE9294` 一致：按 **current** 下标取实际选中项。 |

`0xADE8C2` 的读法适用于核对状态描述，活动输出还可能经过过渡效果或转发。下一轮优先使用同代活动节点表中的模板/clone 对应关系，`+0xE0/+0xE8` 映射留给活动表确实无法核对的场景。

<a id="archive"></a>
## 三、v114 归档复核

| 装备步 | census 条数 | 有关读数 |
| --- | ---: | --- |
| 1 | 113/113 | `DG_1hmEquip`（id 31506，index 0）与 `MT_Idle` 共存。普通 Blender 两个，`BodyWeightBlend` 和 `ArmsBlend`。 |
| 2–12 | 每步 96/96 | 每步都为同一个 `DG_1hmEquip` 运行实例，普通 Blender 为 0。CPK5 只含 `1hm_equip`，binding 22，FNV `12A77CD14ECC3688`，clip 时间逐位固定在 `0.2083333432674408`，11 帧姿态全部不同。 |
| 13 | 94/94 | `DG_1hmEquip` 仍为 index 0，普通 Blender 仍为 0。 |

全 13 步都有 `Weap_Equip_MSG`（29346）、`Weap_EquipBehavior`（28761）、`WeapEquipBehavior`（28359）、`WeapEquipRoot`（28145）和 `WeapEquip.hkb`（28056）。逐步核对了 census 的节点数和连续下标。v114 与 v103 同一 segment/completed 的 249 帧姿态字节全部相同。旧日志的 census 只记录节点行，实际父边和选择器索引尚未留档。

作者树的外层 `STATE root/WeapEquip id=3` 指向状态机，后面另有 `WeapEquip_StandingState id=0` 和 `Weap_EquipDefaultState id=0`（ManualSelector）。右手分支中，`iDynamicGripMode` 控制的内层状态机有 `State0/State1`，两者各有可播 `1HM_Equip` 的选择器。内层 `State3 id=3` 才是本轮候选 Blender，内部也有一条 `1HM_Equip` 路径。因此外层的 state ID 或 clip 名只能作定位线索。 [作者树](weapequip_weap_equip_subtree.txt#L6)

`State3` 的作者 `flags=0`，两个 child weight 都为 1。`0xB09328` 只有在运行时 flags 含 `0x10` 时才读取区间权重。普通路径使用 child 的 `weight +0x40`，并根据 flags 应用钳制或曲线。若实测遇到该 Blender，应先记录运行时 flags 和实际活动子边，再解释哪个权重有效。 [上轮 Blender 字段审计](../../g5-transition-audit-20261118/README.md)

<a id="next-sample"></a>
## 四、下一轮最小验证

1. 复用候选 `pre-sample` 与 `post-sample`，覆盖装备第 1–13 步。按同一 `commanded/completed` 记录 clip 身份、原始父边、必要的输出转发、状态机的当前 ID/数组下标、选择器当前索引和实际 child 的模板/clone。保留多父与断链原因。
2. 第 1 步以两个已知活跃 Blender、可见的过渡效果和两个 clip 作观测覆盖检查。第 2–12 步要区分三种结果：链路确证经过 `State3`、链路完整但经过其他分支、范围或身份异常导致链路未采全。只有第一种结果进入该 Blender 的有效权重对照。
3. 若活动链完整且没有区间 Blender，优先查 clip generation 使用的实际采样相位、同步或输出混合。现有 11 帧只能证明采集到的 clip `local_time` 不足以区分姿态。源侧仍待同语义取证，G4P-7 维持 **FAIL**。

## 复核命令

使用当前工作区已有镜像和只读检查脚本。LLVM 参数是首选基址 `0x140000000` 加 RVA。

```powershell
$dir = 'build/behavior-graph-manager-audit-20261027'
$exe = "$dir/SkyrimSE.original.exe.unpacked.exe"
Get-FileHash -LiteralPath $exe, 'extern/CommonLibSSE/tests/REL/versionlib-1-6-1170-0.bin'
python -B "$dir/audit_image.py" vtable 226706 4
python -B "$dir/audit_image.py" vtable 226313 28
llvm-objdump -d --x86-asm-syntax=intel --start-address=0x140AD72C7 --stop-address=0x140AD733A $exe
llvm-objdump -d --x86-asm-syntax=intel --start-address=0x140AD56D6 --stop-address=0x140AD571B $exe
llvm-objdump -d --x86-asm-syntax=intel --start-address=0x140ADE8A0 --stop-address=0x140ADE8DC $exe
llvm-objdump -d --x86-asm-syntax=intel --start-address=0x140AE9285 --stop-address=0x140AE9298 $exe
```
