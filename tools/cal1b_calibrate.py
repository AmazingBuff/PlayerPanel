#!/usr/bin/env python3
"""C-G5-CAL1-B v1 bounded control-B calibration.

Implements docs/diagnostics/g5-cal1b-20261115/README.md (reviewed 2026-11-15):
reads the three registered CGRD v1 artifacts, the 121-time truth grid with
BOUNDS, and the CAL2 three-process READY ledger; derives Q from the authored
parse bounds, N from all process pairs, and tau_p/tau_r/tau_s with the frozen
six-significant-digit upward rounding. It deliberately does not produce
epsilon_t, layer/weight/mixing semantics, or any G5 equivalence verdict.

Usage:
  python -I tools/cal1b_calibrate.py \
      --artifact <run1.bin> --artifact <run2.bin> --artifact <run3.bin> \
      --log <run1.log> --log <run2.log> --log <run3.log> \
      --truth <grid121.txt> --c2-json <cal2-adjudication.json> \
      --out-dir <dir> [--force]

Exit codes: 0 CALIBRATED_BOUNDED; 2 input/data INVALID; 3 parse/guard failure.
"""

import argparse
import json
import math
import os
import statistics
import struct
import sys
from decimal import Decimal, InvalidOperation, ROUND_CEILING, localcontext

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cal2_adjudicate as c2  # noqa: E402

PROTOCOL = "C-G5-CAL1-B v1 (docs/diagnostics/g5-cal1b-20261115/README.md)"
SCOPE = "CAL1-B/control-B-single-layer"

# Frozen input identities. A different batch requires a new pre-registration.
EXPECTED = {
    "artifacts": [
        "CB5587574E5BA5146788A4DBF0873D4C57C802BE4F34356111343D1F8E1D508B",
        "F57D6251DF699E70CC8C2B888716BA05814C53583EB5BEE1B6A5E110A5522221",
        "60B373DB93BA05C7528768A2F31C298085B6153C30CD86B6197BD85B36BF9008",
    ],
    "logs": [
        "F16061B8094E6B078A8BDD7F52DC11AE81F15CF4746A3A4FABE163200017DB2B",
        "7FD033D3BBFE2041FC66E70FD8AC6FF5ED714CC815898DDB26565330C2722230",
        "90675EE7189C7705D96869AE0BAF04D85D2DE5DCBB56C987567198B47FF8ED04",
    ],
    "truth": "DE26ECEB5679E655ADEFE4790A216F219E8F64F7E405492BA1B8A69484EDC387",
    "c2_json": "B670782A38D975303AFE4F8B3A166DEC7D510E55B9851C7008B6EB33FDBC54A7",
}

BUILTIN_TRUTH = os.path.normpath(os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "..", "docs", "diagnostics",
    "g5-cal2-20261115", "mt_idle_female_truth_grid121.txt"))
BUILTIN_C2 = os.path.normpath(os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "..", "docs", "diagnostics",
    "g5-cal2-20261115", "batch-3of3-adjudication", "cal2-adjudication.json"))

SUMMARY_LIMIT = 200


class CalibrationError(Exception):
    """Input/data failure (exit 2)."""


def sha256_file(path: str) -> str:
    return c2.sha256_file(path).upper()


def basename(path: str) -> str:
    return os.path.basename(path)


def _finite(value: float, what: str) -> float:
    if not math.isfinite(value):
        raise CalibrationError(f"non-finite {what}: {value!r}")
    return value


def _distribution(values):
    if not values:
        return {"count": 0}
    ordered = sorted(values)
    count = len(ordered)

    def nearest_rank(fraction):
        rank = max(1, int(math.ceil(fraction * count)))
        return ordered[rank - 1]

    return {
        "count": count,
        "min": ordered[0],
        "median": statistics.median(ordered),
        "p95": nearest_rank(0.95),
        "p99": nearest_rank(0.99),
        "max": ordered[-1],
    }


