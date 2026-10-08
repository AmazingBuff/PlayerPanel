# 场景图复制实验：设计、游戏验证步骤和回传问题

日期：2026-10-08。实验标识：`CharacterPanel-scopy-S0-2026-10-08`。

## 目标与当前边界

从玩家已经装配完成的第三人称 3D 图取得独立副本，复用现有摄影棚直绘和合成框架。
本实验不创建 NPC base 或 Actor，不修改源角色的 data3D、父节点、姿态、库存或装备。
引擎克隆函数本身的运行时行为仍须游戏验证；隔离审计不是对整个引擎、全部模组回调或 GPU 资源的形式化证明。

当前实现 S0：手动复制、可变引用审计、冻结姿态绘制、旋转副本、手动刷新换装结果和释放。
HKX 动作驱动、CBPC 和 SMP 的独立注册与模拟没有实现。安装物理模组时，副本中的头发／衣物应保持捕获时的姿态；它们不动不是本轮失败。
手动刷新复制的是玩家已经生效的装备结果，不是在副本上独立 EquipObject，也不代表连续自动换装同步已经完成。

## 设计流程

1. 输入回调只排队命令，实际复制、绘制和释放在现有 DrawInterface 渲染回调执行。
2. F7 仅允许在已打开、游戏处于暂停状态的 InventoryMenu 中执行。加载菜单／主菜单拒绝复制。
3. 持有第三人称根及其子节点的 NiPointer，记录源节点集合、局部／世界变换、包围盒和几何数量。
4. 调用当前 CommonLib 的 `NiObject::Clone()`（本地绑定 68835/70187），明确记录调用前后两条日志。
   这与旧实验的 `CreateDeepCopy()` 是不同入口。此处依赖本地接口声明；没有声称在当前运行时反汇编验证过克隆实现。
5. 遍历副本，检查根独立、节点／蒙皮／shader property 独立、可变缓存不与源共享，
   rootParent、bones、boneWorldTransforms、pass geometry 和 fadeNode 不回指源图。
   同时检查节点／几何数量一致，及按树遍历次序对应的节点名称／RTTI 一致。
6. 有共享／外部引用、非法数组或结构差异时记录 BLOCKED，不摆位、不绘制、不自动回退 Actor 路线。
   缺少首次绘制才产生的骨骼槽位仅作为 `unbound-slots` 观测；是否能够正确初始化由 F8 验证，避免再次把初始化挡在绘制门外。
7. 审计通过后，只在副本节点／shader property 上解除 controller，并清除副本节点的 collisionObject 和 userData 关联。
   通过结构对应关系写入捕获时的世界变换和包围盒，然后交给 CharacterClone 的 snapshot 构造入口。
8. 摆位采用 `摄影棚根变换 × 捕获根变换的逆`，整体变换捕获的每个节点世界姿态和包围盒。
   不执行复制的 UpdateDownwardPass／Actor／facegen 动画回调，不重设 T-pose，保留捕获到的 SMP 世界姿态。
9. F8 才开启直绘；没有有效绘制结果就不合成旧纹理。F9 旋转副本 90 度；F10 释放。
10. 关闭背包只隐藏副本，保留它以测试与源模型生命周期的隔离；读档／新游戏消息排队清理，主菜单也清理。
    实际析构留在渲染回调中。输入和主线程消息不直接销毁正在绘制的图。

本轮不修复不明确的克隆后骨骼映射，不按名称强行重绑。先取得 native Clone 的审计证据，再按实际失败字段设计 S1 修复。
`SOURCE-UNCHANGED=false` 也可能来自其他模组在暂停中继续更新源角色，不能仅凭这个字段认定 Clone 修改了源图。
共享的 NiSkinData、NiSkinPartition、材质、纹理和静态网格存储没有被一律深拷贝；当前路径不主动修改这些资源，
但它们的运行时可变性仍是后续动作／物理接入必须另行审计的边界。

## 构建和传输

```powershell
cmake -S . -B build-scopy -DCHARACTER_PANEL_SCENE_COPY_EXPERIMENT=ON -DCHARACTER_PANEL_BUILD_PROBE=OFF
cmake --build build-scopy --config Release --target CharacterPanel CharacterPanelCopyMathTest
ctest --test-dir build-scopy -C Release --output-on-failure
cmake --build build-scopy --config Release --target CharacterPanelSceneCopyPackage
```

The package target writes `dist/CharacterPanel-scopy-S0-<version>.zip` containing
`SKSE/Plugins/CharacterPanel.dll`, its matching PDB, `CharacterPanel-scopy-README.txt`
(the in-game controls and install rules), and `build-manifest.json`. The manifest
records the SHA-256 of both binaries, the repository HEAD, and the actual
`extern/CommonLibSSE` checkout, because the submodule pointer in history can lag
the checkout the compiler read; a package built here also prints all three to the
build log. Use the package instead of a bare DLL when a result has to be tied to
one binary.

