#!/usr/bin/env python3
"""M5 Phase-6 benchmark validation and summarization.

Fail-closed validators for the Phase-6 machine-readable outputs:

  - Google Benchmark JSON payload structure/execution evidence
  - required benchmark inventory (including the full 48-cell M3 accepted
    matrix and the required M4 names; OD-M5-P6-013/022 fail closed)
  - smoke expectation sets (zero-match, SkipWithError, error_occurred)
  - M5_BENCHMARK_WRAPPER_V1 schema, provenance, and payload SHA binding
  - M5_REPLAY_LATENCY_V1 recomputation (nearest-rank-v1) and eligibility
  - human-readable formal evidence summary (derived from machine-readable
    results only)

Usage:
  python3 scripts/benchmark_phase6.py validate-inventory WRAPPER_JSON [PAYLOAD_JSON]
  python3 scripts/benchmark_phase6.py validate-smoke PAYLOAD_JSON WRAPPER_JSON --repetitions N
  python3 scripts/benchmark_phase6.py validate-wrapper WRAPPER_JSON [--binary PATH] [--allow-exploratory]
  python3 scripts/benchmark_phase6.py validate-latency LATENCY_JSON WRAPPER_JSON
  python3 scripts/benchmark_phase6.py summarize WRAPPER_JSON [LATENCY_WRAPPER_JSON ...]
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import sys
from typing import Any, Optional

WRAPPER_SCHEMA = "M5_BENCHMARK_WRAPPER_V1"
MEASUREMENT_CONTRACT = "M5_PHASE6_MEASUREMENT_CONTRACT_V1"
LATENCY_SCHEMA = "M5_REPLAY_LATENCY_V1"
WORKLOAD_SPEC_SCHEMA = "M5_BENCHMARK_WORKLOAD_SPEC_V1"
QUANTILE_ESTIMATOR = "nearest-rank-v1"

M1_NAMES = [
    "M1/ParsePrice/MatchedScale",
    "M1/ParsePositiveQuantity/MatchedScale",
    "M1/ParseQuantity/ZeroSuccess",
    "M1/ParsePositiveQuantity/ZeroRejected",
    "M1/ParsePrice/ExactUpscale",
    "M1/ParsePrice/ExactDownscale",
    "M1/ParsePrice/InexactDownscaleRejected",
    "M1/ParsePrice/OverflowRejected",
    "M1/ParsePrice/SyntaxRejected",
    "M1/FormatPriceFixed",
    "M1/FormatQuantityFixed",
]

ROUTINE_DEPTHS = [8, 100, 1000]
FULL_DEPTH_SET = [0, 8, 100, 1000, 5000, 10000]
BATCH_SET = [1, 10, 100]
M3_DEPTH_SET = [0, 8, 100, 1000, 5000, 10000]
M3_BATCH_SET = [0, 1, 10, 100]
TOP_N_SET = [1, 5, 50]
M4_FAMILIES = [
    "AdaptExchangeDepthSnapshot/Spot",
    "AdaptDepthUpdate/Spot",
    "CheckedInstall",
    "CheckedApply",
    "MakeLocalOrderBookSnapshot/Unlimited",
    "MakeLocalOrderBookSnapshot/Limited",
    "SerializeSnapshot/FreshBuffer",
    "SerializeSnapshot/ReusedBuffer",
]
CLASSIFICATIONS = ["Stale", "Duplicate", "Gap", "Reset", "BaselineInstall"]
POLICIES = ["Spot", "UsdMPerpetual"]


class ValidationError(Exception):
    pass


def _required_inventory() -> list[str]:
    required: list[str] = []
    required.extend(M1_NAMES)
    for family in ["insert", "update", "delete", "missing_delete"]:
        for depth in ROUTINE_DEPTHS:
            required.append(f"M2/apply_level/{family}/{depth}")
    for batch in BATCH_SET:
        for depth in ROUTINE_DEPTHS:
            required.append(f"M2/apply_updates/{batch}/{depth}")
    for depth in FULL_DEPTH_SET:
        required.append(f"M2/apply_updates/update_mix/{depth}")
    for depth in FULL_DEPTH_SET:
        required.append(f"M2/replace_all/{depth}")
    for name in ["best_bid", "best_ask", "quantity_at/hit", "quantity_at/miss"]:
        for depth in ROUTINE_DEPTHS:
            required.append(f"M2/{name}/{depth}")
    for limit in TOP_N_SET:
        for depth in ROUTINE_DEPTHS:
            required.append(f"M2/top_levels/{limit}/{depth}")
    for depth in FULL_DEPTH_SET:
        required.append(f"M2/all_levels/{depth}")
    for policy in POLICIES:
        for depth in M3_DEPTH_SET:
            for batch in M3_BATCH_SET:
                required.append(f"M3/LiveApply/Accepted/{policy}/D{depth}/B{batch}")
        for classification in CLASSIFICATIONS:
            required.append(f"M3/Classification/{classification}/{policy}")
    for depth in ROUTINE_DEPTHS:
        required.append(f"M3/Component/AllLevelsBothSides/{depth}")
        required.append(f"M3/Proxy/CandidateRebuildFromVectors/{depth}")
        required.append(f"M3/Proxy/CandidateApplyUpdates/{depth}")
        required.append(f"M3/Proxy/OrderBookMoveCommit/{depth}")
    for family in M4_FAMILIES:
        for depth in ROUTINE_DEPTHS:
            required.append(f"M4/{family}/{depth}")
    required.append("CoreNormalizedReplay/Spot")
    required.append("CoreNormalizedReplay/UsdMPerpetual")
    required.append("AdapterWireReplay/Spot")
    required.append("AdapterWireReplay/UsdMPerpetual")
    return required


def _smoke_expected_set() -> list[str]:
    expected: list[str] = []
    expected.extend(M1_NAMES)
    for family in ["insert", "update", "delete", "missing_delete"]:
        expected.append(f"M2/apply_level/{family}/8")
    expected.append("M2/apply_updates/10/8")
    expected.append("M2/apply_updates/update_mix/8")
    expected.append("M2/replace_all/8")
    expected.append("M2/best_bid/8")
    expected.append("M2/best_ask/8")
    expected.append("M2/quantity_at/hit/8")
    expected.append("M2/quantity_at/miss/8")
    expected.append("M2/top_levels/5/8")
    expected.append("M2/all_levels/8")
    # Locked 8-cell accepted-live M3 CI smoke subset (OD-M5-P6-047).
    for policy in POLICIES:
        for depth in [8, 1000]:
            for batch in [0, 10]:
                expected.append(f"M3/LiveApply/Accepted/{policy}/D{depth}/B{batch}")
        for classification in CLASSIFICATIONS:
            expected.append(f"M3/Classification/{classification}/{policy}")
    expected.append("M3/Component/AllLevelsBothSides/8")
    expected.append("M3/Proxy/CandidateRebuildFromVectors/8")
    expected.append("M3/Proxy/CandidateApplyUpdates/8")
    expected.append("M3/Proxy/OrderBookMoveCommit/8")
    for family in M4_FAMILIES:
        expected.append(f"M4/{family}/100")
    expected.append("CoreNormalizedReplay/Spot")
    expected.append("CoreNormalizedReplay/UsdMPerpetual")
    expected.append("AdapterWireReplay/Spot")
    expected.append("AdapterWireReplay/UsdMPerpetual")
    expected.append("BM_LibraryVersionAccess")
    return expected


import re

_DECORATION_COMPONENT = re.compile(
    r"^(min_time:[0-9.]+|iterations:[0-9]+|real_time|threads:[0-9]+|"
    r"repetition:[0-9]+|[a-z_]+:[-0-9]+)$"
)


def normalize_benchmark_name(name: str) -> str:
    components = name.split("/")
    while components and _DECORATION_COMPONENT.match(components[-1]):
        components.pop()
    return "/".join(components)


def _load_json(path: str, description: str) -> Any:
    if not os.path.isfile(path):
        raise ValidationError(f"{description} not found: {path}")
    try:
        with open(path, "r", encoding="utf-8") as stream:
            return json.load(stream)
    except (json.JSONDecodeError, OSError) as error:
        raise ValidationError(f"invalid {description} JSON at {path}: {error}") from error


def _sha256_file(path: str) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _fail(message: str) -> None:
    raise ValidationError(message)


def _require(condition: bool, message: str) -> None:
    if not condition:
        _fail(message)


def validate_payload_structure(payload: dict[str, Any]) -> None:
    _require(isinstance(payload, dict), "payload must be a JSON object")
    _require(isinstance(payload.get("context"), dict), "payload missing context object")
    benchmarks = payload.get("benchmarks")
    _require(isinstance(benchmarks, list), "payload missing benchmarks array")
    _require(len(benchmarks) > 0, "payload benchmark array is empty (zero-match filter failure)")
    for entry in benchmarks:
        name = entry.get("name")
        _require(isinstance(name, str) and name, "payload benchmark entry missing name")
        if entry.get("error_occurred") is True:
            _fail(f"payload benchmark {name} reports error_occurred=true")
        if entry.get("skipped") is True or entry.get("skip_message"):
            _fail(f"payload benchmark {name} was skipped: {entry.get('skip_message')}")
        iterations = entry.get("iterations")
        _require(isinstance(iterations, (int, float)) and iterations > 0,
                 f"payload benchmark {name} has non-positive iterations")
        for time_key in ("real_time", "cpu_time"):
            value = entry.get(time_key)
            _require(isinstance(value, (int, float)) and math.isfinite(value) and value > 0,
                     f"payload benchmark {name} has non-finite/non-positive {time_key}")


def _workload_names(wrapper: dict[str, Any]) -> set[str]:
    identities = wrapper.get("workload_identities")
    _require(isinstance(identities, list) and identities, "wrapper missing workload_identities")
    names: set[str] = set()
    for entry in identities:
        name = entry.get("benchmark_name")
        _require(isinstance(name, str) and name, "workload identity missing benchmark_name")
        _require(entry.get("workload_spec_schema") == WORKLOAD_SPEC_SCHEMA,
                 f"workload {name} has wrong workload_spec_schema")
        spec_sha = entry.get("workload_spec_sha256")
        canonical = entry.get("canonical_spec_text")
        _require(isinstance(spec_sha, str) and len(spec_sha) == 64,
                 f"workload {name} missing workload_spec_sha256")
        _require(isinstance(canonical, str) and canonical,
                 f"workload {name} missing canonical_spec_text")
        recomputed = hashlib.sha256(canonical.encode("utf-8")).hexdigest()
        _require(recomputed == spec_sha,
                 f"workload {name} workload_spec_sha256 does not match its canonical text")
        names.add(name)
    return names


def validate_inventory(wrapper: dict[str, Any]) -> set[str]:
    names = _workload_names(wrapper)
    required = _required_inventory()
    missing = [name for name in required if name not in names]
    if missing:
        _fail("required benchmark inventory missing "
              f"({len(missing)} entries): {', '.join(missing[:12])}...")
    m3_cells = [name for name in names
                if name.startswith("M3/LiveApply/Accepted/")]
    _require(len(m3_cells) == 48,
             f"expected 48 registered M3 accepted cells, got {len(m3_cells)}")
    m4_names = [name for name in names if name.startswith("M4/")]
    _require(len(m4_names) >= len(M4_FAMILIES) * 3,
             f"required M4 benchmark inventory missing: got {len(m4_names)} M4 entries")
    return names


def validate_smoke(payload: dict[str, Any], wrapper: dict[str, Any],
                   repetitions: int) -> None:
    validate_payload_structure(payload)
    validate_inventory(wrapper)
    executed = {normalize_benchmark_name(entry.get("name", ""))
                for entry in payload.get("benchmarks", [])}
    expected = set(_smoke_expected_set())
    _require(len(executed) > 0, "smoke filter matched zero benchmarks")
    missing = sorted(expected - executed)
    unexpected = sorted(executed - expected)
    if missing or unexpected:
        _fail(f"smoke executed set mismatch: missing={missing} unexpected={unexpected}")
    for entry in payload.get("benchmarks", []):
        name = normalize_benchmark_name(entry.get("name", ""))
        _require(isinstance(entry.get("repetitions"), (int, float)), "missing repetitions")
        if repetitions == 1:
            _require(int(entry["repetitions"]) == 1,
                     f"smoke benchmark {name} must run exactly 1 repetition")


def _require_required_strings(document: dict[str, Any], keys: list[str],
                              description: str) -> None:
    for key in keys:
        _require(isinstance(document.get(key), str) and document.get(key),
                 f"{description} missing required field {key}")


def validate_wrapper(path: str, wrapper: dict[str, Any], allow_exploratory: bool,
                     binary: Optional[str], require_inventory: bool = False) -> None:
    _require(wrapper.get("schema") == WRAPPER_SCHEMA,
             f"unknown wrapper schema: {wrapper.get('schema')}")
    _require(wrapper.get("measurement_contract_version") == MEASUREMENT_CONTRACT,
             "missing/wrong measurement_contract_version")
    _require_required_strings(wrapper, ["schema", "measurement_contract_version",
                                        "evidence_class", "requested_evidence_class"],
                              "wrapper")

    source = wrapper.get("source_provenance")
    _require(isinstance(source, dict), "wrapper missing source_provenance")
    _require_required_strings(source, ["git_sha"], "source_provenance")
    _require(isinstance(source.get("dirty_at_configure"), bool),
             "source_provenance missing dirty_at_configure")

    evidence_class = wrapper.get("evidence_class")
    _require(evidence_class in ("formal", "exploratory"), "invalid evidence_class")
    if source.get("dirty_at_configure") and evidence_class == "formal":
        _fail("dirty source cannot produce formal baseline evidence")

    binary_provenance = wrapper.get("binary_provenance")
    _require(isinstance(binary_provenance, dict), "wrapper missing binary_provenance")
    _require_required_strings(binary_provenance, ["path", "sha256"], "binary_provenance")
    if binary is not None:
        actual = _sha256_file(binary)
        _require(actual == binary_provenance.get("sha256"),
                 f"binary provenance SHA mismatch: {actual} != "
                 f"{binary_provenance.get('sha256')}")

    build = wrapper.get("build_identity")
    _require(isinstance(build, dict), "wrapper missing build_identity")
    for key in ("compiler", "cxx_standard", "build_type", "sanitizer_state", "lto_state",
                "standard_library", "conan_lock_sha256", "conan_references",
                "google_benchmark_version"):
        _require(key in build, f"build_identity missing required field {key}")
    _require(isinstance(build.get("compiler"), dict), "build_identity.compiler must be an object")
    _require_required_strings(build["compiler"], ["id", "version"], "build_identity.compiler")
    stdlib = build.get("standard_library")
    _require(isinstance(stdlib, dict), "build_identity.standard_library must be an object")
    _require_required_strings(stdlib, ["name", "version", "detection_status"],
                              "build_identity.standard_library")
    _require(isinstance(build.get("conan_references"), list),
             "build_identity.conan_references must be an array")

    environment = wrapper.get("environment_identity")
    _require(isinstance(environment, dict), "wrapper missing environment_identity")
    _require_required_strings(environment, ["os_name", "os_version", "architecture",
                                            "cpu_model", "logical_core_count"],
                              "environment_identity")

    m4 = wrapper.get("m4_dependency_identity")
    _require(isinstance(m4, dict), "wrapper missing m4_dependency_identity")
    _require("status" in m4, "m4_dependency_identity missing status")
    if m4.get("status") == "ON":
        _require_required_strings(m4, ["contracts_source_revision", "contracts_conan_reference",
                                       "contracts_recipe_revision", "contracts_package_id",
                                       "protobuf_runtime_version", "protobuf_runtime_rrev"],
                                  "m4_dependency_identity")
    elif m4.get("status") == "OFF":
        _require(m4.get("reason") == "not_applicable_core_only_payload",
                 "Core-only payload must record explicit not_applicable")
    else:
        _fail(f"invalid m4_dependency_identity status: {m4.get('status')}")

    measurement = wrapper.get("measurement_identity")
    _require(isinstance(measurement, dict), "wrapper missing measurement_identity")
    _require_required_strings(measurement, ["timer", "primary_denominator"],
                              "measurement_identity")
    warmup = measurement.get("warmup")
    _require(isinstance(warmup, dict), "measurement_identity missing warmup")
    _require_required_strings(warmup, ["kind"], "measurement_identity.warmup")
    _require(isinstance(measurement.get("repetitions"), int)
             and measurement["repetitions"] >= 1,
             "measurement_identity missing/invalid repetitions")

    payload = wrapper.get("result_payload")
    _require(isinstance(payload, dict), "wrapper missing result_payload")
    _require_required_strings(payload, ["path", "sha256", "schema"], "result_payload")
    payload_path = payload.get("path")
    if os.path.isabs(payload_path):
        resolved = payload_path
    else:
        resolved = os.path.normpath(os.path.join(os.getcwd(), payload_path))
    _require(os.path.isfile(resolved), f"result payload file not found: {resolved}")
    actual = _sha256_file(resolved)
    _require(actual == payload.get("sha256"),
             f"result payload SHA mismatch: {actual} != {payload.get('sha256')}")

    if require_inventory:
        validate_inventory(wrapper)


def _nearest_rank(sorted_samples: list[int], probability: float) -> Optional[int]:
    if not sorted_samples or probability <= 0.0 or probability > 1.0:
        return None
    rank = int(math.ceil(probability * len(sorted_samples)))
    if rank < 1:
        return None
    return sorted_samples[rank - 1]


def validate_latency(path: str, wrapper_path: str, latency: dict[str, Any],
                     wrapper: dict[str, Any]) -> None:
    _require(latency.get("schema") == LATENCY_SCHEMA,
             f"unknown latency schema: {latency.get('schema')}")
    _require(latency.get("measurement_contract_version") == MEASUREMENT_CONTRACT,
             "latency payload missing measurement_contract_version")
    _require(latency.get("quantile_estimator") == QUANTILE_ESTIMATOR,
             "latency quantile_estimator must be nearest-rank-v1")
    _require(latency.get("timer", {}).get("type") == "steady_clock",
             "latency timer must be steady_clock")
    _require(latency.get("timer", {}).get("primary_denominator") == "wall_time",
             "latency primary denominator must be wall_time")

    workload = latency.get("workload")
    _require(isinstance(workload, dict), "latency payload missing workload")
    _require_required_strings(workload, ["workload_id", "canonical_log_sha256",
                                         "workload_spec_sha256", "workload_spec_text"],
                              "latency workload")
    recomputed_spec = hashlib.sha256(workload["workload_spec_text"].encode("utf-8")).hexdigest()
    _require(recomputed_spec == workload["workload_spec_sha256"],
             "latency workload_spec_sha256 mismatch")
    event_count = workload.get("event_count")
    _require(isinstance(event_count, int) and event_count > 0, "latency workload event_count")

    passes = latency.get("passes")
    sample_count = latency.get("sample_count")
    unique_count = latency.get("unique_event_count")
    _require(isinstance(passes, int) and passes >= 1, "latency passes invalid")
    _require(isinstance(sample_count, int) and sample_count == passes * event_count,
             "latency sample_count != passes * event_count")
    _require(isinstance(unique_count, int) and unique_count == event_count,
             "latency unique_event_count must equal the workload event count")

    raw = latency.get("raw_samples_ns")
    _require(isinstance(raw, list) and len(raw) == sample_count,
             "latency raw_samples_ns count mismatch")
    _require(all(isinstance(value, int) and value >= 0 for value in raw),
             "latency raw samples must be non-negative integers")
    sorted_samples = sorted(raw)
    _require(sorted_samples == raw,
             "latency raw_samples_ns must be stored ascending for auditability")
    quantiles = latency.get("quantiles_ns")
    _require(isinstance(quantiles, dict), "latency payload missing quantiles_ns")
    for probability, key in ((0.5, "p50"), (0.9, "p90"), (0.99, "p99"), (0.999, "p99_9")):
        expected = _nearest_rank(sorted_samples, probability)
        reported = quantiles.get(key)
        eligibility = latency.get("eligibility", {}).get(key)
        if eligibility in (True, "true"):
            _require(reported == expected,
                     f"latency {key} does not match nearest-rank-v1: {reported} != {expected}")
        elif reported is not None:
            _fail(f"latency {key} reported without eligibility")
    eligibility = latency.get("eligibility", {})
    _require(isinstance(eligibility, dict), "latency payload missing eligibility")
    if not eligibility.get("p99_9") and not eligibility.get("p99_9_reason"):
        _fail("latency p99.9 omission must record a reason")

    checksum = latency.get("checksum")
    _require(isinstance(checksum, dict), "latency payload missing checksum")
    _require_required_strings(checksum, ["methodology_version"], "latency checksum")
    _require(checksum.get("validated") is True, "latency checksum not validated")
    per_pass = checksum.get("per_pass")
    _require(isinstance(per_pass, list) and len(per_pass) == passes,
             "latency checksum per_pass count mismatch")
    _require(all(value == checksum.get("expected") for value in per_pass),
             "latency pass checksum does not match expected")

    calibration = latency.get("calibration")
    _require(isinstance(calibration, dict), "latency payload missing calibration")
    _require(calibration.get("subtracted_from_event_samples") is False,
             "calibration must never be subtracted from event samples")
    calibration_raw = calibration.get("calibration_samples_ns")
    _require(isinstance(calibration_raw, list) and calibration_raw,
             "latency calibration samples missing")

    # The wrapper must bind this exact latency payload.
    _require(wrapper.get("result_payload", {}).get("schema") == LATENCY_SCHEMA,
             "wrapper must bind a latency payload")
    payload_sha = wrapper.get("result_payload", {}).get("sha256")
    _require(payload_sha == _sha256_file(os.path.abspath(path)),
             "wrapper payload binding does not match the latency payload file")
    validate_wrapper(wrapper_path, wrapper, allow_exploratory=True, binary=None,
                     require_inventory=False)


def summarize(wrappers: list[tuple[str, dict[str, Any]]]) -> str:
    lines: list[str] = []
    lines.append("M5 Phase-6 benchmark evidence summary (derived from "
                 "machine-readable results)")
    for path, wrapper in wrappers:
        lines.append("")
        lines.append(f"wrapper: {path}")
        lines.append(f"  schema: {wrapper.get('schema')}")
        lines.append(f"  evidence_class: {wrapper.get('evidence_class')}")
        source = wrapper.get("source_provenance", {})
        lines.append(f"  git_sha: {source.get('git_sha')}")
        lines.append(f"  dirty_at_configure: {source.get('dirty_at_configure')}")
        binary = wrapper.get("binary_provenance", {})
        lines.append(f"  binary_sha256: {binary.get('sha256')}")
        build = wrapper.get("build_identity", {})
        compiler = build.get("compiler", {})
        stdlib = build.get("standard_library", {})
        lines.append(f"  compiler: {compiler.get('id')} {compiler.get('version')}")
        lines.append(f"  build_type: {build.get('build_type')} "
                     f"sanitizers: {build.get('sanitizer_state')} "
                     f"lto: {build.get('lto_state')}")
        lines.append(f"  stdlib: {stdlib.get('name')} {stdlib.get('version')} "
                     f"({stdlib.get('detection_status')})")
        environment = wrapper.get("environment_identity", {})
        lines.append(f"  environment: {environment.get('os_name')} "
                     f"{environment.get('os_version')} {environment.get('architecture')} "
                     f"cpu={environment.get('cpu_model')} "
                     f"cores={environment.get('logical_core_count')}")
        measurement = wrapper.get("measurement_identity", {})
        lines.append(f"  timer: {measurement.get('timer')} "
                     f"denominator: {measurement.get('primary_denominator')} "
                     f"repetitions: {measurement.get('repetitions')}")
        names = {entry.get("benchmark_name")
                 for entry in wrapper.get("workload_identities", [])}
        required = set(_required_inventory())
        if len(names) == 1:
            lines.append(f"  inventory: latency payload "
                         f"({sorted(names)[0] if names else 'none'})")
        else:
            lines.append(f"  inventory: {len(names)} workloads registered, "
                         f"{len(required & names)}/{len(required)} required present")
        measurements = wrapper.get("measurements", [])
        grouped: dict[str, list[dict]] = {}
        for entry in measurements:
            grouped.setdefault(entry.get("name", ""), []).append(entry)
        replay_names = sorted(
            name for name in grouped
            if name.startswith(("CoreNormalizedReplay", "AdapterWireReplay")))

        def events_per_iteration(entry_name: str) -> float:
            # The dispatched-event count per iteration is recorded in the
            # workload identity (event_count=... of the replay fixture).
            for identity in wrapper.get("workload_identities", []):
                if identity.get("benchmark_name") == entry_name:
                    match = re.search(r"event_count=(\d+)",
                                      identity.get("canonical_spec_text", ""))
                    if match:
                        return float(match.group(1))
            return 2048.0

        for name in replay_names:
            entries = grouped[name]
            events = events_per_iteration(name)
            ns_per_events = [
                entry.get("real_time_ns", 0.0) / events for entry in entries
            ]
            mean_ns = sum(ns_per_events) / len(ns_per_events) if ns_per_events else 0.0
            median_ns = sorted(ns_per_events)[len(ns_per_events) // 2] if ns_per_events else 0.0
            stddev = math.sqrt(
                sum((value - mean_ns) ** 2 for value in ns_per_events) / len(ns_per_events)
            ) if ns_per_events else 0.0
            cv = (stddev / mean_ns * 100.0) if mean_ns else 0.0
            rates = [entry.get("items_per_second", 0.0) for entry in entries]
            mean_rate = sum(rates) / len(rates) if rates else 0.0
            lines.append(
                f"  {name}: repetitions={len(entries)} events/s={mean_rate:.1f} "
                f"ns/event mean={mean_ns:.0f} median={median_ns:.0f} "
                f"stddev={stddev:.0f} CV={cv:.1f}%")
        payload = wrapper.get("result_payload", {})
        lines.append(f"  payload: {payload.get('path')} sha256={payload.get('sha256')}")
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description="M5 Phase-6 benchmark validation")
    subparsers = parser.add_subparsers(dest="mode", required=True)

    inventory_parser = subparsers.add_parser("validate-inventory")
    inventory_parser.add_argument("wrapper_json")
    inventory_parser.add_argument("payload_json", nargs="?")

    smoke_parser = subparsers.add_parser("validate-smoke")
    smoke_parser.add_argument("payload_json")
    smoke_parser.add_argument("wrapper_json")
    smoke_parser.add_argument("--repetitions", type=int, default=1)

    wrapper_parser = subparsers.add_parser("validate-wrapper")
    wrapper_parser.add_argument("wrapper_json")
    wrapper_parser.add_argument("--binary")
    wrapper_parser.add_argument("--allow-exploratory", action="store_true")

    latency_parser = subparsers.add_parser("validate-latency")
    latency_parser.add_argument("latency_json")
    latency_parser.add_argument("wrapper_json")

    summarize_parser = subparsers.add_parser("summarize")
    summarize_parser.add_argument("wrapper_json", nargs="+")

    args = parser.parse_args()
    try:
        if args.mode == "validate-inventory":
            wrapper = _load_json(args.wrapper_json, "wrapper")
            validate_wrapper(args.wrapper_json, wrapper, allow_exploratory=True, binary=None,
                             require_inventory=True)
            names = validate_inventory(wrapper)
            print(f"inventory PASS: {len(names)} registered workloads, "
                  f"{len(_required_inventory())} required present")
            if args.payload_json:
                payload = _load_json(args.payload_json, "payload")
                validate_payload_structure(payload)
                print("payload structure PASS")
        elif args.mode == "validate-smoke":
            payload = _load_json(args.payload_json, "payload")
            wrapper = _load_json(args.wrapper_json, "wrapper")
            validate_wrapper(args.wrapper_json, wrapper, allow_exploratory=True, binary=None,
                             require_inventory=False)
            validate_smoke(payload, wrapper, args.repetitions)
            print(f"smoke PASS: {len(payload['benchmarks'])} benchmarks executed, "
                  "expected set matched exactly, no skip/error records")
        elif args.mode == "validate-wrapper":
            wrapper = _load_json(args.wrapper_json, "wrapper")
            validate_wrapper(args.wrapper_json, wrapper, args.allow_exploratory, args.binary)
            print("wrapper PASS: schema, provenance, build/environment identity, "
                  "workload inventory, payload SHA binding")
        elif args.mode == "validate-latency":
            latency = _load_json(args.latency_json, "latency")
            wrapper = _load_json(args.wrapper_json, "wrapper")
            validate_latency(args.latency_json, args.wrapper_json, latency, wrapper)
            print("latency PASS: nearest-rank-v1 recomputation, eligibility rules, "
                  "sample/pass consistency, checksum, wrapper binding")
        elif args.mode == "summarize":
            loaded = [(path, _load_json(path, "wrapper")) for path in args.wrapper_json]
            sys.stdout.write(summarize(loaded))
        return 0
    except ValidationError as error:
        print(f"VALIDATION FAILED: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
