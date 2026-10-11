# HKX41 测试单（2026-10-12，一次性动作不回绕：equip 播一遍就停）——**待上机**

- 构建身份：`CharacterPanel-scopy-HKX41-1fd1779d46+dirty-cl94faaed0c6-20261011T163821Z`
  （基线 = HKX40 判读提交 `1fd1779d46`，其上为未提交的回绕修复）。
- 包：`dist/CharacterPanel-scopy-HKX41-1.2.1.zip`，DLL SHA-256 `88028a48…`
  （全值 `88028a48d76c948c19cb2c011e50d1ca5cd5f746fea7583a2c859fb59eb0e216`），
  **DLL 与 PDB 均已从最终 zip 部署到 `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\` 并校验一致**。单测 1/1。
- 用户报告：背包内换武器动作结束后又循环播放换武器动作；预期 = 换装动作只播一次，随后播持械 idle。

## 根因（HKX40 日志已实锤，修复针对它）

傀儡图本身是对的：digest 时间线显示 `2HW_Equip` 播 0.77→1.62s 后图切 `2HW_Idle`，无重入。
循环出在**显示端**：`play_animation_clip` 无条件 `fmod(duration)` 回绕采样时间——一次性 clip
（equip/enter）在图混合出场窗口里 control 的 localTime 越过 clip 时长（0.77→1.62 就是证据），
fmod 把它从头重放。idle 没暴露此问题是因为它们的 control localTime 由引擎自己回绕（digest 里
2HW_Idle 1.64→0.51 是引擎行为），fmod 只是碰巧无害。

## 修复（一处机制 + 一处日志）

`play_animation_clip` 增加 `loop` 语义，采样时间按**clip generator 自己的播放模式**处理：
`kModeLooping` → 回绕（现状）；`kModeSinglePlay`（及 PingPong/UserControlled）→ **钳制在时长
末端持住最后一帧**——引擎此时正在把该帧混合淡出，显示端持住它才是忠实呈现。驱动器逐帧读
`generator->mode` 决定；捕获期 ClipPlayer 路线语义不变（默认 loop）。
driver 切换行补 `mode=loop|single` 字段，下轮日志可直接核对每个 clip 的模式判读。

## 上机步骤

1. 确认部署：DLL SHA-256 `88028a48…`。
2. 进存档 → 开背包 F7/F8 → **换一件右手武器，盯着面板人物**：装备动作应**只播一遍**，随后
   （混合出场约半秒的末端持帧后）进入持械 idle 循环。
3. 再换一次异族武器（单手→双手→弓），确认每族都"一遍→持械 idle"。
4. 收武器（空手）：sheathe 动作同样只播一遍。
5. 回归：F2 关/开、F3、F4、读档各一次；ab 行照常（本修不影响映射判读，`calm=` 数据继续累积）。
6. 日志照惯例名归档 `docs/diagnostics/CharacterPanel-hkx41-oneloop-20261012.log`。

## 判读表

| # | 检查 | 通过判据 | 失败时 |
| --- | --- | --- | --- |
| 1 | **equip 只播一遍** | 换装后面板人物：装备动作 1 次 → （≤1s 末端持帧）→ 持械 idle | 仍循环 = 该 clip 的 mode 是 looping（看 driver 行 `mode=`）——那就是 OAR 替换件把 mode 改成了 loop，需在驱动端按 clip 名族覆写，回传 driver 行 |
| 2 | 末端持帧自然 | 装备动作结束到 idle 起播之间是"定住淡出"，不是跳变 | 跳变明显 = 混合出场窗口显示端没跟上（对照引擎期同动画观感），回传 |
| 3 | driver 行 `mode=` | equip 族显示 `single`、idle 族显示 `loop` | mode 全 loop/single = 读取错位，回传 |
| 4 | 零回归 | 零崩溃、无新 `[E]`、idle 循环照常、ab 照常 | —— |

**若 #1 仍循环**：说明该替换件的 generator mode 被 OAR 写成 looping——那是替换件的数据，
显示端忠实回放它反而是"对"的；下一步才是在驱动端做"一次性族名单覆盖"（equip/enter/sheath
名字族强制 single），届时把 driver 行拿回来定名单。