默认 CMake 开关为 OFF，保留 Actor 装配路线。开启实验后，此构建不调用旧 Actor 出生／摘图路线。
实际测试包的 `build-manifest.json` 记录 DLL、PDB 的 SHA-256、插件源码基线和实际 CommonLib checkout。
仓库子模块指针尚未提交更新，另一台电脑从旧 HEAD 构建时不能忽略这个差异；直接使用测试包可避免不同 DLL 混淆。

本机已验证（2026-10-08）：实验配置 Release 构建通过（`scene_graph_copy.cpp` 零错误），
`snapshot_transform` 单测 1/1 通过，测试包生成后 DLL／PDB 的 SHA-256 与 manifest 自证一致。
游戏验证仍未开始。

将包内 `SKSE/Plugins/CharacterPanel.dll` 和对应 PDB 安装到一个单独的 MO2 测试 mod，替换原 CharacterPanel。
不要同时加载旧 CharacterPanel、CharacterPanelProbe 或 CharacterPanelProto；Probe 的 F7/F8 与本实验冲突。
本插件元数据仍声明 Skyrim AE 1.6.1170 / SKSE 2.2.6。其他版本不属于本轮通过范围。
使用测试存档，原始 DLL 保留用于撤销。本实验没有部署到本机游戏目录，也不要求覆盖原存档。

日志通常位于文档目录的 `My Games/Skyrim Special Edition/SKSE/CharacterPanel.log`；MO2 日志重定向时以实际路径为准。
每次启动游戏会覆盖该日志。结束一次测试后先复制日志，再重启游戏。

## 按键

| 按键 | 行为 | 可见结果与日志 |
| --- | --- | --- |
| F7 | 释放旧副本，复制并审计当前第三人称图 | READY 或 BLOCKED／FAILED；仍不显示摄影棚 |
| F8 | 开始／停止绘制已通过审计的副本 | `DRAW-TOGGLE`，首次和之后低频 `SCOPY DRAW` |
| F9 | 旋转副本 90 度 | `ROTATE angle-deg=...`；世界人物不应旋转 |
| F10 | 释放副本 | `RELEASE reason=F10`；摄影棚隐藏 |

按键只观察按下事件，不消费游戏输入。若其他 mod 也绑定这些键，应在测试前避开冲突。

## 测试 A：复制、绘制和生命周期（先做）

1. 启动并读取存档。先等待角色脸部和装备加载完成，保持普通站立姿态。
2. 打开背包，等待 2 秒。不要在装备还在加载或切换中的同一瞬间按 F7。
3. 按 F7 一次。日志应有 BEGIN、CLONE-RETURN、SOURCE-UNCHANGED、AUDIT、CENSUS，以及 READY。
4. 若 FAILED／BLOCKED，不按 F8，不切换到旧 Actor 路线解释结果；保存完整日志。
   如果只留下 BEGIN 就崩溃，保存 CrashLogger 日志与 PDB；这只能定位到克隆调用阶段，不能直接断言角色类型不支持克隆。
5. READY 后按 F8，截取面板画面。检查身体、脸、手、脚、头发、护甲和武器；检查黑块、碎面、模型飞散、头身分离。
6. 连按 F9 四次，逐次观察正面、侧面、背面和恢复方向。退出背包确认世界角色没有移动、转向、隐身或姿态改变。
7. 不按 F7，重新打开背包。副本应仍是第一次捕获的外观和姿态。
8. 重复 F10 → F7 → F8 十次，再反复开关背包十次。记录是否崩溃、画面残留或越来越卡。
9. 读取另一个存档／经主菜单读取存档。没有再次 F7/F8 前应没有旧人像；之后重新捕获新角色。
10. 保存日志与截图，并填写回传表。若出现世界人物异常，结束该轮并回传，不继续积累结果。

## 测试 B：换装、源图变化和外观／物理模组（A 通过后）

1. 用当前常用装备捕获并绘制一次。保持副本，不按 F7。
2. 在背包中更换护甲／头盔／靴子／左右手武器。副本暂时保持旧装备是冻结快照的预期行为。
3. 等装备实际装配完成，按 F7；再次 READY 后按 F8。检查是否完整匹配新装备及是否保留体型、肤色、妆容／纹身等外观。
4. 关闭背包，让世界角色转身、蹲伏、播放动作。重新打开背包、不按 F7，检查副本是否仍保持旧姿态且能独立旋转。
5. 第一人称打开背包并 F7/F8，检查完整第三人称副本；再测试室内／室外切换后的旧快照和重新捕获。
6. 在安装 CBPC／FSMP／3BA／UBE 等的实际环境重复前面步骤，列出精确版本、骨架和身体。
   物理头发／衣物不继续模拟是预期；几何缺失、绑定到原角色、姿态飞散或影响原角色才是本轮问题。
