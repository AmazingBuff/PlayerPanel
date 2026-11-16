#!/usr/bin/env python3
"""C-G5-G4 v1 gate-4 pairing tool (docs/diagnostics/g5-gate4-20261115/README.md §十四).

Pairs candidate and source K5 pose artifacts (CPK5 v1 / CPK5S v2) by the frozen
structural rules: per-record clock-family priority, nearest-clock within
instance windows (wrap-split), IDUN record exclusion and a closed ledger
identity. Metadata-only by default: the 116x40-byte track region is never
sliced or decoded unless --tolerances is supplied (formal mode; only valid
after the complete gate-4 freeze fills the numeric slots). The tool emits
mechanical counts and distributions only — the G4P-7 verdict wording is
written by the run record, not by this tool.

Usage:
  python -I tools/gate4_pair.py --candidate <cpk5.bin> --source <cpk5s.bin> --out-dir <dir>
        [--candidate-json <ledger.json>] [--source-json <ledger.json>] [--tolerances <json>]

Exit codes: 0 ok; 2 = data INVALID (K5-7 out-of-set); 3 = parse/guard failure.
"""

import argparse
import hashlib
import json
import math
import os
import struct
import sys

K5_CANDIDATE_HEADER = struct.Struct("<4sHHIII")  # magic version header_size record_capacity track_capacity record_count
K5_SOURCE_HEADER = struct.Struct("<4sHHIIIIII")  # magic version header_size source record_capacity track_capacity record_size record_count
K5_CANDIDATE_META = struct.Struct("<IIIIIIQ")    # segment completed commanded track_count two_read_equal clip_count elapsed_raw
K5_SOURCE_META = struct.Struct("<QIHHBBBB")      # tsc seq track_count clip_count path two_read_equal flags pad
K5_CLIP = struct.Struct("<QQII4s64sIQQQI")       # node binding binding_index local_time mode+pad name duration data blockoff map_fnv map_count

TRACK_COUNT = 116
TRACK_BYTES = 40
CLIP_CAPACITY = 8
CAND_HEADER_SIZE = 116
CAND_RECORD_SIZE = K5_CANDIDATE_META.size + CLIP_CAPACITY * K5_CLIP.size + TRACK_COUNT * TRACK_BYTES  # 5664
SRC_HEADER_SIZE = 124
SRC_RECORD_SIZE = K5_SOURCE_META.size + CLIP_CAPACITY * K5_CLIP.size + TRACK_COUNT * TRACK_BYTES      # 5652

MANIFEST_BUDGET = 1 * 1024 * 1024
DETAIL_BUDGET = 8 * 1024 * 1024
SUMMARY_LINE_BUDGET = 200

CLOCK_PRIORITY = ("1hm_equip", "1hm_unequip", "1hm_idle", "mt_idle", "mlh_idle", "scar_1hmreadydummy")

# Dry-run anchor from the registered Track 2 metadata (§8; informational).
DRY_RUN_ANCHOR = {"1hm_equip": 96, "1hm_idle": 22, "1hm_unequip": 129}

# Track 2 same-resource set — verbatim from tools/k5_reconcile.py (registered in
# docs/diagnostics/g5-track2-20261114/README.md §15/§16; no declared-gap bucket
# in this environment, matching k5_reconcile --track2 semantics).
TRACK2_REGISTERED = {
    ("mt_idle", 0xF56EE1500EB5A3D0),
    ("1hm_equip", 0x12A77CD14ECC3688),
    ("1hm_unequip", 0x4DED5EAF23DF26E3),
    ("mlh_idle", 0xDAD1F1C997C0F0C6),
    ("1hm_idle", 0xC62C233B06FAE9A9),
    ("scar_1hmreadydummy", 0x8728A54A50D85C22),
}

PROTOCOL = "C-G5-G4 v1 (docs/diagnostics/g5-gate4-20261115/README.md)"


class ParseError(Exception):
    """Guard failure (exit 3)."""


class InvalidData(Exception):
    """Data INVALID per K5-7 (exit 2)."""


def clip_name(raw: bytes) -> str:
    return raw.rstrip(b"\x00").decode("utf-8", errors="replace")


def bare_clip_name(path_like: str) -> str:
    # Same normalization as tools/k5_reconcile.py: comparison-side only; the
    # resource identity stays the data FNV (K1-C4).
    leaf = path_like.replace("\\", "/").rsplit("/", 1)[-1]
    stem = leaf.rsplit(".", 1)[0] if "." in leaf else leaf
    return stem.strip().lower()


