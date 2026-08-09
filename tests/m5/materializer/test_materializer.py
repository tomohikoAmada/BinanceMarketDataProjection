from __future__ import annotations

import hashlib
import json
import os
import shutil
import sqlite3
import struct
import subprocess
import tempfile
import unittest
from pathlib import Path
from typing import Any

from tools import m5_recorded_corpus_materializer as materializer


def _head(major: int, value: int) -> bytes:
    if value < 24:
        return bytes([(major << 5) | value])
    if value <= 0xFF:
        return bytes([(major << 5) | 24, value])
    if value <= 0xFFFF:
        return bytes([(major << 5) | 25]) + value.to_bytes(2, "big")
    if value <= 0xFFFFFFFF:
        return bytes([(major << 5) | 26]) + value.to_bytes(4, "big")
    return bytes([(major << 5) | 27]) + value.to_bytes(8, "big")


def _cbor(value: object) -> bytes:
    if value is None:
        return b"\xf6"
    if value is False:
        return b"\xf4"
    if value is True:
        return b"\xf5"
    if isinstance(value, int):
        return _head(0, value) if value >= 0 else _head(1, -1 - value)
    if isinstance(value, bytes):
        return _head(2, len(value)) + value
    if isinstance(value, str):
        body = value.encode()
        return _head(3, len(body)) + body
    if isinstance(value, (list, tuple)):
        return _head(4, len(value)) + b"".join(_cbor(item) for item in value)
    if isinstance(value, dict):
        items = [(_cbor(key), _cbor(item)) for key, item in value.items()]
        items.sort(key=lambda item: (len(item[0]), item[0]))
        return _head(5, len(items)) + b"".join(key + item for key, item in items)
    raise TypeError(value)


def _crc32c(data: bytes) -> int:
    crc = 0xFFFFFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0x82F63B78 if crc & 1 else 0)
    return crc ^ 0xFFFFFFFF


def _sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _envelope(
    *,
    market: str,
    stream: str,
    timestamp: int,
    payload: dict[str, object],
    sequence: dict[str, int],
    symbol: str = "BTCUSDT",
    connection: str = "connection-1",
) -> dict[str, object]:
    return {
        "schema_version": "event-envelope.v1",
        "venue": "binance",
        "market": market,
        "symbol": symbol,
        "stream": stream,
        "module": "deterministic.test.v1",
        "connection_id": connection,
        "collector_instance_id": "collector-1",
        "collector_version": "test-1",
        "receive_time_utc_ns": timestamp,
        "receive_monotonic_ns": timestamp - materializer.SOURCE_START_NS + 1,
        "exchange_event_time": None,
        "exchange_transaction_time": None,
        "exchange_trade_time": None,
        "source_sequence": sequence,
        "payload_encoding": (
            "utf-8-json-provenance" if stream == "depth_snapshot" else "utf-8-json"
        ),
        "raw_payload": json.dumps(payload, sort_keys=True, separators=(",", ":")).encode(),
        "capture_flags": ["rest_snapshot"] if stream == "depth_snapshot" else [],
    }


def _snapshot(market: str, timestamp: int, last_update_id: int = 100) -> dict[str, object]:
    schema = (
        "binance-spot-depth-snapshot-provenance.v2"
        if market == "spot"
        else "binance-usdm-depth-snapshot-provenance.v1"
    )
    model = {
        "lastUpdateId": last_update_id,
        "bids": [["100.00", "1.000"], ["99.00", "2.000"]],
        "asks": [["101.00", "1.500"], ["102.00", "2.500"]],
    }
    payload = {"schema_version": schema, "response": {"model": model}}
    return _envelope(
        market=market,
        stream="depth_snapshot",
        timestamp=timestamp,
        payload=payload,
        sequence={"lastUpdateId": last_update_id},
        connection="rest-1",
    )


def _depth(
    market: str,
    timestamp: int,
    first: int,
    final: int,
    previous: int | None,
    *,
    symbol: str = "BTCUSDT",
) -> dict[str, object]:
    payload: dict[str, object] = {
        "e": "depthUpdate",
        "s": symbol,
        "U": first,
        "u": final,
        "b": [["100.00", "1.125"]],
        "a": [["101.00", "0.000"]],
    }
    sequence = {"U": first, "u": final}
    if market == "um_perpetual" and previous is not None:
        payload["pu"] = previous
        sequence["pu"] = previous
    return _envelope(
        market=market,
        stream="diff_depth",
        timestamp=timestamp,
        payload=payload,
        sequence=sequence,
        symbol=symbol,
    )


