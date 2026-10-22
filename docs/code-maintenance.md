# Code maintenance

The sole current preview backend is [SceneGraph](scene-graph.md); animation ownership and phase rules are documented in [animation previews](animation.md). Dated reports linked below remain historical evidence for their recorded revisions.

## Current ownership and source map

| Module | Responsibility |
| --- | --- |
| [scene_graph.h](../src/character/scene_graph.h), [scene_graph.cpp](../src/character/scene_graph.cpp) | persistent per-source-actor observation/controller, F7/F8 intents, capture/retarget decisions, placement/calibration, animation and draw |
| [character_manager.cpp](../src/character/character_manager.cpp) | source registry, equipment-event revisions and menu/frame/lifetime distribution |
| [scene_graph_capture.cpp](../src/character/scene_graph_capture.cpp) | private SceneGraph native copy, writable-isolation audits and process-lifetime retirement |
| [equipment_state.cpp](../src/character/equipment_state.cpp) | read-only hand/form observations |
| [figure_audit.cpp](../src/character/figure_audit.cpp), [snapshot_transform.h](../src/character/snapshot_transform.h) | appearance-settle signature, collision filtering, placement and subtree math |
| [animation/controller.cpp](../src/animation/controller.cpp), [sequence.h](../src/animation/sequence.h) | isolated requests/leases, current-state choices, accepted continuation, event clocks and composed poses |
| [animation/event_clock.h](../src/animation/event_clock.h), [event_clock.cpp](../src/animation/event_clock.cpp) | retained HKX annotation timing; no DAC rules or effect emulation |
| [animation/dac/session.cpp](../src/animation/dac/session.cpp), [native.cpp](../src/animation/dac/native.cpp), [effects.cpp](../src/animation/dac/effects.cpp) | UI task/menu epochs, original native DAC handler/loaded metadata, marker-scope preflight and engine snapshot restoration |
| [animation/clip.cpp](../src/animation/clip.cpp) | equipment-independent binding resolution, track→bone mapping and detached pose samples |
| [animation/oar/hooks.cpp](../src/animation/oar/hooks.cpp) | verified isolated OAR queries/eligible metadata and optional current-world API mapping |
| [animation/oar/base_animation_cache.h](../src/animation/oar/base_animation_cache.h), [query_policy.h](../src/animation/oar/query_policy.h) | finite project base indices/missing aliases; `QuerySession` keeps condition state only |
| [panel_monitor.cpp](../src/panel/panel_monitor.cpp) | InventoryMenu state and revision only |
| [input.cpp](../src/input/input.cpp), [main.cpp](../src/main.cpp), [renderer.cpp](../src/render/renderer.cpp) | queued intent, installation/lifetime signals, serialized update and drawing |
| [figure_passes.cpp](../src/render/studio/figure_passes.cpp), [figure_draw_order.h](../src/render/studio/figure_draw_order.h), [figure_pipeline_state.h](../src/render/studio/figure_pipeline_state.h) | existing material/pass submission and transparency/state restoration |
| [d3d11_util.cpp](../src/render/dx11/d3d11_util.cpp) | render-target and device binding protection |

## Runtime hash composition

Custom process-local keys and fingerprints use [base/def.h](../src/base/def.h) `hash_combine`, starting from `Amazing_Hash`. The seed remains derived from `Plugin::Plugin_Author` by the supplied `hash_str`; do not duplicate the algorithm or hardcode a second seed. Combine logical fields individually in a fixed order, never raw struct bytes.

- [StateKeyHash and condition RandomKeyHash](../src/animation/oar/selection_state.h): owner → group → scope, then condition for the random key.
- [Animation RandomKeyHash](../src/animation/variant_state.h): extend the same StateKeyHash with condition.
- [query_animation](../src/animation/oar/hooks.cpp): project/wanted pointer → base index → channel for `clip_key`.
- [FigureAssembly::signature](../src/character/figure_audit.h): geometries → skinned → bound_nodes → alloc_slots, retaining the full `size_t` result.

