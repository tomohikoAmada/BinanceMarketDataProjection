from __future__ import annotations

import io
import json
import tarfile
import tempfile
import unittest
from dataclasses import replace
from pathlib import Path
from unittest.mock import patch

from scripts import m5_phase10


class Phase10VerifierTests(unittest.TestCase):
    _REPOSITORY_ROOT = Path(__file__).resolve().parents[3]

    def _recorded_benchmark_source(self) -> str:
        return (
            self._REPOSITORY_ROOT / "benchmarks" / "recorded_replay_benchmark_main.cpp"
        ).read_text(encoding="utf-8")

    def _package(
        self,
        *,
        manifest_mutator=None,
        payload_mutator=None,
        extra_member=None,
        missing_member=None,
        duplicate_member=None,
        rename_member=None,
        special_member=None,
        special_type=tarfile.SYMTYPE,
    ) -> tuple[bytes, m5_phase10.DistributionContract]:
        payloads: dict[str, bytes] = {}
        fixtures = []
        for fixture in m5_phase10.PRODUCTION_FIXTURES:
            for path in fixture.payload_paths:
                payloads[path] = f"{path}\n".encode("utf-8")
            if payload_mutator is not None:
                payload_mutator(payloads, fixture)
            fixtures.append(
                replace(
                    fixture,
                    payload_sha256={
                        path: m5_phase10._sha256_bytes(payloads[path])
                        for path in fixture.payload_paths
                    },
                )
            )

        contract = replace(m5_phase10.PRODUCTION_CONTRACT, fixtures=tuple(fixtures))
        manifest = {
            "asset_name": contract.asset_name,
            "distribution_schema": contract.distribution_schema,
            "fixtures": [
                {
                    "authoritative_replay_log_sha256": fixture.replay_sha256,
                    "event_count": fixture.event_count,
                    "fixture_id": fixture.fixture_id,
                    "included_relative_payload_paths": list(fixture.payload_paths),
                    "market": fixture.market,
                    "numeric_spec": {
                        "price_scale": fixture.price_scale,
                        "quantity_scale": fixture.quantity_scale,
                    },
                    "payload_sha256": dict(fixture.payload_sha256),
                    "symbol": fixture.symbol,
                }
                for fixture in contract.fixtures
            ],
            "materializer_version": contract.materializer_version,
            "owner_distribution_authority": "AUTHORIZED_BY_PROJECT_OWNER",
            "package_id": contract.package_id,
            "raw_source_included": False,
            "recorder_commit": contract.recorder_commit,
            "recorder_config_sha256": contract.recorder_config_sha256,
            "recorder_wheel_sha256": contract.recorder_wheel_sha256,
            "release_tag": contract.release_tag,
            "repository": contract.repository,
            "source_inventory_catalog_sha256": contract.source_inventory_catalog_sha256,
            "source_run_identity": contract.source_run_identity,
        }
        if manifest_mutator is not None:
            manifest_mutator(manifest)
        manifest_bytes = json.dumps(manifest, sort_keys=True, indent=2).encode("utf-8")
        files = {"distribution-manifest.json": manifest_bytes, **payloads}
        if extra_member is not None:
            files[extra_member] = b"unexpected\n"
        if missing_member is not None:
            files.pop(missing_member, None)

        member_names = list(files)
        if duplicate_member is not None:
            member_names.append(duplicate_member)

        archive_bytes = io.BytesIO()
        with tarfile.open(fileobj=archive_bytes, mode="w:gz") as archive:
            for original_name in member_names:
                name = rename_member.get(original_name, original_name) if rename_member else original_name
                info = tarfile.TarInfo(name)
                if special_member == original_name:
                    info.type = special_type
                    if special_type == tarfile.SYMTYPE:
                        info.linkname = "target"
                    elif special_type == tarfile.CHRTYPE:
                        info.devmajor = 1
                        info.devminor = 3
                elif original_name == duplicate_member:
                    info.size = len(files[original_name])
                else:
                    info.size = len(files[original_name])
                data = files.get(original_name, b"")
                if info.isreg():
                    archive.addfile(info, io.BytesIO(data))
                else:
                    archive.addfile(info)
        result = archive_bytes.getvalue()
        return result, replace(
            contract,
            manifest_sha256=m5_phase10._sha256_bytes(manifest_bytes),
            archive_sha256=m5_phase10._sha256_bytes(result),
        )

    def _verify_invalid(self, package: bytes, contract: m5_phase10.DistributionContract) -> None:
        with tempfile.TemporaryDirectory() as directory:
            archive = Path(directory) / "package.tar.gz"
            archive.write_bytes(package)
            with self.assertRaises(m5_phase10.VerificationError):
                m5_phase10.verify_distribution(
                    archive,
                    Path(directory) / "extracted",
                    contract=contract,
                )

    def _benchmark_payload(
        self, aggregates: dict[str, float], repetition_count: int = 3
    ) -> dict[str, object]:
        expected_name = "M5RecordedReplay/Spot"
        entries: list[dict[str, object]] = [
            {
                "name": f"{expected_name}/real_time",
                "run_type": "iteration",
                "repetitions": 3,
                "repetition_index": index,
                "iterations": 1,
                "real_time": 10.0 + index,
                "cpu_time": 9.0 + index,
                "items_per_second": 1000.0,
            }
            for index in range(repetition_count)
        ]
        entries.extend(
            {
                "name": f"{expected_name}/real_time_{statistic}",
                "run_type": "aggregate",
                "aggregate_name": statistic,
                "real_time": value,
            }
            for statistic, value in aggregates.items()
        )
        return {"context": {}, "benchmarks": entries}

    def _validate_benchmark_payload(
        self, aggregates: dict[str, float]
    ) -> tuple[list[dict[str, object]], list[dict[str, object]]]:
        payload = self._benchmark_payload(aggregates)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "benchmark.json"
            path.write_text(json.dumps(payload), encoding="utf-8")
            return m5_phase10._validate_benchmark_payload(
                path, "M5RecordedReplay/Spot"
            )

    def test_three_repetition_default_aggregates_are_accepted(self) -> None:
        iterations, aggregates = self._validate_benchmark_payload(
            {"mean": 11.0, "median": 11.0, "stddev": 0.5, "cv": 0.04}
        )
        self.assertEqual(len(iterations), 3)
        self.assertEqual(len(aggregates), 4)

    def test_zero_dispersion_aggregates_are_accepted(self) -> None:
        iterations, aggregates = self._validate_benchmark_payload(
            {"mean": 11.0, "median": 11.0, "stddev": 0.0, "cv": 0.0}
        )
        self.assertEqual(len(iterations), 3)
        self.assertEqual(len(aggregates), 4)

    def test_unexpected_aggregate_is_rejected(self) -> None:
        with self.assertRaises(m5_phase10.VerificationError):
            self._validate_benchmark_payload(
                {"mean": 11.0, "median": 11.0, "stddev": 0.5, "cv": 0.04, "p99": 12.0}
            )

    def test_wrong_iteration_count_is_rejected(self) -> None:
        payload = self._benchmark_payload(
            {"mean": 11.0, "median": 11.0, "stddev": 0.5, "cv": 0.04},
            repetition_count=2,
        )
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "benchmark.json"
            path.write_text(json.dumps(payload), encoding="utf-8")
            with self.assertRaises(m5_phase10.VerificationError):
                m5_phase10._validate_benchmark_payload(path, "M5RecordedReplay/Spot")

    def _wrapper_for_measurement_validation(
        self, directory: str, measurement_count: int
    ) -> tuple[Path, Path, Path, str]:
        expected = m5_phase10.PRODUCTION_FIXTURES[0]
        benchmark_name = "M5RecordedReplay/Spot"
        checkout_sha = "a" * 40
        binary_path = Path(directory) / "benchmark"
        payload_path = Path(directory) / "benchmark.json"
        wrapper_path = Path(directory) / "wrapper.json"
        binary_path.write_bytes(b"benchmark")
        payload_path.write_text("{}", encoding="utf-8")
        canonical = "\n".join(
            (
                f"benchmark_name={benchmark_name}",
                "replay_mode=CoreOnly",
                "tier=recorded_medium_v1",
                f"fixture_id={expected.fixture_id}",
                f"workload_id={expected.fixture_id}",
                f"event_count={expected.event_count}",
                f"market={expected.market}",
                f"symbol={expected.symbol}",
                f"price_scale={expected.price_scale}",
                f"quantity_scale={expected.quantity_scale}",
                f"policy={expected.market}",
                f"canonical_log_sha256={expected.replay_sha256}",
                f"distribution_schema={m5_phase10.DISTRIBUTION_SCHEMA}",
                f"distribution_package_id={m5_phase10.PACKAGE_ID}",
                f"distribution_release_tag={m5_phase10.RELEASE_TAG}",
                f"distribution_asset_name={m5_phase10.ASSET_NAME}",
                f"distribution_outer_sha256={m5_phase10.ARCHIVE_SHA256}",
                f"distribution_manifest_sha256={m5_phase10.DISTRIBUTION_MANIFEST_SHA256}",
                "throughput_denominator=wall_time",
                "primary_timer=wall",
                "checksum_methodology_version=M5_PHASE6_REPLAY_CHECKSUM_V1",
                f"logical_items_per_iteration={expected.event_count}",
                "generator_schema=M5_PHASE6_REPLAY_V1",
            )
        )
        wrapper = {
            "schema": "M5_BENCHMARK_WRAPPER_V1",
            "evidence_class": "exploratory",
            "requested_evidence_class": "exploratory",
            "source_provenance": {
                "git_sha": checkout_sha,
                "status": "known",
                "dirty_at_configure": False,
            },
            "binary_provenance": {"sha256": m5_phase10._sha256_file(binary_path)},
            "result_payload": {
                "schema": "google_benchmark_json",
                "sha256": m5_phase10._sha256_file(payload_path),
            },
            "workload_identities": [
                {
                    "benchmark_name": benchmark_name,
                    "canonical_spec_text": canonical,
                    "workload_spec_sha256": m5_phase10._sha256_bytes(
                        canonical.encode("utf-8")
                    ),
                }
            ],
            "measurements": [
                {
                    "name": f"{benchmark_name}/real_time",
                    "real_time_ns": 10.0 + index,
                    "cpu_time_ns": 9.0 + index,
                    "items_per_second": 1000.0,
                }
                for index in range(measurement_count)
            ],
        }
        wrapper_path.write_text(json.dumps(wrapper), encoding="utf-8")
        return wrapper_path, payload_path, binary_path, checkout_sha

    def test_wrapper_measurement_count_must_be_three(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            wrapper_path, payload_path, binary_path, checkout_sha = (
                self._wrapper_for_measurement_validation(directory, measurement_count=2)
            )
            with patch("scripts.benchmark_phase6.validate_wrapper"):
                with self.assertRaises(m5_phase10.VerificationError):
                    m5_phase10._validate_wrapper(
                        wrapper_path,
                        payload_path,
                        m5_phase10.PRODUCTION_FIXTURES[0],
                        "M5RecordedReplay/Spot",
                        binary_path,
                        checkout_sha,
                    )

    def test_replay_warmup_is_outside_repeated_callback(self) -> None:
        source = self._recorded_benchmark_source()
        callback_start = source.index("void run_recorded_replay(")
        callback_end = source.index("} // namespace", callback_start)
        callback = source[callback_start:callback_end]
        main_start = source.index("int main(")
        main = source[main_start:]

        self.assertIn("for ([[maybe_unused]] auto _ : state)", callback)
        self.assertEqual(callback.count("context->executor.run(projection)"), 1)
        self.assertNotIn("warmup_projection", callback)
        self.assertEqual(main.count("run_explicit_warmup(context)"), 1)
        self.assertLess(
            main.index("set_expected_checksum"), main.index("run_explicit_warmup(context)")
        )

    def test_phase10_workflow_preserves_three_repetition_canary_contract(self) -> None:
        workflow = (
            self._REPOSITORY_ROOT / ".github" / "workflows" / "m5-performance.yml"
        ).read_text(encoding="utf-8")

        self.assertIn('cron: "17 3 * * 1"', workflow)
        self.assertIn("timeout-minutes: 45", workflow)
        self.assertEqual(workflow.count("--benchmark_repetitions=3"), 2)
        self.assertEqual(workflow.count("--benchmark_repetitions=5"), 0)
        self.assertIn("M5-REC-SPOT-BTCUSDT-V1", workflow)
        self.assertIn("M5-REC-USDM-BTCUSDT-V1", workflow)
        self.assertNotIn("--benchmark_min_time", workflow)
        self.assertNotIn("--benchmark_iterations", workflow)
        self.assertIn(
            "--target bmd_projection_m5_recorded_replay_benchmark",
            workflow,
        )
        self.assertNotIn("bmd_projection_m5_corpus_validate", workflow)
        self.assertNotIn("Validate Spot medium corpus with the existing Core validator", workflow)
        self.assertNotIn("Validate USD-M medium corpus with the existing Core validator", workflow)

    def test_valid_exact_contract_shaped_package(self) -> None:
        package, contract = self._package()
        with tempfile.TemporaryDirectory() as directory:
            archive = Path(directory) / "package.tar.gz"
            extracted = Path(directory) / "extracted"
            archive.write_bytes(package)
            result = m5_phase10.verify_distribution(archive, extracted, contract=contract)
            self.assertEqual(result["members"], list(contract.member_paths))
            for member in contract.member_paths:
                self.assertTrue((extracted / Path(member)).is_file())

    def test_wrong_outer_sha(self) -> None:
        package, contract = self._package()
        self._verify_invalid(package, replace(contract, archive_sha256="0" * 64))

    def test_wrong_distribution_manifest_sha(self) -> None:
        package, contract = self._package()
        self._verify_invalid(package, replace(contract, manifest_sha256="0" * 64))

    def test_extra_member(self) -> None:
        package, contract = self._package(extra_member="unexpected.txt")
        self._verify_invalid(package, contract)

    def test_missing_member(self) -> None:
        package, contract = self._package(missing_member=contract_member("spot", "manifest.txt"))
        self._verify_invalid(package, contract)

    def test_duplicate_member(self) -> None:
        package, contract = self._package(duplicate_member="distribution-manifest.json")
        self._verify_invalid(package, contract)

    def test_absolute_member(self) -> None:
        package, contract = self._package(
            rename_member={"distribution-manifest.json": "/distribution-manifest.json"}
        )
        self._verify_invalid(package, contract)

    def test_parent_traversal_member(self) -> None:
        package, contract = self._package(
            rename_member={"distribution-manifest.json": "../distribution-manifest.json"}
        )
        self._verify_invalid(package, contract)

    def test_symlink_member(self) -> None:
        package, contract = self._package(special_member="distribution-manifest.json")
        self._verify_invalid(package, contract)

    def test_hardlink_member(self) -> None:
        package, contract = self._package()
        # Rebuild a minimal hardlink archive with the exact outer contract
        # identity updated for this synthetic test package.
        archive_bytes = io.BytesIO()
        with tarfile.open(fileobj=archive_bytes, mode="w:gz") as archive:
            for name in contract.member_paths:
                info = tarfile.TarInfo(name)
                if name == "distribution-manifest.json":
                    info.type = tarfile.LNKTYPE
                    info.linkname = contract.member_paths[1]
                    archive.addfile(info)
                else:
                    info.size = 1
                    archive.addfile(info, io.BytesIO(b"x"))
        package = archive_bytes.getvalue()
        self._verify_invalid(package, replace(
            contract, archive_sha256=m5_phase10._sha256_bytes(package)
        ))

    def test_non_regular_special_member(self) -> None:
        package, contract = self._package(
            special_member="distribution-manifest.json", special_type=tarfile.CHRTYPE
        )
        self._verify_invalid(package, contract)

    def test_payload_sha_mismatch(self) -> None:
        package, contract = self._package()
        original, original_contract = self._package()
        self.assertEqual(package, original)
        # Change one regular payload after the manifest's payload SHA map was
        # fixed, then bind the synthetic contract to the changed outer archive.
        changed = io.BytesIO()
        with tarfile.open(fileobj=io.BytesIO(package), mode="r:gz") as source:
            with tarfile.open(fileobj=changed, mode="w:gz") as target:
                for info in source.getmembers():
                    output_info = tarfile.TarInfo(info.name)
                    output_info.size = info.size
                    data = source.extractfile(info).read() if info.isreg() else b""
                    if info.name == original_contract.fixtures[0].payload_paths[0]:
                        data = b"changed\n"
                        output_info.size = len(data)
                    target.addfile(output_info, io.BytesIO(data))
        changed_package = changed.getvalue()
        self._verify_invalid(
            changed_package,
            replace(contract, archive_sha256=m5_phase10._sha256_bytes(changed_package)),
        )

    def test_wrong_fixture_id(self) -> None:
        package, contract = self._package(
            manifest_mutator=lambda manifest: manifest["fixtures"][0].__setitem__(
                "fixture_id", "WRONG-FIXTURE"
            )
        )
        self._verify_invalid(package, contract)

    def test_wrong_event_count(self) -> None:
        package, contract = self._package(
            manifest_mutator=lambda manifest: manifest["fixtures"][0].__setitem__(
                "event_count", 7
            )
        )
        self._verify_invalid(package, contract)

    def test_wrong_replay_sha(self) -> None:
        package, contract = self._package(
            manifest_mutator=lambda manifest: manifest["fixtures"][0].__setitem__(
                "authoritative_replay_log_sha256", "0" * 64
            )
        )
        self._verify_invalid(package, contract)

    def test_wrong_market(self) -> None:
        package, contract = self._package(
            manifest_mutator=lambda manifest: manifest["fixtures"][0].__setitem__(
                "market", "UsdMPerpetual"
            )
        )
        self._verify_invalid(package, contract)

    def test_wrong_symbol(self) -> None:
        package, contract = self._package(
            manifest_mutator=lambda manifest: manifest["fixtures"][0].__setitem__(
                "symbol", "ETHUSDT"
            )
        )
        self._verify_invalid(package, contract)

    def test_wrong_numeric_spec(self) -> None:
        package, contract = self._package(
            manifest_mutator=lambda manifest: manifest["fixtures"][0]["numeric_spec"].__setitem__(
                "price_scale", 7
            )
        )
        self._verify_invalid(package, contract)

    def test_wrong_package_id(self) -> None:
        package, contract = self._package(
            manifest_mutator=lambda manifest: manifest.__setitem__("package_id", "WRONG")
        )
        self._verify_invalid(package, contract)

    def test_raw_source_included(self) -> None:
        package, contract = self._package(
            manifest_mutator=lambda manifest: manifest.__setitem__("raw_source_included", True)
        )
        self._verify_invalid(package, contract)


def contract_member(market: str, filename: str) -> str:
    prefix = "M5-REC-SPOT-BTCUSDT-V1" if market == "spot" else "M5-REC-USDM-BTCUSDT-V1"
    return f"fixtures/{prefix}/{filename}"


if __name__ == "__main__":
    unittest.main()
