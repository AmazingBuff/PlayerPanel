# AFP2 设计输入：同 Actor 直绘的结构侦察（2026-10-13，只读）

- 依据：[PRD 0.9 §6.6](player-panel-prd.md)、[同 Actor 最终姿态测试单](actor-final-pose-validation-2026-10-12.md)、
  [AFP1 测试单](scene-copy-afp1-final-output-test-2026-10-13.md)。
- 性质：对现行绘制路径的一次系统只读核对（`character_clone.cpp`／`scene_graph_copy.cpp`／`renderer.cpp`／
  `render_hook.cpp`／`studio/light.cpp`／`dx11/d3d11_util.{h,cpp}`／`UI3DSceneManager.h`），行号即锚点。
  本文是 AFP2 发包前"必须能解释清楚"的那部分证据；**AFP2 本身未实现**。

## 1. 现行绘制序列（`CharacterClone::draw()`）

| 步骤 | 位置 | 说明 |
| --- | --- | --- |
| 所有权门 | :878 | 非自有/未就绪直接 false（**路线专属**） |
| 根/累加器门 | :881-884 | `p_root = m_graph_object`；`accumulator = ui3d->unk10` |
| 渲染器数据门 | :886-889 | |
| `DIRTY_RENDERTARGET` 复位 | :891 | 引擎账本目标被翻走的对策 |
| 摆姿阶段 | :894 → :816-874 | `m_anchor` 计算 + 写根 local + 骨骼 bind 重置 + 级联 |
| 动画/探针缝 | :901／:903 | `SceneGraphCopy::animate_at_base_pose`／`probe_at_base_pose` |
| 灯光 rig 落位 | :910 | `StudioLight::place(m_anchor)`（锚点来自摆姿阶段） |
| 视口/RTV/深度/清屏/混合/光栅 | :912-933 | 外层 `D3D11StateCapture` 负责恢复 |
| 光照槽注入门 | :938-939 | `lights()` + `inject_lights` |
| 几何遍历 → 生成 pass | :943-949 | `GetRenderPasses(geometry, kNormal, accumulator)` |
| 逐 pass 提交 | :951-965 | 有效门 :953、每 pass 重绑 OM、`submit_pass` :964 |
| 收尾 | :972-1006 | draw-done 探针、OM 回读比对 :980-989、普查日志、`DIRTY_RENDERTARGET` 置位 :1006 |
| 失败/成功出口 | :1008-1019 | `drawn==0` → warn+park+false；否则 park+true |

`submit_pass`（:496-530）：保存 pass 的 `sceneLights/numLights/numShadowLights` → 换成本 rig 的 3 盏灯
（:507-509）→ 补壳（:511-519）→ **`BSBatchRenderer::SetupAndDrawPass(pass, pass->passEnum, (pass->passEnum & 0x40) != 0, 0x200)`**（:522）
→ 原值还原（:524-528）。

## 2. 所有权耦合：复用面与路线专属面

- **路线专属**：`:878` 的所有权门；:894 的摆姿阶段（`pose()` 写根 local :849/:862/:868-870、`reset_root_to_bind_pose` :853
  → :349-360 / :362-466）；`m_is_snapshot` 相关的一切（:829-845、:996-1003）；灯光锚点取值 :910。
- **与所有权无关、可复用**：:884 累加器选择、:891/:1006 DIRTY 前后处理、:912-969 的绑定/清屏/遍历/逐 pass 提交、
  :980-989 OM 回读、:938-939 的光照注入门。
- 现行 draw **不**含 D3D 状态捕获：由调用方 `renderer.cpp` :125/:136 的 `D3D11StateCapture` 负责恢复
  （`d3d11_util.cpp` :248-277 捕获 / :293-329 恢复）。
- **没有任何一步去读 Actor**：待画对象全部来自 `m_graph_object`。

## 3. 取景现状与相机问题（AFP2 的主要未知）

- 锚点 = UI3D 相机 `worldToCam` 第 3 行 × `Studio_Depth=485`（:818-827）；Actor 模式把**根 local 每帧写到锚点**
  （:849）、按蒙皮包围盒再居中（:866-870）、缩放 0.35（:862）。**这正是新路线不允许做的**。
