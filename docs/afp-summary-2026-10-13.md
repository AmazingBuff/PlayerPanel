# AFP summary: the same-Actor final-pose route, AFP1 through AFP8

Date: 2026-10-13 · route owner: PRD 0.9 §6.6
Spans: [AFP1/1b](afp1-verdict-2026-10-13.md) → [AFP3](afp3-verdict-2026-10-13.md) →
[AFP4](afp4-verdict-2026-10-13.md) → [AFP5](afp5-verdict-2026-10-13.md) → [AFP8](afp8-verdict-2026-10-13.md)
Tooling built along the way: `tools/re/engine_scan.py`, `src/diagnostics/engine_image.*`

## 1. The route in one sentence

While the panel pauses the world, the puppet Actor's own behaviour graph produces its own final
output pose, and that pose is written into the puppet Actor's own skeleton — no detached copy, no
clip resampled onto a second skeleton, no empirical track solver.

## 2. The four engine facts the whole route stands on

Each was measured, then re-measured in a later round:

1. **The paused graph is alive but its generate is skipped.** `is-active=1`, 109 node clones, a live
   root generator — and an empty output pose, until `BSAnimationUpdateData::unk2A` ("forces animation
   update (hkbGenerator::generate()) without skipping frames", Precision/src/Hooks.cpp:583) is set.
   With it: 0 → 116 bones, reversible inside one session. The engine's per-frame chain is
   `Actor::UpdateAnimation` (vtable 0x7D) → `UpdateAnimationGraphManager` (id 32899) →
   `BSAnimationGraphManager` frame function (id 63358) → `BShkbAnimationGraph::UpdateAnimation`
   → Havok `Update` (vtable 0x5) and, unless skipped, `Generate` (vtable 0x17).
2. **The pose is skeleton-ordered, and there is only one order.** `hkbCharacter::poseLocal` is an
   array indexed by animation bone (109/109 against the tree), which corrects HKX2's claim that the
   graph output was boneNodes-ordered. The engine's own `boneNodes` table is a *different* set that
   overlaps the tree only partially — it addresses 36–37 of the 68–116 bones the pose carries, which
   is why writing through the table alone could never cover a figure.
3. **The park never mattered and the pause never disabled anything.** A parked-deep puppet with the
   world running generates a full pose (98 elements, and in AFP8 a parked puppet stood at a normal
   location with 116). The single variable was the skip.
4. **Proving the order and writing the pose need opposite worlds.** The proof compares the pose
   against the locals *the engine applied*, so it is only meaningful while the world runs and the
   panel is not forcing a generate (0.98–1.00). The write happens while the world is paused and the
   panel IS forcing one. AFP4 measured the proof collapsing 0.99 → 0.67 the moment a forced pose was
   scored against an unforced apply.

## 3. The results, by round

| round | result |
|---|---|
| AFP1/1b | Both pose sources convicted while paused: the output pose empty (`numAnimBones=0`), `poseLocal` static (0.069° two-state jitter, tick-independent, skeleton-ordered). `world-within-1deg=0`. |
| AFP3 | `unk2A` flips the skip: paused, `parked-deep`, 0 → 116 bones with a 109/109 match, reversible. `is-active=1` throughout falsifies "the graph gets deactivated". Engine image dumped (Steam-wrapped `.text` is encrypted on disk; only a live process holds it as code). |
| AFP4 | The order proof moves from 37 bones to 109 by reading the candidate local from the matched tree node: 108/109 = 0.99. Also measures a forced generate in a running world breaking that proof (0.99 → 0.67). |
| AFP5 | First write: `written=36 max-rot=13.937deg` while paused. Both output buffers hold the forced pose (68/68 each) and `interp=(0.194,0.068)` — the two-buffer blend the `[Animation]` INI block describes, measured directly. `unreachable=32` quantifies the bone-table limit. |
| AFP8 | The closed receipt: `verify 90/92=0.98 match=1` in the running world, then paused `written=92 unreachable=6`, and the mark's own calibration scores the *written* nodes against the pose at **109/109 with max-rot=0.048°** — conversion rounding. World space 0/92 (AFP1) → 104/109, the five outliers being weapon attachment nodes the graph does not drive. |

## 4. The four fixes between AFP5 and AFP8, and what each one was

1. **The proof must survive the drive cycle.** `cycle()` reset the verified latch, so cycling from
   `tick+apply` into the force state threw away a proof taken seconds earlier at 0.98, and the paused
   frame reported `unverified-world-first` with the pose sitting right there. Only an identity change
   (new 3D root or new graph) invalidates the mapping.
2. **The write must ask the tree, not the table.** The bone-table index gate skipped 32 bones before
   their name was ever looked up; resolving by name reaches 92 of 92 (AFP8: `unreachable=6`).
3. **The verification must refuse a forced world.** Scoring a forced pose against an unforced apply
   measures their drift, not the index order; it now blocks with
   `verification-waits-for-unforced-world` instead of producing a number that looks like a verdict.
4. **F3 belonged to another mod.** Its handler pauses the world — the one state the verification
   cannot run in — so an entire session cycled F3 and waited for a verify that could never appear.
   The drive lives on F4; the placement cycle it displaced is unbound (the park has nothing left to
   answer).

## 5. What the route did NOT need

Worth recording, because each was a real hypothesis that a measurement killed:

- re-activating the behaviour graph (`is-active` never dropped),
- moving the puppet out of the park (`parked-deep` generates fine),
- the Havok character-LOD fields (`currentLOD`/`numTracksInLOD` are `-1` by design — OAR treats that
  as "no cap"),
- calling `hkbBehaviorGraph::Generate` directly with a hand-built context (the skip flag does it
  through the engine's own entry point),
- a decompiler (the shipped binary is Steam-wrapped; the live dump replaced one).

## 6. Next

1. **R01 direct draw** — the panel still renders the clone, so the route is proven but not visible.
   Draw the puppet's own third-person 3D; framing must use the temporary root transform because the
   accumulator camera is null.
2. **Name the 6 unreachable bones** (of 98 sampled) — likely the same unnamed family as the probe's
   `missing=10`, and worth confirming before coverage is called complete.
3. Cosmetic: `A2 frames=` reads 1 by construction; it should count frames, not report the last one.
