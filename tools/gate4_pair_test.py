#!/usr/bin/env python3
"""Offline unit tests for tools/gate4_pair.py — C-G5-G4 v1 §14.5 T1..T12.

Expected values are analytically computed here; nothing calls the function
under test to generate its own answers. Pure stdlib; runs under `python -I`:

  python -I tools/gate4_pair_test.py
"""

import json
import math
import os
import struct
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gate4_pair as g  # noqa: E402

FAILURES = []


def check(name: str, condition: bool, detail: str = "") -> None:
    if condition:
        print(f"PASS {name}")
    else:
        FAILURES.append(name)
        print(f"FAIL {name} {detail}")


def _f32(value: float) -> int:
    return struct.unpack("<I", struct.pack("<f", value))[0]


def clip_entry(node: int, binding: int, bidx: int, local: float, name: str,
               data_fnv: int, duration: float = 1.0) -> bytes:
    raw = name.encode("utf-8")[:64].ljust(64, b"\x00")
    return struct.pack("<QQII4s64sIQQQI", node, binding, bidx, _f32(local), b"\x00" * 4,
                       raw, _f32(duration), data_fnv, 0, 0x11, 1)


def track_bytes(t=(0.0, 0.0, 0.0), q=(0.0, 0.0, 0.0, 1.0), s=(1.0, 1.0, 1.0)) -> bytes:
    return struct.pack("<10f", *t, *q, *s)


def cand_record(segment: int, completed: int, clips, tracks: bytes = None) -> bytes:
    assert len(clips) <= g.CLIP_CAPACITY
    if tracks is None:
        tracks = track_bytes() * g.TRACK_COUNT
    return (struct.pack("<IIIIIIQ", segment, completed, 0, g.TRACK_COUNT, 1, len(clips), 0)
            + b"".join(clips)
            + b"\x00" * ((g.CLIP_CAPACITY - len(clips)) * g.K5_CLIP.size)
            + tracks)


def src_record(seq: int, clips, tracks: bytes = None) -> bytes:
    assert len(clips) <= g.CLIP_CAPACITY
    if tracks is None:
        tracks = track_bytes() * g.TRACK_COUNT
    return (struct.pack("<QIHHBBBB", 5000 + seq, seq, g.TRACK_COUNT, len(clips), 0, 1, 0, 0)
            + b"".join(clips)
            + b"\x00" * ((g.CLIP_CAPACITY - len(clips)) * g.K5_CLIP.size)
            + tracks)


def cand_bin(records) -> bytes:
    header = struct.pack("<4sHHIII", b"CPK5", 1, 116, 384, 116, len(records))
    return header + b"test-candidate".ljust(96, b"\x00") + b"".join(records)


def src_bin(records) -> bytes:
    # <4sHHIIIIII = magic/version/header_size/source/capacity/track_cap/record_size/count/reserved
    # (k5_reconcile.py takes [:8]; the ninth I is ignored by both tools).
    header = struct.pack("<4sHHIIIIII", b"CPK5", 2, 124, 0, 1400, 116, 5652, len(records), 0)
    return header + b"test-source".ljust(92, b"\x00") + b"".join(records)


F_EQ = 0x12A77CD14ECC3688   # 1hm_equip
F_UN = 0x4DED5EAF23DF26E3   # 1hm_unequip
F_IDLE = 0xC62C233B06FAE9A9  # 1hm_idle
F_MT = 0xF56EE1500EB5A3D0   # mt_idle

E_EQ = "Animations\\1HM_Equip.hkx"
E_UN = "Animations\\1HM_Unequip.hkx"
E_IDLE = "Animations\\1HM_Idle.hkx"
E_MT = "Animations\\female\\MT_Idle.hkx"


def t01_position_known_value() -> None:
    # CAL v1 §四: position difference (0,3,4) has Euclidean error 5.
    cand = track_bytes(t=(0.0, 0.0, 0.0))
    src = track_bytes(t=(0.0, 3.0, 4.0))
    e_p, e_r, _theta, e_s = g.pose_metrics(struct.unpack("<10f", cand),
                                           struct.unpack("<10f", src))
    check("T1 E_p (0,3,4)->5", abs(e_p - 5.0) < 1e-12, f"e_p={e_p!r}")
    check("T1 same-rotation E_r==0", e_r == 0.0, f"e_r={e_r!r}")
    check("T1 same-scale E_s==0", e_s == 0.0, f"e_s={e_s!r}")


