# OAR 3.2.1 条件 hook

更新日期：2026-10-15。

## 范围与入口

实现位于 [src/oar](../src/oar/hooks.h)，针对 Nexus 当日最新主文件 3.2.1。
`main.cpp` 在 `kPostPostLoad` 调用 `oar::install_hooks()`；OAR 未加载、版本／二进制
身份不匹配或安装失败时，`hooks_ready()` 为 false。

本模块实现条件 hook 与可调用的同步求值入口。当前渲染器仍使用既有后端，尚未把
idle／equip 选片与播放切到本模块；不能据此宣称独立站立显示和换装流程已完成。

调用方使用 `evaluate_condition(condition, source, preview_clip, state, parent_submod)`。
`source` 保留原角色身份与装备，`preview_clip` 必须属于预览实例，`parent_submod`
应沿用该条件所在的 OAR 子模组上下文。返回值只有 `supported && matched` 才能用于接受候选。
`diagnostic` 记录首个拒绝原因，`overrides` 记录覆盖次数。
调用者还须持有条件树并防止求值期间被编辑器重建，例如使用上游读锁或独立的稳定副本；本模块
不跨帧缓存条件、Actor 或 clip 指针。OAR 编辑器的 `GetCurrent` 展示仍可能是来源真实值，不能把
该展示值当作 hook 后的求值结果。

`ScopedEvaluation` 是内部的同步作用域，不能跨线程、异步任务或 Actor／clip 生命周期使用。
作用域按线程保存，可嵌套恢复；覆盖同时匹配来源与 clip。它不由“背包打开”这一全局条件启用。

## 已实现的状态覆盖

- 潜行、腾空、奔跑、冲刺、游泳、骑乘、攻击、格挡、对话和交互等内置谓词使用站立展示状态。
  实际名称清单见 [policy.h](../src/oar/policy.h)。`IsWeaponDrawn` 由预览状态提供。
- 当前速度、浸没量、坐睡状态与攻击状态的比较使用零值；战斗状态和静止方向按状态 0 求相等。
  仍由 OAR 自己的 comparison component 处理比较运算，不把比较条件一律当作 false。
- `PreviewState::graph_variables` 表示该预览实例完整的变量表；`HasGraphVariable` 判断变量是否存在，
  值为零也代表存在。数字条件中的图变量值从同一表读取；数字读取缺失变量、非有限输入或无法转换的
  数字类型导致本次求值不受支持，不偷读原角色的同名变量。
- 身份、种族、性别、阵营、已确认装备及允许的环境条件使用原始 OAR 求值。
- 内置条件的 disabled／negated 语义保留；AND／OR／XOR 由 OAR 原实现组合。

求值前审计整棵条件树。第三方条件、自定义／preset component、Random、IdleTime 及其他未列入
支持清单的条件会明确拒绝，不以原角色真实行为代替预览状态。最大审计深度为 64。
这是有界支持范围，并非任意 OAR 动作包通用兼容。新增适配应扩充策略和验证，不能直接放行未知条件。

## 下载包与二进制证据

来源：[Nexus 文件页](https://www.nexusmods.com/skyrimspecialedition/mods/92109?tab=files)。
用户下载的主文件包名为 `Open Animation Replacer 92109 3.2.1 2026-08-31T16-32Z MWx0rveD6.7z`。
分析副本保存在忽略的 `build/oar-3.2.1`，没有将 DLL／PDB 纳入源码。

| 项目 | 值 |
| --- | --- |
| DLL 文件版本 | 3.2.1.0 |
| DLL SHA-256 | `db61b8b691e709bede355514871918baa63eabe8f6d0734bf17ffcf793b4c894` |
| PDB SHA-256 | `7b90b51a71bd074d3628afdc231570a58e327219908675bc25dcc8a0509edf28` |
| DLL CodeView／PDB GUID | `15eb65f2-91de-4a98-99ce-9ca2192e6af4`，age 1 |
| PE timestamp | `0x6a95a9e5` |
| SizeOfImage | `0x2f6000` |
| 源码提交 | `f4e7688b065175aff70aa523073857911e15aca3`，CMake version 3.2.1 |

`llvm-pdbutil dump -publics -section-headers` 的段偏移结合 DLL PE section 得到以下 RVA；
`llvm-objdump` 反汇编核对签名与实参使用，未按源码行号或其他版本猜地址。
另从 PDB 定位并读取了 126 个原生 Condition 虚表，其 Evaluate 槽均指向 `0x68640`。
第三方 DLL 自带的 `CustomCondition::Evaluate` 不属于这项证明，因此在受支持查询入口中预先拒绝。

| 入口 | RVA | 核对内容 |
| --- | --- | --- |
| `Conditions::ConditionBase::Evaluate` | `0x68640` | 读取 disabled `+0x10`、negated `+0x11`，经虚表 `+0xd0` 调用具体判断 |
| `Components::NumericValue::GetValue` | `0x1417f0` | `_type +0x80`；图变量 type `+0xf0`、MSVC string `+0xf8` |

入口前 16 字节在 [binary_profile.h](../src/oar/binary_profile.h) 保存。运行时同时核对 SKSE
插件版本、PE timestamp／image size、CodeView GUID／age 和两个入口字节，全部成立后才以一个
Detours transaction 安装。数字字段读取依赖本包的 MSVC 14.44 ABI，不能直接用于其他构建。
PDB 用于开发核对；运行时只读取 DLL 自带的 CodeView 身份信息，无需分发或加载 PDB。

## 验证与后续验收

本地回归包括预览作用域的来源／clip／线程隔离、嵌套恢复、站立谓词与取反，以及二进制身份拒绝。
默认与实验两种 Release 配置均构建通过；`snapshot_transform` 的 11 项检查全部通过（含本轮新增两项）。
这里的 11 项是 OAR 模块交付时的记录；后续透明材质试修已增加到 14 项，见
[当前维护说明](code-maintenance.md#ube-透明材质试修ube-alpha1)，OAR 两项检查继续保留。
入口、CodeView 与字段访问已对下载包作静态核对；这些证据不等于已经在游戏中成功安装 detour。

游戏内必须进一步核对：`OAR 3.2.1 hooks installed` 日志；来源跳跃／潜行时普通求值保持真实状态、
预览求值保持站立；玩家专属身份不丢失；退出作用域后结果恢复。暂不通过额外推进来源行为图来验证。

本模块没有提供 OAR 替换选择的公开桥接、动画加载、变体会话隔离、过渡播放或显示图换装。
这些需要随后接入，不能把本次条件 hook 建立完成算作 idle／equip 全链路通过。

上游更新时重新取得匹配 DLL／PDB，固定对应源码，核对 ABI／符号／反汇编并更新完整 profile；
不能只修改版本号跳过二进制检查。
