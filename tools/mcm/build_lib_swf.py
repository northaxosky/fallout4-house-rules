#!/usr/bin/env python3
"""Build package/frontends/mcm/MCM/Config/HouseRules/lib.swf from a source image.

The SWF contains one bitmap displayed via a DefineShape (bitmap fill) wrapped
in a DefineSprite, with an AS3 class export named "s1". MCM looks up "s1" in
the library by `libName`/`className` reference in config.json.

Pure Python: Pillow is the only runtime dependency. The DoABC bytecode in
tools/mcm/s1_class.abc is a small AS3 class definition for the "s1"
MovieClip symbol; it's the stock 278-byte blob produced by Flash authoring
for "expose this Sprite as class s1" and is byte-identical across every
MCM-image SWF I've inspected.

Default output is a 2:1 banner cropped from the source image, sized so the
SWF stage / DefineShape bounds match the cropped image exactly. This is what
makes MCM allocate the right vertical space for the image entry.
"""

from __future__ import annotations

import argparse
import base64
import struct
import sys
import zlib
from pathlib import Path

try:
    from PIL import Image, ImageOps
except ImportError:
    sys.exit("Pillow is required. Install with: python -m pip install Pillow")


REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_INPUT = Path(__file__).resolve().parent / "house.webp"
DEFAULT_OUTPUT = (
    REPO_ROOT
    / "package"
    / "frontends"
    / "mcm"
    / "MCM"
    / "Config"
    / "HouseRules"
    / "lib.swf"
)
DOABC_BLOB_PATH = Path(__file__).resolve().parent / "s1_class.abc"

SWF_VERSION = 10
TWIPS = 20

TAG_END                   = 0
TAG_SHOW_FRAME            = 1
TAG_SET_BACKGROUND_COLOR  = 9
TAG_DEFINE_SHAPE          = 2
TAG_DEFINE_BITS_LOSSLESS2 = 36
TAG_DEFINE_SPRITE         = 39
TAG_FILE_ATTRIBUTES       = 69
TAG_DOABC                 = 82
TAG_SYMBOL_CLASS          = 76


# ---------- bit writer ----------

class BitWriter:
    """MSB-first bit accumulator."""
    def __init__(self):
        self._acc = 0
        self._nbits = 0
        self._out = bytearray()

    def write_ub(self, value: int, n: int) -> None:
        value &= (1 << n) - 1 if n else 0
        self._acc = (self._acc << n) | value
        self._nbits += n
        while self._nbits >= 8:
            self._nbits -= 8
            self._out.append((self._acc >> self._nbits) & 0xFF)
            self._acc &= (1 << self._nbits) - 1 if self._nbits else 0

    def write_sb(self, value: int, n: int) -> None:
        if value < 0:
            value = (1 << n) + value
        self.write_ub(value, n)

    def write_flag(self, flag: bool) -> None:
        self.write_ub(1 if flag else 0, 1)

    def align(self) -> None:
        if self._nbits:
            self._out.append((self._acc << (8 - self._nbits)) & 0xFF)
            self._acc = 0
            self._nbits = 0

    def bytes(self) -> bytes:
        out = bytearray(self._out)
        if self._nbits:
            out.append((self._acc << (8 - self._nbits)) & 0xFF)
        return bytes(out)


def sb_nbits(value: int) -> int:
    if value == 0:
        return 0
    if value > 0:
        return value.bit_length() + 1
    return (-value - 1).bit_length() + 1


def encode_rect(xmin: int, xmax: int, ymin: int, ymax: int) -> bytes:
    nbits = max(sb_nbits(v) for v in (xmin, xmax, ymin, ymax)) or 1
    bw = BitWriter()
    bw.write_ub(nbits, 5)
    for v in (xmin, xmax, ymin, ymax):
        bw.write_sb(v, nbits)
    return bw.bytes()


# ---------- tag header ----------

def encode_tag(tag_type: int, body: bytes) -> bytes:
    if len(body) < 0x3f:
        header = struct.pack("<H", (tag_type << 6) | len(body))
    else:
        header = struct.pack("<HI", (tag_type << 6) | 0x3f, len(body))
    return header + body


# ---------- bitmap encoding (DefineBitsLossless2 format 5: 32-bit ARGB premultiplied) ----------

def pack_pixels_argb_premultiplied(image: Image.Image) -> bytes:
    rgba = image.convert("RGBA")
    r, g, b, a = rgba.split()
    extrema = a.getextrema()
    if extrema == (255, 255):
        return Image.merge("RGBA", (a, r, g, b)).tobytes()
    src = bytearray(rgba.tobytes())
    n = len(src) // 4
    out = bytearray(n * 4)
    for i in range(n):
        rr = src[i * 4 + 0]; gg = src[i * 4 + 1]; bb = src[i * 4 + 2]; aa = src[i * 4 + 3]
        out[i * 4 + 0] = aa
        out[i * 4 + 1] = (rr * aa) // 255
        out[i * 4 + 2] = (gg * aa) // 255
        out[i * 4 + 3] = (bb * aa) // 255
    return bytes(out)


