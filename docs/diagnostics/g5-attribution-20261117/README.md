# C-G5-ATT v1：门 5 G4P-7 失败归因审计（2026-11-17，POST-HOC）

**状态：POST-HOC 描述性审计，已完成；不是预注册判据，不改变门 5/G4P-7 的 FAIL 裁定。** 本审计只回答“失败集中在哪些骨、静态跨侧偏移占多少”，用于决定下一版协议要闭合哪个前置；任何删轨、放宽 tau、静态相等替代姿态等价，都必须另立预注册并在新采集上验证。

## 一、背景与边界

- 门 5 正式批次：248 对，G4P-1..6、8、9 成立（G4P-6 带 IDUN 锚点说明），**G4P-7 FAIL**；`E_p/E_r/E_s` 超容差 3,826 / 16,755 / 352（主分母 24,552）。
- 失败主轴向静态装备/附件骨集中：`Weapon`、`WeaponDagger`、`WeaponBack`、`Quiver`、`WeaponBow`、`WeaponMace`、`WeaponSword`、`WeaponAxe`、`Pauldron`、`NPC COM`。代表样本 bone 64 旋转逐位相同、平移差约 4.08。
- 本审计输入为已归档正式工件与 K1 census 骨名；**不重新采样、不重跑游戏、不修改代码/INI/容差/覆盖**。

## 二、输入

| 输入 | SHA-256 |
| --- | --- |
| formal manifest | `8D61E0F41C56FEC66A44DB93502F31B250414262FC0FB4B40A312594CA78E144` |
| formal detail | `51A44CEBF6D572A28BF5DC3D43458DEB8D30A7D96E2863FEA4C0B5D9A7D78D04` |
| candidate bin | `5686348BDBEA2F63B3618B7F4136A160227AA4E3014E55EB1B8D1247A3C0C31B` |
| source bin | `4187890D7FF33E6B4F45E0D9319ABF98FCCB3F8D84254BD7FE15646F8985CF7C` |
| coverage | `3DF45251748EF7C7F80381A57A196901298C48A0C43E782E378B83030DD0A52C` |
| tolerances | `B810698EC180B108A1515A21324A37E5DC293175AF05C18B19EA2AC7D55710EE` |
| bone names | K1 census `CharacterPanel-g5k1-census-v101-run1-20261114-1411.log` |

## 三、方法（事后定义，明确非预注册）

工具：[`tools/gate5_attribution.py`](../../../tools/gate5_attribution.py)，纯 stdlib、只读输入、输出固定排序；SHA-256 `4C42778CE92BF5AAD5E8BCC6ED48FCBC5704EE45942AF51C5A92B3E0EC4D6E9A`。

- 对每条已配对骨（116 轨）：读取候选/源 10 分量，计算两侧在配对样本上的逐分量 span（最大分量极差）。
- `static_both` 定义：候选 span ≤ `static_epsilon` **且** 源 span ≤ `static_epsilon`；主审计取 `1e-3`，并报告 `1e-6/1e-4/1e-2` 敏感性。该阈值是**事后描述选择**，不是门 4/门 5 判据。
- 分组：`auxiliary` = COV A17（99..115）；`attachment` = 名称含 `Weapon`/`Quiver`/`Pauldron`/`COM`；`body` = 其余。
- 统计每骨的 `exceed_p/r/s`、`max/p99/p50 E_p`、精确零计数；汇总必须满足 `body + attachment + auxiliary = G4P-7 总数`。
- 输出：[attachment-attribution.json](results/attachment-attribution.json)（SHA-256 `2D88EBAC133BB2172DEEB63AA2E07F499DD46169C0E3A912D933E6397EA3D994`）与 [attachment-attribution.txt](results/attachment-attribution.txt)（SHA-256 `77FE05A3B2E7020057DBB80DB96AD30B927A3ED3DB9C564FA5A0811523C03CFA`）。两次同参调用逐字节一致。

## 四、结果

| 组 | 骨数 | 样本 | exceed_p | exceed_r | exceed_s | static_both 骨 | static exceed_p |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| body | 88 | 21,824 | 1,226 | 15,852 | 351 | 6 | 0 |
| attachment | 11 | 2,728 | 2,600 | 903 | 1 | 5 | 1,240 |
| auxiliary A17 | 17 | 4,216 | 0 | 0 | 0 | 17 | 0 |
| **合计** | 116 | 24,552 | **3,826** | **16,755** | **352** | 28 | 1,240 |

**静态跨侧偏移**（`static_both` 且 `max E_p > tau_p`）：

| 索引 | 名称 | max E_p | exceed_p | 候选 span | 源 span |
| ---: | --- | ---: | ---: | ---: | ---: |
| 60 | `Quiver` | 4.21502542 | 248 | 9.16e-5 | 8.39e-5 |
| 61 | `WeaponAxe` | 3.4892807 | 248 | 1.68e-4 | 1.58e-4 |
| 63 | `WeaponBow` | 4.2150197 | 248 | 6.84e-5 | 7.61e-5 |
| 64 | `WeaponDagger` | 5.5912056 | 248 | 6.15e-5 | 5.91e-5 |
| 66 | `WeaponSword` | 3.8015337 | 248 | 1.13e-4 | 1.08e-4 |

**敏感性**：

| `static_epsilon` | `static_both` 骨 | static exceed_p |
| ---: | ---: | ---: |
| `1e-6` | 12 | 0 |
| `1e-4` | 25 | 744 |
| `1e-3` | 28 | 1,240 |
| `1e-2` | 28 | 1,240 |

## 五、解释与边界

1. **静态附件偏移是真实子问题**：5 个未使用武器/箭袋节点在两侧都近似不动（span ≤ 1.7e-4），但跨侧平移相差 3.49–5.59；它们贡献 1,240/3,826 次 `E_p` 超容差。候选的注入装备变量没有产生与源实际装备一致的静态附件局部变换。
2. **不是全部**：body 88 轨仍贡献 1,226 次；attachment 的动态部分贡献 1,360 次（2,600-1,240）。因此“只删静态装备骨”不能解释或通过 G4P-7；`Weapon`/`COM`/`Pauldron` 与 body 的差异仍指向 K5-5 层/权重、父级/COM 与时间对齐。
3. **A17 不是本轮失败来源**：17 条 auxiliary 的 `E_p/E_r/E_s` 超容差均为 0；但这是描述性结果，不把 A17 升格为已控制。
4. **阈值敏感性**：静态偏移的计数依赖 `static_epsilon`；本审计不选一个“好看”的阈值。下一版若要把静态附件单列，必须在**新采集前**预注册分类规则、相等阈值与失败语义，并保留全部 116 轨的姿态账本。

## 六、裁定与下一入口

**本审计不改变门 5 裁定：G4P-7 仍 FAIL，G5 未通过。** 下一版协议应优先处理：

- 候选静态附件/装备初态与源实际装备的相等性（K2-C1 上下文 + 候选图静态节点）；
- K5-5 层/权重与 `Weapon`/`COM`/`Pauldron` 动态差异；
- body 残余的时钟/父级对齐审计。

任何“静态附件独立判据”都只能作为**新增的预注册轴**，不能回填为本轮 G4P-7 的通过条件。