def round_up_6sig(value: float) -> str:
    """Six decimal significant digits, rounded upward; deterministic string."""
    if not math.isfinite(value) or value < 0:
        raise CalibrationError(f"cannot round {value!r}")
    if value == 0:
        return "0"
    with localcontext() as context:
        context.prec = 80
        try:
            decimal_value = Decimal.from_float(value)
            exponent = decimal_value.adjusted() - 5
            quantum = Decimal(1).scaleb(exponent)
            rounded = decimal_value.quantize(quantum, rounding=ROUND_CEILING)
        except InvalidOperation as error:
            raise CalibrationError(f"rounding failure for {value!r}: {error}") from error
    return str(rounded)


def load_inputs(artifact_paths, log_paths, truth_path, c2_json_path):
    if len(artifact_paths) != 3:
        raise CalibrationError("exactly three --artifact inputs are required")
    if len(log_paths) != 3:
        raise CalibrationError("exactly three --log inputs are required")
    if not os.path.isfile(truth_path):
        raise CalibrationError(f"missing truth: {truth_path}")
    if not os.path.isfile(c2_json_path):
        raise CalibrationError(f"missing C2 ledger: {c2_json_path}")

    truth = c2.parse_truth(truth_path)
    truth_sha = sha256_file(truth_path)
    if truth_sha != EXPECTED["truth"]:
        raise CalibrationError(f"truth SHA-256 {truth_sha} != {EXPECTED['truth']}")

    c2_sha = sha256_file(c2_json_path)
    if c2_sha != EXPECTED["c2_json"]:
        raise CalibrationError(f"C2 ledger SHA-256 {c2_sha} != {EXPECTED['c2_json']}")
    with open(c2_json_path, "r", encoding="utf-8") as handle:
        c2_ledger = json.load(handle)
    if c2_ledger.get("schema") != "C-G5-CAL2-v1-adjudication":
        raise CalibrationError("C2 ledger schema mismatch")
    if c2_ledger.get("batch", {}).get("verdict") != "READY":
        raise CalibrationError("C2 ledger batch verdict is not READY")
    if c2_ledger.get("batch", {}).get("c2_d", {}).get("verdict") != "PASS":
        raise CalibrationError("C2 ledger C2-D is not PASS")
    runs = c2_ledger.get("runs", [])
    if len(runs) != 3:
        raise CalibrationError(f"C2 ledger runs={len(runs)}, expected 3")
    if c2_ledger.get("truth", {}).get("sha256", "").upper() != truth_sha:
        raise CalibrationError("C2 ledger truth hash differs from the supplied truth")

    artifacts = []
    logs = []
    for index, (artifact_path, log_path) in enumerate(zip(artifact_paths, log_paths)):
        if not os.path.isfile(artifact_path):
            raise CalibrationError(f"missing run {index + 1} artifact: {artifact_path}")
        if not os.path.isfile(log_path):
            raise CalibrationError(f"missing run {index + 1} log: {log_path}")
        artifact = c2.parse_artifact(artifact_path)
        artifact_sha = artifact["sha256"].upper()
        if artifact_sha != EXPECTED["artifacts"][index]:
            raise CalibrationError(
                f"run {index + 1} artifact SHA-256 {artifact_sha} != {EXPECTED['artifacts'][index]}")
        if artifact["recorded"] != c2.GRID_GENERATION_COUNT:
            raise CalibrationError(f"run {index + 1} recorded={artifact['recorded']}")
        if artifact["track_count"] != c2.REGISTERED_TRACKS:
            raise CalibrationError(f"run {index + 1} track_count={artifact['track_count']}")
        if artifact["identity"] != c2.REGISTERED_BUILD_IDENTITY:
            raise CalibrationError(f"run {index + 1} identity={artifact['identity']!r}")

        log_sha = sha256_file(log_path)
        if log_sha != EXPECTED["logs"][index]:
            raise CalibrationError(
                f"run {index + 1} log SHA-256 {log_sha} != {EXPECTED['logs'][index]}")

        ledger_run = runs[index]
        if ledger_run.get("artifact", {}).get("sha256", "").upper() != artifact_sha:
            raise CalibrationError(f"run {index + 1} artifact hash differs from the C2 ledger")
        if ledger_run.get("c2_e", {}).get("log", {}).get("sha256", "").upper() != log_sha:
            raise CalibrationError(f"run {index + 1} log hash differs from the C2 ledger")
        for criterion in ("c2_a", "c2_b", "c2_c", "c2_e", "c2_f"):
            if ledger_run.get(criterion, {}).get("verdict") != "PASS":
                raise CalibrationError(f"run {index + 1} {criterion} is not PASS")

        artifacts.append(artifact)
        logs.append({"basename": basename(log_path), "sha256": log_sha})

    return {
        "truth": truth,
        "artifacts": artifacts,
        "logs": logs,
        "c2_json": {"basename": basename(c2_json_path), "sha256": c2_sha},
    }


