"""C-G5-VER v0 §五 O-1：配对粒度与驱动突发决断分析（2026-11-21，纯离线）。

用已归档的 v103 正式批次工件回答三个问题：
  Q1 突发（同一观测 clip localTime 的相邻记录）内候选 pose 是否变化、如何变化；
  Q2 burst 末步是否为与配对源帧相位对齐的成员（逐成员 vs 源误差剖面）；
  Q3 按 burst 末步重算，正式超差（tau_p/tau_r/tau_s）如何分解，剩多少真实语义差。

输入（全部为仓库内归档，只读）：
  docs/diagnostics/g5-gate5-20261116/raw/candidate/…bin     CPK5（249 记录）
  docs/diagnostics/g5-gate5-20261116/raw/source/…bin        CPK5S（849 记录）
  docs/diagnostics/g5-gate5-20261116/formal/gate4-pair-manifest.json（248 对）
  docs/diagnostics/g5-gate5-20261116/formal/gate4-pair-detail.bin（248 对明细）

用法：
  python -B docs/diagnostics/g5-o1-pairing-granularity-20261121/o1_pairing_analysis.py
"""
from __future__ import annotations

import json
import math
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
MANIFEST = GATE5 / "formal/gate4-pair-manifest.json"
DETAIL = GATE5 / "formal/gate4-pair-detail.bin"

TAU = {"p": 4.76592e-4, "r": 6.03341e-8, "s": 3.86264e-6}  # 门 4 冻结值
EQ_W1B_MAX_COMPLETED = 24
DT_STEP = 1.0 / 60.0


def pose_of(parsed: dict, record: dict) -> list[tuple]:
    base = (parsed["header_size"] + record["index"] * parsed["record_size"]
            + parsed["meta_size"] + g.CLIP_CAPACITY * g.K5_CLIP.size)
    return [struct.unpack_from("<10f", parsed["blob"], base + t * g.TRACK_BYTES)
            for t in range(g.TRACK_COUNT)]


def pick_clock(rec: dict):
    """与 gate4 钟族优先级一致：首个 ∈ CLOCK_PRIORITY 且身份可读的 clip。"""
    for fam in g.CLOCK_PRIORITY:
        for c in rec["clips"]:
            if c["bare"] == fam and c["data_fnv"] != 0:
                return fam, c
    return None, None


def build_bursts(cand: dict) -> list[list[dict]]:
    """burst = 文件相邻、同钟族、同实例、同观测 local_time 的候选记录。"""
    for rec in cand["records"]:
        rec["fam"], rec["clock_clip"] = pick_clock(rec)
    bursts: list[list[dict]] = []
    cur: list[dict] = []
    key = None
    for rec in cand["records"]:
        if rec["fam"] is None:
            if cur:
                bursts.append(cur)
            cur, key = [], None
            continue
        c = rec["clock_clip"]
        k = (rec["fam"], *c["stable_id"], c["local_time"])
        if key is None:
            cur, key = [rec], k
        elif k == key:
            cur.append(rec)
        else:
            bursts.append(cur)
            cur, key = [rec], k
    if cur:
        bursts.append(cur)
    return bursts


def position_of(bursts: list[list[dict]]) -> dict[int, str]:
    pos = {}
    for b in bursts:
        n = len(b)
        for k, rec in enumerate(b):
            pos[rec["index"]] = ("single" if n == 1 else
                                 "m0" if k == 0 else "last" if k == n - 1 else "mid")
    return pos


