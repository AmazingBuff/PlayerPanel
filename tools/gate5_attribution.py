#!/usr/bin/env python3
"""C-G5-ATT v1: descriptive attribution of the gate-5 G4P-7 failures.

This tool does not change the gate-4 verdict, tolerances, coverage or pairing.
It reads an already-produced formal manifest/detail pair plus the two CPK5
artifacts and reports per-bone static spans, exceedance counts and names.
"""

from __future__ import annotations

import argparse
import json
import math
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gate4_pair as g  # noqa: E402


TRACK_COUNT = 116
TRACK_BYTES = 40
DETAIL_ROW = 16 + TRACK_COUNT * 16


def fail(message: str) -> None:
    raise SystemExit(f"[ATTR GUARD] {message}")


def load_bone_names(path: str) -> dict[int, str]:
    names: dict[int, str] = {}
    pattern = re.compile(r"side=source phase=bone index=(\d+) name='([^']+)'")
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            match = pattern.search(line)
            if match:
                names[int(match.group(1))] = match.group(2)
    if sorted(names) != list(range(TRACK_COUNT)):
        fail(f"bone log names={len(names)}; expected 0..115")
    return names


def load_manifest(path: str) -> dict:
    with open(path, "r", encoding="utf-8") as handle:
        manifest = json.load(handle)
    if manifest.get("schema") != "C-G5-G4 manifest v1":
        fail(f"manifest schema={manifest.get('schema')!r}")
    if not manifest.get("pose_decoded"):
        fail("manifest is metadata-only; formal pose data required")
    return manifest


def load_detail(path: str, pair_count: int) -> bytes:
    with open(path, "rb") as handle:
        detail = handle.read()
    if len(detail) != pair_count * DETAIL_ROW:
        fail(f"detail bytes={len(detail)} != {pair_count}x{DETAIL_ROW}")
    return detail


def percentile(values: list[float], fraction: float) -> float:
    if not values:
        return float("nan")
    ordered = sorted(values)
    return ordered[min(len(ordered) - 1, int(fraction * (len(ordered) - 1)))]


