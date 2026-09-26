#!/usr/bin/env python3
"""Generate the default PlayerPanel skin SWF and validate its own structure.

The panel is an engine-registered Scaleform menu whose chrome comes from
``Interface\\PlayerPanel\\panel.swf``.  ``BSScaleformManager::LoadMovie`` resolves
that path against the interface folder and appends the extension itself, so the
file this script writes is the movie the panel loads by default.  A reskin mod
replaces the file at the same path with its own movie; this generator exists so
that the repository carries a deterministic, script-reproducible placeholder and
so that the mechanism can be proven without a compiler.

The emitted SWF is deliberately minimal and script-free:

* SWF version 8, uncompressed (``FWS``), so the bytes are byte-for-byte
  reproducible and easy to audit.
* An opaque stage-filling backdrop plus one stage-filling frame: a ring of four
  straight edges drawn in a single solid colour.  The backdrop is what stops the
  world showing through the panel; the ring is the panel's frame.  The frame is
  deliberately a visible band rather than a hairline, because the plugin
  composites the character over the backdrop and inside the frame.
* No ActionScript, no ``DefineEditText`` and no gradient or bitmap fill, so the
  only tags present are ``SetBackgroundColor``, ``DefineShape3``,
  ``PlaceObject2``, ``ShowFrame`` and ``End``.

Nothing is deployed: the caller installs the artifact at the documented path.
The script uses only the Python standard library, and it never writes a file it
has not first parsed in memory.
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

# ---------------------------------------------------------------------------
# Deterministic content
# ---------------------------------------------------------------------------

SWF_SIGNATURE = b"FWS"
SWF_VERSION = 8
FRAME_RATE = 24.0
FRAME_COUNT = 1

TWIPS_PER_PIXEL = 20
STAGE_WIDTH_TWIPS = 320 * TWIPS_PER_PIXEL
STAGE_HEIGHT_TWIPS = 480 * TWIPS_PER_PIXEL

# The frame band is the panel's outer border band.  The plugin composites the
# character inset by ``PanelSkinInsetFraction`` of the panel height, and it scales
# the movie into exactly the panel rectangle, so authoring the band as the same
# fraction of the stage height keeps the visible band and the plugin's inset in
# step after the movie is scaled: the band is exactly the ring the plugin leaves
# untouched.
#
# The default mirrors ``Default_Panel_Skin_Inset_Fraction`` in src/config/config.cpp
# (0.03).  It is deliberately a frame rather than a hairline: the authoring rule is
# that a skin's frame must be at least as thick as the configured inset, and a band
# one pixel wide would be invisible once the character is composited over it.
BORDER_BAND_FRACTION = 0.03
BORDER_BAND_TWIPS = round(BORDER_BAND_FRACTION * STAGE_HEIGHT_TWIPS)  # 288 twips

# The skin draws both things the panel needs from it: an opaque backdrop that fills
# its stage, so no world content shows through the panel, and the frame band on top
# of it.  A skin whose backdrop is transparent would show the world through the
# panel; that is the skin's responsibility, and this default skin is opaque.
BACKGROUND_RGB = (0x0D, 0x0D, 0x10)
BACKDROP_RGBA = (0x0D, 0x0D, 0x10, 0xFF)
FRAME_RGBA = (0xC8, 0xA8, 0x6B, 0xFF)

# Fill style indices, 1-based as SWF requires.  The backdrop is style 1 and the
# frame band is style 2, so the frame is painted after the backdrop and stays visible
# where the two overlap.
FILL_STYLE_BACKDROP = 1
FILL_STYLE_FRAME = 2

SHAPE_ID = 1
PLACE_DEPTH = 1

# The complete tag vocabulary the emitted file is allowed to use.  Action tags
# (DoAction 12, DoInitAction 59, DefineFunction 62, DefineFunction2 64, DoABC 82)
# and DefineEditText (37) are absent by construction; the whitelist below is what
# makes their absence checkable rather than merely intended.
TAG_END = 0
TAG_SHOW_FRAME = 1
TAG_SET_BACKGROUND_COLOR = 9
TAG_PLACE_OBJECT_2 = 26
TAG_DEFINE_SHAPE_3 = 32
ALLOWED_TAGS = frozenset({TAG_END, TAG_SHOW_FRAME, TAG_SET_BACKGROUND_COLOR, TAG_PLACE_OBJECT_2, TAG_DEFINE_SHAPE_3})

TAG_LONG_FORM = 0x3F
FILL_STYLE_SOLID = 0x00

ARTIFACT_RELATIVE_PATH = Path("assets") / "PlayerPanel" / "panel.swf"


class SwfError(Exception):
    """Raised when a buffer is not a structurally valid SWF of the expected shape."""


# ---------------------------------------------------------------------------
# Bit-level writer and reader
# ---------------------------------------------------------------------------


class _BitWriter:
    """Appends most-significant-bit-first, byte-aligned-at-the-end bit fields."""

    def __init__(self) -> None:
        self._bits: list[int] = []

    def write_unsigned(self, value: int, bit_count: int) -> None:
        if bit_count < 0:
            raise SwfError("negative bit count")
        if value < 0 or value >= (1 << bit_count):
            raise SwfError(f"value {value} does not fit in {bit_count} bits")
        for shift in range(bit_count - 1, -1, -1):
            self._bits.append((value >> shift) & 1)

    def write_signed(self, value: int, bit_count: int) -> None:
        if bit_count < 1:
            raise SwfError("a signed field needs at least one bit")
        if value < -(1 << (bit_count - 1)) or value >= (1 << (bit_count - 1)):
            raise SwfError(f"value {value} does not fit in {bit_count} signed bits")
        self.write_unsigned(value & ((1 << bit_count) - 1), bit_count)

    def to_bytes(self) -> bytes:
        padding = (-len(self._bits)) % 8
        bits = self._bits + [0] * padding
        out = bytearray()
        for index in range(0, len(bits), 8):
            byte = 0
            for offset in range(8):
                byte = (byte << 1) | bits[index + offset]
            out.append(byte)
        return bytes(out)


class _Reader:
    """Reads bytes and bit fields from a buffer, refusing to run past the end."""

    def __init__(self, data: bytes, bit_position: int = 0) -> None:
        self._data = data
        self._bit_position = bit_position

    def bit_position(self) -> int:
        return self._bit_position

    def remaining_bits(self) -> int:
        return len(self._data) * 8 - self._bit_position

    def align(self) -> None:
        self._bit_position = (self._bit_position + 7) & ~7

    def _take_bits(self, count: int) -> int:
        if count < 0:
            raise SwfError("negative bit count")
        if count > self.remaining_bits():
            raise SwfError("read past the end of the buffer")
        value = 0
        for _ in range(count):
            byte = self._data[self._bit_position >> 3]
            bit = (byte >> (7 - (self._bit_position & 7))) & 1
            value = (value << 1) | bit
            self._bit_position += 1
        return value

    def read_unsigned(self, count: int) -> int:
        return self._take_bits(count)

    def read_signed(self, count: int) -> int:
        if count == 0:
            return 0
        value = self._take_bits(count)
        if value & (1 << (count - 1)):
            value -= 1 << count
        return value

    def read_bytes(self, count: int) -> bytes:
        self.align()
        if count < 0:
            raise SwfError("negative byte count")
        if count * 8 > self.remaining_bits():
            raise SwfError("read past the end of the buffer")
        start = self._bit_position >> 3
        self._bit_position += count * 8
        return self._data[start : start + count]

    def read_u8(self) -> int:
        return self.read_bytes(1)[0]

    def read_u16(self) -> int:
        return struct.unpack("<H", self.read_bytes(2))[0]

    def read_u32(self) -> int:
        return struct.unpack("<I", self.read_bytes(4))[0]


# ---------------------------------------------------------------------------
# Encoding helpers
# ---------------------------------------------------------------------------


def _signed_bit_length(value: int) -> int:
    """Smallest two's-complement width that can hold ``value`` (at least one bit)."""
    if value >= 0:
        return max(1, value.bit_length() + 1)
    return max(1, (~value).bit_length() + 1)


