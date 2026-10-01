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

## v2: menu-scoped pass redirection (runs 9+)

Run 8 was equally inert (capture report addendum 2), closing the
accumulator-swap route: the menu path bypasses the global slot, CLib's
culler layout does not match the 1.6.1170 runtime, and neither UI3D
accumulator ingests foreign geometry through the documented API. The v2
prototype retires the swap frame entirely and combines the two mechanisms
the investigation proved in-game:

1. The `DrawInterfaceStart` Detours detour (unchanged) brackets exactly one
   rendered menu frame per F6 press.
2. Pass hooks on the three `RenderPassImmediately` call sites (the stage-0
   spike's mechanism, the same sites Community Shaders hooks; private
   64 KiB trampoline) observe every pass flowing while the bracket is
   open. The v2 hooks are **non-destructive**: after the observation or
   replay, the original `SetupAndDrawPass` call still runs, so the visible
   menu frame is untouched — the M0 requirement this round is only that
   menu passes *can* be captured into a private target, not that they are
   diverted yet.

Menu-pass identification: a pass whose `geometry` descends from one of the
eight `UI3DSceneManager::menuObjects` roots (run 6 located the highlighted
item under root[1]) is a menu-scene pass. The parent chain walk is bounded
at 32 levels; root pointers are snapshotted when the bracket opens.
v2 replays only BSLightingShader passes (shaderType 6 — the spike showed
depth/shadow passes write no colour); every other pass is counted and
logged for the next iteration.

Replay mechanics: the spike's reconciled raw-bind approach — save the
current OM targets and viewport, bind a private kMAIN-sized target
(R11G11B10_FLOAT color + own D24S8 depth), clear once per frame, call
`SetupAndDrawPass` on the pass with its original arguments, restore. Later
passes depth-test against earlier ones, so occlusion inside the item is
preserved. When the bracket closes, the target is read back to a
tone-mapped TGA (`proto-pass-NNN.tga`, previously `proto-swap-NNN.tga`).

What success looks like:

- the bracket log lists the menu roots and per-pass lines with
  `menu=true` for the highlighted item's geometry;
- `lighting_replayed` advances past 0 and the TGA shows the item,
  correctly shaded;
- the inventory item and the world look unchanged after the bracket.

If no `menu=true` pass appears while the bracket is open, the menu scene
does not submit through the three hooked call sites — the per-pass log is
the diagnostic that decides the next move. A black TGA with
`lighting_replayed>0` isolates the problem to replay shading (constant
buffers / technique stack), the spike's round-2 territory.

## Run 9 result (2026-10-01): identification proven, replay landed zero pixels

Six F6 arms in one session (user run, AE 1.6.1170). Every bracket caught
exactly one pass: `IronShield:0`, `menu=true` (ancestry match against the
menuObjects roots works), shader=6 (BSLightingShader), passEnum 0x48000035,
numLights=2, flowing through **call site 1** — so menu-item passes do
submit through the hooked call sites, and the v2 identification rule
produced zero false positives across the whole frame. All six replays ran
and all six readbacks (`proto-pass-000..005.tga`) were **100% pure black** —
not dim, not clear-color noise: the draw landed zero pixels.

Root cause investigation, updated 2026-10-01: v2.1 first attributed the
black readbacks to the inventory pane's stencil mask (v2 bound a private
D24S8 depth target; the spike's proven replay config binds none), so v2.1
returned to the no-DSV configuration and logged the inherited
depth-stencil state. The follow-up session (run 10) **falsified that
hypothesis**: the replays were still 100% black, and the logged state
showed `depthEnable=0, stencilEnable=0` at the call site — no depth or
stencil rejection can be the cause. Leading theory now: on the menu path
the shadow state carries dirty flags into the direct call, and
`SetupAndDrawPass` re-applies the engine's own render targets inside it,
so the draw lands on the engine's target (the on-screen shield is then
drawn twice with identical opaque pixels — invisible), while the private
target keeps its clear color. The spike's world-path state was clean at
its call sites, which is why its raw bind survived.

