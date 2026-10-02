CharacterPanelProto M0 panel prototype (v5.4, stage-2 display instance P,
live world-actor route)

This package is an opt-in rendering experiment for the M0 studio renderer
contract. Runs 9-16 (2026-10-01) passed the M0 gate: menu-scene passes are
identified with zero false positives, hook-shared with Community Shaders,
and re-drawn 1:1 into a private offscreen target with correct shading and
intra-item occlusion. v3 made the pipeline persistent (handoff packages
1+2); v4 (package 3) put the studio output ON SCREEN; v5.x are stage 2
of the handoff plan — the panel content becomes the INDEPENDENT DISPLAY
INSTANCE P (docs/stage2-p-instance-plan.md):

- Falsification chain: v5 deep copy returned no node (run 27); v5.1/5.2
  built and dressed a clone actor but the menu scene never collected its
  graph (runs 29/30: attach succeeded, ZERO passes — the menu culler
  works from its own private queue, not scene-graph attachment); a
  graph detached from the world belongs to no culler at all (run 30,
  zero P passes with the whitelist live).
- v5.4 (route 3): P stays a LIVE WORLD ACTOR — dressed clone parked
  ~8000 units below the player, inside the camera far plane (20480) so
  the WORLD culler keeps emitting its passes, far below any gameplay
  pitch so no camera sees it. The replay whitelist (root ancestry +
  panel-open gate) catches those world-stream passes, SUPPRESSES their
  world-side draw (passthrough skipped — the world view shows no double,
  PRD 0.5), and replays them into the studio target instead.
- Expected per open (a few real seconds): "clone placed" -> "dressed;
  waiting for the biped 3D (park follows)" -> "Proto P whitelist armed"
  -> "Proto v5 P pass: ... [piece name] ..." lines -> a HUMAN FIGURE in
  the rectangle. Closing disarms the whitelist and kills the shell
  ("Proto P whitelist disarmed", "clone actor killed").

No persistent game state changes beyond a transient parked clone. Do NOT
load this together with the old CharacterPanel plugin or
CharacterPanelProbe (the probe's F7/F8 would collide).

Build (opt-in, off by default):

    cmake -S . -B build -DCHARACTER_PANEL_BUILD_PROBE=ON \
      -DCHARACTER_PANEL_BUILD_PROTO=ON \
      -DCMAKE_TOOLCHAIN_FILE="<vcpkg>/scripts/buildsystems/vcpkg.cmake" \
      -DVCPKG_TARGET_TRIPLET=x64-windows-static-md
    cmake --build build --config Release --target CharacterPanelProto

The DLL is build/Release/CharacterPanelProto.dll. Install it manually (MO2
or SKSE/Plugins).

In-game procedure (AE 1.6.1170 only) — v5.4 round:

1. Enter a save, open the SkyUI inventory, highlight an item with a
   visible 3D model, press F7 (panel opens). Wait a few REAL seconds.
   EXPECT the "P pass" discovery lines and a HUMAN FIGURE in the
   rectangle (the parked double's armor and pose matter this round —
   note both).
2. Look around the world with the panel open: the parked double must
   NOT appear anywhere (passthrough suppression). Note any sighting.
3. Toggle the panel >=5 times, F8 grabs as you like; save + load WITH
   the panel open (expect force-close, disarm + kill lines, no dump).
4. Exit. Return the log and the TGAs, plus what the panel showed.

Failure isolation: no "P pass" lines after "whitelist armed" means the
parked actor is being culled anyway (park depth vs. far plane — the log
line names the root; the fix is a park-depth calibration); a figure in
the panel but garbled points at the skin replay; a world sighting of
the double points at passthrough suppression. See
docs/stage2-p-instance-plan.md section 3 for signatures and fallbacks.
Report the log either way; every outcome is evidence.
