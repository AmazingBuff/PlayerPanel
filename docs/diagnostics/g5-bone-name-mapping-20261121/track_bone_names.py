"""C-G5-TRANS §八.1：门 5 逐轨超差落到骨骼名（2026-11-21，纯离线）。

输入（全部为仓库内归档，只读，与 g5-transition-window-rejudge-20261121 完全一致）：
  docs/diagnostics/g5-gate5-20261116/raw/candidate/…bin      CPK5
  docs/diagnostics/g5-gate5-20261116/raw/source/…bin         CPK5S
  docs/diagnostics/g5-gate5-20261116/formal/gate4-pair-detail.bin
  docs/diagnostics/g5-g4-cov-20261115/coverage.json
  docs/diagnostics/g5-gate4-freeze-20261116/tolerances.json
  本目录 bone-map.json（K1 普查 v101 两份日志交叉一致的 116 骨索引→名→父）

track 语义：K5 的 track i = read_pose_local(characterInstance)[i]，
即 characterInstance.numPoseLocal 姿态数组第 i 项 = animationSkeleton->bones[i]（116 骨）。
与 K1 CENSUS phase=bone index=i 同一索引空间（两份普查日志逐项一致）。

方法：不改度量/阈值/配对，复用 rejudge.py 的过滤（丢弃装备段 completed ≤ 12），
在 rest（236 对）上输出：
  1. E_p 全部超差轨（应有 20 条）→ 静态附件偏移成分 B 的完整轨名表；
  2. E_r 全部超差轨按对数排序 + 逐轨 E_r 统计；
  3. 尖峰普查：阈值 {1e-2, 1e-3, 1e-4} 下逐轨受影响对数与最大 E_r，
     以及逐对尖峰轨清单（对照文档例：pair 12 / 154 / 13）；
  4. 尖峰轨与 B 的 20 条静态轨、辅轨（item/wing/belly）的交集判断。

用法：
  python -B docs/diagnostics/g5-bone-name-mapping-20261121/track_bone_names.py
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
TOOLS = ROOT / "tools"
sys.path.insert(0, str(TOOLS))

import gate4_pair as g  # noqa: E402

REJUDGE = ROOT / "docs/diagnostics/g5-transition-window-rejudge-20261121"
GATE5 = ROOT / "docs/diagnostics/g5-gate5-20261116"
DETAIL = GATE5 / "formal/gate4-pair-detail.bin"
COVERAGE = ROOT / "docs/diagnostics/g5-g4-cov-20261115/coverage.json"
TOLERANCES = ROOT / "docs/diagnostics/g5-gate4-freeze-20261116/tolerances.json"
BONE_MAP = Path(__file__).resolve().parent / "bone-map.json"

TRANSITION_MAX_COMPLETED = 12  # 与 rejudge.py 同一预注册值
SPIKE_THRESHOLDS = (1e-2, 1e-3, 1e-4)  # 与过渡窗文档 §四 的尖峰阈值一致


def load_bone_map() -> dict[int, dict]:
    raw = json.loads(BONE_MAP.read_text(encoding="utf-8"))
    return {int(index): entry for index, entry in raw.items()}


def chain(bones: dict[int, dict], index: int, depth: int = 6) -> str:
    parts = []
    current = index
    for _ in range(depth):
        entry = bones.get(current)
        if entry is None:
            break
        parts.append(entry["name"])
        current = entry["parent"]
        if current < 0:
            break
    return " <- ".join(parts)


def load_rows() -> list[dict]:
    import struct

    cand = g.parse_candidate(str(next((GATE5 / "raw/candidate").glob("CharacterPanel-g5k5-*.bin"))))
    manifest = json.loads((GATE5 / "formal/gate4-pair-manifest.json").read_text(encoding="utf-8"))
    pairs = manifest["pairs"]
    detail_row = g.TRACK_COUNT * struct.Struct("<4f").size + struct.Struct("<IIII").size
    if len(pairs) * detail_row != DETAIL.stat().st_size:
        raise SystemExit("detail size mismatch")
    rows = []
    blob = DETAIL.read_bytes()
    for index, pair in enumerate(pairs):
        off = index * detail_row
        rec = cand["records"][pair["cand"]["record"]]
        tracks = [struct.unpack_from("<4f", blob, off + 16 + bone * 16) for bone in range(g.TRACK_COUNT)]
        rows.append({
            "pair_index": index,
            "segment": rec["segment"],
            "completed": rec["completed"],
            "tracks": tracks,
        })
    return rows


def fmt(value: float) -> str:
    return f"{value:.3g}"


def main() -> int:
    bones = load_bone_map()
    coverage = g.load_coverage(str(COVERAGE))
    tolerances = g.load_tolerances(str(TOLERANCES))
    rows = load_rows()
    kept = [r for r in rows if not (r["segment"] == 0 and r["completed"] <= TRANSITION_MAX_COMPLETED)]
    dropped = [r for r in rows if r["segment"] == 0 and r["completed"] <= TRANSITION_MAX_COMPLETED]
    primary = sorted(coverage["primary_set"])
    print(f"pairs={len(rows)} rest={len(kept)} window={len(dropped)} "
          f"tau_p={tolerances['tau_p']:g} tau_r={tolerances['tau_r']:g} tau_s={tolerances['tau_s']:g}")
    print()

    # ---- 1. E_p 静态偏移轨（rest） ----
    print("== 1. E_p 超差轨（rest 236 对）—— 成分 B：静态附件偏移 ==")
    static_tracks = []
    for track in primary:
        values = [row["tracks"][track][0] for row in kept if row["tracks"][track][0] == row["tracks"][track][0]]
        exceeded = [value for value in values if value > tolerances["tau_p"]]
        if exceeded:
            static_tracks.append(track)
            entry = bones.get(track, {"name": "?"})
            spread = max(exceeded) - min(exceeded)
            print(f"  t{track:<3} {entry['name']:<34} exceeded={len(exceeded)}/{len(values)} "
                  f"min={fmt(min(exceeded))} max={fmt(max(exceeded))} spread={fmt(spread)} "
                  f"chain={chain(bones, track, 4)}")
    print(f"  -> {len(static_tracks)} tracks: {static_tracks}")
    print()

    # ---- 2. E_r 超差轨（rest），按受影响对数排序 ----
    print("== 2. E_r 超差轨（rest），按受影响对数降序 ==")
    rot_tracks = []
    for track in primary:
        values = [row["tracks"][track][1] for row in kept if row["tracks"][track][1] == row["tracks"][track][1]]
        exceeded = [value for value in values if value > tolerances["tau_r"]]
        if exceeded:
            rot_tracks.append((track, len(exceeded), min(exceeded), max(exceeded)))
    rot_tracks.sort(key=lambda item: (-item[1], item[0]))
    for track, count, low, high in rot_tracks:
        entry = bones.get(track, {"name": "?"})
        print(f"  t{track:<3} {entry['name']:<34} pairs={count}/{len(kept)} "
              f"min={fmt(low)} max={fmt(high)} chain={chain(bones, track, 4)}")
    print(f"  -> {len(rot_tracks)} tracks")
    print()

    # ---- 3. 尖峰普查 ----
    print("== 3. E_r 尖峰普查（rest）==")
    spike_pair_tracks: dict[int, list[tuple[int, float]]] = {}
    for threshold in SPIKE_THRESHOLDS:
        affected = 0
        per_track: dict[int, int] = {}
        for row in kept:
            hits = [(track, row["tracks"][track][1]) for track in primary
                    if row["tracks"][track][1] == row["tracks"][track][1]
                    and row["tracks"][track][1] > threshold]
            if hits:
                affected += 1
                if threshold == SPIKE_THRESHOLDS[0]:
                    spike_pair_tracks[row["pair_index"]] = hits
            for track, _value in hits:
                per_track[track] = per_track.get(track, 0) + 1
        ranked = sorted(per_track.items(), key=lambda item: (-item[1], item[0]))
        top = ", ".join(f"t{track}({bones.get(track, {'name': '?'})['name']}):{count}"
                        for track, count in ranked[:10] if count)
        print(f"  E_r>{threshold:g}: pairs affected={affected}/{len(kept)} tracks={len(per_track)}")
        if top:
            print(f"    top: {top}")
    print()

    max_by_track = {}
    for track in primary:
        values = [row["tracks"][track][1] for row in kept
                  if row["tracks"][track][1] == row["tracks"][track][1]]
        if values:
            max_by_track[track] = max(values)
    spike_tracks = [track for track, value in max_by_track.items() if value > 1e-2]
    spike_tracks.sort(key=lambda track: -max_by_track[track])
    print(f"  尖峰轨（max E_r > 1e-2，{len(spike_tracks)} 条）:")
    for track in spike_tracks:
        entry = bones.get(track, {"name": "?"})
        print(f"    t{track:<3} {entry['name']:<34} max={fmt(max_by_track[track])} "
              f"chain={chain(bones, track, 5)}")
    print()

    # ---- 4. 交集与归组 ----
    print("== 4. 归组 ==")
    aux = sorted(coverage["auxiliary"])
    overlap = sorted(set(spike_tracks) & set(static_tracks))
    print(f"  B 静态轨（E_p rest 超差）: {static_tracks}")
    print(f"  C 尖峰轨（max E_r>1e-2）: {spike_tracks}")
    print(f"  交集 B∩C: {overlap}")
    print(f"  辅轨（非 primary，item={aux[:6]} wing={aux[6:16]} belly={aux[16:]}，已排除在外）")
    print()
    print("  逐对尖峰示例（前 12 对，含段与 completed）:")
    for index, (pair_index, hits) in enumerate(sorted(spike_pair_tracks.items())[:12]):
        row = next(r for r in kept if r["pair_index"] == pair_index)
        seg = "equip" if row["segment"] == 0 else "unequip"
        listing = ", ".join(f"t{track}={fmt(value)}({bones.get(track, {'name': '?'})['name']})"
                            for track, value in hits)
        print(f"    pair {pair_index:<4} {seg:<7} completed={row['completed']:<3} {listing}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
