#!/usr/bin/env python3
"""
sprite-packer.py

Repack edited sprite PNGs (from the extractor) and their palettes
back into gadgets.dat for Premier Manager 3.

Algorithms for sprite layout, interleaving, and palette format are
kept identical to the original working script; this version only
adds a friendlier CLI and some safety features.
"""

import argparse
import json
import struct
from pathlib import Path
from PIL import Image
import re

DEFAULT_DAT_NAME = "gadgets.dat"
DEFAULT_OFF_NAME = "gadgets.off"
DEFAULT_OUT_NAME = "gadgets_patched.dat"
DEFAULT_SPRITE_DIR = "sprites_color_auto"
DEFAULT_PALETTE_MAP = "palette-map.json"

# sprite_012__pal_034.png  (from the extractor)
FILENAME_RE = re.compile(r"^sprite_(\d{3})__pal_(\d{3})\.(png|ppm)$", re.IGNORECASE)

INTERLEAVE = 4  # must match the extractor


def load_offsets(path: Path):
    """Load <offset,uint32> <length,uint32> pairs from gadgets.off."""
    data = path.read_bytes()
    if len(data) % 8 != 0:
        raise ValueError(f"{path} length is not a multiple of 8.")
    entries = []
    for i in range(0, len(data), 8):
        off, length = struct.unpack_from("<II", data, i)
        entries.append((off, length))
    return entries


def read_sprite_header_strict(dat: bytes, offset: int, length: int):
    """
    Read sprite header (w,h) and enforce that chunk length matches w*h + 4.

    Format:
        uint16 width
        uint16 height
        width*height bytes of pixel indices
    """
    if length < 4:
        raise ValueError(f"Chunk too small ({length}) for sprite header.")
    w, h = struct.unpack_from("<HH", dat, offset)
    expected_pixels = w * h
    actual_pixels = max(0, length - 4)
    if expected_pixels != actual_pixels:
        raise ValueError(
            f"Sprite size mismatch at offset {offset}: "
            f"header {w}x{h}={expected_pixels} pixels, "
            f"chunk has {actual_pixels} bytes"
        )
    return w, h


def interleave_4(w: int, h: int, linear_pixels: bytes) -> bytes:
    """
    Inverse of deinterleave_4 from the extractor.

    The extractor:
      - splits the original (interleaved) data into 4 streams with:
            total = w*h
            per = ceil(total/4)
            stream[lane] = data[lane*per : min((lane+1)*per, total)]
      - then fills the output scanline-wise, using lane = x % 4.

    Here we do the reverse:
      - compute the stream lengths
      - fill those streams from the linear scanline pixels
      - concatenate streams back into the interleaved byte sequence.
    """
    total = w * h
    if len(linear_pixels) != total:
        raise ValueError(
            f"interleave_4: got {len(linear_pixels)} pixels, expected {total}"
        )

    per = (total + INTERLEAVE - 1) // INTERLEAVE

    # same slicing scheme as the extractor
    stream_lens = []
    for lane in range(INTERLEAVE):
        start = lane * per
        remaining = max(0, total - start)
        stream_len = min(per, remaining)
        stream_lens.append(stream_len)

    streams = [bytearray(l) for l in stream_lens]
    idx = [0] * INTERLEAVE

    for y in range(h):
        base = y * w
        for x in range(w):
            lane = x % INTERLEAVE
            streams[lane][idx[lane]] = linear_pixels[base + x]
            idx[lane] += 1

    return b"".join(streams)


def bytes_8bit_to_6bit_palette(pal8: bytes, ncolors: int) -> bytes:
    """
    Convert an 8-bit RGB palette (pal8) to a 6-bit RGB palette with exactly
    ncolors entries (ncolors * 3 bytes).
    - If pal8 is longer, truncate.
    - If pal8 is shorter, pad with zeros.
    """
    needed = ncolors * 3

    if len(pal8) < needed:
        pal8 = pal8 + bytes(needed - len(pal8))
    elif len(pal8) > needed:
        pal8 = pal8[:needed]

    # Scale 0-255 down to 0-63 (6-bit) with rounding
    pal6 = bytes(int(round(v * 63 / 255)) for v in pal8)
    return pal6


