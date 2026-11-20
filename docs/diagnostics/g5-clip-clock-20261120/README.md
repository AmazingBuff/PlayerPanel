# H-C1 离线审计：装备 clip 的采样时钟（2026-11-20）

**裁定：H-C1 否证——采样用的就是探针记录的 `clip->localTime (+0xF0)`，不存在第二条时钟。** 于是 v6 run 1 里 11 帧 `localTime` 冻结意味着该 clip 每帧采样到的轨完全相同；而同一份日志的 `clip-ref` 用同一 `localTime` 重采样、与捕获姿态只有 16/99 一致（`max-dpos=2.344827`），说明**捕获姿态不是该 clip 在该时刻的采样输出**。下一层应查「采样之后谁写这些轨」，而不是再找第二个时钟。

## 一、输入与身份

| 项 | 值 |
| --- | --- |
| 解包镜像 | `build/g1-audit-20261023/SkyrimSE.original.exe.unpacked.exe`，36,950,016 B，SHA-256 `843823C115DBC56B5E9740350DB49170CC4EEA013A79513B7FD066DEEC719A2B` |
| 地址库 | `extern/CommonLibSSE/tests/REL/versionlib-1-6-1170-0.bin`，795,129 B，SHA-256 `C4093C569A3C83B26587F4B9EA4C55DE9AE6E73B84A2AF9FB3FBD30E2FE0D452` |
| CommonLibSSE | `94faaed0c6`（`ng`） |
| 运行时 | AE 1.6.1170；下文地址均为镜像 RVA，运行时地址 = 模块基址 + RVA |

| 对象 | vtable ID | RVA | RTTI 校验 |
| --- | ---: | --- | --- |
| `hkbClipGenerator` | AE 226785 | `0x1963E78` | `.?AVhkbClipGenerator@@`（offset 0、sig 1） |
| `hkaDefaultAnimationControl` | AE 232980 | `0x198A2B8` | `.?AVhkaDefaultAnimationControl@@`（offset 0、sig 1） |

`hkbClipGenerator` 的 vtable 共 27 槽：slot 4 `0xACD750`（Activate）、slot 5 **`0xACDE40`**（Update）、slot 7 `0xACE1E0`（Deactivate）、slot 23 **`0xACE400`**（Generate）、slot 25 `0xACE930`。`hkaDefaultAnimationControl`：slot 3 `0xBD9440`（Update）、slot 4 `0xBD9750`（GetFutureTime）、slot 5 `0xBD99C0`（SampleTracks）、slot 6 `0xBD9B80`（SamplePartialTracks）。

## 二、字段与代码核对

`hkbClipGenerator`（CommonLibSSE `RE/H/hkbClipGenerator.h`，大小 `0x110`）：

| 偏移 | 字段 | 引擎侧引用 |
| --- | --- | --- |
| `+0x58` | `cropStartAmountLocalTime` | `0xACFE38`/`0xACFE47` `addss xmm,[rdi+0x58]` |
| `+0x5C` | `cropEndAmountLocalTime` | 布局；`0xAD01F0` 附近按同样结构读取 |
| `+0x64` | `playbackSpeed` | `0xAD0222` `mulss xmm0,[rbx+0x64]` |
| `+0x68` | `enforcedDuration` | `0xAD0206` `movss xmm1,[rbx+0x68]` |
| `+0x6C` | `userControlledTimeFraction` | `0xACDF33`、`0xACE169` |
| `+0x72` | `mode` | `0xACDF1D`、`0xACFED4`（`==2` 用户控制、`==3` ping-pong） |
| `+0x88` | `animationControl` | `0xACDF04`、`0xACDF9C`、`0xACE40B`、`0xACFE6A` |
| `+0xF0` | `localTime` | `0xACDF0B`（读）、`0xACDFA3`（写）、`0xACE424`（Generate 读） |
| `+0xF4` | `time` | `0xACE09E`（读）、`0xACE0BA`/`0xACE0C2`（累加写） |
| `+0xF8` | `previousUserControlledTimeFraction` | `0xACE16C`（写）、`0xACFE2E`（读） |
| `+0x104` | `atEnd` | `0xACDF14`、`0xACDF8A`、`0xACE014`、`0xACFDF2` |
| `+0x106` | `pingPongBackward` | `0xACE061`、`0xAD0229` |

