# Build CharacterPanel

更新：2026-11-03。[当前证据交接](independent-behavior-graph.md#handoff-20261102-evidence)拥有运行结论：G1/G2 有界通过、G3-A 成立、G3-B/F 相位回放通过；G4 历史 PASSED 保留，A numeric 全窗覆盖由 [v63 运行](#v63-g4a4-numeric-types-2026-11-02)关闭，C 兼容源事件窗口由 [v66 运行](#v66-g4c1-pinned-graph-2026-11-02)关闭，范围见 [G4 收官](independent-behavior-graph.md#g4-closure)。[audit28](behavior-graph-construction-audit.md#audit28)完成 G5 首轮原生审计，门 1 NOT_READY；按 [E1→E2→E3→联合证据验收](independent-behavior-graph.md#g5-evidence-plan)继续，当前 E1 静态审计，三片探针均未实现。[G5 v2](independent-behavior-graph.md#g5-draft)仍须仪器→独立校准→完整冻结→新正式验证，G5 未冻结/未通过，G6 后置。本次仅修改文档。

最新记录安装为 [v66](#v66-g4c1-pinned-graph-2026-11-02)，DLL `190c7236…` / PDB `b4f2596d…`，C1 裁定后部署 INI 回 dormant `27f481f5…`；不是重新读取安装目录的核验。当前 `PLUGIN_SOURCES` 实数 **32**，保留的未跟踪 util.cpp 不在编译清单。v45 的 30 与 v60 叙述的 33 保留为日期记录，后者计数误差以前述当前清单为准。

[v46 正常工作流](scene-graph.md#v46-manual-regression)已于 2026-10-30 用同源重建身份观察，非 20261029 归档逐字节验证。以下日期段落描述工件生成/当次操作；历史“未部署/未上机”不代表后来仍未执行，source commit 不会重建或重标 DLL/PDB。

## Purpose

Build the current Windows DLL from this checkout. Release is the only configure/build preset and `CharacterPanel` is the only first-party target. The result is `build/Release/CharacterPanel.dll` with matching debugging symbols.

<a id="current-research-status-2026-10-26"></a>
<a id="current-research-status-2026-10-29"></a>
## Current research status (2026-11-03)

[G1/G2 的限定通过、G3-A 与 G3-B/F 相位通过](independent-behavior-graph.md#verification-matrix)均有留存运行证据。G2 的 109/116 写骨与三未知缺失名、同 controller 来源/generation 替换和 frozen-only 等限制不变；G3-B 不能替代完整姿态验收。

G4 的两项缺口已由 A4 全窗计数与 C1 并发钉图观察关闭，仍保留装备族/58377 路由等限定。共享 data/绑定与候选哈希复现不证明源候选完整姿态/时钟/层一致。[G5 v2](independent-behavior-graph.md#g5-draft)以有资格门的 R2 完整原生输出为正式主参照方向，R1 仅诊断；首轮审计后门 1 仍 NOT_READY，[E1–E3 方案及稍后验证前置/回传清单](independent-behavior-graph.md#g5-evidence-plan)拥有下一步。**记录安装 v66 不含新 E1–E3 探针，既有开关不能产生新证据；当前无新验证包或 INI 配方。** 未来实现须先评审 native 兼容性、通过聚焦检查并匹配归档/实际加载的 DLL/PDB、build identity、设置与资源，再按该片协议运行；完整仪器与独立校准后才冻结正式协议。G6 保持完整非豁免生命周期范围；装载 teardown NO-GO。

<a id="v18-frame-trace-build"></a>
## V18 bounded frame-trace verification (2026-10-26)

The [pre-registered observation contract](independent-behavior-graph.md#g2-v18-frame-trace) adds [FrameTrace](../src/animation/behavior_graph/frame_trace.cpp), its [engine-free policy/progress](../src/animation/behavior_graph/frame_trace_policy.h) and [two cases](../tests/behavior_graph_frame_trace_test.cpp). Only inject on exactAE1.6.1170 reads new fields. The original native call/write sequence is retained; no hook, dependency, settings or CMake target is added.

The selected verification is the two direct-compiled policy/progress cases and one Release configure/build with `CHARACTER_PANEL_BUILD_LABEL=behavior-graph-v18-frame-trace-20261026`, followed by DLL/PDB identity, hashes, ZIP entry checks and document/source checks. Actual commands/results and the isolated package manifest are under `build/behavior-graph-v18-trace-20261026/`; the two cases and the Release build passed. The 2026-10-26 rebuild ran with full file access, so the earlier MSB4184 SDK-metadata denial did not recur; the build emitted only the existing `cl` D9025 `/Ob2`→`/Ob3` warnings and configure only the existing install-rule/cache-toolchain warnings. No dependency update was required. The two engine-free cases were compiled with the local `g++` 15.1 (`C:/env/mingw64/bin/g++.exe`, C++23, `-Wall -Wextra -Werror`); the earlier record's `C:/env/llvm/bin/clang++.exe` does not exist in this environment. Repository `IndependentGraph` remains `off`; the test-package INI fragment enables the existing `inject` value for manual testing. The package does not install itself and contains no game binaries. New native writer/normal-frame/context/payload evidence remains UNVERIFIED; the [first v18 run](independent-behavior-graph.md#g2-v18-run) is recorded there and G2 is still NOT PASSED.

Build identity: `CharacterPanel-scenegraph-behavior-graph-v18-frame-trace-20261026-7ee4cce3bf-cl94faaed0c6-20261026T143615Z` (built 2026-10-26 22:36 local from clean HEAD `7ee4cce3bf1168c39fbcae4e61ae0277a6463685`, submodule `94faaed0c60eddd8347767f2d4d29a97c93bde8c`). Matching CodeView/PDB GUID `{7F586C2E-9F62-4CE8-B33B-D19AFE9C6461}`, age 317; DLL SHA-256 `72c33a0e8c3416b2402f8b0b8ec432f5d12eaa476f765811e1b6f8ea31fe9b20` (1,249,280 bytes), PDB SHA-256 `1baceb553bdda16a8a0ad3b40ca0910f0c6c914871831e9f1e414e82840efbb8` (35,409,920 bytes). The isolated package is `build/behavior-graph-v18-trace-20261026/test-package/` and `CharacterPanel-behavior-graph-v18-frame-trace-20261026.zip` in its parent; per-file hashes, source hashes, toolchain and provenance are recorded in the manifest.

Rebuild reason: the first v18 record (identity `…-92a3ed071d+dirty-cl94faaed0c6-20261026T063108Z`, DLL `e24ad2a5…`, ZIP `cd364569…`) describes artifacts that are absent from this checkout — `build/` was still configured with label `scenegraph` and held only the v17 pair — so the same committed source was rebuilt at `7ee4cce`. Two inert differences follow: the label carries the post-commit identity instead of the pre-commit `92a3ed0+dirty` one, and the previously untracked, unreferenced `src/base/util.cpp`/`util.h` no longer exist here, so the unchanged CMake glob no longer compiles them (content search finds no reference). The earlier DLL/PDB/ZIP hashes therefore remain history only; the loaded `SCENEGRAPH BUILD` identity and this manifest are authoritative.

Deployment (2026-10-26): the on-disk v17 pair was preserved first (`build/backup-v17-20261026/`, DLL SHA-256 `904D6B5B…`, PDB `1B208E04…` — both match the v17 record), then backed up as `dist/deploy-backup-20261026-v17/` together with its `off` INI. The MO2 mod `CharacterPanel/SKSE/Plugins/` now holds this package's DLL (`72c33a0e…`, 1,249,280 bytes) and PDB (`1baceb55…`, 35,409,920 bytes) with `IndependentGraph=inject` in the deployed INI (1,042 bytes; only the value changed, all other keys untouched). The repository sample stays `off`. Reverting means restoring those two backups and restarting the game.

First v18 run (2026-10-26 23:01): the loaded `SCENEGRAPH BUILD` line reported exactly the identity above, so the run used this package. It produced the full 30 stage rows plus one summary, 40 INDEPENDENT GRAPH rows and 36 BEHAVIOR GRAPH rows, with no `[E]` and no crash; the raw log is archived verbatim as [CharacterPanel-g2v18-trace-20261026-2301.log](diagnostics/CharacterPanel-g2v18-trace-20261026-2301.log) (40,366 bytes, SHA-256 `D42DB99D…`). After adjudication the deployed INI was returned to `IndependentGraph=off`; the v18 DLL/PDB stay deployed and the v17 backups remain the documented revert. The [run record](independent-behavior-graph.md#g2-v18-run) and the [narrowed audit questions](independent-behavior-graph.md#next-audit) live in the experiment document; this guide records only the build/package/run facts.

The current ZIP is `CharacterPanel-behavior-graph-v18-frame-trace-20261026.zip` (9,763,102 bytes, SHA-256 `dad6b72ce17e53ae0d7e823bbe5bec28ad29aa0cf3f98dffa5885b42081c8ae2`); its five entries (`SKSE/Plugins/CharacterPanel.dll|.pdb|.ini`, `README-测试步骤.md`, `manifest.json`) were extracted and re-hashed against the manifest, and the DLL/PDB/INI hashes match. A later Git commit does not modify or rebuild a package; the `7ee4cce3bf` identity and manifest describe this build. The package stays frozen as the artifact of the adjudicated run recorded below; `completed` in that log counts returned 63581 calls, not successful sampling. No new experiment is scheduled before the next bounded audit.

Reproduction commands (two engine-free cases total, no old suites):

```powershell
C:/env/mingw64/bin/g++.exe -std=c++23 -Wall -Wextra -Werror -Isrc tests/behavior_graph_frame_trace_test.cpp -o build/behavior-graph-v18-trace-20261026/frame_trace_test.exe
build/behavior-graph-v18-trace-20261026/frame_trace_test.exe
cmake --preset Release -DCHARACTER_PANEL_BUILD_LABEL=behavior-graph-v18-frame-trace-20261026
cmake --build --preset Release
```

<a id="v19-eye-arm-build"></a>
## V19 eye-arm experiment build (2026-10-26)

The [v19 contract](independent-behavior-graph.md#g2-v19-contract) adds one record field to the inject branch: a read-only `BSAnimationUpdateData::optionalEyePosition` (+0x18) pointing at the candidate root's `+0xA0..+0xA8` shifted +900 on Z, or a zero vector when the root is null. The trace becomes `GRAPH FRAME schema=v19` — three appended fields per stage row (`root218/b248/eye`) plus `arm-observed` in the summary — with the 30+1 and 40-row budgets unchanged. Drive/construct/load modes keep the v18 record.

Build identity: `CharacterPanel-scenegraph-behavior-graph-v19-eye-arm-20261026-e17d5d12f6-cl94faaed0c6-20261026T153952Z` (clean HEAD `e17d5d12f6`, submodule `94faaed0`); CodeView GUID `{7F586C2E-9F62-4CE8-B33B-D19AFE9C6461}`, age 319; DLL SHA-256 `344d9abb8373292322e2e1216d85291fa218b4cea175cb171633777567fba274` (1,249,792 bytes), PDB SHA-256 `9848cd143671bd7c7d8b47a1ad1aff7e53b9ab8c53945f7020703f5af0185868` (35,418,112 bytes). Configure/build emitted only the existing D9025 `/Ob2`→`/Ob3` warnings and the existing install-rule warning; no dependency update.

The engine-free case set is now three: the v18 policy/progress pair plus one case for `make_eye_point` (null root → zero point, root → +900 on Z) and `arm_observed` (reset value false, armed interval true). All three pass under g++ 15.1 with `-Wall -Wextra -Werror`.

**Deployed (2026-10-26) and run once, not packaged.** The v18 pair (DLL `72C33A0E…`, PDB `1BACEB55…`) and its `off` INI were backed up to `dist/deploy-backup-20261026-v18/`; the MO2 mod `CharacterPanel/SKSE/Plugins/` holds this build's DLL (`344d9abb…`) and PDB (`9848cd14…`). The 23:46 session loaded exactly the identity above and produced the full 30 stage rows plus one summary with `arm-observed=no`, no `[E]` and no crash; the raw log is archived verbatim as [CharacterPanel-g2v19-eye-arm-20261026-2346.log](diagnostics/CharacterPanel-g2v19-eye-arm-20261026-2346.log) (41,470 bytes, SHA-256 `45AAEE09…`). Adjudication: pre-registered branch 6 — the arm block's outer gates passed but `63864()` caps the step below `1.25*dt`, so no interpolation timer was rewritten. After adjudication the deployed INI was returned to `IndependentGraph=off`; the v19 DLL/PDB stay deployed and the v18 backups remain the documented revert. See the [v19 run record](independent-behavior-graph.md#g2-v19-run).

Reproduction commands:

```powershell
C:/env/mingw64/bin/g++.exe -std=c++23 -Wall -Wextra -Werror -Isrc tests/behavior_graph_frame_trace_test.cpp -o build/v19-frame-trace-test.exe
build/v19-frame-trace-test.exe
cmake --preset Release -DCHARACTER_PANEL_BUILD_LABEL=behavior-graph-v19-eye-arm-20261026
cmake --build --preset Release
```

<a id="v20-generation-entry-build"></a>
## V20 native generation-entry test (2026-10-27)

[Audit22](behavior-graph-construction-audit.md#audit22) supplies the native63589 ABI and mode0 synchronous branch. The [pre-registered test](independent-behavior-graph.md#g2-v20-generation-entry) uses the [production value guard](../src/animation/behavior_graph/generation_entry_policy.h) and [one parameterized engine-free case](../tests/behavior_graph_generation_entry_test.cpp); the22value rows passed with assertions enabled, without native calls. Earlier trace/progress/eye algorithms are unchanged and their old evidence is reused, not rerun as new acceptance.

The intended local verification is Release configure/build with label `behavior-graph-v20-generation-entry-20261027`, matching DLL/PDB inspection, protected input/source hashes, links and isolated ZIP CRC. The initial Release configure/build passed; a review-only const-byte/comment repair was followed by an incremental compile/link of independent_graph.cpp and an up-to-date completion check (exit0), without rerunning the unchanged guard case. Configure reported the existing install-rule warning and compilation the existing /Ob2→/Ob3 warning. The SDK metadata directory was read through authorized build permissions. No dependencies, CMake targets, runtime configuration defaults, original asset executable or vendor declarations change. Game execution/deployment are not performed. Latest actualrunv19, v20 pending and G2 not passed.

Final identity: `CharacterPanel-scenegraph-behavior-graph-v20-generation-entry-20261027-242d0a50c5+dirty-cl94faaed0c6-20261027T070457Z`. DLL SHA-256 `6c76c2a7a5cbbe32437bef3634404ecde9928d5cc6eff73d367ca146f54d84b7` (1,251,328 bytes), PDB SHA-256 `9d2a09e98f9ae9161f9398fb4ee94e2fcd50addb93f3e8692e4898f4f442f43e` (35,196,928 bytes); matched CodeView/PDB GUID `{748DB13E-5AAA-4883-8283-751805A36ED4}`, age79. The new isolated folder is `build/behavior-graph-v20-generation-entry-20261027/test-package/`, ZIP `CharacterPanel-behavior-graph-v20-generation-entry-20261027.zip` in its parent. Manifest records source hashes (including unchanged preexisting util.* compiled by the existing glob), input hashes and restrictions; no game image/tool/SDK is inside the ZIP. Native runtime evidence remains pending.

ZIP SHA-256 `3f9f9182c0590d9285fa2fd77895cc35496da3ee75a3f70adf6ac4f50b0b8a49` (9,469,280 bytes), five entries; CRC and each file hash match. This identity/manifest describe the build-time `242d0a5+dirty` workspace; a later Git commit will not rewrite the existing package. Return the complete log plus actual loaded identity/manifest and first-open/close/abnormal times; guard-skip has only ENTRY by design. No new result is pre-judged.

The [v20 handoff](independent-behavior-graph.md#v20-handoff) is the current stop. The frozen package recorded above is absent from this checkout, so the same committed source was rebuilt, repackaged and redeployed on 2026-10-27 (see [rebuild and deployment](#v20-rebuild-2026-10-27)); that deployed attempt then crashed before its entry, so 63589 has still never run and G2 has not passed ([run record](independent-behavior-graph.md#g2-v20-run)). Any further attempt returns the complete log plus the actually loaded identity/manifest and first-open/close/abnormal times; guard-skip has only ENTRY by design. Later commits do not change the recorded artifact hashes of either build.

The command below uses the local `g++` 15.1 (`C:/env/mingw64/bin/g++.exe`); the earlier record's `C:/env/llvm/bin/clang++.exe` does not exist in this environment.

```powershell
C:/env/mingw64/bin/g++.exe -std=c++23 -Wall -Wextra -Werror -Isrc tests/behavior_graph_generation_entry_test.cpp -o build/behavior-graph-v20-generation-entry-20261027/generation_entry_test.exe
build/behavior-graph-v20-generation-entry-20261027/generation_entry_test.exe
cmake --preset Release -DCHARACTER_PANEL_BUILD_LABEL=behavior-graph-v20-generation-entry-20261027
cmake --build --preset Release
```

<a id="v20-rebuild-2026-10-27"></a>
### Rebuild and deployment (2026-10-27)

Rebuild reason: the frozen v20 package above (identity `…-242d0a50c5+dirty-cl94faaed0c6-20261027T070457Z`, ZIP `3f9f9182…`, DLL `6c76c2a7…`) is not present in this checkout — `build/behavior-graph-v20-generation-entry-20261027/` did not exist, `build/Release` held the v19 pair, and no `*v20-generation-entry*` artifact existed on this machine. The committed source at `a83e522` (the commit that added the guarded native generation experiment on the same parent `242d0a5`) was rebuilt and repackaged; the earlier package was built from an uncommitted tree whose bytes are unrecorded, so byte identity is not claimed and its hashes remain history only.

New identity: `CharacterPanel-scenegraph-behavior-graph-v20-generation-entry-20261027-a83e5229e7-cl94faaed0c6-20261027T134152Z` (clean HEAD `a83e5229e7e25f84a1e210d290bf2dc57e0fd02b`, submodule `94faaed0`; the embedded identity carries no `+dirty` suffix because the tree and submodule were clean at configure time). DLL SHA-256 `13688a9d243873f25ae153e8776cbee3d70b8760a10311042234da3ae0cc1535` (1,253,888 bytes), PDB SHA-256 `07a509da9b0a9b91e76cf222f5002c57dd99bb832caf56b9ebcfed78724b9668` (35,467,264 bytes); CodeView/PDB GUID `{7F586C2E-9F62-4CE8-B33B-D19AFE9C6461}`, age 320 (same GUID family as v18 age 317 and v19 age 319). Configure and build exited 0 with only the existing install-rule warning and the existing `cl` D9025 `/Ob2`→`/Ob3` warnings; no dependency install or update. The engine-free guard case was compiled and run with the local `g++` 15.1 under `-Wall -Wextra -Werror`: `1 parameterized generation-entry ownership case passed (22 value rows, engine-free)`.

Package: `build/behavior-graph-v20-generation-entry-20261027/test-package/` holds `SKSE/Plugins/{CharacterPanel.dll|.pdb|.ini}`, `README-测试步骤.md` and `manifest.json` (`a9ae936d…`, 6,999 bytes); the sibling ZIP `CharacterPanel-behavior-graph-v20-generation-entry-20261027.zip` is 9,750,395 bytes, SHA-256 `076cf6f793a8a8bc5939bda85dc6eb5c24416ad363324cb19c1958af2e164e3e`, five entries with forward-slash names. Extracted and re-hashed against the manifest: DLL/PDB/INI hashes match. The package INI (1,207 bytes, `7128173d…`) is the repository sample plus `[Experiment] IndependentGraph=inject` and keeps both diagnostics flags false; the repository sample stays `off`.

Deployment (2026-10-27 21:42): the on-disk v19 trio was backed up first to `dist/deploy-backup-20261026-v19/` (DLL `344d9abb…` 1,249,792 bytes, PDB `9848cd14…` 35,418,112 bytes, INI 1,034 bytes `875bb216…`, all matching the v19 record), and the MO2 mod `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\` now holds this build's DLL (`13688a9d…`) and PDB (`07a509da…`) with the deployed INI at 1,055 bytes (`1db104fa…`) carrying `IndependentGraph=inject`; every other deployed key is unchanged. The documented revert is now this v19 backup; the earlier `dist/deploy-backup-20261026-v18/` pair remains as the second step back.

First deployed run (2026-10-27 21:48): the game loaded this identity (the log's `SCENEGRAPH BUILD` line matches) on exact AE 1.6.1170 with `IndependentGraph=inject`, the private-state guard passed, and the process then crashed in the pre-existing read-only g3-gates diagnostic before the drive loop — `CharacterPanel.dll+0x28067`, `independent_graph.cpp:955` — so 63589 never ran and no `GRAPH FRAME`, `completed` or pose sample exists. The log and matching crash report are archived; the deployed INI was returned to `IndependentGraph=off` and the v20 DLL/PDB stay in place dormant. Details and the static cause (`[ctx+0x50]` read past a 0x50-byte `hkbContext`) are in the [v20 run record](independent-behavior-graph.md#g2-v20-run) and [audit24](behavior-graph-construction-audit.md#audit24).

<a id="v20a-chain-fix-2026-10-27"></a>
## V20a observation-chain fix build (2026-10-27)

[Audit24](behavior-graph-construction-audit.md#audit24) pinned the v20 crash to an out-of-bounds read: the v16-era `g3-gates` row started its chain at `[ctx+0x50]` while `sizeof(RE::hkbContext)` is 0x50. The fix keeps the v20 experiment unchanged and replaces that diagnostic with [audit23](behavior-graph-construction-audit.md#audit23)'s real chain — `[context]` → `hkbCharacter::character`, `setup` (`+0x50`) → `hkbCharacterSetup::data` (`+0x40`) → `hkbCharacterData::footIkDriverInfo` (`+0x88`) — expressed entirely through declared members, with `[graph+0x100]` typed as `hkbFootIkDriver` (`disableFootIk`, `+0x4A`). Every intermediate passes the new engine-free [`observation_chain_policy.h`](../src/animation/behavior_graph/observation_chain_policy.h) before it is dereferenced, so an implausible value is reported as `null` instead of faulting. The row's fields become `character/setup/data/footik-info`; no native call, budget or acceptance criterion changes.

Build identity: `CharacterPanel-scenegraph-behavior-graph-v20a-chain-fix-20261027-7d18a90787-cl94faaed0c6-20261027T135614Z` (clean HEAD `7d18a907872bf417ba6b3d9d0883c420a4f918cb`, submodule `94faaed0`; no `+dirty`). DLL SHA-256 `691f7592e2fa22c1de5c636c6e0f089a8731dd9aa79aa7f386a24581ba727f54` (1,254,400 bytes), PDB SHA-256 `5b3d8cc8daf5a9b27da298f360e95067144d463d1eeb92c1e1cd2f60c48e7c1c` (35,483,648 bytes); CodeView/PDB GUID `{7F586C2E-9F62-4CE8-B33B-D19AFE9C6461}`, age 321. Configure/build exited 0 with only the existing install-rule and `cl` D9025 warnings. Three engine-free cases were compiled with the local `g++` 15.1 under `-Wall -Wextra -Werror` and pass: the new `observation_chain_policy` case (11 value rows), the v20 guard case (22 rows) and the frame-trace case set.

Package: `build/behavior-graph-v20a-chain-fix-20261027/test-package/` (`SKSE/Plugins/{CharacterPanel.dll|.pdb|.ini}`, `README-测试步骤.md`, `manifest.json` `0f2398cb…` 8,255 bytes); ZIP `CharacterPanel-behavior-graph-v20a-chain-fix-20261027.zip` is 9,747,510 bytes, SHA-256 `f4586e163acd2bd012b749d5d454f07fdb9eebedec40e3965fa0265354c1abf6`, five forward-slash entries re-extracted and re-hashed against the manifest. Package INI 1,332 bytes `c8d7c75e…`; repository sample stays `off`.

Deployment (2026-10-27 21:57): the v20 trio was backed up to `dist/deploy-backup-20261027-v20/` (DLL `13688a9d…`, PDB `07a509da…`, INI 1,078 bytes `95528858…` with the crash note), the MO2 mod now holds this DLL/PDB, and the deployed INI was 1,073 bytes `6e9d2005…` with `IndependentGraph=inject`; all other keys unchanged. The revert chain is now this v20 backup, then `dist/deploy-backup-20261026-v19/`, then the v18 pair.

First deployed run (2026-10-27 22:02): the loaded `SCENEGRAPH BUILD` matched this identity, the guard passed, the corrected g3-gates row reported `character/setup/data/footik-info` all non-null, and the drive completed 120 steps with `arm-observed=yes`, filled output headers and a moving candidate pose (`bc160a8c…`→`63f638a6…`→`8fb4cd33…`) while the source stayed byte-stable — see the [v20a run record](independent-behavior-graph.md#g2-v20a-run). No crash and no `[E]`. After adjudication the deployed INI was returned to `IndependentGraph=off`; the v20a DLL/PDB stay in place dormant, and G2 remains NOT PASSED because no bone-level or finite-value evidence exists yet.

<a id="v21-pose-content-2026-10-27"></a>
## V21 pose-content decode build (2026-10-27)

The [v21 contract](independent-behavior-graph.md#g2-v21-contract) adds a read-only decode of `character.poseLocal` at steps 0/60/120: every entry is read through the declared `hkQsTransform` (0x30 stride, capped at 256 entries), checked for finiteness over translation xyz / rotation xyzw / scale xyz, compared component-wise with the previous checkpoint (1e-6 threshold) and with the source's own `poseLocal`, and the candidate/source identical-entry count is reported for aliasing. Four index-spread bones (`0 / ⅓ / ⅔ / last`, i.e. 0/38/76/115 for the observed 116-bone skeleton) are named in dedicated rows using a bounded, plausibility-checked name read. The pure arithmetic is in [`pose_content_policy.h`](../src/animation/behavior_graph/pose_content_policy.h) with an engine-free case. No native call, write, REL id or previous budget changes; the new rows are `pose-content` (≤3) plus `pose-bone` (≤12), accounted separately from `GRAPH FRAME` 30+1, `INDEPENDENT GRAPH` 40 and `BEHAVIOR GRAPH` 36.

Build identity: `CharacterPanel-scenegraph-behavior-graph-v21-pose-content-20261027-fe7c4d15e1-cl94faaed0c6-20261027T141004Z` (clean HEAD `fe7c4d15e19aae851bdb444d6d501000a9bf7261`, submodule `94faaed0`; no `+dirty`). DLL SHA-256 `e088065152768a1abd3f207e5937cd04a9eb3b6bb7937e1b3ed38527ddb6d2b5` (1,267,200 bytes), PDB SHA-256 `787b30055bef0bd73ffd8f0fb2e029b564f3cdea37516e7445a985f9dc58910f` (35,614,720 bytes); CodeView/PDB GUID `{7F586C2E-9F62-4CE8-B33B-D19AFE9C6461}`, age 322. The first compile failed at `independent_graph.cpp:1080` because `hkArray::operator[]` takes `size_type` (cl C4267 under `/WX`); indexing the declared `data()` array fixed it and the rebuild exited 0 with only the existing install-rule and D9025 warnings. Four engine-free cases pass with the local `g++` 15.1 under `-Wall -Wextra -Werror`: pose content (20 assertions), observation chain (11 rows), generation entry (22 rows) and frame trace (3 cases).

Package: `build/behavior-graph-v21-pose-content-20261027/test-package/` (`SKSE/Plugins/{CharacterPanel.dll|.pdb|.ini}`, `README-测试步骤.md`, `manifest.json` `e2873121…` 6,958 bytes); ZIP `CharacterPanel-behavior-graph-v21-pose-content-20261027.zip` is 9,928,507 bytes, SHA-256 `4cfa5ac83318ef877e7d915bba3a71ecdb17fbc1e8c010025eb73a9baebf93ed`, five forward-slash entries re-extracted and re-hashed against the manifest. Package INI 1,278 bytes `efff49a5…`; repository sample stays `off`.

Deployment (2026-10-27 22:11): the v20a trio was backed up to `dist/deploy-backup-20261027-v20a/` (DLL `691f7592…`, PDB `5b3d8cc8…`, INI 1,059 bytes `6e6f668f…` with the run note), the MO2 mod now holds this DLL/PDB, and the deployed INI is `inject` (1,075 bytes `11798909…`); all other keys unchanged. **One run happened at 22:18**: it hit pre-registered branch 3 (`source-equal` 8/21/15) and stopped, which the [v21 run record](independent-behavior-graph.md#g2-v21-run) audits as a criterion false positive (distinct `poseLocal` addresses, candidate hashes bit-identical across two runs while the source hash differed, and `pose_sample_identical` being exact ten-component equality). The deployed INI returned to `off` at 22:20 and both files stayed; G2 remains NOT PASSED.

<a id="v22-alias-watch-2026-10-27"></a>
## V22 alias-watch build (2026-10-27)

The [v22 contract](independent-behavior-graph.md#g2-v22-contract) replaces the v21 aliasing watch after that watch fired on a criterion the [v21 run record](independent-behavior-graph.md#g2-v21-run) audits as a false positive. `pose_sample_identical` is exact ten-component equality, so counting candidate/source equal entries can never separate constant bones from a shared buffer. The build adds two decidable observations and no native behaviour: `pose_buffer_alias` compares the two `character.poseLocal` pointers, and `watch_aliasing` reports `equal_moving` (identical to the source **and** changed since the step-0 baseline by more than 1e-6) alongside `equal_static`. The raw `source-equal` count is still printed for round-to-round comparability but no longer gates. The `pose-content` row gains three fields and keeps its three-row budget; `pose-bone` rows, the `63589(mode0)` entry, the guard, the eye point, 120 x 1/60 drive, world/ragdoll/slot/event/retention recipes and every other budget are untouched.

Commands (same preset as every previous round; the proxy variables are cleared because this shell exports both cases and MSBuild rejects the duplicate switch):

```powershell
cmake --preset Release -DCHARACTER_PANEL_BUILD_LABEL=behavior-graph-v22-alias-watch-20261027
cmake --build --preset Release
```

Build identity: `CharacterPanel-scenegraph-behavior-graph-v22-alias-watch-20261027-22ef58464f-cl94faaed0c6-20261027T145636Z` (clean HEAD `22ef58464fae657af3f82ae60df2eaffe9b6400b`, submodule `94faaed0`; no `+dirty`). DLL SHA-256 `5756e0e647056cbaf1a8664007328c8289b5c66d441e254975ecd348daa15aa2` (1,268,736 bytes), PDB SHA-256 `b763d05fe0528e74b02413d2f2027cb1697187f706e0bc390c24a12b38bb04d0` (35,631,104 bytes); CodeView/PDB GUID `{7F586C2E-9F62-4CE8-B33B-D19AFE9C6461}`, age 323. Configure and build exited 0 with only the existing install-rule and `cl` D9025 `/Ob2`→`/Ob3` warnings; no dependency install or update. Four engine-free cases pass with the local `g++` 15.1 under `-Wall -Wextra -Werror`: pose content (36 assertions, up from 20), observation chain (11 rows), generation entry (22 rows) and frame trace (3 cases).

Package: `build/behavior-graph-v22-alias-watch-20261027/test-package/` (`SKSE/Plugins/{CharacterPanel.dll|.pdb|.ini}`, `README-测试步骤.md`, `manifest.json` `c9878cc7…` 7,468 bytes); ZIP `CharacterPanel-behavior-graph-v22-alias-watch-20261027.zip` is 9,921,485 bytes, SHA-256 `be5bd284d6c927157828cb22c7ec242533d74af368bd2f420aa48723083d2928`, five forward-slash entries re-extracted and re-hashed against the manifest. The package INI is the unchanged v21 test fragment (`inject`, 1,278 bytes `efff49a5…`); the repository sample stays `off`.

Deployment (2026-10-27 22:59): the v21 trio was backed up to `dist/deploy-backup-20261027-v21/` (DLL `e0880651…`, PDB `787b3005…`, `off` INI 1,058 bytes `98a2d082…` with the run note), the MO2 mod now holds this DLL/PDB (`5756e0e6…` / `b763d05f…`, byte-identical to the package), and the deployed INI is `inject` (1,056 bytes `e7ba0854…`) with the v21 run note replaced by a v22 deployment note. **One run happened at 23:03**: identity matched, no crash, no `[E]`, and the revised gate passed — `alias=no` at all three checkpoints, `source-equal-moving=0` at 60/120, `moved=94`, and two of the four sampled bones moving, with `source-equal-static` equal to the raw `source-equal` count (21/15). That closes the v21 false-positive audit and establishes pose content evidence; G2 remains NOT PASSED for continuity, semantic correspondence and scene application. The deployed INI returned to `off` after the run (1,061 bytes `cc0572ca…`) and both files stayed. See the [v22 run record](independent-behavior-graph.md#g2-v22-run).

<a id="v23-clip-ref-2026-10-27"></a>
## V23 clip-reference build (2026-10-27)

The [v23 contract](independent-behavior-graph.md#g2-v23-contract) turns "meaningfully ahead" into a decidable observation: are the finite, changing, un-aliased `poseLocal` values the sample of an active clip at that clip's own time? Each checkpoint walks the candidate's `activeNodes` (first 256 entries), accepts a `nodeClone` as a clip generator only when its vtable equals `RE::VTABLE_hkbClipGenerator[0].address()`, reads `animationName`/`localTime`/`mode`/`playbackSpeed`/`binding`, samples the bound animation at `localTime` through `SamplePartialTracks`, maps tracks to bones with `transformTrackToBoneIndices` and compares every mapped entry against `poseLocal`. Rotations are compared by `1-|dot|` of the unit quaternions so q and -q agree; epsilons are pre-registered at 1e-4 / 1e-5 / 1e-4. **This is the first round that adds a native call**: `SamplePartialTracks` is a const virtual on `hkaAnimation`, the same path the display player uses every frame, and it writes only the logging lambda's temporary buffer. Entry, guard, eye point, 120 x 1/60 drive, world/ragdoll/slot/event/retention recipes and every earlier row budget are unchanged; the new rows are `clip-ref` (≤3 per checkpoint, ≤9 total).

Commands (proxy variables cleared for the reason recorded above):

```powershell
cmake --preset Release -DCHARACTER_PANEL_BUILD_LABEL=behavior-graph-v23-clip-ref-20261027
cmake --build --preset Release
```

Build identity: `CharacterPanel-scenegraph-behavior-graph-v23-clip-ref-20261027-ddb58bc269-cl94faaed0c6-20261027T151451Z` (clean HEAD `ddb58bc`, submodule `94faaed0`; no `+dirty`). DLL SHA-256 `efa90c218e97cedb07bf65014fc00965f748f5c251a454e2f1dc77c2ddbda686` (1,277,440 bytes), PDB SHA-256 `f7e3e2c69cc58169ae99af5082504d12f8a56683e974134c52e3f7147b426581` (35,696,640 bytes); CodeView/PDB GUID `{7F586C2E-9F62-4CE8-B33B-D19AFE9C6461}`, age 327. Two compiles were needed. The first failed at `independent_graph.cpp:1193` with cl C4834 under `/WX` because `clip_reference_agrees` is `[[nodiscard]]` while the caller only accumulates; that attribute was removed (the function's purpose is the accumulator, the bool is for tests). The second build added the pointer filter the v20 crash demanded: `clip->binding` was previously dereferenced after a null check only, and is now filtered with `plausible_object_pointer` before `animation` is read; that changed the binary, so the identity was re-stamped from the hardening commit instead of shipping a binary whose identity described the earlier source. The recorded build exited 0 with only the existing install-rule and D9025 warnings. Five engine-free cases pass with the local `g++` 15.1 under `-Wall -Wextra -Werror`: pose content (36 assertions), clip reference (19 assertions, new), observation chain (11 rows), generation entry (22 rows) and frame trace (3 cases).

Package: `build/behavior-graph-v23-clip-ref-20261027/test-package/` (`SKSE/Plugins/{CharacterPanel.dll|.pdb|.ini}`, `README-测试步骤.md`, `manifest.json` `2f143a77…` 7,373 bytes); ZIP `CharacterPanel-behavior-graph-v23-clip-ref-20261027.zip` is 9,928,402 bytes, SHA-256 `05ac4a3ef1bc2f7cb25985b74c4850b3010ccc14644ea3197259ff0e836006e9`, five forward-slash entries re-extracted and re-hashed against the manifest. The package INI is the unchanged v21 fragment (`inject`, 1,278 bytes `efff49a5…`); the repository sample stays `off`.

Deployment (2026-10-27 23:16): the v22 trio was backed up to `dist/deploy-backup-20261027-v22/` (DLL `5756e0e6…`, PDB `b763d05f…`, `off` INI 1,061 bytes `cc0572ca…` with the run note), the MO2 mod now holds this DLL/PDB (byte-identical to the package), and the deployed INI is `inject` (1,072 bytes `655a9c89…`). **One run happened at 23:19**: identity matched, no crash, no `[E]`, and the round produced 8 `clip-ref` rows. Branch 5 fired: the only real comparison (step 0, `male\MT_Idle.HKX`) agreed on 22 of 99 tracks with `max-dpos` 3.98, and that checkpoint still holds the bind pose. At steps 60/120 every reported binding has an empty `transformTrackToBoneIndices`, so `mapped=0` and no comparison happened — a state the contract did not pre-register. Structurally the round is the most informative so far: the candidate runs `H2H_Idle` + `1HM_Idle` + `SCAR_1hmReadyDummy` as a synchronised stack, with clip time advancing 0.0167 → 0.1833 → 1.2000 over 120 steps (2.0 s simulated). The deployed INI returned to `off` (1,077 bytes `02f37ace…`). G2 remains NOT PASSED. See the [v23 run record](independent-behavior-graph.md#g2-v23-run).

<a id="v24-layer-census-2026-10-27"></a>
## V24 layer census build (2026-10-27)

The [v24 contract](independent-behavior-graph.md#g2-v24-contract) closes the gap the v23 run exposed: at steps 60/120 every active clip reported `mapped=0` because its `hkaAnimationBinding::transformTrackToBoneIndices` is empty. [Audit25](behavior-graph-construction-audit.md#audit25) settled that state at instruction level on the AE 1.6.1170 image — `ConvertTrackPoseToBonePose` (REL 60005, `0xB03380`) does `0xB033C0 cmp [rdx+8],0` / `0xB033C4 sete`, then `0xB03405 movzx esi, bx` on the empty branch (`bone = track`) and `0xB03411 movzx esi, word [rax+rcx*2]` on the table branch — so this build compares with the engine's own identity semantics instead of skipping. Two further observations ride along: the census rises from 3 to 8 slots and reports `animationBindingIndex` and map length, and a `clip-clock` row at steps 0/20/40/60/80/100/120 turns the clip clock into a curve. `collect_active_clips` is shared by both rows; the only native call is still the const `SamplePartialTracks`. No policy header changed.

Commands (proxy variables cleared for the reason recorded above):

```powershell
cmake --preset Release -DCHARACTER_PANEL_BUILD_LABEL=behavior-graph-v24-layer-census-20261027
cmake --build --preset Release
```

Build identity: `CharacterPanel-scenegraph-behavior-graph-v24-layer-census-20261027-50c2396d66-cl94faaed0c6-20261027T153521Z` (clean HEAD `50c2396d66`, submodule `94faaed0`; no `+dirty`). DLL SHA-256 `13bc2b126b1cf2a30eb32d7a44128bb4561788d73b787f7a9e218e23036f41d6` (1,282,560 bytes), PDB SHA-256 `c4a72f086c5ac12186870d7b35e3859dc348ed68eb8cb8bf8f7f15a5162a42e0` (35,770,368 bytes); CodeView/PDB GUID `{7F586C2E-9F62-4CE8-B33B-D19AFE9C6461}`, age 329. Configure and build exited 0 with only the existing install-rule and D9025 warnings. Five engine-free cases pass with the local `g++` 15.1 under `-Wall -Wextra -Werror`: pose content (36 assertions), clip reference (19), observation chain (11 rows), generation entry (22 rows) and frame trace (3 cases).

Package: `build/behavior-graph-v24-layer-census-20261027/test-package/` (`SKSE/Plugins/{CharacterPanel.dll|.pdb|.ini}`, `README-测试步骤.md`, `manifest.json` `7e47d6fc…` 6,268 bytes); ZIP `CharacterPanel-behavior-graph-v24-layer-census-20261027.zip` is 10,015,469 bytes, SHA-256 `68991ca70d5d8e81da0c47eccfee7880c5151634672a547a6f4685b4e8cbaa32`, five forward-slash entries re-extracted and re-hashed against the manifest. The package INI is the unchanged v21 fragment (`inject`, 1,278 bytes `efff49a5…`); the repository sample stays `off`.

Deployment (2026-10-27 23:38): the v23 trio was backed up to `dist/deploy-backup-20261027-v23/` (DLL `efa90c21…`, PDB `f7e3e2c6…`, `off` INI 1,077 bytes `02f37ace…` with the run note), the MO2 mod now holds this DLL/PDB (byte-identical to the package), and the deployed INI is `inject` (1,062 bytes `ac255097…`). **One run happened at 23:48**: identity matched, no crash, no `[E]`, 9 `clip-ref` and 7 `clip-clock` rows. Every identity-mapped binding reported `mapped=99/97` with a full `compared` count, so audit25's semantics held at runtime; `H2H_Idle` (binding-index 681) owned 90 of 99 tracks at step 120 with rotation error ≤6.1e-5 across all 99, which is the first layer attribution and satisfies the v24 gate. The clock row showed the stack switching mid-drive (`H2H_Equip` alone at steps 20/40, the idle stack from step 60) and the idle clock advancing 0.75x-1.55x per drive step rather than 1x, leaving continuity as the only open G2 item. The deployed INI returned to `off` (1,046 bytes `1424473c…`). See the [v24 run record](independent-behavior-graph.md#g2-v24-run).

<a id="v25-step-clock-2026-10-27"></a>
## V25 step-clock build (2026-10-27)

[Audit26](behavior-graph-construction-audit.md#audit26) established that the clip clock consumes dt and nothing else: `hkbClipGenerator::Update` (`0xACDE40`) hands the advance to `0xACFDE0`, which computes `new = old + dt x speed`, `speed = (effDuration/enforcedDuration or 1) x playbackSpeed`, clamped to `[cropStart, cropStart+effDuration]`. The v24 lag of 0.75x-1.55x therefore cannot come from the clock's own arithmetic. The [v25 contract](independent-behavior-graph.md#g2-v25-contract) makes both remaining explanations readable: `clip-ref` now reports `enforced` (`hkbClipGenerator::enforcedDuration`), `crop-sum` (`hkaDefaultAnimationControl::cropStartAmountLocalTime + cropEndAmountLocalTime`) and `eff-duration` (`duration - crop-sum`, the same expression as `0xACF340`), which rules out a constant multiplier; and a new `clip-steps` row holds one character per drive step with that step's `localTime` delta in 1/60 frames (`.`=0, `1`-`9`, `A`-`Z`=10-35, `N`=backwards, `-`=absent), so a periodic gap versus a fractional speed become distinguishable without 121 log rows. Entry, guard, eye point, the drive and every earlier row are unchanged; the only native call is still the const `SamplePartialTracks`.

Commands (proxy variables cleared for the reason recorded above):

```powershell
cmake --preset Release -DCHARACTER_PANEL_BUILD_LABEL=behavior-graph-v25-step-clock-20261027
cmake --build --preset Release
```

Build identity: `CharacterPanel-scenegraph-behavior-graph-v25-step-clock-20261027-862cebdf1b-cl94faaed0c6-20261027T155811Z` (clean HEAD `862cebdf1b`, submodule `94faaed0`; no `+dirty`). DLL SHA-256 `d8ca80bbf76693b23a3faafa46c33158c853972abb54dd157717874971405463` (1,290,752 bytes), PDB SHA-256 `a468bead87e32664fe283c0f9c85d2b4b9e8a7516371e789b3c56e2ae69ddbe6` (35,860,480 bytes); CodeView/PDB GUID `{7F586C2E-9F62-4CE8-B33B-D19AFE9C6461}`, age 331. Configure and build exited 0 with only the existing install-rule and D9025 warnings. Five engine-free cases pass with the local `g++` 15.1 under `-Wall -Wextra -Werror`: pose content (36 assertions), clip reference (19), observation chain (11 rows), generation entry (22 rows) and frame trace (3 cases).

Package: `build/behavior-graph-v25-step-clock-20261027/test-package/` (`SKSE/Plugins/{CharacterPanel.dll|.pdb|.ini}`, `README-测试步骤.md`, `manifest.json` `ba4bc726…` 5,571 bytes); ZIP `CharacterPanel-behavior-graph-v25-step-clock-20261027.zip` is 10,029,342 bytes, SHA-256 `15820120451f95a55c799fbd40f8c584bbe020aa880448e05e8a8abe4b2732d9`, five forward-slash entries re-extracted and re-hashed against the manifest. The package INI is the unchanged v21 fragment (`inject`, 1,278 bytes `efff49a5…`); the repository sample stays `off`.

Deployment (2026-10-27 23:59): the v24 trio was backed up to `dist/deploy-backup-20261027-v24/` (DLL `13bc2b12…`, PDB `c4a72f08…`, `off` INI 1,046 bytes `1424473c…` with the run note), the MO2 mod now holds this DLL/PDB (byte-identical to the package), and the deployed INI is `inject` (1,057 bytes `4af7e8ee…`). **One run happened at 2026-10-28 00:03**: identity matched, no crash, no `[E]`, five `clip-steps` rows. The bitstrings settle the clock question: the three idle clips advanced 72 frames over 72 drive steps (exactly 1.000x), and `H2H_Equip` advanced 50 frames, its own `duration`, before stopping. Delivery is batched though - twelve single-frame steps, then 15 frames every 15 steps with 14 zero steps between - so the v24 "0.75x lag" is a sampling artefact of a 20-step window against a 15-step period and that reading is withdrawn. The deployed INI returned to `off` (1,063 bytes `e910d4e8…`). See the [v25 run record](independent-behavior-graph.md#g2-v25-run).

<a id="v26-pose-step-2026-10-28"></a>
## V26 per-step pose build (2026-10-28)

The [v26 contract](independent-behavior-graph.md#g2-v26-contract) answers the one item v25 left open. v25 showed the idle clip clock advances exactly 1.000x but in batches - 15 frames every 15 drive steps, 14 zero steps between - so the remaining question is whether `poseLocal` follows that batching (a 4 Hz slideshow) or changes every step. This build reads `poseLocal` at the end of every drive step, compares the ten meaningful components against the previous step with `pose_sample_max_delta`, and writes one bucketed character per step (`.` = 0, `1` <= 1e-5, `2` <= 1e-4, `3` <= 1e-3, `4` <= 1e-2, `5` > 1e-2) into a single `pose-steps` row. No native call was added; entry, guard, drive and every earlier row are unchanged.

Commands (proxy variables cleared for the reason recorded above):

```
cmake --preset Release -DCHARACTER_PANEL_BUILD_LABEL=behavior-graph-v26-pose-step-20261028
cmake --build --preset Release
```

Build identity: `CharacterPanel-scenegraph-behavior-graph-v26-pose-step-20261028-aed15ad6c0-cl94faaed0c6-20261027T160910Z` (clean HEAD `aed15ad6c0`, submodule `94faaed0`; no `+dirty`). DLL SHA-256 `6c117d47ced99f149df1024a35c9cc4919ad6b7fdef9606f57f93d21f56066c7` (1,294,848 bytes), PDB SHA-256 `9203291b9fd0175602d7389ebc479b9e16e76abea02e18475d3253895e641a13` (35,909,632 bytes); CodeView/PDB GUID `{7F586C2E-9F62-4CE8-B33B-D19AFE9C6461}`, age 333. Configure and build exited 0 with only the existing install-rule and D9025 warnings. Five engine-free cases pass with the local `g++` 15.1 under `-Wall -Wextra -Werror`.

Package: `build/behavior-graph-v26-pose-step-20261028/test-package/` (`SKSE/Plugins/{CharacterPanel.dll|.pdb|.ini}`, `README-测试步骤.md`, `manifest.json` `a02e25c1…` 4,824 bytes); ZIP `CharacterPanel-behavior-graph-v26-pose-step-20261028.zip` is 10,029,787 bytes, SHA-256 `d3d7f1b71048000ca26df5845d585f8662e3f99907da2cf28ac2df2f572e5f17`, five forward-slash entries re-extracted and re-hashed against the manifest. The package INI is the unchanged v21 fragment (`inject`, 1,278 bytes `efff49a5…`); the repository sample stays `off`.

Deployment (2026-10-28 00:10): the v25 trio was backed up to `dist/deploy-backup-20261027-v25/` (DLL `d8ca80bb…`, PDB `a468bead…`, `off` INI 1,063 bytes `e910d4e8…` with the run note), the MO2 mod now holds this DLL/PDB (byte-identical to the package), and the deployed INI is `inject` (1,042 bytes `29692657…`). **One run happened at 00:12**: identity matched, no crash, no `[E]`, and the single `pose-steps` row read `changed=119` of 120 comparisons with every non-baseline character in the >1e-2 bucket. The pose therefore changes on every drive step, which retires the 4 Hz worry the v25 clock batching suggested (the sampled time is not the batched `localTime` field) and closes the last open G2 item. The deployed INI returned to `off` (1,041 bytes `04c2af00…`). See the [v26 run record](independent-behavior-graph.md#g2-v26-run).

<a id="v27-display-pose-2026-10-28"></a>
## V27 display handoff build (2026-10-28)

The [v27 contract](independent-behavior-graph.md#g2-v27-contract) puts the candidate's pose on screen for the first time. G2's evidence up to v26 came entirely from buffers, so this build converts the retained candidate's `poseLocal` into `(bone name, NiTransform)` pairs and lets `AnimationController::advance()` apply them through the existing name→node map, instead of the OAR clip sampler. Four files change: `independent_graph.{h,cpp}` remember the retained candidate's `hkbCharacter*` and expose the read-only `held_display_pose`, `settings.{h,cpp}` add the `DisplayPose` key, and `controller.cpp` branches at the top of `advance()`. **No native call is added** - the branch only reads `poseLocal` and the skeleton names - and the pose stays frozen at the last driven step, so this round establishes the wiring, not playback. `DisplayPose=false` or a missing candidate leaves the original path untouched.

The first compile failed with C2065 because the retained-character assignment referenced `candidate` outside its block scope; capturing the pointer at the point where the graph is deliberately leaked (`retained_character`) and storing it at function end fixed it.

Commands (proxy variables cleared for the reason recorded above):

```
cmake --preset Release -DCHARACTER_PANEL_BUILD_LABEL=behavior-graph-v27-display-pose-20261028
cmake --build --preset Release
```

Build identity: `CharacterPanel-scenegraph-behavior-graph-v27-display-pose-20261028-c894c956f8-cl94faaed0c6-20261027T162057Z` (clean HEAD `c894c956f8`, submodule `94faaed0`; no `+dirty`). DLL SHA-256 `b973aa5bc72d86e1ddff3db8d5c868731b49075f19c329a2539edd936ab450b5` (1,298,944 bytes), PDB SHA-256 `75a228f51d814055f304bd6fd43f9d1e9b747131ad8c8af4704fcffd45c8e4b0` (35,926,016 bytes); CodeView/PDB GUID `{7F586C2E-9F62-4CE8-B33B-D19AFE9C6461}`, age 335. Configure and build exited 0 with only the existing install-rule and D9025 warnings. Five engine-free cases pass with the local `g++` 15.1 under `-Wall -Wextra -Werror`.

Package: `build/behavior-graph-v27-display-pose-20261028/test-package/` (`SKSE/Plugins/{CharacterPanel.dll|.pdb|.ini}`, `README-测试步骤.md`, `manifest.json` `d3545033…` 5,020 bytes); ZIP `CharacterPanel-behavior-graph-v27-display-pose-20261028.zip` is 10,018,217 bytes, SHA-256 `6a195266c2b33af3185f03a586291fb43753d8e84f422fa15842b85e60394255`, five forward-slash entries re-extracted and re-hashed against the manifest. The package INI is the v21 fragment plus `DisplayPose=true` (1,295 bytes `618b1081…`); the repository sample keeps `DisplayPose=false`.

Deployment (2026-10-28 00:21): the v26 trio was backed up to `dist/deploy-backup-20261028-v26/`, the MO2 mod now holds this DLL/PDB (byte-identical to the package), and the deployed INI is `inject` + `DisplayPose=true` (1,063 bytes `2834670d…`). **No run has happened yet.**

<a id="v28-display-step-2026-10-28"></a>
## V28 per-frame display step build (2026-10-28)

v27 established the display handoff and the user confirmed it visually: `ANIMATION DISPLAY … bones=116`, the panel showing the candidate's `H2H_Idle` stance, frozen by design. The [v28 contract](independent-behavior-graph.md#g2-v28-contract) makes it move by advancing the retained graph once per rendered frame through the same `63589(candidate, &mode0)` entry the bounded drive used, then applying the resulting `poseLocal`. A failed step falls back to the v27 frozen pose, and `DisplayStep=false` returns to v27. Steps 1/60/120 log a pose hash, which separates "did the graph advance" from "did the screen follow".

This is the first round that calls the native generation entry from the render path, once per frame. The per-frame path has no world borrow and no ragdoll slot work; the entry's gates (setup / animationSkeleton / behaviorGraph non-null, `+0x12C` active) do not depend on them, but if the three hashes repeat the entry does not self-advance in isolation and the update chain becomes the next question.

Commands (proxy variables cleared for the reason recorded above):

```
cmake --preset Release -DCHARACTER_PANEL_BUILD_LABEL=behavior-graph-v28-display-step-20261028
cmake --build --preset Release
```

Build identity: `CharacterPanel-scenegraph-behavior-graph-v28-display-step-20261028-44dcdf4663-cl94faaed0c6-20261027T162729Z` (clean HEAD `44dcdf4663`, submodule `94faaed0`; no `+dirty`). DLL SHA-256 `e3f9ade882fb33eb7112026ca057df0176f06f91385597de99ec857bbf6b60e8` (1,302,016 bytes), PDB SHA-256 `3acff4afa5b5c1c8dd5748bd12ee4770e313b1b1b4c19056dcbf769777e207d9` (35,958,784 bytes); CodeView/PDB GUID `{7F586C2E-9F62-4CE8-B33B-D19AFE9C6461}`, age 337. Configure and build exited 0 with only the existing install-rule and D9025 warnings. Five engine-free cases pass with the local `g++` 15.1.

Package: `build/behavior-graph-v28-display-step-20261028/test-package/` (`SKSE/Plugins/{CharacterPanel.dll|.pdb|.ini}`, `README-测试步骤.md`, `manifest.json` `46ae35ee…` 4,357 bytes); ZIP `CharacterPanel-behavior-graph-v28-display-step-20261028.zip` is 10,015,286 bytes, SHA-256 `d2965ca80f24c5f4415b9f3e0f1a066f6840e86660cbaca4342ea25e28b1551a`, five entries re-extracted and re-hashed against the manifest. The package INI carries all three experiment keys (`inject`, `DisplayPose=true`, `DisplayStep=true`); the repository sample keeps every switch off.

Deployment (2026-10-28 00:29): the v27 trio was backed up to `dist/deploy-backup-20261028-v27/`, the MO2 mod now holds this DLL/PDB (byte-identical to the package), and the deployed INI carries the three keys (1,070 bytes `0866cec7…`). **No run has happened yet.**

<a id="v29-display-pipeline-2026-10-28"></a>
## V29 per-frame pipeline step build (2026-10-28)

The v28 run was a clean negative result: three identical pose hashes (`7d568c4d…`) across 120 per-frame calls spread over 3.35 s, with the figure visibly frozen. Comparing it against the batch showed why — the bounded drive does not just call `63589` per step, it runs the engine's own frame pipeline first:

```
63555 construct(shell, nullptr, false)      // scratch shell on the stack
63602 frame_pipeline(shell, candidate)      // hkbBehaviorGraph update -> clip advance
63556 shell_destroy(shell)
63589 (candidate, &mode0)                   // sampling entry only
```

The [v29 contract](independent-behavior-graph.md#g2-v29-contract) reproduces that core once per rendered frame inside `step_display_pose`. **This is the first round to put 63602 into the render path**; the batch ran the same three calls 120 times without a crash, and the shell uses stack storage destroyed immediately.

Commands (proxy variables cleared for the reason recorded above):

```
cmake --preset Release -DCHARACTER_PANEL_BUILD_LABEL=behavior-graph-v29-display-pipeline-20261028
cmake --build --preset Release
```

Build identity: `CharacterPanel-scenegraph-behavior-graph-v29-display-pipeline-20261028-04903ed263-cl94faaed0c6-20261027T165004Z` (clean HEAD `04903ed263`, submodule `94faaed0`; no `+dirty`). DLL SHA-256 `ce712e2058522d90e2cda9ca6fdafa1e6725868113eff5feda1ffb07282db79f` (1,302,016 bytes), PDB SHA-256 `6157150ea25d19c06c502c8c79ef015d078160294c8b4b04cc900269141b7156` (35,958,784 bytes); CodeView/PDB GUID `{7F586C2E-9F62-4CE8-B33B-D19AFE9C6461}`, age 339. Configure and build exited 0 with only the existing install-rule and D9025 warnings. Five engine-free cases pass.

Package: `build/behavior-graph-v29-display-pipeline-20261028/test-package/` (`SKSE/Plugins/{CharacterPanel.dll|.pdb|.ini}`, `README-测试步骤.md`, `manifest.json` `12970d32…` 4,110 bytes); ZIP `CharacterPanel-behavior-graph-v29-display-pipeline-20261028.zip` is 9,996,292 bytes, SHA-256 `42f92e948c47af21eaada48c64665a70a4d1b44f1abc0b559f0f1daed5de6405`, five entries re-extracted and re-hashed against the manifest. The package INI carries the three experiment keys; the repository sample keeps every switch off.

Deployment (2026-10-28 00:51): the v28 trio was backed up to `dist/deploy-backup-20261028-v28/`, the MO2 mod now holds this DLL/PDB (byte-identical to the package), and the deployed INI carries the three keys (1,095 bytes `468fb691…`). **No run has happened yet.**

<a id="v30-display-gates-2026-10-28"></a>
## V30 display gate diagnostics build (2026-10-28)

v28 called only `63589`; v29 added the batch's pipeline (`63555 → 63602 → 63556`). Both produced **the identical pose hash** `7d568c4dee1ac065`, which rules out "the advance is too small" and points at the calls changing nothing on the display path. This round adds no recipe: `step_display_pose` logs the gates that `63587`/`63589` read — `+0x12C` (active), `+0x12E` (children), `+0x245`, `+0x246`, `+0x240` (word), `+0x110`, `+0x208` — plus both output slots' `numData` (`+0x32`), on calls 1 and 60. Slot pointers pass `plausible_object_pointer` first and report `-1` when unreadable.

Commands (proxy variables cleared for the reason recorded above):

```
cmake --preset Release -DCHARACTER_PANEL_BUILD_LABEL=behavior-graph-v30-display-gates-20261028
cmake --build --preset Release
```

Build identity: `CharacterPanel-scenegraph-behavior-graph-v30-display-gates-20261028-0829b1d26d-cl94faaed0c6-20261027T165604Z` (clean HEAD `0829b1d26d`, submodule `94faaed0`). DLL SHA-256 `e5b0e57c657751cbec317bc3e53350ce1b509ec55ae8b44a318faf6effa73381` (1,304,064 bytes), PDB SHA-256 `e450627abc459dfee9559e793d722f8cfbb3dc5b0201f7cc9b5744c1e5fa4cc6` (35,975,168 bytes); CodeView age 341. Five engine-free cases pass.

Package: `build/behavior-graph-v30-display-gates-20261028/test-package/` (`manifest.json` `cdebfff0…` 3,919 bytes); ZIP 9,986,675 bytes, SHA-256 `1ce2b765…`, five entries re-hashed against the manifest. Deployment: the v29 trio was backed up to `dist/deploy-backup-20261028-v29/` and the deployed INI keeps the three experiment keys (1,068 bytes `50adad10…`). **No run has happened yet.**

<a id="v31-display-step-2026-10-28"></a>
## V31 shared display step and corrected observations (2026-10-28)

[V31](independent-behavior-graph.md#g2-v31-correction) makes the batch and eligible display use the same existing native step, including the explicit 63587 timestep update, eye record, behavior update flag, bone-count mirror and generation entry. The display borrows the source world only during that step and restores the candidate's previous field. Activity comes from the internal behavior graph; output numData comes through its element. Default experiment switches stay off. No new ABI, dependencies, package, deployment or game run.

Two engine-free cases in [behavior_graph_display_step_test.cpp](../tests/behavior_graph_display_step_test.cpp) passed with assertions and MSVC 14.44.35207. To reproduce, use an x64 VS developer PowerShell with that toolset initialized, from the repository root:

```powershell
New-Item -ItemType Directory -Force build/behavior-graph-v31-display-step-20261028 | Out-Null
cl /nologo /std:c++latest /EHsc /W4 /WX /utf-8 /I src tests/behavior_graph_display_step_test.cpp /Fobuild/behavior-graph-v31-display-step-20261028/display_step_test.obj /Febuild/behavior-graph-v31-display-step-20261028/display_step_test.exe
./build/behavior-graph-v31-display-step-20261028/display_step_test.exe
cmake --preset Release -DCHARACTER_PANEL_BUILD_LABEL=behavior-graph-v31-display-step-20261028
cmake --build --preset Release
```

The ready local vcpkg root is `D:/Microsoft Visual Studio/2022/Community/VC/vcpkg`; configure passes using the existing installed packages. Proxy variables are cleared only in the build child process. The first restricted build stopped at MSB4184 because it could not read the Windows SDK registration directory; the retry with SDK access reached compilation. A batch-local pose-reader visibility error was corrected by sharing that same reader; the affected rebuild then passes. Remaining warnings are the existing install-rules warning and D9025 `/Ob2` overridden by `/Ob3`.

Build identity: `CharacterPanel-scenegraph-behavior-graph-v31-display-step-20261028-f5ec13f2c2+dirty-cl94faaed0c6-20261028T033354Z`. DLL `build/Release/CharacterPanel.dll`: **1,306,624 bytes**, SHA-256 `a15e75baf36925bc463cb9d6619f8b8f06f71d5c6f38d57afa45952627419b3c`; PDB `build/Release/CharacterPanel.pdb`: **35,753,984 bytes**, SHA-256 `0280e4f8bf9b17de9f6afadfa265a93cf98130b63bd698731f5114737b3eb223`. These are a new local build from the uncommitted correction, not the old v20 DLL or a v30 package. Logs are under `build/behavior-graph-v31-display-step-20261028/`.

**Game playback remains unverified.** Compare meaningful finite bone changes and display motion within v31 over 120 actual display calls, with source before/after controls. V31 display hashes now use the batch hash function; their values must not be compared with v28–v30's different FNV initial value to infer motion. Loaded teardown/global convergence and G3–G6 remain open.

<a id="v32-display-session-2026-10-28"></a>
## V32 display ownership and motion/coverage evidence (2026-10-28)

[V32](independent-behavior-graph.md#g2-v32-display-session) corrects source/controller validity, request/view measurement windows, sign-invariant candidate motion and actual node-write counts. The v31 native-step helper and frame readers are unchanged; all defaults stay off. Exactly two new coherent engine-free cases passed with assertions and MSVC14.44 `/W4 /WX`; the old suite was not rerun. They are `source_sessions_do_not_reuse_stale_views_or_partial_measurements` and `candidate_motion_and_applied_coverage_do_not_overstate_visible_motion` in [behavior_graph_display_session_test.cpp](../tests/behavior_graph_display_session_test.cpp). These are prior verified results; documentation completion reruns neither tests nor build. To reproduce from the repository root in an x64 VS developer PowerShell with MSVC 14.44.35207 initialized:

```powershell
New-Item -ItemType Directory -Force build/behavior-graph-v32-display-session-20261028 | Out-Null
cl /nologo /std:c++latest /EHsc /W4 /WX /utf-8 /I src tests/behavior_graph_display_session_test.cpp /Fobuild/behavior-graph-v32-display-session-20261028/display_session_test.obj /Febuild/behavior-graph-v32-display-session-20261028/display_session_test.exe
./build/behavior-graph-v32-display-session-20261028/display_session_test.exe
cmake --preset Release -DCHARACTER_PANEL_BUILD_LABEL=behavior-graph-v32-display-session-20261028
cmake --build --preset Release
```

The recorded compile/test/configure/build commands exited 0. The directory-creation line above makes the reproduction snippet self-contained; it is not a newly run check. The build uses the same installed vcpkg packages and ordinary SDK read access as v31; proxy variables are cleared only in its child process. Existing install-rule and D9025 `/Ob2`→`/Ob3` warnings remain. Logs are under `build/behavior-graph-v32-display-session-20261028/`.

Identity: `CharacterPanel-scenegraph-behavior-graph-v32-display-session-20261028-f5ec13f2c2+dirty-cl94faaed0c6-20261028T073724Z`. DLL **1,316,864 bytes**, SHA-256 `7605f2277838109cb209cc7d50c87a75bbca8d89acd3515fdb0100589451d338`; PDB **35,958,784 bytes**, SHA-256 `791aef7e2cac855e851c64d038c154c876a872827a0524536b07675736d3a83e`. Outputs are `build/Release/CharacterPanel.dll` and `.pdb` from that pre-commit working tree. This is a historical configure-time identity at `f5ec13f2c2+dirty`; a later local commit does not relabel either artifact. No binary was rebuilt for the documentation completion. A binary for a new HEAD requires fresh configure then build and a new recorded identity/hash pair; it may still carry `+dirty` while unrelated local files remain. CMake recursively gathers first-party sources and Git status includes untracked files, so the preserved local `src/base/util.cpp`/`util.h` must not be silently treated as part of the tracked commit or as proof of byte-for-byte reproduction from that commit alone.

**No game execution, package or deployment.** Candidate semantic continuity must be rechecked with new runtime data; actual node coverage/source controls and visual display remain separate pending verification under the [v32 checklist](independent-behavior-graph.md#v32-manual-checklist). Historical raw119/120 does not establish semantic motion, and bones=116 did not count applied nodes.

<a id="v33-rebuild-2026-10-28"></a>
## V33 rebuild of the committed v32 code (2026-10-28)

The v32 identity above came from an uncommitted tree and no v31/v32 artifact survived in the checkout: `build/Release/CharacterPanel.dll` was the deployed v30 binary (`e5b0e57c…`, 1,304,064 bytes) and no `build/behavior-graph-v31-*`/`v32-*` directory or package existed. A game run therefore needs a fresh artifact from committed HEAD `d323b35` under a new label; the source is unchanged from the v32 tree, so the contract and the two engine-free cases are the same code.

- Configure: `cmake --preset Release -DCHARACTER_PANEL_BUILD_LABEL=behavior-graph-v33-display-session-20261028` with the lowercase proxy variables removed from the child environment; clean tree, so the identity carries no `+dirty`.
- Build: `cmake --build --preset Release`, exit 0 (incremental against the v30 objects). Only the pre-existing install-rule warning and D9025 `/Ob2`→`/Ob3` remain.
- Engine-free cases rerun on this tree, both exit 0: [behavior_graph_display_session_test.cpp](../tests/behavior_graph_display_session_test.cpp) (`2 display-session regression cases passed`) and [behavior_graph_display_step_test.cpp](../tests/behavior_graph_display_step_test.cpp) (`2 display-step regression cases passed`), object/exe under `build/behavior-graph-v33-display-session-20261028/`. The older policy cases were still not rerun.

Identity: `CharacterPanel-scenegraph-behavior-graph-v33-display-session-20261028-d323b357a8-cl94faaed0c6-20261028T142029Z`, embedded and verified in the DLL. DLL **1,319,424 bytes**, SHA-256 `061d5b8af09c14c1c9fffabdfed82cd4ac5889feabec80bb62bf357b1adadd75`; PDB **36,261,888 bytes**, SHA-256 `5924464d94f0c8843fd1a4dc50ff3651dac9321e12c7b9c4460ffc4fb7eefdbe`. The binary is larger than the recorded v32 one because the label and configure stamp differ, not because of a source change.

Toolchain note for reproduction: the `vcvars64.bat`/`Enter-VsDevShell` route fails in this environment because the sandbox blocks `reg.exe`, so the SDK include paths are never resolved and `cl` stops at `crtdbg.h`. Compiling with explicit `INCLUDE`/`LIB`/`PATH` (`<VS>\VC\Tools\MSVC\14.44.35207` plus `Windows Kits\10` `10.0.26100.0`) works and was the route used here. CMake/MSBuild is unaffected because the cache already carries the resolved SDK.

**Deployed 2026-10-28 22:23 and run the same evening.** The v30 trio in `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\` was first copied to `dist/deploy-backup-20261028-v30/` (DLL `e5b0e57c…`, PDB `e450627a…`, INI `2e150d41…`, 1,094 bytes), then replaced by this DLL/PDB; the deployed copies hash to `061d5b8a…` and `5924464d…`. MO2 profile `AE` lists `+CharacterPanel`, so nothing else needs enabling. The deployed `CharacterPanel.ini` briefly set `IndependentGraph=inject`, `DisplayPose=true`, `DisplayStep=true` (1,338 bytes, SHA-256 `0861a221…`); after the 22:28 session all three were restored to off with a run comment, so the deployed file is now 1,384 bytes, SHA-256 `8beb6286ead09d7cc8fcba955fb8a14efc3a896588375471b312221e448da408`. Settings load at DataLoaded, so a full restart is required either way. The repository sample `config/CharacterPanel.ini` stays default-off. Session result: the [v33 run record](independent-behavior-graph.md#g2-v33-run); the later [v34 sessions](independent-behavior-graph.md#g2-v34-boundaries) record four boundary outcomes with the stated limits.

<a id="v35-registry-probe-2026-10-28"></a>
## V35 registry-count probe (2026-10-28)

> **The premise of this build was wrong.** The 23:16 run printed `registry=300474432->300474432`; re-reading the instructions showed the field is a local parity/sequence value that is never written back, and `63529` is a lock-free append rather than a registry insert. The probe is reverted in [v36](#v36-revert-2026-10-28) and the claim is corrected in the [G1 leftover section](independent-behavior-graph.md#g1-teardown-audit). This record is kept as the negative result.

[G1 leftover two](independent-behavior-graph.md#g1-teardown-audit) turned the hardcoded `registry-probe=unavailable` into a measurement: `registered_graph_count()` read `REL::Offset{0x315C950}` + `0x1F8` + `0x30`, and `phase=retained` printed `registry=<before>-><after>`. Plain data read with a null guard and an exact-AE1170 gate; no native call, no engine write, no Deactivate.

Configure and build used the same commands as v33; the tree was committed first (`1ca2ad6979`) so the identity is clean. The two engine-free suites were **not** rerun for this build — the delta touches only native glue and logging, and neither suite links that code.

Identity: `CharacterPanel-scenegraph-behavior-graph-v35-registry-probe-20261028-1ca2ad6979-cl94faaed0c6-20261028T150959Z`. DLL **1,319,936 bytes**, SHA-256 `9d5cce9ffbfc925155f0f09305c8a14363dbb4c6802e9dd74ccef7db34cc1753`; PDB **36,261,888 bytes**, SHA-256 `c9fdd1d92431098dcfda6ff680af2dc647871321b7e1c3c159d143fc25b6553f`.

**Deployed 2026-10-28 23:11.** The v33 trio was backed up to `dist/deploy-backup-20261028-v33/` (DLL `061d5b8a…`, PDB `5924464d…`, INI `a7140437…`), then replaced. The deployed INI sets `IndependentGraph=inject` only — `DisplayPose`/`DisplayStep` stay false, because the probe runs at load and needs no display stepping. Pre-registered reading, stop conditions and the non-goals are in the [contract](independent-behavior-graph.md#g1-teardown-audit).

<a id="v36-revert-2026-10-28"></a>
## V36 registry-probe revert (2026-10-28)

`1ca2ad6` is reverted by `cee948d` because the field it read is not a registry count. Configure and build used the same commands; the tree was clean at the revert commit, so the identity carries no `+dirty`. The two engine-free suites were not rerun — the revert restores the exact v33 source for that file.

Identity: `CharacterPanel-scenegraph-behavior-graph-v36-registry-probe-revert-20261028-cee948dcf8-cl94faaed0c6-20261028T151759Z`. DLL **1,319,424 bytes**, SHA-256 `ac01ac953dc923b62cfecec17d83bfa8371d0a622ba13c72370f11a233c278c3`; PDB **36,261,888 bytes**, SHA-256 `584e3e4f527e8e929d74dfa40c24116721b2f968c629a966fad848c04b8b0b68`.

**Deployed 2026-10-28 23:18**: the v35 trio went to `dist/deploy-backup-20261028-v35/` (DLL `9d5cce9f…`), the deployed DLL now hashes to `ac01ac95…`, and all three experiment keys are back to `off/false/false`, so the shipped behaviour is the plain OAR product path.

<a id="v37-source-timeline-2026-10-28"></a>
## V37 source timeline recorder (2026-10-28)

First half of G3: a read-only recorder for the player's own graph, sampled once per post-UI frame and called from the existing render hook, so no new hook and no new native call are introduced. It logs only word-variable transitions, the active-node count and a 64-entry node hash, with a 200-line process budget. Contract, read points and pre-registered readings are in the [G3-A contract](independent-behavior-graph.md#g3-a-contract).

New files: [source_timeline.h](../src/animation/behavior_graph/source_timeline.h) / [source_timeline.cpp](../src/animation/behavior_graph/source_timeline.cpp); `settings.cpp/h` gains `SourceTimeline`; [renderer.cpp](../src/render/renderer.cpp) calls it from `post_ui_draw`. The tree was committed first (`81578f5c63`) so the identity is clean.

**No engine-free test was added for this build.** The recorder is native glue over engine offsets; the only pure logic is the diff loop and the FNV mix, and neither has a policy header to host a test. This is a deliberate gap for this probe, not a rerun of the existing suites (which were not executed either).

Identity: `CharacterPanel-scenegraph-behavior-graph-v37-source-timeline-20261028-81578f5c63-cl94faaed0c6-20261028T153941Z`. DLL **1,327,616 bytes**, SHA-256 `7f3bd92eb5e341db50314168fe2a18eaaaad86a84813d5ea13d0130c76fd8103`; PDB **36,327,424 bytes**, SHA-256 `c85cb1926e23c18fa9326ba5d241522c179c9401855bb84ac70ae3ce4ec69208`.

**Deployed 2026-10-28 23:41**: the v36 trio went to `dist/deploy-backup-20261028-v36/` (DLL `ac01ac95…`); the deployed INI sets `SourceTimeline=true` with `IndependentGraph=off` and both display keys false, so the product animation path is untouched.

<a id="v38-settle-2026-10-29"></a>
## V38 settled source timeline (2026-10-29)

The v37 run booked 200 lines in 8.5 seconds — 76 from `TimeDelta`, 91 from a per-frame node hash — and never reached the equip sequence, so the [contract](independent-behavior-graph.md#g3-a-contract) marks that run uncollected. V38 changes the recording rule instead of the threshold: a variable is written only after its new value has survived three consecutive samples, and the node line keeps the count alone (the hash was per-frame noise). Budget, read points and the read-only guarantees are unchanged.

Identity: `CharacterPanel-scenegraph-behavior-graph-v38-source-timeline-settle-20261029-ad15b58ae8-cl94faaed0c6-20261028T160832Z`. DLL **1,327,616 bytes**, SHA-256 `17567d57e8deddadcc14949f9449cb1ba51c87a55992bf73d35aa9b49a94f18f`; PDB **36,343,808 bytes**, SHA-256 `32261d2439d4d472e1cc3e0801b326c63e019a42b8d905adfbfaa6e9931b9b9a`.

**Deployed 2026-10-29 00:09**: the v37 trio went to `dist/deploy-backup-20261029-v37/`, and the INI keeps `SourceTimeline=true` with everything else off. No engine-free test was added for this build either; the change is the same native glue with a different write rule.

<a id="v39-window-2026-10-29"></a>
## V39 bounded timeline window (2026-10-29)

The v38 run lasted 18 seconds and still lost the equip sequence: the source graph runs a roughly 1.2-second cycle (`bVoiceReady`, `BlendJump`, `bIsInMT`, `iLeftHandEquipped`, node count 22↔27↔44↔54) that consumes any always-on budget. V39 stops sampling outside an explicit window: **F9 opens 20 seconds**, the window closes by itself, and the line budget rises to 800. Settle rule, read points and read-only guarantees are unchanged; the hotkey is handled before the scene-graph gate so it works during normal play.

Identity: `CharacterPanel-scenegraph-behavior-graph-v39-timeline-window-20261029-5f2cf53de6-cl94faaed0c6-20261028T161546Z`. DLL **1,328,640 bytes**, SHA-256 `5f2f660a1caba06d954244a0ee019c715684f554281a793db2d3a31494a46191`; PDB **36,352,000 bytes**, SHA-256 `a6a418df4a5155996260e61857d2f4af6aff31420672358785c479b31eeddaf0`.

**Deployed 2026-10-29 00:17**: the v38 trio went to `dist/deploy-backup-20261029-v38/`, the deployed DLL hashes to `5f2f660a…`, and the INI keeps `SourceTimeline=true` with everything else off. The build string now advertises `F9-timeline`.

<a id="v40-f7-2026-10-29"></a>
## V40 timeline hotkey moved to F7 (2026-10-29)

F9 is the game's own quick-load key, so v39's binding was unusable. V40 binds the timeline window to **F7 when no panel is open**; inside the panel F7 keeps its recapture meaning. The window length, settle rule, read points and budget are unchanged; only the input branch moved.

Identity: `CharacterPanel-scenegraph-behavior-graph-v40-timeline-f7-20261029-58d25af47b-cl94faaed0c6-20261028T161853Z`. DLL **1,328,640 bytes**, SHA-256 `5f791d2cedcad54ff97b6c18cc294966617972c0e4ed03ff57c353924dc1bc49`; PDB **36,352,000 bytes**, SHA-256 `139a491a6b3210495d98fe42ab17ad457a653edadafaa8815e54e35cf779055c`.

**Deployed 2026-10-29 00:20**: the v39 trio went to `dist/deploy-backup-20261029-v39/`, the deployed DLL hashes to `5f791d2c…`, and the build string now reads `F7-recapture-or-timeline`.

<a id="v41-panel-gate-2026-10-29"></a>
## V41 F7 gated on the panel state (2026-10-29)

The v40 run loaded `SourceTimeline=true` but F7 only produced `SceneGraph ... command=1 ignored outside paused inventory`: a scene graph object exists outside the pause menu, so "no graph" was the wrong discriminator and the key fell into the recapture branch. V41 gates on `PanelMonitor::is_menu_open()` instead — F7 opens the timeline window outside the panel and keeps recapture inside it. Nothing else changed.

Identity: `CharacterPanel-scenegraph-behavior-graph-v41-timeline-panel-gate-20261029-9bc1a4b03a-cl94faaed0c6-20261028T162406Z`. DLL **1,328,640 bytes**, SHA-256 `3b8f8efe72d388f4870088804e6bf52f27a0d344f9e07c2e8dd933ea3ed099c6`; PDB **36,352,000 bytes**, SHA-256 `e99d04cd65c4c4e6a29a1c91bcac5ec43fd947ffec3aa706a1eb85dadb3812ff`.

**Deployed 2026-10-29 00:25**: the v40 trio went to `dist/deploy-backup-20261029-v40/`, the deployed DLL hashes to `3b8f8efe…`.

<a id="v42-residual-corrections-2026-10-29"></a>
## V42 residual corrections and bounded diagnostics (2026-10-29, local only)

[Canonical scope and pending observations](independent-behavior-graph.md#v42-residual-corrections): missing names increase from the historical cap4 to cap8; a rejected request/view emits at most one typed reason using the existing 18-row budget. Original source-identity acceptance, valid-source frozen fallback, native guards, timeline/input and motion algorithms are unchanged. The two existing session scenarios alone are extended: lifecycle/identity/window rejection classification and seven missing names with application/root arithmetic. No old v31 suite or full suite rerun.

From an x64 developer shell initialized by `D:/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat -vcvars_ver=14.44`, with assertions enabled:

```powershell
New-Item -ItemType Directory -Force build/behavior-graph-v42-residual-corrections-20261029 | Out-Null
cl /nologo /std:c++latest /EHsc /W4 /WX /utf-8 /I src tests/behavior_graph_display_session_test.cpp /Fobuild/behavior-graph-v42-residual-corrections-20261029/display_session_test.obj /Febuild/behavior-graph-v42-residual-corrections-20261029/display_session_test.exe
./build/behavior-graph-v42-residual-corrections-20261029/display_session_test.exe
cmake --preset Release -DCHARACTER_PANEL_BUILD_LABEL=behavior-graph-v42-residual-corrections-20261029
cmake --build --preset Release
```

Direct MSVC compile/run and configure exited **0**; output: `2 display-session regression cases passed (engine-free)`. The first sandbox build exited **1**, MSB4184 while reading `C:/Users/lhq49/AppData/Local/Microsoft SDKs`; the same build with ordinary installed-SDK read permission exited **0**. Proxy variables were removed only in the child build environment. Existing install-rule/cache-toolchain and D9025 `/Ob2`→`/Ob3` warnings remain. Logs, protected-file snapshot, test artifacts and copied DLL/PDB are under `build/behavior-graph-v42-residual-corrections-20261029/`.

Identity: `CharacterPanel-scenegraph-behavior-graph-v42-residual-corrections-20261029-38fb82c54f+dirty-cl94faaed0c6-20261029T055837Z` (also verified embedded in the DLL). DLL **1,327,104 bytes**, SHA-256 `0da91fd8df6cf9740ba3c60cba0cfe018a79eb438c83defb015ce7d5fae2fd84`; PDB **36,065,280 bytes**, SHA-256 `9973c4c4b6e4a31ddaff229ac584378e2417df5045023de145b39190fabfbb92`. The dirty identity includes this uncommitted correction and preserved unrelated `src/base/util.cpp`/`util.h`; the existing source glob still includes them, so this is not a byte-for-byte clean-commit reproduction claim.

**Local build only; no commit, push, package, deployment or game launch.** The latest recorded install remains v41 with its distinct identity/hash and SourceTimeline=true, other three experiment keys off; repository defaults remain all off. Real seven missing names/application semantics and precise same-controller source/generation or frozen-only rejection still need new authorized game data. Offline instrumentation does not establish those runtime results. G3-A remains successful settled phase observation; G3-B external role/type/event-timing prerequisites and full observable G6 limits are in the canonical handoff.

<a id="v43-source-sessions-2026-10-29"></a>
## V43 independent source-timeline sessions (2026-10-29, local only)

[Canonical invariants and limitations](independent-behavior-graph.md#v43-source-sessions) cover fresh per-arm 20-second/800-line budgets, sampler-owned array/settle reset, address-value source binding and queued world cancellation. Production uses [SourceTimelineWindow/SourceTimelineRequests](../src/animation/behavior_graph/source_timeline_policy.h); [source_timeline.cpp](../src/animation/behavior_graph/source_timeline.cpp) consumes intents and deadlines before optional graph reads. `main.cpp::message_handler` publishes PreLoadGame/NewGame invalidation before the existing DAC/preview reset actions. F7 routing, settle3/raw reporting and defaults are unchanged.

Exactly [two new engine-free scenarios](../tests/source_timeline_session_test.cpp) ran: exhaustion then two independent re-arms/new baselines; and a source/world lifecycle with same-count replacement, storage change, source loss, deadline before first source and pending/active cancellation. Production array/settle initialization is also reviewed at `Recorder{}`/`baseline`. Existing v42 cases and full suites were not rerun.

From an x64 developer shell initialized by `D:/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat -vcvars_ver=14.44`, with assertions enabled:

```powershell
New-Item -ItemType Directory -Force build/behavior-graph-v43-source-sessions-20261029 | Out-Null
cl /nologo /std:c++latest /EHsc /W4 /WX /utf-8 /I src tests/source_timeline_session_test.cpp /Fobuild/behavior-graph-v43-source-sessions-20261029/source_timeline_session_test.obj /Febuild/behavior-graph-v43-source-sessions-20261029/source_timeline_session_test.exe
./build/behavior-graph-v43-source-sessions-20261029/source_timeline_session_test.exe
cmake --preset Release -DCHARACTER_PANEL_BUILD_LABEL=behavior-graph-v43-source-sessions-20261029
cmake --build --preset Release
```

MSVC compile/run and configure exited **0**; output: `2 source-timeline session regression cases passed (engine-free)`. An initial wrapper invocation with a forward-slash CMD path failed before compilation (`build` not recognized); using the resolved Windows script path succeeded. The first sandbox build exited **1**, MSB4184 while reading `C:/Users/lhq49/AppData/Local/Microsoft SDKs`; the same build with installed-SDK read permission exited **0**. Proxy variables were removed only in the child configure/build environment. Existing install-rule and D9025 `/Ob2`→`/Ob3` warnings remain. Logs, source snapshots, preservation/navigation evidence, tests and copied DLL/PDB are under `build/behavior-graph-v43-source-sessions-20261029/`.

Identity: `CharacterPanel-scenegraph-behavior-graph-v43-source-sessions-20261029-38fb82c54f+dirty-cl94faaed0c6-20261029T090649Z` (also verified embedded in the DLL). DLL **1,328,128 bytes**, SHA-256 `99313a18b14cf5275b0ed329df385ef4183d76e0ac51e07d69e33a291f670400`; PDB **36,089,856 bytes**, SHA-256 `9345bfecb7ec8874e500c5bc6f81e48b772e2ab6095270688a9f6716bbafc2e9`. The configure-time dirty identity includes the preserved untracked `src/base/util.cpp`/`util.h`, still included by the existing source glob, alongside v42 and this change. This is not a clean-commit reproduction claim.

**Local only; no commit, push, package, deployment or game launch.** Latest recorded install remains [v41](#v41-panel-gate-2026-10-29); v42 artifacts/history are preserved. Source identity snapshots do not establish writer roles, exact setter/event timing, atomic native arrays or unobserved same-address lifetime changes. No new native calls/hooks, configuration/dependencies or teardown authority. Historical G2 and G3-A evidence is unchanged.

<a id="v44-display-clock-2026-10-29"></a>
## V44 elapsed candidate display clock (2026-10-29, local only)

[Current timing/reset contract](independent-behavior-graph.md#v44-display-clock): eligible elapsed steady-clock time schedules fixed native 1/60 steps, first call establishes an origin, each delta caps at 100 ms/six steps and excess stall time is discarded while fractional carry remains. Zero-due calls retain stepped-mode candidate output without native/motion increments. Request/retarget/rejection and ineligible/frozen fallback reset pacing. Original native recipe, batch steps, v42 diagnostics and v43 source sessions are preserved.

The production [DisplayStepClock](../src/animation/behavior_graph/display_step_clock.h) has exactly [two engine-free scenarios](../tests/behavior_graph_display_clock_test.cpp): 30 FPS two-second elapsed progression produces 120 steps; 120 FPS produces the same 120 with zero-due calls and checks reset gaps, six-step stall cap, fractional carry and discarded backlog. Source inspection checks per-native guards/motion/checkpoints and zero-due mode; no prior behavioral suites rerun.

From an x64 developer shell initialized by `D:/Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat -vcvars_ver=14.44`, with assertions enabled:

```powershell
New-Item -ItemType Directory -Force build/behavior-graph-v44-display-clock-20261029 | Out-Null
cl /nologo /std:c++latest /EHsc /W4 /WX /utf-8 /I src tests/behavior_graph_display_clock_test.cpp /Fobuild/behavior-graph-v44-display-clock-20261029/display_clock_test.obj /Febuild/behavior-graph-v44-display-clock-20261029/display_clock_test.exe
./build/behavior-graph-v44-display-clock-20261029/display_clock_test.exe
cmake --preset Release -DCHARACTER_PANEL_BUILD_LABEL=behavior-graph-v44-display-clock-20261029
cmake --build --preset Release
```

Compile/run/configure exited **0**; `2 display-clock regression cases passed (engine-free)`. The first sandbox build exited **1** on the known MSB4184 installed-SDK metadata denial at `C:/Users/lhq49/AppData/Local/Microsoft SDKs`; the authorized SDK-read retry exited **0**. Only child configure/build proxy variables were cleared. Existing install-rule and `/Ob2`→`/Ob3` warnings remain. Logs, source snapshots, tests, verification evidence and copied DLL/PDB are under `build/behavior-graph-v44-display-clock-20261029/`.

Identity: `CharacterPanel-scenegraph-behavior-graph-v44-display-clock-20261029-38fb82c54f+dirty-cl94faaed0c6-20261029T091938Z` (verified embedded in the DLL). DLL **1,332,224 bytes**, SHA-256 `129dee119d1bc07129aba788d0e98b537b68ef5f5dd5c0ea5319ac3b995ac66b`; PDB **36,122,624 bytes**, SHA-256 `62bb1c0db1da25e46e02b2aeb0de7cca9dc6075e5dcd688fc8e21641cda1ee10`. Configure-time dirty identity includes preserved untracked `src/base/util.cpp`/`util.h` under the unchanged source glob and prior v42/v43 work; no clean-commit reproduction claim.

**Local only; no commit, push, package, deployment or game launch.** Latest recorded install stays [v41](#v41-panel-gate-2026-10-29), SourceTimeline=true and other experiment keys off; repository defaults remain all off. Historical v33/v34 per-call results and v41 G3-A evidence are unchanged. Two deterministic clock scenarios establish scheduling arithmetic, while game speed/visual validation remains pending.

<a id="v45-source-list-2026-10-29"></a>
## V45 explicit plugin translation units (2026-10-29, local only)

The recursive `.cpp` glob previously compiled unrelated untracked `src/base/util.cpp`. `PLUGIN_SOURCES` now lists exactly the 30 reviewed intended plugin units, sorted by path. Both util.cpp and util.h remain byte-for-byte preserved and untracked; util.cpp is absent from generated ClCompile inputs. The header glob, target, generated shader/version/resource files, dependencies, toolchain, native APIs and product/test sources are unchanged. All v42–v44 features remain present. Historical v42–v44 artifacts actually used the old glob and included util.cpp; their identities/hashes and evidence remain unchanged.

Future intended `.cpp` additions/removals must update `PLUGIN_SOURCES` in [CMakeLists.txt](../CMakeLists.txt), followed by configure and compile-inventory review. CMake resolves the list directly; Git is used only for this local validation comparison, with no new build-time Git requirement. Existing build identity metadata/fallback behavior is unchanged. Explicit first-party units do not by themselves establish external-dependency or byte-for-byte reproducibility.

```powershell
cmake --preset Release -DCHARACTER_PANEL_BUILD_LABEL=behavior-graph-v45-source-list-20261029
python build/behavior-graph-v45-source-list-20261029/verify_inventory.py
cmake --build --preset Release
```

Configure and the inventory check exited **0**. The XML inventory in `build/CharacterPanel.vcxproj`, excluding only generated `cmake_pch.cxx`, matches all 30 tracked intended units from the reviewed `git ls-files -- src/*.cpp` inventory; every path exists, with no util.cpp or test units. The script and exact inventory are local evidence under `build/behavior-graph-v45-source-list-20261029/`, not a new build entry point. No behavioral tests were needed or rerun for this mechanical selection change.

The first sandbox build exited **1** on the known MSB4184 denial reading `C:/Users/lhq49/AppData/Local/Microsoft SDKs`; the authorized installed-SDK-read retry exited **0**. Only child configure/build proxy variables were cleared. Existing install-rule and `/Ob2`→`/Ob3` warnings remain. Logs, inventory, snapshots, preservation/link evidence and copied DLL/PDB are in the same v45 artifact directory.

Identity: `CharacterPanel-scenegraph-behavior-graph-v45-source-list-20261029-38fb82c54f+dirty-cl94faaed0c6-20261029T092710Z` (also verified embedded in the DLL). DLL **1,330,176 bytes**, SHA-256 `7cee6442b310cb9caaf943ea2294891bafaf92f5acb9e2ffd3dc72b35907bcc6`; PDB **36,122,624 bytes**, SHA-256 `bb083876bf9b30851093783cdd6fde91a52d065b6105a1f158a47f06551d4ff5`. The configure-time identity remains dirty because prior v42–v44 work and this build change are uncommitted and util files remain untracked; their presence in status no longer makes util.cpp a compile input. No clean-commit or HEAD-only byte-reproduction claim is made.

**Local only; no commit, push, package, deployment or game launch.** Latest recorded install remains [v41](#v41-panel-gate-2026-10-29), SourceTimeline=true with the other experiment keys off; repository defaults remain all off. Product/test hashes, earlier artifacts and bounded historical game conclusions are preserved.

<a id="v46-retained-copy-ledger-2026-10-29"></a>
## V46 complete retained-copy ledger (2026-10-29, local only)

[Canonical lifetime/accounting contract](scene-graph.md#retired-copy-accounting-v46): `SceneGraph::retire_graph` keeps all NiPointer values in one intentionally surviving heap vector, removes per-copy heap wrappers/erased raw pointers, and logs the true process-total handle count. Twelve is a diagnostic threshold; only total 13 warns. Native allocation remains intentionally unbounded and unfreed, separately from the experimental one-per-process candidate. Both caller order and all other native behavior are unchanged.

Read-only inspection confirms pinned CommonLib `NiPointer(NiPointer&&) noexcept` steals `_ptr` and nulls its source; `TryDetach` decrements only a non-null pointer. Thus vector reallocation can move handles without releasing their native objects. Existing replacement retargets before retirement; destruction closes the session before retirement. This is not native destructor safety evidence. No synthetic container tests or old behavioral suites were needed or run.

```powershell
cmake --preset Release -DCHARACTER_PANEL_BUILD_LABEL=behavior-graph-v46-retained-copy-ledger-20261029
cmake --build --preset Release
```

Configure exited **0**. The first sandbox build exited **1** on the known MSB4184 installed-SDK metadata denial at `C:/Users/lhq49/AppData/Local/Microsoft SDKs`; the authorized SDK-read retry exited **0**. Proxy variables were cleared only in child configure/build environments. Existing install-rule and `/Ob2`→`/Ob3` warnings remain. Logs, source snapshots, SDK/caller inspection and protected-file/navigation evidence plus copied DLL/PDB are under `build/behavior-graph-v46-retained-copy-ledger-20261029/`.

Identity: `CharacterPanel-scenegraph-behavior-graph-v46-retained-copy-ledger-20261029-38fb82c54f+dirty-cl94faaed0c6-20261029T093244Z` (verified embedded in the DLL). DLL **1,331,200 bytes**, SHA-256 `1b86665563be9e4534855ec8d629cf84a673326f5ad8c54ce74cfffeb95d4a57`; PDB **36,122,624 bytes**, SHA-256 `f1b106052ec87d8ff0ef7e4f8e4f8e4fbde1fb0e0fb828b080459311bde1fab3`. Configure-time dirty metadata reflects uncommitted v42–v46 work and preserved untracked util files; the unchanged v45 source list excludes util.cpp from compile inputs. No clean-commit reproduction claim is made.

**Local only; no commit, push, package, deployment or game launch.** The latest recorded install remains [v41](#v41-panel-gate-2026-10-29). Eight prior v42–v45 DLL/PDB files and historical records/logs are preserved. Native memory usage, safe destruction and repeated-session convergence remain unverified; this change fixes bookkeeping/diagnostic accuracy, not native memory reclamation.

## Scope and non-goals

This guide covers the existing C++23/x64 toolchain, dependencies, generated files and local build evidence. It does not add test, package, install or compatibility routes, upgrade the toolchain, deploy files or establish game-time correctness. Plugin metadata declares Skyrim 1.6.1170 compatibility and minimum SKSE 2.2.6 through `CompatibleVersions` and `MinimumRequiredXSEVersion`; see [main.cpp](../src/main.cpp), `SKSEPlugin_Version`.

## Dependencies and checkout

Use CMake **3.22 or newer**, Visual Studio **2022** with the C++ desktop tools, MSVC **14.44.35207**, and an installed Windows SDK. Make `cmake` and the existing vcpkg installation available in your build environment.

Initialize the repository's recorded submodule before configuring:

```powershell
git submodule update --init --recursive
```

[.gitmodules](../.gitmodules) declares `extern/CommonLibSSE` from CommonLibSSE-NG. This checkout pins it to `94faaed0c60eddd8347767f2d4d29a97c93bde8c`; use the recorded revision rather than updating to a moving branch.

Set `VCPKG_ROOT` to your existing, ready-to-use vcpkg directory containing `scripts/buildsystems/vcpkg.cmake`:

```powershell
$env:VCPKG_ROOT = '<your-vcpkg-root>'
```

Replace the placeholder with your local path. [CMakePresets.json](../CMakePresets.json) reads that environment variable and requests Windows x64, the `v143,version=14.44.35207` toolset, `MultiThreadedDLL`, and `x64-windows-static-md`. [vcpkg.json](../vcpkg.json) owns package dependencies and their baseline. The unused tomlplusplus item is removed; no DAC build or runtime TOML parser is required. [Original DAC compatibility](dac.md#original-binary-compatibility-and-provenance) is checked against the untouched supplied binary. A normal configure uses that manifest; this guide does not supply a separate bootstrap or dependency-upgrade procedure.

## Build interface

Run from the repository root:

```powershell
cmake --preset Release
cmake --build --preset Release
```

The configure preset uses `build`; the build preset selects the `CharacterPanel` target and Release configuration. `cmake --list-presets=all` lists only those two Release entries. CommonLibSSE is a required static dependency. `ALL_BUILD` and `ZERO_CHECK` are normal CMake build/regeneration utilities, not alternate plugin projects. Dependency tests and unused installation generation are disabled.

### Reuse already-installed dependencies

Use this only when the installed-package directory is already populated with the manifest's required `x64-windows-static-md` packages. For the existing directory under this checkout:

```powershell
$characterPanelInstalled = (Resolve-Path .\build\vcpkg_installed).Path
cmake --preset Release -DVCPKG_MANIFEST_INSTALL=OFF "-DVCPKG_INSTALLED_DIR=$characterPanelInstalled"
cmake --build --preset Release
```

`Resolve-Path` requires the directory to exist. Disabling manifest installation does not provide missing headers/libraries. A different populated directory may be selected with the same CMake variable; it must match the required packages/triplet. Keep the toolchain consistent with the existing build cache.

## Architecture and generated outputs

[CMakeLists.txt](../CMakeLists.txt) lists 32 intended first-party `.cpp` translation units explicitly in sorted `PLUGIN_SOURCES`; headers retain their existing `CONFIGURE_DEPENDS` glob. It links CommonLibSSE and the existing installed libraries, embeds HLSL, and generates plugin/version metadata. No additional first-party executable or test target is configured.

OAR and DAC sources are nested under `src/animation/oar` and `src/animation/dac`. The explicit translation-unit list, existing header glob and `src` include root cover both. When adding/removing an intended plugin `.cpp`, update the sorted `PLUGIN_SOURCES` list and reconfigure; placing an unlisted file under `src` does not compile it. The list is declared directly in CMake and adds no Git runtime requirement for archived source builds. No separate target or namespace change is needed.

Production callers use root-relative headers such as `#include "animation/oar/hooks.h"` and `#include "animation/dac/session.h"` through `target_include_directories` in [CMakeLists.txt](../CMakeLists.txt). Direct-compiled offline tests use the same convention with `/I src`, as in the [shared cache cases](../tests/base_animation_cache_cases.h), and retain their [existing verification command](#fixed-base-index-verification). Both integrations use this shared include root; no additional module include directory or test target is required.

| Output | Role |
| --- | --- |
| `build/Release/CharacterPanel.dll` | current SKSE plugin |
| `build/Release/CharacterPanel.pdb` | symbols for that exact DLL; keep the pair together |
| `build/src/plugin.h` | configured name/version/author and `Plugin_Build_Identity` |
| `build/version.rc` | configured Windows version resource |
| `build/src/render/shader_sources.h` | shader text embedded during the build |

Normal Windows linker `.lib`/`.exp` files may accompany the DLL. They are not additional plugins. Nothing is automatically installed into a game directory.

## Code map

| Source | Entry point |
| --- | --- |
| [CMakeLists.txt](../CMakeLists.txt) | `CharacterPanel`, `CHARACTER_PANEL_BUILD_IDENTITY`, CommonLib linkage and source regeneration |
| [CMakePresets.json](../CMakePresets.json) | Release configure/build settings |
| [plugin.h.in](../cmake/plugin.h.in) | `Plugin_Build_Identity` and plugin metadata |
| [version.rc.in](../cmake/version.rc.in) | Windows resource input |
| [embed_shaders.cmake](../cmake/embed_shaders.cmake) | embedded shader header generation |
| [pch.h](../src/pch.h) | consumes generated plugin metadata |

## Invariants and build identity

Preserve the existing project/output/namespace and Release `/Zi` plus `/DEBUG:FULL` symbols. Keep CommonLibSSE and shader/automatic-generation plumbing when changing the main target.

`Plugin_Build_Identity` is generated **at configure time** from the configured label, Git revision/dirty state, CommonLib revision/dirty state and configure timestamp. A commit alone does not refresh an existing header or DLL. When preparing a binary for a new commit, run **configure, then build** again and retain its matching PDB. The log's `SCENEGRAPH BUILD` identity identifies that configured binary, not whatever HEAD happens to be later. Local untracked source files also affect the working-tree provenance: a clean tracked commit alone does not capture them or guarantee a clean/byte-identical build.

## Failure modes

Generator/toolset discovery errors require the stated VS2022/MSVC installation. Missing CommonLib source requires the recorded submodule; missing package headers/libraries requires a working vcpkg root and the requested installed triplet. Installed-dependency reuse is valid only when those packages exist.

A warning that installation rules were specified while `CMAKE_SKIP_INSTALL_RULES` is enabled can come from retained dependency declarations; it does not add an install target to this project. Follow actual configure/build exit results.

## Animation configuration

Copy [config/CharacterPanel.ini](../config/CharacterPanel.ini) to `Data/SKSE/Plugins/CharacterPanel.ini` alongside CharacterPanel.dll. Under `[Animation]`, `UnarmedIdle=non_combat_loop` is the default; `mt_idle` chooses direct vanilla MT_Idle only when hands contain no displayable equipped item. Missing files retain the default; invalid values warn/default. Restart the game after changes. Existing SimpleIni handles loading at DataLoaded; no new dependency, MCM or reload control is required. Calibration always stays vanilla MT_Idle. See the [exact option table](animation.md#configuration); the visible direct-base option does not change the independent zero-time calibration.

## Behavior graph baseline checks (2026-10-23)

Historical build ledger: this and the dated G1/G2 sections below describe each original build's scope, identity and then-pending work. Statements such as “not run”, “G1 not passed” and deployed INI values apply to that timestamp; [current status](#current-research-status-2026-10-26) supersedes them. Original commands/hashes remain for reproduction, not a request to rerun old experiments.

The [independent behavior graph experiment](independent-behavior-graph.md) adds a default-off, restart-required `[Diagnostics] BehaviorGraphProbe=false`, independent of `DetailedAnimationTrace`. This build only collects source ownership baselines in the existing retained-graph request bracket. It cannot create/link/drive/destroy an independent graph, evaluate OAR on a new instance or prove source isolation. G1 remains NOT PASSED and G2–G6 remain NOT STARTED/LOCKED. That round did not run or deploy the game; the current environment is recorded in the [G1 binary audit](#g1-binary-audit-2026-10-23) below.

Use the existing x64 MSVC 14.44 environment and ready installed dependencies, then configure/build as above. No new target, dependency or behavioral test is introduced for the read-only logger. Verification comprises the declared-member/API/ownership audit, default/call-site/side-effect review, document links/navigation, `git diff --check`, unchanged excluded-file hashes/index, Release compilation, DLL/PDB CodeView identity and ZIP/file integrity. The two planned native behavioral experiments remain UNEXECUTED; previous animation-repair scenario results are historical for this change.

Independent output is `build/independent-behavior-graph-20261023/test-package` and `CharacterPanel-independent-behavior-graph-20261023.zip` alongside it. The package contains matched DLL/PDB, Chinese steps, a minimal opt-in INI snippet and a manifest; merge the snippet into an existing config instead of replacing it. Preserve the earlier `build/animation-repair-20261023` package. Detailed command exits, source hashes, member inventory, build identity and pair/hash verification are retained in the new build folder and task record. To use the package later, enable the one flag, restart and collect `BEHAVIOR GRAPH` rows from ordinary panel-open/equipment requests; each starts with `role=source-baseline candidate=none construction=unverified gate1=not-passed`, at most sixteen grouped rows. Logging is not a passed gate.

Offline result: final Release configure/build passed with the existing ready packages, sixteen bounded source/document checks and twenty-three new relative links passed, and the final DLL/PDB matched CodeView GUID `{748DB13E-5AAA-4883-8283-751805A36ED4}`, age 76. The configured identity is `CharacterPanel-scenegraph-independent-behavior-graph-20261023-a9a0a20883+dirty-cl94faaed0c6-20261023T093448Z`; it is present in the final DLL. ZIP CRC and every packaged file hash passed. The first sandbox build hit MSB4184 on installed SDK metadata; the same authorized build with SDK read access passed. Existing install-rule and /Ob2→/Ob3 warnings remain. No native behavioral test ran.

## G1 experiment implementation (2026-10-24)

The [G1 runtime experiment contract](independent-behavior-graph.md#g1-实验契约2026-10-23-设计未实现) is implemented behind a new default-off `[Experiment] IndependentGraph=off|construct|load` (restart required, independent of the diagnostics flags). New sources are [independent_graph.h](../src/animation/behavior_graph/independent_graph.h) / [independent_graph.cpp](../src/animation/behavior_graph/independent_graph.cpp); the controller calls it once per process from the first paused `panel-open` request, after the retained-graph bracket. `construct` allocates `0x250` through `MemoryManager::Allocate(0x250, 0x10, true)`, runs the audited native constructor `63555` with `holder = nullptr` and holds the graph in `BSTSmartPointer`; `load` additionally calls the audited loader `63585` with a copy of the source `projectName`. Observation is bounded to 24 `INDEPENDENT GRAPH` rows (identity snapshots, 12-field comparisons, drift, verdict line), the source baseline is reused twice, and `registry-probe=unavailable` is explicit. The module never activates, advances, writes or drives any graph, and only releases by reference count. AE-only, version ≥ 1.6.1170; the run itself has not happened.

Verification: Release configure/build with the existing preset and installed packages passed with no warnings-as-errors; the new object is `build/CharacterPanel.dir/Release/independent_graph.obj`. The clean deployed identity is `CharacterPanel-scenegraph-scenegraph-5d4bee1d54-cl94faaed0c6-20261023T155405Z` (HEAD `5d4bee1`) and is present in the DLL; DLL SHA-256 `3BCC9FC14B6FD2492D2627CC0CBB4D9B12B6A9069ACB7B7D5C99E4BE58A4A658` (1,202,176 bytes) and PDB SHA-256 `127347733E9B8D565A3819E69582193848C6A4A091C41719EB5F76DA52A3411B` (34,779,136 bytes) come from the same link. Deployment on 2026-10-23 23:54 replaced the `5ef6f05` build under `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\` after backing it up to `dist/deploy-backup-20261023-5ef6f05/`; the deployed INI sets `IndependentGraph=construct` with both diagnostics flags off, so the next launch collects G1a only. The G1a run happened on 2026-10-24 00:00 with this exact build identity and passed its observable criteria (null-holder shell construction, reference-count release, byte-identical source baselines, no crash; `registry-probe=unavailable` stays UNVERIFIED); the raw log is archived as [CharacterPanel-g1a-construct-20261024-0000.log](diagnostics/CharacterPanel-g1a-construct-20261024-0000.log) and the verdict is recorded in the [G1a run record](independent-behavior-graph.md#g1a-运行记录2026-10-24-会话). The `load` stage (G1b) then ran in the same identity on 2026-10-24 00:11 and failed its pre-registered criteria: loader 63585 rejected the `BSFixedString*` second argument within 0.166 ms and left every assembly field null, while construction, destruction and both source baselines stayed clean ([raw log](diagnostics/CharacterPanel-g1b-load-20261024-0011.log), verdict in the [G1b run record](independent-behavior-graph.md#g1b-运行记录2026-10-24-会话)). Per the contract's stop-and-fallback clause the deployed INI is set back to `IndependentGraph=off`, and the second-argument form and rejection cause return to design review (static audit of 69488/63585) before any retry. The same-day follow-up audit ([second loader audit](independent-behavior-graph.md#g1b-运行记录2026-10-24-会话), recorded in the [binary audit](behavior-graph-construction-audit.md#第二实参形态与拒绝路径2026-10-24-续审计二)) then identified the root cause offline with no new build: 63585's second argument is a `const char*` full project path, so the `BSFixedString*` form was rejected exactly as observed. G1 remains NOT PASSED.

## G1b contract revision and reimplementation (2026-10-24)

The [second loader audit](behavior-graph-construction-audit.md#第二实参形态与拒绝路径2026-10-24-续审计二) identified 63585's second argument as a NUL-terminated full project path (`const char*`). The `load` branch of [independent_graph.cpp](../src/animation/behavior_graph/independent_graph.cpp) now rebuilds that path from the source character's own project string data (`stringData->behaviorPath` plus `projectName`, combined with the audited 69487 separator rules) and logs its content in a new bounded `phase=load-args` row before the call; the third argument stays `false`, which the audit measured as the Actor/PlayerCharacter holder's Unk_06 answer on the engine's own path. Construction, observation budget, once-per-process gating and reference-count release are unchanged.

Verification: Release reconfigure/build passed with only the existing /Ob2→/Ob3 override warning. The new DLL embeds the configured identity `CharacterPanel-scenegraph-scenegraph-646a0f2064+dirty-cl94faaed0c6-20261024T051200Z` (HEAD `646a0f2` plus the uncommitted revision), and DLL SHA-256 `1B3DA312DE53BD1169760BB40542829C1DF6007FC3F9AAF132DABD545D7BBB33` (1,202,320 bytes) and PDB SHA-256 `2478547CEB5ADE9F6537DE21221FC9A82EB9113F0A16BA84069A45B1BE1C34C9` (34,795,520 bytes) come from the same link. Deployment on 2026-10-24 13:12 replaced the `5d4bee1` build under `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\` after backing it up to `dist/deploy-backup-20261024-5d4bee1/`; the deployed INI sets `IndependentGraph=load`. The G1b v2 run has not happened.

<a id="g1b-v3-顺序取值实验-2026-10-24"></a>
## G1b v2 run and v3 sequential-value experiment (2026-10-24)

The v2 build ran on 2026-10-24 13:30 and was rejected again in 0.289 ms, this time with the new `load-args` row showing `behavior-path='' project-name='DefaultFemale' path='DefaultFemale'` ([raw log](diagnostics/CharacterPanel-g1bv2-load-20261024-1330.log); verdict in the [v2 run record](independent-behavior-graph.md#g1b-v2-运行记录2026-10-24-会话)). The follow-up static audit ([third loader audit](behavior-graph-construction-audit.md#加载取值与前置链的续审计三2026-10-24g1b-v2-复盘)) established that registry keys carry the directory and extension (helper 69489 strips the extension only when writing `projectName`) and that 63949 creates the character setup itself, so the rejection was a pure registry miss.

The `load` branch was then reimplemented (v3): one read-only `race-fields` row dumps the holder's race candidates (`behaviorGraphs[sex].model`, `rootBehaviorGraphNames[sex]`, `behaviorGraphProjectNames[sex]`, `stringData->behaviorPath`, source `projectName`), and the loader is called in a pre-registered priority order (bg-model → project-name → root-name → stringData rebuild), at most four tries, stopping at the first accepted load — failed loads are the audited no-op path on a bare shell. Construction, budget (16 rows worst case), once-per-process gating and reference-count release are unchanged.

Verification: Release reconfigure/build passed with only the existing /Ob2→/Ob3 override warning. The new DLL embeds `CharacterPanel-scenegraph-scenegraph-646a0f2064+dirty-cl94faaed0c6-20261024T054649Z`, DLL SHA-256 `A3C2213F5B31A39652B79742FE49F512B91F745A55973EBA9AC5F3C9BD822C0D` (1,209,856 bytes), PDB SHA-256 `9753128A494A1F801118AFE4B7FF53B08098FA76BE5D9B3559400EDAF4B35046` (34,861,056 bytes). Deployment on 2026-10-24 13:55 replaced the v2 build after backing it up to `dist/deploy-backup-20261024-g1bv2/`; the deployed INI keeps `IndependentGraph=load`. The G1b v3 run happened the same day at 14:25: the first candidate (`race-bg-model` = `Actors\Character\DefaultFemale.hkx`) loaded successfully in 8.729 ms with every assembly field non-null and the shared resources same-addressed as the source, and the process then crashed in the destruction chain — `hkbBehaviorGraph::Deactivate` (vtable slot 7, reached from the `BShkbAnimationGraph` destructor) dereferences the null `activeNodes` of a never-activated graph (`SkyrimSE.exe+0AB1FC3`; attribution in the [fourth audit](behavior-graph-construction-audit.md#已装载图析构的崩溃定位2026-10-24-续审计四), verdict in the [v3 run record](independent-behavior-graph.md#g1b-v3-运行记录2026-10-24-会话)). Per the contract's stop-and-fallback clause the deployed INI is back to `IndependentGraph=off` (the deployed DLL is unchanged, so the experiment stays dormant); raw logs are archived as [CharacterPanel-g1bv3-load-crash-20261024-1425.log](diagnostics/CharacterPanel-g1bv3-load-crash-20261024-1425.log) plus the Crash Logger report. The destruction criterion returned to design review and was resolved the same day: the contract was revised (destruction evidence = shell release plus the loaded graph's teardown inside the G2 lifecycle) and the `load` branch now retains the assembled candidate in a deliberately never-released owning smart pointer (leaked on purpose — even static destruction must not run the engine's destructor chain; at most one loaded graph per process while the experiment is enabled), with the tail rows renamed to `phase=retained`/`phase=destroyed` accordingly. The v4 build embeds `CharacterPanel-scenegraph-scenegraph-646a0f2064+dirty-cl94faaed0c6-20261024T065321Z`, DLL SHA-256 `37C255DA5985DCF3F7ECA3CA6F56F0D570A0AA008A0177A1391E9440D3B4C3ED` (1,209,856 bytes), PDB SHA-256 `F5D290D2575D158F41D4804E9CDEF2F09B636D919DC779DAA66BD3CA22AA94F4`; deployed on 2026-10-24 15:10 after backing v3 up to `dist/deploy-backup-20261024-g1bv3/`, with the INI set back to `IndependentGraph=load`. The v4 run happened the same day at 14:57 and completed the G1 verdict: the load reproduced (`accepted ms=8.797`, identical race-fields), the post-load source baseline (captured while the candidate was alive) is byte-identical to the pre-construct baseline under an independent diff, and the session ended with `phase=end … retained=true`, no crash, and several further normal panel open/close cycles with the retained graph inert ([raw log](diagnostics/CharacterPanel-g1bv4-retain-20261024-1457.log), verdict in the [v4 run record](independent-behavior-graph.md#g1b-v4-运行记录与-g1-判定2026-10-24-会话)). **G1 is PASSED under the revised (Option A) criteria**; the deployed INI is back to `IndependentGraph=off` (the deployed DLL is unchanged) so future sessions do not re-run the experiment, and the loaded graph's destruction evidence moves to the G2 lifecycle. G1 remains the only gate passed; G2's prerequisite (the Activate/Update entry audit) has not started.

## G2 drive experiment implementation (2026-10-24)

The [G2 contract](independent-behavior-graph.md#2026-10-24-g2-契约drive-模式实验设计) adds a `drive` value to the existing experiment setting: after the unchanged G1 construct/load/retain flow the same paused bracket captures a full source baseline (pre-activation), calls the engine's own activator 63586 once on the candidate, then drives `hkbBehaviorGraph::Update` (58329, vtable slot 5) 120 times at a fixed 1/60 s step through a bare 58315-built context, and records three structural snapshots plus a pose-motion hash tier (source control pair and candidate steps 0/60/120). The context lives in raw zero-initialized storage because `hkbContext`'s `hkRefPtr<hkbGeneratorOutputListener>` member has no complete type on this side and can be neither constructed nor destructed here; 58315 writes every field as raw stores. Failed loads keep the proven shell path; the retention policy is unchanged.

Verification: Release reconfigure/build passed with only the existing /Ob2→/Ob3 override warning. The DLL embeds `CharacterPanel-scenegraph-scenegraph-646a0f2064+dirty-cl94faaed0c6-20261024T072214Z`, DLL SHA-256 `A3DEDE5FDD4635EEC928D54F73FCF00C6F9862F0A9F30C531264FD42B36B4F68` (1,216,000 bytes), PDB SHA-256 `452DE1802FDB15042587534638691068D0A87F77C6B85D0CB6DC5A83BD2F1FF2` (34,942,976 bytes). Deployment on 2026-10-24 15:35 replaced the v4 build after backing it up to `dist/deploy-backup-20261024-g1bv4/`; the deployed INI sets `IndependentGraph=drive`. The G2 run happened the same day at 15:31: activation fully succeeded (`activeNodes` 0→97 nodes, variable set allocated, flag set), the source stayed untouched (both baselines and the source pose-hash control identical), and all 120 update steps returned without a crash — but the candidate produced no observable motion: its `poseLocal` was never allocated (all three pose hashes `unavailable`) and the structural snapshot is unchanged across the drive. G2 is NOT PASSED on the motion criterion (insufficient evidence, per the pre-registered rule); the deployed INI is back to `off`. Raw log: [CharacterPanel-g2-drive-20261024-1531.log](diagnostics/CharacterPanel-g2-drive-20261024-1531.log); verdict in the [G2 run record](independent-behavior-graph.md#g2-drive-运行记录2026-10-24-会话). The pose-write chain audit ([sixth audit](behavior-graph-construction-audit.md#姿态写入链与字符级激活2026-10-24-续审计六)) then found the root cause: `hkbBehaviorGraph::Update` never writes `poseLocal`; the pose buffer is allocated by 58466 (`hkbCharacter::AllocatePose`, from `setup.animationSkeleton`) inside the ENGINE's character-level activator 58453, which runs AllocatePose BEFORE Activate and is gated on the graph's +0x12C flag — the graph-level 63586 wrapper used in the first run bypassed it entirely. The drive flow was revised (v2): one 58453 call replaces 63586, with a pre-registered clean skip (`skip=setup-skeleton-missing`) when the fresh setup's `animationSkeleton` is still null (the allocator would dereference it unchecked), and the structural snapshot now also records the setup fields and the +0x12E children-activation gate. The v2 build embeds `CharacterPanel-scenegraph-scenegraph-646a0f2064+dirty-cl94faaed0c6-20261024T075048Z`, DLL SHA-256 `99E0B38E44B1B743E4518BCEB5FDE64CCAB8469711E8B6E20F8E44E4A5E0DD26` (1,216,512 bytes), PDB SHA-256 `8CA2AAE664CDB9425599BA7763A5DFC02DA4F4C2A584BE7E958978E49D88DDA3`; deployed on 2026-10-24 16:05 after backing the first drive build up to `dist/deploy-backup-20261024-g2v1/`, with the INI set back to `IndependentGraph=drive`. The G2 v2 run happened the same day at 15:57 and validated the fix: the character-level activator allocated the pose buffer (`pose-local=…/116`, and `setup.animationSkeleton` confirmed non-null — the load chain does fill it), activation reached 97 active nodes, the source stayed untouched and nothing crashed — but the candidate's three pose hashes are identical (`bc160a8c…`): the buffer keeps the skeleton bind pose written by AllocatePose, so the update's output never reaches it. A tail audit of 58329 confirmed it contains no `poseLocal` write at all. The decisive discriminator is now implemented and pre-registered: `phase=gen-hash` rows hash the candidate's `generatorOutputs[0/1]` (self-describing window via the track master header's byte count) at activation, step 60 and step 120 — changed hashes mean generation works and only the sampling step is missing; unchanged hashes point upstream (+0x12E gate / 60079 semantics / rootGenerator state). Build `…20261024T080506Z`, DLL SHA-256 `A03705169F725DB117997DEA81079FAEA4CDAD26A46F43E5FC096C4448DB4650` (1,219,072 bytes), deployed 2026-10-24 16:20 with the INI set to `IndependentGraph=drive` ([v2 run record](independent-behavior-graph.md#g2v2-drive-运行记录2026-10-24-会话); raw log [CharacterPanel-g2v2-drive-20261024-1557.log](diagnostics/CharacterPanel-g2v2-drive-20261024-1557.log), INI reverted to off after the run and re-set for the discriminator run). The discriminator run happened at 16:09: the generator-output hash read garbage (`numBytes=-898388560` — the objects do not carry the CommonLib TrackMasterHeader layout, which is an unverified activeragdoll port), so the window assumption failed; the pose hashes stayed identical again. A context-read enumeration of 58329 then established the update consumes ONLY the activeNodes list, ctx.eventQueue and the timestep, and that vtable slot 6 is an event-dispatch-to-active-nodes helper — neither is a missing output request. The probe was revised (v3): `phase=probe` rows hash the activeNodes entry array (0x90-byte per-node runtime states, self-describing count) and the variable set word array at activation/step 60/step 120, plus a raw 16-byte dump of each generator output. Changed hashes mean the update advances node state (and only the pose sampling is missing); a fully frozen array means the per-node gates skip everything. Build `…20261024T081858Z`, DLL SHA-256 `A8A4BF3D4C8938B0A5D811CF60F18B3E0A0E7FC7D2628280F937AC7420360CBE` (1,220,096 bytes), deployed 2026-10-24 16:40 with the INI set to `IndependentGraph=drive`. The probe run at 16:22 was decisive for the generation chain: the activeNodes entry-array hash changes every step (three distinct values) — the update does advance all 97 nodes — while the pose buffer stays at the bind pose and the generator-output dumps revealed one-element array headers rather than pose tracks. The sampler was then located: 63581 (0xbafda0) copies the generated pose track from the generator-output element into poseLocal via the same 57148 fill helper AllocatePose uses, entered through 63589 (0xbb2a40) whose gates the candidate passes EXCEPT the current-animation word at graph+0x240 (written per frame by the manager-driven 63587; the candidate has no manager). G2 v4 mirrors `[source+0x240]` into the candidate after each update step (an engine-meaningful field for the same graph over the same shared binding set) and calls 63589 to sample. Build `…20261024T083143Z`, DLL SHA-256 `AA0A36AF621CE04447546BD521FC526EDC10EC4984B21148F751D6A4202A0CB4` (1,221,120 bytes), deployed 2026-10-24 16:50 with the INI set to `IndependentGraph=drive`. The v4 run happened at 16:35 and corrected the field semantics: `[graph+0x240]` is NOT an animation index but the SAMPLE-TRANSFORM COUNT (0x74 = 116 = the skeleton bone count; 63587 clamps it against exactly that count), the mirror landed correctly (`cand-anim=0074`) and 63589 was called every step without a crash — but the generator-output ELEMENT content hash stayed constant across all 120 steps: the generation stage writes nothing into the output. G2 remains NOT PASSED on the motion criterion; INI back to `off` ([raw log](diagnostics/CharacterPanel-g2v4-drive-20261024-1635.log)). The remaining gap is the manager per-frame pipeline the drive flow skips: the event-processing stage (63602 at 0xbb4c50, which sets the +0x12E children-activation-request flag; called from 0xba4800 in the manager cluster) and 63587's pre-update calls (63761 with the global singleton, 77924 with physicsWorld — possibly where the output buffers get bound for the frame). The pipeline audit then found the capstone: a notification-hub singleton (0x2022570, vtable 0x1986740) subscribes graphs (63761) and its slot-5 handler runs the per-frame graph pipeline — 63602's tail sets **`behaviorGraph+0x12E = 1` (the per-frame children-activation request) before calling 63587** for each subscribed graph. Our drive never set that byte, so Update's internal gate skipped its 60079 activation walk every frame — the root cause of the silent generation. G2 v5 adds exactly that one-byte request before each update step (everything else unchanged). Build `…20261024T085731Z`, DLL SHA-256 `9A51725C635B3A7D7A98E7916DB012FAE108D4AE0D7E2277FA9F6B37B4A71194` (1,221,120 bytes), deployed 2026-10-24 17:05 with the INI set to `IndependentGraph=drive` ([v4 log](diagnostics/CharacterPanel-g2v4-drive-20261024-1635.log), INI reverted to off after the v4 run and re-set for v5). The v5 run at 17:02 honored the request (children-flag consumed, node states churning) but the generator-output element and the pose buffer STAYED constant — the children-activation request alone does not unlock generation. Strategy pivot (v6): instead of hand-assembling the drive from parts (raw 58329 + field mirror + 63589), each step now calls the ENGINE's own per-frame update wrapper 63587 with the instruction-audited 63602 record bytes (all-zero except the transform-count word @+0x28 = the candidate's numPoseLocal 116, a flag byte @+0x2A = 1 and the dword 0x1000101 @+0x2B) — the wrapper internally subscribes the graph into the notification hub, writes graph+0x240, runs the ragdoll/physics pre-calls and the raw update; the 63589 sampling entry still follows. Build `…20261024T090815Z`, DLL SHA-256 `8FC00E4EFD43B7D901D8F49BD2394262C5765AD58355DDA1AE512E52CA8E5A4D` (1,221,120 bytes), deployed 2026-10-24 17:20 with the INI set to `IndependentGraph=drive` ([v5 log](diagnostics/CharacterPanel-g2v4-drive-20261024-1635.log) era reverted to off between runs). The v6 run crashed immediately on opening the panel: 63587's post-update stage 63583 registers the animation NAME into a global table derived from `projectDBData->+0xD0` and indexed by the animation selection — context only the manager-driven flow provides; on the candidate `BSFixedString::Set` wrote through a null holder (`SkyrimSE.exe+0CEC6B1`, full attribution in the [crash report](diagnostics/CharacterPanel-g2v6-crash-report-20261024-1711.log)). The wrapper route is therefore closed: the drive was reverted to the crash-free v5 form (raw 58329 + transform-count mirror + 63589) and redeployed — build `…20261024T091522Z`, DLL SHA-256 `3733ACFEC1F841671111D0DFEDA85D56075FA3831EC2B08681E0A2BC0F3473BA`, INI back to `IndependentGraph=off` ([v6 crash log](diagnostics/CharacterPanel-g2v6-crash-20261024-1711.log)). Strategic conclusion after six drive iterations: every stage of the drive chain is now instruction-level mapped, and the remaining gap — animation SELECTION during generation — is manager-driven and belongs to feature design (the panel preview will choose animations itself), not to feasibility. G2's motion criterion needs redefinition at design review.

## G2 v7 engine-pipeline drive (2026-10-24)

The [per-frame pipeline audit (seventh)](behavior-graph-construction-audit.md#管理器每帧管线与记录全解2026-10-24-续审计七) revised the six-iteration strategic conclusion: the 63587 record carries NO animation identity (63575 is the LOD sample-count resolver, 63583 the event-drain/name bookkeeping whose registration block is statically skipped with the engine's own null third argument), and the real gap is that the drive never ran the GENERATION traversal (65149→65232→65237) — it only exists inside the engine's per-graph frame pipeline 63602. The [G2 v7 contract](independent-behavior-graph.md#2026-10-24-g2-v7-契约引擎管线驱动经续审计七设计) replaces the v5 bare-update loop: each of the 120 steps now replicates the notification-center slot-5 recipe exactly — a stack shell graph constructed by 63555, one `63602(shell, candidate)` pass (event collection, the generation traversal into `generatorOutputs`, a zero-dt update, the event drain, 63584), shell destruction by 63556 — followed by the proven real-dt 58329 update, the `+0x240` transform-count mirror and the 63589 sampling entry. The shell pair runs on stack storage with the engine's own ctor/dtor, both valid only on empty graphs, which the shell always is. One bounded `phase=pipeline` summary row is added; every other observation row is unchanged.

Verification: Release reconfigure/build passed with only the existing /Ob2→/Ob3 override warning. The DLL embeds `CharacterPanel-scenegraph-scenegraph-440e7cf784+dirty-cl94faaed0c6-20261024T130326Z` (HEAD `440e7cf` plus the uncommitted v7 revision), DLL SHA-256 `12663FFE3FCBD011B53CB6DCB8C05C742DF56C871309DFDD00C43B00A435AB9A` (1,222,144 bytes), PDB SHA-256 `B740657426538CA276B75CD92B326EDCC79109892DDF9B5D82C1C36E0F31327F` (35,033,088 bytes). Deployment on 2026-10-24 replaced the `3733ACFE` build under `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\`; the deployed INI sets `IndependentGraph=drive`. The v7 run happened the same day at 21:08 and stayed clean: the load reproduced, the character-level activation allocated the pose buffer (97 active nodes, 116 transforms), all 120 engine-pipeline steps (120 shell construct/destroy pairs plus the real-dt update, mirror and sampling) returned without a crash or assertion, and the source stayed untouched (pose-hash control identical, drift none ×3) across further normal panel cycles — but the candidate's three pose hashes are still identical AND the generator-output element hashes stayed constant, so the pre-registered "frozen elements" attribution applies: without event injection the default state produces no pose, and the remaining gap is the event/initialization sequence that walks the state machine into idle (merged into G3's design). G2 v7 is NOT PASSED on the motion criterion per the contract's own attribution table; the deployed INI is back to `IndependentGraph=off`. Raw log: [CharacterPanel-g2v7-pipeline-20261024-2108.log](diagnostics/CharacterPanel-g2v7-pipeline-20261024-2108.log); verdict in the [v7 run record](independent-behavior-graph.md#g2-v7-运行记录2026-10-24-会话).

## G2 v8 inject experiment (2026-10-24)

The [dispatch-chain audit (eighth)](behavior-graph-construction-audit.md#事件派发链与槽-2-输出门2026-10-24-续审计八) located the event-injection primitive: 58377 is the engine's event-triggered-transition dispatch (hkbBehaviorGraph vtable slot 6's callee; Update's own tail and the batch dispatcher route queued events through it), event names are resolved at runtime from the projectDBData name→id maps, and the per-frame output reset (graph vtable slot 2 / 63621) is double-gated on physicsWorld and the ragdoll driver — both empty on the candidate, the pre-registered next attribution if injection changes nothing. The [G2 v8 contract](independent-behavior-graph.md#2026-10-24-g2-v8-契约inject-模式事件注入判别经续审计八设计) adds a new `inject` setting value: the full v7 drive flow preceded by a read-only BSTHashMap name→id lookup over the shared projectDBData maps (unkA0 then unk70, inline entry walk, no engine state touched) for the pre-registered candidates `MT_Idle`/`MoveStart`/`WeaponDraw`/`SyncForward`, then ONE dispatch of the first resolved name through 58377 with a stack hkbEvent before the first pipeline step; all names missing is a clean `skip=no-known-event`. The observation budget is pre-registered at 30 rows; everything else is unchanged.

Verification: Release reconfigure/build passed with only the existing /Ob2→/Ob3 override warning. The DLL embeds `CharacterPanel-scenegraph-scenegraph-440e7cf784+dirty-cl94faaed0c6-20261024T133638Z` (HEAD `440e7cf` plus the uncommitted v7/v8 revision), DLL SHA-256 `69B8683D2173E6C6FD17B412A2A570CAC956BCD2B3F25E4AE2148CE858051BF0` (1,225,216 bytes), PDB SHA-256 `AF4B36EC172ABC0C605BA86F6241FD93C43F7CC3B3C998B8315BC7BB19117541` (35,074,048 bytes). Deployment on 2026-10-24 replaced the `12663FFE` build under `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\`; the deployed INI set `IndependentGraph=inject`. The first v8 run crashed at 21:45 on opening the panel: the crash is in the PLUGIN's read-only map walk (`lookup_event_id`), whose bucket-chain loop stopped on null instead of the BSTHashMap sentinel at `map+0x18` (the engine's own walks in 63639/63773 stop there), so an empty bucket's sentinel was dereferenced as an entry and its contents handed to strcmp (`rcx` = the ASCII bytes of a name string); the 58377 dispatch never ran and the contract criteria are unaffected ([run record](independent-behavior-graph.md#g2-v8-运行记录2026-10-24-会话崩溃与修复), [crash report](diagnostics/CharacterPanel-g2v8-crash-report-20261024-2145.log)). The fix stops the chain walk at the sentinel (with a null guard and a chain-length cap); the rebuilt DLL embeds `CharacterPanel-scenegraph-scenegraph-440e7cf784+dirty-cl94faaed0c6-20261024T134847Z`, DLL SHA-256 `109D1C7DFE8427DFBF56FD206002D214EB0791C0D9BEA701D8B029018BBBF96E` (1,225,216 bytes), PDB SHA-256 `428CA9B4F98E076149239A3E3CA23E466F24C78EE75891EAF860BDE6D7C4ECE0` (35,074,048 bytes), redeployed with the INI back to `inject`. The rerun crashed AGAIN at the same strcmp with different string bytes as the bogus pointer (`0x6F47646542434C4D` = `MLCBedoG`) — the sentinel stop was right, but the +0xA0 map cannot be walked with the unk70-derived bucket layout at all; the assumed array holds name pointers ([v8b log](diagnostics/CharacterPanel-g2v8b-inject-crash-20261024-2154.log), [crash report](diagnostics/CharacterPanel-g2v8b-crash-report-20261024-2154.log)). The map-walk route is retired: the lookup now linearly scans the engine-verified animation-name array (unkD0 at db+0xD0, id = array index, bounds at db+0xD8/0xE0 — the exact array 63583 reads), preceded by one raw `phase=dbdump` row of db+0x60..0xE8 so the real structure lands in evidence. Build `…20261024T135805Z`, DLL SHA-256 `5764BCB546300E52DE0708915DADC92E997AF5B771DF46E09B97308F605EFB63` (1,225,728 bytes), PDB SHA-256 `A7450A3524FCE73188DDD2D8EA0E011B25781E4F41829FD0A25B1B83CFC91BB6` (35,082,240 bytes); deployed with the INI set to `inject`. That rerun (22:04) completed cleanly with no crash and the source untouched, but all four pre-registered names missed (`id=-1` → pre-registered clean `skip=no-known-event`; the 58377 dispatch still has not executed) — and the dbdump row settled the structure: BOTH hash maps match the declared layout (module-static sentinels at +0x18 both, counts 1024/16384 power-of-two, heap bucket arrays), so the two crashes' real root cause is TOMBSTONE entries in the bucket chains whose key bytes are garbage — the engine only ever pointer-compares keys, making any by-content walk unsafe — and the 9379-entry db+0xD0 array is the animation/clip-name table, not the event table ([v8c log](diagnostics/CharacterPanel-g2v8c-inject-skip-20261024-2204.log), decode in the [run record](independent-behavior-graph.md#g2-v8-运行记录2026-10-24-会话崩溃与修复)). The lookup's authoritative source is now the behavior graph's OWN string data (`hkbBehaviorGraphData::stringData->eventNames`: a plain Havok hkArray of hkStringPtr, managed-bit masked, event id = array index — the very id space the state-machine transition maps key on), with a new bounded `phase=events` evidence row listing the first sixteen real event names; the pre-registered name list, injection and criteria are unchanged. Build `…20261024T141115Z`, DLL SHA-256 `D86BAA8706B20AD0DF73D04AE7F5A9476BD58A81B7B6A0E8A5A199C9DF8E7C9A` (1,228,288 bytes), PDB SHA-256 `37485FC9A4ED18C04D45EA30BBEEDE0A135566A112A0D55DBEDC642DF57006A6` (35,098,624 bytes); deployed with the INI set to `inject`. That run (22:18) was clean and put the graph's event table on record for the first time — `count=8223` with `FootLeft`/`FootRight`/`WeapEquip`/`SprintStart`/`SprintStop`/`1HM_Unequip` and the MRh/MLh cast family among the first sixteen names — proving the pre-registered `MT_Idle`/`MoveStart`/`WeaponDraw`/`SyncForward` are Skyrim-level anim-event names, not behavior-graph events (all four missed → pre-registered clean skip; the 58377 dispatch has still not executed; [v8d log](diagnostics/CharacterPanel-g2v8d-inject-skip-20261024-2218.log)). The candidate list is re-registered from that evidence for v8e — `WeapEquip` (equip-chain entry, matching the G3 first batch) → `SprintStart` → `FootLeft` → `FootRight`; injection and criteria unchanged. Build `…20261024T141951Z`, DLL SHA-256 `173647478DA17435E8B1E1D866DFED7D6270D8EE07566A7F82C90C884A9A7186` (1,228,288 bytes), PDB SHA-256 `3CA2C20608CD35A123396D8717D73D4B07E6296F093B328607F2A039FBFCC009` (35,098,624 bytes); deployed with the INI set to `inject`. That run (22:28) is the injection milestone: all four evidence-derived names resolved (`FootLeft=0`, `FootRight=1`, `WeapEquip=2`, `SprintStart=12`), the 58377 dispatch executed for the first time without a crash, and the state machine genuinely transitioned — 97→92 active nodes, the variable set rewritten, the graph settling into a stable state — while the generator outputs and pose hashes still did not move, landing in the pre-registered slot-2 attribution branch ([v8e log](diagnostics/CharacterPanel-g2v8e-inject-fired-20261024-2228.log)). v9 adds one read-only `phase=gates` row (source/candidate physicsWorld and ragdoll driver plus both generator-output element heads with their +0x32/+0x38 fields) whose pre-registered decision rule kills or confirms the slot-2 hypothesis: a living source actor with a null ragdoll driver still producing poses falsifies it. Build `…20261024T143116Z`, DLL SHA-256 `898C46F0217C5B0FC16828E8A137F115` (1,229,824 bytes); deployed with the INI set to `inject`. Its run (22:35) settled the attribution with one row: the living source passes BOTH slot-2 gates (physics world and ragdoll driver both non-null — the death-only assumption was wrong), the candidate already owns a ragdoll driver from the load chain and lacks exactly the physics world, and the generator-output elements are byte-identical in their heads while the source's carries the bound track fields (`+32=0x0074` transform count, `+38=1.000`) against the candidate's zeros — the slot-2 hypothesis is confirmed ([v9 log](diagnostics/CharacterPanel-g2v9-gates-20261024-2235.log)). The pre-borrow audit then closed the shared-world risk: slot 2's only touches on the borrowed world are the vtable-40 getter (a pure `mov rax,[rcx+0x10]` read) and a release-disabled assertion stub; the rest of the unlocked body is candidate-local. v10 borrows the source's physicsWorld for the drive duration only (raw pointer, refcount untouched, restored to null immediately after the loop, one `phase=borrow` row), with criterion ① narrowed to the pose hashes — an element-hash change alone now only proves the binding gate opened. Build `…20261024T144235Z`, DLL SHA-256 `789C5026EC5DE08993D3C9C8D2F4357A19E655A5` (1,229,824 bytes). Its run (22:45) executed and restored the borrow cleanly (no crash, source untouched, injection/transition reproduced) but the binding fields were STILL not written — the element hash never moved, so by the pre-registered attribution the borrow was ineffective: the slot-2 call inside 63602 sits in a loop whose count is filled through ARG2's vtable slot 0x58 out-parameter, and the candidate's side of that count is zero ([v10 log](diagnostics/CharacterPanel-g2v10-borrow-20261024-2245.log)). The gap converges back — precisely this time — to manager-side state: the binding loop walks a current-animations/channels list the manager maintains each frame; a manager-less candidate is empty by construction. The INI is back to `off`; the next work package is the static audit of the slot-0x58 provider and what populates its list, plus resolving the primary-vtable slot-label ambiguity recorded in the run record.

## G1 binary audit (2026-10-23)

Historical initial-audit record only: later loader, activation and v17 results are in [the canonical current record](independent-behavior-graph.md#current-status); the original offline claims below are dated to 10-23.

The [independent behavior graph audit](behavior-graph-construction-audit.md) adds no runtime code. It reads a copy of the retail `SkyrimSE.exe` 1.6.1170 (SHA-256 `C434208894F07F604B852F29B8EDC3A58C4DE63DE783373733E72B2B73F33BE9`), which is SteamStub 3.1 packed (`.bind` entry point, whole `.text` entropy about 7.997). The copy is unpacked with Steamless 3.1.0.5 from the official release (zip SHA-256 `E3E2D22E098FF3FB359B2876AA2BED9596F0501E6FF588CBFFAE90A76D2DC4F5`); the unpacked image (SHA-256 `843823C115DBC56B5E9740350DB49170CC4EEA013A79513B7FD066DEEC719A2B`) restores the `.text` entry point, keeps `.rdata` byte-identical and its RVAs align with the pinned `versionlib-1-6-1170-0.bin` (format 2, 428,973 records; SHA-256 `C4093C569A3C83B26587F4B9EA4C55DE9AE6E73B84A2AF9FB3FBD30E2FE0D452`).

Verification covers vtable/RTTI identity and subobject offsets for the holder, graph and manager classes; every slot-5 target checked against a `.pdata` function start; and disassembly of the five `ConstructAnimationGraph` implementations, the `BShkbAnimationGraph` constructor, both destructors and the allocator/deallocator pair. Reproduce with the scripts and commands recorded in `build/g1-audit-20261023/`; they only parse the PE and versionlib and never load a module, call the engine or touch the game directory. The original executable was not modified.

The audited facts are limited to construction/ownership/destruction: allocation size `0x250`, holder and flag arguments, no project loading, no manager or global registration, and paired aligned deallocation. Project loading, linking, activation and `hkbCharacter` setup remain unidentified; G1 is still NOT PASSED and G2–G6 remain locked. The machine does have an AE 1.6.1170 installation and an MO2 instance; this round did not deploy or launch the game, so no behavioral or visual claim is made.

## Animation repair checks (2026-10-23)

Exactly two current engine-free scenarios in [animation_repair_test.cpp](../tests/animation_repair_test.cpp) use production `transition_hands`, `left_shield_transition`, `ied_condition_rejection`/`evaluate_ied_condition`, `standing_movement_direction`, route/cache helpers and `compose_contributor`:

- `equipment_removal_preserves_query_hands_and_native_ied_boundary`: shield then weapon removal contexts, native false for an unchanged requested hand, rejection before native read for a changed hand, direction-0 standing/negation.
- `one_handed_torch_composes_complete_left_arm_and_removes_cleanly`: one-handed body plus cached Tor_1HMPose route, complete left arm mask while body/right arm/legs remain, and body restoration after removal.

Run from the repository root in the existing x64 VS2022 developer environment (`vcvarsall.bat x64 -vcvars_ver=14.44`, installed toolset 14.44.35207). Keep assertions enabled; do not define NDEBUG:

```powershell
New-Item -ItemType Directory -Force build/animation-repair-20261023 | Out-Null
cl /nologo /std:c++latest /EHsc /W4 /WX /I src tests/animation_repair_test.cpp /Fobuild/animation-repair-20261023/animation_repair_test.obj /Febuild/animation-repair-20261023/animation_repair_test.exe
./build/animation-repair-20261023/animation_repair_test.exe
cmake --preset Release
cmake --build --preset Release
```

The two scenarios passed. The same torch-routing regression compiled against baseline action.h exits 1 as expected (whole-body torch selected), using an explicit exit rather than an assertion dialog. Release configure/build passed using the existing installed packages and cache (`VCPKG_MANIFEST_INSTALL=OFF`), without dependency changes. The first sandboxed build was denied access to installed Windows SDK metadata by MSBuild (MSB4184); the same authorized local build succeeded with SDK read access. Existing /Ob2→/Ob3 override warnings remain. `git diff --check`, bounded relative-link/symbol/source checks and preserved unrelated-file hashes are separate checks. No older driver or full suite is rerun; retained obsolete assertions are updated for current policy only.

The 10-23 source review covers typed transition effect-refresh, pending reuse/cancel, arm readiness/retarget/same-asset/outgoing composition, raw XYZ sampling and trace caps. Later [r4](diagnostics/display-issues-2026-10-22.md#r4-session) observes accepted shield selection, composed torch rows and five matched restorations; scale-x as a visual contributor still needs native semantics and aligned A/B. The scenarios do not execute `AnimationController`, the game loader, OAR/IED hooks, DAC, camera or rendering. Native condition answers, asynchronous timing, complete-left-arm coverage against installed HKX assets, source isolation at runtime, Smooth Start/Loop, axe flash and bow/crossbow scale causes remain UNVERIFIED. No clamp or guessed mapping was added. The final DLL/PDB pair matches CodeView/PDB GUID `{748DB13E-5AAA-4883-8283-751805A36ED4}`, age 74; hashes and configure-time identity are in the package manifest and dated handover. At the original 2026-10-23 pre-run package-verification stage, the DLL/PDB was copied only to the ignored `build/animation-repair-20261023/test-package`; that step performed no game deployment. The later actual deployment and r4 session are recorded in the [dated handover](diagnostics/display-issues-2026-10-22.md#r4-session); they do not retroactively make the earlier offline checks runtime tests. Follow the [current handover](diagnostics/display-issues-2026-10-22.md#10-2026-10-23-装备上下文与火把分层修复待上机).

## Verification and limits

### Current animation checks (2026-10-22)

Historical check selection for that dated change; current repair scope is the two scenarios above.

The completed animation changes have four narrowly scoped engine-free drivers. Each ran for its own implementation change; documentation/commit finalization reuses those passing results and does not run a new suite or add cases. The normal Release build separately compiles the real SDK integrations.

| Driver | Recorded cases and observable boundary | Not covered |
| --- | --- | --- |
| [dac_event_match_test.cpp](../tests/dac_event_match_test.cpp) | **2**: mixed-case Smooth loop tag matches; loop-off remains distinct using the production comparator. | Engine Actor preflight/native DAC execution; whole-event checks and original spelling are source-reviewed. |
| [torch_shield_policy_test.cpp](../tests/torch_shield_policy_test.cpp) | **2**: weather goes through source forwarding without a projected constant; left-shield equip/remove retains the right weapon and correct former-hand query context. | Native weather/provider answers, pending-load timing or visual equivalence. |
| [animation_mapping_test.cpp](../tests/animation_mapping_test.cpp) | **2**: accepted alias/variant changes and new requests publish while identical loops coalesce; one selected read at index 40,000 in a 50,000-name table. | Actual current-world OAR API, loader fallback timing or game performance. |
| [diagnostic_trace_test.cpp](../tests/diagnostic_trace_test.cpp) | **1** coherent request-budget case: window/sample caps, rearming, denial stopping stale windows, same/new request behavior, skipped slots and expiry. | Native conditions, UI tasks, capture/mount/calibration/projection or visuals. |

The 2026-10-22 standing-predicate fix ([`IsMovementDirection`](../src/animation/oar/policy.h)) ran one direct-compiled engine-free check: the predicate answers false for a standing and a drawn preview, the negated `NOT` arm is true (Smooth's non-combat gate), and unrelated predicates are unchanged. Its assertion also exists in [snapshot_transform_test.cpp](../tests/snapshot_transform_test.cpp), but that retained graph/math driver was not executed for this change — it needs the generated CommonLib headers. Release compilation and source review do not execute OAR conditions, selection or visuals.

To reproduce only the relevant driver, use the repository root in an x64 VS2022 developer shell initialized with MSVC 14.44.35207, and keep assertions enabled (no `NDEBUG`). Set the driver name to one table entry without `.cpp`:

```powershell
$characterPanelDriver = 'dac_event_match_test' # Or one of the other three table entries.
$characterPanelCheckDir = 'build/investigation-20261022'
New-Item -ItemType Directory -Force $characterPanelCheckDir | Out-Null
cl /nologo /std:c++latest /EHsc /W4 /WX /I src "tests/$characterPanelDriver.cpp" "/Fo$characterPanelCheckDir/$characterPanelDriver.obj" "/Fe$characterPanelCheckDir/$characterPanelDriver.exe"
& "./$characterPanelCheckDir/$characterPanelDriver.exe"
```

There is no CMake test target or automatic game execution. For current torch/shield/Smooth/axe diagnosis, follow the [section-9 symptom procedure](diagnostics/display-issues-2026-10-22.md#10-2026-10-23-装备上下文与火把分层修复待上机), [accepted mapping semantics](animation.md#accepted-mappings-and-world-snapshots) and [debugging navigation](code-maintenance.md#debugging-navigation). Detailed tracing defaults false, requires a restart when enabled, and has request-wide four-window/twenty-four-snapshot pose caps plus one fixed fifteen-bone source snapshot. Native runtime, visual equivalence and performance remain unverified.

After committing, configure and build again to embed that commit's identity; this does not rerun behavioral coverage. Preserved unrelated untracked local sources can truthfully leave the identity `+dirty` even when tracked changes are committed. Do not stage or remove unrelated files just to change that marker.

### Earlier attachment and idle-policy evidence

The earlier stock-binary adaptation verified matching and mismatched offline inspector cases described in [DAC verification](dac.md#verification-and-limits). The dated 2026-10-21 attachment change used the following two focused cases; these and the older idle-policy/IED/cache checks below remain separate historical verification entries, not the current four-driver list.

From the repository root in an x64 VS2022 developer shell initialized with MSVC 14.44.35207:

```powershell
New-Item -ItemType Directory -Force build/dac-script-attachments | Out-Null
cl /nologo /std:c++latest /EHsc /W4 /WX /I src tests/dac_script_attachment_test.cpp /Fobuild/dac-script-attachments/dac_script_attachment_test.obj /Febuild/dac-script-attachments/dac_script_attachment_test.exe
./build/dac-script-attachments/dac_script_attachment_test.exe
```

Keep assertions enabled; do not add `NDEBUG`. The [attachment cases](../tests/dac_script_attachment_test.cpp) compile the production [scanner/cache/decision](../src/animation/dac/script_attachment.cpp) in the test translation unit without Skyrim or another CMake target. Fixture files stay under the ignored output directory. A script-free two-effect marker with absent/empty VMAD is admitted despite a simulated VM handle; a scripted overriding MGEF is rejected before simulated dispatch without a running script, even after the base file was cached. These were the two cases selected for that attachment change. The normal Release configure/build above separately compiles the SDK adapter into the DLL.

Runtime inspection supports standard uncompressed MGEF records and VMAD version 5/object formats 1 or 2. Compressed target records, unsupported or malformed metadata and unavailable files reject affected events. Actual `GetFile()` provenance, SDK master/full/light remapping and virtual filesystem visibility require game verification. Offline tests do not execute the native DAC handler, paused effect activation or restoration; see [attachment semantics](dac.md#marker-script-attachment-preflight) and the [historical follow-up summary](history-animation.md#dac-ied). Original test sheets are recoverable through [the source index](history-sources.md).

The earlier equipment idle policy has two direct-compiled behavior cases, without adding any CMake test/project target. Its separate reproduction command is:

```powershell
New-Item -ItemType Directory -Force build | Out-Null
cl /nologo /std:c++latest /EHsc /W4 /WX /I src tests/animation_controller_test.cpp /Fo:build/animation_controller_test.obj /Fe:build/animation_controller_test.exe
./build/animation_controller_test.exe
```

These exercise production INI value parsing/idle targets, current preparation progress, marker-loss invalidation, immutable entry restoration and stale request/close rejection. They do not execute native DAC/OAR or Skyrim effect creation. The separate [offline original-binary inspector](dac.md#dependencies-and-offline-inspection) checks paired symbols without building, modifying or executing DAC. Run `cmake --preset Release` and `cmake --build --preset Release` for the DLL separately. If a reused cache has `VCPKG_MANIFEST_INSTALL=OFF`, install the manifest's packages with the existing vcpkg root first (for example `vcpkg install --triplet x64-windows-static-md --x-install-root=build/vcpkg_installed`), or reenable normal manifest installation. Preserve the baseline and installed triplet.

Prior local native evidence on 2026-10-19:

- Before test-target removal, production, standalone animation and math targets built, and the two selected animation behavior cases passed.
- After consolidation, the Release preset configured/built the DLL. Its SKSE load/query/version exports, matching PDB, embedded identity, Release-only graph and required CommonLib/CMake utility projects were checked.
- Documentation/source checks cover links, history parity and unchanged inputs; they are separate from native execution and runtime validation.

The [animation cases](../tests/animation_controller_test.cpp) use the direct command above; retained [graph/math](../tests/snapshot_transform_test.cpp) checks remain outside CMake. Earlier test results do not imply that current CMake builds either source. Those checks were offline-only; later AE 1.6.1170/MO2 evidence is recorded in [r4](diagnostics/display-issues-2026-10-22.md#r4-session) and [G1/G2](independent-behavior-graph.md#current-status). Native loading, hooks, visuals, framing and graph lifetime still have unverified boundaries. See [debugging navigation](code-maintenance.md#debugging-navigation) and [SceneGraph](scene-graph.md).

### IED condition verification

The native IED adapter has a separate two-case verification command in the same x64 MSVC 14.44.35207 developer shell, with assertions enabled:

```powershell
New-Item -ItemType Directory -Force build/ied-condition-adapter | Out-Null
cl /nologo /std:c++latest /EHsc /W4 /WX /I src tests/ied_condition_adapter_test.cpp /Fobuild/ied-condition-adapter/ied_condition_adapter_test.obj /Febuild/ied-condition-adapter/ied_condition_adapter_test.exe
./build/ied-condition-adapter/ied_condition_adapter_test.exe
```

The [cases](../tests/ied_condition_adapter_test.cpp) exercise the production [admission/guarded native dispatch](../src/animation/oar/ied_conditions.h): the representative native-preservation scenario covers equipped-placement false, left/right comparison, negation and disabled behavior, plus the zero-component SDS shield-setting false and negation; a changed or unproven historical requested-hand scope rejects before any native read. These remain two cases. They use narrow provider fakes and do not execute Skyrim, IED, SDS or the supplied DLL. The normal Release configure/build separately compiles the real OAR/SDK integration; no CMake test target or dependency is added. The [three-condition reference](oar-hooks.md#native-ied-placement-conditions-2026-10-21) records exact provider/schema versions, provider-side SDS prerequisites and archive/DLL provenance. Real OAR/SDS registration, paused IED hint freshness, native evaluation, missing-provider fallback and visuals remain game-only follow-up.

### Fixed base index verification

In the same x64 MSVC 14.44.35207 developer shell, with assertions enabled:

```powershell
New-Item -ItemType Directory -Force build/fixed-base-index-cache | Out-Null
cl /nologo /std:c++latest /EHsc /W4 /WX /I src tests/base_animation_cache_test.cpp /Fobuild/fixed-base-index-cache/base_animation_cache_test.obj /Febuild/fixed-base-index-cache/base_animation_cache_test.exe
./build/fixed-base-index-cache/base_animation_cache_test.exe
```

Exactly two [shared cases](../tests/base_animation_cache_cases.h) compile the production sparse resolver/cache: finite originals amid 15,000 replacements with alias/exact/left-primary precedence and reuse across menu sessions; project/table/count/load invalidation with missing originals, standing fallback availability and actual selected-index names. The fixture supplies author metadata for the existing hash helper. No new CMake target/dependency is added. The [retained graph/math driver](../tests/snapshot_transform_test.cpp) now shares these definitions instead of obsolete whole-catalogue APIs; its unrelated cases were not run. Normal Release configure/build checks real SDK/OAR integration separately. [Preparation timing/readiness](animation.md#fixed-base-indices-and-project-readiness), native loader/hook behavior, game lifetime and actual hitch reduction remain unverified without Skyrim; DAC preflight, Clone and first asset/GPU use are unchanged.

## Safe modification

Keep target/preset changes synchronized, preserve required dependencies/generation and inspect the actual generated solution after removing entry points. Old generated project files are not supported routes. Do not delete archives or unrelated build-directory contents as a substitute for checking the current target graph.

<a id="session-20261030-builds"></a>
## 2026-10-30/31 session builds (local artifacts)

Seven Release builds were produced in one session. Every configure used `cmake --preset Release -DCHARACTER_PANEL_BUILD_LABEL=<label>` and every build used `cmake --build --preset Release` with the proxy variables removed from the child environment — `http_proxy` and `HTTP_PROXY` together make MSBuild abort with `MSB6001` (duplicate `http_proxy` key). Each pair is archived under `build/<label>/` with its own `BUILD.txt`; each deployment backed the previous DLL/PDB/INI up under `dist/`.

| Label | Identity suffix | DLL bytes / SHA-256 | PDB bytes / SHA-256 | Run |
| --- | --- | --- | --- | --- |
| v46-retained-copy-ledger-20261030 | `-c0f2d81953-cl94faaed0c6-20261030T150024Z` | 1,334,784 / `70b3560a8d5f7cd3b534b5f784be4729765cfc7cd5b0c10a741ff9c626028c38` | 36,442,112 / `9eddefaffb676be8ee1a8ef46a7c7ab9456368507a28a35c002de224ec826683` | v46 first pass |
| v47-candidate-census-20261030 | `…-20261030T154738Z` | 1,367,040 / `aca4046f6247e5cd7f32769bcca8218a8462ec1440c57d7b56e057759c1a5c7e` | 36,950,016 / `89581d204d053b785350caa4809fcf356b717c339ae5a234749400c65c907004` | C-VC1 load |
| v48-census-post-activate-20261030 | `…-20261030T155732Z` | 1,367,552 / `06fabbe7ae9123bdd9af79dac66058971f7d5e29afa12499f99cd9477bcf3f2f` | 37,064,704 / `c1739368d369b31e16db293fd78b717ed538176c6aa10fcf1888112eb1ab6284` | C-VC1 post-activate |
| v49-candidate-input-20261031 | `…-20261030T160940Z` | 1,374,720 / `8f516a9397b7ea152f2317f65dda500b539b38d45054f703bf954765991a7cbb` | 37,122,048 / `f07cb9725f98e093adf7473dbc96516fcbc54569edd282ffec53a1d5459101ab` | S1 (invalid run) |
| v50-generation-entry-20261031 | `…-20261030T161647Z` | 1,375,232 / `f8a9a4c67cdd291483db2c2056884f724d55bb0def52713ce40e9a1a4a8c0dcb` | 37,122,048 / `3690c3d99a474ad66065360429b6bc34edb91c3f81d2354183e16dfdcc0930b4` | S1 re-run |
| v51-input-slice2-20261031 | `…-20261030T162411Z` | 1,379,328 / `5090a48470a3db1ad8900966c0a691624ed26a8bceb4fe22c3d272205715a73b` | 37,163,008 / `b456ca714d1e56e1b871f99b40420ece3da991f7138167ed2ae78d0914cb14a0` | S2 |
| v52-event-path-20261031 | `…-20261030T162931Z` | 1,380,864 / `2ee8db3d5ed0eed0f55b37347cab74733da9cfe4c22f8e3387680db49776c466` | 37,179,392 / `315eb10fb2fb98bf98432fdaacf59f6eeacd5f8dc9b7ade34e1eec3584026a53` | event path |

All identities carry `CharacterPanel-scenegraph-behavior-graph-` and `+dirty` (uncommitted session work). `PLUGIN_SOURCES` stayed at 30 units throughout — C-VC1 and the input slice added headers, call sites and settings keys only. v50–v52 and the INI-only combination round reused the binaries above with only the deployed INI changing. The recorded installation at the end of the session is v52 with `IndependentGraph=inject`, `CandidateInputProbe=true`, `CandidateInputVariable=iRightHandEquipped`, `DisplayPose/DisplayStep=false`. Session results: [summary](independent-behavior-graph.md#session-20261030).

<a id="v53-unequip-event-2026-10-31"></a>
## V53 unequip-event slice build (2026-10-31)

The [C-G3B-S3 implementation](independent-behavior-graph.md#c-g3b-s3-unequip-event) was built from committed HEAD `ecb5869` (clean identity, no `+dirty`). Offline first: the new [unequip settle policy](../src/animation/behavior_graph/unequip_settle_policy.h) passed three engine-free cases in [behavior_graph_unequip_settle_test.cpp](../tests/behavior_graph_unequip_settle_test.cpp) (settled tail opens the injection; disturbance restarts the stability run and the baseline follows the newest triple; no-settle when the target never holds or the tail drifts off it), compiled with `cl /nologo /std:c++latest /EHsc /W4 /WX /I src` and exiting 0 (artifacts under `build/unequip-settle-20261031/`). One header comment was rewritten from an em-dash to ASCII after MSVC C4819 rejected it under code page 936. Configure/build used the session recipe above with the proxy variables removed; the only build warnings were the pre-existing D9025 `/Ob2`→`/Ob3` overrides (one per unit), and configure printed the existing `CMAKE_SKIP_INSTALL_RULES` note. `PLUGIN_SOURCES` stayed at 30 units.

Build identity `CharacterPanel-scenegraph-behavior-graph-v53-unequip-event-20261031-ecb5869efd-cl94faaed0c6-20261031T074124Z`; DLL SHA-256 `5f4559e18ffa85b7dfc531e86e3525b86dd432efe2c248e7bf64fc54a6a8dfb8` (1,403,392 bytes), PDB SHA-256 `01276fbd08c6078c9a3ee30470e84ccfc9112d94cc6a3b8fe966fba2599ddea7` (37,564,416 bytes). Deployment (2026-10-31) backed the on-disk v52 trio up to `dist/deploy-backup-20261031-v52/` (DLL `2ee8db3d…`, PDB `315eb10f…`, INI 2,182 bytes `a26731ef…`, all matching the session record) and installed this DLL/PDB under `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\` (deployed hashes equal the build). The deployed INI (1,778 bytes, SHA-256 `0d0fea7a…`) sets `IndependentGraph=inject`, `CandidateUnequipProbe=true`, `CandidateInputProbe=true` with `CandidateInputVariable=` **empty** (observation only — the v52 event-only shape, so no variable is written), `CandidateGenerationEntry=true`, and every other experiment key off. The repository sample stays default-off. Settings load at DataLoaded, so a full restart is required; no game run has happened yet.

<a id="v54-event-capture-2026-10-31"></a>
## V54 event-capture build (2026-10-31)

The C-G3B-E1 implementation was built from committed HEAD `995d323` (clean identity). Two verification steps preceded it. First, the [unpacked 1.6.1170 image](behavior-graph-construction-audit.md#方法与校验) — hash-checked `843823c1…` against the audit record; the shipped exe's `.text` reads as encrypted garbage, consistent with the audit's Steamless methodology — yields the 58377 head bytes `48 8b c4 48 89 48 08` (`mov rax,rsp; mov [rax+8],rcx`), both RIP-independent with an instruction boundary at +7, so a 5-byte jmp plus two pads fits without splitting anything; the stub head `48 83 ec 48 48 83 7a 08 00` also matches the audit's `0xab1f30` reading. Second, the new [event-capture policy](../src/animation/behavior_graph/source_event_capture_policy.h) passed three engine-free cases in [source_event_capture_test.cpp](../tests/source_event_capture_test.cpp) under `cl /W4 /WX` (ring order/overflow/drain-discard; independent 600/200 budgets closing the window; cancel and expiry beating open budgets). The overflow case pinned a real design point: the ring drops the **newest** records when full, because a producer must never write the consumer's tail index — per-frame draining makes that path unreachable at game event rates, and the dropped counter is the loud failure signal. `PLUGIN_SOURCES` went 30→31 (`source_event_capture.cpp`); the only build warnings were the pre-existing D9025 overrides.

Build identity `CharacterPanel-scenegraph-behavior-graph-v54-event-capture-20261031-995d32347d-cl94faaed0c6-20261031T081640Z`; DLL SHA-256 `c7e31b78db03465372115d792eebf0f745959620268ba21ba37f91a2b0850ae9` (1,423,360 bytes), PDB SHA-256 `8b4d0e232950361c66ec6ca562813477d873f008b45392c58edde461dd6a7c29` (37,736,448 bytes). Deployment (2026-10-31) backed the deployed v53 trio up to `dist/deploy-backup-20261031-v53/` (DLL `5f4559e1…`, PDB `01276fbd…`, INI 1,585 bytes `1bcd95bc…`, all matching the S3 record) and installed this DLL/PDB. The deployed INI (1,711 bytes, SHA-256 `c75e0c88…`) sets `SourceEventCapture=true` with `IndependentGraph=off` and `SourceTimeline=false` and every other experiment key off, so the detour installs at startup and F7 opens the capture window. The install self-verifies at runtime: a head mismatch logs the observed 16 bytes and leaves the process unpatched. Repository sample stays default-off; full restart required. Run sheet: F7 outside any menu → equip one 1H weapon → ~2 s → unequip → ~2 s → the window closes itself; no game run yet.

<a id="v55-unequip-variable-2026-10-31"></a>
## V55 unequip-variable build (2026-10-31)

The [C-G3B-S4 ladder](independent-behavior-graph.md#c-g3b-s4-unequip-variable) reuses the S3 machinery unchanged — the only code change is a branch in the settled second stage: a non-empty `CandidateUnequipVariable` swaps the `1HM_Unequip` dispatch for one typed by-name variable write (`SetGraphVariableBool(true)` for BOOL, `SetGraphVariableInt(1)` for INT8/16/32, other types logged and skipped), with the setter result and read-back logged. No new policy logic, so no new offline cases; one C4456 shadowing rename during the build. `PLUGIN_SOURCES` stays at 31.

Build identity `CharacterPanel-scenegraph-behavior-graph-v55-unequip-variable-20261031-9f1bbb9e59-cl94faaed0c6-20261031T083907Z` (committed HEAD `9f1bbb9`); DLL SHA-256 `119171e29d8a5ad167b32429f26179359a3fb3225588c4386d71b5f1ad98ed66` (1,427,456 bytes), PDB SHA-256 `d542db853bff4a379583530dd98e9c9804f5582fb0b406fd2d9dc58d539aa5e6` (37,842,944 bytes); only the pre-existing D9025 warnings. Deployment (2026-10-31) backed the v54 trio up to `dist/deploy-backup-20261031-v54/` (DLL `c7e31b78…`, PDB `8b4d0e23…`, INI 1,633 bytes `aa49d0f7…`) and installed this pair. The deployed INI (1,676 bytes, SHA-256 `9c8e464a…`) is S4 round 1: `IndependentGraph=inject`, `CandidateUnequipProbe=true`, `CandidateUnequipVariable=bEquipUnequip`, everything else off. Repository sample keeps the new key empty (event path). Round 2 (`IsUnequipping`) and round 3 (`PitchManualOverride`) are INI-only re-runs of this same binary. No game run yet.

<a id="v56-write-capture-2026-10-31"></a>
## V56 write-capture build (2026-10-31, local only)

The [C-G3B-E2 setter-write observer](independent-behavior-graph.md#c-g3b-e2-write-capture) was built from committed HEAD `a185603` (clean identity) but is **not deployed** — the S4 ladder owns the deployment slot until its rounds conclude. Two verification steps preceded it. The address library `versionlib-1-6-1170-0.bin` (format 2, 428,461 entries, parse cross-checked against E1's 58377→`0xab58a0`) resolves `SetGraphVariableBool` to `0x140bb6600` (REL 63607) and `SetGraphVariableInt` to `0x140bb6790` (REL 63609); both entries in the unpacked image begin with `48 89 5c 24 10` (`mov [rsp+10h],rbx`, five bytes, RIP-independent, boundary exactly at +5), so each detour relocations exactly one instruction with no padding. The new [write-capture policy](../src/animation/behavior_graph/source_write_capture_policy.h) passed three engine-free cases in [source_write_capture_test.cpp](../tests/source_write_capture_test.cpp) under `cl /W4 /WX` (name copy at push time with 63-byte truncation and null degradation; gate array matching with clamped counts; the 300/200 budgets closing the window). `PLUGIN_SOURCES` went 31→32 (`source_write_capture.cpp`); only the pre-existing D9025 warnings.

Build identity `CharacterPanel-scenegraph-behavior-graph-v56-write-capture-20261031-a185603f5a-cl94faaed0c6-20261031T085732Z`; DLL SHA-256 `868e6c35bdfd57224a56227ae48f94794d864194de78a40d906e1caa938b8781` (1,444,352 bytes), PDB SHA-256 `68bebe2e109f4ddb12c1cf237fd14ba4c4869a72e76395d61895b5e8384538e9` (37,990,400 bytes). Repository sample keeps the new key off. **No deployment and no game result** — deployment happens after the S4 ladder concludes, with the deployed INI at `SourceWriteCapture=true` + everything else off.

<a id="v57-two-write-2026-10-31"></a>
## V57 two-write build (2026-10-31)

The [C-G3B-S5 two-write replay](independent-behavior-graph.md#c-g3b-s5-two-write) was built from committed HEAD `227d510` (clean identity). The only code change: the S4 typed-write block was refactored into a shared `typed_write` lambda (name-missing/unsupported-type rows now carry a `stage=` marker), and the settled second stage gained an optional prime write before the command write — `CandidateUnequipPrime` non-empty writes that variable first, either write failing ends the slice without a sampling window, and an empty prime keeps the S4 single-write behaviour while an empty `CandidateUnequipVariable` keeps the S3 event path. No new policy logic, so no new offline cases. `PLUGIN_SOURCES` stays at 32; only the pre-existing D9025 warnings.

Build identity `CharacterPanel-scenegraph-behavior-graph-v57-two-write-20261031-227d5104a3-cl94faaed0c6-20261031T094750Z`; DLL SHA-256 `250abfb18d6c2c565144091c0c66a4659ecbe5b996b596b00d2c88921485f9c0` (1,445,888 bytes), PDB SHA-256 `3c15abb0e9602f7eb9a901c29e3b4e6ce01b8199c5aaab5fb9ce92cfbcca5325` (37,990,400 bytes). Deployment (2026-10-31) backed the v56 trio up to `dist/deploy-backup-20261031-v56/` (DLL `868e6c35…`, PDB `68bebe2e…`, INI 1,683 bytes `609f74fc…`) and installed this pair. The deployed INI (1,817 bytes, SHA-256 `4ee969a2…`) is the S5 configuration: `IndependentGraph=inject`, `CandidateUnequipProbe=true`, `CandidateUnequipPrime=iRightHandEquipped`, `CandidateUnequipVariable=bEquipUnequip`, everything else off. Repository sample keeps the new key empty. No game run yet.

<a id="v58-transient-watch-2026-10-31"></a>
## V58 transient-watch build (2026-10-31)

The [C-G3B-E3 transient-write watcher](independent-behavior-graph.md#c-g3b-e3-transient-watch) was built from committed HEAD `65c069e` (clean identity) after the S5 ladder closed. The change is contained in the source timeline: a `SourceTransientWatch` mode key resolves each index's `m_type` at baseline and, for BOOL/INT8/16/32, logs every observed word change immediately (`mode=transient` rows, including reverts) while REAL/quad keep settle-3; the window-open row carries `mode=transient|settled`. No new policy logic, so no new offline cases. `PLUGIN_SOURCES` stays at 32; only the pre-existing D9025 warnings.

Build identity `CharacterPanel-scenegraph-behavior-graph-v58-transient-watch-20261031-65c069e1ba-cl94faaed0c6-20261031T095536Z`; DLL SHA-256 `75b1390237caaab1aed2519c3cea2007ecdbdc8fcd1d46cf83c715a6962c2f5c` (1,448,960 bytes), PDB SHA-256 `5ce2f421244005537f0f2bc6ea9250c85d1e57cececfde3bb04ca6d522087d7c`. Deployment (2026-10-31) backed the v57 trio up to `dist/deploy-backup-20261031-v57/` and installed this pair with the deployed INI (1,788 bytes, SHA-256 `cc92052e…`) at `SourceTimeline=true` + `SourceTransientWatch=true` + everything else off. Repository sample keeps the new key off. No game run yet.

<a id="v59-unequip-event-name-2026-10-31"></a>
## V59 unequip-event-name build (2026-10-31)

The [C-G3B-S6 unequip-event retry](independent-behavior-graph.md#c-g3b-s6-unequip-event-retry) was built from committed HEAD `dcf05ca` (clean identity) after the [C-G3B-A1 structural audit](independent-behavior-graph.md#c-g3b-a1-structural-audit) resolved the equipped-idle 1→12 edge to the event `Unequip` (id 144, condition flag disabled). The only code change: a new `[Experiment] CandidateUnequipEvent` string key (settings parse/log/accessor, same shape as the existing experiment keys) selects the settle-gated second-stage event name in the S3 event branch — non-empty dispatches that name, empty keeps the `1HM_Unequip` literal so historical INI files reproduce their rounds verbatim. No new policy logic, so no new offline cases. `PLUGIN_SOURCES` stays at 32; only the pre-existing D9025 warnings.

Build identity `CharacterPanel-scenegraph-behavior-graph-v59-unequip-event-name-20261031-dcf05cab1e-cl94faaed0c6-20261031T131725Z`; DLL SHA-256 `013e1bdcfb156b25c6d25bd58d8b15f442c7499329e0eaae39399d34eaa983bf` (1,448,960 bytes), PDB SHA-256 `8b8fd16f1c29202017a9ac9eb47d49cec437b17fd024c280a8e07ee09b1cbec7` (38,031,360 bytes). Deployment (2026-10-31) backed the v58 trio up to `dist/deploy-backup-20261031-v58/` (DLL `75b13902…`, PDB `5ce2f421…`, INI 1,795 bytes `86f8bed1…`) and installed this pair. The deployed INI (1,932 bytes, SHA-256 `88763dd68cba8c831cd0ca2e8e7bbad5929b695e41f437aa02412420a7e6b0a6`) is the S6 configuration: `IndependentGraph=inject`, `CandidateUnequipProbe=true`, `CandidateUnequipEvent=Unequip`, `CandidateUnequipVariable`/`CandidateUnequipPrime` empty, everything else off. Repository sample keeps the new key empty. First game run (2026-10-31 21:23, identity confirmed in the live log): **the S6 slice hit — see the [S6 run record](independent-behavior-graph.md#c-g3b-s6-unequip-event-retry)**. After adjudication the deployed INI returned to dormant off (1,834 bytes, SHA-256 `1d40a0eb84e04c2df98bc5ea2989ec721c34b15efff3894e5cc680ffc0361db7`) with the v59 DLL/PDB left installed.

## Synchronized files

Maintain this guide with [README](../README.md), [SceneGraph](scene-graph.md), [animation](animation.md), [maintenance](code-maintenance.md), CMake/preset/template inputs and [runtime notes](../tools/scene_copy/README.txt). Keep current build commands out of [historical evidence](history.md), whose commands describe earlier revisions.

## Related history

The [historical index](history.md) preserves older probe, test and package records. The current single-DLL setup supersedes their build entry points without changing their recorded outcomes.

The same v59 binary also carries the [C-G3B-F full-replay acceptance](independent-behavior-graph.md#c-g3b-f-full-replay) run (2026-10-31): the dormant INI was backed up to `dist/deploy-backup-20261031-v59/` (1,834 bytes, `1d40a0eb…`) and the deployed INI switched to the F configuration (1,921 bytes, SHA-256 `6dfb54ec28e4086747d18017a6970eb3a4f85778fc33d34b15bac131e0ace9ba`) — `inject`, `CandidateInputProbe=true` with an **empty** `CandidateInputVariable` (observe-only six-point sampling), plus the S6 keys. DLL/PDB unchanged; no rebuild. First game run (2026-10-31 21:38): **the F acceptance hit — G3-B PASSED, see the [F run record](independent-behavior-graph.md#c-g3b-f-full-replay)**. After adjudication the deployed INI returned to dormant off (2,013 bytes, SHA-256 `65e574012f808d7582c0c8327fbcf4e53d7db8f1cd13793c4f3a449733c4543d`) with the v59 DLL/PDB left installed.

<a id="v60-g4-oar-context-2026-10-31"></a>
## v60: C-G4-A instrument (2026-10-31)

> **2026-11-02 计数更正**：下方 `PLUGIN_SOURCES 33` 为旧记录误差；当前同基线 CMake 实数 32，不改工件身份，也不据此宣称重建。

Build identity `CharacterPanel-scenegraph-behavior-graph-v60-g4-oar-context-20261031-fdbd414f45-cl94faaed0c6-20261031T143920Z` (clean tree at `fdbd414f45`; two earlier same-label builds were superseded in-session before any run — one from the uncommitted tree carried `+dirty`, one at `d2fbc4bb56` predated the tally arm/take hardening in [candidate_window.h](../src/animation/behavior_graph/candidate_window.h) — and neither was deployed); DLL SHA-256 `b519b21d62ea90045606cd5790226b15f0c9380e25cff50ea3260fa9a83b4800` (1,456,128 bytes), PDB SHA-256 `d4aa2f6e9904aed592d984460671a9443f664b8b49534370149e4b15340aee8d` (38,187,008 bytes). Source set unchanged (`PLUGIN_SOURCES` 33; the new `candidate_window.h` is header-only and picked up by the existing header glob). The engine-free `behavior_graph_display_step_test` (compiles the touched `drive_step_policy.h`) passed under g++ C++23 `-Wall -Wextra -Werror` ("2 display-step regression cases passed").

Deployment (2026-10-31) backed the v59 trio up to `dist/deploy-backup-20261031-v60/` (DLL `013e1bdc…`, PDB `8b8fd16f…`, dormant INI 2,013 bytes `65e57401…`) and installed the v60 pair. The deployed dormant INI was rebuilt (2,147 bytes, SHA-256 `27f481f5…`): it fixes the duplicated `CandidateUnequipEvent` key the S6+F deployments had stacked (same value, no behavioral effect) and adds `OarContextWatch=false`; all 20 keys were printed back and asserted after writing. Repository sample keeps the new key false. No game run yet — the [C-G4 contract](independent-behavior-graph.md#c-g4-context-isolation) awaits the user's authorized session.

Run-1 switch (2026-10-31, user-requested): the dormant INI was backed up to `dist/deploy-backup-20261031-v60/deployed-dormant-27f481f5.ini` and the deployed INI switched to the C-G4-A run configuration (2,189 bytes, SHA-256 `add1caf9…`) — `inject`, `DisplayPose=true`, `DisplayStep=true`, S1 observe-only two keys, S6 unequip two keys (`CandidateUnequipEvent=Unequip`), `OarContextWatch=true`, `DetailedAnimationTrace=true`, everything else dormant; all 20 keys asserted after writing. DLL/PDB unchanged. The next game session runs experiment A per the [C-G4 contract](independent-behavior-graph.md#c-g4-context-isolation).

First game run (2026-10-31 22:58, identity confirmed in the live log): **G4-A unresolved — see the [run record](independent-behavior-graph.md#c-g4-context-isolation)**. The replay determinism criteria reproduced S6/F bit-for-bit, but the experiment window recorded 2,037 OAR evaluations with null REFR (overlaps=0, display-window evaluations=0) — the C-G4-A2 slice preregistered there localizes the entry point with clip-generator→graph identity. After adjudication the deployed INI returned to dormant off (2,147 bytes, SHA-256 `27f481f5…`, restored from the backup and re-asserted) with the v60 DLL/PDB left installed.

<a id="v61-g4a2-clip-identity-2026-10-31"></a>
## v61: C-G4-A2 clip-identity instrument (2026-10-31)

> **2026-11-02 仪器语义更正**：下方保留当时记录；numeric site 命中只表示 numeric 访问器被调用，只有 type3 才属这里的图变量访问，site 总数不能推出每次读值类型。详见[全窗类型待办](independent-behavior-graph.md#g4-followup-numeric)。

Build identity `CharacterPanel-scenegraph-behavior-graph-v61-g4a2-clip-identity-20261031-57fa4affab-cl94faaed0c6-20261031T152526Z` (clean tree at `57fa4affab`); DLL SHA-256 `d1583a7e3d3e4e587ec6874fd12b6698197ee7047a8269a57f3603221cb82a26` (1,460,736 bytes), PDB SHA-256 `f41d5e7469ec8c00a763e64529f69006b8c2e84d6ec4dc3edb65b7d2fe3d707b` (38,277,120 bytes). A2 adds per-detour-site tallies (a numeric hit means a graph variable was fetched), clip-owner classification of every hit against the candidate's and the source's `hkbBehaviorGraph::activeNodes` (audited NodeList layout: entries@+0, count@+8, 0x90-byte entries, node@+0x58), clip `animationName` in hit rows, and four cumulative segment snapshots (post-activate / post-weapequip / at-settle / post-unequip). Hit-row cap raised 8→32. All gated on the existing `OarContextWatch` key. The engine-free `behavior_graph_display_step_test` passed (`-Wall -Wextra -Werror`).

Deployment (2026-10-31) backed the v60 trio up to `dist/deploy-backup-20261031-v61/` (DLL `b519b21d…`, PDB `d4aa2f6e…`, dormant INI `27f481f5…`) and installed the v61 pair. The deployed INI switched to the A2 run configuration (2,151 bytes, SHA-256 `60ff8f27…` — identical key set to the C-G4-A run 1 config; the key-by-key assert caught a dormant-default residue `CandidateInputVariable=IsEquipping` that the derivation missed, fixed to empty before finalizing). No game run yet — the A2 session repeats the run-1 procedure verbatim.

<a id="v62-g4a3-scan-anchor-2026-10-31"></a>
## v62: C-G4-A3 scan-anchor micro-instrument (2026-10-31)

> **2026-11-02 前向更正**：下方 G4-A/G4 PASSED 保留为历史裁定。“every numeric hit/type0/zero graph-variable reads”实际只有 7 条 numeric 类型明细（全窗总数 72），A 全窗覆盖 PENDING；B+C 源事件观察器关闭且安装门不兼容 inject，C 源事件泄漏证据 PENDING。B 仅支持装备族稳定。当前状态/两项后续见[证据交接](independent-behavior-graph.md#handoff-20261102-evidence)，本次不实现/构建/部署。

Build identity `CharacterPanel-scenegraph-behavior-graph-v62-g4a3-scan-anchor-20261031-2dbe98c785-cl94faaed0c6-20261031T154250Z` (clean tree at `2dbe98c785`; two earlier same-label builds carried `+dirty` from uncommitted run-record docs and were discarded); DLL SHA-256 `cdb8a5ed72f7132d93f4cef8f41bdddf4664814f508a9e9ff4aeac88a0bc2060` (1,461,760 bytes), PDB SHA-256 `a70200453b42ee5a4b450655d8012ed32150896fb749cc011ffb7e9cd6d058d9` (38,383,616 bytes). A3 adds: the raw numeric type value in hit rows (type 3 = graph-variable accessor — the A2 run's 72 nameless numeric hits were ambiguous), an `OAR CONTEXT scan` row at candidate-list arm time (entries/count of both NodeLists, anchored against the DriveState `active-nodes=…/N` readings), and the live candidate active count in every hit row (interpreting the neither bucket). Engine-free `behavior_graph_display_step_test` passed.

Deployment (2026-10-31) backed the v61 pair up to `dist/deploy-backup-20261031-v62/` and installed the v62 pair. The deployed INI stays the A2/A3 run configuration (`60ff8f27…`) — A3 uses the identical key set and procedure.

First v62 game run (2026-10-31 23:46, identity confirmed in the live log): **G4-A closed — see the [A3 run record](independent-behavior-graph.md#c-g4-context-isolation)**. The scan anchor read live NodeLists (cand 97 / src 106, matching the DriveState counts), every numeric hit reported `type=0` (not the graph-variable type 3 — zero graph-variable reads), and all 2,043 evaluations classified neither (template-level clip data, consistent across the three runs). G4-A criterion 1 closes under the revised interpretation; B/C are unblocked. After adjudication the deployed INI returned to dormant off (`27f481f5…`, restored from the v61 backup and re-asserted) with the v62 DLL/PDB left installed.

Run B + C-segment game run (2026-11-01 00:28, config ② on the v62 binary, identity confirmed in the live log): **G4 PASSED — see the [run record](independent-behavior-graph.md#c-g4-context-isolation)**. The replay reproduced deterministically a fourth time, the A-segment isolation profile reproduced under replay+display coexistence (experiment window 2,037 evaluations / overlaps 0 / owner neither:2037; all display windows zero), the world-selection round matched the A/A baseline per family, and the C-segment evidence (5 accepted captures, retained-total 2→4 within the intentional-retention rule, equipment revisions 10→14, DAC restore matched ×3) closed the cleanup criteria. After adjudication the deployed INI returned to dormant off (`27f481f5…`) with the v62 DLL/PDB left installed.

<a id="v63-g4a4-numeric-types-2026-11-02"></a>
## v63: C-G4-A4 numeric-type histogram (2026-11-02)

Build identity `CharacterPanel-scenegraph-v63-g4a4-numeric-types-e97eb65e68-cl94faaed0c6-20261102T142727Z` (clean tree at `e97eb65e68`, no `+dirty` suffix — the instrument commit landed before the build, so no superseded same-label artifact exists); DLL SHA-256 `2c81435c8d979a2af44de54132794a63dd000c3271ab37eccda01365d8cd84f2` (1,466,368 bytes), PDB SHA-256 `c8e361ec721ef83e1d3060d26e568c1e95040fc74b6a8527ec2f7e4d36ca3277` (38,432,768 bytes). `PLUGIN_SOURCES` stayed at 32 units — the change is header-only plus three emit sites, and the new regression test is not a plugin unit. A4 adds: the raw numeric-accessor type histogram counted for every armed hit before the `Oar_Context_Hit_Row_Cap = 32` detail budget (`numeric_types[0..3]`, `other`, `un`, `t3-rows`), the reconciliation figure `numeric=…/sum:N` on every `window=`/`segment=` row, bounded (8 per window) `OAR CONTEXT numeric-type3` attribution rows for type-3 hits past the budget, and one shared `clear_candidate_window_counts` for arm/take/foreign-take.

Build (2026-11-02 22:27 local): `cmake --preset Release -DCHARACTER_PANEL_BUILD_LABEL=v63-g4a4-numeric-types` then `cmake --build --preset Release`, both exited 0 with the proxy variables cleared from the child environment. Warnings: only the pre-existing D9025 `/Ob2`→`/Ob3` override notice per unit and the existing `CMAKE_SKIP_INSTALL_RULES` configure note; no new diagnostic. Offline first: [behavior_graph_candidate_window_test.cpp](../tests/behavior_graph_candidate_window_test.cpp) compiled with `cl /nologo /std:c++latest /EHsc /W4 /WX /utf-8 /I src` (artifacts under `build/behavior-graph-v63-g4a4-numeric-types/`), zero warnings, and ran "6 candidate-window numeric-type cases passed (engine-free)" with exit 0.

Deployment (2026-11-02): the dormant v62 trio was backed up to `dist/deploy-backup-20261102-v62/` (DLL `cdb8a5ed…`, PDB `a7020045…`, dormant INI `27f481f5…` 2,147 bytes — hashes re-asserted against the [v62 record](#v62-g4a3-scan-anchor-2026-10-31) before overwriting), then the v63 pair was installed and the deployed INI moved from dormant to the A4 run configuration (2,612 bytes, SHA-256 `7cb2562f…`). The key set is the A3 run's verbatim — `IndependentGraph=inject`, `DisplayPose=true`, `DisplayStep=true`, `CandidateInputProbe=true` with an empty variable, `CandidateUnequipProbe=true` with empty variable/prime and `CandidateUnequipEvent=Unequip`, `OarContextWatch=true`, `DetailedAnimationTrace=true`, every other instrument off — so the only delta inside the window is the type histogram. Post-deploy readback asserted all 20 keys (including the dormant-default residue `CandidateInputVariable=IsEquipping` fixed to empty before finalizing, anti-pattern 11/14). No game run yet.

First v63 game run (2026-11-02 22:32–22:37, identity confirmed in the live log, actor 00000014 request=1): **G4-A numeric coverage closed — see the [A4 run record](independent-behavior-graph.md#c-g4-a4-numeric-types)**. The full 2,037-evaluation experiment window reported `sites=e:1379/n:72/s:339/r:247` against `numeric=t0:72/t1:0/t2:0/t3:0/other:0/un:0/sum:72`, i.e. every one of the 72 numeric hits in the complete window is the non-graph type 0 — 65 of them counted beyond the 32-row detail budget (the rows still stop at exactly 32, 7 of them numeric). All 31 window/segment rows reconciled `sum == sites n:`, the 21 following display windows all read zero (no cross-window residue), the replay reproduced pose hashes a fifth time (`bc160a8c…/63f638a6…/c407cb4e…` and `8fb4cd33…/4f2aebde…/c407cb4e…`), and `source-drift=none` held with zero `[E]`. Log archived ([567,481 bytes](diagnostics/CharacterPanel-g4a4-numeric-types-20261102-2232.log), SHA-256 `59fa75ca…`). Deviation filed, not adjudicated: the display phase was 21 retarget/request-interrupted windows totalling 364 steps (~6 s) rather than A3's continuous single window, and `DISPLAY advance` still did not fire — no A4 criterion depends on either. After adjudication the deployed INI returned to dormant off (`27f481f5…`, 2,147 bytes, restored from the backup and re-asserted) with the v63 DLL/PDB left installed.

<a id="v64-g4c1-concurrent-events-2026-11-02"></a>
## v64: C-G4-C1 concurrent source-event observer (2026-11-02)

Build identity `CharacterPanel-scenegraph-v64-g4c1-concurrent-events-02c16e5b65-cl94faaed0c6-20261102T145517Z` (clean tree at `02c16e5b65`); DLL SHA-256 `ff00daad5bebed2b8a4497ce26b2af74d94db09255fdb19d9db4f6d50de3ef0d` (1,470,464 bytes), PDB SHA-256 `3364ea18eb931594f0482fa02ba4c2e1c7a555d38ab013d825debcfdc280a6f3` (38,481,920 bytes). `PLUGIN_SOURCES` stayed at 32 — no new unit. C1 turns `SourceEventCapture` into `off|on|concurrent`, keeps every E1 guard for `on`, and adds a second lifecycle that only replaces *who owns the window*: the experiment arms it synchronously (identity + baseline + gate) before its own dispatches, the first stepped display close / the 20-second expiry / a world reset closes it, and F7 no longer arms it. The detour gains two pre-ring counters: `calls` (every entry, before the gate) and `other-graph` (armed-window dispatches to a different graph). The close row now prints `calls/other-graph/ring-dropped` beside the source event lines, so one row carries both the liveness control and the leak signature.

Build (2026-11-02 22:55 local): `cmake --preset Release -DCHARACTER_PANEL_BUILD_LABEL=v64-g4c1-concurrent-events` then `cmake --build --preset Release`, both exit 0 with the proxy variables cleared; only the pre-existing D9025 `/Ob2`→`/Ob3` notices and the `CMAKE_SKIP_INSTALL_RULES` configure note. Offline: [source_event_capture_test.cpp](../tests/source_event_capture_test.cpp) extended with `mode_text_maps_to_three_states` (off/false/0/empty → off; on/TRUE/1 → on; concurrent/Concurrent → concurrent; `concurrentt`/`yes` → nullopt so a typo cannot arm the hook) alongside the existing ring/budget/cancel cases — `cl /nologo /std:c++latest /EHsc /W4 /WX /utf-8 /I src` compiles clean and the binary exits 0 (artifacts under `build/behavior-graph-v64-g4c1-concurrent-events/`).

Deployment (2026-11-02): the dormant v63 trio was backed up to `dist/deploy-backup-20261102-v63/` (DLL `2c81435c…`, PDB `c8e361ec…`, dormant INI `27f481f5…` 2,147 bytes), the v64 pair installed, and the deployed INI switched to the C1 run configuration (2,530 bytes, SHA-256 `f714e05f…`): the A4 key set verbatim plus `SourceEventCapture=concurrent`, all 20 keys read back. Post-install DLL string check confirms the v64 identity and the new `concurrent-armed` / `other-graph=` markers.

C1 run sheet (user-executed, one session): open the inventory panel once → the replay runs by itself (~3–4 s) → keep the panel open 2–3 s so display stepping happens → close the panel within ~15 s. Expect `SOURCE EVENTCAPTURE installed … mode=concurrent`, `concurrent-armed` + `window-open`, exactly two `result=dispatched` rows after `window-open`, and `window-closed reason=display-close calls=… other-graph=… events=… ring-dropped=0` with `calls ≥ 2`, `other-graph ≥ 2` and `events=0`. A `reason=expired` close only carries the injection-phase reading (see the contract's criterion 4 branch).

<a id="v65-g4c1-resolver-fix-2026-11-02"></a>
## v65: C-G4-C1 arm-resolver fix and mismatch diagnostics (2026-11-02)

The first v64 attempt was invalid: the concurrent window was armed with the experiment's graph, while the sampler re-resolves the source every frame with its own resolver. With more than one non-1stperson graph the two resolvers return different objects, so the identity rule aborted the window on the first sampler pass (`source-lost-or-replaced`) — the rule working as designed over a wrong resolver on the arm side. Evidence: [attempt 1](diagnostics/CharacterPanel-g4c1-attempt1-source-identity-20261102-2321.log) (137,183 bytes, SHA-256 `f296c348…`), whose replay still reproduced deterministically.

Build identity `CharacterPanel-scenegraph-v65-g4c1-resolver-fix-adc2acb18a-cl94faaed0c6-20261102T152612Z` (clean tree at `adc2acb18a`); DLL SHA-256 `1b3d60ad3c92701d4b9c7ee116bd951af86ca31211901bc3c8ebc951903e0716` (1,474,048 bytes), PDB SHA-256 `22ef7059bdcfe089e63cd338fe25378a96bd8ae37f36752a139c29b27a75c9ff` (38,522,880 bytes). Changes: arm resolves through `player_graph(*holder)` (the sampler's function) and prints `graph-match=yes|no`; `window-open` adds the graph/behavior/data/set/words pointers; a new `identity-mismatch` row compares prev and now field by field; `window-aborted` now carries `calls/other-graph` so an aborted window still yields the positive-control reading. Criteria unchanged.

Build (2026-11-02 23:26 local): `cmake --preset Release -DCHARACTER_PANEL_BUILD_LABEL=v65-g4c1-resolver-fix` + `cmake --build --preset Release`, exit 0, only the pre-existing D9025 notices. Deployment: the v64 trio (DLL `ff00daad…`, PDB `3364ea18…`, the 2,530-byte C1 run INI `f714e05f…`) was backed up to `dist/deploy-backup-20261102-v64/`, then only the DLL/PDB were swapped — the INI stays the C1 run configuration, so the retry differs from attempt 1 by the binary alone. DLL string check confirms the v65 identity, `graph-match=` and `identity-mismatch`.

<a id="v66-g4c1-pinned-graph-2026-11-02"></a>
## v66: pin the concurrent window to the candidate's source graph (2026-11-02)

Attempt 2 (v65) validated the instrument mechanics but watched the wrong graph: `window-open` armed `words=463 nodes=36 data=0x1fec5517960`, while the candidate's source is `words=652 nodes=106 data=0x1feb4eb7510` (the `load-bindings` 15,203-binding set and `pre-activate` `behavior-data` both belong to the latter). Both resolvers walk the same manager list but prefer different objects — `player_graph()` takes the last non-1stperson graph, the controller's `source_graph()` prefers the graph whose `rootNode` is the actor's 3D. A zero on the non-source graph cannot answer the leak criterion, so attempt 2 is filed as inconclusive ([attempt 2](diagnostics/CharacterPanel-g4c1-attempt2-wrong-graph-20261102-2338.log), 137,435 bytes, SHA-256 `8a478311…`).

Build identity `CharacterPanel-scenegraph-v66-g4c1-pinned-graph-0ab9571833-cl94faaed0c6-20261102T154311Z` (clean tree at `0ab9571833`); DLL SHA-256 `190c7236b7de5299923fb32a3f5727dce0325dcbb68bb9ba41acf33a1962e7ad` (1,474,048 bytes), PDB SHA-256 `b4f2596d6e61ec7aa2ff61b25d75d65f2f737605967110a37a83871a4e2e4c7b` (38,522,880 bytes). Change: concurrent windows pin the experiment's graph — the arm validates it with `managed_graph(*holder, &a_source_graph)` and the sampler re-validates membership every frame (pointer comparison only, so a freed graph is never dereferenced); legacy F7 windows keep `player_graph()`. `window-open` therefore prints the pinned graph's words/nodes/data, checkable against the `OAR CONTEXT scan` and `load-bindings` rows of the same log.

Build (2026-11-02 23:43 local): configure + build exit 0, only the pre-existing D9025 notices. Deployment: the v65 trio was backed up to `dist/deploy-backup-20261102-v65/`, only DLL/PDB swapped; the INI stays the C1 run configuration (`f714e05f…`), so attempt 3 differs from attempt 2 by the binary alone. DLL string check confirms the v66 identity, `source-not-in-manager` and `pinned=yes`.

Attempt-3 acceptance (unchanged criteria): `installed mode=concurrent`, `window-open` before both `result=dispatched` rows with `words=652 nodes=106` (same source as the scan row), `window-closed reason=display-close` with `calls ≥ 2`, `other-graph ≥ 2`, `events=0`, `ring-dropped=0`.

Attempt-3 run (2026-11-03 00:10:44–00:10:54, identity confirmed in the live log): **hit — the C-segment source-event gap is closed (PENDING 2 retired)**. `window-open` reported `words=652 nodes=106 data=0x1c4b4cb7510`, bit-identical to the `pre-activate` `behavior-data` and consistent with the scan row's `src-count=106`, so the pinned target really was the candidate's source graph; it was armed at 00:10:44.638, before `WeapEquip` (44.890) and `Unequip` (45.025). The window stayed open 10.1 s through the replay and the display phase and closed as `reason=display-close events=0 phases=0 calls=62 other-graph=55 ring-dropped=0`: the positive control (62 calls, 55 classified as other graphs, including our two candidate dispatches) proves the detour was live and discriminating, while zero source events is the isolation reading. No `identity-mismatch`, no `window-aborted`, no `source-not-in-manager`, zero `[E]`; the replay reproduced its pose hashes an eighth time and `source-drift=none` held. Log archived ([137,368 bytes](diagnostics/CharacterPanel-g4c1-attempt3-pinned-graph-20261103-0010.log), SHA-256 `d2fa2b03…`). After adjudication the deployed INI returned to dormant off (`27f481f5…`, 2,147 bytes) with the v66 pair installed. G4's two evidence gaps are now both closed; the next work package is the [G5 v2 oracle/comparability and sampling audit](independent-behavior-graph.md#g5-draft).

> **前向更正（2026-11-03，[audit30](behavior-graph-construction-audit.md#audit30)）**：上段的 `events=0` 与 `other-graph=55` 已被判为**仪器无效**——58377 detour 的 thunk 把入口实参读低一格，门拿到的是调用者 `rax` 残值。`calls=62`（门之前计数）仍有效；G4-C 回到"待重跑"。历史日志与本节读数保留不改。

<a id="v67-g5e1-liveness-2026-11-03"></a>
## v67 G5-E1 liveness probe (2026-11-03): engine-free cases pass, Release build succeeds

**状态：源码、离线用例与 Release 构建完成；未部署、未运行。** 首轮构建在链接阶段失败，根因是**本机依赖状态**，不是代码；下面按"失败 → 根因 → 修复 → 结果"记录，避免下一次重复踩坑。

**改动**：`PLUGIN_SOURCES` 32→33（[probe_liveness.cpp](../src/animation/behavior_graph/probe_liveness.cpp)）；新策略头 [probe_liveness_policy.h](../src/animation/behavior_graph/probe_liveness_policy.h)（三态解析、计数器、SPSC 环、线程配对账本、单窗口状态机、`encode_detour_thunk`）；新键 `[Experiment] SamplingLiveness=off|on|concurrent`；接驳 `kDataLoaded` 安装、`PreLoadGame/NewGame` 失效、`renderer.cpp` 每帧 drain、实验 arm、显示关闭 close。同一轮修正了 `source_event_capture.cpp` / `source_write_capture.cpp` 的 thunk 实参槽位与 `source_write_capture` 的越界分配（[audit30](behavior-graph-construction-audit.md#audit30)）。

**离线用例（通过）**：`cl /nologo /std:c++latest /EHsc /W4 /WX /utf-8 /I src tests\probe_liveness_test.cpp`，exit 0，输出 `7 probe-liveness cases passed (engine-free)`；产物在 `build/behavior-graph-v67-g5e1-liveness-20261103/`。其中 `thunk_passes_entry_arguments_and_returns` 在 `VirtualAlloc` 内存里执行**生产字节**，锁定入口实参槽位与返回地址；它同时是发现旧 thunk 缺陷的用例（旧布局实测收到 `[rax, this, arg2]`）。

**失败与根因（全部已解决）**：

1. **`detours.lib` 选到 debug 库**：清缓存后 `find_library(DETOURS_LIBRARY detours REQUIRED)` 解析到 `build/vcpkg_installed/x64-windows-static-md/**debug**/lib/detours.lib`，于是 `__imp__CrtDbgReport` 未定义。修复：显式 `-DDETOURS_LIBRARY=<…>/lib/detours.lib`（release）。
2. **`spdlog.lib` 的三个 STL 内部实例不可解析**：vcpkg 的 1.17.0 `spdlog.lib` 引用 `??$_Find_first_of_pos_vectorized@DD@std@@…`、`??$_Find_last_of_pos_vectorized@DD@std@@…`、`??$_Find_last_vectorized@$$CBDD@std@@…`，而两个已装 toolset（14.44.35207 / 14.51.36231）的 `msvcprt.lib`/`libcpmt.lib` 都不导出对应修饰名。已排除的做法：用 `/std:c++17` 与 `/std:c++latest` 从 buildtrees 各重建一次 `spdlog.lib`（`/MD /O2`、`SPDLOG_COMPILED_LIB`+`SPDLOG_FMT_EXTERNAL`）、加 `_USE_STD_VECTOR_ALGORITHMS=0`——重建产物仍引用这三个符号。**修复：改用 `spdlog::spdlog_header_only`**（[CMakeLists.txt](../CMakeLists.txt) 注释记录了原因）。同样三个用例在普通 TU 里能正常 emit（独立探针 `build/std-helper-probe/` 验证过），header-only 把 spdlog 代码编进我们自己的 TU，因此不再需要那个外来二进制。
3. **FetchContent 需要 GitHub**：清缓存后 `_deps/hde64` 重新 populate 会 `git clone https://github.com/TsudaKageyu/minhook.git`，本机 TLS 不通。修复：已存在 `build/_deps/hde64-src` 的前提下加 `-DFETCHCONTENT_FULLY_DISCONNECTED=ON`。
4. **vcpkg 全量 from-source 重建不可行**：`--binarysource=clear` 会在 `simpleini` 上需要下载 MSYS2/mingw `pkgconf`，`mirror.msys2.org` 不可达；因此保留二进制缓存（`vcpkg install` 180 ms 恢复全部包），只对 spdlog 用 header-only 绕过。

**构建命令（本机实际使用）**：`vcvars64.bat`（VS18，默认 toolset 14.51.36231）→ `cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_CONFIGURATION_TYPES=Release -DCMAKE_CXX_FLAGS="/EHsc /MP /W4 /WX" -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL -DVCPKG_TARGET_TRIPLET=x64-windows-static-md -DCMAKE_TOOLCHAIN_FILE=C:/env/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_MANIFEST_INSTALL=OFF -DVCPKG_INSTALLED_DIR=…/build/vcpkg_installed -DDETOURS_LIBRARY=…/lib/detours.lib -DFETCHCONTENT_FULLY_DISCONNECTED=ON` → `cmake --build build --config Release --target CharacterPanel`。

**结果**：构建 exit 0，只出现既有 `D9025`／CommonLibSSE 的 `hde64.c` C4701/C4706 与安装规则警告。**clean-tree 重建**（提交 `7dd5ea7458`，子模块 `94faaed0`，无 `+dirty`）的身份与工件：

- 身份：`CharacterPanel-scenegraph-v67-g5e1-liveness-7dd5ea7458-cl94faaed0c6-20261103T155041Z`
- DLL：1,501,696 字节，SHA-256 `e725066c0428f4642bb4529b80c0419490ef4aa1be7b0ec9cd24fd49e4e3457e`
- PDB：48,902,144 字节，SHA-256 `1a4da885394785146ad2dcf5cf7f7e030d87ccec0657ff4cb0fbf6cafcd47bc4`
- CodeView：`{7F586C2E-9F62-4CE8-B33B-D19AFE9C6461}` age 395（与 v18–v66 同一 GUID 家族）
- 导出：`SKSEPlugin_Load` / `SKSEPlugin_Query` / `SKSEPlugin_Version` 三项齐全

本轮**未部署、未运行**；`SamplingLiveness` 仍为 dormant `off`，v66 的 DLL/PDB 继续留在部署目录与 `dist/deploy-backup-20261102-*`。上机前须按 [C-G5-E1 契约](independent-behavior-graph.md#c-g5-e1-probe)第四节准备 INI 并重启。

### Deployment (2026-11-04 00:0x local)

- **备份** v66 三件套到 `dist/deploy-backup-20261104-v66/`：DLL 1,474,048 字节 `190c7236…`、PDB 38,522,880 字节 `b4f2596d…`、部署 INI（dormant）2,147 字节 `27f481f5…`（另存 `deployed-dormant-27f481f5.ini`）。
- **部署** v67：DLL 1,501,696 字节 `e725066c0428f4642bb4529b80c0419490ef4aa1be7b0ec9cd24fd49e4e3457e`、PDB 48,902,144 字节 `1a4da885…`（与 clean-tree 构建逐字节相同；DLL 内嵌身份字符串 `…-7dd5ea7458-…` 与 `PROBE LIVENESS` 目标串均已核对）。
- **运行 INI**：9,465 字节 `ff3c6f34a29bd67d4134b6a5a4e9de43e47c000d5bf6a445393a2b6d45ba2809`，由仓库样例派生并逐键读回断言：`IndependentGraph=inject`、`DisplayPose=true`、`DisplayStep=true`、`SamplingLiveness=concurrent`，`SourceTimeline/SourceEventCapture/SourceWriteCapture/OarContextWatch` 与全部 candidate 探针保持 off/空（契约禁止叠加 detour 仪器）。
- **运行（2026-11-04 00:10，已测）**：面板打开于 00:12:00.314（源快照 `paused-source`），实验 00:12:00.333 武装窗口，00:12:20.345 到期（995 帧）；日志归档 [221,958 字节 `b5318dcb…`](diagnostics/CharacterPanel-g5e1-liveness-first-20261104-0010.log)。判读见 [C-G5-E1 首轮运行](independent-behavior-graph.md#c-g5-e1-run1)：P1/P5 通过、Q2 因窗口只覆盖暂停期而未被检验。
- **部署已回到 dormant**：`CharacterPanel.ini` 已从备份恢复为 `27f481f5…`（2,147 字节）；v67 DLL/PDB 留在原位，等待 v2 窗口的第二次运行。
- **补充运行（2026-11-04 00:22，`on` 模式，零代码改动）**：INI 9,456 字节 `11d94ec2…`（`SamplingLiveness=on`、`IndependentGraph=off`），游玩态 F7 窗口 19.99 s / 748 帧，`e:engine=161006`、`flag=0`；日志归档 [5,669 字节 `c0a07a85…`](diagnostics/CharacterPanel-g5e1-liveness-on-mode-20261104-0021.log)。判读见 [补充运行 A](independent-behavior-graph.md#c-g5-e1-run2)。运行结束后 INI 再次恢复为 dormant `27f481f5…`。

<a id="v68-g5e1-resumed-2026-11-04"></a>
## v68 G5-E1 v2 resumed window (2026-11-04)

**改动**：只改探针的窗口语义与记录器门（[v2 预注册](independent-behavior-graph.md#c-g5-e1-v2)）——`ProbeWindow` 增加 `display`/`resumed` 两段（首次 `display-close` 转段并重计时 10 秒）、`close_probe_liveness_window("display-close")` 在 concurrent 下转段而不是关闭、`expired` 标签补齐、记录器门改 `std::atomic<bool>`、插件发起的 `63856` 入明细。Engine-free 用例 8 例通过（`8 probe-liveness cases passed`，产物 `build/behavior-graph-v68-g5e1-resumed-20261104/`）。

**构建**：同一套命令（toolset 14.51.36231、header-only spdlog、release detours、`FETCHCONTENT_FULLY_DISCONNECTED=ON`），configure/build 均 exit 0，仅既有警告。

- 身份：`CharacterPanel-scenegraph-v68-g5e1-resumed-73bff1d32d-cl94faaed0c6-20261103T162754Z`（clean tree，无 `+dirty`）
- DLL：1,503,232 字节，SHA-256 `63f2c1696a087718d338bf4f16cf59a424af81c0088ad4bdea81c69a7a64cec8`
- PDB：48,918,528 字节，SHA-256 `9ef8305de63d6b05dfcf879de4b31a13491c237f91675908820790dd3f8a78ed`

**部署（2026-11-04 00:2x）**：v67 三件套备份到 `dist/deploy-backup-20261104-v67/`（DLL `e725066c…`、PDB `1a4da885…`、dormant INI `27f481f5…`）；v68 DLL/PDB 就位；运行 INI 回到 concurrent 配置 9,465 字节 `ff3c6f34…`（`IndependentGraph=inject`、`DisplayPose=true`、`DisplayStep=true`、`SamplingLiveness=concurrent`）。**尚未运行**；运行结束后恢复 dormant `27f481f5…`。



<a id="g5-e1-evidence-repair-20261104"></a>
## G5 E1 evidence repair verification (2026-11-04, not deployed)

The current repair changes two instruments' hook storage to shared 256-byte blocks / two-hook 512-byte pools, serializes concurrent push with a try-once counted contention gate, closes recorder admission before asynchronously waiting for accepted callbacks and draining the tail, and reports 63856 as `apply-entry` with a pre-clamp `requested-count`. See [current semantics and pending E1 evidence](independent-behavior-graph.md#g5-e1-evidence-repair) and [audit31](behavior-graph-construction-audit.md#audit31). It adds no native completion hook and establishes no game pass.

Focused verification: MSVC VS 2022 `/std:c++latest /EHsc /W4 /WX /utf-8 /I src` compiles [probe_liveness_repair_test.cpp](../tests/probe_liveness_repair_test.cpp), and both cases pass (`2 probe-liveness repair cases passed (engine-free)`). [The prior driver](../tests/probe_liveness_test.cpp) compiles after the entry-name update; its eight cases were not rerun. A generated-only mutation that restores the old relocated-head offset fails the first case at its owned-block bounds assertion (exit 1); the repaired production policy passes. Artifacts are under ignored `build/g5-e1-evidence-repair-20261104/`; no package or installed file is changed.

The first production build exposed a stale generated `CMAKE_ROOT` pointing to an absent CLion CMake 4.3 module directory. Reconfiguring only that cache path to the installed CMake 4.1 modules succeeded offline, preserving VS 2022/v143 14.44.35207 and local dependencies (`VCPKG_MANIFEST_INSTALL=OFF`, disconnected FetchContent). The next default-permission build stopped before compilation because MSBuild SDK discovery could not read `C:/Users/lhq49/AppData/Local/Microsoft SDKs` (`MSB4184`). The same smallest production target subsequently **passed (exit 0)** with tool-managed elevation for the demonstrated SDK read restriction; only the existing D9025 `/Ob2` to `/Ob3` notice remained. This distinction preserves the failed default-permission attempt without describing it as a compiler failure.

Latest recorded deployed DLL remains [v68](#v68-g5e1-resumed-2026-11-04); the last recorded INI restore is dormant `27f481f5…`. This turn does not inspect the installation or deploy a repaired DLL. Before further game testing, archive matching DLL/PDB/build identity and actual INI, register the revised entry/transport acceptance criteria, then return full install/arm/segment/final-close rows, raw log and settings/resource identity. Old v67/v68 logs do not validate these repairs or the original E1 completion boundary.

The resulting **local, uncommitted repair build** contains DLL 1,590,784 bytes SHA-256 `5220a8c7cf8da815859ac353ddf87c238f9fe68e2464d2534d7e7c2d3b569d66` and PDB 37,965,824 bytes SHA-256 `8b5c9d06c4abff0ca9b53a64d07b1f6da034bbd9c37b463c46cd6229e3afd05d`. Its cached label remains `CharacterPanel-scenegraph-behavior-graph-v46-retained-copy-ledger-20261029-ff5f411e10+dirty-cl94faaed0c6-20261104T065341Z`: this is a newly built repair artifact with an old cached label and an honest dirty revision, **not** the historical v46 artifact, a clean-tree build, v69, a deployment or game evidence. Root's build record is ignored `.codex/task-runs/g5-e1-evidence-repair-20261104/planner-production-build.json`.

<a id="v69-g5e1-clean-rebuild-2026-11-04"></a>
## v69 G5-E1 evidence-repair clean-tree rebuild (2026-11-04)

**Scope**: production Release rebuild of the committed repair (`a89fa5cba039306ebfe2244ad1b0af417af31529`, clean tree at configure time, submodule `94faaed0c6` clean), replacing the uncommitted dirty repair build above whose artifacts are no longer present on disk. No source changes; same repair semantics as [this section](#g5-e1-evidence-repair-20261104). **Not deployed, not run**; the deployed plugin remains [v68](#v68-g5e1-resumed-2026-11-04) with dormant INI `27f481f5…` (verified by hash on 2026-11-04).

**Build**: existing `build/` cache reused (CMake 3.31.4 at `C:/env/CMake`, VS 2022 generator, toolset `v143,version=14.44.35207`, `VCPKG_MANIFEST_INSTALL=OFF`, `FETCHCONTENT_FULLY_DISCONNECTED=ON`, release `DETOURS_LIBRARY`, `x64-windows-static-md`). Reconfigured in place only to set the identity label, then `cmake --build build --config Release --target CharacterPanel` → exit 0, only the pre-existing `D9025 /Ob2 → /Ob3` notices; no MSB4184 this round (session runs with unrestricted file access).

- Identity (embedded, verified by byte search in the DLL): `CharacterPanel-scenegraph-v69-g5e1-evidence-repair-a89fa5cba0-cl94faaed0c6-20261104T143255Z`
- DLL: 1,590,272 bytes, SHA-256 `2490c38df17107297a3fa1b9debfc1586700484793b1f421feb75def4dfda11e`
- PDB: 48,918,528 bytes, SHA-256 `f69c97906e6f2543eb6134e85352ce832c8709b4f222f48ff97cf8a4a95fe23a`
- CodeView (read from the DLL debug directory): `{7F586C2E-9F62-4CE8-B33B-D19AFE9C6461}` age 397, PDB path `E:\SkyrimTools\Proj\CharacterPanel\build\Release\CharacterPanel.pdb` — same GUID family as v18–v67; ages increment by one per build (v67 recorded 395; v68 measured 396 from the deployed DLL this round, previously unrecorded; v69 is 397)
- Exports: `SKSEPlugin_Load` / `SKSEPlugin_Query` / `SKSEPlugin_Version` all present
- Relation to the retired dirty build: size 1,590,272 vs 1,590,784 and different hashes — expected from the different source revision (clean `a89fa5cba0` vs dirty `ff5f411e10+dirty`), identity label and configure stamp; both implement the same committed repair. The dirty build's files are not retained anywhere on disk; this clean v69 build supersedes it.

**Next** (unchanged from the [handoff](independent-behavior-graph.md#handoff-20261102-evidence)): before any game session, derive the run INI from the repo sample with per-key read-back assertions (concurrent configuration: `IndependentGraph=inject`, `DisplayPose=true`, `DisplayStep=true`, `SamplingLiveness=concurrent`, all other instruments off), back up the deployed v68 trio, deploy v69 with matching identities, run the concurrent→resumed window, and return the new close-row reconciliation (`eligible/recorded/drained/missing`, `contention-dropped/full-dropped`, pairing diagnostics). Then the G4-C fixed-instrument positive-control re-run in a separate session.

### Deployment (2026-11-04 22:5x local, v69 + concurrent run INI)

- **Backup** of the deployed v68 trio to `dist/deploy-backup-20261104-v68-v69pre/` (hash-verified copies): DLL `63f2c169…` 1,503,232 bytes, PDB `9ef8305d…`, dormant INI `27f481f5…` 2,147 bytes.
- **Run INI derivation** (anti-pattern 14, per-key asserted): regex-flipped four keys in the committed sample (`IndependentGraph=off→inject`, `DisplayPose/DisplayStep=false→true`, `SamplingLiveness=off→concurrent`), all other instrument keys asserted unchanged (`SourceTimeline/SourceTransientWatch/SourceWriteCapture/CandidateVariableCensus/CandidateInputProbe/CandidateGenerationEntry/CandidateUnequipProbe/OarContextWatch=false`, `SourceEventCapture=off`, probe keys `IsEquipping/1`, three unequip name keys empty). First write gained a UTF-8 BOM (`Set-Content -Encoding UTF8`); rejected and rewritten BOM-less to match the sample's literal format. Post-write read-back asserted all 20 keys from disk. Final run INI: 9,640 bytes SHA-256 `c3703b74b0328dc22e2efb7b3581c5c4783c13119f0d9b4e3c3dc2f867fcf7b0` (source candidate kept at ignored `build/v69-run-ini-candidate.ini`).
- **Deployed** to `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\` (no Skyrim process running at the time; `CompiledPluginsPath` unset): v69 DLL 1,590,272 bytes `2490c38d…` + PDB `f69c9790…` + run INI `c3703b74…`, each destination copy hash-compared to its source. Deployed INI gate keys re-read from the destination: `inject/true/true/concurrent` with all conflict instruments off — 7/7 correct.
- **Awaiting run**: the game session (open panel → experiment arms concurrent → first display close → 10 s resumed → final close) is performed by the user; judgement follows the [E1 repair boundary](independent-behavior-graph.md#g5-e1-evidence-repair) close-row reconciliation. After the run, restore the dormant INI from the backup.

### Game runs (2026-11-04 23:0x/23:1x local, v69, two runs)

- **Run 1** (23:05:37.167 arm → 23:05:57.167 expiry, 1040 frames, no transition): panel stayed open for the whole window, so no `display-close` occurred and the window expired inside the display segment — **Q2 untested per the v2 pre-registered invalid branch, not a negative result**. The repaired instrument itself validated: install rows match the new 256-byte thunk layout (thunk `+0/+0x100`, original blocks `+0x80/+0x180`), the close-row reconciliation held with zero gaps (`eligible=recorded=drained=2628`, `missing/orphan/unpaired/mismatch/no-slot=0`, `contention/full-dropped=0`, `transport-complete=yes`), and the v68 1,100/1,099 pairing gap is gone (1314/1314). Log [323,130 bytes `f00960a7…`](diagnostics/CharacterPanel-g5e1-v69-repair-run1-20261104-2307.log); judgement in [v69 run 1](independent-behavior-graph.md#c-g5-e1-v69-run1).
- **Run 2** (same binary/INI, game restart; 23:15:23.983 arm → 23:15:28.432 transition at display-segment frame 225 → 23:15:38.437 expiry): **full two-segment window**. Close row: `e:engine=88645/plugin=380`, `pinned-engine=408` (resumed delta from 0, all `caller=0xba3f6e`, `num240=116`), `tracked=paired=788` with zero mismatch/orphan/no-slot, six rotating worker tids (P4 refutation reproduced), `flag=0` both segments, reconciliation identities with zero gaps, `suppressed-rows=0` (1,576 rows all printed: 380 plugin pairs + 408 pinned pairs). **The repaired instrument's E1 evidence gap (entry liveness / pairing / transport-close reconciliation) is closed**; per audit31 this is not output completion and gate 1 stays NOT_READY. Log [261,457 bytes `ddede89b…`](diagnostics/CharacterPanel-g5e1-v69-repair-run2-resumed-20261104-2315.log); judgement in [v69 run 2](independent-behavior-graph.md#c-g5-e1-v69-run2).
- **Post-run state**: deployed INI restored from the backup to dormant `27f481f5…` (hash-verified against `deploy-backup-20261104-v68-v69pre`); v69 DLL/PDB remain installed. Next: G4-C fixed-instrument positive-control re-run in a fresh session (SourceEventCapture concurrent, requires its own INI derivation).

### G4-C1 re-run INI deployment (2026-11-04 23:2x local, v69 unchanged)

- **Derivation** (anti-pattern 14, per-key asserted): same clean-tree sample, four keys flipped — `IndependentGraph=inject`, `DisplayPose=true`, `DisplayStep=true`, `SourceEventCapture=concurrent` — with `SamplingLiveness` asserted **off** (probe install-skip discipline; the two detour probes must not stack, [C-G5-E1 contract §4](independent-behavior-graph.md#c-g5-e1-probe)) and all other instrument keys asserted unchanged. Pre-write and post-write read-backs verified all 20 keys; no BOM. Run INI: 9,640 bytes SHA-256 `12fbcb41acecf99e0fe7adb01e2e76c832f3809f75986b0ac34c44e35b793c4b` (candidate kept at ignored `build/v69-c1-run-ini.ini`).
- **Deployed** the INI to `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\` (destination hash-compared, five gate keys re-read correct; no game process running). v69 DLL `2490c38d…` / PDB `f69c9790…` confirmed still in place — **no rebuild or redeploy needed**: the committed repair `a89fa5cba0` already contains the fixed 58377 thunk slot layout (audit30's fix), so the [C-G4-C1 contract](independent-behavior-graph.md#c-g4-c1-concurrent-events) re-runs on v69 as-is.
- **Session protocol** (user, per the v66-validated procedure): start game → load save → open the panel once → the replay runs by itself (two `result=dispatched`) → keep the panel open 2–3 s so the display segment produces steps → **close the panel within 15 s**. Judgement follows the pre-registered criteria 1–6 (`installed mode=concurrent`; `window-open` with `words=652 nodes=106` pinned identity before both dispatches; close `reason=display-close` with `calls≥2`, `other-graph≥2`, `events=0`, `ring-dropped=0`; `expired` closes accept only injection-segment readings and do not close G4-C). INI returns to dormant after the run.
