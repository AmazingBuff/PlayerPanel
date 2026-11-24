# C-IDLE v1 · 上机取证记录（2026-11-24，v126 + DisplayPose=false）

> 本文件归档 C-IDLE v1 契约（[idle-loop-contract-20261123](../idle-loop-contract-20261123/README.md)）的**首次上机取证**结果。
> 契约 §1–§5 的预注册文本不改写；§7/§8 的离线推断由本文的运行证据取代或修正。

## 一、会话身份

| 项 | 值 |
| --- | --- |
| 日期 | 2026-11-24 22:39–22:44 |
| 构建 | `CharacterPanel-scenegraph-v126-g6-domain-v3-20261122-db8da8a1e3-cl94faaed0c6-20261122T155429Z`（v126，与 G6-CON v3 同一 DLL） |
| INI | `823e24eb…`（`DisplayPose=false` + `DetailedAnimationTrace=true`，其余与 v126 实验档一致） |
| 日志 | [CharacterPanel-g6idle-displaypose-off-20261124.log](../CharacterPanel-g6idle-displaypose-off-20261124.log) 1,424,942 B，SHA-256 `a2c6cebb…` |
| MO2 档 | `AE`（OAR + Smooth Moveset + DAC 全启用） |
| 操作 | 收刀待机 → 拔刀开包 20 s → 关包 → 换武器 → 开包 20 s → 退出（5 个 request，5 个武器族） |

## 二、两个独立结论

### 2.1 `DisplayPose=false` 改变了面板显示的动画来源（架构发现）

**代码路径**（[controller.cpp:1222-1237](../../../src/animation/controller.cpp)，`advance()` 内）：

```cpp
if (display_pose()) {                       // ← true 时走这里
    bool const stepped = stepping && state.display.step_display_pose(*state.graph, held);
    if (have_pose) {
        state.pose.clear();
        for (auto& e : held) state.pose.emplace(e.first, e.second);   // held = 候选图 poseLocal
        apply_named_display_pose(state.pose, state.nodes, root_name);
        return coverage.applied;                                       // ← 提前返回
    }
}
// 以下整段（OAR 查询 → Smooth clip → compose_contributor）在 DisplayPose=true 时被跳过
```

`poseLocal` 来自**候选图**，而候选图由引擎原生 `63585` 加载 + `63587` 每帧更新驱动——**它不携带 OAR 的替换结果**。因此：

| `DisplayPose` | 面板显示的动画 | 机制 |
| --- | --- | --- |
| `true`（历史默认） | **原版动画**（Smooth 等 OAR 替换不生效） | 候选图 `poseLocal` |
| `false`（本轮） | **OAR 替换后的动画**（Smooth 生效） | 控制器合成 `state.pose` |

**运行证据**：`DisplayPose=true` 的会话里 `reason=boundary` 计数为 **0**；`false` 的本轮为 **36**。因为边界检测代码位于提前返回点之后。

**这解释了长期观察到的"面板动作与游戏世界不一致"**——不是动作损坏，而是两条并行显示路径取其一，`true` 取的是不含 OAR 的那条。

### 2.2 Start 段驻留缺陷在当前构建上真实复现（缺陷确认）

5 个武器族、36 次边界查询的完整对照。`801` = `mag_start on`，`802` = `mag_loop on`，`804` = `mag_combat on`（EDID 见下）：

| req | 武器族 | 边界时标记状态 | OAR 每次边界的选择 | 判定 |
| ---: | --- | --- | --- | --- |
| 1 | 1hm | `801=1 802=1 804=0`（4/4 快照） | `1hm Non-Combat idle Loop2`（9008，3/3） | **正常驻留** |
| 2 | 2hm | `801=1 802=1 804=0`（4/4 快照） | `2hm Non-Combat idle start`（9876，4/4） | **卡 Start** |
| 3 | 2h | `801=1 802=1 804=0` | `2hm Non-Combat idle loop`（9871） | 正常 |
| 4 | 2hw Axe | `801=1 802=1 804=0`（2/2 快照） | `2hw Axe Non-Combat idle Start`（10353，2/2） | **卡 Start** |
| 5 | 1hm（换装后） | `801=1 802=1 804=0`（2/2 快照） | `1hm Non-Combat idle Start`（9044，2/2） | **卡 Start** |

**操作者目视**（同一会话回传）：面板内切换武器时"会陷入由 Smooth 动作引入的 Loop Start 段的循环，而不会过渡到 Loop Idle 段"；动作流畅、无扭曲。

### 2.3 `801` 在全会话 20 次快照中从未被清除

| 元素 | 20 次快照中出现次数 |
| --- | ---: |
| `801`（`mag_start on`） | **20 / 20** |
| `805`（`mag_start off`） | **0 / 20** |
| `802`（`mag_loop on`） | 14 / 20（每次换武器后短暂消失，`Smooth_loop` 施法后恢复） |
| `804`（`mag_combat on`） | 3 / 20（仅换武器瞬间） |

