# Actor animation previews

## Purpose

Continue a standing preview from the source actor's observed entry phase, then hand over to a stable idle. Equip enters at its sampled local time, hands over to a complete Start, and reaches Loop. A missing Loop cannot turn Start into an indefinite idle.

## Scope and non-goals

This module covers standing, weapon draw/sheathe and optional hand posture. It supports independent actor records internally. It adds no multi-actor UI, movement/attack mirroring, world animation events, physics integration or safe native graph teardown. Optional hand contributors use an explicit hand/finger mask; no separate hand layer is enabled by default.

## Architecture

`PanelMonitor` records InventoryMenu state and a revision. `CharacterManager::update` distributes lifecycle inputs and equipment revisions inside the serialized render bracket. Event callbacks only increment watched actor revisions.

Each `SceneGraph` owns `MonitorState` and `animation::AnimationController`. Observation retains the actor handle, stance, clip index/local phase, equipment revision, source graph generation, request generation and dirty reason. Replacing the detached display graph calls `replace_graph` and retargets the same controller; it preserves playback time and maps every node again.

OAR is a query boundary. It audits conditions and returns copied replacement, variant and candidate descriptors. The animation module plans ordered actions, owns accepted variant history and caller-local Random state, loads clip leases, advances only the visible stage, composes pose contributors/crossfade, then writes each skeleton node once. `SceneGraph::capture_graph` privately copies/audits its source graph; `retire_graph` parks retired allocations across preview lifetimes. Renderer pumps the manager and draws.

## Code map

| Path | Stable entry point |
| --- | --- |
| [action.h](../src/animation/action.h) | `ActionKind`, `ActionPrototype`, folder compatibility rules |
| [sequence.h](../src/animation/sequence.h) | `idle_sequence`, `PlaybackTimeline`, `resume_continuation`, `RequestGeneration`, `BoneMask`, `compose_contributor` |
| [controller.h](../src/animation/controller.h), [controller.cpp](../src/animation/controller.cpp) | `SourceObservation`, `AnimationController::request`, `retarget`, `poll`, `advance` |
| [clip.h](../src/animation/clip.h), [clip.cpp](../src/animation/clip.cpp) | `ClipPlayback`, `resolve_binding_playback`, `sample_animation_clip`, `strongest_idle_clip` |
| [pose.h](../src/animation/pose.h), [variant_state.h](../src/animation/variant_state.h) | pose blend, assembly settle gate, `SelectionState` |
| [scene_graph.h](../src/character/scene_graph.h) | `SceneGraph::MonitorState`, `update_monitor`, `observe_source`, `schedule_animation`, `replace_graph` |
| [character_manager.cpp](../src/character/character_manager.cpp) | `CharacterManager::install`, `EquipSink::ProcessEvent`, `update`, `clear_previews` |
| [scene_graph_capture.cpp](../src/character/scene_graph_capture.cpp) | private `SceneGraph::capture_graph`, `retire_graph` |
| [hooks.h](../src/oar/hooks.h), [hooks.cpp](../src/oar/hooks.cpp) | `AnimationDescriptor`, `AnimationSelection`, `query_animation` |
| [query_policy.h](../src/oar/query_policy.h), [selection_state.h](../src/oar/selection_state.h) | project catalogue, caller-owned condition Random state, copied variant metadata |
| [panel_monitor.cpp](../src/panel/panel_monitor.cpp) | InventoryMenu state/revision only |

## Interfaces

[ActionKind and ActionPrototype](../src/animation/action.h) define the following stages. Weapon forms parameterize base names; entry phase is an input snapshot, not another action family.

| ActionKind | Numeric value | Planned role |
| --- | --- | --- |
| `e_standing_idle` | 0 | stable standing body/idle |
| `e_equip` | 1 | finite draw stage; observed entry resumes its sampled local phase |
| `e_weapon_idle_start` | 2 | optional finite idle-entry member |
| `e_weapon_idle_loop` | 3 | weapon idle member; only a stage explicitly marked loop may dwell |
| `e_unequip` | 4 | finite sheathe stage, handing over to standing |
| `e_hand_idle` | 5 | optional hand/finger-masked contribution; no separate hand layer enabled by default |

[SourceObservation](../src/animation/controller.h) carries drawn state, weapon form/hands, source clip index/time, request generation, source graph generation and reason. A captured Equip resumes its own phase, then a full Start begins at handover, then Loop. Start or Loop entry resumes the matching sampled stage. Preloading successors never advances their clocks. `PlaybackTimeline` and `resume_continuation` in [sequence.h](../src/animation/sequence.h) keep accepted continuation at the phase currently visible when an appearance refresh finishes loading.

