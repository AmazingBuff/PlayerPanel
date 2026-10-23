# 面板显示问题交接（2026-10-22）

> 本轮只记录问题与日志使用方式，不做代码修改。下一会话从这里开始。
> 早期会话摘要：[OAR 探针与当前证据的区别](../history-animation.md#recent)；原日志 `CharacterPanel-oar-probe-20261022.log` 从[来源索引](../history-sources.md)恢复。
> （会话构建 `CharacterPanel-scenegraph-scenegraph-7847e70152+dirty-cl94faaed0c6-20261021T164147Z`，
> 原件含该轮全部探针输出，已归并总结并可从固定 Git 基线恢复）。

> 本交接保留前几版测试说明；更早报告/日志已归并为[历史总结](../history.md)，原件见[来源索引](../history-sources.md)；当前关联诊断、POSE 前缀与请求总预算见[第 8 节](#8-2026-10-22-已报告问题的关联诊断)，基础快速映射格式见第 7 节。
> **当前入口是[第 10 节](#10-2026-10-23-装备上下文与火把分层修复待上机)**。[第 9 节](#9-2026-10-22-换装片源与站立谓词修复)保留此前的换装片源/站立谓词修复、两场会话（`r1` `5ec8fcf99f`、`r2` `3617bf1f27`）和纯诊断包 `r3`（`a46e724`，尚无会话）的历史判读；r1/r2 原始日志仍保留原处。

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

日志文件是运行时 `SKSE::log::log_directory()` 目录中的 `CharacterPanel.log`（每次会话覆盖，
退出游戏后复制到本目录归档）。先确认实际运行时路径，不猜某台机器的用户目录。关键行：

| 行 | 含义 |
| --- | --- |
| `ANIMATION WORLD actor=… request=N clips=…` | 历史构建的**开包瞬间世界侧**激活剪辑：`original='<引擎动画名>' index=<绑定索引> replaced='<OAR 替换路径>' t=<播放时间>`；旧 `replaced=''` 只代表未解析出逻辑替换路径，**不能证明原版播放**。当前构建的实际绑定、官方 API 与 paused-source 字段见第 7 节；第 6 节保留上一测试包格式 |
| `ANIMATION … request=N state=… base='<原始动画名>' path='<替换路径>' index=…` | **面板侧**选中结果：`base` 是查询的原始动画，`path` 是 OAR 替换件（未替换时为原始名） |
| `ANIMATION … request=N oar walk base=… winner=… replaced=… candidates='…'` | 该基的全部候选（按 OAR 求值顺序）与胜者，用于判断"为什么没选到某个包" |
| `ANIMATION … request=N smooth markers='…'` | 源角色身上的 Smooth 标记场（旧文编号含轻插件索引，非规范 local id；见第 10 节，`!`=inactive）：`0x3801` start on、`0x3802` loop on、`0x3804` combat on、`0x3806` loop off、`0x3808` combat off |
| `DAC … event='…' native call completed` / `native-effect` | DAC 原生施法与效果签名变化 |
| `ANIMATION … event='Smooth_loop|Smooth_NonCombat|weaponDraw' time=…` | 剪辑标注触发与施法时点 |
| `SCOPY FRAME calibrated offset=… radius=…` / `SCOPY FRAME … center=… radius=…` | 取景标定与实时包围（判断第 1 项的"扩大"是 radius 还是 scale 引起） |
| `SCOPY HELD actor=… attached=… missing=0` / `SCOPY WEAPON …` | 手持挂载（判断第 2/3 项的挂点） |
| `asset ready index=… base=… replaced=… tracks=… resolved=… pose=…` | 资产加载与轨道解析（第 2 项手部形变先看这里） |

定位方式（PowerShell）：

```powershell
$log = '<runtime-SKSE-log-directory>/CharacterPanel.log' # Replace with the actual runtime log path.
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

## 5. 2026-10-22 实现跟进与待上机标准

用户后续明确：**axe 只在刚装备时发生一次扩大再复原**，不是每两秒重复；bow、crossbow 本轮暂缓。以上原始问题、猜测与归档日志保留为历史，不能把当时的推测当作根因。

### 已确认的事件预检缺陷

归档日志共有五次 `Smooth_Loop` 标注（axe/bow），没有一次同拼写的 `native call completed`。`event_scope` 原来把 `std::string` 与标注精确比较，而原生 `BSFixedString` 的 string-view 比较采用等长加 `_strnicmp`。本次让 `event_scope`、`smooth_idle_goal` 共用同样的大小写无关比较；原始标注拼写仍直接交给原生 DAC。所有同名大小写变体都要走整事件安全检查，任何不安全规则仍拒绝整个事件。未修改 DAC 二进制、规则或 HKX。

离线仅跑 [dac_event_match_test.cpp](../../tests/dac_event_match_test.cpp) 两例：`Smooth_Loop` 匹配 `Smooth_loop`，`Smooth_Loop_off` 不匹配。它们执行生产比较器；整事件拒绝、原样传递与请求/菜单保护经代码审查保留，**没有离线执行引擎预检或原生施法**。上机须分别确认调用完成、实际标记变化与 Loop 胜者，不能只看一条调用日志。

### Smooth axe 原始归档的离线事实

输入是用户 Downloads 的 `Smooth Moveset - OAR-149713-1-5-1756381509.zip`；只读分析 `Smooth Moveset - 2hw Axe` 内实际选中的 Equip、非战斗 Start、非战斗 Loop，未改写源文件。

| 剪辑 | 原始 HKX SHA-256 |
| --- | --- |
| `2hw Axe Equip Base/2hw_equip.hkx` | `ac478c26fd966b876a49223d8e23d66f9f72092e64d155e022e481e414cd945d` |
| `2hw Axe Non-Combat idle Start/2hw_idle.hkx` | `5ba8c0d4029acca6415ef85bcab29a51cb094327f999a3a9e72ed78ef030db12` |
| `2hw Axe Non-Combat idle Loop/2hw_idle.hkx` | `68383131d5de52b7452b62113cf00bbc7848465b941142d9305242e9f15466a2` |

- 三个剪辑均有 99 条变换轨，所有 scale mask 都是 identity，前四轨也都是 identity。不能据此说所有轨道平移为零，更不能据此证明运行时骨骼映射正确。
- track 4 有显著原始平移。Equip 的范围为 X `[-0.804576, 5.323094]`、Y `[-20.519398, -1.990397]`、Z `[45.883026, 67.648140]`；Start 为 X `[-4.457511, 0.293181]`、Y `[-10.350513, 2.720211]`、Z `[53.552467, 67.373474]`，均为源局部坐标单位。
- 三个文件的映射表为空，`originalSkeletonName` 为 `NPC Root [Root]`。**源 track 4、43 的语义骨名仍未知**；不能把它们直接称为骨盆、COM 或 Weapon。当前加载器继续使用原有 `identity-unverified` 回退，本轮只记录它实际采用的目标骨索引/名称。
- 平移通道的端点/中点采样中，只有 track 4、43 的变化超过 `0.05`；其余轨道的局部平移在三剪辑间近乎相同。解析消耗 Equip `19856/19856`、Start `9792/9792`、Loop `12936/12944` 字节（Loop 尾部有对齐填充）。这不是完整源骨架或渲染验证。
- Equip 末端与 Start 起点的 track 4 平移在约 `0.0003` 内衔接，Start 末端与 Loop 起点也如此。Start 自己从末端回到开头时却约跳变 `(-0.4045, -6.2652, -13.8210)`。旧日志的 Start 每两秒重复标注与丢失 Loop 标记一致；这是独立的阶段问题，**不能用来解释用户澄清的一次性装备闪变**。

### 新探针怎么读

历史第 2 节的 `FRAME` 表述需更正：`SCOPY FRAME` 是捕获/标定时的证据，并非逐帧包围时间序列。旧 request 6 的标定半径 `28.2`、offset `(1.5,-2.4,-0.2)` 与邻近请求一致；已有取景代码固定 scale `0.35`，标定只修正平移。这些都不足以排除首帧投影变化。

request 6 首次 Equip handover 是 `00:46:40.992`，`00:46:41.074` 又发生 graph retarget（约 82 ms 后，剪辑时钟 `0.05`）。重点比较这段换图、标定和混合，不预先认定是根缩放、原始平移或相机变化。

[控制器探针说明](../animation.md#bounded-axe-pose-and-projection-evidence-2026-10-22)定义精确字段：仅已接受的 axe 剪辑在 handover/retarget 启动新 probe，立即记录图指针和根 local/world；之后最多十二次，在 `0/.016/.033/.066/.100/.200/.500/1/1.5/2/3/4` 秒附近记录，4.1 秒后停止，漏过的时间槽不补打。兼容刷新或循环边界不重置探针。

`AXE MAP` 报告 track 0–4/43 与关键身体目标节点的真实索引/骨名/复制节点；`AXE POSE` 给出 clip-time、wraps、当前/退出剪辑和混合权重；`AXE NODE` 分别给出输入采样 local、最终 local/world 以及有效的引擎投影。平移为游戏坐标单位、缩放为比率、旋转为 x/y/z/w 四元数。`AXE CAMERA` 给出 UI3D 相机指针、world、原始 frustum 和 port 宽高；`screen-port-xyz` 的 z 是引擎投影返回值，**不是线性深度**，x/y 也不是最终合成像素。缺节点或无效投影明确输出 missing/unavailable。不要从矩阵某行猜深度方向。

### 待上机验收（axe 视觉仍 UNVERIFIED）

1. 使用同一构建身份的本地 Release DLL/PDB，自行在测试环境安装后记录完整日志；本轮只生成本地文件，没有部署。
2. 在背包内换 axe，保留第一帧到四秒的日志，尤其 handover 与每次 retarget 周围的 `AXE PROBE/POSE/NODE/CAMERA/MAP`、捕获和 `FRAME calibrated`。用 actor/request/probe/graph 指针和时间对齐；开包直接 idle 与换装路径不要混为一组。
3. 比较扩大前后的根 local/world scale、输入采样与最终身体变换、混合进度、相机/frustum、头脚投影间距。确认实际 track 4/43 目标；identity 回退的日志只说明采用了什么映射，不能证明源顺序正确。若闪变未被采到，不能宣称根因已排除。
4. 确认原样 `Smooth_Loop` 到达原生 DAC、loop-on 实际生效、OAR 从 Start 选到真实 Loop；关包还需 `DAC restore ... result=matched`。调用完成本身不是效果或视觉成功。
5. 记录一次性扩大是否仍发生，附对应画面/时间与完整日志后再决定视觉修正。本轮没有改采样、骨骼映射、姿态、缩放、放置、相机或资源，没有根/骨盆钳制；bow/crossbow 和内存退休仍在本轮范围外。

## 6. 2026-10-22 torch/shield 查询跟进

用户追加报告 torch/shield 的世界与面板差异，并明确 **shield 差异在装备/卸下过程，不是站姿**。前一节的 DAC 大小写修复、axe 限定探针和测试包保留；bow/crossbow 仍暂缓。

### 两处已确认的窄缺陷

- 旧 request 34/35/37/38/39/40/41 的 TOR_Idle 候选被 `CurrentWeatherHasFlag` 拒绝，退到原版索引 `1598`。该 core 条件只读当前天气标志；现在通过既有 native source forwarding 求值，保留 false/negation，不强制天气条件成立。仍可能因其它原生条件选不到某个 torch 包。
- 保留右手武器、只卸下左手盾牌时，旧逻辑用 `target.empty()` 判定方向，目标仍非空便误选 Equip。现在仅在一手武器形态、右手不变、实际左盾从空到有/从有到空时纠正方向；卸盾选 `1HM_Unequip`，装备盾仍是 Equip。旧 torch-hand 与其它形态策略保留。
- 卸盾通过既有 `hand_override` 向三种 core 装备条件提供先前手中 form ID；同一个盾牌动作在加载中遇到 effect revision 改变时，保留原 action/preview 再查询。后续 idle 不继承历史手部。**历史 IED 仍拒绝**，因此需要旧 IED 放置提示的卸盾分支仍可能退到基本 Unequip，不能宣称全包动作一致。

### 旧日志不能证明的事

归档没有 world torch 快照。world request 3 的 `9037`、request 44 的 `10985` 与面板已知替换索引相同，旧 `replaced=''` 不能据此判原版。request 42 曾成功选中 Smooth `1hm Shield Equip Base` 索引 `8905`，随后 shield Start `9044`，不能笼统归因为 IED 一概不可用。并行的 `DW_ShieldAdjustment` 只有激活节点记录，没有权重/掩码，不能据此证明面板缺少有效层，更不能直接补一层。

### 新日志与上机步骤（视觉仍 UNVERIFIED）

[OAR 跟进说明](../oar-hooks.md#torchshield-query-follow-up-2026-10-22)与[动作策略](../animation.md#torch-weather-and-left-shield-transitions-2026-10-22)定义字段与边界。

1. 使用新的独立 torch-shield 本地测试包；旧 `smooth-loop-axe-test-r2` 保留未覆盖。本轮没有部署、提交或推送。
2. 背包内装备 torch，记录选片、`OAR QUERY` 与 WORLD 行；确认不再因 `CurrentWeatherHasFlag` unsupported 被拒。真正天气条件为 false 时不应强行通过。世界快照是 `snapshot=paused-source`，附 reason、当前双手和真实 drawn；它不是已完成换装后的稳定世界动作。
3. 保持同一右手一手武器，先装备左盾，再单独卸盾。确认分别 `transition=equip query-hands=current` 与 `transition=unequip query-hands=previous`，卸盾基名为 `1HM_Unequip`。对照完整动作与加载/效果修订日志，确认没有提前被 idle 替换；原生效果时序仍需上机证据。
4. 每个 WORLD clip 都保留 `original/index/t` 并新增 `original-index/bound/lookup`。`bound` 是当前绑定索引的实际资产；`replaced` 是另行解析的逻辑路径。`lookup=base` 才表示正面匹配到原始索引，`replacement` 为无歧义解析，`unresolved` 或 `unresolved-ambiguous` 保留真实绑定但不猜结论。比较画面时也须对齐阶段和时间。
5. `OAR QUERY rejected-candidate` 给出完整候选路径与 provider/condition 拒绝原因，每查询为 unsupported/metadata 保留八条，普通原生 false 另有两条额度，之后最多一行报各自省略数量。普通 false 不会挤掉后面的不支持详情。`first-query-rejection` 可能属于较早失败的候选，不能当成最终胜者的失败原因。原生 false 另标为 native conditions/eligibility false，不能误读为不支持。
6. 若盾牌/火把画面仍不一致，保存完整日志与世界/面板阶段对应的画面，再讨论下一步。原生天气/IED 求值、卸盾期间历史选择、暂停状态时序及世界/面板视觉一致性都尚未验证；本次没有改挂载、左右臂掩码、叠层图、资产、相机或源角色。

本轮只运行 [torch_shield_policy_test.cpp](../../tests/torch_shield_policy_test.cpp) 两个 engine-free case：天气走源读取且无常量投影、保留右武器的左盾装卸与历史查询上下文。不执行原生天气/IED、加载刷新时序或游戏视觉；先前 DAC/axe/IED/cache driver 不重跑。

## 7. 2026-10-22 低开销映射日志跟进

本节替代上一 torch-shield 测试包的全表逆向查名及 `original-index/lookup/replaced` 日志格式；前文原始证据和两个旧测试包保留。旧归档 request 1/2 从请求到 WORLD 构造结束约 `7295/7284 ms`，request 3/44 三个剪辑约 `22040/22133 ms`，符合约每剪辑 7.3 秒的日志构造成本。这定位到一项重大开销，不是完整 profiler 或 FPS 测量。

### 当前格式与开销边界

- WORLD 在开包/换装请求时对真实世界 active clip 调用官方 OAR Animations V1 API；只在既有 3.1.6/3.2.1 profile 安装成功后动态请求一次可选接口。保留原始 `animationName` 为 `original`、当前索引和直接读取的 `bound`。不逆扫动画名表、不枚举全部替换、不建立全表缓存，不推进世界图或重新求值条件。
- `oar-api=available oar-result=replacement` 是正面的当前替换信息；`empty-unknown`、`unavailable-unknown` 明确表示未知。OAR 返回的 `oar-path/project/variant/submod/mod` 分开记录，不拼出一个声称唯一的别名。空响应不能证明原版；实际绑定名也可能是 hash 去重别名。面板 PreviewClip 不注册到此 API。
- 默认面板日志为 `ANIMATION PANEL ... accepted`：加载/回退完成后的最终 handover 或已接受的同资产复用才发布。含原始基、选中逻辑路径、variant index/position 或 none、实际 index/bound、base/replacement 状态。未加载/失败 proposal 不是播放记录。相同请求中重复选中相同映射不重复打印，新请求或有效映射变化仍打印。
- 移除全部候选复制和 `oar walk`，即便详细日志开启也不恢复。实际选择/历史需要的当前候选与 variant 元数据保留，有限原始动作预热不变。API 可能扫描当前替换的 variant 并复制若干字符串；不是无成本或全局 O(1) 保证。
- `build-ms` 是日志调用前、源线程内的快照/映射构造耗时，不含日志磁盘写入/flush、总开包延迟或后续渲染，不能当作 FPS。WORLD 仍是 `paused-source`，带 reason/current hands/真实 drawn，不是换装完成后的稳定世界动画。

### 新包测试步骤（性能与视觉仍 UNVERIFIED）

1. 使用新的独立 `animation-map-test-r1` 包，核对构建身份。旧 `smooth-loop-axe-test-r2`、`torch-shield-test-r1` 不覆盖。本轮没有部署、暂存、提交或推送。
2. 常规测试先保持 `[Diagnostics] DetailedAnimationTrace=false`，重启游戏。开包并依次做需要比较的 torch 装备、右武器不变的左盾装备/卸下；退出后保存完整日志。普通 info 约每秒 flush，警告/错误立即 flush，实时看文件可能延迟一小段；flush 线程只处理 sink，没有引擎工作。
3. 对齐 actor/request/reason、WORLD 的 source clip 时间及真实手部状态、PANEL accepted 的 action/clip-time。检查原始基、OAR 返回逻辑信息、当前绑定与接受后的面板 variant；不要把空 API、共享绑定或暂停世界的不同阶段误判为原版/错片。
4. 比较 WORLD `build-ms` 与上述旧构造间隙，但另行记录体感开包等待/画面卡顿；仅凭 build-ms 不能宣称总卡顿已消失或 FPS 改善。等待资产时没有 PANEL accepted 新行是正常的；回退后须显示实际已接受基/绑定，相同请求重复 loop 不应刷相同映射，新请求应重新发布。
5. 只有需要第 5/6 节的 axe 或候选细节时，将 `DetailedAnimationTrace=true` 后重启再复现。此时启用限定 axe pose/map、标注 info、Smooth marker 构造、限定拒绝候选、proposal 路径和捕获 geometry 列表；实际 DAC 分发、条件、隔离审计、警告/错误不受开关影响。axe 仍按原十二槽/4.1 秒上限；unsupported/metadata 与原生 false 仍各自八/两条额度。
6. 若 API 不可用/空、盾牌历史 IED 回退或视觉仍不同，保留事实状态和画面/完整日志后再判断。原生 API 调用、运行时性能、torch/shield/axe 视觉一致性均尚未验证；不据此追加叠层、根/骨盆钳制、资产或相机修正。

本轮只运行 [animation_mapping_test.cpp](../../tests/animation_mapping_test.cpp) 两例：接受后的同绑定不同逻辑别名/新请求发布、相同 loop 合并；五万条表中只读取选中名称一次。旧测试不重跑。详见[当前映射说明](../animation.md#accepted-mappings-and-world-snapshots)、[官方 API 与时序](../oar-hooks.md#low-overhead-mapping-and-opt-in-traces-2026-10-22)和[配置样本](../../config/CharacterPanel.ini)。

## 8. 2026-10-22 已报告问题的关联诊断

本节新增诊断，不宣称视觉根因或修复已证实。前三个测试包与历史日志保留；正常 PANEL/WORLD 快速映射、默认关闭的 DetailedAnimationTrace 和约每秒 info flush 均不变。

### 设置、限额与关联 ID

使用新的独立 `issue-trace-test-r1` 包，先核对构建身份。在 `[Diagnostics]` 设置 `DetailedAnimationTrace=true` 后重启。关闭/缺省时仍输出权威 accepted PANEL/WORLD 和必要审计，新增名字匹配、格式化、骨骼/投影、成功挂载细节均跳过；实际分发/条件/隔离检查不受开关影响。info 可能约一秒后才在文件中可见，警告/错误立即 flush；日志 timer 不执行引擎工作。

`request` 表示最新请求，`accepted-request` 表示当前仍显示的接受剪辑所属代，`asset-request` 表示 loader 最初创建请求。新请求加载期间旧剪辑仍可见，不能把三者混为一谈。用 actor/request、probe epoch、copy graph 指针关联捕获 ID；源 snapshot 明确为 paused-source，可能与面板不同阶段。

原 `AXE PROBE/MAP/POSE/CAMERA/NODE` 迁移为 `POSE PROBE/MAP/SAMPLE/CAMERA/NODE`，附 profile。axe、torch/盾牌手臂、火把旁武器装卸及其移除后的单手 idle、generic bow/crossbow 共用同一预算：**每请求最多四个窗口、二十四次姿态快照**。每窗口仍是 `0/.016/.033/.066/.1/.2/.5/1/1.5/2/3/4` 秒的十二槽，4.1 秒过期，漏槽不补打。限额跨 handover/retarget 保留，只随新请求复位；相同接受 loop 不重新开启窗口。拒绝开启/离开支持 profile 会停止旧窗口，避免用旧时间/epoch 记录新图或新剪辑；每请求最多一条预算耗尽通知。

身体固定请求十五个实际名字：原 root/COM/pelvis/spine/head/feet 七个，加左右 Clavicle `[LClv]/[RClv]`、UpperArm `[LUar]/[RUar]`、Forearm `[LLar]/[RLar]`、Hand `[LHnd]/[RHnd]` 八个。缺失明确打印；target skeleton 来自实际 asset 保留的图并检查索引，不从较新的 pending 图猜骨名。identity-unverified 仍未知源语义，track 4/43 不命名为某个身体/装备骨。POSE 的输入 sampled local、最终 local/world 和 camera/projection 延续原单位；z 投影不是线性深度。

`SCOPY SOURCE-SNAPSHOT/SOURCE-NODE` 每请求最多一次，复用既有捕获 transform pass 保存的十五骨源 local/world/parent，不新增遍历。缺节点为 missing，实际无名对象为 `<unnamed>`，无父节点为 `<none>`。它发生在 Clone 前，不是稳定世界姿态或所有骨骼同时采样的证明。

### 每个问题按哪条证据链读

| 症状 | 同请求优先对齐的实际记录 | 不能据此推断 |
| --- | --- | --- |
| torch/盾牌世界与面板分支不同 | WORLD 实际绑定/OAR 字段与可选 node/mode/flags/speed/at-end → ANIMATION QUERY 的 bases、源/投影 hands/drawn、action/reason/revision → OAR QUERY 拒绝 → PANEL accepted | 空 API 不是原版；激活节点不是有效叠层/权重 |
| 左盾 Equip/Unequip 错时或中途消失 | QUERY → DECISION pending-choice/reuse/refresh/cancel/waiting-goal → LOAD target/standing/calibration 与 asset-request/queue-to-ready → REBIND → accepted → HANDOFF → MOUNT-MAP/MOUNT-SWITCH | load ready 不等于正在显示；不能凭旧 accepted 代说新请求已播放 |
| Smooth Start 不进 Loop | ANNOTATION 的 dispatch/direct 决定 → DAC TRACE queued → skipped 或 native completed → DAC OBSERVED 实际 markers/revision → EFFECT-REV/PENDING-REV → 下一次 QUERY/PANEL accepted | native completed 不是标记就绪；revision 变化不证明一定由某一事件造成 |
| 刚装备 axe 一次扩大再复原 | CAPTURE-DUE/CAPTURE 的实际原因、old/new graph → SOURCE-SNAPSHOT → GRAPH/PLACEMENT/CALIBRATION 的原有 anchor/offset/radius/root → HANDOFF/MOUNT-SWITCH → 最早 POSE slots 和投影 | 重复 Start 不是已证实闪变原因；缺样/耗尽不能排除某原因 |
| generic bow/crossbow 手臂/挂点 | 同一 POSE arm/source/mount 路径；保存实际所选绑定、local/world、父节点 | 本轮未取得/分析缺失 HKX，也不修资源或猜源骨顺序 |

OAR root-false 只记录原有一次 Evaluate 的 **ROOT EvaluateAll 顶层首个 false**，带 name/provider/negated。OR/TARGET 等只说明该顶层条件实际 false，不猜内部哪个叶子是最终原因；未观察到 root false 时明确说 native eligibility/other cause unknown。metadata 只在既有两条 native-false 额度内采集，unsupported/metadata 八条额度独立，保留省略汇总。不额外 Evaluate/GetCurrent/比较源答案、不改变 supported/false/negation。

DECISION 只记录实际状态变化/决定，waiting 不逐 poll 刷屏。PENDING-REV 使用原有读取的 proposal/observed/current 值；历史盾牌 override 不泄漏至后续 idle。HANDOFF 报现有阈值来自首个 weaponDraw/weaponSheathe 还是 duration-midpoint，不改时间。DAC skipped 标签可为诚实的组合 gate；没有重入锁内 session_epoch/marker 读取，没有新增任务/施法，routine observe 失败保持安静以免逐帧刷屏。

### 上机与验证边界

1. 每次只复现一类问题：先保留右武器装/卸左盾，再 torch 及其旁边武器装卸，最后 axe 刚装备闪变。保存完整日志与操作/画面时刻，至少覆盖请求、加载接受和最早 pose 窗口；无需调整动画/相机/根骨。
2. 核对请求/接受代、graph/capture、相位/时间后再比较源与面板。检查窗口是否耗尽、节点/投影是否缺失；不要把不同坐标背景或暂停世界的不同阶段当作同一状态。
3. 确认关包仍有原有 `DAC restore ... result=matched|MISMATCH`，但恢复是否对世界可见仍需实际观察。若仍不同，提交原始日志/画面后再决定修复；本轮没有新增层/掩码、条件强制、源写入、资源或取景修正。
4. 正常使用可恢复 `DetailedAnimationTrace=false` 并重启。前三包不覆盖，没有部署、暂存、提交或推送。

本轮仅跑 [diagnostic_trace_test.cpp](../../tests/diagnostic_trace_test.cpp) 一例 engine-free 请求预算行为，覆盖重复开窗、四窗口/二十四快照上限、同请求不复位、新请求复位、拒绝停止旧窗、漏槽和过期。原生条件/UI 任务/加载/挂载/投影/性能/视觉全部 **UNVERIFIED**。规范入口：[动作与预算](../animation.md#correlating-reported-issues)、[顶层条件观测](../oar-hooks.md#correlated-rejection-evidence-2026-10-22)、[DAC 证据](../dac.md#opt-in-annotation-and-marker-evidence)、[捕获/放置/挂载](../scene-graph.md#opt-in-capture-placement-and-mount-evidence)。

## 9. 2026-10-22 换装片源与站立谓词修复

第 8 节诊断包（`issue-trace-test-r1`，构建 `5ec8fcf99f`）上机回传后，用户按现象确认：axe 是 Equip 动作中的"位置拉近"；torch 与世界的持火把动作不一致；shield 的装卸用了一手武器原始动作；bow/crossbow 同类。归档日志 [CharacterPanel-issue-trace-r1-20261022-2231.log](CharacterPanel-issue-trace-r1-20261022-2231.log)（17 请求、17 捕获、5 次 `DAC restore … matched`、无崩溃）。

### 日志能证明的三件事

1. **axe 装卸片源不是 Smooth 的斧动作**：`request=15` 接受 `Gunslicer Animations OAR\Sword\1hm_equip.hkx`（index 8908，优先级 800002），而 Smooth 的 `Axe Equip/Non-combat Equip Base`（83030320/83030324）未被选中；候选拒绝日志的旧额度（8 条 unsupported、2 条 native-false）被 RaySense 封面动画占满，斧装动作的拒绝原因没有出现。
2. **torch 没有装卸动作**：`request=9 transition base unavailable form=none; idle only`（`panel_transition_bases` 对 torch 返回空），面板直接跳 `tor_idle`（base 1598）；世界同一时刻同时有 `MT_Idle`（含 SVFI 替换）与 `Tor_Idle(1598)` 两层，此外 `Malignis Animations\Idle_Torch` 替换的是 female `mt_idle`，显示端从不查询。候选方面：2 个 SDB 灯笼包因 `IsInInterior` 取反后为 false（室内，正确），5 个 Ashe 包因 `core::WindAngleDifference` 不支持被拒。
3. **shield 选中战斗变体**：装卸方向正确（`query-hands=current/previous`），但接受 `1hm Shield Equip Base`（8940）与 `Sword_and_Shield` 卸盾（9503），而非 `1hm Shield Non-Combat Equip Base`。同请求标记为 `243801,243804,243806,…`（无 loop 标记 243802）。

### 当时的谓词判断与修改（方向语义错误，已由第 10 节纠正）

Smooth 的非战斗变体统一带 `OR( HasMagicEffect(Smooth 0x802), NOT IsMovementDirection(0) )`。旧实现在 [hooks.cpp](../../src/animation/oar/hooks.cpp) 里用 `INumericConditionComponent::GetNumericValue(source) == 0` 回答 `IsMovementDirection`——该组件装的是**请求方向参数**（`Direction: 0.0`），不是角色运动，因此恒为真，`NOT …` 恒假；换装瞬间 loop 标记尚未施法，OR 整体为假，非战斗装卸变体全部被拒，只能落到战斗变体或其它包的动作。修复把 `IsMovementDirection` 改为 [policy.h](../../src/animation/oar/policy.h) 的站立谓词（站立图在任何方向都不移动 → false）；`IsCombatState` 保留数值读取（其组件即请求状态，站立图在状态 0）。

同一轮把 `OAR QUERY` 候选证据从"前 8/2 条"改为**按不同原因去重**（首见 provider/condition/root-false 签名，至多 24 条）+ 一条省略计数，避免单一包的重复条件挤掉后续文件夹的原因。

本轮只跑一个 engine-free 站立谓词检查（`IsMovementDirection` 对站立/drawn 均 false、`NOT` 分支为真、无关谓词不变），源码审查确认 `IsCombatState` 语义；保留的 [snapshot_transform_test.cpp](../../tests/snapshot_transform_test.cpp) 用例已同步断言但本轮未执行该驱动。原生条件、OAR 选片、加载、挂载、投影、视觉与性能仍 **UNVERIFIED**。

### r2 包的上机步骤（`DetailedAnimationTrace=true`，已完成）

1. 保留右武器，装/卸左盾，确认 `ANIMATION PANEL` 的 `logical` 落在 `1hm Shield Non-Combat Equip Base` / 对应卸盾，而不是战斗变体。
2. 装 axe（单手斧），确认 Equip 片源是 `Smooth Moveset - Axe` 的装卸动作；`OAR QUERY` 现在应为每类原因至少一行。
3. torch 姿态本轮**未改**：仍走 `TOR_Idle` 单层，等下一包改为“武器形态 + 火把层”后再验。
4. bow/crossbow 待同一修复的下一场会话验证；axe Start→Loop 不切换（`Smooth_loop` 已施法、边界重查仍 `accepted-same-asset`）仍是独立待查项。

这四条中**仅第 4 条部分成立**：r2 会话确实覆盖了弓/弩，但装卸片源没有改变（见下节）。

### r2 上机结果（2026-10-22 23:02–23:14 会话）

归档 [CharacterPanel-issue-trace-r2-20261022-2314.log](CharacterPanel-issue-trace-r2-20261022-2314.log)：构建 `3617bf1f27`，12 请求、无错误、关包 `DAC restore … matched` 2 次（包括 23:14:11 的 `spell-groups=3`）。

- **`IsMovementDirection` 修复没有改变任何选片**：装盾仍是 `1hm Shield Equip Base`（战斗变体）、卸盾仍是 `Gunslicer … Sword_and_Shield`、斧装仍是 `Gunslicer … Sword\1hm_equip`、弓/弩也仍是 Gunslicer。按 `req=10` 的证据（`Smooth Moveset - 1hm\1hm Equip Base` 83030020 压过 Gunslicer `Sword` 800002）确认 **OAR 优先级是数值越大越优先**；因此非战斗变体（83031024/83030324）没赢，只能是它们仍被判为不合格。要么 OR 门里的 `IsMovementDirection` 分支没有走到本模块的谓词（原生答案仍是"移动中"），要么另有条件先失败。
- **去重日志生效**：RaySense 每查询只占 2 行，其余条件（IED placement hints、IsSneaking、OR、HasMagicEffect、CurrentMovementSpeed 等）都出现了；但每条查询仍有 15–64 条被省略，且签名只有 condition、没有项目文件夹，所以 Smooth 斧/盾候选的具体拒绝原因仍看不到。
- **torch 得到世界侧实证**：`request=12` 的 WORLD 快照里源角色同时有 `Animations\Tor_1HMPose.hkx(1590)` 与 `1hm Non-Combat idle Start(8978)`，而面板同一请求只查 `TOR_Idle` 并落到 base 1598。也就是说世界是"武器身体 idle + 火把手臂层"，面板是"整身 TOR_Idle"——与用户描述一致，下一包按"武器形态 + 火把层"改。
- axe 的 Start→Loop 仍未发生（Start 9053，无 Loop handoff）。

### r3 诊断（历史方案，未得到上机验证；2026-10-22 深夜，构建 `a46e724`，已部署未上机）

1. 拒绝签名改为 **项目文件夹 + 原因**，上限 48 行/查询；这样 `Smooth Moveset - Axe`/`- 1hm Shield` 的候选会以项目名出现在拒绝行里。
2. 新增 `OAR COND … movement-direction=seen answered=yes|no` 一行/查询：区分"谓词答了 false"与"该条件根本没走到本模块的钩子"（若没走到，原生答案会保留，`NOT` 分支依旧为假）。
3. 本轮**不改**任何选片或姿态行为；torch 姿态改动仍待下一包。

部署身份：`CharacterPanel-scenegraph-scenegraph-a46e724cc0-cl94faaed0c6-20261022T151801Z`，DLL SHA-256 `7ef034b1…`。**截至 2026-10-23 00:15 尚无 r3 会话日志**（最新运行时日志停在 r2 会话的 23:14:11）。下一场只需复现装/卸左盾、换单手斧、换弓/弩；判读顺序为：

1. 每查询的 `OAR COND movement-direction=seen answered=…` —— 旧 r3 实际只在 seen 时输出，不能由缺行推出 `seen=no` 或钩子缺失；当前显式 flags 见第 10 节，`seen=yes` 而选片不变表示问题在别的条件。
2. `OAR QUERY … rejected-candidate='…\Smooth Moveset - Axe\…'` —— 项目名现在会出现在拒绝行，直接给出该包落选的原因。
3. 若两项都指向 IED placement 条件，则下一包转入 IED 侧（装备挂点与查询时机），而不是继续改谓词。
4. torch 仍未改："武器形态 + 火把层"是下一个功能包；世界侧基线为 `Tor_1HMPose`(1590) + 武器 idle。


## 10. 2026-10-23 装备上下文与火把分层修复（待上机）

当前代码修复：所有实际 Unequip 查询使用之前的双手 ID，效果修订重查保留原过渡动作/上下文；下一 idle 才用当前装备。IED 历史查询按依赖放行：已核对 SDS 设置、显式 gear node；equipped hint 仅允许其请求手未变化。改变的手及未知历史谓词仍拒绝，原生 false 保留。

右手单手武器＋左火把使用武器 body idle＋独立查询加载的 Tor_1HMPose 左臂层，覆盖左锁骨、上臂、前臂、手指；不覆盖根/躯干/腿/右臂。只装卸火把不触发右手武器装卸；只卸右手武器而保留火把时，过渡仍保留火把层。torch-only 仍走 TOR_Idle。加载/基础片缺失会明确记录 body-only 回退，不能当作视觉等价。pending/reuse/refresh/retarget/same-asset/outgoing 路径均携带层所有权；当前只做源码审查，游戏时序未验证。

方向语义纠正：OAR 3.1.6 实际 DLL/PDB 和上游 enum 均为 0=None，1=Forward。站立时方向 0 为 true，其它为 false，NOT 仅应用一次。第 9 节的“全部 false 即可放行 Smooth”判断错误；保留原历史结果，不再以该说法判断选片。r2 装盾接受索引为 8938，卸盾为 9496；共两次 restore matched，日志文件保持原样。

诊断补齐：当前或刚卸下的单手斧/双手斧均可触发相同请求预算；POSE SCALE 输出原始 Havok x/y/z，POSE MAP 保持真实轨道/目标标识及 identity-unverified。转换仍用 x，未猜测修正弓/弩缩放。每个详细查询都有 movement-seen/answered=yes|no；候选拒绝带自身 flags、条件 GetArgument、root-false 与 observed false leaf。leaf 可以只是 OR 的一个 false 分支，不等于候选落选根因。上限仍为 48 条/查询，姿态仍为 4 窗/24 快照/请求，源骨骼快照仍仅 15 点。

旧 Smooth 标记报告以 formID & 0xFFFFFF 输出，因此 243801/243802 等字符串含轻插件的部分索引，不能当作 literal local 0x243802/0x3802；已知配置 loop 本地 ID 为 0x802。新条件参数证据用于比对实际效果，不能从旧字符串推断 FormID 不匹配。

本轮仅运行 [animation_repair_test.cpp](../../tests/animation_repair_test.cpp) 的两个 engine-free 场景，分别覆盖装备移除/IED/方向规则和火把路由/完整左臂合成/移除。未执行旧驱动或完整套件；另做 Release 构建及有界源码/链接核查。具体命令见 [build.md](../build.md#animation-repair-checks-2026-10-23)。未部署，游戏选片、原生 IED、异步加载/重定向、遮罩实际资产、画面及性能仍 UNVERIFIED。

下一轮同一次开包流程：保留右武器卸左盾→卸右武器，核对 query-hands=previous 与具体 IED 拒绝；单手斧观察 Equip→Start→Loop 的候选条件与前几帧 POSE SCALE/MAP；装备左火把并只卸火把，检查 ANIMATION TORCH composed/body-only、左臂与身体及移除残留；保留火把再卸右武器检查过渡；切弓/弩保存对应 raw-xyz。F7 重捕获与快速换装同时核对 graph/request，关包检查 DAC restore matched。保存 DLL 构建身份、完整日志及短视频；看不到/预算耗尽须注明，不算成功。

本地交付包：`build/animation-repair-20261023/CharacterPanel-animation-repair-20261023.zip`，内含新 DLL、匹配 PDB、中文测试说明及 manifest；未部署。构建身份 `CharacterPanel-scenegraph-scenegraph-7de3b9f9cf+dirty-cl94faaed0c6-20261023T063440Z` 为 configure-time 身份，dirty 工作树后续增量构建以哈希区分：DLL SHA-256 `ba844250bd43e6a5e6e473242a3a3678aa2439c0781254f7d762f752b1532a3a`；PDB SHA-256 `12984e676854f0cd13e1ad066760e8f980110a6b1b00b901d83ea0ffdfb93931`。配对 GUID `{748DB13E-5AAA-4883-8283-751805A36ED4}`、Age `74` 已一致核对。
