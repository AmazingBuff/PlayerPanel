# Actor animation previews

## Purpose

Continue the source Actor's observed draw/sheathe or idle phase, dispatch accepted HKX annotation events through native DAC, and ask OAR for replacements using the Actor's actual effects. Folder labels do not define playback order: Loop2 remains an alternative chosen by conditions and priority.

## Scope and non-goals

The module covers detached standing, weapon draw/sheathe and optional hand posture, with independent clocks/history per controller. It does not create an Actor, drive the original behavior graph, simulate magic, parse DAC TOML, mirror locomotion/attacks or change native graph retirement. Original Actor marker effects are temporarily borrowed by [dac::Session](dac.md), which restores captured entry effects on inventory close; equipment and unrelated gameplay state remain outside that transaction.

## Architecture

`PanelMonitor` observes InventoryMenu only. `CharacterManager` owns equipment revisions and distributes source/lifecycle inputs inside the serialized render bracket. Each `SceneGraph` owns one `AnimationController` and one `dac::Session`. Appearance/source graph recapture retargets pose nodes without reseeding the inventory entry effect snapshot.

Accepted playback owns one `EventClock`. Its annotations enqueue native DAC-only processing through the session's serialized UI task queue. Native effect observations publish revisions; OAR queries read actual source Actor effects, while retaining the existing standing/equipment/identity condition policy. The controller owns copied choice metadata, staged Random/variant history, asynchronous clip leases, current request/state acceptance and pose composition.

## Code map

| Path | Stable entry point |
| --- | --- |
| [action.h](../src/animation/action.h) | `ActionKind`, `entry_action_kind`, engine base-name families |
| [event_clock.h](../src/animation/event_clock.h), [event_clock.cpp](../src/animation/event_clock.cpp) | `EventClock`, `Annotation`, `clip_annotations` |
| [controller.h](../src/animation/controller.h), [controller.cpp](../src/animation/controller.cpp) | `SourceObservation`, `AnimationController::request`, `poll`, `advance`, `retarget`, `apply_standing_pose` |
| [sequence.h](../src/animation/sequence.h), [variant_state.h](../src/animation/variant_state.h) | current `Stage`, pose masks/composition and accepted `SelectionState` |
| [clip.h](../src/animation/clip.h), [clip.cpp](../src/animation/clip.cpp) | `ClipPlayback`, retained binding ownership, mapping and detached pose samples |
| [session.h](../src/dac/session.h), [session.cpp](../src/dac/session.cpp) | native event tasks, revision observation and inventory transaction lifecycle |
| [hooks.h](../src/oar/hooks.h), [hooks.cpp](../src/oar/hooks.cpp) | `AnimationSelection`, copied query-only replacement metadata and source effect conditions |
| [scene_graph.h](../src/character/scene_graph.h) | SceneGraph-owned animation/session and source observation |
| [animation_controller_test.cpp](../tests/animation_controller_test.cpp) | the two focused transaction/event cases |

## Interfaces

`SourceObservation` supplies stance, hands/form, source binding index/time, request/source-graph generations and reason. Engine clip filenames identify known idle/draw/sheathe families; mod folder names never define transition roles, looping or successors.

| ActionKind | Value | Role |
| --- | --- | --- |
| `e_standing_idle` | 0 | standing idle |
| `e_equip` | 1 | finite draw |
| `e_weapon_idle` | 2 | condition-selected idle |
| `e_unequip` | 3 | finite sheathe |
| `e_hand_idle` | 4 | optional masked hand/finger contribution |

`AnimationController::request` also receives its SceneGraph's `dac::Session`. The session receives the latest request generation immediately; obsolete queued equipment events cannot mutate a later request. Appearance-only changes adopt the generation for future accepted-clip events while rejecting already queued old events.

Selection logical `name`/descriptor `path` remain separate from actual loading `index`/`binding_name`. Copied interruptibility, replace-on-loop, blend durations and native variant/PlayOnce/Random metadata survive only as values, never dereferenceable native candidate pointers.

## Event timing and native state

`clip_annotations` reads retained `hkaAnimation::annotationTracks`, validates times once and stable-sorts them chronologically. `EventClock` dispatches each crossed occurrence once, including zero/end boundaries and loop wrap. Fresh clips may emit zero-time events. A source-phase resume skips annotations through the observed time. Compatible refreshes and retarget preserve clock/cursor.

Only current accepted playback emits events. Loading, vanilla calibration/standing underlay and outgoing fades never replay annotations. Event intervals consume remaining frame time; finite completion and loop boundaries keep the existing semantics. Delta remains clamped to 0–0.1 seconds. Observed draw/sheathe resumes its recorded phase; typed equipment requests begin at zero. Idle is queried at actual finite completion, rather than preplanned before draw events have run.