`AnimationSelection::name` and descriptor `path` carry logical replacement paths; `binding_name` identifies the possibly deduplicated engine catalogue entry. `index` remains the actual loading index. No native candidate pointer escapes the read lock.

`query_animation` may update only its caller's condition Random cache. It cannot seek, commit, dwell or advance playback. Variant selection/PlayOnce acceptance and loop reevaluation belong to the controller. The shared catalogue contains only copied project metadata and is invalidated at a load boundary through `QuerySession::invalidate_projects`.

## Invariants and compatibility

- Only a current actor request generation may accept pending resources. Requests and random/variant histories never cross actors.
- Preloaded successors have no running clock. Handover starts a successor at zero unless it is the explicitly observed entry stage.
- Native OAR sequence metadata takes precedence. Otherwise the explicit folder adapter requires the same root, filename and Start/Loop stem. Logical candidates preserve a Loop even when the engine registers it under an unrelated hash-deduplicated name.
- Only explicit loop stages dwell. Start-only families finish and hand over to a stable non-Start standing idle with a diagnostic.
- Non-mirrored attack/locomotion entries enter an idle loop. Unequip finishes in standing. Appearance revisions refresh conditions without inventing a weapon draw; unchanged choices preserve phase.
- Retargeting clears all old node mappings; pose contributors are combined before one skeleton write. A hand mask admits only the chosen side's hand/finger nodes.
- Crossfade is local pose composition. OAR's internal ActiveClip blending requires an active behavior graph; the pinned public API has no detached offline blend entry point.

## Failure modes

Missing projects/bases leave the preview loading. Rejected replacement bindings fall back to their base; unsupported additive bindings and incomplete standing coverage are diagnosed. Failed sequence assets hand over to standing. New requests discard older proposals and unload their leases. An expired source handle releases its actor record; temporary missing 3D suspends the session while retaining the actor record. Load boundaries queue controller/resource release in the serialized bracket and invalidate project metadata.

Graph audits may reject a recapture, in which case the last accepted graph remains. Retired graph allocations intentionally remain parked because safe native destruction is unresolved. Event logs report actor ID, request/reason, action/path/index, handover, cancellation/load failure and graph retarget; they do not log each frame or bone.

## Dependencies

No new dependencies. The runtime uses the existing Skyrim/CommonLib, Havok loader, SKSE logger and audited OAR 3.1.6/3.2.1 profiles. See [OAR ABI evidence](oar-hooks.md). OAR's authoritative source pin is `f4e7688b065175aff70aa523073857911e15aca3`. Private ABI reads require profile identity and prefix matches.

## Tests and verification

The two preserved [animation cases](../tests/animation_controller_test.cpp) cover entry-phase/complete descriptor sequencing and independent latest-request/continuation/mask behavior. [Graph/math sources](../tests/snapshot_transform_test.cpp) retain earlier consumers. Current CMake does not register these test targets; they are reference material.

[Build verification](build.md#verification-and-limits) records the prior native results and separates them from current build-only checks and unverified Skyrim hooks, native loading, visuals and lifetime behavior. [Debugging navigation](code-maintenance.md#debugging-navigation) maps request/asset/stage/cancellation/retarget logs to their owners.

## Safe modification

Keep lifecycle/event observation in the manager, per-actor requests in the SceneGraph/controller and query ABI reads in OAR. Preserve loading leases, source handles, graph generation checks, latest-request checks and existing graph parking. Do not add fallback chains to compensate for unsupported metadata. Extend action planning rather than changing source actor graphs or emitting world events.

## Synchronized files

Keep the animation module, direct character/render/panel consumers, [CMakeLists.txt](../CMakeLists.txt), both test entry points, [README](../README.md), [Changelog](../CHANGELOG.md), [maintenance map](code-maintenance.md), [build guide](build.md), [OAR query history](oar-preview-query-2026-10-16.md) and [ABI history](oar-hooks.md) synchronized.

## Related history

The [2026-10-16 query report](oar-preview-query-2026-10-16.md) and [OAR hook report](oar-hooks.md) retain earlier experiments. Their process-global scheduler and Start-only dwell claims are historical and superseded by this document. The sole preview owner is documented in [SceneGraph previews](scene-graph.md); Actor/puppet diagnostic routes are retired. Rendering and graph retirement retain their existing policies.
