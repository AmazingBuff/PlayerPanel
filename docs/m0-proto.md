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

## v3: persistent panel skeleton (handoff work packages 1+2)

Run 16 closed the evidence chain; v3 (built 2026-10-01, awaiting its game
session) turns the F6 one-shot into the M0 panel skeleton per the handoff's
next-work-package list — no new engine mechanisms, only engineering of the
proven ones:

1. **Panel toggle, persistent bracket.** F6 toggles the panel (FR-05 input
   rules: the sink still only observes its two keys, rejects held/repeat,
   and stays active while InventoryMenu pauses the game). While open, EVERY
   `DrawInterfaceStart` frame is bracketed — the HUD is a menu too, so the
   bracket also runs during normal gameplay, where zero menu passes flow
   and the frame budget is one root snapshot plus counters.
2. **Persistent studio target.** `OffscreenTarget` is held across frames:
   still created lazily from the call site's own RTV/depth description,
   self-recreated whenever that description changes (resolution change),
   and released on the RENDER THREAD at the first non-bracketed
   `DrawInterfaceStart` after close (D3D11 Release is legal cross-thread,
   but the render thread is the only place the target is in flight).
   Session invalidation (FR-06): `kPreLoadGame`/`kNewGame` force-close the
   panel; there is no kToMain message in SKSE v2 messaging, but the main
   menu has no menuObjects content, so a panel left open there renders
   nothing and is closed by the next save load. Creation failures are
   sticky per configuration signature (w/h/color-format/depth-format):
   no CreateTexture2D hammering per frame; retry on panel reopen or desc
   change.
3. **F7 evidence dump.** The per-frame automatic TGA readback is gone (a
   synchronous readback every menu frame violates PRD 5.3). F7 requests a
   one-shot dump, consumed when the current bracket closes. The clear
   still happens lazily at the first replay of each frame, so on
   content-free frames the target keeps its last image (the future
   composite's behavior) at zero GPU cost.
4. **Throttled logging for the persistent frame.** Menu geometries log
   once per panel open (discovery set, cap 256), frame summaries only when
   the replay count changes or every 60 content frames, menu-root
   snapshots only on change, and one heartbeat line per ~1800 bracketed
   frames (~30 s) proving the bracket is alive during gameplay.

What success looks like (run 17 checklist):

- repeated F6 open/close cycles show exactly one "Panel opened" /
  "Panel closed" pair, one target create and one release each — no
  accumulation (FR-06, PRD 5.3);
- with the inventory open and the panel on, every F7 dump shows the
  currently highlighted item, correctly shaded and occluded — the run-16
  gate result sustained across frames and across item changes;
- normal gameplay with the panel open produces only heartbeats and an
  untouched frame;
- loading a save with the panel open logs "Panel force-closed (save
  loading)" plus the release line;
- the world and inventory look normal throughout (the replay remains
  non-destructive).

Compositing (work package 3 — studio output onto the world image before
other UI, PRD §2) is deliberately NOT in this build: it is the next
mechanism to prove, and bundling it would muddy run-17 attribution.

## Run 17 result (2026-10-01): persistent mechanics verified, evidence session incomplete

