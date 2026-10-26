# OAR 条件 hook（3.1.6 / 3.2.1）

## Independent graph investigation (2026-10-23)

Current status (2026-10-26): [G1 passed only revised Option A; G2 v17 has not passed](independent-behavior-graph.md#current-status). Repeated load/retention and sampled source equality do not prove loaded-graph teardown or complete isolation. v17 dispatches WeapEquip and changes graph state but the 116-pose buffer remains unchanged; a single event is not full G3. G4 still must verify independent OAR context, history and sinks.

At fixed OAR 3.1.6 revision `0c1c8b38a5323b9e26df41ff9d10ce801b984a45`, `GetReplacementAnimation(hkbCharacter*,...)` derives the outer graph/holder; graph-variable/drawn conditions may read the Actor. Graph addresses do not isolate Actor/ref/submod Random, sequence/PlayOnce history, event sinks or callbacks. The source-only `BehaviorGraphProbe` has a sixteen-row budget and fixed observation labels, distinct from the forty-row v17 `INDEPENDENT GRAPH` inject flow. The OAR adapter still has no verified independent context/history bridge or playback parity; current product queries continue through the existing `PreviewContext`/`PreviewClip` carriers. See [audit limits](behavior-graph-construction-audit.md#audit-current).

## Historical hand and direction boundary (2026-10-23)

The exact IED provider/embedded-version and three ordered schema guards below remain. Historical queries admit checked `SDS_IsShieldOnBackEnabled` (source settings) and `IED_GearNodePlacementHint` (explicit node). Checked `IED_GearNodeEquippedPlacementHint` retains the native answer only if its requested hand's form ID equals the source's current hand. A changed requested hand and unknown historical provider predicates reject before native evaluation. Current-source unfamiliar IED names retain prior behavior. Native false/disabled/negation are preserved; no historical hint or placement is fabricated. [ied_conditions.h](../src/animation/oar/ied_conditions.h), `ied_condition_rejection`, owns schema/history policy; `ied_rejection` in [hooks.cpp](../src/animation/oar/hooks.cpp) reads the verified Bool component only after schema admission.

The authoritative [OAR source](https://github.com/ersh1/OpenAnimationReplacer/blob/f4e7688b065175aff70aa523073857911e15aca3/src/Conditions.cpp) and exact supplied OAR 3.1.6 DLL/PDB establish movement enum 0=None, 1=Forward. `standing_movement_direction` reads the requested numeric direction and returns true only for 0; `finish_predicate` applies negation once. `IsCombatState` keeps its state-0 contract. The earlier all-false movement policy was wrong; this repair does not force Smooth eligibility or create missing markers.

Detailed queries always emit `OAR COND ... movement-seen=yes|no movement-answered=yes|no`, including unavailable base/project paths. Bounded rejection rows also carry those flags for that candidate. `condition_diagnostic` captures public `GetArgument()` text (effect/form/direction when supplied by the condition) only in detailed mode. `root-false` remains the actual failing root top-level condition from its existing Evaluate call; `first-false` is only an observed false leaf and may be an arm of a successful OR. Neither is a reconstructed causal path. Details retain the existing forty-eight distinct project/cause rows and suppression summary; missing or capped evidence is inconclusive. There are no new native hooks, retries of Evaluate, unknown-condition admission or reverse animation-table scans.

Current two-scenario policy/composition evidence and build commands are in [build.md](build.md#animation-repair-checks-2026-10-23); native loading, IED registration/hint freshness, asynchronous layer lifecycle and actual Start/Loop selection remain UNVERIFIED. Keep this boundary synchronized with [animation.md](animation.md), [README](../README.md), [Changelog](../CHANGELOG.md) and the [current handover](diagnostics/display-issues-2026-10-22.md#10-2026-10-23-装备上下文与火把分层修复待上机). No dependencies or binary profiles change.

> Current query/playback boundary: [Actor animation previews](animation.md). The older projection/package sections below are historical.

The integration lives under [src/animation/oar](../src/animation/oar/hooks.h), including its unchanged [upstream ABI headers](../src/animation/oar/api/README.md). Its namespace remains `oar`; native behavior and binary profiles are unchanged by this organization.

## Fixed base metadata (2026-10-21)

[BaseAnimationCache](../src/animation/oar/base_animation_cache.h) owns bounded per-project original-action metadata derived from [action.h](../src/animation/action.h), `panel_base_animation_names`; [QuerySession](../src/animation/oar/query_policy.h) retains only isolated condition state. [hooks.cpp](../src/animation/oar/hooks.cpp), `ready_animation_table` and `prepare_bases`, validate the table boundary once per access, scan it once on initialization using borrowed published engine strings and copy only finite matches. Warm queries and missing aliases do not rescan or copy the expanded table.

Prewarming is metadata-only after the existing third-person graph resolver observes a published graph/behavior/skeleton and `bindings.size() >= animationNames.size()`. At the pinned OAR revision `f4e7688b065175aff70aa523073857911e15aca3`, [Hooks.cpp, HavokHooks::LoadClips](https://github.com/ersh1/OpenAnimationReplacer/blob/f4e7688b065175aff70aa523073857911e15aca3/src/Hooks.cpp) calls `CreateReplacementAnimations` before the original engine loader creates bindings from the expanded names; `CreateSynchronizedClips` may add further binding entries. The guard therefore uses coverage rather than equality, and does not require replacement project registration when OAR is absent/unmatched. Pinned CommonLib's `hkbAnimationBindingSet::bindings` and `hkbCharacter`/`hkbCharacterSetup` provide the existing typed read boundary. A DLL-loaded or merely registered OAR project does not establish readiness. Missing metadata retries without permanent negative entries or a wait loop. Real publication/thread timing remains a game check.

The sparse identity includes the string-data owner, array address/count; world release clears it. No borrowed string is retained across frames. Canonical case/slash, exact-before-gender/suffix and caller alias ordering remain, and OAR/DAR replacement folders are excluded from base resolution. Missing aliases are recorded once after readiness. Read [lifecycle/fallback details](animation.md#fixed-base-indices-and-project-readiness) and [two portable cases](../tests/base_animation_cache_cases.h).

Initialization resolves original names into indices for each actual project/table. In `query_animation`, string-data identity selects the native project, then `s_get_replacements(project, base->index)` locates its candidates by original binding index. Full selection additionally calls `s_evaluate_replacements` with the source Actor and preview clip inside `ScopedEvaluation`; project/index alone does not provide condition answers. The existing native functions, projected context, supported-condition audit and accepted variant/history semantics remain authoritative. See the [stage-by-stage data flow](animation.md#fixed-base-indices-and-project-readiness).

`animation_binding_name` fetches only requested indices, including variant picks and hash-deduplicated aliases; logical replacement paths remain distinct from actual engine binding names. Names are still retained in [PreviewClip::initialize_native](../src/animation/oar/preview_clip.h) carrier fields, loading metadata and logs alongside indices. The selected index addresses the engine binding. No new hooks/profiles, OAR-result cache or shared condition/history state is added. `OAR bases stage=world-prewarm|menu-fallback` reports once per initialization the total table count, finite requested/resolved/missing counts and elapsed time. DAC preflight, Clone/audits and first asset/GPU use remain unchanged, and no measured hitch elimination is claimed.

The finite canonical keys, including Tor_1HMPose, describe original entry aliases, not a replacement-count limit. Custom weapons reusing existing engine families/OAR entry points can use these indices when their conditions are supported. Independent new behavior/clip entries require an explicit family/base mapping; the old full-table catalogue did not discover new action families either. [Custom-weapon boundaries](animation.md#custom-weapons-and-new-behavior-entries) and the existing [two-case verification](build.md#fixed-base-index-verification) explain the scope without claiming complete game/moveset compatibility.

## Native IED placement conditions (2026-10-21)

The preview query explicitly audits three condition schemas from `OpenAnimationReplacer-IEDConditionExtensions`, with loaded provider version **1.0.2.0** (`0x01000020`) and required condition version **1.0.0.0** for those schemas. The supplied archive is labelled **1.0.3**; its embedded SKSE version is 1.0.2.0. Admission uses module/SKSE metadata cached at PostPostLoad hook installation, then checks the exact ordered public component signature:

| Condition | Components (type / name) |
| --- | --- |
| `IED_GearNodeEquippedPlacementHint` | Bool / `Left hand`; Comparison / `Comparison`; Numeric / `Weapon placement ID` |
| `IED_GearNodePlacementHint` | Numeric / `Gear node ID`; Comparison / `Comparison`; Numeric / `Weapon placement ID` |
| `SDS_IsShieldOnBackEnabled` | No components; native source Actor shield-on-back setting |

The complete Smooth source-config scan found 53 equipped-placement, 154 gear-node-placement and 32 `SDS_IsShieldOnBackEnabled` occurrences. These counts identify the pack's used predicates; they do not establish registration or actual selection. The SDS predicate reads `GetShieldOnBackEnabled(actor)` through the provider's existing SDS interface. Its branch needs the provider's own successful SDS interface/condition registration and prerequisites; SDS is not a universal prerequisite for the core non-combat phase goal. No SDS setting or substitute interface is implemented here.

[ied_conditions.h](../src/animation/oar/ied_conditions.h), `ied_condition_rejection` and `evaluate_ied_condition`, own admission and guarded dispatch. [hooks.cpp](../src/animation/oar/hooks.cpp), `audit`, checks the complete tree before any native custom `Evaluate`, including overrides that bypass OAR's base detour. `evaluate_set_thunk` and `evaluate_condition` keep a `Frame` around native evaluation so root custom numeric components use the existing preview graph projection. `evaluate_thunk` forwards admitted provider predicates through its original trampoline without recursively calling their virtual `Evaluate`. No IED state, Actor equipment or graph state is written.

The provider's own `Evaluate` retains source reference, hand/node selection, comparison, shield settings, disabled and negation. Native false remains supported false. Current-source unfamiliar names preserve native behavior. Exact provider/version and the three checked schema/required-version guards remain. Historical permission is dependency-specific as documented in the current section: settings/explicit node are admitted; equipped hints need an unchanged requested hand; unknown historical names reject. No Actor, IED or SDS state is mutated.

`OAR IED placement adapter` logs distinguish unavailable, mismatched and available metadata at hook installation. Query diagnostics distinguish provider failure, unsupported schema/version, historical-hand scope and unrelated third-party rejection. Available metadata does **not** establish successful OAR factory registration. PlayerPanel does not create factories, rebuild the provider or repair registration; install the original provider and its IED prerequisites separately. A rejected candidate follows the existing OAR next-candidate/base policy.

Provenance: user-supplied `Open Animation Replacer - IED conditions-98308-1-0-3-1693587943.zip`, archive SHA-256 `e46bcb8087f4f95ce59ae0d4aa1ea16fcf38d348f5b957d4eb0b2792a7935a0a`; its unchanged `SKSE/Plugins/OpenAnimationReplacer-IEDConditionExtensions.dll`, SHA-256 `8669298ea4e640e2ac7867ecdf29be59b7e2f56014831b11b99a14bdb5b3b698`. Offline PE inspection reads exported `SKSEPlugin_Version` at RVA `0xFD030`; no provider DLL was executed or modified here.

Primary semantic references: the provider author's [Conditions.h](https://github.com/SlavicPotato/OpenAnimationReplacer-IEDConditionExtensions/blob/master/src/Conditions.h) and [Conditions.cpp](https://github.com/SlavicPotato/OpenAnimationReplacer-IEDConditionExtensions/blob/master/src/Conditions.cpp), retrieved 2026-10-21 (source tree version 1.0.1; binary source revision unknown). The pinned OAR [API request implementation](https://github.com/ersh1/OpenAnimationReplacer/blob/f4e7688b065175aff70aa523073857911e15aca3/src/main.cpp) supports interface V2/V3; this is distinct from condition-object `ConditionAPIVersion` V3/V4. The supplied DLL requests interface V2. These facts alone do not verify live registration or message ordering. No provider implementation was copied.

The two [focused admission/dispatch cases](../tests/ied_condition_adapter_test.cpp) and [Release build instructions](build.md#ied-condition-verification) are local evidence. Real provider registration, native evaluation, IED hint refresh while paused, appearance and missing-provider fallback in Skyrim remain unverified. See the [historical follow-up summary](history-animation.md#dac-ied), with original procedures recoverable through [the source index](history-sources.md); loading the DLL is insufficient evidence of a matching condition.

## Torch/shield query follow-up (2026-10-22)

The archived TOR_Idle failures include the core `CurrentWeatherHasFlag` predicate. [policy.h](../src/animation/oar/policy.h), `forwards_source`, now admits that name through the existing native callback. The pinned OAR implementation, [CurrentWeatherHasFlagCondition::EvaluateImpl](https://github.com/ersh1/OpenAnimationReplacer/blob/f4e7688b065175aff70aa523073857911e15aca3/src/Conditions.cpp), reads `Sky.currentWeather` flags; absence/native false and native negation are preserved. No weather projection, blanket unknown-condition admission or new ABI entry is introduced.

[hooks.cpp](../src/animation/oar/hooks.cpp), `condition_diagnostic`, includes provider and condition name in unsupported core/provider evidence. With detailed tracing enabled, the existing `QueryFrame` records candidate rejection detail; `evaluate_replacement_thunk` publishes **one full-path detail per distinct project/cause pair** (the first candidate of each OAR project folder plus provider/condition/root-false signature, at most forty-eight), then one suppression line with the remaining rejection counts. The earlier fixed eight-unsupported/two-native-false reservation is retired: a pack whose whole replacement set shares one condition (RaySense's cover animations all read `RaySense_Obstacle`) consumed every slot before a later folder such as `Smooth Moveset - Axe/Axe Non-combat Equip Base` was reached, which is why the 2026-10-22 r1 log explained the wrong equip clip only after the second read. A supported native false is labelled `native conditions or eligibility returned false`, not an unsupported adapter failure. The query's first aggregate rejection may coexist with a successfully selected later candidate; traced proposals call it `first-query-rejection` to avoid blaming that winner.

The first torch/shield package read the indexed binding independently, but still reverse-scanned the full table and described replacement members for logical lookup. That expensive diagnostic implementation and its `lookup/replaced/original-index` fields are superseded by [the official current-clip API mapping below](#low-overhead-mapping-and-opt-in-traces-2026-10-22). Empty replacement metadata remains inconclusive. This diagnostic never advances the graph, evaluates new conditions, picks variants or alters panel selection.

WORLD rows at panel-open/equipment requests explicitly say `snapshot=paused-source` and include request reason, current hand IDs and actual source drawn state. They can precede or trail a native equipment transition; they are not a settled post-change reference animation. The archived log has no world torch snapshot. Its world request 3 index `9037` and request 44 index `10985` already equal known panel replacement indices despite empty historical `replaced` strings. Request 42 also selected Smooth shield Equip `8905` then Start `9044`, so that log does not establish blanket IED failure. A parallel `DW_ShieldAdjustment` node has no recorded weights/masks and is not proof of a missing displayed layer.

The [equipment policy](animation.md#equipment-context-and-torch-composition-2026-10-23) retains previous query hands and typed intent for every Unequip and adds the observed one-handed body plus left torch arm composition. Historical IED uses the bounded policy above. No source mutation, new Actor, preselected-winner cache or generic compatibility expansion is added.

Exactly [two focused cases](../tests/torch_shield_policy_test.cpp) exercise the production weather policy and coherent shield equip/remove context. From an MSVC 14.44 x64 developer prompt at the repository root:

```powershell
New-Item -ItemType Directory -Force build/investigation-20261022 | Out-Null
cl /nologo /std:c++latest /EHsc /W4 /WX /I src tests/torch_shield_policy_test.cpp /Fobuild/investigation-20261022/torch_shield_policy_test.obj /Febuild/investigation-20261022/torch_shield_policy_test.exe
./build/investigation-20261022/torch_shield_policy_test.exe
cmake --preset Release -DVCPKG_MANIFEST_INSTALL=OFF
cmake --build --preset Release
```

Existing DAC/axe/IED/cache drivers are not rerun for this follow-up. Native weather/provider answers, pending-load timing, diagnostic values, transition appearance and world/panel equivalence remain **UNVERIFIED** pending the [dated game procedure](diagnostics/display-issues-2026-10-22.md#6-2026-10-22-torchshield-查询跟进). Keep docs/animation.md, this document, the dated handover, [README](../README.md), [Changelog](../CHANGELOG.md), policy/helper/controller and focused test synchronized.

## Low-overhead mapping and opt-in traces (2026-10-22)

`world_clip_report` now uses the official optional Animations V1 interface on actual active world `hkbClipGenerator` objects. `install_hooks` requests `RequestPluginAPI_Animations(V1, ...)` once after the existing verified profile/hook installation succeeds and retains only the module-owned interface pointer. The [unmodified public header and provenance](../src/animation/oar/api/README.md) are pinned to OAR `f4e7688b065175aff70aa523073857911e15aca3`; its public return layout/vtable slot were checked against both supported 3.1.6 and 3.2.1 DLL/PDB pairs. No additional private address, hook or hard dependency is added; other versions remain outside the current gate.

The official [GetCurrentReplacementAnimationInfo implementation](https://github.com/ersh1/OpenAnimationReplacer/blob/f4e7688b065175aff70aa523073857911e15aca3/src/ModAPI.cpp) reads the accepted ActiveClip replacement, with active-map locking and any member lookup confined to that replacement. It does not reevaluate conditions, advance Random/variants or clear state. CharacterPanel does not call state-clearing methods or register/activate synthetic panel clips. API absence, missing ActiveClip and empty replacement information are not proof of vanilla.

Each WORLD clip retains raw authored `original`, active `index`, direct actual `bound` and source `t`. OAR-supplied `oar-path/project/variant/submod/mod` are reported separately, without concatenating a supposedly unique path under hash deduplication. `oar-api=available` plus `oar-result=replacement` is positive information; `empty-unknown` or `unavailable-unknown` explicitly preserves uncertainty. Missing bound metadata says `<unavailable>`. The old reverse name lookup/project traversal/descriptor enumeration is removed. `animation_binding_name` uses `BaseAnimationCache::binding_name` for one bounded selected-entry read, validates that entry and its string, and does not weaken functional `ready_animation_table` or finite base prewarming.

Panel `ANIMATION PANEL ... accepted` comes from final handover metadata after load/fallback, or the existing same-asset acceptance branch. `State::publish_mapping` reads the actual accepted index directly; it never asks the current-world API about PreviewClip. The [small publication policy](../src/animation/mapping_report.h), `MappingPublication::accept`, suppresses identical mappings within a request while retaining changes/new requests, including two logical aliases sharing a bound index. Proposed/failed assets are not published as playing. `AnimationSelection::candidates` and `oar walk` are removed; only descriptors/variants required by actual selection/history remain.

These are schematic field examples, **not captured game results**:

```text
ANIMATION WORLD ... snapshot=paused-source reason='equipment' hands=(...) drawn=false build-ms=... clips=|original='Animations\TOR_Idle.hkx' index=... bound='...' oar-api=available oar-result=replacement oar-path='...' oar-project='...' oar-variant='...' oar-submod='...' oar-mod='...' t=...
ANIMATION WORLD ... index=... bound='...' oar-api=available oar-result=empty-unknown oar-path='' ...
ANIMATION PANEL ... accepted action=1 original='Animations\TOR_Idle.hkx' base-index=... logical='...' variant='index=... position=...' index=... bound='...' status=replacement clip-time=... blend=... build-ms=...
ANIMATION PANEL ... accepted ... logical='' variant='none' ... status=base ...
```

Snapshots are event-driven at panel-open/equipment, not a continuous world monitor. They retain `paused-source`, reason, current hand IDs and actual source drawn state. `build-ms` times source-side snapshot/publication construction before logging; it excludes disk writes, periodic flush and later render work, so it is neither total inventory latency nor FPS. The archived request-to-WORLD gaps were about 7.3 seconds per clip (roughly 22 seconds for three), localizing a major logging cost without a complete profiler claim. The full-name scans/candidate copies and per-info flushes are removed, but live performance improvement and visual equality remain **UNVERIFIED**.

The existing settings loader reads `[Diagnostics] DetailedAnimationTrace=false` once at DataLoaded; restart after changing [CharacterPanel.ini](../config/CharacterPanel.ini). Default mapping stays on. `true` enables the bounded axe mapping/pose probes, annotation info, Smooth marker construction, bounded candidate rejection detail/summary, proposal paths and capture geometry listings. Guards precede expensive formatting/traversal. Actual dispatch, native conditions, audits and warning/error paths are unchanged. The synchronous logger retains buffered info with approximately one-second sink flushing and immediate warning/error flushing; only sink flushing runs on that timer, with no engine work or new async logging architecture. Recently buffered info may not yet appear in a live file view.

The [two focused cases](../tests/animation_mapping_test.cpp) cover accepted alias/new-request publication with loop coalescing and exactly one selected read amid 50,000 names. From an MSVC 14.44 x64 developer prompt:

```powershell
New-Item -ItemType Directory -Force build/investigation-20261022 | Out-Null
cl /nologo /std:c++latest /EHsc /W4 /WX /I src tests/animation_mapping_test.cpp /Fobuild/investigation-20261022/animation_mapping_test.obj /Febuild/investigation-20261022/animation_mapping_test.exe
./build/investigation-20261022/animation_mapping_test.exe
cmake --preset Release -DVCPKG_MANIFEST_INSTALL=OFF
cmake --build --preset Release
```

No older driver or game/API call is executed by these cases. The [latest comparison procedure](diagnostics/display-issues-2026-10-22.md#7-2026-10-22-低开销映射日志跟进) distinguishes construction timing from disk/FPS and keeps API-empty, fallback, paused-world and visual limits explicit. Synchronize policy/controller/hooks, settings/config, public-header provenance, focused test, canonical docs, [SceneGraph](scene-graph.md), README and Changelog.

## Correlated rejection evidence (2026-10-22)

With `DetailedAnimationTrace=true`, `QueryRequest::diagnostic_request` enters only `QueryFrame` logging. It is excluded from channel, clip-key hashing, Random state and accepted history. Every candidate rejection/suppression row identifies actor/request/base/clip key; [controller input records](animation.md#correlating-reported-issues) provide attempted bases, source versus projected hands/drawn state, action/reason and effect revision.

For a genuinely rejected ordinary native candidate, `evaluate_set_thunk` records the first false condition in its **root top-level EvaluateAll set**, using the result of the existing single `condition->Evaluate(...)`. `outer == &query->scope` excludes nested sets. Name/provider/negated metadata is captured while the native-false detail allowance remains (twelve candidates, so one representative row per distinct cause still gets a name) and printed only if that candidate ultimately rejects. `root-false` explicitly says `scope=root-top-level result=false`; an OR/TARGET/PRESET entry is a top-level condition, not a reconstructed failing leaf. If no root false was observed, the row says native eligibility/other cause unknown. These observations never set `ScopedEvaluation::diagnostic`, convert ordinary false into unsupported, call `GetCurrent`, rerun Evaluate or compare against an extra native/source result. Details are deduplicated per distinct project folder and cause, capped at forty-eight rows per query, and the single suppression summary reports the remaining counts. A separate `OAR COND … movement-seen=yes|no movement-answered=yes|no` row states whether the `IsMovementDirection` hook was reached at all, which distinguishes a policy answer from a condition the hook never saw.

Optional WORLD detail appends only declared `hkbNode::name` and `hkbClipGenerator` mode/flags/playbackSpeed/atEnd fields to the existing actual-world mapping. Mode and flags are raw declared values; no opaque `hkbNodeInfo` weights/masks are read, and active nodes do not prove weighted visible contribution. Default API/direct-binding mapping remains on and unchanged, with no reverse scan, all-candidate copy, extra graph query or condition evaluation.

The [single engine-free request-budget case](../tests/diagnostic_trace_test.cpp) does not execute conditions, API callbacks or the game. Source review verifies one Evaluate call and the root-boundary/quotas separately. From an MSVC 14.44 x64 developer prompt at the repository root:

```powershell
New-Item -ItemType Directory -Force build/investigation-20261022 | Out-Null
cl /nologo /std:c++latest /EHsc /W4 /WX /I src tests/diagnostic_trace_test.cpp /Fobuild/investigation-20261022/diagnostic_trace_test.obj /Febuild/investigation-20261022/diagnostic_trace_test.exe
./build/investigation-20261022/diagnostic_trace_test.exe
cmake --preset Release -DVCPKG_MANIFEST_INSTALL=OFF
cmake --build --preset Release
```

Prior drivers are not rerun for this diagnostic-only follow-up. Native rejection values, query timing and visual causes remain **UNVERIFIED** until the [current game procedure](diagnostics/display-issues-2026-10-22.md#10-2026-10-23-装备上下文与火把分层修复待上机) is executed (the r2 session showed the transitions unchanged, so the 2026-10-23 argument/movement rows are the next evidence). Synchronize controller/hooks, [diagnostic budget](../src/animation/diagnostic_trace.h), focused test, canonical animation/DAC/scene docs, config comments and Changelog.

## Actor-animation ABI audit (2026-10-19)

The binary profiles retain the audited `AnimationReplacements::ForEachReplacementAnimation` entry as identity/provenance evidence; current mapping/query logging no longer calls it or copies an all-candidate catalogue. Actual selection uses the existing EvaluateConditions path: `evaluate_replacement_thunk` copies metadata only for a native-eligible candidate before returning to OAR's selection loop, while OAR retains its selection read lock. The historical enumeration entry is RVA `0x1EB900` in 3.1.6 (prefix `48 89 5c 24 18 55 56 57 48 83 ec 40 48 8b 05 2d`) and `0x12D7B0` in 3.2.1 (same prefix ending `bd`); these are not an active logging-enumeration route. Profiles remain in [binary_profile.h](../src/animation/oar/binary_profile.h).

Both PDB field lists confirm `ReplacementAnimation::_originalIndex=0xA8`, `_bDisabled=0xAA`, `_path=0xB0`, `_projectName=0xD0`, `_parentSubMod=0x108`, size 272. `Variant::_index=0`, `_filename=8`, `_bDisabled=0x28`, `_bPlayOnce=0x29`, size 56. Evidence: `llvm-pdbutil dump --types --type-index=0x4473 --type-index=0x42D3` for 3.1.6 and types `0x7A73` / `0x7899` for 3.2.1 against the provided DLL/PDB pairs. Earlier +0xA8 path hypotheses confuse the original index with the string; use +0xB0.

The authoritative source pin `f4e7688b065175aff70aa523073857911e15aca3` registers replacement bindings at project construction and may hash-deduplicate their names. `AnimationDescriptor::path` therefore remains separate from `binding_name` and the actual loading `index`. Native variant metadata is copied under its variant read lock. Conditions and caller-owned Random evaluation stay in OAR; accepted variant history, annotation clocks and local crossfade belong to animation; native processing and session restoration belong to [DAC](dac.md). OAR's internal ActiveClip blending needs an active behavior graph; the pinned public API offers no detached offline blend entry point.


## Native Actor effects and copied metadata (2026-10-20)

`HasMagicEffect` and `HasMagicEffectWithKeyword` now follow OAR's ordinary source Actor evaluation, including its own Active effects only/disabled/negated semantics. The independent effect projection was removed. [dac::Session](dac.md) dispatches accepted preview events through the exact native DAC-only handler and restores captured marker effects on inventory close; all DAC transaction logic stays outside OAR. Existing source/clip evaluation scoping and standing/equipment/identity policy remain.

Animation now requests equipment-driven non-combat idles with panel-drawn=true regardless of world draw/action. Native Smooth_loop goal readiness precedes queried choices. Direct empty-hand MT_Idle bypasses OAR replacements and marker dispatch; its calibration remains an independent vanilla base. [Policy/configuration](animation.md#configuration) and [DAC preparation](dac.md#non-combat-idle-goal-preparation) own these decisions; this query adapter remains unchanged.

Smooth Moveset's [compatibility boundary](dac.md#smooth-moveset-compatibility-boundary) documents remaining query limits. The [three explicit IED/SDS schemas and matched-provider forwarding](#native-ied-placement-conditions-2026-10-21) retain their exact provider and bounded historical-hand guards; unrelated providers still require an explicit adapter. The combat Start/Loop configs use TDM effect `0x804` OR `IsCombatState == 1`; `evaluate_thunk` currently projects the latter to zero, while ordinary source effects remain native reads.

The existing version/PE/CodeView/evaluate/numeric gates remain unchanged. `matches_query` additionally gates these copied-metadata getters:

| Function | 3.1.6 RVA | 3.2.1 RVA | 16-byte prefix |
| --- | --- | --- | --- |
| `GetInterruptible(void const*)` | `0x1E2980` | `0x124830` | `48 8b 81 08 01 00 00 0f b6 80 62 01 00 00 c3 cc` |
| `GetCustomBlendTime(void const*, ActiveClip*, int32, bool, bool)` | `0x1E24F0` | `0x1243A0` | `48 89 5c 24 08 48 89 74 24 10 57 48 83 ec 20 41` |

`describe` invokes the blend getter with null ActiveClip and `checkWithinSequence=false`, copying interrupt (type 0) and loop (type 1) durations under the existing replacement query locks. The audited null path bypasses ActiveClip state reads in both binaries. `VariantOptions::blend_between` copies the confirmed variant flag at `+0x20` under the variant read lock. No new guessed private offsets or retained native pointers are introduced. Rejected/base selections clear replacement policy.

The animation controller uses these values for its own advancing-pose fade, including zero duration. It does not invoke OAR's native blender or reproduce ActiveClip's loop lookahead/end-relative incoming seek. Upstream semantic references are pinned at `f4e7688b065175aff70aa523073857911e15aca3` (3.2.1) and `0c1c8b38a5323b9e26df41ff9d10ce801b984a45` (3.1.6); they are reference evidence, not linked/copied implementation. No Skyrim runtime validation is claimed.


> 历史查询/换装方案见[OAR 实验总结](history-animation.md#oar)，详细原件由[来源索引](history-sources.md)从 Git 恢复；当前查询和播放规范见[动画](animation.md)。
> 下文投影及旧包为历轮记录；全局站立窗口已移除，两版主动查询入口已重新核对并接入。

更新日期：2026-10-15（第二轮：3.1.6 profile 与投影作用域接入）。

## 范围与入口

实现位于 [src/animation/oar](../src/animation/oar/hooks.h)，对 Nexus 主文件 **3.1.6**（测试环境实际安装的版本）与
**3.2.1** 各持一份经核对的二进制 profile（[binary_profile.h](../src/animation/oar/binary_profile.h)）。
只有装载版本的 SKSE 版本号、PE 时间戳／映像大小、CodeView 身份与两个入口字节**全部**对得上，
才以该 profile 的地址安装。`main.cpp` 在 `kPostPostLoad` 调用 `oar::install_hooks()`；
OAR 未加载、无 profile 匹配或安装失败时 `hooks_ready()` 为 false，日志给出拒绝原因与被拒版本。

本模块有两条用法，由 `ScopedEvaluation::ScopeMode` 区分：

- **`kAudited`（默认）**：调用方自己持有一棵候选条件树，用 `evaluate_condition(...)` 求值。
  审计集合之外的条件使本次查询**不成立**；调用方不得据此接受候选。
- **`kProjected`**：调用方包裹**引擎自己**的求值过程（面板手动推傀儡行为图的调用栈内）。
  本模块只覆盖它建模的那些条件，其余条件**保留引擎的答案**，不否决 OAR 本会接受的替换。
  作用域允许 `clip = nullptr`：OAR 在自己的激活钩子里才决定 clip，包裹式作用域无法提前点名它。

**已接入**：`PuppetDriver` 在暂停面板里手动推进傀儡行为图时，用
`ScopedEvaluation(puppet, nullptr, preview, kProjected)` 包住 `UpdateAnimationGraphManager`。
OAR 的替换决策发生在它自己的 clip 激活钩子内、于该调用栈中同步求值（已按源码核对：
`HavokHooks::hkbClipGenerator_Activate` → `ActiveClip::OnActivate` →
`EvaluateConditionsAndGetReplacementAnimation` → `condition->Evaluate(refr, clip, submod)`，
其中 `refr = animGraph->holder` 即傀儡本体），因此整轮决策落在投影内：身份与装备仍是傀儡的
（它穿的就是玩家那一套），姿态谓词按站立角色回答。手动推图只在暂停帧运行，所以投影恰好只覆盖
"面板打开"这条路径。证据行：

```
SCOPY OAR project ticks=… hooks=installed|absent mode=projected|plain evaluated=… overridden=… differing=… top=[名称 差异/覆盖, …]
```

**零 `evaluated` 是"钩子没被走到"的证据，不是"引擎没有求值"**——两者必须区分开读。

### 差值与开／关对照（oar-ab 轮加入）

投影每次给出自己的答案时，**同一次求值**再问一次引擎的原答案并比对，差值按条件名计数——
这是"投影改变了一个 OAR 据以决策的输入"的**直接**测量，不是两个时间点的对比：

- `overridden` = 投影回答的条件求值条数；`differing` = 其中引擎原答案与之不同的条数；
  `top=` 是按差异数排序的前四个条件名（`名称 差异/覆盖`）。
- **`differing≈0` 说明在当时的角色状态下引擎的答案本来就是站立值**——投影拦到了、也改写了，
  但没有改变任何决策输入。这正是"画面无变化"的可读证据（2026-10-15 实测：面板人物常驻
  站立 idle，观感无变化，`differing` 就是判定这一点的量）。
- **F11 = 投影开／关**（`SCOPY OAR projection on|off`）。两条臂各自记录自己产生过的 clip 索引，
  都拿到数据后打一行
  `SCOPY OAR ab projected-clips=[…] plain-clips=[…] sets-equal=yes|no`——用于回答"换一个状态
  （拔刀、跳跃、潜行）时选片是否真的不同"，不需要操作者掐时间。

**尚未接入**：本模块不提供 OAR 替换选择的公开桥接、动画装载、变体会话隔离、过渡播放或
显示图换装。选片与播放仍由 OAR 自己完成，本模块只改它求值时所见的**状态**。

调用方使用 `evaluate_condition(condition, source, preview_clip, state, parent_submod)`。
`source` 保留原角色身份与装备，`preview_clip` 必须属于预览实例，`parent_submod`
应沿用该条件所在的 OAR 子模组上下文。返回值只有 `supported && matched` 才能用于接受候选。
`diagnostic` 记录首个拒绝原因，`overrides` 记录覆盖次数。
调用者还须持有条件树并防止求值期间被编辑器重建，例如使用上游读锁或独立的稳定副本；本模块
不跨帧缓存条件、Actor 或 clip 指针。OAR 编辑器的 `GetCurrent` 展示仍可能是来源真实值，不能把
该展示值当作 hook 后的求值结果。

`ScopedEvaluation` 是内部的同步作用域，不能跨线程、异步任务或 Actor／clip 生命周期使用。
作用域按线程保存，可嵌套恢复；覆盖同时匹配来源与 clip。它不由“背包打开”这一全局条件启用。

## 已实现的状态覆盖

- 潜行、腾空、奔跑、冲刺、游泳、骑乘、攻击、格挡、对话和交互等内置谓词使用站立展示状态。
  实际名称清单见 [policy.h](../src/animation/oar/policy.h)。`IsWeaponDrawn` 由预览状态提供。
- 当前速度、浸没量、坐睡状态与攻击状态的比较使用零值；战斗状态和静止方向按状态 0 求相等。
  仍由 OAR 自己的 comparison component 处理比较运算，不把比较条件一律当作 false。
- `PreviewState::graph_variables` 表示该预览实例完整的变量表；`HasGraphVariable` 判断变量是否存在，
  值为零也代表存在。数字条件中的图变量值从同一表读取；数字读取缺失变量、非有限输入或无法转换的
  数字类型导致本次求值不受支持，不偷读原角色的同名变量。
  **这两条是 `kAudited` 的语义**：投影模式下的表是"覆盖表"而不是"全表"，因此缺失的变量名一律保留
  引擎答案（`kProjected` 不因一个未被建模的图变量而否决 OAR 的候选）。
- 身份、种族、性别、阵营、已确认装备及允许的环境条件使用原始 OAR 求值。
- 内置条件的 disabled／negated 语义保留；AND／OR／XOR 由 OAR 原实现组合。

以下保留当时投影原型的审计记录：未列入当时支持清单的第三方条件、自定义／preset component、Random、IdleTime 等会明确拒绝，
不以原角色真实行为代替预览状态；最大审计深度为 64。当前 PRESET/Random 与匹配 IED 提供方的处理以文首各节为准，
并非这里历史清单的全量复用。当前支持仍有明确边界，不代表任意 OAR 动作包通用兼容。

## 下载包与二进制证据

来源：[Nexus 文件页](https://www.nexusmods.com/skyrimspecialedition/mods/92109?tab=files)。

### 3.1.6（测试环境实际安装，本轮新增）

DLL 与 PDB 取自测试环境安装目录 `E:\SkyrimAE\mods\Open Animation Replacer\SKSE\Plugins\`
（用户指定，未从 Nexus 重新下载）；两者 PE 时间戳一致。没有将 DLL／PDB 纳入源码。

| 项目 | 值 |
| --- | --- |
| DLL 文件版本 | 3.1.6.0 |
| DLL SHA-256 | `d94e55f257d404e4f30bc6d60983e94c1379f92d916598b2b069930c5ad31434` |
| PDB SHA-256 | `bce55bee19d57839ba2e2fd5219266018ee8fb7057e95887701f39c59ed596e1` |
| DLL CodeView／PDB GUID | `735b4f73-d7d0-4e6c-aa8d-3e9f8384c6b5`，age 1 |
| PE timestamp | `0x6a40292a` |
| SizeOfImage | `0x3b3000` |
| 源码提交 | `0c1c8b38a5323b9e26df41ff9d10ce801b984a45`（`Version 3.1.6`） |

| 入口 | RVA | 入口前 16 字节 | 核对内容 |
| --- | --- | --- | --- |
| `Conditions::ConditionBase::Evaluate` | `0x1217b0` | `48 83 ec 28 80 79 10 00 74 07 b0 01 48 83 c4 28` | 读取 disabled `+0x10`、negated `+0x11`，经虚表 `+0xd0` 调用具体判断 |
| `Components::NumericValue::GetValue` | `0x1ff8b0` | `48 8b c4 48 89 58 18 55 56 57 48 8d 68 a1 48 81` | `_type +0x80`；图变量 type `+0xf0`、MSVC string `+0xf8`（size 在 `+0x110`） |

两个入口的 RVA 由 `llvm-pdbutil dump -publics` 的 `addr = 0001:<段内偏移>`（十进制，`.text`
虚拟地址 `0x1000`）换算，再用 `llvm-objdump -d --start-address=0x180000000+RVA` 反汇编核对：
`Evaluate` 入口字节与 3.2.1 **逐字节相同**，`GetValue` 不同但 `_type`／图变量 type／string 三个
字段偏移**与 3.2.1 完全一致**。GUID 原始字节经 `llvm-readobj --coff-debug-directory` 给出的
CodeView 记录偏移直接读文件核对（`RSDS` + 16 字节 + age）。

**降级安全性依据**：`0c1c8b3`（3.1.6）→ `f4e7688`（3.2.1）之间，公开 API 头
`src/API/OpenAnimationReplacer-ConditionTypes.h` **零差异**；`src/Conditions.h` 只新增了一个条件类
（`HasBoundWeaponEquippedCondition`）与一处构造参数，均不改变本模块调用的
`ICondition`／`ConditionBase`／各 component 的虚表布局。

### 3.2.1（上一轮，保留为第二份已验证 profile）

用户下载的主文件包名为 `Open Animation Replacer 92109 3.2.1 2026-08-31T16-32Z MWx0rveD6.7z`。
分析副本保存在忽略的 `build/oar-3.2.1`。

| 项目 | 值 |
| --- | --- |
| DLL 文件版本 | 3.2.1.0 |
| DLL SHA-256 | `db61b8b691e709bede355514871918baa63eabe8f6d0734bf17ffcf793b4c894` |
| PDB SHA-256 | `7b90b51a71bd074d3628afdc231570a58e327219908675bc25dcc8a0509edf28` |
| DLL CodeView／PDB GUID | `15eb65f2-91de-4a98-99ce-9ca2192e6af4`，age 1 |
| PE timestamp | `0x6a95a9e5` |
| SizeOfImage | `0x2f6000` |
| 源码提交 | `f4e7688b065175aff70aa523073857911e15aca3`，CMake version 3.2.1 |

| 入口 | RVA | 核对内容 |
| --- | --- | --- |
| `Conditions::ConditionBase::Evaluate` | `0x68640` | 读取 disabled `+0x10`、negated `+0x11`，经虚表 `+0xd0` 调用具体判断 |
| `Components::NumericValue::GetValue` | `0x1417f0` | `_type +0x80`；图变量 type `+0xf0`、MSVC string `+0xf8` |

`llvm-pdbutil dump -publics -section-headers` 的段偏移结合 DLL PE section 得到以上 RVA；
`llvm-objdump` 反汇编核对签名与实参使用，未按源码行号或其他版本猜地址。
另从 PDB 定位并读取了 126 个原生 Condition 虚表，其 Evaluate 槽均指向 `0x68640`。
第三方 DLL 自带的 `CustomCondition::Evaluate` 不属于这项历史基础 hook 证明；当前匹配 IED 提供方的三项显式 schema 检查及其它名称原生转发见文首，不能从这里推断所有未知名称一律拒绝。

入口前 16 字节在 [binary_profile.h](../src/animation/oar/binary_profile.h) 保存，两份 profile 各存一份。
运行时同时核对 SKSE 插件版本、PE timestamp／image size、CodeView GUID／age 和两个入口字节，
全部成立后才以一个 Detours transaction 按该 profile 的地址安装；安装成功打印
`OAR <版本> hooks installed: …`，被拒时打印拒绝原因与被拒版本的四个字段。
数字字段读取依赖本包的 MSVC 14.44 ABI，不能直接用于其他构建。
PDB 用于开发核对；运行时只读取 DLL 自带的 CodeView 身份信息，无需分发或加载 PDB。

## 测试包身份

### oar-316（3.1.6 降级 + 投影首次上机，已实测）

| 项目 | 值 |
| --- | --- |
| 构建身份 | `CharacterPanel-scopy-oar-316-f63f41ca49+dirty-cl94faaed0c6-20261015T142245Z` |
| DLL SHA-256 | `9cdb8db54060a8d8ab8eff5a7d0c5c1d890bcf456edb61d50abf2216141e46ce` |
| PDB SHA-256 | `47b37757ef343282f103f124fcd9f3ffdf625de87d65730c08ce464e5f203767` |
| ZIP SHA-256 | `7691845cab1511272b71054fcdcdff7a14f372a578a03f0a112e8038e2fd6185` |

**游戏内结果（2026-10-15 晚，日志（历史文件未随仓库提供：`../log-archive/CharacterPanel-20261015-2230-oar316.log`））**：

- `OAR 3.1.6 hooks installed: …` —— 3.1.6 profile 被接受，不再 `refused`。
- 2 640 步面板时间内累计 `evaluated=186272`、`overridden=26877`（约 70 次求值/步、10 次改写/步），
  **证明钩子被走到且投影在每一次决策里生效**；零 `[E]`、零崩溃，`AFP2 verify match=1`、
  `R01 identity verified=1`、`FIGURE transparency render-flags=0x204` 与 `COMPOSITE match=1` 全程正常。
- 投影已影响族选择：换上手部武器时 digest 同步出现 `StaffRight_Idle.hkx#1536`、
  `DW_ShieldAdjustment.hkx#654`（会话内 40 次换装反应全部跟上）。
- **观感判读（用户）**：动作没有变化，面板人物一直是站立 idle——因为面板画的是傀儡 actor 本体，
  它本来就常驻默认站立状态，所以引擎的答案与投影一致。**这不是失败，而是"投影在此状态下不改变
  决策输入"**；`differing` 计数（oar-ab 轮加入）就是把这个判断变成可读数字的量。

### oar-ab（差值 + F11 开／关对照，已实测）

| 项目 | 值 |
| --- | --- |
| 构建身份 | `CharacterPanel-scopy-oar-ab-f63f41ca49+dirty-cl94faaed0c6-20261015T143724Z` |
| DLL SHA-256 | `9d81e29d204bc932a3dff6dbc4a6a6adac84a22b55a975e5ec00cc20d2370582` |
| ZIP SHA-256 | `e99483aa07d1fc400feb4b42fa61805f34d1ea0f0f199ab388c373347ef73b66` |

**游戏内结果（2026-10-15 晚，日志（历史文件未随仓库提供：`../log-archive/CharacterPanel-20261015-2245-oar-ab-2hcrash.log`）；
该会话末尾死于双手武器换装崩溃，与本模块无关，见不变量 32）**：

- **投影改变了 OAR 的选片**：F11 切到 plain 臂后
  `SCOPY OAR ab projected-clips=[9044,15069,9043,9800,9859,8917,10331,10342] plain-clips=[9043,15069] sets-equal=no`
  ——投影臂产出 8 个不同 clip、plain 臂 2 个。**这是"条件求值被应用且影响了决策"的直接证据**；
  观感上仍是站立 idle，因为两边选中的都是 idle 族内的变体。
- **差值点名到唯一一处**：全程 `differing=306 top=[IsWeaponDrawn 306/81096, CurrentMovementSpeed 0/1409, IsAttacking 0/19246, IsCombatState 0/8167]`
  ——常用谓词（`IsAttacking`／`IsCombatState`／`IsInAir`／`CurrentMovementSpeed`）逐条为 0，
  即**傀儡的引擎状态本来就等于站立预览**；唯一出现分歧的是 `IsWeaponDrawn`，而它正指向随后
  暴露的双手武器换装缺陷（傀儡的持械状态与玩家手部读数在那段时间不一致）。
- 结论：投影对"自身状态已等于预览状态"的角色不产生选片差异（这是 `differing` 的用途），
  但它确实在状态有分歧时改写决策——`sets-equal=no` 已经证明了这一步。

### 2h-fix（双手武器换装崩溃修复，当前部署）

| 项目 | 值 |
| --- | --- |
| 构建身份 | `CharacterPanel-scopy-2h-fix-f63f41ca49+dirty-cl94faaed0c6-20261015T145312Z` |
| DLL SHA-256 | `565ae299ec0252daa671608b6e712a200e908093043b06495f13733282a088b1` |
| PDB SHA-256 | `0d8d8c18b13cfc3ea444dcddb7920b92c0d7f43badb73521b9b4419c095be2c5` |
| ZIP SHA-256 | `30dd5d3fceadf8f86bed87803d2aa8c4efb68b56a89c17ad73a3e48f78c8a403` |
| 包名 | `dist/CharacterPanel-scopy-2h-fix-1.2.1.zip` |

zip 内 DLL/PDB 与构建产物、manifest 三方哈希一致，且与已部署到
`E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\` 的 DLL 相同（当前部署 = 2h-fix）。

崩溃、根因与规则见 [维护说明不变量 32／33](code-maintenance.md) 与
[历史崩溃总结](history-animation.md#oar)；原始 crash 日志由[来源索引](history-sources.md)从 Git 恢复。

两份包的 zip 内 DLL/PDB 与构建产物、manifest 三方哈希一致；DLL 与 PDB 均已部署到
`E:\SkyrimAE\mods\CharacterPanel\SKSE\Plugins\` 并校验一致（当前部署 = oar-ab）。
源码基线 `f63f41c`，`+dirty` = 本轮 OAR 改动尚未提交。

## 验证与后续验收

本地回归包括预览作用域的来源／clip／线程隔离、嵌套恢复、站立谓词与取反、两种作用域模式的
匹配面与计数，以及两份 profile 的自校验与互相拒绝。默认与实验两种 Release 配置均构建通过；
`snapshot_transform` 现有 15 项检查全部通过（OAR 相关的三项：预览作用域、投影作用域、二进制身份）。
这 15 项是当前维护轮的记录，见 [当前维护说明](code-maintenance.md)。
入口、CodeView 与字段访问已对两份 DLL 作静态核对；这些证据不等于已经在游戏中成功安装 detour。

游戏内必须核对（oar-ab 轮判据）：

1. 日志出现 `OAR 3.1.6 hooks installed: …` —— 3.1.6 profile 被接受；若仍打印 `refused`，
   那行会同时给出被拒版本的四个字段。
2. 暂停面板里出现 `SCOPY OAR project ticks=… hooks=installed mode=… evaluated=N overridden=M differing=D top=[…]`：
   `evaluated` 是落在投影内的 OAR 条件求值条数，`overridden` 是被站立预览改写的条数，
   **`differing` 是引擎原答案与投影答案不同的条数**。**`evaluated=0` 说明钩子没被走到**
   （作用域没覆盖到求值路径或 OAR 没在这条路径上决策），不是"引擎没求值"。
3. `differing` 与 `top=` 是"投影是否改变了决策输入"的答案：站立 idle 状态下 Expected 接近 0；
   若拔刀／跳跃／潜行等状态下出现非零，`top=` 会点名是哪些条件（例如 `IsInAir`、`CurrentMovementSpeed`）。
4. 按 **F11** 切到另一条臂，让两条臂都产出过 clip，读
   `SCOPY OAR ab projected-clips=[…] plain-clips=[…] sets-equal=…`：`sets-equal=no` 即"投影改变了 OAR
   在这个状态下的选片"的直接证据（`=yes` 表示两条臂选出同一批 clip，即该状态下投影不改决策）。
5. 投影只改状态、不改身份：装备／种族／阵营类条件仍按傀儡（= 玩家那一套）回答；
   退出面板后来源角色与 NPC 的求值不受影响（作用域随推图调用结束即离开）。

暂不通过额外推进来源行为图来验证。本模块仍**没有**提供 OAR 替换选择的公开桥接、动画装载、
变体会话隔离、过渡播放或显示图换装——选片与播放仍由 OAR 自己完成，不能把条件 hook 与投影接入
算作 idle／equip 全链路通过。

上游更新时重新取得匹配 DLL／PDB，固定对应源码，核对 ABI／符号／反汇编并更新完整 profile；
不能只修改版本号跳过二进制检查。
