#!/usr/bin/env python3
"""Offline unit tests for tools/cal2_adjudicate.py — C-G5-CAL2 v1 §9.7.

All fixtures are synthetic and analytically controlled; no game artifact or
truth-grid value is used to derive an expected answer. Pure stdlib:

  python -I tools/cal2_adjudicate_test.py
"""

import math
import os
import struct
import sys
import tempfile
import contextlib
import io

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cal2_adjudicate as c  # noqa: E402

FAILURES = []


def check(name: str, condition: bool, detail: str = "") -> None:
    if condition:
        print(f"PASS {name}")
    else:
        FAILURES.append(name)
        print(f"FAIL {name} {detail}")


def f32bits(value: float) -> int:
    return struct.unpack("<I", struct.pack("<f", value))[0]


def identity_bits() -> list:
    return [0, 0, 0, 0, 0, 0, f32bits(1.0), f32bits(1.0), f32bits(1.0), f32bits(1.0)]


def build_truth_text():
    times = [f32bits(index / 60.0) for index in range(121)]
    lines = [
        "HKXTRUTH 1",
        "TRACKS count=99",
        "BOUNDS blocks=6",
    ]
    for ordinal in range(121):
        lines.append(f"TIME index={ordinal} bits={times[ordinal]:08X} value=0 block=0 "
                     f"local_time_bits={times[ordinal]:08X} local_frame=0")
        for track in range(99):
            values = identity_bits()
            if track == 0:
                values[0] = f32bits(0.5)
            elif track == 1:
                values[1] = f32bits(0.5)
            elif track == 2:
                values[2] = 0
            elif track == 3:
                values[3] = 0
            lines.append(
                f"TRACK time={ordinal} track={track} "
                f"t={values[0]:08X},{values[1]:08X},{values[2]:08X} "
                f"r={values[3]:08X},{values[4]:08X},{values[5]:08X},{values[6]:08X} "
                f"s={values[7]:08X},{values[8]:08X},{values[9]:08X}")
    lines.append("BITS_FNV1A64 0000000000000000")
    for block in range(6):
        for track in range(99):
            px = py = pz = "IDN"
            sx = sy = sz = "IDN"
            rot = "IDN"
            if track == 0:
                px = "DYN:00000000:3F800000" if block == 0 else "IDN"
            elif track == 1:
                py = "STA:3F000000"
            lines.append(
                f"BOUND block={block} track={track} pos_q=1 rot_q=3 scale_q=1 "
                f"px={px} py={py} pz={pz} rot={rot} sx={sx} sy={sy} sz={sz}")
    return "\n".join(lines) + "\n", times


def build_truth(directory: str):
    text, times = build_truth_text()
    path = os.path.join(directory, "truth.txt")
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(text)
    return c.parse_truth(path)


def make_artifact(truth, path: str, mutate=None, records=121,
                  two_read_bad=(), nonfinite=(), tsc_break=()):
    identity = c.REGISTERED_BUILD_IDENTITY.encode("ascii")
    header = c.GRID_HEADER.pack(b"CGRD", 1, c.GRID_HEADER_SIZE, 121, records, 99, 0)
    header += identity.ljust(92, b"\x00") + b"\x00\x00"
    chunks = [header]
    for ordinal in range(records):
        data = []
        for track in range(99):
            data.extend(truth["tracks"][ordinal][track])
        if mutate:
            data = mutate(ordinal, data)
        if ordinal in nonfinite:
            data[0] = 0x7FC00000
        completed = 1 if ordinal <= 1 else ordinal
        two_read = 0 if ordinal in two_read_bad else 1
        call = 1000 + ordinal * 10
        after_first = call + 2
        after_second = after_first + 3
        after_compare = after_second + 4
        if ordinal in tsc_break:
            after_compare = after_first - 1
        meta = c.GRID_RECORD_META.pack(
            ordinal, truth["times"][ordinal], completed, two_read, 1, 0, 0,
            call, after_first, after_second, after_compare)
        chunks.append(meta + struct.pack(f"<{c.GRID_DATA_WORDS}I", *data))
    with open(path, "wb") as handle:
        handle.write(b"".join(chunks))
    return path


