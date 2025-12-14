# PM3 Sprite Repacker

Repack edited sprite PNGs (and their palettes) back into **Premier Manager 3**
`gadgets.dat`, using the same layout and palette format as the extractor.

This is the "opposite" of the sprite extractor: it takes
`sprite_XXX__pal_YYY.png` files and writes a patched DAT file.

---

## Requirements

- Python 3.8+
- Pillow (PIL fork)

```bash
pip install pillow
```

---

## Relationship to the Extractor

- The **extractor** reads `gadgets.dat`/`gadgets.off` and writes paletted PNGs:
  - `sprite_012__pal_034.png`
- The **repacker** reads those PNGs, and writes a new DAT:
  - It verifies width/height against the original sprite chunk.
  - It converts the PNG's 8-bit palette back to 6-bit.
  - It mirrors the original 4-way horizontal interleave.

You typically use them together:

1. Run extractor to dump sprites.
2. Edit the PNGs in a graphics program (keeping size & palette reasonable).
3. Run the repacker to push changes back into a patched DAT.

---

## Basic Usage

Assuming:

- `pm3/`
  - `gadgets.dat`
  - `gadgets.off`
- `sprites_color_auto/`
  - `sprite_000__pal_001.png`
  - `sprite_001__pal_002.png`
  - ...

Run:

```bash
python sprite-packer.py pm3 --sprites-dir sprites_color_auto
```

### Defaults

- DAT file name: `gadgets.dat` (inside `pm3`)
- OFF file name: `gadgets.off` (inside `pm3`)
- Output DAT: `gadgets_patched.dat` (inside `pm3`)
- Interleave: `4` (must match the extractor)
- Sprites dir: `sprites_color_auto` (relative to current working directory)
- Applies **all** `sprite_XXX__pal_YYY.png` files it finds in `--sprites-dir`

---

## CLI Reference

```text
usage: WORKING-repack-production.py [-h]
       [--dat-name DAT_NAME] [--off-name OFF_NAME]
       [--out-name OUT_NAME] [--sprites-dir SPRITES_DIR]
       [--interleave INTERLEAVE] [--sprites SPRITES] [--dry-run]
       pm3_dir
```

### Positional argument

- `pm3_dir`  
  Directory containing `gadgets.dat` / `gadgets.off` (or equivalents).

### Options

- `--dat-name DAT_NAME`  
  DAT file name or path (default: `gadgets.dat` in `pm3_dir`).

- `--off-name OFF_NAME`  
  OFF file name or path (default: `gadgets.off` in `pm3_dir`).

- `--out-name OUT_NAME`  
  Output DAT name or path (default: `gadgets_patched.dat` in `pm3_dir`).  
  If you pass an absolute path, it will be used as-is.

- `--sprites-dir SPRITES_DIR`  
  Directory containing `sprite_XXX__pal_YYY.png` files.  
  Defaults to `sprites_color_auto`, resolved relative to current working dir.

- `--interleave INTERLEAVE`  
  Horizontal interleave factor (default: `4`).  
  Must match whatever the extractor used.

- `--sprites SPRITES`  
  Optional subset of sprites to apply, e.g.:

  ```text
  --sprites "0-20,35,40-45"
  ```

  Where:

  - Indices are **0-based**.
  - They refer to `XXX` from `sprite_XXX__pal_YYY.png`.
  - If omitted, all matching sprites found in `--sprites-dir` are applied.

- `--dry-run`  
  Perform all checks and log all updates, but **do not** write the patched DAT.

---

## Examples

### 1. Repack everything

```bash
python sprite-packer.py pm3 --sprites-dir sprites_color_auto
```

Writes:

```text
pm3/gadgets_patched.dat
```

### 2. Repack a subset only

```bash
python sprite-packer.py pm3 \
  --sprites-dir sprites_color_auto \
  --sprites "0-49,120-130" \
  --out-name gadgets_patched_subset.dat
```

