# 探针轮：菜单内装备操作是否在暂停期激活 clip（2026-10-11）——**已实测，判读 = C（不可观测）**

## 判读（2026-10-11 晚，用户上机，日志
[CharacterPanel-hkx36probe-menu-equip-20261011.log](diagnostics/CharacterPanel-hkx36probe-menu-equip-20261011.log)）

**结论：C——菜单内装备操作在暂停期对动画图不可观测。**

证据链（两次捕获，capture=1 基线 → 装备 → capture=2）：

1. **装备确实发生了**：几何普查 `nodes=1878→1868 geoms=131→129`；几何清单 diff 显示
   capture 2 新增 `1stPersonElvenDagger:0` / `ElvenDaggerBloodEdge` / `ElvenDaggerBloodEdgeSpec`
   （装备了精灵匕首），同时少了 6 个法术特效几何（`Circle04`/`Flash02`/`SuperSpray06`/
   `Tube01`/`Wisps03`/`pSmallFlare02`——一个手上的法术被匕首替换）。
2. **动画图纹丝未动**：两次捕获的活动 clip 集（`kClone`+`control` 非空）完全一致——
   `MRh_Idle` + `MLh_Idle`，idx=11616，**连 binding 指针都是同一个**（0x1e858980a00）
   = 没有任何 clip 激活、没有分支切换、没有重激活；绑定集也无引擎侧新填充
   （`MT_Idle idx=1022` 那条从 `no-binding-in-set` 变 `valid tracks=99` 是 capture 1
   自身装载链的副作用，不是装备触发的）。
3. 池子两轮 `samples=25 idle=0 equip=0` 不变——用户全程在菜单内（暂停期不采样），符合流程。

**诚实的边界**：本样本是"法术 → 匕首（收鞘状态）"，即使不暂停，手持未出鞘武器时的
手部 idle 也未必切换——单样本不能证明"所有装备操作都不可观测"，只能证明
**对这次装备操作，暂停期动画层零响应**。对面板的工程含义不变：**不能依赖暂停期观测**。

## 决策映射（按探针测试单的约定，C 级落地）

- **equip = 事件 + 池子映射回放**：装备事件（TESEquipEvent / 物品菜单）触发面板从 equip 池
  选一个播一次再回 idle；"OAR 会不会选别的"在暂停期问不到，分布语义由池子承担。
- **"暂停期状态精确选片"（菜单内装备的实时 OAR 决策）只有 fork 能做**——是否立项由用户
  按产品必要性决定；面板侧做成探测式（fork 在→查询，不在→池子），两条路线共存。
- **调度器开工不依赖此决定**（idle 池轮换 + equip 事件回放两条都属路线 2）。

## 顺带记入调度器需求的分类器缺口

本轮池子 `samples=25 idle=0`，`other-names` 全是 `MRh_Idle`/`MLh_Idle`——**持法术角色的
站立 idle 就是手部 idle**，但 `mentions_idle`/`is_base_idle` 不认它们 → 全被归入 other，
池子的 idle 类是空的。调度器动工时必须把手部 idle 族（`*Lh_Idle`/`*Rh_Idle`，
以及 `1HM_Idle`/`2HM_Idle`/`Bow_Idle` 等）纳入 idle 类，否则施法存档的面板无片可播。

- **目的**：回答一个事实问题——玩家在背包菜单（游戏暂停）里做装备操作时，
  引擎会不会在暂停期就在玩家第三人称行为图上激活新的 clip（= OAR 的钩子会不会
  在暂停期写出它的选择索引、从而被捕获探针读到）。
- **这个答案决定什么**：
  - **是** → 路线 2（观察玩家选择）在两状态范围内语义完整：idle=池子轮换，
    equip=装备时实时读到 OAR 的选择；**fork OAR 不立项**；
  - **否** → 菜单内装备的实时 pick 只有 fork 能做；装备动作退回"事件 + 池子映射回放"，
    是否立项 fork 由"状态精确"语义的必要性决定。