def _raw_chunk(chunk_hex: str, market: str, stream: str, records: list[dict[str, object]]) -> bytes:
    header = {
        "chunk_id": bytes.fromhex(chunk_hex),
        "chunk_schema_version": "raw-chunk.v1",
        "collector_instance_id": "collector-1",
        "collector_version": "test-1",
        "created_at_utc_ns": records[0]["receive_time_utc_ns"],
        "envelope_schema_version": "event-envelope.v1",
        "format": "bmdr-raw-chunk",
        "market": market,
        "max_frame_bytes": 16 * 1024 * 1024,
        "stream": stream,
        "symbol": records[0]["symbol"],
    }
    body = _cbor(header)
    fixed_without_crc = struct.pack(
        ">8sBBHII", b"BMRCHNK\x1a", 1, 0, 0xFEFF, 0, len(body)
    )
    output = fixed_without_crc + struct.pack(">I", _crc32c(fixed_without_crc + body)) + body
    for envelope in records:
        frame = _cbor(envelope)
        prefix = struct.pack(">IHH", len(frame), 0, 0)
        output += prefix + struct.pack(">I", _crc32c(prefix + frame)) + frame
    return output


class ArchiveBuilder:
    def __init__(self, root: Path, market: str, *, compressed: bool = False) -> None:
        self.root = root
        self.market = market
        self.compressed = compressed
        self.entries: list[dict[str, object]] = []
        self.manifests: dict[str, dict[str, object]] = {}
        (root / "chunks").mkdir(parents=True)
        (root / "manifests").mkdir()
        self.catalog_path = root / "catalog.sqlite"

    def add_chunk(
        self, chunk_number: int, stream: str, records: list[dict[str, object]]
    ) -> str:
        chunk_hex = f"{chunk_number:032x}"
        chunk_id = (
            f"{chunk_hex[:8]}-{chunk_hex[8:12]}-{chunk_hex[12:16]}-"
            f"{chunk_hex[16:20]}-{chunk_hex[20:]}"
        )
        raw = _raw_chunk(chunk_hex, self.market, stream, records)
        artifact = self.root / "chunks" / f"{chunk_hex}.bmdr"
        artifact.write_bytes(raw)
        stored = raw
        encoding = "raw-test"
        if self.compressed:
            zstd = shutil.which("zstd")
            if zstd is None:
                raise unittest.SkipTest("zstd executable unavailable")
            compressed_path = artifact.with_suffix(".bmdr.zst")
            subprocess.run(
                [zstd, "--quiet", "--force", "--content-size", str(artifact), "-o", str(compressed_path)],
                check=True,
            )
            artifact.unlink()
            artifact = compressed_path
            stored = artifact.read_bytes()
            encoding = "zstd-frame.v1"
        manifest: dict[str, object] = {
            "manifest_schema_version": "raw-chunk-manifest.v1",
            "chunk_id": chunk_id,
            "chunk_schema_version": "raw-chunk.v1",
            "envelope_schema_version": "event-envelope.v1",
            "market": self.market,
            "symbol": records[0]["symbol"],
            "stream": stream,
            "record_count": len(records),
            "uncompressed_bytes": len(raw),
            "stored_bytes": len(stored),
            "uncompressed_sha256": _sha(raw),
            "stored_sha256": _sha(stored),
            "complete": True,
            "gap": False,
            "resync": False,
            "recovered": False,
            "capture_flags": [],
        }
        manifest_path = self.root / "manifests" / f"{chunk_hex}.manifest.json"
        manifest_bytes = (
            json.dumps(manifest, sort_keys=True, separators=(",", ":")) + "\n"
        ).encode()
        manifest_path.write_bytes(manifest_bytes)
        self.manifests[chunk_id] = manifest
        self.entries.append(
            {
                "source_order": len(self.entries),
                "chunk_id": chunk_id,
                "artifact_relative_path": artifact.relative_to(self.root).as_posix(),
                "manifest_relative_path": manifest_path.relative_to(self.root).as_posix(),
                "manifest_sha256": _sha(manifest_bytes),
                "storage_encoding": encoding,
            }
        )
        return chunk_id

    def finish(self) -> Path:
        connection = sqlite3.connect(self.catalog_path)
        connection.execute(
            "CREATE TABLE chunks (chunk_id TEXT PRIMARY KEY,state TEXT,record_count INTEGER,"
            "uncompressed_bytes INTEGER,stored_bytes INTEGER,uncompressed_sha256 TEXT,"
            "stored_sha256 TEXT)"
        )
        for entry in self.entries:
            manifest = self.manifests[entry["chunk_id"]]
            connection.execute(
                "INSERT INTO chunks VALUES (?,?,?,?,?,?,?)",
                (
                    entry["chunk_id"],
                    "SEALED",
                    manifest["record_count"],
                    manifest["uncompressed_bytes"],
                    manifest["stored_bytes"],
                    manifest["uncompressed_sha256"],
                    manifest["stored_sha256"],
                ),
            )
        connection.commit()
        connection.close()
        return self.write_inventory()

    def write_inventory(self) -> Path:
        inventory = {
            "schema_version": materializer.SOURCE_INVENTORY_SCHEMA,
            "recorder_repository": materializer.RECORDER_REPOSITORY,
            "recorder_commit": materializer.RECORDER_COMMIT,
            "deployed_wheel_sha256": materializer.RECORDER_WHEEL_SHA256,
            "production_config_sha256": materializer.RECORDER_CONFIG_SHA256,
            "run_identity": materializer.SOURCE_RUN_IDENTITY,
            "source_start_utc": materializer.SOURCE_START_UTC,
            "source_end_utc": materializer.SOURCE_END_UTC,
            "catalog_relative_path": "catalog.sqlite",
            "catalog_sha256": hashlib.sha256(self.catalog_path.read_bytes()).hexdigest(),
            "complete_inventory": True,
            "test_archive": not self.compressed,
            "chunks": self.entries,
        }
        path = self.root / "m5_source_inventory.json"
        path.write_text(
            json.dumps(inventory, sort_keys=True, separators=(",", ":")) + "\n",
            encoding="utf-8",
        )
        return path

    def rewrite_manifest_and_catalog(self, chunk_id: str, raw: bytes) -> None:
        entry = next(item for item in self.entries if item["chunk_id"] == chunk_id)
        artifact = self.root / str(entry["artifact_relative_path"])
        artifact.write_bytes(raw)
        manifest = self.manifests[chunk_id]
        manifest["stored_bytes"] = len(raw)
        manifest["uncompressed_bytes"] = len(raw)
        manifest["stored_sha256"] = _sha(raw)
        manifest["uncompressed_sha256"] = _sha(raw)
        manifest_path = self.root / str(entry["manifest_relative_path"])
        body = (json.dumps(manifest, sort_keys=True, separators=(",", ":")) + "\n").encode()
        manifest_path.write_bytes(body)
        entry["manifest_sha256"] = _sha(body)
        connection = sqlite3.connect(self.catalog_path)
        connection.execute(
            "UPDATE chunks SET record_count=?,uncompressed_bytes=?,stored_bytes=?,"
            "uncompressed_sha256=?,stored_sha256=? WHERE chunk_id=?",
            (
                manifest["record_count"],
                len(raw),
                len(raw),
                _sha(raw),
                _sha(raw),
                chunk_id,
            ),
        )
        connection.commit()
        connection.close()
        self.write_inventory()