`hkaDefaultAnimationControl`（`RE/H/hkaDefaultAnimationControl.h`）：`localTime +0x10`、`weight +0x14`、`transformTrackWeights +0x18`、`binding +0x38`、`playbackSpeed +0x5C`、`cropStartAmountLocalTime +0x9C`、`cropEndAmountLocalTime +0xA0`。引擎侧：`0xACDF11`/`0xACDFAB`/`0xACE42A` 写 `+0x10`；`0xACFE74`/`0xACFE9B` 读写 `+0x5C`；`0xACFEE6` 读 `+0x9C`。

## 三、时钟链路（四段，全部有反汇编支撑）

**1) 采样真的读 `control+0x10`。** 两个采样入口都把该字段作为时间参数传给底层采样：

```text
; hkaDefaultAnimationControl::SampleTracks  0xBD99C0
bd99ef: movss xmm1, dword ptr [rdi + 0x10]   ; t = control->localTime
bd99fc: mov   r8, rdx                        ; out tracks
bd99ff: call  qword ptr [rax + 0x18]         ; animation->Sample(t, ...)
; hkaDefaultAnimationControl::SamplePartialTracks  0xBD9B80
bd9b94: movss xmm1, dword ptr [r11 + 0x10]   ; t = control->localTime
bd9bb7: call  qword ptr [r10 + 0x20]         ; animation->SamplePartial(t, ...)
```

**2) Generate 把 clip 的 `localTime` 灌进控制。**（`0xACE400` 开头，`r8 = [rcx+0x88]`）

```text
ace40b: mov  r8, qword ptr [rcx + 0x88]      ; control
ace424: mov  eax, dword ptr [rcx + 0xf0]     ; clip->localTime
ace42a: mov  dword ptr [r8 + 0x10], eax      ; control->localTime = clip->localTime
```

**3) Update 自己推进时钟。** `time` 按步长累加，`localTime` 由 helper `0xACFDE0` 从「当前 localTime + 步长×速率」算出并夹在「时长 + crop 起点」：

```text
ace09e: movss xmm1, dword ptr [rbx + 0xf4]   ; time
ace0ba: addss xmm6, dword ptr [rbx + 0xf4]
ace0c2: movss dword ptr [rbx + 0xf4], xmm6   ; time += step*rate
acdf9c: mov   rax, qword ptr [rbx + 0x88]
acdfa3: movss dword ptr [rbx + 0xf0], xmm0   ; clip->localTime = helper out
acdfab: movss dword ptr [rax + 0x10], xmm0   ; control->localTime = 同值
; helper 0xACFDE0（非用户控制分支）
acfe57: mov  eax, dword ptr [rcx + 0xf0]     ; seed = 当前 localTime
acfe9b: mulss xmm6, dword ptr [rcx + 0x5c]   ; step × rate
acfef3: addss xmm6, dword ptr [r15]          ; += seed
acfef8: addss xmm0, xmm7                     ; 时长 + control->cropStart(+0x9C)
acfefc: comiss xmm6, xmm0
acff01: movss dword ptr [rsi], xmm0          ; 到端 → 夹住
acff05: mov  byte ptr [rbp], 1               ; at-end 标志
```

**4) 到端后 Update 不再刷新 `localTime`，但 `time` 继续走。**

```text
acdf8a: cmp  byte ptr [rbx + 0x104], dil     ; atEnd ?
acdf91: jne  0xacdfb0                        ; 非零 → 跳过 F0/控制写入
```

`mode == 2`（用户控制）时整段 `localTime` 写入也走另一分支（`0xACE057`/`0xACE071` → `0xACE169`，只写 `+0xF8`）。

## 四、落到 v6 run 1 的读数上

同一份日志的 `clip-ref` 行给出该 clip 的静态读数：`mode=0`、`speed=1.000`、`duration=1.4333`、`enforced=0.0000`、`crop-sum=0.0000`、`eff-duration=1.4333`、`map-len=0`、`mapping=identity`。