v2.2 tests this in the same session: the replay calls once, queries
`OMGetRenderTargets` to see whether the raw bind survived (logging the
survivor's pool identity and dimensions when it did not), and — when the
engine took the targets back — re-binds ours and calls a second time: the
first call consumed the dirty flags, so the second should honor the raw
bind and land in the private target. Per-frame diagnostics also log the
call site's engine target identity and scissor/rasterizer state. If the
second call still cannot land pixels, the log identifies which pool target
the engine insists on and the routing moves to the engine's own
`Renderer`/shadow-state path instead of raw binds.

## Run 11 result (2026-10-01): shadow-state re-application confirmed, shield geometry lands, textures garbage

The theory was confirmed line-by-line. `scissorEnable=0` ruled out the last
raster rejection; the engine target at the call site is 2560x1440 format 28
(R11G11B10 — the main HDR); **the raw bind did not survive the first call
and did survive the second** — the consume-and-retry works. The readback
(`proto-pass-012..017.tga`) shows the Iron Shield with correct silhouette,
position (the SkyUI preview region, right side of the frame) and lighting
shape — but its surface is tiled magenta/blue noise instead of the iron
texture, the classic signature of wrong texture bindings / garbage UV
coordinates.

Leading cause: **hook-chain collision with Community Shaders**. CS is
installed (with LightLimitFix 3.1.0, the feature that patches exactly these
three call sites), and our `write_call<5>` overwrote its patch — so both
the thunk's passthrough and the replay called vanilla `SetupAndDrawPass`
directly, bypassing CS's interposer; CS-replaced shaders then draw with
stale constant buffers (garbage UV/texture indices) while engine-side
state keeps geometry correct.

v2.3 fixes the chain and adds the texture-level evidence: at install time
each site's E8 rel32 is parsed BEFORE patching and the pre-patch target is
stored (logged against SetupAndDrawPass — "interposed, chain restored"
means CS had hooked it); the passthrough and both replay calls now run
that true original. Per-frame PS SRV snapshots (slots 0–9: pointer,
dimension, format) before the replay and before the second call make any
remaining texture garbage attributable. If the chain restoration fixes the
textures, the M0 core evidence is complete: menu passes can be captured
into a private target with correct shading.

## Run 12 result (2026-10-01): chain collision confirmed and fixed; SRV snapshots expose the last stomper

The install log confirmed the collision exactly where it matters: **call
site 1 — the only site the menu pass uses — had been interposed by
Community Shaders before us** (`pre-patch target ... interposed, chain
restored`; sites 0 and 2 were unhooked), and v2.3 restored that chain.
The textures were still garbage, but the SRV snapshots caught the
remaining culprit red-handed:

- before the replay (the caller's set for THIS pass): slots {4,5,6};
- after the first call: slots {0,1,5} — completely different resources.

So the same shadow-state re-application that reclaims the OM also stomps
the pixel-shader texture slots with a stale set; the retry then draws the
correct geometry with the wrong textures — the tiled noise. The snapshots
also corrected a format assumption: the call site's target is format 28
(R8G8B8A8_UNORM, the UI composite), not kMAIN's R11G11B10.

v2.4 closes the loop: the caller's 16 PS SRV slots are captured (AddRef)
before the first call, restored before the retry, and left in place for
the thunk's passthrough (which runs next with the dirty flags spent);
the snapshot set the engine left is logged and released. The private
target is now sized/formatted from the call site's own RTV description.
If the retry now draws the shield with its own textures, the M0 core
evidence is complete.

## Run 13 result (2026-10-01): SRV restore worked — and exposed the whack-a-mole; order inverted for v2.5

The v2.4 readback no longer shows noise (smooth surface — the restored
texture set is the right one), but only a thin dim arc of the shield lands
(0.23% coverage, max value 41/255): the pre-call replay order is a losing
race against the shadow-state re-application, one slot family at a time —
OM was reclaimed in run 11, the SRV set in run 12, and the constant-buffer
state is evidently next. Restoring every family around a pre-call replay
reproduces the shadow state by hand; the structure is wrong, not the
details.

v2.5 inverts the order: the replay now runs **after** the original call
(the thunk's passthrough draws first — consuming the dirty flags and
leaving every pipeline slot exactly as the pass was drawn with — and only
then does the hook bind the private target and call again). A post-
original replay needs no state capture or restore at all: the second call
re-draws 1:1 with the original's own state, our render target being the
only difference. The whole capture/restore machinery (caller SRV set,
post-call snapshots, survive-check/retry ladder) is deleted; the armed
frame's only footprint is one extra draw into the private target.

## Run 14 result (2026-10-01): gate PASSED — the shield renders correctly into the private target

Six F6 arms, six clean captures. The post-original replay's raw bind
survived every frame (`binding survived: true` — no dirty flags left to
consume), the replay inherited exactly the texture set the original drew
with ({0,1,5}, logged per frame), and the readbacks
(`proto-pass-032..041.tga`) show the **complete Iron Shield with correct
materials**: wood-grain shield face, iron rim, boss and rivets, correct
lighting and silhouette, full brightness range, at the item-preview
position. The menu frame itself rendered normally (non-destructive by
design) and the session ended without errors.

This closes the M0 core evidence chain for the pass-redirection route:

1. menu-scene passes are identifiable (geometry ancestry vs menuObjects
   roots) with zero false positives across whole frames;
2. they flow through the three RenderPassImmediately call sites, which can
   be hook-shared with Community Shaders by restoring the pre-patch chain;
3. a menu pass can be re-drawn into a private offscreen target with
   correct shading by calling the original again after the engine's own
   draw — one extra draw, no state ownership fights.

The next stage (M0 panel prototype) builds on this: consume/duplicate the
menu-scene passes into a studio target composited as an opaque rectangle
before other UI, per the PRD's composition order (section 2). The run-12
position discrepancy is also resolved: the pre-call order's stale
constants had misplaced the shield; the post-original order lands it where
the item preview actually sits.

## v2.6: private depth buffer (user-reported handle occlusion artifact)

The user reviewing the run-14 readback spotted the one known v2.5
limitation made visible: the handle appeared clamped onto the front boss.
The replay bound no DSV, so the pass's depth test was bypassed entirely
and triangles landed in draw order — the back-mounted handle overwrote
the boss it sits behind. v2.6 adds a private depth buffer (format matched
to the engine's bound depth view), clears it once per armed frame, binds
an explicit depth-on state (LESS_EQUAL, write-all — the call site's own
state has depth testing disabled per run 10, and the post-original state
is not guaranteed to have it on), and restores the engine's state after
the replay.

Run 15 (first v2.6 build) failed to create the target on every armed
frame: the engine's depth resource reports a TYPELESS format
(e.g. R24G8_TYPELESS), and a new texture created with that format cannot
back a default-desc depth view. The fix normalizes the extracted format
to its typed depth counterpart (R24G8_TYPELESS → D24_UNORM_S8_UINT,
R32G8X24_TYPELESS → D32_FLOAT_S8X24_UINT, R32/R16_TYPELESS likewise,
typed depth formats kept, anything else falling back to D24S8) and logs
the failing step with its HRESULT.

Run 16 (2026-10-01): depth occlusion verified. Six arms, six clean
replays (`binding survived: true`, no creation failures), and the
readbacks show the complete Iron Shield with the handle correctly hidden
behind the shield face — the run-14 artifact is gone. The M0 core
evidence chain now covers PRD FR-04's intra-item occlusion requirement
alongside identification, capture, and correct shading.
