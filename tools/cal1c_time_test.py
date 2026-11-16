#!/usr/bin/env python3
"""Offline unit tests for tools/cal1c_time.py — C-G5-TIME v1.

  python -I tools/cal1c_time_test.py
"""

import contextlib
import io
import json
import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cal1c_time as t  # noqa: E402
import cal2_adjudicate_test as c2t  # noqa: E402

FAILURES = []


def check(name, condition, detail=""):
    if condition:
        print(f"PASS {name}")
    else:
        FAILURES.append(name)
        print(f"FAIL {name} {detail}")


def write_json(path, value):
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        json.dump(value, handle, indent=1)
        handle.write("\n")


def make_inputs(directory, invariant=1, frame=True, pairs=100, over=1):
    truth = c2t.build_truth(directory)
    artifacts = []
    for index in range(3):
        path = os.path.join(directory, f"run{index}.bin")
        c2t.make_artifact(truth, path)
        artifacts.append(path)
    tsc = os.path.join(directory, "tsc.json")
    write_json(tsc, {
        "schema": "C-G5-TIME-v1-tsc-evidence",
        "invariant_tsc": invariant,
        "qpc_frequency_hz": 10000000,
        "f_tsc_hz": 1000000000.0,
        "min_hz": 999999999.0,
        "max_hz": 1000000001.0,
        "monotonic": True,
    })
    frame_path = os.path.join(directory, "frame.json")
    clips = []
    if frame:
        clips.append({
            "family": "1hm_equip",
            "name": "Animations\\1HM_Equip.hkx",
            "frame_duration_bits": "3D088889",
            "frame_duration": 0.03333333507180214,
            "paired_in_track2": True,
        })
    write_json(frame_path, {"schema": "C-G5-TIME-v1-frame-table", "clips": clips})
    gate4 = os.path.join(directory, "gate4.json")
    pair_list = []
    for index in range(pairs):
        dt = 0.04 if index < over else 0.01
        pair_list.append({
            "domain": "1hm_equip",
            "dt_clock": dt,
            "flags": ["edge-pair"] if index < over else [],
        })
    write_json(gate4, {
        "schema": "C-G5-G4 manifest v1",
        "pairs": pair_list,
    })
    return artifacts, tsc, frame_path, gate4


def run_main(directory, inputs, out_name="out"):
    artifacts, tsc, frame_path, gate4 = inputs
    argv = []
    for path in artifacts:
        argv.extend(("--artifact", path))
    argv.extend(("--tsc", tsc, "--frame-table", frame_path,
                 "--gate4-manifest", gate4, "--out-dir", os.path.join(directory, out_name)))
    stdout = io.StringIO()
    stderr = io.StringIO()
    with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
        code = t.main(argv)
    return code, stdout.getvalue(), stderr.getvalue()


def test_valid_and_deterministic(directory):
    inputs = make_inputs(directory)
    code, stdout, stderr = run_main(directory, inputs, "out1")
    check("valid_exit", code == 0, stderr)
    check("valid_summary", "VERDICT TIME_READY" in stdout and "EPSILON_T" in stdout, stdout)
    with open(os.path.join(directory, "out1", "cal1c-time.json"), encoding="utf-8") as handle:
        report = json.load(handle)
    check("valid_verdict", report["verdict"] == "TIME_READY")
    check("valid_epsilon", float(report["epsilon_t"]["rounded"]) > 0.03)
    check("valid_excluded", report["dt_exclude"]["excluded_count"] == 1)
    code2, _, _ = run_main(directory, inputs, "out2")
    check("deterministic_exit", code2 == 0)
    for name in ("cal1c-time.json", "cal1c-time-summary.txt"):
        with open(os.path.join(directory, "out1", name), "rb") as left, \
             open(os.path.join(directory, "out2", name), "rb") as right:
            check(f"deterministic_{name}", left.read() == right.read())


def test_guards(directory):
    inputs = make_inputs(directory, frame=False)
    code, _, stderr = run_main(directory, inputs, "missing-frame")
    check("missing_frame", code == 2, stderr)

    inputs = make_inputs(directory, over=3)
    code, _, stderr = run_main(directory, inputs, "too-many-excluded")
    check("exclusion_limit", code == 2, stderr)

    inputs = make_inputs(directory, invariant=0)
    code, _, stderr = run_main(directory, inputs, "bad-tsc")
    check("invariant_guard", code == 2, stderr)


def main():
    with tempfile.TemporaryDirectory() as directory:
        test_valid_and_deterministic(directory)
    with tempfile.TemporaryDirectory() as directory:
        test_guards(directory)
    if FAILURES:
        print(f"{len(FAILURES)} FAILURE(S): {', '.join(FAILURES)}")
        return 1
    print("ALL TESTS PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
