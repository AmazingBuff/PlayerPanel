# M0 accumulator-swap prototype

`CharacterPanelProto.dll` is an opt-in, one-shot rendering experiment for the
blocked M0 studio-renderer contract. It directly verifies the first finding
of the [2026-09-28 capture report](m0-capture-report-2026-09-28.md): the
engine's current accumulator is a plain global pointer slot
(`GetCurrentAccumulator`/`SetCurrentAccumulator`, 16 bytes apart), so a
private frame can run by swapping the UI3D secondary accumulator in,
accumulating the menu scene into a private color/depth target, and
restoring.

The prototype installs one entry detour on `MenuManager::DrawInterfaceStart`
(REL::RelocationID(79947, 82084)) via Microsoft Detours — the same mechanism
as Community Shaders' `stl::detour_thunk` — so the original prologue is
relocated to a trampoline and the thunk can call the true original body.
(A first iteration used a raw `write_call<5>` on the entry instead; the
entry's first five bytes are themselves a `jmp rel32` on 1.6.1170, the raw
patch destroyed them, and the thunk recursed into itself — a stack-overflow
crash the moment the main menu first drew, right after the intro animation.
Detours fixes this by relocating the overwritten bytes.) F6 arms exactly one
frame: on that rendered menu frame the hook swaps the current accumulator to
the UI3D secondary (the one the inventory scene actually renders through,
capture report finding 2), forces `RENDER_MODE::kNormal`, runs the original
menu draw, restores the previous current accumulator, and reads the private
target back to a tone-mapped TGA. No culling-process fields are touched; the
capture report flags `BSCullingProcess` extension offsets as unverified on
1.6.1170.

Unlike the probe, this DLL does render and does patch one call instruction
for the lifetime of the session. It registers input only on the exact AE
1.6.1170 runtime.

## Build

The target is off by default; enable it alongside the probe:

```powershell
cmake -S . -B build `
  -DCHARACTER_PANEL_BUILD_PROBE=ON `
  -DCHARACTER_PANEL_BUILD_PROTO=ON `
  -DCMAKE_TOOLCHAIN_FILE="D:/Microsoft Visual Studio/2022/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows-static-md
cmake --build build --config Release --target CharacterPanelProto --parallel 4
```

The DLL is `build/Release/CharacterPanelProto.dll`; see
`tools/m0_proto/README.txt` for installation and the in-game procedure. It
is never copied to a game or MO2 directory automatically.

## What success looks like

- The swap-frame log line prints `current=<saved> -> secondary=<ui3d
  secondary>`, with the secondary matching the address the probe manifests
  recorded.
- The secondary accumulator's `pass`/`bucket` counters advance during the
  swapped frame.
- `proto-swap-NNN.tga` under
  `Documents/My Games/Skyrim Special Edition/SKSE/CharacterPanelProto`
  shows the highlighted item — the engine accepted a private accumulator as
  current and rendered the menu scene through it.

Any of those failing (black TGA, counters stuck at zero, crash on F6) is
diagnostic evidence for the next iteration; the previous accumulator is
restored before the hook returns, so the engine continues with its own
state.

## Known limits

- The private target binds through the accumulator's own
  `StartAccumulating`, which targets the engine's render targets, not the
  prototype's offscreen texture; whether the output lands in the private
  target or the engine's is exactly what this experiment measures. A black
  TGA with advancing counters would mean the swap works but output routing
  needs the `Renderer::SetRenderTarget` layer (next iteration).
- The dump is a synchronous GPU readback on the render thread: one frame
  hitch on F6, by design, not a steady-state path.
- The old CharacterPanel plugin and CharacterPanelProbe must not run in the
  same session.
