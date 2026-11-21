"""C-G5-TRANS 过渡窗重判（2026-11-21）。

用已归档的 v103 正式批次工件，按「跳过装备段过渡混合窗（candidate completed 1-12）」
重新统计既有 gate-4 度量的超阈计数，与正式批次原值并排比较。

输入（全部为仓库内归档，只读）：
  docs/diagnostics/g5-gate5-20261116/raw/candidate/…bin      CPK5
  docs/diagnostics/g5-gate5-20261116/raw/source/…bin         CPK5S
  docs/diagnostics/g5-gate5-20261116/formal/gate4-pair-detail.bin
  docs/diagnostics/g5-g4-cov-20261115/coverage.json
  docs/diagnostics/g5-gate4-freeze-20261116/tolerances.json

用法：
  python -B docs/diagnostics/g5-transition-window-rejudge-20261121/rejudge.py
"""
from __future__ import annotations

import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
TOOLS = ROOT / "tools"
sys.path.insert(0, str(TOOLS))

import gate4_pair as g  # noqa: E402

GATE5 = ROOT / "docs/diagnostics/g5-gate5-20261116"
CAND = next((GATE5 / "raw/candidate").glob("CharacterPanel-g5k5-*.bin"))
SRC = next((GATE5 / "raw/source").glob("CharacterPanel-g5k5s-*.bin"))
DETAIL = GATE5 / "formal/gate4-pair-detail.bin"
COVERAGE = ROOT / "docs/diagnostics/g5-g4-cov-20261115/coverage.json"
TOLERANCES = ROOT / "docs/diagnostics/g5-gate4-freeze-20261116/tolerances.json"

DETAIL_HEADER = struct.Struct("<IIII")
DETAIL_ROW = DETAIL_HEADER.size + g.TRACK_COUNT * struct.Struct("<4f").size

# Pre-registered: the equip transition blend lasts 0.2 s = 12 frames at 60 Hz, and the
# v120 run measured its weight stepping 1/11 per command over completed 1..12.
TRANSITION_MAX_COMPLETED = 12


def load_pairs() -> list[dict]:
    cand = g.parse_candidate(str(CAND))
    src = g.parse_source(str(SRC))
    manifest = json.loads((GATE5 / "formal/gate4-pair-manifest.json").read_text(encoding="utf-8"))
    pairs = manifest["pairs"]
    if len(pairs) * DETAIL_ROW != DETAIL.stat().st_size:
        raise SystemExit(f"detail size {DETAIL.stat().st_size} != {len(pairs)} x {DETAIL_ROW}")
    rows = []
    blob = DETAIL.read_bytes()
    for index, pair in enumerate(pairs):
        off = index * DETAIL_ROW
        pair_index, cand_record, src_seq, _reserved = DETAIL_HEADER.unpack_from(blob, off)
        if pair_index != index:
            raise SystemExit(f"detail row {index}: pair_index={pair_index}")
        rec = cand["records"][cand_record]
        tracks = []
        for bone in range(g.TRACK_COUNT):
            tracks.append(struct.unpack_from("<4f", blob, off + DETAIL_HEADER.size + bone * 16))
        rows.append({
            "pair_index": pair_index,
            "cand_record": cand_record,
            "segment": rec["segment"],
            "completed": rec["completed"],
            "src_seq": src_seq,
            "tracks": tracks,
        })
    return rows


def summarise(rows: list[dict], coverage: dict, tolerances: dict) -> dict:
    primary = coverage["primary_set"]
    tau_p, tau_r, tau_s = tolerances["tau_p"], tolerances["tau_r"], tolerances["tau_s"]
    exceeded = {"tau_p": 0, "tau_r": 0, "tau_s": 0}
    samples = {"E_p": [], "E_r": [], "E_s": []}
    invalid = 0
    for row in rows:
        for bone in primary:
            e_p, e_r, _theta, e_s = row["tracks"][bone]
            if not (e_r == e_r) or not (e_p == e_p) or not (e_s == e_s):
                invalid += 1
                continue
            samples["E_p"].append(e_p)
            samples["E_r"].append(e_r)
            samples["E_s"].append(e_s)
            if e_p > tau_p:
                exceeded["tau_p"] += 1
            if e_r > tau_r:
                exceeded["tau_r"] += 1
            if e_s > tau_s:
                exceeded["tau_s"] += 1
    return {
        "pairs": len(rows),
        "components": len(samples["E_p"]),
        "invalid": invalid,
        "exceeded": exceeded,
        "p99": {k: _p99(v) for k, v in samples.items()},
        "median": {k: _median(v) for k, v in samples.items()},
    }


def _p99(values: list[float]) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    index = min(len(ordered) - 1, max(0, int(round(0.99 * (len(ordered) - 1)))))
    return ordered[index]


