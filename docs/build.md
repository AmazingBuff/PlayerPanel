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

## Verification and limits

### Current animation checks (2026-10-22)

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

There is no CMake test target or automatic game execution. For current torch/shield/Smooth/axe diagnosis, follow the [section-8 symptom procedure](diagnostics/display-issues-2026-10-22.md#8-2026-10-22-已报告问题的关联诊断), [accepted mapping semantics](animation.md#accepted-mappings-and-world-snapshots) and [debugging navigation](code-maintenance.md#debugging-navigation). Detailed tracing defaults false, requires a restart when enabled, and has request-wide four-window/twenty-four-snapshot pose caps plus one fixed fifteen-bone source snapshot. Native runtime, visual equivalence and performance remain unverified.

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

Runtime inspection supports standard uncompressed MGEF records and VMAD version 5/object formats 1 or 2. Compressed target records, unsupported or malformed metadata and unavailable files reject affected events. Actual `GetFile()` provenance, SDK master/full/light remapping and virtual filesystem visibility require game verification. Offline tests do not execute the native DAC handler, paused effect activation or restoration; see [attachment semantics](dac.md#marker-script-attachment-preflight) and the [dated follow-up](equipment-idle-dac-test-2026-10-20.md#8-脚本附件预检复核2026-10-21待上机).

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

The [cases](../tests/ied_condition_adapter_test.cpp) exercise the production [admission/guarded native dispatch](../src/animation/oar/ied_conditions.h): the representative native-preservation scenario covers equipped-placement false, left/right comparison, negation and disabled behavior, plus the zero-component SDS shield-setting false and negation; a historical-hand scope rejects before any native read. These remain two cases. They use narrow provider fakes and do not execute Skyrim, IED, SDS or the supplied DLL. The normal Release configure/build separately compiles the real OAR/SDK integration; no CMake test target or dependency is added. The [three-condition reference](oar-hooks.md#native-ied-placement-conditions-2026-10-21) records exact provider/schema versions, provider-side SDS prerequisites and archive/DLL provenance. Real OAR/SDS registration, paused IED hint freshness, native evaluation, missing-provider fallback and visuals remain game-only follow-up.

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
