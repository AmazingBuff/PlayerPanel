"""Match each buffer slot against EVERY bone of the same skin, with the transform scale factored in.

The S2P4 dump gives, per slot i: the bone it claims (bones[i]) and that bone's world transform, plus
the buffer's raw contents. If the buffer is not indexed like bones[], most slots will match some
*other* bone's world (x scale) far better than their own — and the resulting permutation is the
answer.
"""
import re
import sys

LINE = re.compile(
    r"SCOPY TSLOT i=(\d+) bone='([^']*)' world=\(([^)]*)\) matrix=\(([^)]*)\) previous=\(([^)]*)\)")
ROT = [0, 1, 2, 4, 5, 6, 8, 9, 10]


def numbers(text):
    return [float(v) for v in text.replace("|", " ").split()]


def dist_rot(a, b, scale):
    return sum(abs(a[i] - scale * b[i]) for i in ROT)


def best_scale(a, b):
    num = sum(a[i] * b[i] for i in ROT)
    den = sum(b[i] * b[i] for i in ROT)
    return num / den if den else 0.0


def main(path):
    slots = []
    for text in open(path, encoding="utf-8", errors="replace"):
        m = LINE.search(text)
        if m:
            slots.append({"i": int(m.group(1)), "bone": m.group(2),
                          "world": numbers(m.group(3)), "matrix": numbers(m.group(4))})
    split = 31
    for title, group in (("clothes", slots[:split]), ("Dress", slots[split:])):
        if not group:
            continue
        print(f"=== {title}: {len(group)} slots ===")
        own_ok = 0
        mapping = []
        for s in group:
            best = None
            for t in group:
                k = best_scale(s["matrix"], t["world"])
                d = dist_rot(s["matrix"], t["world"], k)
                if best is None or d < best[0]:
                    best = (d, t["i"], t["bone"], k)
            if best[1] == s["i"]:
                own_ok += 1
            mapping.append((s["i"], best[1], best[0], best[3], s["bone"], best[2]))
        print(f"  slots whose best match is their own bones[i]: {own_ok}/{len(group)}")
        print("  slot -> best-matching slot (distance, fitted scale, claimed bone -> matched bone):")
        for i, j, d, k, own, other in mapping[:12]:
            flag = "" if i == j else "   <-- differs"
            print(f"    i={i:>3} -> {j:>3} d={d:.4f} k={k:.4f} | '{own[:26]}' -> '{other[:26]}'{flag}")
        shifted = [m for m in mapping if m[1] == m[0] + 1]
        print(f"  slots matching slot i+1 (off-by-one shift): {len(shifted)}/{len(group)}")
        print()


if __name__ == "__main__":
    main(sys.argv[1])
