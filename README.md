# PlayerPanel

- Name: PlayerPanel
- Author: AmazingBuff
- Version: 0.1.0
- CommonLib: CommonLibSSE-NG, branch `ng`, consumed as the `extern/CommonLibSSE` submodule

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

The DLL is build/Release/PlayerPanel.dll. CMake configures the project
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
