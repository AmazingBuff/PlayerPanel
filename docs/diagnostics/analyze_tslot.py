"""Offline analysis of the S2P4 raw slot dump (SCOPY TSLOT lines).

Each line carries, for one slot of the target skin:
  world=(12 floats)   the bone's own world transform, rows 0-2 with the translation in column 3
  matrix=(12 floats)  the skin's matrix buffer content for that slot
  previous=(12 floats) the second buffer's content for that slot

The question this answers: what does the buffer hold, and is slot i the bone bones[i]?
"""
import re
import sys

LINE = re.compile(
    r"SCOPY TSLOT i=(\d+) bone='([^']*)' world=\(([^)]*)\) matrix=\(([^)]*)\) previous=\(([^)]*)\)")


def numbers(text):
    if text.strip() == "unavailable":
        return None
    return [float(v) for v in text.replace("|", " ").split()]


def rows_of(values):
    """3x4 rows: (r0, r1, r2, t) from the flat 12."""
    return [values[0:3], values[4:7], values[8:11]], [values[3], values[7], values[11]]


def as_matrix(values):
    r, t = rows_of(values)
    return r, t


def transpose(m):
    return [[m[j][i] for j in range(3)] for i in range(3)]


def l1(a, b):
    return sum(abs(x - y) for ra, rb in zip(a, b) for x, y in zip(ra, rb))


def l1_translation(a, b):
    return sum(abs(x - y) for x, y in zip(a, b))


def parse(path):
    slots = []
    for text in open(path, encoding="utf-8", errors="replace"):
        m = LINE.search(text)
        if m:
            slots.append({
                "i": int(m.group(1)),
                "bone": m.group(2),
                "world": numbers(m.group(3)),
                "matrix": numbers(m.group(4)),
                "previous": numbers(m.group(5)),
            })
    return slots


def report(title, slots):
    print(f"=== {title}: {len(slots)} slots ===")
    hits = {"raw-world": 0, "raw-world-transposed": 0, "col-major-t4": 0, "col-major-t3": 0}
    best_examples = []
    for s in slots:
        if s["matrix"] is None or s["world"] is None:
            continue
        wr, wt = as_matrix(s["world"])
        mr, mt = as_matrix(s["matrix"])
        d_raw = l1(mr, wr) + l1_translation(mt, wt)
        d_t = l1(mr, transpose(wr)) + l1_translation(mt, wt)
        # Column-major 4x3 reading: the 12 numbers are four columns of three, so the rotation is
        # [0,4,8],[1,5,9],[2,6,10] and the translation is the last column [3,7,11] (or first).
        cm = [[s["matrix"][0], s["matrix"][4], s["matrix"][8]],
              [s["matrix"][1], s["matrix"][5], s["matrix"][9]],
              [s["matrix"][2], s["matrix"][6], s["matrix"][10]]]
        cmt_t4 = [s["matrix"][3], s["matrix"][7], s["matrix"][11]]
        cmt_t1 = [s["matrix"][0], s["matrix"][4], s["matrix"][8]]
        d_cm4 = l1(cm, wr) + l1_translation(cmt_t4, wt)
        d_cm1 = l1(cm, wr) + l1_translation(cmt_t1, wt)
        best = min((d_raw, "raw-world"), (d_t, "raw-world-transposed"),
                   (d_cm4, "col-major-t4"), (d_cm1, "col-major-t3"))
        hits[best[1]] += 1
        best_examples.append((best[0], best[1], s["i"], s["bone"], d_raw, d_t, d_cm4, d_cm1))
    best_examples.sort()
    print("per-slot best reading:", hits)
    print("closest 3 slots by total distance:")
    for e in best_examples[:3]:
        print(f"  i={e[2]:>3} bone='{e[3][:38]}' best={e[1]} d={e[0]:.4f} | raw={e[4]:.4f} rawT={e[5]:.4f} cm4={e[6]:.4f} cm1={e[7]:.4f}")

    # Is the buffer maybe just the world matrix of SOME OTHER bone (a palette remap)?
    world_by_index = {}
    for s in slots:
        if s["world"] is not None:
            world_by_index[s["i"]] = as_matrix(s["world"])
    matches = []
    for s in slots:
        if s["matrix"] is None:
            continue
        mr, mt = as_matrix(s["matrix"])
        best = None
        for j, (wr, wt) in world_by_index.items():
            d = l1(mr, wr) + l1_translation(mt, wt)
            if best is None or d < best[0]:
                best = (d, j)
        matches.append((best[0], s["i"], best[1], s["bone"]))
    matches.sort()
    identity_hits = sum(1 for d, i, j, _ in matches if i == j and d < 0.01)
    print(f"slots whose own world matches its own buffer exactly: {identity_hits}/{len(matches)}")
    print("closest 5 (distance, its slot, the slot whose world matches best):")
    for d, i, j, bone in matches[:5]:
        print(f"  i={i:>3} best-world-slot={j:>3} d={d:.4f} bone='{bone[:38]}'")

    # Does the buffer change at all between frames? The dump is single-shot per arm, so this only
    # compares the two buffers of the same frame.
    same = 0
    compared = 0
    for s in slots:
        if s["matrix"] is None or s["previous"] is None:
            continue
        compared += 1
        if l1(as_matrix(s["matrix"])[0], as_matrix(s["previous"])[0]) < 1e-6:
            same += 1
    print(f"current vs previous buffer identical (rotation part): {same}/{compared}")
    print()


if __name__ == "__main__":
    slots = parse(sys.argv[1])
    first = [s for s in slots if s["i"] < 32]
    second = [s for s in slots if s["i"] >= 32 or len(slots) > 40 and s is not slots[0] and s["bone"].lower().find("dress") >= 0]
    # The log holds two dumps back to back: 31 slots (clothes) then 71 (Dress).
    split = 31 if len(slots) >= 31 else len(slots)
    report("capture 1 (clothes, 31 slots)", slots[:split])
    if len(slots) > split:
        report("capture 2 (Dress, 71 slots)", slots[split:])