One session, AE 1.6.1170 + CS installed (site 1 "interposed, chain
restored" as in runs 11–16). What the log proves:

- Seven F6 open/close cycles (five in normal gameplay, two in the
  inventory) with zero crashes, zero [W] lines other than the dump guard,
  and a "studio target released (panel closed)" render-thread line after
  every close — the FR-06 lifecycle works, repeatedly.
- The persistent bracket ran silently through all gameplay windows (one
  root-change line at frame #1: 8 roots, unnamed — correct, they are
  containers) and needed no per-frame logging. Frame arithmetic across
  the seven open windows (~7 s, 327 frames ≈ 40 fps) shows no bracket
  overhead anomaly.
- With the SkyUI inventory open the studio path reproduced the run-14/16
  fingerprints exactly: IronShield:0, shader 6, site 1, engine target
  2560x1440 format 28, post-original SRV set {0,1,5}, `binding survived:
  true`, lighting_replayed=1 — on BOTH panel opens (frames #328 and
  #347), i.e. the replay re-arms across a close/reopen cycle including
  target re-creation.
- The dump guard behaved as designed: all four F7 presses came after the
  panel was closed and were rejected with "Dump ignored: panel is
  closed".

What this session did NOT deliver — run 18 must cover, same DLL, no
rebuild:

1. **Zero TGA evidence.** No `proto-pass-NNN.tga` newer than the v3
   build exists; the visual proof of sustained correct replay is missing.
   Press F7 only while the panel is open (inventory up, item
   highlighted).
2. **Sustained soak.** The panel was open for ~0.5 s per inventory
   stretch; keep it open 1–2 minutes while rotating/highlighting
   different items, with an F7 dump at the start, middle, and end.
3. **Gameplay soak / heartbeat.** The longest gameplay open window was
   ~2.7 s — under the 1800-frame heartbeat threshold. Leave the panel
   open during normal play for a few minutes.
4. **Save-load force-close.** No load happened with the panel open; do
   one save + load with the panel open and expect "Panel force-closed
   (save loading)" plus the release line.

## Run 18 result (2026-10-01): soak and effect-pass discovery verified — and zero TGA again

One session, 16:16–16:20. New verifications:

- **Sustained soak.** One 31-second continuous panel-open stretch in a
  menu (frames #253–1633, ≈45 fps) with per-frame bracketing: stable, no
  crash, no stall. Two more open/close cycles with releases on the
  render thread. The lifecycle now has 11 clean toggle cycles across two
  sessions.
- **Menu-scene effect passes discovered.** The long stretch carried 7
  menu passes per frame with `shader=7` (BSEffectShader: pFireballCore,
  pSparks, lightRays, Glow, PArray, Sphere, Cylinder — particle/glow
  geometry), site 0, numLights=0, alphaTest=true. Identification handles
  them correctly (menu_passes counted), and v3 correctly does NOT replay
  them (lighting only). Consequence for the studio later: glows and
  particles attached to menu objects are effect-shader passes; FR-04's
  transparency/glow requirements will need them replayed with additive
  blending — recorded as work-package-3+ input, not a v3 defect.
- **Replay follows item changes.** Two brief item highlights each
  replayed on their own open (HelmetGO then TorsoGO, shader 6, site 1,
  `binding survived: true`), including target re-creation per open.

Not delivered — again — was a single TGA: **all twelve F7 presses came
after the panel was closed** (twelve "Dump ignored: panel is closed"
lines). Two sessions in a row with the same sequencing shows this is an
interaction-design failure, not user error: the natural flow treats
"close the panel" as "done — capture now", but v3.0 released the target
on close and only honored F7 while open.

**v3.1 (built 2026-10-01) changes the design to match the flow:**

1. **Panel close now writes the evidence.** A user-initiated close dumps
   one synchronous TGA of the last studio image on the render thread
   BEFORE the release destroys the target (`dump_and_release`, gated on
   replays having happened this open — an effects-only open produces no
   dump). Force-closes (save loading / new game) release silently.
   Closing the panel IS the capture button now.
2. **The dump key is demoted to an optional mid-session grab** (still
   requires the panel open; still warns otherwise).
3. **Content-summary cadence fixed**: v3.0 logged a summary every 60
   content frames (run 18: 27 lines in ~38 s — spammy); now both summary
   flavors fire on state change or every 1800 frames.
4. **Hotkeys moved F6/F7 → F7/F8**: F6 is bound in the user's game
   setup, so F7 toggles the panel and F8 is the mid-session grab. The
   probe's F7/F8 never co-load with this DLL, so there is no collision.

Run 19 checklist (same install flow, new DLL):

1. Open the panel (F7) in the inventory, browse to an item with a 3D
   model, close the panel (F7) — expect exactly one `proto-pass-NNN.tga`
   plus "studio dump written" and the release lines in the log.
2. Optional: F8 mid-session while open for an extra frame.
3. Save + load with the panel open — expect "Panel force-closed (save
   loading)" and a release WITHOUT a dump line.
4. Leave the panel open during normal play for a few minutes — expect
   the heartbeat line (~30 s cadence) and no spam.

## Run 19 result (2026-10-01): v3.1 PASSED — the evidence chain is closed

One session, 16:33–16:36, new DLL (v3.1, F7/F8). All core checks green:

- **Close-as-capture works.** Four F7 open/close cycles produced exactly
  four TGAs (`proto-pass-048..050, 052`), each close logging "studio
  dump written" followed by the release line — one dump, one release,
  no accumulation. Zero warnings in the whole session.
- **F8 mid-session grab works.** One request while open wrote
  `proto-pass-051` on the next bracket close.
- **Replay scales and follows items.** IronShield across cycles 1–3;
  cycle 4 (52 s open) browsed TorsoGO, GlovesGO, BrokenAmuletBottom,
  BootsGO, HelmetGO, and two 8-geometry cluster frames (Scroll02,
  SoulGemCommon, BackpackCord, Waterskin, BackpackMain, GemMetal,
  Potion, GemStone — `lighting_replayed=8`, site 1 throughout).
- **Visual verdict (readback analysis + cropped PNGs):** 048/049 show
  the Iron Shield small in the SkyUI preview region; 050 (the 28.5 s
  soak close) shows it zoomed with crisp wood grain, iron rim, boss and
  correct speculars; 052 shows a steel cuirass with leather accents.
  Correct materials, lighting, silhouette — no garbage textures, no
  missing occlusion.
- Install fingerprint unchanged: site 1 "interposed, chain restored",
  engine target format 28, SRV set {0,1,5}, `binding survived: true`.

Observations and residuals:

- Menu fps (from bracketed-frame arithmetic) varied widely: ~11 fps in
  cycle 3 (1 replay/frame), ~3.4 fps for 46 s of cycle 4, then ~54 fps
  for its last second — including frames replaying 8 passes. The 54 fps
  burst rules out replay cost as the bottleneck; the slow stretches are
  most plausibly game-side (streaming / SkyUI list work). Proper CPU/GPU
  timing belongs to PRD 5.3's measurement stage, not this prototype.
- Not exercised this session (same mechanisms are verified elsewhere):
  the save-load force-close (the release machinery it calls ran four
  times here, and the CAS guard ran in runs 17/18) and the gameplay
  heartbeat (log cadence only).

**Verdict: handoff work packages 1+2 (persistent bracket, persistent
studio target, panel lifecycle) are complete and game-verified. The next
implementation target is work package 3 — compositing the studio target
onto the world image as an opaque rectangle before other UI (PRD §2).**

## v4: composite (work package 3, built 2026-10-01, awaiting run 20)

The studio output finally lands on screen. The composite runs at the
`DrawInterfaceStart` detour ENTRY, before the original menu draw —
whatever the panel draws is under every other UI element, which is the
PRD §2 order (stage 2 before stage 3). Per-frame sequence with the panel
open: composite the (previous frame's) studio image → bracket opens →
menu passes re-render the studio target → bracket closes. One frame of
content latency at the entry point; irrelevant for a menu preview.

Mechanics:

- **Quad**: an SV_VertexID triangle pair (no vertex buffers, no input
  layout) placed by a one-float4 constant buffer holding the panel rect
  in NDC; the pixel shader samples the studio target's new SRV opaquely
  (blend disabled, depth test off). Shaders are runtime-compiled with
  the native d3dcompiler (linked via CMake; no blob-type coupling into
  REX).
- **Panel rect**: fixed screen fraction 58–88% × 12–68% (top-left
  origin), the whole studio target squeezed into it. Framing/zooming the
  item is studio-camera work (M1), not composite scope. Calibration
  constants, logged at first use.
- **State footprint**: only what the quad sets is saved and restored —
  blend, depth-stencil state, PS/VS shaders, PS slot 0 (SRV, sampler,
  CB), VS CB 0, IA topology. OM render targets are untouched (the quad
  draws into whatever is bound); the engine rebinds the rest for its own
  draws.
- **Guards**: no composite before the studio target exists (first
  replay); a feedback guard skips the draw if the bound RTV is our own
  studio RTV; draw counter logs at #1 and every ~1800 draws.

What run 20 answers (PRD §7 question 2 territory):

1. what target is actually bound at DrawInterfaceStart entry (logged
   once as "composite target at DrawInterfaceStart entry" — run 12 only
   identified the target DURING the menu pass);
2. whether the rectangle is visible and sits UNDER SkyUI widgets
   (tooltips, cursor, item card) as PRD §2 requires;
3. whether the world and the menu are unharmed (A01's "next frame no
   pollution" pre-check) and whether closing the panel removes the rect;
4. how the composite interacts with CS/ENB/ReShade final passes in this
   load order (the rect should receive those effects with everything
   else, per PRD §2 stage 4).

Run 20 checklist:

1. Open the panel (F7) in the inventory with an item highlighted: a
   visible opaque rectangle appears (right side of the screen) showing
   the item; SkyUI widgets render above it.
2. Close the panel (F7): the rectangle disappears, the usual evidence
   TGA is written, world and inventory look normal.
3. Walk around with the panel open (world visible): the rectangle stays
   put over the world; the world moves normally behind/around it (A01
   core behavior, camera part pending the menu-context caveat).
4. Log check: "composite ready" + target-identity + draw-counter lines;
   no errors/warnings.

## Run 20 result (2026-10-01): mechanism works, routing wrong — entry target is a non-visible intermediate

One session (18:18–18:20, v4). The composite pipeline ran exactly as
designed — "composite ready", the identity line, "composite draw #1",
clean close-as-capture dumps (053–057, with content-free closes
correctly dumping nothing) and no crash — but **no rectangle appeared on
screen**. The diagnostic line decided it:

> composite target at DrawInterfaceStart entry: 2560x1440 **format 24**
> (R10G10B10A2_UNORM)

The target bound at DrawInterfaceStart entry is a different, non-visible
intermediate — the visible menu preview draws into the format-28 UI
composite DURING the bracket (runs 11–19), not into whatever is bound at
entry. The user's screenshot adds a load-bearing observation: the item
image inside the modded UI's item card sits exactly where the TGA
bounding boxes place the replayed item (x 56–65%, y 29–45% of the
frame) — i.e., the format-28 target's content demonstrably reaches the
screen.

**v4.1 (built 2026-10-01, run 21 pending) reroutes the composite:** at
each replay the hook captures the call-site RTV (AddRef; recapture on
pointer change — the pool reallocates on resolution change; released
with the studio target on panel close), and the entry composite binds
THAT target (OM + viewport now in the save/restore set), draws the quad,
restores. The quad still runs before the original menu draw, so the
panel still lands under other UI *within that target*; the item preview
and SkyUI widgets draw after it by engine order.

Run 21 checklist (new DLL):

1. Open the panel (F7) in the inventory with an item highlighted: an
   opaque rectangle (58–88% × 12–68% of the screen) should now appear,
   showing the studio image, under the item card/tooltip/cursor.
   Overlap with the item's own 3D preview is expected and fine (both
   live in the same target; layout is M1+).
2. Log: "panel composite target captured: 2560x1440 format=28" — if the
   captured format is not 28, capture the log; the routing assumption is
   falsified.
3. If the rectangle is STILL invisible with format 28 captured and
   draws advancing: the merge of the format-28 composite onto the screen
   must be region-gated (stencil/scissor in the merge pass) — the next
   probe records that merge, not another blind reroute.
4. Everything from v4 unchanged: close removes the rectangle + writes
   the TGA; world/menu unharmed; F8 mid-session grab works.

## Run 21 result (2026-10-01): format 28 confirmed — and the target is a per-frame rotating instance

One session (18:32–18:33, v4.1). The capture worked exactly as designed:
`panel composite target captured: 2560x1440 format=28` — the call-site
target IS the UI composite. Draws ran, close-captures produced TGAs, no
crash — and the rectangle STILL did not show. The load-bearing new fact
is in the log: **five capture lines ~0.35 s apart**. The capture fires
only when the pointer changes, so the format-28 composite is NOT a
stable pool slot — **the engine rotates composite instances every menu
frame**. The entry-time draw necessarily painted the PREVIOUS frame's
instance; the engine's merge reads the current one. Invisible by
construction.

**v4.2 (built+deployed 2026-10-01, run 22 pending)** moves the composite
from the DrawInterfaceStart entry to the bracket close (end_frame, right
after the original menu draw returns — still before the frame's UI
merge), drawing into the instance captured during THIS frame's replays,
gated on `lighting_replayed > 0` so a stale cross-frame capture is never
used. The stale-instance hold shrinks from one frame to microseconds.

Run 22 checklist:

1. Same procedure (F7 open with an item highlighted): the rectangle
   should now appear during menus, under the item card/tooltip/cursor.
2. Known M0 scope change: the rectangle shows only on menu frames with
   replayed content (it draws at bracket close). A zero-replay menu
   stretch (effects-only) will blank it for those frames; gameplay still
   shows nothing — gameplay-path compositing is a separate, later
   evidence step (the gameplay merge path differs from the menu one).
3. If STILL invisible with v4.2 draws advancing: the merge must be
   region-gated (stencil/scissor only letting the preview region
   through) — the next probe records the merge pass state, not another
   reroute.
4. Everything else unchanged: close removes it + writes the TGA; world
   and menu unharmed; log shows `v4.2 ... captured` + `v4.2 composite
   draw #N`.

## Run 22 result (2026-10-01): INVALID — v4.2 never drew (integration bug)

Session 18:41–18:43 (zoomed-preview state included): captures fired
(format 28, one per fresh instance), replays ran, close-captures wrote
TGAs (064–074) — but **not a single "composite draw" line**. Code review
found why: moving the composite from the entry path to end_frame left
`ensure()` behind in the deleted function, so `draw()` early-returned on
null shaders every frame. v4.2 was never a test of the end-frame timing.

**v4.3 (built+deployed 18:59/19:00, MD5-verified against the MO2 copy)
adds the `ensure()` call in end_frame.** Run 23 re-runs the run-22
checklist unchanged: with the panel open on a menu frame with replayed
content, expect `composite draw #N` lines to finally appear and the
58–88% × 12–68% opaque rectangle on screen; if draws advance and the
rectangle is still invisible, the merge is region-gated and the next
probe targets the merge pass.

## Run 23 result (2026-10-01): Draw called, zero pixels — the quad was back-facing and culled

v4.3 ran correctly (session 19:05–19:08): `composite ready`, captures
(format 28, per fresh instance), exactly one `composite draw #1` line
(heartbeat not reached), replays and dumps all green — and STILL no
rectangle. With draws confirmed and nothing rasterized, the remaining
suspect was rasterization state, and re-deriving the winding exposed a
day-one bug: **the viewport transform flips NDC y, so the quad's
`{(-1,-1),(1,-1),(-1,1)}` order is COUNTER-clockwise in window space —
back-facing — and the engine's CULL_BACK rasterizer state silently
culled every quad since v4.1** (Draw succeeds, no error, zero pixels —
consistent with all three failed routing runs; the replay path is
unaffected because engine passes bind their own state, and the TGA
readback is a CopyResource, not a rasterized draw).

**v4.4 (built+deployed 19:15, MD5-verified) fixes it with belt and
braces:**

1. the corner order is corrected to clockwise in window space;
2. the composite binds its OWN rasterizer state (CullNone,
   ScissorOff, Solid) with save/restore — immune to whatever cull or
   scissor state the engine leaves bound;
3. one diagnostic line logs the previously bound rasterizer's
   cull/scissor at composite time (confirms the hypothesis per session).

Run 24 checklist: identical to run 23. This is the first run where the
quad can actually rasterize; if the rectangle now appears, the M0
composite mechanism is proven and the remaining work is placement/
lifecycle polish; if draws advance and it is STILL invisible after a
cull-proof draw, the merge-pass region-gating hypothesis is the only one
left standing.

## Run 24 result (2026-10-01): culling hypothesis FALSIFIED by its own diagnostic

v4.4 ran (19:18–19:20): install line v4.4, `composite ready`, captures
(format 28, one per fresh instance), one `composite draw #1`, replays and
close-dumps green — and no rectangle. But the new diagnostic line
retracted the v4.4 diagnosis instead of confirming it:

> rasterizer bound at composite time: **cull=1** scissor=0

`cull=1` is `D3D11_CULL_NONE` — the engine binds cull-OFF at composite
time, so the v4.3 quad was never culled; it rasterized fine, and the
v4.4 winding "fix" (predicted cull=2/CULL_BACK) was hygiene, not the
cure. Honest retraction: the back-face culling story is dead.

What survives: pixels rasterize into the last-replayed instance at
end_frame; the item's pixels in that SAME instance are visible on
screen; the panel drawn microseconds later into it is not. Two
explanations remain: **(a)** the merge happens INSIDE the original call
— before end_frame; **(b)** format-28 is not the visible path at all
(the on-screen preview would come from an unhooked render; the TGA
position match proves nothing about target identity — any render of the
same scene through the same camera lands at the same pixels).

## v4.5 (built+deployed 19:29, MD5-verified): the decisive placement

The composite moved INTO the replay hook: immediately after the replay
call returns and the engine's OM is restored, the quad is drawn into
`prev_rtv` — the call-site instance, still bound, containing the item's
pixels the passthrough wrote microseconds earlier. The logic is
airtight: nothing engine-side runs between the passthrough's return and
our draw (it is all our code), so if the visible image is produced from
this instance, it is produced AFTER our draw — the quad must appear. The
falsified end_frame composite is removed; the capture stays as evidence
(it logs the call-site format each session).

Run 25 decision tree:

1. Install line must read `M0 proto v4.5 panel composite installed`.
2. F7 open on an item: `Proto v4.5 composite draw #N` lines should
   appear per replay, and the 58–88% × 12–68% rectangle should finally
   show (under UI drawn later in the same frame — §2-compliant).
3. **If visible**: the M0 composite mechanism is proven; remaining work
   is placement polish, lifecycle, and A01.
4. **If draws advance and it is STILL invisible**: format-28 is not the
   visible path — the on-screen preview is rendered somewhere we do not
   hook. Next step (run 26): a Present-hook diagnostic composite (draw
   the studio image onto the backbuffer at Present — guaranteed
   visible) to put the content on screen while the engine's real UI
   merge path is mapped separately.

## Run 25 result (2026-10-01) + v4.6: the user's RenderDoc capture finds the blend bug

v4.5 ran (19:34–19:36): in-replay draws executed (one `composite draw`
line), the engine's rasterizer at composite time read cull=3 (CullFront
— varies by point, irrelevant under our own CullNone state) — and still
no rectangle. Instead of a sixth blind run, the user captured the frame
with RenderDoc and found the real problem: **the composite target is
consumed through the UI alpha-composition pipeline, so the quad must be
BLENDED over the existing layer content — v4.x wrote it with blending
disabled, an opaque overwrite that breaks the layer's composition
semantics.**

**v4.6 (built+deployed 20:19, MD5-verified) fixes the blend:** the
composite quad now uses standard non-premultiplied over (SrcBlend=
SrcAlpha, DestBlend=InvSrcAlpha, alpha channel One/InvSrcAlpha) instead
of the disabled blend. A new per-session diagnostic logs the ENGINE's
own blend state bound at composite time (same pattern as the run-24
rasterizer line): if the pipeline expects a nonstandard convention (e.g.
inverted alpha), the next log pins it exactly instead of another guess.

Run 26 checklist:

1. Install line = `M0 proto v4.6 panel composite installed`.
2. F7 open on an item: the 58–88% × 12–68% rectangle should appear
   (blended, under later-drawn UI per §2); F7 close removes it + TGA.
3. If still nothing: compare the logged `engine blend at composite time`
   values against what RenderDoc shows for the engine's OWN draws onto
   that target — the correct blend configuration is then copied verbatim
   rather than inferred.

## Run 26 result (2026-10-01): PANEL VISIBLE — the M0 composite works

The user's screenshot (20:27, v4.6 with RenderDoc attached) shows the
M0 panel on screen for the first time: a black opaque rectangle exactly
at 58–88% × 12–68% of the frame, with the studio image inside it (the
Iron Shield), the world and character rendering normally around it, and
the UI list untouched on the left. Session log green throughout:

- `engine blend at composite time: enable=0 src=2 dest=1` (ONE/ZERO —
  the engine's own bound state at that point is an opaque write; the
  precise layer-alpha semantics that made the over-blend write visible
  where the opaque write was not are left for later study — the user's
  RenderDoc finding stands as the empirical fix);
- `composite draw #1800` and `#3600` heartbeats — the panel drew every
  bracketed frame across the session (~3600+ draws) with no crash, no
  stall;
- 3 close-capture TGAs — the evidence path is intact alongside the
  visible panel;
- captures keep confirming the call-site target (format 28).

Known placement facts for the polish pass (M1, not blockers):

- the rectangle currently OVERLAPS the modded item card (it covers the
  card's right portion) — the card draws before the composite point, so
  the panel sits above it; PRD §2 wants other UI above the panel, which
  needs either an earlier composite point for the panel or a layout that
  avoids the card (M1 placement work);
- the studio image is squeezed whole into the rect (the item appears at
  its preview position scaled down) — framing/zoom is the M1 studio
  camera work.

**Verdict: handoff work package 3 (compositing) is complete. The M0
panel prototype now renders a persistent, non-destructive, correctly
shaded studio view as an opaque on-screen rectangle. Remaining M0 work:
the formal A01 acceptance run (world + panel + camera movement +
next-frame cleanliness) and the paused-scenario placeholder (package 4,
fixed pose).**
