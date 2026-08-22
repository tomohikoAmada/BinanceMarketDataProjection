#!/usr/bin/env python3
"""Verify that Phase-8 Google Benchmark cells stay out of Phase 6."""

from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path


def benchmark_names(binary: str) -> list[str]:
    with tempfile.TemporaryDirectory() as directory:
        output = Path(directory) / "benchmark.json"
        wrapper = Path(directory) / "wrapper.json"
        result = subprocess.run(
            [binary, "--benchmark_list_tests=true", "--benchmark_format=json",
             f"--benchmark_out={output}",
             f"--m5_wrapper_out={wrapper}", "--m5_evidence_class=exploratory"],
            capture_output=True,
            text=True,
            check=False,
        )
    if result.returncode != 0:
        raise RuntimeError(f"{binary} failed: {result.stderr[-500:]}")
    return [line.strip() for line in result.stdout.splitlines() if line.strip()]


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: check_phase6_phase8_separation.py PHASE6_BINARY PHASE8_BINARY",
              file=sys.stderr)
        return 2

    phase6_names = benchmark_names(sys.argv[1])
    phase8_names = benchmark_names(sys.argv[2])
    contaminated = [name for name in phase6_names if name.startswith("phase8-")]
    if contaminated:
        print(f"Phase-6 benchmark contains Phase-8 registrations: {contaminated[:5]}",
              file=sys.stderr)
        return 1
    if len(phase8_names) != 40 or not all(name.startswith("phase8-") for name in phase8_names):
        print(f"Phase-8 benchmark inventory is not the accepted 40-cell set: "
              f"{len(phase8_names)} registrations", file=sys.stderr)
        return 1

    print("Phase-6/Phase-8 benchmark registration separation PASS "
          "(Phase-6 has 0 Phase-8 cells; Phase-8 has 40 candidate cells)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
