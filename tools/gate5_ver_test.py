"""gate5_ver T1–T12（C-G5-VER v1 §9.2.13，2026-11-22 冻结清单）。

纯 stdlib、`python -I` 可跑、显式 sys.path 引导；期望值全部解析计算，
不调用被测函数生成答案。合成 CPK5/CPK5S 布局与 gate4_pair 解析器逐字一致
（候选头 20+96 身份=116 B；源头 28+96=124 B；记录 = meta + 8×K5_CLIP + 116×40）。

运行：python -I tools/gate5_ver_test.py   或   python -I tools/gate5_ver.py --self-test
"""
from __future__ import annotations

import json
import math
import struct
import sys
import tempfile
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS))

import gate4_pair as g  # noqa: E402
import gate5_ver as v  # noqa: E402

FAILURES: list[str] = []


def check(name: str, cond: bool, detail: str = "") -> None:
    if cond:
        print(f"  PASS {name}")
    else:
        FAILURES.append(f"{name}: {detail}")
        print(f"  FAIL {name} {detail}")


def f32bits(value: float) -> int:
    return struct.unpack("<I", struct.pack("<f", value))[0]


def clip_slot(name: str, lt: float, dur: float, dfnv: int,
              node: int = 0x1000, binding: int = 0x2000, bidx: int = 0) -> bytes:
    return g.K5_CLIP.pack(node, binding, bidx, f32bits(lt), b"\x00\x00\x00\x00",
                          name.encode()[:64].ljust(64, b"\x00"), f32bits(dur),
                          dfnv, 0, 0, 0)


IDENT = ((0.0, 0.0, 0.0), (0.0, 0.0, 0.0, 1.0), (1.0, 1.0, 1.0))


def qrot_x(deg: float) -> tuple:
    rad = math.radians(deg) / 2.0
    return (math.sin(rad), 0.0, 0.0, math.cos(rad))


def track_block(overrides: dict | None = None) -> bytes:
    overrides = overrides or {}
    cells = []
    for t in range(g.TRACK_COUNT):
        value = overrides.get(t, IDENT)
        cells.append(struct.pack("<10f", *value[0], *value[1], *value[2]))
    return b"".join(cells)


def cand_record(segment: int, completed: int, clips: list[dict],
                overrides: dict | None = None) -> bytes:
    meta = g.K5_CANDIDATE_META.pack(segment, completed, 0, g.TRACK_COUNT, 1,
                                    len(clips), 0)
    slots = b"".join(clip_slot(**c) for c in clips)
    slots += b"\x00" * ((g.CLIP_CAPACITY - len(clips)) * g.K5_CLIP.size)
    return meta + slots + track_block(overrides)


def src_record(seq: int, clips: list[dict]) -> bytes:
    meta = g.K5_SOURCE_META.pack(1000 + seq, seq, g.TRACK_COUNT, len(clips),
                                 0, 1, 0, 0)
    slots = b"".join(clip_slot(**c) for c in clips)
    slots += b"\x00" * ((g.CLIP_CAPACITY - len(clips)) * g.K5_CLIP.size)
    return meta + slots + track_block()


def cand_bin(records: list[bytes], tmp: Path) -> Path:
    head = g.K5_CANDIDATE_HEADER.pack(b"CPK5", 1, g.CAND_HEADER_SIZE, 1,
                                      g.TRACK_COUNT, len(records))
    blob = head + b"test-candidate".ljust(96, b"\x00") + b"".join(records)
    path = tmp / "cand.bin"
    path.write_bytes(blob)
    return path


def src_bin(records: list[bytes], tmp: Path) -> Path:
    head = g.K5_SOURCE_HEADER.pack(b"CPK5", 1, g.SRC_HEADER_SIZE, 1, 1,
                                   g.TRACK_COUNT, g.SRC_RECORD_SIZE, len(records))
    blob = head + b"test-source".ljust(96, b"\x00") + b"".join(records)
    path = tmp / "src.bin"
    path.write_bytes(blob)
    return path


def equip_clip(lt: float, node: int = 0x1000, dur: float = 1.4333,
               dfnv: int = 123) -> dict:
    return {"name": "1HM_Equip", "lt": lt, "dur": dur, "dfnv": dfnv, "node": node}


