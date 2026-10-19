# Build CharacterPanel

## Purpose

Build the current Windows DLL from this checkout. Release is the only configure/build preset and `CharacterPanel` is the only first-party target. The result is `build/Release/CharacterPanel.dll` with matching debugging symbols.

## Scope and non-goals

This guide covers the existing C++23/x64 toolchain, dependencies, generated files and local build evidence. It does not add test, package, install or compatibility routes, upgrade dependencies, deploy files or establish game-time correctness. Plugin metadata declares Skyrim 1.6.1170 compatibility and minimum SKSE 2.2.6 through `CompatibleVersions` and `MinimumRequiredXSEVersion`; see [main.cpp](../src/main.cpp), `SKSEPlugin_Version`.

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

Replace the placeholder with your local path. [CMakePresets.json](../CMakePresets.json) reads that environment variable and requests Windows x64, the `v143,version=14.44.35207` toolset, `MultiThreadedDLL`, and `x64-windows-static-md`. [vcpkg.json](../vcpkg.json) owns package dependencies and their baseline. A normal configure uses that manifest; this guide does not supply a separate bootstrap or dependency-upgrade procedure.

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

## Verification and limits

Prior local native evidence on 2026-10-19:

- Before test-target removal, production, standalone animation and math targets built, and the two selected animation behavior cases passed.
- After consolidation, the Release preset configured/built the DLL. Its SKSE load/query/version exports, matching PDB, embedded identity, Release-only graph and required CommonLib/CMake utility projects were checked.
- Documentation/source checks cover links, history parity and unchanged inputs; they are separate from native execution and runtime validation.

The retained [animation](../tests/animation_controller_test.cpp) and [graph/math](../tests/snapshot_transform_test.cpp) test sources are reference material, not current CMake targets. Earlier test results do not imply that current CMake still builds them. No Skyrim runtime is available here: native loading, hooks, visuals, framing and graph lifetime remain unverified. See [debugging navigation](code-maintenance.md#debugging-navigation) and [SceneGraph](scene-graph.md).

## Safe modification

Keep target/preset changes synchronized, preserve required dependencies/generation and inspect the actual generated solution after removing entry points. Old generated project files are not supported routes. Do not delete archives or unrelated build-directory contents as a substitute for checking the current target graph.

## Synchronized files

Maintain this guide with [README](../README.md), [SceneGraph](scene-graph.md), [animation](animation.md), [maintenance](code-maintenance.md), CMake/preset/template inputs and [runtime notes](../tools/scene_copy/README.txt). Keep current build commands out of [historical evidence](history.md), whose commands describe earlier revisions.

## Related history

The [historical index](history.md) preserves older probe, test and package records. The current single-DLL setup supersedes their build entry points without changing their recorded outcomes.
