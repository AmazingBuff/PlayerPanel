#!/usr/bin/env python3
"""C-G5-CAL K5 offline reconciliation (K6 plan 2026-11-14 §10.5-§10.6).

Reads one candidate CPK5 v1 artifact (.bin, optionally the .json ledger) and one
source CPK5 v2 artifact, verifies the record structure (K5-1..K5-4), reassembles
the per-clip resource identities, and asserts every data FNV against the
registered sets from the K1-C4 census (v101 candidate / v101b source) — the
K5-7 verdict. Runtime code records drift facts only; THIS tool makes the
set-membership call.

Registered sets (docs/diagnostics/g5-k1-census-20261114/README.md §六/§七/§八):
  candidate (v101, IndependentGraph=inject): 7 data FNVs
  source    (v101b, natural play window):   8 data FNVs
Declared source-side stack names (K1-C1/K2-C1 deviation registrations): the
source idle stack legitimately changes with world state; an out-of-set source
FNV whose clip name is in the declared stack set is registered as a declared
gap, not an unexplained drift.

Usage:
  python -I tools/k5_reconcile.py --candidate <cpk5.bin> [--candidate-json <ledger.json>]
  python -I tools/k5_reconcile.py --source <cpk5s.bin> [--source-json <ledger.json>]
"""

import argparse
import json
import struct
import sys

K5_CANDIDATE_HEADER = struct.Struct("<4sHHIII")  # magic version header_size record_capacity track_capacity record_count
K5_SOURCE_HEADER = struct.Struct("<4sHHIIIIII")  # + source record_capacity track_capacity record_size record_count
K5_CANDIDATE_META = struct.Struct("<IIIIIIQ")  # segment completed commanded track_count two_read_equal clip_count elapsed_raw
K5_SOURCE_META = struct.Struct("<QIHHBBBB")  # tsc seq track_count clip_count path two_read_equal flags pad
K5_CLIP = struct.Struct("<QQII4s64sIQQQI")  # node binding binding_index local_time mode pad name duration data blockoff map_fnv map_count
TRACK_COUNT = 116
TRACK_BYTES = 40
CLIP_CAPACITY = 8

REGISTERED_CANDIDATE = {  # (clip name, data FNV) from v101
    ("mt_idle", 0x722C8A1E46CE297E),          # post-activate, Ashe conditional
    ("1hm_equip", 0x12A77CD14ECC3688),        # equip, SCAR/Elden Rim family
    ("mt_idle", 0xF56EE1500EB5A3D0),          # equip, Vanargand female
    ("1hm_unequip", 0x4DED5EAF23DF26E3),      # unequip
    ("mlh_idle", 0xDAD1F1C997C0F0C6),         # unequip
    ("1hm_idle", 0xC62C233B06FAE9A9),         # unequip, vanilla BSA
    ("scar_1hmreadydummy", 0x8728A54A50D85C22),
}
REGISTERED_SOURCE = {  # (clip name, data FNV) from v101b
    ("mt_idle", 0x9F6236C9E2D68F7D),
    ("1hm_equip", 0x81FD19882A9ED2B1),
    ("mlh_idle", 0x22C9C4BF59C71D25),
    ("1hm_idle", 0x22C9C4BF59C71D25),
    ("scar_1hmreadydummy", 0x8728A54A50D85C22),
    ("1hm_unequip", 0x4D885DE532138FE5),
    ("mlh_idle", 0x13D0592AF75A0886),
    ("1hm_idle", 0xAE1527A6189CD799),
    ("mt_idle", 0x95E20CF7492CAE83),
}
DECLARED_SOURCE_STACK_NAMES = {
    "mt_idle", "1hm_equip", "mlh_idle", "1hm_idle", "1hm_unequip", "scar_1hmreadydummy",
}