def compute_q(truth):
    tracks = []
    for track in range(c2.REGISTERED_TRACKS):
        bounds = [truth["bounds"][track][component]["bound"]
                  for component in range(c2.GRID_COMPONENT_COUNT)]
        if any(not math.isfinite(value) or value < 0 for value in bounds):
            raise CalibrationError(f"track {track}: invalid BOUNDS bound")
        q_p = math.sqrt(bounds[0] ** 2 + bounds[1] ** 2 + bounds[2] ** 2)
        q_s = max(bounds[7], bounds[8], bounds[9])
        q_rot = max(bounds[3:7])
        q_theta = 2.0 * math.asin(min(1.0, q_rot + 32.0 * c2.F32_EPS))
        q_r = 1.0 - math.cos(q_theta / 2.0)
        tracks.append({
            "track": track,
            "Q_p": _finite(q_p, "Q_p"),
            "Q_s": _finite(q_s, "Q_s"),
            "Q_theta": _finite(q_theta, "Q_theta"),
            "Q_r": _finite(q_r, "Q_r"),
        })
    global_values = {
        key: max(track[key] for track in tracks)
        for key in ("Q_p", "Q_s", "Q_theta", "Q_r")
    }
    distributions = {
        key: _distribution([track[key] for track in tracks])
        for key in ("Q_p", "Q_s", "Q_theta", "Q_r")
    }
    return {"global": global_values, "tracks": tracks, "distributions": distributions}


def _quat_metric(a_values, b_values):
    norm_a = math.sqrt(sum(value * value for value in a_values))
    norm_b = math.sqrt(sum(value * value for value in b_values))
    if not math.isfinite(norm_a) or not math.isfinite(norm_b) or norm_a <= 0 or norm_b <= 0:
        raise CalibrationError("zero/non-finite quaternion norm")
    dot = sum((a / norm_a) * (b / norm_b) for a, b in zip(a_values, b_values))
    c = min(1.0, max(0.0, abs(dot)))
    return 1.0 - c, 2.0 * math.acos(c)


def _pose_metrics(left_record, right_record, track):
    left = left_record["data"]
    right = right_record["data"]
    base = track * c2.GRID_COMPONENT_COUNT
    left_t = [c2.f32_from_bits(left[base + i]) for i in range(3)]
    right_t = [c2.f32_from_bits(right[base + i]) for i in range(3)]
    left_r = [c2.f32_from_bits(left[base + 3 + i]) for i in range(4)]
    right_r = [c2.f32_from_bits(right[base + 3 + i]) for i in range(4)]
    left_s = [c2.f32_from_bits(left[base + 7 + i]) for i in range(3)]
    right_s = [c2.f32_from_bits(right[base + 7 + i]) for i in range(3)]
    for values, what in ((left_t, "left translation"), (right_t, "right translation"),
                         (left_r, "left rotation"), (right_r, "right rotation"),
                         (left_s, "left scale"), (right_s, "right scale")):
        if any(not math.isfinite(value) for value in values):
            raise CalibrationError(f"track {track}: non-finite {what}")
    p = math.sqrt(sum((a - b) ** 2 for a, b in zip(left_t, right_t)))
    r, theta = _quat_metric(left_r, right_r)
    s = max(abs(a - b) for a, b in zip(left_s, right_s))
    return _finite(p, "E_p"), _finite(r, "E_r"), _finite(theta, "E_theta"), _finite(s, "E_s")


