# PM3 Sprite Extractor

This repository contains the sprite extractor for
**Premier Manager 3** (`gadgets.dat` / `gadgets.off`).

The extractor uses an **explicit mapping of sprite chunk IDs to palette chunk IDs**
loaded from `palette-map.json`.

---

## Features

- Reads chunk offsets from `gadgets.off`
- Classifies chunks as **sprites** or **binary bins**
- Uses an **explicit sprite → palette mapping**
- Validates **6‑bit VGA palettes**
- Converts palettes to **8‑bit RGB**
- Corrects PM3’s **4‑way horizontal interleave**
- Outputs **PNG** images
- Supports **sprite subset extraction**
- Ships with a permutation‑based **test harness**

---

## Requirements

- Python 3.8+
- Pillow

```bash
pip install pillow
```

---

## Basic usage

```bash
python sprite-extractor.py pm3
```

Directory layout:

```
pm3/
  gadgets.dat
  gadgets.off
```

---

## Output

Sprites are written to:

```
sprites_color_auto/
```

Filename format:

```
sprite_###__pal_###.png
```

---

## Sprite subset extraction

```bash
python sprite-extractor.py pm3 --sprites "203-232"
```

---

## Palette model (important)

- Each sprite ID maps to a **specific palette chunk**
- Mapping is defined in `palette-map.json`
- Unmapped sprites are skipped with a warning

---

## Tests

```bash
python test-sprite-extractor.py /path/to/pm3
```

Use `--fail-on-empty` to enforce output validation.

---

## Palette map

By default, the extractor reads:

- `palette-map.json`

You can override the path:

```bash
python sprite-extractor.py pm3 --palette-map /path/to/palette-map.json
```

JSON format:

```json
{
  "211": 223,
  "212": "unknown",
  "223": "palette"
}
```
