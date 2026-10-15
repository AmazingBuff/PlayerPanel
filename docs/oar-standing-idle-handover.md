# 面板站立 idle：OAR 主动查询链交接（2026-10-16）

- 读者：下一个会话的 Agent / 开发者。本文是"面板内人物动作"这条线的**当前入口**；
  逐轮实验报告与其证据仍以 `docs/oar-hooks.md`、`oar-fork-handoff.md` 为准。
- 相关提交：`be089cd`（复制图为主显示）、`0b09e51`/`ac5c4b3`（站立判定与背包打开窗口）、
  `9e770a6`（**主动查询链**）。部署包身份见下。

## 1. 需求（用户拍板，勿改写）

面板内人物**永远站立**：

- 潜行、游泳、跳跃、移动中开背包 → 显示**普通站立 idle**（不是 sneakIdle / 游泳 / 跑步循环）；
- 战斗姿态（武器出鞘）→ 显示**对应武器形态的站立 idle**（单手 / 双手 / 弓 / 双持 / 法术 / 法杖 / 火把 / 盾）；
- 判定由 **OAR 自己**做出（条件按站立姿态回答，选片仍走 OAR 的规则）；
  不用"采样池回放"——没采到的动作也必须能播（用户明确否决过池子路线，且 `code-maintenance.md`
  已记"idle 池统计均已退役"）。

## 2. 已实现（全部已提交）

| 模块 | 行为 |
| --- | --- |
| `src/oar/policy.h` | 判定面：`standing_predicate`/`standing_value`（潜行/游泳/攻击/移动/骑乘/格挡等判否）、`WeaponForm` 与 `weapon_form_from_hands`（双手/弓占双手；双单手=双持；单手+盾/法术/火把仍算单手）、`weapon_form_type`（IsEquippedType 数值） |
| `src/oar/hooks.h/.cpp` | 站立窗口（`arm_standing_window`/`tick_standing_window`/`standing_window_open`、`set_standing_weapon_form`、`selected_clip`）；两个 thunk 在窗口内按站立回答并把被求值生成器记为选中；**主动查询**：`query_supported()`/`query_replacement()` |
| `src/render/renderer.cpp` | 背包打开**上升沿**武装窗口（读边沿、不 consume 菜单事件），每帧 tick，面板关闭即清零；`Standing_Window_Frames = 8` |
| `src/character/scene_graph_copy.cpp` | 面板打开时**等窗口结束**再捕获（`m_open_pending`）；捕获后 log-only 调用一次主动查询；捕获日志带 `state[sneaking=… swimming=… weapon-drawn=… attack=… knock=… life=…]` |
| `src/oar/binary_profile.h` | `QueryProfile` + `matches_query`：四条查询入口的 RVA 与入口字节，**任一条缺失或字节不符则整个查询不启用**（3.2.1 无 PDB → 查询停用，两个条件 hook 照常） |

### 2.1 查询链（3.1.6 已核对，安装版 DLL 时间戳 `0x6A40292A`、镜像 `0x3B3000`）

| 符号 | RVA | 入口字节（前 16） |
| --- | --- | --- |
| `OpenAnimationReplacer::GetSingleton` | `0x0F7CD0` | `40 53 48 83 EC 20 8B 0D 3C 91 29 00 65 48 8B 04` |
| `OpenAnimationReplacer::ForEachReplacerProjectData` | `0x199780` | `48 89 5C 24 18 55 56 57 48 83 EC 50 48 8B 05 AD` |
| `ReplacerProjectData::GetAnimationReplacements(uint16)` | `0x1EBEA0` | `48 B8 25 23 22 84 E4 9C F2 CB 44 0F B6 C2 4C 33` |
| `AnimationReplacements::EvaluateConditionsAndGetReplacementAnimation(refr, clip)` | `0x1A5720` | `40 53 48 83 EC 20 B8 FF FF 00 00 41 0F B7 D8 66` |

最后一个就是 **OAR 在 clip 激活时自己调用的决策函数**；它内部逐个 `ReplacementAnimation::EvaluateConditions`
→ 条件求值走我们已 hook 的 `ConditionBase::Evaluate` → **站立判定在 OAR 的决策里自动生效**。

## 3. 实测证据（2026-10-16 上机日志）

```
SCOPY OAR hooks installed: … active query available
SCOPY OAR window closed hits=0 selected=0                          ← 开背包后世界已暂停，OAR 无求值
SCOPY ANIM captured clip='Animations\SneakMTIdle.hkx' pick=running-clip
SCOPY OAR query base=12220 clip='' candidates=6 with-replacements=0 replacement=0x0
```

