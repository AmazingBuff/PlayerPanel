CharacterPanelProto M0 pass-redirection prototype (v2.5)

This package is an opt-in, one-shot rendering experiment for the M0 studio
renderer contract. Run 14 (2026-10-01) passed the M0 gate: the highlighted
item renders completely and correctly shaded into a private offscreen
target. The prototype combines two proven mechanisms:

- the DrawInterfaceStart entry detour (Microsoft Detours), which brackets
  exactly one rendered menu frame per F6 press;
- pass hooks on the three RenderPassImmediately call sites (the stage-0
  spike's mechanism, hook-shared with Community Shaders by restoring the
  pre-patch call chain).

While the bracket is open, every pass flowing through the call sites is
logged. A pass whose geometry descends from a UI3DSceneManager menuObjects
root is a menu-scene pass; BSLightingShader (shaderType 6) passes among
those are re-drawn into a private offscreen target (sized/formatted from
the call site's own render target) immediately AFTER the original call —
the original's draw leaves every pipeline slot exactly as the pass needs,
so the replay is a 1:1 copy with no state capture or restore. The visible
menu frame is untouched. When the bracket closes, the private target is
read back to a TGA. It draws no panel, creates no actors, and changes no
persistent game state.

Build (opt-in, off by default):

    cmake -S . -B build -DCHARACTER_PANEL_BUILD_PROBE=ON \
      -DCHARACTER_PANEL_BUILD_PROTO=ON \
      -DCMAKE_TOOLCHAIN_FILE="<vcpkg>/scripts/buildsystems/vcpkg.cmake" \
      -DVCPKG_TARGET_TRIPLET=x64-windows-static-md
    cmake --build build --config Release --target CharacterPanelProto

The DLL is build/Release/CharacterPanelProto.dll. Install it manually (MO2
or SKSE/Plugins); do not run it together with the old CharacterPanel plugin
or CharacterPanelProbe.

In-game procedure (AE 1.6.1170 only):

1. Enter a save, open the SkyUI inventory, and highlight an item with a
   visible 3D model.
2. Press F6 once: the next rendered menu frame is bracketed. The log
   records every pass seen during the bracket (geometry pointer, name,
   menu-match result, shader type, passEnum, technique) and writes
   proto-pass-NNN.tga (tone-mapped readback of the private target) under
   Documents/My Games/Skyrim Special Edition/SKSE/CharacterPanelProto.
3. Close the inventory, confirm the menu and the world look normal, and
   exit. Return the log and TGA files.

Expected readings:

- the bracket log shows menu roots with the highlighted item's root among
  them (run 6 put the item under root[1]);
- passes with menu=true appear in the per-pass log; menu-scene lighting
  passes (shader=6) are replayed (lighting_replayed advances past 0);
- per-frame state lines record the inherited depth/stencil and scissor
  configuration, the engine target bound at the call site, and whether the
  replay's raw OM binding survived each call (runs 9-10: black readbacks
  with depth/stencil disabled — v2.2 re-binds and retries once when the
  engine re-applies its own targets inside the call);
- a TGA showing the highlighted item, correctly shaded, proves menu passes
  flow through the hooked call sites and render correctly into a private
  target;
- the inventory item and the world look unchanged after the bracket (the
  replay is non-destructive by design).

If no menu=true pass appears, the menu scene does not submit through the
three hooked call sites — the per-pass log is the diagnostic for the next
iteration. If the TGA is black with replayed>0, capture the log and report
back; both outcomes are evidence, and a failure isolates to the redirect
itself.
