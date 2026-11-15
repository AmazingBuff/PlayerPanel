#!/usr/bin/env python3
"""C-G5-CAL2 v1 offline adjudicator (docs/diagnostics/g5-cal2-20261115/README.md §9.7).

Reads one or more CGRD v1 artifacts produced by the v104 ControlSampleGrid
instrument, the registered 121-time truth grid (TRACK + BOUNDS sections), and
optionally the matching CharacterPanel.log files. It performs the frozen
C2-A..C2-F checks, keeps every raw component difference in a binary detail
file when at least two processes are supplied, and writes a compact JSON
ledger plus a human-readable summary. It never derives N, tau_* or any pose
equivalence verdict.

The truth comparison uses the B2 parse-bound rules verbatim:
  rotation (40-bit)        3.45436e-4
  dynamic 8/16-bit         max(2*(max-min)/255|65535, 32*eps*(|min|+|max|))
  static float32           32*eps*|value|
  identity                 32*eps
with the authored (min,max) union taken over all six blocks, as required by
§9.7. Rotation components use the registered 40-bit step for every component,
including identity rotation channels (conservative, matching the B2 script).

Usage:
  python -I tools/cal2_adjudicate.py \
      --artifact <run1.bin> [--artifact <run2.bin> ...] \
      [--log <run1.log> ...] --out-dir <dir> [--truth <grid121.txt>] [--force]

Exit codes: 0 adjudication completed (any verdict); 2 = at least one run
failed C2-A/B/C/F; 3 = parse/guard failure.
"""

import argparse
import datetime
import hashlib
import json
import math
import os
import re
import struct
import sys

GRID_MAGIC = b"CGRD"
GRID_VERSION = 1
GRID_HEADER_SIZE = 112
GRID_RECORD_SIZE = 4004
GRID_GENERATION_COUNT = 121
GRID_TRACK_COUNT = 99
GRID_COMPONENT_COUNT = 10
GRID_HEADER = struct.Struct("<4sHHHHHI")
GRID_RECORD_META = struct.Struct("<HIHBBBBQQQQ")
GRID_RECORD_DATA_OFFSET = GRID_RECORD_META.size  # 44
GRID_DATA_WORDS = GRID_TRACK_COUNT * GRID_COMPONENT_COUNT  # 990

CROSS_MAGIC = b"C2DF"
CROSS_VERSION = 1
CROSS_HEADER = struct.Struct("<4sHHIIIIQ")  # magic/version/header_size/runs/pairs/components/reserved/bytes
CROSS_HEADER_SIZE = CROSS_HEADER.size

F32_EPS = 2.0 ** -24
ROTATION_BOUND = 3.45436e-4

# Frozen registrations from src/animation/behavior_graph/control_sample_policy.h
# and docs/build.md v104-g5cal2-grid-20261115.
REGISTERED_RESOURCE = "meshes\\actors\\character\\animations\\female\\mt_idle.hkx"
REGISTERED_RESOURCE_BYTES = 113920
REGISTERED_RESOURCE_SHA256 = (
    "0EF8248CE2C50A2251FA592F7C65E75E75B5E4DC55E9C654436543180BCD73C7"
)
REGISTERED_DURATION_BITS = 0x41C82222
REGISTERED_TRACKS = 99
REGISTERED_FLOATS = 0
REGISTERED_FRAMES = 1501
REGISTERED_BLOCKS = 6
REGISTERED_DATA_FNV = 0xF56EE1500EB5A3D0
REGISTERED_BLOCK_OFFSETS_FNV = 0xF17EBD9ADA8CF785
REGISTERED_MAP_FNV = 0x077954859409395E
REGISTERED_BINDING_INDEX = 1022
REGISTERED_BUILD_IDENTITY = (
    "CharacterPanel-scenegraph-v104-g5cal2-grid-d61a6e19b9-"
    "cl94faaed0c6-20261115T073547Z"
)

DEFAULT_TRUTH = os.path.normpath(os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "..", "docs", "diagnostics",
    "g5-cal2-20261115", "mt_idle_female_truth_grid121.txt"))

SUMMARY_LINE_BUDGET = 200
OAR_ERROR = "failed to get plugin info for OpenAnimationReplacer"

_FIELD_RE = re.compile(r"([A-Za-z0-9_+\-]+)=('(?:[^']*)'|[^\s]+)")
_TRACK_RE = re.compile(
    r"^TRACK time=(\d+) track=(\d+) t=([0-9A-Fa-f,]+) r=([0-9A-Fa-f,]+) s=([0-9A-Fa-f,]+)$")


class ParseError(Exception):
    """Input/guard failure (exit 3)."""


def sha256_file(path: str) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def parse_fields(text: str) -> dict:
    return {match.group(1): match.group(2).strip("'") for match in _FIELD_RE.finditer(text)}


def f32_from_bits(bits: int) -> float:
    return struct.unpack("<f", struct.pack("<I", bits))[0]