def make_define_bits_lossless2(char_id: int, image: Image.Image) -> bytes:
    w, h = image.size
    pixels = pack_pixels_argb_premultiplied(image)
    compressed = zlib.compress(pixels, level=9)
    body = struct.pack("<HBHH", char_id, 5, w, h) + compressed
    return encode_tag(TAG_DEFINE_BITS_LOSSLESS2, body)


# ---------- DefineShape with bitmap-fill rectangle ----------

def make_define_shape_bitmap_rect(shape_id: int, bitmap_id: int,
                                   width_px: int, height_px: int) -> bytes:
    """Shape that fills a rectangle (0,0)-(W,H) twips with bitmap_id, scaled
    so 1 bitmap pixel == 1 shape pixel. Used for MCM banner display."""
    width_t  = width_px * TWIPS
    height_t = height_px * TWIPS

    body = bytearray()
    body += struct.pack("<H", shape_id)
    body += encode_rect(0, width_t, 0, height_t)

    # ShapeWithStyle ----------------------------------------------------------
    body.append(1)  # FillStyleCount = 1

    # FillStyle: type 0x40 (clipped bitmap), bitmap id, matrix
    body.append(0x40)
    body += struct.pack("<H", bitmap_id)

    # Matrix: scale = (TWIPS, TWIPS), no rotation, translate = (0, 0).
    # Scale is 16.16 fixed: TWIPS * 65536.
    scale_fp = TWIPS * 65536
    mbw = BitWriter()
    mbw.write_flag(True)                       # HasScale
    nbits_scale = sb_nbits(scale_fp)
    mbw.write_ub(nbits_scale, 5)
    mbw.write_sb(scale_fp, nbits_scale)        # ScaleX
    mbw.write_sb(scale_fp, nbits_scale)        # ScaleY
    mbw.write_flag(False)                      # HasRotate
    mbw.write_ub(0, 5)                         # NTranslateBits = 0 (translate omitted)
    body += mbw.bytes()

    body.append(0)  # LineStyleCount = 0

    # NumFillBits = 1, NumLineBits = 0  -> 0x10
    body.append(0x10)

    # Shape records ----------------------------------------------------------
    # 1. StyleChange: set FillStyle0 -> 1 (no move, start path at origin)
    # 2. Four StraightEdge records drawing the rectangle outline
    # 3. EndShapeRecord (TypeFlag=0, all flags=0)
    sw = BitWriter()

    # StyleChange
    sw.write_ub(0, 1)        # TypeFlag = 0 (non-edge)
    sw.write_ub(0, 1)        # StateNewStyles
    sw.write_ub(0, 1)        # StateLineStyle
    sw.write_ub(0, 1)        # StateFillStyle1
    sw.write_ub(1, 1)        # StateFillStyle0  <- only flag set
    sw.write_ub(0, 1)        # StateMoveTo
    sw.write_ub(1, 1)        # FillStyle0 = 1 (NumFillBits=1)

    def emit_straight(dx_twips: int, dy_twips: int) -> None:
        sw.write_ub(1, 1)    # TypeFlag = 1 (edge)
        sw.write_ub(1, 1)    # StraightFlag = 1 (straight, not curve)

        nbits = max(sb_nbits(dx_twips), sb_nbits(dy_twips), 2)
        sw.write_ub(nbits - 2, 4)  # NumBits encoded as (real - 2)
        if dx_twips != 0 and dy_twips != 0:
            sw.write_ub(1, 1)  # GeneralLineFlag = 1
            sw.write_sb(dx_twips, nbits)
            sw.write_sb(dy_twips, nbits)
        else:
            sw.write_ub(0, 1)  # GeneralLineFlag = 0
            if dy_twips != 0:
                sw.write_ub(1, 1)  # VertLineFlag = 1
                sw.write_sb(dy_twips, nbits)
            else:
                sw.write_ub(0, 1)  # VertLineFlag = 0 (horizontal)
                sw.write_sb(dx_twips, nbits)

    emit_straight( width_t,         0)
    emit_straight(        0,  height_t)
    emit_straight(-width_t,         0)
    emit_straight(        0, -height_t)

    # EndShapeRecord: TypeFlag=0 + 5 zero flags = 6 zero bits
    sw.write_ub(0, 6)

    body += sw.bytes()
    return encode_tag(TAG_DEFINE_SHAPE, bytes(body))


# ---------- DefineSprite: 1 frame containing one PlaceObject2 of the shape ----------

def make_define_sprite(sprite_id: int, shape_id: int) -> bytes:
    body = bytearray()
    body += struct.pack("<HH", sprite_id, 1)  # sprite_id, FrameCount = 1

    # PlaceObject2 (tag 26): place shape_id at depth 1, no name, no matrix.
    # Flags byte: only HasCharacter (bit 1) set -> 0x02.
    po2_body = bytes([0x02])                              # PlaceFlags
    po2_body += struct.pack("<H", 1)                      # Depth = 1
    po2_body += struct.pack("<H", shape_id)               # CharacterID
    body += encode_tag(26, po2_body)

    # ShowFrame
    body += encode_tag(TAG_SHOW_FRAME, b"")
    # End sprite tag list
    body += encode_tag(TAG_END, b"")

    return encode_tag(TAG_DEFINE_SPRITE, bytes(body))


