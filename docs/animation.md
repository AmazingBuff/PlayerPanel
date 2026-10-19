# Actor animation previews

## Purpose

Continue a standing preview from the source actor's observed entry phase, then hand over to a stable idle. Equip enters at its sampled local time, hands over to a complete Start, and reaches Loop. A missing Loop cannot turn Start into an indefinite idle.

## Scope and non-goals

This module covers standing, weapon draw/sheathe and optional hand posture. It supports independent actor records internally. It adds no multi-actor UI, movement/attack mirroring, world animation events, physics integration or safe native graph teardown. Optional hand contributors use an explicit hand/finger mask; no separate hand layer is enabled by default.

## Architecture

`PanelMonitor` records InventoryMenu state and a revision. `CharacterManager::update` distributes lifecycle changes, samples each registered source and consumes equipment revisions inside the serialized render bracket. Event callbacks only increment watched actor revisions.

Each `CharacterClone` owns `MonitorState` and `animation::AnimationController`. Observation retains the actor handle, stance, clip index/local phase, equipment revision, source graph generation, request generation and dirty reason. Assembly state is separate. Replacing the detached display graph calls `replace_graph` and retargets the same controller; it preserves playback time and maps every node again.

OAR is a query boundary. It audits conditions and returns copied replacement, variant and candidate descriptors. The animation module plans ordered actions, owns accepted variant history and caller-local Random state, loads clip leases, advances only the visible stage, composes pose contributors/crossfade, then writes each skeleton node once. `SceneGraphCopy::capture` only copies/audits a supplied actor graph and parks retired allocations. Renderer pumps the manager and draws.

## Code map

| Path | Stable entry point |
| --- | --- |
| [action.h](../src/animation/action.h) | `ActionKind`, `ActionPrototype`, folder compatibility rules |
| [sequence.h](../src/animation/sequence.h) | `idle_sequence`, `PlaybackTimeline`, `resume_continuation`, `RequestGeneration`, `BoneMask`, `compose_contributor` |
| [controller.h](../src/animation/controller.h), [controller.cpp](../src/animation/controller.cpp) | `SourceObservation`, `AnimationController::request`, `retarget`, `poll`, `advance` |
| [clip.h](../src/animation/clip.h), [clip.cpp](../src/animation/clip.cpp) | `ClipPlayback`, `resolve_binding_playback`, `sample_animation_clip`, `strongest_idle_clip` |
| [pose.h](../src/animation/pose.h), [variant_state.h](../src/animation/variant_state.h) | pose blend, assembly settle gate, `SelectionState` |
| [character_clone.h](../src/character/character_clone.h) | `CharacterClone::MonitorState`, `update_monitor`, `observe_source`, `schedule_animation`, `replace_graph` |
| [character_manager.cpp](../src/character/character_manager.cpp) | `CharacterManager::install`, `EquipSink::ProcessEvent`, `update`, `release_clones` |
| [scene_graph_copy.cpp](../src/character/scene_graph_copy.cpp) | `SceneGraphCopy::capture`, `retire` |
| [hooks.h](../src/oar/hooks.h), [hooks.cpp](../src/oar/hooks.cpp) | `AnimationDescriptor`, `AnimationSelection`, `query_animation` |
| [query_policy.h](../src/oar/query_policy.h), [selection_state.h](../src/oar/selection_state.h) | project catalogue, caller-owned condition Random state, copied variant metadata |
| [panel_monitor.cpp](../src/panel/panel_monitor.cpp) | InventoryMenu state/revision only |

## Interfaces

Action prototypes are StandingIdle, Equip, optional WeaponIdleStart, WeaponIdleLoop, Unequip and optional HandIdle. Weapon forms parameterize base names; entry phase is an input snapshot. `SourceObservation` is the controller request input. `AnimationSelection::name` and descriptor `path` carry logical replacement paths; `binding_name` identifies the possibly deduplicated engine catalogue entry. `index` remains the actual loading index. No native candidate pointer escapes the read lock.

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

[animation_controller_test.cpp](../tests/animation_controller_test.cpp), target `CharacterPanelAnimationTest`, test `animation_controller`, contains exactly two focused cases: Equip phase through a sparse/aliased descriptor family to Start/Loop (including missing Loop handling), and independent actor request/Random/playback state with acceptance-time continuation and masked pose output. [snapshot_transform_test.cpp](../tests/snapshot_transform_test.cpp) remains a legacy compile target; its whole behavioral suite is not rerun for this change.

```text
cmake --preset Release
cmake --build build --config Release --target CharacterPanel
cmake --preset Release-scopy
cmake --build build-scopy --config Release --target CharacterPanel CharacterPanelAnimationTest CharacterPanelCopyMathTest
ctest --test-dir build-scopy -C Release -R "^animation_controller$" --output-on-failure
```

Native builds need access to the installed MSVC/Windows SDK. Existing installed vcpkg packages can be used with manifest installation disabled. No game is available here: local tests/builds do not establish game-time hooks, asynchronous loader behavior, graph lifetime or visual correctness. Verification results are recorded with the implementation result.

## Safe modification

Keep lifecycle/event observation in the manager, per-actor requests in the clone/controller and query ABI reads in OAR. Preserve loading leases, source handles, graph generation checks, latest-request checks and existing graph parking. Do not add fallback chains to compensate for unsupported metadata. Extend action planning rather than changing source actor graphs or emitting world events.

## Synchronized files

Keep the animation module, direct character/render/panel consumers, [CMakeLists.txt](../CMakeLists.txt), both test entry points, [README](../README.md), [Changelog](../CHANGELOG.md), [maintenance map](code-maintenance.md), [OAR query history](oar-preview-query-2026-10-16.md) and [ABI history](oar-hooks.md) synchronized.

## Related history

The [2026-10-16 query report](oar-preview-query-2026-10-16.md) and [OAR hook report](oar-hooks.md) retain earlier experiments. Their process-global scheduler and Start-only dwell claims are historical and superseded by this document. Rendering, material handling, diagnostic puppet routes and the graph retirement policy remain governed by their existing documents.
