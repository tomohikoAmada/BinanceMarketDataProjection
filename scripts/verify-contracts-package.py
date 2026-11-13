#!/usr/bin/env python3
"""Fail-closed validation of the locally created C-M4-001 Conan package surfaces."""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

EXPECTED_RREV = "b8ed9e62988716feaeb973cc65e4a279"
EXPECTED = {
    "01d76a41929f36d89573159f5f458f9f1e378ada",
    "a2a93ca5cf4e17c9a64ca0939ad208f1202c1746628eed5260bd0edb81666c86",
    "NOT_FORMALLY_ASSIGNED",
    "libprotoc 33.5",
    "6.33.5",
    "ca5ff466767b31a1b496ec60247e105c",
    "full",
    "fd104a08fc4dbdb195ae20f685942e445342e66f",
}


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit("usage: verify-contracts-package.py CONAN PACKAGE_JSON")
    conan = Path(sys.argv[1])
    package_json = Path(sys.argv[2])
    document = json.loads(package_json.read_text(encoding="utf-8"))
    coordinate = document["Local Cache"]["binance-market-data-contracts-cpp/0.1.0"]
    revisions = coordinate["revisions"]
    if set(revisions) != {EXPECTED_RREV}:
        raise SystemExit(f"unexpected Contracts recipe revisions: {sorted(revisions)}")
    packages = revisions[EXPECTED_RREV]["packages"]
    if not packages:
        raise SystemExit("the pinned Contracts recipe has no binary package")

    for package_id in packages:
        reference = (
            "binance-market-data-contracts-cpp/0.1.0#"
            f"{EXPECTED_RREV}:{package_id}"
        )
        package_folder = Path(
            subprocess.check_output(
                [str(conan), "cache", "path", reference], text=True
            ).strip()
        )
        config = package_folder / "lib/cmake/BinanceMarketDataContracts/BinanceMarketDataContractsConfig.cmake"
        provenance = package_folder / "share/BinanceMarketDataContracts/provenance.json"
        if not config.is_file() or not provenance.is_file():
            raise SystemExit(f"required package metadata is missing from {package_folder}")
        combined = config.read_text(encoding="utf-8") + provenance.read_text(encoding="utf-8")
        missing = sorted(value for value in EXPECTED if value not in combined)
        if missing:
            raise SystemExit(f"Contracts metadata mismatch in {package_folder}: {missing}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