# ---------- SymbolClass ----------

def make_symbol_class(symbols: list[tuple[int, str]]) -> bytes:
    body = struct.pack("<H", len(symbols))
    for char_id, name in symbols:
        body += struct.pack("<H", char_id) + name.encode("ascii") + b"\x00"
    return encode_tag(TAG_SYMBOL_CLASS, body)


# ---------- Other small tags ----------

def make_file_attributes(uses_as3: bool = True) -> bytes:
    flags = 0x08 if uses_as3 else 0
    return encode_tag(TAG_FILE_ATTRIBUTES, struct.pack("<I", flags))


def make_set_background_color(rgb: tuple[int, int, int]) -> bytes:
    return encode_tag(TAG_SET_BACKGROUND_COLOR, bytes(rgb))


# ---------- DoABC (copy of SCM's class definition for symbol "s1") ----------

_DOABC_FALLBACK_B64 = ""  # populated below if blob present


def load_doabc_blob() -> bytes:
    if DOABC_BLOB_PATH.is_file():
        return DOABC_BLOB_PATH.read_bytes()
    if _DOABC_FALLBACK_B64:
        return base64.b64decode(_DOABC_FALLBACK_B64)
    sys.exit(
        f"DoABC blob not found at {DOABC_BLOB_PATH}. "
        "Should be the 278-byte 's1' class blob shipped alongside this script."
    )


def make_doabc(body: bytes) -> bytes:
    return encode_tag(TAG_DOABC, body)


# ---------- SWF assembly ----------

def build_swf(image: Image.Image) -> bytes:
    w, h = image.size
    body = bytearray()
    body += encode_rect(0, w * TWIPS, 0, h * TWIPS)  # FrameSize
    body += struct.pack("<H", 24 << 8)               # FrameRate (24.0 fps, 8.8 fixed)
    body += struct.pack("<H", 1)                     # FrameCount = 1

    body += make_file_attributes(uses_as3=True)
    body += make_set_background_color((0, 0, 0))
    body += make_define_bits_lossless2(char_id=3, image=image)
    body += make_define_shape_bitmap_rect(shape_id=1, bitmap_id=3, width_px=w, height_px=h)
    body += make_define_sprite(sprite_id=2, shape_id=1)
    body += make_doabc(load_doabc_blob())
    body += make_symbol_class([(2, "s1")])           # sprite (id=2) <- "s1"
    body += encode_tag(TAG_SHOW_FRAME, b"")
    body += encode_tag(TAG_END, b"")

    file_length = 8 + len(body)
    header = b"FWS" + bytes([SWF_VERSION]) + struct.pack("<I", file_length)
    return header + bytes(body)


# ---------- main: image prep ----------

def crop_to_aspect(image: Image.Image, aspect: float) -> Image.Image:
    """Center-crop the image so its width:height matches `aspect` (W/H)."""
    src_aspect = image.width / image.height
    if abs(src_aspect - aspect) < 1e-3:
        return image
    if src_aspect > aspect:
        # source too wide; crop width
        new_w = int(round(image.height * aspect))
        x0 = (image.width - new_w) // 2
        return image.crop((x0, 0, x0 + new_w, image.height))
    new_h = int(round(image.width / aspect))
    y0 = (image.height - new_h) // 2
    return image.crop((0, y0, image.width, y0 + new_h))


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--input", default=str(DEFAULT_INPUT))
    p.add_argument("--output", default=str(DEFAULT_OUTPUT))
    p.add_argument("--aspect", type=float, default=2.0,
                   help="Target W:H aspect ratio after center-crop (default 2.0 = wide banner).")
    p.add_argument("--max-width", type=int, default=720,
                   help="Downscale to this max width; 0 disables resizing.")
    args = p.parse_args()

    src = Path(args.input)
    if not src.is_file():
        print(f"input not found: {src}", file=sys.stderr)
        return 1

    img = Image.open(src)
    print(f"loaded {src.name}: {img.width}x{img.height} mode={img.mode}")

    if args.aspect and args.aspect > 0:
        img = crop_to_aspect(img, args.aspect)
        print(f"cropped to aspect {args.aspect}: {img.width}x{img.height}")

    if args.max_width > 0 and img.width > args.max_width:
        scale = args.max_width / img.width
        img = img.resize((args.max_width, int(round(img.height * scale))), Image.LANCZOS)
        print(f"resized to {img.width}x{img.height}")

    swf = build_swf(img)

    out = Path(args.output)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(swf)
    print(f"wrote {out}: {len(swf):,} bytes (stage {img.width}x{img.height}, class 's1')")
    return 0


if __name__ == "__main__":
    sys.exit(main())