def encode_rect(values: tuple[int, int, int, int]) -> bytes:
    """A RECT: a five-bit width followed by four signed values, byte-aligned.

    ``values`` is the SWF spec's field order ``(x_min, x_max, y_min, y_max)`` - not
    the ``(x0, y0, x1, y1)`` corner order a drawing helper would use.  Writing the
    two pairs the other way round declares a zero-width, displaced stage, which is
    exactly what a Scaleform player then renders: nothing.
    """
    x_min, x_max, y_min, y_max = values
    width = max(_signed_bit_length(value) for value in values)
    writer = _BitWriter()
    writer.write_unsigned(width, 5)
    for value in (x_min, x_max, y_min, y_max):
        writer.write_signed(value, width)
    return writer.to_bytes()


def encode_tag(code: int, body: bytes) -> bytes:
    if len(body) < TAG_LONG_FORM:
        header = struct.pack("<H", (code << 6) | len(body))
    else:
        header = struct.pack("<H", (code << 6) | TAG_LONG_FORM) + struct.pack("<I", len(body))
    return header + body


def _fill_style_bits() -> int:
    """Bits needed to index the highest fill style in use (style indices are 1-based)."""
    return max(FILL_STYLE_BACKDROP, FILL_STYLE_FRAME).bit_length()


