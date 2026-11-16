#!/usr/bin/env python3
"""Offline tests for tools/gate4_clock.py — C-G5-G4-CLOCK v1."""

import contextlib
import io
import json
import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gate4_clock as c  # noqa: E402

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


def make_case(directory, rate=1.0, endpoint=0.0, dt=0.0, reverse=False):
    pairs = []
    for index in range(10):
        clock_s = index / 30.0
        clock_c = clock_s * rate + endpoint
        if reverse and index == 9:
            clock_c = 0.0
        pairs.append({
            "domain": "1hm_equip",
            "cand": {"record": index},
            "src": {"seq": index},
            "clock_c": clock_c,
            "clock_x": clock_s,
            "dt_clock": dt,
            "flags": ["edge-pair"] if index == 0 else [],
        })
    manifest = {"schema": "C-G5-G4 manifest v1", "pairs": pairs,
                "domains": {"1hm_equip": {"wrap_cycles": {"candidate": [], "source": []}}}}
    time_result = {"schema": "C-G5-TIME-v1-result", "verdict": "TIME_READY",
                   "epsilon_t": {"unrounded": 0.0333960}}
    mp, tp = os.path.join(directory, "m.json"), os.path.join(directory, "t.json")
    write_json(mp, manifest)
    write_json(tp, time_result)
    return mp, tp


def run_case(directory, name, **kwargs):
    mp, tp = make_case(directory, **kwargs)
    out = os.path.join(directory, name)
    stdout, stderr = io.StringIO(), io.StringIO()
    with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
        code = c.main(["--manifest", mp, "--time-result", tp, "--out-dir", out])
    return code, stdout.getvalue(), stderr.getvalue(), out


def test_valid_and_guards(directory):
    code, stdout, stderr, out = run_case(directory, "valid")
    check("valid_exit", code == 0, stderr)
    check("valid_verdict", "VERDICT CLOCK_READY" in stdout, stdout)
    with open(os.path.join(out, "gate4-clock.json"), encoding="utf-8") as handle:
        report = json.load(handle)
    check("valid_criteria", all(report["criteria"].values()))
    check("valid_staircase", report["domains"]["1hm_equip"]["candidate_adjacent_equal"] >= 0)

    code, _, _, _ = run_case(directory, "rate", rate=1.02)
    check("rate_guard", code == 2)
    code, _, _, _ = run_case(directory, "endpoint", endpoint=0.04)
    check("endpoint_guard", code == 2)
    code, _, _, _ = run_case(directory, "dt", dt=0.04)
    check("dt_guard", code == 2)
    code, _, _, _ = run_case(directory, "monotonic", reverse=True)
    check("monotonic_guard", code == 2)


def test_source_multiple_clocks(directory):
    pairs = []
    for index in range(4):
        pairs.append({"domain": "1hm_equip", "cand": {"record": index},
                      "src": {"seq": 7}, "clock_c": index / 30.0,
                      "clock_x": (index % 2) / 30.0, "dt_clock": 0.0, "flags": []})
    manifest = {"schema": "C-G5-G4 manifest v1", "pairs": pairs,
                "domains": {"1hm_equip": {"wrap_cycles": {"candidate": [], "source": []}}}}
    time_result = {"schema": "C-G5-TIME-v1-result", "verdict": "TIME_READY",
                   "epsilon_t": {"unrounded": 0.0333960}}
    mp, tp = os.path.join(directory, "multi-m.json"), os.path.join(directory, "multi-t.json")
    write_json(mp, manifest)
    write_json(tp, time_result)
    report = c.analyze_domain("1hm_equip", pairs, 0.0333960)
    check("source_multi_points_allowed", report["source_records"] == 1
          and report["source_unique_points"] == 2, str(report))


def main():
    with tempfile.TemporaryDirectory() as directory:
        test_valid_and_guards(directory)
    with tempfile.TemporaryDirectory() as directory:
        test_source_multiple_clocks(directory)
    if FAILURES:
        print(f"{len(FAILURES)} FAILURE(S): {', '.join(FAILURES)}")
        return 1
    print("ALL TESTS PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
