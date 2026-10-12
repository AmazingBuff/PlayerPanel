# AFP8 verdict: the same-Actor route is closed end to end

Date: 2026-10-13 · build `CharacterPanel-scopy-AFP8-ac27f6e83a+dirty-cl94faaed0c6-20261012T173308Z`
Log: `docs/diagnostics/CharacterPanel-afp8-f4-drive-20261013.log`
Baseline: [afp5 verdict](afp5-verdict-2026-10-13.md), [afp6/7 latch+coverage fixes](afp4-verdict-2026-10-13.md)

## 1. The receipt

```
01:37:50  verify samples=98 names skeleton=116 tree=92 table=37
          skeleton 90/92 ratio=0.98   match=1 reason=matched

01:38:48  A2  drive=tick+apply+force apply=on mapping=skeleton verified=1
              written=92 unreachable=6 frames=1 max-rot=0.028deg max-pos=0.000
              blocked=verified
```

And the mark's own calibration, taken on the same frame — pose versus the puppet's node locals:

```
order=skeleton slot=0 sampled=116 named=109 compared=109 within-1deg=109 beyond=0
               ratio=1.00 max-rot=0.048deg max-pos=0.000 max-scale=0.0000 outliers=-
world order=skeleton compared=109 within-1deg=104/109 ratio=0.95
               outliers=WeaponAxe,WeaponBack,WeaponBow,WeaponDagger,WeaponMace
```

The world was paused and the engine applies nothing while paused, so the only thing that can have
put the pose onto those nodes is this stage's write. `max-rot=0.048°` is the hkQsTransform →
NiTransform conversion's rounding — i.e. the nodes hold the pose to within conversion noise, on all
109 named bones. Even the world-space composition now agrees (104/109); AFP1 measured the same
comparison at **0/92**. The five world-space outliers are weapon attachment nodes, which the graph
does not drive.

## 2. What each fix contributed

| change | measured effect |
|---|---|
| drive moved to F4 (F3 belongs to another mod that pauses the world) | `verify` finally appears: 0.98, and the session reached the force window |
| proof survives a drive cycle (AFP7) | the paused forced frame wrote instead of reporting `unverified-world-first` |
| write resolves by name in the tree (AFP7) | `written=36 unreachable=32` → **`written=92 unreachable=6`** |
| force-generate (AFP3) | paused, `parked-deep`: `num-anim-bones=116`, slot0 `num-data=116 readable=1`, `+0x242=0x74` |

## 3. The route, as it now stands

While the panel pauses the world, the puppet Actor's own behaviour graph produces its own final
output pose (116 of 116 bones), and that pose is written into the puppet Actor's own skeleton —
proved on the skeleton itself at 1.00 with 0.048° of conversion noise. PRD 0.9 §6.6's premise, plus
R03 (index order) and R04 (the A/B pair), are all measured and green.

## 4. Still open

1. **R01 direct draw** — the panel still draws the *clone*, so nothing is visually different yet.
   Drawing the puppet's own third-person 3D is the next work item (framing must use the temporary
   root transform; the accumulator camera is null).
2. **6 unreachable bones** of 98 sampled — bones whose names resolve neither in the tree nor in the
   table. Worth naming (the probe logs `missing=10` unnamed table entries; these six are probably the
   same family) before calling coverage complete.
3. Cosmetic: the `A2 frames=` counter reads 1 by construction (the stats are replaced per frame), so
   it reports "the last frame wrote", not how many frames wrote.
