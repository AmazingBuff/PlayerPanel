# OAR fork 会话交接（2026-10-11 会话收官，fork 会话从这里开始）

- 读者：fork 会话的 Agent / 开发者。OAR 源码已在工作区：
  `E:\SkyrimTools\Proj\OpenAnimationReplacer`（v3.2.1，commit `f4e7688`，与面板侧静态核对同一 checkout）。
- 面板侧入口：[s2-animation-handoff.md](s2-animation-handoff.md)（§0 有本会话全部判读链）。

## 一句话

为面板的"暂停期实时 OAR 决策"立项 fork：给 OpenAnimationReplacer 加一个**纯增量**的查询接口
（给定 refr + 基础动画，此刻 OAR 会选哪个），面板侧做成**探测式共存**
（fork 在 → 查询；不在 → 池子回放，现有路线一行不作废）。

## 决策链（为什么走到 fork，全部有实证）

1. 面板两状态范围：idle + equip（用户拍板，milestone §11）；跑/战斗/跳等不收。
2. 路线 2（观察玩家的 OAR 选择）已建成并验证：游玩期池子采样 + 捕获时实时读当前 clip。
3. **探针判读 = C**（2026-10-11 晚，[探针测试单](scene-copy-probe-menu-equip-2026-10-11.md)）：
   菜单内装备精灵匕首（几何普查 +4/−6 确认装备发生），但活动 clip 集两次捕获**逐字节一致**
   （MRh/MLh idx=11616，连 binding 指针都没变）——暂停期动画图零响应。
   → 暂停期"OAR 的决策"**尚不存在**（决策发生在 clip Activate/Update 时刻，菜单内装备的
   clip 关包前不激活），观察路线原理上拿不到。
4. 不 fork 的替代路全部评估过：
   - 池子回放 = OAR 的**历史**决策按分布重放（equip 落地用这个，非实时）；
   - 自己复刻 OAR 的选择逻辑（读 JSON 自求值）——否决：条件类型繁多、升级即失效，且那是模仿不是 OAR 决策；
   - 暂停期强制推进玩家图逼 clip 激活——违反 FR-05 红线（面板操作不改变 O），排除。
5. 公开 API 补不上：`GetCurrentReplacementAnimationInfo` 需要已存在的 clip generator；
   `AddCustomCondition` 等只能注册扩展、无按需求值入口（OAR 源码已核对）。
6. 用户拍板：另开会话处理 fork。

## fork 的设计方向（已讨论，未写码）

- **形态**：不改 OAR 任何现有路径，只加查询入口（SKSE 消息接口或导出函数）：
  `Query(refr, baseClipNameOrIndex) → { replacement index / animation path, submod, variant, … }`。
- **实现要点**：用 OAR 自己的条件对象按需求值——
  `ICondition::Evaluate(a_refr, a_clipGenerator, a_parentSubMod)`（refr 传玩家）；
  选择机制在 `ReplacerMods → project → SubMod（条件/变体/优先级）→ ReplacementAnimation`；
  最终产物 = `animationBindingIndex`（`ActiveClip::ReplaceAnimation` 里那行
  `variant->GetIndex()`），索引语义 = `hkbCharacterStringData->animationNames` 的下标。
- **现成先例**：OAR 的预览 UI 用 `FakeClipGenerator` 在不激活 clip 的情况下走完整选择/采样
  （`FakeClipGenerator.cpp`、`ActiveAnimationPreview.cpp:38` cache=nullptr）——查询接口可复用同一条路。
- **已知设计点（fork 会话要处理）**：
  - 有状态变体语义（random-no-immediate-repeat / sequence 的 `VariantStateData` 挂在 ActiveClip 上）
    ——查询时没有 ActiveClip：合成最小上下文，或降级为纯随机（需明确取舍）；
  - 少数条件类型读 clipGenerator 状态——传 nullptr 还是合成 FakeClipGenerator；
  - 版本锚定：checkout = v3.2.1；**用户现装的 OAR 版本必须对齐**（fork 是同名同导出的整 DLL 替换）；
  - 补丁严格只加不改——fork 的 bug 会影响全系统动画。

