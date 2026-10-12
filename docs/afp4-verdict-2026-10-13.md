# AFP4 verdict: the mapping is proved on 109 bones; forcing a generate breaks the engine's own apply

Date: 2026-10-13 · build `CharacterPanel-scopy-AFP4-7f8a8668b0+dirty-cl94faaed0c6-20261012T161535Z`
Log: `docs/diagnostics/CharacterPanel-afp4-apply-20261013.log`
Baseline: [afp3 verdict](afp3-verdict-2026-10-13.md)

## 1. The tree fix works — the order is now proved on the full skeleton

```
00:23:42.843  verify generation=3 samples=116 names skeleton=116 tree=109 table=37
              bone-nodes within-1deg=49/106  ratio=0.46
              skeleton   within-1deg=108/109 ratio=0.99   match=1 reason=matched
```

The skeleton order now scores **108 of 109** at 1.0° instead of 33 of 37. Taking the candidate local
from the matched tree node instead of only from bones the bone table addresses was the whole
difference: the proof went from a third of the skeleton to all of it, and `match=1` follows.

R03's question — *which index order does the pose use* — is answered, on the engine's own applied
locals, at 0.99.

## 2. But forcing a generate degrades the very thing the proof compares against

Thirty-two milliseconds later, on the frame the force flag flipped:

```
00:23:52.094  drive=tick+apply+force apply=on force-generate=on
00:23:52.126  verify skeleton within-1deg=73/109 ratio=0.67   match=0 reason=below-ratio
00:23:55.228  verify skeleton within-1deg=64/109 ratio=0.59   match=0 reason=below-ratio
```

The bracket order rules out a sampling artifact — the driver ticks (renderer.cpp:86) *before* the
application stage (renderer.cpp:89), so the pose being scored on that frame really was generated with
force on. The degradation is monotonic (108 → 73 → 64) and tracks the forced ticks.

**What this says.** `verify()` proves the mapping by comparing the graph's output pose against the
node locals *the engine applied*. A forced generate produces a pose the engine did not choose to
apply on that frame — the engine's own apply pass is still running its own schedule — so the two
drift apart, and the score measures that drift rather than the index order. The pose is a valid pose
(AFP3 measured 116/116 with a 109/109 match); what is no longer true is that it matches the locals
currently standing on the skeleton.

This is not a regression in the mapping. It is the verification method meeting a source it was not
designed for: it assumed one generator of the locals, and force made two.

## 3. Where that leaves the route

- **Read side: solved.** Paused, force on: 116 bones, skeleton order, 109/109. Proved twice.
- **Proof of the order: solved** at 0.99 on 109 bones — but only while the world runs *without*
  force, i.e. against the engine's own applied locals.
- **Write side: still blocked**, and now by a real design question rather than a bug: what is the
  reference a forced pose should be verified against?

The clean answer is that the two are now separable, and AFP3 already gave us the evidence for each:
prove the **order** where the engine applies (no force, 0.99, done — and latch it), then write the
**pose** where the panel owns the clock (paused, force on, 116 bones). Verifying the order does not
require the pose to be force-generated; it requires the locals and the pose to have the same origin,
which is true while the world runs without force.

## 4. Next

Decouple the two: keep `verify()` strictly in the no-force running world and let it latch
(`m_verified` survives into the force state instead of being reset by it), so the paused forced write
proceeds on a mapping that was proved under the one condition where proof is meaningful. The latch
reset added this round should therefore be narrowed: re-arm on identity change and on `cycle()`, but
not merely because the force flag moved.

Also worth carrying forward: `table=37` is holding at 37 across every state, so the write will report
~79 bones unreachable (116 − 37) unless the write path is also moved to the tree-resolved node. That
is the next structural question for the write, and it is independent of the verification one.
