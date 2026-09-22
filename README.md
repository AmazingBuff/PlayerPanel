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

`cmake --preset Release` needs the `builtin-baseline` pinned by vcpkg.json to exist in
your vcpkg clone. When it does not, configure with an explicit `-S`/`-B` command plus
`-DVCPKG_MANIFEST_INSTALL=OFF` and `-DVCPKG_INSTALLED_DIR=<installed dependency tree>`;
the "Tests and verification" section of docs/features/preview-actor.md records the exact
configure and build commands used for the current verification run.

The DLL is build/Release/PlayerPanel.dll. CMake configures the project
namespace and metadata in build/src/plugin.h. New source files under src need
reconfiguration with the current glob strategy. main.cpp already owns Load,
Query and Version exports; do not generate duplicates through another helper.

The panel's chrome is loaded from `Data/Interface/PlayerPanel/panel.swf`. A skin supplies the panel's
**opaque backdrop and its frame**: the character is alpha-blended over the backdrop and inset by
`PanelSkinInsetFraction` of the panel height, so keep the frame at least that thick, and give the skin an
opaque backdrop because a transparent one lets the world show through. The repository's generated
default skin is `assets/PlayerPanel/panel.swf`, produced and structurally re-parsed by
`python tools/make_panel_swf.py --verify`; a reskin mod replaces the installed file at that path, and
deleting it makes the panel fall back to its built-in fill and hairline. Nothing is deployed
automatically: copy the DLL and the SWF into the game's `Data` tree yourself.

CommonLib needs its own dependency libraries even when first-party source does
not use them directly. Without --features, the plugin does not include SimpleIni, SKSE-MCP, custom
rendering code or shaders. Selected feature packages add only their required
source, dependencies and lifecycle integration. Feature dependencies are
resolved automatically; see the selected feature list below when present.
The render feature is an extension point, not a ready-made drawing effect.

packaging.cmake is an organizational example, not a configured mod package.
Define installation contents when release packaging is requested. No automatic
deployment is performed. See LICENSE and dependency terms before distribution.

## Documentation

- [Feature documentation](docs/features/README.md): one canonical document per feature module.
- [Preview actor](docs/features/preview-actor.md): the M1 preview character — creation, supported
  appearance and worn-equipment mirroring, destruction, and the manual in-game checklist.
- [Preview panel](docs/features/preview-panel.md): the M1 on-screen window — offscreen rendering with
  a private target, its own camera and fixed lighting, sizing relative to the render height, a backdrop
  and frame both loaded from a swappable `Interface\PlayerPanel\panel.swf` through a registered menu
  with the character alpha-blended over them and inset by `PanelSkinInsetFraction`, dragging with the
  game's own menu cursor and a saved normalised position, the built-in fallback chrome, the skin
  generator, the panel configuration keys, and the manual in-game checklist.
- [CHANGELOG.md](CHANGELOG.md): user-visible changes.
- [Design documents](../docs/README.md): the product PRD and the CommonLibSSE-NG references.

## Selected features

config, input, present_hook, frame_hook, preview_actor, panel_menu, panel_renderer, panel_geometry, shader_manager, d3d11_util