- **无需新构建**：已部署的 HKX35（`…-HKX35-9bbfcf3ccc+dirty-…`）自带的捕获探针
  就是本探针的仪器——每次 F7 捕获都会打印 `SCOPY ANIM instance clip` 行
  （玩家第三人称图、实例侧活动 clip：名字 / 索引 / clone-state / control）。
  实测确认它只走 `graph=0`（DefaultFemale，正是面板克隆的那张图），
  且**活动 clip（`clone-state=kClone`、`control` 非空）排在打印序列最前**
  （打印上限 12 个，HKX35 实测 30 个 clip 中前 12 全部覆盖两个活动 clip）。

## 已知参照（来自 HKX35 日志的基线事实）

- 活动中的 clip 长这样（:67/:73）：
  `SCOPY ANIM instance clip name='Animations\MRh_Idle.hkx' idx=11617 clone-state=kClone binding=0x… control=0x… bind-src=clip`
  ——`kClone` + `control` 非空 = **正在运行**；`kTemplate`/`kDefault` + `control=0x0` = 模板桩/未激活。
- 池子（游玩期采样）已经给出装备类索引的对照表：
  `equip=4 'idx=11782:2, idx=528:2, idx=11766:1, idx=526:1'`（拔/收武器时 OAR 写的选择）。
- 12 个打印名额的截断风险：新激活的 clip 在实例侧遍历中先被发现、先打印，
  几乎必然在列；若判读时怀疑被截断，下一轮发"菜单期活动 clip 监视器"（HKX36）补测。

## 上机步骤（全程不需要重新部署）

1. 进重度存档，确认横幅 `SCOPY BUILD …-HKX35-…`。
2. **基线捕获**：站立不动几秒 → 开背包 → **先不做任何装备操作** → F7 捕获一次。
   记下本次 `SCOPY ANIM instance clip` 行里所有 `kClone`+`control` 非空的条目
   （名字 + idx）——这就是"未做装备操作时的活动集"。
3. **装备探针**：仍在背包里，**换装一件不同类型的武器**
   （例如当前单手 → 换成双手/弓；类型变化才会逼 idle 分支重新评估）→ 等 2–3 秒 →
   再 F7 捕获一次。对比两次捕获的 `instance clip` 行。
   - 若同一次菜单内第二次 F7 没有产出新的 capture 块：关背包重开再 F7（装备状态保留，
     暂停期图不推进，结果等价）。
4. **可选第三样本**：把武器换回去/卸下 → 再捕获一次（看恢复行为）。
5. 复制日志进 `docs/diagnostics/`（惯例名 `CharacterPanel-hkx36probe-menu-equip-<日期>.log`，
   即使构建还是 HKX35 也用这个轮次名记录）。

## 判读（三级，预登记的方法学框架，实际判读见文首）

| 级别 | 日志特征 | 结论 |
| --- | --- | --- |
| **A（完全可观测）** | 装备操作后出现**新的** `kClone`+`control` 非空 clip，且其 `idx` 命中 OAR 替换索引（与池子里的 equip 索引或名字表里的 OAR 路径对得上），或 idle 分支的活跃 clip 换成了新武器类型对应的 idle（idx 同样是 OAR 替换件） | 引擎在暂停期处理了装备事件且 **OAR 已写出选择** → 路线 2 实时可观测，fork 不立项 |
| **B（部分可观测）** | 出现新 clip 但 `idx` 是原版索引（OAR 钩子还没跑） | 图在暂停期处理了事件，但选择要等激活——装备动画仍可由"事件 + 池子映射"回放；状态精确语义需 fork |
| **C（不可观测）** | 活动集零变化（连 idle 分支都没动） | 菜单内装备对图完全不可见直到关包；同 B 的后续路线 |

**注意一个陷阱**：换装同类型武器（单手换单手）可能不触发任何分支切换——
那不算 C 的证据，必须用**不同武器类型**才能逼出 idle 分支的重新评估。
护甲/衣物换装本来就没有动画语义，不作为判据。
