import hashlib
import importlib.util
import json
import statistics
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
SPEC = importlib.util.spec_from_file_location("phase8_validator", ROOT / "scripts/benchmark_phase8.py")
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def _sha256(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def _stats(values: list[float]) -> dict[str, float]:
    mean = statistics.fmean(values)
    stddev = statistics.pstdev(values)
    return {
        "mean": mean,
        "median": statistics.median(values),
        "minimum": min(values),
        "maximum": max(values),
        "standard_deviation": stddev,
        "coefficient_of_variation": stddev / mean,
    }


class Phase8ValidatorTests(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.directory = Path(self.tmp.name)
        self.payload_path, self.wrapper_path, self.payload, self.wrapper = self._valid_pair()

    def tearDown(self) -> None:
        self.tmp.cleanup()

    def _valid_pair(self, repetitions: int = 6):
        workload_id = "M2/apply_updates/10/8"
        generated_sha = "a" * 64
        fields = {
            "batch": "10",
            "benchmark_name": workload_id,
            "depth_per_side": "8",
            "generated_workload_sha256": generated_sha,
            "generator_schema": "M5_PHASE6_M2_CELLS_V1",
            "operation": "apply_updates",
            "operation_mix": "replace",
        }
        canonical = "".join(f"{key}={fields[key]}\n" for key in sorted(fields))
        workload = {
            "benchmark_name": workload_id,
            "workload_spec_schema": MODULE.WORKLOAD_SCHEMA,
            "workload_spec_sha256": _sha256(canonical),
            "generator_schema": fields["generator_schema"],
            "generator_version": "1",
            "seed": "not_applicable",
            "generated_workload_sha256": generated_sha,
            "canonical_spec_text": canonical,
        }
        values = [float(index) for index in range(1, repetitions + 1)]
        candidates = sorted(MODULE.CANDIDATES)
        records = []
        for candidate in candidates:
            records.append({
                "schema": MODULE.RECORD_SCHEMA,
                "candidate_model_id": candidate,
                "workload_id": workload_id,
                "workload_spec_schema": MODULE.WORKLOAD_SCHEMA,
                "workload_spec_sha256": workload["workload_spec_sha256"],
                "generated_workload_sha256": generated_sha,
                "operation": "apply_updates",
                "depth_per_side": 8,
                "batch": 10,
                "query_limit": 0,
                "metric": "replay_update_throughput",
                "unit": "updates_per_second",
                "repetitions": repetitions,
                "measurement": {"raw": values, "summary": _stats(values)},
                "allocation_supporting_evidence": {
                    "boundary": MODULE.BOUNDARY,
                    "allocation_count": [0] * repetitions,
                    "allocated_bytes": [0] * repetitions,
                },
                "persistent_live_storage": {
                    "measured_requested_bytes": [0] * repetitions,
                    "post_destroy_consistent": True,
                    "rss": "not_measured",
                },
                "final_state_digest": "digest",
            })
        payload = {
            "schema": MODULE.PAYLOAD_SCHEMA,
            "measurement_contract_version": MODULE.CONTRACT,
            "candidate_models": candidates,
            "repetitions": repetitions,
            "timer_overhead_calibration": {
                "timer": "steady_clock",
                "unit": "ns",
                "raw": values,
                "summary": _stats(values),
            },
            "empirical_noise_floor": {
                "method": "unchanged_control_repeated_measurements_v1",
                "baseline_candidate_model_id": "phase8-std-map-control-v1",
                "samples": [{
                    "workload_id": workload_id,
                    "metric": "replay_update_throughput",
                    "unit": "updates_per_second",
                    "raw": values,
                    "summary": _stats(values),
                }],
            },
            "records": records,
        }
        self.payload_path = self.directory / "payload.json"
        self.payload_path.write_text(json.dumps(payload), encoding="utf-8")
        wrapper = {
            "schema": MODULE.WRAPPER_SCHEMA,
            "measurement_contract_version": "M5_PHASE6_MEASUREMENT_CONTRACT_V1",
            "source_provenance": {"status": "known", "git_sha": "b" * 40},
            "binary_provenance": {"path": "/nonexistent/binary", "sha256": "c" * 64},
            "workload_identities": [workload],
            "result_payload": {
                "path": str(self.payload_path),
                "sha256": hashlib.sha256(self.payload_path.read_bytes()).hexdigest(),
                "schema": MODULE.PAYLOAD_SCHEMA,
            },
        }
        self.wrapper_path = self.directory / "wrapper.json"
        self.wrapper_path.write_text(json.dumps(wrapper), encoding="utf-8")
        return self.payload_path, self.wrapper_path, payload, wrapper

    def assert_rejected(self) -> None:
        with self.assertRaises(ValueError):
            MODULE.validate(self.payload_path, self.wrapper_path)

    def rewrite_payload(self) -> None:
        self.payload_path.write_text(json.dumps(self.payload), encoding="utf-8")
        self.wrapper["result_payload"]["sha256"] = hashlib.sha256(
            self.payload_path.read_bytes()).hexdigest()
        self.wrapper_path.write_text(json.dumps(self.wrapper), encoding="utf-8")

    def test_duplicate_json_key_is_rejected(self):
        path = self.directory / "duplicate.json"
        path.write_text('{"schema": 1, "schema": 2}', encoding="utf-8")
        with self.assertRaises(ValueError):
            MODULE.load(path)

    def test_even_repetition_count_round_trips(self):
        MODULE.validate(self.payload_path, self.wrapper_path)
        self.assertEqual(self.payload["repetitions"], 6)
        self.assertEqual(self.payload["records"][0]["measurement"]["summary"]["median"], 3.5)

    def test_mutating_google_benchmark_cells_pause_for_teardown(self):
        source = (ROOT / "benchmarks/phase8/phase8_benchmarks.cpp").read_text(encoding="utf-8")
        for operation in ("apply_level", "apply_updates", "replace_all"):
            start = source.index(f"case Phase8Operation::{operation}")
            end = source.find("case Phase8Operation::", start + 1)
            segment = source[start:] if end == -1 else source[start:end]
            self.assertIn("::benchmark::ClobberMemory();\n                state.PauseTiming();", segment)
            self.assertGreaterEqual(segment.count("state.PauseTiming();"), 2)

    def test_all_records_for_one_workload_removed_is_rejected(self):
        self.payload["records"] = []
        self.rewrite_payload()
        self.assert_rejected()

    def test_record_spec_identity_mismatch_is_rejected(self):
        self.payload["records"][0]["workload_spec_sha256"] = "d" * 64
        self.rewrite_payload()
        self.assert_rejected()

    def test_record_generated_identity_mismatch_is_rejected(self):
        self.payload["records"][0]["generated_workload_sha256"] = "e" * 64
        self.rewrite_payload()
        self.assert_rejected()

    def test_unknown_workload_is_rejected(self):
        self.payload["records"][0]["workload_id"] = "M5_PHASE8/extra"
        self.rewrite_payload()
        self.assert_rejected()

    def test_missing_candidate_cell_is_rejected(self):
        self.payload["records"].pop()
        self.rewrite_payload()
        self.assert_rejected()

    def test_duplicate_candidate_cell_is_rejected(self):
        self.payload["records"].append(self.payload["records"][0].copy())
        self.rewrite_payload()
        self.assert_rejected()

    def test_metric_unit_mismatch_is_rejected(self):
        self.payload["records"][0]["unit"] = "ns"
        self.rewrite_payload()
        self.assert_rejected()

    def test_non_hex_source_sha_is_rejected(self):
        self.wrapper["source_provenance"]["git_sha"] = "not-a-git-sha"
        self.wrapper_path.write_text(json.dumps(self.wrapper), encoding="utf-8")
        self.assert_rejected()

    def test_candidate_set_is_exact(self):
        self.assertEqual(len(MODULE.CANDIDATES), 4)
        self.assertIn("phase8-std-map-control-v1", MODULE.CANDIDATES)
        self.assertIn("phase8-sorted-vector-batch-lww-v1", MODULE.CANDIDATES)


if __name__ == "__main__":
    unittest.main()
