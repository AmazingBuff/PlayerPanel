"""C-G5-TRANS §八.2：旋转底噪来源三方对照（2026-11-21，纯离线）。

假设（预注册于本脚本，对应过渡窗文档 §八.2 的三个候选来源）：
  H-REF（参考姿态/绑定基差异）：一侧 pose 逐位等于骨架 referencePose 而另一侧不等，
      或两侧偏离参考的差恒定 → 底噪是数据/基座差异；
  H-FP（求值路径/累积顺序）：底噪只出现在被动画求值的骨上、幅度随运动量变化，
      保持参考值的骨上两侧逐位相等 → 底噪是浮点求值差异；
  H-MIX：两者并存（按轨区分）。

输入（全部为仓库内归档，只读）：
  docs/diagnostics/g5-gate5-20261116/raw/{candidate,source}/…bin   v103 正式 CPK5/CPK5S
  docs/diagnostics/g5-gate5-20261116/formal/gate4-pair-manifest.json
  docs/diagnostics/g5-g4-cov-20261115/coverage.json
  docs/diagnostics/CharacterPanel-g5k1-census-v101-run1-20261114-1411.log
      （K1 CENSUS phase=bone … bits=… = skeleton->referencePose 的 t3+q4+s3 原始位型）
  本目录 bone-map.json

输出（stdout）：
  1. 逐轨三方角差（θ_cs 候选↔源、θ_cr 候选↔参考、θ_sr 源↔参考）在静止子集的统计；
  2. 逐位相等计数（cs/cr/sr 全 10 浮点、仅旋转 4 浮点）；
  3. H-REF vs H-FP 判别表；
  4. 底噪与运动量的相关性（每轨 θ_cs 对源侧跨对运动）。

用法：
  python -B docs/diagnostics/g5-bone-name-mapping-20261121/noise_floor_source.py
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
COVERAGE = ROOT / "docs/diagnostics/g5-g4-cov-20261115/coverage.json"
K1_LOG = ROOT / "docs/diagnostics/CharacterPanel-g5k1-census-v101-run1-20261114-1411.log"
BONE_MAP = Path(__file__).resolve().parent / "bone-map.json"

TRACK_BYTES = 40
IDLE_DOMAIN = "1hm_idle"
UNEQUIP_DOMAIN = "1hm_unequip"


def load_reference_pose() -> tuple[dict[int, tuple], dict[int, tuple]]:
    """K1 CENSUS phase=bone bits= → per-side per-bone (t3, q4, s3) raw float values.

    日志含 side=source 与 side=candidate 两块（各 116 骨）：两块分别是控制器骨架与
    候选骨架的 referencePose；两块逐位对照本身就是 H-REF 检验。"""
    import re

    pattern = re.compile(
        r"K1 CENSUS actor=\w+ request=\d+ side=(\w+) phase=bone index=(\d+) name='[^']*' parent=(-?\d+) "
        r"bits=([0-9A-Fa-f]{8}(?:,[0-9A-Fa-f]{8}){9})")
    reference: dict[str, dict[int, tuple]] = {}
    for match in pattern.finditer(K1_LOG.read_text(encoding="utf-8", errors="replace")):
        side = match.group(1)
        index = int(match.group(2))
        words = [struct.unpack("<f", struct.pack("<I", int(word, 16)))[0]
                 for word in match.group(4).split(",")]
        block = reference.setdefault(side, {})
        if index in block:
            raise SystemExit(f"duplicate {side} reference rows for bone {index}")
        block[index] = tuple(words)
    return reference.get("source", {}), reference.get("candidate", {})


def pose_at(parsed: dict, record: dict, track: int) -> tuple:
    base = (parsed["header_size"] + record["index"] * parsed["record_size"]
            + parsed["meta_size"] + g.CLIP_CAPACITY * g.K5_CLIP.size)
    return struct.unpack_from("<10f", parsed["blob"], base + track * TRACK_BYTES)


def quat_angle(qa, qb) -> float | None:
    na = math.sqrt(sum(v * v for v in qa))
    nb = math.sqrt(sum(v * v for v in qb))
    if na <= 0.0 or nb <= 0.0:
        return None
    dot = sum(qa[i] / na * qb[i] / nb for i in range(4))
    c = min(1.0, max(0.0, abs(dot)))
    return 2.0 * math.acos(c)


def relative_quat(qa, qb):
    """qa ⊗ qb⁻¹ (unit-normalized inputs) → (x, y, z, w)."""
    na = math.sqrt(sum(v * v for v in qa))
    nb = math.sqrt(sum(v * v for v in qb))
    a = [v / na for v in qa]
    b = [v / nb for v in qb]
    bx, by, bz, bw = -b[0], -b[1], -b[2], b[3]
    return (
        a[3] * bx + a[0] * bw + a[1] * bz - a[2] * by,
        a[3] * by - a[0] * bz + a[1] * bw + a[2] * bx,
        a[3] * bz + a[0] * by - a[1] * bx + a[2] * bw,
        a[3] * bw - a[0] * bx - a[1] * by - a[2] * bz,
    )


def median(values):
    ordered = sorted(values)
    return ordered[len(ordered) // 2] if ordered else 0.0


def percentile(values, fraction):
    if not values:
        return 0.0
    ordered = sorted(values)
    index = min(len(ordered) - 1, max(0, int(round(fraction * (len(ordered) - 1)))))
    return ordered[index]


def fmt_deg(theta) -> str:
    return "n/a" if theta is None else f"{math.degrees(theta):.3f}"


def main() -> int:
    bones = {int(k): v["name"] for k, v in
             json.loads(BONE_MAP.read_text(encoding="utf-8")).items()}
    ref_src, ref_cand = load_reference_pose()
    if len(ref_src) != 116 or len(ref_cand) != 116:
        raise SystemExit(f"reference pose rows src={len(ref_src)} cand={len(ref_cand)} != 116")
    coverage = g.load_coverage(str(COVERAGE))
    primary = sorted(coverage["primary_set"])
    cand = g.parse_candidate(str(CAND))
    src = g.parse_source(str(SRC))
    src_by_seq = {rec["seq"]: rec for rec in src["records"]}
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    pairs = manifest["pairs"]
    print(f"pairs={len(pairs)} cand_records={cand['count']} src_records={src['count']} "
          f"reference_rows src={len(ref_src)} cand={len(ref_cand)}")

    # ---- 0. H-REF 直接检验：两骨架 referencePose 逐位对照 ----
    print()
    print("== 0. 两骨架 referencePose 逐位对照（H-REF 直接证据）==")
    bit_diff = []
    for track in primary:
        if ref_src[track] != ref_cand[track]:
            bit_diff.append(track)
    print(f"   主轨中两侧参考位姿逐位不同的: {len(bit_diff)}/{len(primary)}"
          + (f" -> {bit_diff}" if bit_diff else ""))
    theta_ref_diff = [(track, quat_angle(ref_src[track][3:7], ref_cand[track][3:7]))
                      for track in primary]
    theta_ref_diff = [(t, th) for t, th in theta_ref_diff if th is not None]
    max_pair = max(theta_ref_diff, key=lambda item: item[1]) if theta_ref_diff else None
    if max_pair:
        print(f"   参考位姿旋转差最大: t{max_pair[0]} ({bones.get(max_pair[0], '?')}) "
              f"= {fmt_deg(max_pair[1])}")
    print()

    reference = ref_src  # 三方对照的参考 = 控制器侧骨架（源侧运行时基座）

    rows = []
    for pair in pairs:
        cand_rec = cand["records"][pair["cand"]["record"]]
        src_rec = src_by_seq[pair["src"]["seq"]]
        rows.append({
            "domain": pair["domain"],
            "segment": cand_rec["segment"],
            "completed": cand_rec["completed"],
            "cand_rec": cand_rec,
            "src_rec": src_rec,
        })

    idle_set = {i for i, row in enumerate(rows) if row["domain"] == IDLE_DOMAIN}
    quiet_set = idle_set  # 静止子集 = idle domain（已 settle，无过渡/衰减尾）
    print(f"quiet subset: domain={IDLE_DOMAIN} pairs={len(quiet_set)} (indices {min(quiet_set)}..{max(quiet_set)})")
    print()

    # ---- 1. 逐轨三方角差与逐位相等（quiet 子集） ----
    print("== 1. quiet 子集逐轨三方对照（theta 单位为度）==")
    print("   t name                          n  theta_cs med/max  theta_cr med/max  theta_sr med/max"
          "  bits_eq(cs all/rot) (cr) (sr)")
    for track in primary:
        cs_vals, cr_vals, sr_vals = [], [], []
        eq_all_cs = eq_rot_cs = eq_all_cr = eq_rot_cr = eq_all_sr = eq_rot_sr = 0
        ref = reference[track]
        for i in quiet_set:
            row = rows[i]
            cp = pose_at(cand, row["cand_rec"], track)
            sp = pose_at(src, row["src_rec"], track)
            cs_vals.append(quat_angle(cp[3:7], sp[3:7]))
            cr_vals.append(quat_angle(cp[3:7], ref[3:7]))
            sr_vals.append(quat_angle(sp[3:7], ref[3:7]))
            if cp == sp:
                eq_all_cs += 1
            if cp[3:7] == sp[3:7]:
                eq_rot_cs += 1
            if cp == ref:
                eq_all_cr += 1
            if cp[3:7] == ref[3:7]:
                eq_rot_cr += 1
            if sp == ref:
                eq_all_sr += 1
            if sp[3:7] == ref[3:7]:
                eq_rot_sr += 1
        cs_vals = [v for v in cs_vals if v is not None]
        cr_vals = [v for v in cr_vals if v is not None]
        sr_vals = [v for v in sr_vals if v is not None]
        print(f"   t{track:<3} {bones.get(track, '?'):<30} {len(quiet_set)}  "
              f"{fmt_deg(median(cs_vals))}/{fmt_deg(max(cs_vals))}  "
              f"{fmt_deg(median(cr_vals))}/{fmt_deg(max(cr_vals))}  "
              f"{fmt_deg(median(sr_vals))}/{fmt_deg(max(sr_vals))}  "
              f"{eq_all_cs}/{eq_rot_cs} {eq_all_cr}/{eq_rot_cr} {eq_all_sr}/{eq_rot_sr}")
    print()

    # ---- 2. H 判别摘要 ----
    print("== 2. H 判别摘要（quiet 子集，99 主轨汇总）==")
    n_tracks = len(primary)
    src_eq_ref_rot = 0
    cand_eq_ref_rot = 0
    both_ne = 0
    cs_exact_rot = 0
    for track in primary:
        ref = reference[track]
        any_rot_eq_cr = any(pose_at(cand, rows[i]["cand_rec"], track)[3:7] == ref[3:7] for i in quiet_set)
        any_rot_eq_sr = any(pose_at(src, rows[i]["src_rec"], track)[3:7] == ref[3:7] for i in quiet_set)
        any_rot_eq_cs = any(pose_at(cand, rows[i]["cand_rec"], track)[3:7]
                            == pose_at(src, rows[i]["src_rec"], track)[3:7] for i in quiet_set)
        src_eq_ref_rot += any_rot_eq_sr
        cand_eq_ref_rot += any_rot_eq_cr
        cs_exact_rot += any_rot_eq_cs
        if not any_rot_eq_cr and not any_rot_eq_sr:
            both_ne += 1
    print(f"   src 逐位==参考的轨（旋转任一对）: {src_eq_ref_rot}/{n_tracks}")
    print(f"   cand 逐位==参考的轨（旋转任一对）: {cand_eq_ref_rot}/{n_tracks}")
    print(f"   两侧均不==参考的轨: {both_ne}/{n_tracks}")
    print(f"   cand 逐位==src 的轨（旋转任一对）: {cs_exact_rot}/{n_tracks}")
    print()

    # ---- 3. 底噪与运动量相关性（全 248 对） ----
    print("== 3. 每轨 theta_cs 与源侧跨对运动量的相关（全 248 对，Pearson）==")
    corr_rows = []
    for track in primary:
        xs, ys = [], []
        prev_sp = None
        for row in rows:
            sp = pose_at(src, row["src_rec"], track)
            cp = pose_at(cand, row["cand_rec"], track)
            theta_cs = quat_angle(cp[3:7], sp[3:7])
            if prev_sp is not None and theta_cs is not None:
                motion = quat_angle(sp[3:7], prev_sp)
                if motion is not None:
                    xs.append(motion)
                    ys.append(theta_cs)
            prev_sp = sp
        if len(xs) < 10:
            continue
        mx = sum(xs) / len(xs)
        my = sum(ys) / len(ys)
        cov = sum((x - mx) * (y - my) for x, y in zip(xs, ys))
        vx = math.sqrt(sum((x - mx) ** 2 for x in xs))
        vy = math.sqrt(sum((y - my) ** 2 for y in ys))
        r = cov / (vx * vy) if vx > 0 and vy > 0 else 0.0
        corr_rows.append((track, r, median(ys), max(ys)))
    corr_rows.sort(key=lambda item: -item[1])
    print("   高相关 top10（随运动增大 → 求值差异形状）:")
    for track, r, med, top in corr_rows[:10]:
        print(f"     t{track:<3} {bones.get(track, '?'):<30} r={r:+.3f}  theta_cs med={fmt_deg(med)} max={fmt_deg(top)}")
    print("   低相关 bottom10（静止仍有底噪 → 基座/数据差异形状）:")
    for track, r, med, top in corr_rows[-10:]:
        print(f"     t{track:<3} {bones.get(track, '?'):<30} r={r:+.3f}  theta_cs med={fmt_deg(med)} max={fmt_deg(top)}")
    print()

    # ---- 4. 相对旋转轴稳定性（quiet 子集，θ_cs 最大的几条轨） ----
    print("== 4. quiet 子集相对旋转 Δq=q_c⊗q_s⁻¹ 轴稳定性（按 theta_cs 中位降序 top8）==")
    stats = []
    for track in primary:
        vals = []
        axes = []
        for i in quiet_set:
            row = rows[i]
            cp = pose_at(cand, row["cand_rec"], track)
            sp = pose_at(src, row["src_rec"], track)
            theta = quat_angle(cp[3:7], sp[3:7])
            if theta is None:
                continue
            vals.append(theta)
            if theta > 1e-9:
                dq = relative_quat(cp[3:7], sp[3:7])
                n = math.sqrt(sum(v * v for v in dq[:3]))
                if n > 0:
                    axes.append([v / n for v in dq[:3]])
        if vals:
            stats.append((track, median(vals), max(vals), axes))
    stats.sort(key=lambda item: -item[1])
    for track, med, top, axes in stats[:8]:
        if len(axes) >= 2:
            dots = [sum(a * b for a, b in zip(axes[i], axes[j]))
                    for i in range(len(axes)) for j in range(i + 1, len(axes))]
            min_dot = min(dots)
        else:
            min_dot = 1.0
        print(f"     t{track:<3} {bones.get(track, '?'):<30} theta_cs med={fmt_deg(med)} max={fmt_deg(top)} "
              f"axis-pairs={len(axes)} min_axis_dot={min_dot:+.4f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
