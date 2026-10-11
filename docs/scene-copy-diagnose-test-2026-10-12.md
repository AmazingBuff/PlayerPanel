# HKX40 测试单（2026-10-12，恒等档诊断补全 + 隔离装备再基线）——**待上机**

- 构建身份：`CharacterPanel-scopy-HKX40-c683faca89+dirty-cl94faaed0c6-20261011T161917Z`
  （基线 = HKX39 判读提交 `c683faca89`，其上为未提交的两项小改）。
- 包：`dist/CharacterPanel-scopy-HKX40-1.2.1.zip`，DLL SHA-256 `60ec0028…`
  （全值 `60ec00284f71b9e9efbd8eccdd09ead5f224938eabe9198590ce29d9400eea51`），
  **DLL 与 PDB 均已从最终 zip 部署到 `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\` 并校验一致**。
- 单测 1/1。
- 出处：[调度器 v2 测试单](scene-copy-scheduler-v2-test-2026-10-11.md) HKX39 判读的后续两项。

## 判读前提（为什么这轮不动泊位）

HKX37（泊玩家身边）与 HKX39（泊深）的 ab **都只到 37 骨**——37 不是泊位位置问题，而是
**名字桥的固有可比面**：动画骨架用 vanilla 名（NPC Pelvis 等），CBBE/UBE 图的骨架挂 CME 系
改名骨，两者交集 ≈37 根核心骨（HKX1 当年测得 boneNodes 名字一致率 37/116——同一回事）。
37 根核心骨就是 idle 动画的可见主体，所以这轮不搬泊位，先把诊断补齐、消噪音，下一轮按数据
决定映射路线。

## 本轮内容（两处小改）

1. **ab 行补 `named=` 与 `calm=`**：`named` = 名字桥到达的骨数（pose map 大小——钉死 37 的成因：
   若 named≈37，则是桥固有面而非绑定残缺）；`calm` = 比较骨中旋转差 ≤1° 的数量——单根野骨
   不再掩盖"大体正确"的映射（37 骨里 calm=36 → 修一根；calm=10 → 系统性错位）。
   判定阈值不变（≥24 骨且 max-rot ≤1° → match）。
2. **隔离仪器装备再基线**：`poll_equipment` 检测到玩家装备变化 → `note_world_change("world-equipment")`
   → 当前隔离会话以 `end reason=world-equipment` 收口（污染基线的合法动作获得交代），下一个
   操作的普查以新世界状态重新基线——HKX39 那种 64 行 [E] 复读不再出现。

## 上机步骤（**第一步就是读档**，还两轮欠账）

1. 确认部署：DLL SHA-256 `60ec0028…`。
2. **进存档 → 立刻读档一次**：日志应见 `SCOPY PUPPET released reason=load-boundary` →
   新 `born form=…`（新 formID）→ `parked`；零崩溃零残留。
3. 开背包 F7/F8，等几次切换：收集 ab 行（重点看 `named=` 与 `calm=`）。
4. **equip 当场性目视**（HKX39 欠的）：面板开着，**眼睛盯着面板人物**换一次右手武器——
   人物当场是否做出拔/换手动作；日志 `equip … pushed=draw` 之后的 digest 是否切 1HM 族。
5. 装备变化后继续操作：确认**不再出现** object-set DRIFT 复读（应见
   `ISOLATION end reason=world-equipment` + 新 baseline 行）。
6. 日志复制进 `docs/diagnostics/`（惯例名 `CharacterPanel-hkx40-diagnose-20261012.log`）。

## 判读表

| # | 检查 | 通过判据 | 失败时 |
| --- | --- | --- | --- |
| 1 | 读档边界 | released → born 新 formID；零崩溃 | 回传（三连欠账就立专项） |
| 2 | ab 诊断 | ab 行带 `named=37` 量级 + `calm=K` | named 异常大/小 = 桥的另一形态，回传 |
| 3 | **calm 判读（决定映射路线）** | 多数切换 `calm/compared ≥ 80%` 且 max-rot 小 = 恒等大体成立、个别骨错 → 逐变体标定；`calm/compared < 50%` = 轨道序系统性错位 → HKX41 上**经验映射求解器**（逐轨道对 poseLocal 最优匹配，实测而非猜表） | —— |
| 4 | equip 当场性 | 面板人物当场有拔/换动作 + digest 切 1HM 族 | 无动作 = 杠杆只推了状态没推混合，回传 |
| 5 | 隔离再基线 | `end reason=world-equipment` 后零 object-set 复读 | 复读仍在 = 再基线没触发，回传 |
| 6 | 零回归 | 零崩溃、无新 `[E]`、驱动链/digest 照常 | —— |

---

## HKX40 判读（已实测，2026-10-12 凌晨；日志 [CharacterPanel-hkx40-diagnose-20261012.log](diagnostics/CharacterPanel-hkx40-diagnose-20261012.log)）

| # | 检查 | 结果 |
| --- | --- | --- |
| 1 | **读档边界（还两轮欠账）** | **通过**——`released reason=load-boundary` → 新 `born`，零崩溃零残留 |
| 2 | ab 诊断 | **named=39 全轮恒定**——名字桥到达骨数 = vanilla 名 ∩ CBBE 图名 ≈39（与 HKX1 的 37/116 同源），桥本身健康；`bones=37/99 unnamed=62` 与之自洽（62 条轨道指向桥外骨） |
| 3 | **calm 分布（路线开关）** | **系统性错位，触发换轨**——五族武器 idle 的 calm/compared：1HM 23/37(62%)、2HW 14/37(38%)、2HM 16/37(43%)、Bow 18/37(49%)、1HM 17/37(46%)——多数 <50%，恒等映射对本机替换件**整体失效**（不是个别野骨） |
| 4 | **equip 杠杆（彻底实证）** | **通过**——7 次换装 `pushed=draw` 全部触发，**每次家族切换在 3–4 秒内跟上**（00:26:42 push → 00:26:46 2HM ab；00:27:01→05 2HM；00:27:08→11 1HM；00:27:22→25 Bow……）——暂停期内装备→OAR 重决策→驱动链换族的闭环全程可测 |
| 5 | 隔离再基线 | **通过**——第二次换装即 `end reason=world-equipment verdict=CLEAN`；`[E]` 从 64 行降到 **2 行**（剩余 = 已知 SuperSprayBreath 粒子对） |
| 6 | 零回归 | 零崩溃；手动推图/驱动链/digest 照常 |

### 路线判定：恒等档判死，HKX41 上经验映射求解器

恒等映射对本机 OAR 替换件系统性错位（<50% calm），"逐变体标定"没有意义。**求解器设计**
（HKX41，依据本轮数据定型）：

- **真值源换轴**：poseLocal 只覆盖 39 个 vanilla 名——改用**傀儡自己的 3D 节点 local**（按骨名，
  与显示映射同名空间，覆盖 ~93 骨，引擎直写、逐帧真值）。
- **求解**：每条轨道在 K≈16 个相位采样，与候选节点 local 的旋转逐相位比，最优匹配 = 该轨道的
  真实目标骨。O(99×93×16) 四元数比较 ≈15 万次，每 clip 一次（按 animation 指针缓存），毫秒级。
- **验证闭环**：求解表回填显示映射后，ab 协议（同相位 vs 节点 local）应回到浮点量级——
  HKX28 的 0.15° 判据在求解映射上复现才算通过。
- **日志**：`SCOPY PUPPET solve tracks=99 solved=K identity-agrees=J`（多少轨道引擎本来就按
  恒等排——顺带量化 OAR 替换件的轨道重排规律）。

遗留队列：**HKX41 求解器（第 1）** → S1 安全回收 → OAR fork（免立项）。
