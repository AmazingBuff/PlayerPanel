# AFP9：覆盖审计与 R01 直绘接入

日期：2026-10-13。依据：[AFP 汇总 §6 Next](afp-summary-2026-10-13.md)、[AFP 后续验证](afp-next-validation-and-paused-gaps-2026-10-13.md) §2、[同 Actor 测试单](actor-final-pose-validation-2026-10-12.md) R01。
基线：`ce35ad79e3`（AFP8 收官 + 清理提交后的干净工作区）。

## 1. 本轮两个工作包

### 工作包 A：覆盖审计（只点名、只分类，不扩写）

AFP8 收据的三笔账：应用循环只处理 `min(m_targets.size(), count)` = 98 槽中的 92 写 + 6 不可达，强制输出的 116 槽里有 **18 槽从未进入循环**；6 个 unreachable 只计数未点名；世界空间 104/109 的 5 个超差项是武器附着节点，尚未做 skin 必要性分类。

AFP9 新增（`FinalPoseApply`）：

1. `apply()` 同帧对账，各集合互斥：`output-slots`（本帧姿态元素数）/ `mapping-slots`（验证时锁定的映射表长）/ `attempted`（循环处理的槽数）/ `written` / `unreachable`（名字解析失败）/ `unprocessed`（attempted 之后的槽）。`written + unreachable + unprocessed = output-slots` 必须能对账。
2. **点名**：unreachable 逐项给出骨名（验证时捕获的名字表）；unprocessed 的名字不依赖验证时表——apply 现场从动画骨架读（骨架对任意槽都有名字）。名字表各封顶 16 项，超出一律 `+N` 计数。
3. **必要性分类**：unreachable/unprocessed 的名字对照 skin palette（`NiSkinInstance::bones` 实际指向的节点名集合，新增共享助手 `skin_palette_names()`）：`necessary`（被目标网格读，不可跳过）/ `helper`（面板不画的辅助支链）/ `weapon-attachment`（名字含 `Weapon`，AFP8 已证图不驱动的附着节点）。palette 未绑定（图从未被渲染）时记 `unbound-palette`，不得记作"无必要骨"。
4. **明确不扩写**：116 槽全覆盖写入不在本轮——`Weapon*` 槽若被图输出的残差驱动，盲写会把武器写出正确位置。扩写是审计点名 + 分类通过后的独立决定。
5. 审计行打印时机：计数签名相对上一帧变化时、以及每次 F2 mark 的 A2 行（A2 行追加 `attempted/unprocessed` 字段）。连续相同的审计不逐帧刷屏。

### 工作包 B：R01 直绘（同 Actor 静态显示，物理/表情仍关）

当前 renderer 画的是复制图（`CharacterClone::draw`）；AFP 写入的傀儡骨架从未上屏。AFP9 新增 `PuppetDraw`（`src/character/puppet_draw.h/.cpp`）：

1. **身份行**（R01 步骤 2）：绘制来源一次性/换代打印 actor FormID、`Get3D(false)` 根指针、graph 指针、是否与 `FinalPoseApply` 当前 generation 同根——证明画的就是被写姿态的同一个树。
2. **帧内顺序**（渲染括号内、面板打开分支）：
   - **A4（pass 前）**：按固定骨名集（`NPC Head [Head]`、`NPC Spine1 [Spn1]`、`NPC Spine2 [Spn2]`、`NPC R Hand [RHand]`、`NPC L Hand [LHand]`，缺失项记 missing）采样 local，证明 A2 的写入仍节点在手；
   - **临时取景**：保存根 local（含 scale/rotate/translate）→ 写 placement（父世界逆 × 目标世界；视轴锚点 `Studio_Depth=485`、`Studio_Facing_Z_Rad=0`、`Studio_Figure_Scale=0.35`，全部沿用复制图已验证常量）→ `UpdateDownwardPass` + `UpdateWorldBound` → 蒙皮包围盒居中（复用 `is_collision_helper` 过滤）→ 再级联一次；
   - **绘制**：`StudioLight::place(anchor)` → `draw_figure_passes(root=傀儡根)` → `StudioLight::park()`，与复制图共用同一条缝；
   - **A5（draw 后）**：同一骨名集再采样，与 A4 的副本比 max-rot/max-pos——判定"绘制没有覆盖姿态"；
   - **恢复**：根 local 写回保存值 → 再级联 → 对账 root world / worldBound 恢复前 / 恢复后（数值行）。**失败与关闭出口同样恢复**（作用域保证，包括 drawn==0 早退）。
