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
3. **不做名字调度**：Start/Stop 的推进完全由剪辑自带标注（经 DAC 施放相位法术）与 OAR 的
   相位条件驱动；循环边界本来就重新查询 OAR，Start 的 `Smooth_loop` 标注在边界前 0.3s
   触发（2.033s / 2.33s，第二轮日志实测），因此边界查询自然落到 Loop。没有按文件夹名
   强制 Start 一次性，也没有 playlist。
4. **武器挂点**：捕获时记录每个挂载件的原鞘挂点（`WeaponSword`/`WeaponAxe`/… 父节点）
   与本地变换；Equip 过渡在 `weaponDraw` 事件之前把武器留在鞘位，事件后挂回手部；
   Unequip 反向。复制图没有 behavior graph，这是用动画自己的 `weaponDraw` 时点复刻
   "先从腰/背取武器，再拿在手里"。

| 项目 | 值 |
| --- | --- |
| 构建身份 | `CharacterPanel-scenegraph-scenegraph-5d5c510017+dirty-cl94faaed0c6-20261020T151757Z` |
| DLL SHA-256 | `FF33FE8CBF5FE4443679ABA66D0A70BD3A39707F082365047E797344B288EA54` |
| PDB SHA-256 | `AD1B15038EF6C2463123219549E932CB046F03975294B5982A478906167EC29E` |
| 部署 | `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\`（DLL+PDB 已同步） |
| 本地回归 | Release 配置/构建通过；`animation_controller_test` 2/2 通过 |

第三轮判读：

- **DAC 恢复**：不再出现 `DAC native spell … rejected`；出现
  `DAC actor=… Smooth_loop preparation requested` 后应为 `ready`（而非 `failed`）；
  `DAC snapshot … effects=N scope=M` 的 `scope` 应大于 0。
- **三阶段**：背包内换武器后依次出现
  `transition=equip` → `event='weaponDraw' time=…` → `goal=…` 后的
  `state=… path='… idle Start …'`（`replaced=true`）→ `event='Smooth_loop'`
  → 下一个边界 `state=… path='… idle Loop …'`。Start 段驻留超过一个循环体即说明
  `Smooth_loop` 的 DAC 施法没有落地（把该事件前后的 `DAC actor=… event='Smooth_loop'`
  行发回）。
- **武器挂点**：Equip 过渡的前半段武器在鞘位（腰/背），`weaponDraw` 时点之后才出现在手里；
  若仍一开播就在手上，把该过渡的 `weaponDraw` 时间与 `SCOPY HELD` 行发回。
- 若仍见 `Smooth_loop preparation failed: …`，把该行与 `DAC native spell` 行发回
  （可能是事件级 `event_scope` 的新拒绝原因，例如无关效果的关键词冲突）。

## 8. 脚本附件预检复核（2026-10-21，待上机）

第 6、7 节保留当时的日志、判断和构建信息。本节修正其中对 `HasVMAD()` 和验证范围的解释，
不把第三轮“待上机”改成已通过。

`TESForm::HasVMAD()` 的实际实现只检查 VM 对象句柄是否为空。`true` 不能证明记录附带脚本，
也不能仅凭该值确定句柄为何存在。因此当时 66 个条目全部为 true 足以解释旧门槛全部误拒，
但“被脚本引用”不是已经验证的具体原因。直接移除该门槛解决误拒，却没有建立真正的无脚本边界。

当前修复在 `restorable_spell` 中核验每个 MGEF 的最后贡献插件定义，供规则复制和关键词碰撞
范围共同使用。独立只读文件流扫描标准顶层 MGEF 记录，通过 SDK 映射原始 ID 的 master、
完整插件与轻量插件索引；不碰引擎文件游标，不调用 `HasVMAD()` 或枚举运行中的脚本实例。
无 VMAD，或 VMAD v5、object format 1/2 且仅含脚本数为零的六字节头，才通过附件检查；
脚本数大于零拒绝整个事件。原有 self、零耗、非敌对等标记限制继续有效。

无贡献插件、动态形态、文件打不开、记录缺失、边界/格式损坏、deleted/ignored 以及目标记录
被压缩，均报告 unavailable 并拒绝，不能按“没找到脚本”放行。当前不解压 MGEF。
成功分类按运行时 ID 与贡献文件身份缓存；覆盖补丁不能复用原插件的 script-free 结果。
仓库原始 Smooth ESP 的 19 个 SPEL、19 个 MGEF 无 VMAD、未压缩，仅代表该源文件。

本地仅选两项离线行为用例，见 [构建说明](build.md#verification-and-limits)：生产扫描器/缓存/
放行逻辑接受具有模拟 VM 句柄的无脚本标记（含 absent/empty VMAD）；脚本覆盖记录在没有运行中
脚本实例时仍于模拟派发前被拒绝。它们不调用游戏的 SDK 文件选择/ID 映射或虚拟文件系统。

上机检查（尚未执行，需记录实际 DLL/PDB 身份）：

- 使用实际载入顺序核对 Smooth 标记的最后贡献记录及其 VMAD/压缩状态。确认模组管理器环境
  中贡献文件可见，完整/轻量插件与 master 引用均匹配实际 MGEF；若被拒，保留
  `DAC native spell ... rejected` 的效果 ID 和具体原因，不能用改回 `HasVMAD()` 绕过。
- 对无脚本原包观察 `scope>0`、preparation ready、真实 Loop 路径和关包 restore 结果；
  在隔离测试配置中，给同一 MGEF 添加有脚本的最终覆盖，确认没有运行实例时也先拒绝对应
  整个事件，不发生原生派发。不要改写仓库原始资源或日常存档来取得该证据。

`native call completed` 只证明函数返回。Start 重复仍需分别检查标注、真实效果可见性和 OAR
边界重查，不能直接断言施法未落地。`restore ... result=matched` 只比较受控效果快照值，
不证明 Papyrus 状态或任意副作用回滚。运行时改定义、暂停期施法、恢复可见性仍未验证。
规范以 [DAC 文档](dac.md#marker-script-attachment-preflight) 为准。

## 9. IED placement 适配（2026-10-21，待上机）

前面各轮保持历史记录。本节新增当前 IED 查询边界，未把本地构建或 DLL 可见性当作游戏验证。

当前支持 `OpenAnimationReplacer-IEDConditionExtensions` 的
`IED_GearNodeEquippedPlacementHint`、`IED_GearNodePlacementHint` 与
`SDS_IsShieldOnBackEnabled`，条件版本为 1.0.0.0。placement 的三个基础 component
逐项核对类型、名称和顺序；SDS 必须无 component。全量 Smooth 来源 config 扫描分别有
53、154、32 处，数量不代表原生注册或选片已成功。用户提供的压缩包标签为 1.0.3，DLL 导出的
实际 SKSE 版本为 **1.0.2.0**；运行时按后者核验，不按文件名推断。原始 ZIP/DLL 保持不变，
来源及 SHA-256 见 [适配说明](oar-hooks.md#native-ied-placement-conditions-2026-10-21)。

原生 `Evaluate` 继续处理左/右手或 gear node、比较、盾背配置、disabled 和 negated；false 不能强制改成 true。
SDS 条件通过 provider 现有接口读取真实来源 Actor 的 `GetShieldOnBackEnabled` 设置，
需要 provider 自己完成 SDS interface/condition 注册并满足分支前置依赖；SDS 不是核心非战斗
阶段的普遍必要依赖。本适配不提供 SDS 接口或合成设置。
仅查询当前来源角色装备/IED 配置，`hand_override` 历史装备、未知条件、非兼容 schema/version
和缺失/不匹配 provider 均拒绝。其余第三方条件仍不放行。未重建、注册工厂、安装或修改 provider。

本地 [两个行为用例](build.md#ied-condition-verification) 验证原生结果保留（同一代表场景含 SDS
零 component 的 false 与 negation），以及历史装备作用域
在原生读取前被拒绝；它们不验证游戏 IED API。先前 DAC 脚本附件规则与限制继续有效。

沿用既有两类上机核验，在装备选片类别中追加以下观察；尚未执行：

- 保留实际 CharacterPanel DLL/PDB 与 `SCENEGRAPH BUILD`，核对 IED/SDS/provider 自身载入、
  OAR 条件注册日志和 `OAR IED placement adapter`。后者只证明模块/版本可见，不能证明
  OAR 已创建对应条件。盾背分支另核对 provider 的 SDS 注册与真实设置。若配置被报告为
  无效/缺失条件，先确认原生注册，不能伪造工厂绕过。
- 使用现有腰/背武器分支核对左右手的原生 placement 值与比较结果；`<= 1`、`== 2` 的
  真实 false 应保留，候选应由既有 OAR 次选/基础动作处理。背包暂停后换装备再观察 IED
  hint 是否及时刷新；过期 hint 是待判定的运行时限制，当前没有合成值或强制刷新。
- 在隔离测试配置中缺失 provider 或使用未支持条件时，核对 provider/第三方拒绝原因与
  既有次选/基础动作处理，不能把它归因于 DAC 施法失败。恢复配置后重新启动核验，
  不修改原始资源或日常存档取得证据。历史手装备没有 IED 查询保证。

| 项目 | 当前证据 |
| --- | --- |
| 原生配置与 false 保留、历史作用域拒绝 | 两项本地边界用例通过；窄 fake，非游戏 IED |
| provider 工厂注册、实际 placement 求值 | 待上机 |
| 暂停时 IED 刷新、缺失 provider 回退、实际装备动作/外观 | 待上机 |

Combat-zero 投影及非战斗目标保持原边界；不宣称完整 Smooth/IED/SDS 包兼容。

## 10. 固定基础动作索引预热（2026-10-21，待上机）

前面各轮保留历史结果。本轮改为从 `action.h` 既有别名派生有限 idle/equip/unequip 集合，
覆盖十一类武器及 MT_Idle 校准/回退，不硬编码数字索引。每个已就绪项目只借用名称表扫描一遍，
复制有限匹配项并记录缺失，不再缓存 OAR 扩展全表或为每个菜单会话复制全表。
OAR 原生条件、变体/历史与真实绑定索引保持原有选择；去重别名按实际选中索引读绑定名。

预热在非暂停、非背包的来源第三人称图观察时进行，要求已发布图/behavior/骨架及覆盖名称表的
binding set；单纯 DLL 载入或 OAR 项目登记不算就绪。LoadingMenu 期间不观察旧图；世界释放
清空缓存，名称表 owner/地址/数量变化重新解析。未就绪只重试元数据，不求值条件或调用
DAC/IED；若到开包才就绪，允许同一有限解析的 `menu-fallback`，没有等待循环。
原有 DAC 脚本附件、IED/SDS 边界与限制不变。

本地仅运行 [两项固定缓存用例](build.md#fixed-base-index-verification)，覆盖 15,000 个替换名称
中的原始动作优先级/共享复用，以及项目/表/读档失效、缺失项和选中索引名称。
它们没有运行 Skyrim；完整 retained graph/math driver 未执行。

沿用现有两类上机核验，追加首次开包的日志时序观察（尚未执行）：

- 保留实际 DLL/PDB 与 `SCENEGRAPH BUILD`。进入存档后、首次开包前查找
  `OAR bases stage=world-prewarm project=... table=... requested=... resolved=... missing=... elapsed_ms=...`。
  核对 requested/resolved 是有限基础动作集合，table 是引擎全表数量；同一表后续开包不应重复初始化。
  如仅出现 `stage=menu-fallback`，记录图/表何时就绪，不能仅凭 DLL 已加载判断预热失败原因。
- 关包重开、读档或图/表更换后核对复用/重新初始化；确认换装、左右手、校准、缺失动作回退与
  OAR 真实选片保持原表现。原始图与装备保持只读，标记事务仍按既有 DAC 规则恢复。
- 分别记录首次整个菜单卡住的时长与预热耗时。DAC 文件预检、人物 Clone/审计、动画资源和 GPU
  首次使用成本未修改，不能把初始化日志或本地构建通过当作“卡顿已消失”的游戏证据。

| 项目 | 当前证据 |
| --- | --- |
| 有限解析、优先级、复用、失效与选中索引名称 | 两项本地生产策略用例通过 |
| 真实来源项目发布/预热时机、原生选片和读档生命周期 | 待上机 |
| 首次菜单卡顿实际改善及剩余成本占比 | 未测量 |

规范与所有权见 [动作缓存](animation.md#fixed-base-indices-and-project-readiness) 与
[OAR 载入边界](oar-hooks.md#fixed-base-metadata-2026-10-21)。

## 11. 本次构建与部署身份（2026-10-21，待上机）

覆盖 §8（脚本附件预检）、§9（IED placement）、§10（固定基础索引预热）三项待上机判据，
以及 T01–T12 回归；尚未上机。

| 项目 | 值 |
| --- | --- |
| 源码 | `oar-idle3` @ `7847e70`（工作树干净；`log-archive/` 通过 `.git/info/exclude` 本地排除） |
| 构建身份 | `CharacterPanel-scenegraph-scenegraph-7847e70152-cl94faaed0c6-20261021T134044Z` |
| DLL SHA-256 | `3A8B1F035F504F5C1E7CDD4D3C84B221345E58E9A8B0E20B464B60A72F0D9BB8` |
| PDB SHA-256 | `9FC79509BA5BE2B2C3CED685E6A4DB278667765A557A41DB0940A05B61A86497` |
| 部署 | `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\`（DLL+PDB 已同步校验） |
| 旧构建备份 | `dist\deploy-backup-20261020-5d5c510a\`（round-3 `FF33FE8C…`） |
| 本地回归 | 四项直编译用例 8/8 通过（dac_script_attachment / animation_controller / ied_condition_adapter / base_animation_cache），见 [构建说明](build.md#verification-and-limits) |
| INI | `UnarmedIdle=non_combat_loop`（默认，`mods` 侧 `SKSE\Plugins\CharacterPanel.ini`） |

## 12. 三项修复（2026-10-21 23:13，待上机）

2026-10-21 晚间上机判读（第 11 节构建）得出三个根因并修复：

1. **PRESET 条件支持（弩/火把选片）**：Gunslicer Crossbow 分支与 Malignis/Ashe
   `Idle_Torch` 分支都挂在 OAR 命名预设后（`Gunslicer_Base` = IsFemale+IsActorBase；
   `Malignis_Base` = OR(IsFemale/IsPlayerTeammate/非 IsChild)），面板审计曾把 kPreset
   组件一概拒绝 → 候选出局 → 弩落原版 `crossbow_idleheld`。现按 kMulti 同样遍历
   `ConditionPresetComponent::ForEachCondition` 的展开条件并逐项审计；kCustom 仍拒绝。
2. **火把独立姿态**：世界收鞘+火把时火把 idle 变体挂在 mt_idle 族（`IsEquippedType 18,
   Left hand=true` 条件），面板此前把"单手+火把"归入 1h → 永远查不到。现火把在任一手
   即 `form=kTorch`（优先于单手），idle 基改为 `MT_Idle` 优先、`TOR_Idle` 兜底；
   盾/法术仍算单手（未动）。
3. **Start→Loop 标记判据对齐**：`idle_goal_ready` 原要求"off 关键词无效果在场"，
   而 `mag_loop off`(0x806) 与 `mag_loop on`(0x802) 共享关键词 0x812（且 off 为常驻
   kRecover），世界残留会拉死判据 → 17/17 超时。OAR Loop 分支本身只查
   `HasMagicEffect(0x802)`，现就绪只要求 on 标记在场（非 dispelled/kInactive），
   失败时打 `Smooth_loop goal markers diagnostic=` 行（on 缺席/inactive、off 持有者）。

| 项目 | 值 |
| --- | --- |
| 源码 | `oar-idle3` @ `7847e70`+dirty（PRESET 遍历、火把姿态、标记判据三处修改，未提交） |
| 构建身份 | `CharacterPanel-scenegraph-scenegraph-7847e70152+dirty-cl94faaed0c6-20261021T151249Z` |
| DLL SHA-256 | `5211814C6EEF1918CA9F68AB3BB6EAF3458239EB98AE586BD2207A80BE3D65C8` |
| PDB SHA-256 | `D635267D7DD23BBA5CCFC4DB261D723F2D8C93FC1F9389FE537C1F9B371EE1C6` |
| 部署 | `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\`（DLL+PDB 已同步校验） |
| 旧构建备份 | `dist\deploy-backup-20261021-7847e70a\`（第 11 节干净构建 `3A8B1F03…`） |
| 本地回归 | 四项直编译用例 8/8 通过（含新增火把姿态断言） |

上机判据（重启游戏后）：

- **弩**：装弩开包 → `state=… path='…Gunslicer…crossbow_idleheld.hkx' replaced=true`（不再是
  `DLC01\crossbow_idleheld` + `custom condition` 诊断）；equip 过渡同理走 Gunslicer。
- **火把**：单手剑+火把（收鞘或持械皆可）→ `equipment-target=torch` →
  `goal=… family=torch` → `path='…Idle_Torch\female\_variants_mt_idle\…' replaced=true`。
- **Start→Loop**：开包等 ≥8 秒 → `Smooth_loop preparation ready`（不再是 timed out）→
  Start 后边界 `…idle Loop… replaced=true`；若仍失败，把
  `Smooth_loop goal markers diagnostic=` 行原样发回（on absent/inactive、off-holder 明细）。
- 回归：弓/1h/法术/空手/双持选片与关包 restore `result=matched` 不回退。

## 13. 探针构建（2026-10-21 23:36，待上机）

第 12 节构建的上机判读：PRESET/火把分类/准备判据三项生效（`preparation ready` 全过、
面板开启路径直达 Loop、`equipment-target=torch` 成立），但四个新问题待收口：

1. Start→Loop 仅剩换装路径：Start 播放中 `Smooth_loop` 标注施法已落地（revision 跳变），
   边界重查仍选 Start；Loop 分支唯一可能被压的条件是 `HasMagicEffect(804) negated`。
2. 火把：mt_idle 查询胜者是 SVFI 基础 idle（priority 900001 ≫ 火把分支 74995），需确认
   OAR 优先级方向与火把分支是否进入候选集。
3. 弩：Gunslicer 替换件已选中，但人物有上下位移（疑剪辑自带骨盆位移，待专门探针）。
4. 盾：盾-only 换装过渡走原版 `1hm_equip`，且有被拒的 IED 候选（unsupported IED）。

本轮加两个只读探针：goal 查询前输出 `smooth markers='…'`（源角色 Smooth 标记 local id，
`!`=inactive）；选片后输出 `oar walk base=… winner=… candidates='…'`（求值顺序全候选）。

| 项目 | 值 |
| --- | --- |
| 构建身份 | `CharacterPanel-scenegraph-scenegraph-7847e70152+dirty-cl94faaed0c6-20261021T153619Z` |
| DLL SHA-256 | `B6A61DA2991A2D46F09F079A7CF36FA19B045ADA53AEAFFC893D7F6AEC91BD47` |
| 部署 | 待游戏退出后覆盖（第 12 节构建 `5211814C…` 仍在游戏中） |

## 14. 探针判读与三项修复（2026-10-22 00:11，待上机）

探针构建上机判读（第 13 节身份，会话 23:55–00:03）：

- **火把路由正确**：`TOR_Idle`(0x63e) 基被查询，7 个火把候选在列，用户确认画面正常。
- **Start→Loop 的阻断条件找到**：装备过渡后 `weaponDraw` 施法把 **`mag_combat on`(0x3804)
  挂上**，而 Smooth 的 Loop 分支要求 `HasMagicEffect(0x804) negated`——Loop 被排除，只剩
  Start；Start 剪辑 0.1s 的 `Smooth_NonCombat` 标注驱散战斗标记（标记场翻为 0x3808）、
  2.5s 的 `Smooth_loop` 标注才挂 `mag_loop on`(0x3802)。用户测试在 2 秒内就卸下武器，
  边界重查从未同时看到"战斗关 + 循环开"。
- **盾**：Smooth 的 `1hm Shield Equip Base` 条件为 `AND(IsEquippedType(1,right),
  IsEquippedType(11,left))`——**必须剑+盾**；只有盾时世界中同样没有该分支可匹配。

本轮修复：

1. **火把+武器的 Equip/Unequip**：站立形态保持 torch，但过渡动作改按**变化的那只手**取形
   （火把变化→无过渡；武器变化→对应族过渡），不再让火把吞掉武器拔/收刀动作。
2. **弩上下位移**：站姿标定骨盆高度（`apply_standing_pose` 捕获），每帧姿态合成后把
   `NPC Pelvis [Pelv]` 的局部 Y 钳回标定值——移植剪辑的骨盆位移不再把人物带出画面。

| 项目 | 值 |
| --- | --- |
| 构建身份 | `CharacterPanel-scenegraph-scenegraph-7847e70152+dirty-cl94faaed0c6-20261021T161117Z` |
| DLL SHA-256 | `C196986DDCE60B7135580568E3408EF3925F28E1F6F68D11195572E6EDB1B8FF` |
| 部署 | `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\`（DLL+PDB 已同步） |
| 本地回归 | 四项直编译用例 8/8 通过 |

上机判读（探针仍在，日志会带 `smooth markers=` 与 `oar walk=`）：

- **Start→Loop 受控测试**：装备单手武器后**什么都不做等 ≥10 秒**；看 Start 之后边界查询
  时 `smooth markers` 是否含 `0x3802`（loop on）且不含 `0x3804`（combat on），以及是否转入
  `…idle Loop`。快速换装（<2.5s）永远到不了平滑链的循环段，这是剪辑标注时序，不是缺陷。
- **火把+武器**：持火把时拔/收单手武器，应播对应族的 Equip/Unequip；换火把本身无过渡。
- **弩**：装弩开包，人物应不再上下漂出画面。
- **盾**：只有盾时走 1HM Equip（与 Smooth 条件一致）；**剑+盾**时应播
  `Smooth Moveset - 1hm Shield\1hm Shield Equip Base`。

## 15. IED 条件放宽（2026-10-22 00:16，待上机）

用户指出盾装行为不应只对齐 Smooth：其他动作包也支持盾装，OAR 能选中就应当可用。根因是
审计器把 IED 提供方（`OpenAnimationReplacer-IEDConditionExtensions`）的**未知条件**一律
拒绝（`unsupported IED preview condition`），把使用其它 IED 条件的盾装候选挡在门外。

修改：IED 提供方的条件全部只读源角色真实装备/IED 配置，与预览原生转发的引擎谓词同类，
因此**未知 IED 条件不再排除，保留原生求值答案**；三个已知 placement/SDS 条件的 schema
校验、provider 版本校验与历史手装备拒绝保持不变。

查证：所有盾装包的替换目标都是 `1hm_equip.hkx`/`1hm_unequip.hkx`（与面板过渡基一致），
因此放宽后由 OAR 原生条件决定胜者，无需新增基名。

| 项目 | 值 |
| --- | --- |
| 构建身份 | `CharacterPanel-scenegraph-scenegraph-7847e70152+dirty-cl94faaed0c6-20261021T161634Z` |
| DLL SHA-256 | `14E07C20B50A333B47C54EFECA300B8E64BF3FA7F888E7F47DFD0964274A5FD8` |
| 部署 | `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\`（DLL+PDB 已同步） |
| 本地回归 | 四项直编译用例 8/8 通过 |

上机判读：装盾（单独或与武器一起）开包，`state=` 行应显示盾装包的替换路径（`replaced=true`）
而非 `Animations\1hm_equip.hkx`；若仍为原版，把该请求的 `oar walk` 行发回（列出全部候选与胜者）。

## 16. 骨盆钳制回退与双侧动作日志（2026-10-22 00:30）

用户反馈骨盆钳制引入了其他问题，已**完全回退**（`advance` 不再改骨盆 Y）。按用户要求，火把/
盾/Start→Loop 三项暂停修复，先加可对照的日志：

- **世界侧**（每次开包，源角色自身行为图）：`ANIMATION WORLD actor=… request=… clips=` 后接
  每个激活剪辑生成器的 `original='<animationName>' index=<绑定索引> replaced='<OAR 替换路径>'
  t=<localTime>`（未替换时 `replaced=''`）。`index` 越过原表条目且 `replaced` 非空即为 OAR
  已替换；多剪辑以空格分隔。
- **面板侧**：`state=` 行新增 `base='<原始动画名>'`（原为仅 `path='<替换路径>'`）。
- 保留既有探针：`smooth markers=`（Smooth 标记场）与 `oar walk=`（候选顺序与胜者）。

| 项目 | 值 |
| --- | --- |
| 构建身份 | `CharacterPanel-scenegraph-scenegraph-7847e70152+dirty-cl94faaed0c6-20261021T163022Z` |
| DLL SHA-256 | `74184056A474C076620502D3C0DF6DB6501B8359893F62334C885AEA196B999B` |
| 部署 | `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\`（DLL+PDB 已同步） |
| 本地回归 | 四项直编译用例 8/8 通过 |
| 遗留 | 上一轮的 IED 未知条件放宽仍在（第 15 节），日志会显示其效果 |
