# Native DAC inventory sessions

## Purpose

Let the actual Dynamic Animation Casting plugin prepare an equipment-driven non-combat idle and process accepted preview annotation events against the source Actor during InventoryMenu, then restore the captured animation-marker effect state when the inventory closes. SceneGraph remains the sole preview display owner. CharacterPanel no longer parses DAC TOML or emulates magic effects.

## Scope and non-goals

This transaction covers bounded, self-targeted inert marker spells. It does not roll back inventory, equipment, Actor values, arbitrary scripts, projectiles or general magic gameplay. It does not create an Actor, advance the original behavior graph, send synthetic draw/sheathe events, or change rendering/calibration/graph retirement.

Native DAC remains responsible for matching events, filters, exclusive groups and casting. CharacterPanel reads its loaded metadata only to decide whether the entire emitted event is safe to borrow and restore. It never edits DAC's configuration, rule vector, custom spells or cooldown/RNG state.

## Architecture and lifecycle ordering

Each `SceneGraph` owns a `dac::Session`. A coordinator keeps weak registrations; queued UI tasks retain their own shared task state, so display retirement cannot silently lose an entry snapshot.

1. The private DAC InventoryMenu sink marks the menu open and captures each registered Actor's controlled effects once. Records created while open capture on their first valid UI-task initialization, before any preview dispatch. DAC metadata is read at this later boundary, after all plugins' DataLoaded handlers have finished; `TomlLoaded=false` means wait, rather than a permanent identity failure.
2. Accepted HKX annotations enqueue native processing through SKSE's `AddUITask` queue. Menu/world epochs and animation request generation are checked before dispatch and immediately before mutation. Appearance-only refresh adopts the new request generation without rewinding accepted playback; old queued requests remain stale.
3. Each whole event is preflighted against all native-loaded rules with that tag. A supported event calls only DAC's `ProcessEvent`, with the real Actor as holder. Its PC/NPC wrappers are excluded because they also call the original Actor graph sinks. The instant caster's prior dual-casting flag is restored after the call.
4. Native effect signatures are observed on valid UI tasks. Actual visible effect changes increment a revision used by animation's interrupt/loop queries and pending-choice checks. A completed function call does not prove that a paused engine has activated the effects; later observation remains authoritative. No virtual expiration clock is used.
5. On the actual InventoryMenu close callback, the coordinator first closes the menu epoch, rejects queued events, then synchronously closes each transaction and restores it before returning. It does not wait for a later render poll. Engine callbacks run outside bookkeeping locks. [SKSE's UI queue](https://github.com/ianpatt/skse64/blob/master/skse64/Hooks_UI.cpp) runs `AddUITask` delegates after the original UI command/event processing in the same `UIManager::ProcessCommands` invocation. A close event therefore invalidates epochs/restores before previously queued UI work is considered; the queue continues with paused UI.
6. PreLoadGame/NewGame invalidate world/menu epochs and discard old-world snapshots before CharacterManager releases previews. They never restore old values onto a newly loaded Actor with the same form ID. Actor handles also prevent form-ID reuse from identifying a different live source.

Recapture, appearance/equipment changes, source graph replacement and temporary display hiding do not recapture or close the DAC transaction. They still update animation requests and pose-node mappings. Actor deletion/record retirement invalidates future events; a retained cleanup task restores only while the original world and Actor handle remain valid.

## Non-combat idle goal preparation

`Session::prepare_idle` supports the supplied Smooth package's literal `Smooth_loop` protocol: loop-on, combat-off and combat-loop-off spells pass through the original native handler and the same whole-tag safety preflight as annotations. This explicit package goal is not generic phase inference or a reimplemented cast. Native filters still decide what applies.

The transaction captures entry effects first. `smooth_idle_goal` derives expected persistent on-effect identities from native-loaded literal Spell/Ability records' constant casting types and off-keyword clearance from instantaneous dispel records. No form IDs, TOML parser or virtual markers are introduced. `idle_goal_ready` observes actual non-dispelled effects: required on-markers must be active and matching off-keyword effects absent. Call completion alone cannot publish a candidate.

`IdlePreparation` exposes current-request pending/ready/failed progress. Each preparation request queues at most one native cast. An unchanged compatible refresh of a ready goal queues observation only. Refresh while preparation is pending starts one attempt for the newer generation; stale queued work is rejected. Retained compatible clip leases wait behind this gate, then pass the current query/generation/revision checks before acceptance. A failed goal not yet consumed by the controller may likewise receive a new attempt on a newer request; once the controller retains its direct fallback, compatible refresh skips preparation until equipment changes or its graph/controller lifetime resets.

Coalesced UI observation invalidates readiness if markers disappear. Missing session/tag support, unsafe rules or a two-second observation timeout yields the stable base fallback rather than an automatic preparation/query/recast loop. Direct empty-hand MT_Idle bypasses preparation and annotation dispatch. These choices never replace the original entry snapshot; only the controlled effect groups actually touched are restored at UI close.

Existing world/menu/request checks immediately before mutation reject obsolete work. UI close still restores original groups synchronously. Paused goal application/cleanup and restore visibility remain game-only evidence.

## Original binary compatibility and provenance

Runtime support is for the user's untouched original `loki_DynamicAnimationCasting.dll` from **Dynamic Animation Casting NG Plus 3.2.4**. Its embedded PE FileVersion/ProductVersion is **3.2.3**. The package label and embedded version are distinct; no source commit is inferred from either.

| Item | Original package identity |
| --- | --- |
| Archive | `Dynamic Animation Casting NG Plus-73293-3-2-4-1686888583.zip` |
| Archive SHA-256 | `668c6e0b7eb63e51b21077ab60db8c011671a1077858ecfd94782551fbe3be5a` |
| DLL | `loki_DynamicAnimationCasting.dll`, 1,676,800 bytes |
| DLL SHA-256 | `86fbe560e425ae61721194a79874ada5a9f1b5bad31509f4a0e6c8ce07db964a` |
| Matching PDB | 34,107,392 bytes, SHA-256 `448f269bd9e76f166043dcabea778edbe5629bd6a1dceee8688bb2effb470b9e` |
| PE timestamp / image size | `0x648BDAC4` / `0x19E000` |
| DLL RSDS and PDB GUID / age | `{F32C3083-CFB0-4B85-A0AE-FD8F7E09F9E7}` / 3 |
| DAC-only ProcessEvent RVA | `0x61FA0` |
| loaded CastTriggers / TomlLoaded RVA | `0x17A780` / `0x182A80` |
| LoadTomls / native Invoke RVA | `0x61370` / `0x82A00` |
| CastTrigger / WeaponTrigger size | 232 / 40 bytes |

`native_profile.h` contains the generated original module name, entry prefixes, field offsets and identity. Runtime matching requires the exact PE/CodeView identity, handler/LoadTomls prefixes and bounded native vector layout. A version label alone admits no other build. The original PDB confirms the fields; disassembly shows the handler iterating its vector with stride `0xE8`, calling native Invoke and returning directly. PC/NPC wrappers (`0x62170`/`0x621C0`) additionally forward to saved original Actor handlers and are never used for preview dispatch.

This binary lacks the later source's `isSprinting` field. Its small spell/custom arrays are 24 bytes: allocator at offset 0, count base at 16, inline/heap storage at 8, allocator capacity in bits 0–30 and local flag in bit 31. These measured layouts match the pinned SDK's read-only array access used by the adapter. No native rule pointer is stored as snapshot state.

Missing or mismatched original DAC disables native phase events with one diagnostic; detached preview playback remains available. Users keep their original DAC installation untouched and replace only CharacterPanel.dll. The plugin is not renamed, patched, rebuilt or deployed by this project.

## Smooth Moveset compatibility boundary

For the core equipment-driven non-combat idle goal, install the project's existing Skyrim/SKSE prerequisites, OAR, the supported original DAC binary above, and Smooth Moveset's own `Smooth Moveset.esp`, `SmoothMoveset.toml`, HKX files and OAR configs. No additional sequence-control plugin is needed for that mechanism. The known Smooth_loop event prepares real markers before OAR selects the eligible idle; accepted annotations maintain native state; folder names never schedule a playlist. This does not establish support for every equipment action or branch in the pack.

| Branch | Config requirement | Current preview boundary |
| --- | --- | --- |
| Core non-combat idle phases | Smooth annotations, DAC rules/ESP markers and OAR conditions | Smooth_loop preparation and observed-readiness gating are implemented; game-time behavior remains unverified. The 16 supported DAC rule records are not proof of complete OAR branch coverage. |
| Equipment placement | `IED_GearNodeEquippedPlacementHint` from `OpenAnimationReplacer-IEDConditionExtensions`, required version `1.0.0.0` | Pending an explicit IED condition-query adapter. Installing IED and its provider alone does not make these candidates supported. |
| Combat idle Start and Loop | `HasMagicEffect(TrueDirectionalMovement.esp\|0x804, Active effects only=false)` **OR** `IsCombatState == 1`, in addition to the other Smooth conditions | The current standing preview projects `IsCombatState` to zero. World combat alone therefore cannot satisfy this alternative in the preview; the TDM effect branch may satisfy the OR if present, but this panel deliberately clears Smooth combat markers for its non-combat goal. |

IED placement is used by substantial equipment branches, including `1hm Equip Base` and `1hm Non-Combat Equip Base` with placement ID `<= 1`; back-weapon variants use `== 2`. It is not merely a cosmetic optional condition. The current [OAR audit](../src/oar/hooks.cpp), `audit`, rejects every enabled condition with a nonempty required-plugin name using `third-party condition requires an explicit preview adapter`. No IED adapter exists in the current policy. That diagnostic identifies an unsupported condition-query path, not missing DAC phase state; do not bypass the condition or treat installing the provider as the implementation fix.

The TDM predicate is an OR branch, not a universal prerequisite for the core non-combat phase path, and the two predicates are not both required. [Current query policy](../src/oar/hooks.cpp), `evaluate_thunk`, supplies the combat-zero projection. These are static/config and implementation boundaries: native effect visibility while paused, restoration, visuals and complete pack compatibility still require game validation.

## Controlled scope and restoration

Supported native-loaded rules use literal spells, self-targeting, zero resources/cooldown and chance one. Dynamic/custom spells, external targets, weapon/hand-node casts, replacement-casting globals and non-restorable spell records reject the whole preview event, even when another matching rule is supported. Native conditions are never reimplemented or weakened.

Loaded Spell/Ability records and their MGEFs must be self-targeted constant/ability or FireAndForget markers, with no effect conditions, nonzero spell cost, hostile/counter-dispel behavior or area/projectile/explosion/associated-form gameplay. Unattached Script archetypes and zero-magnitude ValueModifier/PeakValueModifier effects are allowed. `TESForm::HasVMAD()` is not a gate here: it is the SKSE VM-handle heuristic and returns true for script-referenced forms (the 2026-10-20 log showed true for all 66 rule spells while the Smooth spell/MGEF records contain no VMAD subrecord). Native casting owns constant presence, instant-off behavior, keyword dispelling and any paused activation timing. All 16 supplied Smooth rules fit the documented static metadata subset; actual native-loaded acceptance requires a game log.

The entry scope includes supported literal spells and existing restorable spell groups that share incoming dispel keywords. Before every event, collisions with newly active unrelated/non-entry groups are rejected. Touched scope includes keyword-affected groups, not only the event's literal spells.

Snapshots contain values: source spell/effect/index, caster handle, source item ID, casting source, inactive/dual flags, magnitude, duration and elapsed time. They contain no saved `ActiveEffect*`. Close compares controlled groups with the entry values, leaves identical groups intact, dispels changed groups using `MagicTarget::DispelEffect` outside the effect-list visitor, and recreates baseline effects using `MagicTarget::AddTarget` plus a bounded post-creation modifier. Unrelated effects and current equipment remain outside the transaction.

The pinned SDK declares `IPostCreationModification`'s destructor without defining a linkable implementation. The adapter uses a local borrowed MSVC mirror of its documented destructor/ModifyActiveEffect vtable slots for synchronous AddTarget; it does not guess an engine destructor address. Callback invocation and AddTarget results are checked, then the actual logical effect list is compared. A `MISMATCH` log is a restoration failure, not success. Dispelled entries are treated as logically removed. Exact paused engine cleanup and callback timing still require runtime verification.

## Code and verification map

| Path | Entry point |
| --- | --- |
| [session.h](../src/dac/session.h), [session.cpp](../src/dac/session.cpp) | `Session`, `install`, `reset_world`, private MenuSink and UI-task lifetime |
| [idle_goal.h](../src/dac/idle_goal.h) | `IdlePreparation`, `IdleReadiness`, current-request progress |
| [session_core.h](../src/dac/session_core.h) | `SessionCore`, `EventTicket`, `EffectValue`, `Restoration` |
| [native.h](../src/dac/native.h), [native.cpp](../src/dac/native.cpp) | `native_rules`, `native_dispatch`, exact identity and metadata preflight |
| [native_profile.h](../src/dac/native_profile.h) | generated exact binary identity, entry prefixes and field offsets |
| [effects.h](../src/dac/effects.h), [effects.cpp](../src/dac/effects.cpp) | `entry_scope`, `capture_effects`, `event_scope`, `restore_effects`, `effect_signature`, `smooth_idle_goal`, `idle_goal_ready` |
| [inspect_binary.py](../src/dac/inspect_binary.py) | read-only original DLL/PDB pairing, symbols/layouts and offline profile evidence |
| [MIT-DAC.txt](../src/dac/MIT-DAC.txt) | upstream MIT notice |

## Interfaces and failure modes

`prepare_idle` queues or observes the known goal; `idle_readiness` exposes observed progress. `Session::request` publishes the current animation generation; `dispatch` queues a literal annotation tag, `observe` coalesces native effect observation, and `revision` exposes only completed actual-state observations. `install` registers the private inventory lifecycle sink; `reset_world` invalidates old-world snapshots and queued work before their display records are released. The engine-free `SessionCore` owns immutable entry values and close/reset ticket invalidation.

An unavailable profile leaves phase events disabled, without a parser fallback. Unsupported native rule scope and newly unrelated keyword collisions reject a whole event once. Missing original caster/source records, failed AddTarget/callback completion or a differing logical effect list produce restoration `MISMATCH`; no synthetic events or saved effect-pointer attachment are used to hide it.

## Dependencies and offline inspection

The root project retains one CharacterPanel Release target/preset and its existing CommonLib/SKSE dependencies. There is no DAC build target, source fetch/patch route or tomlplusplus requirement. Runtime only needs the supported original DAC installation; PDBs, Python and LLVM tools are maintenance-time inputs, not runtime dependencies.

Maintainers can inspect read-only original files with the Python standard library and existing LLVM readers. From the repository root:

```powershell
python src/dac/inspect_binary.py --dll "<original-folder>/loki_DynamicAnimationCasting.dll" --pdb "<original-folder>/loki_DynamicAnimationCasting.pdb" --header src/dac/native_profile.h --evidence build/dac-stock-validation/inspection
cmake --preset Release
cmake --build --preset Release
```

The inspector checks PDB GUID/age against DLL RSDS before emitting a profile. It records original DLL/PDB hashes, module name, package label, embedded-version label, field lists, publics and raw/wrapper disassembly. Its supplied defaults are package NG Plus 3.2.4 and embedded version 3.2.3; `--package-label`/`--embedded-version` make alternate maintenance input labels explicit. It never certifies a source revision. Output paths cannot overwrite either input.

Only original bytes and paired symbols establish compatibility. A maintainer must review the native entry/call path and reader layouts before adopting a different artifact. The tool never compiles, patches, loads, installs or deploys DAC. The unnecessary construction helpers and generated reference output were removed; prior immutable task records remain historical evidence only.

## Verification and limits

This compatibility change ran exactly two offline inspector cases: the supplied original DLL/PDB reproduced the profile and authoritative inspection; a known mismatched PDB was rejected before any new header was emitted. Original archive/DLL/PDB hashes remained unchanged. No original DAC was built or executed. CharacterPanel's Release configure/build remains the native compile check.

The current [two policy/preparation cases](../tests/animation_controller_test.cpp) use production policy/progress/transaction types and a narrow fake Actor boundary for default and direct MT_Idle behavior. They do not execute native dispatch, magic or parenting APIs. Prior paired/mismatched inspector evidence is separate and is not rerun for this policy change. See [build guidance](build.md#verification-and-limits).

No game is installed here: live profile acceptance, paused activation/expiration, synchronous AddTarget/dispel ordering, restoration visibility before world animation, hooks, visuals and native lifetime remain unverified. Inspect `DAC snapshot`, event/native revision, rejected-event and `DAC restore ... result=matched|MISMATCH` logs with the matching CharacterPanel binary. A native call-completed line is distinct from an observed effect revision.

## Invariants and safe modification

Keep native processing in DAC, transaction authority in this module and clocks/poses in animation. Preserve epoch invalidation before restore/reset, no lock-held engine callbacks, immutable entry snapshots, current request/effect revision checks, safe group-based dispel and engine creation. Scheduling follows SKSE's [UI queue](https://github.com/ianpatt/skse64/blob/master/skse64/Hooks_UI.cpp) rather than its separate [general task pool](https://github.com/ianpatt/skse64/blob/master/skse64/Hooks_Threads.cpp). Additional spell semantics need evidence and a genuinely restorable boundary; do not add parser/emulator fallbacks, Actor graph dispatch or pointer resurrection. Retain `hash_combine`/`Amazing_Hash` for computed process keys.

## Synchronized files and related history

Maintain this document with [animation](animation.md), [README](../README.md), [Changelog](../CHANGELOG.md), [build](build.md), [OAR hooks](oar-hooks.md), [SceneGraph](scene-graph.md), [maintenance](code-maintenance.md), [runtime notes](../tools/scene_copy/README.txt), the DAC adapter/inspector sources and focused tests. Previous virtual marker parsing/projection and folder playlists are superseded; dated experiment reports remain historical. Rendering and parked native graph retirement remain unchanged.
