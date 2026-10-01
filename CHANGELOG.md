# Changelog

## Unreleased

### Changed

- Land the visible panel (v4.6 confirmed by run 26): with the blend fix
  from the user's RenderDoc capture, the studio image now appears as the
  M0 opaque rectangle — exactly at 58-88% x 12-68% of the frame, world
  and character rendering normally around it, ~3600 composite draws
  across the session with no crash, and 3 close-capture TGAs alongside.
  Work package 3 (compositing) is complete; remaining M0 work is the
  formal A01 acceptance run and the fixed-pose paused scenario. Known
  M1 polish items: the rectangle currently overlaps the modded item card
  (card draws earlier, so the panel sits above it — PRD section 2
  ordering refinement), and the studio image is squeezed whole into the
  rect (framing is M1 camera work).
- Enable alpha blending on the composite quad (v4.6, run 26 pending,
  from the user's RenderDoc capture): the composite target is consumed
  through the UI alpha-composition pipeline, so the quad must be BLENDED
  over the existing layer content — the disabled-blend overwrite used
  since v4.1 broke the layer's composition semantics. The quad now uses
  standard non-premultiplied over (SrcAlpha/InvSrcAlpha), and a
  per-session diagnostic logs the engine's own blend state bound at
  composite time so a nonstandard convention can be copied verbatim from
  the log/RenderDoc instead of inferred.
- Move the composite into the replay hook (v4.5, run 25 pending): run 24
  falsified the culling diagnosis with its own diagnostic line — the
  rasterizer bound at composite time is cull=1 (CullNone), so the v4.3
  quad was never culled and the v4.4 winding "fix" is retracted. What
  survives: pixels rasterize into the last-replayed instance at
  end_frame, the item's pixels in that same instance are visible on
  screen, and the panel drawn microseconds later is not — the merge
  happens inside the original call (before end_frame), or format-28 is
  not the visible path at all. v4.5 draws inside the replay hook, right
  after the replay returns and the engine's OM is restored, into
  prev_rtv while the call-site instance is still bound — nothing
  engine-side runs between the passthrough's return and our draw, so a
  visible-image merge must read the instance after it. The falsified
  end_frame composite is removed; the capture stays as evidence.
- Fix the composite quad being silently culled (v4.4, run 24 pending):
  run 23 proved Draw was called every bracketed frame with replays while
  zero pixels rasterized. Root cause: the viewport transform flips NDC
  y, so the quad's original corner order is counter-clockwise in window
  space — back-facing — and the engine's CULL_BACK rasterizer state
  culled every quad since v4.1. v4.4 corrects the winding to clockwise,
  binds the composite's own rasterizer state (CullNone, ScissorOff) with
  save/restore, and logs the previously bound rasterizer's cull/scissor
  once per session as evidence.
- Move the panel composite to the bracket close (v4.2, run 22 pending):
  run 21 confirmed the captured call-site target is the format-28 UI
  composite, but the log showed five recaptures ~0.35 s apart — the
  engine rotates composite instances every menu frame, so the v4.1
  entry-time draw painted the previous frame's instance while the merge
  reads the current one. v4.2 draws at end_frame (right after the
  original menu draw returns, before the frame's UI merge) into the
  instance captured during THIS frame's replays, gated on
  lighting_replayed > 0 so a stale cross-frame capture is never used;
  v4.3 adds the missing ensure() call (v4.2 shipped without it and never
  drew at all). The rectangle is therefore expected on menu frames with
  replayed content; gameplay-path compositing is a later, separate
  evidence step.
- Reroute the v4 panel composite (v4.1, run 21 pending): run 20 proved
  the composite mechanism (shaders, state save/restore, draw, clean
  close-captures 053-057) but the rectangle never showed — the target
  bound at DrawInterfaceStart entry is a format-24 (R10G10B10A2) non-
  visible intermediate, not the format-28 UI composite the visible menu
  preview draws into. v4.1 captures the call-site RTV (AddRef, recapture
  on pointer change, released on panel close) during replays and binds
  that target for the entry composite; OM and viewport join the
  save/restore set. If format 28 is captured and the rectangle is still
  invisible, the next probe targets the merge pass's region gating, not
  another reroute.
- Add the v4 panel composite (handoff work package 3, built 2026-10-01,
  run 20 pending): while the panel is open, the DrawInterfaceStart entry
  draws the studio target (new SRV) as an opaque rectangle — SV_VertexID
  quad with runtime-compiled shaders, blend/depth off — into the bound
  target BEFORE the original menu draw, so the visible panel sits under
  every other UI element per the PRD composition order. Fixed panel
  rect (58-88% x 12-68% of the screen) with the whole studio image
  squeezed in (framing is M1 camera work); only the quad's own state is
  saved and restored; a feedback guard skips drawing over the studio
  target itself. Run 20 answers what target the entry binds, whether the
  rectangle shows under SkyUI widgets, world/menu cleanliness (A01
  pre-check), and the CS/ENB/ReShade relative behavior.
- Turn the M0 prototype from the F6 one-shot into the v3 persistent panel
  skeleton (handoff work packages 1+2, built 2026-10-01): F6 toggles the
  panel and, while open, every DrawInterfaceStart frame is bracketed so
  menu-scene lighting passes keep the studio target current every menu
  frame; the offscreen target is persistent (created from the call site
  description, self-recreated on resolution change, released on the
  render thread on panel close, and force-closed on save loading / new
  game per FR-06); the per-frame TGA readback is replaced by an opt-in
  F7 one-shot dump (PRD 5.3: no steady-state GPU readback);
  persistent-frame logging is throttled (per-geometry discovery lines,
  state-change summaries, slow heartbeat). Run 17 verifies the toggle,
  release, silent-gameplay-bracket, and reopen-replay mechanics; run 18
  adds a 31-second soak, effect-shader menu-pass discovery, and
  item-change tracking — but both sessions produced zero TGA because
  every F7 press came after panel close. v3.1 fixes the interaction
  instead of the operator: closing the panel with F6 now writes one
  evidence TGA from the studio target before the render-thread release
  (force-closes and content-free closes write nothing), the dump key is
  demoted to an optional mid-session grab, the content-summary cadence is
  fixed to state changes / ~30 s, and the hotkeys move F6/F7 to F7/F8
  (F6 is bound in the user's game). Run 19 passes the v3.1 checklist:
  four close-captures and one mid-session grab produced five TGAs whose
  readbacks show correct materials and occlusion across seven item
  geometries and an 8-pass cluster frame, over 28.5 s and 52 s soak
  stretches — closing handoff work packages 1+2.
- Retire the falsified accumulator-swap route in the M0 prototype and add
  the v2 pass-redirection frame: F6 now brackets one DrawInterfaceStart
  frame, the three RenderPassImmediately call-site hooks log every pass in
  the bracket, and menu-scene BSLightingShader passes (geometry descending
  from a UI3DSceneManager menuObjects root) are non-destructively replayed
  into a private kMAIN-format offscreen target that is read back to
  proto-pass-NNN.tga.
- Record the v2 run-9 session: menu-pass identification and call-site
  interception verified (IronShield pass, menu=true, site 1), but replays
  landed zero pixels. Runs 10-13 then peeled the onion: depth/stencil
  rejection falsified (state logged disabled), the shadow-state
  re-application proven (raw bind lost on the first call, honored on the
  second), the Community Shaders chain collision fixed (site 1 was
  interposed; the pre-patch E8 target is now captured and restored), the
  SRV stomp caught by snapshots and repaired — and the remaining dim-arc
  result showed the pre-call replay order itself was the problem. v2.5
  inverts the order: the passthrough draws first, then the hook binds the
  private target and calls once more, reusing the original's own pipeline
  state with no capture or restore machinery at all. Run 14 passes the M0
  gate: the Iron Shield renders completely and correctly shaded into the
  private target every armed frame, non-destructively, closing the M0 core
  evidence chain for the pass-redirection route. v2.6 adds a private
  depth buffer with an explicit depth-on state after the user spotted the
  handle rendered over the boss (depth test bypassed without a DSV); run
  15's target-creation failure (the engine's depth resource reports a
  typeless format) is fixed by normalizing to the typed depth format, and
  run 16 verifies the handle is correctly occluded.

### Added

- Add an opt-in engine evidence probe for the experimental studio renderer.
- Add the 2026-09-28 M0 capture report analyzing the first user-run probe
  session; verifies contract routine offsets on AE 1.6.1170 under Community
  Shaders and locates the engine's current-accumulator swap path.
- Change the probe capture hotkeys to F7 (code + menu) and F8 (menu only);
  F9 stays reserved for quick load.
- Add the opt-in M0 accumulator-swap prototype (CharacterPanelProto): one
  DrawInterfaceStart entry detour (Microsoft Detours) and an F6-armed single
  frame that swaps the UI3D secondary accumulator in as the engine's current
  accumulator, then restores and dumps a tone-mapped TGA readback. A first
  raw write_call-based hook recursed into the patched entry and crashed the
  main menu; the detour relocates the original prologue instead.