- 相机侧唯一入口：`UI3DSceneManager::SetCameraPosition/SetCameraRotate/SetCameraFOV`（`UI3DSceneManager.h` :46-48）。
- **投影来源未定**：`SetupAndDrawPass` 不带相机、不写投影，`src/` 里唯一的相机读取就是 :823。所以投影用的是
  "留在那里的引擎状态"。候选：`ui3d->camera`、UI3D 的 `viewFrustum`（:68）、或渲染器状态里的相机。
- **判据已进 AFP1 的包**：`SCOPY FP camera` 行给出 `NiAccumulator::camera`（生成这些 pass 的累加器**启动时**用的相机）
  与 `ui3d->camera`／`Main::WorldRootCamera()` 的指针同一性。`accumulator-is-ui3d=1` → 直绘路线按"相机适配"设计；
  `=0` → 退路是在 draw 窗口内临时改根变换并逐项恢复（PRD §6.6 允许，但须写明同步与恢复，且不得暴露给行为/物理 worker）。
- `SCOPY FP figure` 行给出被摄体 3D 根的 `world-t`／沿视轴深度／离轴距离／`eye-distance`／`fade`：相机适配要把
  离轴距离驱动到 0 并选定深度；`fade=0` 会让引擎跳过该节点（`character_clone.cpp` :103-110），必须先解掉。

## 4. 可复用缝（最小重构，尚未实施）

抽出 `src/render/studio/figure_passes.h`，不含 `CharacterClone`/`SceneGraphCopy`：

```cpp
struct FigurePassRequest
{
    RE::NiAVObject* root;
    RE::BSShaderAccumulator* accumulator;
    REX::W32::ID3D11DeviceContext* context;
    const RenderTarget& target;
    const CommonStates& states;
    RE::BSLight** light_slots;
    std::size_t light_count;
};
struct FigurePassStats { std::uint32_t drawn; std::uint32_t renderer_data_null; bool om_matched; };

FigurePassStats draw_figure_passes(const FigurePassRequest& request,
    std::function<void(RE::BSRenderPass*)> const& before_pass = {},
    std::function<void(std::uint32_t)> const& after_batch = {});
```

函数体 = :912-969 原样 + :980-989 的 OM 回读 + :1006 置位 + :891 复位；`m_graph_object`→`root`、
`studio_lights/inject_lights`→`light_slots/light_count`、`m_om_check_logged`→调用方持有、`m_draw_frames`→统计；
两个回调承接现有探针缝（:962／:972）。两条路线各自只提供 request。

## 5. 风险清单（Actor 自持图带来的差异，逐条带锚点）

1. **线程/双写**：draw 在渲染回调里跑（`render_hook.cpp` :75-80 → `renderer.cpp` :190-193）；活 `data3D` 的图由引擎
   自己 tick/re-home（:776-781），而快照路线刻意躲开引擎更新（:838，`scene_graph_copy.cpp` :1189 清控制器/碰撞/userData）。
   :849/:853 的逐帧写入会与动画/Havok/SMP 抢写。
2. **剔除与 fade**：遍历是我们自己的（:943），但世界里的 Actor 仍受剔除/fade 支配（:103-110）；装配门还依赖
   "引擎渲染过它"（:683-690）。
3. **共享 pass 对象被改写**：:507-519 改的是引擎 pass 的字段，:524-528 就地还原——前提是该 pass 本帧没在别处被提交；
   `scene_graph_copy.cpp` :276-281 已把"共享 pass"列为审计项。
4. **生命周期**：现行唯一引用是 `m_graph_object`（`character_clone.h` :46，`character_clone.cpp` :881），只在 Actor 模式
   从 `loadedData->data3D` 取一次（:658-659）并在摘图时清空（:781）。Actor 路线必须**每帧重新解析**根，不跨帧持指针
   （HKX43 悬空读的教训）。
5. **双宿主**：`:781`（`data3D=nullptr`）与 `:786`（DetachChild）存在的目的就是阻止 :776-780 的引擎 re-home；
   保留 `data3D` 意味着 Actor 也会在世界里被画——**未决测量**：数一下每帧 `SetupAndDrawPass` 的调用次数，
   确认它是否同帧被世界提交流程画过。
