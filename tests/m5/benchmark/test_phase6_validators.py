"""Deterministic tests for the Phase-6 benchmark validators (OD-M5-P6-024)."""

import hashlib
import json
import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[3]))

import scripts.benchmark_phase6 as validator  # type: ignore


def _sha256(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def _write_json(directory: Path, name: str, document: object) -> Path:
    path = directory / name
    path.write_text(json.dumps(document), encoding="utf-8")
    return path


class BaseTestCase(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory()
        self.tmp = Path(self._tmp.name)
        self.payload_bytes = b'{"context":{},"benchmarks":[]}'
        self.payload_path = self.tmp / "payload.json"
        self.payload_path.write_bytes(self.payload_bytes)

    def tearDown(self) -> None:
        self._tmp.cleanup()

    def make_wrapper(self, **overrides) -> dict:
        document = {
            "schema": validator.WRAPPER_SCHEMA,
            "measurement_contract_version": validator.MEASUREMENT_CONTRACT,
            "evidence_class": "formal",
            "requested_evidence_class": "formal",
            "source_provenance": {
                "git_sha": "deadbeef" * 8,
                "status": "known",
                "dirty_at_configure": False,
            },
            "binary_provenance": {"path": "/nonexistent/binary", "sha256": "ab" * 32},
            "build_identity": {
                "compiler": {"id": "Clang", "version": "19"},
                "cxx_standard": "20",
                "build_type": "Release",
                "sanitizer_state": "off",
                "lto_state": "off",
                "standard_library": {
                    "name": "libc++",
                    "version": "190000",
                    "detection_status": "detected_via__LIBCPP_VERSION",
                },
                "conan_lock_sha256": "cd" * 32,
                "conan_references": ["benchmark/1.9.5#rev"],
                "google_benchmark_version": "1.9.5",
            },
            "environment_identity": {
                "os_name": "Linux",
                "os_version": "6.8.0",
                "architecture": "x86_64",
                "cpu_model": "test",
                "logical_core_count": "8",
            },
            "m4_dependency_identity": {"status": "ON",
                                       "contracts_source_revision": "rev",
                                       "contracts_conan_reference": "ref",
                                       "contracts_recipe_revision": "rrev",
                                       "contracts_package_id": "ab" * 20,
                                       "protobuf_runtime_version": "6.33.5",
                                       "protobuf_runtime_rrev": "prrev"},
            "workload_identities": [],
            "measurement_identity": {
                "timer": "cpu",
                "primary_denominator": "cpu_time",
                "warmup": {"kind": "explicit_workload_pass_v1", "count": 1},
                "repetitions": 5,
            },
            "measurements": [],
            "result_payload": {
                "path": str(self.payload_path),
                "sha256": hashlib.sha256(self.payload_bytes).hexdigest(),
                "schema": "google_benchmark_json",
            },
        }
        document.update(overrides)
        return document

    def make_workload(self, name: str, **fields) -> dict:
        logical_items = 16 if name.startswith("M1/") else (
            2048 if name.startswith(("CoreNormalizedReplay/", "AdapterWireReplay/")) else 1)
        generated_fields = {
            "benchmark_name": name,
            "generator_schema": "TEST_GENERATOR_V1",
            "generator_version": "1",
            "seed": "not_applicable",
            "logical_items_per_iteration": str(logical_items),
            **fields,
        }
        if name.startswith("M4/CheckedApply/"):
            # Locked defaults first; explicit call-site fields override them.
            generated_fields = {**validator.CHECKED_APPLY_SEQUENCE_FIELDS, **generated_fields}
        generated_text = "".join(
            f"{key}={value}\n" for key, value in sorted(generated_fields.items())
            if key not in {"primary_timer", "primary_denominator", "throughput_denominator"}
        )
        generated_sha = _sha256(generated_text)
        canonical_fields = {**generated_fields,
                            "generated_workload_sha256": generated_sha}
        canonical = "".join(f"{key}={value}\n" for key, value in
                            sorted(canonical_fields.items()))
        return {
            "benchmark_name": name,
            "workload_spec_schema": validator.WORKLOAD_SPEC_SCHEMA,
            "workload_spec_sha256": _sha256(canonical),
            "generator_schema": "TEST_GENERATOR_V1",
            "generator_version": "1",
            "seed": "not_applicable",
            "generated_workload_sha256": generated_sha,
            "canonical_spec_text": canonical,
        }

    def checked_apply_workload(self, name: str = "M4/CheckedApply/8",
                               drop_fields=(), **overrides) -> dict:
        workload = self.make_workload(name, **overrides)
        fields = dict(
            line.split("=", 1) for line in workload["canonical_spec_text"].splitlines()
        )
        for key in drop_fields:
            fields.pop(key, None)
        fields.update(overrides)
        canonical = "".join(f"{key}={value}\n" for key, value in sorted(fields.items()))
        workload["canonical_spec_text"] = canonical
        workload["workload_spec_sha256"] = _sha256(canonical)
        return workload

    def wrapper_with_checked_apply(self, **overrides) -> dict:
        wrapper = self.wrapper_with_inventory()
        replacements = [self.checked_apply_workload(
            "M4/CheckedApply/" + str(depth), **overrides)
            for depth in (8, 100, 1000)]
        names = {entry["benchmark_name"]: entry for entry in replacements}
        wrapper["workload_identities"] = [
            names.get(entry["benchmark_name"], entry)
            for entry in wrapper["workload_identities"]
        ]
        return wrapper

    def wrapper_with_inventory(self) -> dict:
        workloads = [self.make_workload(name) for name in validator._required_inventory()]
        return self.make_wrapper(workload_identities=workloads)


class InventoryTest(BaseTestCase):
    def test_required_inventory_complete_passes(self) -> None:
        wrapper = self.wrapper_with_inventory()
        names = validator.validate_inventory(wrapper)
        self.assertEqual(len(names), len(validator._required_inventory()))
        self.assertEqual(len([n for n in names if n.startswith("M3/LiveApply/Accepted/")]), 48)

    def test_missing_required_benchmark_fails(self) -> None:
        wrapper = self.wrapper_with_inventory()
        wrapper["workload_identities"] = [
            entry for entry in wrapper["workload_identities"]
            if entry["benchmark_name"] != "M1/ParsePrice/MatchedScale"
        ]
        with self.assertRaises(validator.ValidationError):
            validator.validate_inventory(wrapper)

    def test_missing_required_m4_fails_closed(self) -> None:
        wrapper = self.wrapper_with_inventory()
        wrapper["workload_identities"] = [
            entry for entry in wrapper["workload_identities"]
            if not entry["benchmark_name"].startswith("M4/")
        ]
        with self.assertRaises(validator.ValidationError):
            validator.validate_inventory(wrapper)

    def test_wrong_spec_sha_fails(self) -> None:
        wrapper = self.wrapper_with_inventory()
        wrapper["workload_identities"][0]["workload_spec_sha256"] = "00" * 32
        with self.assertRaises(validator.ValidationError):
            validator.validate_inventory(wrapper)

    def test_missing_generated_identity_fails(self) -> None:
        wrapper = self.wrapper_with_inventory()
        workload = wrapper["workload_identities"][0]
        workload["generator_version"] = ""
        with self.assertRaises(validator.ValidationError):
            validator.validate_inventory(wrapper)

    def test_placeholder_generated_hash_fails(self) -> None:
        wrapper = self.wrapper_with_inventory()
        workload = wrapper["workload_identities"][0]
        workload["generated_workload_sha256"] = "0" * 64
        with self.assertRaises(validator.ValidationError):
            validator.validate_inventory(wrapper)

    def test_inconsistent_generated_hash_fails(self) -> None:
        wrapper = self.wrapper_with_inventory()
        workload = wrapper["workload_identities"][0]
        workload["generated_workload_sha256"] = "ab" * 32
        with self.assertRaises(validator.ValidationError):
            validator.validate_inventory(wrapper)


class PayloadTest(BaseTestCase):
    def make_payload(self, entries) -> dict:
        return {"context": {}, "benchmarks": entries}

    def test_zero_match_fails(self) -> None:
        with self.assertRaises(validator.ValidationError):
            validator.validate_payload_structure(self.make_payload([]))

    def test_error_occurred_fails(self) -> None:
        payload = self.make_payload(
            [{"name": "X/1", "iterations": 10, "real_time": 5.0, "cpu_time": 5.0,
              "time_unit": "ns", "error_occurred": True}]
        )
        with self.assertRaises(validator.ValidationError):
            validator.validate_payload_structure(payload)

    def test_skip_record_fails(self) -> None:
        payload = self.make_payload(
            [{"name": "X/1", "iterations": 10, "real_time": 5.0, "cpu_time": 5.0,
              "time_unit": "ns", "skipped": True, "skip_message": "SkipWithError"}]
        )
        with self.assertRaises(validator.ValidationError):
            validator.validate_payload_structure(payload)

    def test_non_finite_timing_fails(self) -> None:
        payload = self.make_payload(
            [{"name": "X/1", "iterations": 10, "real_time": float("inf"),
              "cpu_time": 5.0, "time_unit": "ns"}]
        )
        with self.assertRaises(validator.ValidationError):
            validator.validate_payload_structure(payload)

    def test_non_positive_timing_fails(self) -> None:
        payload = self.make_payload(
            [{"name": "X/1", "iterations": 10, "real_time": 0.0, "cpu_time": 5.0,
              "time_unit": "ns"}]
        )
        with self.assertRaises(validator.ValidationError):
            validator.validate_payload_structure(payload)


class CheckedApplySpecTest(BaseTestCase):
    def test_correct_checked_apply_spec_passes(self) -> None:
        wrapper = self.wrapper_with_inventory()
        names = validator.validate_inventory(wrapper)
        checked = [name for name in names if name.startswith("M4/CheckedApply/")]
        self.assertEqual(len(checked), 3)

    def test_checked_apply_positive_fixture_fields(self) -> None:
        workload = self.checked_apply_workload("M4/CheckedApply/8")
        fields = dict(line.split("=", 1)
                      for line in workload["canonical_spec_text"].splitlines())
        for key, value in validator.CHECKED_APPLY_SEQUENCE_FIELDS.items():
            self.assertEqual(fields[key], value)

    def test_missing_policy_fails(self) -> None:
        wrapper = self.wrapper_with_checked_apply(drop_fields=("policy",))
        with self.assertRaises(validator.ValidationError):
            validator.validate_inventory(wrapper)

    def test_wrong_policy_fails(self) -> None:
        wrapper = self.wrapper_with_checked_apply(policy="UsdMPerpetual")
        with self.assertRaises(validator.ValidationError):
            validator.validate_inventory(wrapper)

    def test_missing_initial_update_id_fails(self) -> None:
        wrapper = self.wrapper_with_checked_apply(drop_fields=("initial_update_id",))
        with self.assertRaises(validator.ValidationError):
            validator.validate_inventory(wrapper)

    def test_wrong_initial_update_id_fails(self) -> None:
        wrapper = self.wrapper_with_checked_apply(initial_update_id="1000000")
        with self.assertRaises(validator.ValidationError):
            validator.validate_inventory(wrapper)

    def test_missing_first_update_id_fails(self) -> None:
        wrapper = self.wrapper_with_checked_apply(drop_fields=("first_update_id",))
        with self.assertRaises(validator.ValidationError):
            validator.validate_inventory(wrapper)

    def test_historical_successor_metadata_fails(self) -> None:
        # The historical defect encoded the prepared id as the successor.
        wrapper = self.wrapper_with_checked_apply(first_update_id="1000001",
                                                  final_update_id="1000001")
        with self.assertRaises(validator.ValidationError):
            validator.validate_inventory(wrapper)

    def test_missing_final_update_id_fails(self) -> None:
        wrapper = self.wrapper_with_checked_apply(drop_fields=("final_update_id",))
        with self.assertRaises(validator.ValidationError):
            validator.validate_inventory(wrapper)

    def test_wrong_final_update_id_fails(self) -> None:
        wrapper = self.wrapper_with_checked_apply(final_update_id="1000001")
        with self.assertRaises(validator.ValidationError):
            validator.validate_inventory(wrapper)

    def test_invalid_successor_relationship_fails(self) -> None:
        wrapper = self.wrapper_with_checked_apply(first_update_id="1000003",
                                                  final_update_id="1000003")
        with self.assertRaises(validator.ValidationError):
            validator.validate_inventory(wrapper)

    def test_wrong_previous_final_update_id_fails(self) -> None:
        wrapper = self.wrapper_with_checked_apply(previous_final_update_id="1000002")
        with self.assertRaises(validator.ValidationError):
            validator.validate_inventory(wrapper)


class ContractsPackageIdTest(BaseTestCase):
    def test_valid_40_hex_package_id_passes(self) -> None:
        wrapper = self.make_wrapper(
            m4_dependency_identity={
                "status": "ON",
                "contracts_source_revision": "rev",
                "contracts_conan_reference": "ref",
                "contracts_recipe_revision": "rrev",
                "contracts_package_id": "a1a286da6ca09b590d78bcb14d8250c025131c29",
                "protobuf_runtime_version": "6.33.5",
                "protobuf_runtime_rrev": "prrev",
            })
        validator.validate_wrapper("x", wrapper, True, None)

    def test_cache_locator_package_id_fails(self) -> None:
        # The historical cache-directory locator must be rejected.
        wrapper = self.wrapper_with_inventory()
        wrapper["m4_dependency_identity"]["contracts_package_id"] = "binan45ca4a301956d"
        with self.assertRaises(validator.ValidationError):
            validator.validate_wrapper("x", wrapper, True, None)

    def test_empty_package_id_fails(self) -> None:
        wrapper = self.wrapper_with_inventory()
        wrapper["m4_dependency_identity"]["contracts_package_id"] = ""
        with self.assertRaises(validator.ValidationError):
            validator.validate_wrapper("x", wrapper, True, None)

    def test_unavailable_package_id_fails(self) -> None:
        wrapper = self.wrapper_with_inventory()
        wrapper["m4_dependency_identity"]["contracts_package_id"] = "unavailable"
        with self.assertRaises(validator.ValidationError):
            validator.validate_wrapper("x", wrapper, True, None)

    def test_short_package_id_fails(self) -> None:
        wrapper = self.wrapper_with_inventory()
        wrapper["m4_dependency_identity"]["contracts_package_id"] = "ab" * 19 + "a"
        with self.assertRaises(validator.ValidationError):
            validator.validate_wrapper("x", wrapper, True, None)

    def test_long_package_id_fails(self) -> None:
        wrapper = self.wrapper_with_inventory()
        wrapper["m4_dependency_identity"]["contracts_package_id"] = "ab" * 20 + "a"
        with self.assertRaises(validator.ValidationError):
            validator.validate_wrapper("x", wrapper, True, None)

    def test_non_hex_package_id_fails(self) -> None:
        wrapper = self.wrapper_with_inventory()
        wrapper["m4_dependency_identity"]["contracts_package_id"] = "g" * 40
        with self.assertRaises(validator.ValidationError):
            validator.validate_wrapper("x", wrapper, True, None)

    def test_uppercase_package_id_fails(self) -> None:
        wrapper = self.wrapper_with_inventory()
        wrapper["m4_dependency_identity"]["contracts_package_id"] = "AB" * 20
        with self.assertRaises(validator.ValidationError):
            validator.validate_wrapper("x", wrapper, True, None)


class SmokeTest(BaseTestCase):
    def make_smoke_payload(self) -> dict:
        entries = []
        for name in validator._smoke_expected_set():
            logical_items = 16 if name.startswith("M1/") else (
                2048 if name.startswith(("CoreNormalizedReplay/", "AdapterWireReplay/")) else 1)
            entries.append({
                "name": name + "/min_time:0.050",
                "iterations": 100,
                "real_time": 1.0,
                "cpu_time": 1.0,
                "time_unit": "ns",
                "items_per_second": logical_items * 1_000_000_000.0,
                "repetitions": 1,
            })
        return {"context": {}, "benchmarks": entries}

    def test_exact_smoke_set_passes(self) -> None:
        wrapper = self.wrapper_with_inventory()
        payload = self.make_smoke_payload()
        validator.validate_smoke(payload, wrapper, 1)

    def test_smoke_missing_executed_benchmark_fails(self) -> None:
        wrapper = self.wrapper_with_inventory()
        payload = self.make_smoke_payload()
        payload["benchmarks"] = payload["benchmarks"][:-1]
        with self.assertRaises(validator.ValidationError):
            validator.validate_smoke(payload, wrapper, 1)

    def test_smoke_unexpected_benchmark_fails(self) -> None:
        wrapper = self.wrapper_with_inventory()
        payload = self.make_smoke_payload()
        payload["benchmarks"].append(
            {"name": "Unexpected/Benchmark", "iterations": 1, "real_time": 1.0,
             "cpu_time": 1.0, "time_unit": "ns", "items_per_second": 1.0e9,
             "repetitions": 1}
        )
        with self.assertRaises(validator.ValidationError):
            validator.validate_smoke(payload, wrapper, 1)

    def test_name_normalization_strips_decorations(self) -> None:
        self.assertEqual(
            validator.normalize_benchmark_name("M3/LiveApply/Accepted/Spot/D8/B0/policy:0/depth:8/batch:0/min_time:0.050"),
            "M3/LiveApply/Accepted/Spot/D8/B0",
        )
        self.assertEqual(
            validator.normalize_benchmark_name("M2/apply_level/insert/8/depth:8/iterations:4096"),
            "M2/apply_level/insert/8",
        )
        self.assertEqual(
            validator.normalize_benchmark_name("CoreNormalizedReplay/Spot/min_time:0.050/real_time"),
            "CoreNormalizedReplay/Spot",
        )

    def test_overwritten_item_count_rate_fails(self) -> None:
        wrapper = self.wrapper_with_inventory()
        payload = self.make_smoke_payload()
        target = next(entry for entry in payload["benchmarks"]
                      if entry["name"].startswith("M1/ParsePrice/MatchedScale/"))
        target["items_per_second"] = 1_000_000_000.0
        with self.assertRaises(validator.ValidationError):
            validator.validate_smoke(payload, wrapper, 1)

    def test_replay_events_rate_and_ns_per_event_are_reciprocal(self) -> None:
        wrapper = self.wrapper_with_inventory()
        payload = self.make_smoke_payload()
        validator.validate_item_rates(payload, wrapper)


class WrapperTest(BaseTestCase):
    def test_valid_wrapper_passes(self) -> None:
        wrapper = self.wrapper_with_inventory()
        path = _write_json(self.tmp, "wrapper.json", wrapper)
        validator.validate_wrapper(str(path), wrapper, False, None, require_inventory=True)

    def test_unknown_schema_fails(self) -> None:
        wrapper = self.wrapper_with_inventory()
        wrapper["schema"] = "UNKNOWN_SCHEMA"
        with self.assertRaises(validator.ValidationError):
            validator.validate_wrapper("x", wrapper, True, None)

    def test_missing_required_metadata_fails(self) -> None:
        for key in ("source_provenance", "binary_provenance", "build_identity",
                    "environment_identity", "m4_dependency_identity",
                    "measurement_identity", "result_payload"):
            wrapper = self.wrapper_with_inventory()
            del wrapper[key]
            with self.assertRaises(validator.ValidationError):
                validator.validate_wrapper("x", wrapper, True, None)

    def test_payload_sha_mismatch_fails(self) -> None:
        wrapper = self.wrapper_with_inventory()
        wrapper["result_payload"]["sha256"] = "00" * 32
        with self.assertRaises(validator.ValidationError):
            validator.validate_wrapper("x", wrapper, True, None)

    def test_missing_payload_file_fails(self) -> None:
        wrapper = self.wrapper_with_inventory()
        wrapper["result_payload"]["path"] = str(self.tmp / "missing.json")
        with self.assertRaises(validator.ValidationError):
            validator.validate_wrapper("x", wrapper, True, None)

    def test_dirty_formal_rejected(self) -> None:
        wrapper = self.wrapper_with_inventory()
        wrapper["source_provenance"]["dirty_at_configure"] = True
        with self.assertRaises(validator.ValidationError):
            validator.validate_wrapper("x", wrapper, True, None)

    def test_dirty_exploratory_allowed(self) -> None:
        wrapper = self.wrapper_with_inventory()
        wrapper["source_provenance"]["dirty_at_configure"] = True
        wrapper["evidence_class"] = "exploratory"
        path = _write_json(self.tmp, "wrapper.json", wrapper)
        validator.validate_wrapper(str(path), wrapper, True, None, require_inventory=True)

    def test_core_only_payload_records_not_applicable(self) -> None:
        wrapper = self.wrapper_with_inventory()
        wrapper["m4_dependency_identity"] = {"status": "OFF",
                                             "reason": "not_applicable_core_only_payload"}
        path = _write_json(self.tmp, "wrapper.json", wrapper)
        validator.validate_wrapper(str(path), wrapper, True, None, require_inventory=True)

    def test_binary_sha_mismatch_fails(self) -> None:
        wrapper = self.wrapper_with_inventory()
        binary = self.tmp / "binary"
        binary.write_bytes(b"binary-bytes")
        with self.assertRaises(validator.ValidationError):
            validator.validate_wrapper("x", wrapper, True, str(binary))

    def test_binary_sha_match_passes(self) -> None:
        wrapper = self.wrapper_with_inventory()
        binary = self.tmp / "binary"
        binary.write_bytes(b"binary-bytes")
        wrapper["binary_provenance"]["sha256"] = hashlib.sha256(b"binary-bytes").hexdigest()
        path = _write_json(self.tmp, "wrapper.json", wrapper)
        validator.validate_wrapper(str(path), wrapper, True, str(binary), require_inventory=True)

    def test_missing_stdlib_identity_fails(self) -> None:
        wrapper = self.wrapper_with_inventory()
        del wrapper["build_identity"]["standard_library"]
        with self.assertRaises(validator.ValidationError):
            validator.validate_wrapper("x", wrapper, True, None)

    def test_unknown_source_cannot_be_handcrafted_as_clean_formal(self) -> None:
        wrapper = self.wrapper_with_inventory()
        wrapper["source_provenance"] = {
            "git_sha": "unavailable", "status": "unavailable",
            "dirty_at_configure": False,
        }
        with self.assertRaises(validator.ValidationError):
            validator.validate_wrapper("x", wrapper, True, None)

    def test_formal_google_repetition_minimum(self) -> None:
        for repetitions in (0, 1, 4):
            wrapper = self.wrapper_with_inventory()
            wrapper["measurement_identity"]["repetitions"] = repetitions
            with self.subTest(repetitions=repetitions), self.assertRaises(
                    validator.ValidationError):
                validator.validate_wrapper("x", wrapper, True, None)
        for repetitions in (5, 6):
            wrapper = self.wrapper_with_inventory()
            wrapper["measurement_identity"]["repetitions"] = repetitions
            validator.validate_wrapper("x", wrapper, True, None)

    def test_wrong_warmup_contract_fails(self) -> None:
        wrapper = self.wrapper_with_inventory()
        wrapper["measurement_identity"]["warmup"]["kind"] = "internal"
        with self.assertRaises(validator.ValidationError):
            validator.validate_wrapper("x", wrapper, True, None)


class LatencyTest(BaseTestCase):
    def make_latency(self, samples, passes, event_count, expected_checksum,
                     validated=True) -> dict:
        return {
            "schema": validator.LATENCY_SCHEMA,
            "measurement_contract_version": validator.MEASUREMENT_CONTRACT,
            "workload": {
                "workload_id": "wl",
                "market": "Spot",
                "symbol": "BTCUSDT",
                "price_scale": 2,
                "quantity_scale": 3,
                "sequence_policy": "Spot",
                "event_count": event_count,
                "canonical_log_sha256": "ab" * 32,
                "workload_spec_schema": validator.WORKLOAD_SPEC_SCHEMA,
                "workload_spec_text": "workload_id=wl\n",
                "workload_spec_sha256": _sha256("workload_id=wl\n"),
            },
            "timer": {"type": "steady_clock", "primary_denominator": "wall_time"},
            "warmup": {"kind": "full_workload_pass", "count": 1,
                       "state_isolation": "fresh"},
            "passes": passes,
            "sample_count": len(samples),
            "unique_event_count": event_count,
            "quantile_estimator": validator.QUANTILE_ESTIMATOR,
            "checksum": {
                "methodology_version": "M5_PHASE6_REPLAY_CHECKSUM_V1",
                "expected": expected_checksum,
                "per_pass": [expected_checksum] * passes,
                "validated": validated,
            },
            "eligibility": {
                "p50": len(samples) >= 1000,
                "p90": len(samples) >= 1000,
                "p99": len(samples) >= 10000,
                "p99_9": False,
                "p99_9_reason": "small workload",
            },
            "quantiles_ns": {
                "p50": validator._nearest_rank(sorted(samples), 0.5)
                if len(samples) >= 1000 else None,
                "p90": validator._nearest_rank(sorted(samples), 0.9)
                if len(samples) >= 1000 else None,
                "p99": validator._nearest_rank(sorted(samples), 0.99)
                if len(samples) >= 10000 else None,
                "p99_9": None,
            },
            "calibration": {
                "sample_count": 10,
                "quantiles_ns": {"p50": 100, "p90": 200, "p99": 300, "p99_9": None},
                "subtracted_from_event_samples": False,
                "calibration_samples_ns": [1, 2, 3],
            },
            "raw_samples_ns": sorted(samples),
            "calibration_samples_ns": [1, 2, 3],
        }

    def test_recomputed_quantiles_pass(self) -> None:
        samples = list(range(1000, 0, -1))
        latency = self.make_latency(samples, 1, 1000, "aabbccdd")
        latency_path = _write_json(self.tmp, "latency.json", latency)
        payload_sha = hashlib.sha256(json.dumps(latency).encode("utf-8")).hexdigest()
        # The wrapper must bind the exact serialized file bytes; recompute from
        # the written file.
        wrapper = self.make_wrapper()
        wrapper["result_payload"] = {
            "path": str(latency_path),
            "sha256": hashlib.sha256(latency_path.read_bytes()).hexdigest(),
            "schema": validator.LATENCY_SCHEMA,
        }
        wrapper_path = _write_json(self.tmp, "latency-wrapper.json", wrapper)
        validator.validate_latency(str(latency_path), str(wrapper_path), latency, wrapper)

    def test_quantile_mismatch_fails(self) -> None:
        samples = list(range(1000, 0, -1))
        latency = self.make_latency(samples, 1, 1000, "aabbccdd")
        latency["quantiles_ns"]["p50"] = 1
        with self.assertRaises(validator.ValidationError):
            validator.validate_latency("x", "y", latency, self.make_wrapper())

    def test_sample_pass_consistency_fails(self) -> None:
        samples = [5] * 2048
        latency = self.make_latency(samples, 5, 2048, "aabbccdd")
        with self.assertRaises(validator.ValidationError):
            validator.validate_latency("x", "y", latency, self.make_wrapper())

    def test_p999_unique_event_rule(self) -> None:
        # sample_count >= 100000 but unique_event_count < 100000: p99.9 must
        # never become eligible from repeated small fixtures.
        samples = [5] * 100_000
        latency = self.make_latency(samples, 1, 100_000, "aabbccdd")
        latency["eligibility"]["p99_9"] = True
        with self.assertRaises(validator.ValidationError):
            validator.validate_latency("x", "y", latency, self.make_wrapper())

    def test_p999_omission_needs_reason(self) -> None:
        samples = [5] * 2048
        latency = self.make_latency(samples, 1, 2048, "aabbccdd")
        latency["eligibility"].pop("p99_9_reason")
        with self.assertRaises(validator.ValidationError):
            validator.validate_latency("x", "y", latency, self.make_wrapper())

    def test_unvalidated_checksum_fails(self) -> None:
        samples = list(range(1000, 0, -1))
        latency = self.make_latency(samples, 1, 1000, "aabbccdd", validated=False)
        with self.assertRaises(validator.ValidationError):
            validator.validate_latency("x", "y", latency, self.make_wrapper())

    def test_calibration_subtraction_fails(self) -> None:
        samples = list(range(1000, 0, -1))
        latency = self.make_latency(samples, 1, 1000, "aabbccdd")
        latency["calibration"]["subtracted_from_event_samples"] = True
        with self.assertRaises(validator.ValidationError):
            validator.validate_latency("x", "y", latency, self.make_wrapper())


if __name__ == "__main__":
    unittest.main()
