#!/usr/bin/env python3
"""
Permutation test harness for sprite-extractor.py

Usage:
    python3 test-sprite-extractor.py /absolute/path/to/pm3 \
        --extractor sprite-extractor.py

Options:
    --dat-name / --off-name  Forwarded to extractor (if your files are renamed)
    --fail-on-empty          Fail a test if the output directory is empty
"""

import argparse
import subprocess
import sys
from pathlib import Path


def run_cmd(cmd: list[str], desc: str):
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


def ensure_non_empty_dir(path: Path, desc: str):
    files = list(path.glob("*"))
    if not files:
        raise AssertionError(f"{desc}: output directory is empty: {path}")
    print(f"{desc}: OK, found {len(files)} file(s) in {path}")


def main():
    parser = argparse.ArgumentParser(
        description="Run basic permutation tests against the sprite extractor."
    )
    parser.add_argument(
        "pm3_dir",
        type=Path,
        help="Real PM3 directory containing gadgets.dat/gadgets.off (or equivalents).",
    )
    parser.add_argument(
        "--extractor",
        type=Path,
        default=Path("sprite-extractor.py"),
        help="Path to the extractor script to test (default: sprite-extractor.py).",
    )
    parser.add_argument(
        "--dat-name",
        help="Optional DAT file name to pass through to extractor (e.g. GADGETS.DAT).",
    )
    parser.add_argument(
        "--off-name",
        help="Optional OFF file name to pass through to extractor (e.g. GADGETS.OFF).",
    )

    parser.add_argument(
        "--palette-map",
        type=Path,
        default=Path("palette_map.final.json"),
        help="Palette map JSON to pass through to extractor (default: palette_map.final.json).",
    )
    parser.add_argument(
        "--fail-on-empty",
        action="store_true",
        help="Fail a permutation if its output directory is empty.",
    )
    parser.add_argument(
        "--base-out-dir",
        type=Path,
        default=Path("test_outputs"),
        help="Base directory in which to place per-test output folders (default: test_outputs).",
    )

    args = parser.parse_args()

    pm3_dir = args.pm3_dir
    extractor = args.extractor
    base_out = args.base_out_dir

    if not extractor.is_file():
        raise FileNotFoundError(f"Extractor script not found: {extractor}")
    if not pm3_dir.is_dir():
        raise NotADirectoryError(f"PM3 directory not found: {pm3_dir}")

    base_out.mkdir(parents=True, exist_ok=True)

    # 1) Test --help (no PM3 needed)
    help_cmd = [sys.executable, str(extractor), "--help"]
    run_cmd(help_cmd, "Help output")

    # Common args for all actual extraction runs
    common = [sys.executable, str(extractor), str(pm3_dir)]
    if args.dat_name:
        common += ["--dat-name", args.dat_name]
    if args.off_name:
        common += ["--off-name", args.off_name]

    # Palette map passthrough
    if args.palette_map:
        common += ["--palette-map", str(args.palette_map)]

    # Define permutations / scenarios
    scenarios = [
        {
            "name": "default_all",
            "extra": [],
            "expect_output": True,
        },
        {
            "name": "subset_png_small_range",
            "extra": ["--sprites", "0-50"],
            "expect_output": True,
        },
        {
            "name": "subset_both_sparse",
            "extra": ["--sprites", "0,5,42,100-120"],
            "expect_output": True,
        }
    ]

    # 2) Run permutations
    for scenario in scenarios:
        name = scenario["name"]
        out_dir = base_out / name
        out_dir.mkdir(parents=True, exist_ok=True)

        cmd = common + ["--output-dir", str(out_dir)] + scenario["extra"]
        desc = f"Scenario: {name}"
        run_cmd(cmd, desc)

        if scenario.get("expect_output") and args.fail_on_empty:
            ensure_non_empty_dir(out_dir, desc)

    print("\nAll permutations completed successfully.")


if __name__ == "__main__":
    main()