def load_coverage(path: str) -> dict:
    """Load the C-G5-G4-COV v1 99-primary/17-auxiliary split (frozen)."""
    with open(path, "r", encoding="utf-8") as handle:
        data = json.load(handle)
    if data.get("schema") != "C-G5-G4-COV v1":
        raise ParseError("coverage schema mismatch")
    primary = [int(value) for value in data.get("primary", [])]
    auxiliary = [int(value) for value in data.get("auxiliary", [])]
    if len(primary) != 99 or len(auxiliary) != 17:
        raise ParseError(f"coverage counts primary={len(primary)} auxiliary={len(auxiliary)}")
    if len(set(primary)) != len(primary) or len(set(auxiliary)) != len(auxiliary):
        raise ParseError("coverage contains duplicate track indices")
    if set(primary) & set(auxiliary):
        raise ParseError("coverage primary/auxiliary overlap")
    if set(primary) | set(auxiliary) != set(range(TRACK_COUNT)):
        raise ParseError("coverage union is not 0..115")
    groups = data.get("auxiliary_groups", {})
    item = [int(value) for value in groups.get("item", [])]
    wing = [int(value) for value in groups.get("wing", [])]
    belly = [int(value) for value in groups.get("belly", [])]
    if (len(item), len(wing), len(belly)) != (6, 10, 1):
        raise ParseError(f"coverage auxiliary groups item/wing/belly={len(item)}/{len(wing)}/{len(belly)}")
    if set(item) | set(wing) | set(belly) != set(auxiliary):
        raise ParseError("coverage auxiliary groups do not cover A17")
    return {
        "schema": data["schema"],
        "primary": primary,
        "auxiliary": auxiliary,
        "primary_set": set(primary),
        "auxiliary_set": set(auxiliary),
        "auxiliary_groups": {"item": item, "wing": wing, "belly": belly},
        "sha256": sha256_of(path),
    }


def sha256_of(path: str) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def clock_value(bits: int) -> float:
    value = struct.unpack("<f", struct.pack("<I", bits))[0]
    if not math.isfinite(value):
        raise ParseError(f"non-finite clip local_time bits={bits:#010x}")
    return value


def _read_clips(data: bytes, base: int, clip_count: int) -> list:
    clips = []
    for c in range(clip_count):
        coff = base + c * K5_CLIP.size
        fields = K5_CLIP.unpack_from(data, coff)
        node, binding, binding_index, local_bits = fields[0], fields[1], fields[2], fields[3]
        duration_bits, data_fnv, blockoff_fnv, map_fnv, map_count = fields[6:]
        clips.append({
            "stable_id": (node, binding, binding_index),
            "local_time": clock_value(local_bits),
            "name": clip_name(data[coff + 28: coff + 92]),
            "bare": bare_clip_name(clip_name(data[coff + 28: coff + 92])),
            "duration_bits": duration_bits,
            "data_fnv": data_fnv,
            "blockoff_fnv": blockoff_fnv,
            "map_fnv": map_fnv,
            "map_count": map_count,
        })
    return clips


def parse_candidate(path: str) -> dict:
    with open(path, "rb") as handle:
        data = handle.read()
    magic, version, header_size, capacity, track_cap, count = K5_CANDIDATE_HEADER.unpack_from(data, 0)
    if magic != b"CPK5":
        raise ParseError(f"candidate magic={magic!r} != CPK5")
    if header_size != CAND_HEADER_SIZE:
        raise ParseError(f"candidate header_size={header_size} != {CAND_HEADER_SIZE}")
    if len(data) != header_size + count * CAND_RECORD_SIZE:
        raise ParseError(f"candidate size {len(data)} != {header_size} + {count}x{CAND_RECORD_SIZE}")
    records = []
    for i in range(count):
        off = header_size + i * CAND_RECORD_SIZE
        seg, completed, _commanded, tracks, equal, clip_count, _elapsed = K5_CANDIDATE_META.unpack_from(data, off)
        if tracks != TRACK_COUNT:
            raise ParseError(f"candidate record {i}: track_count={tracks}")
        if equal != 1:
            raise ParseError(f"candidate record {i}: two_read_equal={equal}")
        if clip_count > CLIP_CAPACITY:
            raise ParseError(f"candidate record {i}: clip_count={clip_count}")
        records.append({
            "index": i,
            "segment": seg,
            "completed": completed,
            "clips": _read_clips(data, off + K5_CANDIDATE_META.size, clip_count),
        })
    identity = data[20:header_size].rstrip(b"\x00").decode("utf-8", errors="replace")
    return {"blob": data, "records": records, "count": count, "identity": identity,
            "version": version, "capacity": capacity, "record_size": CAND_RECORD_SIZE,
            "meta_size": K5_CANDIDATE_META.size, "header_size": header_size}


