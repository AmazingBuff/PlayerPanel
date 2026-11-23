"""C-IDLE v1 §7.6：Smooth Moveset.esp 的 MGEF 记录普查（2026-11-23 离线取证）。

回答一个具体问题：OAR 条件里写的 formID "801"/"802"/"804" 与运行日志
smooth_marker_report 报的 token "243801"/"243802"/"243804" 是不是同一组效果？

做法：按 TES4/ESP 容器格式遍历 GRUP/MGRE 记录，取每条的 32 位 FormID 与
EDID，然后按两种候选换算规则比对日志 token。不做任何猜测性匹配：换算是
输入参数，结论只陈述"在该规则下命中/不命中"。

纯 stdlib、零第三方依赖、零网络、只读输入。
运行：python -I tools/esp_mgef_probe.py --esp <Smooth Moveset.esp> [--tokens 243801,...]
      python -I tools/esp_mgef_probe.py --self-test
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

# Observed in CharacterPanel-issue-trace-r2-20261022-2314.log (DAC OBSERVED rows).
DEFAULT_TOKENS = [0x243801, 0x243802, 0x243804, 0x243806, 0x243807, 0x243808]
# The formIDs the OAR conditions name, as written in config.json ("801" etc.).
DEFAULT_CONDITION_IDS = [0x801, 0x802, 0x804]

RECORD_HEADER = 24  # 4 type + 4 dataSize + 4 flags + 4 formID + 4 vc + 2 formVersion + 2 vc2
GRUP_HEADER = 24


def _parse_records(data: bytes, start: int, end: int, out: list[tuple[str, int, str]]):
    """Walk one container range, appending (signature, form_id, edid) for MGEF records."""
    pos = start
    while pos + RECORD_HEADER <= end:
        signature = data[pos:pos + 4]
        if signature == b"GRUP":
            if pos + GRUP_HEADER > end:
                return
            size = struct.unpack_from("<I", data, pos + 4)[0]
            group_end = min(pos + size, end)
            # A GRUP payload begins right after its own 24-byte header.
            _parse_records(data, pos + GRUP_HEADER, group_end, out)
            pos = group_end
            continue
        data_size = struct.unpack_from("<I", data, pos + 4)[0]
        form_id = struct.unpack_from("<I", data, pos + 12)[0]
        body = pos + RECORD_HEADER
        next_record = body + data_size
        if next_record > end:
            return
        if signature == b"MGEF":
            out.append(("MGEF", form_id, _read_edid(data, body, next_record)))
        pos = next_record


def _read_edid(data: bytes, start: int, end: int) -> str:
    """First EDID subrecord in the record body, decoded as null-terminated ASCII."""
    pos = start
    while pos + 6 <= end:
        sub_type = data[pos:pos + 4]
        sub_size = struct.unpack_from("<H", data, pos + 4)[0]
        body = pos + 6
        if body + sub_size > end:
            break
        if sub_type == b"EDID":
            return data[body:body + sub_size].split(b"\x00")[0].decode("ascii", "replace")
        pos = body + sub_size
    return ""


def read_mgef(esp: Path) -> list[tuple[str, int, str]]:
    """All MGEF records in an ESP/ESM, in file order."""
    data = esp.read_bytes()
    if len(data) < RECORD_HEADER or data[0:4] != b"TES4":
        raise ValueError(f"{esp} is not a TES4 plugin")
    tes4_size = struct.unpack_from("<I", data, 4)[0]
    records: list[tuple[str, int, str]] = []
    _parse_records(data, RECORD_HEADER + tes4_size, len(data), records)
    return records


def local_id(form_id: int) -> int:
    """The 24-bit object index, dropping the load-order byte."""
    return form_id & 0xFFFFFF


def match_rule(tokens: list[int], condition_ids: list[int]) -> dict:
    """Compare log tokens to condition formIDs under the two candidate conversions.

    Rule A: a FormID is (load_order_byte << 24) | object_index. If a log token is a
    full FormID, dropping the load-order byte must land on the OAR condition value.
    No second rule is asserted: the byte-order hypothesis was dropped because there
    is no evidence for it and it would only invite a coincidental match.
    """
    mapped = {t: local_id(t) for t in tokens}
    hits = {t: v for t, v in mapped.items() if v in set(condition_ids)}
    return {"mapping": mapped, "hits": hits}


def _self_test() -> int:
    """T1-T4: a synthetic plugin exercises the container walk without touching disk."""
    import tempfile

    def record(signature: bytes, form_id: int, edid: str) -> bytes:
        payload = b"EDID" + struct.pack("<H", len(edid) + 1) + edid.encode() + b"\x00"
        return signature + struct.pack("<III", len(payload), 0, form_id) + b"\x00" * 8 + payload

    def grup(label: bytes, children: bytes) -> bytes:
        size = GRUP_HEADER + len(children)
        return b"GRUP" + struct.pack("<I", size) + label + b"\x00" * 12 + children

    mgef_a = record(b"MGEF", 0x24000801, "Smooth_Start")
    mgef_b = record(b"MGEF", 0x24000802, "Smooth_Loop")
    other = record(b"SPEL", 0x2400080A, "Smooth_Spell")
    body = grup(b"MGEF", mgef_a + mgef_b) + other
    tes4_payload = b"HEDR" + struct.pack("<H", 12) + b"\x00" * 12
    plugin = b"TES4" + struct.pack("<I", len(tes4_payload)) + b"\x00" * 16 + tes4_payload + body

    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "synthetic.esp"
        path.write_bytes(plugin)
        found = read_mgef(path)
    assert found == [("MGEF", 0x24000801, "Smooth_Start"), ("MGEF", 0x24000802, "Smooth_Loop")], found

    # T2: non-MGEF records are skipped.
    assert all(kind == "MGEF" for kind, _, _ in found)

    # T3: local_id keeps the low 24 bits and drops only the load-order byte.
    assert local_id(0x24000801) == 0x000801
    assert local_id(0x00000801) == 0x801
    assert local_id(0xFF000801) == 0x000801

    # T4: match is a pure function of its inputs, computed independently below.
    hits = match_rule([0x243801, 0x243802], [0x801, 0x802])
    assert hits["mapping"] == {0x243801: 0x243801 & 0xFFFFFF, 0x243802: 0x243802 & 0xFFFFFF}
    # Under the low-24-bit rule these tokens are NOT 0x801/0x802, so nothing matches:
    # 0x243801 & 0xFFFFFF == 0x243801, which is a different number from 0x801.
    assert hits["hits"] == {}
    print("4 esp_mgef_probe cases passed")
    return 0


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--esp", type=Path)
    parser.add_argument("--tokens", default="")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args(argv)

    if args.self_test:
        return _self_test()
    if not args.esp:
        parser.error("--esp is required unless --self-test")

    tokens = ([int(x, 16) for x in args.tokens.split(",") if x.strip()]
              if args.tokens else DEFAULT_TOKENS)
    records = read_mgef(args.esp)
    print(f"{args.esp.name}: {len(records)} MGEF records")
    for kind, form_id, edid in records:
        print(f"  {form_id:08X}  local={local_id(form_id):06X}  {edid}")

    print()
    print("log tokens:", ", ".join(f"{t:06X}" for t in tokens))
    print("OAR condition ids:", ", ".join(f"{v:03X}" for v in DEFAULT_CONDITION_IDS))
    hits = match_rule(tokens, DEFAULT_CONDITION_IDS)
    known = {local_id(form_id) for _, form_id, _ in records}
    print()
    print("token                 low24   in-esp  OAR-condition")
    for token in tokens:
        low = local_id(token)
        print(f"  {token:06X}           {low:06X}   {'yes' if low in known else 'no ':5}"
              f"   {'yes' if low in DEFAULT_CONDITION_IDS else 'no'}")
    print(f"\nshared with OAR condition ids: {len(hits['hits'])}/{len(tokens)}")
    for token, low in sorted(hits["hits"].items()):
        name = next((e for k, f, e in records if local_id(f) == low), "")
        print(f"  {token:06X} -> {low:03X}  {name}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