### 3. Repack with explicit filenames (uppercase on disk)

```bash
python sprite-packer.py /path/to/PM3 \
  --dat-name GADGETS.DAT \
  --off-name GADGETS.OFF \
  --sprites-dir sprites_color_auto
```

### 4. Dry run (no write)

```bash
python sprite-packer.py pm3 \
  --sprites-dir sprites_color_auto \
  --sprites "0-10" \
  --dry-run
```

You’ll see which chunks *would* be updated without touching any files.

---

## What the Repacker Actually Does

For each `sprite_XXX__pal_YYY.png`:

1. Parses `XXX` → sprite chunk index, `YYY` → palette chunk index.
2. Reads the original sprite header from the DAT:
- Validates that the chunk is a sprite.
- Extracts the original width/height.
3. Loads the PNG as a **paletted** image (`mode="P"`):
- Verifies the PNG dimensions match the original sprite.
- Reads the per-pixel indices.
4. Re-applies PM3’s 4-way horizontal interleave to the index data.
5. Writes the interleaved indices back into the sprite chunk (keeping the 4-byte header).
6. Reads the original palette chunk:
- Assumes a 4-byte header + N\*3-byte palette payload.
- Converts the PNG’s 8-bit palette back to 6-bit.
- Writes the converted palette payload back into the chunk.

If anything doesn’t line up (wrong size, bad palette length, etc.), that PNG is skipped with a warning.

---

# 🧪 Tests

A simple permutation test harness is included:

- `test_repack_permutations.py`

It does *smoke tests* only: it checks that the CLI runs successfully and that
output DAT files are produced for various argument combinations.

---

## Test Requirements

- A working PM3 directory
- A sprites directory with `sprite_XXX__pal_YYY.png` files
  (for example, from the extractor)
- Python 3.8+
- Pillow installed

---

## Running the Tests

### Basic run

```bash
python3 test-sprite-packer.py /path/to/pm3 /path/to/sprites_color_auto
```

### With explicit DAT/OFF names

```bash
python3 test-sprite-packer.py \
  /path/to/pm3 \
  /path/to/sprites_color_auto \
  --dat-name GADGETS.DAT \
  --off-name GADGETS.OFF
```

### Custom output base directory for patched DATs

```bash
python3 test-sprite-packer.py \
  /path/to/pm3 \
  /path/to/sprites_color_auto \
  --base-out-dir repack_test_runs
```

---

## Test Output Layout

A successful run creates something like:

```text
repack_test_outputs/
  gadgets_patched_default_all.dat
  gadgets_patched_subset_small_range.dat
  gadgets_patched_subset_sparse.dat
  gadgets_patched_custom_interleave.dat
```

The `dry_run_no_write` scenario does not produce a DAT file (by design).

---

## Troubleshooting

### “No PNG files found in sprites directory”

- Check the path passed to `--sprites-dir`.
- Make sure the filenames match the pattern `sprite_XXX__pal_YYY.png`.

### “sprite chunk X not valid sprite”

- The chunk indexed by `XXX` in `gadgets.off` doesn’t look like a sprite
  (width/height don’t line up with the chunk length). Check that the
  filename’s indices are correct.

### “size mismatch, PNG WxH != original WxH”

- The edited PNG was resized.  
  You must keep the **exact** width and height from the original sprite.

---

## License

This project is provided to help PM3 reverse-engineering efforts; no license
restrictions implied.


## Palette map support (recommended)

The repacker supports the same palette mapping JSON as the extractor.

By default it looks for `palette-map.json` and uses it as the authoritative
sprite → palette association (even if `__pal_YYY` in filenames differs).

```bash
python3 sprite-packer.py pm3 --sprites-dir sprites_color_auto --palette-map palette-map.json
```

JSON format:

```json
{
  "211": 223,
  "212": "unknown",
  "223": "palette"
}
```
