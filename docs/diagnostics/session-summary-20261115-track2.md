# 会话总结：2026-11-14/15 会话（Track 2 批次：预注册 → 三次修正 → Run 0/1/2 → 收官）

**范围**：本会话从 `552e492`（K6 收官）之后开始，共 **18 个提交**。分支 `oar-idle3`，结束领先 `origin` **78 提交、未推送**，工作树 clean。完成 [K6 关闭裁定 §十三](../../independent-behavior-graph.md#c-g5-cal-v1) 指定的下一步之一：**Track 2 同资源姿态对照批次（B + C）全程**。本文是会话级索引；判据/哈希以 [Track 2 预注册](g5-track2-20261114/README.md) 为准。

## 一、批次历程

| 阶段 | 提交 | 结果 |
| --- | --- | --- |
| 预注册 | `d9699d2` | Track 2 = B + C（停 OAR 引擎新 MO2 档 + data FNV 分组）；Run 0 七判据 T2-C1..C7；K5-1..8 复用；层/混合转移声明；零构建声明。用户批准档方案（§九决策 1） |
| 运行档 | `c7612ba` | 四个 INI 派生（census/k5 × inject/source），三个与 v101b/v103 登记哈希逐字一致 |
| 离线登记 | `2d7d284` | `AE-Track2` 建档（用户；modlist diff 恰 4 行）；winner-map 赋值 24,204 路径逐行一致（仅 pos 漂移，4 引擎模组 0 内容行）；oar-config 逐字节一致；闭包走链 23/23（**T2-C6**）；双锚点 ✓ |
| Run 0a run 1 | `fa01325` | **INVALID**——dummy `identity=false`（无非 OAR 来源）+ 场景不符 + 启动期 OAR 探测 `[E]`；**发现一：候选资源与 OAR 从来无关**（原版 BSA 定点解码证实 1HM 三件套 = `12A77CD1`/`4DED5EAF`/`C62C233B`，K1-C1 归属更正） |
| 修订 v2 | `36afc26`+`d0e0f59` | 用户批准方案 b：`CharacterPanel-Track2Dummy` 占位模组（逐字节副本到普通路径）；winner diff 恰 1 行、闭包复验 |
| Run 0a run 2 | `2325324` | **T2-C2 恢复 PASS**（dummy `identity=true 8728A54A…`）；仍 INVALID——场景不符（用户所用存档非 Save116） |
| 修订 v3 | `9771641` | 方法失误登记（§2.1 漏判 `RaySense.esp` 805 B 空壳 → Save116 插件表失配"进不了"）；用户批准方案 A：`CharacterPanel-Track2RaySenseEsp` 占位模组 |
| Run 0a run 3 | `0b30afe` | **PASS**——Save116 逐字重验（hands/armor = K2-C1）、missing=0、unequip completed=129 与 Track 1 同值 |
| Run 0b | `e6f2b15` | **PASS——T2-C4 达成：六族跨侧 data FNV 逐字节相等**（K1-C4 NOT MATCHED 反转）；Run 0 收官，断言集合登记 |
| Run 1（候选） | `3e0ea35` | **PASS**——249/249 代、双读全等、116/116 轨、4 审计点齐全；2 条 identity-unreadable（equip 边界 87/88 dummy 懒加载竞态）按 K6 §10.3 IDUN 记账；reconcile 工具 `--track2` 模式（语义澄清，判据文本不变） |
| Run 2（源） | `41a7332` | **PASS**——1065/1065 apply、六族可读身份全部 ∈ 集合、1 IDUN 记账；登记执行偏差（k5-source 档走 SamplingLiveness 直采、无 F7 窗口，指令表述更正） |
| 收官 | `41a7332`+`318184f` | **C 分组账本：六族全 PAIRED**（§19）；批次裁定（§20）；主文档指针同步 |

## 二、方法学新事实

1. **候选侧资源与 OAR 从来无关**：源/候选资源差异的全部来源是源图被 OAR 替换；候选 1HM 三件套实为原版 BSA 内容（SCAR/Elden Rim 等只是复制了同字节文件到替换目录）。
2. **K1-C4 NOT MATCHED 是环境属性**：Track 2 环境（OAR 停用）下六族两侧同 FNV；未来跨环境身份以 data FNV 为准的结论保持。
3. **引擎懒加载竞态形态**：idle 栈淡入边界（equip completed 87/88）dummy binding 瞬时 `0x0`——`K5_Flag_Identity_Failed` 的实际形态，有界（候选 2 + 源 1），IDUN 记账排除出配对。
4. **K1-C1"多选一"归属教训**：FNV 对账命中多路径时，应按"运行时被请求的路径"优先归属（普通路径 > OAR/DAR 目录）。
5. 两侧 binding-index 完全一致（同工程同加载序的确定性表现）——K1-C4"binding-index 非身份"结论不变。

## 三、当前 MO2 / 部署状态

- MO2 档 `AE-Track2`（979 mod 项 + `*RaySense.esp`）：Track 2 专用；主档 `AE` 恢复原状（981 项，未动）。
- 新本地模组（仅 AE-Track2 启用）：`CharacterPanel-Track2Dummy`（dummy 普通路径副本）、`CharacterPanel-Track2RaySenseEsp`（805 B 空 esp 占位）。
- 安装位 `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\`：v103 DLL `8c3857a3…` + **k5-source 档** `21bd938b…`（Run 2 状态）；回退点 `dist/deploy-backup-20261114-v103-run0apre/`、`…run0bpost/`、`…run1post/`。
- Track 2 运行档在 `dist/g5-track2-20261114/`（gitignored，哈希登记于 [build.md](../../build.md)）。

## 四、下一步（下会话入口）

1. 先读 [Track 2 预注册 §十九/§二十](g5-track2-20261114/README.md)（收官裁定与边界）。
2. **门 4 配对/比较协议设计**：输入 = Run 1/2 两侧工件（docs/diagnostics/ `…track2-run1-*.bin`）+ §十九 PAIRED 账本；预注册后另采正式运行（CAL v1 §九 / G5 v2 门 4）。
3. **CAL2 三进程/120 代协议预注册**（控制 B 补 2 个合格进程；独立于门 4，可并行）。
4. 既有挂起：78 提交 push 与否（用户决定）；工作区 AGENTS.md 的 CharacterPanel 条目仍停留在 2026-10-31 交接（可选清理）。

**不产生** `tau_*`；不关闭姿态等价/G5/G6；Track 1 历史裁定全部原样保留。
