"""Deterministic fail-closed tests for the Phase-7 allocation validators.

Each case fabricates a payload/wrapper pair, corrupts exactly one aspect, and
asserts the independent validator fails closed. Valid fixtures pass first so
the negative assertions are meaningful. The validator never consumes a
producer-generated "expected inventory": all required-inventory sets are
defined inside scripts/benchmark_phase7.py.
"""

from __future__ import annotations

import hashlib
import json
import os
import sys
import tempfile
import unittest
from typing import Any

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", ".."))

import scripts.benchmark_phase7 as phase7  # type: ignore

COMPILER = {"id": "Clang", "version": "18.1.3"}
STDLIB = {"name": "libc++", "version": "190000", "detection_status": "detected_via__LIBCPP_VERSION"}
M4_OFF = {"status": "OFF", "reason": "not_applicable_core_only_payload"}


def _provenance() -> dict[str, Any]:
    return {
        "source": {"git_sha": "a" * 40, "status": "known", "dirty_at_configure": False},
        "binary": {"path": "/nonexistent/binary", "sha256": "b" * 64},
        "build": {
            "compiler": COMPILER,
            "cxx_standard": "20",
            "build_type": "Release",
            "sanitizer_state": "off",
            "lto_state": "off",
            "standard_library": STDLIB,
            "conan_lock_sha256": "c" * 64,
        },
        "environment": {
            "os_name": "macOS", "os_version": "15.0", "architecture": "arm64",
            "cpu_model": "cpu", "logical_core_count": "10",
        },
        "m4_dependency_identity": M4_OFF,
    }


def _wrapper(payload_path: str, payload_sha: str) -> dict[str, Any]:
    return {
        "schema": phase7.WRAPPER_SCHEMA,
        "measurement_contract_version": phase7.MEASUREMENT_CONTRACT,
        "evidence_class": "exploratory",
        "requested_evidence_class": "exploratory",
        "evidence_class_downgrade_reason": None,
        "source_provenance": {"git_sha": "a" * 40, "status": "known",
                              "dirty_at_configure": False},
        "binary_provenance": {"path": "/nonexistent/binary", "sha256": "b" * 64},
        "build_identity": {
            "compiler": COMPILER, "cxx_standard": "20", "build_type": "Release",
            "sanitizer_state": "off", "lto_state": "off", "standard_library": STDLIB,
            "conan_lock_sha256": "c" * 64, "conan_references": [],
            "google_benchmark_version": "1.9.5",
        },
        "environment_identity": {
            "os_name": "macOS", "os_version": "15.0", "architecture": "arm64",
            "cpu_model": "cpu", "logical_core_count": "10",
        },
        "m4_dependency_identity": M4_OFF,
        "allocation_instrumentation_identity": {
            "allocation_boundary": phase7.ALLOCATION_BOUNDARY,
            "provenance_capacity": 1 << 20,
            "tracking_model": "two_lifetime_tracking_and_measurement_v1",
            "backing_allocator": "std_malloc_std_free",
            "live_metric_model": "A_P_B_with_post_destroy_lifecycle_v1",
        },
        "measurement_identity": {
            "warmup": {"kind": "explicit_workload_pass_v1", "count": 1},
            "repetitions": 3,
            "bracket_discipline": "single_logical_operation_per_bracket",
            "preparation": "outside_measurement_bracket",
            "calibration": "reported_never_subtracted",
        },
        "workload_identities": [],
        "result_payload": {
            "path": payload_path, "sha256": payload_sha,
            "schema": phase7.PAYLOAD_SCHEMA,
        },
    }


def _workload_identity(name: str, spec: str) -> dict[str, Any]:
    fields = dict(line.split("=", 1) for line in spec.splitlines() if "=" in line)
    return {
        "benchmark_name": name,
        "workload_spec_schema": phase7.WORKLOAD_SPEC_SCHEMA,
        "workload_spec_sha256": hashlib.sha256(spec.encode("utf-8")).hexdigest(),
        "generator_schema": fields["generator_schema"],
        "generator_version": fields.get("generator_version", "1"),
        "seed": fields.get("seed", "not_applicable"),
        "generated_workload_sha256": fields["generated_workload_sha256"],
        "canonical_spec_text": spec,
    }


