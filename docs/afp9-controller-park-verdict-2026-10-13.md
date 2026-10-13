# AFP9 verdict 9：面板稳定；"能感到克隆体"= 角色控制器没跟着引用搬（已修复）

日期：2026-10-13/14。构建：`CharacterPanel-scopy-AFP9i-…`（DLL SHA-256 `43cf80a0…`，用户上机确认的版本）。
前序：[verdict 8 蒙皮帧门](afp9-skin-cache-verdict-2026-10-13.md)；证据日志见 `docs/diagnostics`。

## 1. 用户确认与本轮唯一遗留

- **F6 直绘已稳定**：多次进出背包、反复开面板，面板内人物正常（verdict 8 的 `invalidate_skin_cache` 生效）。
- 遗留：**克隆体在玩家附近仍可被"可交互信息"感知**（fade 已为 0、画面不可见，但激活提示仍能命中）。

## 2. 根因：`SetPosition` 的第二参数没传

历史路线（被清理掉的 `tools/m0_proto/proto_pinstance.cpp`，v6.25 已验证"交互/对话/碰撞天然不可达"）的写法：

```cpp
// Actor's override carries the character-controller update flag
// (true = warp the controller with the ref).
a_clone->SetPosition(RE::NiPoint3{ pos.x, pos.y, pos.z - Park_Depth_Below_Player }, true);
```

CLib 签名确认语义：`Actor::SetPosition(const NiPoint3& a_pos, bool a_updateCharController)`。
我们一直传 `false`——**只挪引用、不搬运角色控制器**，于是：

- 碰撞/激活目标留在玩家身边 → 可交互提示仍能命中（用户本轮报告）；
- 引擎的 3D 推导源（控制器）也停在原处 → 这正是"只写 world/local 都会被每帧推回 spawn"的底层原因。

## 3. 修复（AFP9j）

1. `park()` 改为在引用确实移动时传 `a_updateCharController = true`（`warp`），并把 `warp=0/1` 写进 `park-3d` 证据行；引用未动时传 `false`，避免每帧 warp 控制器。
2. 保留了此前的每帧重泊与取景（与 warp 兼容）；预期副作用是正向的：控制器搬过去后，引擎自身的推导也会把 3D 留在泊位，`world-t-before` 不再回到 spawn。

## 4. 待验证（AFP9j）

1. 玩家身边**不再出现可交互提示**（十字准星不再指向克隆体）；画面依旧不可见。
2. `park-3d` 行：首次 `warp=1`，此后 `warp=0`；`world-t-before` 应稳定在泊位坐标（不再回 spawn）。
3. 面板侧不得回归（反复开关仍完整人物）。

## 5. 仍开放的遗留

- 摄影棚灯光亮度/与世界光照一致性。
- 98→116 覆盖审计的 unreachable 六项重新取证。
- 傀儡的 facegen／体型同步（F00–F04／BM00–BM03）与物理（R08）仍属后续关卡。