判读：

- `hits=0`：**等不到判定**——OAR 只在 clip 激活/图更新时求值，开背包不激活新 clip（与
  `oar-fork-handoff.md` 的"暂停期动画图零响应"一致）。所以"利用世界内正在推进的判定"这条路
  在此时刻无判定可用，必须**主动问**（本提交已实现）。
- `candidates=6`：单例可达、project data 遍历成功、`std::function` ABI 正确、四条入口字节校验通过。
- `with-replacements=0`：**唯一剩余阻塞**——索引语义不对（见 §4）。

## 4. 当前阻塞：索引语义（下一步要修的就是这一处）

`ReplacerProjectData::GetAnimationReplacements(uint16 a_originalIndex)` 的键是**基础动画索引**
（`hkbCharacterStringData::animationNames` 下标）。而 `hkbClipGenerator::animationBindingIndex`
在 OAR 替换后会被写成**替换件自己的索引**（`ActiveClip::ReplaceAnimation`：`clipGenerator->animationBindingIndex = variant->GetIndex()`）。
我们传的是后者，所以查不到替换集。

**`ForEachActiveClip` 未导出**（publics 只有 `RegisterEventSink`/RTTI/析构），所以"从 ActiveClip 读
`_originalIndex`"这条路不可用。

### 修法 A（推荐）：用基础动画名反查索引

`hkbClipGenerator::animationName` 保留的是**基础动画名**（OAR 只改索引、不改名字）。已核对的 CLib 字段：

- `hkbCharacterStringData::animationNames` — `hkArray<hkStringPtr>`，偏移 `0x30`
- `BShkbAnimationGraph::characterInstance` — `hkbCharacter`，偏移 `0xC0`
- `hkbCharacter::projectData` — `hkRefPtr<hkbProjectData>`，偏移 `0x60`
- **待补的一环**：`hkbProjectData → stringData`（Havok 侧入口）。取到后按名字查表得下标，
  替换掉现在传的 `animationBindingIndex`。首调用先打印表条数与已知 **15203** 对比验证，
  全程走现有 `readable()`/`looks_like_object()` 守卫，不盲读偏移。

### 修法 B（补 A 的盲区）：记住"站立时的基础索引"

玩家潜行/移动时开背包，当前生成器不是站立 idle，A 会查到潜行/移动的基础索引。所以游玩期
**玩家站立静止**时，把当时的 idle 基础索引按武器形态各记一个数（**只记索引，不存动画、不回放采样**，
实际动作仍由 OAR 当场决策——与已退役的"池子回放"性质不同），查不到时回落 A。

## 5. 后续步骤（修完索引之后）

1. 日志应从 `with-replacements=0` 变为 `with-replacements>0`；若 `replacement=0x0`，用既有
   `OverrideTally` 机制把"哪条条件否决了"点名（`docs/oar-hooks.md` 已有该仪器的用法）。
2. `replacement≠0` 后：取替换件的动画索引 → 引擎已加载动画（HKX 链：索引 → Queue/Load →
   绑定集条目 → `SamplePartialTracks`，HKX18/24/28 已验证）→ 采样到复制图，替换
   `pick=running-clip` 这条回退。
3. 换装动作（背包内换装备播一次装备动作再回 idle）是这条链的下一个应用；不要用池子回放实现。

## 6. 构建 / 部署 / 判据

- 实验配置：

```powershell
cmake --preset Release -B build-scopy -DCHARACTER_PANEL_SCENE_COPY_EXPERIMENT=ON -DCHARACTER_PANEL_PROBE_REVISION=copy-primary
cmake --build build-scopy --config Release --target CharacterPanel CharacterPanelCopyMathTest
ctest --test-dir build-scopy -C Release -R '^snapshot_transform$' --output-on-failure
cmake --build build-scopy --config Release --target CharacterPanelSceneCopyPackage
```

- 本地检查 17 项（含新增 `panel pose answer`）。当前部署：`E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\`，
  DLL SHA-256 `41928079b99a98474277675f2bf2df4f5bb8e562aca55715979de7873a295a52`。
- 上机判据：站立/潜行/持械各开一次背包，看 `SCOPY OAR query … with-replacements=` 与
  `replacement=` 两列；`pick=` 列在动画接入前仍可能是 `running-clip`（回退），这不是失败判据。
