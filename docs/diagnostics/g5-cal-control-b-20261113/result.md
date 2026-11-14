# K2-C4 control B qualification — 2026-11-13

**Verdict: NOT_READY. No native control output was viewed and no control was run.**

Canonical record: [C-G5-CAL K2-C4](../../independent-behavior-graph.md#c-g5-cal-b-qual).

## Base-game candidate (offline authored data)

- Path in archive: `meshes\actors\character\animations\male\mt_idle.hkx` (`Skyrim - Animations.bsa`).
- Bytes / SHA-256: 30,944 / `18acc5a60644a5a9d3b10c6bfa1d35ce891c7687869084893ab1cc602433c18c`.
- Container: 1 animation, 1 binding, 0 skeletons, 0 attachments, 0 skins.
- Animation: `hkaSplineCompressedAnimation` type 5, duration 9.133333 s, 99 transform + 4 float tracks, 275 frames / 2 blocks, frameDuration 0.033333.
- Events: 99 annotation tracks, **0 annotations**.
- Binding: skeleton `NPC Root [Root]`, identity transform-track map, blendHint 0.

Full structural dump: [mt_idle_base_audit.txt](mt_idle_base_audit.txt).

## Runtime disqualifier

MO2 supplies a loose override at `Vanargand Animations II - Male Idle Walk and Run\meshes\actors\character\animations\male\mt_idle.hkx` — 110,320 B, SHA-256 `24e892368499091949882ea43a6087c84be03429afed48bc10e101a7f1b64235`. OAR configs (Ashe etc.) also reference `mt_idle`. The runtime path therefore cannot be assumed to be the audited 30,944-byte file.

## Missing pieces

1. **Independent truth**: spline-compressed keyframes need an audited decoder; using `SamplePartialTracks` output as truth is circular.
2. **Carrier**: CommonLibSSE exposes no audited way to load an arbitrary HKX into an `hkaAnimation*`; `SamplePartialTracks` needs an already-loaded animation.

## Next (new contract required)

- **A (recommended)**: audited Havok spline decoder; choose a binding whose runtime bytes match the audited file (no loose/OAR replacement); compare at fixed keyframe times.
- **B**: author an `hkaInterleavedUncompressedAnimation` control HKX (known keyframes, endpoint samples) plus an audited arbitrary-HKX loader; costlier because no public API exists.