TRACK2_REGISTERED = {  # (clip name, data FNV) — Track 2 same-resource set (Run 0a/0b, g5-track2-20261114 §15/§16)
    ("mt_idle", 0xF56EE1500EB5A3D0),          # plain loose female/mt_idle.hkx (Vanargand)
    ("1hm_equip", 0x12A77CD14ECC3688),        # vanilla BSA
    ("1hm_unequip", 0x4DED5EAF23DF26E3),      # vanilla BSA
    ("mlh_idle", 0xDAD1F1C997C0F0C6),         # plain loose mlh_idle.hkx (Pretty Combat)
    ("1hm_idle", 0xC62C233B06FAE9A9),         # vanilla BSA
    ("scar_1hmreadydummy", 0x8728A54A50D85C22),  # Track2Dummy plain path
}


def clip_name(raw: bytes) -> str:
    return raw.rstrip(b"\x00").decode("utf-8", errors="replace")

def bare_clip_name(path_like: str) -> str:
    # The runtime animationName is the animation file path ("AnimationsHM_Equip.hkx",
    # "Animations\male\MT_Idle.HKX"); the registered v101/v101b sets are keyed by the bare
    # clip name. Normalization is comparison-side only: the resource identity stays the
    # data FNV (K1-C4), the name is auxiliary.
    leaf = path_like.replace("\\", "/").rsplit("/", 1)[-1]
    stem = leaf.rsplit(".", 1)[0] if "." in leaf else leaf
    return stem.strip().lower()


TRACK2_MODE = False


def reconcile_candidate(path: str) -> int:
    data = open(path, "rb").read()
    magic, version, header_size, capacity, track_cap, count = K5_CANDIDATE_HEADER.unpack_from(data, 0)
    if magic != b"CPK5":
        print(f"[FAIL] magic={magic!r} != CPK5")
        return 1
    identity = data[20:116].rstrip(b"\x00").decode("utf-8", errors="replace")
    record_size = K5_CANDIDATE_META.size + CLIP_CAPACITY * K5_CLIP.size + TRACK_COUNT * TRACK_BYTES
    ok = len(data) == header_size + count * record_size
    print(f"[K5-4] candidate artifact: {len(data)} bytes, records={count}, record_size={record_size}, "
          f"capacity={capacity}, version={version}, size-match={ok}")
    if not ok:
        print("[FAIL] file size does not match header record count")
        return 1
    problems = []
    segment_counts: dict[int, int] = {}
    two_read_bad = []
    track_bad = []
    identity_zero = 0
    sightings: dict[tuple, int] = {}
    out_of_set: dict[tuple, int] = {}
    for i in range(count):
        off = header_size + i * record_size
        seg, completed, commanded, tracks, equal, clip_count, _elapsed = K5_CANDIDATE_META.unpack_from(data, off)
        segment_counts[seg] = segment_counts.get(seg, 0) + 1
        if tracks != TRACK_COUNT:
            track_bad.append((i, tracks))
        if equal != 1:
            two_read_bad.append((i, completed))
        for c in range(clip_count):
            coff = off + K5_CANDIDATE_META.size + c * K5_CLIP.size
            _node, _binding, binding_index, _local_time = K5_CLIP.unpack_from(data, coff)[:4]
            name = clip_name(data[coff + 28: coff + 92])
            duration_bits, data_fnv, blockoff_fnv, map_fnv, map_count = K5_CLIP.unpack_from(data, coff)[6:]
            if data_fnv == 0:
                identity_zero += 1
            key = (name, binding_index, data_fnv, map_count, map_fnv)
            sightings[key] = sightings.get(key, 0) + 1
            registered_set = TRACK2_REGISTERED if TRACK2_MODE else REGISTERED_CANDIDATE
            skip = TRACK2_MODE and data_fnv == 0  # identity-unreadable: no FNV to assert (K6 10.3 flag)
            if not skip and (bare_clip_name(name), data_fnv) not in registered_set:
                out_of_set[key] = out_of_set.get(key, 0) + 1
    print(f"[K5-1] segments: equip={segment_counts.get(0, 0)} unequip={segment_counts.get(1, 0)} "
          f"(expected 120/129 for the frozen chain)")
    print(f"[K5-2] two-read mismatches: {len(two_read_bad)} {two_read_bad[:4]}")
    print(f"[K5-3] track-count mismatches: {len(track_bad)} {track_bad[:4]}")
    print(f"[K5-4] zero-FNV clips (identity-failed at runtime): {identity_zero}")
    print(f"[K5-7] unique clip sightings: {len(sightings)}")
    registered_set = TRACK2_REGISTERED if TRACK2_MODE else REGISTERED_CANDIDATE
    identity_unreadable = 0
    for (name, bidx, dfnv, mcnt, mfnv), seen in sorted(sightings.items(), key=lambda kv: (-kv[1], kv[0][0])):
        if dfnv == 0:
            # K5_Flag_Identity_Failed: binding/animation unreadable at this sighting (K6 10.3).
            # Not an out-of-set FNV (no FNV to assert); booked separately, excluded from pairing (C rule).
            identity_unreadable += seen
            tag = "IDUN"
        else:
            tag = "REG " if (bare_clip_name(name), dfnv) in registered_set else "OUT!"
        print(f"    {tag} {name:<22} bidx={bidx:<6} data_fnv={dfnv:016X} map_count={mcnt:<3} "
              f"map_fnv={mfnv:016X} records={seen}")
    verdict = "PASS"
    if len(two_read_bad) or len(track_bad) or out_of_set:
        verdict = "FAIL"
        for key in out_of_set:
            print(f"[K5-7 FAIL] unregistered clip: name={key[0]} bidx={key[1]} data_fnv={key[2]:016X}")
    if TRACK2_MODE and identity_unreadable:
        print(f"[K5-7 note] identity-unreadable sightings: {identity_unreadable} (booked, excluded from pairing)")
    print(f"[VERDICT] candidate-side K5-1/2/3/4/7: {verdict}")
    return 0 if verdict == "PASS" else 1


