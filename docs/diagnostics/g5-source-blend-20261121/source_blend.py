"""C-G5-TRANS §八.3：源侧过渡语义取证（2026-11-21，纯离线）。

用已归档的 v103 正式批次源侧工件（CPK5S 849 条）测量 live 图自身的装备/卸下过渡：
混合窗界定、混合周期、端点时钟行为、逐帧差形状，并对照候选侧（CPK5 + 配对清单）
的 clip 相位与配对结构，回答「源是否同样混合、周期是否一致」与 A′ 处置。

输入（全部为仓库内归档，只读）：
  docs/diagnostics/g5-gate5-20261116/raw/source/…bin        CPK5S
  docs/diagnostics/g5-gate5-20261116/raw/candidate/…bin     CPK5
  docs/diagnostics/g5-gate5-20261116/formal/gate4-pair-manifest.json

用法：
  python -B docs/diagnostics/g5-source-blend-20261121/source_blend.py
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
SRC = next((GATE5 / "raw/source").glob("CharacterPanel-g5k5s-*.bin"))
CAND = next((GATE5 / "raw/candidate").glob("CharacterPanel-g5k5-*.bin"))
MANIFEST = GATE5 / "formal/gate4-pair-manifest.json"


def pose_at(parsed: dict, record: dict, track: int) -> tuple:
    base = (parsed["header_size"] + record["index"] * parsed["record_size"]
            + parsed["meta_size"] + g.CLIP_CAPACITY * g.K5_CLIP.size)
    return struct.unpack_from("<10f", parsed["blob"], base + track * 40)


def qangle(qa, qb) -> float:
    na = math.sqrt(sum(v * v for v in qa))
    nb = math.sqrt(sum(v * v for v in qb))
    if na <= 0.0 or nb <= 0.0:
        return 0.0
    dot = sum(qa[i] / na * qb[i] / nb for i in range(4))
    return 2.0 * math.acos(min(1.0, max(0.0, abs(dot))))


def clip_lt(record: dict, name: str):
    return next((c["local_time"] for c in record["clips"] if c["bare"] == name), None)


def fmt(value) -> str:
    return ("%.4f" % value) if value is not None else "-"


def main() -> int:
    src = g.parse_source(str(SRC))
    cand = g.parse_candidate(str(CAND))
    recs = src["records"]
    print(f"source records={src['count']} candidate records={cand['count']}")

    # ---- 1. clip 集变化时间线（过渡窗界定） ----
    print("\n== 1. 源侧 clip 集时间线 ==")
    prev = None
    for rec in recs:
        names = tuple(sorted(set(c["bare"] for c in rec["clips"])))
        if names != prev:
            print("  seq %3d: %s" % (rec["seq"], ", ".join(names)))
            prev = names

    # ---- 2. 四个混合窗的逐帧差与子时钟 ----
    windows = [
        ("equip 入混合", range(136, 147), ("mt_idle", "1hm_equip")),
        ("equip->idle 混合", range(196, 207), ("1hm_equip", "1hm_idle")),
        ("unequip 入混合", range(402, 413), ("1hm_idle", "1hm_unequip")),
        ("unequip->mt 混合", range(485, 497), ("1hm_unequip", "mt_idle")),
    ]
    for label, rng, (na, nb) in windows:
        print(f"\n== 2. {label}（seq {min(rng)+1}..{max(rng)}）逐帧差（99 主轨最大值）==")
        for i in rng:
            a, b = recs[i - 1], recs[i]
            max_dp = 0.0
            max_dr = 0.0
            for t in range(99):
                pa = pose_at(src, a, t)
                pb = pose_at(src, b, t)
                max_dp = max(max_dp, math.dist(pa[:3], pb[:3]))
                max_dr = max(max_dr, qangle(pb[3:7], pa[3:7]))
            print("  seq %d->%d max_dp=%.4f max_dr=%.2fdeg  %s %s->%s  %s %s->%s" % (
                a["seq"], b["seq"], max_dp, math.degrees(max_dr),
                na, fmt(clip_lt(a, na)), fmt(clip_lt(b, na)),
                nb, fmt(clip_lt(a, nb)), fmt(clip_lt(b, nb))))

    # ---- 3. 配对相位：候选 vs 源 equip clip localTime ----
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    src_by_seq = {r["seq"]: r for r in recs}
    print("\n== 3. 配对段 equip clip 相位差（cand_lt − src_lt，秒）==")
    diffs = []
    for p in manifest["pairs"]:
        c = p["cand"]
        if c["segment"] != 0:
            continue
        crec = cand["records"][c["record"]]
        srec = src_by_seq[p["src"]["seq"]]
        a = clip_lt(crec, "1hm_equip")
        b = clip_lt(srec, "1hm_equip")
        if a is None or b is None:
            continue
        diffs.append((c["completed"], a - b))
    for completed, d in diffs[:14]:
        print("  completed %3d: %+0.4f" % (completed, d))
    body = [d for _c, d in diffs]
    print("  全装备段 n=%d min=%+.4f max=%+.4f" % (len(body), min(body), max(body)))
    # 候选突发结构：同一 completed 组的 clock 与 clip lt 是否恒定
    print("\n== 4. 候选突发结构（completed 2..14 的 clip lt 与配对时钟）==")
    seen = {}
    for p in manifest["pairs"]:
        c = p["cand"]
        if c["segment"] == 0 and 2 <= c["completed"] <= 14:
            crec = cand["records"][c["record"]]
            seen[c["completed"]] = (p["clock_x"], clip_lt(crec, "1hm_equip"))
    for completed in sorted(seen):
        clock, lt = seen[completed]
        print("  completed %2d clock_x=%.6f cand_lt=%s" % (completed, clock, fmt(lt)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