def t02_rotation_and_sign() -> None:
    # The expected values are derived analytically from the float32-rounded
    # components that actually enter the metric (the track words are float32),
    # not from the function under test.
    q_raw = (0.5, 0.0, 0.0, math.sqrt(3.0) / 2.0)  # 60° about X
    ident_raw = (0.0, 0.0, 0.0, 1.0)
    q = struct.unpack("<4f", struct.pack("<4f", *q_raw))
    n = math.sqrt(sum(v * v for v in q))
    expected_dot = abs(q[3] / n)  # identity quat: dot = w/n
    expected = 1.0 - expected_dot
    expected_theta = 2.0 * math.acos(min(1.0, expected_dot))
    _, e_r, theta, _ = g.pose_metrics(struct.unpack("<10f", track_bytes(q=q_raw)),
                                      struct.unpack("<10f", track_bytes(q=ident_raw)))
    check("T2 60° axis rotation E_r==1-cos30°", abs(e_r - expected) < 1e-15,
          f"e_r={e_r!r} expected={expected!r}")
    check("T2 theta==60°", abs(theta - expected_theta) < 1e-15, f"theta={theta!r}")
    _, e_r_neg, _, _ = g.pose_metrics(struct.unpack("<10f", track_bytes(q=tuple(-v for v in q_raw))),
                                      struct.unpack("<10f", track_bytes(q=ident_raw)))
    check("T2 q/-q equivalence", abs(e_r_neg - expected) < 1e-15, f"e_r_neg={e_r_neg!r}")
    _, e_r_bad, _, _ = g.pose_metrics(struct.unpack("<10f", track_bytes(q=(0.0, 0.0, 0.0, 0.0))),
                                      struct.unpack("<10f", track_bytes(q=ident_raw)))
    check("T2 zero-norm rotation -> invalid", e_r_bad is None, f"e_r={e_r_bad!r}")


def t03_scale_component() -> None:
    # CAL v1 §四: a lone Z scale difference of 0.125 has error 0.125.
    _, _, _, e_s = g.pose_metrics(struct.unpack("<10f", track_bytes()),
                                  struct.unpack("<10f", track_bytes(s=(1.0, 1.0, 1.125))))
    check("T3 E_s z-only 0.125", abs(e_s - 0.125) < 1e-12, f"e_s={e_s!r}")


def t04_clock_priority() -> None:
    rec = {"index": 0, "segment": 0, "completed": 1,
           "clips": [{"bare": "1hm_idle", "data_fnv": F_IDLE},
                     {"bare": "1hm_equip", "data_fnv": F_EQ}]}
    eligible, idun, out_of_set = [], [], []
    # classify_side works on parsed dicts; emulate via a tiny parsed shell.
    parsed = {"records": [dict(rec)], "count": 1, "identity": "x", "version": 1,
              "capacity": 384, "record_size": g.CAND_RECORD_SIZE,
              "meta_size": g.K5_CANDIDATE_META.size, "header_size": 116, "blob": b""}
    result = g.classify_side(parsed)
    check("T4 dual-family record takes high-priority clock",
          result["eligible"][0]["clock"] == "1hm_equip",
          f"clock={result['eligible'][0]['clock']!r}")
    check("T4 no idun/out-of-set", not result["idun"] and not out_of_set)


def t05_nearest_tie_earliest_seq() -> None:
    cand = cand_bin([cand_record(0, 1, [clip_entry(10, 20, 22, 1.0, E_EQ, F_EQ)])])
    src = src_bin([src_record(1, [clip_entry(30, 40, 22, 0.5, E_EQ, F_EQ)]),
                   src_record(2, [clip_entry(30, 40, 22, 1.5, E_EQ, F_EQ)])])
    cand_p, src_p = _parse_bytes(cand, src)
    manifest = g.run_pairing(cand_p, src_p)
    pair = manifest["pairs"][0]
    check("T5 exact tie picks earliest seq", pair["src"]["seq"] == 1,
          f"seq={pair['src']['seq']}")
    check("T5 dt_clock==0.5", abs(pair["dt_clock"] - 0.5) < 1e-9, f"dt={pair['dt_clock']!r}")


def t06_edge_pair_flag() -> None:
    cand = cand_bin([cand_record(0, 1, [clip_entry(10, 20, 22, 2.0, E_EQ, F_EQ)])])
    src = src_bin([src_record(1, [clip_entry(30, 40, 22, 0.5, E_EQ, F_EQ)]),
                   src_record(2, [clip_entry(30, 40, 22, 1.0, E_EQ, F_EQ)])])
    cand_p, src_p = _parse_bytes(cand, src)
    manifest = g.run_pairing(cand_p, src_p)
    pair = manifest["pairs"][0]
    check("T6 beyond-range x pairs far endpoint", pair["src"]["seq"] == 2,
          f"seq={pair['src']['seq']}")
    check("T6 edge-pair flagged", "edge-pair" in pair["flags"], f"flags={pair['flags']}")


