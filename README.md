# CharacterPanel

Minimal C++23 SKSE plugin: logging, SKSE initialization, exports and generated
metadata. Add configuration, input, UI, rendering or game logic only when needed.

```powershell
git init
git submodule add https://github.com/alandtse/CommonLibSSE-NG.git extern/CommonLibSSE
git submodule update --init --recursive
cmake --preset Release
cmake --build build --config Release
```

Set VCPKG_ROOT and provide a Windows x64 VS2022/SDK environment. The supplied
preset/toolset settings come from the reference's MSVC 14.44.35207 environment;
adjust both preset and target settings together if your compatible toolset differs.
The generated .gitmodules does not itself acquire source or create gitlinks.

The DLL is build/Release/CharacterPanel.dll. CMake configures the project
namespace and metadata in build/src/plugin.h. New source files under src need
reconfiguration with the current glob strategy. main.cpp already owns Load,
Query and Version exports; do not generate duplicates through another helper.

CommonLib needs its own dependency libraries even when first-party source does
not use them directly. Without --features, the plugin does not include SimpleIni, SKSE-MCP, custom
rendering code or shaders. Selected feature packages add only their required
source, dependencies and lifecycle integration. Feature dependencies are
resolved automatically; see the selected feature list below when present.
The render feature is an extension point, not a ready-made drawing effect.

packaging.cmake is an organizational example, not a configured mod package.
Define installation contents when release packaging is requested. No automatic
deployment is performed. See LICENSE and dependency terms before distribution.

## Selected features

config, input, present_hook

## Documentation

- [PlayerPanel PRD](docs/player-panel-prd.md): product requirements for an independent character studio rendered alongside the game world, including composition order and acceptance criteria.
- [Stage-0 spike](docs/stage0-spike.md): historical rendering experiment notes; not validation of the current PRD.
- [M0 diagnostics](docs/m0-diagnostics.md): opt-in bounded engine-evidence probe; the studio renderer remains blocked pending a safe accumulator/culling contract.
- [M0 capture report (2026-09-28)](docs/m0-capture-report-2026-09-28.md): analysis of the first user-run probe session (AE 1.6.1170 + Community Shaders); verifies routine offsets and the accumulator swap path.
- [M0 prototype](docs/m0-proto.md): opt-in one-shot accumulator-swap experiment verifying the report's findings; F6 arms one private menu frame.