7. 若通过关闭背包后的换装／角色图重建可以使旧副本出错，记录触发动作、首次异常时间和对应截图。

## 回传问题

| 编号 | 要回答的问题 | 需要的证据 |
| --- | --- | --- |
| Q01 | F7 的 native Clone 是否返回有效独立节点？在哪一条日志后失败？ | BEGIN 到 READY／BLOCKED／FAILED 的完整片段；崩溃日志 |
| Q02 | 源图局部／世界变换及节点集合是否保持不变？是否有世界人物异常？ | SOURCE-UNCHANGED 与退出背包后的观察 |
| Q03 | 哪种可变资源被共享，或哪个引用回指源图？ | AUDIT 所有字段，不能只回传 READY／失败文字 |
| Q04 | 节点／几何／动态几何／蒙皮数量是否一致？ | CENSUS、GEOMETRY 列表 |
| Q05 | F8 后完整人物是否正确显示？ | 正面截图、首条 SCOPY DRAW、实际 passes 数量 |
| Q06 | F9 是否只旋转副本？旋转后是否有头发或身体留在原处？ | 正／侧／背面截图及世界观察 |
| Q07 | 关闭背包、源角色换装或动作变化后，旧副本是否独立存活？ | 操作顺序、截图、日志 |
| Q08 | 手动 F7 刷新后，新装备和运行时外观是否准确？ | 换装前后截图、capture 编号 |
| Q09 | 第一人称、室内外切换、读档／主菜单转换能否正确处理？ | 每个场景结果、RELEASE 记录 |
| Q10 | 连续复制／释放和菜单循环是否稳定？ | 次数、异常时点、完整日志 |
| Q11 | CBPC／FSMP 环境中的静态物理几何是否完整，源角色物理是否恢复正常？ | 模组版本、几何截图；不把副本冻结误判为模拟失败 |
| Q12 | 是否有体型、覆盖纹理、脸部动态数据、武器或扩展装备丢失？ | 精确外观组合和前后截图 |

S0 的可执行回传表使用 Q01–Q12。后续轮次另回答以下问题；本包没有对应驱动，不要求现在操作：

- Q13（S2）：兼容 HKX 能否只驱动副本？暂停时进度是否独立？切换动作、根运动与武器附着是否正常？
- Q14（S3-CBPC）：能否为副本建立独立配置／节点状态并定向推进？原角色与附近 NPC 是否保持冻结？
- Q15（S3-FSMP）：新头发／护甲的 XML 与节点映射是否正确？只有副本模拟吗？释放是否注销其系统？
- Q16（S4）：连续快速换装是否最终显示最新请求，并保持动作、CBPC、SMP 正确续接？

## 后续关卡（本轮未实现，不作为 S0 已通过项）

| 阶段 | 实施内容 | 下一轮关键问题／通过条件 |
| --- | --- | --- |
| S1 | 根据 Q03 修复必要的引用映射与可变数据隔离，测试源 3D 卸载 | 修复后源／副本都不变；旧副本在源重建后可画且可安全释放 |
| S2 | 独立加载兼容 HKX，建立轨道→副本骨骼映射和展示时钟 | 只驱动副本；暂停可播放／切换；根运动不移动世界人物；不覆盖物理骨骼 |
| S3 | 分别确认 CBPC、FSMP 的独立注册、定向推进及销毁入口 | 世界暂停时只有副本模拟；原玩家／NPC 状态冻结；关闭后无跳变／残留 |
| S4 | 持续装备同步、异步请求版本控制和物理重建 | 快速连续换装最终显示最新装备；旧结果不覆盖新结果；动作／物理正确续接 |

如果 S3 只能通过恢复全局模拟实现，必须回到架构选择，不把“副本动了”当作独立物理已通过。

## 研究依据与限制

- 当前本地 CommonLib checkout：`94faaed0c60eddd8347767f2d4d29a97c93bde8c`；实际测试包另记录哈希。
- [CommonLib 节点克隆声明](https://ng.commonlib.dev/class_r_e_1_1_ni_node.html)、[蒙皮实例布局](https://ng.commonlib.dev/_ni_skin_instance_8h_source.html)。声明和结构只能支持实验入口，不能替代运行时 ABI／整体人物复制验证。
- [旧 run 27 记录](stage2-p-instance-plan.md)：CreateDeepCopy 未得到可用根；不能推导所有克隆入口均不可行。
- [FSMP 活动与注册管理](https://github.com/DaymareOn/hdtSMP64/blob/dev/src/ActorManager.cpp)：复制图不自动建立独立模拟。
- 本机完成编译／静态检查不代表游戏验证通过。各 Q 的结论以另一台电脑的实际日志和观察为准。
