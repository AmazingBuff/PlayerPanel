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

Verification: Release configure/build with the existing preset and installed packages passed with no warnings-as-errors; the new object is `build/CharacterPanel.dir/Release/independent_graph.obj`. The clean deployed identity is `CharacterPanel-scenegraph-scenegraph-5d4bee1d54-cl94faaed0c6-20261023T155405Z` (HEAD `5d4bee1`) and is present in the DLL; DLL SHA-256 `3BCC9FC14B6FD2492D2627CC0CBB4D9B12B6A9069ACB7B7D5C99E4BE58A4A658` (1,202,176 bytes) and PDB SHA-256 `127347733E9B8D565A3819E69582193848C6A4A091C41719EB5F76DA52A3411B` (34,779,136 bytes) come from the same link. Deployment on 2026-10-23 23:54 replaced the `5ef6f05` build under `E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\` after backing it up to `dist/deploy-backup-20261023-5ef6f05/`; the deployed INI sets `IndependentGraph=construct` with both diagnostics flags off, so the next launch collects G1a only. The run itself has not happened.

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
