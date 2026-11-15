#!/usr/bin/env python3
"""Offline unit tests for tools/cal1b_calibrate.py — C-G5-CAL1-B v1.

All fixtures are synthetic; no archived game data is used. Pure stdlib:

  python -I tools/cal1b_calibrate_test.py
"""

import contextlib
import io
import json
import math
import os
import struct
import sys
import tempfile
from decimal import Decimal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cal1b_calibrate as cal1b  # noqa: E402
import cal2_adjudicate as c2  # noqa: E402
import cal2_adjudicate_test as c2t  # noqa: E402

FAILURES = []


def check(name: str, condition: bool, detail: str = "") -> None:
    if condition:
        print(f"PASS {name}")
    else:
        FAILURES.append(name)
        print(f"FAIL {name} {detail}")


def bits(value: float) -> int:
    return struct.unpack("<I", struct.pack("<f", value))[0]


def record_for(track_values: dict) -> dict:
    data = [0] * (c2.REGISTERED_TRACKS * c2.GRID_COMPONENT_COUNT)
    for track, values in track_values.items():
        base = track * c2.GRID_COMPONENT_COUNT
        data[base:base + 10] = values
    return {"data": tuple(data)}


def write_text(path: str, content: str) -> None:
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(content)


def write_c2_ledger(path: str, truth_sha: str, artifact_shas, log_shas) -> None:
    runs = []
    for artifact_sha, log_sha in zip(artifact_shas, log_shas):
        runs.append({
            "artifact": {"sha256": artifact_sha},
            "c2_a": {"verdict": "PASS"},
            "c2_b": {"verdict": "PASS"},
            "c2_c": {"verdict": "PASS"},
            "c2_e": {"verdict": "PASS", "log": {"sha256": log_sha}},
            "c2_f": {"verdict": "PASS"},
        })
    ledger = {
        "schema": "C-G5-CAL2-v1-adjudication",
        "truth": {"sha256": truth_sha},
        "runs": runs,
        "batch": {"verdict": "READY", "c2_d": {"verdict": "PASS"}},
    }
    write_text(path, json.dumps(ledger, indent=1) + "\n")


def synthetic_inputs(directory: str, differ=False):
    truth = c2t.build_truth(directory)
    artifacts = []
    logs = []
    for index in range(3):
        path = os.path.join(directory, f"run{index}.bin")
        if differ and index == 2:
            c2t.make_artifact(truth, path, mutate=c2t.mutate_component(0, 0, 0.5))
        else:
            c2t.make_artifact(truth, path)
        artifacts.append(path)
        log_path = os.path.join(directory, f"run{index}.log")
        write_text(log_path, f"synthetic log {index}\n")
        logs.append(log_path)
    truth_path = os.path.join(directory, "truth.txt")
    artifact_shas = [cal1b.sha256_file(path) for path in artifacts]
    log_shas = [cal1b.sha256_file(path) for path in logs]
    truth_sha = cal1b.sha256_file(truth_path)
    c2_path = os.path.join(directory, "c2.json")
    write_c2_ledger(c2_path, truth_sha, artifact_shas, log_shas)
    expected = {
        "artifacts": artifact_shas,
        "logs": log_shas,
        "truth": truth_sha,
        "c2_json": cal1b.sha256_file(c2_path),
    }
    return truth, artifacts, logs, truth_path, c2_path, expected


def test_rounding():
    check("round_zero", cal1b.round_up_6sig(0.0) == "0")
    value = 1.23456789e-5
    rounded = cal1b.round_up_6sig(value)
    check("round_up_value", Decimal(rounded) == Decimal("1.23457E-5"), rounded)
    check("round_up_not_below", Decimal(rounded) >= Decimal.from_float(value))
    tiny = cal1b.round_up_6sig(5.960464477539063e-8)
    check("round_tiny", tiny == "5.96047E-8", tiny)


def test_q(directory):
    truth = c2t.build_truth(directory)
    q = cal1b.compute_q(truth)
    track0 = q["tracks"][0]
    expected_p = math.sqrt((2.0 / 65535.0) ** 2 + 2.0 * (32.0 * c2.F32_EPS) ** 2)
    check("q_dynamic_p", math.isclose(track0["Q_p"], expected_p, rel_tol=0.0, abs_tol=1e-18),
          str(track0))
    expected_theta = 2.0 * math.asin(3.45436e-4 + 32.0 * c2.F32_EPS)
    check("q_rotation_theta", math.isclose(track0["Q_theta"], expected_theta,
                                           rel_tol=0.0, abs_tol=1e-15), str(track0))
    check("q_rotation_r", math.isclose(track0["Q_r"], 1.0 - math.cos(expected_theta / 2.0),
                                       rel_tol=0.0, abs_tol=1e-18))
    check("q_identity_position", math.isclose(q["tracks"][2]["Q_p"],
                                               math.sqrt(3.0) * 32.0 * c2.F32_EPS,
                                               rel_tol=0.0, abs_tol=1e-18),
          str(q["tracks"][2]))
    check("q_global_max", q["global"]["Q_p"] == max(t["Q_p"] for t in q["tracks"]))