def group_for(name: str, index: int, auxiliary: set[int]) -> str:
    if index in auxiliary:
        return "auxiliary"
    if any(token in name for token in ("Weapon", "Quiver", "Pauldron", "COM")):
        return "attachment"
    return "body"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--detail", required=True)
    parser.add_argument("--candidate", required=True)
    parser.add_argument("--source", required=True)
    parser.add_argument("--bone-log", required=True)
    parser.add_argument("--coverage", required=True)
    parser.add_argument("--tolerances", required=True)
    parser.add_argument("--out-dir", required=True)
    parser.add_argument("--static-epsilon", type=float, default=1.0e-3)
    args = parser.parse_args()

    if args.static_epsilon <= 0.0 or not math.isfinite(args.static_epsilon):
        fail("static-epsilon must be finite and positive")
    if os.path.exists(args.out_dir):
        fail(f"output directory already exists: {args.out_dir}")

    manifest = load_manifest(args.manifest)
    coverage = g.load_coverage(args.coverage)
    tolerances = g.load_tolerances(args.tolerances)
    names = load_bone_names(args.bone_log)
    cand = g.parse_candidate(args.candidate)
    src = g.parse_source(args.source)
    detail = load_detail(args.detail, len(manifest["pairs"]))
    tau_p = tolerances["tau_p"]
    tau_r = tolerances["tau_r"]
    tau_s = tolerances["tau_s"]

    src_by_seq = {rec["seq"]: rec for rec in src["records"]}
    if len(src_by_seq) != src["count"]:
        fail("duplicate source seq")

    rows: list[dict] = []
    samples = {b: {"ep": [], "er": [], "es": [], "cand": [], "src": []}
               for b in range(TRACK_COUNT)}
    for pair_index, pair in enumerate(manifest["pairs"]):
        ci = pair["cand"]["record"]
        seq = pair["src"]["seq"]
        if not 0 <= ci < cand["count"]:
            fail(f"pair {pair_index}: candidate record {ci} out of range")
        if seq not in src_by_seq:
            fail(f"pair {pair_index}: source seq {seq} missing")
        src_rec = src_by_seq[seq]
        cb = cand["blob"][g._pose_base(cand, cand["records"][ci]):
                          g._pose_base(cand, cand["records"][ci]) + TRACK_COUNT * TRACK_BYTES]
        sb = src["blob"][g._pose_base(src, src_rec):
                          g._pose_base(src, src_rec) + TRACK_COUNT * TRACK_BYTES]
        row = detail[pair_index * DETAIL_ROW:(pair_index + 1) * DETAIL_ROW]
        for b in range(TRACK_COUNT):
            ca = struct.unpack_from("<10f", cb, b * TRACK_BYTES)
            sa = struct.unpack_from("<10f", sb, b * TRACK_BYTES)
            ep, er, _theta, es = struct.unpack_from("<4f", row, 16 + b * 16)
            if not all(math.isfinite(v) for v in ca + sa):
                fail(f"pair {pair_index} bone {b}: non-finite pose")
            if not all(math.isfinite(v) for v in (ep, er, es)):
                fail(f"pair {pair_index} bone {b}: non-finite metric")
            samples[b]["cand"].append(ca)
            samples[b]["src"].append(sa)
            samples[b]["ep"].append(ep)
            samples[b]["er"].append(er)
            samples[b]["es"].append(es)

    auxiliary = coverage["auxiliary_set"]
    for b in range(TRACK_COUNT):
        data = samples[b]
        cand_span = max(max(v[j] for v in data["cand"]) - min(v[j] for v in data["cand"])
                        for j in range(10))
        src_span = max(max(v[j] for v in data["src"]) - min(v[j] for v in data["src"])
                       for j in range(10))
        ep = data["ep"]
        er = data["er"]
        es = data["es"]
        rows.append({
            "index": b,
            "name": names[b],
            "group": group_for(names[b], b, auxiliary),
            "pairs": len(ep),
            "cand_span": cand_span,
            "source_span": src_span,
            "static_candidate": cand_span <= args.static_epsilon,
            "static_source": src_span <= args.static_epsilon,
            "static_both": cand_span <= args.static_epsilon and src_span <= args.static_epsilon,
            "max_ep": max(ep),
            "p99_ep": percentile(ep, 0.99),
            "p50_ep": percentile(ep, 0.50),
            "max_er": max(er),
            "max_es": max(es),
            "exceed_p": sum(v > tau_p for v in ep),
            "exceed_r": sum(v > tau_r for v in er),
            "exceed_s": sum(v > tau_s for v in es),
            "exact_zero_p": sum(v == 0.0 for v in ep),
        })

    groups: dict[str, dict] = {}
    for row in rows:
        group = groups.setdefault(row["group"], {
            "bones": 0, "samples": 0, "exceed_p": 0, "exceed_r": 0,
            "exceed_s": 0, "static_bones": 0, "static_exceed_p": 0,
        })
        group["bones"] += 1
        group["samples"] += row["pairs"]
        group["exceed_p"] += row["exceed_p"]
        group["exceed_r"] += row["exceed_r"]
        group["exceed_s"] += row["exceed_s"]
        if row["static_both"]:
            group["static_bones"] += 1
            group["static_exceed_p"] += row["exceed_p"]

    total_exceed = sum(row["exceed_p"] for row in rows)
    static_exceed = sum(row["exceed_p"] for row in rows if row["static_both"])
    static_offset = [row for row in rows if row["static_both"] and row["max_ep"] > tau_p]
    top = sorted(rows, key=lambda row: (-row["max_ep"], row["index"]))[:20]

    report = {
        "schema": "C-G5-ATT v1",
        "inputs": {
            "manifest_sha256": g.sha256_of(args.manifest),
            "detail_sha256": g.sha256_of(args.detail),
            "candidate_sha256": g.sha256_of(args.candidate),
            "source_sha256": g.sha256_of(args.source),
            "coverage_sha256": coverage["sha256"],
            "tolerances_sha256": g.sha256_of(args.tolerances),
        },
        "parameters": {"static_epsilon": args.static_epsilon, **tolerances},
        "summary": {
            "pairs": len(manifest["pairs"]),
            "bones": len(rows),
            "primary_samples": len(manifest["pairs"]) * len(coverage["primary"]),
            "auxiliary_samples": len(manifest["pairs"]) * len(coverage["auxiliary"]),
            "total_exceed_p": total_exceed,
            "static_both_bones": sum(1 for row in rows if row["static_both"]),
            "static_both_exceed_p": static_exceed,
            "groups": groups,
        },
        "bones": rows,
        "static_offset_bones": [
            {"index": row["index"], "name": row["name"], "max_ep": row["max_ep"],
             "exceed_p": row["exceed_p"], "cand_span": row["cand_span"],
             "source_span": row["source_span"]}
            for row in static_offset
        ],
        "top_by_max_ep": [
            {"index": row["index"], "name": row["name"], "group": row["group"],
             "max_ep": row["max_ep"], "p99_ep": row["p99_ep"],
             "exceed_p": row["exceed_p"]}
            for row in top
        ],
    }

    os.makedirs(args.out_dir, exist_ok=False)
    with open(os.path.join(args.out_dir, "attachment-attribution.json"), "w",
              encoding="utf-8", newline="\n") as handle:
        json.dump(report, handle, indent=1, sort_keys=True)
        handle.write("\n")

    lines = [
        "ATTR SUMMARY protocol=C-G5-ATT v1",
        f"ATTR INPUT pairs={len(manifest['pairs'])} bones={len(rows)} "
        f"primary-samples={report['summary']['primary_samples']} "
        f"auxiliary-samples={report['summary']['auxiliary_samples']}",
        f"ATTR STATIC epsilon={args.static_epsilon:.6g} "
        f"static-both={report['summary']['static_both_bones']} "
        f"static-exceed-p={static_exceed}/{total_exceed}",
    ]
    for group in ("body", "attachment", "auxiliary"):
        data = groups.get(group, {})
        lines.append(
            f"ATTR GROUP {group} bones={data.get('bones', 0)} samples={data.get('samples', 0)} "
            f"exceed-p={data.get('exceed_p', 0)} exceed-r={data.get('exceed_r', 0)} "
            f"exceed-s={data.get('exceed_s', 0)} static-bones={data.get('static_bones', 0)} "
            f"static-exceed-p={data.get('static_exceed_p', 0)}")
    lines.append("ATTR TOP")
    for row in top[:12]:
        lines.append(f"  {row['index']:3d} {row['name']:<28} {row['group']:<10} "
                     f"max-ep={row['max_ep']:.9g} p99={row['p99_ep']:.9g} "
                     f"exceed-p={row['exceed_p']}")
    lines.append("ATTR STATIC-OFFSET")
    for row in static_offset:
        lines.append(f"  {row['index']:3d} {row['name']:<28} max-ep={row['max_ep']:.9g} "
                     f"exceed-p={row['exceed_p']} cand-span={row['cand_span']:.6g} "
                     f"src-span={row['source_span']:.6g}")
    lines.append("ATTR VERDICT G4P-7=FAIL attribution-only; tolerances/coverage unchanged")
    summary = "\n".join(lines) + "\n"
    with open(os.path.join(args.out_dir, "attachment-attribution.txt"), "w",
              encoding="utf-8", newline="\n") as handle:
        handle.write(summary)
    sys.stdout.write(summary)
    return 0


if __name__ == "__main__":
    sys.exit(main())