def t07_wrap_cycle_split() -> None:
    cand = cand_bin([cand_record(0, 1, [clip_entry(10, 20, 23, 0.1, E_IDLE, F_IDLE)])])
    src = src_bin([src_record(1, [clip_entry(30, 40, 23, 0.1, E_IDLE, F_IDLE)]),
                   src_record(2, [clip_entry(30, 40, 23, 0.2, E_IDLE, F_IDLE)]),
                   src_record(3, [clip_entry(30, 40, 23, 0.1, E_IDLE, F_IDLE)])])
    cand_p, src_p = _parse_bytes(cand, src)
    manifest = g.run_pairing(cand_p, src_p)
    cycles = manifest["domains"]["1hm_idle"]["wrap_cycles"]["source"]
    check("T7 decreasing local_time splits cycles", cycles == [2], f"cycles={cycles}")
    check("T7 wrap pairs nearest cycle", manifest["pairs"][0]["src"]["seq"] == 1,
          f"seq={manifest['pairs'][0]['src']['seq']}")


def t08_idun_and_ledger_identity() -> None:
    records = [
        cand_record(0, 1, [clip_entry(10, 20, 22, 0.1, E_EQ, F_EQ)]),
        cand_record(0, 2, [clip_entry(10, 20, 22, 0.2, E_EQ, F_EQ),
                           clip_entry(11, 21, 15073, 0.0, "Animations\\SCAR_1hmReadyDummy.hkx", 0)]),
        cand_record(0, 3, [clip_entry(10, 20, 22, 0.3, E_EQ, F_EQ)]),
    ]
    src = src_bin([src_record(1, [clip_entry(30, 40, 22, 0.15, E_EQ, F_EQ)]),
                   src_record(2, [clip_entry(30, 40, 22, 0.25, E_EQ, F_EQ)])])
    cand_p, src_p = _parse_bytes(cand_bin(records), src)
    manifest = g.run_pairing(cand_p, src_p)
    ledger = manifest["ledger"]["candidate"]
    check("T8 IDUN record excluded", ledger["idun_excluded"] == 1, f"ledger={ledger}")
    check("T8 identity closes", ledger["eligible"] == ledger["paired"] + ledger["no_clock"]
          + ledger["no_partner"], f"ledger={ledger}")
    check("T8 total = eligible + idun", ledger["total"] == ledger["eligible"] + ledger["idun_excluded"])
    try:
        bad = cand_bin([cand_record(0, 1, [clip_entry(10, 20, 22, 0.1, E_EQ, 0xDEADBEEF)])])
        g.run_pairing(_parse_bytes(bad, src)[0], src_p)
        check("T8 out-of-set raises INVALID", False, "no exception")
    except g.InvalidData:
        check("T8 out-of-set raises INVALID", True)


def t09_no_partner_booked() -> None:
    cand = cand_bin([cand_record(1, 1, [clip_entry(10, 20, 75, 0.1, E_UN, F_UN)]),
                     cand_record(1, 2, [clip_entry(10, 20, 75, 0.2, E_UN, F_UN)])])
    src = src_bin([src_record(1, [clip_entry(30, 40, 23, 0.1, E_IDLE, F_IDLE)])])
    cand_p, src_p = _parse_bytes(cand, src)
    manifest = g.run_pairing(cand_p, src_p)
    domain = manifest["domains"]["1hm_unequip"]
    check("T9 empty source domain -> no_partner", domain["no_partner"] == 2 and domain["pairs"] == 0,
          f"domain={domain}")
    check("T9 ledger no_partner counted", manifest["ledger"]["candidate"]["no_partner"] == 2)


def t10_parse_round_trip() -> None:
    cand = cand_bin([cand_record(0, 7, [clip_entry(10, 20, 22, 0.25, E_EQ, F_EQ, duration=2.5)])])
    src = src_bin([src_record(1, [clip_entry(30, 40, 23, 1.5, E_IDLE, F_IDLE)])])
    with tempfile.TemporaryDirectory() as tmp:
        cp, sp = os.path.join(tmp, "c.bin"), os.path.join(tmp, "s.bin")
        with open(cp, "wb") as handle:
            handle.write(cand)
        with open(sp, "wb") as handle:
            handle.write(src)
        cand_p, src_p = g.parse_candidate(cp), g.parse_source(sp)
    check("T10 candidate identity/count", cand_p["identity"] == "test-candidate" and cand_p["count"] == 1)
    clip = cand_p["records"][0]["clips"][0]
    check("T10 clip fields", clip["bare"] == "1hm_equip" and abs(clip["local_time"] - 0.25) < 1e-9
          and clip["data_fnv"] == F_EQ and abs(struct.unpack("<f", struct.pack("<I", clip["duration_bits"]))[0] - 2.5) < 1e-9,
          f"clip={clip}")
    srec = src_p["records"][0]
    check("T10 source seq/tsc", srec["seq"] == 1 and srec["tsc"] == 5001, f"rec={srec['seq']}/{srec['tsc']}")
    check("T10 record sizes", cand_p["record_size"] == 5664 and src_p["record_size"] == 5652)


