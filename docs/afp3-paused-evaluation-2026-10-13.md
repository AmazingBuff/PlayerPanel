# AFP3: why the paused output is empty, and the one field that decides it

Date: 2026-10-13 · build: `CharacterPanel-scopy-AFP3-44f6ab6103+dirty-cl94faaed0c6-20261012T154502Z`
Baseline: [afp1b-verdict §13](afp1b-verdict-2026-10-13.md), HKX42, commit `44f6ab6`.

## 1. The question

Two rounds measured the same thing from opposite ends and neither said *why*:

| condition | puppet output pose | `numAnimBones` |
|---|---|---|
| world running, held at the player | 98 elements | 98 |
| world running, parked 8000 below | 0 elements | 0 |
| world paused, held at the player | 0 elements | 0 |
| world paused, player (reference) | 116 elements | 116 |

`poseLocal` was convicted as a second source in the last round: it does not move with our clock
either, and it is skeleton-ordered. So the question became "is the engine evaluating this graph at
all", and the answer needed the engine's own update chain rather than more measurements of its
effects.

## 2. The chain, from the sources that hook it

Everything below is read out of code in this workspace, not inferred:

```
Actor::UpdateAnimation(float)                       vtable 0x7D  (TESObjectREFR.h:305)
  Actor::ModifyAnimationUpdateData(data)            vtable 0x79  (Actor.h:340)
    IAnimationGraphManagerHolder::UpdateAnimationGraphManager(data)   id 32155 / 32899
      BSAnimationGraphManager frame function                          id 62416 / 63358
        BShkbAnimationGraph::UpdateAnimation(graph, updateData, a3)
          hkbBehaviorGraph::Update(context, dt)     vtable 0x5
          hkbBehaviorGraph::Generate(...)           vtable 0x17   -- may be skipped
```

- `BShkbAnimationGraph::UpdateAnimation(BShkbAnimationGraph*, BShkbAnimationGraph_UpdateData*, void*)`
  is declared and hooked by Precision (`Precision/src/Hooks.h:271`, body `Precision/src/Hooks.cpp:577`),
  called from the manager frame function at `+0xF5` (SE) / `+0x185` (AE) (`Precision/src/Hooks.h:225,242`).
- `a3` is *"null if the graph is not active"* (`Precision/src/Hooks.cpp:580`).
- `hkbBehaviorGraph::Update` = 0x5, `Generate` = 0x17 — OAR hooks both in production
  (`OpenAnimationReplacer/src/Hooks.h:73-74`); `Generate`'s real signature has five arguments
  (`OpenAnimationReplacer/src/Hooks.h:109`), not the one CLib declares.

**The decisive field.** The update block Precision defines carries, at `+0x2A`:

> `unk2A = true;  // forces animation update (hkbGenerator::generate()) without skipping frames`
> — `Precision/src/Hooks.cpp:583`, over `BShkbAnimationGraph_UpdateData`
> (`Precision/src/Havok/Havok.h:233-253`, same offsets as `BSAnimationUpdateData`).

So the engine is explicitly allowed to **generate a pose on some frames and skip it on others**, and
the flag that forbids the skip is a documented field. "Clip time advances, no pose is produced" —
measured twice — is the signature of exactly that skip. Our manual tick passed a block with
`deltaTime` and `refr` set and **everything else zero**, including the camera position (+0x18, the
translation inside the WorldRoot camera's world transform), the Actor's own
`IPostAnimationChannelUpdateFunctor` (+0x20, `Actor+0xE0` on AE) and `unk2A`.

The skip is also the only reading that fits the *two* output buffers together with
`interpolationAmounts[2]` and the `[Animation]` INI pair `fAnimInterpNearDist` 400 /
`fAnimInterpFarDist` 800: generate when due, blend between the two buffers otherwise.

## 3. What this build changes

1. **F3 gains `tick+apply+force`** — the same push and the same write, with `unk2A` set (and the
   camera position and the Actor's own functor supplied in both apply states). The two adjacent F3
   states are therefore an A/B of one field, inside one session, one placement, one pause.
2. **Every mark prints the graph's own state** for the puppet *and* the player — `isActive`,
   `isLinked`, `updateActiveNodes`, `stateOrTransitionChanged`, `numStaticNodes`,
   `numIntermediateOutputs`, `activeNodes` + node-clone count, `rootGenerator`, `userData`,
   `interpolationAmounts`, plus the raw bytes of the fields no source names yet
   (`BShkbAnimationGraph+0x238..0x250`, `hkbBehaviorGraph+0x118..0x130`, `hkbCharacter+0x18..0x30`).
   `isActive == 0` means *deactivated* — a different answer from *nobody called generate*, and one
   that needs a different fix.
3. **The engine image is dumped once per process.** Both installed `SkyrimSE.exe` copies are
   Steam-wrapped: `.text` measures **8.000 bits/byte** with no `CC` padding and no MSVC prologues,
   while `.rdata` strings are intact and the entry point sits in a `.bind` stub section. Static
   analysis of the engine is therefore impossible from disk, and the address library still works
   only because it resolves RVAs. `EngineImage::dump_once()` writes the live section next to the
   plugin log with a sidecar of resolved anchors and vtables; `tools/re/engine_scan.py` reads the
   pair (`.pdata` function bounds, disp32 scan, hexdump).

## 4. Corrections to earlier notes

- **The crash-log call chain is not a chain.** `docs/diagnostics/crash-2026-10-12-01-27-26.log`
  frames `[14]` and up are marked `[S]` — a stack scan, sitting between `ucrtbase` and `amdxx64.dll`.
  `BShkbAnimationGraph::sub_BAEBB8 -> 63568+0x358` and `BSAnimationGraphManager::sub_BA3FF1 ->
  63358+0x3A1` are therefore hints, not a proven call path. (`63568`/`63358` do fall in the id
  ranges Precision independently assigns to those two classes, and `63358` is the exact id of the
  manager frame function, so they are worth checking against the dump — not quoting as fact.)
- **`currentLOD == -1` / `numTracksInLOD == -1` is the expected state**, not a bad offset: OAR guards
  `numTracksInLOD > -1` and treats `-1` as "no LOD cap" (`OpenAnimationReplacer/src/ActiveAnimationPreview.cpp:29-31`).
  The Havok character-LOD feature is evidently undriven in shipping Skyrim; the distance throttle is
  the interpolation/visibility system.
- **`poseLocal` is skeleton-ordered** (last round) and is now explained structurally: it is
  `hkbCharacter::poseLocal`, an array indexed by animation bone — there is only one order.

## 5. What one session decides

With the pause held and the placement fixed, marks taken with `force=off` then `force=on`:

- **pose appears only with force=on** → the pose was being skipped; the route needs this flag and
  the engine's evaluation was never the problem.
- **pose stays empty either way** → read the `state` line: `is-active=0` says the graph was
  deactivated and the fix is to re-activate it; `is-active=1` with `interp` mid-blend says generate
  is still being declined, and the next stop is the dumped code for `UpdateAnimation`.
