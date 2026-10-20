CharacterPanel — SceneGraph runtime notes

Build setup and pinned dependencies: docs/build.md
Current architecture/actions: docs/scene-graph.md and docs/animation.md
Native DAC dependency/session/restore: docs/dac.md
Log navigation and hash conventions: docs/code-maintenance.md
Archived controls/results/proposals: docs/history.md

Current build (repository root, prepared toolchain):
  cmake --preset Release
  cmake --build --preset Release

Release is the only configure/build preset. CharacterPanel is the sole first-party
DLL target. Outputs are build/Release/CharacterPanel.dll and its matching PDB.
CommonLibSSE and normal CMake generation/build utility projects remain required.
There are no CMake test/package/install/compatibility entry points. The two
idle policy/preparation cases compile directly with MSVC; see docs/build.md.

Controls in a paused InventoryMenu:
  F7  Queue recapture.
  F8  Toggle display.
Input queues intent; capture/retarget/animation run in the serialized bracket.
The source Actor graph/transforms stay read-only; native DAC temporarily changes
controlled marker effects and restores their inventory entry values at close.
No second Actor is created.

Identity and logs:
  CharacterPanel.log is written under SKSE::log::log_directory(), not a fixed
  machine path. Start with SCENEGRAPH BUILD, then correlate actor/request/capture
  fields in CharacterManager equipment, ANIMATION, SCOPY and SceneGraph draw lines.
  The build identity is generated at configure time. A later commit does not
  refresh an existing header/DLL: configure then build when preparing a binary
  for that commit, and keep its exact DLL/PDB pair together.

Copy config/CharacterPanel.ini to Data/SKSE/Plugins/CharacterPanel.ini.
[Animation] UnarmedIdle=non_combat_loop is the empty-hand default; mt_idle uses
vanilla MT_Idle directly. Restart to apply. Equipped families ignore world action
and draw state. Actual held meshes are remounted only in the owned copy at
WEAPON/SHIELD; generic tracks map independently, and source graph/equipment/draw
flags remain read-only. Identical-form dual/custom/bow visibility is game-only.

Queried idles queue the known Smooth_loop event once per relevant request and
wait for actual persistent markers/off-keyword clearance. A ready compatible refresh
observes without recasting; a pending refresh starts one newer-generation attempt
and rejects stale work. Compatible clip leases wait for readiness/current checks.
A retained direct fallback skips preparation on compatible refresh until equipment
or graph/controller lifetime changes. Missing/rejected/timed-out preparation uses a stable family base (empty
hands use MT_Idle). Direct MT_Idle skips preparation and annotation dispatch.

Accepted HKX annotations enqueue SKSE UI tasks that call the profiled original native
DAC-only handler on the source Actor. CharacterPanel does not parse DAC TOML or
emulate effects. OAR reads actual Actor effects; UI close invalidates queued events
and synchronously restores the immutable entry snapshot. See docs/dac.md for the
supported original DLL and offline PDB/profile inspection, unsupported-event checks and runtime limits.
The original DAC installation stays untouched; update only CharacterPanel.dll.
No DAC source/build/patch route or tomlplusplus dependency remains.
Folder Start/Loop names do not schedule successors. Local fades advance both
poses using copied native durations; calibration stays vanilla MT_Idle. Snapshot,
call completion, observed effect revision and checked restoration are separate logs.

Smooth compatibility: docs/dac.md#smooth-moveset-compatibility-boundary.
Core non-combat phases use OAR, supported original DAC and the pack's own ESP,
TOML, HKX/configs with existing Skyrim/SKSE prerequisites. IED placement branches
still need an explicit preview query adapter; provider installation alone is not
enough. The unsupported-third-party diagnostic is a query-adapter limit, not
missing DAC state. Combat Start/Loop use the TDM-effect OR combat-state clause;
this preview currently projects combat state to zero. Full pack/game validation
remains pending.

Two representative game follow-up categories (not verified locally):
  1. Equipment idle policy: world draw/attack/move/sheathe still selects held non-combat idle,
     rapid equipment changes and restored marker state with unrelated equipment/buffs intact.
  2. Copied-graph presentation/lifetime: F7 compatible-loop preservation, F8, close/reopen/
     load boundaries, appearance, attachments, framing and transparency.

Retirement bookkeeping is capped; retained native graph memory is not. Safe
teardown remains unresolved. Local build/document checks do not prove game-time
hooks, native loading, visual output or lifetime behavior. Prior local evidence
and its limits are recorded centrally in docs/build.md.
