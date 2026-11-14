# 会话总结：2026-11-14 第三会话（K6 计划评审 → K5 仪器实现 → v103 两侧运行 → K6 关闭）

**范围**：本会话从 `216e2a9`（第二会话总结）之后开始，到 K6 关闭登记结束，共 **8 个提交**。分支 `oar-idle3`，结束时领先 `origin` **60 提交、未推送**，工作树 clean。按 [K6 计划](g5-k6-plan-20261114/README.md) §八 顺序完成：计划评审通过（用户批准两轨制）→ 实现预注册 → K5 三仪器 → v103 构建部署 → 候选/源两侧各一次运行 → 离线对账 → **K6 关闭**。本文是会话级索引；判据/工件哈希以 [K6 计划 §十一/§十二/§十三](g5-k6-plan-20261114/README.md) 为准。

## 一、K6 计划（评审通过，`4cf56d5`）

- **OAR 资源策略 = 两轨制**：Track 1（K5 仪器资格采样）= A 冻结断言 + C 分组记账，现模组不动；Track 2（姿态对照批次，另批另立预注册）= B 关闭动态替换 + C 分组，**待用户批准 MO2 档**；否决 B 的回退 = 门 1"无法建立可观测配对，停止正式路径"。
- K5 仪器范围/格式/预算/判据 K5-1…K5-8 预先固定；实现预注册（`d60849d`）冻结触点/键名/字节布局（K5ClipRecord 124 B、候选记录 5,664 B cap 384、源记录 5,652 B cap 1,400、三工件预算）。

## 二、K5 仪器实现（`9e83ff8`，v103 `e23be51`）

- 新键 3 个（`K5FullSample`/`K5LayerAudit`/`K5Reconciliation`，全默认 off，off 输出与 v102 逐字节一致）；CPK5 格式 v1（K3 布局 + 逐 clip 身份列：stable-id 指针/duration/data/blockOffsets/map FNV）；4 审计点层审计（vtable 精确分类）；RECON 行 + JSON 账本；身份缓存 64 项（哨兵 CAS 发布协议，命中校验 duration/map_count/spline-data-bytes）。K3/K1/控制 B 代码零改动；复用 `read_animation_resource_identity`。
- 部署：备份 v102 → v103 三件套（DLL `8c3857a3…`，身份 `…-v103-g5k5-fullsample-9e83ff8e8f-…`）；inject 档为安装位；两条运行档在 `dist/g5-k5-20261114/`。登记一条部署偏差（首次安装 INI 写成旁路文件，当即重装核验，未进任何游戏会话）。

## 三、两侧运行与 K6 关闭（`1aa7af9` + 收官提交）

| 运行 | 结果 |
| --- | --- |
| **run 1 候选侧**（inject，panel 链，22:00） | 249/249 记录、双读全等、116/116 轨、零丢失；4 审计点 `missing=0`；**7/7 data FNV 命中 v101 登记集**；equip 87–98 层混合带；K5-1…8 PASS（K5-5 带缺项清单：unknown 类 328 行 = `hkbBlendingBehavior` 等无 CommonLibSSE 布局类） |
| **run 2 源侧**（source 档，F7 probe 20 s，22:29） | 906/906 记录、seq 连续、TSC 单调、全生命周期覆盖（equip 56 条 + unequip 67 条 + 收鞘 idle）；**7 个命中 v101b 登记集 + 2 个声明缺口**（MT_Idle 新 FNV `08E65508…`/`C95CED03…`，K1-C4 栈漂移再现）；旁注 b3/b4=906/906 第二会话复现 |

**K6 关闭（限定范围）**：K5 仪器资格成立；CAL v1 §三 K1–K6 形式齐备，但姿态等价与门 3 原生校准仍不可启动——**下一步 = Track 2 预注册（待用户批准关闭 OAR 替换的 MO2 档）与 CAL2 协议预注册**。带缺项转移清单：①逐骨/通道权重与 blend 语义；②MT_Idle 两新 FNV 并入声明清单；③控制 B 三进程缺口。`tau_*` 未产生，G5/G6 不关闭，装载 teardown NO-GO 不变。

## 四、方法学新事实

1. 运行时 `animationName` 是完整动画路径（`Animations\1HM_Equip.hkx`），`hkbNode::name`（+0x38）才是裸名（`DG_1hmEquip` 等）——K5 工具以裸名匹配、data FNV 为身份。
2. 跨运行 binding-index 漂移再现（SCAR dummy 15078→15073、MT_Idle 11909→11915）——K1-C4"binding-index 非身份"再次成立。
3. MT_Idle 源侧资源第四、五个版本（本轮 `08E65508…`/`C95CED03…`，此前 9F6236C9/95E20CF7）——OAR 栈漂移比 census 登记的更宽，Track 2 的资源断言必须关闭该栈或包含全部版本。

## 五、当前 MO2 安装状态（`E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\`）

| 文件 | 字节 | SHA-256 |
| --- | ---: | --- |
| CharacterPanel.dll | 2,007,552 | `8c3857a39479c172f894c73449677adf3bb19dbd6baf6718609d0d85ea58e5ef` |
| CharacterPanel.pdb | 51,367,936 | `49581217b75ff7a93646c8ff106a045584071eaea54f214cfdc4627f83045566` |
| CharacterPanel.ini | 12,322 | `21bd938b94268c3da7a81ebaa6bdee75ad4d49e9c4c36114c052142fc626ec2b` |

即 **v103 诊断档（source 运行档）**；inject 档在 `dist/g5-k5-20261114/CharacterPanel-inject.ini`（`76f2bebb…`）。`config/CharacterPanel.ini` 保持全键默认 off。部署备份 `dist/deploy-backup-20261114-v102-v103pre/`（v102 三件套）。

## 六、下一步（下会话入口）

1. 先读 [K6 计划 §十三 关闭裁定](g5-k6-plan-20261114/README.md#十三k6-关闭裁定) + [CAL v1](../independent-behavior-graph.md#c-g5-cal-v1)。
2. **Track 2 预注册**（姿态对照批次）：关闭 OAR 替换的 MO2 档方案须用户批准；批次自带 census 重登记 + hands/drawn/armor 重验 + 资源断言集合（含 MT_Idle 新版本处置）。
3. **CAL2 预注册**（三进程/120 代驱动协议；控制 B 补 2 个独立进程）。
4. 可选清理：60 提交 push 与否由用户决定。

**不产生** `tau_*`；不把 K6 关闭说成姿态等价或 G5 通过；不自动 push。