def parse_sprite_pal_from_name(name: str):
    """
    Parse sprite and palette indices from filename:
      sprite_XXX__pal_YYY.png
    Returns (sprite_index, palette_index) as ints or None if not matched.
    """
    m = FILENAME_RE.match(name)
    if not m:
        return None
    s_idx = int(m.group(1))
    p_idx = int(m.group(2))  # may be overridden by palette map
    return s_idx, p_idx


def parse_sprite_subset(spec: str):
    """
    Parse a sprite subset specification like:
        "0-10,15,20-25"

    Returns a set of sprite indices (ints). No max bound is enforced here;
    indices just need to be non-negative. If spec is empty/None, returns None.
    """
    if not spec:
        return None

    selected = set()
    parts = spec.split(",")
    for part in parts:
        part = part.strip()
        if not part:
            continue
        if "-" in part:
            start_s, end_s = part.split("-", 1)
            try:
                start = int(start_s)
                end = int(end_s)
            except ValueError:
                raise ValueError(f"Invalid range in --sprites: {part!r}")
            if start > end:
                start, end = end, start
            for i in range(start, end + 1):
                if i >= 0:
                    selected.add(i)
        else:
            try:
                i = int(part)
            except ValueError:
                raise ValueError(f"Invalid index in --sprites: {part!r}")
            if i >= 0:
                selected.add(i)

    return selected or None




def load_sprite_to_palette_map(path: Path):
    """Load sprite->palette mapping from JSON.

    JSON format:
      - Keys: sprite chunk indices (strings or integers)
      - Values:
          * int        -> palette chunk index
          * "unknown"  -> explicitly unknown / skip
          * "palette"  -> marks a palette chunk itself (not a sprite)

    Returns:
        dict[int, int|None] where None means skip.
    """
    raw = json.loads(path.read_text())
    if not isinstance(raw, dict):
        raise ValueError("palette map must be a JSON object")

    mapping = {}
    for k, v in raw.items():
        try:
            sk = int(k)
        except Exception:
            raise ValueError(f"Invalid sprite id key in palette map: {k!r}")

        if isinstance(v, int):
            if sk < 0 or v < 0:
                raise ValueError(f"Negative ids not allowed in palette map: {k!r}:{v!r}")
            mapping[sk] = v
        elif v == "unknown" or v == "palette":
            mapping[sk] = None
        else:
            raise ValueError(f"Invalid palette map value for {k!r}: {v!r}")
    return mapping
def resolve_in_pm3(pm3_dir: Path, name_or_path: str) -> Path:
    """
    If 'name_or_path' is absolute, return it as a Path.
    Otherwise, resolve it relative to pm3_dir.
    """
    p = Path(name_or_path)
    if p.is_absolute():
        return p
    return pm3_dir / p


def parse_args():
    parser = argparse.ArgumentParser(
        description=(
            "Repack edited sprite PNGs (sprite_XXX__pal_YYY.png) and their "
            "6-bit palettes back into gadgets.dat."
        )
    )
    parser.add_argument(
        "pm3_dir",
        type=Path,
        help="Directory containing gadgets.dat and gadgets.off (or variants).",
    )
    parser.add_argument(
        "--dat-name",
        default=DEFAULT_DAT_NAME,
        help=f"DAT file name or path (default: {DEFAULT_DAT_NAME} in pm3_dir).",
    )
    parser.add_argument(
        "--off-name",
        default=DEFAULT_OFF_NAME,
        help=f"OFF file name or path (default: {DEFAULT_OFF_NAME} in pm3_dir).",
    )
    parser.add_argument(
        "--out-name",
        default=DEFAULT_OUT_NAME,
        help=(
            "Output DAT file name or path (default: "
            f"{DEFAULT_OUT_NAME} in pm3_dir)."
        ),
    )
    parser.add_argument(
        "--sprites-dir",
        default=DEFAULT_SPRITE_DIR,
        help=(
            "Directory containing sprite_XXX__pal_YYY.png files "
            f"(default: {DEFAULT_SPRITE_DIR}, relative to current working dir)."
        ),
    )
    parser.add_argument(
        "--interleave",
        type=int,
        default=INTERLEAVE,
        help="Horizontal interleave factor (must match extractor; default: 4).",
    )
    parser.add_argument(
        "--sprites",
        help=(
            "Optional subset of sprite indices to apply, as a comma-separated "
            'list of indices and/or ranges, e.g. "0-20,35,40-45". '
            "Indices are 0-based and correspond to the XXX part of "
            "sprite_XXX__pal_YYY.png. If omitted, all matching sprite files "
            "in --sprites-dir are applied."
        ),
    )
    parser.add_argument(
        "--palette-map",
        default=DEFAULT_PALETTE_MAP,
        help=(
            "Path to palette-map.json (sprite->palette). "
            "The repacker will use this mapping for palette selection "
            "instead of the pal id embedded in filenames."
        ),
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Do everything except actually write the patched DAT file.",
    )
    return parser.parse_args()


