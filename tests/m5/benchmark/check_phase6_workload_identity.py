#!/usr/bin/env python3
"""Phase-6 workload identity regression (bit-for-bit preservation).

Runs the Phase-6 benchmark executable with a zero-match filter (no timed
benchmarks execute; the wrapper's workload_identities are the static
registration registry), then compares every registered canonical workload
spec text and SHA-256 against the committed golden identity set. Any drift in
an accepted Phase-6 workload identity fails closed; the golden values are
never updated to make a test pass.

For the Limited M4 snapshot family, it also executes one real benchmark cell
and requires the runtime depth-limit label. The executable's narrow
provenance seam compares that concrete runtime option with the generated
identity; this check ensures that the actual path is exercised rather than
only inspecting the static registry.

The golden set is split into a Core subset (always required) and an adapter
subset (required only when the binary registers M4/AdapterWireReplay specs,
i.e. built with BMD_PROJECTION_BUILD_PROTO_ADAPTER=ON).

Usage:
  python3 tests/m5/benchmark/check_phase6_workload_identity.py \
      /path/to/bmd_projection_benchmarks /scratch/dir
"""

from __future__ import annotations

import hashlib
import json
import os
import subprocess
import sys


def fail(message: str) -> None:
    print(f"VALIDATION FAILED: {message}", file=sys.stderr)
    sys.exit(1)


def run_benchmark(binary: str, arguments: list[str], payload_path: str,
                  description: str) -> dict:
    result = subprocess.run([binary, *arguments], capture_output=True, text=True)
    if result.returncode != 0:
        fail(f"{description} failed: {result.stderr[-500:]}")
    try:
        with open(payload_path, "r", encoding="utf-8") as stream:
            return json.load(stream)
    except (ValueError, OSError, StopIteration) as error:
        fail(f"{description} produced invalid benchmark JSON: {error}")


def main() -> int:
    if len(sys.argv) != 3:
        fail("usage: check_phase6_workload_identity.py BINARY SCRATCH_DIR")
    binary = sys.argv[1]
    scratch = sys.argv[2]
    os.makedirs(scratch, exist_ok=True)
    payload = os.path.join(scratch, "identity-empty-payload.json")
    wrapper = os.path.join(scratch, "identity-wrapper.json")

    result = subprocess.run(
        [binary, "--benchmark_filter=^$", "--benchmark_format=json",
         f"--benchmark_out={payload}", f"--m5_wrapper_out={wrapper}",
         "--m5_evidence_class=exploratory"],
        capture_output=True, text=True)
    if result.returncode != 0:
        fail(f"benchmark binary failed: {result.stderr[-500:]}")
    if not os.path.isfile(wrapper):
        fail("benchmark binary produced no wrapper")

    with open(wrapper, "r", encoding="utf-8") as stream:
        registered = json.load(stream).get("workload_identities", [])

    here = os.path.dirname(os.path.abspath(__file__))
    core_golden = []
    adapter_golden = []
    for path, target in (
            (os.path.join(here, "phase6_workload_identity_golden_core.txt"), core_golden),
            (os.path.join(here, "phase6_workload_identity_golden_adapter.txt"),
             adapter_golden)):
        with open(path, "r", encoding="utf-8") as stream:
            for line in stream:
                name, _, sha = line.strip().partition(" ")
                if name:
                    target.append((name, sha))

    adapter_present = any(entry.get("benchmark_name", "").startswith("M4/")
                          for entry in registered)
    expected = list(core_golden)
    if adapter_present:
        expected.extend(adapter_golden)

    registered_by_name = {entry["benchmark_name"]: entry for entry in registered}
    expected_by_name = {name: sha for name, sha in expected}
    missing = sorted(set(expected_by_name) - set(registered_by_name))
    if missing:
        fail(f"identity set drift: missing={missing[:5]}")

    for name, golden_sha in expected_by_name.items():
        actual = registered_by_name[name]
        if actual.get("workload_spec_schema") != "M5_BENCHMARK_WORKLOAD_SPEC_V1":
            fail(f"workload {name} has wrong workload_spec_schema")
        canonical = actual.get("canonical_spec_text", "")
        sha = actual.get("workload_spec_sha256", "")
        if hashlib.sha256(canonical.encode("utf-8")).hexdigest() != sha:
            fail(f"workload {name} workload_spec_sha256 does not match its canonical text")
        if sha != golden_sha:
            fail(f"workload {name} workload_spec_sha256 drifted from the accepted identity")

    if adapter_present:
        limited_name = "M4/MakeLocalOrderBookSnapshot/Limited/100"
        if limited_name not in registered_by_name:
            fail(f"missing Limited runtime identity {limited_name}")
        runtime_payload = os.path.join(scratch, "limited-runtime-payload.json")
        runtime_wrapper = os.path.join(scratch, "limited-runtime-wrapper.json")
        runtime_arguments = [
            "--benchmark_filter=^M4/MakeLocalOrderBookSnapshot/Limited/100/",
            "--benchmark_format=json",
            "--benchmark_min_time=0.001",
            f"--benchmark_out={runtime_payload}",
            f"--m5_wrapper_out={runtime_wrapper}",
            "--m5_evidence_class=exploratory",
        ]
        runtime = run_benchmark(binary, runtime_arguments, runtime_payload,
                                "Limited runtime benchmark")
        measurements = [entry for entry in runtime.get("benchmarks", [])
                        if entry.get("run_type") != "aggregate"]
        if len(measurements) != 1:
            fail(f"Limited runtime benchmark produced {len(measurements)} measurements")
        label = measurements[0].get("label")
        if not isinstance(label, str) or not label.startswith("depth_limit="):
            fail(f"Limited runtime benchmark did not expose its concrete depth limit: {label!r}")
        if not label.removeprefix("depth_limit=").isdigit():
            fail(f"Limited runtime benchmark exposed a non-numeric depth limit: {label!r}")

    print(f"Phase-6 workload identity regression PASS: {len(expected)} registered "
          "workload identities bit-for-bit identical to the accepted golden set; "
          "Limited runtime depth matches its canonical identity")
    return 0


if __name__ == "__main__":
    sys.exit(main())