def idle_clip(lt: float, node: int = 0x3000, dur: float = 9.0,
              dfnv: int = 456) -> dict:
    return {"name": "1HM_Idle", "lt": lt, "dur": dur, "dfnv": dfnv, "node": node}


def mlh_clip(lt: float) -> dict:
    return {"name": "MLh_Idle", "lt": lt, "dur": 2.53, "dfnv": 789, "node": 0x4000}


def write_tolerances(tmp: Path, **kw) -> Path:
    base = {"tau_p": 4.76592e-4, "tau_r": 6.03341e-8, "tau_s": 3.86264e-6,
            "epsilon_t": 1.0}
    base.update(kw)
    path = tmp / "tol.json"
    path.write_text(json.dumps(base), encoding="utf-8")
    return path


def load_manifest(out: Path) -> dict:
    return json.loads((out / "gate5-ver-manifest.json").read_text(encoding="utf-8"))


def kinds_of(manifest: dict) -> dict:
    return {p["unit_record"]: p["kind"] for p in manifest["pairs"]}


# T1 burst 分组：相邻等钟聚合 + stable_id 变更切分 + 时钟变更切分 + 域变更切分
def t1(tmp: Path) -> None:
    recs = [
        cand_record(0, 30, [equip_clip(0.10, node=0x1000)]),
        cand_record(0, 31, [equip_clip(0.10, node=0x1000)]),
        cand_record(0, 32, [equip_clip(0.10, node=0x2000)]),
        cand_record(0, 33, [equip_clip(0.20, node=0x1000)]),
        cand_record(0, 34, [idle_clip(0.10)]),
    ]
    cand = g.parse_candidate(str(cand_bin(recs, tmp)))
    v.prepare(cand)
    bursts, no_clock = v.build_bursts(cand)
    check("T1 burst count", len(bursts) == 4, f"got {len(bursts)}")
    check("T1 burst sizes", [len(b) for b in bursts] == [2, 1, 1, 1],
          f"got {[len(b) for b in bursts]}")
    check("T1 no_clock", no_clock == 0, f"got {no_clock}")


# T2 比较单元 = burst 末步
def t2(tmp: Path) -> None:
    recs = [
        cand_record(0, 30, [equip_clip(0.10)]),
        cand_record(0, 31, [equip_clip(0.10)]),
        cand_record(0, 32, [equip_clip(0.10)]),
    ]
    cand = g.parse_candidate(str(cand_bin(recs, tmp)))
    v.prepare(cand)
    bursts, _ = v.build_bursts(cand)
    entries, _ = v.classify_bursts(bursts)
    check("T2 unit is last", entries[0]["unit_record"] == 2,
          f"got {entries[0]['unit_record']}")
    check("T2 mid records", entries[0]["mid_burst_records"] == [0, 1],
          f"got {entries[0]['mid_burst_records']}")


# T3 clip 末段饱和判据边界
def t3(tmp: Path) -> None:
    dur = 1.4333
    near = dur - 1.5 / 60.0  # 1.408333…
    recs = [
        # kept: len 2, lt 低于界
        cand_record(0, 30, [equip_clip(1.400, node=0x1000, dur=dur)]),
        cand_record(0, 31, [equip_clip(1.400, node=0x1000, dur=dur)]),
        # kept: len 2, lt 越界但 duration 不可读（0）
        cand_record(0, 32, [equip_clip(near + 0.002, node=0x2000, dur=0.0)]),
        cand_record(0, 33, [equip_clip(near + 0.002, node=0x2000, dur=0.0)]),
        # excluded_saturation: len 2, lt 越界
        cand_record(0, 34, [equip_clip(near + 0.004, node=0x3000, dur=dur)]),
        cand_record(0, 35, [equip_clip(near + 0.004, node=0x3000, dur=dur)]),
        # kept: len 1 即便越界（单记录墙帧自锚）
        cand_record(0, 36, [equip_clip(near + 0.006, node=0x4000, dur=dur)]),
    ]
    cand = g.parse_candidate(str(cand_bin(recs, tmp)))
    v.prepare(cand)
    bursts, _ = v.build_bursts(cand)
    entries, counters = v.classify_bursts(bursts)
    kinds = [e["kind"] for e in entries]
    check("T3 kinds", kinds == ["endpoint", "endpoint", "excluded_saturation", "endpoint"],
          f"got {kinds}")
    check("T3 counter", counters["excluded_saturation_bursts"] == 1, f"got {counters}")