def main():
    global INTERLEAVE

    args = parse_args()
    INTERLEAVE = args.interleave

    pm3_dir: Path = args.pm3_dir
    if not pm3_dir.is_dir():
        raise NotADirectoryError(f"PM3 directory not found: {pm3_dir}")

    off_path = resolve_in_pm3(pm3_dir, args.off_name)
    dat_path = resolve_in_pm3(pm3_dir, args.dat_name)
    out_path = resolve_in_pm3(pm3_dir, args.out_name)

    sprite_dir = Path(args.sprites_dir)
    if not sprite_dir.is_absolute():
        sprite_dir = sprite_dir.resolve()

    palette_map_path = Path(args.palette_map)
    if not palette_map_path.is_absolute():
        palette_map_path = palette_map_path.resolve()
    if not palette_map_path.exists():
        raise FileNotFoundError(f"Palette map not found: {palette_map_path}")
    sprite_to_palette = load_sprite_to_palette_map(palette_map_path)

    print(f"PM3 directory : {pm3_dir}")
    print(f"Palette map  : {palette_map_path}")
    print(f"DAT file      : {dat_path}")
    print(f"OFF file      : {off_path}")
    print(f"Output DAT    : {out_path}")
    print(f"Sprites dir   : {sprite_dir}")
    print(f"Interleave    : {INTERLEAVE}")
    if args.sprites:
        print(f"Sprite subset : {args.sprites}")
    print(f"Dry run       : {args.dry_run}")

    if not off_path.exists():
        raise FileNotFoundError(f"{off_path} not found.")
    if not dat_path.exists():
        raise FileNotFoundError(f"{dat_path} not found.")
    if not sprite_dir.exists():
        raise FileNotFoundError(f"{sprite_dir} not found.")

    offsets = load_offsets(off_path)
    total_chunks = len(offsets)
    print(f"Loaded {total_chunks} chunks from {off_path}")

    dat = bytearray(dat_path.read_bytes())
    print(f"Loaded {len(dat)} bytes from {dat_path}")

    # Sprite subset, if any
    sprite_subset = parse_sprite_subset(args.sprites) if args.sprites else None
    if sprite_subset:
        print(f"Applying only sprites: {sorted(sprite_subset)}")

    # Collect PNG files with the expected naming convention
    img_files = sorted(
        [
            p
            for p in sprite_dir.iterdir()
            if p.is_file() and p.suffix.lower() in (".png", ".ppm")
        ],
        key=lambda p: p.name.lower(),
    )

    if not img_files:
        print("No sprite image files found in sprites directory; nothing to do.")
        return

    updates = 0
    palette_updates = {}

    for png_path in img_files:
        parsed = parse_sprite_pal_from_name(png_path.name)
        if not parsed:
            # Ignore unrelated PNGs
            continue

        sprite_idx, pal_idx_from_name = parsed

        pal_idx = sprite_to_palette.get(sprite_idx)
        if pal_idx is None:
            print(
                f"Skipping {png_path.name}: sprite {sprite_idx} has no palette mapping (unknown/palette/unmapped)"
            )
            continue

        if pal_idx_from_name != pal_idx:
            print(
                f"NOTE {png_path.name}: filename palette {pal_idx_from_name} overridden by palette map -> {pal_idx}"
            )

        # Respect sprite subset
        if sprite_subset is not None and sprite_idx not in sprite_subset:
            continue

        if sprite_idx < 0 or sprite_idx >= total_chunks:
            print(f"Skipping {png_path.name}: sprite index {sprite_idx} out of range")
            continue
        if pal_idx < 0 or pal_idx >= total_chunks:
            print(f"Skipping {png_path.name}: palette index {pal_idx} out of range")
            continue

        sprite_off, sprite_len = offsets[sprite_idx]
        pal_off, pal_len = offsets[pal_idx]

        # ---- Verify sprite shape matches original ----
        try:
            w_orig, h_orig = read_sprite_header_strict(dat, sprite_off, sprite_len)
        except ValueError as e:
            print(f"{png_path.name}: sprite chunk {sprite_idx} not valid sprite: {e}")
            continue

        # Load PNG as paletted image
        img = Image.open(png_path)
        if img.mode != "P":
            img = img.convert("P")

        w_png, h_png = img.size
        if (w_png, h_png) != (w_orig, h_orig):
            print(
                f"{png_path.name}: size mismatch, PNG {w_png}x{h_png} "
                f"!= original {w_orig}x{h_orig}; skipping"
            )
            continue

        # ---- Replace sprite pixel data ----
        linear_pixels = img.tobytes()
        if len(linear_pixels) != w_orig * h_orig:
            print(
                f"{png_path.name}: pixel data length {len(linear_pixels)} "
                f"!= {w_orig*h_orig}; skipping"
            )
            continue

        # Re-apply the original 4-way interleaving
        interleaved_pixels = interleave_4(w_orig, h_orig, linear_pixels)

        # Sprite format: [uint16 w][uint16 h][w*h bytes indices]
        sprite_data_start = sprite_off + 4
        sprite_data_end = sprite_data_start + len(interleaved_pixels)
        dat[sprite_data_start:sprite_data_end] = interleaved_pixels

        # ---- Replace palette data (6-bit) ----
        # Original palette chunk is assumed:
        #   [4-byte header][palette payload of N*3 bytes]
        if pal_len < 4:
            print(
                f"{png_path.name}: palette chunk {pal_idx} too small ({pal_len}); skipping palette"
            )
        else:
            pal_header = dat[pal_off : pal_off + 4]
            old_payload = dat[pal_off + 4 : pal_off + pal_len]
            if len(old_payload) % 3 != 0:
                print(
                    f"{png_path.name}: palette chunk {pal_idx} payload not multiple of 3; "
                    f"len={len(old_payload)}; skipping palette"
                )
            else:
                ncolors = len(old_payload) // 3
                pal8_list = img.getpalette()
                if pal8_list is None:
                    print(
                        f"{png_path.name}: no palette in PNG; skipping palette update"
                    )
                else:
                    pal8_bytes = bytes(pal8_list)
                    pal6_bytes = bytes_8bit_to_6bit_palette(pal8_bytes, ncolors)

                    if len(pal6_bytes) != len(old_payload):
                        print(
                            f"{png_path.name}: internal error, new pal length {len(pal6_bytes)} "
                            f"!= old {len(old_payload)}; skipping palette"
                        )
                    else:
                        new_chunk = pal_header + pal6_bytes
                        if len(new_chunk) != pal_len:
                            print(
                                f"{png_path.name}: internal error, new chunk len {len(new_chunk)} "
                                f"!= original {pal_len}; skipping palette"
                            )
                        else:
                            dat[pal_off : pal_off + pal_len] = new_chunk
                            palette_updates[pal_idx] = png_path.name

        updates += 1
        print(
            f"Updated sprite chunk {sprite_idx} and palette chunk {pal_idx} "
            f"from {png_path.name}"
        )

    if updates == 0:
        print("No matching sprite_XXX__pal_YYY.png files were applied.")
    else:
        print(f"\nTotal updated sprites: {updates}")
        print(f"Palettes touched: {sorted(palette_updates.keys())}")

    if args.dry_run:
        print("\nDry run enabled; not writing patched DAT file.")
        return

    # Write patched DAT
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(dat)
    print(f"\nWrote patched data to {out_path}")


if __name__ == "__main__":
    main()
