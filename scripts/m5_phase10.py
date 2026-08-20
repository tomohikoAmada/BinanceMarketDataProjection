#!/usr/bin/env python3
"""Fail-closed Phase-10 recorded-medium distribution and result verifier.

The production command has no corpus-identity inputs.  Its release, archive,
manifest, source-authority, and fixture identities are constants bound to the
owner-authorized public asset.  The test suite uses the same helpers with a
synthetic contract so it never performs network I/O.
"""

from __future__ import annotations

import argparse
import dataclasses
import hashlib
import json
import math
import os
import re
import sys
import tarfile
from pathlib import Path, PurePosixPath
from typing import Any, Mapping, Sequence


PUBLIC_DOWNLOAD_URL = (
    "https://github.com/tomohikoAmada/BinanceMarketDataProjection/releases/"
    "download/m5-medium-corpus-v1/m5-medium-recorded-v1.tar.gz"
)
ARCHIVE_SHA256 = "5143521fe9728a7c2ce03522b78be4ba2fd91388cdabac800f4a87e970e4adfb"
DISTRIBUTION_MANIFEST_SHA256 = (
    "13e4c37119e26f32c60f64f73565363d51ef58f59245f4a6678f4bf016cdba65"
)
DISTRIBUTION_SCHEMA = "M5_MEDIUM_CORPUS_DISTRIBUTION_V1"
PACKAGE_ID = "M5-MEDIUM-RECORDED-V1"
REPOSITORY = "tomohikoAmada/BinanceMarketDataProjection"
RELEASE_TAG = "m5-medium-corpus-v1"
ASSET_NAME = "m5-medium-recorded-v1.tar.gz"
MATERIALIZER_VERSION = "m5-recorder-materializer-v1"
SOURCE_RUN_IDENTITY = "preflight/m21-4-24h-20260805T150930Z/"
RECORDER_COMMIT = "cf1e749c7a533e916dbfb685212e5549a38c70dd"
RECORDER_WHEEL_SHA256 = "926615b09ef46130f49a87fe8ab20acb7cfa6313daa67af5b718931bd95ff329"
RECORDER_CONFIG_SHA256 = "a399e647faaac58b5db24e835f1c29e799c70ad0c94ec77b597cac2647cfb734"
SOURCE_INVENTORY_CATALOG_SHA256 = (
    "f7289fcc3383063c5e3b83e65201df29503cf7c9bba227dbb8298dcdb4805d8c"
)
MAX_RESULT_BYTES = 200 * 1024 * 1024
WEEKLY_CANARY_REPETITIONS = 3
STANDARD_AGGREGATES = frozenset(("mean", "median", "stddev", "cv"))
RESULT_NAMES = (
    "spot-benchmark.json",
    "spot-wrapper.json",
    "usdm-benchmark.json",
    "usdm-wrapper.json",
    "performance-summary.txt",
)
HEX64 = re.compile(r"^[0-9a-f]{64}$")
GIT_SHA = re.compile(r"^[0-9a-fA-F]{40}$")


class VerificationError(Exception):
    """A fail-closed distribution or result-contract violation."""


@dataclasses.dataclass(frozen=True)
class FixtureContract:
    fixture_id: str
    event_count: int
    replay_sha256: str
    market: str
    symbol: str
    price_scale: int
    quantity_scale: int
    payload_sha256: Mapping[str, str]

    @property
    def payload_paths(self) -> tuple[str, ...]:
        prefix = f"fixtures/{self.fixture_id}"
        return (
            f"{prefix}/replay.log",
            f"{prefix}/manifest.txt",
            f"{prefix}/corpus_provenance.json",
        )


@dataclasses.dataclass(frozen=True)
class DistributionContract:
    repository: str
    release_tag: str
    asset_name: str
    archive_sha256: str
    manifest_sha256: str
    distribution_schema: str
    package_id: str
    materializer_version: str
    source_run_identity: str
    recorder_commit: str
    recorder_wheel_sha256: str
    recorder_config_sha256: str
    source_inventory_catalog_sha256: str
    fixtures: tuple[FixtureContract, ...]

    @property
    def member_paths(self) -> tuple[str, ...]:
        return ("distribution-manifest.json",) + tuple(
            path for fixture in self.fixtures for path in fixture.payload_paths
        )


