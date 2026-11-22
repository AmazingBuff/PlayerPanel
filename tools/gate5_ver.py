"""C-G5-VER v1 §九：新正式验证的配对/比较/分账工具（2026-11-22 冻结实现）。

消费两次独立会话的归档工件（候选 CPK5 + 源 CPK5S），按冻结规则执行：
burst 分组 → 末步比较单元 → 排除窗（clip 末段饱和 / EQ-W1b / EQ-W1a / dt 门）
→ nearest-clock 端点配对 → 逐轨 pose 度量 → 等值主张集机械计数 + DECL 分账。

协议：docs/diagnostics/g5-verify-protocol-20261121/README.md §三/§九。
只 import 复用 tools/gate4_pair.py 的解析与度量（其本体冻结不动）。
纯 stdlib、零第三方依赖、零网络、只读输入；工具不输出 G5 级裁定语。

用法：
  python -I tools/gate5_ver.py --candidate <cpk5.bin> --source <cpk5s.bin> --out-dir <dir>
        [--tolerances <json>]
  python -I tools/gate5_ver.py --self-test
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import struct
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS))

import gate4_pair as g  # noqa: E402

PROTOCOL = "C-G5-VER v1 (frozen 2026-11-22)"

# §一/§三 冻结的声明集与主轨集（轨号 = animationSkeleton->bones[NN]，见 K1 落名登记）。
DECL1_ROT = frozenset({30, 31, 58, 59, 45})        # EQ-DECL-1：rotation 声明；t45 另含 translation
DECL1_POS_ONLY = frozenset({30, 31, 58, 59})       # 其中 translation 判
DECL2_POS = frozenset(range(60, 67))               # EQ-DECL-2：translation 声明；rotation 判
MAIN_TRACK_MAX = 98                                # 99 主轨 = 0..98；99..115 = A17 描述性账本
PRIMARY_FAMS = frozenset({"1hm_equip", "1hm_unequip", "1hm_idle", "mt_idle"})
SATURATION_FRAME_S = 1.5 / 60.0                    # clip 末段饱和判据的帧宽
W1B_MAX_COMPLETED = 24                             # EQ-W1b 候选侧 completed 窗上界
DT_EXCLUDE_CEIL = 0.02                             # TIME v1：dt 排除比例上限（edge-only）


class VerInvalid(Exception):
    """数据判 INVALID（退出码 2）。"""


def channel_judged(track: int, channel: str) -> bool:
    """§一 冻结算术：等值主张集的逐轨逐通道判定资格。"""
    if track > MAIN_TRACK_MAX:
        return False  # A17 辅助轨：描述性照记，不判
    if channel == "rot":
        return track not in DECL1_ROT
    if channel == "pos":
        return track not in DECL2_POS and track != 45
    if channel == "scale":
        return track not in DECL1_ROT and track not in DECL2_POS
    raise ValueError(channel)


def pose_of(parsed: dict, record: dict) -> list[tuple]:
    base = (parsed["header_size"] + record["index"] * parsed["record_size"]
            + parsed["meta_size"] + g.CLIP_CAPACITY * g.K5_CLIP.size)
    return [struct.unpack_from("<10f", parsed["blob"], base + t * g.TRACK_BYTES)
            for t in range(g.TRACK_COUNT)]


def pick_clock(rec: dict):
    for fam in g.CLOCK_PRIORITY:
        for c in rec["clips"]:
            if c["bare"] == fam and c["data_fnv"] != 0:
                return fam, c
    return None, None


def prepare(parsed: dict) -> None:
    for rec in parsed["records"]:
        rec["fam"], rec["clock_clip"] = pick_clock(rec)
        rec["idun"] = (not rec["clips"]) or any(c["data_fnv"] == 0 for c in rec["clips"])
        rec["primary_fams"] = frozenset(
            c["bare"] for c in rec["clips"] if c["data_fnv"] != 0 and c["bare"] in PRIMARY_FAMS)


def build_bursts(cand: dict) -> tuple[list[list[dict]], int]:
    """burst = 文件相邻 + 同钟族 + 同实例（stable_id）+ 同观测 local_time。"""
    bursts: list[list[dict]] = []
    cur: list[dict] = []
    key = None
    no_clock = 0
    for rec in cand["records"]:
        if rec["fam"] is None:
            no_clock += 1
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
    return bursts, no_clock


def duration_of(clip: dict) -> float:
    bits = clip.get("duration_bits", 0)
    value = struct.unpack("<f", struct.pack("<I", bits & 0xFFFFFFFF))[0]
    return value if math.isfinite(value) else 0.0


def classify_bursts(bursts: list[list[dict]]) -> tuple[list[dict], dict]:
    """逐 burst 分类；比较单元 = burst 末步（单记录 burst 自身）。

    entry 字段：kind / unit_record / unit_completed / domain / clock / duration /
    burst_size / burst_records / completed_range / mid_burst_records。
    同域 burst 时钟非递减守卫：回归 → INVALID（本批零 wrap）。
    """
    entries: list[dict] = []
    counters = {"excluded_saturation_bursts": 0, "excluded_w1b_bursts": 0,
                "excluded_idun_units": 0, "endpoints": 0}
    last_clock_by_domain: dict[str, float] = {}
    for burst in bursts:
        fam = burst[0]["fam"]
        clip = burst[0]["clock_clip"]
        clock = clip["local_time"]
        prev = last_clock_by_domain.get(fam)
        if prev is not None and clock < prev - 1e-9:
            raise VerInvalid(f"domain {fam}: burst clock regressed {prev:.6f} -> {clock:.6f}")
        last_clock_by_domain[fam] = clock
        dur = duration_of(clip)
        saturated = len(burst) >= 2 and dur > 0.0 and clock >= dur - SATURATION_FRAME_S
        w1b = any(r["completed"] <= W1B_MAX_COMPLETED for r in burst)
        unit = burst[-1]
        if saturated:
            kind, counter = "excluded_saturation", "excluded_saturation_bursts"
        elif w1b:
            kind, counter = "excluded_w1b", "excluded_w1b_bursts"
        elif unit["idun"]:
            kind, counter = "excluded_idun", "excluded_idun_units"
        else:
            kind, counter = "endpoint", "endpoints"
        counters[counter] += 1
        entries.append({
            "kind": kind,
            "unit_record": unit["index"],
            "unit_completed": unit["completed"],
            "domain": fam,
            "clock": clock,
            "duration": dur,
            "burst_size": len(burst),
            "burst_records": [r["index"] for r in burst],
            "completed_range": [min(r["completed"] for r in burst),
                                max(r["completed"] for r in burst)],
            "mid_burst_records": [r["index"] for r in burst[:-1]],
        })
    return entries, counters


def pair_endpoints(entries: list[dict], src: dict) -> list[dict]:
    """nearest-clock 端点配对（§3.4 语义：域内全部非 IDUN 源记录取 |c−x| 最小，等距取 seq 早）。

    返回值 = endpoint entries（含 no-partner）；paired entry 增设
    pair / clock_x / dt_clock / pair_flags / src_dual。
    """
    by_domain: dict[str, list[dict]] = {}
    for rec in src["records"]:
        if rec["fam"] is None or rec["idun"]:
            continue
        by_domain.setdefault(rec["fam"], []).append(rec)
    domain_span = {fam: (min(r["clock_clip"]["local_time"] for r in recs),
                         max(r["clock_clip"]["local_time"] for r in recs))
                   for fam, recs in by_domain.items()}
    pairs = []
    for entry in entries:
        if entry["kind"] != "endpoint":
            continue
        fam = entry["domain"]
        x = entry["clock"]
        candidates = by_domain.get(fam, [])
        if not candidates:
            entry["pair"] = None
            entry["pair_flags"] = ["no-partner"]
            pairs.append(entry)
            continue
        best = None
        for rec in candidates:
            sc = rec["clock_clip"]["local_time"]
            d = abs(x - sc)
            if best is None or d < best[0] - 1e-12 or (abs(d - best[0]) <= 1e-12
                                                       and rec["seq"] < best[1]["seq"]):
                best = (d, rec)
        src_rec = best[1]
        lo, hi = domain_span[fam]
        flags = []
        if x < lo - 1e-12 or x > hi + 1e-12:
            flags.append("edge-pair")
        entry["pair"] = src_rec
        entry["pair_flags"] = flags
        entry["clock_x"] = src_rec["clock_clip"]["local_time"]
        entry["dt_clock"] = abs(x - entry["clock_x"])
        entry["src_dual"] = len(src_rec["primary_fams"]) >= 2
        pairs.append(entry)
    return pairs


def summarize_values(values: list[float]) -> dict:
    if not values:
        return {"n": 0}
    ordered = sorted(values)
    return {"n": len(values), "min": ordered[0], "median": ordered[len(ordered) // 2],
            "p95": ordered[min(len(ordered) - 1, int(len(ordered) * 0.95))], "max": ordered[-1]}


def execute(cand_path: str, src_path: str, out_dir: str, tol_path: str | None) -> int:
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    cand = g.parse_candidate(cand_path)
    src = g.parse_source(src_path)
    prepare(cand)
    prepare(src)
    bursts, no_clock = build_bursts(cand)
    entries, burst_counters = classify_bursts(bursts)
    pairs = pair_endpoints(entries, src)
    paired = [e for e in pairs if e.get("pair") is not None]

    tolerances = None
    if tol_path:
        tolerances = g.load_tolerances(tol_path)
        for e in paired:
            if e["dt_clock"] > tolerances["epsilon_t"]:
                e["pair_flags"].append("dt-excluded")
        dt_excluded = [e for e in paired if "dt-excluded" in e["pair_flags"]]
        if dt_excluded:
            non_edge = [e for e in dt_excluded if "edge-pair" not in e["pair_flags"]]
            if non_edge:
                raise VerInvalid(f"dt-excluded non-edge pairs: {len(non_edge)}")
            if len(dt_excluded) > DT_EXCLUDE_CEIL * len(paired):
                raise VerInvalid(
                    f"dt-excluded ratio {len(dt_excluded)}/{len(paired)} exceeds {DT_EXCLUDE_CEIL:.0%}")
    else:
        dt_excluded = []
    w1a_excluded = [e for e in paired if e.get("src_dual") and "dt-excluded" not in e["pair_flags"]]
    counted = [e for e in paired
               if "dt-excluded" not in e["pair_flags"] and not e.get("src_dual")]

    pose_decoded = tolerances is not None
    detail_rows: list[list[tuple]] = []
    accounting = None
    if pose_decoded:
        tau = {"p": 0, "r": 0, "s": 0}
        invalid_components = 0
        per_track = {t: {"p": 0, "r": 0, "s": 0} for t in range(g.TRACK_COUNT)}
        decl_measured: dict[str, list[float]] = {}
        dt_values = [e["dt_clock"] for e in paired]
        for e in paired:
            cand_rec = next(r for r in cand["records"] if r["index"] == e["unit_record"])
            cp = pose_of(cand, cand_rec)
            sp = pose_of(src, e["pair"])
            row = []
            dt_skip = "dt-excluded" in e["pair_flags"]
            judged_eq = not e.get("src_dual")
            for t in range(g.TRACK_COUNT):
                e_p, e_r, theta, e_s = g.pose_metrics(cp[t], sp[t])
                row.append((e_p, e_r, theta, e_s))
                if dt_skip:
                    continue
                if t <= MAIN_TRACK_MAX:
                    if e_p is not None and e_p > tolerances["tau_p"] and judged_eq \
                            and channel_judged(t, "pos"):
                        tau["p"] += 1
                        per_track[t]["p"] += 1
                    if e_r is None or theta is None:
                        invalid_components += 1
                    elif e_r > tolerances["tau_r"] and judged_eq and channel_judged(t, "rot"):
                        tau["r"] += 1
                        per_track[t]["r"] += 1
                    if e_s is not None and e_s > tolerances["tau_s"] and judged_eq \
                            and channel_judged(t, "scale"):
                        tau["s"] += 1
                        per_track[t]["s"] += 1
                decl_key = f"t{t}_rot" if t in DECL1_ROT else (
                    f"t{t}_pos" if t in DECL2_POS else None)
                if decl_key is not None:
                    if decl_key.endswith("_rot"):
                        value = math.degrees(theta) if theta is not None else None
                    else:
                        value = e_p
                    if value is not None and math.isfinite(value):
                        decl_measured.setdefault(decl_key, []).append(round(value, 6))
            detail_rows.append(row)
        accounting = {
            "equivalence": {"tau_p": tau["p"], "tau_r": tau["r"], "tau_s": tau["s"],
                            "invalid_components": invalid_components,
                            "counted_pairs": len(counted)},
            "per_track_exceed": {f"t{t}": v for t, v in per_track.items()
                                 if v["p"] or v["r"] or v["s"]},
            "decl_measured": {k: summarize_values(v) for k, v in sorted(decl_measured.items())},
            "dt_clock": summarize_values(dt_values),
        }

    manifest = build_manifest(cand_path, src_path, cand, src, entries, burst_counters,
                              no_clock, pairs, tolerances, pose_decoded, accounting)
    manifest_bytes = json.dumps(manifest, ensure_ascii=False, sort_keys=True,
                                separators=(",", ":")).encode("utf-8")
    if len(manifest_bytes) > g.MANIFEST_BUDGET:
        print("gate5-ver: manifest budget exceeded", file=sys.stderr)
        return 3
    (out / "gate5-ver-manifest.json").write_bytes(manifest_bytes)

    summary_lines = render_summary(manifest)
    if len(summary_lines) > g.SUMMARY_LINE_BUDGET:
        print("gate5-ver: summary budget exceeded", file=sys.stderr)
        return 3
    (out / "gate5-ver-summary.txt").write_text("\n".join(summary_lines) + "\n", encoding="utf-8")

    if pose_decoded:
        parts = []
        for i, (e, row) in enumerate(zip(paired, detail_rows)):
            head = struct.pack("<IIII", i, e["unit_record"], e["pair"]["seq"], 0)
            cells = []
            for e_p, e_r, theta, e_s in row:
                if all(v is not None for v in (e_p, e_r, theta, e_s)):
                    cells.append(struct.pack("<4f", e_p, e_r, theta, e_s))
                else:
                    cells.append(struct.pack("<4f", float("nan"), float("nan"),
                                             float("nan"), float("nan")))
            parts.append(head + b"".join(cells))
        blob = b"".join(parts)
        if len(blob) > g.DETAIL_BUDGET:
            print("gate5-ver: detail budget exceeded", file=sys.stderr)
            return 3
        (out / "gate5-ver-detail.bin").write_bytes(blob)
    print(f"gate5-ver: manifest={len(manifest_bytes)}B summary={len(summary_lines)}L "
          f"endpoints={len(pairs)} paired={len(paired)} "
          f"counted={len(counted) if pose_decoded else 'n/a'}")
    return 0


def build_manifest(cand_path, src_path, cand, src, entries, burst_counters, no_clock,
                   pairs, tolerances, pose_decoded, accounting) -> dict:
    def input_block(path, parsed):
        return {"path": str(path), "bytes": Path(path).stat().st_size,
                "sha256": hashlib.sha256(Path(path).read_bytes()).hexdigest(),
                "records": parsed["count"], "identity": parsed["identity"]}

    ledger_cand = {
        "total": cand["count"], "no_clock": no_clock,
        "mid_burst_records": sum(len(e["mid_burst_records"]) for e in entries),
        "endpoints": sum(1 for e in entries if e["kind"] == "endpoint"),
        "excluded_saturation_bursts": burst_counters["excluded_saturation_bursts"],
        "excluded_w1b_bursts": burst_counters["excluded_w1b_bursts"],
        "excluded_idun_units": burst_counters["excluded_idun_units"],
    }
    burst_records_total = sum(e["burst_size"] for e in entries)
    ledger_cand["closure"] = bool(ledger_cand["total"] - ledger_cand["no_clock"]
                                  == burst_records_total)
    pair_ledger = {
        "paired": sum(1 for e in pairs if e.get("pair") is not None),
        "no_partner": sum(1 for e in pairs if e.get("pair") is None),
        "edge_pairs": sum(1 for e in pairs if "edge-pair" in e.get("pair_flags", [])),
        "dt_excluded": sum(1 for e in pairs if "dt-excluded" in e.get("pair_flags", [])),
        "w1a_excluded": sum(1 for e in pairs if e.get("src_dual")),
    }
    if pose_decoded and accounting:
        pair_ledger["equivalence_counted"] = accounting["equivalence"]["counted_pairs"]
    pair_rows = [{
        "index": i, "kind": e["kind"], "domain": e["domain"],
        "unit_record": e["unit_record"], "unit_completed": e["unit_completed"],
        "burst_size": e["burst_size"], "clock_c": e["clock"],
        "src_seq": e["pair"]["seq"] if e.get("pair") else None,
        "clock_x": e.get("clock_x"), "dt_clock": e.get("dt_clock"),
        "flags": e.get("pair_flags", []),
    } for i, e in enumerate(pairs)]
    return {
        "schema": "gate5-ver-manifest/1", "protocol": PROTOCOL,
        "pose_decoded": pose_decoded,
        "inputs": {"candidate": input_block(cand_path, cand),
                   "source": input_block(src_path, src)},
        "constants": {"decl1_rot": sorted(DECL1_ROT),
                      "decl1_pos_only": sorted(DECL1_POS_ONLY),
                      "decl2_pos": sorted(DECL2_POS), "main_track_max": MAIN_TRACK_MAX,
                      "primary_fams": sorted(PRIMARY_FAMS),
                      "saturation_frame_s": SATURATION_FRAME_S,
                      "w1b_max_completed": W1B_MAX_COMPLETED,
                      "dt_exclude_ceil": DT_EXCLUDE_CEIL},
        "tolerances": (None if not tolerances else
                       {k: tolerances[k] for k in ("tau_p", "tau_r", "tau_s", "epsilon_t")}),
        "ledger": {"candidate": ledger_cand, "source": {"total": src["count"]},
                   "pairing": pair_ledger},
        "pairs": pair_rows,
        "accounting": accounting,
    }


def render_summary(manifest: dict) -> list[str]:
    lines = ["G5VER SUMMARY", f"protocol={manifest['protocol']}"]
    led = manifest["ledger"]
    lines.append(f"cand total={led['candidate']['total']} no_clock={led['candidate']['no_clock']} "
                 f"mid_burst={led['candidate']['mid_burst_records']} "
                 f"endpoints={led['candidate']['endpoints']} "
                 f"closure={'OK' if led['candidate']['closure'] else 'BROKEN'}")
    lines.append(f"saturation_bursts={led['candidate']['excluded_saturation_bursts']} "
                 f"w1b_bursts={led['candidate']['excluded_w1b_bursts']} "
                 f"idun_units={led['candidate']['excluded_idun_units']}")
    pr = led["pairing"]
    lines.append(f"pairs={pr['paired']} no_partner={pr['no_partner']} edge={pr['edge_pairs']} "
                 f"dt_excluded={pr['dt_excluded']} w1a_excluded={pr['w1a_excluded']}"
                 + (f" counted={pr['equivalence_counted']}"
                    if "equivalence_counted" in pr else ""))
    acc = manifest.get("accounting")
    if acc:
        eq = acc["equivalence"]
        lines.append(f"equivalence counted_pairs={eq['counted_pairs']} "
                     f"tau_p={eq['tau_p']} tau_r={eq['tau_r']} tau_s={eq['tau_s']} "
                     f"invalid_components={eq['invalid_components']}")
        dt = acc["dt_clock"]
        if dt.get("n"):
            lines.append(f"dt_clock n={dt['n']} median={dt['median']:.6f} max={dt['max']:.6f}")
        for key in sorted(acc.get("decl_measured") or {}):
            d = acc["decl_measured"][key]
            if d.get("n"):
                lines.append(f"decl {key} n={d['n']} median={d['median']:.4g} max={d['max']:.4g}")
        top = sorted(acc.get("per_track_exceed", {}).items(),
                     key=lambda kv: -(kv[1]["p"] + kv[1]["r"] + kv[1]["s"]))[:12]
        for track, v in top:
            lines.append(f"exceed {track} p={v['p']} r={v['r']} s={v['s']}")
    return lines


def main(argv: list[str] | None = None) -> int:
    try:
        return run(argv)
    except g.ParseError as exc:
        print(f"gate5-ver: parse/guard failure: {exc}", file=sys.stderr)
        return 3
    except VerInvalid as exc:
        print(f"gate5-ver: INVALID: {exc}", file=sys.stderr)
        return 2


def run(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="C-G5-VER v1 pairing/verdict tool")
    parser.add_argument("--candidate")
    parser.add_argument("--source")
    parser.add_argument("--out-dir")
    parser.add_argument("--tolerances")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args(argv)
    if args.self_test:
        import gate5_ver_test as test
        return test.run_all()
    if not args.candidate or not args.source or not args.out_dir:
        parser.error("--candidate/--source/--out-dir are required")
    return execute(args.candidate, args.source, args.out_dir, args.tolerances)


if __name__ == "__main__":
    sys.exit(main())
