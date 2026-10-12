# AFP5 verdict: the paused write lands — the route is closed

Date: 2026-10-13 · build `CharacterPanel-scopy-AFP5-e970256356+dirty-cl94faaed0c6-20261012T163457Z`
Log: `docs/diagnostics/CharacterPanel-afp5-latch-20261013.log`
Baseline: [afp4 verdict](afp4-verdict-2026-10-13.md)

## 1. The write happened

```
00:43:56.784  verify generation=0 samples=68 names skeleton=116 tree=62 table=36
              bone-nodes within-1deg=42/58 ratio=0.72
              skeleton   within-1deg=58/62 ratio=0.94   match=1 reason=matched

00:45:39.771  A2 mapping=skeleton verified=0 generation=0 written=36 unreachable=32
              frames=1 verifications=1 max-rot=13.937deg max-pos=0.100
```

`written=36` with `max-rot=13.937deg`: the behaviour graph's own final output pose was written into
the puppet Actor's own skeleton, while the world was paused, on the panel's clock. That is
PRD 0.9 §6.6 — same Actor, its own final pose, its own skeleton — done.

The mapping that wrote it is `skeleton`, proved at 0.94 in the running world without force, and the
latch carried that proof through the force flip into the paused write, which is exactly what AFP5
changed.

## 2. The pose is force-generated, and both buffers now carry it

At the mark, while paused, `parked-deep`:

```
census  num-anim-bones=68
A0A1    pose0=68/68 pose1=68/68 pose-max-rot=0.079deg
state   is-active=1 node-clones=109 interp=(0.194,0.068) do-foot-ik=1
```

Two things worth keeping:

- **Both output buffers hold 68 elements** (`pose0=68/68 pose1=68/68`). Every earlier round had one
  live and one released. A forced generate keeps both populated.
- **`interp=(0.194,0.068)`** is the first non-trivial interpolation reading in this project's logs, and
  it is the two-buffer blend the `[Animation]` INI block describes (`fAnimInterpNearDist`/
  `fAnimInterpFarDist`, `bAnimInterpEnable`) finally visible in motion. The mechanism inferred in
  AFP3 is now measured directly.

## 3. Reading the numbers honestly

- `samples=68` here against 116 in AFP3 — this session's puppet is in a reduced state (68 anim bones,
  `tree=62`, `table=36`), not the 116-bone one. The order still proves (0.94) and still writes.
- `unreachable=32` is the bone-table limit quantified: `m_targets` resolves 36 of 68 through the
  engine's table and 32 have no table entry. It is counted, never silently dropped, and it is the
  next structural question for the write — independent of everything this round settled.
- `drive=off` by mark time, hence `frames=1`: the write occurred in the force window, then the
  operator cycled past it. One frame of write is what was asked for and what the receipt shows.

## 4. What the route now rests on

| piece | status |
|---|---|
| index order (skeleton, not boneNodes) | proved 0.94–1.00 on 109 bones (AFP4) |
| pose source while paused | force-generate: 0 → 68/116 bones, reversible (AFP3) |
| graph not deactivated while paused | `is-active=1` throughout (AFP3) |
| mapping proof survives into the write | latch separated (AFP5) |
| write into the Actor's own skeleton | `written=36 max-rot=13.937deg` (AFP5) |

## 5. Next

1. **Write coverage**: `unreachable=32` — move the write target from the bone table to the
   tree-resolved node the verification already matches, so all 68 land. This is a write-path change
   only; the proof is unaffected.
2. **Draw it** (R01): the puppet's own third-person 3D, framed by the temporary root transform — the
   accumulator camera is null, so framing cannot reuse the UI3D camera.
3. Re-confirm at 116 bones (the fuller puppet state) once coverage is fixed.