6. **账本目标翻转**：特效着色器的 pass 生成可能翻 `DIRTY_RENDERTARGET`（:236-240）；快照路线靠剪掉特效对象
   （:230-290）规避，而 Actor 图带着这些对象——剪枝不可用，只剩 :891/:1006 的缓解。
7. **灯光**：注入改写 `pass->sceneLights`（:509）并补壳（:511-519）；rig 的 key/fill 偏移是按"相机沿 −Y 看"调过的
   固定 rig-local（`studio/light.cpp` :281-289），锚点一旦改成 Actor 世界坐标就会系统性偏；`park()` 仍须每 draw 出口执行。
8. **装配门/census 读的是真 Actor**：:996-1000 的普查、:683-690 与 :727-757 的门限会开始描述真实 Actor。
9. **探针身份检查会静默失效**：`probe_at_pass_submit` 比的是缓存的几何指针（`scene_graph_copy.cpp` :868）并对克隆做
   身份校验（:624/:821/:866/:879）——换路线后它只会静默 no-op，不会报错。

## 6. AFP2 待办（不依赖 AFP1 结果的部分已明确）

### 6.0 先说一个被 AFP1 逼出来的设计张力（下一会话的直接后果）

AFP1 判读给出：**泊深 = 世界里看不见、摸不到，但图不产出最终姿态**；**站玩家处 = 在引擎动画作用域内**。
于是"隐藏"和"有姿态"在 v6.25 的泊位机制下是互斥的。三条出路：

1. **held-pinned + 世界侧 pass 抑制**（本仓已有实测机制）：v6.19/v6.22 证明过"把属于 P 的 pass 在世界流里
   透传抑制"（`is_p_descendant` 纯祖先链判据 + 预热窗），当时用于隐藏游离克隆。把这条用在"站在作用域内
   的傀儡"上，得到"引擎照常动画它、世界侧看不见它、摄影棚照常画它"——**这是目前最完整的一条**，
   代价是抑制器要跟着 Actor 的图（而非克隆的图）走，且 R07 要给出世界副作用的数值证据。
2. **换隐藏位置**：把傀儡放到视野外但在 LOD 集合内（另一个房间/遮挡后）。要实测 cull 与动画作用域是否
   真的同一件事——若不是，这条路最省事。
3. **自己驱动 generate**：引擎对 culled actor 跳过的是"把求值结果写进 `generatorOutputs`"那一步（HKX37/38
   证明 update 在跑：clip 时间、状态机、OAR 决策都在动）。若 placement 之外还有必要，就去查
   `hkbBehaviorGraph::Generate` 这一层（OAR 正是钩它，`Hooks.cpp:211`）能否由我们按帧调用——**风险高于前两条**，
   放在最后。

下一步的顺序因此是：先由 placement 实验判定"空输出是不是泊位造成的"；是 → 走第 1 条并为其取证据；
不是 → 先查第 3 条的入口，再决定路线前提。

### 6.1 其余待办

1. **应用阶段**：`read_pose_track` → `score_pose_order`/`decide_pose_order`（已单测）→ 写节点 local + 一次自顶向下重算。
   写路径照抄已验证写法（`animation_source.cpp` :397-418）：`ni_local_from_pose(..., transposed=false)`，**保证 NIF 自身缩放**
   （pose 的缩放退化时保留节点原值），然后 `recompute_subtree_worlds`（`snapshot_transform.h` :107）。
   `decide` 不 `match` → 如实阻断并打印 `reason`，不回退到"最强 clip"或自制 idle。
2. **控制清单**（R04 语义）：推图开／关（F12 已有）、姿态应用开／关、冻结全部／单步（一次声明 dt + 一次应用）／继续、
   标记（F11 已有）、相机取景／旋转、隐藏／销毁。键位需重排并在 manifest 写明——**旧路线的 F2/F3/F4/F7/F8 在新路线包中应让位**。
3. **A0–A5 观测点**：推图前／求值后／应用后／物理后／pass 前／draw 后，每点带上 3D generation 与骨表身份。
4. **显示侧**：按第 4 节抽缝；取景策略等 AFP1 的 `camera` 判据。
5. **未决测量**：(i) 投影来源（AFP1 `camera` 行）；(ii) 保留 `data3D` 时世界提交流程是否同帧画它（数调用次数）。