3. **对照表**：取景改写了根 local/world/previousWorld/worldBound，恢复前 / 后逐项打印；暂停中无其他写者（渲染括号串行），下一世界帧由引擎按未动过的 local 重推——"下一世界帧无泄漏"以恢复对账行 + 用户观察世界侧无异常为准。
4. **世界隔离不变量**：不移动 Actor 引用位置（`SetPosition` 禁用）、不写骨 local 以外的树数据、不动相机、不解除暂停。
5. **控制**：F6 = R01 直绘开关（默认关）。开：面板画傀儡（傀儡不可用时回退复制图并告警）；关：现行为不变。F7/F8（复制图捕获/显示开关）在 R01 开启时不影响面板内容。F3 仍被其他 mod 占用，不碰；F2/F4 语义不变。
6. 首轮范围：物理（A3=`skipped`）、FaceGen、体型同步不接入；看见身体动画只通过"直绘 + AFP 应用"这条前置链。

## 2. 发包 manifest 最低项

- 构建：`CharacterPanel-scopy-AFP9-<src-rev><dirty>-cl<clib-rev><dirty>-<UTC>`；DLL/PDB SHA-256 进包 manifest。
- 关键开关状态：`CHARACTER_PANEL_SCENE_COPY_EXPERIMENT=ON`、R01=F6 状态、F4 drive 档位、OAR/物理 mod 版本。
- 判读容差：沿用 `Pose_Match_Degrees=1.0`、`Pose_Match_Min_Compared=8`、`Pose_Match_Min_Ratio=0.9`（R00：发包前固定，不事后改）。

## 3. 游戏内验收

### V1 覆盖对账

1. 读档 → 等 AFP 校准在世界运行时成立（verify=0.98 量级）→ F4 至 `tick+apply+force` → 暂停背包。
2. F2 mark：A2 行应出现 `attempted=98 unprocessed=18`（以实际为准），对账 `written + unreachable + unprocessed = output-slots` 成立。
3. 审计行：18 个 unprocessed 与 6 个 unreachable 逐项点名 + 分类；`Weapon*` 应落在 `weapon-attachment`。必要骨（`necessary`）若非零，逐个给出名字——这是下一轮扩写/豁免的输入。
4. 重复 F4 档位切换 + 两次单步：审计行只在计数变化时刷新，无逐帧刷屏。

**通过**：对账恒等式成立且点名完整；不出现把观测匹配数当写入数的表述。

### V2 R01 直绘

1. 独立测试存档进游戏，等傀儡装配完成（`SCOPY PUPPET born` → parked）。F6 开（日志 `SCOPY R01 direct-draw on`）。
2. 正常世界帧先做一次 F2 mark（引擎参考仍可得）→ 开背包（世界暂停）→ F4 至 `tick+apply+force`。
3. 面板应显示**傀儡本体**（不是复制图）：日志身份行 root/graph 与 A2 的 generation 同源；`SCOPY R01 draw passes=N`。检查身体、脸、头发、护甲、左右手——首轮**发型/护甲物理静止、无表情**属既定范围。
4. A4/A5 对：`A5 vs A4 max-rot≈0`（绘制不覆盖姿态）；A2 后 A4 前的骨 local 与 AFP 应用数字一致。
5. 恢复对账行：root world / worldBound 前后一致；关面板回世界，玩家/傀儡位置外观无异常、无新克隆闪现（傀儡仍在泊位深度）。
6. F6 关：回到复制图路线，行为与 AFP8 时代一致（回归确认）。

**通过**：面板画面来自傀儡自身的树（几何/蒙皮/根指针证据），取景恢复可证，世界侧零副作用。静态直绘不等于更新/物理/外观一致性通过——那些仍按 R04/R05/R08/BM 各自关卡验收。

## 4. 红线

- 不扩写 98 槽之外的姿态；不做重复捕获/释放压力测试；停放图泄漏问题原样保留（S1/R09 仍关）。
- 不在暂停中解除全局暂停推进任何子系统；不改 FSMP/CBPC 接口。
- F3 不绑定；一次游戏会话只装一个 DLL；`extern/CommonLibSSE` 的 dirty 状态提交时排除。

## 5. 回传补充

| 项 | 读数 |
| --- | --- |
| 对账恒等式（output/attempted/written/unreachable/unprocessed） | |
| 18 unprocessed 点名 + 分类 | |
| 6 unreachable 点名 + 分类 | |
| R01 身份行（root/graph/generation 同源） | |
| A4→A5 max-rot/max-pos | |
| 取景恢复对账（root world/worldBound） | |
| F6 关回归 | |
| 截图（正/侧/背）+ 日志 + manifest | |
