#!/usr/bin/env python3
"""
Runs the TILEPro64 experiment matrix and saves raw results to tilepro64/results/raw_results.csv.
Adheres strictly to Sections 26 and 27 of prompt.md.
"""
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
BIN = REPO_ROOT / "bin" / "tilepro64"
RESULTS_DIR = REPO_ROOT / "results"
RESULTS_DIR.mkdir(parents=True, exist_ok=True)
RAW_CSV = RESULTS_DIR / "raw_results.csv"

def main():
    if not BIN.exists():
        print(f"Error: {BIN} does not exist. Run 'make' first.", file=sys.stderr)
        sys.exit(1)

    print("Running TILEPro64 experimental matrix...")
    cmd = [str(BIN), "--all"]
    proc = subprocess.run(cmd, capture_output=True, text=True)

    if proc.returncode != 0:
        print(f"Error running simulator: {proc.stderr}", file=sys.stderr)
        sys.exit(proc.returncode)

    with open(RAW_CSV, "w") as f:
        f.write(proc.stdout)

    line_count = len(proc.stdout.strip().splitlines()) - 1
    print(f"Successfully generated {line_count} experimental rows -> {RAW_CSV}")

if __name__ == "__main__":
    main()
