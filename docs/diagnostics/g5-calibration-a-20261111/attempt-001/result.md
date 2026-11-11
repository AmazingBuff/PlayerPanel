# G5 offline calibration A

Status: **CALIBRATED_OFFLINE_A_ONLY**, fixture `g5-cal-a-analytic-sign-v1`.

Only the two frozen pairs through shared typed extraction, six-decimal formatting and actual parsing are covered. The aligned 0x30 carrier substitutes for the native transform; it proves no SDK/ABI or native lifetime/completion property. K1-K6, native B, time calibration and G5 remain NOT_READY; no G5 tolerance is produced.

Batch: `attempt-001`. [Frozen plan](plan.json), [manifest](manifest.json), [full metrics](metrics.json).

| round | status | capture exit | track rows | components | raw stdout / stderr |
| --- | --- | --- | --- | --- | --- |
| 1 | PASS | 0 | 4 | 40 | [stdout](round-1.stdout.jsonl) / [stderr](round-1.stderr.txt) |
| 2 | PASS | 0 | 4 | 40 | [stdout](round-2.stdout.jsonl) / [stderr](round-2.stderr.txt) |
| 3 | PASS | 0 | 4 | 40 | [stdout](round-3.stdout.jsonl) / [stderr](round-3.stderr.txt) |

| case | metric | ideal | raw oracle | text / double | ideal to binary32 | raw to text | numerical abs error |
| --- | --- | --- | --- | --- | --- | --- | --- |
| analytic | position_l2 | 5 | 5.00000000000000000 | 5 | 0e-21 | 0e-21 | 0.000000 |
| analytic | rotation_error | 0.133974596215561353 | 0.133974595167876386 | 0.133974640478251583 | -1.047684968e-9 | 4.531037500e-8 | 1.942620602e-16 |
| analytic | scale_linf | 0.125 | 0.125000000000000000 | 0.125 | 0e-23 | 0e-23 | 0.000000 |
| sign | position_l2 | 0 | 0 | 0 | 0 | 0 | 0 |
| sign | rotation_error | 0 | 0 | 3.33066907387546962e-16 | 0 | 0 | 3.330669074e-16 |
| sign | scale_linf | 0 | 0e-23 | 0 | 0e-23 | 0e-23 | 0.000000 |

All three rounds and combined count/min/median/P95/P99/max are in metrics.json (nearest-rank). Scoped combined maxima:

- position_l2: text representation `0`; double numerical `0`.
- rotation_error: text representation `4.531037500337655777516939213693777596371178004239022693104787168839494894E-8`; double numerical `3.3306690738754696212708950042724609375E-16`.
- scale_linf: text representation `0E-23`; double numerical `0.000000`.

Repeated raw bits and formatted rows identical: `True` / `True`. Per-case cross-round metric ranges: `{'analytic': {'position_l2': '0.0', 'rotation_error': '0E-17', 'scale_linf': '0.000'}, 'sign': {'position_l2': '0.0', 'rotation_error': '0E-32', 'scale_linf': '0.0'}}`.

The component Q limit is 5e-7 per side; pair position Q is sqrt(3)*1e-6 and scale Q is 1e-6. Rotation Q uses these fixture quaternion norms and normalized chord bounds; it is not a general quaternion bound. Arithmetic comparison limits come from binary64 u and operation counts in plan.json, independently of G5 thresholds.

Raw-vs-ideal rotation deviation is authored ideal-to-binary32 representation. Raw-to-text changes are formatting representation. Double-vs-Decimal residuals are numerical analysis/decimal parsing. Repeat ranges describe this fixed offline fixture only; none is native measurement noise or a population guarantee.
