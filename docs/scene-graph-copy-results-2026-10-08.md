# 场景图复制实验回传（S0，2026-10-08）

本文件按 [回传模板](scene-graph-copy-results-template.md) 记录 S0 的结果。
2026-10-09 复核：保留原始测试观察；静态显示初步成立，Q09／Q10 的资源回收阻塞。
现行验收以 [PRD 0.6](player-panel-prd.md) 和 [下一轮测试单](scene-graph-copy-next-test-2026-10-09.md) 为准，不把停放后的无崩溃记为安全销毁。
实验标识 `CharacterPanel-scopy-S0-2026-10-08`，运行于 Skyrim AE 1.6.1170 + SKSE 2.2.6，
Community Shaders 同时启用，本机 MO2 独立测试 mod。

## 环境

- 测试日期：2026-10-08（22:36–23:36 多个会话）
- 插件横幅：`SCOPY BUILD CharacterPanel-scopy-S0-2026-10-08 runtime=1-6-1170-0 … hotkeys=F7-copy F8-draw F3-rotate F4-release`
- 身体／骨架：CBBE 与 UBE 两种身形存档各验一轮
- 物理：CBPC／FSMP 未建立独立驱动（S0 范围外）
- 热键占用：F9/F10 被其他 mod 占用，故旋转／释放改为 F3/F4

## 结果

| 问题 | 结果 | 证据 |
| --- | --- | --- |
| Q01 克隆返回 | 通过 | `CLONE-RETURN copy=<非空>`，`nodes=470/geoms=23`（CBBE）与 `1851/123`（UBE）均返回独立根；`root == source`、`root->parent` 非空两种失败形态均未出现 |
| Q02 源角色不变 | 通过 | `SOURCE-DIFF root-same=true parent-same=true object-set-same=true`；退出背包后世界角色无位移／转向／隐身 |
| Q03 引用隔离 | 通过 | `AUDIT` 十二项全 0（shared-nodes/skins/properties/buffers/passes、external-roots/bones/transforms/pass-geometry/fade-nodes、invalid-arrays）；`unbound-slots=0` |
| Q04 数量完整 | 通过 | `CENSUS source-nodes == copy-nodes`、`source-geoms == copy-geoms`、skins／dynamic 均对等 |
| Q05 正确绘制 | 通过 | `SCOPY DRAW passes=47`（剪枝后）、人物完整着装显示；`SCOPY FRAME pruned-nodes=71` |
| Q06 独立旋转 | 通过 | `SCOPY KEY rotate pressed (VK_F3 -> scan 61)` + `ROTATE 90→360`；世界角色不跟随旋转 |
| Q07 独立存活 | 通过 | 关闭／重开背包后副本保持捕获时的外观与姿态，可继续独立旋转 |
| Q08 手动换装刷新 | 通过 | 重新 F7 捕获后 `READY`，装备与外观与当前状态一致 |
| Q09 视角／切场景／读档 | 显示切换通过；回收阻塞 | 读档与回主菜单触发 RELEASE + PARK，无旧人像；PARK 没有销毁／回收资源 |
| Q10 连续生命周期 | 有限循环无崩溃；生命周期未通过 | 14 次捕获后 cap 只丢记账条目；实际未释放的图继续累积，不能证明资源稳定 |
| Q11 物理几何与源物理恢复 | 通过 | 测试 B 在 CBPC／FSMP／3BA／UBE 实际环境下重跑：副本内头发／衣物保持捕获姿态（S0 无独立驱动，**冻结即预期**），几何无缺失、无绑定回原角色、无姿态飞散；源角色物理在其后的世界动作中恢复正常 |
| Q12 外观完整 | 通过 | CBBE 与 UBE 两种身形下人物主体、脸、头发、当前装备服装与武器均正确显示；换装后重新 F7 捕获，体型、肤色、妆容／纹身均保留 |

未实现（不作为 S0 通过项）：HKX 动作驱动、CBPC／SMP 独立注册与推进；S1–S4 见
[后续关卡](scene-graph-copy-validation.md#后续关卡本轮未实现不作为-s0-已通过项)。

## 测试 A

复制、绘制与显示切换：初步通过；安全析构与回收：阻塞。覆盖 F7 复制／审计、F8 绘制、F3 旋转（90→360° 逐次配对）、
F4 释放、背包开关、连续 14 次捕获、读档与回主菜单转换；CBBE 与 UBE 两种身形各一轮，
停放构建的验证会话零崩溃。`PARK` 上限触发时只丢记账条目（`no destruction attempted`），没有限制实际分配数量。

## 测试 B

换装、源图变化与外观／物理模组：**通过**（用户确认"所有行为都通过"）。

- 换装后重新 F7 捕获，副本完整匹配新装备，体型、肤色、妆容与纹身保留（Q08／Q12）；
- 不重新捕获时，副本保持冻结快照的旧装备与旧姿态，可独立旋转（Q07）；
- 关闭背包后让世界角色转身、蹲伏、播放动作，重新打开背包副本仍保持旧姿态；
- 在 CBPC／FSMP／3BA／UBE 实际环境下重跑：副本内物理头发／衣物不继续模拟，**属于 S0 范围
  内的预期行为**（无独立驱动，见[关卡表](scene-graph-copy-validation.md#后续关卡本轮未实现不作为-s0-已通过项)的
  S3）；几何无缺失、无绑定回原角色、姿态无飞散，源角色物理恢复正常（Q11）。

## 本轮修复的关键缺陷

| 缺陷 | 根因 | 修法 |
| --- | --- | --- |
| F3 无反应 | 部署的 DLL 早于按键重映射 | 重映射后 `SCOPY KEY`／`hotkeys=` 日志可自证 |
| CBBE 无法 `READY` | `SOURCE-UNCHANGED` 用**精确浮点相等**比较逐节点变换，暂停帧内几个 ULP 的漂移即被判为"源图被改" | 拆分为 `SOURCE-DIFF`：只有对象集合变化才 BLOCKED |
| F7 重捕即崩 | 在渲染回调内**就地析构**原生克隆图 | 改为停放，见下条 |
| F4 释放崩、回主菜单崩 | 三种已测试释放路径同一崩溃签名，根因未定位；不证明所有时机都不可销毁 | 当前只停放，不计为回收修复；析构体不 reset 也不能阻止成员自动析构 |
| UBE 人物显示不全（只剩武器） | 玩家图里混入 100 个特效／碰撞／光源对象，`passes=106` 而身体几何从未被世界渲染器画过 | 剪枝非人物对象（粒子、碰撞辅助、法术特效、泄漏光源）→ `geoms 123→51`、`passes 106→47` |
| 面板外多一个烛光术光源 | 副本携带了法术的灯 | 剪枝按 RTTI 名剔除光源 |

## 已知遗留

- 停放的图**有意不释放**，每次 F7 累积一份（UBE 量级约数 MB）。干净销毁需要单独一轮定位引擎在
  `BSFadeNode` 析构中踩空指针的机制（线索：崩溃现场 `RSP` 上的 `BSFlattenedBoneTree "NPC Root [Root]"`／
  `skeleton_female.nif`，只是线索，尚未证明全局注册根因）。
- 正式性能测量（PRD §5.3）与灯光／世界光照一致性微调仍待做。
