"""C-G5-TOL v1 零假设测量：源管线跨会话自差异（2026-11-22，纯离线）。

两份已归档源侧批次（gate-5 v103 849 条 + VER v122 1145 条）来自同一
save/modlist/冻结场景族的两次独立会话。按域内 nearest-clock 相位配对，
对 99 主轨的等值分量（与 gate5_ver 的 DECL 通道映射一致）计算源-源
逐分量差异分布——该分布即"良性差异"的零假设，容差阈值从它推导。

排除：IDUN 记录、双主族共存记录（过渡混合窗，注入时序依赖）、
dt_clock > 1/120 s 的配对（相位覆盖稀疏处的粗配对）。

用法：python -B docs/diagnostics/g5-tol-contract-20261122/measure_null.py
"""
from __future__ import annotations

import math
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
TOOLS = ROOT / "tools"
sys.path.insert(0, str(TOOLS))

import gate4_pair as g  # noqa: E402
import gate5_ver as v  # noqa: E402

GATE5 = ROOT / "docs/diagnostics/g5-gate5-20261116/raw/source"
VER = ROOT / "docs/diagnostics/g5-ver-formal-20261122/raw/source"
A_BIN = next(GATE5.glob("CharacterPanel-g5k5s-*.bin"))     # v103 849 条
B_BIN = next(VER.glob("CharacterPanel-g5k5s-*.bin"))       # v122 1145 条

PRIMARY = v.PRIMARY_FAMS
DT_CAP = 1.0 / 120.0


def pose_of(parsed: dict, record: dict) -> list[tuple]:
    base = (parsed["header_size"] + record["index"] * parsed["record_size"]
            + parsed["meta_size"] + g.CLIP_CAPACITY * g.K5_CLIP.size)
    return [struct.unpack_from("<10f", parsed["blob"], base + t * g.TRACK_BYTES)
            for t in range(g.TRACK_COUNT)]