def compute_n(artifacts):
    pairs = [(0, 1), (0, 2), (1, 2)]
    sample_count = 0
    track_values = [{"track": track, "N_p": 0.0, "N_r": 0.0, "N_theta": 0.0, "N_s": 0.0}
                    for track in range(c2.REGISTERED_TRACKS)]
    pair_results = []
    for left_index, right_index in pairs:
        left = artifacts[left_index]
        right = artifacts[right_index]
        values = {"N_p": [], "N_r": [], "N_theta": [], "N_s": []}
        for ordinal in range(c2.GRID_GENERATION_COUNT):
            left_record = left["records"][ordinal]
            right_record = right["records"][ordinal]
            if left_record["time_bits"] != right_record["time_bits"]:
                raise CalibrationError(f"pair {left_index + 1}-{right_index + 1}: time grid mismatch")
            for track in range(c2.REGISTERED_TRACKS):
                p, r, theta, s = _pose_metrics(left_record, right_record, track)
                values["N_p"].append(p)
                values["N_r"].append(r)
                values["N_theta"].append(theta)
                values["N_s"].append(s)
                sample_count += 1
                for key, value in (("N_p", p), ("N_r", r), ("N_theta", theta), ("N_s", s)):
                    if value > track_values[track][key]:
                        track_values[track][key] = value
        pair_results.append({
            "left": f"run{left_index + 1}",
            "right": f"run{right_index + 1}",
            "count": len(values["N_p"]),
            "max": {key: max(values[key]) for key in values},
            "distributions": {key: _distribution(values[key]) for key in values},
        })
    expected_samples = 3 * c2.GRID_GENERATION_COUNT * c2.REGISTERED_TRACKS
    if sample_count != expected_samples:
        raise CalibrationError(f"N samples={sample_count}, expected {expected_samples}")
    global_values = {
        key: max(track[key] for track in track_values)
        for key in ("N_p", "N_r", "N_theta", "N_s")
    }
    return {
        "global": global_values,
        "tracks": track_values,
        "pairs": pair_results,
        "sample_count": sample_count,
    }


def _tau_entry(q_value, n_value):
    unrounded = _finite(q_value + n_value, "tau")
    return {"unrounded": unrounded, "rounded": round_up_6sig(unrounded)}


def compute_tau(q, n):
    global_tau = {
        "tau_p": _tau_entry(q["global"]["Q_p"], n["global"]["N_p"]),
        "tau_s": _tau_entry(q["global"]["Q_s"], n["global"]["N_s"]),
    }
    tau_theta = min(math.pi, q["global"]["Q_theta"] + n["global"]["N_theta"])
    global_tau["tau_theta"] = {"unrounded": tau_theta}
    global_tau["tau_r"] = {
        "unrounded": _finite(1.0 - math.cos(tau_theta / 2.0), "tau_r"),
    }
    global_tau["tau_r"]["rounded"] = round_up_6sig(global_tau["tau_r"]["unrounded"])

    tracks = []
    for q_track, n_track in zip(q["tracks"], n["tracks"]):
        entry = {
            "track": q_track["track"],
            "tau_p": _tau_entry(q_track["Q_p"], n_track["N_p"]),
            "tau_s": _tau_entry(q_track["Q_s"], n_track["N_s"]),
        }
        theta = min(math.pi, q_track["Q_theta"] + n_track["N_theta"])
        entry["tau_theta"] = {"unrounded": theta}
        entry["tau_r"] = {"unrounded": _finite(1.0 - math.cos(theta / 2.0), "tau_r")}
        entry["tau_r"]["rounded"] = round_up_6sig(entry["tau_r"]["unrounded"])
        tracks.append(entry)
    return {"global": global_tau, "tracks": tracks}


