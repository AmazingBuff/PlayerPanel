# AFP9 verdict 1：覆盖审计的 palette 解引用崩溃，已修复待复测

日期：2026-10-13。构建：`CharacterPanel-scopy-AFP9-ce35ad79e3+dirty-cl94faaed0c6-20261013T141655Z`（DLL SHA-256 `143328ac…`）。
现场日志：[崩溃日志](diagnostics/crash-2026-10-13-22-34-00-afp9-audit-palette-deref.log)、
[插件日志](diagnostics/CharacterPanel-afp9-first-test-20261013.log)。
测试单：[AFP9 测试单](afp9-coverage-and-direct-draw-test-2026-10-13.md)。

## 1. 会话结果：V1 校准成立，审计行未产出即崩

用户操作序列（日志时间）：

```
22:33:19.793  F4 -> tick+apply
22:33:19.819  verify generation=0 samples=98 names skeleton=116 tree=92 table=37
              bone-nodes 40/88=0.45 | skeleton 88/92=0.96 | match=1 reason=matched   ← V1 步骤 1 成立
22:33:21.001  apply blocked reason=no-pose（暂停态、未开 force，符合预期）
22:33:57.246  F4 -> tick+apply+force；force-generate on
22:34:00.944  ANIM restart at panel open capture=0 driver=procedural-idle     ← 开背包
22:34:00.944  PUPPET update-data ... force-generate=1                          ← 暂停帧首次带 force 推进
              ---- 崩溃 ----
```

判读：世界运行中的校准、F4 档位链、面板打开、暂停帧 force 生成全部按设计走完；**崩溃发生在本轮新增的覆盖审计里**，审计行从未打印，因此 V1 的对账数字与点名本轮**没有取得**。V2（F6 直绘）本轮未开始。

## 2. 崩溃签名与根因

```
Unhandled exception "EXCEPTION_ACCESS_VIOLATION" at 0x7FFB336BB78A CharacterPanel.dll+002B78A
  cmp [rcx], r12b        rcx = 0x0000000000001010     Tried to read memory at 0x1010
CALL STACK
  [3] figure_audit.cpp:83  figure_skin_palette(RE::NiAVObject&)
  [4] final_pose_apply.cpp:515  FinalPoseApply::audit_slots(...)
  [5] final_pose_apply.cpp:451  FinalPoseApply::apply(RE::Actor&)
  [6] final_pose_apply.cpp:640  FinalPoseApply::tick(bool)
```

根因：`figure_skin_palette()` 把 `NiSkinInstance::bones[i]` 当作可信节点**解引用**（`name_present()` → `node->name.c_str()` → `*name`），而 palette 槽位**不保证是可读的 NiAVObject**——0x1010 就是被当成字符串指针读的槽位内容，`cmp [rcx], r12b` 正是那次首字节读取。

对照证据：崩溃前插件已上机数周的探针版 `skin_palette()`（`final_pose_probe.cpp`）用的是**同一段遍历**，但它只把指针放进集合做成员判定、**从不解引用**，所以从未出事。AFP9 的审计为了按名字分类，额外读了名字——引入了这条新路径。栈上出现的 `skeleton_female.nif`／CME／`NPC L Joint 1` 等对象只是遍历现场，不是嫌疑方；`~unordered_map<NiAVObject const*, int>` 符号来自 `/OPT:ICF` 折叠的误报。

## 3. 修复（AFP9b）

1. `figure_skin_palette()` 改为**只收集指针**、绝不读槽位内容（与探针已验证形态一致）；结构体字段由 `names` 改为 `nodes`。
2. 分类改由**调用方自己在角色树里按名字解析出的节点**完成成员判定：`figure_slot_necessity(name, node, palette)`。解析不到的槽位（unreachable 情形）按规则只能是非 necessary——本树里没有节点，任何 palette 都不可能引用它。
3. 探针里的局部 palette 实现删除，改为共用同一份（同一内存规则不留第二份实现），两处行为不变。
4. 审计行补充 `resolved=x/y`（名字是否在树中解析到节点）与 `nodes=` 计数，判读不再依赖猜测。
5. 单元测试更新到新签名，并新增判据：节点为空的槽位永不为 `necessary`。

## 4. 复测要求（AFP9b 包）

按测试单重跑：V1 看 `SCOPY AFP9 audit ... resolved=/` 与对账恒等式；V2 走 F6 直绘、A5vsA4、restore 对账与关面板回归。
判读补充：本条修复只消除"审计解引用"，**并不证明** palette 槽位语义已被完全理解——审计只按指针成员判定，若某骨确实必要却因名字解析失败被判 helper，日志里会体现为 `resolved=0` + `helper`，此时以该行回传，不据它下结论。

## 5. 留档教训

`NiSkinInstance::bones` 的槽位可以不是可读节点；任何遍历它的代码**只能比较指针**，不得读名字、不得调用虚函数、不得当作 NiAVObject 使用。这条与 run 51（Disable 毁 3D）、run 104（出生深度决定 LOD）同级，写入维护文档不变量。