Native DAC is asynchronous relative to render sampling. Call completion and actual effect visibility are distinct: `Session::observe` publishes real native effect changes through a revision on SKSE UI tasks. Interruptible idles may requery when that revision changes; noninterruptible idles wait for a boundary. Pending choices recheck revisions before acceptance and retain a compatible in-flight lease. An event at 2.5 seconds can change eligibility before clip end when the paused engine applies the native effects; no synthetic marker state is manufactured to hide delayed application.

At loop boundaries, retain OAR replace-on-loop and native sequence/PlayOnce/Random scope semantics. Same-replacement interrupt or appearance queries preserve the accepted variant/history. Native choices are proposed from copied state and committed only at acceptance/permitted boundaries. No Start → Loop → Loop2 playlist exists.

## Pose mixing and placement

The outgoing asset remains alive and its pose advances while the incoming clip is sampled. Local fades use copied OAR interrupt/loop durations, including zero; native variant policy can suppress a between-variant fade. Event dispatch for the outgoing clip stops at handover.

This is detached pose composition, not OAR's native ActiveClip blender. It does not reproduce ActiveClip loop lookahead or end-relative incoming seek; incoming clips start at zero unless explicitly resuming the source phase. Calibration remains independent vanilla MT_Idle at zero through `apply_standing_pose`, before the current action write. Neither a native effect event nor replacement selection changes placement.

## Invariants and compatibility

Only current request/source generations and native effect revisions can accept new resources. Accepted playback stays visible while a replacement loads. Same-equipment refresh reevaluates current conditions without inventing draw motion and preserves matching clocks. Source graph replacement resets graph-specific playback safely while preserving the independent menu transaction.

Clocks, accepted history and native session snapshots remain per Actor record. SceneGraph owns the display; PanelMonitor stays inventory-only. The original Actor graph/transforms are read-only; the only approved original-Actor mutations are bounded native DAC marker casts and effect restoration. Existing rendering, attachments, calibration and parked graph retirement remain unchanged.

## Failure modes and logs

Missing/mismatched native DAC disables phase-event processing once and leaves preview playback available. Unsupported native rules or unrelated keyword collisions reject the entire preview event. The [DAC document](dac.md) defines exact profile/restoration failures and `MISMATCH` logs.

Missing bases/projects retain loading or accepted playback. Concrete binding rejection retains the existing base fallback and clears rejected replacement policy. Failed proposals keep the old visible clip. ABI mismatches refuse OAR queries under unchanged identity/prefix gates. Logs identify actor/request/source generation, choice/state revision/path/index/reason, annotation/time, asset failure, handover/blend and retarget; native snapshot/event/revision/restore logs are under DAC. No frame/bone spam is added.

## Dependencies

Existing Skyrim/CommonLib/Havok/SKSE and audited OAR 3.1.6/3.2.1 remain. Native event support requires the supported original DAC NG Plus binary described in [dac.md](dac.md#original-binary-compatibility-and-provenance). CharacterPanel has no tomlplusplus runtime link or parser. No DAC rebuild or tomlplusplus package is required. See [build setup](build.md).

## Smooth Moveset support boundary

The [canonical compatibility boundary](dac.md#smooth-moveset-compatibility-boundary) separates the core non-combat phase prerequisites from IED-dependent equipment branches and the TDM-effect OR combat-state branch. The core event/native-DAC/query mechanism does not imply complete pack coverage. IED placement conditions still need an explicit preview query adapter, even when IED and its provider are installed. The current standing projection answers `IsCombatState` with zero, so source world combat alone does not enable that OR alternative. No IED or combat-policy behavior is changed here.

## Tests and verification

Exactly two direct-compiled [cases](../tests/animation_controller_test.cpp) use production `SessionCore`/`EventClock` with a narrow fake engine backend: entry/event/close restoration preserving unrelated/equipment state; and stale requests, close/reopen, load/reset and independent sessions. They do not execute native DAC/OAR/Skyrim effect creation. [Retained graph/math sources](../tests/snapshot_transform_test.cpp) remain outside CMake and are not part of this task's executed cases.

[Build verification](build.md#verification-and-limits) documents the CharacterPanel build and original-binary maintenance inspector. No local Skyrim process exists: paused native effect application/expiration, close restoration visibility/order, hooks, visuals and native lifetime remain unverified.

## Safe modification

Keep all DAC native metadata/safety, transaction capture/restoration and UI/task lifecycle inside src/dac. Animation owns only annotation clocks, resource/history acceptance, queries and poses. Preserve loader leases, request/revision guards, vanilla calibration and graph parking. Do not reintroduce effect projections, a parser/emulator fallback, folder ordering or original graph event dispatch.

## Synchronized files and related history

Maintain this document with [dac.md](dac.md), [README](../README.md), [Changelog](../CHANGELOG.md), [build](build.md), [OAR hooks](oar-hooks.md), [SceneGraph](scene-graph.md), [maintenance](code-maintenance.md), [runtime notes](../tools/scene_copy/README.txt), implementation and focused test consumers. Earlier virtual markers/folder playlists and Actor/puppet experiments in [historical reports](history.md) are superseded. The retained calibration fix and native graph retirement policy remain independent.