PRODUCTION_FIXTURES = (
    FixtureContract(
        fixture_id="M5-REC-SPOT-BTCUSDT-V1",
        event_count=100002,
        replay_sha256="9e9831231192938ac1bd21c90b157ec17e8e2d4e8034131eb21ba57c99b2cc9d",
        market="Spot",
        symbol="BTCUSDT",
        price_scale=8,
        quantity_scale=8,
        payload_sha256={
            "fixtures/M5-REC-SPOT-BTCUSDT-V1/replay.log":
                "9e9831231192938ac1bd21c90b157ec17e8e2d4e8034131eb21ba57c99b2cc9d",
            "fixtures/M5-REC-SPOT-BTCUSDT-V1/manifest.txt":
                "7e6a022574d591aaf7e23a4ea90077244b01c57d5bff58a6014d3b54d57084ed",
            "fixtures/M5-REC-SPOT-BTCUSDT-V1/corpus_provenance.json":
                "ec45f5b9408f8f04fb3550873f2ee42f8017e83c4f292c9a63f9b82117bdbf60",
        },
    ),
    FixtureContract(
        fixture_id="M5-REC-USDM-BTCUSDT-V1",
        event_count=100002,
        replay_sha256="d28ffe19e134e4d5d1c4d57a60762e8884dee676c858587224aebf8afed29afc",
        market="UsdMPerpetual",
        symbol="BTCUSDT",
        price_scale=8,
        quantity_scale=8,
        payload_sha256={
            "fixtures/M5-REC-USDM-BTCUSDT-V1/replay.log":
                "d28ffe19e134e4d5d1c4d57a60762e8884dee676c858587224aebf8afed29afc",
            "fixtures/M5-REC-USDM-BTCUSDT-V1/manifest.txt":
                "8489bad8a1c618bb8f1c95379d2cf81b993f5016b9d16d54f0ba51d06825c65a",
            "fixtures/M5-REC-USDM-BTCUSDT-V1/corpus_provenance.json":
                "189c68768d418d4ef15e16ea7076b9b414731546cc41cbf2f5e9d31510c8f261",
        },
    ),
)

PRODUCTION_CONTRACT = DistributionContract(
    repository=REPOSITORY,
    release_tag=RELEASE_TAG,
    asset_name=ASSET_NAME,
    archive_sha256=ARCHIVE_SHA256,
    manifest_sha256=DISTRIBUTION_MANIFEST_SHA256,
    distribution_schema=DISTRIBUTION_SCHEMA,
    package_id=PACKAGE_ID,
    materializer_version=MATERIALIZER_VERSION,
    source_run_identity=SOURCE_RUN_IDENTITY,
    recorder_commit=RECORDER_COMMIT,
    recorder_wheel_sha256=RECORDER_WHEEL_SHA256,
    recorder_config_sha256=RECORDER_CONFIG_SHA256,
    source_inventory_catalog_sha256=SOURCE_INVENTORY_CATALOG_SHA256,
    fixtures=PRODUCTION_FIXTURES,
)


def _fail(message: str) -> None:
    raise VerificationError(message)


def _require(condition: bool, message: str) -> None:
    if not condition:
        _fail(message)


def _sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    try:
        with path.open("rb") as stream:
            for chunk in iter(lambda: stream.read(1 << 20), b""):
                digest.update(chunk)
    except OSError as error:
        raise VerificationError(f"cannot read {path}: {error}") from error
    return digest.hexdigest()


def _read_tar_member(archive: tarfile.TarFile, info: tarfile.TarInfo) -> bytes:
    stream = archive.extractfile(info)
    if stream is None:
        _fail(f"cannot read regular archive member: {info.name}")
    try:
        return stream.read()
    except OSError as error:
        raise VerificationError(f"cannot read archive member {info.name}: {error}") from error