def mutate_component(track: int, component: int, delta: float):
    index = track * c.GRID_COMPONENT_COUNT + component

    def apply(ordinal, data):
        value = struct.unpack("<f", struct.pack("<I", data[index]))[0]
        data[index] = f32bits(value + delta)
        return data

    return apply


def write_log(truth, artifact, path: str, unexpected_error: bool = False):
    manifest = (
        "[00:00:00.000] [1] [I] CONTROL GRID actor=00000014 request=1 phase=manifest "
        f"completed=1 resource='{c.REGISTERED_RESOURCE}' bytes={c.REGISTERED_RESOURCE_BYTES} "
        f"sha256={c.REGISTERED_RESOURCE_SHA256} duration_bits={c.REGISTERED_DURATION_BITS:08X} "
        f"tracks={c.REGISTERED_TRACKS} floats={c.REGISTERED_FLOATS} frames={c.REGISTERED_FRAMES} "
        f"blocks={c.REGISTERED_BLOCKS} data_fnv1a64={c.REGISTERED_DATA_FNV:016X} "
        f"block_offsets_fnv1a64={c.REGISTERED_BLOCK_OFFSETS_FNV:016X} "
        f"map_fnv1a64={c.REGISTERED_MAP_FNV:016X} times=121 baseline=1")
    carrier = (
        "[00:00:00.001] [1] [I] CONTROL GRID actor=00000014 request=1 phase=carrier completed=1 "
        "node=0x1 index=14 clip='Animations\\male\\MT_Idle.HKX' binding=0x2 binding_index=1022 "
        "animation=0x3 map_count=99 map_fnv1a64=077954859409395E blend_hint=0 map_match=true")
    lines = [
        "[00:00:00.000] [1] [I] SCENEGRAPH BUILD " + c.REGISTERED_BUILD_IDENTITY +
        " runtime=1-6-1170-0 backend=scene-graph",
        "[00:00:00.000] [1] [E] failed to get plugin info for OpenAnimationReplacer",
        "[00:00:00.000] [1] [I] EXPERIMENT setting IndependentGraph=inject",
        "[00:00:00.000] [1] [I] EXPERIMENT setting ControlSampleGrid=true",
        "[00:00:00.000] [1] [I] EXPERIMENT setting ControlSample=false",
        manifest,
        carrier,
    ]
    for ordinal in range(121):
        record = artifact["records"][ordinal]
        lines.append(
            f"[00:00:00.002] [1] [I] CONTROL GRID actor=00000014 request=1 phase=gen "
            f"ordinal={ordinal} completed={record['completed']} time_bits={record['time_bits']:08X} "
            f"two_read_equal={record['two_read_equal']} identity_ok=true tsc_first=1 tsc_second=1 "
            f"tsc_compare=1")
    lines.append(
        f"[00:00:00.003] [1] [I] CONTROL GRID actor=00000014 request=1 phase=close status=complete "
        f"generations=121 two_read_bad=0 identity_bad=0 nonfinite=0 "
        f"data_fnv1a64={artifact['data_fnv1a64']:016X} artifact='{artifact['path']}' bytes={artifact['size']}")
    lines.append(
        "[00:00:00.004] [1] [I] INDEPENDENT GRAPH actor=00000014 request=1 phase=drive-summary "
        "schema=2 equip-completed=120 unequip-completed=129")
    lines.append(
        "[00:00:00.005] [1] [I] INDEPENDENT GRAPH actor=00000014 request=1 phase=segment-end "
        "outcome=completed stop-reason=state-12-to-0 source-drift=none")
    if unexpected_error:
        lines.append("[00:00:00.006] [1] [E] unexpected instrument failure")
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("\n".join(lines) + "\n")
    return path


def run_without_log(artifact_path, truth):
    return c.adjudicate_run(1, artifact_path, truth)


