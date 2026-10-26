CharacterPanel — SceneGraph runtime notes

Build setup and pinned dependencies: docs/build.md
Current architecture/actions: docs/scene-graph.md and docs/animation.md
Native DAC dependency/session/restore: docs/dac.md
Log navigation and hash conventions: docs/code-maintenance.md
Historical research summaries/source recovery: docs/history.md and docs/history-sources.md

Research status (2026-10-26): docs/independent-behavior-graph.md#current-status
G1 passes revised Option A only (shell release, repeated load, sampled source
equality, loaded graph retained). Loaded-graph teardown/global counts/convergence
are unverified. G2 v17 has not passed: 116 poses stay unchanged despite WeapEquip
dispatch and node/variable evolution. Complete G3-G6 and OAR parity remain open.
IndependentGraph defaults off and runs once per process. v17 inject budgets 40
INDEPENDENT GRAPH rows plus source snapshots; 120 fixed 1/60-second steps mean
two simulated seconds in one paused batch, not 120 rendered frames.
BehaviorGraphProbe is a separate source-only 16-row observer. Its fixed
candidate=none/gate1=not-passed label describes that row, not current G1 status.
Header initialization/reset/refill is a first blocking hypothesis, not the
proved only cause. n at +0x32 is numData; native numTracks is at +0x4. Sampling
payload/context/application still need evidence; no audit16/v18 runs here.

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

Fixed idle/equip/unequip base indices derive from action.h aliases for all eleven
weapon families, including MT_Idle calibration/fallback. Nonpaused noninventory
observation prewarms through the existing third-person graph resolver only after
published graph/behavior/skeleton and covering binding data are ready; DLL load
alone is too early. LoadingMenu observation is skipped. One borrowed-name table
pass retains only finite matches and missing aliases; no expanded catalogue or
session copies remain. World release and changed table owner/address/count
invalidate reuse. A late-ready menu fallback is logged, without waiting.
OAR bases stage=world-prewarm|menu-fallback reports table/requested/resolved/
missing counts and elapsed_ms once per initialization. Warm queries keep native
OAR/variant choices and fetch actual selected binding names by index, including
deduplicated aliases. Missing bases keep the existing standing fallback policy.
See docs/animation.md#fixed-base-indices-and-project-readiness and the two direct
cache cases in docs/build.md#fixed-base-index-verification. DAC preflight, Clone/
audits and first asset/GPU use are separate unchanged costs. Hitch reduction and
actual prewarm publication timing remain unmeasured in game.

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
TOML, HKX/configs with existing Skyrim/SKSE prerequisites. The native IED adapter
supports IED_GearNodeEquippedPlacementHint, IED_GearNodePlacementHint and
SDS_IsShieldOnBackEnabled for current source equipment/configuration, with
required condition 1.0.0.0, exact component schemas (three for placement, zero for
SDS) and provider embedded 1.0.2.0 (supplied archive label 1.0.3).
Left/right, comparisons, negation and false results stay native. Historical settings
and explicit-node predicates are allowed; equipped hints require an unchanged requested
hand. Changed-hand/unknown historical predicates, unknown checked schemas/providers
and missing/mismatched providers reject; the
unsupported-third-party diagnostic remains a query-adapter limit, not missing
DAC state. Original IED/provider installation is separate; loaded metadata does
not prove OAR registration. The shield-setting branch needs provider-side SDS
interface/condition registration and prerequisites; SDS is not universally needed
for core non-combat phases. Paused IED hint refresh/native reads remain unverified.
See docs/oar-hooks.md#native-ied-placement-conditions-2026-10-21 and the dated checklist.
Combat Start/Loop use the TDM-effect OR combat-state clause;
this preview currently projects combat state to zero. Full pack/game validation
remains pending.

Two game follow-up categories (r4 is partial evidence, not complete acceptance):
  1. Equipment idle policy: world draw/attack/move/sheathe still selects held non-combat idle,
     rapid equipment changes and restored marker state with unrelated equipment/buffs intact.
  2. Copied-graph presentation/lifetime: F7 compatible-loop preservation, F8, close/reopen/
     load boundaries, appearance, attachments, framing and transparency.

r4 (docs/diagnostics/display-issues-2026-10-22.md#r4-session) records shield9515,
torch composed rows and five matched restores. Torch-only removal was not covered.
Raw nonuniform scale and read_pose's scalar x are evidence to compare against
native semantics and phase-aligned world/panel transforms, not a proven root cause.

Retirement bookkeeping is capped; retained native graph memory is not. Safe
teardown remains unresolved. Local build/document checks do not prove game-time
hooks, native loading, visual output or lifetime behavior. Prior local evidence
and its limits are recorded centrally in docs/build.md.