# T4 EQ-W1b completed 边界
def t4(tmp: Path) -> None:
    recs = [
        cand_record(0, 24, [equip_clip(0.10, node=0x1000)]),
        cand_record(0, 25, [equip_clip(0.20, node=0x2000)]),
    ]
    cand = g.parse_candidate(str(cand_bin(recs, tmp)))
    v.prepare(cand)
    bursts, _ = v.build_bursts(cand)
    entries, counters = v.classify_bursts(bursts)
    kinds = [e["kind"] for e in entries]
    check("T4 kinds", kinds == ["excluded_w1b", "endpoint"], f"got {kinds}")
    check("T4 counter", counters["excluded_w1b_bursts"] == 1, f"got {counters}")


# T5 EQ-W1a 源侧双主族共存排除（常驻 mlh/scar 不算共存）
def t5(tmp: Path) -> None:
    recs = [
        cand_record(0, 30, [equip_clip(0.50, node=0x1000)]),
        cand_record(0, 40, [equip_clip(0.60, node=0x2000)]),
    ]
    srcs = [
        src_record(1, [equip_clip(0.50), idle_clip(3.0)]),      # 双主族 → w1a
        src_record(2, [equip_clip(0.60), mlh_clip(1.0)]),       # 单主族 → 计入
    ]
    tol = write_tolerances(tmp)
    out = tmp / "out"
    rc = v.main(["--candidate", str(cand_bin(recs, tmp)), "--source", str(src_bin(srcs, tmp)),
                 "--out-dir", str(out), "--tolerances", str(tol)])
    check("T5 exit", rc == 0, f"rc={rc}")
    manifest = load_manifest(out)
    kinds = {p["src_seq"]: p["flags"] for p in manifest["pairs"]}
    check("T5 w1a flag", "w1a" not in kinds[1] and manifest["ledger"]["pairing"]["w1a_excluded"] == 1,
          f"flags={kinds}")
    check("T5 counted", manifest["ledger"]["pairing"]["equivalence_counted"] == 1,
          f"got {manifest['ledger']['pairing']}")


# T6 nearest-clock 等距平手取 seq 早
def t6(tmp: Path) -> None:
    recs = [cand_record(0, 30, [equip_clip(1.10, node=0x1000)])]
    srcs = [
        src_record(7, [equip_clip(1.00)]),
        src_record(3, [equip_clip(1.20, node=0x9000)]),
    ]
    out = tmp / "out"
    rc = v.main(["--candidate", str(cand_bin(recs, tmp)), "--source", str(src_bin(srcs, tmp)),
                 "--out-dir", str(out)])
    check("T6 exit", rc == 0, f"rc={rc}")
    manifest = load_manifest(out)
    check("T6 earlier seq", manifest["pairs"][0]["src_seq"] == 3,
          f"got {manifest['pairs'][0]['src_seq']}")


