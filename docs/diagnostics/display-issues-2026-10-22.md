# 面板显示问题交接（2026-10-22）

> 本轮只记录问题与日志使用方式，不做代码修改。下一会话从这里开始。
> 归档日志：[CharacterPanel-oar-probe-20261022.log](CharacterPanel-oar-probe-20261022.log)
> （会话构建 `CharacterPanel-scenegraph-scenegraph-7847e70152+dirty-cl94faaed0c6-20261021T164147Z`，
> 含全部探针输出）。

## 1. 需要修的三项显示问题（用户报告）

| # | 操作 | 现象 |
| --- | --- | --- |
| 1 | 背包内切换到 **axe** | 面板人物先**扩大**再复原（取景/缩放在装备瞬间跳变） |
| 2 | 背包内切换到 **bow** | 面板人物**手部出现异常形变** |
| 3 | 背包内切换到 **crossbow** | 面板人物**持续上下移动**（Gunslicer 移植剪辑自带骨盆/根位移） |

三项都发生在**背包内换装**（`reason='equipment'`）路径，不在开包沿。已知背景：

- 取景标定固定用原版 MT_Idle（`SCOPY FRAME calibrated offset=… radius=…`），装备过渡与
  idle 的切换点在 `handover action=2`（Equip）→ `handover action=1`（idle）。
- 第 3 项此前尝试过"骨盆 Y 钳制"，引入了其它问题，**已完全回退**；不要沿用该方案，
  应先确认位移来自骨盆轨道、根节点轨道还是装备挂点层级。
- 第 2 项的手部形变优先怀疑骨骼轨道解析（`asset ready … tracks=… resolved=… pose=…`）
  与持弓挂载（`SCOPY HELD … side=… mount=…`）。

## 2. 日志里怎么找证据

日志在 `Documents\My Games\Skyrim Special Edition\SKSE\CharacterPanel.log`（每次会话覆盖，
退出游戏后复制到本目录归档）。关键行：

| 行 | 含义 |
| --- | --- |
| `ANIMATION WORLD actor=… request=N clips=…` | **开包瞬间世界侧**所有激活剪辑：`original='<引擎动画名>' index=<绑定索引> replaced='<OAR 替换路径>' t=<播放时间>`；`replaced=''` 表示未替换 |
| `ANIMATION … request=N state=… base='<原始动画名>' path='<替换路径>' index=…` | **面板侧**选中结果：`base` 是查询的原始动画，`path` 是 OAR 替换件（未替换时为原始名） |
| `ANIMATION … request=N oar walk base=… winner=… replaced=… candidates='…'` | 该基的全部候选（按 OAR 求值顺序）与胜者，用于判断"为什么没选到某个包" |
| `ANIMATION … request=N smooth markers='…'` | 源角色身上的 Smooth 标记场（local id，`!`=inactive）：`0x3801` start on、`0x3802` loop on、`0x3804` combat on、`0x3806` loop off、`0x3808` combat off |
| `DAC … event='…' native call completed` / `native-effect` | DAC 原生施法与效果签名变化 |
| `ANIMATION … event='Smooth_loop|Smooth_NonCombat|weaponDraw' time=…` | 剪辑标注触发与施法时点 |
| `SCOPY FRAME calibrated offset=… radius=…` / `SCOPY FRAME … center=… radius=…` | 取景标定与实时包围（判断第 1 项的"扩大"是 radius 还是 scale 引起） |
| `SCOPY HELD actor=… attached=… missing=0` / `SCOPY WEAPON …` | 手持挂载（判断第 2/3 项的挂点） |
| `asset ready index=… base=… replaced=… tracks=… resolved=… pose=…` | 资产加载与轨道解析（第 2 项手部形变先看这里） |

定位方式（PowerShell）：

```powershell
$log='C:\Users\AmazingBuff\Documents\My Games\Skyrim Special Edition\SKSE\CharacterPanel.log'
Select-String -Path $log -Pattern 'transition=equip form=(axe|bow|crossbow)' -Context 0,12
Select-String -Path $log -Pattern 'request=<N> (state=|handover|asset ready|SCOPY FRAME|SCOPY HELD)'
```

## 3. 本轮已落地但尚未验证的改动（同一提交）

- 审计器遍历 OAR 命名预设（`PRESET`）的展开条件（`src/animation/oar/hooks.cpp`）。
- IED 提供方的未知条件不再拒绝，保留原生求值答案（`src/animation/oar/ied_conditions.h`）。
- 火把单独成形态、查询 `TOR_Idle`；装备/卸下过渡按**变化的那只手**取形
  （`src/animation/action.h`、`src/animation/controller.cpp`）。
- Smooth 非战斗就绪只要求 on 标记在场（对齐 OAR 条件语义），失败时输出标记诊断
  （`src/animation/dac/effects.cpp`、`src/animation/dac/session.cpp`）。
- 世界侧/面板侧选片日志、标记场日志、候选遍历日志（探针，见第 2 节）。
- **已回退**：骨盆 Y 钳制。

## 4. 建议的下一步

1. 先按第 2 节的方法读归档日志，把三个问题各自落到具体行：axe 的 `FRAME`/`radius`、
   bow 的 `asset ready`/`SCOPY HELD`、crossbow 的 `handover` 后 `FRAME center` 变化。
2. 第 1 项优先查取景标定的重入（装备过渡期间是否重复 calibrate 或 radius 用了不同基）。
3. 第 2 项优先查 bow 替换件的轨道解析与弓挂点。
4. 第 3 项先确认位移来源（骨盆 vs 根 vs 挂点）再改，不要回到钳制方案。