def _record(scope: str, workload: dict[str, Any], measurement: dict[str, Any]) -> dict[str, Any]:
    return {
        "schema": phase7.RECORD_SCHEMA,
        "measurement_contract_version": phase7.MEASUREMENT_CONTRACT,
        "evidence_class": "exploratory",
        "measurement_scope": scope,
        "operation_denominator": "operation",
        "allocation_boundary": phase7.ALLOCATION_BOUNDARY,
        "workload_id": workload["benchmark_name"],
        "workload_spec_sha256": workload["workload_spec_sha256"],
        "generator_schema": workload["generator_schema"],
        "generated_workload_sha256": workload["generated_workload_sha256"],
        "fixture_identity": {"seed": "not_applicable",
                             "canonical_log_sha256": "not_applicable",
                             "event_count": None},
        "provenance": _provenance(),
        **measurement,
    }


def _calibration() -> dict[str, Any]:
    return {
        "calibration_id": "calibration/empty-bracket-v1",
        "evidence_class": "exploratory",
        "allocation_boundary": phase7.ALLOCATION_BOUNDARY,
        "description": "empty measurement bracket",
        "allocation_count": 0,
        "total_allocated_bytes": 0,
        "deallocation_count": 0,
        "live_bytes_before": 100,
        "peak_live_bytes_absolute": 100,
        "live_bytes_after": 100,
        "live_metric_eligibility": "eligible",
        "subtracted_from_measurements": False,
    }


def _eligible_measurement(before: int, peak: int, after: int, allocs: int, bytes_: int,
                          frees: int, freed: int) -> dict[str, Any]:
    delta = phase7._recompute_persistent_delta(before, after)
    ceiling = max(before, after)
    return {
        "allocation_count": allocs,
        "total_allocated_bytes": bytes_,
        "deallocation_count": frees,
        "deallocated_bytes": freed,
        "live_bytes_before": before,
        "peak_live_bytes_absolute": peak,
        "live_bytes_after": after,
        "persistent_live_delta": delta,
        "peak_above_entry": peak - before,
        "transient_excess_over_persistent": peak - ceiling,
        "live_metric_eligibility": "eligible",
        "metric_validity": {
            "allocation_count_valid": True, "total_allocated_bytes_valid": True,
            "deallocation_count_valid": True, "deallocated_bytes_valid": True,
        },
        "operation_aborted": False,
        "allocation_failure_observed": False,
        "post_destroy_lifecycle_status": "not_applicable",
        "baseline_definition": {
            "snapshot_a": "bracket open", "snapshot_b": "bracket close",
            "delta_formula": "exact A/B comparison -> persistent_live_delta",
        },
        "calibration_record": {"reference": "calibration/empty-bracket-v1",
                               "subtracted": False},
        "repetitions": 3,
        "determinism_confirmed": True,
        "result_payload_sha256": None,
    }


def _canonical_result_object(record: dict[str, Any]) -> dict[str, Any]:
    return phase7._canonical_result_object(record, record["measurement_scope"])


def _bind_result_sha(record: dict[str, Any]) -> None:
    record["result_payload_sha256"] = hashlib.sha256(
        json.dumps(_canonical_result_object(record), sort_keys=True,
                   separators=(",", ":"), ensure_ascii=False).encode("utf-8")
    ).hexdigest()


