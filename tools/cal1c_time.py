#!/usr/bin/env python3
"""C-G5-TIME v1: gate-4 time model / epsilon_t evaluation.

Implements docs/diagnostics/g5-time-20261115/README.md after the 2026-11-16
approval: T1 machine-level invariant-TSC evidence, a frameDuration table for
all paired gate-4 clips, epsilon_read from the three archived CGRD artifacts,
and epsilon_t = round_up_6sig(epsilon_read + epsilon_grid). It also applies the
frozen 2% / edge-only dt_exclude discipline to the metadata-only dry-run.

Usage:
  python -I tools/cal1c_time.py \
      --artifact <run1.bin> --artifact <run2.bin> --artifact <run3.bin> \
      --tsc <tsc_evidence.json> --frame-table <frame_table.json> \
      --gate4-manifest <gate4-pair-manifest.json> --out-dir <dir>
"""

import argparse
import json
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cal1b_calibrate as cal1b  # noqa: E402
import cal2_adjudicate as c2  # noqa: E402

PROTOCOL = "C-G5-TIME v1 (docs/diagnostics/g5-time-20261115/README.md)"
SUMMARY_LIMIT = 200
EXCLUDE_LIMIT = 0.02


class TimeError(Exception):
    """Data/guard failure (exit 2)."""


def sha256(path):
    return c2.sha256_file(path).upper()


def basename(path):
    return os.path.basename(path)


def load_tsc(path):
    with open(path, "r", encoding="utf-8") as handle:
        data = json.load(handle)
    if data.get("schema") != "C-G5-TIME-v1-tsc-evidence":
        raise TimeError("TSC evidence schema mismatch")
    if data.get("invariant_tsc") != 1 or data.get("monotonic") is not True:
        raise TimeError("TSC evidence is not invariant/monotonic")
    frequency = float(data.get("f_tsc_hz", 0.0))
    if not math.isfinite(frequency) or frequency <= 0:
        raise TimeError("invalid f_tsc_hz")
    return data, frequency


def load_frame_table(path):
    with open(path, "r", encoding="utf-8") as handle:
        data = json.load(handle)
    if data.get("schema") != "C-G5-TIME-v1-frame-table":
        raise TimeError("frame-table schema mismatch")
    clips = data.get("clips", [])
    if not clips:
        raise TimeError("empty frame table")
    by_family = {}
    for clip in clips:
        family = clip.get("family")
        if family in by_family:
            raise TimeError(f"duplicate frame family {family!r}")
        by_family[family] = clip
    return data, by_family


def load_gate4(path):
    with open(path, "r", encoding="utf-8") as handle:
        data = json.load(handle)
    if data.get("schema") != "C-G5-G4 manifest v1":
        raise TimeError("gate4 manifest schema mismatch")
    pairs = data.get("pairs", [])
    if not pairs:
        raise TimeError("gate4 manifest has no pairs")
    families = set()
    for pair in pairs:
        # The paired clock family is the pair's domain; cand_families/src_families
        # may also list coexisting idle layers (e.g. dummy) that are not the
        # quantization domain of the pair.
        families.add(pair.get("domain"))
    return data, families


def compute_epsilon_read(artifacts, frequency):
    max_cycles = 0
    for index, artifact in enumerate(artifacts):
        for record in artifact["records"]:
            cycles = record["tsc_after_compare"] - record["tsc_call_start"]
            if cycles < 0:
                raise TimeError(f"run {index + 1}: negative read interval")
            max_cycles = max(max_cycles, cycles)
    seconds = max_cycles / frequency
    epsilon_read = seconds + 1.0 / frequency + 1e-6 * seconds
    return {
        "max_read_cycles": max_cycles,
        "max_read_seconds": seconds,
        "unrounded": epsilon_read,
        "rounded": cal1b.round_up_6sig(epsilon_read),
    }


def compute_epsilon_grid(families, by_family):
    selected = []
    missing = []
    for family in sorted(families):
        clip = by_family.get(family)
        if clip is None or clip.get("frame_duration") is None:
            missing.append(family)
            continue
        value = float(clip["frame_duration"])
        if not math.isfinite(value) or value <= 0:
            raise TimeError(f"invalid frameDuration for {family}")
        selected.append({"family": family, "frame_duration": value,
                         "frame_duration_bits": clip.get("frame_duration_bits"),
                         "name": clip.get("name")})
    if missing:
        raise TimeError(f"paired families missing frameDuration: {missing}")
    if not selected:
        raise TimeError("no paired families with frameDuration")
    return {
        "clips": selected,
        "unrounded": max(clip["frame_duration"] for clip in selected),
        "rule": "max authored frameDuration over paired clips",
    }


def compute_epsilon_t(epsilon_read, epsilon_grid):
    unrounded = epsilon_read["unrounded"] + epsilon_grid["unrounded"]
    return {
        "unrounded": unrounded,
        "rounded": cal1b.round_up_6sig(unrounded),
        "epsilon_read": epsilon_read,
        "epsilon_grid": epsilon_grid,
    }