| 事实 | 取值 | 含义 |
| --- | --- | --- |
| 装备 2–12 步 `localTime` | 恒 `0x3E555556` = 0.2083333432674408 | 采样时间确实没推进 |
| 到端夹点 | `duration + cropStart` = 1.4333 | 0.2083 不是夹点 ⇒ `atEnd` 不是冻结原因 |
| `mode` | 0（单次播放） | 走「Update 自己推进」分支 |
| `time (+0xF4)` | 本轮未采 | **待 v7 判别**：推进 ⇒ 只因 `atEnd` 停写；也冻结 ⇒ 该帧步长为 0 |
| `clip-ref` 一致性 | `agreed=16/99`、`max-dpos=2.344827`（baseline 即如此） | 捕获姿态 ≠ 该时刻的 clip 采样 |

因此本轮矛盾的正确表述是：**采样时间冻结 ⇒ 该 clip 的采样输出逐帧相同 ⇒ 11 帧姿态差异出自采样之后**，而不是「存在另一条隐藏时钟」。

## 五、剩余机制与判别量（v7 最小读取集建议，未实施）

| 机制 | 说明 | 判别量 |
| --- | --- | --- |
| M1 权重斜坡 | 控制按 `transformTrackWeights`/`weight` 做部分加权采样，权重爬升可在时间冻结时移动姿态 | `control+0x14`（weight）、`+0x18`（数组 size/data） |
| M2 采样后写入者 | 生成之后仍有写入者改这些轨（E2-A/E2-B 已列为 58423 足部 IK、布料、手部 IK/附件） | 现有 E2 探针读数，不需要新偏移 |
| M3 捕获点/映射 | 捕获缓冲或轨道→骨骼映射与采样结果不是同一对象 | 同帧 `post-sample` 与下一帧前后对照 |

建议 v7 只在既有 26 个观测点追加只读字段：`clip +0xF0/+0xF4/+0x104/+0x72/+0x64/+0x58/+0x5C`，`control +0x10/+0x14/+0x18(size)/+0x5C/+0x9C/+0xA0`。全部经 `plausible_object_pointer`，不新增 detour，不改 `TransitionProbeRecord`/CPK5 布局。

## 六、复现命令

从仓库根目录执行（`disasm_func.py` 与镜像在本地 gitignored 目录）：

```powershell
cd build\g1-audit-20261023
python -B disasm_func.py 0xacde40   # hkbClipGenerator::Update（time/localTime/atEnd）
python -B disasm_func.py 0xace400   # Generate 开头：control+0x10 = clip+0xF0
python -B disasm_func.py 0xacfde0   # 局部时间 helper（seed / step×rate / 夹点）
python -B disasm_func.py 0xad01f0   # rate = playbackSpeed × duration/enforcedDuration
python -B disasm_func.py 0xbd99c0   # SampleTracks：t = control+0x10
python -B disasm_func.py 0xbd9b80   # SamplePartialTracks：t = control+0x10
```

vtable/RTTI 交叉核对（Python，`pe_tools`）：

```python
from pe_tools import Image, parse_versionlib
img = Image(r'build/g1-audit-20261023/SkyrimSE.original.exe.unpacked.exe')
fmt, ver, db, trailing, ptr = parse_versionlib(r'extern/CommonLibSSE/tests/REL/versionlib-1-6-1170-0.bin')
for name, ident in [('clip', 226785), ('control', 232980)]:
    rva = db[ident]
    print(name, hex(rva), img.rtti_class(rva))
```

预期：`clip 0x1963e78 ('.?AVhkbClipGenerator@@', 0, 1)`、`control 0x198a2b8 ('.?AVhkaDefaultAnimationControl@@', 0, 1)`。

## 七、边界

- 只对 AE 1.6.1170 + 上述镜像成立；SE/VR 未审计。
- 本审计只回答「采样时间取自哪个字段」。它不证明运行时哪一帧步长为 0、也不证明 M1/M2/M3 中哪一个成立——这些要 v7 的读数。
- 不改变 `G4P-7 FAIL`、G5/G6 与任何 `tau_*`；不据此修改探针或 INI。
