# C-G5-TRANS v6 候选父链探针

v115 在 AE 1.6.1170 的既有 K5 候选采样点读取主图的活动节点和子索引表。首轮只回答一个问题：装备第 2–12 步实际经过哪些生成器，作者树的内层 `State3` 是否在这条链上。 [v6 离线审计](../g5-transition-probe-v5-20261119/hkx/stateinfo_layout_audit.md)

## 采集范围

- 只采候选装备段第 1–13 步的 `pre-sample` 和 `post-sample`，共 26 个观测点。锚点为已实见的 `DG_1hmEquip`，用同代 clip 指针、绑定与时钟和 K5 对账。前后读数用 `commanded` 对齐，`post-sample` 的 `completed` 与 CPK5 记录对齐。
- 反向记录活动图中所有可达父边，保留原始 edge slot 和必要的输出转发关系。路径节点记录模板、运行实例和所属图。状态机记录当前 ID、StateInfo 下标及 generator，选择器记录实际使用的索引和选中 generator。
- 与已有 `TRANSITION PROBE`、`TRANSITION CENSUS` 和 `K5 RECON` 一起判读。v6 的新读取在现有候选观察阶段进行，CPK5 格式沿用 v114。
- 新行前缀为 `TRANSITION PARENT CHAIN`。`SUMMARY` 登记数量、溢出和读数状态，`CLIP` 与 CPK5 对账，`NODE`/`EDGE` 还原原始边，`SM`/`SELECTOR` 校验实际选择，`BLENDER` 报告 flags 与权重模式。权重读数按同代 clone 与既有 `TRANSITION PROBE` 行关联。每相位最多读取 256 个节点、4,096 个子索引，最多报告父链中的 64 个节点和 128 条边。

## 上机步骤

1. 关闭 Skyrim/SKSE。在 MO2 的 `AE-Track2` 使用之前的 Save116 和装备状态。备份当前 CharacterPanel 的 DLL、PDB 和 INI，再将测试包的三件套放入 CharacterPanel 模组。
2. 确认 `IndependentGraph=inject`、`CandidateEquipPrime=true`、`CandidateEquipPrimeBranch=true`、`CandidateUnequipProbe=true`、`CandidateUnequipEvent=Unequip`、`K5FullSample=true`、`TransitionStateProbe=true`、`TransitionParentChainProbe=true`。旧 `TransitionSubgraphProbe=false`，`SamplingLiveness=off`。Track 2 的 OAR 状态沿用 v114。
3. 启动游戏并加载 Save116。按 v114 的操作打开一次背包，候选回放会自动执行装备与卸下段。等 K5 写入后关闭面板并退出游戏。每次重跑用新的 Skyrim 进程。
4. 保留完整 `CharacterPanel.log` 和同一 session 的 `CharacterPanel-g5k5-*.bin`、`.json`。同时记录意外退出、画面异常或操作偏差。

## 判读

| 项 | 首轮核对 |
| --- | --- |
| 构建及环境 | 日志识别为 v115，运行时为 1.6.1170，`AE-Track2` 和 Save116 与 v114 同场景。 |
| K5 完整性 | 候选 249/249，装备 120、卸下 129，以 `state-12-to-0` 结束。已登记的 SCAR dummy 单条 IDUN 单列。 |
| 正控与覆盖 | 第 1 步的既有 census 预计看见两个 clip 和两个普通 Blender。第 1–13 步各有前后观测，共 26 个 summary，按装备步和相位与 K5 对照。 |
| 父链资格 | 同代 clip 身份可对账，父链完整回溯，范围错误、读取失败、模糊锚点、截断和环均判为观测缺口。多父边是要保留的正常证据。 |
| 分叉 | 链路经过实际 `State3` 时对照运行时 flags、活动子边和有效权重。若完整链路无普通 Blender，转查引擎实际采样相位、同步和输出混合。链路未闭合时先诊断缺口。 |

`SUMMARY status=ok` 表示原始父链范围和采集到的身份检查成立。判读分支时还要核对 `SM`/`SELECTOR` 的 `on-path`。`no-parent`、`incomplete-identity`、读失败、溢出、索引异常或环都须保留原始日志。

首轮为候选侧诊断，正式 `G4P-7` 裁定仍是 **FAIL**。

## 本地构建

2026-11-20 使用 VS 2026 MSVC 19.52、Ninja Multi-Config Release 和已有 vcpkg 库。链接显式选用 Detours 发布库，构建使用当前工作树。候选 INI 保留 v114 的只读输入观察键，并以父链探针取代子图探针。

构建身份从 DLL 提取为 `CharacterPanel-scenegraph-v115-g5trans-parent-chain-20261120-b9bd29fad6+dirty-cl94faaed0c6-20261120T033336Z`。链接重试复用了先前编译的对象，保留其编译时身份。离线 C++ 测试 10/10 通过，其中新增两组分别覆盖共享父边与空槽、无效范围与带有效根的环路。

| 工件 | 字节数 | SHA-256 |
| --- | ---: | --- |
| CharacterPanel.dll | 2,107,392 | `e1f2c69c11e2a7a6de2ed19adc842fc848d764e833bc6b225c04d86040a13974` |
| CharacterPanel.pdb | 35,106,816 | `69401e8a1b8f44f97b9cb0f335352fc781bc102a2960193f5737923807222a96` |
| CharacterPanel.ini | 17,110 | `a19c8d95327f5eabf06b0112c3fb4a07ccbb9afc1c0f5ab94ed00f82590c54fa` |
