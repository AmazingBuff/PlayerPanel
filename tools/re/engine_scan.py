#!/usr/bin/env python3
"""Read-only reconnaissance over a Skyrim executable image.

Answers two questions without a debugger:

* which function owns an address - taken from the PE exception directory
  (.pdata), so the answer is the real unwind bounds and not a guess;
* which functions touch a known struct offset - found by scanning .text for
  memory operands whose disp32 equals the offset, then attributed to the
  owning .pdata function.

Stdlib only. Nothing is written.
"""

from __future__ import annotations

import argparse
import bisect
import struct
import sys
from dataclasses import dataclass, field
from pathlib import Path

DIRECTORY_ENTRY_EXCEPTION = 3
PE32_PLUS_MAGIC = 0x20B

# A memory operand with a 32-bit displacement encodes mod=10 in the ModRM byte
# and cannot use r/m=100 (that form means SIB follows).
MODRM_MOD_MASK = 0xC0
MODRM_MOD_DISP32 = 0x80
MODRM_RM_MASK = 0x07
MODRM_RM_SIB = 0x04

# Opcode tails that can directly precede a memory ModRM. Only used to label a
# hit, never to reject one: an unlabelled hit is still reported.
OPCODE_TAILS = {
    b"\x66\x89": "mov r16",
    b"\x66\x8b": "mov r16(read)",
    b"\x66\xc7": "mov r16,imm",
    b"\x89": "mov r32",
    b"\x8b": "mov r32(read)",
    b"\xc7": "mov r32,imm",
    b"\x48\x89": "mov r64",
    b"\x48\x8b": "mov r64(read)",
    b"\x48\x8d": "lea",
    b"\x0f\xb7": "movzx r32,r16",
    b"\x0f\xbf": "movsx r32,r16",
    b"\x0f\xb6": "movzx r32,r8",
    b"\x0f\xbe": "movsx r32,r8",
    b"\x0f\x10": "movups(read)",
    b"\x0f\x11": "movups",
    b"\x0f\x28": "movaps(read)",
    b"\x0f\x29": "movaps",
    b"\x0f\x2e": "ucomiss",
    b"\x0f\x2f": "comiss",
    b"\xf3\x0f\x10": "movss(read)",
    b"\xf3\x0f\x11": "movss",
    b"\xf2\x0f\x10": "movsd(read)",
    b"\xf2\x0f\x11": "movsd",
    b"\xf3\x0f\x2c": "cvttss2si",
    b"\xf3\x0f\x59": "mulss",
    b"\xf3\x0f\x5c": "subss",
    b"\xf3\x0f\x58": "addss",
    b"\x66\x0f\x2e": "ucomisd",
}


@dataclass
class Section:
    name: str
    virtual_address: int
    virtual_size: int
    raw_pointer: int
    raw_size: int

    @property
    def span(self) -> int:
        return max(self.virtual_size, self.raw_size)


@dataclass
class Function:
    begin: int
    end: int
    unwind: int


@dataclass
class Hit:
    rva: int
    opcode: str
    function: Function | None


@dataclass
class FunctionHits:
    function: Function
    disps: dict[int, list[Hit]] = field(default_factory=dict)

    @property
    def total(self) -> int:
        return sum(len(hits) for hits in self.disps.values())


