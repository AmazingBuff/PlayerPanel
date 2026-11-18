# K5-5 装备过渡离线审计（2026-11-18）

**结论：AE 1.6.1170 的 `hkbBlendingTransitionEffect::timeInTransition` 确实参与姿态混合。** 引擎从活动状态机的 `activeTransitions` 取过渡实例，按 **0x28 字节**遍历记录。已经定位读取路径，仍待确认 Track 2 的装备入口在候选第 2–12 步实际采用哪种效果、该实例在两侧采样点的寿命及读数。

## 一、输入与装备分支

- 本轮复核的解包镜像为工作区已有的 `build/behavior-graph-manager-audit-20261027/SkyrimSE.original.exe.unpacked.exe`，SHA-256 `843823C115DBC56B5E9740350DB49170CC4EEA013A79513B7FD066DEEC719A2B`。地址库 `versionlib-1-6-1170-0.bin` 的 SHA-256 为 `C4093C569A3C83B26587F4B9EA4C55DE9AE6E73B84A2AF9FB3FBD30E2FE0D452`。CommonLibSSE 固定在 `94faaed0c`。下文地址均为镜像 RVA。 [镜像复查方法](../../behavior-graph-construction-audit.md#复现)
- 既有 HKX 审计记录 `0_Master.hkx` 内九态机的 `0→10` 边由 `WeapEquip(2)` 触发，装备子图通过 `WeapEquipBFR` 引用 `weapequip.hkx`。D3 候选运行验证了 `1HM_Equip` 分支。v103 层日志另见 `Default_Behavior`（id 172）和装备段的 `Weap_Equip_MSG`、`WeapEquipRoot`、`WeapEquip.hkb`。定位运行时实例时仍须核对状态表和父子路径，现有日志只给出名称和节点 id。 [既有 HKX 审计](../../independent-behavior-graph.md#c-g3b-a1-structural-audit) · [v103 层日志](../CharacterPanel-g5k5c-v103-run1-20261114-2200.log)
- 当前检出缺少 `0_Master.hkx`、`weapequip.hkx`、`HkxDump` 和完整转储。上述分支来自已有审计。本轮能够独立核实引擎实现，尚未重新提取这条边的 HKX 效果类、曲线、时长和变量绑定。补验应使用闭包中登记的 Pandora_Output 文件。 [闭包文件和完整哈希](../g5-k1-c1-20261114/closure-files.txt)

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