def _median(values: list[float]) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    return ordered[len(ordered) // 2]


def main() -> int:
    coverage = g.load_coverage(str(COVERAGE))
    tolerances = g.load_tolerances(str(TOLERANCES))
    rows = load_pairs()

    baseline = summarise(rows, coverage, tolerances)
    kept = [r for r in rows if not (r["segment"] == 0 and r["completed"] <= TRANSITION_MAX_COMPLETED)]
    dropped = [r for r in rows if r["segment"] == 0 and r["completed"] <= TRANSITION_MAX_COMPLETED]
    filtered = summarise(kept, coverage, tolerances)

    print("inputs")
    print(f"  candidate {CAND.name}")
    print(f"  source    {SRC.name}")
    print(f"  detail    {DETAIL.name} ({len(rows)} pairs, {DETAIL_ROW} B/row)")
    print(f"  tau_p={tolerances['tau_p']} tau_r={tolerances['tau_r']} tau_s={tolerances['tau_s']}")
    print()
    print("baseline (all pairs)")
    print(f"  pairs={baseline['pairs']} components={baseline['components']} invalid={baseline['invalid']}")
    print(f"  exceeded {baseline['exceeded']}")
    print(f"  median {baseline['median']}")
    print(f"  p99 {baseline['p99']}")
    print()
    print(f"transition window dropped: segment=equip completed<= {TRANSITION_MAX_COMPLETED} "
          f"-> {len(dropped)} pairs")
    print("filtered (equip transition window excluded)")
    print(f"  pairs={filtered['pairs']} components={filtered['components']} invalid={filtered['invalid']}")
    print(f"  exceeded {filtered['exceeded']}")
    print(f"  median {filtered['median']}")
    print(f"  p99 {filtered['p99']}")
    print()
    for key in ("tau_p", "tau_r", "tau_s"):
        before = baseline["exceeded"][key]
        after = filtered["exceeded"][key]
        delta = after - before
        share = f"{100.0 * (-delta) / before:.1f}%" if before else "-"
        print(f"  {key}: {before} -> {after} (delta {delta:+d}, removed {share} of the excess)")
    print()
    print("per-pair excess rate (dropped window vs the rest)")
    for key in ("tau_p", "tau_r", "tau_s"):
        inside = baseline["exceeded"][key] - filtered["exceeded"][key]
        inside_rate = inside / len(dropped) if dropped else 0.0
        outside_rate = filtered["exceeded"][key] / filtered["pairs"] if filtered["pairs"] else 0.0
        print(f"  {key}: inside={inside_rate:.1f}/pair outside={outside_rate:.1f}/pair")
    print()
    print("window sensitivity (equip completed <= N dropped)")
    print("  N   pairs  tau_p  tau_r  tau_s")
    for window in (0, 6, 12, 24, 48):
        kept_n = [r for r in rows if not (r["segment"] == 0 and r["completed"] <= window)]
        summary = summarise(kept_n, coverage, tolerances)
        print(f"  {window:<3} {summary['pairs']:<6} {summary['exceeded']['tau_p']:<6} "
              f"{summary['exceeded']['tau_r']:<6} {summary['exceeded']['tau_s']}")
    print()
    print("per-track exceedances, transition window vs the rest")
    for tau_key, index, metric in METRICS:
        print(f"  --- {metric} ({tau_key} = {tolerances[tau_key]:g}) ---")
        for label, subset in (("window", dropped), ("rest", kept)):
            counts = _track_exceeds(subset, coverage, tolerances, index, tau_key)
            total = sum(counts.values())
            if total == 0:
                print(f"  {label:<6} total=0")
                continue
            top = sorted(counts.items(), key=lambda item: (-item[1], item[0]))
            top8 = sum(count for _track, count in top[:8])
            top24 = sum(count for _track, count in top[:24])
            active = sum(1 for count in counts.values() if count)
            print(f"  {label:<6} total={total:<5} tracks-with-excess={active:<3} "
                  f"top8={top8} ({100.0 * top8 / total:.0f}%) top24={top24} ({100.0 * top24 / total:.0f}%)")
            print("         top: " + ", ".join(f"t{track}={count}" for track, count in top[:8] if count))
    print()
    print("per-pair exceed count inside 'rest' (min / median / max per pair)")
    for tau_key, index, metric in METRICS:
        counts = []
        for row in kept:
            number = 0
            for track in coverage["primary_set"]:
                value = row["tracks"][track][index]
                if value == value and value > tolerances[tau_key]:
                    number += 1
            counts.append(number)
        if not counts:
            continue
        counts.sort()
        print(f"  {metric}: min={counts[0]} median={counts[len(counts) // 2]} max={counts[-1]} "
              f"(of {len(coverage['primary_set'])} primary tracks)")

    print()
    print("per-track E_r stability on 'rest' (mean / std / coefficient of variation)")
    rest_stats = _track_stats(kept, coverage, 1)
    equip_stats = _track_stats([r for r in kept if r["segment"] == 0], coverage, 1)
    unequip_stats = _track_stats([r for r in kept if r["segment"] == 1], coverage, 1)
    bins = {"cov<0.1": 0, "0.1-0.3": 0, "0.3-1": 0, ">=1": 0}
    for stats in rest_stats.values():
        if stats["count"] == 0 or stats["mean"] <= 0.0:
            continue
        cov = stats["std"] / stats["mean"]
        if cov < 0.1:
            bins["cov<0.1"] += 1
        elif cov < 0.3:
            bins["0.1-0.3"] += 1
        elif cov < 1.0:
            bins["0.3-1"] += 1
        else:
            bins[">=1"] += 1
    print(f"  tracks={len(rest_stats)} bins {bins}")
    ranked = sorted(rest_stats.items(), key=lambda item: -item[1]["mean"])[:8]
    print("  top by mean: track  mean        std         cov    equip-mean  unequip-mean")
    for track, stats in ranked:
        equip_mean = equip_stats[track]["mean"]
        unequip_mean = unequip_stats[track]["mean"]
        cov = stats["std"] / stats["mean"] if stats["mean"] > 0.0 else 0.0
        print(f"                t{track:<4} {stats['mean']:.6e}  {stats['std']:.6e}  {cov:5.3f}  "
              f"{equip_mean:.6e}  {unequip_mean:.6e}")
    # Segment agreement is the static/dynamic discriminator: a fixed per-track offset looks
    # the same in both segments, a pose-dependent difference does not.
    ratios = []
    for track, stats in rest_stats.items():
        if stats["mean"] <= 0.0:
            continue
        equip_mean = equip_stats[track]["mean"]
        unequip_mean = unequip_stats[track]["mean"]
        larger = max(equip_mean, unequip_mean)
        if larger <= 0.0:
            continue
        ratios.append(abs(equip_mean - unequip_mean) / larger)
    if ratios:
        ratios.sort()
        print(f"  segment disagreement |equip-unequip|/max: median={ratios[len(ratios) // 2]:.3f} "
              f"p90={ratios[min(len(ratios) - 1, int(0.9 * (len(ratios) - 1)))]:.3f}")

    print()
    print("per-pair E_r spread on 'rest' (max over primary tracks, worst 10 pairs)")
    per_pair = []
    for row in kept:
        values = [row["tracks"][track][1] for track in coverage["primary_set"]]
        values = [value for value in values if value == value]
        if not values:
            continue
        values.sort()
        per_pair.append({
            "pair": row["pair_index"],
            "segment": "equip" if row["segment"] == 0 else "unequip",
            "completed": row["completed"],
            "max": values[-1],
            "median": values[len(values) // 2],
        })
    per_pair.sort(key=lambda item: -item["max"])
    print("  pair  segment  completed  max-E_r     median-E_r")
    for item in per_pair[:10]:
        print(f"  {item['pair']:<5} {item['segment']:<8} {item['completed']:<10} "
              f"{item['max']:.6e}  {item['median']:.6e}")
    for threshold, label in ((1.0e-2, "0.01 (~18deg)"), (1.0e-3, "1e-3 (~5.7deg)"), (1.0e-4, "1e-4 (~1.8deg)")):
        number = sum(1 for item in per_pair if item["max"] > threshold)
        print(f"  pairs with max E_r > {label}: {number}/{len(per_pair)}")
    print()
    print("worst pairs: which tracks carry the rotation error (top 6 each)")
    by_pair = {row["pair_index"]: row for row in kept}
    for item in per_pair[:3]:
        row = by_pair[item["pair"]]
        ranked_tracks = sorted(((track, row["tracks"][track][1]) for track in coverage["primary_set"]
                                if row["tracks"][track][1] == row["tracks"][track][1]),
                               key=lambda entry: -entry[1])[:6]
        rendered = ", ".join(f"t{track}={value:.4e}" for track, value in ranked_tracks)
        print(f"  pair {item['pair']} ({item['segment']} {item['completed']}): {rendered}")
    return 0


def _track_stats(rows: list[dict], coverage: dict, index: int) -> dict[int, dict]:
    primary = coverage["primary_set"]
    out = {}
    for track in primary:
        values = []
        for row in rows:
            value = row["tracks"][track][index]
            if value == value:
                values.append(value)
        if not values:
            out[track] = {"count": 0, "mean": 0.0, "std": 0.0}
            continue
        mean = sum(values) / len(values)
        variance = sum((value - mean) ** 2 for value in values) / len(values)
        out[track] = {"count": len(values), "mean": mean, "std": variance ** 0.5}
    return out


METRICS = (("tau_p", 0, "E_p"), ("tau_r", 1, "E_r"), ("tau_s", 3, "E_s"))


def _track_exceeds(rows: list[dict], coverage: dict, tolerances: dict, index: int,
                   tau_key: str) -> dict[int, int]:
    primary = coverage["primary_set"]
    tau = tolerances[tau_key]
    counts = {track: 0 for track in primary}
    for row in rows:
        for track in primary:
            value = row["tracks"][track][index]
            if value == value and value > tau:
                counts[track] += 1
    return counts


if __name__ == "__main__":
    raise SystemExit(main())
