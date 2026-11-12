# G5 observation v2 offline result — 2026-11-12

**OFFLINE VERIFIED; NATIVE VALIDATION PENDING.** Approved task `g5-observation-v2-20261112`, contract revision 1; baseline `oar-idle3` / `dfd37acc166f2ca8dbc40ff1ac94fb54ad095626`, initially empty index and clean tracked worktree. The untracked `src/base/util.cpp` / `.h` were preserved. No commit, push, deployment or game launch was authorized or performed.

[Canonical specification](../../independent-behavior-graph.md#c-g5-observation-v2-20261112), [build commands and hashes](../../build.md#v96-g5-observation-v2-20261112), [manual package](../../../dist/g5-observation-v2-20261112/MANUAL.md), [source/build/package manifest](manifest.json).

| Check | Actual result | Evidence |
| --- | --- | --- |
| Initial MSVC strict compile | exit 2; C4389 under /WX from optional uint32 vs int literal in the regression | [initial output](verification/focused-compile.txt) |
| Corrected MSVC strict compile | exit 0, no warning; unsigned literal corrected | [fixed output](verification/focused-compile-fixed.txt) |
| Exactly two focused scenarios | exit 0; A completed equip=120 / scripted unequip=123, B timeout=180 | [scenario output](verification/focused-scenarios.txt) |
| Release configure | exit 0; existing skip-install note | [configure output](verification/release-configure.txt) |
| Release build | exit 0; only existing D9025 override notices | [build output](verification/release-build.txt) |
| Initial artifact/document/scope check | exit 1; result document linked the validator output before its first buffered write created the file | [initial static output](verification/artifacts-and-docs-initial.txt) |
| Artifact/document/scope checks | exit 0; bounded static checks, no extra behavioral cases | [verification output](verification/artifacts-and-docs.txt) |

The policy shared by production and the fake observations is [observation_policy.h](../../../src/animation/behavior_graph/observation_policy.h). Integration is in [independent_graph.cpp](../../../src/animation/behavior_graph/independent_graph.cpp), with two scripted scenarios in [behavior_graph_observation_test.cpp](../../../tests/behavior_graph_observation_test.cpp). It records lossless float32 clocks and absences, separate axes, post-Generate completed counters, deduplicated sparse checkpoints and a truthful bounded state-12-to-0 / timeout / invalid end. Overflow is exposed; full-name/node/binding keys are process-local only. No native ABI, hook, resolver, setting value or prime value changed.

The original lossy bug is reproduced inside scenario A: positive 23 maps to `N`, the same as any negative delta. This is not a third scenario. Exact bits now preserve the fractional and signed observations; first appearance/reappearance has no invented delta. Scenario B never declares completion even after the full cap.

The first compile failure was an implementation-test warning, not a native failure, and its output remains retained. The first static check passed worktree/package/config checks, then caught its own not-yet-created output link; retaining that first output established the artifact path, and the final bounded check passed. No full historical test suite or case matrix ran. The dated calibration-A pack and its original source hashes were not rerun, modified or relabelled.

DLL/PDB/INI copy hashes, exact source hashes, build identity and seven manual overrides are in the manifest. The UTF-8/no-BOM INI keeps both display keys and source instrumentation/liveness off, and preserves every other sample value. Repository sample values stay dormant.

Native validation is **UNVERIFIED** on all separate axes: v96 clip-family selection, state completion, clock semantics/rate, control B, B3/B4, source completion pairing, input/resource/instance identity and pose equivalence. The 180 cap is a predeclared experimental budget, not a predicted native completion point. No tolerance, G5/G6 pass or source-write claim is made.