def _encode_shape_records(max_delta: int) -> bytes:
    """The shape records: the opaque stage-filling backdrop, then the frame ring over it."""
    num_fill_bits = _fill_style_bits()
    num_line_bits = 0
    # The two size fields follow two different spec conventions, established by rendering
    # every combination with a spec-correct parser (JPEXS ffdec): a style change's MoveBits
    # carries the raw bit count, while an edge record's NumBits carries "the actual bit
    # count minus two".  Writing either field the other way shifts the whole record stream:
    # the first generated skin did exactly that, and the shape parsed into garbage that
    # painted nothing - the backdrop and the frame band never reached the screen.
    move_bits = _signed_bit_length(max_delta)
    edge_bits = _signed_bit_length(max_delta)
    move_bits_field = move_bits
    edge_bits_field = edge_bits - 2
    stage_w = STAGE_WIDTH_TWIPS
    stage_h = STAGE_HEIGHT_TWIPS
    band = BORDER_BAND_TWIPS
    # Each rectangle is traced clockwise, so its interior lies on the right of the
    # directed edge and the single right-side fill (FillStyle1) carries the colour - the
    # combination the rendered proof picked out.  The backdrop is traced first, so the
    # four frame bands are painted over its outer edge.
    rectangles = (
        (0, 0, stage_w, stage_h, FILL_STYLE_BACKDROP),
        (0, 0, stage_w, band, FILL_STYLE_FRAME),
        (0, stage_h - band, stage_w, stage_h, FILL_STYLE_FRAME),
        (0, band, band, stage_h - band, FILL_STYLE_FRAME),
        (stage_w - band, band, stage_w, stage_h - band, FILL_STYLE_FRAME),
    )

    writer = _BitWriter()
    writer.write_unsigned(num_fill_bits, 4)
    writer.write_unsigned(num_line_bits, 4)

    for x0, y0, x1, y1, style in rectangles:
        # STYLECHANGERECORD: move to the corner and select a fill style on the right.
        writer.write_unsigned(0, 1)  # TypeFlag: a style change
        writer.write_unsigned(0, 1)  # StateNewStyles
        writer.write_unsigned(0, 1)  # StateLineStyle
        writer.write_unsigned(1, 1)  # StateFillStyle1
        writer.write_unsigned(0, 1)  # StateFillStyle0
        writer.write_unsigned(1, 1)  # StateMoveTo
        writer.write_unsigned(move_bits_field, 5)
        writer.write_signed(x0, move_bits)
        writer.write_signed(y0, move_bits)
        writer.write_unsigned(style, num_fill_bits)
        for delta_x, delta_y in ((x1 - x0, 0), (0, y1 - y0), (-(x1 - x0), 0), (0, -(y1 - y0))):
            # STRAIGHTEDGERECORD with the general-line flag.
            writer.write_unsigned(1, 1)  # TypeFlag: an edge
            writer.write_unsigned(1, 1)  # StraightFlag
            writer.write_unsigned(edge_bits_field, 4)
            writer.write_unsigned(1, 1)  # GeneralLineFlag
            writer.write_signed(delta_x, edge_bits)
            writer.write_signed(delta_y, edge_bits)
    writer.write_unsigned(0, 6)  # ENDSHAPERECORD
    return writer.to_bytes()


