# AFP3 verdict: the paused pose was being skipped, and one field stops the skip

Date: 2026-10-13 · build `CharacterPanel-scopy-AFP3-44f6ab6103+dirty-cl94faaed0c6-20261012T154502Z`
Log: `docs/diagnostics/CharacterPanel-afp3b-force-generate-20261013.log`
Baseline: [afp3-paused-evaluation](afp3-paused-evaluation-2026-10-13.md)

## 1. The A/B, one session, one placement, one pause

The puppet stayed `parked-deep` throughout — the previous round had already shown the park is not
the variable. Only F3 moved:

| mark | paused | F3 state | puppet `num-anim-bones` | pose elements | verdict |
|---|---|---|---|---|---|
| 1 | 0 (running) | tick+apply | 0 | 0 | `no-candidate` |
| 2 | 1 | tick+apply | 0 | 0 | `no-candidate` |
| 3 | 1 | **tick+apply+force** | **116** | **116** | **`match=1 reason=matched ratio=1.00 within-1deg=109/109`** |
| 4 | 1 | tick+apply (back off) | 0 | 0 | `no-candidate` |

The player's reference column never moved (116, `ratio=0.96`) — the subject under test is the only
thing that changed.

**The effect is reversible inside one session**: force on → 116, force off → 0. Not a coincidence of
timing, not a one-off graph event.

## 2. What the state lines say about the mechanism

Both subjects, every mark:

```
is-active=1 is-linked=0 update-active-nodes=0 state-changed=1
node-clones=109 (puppet) / 106 (player)   root-generator=0x214b41a2c70
user-data=0x2199871ae80 == the puppet's BShkbAnimationGraph
interp=(1.000,0.000)
```

- **`is-active=1` in every state, including force-off with an empty pose.** The graph is *not*
  deactivated. "The engine deactivates the puppet's graph while paused" — the hypothesis that would
  have needed a re-Activate fix — is **falsified**.
- **`node-clones=109` and a live `root-generator`**: the state machine has its node clones. There is
  nothing to activate.
- **`user-data` equals the graph pointer** for both subjects, confirming `hkbBehaviorGraph::userData`
  is the owning `BShkbAnimationGraph*` (OAR/Precision's trick), so a hook can reach the graph back.
- **`interp=(1.000,0.000)`**: the two output buffers are not mid-blend; the engine is not
  interpolating a previous pose into the live one on these frames.

So: graph active, nodes present, no interpolation — and still no pose, until the one field is set.
That is the skip.

## 3. The byte that moves

`SCOPY FP raw subject=puppet what=anim-graph off=0x238`, three bytes after `numAnimBones` at +0x240:

```
force off:  00 04 2f d1 16 02 00 00 | 00 | 00 00 00 00 01 01 01
force on :  00 04 2f d1 16 02 00 00 | 74 | 00 00 00 00 01 01 01
                                    ^ +0x242
```

and the same frame's within-frame pair:

```
force off:  A0A1 pose0=0/0    pose-max-rot=0.000deg  nodes-moved=1
force on :  A0A1 pose0=116/116 pose-max-rot=0.285deg nodes-moved=14
```

With force on the pose is generated *and the nodes move with it* (14 nodes, 0.287°) — the whole
chain runs, not just a buffer being filled.

## 4. What this settles

1. **The route's premise holds.** The paused puppet's graph *can* produce a complete final output
   pose: 116 of 116 bones, skeleton order, `ratio=1.00`, `within-1deg=109/109`, and the same
   `max-pos=2.796` / `max-scale=0.1483` signature the running-world poses carry. The last three
   rounds' "no pose while paused" was the engine's frame-skip, not a dead graph.
2. **`BSAnimationUpdateData::unk2A` is the lever** — documented by Precision as *"forces animation
   update (hkbGenerator::generate()) without skipping frames"*, and now measured to take the puppet
   from 0 to 116 bones.
3. **The park never mattered** (previous round), and **the pause alone did not disable the graph**
   (this round). The single variable was the skip.
4. `A2` still reports `blocked=unverified-world-first` on the force-on frame: the application stage
   insists on verifying the mapping while the world runs, and this session never gave it a running
   world with a pose to verify against. The read side is solved; the write side needs that
   verification pass (next round).

## 5. Next

- The apply stage must be armed while the world runs, with force on, so it can verify the mapping
  against engine-applied locals that *our* force also produced; then the paused write can proceed.
- `num-anim-bones` reached **116** with force on, against 98 in the running world: force skips the
  distance/interpolation reduction entirely, which is exactly what a paused panel figure wants.
- The engine image is dumped and verified in place (`CharacterPanel-engine-text.bin`, 24 435 000
  bytes, `fnv1a=0xc529f5aecf5545d1`, text-rva 0x1000, `UpdateAnimationGraphManager` at rva 0x54c1f0).
  Every later engine question can now be answered offline with `tools/re/engine_scan.py`.