def parse_source(path: str) -> dict:
    with open(path, "rb") as handle:
        data = handle.read()
    fields = K5_SOURCE_HEADER.unpack_from(data, 0)
    magic, version, header_size = fields[0], fields[1], fields[2]
    capacity, track_cap, record_size_field, count = fields[4], fields[5], fields[6], fields[7]
    if magic != b"CPK5":
        raise ParseError(f"source magic={magic!r} != CPK5")
    if header_size != SRC_HEADER_SIZE:
        raise ParseError(f"source header_size={header_size} != {SRC_HEADER_SIZE}")
    if record_size_field != SRC_RECORD_SIZE:
        raise ParseError(f"source record_size field={record_size_field} != {SRC_RECORD_SIZE}")
    if len(data) != header_size + count * SRC_RECORD_SIZE:
        raise ParseError(f"source size {len(data)} != {header_size} + {count}x{SRC_RECORD_SIZE}")
    records = []
    for i in range(count):
        off = header_size + i * SRC_RECORD_SIZE
        tsc, seq, tracks, clip_count, _path, equal, flags, _pad = K5_SOURCE_META.unpack_from(data, off)
        if tracks != TRACK_COUNT:
            raise ParseError(f"source record seq={seq}: track_count={tracks}")
        if equal != 1:
            raise ParseError(f"source record seq={seq}: two_read_equal={equal}")
        if clip_count > CLIP_CAPACITY:
            raise ParseError(f"source record seq={seq}: clip_count={clip_count}")
        records.append({
            "index": i,
            "seq": seq,
            "tsc": tsc,
            "flags": flags,
            "clips": _read_clips(data, off + K5_SOURCE_META.size, clip_count),
        })
    identity = data[header_size - 92:header_size].rstrip(b"\x00").decode("utf-8", errors="replace")
    return {"blob": data, "records": records, "count": count, "identity": identity,
            "version": version, "capacity": capacity, "record_size": SRC_RECORD_SIZE,
            "meta_size": K5_SOURCE_META.size, "header_size": header_size}


def classify_side(parsed: dict) -> dict:
    """Frozen §14.3: out-of-set (readable identity outside the Track 2 set) ->
    InvalidData; any zero-FNV clip -> IDUN record exclusion; otherwise the
    per-record clock family comes from the frozen priority table."""
    eligible, idun, out_of_set = [], [], []
    for rec in parsed["records"]:
        bad = sorted({(c["bare"], c["data_fnv"]) for c in rec["clips"]
                      if c["data_fnv"] != 0 and (c["bare"], c["data_fnv"]) not in TRACK2_REGISTERED})
        if bad:
            out_of_set.append({"record": rec.get("index", rec.get("seq")), "identities": bad})
            continue
        if any(c["data_fnv"] == 0 for c in rec["clips"]):
            idun.append(rec)
            continue
        clock = None
        for fam in CLOCK_PRIORITY:
            if any(c["bare"] == fam for c in rec["clips"]):
                clock = fam
                break
        rec["clock"] = clock
        rec["families"] = sorted({c["bare"] for c in rec["clips"]})
        eligible.append(rec)
    if out_of_set:
        raise InvalidData(f"K5-7 out-of-set identities on {len(out_of_set)} record(s): "
                          f"{out_of_set[:4]}")
    return {"eligible": eligible, "idun": idun}


def _fam_clip(rec: dict, fam: str) -> dict:
    return next(c for c in rec["clips"] if c["bare"] == fam)


