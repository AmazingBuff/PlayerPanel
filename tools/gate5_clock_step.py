#!/usr/bin/env python3
"""C-G5-CLOCK v2 audit: candidate clock-step groups vs gate-5 pairing.

Post-hoc descriptive only. It reads the formal gate-5 artifacts and reports how
many candidates share one clip local time, how many source records are reused by
nearest-clock pairing, and which bone groups contribute the G4P-7 exceedances.
It does not change the gate-4 verdict, tolerances or coverage.
"""

from __future__ import annotations

import argparse
import collections
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
FAMILIES = ("1hm_equip", "1hm_unequip", "1hm_idle")


def fail(message: str) -> None:
    raise SystemExit(f"[CLOCK GUARD] {message}")


def family(record: dict) -> str | None:
    for name in FAMILIES:
        if any(clip["bare"] == name for clip in record["clips"]):
            return name
    return None


def load_names(path: str) -> dict[int, str]:
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


def clock_of(record: dict, name: str) -> float:
    for clip in record["clips"]:
        if clip["bare"] == name:
            return clip["local_time"]
    fail(f"record {record.get('index', record.get('seq'))} has no {name} clock")


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
    args = parser.parse_args()

    if os.path.exists(args.out_dir):
        fail(f"output directory already exists: {args.out_dir}")
    with open(args.manifest, "r", encoding="utf-8") as handle:
        manifest = json.load(handle)
    if manifest.get("schema") != "C-G5-G4 manifest v1" or not manifest.get("pose_decoded"):
        fail("formal pose manifest required")
    with open(args.detail, "rb") as handle:
        detail = handle.read()
    if len(detail) != len(manifest["pairs"]) * DETAIL_ROW:
        fail(f"detail bytes={len(detail)}")

    coverage = g.load_coverage(args.coverage)
    tolerances = g.load_tolerances(args.tolerances)
    names = load_names(args.bone_log)
    cand = g.parse_candidate(args.candidate)
    src = g.parse_source(args.source)
    src_by_seq = {record["seq"]: record for record in src["records"]}
    auxiliary = coverage["auxiliary_set"]
    attachment = {index for index, name in names.items()
                  if any(token in name for token in ("Weapon", "Quiver", "Pauldron", "COM"))}
    body = set(range(99)) - attachment

    def group(index: int) -> str:
        if index in auxiliary:
            return "auxiliary"
        if index in attachment:
            return "attachment"
        return "body"

    domains: dict[str, dict] = {}
    for name in FAMILIES:
        cand_records = [record for record in cand["records"] if family(record) == name]
        src_records = [record for record in src["records"] if family(record) == name]
        if not cand_records or not src_records:
            continue
        cand_time = {record["index"]: clock_of(record, name) for record in cand_records}
        groups = collections.Counter(round(value, 9) for value in cand_time.values())
        pairs = [(index, pair) for index, pair in enumerate(manifest["pairs"])
                 if pair["domain"] == name]
        src_seq_count = collections.Counter(pair["src"]["seq"] for _, pair in pairs)
        stats = {group_name: {"stair": [0, 0, 0.0], "unique": [0, 0, 0.0]}
                 for group_name in ("body", "attachment", "auxiliary")}
        per_bone = collections.defaultdict(lambda: [0, 0, 0.0])  # stair, unique, max
        stair_pairs = 0
        for pair_index, pair in pairs:
            ci = pair["cand"]["record"]
            stair = groups[round(cand_time[ci], 9)] > 1
            if stair:
                stair_pairs += 1
            row = detail[pair_index * DETAIL_ROW:(pair_index + 1) * DETAIL_ROW]
            for bone in range(TRACK_COUNT):
                ep = struct.unpack_from("<f", row, 16 + bone * 16)[0]
                if not math.isfinite(ep):
                    fail(f"pair {pair_index} bone {bone}: non-finite E_p")
                key = "stair" if stair else "unique"
                bucket = stats[group(bone)][key]
                bucket[1] += 1
                if ep > tolerances["tau_p"]:
                    bucket[0] += 1
                    per_bone[bone][0 if stair else 1] += 1
                if ep > per_bone[bone][2]:
                    per_bone[bone][2] = ep
        domains[name] = {
            "candidate_records": len(cand_records),
            "candidate_unique_times": len(groups),
            "candidate_max_group": max(groups.values()),
            "source_records": len(src_records),
            "source_unique_times": len({round(clock_of(record, name), 9) for record in src_records}),
            "pairs": len(pairs),
            "stair_pairs": stair_pairs,
            "unique_pairs": len(pairs) - stair_pairs,
            "source_unique_seqs_used": len(src_seq_count),
            "source_max_reuse": max(src_seq_count.values()),
            "groups": stats,
            "top_bones": [
                {"index": bone, "name": names[bone], "group": group(bone),
                 "stair_exceed": value[0], "unique_exceed": value[1], "max_ep": value[2]}
                for bone, value in sorted(per_bone.items(),
                                          key=lambda item: (-(item[1][0] + item[1][1]), item[0]))[:20]
            ],
        }

    report = {
        "schema": "C-G5-CLOCK v2 audit",
        "inputs": {
            "manifest_sha256": g.sha256_of(args.manifest),
            "detail_sha256": g.sha256_of(args.detail),
            "candidate_sha256": g.sha256_of(args.candidate),
            "source_sha256": g.sha256_of(args.source),
            "coverage_sha256": coverage["sha256"],
            "tolerances_sha256": g.sha256_of(args.tolerances),
        },
        "domains": domains,
    }
    os.makedirs(args.out_dir, exist_ok=False)
    with open(os.path.join(args.out_dir, "clock-step.json"), "w",
              encoding="utf-8", newline="\n") as handle:
        json.dump(report, handle, indent=1, sort_keys=True)
        handle.write("\n")

    lines = ["CLOCK SUMMARY protocol=C-G5-CLOCK v2 audit"]
    lines.append("CLOCK VERDICT G4P-7=FAIL attribution-only; tolerances/coverage unchanged")
    for name, data in domains.items():
        lines.append(
            f"CLOCK DOMAIN {name} cand-records={data['candidate_records']} "
            f"cand-unique-times={data['candidate_unique_times']} "
            f"cand-max-group={data['candidate_max_group']} "
            f"source-records={data['source_records']} source-unique-times={data['source_unique_times']} "
            f"pairs={data['pairs']} stair-pairs={data['stair_pairs']} "
            f"source-unique-seqs={data['source_unique_seqs_used']} "
            f"source-max-reuse={data['source_max_reuse']}")
        for group_name in ("body", "attachment", "auxiliary"):
            value = data["groups"][group_name]
            lines.append(
                f"  {group_name:<10} stair={value['stair'][0]}/{value['stair'][1]} "
                f"unique={value['unique'][0]}/{value['unique'][1]}")
        for bone in data["top_bones"][:8]:
            lines.append(
                f"  TOP {bone['index']:3d} {bone['name']:<28} {bone['group']:<10} "
                f"stair={bone['stair_exceed']} unique={bone['unique_exceed']} "
                f"max={bone['max_ep']:.9g}")
    summary = "\n".join(lines) + "\n"
    with open(os.path.join(args.out_dir, "clock-step.txt"), "w",
              encoding="utf-8", newline="\n") as handle:
        handle.write(summary)
    sys.stdout.write(summary)
    return 0


if __name__ == "__main__":
    sys.exit(main())
