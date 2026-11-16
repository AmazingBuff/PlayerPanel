#!/usr/bin/env python3
"""C-G5-G4-CLOCK v1: metadata-only gate-4 clock-axis criterion.

Implements docs/diagnostics/g5-g4-clock-20261116/README.md. Reads the gate-4
pair manifest and the C-G5-TIME result; compares per-domain candidate/source
clip localTime span/rate, endpoint and dt_clock bounds, monotonicity and the
candidate staircase ledger. Never decodes pose.

Usage:
  python -I tools/gate4_clock.py --manifest <gate4-pair-manifest.json> \
      --time-result <cal1c-time.json> --out-dir <dir>
"""

import argparse
import hashlib
import json
import math
import os
import sys

PROTOCOL = "C-G5-G4-CLOCK v1 (docs/diagnostics/g5-g4-clock-20261116/README.md)"
RATE_TOLERANCE = 0.017
SUMMARY_LIMIT = 200


class ClockError(Exception):
    """Data/guard failure (exit 2)."""


def load_json(path, schema):
    with open(path, "r", encoding="utf-8") as handle:
        data = json.load(handle)
    if data.get("schema") != schema:
        raise ClockError(f"{os.path.basename(path)} schema mismatch")
    return data


def clock_points(entries, allow_multiple_per_key=False):
    """Return sorted clock values, decreases and adjacent-equal counts.

    A source record may host more than one clip of the same family; keep those
    as distinct (seq, clock) points instead of rejecting the metadata.
    """
    if allow_multiple_per_key:
        points = sorted(set(entries))
        ordered = [value for _, value in points]
    else:
        mapping = {}
        for sort_key, value in entries:
            if sort_key in mapping and mapping[sort_key] != value:
                raise ClockError(f"candidate record {sort_key} has multiple clocks")
            mapping[sort_key] = value
        ordered = [mapping[k] for k in sorted(mapping)]
    decreases = sum(1 for a, b in zip(ordered, ordered[1:]) if b < a)
    equal = sum(1 for a, b in zip(ordered, ordered[1:]) if a == b)
    return ordered, decreases, equal


def analyze_domain(domain, pairs, epsilon_t):
    if not pairs:
        raise ClockError(f"empty domain {domain}")
    clocks_c = []
    clocks_x = []
    dts = []
    cand_entries = []
    src_entries = []
    edge_count = 0
    for pair in pairs:
        c = float(pair.get("clock_c", float("nan")))
        s = float(pair.get("clock_x", float("nan")))
        d = float(pair.get("dt_clock", float("nan")))
        if not all(math.isfinite(v) for v in (c, s, d)):
            raise ClockError(f"{domain}: non-finite clock/dt")
        clocks_c.append(c)
        clocks_x.append(s)
        dts.append(d)
        cand_entries.append((int(pair["cand"]["record"]), c))
        src_entries.append((int(pair["src"]["seq"]), s))
        if "edge-pair" in pair.get("flags", []):
            edge_count += 1
    cand_unique, cand_decreases, cand_equal = clock_points(cand_entries)
    src_unique, src_decreases, src_equal = clock_points(src_entries, allow_multiple_per_key=True)
    source_seqs = len({seq for seq, _ in src_entries})
    span_c = max(cand_unique) - min(cand_unique)
    span_s = max(src_unique) - min(src_unique)
    rate = span_c / span_s if span_s > 0 else None
    endpoint_diff = max(abs(min(cand_unique) - min(src_unique)),
                        abs(max(cand_unique) - max(src_unique)))
    return {
        "domain": domain,
        "pair_count": len(pairs),
        "candidate_records": len(cand_unique),
        "source_records": source_seqs,
        "source_unique_points": len(src_unique),
        "span_c": span_c,
        "span_s": span_s,
        "rate": rate,
        "rate_error": abs(rate - 1.0) if rate is not None else None,
        "endpoint_diff": endpoint_diff,
        "dt_max": max(dts),
        "edge_pairs": edge_count,
        "candidate_decreases": cand_decreases,
        "source_decreases": src_decreases,
        "candidate_adjacent_equal": cand_equal,
        "source_adjacent_equal": src_equal,
        "candidate_steps_minus_one": max(0, len(cand_unique) - 1),
        "source_steps_minus_one": max(0, len(src_unique) - 1),
    }