def build_windows(sightings: list, key_of) -> list:
    """Frozen §14.4.2: sightings are (record, clip) in side order; a window is
    a maximal run of file-adjacent sightings with the same stable-id; a cycle
    splits when local_time decreases (wrap)."""
    windows = []
    cur = None
    for rec, clip in sightings:
        if cur is not None:
            same_id = clip["stable_id"] == cur["stable_id"]
            adjacent = key_of(rec) == key_of(cur["last"]) + 1
        else:
            same_id, adjacent = False, False
        if cur is None or not (same_id and adjacent):
            cur = {"stable_id": clip["stable_id"], "cycles": [], "last": None}
            windows.append(cur)
        if not cur["cycles"] or clip["local_time"] < cur["cycles"][-1][-1][1]["local_time"]:
            cur["cycles"].append([])
        cur["cycles"][-1].append((rec, clip))
        cur["last"] = rec
    return windows


def pair_domain(fam: str, cand_recs: list, src_recs: list) -> dict:
    """Frozen §14.4.3: nearest-clock across the whole source domain (all
    windows/cycles); strict-improvement keeps the earliest seq on exact ties;
    x outside [min, max] of the source clocks -> edge-pair flag."""
    src_entries = [(rec, _fam_clip(rec, fam)) for rec in src_recs]
    pairs, no_partner = [], 0
    if not src_entries:
        return {"pairs": pairs, "no_partner": len(cand_recs), "edge_pairs": 0}
    src_clocks = [clip["local_time"] for _, clip in src_entries]
    min_c, max_c = min(src_clocks), max(src_clocks)
    for rec in cand_recs:
        clip = _fam_clip(rec, fam)
        x = clip["local_time"]
        best_d, best_pair = None, None
        for srec, sclip in src_entries:
            d = abs(sclip["local_time"] - x)
            if best_d is None or d < best_d:
                best_d, best_pair = d, (srec, sclip)
        srec, sclip = best_pair
        flags = []
        if x < min_c or x > max_c:
            flags.append("edge-pair")
        cand_only = sorted(set(rec["families"]) - set(srec["families"]))
        src_only = sorted(set(srec["families"]) - set(rec["families"]))
        pairs.append({
            "domain": fam,
            "cand": {"record": rec["index"], "segment": rec["segment"], "completed": rec["completed"]},
            "src": {"seq": srec["seq"]},
            "clock_x": x,
            "clock_c": sclip["local_time"],
            "dt_clock": abs(x - sclip["local_time"]),
            "cand_families": rec["families"],
            "src_families": srec["families"],
            "family_set_diff": {"cand_only": cand_only, "src_only": src_only},
            "flags": flags,
        })
    edge = sum(1 for p in pairs if p["flags"])
    return {"pairs": pairs, "no_partner": 0, "edge_pairs": edge}


