#!/usr/bin/env python3
"""Fail unless every M5 differential-fuzz translation unit is instrumented."""

from __future__ import annotations

import json
import sys
from pathlib import Path


SUPPORT_SOURCES = (
    "tests/m5/replay/canonical_text.cpp",
    "tests/m5/replay/replay_fixture.cpp",
    "tests/m5/replay/replay_manifest.cpp",
    "tests/m5/replay/replay_parser.cpp",
    "tests/m5/replay/replay_types.cpp",
    "tests/m5/oracle/core_production_side.cpp",
    "tests/m5/oracle/divergence.cpp",
    "tests/m5/oracle/production_decimal_observation.cpp",
    "tests/m5/oracle/replay_driver.cpp",
    "tests/m5/oracle/reference_side.cpp",
    "tests/m5/reference/reference_adapter.cpp",
    "tests/m5/reference/reference_decimal.cpp",
)
FUZZER_SOURCES = (
    "fuzz/replay_fuzz.cpp",
    "fuzz/m5/replay_fuzz_decoder.cpp",
    "fuzz/m5/replay_fuzz_fixture.cpp",
    "tests/m5/oracle/adapter_production_side.cpp",
)
SUPPORT_SANITIZERS = "-fsanitize=fuzzer-no-link,address,undefined"
FUZZER_SANITIZERS = "-fsanitize=fuzzer,address,undefined"
FRAME_POINTER = "-fno-omit-frame-pointer"


def command_text(entry: dict[str, object]) -> str:
    arguments = entry.get("arguments")
    if isinstance(arguments, list) and all(isinstance(item, str) for item in arguments):
        return " ".join(arguments)
    command = entry.get("command")
    return command if isinstance(command, str) else ""


def matching_entries(entries: list[dict[str, object]], suffix: str) -> list[dict[str, object]]:
    return [
        entry
        for entry in entries
        if isinstance(entry.get("file"), str)
        and Path(str(entry["file"])).as_posix().endswith(suffix)
    ]


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: check-m5-fuzz-instrumentation.py <compile_commands.json>", file=sys.stderr)
        return 2

    path = Path(sys.argv[1])
    try:
        raw = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        print(f"instrumentation check: cannot read {path}: {exc}", file=sys.stderr)
        return 1
    if not isinstance(raw, list) or not all(isinstance(entry, dict) for entry in raw):
        print("instrumentation check: compile database must be an array of objects", file=sys.stderr)
        return 1

    entries: list[dict[str, object]] = raw
    failures: list[str] = []
    for suffix in SUPPORT_SOURCES:
        matches = matching_entries(entries, suffix)
        if not matches:
            failures.append(f"missing compile command: {suffix}")
            continue
        for entry in matches:
            command = command_text(entry)
            if SUPPORT_SANITIZERS not in command or FRAME_POINTER not in command:
                failures.append(f"support source is not fully instrumented: {suffix}")

    for suffix in FUZZER_SOURCES:
        matches = matching_entries(entries, suffix)
        if not matches:
            failures.append(f"missing compile command: {suffix}")
            continue
        for entry in matches:
            command = command_text(entry)
            if FUZZER_SANITIZERS not in command or FRAME_POINTER not in command:
                failures.append(f"fuzzer source is not fully instrumented: {suffix}")

    if failures:
        for failure in failures:
            print(f"instrumentation check: FAIL: {failure}", file=sys.stderr)
        return 1

    print(
        "instrumentation check: PASS "
        f"({len(SUPPORT_SOURCES)} support and {len(FUZZER_SOURCES)} fuzzer sources)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
