from pathlib import Path
import configparser
import hashlib
import json
import re
import subprocess


ROOT = Path(__file__).resolve().parents[3]
EVIDENCE = Path(__file__).resolve().parent
PACKAGE = ROOT / "dist/g5-observation-v2-20261112"
BASELINE = "dfd37acc166f2ca8dbc40ff1ac94fb54ad095626"


def git(*args):
    return subprocess.check_output(["git", "-C", str(ROOT), *args], encoding="utf-8").strip()


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def anchor_exists(path, anchor):
    text = path.read_text(encoding="utf-8")
    if re.fullmatch(r"L\d+", anchor):
        return int(anchor[1:]) <= len(text.splitlines())
    if f'id="{anchor}"' in text:
        return True
    for line in text.splitlines():
        if line.startswith("#"):
            slug = re.sub(r"[^\w\- ]", "", line.lstrip("# ").lower()).replace(" ", "-")
            if slug == anchor:
                return True
    return False


assert git("rev-parse", "HEAD") == BASELINE
assert git("branch", "--show-current") == "oar-idle3"
assert not git("diff", "--cached", "--name-status")
protected = {
    "src/base/util.cpp": "4f15cd5d05b32365c59d35bfbd8aec31548079b9693333c1b3d6cc6de5dd6b18",
    "src/base/util.h": "0ba950ff1cee776c62aec3808d7a99764fb478a33ab68ce587dad85721338dc7",
    "src/animation/behavior_graph/frame_trace_policy.h": "c5d849841aea4788871ac11938a289c548df52bc98acebaa54a858cb8b20273d",
}
for name, expected in protected.items():
    assert sha(ROOT / name) == expected, name

allowed = {
    ".gitattributes", "CHANGELOG.md", "README.md", "config/CharacterPanel.ini",
    "docs/build.md", "docs/history.md", "docs/independent-behavior-graph.md",
    "src/animation/behavior_graph/independent_graph.cpp",
    "src/animation/behavior_graph/observation_policy.h",
    "tests/behavior_graph_observation_test.cpp",
}
changed = set(git("diff", "--name-only").splitlines())
untracked = set(git("ls-files", "--others", "--exclude-standard").splitlines())
for name in changed | untracked:
    assert name in allowed or name in protected or name.startswith("docs/diagnostics/g5-observation-v2-20261112/"), name
assert not changed.intersection(protected)
print("PASS worktree scope: baseline HEAD/branch, empty staged state, protected util/frame-policy hashes, historical artifacts unchanged")

manifest = json.loads((EVIDENCE / "manifest.json").read_text(encoding="utf-8"))
assert (EVIDENCE / "manifest.json").read_bytes() == (PACKAGE / "manifest.json").read_bytes()
for name, entry in manifest["source_files"].items():
    assert sha(ROOT / name) == entry["sha256"], name
for name, entry in manifest["package_files"].items():
    assert sha(PACKAGE / name) == entry["sha256"], name
    assert (PACKAGE / name).stat().st_size == entry["bytes"], name
    if name.endswith((".dll", ".pdb")):
        assert sha(PACKAGE / name) == sha(ROOT / "build/Release" / name)
assert manifest["build_identity"].encode() in (PACKAGE / "CharacterPanel.dll").read_bytes()
assert manifest["source_revision"] == BASELINE
assert manifest["verification"]["scenarios"] == 2
print("PASS package identity: matching Release DLL/PDB copies, embedded v96 identity, exact source/package hashes and duplicated manifest")

base, sample, manual = [configparser.ConfigParser() for _ in range(3)]
base.read_string(git("show", "HEAD:config/CharacterPanel.ini"))
sample.read(ROOT / "config/CharacterPanel.ini", encoding="utf-8")
manual.read(PACKAGE / "CharacterPanel.ini", encoding="utf-8")
assert base == sample
for section in sample:
    for key in sample[section]:
        expected = next((value for name, value in manifest["manual_ini_overrides"].items() if name.lower() == key), sample[section][key])
        assert manual[section][key] == expected, (section, key)
assert not (PACKAGE / "CharacterPanel.ini").read_bytes().startswith(b"\xef\xbb\xbf")
print("PASS configuration: sample values unchanged; exactly seven manual overrides; display/source instruments/liveness off; UTF-8 without BOM")

docs = [ROOT / name for name in ["README.md", "docs/build.md", "docs/history.md", "docs/independent-behavior-graph.md", "CHANGELOG.md"]]
link_count = 0
for path in docs + list(EVIDENCE.glob("*.md")) + [PACKAGE / "MANUAL.md"]:
    if path in docs:
        diff = git("diff", "--unified=0", "--", path.relative_to(ROOT).as_posix())
        text = "\n".join(line[1:] for line in diff.splitlines() if line.startswith("+") and not line.startswith("+++"))
    else:
        text = path.read_text(encoding="utf-8")
    for target in re.findall(r"\[[^\]]+\]\(([^)]+)\)", text):
        if "://" in target:
            continue
        file, _, anchor = target.partition("#")
        destination = (path.parent / file).resolve() if file else path
        assert destination.is_file(), (path, target)
        assert not anchor or anchor_exists(destination, anchor), (path, target)
        link_count += 1
canonical = (ROOT / "docs/independent-behavior-graph.md").read_text(encoding="utf-8")
assert anchor_exists(ROOT / "docs/independent-behavior-graph.md", "c-g5-observation-v2-20261112")
assert "D1 诊断未运行" not in canonical
for symbol in ["SegmentObservation", "ClipClockHistory", "clock_delta"]:
    assert symbol in (ROOT / "src/animation/behavior_graph/observation_policy.h").read_text(encoding="utf-8")
assert "run_independent_graph_experiment" in (ROOT / "src/animation/behavior_graph/independent_graph.cpp").read_text(encoding="utf-8")
print(f"PASS documentation: {link_count} added/current local links and anchors; canonical section, code/test paths and symbols current; stale D1-not-run corrected")

for name in ["src/animation/behavior_graph/observation_policy.h", "tests/behavior_graph_observation_test.cpp", "docs/independent-behavior-graph.md"]:
    assert b"\r" not in (ROOT / name).read_bytes(), name
native = (ROOT / "src/animation/behavior_graph/independent_graph.cpp").read_bytes()
assert native.count(b"\n") == native.count(b"\r\n")
assert "eol: lf" in git("check-attr", "eol", "--", "src/animation/behavior_graph/observation_policy.h")
assert "text: unset" in git("check-attr", "text", "--", "docs/diagnostics/g5-observation-v2-20261112/manifest.json")
assert subprocess.run(["git", "-C", str(ROOT), "diff", "--check"], check=False).returncode == 0
print("PASS formatting: policy/test/canonical LF, integration CRLF, evidence exact-byte attributes, git diff --check exit 0")
print("Native validation remains PENDING; no extra behavioral scenario or historical suite run.")
