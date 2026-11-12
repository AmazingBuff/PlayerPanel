from pathlib import Path
import configparser
import hashlib
import json
import re
import subprocess


ROOT = Path(__file__).resolve().parents[3]
EVIDENCE = Path(__file__).resolve().parent
PACKAGE = ROOT / "dist/g5-observation-v2a-20261112"
BASELINE = "f39ffa8b36b691b9968064ee0a2a59a67a87578d"


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
assert not (ROOT / "src/base/util.cpp").exists()
assert not (ROOT / "src/base/util.h").exists()

allowed = {
    ".gitattributes", "README.md", "CHANGELOG.md", "docs/build.md", "docs/history.md",
    "docs/independent-behavior-graph.md",
}
changed = set(git("diff", "--name-only").splitlines())
untracked = set(git("ls-files", "--others", "--exclude-standard").splitlines())
for name in changed | untracked:
    assert name in allowed or name.startswith("docs/diagnostics/g5-observation-v2a-20261112/"), name
print("PASS worktree scope: clean baseline f39ffa8b36 on oar-idle3, empty staged state, absent protected util files, only approved docs/evidence changes")

manifest = json.loads((EVIDENCE / "manifest.json").read_text(encoding="utf-8"))
assert (EVIDENCE / "manifest.json").read_bytes() == (PACKAGE / "manifest.json").read_bytes()
for name, entry in manifest["source_files"].items():
    assert sha(ROOT / name) == entry["sha256"], name
    assert (ROOT / name).stat().st_size == entry["bytes"], name
for name, entry in manifest["package_files"].items():
    assert sha(PACKAGE / name) == entry["sha256"], name
    assert (PACKAGE / name).stat().st_size == entry["bytes"], name
    if name.endswith((".dll", ".pdb")):
        assert sha(PACKAGE / name) == sha(ROOT / "build/Release" / name)
identity = manifest["build_identity"]
assert identity.encode() in (PACKAGE / "CharacterPanel.dll").read_bytes()
assert identity in (ROOT / "build/src/plugin.h").read_text(encoding="utf-8")
assert sha(ROOT / "build/src/plugin.h") == manifest["generated_build_header_sha256"]
assert manifest["source_revision"] == BASELINE
assert manifest["verification"]["scenarios"] == 2
print("PASS package identity: clean-tree identity embedded in DLL and generated header, matching Release DLL/PDB copies, exact source/package hashes and duplicated manifest")

sample_text = git("show", "HEAD:config/CharacterPanel.ini")
base, sample, manual = [configparser.ConfigParser() for _ in range(3)]
base.read_string(sample_text)
sample.read(ROOT / "config/CharacterPanel.ini", encoding="utf-8")
manual.read(PACKAGE / "CharacterPanel.ini", encoding="utf-8")
assert base == sample
assert sha(ROOT / "config/CharacterPanel.ini") == manifest["source_files"]["config/CharacterPanel.ini"]["sha256"]
for section in sample:
    for key in sample[section]:
        expected = next((value for name, value in manifest["manual_ini_overrides"].items() if name.lower() == key), sample[section][key])
        assert manual[section][key] == expected, (section, key)
assert not (PACKAGE / "CharacterPanel.ini").read_bytes().startswith(b"\xef\xbb\xbf")
assert len((PACKAGE / "CharacterPanel.ini").read_text(encoding="utf-8").splitlines()) == len(sample_text.splitlines())
print("PASS configuration: committed sample unchanged; exactly seven manual overrides; display/source instruments/liveness off; UTF-8 without BOM and line count preserved")

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
assert anchor_exists(ROOT / "docs/build.md", "v96a-g5-observation-v2a-20261112")
for symbol in ["SegmentObservation", "ClipClockHistory", "clock_delta"]:
    assert symbol in (ROOT / "src/animation/behavior_graph/observation_policy.h").read_text(encoding="utf-8")
assert "run_independent_graph_experiment" in (ROOT / "src/animation/behavior_graph/independent_graph.cpp").read_text(encoding="utf-8")
print(f"PASS documentation: {link_count} local links and anchors resolve; canonical/build sections and code/test paths current")

assert "PENDING" in (EVIDENCE / "result.md").read_text(encoding="utf-8")
assert "text: unset" in git("check-attr", "text", "--", "docs/diagnostics/g5-observation-v2a-20261112/manifest.json")
assert subprocess.run(["git", "-C", str(ROOT), "diff", "--check"], check=False).returncode == 0
print("PASS boundary: native validation still PENDING, no deployment claimed, git diff --check exit 0")