def apply_dt_exclude(pairs, epsilon_t):
    excluded = []
    for index, pair in enumerate(pairs):
        dt = float(pair.get("dt_clock", 0.0))
        if not math.isfinite(dt):
            raise TimeError(f"pair {index}: non-finite dt_clock")
        if dt > epsilon_t:
            excluded.append({
                "pair_index": index,
                "domain": pair.get("domain"),
                "dt_clock": dt,
                "flags": pair.get("flags", []),
            })
    ratio = len(excluded) / len(pairs)
    if ratio > EXCLUDE_LIMIT:
        raise TimeError(f"dt_exclude ratio {ratio:.6f} exceeds {EXCLUDE_LIMIT:.6f}")
    if any("edge-pair" not in item["flags"] for item in excluded):
        raise TimeError("dt_exclude contains a non-edge pair")
    return {
        "epsilon_t": epsilon_t,
        "pair_count": len(pairs),
        "excluded_count": len(excluded),
        "excluded_ratio": ratio,
        "excluded": excluded,
        "limit": EXCLUDE_LIMIT,
        "edge_only": True,
    }


def render_summary(report):
    tsc = report["tsc"]
    read = report["epsilon_read"]
    grid = report["epsilon_grid"]
    final = report["epsilon_t"]
    dt = report["dt_exclude"]
    lines = [
        PROTOCOL,
        f"VERDICT {report['verdict']}",
        f"TSC invariant={tsc['invariant_tsc']} f_tsc_hz={tsc['f_tsc_hz']:.3f} "
        f"qpc_hz={tsc['qpc_frequency_hz']}",
        f"READ max_cycles={read['max_read_cycles']} max_s={read['max_read_seconds']:.12g} "
        f"epsilon_read={read['rounded']}",
        f"GRID max_frameDuration={grid['unrounded']:.12g} "
        f"families={','.join(clip['family'] for clip in grid['clips'])}",
        f"EPSILON_T unrounded={final['unrounded']:.12g} rounded={final['rounded']}",
        f"DT_EXCLUDE pairs={dt['pair_count']} excluded={dt['excluded_count']} "
        f"ratio={dt['excluded_ratio']:.6f} limit={dt['limit']:.6f}",
    ]
    for name, status in report["criteria"].items():
        lines.append(f"CRITERION {name} {status}")
    if len(lines) > SUMMARY_LIMIT:
        raise TimeError("summary exceeds line budget")
    return "\n".join(lines) + "\n"


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--artifact", action="append", required=True)
    parser.add_argument("--tsc", required=True)
    parser.add_argument("--frame-table", required=True)
    parser.add_argument("--gate4-manifest", required=True)
    parser.add_argument("--out-dir", required=True)
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args(argv)

    try:
        if len(args.artifact) != 3:
            raise TimeError("exactly three --artifact inputs are required")
        artifacts = [c2.parse_artifact(path) for path in args.artifact]
        for index, artifact in enumerate(artifacts):
            if artifact["recorded"] != c2.GRID_GENERATION_COUNT:
                raise TimeError(f"run {index + 1}: incomplete CGRD")
        tsc, frequency = load_tsc(args.tsc)
        frame_table, by_family = load_frame_table(args.frame_table)
        gate4, families = load_gate4(args.gate4_manifest)
        epsilon_read = compute_epsilon_read(artifacts, frequency)
        epsilon_grid = compute_epsilon_grid(families, by_family)
        epsilon_t = compute_epsilon_t(epsilon_read, epsilon_grid)
        dt_exclude = apply_dt_exclude(gate4["pairs"], epsilon_t["unrounded"])
        report = {
            "schema": "C-G5-TIME-v1-result",
            "protocol": PROTOCOL,
            "verdict": "TIME_READY",
            "tsc": tsc,
            "inputs": {
                "artifacts": [{"basename": basename(path), "sha256": sha256(path)}
                              for path in args.artifact],
                "tsc": {"basename": basename(args.tsc), "sha256": sha256(args.tsc)},
                "frame_table": {"basename": basename(args.frame_table),
                                "sha256": sha256(args.frame_table)},
                "gate4_manifest": {"basename": basename(args.gate4_manifest),
                                   "sha256": sha256(args.gate4_manifest)},
            },
            "epsilon_read": epsilon_read,
            "epsilon_grid": epsilon_grid,
            "epsilon_t": epsilon_t,
            "dt_exclude": dt_exclude,
            "criteria": {name: "PASS" for name in (
                "TIME-1", "TIME-2", "TIME-3", "TIME-4", "TIME-5", "TIME-6", "TIME-7")},
            "scope": "gate4-time-only; epsilon_t not propagated to pose tau",
            "tool_sha256": sha256(__file__),
        }
        summary = render_summary(report)
        if os.path.isdir(args.out_dir) and os.listdir(args.out_dir) and not args.force:
            raise TimeError("--out-dir is not empty (pass --force)")
        os.makedirs(args.out_dir, exist_ok=True)
        with open(os.path.join(args.out_dir, "cal1c-time.json"), "w", encoding="utf-8",
                  newline="\n") as handle:
            json.dump(report, handle, indent=1, sort_keys=True, ensure_ascii=False)
            handle.write("\n")
        with open(os.path.join(args.out_dir, "cal1c-time-summary.txt"), "w", encoding="utf-8",
                  newline="\n") as handle:
            handle.write(summary)
        sys.stdout.write(summary)
        return 0
    except TimeError as error:
        print(f"[INVALID] {error}", file=sys.stderr)
        return 2
    except (OSError, ValueError, KeyError) as error:
        print(f"[GUARD FAIL] {error}", file=sys.stderr)
        return 3


if __name__ == "__main__":
    sys.exit(main())