def _fmt(value):
    return f"{value:.12g}"


def render_summary(report):
    lines = [f"{report['protocol']}", f"VERDICT {report['verdict']}", f"SCOPE {report['scope']}"]
    for index, run in enumerate(report["inputs"]["runs"]):
        lines.append(f"INPUT run{index + 1} {run['basename']} sha256={run['sha256']}")
    lines.append(f"INPUT truth {report['inputs']['truth']['basename']} "
                 f"sha256={report['inputs']['truth']['sha256']}")
    lines.append(f"INPUT c2 {report['inputs']['c2_json']['basename']} "
                 f"sha256={report['inputs']['c2_json']['sha256']}")
    q = report["q"]["global"]
    n = report["n"]["global"]
    tau = report["tau"]["global"]
    lines.append(f"Q global Q_p={_fmt(q['Q_p'])} Q_r={_fmt(q['Q_r'])} "
                 f"Q_theta={_fmt(q['Q_theta'])} Q_s={_fmt(q['Q_s'])}")
    lines.append(f"N global N_p={_fmt(n['N_p'])} N_r={_fmt(n['N_r'])} "
                 f"N_theta={_fmt(n['N_theta'])} N_s={_fmt(n['N_s'])} "
                 f"samples={report['n']['sample_count']}")
    lines.append(f"TAU global tau_p={tau['tau_p']['rounded']} "
                 f"(unrounded {_fmt(tau['tau_p']['unrounded'])}) "
                 f"tau_r={tau['tau_r']['rounded']} "
                 f"(unrounded {_fmt(tau['tau_r']['unrounded'])}) "
                 f"tau_s={tau['tau_s']['rounded']} "
                 f"(unrounded {_fmt(tau['tau_s']['unrounded'])}) "
                 f"tau_theta={_fmt(tau['tau_theta']['unrounded'])}")
    track_tau = report["tau"]["tracks"]
    lines.append(f"TAU tracks count={len(track_tau)} "
                 f"tau_p=[{min((t['tau_p']['rounded'] for t in track_tau), key=lambda x: Decimal(x))}.."
                 f"{max((t['tau_p']['rounded'] for t in track_tau), key=lambda x: Decimal(x))}] "
                 f"tau_r=[{min((t['tau_r']['rounded'] for t in track_tau), key=lambda x: Decimal(x))}.."
                 f"{max((t['tau_r']['rounded'] for t in track_tau), key=lambda x: Decimal(x))}] "
                 f"tau_s=[{min((t['tau_s']['rounded'] for t in track_tau), key=lambda x: Decimal(x))}.."
                 f"{max((t['tau_s']['rounded'] for t in track_tau), key=lambda x: Decimal(x))}]")
    for name, status in report["criteria"].items():
        lines.append(f"CRITERION {name} {status}")
    lines.append("EXCLUDED epsilon_t=NOT_PRODUCED; K5-5=layer/weight/mixing; G5=not adjudicated")
    if len(lines) > SUMMARY_LIMIT:
        raise CalibrationError(f"summary {len(lines)} lines exceeds {SUMMARY_LIMIT}")
    return "\n".join(lines) + "\n"


def build_report(inputs, q, n, tau):
    return {
        "schema": "C-G5-CAL1-B-v1",
        "protocol": PROTOCOL,
        "scope": SCOPE,
        "verdict": "CALIBRATED_BOUNDED",
        "inputs": {
            "runs": [{"basename": basename(artifact["path"]), "sha256": artifact["sha256"].upper()}
                     for artifact in inputs["artifacts"]],
            "logs": inputs["logs"],
            "truth": {"basename": basename(inputs["truth"]["path"]),
                      "sha256": inputs["truth"]["sha256"].upper()},
            "c2_json": inputs["c2_json"],
        },
        "criteria": {name: "PASS" for name in ("C1B-A", "C1B-B", "C1B-C", "C1B-D", "C1B-E", "C1B-F")},
        "q": q,
        "n": n,
        "tau": tau,
        "excluded": ["epsilon_t: NOT_PRODUCED", "K5-5: layer/weight/mixing",
                     "K2-C1: complete equipment list", "G5: not adjudicated"],
        "tool_sha256": sha256_file(__file__),
        "cal2_tool_sha256": sha256_file(os.path.join(os.path.dirname(__file__), "cal2_adjudicate.py")),
    }


