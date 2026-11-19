# v6-A 离线审计输入：`WeapEquipRoot` state 3 → `State3`

本目录是 [C-G5-TRANS v5 run 1](../run1-candidate/README.md) 判负后，为下一层
**`hkbStateMachine::StateInfo` → state 3 generator → `State3`** 离线审计准备的
有界输入包。全部为只读文本；不包含游戏进程、DLL 或完整 15 MB HKX 转储。

## 文件

| 文件 | 内容 | 字节 | SHA-256 |
| --- | --- | ---: | --- |
| `weapequip_weap_equip_subtree.txt` | `weapequip.hkx` 中 `root/WeapEquip` 子树（状态机、ManualSelector、`msg/State3`、children/bindings） | 379,210 | `7502E65C709B078D32F7B478383F3668C502A63E42476970A733775274EB6D7B` |
| `0_master_default_state_subtree.txt` | `0_Master.hkx` 中 `Default_State` 子树（0→10→1→12 转移、`MT_Behavior_State`/`Weap_*` 状态） | 331,230 | `F9789C9E2F43EC8B9465ACF60471C55B76C621B2DD294BB06461D13DF41907AF` |
| `runtime_evidence_v114.txt` | v114 run 1 的 `WeapEquipBFR` 子图行、cmd=1 census、`WeapEquipRoot`/`Default_Behavior` SM、K5 RECON | 4,881 | `AD31D8AC8E10CEC47CF8D958933EB97C63668CC6B5F8973E5B067EB123590816` |
| `stateinfo_layout_targets.md` | `hkbStateMachine`/`StateInfo`/ManualSelector 的引擎审计目标与问题清单 | — | — |
| `extract_context.py` | 从 `build/hkx-audit` 的完整转储重新生成上述三个文本 | 4,371 | — |

## 源 HKX

| 文件 | 路径 | 字节 | SHA-256 |
| --- | --- | ---: | --- |
| `0_Master.hkx` | `E:\SkyrimAE\mods\Pandora_Output\meshes\actors\character\Behaviors\0_Master.hkx` | 12,951,504 | `466cf1169ee0a0afad01532d3f1b9a1b1d410dc8dcc3ad3045bb1aae65775e61` |
| `weapequip.hkx` | `E:\SkyrimAE\mods\Pandora_Output\meshes\actors\character\Behaviors\weapequip.hkx` | 75,456 | `fd0f0e096eca0c5863b33e854883002ab9e19512aebc2d7161ae0a6ee5bb36e6` |

完整转储仍在本机 `build/hkx-audit/`（gitignored）：

- `0_master_v3.txt` 15,322,955 B `277C46704211C9D6484ADA0424652BF08F1BA56E229C6579E6B20E4CA4436D81`
- `weapequip_v3_full.txt` 2,355,413 B `E10E12F24A169740B404EB2D0707703112A7BF599C7E1252EA2561C08635B2BA`

## 重新生成

从仓库根目录执行（只读输入，覆盖本目录的三个 `.txt`）：

```powershell
python docs\diagnostics\g5-transition-probe-v5-20261119\hkx\extract_context.py
```

脚本只做行过滤/块截取，不解析 HKX 二进制；完整转储由 `build/hkx-audit/HkxDump` 生成。

## 为什么需要这些输入

v5 run 1 证明 `WeapEquipBFR +0x50` 指向的是 `weapequip.hkx` 的**模板图**
（`behavior-active=0`、`active-nodes=0`），不是运行时 clone。主图 census 仍能看到
`WeapEquipRoot`（state=3）和 `WeapEquip.hkb`，但没有顶层 `State3`。因此下一层必须从
`WeapEquipRoot` 的运行时 state 3 出发，沿 state generator / ManualSelector 链找到
`State3` Blender。`weapequip_weap_equip_subtree.txt` 给出作者树的 state/selector/blender
顺序，`0_master_default_state_subtree.txt` 给出 0→10 和 10→1 的转移上下文，
`runtime_evidence_v114.txt` 给出运行时节点身份和 state=3 的对照。

## 边界

- 只适用于 AE 1.6.1170 + 当前 Pandora_Output 行为文件；不外推 SE/VR。
- 不是运行时证据；v6 仍需新的只读探针。
- 不改变 `G4P-7 FAIL` / G5/G6，不产生 `tau_*`。