def run_pairing(cand_parsed: dict, src_parsed: dict, pose_mode: bool = False,
                tolerances: dict = None, coverage: dict = None) -> dict:
    """Full pipeline over parsed artifacts. Returns the manifest dict (and
    attaches `detail_rows` for pose_mode). Pure function of its inputs."""
    cand_cls = classify_side(cand_parsed)
    src_cls = classify_side(src_parsed)

    domains = {}
    for fam in CLOCK_PRIORITY:
        cand_recs = [r for r in cand_cls["eligible"] if r["clock"] == fam]
        src_recs = [r for r in src_cls["eligible"] if r["clock"] == fam]
        cand_windows = build_windows([(r, _fam_clip(r, fam)) for r in cand_recs], lambda r: r["index"])
        src_windows = build_windows([(r, _fam_clip(r, fam)) for r in src_recs], lambda r: r["seq"])
        outcome = pair_domain(fam, cand_recs, src_recs)
        domains[fam] = {
            "candidate_records": len(cand_recs),
            "source_records": len(src_recs),
            "pairs": len(outcome["pairs"]),
            "edge_pairs": outcome["edge_pairs"],
            "no_partner": outcome["no_partner"],
            "wrap_cycles": {"candidate": [len(w["cycles"]) for w in cand_windows if len(w["cycles"]) > 1],
                            "source": [len(w["cycles"]) for w in src_windows if len(w["cycles"]) > 1]},
            "pairs_list": outcome["pairs"],
        }

    paired = sum(d["pairs"] for d in domains.values())
    no_clock = sum(1 for r in cand_cls["eligible"] if r["clock"] is None)
    no_partner = sum(d["no_partner"] for d in domains.values())
    cand_ledger = {
        "total": cand_parsed["count"],
        "idun_excluded": len(cand_cls["idun"]),
        "eligible": len(cand_cls["eligible"]),
        "paired": paired,
        "no_clock": no_clock,
        "no_partner": no_partner,
        "edge_pairs": sum(d["edge_pairs"] for d in domains.values()),
        "out_of_set": 0,
    }
    if cand_ledger["eligible"] != cand_ledger["paired"] + cand_ledger["no_clock"] + cand_ledger["no_partner"]:
        raise ParseError("candidate ledger identity does not close")
    src_hosted = sum(1 for fam in CLOCK_PRIORITY for p in domains[fam]["pairs_list"])
    used_src = {p["src"]["seq"] for fam in CLOCK_PRIORITY for p in domains[fam]["pairs_list"]}
    src_ledger = {
        "total": src_parsed["count"],
        "idun_excluded": len(src_cls["idun"]),
        "eligible": len(src_cls["eligible"]),
        "paired": src_hosted,
        "source_records_hosting_pairs": len(used_src),
        "out_of_set": 0,
    }

    manifest = {
        "schema": "C-G5-G4 manifest v1",
        "protocol": PROTOCOL,
        "pose_decoded": bool(pose_mode),
        "inputs": {
            "candidate": {"records": cand_parsed["count"], "identity": cand_parsed["identity"],
                          "version": cand_parsed["version"]},
            "source": {"records": src_parsed["count"], "identity": src_parsed["identity"],
                       "version": src_parsed["version"]},
        },
        "clock_priority": list(CLOCK_PRIORITY),
        "domains": {fam: {k: v for k, v in d.items() if k != "pairs_list"} for fam, d in domains.items()},
        "ledger": {"candidate": cand_ledger, "source": src_ledger},
        "pairs": [p for fam in CLOCK_PRIORITY for p in domains[fam]["pairs_list"]],
    }
    if coverage is None:
        manifest["coverage"] = {
            "schema": "legacy-116",
            "primary_count": TRACK_COUNT,
            "auxiliary_count": 0,
            "legacy_116": True,
            "primary": list(range(TRACK_COUNT)),
            "auxiliary": [],
        }
    else:
        manifest["coverage"] = {
            "schema": coverage["schema"],
            "primary_count": len(coverage["primary"]),
            "auxiliary_count": len(coverage["auxiliary"]),
            "legacy_116": False,
            "primary": list(coverage["primary"]),
            "auxiliary": list(coverage["auxiliary"]),
            "auxiliary_groups": coverage["auxiliary_groups"],
            "sha256": coverage["sha256"],
        }
    if pose_mode:
        manifest["detail_rows"] = compute_detail(cand_parsed, src_parsed, manifest["pairs"],
                                                 tolerances, coverage)
    return manifest


def _pose_base(parsed: dict, record: dict) -> int:
    return (parsed["header_size"] + record["index"] * parsed["record_size"]
            + parsed["meta_size"] + CLIP_CAPACITY * K5_CLIP.size)


def _quat_norm(q) -> float:
    n = q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]
    return math.sqrt(n) if n > 0.0 else 0.0


def pose_metrics(cand_track: tuple, src_track: tuple):
    """Frozen §14.5 detail metrics for one bone: returns (E_p, E_r, theta, E_s).
    Non-finite or zero-norm rotation -> (E_p, None, None, None); E_p itself is
    finite or flagged by the caller."""
    e_p = math.sqrt((cand_track[0] - src_track[0]) ** 2
                    + (cand_track[1] - src_track[1]) ** 2
                    + (cand_track[2] - src_track[2]) ** 2)
    e_r = theta = None
    ca, cs = cand_track[3:7], src_track[3:7]
    na, ns = _quat_norm(ca), _quat_norm(cs)
    if all(math.isfinite(v) for v in ca + cs) and na > 0.0 and ns > 0.0:
        dot = sum(ca[i] / na * cs[i] / ns for i in range(4))
        c = min(1.0, max(0.0, abs(dot)))
        e_r = 1.0 - c
        theta = 2.0 * math.acos(c)
    e_s = max(abs(cand_track[7] - src_track[7]),
              abs(cand_track[8] - src_track[8]),
              abs(cand_track[9] - src_track[9]))
    return e_p, e_r, theta, e_s


