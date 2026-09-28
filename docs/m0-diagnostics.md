# M0 diagnostics

`CharacterPanelProbe.dll` is a separate, opt-in SKSE diagnostic package for the
blocked M0 studio-renderer experiment. It collects bounded evidence from Skyrim
AE 1.6.1170 while the user has an inventory 3D model visible. It does not draw a
panel, install a renderer or Present hook, create actors, attach scene objects,
change cameras or lights, change inventory, or persist game state.

The current blocker is the missing safe contract for constructing an independent
`BSCullingProcess` and submitting an accumulator into a private color/depth
target. The local declarations expose `BSCullingProcess::CullingContext` and
`Process`, `BSShaderAccumulator::Create`, and the `Renderer` accumulator entry
points, but they do not establish independent initialization or custom-target
state ownership. The original M0 implementation remains unchanged and is not
implemented or game-validated.

## Build and package

The probe is opt-in and leaves the normal `CharacterPanel` target unchanged.
Configure with the existing Release toolchain and the local vcpkg path, then
build the probe target:

```powershell
cmake -S . -B build `
  -DCHARACTER_PANEL_BUILD_PROBE=ON `
  -DCMAKE_TOOLCHAIN_FILE="D:/Microsoft Visual Studio/2022/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows-static-md
cmake --build build --config Release --target CharacterPanelProbe --parallel 4
cmake --build build --config Release --target CharacterPanelProbePackage --parallel 4
```

The DLL is `build/Release/CharacterPanelProbe.dll`. The local package is
`dist/CharacterPanelProbe-1.0.0.zip` and contains:

- `SKSE/Plugins/CharacterPanelProbe.dll`
- `CharacterPanelProbe-README.txt`
- `build-manifest.txt` with the DLL SHA-256 and contract identity

The package is a local MO2-format ZIP. It is never copied to a game or MO2
directory automatically.

## Controls and capture data

The probe registers its input sink at `kDataLoaded` only when the runtime is
exactly AE 1.6.1170. F8 captures the menu snapshot and bounded code evidence;
F9 captures menu/object metadata only. `ButtonEvent::IsDown()` rejects held and
repeat events, and the callback remains active while the game is paused by
`InventoryMenu`.

Captures are written to the standard SKSE log directory under
`CharacterPanelProbe/capture-NNN`. Existing capture folders are preserved across
process launches. Each manifest records the runtime, trigger, menu state,
window dimensions, UI3D object/camera/light counts, culler and accumulator
fields, code limits, module-relative addresses, bounds, status, and errors.

F8 resolves only the contract-listed native routines: the culling-context and
process entry points, accumulator constructor/current-accumulator entry points,
renderer accumulator entry points, `Inventory3DManager::Render`,
`Inventory3DManager::{Begin3D,Render,End3D}`,
`UI3DSceneManager::{AttachChild,DetachChild}`, `NiObject::Clone`, and
`MenuManagerDrawInterfaceStart`. For live UI3D culler and both primary and
secondary accumulator objects it records the declared vtable slots and attempts
the same bounded code capture. Function bounds use `RtlLookupFunctionEntry` where
available. Leaf or cap-truncated reads are marked partial. Reads require a
committed, readable executable PE section and use checked `ReadProcessMemory`;
the limits are 64 KiB per routine and 1 MiB per capture. SkyrimSE.exe,
CommunityShaders.dll, and CharacterPanelProbe.dll are eligible for code bytes;
other module targets remain metadata-only.

If a singleton, object, vtable entry, unwind range, module, or capture directory
is unavailable, the manifest records the missing evidence and continues without
dereferencing an arbitrary object graph. Disk writes occur after the UI3D lock
is released. A capture is `complete` only when all requested bounded evidence
is complete; unavailable, partial, or failed evidence is reported explicitly.

## User scenarios

Run both scenarios on the target machine and return the capture folders,
`build-manifest.txt`, and the probe log. Do not redistribute executable snippets.

1. Disable the old `CharacterPanel` plugin, run the original renderer, enter a
   save, open SkyUI inventory, highlight an item with a visible 3D model, press
   F8 after the model appears, close the inventory, press F9, and exit normally.
2. Repeat the same sequence with Community Shaders 1.9.1 enabled, recording the
   CS version and any unavailable or redirected function metadata.

Screenshots are optional. These scenarios are user-run and remain unverified in
this repository.

## Safe modification and cleanup

Keep `tools/m0_probe/probe_main.cpp` limited to runtime gating, input and SKSE
exports. Keep bounded memory and manifest logic in
`tools/m0_probe/probe_capture.cpp`; do not add render calls, hooks, actor
creation, or broad memory scans. The synchronized artifacts are the probe
sources, CMake opt-in target, packaging script, package instructions, this
document, the README link, and the Unreleased changelog entry.

To uninstall the diagnostic, remove the local ZIP and any manually installed
`CharacterPanelProbe.dll`; no game or MO2 path is modified by the build. The
historical `docs/stage0-spike.md` and the PRD remain unchanged.