def test_truth_parse_and_bounds(directory):
    truth = build_truth(directory)
    check("truth_grid_count", len(truth["times"]) == 121)
    check("truth_track_count", len(truth["tracks"]) == 121 and len(truth["tracks"][0]) == 99)
    check("truth_anchors", truth["times"][0] == 0 and truth["times"][1] == 0x3C888889 and
          truth["times"][60] == 0x3F800000 and truth["times"][120] == 0x40000000)
    dynamic = truth["bounds"][0][0]
    static = truth["bounds"][1][1]
    identity = truth["bounds"][2][2]
    rotation = truth["bounds"][3][3]
    check("bound_dynamic_kind", dynamic["kind"] == "dynamic-16", str(dynamic))
    check("bound_dynamic_value", math.isclose(dynamic["bound"], 2.0 / 65535.0, rel_tol=0.0, abs_tol=1e-12))
    check("bound_static_value", math.isclose(static["bound"], 32.0 * c.F32_EPS * 0.5, rel_tol=0.0, abs_tol=1e-15))
    check("bound_identity_value", math.isclose(identity["bound"], 32.0 * c.F32_EPS, rel_tol=0.0, abs_tol=1e-15))
    check("bound_rotation_value", rotation["bound"] == c.ROTATION_BOUND)


def test_artifact_roundtrip(directory, truth):
    path = make_artifact(truth, os.path.join(directory, "run.bin"))
    artifact = c.parse_artifact(path)
    check("artifact_magic", artifact["magic"] == "CGRD")
    check("artifact_header", artifact["recorded"] == 121 and artifact["track_count"] == 99 and
          artifact["capacity"] == 121)
    check("artifact_identity", artifact["identity"] == c.REGISTERED_BUILD_IDENTITY)
    check("artifact_fnv", artifact["data_fnv1a64"] == c.fnv1a64(
        b"".join(struct.pack(f"<{c.GRID_DATA_WORDS}I", *record["data"])
                  for record in artifact["records"])))


def test_c2_b_detection(directory, truth):
    bad_read = make_artifact(truth, os.path.join(directory, "bad_read.bin"), two_read_bad={5})
    result = run_without_log(bad_read, truth)
    check("c2_b_two_read_bad", result["c2_b"]["verdict"] == "FAIL" and
          any("two_read_equal" in issue for issue in result["c2_b"]["issues"]["sample"]))

    nonfinite = make_artifact(truth, os.path.join(directory, "nonfinite.bin"), nonfinite={7})
    result = run_without_log(nonfinite, truth)
    check("c2_b_nonfinite", result["c2_b"]["verdict"] == "FAIL" and
          any("non-finite" in issue for issue in result["c2_b"]["issues"]["sample"]))

    broken_tsc = make_artifact(truth, os.path.join(directory, "tsc.bin"), tsc_break={9})
    result = run_without_log(broken_tsc, truth)
    check("c2_f_nonmonotonic", result["c2_f"]["verdict"] == "FAIL" and
          any("non-monotonic" in issue for issue in result["c2_f"]["issues"]["sample"]))


def test_c2_c_bounds(directory, truth):
    within = make_artifact(truth, os.path.join(directory, "within.bin"),
                           mutate=mutate_component(0, 0, 2.0e-5))
    result = run_without_log(within, truth)
    check("c2_c_within_bound", result["c2_c"]["verdict"] == "PASS", str(result["c2_c"]["worst"][:1]))

    outside = make_artifact(truth, os.path.join(directory, "outside.bin"),
                            mutate=mutate_component(0, 0, 4.0e-5))
    result = run_without_log(outside, truth)
    check("c2_c_dynamic_violation", result["c2_c"]["verdict"] == "FAIL" and
          result["c2_c"]["violations_count"] > 0)

    rotation = make_artifact(truth, os.path.join(directory, "rotation.bin"),
                             mutate=mutate_component(3, 3, 4.0e-4))
    result = run_without_log(rotation, truth)
    check("c2_c_rotation_violation", result["c2_c"]["verdict"] == "FAIL" and
          result["c2_c"]["violations_count"] > 0)

    static = make_artifact(truth, os.path.join(directory, "static.bin"),
                           mutate=mutate_component(1, 1, 2.0e-6))
    result = run_without_log(static, truth)
    check("c2_c_static_violation", result["c2_c"]["verdict"] == "FAIL" and
          result["c2_c"]["violations_count"] > 0)