def compute_detail(cand_parsed: dict, src_parsed: dict, pairs: list, tolerances: dict,
                   coverage: dict = None):
    """Formal mode only: per-pair per-bone metrics + mechanical exceed counts.
    Row header is the frozen §14.5 layout: pair_index u32 / cand_record u32 /
    src_seq u32 / reserved u32. dt exclusion (dt_exclude=true) removes a pair
    from the distributions but keeps its rows (the flag is derivable from
    dt_clock vs epsilon_t, not packed into the row)."""
    src_by_seq = {}
    for rec in src_parsed["records"]:
        if rec["seq"] in src_by_seq:
            raise ParseError(f"duplicate source seq={rec['seq']}")
        src_by_seq[rec["seq"]] = rec
    detail = bytearray()
    invalid_bones_total = 0
    primary_set = set(range(TRACK_COUNT)) if coverage is None else coverage["primary_set"]
    auxiliary_set = set() if coverage is None else coverage["auxiliary_set"]
    primary_invalid = 0
    auxiliary_invalid = 0
    samples = {"E_p": [], "E_r": [], "E_s": []}
    auxiliary_samples = {"E_p": [], "E_r": [], "E_s": []}
    exceeded = {"tau_p": 0, "tau_r": 0, "tau_s": 0}
    dt_excluded_pairs = 0
    for pair_index, p in enumerate(pairs):
        cand_rec = cand_parsed["records"][p["cand"]["record"]]
        src_rec = src_by_seq.get(p["src"]["seq"])
        if src_rec is None:
            raise ParseError(f"pair {pair_index}: source seq={p['src']['seq']} not found")
        cb = cand_parsed["blob"][_pose_base(cand_parsed, cand_rec):
                                 _pose_base(cand_parsed, cand_rec) + TRACK_COUNT * TRACK_BYTES]
        sb = src_parsed["blob"][_pose_base(src_parsed, src_rec):
                                _pose_base(src_parsed, src_rec) + TRACK_COUNT * TRACK_BYTES]
        dt_excluded = bool(tolerances.get("dt_exclude")) and p["dt_clock"] > tolerances["epsilon_t"]
        if dt_excluded:
            dt_excluded_pairs += 1
        row = struct.pack("<IIII", pair_index, p["cand"]["record"], p["src"]["seq"], 0)
        row_invalid = 0
        for b in range(TRACK_COUNT):
            ca = struct.unpack_from("<10f", cb, b * TRACK_BYTES)
            cs = struct.unpack_from("<10f", sb, b * TRACK_BYTES)
            e_p, e_r, theta, e_s = pose_metrics(ca, cs)
            if e_r is None or not all(math.isfinite(v) for v in (e_p, e_s)):
                row_invalid += 1
                if b in primary_set:
                    primary_invalid += 1
                elif b in auxiliary_set:
                    auxiliary_invalid += 1
                row += struct.pack("<4f", e_p if math.isfinite(e_p) else float("nan"),
                                   float("nan"), float("nan"),
                                   e_s if math.isfinite(e_s) else float("nan"))
                continue
            row += struct.pack("<4f", e_p, e_r, theta, e_s)
            if not dt_excluded:
                if b in primary_set:
                    samples["E_p"].append(e_p)
                    samples["E_r"].append(e_r)
                    samples["E_s"].append(e_s)
                elif b in auxiliary_set:
                    auxiliary_samples["E_p"].append(e_p)
                    auxiliary_samples["E_r"].append(e_r)
                    auxiliary_samples["E_s"].append(e_s)
            if b in primary_set:
                if e_p > tolerances["tau_p"]:
                    exceeded["tau_p"] += 1
                if e_r > tolerances["tau_r"]:
                    exceeded["tau_r"] += 1
                if e_s > tolerances["tau_s"]:
                    exceeded["tau_s"] += 1
        detail += row
        invalid_bones_total += row_invalid
    if len(detail) > DETAIL_BUDGET:
        raise ParseError(f"detail artifact {len(detail)} B exceeds budget {DETAIL_BUDGET}")
    stats = {
        "invalid_bones_total": invalid_bones_total,
        "dt_excluded_pairs": dt_excluded_pairs,
        "exceeded": exceeded,
        "distributions": {k: _distribution(v) for k, v in samples.items()},
        "coverage": {
            "legacy_116": coverage is None,
            "primary_count": len(primary_set),
            "auxiliary_count": len(auxiliary_set),
        },
        "primary": {
            "invalid_bones_total": primary_invalid,
            "exceeded": exceeded,
            "distributions": {k: _distribution(v) for k, v in samples.items()},
        },
        "auxiliary": {
            "invalid_bones_total": auxiliary_invalid,
            "no_tolerance": True,
            "distributions": {k: _distribution(v) for k, v in auxiliary_samples.items()},
        },
    }
    return bytes(detail), stats


