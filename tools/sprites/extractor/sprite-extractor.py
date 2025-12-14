import argparse
import struct
from pathlib import Path
from PIL import Image

INTERLEAVE = 4  # from working sprite extractor


def load_offsets(path: Path):
    """Load <offset,uint32> <length,uint32> pairs from gadgets.off."""
    data = path.read_bytes()
    if len(data) % 8 != 0:
        raise ValueError("gadgets.off length is not a multiple of 8.")
    entries = []
    for i in range(0, len(data), 8):
        off, length = struct.unpack_from("<II", data, i)
        entries.append((off, length))
    return entries


def read_sprite_strict(dat: bytes, offset: int, length: int):
    """Parse sprite; require that chunk length exactly matches header size.

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
            f"Sprite size mismatch: header {w}x{h}={expected_pixels} pixels, "
            f"chunk has {actual_pixels} bytes"
        )
    px = dat[offset + 4 : offset + 4 + expected_pixels]
    return w, h, px


def deinterleave_4(w: int, h: int, data: bytes):
    """Correct 4-way horizontal interleave (same algorithm as working script)."""
    total = w * h
    per = (total + INTERLEAVE - 1) // INTERLEAVE

    streams = []
    for lane in range(INTERLEAVE):
        start = lane * per
        end = min(start + per, total)
        streams.append(data[start:end])

    out = bytearray(total)
    idx = [0] * INTERLEAVE

    for y in range(h):
        base = y * w
        for x in range(w):
            lane = x % INTERLEAVE
            out[base + x] = streams[lane][idx[lane]]
            idx[lane] += 1

    return out


def is_valid_6bit_palette(payload: bytes) -> bool:
    """Return True if payload looks like a 6-bit RGB palette.

    Requirements:
        - length is multiple of 3
        - 1 <= colors <= 256
        - all bytes <= 63
    """
    nbytes = len(payload)
    if nbytes == 0 or nbytes % 3 != 0:
        return False
    n_colors = nbytes // 3
    if n_colors < 1 or n_colors > 256:
        return False
    return all(0 <= b <= 63 for b in payload)


def palette_6bit_to_8bit(payload: bytes):
    """Scale 6-bit RGB palette bytes (0-63) to 8-bit (0-255)."""
    return [int(b * 255 / 63) for b in payload]


def parse_sprite_subset(spec: str, max_index: int) -> set[int]:
    """
    Parse a sprite subset specification like:
        "0-10,15,20-25"

    Indices are 0-based and refer to the chunk index / numeric part
    of sprite_### in the filenames.
    """
    selected = set()
    if not spec:
        return selected





def load_sprite_to_palette_map(path: Path):
    """Load sprite->palette mapping from JSON.

    Expected JSON format:
      - Keys are sprite chunk indices (strings or integers)
      - Values are either:
          * int         -> palette chunk index
          * "unknown"   -> explicitly unknown / skip
          * "palette"   -> marks this chunk as a palette bin (not a sprite)

    Example:
        {
          "211": 223,
          "212": "unknown",
          "223": "palette"
        }

    Returns:
        dict[int, int|None] where None means "skip / unmapped / not a sprite".
    """
    import json

    raw = json.loads(path.read_text())
    if not isinstance(raw, dict):
        raise ValueError("palette map must be a JSON object")

    mapping: dict[int, int | None] = {}
    for k, v in raw.items():
        try:
            sk = int(k)
        except Exception:
            raise ValueError(f"Invalid sprite id key in palette map: {k!r}")

        if isinstance(v, int):
            if v < 0 or sk < 0:
                raise ValueError(f"Negative ids are not allowed in palette map: {k!r}:{v!r}")
            mapping[sk] = v
        elif v == "unknown" or v == "palette":
            mapping[sk] = None
        else:
            raise ValueError(f"Invalid palette map value for sprite {k!r}: {v!r}")

    return mapping

def parse_args():
    parser = argparse.ArgumentParser(
        description=(
            "Extract sprites from gadgets.dat/gadgets.off using 6-bit palettes, "
            "fix 4-way interleave, and write PNG/PPM images."
        )
    )
    parser.add_argument(
        "pm3_dir",
        type=Path,
        help="Directory containing gadgets.dat and gadgets.off",
    )
    parser.add_argument(
        "--dat-name",
        default="gadgets.dat",
        help="Name of DAT file inside pm3_dir (default: gadgets.dat)",
    )
    parser.add_argument(
        "--off-name",
        default="gadgets.off",
        help="Name of OFF file inside pm3_dir (default: gadgets.off)",
    )
    parser.add_argument(
        "-o",
        "--output-dir",
        type=Path,
        default=Path("sprites_color_auto"),
        help="Directory to write output images (default: sprites_color_auto)",
    )
    parser.add_argument(
        "--interleave",
        type=int,
        default=INTERLEAVE,
        help="Horizontal interleave factor (default: 4)",
    )
    parser.add_argument(
        "--sprites",
        help=(
            "Comma-separated list of sprite indices or ranges to extract, "
            'e.g. "0-20,35,40-45". '
            "Indices are 0-based and correspond to the numeric part of "
            "sprite_### in the filenames / log output. If omitted, all "
            "detected sprites are extracted."
        ),
    )

    parser.add_argument(
        "--palette-map",
        default="palette_map.json",
        help="Path to palette-map.json mapping sprite chunk index -> palette chunk index (default: palette-map.json)",
    )
    return parser.parse_args()


def main():
    global INTERLEAVE

    args = parse_args()
    INTERLEAVE = args.interleave

    pm3_dir: Path = args.pm3_dir
    dat_path = pm3_dir / args.dat_name
    off_path = pm3_dir / args.off_name
    output_dir: Path = args.output_dir

    output_dir.mkdir(parents=True, exist_ok=True)

    print(f"PM3 directory : {pm3_dir}")
    print(f"DAT file      : {dat_path}")
    print(f"OFF file      : {off_path}")
    print(f"Output dir    : {output_dir}")
    print(f"Palette map   : {args.palette_map}")
    print(f"Interleave    : {INTERLEAVE}")

    print("Loading data...")
    dat = dat_path.read_bytes()
    offsets = load_offsets(off_path)
    count = len(offsets)
    print(f"Found {count} chunks in {off_path}.")

    # Sprite subset
    sprite_subset = None
    if args.sprites:
        sprite_subset = parse_sprite_subset(args.sprites, count)
        if not sprite_subset:
            print("WARNING: --sprites specified but parsed as empty; nothing to do.")
        else:
            print(f"Restricting to sprite indices: {sorted(sprite_subset)}")

    # Pre-classify entries as sprite or bin, and keep bin payloads
    entries = []
    for idx, (off, length) in enumerate(offsets):
        chunk = dat[off : off + length]
        try:
            w, h, px = read_sprite_strict(dat, off, length)
            entries.append(
                {
                    "type": "sprite",
                    "offset": off,
                    "length": length,
                    "w": w,
                    "h": h,
                    "px": px,
                }
            )
        except ValueError:
            # Non-sprite chunk: treat as generic bin.
            payload = chunk[4:] if len(chunk) > 4 else b""
            entries.append(
                {
                    "type": "bin",
                    "offset": off,
                    "length": length,
                    "payload": payload,
                }
            )

    print("Classification complete.")

    sprite_to_palette = load_sprite_to_palette_map(Path(args.palette_map))


    # Cache palettes we've already converted by bin index
    palette_cache = {}

    for idx, entry in enumerate(entries):
        # Respect sprite subset if provided (indices are chunk indices)
        if sprite_subset is not None and idx not in sprite_subset:
            continue

        if entry["type"] != "sprite":
            continue

        w, h, px = entry["w"], entry["h"], entry["px"]
        # Palette selection is explicit (no auto-detection).
        palette_index = sprite_to_palette.get(idx)
        if palette_index is None:
            print(f"[{idx:03d}] WARNING: no palette mapping for sprite; skipping")
            continue
        if palette_index < 0 or palette_index >= count:
            print(
                f"[{idx:03d}] ERROR: mapped palette chunk {palette_index} out of range; skipping"
            )
            continue

        pal_entry = entries[palette_index]
        if pal_entry["type"] != "bin":
            print(
                f"[{idx:03d}] ERROR: mapped palette chunk {palette_index} is not a bin; skipping"
            )
            continue

        pal_payload = pal_entry["payload"]
        if not is_valid_6bit_palette(pal_payload):
            print(
                f"[{idx:03d}] ERROR: mapped palette chunk {palette_index} is not a valid 6-bit palette; skipping"
            )
            continue
        # Get or compute 8-bit palette for that bin index
        if palette_index in palette_cache:
            pal8 = palette_cache[palette_index]
        else:
            pal8 = palette_6bit_to_8bit(pal_payload)
            palette_cache[palette_index] = pal8

        try:
            fixed = deinterleave_4(w, h, px)
            img = Image.frombytes("P", (w, h), bytes(fixed))
            img.putpalette(pal8)

            base_name = f"sprite_{idx:03d}__pal_{palette_index:03d}"

            outfile_png = output_dir / f"{base_name}.png"
            img.save(outfile_png)
            print(
                f"[{idx:03d}] {w}x{h} using palette from chunk "
                f"{palette_index:03d} -> {outfile_png}"
            )

        except Exception as e:
            print(
                f"[{idx:03d}] ERROR creating image with palette "
                f"{palette_index:03d}: {e}"
            )

    print("Done.")


if __name__ == "__main__":
    main()
