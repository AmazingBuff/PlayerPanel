# AFP8 verdict: paused final-pose application has positive evidence; direct draw remains open

**2026-10-13 review:** this is an application-stage result, not end-to-end panel validation. The renderer still draws the copied graph. See [next validation and paused gaps](afp-next-validation-and-paused-gaps-2026-10-13.md).

Date: 2026-10-13 · build `CharacterPanel-scopy-AFP8-ac27f6e83a+dirty-cl94faaed0c6-20261012T173308Z`
Log: `docs/diagnostics/CharacterPanel-afp8-f4-drive-20261013.log`
Baseline: [afp5 verdict](afp5-verdict-2026-10-13.md), [afp6/7 latch+coverage fixes](afp4-verdict-2026-10-13.md)

## 1. The receipt

```
01:37:50  verify samples=98 names skeleton=116 tree=92 table=37
          skeleton 90/92 ratio=0.98   match=1 reason=matched

01:38:48  A2  drive=tick+apply+force apply=on mapping=skeleton verified=1
              written=92 unreachable=6 frames=1 max-rot=0.028deg max-pos=0.000
              blocked=verifying
```

And the mark's own calibration, taken on the same frame — pose versus the puppet's node locals:

```
order=skeleton slot=0 sampled=116 named=109 compared=109 within-1deg=109 beyond=0
               ratio=1.00 max-rot=0.048deg max-pos=0.000 max-scale=0.0000 outliers=-
world order=skeleton compared=109 within-1deg=104/109 ratio=0.95
               outliers=WeaponAxe,WeaponBack,WeaponBow,WeaponDagger,WeaponMace
```

The application explicitly reports 92 written nodes. The 109/109 comparison establishes agreement
between the observed nodes and the pose, not that this stage wrote all 109 or was their only writer.
The small rotation residual is consistent with conversion noise but does not prove that cause alone.
World-space agreement is 104/109; the five weapon attachment outliers require attachment, hierarchy
and skin relevance checks before they may be excluded.

Verification captured 98 samples while the forced output contains 116. `apply()` limits its loop to
`min(m_targets.size(), count)`, and verification sized that table and the name list from the 98 samples.
Thus 92 written + 6 unreachable describes only the initial 98 slots; 18 later output slots are not
processed by this loop. Complete coverage is not established.

## 2. What each fix contributed

| change | measured effect |
|---|---|
| drive moved to F4 (F3 belongs to another mod that pauses the world) | `verify` finally appears: 0.98, and the session reached the force window |
| proof survives a drive cycle (AFP7) | the paused forced frame wrote instead of reporting `unverified-world-first` |
| write resolves by name in the tree (AFP7) | `written=36 unreachable=32` → **`written=92 unreachable=6`** |
| force-generate (AFP3) | paused, `parked-deep`: `num-anim-bones=116`, slot0 `num-data=116 readable=1`, `+0x242=0x74` |

## 3. The route, as it now stands

While the panel pauses the world, the puppet's graph can produce a 116-element output and the
application writes 92 nodes of its own skeleton. R02 and the index-order portion of R03 have
positive evidence. R04's node-application portion has evidence, but its actual draw/skin consumption
is not yet measured. R01 and complete R03/R04 remain open; physics, expressions and cleanup do too.

## 4. Still open

1. Audit the 98→116 coverage limit; list the 18 unprocessed slots and 6 unreachable items, with
   bone/branch/skin relevance and explicit reasons.
2. **R01 direct draw**: connect the puppet's own 3D and A2/A4/A5. If temporary framing transforms
   are used, prove complete restoration and synchronization; a null accumulator camera alone does
   not prove every camera-adaptation route impossible.
3. Validate buffer selection/interpolation through the full idle/equip/idle transition, including
   both output slots populated. Selecting the first readable slot is not yet a verified policy.
4. Validate R08 physical simulation and FaceGen independently; record the user's paused observation
   of animation without physics or expressions without attributing it to an unconfirmed build.
5. Cosmetic: `A2 frames=1` is a last-frame statistic, not duration or stress evidence.