def test_metrics():
    identity = [0, 0, 0, 0, 0, 0, bits(1.0), bits(1.0), bits(1.0), bits(1.0)]
    left = record_for({0: identity})
    right = record_for({0: identity})
    p, r, theta, s = cal1b._pose_metrics(left, right, 0)
    check("metric_identity", p == 0.0 and r == 0.0 and theta == 0.0 and s == 0.0)

    moved = list(identity)
    moved[0] = bits(0.5)
    p, r, theta, s = cal1b._pose_metrics(left, record_for({0: moved}), 0)
    check("metric_position", p == 0.5 and r == 0.0 and theta == 0.0 and s == 0.0)

    rotated = list(identity)
    rotated[3] = 0
    rotated[4] = 0
    rotated[5] = bits(math.sin(math.pi / 4.0))
    rotated[6] = bits(math.cos(math.pi / 4.0))
    p, r, theta, s = cal1b._pose_metrics(left, record_for({0: rotated}), 0)
    check("metric_rotation", math.isclose(theta, math.pi / 2.0, rel_tol=0.0, abs_tol=1e-7),
          str((r, theta)))
    check("metric_rotation_r", math.isclose(r, 1.0 - math.sqrt(0.5), rel_tol=0.0, abs_tol=1e-7))

    scaled = list(identity)
    scaled[7] = bits(1.25)
    p, r, theta, s = cal1b._pose_metrics(left, record_for({0: scaled}), 0)
    check("metric_scale", s == 0.25)


def test_n_and_tau(directory):
    truth, artifacts, _, _, _, _ = synthetic_inputs(directory, differ=True)
    parsed = [c2.parse_artifact(path) for path in artifacts]
    q = cal1b.compute_q(truth)
    n = cal1b.compute_n(parsed)
    tau = cal1b.compute_tau(q, n)
    check("n_sample_count", n["sample_count"] == 3 * 121 * 99)
    check("n_pair_count", len(n["pairs"]) == 3)
    check("n_position", math.isclose(n["global"]["N_p"], 0.5, rel_tol=0.0, abs_tol=1e-9))
    check("n_other_zero", n["global"]["N_r"] == 0.0 and n["global"]["N_theta"] == 0.0 and
          n["global"]["N_s"] == 0.0)
    check("tau_p_exceeds_n", float(tau["global"]["tau_p"]["rounded"]) >= 0.5)
    check("tau_r_positive", float(tau["global"]["tau_r"]["rounded"]) > 0.0)
    check("tau_tracks", len(tau["tracks"]) == 99)


def test_main_and_determinism(directory):
    truth, artifacts, logs, truth_path, c2_path, expected = synthetic_inputs(directory)
    old = cal1b.EXPECTED
    cal1b.EXPECTED = expected
    try:
        out1 = os.path.join(directory, "out1")
        out2 = os.path.join(directory, "out2")
        argv = []
        for artifact, log in zip(artifacts, logs):
            argv.extend(("--artifact", artifact, "--log", log))
        argv.extend(("--truth", truth_path, "--c2-json", c2_path))
        with contextlib.redirect_stdout(io.StringIO()) as stdout1:
            code1 = cal1b.main(argv + ["--out-dir", out1])
        with contextlib.redirect_stdout(io.StringIO()) as stdout2:
            code2 = cal1b.main(argv + ["--out-dir", out2])
        check("main_exit", code1 == 0 and code2 == 0, f"{code1}/{code2}")
        for name in ("cal1b-calibration.json", "cal1b-tracks.csv", "cal1b-summary.txt"):
            with open(os.path.join(out1, name), "rb") as left, \
                 open(os.path.join(out2, name), "rb") as right:
                check(f"deterministic_{name}", left.read() == right.read())
        with open(os.path.join(out1, "cal1b-calibration.json"), "r", encoding="utf-8") as handle:
            report = json.load(handle)
        check("main_verdict", report["verdict"] == "CALIBRATED_BOUNDED")
        check("main_scope", report["scope"] == "CAL1-B/control-B-single-layer")
        check("main_criteria", all(report["criteria"].values()))
        check("main_no_epsilon_value", "epsilon_t" not in report["tau"]["global"])
        check("main_excluded", any("epsilon_t" in item for item in report["excluded"]))
        summary_lines = stdout1.getvalue().splitlines()
        check("summary_budget", len(summary_lines) <= 200, str(len(summary_lines)))

        bad = dict(expected)
        bad["artifacts"] = list(expected["artifacts"])
        bad["artifacts"][0] = "0" * 64
        cal1b.EXPECTED = bad
        with contextlib.redirect_stderr(io.StringIO()):
            code_bad = cal1b.main(argv + ["--out-dir", os.path.join(directory, "out-bad")])
        check("main_rejects_hash", code_bad == 2, str(code_bad))
    finally:
        cal1b.EXPECTED = old


def main() -> int:
    test_rounding()
    with tempfile.TemporaryDirectory() as directory:
        test_q(directory)
        test_metrics()
    with tempfile.TemporaryDirectory() as directory:
        test_n_and_tau(directory)
    with tempfile.TemporaryDirectory() as directory:
        test_main_and_determinism(directory)
    if FAILURES:
        print(f"{len(FAILURES)} FAILURE(S): {', '.join(FAILURES)}")
        return 1
    print("ALL TESTS PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