def _hash_tar_member(archive: tarfile.TarFile, info: tarfile.TarInfo) -> str:
    stream = archive.extractfile(info)
    if stream is None:
        _fail(f"cannot read regular archive member: {info.name}")
    digest = hashlib.sha256()
    try:
        for chunk in iter(lambda: stream.read(1 << 20), b""):
            digest.update(chunk)
    except OSError as error:
        raise VerificationError(f"cannot hash archive member {info.name}: {error}") from error
    return digest.hexdigest()


def _validate_member_name(name: str) -> None:
    _require(name and not name.startswith("/"), f"absolute archive member rejected: {name!r}")
    _require("\\" not in name, f"backslash archive member rejected: {name!r}")
    parts = PurePosixPath(name).parts
    _require(".." not in parts, f"parent traversal archive member rejected: {name!r}")


def _load_archive_members(archive_path: Path) -> dict[str, tarfile.TarInfo]:
    try:
        archive = tarfile.open(archive_path, mode="r:gz")
    except (OSError, tarfile.TarError) as error:
        raise VerificationError(f"cannot open gzip archive: {error}") from error
    members: dict[str, tarfile.TarInfo] = {}
    try:
        for info in archive.getmembers():
            _validate_member_name(info.name)
            _require(info.name not in members, f"duplicate archive member: {info.name}")
            _require(info.isreg(), f"non-regular archive member rejected: {info.name}")
            members[info.name] = info
    except (OSError, tarfile.TarError) as error:
        raise VerificationError(f"cannot inspect archive members: {error}") from error
    finally:
        archive.close()
    return members


def _validate_manifest(manifest: Any, contract: DistributionContract) -> dict[str, Any]:
    _require(isinstance(manifest, dict), "distribution manifest must be a JSON object")
    expected_fields = {
        "distribution_schema": contract.distribution_schema,
        "package_id": contract.package_id,
        "repository": contract.repository,
        "release_tag": contract.release_tag,
        "asset_name": contract.asset_name,
        "owner_distribution_authority": "AUTHORIZED_BY_PROJECT_OWNER",
        "raw_source_included": False,
        "source_run_identity": contract.source_run_identity,
        "recorder_commit": contract.recorder_commit,
        "recorder_wheel_sha256": contract.recorder_wheel_sha256,
        "recorder_config_sha256": contract.recorder_config_sha256,
        "source_inventory_catalog_sha256": contract.source_inventory_catalog_sha256,
        "materializer_version": contract.materializer_version,
    }
    for key, expected in expected_fields.items():
        _require(manifest.get(key) == expected, f"manifest field {key} mismatch")

    fixture_values = manifest.get("fixtures")
    _require(isinstance(fixture_values, list), "manifest fixtures must be an array")
    _require(len(fixture_values) == len(contract.fixtures), "manifest fixture count mismatch")
    by_id: dict[str, Any] = {}
    for value in fixture_values:
        _require(isinstance(value, dict), "manifest fixture entry must be an object")
        fixture_id = value.get("fixture_id")
        _require(isinstance(fixture_id, str) and fixture_id, "manifest fixture ID missing")
        _require(fixture_id not in by_id, f"duplicate manifest fixture ID: {fixture_id}")
        by_id[fixture_id] = value

    expected_ids = {fixture.fixture_id for fixture in contract.fixtures}
    _require(set(by_id) == expected_ids, "manifest fixture identity set mismatch")
    for fixture in contract.fixtures:
        value = by_id[fixture.fixture_id]
        expected_paths = list(fixture.payload_paths)
        _require(value.get("authoritative_replay_log_sha256") == fixture.replay_sha256,
                 f"fixture {fixture.fixture_id} replay SHA mismatch")
        _require(value.get("event_count") == fixture.event_count,
                 f"fixture {fixture.fixture_id} event count mismatch")
        _require(value.get("market") == fixture.market,
                 f"fixture {fixture.fixture_id} market mismatch")
        _require(value.get("symbol") == fixture.symbol,
                 f"fixture {fixture.fixture_id} symbol mismatch")
        _require(value.get("numeric_spec") == {
            "price_scale": fixture.price_scale,
            "quantity_scale": fixture.quantity_scale,
        }, f"fixture {fixture.fixture_id} NumericSpec mismatch")
        _require(value.get("included_relative_payload_paths") == expected_paths,
                 f"fixture {fixture.fixture_id} payload path list mismatch")
        payload_sha = value.get("payload_sha256")
        _require(isinstance(payload_sha, dict),
                 f"fixture {fixture.fixture_id} payload SHA map missing")
        _require(payload_sha == dict(fixture.payload_sha256),
                 f"fixture {fixture.fixture_id} payload SHA map mismatch")
    return manifest