即：**`Smooth_loop` 链正常工作（802 被打上、804 被打掉），唯独 `801` 全程不被清除。**

## 三、机制判断的修正（撤回一条、保留一条、新增一条）

### 3.1 撤回：「Start 的 priority 数值小所以胜出」

**不成立。** 同一时刻（`801=1 802=1 804=0`）：

- `1hm` 族选 **Loop2**（priority `…013`）
- `2hm` 族选 **start**（priority `…001`）

若"数值小者优先"成立，`2hm` 应选 `loop`（`…002`）而非 `start`（`…001`）；若"数值大者优先"成立，`2hm` 也不应选 `start`。**两种方向都无法解释族间差异**，因此 priority 不是本轮的决定因素。

### 3.2 保留：「`801`（`mag_start on`）从未被清除」

这条由 20/20 快照直接支持。`801` 只由 `weaponDraw` 施加、只由 `weaponSheathe` 移除（DAC toml 严格二选一；HKX 普查确认 `weaponSheathe` 仅存在于 `*_unequip.hkx`）——面板的 idle 通路不经过 Sheathe，故 `801` 常驻。

### 3.3 新增：族间差异来自 Start 变体的**条件结构不同**

对比两个 Start 变体的条件（离线读 `config.json`）：

| 变体 | 相位条件结构 |
| --- | --- |
| `1hm Non-Combat idle Start` | `HasMagicEffect 801` **单独一项**，另有 `OR[IsEquippedType == 1.0, right hand]` |
| `2hm Non-Combat idle start` | `OR[ HasMagicEffect 801, OR[ HasMagicEffect 804, IsInCombat negated, HasMagicEffect 801 ] ]`，无右手 `IsEquippedType` OR |

这是本轮**唯一发现的、能区分"1hm 正常 / 2hm 卡住"的结构差异**。它的确切影响**尚未验证**——需要单独取证（见 §五）。

## 四、结论的适用范围

**可以声称**：

- `DisplayPose=false` 使面板显示 OAR 替换后的动画，`true` 则不显示。此结论由代码路径 + `reason=boundary` 计数 0 vs 36 双重支持。
- Start 段驻留在 v126 + `DisplayPose=false` 上真实复现，涉及 `2hm` / `2hw Axe` / `1hm`（换装后）三族。
- `801` 在该会话中从未被清除。

**不能声称**：

- 不能说 priority 是决定因素（§3.1 已撤回）。
- 不能说所有武器族都受影响（`1hm` 首轮与 `2h` 正常）。
- 不能说修复方向已定（§五 三选项均未验证）。
- 不能说 `DisplayPose=true` 的历史行为等价于"缺陷不存在"——它只是没走 OAR。

## 五、候选修复方向（均未验证，不排优先级）

| 方向 | 做法 | 已知风险 |
| --- | --- | --- |
| **A. 面板侧强制驻留** | 检测到当前 clip 属于 `* idle Start` 且已完成一轮时，切换到同族 `* idle Loop` | 依赖命名约定（启发式）；需处理 `Loop2` 的 `Random<0.5` 变体 |
| **B. 面板补派 `sp_start off` 或 `weaponSheathe`** | 清除 `801`，使 Start 条件失效 | 直接施法与派发事件风险不同；可能干扰正常游玩的相位机；`DAC restore` 是否覆盖该 spell 未验证 |
| **C. 记录缺陷、暂不修** | 归档为已知限制 | 面板内 idle 首圈重放一次 |

**A 与 B 共同的前置条件**：必须证明修复动作在面板关闭/切换时不会把状态漏进正常游玩的相位机。

## 六、部署与回滚

| 档 | SHA-256 | 说明 |
| --- | --- | --- |
| 当前安装位 | `823e24eb8514bbfe580f` | `DisplayPose=false` + `DetailedAnimationTrace=true` |
| 探针档 | `4b62e7ad13cee5acfa10` | `DisplayPose=true` + `DetailedAnimationTrace=true`（本轮对照用） |
| 日常档 | `f8c291f1fabd14982142` | v126 G6 实验档原始状态（`DisplayPose=true`、`DetailedAnimationTrace=false`） |

文件均在 `dist/idle-probe-20261123/`。回滚：

```bash
cp dist/idle-probe-20261123/CharacterPanel-v126-g6exp-backup.ini \
   "E:/SkyrimAE/mods/CharacterPanel/SKSE/Plugins/CharacterPanel.ini"
```

**建议**：确认取证结束后把 `DetailedAnimationTrace` 改回 `false`（日志 1.4 MB/5 分钟，游戏内有开销）；`DisplayPose` 的取值取决于用户是否接受"原版动作"。

## 七、变更记录

- 2026-11-24：首次上机取证归档。撤回 priority 归因；确证 `801` 常驻、Start 驻留复现、`DisplayPose` 决定动画来源。判据文本不改写。