def fnv1a64(data: bytes) -> int:
    value = 0xCBF29CE484222325
    for byte in data:
        value ^= byte
        value = (value * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return value


def ordered_float_key(bits: int) -> int:
    if bits & 0x7FFFFFFF == 0:  # fold +0 and -0 into one key
        return 0
    if bits & 0x80000000:
        return 0x80000000 - (bits & 0x7FFFFFFF)
    return bits


def ulp_distance(a: int, b: int) -> int:
    return abs(ordered_float_key(a) - ordered_float_key(b))


def median(values):
    if not values:
        return None
    ordered = sorted(values)
    count = len(ordered)
    middle = count // 2
    if count % 2:
        return ordered[middle]
    return (ordered[middle - 1] + ordered[middle]) / 2.0


def percentile(values, fraction: float):
    if not values:
        return None
    ordered = sorted(values)
    if len(ordered) == 1:
        return ordered[0]
    position = (len(ordered) - 1) * fraction
    lower = int(math.floor(position))
    upper = min(lower + 1, len(ordered) - 1)
    weight = position - lower
    return ordered[lower] * (1.0 - weight) + ordered[upper] * weight


def summarize(values):
    if not values:
        return {"count": 0}
    return {
        "count": len(values),
        "min": min(values),
        "median": median(values),
        "p95": percentile(values, 0.95),
        "max": max(values),
    }


def parse_hex_values(text: str):
    return [int(item, 16) for item in text.split(",")]


def parse_descriptor(raw: str):
    if raw == "IDN":
        return {"kind": "identity"}
    parts = raw.split(":")
    try:
        if parts[0] == "STA" and len(parts) >= 2:
            values = [f32_from_bits(int(part, 16)) for part in parts[1:]]
            return {"kind": "static", "value": values[0], "values": values}
        if parts[0] == "DYN" and len(parts) == 1:
            return {"kind": "dynamic"}
        if parts[0] == "DYN" and len(parts) == 3:
            return {
                "kind": "dynamic",
                "min": f32_from_bits(int(parts[1], 16)),
                "max": f32_from_bits(int(parts[2], 16)),
            }
    except ValueError as error:
        raise ParseError(f"bad BOUND descriptor {raw!r}: {error}") from error
    raise ParseError(f"bad BOUND descriptor {raw!r}")


def bound_for_component(track: int, component: int, descriptors, quant_types):
    """Return the frozen B2 bound for one truth track/component."""
    if 3 <= component <= 6:
        return {"kind": "rotation-40", "bound": ROTATION_BOUND}

    if component < 3:
        canonical = 0.0
        scope = "pos"
    else:
        canonical = 1.0
        scope = "scale"

    values = []
    has_dynamic = False
    has_static = False
    for descriptor in descriptors:
        kind = descriptor["kind"]
        if kind == "dynamic":
            has_dynamic = True
            values.extend((descriptor["min"], descriptor["max"]))
        elif kind == "static":
            has_static = True
            values.append(descriptor["value"])
        else:
            values.append(canonical)

    if has_dynamic:
        q_values = [value for value in quant_types if value is not None]
        if not q_values:
            raise ParseError(f"track {track} component {component}: dynamic without quantization")
        if any(value not in (0, 1) for value in q_values):
            raise ParseError(
                f"track {track} {scope} component {component}: unsupported quantization {q_values}")
        divisor = 255.0 if any(value == 0 for value in q_values) else 65535.0
        low, high = min(values), max(values)
        step_bound = 2.0 * (high - low) / divisor
        round_bound = 32.0 * F32_EPS * (abs(low) + abs(high))
        return {
            "kind": "dynamic-8" if divisor == 255.0 else "dynamic-16",
            "bound": max(step_bound, round_bound),
            "min": low,
            "max": high,
        }
    if has_static:
        return {"kind": "static", "bound": 32.0 * F32_EPS * max(abs(value) for value in values)}
    return {"kind": "identity", "bound": 32.0 * F32_EPS}


def parse_truth(path: str):
    times = {}
    tracks = {}
    bounds = {}
    blocks_declared = None
    tracks_declared = None
    integrity = None
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        for line_number, raw_line in enumerate(handle, 1):
            line = raw_line.rstrip("\r\n")
            if line.startswith("BOUNDS "):
                fields = parse_fields(line[7:])
                blocks_declared = int(fields["blocks"])
            elif line.startswith("TRACKS "):
                fields = parse_fields(line[7:])
                tracks_declared = int(fields["count"])
            elif line.startswith("TIME "):
                fields = parse_fields(line[5:])
                index = int(fields["index"])
                if index in times:
                    raise ParseError(f"truth line {line_number}: duplicate TIME index {index}")
                times[index] = {
                    "bits": int(fields["bits"], 16),
                    "block": int(fields["block"]),
                }
            elif line.startswith("TRACK "):
                match = _TRACK_RE.match(line)
                if not match:
                    raise ParseError(f"truth line {line_number}: malformed TRACK")
                time_index = int(match.group(1))
                track = int(match.group(2))
                values = (parse_hex_values(match.group(3)) + parse_hex_values(match.group(4)) +
                          parse_hex_values(match.group(5)))
                if len(values) != GRID_COMPONENT_COUNT:
                    raise ParseError(f"truth line {line_number}: expected 10 components")
                key = (time_index, track)
                if key in tracks:
                    raise ParseError(f"truth line {line_number}: duplicate TRACK {key}")
                tracks[key] = values
            elif line.startswith("BOUND "):
                fields = parse_fields(line[6:])
                block = int(fields["block"])
                track = int(fields["track"])
                if (block, track) in bounds:
                    raise ParseError(f"truth line {line_number}: duplicate BOUND {block}/{track}")
                bounds[(block, track)] = {
                    "pos_q": int(fields["pos_q"]),
                    "rot_q": int(fields["rot_q"]),
                    "scale_q": int(fields["scale_q"]),
                    "px": parse_descriptor(fields["px"]),
                    "py": parse_descriptor(fields["py"]),
                    "pz": parse_descriptor(fields["pz"]),
                    "rot": parse_descriptor(fields["rot"]),
                    "sx": parse_descriptor(fields["sx"]),
                    "sy": parse_descriptor(fields["sy"]),
                    "sz": parse_descriptor(fields["sz"]),
                }
            elif line.startswith("BITS_FNV1A64 "):
                integrity = int(line.split()[1], 16)

    if blocks_declared != REGISTERED_BLOCKS:
        raise ParseError(f"truth blocks={blocks_declared}, expected {REGISTERED_BLOCKS}")
    if tracks_declared != REGISTERED_TRACKS:
        raise ParseError(f"truth tracks={tracks_declared}, expected {REGISTERED_TRACKS}")
    if sorted(times) != list(range(GRID_GENERATION_COUNT)):
        raise ParseError(f"truth TIME indices are not 0..{GRID_GENERATION_COUNT - 1}")
    if len(tracks) != GRID_GENERATION_COUNT * REGISTERED_TRACKS:
        raise ParseError(f"truth TRACK count={len(tracks)}, expected {GRID_GENERATION_COUNT * REGISTERED_TRACKS}")
    for time_index in range(GRID_GENERATION_COUNT):
        for track in range(REGISTERED_TRACKS):
            if (time_index, track) not in tracks:
                raise ParseError(f"truth missing TRACK time={time_index} track={track}")
    if len(bounds) != REGISTERED_BLOCKS * REGISTERED_TRACKS:
        raise ParseError(
            f"truth BOUND count={len(bounds)}, expected {REGISTERED_BLOCKS * REGISTERED_TRACKS}")

    time_bits = [times[index]["bits"] for index in range(GRID_GENERATION_COUNT)]
    time_values = [f32_from_bits(bits) for bits in time_bits]
    if time_bits[0] != 0 or time_bits[1] != 0x3C888889 or \
            time_bits[60] != 0x3F800000 or time_bits[120] != 0x40000000:
        raise ParseError("truth grid anchor times do not match the frozen registrations")
    if any(not math.isfinite(value) for value in time_values):
        raise ParseError("truth grid contains a non-finite time")
    if any(time_values[index] >= time_values[index + 1]
           for index in range(GRID_GENERATION_COUNT - 1)):
        raise ParseError("truth grid times are not strictly increasing")

    track_bits = [[None] * REGISTERED_TRACKS for _ in range(GRID_GENERATION_COUNT)]
    for time_index in range(GRID_GENERATION_COUNT):
        for track in range(REGISTERED_TRACKS):
            track_bits[time_index][track] = tracks[(time_index, track)]

    bound_specs = [[None] * GRID_COMPONENT_COUNT for _ in range(REGISTERED_TRACKS)]
    for track in range(REGISTERED_TRACKS):
        pos_quant = [bounds[(block, track)]["pos_q"] for block in range(REGISTERED_BLOCKS)]
        scale_quant = [bounds[(block, track)]["scale_q"] for block in range(REGISTERED_BLOCKS)]
        for component in range(GRID_COMPONENT_COUNT):
            if 3 <= component <= 6:
                descriptors = [bounds[(block, track)]["rot"] for block in range(REGISTERED_BLOCKS)]
                bound_specs[track][component] = bound_for_component(track, component, descriptors, [])
            elif component < 3:
                key = ("px", "py", "pz")[component]
                descriptors = [bounds[(block, track)][key] for block in range(REGISTERED_BLOCKS)]
                bound_specs[track][component] = bound_for_component(
                    track, component, descriptors, pos_quant)
            else:
                key = ("sx", "sy", "sz")[component - 7]
                descriptors = [bounds[(block, track)][key] for block in range(REGISTERED_BLOCKS)]
                bound_specs[track][component] = bound_for_component(
                    track, component, descriptors, scale_quant)

    return {
        "path": os.path.abspath(path),
        "sha256": sha256_file(path),
        "size": os.path.getsize(path),
        "times": time_bits,
        "time_values": time_values,
        "blocks": blocks_declared,
        "tracks": track_bits,
        "bounds": bound_specs,
        "integrity_fnv1a64": integrity,
    }


def parse_artifact(path: str):
    with open(path, "rb") as handle:
        raw = handle.read()
    if len(raw) < GRID_HEADER_SIZE:
        raise ParseError(f"{path}: shorter than the CGRD header")
    (magic, version, header_size, capacity, recorded, track_count,
     reserved) = GRID_HEADER.unpack_from(raw, 0)
    identity = raw[18:110].split(b"\x00", 1)[0].decode("ascii", errors="replace")
    expected_size = GRID_HEADER_SIZE + recorded * GRID_RECORD_SIZE
    if len(raw) != expected_size:
        raise ParseError(
            f"{path}: size {len(raw)} != header {GRID_HEADER_SIZE} + {recorded}x{GRID_RECORD_SIZE}")
    records = []
    for index in range(recorded):
        offset = GRID_HEADER_SIZE + index * GRID_RECORD_SIZE
        meta = GRID_RECORD_META.unpack_from(raw, offset)
        data = struct.unpack_from(f"<{GRID_DATA_WORDS}I", raw, offset + GRID_RECORD_DATA_OFFSET)
        records.append({
            "ordinal": meta[0],
            "time_bits": meta[1],
            "completed": meta[2],
            "two_read_equal": meta[3],
            "identity_ok": meta[4],
            "flags": meta[5],
            "pad": meta[6],
            "tsc_call_start": meta[7],
            "tsc_after_first": meta[8],
            "tsc_after_second": meta[9],
            "tsc_after_compare": meta[10],
            "data": data,
        })
    data_bytes = b"".join(struct.pack(f"<{GRID_DATA_WORDS}I", *record["data"])
                          for record in records)
    return {
        "path": os.path.abspath(path),
        "basename": os.path.basename(path),
        "sha256": sha256_file(path),
        "size": len(raw),
        "magic": magic.decode("ascii", errors="replace"),
        "version": version,
        "header_size": header_size,
        "capacity": capacity,
        "recorded": recorded,
        "track_count": track_count,
        "reserved": reserved,
        "identity": identity,
        "records": records,
        "data_fnv1a64": fnv1a64(data_bytes),
    }


def _issue(issues, message):
    issues["count"] += 1
    if len(issues["sample"]) < 20:
        issues["sample"].append(message)


def _verdict(issues):
    return "PASS" if issues["count"] == 0 else "FAIL"


def _new_issues():
    return {"count": 0, "sample": []}


def expected_completed(ordinal: int) -> int:
    return 1 if ordinal <= 1 else ordinal


def check_artifact_sequence(artifact, truth):
    issues = _new_issues()
    records = artifact["records"]
    if len(records) != GRID_GENERATION_COUNT:
        _issue(issues, f"recorded={len(records)}, expected {GRID_GENERATION_COUNT}")
        return issues
    for index, record in enumerate(records):
        if record["ordinal"] != index:
            _issue(issues, f"ordinal[{index}]={record['ordinal']} != {index}")
        if record["time_bits"] != truth["times"][index]:
            _issue(issues, f"time[{index}]={record['time_bits']:08X} != {truth['times'][index]:08X}")
        if record["completed"] != expected_completed(index):
            _issue(issues, f"completed[{index}]={record['completed']} != {expected_completed(index)}")
    return issues


def check_tsc(records):
    issues = _new_issues()
    first = []
    second = []
    compare = []
    total = []
    for index, record in enumerate(records):
        call = record["tsc_call_start"]
        after_first = record["tsc_after_first"]
        after_second = record["tsc_after_second"]
        after_compare = record["tsc_after_compare"]
        if min(call, after_first, after_second, after_compare) <= 0:
            _issue(issues, f"ordinal {index}: zero/missing TSC point")
            continue
        if not (call <= after_first <= after_second <= after_compare):
            _issue(issues, f"ordinal {index}: non-monotonic TSC {call}/{after_first}/"
                            f"{after_second}/{after_compare}")
            continue
        first.append(after_first - call)
        second.append(after_second - after_first)
        compare.append(after_compare - after_second)
        total.append(after_compare - call)
    return issues, {
        "first_read": summarize(first),
        "second_read": summarize(second),
        "compare": summarize(compare),
        "total": summarize(total),
    }


COMPONENT_NAMES = ["t.x", "t.y", "t.z", "r.x", "r.y", "r.z", "r.w", "s.x", "s.y", "s.z"]


def compare_truth(artifact, truth):
    issues = _new_issues()
    violations = []
    components_checked = 0
    nonzero = 0
    max_ratio = 0.0
    worst = []
    per_component = [{"checked": 0, "nonzero": 0, "violations": 0, "max_ratio": 0.0}
                     for _ in range(GRID_COMPONENT_COUNT)]
    per_time = [{"checked": 0, "nonzero": 0, "violations": 0, "max_ratio": 0.0}
                for _ in range(GRID_GENERATION_COUNT)]

    records = artifact["records"]
    if len(records) != GRID_GENERATION_COUNT:
        _issue(issues, f"recorded={len(records)}, cannot compare the full truth grid")
        return {
            "verdict": "NOT_EVALUATED", "issues": issues, "violations": violations,
            "components_checked": 0, "nonzero": 0, "max_ratio": None, "worst": [],
            "per_component": per_component, "per_time": per_time,
        }

    for ordinal, record in enumerate(records):
        for track in range(REGISTERED_TRACKS):
            truth_values = truth["tracks"][ordinal][track]
            base = track * GRID_COMPONENT_COUNT
            for component in range(GRID_COMPONENT_COUNT):
                runtime_bits = record["data"][base + component]
                truth_bits = truth_values[component]
                components_checked += 1
                per_component[component]["checked"] += 1
                per_time[ordinal]["checked"] += 1
                if runtime_bits == truth_bits:
                    continue
                nonzero += 1
                per_component[component]["nonzero"] += 1
                per_time[ordinal]["nonzero"] += 1
                runtime = f32_from_bits(runtime_bits)
                offline = f32_from_bits(truth_bits)
                if not math.isfinite(runtime) or not math.isfinite(offline):
                    _issue(issues, f"ordinal {ordinal} track {track} {COMPONENT_NAMES[component]}: non-finite")
                    continue
                difference = abs(runtime - offline)
                spec = truth["bounds"][track][component]
                bound = spec["bound"]
                ratio = difference / bound if bound > 0 else (0.0 if difference == 0 else math.inf)
                if ratio > per_component[component]["max_ratio"]:
                    per_component[component]["max_ratio"] = ratio
                if ratio > per_time[ordinal]["max_ratio"]:
                    per_time[ordinal]["max_ratio"] = ratio
                if ratio > max_ratio:
                    max_ratio = ratio
                row = {
                    "ordinal": ordinal,
                    "track": track,
                    "component": COMPONENT_NAMES[component],
                    "runtime_bits": runtime_bits,
                    "truth_bits": truth_bits,
                    "runtime": runtime,
                    "truth": offline,
                    "difference": difference,
                    "bound": bound,
                    "ratio": ratio,
                    "kind": spec["kind"],
                }
                if ratio > 1.0:
                    per_component[component]["violations"] += 1
                    per_time[ordinal]["violations"] += 1
                    violations.append(row)
                if len(worst) < 10 or ratio > worst[-1]["ratio"]:
                    worst.append(row)
                    worst.sort(key=lambda item: item["ratio"], reverse=True)
                    del worst[10:]

    for row in violations[:20]:
        _issue(issues, f"C2-C violation ordinal {row['ordinal']} track {row['track']} "
                        f"{row['component']}: diff={row['difference']:.9g} > bound={row['bound']:.9g}")
    return {
        "verdict": "PASS" if not violations and issues["count"] == 0 else "FAIL",
        "issues": issues,
        "violations": violations,
        "components_checked": components_checked,
        "nonzero": nonzero,
        "max_ratio": max_ratio if components_checked else None,
        "worst": worst,
        "per_component": per_component,
        "per_time": per_time,
    }


def parse_log(path: str):
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        lines = handle.read().splitlines()
    manifests = []
    carriers = []
    closes = []
    generations = []
    errors = []
    settings = {}
    build_identities = []
    drive_summaries = []
    segment_ends = []
    crash_lines = []
    for line in lines:
        if "SCENEGRAPH BUILD " in line:
            build_identities.append(line.split("SCENEGRAPH BUILD ", 1)[1].split()[0])
        if "EXPERIMENT setting " in line:
            fields = parse_fields(line.split("EXPERIMENT setting ", 1)[1])
            for key, value in fields.items():
                settings[key] = value
        if " CONTROL GRID " in line:
            payload = line.split(" CONTROL GRID ", 1)[1]
            fields = parse_fields(payload)
            phase = fields.get("phase")
            if phase == "manifest":
                manifests.append(fields)
            elif phase == "carrier":
                carriers.append(fields)
            elif phase == "close":
                closes.append(fields)
            elif phase == "gen":
                generations.append(fields)
        if "[E]" in line:
            errors.append(line.split("[E]", 1)[1].strip())
        if "phase=drive-summary" in line:
            drive_summaries.append(parse_fields(line))
        if "phase=segment-end" in line:
            segment_ends.append(parse_fields(line))
        lowered = line.lower()
        if re.search(r"\bcrash\b|\bunhandled exception\b|\bfatal error\b", lowered):
            crash_lines.append(line)
    return {
        "path": os.path.abspath(path),
        "sha256": sha256_file(path),
        "size": os.path.getsize(path),
        "lines": len(lines),
        "manifests": manifests,
        "carriers": carriers,
        "closes": closes,
        "generations": generations,
        "errors": errors,
        "settings": settings,
        "build_identities": build_identities,
        "drive_summaries": drive_summaries,
        "segment_ends": segment_ends,
        "crash_lines": crash_lines,
    }


def _field_int(fields, key, default=None):
    try:
        return int(fields[key])
    except (KeyError, ValueError):
        return default


def _field_hex(fields, key, default=None):
    try:
        return int(fields[key], 16)
    except (KeyError, ValueError):
        return default


def reconcile_log(log, artifact, allow_oar_exemption=False):
    a_issues = _new_issues()
    b_issues = _new_issues()
    e_issues = _new_issues()

    manifests = log["manifests"]
    carriers = log["carriers"]
    closes = log["closes"]
    generations = log["generations"]

    if len(manifests) != 1:
        _issue(a_issues, f"manifest count={len(manifests)}, expected 1")
    else:
        manifest = manifests[0]
        expected = {
            "resource": REGISTERED_RESOURCE,
            "bytes": str(REGISTERED_RESOURCE_BYTES),
            "sha256": REGISTERED_RESOURCE_SHA256,
            "duration_bits": f"{REGISTERED_DURATION_BITS:08X}",
            "tracks": str(REGISTERED_TRACKS),
            "floats": str(REGISTERED_FLOATS),
            "frames": str(REGISTERED_FRAMES),
            "blocks": str(REGISTERED_BLOCKS),
            "times": str(GRID_GENERATION_COUNT),
            "baseline": "1",
        }
        for key, value in expected.items():
            if manifest.get(key) != value:
                _issue(a_issues, f"manifest {key}={manifest.get(key)!r} != {value!r}")
        for key, value in (("data_fnv1a64", REGISTERED_DATA_FNV),
                           ("block_offsets_fnv1a64", REGISTERED_BLOCK_OFFSETS_FNV),
                           ("map_fnv1a64", REGISTERED_MAP_FNV)):
            if _field_hex(manifest, key) != value:
                _issue(a_issues, f"manifest {key}={manifest.get(key)!r} != {value:016X}")

    if len(carriers) != 1:
        _issue(a_issues, f"carrier count={len(carriers)}, expected 1")
    else:
        carrier = carriers[0]
        if _field_int(carrier, "completed") != 1:
            _issue(a_issues, f"carrier completed={carrier.get('completed')!r}, expected 1")
        if _field_int(carrier, "binding_index") != REGISTERED_BINDING_INDEX:
            _issue(a_issues, f"carrier binding_index={carrier.get('binding_index')!r}, "
                              f"expected {REGISTERED_BINDING_INDEX}")
        if _field_int(carrier, "map_count") != REGISTERED_TRACKS:
            _issue(a_issues, f"carrier map_count={carrier.get('map_count')!r}, expected {REGISTERED_TRACKS}")
        if _field_hex(carrier, "map_fnv1a64") != REGISTERED_MAP_FNV:
            _issue(a_issues, f"carrier map_fnv1a64={carrier.get('map_fnv1a64')!r} mismatch")
        if carrier.get("map_match") != "true":
            _issue(a_issues, f"carrier map_match={carrier.get('map_match')!r}, expected true")

    if not any(identity == artifact["identity"] for identity in log["build_identities"]):
        _issue(a_issues, "log SCENEGRAPH BUILD identity does not match the CGRD header")

    if len(generations) != GRID_GENERATION_COUNT:
        _issue(b_issues, f"gen rows={len(generations)}, expected {GRID_GENERATION_COUNT}")
    else:
        for index, fields in enumerate(generations):
            record = artifact["records"][index] if index < len(artifact["records"]) else None
            if _field_int(fields, "ordinal") != index:
                _issue(b_issues, f"gen[{index}] ordinal={fields.get('ordinal')!r}")
            if record is not None:
                if _field_int(fields, "completed") != record["completed"]:
                    _issue(b_issues, f"gen[{index}] completed={fields.get('completed')!r} "
                                      f"!= {record['completed']}")
                if _field_hex(fields, "time_bits") != record["time_bits"]:
                    _issue(b_issues, f"gen[{index}] time_bits={fields.get('time_bits')!r} mismatch")
            if fields.get("two_read_equal") != "1":
                _issue(b_issues, f"gen[{index}] two_read_equal={fields.get('two_read_equal')!r}")
            if fields.get("identity_ok") != "true":
                _issue(b_issues, f"gen[{index}] identity_ok={fields.get('identity_ok')!r}")
            for key in ("tsc_first", "tsc_second", "tsc_compare"):
                if _field_int(fields, key) is None or _field_int(fields, key) < 0:
                    _issue(b_issues, f"gen[{index}] {key} missing/negative")

    if len(closes) != 1:
        _issue(e_issues, f"close count={len(closes)}, expected 1")
    else:
        close = closes[0]
        if close.get("status") != "complete":
            _issue(e_issues, f"close status={close.get('status')!r}")
        if _field_int(close, "generations") != GRID_GENERATION_COUNT:
            _issue(e_issues, f"close generations={close.get('generations')!r}")
        for key in ("two_read_bad", "identity_bad", "nonfinite"):
            if _field_int(close, key) != 0:
                _issue(e_issues, f"close {key}={close.get(key)!r}, expected 0")
        if _field_hex(close, "data_fnv1a64") != artifact["data_fnv1a64"]:
            _issue(e_issues, f"close data_fnv1a64={close.get('data_fnv1a64')!r} "
                              f"!= artifact {artifact['data_fnv1a64']:016X}")
        if _field_int(close, "bytes") != artifact["size"]:
            _issue(e_issues, f"close bytes={close.get('bytes')!r} != artifact {artifact['size']}")
        close_artifact = os.path.basename((close.get("artifact") or "").replace("\\", "/"))
        if close_artifact != artifact["basename"]:
            _issue(e_issues, f"close artifact basename={close_artifact!r} != {artifact['basename']!r}")

    if len(manifests) != 1 or len(carriers) != 1 or len(closes) != 1:
        _issue(e_issues, "window count is not exactly manifest=1 carrier=1 close=1")
    if len(log["build_identities"]) != 1:
        _issue(e_issues, f"SCENEGRAPH BUILD rows={len(log['build_identities'])}, expected 1")
    if len(log["drive_summaries"]) != 1:
        _issue(e_issues, f"drive-summary rows={len(log['drive_summaries'])}, expected 1")
    if artifact["size"] > 8 * 1024 * 1024:
        _issue(e_issues, f"artifact bytes={artifact['size']} exceed 8 MiB")

    oar_errors = [error for error in log["errors"] if error.endswith(OAR_ERROR)]
    if allow_oar_exemption:
        exempt = oar_errors
        unexpected = [error for error in log["errors"] if not error.endswith(OAR_ERROR)]
    else:
        exempt = []
        unexpected = list(log["errors"])
        if oar_errors:
            _issue(e_issues, "OAR plugin-info [E] present; C2-E requires zero [E] "
                              "(no Track 2 OAR exemption active)")
    if unexpected:
        _issue(e_issues, f"unexpected [E] rows={len(unexpected)}: {unexpected[0][:120]}")

    if log["settings"].get("IndependentGraph") != "inject":
        _issue(e_issues, f"IndependentGraph={log['settings'].get('IndependentGraph')!r}")
    if log["settings"].get("ControlSampleGrid") != "true":
        _issue(e_issues, f"ControlSampleGrid={log['settings'].get('ControlSampleGrid')!r}")
    if log["settings"].get("ControlSample") != "false":
        _issue(e_issues, f"ControlSample={log['settings'].get('ControlSample')!r}")

    drive_ok = False
    for summary in log["drive_summaries"]:
        if _field_int(summary, "equip-completed") == 120 and \
                _field_int(summary, "unequip-completed") == 129:
            drive_ok = True
    if not drive_ok:
        _issue(e_issues, "no drive-summary with equip-completed=120 and unequip-completed=129")
    if not any(fields.get("source-drift") == "none" for fields in log["segment_ends"]):
        _issue(e_issues, "no segment-end row with source-drift=none")
    if log["crash_lines"]:
        _issue(e_issues, f"crash-like log rows={len(log['crash_lines'])}")

    if len(log["generations"]) > SUMMARY_LINE_BUDGET:
        _issue(e_issues, f"CONTROL GRID rows={len(log['generations'])} exceed {SUMMARY_LINE_BUDGET}")

    return {
        "a_issues": a_issues,
        "b_issues": b_issues,
        "e_issues": e_issues,
        "errors_total": len(log["errors"]),
        "errors_exempt_oar": len(exempt),
        "errors_oar": len(oar_errors),
        "oar_exemption": allow_oar_exemption,
        "errors_unexpected": len(unexpected),
        "manifest_count": len(manifests),
        "carrier_count": len(carriers),
        "close_count": len(closes),
        "gen_count": len(generations),
    }


def adjudicate_run(index, artifact_path, truth, log_path=None, allow_oar_exemption=False):
    artifact = parse_artifact(artifact_path)
    header_issues = _new_issues()
    if artifact["magic"] != GRID_MAGIC.decode("ascii"):
        _issue(header_issues, f"magic={artifact['magic']!r}")
    if artifact["version"] != GRID_VERSION:
        _issue(header_issues, f"version={artifact['version']}")
    if artifact["header_size"] != GRID_HEADER_SIZE:
        _issue(header_issues, f"header_size={artifact['header_size']}")
    if artifact["capacity"] != GRID_GENERATION_COUNT:
        _issue(header_issues, f"capacity={artifact['capacity']}")
    if artifact["track_count"] != REGISTERED_TRACKS:
        _issue(header_issues, f"track_count={artifact['track_count']}")
    if artifact["reserved"] != 0:
        _issue(header_issues, f"reserved={artifact['reserved']}")
    if artifact["recorded"] != GRID_GENERATION_COUNT:
        _issue(header_issues, f"recorded={artifact['recorded']}")
    if artifact["identity"] != REGISTERED_BUILD_IDENTITY:
        _issue(header_issues, f"identity={artifact['identity']!r}")

    sequence_issues = check_artifact_sequence(artifact, truth)

    two_read_bad = sum(1 for record in artifact["records"] if record["two_read_equal"] != 1)
    identity_bad = sum(1 for record in artifact["records"] if record["identity_ok"] != 1)
    nonfinite = 0
    for record in artifact["records"]:
        for bits in record["data"]:
            if not math.isfinite(f32_from_bits(bits)):
                nonfinite += 1

    b_issues = _new_issues()
    if artifact["recorded"] != GRID_GENERATION_COUNT:
        _issue(b_issues, f"recorded={artifact['recorded']}, expected {GRID_GENERATION_COUNT}")
    for message in sequence_issues["sample"]:
        _issue(b_issues, message)
    if sequence_issues["count"] > len(sequence_issues["sample"]):
        _issue(b_issues, f"sequence issues total={sequence_issues['count']}")
    if two_read_bad:
        _issue(b_issues, f"two_read_equal bad generations={two_read_bad}")
    if nonfinite:
        _issue(b_issues, f"non-finite components={nonfinite}")

    tsc_issues, timing = check_tsc(artifact["records"])
    f_issues = _new_issues()
    for message in tsc_issues["sample"]:
        _issue(f_issues, message)
    if tsc_issues["count"] > len(tsc_issues["sample"]):
        _issue(f_issues, f"TSC issues total={tsc_issues['count']}")
    if len(artifact["records"]) != GRID_GENERATION_COUNT:
        _issue(f_issues, "incomplete generation set")

    truth_result = compare_truth(artifact, truth)

    log_info = None
    a_issues = _new_issues()
    for message in header_issues["sample"]:
        _issue(a_issues, message)
    if header_issues["count"] > len(header_issues["sample"]):
        _issue(a_issues, f"header issues total={header_issues['count']}")
    if identity_bad:
        _issue(a_issues, f"identity_ok bad generations={identity_bad}")

    if log_path:
        log_info = parse_log(log_path)
        reconciled = reconcile_log(log_info, artifact, allow_oar_exemption)
        for message in reconciled["a_issues"]["sample"]:
            _issue(a_issues, message)
        if reconciled["a_issues"]["count"] > len(reconciled["a_issues"]["sample"]):
            _issue(a_issues, f"log A issues total={reconciled['a_issues']['count']}")
        for message in reconciled["b_issues"]["sample"]:
            _issue(b_issues, message)
        if reconciled["b_issues"]["count"] > len(reconciled["b_issues"]["sample"]):
            _issue(b_issues, f"log B issues total={reconciled['b_issues']['count']}")
        e_issues = reconciled["e_issues"]
        log_info["reconciliation"] = reconciled
        a_verdict = _verdict(a_issues)
        e_verdict = _verdict(e_issues)
        e_scope = "log"
        e_limitations = ["PDB/INI hashes and save identity are not present in the log"]
    else:
        a_verdict = "NOT_EVALUATED"
        _issue(a_issues, "no --log supplied; manifest/carrier uniqueness not checked")
        e_issues = _new_issues()
        _issue(e_issues, "no --log supplied")
        e_verdict = "NOT_EVALUATED"
        e_scope = "none"
        e_limitations = ["no log supplied"]

    result = {
        "label": f"run{index}",
        "artifact": {
            "path": artifact["path"],
            "basename": artifact["basename"],
            "sha256": artifact["sha256"],
            "size": artifact["size"],
            "identity": artifact["identity"],
            "recorded": artifact["recorded"],
            "capacity": artifact["capacity"],
            "track_count": artifact["track_count"],
            "data_fnv1a64": f"{artifact['data_fnv1a64']:016X}",
        },
        "header_issues": header_issues,
        "sequence_issues": sequence_issues,
        "counts": {
            "records": len(artifact["records"]),
            "two_read_bad": two_read_bad,
            "identity_bad": identity_bad,
            "nonfinite": nonfinite,
        },
        "c2_a": {
            "verdict": a_verdict,
            "scope": "artifact+log" if log_info else "artifact",
            "issues": a_issues,
        },
        "c2_b": {
            "verdict": _verdict(b_issues),
            "issues": b_issues,
        },
        "c2_c": {
            "verdict": truth_result["verdict"],
            "issues": truth_result["issues"],
            "components_checked": truth_result["components_checked"],
            "nonzero": truth_result["nonzero"],
            "violations_count": len(truth_result["violations"]),
            "max_ratio": truth_result["max_ratio"],
            "worst": truth_result["worst"],
            "per_component": truth_result["per_component"],
            "per_time": truth_result["per_time"],
        },
        "c2_e": {
            "verdict": e_verdict,
            "scope": e_scope,
            "issues": e_issues,
            "limitations": e_limitations,
            "log": log_info,
        },
        "c2_f": {
            "verdict": _verdict(f_issues),
            "issues": f_issues,
            "timing": timing,
        },
        "_artifact": artifact,
        "_violations": truth_result["violations"],
    }
    return result


def cross_process(runs, out_dir):
    if len(runs) < 2:
        return {
            "verdict": "NOT_READY",
            "pairs": [],
            "required_processes": 3,
            "reason": f"only {len(runs)} process artifact(s)",
        }
    complete = [run for run in runs if len(run["_artifact"]["records"]) == GRID_GENERATION_COUNT]
    if len(complete) < 2:
        return {
            "verdict": "NOT_EVALUATED",
            "pairs": [],
            "required_processes": 3,
            "reason": "fewer than two complete artifacts",
        }
    reference = complete[0]["_artifact"]["records"]
    reference_keys = [(record["ordinal"], record["time_bits"]) for record in reference]
    for run in complete[1:]:
        keys = [(record["ordinal"], record["time_bits"])
                for record in run["_artifact"]["records"]]
        if keys != reference_keys:
            return {
                "verdict": "NOT_EVALUATED",
                "pairs": [],
                "required_processes": 3,
                "reason": f"{run['label']} ordinal/time grid differs from {complete[0]['label']}",
            }

    component_count = GRID_GENERATION_COUNT * REGISTERED_TRACKS * GRID_COMPONENT_COUNT
    pair_count = len(complete) * (len(complete) - 1) // 2
    detail_bytes = pair_count * component_count * 8
    header = CROSS_HEADER.pack(CROSS_MAGIC, CROSS_VERSION, CROSS_HEADER_SIZE,
                               len(complete), pair_count, component_count, 0, detail_bytes)
    detail_path = None
    pairs = []
    with open(os.path.join(out_dir, "cal2-cross-process-diff.bin"), "wb") as handle:
        handle.write(header)
        offset = CROSS_HEADER_SIZE
        for left in range(len(complete)):
            for right in range(left + 1, len(complete)):
                left_records = complete[left]["_artifact"]["records"]
                right_records = complete[right]["_artifact"]["records"]
                exact = 0
                nonzero = 0
                max_abs = 0.0
                max_ulp = 0
                max_abs_at = None
                per_time = [0.0] * GRID_GENERATION_COUNT
                per_component = [0.0] * GRID_COMPONENT_COUNT
                for ordinal in range(GRID_GENERATION_COUNT):
                    left_data = left_records[ordinal]["data"]
                    right_data = right_records[ordinal]["data"]
                    for track in range(REGISTERED_TRACKS):
                        base = track * GRID_COMPONENT_COUNT
                        for component in range(GRID_COMPONENT_COUNT):
                            a_bits = left_data[base + component]
                            b_bits = right_data[base + component]
                            if a_bits == b_bits:
                                exact += 1
                            else:
                                nonzero += 1
                            difference = abs(f32_from_bits(a_bits) - f32_from_bits(b_bits))
                            handle.write(struct.pack("<d", difference))
                            if difference > max_abs:
                                max_abs = difference
                                max_abs_at = {
                                    "ordinal": ordinal, "track": track,
                                    "component": COMPONENT_NAMES[component],
                                    "left_bits": f"{a_bits:08X}",
                                    "right_bits": f"{b_bits:08X}",
                                }
                            if difference > per_time[ordinal]:
                                per_time[ordinal] = difference
                            if difference > per_component[component]:
                                per_component[component] = difference
                            ulp = ulp_distance(a_bits, b_bits)
                            if ulp > max_ulp:
                                max_ulp = ulp
                pairs.append({
                    "left": complete[left]["label"],
                    "right": complete[right]["label"],
                    "exact": exact,
                    "nonzero": nonzero,
                    "components": component_count,
                    "max_abs": max_abs,
                    "max_ulp": max_ulp,
                    "max_abs_at": max_abs_at,
                    "per_time_max_abs": per_time,
                    "per_component_max_abs": per_component,
                    "detail_offset": offset,
                    "detail_bytes": component_count * 8,
                })
                offset += component_count * 8
        detail_path = os.path.join(out_dir, "cal2-cross-process-diff.bin")
    return {
        "verdict": "PASS" if len(complete) == 3 else "NOT_READY",
        "pairs": pairs,
        "required_processes": 3,
        "detail": {
            "path": detail_path,
            "bytes": os.path.getsize(detail_path),
            "sha256": sha256_file(detail_path),
        },
    }


def write_violations(out_dir, runs):
    rows = []
    for run in runs:
        for row in run["_violations"]:
            rows.append((run["label"], row))
    if not rows:
        return None
    path = os.path.join(out_dir, "cal2-violations.csv")
    with open(path, "w", encoding="utf-8", newline="") as handle:
        handle.write("run,ordinal,track,component,runtime_bits,truth_bits,runtime,truth,difference,bound,ratio,kind\n")
        for label, row in rows:
            handle.write(
                f"{label},{row['ordinal']},{row['track']},{row['component']},"
                f"{row['runtime_bits']:08X},{row['truth_bits']:08X},"
                f"{row['runtime']:.9g},{row['truth']:.9g},{row['difference']:.9g},"
                f"{row['bound']:.9g},{row['ratio']:.9g},{row['kind']}\n")
    return {"path": path, "bytes": os.path.getsize(path), "sha256": sha256_file(path)}


def render_summary(report):
    lines = ["CAL2 ADJUDICATION C-G5-CAL2 v1"]
    truth = report["truth"]
    lines.append(f"TRUTH path='{truth['path']}' sha256={truth['sha256']} times=121 tracks=99 "
                 f"blocks={truth['blocks']}")
    for run in report["runs"]:
        artifact = run["artifact"]
        lines.append(f"RUN {run['label']} artifact='{artifact['basename']}' sha256={artifact['sha256']} "
                     f"bytes={artifact['size']} identity='{artifact['identity']}' recorded={artifact['recorded']}")
        a = run["c2_a"]
        lines.append(f"RUN {run['label']} C2-A {a['verdict']} scope={a['scope']} "
                     f"issues={a['issues']['count']}")
        b = run["c2_b"]
        lines.append(f"RUN {run['label']} C2-B {b['verdict']} records={run['counts']['records']} "
                     f"two_read_bad={run['counts']['two_read_bad']} nonfinite={run['counts']['nonfinite']} "
                     f"issues={b['issues']['count']}")
        c = run["c2_c"]
        max_ratio = "none" if c["max_ratio"] is None else f"{c['max_ratio']:.6g}"
        lines.append(f"RUN {run['label']} C2-C {c['verdict']} components={c['components_checked']} "
                     f"nonzero={c['nonzero']} violations={c['violations_count']} max_ratio={max_ratio} "
                     f"issues={c['issues']['count']}")
        f = run["c2_f"]
        timing = f["timing"]
        med = timing["first_read"].get("median") if timing["first_read"].get("count") else None
        lines.append(f"RUN {run['label']} C2-F {f['verdict']} tsc_issues={f['issues']['count']} "
                     f"first_median={med} first_max={timing['first_read'].get('max')}")
        e = run["c2_e"]
        log = e["log"]
        if log and log.get("reconciliation"):
            rec = log["reconciliation"]
            lines.append(f"RUN {run['label']} C2-E {e['verdict']} scope={e['scope']} "
                         f"manifest={rec['manifest_count']} carrier={rec['carrier_count']} "
                         f"close={rec['close_count']} errors={rec['errors_total']} "
                         f"oar={rec['errors_oar']} exempt_oar={rec['errors_exempt_oar']} "
                         f"exemption={rec['oar_exemption']} unexpected={rec['errors_unexpected']} "
                         f"issues={e['issues']['count']}")
        else:
            lines.append(f"RUN {run['label']} C2-E {e['verdict']} scope={e['scope']} "
                         f"issues={e['issues']['count']}")
        for issue in (a["issues"]["sample"] + b["issues"]["sample"] + c["issues"]["sample"] +
                      e["issues"]["sample"] + f["issues"]["sample"])[:3]:
            lines.append(f"RUN {run['label']} ISSUE {issue}")
    cross = report["cross_process"]
    if cross["pairs"]:
        for pair in cross["pairs"]:
            lines.append(f"CROSS {pair['left']}-{pair['right']} components={pair['components']} "
                         f"exact={pair['exact']} nonzero={pair['nonzero']} max_abs={pair['max_abs']:.9g} "
                         f"max_ulp={pair['max_ulp']}")
    else:
        lines.append(f"CROSS none reason='{cross.get('reason', 'not available')}'")
    batch = report["batch"]
    lines.append(f"BATCH C2-D {batch['c2_d']['verdict']} runs={len(report['runs'])} required=3 "
                 f"reasons={'; '.join(batch['c2_d']['reasons'])[:240]}")
    lines.append(f"VERDICT {batch['verdict']}")
    if len(lines) > SUMMARY_LINE_BUDGET:
        raise ParseError(f"summary {len(lines)} lines exceeds budget {SUMMARY_LINE_BUDGET}")
    return "\n".join(lines) + "\n"


def ensure_output_dir(path: str, force: bool):
    if os.path.exists(path) and not os.path.isdir(path):
        raise ParseError(f"--out-dir is not a directory: {path}")
    if os.path.isdir(path) and os.listdir(path) and not force:
        raise ParseError(f"--out-dir is not empty (pass --force to overwrite): {path}")
    os.makedirs(path, exist_ok=True)


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--artifact", action="append", required=True,
                        help="CGRD v1 artifact (.bin); repeat for each process")
    parser.add_argument("--log", action="append", default=[],
                        help="CharacterPanel.log for the corresponding artifact; repeat in the same order")
    parser.add_argument("--truth", default=DEFAULT_TRUTH,
                        help="121-time truth grid with BOUNDS section (default: registered grid)")
    parser.add_argument("--out-dir", required=True, help="absent/empty output directory")
    parser.add_argument("--force", action="store_true", help="allow a non-empty output directory")
    parser.add_argument("--track2-oar-exemption", action="store_true",
                        help="apply the documented Track 2 exemption for the exact "
                             "'failed to get plugin info for OpenAnimationReplacer' [E] row")
    args = parser.parse_args(argv)

    try:
        if args.log and len(args.log) != len(args.artifact):
            raise ParseError("--log must be supplied once per --artifact, in the same order")
        truth = parse_truth(args.truth)
        ensure_output_dir(args.out_dir, args.force)
        runs = [adjudicate_run(index + 1, artifact, truth,
                               args.log[index] if args.log else None,
                               args.track2_oar_exemption)
                for index, artifact in enumerate(args.artifact)]
        cross = cross_process(runs, args.out_dir)
        violations = write_violations(args.out_dir, runs)

        run_verdicts = [[run["c2_a"]["verdict"], run["c2_b"]["verdict"],
                         run["c2_c"]["verdict"], run["c2_e"]["verdict"],
                         run["c2_f"]["verdict"]] for run in runs]
        any_invalid = any("FAIL" in verdicts for verdicts in run_verdicts)
        all_pass = all(all(verdict == "PASS" for verdict in verdicts) for verdicts in run_verdicts)
        missing = 3 - len(runs)
        if any_invalid:
            c2d = "FAIL"
            reasons = ["at least one run failed a per-process criterion"]
        elif missing > 0:
            c2d = "NOT_READY"
            reasons = [f"missing {missing} independent process(es)"]
        elif all_pass and cross["verdict"] == "PASS":
            c2d = "PASS"
            reasons = []
        else:
            c2d = "NOT_READY"
            reasons = ["per-process criteria or cross-process evidence incomplete"]
        batch_verdict = "INVALID" if any_invalid else ("READY" if c2d == "PASS" else "NOT_READY")

        report = {
            "schema": "C-G5-CAL2-v1-adjudication",
            "protocol": "C-G5-CAL2 v1 (docs/diagnostics/g5-cal2-20261115/README.md §9.7)",
            "generated_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
            "tool": os.path.abspath(__file__),
            "truth": {
                "path": truth["path"], "sha256": truth["sha256"], "size": truth["size"],
                "blocks": truth["blocks"], "times": len(truth["times"]),
                "tracks": len(truth["tracks"][0]), "integrity_fnv1a64":
                    f"{truth['integrity_fnv1a64']:016X}" if truth["integrity_fnv1a64"] is not None else None,
            },
            "runs": runs,
            "cross_process": cross,
            "batch": {
                "c2_d": {"verdict": c2d, "reasons": reasons},
                "verdict": batch_verdict,
                "note": "C2-D/overall READY requires three independent processes and every C2-A..F criterion to pass; no tau_* is produced.",
            },
        }
        for run in runs:
            del run["_artifact"]
            del run["_violations"]

        summary = render_summary(report)
        summary_path = os.path.join(args.out_dir, "cal2-adjudication-summary.txt")
        with open(summary_path, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(summary)
        outputs = {
            "summary": {"path": summary_path, "bytes": os.path.getsize(summary_path),
                        "sha256": sha256_file(summary_path)},
        }
        if violations:
            outputs["violations"] = violations
        if cross.get("detail"):
            outputs["cross_process_detail"] = cross["detail"]
        report["outputs"] = outputs
        json_path = os.path.join(args.out_dir, "cal2-adjudication.json")
        with open(json_path, "w", encoding="utf-8", newline="\n") as handle:
            json.dump(report, handle, indent=1, ensure_ascii=False)
            handle.write("\n")
        sys.stdout.write(summary)
        return 2 if any_invalid else 0
    except (ParseError, OSError, ValueError, KeyError, struct.error) as error:
        print(f"[GUARD FAIL] {error}", file=sys.stderr)
        return 3


if __name__ == "__main__":
    sys.exit(main())
