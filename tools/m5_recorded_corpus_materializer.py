#!/usr/bin/env python3
"""Offline M5 Recorder Raw-v1 to canonical Replay_V1 materializer.

This tool independently implements the minimal accepted Recorder Raw profile used by
M5 Phase 3. It never imports Recorder internals and never writes the source archive.
Production archives must be zstd-compressed Recorder sealed chunks. Uncompressed Raw
chunks are accepted only with the explicit deterministic-test-archive gate.

The baseline selection follows the accepted M3/ADR-0005 bootstrap authority
(docs/M5_PHASE1_CANONICAL_REPLAY.md): for Spot the bootstrap target is the
snapshot ``last_update_id`` (``L``) itself and the advancing bridge must contain
it (``U <= L < u``); for USD-M the first relevant bridge satisfies
``U <= L <= u``. Recorded snapshots that cannot bridge the diff stream are
skipped in receive order, and the first valid baseline is selected. A live
continuity failure after a proven bridge is never retried against another
snapshot.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import sqlite3
import struct
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path, PurePosixPath
from typing import Any, Iterable, Sequence
from urllib.parse import quote

MATERIALIZER_VERSION = "m5-recorder-materializer-v1"
CORPUS_SCHEMA_VERSION = "M5_CORPUS_PROVENANCE_V1"
REPLAY_SCHEMA_VERSION = "REPLAY_V1"
SOURCE_INVENTORY_SCHEMA = "M5_RECORDER_SOURCE_INVENTORY_V1"

RECORDER_REPOSITORY = "tomohikoAmada/BinanceMarketDataRecorder"
RECORDER_COMMIT = "cf1e749c7a533e916dbfb685212e5549a38c70dd"
RECORDER_WHEEL_SHA256 = (
    "926615b09ef46130f49a87fe8ab20acb7cfa6313daa67af5b718931bd95ff329"
)
RECORDER_CONFIG_SHA256 = (
    "a399e647faaac58b5db24e835f1c29e799c70ad0c94ec77b597cac2647cfb734"
)
SOURCE_RUN_IDENTITY = "preflight/m21-4-24h-20260805T150930Z/"
SOURCE_START_UTC = "2026-08-05T15:09:30.200566Z"
SOURCE_END_UTC = "2026-08-06T15:09:30.200566Z"
SOURCE_START_NS = 1_785_942_570_200_566_000
SOURCE_END_NS = 1_786_028_970_200_566_000

RAW_MAGIC = b"BMRCHNK\x1a"
RAW_HEADER = struct.Struct(">8sBBHIII")
RAW_FRAME = struct.Struct(">IHHI")
MAX_HEADER_BYTES = 64 * 1024
MAX_SUPPORTED_FRAME_BYTES = 64 * 1024 * 1024
INCOMPLETE_FLAGS = {
    "checksum_failure",
    "mixed_sequence_type",
    "orderbook_resync",
    "recovered_tail",
    "sequence_gap",
}
TOKEN_CHARS = frozenset(
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789._:/+-"
)


class MaterializationError(RuntimeError):
    """A stable fail-closed source/materialization rejection."""


class BridgeEligibilityError(MaterializationError):
    """One snapshot baseline cannot establish any bootstrap bridge.

    Raised only while the projection is still awaiting its first bridge. The
    caller may skip this baseline and retry the next recorded snapshot. Any
    failure after a bridge has been proven is a plain MaterializationError and
    must never be retried as if the baseline had been invalid.
    """


@dataclass(frozen=True)
class MarketConfig:
    inventory_market: str
    replay_market: str
    policy: str
    fixture_id: str
    snapshot_schema: str


MARKETS = {
    "spot": MarketConfig(
        "spot",
        "Spot",
        "Spot",
        "M5-REC-SPOT-BTCUSDT-V1",
        "binance-spot-depth-snapshot-provenance.v2",
    ),
    "usdm": MarketConfig(
        "um_perpetual",
        "UsdMPerpetual",
        "UsdMPerpetual",
        "M5-REC-USDM-BTCUSDT-V1",
        "binance-usdm-depth-snapshot-provenance.v1",
    ),
}


@dataclass(frozen=True)
class SourceRecord:
    envelope: dict[str, Any]
    chunk_id: str
    stored_sha256: str
    uncompressed_sha256: str
    manifest_sha256: str
    record_ordinal: int
    logical_sha256: str

    def order_key(self) -> tuple[object, ...]:
        envelope = self.envelope
        sequence = json.dumps(
            envelope["source_sequence"], sort_keys=True, separators=(",", ":")
        )
        return (
            envelope["receive_time_utc_ns"],
            envelope["market"],
            envelope["stream"],
            envelope["symbol"],
            envelope["collector_instance_id"],
            envelope["connection_id"],
            envelope["receive_monotonic_ns"],
            sequence,
            self.uncompressed_sha256,
            self.record_ordinal,
            0,
            self.logical_sha256,
        )

    def identity(self) -> dict[str, object]:
        return {
            "chunk_id": self.chunk_id,
            "manifest_sha256": self.manifest_sha256,
            "record_ordinal": self.record_ordinal,
            "receive_time_utc_ns": self.envelope["receive_time_utc_ns"],
            "stored_sha256": self.stored_sha256,
            "uncompressed_sha256": self.uncompressed_sha256,
        }


@dataclass(frozen=True)
class ParsedDepth:
    record: SourceRecord
    first_update_id: int
    final_update_id: int
    previous_final_update_id: int | None
    bids: tuple[tuple[str, str], ...]
    asks: tuple[tuple[str, str], ...]


@dataclass(frozen=True)
class ParsedSnapshot:
    record: SourceRecord
    last_update_id: int
    bids: tuple[tuple[str, str], ...]
    asks: tuple[tuple[str, str], ...]


@dataclass(frozen=True)
class MaterializationResult:
    output_directory: Path
    fixture_id: str
    event_count: int
    replay_log_sha256: str
    final_update_id: int
    provenance: dict[str, object]


def _require(condition: bool, message: str) -> None:
    if not condition:
        raise MaterializationError(message)


def _sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while block := source.read(1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def _safe_relative(root: Path, value: object, field: str) -> Path:
    _require(isinstance(value, str) and value != "", f"{field} must be text")
    relative = PurePosixPath(value)
    _require(
        not relative.is_absolute() and ".." not in relative.parts,
        f"{field} must be a safe relative path",
    )
    resolved_root = root.resolve()
    resolved = (resolved_root / Path(*relative.parts)).resolve()
    _require(resolved == resolved_root or resolved_root in resolved.parents, f"unsafe {field}")
    return resolved


def _read_json(path: Path, label: str) -> tuple[dict[str, Any], bytes]:
    try:
        data = path.read_bytes()
        value = json.loads(data)
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        raise MaterializationError(f"cannot read {label}: {error}") from error
    _require(isinstance(value, dict), f"{label} root must be an object")
    return value, data


def _exact_keys(value: dict[str, Any], keys: set[str], label: str) -> None:
    _require(set(value) == keys, f"{label} fields do not match schema")


def _canonical_uint(value: object, field: str) -> int:
    _require(
        isinstance(value, int)
        and not isinstance(value, bool)
        and 0 <= value <= 0xFFFFFFFFFFFFFFFF,
        f"{field} must be a canonical uint64",
    )
    return value


def _valid_sha256(value: object, field: str) -> str:
    _require(
        isinstance(value, str)
        and len(value) == 64
        and all(character in "0123456789abcdef" for character in value),
        f"{field} must be lowercase SHA-256",
    )
    return value


def _validate_inventory(
    source_root: Path, inventory_path: Path, allow_test_archive: bool
) -> tuple[dict[str, Any], str]:
    inventory, inventory_bytes = _read_json(inventory_path, "source inventory")
    required = {
        "schema_version",
        "recorder_repository",
        "recorder_commit",
        "deployed_wheel_sha256",
        "production_config_sha256",
        "run_identity",
        "source_start_utc",
        "source_end_utc",
        "catalog_relative_path",
        "catalog_sha256",
        "complete_inventory",
        "test_archive",
        "chunks",
    }
    _exact_keys(inventory, required, "source inventory")
    expected = {
        "schema_version": SOURCE_INVENTORY_SCHEMA,
        "recorder_repository": RECORDER_REPOSITORY,
        "recorder_commit": RECORDER_COMMIT,
        "deployed_wheel_sha256": RECORDER_WHEEL_SHA256,
        "production_config_sha256": RECORDER_CONFIG_SHA256,
        "run_identity": SOURCE_RUN_IDENTITY,
        "source_start_utc": SOURCE_START_UTC,
        "source_end_utc": SOURCE_END_UTC,
        "complete_inventory": True,
    }
    for field, expected_value in expected.items():
        _require(inventory[field] == expected_value, f"source inventory {field} mismatch")
    _require(isinstance(inventory["test_archive"], bool), "invalid test_archive flag")
    if inventory["test_archive"]:
        _require(allow_test_archive, "deterministic test archive requires explicit opt-in")
    else:
        _require(not allow_test_archive, "test-archive opt-in cannot relabel production source")
    _valid_sha256(inventory["catalog_sha256"], "catalog_sha256")
    catalog = _safe_relative(source_root, inventory["catalog_relative_path"], "catalog path")
    _require(catalog.is_file(), "source Catalog is missing")
    _require(_sha256_file(catalog) == inventory["catalog_sha256"], "Catalog SHA-256 mismatch")
    chunks = inventory["chunks"]
    _require(isinstance(chunks, list) and chunks, "source inventory chunks are missing")
    previous_order = -1
    seen_ids: set[str] = set()
    for chunk in chunks:
        _require(isinstance(chunk, dict), "chunk inventory entry must be an object")
        _exact_keys(
            chunk,
            {
                "source_order",
                "chunk_id",
                "artifact_relative_path",
                "manifest_relative_path",
                "manifest_sha256",
                "storage_encoding",
            },
            "chunk inventory entry",
        )
        source_order = _canonical_uint(chunk["source_order"], "invalid source_order")
        _require(source_order == previous_order + 1, "chunk source order is not contiguous")
        previous_order = source_order
        chunk_id = chunk["chunk_id"]
        _require(isinstance(chunk_id, str) and chunk_id and chunk_id not in seen_ids, "chunk ID")
        seen_ids.add(chunk_id)
        _safe_relative(source_root, chunk["artifact_relative_path"], "artifact path")
        _safe_relative(source_root, chunk["manifest_relative_path"], "manifest path")
        _valid_sha256(chunk["manifest_sha256"], "manifest_sha256")
        expected_encoding = "raw-test" if inventory["test_archive"] else "zstd-frame.v1"
        _require(chunk["storage_encoding"] == expected_encoding, "chunk storage encoding mismatch")
    return inventory, _sha256_bytes(inventory_bytes)


def _verify_catalog(source_root: Path, inventory: dict[str, Any]) -> None:
    catalog_path = _safe_relative(
        source_root, inventory["catalog_relative_path"], "catalog path"
    )
    uri = f"file:{quote(str(catalog_path))}?mode=ro&immutable=1"
    try:
        connection = sqlite3.connect(uri, uri=True)
        connection.row_factory = sqlite3.Row
        catalog_chunk_ids = {
            row["chunk_id"] for row in connection.execute("SELECT chunk_id FROM chunks")
        }
        inventory_chunk_ids = {chunk["chunk_id"] for chunk in inventory["chunks"]}
        _require(
            catalog_chunk_ids == inventory_chunk_ids,
            "Catalog/inventory chunk set mismatch",
        )
        for chunk in inventory["chunks"]:
            row = connection.execute(
                "SELECT chunk_id,state,record_count,uncompressed_bytes,stored_bytes,"
                "uncompressed_sha256,stored_sha256 FROM chunks WHERE chunk_id=?",
                (chunk["chunk_id"],),
            ).fetchone()
            _require(row is not None, f"Catalog is missing chunk {chunk['chunk_id']}")
            _require(row["state"] != "QUARANTINED", "Catalog identifies quarantined chunk")
            manifest_path = _safe_relative(
                source_root, chunk["manifest_relative_path"], "manifest path"
            )
            manifest, _ = _read_json(manifest_path, "raw manifest")
            for field in (
                "record_count",
                "uncompressed_bytes",
                "stored_bytes",
                "uncompressed_sha256",
                "stored_sha256",
            ):
                _require(row[field] == manifest.get(field), f"Catalog {field} mismatch")
    except sqlite3.Error as error:
        raise MaterializationError(f"cannot validate source Catalog: {error}") from error
    finally:
        if "connection" in locals():
            connection.close()


def crc32c(data: bytes) -> int:
    """Return Castagnoli CRC32C; exposed for the repository known-vector test."""

    crc = 0xFFFFFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            mask = -(crc & 1) & 0x82F63B78
            crc = (crc >> 1) ^ mask
    return crc ^ 0xFFFFFFFF


class _CborDecoder:
    def __init__(self, data: bytes) -> None:
        self.data = data
        self.offset = 0

    def _take(self, count: int) -> bytes:
        end = self.offset + count
        _require(end <= len(self.data), "truncated CBOR")
        block = self.data[self.offset : end]
        self.offset = end
        return block

    def _argument(self, additional: int) -> int:
        if additional < 24:
            return additional
        sizes = {24: 1, 25: 2, 26: 4, 27: 8}
        _require(additional in sizes, "unsupported indefinite/reserved CBOR argument")
        return int.from_bytes(self._take(sizes[additional]), "big")

    def decode(self) -> object:
        initial = self._take(1)[0]
        major = initial >> 5
        additional = initial & 0x1F
        if major in {0, 1}:
            value = self._argument(additional)
            return value if major == 0 else -1 - value
        if major in {2, 3}:
            length = self._argument(additional)
            data = self._take(length)
            if major == 2:
                return data
            try:
                return data.decode("utf-8")
            except UnicodeDecodeError as error:
                raise MaterializationError("invalid CBOR UTF-8") from error
        if major == 4:
            return [self.decode() for _ in range(self._argument(additional))]
        if major == 5:
            result: dict[object, object] = {}
            for _ in range(self._argument(additional)):
                key = self.decode()
                _require(key not in result, "duplicate CBOR map key")
                result[key] = self.decode()
            return result
        if major == 7 and additional in {20, 21, 22}:
            return {20: False, 21: True, 22: None}[additional]
        raise MaterializationError("unsupported CBOR type")


def _cbor_head(major: int, value: int) -> bytes:
    _require(value >= 0, "negative CBOR argument")
    if value < 24:
        return bytes([(major << 5) | value])
    if value <= 0xFF:
        return bytes([(major << 5) | 24, value])
    if value <= 0xFFFF:
        return bytes([(major << 5) | 25]) + value.to_bytes(2, "big")
    if value <= 0xFFFFFFFF:
        return bytes([(major << 5) | 26]) + value.to_bytes(4, "big")
    _require(value <= 0xFFFFFFFFFFFFFFFF, "CBOR integer overflow")
    return bytes([(major << 5) | 27]) + value.to_bytes(8, "big")


def canonical_cbor(value: object) -> bytes:
    """Encode the Recorder-supported deterministic CBOR subset."""

    if value is None:
        return b"\xf6"
    if value is False:
        return b"\xf4"
    if value is True:
        return b"\xf5"
    if isinstance(value, int):
        return _cbor_head(0, value) if value >= 0 else _cbor_head(1, -1 - value)
    if isinstance(value, bytes):
        return _cbor_head(2, len(value)) + value
    if isinstance(value, str):
        encoded = value.encode("utf-8")
        return _cbor_head(3, len(encoded)) + encoded
    if isinstance(value, (list, tuple)):
        return _cbor_head(4, len(value)) + b"".join(canonical_cbor(item) for item in value)
    if isinstance(value, dict):
        encoded_items = [(canonical_cbor(key), canonical_cbor(item)) for key, item in value.items()]
        encoded_items.sort(key=lambda item: (len(item[0]), item[0]))
        return _cbor_head(5, len(encoded_items)) + b"".join(
            key + item for key, item in encoded_items
        )
    raise MaterializationError("unsupported canonical CBOR value")


def _decode_canonical_cbor(data: bytes, label: str) -> object:
    decoder = _CborDecoder(data)
    value = decoder.decode()
    _require(decoder.offset == len(data), f"{label} CBOR has trailing bytes")
    _require(canonical_cbor(value) == data, f"{label} CBOR is not canonical")
    return value


def _decompress_zstd(path: Path) -> bytes:
    executable = shutil.which("zstd")
    _require(executable is not None, "zstd decoder is unavailable for sealed Recorder source")
    completed = subprocess.run(
        [executable, "--decompress", "--stdout", "--quiet", str(path)],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    _require(completed.returncode == 0, "zstd source artifact is corrupt or unsupported")
    return completed.stdout


def _validate_envelope(value: object, header: dict[str, object]) -> dict[str, Any]:
    _require(isinstance(value, dict), "envelope root must be a map")
    required = {
        "schema_version",
        "venue",
        "market",
        "symbol",
        "stream",
        "module",
        "connection_id",
        "collector_instance_id",
        "collector_version",
        "receive_time_utc_ns",
        "receive_monotonic_ns",
        "exchange_event_time",
        "exchange_transaction_time",
        "exchange_trade_time",
        "source_sequence",
        "payload_encoding",
        "raw_payload",
        "capture_flags",
    }
    _exact_keys(value, required, "EventEnvelope")
    _require(value["schema_version"] == "event-envelope.v1", "envelope schema mismatch")
    _require(value["venue"] == "binance", "envelope venue mismatch")
    for field in (
        "market",
        "symbol",
        "stream",
        "module",
        "connection_id",
        "collector_instance_id",
        "collector_version",
        "payload_encoding",
    ):
        _require(isinstance(value[field], str) and value[field], f"invalid envelope {field}")
    for field in ("receive_time_utc_ns", "receive_monotonic_ns"):
        _canonical_uint(value[field], f"invalid envelope {field}")
    for field in (
        "exchange_event_time",
        "exchange_transaction_time",
        "exchange_trade_time",
    ):
        _require(value[field] is None or _canonical_uint(value[field], field) >= 0, field)
    _require(isinstance(value["source_sequence"], dict), "invalid source_sequence")
    for key, item in value["source_sequence"].items():
        _require(isinstance(key, str) and key, "invalid source_sequence key")
        _require(
            (isinstance(item, int) and not isinstance(item, bool)) or isinstance(item, str),
            "invalid source_sequence value",
        )
    _require(isinstance(value["raw_payload"], bytes), "raw_payload must be bytes")
    _require(
        isinstance(value["capture_flags"], list)
        and all(isinstance(item, str) and item for item in value["capture_flags"]),
        "invalid capture_flags",
    )
    for field in ("market", "symbol", "stream"):
        _require(value[field] == header[field], f"envelope/header {field} mismatch")
    return value


def _scan_raw_chunk(raw: bytes) -> tuple[dict[str, object], list[tuple[dict[str, Any], str]]]:
    _require(len(raw) >= RAW_HEADER.size, "truncated Raw header")
    fixed = raw[: RAW_HEADER.size]
    magic, major, minor, marker, flags, body_length, expected_crc = RAW_HEADER.unpack(fixed)
    _require(magic == RAW_MAGIC, "Raw magic mismatch")
    _require((major, minor) == (1, 0), "Raw version mismatch")
    _require(marker == 0xFEFF and flags == 0, "Raw header flags/byte order mismatch")
    _require(body_length <= MAX_HEADER_BYTES, "Raw header is oversized")
    header_end = RAW_HEADER.size + body_length
    _require(header_end <= len(raw), "truncated Raw CBOR header")
    header_body = raw[RAW_HEADER.size:header_end]
    _require(crc32c(fixed[:-4] + header_body) == expected_crc, "Raw header CRC32C mismatch")
    header_value = _decode_canonical_cbor(header_body, "Raw header")
    _require(isinstance(header_value, dict), "Raw header root must be a map")
    header_keys = {
        "chunk_id",
        "chunk_schema_version",
        "collector_instance_id",
        "collector_version",
        "created_at_utc_ns",
        "envelope_schema_version",
        "format",
        "market",
        "max_frame_bytes",
        "stream",
        "symbol",
    }
    _exact_keys(header_value, header_keys, "Raw header")
    _require(header_value["format"] == "bmdr-raw-chunk", "Raw format mismatch")
    _require(header_value["chunk_schema_version"] == "raw-chunk.v1", "chunk schema mismatch")
    _require(
        header_value["envelope_schema_version"] == "event-envelope.v1",
        "envelope schema mismatch",
    )
    _require(
        isinstance(header_value["chunk_id"], bytes) and len(header_value["chunk_id"]) == 16,
        "Raw chunk_id is invalid",
    )
    max_frame = _canonical_uint(header_value["max_frame_bytes"], "invalid max_frame_bytes")
    _require(1024 <= max_frame <= MAX_SUPPORTED_FRAME_BYTES, "unsupported max_frame_bytes")
    for field in ("collector_instance_id", "collector_version", "market", "stream", "symbol"):
        _require(isinstance(header_value[field], str) and header_value[field], f"header {field}")
    _canonical_uint(header_value["created_at_utc_ns"], "invalid created_at_utc_ns")

    records: list[tuple[dict[str, Any], str]] = []
    offset = header_end
    while offset < len(raw):
        _require(offset + RAW_FRAME.size <= len(raw), "truncated Raw frame prefix")
        prefix = raw[offset : offset + RAW_FRAME.size]
        body_length, frame_flags, reserved, frame_crc = RAW_FRAME.unpack(prefix)
        _require(body_length <= max_frame, "Raw frame exceeds declared maximum")
        _require(frame_flags == 0 and reserved == 0, "unsupported Raw frame flags")
        body_start = offset + RAW_FRAME.size
        body_end = body_start + body_length
        _require(body_end <= len(raw), "truncated Raw frame body")
        body = raw[body_start:body_end]
        _require(crc32c(prefix[:8] + body) == frame_crc, "Raw frame CRC32C mismatch")
        envelope = _validate_envelope(_decode_canonical_cbor(body, "envelope"), header_value)
        records.append((envelope, _sha256_bytes(body)))
        offset = body_end
    return header_value, records


def _chunk_uuid_text(chunk_id: bytes) -> str:
    hexadecimal = chunk_id.hex()
    return (
        f"{hexadecimal[:8]}-{hexadecimal[8:12]}-{hexadecimal[12:16]}-"
        f"{hexadecimal[16:20]}-{hexadecimal[20:]}"
    )


def _load_chunk(
    source_root: Path, entry: dict[str, Any], config: MarketConfig, test_archive: bool
) -> tuple[dict[str, Any], list[SourceRecord]]:
    manifest_path = _safe_relative(source_root, entry["manifest_relative_path"], "manifest path")
    manifest, manifest_bytes = _read_json(manifest_path, "raw manifest")
    _require(
        _sha256_bytes(manifest_bytes) == entry["manifest_sha256"],
        "raw manifest SHA-256 mismatch",
    )
    manifest_sha = entry["manifest_sha256"]
    required_manifest_fields = {
        "manifest_schema_version",
        "chunk_id",
        "chunk_schema_version",
        "envelope_schema_version",
        "market",
        "symbol",
        "stream",
        "record_count",
        "uncompressed_bytes",
        "stored_bytes",
        "uncompressed_sha256",
        "stored_sha256",
        "complete",
        "gap",
        "resync",
        "recovered",
        "capture_flags",
    }
    _require(required_manifest_fields <= set(manifest), "raw manifest required metadata missing")
    _require(manifest["manifest_schema_version"] == "raw-chunk-manifest.v1", "manifest schema")
    _require(manifest["chunk_schema_version"] == "raw-chunk.v1", "manifest chunk schema")
    _require(manifest["envelope_schema_version"] == "event-envelope.v1", "manifest envelope")
    _require(manifest["chunk_id"] == entry["chunk_id"], "inventory/manifest chunk ID mismatch")
    _require(manifest["market"] == config.inventory_market, "raw manifest market mismatch")
    _require(manifest["symbol"] == "BTCUSDT", "raw manifest symbol mismatch")
    _require(manifest["stream"] in {"depth_snapshot", "diff_depth"}, "unexpected source stream")
    _require(manifest["complete"] is True, "incomplete Raw chunk is ineligible")
    _require(
        manifest["gap"] is False and manifest["resync"] is False and manifest["recovered"] is False,
        "gap/resync/recovered Raw chunk is ineligible",
    )
    _require(
        isinstance(manifest["capture_flags"], list)
        and not (set(manifest["capture_flags"]) & INCOMPLETE_FLAGS),
        "ineligible Raw capture flags",
    )
    stored_sha = _valid_sha256(manifest["stored_sha256"], "stored_sha256")
    uncompressed_sha = _valid_sha256(manifest["uncompressed_sha256"], "uncompressed_sha256")
    artifact = _safe_relative(source_root, entry["artifact_relative_path"], "artifact path")
    _require(artifact.is_file(), "source chunk artifact is missing")
    stored = artifact.read_bytes()
    _require(len(stored) == manifest["stored_bytes"], "stored chunk size mismatch")
    _require(_sha256_bytes(stored) == stored_sha, "stored chunk SHA-256 mismatch")
    raw = stored if test_archive else _decompress_zstd(artifact)
    _require(len(raw) == manifest["uncompressed_bytes"], "uncompressed chunk size mismatch")
    _require(_sha256_bytes(raw) == uncompressed_sha, "uncompressed chunk SHA-256 mismatch")
    header, raw_records = _scan_raw_chunk(raw)
    _require(_chunk_uuid_text(header["chunk_id"]) == entry["chunk_id"], "Raw chunk ID mismatch")
    for field in ("market", "symbol", "stream"):
        _require(header[field] == manifest[field], f"Raw/manifest {field} mismatch")
    _require(len(raw_records) == manifest["record_count"], "Raw record count mismatch")
    records = [
        SourceRecord(
            envelope,
            entry["chunk_id"],
            stored_sha,
            uncompressed_sha,
            manifest_sha,
            ordinal,
            logical_sha,
        )
        for ordinal, (envelope, logical_sha) in enumerate(raw_records)
    ]
    previous: tuple[int, int] | None = None
    for record in records:
        ordering = (
            record.envelope["receive_time_utc_ns"],
            record.envelope["receive_monotonic_ns"],
        )
        _require(previous is None or ordering >= previous, "source ordering inversion within chunk")
        previous = ordering
    return manifest, records


def _levels(value: object, field: str) -> tuple[tuple[str, str], ...]:
    _require(isinstance(value, list), f"{field} must be an array")
    output: list[tuple[str, str]] = []
    for level in value:
        _require(
            isinstance(level, list)
            and len(level) == 2
            and all(isinstance(item, str) for item in level),
            f"{field} contains malformed level",
        )
        output.append((level[0], level[1]))
    return tuple(output)


def _payload(record: SourceRecord) -> dict[str, Any]:
    try:
        value = json.loads(record.envelope["raw_payload"])
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise MaterializationError("required source payload is malformed JSON") from error
    _require(isinstance(value, dict), "required source payload root must be object")
    return value


def _parse_snapshot(record: SourceRecord, config: MarketConfig) -> ParsedSnapshot:
    envelope = record.envelope
    _require(envelope["stream"] == "depth_snapshot", "snapshot stream mismatch")
    _require(envelope["payload_encoding"] == "utf-8-json-provenance", "snapshot encoding")
    payload = _payload(record)
    _require(payload.get("schema_version") == config.snapshot_schema, "snapshot provenance schema")
    response = payload.get("response")
    _require(isinstance(response, dict), "snapshot response provenance missing")
    model = response.get("model")
    _require(isinstance(model, dict), "snapshot model missing")
    last_update_id = _canonical_uint(model.get("lastUpdateId"), "invalid snapshot lastUpdateId")
    _require(
        envelope["source_sequence"] == {"lastUpdateId": last_update_id},
        "snapshot sequence provenance mismatch",
    )
    return ParsedSnapshot(
        record,
        last_update_id,
        _levels(model.get("bids"), "snapshot bids"),
        _levels(model.get("asks"), "snapshot asks"),
    )


def _parse_depth(record: SourceRecord, config: MarketConfig) -> ParsedDepth:
    envelope = record.envelope
    _require(envelope["stream"] == "diff_depth", "diff stream mismatch")
    _require(envelope["payload_encoding"] == "utf-8-json", "diff payload encoding mismatch")
    _require(not (set(envelope["capture_flags"]) & INCOMPLETE_FLAGS), "diff record is incomplete")
    payload = _payload(record)
    _require(payload.get("e") == "depthUpdate", "diff event type mismatch")
    _require(payload.get("s") == "BTCUSDT", "diff payload symbol mismatch")
    first = _canonical_uint(payload.get("U"), "invalid diff U")
    final = _canonical_uint(payload.get("u"), "invalid diff u")
    _require(first <= final, "diff U exceeds u")
    previous: int | None = None
    expected_sequence: dict[str, int] = {"U": first, "u": final}
    if config.inventory_market == "um_perpetual":
        previous = _canonical_uint(payload.get("pu"), "USD-M diff pu is missing or malformed")
        expected_sequence["pu"] = previous
    else:
        _require("pu" not in payload, "Spot diff unexpectedly contains pu")
    _require(envelope["source_sequence"] == expected_sequence, "diff sequence provenance mismatch")
    return ParsedDepth(
        record,
        first,
        final,
        previous,
        _levels(payload.get("b"), "diff bids"),
        _levels(payload.get("a"), "diff asks"),
    )


def _format_levels(side: str, levels: Iterable[tuple[str, str]]) -> str:
    values = [f"{side}:{price},{quantity}" for price, quantity in levels]
    return "|".join(values) if values else "-"


def _baseline_line(snapshot: ParsedSnapshot) -> str:
    return (
        f"INSTALL_BASELINE {snapshot.last_update_id} "
        f"{_format_levels('B', snapshot.bids)} {_format_levels('A', snapshot.asks)}"
    )


def _depth_line(depth: ParsedDepth) -> str:
    pu = "-" if depth.previous_final_update_id is None else str(depth.previous_final_update_id)
    combined = (*(("B", level) for level in depth.bids), *(("A", level) for level in depth.asks))
    levels = "|".join(f"{side}:{price},{quantity}" for side, (price, quantity) in combined)
    return f"DEPTH_UPDATE {depth.first_update_id} {depth.final_update_id} pu={pu} {levels or '-'}"


def _synchronize_from_snapshot(
    snapshot: ParsedSnapshot,
    depths: Sequence[ParsedDepth],
    config: MarketConfig,
    target_live_updates: int,
) -> list[ParsedDepth]:
    """Attempt one snapshot baseline under the accepted M3 bootstrap authority.

    Spot bootstrap target is the snapshot ``last_update_id`` (``L``): events with
    ``u < L`` are stale and discarded, ``u == L`` is a non-advancing duplicate,
    and the first advancing bridge must contain ``L`` (``U <= L < u``). An
    advancing candidate with ``U > L``, including the exact-next range beginning
    at ``L + 1``, is a Spot bootstrap forward gap. ``L == UINT64_MAX`` can never
    form an advancing Spot bridge (no successor arithmetic is performed).

    USD-M bootstrap target is ``L`` with ``U <= L <= u``; the equality bridge
    ``u == L`` is valid and carries ``pu``.

    A baseline that cannot establish any bridge raises BridgeEligibilityError so
    the caller can retry the next recorded snapshot. Every failure after a bridge
    has been proven (live discontinuity, pu mismatch, source exhaustion) is a
    plain MaterializationError and propagates.
    """

    local = snapshot.last_update_id
    selected: list[ParsedDepth] = []
    bridge_found = False
    live_count = 0
    for depth in depths:
        if not bridge_found:
            if config.inventory_market == "spot":
                if depth.final_update_id < local:
                    continue
                if depth.final_update_id == local:
                    continue
                if depth.first_update_id > local:
                    raise BridgeEligibilityError(
                        "Spot bootstrap forward gap before valid bridge"
                    )
                _require(
                    depth.first_update_id <= local < depth.final_update_id,
                    "Spot bootstrap bridge cannot be proven",
                )
            else:
                if depth.final_update_id < local:
                    continue
                if depth.first_update_id > local:
                    raise BridgeEligibilityError(
                        "USD-M bootstrap forward gap before valid bridge"
                    )
                _require(
                    depth.first_update_id <= local <= depth.final_update_id,
                    "USD-M bootstrap bridge cannot be proven",
                )
            selected.append(depth)
            local = depth.final_update_id
            bridge_found = True
            continue

        if depth.final_update_id < local:
            continue
        if depth.final_update_id == local:
            continue
        if config.inventory_market == "spot":
            _require(local != 0xFFFFFFFFFFFFFFFF, "Spot live successor would overflow")
            successor = local + 1
            _require(
                depth.first_update_id <= successor <= depth.final_update_id,
                "Spot live sequence discontinuity",
            )
        else:
            _require(
                depth.previous_final_update_id == local,
                "USD-M pu continuity cannot be established",
            )
        selected.append(depth)
        local = depth.final_update_id
        live_count += 1
        if live_count == target_live_updates:
            break
    if not bridge_found:
        raise BridgeEligibilityError("required bootstrap bridge was not found")
    _require(
        live_count == target_live_updates,
        f"source ended before {target_live_updates} live updates after synchronization",
    )
    return selected


def _select_operations(
    records: Sequence[SourceRecord], config: MarketConfig, target_live_updates: int
) -> tuple[ParsedSnapshot, list[ParsedDepth]]:
    snapshots = sorted(
        (
            _parse_snapshot(record, config)
            for record in records
            if record.envelope["stream"] == "depth_snapshot"
            and record.envelope["receive_time_utc_ns"] >= SOURCE_START_NS
        ),
        key=lambda item: item.record.order_key(),
    )
    _require(snapshots, "no valid REST baseline exists at or after T0")
    depths = sorted(
        (
            _parse_depth(record, config)
            for record in records
            if record.envelope["stream"] == "diff_depth"
            and SOURCE_START_NS <= record.envelope["receive_time_utc_ns"] < SOURCE_END_NS
        ),
        key=lambda item: item.record.order_key(),
    )
    _require(depths, "required ordered diff-depth stream is missing")

    first_bridge_error: BridgeEligibilityError | None = None
    for snapshot in snapshots:
        if snapshot.record.envelope["receive_time_utc_ns"] >= SOURCE_END_NS:
            if first_bridge_error is None:
                first_bridge_error = BridgeEligibilityError(
                    "baseline is outside formal source interval"
                )
            continue
        try:
            selected = _synchronize_from_snapshot(
                snapshot, depths, config, target_live_updates
            )
        except BridgeEligibilityError as error:
            if first_bridge_error is None:
                first_bridge_error = error
            continue
        return snapshot, selected
    raise first_bridge_error or BridgeEligibilityError(
        "required bootstrap bridge was not found"
    )


def _token(value: str, field: str) -> str:
    _require(value != "" and value != "-" and set(value) <= TOKEN_CHARS, f"invalid {field} token")
    return value


def _provenance_fields(inventory_sha256: str) -> list[tuple[str, str]]:
    fields = [
        ("provenance_corpus_schema", CORPUS_SCHEMA_VERSION),
        ("provenance_inventory_sha256", inventory_sha256),
        ("provenance_materializer", MATERIALIZER_VERSION),
        ("provenance_recorder_commit", RECORDER_COMMIT),
        ("provenance_run", SOURCE_RUN_IDENTITY),
        ("provenance_source", "RecorderRawV1"),
    ]
    for key, value in fields:
        _token(key, "provenance key")
        _token(value, "provenance value")
    return sorted(fields)


def _render_replay(
    config: MarketConfig,
    snapshot: ParsedSnapshot,
    depths: Sequence[ParsedDepth],
    price_scale: int,
    quantity_scale: int,
    inventory_sha256: str,
) -> bytes:
    provenance = _provenance_fields(inventory_sha256)
    header = (
        f"REPLAY_V1 market={config.replay_market} symbol=BTCUSDT price_scale={price_scale} "
        f"quantity_scale={quantity_scale} policy={config.policy} fixture_id={config.fixture_id}"
    )
    header += "".join(f" {key}={value}" for key, value in provenance)
    lines = [header, _baseline_line(snapshot), *(_depth_line(depth) for depth in depths)]
    return ("\n".join(lines) + "\n").encode("utf-8")


def _render_manifest(
    config: MarketConfig,
    replay_sha256: str,
    event_count: int,
    price_scale: int,
    quantity_scale: int,
    inventory_sha256: str,
) -> bytes:
    lines = [
        "MANIFEST_V1",
        f"fixture_id={config.fixture_id}",
        "schema_version=REPLAY_V1",
        f"log_sha256={replay_sha256}",
        f"market={config.replay_market}",
        "symbol=BTCUSDT",
        f"price_scale={price_scale}",
        f"quantity_scale={quantity_scale}",
        f"policy={config.policy}",
        f"event_count={event_count}",
    ]
    lines.extend(f"{key}={value}" for key, value in _provenance_fields(inventory_sha256))
    return ("\n".join(lines) + "\n").encode("utf-8")


def _render_corpus_provenance(
    inventory: dict[str, Any],
    inventory_sha256: str,
    config: MarketConfig,
    snapshot: ParsedSnapshot,
    depths: Sequence[ParsedDepth],
    replay_sha256: str,
    event_count: int,
    target_live_updates: int,
    price_scale: int,
    quantity_scale: int,
    conversion_timestamp: str,
    chunk_manifests: Sequence[dict[str, Any]],
) -> tuple[dict[str, object], bytes]:
    used_chunk_ids = {snapshot.record.chunk_id, *(depth.record.chunk_id for depth in depths)}
    manifest_by_id = {manifest["chunk_id"]: manifest for manifest in chunk_manifests}
    chunks = []
    for entry in inventory["chunks"]:
        if entry["chunk_id"] not in used_chunk_ids:
            continue
        manifest = manifest_by_id[entry["chunk_id"]]
        chunks.append(
            {
                "chunk_id": entry["chunk_id"],
                "manifest_sha256": entry["manifest_sha256"],
                "stored_sha256": manifest["stored_sha256"],
                "uncompressed_sha256": manifest["uncompressed_sha256"],
            }
        )
    document: dict[str, object] = {
        "baseline_source": {
            **snapshot.record.identity(),
            "last_update_id": snapshot.last_update_id,
        },
        "bootstrap_bridge": {
            **depths[0].record.identity(),
            "first_update_id": depths[0].first_update_id,
            "final_update_id": depths[0].final_update_id,
        },
        "canonical_replay_log_sha256": replay_sha256,
        "conversion_timestamp": conversion_timestamp,
        "conversion_timestamp_semantic": False,
        "corpus_schema_version": CORPUS_SCHEMA_VERSION,
        "deployed_wheel_sha256": RECORDER_WHEEL_SHA256,
        "event_count": event_count,
        "final_selected_update_id": depths[-1].final_update_id,
        "first_retained_diff": depths[0].record.identity(),
        "fixture_id": config.fixture_id,
        "last_retained_diff": depths[-1].record.identity(),
        "materializer_version": MATERIALIZER_VERSION,
        "market": config.replay_market,
        "numeric_spec": {"price_scale": price_scale, "quantity_scale": quantity_scale},
        "production_config_sha256": RECORDER_CONFIG_SHA256,
        "projection_replay_schema_version": REPLAY_SCHEMA_VERSION,
        "recorder_production_commit": RECORDER_COMMIT,
        "recorder_repository": RECORDER_REPOSITORY,
        "selected_live_updates_after_synchronization": target_live_updates,
        "sequence_policy": config.policy,
        "source_actual_end_utc_ns": depths[-1].record.envelope["receive_time_utc_ns"],
        "source_actual_start_utc_ns": snapshot.record.envelope["receive_time_utc_ns"],
        "source_inventory_sha256": inventory_sha256,
        "source_raw_chunks": chunks,
        "source_run_identity": SOURCE_RUN_IDENTITY,
        "source_stream_types": ["depth_snapshot", "diff_depth_100ms"],
        "source_utc_end": SOURCE_END_UTC,
        "source_utc_start": SOURCE_START_UTC,
        "symbol": "BTCUSDT",
    }
    body = (json.dumps(document, sort_keys=True, separators=(",", ":")) + "\n").encode()
    return document, body


def _write_validated_directory(
    output: Path, files: dict[str, bytes], validators: Sequence[Path]
) -> None:
    _require(not output.exists(), "output directory already exists")
    output.parent.mkdir(parents=True, exist_ok=True)
    staging = Path(tempfile.mkdtemp(prefix=f".{output.name}.partial-", dir=output.parent))
    try:
        for name, body in files.items():
            (staging / name).write_bytes(body)
        _run_validators(validators, staging)
        os.replace(staging, output)
    except Exception:
        shutil.rmtree(staging, ignore_errors=True)
        raise


def _run_validators(validators: Sequence[Path], output: Path) -> None:
    for validator in validators:
        _require(validator.is_file() and os.access(validator, os.X_OK), "fixture validator missing")
        completed = subprocess.run(
            [str(validator), str(output)],
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        if completed.returncode != 0:
            raise MaterializationError(
                "Phase-1/parser/differential validation failed: " + completed.stderr.strip()
            )


def materialize(
    *,
    source_root: Path,
    inventory_path: Path,
    output_directory: Path,
    market: str,
    target_live_updates: int,
    price_scale: int,
    quantity_scale: int,
    conversion_timestamp: str,
    validators: Sequence[Path] = (),
    allow_test_archive: bool = False,
) -> MaterializationResult:
    """Validate immutable source, materialize Replay_V1, then run supplied validators."""

    _require(market in MARKETS, "unsupported market")
    _require(target_live_updates > 0, "target live update count must be positive")
    _require(0 <= price_scale <= 18 and 0 <= quantity_scale <= 18, "NumericSpec scale")
    _token(conversion_timestamp, "conversion timestamp")
    source_root = source_root.resolve()
    _require(source_root.is_dir(), "source root does not exist")
    inventory, inventory_sha256 = _validate_inventory(
        source_root, inventory_path.resolve(), allow_test_archive
    )
    _verify_catalog(source_root, inventory)
    config = MARKETS[market]
    chunk_manifests: list[dict[str, Any]] = []
    records: list[SourceRecord] = []
    for entry in inventory["chunks"]:
        manifest_path = _safe_relative(source_root, entry["manifest_relative_path"], "manifest path")
        preview, _ = _read_json(manifest_path, "raw manifest")
        if preview.get("market") != config.inventory_market:
            continue
        manifest, chunk_records = _load_chunk(
            source_root, entry, config, bool(inventory["test_archive"])
        )
        chunk_manifests.append(manifest)
        records.extend(chunk_records)
    _require(chunk_manifests, "inventory contains no chunks for requested market")
    _require(
        {manifest["stream"] for manifest in chunk_manifests}
        == {"depth_snapshot", "diff_depth"},
        "required source streams are incomplete",
    )
    snapshot, depths = _select_operations(records, config, target_live_updates)
    replay_bytes = _render_replay(
        config, snapshot, depths, price_scale, quantity_scale, inventory_sha256
    )
    replay_sha = _sha256_bytes(replay_bytes)
    event_count = 1 + len(depths)
    manifest_bytes = _render_manifest(
        config,
        replay_sha,
        event_count,
        price_scale,
        quantity_scale,
        inventory_sha256,
    )
    provenance, provenance_bytes = _render_corpus_provenance(
        inventory,
        inventory_sha256,
        config,
        snapshot,
        depths,
        replay_sha,
        event_count,
        target_live_updates,
        price_scale,
        quantity_scale,
        conversion_timestamp,
        chunk_manifests,
    )
    _write_validated_directory(
        output_directory,
        {
            "replay.log": replay_bytes,
            "manifest.txt": manifest_bytes,
            "corpus_provenance.json": provenance_bytes,
        },
        validators,
    )
    return MaterializationResult(
        output_directory,
        config.fixture_id,
        event_count,
        replay_sha,
        depths[-1].final_update_id,
        provenance,
    )


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--source-inventory", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--market", choices=sorted(MARKETS), required=True)
    parser.add_argument("--target-live-updates", type=int, default=100_000)
    parser.add_argument("--price-scale", type=int, default=8)
    parser.add_argument("--quantity-scale", type=int, default=8)
    parser.add_argument("--conversion-timestamp", required=True)
    parser.add_argument("--validator", action="append", type=Path, default=[])
    parser.add_argument("--allow-test-archive", action="store_true", help=argparse.SUPPRESS)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    arguments = _parser().parse_args(argv)
    try:
        result = materialize(
            source_root=arguments.source_root,
            inventory_path=arguments.source_inventory,
            output_directory=arguments.output,
            market=arguments.market,
            target_live_updates=arguments.target_live_updates,
            price_scale=arguments.price_scale,
            quantity_scale=arguments.quantity_scale,
            conversion_timestamp=arguments.conversion_timestamp,
            validators=arguments.validator,
            allow_test_archive=arguments.allow_test_archive,
        )
    except MaterializationError as error:
        print(f"materialization=FAIL reason={error}", file=sys.stderr)
        return 1
    print("M5_RECORDED_CORPUS_MATERIALIZATION_V1")
    print(f"fixture_id={result.fixture_id}")
    print(f"event_count={result.event_count}")
    print(f"replay_log_sha256={result.replay_log_sha256}")
    print(f"materializer_version={MATERIALIZER_VERSION}")
    print("materialization=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
