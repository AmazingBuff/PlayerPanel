CharacterPanel scene-copy / AFP development package

Use this package's CharacterPanel.dll and PDB together. Read build-manifest.json
and SCOPY BUILD to confirm build identity, source baseline, runtime and hashes.

Current trial: oar-idle2 (2026-10-16). Panel-only Random conditions and weighted
variants retain/reset their results per loop settings. Sequential and PlayOnce
history advances when a proposed asset starts, not when a cancelled query runs.
Each idle layer follows its own cycle. A newer weapon target cancels older
pending requests and interrupts an unfinished transition from the displayed pose;
an old completion must never return to an old weapon's idle. Check query channel /
generation, equipment target / stale proposal, phase and transition completion.
Test A -> B -> C/D before B finishes, mixed-duration idle layers, existing Random
and sequence packs, slow loading and recapture during a transition. Return the log,
manifest and pictures. Source notes: docs/oar-preview-query-2026-10-16.md.

Previous trial: oar-preview1 (2026-10-16). The copy queries OAR 3.1.6/3.2.1 on an
independent clip and loads standing bases. The global eight-frame window and
current-player-clip fallback are gone. Check SCOPY PANEL query / asset ready /
phase lines. Test sneaking, jumping, swimming and mounted opens, then drawn-weapon
and armor changes. Finite weapon transitions return to idle; armor changes refresh
the copy with a short pose transition. Return the log, manifest and pictures.
Current notes: docs/oar-preview-query-2026-10-16.md. Older rounds below are history.
F11 only controls the retained puppet diagnostic, not the copy's query isolation.
Only normal bindings are accepted; unsupported conditions/assets fall back to an
eligible candidate or standing base. Native loading and detours need game testing.

2026-10-15 two-handed weapon fix: a two-handed weapon on the figure could crash the game
(crash 2026-10-15 22:45:27, BiPolar Blade, BipedAnim reading a null biped object). The
engine reports a two-handed weapon in whichever hand entry it last touched and the
reading flips, so the per-entry hand reconcile unequipped and re-equipped it every frame
and the prune took it away while the hand poll put it back. The hand pair is now
normalised with the two-handed weapon, both hand rules ask "is this item held" instead of
"does this entry hold it", the prune is held off on a frame where the source's hand
reading just moved, and a churn guard (more than four hand actions in ten polls) holds the
hand reconcile off for ninety polls with a `hand reconcile churning` line instead of
hammering the equip manager. New local check `two-handed hand state`.

2026-10-15 OAR condition hooks and preview projection: the plugin carries a
verified binary profile for Open Animation Replacer 3.1.6 (the build installed
here) next to the earlier 3.2.1 one, and installs its two condition hooks only
when the loaded DLL's plugin version, PE timestamp/image size, CodeView identity
and both entry bytes all agree (`OAR 3.1.6 hooks installed: ...`; a refusal names
the rejected version fields). While the paused panel steps the figure's
behaviour graph, that step runs inside a projected preview scope, so OAR's own
replacement decision for the figure is evaluated as a standing preview: identity
and equipment stay the puppet's (it wears the player's), the state predicates
answer as standing.

  F11: the projection's on/off arm. The panel reports in both arms:
       `SCOPY OAR project ticks=... hooks=... mode=... evaluated=N overridden=M
       differing=D top=[Name D/M, ...]`, where `evaluated` counts the condition
       evaluations the projection saw, `overridden` those it answered, and
       `differing` those whose answer disagreed with the engine's own answer for
       that SAME evaluation - the direct measure of "the projection changed an
       input OAR decides on". Once both arms produced clips the log adds
       `SCOPY OAR ab projected-clips=[...] plain-clips=[...] sets-equal=yes|no`;
       `sets-equal=no` is the direct evidence that the projection changed the
       selection. A standing figure at rest is expected to show `differing` near
       zero - that is what "no visible change" looks like in the numbers, not a
       failure. Selection, animation loading, variant session isolation,
       transition playback and figure re-equipping are still OAR's own; this
       module only changes the state its evaluation sees. Notes: docs/oar-hooks.md.

2026-10-15 UBE transparency trial (ube-alpha1): both figure sources use native
per-mesh alpha blending/testing, opaque-before-alpha order and read-only depth
for blended surfaces. Game result: the brow/eye-shadow mesh-sized black patches
are gone (makeup renders correctly, user-confirmed); rotation, head-equipment
changes and the post-panel game view are still unconfirmed. The log reports
`FIGURE transparency ... render-flags=0x204` on audited draws. Full notes:
docs/ube-transparency-test-2026-10-15.md.

