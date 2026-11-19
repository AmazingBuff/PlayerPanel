"""Regenerate the bounded HKX/runtime excerpts used by the v6 StateInfo audit.

Read-only inputs:
  build/hkx-audit/weapequip_v3_full.txt
  build/hkx-audit/0_master_v3.txt
  docs/diagnostics/g5-transition-probe-v5-20261119/run1-candidate/CharacterPanel.log

Outputs live beside this script. The full dumps stay in build/hkx-audit (gitignored);
only the bounded excerpts and their source hashes are committed.
"""
from __future__ import annotations

import hashlib
import pathlib
import re

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[3]
HKX = ROOT / "build" / "hkx-audit"
RUN = HERE.parent / "run1-candidate" / "CharacterPanel.log"


def sha256(path: pathlib.Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest().upper()


def write(name: str, header: list[str], lines: list[str]) -> None:
    path = HERE / name
    body = "\n".join(header + lines) + "\n"
    path.write_text(body, encoding="utf-8", newline="\n")
    print(f"{name}: {len(lines)} lines, {len(body.encode('utf-8'))} bytes, {sha256(path)}")


def extract_weap_equip() -> None:
    src = HKX / "weapequip_v3_full.txt"
    lines = src.read_text(encoding="utf-8", errors="replace").splitlines()
    selected = [line for line in lines if "root/WeapEquip" in line]
    write("weapequip_weap_equip_subtree.txt", [
        "# Source: build/hkx-audit/weapequip_v3_full.txt",
        f"# Source SHA-256: {sha256(src)}",
        "# Filter: lines containing 'root/WeapEquip'",
        "# The source HKX is meshes/actors/character/Behaviors/weapequip.hkx",
        "",
    ], selected)


def extract_default_state() -> None:
    src = HKX / "0_master_v3.txt"
    lines = src.read_text(encoding="utf-8", errors="replace").splitlines()
    start = next(i for i, line in enumerate(lines)
        if line.startswith("STATE root/Root/mg/msg/msg/msg/Default_State "))
    end = len(lines)
    for i in range(start + 1, len(lines)):
        if lines[i].startswith("STATE root/") and "Default_State" not in lines[i]:
            end = i
            break
    write("0_master_default_state_subtree.txt", [
        "# Source: build/hkx-audit/0_master_v3.txt",
        f"# Source SHA-256: {sha256(src)}",
        f"# Block: lines {start + 1}..{end} (Default_State subtree)",
        "# The source HKX is meshes/actors/character/Behaviors/0_Master.hkx",
        "",
    ], lines[start:end])


def extract_runtime() -> None:
    if not RUN.exists():
        print(f"missing runtime log: {RUN}")
        return
    lines = RUN.read_text(encoding="utf-8", errors="replace").splitlines()
    def commanded_is(line: str, value: str) -> bool:
        return re.search(rf"\bcommanded={value}(?:\s|$)", line) is not None

    selected: list[str] = []
    for line in lines:
        if "TRANSITION SUBGRAPH side=" in line and "bfr-name='WeapEquipBFR'" in line:
            if commanded_is(line, "1") or commanded_is(line, "13"):
                selected.append(line)
        elif "TRANSITION CENSUS" in line and commanded_is(line, "1") and (
                "name='WeapEquip.hkb'" in line or "name='WeapEquipRoot'" in line or
                "name='WeapEquipBFR'" in line):
            selected.append(line)
        elif "TRANSITION PROBE SM" in line and "segment=equip" in line and \
                "phase=pre-sample" in line and ("seq=0 " in line or "seq=12 " in line) and (
                    "name='WeapEquipRoot'" in line or "name='Default_Behavior'" in line):
            selected.append(line)
        elif "TRANSITION PROBE side=candidate" in line and "segment=equip" in line and \
                commanded_is(line, "1"):
            selected.append(line)
        elif "K5 RECON side=candidate" in line:
            selected.append(line)
    write("runtime_evidence_v114.txt", [
        f"# Source: docs/diagnostics/g5-transition-probe-v5-20261119/run1-candidate/CharacterPanel.log",
        f"# Source SHA-256: {sha256(RUN)}",
        "# Selected: WeapEquipBFR subgraph at cmd=1/13, cmd=1 census targets,",
        "# equip pre-sample SM at seq=0/12, cmd=1 probe header, K5 RECON",
        "",
    ], selected)


def main() -> int:
    extract_weap_equip()
    extract_default_state()
    extract_runtime()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