def _safe_extract(archive_path: Path, output_root: Path, member_paths: Sequence[str]) -> None:
    _require(not output_root.exists(), f"safe extraction root must be fresh: {output_root}")
    try:
        output_root.mkdir(parents=True)
        archive = tarfile.open(archive_path, mode="r:gz")
    except (OSError, tarfile.TarError) as error:
        raise VerificationError(f"cannot prepare safe extraction: {error}") from error
    try:
        for member_path in member_paths:
            info = archive.getmember(member_path)
            target = output_root.joinpath(*PurePosixPath(member_path).parts)
            target.parent.mkdir(parents=True, exist_ok=True)
            stream = archive.extractfile(info)
            if stream is None:
                _fail(f"cannot extract verified regular member: {member_path}")
            with target.open("wb") as output:
                for chunk in iter(lambda: stream.read(1 << 20), b""):
                    output.write(chunk)
    except (OSError, KeyError, tarfile.TarError) as error:
        raise VerificationError(f"safe extraction failed: {error}") from error
    finally:
        archive.close()


def verify_distribution(
    archive_path: str | os.PathLike[str],
    output_root: str | os.PathLike[str],
    *,
    contract: DistributionContract = PRODUCTION_CONTRACT,
) -> dict[str, Any]:
    """Verify and safely extract one exact distribution package."""

    archive = Path(archive_path)
    extracted = Path(output_root)
    actual_archive_sha = _sha256_file(archive)
    _require(actual_archive_sha == contract.archive_sha256,
             f"outer archive SHA mismatch: {actual_archive_sha} != {contract.archive_sha256}")

    members = _load_archive_members(archive)
    expected_members = set(contract.member_paths)
    _require(set(members) == expected_members, "archive member inventory mismatch")
    _require(len(members) == 7, f"archive member count must be exactly 7, got {len(members)}")

    try:
        with tarfile.open(archive, mode="r:gz") as opened:
            manifest_bytes = _read_tar_member(opened, members["distribution-manifest.json"])
    except (OSError, tarfile.TarError) as error:
        raise VerificationError(f"cannot read distribution manifest: {error}") from error
    actual_manifest_sha = _sha256_bytes(manifest_bytes)
    _require(actual_manifest_sha == contract.manifest_sha256,
             "distribution manifest SHA mismatch: "
             f"{actual_manifest_sha} != {contract.manifest_sha256}")
    try:
        manifest = json.loads(manifest_bytes.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise VerificationError(f"invalid distribution manifest JSON: {error}") from error
    _validate_manifest(manifest, contract)

    try:
        with tarfile.open(archive, mode="r:gz") as opened:
            for fixture in contract.fixtures:
                for member_path, expected_sha in fixture.payload_sha256.items():
                    actual_sha = _hash_tar_member(opened, members[member_path])
                    _require(actual_sha == expected_sha,
                             f"payload SHA mismatch for {member_path}: "
                             f"{actual_sha} != {expected_sha}")
    except (OSError, tarfile.TarError) as error:
        raise VerificationError(f"cannot verify archive payload hashes: {error}") from error

    _safe_extract(archive, extracted, contract.member_paths)
    return {
        "archive_sha256": actual_archive_sha,
        "distribution_manifest_sha256": actual_manifest_sha,
        "members": list(contract.member_paths),
        "extracted_root": str(extracted),
        "fixtures": [fixture.fixture_id for fixture in contract.fixtures],
    }


def _load_json(path: Path, description: str) -> Any:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        raise VerificationError(f"invalid {description} JSON {path}: {error}") from error


def _finite_positive(value: Any, description: str) -> None:
    _require(isinstance(value, (int, float)) and not isinstance(value, bool),
             f"{description} must be numeric")
    _require(math.isfinite(float(value)) and float(value) > 0, f"{description} must be positive")


def _finite_nonnegative(value: Any, description: str) -> None:
    _require(isinstance(value, (int, float)) and not isinstance(value, bool),
             f"{description} must be numeric")
    _require(math.isfinite(float(value)) and float(value) >= 0,
             f"{description} must be nonnegative")


def _is_iteration_name(name: Any, expected_name: str) -> bool:
    # Google Benchmark appends the selected timer to the JSON/report name
    # when UseRealTime() is active. The canonical workload identity remains
    # the frozen name without that measurement decoration.
    return name in (expected_name, f"{expected_name}/real_time")


def _validate_benchmark_payload(
    path: Path, expected_name: str,
    required_repetitions: int = WEEKLY_CANARY_REPETITIONS,
) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    payload = _load_json(path, "benchmark payload")
    _require(isinstance(payload, dict), "benchmark payload must be an object")
    _require(isinstance(payload.get("context"), dict), "benchmark payload context missing")
    entries = payload.get("benchmarks")
    _require(isinstance(entries, list) and entries, "benchmark payload benchmarks missing")
    iterations: list[dict[str, Any]] = []
    aggregates: list[dict[str, Any]] = []
    aggregate_names = {
        **{
            f"{expected_name}_{suffix}": suffix
            for suffix in ("mean", "median", "stddev", "cv")
        },
        **{
            f"{expected_name}/real_time_{suffix}": suffix
            for suffix in ("mean", "median", "stddev", "cv")
        },
    }
    for entry in entries:
        _require(isinstance(entry, dict), "benchmark entry must be an object")
        _require(entry.get("error_occurred") is not True, "benchmark reports error_occurred")
        _require(entry.get("skipped") is not True and not entry.get("skip_message"),
                 "benchmark reports SkipWithError")
        run_type = entry.get("run_type", "iteration")
        if run_type == "aggregate":
            aggregate_name = aggregate_names.get(entry.get("name"))
            _require(aggregate_name is not None,
                     f"unexpected aggregate benchmark name: {entry.get('name')}")
            _require(entry.get("aggregate_name") == aggregate_name,
                     f"aggregate statistic does not match benchmark name: {entry.get('name')}")
            if aggregate_name in ("mean", "median"):
                _finite_positive(entry.get("real_time"), "aggregate real_time")
            else:
                _finite_nonnegative(entry.get("real_time"), "aggregate real_time")
            aggregates.append(entry)
            continue
        _require(run_type == "iteration", f"unexpected benchmark run_type: {run_type}")
        _require(_is_iteration_name(entry.get("name"), expected_name),
                 f"unexpected benchmark name: {entry.get('name')}")
        _require(entry.get("repetitions") == required_repetitions,
                 f"benchmark repetitions must be exactly {required_repetitions}")
        _finite_positive(entry.get("iterations"), "benchmark iterations")
        _finite_positive(entry.get("real_time"), "benchmark real_time")
        _finite_positive(entry.get("cpu_time"), "benchmark cpu_time")
        _finite_positive(entry.get("items_per_second"), "benchmark items_per_second")
        iterations.append(entry)
    _require(len(iterations) == required_repetitions,
             f"expected {required_repetitions} benchmark repetitions, got {len(iterations)}")
    indices = [entry.get("repetition_index") for entry in iterations]
    if any(index is not None for index in indices):
        _require(all(isinstance(index, int) and not isinstance(index, bool)
                     for index in indices),
                 "benchmark repetition indices must be integers when emitted")
        _require(sorted(indices) == list(range(required_repetitions)),
                 "benchmark repetition indices are not contiguous")
    aggregate_values = [entry["aggregate_name"] for entry in aggregates]
    _require(len(aggregate_values) == len(STANDARD_AGGREGATES)
             and set(aggregate_values) == STANDARD_AGGREGATES,
             "benchmark aggregate set must be exactly mean, median, stddev, cv")
    return iterations, aggregates


def _canonical_fields(text: str) -> dict[str, str]:
    fields: dict[str, str] = {}
    for line in text.splitlines():
        _require("=" in line, "workload canonical text has malformed field")
        key, value = line.split("=", 1)
        _require(key and key not in fields, "workload canonical text has duplicate field")
        fields[key] = value
    return fields


def _validate_wrapper(
    wrapper_path: Path,
    payload_path: Path,
    expected: FixtureContract,
    expected_benchmark_name: str,
    binary_path: Path,
    checkout_sha: str,
    required_repetitions: int = WEEKLY_CANARY_REPETITIONS,
) -> tuple[dict[str, Any], dict[str, str], list[dict[str, Any]]]:
    # Reuse the accepted Phase-6 wrapper contract in addition to the exact
    # Phase-10 identity checks below.
    try:
        try:
            from scripts import benchmark_phase6
        except ModuleNotFoundError:
            # Direct execution (the workflow's `python3 scripts/...` form)
            # places scripts/ on sys.path rather than the repository root.
            import benchmark_phase6

        wrapper = _load_json(wrapper_path, "wrapper")
        benchmark_phase6.validate_wrapper(
            str(wrapper_path), wrapper, allow_exploratory=True, binary=str(binary_path),
            require_inventory=False,
        )
    except VerificationError:
        raise
    except Exception as error:  # benchmark_phase6 uses its own ValidationError type
        raise VerificationError(f"existing Phase-6 wrapper validation failed: {error}") from error

    _require(wrapper.get("schema") == "M5_BENCHMARK_WRAPPER_V1", "wrong wrapper schema")
    _require(wrapper.get("evidence_class") == "exploratory", "wrapper evidence class is not exploratory")
    _require(wrapper.get("requested_evidence_class") == "exploratory",
             "wrapper requested evidence class is not exploratory")
    source = wrapper.get("source_provenance")
    _require(isinstance(source, dict), "wrapper source provenance missing")
    _require(source.get("git_sha") == checkout_sha, "wrapper source SHA does not match checkout")
    _require(source.get("status") == "known", "wrapper source provenance is not known")
    _require(source.get("dirty_at_configure") is False, "wrapper was configured from a dirty tree")
    _require(_sha256_file(binary_path) == wrapper["binary_provenance"]["sha256"],
             "wrapper binary SHA does not match executable")

    result_payload = wrapper.get("result_payload")
    _require(isinstance(result_payload, dict), "wrapper result payload missing")
    _require(result_payload.get("schema") == "google_benchmark_json",
             "wrapper result payload schema is not Google Benchmark JSON")
    _require(result_payload.get("sha256") == _sha256_file(payload_path),
             "wrapper payload SHA does not match benchmark payload")

    identities = wrapper.get("workload_identities")
    _require(isinstance(identities, list) and len(identities) == 1,
             "wrapper must contain exactly one recorded workload identity")
    identity = identities[0]
    _require(identity.get("benchmark_name") == expected_benchmark_name,
             "wrapper workload benchmark name mismatch")
    canonical = identity.get("canonical_spec_text")
    _require(isinstance(canonical, str) and canonical, "wrapper canonical workload text missing")
    _require(identity.get("workload_spec_sha256") == _sha256_bytes(canonical.encode("utf-8")),
             "wrapper workload spec SHA mismatch")
    fields = _canonical_fields(canonical)
    exact_fields = {
        "benchmark_name": expected_benchmark_name,
        "replay_mode": "CoreOnly",
        "tier": "recorded_medium_v1",
        "fixture_id": expected.fixture_id,
        "workload_id": expected.fixture_id,
        "event_count": str(expected.event_count),
        "market": expected.market,
        "symbol": expected.symbol,
        "price_scale": str(expected.price_scale),
        "quantity_scale": str(expected.quantity_scale),
        "policy": expected.market,
        "canonical_log_sha256": expected.replay_sha256,
        "distribution_schema": DISTRIBUTION_SCHEMA,
        "distribution_package_id": PACKAGE_ID,
        "distribution_release_tag": RELEASE_TAG,
        "distribution_asset_name": ASSET_NAME,
        "distribution_outer_sha256": ARCHIVE_SHA256,
        "distribution_manifest_sha256": DISTRIBUTION_MANIFEST_SHA256,
        "throughput_denominator": "wall_time",
        "primary_timer": "wall",
        "checksum_methodology_version": "M5_PHASE6_REPLAY_CHECKSUM_V1",
        "logical_items_per_iteration": str(expected.event_count),
    }
    for key, value in exact_fields.items():
        _require(fields.get(key) == value, f"workload identity field {key} mismatch")
    _require(fields.get("generator_schema") == "M5_PHASE6_REPLAY_V1",
             "workload generator schema mismatch")
    _require(identity.get("event_count") is None or identity.get("event_count") == expected.event_count,
             "wrapper workload event count mismatch")

    measurements = wrapper.get("measurements")
    _require(isinstance(measurements, list) and len(measurements) == required_repetitions,
             f"wrapper must contain {required_repetitions} measurements")
    for measurement in measurements:
        _require(_is_iteration_name(measurement.get("name"), expected_benchmark_name),
                 "wrapper measurement benchmark name mismatch")
        _finite_positive(measurement.get("real_time_ns"), "wrapper real_time_ns")
        _finite_positive(measurement.get("cpu_time_ns"), "wrapper cpu_time_ns")
        _finite_positive(measurement.get("items_per_second"), "wrapper items_per_second")
    return wrapper, fields, measurements


def _summary(
    checkout_sha: str,
    wrappers: Sequence[tuple[Path, dict[str, Any], FixtureContract, str, list[dict[str, Any]], list[dict[str, Any]]]],
) -> str:
    lines = [
        "M5 Phase-10 recorded medium performance summary (derived reporting)",
        f"CHECKOUT_SHA={checkout_sha}",
        "EVIDENCE_CLASS=EXPLORATORY / NONBLOCKING REPORTING",
        "RUNNER_COMPILER_IDENTITY=recorded in each wrapper environment/build identity",
        f"DISTRIBUTION_PACKAGE={PACKAGE_ID}",
        f"DISTRIBUTION_RELEASE_TAG={RELEASE_TAG}",
        f"DISTRIBUTION_ASSET_NAME={ASSET_NAME}",
        f"DISTRIBUTION_OUTER_SHA256={ARCHIVE_SHA256}",
        f"DISTRIBUTION_MANIFEST_SHA256={DISTRIBUTION_MANIFEST_SHA256}",
        "NUMERIC_PERFORMANCE_GATE=NO",
        "CONTAINER_DECISION=KEEP_STD_MAP",
        "PRODUCTION_CONTAINER=std::map",
        "PRODUCTION_MIGRATION=NO",
    ]
    for _, wrapper, fixture, benchmark_name, measurements, aggregates in wrappers:
        build = wrapper.get("build_identity", {})
        compiler = build.get("compiler", {})
        environment = wrapper.get("environment_identity", {})
        lines.extend([
            "",
            f"FIXTURE_ID={fixture.fixture_id}",
            f"EVENT_COUNT={fixture.event_count}",
            f"REPLAY_SHA256={fixture.replay_sha256}",
            f"MARKET={fixture.market}",
            f"SYMBOL={fixture.symbol}",
            f"NUMERIC_SPEC=price scale {fixture.price_scale} / quantity scale {fixture.quantity_scale}",
            f"BENCHMARK_NAME={benchmark_name}",
            f"COMPILER={compiler.get('id')} {compiler.get('version')}",
            f"PLATFORM={environment.get('os_name')} {environment.get('os_version')} "
            f"{environment.get('architecture')}",
            "REPORTED_REPETITION_MEASUREMENTS=",
        ])
        for index, measurement in enumerate(measurements, start=1):
            lines.append(
                f"  repetition_{index}: real_time_ns={measurement.get('real_time_ns')} "
                f"cpu_time_ns={measurement.get('cpu_time_ns')} "
                f"items_per_second={measurement.get('items_per_second')}"
            )
        lines.append("GOOGLE_BENCHMARK_AGGREGATES=")
        for aggregate in aggregates:
            lines.append(
                f"  {aggregate.get('name')}: real_time={aggregate.get('real_time')} "
                f"time_unit={aggregate.get('time_unit')}"
            )
    return "\n".join(lines) + "\n"


def validate_results(
    spot_payload: str | os.PathLike[str],
    spot_wrapper: str | os.PathLike[str],
    usdm_payload: str | os.PathLike[str],
    usdm_wrapper: str | os.PathLike[str],
    binary: str | os.PathLike[str],
    checkout_sha: str,
    summary_out: str | os.PathLike[str],
) -> str:
    _require(GIT_SHA.fullmatch(checkout_sha) is not None, "checkout SHA must be a 40-character Git SHA")
    executable = Path(binary)
    _require(executable.is_file(), f"benchmark executable not found: {executable}")
    output = Path(summary_out)
    _require(output.name == "performance-summary.txt", "summary must be performance-summary.txt")
    result_root = output.parent
    expected_runs = (
        (Path(spot_payload), Path(spot_wrapper), PRODUCTION_FIXTURES[0], "M5RecordedReplay/Spot"),
        (Path(usdm_payload), Path(usdm_wrapper), PRODUCTION_FIXTURES[1], "M5RecordedReplay/UsdMPerpetual"),
    )
    wrapper_records = []
    for payload, wrapper_path, fixture, benchmark_name in expected_runs:
        iterations, aggregates = _validate_benchmark_payload(
            payload, benchmark_name, WEEKLY_CANARY_REPETITIONS
        )
        wrapper, fields, measurements = _validate_wrapper(
            wrapper_path, payload, fixture, benchmark_name, executable, checkout_sha,
            WEEKLY_CANARY_REPETITIONS,
        )
        wrapper_records.append((wrapper_path, wrapper, fixture, benchmark_name, measurements, aggregates))

    result_root.mkdir(parents=True, exist_ok=True)
    output.write_text(_summary(checkout_sha, wrapper_records), encoding="utf-8")
    expected_paths = {result_root / name for name in RESULT_NAMES}
    actual_paths = {path for path in result_root.iterdir() if path.is_file()}
    _require(actual_paths == expected_paths, "performance result directory contains unexpected files")
    total_bytes = sum(path.stat().st_size for path in expected_paths)
    _require(total_bytes <= MAX_RESULT_BYTES,
             f"five-file performance result payload exceeds {MAX_RESULT_BYTES} bytes")
    return output.read_text(encoding="utf-8")


def _main() -> int:
    parser = argparse.ArgumentParser(description="M5 Phase-10 distribution/result verifier")
    subparsers = parser.add_subparsers(dest="command", required=True)

    distribution = subparsers.add_parser("verify-distribution")
    distribution.add_argument("archive")
    distribution.add_argument("output_root")

    results = subparsers.add_parser("validate-results")
    results.add_argument("--spot-benchmark", required=True)
    results.add_argument("--spot-wrapper", required=True)
    results.add_argument("--usdm-benchmark", required=True)
    results.add_argument("--usdm-wrapper", required=True)
    results.add_argument("--binary", required=True)
    results.add_argument("--checkout-sha", required=True)
    results.add_argument("--summary-out", required=True)

    args = parser.parse_args()
    try:
        if args.command == "verify-distribution":
            result = verify_distribution(args.archive, args.output_root)
            print(json.dumps(result, sort_keys=True))
        else:
            validate_results(
                args.spot_benchmark,
                args.spot_wrapper,
                args.usdm_benchmark,
                args.usdm_wrapper,
                args.binary,
                args.checkout_sha,
                args.summary_out,
            )
            print("Phase-10 distribution-bound benchmark results PASS")
        return 0
    except (VerificationError, OSError) as error:
        print(f"M5_PHASE10_VERIFICATION_FAILED: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(_main())