2026-10-16 CS material trial (cs-view1): requires the verified official Community
Shaders 1.9.1 DLL for the material backend. Private GBuffer, readable depth, native
CS SSS (Separable/Burley), native material composite and a fixed studio reflection
environment precede the existing transparent stage. No world history updates are
dispatched. Watch for `CP-CS material view`, not just the hook-installed message.
Local WARP and official shader checks passed; game rendering is unverified.
Full version identities, scope and test steps: docs/community-shaders.md.
Next: verify native engine stages with manual CS compensation disabled; if the
candidate fails or evidence is inconclusive, retain this cs-view1 implementation.
The engine-path diagnostics are planned, not included in this DLL. Decision and
acceptance steps: docs/engine-render-path-validation-2026-10-16.md.

Current controls:
  F7: capture/audit player third-person 3D in paused InventoryMenu; draw starts off.
  F8: toggle drawing of the accepted copied graph.
  F2: AFP mark (final output slots, calibration, camera, geometry, source comparison).
  F4: the AFP drive cycle, starting from `auto`. `auto` is the product state: the
      paused panel forces the generate it needs to have a pose at all, while the
      running world does not (the mapping proof needs a frame the engine both
      generated and applied). The five fixed states after it - off, tick,
      tick+apply, tick+apply+force, step - are the diagnostic instrument, and the
      two adjacent `apply` states A/B force-generation alone. Step advances the
      puppet once by 1/60 second. F3 stays with the other mod that owns it.
  F6: retained puppet direct-draw diagnostic, OFF by default; the current copy
      route does not spawn a puppet.
  F11: retained puppet OAR projection on/off. Default on. It does not disable
       independent-query isolation in the current copy route.

2026-10-15 review fixes: new games request the puppet as well as loaded saves;
parking compares the CURRENT reference position, and dressing waits for binding
and stable assembly again. Hand mirroring preserves hand slots and same-form
dual-wield quantities. OBody reconciliation removes target-only values and
handles a cleared source preset (`copied=0` can be a successful clear). Animation
tracks and skeleton indices are separate; F7 recapture rebinds target nodes.
Unavailable final-pose input retries on running frames; a rejected mapping still
blocks writes. These lifecycle/equipment behaviors still require game validation.

AFP9 additions: the apply stage reconciles its coverage ledger per frame
(attempted = written + unreachable; output-slots = attempted + unprocessed) and names the
unreachable/unprocessed slots against the animation skeleton, classified by the
puppet's own skin palette (necessary / helper / weapon-attachment / unbound
palette). R01 direct draw samples a fixed bone set before the passes and after
the batch (A4/A5) and prints a full framing-restoration reconciliation; physics
and FaceGen remain out of scope.

AFP10 additions - three things the panel figure now does that it did not before:

1. Nothing visible for the first seconds. The shell must still load beside the
   player (the engine picks the character LOD by reference distance at load), so
   the assembly window is spent with the fade pinned to ZERO and it now closes as
   soon as the census says the shell is bound and settled, instead of after a
   fixed 240 frames. Check `SCOPY PUPPET assembly ... reason=assembled` - a
   `reason=assembly-timeout` line means the census never reported ready; return it
   with its `bound-nodes=/stable=` numbers.

2. Clothing, not only weapons. The player's worn body items (armor, clothes,
   boots, jewelry) are mirrored onto the puppet while the reference is still
   beside the player, and reconciled afterwards: `SCOPY PUPPET worn equipped=...
   removed=... worn=...`. Change a piece of armor in the inventory and the panel
   figure should follow within a frame; report the line if it does not.
   The figure is STRIPPED first - once when the reference is born and again
   before dressing - because a freshly placed Actor is not empty: its base record
   carries its own items, and this puppet's base is a copy of the player's NPC
   record. Without the strip it arrived wearing a bow, a shield and an axe that
   nobody had equipped, stacked on top of the mirrored gear, and a panel opened
   during the assembly window showed exactly that. The strip names what it took
   off - `SCOPY PUPPET stripped items=... removed=... inherited=[name(formid)
   ...]` - so the source is identifiable from the log; return that line.
   Weapons are mirrored exactly in BOTH directions - the puppet's current hand
   item is taken off when the player's hand holds something else or nothing, and
   the `SCOPY PUPPET equip` line reports `equipped=`/`removed=`. A weapon on the
   figure that the player is not holding now means the removal failed, so return
   that line rather than only the picture.