def _distribution(values: list) -> dict:
    if not values:
        return {"count": 0}
    ordered = sorted(values)

    def rank(p):
        return ordered[max(0, math.ceil(p * len(ordered)) - 1)]

    return {"count": len(ordered), "min": ordered[0], "median": rank(0.5),
            "p95": rank(0.95), "p99": rank(0.99), "max": ordered[-1]}


def cross_check_json(parsed: dict, json_path: str, side: str) -> dict:
    with open(json_path, "r", encoding="utf-8") as handle:
        ledger = json.load(handle)
    recorded = ledger.get("recorded")
    if recorded != parsed["count"]:
        raise ParseError(f"{side} JSON recorded={recorded} != bin count={parsed['count']}")
    return {"recorded": recorded, "identity_failed_clips": ledger.get("identity_failed_clips"),
            "drift_records": ledger.get("drift_records")}


def load_tolerances(path: str) -> dict:
    with open(path, "r", encoding="utf-8") as handle:
        tol = json.load(handle)
    for key in ("tau_p", "tau_r", "tau_s", "epsilon_t"):
        value = tol.get(key)
        if not isinstance(value, (int, float)) or not math.isfinite(value) or value <= 0:
            raise ParseError(f"tolerances.{key} must be a finite positive number")
    tol["dt_exclude"] = bool(tol.get("dt_exclude"))
    return tol