def build_define_shape_3() -> bytes:
    body = bytearray()
    body += struct.pack("<H", SHAPE_ID)
    body += encode_rect((0, STAGE_WIDTH_TWIPS, 0, STAGE_HEIGHT_TWIPS))
    body.append(2)  # FillStyleCount: the backdrop and the frame band
    body.append(FILL_STYLE_SOLID)
    body += bytes(BACKDROP_RGBA)
    body.append(FILL_STYLE_SOLID)
    body += bytes(FRAME_RGBA)
    body.append(0)  # LineStyleCount
    max_delta = max(STAGE_WIDTH_TWIPS, STAGE_HEIGHT_TWIPS)
    body += _encode_shape_records(max_delta)
    return bytes(body)


def build_place_object_2() -> bytes:
    body = bytearray()
    body.append(0x06)  # HasCharacter | HasMatrix
    body += struct.pack("<H", PLACE_DEPTH)
    body += struct.pack("<H", SHAPE_ID)
    # MATRIX: no scale, no rotation, translation (0, 0) in one bit.
    writer = _BitWriter()
    writer.write_unsigned(0, 1)  # HasScale
    writer.write_unsigned(0, 1)  # HasRotate
    writer.write_unsigned(1, 5)  # NTranslateBits
    writer.write_signed(0, 1)  # TranslateX
    writer.write_signed(0, 1)  # TranslateY
    body += writer.to_bytes()
    return bytes(body)


def build_swf() -> bytes:
    """The complete SWF file, byte for byte."""
    frame_rect = encode_rect((0, STAGE_WIDTH_TWIPS, 0, STAGE_HEIGHT_TWIPS))
    tags = b"".join(
        (
            encode_tag(TAG_SET_BACKGROUND_COLOR, bytes(BACKGROUND_RGB)),
            encode_tag(TAG_DEFINE_SHAPE_3, build_define_shape_3()),
            encode_tag(TAG_PLACE_OBJECT_2, build_place_object_2()),
            encode_tag(TAG_SHOW_FRAME, b""),
            encode_tag(TAG_END, b""),
        )
    )
    body = frame_rect + struct.pack("<H", int(FRAME_RATE * 256)) + struct.pack("<H", FRAME_COUNT) + tags
    file_length = 8 + len(body)
    header = SWF_SIGNATURE + bytes((SWF_VERSION,)) + struct.pack("<I", file_length)
    return header + body


# ---------------------------------------------------------------------------
# Structural parser and validator
# ---------------------------------------------------------------------------


def _parse_rect(reader: _Reader) -> tuple[tuple[int, int, int, int], int]:
    width = reader.read_unsigned(5)
    if width == 0:
        raise SwfError("RECT declares a zero bit width")
    values = tuple(reader.read_signed(width) for _ in range(4))
    reader.align()
    x_min, x_max, y_min, y_max = values
    # A degenerate span (zero width or height) is a spec-order mistake, not a stylistic
    # choice: a player given such a stage renders nothing.  Reject it rather than merely
    # reporting it, so the artifact can never carry one again.
    if x_min > x_max or y_min > y_max:
        raise SwfError("RECT has an inverted span")
    if x_min == x_max or y_min == y_max:
        raise SwfError("RECT has a degenerate (zero-width or zero-height) span")
    return (x_min, x_max, y_min, y_max), width


def _parse_matrix(reader: _Reader) -> None:
    if reader.read_unsigned(1):
        width = reader.read_unsigned(5)
        reader.read_signed(width)
        reader.read_signed(width)
    if reader.read_unsigned(1):
        width = reader.read_unsigned(5)
        reader.read_signed(width)
        reader.read_signed(width)
    width = reader.read_unsigned(5)
    reader.read_signed(width)
    reader.read_signed(width)


