# 诊断证据保留与恢复

更新：2026-11-04。G1/G2 有界通过，G3-A 与 G3-B/F 相位回放保留；G4 A/B 保留、C 与旧 typed-setter 零读数受 [audit30](../behavior-graph-construction-audit.md#audit30) 仪器缺陷影响，修复后待重跑。v67/v68 已有源采样入口活性和多个采样 tid 的记录，但 **E1 完整证据未闭合**；63856 是场景应用入口，不能称 poseLocal 末写或输出完成。本轮修复跳板、多生产者传送和关闭对账，尚未部署/上机，详见 [E1 当前边界](../independent-behavior-graph.md#g5-e1-evidence-repair)。最新记录安装 v68，最近记录 INI dormant，本轮未重验安装。E1 剩余完成点/寿命/同代快照仍需取证，E2/E3 尚未开始；G5 门 1 NOT_READY、未冻结/未通过，G6 待验，装载 teardown NO-GO。

## 本次退役范围与理由

只从工作树移除下列四份 S4/S5 settled-variable 负运行日志；其写入成功但相位不动的结果已有日期摘要，后续 A1/S6/F 已解释并验证成功事件路线。共 **289,377 个 CRLF 工作树字节（约 283 KiB）**；Git 历史未清除，不隐藏复制、不合成新日志。退役不撤销负结果，也不把曾经的假说升级为当前机制结论。

不可变恢复提交：`ae5dac28356d0ebb1a5fd754ddb4882a8dae26f3`。删除前逐份校验工作树 SHA-256/字节数、该提交中的 blob/OID、LF blob SHA-256/字节数，以及工作树只做 CRLF→LF 后与 blob 完全相等。

## 原工作树身份（CRLF）

| 文件 | 字节 | SHA-256（完整） |
| --- | ---: | --- |
| [CharacterPanel-g3bs4-r1-bequipequip-20261031-1645.log](#g3bs4-r1) | 72,238 | `685ed235ad76e53569cfa120ce74db5a3cefc02781e8b5978c4cff3138d6f319` |
| [CharacterPanel-g3bs4-r2-isunequipping-20261031-1707.log](#g3bs4-r2) | 72,231 | `985f210e7b799db013ab2a3c92d24f490b64d3c272f044c0773d17f43566598e` |
| [CharacterPanel-g3bs4-r3-pitchmanualoverride-20261031-1739.log](#g3bs4-r3) | 72,261 | `54436ac55a7ed5c89dc03d05136f58dd3473adf80792d9e8b77d08874bd52beb` |
| [CharacterPanel-g3bs5-two-write-20261031-1753.log](#g3bs5) | 72,647 | `c387aa9a319bf8b477b400847a9d1fe701aa107d530e191e2124a62dc2fc5c1d` |

## 不可变 Git blob 身份（LF）

| 文件 | 字节 | SHA-256（完整） | Git blob OID（SHA-1） |
| --- | ---: | --- | --- |
| [CharacterPanel-g3bs4-r1-bequipequip-20261031-1645.log](#g3bs4-r1) | 71,875 | `26de0f7a07c0b02eb9329e0fce8ba224f525fc038399cb813cb63fb16cb42fe1` | `55b6f8f45da4ef92b2ad908ba6683d277d7df557` |
| [CharacterPanel-g3bs4-r2-isunequipping-20261031-1707.log](#g3bs4-r2) | 71,868 | `06f5eea504d9cc96d226d2b413ad5120b9eeb19f3c7a8967e2134171914baee8` | `c31b605242e010068c49439d6463c529692d7eb4` |
| [CharacterPanel-g3bs4-r3-pitchmanualoverride-20261031-1739.log](#g3bs4-r3) | 71,898 | `b11078a5b4648af55b70f43918930f1d282ed52f98393d43cc41ab5886bbc6ba` | `f4c0a443ceeb007e003ae05d0c2611fba3c377d7` |
| [CharacterPanel-g3bs5-two-write-20261031-1753.log](#g3bs5) | 72,281 | `727c3bf970ba4e471f5771798e0e0eade280e8fa6465050c33e5830765fc72e1` | `ad2db9c5dfb8a76a0757773b2dca905035bc947e` |

CRLF 工作树与 LF Git blob 是两套不同字节身份。`git show` 查看原 blob；将来 checkout/restore 时 Git 的换行转换或终端重定向可能改变字节哈希，不可拿 LF 哈希直接校验 CRLF 文件，也不可将 OID 当 SHA-256。

## 逐轮结果与稳定入口

<a id="g3bs4-r1"></a>
### S4 r1：bEquipUnequip（2026-10-31 16:45，v55）

[日期摘要](../independent-behavior-graph.md#c-g3b-s4-unequip-variable)：settle 第三次复现，target49/settled63、nodes111；BOOL setter true、readback1，121 点保持1，但 default 恒1、nodes111、运动119/119、source-drift=none。只否定该时机的单变量写入。

<a id="g3bs4-r2"></a>
### S4 r2：IsUnequipping（2026-10-31 17:07，v55）

[日期摘要](../independent-behavior-graph.md#c-g3b-s4-unequip-variable)：settle 第四次复现；BOOL 写成功/读回1且121点保持1，default1/nodes111、运动119/119、source-drift=none。镜像变量的该时机单写未触发卸下；17:01 未开面板会话不构成判定。

<a id="g3bs4-r3"></a>
### S4 r3：PitchManualOverride（2026-10-31 17:39，v55）

[日期摘要](../independent-behavior-graph.md#c-g3b-s4-unequip-variable)：settle 第五次复现，类型实测 BOOL；写成功/读回1且121点保持1，default1/nodes111、运动119/119、source-drift=none。三名单写阶梯全负，按预注册停止，不证明所有外部输入无效。

<a id="g3bs5"></a>
### S5：两写合取（2026-10-31 17:53，v57）

[日期摘要](../independent-behavior-graph.md#c-g3b-s5-two-write)：settle 第六次复现（target49/settled63、default1/nodes111）；先 INT32 iRightHandEquipped=1，再 BOOL bEquipUnequip=1，setter 均 true、readback 均1且121点持久，default1/nodes111、source-drift=none。只否定该时机两写合取；后续成功事件路线见 [S6](../independent-behavior-graph.md#c-g3b-s6-unequip-event-retry)/[F](../independent-behavior-graph.md#c-g3b-f-full-replay)。

## 只读恢复查看

从仓库根目录执行以下命令可查看每份不可变 LF 原文，不修改 HEAD、index 或工作树：

```powershell
git show 'ae5dac28356d0ebb1a5fd754ddb4882a8dae26f3:docs/diagnostics/CharacterPanel-g3bs4-r1-bequipequip-20261031-1645.log'
git show 'ae5dac28356d0ebb1a5fd754ddb4882a8dae26f3:docs/diagnostics/CharacterPanel-g3bs4-r2-isunequipping-20261031-1707.log'
git show 'ae5dac28356d0ebb1a5fd754ddb4882a8dae26f3:docs/diagnostics/CharacterPanel-g3bs4-r3-pitchmanualoverride-20261031-1739.log'
git show 'ae5dac28356d0ebb1a5fd754ddb4882a8dae26f3:docs/diagnostics/CharacterPanel-g3bs5-two-write-20261031-1753.log'
```

## 继续保留的关键证据

- 所有崩溃日志、关键通过/边界和原视觉证据均保留；没有执行其它日志删除。原始日志应与匹配工件/设置及[日期运行记录](../independent-behavior-graph.md#verification-matrix)一起判读。
- E1/E2/E3 保留源观察路由及负结果的边界，防止把“未观察”推广为全路由不存在；[S6](CharacterPanel-g3bs6-unequip-event-retry-20261031-2123.log)/[F](CharacterPanel-g3bf-full-replay-20261031-2138.log)保留正确事件与完整相位回放的直接证据。
- G4 [A](CharacterPanel-g4a-oar-context-20261031-2258.log)/[A2](CharacterPanel-g4a2-clip-identity-20261031-2330.log)/[A3](CharacterPanel-g4a3-scan-anchor-20261031-2345.log)、[A/A v1](CharacterPanel-g4b-aa-baseline-20261101-2355.log)/[A/A v2](CharacterPanel-g4b-aa-v2-20261101-2358.log)/[B+C](CharacterPanel-g4bc-run-b-20261101-0028.log)全部保留，供重核明细 cap、numeric72/类型7、关闭的源事件捕获、基线方差和装备族选择范围。
- 更早的144份文档清理有独立的[历史来源/恢复索引](../history-sources.md)，恢复提交不同，不与本次四日志混用。


## C-G5-E1 首轮运行日志（2026-11-04）

> 原件哈希与字段保留；旧 `completion` 实为 63856 应用入口，完整 E1 收官解释已撤回。当前修复与补证要求见 [E1 边界](../independent-behavior-graph.md#g5-e1-evidence-repair)。

- [CharacterPanel-g5e1-liveness-first-20261104-0010.log](CharacterPanel-g5e1-liveness-first-20261104-0010.log)（221,958 字节，SHA-256 b5318dcb1e56bd625914b179f2467c6bd23cb60216e2279062572fe1a61bf6d6）：v67 SamplingLiveness=concurrent 窗口，00:12:00.333 武装、00:12:20.345 到期（995 帧）。判读、计数与两条独立事实见[首轮运行](../independent-behavior-graph.md#c-g5-e1-run1)；原文保留，不改写。

- [CharacterPanel-g5e1-liveness-on-mode-20261104-0021.log](CharacterPanel-g5e1-liveness-on-mode-20261104-0021.log)（5,669 字节，SHA-256 c0a07a8559c2729611e7c521a98c56905bd10fc4f242ee164f03991d16bc7635）：v67 SamplingLiveness=on，游玩态 F7 窗口 19.99 s / 748 帧，引擎本地循环调用 63589 共 161,006 次、`flag=0`。判读见[补充运行 A](../independent-behavior-graph.md#c-g5-e1-run2)。

- [CharacterPanel-g5e1-liveness-v2-resumed-20261104-0031.log](CharacterPanel-g5e1-liveness-v2-resumed-20261104-0031.log)（291,454 字节，SHA-256 603115178c0dc56747e2dcc78dd0aa5c9b0955ac5ab08395e3e7f7f5730e3d6e）：v68 concurrent + resumed 窗口，display 段 461 帧后转段、resumed 段 10.03 s；pinned-engine=411、completions-pinned=411、`threads=7`、`flag=0`。判读见 [v2 运行](../independent-behavior-graph.md#c-g5-e1-run3)。

## C-G5-E1 修复版首轮运行日志（2026-11-04，v69）

- [CharacterPanel-g5e1-v69-repair-run1-20261104-2307.log](CharacterPanel-g5e1-v69-repair-run1-20261104-2307.log)（323,130 字节，SHA-256 f00960a725415822753e028a6c9923acc434574ada62ba299fa9f9e3edf07294）：v69（修复版首次上机）`SamplingLiveness=concurrent` 窗口，23:05:37.167 武装、23:05:57.167 到期（1040 帧，`reason=expired`，无转段）。新字段与关闭对账行完整：`eligible=recorded=drained=2628 missing=0 contention/full-dropped=0 transport-complete=yes`。判读见 [v69 首轮](../independent-behavior-graph.md#c-g5-e1-v69-run1)：修复版传送/对账在其覆盖范围内成立，但窗口未转段、对 Q2 无效（按预注册规则记"未检验"）。

- [CharacterPanel-g5e1-v69-repair-run2-resumed-20261104-2315.log](CharacterPanel-g5e1-v69-repair-run2-resumed-20261104-2315.log)（261,457 字节，SHA-256 ddede89bb2fefe4d835cb24bb5674c387d8d1a282ac13a2773273fa98eb629ef）：v69 两段窗口完整轮，23:15:28.432 转段（display 225 帧）、23:15:38.437 关闭；resumed 段 `pinned-engine=408` 全部 `caller=0xba3f6e`、6 工作线程、788/788 全配对、对账恒等式全零缺口。判读见 [v69 第二轮](../independent-behavior-graph.md#c-g5-e1-v69-run2)：修复版 E1 取证缺口（入口活性 + 配对 + 对账）闭合；运行后部署 INI 已恢复 dormant `27f481f5…`。

## C-G4-C1 修复版重跑日志（2026-11-04/05，v69）

- [CharacterPanel-g4c1-v69-attempt1-single-dispatch-20261104-2336.log](CharacterPanel-g4c1-v69-attempt1-single-dispatch-20261104-2336.log)（58,897 字节，SHA-256 0bfa8712910619b1a30e37d1b22528de46a0f9bac66d400e21e894ef3aab5926）：修复版 58377 观察器首次过 C1 窗口（`calls=42/other-graph=42`、`events=0`、`display-close`），但 INI 派生漏掉卸下切片两键，回放只有一次 `WeapEquip` 派发、收于 readied 姿态——仪器有效、形状不完整，不构成 C 判据。判读见 [build.md attempt 1](../build.md#v69-g5e1-clean-rebuild-2026-11-04)。
- [CharacterPanel-g4c1-v69-attempt2-two-dispatch-20261105-0001.log](CharacterPanel-g4c1-v69-attempt2-two-dispatch-20261105-0001.log)（75,607 字节，SHA-256 06a743c820475631b4224e54a12cba1f2edf0ad973cdf13bd5eb8a6917f4f699）：INI 补齐 `CandidateUnequipProbe=true` + `CandidateUnequipEvent=Unequip` 后的完整两注入轮——判据 1–6 全部成立（`calls=64/other-graph=64`、源图 `events=0`、`ring-dropped=0`、`reason=display-close`），`drive-done`/`unequip-done` 六哈希第九次逐位复现（修复版仪器上第一次）。**G4-C 据此在修复版仪器上重新关闭**；判读见 [C-G4-C1 修复版重跑](../independent-behavior-graph.md#c-g4-c1-concurrent-events)。运行后 INI 恢复 dormant `27f481f5…`。