class ValidatorTestBase(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.payload_path = os.path.join(self.tmp.name, "payload.json")
        self.wrapper_path = os.path.join(self.tmp.name, "wrapper.json")

    def write_pair(self, payload: dict[str, Any], wrapper: dict[str, Any],
                   bind: bool = True) -> None:
        if bind:
            text = json.dumps(payload)
            wrapper["result_payload"]["sha256"] = \
                hashlib.sha256(text.encode("utf-8")).hexdigest()
            with open(self.payload_path, "w", encoding="utf-8") as stream:
                stream.write(text)
        else:
            with open(self.payload_path, "w", encoding="utf-8") as stream:
                stream.write(json.dumps(payload))
        with open(self.wrapper_path, "w", encoding="utf-8") as stream:
            stream.write(json.dumps(wrapper))

    def make_simple_pair(self) -> tuple[dict[str, Any], dict[str, Any], str, dict[str, Any]]:
        spec = ("benchmark_name=M2/best_bid/8\ndepth_per_side=8\n"
                "generated_workload_sha256=d" + "e" * 63 +
                "\ngenerator_schema=M5_PHASE6_M2_CELLS_V1\noperation=best_bid\n"
                "seed=not_applicable\n")
        workload = _workload_identity("M2/best_bid/8", spec)
        measurement = _eligible_measurement(1000, 1000, 1000, 0, 0, 0, 0)
        record = _record("M2/best_bid/8", workload, measurement)
        _bind_result_sha(record)
        payload = {
            "schema": phase7.PAYLOAD_SCHEMA,
            "measurement_contract_version": phase7.MEASUREMENT_CONTRACT,
            "record_count": 1,
            "calibration_record_count": 1,
            "calibration_records": [_calibration()],
            "records": [record],
        }
        wrapper = _wrapper(self.payload_path, "")
        wrapper["workload_identities"] = [workload]
        self.write_pair(payload, wrapper)
        return payload, wrapper, spec, workload

    def expect_failure(self, payload: dict[str, Any], wrapper: dict[str, Any],
                       bind: bool = True) -> str:
        self.write_pair(payload, wrapper, bind=bind)
        with self.assertRaises(phase7.ValidationError) as context:
            phase7.main_run_for_test(self.payload_path, self.wrapper_path)
        return str(context.exception)


class WrapperValidationTests(ValidatorTestBase):
    def test_valid_pair_passes(self) -> None:
        payload, wrapper, _, _ = self.make_simple_pair()
        identities = phase7.validate_wrapper(self.wrapper_path, wrapper,
                                             allow_exploratory=True, binary=None,
                                             payload_path=self.payload_path)
        scopes = phase7.validate_payload_document(payload, identities, None)
        self.assertEqual(scopes, ["M2/best_bid/8"])

    def test_unknown_wrapper_schema_rejected(self) -> None:
        payload, wrapper, _, _ = self.make_simple_pair()
        wrapper["schema"] = "M5_UNKNOWN_WRAPPER_V9"
        self.expect_failure(payload, wrapper)

    def test_payload_sha_mismatch_rejected(self) -> None:
        payload, wrapper, _, _ = self.make_simple_pair()
        wrapper["result_payload"]["sha256"] = "0" * 64
        self.expect_failure(payload, wrapper, bind=False)

    def test_wrong_allocation_boundary_rejected(self) -> None:
        payload, wrapper, _, _ = self.make_simple_pair()
        wrapper["allocation_instrumentation_identity"]["allocation_boundary"] = \
            "malloc_total"
        self.expect_failure(payload, wrapper)

    def test_dirty_formal_source_rejected(self) -> None:
        payload, wrapper, _, _ = self.make_simple_pair()
        wrapper["evidence_class"] = "formal"
        wrapper["requested_evidence_class"] = "formal"
        wrapper["source_provenance"]["dirty_at_configure"] = True
        self.expect_failure(payload, wrapper)


class RecordValidationTests(ValidatorTestBase):
    def _pair_with_record(self, scope: str, workload_name: str,
                          measurement: dict[str, Any]) -> tuple[dict, dict]:
        spec = (f"benchmark_name={workload_name}\ndepth_per_side=8\n"
                f"generated_workload_sha256=d" + "e" * 63 +
                f"\ngenerator_schema=M5_PHASE6_M2_CELLS_V1\noperation=test\n"
                f"seed=not_applicable\n")
        workload = _workload_identity(workload_name, spec)
        record = _record(scope, workload, measurement)
        _bind_result_sha(record)
        payload = {
            "schema": phase7.PAYLOAD_SCHEMA,
            "measurement_contract_version": phase7.MEASUREMENT_CONTRACT,
            "record_count": 1,
            "calibration_record_count": 1,
            "calibration_records": [_calibration()],
            "records": [record],
        }
        wrapper = _wrapper(self.payload_path, "")
        wrapper["workload_identities"] = [workload]
        self.write_pair(payload, wrapper)
        return payload, wrapper

    def test_p_violates_a_invariant_rejected(self) -> None:
        measurement = _eligible_measurement(1000, 900, 1000, 1, 64, 1, 64)
        payload, wrapper = self._pair_with_record("M2/apply_level/insert/8",
                                                  "M2/apply_level/insert/8", measurement)
        with self.assertRaises(phase7.ValidationError) as context:
            phase7.main_run_for_test(self.payload_path, self.wrapper_path)
        self.assertIn("P >= A", str(context.exception))

    def test_peak_above_entry_recomputation_rejected(self) -> None:
        measurement = _eligible_measurement(1000, 1200, 1000, 1, 64, 0, 0)
        measurement["peak_above_entry"] = 999
        payload, wrapper = self._pair_with_record("M2/apply_level/insert/8",
                                                  "M2/apply_level/insert/8", measurement)
        with self.assertRaises(phase7.ValidationError) as context:
            phase7.main_run_for_test(self.payload_path, self.wrapper_path)
        self.assertIn("peak_above_entry", str(context.exception))

    def test_persistent_delta_sign_mismatch_rejected(self) -> None:
        measurement = _eligible_measurement(1000, 1200, 1000, 1, 64, 0, 0)
        measurement["persistent_live_delta"] = {"sign": "negative", "magnitude": 500}
        payload, wrapper = self._pair_with_record("M2/apply_level/insert/8",
                                                  "M2/apply_level/insert/8", measurement)
        with self.assertRaises(phase7.ValidationError) as context:
            phase7.main_run_for_test(self.payload_path, self.wrapper_path)
        self.assertIn("persistent_live_delta", str(context.exception))

    def test_negative_delta_is_not_an_error(self) -> None:
        measurement = _eligible_measurement(2000, 2000, 1500, 0, 0, 1, 500)
        payload, wrapper = self._pair_with_record("M2/apply_level/delete/8",
                                                  "M2/apply_level/delete/8", measurement)
        identities = phase7.validate_wrapper(self.wrapper_path, wrapper,
                                             allow_exploratory=True, binary=None,
                                             payload_path=self.payload_path)
        phase7.validate_payload_document(payload, identities, None)

    def test_ineligible_metric_substitution_rejected(self) -> None:
        measurement = _eligible_measurement(1000, 1200, 1000, 1, 64, 0, 0)
        measurement["live_metric_eligibility"] = {
            "status": "ineligible", "reason_code": "unknown_pointer_delete"}
        measurement["persistent_live_delta"] = {"sign": "zero", "magnitude": 0}
        payload, wrapper = self._pair_with_record("M2/apply_level/insert/8",
                                                  "M2/apply_level/insert/8", measurement)
        with self.assertRaises(phase7.ValidationError) as context:
            phase7.main_run_for_test(self.payload_path, self.wrapper_path)
        self.assertIn("substitutes", str(context.exception))

    def test_zero_allocation_control_with_traffic_rejected(self) -> None:
        measurement = _eligible_measurement(1000, 1000, 1000, 1, 64, 0, 0)
        payload, wrapper = self._pair_with_record("M2/best_bid/8", "M2/best_bid/8",
                                                  measurement)
        with self.assertRaises(phase7.ValidationError) as context:
            phase7.main_run_for_test(self.payload_path, self.wrapper_path)
        self.assertIn("zero-allocation control", str(context.exception))

    def test_workload_identity_sha_mismatch_rejected(self) -> None:
        payload, wrapper, _, workload = self.make_simple_pair()
        payload["records"][0]["workload_spec_sha256"] = "1" * 64
        self.expect_failure(payload, wrapper)

    def test_missing_workload_identity_rejected(self) -> None:
        payload, wrapper, _, _ = self.make_simple_pair()
        wrapper["workload_identities"] = []
        self.expect_failure(payload, wrapper)

    def test_result_payload_sha_mismatch_rejected(self) -> None:
        payload, wrapper, _, _ = self.make_simple_pair()
        payload["records"][0]["result_payload_sha256"] = "2" * 64
        self.expect_failure(payload, wrapper)

    def test_unknown_calibration_reference_rejected(self) -> None:
        payload, wrapper, _, _ = self.make_simple_pair()
        payload["records"][0]["calibration_record"]["reference"] = "missing-calibration"
        self.expect_failure(payload, wrapper)

    def test_calibration_subtraction_rejected(self) -> None:
        payload, wrapper, _, _ = self.make_simple_pair()
        payload["calibration_records"][0]["subtracted_from_measurements"] = True
        self.expect_failure(payload, wrapper)

    def test_heap_complete_overclaim_rejected(self) -> None:
        payload, wrapper, _, _ = self.make_simple_pair()
        payload["records"][0]["heap_complete"] = True
        self.expect_failure(payload, wrapper)

    def test_operation_aborted_rejected(self) -> None:
        payload, wrapper, _, _ = self.make_simple_pair()
        payload["records"][0]["operation_aborted"] = True
        self.expect_failure(payload, wrapper)

    def test_owning_output_post_destroy_must_return_to_baseline(self) -> None:
        spec = ("benchmark_name=M2/all_levels/8\ndepth_per_side=8\n"
                "generated_workload_sha256=d" + "e" * 63 +
                "\ngenerator_schema=M5_PHASE6_M2_CELLS_V1\noperation=all_levels\n"
                "seed=not_applicable\n")
        workload = _workload_identity("M2/all_levels/8", spec)
        measurement = _eligible_measurement(1000, 1400, 1200, 1, 200, 0, 0)
        measurement["post_destroy_lifecycle_status"] = "destroyed"
        measurement["post_destroy_live_bytes"] = 1200
        record = _record("M2/all_levels/8", workload, measurement)
        _bind_result_sha(record)
        payload = {
            "schema": phase7.PAYLOAD_SCHEMA,
            "measurement_contract_version": phase7.MEASUREMENT_CONTRACT,
            "record_count": 1, "calibration_record_count": 1,
            "calibration_records": [_calibration()], "records": [record],
        }
        wrapper = _wrapper(self.payload_path, "")
        wrapper["workload_identities"] = [workload]
        with self.assertRaises(phase7.ValidationError) as context:
            self.write_pair(payload, wrapper)
            phase7.main_run_for_test(self.payload_path, self.wrapper_path)
        self.assertIn("D must equal A", str(context.exception))


class RationalTests(ValidatorTestBase):
    def _replay_pair(self, numerator: int, derived_denominator: int, aggregate: int,
                     event_count: Optional[int] = None) -> tuple[dict, dict]:
        if event_count is None:
            event_count = derived_denominator
        spec = ("benchmark_name=CoreNormalizedReplay/Spot\n"
                "canonical_log_sha256=f" + "a" * 63 + "\n"
                "generated_workload_sha256=f" + "a" * 63 + "\n"
                "generator_schema=M5_PHASE6_REPLAY_V1\n"
                f"logical_items_per_iteration={event_count}\nseed=548746690337\n"
                f"workload_identity=fixture;event_count={event_count};rest\n")
        workload = _workload_identity("CoreNormalizedReplay/Spot", spec)
        measurement = _eligible_measurement(4000, 4000, 4000, aggregate, 96, aggregate, 96)
        measurement["repetitions"] = 1
        measurement["post_destroy_lifecycle_status"] = "destroyed"
        measurement["post_destroy_live_bytes"] = 3000
        record = _record("CoreNormalizedReplay/Spot", workload, measurement)
        record["fixture_identity"] = {"seed": "548746690337",
                                      "canonical_log_sha256": "f" + "a" * 63,
                                      "event_count": event_count}
        record["replay_aggregate"] = {
            "aggregate_allocation_count": aggregate,
            "aggregate_allocated_bytes": 96,
            "aggregate_deallocation_count": aggregate,
            "aggregate_deallocated_bytes": 96,
            "event_count": event_count,
            "derived_per_event_allocations": {"numerator": numerator,
                                              "denominator": derived_denominator},
            "derived_per_event_bytes": {"numerator": 96,
                                        "denominator": derived_denominator},
        }
        _bind_result_sha(record)
        payload = {
            "schema": phase7.PAYLOAD_SCHEMA,
            "measurement_contract_version": phase7.MEASUREMENT_CONTRACT,
            "record_count": 1, "calibration_record_count": 1,
            "calibration_records": [_calibration()], "records": [record],
        }
        wrapper = _wrapper(self.payload_path, "")
        wrapper["workload_identities"] = [workload]
        self.write_pair(payload, wrapper)
        return payload, wrapper

    def test_case27_three_over_two_events_accepted_exactly(self) -> None:
        payload, wrapper = self._replay_pair(3, 2, 3)
        identities = phase7.validate_wrapper(self.wrapper_path, wrapper,
                                             allow_exploratory=True, binary=None,
                                             payload_path=self.payload_path)
        phase7.validate_payload_document(payload, identities, None)

    def test_zero_denominator_rejected(self) -> None:
        payload, wrapper = self._replay_pair(3, 0, 3, event_count=2)
        with self.assertRaises(phase7.ValidationError) as context:
            phase7.main_run_for_test(self.payload_path, self.wrapper_path)
        self.assertIn("denominator must be > 0", str(context.exception))

    def test_truncated_rational_rejected(self) -> None:
        payload, wrapper = self._replay_pair(1, 2, 3)
        with self.assertRaises(phase7.ValidationError) as context:
            phase7.main_run_for_test(self.payload_path, self.wrapper_path)
        self.assertIn("numerator", str(context.exception))

    def test_rational_denominator_must_equal_event_count(self) -> None:
        payload, wrapper = self._replay_pair(3, 3, 3, event_count=2)
        with self.assertRaises(phase7.ValidationError) as context:
            phase7.main_run_for_test(self.payload_path, self.wrapper_path)
        self.assertIn("must equal event_count", str(context.exception))


class FootprintValidationTests(ValidatorTestBase):
    def _footprint_pair(self, depth: int, empty: int, bids: int, both: int) -> dict[str, Any]:
        record = {
            "schema": phase7.FOOTPRINT_SCHEMA,
            "measurement_contract_version": phase7.MEASUREMENT_CONTRACT,
            "evidence_class": "exploratory",
            "measurement_scope": f"M5_Footprint/Depth/{depth}",
            "allocation_boundary": phase7.ALLOCATION_BOUNDARY,
            "depth_per_side": depth,
            "generator_identity": {"schema": "M5_PHASE6_M2_CELLS_V1",
                                   "seed": "not_applicable"},
            "provenance": _provenance(),
            "snapshots": {
                "pre_experiment_baseline_live_bytes": 100,
                "empty_book_live_bytes": empty,
                "bids_only_live_bytes": bids,
                "both_sides_live_bytes": both,
                "post_destroy_live_bytes": 100,
            },
            "measured_requested_heap_bytes_total": both - empty,
            "measured_requested_heap_bytes_per_side_bids": bids - empty,
            "measured_requested_heap_bytes_per_side_asks": both - bids,
            "measured_bytes_per_level_per_side_bids": {"numerator": bids - empty,
                                                       "denominator": depth},
            "measured_bytes_per_level_per_side_asks": {"numerator": both - bids,
                                                       "denominator": depth},
            "post_destroy_lifecycle_status": "consistent",
            "node_structural_model": {
                "non_additive": True,
                "description": "std::map node allocation request includes node structure",
                "toolchain": "Clang:libc++",
            },
            "allocator_backing_model": {
                "evidence_class": "estimated",
                "description": "environment-specific estimate",
                "scope": "environment/toolchain/allocator/size-class",
            },
            "rss": "not_measured",
            "eligibility": {"status": "eligible"},
            "calibration_record": {"reference": "calibration/empty-bracket-v1",
                                   "subtracted": False},
            "repetitions": 3,
            "determinism_confirmed": True,
            "result_payload_sha256": None,
        }
        canonical = {
            "bids_only_live_bytes": bids,
            "both_sides_live_bytes": both,
            "depth_per_side": depth,
            "determinism_confirmed": True,
            "eligible": True,
            "empty_book_live_bytes": empty,
            "post_destroy_lifecycle_status": "consistent",
            "post_destroy_live_bytes": 100,
            "pre_experiment_baseline_live_bytes": 100,
            "repetitions": 3,
        }
        record["result_payload_sha256"] = hashlib.sha256(
            json.dumps(canonical, sort_keys=True, separators=(",", ":")).encode()
        ).hexdigest()
        payload = {
            "schema": phase7.PAYLOAD_SCHEMA,
            "measurement_contract_version": phase7.MEASUREMENT_CONTRACT,
            "record_count": 1, "calibration_record_count": 1,
            "calibration_records": [_calibration()], "records": [record],
        }
        wrapper = _wrapper(self.payload_path, "")
        self.write_pair(payload, wrapper)
        return payload

    def test_footprint_passes(self) -> None:
        payload = self._footprint_pair(1000, 200, 20200, 40200)
        wrapper = phase7._load_json(self.wrapper_path, "wrapper")
        identities = phase7.validate_wrapper(self.wrapper_path, wrapper,
                                             allow_exploratory=True, binary=None,
                                             payload_path=self.payload_path)
        phase7.validate_payload_document(payload, identities, None)

    def test_node_model_must_be_non_additive(self) -> None:
        payload = self._footprint_pair(1000, 200, 20200, 40200)
        payload["records"][0]["node_structural_model"]["non_additive"] = False
        wrapper = phase7._load_json(self.wrapper_path, "wrapper")
        with self.assertRaises(phase7.ValidationError) as context:
            self.write_pair(payload, wrapper)
            phase7.main_run_for_test(self.payload_path, self.wrapper_path)
        self.assertIn("non-additive", str(context.exception))

    def test_allocator_model_must_be_estimated(self) -> None:
        payload = self._footprint_pair(1000, 200, 20200, 40200)
        payload["records"][0]["allocator_backing_model"]["evidence_class"] = "measured"
        wrapper = phase7._load_json(self.wrapper_path, "wrapper")
        with self.assertRaises(phase7.ValidationError) as context:
            self.write_pair(payload, wrapper)
            phase7.main_run_for_test(self.payload_path, self.wrapper_path)
        self.assertIn("estimated", str(context.exception))

    def test_rss_must_be_not_measured(self) -> None:
        payload = self._footprint_pair(1000, 200, 20200, 40200)
        payload["records"][0]["rss"] = 4096
        wrapper = phase7._load_json(self.wrapper_path, "wrapper")
        with self.assertRaises(phase7.ValidationError) as context:
            self.write_pair(payload, wrapper)
            phase7.main_run_for_test(self.payload_path, self.wrapper_path)
        self.assertIn("not_measured", str(context.exception))

    def test_measured_model_mix_rejected(self) -> None:
        payload = self._footprint_pair(1000, 200, 20200, 40200)
        payload["records"][0]["measured_plus_node_model_total"] = 123
        wrapper = phase7._load_json(self.wrapper_path, "wrapper")
        with self.assertRaises(phase7.ValidationError) as context:
            self.write_pair(payload, wrapper)
            phase7.main_run_for_test(self.payload_path, self.wrapper_path)
        self.assertIn("mixes measured and modeled", str(context.exception))

    def test_total_delta_recomputation_rejected(self) -> None:
        payload = self._footprint_pair(1000, 200, 20200, 40200)
        payload["records"][0]["measured_requested_heap_bytes_total"] += 1
        wrapper = phase7._load_json(self.wrapper_path, "wrapper")
        with self.assertRaises(phase7.ValidationError) as context:
            self.write_pair(payload, wrapper)
            phase7.main_run_for_test(self.payload_path, self.wrapper_path)
        self.assertIn("total delta", str(context.exception))

    def test_post_destroy_mismatch_rejected(self) -> None:
        payload = self._footprint_pair(1000, 200, 20200, 40200)
        payload["records"][0]["snapshots"]["post_destroy_live_bytes"] = 101
        wrapper = phase7._load_json(self.wrapper_path, "wrapper")
        with self.assertRaises(phase7.ValidationError) as context:
            self.write_pair(payload, wrapper)
            phase7.main_run_for_test(self.payload_path, self.wrapper_path)
        self.assertIn("post-destroy", str(context.exception))


class InventoryValidationTests(ValidatorTestBase):
    def _build_inventory_pair(self, scopes: list[str]) -> dict[str, Any]:
        records = []
        for scope in scopes:
            spec = (f"benchmark_name={scope}\ndepth_per_side=8\n"
                    "generated_workload_sha256=d" + "e" * 63 +
                    "\ngenerator_schema=M5_PHASE6_M2_CELLS_V1\noperation=test\n"
                    "seed=not_applicable\n")
            workload = _workload_identity(scope, spec)
            measurement = _eligible_measurement(1000, 1000, 1000, 0, 0, 0, 0)
            if scope.startswith(("M2/all_levels/", "M2/top_levels/")):
                measurement["post_destroy_lifecycle_status"] = "destroyed"
                measurement["post_destroy_live_bytes"] = 1000
            record = _record(scope, workload, measurement)
            _bind_result_sha(record)
            records.append((record, workload))
        payload = {
            "schema": phase7.PAYLOAD_SCHEMA,
            "measurement_contract_version": phase7.MEASUREMENT_CONTRACT,
            "record_count": len(records),
            "calibration_record_count": 1,
            "calibration_records": [_calibration()],
            "records": [record for record, _ in records],
        }
        wrapper = _wrapper(self.payload_path, "")
        wrapper["workload_identities"] = [workload for _, workload in records]
        return payload, wrapper

    def test_m2_m3_inventory_is_exact_and_complete(self) -> None:
        required = phase7.m2_required_inventory() + phase7.m3_required_inventory()
        payload, wrapper = self._build_inventory_pair(required)
        self.write_pair(payload, wrapper)
        identities = phase7.validate_wrapper(self.wrapper_path, wrapper,
                                             allow_exploratory=True, binary=None,
                                             payload_path=self.payload_path)
        scopes = phase7.validate_payload_document(payload, identities, "m2_m3")
        accepted = [name for name in scopes if name.startswith("M3/LiveApply/Accepted/")]
        self.assertEqual(len(accepted), 48)

    def test_extra_phase6_timing_family_rejected(self) -> None:
        # The Phase-6 scaling family update_mix is NOT part of the Phase-7 M2
        # inventory; importing it must fail the exact inventory check.
        required = phase7.m2_required_inventory() + phase7.m3_required_inventory()
        payload, wrapper = self._build_inventory_pair(
            list(required) + ["M2/apply_updates/update_mix/10000"])
        with self.assertRaises(phase7.ValidationError) as context:
            self.write_pair(payload, wrapper)
            phase7.main_run_for_test(self.payload_path, self.wrapper_path,
                                     require_inventory="m2_m3")
        self.assertIn("inventory mismatch", str(context.exception))


class DeterminismTests(ValidatorTestBase):
    def test_identical_payloads_pass(self) -> None:
        payload_a, wrapper_a, _, _ = self.make_simple_pair()
        self.write_pair(payload_a, wrapper_a)
        phase7.check_determinism(payload_a, payload_a)

    def test_normalized_mismatch_fails(self) -> None:
        payload_a, wrapper_a, _, workload = self.make_simple_pair()
        payload_b = json.loads(json.dumps(payload_a))
        payload_b["records"][0]["allocation_count"] = 7
        payload_b["records"][0]["result_payload_sha256"] = "3" * 64
        with self.assertRaises(phase7.ValidationError) as context:
            phase7.check_determinism(payload_a, payload_b)
        self.assertIn("normalized metric mismatch", str(context.exception))

    def test_scope_set_mismatch_fails(self) -> None:
        payload_a, _, spec, workload = self.make_simple_pair()
        record = payload_a["records"][0]
        record2 = dict(record)
        record2["measurement_scope"] = "M2/best_ask/8"
        record2["workload_id"] = "M2/best_ask/8"
        payload_b = {
            "schema": phase7.PAYLOAD_SCHEMA,
            "measurement_contract_version": phase7.MEASUREMENT_CONTRACT,
            "record_count": 1, "calibration_record_count": 1,
            "calibration_records": [_calibration()], "records": [record2],
        }
        with self.assertRaises(phase7.ValidationError) as context:
            phase7.check_determinism(payload_a, payload_b)
        self.assertIn("scope sets differ", str(context.exception))


if __name__ == "__main__":
    unittest.main()