def _validate_define_shape_3(body: bytes) -> dict:
    reader = _Reader(body)
    shape_id = reader.read_u16()
    bounds, _ = _parse_rect(reader)

    fill_count = reader.read_u8()
    if fill_count == 0xFF:
        fill_count = reader.read_u16()
    if fill_count == 0:
        raise SwfError("a shape with no fill style would draw nothing")
    fill_colors = []
    for _ in range(fill_count):
        fill_type = reader.read_u8()
        if fill_type != FILL_STYLE_SOLID:
            raise SwfError(f"a non-solid fill style ({fill_type:#04x}) is not script-free-safe for this skin")
        fill_colors.append(tuple(reader.read_bytes(4)))  # RGBA
    # The authoring contract is that a skin supplies the panel's opaque backdrop; a skin made only of
    # translucent fills would let the world show through the panel.  Checking it here makes the
    # obligation verifiable for the shipped default rather than merely intended.
    if not any(color[3] == 0xFF for color in fill_colors):
        raise SwfError("a skin with no fully opaque fill would show the world through the panel")

    line_count = reader.read_u8()
    if line_count == 0xFF:
        line_count = reader.read_u16()
    for _ in range(line_count):
        reader.read_u16()  # line width
        reader.read_bytes(4)  # RGBA

    num_fill_bits = reader.read_unsigned(4)
    num_line_bits = reader.read_unsigned(4)
    if num_fill_bits == 0:
        raise SwfError("a shape with fill styles must declare fill bits")

    style_changes = 0
    straight_edges = 0
    curved_edges = 0
    shape_end = False
    while not shape_end:
        if reader.remaining_bits() < 6:
            raise SwfError("shape records ended without an ENDSHAPERECORD")
        if reader.read_unsigned(1) == 0:
            state_new_styles = reader.read_unsigned(1)
            state_line_style = reader.read_unsigned(1)
            state_fill_style_1 = reader.read_unsigned(1)
            state_fill_style_0 = reader.read_unsigned(1)
            state_move_to = reader.read_unsigned(1)
            if not (state_new_styles or state_line_style or state_fill_style_1 or state_fill_style_0 or state_move_to):
                shape_end = True
                break
            if state_new_styles:
                raise SwfError("a shape that redefines its styles mid-record is not supported here")
            if state_move_to:
                # MoveBits carries the raw bit count (the "minus two" rule is the edge
                # records' NumBits convention only).
                move_bits = reader.read_unsigned(5)
                reader.read_signed(move_bits)
                reader.read_signed(move_bits)
            if state_fill_style_0:
                reader.read_unsigned(num_fill_bits)
            if state_fill_style_1:
                reader.read_unsigned(num_fill_bits)
            if state_line_style:
                if num_line_bits == 0:
                    raise SwfError("a style change selects a line style but the shape declares no line bits")
                reader.read_unsigned(num_line_bits)
            style_changes += 1
        else:
            straight = reader.read_unsigned(1)
            # NumBits stores the actual bit count minus two, per the spec.
            edge_bits = reader.read_unsigned(4) + 2
            if straight:
                if reader.read_unsigned(1):
                    reader.read_signed(edge_bits)
                    reader.read_signed(edge_bits)
                elif reader.read_unsigned(1):
                    reader.read_signed(edge_bits)
                else:
                    reader.read_signed(edge_bits)
                straight_edges += 1
            else:
                for _ in range(4):
                    reader.read_signed(edge_bits)
                curved_edges += 1
    reader.align()
    if reader.remaining_bits() != 0:
        raise SwfError("trailing bytes after the shape records")

    return {
        "shape_id": shape_id,
        "bounds": bounds,
        "fill_count": fill_count,
        "fill_colors": fill_colors,
        "has_opaque_fill": any(color[3] == 0xFF for color in fill_colors),
        "line_count": line_count,
        "style_changes": style_changes,
        "straight_edges": straight_edges,
        "curved_edges": curved_edges,
    }


