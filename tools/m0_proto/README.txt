CharacterPanelProto M0 accumulator-swap prototype

This package is an opt-in, one-shot rendering experiment for the M0 studio
renderer contract. It verifies the capture report's finding 1: the engine's
current accumulator is a plain global slot, so a private frame can run by
swapping the UI3D secondary accumulator in, accumulating the menu scene into
a private color/depth target, and restoring. It draws no panel, creates no
actors, and changes no persistent game state; the swap lasts exactly one
rendered menu frame.

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
2. Press F6 once: the next rendered menu frame runs the swap. The probe log
   records the saved and swapped accumulator pointers, the accumulator
   state after the draw, and writes proto-swap-NNN.tga (tone-mapped readback
   of the private target) under Documents/My Games/Skyrim Special
   Edition/SKSE/CharacterPanelProto.
3. Close the inventory and exit normally. Return the log and TGA files.

Expected readings:

- swap frame log shows current=<engine accumulator> -> secondary=<same
  address as ui3d.secondary_accumulator in the probe manifests>;
- pass/bucket counters on the secondary accumulator advance past 0 during
  the swapped frame;
- a TGA showing the highlighted item proves the engine accepted a private
  accumulator as current and rendered the menu scene through it.

If the TGA is black or the game crashes on F6, capture the log and report
back; the swap frame restores the previous accumulator before returning, so
a failure isolates to the swap itself.
