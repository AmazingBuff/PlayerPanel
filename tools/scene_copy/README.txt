CharacterPanel scene-copy / AFP development package

Use this package's CharacterPanel.dll and PDB together. Read build-manifest.json
and SCOPY BUILD to confirm build identity, source baseline, runtime and hashes.

Current controls:
  F7: capture/audit player third-person 3D in paused InventoryMenu; draw starts off.
  F8: toggle drawing of the accepted copied graph.
  F2: AFP mark (final output slots, calibration, camera, geometry, source comparison).
  F4: off -> tick -> tick+apply -> tick+apply+force -> step -> off.
      Step advances the puppet once by 1/60 second. Verify mapping while the world
      runs before judging paused apply. F3/F6 and old rotate/release controls are gone.

The picture still uses the copied graph. AFP writes the puppet's own tree, so a
successful apply log does not establish same-Actor direct drawing. Puppet clip
sampling, a typed capture-time clip and procedural idle remain the display chain.
No empirical solver, arbitrary animation scan, S2 swing probe or engine image
startup dump remains. Empty binding-table mapping remains identity-unverified.

Retained diagnostics distinguish two output buffers, missing/necessary bones,
local/world transforms, camera and draw prerequisites. Coverage 98 -> 116 and
buffer selection remain open. Physics, facial expressions and runtime body morphs
have not been added by cleanup.

Return CharacterPanel.log and the manifest, plus screenshots for visible changes.
For crashes also return CrashLogger output and this package's PDB. Logs are
replaced on launch; preserve evidence before restarting. Avoid repeated captures:
old copies are still intentionally parked because teardown is unsafe; the 12-entry
bookkeeping cap does not cap leaked memory.

Current commands and module map: docs/code-maintenance.md.
Next validation: docs/afp-next-validation-and-paused-gaps-2026-10-13.md.
Historical documents describe their recorded revisions; removed tools/source can
be recovered from Git history. This package does not install or deploy itself.
