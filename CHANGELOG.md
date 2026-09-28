# Changelog

## Unreleased

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
