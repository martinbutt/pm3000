#!/usr/bin/env python3
"""
test-sprite-packer.py

Permutation test harness for sprite-packer.py.

This uses your real PM3 directory and an existing sprites directory
(typically the output of the extractor) to verify that:

  - The repacker CLI works with different argument permutations.
  - It can repack a subset of sprites.
  - It accepts custom interleave values (even though 4 is the real one).
  - It writes out patched DAT files (or can perform a dry-run).

NOTE: This does NOT validate the semantic correctness of the resulting
DAT; it is a smoke-test for the CLI + basic I/O paths.
"""

import argparse
import subprocess
import sys
from pathlib import Path


def run_cmd(cmd, desc: str):
    print(f"\n=== {desc} ===")
    print("CMD:", " ".join(str(c) for c in cmd))
    result = subprocess.run(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    print("--- STDOUT ---")
    print(result.stdout)
    print("--- STDERR ---")
    print(result.stderr)

    if result.returncode != 0:
        raise RuntimeError(f"{desc} failed with exit code {result.returncode}")


def ensure_file(path: Path, desc: str):
    if not path.is_file():
        raise AssertionError(f"{desc}: expected file not found: {path}")
    if path.stat().st_size <= 0:
        raise AssertionError(f"{desc}: file is empty: {path}")
    print(f"{desc}: OK, file exists and is non-empty: {path}")


def main():
    parser = argparse.ArgumentParser(
        description="Run basic permutation tests against the sprite repacker."
    )
    parser.add_argument(
        "pm3_dir",
        type=Path,
        help="Real PM3 directory containing gadgets.dat/gadgets.off (or equivalents).",
    )
    parser.add_argument(
        "sprites_dir",
        type=Path,
        help="Directory containing sprite_XXX__pal_YYY.png files (e.g. extractor output).",
    )
    parser.add_argument(
        "--repacker",
        type=Path,
        default=Path("sprite-packer.py"),
        help="Path to the repacker script to test (default: sprite-packer.py).",
    )
    parser.add_argument(
        "--dat-name",
        help="Optional DAT file name to pass through (e.g. GADGETS.DAT).",
    )
    parser.add_argument(
        "--off-name",
        help="Optional OFF file name to pass through (e.g. GADGETS.OFF).",
    )

    parser.add_argument(
        "--palette-map",
        type=Path,
        default=Path("palette-map.json"),
        help="Palette map JSON to pass through to repacker (default: palette-map.json).",
    )
    parser.add_argument(
        "--base-out-dir",
        type=Path,
        default=Path("repack_test_outputs"),
        help="Base directory for test output DAT files (default: repack_test_outputs).",
    )

    args = parser.parse_args()

    pm3_dir = args.pm3_dir
    sprites_dir = args.sprites_dir
    repacker = args.repacker
    base_out = args.base_out_dir

    if not repacker.is_file():
        raise FileNotFoundError(f"Repacker script not found: {repacker}")
    if not pm3_dir.is_dir():
        raise NotADirectoryError(f"PM3 directory not found: {pm3_dir}")
    if not sprites_dir.is_dir():
        raise NotADirectoryError(f"Sprites directory not found: {sprites_dir}")

    base_out.mkdir(parents=True, exist_ok=True)

    # 1) Test --help
    help_cmd = [sys.executable, str(repacker), "--help"]
    run_cmd(help_cmd, "Help output")

    # Common arguments
    common = [
        sys.executable,
        str(repacker),
        str(pm3_dir),
        "--sprites-dir",
        str(sprites_dir),
    ]
    if args.dat_name:
        common += ["--dat-name", args.dat_name]
    if args.off_name:
        common += ["--off-name", args.off_name]

    # Palette map passthrough
    if args.palette_map:
        common += ["--palette-map", str(args.palette_map)]

    scenarios = [
        {
            "name": "default_all",
            "extra": [],
            "check_output": True,
        },
        {
            "name": "subset_small_range",
            "extra": ["--sprites", "0-50"],
            "check_output": True,
        },
        {
            "name": "subset_sparse",
            "extra": ["--sprites", "0,5,42,100-120"],
            "check_output": True,
        },
        {
            "name": "custom_interleave",
            "extra": ["--interleave", "4"],
            "check_output": True,
        },
        {
            "name": "dry_run_no_write",
            "extra": ["--sprites", "0-10", "--dry-run"],
            "check_output": False,
        },
    ]

    for scenario in scenarios:
        name = scenario["name"]
        out_dat = base_out / f"gadgets_patched_{name}.dat"

        cmd = common + ["--out-name", str(out_dat)] + scenario["extra"]
        desc = f"Scenario: {name}"
        run_cmd(cmd, desc)

        if scenario["check_output"]:
            ensure_file(out_dat, desc)
        else:
            print(f"{desc}: dry-run scenario; no output DAT expected.")

    print("\nAll repacker permutations completed successfully.")


if __name__ == "__main__":
    main()
