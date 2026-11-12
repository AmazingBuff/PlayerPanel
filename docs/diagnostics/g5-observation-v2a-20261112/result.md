# G5 observation v2a local rebuild — 2026-11-12

**OFFLINE REBUILT AND VERIFIED; NATIVE VALIDATION PENDING.** Task `g5-observation-v2a-20261112`, contract revision 1; baseline `oar-idle3` / `f39ffa8b36b691b9968064ee0a2a59a67a87578d`, clean tracked worktree. This rebuild exists because the v96 package and its `build/Release` artifacts were not present in this checkout; the committed observation-v2 source is unchanged. No deployment or game launch was performed.

[Canonical specification](../../independent-behavior-graph.md#c-g5-observation-v2-20261112), [build record](../../build.md#v96a-g5-observation-v2a-20261112), [manual package](../../../dist/g5-observation-v2a-20261112/MANUAL.md), [source/build/package manifest](manifest.json).

| Check | Actual result | Evidence |
| --- | --- | --- |
| Focused MSVC strict compile | exit 0; no warning | [compile output](verification/focused-compile.txt) |
| Exactly two focused scenarios | exit 0; A equip=120 / unequip=123, B timeout=180 | [scenario output](verification/focused-scenarios.txt) |
| Release configure | exit 0; existing skip-install note | [configure output](verification/release-configure.txt) |
| Release build | exit 0; only existing D9025 override notices | [build output](verification/release-build.txt) |
| Artifact/document/scope checks | exit 0; package hashes, embedded identity, clean baseline, seven INI overrides | [verification output](verification/artifacts-and-docs.txt) |

The rebuilt package is `v96a-g5cal-observation-v2`, identity `CharacterPanel-scenegraph-v96a-g5cal-observation-v2-f39ffa8b36-cl94faaed0c6-20261112T134517Z`. It contains `CharacterPanel.dll` (1,850,368 B, `2c5112ae…eab33f5`), `CharacterPanel.pdb` (50,016,256 B, `e7803453…1814cf8`) and `CharacterPanel.ini` (11,703 B, `f41990d2…bb04b7e90c`). The DLL/PDB match `build/Release`; the identity string is present in the DLL.

The four committed observation-v2 source hashes are identical to the v96 manifest. Only the baseline commit, dirty-state suffix, configure stamp and resulting binary hashes differ: v96 recorded `dfd37acc16+dirty` from an uncommitted tree, v96a records clean `f39ffa8b36`. No source, setting, prime value, limit or behavioral rule changed.

The manual INI is derived from the committed sample with exactly the seven v96 overrides (`inject`, probe on, empty input variable, both prime switches on, unequip probe on, `Unequip` event). All other values, including display and source instrumentation/liveness, remain at the sample values; the file is UTF-8 without BOM.

The first MSBuild attempt inside the command sandbox failed while MSBuild queried the user Windows SDK directory; rerunning the same build outside the sandbox exited 0. That was an environment permission failure, not a source failure, and no source was changed to work around it.

Native validation remains **UNVERIFIED** on every axis: clip-family selection, state-12-to-0 completion, clock semantics/rate, control B, B3/B4, source completion pairing, input/resource/instance identity and pose equivalence. This rebuild does not add evidence to any of them and does not change the G5/G6 verdict.