def _validate_place_object_2(body: bytes) -> dict:
    reader = _Reader(body)
    flags = reader.read_u8()
    if flags & 0x01:
        raise SwfError("a PlaceObject2 that moves an existing character is not expected here")
    if flags & 0x80:
        raise SwfError("a PlaceObject2 with clip actions would carry script")
    has_character = bool(flags & 0x02)
    has_matrix = bool(flags & 0x04)
    depth = reader.read_u16()
    character_id = reader.read_u16() if has_character else None
    if has_matrix:
        _parse_matrix(reader)
    if flags & 0x08:
        raise SwfError("a PlaceObject2 colour transform is not expected here")
    if flags & 0x10:
        raise SwfError("a PlaceObject2 ratio is not expected here")
    if flags & 0x20:
        raise SwfError("a PlaceObject2 name is not expected here")
    if flags & 0x40:
        raise SwfError("a PlaceObject2 clip depth is not expected here")
    # A tag is byte-aligned, so only the sub-byte padding of the last field may remain.
    reader.align()
    if reader.remaining_bits() != 0:
        raise SwfError("trailing bytes after the PlaceObject2 fields")
    return {"depth": depth, "character_id": character_id}


def _validate_tag(code: int, body: bytes) -> dict:
    if code == TAG_END:
        if body:
            raise SwfError("the End tag must be empty")
        return {"tag": "End"}
    if code == TAG_SHOW_FRAME:
        if body:
            raise SwfError("the ShowFrame tag must be empty")
        return {"tag": "ShowFrame"}
    if code == TAG_SET_BACKGROUND_COLOR:
        if len(body) != 3:
            raise SwfError("the SetBackgroundColor tag must carry exactly three bytes")
        return {"tag": "SetBackgroundColor", "rgb": tuple(body)}
    if code == TAG_DEFINE_SHAPE_3:
        return {"tag": "DefineShape3", **_validate_define_shape_3(body)}
    if code == TAG_PLACE_OBJECT_2:
        return {"tag": "PlaceObject2", **_validate_place_object_2(body)}
    raise SwfError(f"tag {code} is not one this skin may contain")


def parse_swf(data: bytes) -> dict:
    """Parse and validate a buffer, raising ``SwfError`` on any structural fault."""
    if len(data) < 8:
        raise SwfError("the buffer is shorter than the eight-byte file header")
    if bytes(data[0:3]) != SWF_SIGNATURE:
        raise SwfError("the file is not an uncompressed SWF (expected the FWS signature)")
    version = data[3]
    if version < 1 or version > SWF_VERSION:
        raise SwfError(f"SWF version {version} is outside the supported range 1..{SWF_VERSION}")
    declared_length = struct.unpack_from("<I", data, 4)[0]
    if declared_length != len(data):
        raise SwfError(f"the declared file length {declared_length} does not match the buffer length {len(data)}")

    reader = _Reader(data, 8 * 8)
    frame_bounds, rect_width = _parse_rect(reader)
    frame_rate = reader.read_u16()
    frame_count = reader.read_u16()

    tags = []
    while True:
        if reader.remaining_bits() < 16:
            raise SwfError("the tag stream ended without an End tag")
        tag_and_length = reader.read_u16()
        code = tag_and_length >> 6
        length = tag_and_length & TAG_LONG_FORM
        if length == TAG_LONG_FORM:
            length = reader.read_u32()
        if code not in ALLOWED_TAGS:
            raise SwfError(f"tag {code} is outside the allowed vocabulary {sorted(ALLOWED_TAGS)}")
        body = reader.read_bytes(length)
        summary = _validate_tag(code, body)
        summary["code"] = code
        summary["length"] = length
        tags.append(summary)
        if code == TAG_END:
            break
    if reader.remaining_bits() != 0:
        raise SwfError("there are trailing bytes after the End tag")

    return {
        "version": version,
        "file_length": declared_length,
        "frame_bounds": frame_bounds,
        "frame_rect_width": rect_width,
        "frame_rate": frame_rate,
        "frame_count": frame_count,
        "tags": tags,
    }


# ---------------------------------------------------------------------------
# Command line
# ---------------------------------------------------------------------------


def _default_artifact_path() -> Path:
    return Path(__file__).resolve().parent.parent / ARTIFACT_RELATIVE_PATH