3. The player's body shape. OBody NG hands a freshly placed NPC a RANDOM preset
   every session (`OBody.log`: "No preset defined for this actor, getting it
   randomly"), so the puppet's body is copied from the player through RaceMenu's
   skee64 BodyMorph interface instead: `SCOPY SHAPE applied ... compared=...
   mismatched=... copied=... skipped=... sample=[...]`. `SCOPY SHAPE skee
   BodyMorph interface acquired version=N` confirms the interface; `unavailable`
   means no RaceMenu was found and the puppet keeps whatever body it was given.
   The copy compares before it writes, so a line only appears when the two
   actually disagreed - its absence on a later check is the healthy state, not a
   failure. ONLY OBody's own morph key is copied: a morph store is shared with
   every other system that uses one, and copying all of it once put XPMSE's
   "which weapon type is equipped" state on the puppet, which made it show an axe
   and a bow it had never been given. The other keys are counted in `skipped=`
   and left alone.

Panel black but the studio target correct? Check `SCOPY COMPOSITE src/bound/match`
- match=0 means the studio texture was bound as an output target and as a sampler
source at once, which D3D11 silently resolves by unbinding the source. Return that
line if it appears.

F6 shows no figure? Check `SCOPY R01 fallback to the copy reason=...` first. A
`reason=not-framed` on every frame means the framing had no bound; the frame line
says where that bound came from (`bound-from=palette` is the live skin palette,
`bound-from=nodes` the tree's node cloud that serves until a draw has allocated
the palette). Then check `SCOPY R01 frame ... center-d=`: it must be ~0, and
`world-t-after` must equal `anchor`. A non-zero center-d means the framing delta did
not reach the tree (a local write on a parented node moves nothing); equal values
with an empty panel move the question to projection, where `rd-null` and the pass
geometry are the next evidence.

Visible clone beside the player? Check `SCOPY PUPPET park-3d`: `warp` must be 1 the first
time the reference moves, and `world-t-after` about 8000 below `ref-t` AND still there on
the next park call. A `warp=0` line when the reference just moved, or a world position back
near the reference, means the character controller was left behind - the collision and
activation target stay interactable even when the figure is invisible (the park requests
the warp exactly then).

F6 shows only rigid attachments (a weapon) and no body? Two known causes, in order:
the placement was applied after the puppet's graph update (`begin_frame` must run before
`PuppetDriver::tick`, `end_frame` after the draw branch - the log order proves it), or
the engine's skin cache still counts as current for this render frame, so the body is
drawn from pre-placement bone worlds; the batch invalidates that id itself
(`invalidate_skin_cache`) and the `R01 census` line shows `skin-frameID` equal to
`render-frame` after a healthy draw.

The picture still uses the copied graph unless F6 is on. A successful apply log
alone still does not establish same-Actor direct drawing - R01's identity,
A4/A5 and restoration lines do. Puppet clip sampling, a typed capture-time clip
and procedural idle remain the fallback display chain when F6 is off.

Retained diagnostics distinguish two output buffers, missing/necessary bones,
local/world transforms, camera and draw prerequisites. Coverage 98 -> 116 and
buffer selection remain open. Physics and facial expressions have not been added;
the body SHAPE is now copied from the player (see AFP10 above), which is not the
same thing as physics or FaceGen.

Return CharacterPanel.log and the manifest, plus screenshots for visible changes.
For crashes also return CrashLogger output and this package's PDB. Logs are
replaced on launch; preserve evidence before restarting. Avoid repeated captures:
old copies are still intentionally parked because teardown is unsafe; the 12-entry
bookkeeping cap does not cap leaked memory.

F6 diagnostics (AFP9g) wrote the studio image itself: read
`.../SKSE/CharacterPanel/r01-panel-NNN.tga` (first drawn frame and every 300 after,
capped at 20) - that picture, plus `SCOPY R01 census` (geoms/skinned/skin frameID/
num-matrices/first bone world) and `SCOPY PUPPET park-3d ... ref-now`, is what a
direct-draw round is judged from now; an external frame capture is optional.

AFP10 test sheet: docs/afp10-spawn-shape-dress-test-2026-10-14.md (start here).
AFP9 test sheet: docs/afp9-coverage-and-direct-draw-test-2026-10-13.md.
Current commands and module map: docs/code-maintenance.md.
Next validation after AFP10: docs/afp-next-validation-and-paused-gaps-2026-10-13.md.
Historical documents describe their recorded revisions; removed tools/source can
be recovered from Git history. This package does not install or deploy itself.
