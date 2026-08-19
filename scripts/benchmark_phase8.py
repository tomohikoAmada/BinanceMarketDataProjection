#!/usr/bin/env python3
"""Fail-closed validation for M5 Phase-8 candidate evidence."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
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
GIT_SHA = re.compile(r"^[0-9a-fA-F]{40}([0-9a-fA-F]{24})?$")
HEX64 = re.compile(r"^[0-9a-f]{64}$")
METRIC_UNITS = {
    "update_latency": "ns",
    "full_replacement_latency": "ns",
    "top_n_read_latency": "ns",
    "replay_update_throughput": "updates_per_second",
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


def canonical_fields(text: str, field: str) -> dict[str, str]:
    fields: dict[str, str] = {}
    for line in text.splitlines():
        require("=" in line, f"{field} contains malformed line")
        key, value = line.split("=", 1)
        require(key and key not in fields, f"{field} contains duplicate field {key}")
        fields[key] = value
    return fields


def require_sha(value: Any, field: str, pattern: re.Pattern[str]) -> str:
    require(isinstance(value, str) and pattern.fullmatch(value) is not None,
            f"invalid {field}")
    return value


def validate_stats(values: list[Any], stats: dict[str, Any], field: str) -> None:
    require(values, f"{field}.raw is empty")
    require(isinstance(stats, dict), f"{field}.summary is not an object")
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


def validate_wrapper(wrapper_path: Path, payload_path: Path) -> tuple[dict[str, Any], dict[str, dict[str, Any]]]:
    wrapper = load(wrapper_path)
    require(isinstance(wrapper, dict), "wrapper is not an object")
    require(wrapper.get("schema") == WRAPPER_SCHEMA, "wrong wrapper schema")
    require(wrapper.get("measurement_contract_version") == "M5_PHASE6_MEASUREMENT_CONTRACT_V1",
            "wrong wrapper measurement contract")
    source = wrapper.get("source_provenance")
    require(isinstance(source, dict) and source.get("status") in {"known", "unavailable"},
            "missing source provenance")
    require_sha(source.get("git_sha"), "source SHA", GIT_SHA)
    result = wrapper.get("result_payload")
    require(isinstance(result, dict), "missing result payload binding")
    require(result.get("path") == str(payload_path), "wrapper payload path mismatch")
    expected_sha = hashlib.sha256(payload_path.read_bytes()).hexdigest()
    require(result.get("sha256") == expected_sha, "wrapper payload SHA mismatch")
    require(result.get("schema") == PAYLOAD_SCHEMA, "wrapper payload schema mismatch")
    binary = wrapper.get("binary_provenance")
    require(isinstance(binary, dict) and isinstance(binary.get("path"), str) and
            binary.get("path"),
            "missing binary provenance")
    require_sha(binary.get("sha256"), "binary SHA", HEX64)
    if Path(binary["path"]).is_file():
        require(hashlib.sha256(Path(binary["path"]).read_bytes()).hexdigest() == binary.get("sha256"),
                "binary SHA mismatch")
    workloads = wrapper.get("workload_identities")
    require(isinstance(workloads, list) and workloads, "missing workload identities")
    identities: dict[str, dict[str, Any]] = {}
    for workload in workloads:
        require(isinstance(workload, dict), "invalid wrapper workload identity")
        workload_id = workload.get("benchmark_name")
        require(isinstance(workload_id, str) and workload_id, "invalid wrapper workload ID")
        require(workload_id not in identities, f"duplicate wrapper workload ID: {workload_id}")
        require(workload.get("workload_spec_schema") == WORKLOAD_SCHEMA,
                "wrong workload schema in wrapper")
        text = workload.get("canonical_spec_text")
        spec_sha = require_sha(workload.get("workload_spec_sha256"),
                               f"{workload_id} workload spec SHA", HEX64)
        require(isinstance(text, str) and canonical_sha(text) == spec_sha,
                "workload spec SHA mismatch")
        fields = canonical_fields(text, f"wrapper workload {workload_id}")
        require(fields.get("benchmark_name") == workload_id,
                f"wrapper workload {workload_id} benchmark_name mismatch")
        generated_sha = require_sha(workload.get("generated_workload_sha256"),
                                    f"{workload_id} generated workload SHA", HEX64)
        require(fields.get("generated_workload_sha256") == generated_sha,
                f"wrapper workload {workload_id} generated workload SHA mismatch")
        identities[workload_id] = {
            "spec_sha": spec_sha,
            "generated_sha": generated_sha,
            "fields": fields,
        }
    return wrapper, identities


def validate(payload_path: Path, wrapper_path: Path) -> None:
    wrapper, wrapper_workloads = validate_wrapper(wrapper_path, payload_path)
    payload = load(payload_path)
    require(isinstance(payload, dict), "payload is not an object")
    require(payload.get("schema") == PAYLOAD_SCHEMA, "wrong Phase-8 payload schema")
    require(payload.get("measurement_contract_version") == CONTRACT, "wrong Phase-8 contract")
    candidates = payload.get("candidate_models")
    require(isinstance(candidates, list) and set(candidates) == CANDIDATES and len(candidates) == 4,
            "candidate set is not exactly the approved four models")
    repetitions = uint(payload.get("repetitions"), "payload repetitions")
    require(repetitions >= 5, "formal comparison requires at least five repetitions")
    calibration = payload.get("timer_overhead_calibration")
    require(isinstance(calibration, dict) and calibration.get("timer") == "steady_clock" and
            calibration.get("unit") == "ns", "missing timer calibration identity")
    validate_stats(calibration.get("raw", []), calibration.get("summary", {}),
                   "timer_overhead_calibration")
    empirical = payload.get("empirical_noise_floor")
    require(isinstance(empirical, dict) and
            empirical.get("method") == "unchanged_control_repeated_measurements_v1" and
            empirical.get("baseline_candidate_model_id") == "phase8-std-map-control-v1",
            "missing empirical noise-floor identity")
    empirical_samples = empirical.get("samples")
    require(isinstance(empirical_samples, list), "invalid empirical noise-floor samples")

    wrapper_ids = set(wrapper_workloads)
    records = payload.get("records")
    require(isinstance(records, list) and records, "missing records")
    seen: set[tuple[str, str]] = set()
    digests: dict[str, str] = {}
    record_workload_ids: set[str] = set()
    workload_specs: dict[str, tuple[str, str]] = {}
    control_identity: dict[str, tuple[str, str, list[float]]] = {}
    for index, record in enumerate(records):
        prefix = f"record[{index}]"
        require(isinstance(record, dict), f"{prefix}: record is not an object")
        require(record.get("schema") == RECORD_SCHEMA, f"{prefix}: wrong record schema")
        candidate = record.get("candidate_model_id")
        workload_id = record.get("workload_id")
        require(candidate in CANDIDATES, f"{prefix}: unknown candidate")
        require(isinstance(workload_id, str) and workload_id in wrapper_ids,
                f"{prefix}: unknown workload")
        record_workload_ids.add(workload_id)
        key = (candidate, workload_id)
        require(key not in seen, f"{prefix}: duplicate candidate/workload cell")
        seen.add(key)
        identity = wrapper_workloads[workload_id]
        require(record.get("workload_spec_schema") == WORKLOAD_SCHEMA,
                f"{prefix}: invalid workload spec schema")
        spec_sha = require_sha(record.get("workload_spec_sha256"),
                               f"{prefix} spec SHA", HEX64)
        generated_sha = require_sha(record.get("generated_workload_sha256"),
                                    f"{prefix} generated workload SHA", HEX64)
        require((spec_sha, generated_sha) ==
                (identity["spec_sha"], identity["generated_sha"]),
                f"{prefix}: workload identity mismatch")
        workload_specs[workload_id] = (spec_sha, generated_sha)
        require(uint(record.get("repetitions"), f"{prefix}.repetitions") == repetitions,
                f"{prefix}: repetition mismatch")
        metric = record.get("metric")
        require(metric in METRIC_UNITS, f"{prefix}: unknown metric")
        require(record.get("unit") == METRIC_UNITS[metric],
                f"{prefix}: metric/unit mismatch")
        fields = identity["fields"]
        operation = record.get("operation")
        expected_operation = fields.get("operation")
        if operation == "top_levels":
            require(expected_operation == f"top_levels/{record.get('query_limit')}",
                    f"{prefix}: top-level operation identity mismatch")
            require(metric == "top_n_read_latency", f"{prefix}: top-level metric mismatch")
        else:
            require(expected_operation == operation, f"{prefix}: operation identity mismatch")
            expected_metric = {
                "apply_level": "update_latency",
                "apply_updates": "replay_update_throughput",
                "replace_all": "full_replacement_latency",
            }.get(operation)
            require(metric == expected_metric, f"{prefix}: operation/metric mismatch")
        require(uint(record.get("depth_per_side"), f"{prefix}.depth_per_side") ==
                int(fields.get("depth_per_side", "-1")), f"{prefix}: depth identity mismatch")
        batch = uint(record.get("batch"), f"{prefix}.batch")
        if operation == "apply_updates":
            require(batch == int(fields.get("batch", "-1")),
                    f"{prefix}: batch identity mismatch")
        else:
            require(batch == 0, f"{prefix}: unexpected batch")
        query_limit = uint(record.get("query_limit"), f"{prefix}.query_limit")
        if operation == "top_levels":
            require(query_limit == int(fields.get("query_limit", "-1")),
                    f"{prefix}: query-limit identity mismatch")
        else:
            require(query_limit == 0, f"{prefix}: unexpected query limit")
        measurement = record.get("measurement")
        require(isinstance(measurement, dict), f"{prefix}: missing measurement")
        validate_stats(measurement.get("raw", []), measurement.get("summary", {}), f"{prefix}.measurement")
        require(len(measurement["raw"]) == repetitions, f"{prefix}: raw repetition mismatch")
        if candidate == "phase8-std-map-control-v1":
            control_identity[workload_id] = (
                metric,
                record["unit"],
                [finite(value, f"{prefix}.measurement.raw") for value in measurement["raw"]],
            )
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
    require(record_workload_ids == wrapper_ids, "payload workload set does not match wrapper")
    require(len(seen) == len(wrapper_ids) * len(CANDIDATES),
            "not every candidate consumed every workload")
    for workload_id in wrapper_ids:
        cells = {candidate for candidate, item in seen if item == workload_id}
        require(cells == CANDIDATES, f"missing or extra candidate cells for {workload_id}")
    empirical_ids: set[str] = set()
    for index, sample in enumerate(empirical_samples):
        prefix = f"empirical_noise_floor.samples[{index}]"
        require(isinstance(sample, dict), f"{prefix}: invalid sample")
        workload_id = sample.get("workload_id")
        require(isinstance(workload_id, str) and workload_id in wrapper_ids,
                f"{prefix}: unknown workload")
        require(workload_id not in empirical_ids, f"{prefix}: duplicate workload")
        empirical_ids.add(workload_id)
        control = control_identity.get(workload_id)
        require(control is not None, f"{prefix}: missing control record")
        control_metric, control_unit, control_raw = control
        require(sample.get("metric") == control_metric,
                f"{prefix}: empirical metric is not bound to control record")
        require(sample.get("unit") == control_unit,
                f"{prefix}: empirical unit is not bound to control record")
        validate_stats(sample.get("raw", []), sample.get("summary", {}), prefix)
        require(len(sample["raw"]) == repetitions, f"{prefix}: repetition mismatch")
        require(sample["raw"] == control_raw,
                f"{prefix}: does not bind to control measurements")
    require(empirical_ids == wrapper_ids, "empirical noise floor workload set mismatch")
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
