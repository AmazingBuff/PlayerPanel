# 装备驱动非战斗 idle 与 DAC 相位：上机测试单（2026-10-20）

> 本轮只验证，不改代码。所有判据以日志行为准，画面观察分开记录；
> 日志匹配不等于视觉通过，反之亦然。

## 0. 构建与部署身份

| 项目 | 值 |
| --- | --- |
| 源码 | `oar-idle3` @ `2eef1a0`（工作树干净） |
| 构建身份 | `CharacterPanel-scenegraph-scenegraph-2eef1a0a89-cl94faaed0c6-20261020T134347Z` |
| DLL SHA-256 | `6C72C6D6074AECE264EFB58EB9F0C19166CB9CF27C8DFCB87774D9EDCCD433D6` |
| PDB SHA-256 | `347D8C3071908073FB0FBE98CCD48FB42E45122C42495827B70239F5605B42ED` |
| 部署目录 | `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\`（DLL+PDB 已同步校验） |
| 旧构建备份 | `dist\deploy-backup-20261019-29964b1a\`（`29964B1A…`，10-19 23:42） |
| 本地回归 | Release 配置/构建通过；`tests/animation_controller_test.cpp` 2/2 通过 |
| 前置 | Skyrim AE 1.6.1170 / SKSE 2.2.6；OAR 3.1.6（已安装版）；DAC NG Plus 3.2.4（原版未改） |
| INI | `Data\SKSE\Plugins\CharacterPanel.ini` 不存在 → 默认 `UnarmedIdle=non_combat_loop` |

本轮验证的是 [animation.md](animation.md)、[dac.md](dac.md)、[scene-graph.md](scene-graph.md)
描述但尚未上机的内容：装备驱动的非战斗 idle、空手两种模式、复制图内持械挂载、
原生 DAC 的 `Smooth_loop` 准备与关包还原，以及复制图表现/生命周期回归。

## 1. 启动核对（T00）

开一次游戏，进入任意存档，打开一次背包后关闭。日志应出现：

```
SCENEGRAPH BUILD CharacterPanel-scenegraph-…-20261020T134347Z runtime=… backend=scene-graph animation=isolated-OAR-query hotkeys=F7-recapture F8-toggle
ANIMATION setting UnarmedIdle=non_combat_loop
OAR 3.1.6 hooks installed: ConditionBase::Evaluate + NumericValue::GetValue; preview scopes only; active query …
DAC native inventory lifecycle sink installed
DAC original NG Plus matched rules=N handler=0x… stride=232
```

判据：

- 构建身份与上表一致；`runtime` 与实际运行时一致。
- OAR 行若为 `OAR hooks refused: no verified profile …`，记录被拒版本的四个字段，本轮 OAR 相关判据全部记"未验证"。
- DAC 行若为 `DAC native phase events disabled: …`，记录原因；此时面板应回退直接基础动作（见 T12），不崩。

## 2. 用例

### T01 空手站立开包（默认模式）

步骤：空手、收鞘、站立；打开背包。

期望画面：面板人物完整站立，播非战斗 idle 循环，无攻击起手、无 T-pose、开包不卡顿。

期望日志（同一 `actor`/`request` 串起来）：

```
ANIMATION actor=… request=… reason=… source-generation=… equipment-target=…
ANIMATION actor=… request=… goal=non-combat-idle family=… direct=false drawn=false
DAC actor=… request=… Smooth_loop preparation requested
DAC actor=… request=… Smooth_loop preparation ready
ANIMATION actor=… request=… asset ready index=… … mapping=table|identity-unverified
SCOPY HELD actor=… attached=0 missing=0 source-graph=read-only
```

判据：`preparation ready` 后才有 `asset ready`；`direct=false`；无 `discard stale` 风暴；
若出现 `failed: marker observation timed out` 或 `unarmed replacement unavailable`，
记录后按稳定回退判"机制成立、相位准备失败"。

### T02 空手 `mt_idle` 模式

步骤：

```powershell
Set-Content "E:\SkyrimAE\Data\SKSE\Plugins\CharacterPanel.ini" "[Animation]`nUnarmedIdle=mt_idle"
```

重启游戏，空手开包。

期望：`ANIMATION setting UnarmedIdle=mt_idle`；`direct=true`；**没有** `Smooth_loop preparation`
行；画面为原版 MT_Idle 站立。

测完删除该 INI（或改回 `non_combat_loop`）再继续后续用例，避免混臂。

### T03 持械（世界收鞘）开包

步骤：装备单手剑（不拔出）→ 打开背包。

期望：`ANIMATION … goal=non-combat-idle family=1h direct=false drawn=true`；
`SCOPY HELD … attached>=1 missing=0`；画面人物手持该武器播 1hm 非战斗 idle，
不是空手 idle、不是 MT_Idle。DAC 行为同 T01。

### T04 世界出鞘/攻击中开包

步骤：拔刀并挥砍一次，1–2 秒内打开背包。

期望：面板仍为持械非战斗 idle（不是攻击动作、不是空手）；`drawn=true`；
随后的 `asset ready` 属于该武器族。若画面回退为 MT_Idle，记录 `ANIMATION`
的 `choice`/`diagnostic` 行。

### T05 移动/潜行/跳跃/游泳中开包

步骤：分别在下落、潜行、冲刺、游泳时开包（至少两项）。

期望：面板一律为站立非战斗 idle（与进入姿态无关）；
取景一致：每次捕获出现一条 `SCOPY FRAME calibrated offset=… radius=…`，
`radius` 在站立量级（≈28），人物不偏上/偏下；无下坠、无位置漂移。

### T06 背包内快速换装

步骤：1h → 2h → 弓 → 空手，快速连续更换（每步间隔 1 秒内）。

期望：

- 每步出现 `CharacterManager equipment actor=… revision=… equipped=…` 与新的
  `ANIMATION … request=…`。
- 最终画面匹配**最后**的装备；旧的加载结果不回卷、不覆盖。
- `SCOPY HELD` 的 `attached/missing` 随新装备更新；`missing=0`。
- 无持续卡顿尖峰（切换期短暂重建可接受）。

### T07 挂载细节

步骤：盾、箭袋、双手斧、弓各开一次包（有则测）。

期望：`SCOPY HELD … side=… mount=… missing=0` 指向实际挂点；
武器/盾在手中，箭袋不误挂到手上；收鞘 MOV 链不抢挂点（`SCOPY WEAPON` 若存在，
node world 应随父骨变化）。

### T08 关闭背包（还原）

步骤：开包 → 换装或待机 → 关闭背包。

期望：`DAC restore actor=… epoch=… baseline=… spell-groups=… result=matched`；
**没有** `MISMATCH`；无关 buff/法术效果保留；世界角色装备与开包前一致
（背包内换装除外）；面板消失、世界正常渲染。

`result=matched` 与画面必须分开记录；MISMATCH 即本轮失败，保留日志与 PDB。

### T09 F7 / F8

- F7：`SCOPY BEGIN capture=…` → `SCOPY READY …` → `SCOPY PARK actor=…`；
  重捕获后 idle 不回卷到序列起点，兼容循环的相位尽量保持。
- F8：`SceneGraph draw actor=… enabled=0|1`，画面显示/隐藏正确，不残留半透明。

### T10 开关循环、读档、主菜单

步骤：连续开关背包 5 次；读档一次；回主菜单一次。

期望：无崩溃；`DAC old-world sessions discarded world=…` 出现在读档边界；
`CharacterManager lifetime boundary actor=…` 正常；`SCOPY PARK` 计数不异常暴涨；
无输入残留、无面板残影。

### T11 外观与透明

步骤：CBBE 与 UBE 各一次（各自存档/服装），开包观察。

期望：人物完整、无白剪影、头发/薄纱/裙摆透明正确；
`SCOPY FRAME …` 与 `SCOPY COMPOSITE … match=1` 正常。

### T12 降级面（可选，测后恢复）

步骤：临时重命名 OAR 或 DAC 的 DLL（或移出 mod 目录），开包；测完恢复。

期望：

- OAR 缺失：`OAR hooks unavailable: Open Animation Replacer is not loaded`，
  面板回退直接 MT_Idle，不崩。
- DAC 缺失：`DAC native phase events disabled: …`，面板仍显示（直接基础动作），不崩。

判据：降级有明确日志、无静默失败、无崩溃。

## 3. 结果记录

每次会话结束立即复制 `CharacterPanel.log`（日志按会话覆盖，不追加），
与 DLL/PDB 哈希一起归档。崩溃时额外保存 `crash-*.log` 与同包 PDB。

| 用例 | 结果（通过/失败/未测） | 关键日志行或截图 |
| --- | --- | --- |
| T00 |  |  |
| T01 |  |  |
| T02 |  |  |
| T03 |  |  |
| T04 |  |  |
| T05 |  |  |
| T06 |  |  |
| T07 |  |  |
| T08 |  |  |
| T09 |  |  |
| T10 |  |  |
| T11 |  |  |
| T12 |  |  |

## 4. 本轮不要做

- 不要在面板里验证 OAR 的实时决策：暂停期 OAR 不求值，面板按"装备 + 站立"查询，
  这是已知边界，不是失败。
- 不要把 `DAC restore … result=matched` 当作视觉证据，也不要用画面正常代替还原证据。
- 不要在同一个游戏会话里换 DLL；换包前后重启或读档。
- 不要为了通过而删除/改写 INI 之外的配置；OAR/DAC 安装保持原样。

## 5. 通过后的下一步

上机通过 → 记录到 [history.md](history.md)，再按 [dac.md](dac.md#smooth-moveset-compatibility-boundary)
的兼容边界决定下一实现工作包：IED placement 查询适配器（装备动作分支）、
战斗 Start/Loop 分支、安全回收或性能测量。未通过 → 保留日志与 PDB，先修当前缺陷，
不进入新工作包。

## 6. 第二轮修复（2026-10-20 22:30，待上机）

第一轮实测暴露的三个问题及本轮改动：

1. **弓开包无面板（扩散到弩/火把/双持/空手）**：`panel_idle_bases` 使用了引擎动画名表
   不存在的臆造名（`bow_idle.hkx`、`crossbow_idle.hkx`、`MT_IdleTorch.hkx`、`dw1hm_idle.hkx`、
   `h2h_idle.hkx`）。`find_base_animation` 失败后 `queue()` 不建立提案 → `ready()` 恒假 →
   `drawable()` 假 → 不合成 → 面板整块消失（截图 1）。现改为引擎原名：`Bow_IdleHeld.hkx`、
   `DLC01\CrossBow_IdleHeld.hkx`、`TOR_Idle.hkx`、`DW1HM1HMIdle.hkx`、`H2H_Idle.hkx`；
   并新增“族基名缺失 → MT_Idle 兜底”，任何族缺名都不再空白。
2. **背包内换装直接跳到 idle**：新增 equip/unequip 一次性过渡。`reason='equipment'` 且装备
   变化时先播 `panel_transition_bases()`（装备=Equip、卸下=Unequip、同族换武器按 Equip），
   时钟到边界后由既有 `queue_idle()` 接回 idle；开包沿仍直接进 idle（没有世界过渡可镜像）。
3. **idle 是原版而非 OAR**：DAC 准备失败时不再强制 `direct`，仍向 OAR 查询（源角色自身
   效果）：能选中替换件就用，否则才落基名。第一轮日志显示全部 33 条原生规则被拒、
   `DAC snapshot effects=0 scope=0`；本轮给 `copy_rule` 的每个被拒法术加详细字段行
   （`vmad/delivery/casting/cost/perk/effects/type`），用于精确定位为何 `Smooth_loop`
   （0x80A/0x810/0x82E）整条被拒。

| 项目 | 值 |
| --- | --- |
| 构建身份 | `CharacterPanel-scenegraph-scenegraph-9f943e12c8+dirty-cl94faaed0c6-20261020T143009Z` |
| DLL SHA-256 | `6B16DABDF0F53EF1F0B1598588BECFF0D2A486C8F14C4E0B216F21C69C3C3759` |
| PDB SHA-256 | `A95A5555CFF62D5F48ED42E419183EA2A10F39B3BAA014E67E867028380B8654` |
| 部署 | `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\`（DLL+PDB 已同步） |
| 本地回归 | Release 配置/构建通过；`animation_controller_test` 2/2 通过 |

第二轮判读：

- **弓（开包）**：`goal=non-combat-idle family=bow` → `state=… path='Animations\Bow_IdleHeld.hkx'`
  → `asset ready … replaced=true|false` → 面板出现（不再空白）。
- **背包内换到弓**：`ANIMATION … transition=equip form=bow` → `asset ready … path='Animations\Bow_Equip.hkx'`
  （或 OAR 替换件）→ `handover action=2 …` → 过渡结束后 `goal=… family=bow` →
  `asset ready … Bow_IdleHeld.hkx`。这就是与游戏世界一致的 Equip → Idle。
- **卸下武器**：`transition=unequip form=…` → `…Unequip.hkx` → 随后 `MT_Idle`。
- **OAR 最终 idle**：若出现 `native idle goal unavailable; querying OAR without preparation`
  且随后 `asset ready … replaced=true`，说明 OAR 选中的替换件已经生效；`replaced=false`
  则把同请求的 `state=`/`diagnostic=` 行发回。
- **DAC 诊断**：启动后每个被拒法术一行 `DAC native spell … rejected: … (vmad=… delivery=…
  casting=… cost=… perk=… effects=… type=…)`；把 `event='Smooth_loop'` 对应的三行
  （`sp_loop on` / `sp_combat off` / `sp_combat_loop off`）发回，即可定位预检误拒的字段。

## 7. 第三轮修复（2026-10-20 23:00，待上机）

第二轮日志给出了决定性证据：**66 个原生规则法术全部 `vmad=true`**，而
`Smooth Moveset.esp` 的 SPEL/MGEF 记录里没有任何 VMAD 子记录（记录 flags=0，未压缩）。
`TESForm::HasVMAD()` 是 SKSE 的 VM 句柄启发式，对"被脚本引用"的法术也会返回 true，
用它做"记录自带 VMAD"的判据是假阳性——这正是 DAC 全部规则被拒的原因。

本轮改动：

1. **移除 VMAD 门槛**（`restorable_spell` 的 spell 与 MGEF 两处）：其余惰性自目标标记子集
   检查（self、Constant/FAF、零耗、无 perk、无效果条件/范围/关联形态、非敌对、零基础耗）
   全部保留。预期规则被接受，`DAC native spell … rejected` 行消失。
2. **三阶段相位时机**：装备变化请求不再强制 `Smooth_loop` 准备（开包沿仍准备）。Equip
   过渡播放时，其标注（`BeginWeaponDraw`/`weaponDraw`）经 DAC 施放 `start on`（0x809）
   → 边界后的 idle 查询得到 **Start** 成员；Start 的 `Smooth_loop` 标注施放 `loop on`
   （0x80A）→ 再下一个边界查询得到 **Loop**。即世界里的 Equip → Start → Loop。
3. **Start 一次性**：按文件夹名（`… idle start` / `start`）识别 Start 成员，`loop=false`
   播一次后由边界重新查询；不再把 Start 当循环体驻留。这修掉"在 Equip→Start 阶段直接用
   Start 做 idle loop"。
4. **武器挂点**：捕获时记录每个挂载件的原鞘挂点（`WeaponSword`/`WeaponAxe`/… 父节点）
   与本地变换；Equip 过渡在 `weaponDraw` 事件之前把武器留在鞘位，事件后挂回手部；
   Unequip 反向。复制图没有 behavior graph，这是用动画自己的 `weaponDraw` 时点复刻
   "先从腰/背取武器，再拿在手里"。

| 项目 | 值 |
| --- | --- |
| 构建身份 | `CharacterPanel-scenegraph-scenegraph-a3dea5e07f+dirty-cl94faaed0c6-20261020T145855Z` |
| DLL SHA-256 | `486CEBFEE9E75EB64CF7631A1021DFC6F2836A2732CAB3B55AB47BFDB776A33A` |
| PDB SHA-256 | `E06C50FE4F419B2FF952A596E8B831DB3CC05F7DBC2E30E5780B2D95BFA09977` |
| 部署 | `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\`（DLL+PDB 已同步） |
| 本地回归 | Release 配置/构建通过；`animation_controller_test` 2/2 通过 |

第三轮判读：

- **DAC 恢复**：不再出现 `DAC native spell … rejected`；出现
  `DAC actor=… Smooth_loop preparation requested` 后应为 `ready`（而非 `failed`）；
  `DAC snapshot … effects=N scope=M` 的 `scope` 应大于 0。
- **三阶段**：背包内换武器后依次出现
  `transition=equip` → `event='weaponDraw' time=…` → `goal=…` 后的
  `state=… path='… idle Start …'`（`replaced=true`）→ `sequence start one-shot path='…'`
  → `event='Smooth_loop'` → 下一个边界 `state=… path='… idle Loop …'`。
- **武器挂点**：Equip 过渡的前半段武器在鞘位（腰/背），`weaponDraw` 时点之后才出现在手里；
  若仍一开播就在手上，把该过渡的 `weaponDraw` 时间与 `SCOPY HELD` 行发回。
- 若仍见 `Smooth_loop preparation failed: …`，把该行与 `DAC native spell` 行发回
  （可能是事件级 `event_scope` 的新拒绝原因，例如无关效果的关键词冲突）。
