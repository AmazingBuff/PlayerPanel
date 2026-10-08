CharacterPanel scene-graph copy test package (experiment CharacterPanel-scopy-S0-2026-10-08)

This build replaces the Actor assembly route with the S0 manual copy route. It
does not create an NPC base or an Actor, and it does not modify the source
character's graph, parent, pose, inventory, or equipment.

Install SKSE/Plugins/CharacterPanel.dll and its matching .pdb into a separate
MO2 test mod that replaces CharacterPanel. Do not load the old CharacterPanel,
CharacterPanelProbe, or CharacterPanelProto at the same time: the probe's F7/F8
conflict with this experiment. Use a test save and keep the original DLL for
rollback.

Target runtime is Skyrim AE 1.6.1170 with SKSE 2.2.6. No HKX, CBPC, or SMP
driver is implemented in S0: a copy whose hair or cloth stays in the captured
pose is expected, not a failure.

Keys (pressed events only; no game input is consumed):

  F7   with the inventory open and the game paused: release the old copy, then
       copy and audit the current third-person graph. The load and main menus
       refuse the copy, and so does the world with no inventory open.
  F8   start or stop drawing the copy that passed the audit
  F3   rotate the copy 90 degrees
  F4   release the copy

Run the copy, draw and lifecycle test first (test A in
docs/scene-graph-copy-validation.md), then the re-equip and appearance test
(test B). Fill in docs/scene-graph-copy-results-template.md and return it with
the complete CharacterPanel.log, the screenshots, and this package's
build-manifest.json.

The log is overwritten on every launch: copy it after each test session before
restarting the game. If the game crashes, return the CrashLogger log together
with the .pdb from this package.