# T7 DECL 分量映射（t45/t60-66 各通道）
def t7(tmp: Path) -> None:
    rot = (0.0, 0.0, 0.0)
    override = {
        5: ((1.0, 0.0, 0.0), qrot_x(10.0), (1.0, 1.0, 1.0)),     # 控制轨：pos+rot 都判
        31: ((1.0, 0.0, 0.0), qrot_x(10.0), (1.0, 1.0, 1.0)),    # DECL1：pos 判，rot 不判
        45: ((1.0, 0.0, 0.0), qrot_x(10.0), (1.0, 1.0, 1.0)),    # DECL1+t45：全不判
        60: ((1.0, 0.0, 0.0), qrot_x(10.0), (1.0, 1.0, 1.0)),    # DECL2：rot 判，pos 不判
    }
    recs = [cand_record(0, 30, [equip_clip(0.50, node=0x1000)], overrides=override)]
    srcs = [src_record(1, [equip_clip(0.50)])]
    tol = write_tolerances(tmp)
    out = tmp / "out"
    rc = v.main(["--candidate", str(cand_bin(recs, tmp)), "--source", str(src_bin(srcs, tmp)),
                 "--out-dir", str(out), "--tolerances", str(tol)])
    check("T7 exit", rc == 0, f"rc={rc}")
    acc = load_manifest(out)["accounting"]
    eq = acc["equivalence"]
    check("T7 tau_p", eq["tau_p"] == 2, f"got {eq['tau_p']}")   # t5 + t31
    check("T7 tau_r", eq["tau_r"] == 2, f"got {eq['tau_r']}")   # t5 + t60
    check("T7 tau_s", eq["tau_s"] == 0, f"got {eq['tau_s']}")
    check("T7 per_track", acc["per_track_exceed"].get("t45") is None
          and acc["per_track_exceed"].get("t31", {}).get("r") == 0
          and acc["per_track_exceed"].get("t31", {}).get("p") == 1
          and acc["per_track_exceed"].get("t60", {}).get("r") == 1,
          f"got {acc['per_track_exceed']}")
    check("T7 decl buckets", "t31_rot" in acc["decl_measured"]
          and "t45_rot" in acc["decl_measured"] and "t60_pos" in acc["decl_measured"],
          f"got {sorted(acc['decl_measured'])}")
    t31 = acc["decl_measured"]["t31_rot"]
    check("T7 decl value", t31["n"] == 1 and abs(t31["median"] - 10.0) < 1e-6,
          f"got {t31}")


# T8 dt 门：edge-only 排除 + 记账；非 edge 排除 → INVALID；超 2% → INVALID
def t8(tmp: Path) -> None:
    # 8a: edge-only dt 排除（候选钟低于源域下界；51 对中 1 对 = 1.96% ≤ 2%）
    recs = [cand_record(0, 30, [equip_clip(0.01, node=0x0500)])]
    recs += [cand_record(0, 31 + i, [equip_clip(0.50, node=0x1000 + i)]) for i in range(50)]
    srcs = [src_record(1 + i, [equip_clip(0.50, node=0x2000 + i)]) for i in range(50)]
    tol = write_tolerances(tmp, epsilon_t=1e-3)
    out = tmp / "out8a"
    rc = v.main(["--candidate", str(cand_bin(recs, tmp)), "--source", str(src_bin(srcs, tmp)),
                 "--out-dir", str(out), "--tolerances", str(tol)])
    check("T8a exit", rc == 0, f"rc={rc}")
    manifest = load_manifest(out)
    flags = {p["unit_record"]: p["flags"] for p in manifest["pairs"]}
    check("T8a edge dt excluded", manifest["ledger"]["pairing"]["dt_excluded"] == 1
          and "dt-excluded" in flags[0] and "edge-pair" in flags[0]
          and manifest["ledger"]["pairing"]["equivalence_counted"] == 50,
          f"rc={rc} {manifest['ledger']['pairing']}")

    # 8b: 非 edge dt 排除 → INVALID
    recs = [cand_record(0, 30, [equip_clip(0.20, node=0x1000)])]
    srcs = [src_record(1, [equip_clip(0.00)]), src_record(2, [equip_clip(0.50, node=0x9000)])]
    out = tmp / "out8b"
    rc = v.main(["--candidate", str(cand_bin(recs, tmp)), "--source", str(src_bin(srcs, tmp)),
                 "--out-dir", str(out), "--tolerances", str(tol)])
    check("T8b non-edge INVALID", rc == 2, f"rc={rc}")

    # 8c: 排除比例 > 2% → INVALID（1/1）
    recs = [cand_record(0, 30, [equip_clip(0.01, node=0x1000)]),
            cand_record(0, 40, [equip_clip(0.60, node=0x2000)])]
    srcs = [src_record(1, [equip_clip(0.50)]), src_record(2, [equip_clip(0.60, node=0x9000)])]
    out = tmp / "out8c"
    rc = v.main(["--candidate", str(cand_bin(recs, tmp)), "--source", str(src_bin(srcs, tmp)),
                 "--out-dir", str(out), "--tolerances", str(tol)])
    check("T8c ratio INVALID", rc == 2, f"rc={rc}")