def _valid_builder(root: Path, market_name: str, *, compressed: bool = False) -> ArchiveBuilder:
    market = "spot" if market_name == "spot" else "um_perpetual"
    start = materializer.SOURCE_START_NS
    builder = ArchiveBuilder(root, market, compressed=compressed)
    builder.add_chunk(1, "depth_snapshot", [_snapshot(market, start + 100)])
    if market == "spot":
        first = [
            _depth(market, start + 10, 90, 99, None),
            _depth(market, start + 20, 99, 100, None),
            _depth(market, start + 110, 99, 101, None),
            _depth(market, start + 120, 102, 102, None),
        ]
        second = [
            _depth(market, start + 130, 103, 103, None),
            _depth(market, start + 140, 104, 104, None),
        ]
    else:
        first = [
            _depth(market, start + 10, 90, 99, 98),
            _depth(market, start + 20, 99, 100, 99),
            _depth(market, start + 110, 99, 101, 100),
            _depth(market, start + 120, 102, 102, 101),
        ]
        second = [
            _depth(market, start + 130, 103, 103, 102),
            _depth(market, start + 140, 104, 104, 103),
        ]
    builder.add_chunk(2, "diff_depth", first)
    builder.add_chunk(3, "diff_depth", second)
    builder.finish()
    return builder


class MaterializerTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        validator_value = os.environ.get("BMD_M5_CORPUS_VALIDATOR")
        self.validator = Path(validator_value) if validator_value else None
        adapter_validator_value = os.environ.get("BMD_M5_ADAPTER_CORPUS_VALIDATOR")
        self.adapter_validator = (
            Path(adapter_validator_value) if adapter_validator_value else None
        )

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def _materialize(
        self,
        builder: ArchiveBuilder,
        market: str,
        name: str,
        *,
        timestamp: str = "2026-08-09T00:00:00Z",
        target: int = 3,
        allow_test_archive: bool = True,
    ) -> materializer.MaterializationResult:
        inventory = builder.root / "m5_source_inventory.json"
        validators = [
            validator
            for validator in (self.validator, self.adapter_validator)
            if validator is not None
        ]
        return materializer.materialize(
            source_root=builder.root,
            inventory_path=inventory,
            output_directory=self.root / name,
            market=market,
            target_live_updates=target,
            price_scale=8,
            quantity_scale=8,
            conversion_timestamp=timestamp,
            validators=validators,
            allow_test_archive=allow_test_archive,
        )

    def _expect_failure(
        self, builder: ArchiveBuilder, market: str, text: str, name: str = "failure"
    ) -> None:
        with self.assertRaisesRegex(materializer.MaterializationError, text):
            self._materialize(builder, market, name)

    def test_crc32c_known_vector(self) -> None:
        self.assertEqual(materializer.crc32c(b"123456789"), 0xE3069283)

    def test_spot_and_usdm_materialize_deterministically_across_chunk_boundary(self) -> None:
        for market in ("spot", "usdm"):
            source = self.root / f"source-{market}"
            builder = _valid_builder(source, market)
            first = self._materialize(builder, market, f"{market}-first")
            second = self._materialize(builder, market, f"{market}-second")
            self.assertEqual(first.replay_log_sha256, second.replay_log_sha256)
            self.assertEqual(first.event_count, 5)
            self.assertEqual(first.final_update_id, 104 if market == "spot" else 103)
            for filename in ("replay.log", "manifest.txt", "corpus_provenance.json"):
                self.assertEqual(
                    (first.output_directory / filename).read_bytes(),
                    (second.output_directory / filename).read_bytes(),
                )
            provenance = json.loads(
                (first.output_directory / "corpus_provenance.json").read_text()
            )
            self.assertEqual(provenance["selected_live_updates_after_synchronization"], 3)
            self.assertEqual(len(provenance["source_raw_chunks"]), 3)
            self.assertEqual(provenance["materializer_version"], materializer.MATERIALIZER_VERSION)

    def test_conversion_timestamp_is_metadata_only(self) -> None:
        builder = _valid_builder(self.root / "source", "spot")
        first = self._materialize(builder, "spot", "first", timestamp="2026-08-09T00:00:00Z")
        second = self._materialize(builder, "spot", "second", timestamp="2026-08-10T00:00:00Z")
        self.assertEqual(first.replay_log_sha256, second.replay_log_sha256)
        self.assertEqual(
            (first.output_directory / "manifest.txt").read_bytes(),
            (second.output_directory / "manifest.txt").read_bytes(),
        )
        left = first.provenance.copy()
        right = second.provenance.copy()
        left.pop("conversion_timestamp")
        right.pop("conversion_timestamp")
        self.assertEqual(left, right)

    def test_zstd_sealed_source_when_decoder_is_available(self) -> None:
        if shutil.which("zstd") is None:
            self.skipTest("zstd executable unavailable")
        builder = _valid_builder(self.root / "compressed", "spot", compressed=True)
        result = self._materialize(
            builder, "spot", "compressed-output", allow_test_archive=False
        )
        self.assertEqual(result.event_count, 5)

    def test_corrupt_stored_hash_fails_closed(self) -> None:
        builder = _valid_builder(self.root / "source", "spot")
        artifact = builder.root / str(builder.entries[1]["artifact_relative_path"])
        artifact.write_bytes(artifact.read_bytes() + b"corruption")
        self._expect_failure(builder, "spot", "stored chunk size mismatch")

    def test_missing_chunk_and_incomplete_inventory_fail_closed(self) -> None:
        missing = _valid_builder(self.root / "missing", "spot")
        artifact = missing.root / str(missing.entries[1]["artifact_relative_path"])
        artifact.unlink()
        self._expect_failure(missing, "spot", "source chunk artifact is missing", "missing-output")

        incomplete = _valid_builder(self.root / "incomplete", "spot")
        inventory_path = incomplete.root / "m5_source_inventory.json"
        inventory = json.loads(inventory_path.read_text())
        inventory["chunks"].pop()
        inventory_path.write_text(
            json.dumps(inventory, sort_keys=True, separators=(",", ":")) + "\n"
        )
        self._expect_failure(
            incomplete,
            "spot",
            "Catalog/inventory chunk set mismatch",
            "incomplete-output",
        )

    def test_corrupt_crc_with_matching_outer_hash_fails_closed(self) -> None:
        builder = _valid_builder(self.root / "source", "spot")
        chunk_id = str(builder.entries[1]["chunk_id"])
        artifact = builder.root / str(builder.entries[1]["artifact_relative_path"])
        raw = bytearray(artifact.read_bytes())
        raw[-1] ^= 1
        builder.rewrite_manifest_and_catalog(chunk_id, bytes(raw))
        self._expect_failure(builder, "spot", "Raw frame CRC32C mismatch")

    def test_missing_snapshot_fails_closed(self) -> None:
        start = materializer.SOURCE_START_NS
        builder = ArchiveBuilder(self.root / "source", "spot")
        builder.add_chunk(1, "diff_depth", [_depth("spot", start + 1, 99, 101, None)])
        builder.finish()
        self._expect_failure(builder, "spot", "required source streams are incomplete")

    def test_wrong_symbol_and_market_fail_closed(self) -> None:
        start = materializer.SOURCE_START_NS
        wrong_symbol = ArchiveBuilder(self.root / "wrong-symbol", "spot")
        snapshot = _snapshot("spot", start + 100)
        snapshot["symbol"] = "ETHUSDT"
        wrong_symbol.add_chunk(1, "depth_snapshot", [snapshot])
        wrong_symbol.add_chunk(
            2,
            "diff_depth",
            [_depth("spot", start + 110, 99, 101, None, symbol="ETHUSDT")],
        )
        wrong_symbol.finish()
        self._expect_failure(wrong_symbol, "spot", "raw manifest symbol mismatch", "symbol")

        wrong_market = _valid_builder(self.root / "wrong-market", "usdm")
        self._expect_failure(
            wrong_market, "spot", "inventory contains no chunks for requested market", "market"
        )

    def test_missing_authority_metadata_fails_closed(self) -> None:
        builder = _valid_builder(self.root / "source", "spot")
        inventory_path = builder.root / "m5_source_inventory.json"
        inventory = json.loads(inventory_path.read_text())
        inventory.pop("production_config_sha256")
        inventory_path.write_text(json.dumps(inventory, sort_keys=True, separators=(",", ":")) + "\n")
        self._expect_failure(builder, "spot", "source inventory fields do not match schema")

    def test_spot_no_bridge_and_forward_gap_fail_closed(self) -> None:
        start = materializer.SOURCE_START_NS
        no_bridge = ArchiveBuilder(self.root / "no-bridge", "spot")
        no_bridge.add_chunk(1, "depth_snapshot", [_snapshot("spot", start + 100)])
        no_bridge.add_chunk(2, "diff_depth", [_depth("spot", start + 110, 90, 99, None)])
        no_bridge.finish()
        self._expect_failure(no_bridge, "spot", "required bootstrap bridge was not found", "no")

        gap = ArchiveBuilder(self.root / "gap", "spot")
        gap.add_chunk(1, "depth_snapshot", [_snapshot("spot", start + 100)])
        gap.add_chunk(2, "diff_depth", [_depth("spot", start + 110, 101, 101, None)])
        gap.finish()
        self._expect_failure(gap, "spot", "Spot bootstrap forward gap", "gap-output")

    def test_usdm_missing_and_incorrect_pu_fail_closed(self) -> None:
        start = materializer.SOURCE_START_NS
        missing = ArchiveBuilder(self.root / "missing", "um_perpetual")
        missing.add_chunk(1, "depth_snapshot", [_snapshot("um_perpetual", start + 100)])
        missing.add_chunk(
            2, "diff_depth", [_depth("um_perpetual", start + 110, 99, 101, None)]
        )
        missing.finish()
        self._expect_failure(missing, "usdm", "USD-M diff pu is missing", "missing-output")

        incorrect = ArchiveBuilder(self.root / "incorrect", "um_perpetual")
        incorrect.add_chunk(1, "depth_snapshot", [_snapshot("um_perpetual", start + 100)])
        incorrect.add_chunk(
            2,
            "diff_depth",
            [
                _depth("um_perpetual", start + 110, 99, 101, 100),
                _depth("um_perpetual", start + 120, 102, 102, 99),
            ],
        )
        incorrect.finish()
        self._expect_failure(incorrect, "usdm", "USD-M pu continuity", "incorrect-output")

    def test_ordering_inversion_and_truncation_fail_closed(self) -> None:
        start = materializer.SOURCE_START_NS
        inverted = ArchiveBuilder(self.root / "inverted", "spot")
        inverted.add_chunk(1, "depth_snapshot", [_snapshot("spot", start + 100)])
        inverted.add_chunk(
            2,
            "diff_depth",
            [
                _depth("spot", start + 120, 99, 101, None),
                _depth("spot", start + 110, 102, 102, None),
            ],
        )
        inverted.finish()
        self._expect_failure(inverted, "spot", "source ordering inversion", "inverted-output")

        truncated = _valid_builder(self.root / "truncated", "spot")
        chunk_id = str(truncated.entries[1]["chunk_id"])
        artifact = truncated.root / str(truncated.entries[1]["artifact_relative_path"])
        truncated.rewrite_manifest_and_catalog(chunk_id, artifact.read_bytes()[:-7])
        self._expect_failure(truncated, "spot", "truncated Raw frame body", "truncated-output")

    def test_existing_output_and_test_archive_without_opt_in_fail_closed(self) -> None:
        builder = _valid_builder(self.root / "source", "spot")
        (self.root / "existing").mkdir()
        self._expect_failure(builder, "spot", "output directory already exists", "existing")
        with self.assertRaisesRegex(
            materializer.MaterializationError, "deterministic test archive requires explicit opt-in"
        ):
            materializer.materialize(
                source_root=builder.root,
                inventory_path=builder.root / "m5_source_inventory.json",
                output_directory=self.root / "not-used",
                market="spot",
                target_live_updates=3,
                price_scale=8,
                quantity_scale=8,
                conversion_timestamp="2026-08-09T00:00:00Z",
                allow_test_archive=False,
            )

    def test_rejected_fixture_is_never_published(self) -> None:
        rejector = shutil.which("false")
        if rejector is None:
            self.skipTest("false executable unavailable")
        builder = _valid_builder(self.root / "source", "spot")
        output = self.root / "rejected-output"
        with self.assertRaisesRegex(
            materializer.MaterializationError,
            "Phase-1/parser/differential validation failed",
        ):
            materializer.materialize(
                source_root=builder.root,
                inventory_path=builder.root / "m5_source_inventory.json",
                output_directory=output,
                market="spot",
                target_live_updates=3,
                price_scale=8,
                quantity_scale=8,
                conversion_timestamp="2026-08-09T00:00:00Z",
                validators=[Path(rejector)],
                allow_test_archive=True,
            )
        self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