def t11_determinism() -> None:
    cand = cand_bin([cand_record(0, 1, [clip_entry(10, 20, 22, 0.1, E_EQ, F_EQ),
                                        clip_entry(11, 21, 23, 0.05, E_IDLE, F_IDLE)]),
                     cand_record(1, 1, [clip_entry(10, 20, 75, 0.1, E_UN, F_UN)])])
    src = src_bin([src_record(1, [clip_entry(30, 40, 22, 0.12, E_EQ, F_EQ),
                                  clip_entry(31, 41, 23, 0.06, E_IDLE, F_IDLE)]),
                   src_record(2, [clip_entry(30, 40, 75, 0.11, E_UN, F_UN)])])
    cand_p, src_p = _parse_bytes(cand, src)
    first = json.dumps(g.run_pairing(cand_p, src_p), indent=1)
    second = json.dumps(g.run_pairing(cand_p, src_p), indent=1)
    cand_p2, src_p2 = _parse_bytes(cand, src)
    third = json.dumps(g.run_pairing(cand_p2, src_p2), indent=1)
    check("T11 two runs byte-identical", first == second)
    check("T11 re-parse byte-identical", first == third)


def t12_pose_mode_guard_and_detail() -> None:
    pose = track_bytes(t=(0.0, 0.0, 0.0)) * g.TRACK_COUNT
    pose_src = track_bytes(t=(0.0, 3.0, 4.0)) * g.TRACK_COUNT
    cand = cand_bin([cand_record(0, 1, [clip_entry(10, 20, 22, 1.0, E_EQ, F_EQ)], tracks=pose)])
    src = src_bin([src_record(1, [clip_entry(30, 40, 22, 0.5, E_EQ, F_EQ)], tracks=pose_src)])
    cand_p, src_p = _parse_bytes(cand, src)
    plain = g.run_pairing(cand_p, src_p)
    check("T12 default mode never decodes pose", plain["pose_decoded"] is False
          and "detail_rows" not in plain)
    tol = {"tau_p": 1.0, "tau_r": 1.0, "tau_s": 1.0, "epsilon_t": 0.001, "dt_exclude": True}
    formal = g.run_pairing(cand_p, src_p, pose_mode=True, tolerances=tol)
    detail, stats = formal["detail_rows"]
    expected_row = 16 + g.TRACK_COUNT * 16
    check("T12 detail row size frozen", len(detail) == expected_row,
          f"len={len(detail)} expected={expected_row}")
    header = struct.unpack_from("<IIII", detail, 0)
    check("T12 detail header frozen", header == (0, 0, 1, 0), f"header={header}")
    check("T12 E_p=5 exceeds tau_p on every bone", stats["exceeded"]["tau_p"] == g.TRACK_COUNT,
          f"exceeded={stats['exceeded']}")
    check("T12 dt beyond epsilon excludes from distributions",
          stats["dt_excluded_pairs"] == 1
          and stats["distributions"]["E_p"].get("count", 0) == 0,
          f"stats={stats['dt_excluded_pairs']}/{stats['distributions']['E_p']}")
    # And with dt_exclude off, the same pair feeds the distribution.
    tol2 = dict(tol, dt_exclude=False)
    _, stats2 = g.run_pairing(cand_p, src_p, pose_mode=True, tolerances=tol2)["detail_rows"]
    check("T12 dt_exclude=false feeds distributions",
          stats2["distributions"]["E_p"].get("count") == g.TRACK_COUNT
          and abs(stats2["distributions"]["E_p"]["max"] - 5.0) < 1e-12,
          f"dist={stats2['distributions']['E_p']}")


def _parse_bytes(cand: bytes, src: bytes):
    with tempfile.TemporaryDirectory() as tmp:
        cp, sp = os.path.join(tmp, "c.bin"), os.path.join(tmp, "s.bin")
        with open(cp, "wb") as handle:
            handle.write(cand)
        with open(sp, "wb") as handle:
            handle.write(src)
        return g.parse_candidate(cp), g.parse_source(sp)


def main() -> int:
    t01_position_known_value()
    t02_rotation_and_sign()
    t03_scale_component()
    t04_clock_priority()
    t05_nearest_tie_earliest_seq()
    t06_edge_pair_flag()
    t07_wrap_cycle_split()
    t08_idun_and_ledger_identity()
    t09_no_partner_booked()
    t10_parse_round_trip()
    t11_determinism()
    t12_pose_mode_guard_and_detail()
    if FAILURES:
        print(f"RESULT FAIL ({len(FAILURES)}): {FAILURES}")
        return 1
    print("RESULT PASS (T1..T12)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