# T9 IDUN 排除与账本闭合
def t9(tmp: Path) -> None:
    bad = {"name": "Junk", "lt": 0.1, "dur": 0.0, "dfnv": 0, "node": 0x5000}
    recs = [
        cand_record(0, 30, [equip_clip(0.50, node=0x1000), bad]),   # 单元 IDUN → 排除
        cand_record(0, 40, [equip_clip(0.60, node=0x2000)]),        # 正常端点
    ]
    srcs = [src_record(1, [equip_clip(0.50)]), src_record(2, [equip_clip(0.60, node=0x9000)])]
    tol = write_tolerances(tmp)
    out = tmp / "out"
    rc = v.main(["--candidate", str(cand_bin(recs, tmp)), "--source", str(src_bin(srcs, tmp)),
                 "--out-dir", str(out), "--tolerances", str(tol)])
    check("T9 exit", rc == 0, f"rc={rc}")
    manifest = load_manifest(out)
    led = manifest["ledger"]["candidate"]
    kinds = kinds_of(manifest)
    check("T9 idun excluded", led["excluded_idun_units"] == 1
          and led["endpoints"] == 1 and list(kinds.values()) == ["endpoint"],
          f"got {led} {kinds}")
    check("T9 closure", led["closure"] is True, f"got {led}")

    # 源侧 IDUN：整域无可用源记录 → no-partner
    recs = [cand_record(0, 30, [equip_clip(0.50, node=0x1000)])]
    srcs = [src_record(1, [equip_clip(0.50), bad])]
    out = tmp / "out9b"
    rc = v.main(["--candidate", str(cand_bin(recs, tmp)), "--source", str(src_bin(srcs, tmp)),
                 "--out-dir", str(out),
                 "--tolerances", str(write_tolerances(tmp, epsilon_t=1.0))])
    manifest = load_manifest(out)
    check("T9b no_partner", rc == 0
          and manifest["ledger"]["pairing"]["no_partner"] == 1
          and manifest["pairs"][0]["flags"] == ["no-partner"],
          f"rc={rc} {manifest['pairs'][0]}")


# T10 合成 CPK5/CPK5S 解析往返
def t10(tmp: Path) -> None:
    recs = [cand_record(2, 87, [equip_clip(1.4333, node=0x1000, dur=1.4333)])]
    cand = g.parse_candidate(str(cand_bin(recs, tmp)))
    check("T10 cand count", cand["count"] == 1, f"got {cand['count']}")
    check("T10 cand fields", cand["records"][0]["segment"] == 2
          and cand["records"][0]["completed"] == 87
          and cand["records"][0]["clips"][0]["bare"] == "1hm_equip"
          and abs(cand["records"][0]["clips"][0]["local_time"] - 1.4333) < 1e-6
          and abs(cand["records"][0]["clips"][0]["duration_bits"] / 4294967296.0 * 0
                  + struct.unpack("<f", struct.pack("<I", cand["records"][0]["clips"][0]["duration_bits"]))[0]
                  - 1.4333) < 1e-6,
          f"got {cand['records'][0]}")
    srcs = [src_record(5, [idle_clip(3.25)])]
    src = g.parse_source(str(src_bin(srcs, tmp)))
    check("T10 src fields", src["count"] == 1 and src["records"][0]["seq"] == 5
          and src["records"][0]["clips"][0]["bare"] == "1hm_idle"
          and abs(src["records"][0]["clips"][0]["local_time"] - 3.25) < 1e-6,
          f"got {src['records'][0]}")