def test_cross_process(directory, truth):
    left = make_artifact(truth, os.path.join(directory, "left.bin"))
    right = make_artifact(truth, os.path.join(directory, "right.bin"),
                          mutate=mutate_component(4, 4, 0.25))
    left_result = run_without_log(left, truth)
    right_result = run_without_log(right, truth)
    out_dir = os.path.join(directory, "cross")
    os.makedirs(out_dir)
    result = c.cross_process([left_result, right_result], out_dir)
    check("cross_pair_count", len(result["pairs"]) == 1)
    pair = result["pairs"][0]
    check("cross_max_abs", math.isclose(pair["max_abs"], 0.25, rel_tol=0.0, abs_tol=1e-7))
    check("cross_nonzero", pair["nonzero"] == 121)
    expected_bytes = c.CROSS_HEADER_SIZE + 121 * 99 * 10 * 8
    check("cross_detail_size", os.path.getsize(result["detail"]["path"]) == expected_bytes)
    check("cross_not_ready_for_two", result["verdict"] == "NOT_READY")


def test_three_process_ready(directory, truth):
    artifacts = []
    logs = []
    for index in range(3):
        artifact_path = make_artifact(truth, os.path.join(directory, f"ready{index}.bin"))
        artifact = c.parse_artifact(artifact_path)
        artifacts.append(artifact_path)
        logs.append(write_log(truth, artifact, os.path.join(directory, f"ready{index}.log")))
    out_dir = os.path.join(directory, "ready-out")
    argv = ["--truth", os.path.join(directory, "truth.txt"), "--out-dir", out_dir,
            "--track2-oar-exemption"]
    for artifact_path, log_path in zip(artifacts, logs):
        argv.extend(("--artifact", artifact_path, "--log", log_path))
    output = io.StringIO()
    with contextlib.redirect_stdout(output):
        code = c.main(argv)
    summary = output.getvalue()
    check("three_process_exit", code == 0, f"code={code}")
    check("three_process_ready", "BATCH C2-D PASS" in summary and "VERDICT READY" in summary, summary)
    check("three_process_cross", "CROSS run1-run3" in summary and summary.count("CROSS run") == 3)
    detail = os.path.join(out_dir, "cal2-cross-process-diff.bin")
    expected = c.CROSS_HEADER_SIZE + 3 * 121 * 99 * 10 * 8
    check("three_process_detail", os.path.getsize(detail) == expected)


def test_log_reconciliation(directory, truth):
    artifact_path = make_artifact(truth, os.path.join(directory, "logged.bin"))
    artifact = c.parse_artifact(artifact_path)
    log_path = write_log(truth, artifact, os.path.join(directory, "good.log"))
    result = c.adjudicate_run(1, artifact_path, truth, log_path)
    check("log_c2_a", result["c2_a"]["verdict"] == "PASS", str(result["c2_a"]["issues"]))
    check("log_c2_b", result["c2_b"]["verdict"] == "PASS", str(result["c2_b"]["issues"]))
    check("log_c2_e_strict", result["c2_e"]["verdict"] == "FAIL" and
          any("OAR plugin-info" in issue for issue in result["c2_e"]["issues"]["sample"]))
    exempt = c.adjudicate_run(1, artifact_path, truth, log_path, allow_oar_exemption=True)
    check("log_c2_e_exempt", exempt["c2_e"]["verdict"] == "PASS", str(exempt["c2_e"]["issues"]))
    check("log_exempt_oar", exempt["c2_e"]["log"]["reconciliation"]["errors_exempt_oar"] == 1)

    bad_log = write_log(truth, artifact, os.path.join(directory, "bad.log"), unexpected_error=True)
    bad_result = c.adjudicate_run(1, artifact_path, truth, bad_log)
    check("log_unexpected_error", bad_result["c2_e"]["verdict"] == "FAIL" and
          any("unexpected [E]" in issue for issue in bad_result["c2_e"]["issues"]["sample"]))


def main() -> int:
    with tempfile.TemporaryDirectory() as directory:
        truth = build_truth(directory)
        test_truth_parse_and_bounds(directory)
        test_artifact_roundtrip(directory, truth)
        test_c2_b_detection(directory, truth)
        test_c2_c_bounds(directory, truth)
        test_cross_process(directory, truth)
        test_three_process_ready(directory, truth)
        test_log_reconciliation(directory, truth)
    if FAILURES:
        print(f"{len(FAILURES)} FAILURE(S): {', '.join(FAILURES)}")
        return 1
    print("ALL TESTS PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