def main() -> int:
    a = g.parse_source(str(A_BIN))
    b = g.parse_source(str(B_BIN))

    def prep(parsed):
        for rec in parsed["records"]:
            fam, clip = None, None
            for f in g.CLOCK_PRIORITY:
                hit = next((c for c in rec["clips"] if c["bare"] == f and c["data_fnv"] != 0), None)
                if hit:
                    fam, clip = f, hit
                    break
            rec["fam"], rec["clock_clip"] = fam, clip
            rec["idun"] = (not rec["clips"]) or any(c["data_fnv"] == 0 for c in rec["clips"])
            primaries = {c["bare"] for c in rec["clips"] if c["data_fnv"] != 0 and c["bare"] in PRIMARY}
            rec["single_primary"] = len(primaries) == 1
            rec["pose"] = pose_of(parsed, rec)

    prep(a)
    prep(b)

    # B 侧按域索引（非 IDUN 且单主族）
    by_domain: dict[str, list] = {}
    for rec in b["records"]:
        if rec["fam"] is None or rec["idun"] or not rec["single_primary"]:
            continue
        by_domain.setdefault(rec["fam"], []).append(rec)

    pairs = []
    for rec in a["records"]:
        if rec["fam"] is None or rec["idun"] or not rec["single_primary"]:
            continue
        cands = by_domain.get(rec["fam"], [])
        if not cands:
            continue
        x = rec["clock_clip"]["local_time"]
        best = None
        for r in cands:
            d = abs(x - r["clock_clip"]["local_time"])
            if best is None or d < best[0]:
                best = (d, r)
        if best[0] <= DT_CAP:
            pairs.append((rec, best[1], best[0]))

    print(f"paired: A={a['count']} B={b['count']} -> matched={len(pairs)}")
    by = {}
    for rec_a, rec_b, dt in pairs:
        by.setdefault(rec_a["fam"], []).append((rec_a, rec_b, dt))
    for fam, ps in sorted(by.items()):
        dts = sorted(p[2] for p in ps)
        print(f"  {fam}: {len(ps)} pairs, dt median={dts[len(dts)//2]*1000:.2f}ms max={dts[-1]*1000:.2f}ms")

    # 等值分量差异分布（DECL 通道映射与 gate5_ver 一致）
    dist = {"pos": [], "rot_deg": [], "scale": []}
    per_track_rot: dict[int, list] = {}
    for rec_a, rec_b, _dt in pairs:
        for t in range(v.MAIN_TRACK_MAX + 1):
            for channel, getter in (("pos", 0), ("rot", 1), ("scale", 3)):
                if not v.channel_judged(t, channel):
                    continue
                ta, tb = rec_a["pose"][t], rec_b["pose"][t]
                e_p, e_r, theta, e_s = g.pose_metrics(ta, tb)
                if channel == "pos" and e_p is not None and math.isfinite(e_p):
                    dist["pos"].append(e_p)
                    if e_p > 1e-4:
                        per_track_rot.setdefault(("p", t), []).append(e_p)
                elif channel == "rot":
                    if e_r is None or theta is None:
                        continue
                    deg = math.degrees(theta)
                    dist["rot_deg"].append(deg)
                    per_track_rot.setdefault(("r", t), []).append(deg)
                elif channel == "scale" and e_s is not None and math.isfinite(e_s):
                    dist["scale"].append(e_s)

    def pct(values, q):
        ordered = sorted(values)
        return ordered[min(len(ordered) - 1, int(len(ordered) * q))]

    print("\n== 零假设分布（等值分量，源-源跨会话）==")
    for channel, values in (("pos", dist["pos"]), ("rot_deg", dist["rot_deg"]), ("scale", dist["scale"])):
        if values:
            print(f"  {channel:8s} n={len(values)} p50={pct(values, .50):.3e} p90={pct(values, .90):.3e} "
                  f"p95={pct(values, .95):.3e} p99={pct(values, .99):.3e} max={max(values):.3e}")

    print("\n== 旋转通道 per-track top（n>=20 的轨）==")
    rows = []
    for (kind, t), values in per_track_rot.items():
        if kind == "r" and len(values) >= 20:
            rows.append((pct(values, .99), t, len(values), max(values)))
    for p99, t, n, mx in sorted(rows, reverse=True)[:12]:
        print(f"  t{t}: n={n} p99={p99:.3f} deg max={mx:.3f} deg")
    print("\n== 平移通道 per-track top（n>=20 的轨）==")
    rows = []
    for (kind, t), values in per_track_rot.items():
        if kind == "p" and len(values) >= 20:
            rows.append((pct(values, .99), t, len(values), max(values)))
    for p99, t, n, mx in sorted(rows, reverse=True)[:12]:
        print(f"  t{t}: n={n} p99={p99:.3e} max={mx:.3e}")

    # 机械生成逐轨容差表：tol_track = max(null P99 × 2, 通道地板)。
    # 地板覆盖已知量化机制（时钟阶梯 rot ≤1.2° / 平移 1e-3 / 缩放 1e-5）。
    # 样本 < MIN_N 的轨用通道地板（null 证据不足，取保守地板）。
    import json  # noqa: E402
    MIN_N = 20
    SAFETY = 2.0
    FLOORS = {"rot_deg": 2.0, "pos": 1e-3, "scale": 1e-5}
    key_of = {"rot_deg": "r", "pos": "p", "scale": "s"}
    table = {"rot_deg": {}, "pos": {}, "scale": {}}
    for t in range(v.MAIN_TRACK_MAX + 1):
        for channel in FLOORS:
            values = per_track_rot.get((key_of[channel], t), [])
            if len(values) >= MIN_N:
                bound = max(pct(values, .99) * SAFETY, FLOORS[channel])
            else:
                bound = FLOORS[channel]
            digits = 6 if channel == "rot_deg" else 9
            table[channel][str(t)] = math.ceil(bound * 10 ** digits) / 10 ** digits
    out = {
        "schema": "g5-tol-per-track/1",
        "basis": "source-vs-source cross-session null (v103 849 vs v122 1145), "
                 "471 matched pairs, per-track P99 x 2, floors rot=2.0deg pos=1e-3 scale=1e-5, min-n 20",
        **table,
    }
    out_path = Path(__file__).resolve().parent / "per_track_tolerances.json"
    out_path.write_text(json.dumps(out, ensure_ascii=False, sort_keys=True, indent=1),
                        encoding="utf-8")
    print(f"\n== 容差表已生成 {out_path.name} ==")
    for ch in ("rot_deg", "pos", "scale"):
        above = {t: v_ for t, v_ in table[ch].items() if v_ > FLOORS[ch]}
        print(f"  {ch}: {len(table[ch])} tracks, 高于地板 {len(above)} 条:",
              dict(sorted(above.items(), key=lambda kv: -kv[1])[:8]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
