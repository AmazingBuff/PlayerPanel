# K5-5 装备过渡离线审计（2026-11-18）

**结论：AE 1.6.1170 的 `hkbBlendingTransitionEffect::timeInTransition` 确实参与姿态混合。** 引擎从活动状态机的 `activeTransitions` 取过渡实例，按 **0x28 字节**遍历记录。离线字段补审计（§六）已确认：本次 `0→10` 装备边为 `hkbBlendingTransitionEffect`（作者 `duration=0.2 s`、smooth、self-reset）；`1HM_Equip` 由 `Weap_EquipDefaultState/msg/State3` 的 Blender 分支选择，不是独立 transition。仍待运行时实例在两侧采样点的寿命、`timeInTransition`/`endIntervalWeight` 读数及与姿态的相关性。

## 一、输入与装备分支

- 本轮复核的解包镜像为工作区已有的 `build/behavior-graph-manager-audit-20261027/SkyrimSE.original.exe.unpacked.exe`，SHA-256 `843823C115DBC56B5E9740350DB49170CC4EEA013A79513B7FD066DEEC719A2B`。地址库 `versionlib-1-6-1170-0.bin` 的 SHA-256 为 `C4093C569A3C83B26587F4B9EA4C55DE9AE6E73B84A2AF9FB3FBD30E2FE0D452`。CommonLibSSE 固定在 `94faaed0c`。下文地址均为镜像 RVA。 [镜像复查方法](../../behavior-graph-construction-audit.md#复现)
- 既有 HKX 审计记录 `0_Master.hkx` 内九态机的 `0→10` 边由 `WeapEquip(2)` 触发，装备子图通过 `WeapEquipBFR` 引用 `weapequip.hkx`。D3 候选运行验证了 `1HM_Equip` 分支。v103 层日志另见 `Default_Behavior`（id 172）和装备段的 `Weap_Equip_MSG`、`WeapEquipRoot`、`WeapEquip.hkb`。定位运行时实例时仍须核对状态表和父子路径，现有日志只给出名称和节点 id。 [既有 HKX 审计](../../independent-behavior-graph.md#c-g3b-a1-structural-audit) · [v103 层日志](../CharacterPanel-g5k5c-v103-run1-20261114-2200.log)
- 当前检出缺少 `0_Master.hkx`、`weapequip.hkx`、`HkxDump` 和完整转储（**2026-11-18 复核：本机实际存在精确哈希文件与转储，见 §六；该句保留为当时检出描述**）。上述分支来自已有审计。本轮能够独立核实引擎实现；§六 已补出这条边的 HKX 效果类、曲线和时长，运行时实例的变量绑定仍待采集。补验应使用闭包中登记的 Pandora_Output 文件。 [闭包文件和完整哈希](../g5-k1-c1-20261114/closure-files.txt)

## 二、引擎实证

| 对象或行为 | AE 1.6.1170 证据 |
| --- | --- |
| 活动过渡数组 | 状态机 vtable `0x1964250`（ID 226812），经 RTTI 核名。反射 `0x201E4A8` 给出 `activeTransitions` 在状态机 `+0xB0`。`59342` 在 `0xAD8EBE` 读数量，在 `0xAD8F07–0xAD8F0E` 取条目首指针，在 `0xAD9036` 以 `0x28` 步进。类型记录 `0x201E8A0` 中的 `hkbStateMachineActiveTransitionInfo` 大小同为 `0x28`。 |
| 条目首指针指向活跃效果 | `59398` 在 `0xADE53D–0xADE55A` 取 `data[i × 0x28 + 0x00]`，查询图的活动节点映射，并在 `0xADE592` 调该对象 vtable 的 `+0xE0`。对 `hkbBlendingTransitionEffect`，该槽是 `59955 / 0xB01670`，读取 `+0x84`、`+0x80` 和起止生成器来判断过渡是否结束。 |
| 混合计时字段 | 效果 vtable `0x19637E8`（ID 226738），经 RTTI 核名。反射 `0x201EAA0` / `0x201EAC8` 与头文件一致，`timeRemaining` 在 `+0x80`、`timeInTransition` 在 `+0x84`。`Update`（`59948`）跳到 `0xB01C20`，在 `0xB01CEE–0xB01D0A` 分别减、加同一个更新步长。 |
| 计时如何进入姿态 | `Generate`（`59952`）跳到 `0xB0F830`。常规双生成器路径在 `0xB0F93E–0xB0F974` 以 `elapsed + remaining` 为分母，以 `elapsed + Generate 时间偏移` 为分子做限幅和归一化，随后将结果和 `blendCurve`（`+0x5B`）传给 `58448`。`0xB0F98A` 将返回权重传入姿态混合函数 `59303`。裸 `timeInTransition` 是原始计时，实际权重还受曲线、生成时间偏移和路径分支影响。 |

[状态机声明](../../../extern/CommonLibSSE/include/RE/H/hkbStateMachine.h) · [效果声明](../../../extern/CommonLibSSE/include/RE/H/hkbBlendingTransitionEffect.h)

**读取约束。** CommonLibSSE 把 `activeTransitions` 声明为 `hkArray<hkRefVariant>`，其条目类型大小只有 `0x08`。在这个固定版本上，数组头位置可以作为对照，按该声明索引第二条及以后会读错。采用已审计的 `0x28` 步长，分别记录空数组、无法读取和效果类型不匹配。确认实例 vtable 后再解释效果字段。 [数组条目类型声明](../../../extern/CommonLibSSE/include/RE/H/hkRefVariant.h)

### Blender 路线复核

- `hkbBlenderGenerator` 的 `children` 在对象 `+0x60`，是指针数组。校验函数 `59526` 在 `0xAE7930–0xAE797A` 先取数组缓冲区，再以 `0x08` 步进取各 child 指针。`childrenInternalStates` 在 `+0x70`，`59521` 分配时指定元素宽度 2，`59534` 按每项两字节写状态。v109 对对象窗口内的每个指针只检查一层目标 vtable，数组缓冲区和这组内联记录都超出了该检查的识别能力。
- 区间 blend 的备选读数是 `endIntervalWeight(+0x90)`、`beginIntervalIndex(+0x98)` 和 `endIntervalIndex(+0x9A)`。`59535` 写这些字段，`60040` 根据 child 索引返回 `weight` 或 `1−weight`。这是另一条可核实的引擎路径。它需要绑定到实际活跃节点。v108 的 `0x100` 窗口已经覆盖这些偏移，其报告未见组内逐步变化，因此优先级低于活动过渡效果。v109 报告没有逐代 child/internal 行，按该轮判据 CP-3 未满足。 [v108 报告](../g5-state-probe-20261117/README.md) · [v109 报告](../g5-state-child-20261117/README.md)

## 三、与门 5 数据对照

重新读取正式 CPK5、CPK5S 和配对清单。候选第 1 步为 `1HM_Equip + MT_Idle`，clip 时钟 `0.016666668`。第 **2–12** 步只记录到 `1HM_Equip`，时钟逐位固定在 `0.208333343`，11 步都配到源 seq **145**。源 seq **137–144** 仍同时记录两种 clip（装备时钟约 `0.021833–0.189167`），seq **145** 只记录 `1HM_Equip`（`0.213833347`）。这个窗口可检验两侧是否在同一有效过渡阶段，也能检验候选的过渡计时是否继续前进。归档只包含 clip 和姿态读数，过渡实例和计时值仍需新采集。 [正式配对清单](../g5-gate5-20261116/formal/gate4-pair-manifest.json) · [同一时钟的姿态残差](../g5-state-20261117/README.md)

## 四、下一次验证的判别点

1. 在实际生效的 HKX 中核对 `0→10` 与进入 `1HM_Equip` 的嵌套过渡，登记效果类、起止节点、曲线及配置时长。运行侧用状态表和子图路径定位同一条边。
2. 候选覆盖装备事件后基线及完成步 1–13，源保留等价的自然装备起始窗口。复用候选 K5 完成点和源 K5 apply-return 点，按 `completed` / `seq` 关联姿态、clip 时钟、状态机当前/前一状态、活动效果类型和起止节点。对匹配的类型读 `duration`、`timeInTransition`、`timeRemaining`、`blendCurve`。记录效果是否在采样点之前结束，避免把空数组误作零权重。 [候选采样点](../../../src/animation/behavior_graph/independent_graph.cpp#L2893) · [源采样点](../../../src/animation/behavior_graph/probe_liveness.cpp#L892)
3. 两侧确认同一语义的活跃效果后，再检验同一 `local_time` 下计时是否随步推进，以及能否解释组内姿态差异。若目标边采用别的效果或在采样点已结束，按已核实的节点路径查另一层（包括 Blender 区间）或收窄观测窗口。新的采集先用于诊断，正式比较仍保留冻结的门 5 判据和 `G4P-7 FAIL` 裁定。

## 五、复核命令

从仓库根目录执行。首条命令核对 RTTI 和虚表。之后用已有解包镜像查看更新字段和权重计算；`llvm-objdump` 的起止地址为 `0x140000000 + RVA`。

```powershell
$auditDir = 'build/behavior-graph-manager-audit-20261027'
python -B "$auditDir/audit_image.py" vtable 226738 29
python -B "$auditDir/audit_image.py" vtable 226812 28
$image = "$auditDir/SkyrimSE.original.exe.unpacked.exe"
llvm-objdump -d --x86-asm-syntax=intel --start-address=0x140B01CEE --stop-address=0x140B01D12 $image
llvm-objdump -d --x86-asm-syntax=intel --start-address=0x140B0F93E --stop-address=0x140B0F98F $image
```

## 六、离线字段补审计（2026-11-18，v3 dump）

**状态：离线只读补审计完成；未上机、未改产品代码、未改门 5 判据。** 本节把 §一 的“缺少 HKX/转储”前提按本机实际状态更正，并给出 `0→10` 装备边与 `1HM_Equip` 分支的作者数据字段。

### 6.1 输入与当前实际状态

§一 写“当前检出缺少 `0_Master.hkx`、`weapequip.hkx`、`HkxDump` 和完整转储”。本机复核结果如下；这些文件在 `build/` 或 MO2 安装树中，不属于 git 跟踪内容，但可直接用于只读审计。

| 输入 | 大小 | SHA-256 | 与闭包登记 |
| --- | ---: | --- | --- |
| `E:\SkyrimAE\mods\Pandora_Output\meshes\actors\character\Behaviors\0_Master.hkx` | 12,951,504 B | `466cf1169ee0a0afad01532d3f1b9a1b1d410dc8dcc3ad3045bb1aae65775e61` | 一致 |
| `E:\SkyrimAE\mods\Pandora_Output\meshes\actors\character\Behaviors\weapequip.hkx` | 75,456 B | `fd0f0e096eca0c5863b33e854883002ab9e19512aebc2d7161ae0a6ee5bb36e6` | 一致 |

两文件与 [`../g5-k1-c1-20261114/closure-files.txt`](../g5-k1-c1-20261114/closure-files.txt) 的 `0_master.hkx` / `weapequip.hkx` 行逐位相同。旧 `build/hkx-audit/0_master_v2.txt` 的头两行已经记录了同一路径和同一 `0_Master` 哈希，其第 85601 行起递归的 `weapequip.hkx` 哈希也一致；`build/hkx-audit/HkxDump` 的可执行文件仍在。**因此不需要等待新的 HKX 或完整转储；§一 的该条只保留为当时检出描述。**

### 6.2 工具与工件

本地 `build/hkx-audit/HkxDump` 已扩为 v3（gitignored 工具）：

- 每条 `TRANS`/`WILDCARD` 追加 `hkbBlendingTransitionEffect` 的 `duration`、`toGeneratorStartTimeFraction`、`flags`、`endMode`、`blendCurve`、`selfTransitionMode`、`eventMode`；`hkbGeneratorTransitionEffect` 追加 `blendInDuration`、`blendOutDuration`、`syncToGeneratorStartTime`、`transitionGenerator`。
- 每个 `BLEND` 追加 `referencePoseWeightThreshold`、`blendParameter`、`min/maxCyclicBlendParameter`、`indexOfSyncMasterChild`、`flags`、`subtractLastChild`；每个 `CHILD` 追加 `weight`、`worldFromModelWeight`、generator 描述与 `boneWeights` 数。
- `--full` 选项关闭“每对象一次”的全局去重，并保留递归栈防环，用于单独展开 `weapequip.hkx` 的共享 manual-selector 子树。

构建与运行命令（从仓库根目录）：

```powershell
dotnet build build/hkx-audit/HkxDump/HkxDump.csproj -c Release --nologo
$dump = 'build/hkx-audit/HkxDump/bin/Release/net10.0/HkxDump.exe'
& $dump 'E:\SkyrimAE\mods\Pandora_Output\meshes\actors\character\Behaviors\0_Master.hkx' 'build/hkx-audit/0_master_v3.txt' 'E:\SkyrimAE\mods'
& $dump 'E:\SkyrimAE\mods\Pandora_Output\meshes\actors\character\Behaviors\weapequip.hkx' 'build/hkx-audit/weapequip_v3_full.txt' 'E:\SkyrimAE\mods' --full
```

| 工件 | 大小 | SHA-256 |
| --- | ---: | --- |
| `build/hkx-audit/0_master_v3.txt` | 15,322,955 B | `277C46704211C9D6484ADA0424652BF08F1BA56E229C6579E6B20E4CA4436D81` |
| `build/hkx-audit/weapequip_v3_full.txt` | 2,355,413 B | `E10E12F24A169740B404EB2D0707703112A7BF599C7E1252EA2561C08635B2BA` |

`0_master_v3.txt` 加载闭包 `LOADED-FILES 23`；`0_Master` 头部哈希与上表一致。`weapequip_v3_full.txt` 头部哈希 `fd0f0e09…36e6` 与上表一致。

### 6.3 结果一：`0→10` 装备边

`0_master_v3.txt:8836`（`Default_State` 的 `MT_Behavior_State` 状态 0 → 状态 10）：

```text
TRANS toState=10 fromNested=0 toNested=0 eventId=2(WeapEquip) trigger=[0,0] initiate=[0,0] cond=null effect=hkbBlendingTransitionEffect(duration=0.2 startFrac=0 flags=0 endMode=0 blendCurve=0 self=2 event=0) prio=0 flags=256
```

按 HKX2 枚举解码：`blendCurve=0` 为 `BLEND_CURVE_SMOOTH`，`endMode=0` 为 `END_MODE_NONE`，`self=2` 为 `SELF_TRANSITION_MODE_RESET`。同一 `0→10` 事件还有两条边：

- `0_master_v3.txt:8946`：`Weap_Unequip_State`（状态 12）→ 10，`duration=0 self=0`；不是本次从 MT idle 出发的装备边。
- `0_master_v3.txt:9234`：`Weap_Readied_State`（状态 1）→ 10，`cond=EXPR[IsNPC == 0]`，`duration=0.2 self=2`；不是本次首段。

本次正式场景从 `MT_Behavior_State`（状态 0）进入，故**相关边是第一条**：`hkbBlendingTransitionEffect`，作者时长 **0.2 s**，`startFrac=0`，smooth 曲线，self-reset。

### 6.4 结果二：`1HM_Equip` 分支是 Blender，不是独立 transition

`weapequip_v3_full.txt` 的 `root/WeapEquip/WeapEquip_StandingState/Weap_EquipDefaultState` 子树：

| 行 | 读数 |
| ---: | --- |
| 5145–5146 | `MSG …/Weap_EquipDefaultState selected=0 n=13`；`selectedGeneratorIndex` 绑定 `var[12]=iRightHandType` |
| 5147–5148 | `MSG …/Weap_EquipDefaultState/msg selected=0 n=12`；`selectedGeneratorIndex` 绑定 `var[13]=iLeftHandType` |
| 5354 | `BLEND …/msg/State3 children=2 refPoseThreshold=0 blendParam=1 min=0 max=1 syncMaster=-1 flags=0 subtractLast=False` |
| 5355 | `CHILD …/msg/State3 index=0 weight=1 wfm=1 gen=Clip[Animations\2HC_Equip.hkx] boneWeights=99` |
| 5360 | `CHILD …/msg/State3 index=1 weight=1 wfm=0 gen=StateMachine boneWeights=99` |
| 5362–5363 | `SM …/msg/State3/b states=6 wildcard=0 startStateId=1`；`startStateId` 绑定 `var[17]=iLeftHandEquipped_DG` |
| 5364–5365 | `STATE …/State3/b/State0 id=1`，`CLIP … anim=Animations\1HM_Equip.hkx triggers=4` |

`0_master_v3.txt`（含 23 个闭包文件）与 `weapequip_v3_full.txt` 内**没有任何 `effect=hkbGeneratorTransitionEffect`**；`1HM_Equip` 的选择与混合由 `Weap_EquipDefaultState` 两级 manual-selector 和 `msg/State3` 的 `hkbBlenderGenerator` 完成。作者 `blendParameter=1`、区间 `[0,1]`；两个 child 的作者权重均为 1，运行时实际区间权重/child 权重不在 HKX 中。

### 6.5 解释与边界

1. **0.2 s 过渡与候选平台同量级**：候选正式数据第 1 步 `1HM_Equip` localTime 为 `0.016666668`，第 2–12 步固定在 `0.208333343`（约 12.5 帧 @60 Hz）；作者装备 transition 为 0.2 s（12 帧）。这支持“0.2 s transition 是首段主计时轴”的方向，但**不是**运行时 `timeInTransition` 已读出的证据；仍须按 §四第 2 条在候选/源同代采样点采集。
2. **`1HM_Equip` 的混合标量在 Blender**：`msg/State3` 的 `blendParameter`/运行时 `endIntervalWeight`/child `weight` 是 K5-5 单标量候选，而不是状态机 transition。v109 的 0x200 直接指针扫描没有进入 `+0x60` children 数组和 `+0x70` 内联 internal-state，故零发现不否定本路径。
3. **不能从事后数据改判**：本节只补作者字段与结构，不改变 `G4P-7 FAIL`/G5 未通过；不删轨、不放宽 `tau`、不把 0.2 s 当作通过条件。

**下一步**：按 §四第 2 条预注册运行时只读探针；候选 K5 点与源 K5 apply-return 点同时记录 state-machine `activeTransitions`（0x28 步长）和 Blender `+0x4C/+0x90/+0x94/+0x98/+0x9A` + `+0x60` children/`+0x40` weight。运行时读数回来前，0.2 s 与 Blender 标量都只是作者数据/假设。
