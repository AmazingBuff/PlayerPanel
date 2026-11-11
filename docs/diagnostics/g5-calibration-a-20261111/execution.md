# Offline control A execution record

The scope is [C-G5-CAL v1 control A](../../independent-behavior-graph.md#c-g5-cal-v1), implemented under task `g5-offline-calibration-a-20261111`, revision 1. The frozen fixture is `g5-cal-a-analytic-sign-v1`. The current [attempt-002 report](attempt-002/result.md), [plan](attempt-002/plan.json), [manifest](attempt-002/manifest.json) and [full metrics](attempt-002/metrics.json) retain all numbers and raw observations. [Attempt-001](attempt-001/manifest.json) remains the first valid batch. Both batches pass independently; attempt-002 follows the reasoned evidence-finalization repair described in the [index](README.md).

## Identity and capture boundary

- Repository baseline: `oar-idle3`, `a7dbf557f48ab2684180d803a164ac9cd7c4bbdc`, no staged paths. The four existing documentation edits were authorized to extend. `src/base/util.cpp` and `util.h` stayed untouched.
- Windows 11 `10.0.26300`; Python `3.12.10`, binary64 (`mant_dig=53`, radix 2), Decimal precision 80. The Python runtime identifies its own compiler as MSC v.1943; the capture's actual compiler is independently recorded in each header as `_MSC_FULL_VER=194435229` (MSVC 19.44.35229), STL version 143/update 202503.
- Standalone compiler environment: Visual Studio 2022 Community, toolset directory `14.44.35207`, Developer Command Prompt v17.14.42. CMake 4.1.0 and MSBuild 17.14.60 were used for the compatibility attempt.
- Capture executable SHA-256: `3f92424f62b28eb9fc822573e0049c365a47931882a8fc5c0df00fcedd738749`. Current analyzer SHA-256: `a4ced37e81044b535aeddf0740a51a2aa0de0fc53c6383a02c5f5dada8da4eb8`. The manifest retains every relevant source digest and verifies source/executable identity after collection.
- [Shared helper](../../../src/animation/behavior_graph/pose_capture_policy.h): `capture_pose_sample` reads typed translation/rotation/scale members; `format_source_pose_track` owns the unchanged SOURCE TIMELINE track message and six-decimal fields. [Both](../../../src/animation/behavior_graph/independent_graph.cpp) [production adapters](../../../src/animation/behavior_graph/source_timeline.cpp) delegate to it.
- [Capture fixture](../../../tests/pose_calibration_capture.cpp): an explicitly offline, 16-byte aligned, 0x30 three-quad carrier. Exactly ten channels (3+4+3) are captured; independently fixed NaN/infinity fourth-lane poisons do not appear among them. This is no SDK/ABI, completion, thread, lifetime or native-pose proof. The log envelope/sink itself is outside this offline round trip.

## Actual commands and exits

Run from the repository root, using the existing VS 2022 installation (no dependency fetch):

```powershell
New-Item -ItemType Directory -Path build/g5-calibration-a-20261111
cmd /d /s /c 'call "D:\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.44.35207 && cl /nologo /std:c++latest /EHsc /W4 /WX /utf-8 /I src tests\pose_calibration_capture.cpp /Fo:build\g5-calibration-a-20261111\pose_calibration_capture.obj /Fe:build\g5-calibration-a-20261111\pose_calibration_capture.exe'
python tools/calibration/calibrate_pose.py --capture build/g5-calibration-a-20261111/pose_calibration_capture.exe --output docs/diagnostics/g5-calibration-a-20261111
python tools/calibration/calibrate_pose.py --capture build/g5-calibration-a-20261111/pose_calibration_capture.exe --output docs/diagnostics/g5-calibration-a-20261111 --reason 'Evidence-finalization review repair: record integrity-failure reasons and identify incomplete prior batches; same frozen fixtures and numerical rules; preserve valid attempt-001.'
cmake -S . -B build -DVCPKG_MANIFEST_INSTALL=OFF
cmake --build --preset Release --parallel 2
cmake --build --preset Release --parallel 2 -- /p:_LatestWindowsTargetPlatformVersion=10.0.26100.0
```

| command | exit | evidence |
| --- | --- | --- |
| focused MSVC compile | 0 | [compile output](verification/compile.txt), no warnings under `/W4 /WX` |
| first analysis command | 0 | [attempt-001](attempt-001/manifest.json); all three capture subprocesses exit 0, empty stderr |
| reasoned current-analyzer command | 0 | [attempt-002](attempt-002/manifest.json); all three capture subprocesses exit 0, empty stderr |
| offline reconfigure | 0 | [configure output](verification/compatibility-configure.txt); existing cache/compiler/dependencies, manifest installation disabled; existing skip-install warning |
| required Release build | 1 | [first failure](verification/compatibility-build.txt): `GetLatestSDKTargetPlatformVersion` cannot access `C:\Users\lhq49\AppData\Local\Microsoft SDKs` |
| one explicit installed-SDK retry | 1 | [retry failure](verification/compatibility-build-sdk-pin.txt): `GetPlatformSDKLocation(Windows, 10.0.26100.0)` hits the same denied path |

The Release compatibility build is **UNVERIFIED**: it stops during SDK discovery, before the production translation units compile. No DLL is qualified by this attempt. The existing cache label `behavior-graph-v46-retained-copy-ledger-20261029` was retained; configure metadata using it is local only and does not identify a new runtime calibration build. No permissions/toolchain/dependencies were broadened; there was no game launch, deployment, staging, commit or push. The recorded v92 install was not revalidated.

Re-running into this output directory requires a new `--reason`; each batch uses an exclusive numbered directory and never overwrites an earlier attempt. `result.md` at the pack root deliberately remains first-valid provenance. Current source-hash navigation uses attempt-002.

## Numerical interpretation

Truth is independent: the manifest freezes authored literals, independently expected IEEE-754 bits and analytic ideal errors before starting the executable. The double analyzer normalizes each quaternion then dots; the separate Decimal(80) oracle uses `abs(dot)/sqrt(norm2_a*norm2_b)`. All raw bits, exact raw/serialized decimals and decimal-to-binary64 parse residuals are retained.

| case / metric | analytic ideal | binary32 oracle | parsed double | ideal to binary32 | raw to text oracle drift |
| --- | --- | --- | --- | --- | --- |
| analytic position L2 | 5 | 5 | 5 | 0 | 0 |
| analytic rotation error | 0.1339745962155613532363 | 0.1339745951678763855369 | 0.133974640478251583 | -1.0476849676993885e-9 | +4.531037500337656e-8 |
| analytic XYZ scale Linf | 0.125 | 0.125 | 0.125 | 0 | 0 |
| sign position / scale | 0 / 0 | 0 / 0 | 0 / 0 | 0 / 0 | 0 / 0 |
| sign rotation error | 0 | 0 | 3.3306690738754696e-16 | 0 | 0 |

The sign pair's Decimal raw/text errors are exactly zero; its small double residual is numerical arithmetic, within the derived `3.552713678800507e-15` text-analysis bound. The maximum raw double numerical residual is `7.907067087577111e-17`; maximum text double residual across both cases and all rounds is `3.3306690738754696e-16`. Position and scale numerical residuals are zero in these cases.

Zero *paired metric drift* does not mean zero position/scale serialization loss. Maximum component rounding is `4.76837158203125e-7`; maximum single-transform position L2 rounding is `5.3312014997000448618e-7`, and maximum single-transform scale component rounding is `3.5762786865234375e-7`. The fixed pair shares rounding terms that cancel in its position/scale differences. Maximum decimal-to-binary64 component parse error is `4.869349368163967e-17`.

The independently justified six-decimal per-component bound is `5e-7`; two-side pair bounds are `sqrt(3)*1e-6` for position and `1e-6` for XYZ scale. Rotation transport is checked only for the frozen norms (approximately 0.9999999829 through 0.9999999865), using normalized chord perturbations; the observed `4.531037500337656e-8` is fixture-scoped. This supplies neither a general quaternion bound nor a native tolerance.

The [plan's numerical derivation](attempt-002/plan.json) uses binary64 unit roundoff `u=2^-53` and `gamma(n)=n*u/(1-n*u)`, with component parsing separated from arithmetic. Position uses a Lipschitz input term plus `gamma(12)` (nine arithmetic operations); scale uses one subtraction factor plus its parse term. Quaternion norm/division uses `gamma(10)`, a four-product/three-add dot uses `gamma(7)`, and the final subtraction adds `u`; text parsing contributes normalized-chord terms `<=4u`. These are floating-point precision bounds, not G5 thresholds. All fixed-case errors satisfy them.

Each batch retains per-round, per-case and combined count/min/median/P95/P99/max; median and percentiles use nearest-rank. Across the three current-analyzer rounds, raw bits, formatted rows and compiler identities are identical, and all per-case metric ranges are zero. This is repetition of two fixed controls, not a population guarantee or native noise estimate. `tau_p/tau_r/tau_s/epsilon_t` remain unproduced; K1-K6, native B, time calibration, complete layers/weights/mapping, B3/B4 and G5 stay open.

## Final static review

The bounded read-only review checks changed/generated local links and anchors, 16 named symbols in five code/test entry paths, exact equality of the previous production track format literal, both retained artifact manifests, all six current source hashes, forbidden util hashes, unchanged HEAD/empty index, whitespace and the 200-line summary budget. It adds no behavioral cases.

The [first diagnostic invocation](verification/static-review-initial.txt) exited 1 before checks because Python's default cp1252 subprocess decoding could not read the Chinese Git diff. The diagnostic was corrected to request UTF-8 explicitly; the [corrected review](verification/static-review-utf8.txt) exited 0. The [final review](verification/static-review-final.txt) and [git diff check](verification/diff-check.txt) both exit 0. These diagnostic-script runs do not change the calibration analyzer or require another calibration batch.

```powershell
python build/g5-calibration-a-20261111/verify_retained_evidence.py
git diff --check
```

## Commit packaging and runtime handoff

The user requested the local commit and will perform program-internal calibration. Native B, timing, runtime validation and G5 remain pending.

Git attributes preserve this evidence pack's exact raw bytes, including CRLF capture stdout and LF manifests. Each of the six manifested source files retains its original working-tree line endings on checkout. Artifact hashes refer to raw bytes; source hashes refer to those working-tree bytes, while Git may store normalized text blobs. This packaging change modifies neither calibration data nor measured source/analysis code and requires no new calibration batch.
