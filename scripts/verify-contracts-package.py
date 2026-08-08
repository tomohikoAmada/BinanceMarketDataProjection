#!/usr/bin/env python3
"""Fail-closed validation of the locally created C-M4-001 Conan package surfaces."""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

EXPECTED_RREV = "7fd3efe3d289462fb16c78ffeced1682"
EXPECTED = {
    "01d76a41929f36d89573159f5f458f9f1e378ada",
    "33286fb1d624f4dd0c827010e93113f523c7f37dc4f6ae526361d2b0c61626c0",
    "NOT_FORMALLY_ASSIGNED",
    "libprotoc 33.5",
    "6.33.5",
    "ca5ff466767b31a1b496ec60247e105c",
    "full",
    "67ee1bf69fad980d114cfa278c3a6ffe310a4d7a",
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
