# HKX3 实测证据：动画目录与绑定集布局（2026-10-10）

- 构建身份：`CharacterPanel-scopy-HKX3-e32eafc31c-cl94faaed0c6-20261009T162129Z`，与
  `dist/CharacterPanel-scopy-HKX3-1.2.1.zip` 内 `build-manifest.json` 一致（`source_baseline_dirty=false`，基线 `e32eafc31c`）。
- 日志：[CharacterPanel-hkx3-catalogue-20261010-0026.log](diagnostics/CharacterPanel-hkx3-catalogue-20261010-0026.log)
  （SHA-256 `0eefd18992101123f516804ba4b47fb25a63862424e216dcc86c224f104b4c6f`）。**无崩溃**；两次捕获（471 / 1851 节点），
  绘制正常，只有既有的 `Studio lights …` 警告。

## 1. 名字表：typed 路径成立，且与绑定集**同序**（一半成功）

```
SCOPY ANIM catalogue character='DefaultFemale' rig='Character Assets Female\skeleton_female.hkx' behavior='Behaviors\0_Master.hkx' names=15203 bindings=15203
SCOPY ANIM catalogue idle-names=1526/15203 first='Animations\1hm_blockidle.hkx@20, Animations\1hm_idle.hkx@23, Animations\2hm_blockidle.hkx@123, …'
```

- `hkbCharacterSetup::data → hkbCharacterData::stringData → animationNames` 这条路**读通了**：CommonLibSSE 对那一格的
  注释写着"与 `mirroredSkeletonInfo` 的归属未定"，实测**是** `stringData`（否则不会给出合理的角色名、rig 与行为文件名）。
- **`names == bindings == 15203`**：名字表与绑定集**同序**这一假设成立（否则两个计数不会一致），
  所以"索引 → 动画名"可以直接用。名字是 **HKX 文件路径**（不是人类的动作名），共 1526 个含 "idle"。
- 名字表**按路径排序**，所以最前面的 idle 都是武器/物件待机（`1hm_blockidle`、`2hm_idle`、`altar_leftidle`…）。
  下一轮要找"裸 idle"要看**文件名词干**以 `idle` 开头的那些（HKX4 起会单独打印这一组）。

## 2. 两个候选布局**都不成立**（另一半失败，但失败得很具体）

```
SCOPY ANIM catalogue layout candidate=element-as-binding valid=0/8 duration=(0.00..0.00)s
SCOPY ANIM catalogue layout candidate=element-holds-binding-pointer valid=0/8
```

- 8 个采样元素**都非空且可读**（否则不会计入 8 次尝试），所以问题不在"数组里是不是指针"，而在**元素内部**：
  元素既不是 `hkaAnimationBinding` 本身，也不以"指向 binding 的指针"开头。
- 最可能的形态：元素是 `hkReferencedObject`，**第一个 qword 是 vtable**，`hkaAnimationBinding` 是它的成员
  （因此偏移 ≥ 0x10）。HKX4 用**偏移表 × 取值方式**（值/指针）逐行结构化校验，并把每个候选**失败的原因**打出来
  （`unreadable` / `no-animation` / `tracks-unreadable` / `duration` / `track-bones`），同时**原始转储**两个元素的
  前 0x40 字节——即使所有候选都不中，也能照着字节把布局读出来。
- 本轮暴露的仪器缺陷（已修）：上一版 `read_binding` 只在早期失败时返回全零，日志里看不出**卡在哪一步**；
  这正是项目自己那条"失败面必须可读"的规则。

## 3. 判定与下一步

- HKX3 的目标一半达成：**动画索引可以命名**（15203 条、同序、1526 条 idle、裸 idle 可筛）；**元素布局尚未确定**。
- HKX4（已实现并打包）：偏移表 + 取值方式的结构化核对（含失败原因）+ 原始转储 + `object-like` 计数 +
  裸 idle 名单与逐条 `valid/reason/duration/tracks`。通过后 HKX5 就能用 `binding->animation->SampleTracks(t, …)`
  采样并写进副本（写路径已由 HKX2 证明），真值取同一相位的 `poseLocal`（按 `boneNodes` 索引）。
- 两次捕获的 `map … other-unresolved` 分别是 22 与 4，与 HKX1/HKX2 一致：跳过清单必须是逐骨运行时状态。
