# Changelog

## Unreleased

### Added

- Locate the binding inside a binding-set element instead of guessing it (S2
  animation handoff, HKX4). The round before proved the element is neither an
  `hkaAnimationBinding` nor a pointer to one, and that the character's animation
  names are typed and already in the binding set's order, so this round tries a
  table of offsets with both readings and judges every row structurally: the
  animation has to look like a live object with a plausible clip length, and its
  tracks have to name bones this skeleton has - no more tracks than bones, since
  a transform track belongs to one bone. Every row reports why it was rejected
  (`unreadable`, `no-animation`, `tracks-unreadable`, `duration`, `track-bones`)
  and says whether the element's first word looks like a vtable at all, the first
  0x40 bytes of two elements are dumped raw so a layout no row matches can still
  be read off by hand, and the plain idles - file names starting with "idle",
  which the path-sorted list buries under weapon and object idles - get their own
  line with their indices.

### Changed

- Read the character's animation list and validate how its bindings can be read
  (S2 animation handoff, HKX3). The names are typed
  (`hkbCharacterStringData::animationNames`), so a binding index can be named
  without guessing, and the round reports how many names mention an idle and
  where they sit. The binding elements are not typed in this checkout, so both
  candidate layouts - the element being the binding, and the element starting
  with a pointer to one - are judged structurally on a spread of indices instead
  of being believed: the candidate's animation pointer has to look like a live
  engine object (its vtable inside the game's own image), its duration has to be
  a plausible clip length, and every one of its tracks has to name a bone the
  skeleton has. Every read goes through a committed-and-readable page check and
  nothing virtual is called on a candidate, so a misread pointer reports
  nonsense instead of crashing the game. The idle indices get a line of their
  own, where a name, a duration and a full track table can be read side by side.

### Changed

- Replay the source character's own pose into the copy and measure how far it
  lands from it (S2 animation handoff, HKX2). The engine already holds that pose
  in `hkbCharacter::poseLocal`, so the write path the sampling route will use
  can be verified without a clip, a binding set, or any structure whose order is
  unknown: bone to node by exact name, Havok's quaternion to `NiMatrix3`, the
  local write, and one downward world recompute shared with the studio's other
  drivers. Every capture poses the copy four ways - the pose indexed by the
  animation skeleton and by the engine's `boneNodes` table, each with the
  rotation and with its transpose - and prints the worst bone-to-source distance
  for each, next to a control that first poses the copy half a radian away from
  the source: without that control, a zero distance would only say the
  measurement is insensitive. The verdict names the candidate that reproduces
  the source, so the engine's own indexing and quaternion conventions are
  measured rather than assumed, and the captured pose is put back afterwards,
  with its own measurement as proof that the panel is as it was found. The
  conversion itself is unit-tested (a quarter turn about Z taking +X to +Y, a
  half turn about X taking +Y to -Y, the inverse undoing the rotation, and a
  pose sample read out of Havok's SSE quads with position, all four quaternion
  components and its scale).

### Changed

- Report how the engine's animation skeleton maps onto the copy, at capture
  (S2 animation handoff, step 1 of the HKX1 work package). F7 now also reads
  the source character's animation graphs read-only and prints what each one
  holds - whether it drives the captured third-person root, its bone-node table
  against the animation skeleton, `poseLocal`, the size of its binding set and
  the class of its root generator - then resolves every bone of the animation
  skeleton against the copy's nodes by exact name and prints
  `SCOPY ANIM gate verdict=PASS|PASS-AMBIGUOUS|INCOMPLETE|UNAVAILABLE` with the
  matched and ambiguous counts plus a sample of what did not resolve. The
  skeleton's own parent relation is reported but does not gate the mapping:
  this rig's spine hangs under CME UBody while its pelvis hangs under CME
  LBody, so those two hierarchies disagree by design, and bones resolve to
  nodes by exact name because loose matching is what drove the wrong skeleton
  in the S2 probe. The rules are pure functions with their own unit test
  (exact match only, a duplicated node name resolved once and flagged, the
  report list capped while the counts stay complete, a cyclic parent table
  terminating). Nothing is written to the source character, to the copy, or to
  the engine's graphs; sampling an animation into the copy is the next step.

### Changed

- Record the S2 animation direction change: the procedural idle proved that
  continuously writing the copy's bones drives it, but it has no ground truth,
  so "does it look right" was unfalsifiable and the pelvis feedback could only
  be answered by tuning numbers. The next work package drives the copy from
  real animation data instead - the engine's own Havok sampling pipeline
  (hkaAnimation::SampleTracks, hkaAnimationBinding's track-to-bone map,
  hkbClipGenerator, the character's animation skeleton, BShkbAnimationGraph's
  bone-node table) - compared numerically against the source character at the
  same phase rather than by eye. The idle stays as a fallback for bodies with
  no usable animation and is not to be tuned further. Plan, evidence pointers,
  failure surfaces and the A/B protocol: docs/s2-animation-handoff.md.

### Fixed

- Give the hips a translation channel and report each driven joint's travel.
  The first in-game look at the idle found the waist apparently motionless
  while everything else moved, and that reading was correct twice over: in
  this skeleton the pelvis hangs under CME LBody while the spine hangs under
  CME UBody, so the two are siblings and a pelvis rotation cannot carry the
  torso; and a rotation about a joint's own origin does not move that joint at
  all, which is all the hips had. The hips now shift sideways (+-0.9 studio
  units), tilt a little further, and every two seconds the log reports how far
  each driven joint has travelled from the captured pose next to the rotation
  its channels ask of it, so "does the waist move" is answered with numbers
  instead of an impression. A unit test now requires at least one translation
  channel, because a rotation-only table is exactly how this regressed.

### Changed

- Move the procedural idle to F2 and retire the S2 measurement probe from the
  product build. F6 is bound by another plugin in the target setup, and the
  probe had already answered its questions over four recorded rounds, so F2
  now toggles the idle while the probe becomes a diagnostic build behind
  -DCHARACTER_PANEL_S2_PROBE=ON (which puts it on F6 and leaves F2 to the
  idle there too). With the option off, the probe's hotkey and its per-frame
  measurement stages are compiled out, and the load banner lists only the keys
  the build actually binds.

### Added

- Give the copy a procedural idle (PRD FR-03's first driver): hips roll
  and turn, the spine counter-rolls and breathes, and the head looks
  around on its own slower cycle, driven from a steady clock we own because
  the engine's timer does not advance while the panel is paused. Channels
  are resolved by exact joint name against the copy's own skeleton and
  reported as `SCOPY IDLE bound N/M channels`, so a body whose skeleton
  differs says so instead of silently animating nothing. Every frame
  recomputes from the captured pose (CharacterClone::pose restores it
  first), so nothing accumulates and F6 off returns the figure to the
  captured pose on the next frame. It runs only while the panel is drawn,
  only writes the copy's nodes, and yields while the F2 probe is armed.
  Amplitudes and periods live in src/character/idle_driver.h and the axis
  convention (X lean, Y screen roll, Z turn; Z is up) is stated there.
- Share apply_world_delta_downward and add rotate_about_own_origin in
  snapshot_transform.h: the idle driver and the probe drive joints the same
  way, and the subtree recomputation (which the engine's dirty-update
  cascade does not perform inside the draw window) should exist once.
- Fourth in-game probe round (2026-10-09, S2P4) and the two answers it
  produced. The skin matrix buffer's 3x3 is the bone's world rotation times
  the graph's 0.35 scale - a skinning matrix with the scale baked in - which
  is why the previous round's ten unit-norm candidates all missed by ~2.0;
  the layout itself (3x4 row-major, translation at 3/7/11) was right all
  along. For the bone the probe swings, the slot matches that bone's CURRENT
  world transform, not the captured pose (residual 0.0002), so the engine
  rebuilds the skinning data from the nodes we write during the draw: the
  CPU half of R05 holds, and with the round's paired off/on observation
  (swings when armed, resets and stays still when not) the chain from node
  write to screen is closed. The same dump found the boundary that matters
  for any future write: slot i is NOT bones[i] in general (5/31 and 4/71
  self-matches; one skin's 71 slots hold only 16 distinct matrices), which is
  why writing bones[i]->world into slot i never moved anything in the
  earlier rounds. Analysis scripts and log:
  docs/diagnostics/analyze_tslot_*.py, docs/s2-anim-probe-evidence-2026-10-09.md.
- Dump the target skin's slots raw, once per F2 arm: every slot's twelve
  floats next to its bone's name and that bone's own world transform, for both
  matrix buffers. The third round compared ten engine-side candidates and
  every one of 1132 samples missed by 1.97-2.64, with the "nearest" rotating
  at random among eight of them - so the candidate set was never the problem
  and the reading layout (or the slot's indexing) is. Two columns of raw
  numbers settle what distances could not.
- Third in-game probe round (2026-10-09, S2P3): 566 measurement sets over one
  CBBE and one UBE capture, no crash. The operator's paired observation closes
  R06: with F2 on the figure swings, with F2 off it resets and stays still. The
  two matrix buffers classify identically, so a shader reading the other buffer
  is not what made the relation labels split 50/50; the slot's contents remain
  unidentified, which is what S2P4's raw dump is for.
- Sample both skin matrix buffers and classify the slot against ten
  engine-side candidates (bone world, captured pose, previous frame's swing,
  each transposed, the bind transform, and the world/bind products), printing
  the nearest two with their distances. The first two rounds compared a single
  candidate pair, so a slot holding some third quantity looked exactly like a
  coin flip: at maximum swing amplitude the labels split 50/50, which rules out
  the simple reading and leaves the buffer's contents unidentified. Reading the
  engine's second buffer covers the remaining cheap explanation, a shader that
  samples the other one on some frames. Measurement stays read-only; unit tests
  now assert a rotation and its transpose stay distinguishable.
- Second in-game probe round (2026-10-09, S2P2): 241 measurement sets over
  one CBBE and two UBE captures, no INVALID reading, no crash, every capture
  parked on F4. The swing now reads back as the designed amplitude
  (1.3-34.4 degrees against a 0.6 rad design), the witness moves in all 241
  sets, and the engine rewrites the sampled skin matrix slot during the draw
  in all 241 - so the engine-side rebuild is active on the drawn copy. The
  slot's contents still cannot be identified: at maximum swing amplitude the
  relation labels split 50/50, which rules out the simple "the slot follows
  the node write" reading, so the CPU half of R05 stays open. Log and
  analysis: docs/s2-anim-probe-evidence-2026-10-09.md.
- Measure where the probe joint sits in the skeleton and how much of the
  figure it can move. The parent chains prove the CBBE/UBE difference seen in
  the first round: the pelvis hangs under CME LBody while the spine hangs
  under CME UBody, both branching from CME Body, so the two branches are
  siblings and a pelvis swing cannot carry the torso. affected-geoms
  quantifies it per body (10/23 for the pelvis, 43/46 for the spine).

### Fixed

- Read the probe's orientation from the relative rotation's trace and its
  slot relation from the whole 3x3 rotation block. The first measurement
  build compared matrix row 0, which is exactly the row a world-X swing
  leaves untouched, so it reported orientation-delta=0 and "neither" for a
  swing the game visibly applied and left the CPU-buffer question
  unreadable. Unit tests now assert the X/Y/Z reading and that row 0 is
  invariant under an X swing.
- Drop the probe target when a capture replaces the drawn graph. It used to
  survive the re-capture, so after loading another save the probe kept
  swinging (and reporting numbers for) the parked copy while the panel drew
  the new one: 136 of 564 measurement lines in the 2026-10-09 session
  described a graph that was not on screen. The T2 line no longer reuses a
  stale pass name: it prints not-submitted when the target geometry's pass
  did not arrive.
- Pick the probe joint across every skin instead of the first one found, and
  report the chosen joint's parent chain and how many geometries the swing
  touches (affected-geoms). In the 2026-10-09 session the CBBE run picked
  the body skin's pelvis while the UBE run picked the face skin's spine, so
  the two bodies visibly swung different halves of the figure — a target
  selection artifact, not an anchor or per-body difference.

### Added

- Land the S2 probe's first in-game result (2026-10-09): with the probe
  armed, the copy's figure visibly follows the bone write on both CBBE and
  UBE bodies. The archived log has 141 of 141 measurement sets reporting
  probe-effective, witness displacements up to 14.1 units, and the engine
  rewriting the skin matrix slot during the draw in every one of them. What
  remains unproven is whether that slot follows our write and what the GPU
  binds; the HKX driver is not started. Log and analysis:
  docs/s2-anim-probe-evidence-2026-10-09.md.
- Add the T0-T3 measurement chain the S2 probe needed to be judgeable
  (PRD 0.6 §6.3): each frame records the baseline after the studio's base
  placement, the swing applied to one bone, the pass that submits the
  target geometry, and the state after the draw, then prints the four
  stages together with the frame number, the bone's world matrix and the
  skin matrix slot read BEFORE the swing. The slot is sampled read-only
  and compared against this frame's two candidate node rotations, so no
  verdict rests on a value the same code wrote. A witness on the swing
  axis is reported as INVALID instead of being read as "nothing moved".
- Give every probe round its own build identity: CMake now generates
  Plugin_Build_Identity from CHARACTER_PANEL_PROBE_REVISION plus both
  source baselines and the configure time, the manifest carries the same
  string, and the package step rejects a DLL that does not contain it.
  The archived S0 rounds all logged one hardcoded identity, so a stale
  deployment could not be told from a fresh build.

### Changed

- Run the S2 probe inside the draw window, after pose() and before pass
  generation. It used to run before draw(), whose first act is pose()
  restoring every captured node world, so the write was undone before the
  engine prepared anything and the probe's own logs predated the
  overwrite — the archived runs could not show whether the write ever
  reached the draw. The stage calls now live in CharacterClone::draw(),
  which is the only place that owns the order.
- Change node transforms only in this probe round: the skin matrix buffer
  rebuild is gone, because the read-back that once classified that buffer
  compared the slot against the value the same code had just written into
  it. write_bone_matrix stays for the later, evidence-gated step.
- Swing about world X and pick the witness furthest from that axis: Z is
  the bone's own long axis, so the old swing spun the witness in place and
  a flat displacement reading could not distinguish a failed write from a
  witness sitting on the rotation axis.
- Land the S0 scene-graph-copy route (CHARACTER_PANEL_SCENE_COPY_EXPERIMENT),
  game-validated on 2026-10-08: F7 copies and audits the player's third-person
  graph through native NiObject::Clone, F8 draws it into the studio, F3 rotates
  the copy without touching the world actor, F4 releases it. Both the copy/draw
  lifecycle test and the re-equip/appearance/physics test pass on CBBE and UBE
  bodies with no crash across capture, release, recapture, load and main-menu
  transitions; frozen hair and cloth in the copy is the documented S0 scope, not
  a failure. Validation record and per-question results:
  docs/scene-graph-copy-results-2026-10-08.md.
- Park every replaced or released snapshot for the process lifetime instead of
  destroying it. Freeing a native clone's graph crashed at every point tried
  (crashes 2026-10-08 23-01-11 in capture(), 23-07-35 at a clean frame boundary
  from the retired queue, 23-12-02 at the main-menu boundary), each time inside
  BSFadeNode's destructor through the tbb allocator; the copy path therefore
  holds its graphs open the way the retired Actor route held its shell alive.
- Prune non-character objects from a captured graph before framing or drawing:
  particle geometries, skeleton-driven collision helpers (3BCA_* parts,
  VirtualGround, CollisionStopper), script-spawned markers, blood and flash
  visuals, and the captured spill lights. A heavily modded player graph carried
  123 geometries and 106 passes, of which only the weapon ever reached the
  screen; after pruning it is 51 and 47 and the figure renders complete.
- Exclude skeleton-driven collision helpers from the studio framing bound: they
  share the body's skeleton and sit far enough from it to drag the bound and the
  centering off the figure, which is what pushed the UBE body out of frame.
- Add the scene-graph-copy test package target (CharacterPanelSceneCopyPackage,
  opt-in with CHARACTER_PANEL_SCENE_COPY_EXPERIMENT): it stages the DLL with its
  PDB, the S0 install/test README, and a JSON build manifest carrying both
  binaries' SHA-256, the repository HEAD, the actual extern/CommonLibSSE checkout
  and each worktree's dirty state, so a result returned from the validating
  machine can be tied to the artifact that produced it. The
  CHARACTER_PANEL_SCENE_COPY_EXPERIMENT option configures the experiment
  tree (there is no Release-scopy preset); the manifest takes its configuration from
  $<CONFIG> rather than assuming Release.
- Add a source-graph drift report (SCOPY SOURCE-DIFF) that separates a changed
  object set from per-node transform drift. The previous exact float comparison
  rejected captures whose nodes had moved by a few ULPs, which blocked the CBBE
  body across the pause; only a changed object set blocks a capture now.
- Log the first few F3 presses and the build's hotkey map in the SCOPY BUILD
  banner, so a stale deployment is distinguishable from a dropped key.

### Changed

- Record the M0 engineering-side acceptance (docs/m0-acceptance.md):
  7 criteria over runs 17-26 — 6 pass (visible composite with correct
  content, content tracking across items/zoom, toggle lifecycle without
  resource accumulation, non-destructive frames, FR-05 input rules,
  reproducible evidence chain), 1 partial (the save-load force-close
  MESSAGE trigger is code-ready but not yet game-verified — folded into
  phase 2's first round along with the formal PRD 5.3 timing). The
  panel-rect INI is deferred by user decision; phase 2 (the independent
  display instance P) is now the current phase.
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
