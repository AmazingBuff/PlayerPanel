CharacterPanel scene-copy / AFP development package

Use this package's CharacterPanel.dll and PDB together. Read build-manifest.json
and SCOPY BUILD to confirm build identity, source baseline, runtime and hashes.

Current controls:
  F7: capture/audit player third-person 3D in paused InventoryMenu; draw starts off.
  F8: toggle drawing of the accepted copied graph.
  F2: AFP mark (final output slots, calibration, camera, geometry, source comparison).
  F4: off -> tick -> tick+apply -> tick+apply+force -> step -> off.
      Step advances the puppet once by 1/60 second. Verify mapping while the world
      runs before judging paused apply. F3 stays with the other mod that owns it.
  F6: R01 direct draw toggle (AFP9). When on, the panel draws the puppet's OWN
      third-person 3D (no detach, framing restored after each draw); when off, the
      copied graph serves as before. The puppet path falls back to the copy and
      logs the reason whenever it cannot draw.

AFP9 additions: the apply stage reconciles its coverage ledger per frame
(output-slots = attempted + written + unreachable + unprocessed) and names the
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

3. The player's body shape. OBody NG hands a freshly placed NPC a RANDOM preset
   every session (`OBody.log`: "No preset defined for this actor, getting it
   randomly"), so the puppet's body is copied from the player through RaceMenu's
   skee64 BodyMorph interface instead: `SCOPY SHAPE applied ... compared=...
   mismatched=... copied=... sample=[...]`. `SCOPY SHAPE skee BodyMorph interface
   acquired version=N` confirms the interface; `unavailable` means no RaceMenu was
   found and the puppet keeps whatever body it was given. The copy compares before
   it writes, so a line only appears when the two actually disagreed - its absence
   on a later check is the healthy state, not a failure.

Panel black but the studio target correct? Check `SCOPY COMPOSITE src/bound/match`
- match=0 means the studio texture was bound as an output target and as a sampler
source at once, which D3D11 silently resolves by unbinding the source. Return that
line if it appears.

F6 shows no figure? Check `SCOPY R01 frame ... center-d=` - it must be ~0, and
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