def write_tracks_csv(path: str, q, n, tau):
    header = ["track", "Q_p", "Q_s", "Q_theta", "Q_r", "N_p", "N_r", "N_theta", "N_s",
              "tau_p_unrounded", "tau_p_rounded", "tau_r_unrounded", "tau_r_rounded",
              "tau_s_unrounded", "tau_s_rounded"]
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(",".join(header) + "\n")
        for q_track, n_track, t_track in zip(q["tracks"], n["tracks"], tau["tracks"]):
            row = [
                str(q_track["track"]), _fmt(q_track["Q_p"]), _fmt(q_track["Q_s"]),
                _fmt(q_track["Q_theta"]), _fmt(q_track["Q_r"]),
                _fmt(n_track["N_p"]), _fmt(n_track["N_r"]), _fmt(n_track["N_theta"]),
                _fmt(n_track["N_s"]),
                _fmt(t_track["tau_p"]["unrounded"]), t_track["tau_p"]["rounded"],
                _fmt(t_track["tau_r"]["unrounded"]), t_track["tau_r"]["rounded"],
                _fmt(t_track["tau_s"]["unrounded"]), t_track["tau_s"]["rounded"],
            ]
            handle.write(",".join(row) + "\n")


def ensure_output_dir(path: str, force: bool):
    if os.path.exists(path) and not os.path.isdir(path):
        raise CalibrationError(f"--out-dir is not a directory: {path}")
    if os.path.isdir(path) and os.listdir(path) and not force:
        raise CalibrationError(f"--out-dir is not empty (pass --force): {path}")
    os.makedirs(path, exist_ok=True)


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--artifact", action="append", required=True,
                        help="CGRD v1 artifact (run 1/2/3, in order)")
    parser.add_argument("--log", action="append", required=True,
                        help="CharacterPanel.log for run 1/2/3, in order")
    parser.add_argument("--truth", default=BUILTIN_TRUTH, help="121-time truth grid + BOUNDS")
    parser.add_argument("--c2-json", default=BUILTIN_C2, help="CAL2 three-process adjudication JSON")
    parser.add_argument("--out-dir", required=True)
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args(argv)

    try:
        inputs = load_inputs(args.artifact, args.log, args.truth, args.c2_json)
        q = compute_q(inputs["truth"])
        n = compute_n(inputs["artifacts"])
        tau = compute_tau(q, n)
        report = build_report(inputs, q, n, tau)
        ensure_output_dir(args.out_dir, args.force)
        summary = render_summary(report)
        json_path = os.path.join(args.out_dir, "cal1b-calibration.json")
        csv_path = os.path.join(args.out_dir, "cal1b-tracks.csv")
        summary_path = os.path.join(args.out_dir, "cal1b-summary.txt")
        with open(json_path, "w", encoding="utf-8", newline="\n") as handle:
            json.dump(report, handle, indent=1, sort_keys=True, ensure_ascii=False)
            handle.write("\n")
        write_tracks_csv(csv_path, q, n, tau)
        with open(summary_path, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(summary)
        sys.stdout.write(summary)
        return 0
    except CalibrationError as error:
        print(f"[INVALID] {error}", file=sys.stderr)
        return 2
    except (OSError, ValueError, KeyError, struct.error) as error:
        print(f"[GUARD FAIL] {error}", file=sys.stderr)
        return 3


if __name__ == "__main__":
    sys.exit(main())