def render_summary(manifest: dict, stats: dict = None) -> str:
    lines = []
    cand, src = manifest["ledger"]["candidate"], manifest["ledger"]["source"]
    lines.append(f"G4P SUMMARY protocol={manifest['protocol']}")
    lines.append(f"G4P SUMMARY pose_decoded={manifest['pose_decoded']}")
    cov = manifest.get("coverage", {})
    lines.append(f"G4P COVERAGE primary={cov.get('primary_count')} "
                 f"auxiliary={cov.get('auxiliary_count')} "
                 f"legacy_116={'yes' if cov.get('legacy_116') else 'no'}")
    lines.append(f"G4P SUMMARY side=candidate total={cand['total']} idun={cand['idun_excluded']} "
                 f"eligible={cand['eligible']} paired={cand['paired']} no_clock={cand['no_clock']} "
                 f"no_partner={cand['no_partner']} edge_pairs={cand['edge_pairs']}")
    lines.append(f"G4P SUMMARY side=source total={src['total']} idun={src['idun_excluded']} "
                 f"eligible={src['eligible']} hosted_pairs={src['paired']} "
                 f"records_hosting={src['source_records_hosting_pairs']}")
    for fam in CLOCK_PRIORITY:
        d = manifest["domains"][fam]
        wc = ",".join(str(x) for x in d["wrap_cycles"]["candidate"]) or "-"
        ws = ",".join(str(x) for x in d["wrap_cycles"]["source"]) or "-"
        lines.append(f"G4P DOMAIN family={fam} cand={d['candidate_records']} src={d['source_records']} "
                     f"pairs={d['pairs']} no_partner={d['no_partner']} edge={d['edge_pairs']} "
                     f"wrap_cand={wc} wrap_src={ws}")
    for fam, expected in DRY_RUN_ANCHOR.items():
        actual = manifest["domains"][fam]["candidate_records"]
        lines.append(f"G4P ANCHOR family={fam} expected={expected} actual={actual} "
                     f"matched={'yes' if actual == expected else 'no'}")
    lines.append("G4P K5-7 out_of_set=0 verdict=PASS")
    if stats is not None:
        lines.append(f"G4P FORMAL invalid_bones={stats['invalid_bones_total']} "
                     f"dt_excluded_pairs={stats['dt_excluded_pairs']}")
        primary = stats.get("primary", {})
        aux = stats.get("auxiliary", {})
        if primary:
            lines.append(f"G4P PRIMARY invalid_bones={primary.get('invalid_bones_total', 0)} "
                         f"exceeded={primary.get('exceeded', {})}")
        if aux:
            lines.append(f"G4P AUX invalid_bones={aux.get('invalid_bones_total', 0)} "
                         f"no_tolerance=true")
        for key, exc in stats["exceeded"].items():
            lines.append(f"G4P FORMAL exceed {key}={exc}")
        for name, dist in stats["distributions"].items():
            if dist.get("count"):
                lines.append(f"G4P FORMAL dist {name} count={dist['count']} min={dist['min']:.6g} "
                             f"median={dist['median']:.6g} p95={dist['p95']:.6g} p99={dist['p99']:.6g} "
                             f"max={dist['max']:.6g}")
            else:
                lines.append(f"G4P FORMAL dist {name} count=0")
        for name, dist in aux.get("distributions", {}).items():
            if dist.get("count"):
                lines.append(f"G4P AUX dist {name} count={dist['count']} min={dist['min']:.6g} "
                             f"median={dist['median']:.6g} p95={dist['p95']:.6g} "
                             f"p99={dist['p99']:.6g} max={dist['max']:.6g}")
            else:
                lines.append(f"G4P AUX dist {name} count=0")
    if len(lines) > SUMMARY_LINE_BUDGET:
        raise ParseError(f"summary {len(lines)} lines exceeds budget {SUMMARY_LINE_BUDGET}")
    return "\n".join(lines) + "\n"


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--candidate", required=True, help="CPK5 v1 candidate artifact (.bin)")
    parser.add_argument("--source", required=True, help="CPK5S v2 source artifact (.bin)")
    parser.add_argument("--out-dir", required=True, help="absent output directory for artifacts")
    parser.add_argument("--candidate-json", help="candidate K5 JSON ledger (cross-check)")
    parser.add_argument("--source-json", help="source K5 JSON ledger (cross-check)")
    parser.add_argument("--coverage", help="C-G5-G4-COV v1 coverage JSON; required for formal pose mode")
    parser.add_argument("--tolerances", help="JSON {tau_p,tau_r,tau_s,epsilon_t,dt_exclude}; "
                                             "UNLOCKS pose decoding (formal mode, post-freeze only)")
    args = parser.parse_args(argv)

    try:
        cand_parsed = parse_candidate(args.candidate)
        src_parsed = parse_source(args.source)
        if args.candidate_json:
            cross_check_json(cand_parsed, args.candidate_json, "candidate")
        if args.source_json:
            cross_check_json(src_parsed, args.source_json, "source")
        coverage = load_coverage(args.coverage) if args.coverage else None
        if args.tolerances and coverage is None:
            raise ParseError("formal pose mode requires --coverage (legacy-116 is not a CAL1-B denominator)")
        tolerances = load_tolerances(args.tolerances) if args.tolerances else None
        manifest = run_pairing(cand_parsed, src_parsed, pose_mode=tolerances is not None,
                               tolerances=tolerances, coverage=coverage)
        manifest["inputs"]["candidate"]["sha256"] = sha256_of(args.candidate)
        manifest["inputs"]["source"]["sha256"] = sha256_of(args.source)
        if args.candidate_json:
            manifest["inputs"]["candidate"]["json_cross_check"] = "ok"
        if args.source_json:
            manifest["inputs"]["source"]["json_cross_check"] = "ok"
        detail_result = manifest.pop("detail_rows", None)
        manifest_bytes = json.dumps(manifest, indent=1, ensure_ascii=False).encode("utf-8")
        if len(manifest_bytes) > MANIFEST_BUDGET:
            raise ParseError(f"manifest {len(manifest_bytes)} B exceeds budget {MANIFEST_BUDGET}")
        os.makedirs(args.out_dir, exist_ok=True)
        with open(os.path.join(args.out_dir, "gate4-pair-manifest.json"), "wb") as handle:
            handle.write(manifest_bytes)
        stats = None
        if detail_result is not None:
            detail, stats = detail_result
            with open(os.path.join(args.out_dir, "gate4-pair-detail.bin"), "wb") as handle:
                handle.write(detail)
        summary = render_summary(manifest, stats)
        with open(os.path.join(args.out_dir, "gate4-pair-summary.txt"), "w",
                  encoding="utf-8", newline="\n") as handle:
            handle.write(summary)
        sys.stdout.write(summary)
        return 0
    except InvalidData as error:
        print(f"[K5-7 INVALID] {error}", file=sys.stderr)
        return 2
    except ParseError as error:
        print(f"[GUARD FAIL] {error}", file=sys.stderr)
        return 3


if __name__ == "__main__":
    sys.exit(main())
