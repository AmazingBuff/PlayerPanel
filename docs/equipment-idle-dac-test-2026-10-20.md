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
