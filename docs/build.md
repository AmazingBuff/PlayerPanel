# Build CharacterPanel

## Purpose

Build the current Windows DLL from this checkout. Release is the only configure/build preset and `CharacterPanel` is the only first-party target. The result is `build/Release/CharacterPanel.dll` with matching debugging symbols.

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

[CMakeLists.txt](../CMakeLists.txt) gathers first-party sources with `CONFIGURE_DEPENDS`, links CommonLibSSE and the existing installed libraries, embeds HLSL, and generates plugin/version metadata. No additional first-party executable or test target is configured.

OAR and DAC sources are nested under `src/animation/oar` and `src/animation/dac`. The existing recursive source/header globs and `src` include root cover both; reconfigure after directory changes to regenerate the source list. No separate target or namespace change is needed.

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

`Plugin_Build_Identity` is generated **at configure time** from the configured label, Git revision/dirty state, CommonLib revision/dirty state and configure timestamp. A commit alone does not refresh an existing header or DLL. When preparing a binary for a new commit, run **configure, then build** again and retain its matching PDB. The log's `SCENEGRAPH BUILD` identity identifies that configured binary, not whatever HEAD happens to be later.

## Failure modes

Generator/toolset discovery errors require the stated VS2022/MSVC installation. Missing CommonLib source requires the recorded submodule; missing package headers/libraries requires a working vcpkg root and the requested installed triplet. Installed-dependency reuse is valid only when those packages exist.

A warning that installation rules were specified while `CMAKE_SKIP_INSTALL_RULES` is enabled can come from retained dependency declarations; it does not add an install target to this project. Follow actual configure/build exit results.

## Animation configuration