def _describe(summary: dict) -> str:
    tags = ", ".join(f"{tag['tag']}({tag['length']}B)" for tag in summary["tags"])
    shape = next((tag for tag in summary["tags"] if tag["tag"] == "DefineShape3"), None)
    shape_detail = ""
    if shape is not None:
        shape_detail = (
            f"; shape bounds {shape['bounds']}, {shape['fill_count']} solid fill(s), "
            f"opaque backdrop {'yes' if shape['has_opaque_fill'] else 'no'}, "
            f"{shape['line_count']} line style(s), {shape['style_changes']} style change(s), "
            f"{shape['straight_edges']} straight edge(s), {shape['curved_edges']} curved edge(s)"
        )
    return (
        f"SWF {summary['version']}, {summary['file_length']} bytes, frame bounds {summary['frame_bounds']}{shape_detail}\n"
        f"  tags: {tags}"
    )


def _validate_stage(summary: dict) -> None:
    """Assert the parsed header and shape bounds declare exactly the intended stage.

    This is the check the original defect slipped past: the stage RECT's field order
    was wrong, the parser read a zero-width stage, and the structural parse still
    passed because a zero-width span is not an inverted one.  The generated skin has
    exactly one legal geometry - a stage-filling backdrop and frame - so the intended
    dimensions are assertable exactly, in spec field order.
    """
    expected = (0, STAGE_WIDTH_TWIPS, 0, STAGE_HEIGHT_TWIPS)
    if summary["frame_bounds"] != expected:
        raise SwfError(
            f"the header stage RECT is {summary['frame_bounds']} (x_min, x_max, y_min, y_max), expected {expected}"
        )
    shape = next((tag for tag in summary["tags"] if tag["tag"] == "DefineShape3"), None)
    if shape is not None and shape["bounds"] != expected:
        raise SwfError(
            f"the shape bounds RECT is {shape['bounds']} (x_min, x_max, y_min, y_max), expected {expected}"
        )


