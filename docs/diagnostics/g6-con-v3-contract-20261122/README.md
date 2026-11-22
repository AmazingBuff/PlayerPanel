# C-G6-CON v3：域方向规则升级——currentDefaultState 四域判定（预注册契约，2026-11-22）

> **本文在实现前落盘，判据不看运行结果。** 前置：[C-G6-CON v2 run1](../g6-con-v2-contract-20261122/README.md) CON-A' 成立但 iState 粒度限制记录在案（魔法就绪域未跟随）；[魔法域取证追加 §九](../g6-con-v2-contract-20261122/README.md) 落定四域签名与两个新驱动事件。本契约是 v2 方向规则的**修订轮**——机制（prime → dispatch、预算、源只读）与判据（CON-P1..P6）原样继承，只升级方向判定与注入集。

## 一、机制假设（H-DOMAIN，运行前固定）

`currentDefaultState` 是域主开关（收鞘=0、拔刀=1、魔法就绪=2、过渡=11），完全可分四域。用 (候选, 源) 的 `currentDefaultState` 对替换 v2 的 iState 对后，方向派发将覆盖魔法就绪域——候选 clip 家族在三域内均与源归一化相等。

## 二、方向规则（预注册，替换 v2 §2.2）

### 2.1 四域域值

| 域 | `currentDefaultState` | `iState` | 源 clip 家族（取证形态） |
| --- | --- | --- | --- |
| 收鞘 MT | 0 | 0 | `{MT_Idle}` |
| 拔刀 1HM | 1 | 9 | `{1HM_Idle, MLh_Idle, SCAR_1HM_Ready_Dummy*}` |
| 魔法就绪 | 2 | 9 | `{MLh_Idle, MRh_Idle}` |
| equip 过渡 | 11 | 瞬态 | （~0.6 s，判定时跳过） |

### 2.2 方向判定表（(候选 cds, 源 cds) 对）

| 候选 | 源 | 派发事件 | 取证依据（[v2 §九](../g6-con-v2-contract-20261122/README.md) + 一次取证） |
| --- | --- | --- | --- |
| 1 | 2 | `MagicForceEquipRight`（476） | 魔法域取证窗口 1 @23:45:38.254 |
| 2 | 1 | `WeapOutRightReplaceForceEquip`（417） | 魔法域取证窗口 1 @23:45:45.558 |
| 1 或 2 | 0 | `Unequip`（144） | C-G6 v1 run2 取证（1HM→MT 与魔法→MT 同走卸下链——**本行 2→0 组合为新假设**，判据覆盖） |
| 0 | 1 | `WeapEquip`（2） | C-G6 v1 run2 取证 |
| 相等 / 源 cds∉{0,1,2}（含 11） | — | `none`（仅 prime，不盲派） | 预注册保守规则 |

其余域对（如 0↔2 直换）为未取证组合：仅 prime、`dispatch=none` 记录（不猜）。事件 id 仍从候选侧表按名解析（未解析 → `skip-unresolved` 停用该方向）。prime 在 dispatch 之前（v2 纪律不变）。

## 三、机制与 INI

- v125 执行体原样保留：10 词同步、签名检测、32 事件预算、失败变量停用、源只读、`G6 SYNC ARM`/`G6 SYNC` 行。
- 方向判定实现替换：iState 对 → `currentDefaultState` 对（sync 集位置 1）。派发事件表扩为 4 名（`WeapEquip`/`Unequip`/`MagicForceEquipRight`/`WeapOutRightReplaceForceEquip`）。
- INI 键不变：`CandidateDomainSync=true` + `CandidateDomainDispatch=true`——`CandidateDomainDispatch` 的方向判定语义升级为 v3 规则（v2 规则的两域行为是 v3 的子集，无回退价值）。
- `G6 SYNC` 行字段不变（`direction=` 改为域对形式 `1>2` 等，`dispatch=` 同 v2）。

## 四、判据（预注册，继承 v2 §三）

- **CON-P1（主判据，域矩阵扩展）**：**三域**（收鞘/拔刀/魔法）内每域 ≥2 检查点：候选 active clip 集（归一化）== 源 active clip 集（归一化）。魔法域的源形态即取证形态 `{MLh_Idle, MRh_Idle}`。
- **CON-P2（旧域退出）**：每次切换后候选不含前一域任何 clip 节点。
- **CON-P3（写与派发）**：`G6 SYNC` 的域对/派发与 §2.2 表一致；派发后转换链完成（settled 快照证据）。
- **CON-P4/P5/P6**：与 v2 字面相同（源安全 / P1 mini 回归 / 稳定性）。
- **附加记录**：1stP 超集形态；WeaponAdjustment 与结构差集；目视；`currentDefaultState=11` 瞬态是否撞上签名采样（撞上 → `dispatch=none` 行，记录频次）。

**判定**：三域 P1+P2 全过且 P3..P6 成立 → **方向规则 v3 成立，G6-B 收敛扩展至三域**。魔法域 P1 不成立 → H-DOMAIN 证伪，判停（转 CON-B 或深挖魔法域机制，不调参重试）。仅个别方向失败 → 逐方向记录，不整体判过。

## 五、上机协议（CON v3 run 1）

1. 环境：AE 1.6.1170、Save116、`AE-Track2` + 魔法（右手法术一枚，即取证会话用的那枚）；构建标签建议 `v126-g6-domain-v3`。
2. INI：v2 档不变（两键已开）。
3. 场景（F7 背包开着）：
   - **T1** 收鞘 idle ≥3 s → 开包 F7 ×2 → 关包（基线域）
   - **T2** 拔刀 idle ≥3 s → F7 ×2 → 关包（`0>1` → WeapEquip）
   - **T3** 热键换右手法术持出 idle ≥3 s → F7 ×2（`1>2` → MagicForceEquipRight）
   - **T4** 热键换回武器 idle ≥3 s → F7 ×2（`2>1` → WeapOutRightReplaceForceEquip）
   - **T5** 收鞘 idle ≥3 s → F7 ×2（`1>0` → Unequip，覆盖新组合行）
   - **T6** P1 mini 回归（开合 ×2 + F9 ×1）
4. 目视：T3/T4 面板姿态域正确性（一句话）。
5. 回传：日志改名 `CharacterPanel-g6conv3-<日期时间>.log` + 目视 + `[E]` 一句话；判读按 §四。

## 六、边界

- 单运行时 AE 1.6.1170；只验三域 clip 家族收敛；`currentDefaultState` 之外的其他域态（如 11 过渡、战斗态）不在判据内。
- 菜单路径换装的事件名未取证（§九边界）——v3 按域状态对派发，不依赖源侧路径；若菜单路径派发无效，记录为边界（不 FAIL，进 v4 讨论）。
- 注入集变更 = 契约修订；判据不放宽。

## 变更记录

- 2026-11-22：契约落盘（四域签名与 `MagicForceEquipRight`/`WeapOutRightReplaceForceEquip` 来自当日魔法域取证；实现前，判据不看运行结果）。

---

> **运行包**：v126 DLL/PDB + v2 候选档 INI（键面与 v3 相同，`CandidateDomainSync/Dispatch=true`）。v125 备份 `dist/deploy-backup-20261122-v125-v126pre/`；会话步骤见 [v2 MANUAL 追加节](../../../dist/g6-con-v2-20261122/MANUAL.md)。