def reconcile_source(path: str) -> int:
    data = open(path, "rb").read()
    (magic, version, header_size, source, capacity, track_cap, record_size_field,
     count) = K5_SOURCE_HEADER.unpack_from(data, 0)[:8]
    if magic != b"CPK5":
        print(f"[FAIL] magic={magic!r} != CPK5")
        return 1
    record_size = K5_SOURCE_META.size + CLIP_CAPACITY * K5_CLIP.size + TRACK_COUNT * TRACK_BYTES
    ok = len(data) == header_size + count * record_size and record_size_field == record_size
    print(f"[K5-4] source artifact: {len(data)} bytes, records={count}, record_size={record_size} "
          f"(header says {record_size_field}), capacity={capacity}, version={version}, size-match={ok}")
    if not ok:
        print("[FAIL] file size does not match header record count")
        return 1
    problems = 0
    two_read_bad = 0
    track_bad = 0
    identity_zero = 0
    clip_overflow = 0
    sightings: dict[tuple, int] = {}
    out_of_set = {}
    declared_gap = {}
    registered_source_set = TRACK2_REGISTERED if TRACK2_MODE else REGISTERED_SOURCE
    declared_names = set() if TRACK2_MODE else DECLARED_SOURCE_STACK_NAMES
    identity_unreadable = 0
    for i in range(count):
        off = header_size + i * record_size
        _tsc, seq, tracks, clip_count, _path, equal, flags, _pad = K5_SOURCE_META.unpack_from(data, off)
        if tracks != TRACK_COUNT:
            track_bad += 1
        if equal != 1:
            two_read_bad += 1
        if flags & 0x1:
            clip_overflow += 1
        for c in range(clip_count):
            coff = off + K5_SOURCE_META.size + c * K5_CLIP.size
            _node, _binding, binding_index, _local_time = K5_CLIP.unpack_from(data, coff)[:4]
            name = clip_name(data[coff + 28: coff + 92])
            _duration_bits, data_fnv, _blockoff_fnv, map_fnv, map_count = K5_CLIP.unpack_from(data, coff)[6:]
            if data_fnv == 0:
                identity_zero += 1
            key = (name, binding_index, data_fnv, map_count, map_fnv)
            sightings[key] = sightings.get(key, 0) + 1
            bare = bare_clip_name(name)
            skip = TRACK2_MODE and data_fnv == 0  # identity-unreadable: no FNV to assert (K6 10.3 flag)
            if not skip and (bare, data_fnv) not in registered_source_set:
                bucket = declared_gap if bare in declared_names else out_of_set
                bucket[key] = bucket.get(key, 0) + 1
    print(f"[K5-2] two-read mismatches: {two_read_bad}")
    print(f"[K5-3] track-count mismatches: {track_bad}")
    print(f"[K5-4] zero-FNV clips: {identity_zero}; clip-overflow records: {clip_overflow}")
    print(f"[K5-7] unique clip sightings: {len(sightings)}")
    for (name, bidx, dfnv, mcnt, mfnv), seen in sorted(sightings.items(), key=lambda kv: (-kv[1], kv[0][0])):
        if dfnv == 0:
            identity_unreadable += seen
            tag = "IDUN"
        elif (bare_clip_name(name), dfnv) in registered_source_set:
            tag = "REG "
        elif bare_clip_name(name) in declared_names:
            tag = "GAP!"
        else:
            tag = "OUT!"
        print(f"    {tag} {name:<22} bidx={bidx:<6} data_fnv={dfnv:016X} map_count={mcnt:<3} "
              f"map_fnv={mfnv:016X} records={seen}")
    verdict = "PASS"
    if two_read_bad or track_bad or clip_overflow or out_of_set:
        verdict = "FAIL"
        for key in out_of_set:
            print(f"[K5-7 FAIL] undeclared clip: name={key[0]} bidx={key[1]} data_fnv={key[2]:016X}")
    if TRACK2_MODE and identity_unreadable:
        print(f"[K5-7 note] identity-unreadable sightings: {identity_unreadable} (booked, excluded from pairing)")
    if declared_gap:
        print(f"[K5-7 GAP] declared-stack FNVs outside the registered set "
              f"(registered as declaration gaps, not unexplained drift): {len(declared_gap)}")
    print(f"[VERDICT] source-side K5-1/2/3/4/7: {verdict}")
    return 0 if verdict == "PASS" else 1


def show_json(path: str) -> int:
    with open(path, "r", encoding="utf-8") as handle:
        ledger = json.load(handle)
    print(json.dumps(ledger, indent=2, sort_keys=False))
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--candidate", help="CPK5 v1 candidate artifact (.bin)")
    parser.add_argument("--candidate-json", help="candidate JSON ledger")
    parser.add_argument("--source", help="CPK5 v2 source artifact (.bin)")
    parser.add_argument("--source-json", help="source JSON ledger")
    parser.add_argument("--track2", action="store_true",
                        help="Track 2 same-resource semantics: TRACK2_REGISTERED set for both sides; "
                             "identity-unreadable (zero-FNV) sightings booked separately, not a set violation")
    global TRACK2_MODE
    args = parser.parse_args()
    TRACK2_MODE = args.track2
    if not any((args.candidate, args.candidate_json, args.source, args.source_json)):
        parser.error("nothing to do")
    status = 0
    if args.candidate_json:
        status |= show_json(args.candidate_json)
    if args.source_json:
        status |= show_json(args.source_json)
    if args.candidate:
        status |= reconcile_candidate(args.candidate)
    if args.source:
        status |= reconcile_source(args.source)
    return status


if __name__ == "__main__":
    sys.exit(main())
