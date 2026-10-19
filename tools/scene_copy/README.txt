CharacterPanel — SceneGraph runtime notes

Build setup and pinned dependencies: docs/build.md
Current architecture/actions: docs/scene-graph.md and docs/animation.md
Log navigation and hash conventions: docs/code-maintenance.md
Archived controls/results/proposals: docs/history.md

Current build (repository root, prepared toolchain):
  cmake --preset Release
  cmake --build --preset Release

Release is the only configure/build preset. CharacterPanel is the sole first-party
DLL target. Outputs are build/Release/CharacterPanel.dll and its matching PDB.
CommonLibSSE and normal CMake generation/build utility projects remain required.
There are no current test/package/install/compatibility entry points. Test source
is retained as reference; older passing tests are not a current CMake test route.

Controls in a paused InventoryMenu:
  F7  Queue recapture.
  F8  Toggle display.
Input queues intent; capture/retarget/animation run in the serialized bracket.
The source Actor is read-only; no second Actor is created.

Identity and logs:
  CharacterPanel.log is written under SKSE::log::log_directory(), not a fixed
  machine path. Start with SCENEGRAPH BUILD, then correlate actor/request/capture
  fields in CharacterManager equipment, ANIMATION, SCOPY and SceneGraph draw lines.
  The build identity is generated at configure time. A later commit does not
  refresh an existing header/DLL: configure then build when preparing a binary
  for that commit, and keep its exact DLL/PDB pair together.

Two representative game follow-up categories (not verified locally):
  1. Entry/equipment continuation: Equip/Start/Loop entry, sheathe to standing,
     rapid equipment changes and an unchanged source actor.
  2. Copied-graph presentation/lifetime: F7 phase preservation, F8, close/reopen/
     load boundaries, appearance, attachments, framing and transparency.

Retirement bookkeeping is capped; retained native graph memory is not. Safe
teardown remains unresolved. Local build/document checks do not prove game-time
hooks, native loading, visual output or lifetime behavior. Prior local evidence
and its limits are recorded centrally in docs/build.md.
