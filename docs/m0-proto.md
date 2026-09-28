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

## Run 1 result (2026-09-28, user session)

The Detours hook ran clean: five armed frames across one session, swap and
restore both stable, no crash, five TGAs written. The swap itself was
**inert**: the saved current accumulator (a scene accumulator, distinct from
both UI3D slots) came back unchanged, and the secondary's `pass`/`bucket`
counters stayed at zero through every swapped frame — all five TGAs are
100% black.

Byte-level analysis of the probe's captured code explains why:
`Inventory3DManager::{Begin3D,Render,End3D}` make **zero** calls into
`BSShaderAccumulator::{Get,Set}CurrentAccumulator` (Begin3D's six direct
calls and Render's three helpers all go elsewhere). The inventory 3D path
drives its accumulators directly and never consults the engine's global
current-accumulator slot; that global belongs to the world-scene path
(`Main::RenderWorld` family) outside the menu draw. Capture-report finding 1
remains correct as a statement about the global slot itself, but swapping it
cannot redirect the inventory scene.

Next-step options, in cost order:

1. **Drive the secondary accumulator directly** inside the swap frame:
   `secondary->StartAccumulating(ui3d camera)` → cull the UI3D scene →
   `FinishAccumulating`, bypassing `Inventory3DManager` entirely. All entry
   points are CLib-bound and verified by the probe.
2. Hook the two unbound helpers `Inventory3DManager::Render` calls
   (module-relative `0x9287B0`, `0x928A60` on 1.6.1170) to interpose the
   accumulator selection; requires new address-library IDs.
3. Identify the third Render call target `0xD1BF70` (suspected scene/cull
   dispatch) before choosing 1 vs 2.

## Run 2: direct accumulator drive

Run 1's byte-level evidence (inventory path bypasses the global slot) led to
run 2: the armed frame now drives the secondary accumulator directly —
`Renderer::StartAccumulating(camera, secondary, 0)`, the shared UI3D culler's
`Process2` over each live `menuObjects` root with `useVirtualAppend` forced
on, then `secondary->FinishAccumulating()`. The private color/depth target
stays bound around the sequence and is restored (with the current
accumulator) before the real menu draw runs. The log line now reports the
cull's visible-geometry count alongside the secondary's `pass`/`bucket`/
`active` state; the TGA readback follows. All entry points are CLib-bound
and probe-verified.

## Run 3: stage-separated measurement

Run 2 forced `useVirtualAppend=true`, which leaves the visible array empty
by design — the logged `visible=0` said nothing about the cull. Run 3 flips
to the classic Ni flow so each stage is measurable: cull with virtual append
OFF (the array collects geometries; the new `stage A` log line reports the
count), then `RegisterObjectArray` ingests the array into the secondary, then
`FinishAccumulating` draws. The done-line now also prints the UI3D camera's
world translate to test the stale-camera hypothesis (the engine updates
camera world data during its own cull, which runs after ours).

## Run 4: frustum-plane initialization

Run 3's `visible=0` with camera world at origin pointed at the culler's test
planes: the Process2 disassembly reads the camera's `viewFrustum`
(camera+0x150, matching the captured `add rdx, 0x150`), but the culler's own
planes (base+0x3C) are only ever filled by `SetFrustum` in the engine's
per-frame path. Run 4 calls `culler->SetFrustum(&camera->GetRuntimeData2()
.viewFrustum)` (CLib ID 69699/71081) before the cull and logs the frustum
values plus the first root's world-bound radius as stage-A0 diagnostics.

## Run 5: world-data update before cull

Run 4's A0 diagnostics nailed it: frustum sane, `root0_bound_radius=0` — the
menu objects' world bounds are empty because the engine updates world
transforms/bounds in its own update pass, after our cull. Run 5 calls
`UpdateWorldData` on every root before `SetFrustum`/`Process2` and logs any
bound-radius change as confirmation.

## Run 6: subtree census + engine downward pass

Run 5's `UpdateWorldData` alone changed nothing (no bound updated, radius
still 0). Run 6 additionally runs the engine's own `UpdateDownwardPass` on
each root, then censuses the first three roots' subtrees — node count,
geometry count by RTTI name (TriShape/Geometry/Particles), max world-bound
radius, and node names — so the next log shows where the item geometry
actually lives and whether any bound is populated after the downward pass.

## Run 7: cull bypass — direct geometry feed

Run 6's census found the geometry: root[1] carries 22 nodes / 14 TriShapes
with populated child bounds (max radius 37.4), the other roots are empty
containers — yet Process2 still culled everything. Run 7 drops the shared
culler entirely: the swap frame walks each root's subtree, collects the
geometry leaves straight into the visible array (skipping app-culled ones),
and hands the array to `RegisterObjectArray` → `FinishAccumulating`. This
isolates stage B+C: if the accumulator ingests and draws geometry it is
handed, the remaining problem is exactly the culler (whose extension layout
is the capture report's standing unverified-layout caveat).

## Run 8: drive the primary accumulator (kNormal dispatch arm)

Run 7 fed 7 geometries into the secondary every frame and still got zero
passes. The probe manifests contain the reason: the secondary's idle
render_mode is 12 (kShadowMask), and FinishAccumulatingDispatch is a table
dispatch on renderMode — the geometry went into a shadow path that drew
nothing. Run 8 switches to the primary accumulator (idle render_mode 0 =
kNormal), forces `RENDER_MODE::kNormal` explicitly before ingestion, then
StartAccumulating → RegisterObjectArray → FinishAccumulating as before.