The [animation test source](../tests/animation_controller_test.cpp) compiles the production idle policy, preparation progress and transaction core directly, without the generated author metadata or a CMake target. Production retains generated Plugin metadata from `PROJECT_AUTHOR`. Packaging SHA-256 checksums in historical records, Git/native identifiers, bit masks, PRNG seeds and the helper/bootstrap implementation keep their distinct semantics.

## Build, verification and controls

[Build guidance](build.md) owns the pinned checkout/toolchain, one Release preset, installed-dependency reuse, outputs and configure-time identity. The supported commands are `cmake --preset Release` followed by `cmake --build --preset Release`; the output is `build/Release/CharacterPanel.dll` with a matching PDB.

[DAC maintenance](dac.md#dependencies-and-offline-inspection) uses `src/animation/dac/inspect_binary.py` against untouched original DLL/PDB inputs. Python/LLVM are offline tooling only; keep the original DAC installation and replace only CharacterPanel.dll.

F7 queues recapture and F8 toggles display in a paused inventory. Current CMake does not expose test, package, install or compatibility entry points. The [current four direct-compiled drivers](build.md#current-animation-checks-2026-10-22) recorded **2 DAC / 2 torch-shield / 2 mapping / 1 budget** cases; choose only the relevant driver using the self-contained output-directory command. Older policy/IED/attachment/cache and retained graph/math sources are separate evidence, not a new suite. Build and source review do not establish game-time correctness.

## Debugging navigation

[main.cpp](../src/main.cpp), `initialize_log`, writes `CharacterPanel.log` under the directory returned by `SKSE::log::log_directory()`. Use that runtime-selected location rather than a guessed machine path. The current info-level log includes timestamps and thread IDs. Keep a returned log with the exact DLL/PDB pair and start with its `SCENEGRAPH BUILD` identity; that identity is generated at configure time, so a later commit alone does not update it.

Begin with the [current section-9 symptom procedure](diagnostics/display-issues-2026-10-22.md#9-2026-10-22-换装片源与站立谓词修复) (the [section-8 budget](diagnostics/display-issues-2026-10-22.md#8-2026-10-22-已报告问题的关联诊断) still defines the pose windows). Normal accepted PANEL mappings and paused-source WORLD snapshots stay on. `[Diagnostics] DetailedAnimationTrace=false` is the default in [CharacterPanel.ini](../config/CharacterPanel.ini); enable it and restart only for detailed investigation. Ordinary info flushes about once per second, warnings/errors immediately. The POSE budget is four windows/twenty-four samples per request, with the twelve existing time slots and 4.1-second window expiry; a fixed fifteen-bone source snapshot occurs at most once/request in the existing capture pass. Missing/exhausted records cannot rule out a transient cause.

| Log prefix/fields | Owner and purpose |
| --- | --- |
| `SCENEGRAPH BUILD` with identity/runtime/backend | [main.cpp](../src/main.cpp): match the configured binary before comparing results |
| `CharacterManager watch` / `CharacterManager lifetime boundary` | [character_manager.cpp](../src/character/character_manager.cpp): delimit the source record whose revisions/requests are being followed |
| `CharacterManager equipment actor=… revision=… equipped=…` | [character_manager.cpp](../src/character/character_manager.cpp), `EquipSink::ProcessEvent`: verify the source actor's equipment event reached the registry |
| `ANIMATION actor=… request=… reason=… source-generation=…` | [controller.cpp](../src/animation/controller.cpp), `AnimationController::request`: identify the per-actor request and source generation |
| `ANIMATION PANEL ... accepted` | [controller.cpp](../src/animation/controller.cpp), `publish_mapping`: final ready accepted base/logical/variant/bound, including loader fallback and accepted reuse; queued/failed proposals are not playing |
| `ANIMATION WORLD ... snapshot=paused-source`, `build-ms`, original/index/bound/OAR fields | `world_clip_report` and `AnimationController::request`: event-driven source observation, not settled post-change pose; API-empty is unknown and construction timing excludes log IO/rendering |
| Detailed `ANIMATION QUERY/DECISION/LOAD/REBIND/EFFECT-REV/PENDING-REV/HANDOFF/ANNOTATION` | request-to-query/load/accept/annotation decisions; distinguish latest `request`, visible `accepted-request` and loader-origin `asset-request` |
| Detailed `POSE PROBE/MAP/SAMPLE/NODE/CAMERA/BUDGET`, `SCOPY SOURCE-SNAPSHOT/CAPTURE/PLACEMENT/CALIBRATION/MOUNT-*` | [probe limits](animation.md#bounded-axe-pose-and-projection-evidence-2026-10-22) and [scene correlation](scene-graph.md#opt-in-capture-placement-and-mount-evidence); join actor/request/probe/graph/capture without inferring weights or source bone semantics |
| `SCOPY BEGIN`, `SOURCE-DIFF`, `AUDIT`, `CENSUS`, `READY`, `FAILED`, `BLOCKED` | [scene_graph_capture.cpp](../src/character/scene_graph_capture.cpp), private `capture_graph`: follow capture ID, correspondence/isolation and rejection reasons |
| `SCOPY PARK` with actor/count or cap warning | private `retire_graph`: tracks retained allocations; the cap limits bookkeeping only and does not bound retained native memory |
| `SCOPY FRAME`, `SceneGraph draw`, `SceneGraph actor=… draw unavailable` | [scene_graph.cpp](../src/character/scene_graph.cpp): framing/calibration, toggle state, target binding and absence of render passes |
| `OAR hooks unavailable/refused`, `OAR … hooks installed` | [hooks.cpp](../src/animation/oar/hooks.cpp), `install_hooks`: distinguish verified query availability from a rejected/unavailable native profile |
| `OAR bases stage=world-prewarm` or `stage=menu-fallback`, with project/table/requested/resolved/missing/elapsed_ms | `prepare_bases`: distinguish preparation before inventory from a late cold fallback; once per ready cached project/table |
| `DAC snapshot`, native event/revision, rejection, `DAC restore ... result=matched|MISMATCH` | [session.cpp](../src/animation/dac/session.cpp), [effects.cpp](../src/animation/dac/effects.cpp): follow immutable inventory entry, native state visibility and checked close restoration |
| Detailed `OAR QUERY ... root-false`, `DAC TRACE/OBSERVED` | original single-evaluation root top-level result or honest unknown, followed by actual enqueue/drop/completion/observed markers; no extra Evaluate or casts |
| Detailed `OAR QUERY ... rejected-candidate`, `OAR COND ... movement-direction=seen answered=` | rejection rows are keyed by OAR project folder plus cause (48/query) with one suppression line; the `OAR COND` row says whether the `IsMovementDirection` hook was reached, separating a policy answer from a condition the hook never saw |

`ANIMATION setting` identifies the loaded [sample option](../config/CharacterPanel.ini); goal/family/direct/drawn logs identify equipment policy. `DAC ... Smooth_loop preparation requested/ready/failed` separates UI submission from observed marker readiness. `SCOPY HELD` reports attached/missing roots and concrete side/form/mount problems. See [idle policy](animation.md) and [held attachment invariant](scene-graph.md#held-equipment-invariant). Ready compatible refresh observes without recasting; pending refresh creates one current-generation preparation attempt, while an already retained direct fallback skips preparation. Check the [precise refresh contract](dac.md#non-combat-idle-goal-preparation) before interpreting repeated request logs.

Detailed `proposal` records are explicitly pending. The normal PANEL `action` is the numeric [ActionKind](../src/animation/action.h), while detailed decisions use human-readable action/reason; see [the action values](animation.md#interfaces). A newer requested generation may load while the older accepted generation stays visible, and a reused loader retains its original asset-request. Follow annotation → DAC queued → skipped/completed → observed marker/revision → next query → accepted mapping. Completion is not readiness; an observed revision alone does not prove which event caused it. See [the correlated chain](animation.md#correlating-reported-issues).

For first-open stalls, follow [fixed-cache readiness/lifecycle](animation.md#fixed-base-indices-and-project-readiness) and the [two focused cases](build.md#fixed-base-index-verification). `panel_base_animation_names` derives all current idle/equip/unequip aliases from the existing action policy; `BaseAnimationProject::resolve` borrows the table once and retains only finite matches/missing status. Table owner/address/count replacement and world release invalidate reuse. Warm queries read only selected bindings by index, preserving deduplication. DAC file preflight, Clone/audits and first asset/GPU use are independent unchanged costs; the initialization log alone does not measure their contributions or establish hitch elimination. The retained graph/math driver shares these two cases but was not run broadly.

The [IED/SDS adapter](oar-hooks.md#native-ied-placement-conditions-2026-10-21) explicitly validates the equipped-weapon placement, gear-node placement and zero-component `SDS_IsShieldOnBackEnabled` schemas. Other names from the same exact matched provider retain native current-source answers without those individual schema/required-version checks. Exact provider/embedded-version/current-hand guards apply to all names; historical IED remains rejected. `OAR IED placement adapter` reports metadata availability/version, not factory or SDS interface registration. `ied_condition_rejection` in [ied_conditions.h](../src/animation/oar/ied_conditions.h) distinguishes provider failures, mismatched checked schemas and historical scope. `audit` also covers custom virtual `Evaluate` before dispatch; `Frame` preserves scoped numeric reads. Native false is valid false. The separate older [two IED cases](../tests/ied_condition_adapter_test.cpp) use the [direct command](build.md#ied-condition-verification), not a fresh game/provider result.

`third-party condition requires an explicit preview adapter` still identifies unrelated third-party providers; consult the [Smooth support boundary](dac.md#smooth-moveset-compatibility-boundary). Do not diagnose it as missing DAC state or bypass it. Confirm the exact provider (archive 1.0.3, embedded 1.0.2.0), its own IED/SDS registration logs and actual hints/settings when checking paused-game behavior. The SDS branch needs its provider-side prerequisites; it is not universally required by the core non-combat goal. Combat configs retain the TDM-effect OR combat-state requirement and current combat-zero projection.

Game follow-up remains two representative categories, not a new test matrix:

1. **Equipment idle policy:** verify the same non-combat equipped target during world draw/attack/move/sheathe, both empty-hand INI choices and rapid equipment changes; correlate actor/request/event/effect/handover logs and confirm controlled source effects restore and unrelated equipment/buffs remain intact.
2. **Copied-graph presentation/lifetime:** F7 without compatible-loop rewind, F8, close/reopen/load boundaries, appearance, attachments, framing and transparency; correlate capture/audit/park and draw logs.

These categories remain unverified without a Skyrim runtime. Use the [current reported-issue procedure](diagnostics/display-issues-2026-10-22.md#9-2026-10-22-换装片源与站立谓词修复) for torch/shield phases, Smooth transitions and the one-shot axe flash. The [2026-10-20 equipment idle/DAC sheet](equipment-idle-dac-test-2026-10-20.md) remains dated historical evidence rather than the current diagnostic format.

## Safety and rendering invariants

- Source graphs/transforms remain read-only. Native `NiObject::Clone` copies a scene graph, not an Actor. The scoped [native DAC transaction](dac.md) temporarily changes only controlled source effects and restores them at close.
- Event/input callbacks record revisions/intents. Capture, resource release, retarget and pose writes run in the serialized bracket.
- Replacing a graph keeps its SceneGraph/controller record and playback phase. Retarget precedes retirement of old nodes.
- Retired graph allocations stay parked. Safe native teardown remains unresolved.
- Preserve effect/collision filtering and attachment drawing, standing-idle calibration and detached roots.
- Preserve material alpha handling, opaque/cutout then ordered transparency, per-pass state restoration and all constant-buffer bindings/subranges. No shader/material change belongs to this cleanup.
- Local tests/builds do not establish game-time native loader, hook, visual or lifetime correctness.

## Historical evidence

The [complete historical index](history.md) keeps past controls, outcomes and proposals separate from current guidance.

[UBE transparency](ube-transparency-test-2026-10-15.md), [OAR ABI](oar-hooks.md), [OAR query experiments](oar-preview-query-2026-10-16.md), [AFP summary](afp-summary-2026-10-13.md), [S2 probes](s2-anim-probe-evidence-2026-10-09.md), [M0 handoff](m0-handoff.md) and [graph-copy validation](scene-graph-copy-validation.md) retain their recorded results. Their former source paths and hotkeys are historical; removed product code remains available through Git history. Evidence archives and old packages were not deleted.