## 面板侧契约（fork 接入时对齐的既有事实，全部实测）

- 面板克隆第三人称图（DefaultFemale，graph=0）；角色名字表 15203 条、与绑定集同序（HKX3）。
- 引擎链已验证：索引 → 引擎 `Queue`/`Load` → 绑定集条目（**原版索引给完整轨道表；OAR 替换件常为空表**）
  → `SamplePartialTracks`（**槽 04**；槽 06 崩——`transformOffsets` 被引擎故意留空，HKX24/16）
  → 恒等映射（轨道 i → 骨骼 i）需结构校验 + 时机性 A/B + 视觉兜底（HKX28 实证 0.15°）。
- 池子机制：游玩期每 45 帧读玩家 `activeNodes`（`collect_running_clip_generators`，名字按指针缓存），
  idle/equip 分类计数 = OAR 配置分布的采样；捕获时 `report_idle_pool("at-capture")` 打印。
- **分类器缺口（调度器与 fork 都相关）**：手部 idle 族（`*Lh_Idle`/`*Rh_Idle`）目前归 other——
  持法术角色的站立 idle 就是它们，法术存档的 idle 池会为空（实测 `samples=25 idle=0`）。
- 探测式共存：面板在捕获时探测 fork 的接口；不在 → 池子回放。

## 本会话（2026-10-11 白天）其余成果与现状

- **HKX34**（`9bbfcf3`）：面板黑屏 + 物品卡泄漏修复。根因 = 名字 token 剪枝漏掉
  `Circle*`/`GeoSphere*`/`Tube*` 等特效网格，其 `GetRenderPasses` 在 UI3D accumulator 窗口内
  翻脏 `DIRTY_RENDERTARGET` → 全部 pass 画进引擎账本目标 kFRAMEBUFFER（面板黑、克隆 pass 以
  UI 相机投影落在屏幕中心 = 物品卡）。修复：`is_effect_object()` 按着色器属性类剪枝
  （RTTI 含 `EffectShader`）+ `draw()` 一次性 OM 绑定回读（`Clone draw OM check: match=true`）。
- **HKX35**（`36c8c04`）：切换卡顿修复。根因 = activeNodes 改写把逐采样名字读带回来
  （`guarded_string` 逐字节查页，OAR 120 字符路径 ≈120 次 VirtualQuery；每采样 ≈1000–1600 次）。
  修复：名字/类别按 clip 指针缓存（首见读一次、停跑扫除）+ `clip_generator_is_live` 廉价虚表先行
  （非 clip 条目 1 次查询）+ `guarded_string` 16 字节窗口（语义不变，查询 ÷16）。稳态 ≈0.05ms/0.75s。
- **实测全绿**：无卡顿（用户确认）、面板正常、`OM check match=true`、池子首次采到完整分布
  （idle=5 索引、equip=4）。部署 DLL：**HKX35**，SHA-256 `678b6936…`
  （`E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\`，PDB 同步）。
- 提交链：`9bbfcf3` → `36c8c04` → `3c58db8`（HKX35 判读）→ `f9ba1ad`（探针单）→ `7b878d3`（探针判读）。
- 判读文档：[HKX34/35 测试单](scene-graph-copy-next-test-2026-10-11.md)、
  [探针测试单](scene-copy-probe-menu-equip-2026-10-11.md)；
  日志归档 `docs/diagnostics/CharacterPanel-hkx35-cost-model-20261011.log`、
  `…-hkx36probe-menu-equip-20261011.log`。

## fork 之外的遗留队列（不阻塞 fork）

1. **多动作循环调度器**（设计已拍板：面板按自身状态从池子选片、idle 加权轮换不立即重复、
   equip 事件 + 池子回放播完回 idle、映射分级验证、手部 idle 分类器修正）；
2. H05 隔离的仪器化证据（只读基线对比面板操作前后 O/NPC 姿态）；
3. S1 安全回收（最硬门槛：三条释放路径全崩、现行停放不销毁）。
