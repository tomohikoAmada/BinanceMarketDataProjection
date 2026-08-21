#!/usr/bin/env python3
"""M5 Phase-7 allocation measurement validation.

Fail-closed independent validators for the Phase-7 machine-readable outputs
(OD-M5-P7-016):

  - M5_ALLOCATION_WRAPPER_V1 wrapper schema, provenance, and payload binding
  - M5_PHASE7_ALLOCATION_RECORD_V1 per-cell records
  - M5_PHASE7_FOOTPRINT_RECORD_V1 footprint records
  - A/P/B invariants and normalized-metric recomputation
  - exact rational replay per-event values (no integer division)
  - required inventory per measurement executable kind
  - process-level determinism comparison of two payload documents
  - wrapper/record evidence-identity binding: every allocation and footprint
    record (and every calibration record's evidence_class) must describe the
    SAME evidence identity as the validated wrapper (M5-P7-PRB-003)
  - --allow-exploratory policy: exploratory evidence fails closed unless the
    flag is given; formal evidence never requires it

The required inventory is defined independently here; it never reads a
producer-generated "expected inventory". The validator fails closed on any
schema, invariant, identity, or eligibility violation.

Usage:
  python3 scripts/benchmark_phase7.py validate-records PAYLOAD WRAPPER
      [--binary PATH] [--require-inventory {m2_m3,m4,replay,footprint}]
      [--allow-exploratory]
  python3 scripts/benchmark_phase7.py check-determinism PAYLOAD_A PAYLOAD_B
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import sys
from typing import Any, Optional

WRAPPER_SCHEMA = "M5_ALLOCATION_WRAPPER_V1"
MEASUREMENT_CONTRACT = "M5_PHASE7_MEASUREMENT_CONTRACT_V1"
RECORD_SCHEMA = "M5_PHASE7_ALLOCATION_RECORD_V1"
FOOTPRINT_SCHEMA = "M5_PHASE7_FOOTPRINT_RECORD_V1"
PAYLOAD_SCHEMA = "M5_PHASE7_ALLOCATION_PAYLOAD_V1"
WORKLOAD_SPEC_SCHEMA = "M5_BENCHMARK_WORKLOAD_SPEC_V1"
ALLOCATION_BOUNDARY = "cxx_replaceable_global_new"

HEX64 = re.compile(r"^[0-9a-f]{64}$")
GIT_SHA = re.compile(r"^[0-9a-fA-F]{40}([0-9a-fA-F]{24})?$")
CONAN_PACKAGE_ID = re.compile(r"^[0-9a-f]{40}$")

ROUTINE_DEPTHS = [8, 100, 1000]
FULL_DEPTH_SET = [0, 8, 100, 1000, 5000, 10000]
BATCH_SET = [1, 10, 100]
M3_DEPTH_SET = [0, 8, 100, 1000, 5000, 10000]
M3_BATCH_SET = [0, 1, 10, 100]
TOP_N_SET = [1, 5, 50]
M4_DEPTH_SET = [8, 100, 1000]
M4_ADAPT_DEPTH_UPDATE_CARDINALITY = 10
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
FOOTPRINT_DEPTHS = [100, 1000, 5000, 10000]
REPLAY_NAMES = [
    "CoreNormalizedReplay/Spot",
    "CoreNormalizedReplay/UsdMPerpetual",
    "AdapterWireReplay/Spot",
    "AdapterWireReplay/UsdMPerpetual",
]

# Cells whose production implementation provably allocates nothing; a nonzero
# result fails the cell closed (OD-M5-P7-008/009).
ZERO_ALLOCATION_PREFIXES = (
    "M2/best_bid/", "M2/best_ask/", "M2/quantity_at/hit/", "M2/quantity_at/miss/",
    "M3/Classification/Stale/", "M3/Classification/Duplicate/",
    "M3/Classification/Gap/",
)

# Owning-output cells: the post-destroy lifecycle check D == A is normative
# (the owning result's destruction must return live bytes to the bracket
# entry point; OD-M5-P7-005).
OWNING_OUTPUT_PREFIXES = (
    "M2/all_levels/", "M2/top_levels/", "M4/AdaptExchangeDepthSnapshot/",
    "M4/AdaptDepthUpdate/", "M4/MakeLocalOrderBookSnapshot/",
    "M4/SerializeSnapshot/FreshBuffer/",
)

REPLAY_PREFIXES = ("CoreNormalizedReplay/", "AdapterWireReplay/")


class ValidationError(Exception):
    pass


def _fail(message: str) -> None:
    raise ValidationError(message)


def _require(condition: bool, message: str) -> None:
    if not condition:
        _fail(message)


def _load_json(path: str, description: str) -> Any:
    if not os.path.isfile(path):
        _fail(f"{description} not found: {path}")
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


def _canonical_text(obj: Any) -> str:
    return json.dumps(obj, sort_keys=True, separators=(",", ":"), ensure_ascii=False)


def _canonical_fields(canonical: str, name: str) -> dict[str, str]:
    fields: dict[str, str] = {}
    for line in canonical.splitlines():
        _require("=" in line, f"workload {name} has malformed canonical field")
        key, value = line.split("=", 1)
        _require(bool(key) and key not in fields,
                 f"workload {name} has duplicate/empty canonical key")
        fields[key] = value
    return fields


def _uint(value: Any, description: str) -> int:
    _require(isinstance(value, int) and not isinstance(value, bool) and value >= 0,
             f"{description} must be a non-negative integer, got {value!r}")
    return value


# ---------------------------------------------------------------------------
# Required inventory (defined independently of any producer output).
# ---------------------------------------------------------------------------
def m2_required_inventory() -> list[str]:
    required: list[str] = []
    for family in ["insert", "update", "delete", "missing_delete"]:
        for depth in ROUTINE_DEPTHS:
            required.append(f"M2/apply_level/{family}/{depth}")
    for batch in BATCH_SET:
        for depth in ROUTINE_DEPTHS:
            required.append(f"M2/apply_updates/{batch}/{depth}")
    for depth in FULL_DEPTH_SET:
        required.append(f"M2/replace_all/{depth}")
    for depth in FULL_DEPTH_SET:
        required.append(f"M2/all_levels/{depth}")
    for limit in TOP_N_SET:
        for depth in ROUTINE_DEPTHS:
            required.append(f"M2/top_levels/{limit}/{depth}")
    for name in ["best_bid", "best_ask", "quantity_at/hit", "quantity_at/miss"]:
        for depth in ROUTINE_DEPTHS:
            required.append(f"M2/{name}/{depth}")
    return required


def m3_required_inventory() -> list[str]:
    required: list[str] = []
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
    return required


def m4_required_inventory() -> list[str]:
    required: list[str] = []
    for family in M4_FAMILIES:
        if family == "AdaptDepthUpdate/Spot":
            required.append(f"M4/{family}/{M4_ADAPT_DEPTH_UPDATE_CARDINALITY}")
        else:
            required.extend(f"M4/{family}/{depth}" for depth in M4_DEPTH_SET)
    return required


def footprint_required_inventory() -> list[str]:
    return [f"M5_Footprint/Depth/{depth}" for depth in FOOTPRINT_DEPTHS] + [
        "M5_Footprint/FixedObject"
    ]


def required_inventory(kind: str) -> list[str]:
    if kind == "m2_m3":
        return m2_required_inventory() + m3_required_inventory()
    if kind == "m4":
        return m4_required_inventory()
    if kind == "replay":
        return list(REPLAY_NAMES)
    if kind == "footprint":
        return footprint_required_inventory()
    _fail(f"unknown executable kind: {kind}")


# ---------------------------------------------------------------------------
# Validated wrapper identity context (M5-P7-PRB-003).
# ---------------------------------------------------------------------------
# Payload hashes prove the bytes of a payload document are internally bound;
# they cannot prove that a record mixed in from another source/binary/build/
# environment actually belongs to the validated wrapper. Record validation
# therefore carries this immutable context and requires every allocation and
# footprint record to describe the SAME evidence identity as the wrapper.
RECORD_BUILD_IDENTITY_KEYS = (
    "compiler", "cxx_standard", "build_type", "sanitizer_state", "lto_state",
    "standard_library", "conan_lock_sha256",
)


def _validation_context(wrapper: dict[str, Any],
                        identities: dict[str, dict[str, str]]) -> dict[str, Any]:
    return {
        "evidence_class": wrapper["evidence_class"],
        "source_provenance": wrapper["source_provenance"],
        "binary_provenance": wrapper["binary_provenance"],
        "build_identity": wrapper["build_identity"],
        "environment_identity": wrapper["environment_identity"],
        "m4_dependency_identity": wrapper["m4_dependency_identity"],
        "workload_identities": identities,
    }


def _require_identity_binding(record: dict[str, Any], scope: str,
                              context: dict[str, Any]) -> None:
    """Require a record to describe the exact validated wrapper evidence
    identity (evidence_class plus source/binary/build/environment provenance
    and M4 dependency identity)."""

    evidence_class = record.get("evidence_class")
    _require(evidence_class == context["evidence_class"],
             f"record {scope} evidence_class {evidence_class!r} differs from the wrapper "
             f"evidence_class {context['evidence_class']!r}")
    provenance = record.get("provenance")
    _require(isinstance(provenance, dict), f"record {scope} missing provenance")
    expected: dict[str, Any] = {
        "source": context["source_provenance"],
        "binary": context["binary_provenance"],
        "build": {key: context["build_identity"][key] for key in RECORD_BUILD_IDENTITY_KEYS},
        "environment": context["environment_identity"],
        "m4_dependency_identity": context["m4_dependency_identity"],
    }
    for block, wrapper_block in (("source", "source_provenance"),
                                 ("binary", "binary_provenance"),
                                 ("build", "build_identity"),
                                 ("environment", "environment_identity"),
                                 ("m4_dependency_identity", "m4_dependency_identity")):
        _require(isinstance(provenance.get(block), dict),
                 f"record {scope} provenance missing {block}")
        _require(provenance[block] == expected[block],
                 f"record {scope} provenance.{block} does not describe the same evidence "
                 f"identity as the wrapper {wrapper_block}")


# ---------------------------------------------------------------------------
# Wrapper validation.
# ---------------------------------------------------------------------------
def validate_wrapper(path: str, wrapper: dict[str, Any], allow_exploratory: bool,
                     binary: Optional[str], payload_path: str) -> dict[str, Any]:
    """Validate the wrapper and return the immutable validated identity
    context that every record must bind to (M5-P7-PRB-003)."""
    _require(wrapper.get("schema") == WRAPPER_SCHEMA,
             f"unknown wrapper schema: {wrapper.get('schema')}")
    _require(wrapper.get("measurement_contract_version") == MEASUREMENT_CONTRACT,
             "missing/wrong measurement_contract_version")
    for key in ("evidence_class", "requested_evidence_class"):
        _require(isinstance(wrapper.get(key), str) and wrapper[key],
                 f"wrapper missing required field {key}")

    source = wrapper.get("source_provenance")
    _require(isinstance(source, dict), "wrapper missing source_provenance")
    _require(isinstance(source.get("git_sha"), str) and source["git_sha"],
             "source_provenance missing git_sha")
    _require(isinstance(source.get("status"), str) and source["status"],
             "source_provenance missing status")
    _require(isinstance(source.get("dirty_at_configure"), bool),
             "source_provenance missing dirty_at_configure")
    evidence_class = wrapper.get("evidence_class")
    _require(evidence_class in ("formal", "exploratory"), "invalid evidence_class")
    if evidence_class == "exploratory" and not allow_exploratory:
        _fail("exploratory evidence requires --allow-exploratory")
    if source.get("dirty_at_configure") and evidence_class == "formal":
        _fail("dirty source cannot produce formal evidence")
    if evidence_class == "formal":
        _require(source.get("status") == "known",
                 "formal evidence requires known source provenance")
        _require(GIT_SHA.fullmatch(source["git_sha"]) is not None,
                 "formal evidence requires a valid Git SHA")

    binary_provenance = wrapper.get("binary_provenance")
    _require(isinstance(binary_provenance, dict), "wrapper missing binary_provenance")
    _require(isinstance(binary_provenance.get("path"), str) and binary_provenance["path"],
             "binary_provenance missing path")
    _require(isinstance(binary_provenance.get("sha256"), str)
             and HEX64.fullmatch(binary_provenance["sha256"]) is not None,
             "binary_provenance missing sha256")
    if binary is not None:
        actual = _sha256_file(binary)
        _require(actual == binary_provenance.get("sha256"),
                 f"binary provenance SHA mismatch: {actual} != {binary_provenance.get('sha256')}")

    build = wrapper.get("build_identity")
    _require(isinstance(build, dict), "wrapper missing build_identity")
    for key in ("compiler", "cxx_standard", "build_type", "sanitizer_state", "lto_state",
                "standard_library", "conan_lock_sha256", "conan_references",
                "google_benchmark_version"):
        _require(key in build, f"build_identity missing required field {key}")
    _require(isinstance(build.get("compiler"), dict), "build_identity.compiler must be an object")
    stdlib = build.get("standard_library")
    _require(isinstance(stdlib, dict), "build_identity.standard_library must be an object")
    for key in ("name", "version", "detection_status"):
        _require(isinstance(stdlib.get(key), str) and stdlib[key],
                 f"build_identity.standard_library missing {key}")
    _require(isinstance(build.get("conan_references"), list),
             "build_identity.conan_references must be an array")

    environment = wrapper.get("environment_identity")
    _require(isinstance(environment, dict), "wrapper missing environment_identity")
    for key in ("os_name", "os_version", "architecture", "cpu_model", "logical_core_count"):
        _require(isinstance(environment.get(key), str) and environment[key],
                 f"environment_identity missing {key}")

    m4 = wrapper.get("m4_dependency_identity")
    _require(isinstance(m4, dict), "wrapper missing m4_dependency_identity")
    _require("status" in m4, "m4_dependency_identity missing status")
    if m4.get("status") == "ON":
        for key in ("contracts_source_revision", "contracts_conan_reference",
                    "contracts_recipe_revision", "contracts_package_id",
                    "protobuf_runtime_version", "protobuf_runtime_rrev"):
            _require(isinstance(m4.get(key), str) and m4[key],
                     f"m4_dependency_identity missing {key}")
        package_id = m4.get("contracts_package_id")
        _require(isinstance(package_id, str) and CONAN_PACKAGE_ID.fullmatch(package_id) is not None,
                 f"contracts_package_id must be a real 40-hex Conan package ID, "
                 f"got {package_id!r}")
    elif m4.get("status") == "OFF":
        _require(m4.get("reason") == "not_applicable_core_only_payload",
                 "Core-only payload must record explicit not_applicable")
    else:
        _fail(f"invalid m4_dependency_identity status: {m4.get('status')}")

    instrument = wrapper.get("allocation_instrumentation_identity")
    _require(isinstance(instrument, dict),
             "wrapper missing allocation_instrumentation_identity")
    _require(instrument.get("allocation_boundary") == ALLOCATION_BOUNDARY,
             "wrapper allocation_boundary must be exactly cxx_replaceable_global_new")
    _require(isinstance(instrument.get("provenance_capacity"), int)
             and instrument["provenance_capacity"] > 0,
             "wrapper provenance_capacity must be a positive integer")

    measurement = wrapper.get("measurement_identity")
    _require(isinstance(measurement, dict), "wrapper missing measurement_identity")
    warmup = measurement.get("warmup")
    _require(isinstance(warmup, dict), "measurement_identity missing warmup")
    _require(warmup.get("kind") == "explicit_workload_pass_v1" and warmup.get("count") == 1,
             "measurement identity requires one explicit workload-equivalent warmup")
    _require(isinstance(measurement.get("repetitions"), int)
             and measurement["repetitions"] >= 1,
             "measurement_identity missing/invalid repetitions")

    identities = _workload_names(wrapper)

    payload = wrapper.get("result_payload")
    _require(isinstance(payload, dict), "wrapper missing result_payload")
    for key in ("path", "sha256", "schema"):
        _require(isinstance(payload.get(key), str) and payload[key],
                 f"result_payload missing {key}")
    _require(payload.get("schema") == PAYLOAD_SCHEMA,
             f"result_payload schema must be {PAYLOAD_SCHEMA}")
    resolved = payload["path"] if os.path.isabs(payload["path"]) else os.path.normpath(
        os.path.join(os.getcwd(), payload["path"]))
    _require(os.path.isfile(resolved), f"result payload file not found: {resolved}")
    actual = _sha256_file(resolved)
    _require(actual == payload.get("sha256"),
             f"result payload SHA mismatch: {actual} != {payload.get('sha256')}")
    _require(os.path.realpath(resolved) == os.path.realpath(payload_path),
             "wrapper result_payload path disagrees with the validated payload file")
    return _validation_context(wrapper, identities)


def _workload_names(wrapper: dict[str, Any]) -> dict[str, dict[str, str]]:
    identities = wrapper.get("workload_identities")
    _require(isinstance(identities, list), "wrapper missing workload_identities")
    result: dict[str, dict[str, str]] = {}
    for entry in identities:
        name = entry.get("benchmark_name")
        _require(isinstance(name, str) and name, "workload identity missing benchmark_name")
        _require(name not in result, f"workload {name} registered twice")
        _require(entry.get("workload_spec_schema") == WORKLOAD_SPEC_SCHEMA,
                 f"workload {name} has wrong workload_spec_schema")
        spec_sha = entry.get("workload_spec_sha256")
        canonical = entry.get("canonical_spec_text")
        _require(isinstance(spec_sha, str) and HEX64.fullmatch(spec_sha) is not None,
                 f"workload {name} missing workload_spec_sha256")
        _require(isinstance(canonical, str) and canonical,
                 f"workload {name} missing canonical_spec_text")
        recomputed = hashlib.sha256(canonical.encode("utf-8")).hexdigest()
        _require(recomputed == spec_sha,
                 f"workload {name} workload_spec_sha256 does not match its canonical text")
        fields = _canonical_fields(canonical, name)
        for key in ("generator_schema", "generated_workload_sha256"):
            _require(isinstance(fields.get(key), str) and fields[key],
                     f"workload {name} missing canonical field {key}")
        generated = fields["generated_workload_sha256"]
        _require(HEX64.fullmatch(generated) is not None
                 and generated not in {"0" * 64, "f" * 64},
                 f"workload {name} has invalid/placeholder generated workload SHA")
        _require(entry.get("generator_schema") == fields["generator_schema"],
                 f"workload {name} wrapper generator_schema disagrees with canonical spec")
        _require(entry.get("generated_workload_sha256") == generated,
                 f"workload {name} wrapper generated_workload_sha256 disagrees with canonical spec")
        fields["__canonical_text"] = canonical
        result[name] = fields
    return result


# ---------------------------------------------------------------------------
# Record validation.
# ---------------------------------------------------------------------------
def _recompute_persistent_delta(before: int, after: int) -> dict[str, Any]:
    if after > before:
        return {"sign": "positive", "magnitude": after - before}
    if after == before:
        return {"sign": "zero", "magnitude": 0}
    return {"sign": "negative", "magnitude": before - after}


def validate_measurement_result(record: dict[str, Any], scope: str) -> None:
    for key in ("allocation_count", "total_allocated_bytes", "deallocation_count"):
        _uint(record.get(key), f"record {scope} missing {key}")
    validity = record.get("metric_validity")
    _require(isinstance(validity, dict), f"record {scope} missing metric_validity")
    for key in ("allocation_count_valid", "total_allocated_bytes_valid",
                "deallocation_count_valid", "deallocated_bytes_valid"):
        _require(isinstance(validity.get(key), bool), f"record {scope} metric_validity.{key}")
    if validity["allocation_count_valid"]:
        _uint(record.get("allocation_count"), f"record {scope} allocation_count")
    if validity["deallocated_bytes_valid"]:
        _uint(record.get("deallocated_bytes"), f"record {scope} deallocated_bytes")
    else:
        _require("deallocated_bytes" not in record or record.get("deallocated_bytes") is None,
                 f"record {scope} reports deallocated_bytes while invalid")

    _require(record.get("operation_aborted") is False,
             f"record {scope} operation_aborted")
    _require(record.get("allocation_failure_observed") is False,
             f"record {scope} allocation_failure_observed")
    _require(record.get("determinism_confirmed") is True,
             f"record {scope} determinism_confirmed must be true")
    repetitions = record.get("repetitions")
    _require(isinstance(repetitions, int) and repetitions >= 1,
             f"record {scope} repetitions must be >= 1")

    eligibility = record.get("live_metric_eligibility")
    _require(eligibility is not None, f"record {scope} missing live_metric_eligibility")
    if eligibility == "eligible":
        before = _uint(record.get("live_bytes_before"), f"record {scope} live_bytes_before")
        peak = _uint(record.get("peak_live_bytes_absolute"),
                     f"record {scope} peak_live_bytes_absolute")
        after = _uint(record.get("live_bytes_after"), f"record {scope} live_bytes_after")
        _require(peak >= before, f"record {scope} violates P >= A")
        _require(peak >= after, f"record {scope} violates P >= B")
        peak_above = _uint(record.get("peak_above_entry"), f"record {scope} peak_above_entry")
        transient = _uint(record.get("transient_excess_over_persistent"),
                          f"record {scope} transient_excess_over_persistent")
        _require(peak_above == peak - before,
                 f"record {scope} peak_above_entry != P - A")
        ceiling = max(before, after)
        _require(transient == peak - ceiling,
                 f"record {scope} transient_excess_over_persistent != P - max(A, B)")
        reported_delta = record.get("persistent_live_delta")
        _require(isinstance(reported_delta, dict),
                 f"record {scope} missing persistent_live_delta")
        expected_delta = _recompute_persistent_delta(before, after)
        _require(reported_delta == expected_delta,
                 f"record {scope} persistent_live_delta {reported_delta!r} != "
                 f"recomputed {expected_delta!r}")
    elif isinstance(eligibility, dict):
        _require(eligibility.get("status") == "ineligible",
                 f"record {scope} malformed live_metric_eligibility")
        _require(isinstance(eligibility.get("reason_code"), str) and eligibility["reason_code"],
                 f"record {scope} ineligible without reason code")
        for key in ("live_bytes_before", "peak_live_bytes_absolute", "live_bytes_after",
                    "persistent_live_delta", "peak_above_entry",
                    "transient_excess_over_persistent"):
            _require(record.get(key) is None,
                     f"record {scope} substitutes a value for ineligible metric {key}")
    else:
        _fail(f"record {scope} invalid live_metric_eligibility")

    lifecycle_status = record.get("post_destroy_lifecycle_status")
    _require(isinstance(lifecycle_status, str) and lifecycle_status,
             f"record {scope} missing post_destroy_lifecycle_status")
    is_owning_output = scope.startswith(OWNING_OUTPUT_PREFIXES) and \
        not scope.startswith(REPLAY_PREFIXES)
    if is_owning_output:
        _require(lifecycle_status != "not_applicable",
                 f"record {scope} owning-output cell missing its post-destroy lifecycle")
    has_post_destroy = record.get("post_destroy_live_bytes") is not None
    if lifecycle_status == "not_applicable":
        _require(not has_post_destroy,
                 f"record {scope} reports post-destroy bytes with not_applicable lifecycle")
    elif lifecycle_status == "destroyed":
        _require(has_post_destroy,
                 f"record {scope} destroyed lifecycle without post_destroy_live_bytes")
        _uint(record.get("post_destroy_live_bytes"), f"record {scope} post_destroy_live_bytes")
        if is_owning_output:
            _require(record.get("live_metric_eligibility") == "eligible"
                     and record["post_destroy_live_bytes"] == record["live_bytes_before"],
                     f"record {scope} owning-output lifecycle: post-destroy D must equal A")
    elif lifecycle_status.startswith("destroyed_mismatch:"):
        _fail(f"record {scope} post-destroy lifecycle mismatch is a run failure")
    elif lifecycle_status.startswith("retained:"):
        _require(isinstance(record.get("post_destroy_live_bytes"), int)
                 or record.get("post_destroy_live_bytes") is None,
                 f"record {scope} malformed retained lifecycle")
    else:
        _fail(f"record {scope} unknown post_destroy_lifecycle_status {lifecycle_status}")

    if scope.startswith(ZERO_ALLOCATION_PREFIXES):
        _require(record.get("allocation_count") == 0
                 and record.get("total_allocated_bytes") == 0,
                 f"record {scope} is a zero-allocation control but reports traffic")
    if scope.startswith("M3/Classification/Reset/"):
        _require(record.get("allocation_count") == 0,
                 f"record {scope} reset must be deallocation-only")

    calibration = record.get("calibration_record")
    _require(isinstance(calibration, dict), f"record {scope} missing calibration_record")
    _require(isinstance(calibration.get("reference"), str) and calibration["reference"],
             f"record {scope} calibration_record missing reference")
    _require(calibration.get("subtracted") is False,
             f"record {scope} calibration must never be subtracted")


def validate_record(record: dict[str, Any], context: dict[str, Any],
                    calibration_ids: set[str]) -> None:
    _require(record.get("schema") == RECORD_SCHEMA,
             f"unknown record schema: {record.get('schema')}")
    _require(record.get("measurement_contract_version") == MEASUREMENT_CONTRACT,
             f"record missing measurement_contract_version")
    scope = record.get("measurement_scope")
    _require(isinstance(scope, str) and scope, "record missing measurement_scope")
    _require(record.get("allocation_boundary") == ALLOCATION_BOUNDARY,
             f"record {scope} allocation_boundary must be exactly cxx_replaceable_global_new")
    _require(record.get("evidence_class") in ("formal", "exploratory"),
             f"record {scope} invalid evidence_class")
    _require(isinstance(record.get("operation_denominator"), str)
             and record["operation_denominator"],
             f"record {scope} missing operation_denominator")

    identities = context["workload_identities"]

    workload_id = record.get("workload_id")
    _require(isinstance(workload_id, str) and workload_id,
             f"record {scope} missing workload_id")
    fields = identities.get(workload_id)
    _require(fields is not None,
             f"record {scope} workload_id {workload_id} lacks a wrapper workload identity")
    spec_sha = record.get("workload_spec_sha256")
    _require(isinstance(spec_sha, str) and HEX64.fullmatch(spec_sha) is not None,
             f"record {scope} missing workload_spec_sha256")
    canonical_text = fields.get("__canonical_text", "")
    recomputed_spec = hashlib.sha256(canonical_text.encode("utf-8")).hexdigest()
    _require(spec_sha == recomputed_spec,
             f"record {scope} workload_spec_sha256 does not match the wrapper "
             f"workload identity")
    _require(record.get("generator_schema") == fields["generator_schema"],
             f"record {scope} generator_schema disagrees with its workload identity")
    _require(record.get("generated_workload_sha256") == fields["generated_workload_sha256"],
             f"record {scope} generated_workload_sha256 disagrees with its workload identity")

    fixture = record.get("fixture_identity")
    _require(isinstance(fixture, dict), f"record {scope} missing fixture_identity")
    seed = fixture.get("seed")
    _require(isinstance(seed, str) and seed
             and (seed == "not_applicable" or seed.isdigit()),
             f"record {scope} has invalid seed")
    canonical_log = fixture.get("canonical_log_sha256")
    _require(isinstance(canonical_log, str) and canonical_log,
             f"record {scope} missing canonical_log_sha256")

    baseline = record.get("baseline_definition")
    _require(isinstance(baseline, dict), f"record {scope} missing baseline_definition")
    for key in ("snapshot_a", "snapshot_b", "delta_formula"):
        _require(isinstance(baseline.get(key), str) and baseline[key],
                 f"record {scope} baseline_definition missing {key}")

    validate_measurement_result(record, scope)

    calibration = record["calibration_record"]
    _require(calibration["reference"] in calibration_ids,
             f"record {scope} references unknown calibration record "
             f"{calibration['reference']!r}")

    _require_identity_binding(record, scope, context)

    _check_no_completeness_overclaim(record, scope)

    result_sha = record.get("result_payload_sha256")
    _require(isinstance(result_sha, str) and HEX64.fullmatch(result_sha) is not None,
             f"record {scope} missing result_payload_sha256")
    canonical = _canonical_text(_canonical_result_object(record, scope))
    _require(hashlib.sha256(canonical.encode("utf-8")).hexdigest() == result_sha,
             f"record {scope} result_payload_sha256 does not match its canonical result")

    if scope.startswith(REPLAY_PREFIXES):
        _validate_replay_aggregate(record, scope, fields)

    # proxy/component cells stay labelled diagnostics; no decomposition claim
    # may exist anywhere in a record.
    for key in record:
        if "decomposition" in key or "exact_breakdown" in key:
            _fail(f"record {scope} carries a forbidden decomposition claim field {key}")


def _validate_replay_aggregate(record: dict[str, Any], scope: str,
                               fields: dict[str, str]) -> None:
    aggregate = record.get("replay_aggregate")
    _require(isinstance(aggregate, dict), f"record {scope} missing replay_aggregate")
    event_count = _uint(aggregate.get("event_count"), f"record {scope} event_count")
    _require(event_count > 0, f"record {scope} event_count must be positive")
    for key in ("aggregate_allocation_count", "aggregate_allocated_bytes",
                "aggregate_deallocation_count", "aggregate_deallocated_bytes"):
        _uint(aggregate.get(key), f"record {scope} {key}")
    # Exact rationals: numerator = aggregate_total, denominator = event_count;
    # no divisibility requirement, no integer division (case 27).
    for rational_key, aggregate_key in (
            ("derived_per_event_allocations", "aggregate_allocation_count"),
            ("derived_per_event_bytes", "aggregate_allocated_bytes")):
        rational = aggregate.get(rational_key)
        _require(isinstance(rational, dict), f"record {scope} missing {rational_key}")
        numerator = _uint(rational.get("numerator"), f"record {scope} {rational_key}.numerator")
        denominator = _uint(rational.get("denominator"),
                            f"record {scope} {rational_key}.denominator")
        _require(denominator > 0, f"record {scope} {rational_key}.denominator must be > 0")
        _require(denominator == event_count,
                 f"record {scope} {rational_key}.denominator must equal event_count")
        _require(numerator == aggregate[aggregate_key],
                 f"record {scope} {rational_key}.numerator must equal {aggregate_key}")
    canonical_log = record["fixture_identity"]["canonical_log_sha256"]
    _require(fields.get("canonical_log_sha256") == canonical_log,
             f"record {scope} canonical_log_sha256 disagrees with its workload identity")
    identity_text = fields.get("workload_identity", "")
    _require(f"event_count={event_count}" in identity_text,
             f"record {scope} event_count disagrees with its workload identity")


def _canonical_result_object(record: dict[str, Any], scope: str) -> dict[str, Any]:
    obj: dict[str, Any] = {
        "allocation_count": record.get("allocation_count"),
        "allocation_count_valid": record["metric_validity"]["allocation_count_valid"],
        "allocation_failure_observed": record.get("allocation_failure_observed"),
        "deallocated_bytes": record.get("deallocated_bytes"),
        "deallocated_bytes_valid": record["metric_validity"]["deallocated_bytes_valid"],
        "deallocation_count": record.get("deallocation_count"),
        "deallocation_count_valid": record["metric_validity"]["deallocation_count_valid"],
        "determinism_confirmed": record.get("determinism_confirmed"),
        "live_bytes_after": record.get("live_bytes_after"),
        "live_bytes_before": record.get("live_bytes_before"),
        "live_metric_eligibility": record.get("live_metric_eligibility"),
        "operation_aborted": record.get("operation_aborted"),
        "peak_above_entry": record.get("peak_above_entry"),
        "peak_live_bytes_absolute": record.get("peak_live_bytes_absolute"),
        "persistent_live_delta": record.get("persistent_live_delta"),
        "post_destroy_lifecycle_status": record.get("post_destroy_lifecycle_status"),
        "post_destroy_live_bytes": record.get("post_destroy_live_bytes"),
    }
    if "replay_aggregate" in record:
        obj["replay_aggregate"] = record["replay_aggregate"]
    obj["repetitions"] = record.get("repetitions")
    obj["total_allocated_bytes"] = record.get("total_allocated_bytes")
    obj["total_allocated_bytes_valid"] = record["metric_validity"]["total_allocated_bytes_valid"]
    obj["transient_excess_over_persistent"] = record.get("transient_excess_over_persistent")
    return obj


def _check_no_completeness_overclaim(record: dict[str, Any], scope: str) -> None:
    for key, value in record.items():
        if "heap_complete" in key or "complete_heap" in key:
            _fail(f"record {scope} carries a heap-completeness claim field {key}")
        if key == "rss":
            _require(value == "not_measured", f"record {scope} rss must be not_measured")


def validate_footprint_record(record: dict[str, Any], calibration_ids: set[str],
                              context: dict[str, Any]) -> None:
    _require(record.get("schema") == FOOTPRINT_SCHEMA,
             f"unknown footprint schema: {record.get('schema')}")
    _require(record.get("measurement_contract_version") == MEASUREMENT_CONTRACT,
             f"footprint record missing measurement_contract_version")
    scope = record.get("measurement_scope")
    _require(isinstance(scope, str) and scope, "footprint record missing measurement_scope")
    _require_identity_binding(record, scope, context)
    _require(record.get("allocation_boundary") == ALLOCATION_BOUNDARY,
             f"footprint record {scope} allocation_boundary must be exactly "
             f"cxx_replaceable_global_new")
    depth = record.get("depth_per_side")
    if scope != "M5_Footprint/FixedObject":
        _require(isinstance(depth, int) and depth > 0,
                 f"footprint record {scope} invalid depth_per_side")
        _require(depth in FOOTPRINT_DEPTHS,
                 f"footprint record {scope} depth not in the accepted depth set")
    snapshots = record.get("snapshots")
    _require(isinstance(snapshots, dict), f"footprint record {scope} missing snapshots")
    for key in ("pre_experiment_baseline_live_bytes", "empty_book_live_bytes",
                "bids_only_live_bytes", "both_sides_live_bytes", "post_destroy_live_bytes"):
        _uint(snapshots.get(key), f"footprint record {scope} snapshots.{key}")
    empty = snapshots["empty_book_live_bytes"]
    bids = snapshots["bids_only_live_bytes"]
    both = snapshots["both_sides_live_bytes"]
    _require(bids >= empty and both >= bids,
             f"footprint record {scope} snapshots are not monotone")
    total = _uint(record.get("measured_requested_heap_bytes_total"),
                  f"footprint record {scope} measured_requested_heap_bytes_total")
    bids_bytes = _uint(record.get("measured_requested_heap_bytes_per_side_bids"),
                       f"footprint record {scope} measured bytes per side bids")
    asks_bytes = _uint(record.get("measured_requested_heap_bytes_per_side_asks"),
                       f"footprint record {scope} measured bytes per side asks")
    _require(total == both - empty,
             f"footprint record {scope} total delta != both - empty snapshot delta")
    _require(bids_bytes == bids - empty,
             f"footprint record {scope} bids delta != bids - empty snapshot delta")
    _require(asks_bytes == both - bids,
             f"footprint record {scope} asks delta != both - bids snapshot delta")
    if scope != "M5_Footprint/FixedObject":
        for key in ("measured_bytes_per_level_per_side_bids",
                    "measured_bytes_per_level_per_side_asks"):
            rational = record.get(key)
            _require(isinstance(rational, dict), f"footprint record {scope} missing {key}")
            numerator = _uint(rational.get("numerator"), f"footprint record {scope} {key}")
            denominator = _uint(rational.get("denominator"), f"footprint record {scope} {key}")
            _require(denominator == depth, f"footprint record {scope} {key}.denominator != depth")
            expected_numerator = bids_bytes if "bids" in key else asks_bytes
            _require(numerator == expected_numerator,
                     f"footprint record {scope} {key}.numerator != measured side delta")
    # Model separation: measured vs non-additive node model vs estimated
    # allocator model vs RSS — never mixed into an additive total.
    node_model = record.get("node_structural_model")
    _require(isinstance(node_model, dict), f"footprint record {scope} missing node model")
    _require(node_model.get("non_additive") is True,
             f"footprint record {scope} node model must be non-additive")
    _require(isinstance(node_model.get("description"), str) and node_model["description"],
             f"footprint record {scope} node model missing description")
    _require(isinstance(node_model.get("toolchain"), str) and node_model["toolchain"],
             f"footprint record {scope} node model missing toolchain")
    backing_model = record.get("allocator_backing_model")
    _require(isinstance(backing_model, dict), f"footprint record {scope} missing backing model")
    _require(backing_model.get("evidence_class") == "estimated",
             f"footprint record {scope} allocator model must be evidence_class estimated")
    _require(record.get("rss") == "not_measured",
             f"footprint record {scope} rss must be not_measured")
    for key in record:
        if ("measured" in key and "model" in key) or "combined_total" in key or \
                "additive_total" in key:
            _fail(f"footprint record {scope} mixes measured and modeled quantities ({key})")
    eligibility = record.get("eligibility")
    _require(isinstance(eligibility, dict), f"footprint record {scope} missing eligibility")
    status = eligibility.get("status")
    if status == "ineligible":
        _require(isinstance(eligibility.get("reason_code"), str) and eligibility["reason_code"],
                 f"footprint record {scope} ineligible without reason code")
        _require(record.get("determinism_confirmed") is True,
                 f"footprint record {scope} determinism_confirmed must be true")
    else:
        _require(status == "eligible", f"footprint record {scope} invalid eligibility")
    lifecycle = record.get("post_destroy_lifecycle_status")
    _require(isinstance(lifecycle, str) and lifecycle,
             f"footprint record {scope} missing post_destroy_lifecycle_status")
    if lifecycle == "consistent":
        _require(snapshots["post_destroy_live_bytes"]
                 == snapshots["pre_experiment_baseline_live_bytes"],
                 f"footprint record {scope} post-destroy snapshot must equal the "
                 f"pre-experiment baseline")
    else:
        _fail(f"footprint record {scope} post-destroy lifecycle status {lifecycle}")
    _require(record.get("determinism_confirmed") is True,
             f"footprint record {scope} determinism_confirmed must be true")
    repetitions = record.get("repetitions")
    _require(isinstance(repetitions, int) and repetitions >= 1,
             f"footprint record {scope} repetitions must be >= 1")
    calibration = record.get("calibration_record")
    _require(isinstance(calibration, dict), f"footprint record {scope} missing calibration_record")
    _require(calibration.get("subtracted") is False,
             f"footprint record {scope} calibration must never be subtracted")
    _require(calibration.get("reference") in calibration_ids,
             f"footprint record {scope} references unknown calibration record")
    result_sha = record.get("result_payload_sha256")
    _require(isinstance(result_sha, str) and HEX64.fullmatch(result_sha) is not None,
             f"footprint record {scope} missing result_payload_sha256")
    canonical = _canonical_text({
        "bids_only_live_bytes": bids,
        "both_sides_live_bytes": both,
        "depth_per_side": record.get("depth_per_side"),
        "determinism_confirmed": record.get("determinism_confirmed"),
        "eligible": status == "eligible",
        "empty_book_live_bytes": empty,
        "post_destroy_live_bytes": snapshots["post_destroy_live_bytes"],
        "post_destroy_lifecycle_status": lifecycle,
        "pre_experiment_baseline_live_bytes":
            snapshots["pre_experiment_baseline_live_bytes"],
        "repetitions": repetitions,
    })
    _require(hashlib.sha256(canonical.encode("utf-8")).hexdigest() == result_sha,
             f"footprint record {scope} result_payload_sha256 does not match its canonical result")


def validate_payload_document(payload: dict[str, Any], context: dict[str, Any],
                              require_inventory: Optional[str]) -> list[str]:
    _require(payload.get("schema") == PAYLOAD_SCHEMA,
             f"unknown payload schema: {payload.get('schema')}")
    _require(payload.get("measurement_contract_version") == MEASUREMENT_CONTRACT,
             "payload missing measurement_contract_version")
    records = payload.get("records")
    _require(isinstance(records, list) and records, "payload missing records array")
    _require(isinstance(payload.get("record_count"), int)
             and payload["record_count"] == len(records),
             "payload record_count mismatch")
    calibrations = payload.get("calibration_records")
    _require(isinstance(calibrations, list) and calibrations,
             "payload missing calibration_records array")
    _require(isinstance(payload.get("calibration_record_count"), int)
             and payload["calibration_record_count"] == len(calibrations),
             "payload calibration_record_count mismatch")

    calibration_ids: set[str] = set()
    for calibration in calibrations:
        _require(isinstance(calibration.get("calibration_id"), str)
                 and calibration["calibration_id"],
                 "calibration record missing calibration_id")
        _require(calibration.get("allocation_boundary") == ALLOCATION_BOUNDARY,
                 "calibration record allocation_boundary must be cxx_replaceable_global_new")
        _require(calibration.get("subtracted_from_measurements") is False,
                 "calibration record must never be subtracted")
        _require(calibration.get("evidence_class") == context["evidence_class"],
                 f"calibration record {calibration.get('calibration_id')} evidence_class "
                 f"{calibration.get('evidence_class')!r} differs from the wrapper "
                 f"evidence_class {context['evidence_class']!r}")
        calibration_ids.add(calibration["calibration_id"])

    identities = context["workload_identities"]
    scopes: list[str] = []
    has_allocation_records = False
    for record in records:
        if record.get("schema") == FOOTPRINT_SCHEMA:
            validate_footprint_record(record, calibration_ids, context)
        else:
            has_allocation_records = True
            validate_record(record, context, calibration_ids)
        scope = record.get("measurement_scope")
        _require(scope not in scopes, f"duplicate record scope {scope}")
        scopes.append(scope)
    if has_allocation_records:
        _require(bool(identities),
                 "payload with allocation records requires wrapper workload identities")

    if require_inventory is not None:
        required = required_inventory(require_inventory)
        missing = [name for name in required if name not in scopes]
        unexpected = [name for name in scopes if name not in required]
        if missing or unexpected:
            _fail(f"{require_inventory} inventory mismatch: missing={missing} "
                  f"unexpected={unexpected}")
        if require_inventory == "m2_m3":
            accepted = [name for name in scopes
                        if name.startswith("M3/LiveApply/Accepted/")]
            _require(len(accepted) == 48,
                     f"expected 48 M3 accepted cells, got {len(accepted)}")
    return scopes


# ---------------------------------------------------------------------------
# Process-level determinism comparison (OD-M5-P7-015).
# ---------------------------------------------------------------------------
def normalized_signature(record: dict[str, Any]) -> Optional[tuple]:
    if record.get("live_metric_eligibility") == "eligible":
        delta = record["persistent_live_delta"]
        live = (record["peak_above_entry"],
                record["transient_excess_over_persistent"],
                delta["sign"], delta["magnitude"])
    else:
        live = (record.get("live_metric_eligibility"),)
    counters = (record.get("allocation_count"), record.get("total_allocated_bytes"),
                record.get("deallocation_count"), record.get("deallocated_bytes"))
    return counters + live


def footprint_signature(record: dict[str, Any]) -> Optional[tuple]:
    snapshots = record.get("snapshots")
    if not isinstance(snapshots, dict):
        return None
    return (record.get("depth_per_side"),
            snapshots.get("empty_book_live_bytes"),
            snapshots.get("bids_only_live_bytes"),
            snapshots.get("both_sides_live_bytes"),
            snapshots.get("post_destroy_live_bytes"))


def check_determinism(payload_a: dict[str, Any], payload_b: dict[str, Any]) -> None:
    def signatures(payload: dict[str, Any]) -> dict[str, tuple]:
        result: dict[str, tuple] = {}
        for record in payload.get("records", []):
            scope = record.get("measurement_scope")
            if record.get("schema") == FOOTPRINT_SCHEMA:
                signature = footprint_signature(record)
            else:
                signature = normalized_signature(record)
            if scope is None or signature is None:
                continue
            result[scope] = signature
        return result

    first = signatures(payload_a)
    second = signatures(payload_b)
    _require(set(first) == set(second),
             f"determinism scope sets differ: only_a={sorted(set(first) - set(second))} "
             f"only_b={sorted(set(second) - set(first))}")
    mismatches: list[str] = []
    for scope in first:
        if first[scope] != second[scope]:
            mismatches.append(f"{scope}: {first[scope]} != {second[scope]}")
    if mismatches:
        _fail("normalized metric mismatch across process invocations: "
              + "; ".join(mismatches[:5]) + ("..." if len(mismatches) > 5 else ""))


# ---------------------------------------------------------------------------
# Entry points.
# ---------------------------------------------------------------------------
def main_run_for_test(payload_path: str, wrapper_path: str,
                      require_inventory: Optional[str] = None,
                      binary: Optional[str] = None,
                      allow_exploratory: bool = True) -> None:
    """Programmatic entrypoint used by the deterministic validator tests."""
    payload = _load_json(payload_path, "payload")
    wrapper = _load_json(wrapper_path, "wrapper")
    context = validate_wrapper(wrapper_path, wrapper, allow_exploratory=allow_exploratory,
                               binary=binary, payload_path=payload_path)
    validate_payload_document(payload, context, require_inventory)


def main() -> int:
    parser = argparse.ArgumentParser(description="M5 Phase-7 allocation validation")
    subparsers = parser.add_subparsers(dest="mode", required=True)

    records_parser = subparsers.add_parser("validate-records")
    records_parser.add_argument("payload_json")
    records_parser.add_argument("wrapper_json")
    records_parser.add_argument("--binary")
    records_parser.add_argument("--require-inventory",
                                choices=["m2_m3", "m4", "replay", "footprint"])
    records_parser.add_argument("--allow-exploratory", action="store_true")

    determinism_parser = subparsers.add_parser("check-determinism")
    determinism_parser.add_argument("payload_a")
    determinism_parser.add_argument("payload_b")

    args = parser.parse_args()
    try:
        if args.mode == "validate-records":
            payload = _load_json(args.payload_json, "payload")
            wrapper = _load_json(args.wrapper_json, "wrapper")
            context = validate_wrapper(args.wrapper_json, wrapper, args.allow_exploratory,
                                       args.binary, args.payload_json)
            scopes = validate_payload_document(payload, context, args.require_inventory)
            print(f"records PASS: {len(scopes)} records validated, wrapper/provenance/payload "
                  "binding, A/P/B invariants, exact rationals, calibration separation")
            if args.require_inventory:
                print(f"inventory PASS ({args.require_inventory}): "
                      f"{len(required_inventory(args.require_inventory))} required cells present")
        elif args.mode == "check-determinism":
            payload_a = _load_json(args.payload_a, "payload A")
            payload_b = _load_json(args.payload_b, "payload B")
            check_determinism(payload_a, payload_b)
            print("determinism PASS: normalized metrics identical across separate "
                  "process invocations")
        return 0
    except ValidationError as error:
        print(f"VALIDATION FAILED: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
