CharacterPanelProto M0 panel prototype (v3.1)

This package is an opt-in rendering experiment for the M0 studio renderer
contract. Runs 9-16 (2026-10-01) passed the M0 gate: menu-scene passes are
identified with zero false positives, hook-shared with Community Shaders,
and re-drawn 1:1 into a private offscreen target with correct shading and
intra-item occlusion. v3 turns the F6 one-shot into the persistent panel
skeleton (handoff work packages 1+2):

- F7 toggles the panel (v3.1 rebinding: F6 is bound in the user's game).
  While it is open, EVERY DrawInterfaceStart frame is bracketed, and
  menu-scene BSLightingShader passes are replayed into the persistent
  studio target immediately AFTER the original call, so the target stays
  current every menu frame and the visible frame is untouched.
- The studio target is persistent: created from the call site's own
  render-target description, self-recreated on resolution/format change,
  and released on the render thread when the panel closes or the session
  invalidates (save loading / new game force-close the panel, FR-06).
- v3.1: closing the panel with F7 writes ONE evidence TGA of the last
  studio image on the render thread, before the release destroys the
  target (both prior sessions pressed the dump key only after closing —
  the close itself is now the capture). An open with nothing replayed
  (e.g. an effects-only menu) dumps nothing; force-closes dump nothing.
- F8 is an optional mid-session grab (panel must be open; otherwise it
  warns and does nothing).
- Logging is throttled: each menu geometry logs once per panel open,
  summaries only on state changes or every ~30 s (heartbeat).

No panel quad is drawn yet (compositing is the next work package), no
actors are created, and no persistent game state changes. Do NOT load
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

In-game procedure (AE 1.6.1170 only) — the capture is the close:

1. Enter a save, open the SkyUI inventory, highlight an item with a
   visible 3D model, press F7 (panel opens), give it a second, then press
   F7 again (panel closes). Expect in the log: the item's geometry line,
   a replay summary, "Panel closed", "Proto v3 studio dump written",
   and "studio target released". This produces one proto-pass-NNN.tga
   under Documents/My Games/Skyrim Special Edition/SKSE/
   CharacterPanelProto showing the highlighted item, correctly shaded
   with correct occlusion.
2. Optional extras:
   - F8 while the panel is open for an additional mid-session frame.
   - Highlight several different items while open before closing —
     the TGA shows the last one.
   - Leave the panel open during normal gameplay for a few minutes:
     expect one heartbeat line per ~30 s and no other spam.
3. Save + load WITH the panel open: expect "Panel force-closed (save
   loading)" and a release line WITHOUT a dump line (force-close writes
   no TGA). Repeat open/close a few times; every close with content
   writes exactly one TGA — no accumulation.
4. Exit. Return the log and the TGAs from this session.

Failure isolation: a close with content but no "dump written" line points
at the readback path; a black/broken TGA isolates to the replay itself;
any crash over repeated cycles points at the lifecycle path. All outcomes
are evidence; report the log either way.
