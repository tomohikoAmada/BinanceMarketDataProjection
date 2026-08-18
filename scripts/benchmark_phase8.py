#!/usr/bin/env python3
"""Fail-closed validation for M5 Phase-8 candidate evidence."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import statistics
import sys
from pathlib import Path
from typing import Any

PAYLOAD_SCHEMA = "M5_PHASE8_EVIDENCE_PAYLOAD_V1"
RECORD_SCHEMA = "M5_PHASE8_MEASUREMENT_RECORD_V1"
CONTRACT = "M5_PHASE8_MEASUREMENT_CONTRACT_V1"
WRAPPER_SCHEMA = "M5_BENCHMARK_WRAPPER_V1"
WORKLOAD_SCHEMA = "M5_BENCHMARK_WORKLOAD_SPEC_V1"
BOUNDARY = "cxx_replaceable_global_new"
CANDIDATES = {
    "phase8-std-map-control-v1",
    "phase8-sorted-vector-naive-v1",
    "phase8-absl-btree-map-v1",
    "phase8-sorted-vector-batch-lww-v1",
}


def fail(message: str) -> None:
    raise ValueError(message)


def no_duplicate_pairs(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            fail(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def load(path: Path) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=no_duplicate_pairs)
    except (OSError, json.JSONDecodeError) as exc:
        fail(f"cannot load {path}: {exc}")


def require(condition: bool, message: str) -> None:
    if not condition:
        fail(message)


def uint(value: Any, field: str) -> int:
    require(isinstance(value, int) and not isinstance(value, bool) and value >= 0, f"invalid {field}")
    return value


def finite(value: Any, field: str) -> float:
    require(isinstance(value, (int, float)) and not isinstance(value, bool), f"invalid {field}")
    result = float(value)
    require(math.isfinite(result) and result > 0.0, f"invalid non-positive/non-finite {field}")
    return result


def finite_nonnegative(value: Any, field: str) -> float:
    require(isinstance(value, (int, float)) and not isinstance(value, bool), f"invalid {field}")
    result = float(value)
    require(math.isfinite(result) and result >= 0.0, f"invalid negative/non-finite {field}")
    return result


def canonical_sha(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def validate_stats(values: list[Any], stats: dict[str, Any], field: str) -> None:
    require(values, f"{field}.raw is empty")
    raw = [finite(value, f"{field}.raw") for value in values]
    for key in ("mean", "median", "minimum", "maximum", "standard_deviation", "coefficient_of_variation"):
        finite_nonnegative(stats.get(key), f"{field}.summary.{key}")
    expected_mean = statistics.fmean(raw)
    expected_median = statistics.median(raw)
    expected_minimum = min(raw)
    expected_maximum = max(raw)
    expected_stddev = statistics.pstdev(raw)
    expected_cv = expected_stddev / expected_mean if expected_mean else 0.0
    expected = {
        "mean": expected_mean,
        "median": expected_median,
        "minimum": expected_minimum,
        "maximum": expected_maximum,
        "standard_deviation": expected_stddev,
        "coefficient_of_variation": expected_cv,
    }
    for key, value in expected.items():
        require(math.isclose(float(stats[key]), value, rel_tol=1e-9, abs_tol=1e-6),
                f"{field}.summary.{key} does not reconstruct from raw values")


def validate_wrapper(wrapper_path: Path, payload_path: Path) -> dict[str, Any]:
    wrapper = load(wrapper_path)
    require(wrapper.get("schema") == WRAPPER_SCHEMA, "wrong wrapper schema")
    require(wrapper.get("measurement_contract_version") == "M5_PHASE6_MEASUREMENT_CONTRACT_V1",
            "wrong wrapper measurement contract")
    source = wrapper.get("source_provenance")
    require(isinstance(source, dict) and source.get("status") in {"known", "unavailable"},
            "missing source provenance")
    require(isinstance(source.get("git_sha"), str) and len(source["git_sha"]) in {40, 64},
            "invalid source SHA")
    result = wrapper.get("result_payload")
    require(isinstance(result, dict), "missing result payload binding")
    require(result.get("path") == str(payload_path), "wrapper payload path mismatch")
    expected_sha = hashlib.sha256(payload_path.read_bytes()).hexdigest()
    require(result.get("sha256") == expected_sha, "wrapper payload SHA mismatch")
    require(result.get("schema") == PAYLOAD_SCHEMA, "wrapper payload schema mismatch")
    binary = wrapper.get("binary_provenance")
    require(isinstance(binary, dict) and isinstance(binary.get("path"), str),
            "missing binary provenance")
    if Path(binary["path"]).is_file():
        require(hashlib.sha256(Path(binary["path"]).read_bytes()).hexdigest() == binary.get("sha256"),
                "binary SHA mismatch")
    workloads = wrapper.get("workload_identities")
    require(isinstance(workloads, list) and workloads, "missing workload identities")
    for workload in workloads:
        require(workload.get("workload_spec_schema") == WORKLOAD_SCHEMA,
                "wrong workload schema in wrapper")
        text = workload.get("canonical_spec_text")
        require(isinstance(text, str) and canonical_sha(text) == workload.get("workload_spec_sha256"),
                "workload spec SHA mismatch")
    return wrapper


def validate(payload_path: Path, wrapper_path: Path) -> None:
    wrapper = validate_wrapper(wrapper_path, payload_path)
    payload = load(payload_path)
    require(payload.get("schema") == PAYLOAD_SCHEMA, "wrong Phase-8 payload schema")
    require(payload.get("measurement_contract_version") == CONTRACT, "wrong Phase-8 contract")
    candidates = payload.get("candidate_models")
    require(isinstance(candidates, list) and set(candidates) == CANDIDATES and len(candidates) == 4,
            "candidate set is not exactly the approved four models")
    repetitions = uint(payload.get("repetitions"), "payload repetitions")
    require(repetitions >= 5, "formal comparison requires at least five repetitions")
    noise = payload.get("noise_floor")
    require(isinstance(noise, dict) and noise.get("timer") == "steady_clock" and noise.get("unit") == "ns",
            "missing noise-floor identity")
    validate_stats(noise.get("raw", []), noise.get("summary", {}), "noise_floor")

    wrapper_ids = {item.get("benchmark_name") for item in wrapper["workload_identities"]}
    records = payload.get("records")
    require(isinstance(records, list) and records, "missing records")
    seen: set[tuple[str, str]] = set()
    digests: dict[str, str] = {}
    workload_specs: dict[str, tuple[str, str]] = {}
    for index, record in enumerate(records):
        prefix = f"record[{index}]"
        require(record.get("schema") == RECORD_SCHEMA, f"{prefix}: wrong record schema")
        candidate = record.get("candidate_model_id")
        workload_id = record.get("workload_id")
        require(candidate in CANDIDATES, f"{prefix}: unknown candidate")
        require(isinstance(workload_id, str) and workload_id in wrapper_ids, f"{prefix}: unknown workload")
        key = (candidate, workload_id)
        require(key not in seen, f"{prefix}: duplicate candidate/workload cell")
        seen.add(key)
        spec_sha = record.get("workload_spec_sha256")
        generated_sha = record.get("generated_workload_sha256")
        require(isinstance(spec_sha, str) and len(spec_sha) == 64, f"{prefix}: invalid spec SHA")
        require(isinstance(generated_sha, str) and len(generated_sha) == 64,
                f"{prefix}: invalid generated workload SHA")
        prior = workload_specs.setdefault(workload_id, (spec_sha, generated_sha))
        require(prior == (spec_sha, generated_sha), f"{prefix}: workload identity mismatch")
        require(uint(record.get("repetitions"), f"{prefix}.repetitions") == repetitions,
                f"{prefix}: repetition mismatch")
        metric = record.get("metric")
        require(metric in {"update_latency", "replay_update_throughput", "full_replacement_latency",
                           "top_n_read_latency"}, f"{prefix}: unknown metric")
        measurement = record.get("measurement")
        require(isinstance(measurement, dict), f"{prefix}: missing measurement")
        validate_stats(measurement.get("raw", []), measurement.get("summary", {}), f"{prefix}.measurement")
        require(len(measurement["raw"]) == repetitions, f"{prefix}: raw repetition mismatch")
        allocation = record.get("allocation_supporting_evidence")
        require(isinstance(allocation, dict) and allocation.get("boundary") == BOUNDARY,
                f"{prefix}: invalid allocation boundary")
        for field in ("allocation_count", "allocated_bytes"):
            values = allocation.get(field)
            require(isinstance(values, list) and len(values) == repetitions,
                    f"{prefix}: invalid {field}")
            for value in values:
                uint(value, f"{prefix}.{field}")
        memory = record.get("persistent_live_storage")
        require(isinstance(memory, dict) and memory.get("rss") == "not_measured" and
                memory.get("post_destroy_consistent") is True,
                f"{prefix}: invalid persistent-memory evidence")
        values = memory.get("measured_requested_bytes")
        require(isinstance(values, list) and len(values) == repetitions,
                f"{prefix}: invalid persistent-memory samples")
        for value in values:
            uint(value, f"{prefix}.persistent_live_storage")
        digest = record.get("final_state_digest")
        require(isinstance(digest, str) and digest, f"{prefix}: missing final-state digest")
        if workload_id in digests:
            require(digests[workload_id] == digest, f"{prefix}: final-state digest mismatch")
        else:
            digests[workload_id] = digest
    require(len(seen) == len(workload_specs) * len(CANDIDATES),
            "not every candidate consumed every workload")
    print(f"Phase-8 evidence validation PASS: {len(workload_specs)} workloads, "
          f"{len(seen)} candidate cells, {repetitions} repetitions")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("validate", choices=["validate"])
    parser.add_argument("payload", type=Path)
    parser.add_argument("wrapper", type=Path)
    args = parser.parse_args()
    try:
        validate(args.payload, args.wrapper)
    except (ValueError, OSError, KeyError, TypeError) as exc:
        print(f"PHASE8 EVIDENCE FAIL: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
