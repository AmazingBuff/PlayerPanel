# 诊断证据保留与恢复

更新：2026-11-04。当前结论以[独立图证据交接](../independent-behavior-graph.md#handoff-20261102-evidence)为准：G3-B 相位回放通过；G4 的 A 项（v63 运行）保留，**C 项（v66 运行）的"缺口已关闭"已被 [audit30](../behavior-graph-construction-audit.md#audit30) 推翻**（58377 detour thunk 实参错位，零事件是门不成立），待修复后重跑；下一阶段 G5 预注册、G6 待验。本页仅管理证据导航，不生成替代原始日志。

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

- [CharacterPanel-g5e1-liveness-first-20261104-0010.log](CharacterPanel-g5e1-liveness-first-20261104-0010.log)（221,958 字节，SHA-256 5318dcb1e56bd625914b179f2467c6bd23cb60216e2279062572fe1a61bf6d6）：v67 SamplingLiveness=concurrent 窗口，00:12:00.333 武装、00:12:20.345 到期（995 帧）。判读、计数与两条独立事实见[首轮运行](../independent-behavior-graph.md#c-g5-e1-run1)；原文保留，不改写。

- [CharacterPanel-g5e1-liveness-on-mode-20261104-0021.log](CharacterPanel-g5e1-liveness-on-mode-20261104-0021.log)（5,669 字节，SHA-256 c0a07a8559c2729611e7c521a98c56905bd10fc4f242ee164f03991d16bc7635）：v67 SamplingLiveness=on，游玩态 F7 窗口 19.99 s / 748 帧，引擎本地循环调用 63589 共 161,006 次、lag=0。判读见[补充运行 A](../independent-behavior-graph.md#c-g5-e1-run2)。