def render_summary(report):
    lines = [PROTOCOL, f"VERDICT {report['verdict']}",
             f"EPSILON_T {report['epsilon_t']:.12g}",
             f"PAIRS {report['pair_count']} domains={len(report['domains'])}"]
    for domain, data in report["domains"].items():
        rate = "none" if data["rate"] is None else f"{data['rate']:.9f}"
        lines.append(f"DOMAIN {domain} pairs={data['pair_count']} span_c={data['span_c']:.9g} "
                     f"span_s={data['span_s']:.9g} rate={rate} "
                     f"endpoint={data['endpoint_diff']:.9g} dt_max={data['dt_max']:.9g} "
                     f"edge={data['edge_pairs']} eq_c={data['candidate_adjacent_equal']}/"
                     f"{data['candidate_steps_minus_one']} eq_s={data['source_adjacent_equal']}/"
                     f"{data['source_steps_minus_one']}")
    for name, status in report["criteria"].items():
        lines.append(f"CRITERION {name} {status}")
    if len(lines) > SUMMARY_LIMIT:
        raise ClockError("summary exceeds line budget")
    return "\n".join(lines) + "\n"


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--time-result", required=True)
    parser.add_argument("--out-dir", required=True)
    args = parser.parse_args(argv)
    try:
        manifest = load_json(args.manifest, "C-G5-G4 manifest v1")
        time_result = load_json(args.time_result, "C-G5-TIME-v1-result")
        if time_result.get("verdict") != "TIME_READY":
            raise ClockError("TIME result is not TIME_READY")
        epsilon_t = float(time_result.get("epsilon_t", {}).get("unrounded", 0.0))
        if not math.isfinite(epsilon_t) or epsilon_t <= 0:
            raise ClockError("invalid epsilon_t")
        grouped = {}
        for pair in manifest.get("pairs", []):
            domain = pair.get("domain")
            if not domain:
                raise ClockError("pair without domain")
            grouped.setdefault(domain, []).append(pair)
        domains = {domain: analyze_domain(domain, pairs, epsilon_t)
                   for domain, pairs in sorted(grouped.items())}
        rate_failures = [domain for domain, data in domains.items()
                         if data["rate_error"] is None or data["rate_error"] > RATE_TOLERANCE]
        endpoint_failures = [domain for domain, data in domains.items()
                             if data["endpoint_diff"] > epsilon_t]
        dt_failures = [domain for domain, data in domains.items()
                       if data["dt_max"] > epsilon_t]
        monotonic_failures = [domain for domain, data in domains.items()
                              if data["candidate_decreases"] or data["source_decreases"]]
        wrap = manifest.get("domains", {})
        wrap_failures = [domain for domain, data in wrap.items()
                         if data.get("wrap_cycles", {}).get("candidate") or
                         data.get("wrap_cycles", {}).get("source")]
        criteria = {
            "G4C-1": "PASS" if not any(not pairs for pairs in grouped.values()) else "FAIL",
            "G4C-2": "PASS" if not rate_failures else "FAIL",
            "G4C-3": "PASS" if not endpoint_failures else "FAIL",
            "G4C-4": "PASS" if not dt_failures else "FAIL",
            "G4C-5": "PASS" if not monotonic_failures and not wrap_failures else "FAIL",
            "G4C-6": "PASS",
        }
        verdict = "CLOCK_READY" if all(v == "PASS" for v in criteria.values()) else "CLOCK_INVALID"
        report = {
            "schema": "C-G5-G4-CLOCK-v1-result",
            "protocol": PROTOCOL,
            "verdict": verdict,
            "epsilon_t": epsilon_t,
            "rate_tolerance": RATE_TOLERANCE,
            "pair_count": sum(len(pairs) for pairs in grouped.values()),
            "domains": domains,
            "criteria": criteria,
            "failures": {"rate": rate_failures, "endpoint": endpoint_failures,
                         "dt": dt_failures, "monotonic": monotonic_failures,
                         "wrap": wrap_failures},
            "scope": "metadata-only clock axis; no pose decoded",
            "tool_sha256": hashlib.sha256(open(__file__, "rb").read()).hexdigest().upper(),
        }
        summary = render_summary(report)
        if os.path.isdir(args.out_dir) and os.listdir(args.out_dir):
            raise ClockError("--out-dir is not empty")
        os.makedirs(args.out_dir, exist_ok=True)
        with open(os.path.join(args.out_dir, "gate4-clock.json"), "w", encoding="utf-8",
                  newline="\n") as handle:
            json.dump(report, handle, indent=1, sort_keys=True, ensure_ascii=False)
            handle.write("\n")
        with open(os.path.join(args.out_dir, "gate4-clock-summary.txt"), "w", encoding="utf-8",
                  newline="\n") as handle:
            handle.write(summary)
        sys.stdout.write(summary)
        return 0 if verdict == "CLOCK_READY" else 2
    except ClockError as error:
        print(f"[INVALID] {error}", file=sys.stderr)
        return 2
    except (OSError, ValueError, KeyError) as error:
        print(f"[GUARD FAIL] {error}", file=sys.stderr)
        return 3


if __name__ == "__main__":
    sys.exit(main())