def load_detail() -> dict[int, list]:
    blob = DETAIL.read_bytes()
    row = 16 + 4 * 4 * g.TRACK_COUNT
    out = {}
    for k in range(len(blob) // row):
        off = k * row
        _pidx, cand_record, _seq, _res = struct.unpack_from("<IIII", blob, off)
        out[cand_record] = struct.unpack_from("<" + "4f" * g.TRACK_COUNT, blob, off + 16)
    return out


def tau_counts(detail: dict, records: list[int]) -> tuple[int, int, int]:
    tp = tr = ts = 0
    for i in records:
        row = detail[i]
        for t in range(g.TRACK_COUNT):
            if row[4 * t + 0] > TAU["p"]:
                tp += 1
            if row[4 * t + 1] > TAU["r"]:
                tr += 1
            if row[4 * t + 3] > TAU["s"]:
                ts += 1
    return tp, tr, ts


def max_track_deg(pose_a, pose_b) -> tuple[float, int]:
    best = (-1.0, -1)
    for t, (ta, tb) in enumerate(zip(pose_a, pose_b)):
        _e_p, e_r, theta, _e_s = g.pose_metrics(ta, tb)
        if theta is not None and math.degrees(theta) > best[0]:
            best = (math.degrees(theta), t)
    return best


def main() -> int:
    cand = g.parse_candidate(str(CAND))
    src = g.parse_source(str(SRC))
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    pair_by_cand = {p["cand"]["record"]: p for p in manifest["pairs"]}
    src_by_seq = {r["seq"]: r for r in src["records"]}
    detail = load_detail()
    poses = {r["index"]: pose_of(cand, r) for r in cand["records"]}
    src_poses = {r["seq"]: pose_of(src, r) for r in src["records"]}
    bursts = build_bursts(cand)
    pos = position_of(bursts)

    print("== Q2 burst inventory ==")
    for fam in g.CLOCK_PRIORITY:
        blist = [b for b in bursts if b[0]["fam"] == fam]
        if not blist:
            continue
        sizes = [len(b) for b in blist]
        comps = [r["completed"] for b in blist for r in b]
        print(f"  {fam:20s} records={sum(sizes):3d} bursts={len(blist):2d} "
              f"ladder_eq={sum(sizes) - len(blist):3d} sizes={sizes} comp=[{comps[0]}..{comps[-1]}]")

    print("\n== Q1 within-burst consecutive pose deltas (selected) ==")
    for b in bursts:
        if len(b) < 4:
            continue
        fam = b[0]["fam"]
        lt = b[0]["clock_clip"]["local_time"]
        for j in range(len(b) - 1):
            ra, rb = b[j], b[j + 1]
            _e_p, e_r, theta, _e_s = g.pose_metrics(poses[ra["index"]][31], poses[rb["index"]][31])
            if theta is None:
                continue
            if j == 0 or j == len(b) - 2:
                print(f"  {fam:12s} lt={lt:.4f} c{ra['completed']:3d}->c{rb['completed']:3d} "
                      f"t31 drot={e_r:.3e} ({math.degrees(theta):.2f} deg)")

    print("\n== Q2 per-member theta vs paired source frame (selected bursts) ==")
    votes = {"first": 0, "last": 0, "other": 0}
    by_pos: dict[str, list[float]] = {"m0": [], "mid": [], "last": [], "single": []}
    for b in bursts:
        profile = []
        for rec in b:
            p = pair_by_cand.get(rec["index"])
            if p is None:
                continue
            theta, _t = max_track_deg(poses[rec["index"]], src_poses[p["src"]["seq"]])
            profile.append((rec["completed"], theta))
            by_pos[pos[rec["index"]]].append(theta)
        if len(profile) >= 2:
            thetas = [t for _c, t in profile]
            arg = thetas.index(min(thetas))
            votes["first" if arg == 0 else "last" if arg == len(thetas) - 1 else "other"] += 1
        if len(b) >= 11 and b[0]["fam"] != "1hm_idle":
            line = " ".join(f"m{k}(c{c}):{t:.2f}" for k, (c, t) in enumerate(profile))
            print(f"  {b[0]['fam']:12s} lt={b[0]['clock_clip']['local_time']:.4f} n={len(b)}  {line}")
    print(f"  argmin votes (multi-member bursts): {votes}")
    for name in ("m0", "mid", "last", "single"):
        vals = sorted(by_pos[name])
        if vals:
            print(f"  {name:6s} n={len(vals):3d} median={vals[len(vals) // 2]:.2f} deg "
                  f"p95={vals[int(len(vals) * 0.95)]:.2f} deg max={vals[-1]:.2f} deg")

    print("\n== Q3 tau decomposition (frozen tolerances) ==")
    print(f"  baseline (all {len(detail)} pairs): {tau_counts(detail, list(detail))}")
    for name in ("single", "m0", "mid", "last"):
        sel = [i for i in detail if pos.get(i) == name]
        print(f"  {name:7s} pairs={len(sel):3d} tau_p/tau_r/tau_s={tau_counts(detail, sel)}")
    endpoints = [i for i in detail if pos.get(i) in ("single", "last")]
    print(f"  option(a) endpoints={len(endpoints)} tau={tau_counts(detail, endpoints)}")
    comp_of = {r["index"]: r["completed"] for r in cand["records"]}
    w1b = [i for i in endpoints if comp_of[i] > EQ_W1B_MAX_COMPLETED]
    print(f"  option(a)+EQ-W1b     ={len(w1b)} tau={tau_counts(detail, w1b)}")

    decl1 = {30, 31, 58, 59, 45}
    decl2 = set(range(60, 67))
    cp_r: dict[int, int] = {}
    cp_p: dict[int, int] = {}
    for i in w1b:
        row = detail[i]
        for t in range(g.TRACK_COUNT):
            if row[4 * t + 1] > TAU["r"]:
                cp_r[t] = cp_r.get(t, 0) + 1
            if row[4 * t + 0] > TAU["p"]:
                cp_p[t] = cp_p.get(t, 0) + 1
    r_total, p_total = sum(cp_r.values()), sum(cp_p.values())
    d1 = sum(c for t, c in cp_r.items() if t in decl1)
    d2 = sum(c for t, c in cp_p.items() if t in decl2)
    print(f"  endpoint+W1b per-track: DECL1 tau_r={d1}/{r_total}, DECL2 tau_p={d2}/{p_total}")
    print(f"  residual tau_r top: {sorted(((t, c) for t, c in cp_r.items() if t not in decl1), key=lambda kv: -kv[1])[:8]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