Copy [config/CharacterPanel.ini](../config/CharacterPanel.ini) to `Data/SKSE/Plugins/CharacterPanel.ini` alongside CharacterPanel.dll. Under `[Animation]`, `UnarmedIdle=non_combat_loop` is the default; `mt_idle` chooses direct vanilla MT_Idle only when hands contain no displayable equipped item. Missing files retain the default; invalid values warn/default. Restart the game after changes. Existing SimpleIni handles loading at DataLoaded; no new dependency, MCM or reload control is required. Calibration always stays vanilla MT_Idle. See the [exact option table](animation.md#configuration); the visible direct-base option does not change the independent zero-time calibration.

## Behavior graph baseline checks (2026-10-23)

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

## G1b v2 run and v3 sequential-value experiment (2026-10-24)

The v2 build ran on 2026-10-24 13:30 and was rejected again in 0.289 ms, this time with the new `load-args` row showing `behavior-path='' project-name='DefaultFemale' path='DefaultFemale'` ([raw log](diagnostics/CharacterPanel-g1bv2-load-20261024-1330.log); verdict in the [v2 run record](independent-behavior-graph.md#g1b-v2-运行记录2026-10-24-会话)). The follow-up static audit ([third loader audit](behavior-graph-construction-audit.md#加载取值与前置链的续审计三2026-10-24g1b-v2-复盘)) established that registry keys carry the directory and extension (helper 69489 strips the extension only when writing `projectName`) and that 63949 creates the character setup itself, so the rejection was a pure registry miss.

The `load` branch was then reimplemented (v3): one read-only `race-fields` row dumps the holder's race candidates (`behaviorGraphs[sex].model`, `rootBehaviorGraphNames[sex]`, `behaviorGraphProjectNames[sex]`, `stringData->behaviorPath`, source `projectName`), and the loader is called in a pre-registered priority order (bg-model → project-name → root-name → stringData rebuild), at most four tries, stopping at the first accepted load — failed loads are the audited no-op path on a bare shell. Construction, budget (16 rows worst case), once-per-process gating and reference-count release are unchanged.

Verification: Release reconfigure/build passed with only the existing /Ob2→/Ob3 override warning. The new DLL embeds `CharacterPanel-scenegraph-scenegraph-646a0f2064+dirty-cl94faaed0c6-20261024T054649Z`, DLL SHA-256 `A3C2213F5B31A39652B79742FE49F512B91F745A55973EBA9AC5F3C9BD822C0D` (1,209,856 bytes), PDB SHA-256 `9753128A494A1F801118AFE4B7FF53B08098FA76BE5D9B3559400EDAF4B35046` (34,861,056 bytes). Deployment on 2026-10-24 13:55 replaced the v2 build after backing it up to `dist/deploy-backup-20261024-g1bv2/`; the deployed INI keeps `IndependentGraph=load`. The G1b v3 run happened the same day at 14:25: the first candidate (`race-bg-model` = `Actors\Character\DefaultFemale.hkx`) loaded successfully in 8.729 ms with every assembly field non-null and the shared resources same-addressed as the source, and the process then crashed in the destruction chain — `hkbBehaviorGraph::Deactivate` (vtable slot 7, reached from the `BShkbAnimationGraph` destructor) dereferences the null `activeNodes` of a never-activated graph (`SkyrimSE.exe+0AB1FC3`; attribution in the [fourth audit](behavior-graph-construction-audit.md#已装载图析构的崩溃定位2026-10-24-续审计四), verdict in the [v3 run record](independent-behavior-graph.md#g1b-v3-运行记录2026-10-24-会话)). Per the contract's stop-and-fallback clause the deployed INI is back to `IndependentGraph=off` (the deployed DLL is unchanged, so the experiment stays dormant); raw logs are archived as [CharacterPanel-g1bv3-load-crash-20261024-1425.log](diagnostics/CharacterPanel-g1bv3-load-crash-20261024-1425.log) plus the Crash Logger report. The destruction criterion returned to design review and was resolved the same day: the contract was revised (destruction evidence = shell release plus the loaded graph's teardown inside the G2 lifecycle) and the `load` branch now retains the assembled candidate in a deliberately never-released owning smart pointer (leaked on purpose — even static destruction must not run the engine's destructor chain; at most one loaded graph per process while the experiment is enabled), with the tail rows renamed to `phase=retained`/`phase=destroyed` accordingly. The v4 build embeds `CharacterPanel-scenegraph-scenegraph-646a0f2064+dirty-cl94faaed0c6-20261024T065321Z`, DLL SHA-256 `37C255DA5985DCF3F7ECA3CA6F56F0D570A0AA008A0177A1391E9440D3B4C3ED` (1,209,856 bytes), PDB SHA-256 `F5D290D2575D158F41D4804E9CDEF2F09B636D919DC779DAA66BD3CA22AA94F4`; deployed on 2026-10-24 15:10 after backing v3 up to `dist/deploy-backup-20261024-g1bv3/`, with the INI set back to `IndependentGraph=load`. The v4 run happened the same day at 14:57 and completed the G1 verdict: the load reproduced (`accepted ms=8.797`, identical race-fields), the post-load source baseline (captured while the candidate was alive) is byte-identical to the pre-construct baseline under an independent diff, and the session ended with `phase=end … retained=true`, no crash, and several further normal panel open/close cycles with the retained graph inert ([raw log](diagnostics/CharacterPanel-g1bv4-retain-20261024-1457.log), verdict in the [v4 run record](independent-behavior-graph.md#g1b-v4-运行记录与-g1-判定2026-10-24-会话)). **G1 is PASSED under the revised (Option A) criteria**; the deployed INI is back to `IndependentGraph=off` (the deployed DLL is unchanged) so future sessions do not re-run the experiment, and the loaded graph's destruction evidence moves to the G2 lifecycle. G1 remains the only gate passed; G2's prerequisite (the Activate/Update entry audit) has not started.

## G2 drive experiment implementation (2026-10-24)

The [G2 contract](independent-behavior-graph.md#2026-10-24-g2-契约drive-模式实验设计) adds a `drive` value to the existing experiment setting: after the unchanged G1 construct/load/retain flow the same paused bracket captures a full source baseline (pre-activation), calls the engine's own activator 63586 once on the candidate, then drives `hkbBehaviorGraph::Update` (58329, vtable slot 5) 120 times at a fixed 1/60 s step through a bare 58315-built context, and records three structural snapshots plus a pose-motion hash tier (source control pair and candidate steps 0/60/120). The context lives in raw zero-initialized storage because `hkbContext`'s `hkRefPtr<hkbGeneratorOutputListener>` member has no complete type on this side and can be neither constructed nor destructed here; 58315 writes every field as raw stores. Failed loads keep the proven shell path; the retention policy is unchanged.

Verification: Release reconfigure/build passed with only the existing /Ob2→/Ob3 override warning. The DLL embeds `CharacterPanel-scenegraph-scenegraph-646a0f2064+dirty-cl94faaed0c6-20261024T072214Z`, DLL SHA-256 `A3DEDE5FDD4635EEC928D54F73FCF00C6F9862F0A9F30C531264FD42B36B4F68` (1,216,000 bytes), PDB SHA-256 `452DE1802FDB15042587534638691068D0A87F77C6B85D0CB6DC5A83BD2F1FF2` (34,942,976 bytes). Deployment on 2026-10-24 15:35 replaced the v4 build after backing it up to `dist/deploy-backup-20261024-g1bv4/`; the deployed INI sets `IndependentGraph=drive`. The G2 run happened the same day at 15:31: activation fully succeeded (`activeNodes` 0→97 nodes, variable set allocated, flag set), the source stayed untouched (both baselines and the source pose-hash control identical), and all 120 update steps returned without a crash — but the candidate produced no observable motion: its `poseLocal` was never allocated (all three pose hashes `unavailable`) and the structural snapshot is unchanged across the drive. G2 is NOT PASSED on the motion criterion (insufficient evidence, per the pre-registered rule); the deployed INI is back to `off`. Raw log: [CharacterPanel-g2-drive-20261024-1531.log](diagnostics/CharacterPanel-g2-drive-20261024-1531.log); verdict in the [G2 run record](independent-behavior-graph.md#g2-drive-运行记录2026-10-24-会话). The pose-write chain audit ([sixth audit](behavior-graph-construction-audit.md#姿态写入链与字符级激活2026-10-24-续审计六)) then found the root cause: `hkbBehaviorGraph::Update` never writes `poseLocal`; the pose buffer is allocated by 58466 (`hkbCharacter::AllocatePose`, from `setup.animationSkeleton`) inside the ENGINE's character-level activator 58453, which runs AllocatePose BEFORE Activate and is gated on the graph's +0x12C flag — the graph-level 63586 wrapper used in the first run bypassed it entirely. The drive flow was revised (v2): one 58453 call replaces 63586, with a pre-registered clean skip (`skip=setup-skeleton-missing`) when the fresh setup's `animationSkeleton` is still null (the allocator would dereference it unchecked), and the structural snapshot now also records the setup fields and the +0x12E children-activation gate. The v2 build embeds `CharacterPanel-scenegraph-scenegraph-646a0f2064+dirty-cl94faaed0c6-20261024T075048Z`, DLL SHA-256 `99E0B38E44B1B743E4518BCEB5FDE64CCAB8469711E8B6E20F8E44E4A5E0DD26` (1,216,512 bytes), PDB SHA-256 `8CA2AAE664CDB9425599BA7763A5DFC02DA4F4C2A584BE7E958978E49D88DDA3`; deployed on 2026-10-24 16:05 after backing the first drive build up to `dist/deploy-backup-20261024-g2v1/`, with the INI set back to `IndependentGraph=drive`. The G2 v2 run happened the same day at 15:57 and validated the fix: the character-level activator allocated the pose buffer (`pose-local=…/116`, and `setup.animationSkeleton` confirmed non-null — the load chain does fill it), activation reached 97 active nodes, the source stayed untouched and nothing crashed — but the candidate's three pose hashes are identical (`bc160a8c…`): the buffer keeps the skeleton bind pose written by AllocatePose, so the update's output never reaches it. A tail audit of 58329 confirmed it contains no `poseLocal` write at all. The decisive discriminator is now implemented and pre-registered: `phase=gen-hash` rows hash the candidate's `generatorOutputs[0/1]` (self-describing window via the track master header's byte count) at activation, step 60 and step 120 — changed hashes mean generation works and only the sampling step is missing; unchanged hashes point upstream (+0x12E gate / 60079 semantics / rootGenerator state). Build `…20261024T080506Z`, DLL SHA-256 `A03705169F725DB117997DEA81079FAEA4CDAD26A46F43E5FC096C4448DB4650` (1,219,072 bytes), deployed 2026-10-24 16:20 with the INI set to `IndependentGraph=drive` ([v2 run record](independent-behavior-graph.md#g2v2-drive-运行记录2026-10-24-会话); raw log [CharacterPanel-g2v2-drive-20261024-1557.log](diagnostics/CharacterPanel-g2v2-drive-20261024-1557.log), INI reverted to off after the run and re-set for the discriminator run). The discriminator run happened at 16:09: the generator-output hash read garbage (`numBytes=-898388560` — the objects do not carry the CommonLib TrackMasterHeader layout, which is an unverified activeragdoll port), so the window assumption failed; the pose hashes stayed identical again. A context-read enumeration of 58329 then established the update consumes ONLY the activeNodes list, ctx.eventQueue and the timestep, and that vtable slot 6 is an event-dispatch-to-active-nodes helper — neither is a missing output request. The probe was revised (v3): `phase=probe` rows hash the activeNodes entry array (0x90-byte per-node runtime states, self-describing count) and the variable set word array at activation/step 60/step 120, plus a raw 16-byte dump of each generator output. Changed hashes mean the update advances node state (and only the pose sampling is missing); a fully frozen array means the per-node gates skip everything. Build `…20261024T081858Z`, DLL SHA-256 `A8A4BF3D4C8938B0A5D811CF60F18B3E0A0E7FC7D2628280F937AC7420360CBE` (1,220,096 bytes), deployed 2026-10-24 16:40 with the INI set to `IndependentGraph=drive`. The probe run at 16:22 was decisive for the generation chain: the activeNodes entry-array hash changes every step (three distinct values) — the update does advance all 97 nodes — while the pose buffer stays at the bind pose and the generator-output dumps revealed one-element array headers rather than pose tracks. The sampler was then located: 63581 (0xbafda0) copies the generated pose track from the generator-output element into poseLocal via the same 57148 fill helper AllocatePose uses, entered through 63589 (0xbb2a40) whose gates the candidate passes EXCEPT the current-animation word at graph+0x240 (written per frame by the manager-driven 63587; the candidate has no manager). G2 v4 mirrors `[source+0x240]` into the candidate after each update step (an engine-meaningful field for the same graph over the same shared binding set) and calls 63589 to sample. Build `…20261024T083143Z`, DLL SHA-256 `AA0A36AF621CE04447546BD521FC526EDC10EC4984B21148F751D6A4202A0CB4` (1,221,120 bytes), deployed 2026-10-24 16:50 with the INI set to `IndependentGraph=drive`. The v4 run happened at 16:35 and corrected the field semantics: `[graph+0x240]` is NOT an animation index but the SAMPLE-TRANSFORM COUNT (0x74 = 116 = the skeleton bone count; 63587 clamps it against exactly that count), the mirror landed correctly (`cand-anim=0074`) and 63589 was called every step without a crash — but the generator-output ELEMENT content hash stayed constant across all 120 steps: the generation stage writes nothing into the output. G2 remains NOT PASSED on the motion criterion; INI back to `off` ([raw log](diagnostics/CharacterPanel-g2v4-drive-20261024-1635.log)). The remaining gap is the manager per-frame pipeline the drive flow skips: the event-processing stage (63602 at 0xbb4c50, which sets the +0x12E children-activation-request flag; called from 0xba4800 in the manager cluster) and 63587's pre-update calls (63761 with the global singleton, 77924 with physicsWorld — possibly where the output buffers get bound for the frame). The pipeline audit then found the capstone: a notification-hub singleton (0x2022570, vtable 0x1986740) subscribes graphs (63761) and its slot-5 handler runs the per-frame graph pipeline — 63602's tail sets **`behaviorGraph+0x12E = 1` (the per-frame children-activation request) before calling 63587** for each subscribed graph. Our drive never set that byte, so Update's internal gate skipped its 60079 activation walk every frame — the root cause of the silent generation. G2 v5 adds exactly that one-byte request before each update step (everything else unchanged). Build `…20261024T085731Z`, DLL SHA-256 `9A51725C635B3A7D7A98E7916DB012FAE108D4AE0D7E2277FA9F6B37B4A71194` (1,221,120 bytes), deployed 2026-10-24 17:05 with the INI set to `IndependentGraph=drive` ([v4 log](diagnostics/CharacterPanel-g2v4-drive-20261024-1635.log), INI reverted to off after the v4 run and re-set for v5). The v5 run at 17:02 honored the request (children-flag consumed, node states churning) but the generator-output element and the pose buffer STAYED constant — the children-activation request alone does not unlock generation. Strategy pivot (v6): instead of hand-assembling the drive from parts (raw 58329 + field mirror + 63589), each step now calls the ENGINE's own per-frame update wrapper 63587 with the instruction-audited 63602 record bytes (all-zero except the transform-count word @+0x28 = the candidate's numPoseLocal 116, a flag byte @+0x2A = 1 and the dword 0x1000101 @+0x2B) — the wrapper internally subscribes the graph into the notification hub, writes graph+0x240, runs the ragdoll/physics pre-calls and the raw update; the 63589 sampling entry still follows. Build `…20261024T090815Z`, DLL SHA-256 `8FC00E4EFD43B7D901D8F49BD2394262C5765AD58355DDA1AE512E52CA8E5A4D` (1,221,120 bytes), deployed 2026-10-24 17:20 with the INI set to `IndependentGraph=drive` ([v5 log](diagnostics/CharacterPanel-g2v4-drive-20261024-1635.log) era reverted to off between runs). The v6 run crashed immediately on opening the panel: 63587's post-update stage 63583 registers the animation NAME into a global table derived from `projectDBData->+0xD0` and indexed by the animation selection — context only the manager-driven flow provides; on the candidate `BSFixedString::Set` wrote through a null holder (`SkyrimSE.exe+0CEC6B1`, full attribution in the [crash report](diagnostics/CharacterPanel-g2v6-crash-report-20261024-1711.log)). The wrapper route is therefore closed: the drive was reverted to the crash-free v5 form (raw 58329 + transform-count mirror + 63589) and redeployed — build `…20261024T091522Z`, DLL SHA-256 `3733ACFEC1F841671111D0DFEDA85D56075FA3831EC2B08681E0A2BC0F3473BA`, INI back to `IndependentGraph=off` ([v6 crash log](diagnostics/CharacterPanel-g2v6-crash-20261024-1711.log)). Strategic conclusion after six drive iterations: every stage of the drive chain is now instruction-level mapped, and the remaining gap — animation SELECTION during generation — is manager-driven and belongs to feature design (the panel preview will choose animations itself), not to feasibility. G2's motion criterion needs redefinition at design review.

## G1 binary audit (2026-10-23)

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

Source review covers typed transition effect-refresh, pending reuse/cancel, arm readiness/retarget/same-asset/outgoing composition, raw XYZ sampling and trace caps. The scenarios do not execute `AnimationController`, the game loader, OAR/IED hooks, DAC, camera or rendering. Native condition answers, asynchronous timing, complete-left-arm coverage against installed HKX assets, source isolation at runtime, Smooth Start/Loop, axe flash and bow/crossbow scale causes remain UNVERIFIED. No clamp or guessed mapping was added. The final DLL/PDB pair matches CodeView/PDB GUID `{748DB13E-5AAA-4883-8283-751805A36ED4}`, age 74; hashes and configure-time identity are in the package manifest and dated handover. The new DLL/PDB is copied only to the ignored `build/animation-repair-20261023/test-package` for manual review; no install/package target or game deployment runs. Follow the [current handover](diagnostics/display-issues-2026-10-22.md#10-2026-10-23-装备上下文与火把分层修复待上机).

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

The [animation cases](../tests/animation_controller_test.cpp) use the direct command above; retained [graph/math](../tests/snapshot_transform_test.cpp) checks remain outside CMake. Earlier test results do not imply that current CMake builds either source. No Skyrim runtime is available here: native loading, hooks, visuals, framing and graph lifetime remain unverified. See [debugging navigation](code-maintenance.md#debugging-navigation) and [SceneGraph](scene-graph.md).

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

## Synchronized files

Maintain this guide with [README](../README.md), [SceneGraph](scene-graph.md), [animation](animation.md), [maintenance](code-maintenance.md), CMake/preset/template inputs and [runtime notes](../tools/scene_copy/README.txt). Keep current build commands out of [historical evidence](history.md), whose commands describe earlier revisions.

## Related history

The [historical index](history.md) preserves older probe, test and package records. The current single-DLL setup supersedes their build entry points without changing their recorded outcomes.
