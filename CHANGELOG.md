# Changelog

## Unreleased

### Changed

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
  stretches — closing handoff work packages 1+2. Compositing (work
  package 3) is the next build.
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
