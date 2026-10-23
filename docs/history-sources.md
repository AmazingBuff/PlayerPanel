# 历史来源、覆盖与只读恢复

更新：2026-10-23。本表逐项映射本轮清理的 **144** 个过期来源（77份Markdown、67份旧日志，共6,829,636字节）。两篇总结分别拥有[渲染与外观](history-rendering.md)、[动画与OAR](history-animation.md)，入口见[历史索引](history.md)。来源名是历史路径文本，不是仍存在的文件链接。

## 恢复基线与证据边界

每一行的原始Git恢复基线均为 **`719c90bfe115a67ac64b80271dc8afced780290f`**（`feat(diagnostics): begin isolated behavior graph validation`）。该提交发生在清理之前，保留全部原件；它与先前动作修复提交`a9a0a20`是两个逻辑阶段。后续整理提交不改变这个固定恢复点。

在仓库根目录可只读查看任一来源，不改当前工作区，例如：

```powershell
git show 719c90bfe115a67ac64b80271dc8afced780290f:docs/afp1b-verdict-2026-10-13.md
git show 719c90bfe115a67ac64b80271dc8afced780290f:docs/diagnostics/CharacterPanel-hkx37-puppet-20261011.log
```

需要导出时，用编辑器将只读结果另存到明确位置，或用Python将`git show`的stdout **bytes**写到单个新文件。不要用旧Windows PowerShell文本重定向声称逐字节恢复。Git blob可能使用规范LF，清理前工作区原件可能为CRLF：Git恢复身份与工作区原始byte/hash是两套指标。不要为查看历史用checkout/reset覆盖当前文件。

原始字节SHA-256、大小、行数、Git blob、逐源分类、summary anchor与精确删除审计保存在`build/docs-consolidation-20261023/`；批准清单在`.codex/task-runs/docs-consolidation/cleanup-manifest-v1.json`。审计只存元数据和必要结论，没有另存完整日志、ZIP或重复报告。原报告引用但未随该Git提交入库的外部图片/日志/游戏文件，仍然没有恢复保证；总结明确其证据等级。