# T11 同参两次运行字节一致
def t11(tmp: Path) -> None:
    recs = [
        cand_record(0, 30, [equip_clip(0.50, node=0x1000)]),
        cand_record(0, 40, [equip_clip(0.60, node=0x2000)]),
    ]
    srcs = [src_record(1, [equip_clip(0.50)]), src_record(2, [equip_clip(0.60, node=0x9000)])]
    cpath = str(cand_bin(recs, tmp))
    spath = str(src_bin(srcs, tmp))
    tol = str(write_tolerances(tmp))
    outs = []
    for i in (1, 2):
        out = tmp / f"out{i}"
        rc = v.main(["--candidate", cpath, "--source", spath, "--out-dir", str(out),
                     "--tolerances", tol])
        check(f"T11 run{i} exit", rc == 0, f"rc={rc}")
        outs.append((out / "gate5-ver-manifest.json").read_bytes()
                    + (out / "gate5-ver-summary.txt").read_bytes()
                    + (out / "gate5-ver-detail.bin").read_bytes())
    check("T11 byte-identical", outs[0] == outs[1], "outputs differ")


# T12 无 --tolerances：metadata-only，拒绝 pose 解码
def t12(tmp: Path) -> None:
    recs = [cand_record(0, 30, [equip_clip(0.50, node=0x1000)])]
    srcs = [src_record(1, [equip_clip(0.50)])]
    out = tmp / "out"
    rc = v.main(["--candidate", str(cand_bin(recs, tmp)), "--source", str(src_bin(srcs, tmp)),
                 "--out-dir", str(out)])
    check("T12 exit", rc == 0, f"rc={rc}")
    manifest = load_manifest(out)
    check("T12 pose_decoded", manifest["pose_decoded"] is False
          and manifest["accounting"] is None
          and not (out / "gate5-ver-detail.bin").exists()
          and manifest["ledger"]["pairing"]["paired"] == 1,
          f"got {manifest['pose_decoded']}, files={sorted(p.name for p in out.iterdir())}")


# T13 逐轨容差覆盖（C-G5-TOL v1 扩展）：per_track 表覆盖全局 tau，未覆盖轨回退全局
def t13(tmp: Path) -> None:
    # 候选轨 5 旋转 10°（超全局 tau_r）、轨 6 旋转 10°（per_track 覆盖为 20° → 不超）
    override = {
        5: ((0.0, 0.0, 0.0), qrot_x(10.0), (1.0, 1.0, 1.0)),
        6: ((0.0, 0.0, 0.0), qrot_x(10.0), (1.0, 1.0, 1.0)),
    }
    recs = [cand_record(0, 30, [equip_clip(0.50, node=0x1000)], overrides=override)]
    srcs = [src_record(1, [equip_clip(0.50)])]
    tol_path = tmp / "tol.json"
    tol_path.write_text(json.dumps({
        "tau_p": 4.76592e-4, "tau_r": 6.03341e-8, "tau_s": 3.86264e-6,
        "epsilon_t": 1.0,
        "per_track": {"rot_deg": {"6": 20.0}},
    }), encoding="utf-8")
    out = tmp / "out"
    rc = v.main(["--candidate", str(cand_bin(recs, tmp)), "--source", str(src_bin(srcs, tmp)),
                 "--out-dir", str(out), "--tolerances", str(tol_path)])
    check("T13 exit", rc == 0, f"rc={rc}")
    acc = load_manifest(out)["accounting"]
    eq = acc["equivalence"]
    # t5 超全局 tau_r 计数；t6 被逐轨 20° 覆盖不计数；pos/scale 全 0
    check("T13 per-track override", eq["tau_r"] == 1 and eq["tau_p"] == 0 and eq["tau_s"] == 0
          and acc["per_track_exceed"] == {"t5": {"p": 0, "r": 1, "s": 0}}
          and eq["per_track_tolerance"] is True,
          f"got {eq} {acc['per_track_exceed']}")


def run_all() -> int:
    tests = [t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11, t12, t13]
    with tempfile.TemporaryDirectory() as raw:
        tmp = Path(raw)
        for i, test in enumerate(tests):
            print(f"T{i + 1} {test.__name__}")
            workdir = tmp / f"w{i + 1}"
            workdir.mkdir()
            test(workdir)
    if FAILURES:
        print(f"gate5_ver_test: {len(FAILURES)} FAILURE(S)")
        for line in FAILURES:
            print(f"  - {line}")
        return 1
    print("gate5_ver_test: T1-T13 all PASS")
    return 0


if __name__ == "__main__":
    sys.exit(run_all())
