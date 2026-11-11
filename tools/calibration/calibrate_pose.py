"""C-G5-CAL v1 control A: two frozen pairs, three independent offline captures."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import platform
import re
import struct
import subprocess
import sys
from datetime import datetime, timezone
from decimal import Decimal, localcontext
from typing import Any


FIXTURE = "g5-cal-a-analytic-sign-v1"
VERSION = 1
REPEATS = 3
METRICS = ("position_l2", "rotation_error", "scale_linf")
CHANNELS = ("tx", "ty", "tz", "qx", "qy", "qz", "qw", "sx", "sy", "sz")
REFERENCE_BITS = "40480001 c0880001 41480000 00000000 00000000 3e8483ee 3f7746ea 3f400006 3f800003 3fa00003".split()
OBSERVED_BITS = "40480001 bfa00004 41840000 00000000 00000000 3f3504f3 3f3504f3 3f400006 3f800003 3fb00003".split()
NEGATED_BITS = "40480001 c0880001 41480000 80000000 80000000 be8483ee bf7746ea 3f400006 3f800003 3fa00003".split()
EXPECTED = (
    ("analytic", "reference", REFERENCE_BITS, ["7fc01234", "7f800000"]),
    ("analytic", "observed", OBSERVED_BITS, ["7fc05678", "ff800000"]),
    ("sign", "reference", REFERENCE_BITS, ["7fc09abc", "7f800000"]),
    ("sign", "observed", NEGATED_BITS, ["7fc0def0", "ff800000"]),
)
NUMBER = r"(-?\d+\.\d{6})"
TRACK_ROW = re.compile(
    r"SOURCE TIMELINE pose-snapshot track=(0|[1-9]\d*) trans="
    + ",".join([NUMBER] * 3)
    + " rot=" + ",".join([NUMBER] * 4)
    + " scale=" + ",".join([NUMBER] * 3)
)
SOURCES = (
    "src/animation/behavior_graph/pose_capture_policy.h",
    "src/animation/behavior_graph/pose_content_policy.h",
    "src/animation/behavior_graph/independent_graph.cpp",
    "src/animation/behavior_graph/source_timeline.cpp",
    "tests/pose_calibration_capture.cpp",
    "tools/calibration/calibrate_pose.py",
)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path: Path, value: Any) -> None:
    with path.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(value, stream, indent=2, allow_nan=False)
        stream.write("\n")


def float_from_bits(bits: str) -> float:
    return struct.unpack("!f", bytes.fromhex(bits))[0]


def decimal_metrics(a: list[Decimal], b: list[Decimal]) -> dict[str, Decimal]:
    """Independent 80-digit oracle: dot / sqrt(product of squared norms)."""
    position = sum((x - y) ** 2 for x, y in zip(a[:3], b[:3])).sqrt()
    qa, qb = a[3:7], b[3:7]
    dot = abs(sum(x * y for x, y in zip(qa, qb)))
    norm_product = sum(x * x for x in qa) * sum(y * y for y in qb)
    cosine = min(Decimal(1), dot / norm_product.sqrt())
    return dict(zip(METRICS, (
        position, Decimal(1) - cosine,
        max(abs(x - y) for x, y in zip(a[7:], b[7:])),
    )))


def double_metrics(a: list[float], b: list[float]) -> dict[str, float]:
    position = math.sqrt(sum((x - y) * (x - y) for x, y in zip(a[:3], b[:3])))
    qa, qb = a[3:7], b[3:7]
    na = math.sqrt(sum(x * x for x in qa))
    nb = math.sqrt(sum(x * x for x in qb))
    cosine = min(1.0, max(0.0, abs(sum((x / na) * (y / nb) for x, y in zip(qa, qb)))))
    return dict(zip(METRICS, (
        position, 1.0 - cosine,
        max(abs(x - y) for x, y in zip(a[7:], b[7:])),
    )))


def arithmetic_bounds(a: list[Decimal], b: list[Decimal], oracle: dict[str, Decimal],
                      text_input: bool) -> dict[str, Decimal]:
    u = Decimal(2) ** -53

    def gamma(n: int) -> Decimal:
        return n * u / (1 - n * u)

    input_position = (
        sum((abs(x) + abs(y)) ** 2 for x, y in zip(a[:3], b[:3])).sqrt() * u
        if text_input else Decimal(0)
    )
    input_scale = (
        max(abs(x) + abs(y) for x, y in zip(a[7:], b[7:])) * u
        if text_input else Decimal(0)
    )
    g10 = gamma(10)
    rotation = 2 * g10 + g10 * g10 + gamma(7) * (1 + g10) ** 2 + u
    return {
        "position_l2": input_position + gamma(12) * (oracle["position_l2"] + input_position),
        "rotation_error": rotation + (4 * u if text_input else 0),
        "scale_linf": input_scale + gamma(1) * (oracle["scale_linf"] + input_scale),
    }


def unique_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key {key!r}")
        result[key] = value
    return result


def reject_constant(value: str) -> None:
    raise ValueError(f"non-finite JSON constant {value}")


def parse_capture(data: bytes) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    lines = data.decode("utf-8").splitlines()
    if len(lines) != 6:
        raise ValueError(f"expected 6 complete JSONL rows, received {len(lines)}")
    records = [json.loads(line, object_pairs_hook=unique_object, parse_constant=reject_constant)
               for line in lines]
    header = records[0]
    fixed_header = {
        "type": "header", "schema": 1, "fixture": FIXTURE, "pairs": 2, "tracks": 4,
        "components": 40, "carrier_bytes": 48, "carrier_alignment": 16, "native": False,
    }
    compiler_keys = {"msvc_full_ver", "stl_version", "stl_update"}
    if not isinstance(header, dict) or set(header) != set(fixed_header) | compiler_keys:
        raise ValueError("missing or extra header fields")
    if any(type(header[k]) is not type(v) or header[k] != v for k, v in fixed_header.items()):
        raise ValueError("fixture identity, planned counts or offline carrier mismatch")
    if any(type(header[k]) is not int or header[k] <= 0 for k in compiler_keys):
        raise ValueError("invalid compiler/STL identity")
    end = {"type": "end", "pairs": 2, "tracks": 4, "components": 40}
    if not isinstance(records[-1], dict) or records[-1] != end or any(type(records[-1][k]) is not type(v) for k, v in end.items()):
        raise ValueError("missing or incomplete end record")
    parsed = []
    for record, (case_id, role, bits, padding) in zip(records[1:-1], EXPECTED):
        keys = {"type", "case", "role", "track", "raw_bits", "padding_bits", "row"}
        if not isinstance(record, dict) or set(record) != keys:
            raise ValueError("missing or extra track fields")
        if (record["type"], record["case"], record["role"], record["track"]) != ("track", case_id, role, 0):
            raise ValueError(f"wrong/duplicate track identity: {case_id}/{role}")
        if type(record["track"]) is not int or record["raw_bits"] != bits or record["padding_bits"] != padding:
            raise ValueError(f"independent raw bits or padding mismatch: {case_id}/{role}")
        if any(poison in record["raw_bits"] for poison in padding):
            raise ValueError(f"padding leaked into the ten captured channels: {case_id}/{role}")
        match = TRACK_ROW.fullmatch(record["row"]) if isinstance(record["row"], str) else None
        if match is None or int(match[1]) != 0:
            raise ValueError(f"incomplete six-decimal production track row: {case_id}/{role}")
        raw = [float_from_bits(value) for value in bits]
        text = [Decimal(value) for value in match.groups()[1:]]
        doubles = [float(value) for value in text]
        if not all(math.isfinite(value) for value in raw + doubles):
            raise ValueError(f"non-finite meaningful channel: {case_id}/{role}")
        if not any(raw[3:7]) or not any(doubles[3:7]):
            raise ValueError(f"zero quaternion: {case_id}/{role}")
        raw_decimal = [Decimal.from_float(value) for value in raw]
        component_error = [abs(x - y) for x, y in zip(raw_decimal, text)]
        if max(component_error) > Decimal("0.0000005"):
            raise ValueError(f"six-decimal nearest-rounding bound exceeded: {case_id}/{role}")
        parsed.append({
            "case": case_id, "role": role, "raw": raw, "raw_decimal": raw_decimal,
            "text": text, "doubles": doubles, "component_error": component_error,
            "row": record["row"], "raw_bits": bits,
        })
    return header, parsed


def analyze_pair(a: dict[str, Any], b: dict[str, Any]) -> dict[str, Any]:
    ideal = dict(zip(METRICS, (
        Decimal(5), 1 - Decimal(3).sqrt() / 2, Decimal("0.125")
    ))) if a["case"] == "analytic" else dict.fromkeys(METRICS, Decimal(0))
    raw_oracle = decimal_metrics(a["raw_decimal"], b["raw_decimal"])
    text_oracle = decimal_metrics(a["text"], b["text"])
    raw_double = double_metrics(a["raw"], b["raw"])
    text_double = double_metrics(a["doubles"], b["doubles"])
    raw_bounds = arithmetic_bounds(a["raw_decimal"], b["raw_decimal"], raw_oracle, False)
    text_bounds = arithmetic_bounds(a["text"], b["text"], text_oracle, True)
    norms = [sum(x * x for x in row["raw_decimal"][3:7]).sqrt() for row in (a, b)]
    representation_bounds = dict(zip(METRICS, (
        Decimal(3).sqrt() * Decimal("0.000001"),
        sum(2 * Decimal("0.000001") / norm for norm in norms),
        Decimal("0.000001"),
    )))
    values: dict[str, Any] = {}
    for metric in METRICS:
        raw_numeric = abs(Decimal.from_float(raw_double[metric]) - raw_oracle[metric])
        text_numeric = abs(Decimal.from_float(text_double[metric]) - text_oracle[metric])
        text_representation = text_oracle[metric] - raw_oracle[metric]
        values[metric] = {
            "ideal": str(ideal[metric]), "binary32_oracle": str(raw_oracle[metric]),
            "serialized_oracle": str(text_oracle[metric]),
            "ideal_to_binary32_delta": str(raw_oracle[metric] - ideal[metric]),
            "binary32_to_text_delta": str(text_representation),
            "raw_double": raw_double[metric], "serialized_double": text_double[metric],
            "raw_numeric_abs_error": str(raw_numeric), "text_numeric_abs_error": str(text_numeric),
            "raw_arithmetic_bound": str(raw_bounds[metric]),
            "text_arithmetic_bound": str(text_bounds[metric]),
            "text_representation_bound": str(representation_bounds[metric]),
            "pass": raw_numeric <= raw_bounds[metric] and text_numeric <= text_bounds[metric]
            and abs(text_representation) <= representation_bounds[metric],
        }
    return {"case": a["case"], "raw_quaternion_norms": [str(n) for n in norms], "metrics": values}


def summary(values: list[Decimal]) -> dict[str, Any]:
    ordered = sorted(values)
    return {
        "count": len(values), "min": str(ordered[0]),
        "median": str(ordered[math.ceil(len(values) * 0.5) - 1]),
        "p95": str(ordered[math.ceil(len(values) * 0.95) - 1]),
        "p99": str(ordered[math.ceil(len(values) * 0.99) - 1]), "max": str(ordered[-1]),
    }


def statistics(attempts: list[dict[str, Any]]) -> dict[str, Any]:
    groups = {f"round-{attempt['round']}": attempt["pairs"] for attempt in attempts}
    groups.update({
        case_id: [pair for attempt in attempts for pair in attempt["pairs"] if pair["case"] == case_id]
        for case_id in ("analytic", "sign")
    })
    groups["combined"] = [pair for attempt in attempts for pair in attempt["pairs"]]
    stages = ("serialized_double", "ideal_to_binary32_delta", "binary32_to_text_delta",
              "raw_numeric_abs_error", "text_numeric_abs_error")
    return {
        group: {metric: {stage: summary([abs(Decimal(str(pair["metrics"][metric][stage]))) for pair in pairs])
                         for stage in stages} for metric in METRICS}
        for group, pairs in groups.items()
    }


def make_plan(root: Path, capture: Path, reason: str | None, previous: list[Path]) -> dict[str, Any]:
    return {
        "schema": VERSION, "fixture": FIXTURE, "status": "FROZEN_BEFORE_CAPTURE",
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "control": "offline A only; no SDK/ABI, native runtime, time or G5 qualification",
        "batch_reason": reason or "first registered offline batch",
        "prior_batches": [{"path": p.name,
                           "state": "finalized" if (p / "manifest.json").is_file() else "incomplete-before-manifest",
                           "artifact_sha256": {f.name: sha256(f) for f in sorted(p.iterdir()) if f.is_file()}}
                          for p in previous],
        "analysis_command": [sys.executable, *sys.argv],
        "analysis_version": VERSION,
        "capture": {"path": str(capture), "sha256": sha256(capture)},
        "source_sha256": {path: sha256(root / path) for path in SOURCES},
        "runtime": {
            "python": sys.version, "executable": sys.executable, "os": platform.platform(),
            "machine": platform.machine(), "float_mant_dig": sys.float_info.mant_dig,
            "float_radix": sys.float_info.radix, "decimal_precision": 80,
        },
        "compiler_identity_source": "each retained header: _MSC_FULL_VER, _MSVC_STL_VERSION, _MSVC_STL_UPDATE",
        "planned": {"rounds": 3, "case_pairs_per_round": 2, "track_rows_per_round": 4,
                    "components_per_row": 10, "track_rows_total": 12, "components_total": 120},
        "authored": {
            "reference_translation_literals": ["3.1250003f", "-4.2500003f", "12.5000003f"],
            "reference_rotation_literals": ["0f", "0f", "0.25881904510252074f", "0.9659258262890683f"],
            "reference_scale_literals": ["0.75000036f", "1.00000036f", "1.25000036f"],
            "analytic_observed": "binary32 reference + translation(0,3,4), rotation(0,0,0.7071067811865475f,0.7071067811865475f), scale(0,0,0.125)",
            "sign_observed": "identical binary32 translation/scale; exact negation of all four reference quaternion lanes",
            "ideal_metric_source": "analytic 3-4-5 triangle; 60-degree relative rotation => 1-sqrt(3)/2; Z scale delta 1/8; q/-q same rotation",
            "expected_bits_source": "independently frozen IEEE-754 binary32 values in analyzer, before executable invocation; never generated from capture or double_metrics",
        },
        "expected_tracks": [{"case": c, "role": r, "track": 0, "channels": CHANNELS,
                             "raw_bits": b, "padding_bits": p} for c, r, b, p in EXPECTED],
        "numerics": {
            "oracle": "Decimal precision 80; independent dot/sqrt(norm2_a*norm2_b), analytic ideal constants",
            "rounding": "six-decimal nearest: <=5e-7 per component; parse-to-binary64 accounted separately",
            "position_Q": "sqrt(3)*1e-6 from two three-component rounding errors",
            "scale_Q": "1e-6 from two XYZ component rounding errors",
            "rotation_Q": "fixture norms only: per-side ||dq||<=sqrt(4)*5e-7=1e-6, normalized chord<=2*||dq||/||q||; abs-dot error<=sum of chords",
            "arithmetic": "binary64 u=2^-53, gamma(n)=n*u/(1-n*u); normal finite fixture values, round-to-nearest operations and sqrt. Position <=gamma(12)*norm plus Lipschitz input parse error (9 arithmetic operations). Scale <=gamma(1)*delta plus component input parse error. Quaternion normalization relative-vector bound gamma(10) (4 squares,3 adds,sqrt,division); dot <=gamma(7)*(1+gamma(10))^2; add 2*gamma(10)+gamma(10)^2 and u for 1-c. Text parse adds <=4u in rotation via normalized chords. Decimal oracle roundoff at 80 digits is negligible beside these binary64 bounds; sign exact-zero comparison is retained.",
            "statistics": "nearest-rank median/P95/P99; absolute discrepancies; no population guarantee",
            "tolerances": "tau_p/tau_r/tau_s/epsilon_t not produced; native B, K1-K6 and G5 remain NOT_READY",
        },
    }


def report_text(status: str, attempts: list[dict[str, Any]], stats: dict[str, Any],
                repetition: dict[str, Any], batch: str, failures: list[str]) -> str:
    lines = ["# G5 offline calibration A", "", f"Status: **{status}**, fixture `{FIXTURE}`.", "",
             "Only the two frozen pairs through shared typed extraction, six-decimal formatting and actual parsing are covered. The aligned 0x30 carrier substitutes for the native transform; it proves no SDK/ABI or native lifetime/completion property. K1-K6, native B, time calibration and G5 remain NOT_READY; no G5 tolerance is produced.", "",
             f"Batch: `{batch}`. [Frozen plan](plan.json), [manifest](manifest.json), [full metrics](metrics.json).", "",
             "| round | status | capture exit | track rows | components | raw stdout / stderr |",
             "| --- | --- | --- | --- | --- | --- |"]
    if failures:
        lines[6:6] = ["Batch failure: " + "; ".join(failures), ""]
    for attempt in attempts:
        n = attempt["round"]
        lines.append(f"| {n} | {attempt['status']} | {attempt['exit_code']} | {attempt.get('track_rows', 0)} | {attempt.get('components', 0)} | [stdout](round-{n}.stdout.jsonl) / [stderr](round-{n}.stderr.txt) |")
        if "error" in attempt:
            lines.extend(["", f"Round {n} failure: {attempt['error']}", ""])
    if stats:
        lines.extend(["", "| case | metric | ideal | raw oracle | text / double | ideal to binary32 | raw to text | numerical abs error |",
                      "| --- | --- | --- | --- | --- | --- | --- | --- |"])
        for pair in attempts[0]["pairs"]:
            for name, value in pair["metrics"].items():
                lines.append(f"| {pair['case']} | {name} | {Decimal(value['ideal']):.18g} | {Decimal(value['binary32_oracle']):.18g} | {value['serialized_double']:.18g} | {Decimal(value['ideal_to_binary32_delta']):.10g} | {Decimal(value['binary32_to_text_delta']):.10g} | {Decimal(value['text_numeric_abs_error']):.10g} |")
        lines.extend(["", "All three rounds and combined count/min/median/P95/P99/max are in metrics.json (nearest-rank). Scoped combined maxima:", ""])
        for metric in METRICS:
            group = stats["combined"][metric]
            lines.append(f"- {metric}: text representation `{group['binary32_to_text_delta']['max']}`; double numerical `{group['text_numeric_abs_error']['max']}`.")
        lines.extend(["", f"Repeated raw bits and formatted rows identical: `{repetition['raw_bits_identical']}` / `{repetition['formatted_rows_identical']}`. Per-case cross-round metric ranges: `{repetition['metric_ranges']}`.", "",
                      "The component Q limit is 5e-7 per side; pair position Q is sqrt(3)*1e-6 and scale Q is 1e-6. Rotation Q uses these fixture quaternion norms and normalized chord bounds; it is not a general quaternion bound. Arithmetic comparison limits come from binary64 u and operation counts in plan.json, independently of G5 thresholds.", "",
                      "Raw-vs-ideal rotation deviation is authored ideal-to-binary32 representation. Raw-to-text changes are formatting representation. Double-vs-Decimal residuals are numerical analysis/decimal parsing. Repeat ranges describe this fixed offline fixture only; none is native measurement noise or a population guarantee."])
    return "\n".join(lines) + "\n"


def run(args: argparse.Namespace) -> int:
    root = Path(__file__).resolve().parents[2]
    capture = args.capture.resolve(strict=True)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    previous = sorted(output.glob("attempt-*"))
    if previous and not args.reason:
        raise ValueError("existing batches retained; a subsequent --reason is required")
    batch = output / f"attempt-{len(previous) + 1:03d}"
    batch.mkdir()
    plan = make_plan(root, capture, args.reason, previous)
    write_json(batch / "plan.json", plan)
    attempts: list[dict[str, Any]] = []
    for n in range(1, REPEATS + 1):
        attempt: dict[str, Any] = {"round": n, "status": "INVALID", "exit_code": None}
        stdout, stderr = b"", b""
        try:
            process = subprocess.run([str(capture)], cwd=root, capture_output=True, timeout=30, check=False)
            stdout, stderr = process.stdout, process.stderr
            attempt["exit_code"] = process.returncode
        except subprocess.TimeoutExpired as error:
            stdout, stderr = error.stdout or b"", error.stderr or b""
            attempt["error"] = "capture timed out after 30 seconds"
        except OSError as error:
            attempt["error"] = f"capture could not start: {error}"
        attempt["stdout_bytes"] = len(stdout)
        attempt["stdout_lines"] = len(stdout.splitlines())
        (batch / f"round-{n}.stdout.jsonl").write_bytes(stdout)
        (batch / f"round-{n}.stderr.txt").write_bytes(stderr)
        if "error" not in attempt:
            try:
                if attempt["exit_code"] != 0 or stderr:
                    raise ValueError(f"capture exit={attempt['exit_code']}, stderr bytes={len(stderr)}")
                header, tracks = parse_capture(stdout)
                attempt.update({"compiler": header, "track_rows": 4, "components": 40})
                attempt["tracks"] = [{
                    "case": t["case"], "role": t["role"], "raw_bits": t["raw_bits"],
                    "raw_exact_decimal": [str(v) for v in t["raw_decimal"]],
                    "text_exact_decimal": [str(v) for v in t["text"]],
                    "component_text_abs_errors": [str(v) for v in t["component_error"]],
                    "component_binary64_parse_abs_errors": [str(abs(Decimal.from_float(x) - y))
                                                             for x, y in zip(t["doubles"], t["text"])],
                    "row": t["row"],
                } for t in tracks]
                attempt["pairs"] = [analyze_pair(tracks[i], tracks[i + 1]) for i in (0, 2)]
                if not all(v["pass"] for p in attempt["pairs"] for v in p["metrics"].values()):
                    raise ValueError("independent numerical/representation bound failed; see retained metrics")
                attempt["status"] = "PASS"
            except (ValueError, UnicodeError, TypeError) as error:
                attempt["error"] = str(error)
        attempts.append(attempt)
    valid = all(a["status"] == "PASS" for a in attempts)
    repetition: dict[str, Any] = {}
    stats: dict[str, Any] = {}
    if valid:
        stats = statistics(attempts)
        repetition = {
            "raw_bits_identical": all([t["raw_bits"] for t in a["tracks"]] == [t["raw_bits"] for t in attempts[0]["tracks"]] for a in attempts),
            "formatted_rows_identical": all([t["row"] for t in a["tracks"]] == [t["row"] for t in attempts[0]["tracks"]] for a in attempts),
            "compiler_identical": all(a["compiler"] == attempts[0]["compiler"] for a in attempts),
            "metric_ranges": {case_id: {metric: str(max(values) - min(values)) for metric in METRICS
                                       for values in [[Decimal(str(a["pairs"][case_index]["metrics"][metric]["serialized_double"])) for a in attempts]]}
                              for case_index, case_id in enumerate(("analytic", "sign"))},
        }
        valid = all(repetition[k] for k in ("raw_bits_identical", "formatted_rows_identical", "compiler_identical"))
    unchanged = plan["source_sha256"] == {p: sha256(root / p) for p in SOURCES} and plan["capture"]["sha256"] == sha256(capture)
    failures = [] if unchanged else ["source or executable changed during collection"]
    if repetition and not all(repetition[k] for k in ("raw_bits_identical", "formatted_rows_identical", "compiler_identical")):
        failures.append("cross-round raw/format/compiler identity changed")
    valid = valid and unchanged
    status = "CALIBRATED_OFFLINE_A_ONLY" if valid else "INVALID"
    counts = {"expected_rows": 12, "visited_rows": sum(a.get("track_rows", 0) for a in attempts),
              "recorded_rows": sum(a.get("track_rows", 0) for a in attempts if a["status"] == "PASS"),
              "expected_components": 120,
              "visited_components": sum(a.get("components", 0) for a in attempts),
              "recorded_components": sum(a.get("components", 0) for a in attempts if a["status"] == "PASS"),
              "invalid_rounds": sum(a["status"] != "PASS" for a in attempts),
              "dropped_rows": 0 if valid else None, "truncated_rows": 0 if valid else None}
    counts["missing_rows"] = counts["expected_rows"] - counts["recorded_rows"]
    counts["missing_components"] = counts["expected_components"] - counts["recorded_components"]
    write_json(batch / "metrics.json", {"status": status, "counts": counts, "attempts": attempts,
                                        "statistics": stats, "repetition": repetition, "failure_reasons": failures})
    (batch / "result.md").write_text(report_text(status, attempts, stats, repetition, batch.name, failures), encoding="utf-8", newline="\n")
    manifest = {
        "schema": VERSION, "fixture": FIXTURE, "status": status, "counts": counts,
        "plan_sha256": sha256(batch / "plan.json"), "source_sha256": plan["source_sha256"],
        "capture": plan["capture"], "runtime": plan["runtime"],
        "compiler": [a.get("compiler") for a in attempts], "analysis_command": plan["analysis_command"],
        "finished_utc": datetime.now(timezone.utc).isoformat(),
        "source_and_executable_unchanged": unchanged,
        "failure_reasons": failures,
        "artifact_sha256": {p.name: sha256(p) for p in sorted(batch.iterdir()) if p.is_file()},
        "scope": plan["control"], "batch_reason": plan["batch_reason"], "prior_batches": plan["prior_batches"],
    }
    write_json(batch / "manifest.json", manifest)
    if valid and not (output / "result.md").exists():
        (output / "result.md").write_text(
            f"# First complete G5 offline control A\n\nStatus: **{status}**.\n\n"
            f"[Report]({batch.name}/result.md), [frozen plan]({batch.name}/plan.json), "
            f"[manifest]({batch.name}/manifest.json), [metrics]({batch.name}/metrics.json).\n\n"
            f"Manifest SHA-256: `{sha256(batch / 'manifest.json')}`.\n\n"
            "K1-K6, native B, time calibration and G5 remain NOT_READY.\n",
            encoding="utf-8", newline="\n")
    print(f"{status}: {batch}; {counts['recorded_rows']}/12 rows, {counts['recorded_components']}/120 components")
    return 0 if valid else 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--reason", help="required explanation before a subsequent retained batch")
    args = parser.parse_args()
    if sys.version_info[:2] != (3, 12) or sys.float_info.mant_dig != 53 or sys.float_info.radix != 2:
        parser.error("this frozen analysis requires Python 3.12 with IEEE binary64 floats")
    try:
        with localcontext() as context:
            context.prec = 80
            return run(args)
    except (OSError, ValueError) as error:
        print(f"calibration failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