保留17个原文件：现行规范/PRD/社区参考、独立图方案、在用问题交接及history索引；原始r1/r2、engine-text元数据、三个TSLOT工具保留原处。`CharacterPanel-engine-text.txt`不是可运行的Skyrim，也不证明工厂协议。最新诊断从[第10节](diagnostics/display-issues-2026-10-22.md#10-2026-10-23-装备上下文与火把分层修复待上机)继续。

## 报告覆盖（77/77）

每行按该来源的重要归并点选一个主落点；跨主题内容在两篇总结互链。静态检查、未执行方案、真实运行及后来纠正的判读分开保存。

| 原始路径（固定恢复基线同上） | 证据/状态 | 总结落点与保留要点 |
| --- | --- | --- |
| `docs/actor-final-pose-validation-2026-10-12.md` | 未执行协议，后续分关有实测 | [归并段](history-animation.md#final-output)：R00–R09：输出、引擎参考、写入、直绘、物理、回收分验，不计整单通过。 |
| `docs/afp-next-validation-and-paused-gaps-2026-10-13.md` | 用户观察与未执行方案 | [归并段](history-rendering.md#future)：覆盖/直绘/物理/FaceGen/morph分层；未附构建反馈不归入AFP8日志。 |
| `docs/afp-summary-2026-10-13.md` | 后续纠正汇总 | [归并段](history-animation.md#final-output)：AFP3正结果、AFP5/8实际写数与观测匹配分开，撤回route closed。 |
| `docs/afp1-verdict-2026-10-13.md` | 早期实测，部分判断被推翻 | [归并段](history-animation.md#final-output)：pose轨可读；poseLocal次序及深泊/停用归因以AFP1e/3后测修正。 |
| `docs/afp10-spawn-shape-dress-test-2026-10-14.md` | 混合：现场修复与待测代码轮 | [归并段](history-rendering.md#ube)：Actor出生/服装/morph；AFP10g断源、prune、弹药与槽位待上机。 |
| `docs/afp1b-verdict-2026-10-13.md` | 同文件含多次更正 | [归并段](history-animation.md#final-output)：§13按骨架序及0.069°对照更正旧headline；drive/暂停混杂另记。 |
| `docs/afp2-control-contract-2026-10-13.md` | 控制协议与实现台账 | [归并段](history-animation.md#final-output)：A0–A5及映射门槛，A4/A5当时尚未接通，字段存在不等于验收。 |
| `docs/afp2-draw-structural-input-2026-10-13.md` | 静态结构输入与待测设计 | [归并段](history-rendering.md#afp-rendering)：共用draw缝已抽；临时取景/恢复、所有权、线程风险另验。 |
| `docs/afp3-paused-evaluation-2026-10-13.md` | 静态调查与待测杠杆 | [归并段](history-animation.md#final-output)：更新链/force字段候选；stack-scan不是已证明调用链。 |
| `docs/afp3-verdict-2026-10-13.md` | 历史运行正结果 | [归并段](history-animation.md#final-output)：同暂停同位置force使0→116→0，109/109；is-active恒1。 |
| `docs/afp4-verdict-2026-10-13.md` | 历史运行观察 | [归并段](history-animation.md#final-output)：树名字参考108/109；强制generate破坏与无force应用的对齐。 |
| `docs/afp5-verdict-2026-10-13.md` | 历史运行，标题后被纠正 | [归并段](history-animation.md#final-output)：暂停首写36、不可达32；不等于端到端route closed。 |
| `docs/afp8-verdict-2026-10-13.md` | 历史运行与范围纠正 | [归并段](history-animation.md#final-output)：写92/不可达6，另109/109仅观察；draw未接通。 |
| `docs/afp9-black-panel-verdict-2026-10-13.md` | 历史定位，修复后另轮确认 | [归并段](history-rendering.md#afp-rendering)：RTV与SRV冲突；unreachable旧61项是审计错误。 |
| `docs/afp9-controller-park-verdict-2026-10-13.md` | 用户确认与待验证修复 | [归并段](history-rendering.md#afp-rendering)：AFP9i画面稳定；AFP9j controller warp消交互仍待测。 |
| `docs/afp9-coverage-and-direct-draw-test-2026-10-13.md` | 实现与预登记 | [归并段](history-rendering.md#afp-rendering)：覆盖账目守恒、骨必要性、同Actor直绘；后续各轮分别取证。 |
| `docs/afp9-evidence-dump-verdict-2026-10-13.md` | 历史观察与取证改动 | [归并段](history-rendering.md#afp-rendering)：local泊位仍被覆盖，恢复TGA与同帧证据；无新增通用保证。 |
| `docs/afp9-f6-framing-verdict-2026-10-13.md` | 历史观察与待测修复 | [归并段](history-rendering.md#afp-rendering)：复制路线合成确认；有父根local/downward不移自身，直绘投影仍错。 |
| `docs/afp9-park-verdict-2026-10-13.md` | 历史观察与待测修复 | [归并段](history-rendering.md#afp-rendering)：只移动引用不移动3D，世界偏移按绝对差幂等。 |
| `docs/afp9-skin-cache-verdict-2026-10-13.md` | 历史画面与后续用户确认 | [归并段](history-rendering.md#afp-rendering)：完整/只剩斧头交替；使frameID失效让引擎重算，非直接写矩阵。 |
| `docs/afp9-stale-bound-verdict-2026-10-13.md` | 历史图像/日志定位 | [归并段](history-rendering.md#afp-rendering)：缓存bound失效致错误深度；改活骨坐标定心，不以center-d自证。 |
| `docs/afp9-transform-layer-verdict-2026-10-13.md` | 历史观察，后续又反转 | [归并段](history-rendering.md#afp-rendering)：world会被重推；补local/提前取景仍需后测，非最终泊位协议。 |
| `docs/afp9-verdict-2026-10-13.md` | 崩溃定位与待复测 | [归并段](history-rendering.md#afp-rendering)：palette槽不可直接读名字，改本树节点与指针集合比对。 |
| `docs/display-backend-routing-2026-10-13.md` | 未实现后续设计 | [归并段](history-rendering.md#future)：交易双方、尸体/容器、混合后端/刷新，保留验收条件不列当前能力。 |
| `docs/equipment-idle-dac-test-2026-10-20.md` | 多轮：计划、局部实测与修正 | [归并段](history-animation.md#dac-ied)：真实base/过渡、VMAD误判、IED边界；最后骨盆clamp已回退。 |
| `docs/m0-acceptance.md` | 有限工程验收 | [归并段](history-rendering.md#m0)：菜单M0六项通过一项部分；不替人物Clone回收或正式性能通过。 |
| `docs/m0-capture-report-2026-09-28.md` | 目标版用户采集与后续更正 | [归并段](history-rendering.md#m0)：CS场景、入口offset；全局accumulator可换不代表菜单消费。 |
| `docs/m0-diagnostics.md` | 历史诊断协议 | [归并段](history-rendering.md#m0)：有界只读探针，原独立culler/RT协议当时未定。 |
| `docs/m0-handoff.md` | 长期交接，后段修正早段 | [归并段](history-rendering.md#scene-copy)：M0/Actor成功与S0三种析构失败分开；cap12不限制原生泄漏。 |
| `docs/m0-proto.md` | 逐轮实测含被推翻假设 | [归并段](history-rendering.md#m0)：swap失败、原调用后重放、RTV/深度/合成；alpha底层机制仍未定。 |
| `docs/oar-fork-handoff.md` | 历史fork提案，后续已替代 | [归并段](history-animation.md#oar)：公开API局限不等于当前二进制主动查询不可行；无fork实施证据。 |
| `docs/oar-idle3-fixes-2026-10-16.md` | 多轮实测/待测，旧策略已替代 | [归并段](history-animation.md#scheduling)：校准/挂点、世界相位/playlist失败；mlh覆盖主层是该轮特定证据。 |
| `docs/oar-preview-query-2026-10-16.md` | 静态ABI核对与本地验证 | [归并段](history-animation.md#oar)：两版本独立profile、错误RVA纠正、clip/variant布局；旧global调度已替代。 |
| `docs/oar-standing-idle-handover.md` | 旧探针，关键结论失效 | [归并段](history-animation.md#oar)：暂停hits0样本；0x1A5720函数误认，唯一索引阻塞判断失效。 |
| `docs/s2-anim-probe-evidence-2026-10-09.md` | 多轮历史观察与方法纠正 | [归并段](history-animation.md#pose-probes)：探针次序/轴盲区/旧目标、CPU槽未知、节点与画面随动边界。 |
| `docs/s2-animation-handoff.md` | 长期交接，后测覆盖旧结论 | [归并段](history-animation.md#hkx-discovery)：template/live反转、正确采样槽、源只读；日志/键位均属历史。 |
| `docs/s2-animation-milestone-2026-10-11.md` | 有限里程碑与后续观测 | [归并段](history-animation.md#hkx-mapping)：HKX28单变体37/37、0.15°；H05另验、池性能及公开API局限。 |
| `docs/s2-hkx-next-work-package-2026-10-10.md` | 未执行协议，后续逐步验证 | [归并段](history-animation.md#hkx-discovery)：活身份、资源持有、确定采样、真值与隔离分关。 |
| `docs/s2-hkx1-alignment-evidence-2026-10-10.md` | 历史运行观察 | [归并段](history-animation.md#hkx-discovery)：109/116名字桥；boneNodes37/116同名否定按长度推同序。 |
| `docs/s2-hkx10-control-evidence-2026-10-10.md` | 早期观察，普遍否定被推翻 | [归并段](history-animation.md#hkx-discovery)：66个template空binding；HKX13活实例正式反证。 |
| `docs/s2-hkx11-animmgr-evidence-2026-10-10.md` | 历史运行观察 | [归并段](history-animation.md#hkx-discovery)：7872项目哈希记录不是animation，注册/载入层分离。 |
| `docs/s2-hkx12-hashed-evidence-2026-10-10.md` | 历史运行与错误入口修正 | [归并段](history-animation.md#hkx-discovery)：索引解析返回0x20文件信息，+0x28读邻记录，不是DBData。 |
| `docs/s2-hkx13-instance-evidence-2026-10-10.md` | 历史运行正结果 | [归并段](history-animation.md#hkx-discovery)：activeNodes取得kClone活binding/control，推翻标准层死路；映射未过。 |
| `docs/s2-hkx13-static-check-2026-10-10.md` | 静态强假设，后续实测支持 | [归并段](history-animation.md#hkx-discovery)：旧扫描是模板树；本轮只读核对不算新运行。 |
| `docs/s2-hkx14-tracks-evidence-2026-10-10.md` | 历史转储与待验建议 | [归并段](history-animation.md#hkx-discovery)：hkaBinding布局自洽、mapping空；control访问器方案后来无用。 |
| `docs/s2-hkx15-control-evidence-2026-10-10.md` | 历史运行观察 | [归并段](history-animation.md#hkx-discovery)：control同一binding，99字节权重非骨索引；不需虚调同空表。 |
| `docs/s2-hkx17-guard-evidence-2026-10-10.md` | 安全门有效，原因后被修正 | [归并段](history-animation.md#hkx-loading)：data-not-present由过严表检查误判；HKX24确认压缩data在。 |
| `docs/s2-hkx18-engine-load-2026-10-10.md` | 多轮装载/采样反转 | [归并段](history-animation.md#hkx-loading)：Queue/Load→binding-set；arena弯路，slot04成功替slot06空表崩。 |
| `docs/s2-hkx2-replay-evidence-2026-10-10.md` | 历史正结果与排序误判 | [归并段](history-animation.md#hkx-discovery)：直接四元数/灵敏对照成立；boneNodes排序被AFP1e修正。 |
| `docs/s2-hkx3-catalogue-evidence-2026-10-10.md` | 历史目录与后续确认 | [归并段](history-animation.md#hkx-discovery)：名字可达；仅同长度不足证同序，binding布局当时未定。 |
| `docs/s2-hkx4-layout-evidence-2026-10-10.md` | 旧推断被后测推翻 | [归并段](history-animation.md#hkx-discovery)：+0x30误当内嵌binding，后知相邻包装对象，不按猜偏移。 |
| `docs/s2-hkx5-clip-evidence-2026-10-10.md` | 历史观察与失败选路 | [归并段](history-animation.md#hkx-discovery)：binding小桩/类型化clip入口；模板空桩不否定运行实例。 |
| `docs/s2-hkx6-clipsearch-evidence-2026-10-10.md` | 历史仪器缺陷 | [归并段](history-animation.md#hkx-discovery)：整0x200可读门跳过0x78 StateInfo，capped不是全树走完。 |
| `docs/s2-hkx8-clipsearch2-evidence-2026-10-10.md` | 历史仪器缺陷与待测修正 | [归并段](history-animation.md#hkx-discovery)：无名扫描未进入typed树，改精确vtable及成员窗口。 |
| `docs/s2-hkx9-vtable-evidence-2026-10-10.md` | 历史运行观察 | [归并段](history-animation.md#hkx-discovery)：typed states布局成立，66clips空binding仍是template问题。 |
| `docs/s2-s3-plan.md` | 历史方案及已撤销排他判断 | [归并段](history-animation.md#future)：程序摆动不是真HKX/物理；公开接口限制不证明唯一自制模拟。 |
| `docs/s3-independent-physics-validation-2026-10-10.md` | 未执行测试协议 | [归并段](history-animation.md#future)：实际SMP/CBPC实例、A/B、碰撞、时钟、解绑与回收，当前无接口通过。 |
| `docs/scene-copy-afp1-final-output-test-2026-10-13.md` | 历史仪器测试单与首轮结果 | [归并段](history-animation.md#final-output)：只读最终输出；旧泊位/次序假设以后续AFP1e/3为准。 |
| `docs/scene-copy-diagnose-test-2026-10-12.md` | 同文件后追加实测 | [归并段](history-animation.md#hkx-mapping)：HKX40 calm38–62%、装备事件链/再基线；未定唯一映射原因。 |
| `docs/scene-copy-h05-isolation-test-2026-10-11.md` | 后追加有限实测 | [归并段](history-animation.md#isolation)：7会话64操作128普查、24NPC上限；粒子world变化不等于源local写入。 |
| `docs/scene-copy-oneloop-test-2026-10-12.md` | 待测显示端回绕修复 | [归并段](history-animation.md#puppet)：真实图已进Idle，显示fmod重播Equip；不计修复上机通过。 |
| `docs/scene-copy-probe-menu-equip-2026-10-11.md` | 有限样本实测 | [归并段](history-animation.md#puppet)：法术→收鞘匕首几何变而源活动clip不变，不外推全部操作。 |
| `docs/scene-copy-puppet-probe-test-2026-10-11.md` | 后追加正结果与未测项 | [归并段](history-animation.md#puppet)：第二Actor3360paused ticks/index切换；equip/映射未过，清理未执行。 |
| `docs/scene-copy-scheduler-v1-test-2026-10-11.md` | 后追加历史运行 | [归并段](history-animation.md#puppet)：4890ticks、Pickaxe选片/显示跟进；motion下坠、读档/回收未证。 |
| `docs/scene-copy-scheduler-v2-test-2026-10-11.md` | 后追加运行与诊断修正 | [归并段](history-animation.md#hkx-mapping)：109.16°identity失配、装备合法变化污染整会话基线。 |
| `docs/scene-copy-solver-test-2026-10-12.md` | 后追加失败实测 | [归并段](history-animation.md#hkx-mapping)：冻结真值与retry缺陷，solved出现inf不算有效映射。 |
| `docs/scene-copy-solver2-test-2026-10-12.md` | 修复方案，随后崩溃 | [归并段](history-animation.md#hkx-mapping)：引擎期观测、值累积与缓存；裸节点跨帧成UAF路径。 |
| `docs/scene-copy-solver3-test-2026-10-12.md` | 已构建未上机 | [归并段](history-animation.md#hkx-mapping)：HKX44逐帧重解析修复；转AFP后决定不测，不写求解成功。 |
| `docs/scene-graph-copy-next-test-2026-10-09.md` | 历史协议与局部结果 | [归并段](history-animation.md#pose-probes)：修探针次序、真实draw/权重/见证点前置；旧程序idle验收作废。 |
| `docs/scene-graph-copy-next-test-2026-10-11.md` | 后追加用户运行确认 | [归并段](history-animation.md#scheduling)：HKX34特效DIRTY泄漏，HKX35名字/页查询成本修复，非FPS基准。 |
| `docs/scene-graph-copy-results-2026-10-08.md` | 有限历史静态通过 | [归并段](history-rendering.md#scene-copy)：CBBE/UBE外观/隔离；14captures停放非回收，三析构失败未定因。 |
| `docs/scene-graph-copy-results-template.md` | 历史空模板 | [归并段](history-rendering.md#scene-copy)：Q09/Q10显示切换与实际释放分开；不含新增测试结果。 |
| `docs/scene-graph-copy-validation.md` | 历史协议及范围修正 | [归并段](history-rendering.md#scene-copy)：nativeClone副本审计，静态/动作/物理/回收分验。 |
| `docs/stage0-spike.md` | 早期未执行清单与静态修复 | [归并段](history-rendering.md#m0)：私有trampoline、CS调用点/RT重放候选，不套当前操作。 |
| `docs/stage2-p-instance-plan.md` | 多轮运行与反转 | [归并段](history-rendering.md#lighting)：Actor取景、设备门错误、原生灯账本/队列/单draw与世界漏光。 |
| `docs/stage2b-studio-home-plan.md` | 历史具体路线正结果 | [归并段](history-rendering.md#studio-home)：迁图杀壳/teardown、灯壳UAF与fetch-stall自愈；不替NiClone销毁。 |
| `docs/ube-transparency-test-2026-10-15.md` | 本地验证与有限用户反馈 | [归并段](history-rendering.md#ube)：10/15黑块消失，三复测未确认；10/16整体确认无新增日志/抓帧。 |

## 旧日志覆盖（67/67）

日志是对应阶段原件，不把启动片段、缺失draw或清理阶段当作成功；CrashLogger的[S] stack scan与[P] probable frame分开解读。

| 原始路径（固定恢复基线同上） | 证据/状态 | 总结落点与保留要点 |
| --- | --- | --- |
| `docs/diagnostics/CharacterPanel-afp1-final-output-20261012.log` | 历史运行日志 | [归并段](history-animation.md#final-output)：AFP1 pose轨116、骨架参考；空傀儡输出不证明深泊/停用因果。 |
| `docs/diagnostics/CharacterPanel-afp1b-held-20261012.log` | 历史运行日志 | [归并段](history-animation.md#final-output)：AFP1b held98/paused0，drive切换混杂，后测另分离。 |
| `docs/diagnostics/CharacterPanel-afp1c-paused-20261012.log` | 历史运行日志 | [归并段](history-animation.md#final-output)：AFP1c paused drive on仍无pose，局部clip时间持续。 |
| `docs/diagnostics/CharacterPanel-afp1d-poselocal-20261012.log` | 历史运行日志 | [归并段](history-animation.md#final-output)：AFP1d poseLocal微抖，undriven玩家也有同量变化。 |
| `docs/diagnostics/CharacterPanel-afp1e-poselocal-order-20261012.log` | 历史运行日志 | [归并段](history-animation.md#final-output)：AFP1e skeleton109/109，修正HKX2次序判断。 |
| `docs/diagnostics/CharacterPanel-afp3-force-generate-20261012.log` | 历史运行日志 | [归并段](history-animation.md#final-output)：AFP3首轮force/pose观测，引用后续3b同条件A/B。 |
| `docs/diagnostics/CharacterPanel-afp3b-force-generate-20261013.log` | 历史运行日志 | [归并段](history-animation.md#final-output)：AFP3b同暂停force0→116→0，is-active1。 |
| `docs/diagnostics/CharacterPanel-afp4-apply-20261013.log` | 历史运行日志 | [归并段](history-animation.md#final-output)：AFP4无force证明与force错相位参考分开。 |
| `docs/diagnostics/CharacterPanel-afp5-latch-20261013.log` | 历史运行日志 | [归并段](history-animation.md#final-output)：AFP5首帧written36/unreachable32，不是全骨上屏。 |
| `docs/diagnostics/CharacterPanel-afp6-coverage-20261013.log` | 历史运行日志 | [归并段](history-animation.md#final-output)：AFP6仍有unverified-world-first，36写数不外推。 |
| `docs/diagnostics/CharacterPanel-afp7-coverage-20261013.log` | 历史运行日志 | [归并段](history-animation.md#final-output)：AFP7会话drive/verify未取足，不把代码变更当完成。 |
| `docs/diagnostics/CharacterPanel-afp8-f4-drive-20261013.log` | 历史运行日志 | [归并段](history-animation.md#final-output)：AFP8实际92写/6不可达，另109观察非109写。 |
| `docs/diagnostics/CharacterPanel-afp9-first-test-20261013.log` | 崩溃前插件日志 | [归并段](history-rendering.md#afp-rendering)：AFP9参考88/92后开包止于更新，配对CrashLogger定位palette解引用。 |
| `docs/diagnostics/CharacterPanel-afp9b-black-panel-20261013.log` | 仅2行启动片段 | [归并段](history-rendering.md#afp-rendering)：不能单独支撑AFP9b整会话结论，报告有另述观察。 |
| `docs/diagnostics/CharacterPanel-afp9c-f6-offscreen-20261013.log` | 历史运行日志 | [归并段](history-rendering.md#afp-rendering)：AFP9c实际109写/7缺/116尝试，COMPOSITE与直绘离屏分开。 |
| `docs/diagnostics/CharacterPanel-afp9c-park-and-scatter-20261013.log` | 仅2行启动片段 | [归并段](history-rendering.md#afp-rendering)：AFP9c park/scatter名不等于完整现场；另e日志更充分。 |
| `docs/diagnostics/CharacterPanel-afp9e-world-write-undone-20261013.log` | 历史运行日志 | [归并段](history-rendering.md#afp-rendering)：AFP9e world写被重推；98尝试/18未处理，取景/恢复和skin分开。 |
| `docs/diagnostics/CharacterPanel-afp9f-park-still-reverted-20261013.log` | 仅15行灯光启动片段 | [归并段](history-rendering.md#afp-rendering)：AFP9f泊位后续未在此片段，报告与g会话互参。 |
| `docs/diagnostics/CharacterPanel-afp9g-tga-evidence-20261013.log` | 历史运行日志 | [归并段](history-rendering.md#afp-rendering)：AFP9g图像取证、裙骨缺口/83写等样本，缓存bound问题。 |
| `docs/diagnostics/CharacterPanel-afp9h-skin-cache-20261013.log` | 历史运行日志 | [归并段](history-rendering.md#afp-rendering)：AFP9h同会话部分完整/部分孤斧，覆盖随样本变化。 |
| `docs/diagnostics/CharacterPanel-afp9i-stable-panel-20261014.log` | 仅15行灯光启动片段 | [归并段](history-rendering.md#afp-rendering)：稳定画面仅报告所记用户确认，非本片段全证。 |
| `docs/diagnostics/CharacterPanel-hkx1-align-20261010-0001.log` | 历史运行日志 | [归并段](history-animation.md#hkx-discovery)：HKX1两项目/109名字桥，缺项不可当完整覆盖。 |
| `docs/diagnostics/CharacterPanel-hkx10-control-20261010.log` | 历史运行日志 | [归并段](history-animation.md#hkx-discovery)：HKX10模板66空binding，普遍死路后推翻。 |
| `docs/diagnostics/CharacterPanel-hkx11-animmgr-20261010.log` | 历史运行日志 | [归并段](history-animation.md#hkx-discovery)：HKX11项目哈希注册表，原始qword不是animation。 |
| `docs/diagnostics/CharacterPanel-hkx12-hashed-20261010.log` | 历史运行日志 | [归并段](history-animation.md#hkx-discovery)：HKX12文件索引解析，错读邻条目不等于DBData。 |
| `docs/diagnostics/CharacterPanel-hkx13-instance-20261010.log` | 历史运行日志 | [归并段](history-animation.md#hkx-discovery)：HKX13 kClone活binding/control，反证模板推断。 |
| `docs/diagnostics/CharacterPanel-hkx14-tracks-20261010.log` | 历史运行日志 | [归并段](history-animation.md#hkx-discovery)：HKX14 animation99轨但mapping data0/size0。 |
| `docs/diagnostics/CharacterPanel-hkx15-control-20261010.log` | 历史运行日志 | [归并段](history-animation.md#hkx-discovery)：HKX15 control同binding，ttw99是权重。 |
| `docs/diagnostics/CharacterPanel-hkx17-guard-20261010.log` | 历史运行日志 | [归并段](history-animation.md#hkx-loading)：HKX17守卫阻止错误入口；data absent归因后修正。 |
| `docs/diagnostics/CharacterPanel-hkx18-load-20261011.log` | 历史运行日志 | [归并段](history-animation.md#hkx-loading)：HKX18 Queue/Load返回真，未取得新animation，诊断漏字段。 |
| `docs/diagnostics/CharacterPanel-hkx19-load-20261011.log` | 仅2行启动片段 | [归并段](history-animation.md#hkx-loading)：HKX19详细结果只在合集报告，不能从此两行验证。 |
| `docs/diagnostics/CharacterPanel-hkx2-replay-20261010-0016.log` | 历史运行日志 | [归并段](history-animation.md#hkx-discovery)：HKX2直接四元数/扰动对照；排序结论后撤回。 |
| `docs/diagnostics/CharacterPanel-hkx20-resource-20261011.log` | 历史运行日志 | [归并段](history-animation.md#hkx-loading)：HKX20 loader/resource扫描未形成可靠取数。 |
| `docs/diagnostics/CharacterPanel-hkx21-tree-20261011.log` | 历史运行日志 | [归并段](history-animation.md#hkx-loading)：HKX21资源定位仍不等于加载后绑定回读。 |
| `docs/diagnostics/CharacterPanel-hkx22-region-20261011.log` | 历史运行日志 | [归并段](history-animation.md#hkx-loading)：HKX22四既有animation，before=after，不证明本次装载产出。 |
| `docs/diagnostics/CharacterPanel-hkx23-bindingset-20261011.log` | 历史运行日志 | [归并段](history-animation.md#hkx-loading)：HKX23加载index的binding填上；present判据仍过严。 |
| `docs/diagnostics/CharacterPanel-hkx24-spline-20261011.log` | 历史运行日志 | [归并段](history-animation.md#hkx-loading)：HKX24压缩data在、offsets空，slot06错误归因修正。 |
| `docs/diagnostics/CharacterPanel-hkx25-sample-20261011.log` | 历史运行日志 | [归并段](history-animation.md#hkx-loading)：HKX25槽04可采，ground-truth仍未过旧门槛。 |
| `docs/diagnostics/CharacterPanel-hkx26-phase-20261011.log` | 历史运行日志 | [归并段](history-animation.md#hkx-mapping)：HKX26相位/参考失配，未证明通用mapping。 |
| `docs/diagnostics/CharacterPanel-hkx27-driver-20261011.log` | 历史运行日志 | [归并段](history-animation.md#hkx-mapping)：HKX27可视驱动，4.65°残差尚需归因。 |
| `docs/diagnostics/CharacterPanel-hkx28-spread-20261011.log` | 历史运行日志 | [归并段](history-animation.md#hkx-mapping)：HKX28同相位0/0.15°，37/37仅该样本。 |
| `docs/diagnostics/CharacterPanel-hkx3-catalogue-20261010-0026.log` | 历史运行日志 | [归并段](history-animation.md#hkx-discovery)：HKX3名字/绑定目录，类型布局当时未定。 |
| `docs/diagnostics/CharacterPanel-hkx35-cost-model-20261011.log` | 历史运行日志 | [归并段](history-animation.md#scheduling)：HKX35池与成本修复，另变体mapping未过。 |
| `docs/diagnostics/CharacterPanel-hkx36-isolation-20261011.log` | 历史运行日志 | [归并段](history-animation.md#isolation)：HKX36窗口隔离、源116/最多24NPC及粒子例外。 |
| `docs/diagnostics/CharacterPanel-hkx36probe-menu-equip-20261011.log` | 历史运行日志 | [归并段](history-animation.md#puppet)：源法术→匕首几何变，暂停活动clip未换。 |
| `docs/diagnostics/CharacterPanel-hkx37-puppet-20261011.log` | 历史运行日志 | [归并段](history-animation.md#puppet)：第二Actor manual3360/索引切换，末尾无清理阶段。 |
| `docs/diagnostics/CharacterPanel-hkx38-scheduler-20261011.log` | 历史运行日志 | [归并段](history-animation.md#puppet)：HKX38 manual4890、图选片，清理/映射另验。 |
| `docs/diagnostics/CharacterPanel-hkx39-calibration-20261011.log` | 历史运行日志 | [归并段](history-animation.md#hkx-mapping)：HKX39 109.16°失配/装备DRIFT，需窗口再基线。 |
| `docs/diagnostics/CharacterPanel-hkx4-layout-20261010-0033.log` | 历史运行日志 | [归并段](history-animation.md#hkx-discovery)：HKX4邻包装vtable被误作内嵌，后测纠正。 |
| `docs/diagnostics/CharacterPanel-hkx40-diagnose-20261012.log` | 历史运行日志 | [归并段](history-animation.md#hkx-mapping)：HKX40多族calm、draw事件、再基线；非唯一映射根因证明。 |
| `docs/diagnostics/CharacterPanel-hkx42-solver-20261012.log` | 历史失败日志 | [归并段](history-animation.md#hkx-mapping)：HKX42冻结真值，solved出现180°/inf不可计通过。 |
| `docs/diagnostics/CharacterPanel-hkx5-clip-20261010-0041.log` | 历史运行日志 | [归并段](history-animation.md#hkx-discovery)：HKX5空桩/template，后活实例反证。 |
| `docs/diagnostics/CharacterPanel-hkx6-clipsearch-20261010-0056.log` | 历史运行日志 | [归并段](history-animation.md#hkx-discovery)：HKX6 capped96/未过StateInfo门，非不存在clip。 |
| `docs/diagnostics/CharacterPanel-hkx7-crash-20261010-0103.log` | 崩溃前插件日志 | [归并段](history-animation.md#hkx-discovery)：HKX7不可信对象虚调用判类风险，止于catalogue。 |
| `docs/diagnostics/CharacterPanel-hkx8-clipsearch2-20261010-0107.log` | 历史运行日志 | [归并段](history-animation.md#hkx-discovery)：HKX8搜索460未进typed树，后改vtable。 |
| `docs/diagnostics/CharacterPanel-hkx9-vtable-20261010.log` | 历史运行日志 | [归并段](history-animation.md#hkx-discovery)：HKX9精确vtable/typedstates通，仍模板空binding。 |
| `docs/diagnostics/CharacterPanel-oar-probe-20261022.log` | 早期诊断日志 | [归并段](history-animation.md#recent)：00:42会话DAC拒绝/恢复与PANEL/WORLD，不替r1/r2。 |
| `docs/diagnostics/CharacterPanel-s2-anim-probe-20261009-0133.log` | 早期问题探针日志 | [归并段](history-animation.md#pose-probes)：写后自比/调用次序/无效槽号，撤销全面路线否定。 |
| `docs/diagnostics/CharacterPanel-s2p1-probe-20261009-2213.log` | 历史运行日志 | [归并段](history-animation.md#pose-probes)：S2P1画面随动，轴盲区/旧目标136行需排除。 |
| `docs/diagnostics/CharacterPanel-s2p2-probe-20261009-2230.log` | 历史运行日志 | [归并段](history-animation.md#pose-probes)：S2P2见证点与draw期buffer重算，槽语义未定。 |
| `docs/diagnostics/CharacterPanel-s2p3-probe-20261009-2243.log` | 历史运行日志 | [归并段](history-animation.md#pose-probes)：S2P3候选未匹配，关/开画面与CPU语义分开。 |
| `docs/diagnostics/CharacterPanel-s2p4-probe-20261009-2254.log` | 历史运行日志 | [归并段](history-animation.md#pose-probes)：S2P4 TSLOT供三工具，未知槽不授权写缓存。 |
| `docs/diagnostics/crash-2026-10-12-01-27-26.log` | CrashLogger | [归并段](history-animation.md#hkx-mapping)：HKX43跨帧裸node崩；stack-scan不是已证调用链。 |
| `docs/diagnostics/crash-2026-10-13-22-34-00-afp9-audit-palette-deref.log` | CrashLogger | [归并段](history-rendering.md#afp-rendering)：AFP9 palette读名访问0x1010，栈上骨名不是根因。 |
| `docs/diagnostics/crash-2026-10-15-22-45-27.log` | CrashLogger | [归并段](history-animation.md#oar)：22:45 BipedAnim空rcx，傀儡双手Equip链；不能归罪单一第三方。 |
| `docs/diagnostics/crash-2026-10-15-23-02-27.log` | CrashLogger | [归并段](history-animation.md#oar)：23:02 poll_equipment空rbx读type，与前崩点不同。 |
| `docs/diagnostics/crash-2026-10-15-23-55-47.log` | CrashLogger | [归并段](history-animation.md#oar)：23:55再现BipedAnim空rcx，旧Actor装备链约束。 |

## 本轮验证口径

删除前核验144项原件字节/哈希、受Git跟踪与恢复对象，并在总结/本表形成后内部审阅；随后仅删除批准清单中的单个docs文件。清理后检查144项覆盖、保护原件、当前相对路径/锚点、索引可达、范围与diff空白。没有重建DLL，没有新单元或游戏行为测试。文档检查不改变G1未通过、G2–G6未开始，也不关闭当前动画缺陷。实际数量、结果与命令退出码见清理审计。
