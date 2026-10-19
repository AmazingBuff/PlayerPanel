CharacterPanel — SceneGraph preview

Every build uses the same copied-SceneGraph backend. Release-scopy and
CharacterPanelSceneCopyPackage remain compatibility entry-point names.
No second Actor, equipment mirroring or live-Actor diagnostic route exists.

Controls (paused InventoryMenu):
  F7  Queue recapture of the source graph.
  F8  Toggle the copied preview display.
Input only queues intent; capture, retarget and animation run in the serialized
render/update bracket. Other historical diagnostic controls have been removed.

Build:
  cmake --preset Release
  cmake --build build --config Release --target CharacterPanel
The optional Release-scopy preset uses build-scopy for the same implementation.
Existing VS2022/MSVC 14.44.35207, CommonLibSSE and vcpkg dependencies are required.

Package identity:
  The load banner contains CharacterPanel-scenegraph- followed by the configured
  label, source/CommonLib revisions, dirty state and configure timestamp.
  The compatibility package target records this exact identity, DLL/PDB SHA-256,
  source baselines and backend: SceneGraph in build-manifest.json.
  Packaging does not deploy files. Match a returned log to that manifest.

Manual game follow-up (not verified by local builds):
  Check inventory entry during Equip, Start and Loop; sheathe to standing;
  rapid equipment changes; F7 without a playback rewind; F8; close/reopen and
  load boundaries; appearance, weapon attachment, framing and transparency.
  Confirm source actors remain visually and behaviorally unchanged.

Retired copied graphs intentionally remain parked because safe native teardown
is unresolved. Animation hook/loader, visual and graph-lifetime behavior still
requires a Skyrim runtime. See docs/scene-graph.md and docs/animation.md.