def _run_selftest(good: bytes) -> None:
    """Prove the parser rejects structural faults rather than accepting anything."""

    def expect_rejected(label: str, payload: bytes) -> None:
        try:
            parse_swf(payload)
        except SwfError:
            return
        raise SwfError(f"the self-test expected {label} to be rejected")

    # A buffer whose signature is not the uncompressed-SWF one.
    expect_rejected("a wrong signature", b"XYZ" + good[3:])

    # A buffer whose declared file length is wrong.
    corrupted = bytearray(good)
    struct.pack_into("<I", corrupted, 4, len(good) + 1)
    expect_rejected("a mismatched file length", bytes(corrupted))

    # A truncated buffer.
    expect_rejected("a truncated buffer", good[:-1])

    # A version beyond the supported ceiling.
    corrupted = bytearray(good)
    corrupted[3] = SWF_VERSION + 1
    expect_rejected("an unsupported version", bytes(corrupted))

    # A file whose tag stream carries a DoAction action tag.
    header = good[:8]
    frame_rect = encode_rect((0, STAGE_WIDTH_TWIPS, 0, STAGE_HEIGHT_TWIPS))
    fixed = frame_rect + struct.pack("<H", int(FRAME_RATE * 256)) + struct.pack("<H", FRAME_COUNT)
    with_action = header + fixed + encode_tag(12, b"\x07\x00") + encode_tag(TAG_END, b"")
    corrupted = bytearray(with_action)
    struct.pack_into("<I", corrupted, 4, len(with_action))
    expect_rejected("a DoAction tag", bytes(corrupted))

    # A file whose tag stream simply ends without an End tag.
    without_end = header + fixed + encode_tag(TAG_SHOW_FRAME, b"")
    corrupted = bytearray(without_end)
    struct.pack_into("<I", corrupted, 4, len(without_end))
    expect_rejected("a missing End tag", bytes(corrupted))

    # A header RECT whose Xmin equals Xmax - a degenerate, zero-width stage, which is
    # what a corner-order RECT mistake produces and what a player then renders as nothing.
    degenerate_stage = (
        header
        + encode_rect((0, 0, 0, STAGE_HEIGHT_TWIPS))
        + struct.pack("<H", int(FRAME_RATE * 256))
        + struct.pack("<H", FRAME_COUNT)
        + encode_tag(TAG_SET_BACKGROUND_COLOR, bytes(BACKGROUND_RGB))
        + encode_tag(TAG_END, b"")
    )
    corrupted = bytearray(degenerate_stage)
    struct.pack_into("<I", corrupted, 4, len(degenerate_stage))
    expect_rejected("a degenerate stage RECT", bytes(corrupted))

    # A shape whose only fill style is a gradient rather than a solid colour.
    body = bytearray()
    body += struct.pack("<H", SHAPE_ID)
    body += encode_rect((0, STAGE_WIDTH_TWIPS, 0, STAGE_HEIGHT_TWIPS))
    body.append(1)  # FillStyleCount
    body.append(0x10)  # a linear gradient fill style
    body += bytes(8)
    body.append(0)  # LineStyleCount
    body += _encode_shape_records(max(STAGE_WIDTH_TWIPS, STAGE_HEIGHT_TWIPS))
    shape_file = header + fixed + encode_tag(TAG_DEFINE_SHAPE_3, bytes(body)) + encode_tag(TAG_END, b"")
    corrupted = bytearray(shape_file)
    struct.pack_into("<I", corrupted, 4, len(shape_file))
    expect_rejected("a gradient fill style", bytes(corrupted))

    # A shape whose only fill is translucent, which would let the world show through the panel.  The
    # check rejects it at the fill styles, before the records are even needed.
    body = bytearray()
    body += struct.pack("<H", SHAPE_ID)
    body += encode_rect((0, STAGE_WIDTH_TWIPS, 0, STAGE_HEIGHT_TWIPS))
    body.append(1)  # FillStyleCount
    body.append(FILL_STYLE_SOLID)
    body += bytes(BACKDROP_RGBA[:3]) + bytes((0x40,))
    shape_file = header + fixed + encode_tag(TAG_DEFINE_SHAPE_3, bytes(body)) + encode_tag(TAG_END, b"")
    corrupted = bytearray(shape_file)
    struct.pack_into("<I", corrupted, 4, len(shape_file))
    expect_rejected("a translucent-only skin", bytes(corrupted))


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Generate and validate the default PlayerPanel skin SWF.")
    parser.add_argument("--output", type=Path, default=None, help="path of the artifact to write")
    parser.add_argument("--verify", action="store_true", help="re-read and re-parse the artifact after writing it")
    parser.add_argument("--check", action="store_true", help="parse the artifact in place without writing it")
    parser.add_argument("--selftest", action="store_true", help="also prove the parser rejects corrupt buffers")
    args = parser.parse_args(argv)

    output = args.output if args.output is not None else _default_artifact_path()

    # Build and validate in memory first, so a structurally broken file is never written.
    try:
        data = build_swf()
        summary = parse_swf(data)
        _validate_stage(summary)
    except SwfError as error:
        print(f"error: the generated SWF is structurally invalid: {error}", file=sys.stderr)
        return 1

    if args.check:
        if not output.exists():
            print(f"error: {output} does not exist", file=sys.stderr)
            return 1
        try:
            on_disk = parse_swf(output.read_bytes())
        except SwfError as error:
            print(f"error: {output} is structurally invalid: {error}", file=sys.stderr)
            return 1
        if output.read_bytes() != data:
            print(f"error: {output} does not match the current generator output", file=sys.stderr)
            return 1
        print(f"checked {output}")
        print(_describe(on_disk))
        return 0

    if args.selftest:
        try:
            _run_selftest(data)
        except SwfError as error:
            print(f"error: the parser self-test failed: {error}", file=sys.stderr)
            return 1
        print("parser self-test: every deliberately corrupt buffer was rejected")

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(data)
    print(f"wrote {output} ({len(data)} bytes)")

    if args.verify:
        try:
            on_disk = parse_swf(output.read_bytes())
        except SwfError as error:
            print(f"error: the written artifact is structurally invalid: {error}", file=sys.stderr)
            return 1
        if output.read_bytes() != data:
            print("error: the written artifact differs from the generated bytes", file=sys.stderr)
            return 1
        print(_describe(on_disk))

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