class Image:
    def __init__(self, path: str | Path) -> None:
        path = Path(path)
        self.path = path
        self.data = path.read_bytes()
        self.sections: list[Section] = []
        self.functions: list[Function] = []
        self._function_begins: list[int] = []
        self.image_base = 0
        self.entry_point = 0
        self._parse_headers()
        self._parse_functions()

    def _parse_headers(self) -> None:
        data = self.data
        if data[:2] != b"MZ":
            raise SystemExit(f"{self.path}: not a PE image")
        pe = struct.unpack_from("<I", data, 0x3C)[0]
        if data[pe : pe + 4] != b"PE\0\0":
            raise SystemExit(f"{self.path}: missing PE signature")
        coff = pe + 4
        section_count = struct.unpack_from("<H", data, coff + 2)[0]
        optional_size = struct.unpack_from("<H", data, coff + 16)[0]
        optional = coff + 20
        magic = struct.unpack_from("<H", data, optional)[0]
        if magic != PE32_PLUS_MAGIC:
            raise SystemExit(f"{self.path}: expected PE32+ (got magic 0x{magic:x})")
        self.image_base = struct.unpack_from("<Q", data, optional + 24)[0]
        self.entry_point = struct.unpack_from("<I", data, optional + 16)[0]
        directory_count = struct.unpack_from("<I", data, optional + 108)[0]
        directories = [
            struct.unpack_from("<II", data, optional + 112 + 8 * index)
            for index in range(directory_count)
        ]
        table = optional + optional_size
        for index in range(section_count):
            entry = table + 40 * index
            name = data[entry : entry + 8].rstrip(b"\0").decode("ascii", "replace")
            virtual_size, virtual_address, raw_size, raw_pointer = struct.unpack_from(
                "<IIII", data, entry + 8
            )
            self.sections.append(
                Section(name, virtual_address, virtual_size, raw_pointer, raw_size)
            )
        if len(directories) > DIRECTORY_ENTRY_EXCEPTION:
            self.exception_rva, self.exception_size = directories[DIRECTORY_ENTRY_EXCEPTION]
        else:
            self.exception_rva, self.exception_size = 0, 0

    def _parse_functions(self) -> None:
        if not self.exception_rva or not self.exception_size:
            return
        offset = self.rva_to_offset(self.exception_rva)
        if offset is None:
            return
        for index in range(self.exception_size // 12):
            begin, end, unwind = struct.unpack_from("<III", self.data, offset + 12 * index)
            if end > begin:
                self.functions.append(Function(begin, end, unwind))
        self.functions.sort(key=lambda function: function.begin)
        self._function_begins = [function.begin for function in self.functions]

    def rva_to_offset(self, rva: int) -> int | None:
        for section in self.sections:
            if section.virtual_address <= rva < section.virtual_address + section.span:
                delta = rva - section.virtual_address
                if delta < section.raw_size:
                    return section.raw_pointer + delta
                return None
        return None

    def function_at(self, rva: int) -> Function | None:
        index = bisect.bisect_right(self._function_begins, rva) - 1
        if index >= 0:
            function = self.functions[index]
            if function.begin <= rva < function.end:
                return function
        return None

    def va(self, rva: int) -> int:
        return self.image_base + rva

    def text_sections(self) -> list[Section]:
        return [section for section in self.sections if section.raw_size and "text" in section.name.lower()]

    def find_disp32(self, disp: int) -> list[Hit]:
        """Every memory operand in .text whose 32-bit displacement equals disp."""
        needle = struct.pack("<I", disp)
        hits: list[Hit] = []
        for section in self.text_sections():
            blob = self.data[section.raw_pointer : section.raw_pointer + section.raw_size]
            start = 0
            while True:
                index = blob.find(needle, start)
                if index < 0:
                    break
                start = index + 1
                modrm_index = index - 1
                if modrm_index < 2:
                    continue
                modrm = blob[modrm_index]
                if (modrm & MODRM_MOD_MASK) != MODRM_MOD_DISP32:
                    continue
                if (modrm & MODRM_RM_MASK) == MODRM_RM_SIB:
                    continue
                opcode = self._label_opcode(blob, modrm_index)
                rva = section.virtual_address + index - 4
                hits.append(Hit(rva, opcode, self.function_at(rva)))
        return hits

    @staticmethod
    def _label_opcode(blob: bytes, modrm_index: int) -> str:
        for length in (3, 2, 1):
            start = modrm_index - length
            if start < 0:
                continue
            label = OPCODE_TAILS.get(blob[start:modrm_index])
            if label:
                return label
        return "?"


def group_by_function(hits: list[Hit], disps: list[int]) -> tuple[dict[int, FunctionHits], list[Hit]]:
    grouped: dict[int, FunctionHits] = {}
    orphans: list[Hit] = []
    for disp in disps:
        for hit in hits:
            if hit.function is None:
                orphans.append(hit)
                continue
            entry = grouped.setdefault(hit.function.begin, FunctionHits(hit.function))
            entry.disps.setdefault(disp, []).append(hit)
    return grouped, orphans


def command_func(image: Image, args: argparse.Namespace) -> int:
    for value in args.rva:
        rva = int(value, 0)
        function = image.function_at(rva)
        if function is None:
            print(f"rva 0x{rva:x} (va 0x{image.va(rva):x}): no .pdata entry")
            continue
        print(
            f"rva 0x{rva:x} (va 0x{image.va(rva):x}): function "
            f"0x{function.begin:x}..0x{function.end:x} (va 0x{image.va(function.begin):x}"
            f"..0x{image.va(function.end):x}) size=0x{function.end - function.begin:x} "
            f"delta=+0x{rva - function.begin:x} unwind=0x{function.unwind:x}"
        )
    return 0


def command_scan(image: Image, args: argparse.Namespace) -> int:
    disps = [int(value, 0) for value in args.disp]
    all_hits: list[Hit] = []
    per_disp: dict[int, list[Hit]] = {}
    for disp in disps:
        hits = image.find_disp32(disp)
        per_disp[disp] = hits
        all_hits.extend(hits)
    grouped, orphans = group_by_function(all_hits, disps)

    require = [int(value, 0) for value in (args.require or [])]
    print(f"image base 0x{image.image_base:x}  functions={len(image.functions)}")
    for disp in disps:
        attributed = sum(1 for hit in per_disp[disp] if hit.function is not None)
        print(
            f"disp +0x{disp:x}: {len(per_disp[disp])} operands, {attributed} attributed, "
            f"{len(per_disp[disp]) - attributed} outside .pdata"
        )

    selected = []
    for entry in grouped.values():
        present = set(entry.disps)
        if any(disp not in present for disp in require):
            continue
        selected.append(entry)
    selected.sort(key=lambda entry: (-len(entry.disps), -entry.total))

    print(f"\n{len(selected)} function(s) matching require={[hex(d) for d in require]}")
    for entry in selected[: args.limit]:
        offsets = " ".join(f"+0x{disp:x}x{len(hits)}" for disp, hits in sorted(entry.disps.items()))
        print(
            f"  0x{entry.function.begin:x}..0x{entry.function.end:x} "
            f"size=0x{entry.function.end - entry.function.begin:x} disps[{offsets}]"
        )
        for disp, hits in sorted(entry.disps.items()):
            for hit in hits:
                print(f"      +0x{disp:x} at 0x{hit.rva:x} ({hit.opcode})")

    if args.show_unattributed and orphans:
        print(f"\n{len(orphans)} operand(s) outside .pdata:")
        for hit in orphans[: args.limit]:
            print(f"  {hit.opcode} at 0x{hit.rva:x}")
    return 0


def command_bytes(image: Image, args: argparse.Namespace) -> int:
    rva = int(args.rva, 0)
    offset = image.rva_to_offset(rva)
    if offset is None:
        print(f"rva 0x{rva:x}: not backed by file data")
        return 1
    blob = image.data[offset : offset + args.length]
    for index in range(0, len(blob), 16):
        chunk = blob[index : index + 16]
        hexed = " ".join(f"{byte:02x}" for byte in chunk)
        text = "".join(chr(byte) if 32 <= byte < 127 else "." for byte in chunk)
        print(f"0x{rva + index:08x}  {hexed:<47}  {text}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("image", type=Path)
    subparsers = parser.add_subparsers(dest="command", required=True)

    func = subparsers.add_parser("func", help="show the .pdata function owning an RVA")
    func.add_argument("rva", nargs="+")
    func.set_defaults(handler=command_func)

    scan = subparsers.add_parser("scan", help="find memory operands with a given disp32")
    scan.add_argument("disp", nargs="+")
    scan.add_argument("--require", nargs="*", help="only functions also touching these disps")
    scan.add_argument("--limit", type=int, default=25)
    scan.add_argument("--show-unattributed", action="store_true")
    scan.set_defaults(handler=command_scan)

    raw = subparsers.add_parser("bytes", help="hexdump at an RVA")
    raw.add_argument("rva")
    raw.add_argument("--length", type=int, default=64)
    raw.set_defaults(handler=command_bytes)

    args = parser.parse_args()
    image = Image(args.image)
    print(f"{args.image.name}: image base 0x{image.image_base:x} entry 0x{image.entry_point:x} "
          f"sections={len(image.sections)} .pdata functions={len(image.functions)}", file=sys.stderr)
    return args.handler(image, args)


if __name__ == "__main__":
    raise SystemExit(main())
