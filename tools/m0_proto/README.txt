CharacterPanelProto M0 panel prototype (v4, composite)

This package is an opt-in rendering experiment for the M0 studio renderer
contract. Runs 9-16 (2026-10-01) passed the M0 gate: menu-scene passes are
identified with zero false positives, hook-shared with Community Shaders,
and re-drawn 1:1 into a private offscreen target with correct shading and
intra-item occlusion. v3 made the pipeline persistent (handoff packages
1+2); v4 (package 3) puts the studio output ON SCREEN:

- F7 toggles the panel (F6 is bound in the user's game). While open,
  every DrawInterfaceStart frame draws the studio image as an OPAQUE
  RECTANGLE (fixed 58-88% x 12-68% of the screen) into the bound target,
  BEFORE the original menu draw — the panel sits under every other UI
  element, per PRD section 2. The studio target itself is still
  re-rendered every menu frame by the pass replay.
- The rectangle persists while the panel is open, including normal
  gameplay (showing the last menu item) — it is drawn every frame into
  the UI target and removed when the panel closes.
- Closing the panel with F7 still writes ONE evidence TGA of the studio
  image before the release destroys the target; F8 is the optional
  mid-session grab; logging is throttled (discovery lines per geometry,
  state-change summaries, ~30 s heartbeat, composite draw counter).

No actors are created and no persistent game state changes. Do NOT load
this together with the old CharacterPanel plugin or CharacterPanelProbe
(the probe's F7/F8 would collide).

Build (opt-in, off by default):

    cmake -S . -B build -DCHARACTER_PANEL_BUILD_PROBE=ON \
      -DCHARACTER_PANEL_BUILD_PROTO=ON \
      -DCMAKE_TOOLCHAIN_FILE="<vcpkg>/scripts/buildsystems/vcpkg.cmake" \
      -DVCPKG_TARGET_TRIPLET=x64-windows-static-md
    cmake --build build --config Release --target CharacterPanelProto

The DLL is build/Release/CharacterPanelProto.dll. Install it manually (MO2
or SKSE/Plugins).

In-game procedure (AE 1.6.1170 only) — the panel is VISIBLE now:

1. Enter a save, open the SkyUI inventory, highlight an item with a
   visible 3D model, press F7 (panel opens). EXPECT: an opaque rectangle
   on the right side of the screen showing the studio image, with SkyUI
   widgets (item card, tooltip, cursor) rendering ABOVE it. Press F7
   again: the rectangle disappears and the evidence TGA is written as
   before ("studio dump written" + release lines in the log).
2. Walk around in the world with the panel open: the rectangle stays
   put while the world renders normally behind and around it. Note any
   one-frame flicker or state pollution — that is the A01 evidence.
3. Optional extras: F8 while open for a mid-session TGA; highlight
   several items (the rectangle follows the studio content); leave the
   panel open during normal play for a few minutes (one heartbeat line
   per ~30 s).
4. Save + load WITH the panel open: expect "Panel force-closed (save
   loading)", a release line WITHOUT a dump line, and the rectangle
   gone.
5. Exit. Return the log and the TGAs from this session, plus a note on
   what the rectangle looked like (visible? under the widgets? correct
   item?) — the composite is judged on screen, not only by TGA.

Failure isolation: no rectangle but "composite ready" in the log means
the bound target at DrawInterfaceStart entry is not the visible
composite — the "composite target at DrawInterfaceStart entry" identity
line decides the next hookup point. A misplaced/wrong-aspect rectangle
points at the panel-rect constants. Any crash over repeated cycles
points at the lifecycle path. All outcomes are evidence; report the log
either way.
