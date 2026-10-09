"""Test the hypothesis the S2P4 dump made visible: the skin matrix buffer holds the bone's world
rotation with the transform SCALE baked into the 3x3 (which is what a skinning matrix needs), so a
comparison against the unit-norm NiTransform rotation is off by that scale everywhere.

For each slot, fit k = <M, W> / <W, W> over the nine rotation components and report the residual.
"""
import re
import sys

LINE = re.compile(
    r"SCOPY TSLOT i=(\d+) bone='([^']*)' world=\(([^)]*)\) matrix=\(([^)]*)\) previous=\(([^)]*)\)")
ROT = [0, 1, 2, 4, 5, 6, 8, 9, 10]
TRA = [3, 7, 11]


def numbers(text):
    if text.strip() == "unavailable":
        return None
    return [float(v) for v in text.replace("|", " ").split()]


def fit_scale(matrix, world):
    num = sum(matrix[i] * world[i] for i in ROT)
    den = sum(world[i] * world[i] for i in ROT)
    if den == 0.0:
        return 0.0, 0.0
    k = num / den
    residual = sum(abs(matrix[i] - k * world[i]) for i in ROT)
    return k, residual


def main(path):
    slots = []
    for text in open(path, encoding="utf-8", errors="replace"):
        m = LINE.search(text)
        if m:
            slots.append({"i": int(m.group(1)), "bone": m.group(2),
                          "world": numbers(m.group(3)), "matrix": numbers(m.group(4)),
                          "previous": numbers(m.group(5))})
    split = 31
    for title, group in (("clothes (31 slots)", slots[:split]), ("Dress (71 slots)", slots[split:])):
        if not group:
            continue
        print(f"=== {title} ===")
        scales = []
        worst = []
        for s in group:
            if not s["matrix"] or not s["world"]:
                continue
            k, residual = fit_scale(s["matrix"], s["world"])
            scales.append(k)
            worst.append((residual, s["i"], s["bone"], k))
        worst.sort(reverse=True)
        print(f"  fitted scale: min={min(scales):.5f} max={max(scales):.5f} mean={sum(scales)/len(scales):.5f}")
        print(f"  worst residuals: " + ", ".join(f"i={i}(r={r:.4f},k={k:.4f})" for r, i, _, k in worst[:3]))

        # Translation: is the difference a constant offset shared by the whole skin?
        diffs = []
        for s in group:
            if not s["matrix"] or not s["world"]:
                continue
            diffs.append(tuple(s["matrix"][t] - s["world"][t] for t in TRA))
        if diffs:
            mean = tuple(sum(d[a] for d in diffs) / len(diffs) for a in range(3))
            spread = max(max(abs(d[a] - mean[a]) for a in range(3)) for d in diffs)
            print(f"  matrix_t - world_t: mean=({mean[0]:.2f}, {mean[1]:.2f}, {mean[2]:.2f}) max deviation from that mean={spread:.2f}")

        # Does the buffer's rotation follow the world rotation across slots at that scale?
        match = sum(1 for s in group if s["matrix"] and s["world"] and fit_scale(s["matrix"], s["world"])[1] < 0.01)
        print(f"  slots where matrix == 0.35 * world within 0.01: {match}/{len(group)}")

        # And the second buffer, which the classifier could not separate from the first.
        same = sum(1 for s in group if s["matrix"] and s["previous"] and
                   sum(abs(s["matrix"][i] - s["previous"][i]) for i in ROT) < 1e-6)
        print(f"  current == previous (rotation): {same}/{len(group)}")
        print()


if __name__ == "__main__":
    main(sys.argv[1])
