#!/usr/bin/env python3
"""Fail-closed validation for M5 semantic-manifest evidence."""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any


SCHEMA_VERSION = "M5_SEMANTIC_MANIFEST_V1"
AUTHORITATIVE_WORKLOAD_IDS = (
    "m5-small-core-spot-v1",
    "m5-small-core-usdm-v1",
    "m5-small-adapter-spot-v1",
    "m5-small-adapter-usdm-v1",
)
CROSS_COMPILER_ROLES = {
    ("GNU", "Linux"): "GNU/Linux",
    ("Clang", "Linux"): "Clang/Linux",
    ("AppleClang", "Darwin"): "AppleClang/Darwin",
}
ROLE_ORDER = ("GNU/Linux", "Clang/Linux", "AppleClang/Darwin")
TRANSPORT_ROLE_HINTS = {
    "ubuntu-gcc": "GNU/Linux",
    "ubuntu-clang": "Clang/Linux",
    "macos-appleclang": "AppleClang/Darwin",
}


class ManifestValidationError(Exception):
    """A deterministic semantic-evidence validation failure."""


@dataclass(frozen=True)
class Toolchain:
    compiler: str
    compiler_version: str
    os: str
    architecture: str


@dataclass(frozen=True)
class Workload:
    workload_id: str
    fixture_id: str
    fixture_hash: str
    semantic_digest: str


@dataclass(frozen=True)
class Manifest:
    label: str
    path: Path
    schema_version: str
    head_sha: str
    toolchain: Toolchain
    build_type: str
    fixture_set_id: str
    workloads: tuple[Workload, ...]


def fail(message: str) -> None:
    raise ManifestValidationError(message)


def is_valid_sha256_hex(value: object) -> bool:
    return (
        isinstance(value, str)
        and len(value) == 64
        and all(character in "0123456789abcdef" for character in value)
    )


def is_valid_git_sha(value: object) -> bool:
    return (
        isinstance(value, str)
        and len(value) == 40
        and all(character in "0123456789abcdef" for character in value)
    )


def _reject_json_constant(value: str) -> None:
    raise ValueError(f"non-standard JSON constant {value!r}")


def _object_without_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON field {key!r}")
        result[key] = value
    return result


def load_manifest(path: Path, label: str) -> Any:
    try:
        with path.open(encoding="utf-8") as handle:
            return json.load(
                handle,
                object_pairs_hook=_object_without_duplicate_keys,
                parse_constant=_reject_json_constant,
            )
    except (json.JSONDecodeError, UnicodeDecodeError, ValueError) as exc:
        fail(f"{label}: invalid JSON: {exc}")
    except OSError as exc:
        fail(f"{label}: cannot read {path}: {exc}")


def require_object(value: object, field: str, label: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        fail(f"{label}: {field} must be an object")
    return value


def require_nonempty_string(container: dict[str, Any], field: str, label: str) -> str:
    value = container.get(field)
    if not isinstance(value, str) or not value:
        fail(f"{label}: {field} must be a non-empty string")
    if value != value.strip() or any(ord(character) < 0x20 or ord(character) == 0x7F for character in value):
        fail(f"{label}: {field} contains forbidden whitespace or control characters")
    return value


def compute_fixture_set_id(workloads: tuple[Workload, ...]) -> str:
    canonical = "".join(
        f"{workload.workload_id}\n{workload.fixture_id}\n{workload.fixture_hash}\n"
        for workload in workloads
    )
    return hashlib.sha256(canonical.encode("utf-8")).hexdigest()


def validate_manifest(path: Path, position: int, expected_head: str) -> Manifest:
    label = f"manifest #{position} ({path})"
    raw = require_object(load_manifest(path, label), "top level", label)

    schema_version = raw.get("schema_version")
    if schema_version != SCHEMA_VERSION:
        fail(f"{label}: unrecognized schema_version: {schema_version!r}")

    head_sha = raw.get("head_sha")
    if not is_valid_git_sha(head_sha):
        fail(f"{label}: head_sha must be exactly 40 lowercase hexadecimal characters")
    if head_sha != expected_head:
        fail(f"{label}: head_sha {head_sha!r} does not match expected {expected_head!r}")

    raw_toolchain = require_object(raw.get("toolchain"), "toolchain", label)
    toolchain = Toolchain(
        compiler=require_nonempty_string(raw_toolchain, "compiler", f"{label}/toolchain"),
        compiler_version=require_nonempty_string(
            raw_toolchain, "compiler_version", f"{label}/toolchain"
        ),
        os=require_nonempty_string(raw_toolchain, "os", f"{label}/toolchain"),
        architecture=require_nonempty_string(
            raw_toolchain, "architecture", f"{label}/toolchain"
        ),
    )

    build_type = require_nonempty_string(raw, "build_type", label)
    fixture_set_id = raw.get("fixture_set_id")
    if not is_valid_sha256_hex(fixture_set_id):
        fail(f"{label}: fixture_set_id must be 64 lowercase hexadecimal characters")

    raw_workloads = raw.get("workloads")
    if not isinstance(raw_workloads, list):
        fail(f"{label}: workloads must be an array")

    workloads: list[Workload] = []
    for workload_index, raw_workload_value in enumerate(raw_workloads, start=1):
        workload_label = f"{label}/workload #{workload_index}"
        raw_workload = require_object(raw_workload_value, "workload", workload_label)
        workload_id = require_nonempty_string(raw_workload, "workload_id", workload_label)
        fixture_id = require_nonempty_string(raw_workload, "fixture_id", workload_label)
        fixture_hash = raw_workload.get("fixture_hash")
        if not is_valid_sha256_hex(fixture_hash):
            fail(f"{workload_label}: fixture_hash must be 64 lowercase hexadecimal characters")
        semantic_digest = raw_workload.get("semantic_digest")
        if not is_valid_sha256_hex(semantic_digest):
            fail(
                f"{workload_label}: semantic_digest must be 64 lowercase hexadecimal characters"
            )
        workloads.append(
            Workload(workload_id, fixture_id, fixture_hash, semantic_digest)
        )

    workload_ids = tuple(workload.workload_id for workload in workloads)
    if workload_ids != AUTHORITATIVE_WORKLOAD_IDS:
        fail(
            f"{label}: workloads must be exactly the authoritative ordered set "
            f"{AUTHORITATIVE_WORKLOAD_IDS!r}; got {workload_ids!r}"
        )

    validated_workloads = tuple(workloads)
    computed_fixture_set_id = compute_fixture_set_id(validated_workloads)
    if fixture_set_id != computed_fixture_set_id:
        fail(
            f"{label}: fixture_set_id {fixture_set_id!r} does not match workload identity "
            f"{computed_fixture_set_id!r}"
        )

    return Manifest(
        label=label,
        path=path,
        schema_version=schema_version,
        head_sha=head_sha,
        toolchain=toolchain,
        build_type=build_type,
        fixture_set_id=fixture_set_id,
        workloads=validated_workloads,
    )


def semantic_role(manifest: Manifest) -> str:
    key = (manifest.toolchain.compiler, manifest.toolchain.os)
    role = CROSS_COMPILER_ROLES.get(key)
    if role is None:
        fail(
            f"{manifest.label}: unsupported compiler/OS role "
            f"{manifest.toolchain.compiler!r}/{manifest.toolchain.os!r}"
        )
    return role


def validate_transport_hint(manifest: Manifest, role: str) -> None:
    matching_hints = {
        expected_role
        for hint, expected_role in TRANSPORT_ROLE_HINTS.items()
        if hint in str(manifest.path)
    }
    if len(matching_hints) > 1:
        fail(f"{manifest.label}: path contains conflicting transport role hints")
    if matching_hints and role not in matching_hints:
        fail(
            f"{manifest.label}: transport path expects {next(iter(matching_hints))}, "
            f"but manifest metadata proves {role}"
        )


def compare_semantic_identity(reference: Manifest, candidate: Manifest) -> None:
    if candidate.fixture_set_id != reference.fixture_set_id:
        fail(
            f"fixture_set_id mismatch: {reference.label}={reference.fixture_set_id} vs "
            f"{candidate.label}={candidate.fixture_set_id}"
        )

    for reference_workload, candidate_workload in zip(
        reference.workloads, candidate.workloads, strict=True
    ):
        workload_id = reference_workload.workload_id
        if candidate_workload.fixture_id != reference_workload.fixture_id:
            fail(
                f"fixture_id mismatch for {workload_id}: "
                f"{reference.label}={reference_workload.fixture_id!r} vs "
                f"{candidate.label}={candidate_workload.fixture_id!r}"
            )
        if candidate_workload.fixture_hash != reference_workload.fixture_hash:
            fail(
                f"fixture_hash mismatch for {workload_id}: "
                f"{reference.label}={reference_workload.fixture_hash} vs "
                f"{candidate.label}={candidate_workload.fixture_hash}"
            )
        if candidate_workload.semantic_digest != reference_workload.semantic_digest:
            fail(
                f"semantic_digest mismatch for {workload_id}: "
                f"{reference.label}={reference_workload.semantic_digest} vs "
                f"{candidate.label}={candidate_workload.semantic_digest}"
            )


def compare_cross_compiler(manifests: list[Manifest]) -> dict[str, Manifest]:
    if len(manifests) != 3:
        fail(f"cross-compiler mode requires exactly 3 manifests, got {len(manifests)}")

    by_role: dict[str, Manifest] = {}
    for manifest in manifests:
        if manifest.build_type != "Release":
            fail(f"{manifest.label}: build_type is {manifest.build_type!r}, expected 'Release'")
        role = semantic_role(manifest)
        validate_transport_hint(manifest, role)
        if role in by_role:
            fail(f"duplicate semantic producer role {role}: {by_role[role].label}, {manifest.label}")
        by_role[role] = manifest

    missing_roles = [role for role in ROLE_ORDER if role not in by_role]
    if missing_roles:
        fail(f"missing semantic producer roles: {missing_roles!r}")

    reference = by_role[ROLE_ORDER[0]]
    for role in ROLE_ORDER[1:]:
        compare_semantic_identity(reference, by_role[role])
    return by_role


def compare_replay(manifests: list[Manifest]) -> None:
    if len(manifests) != 4:
        fail(f"replay mode requires exactly 4 manifests, got {len(manifests)}")

    expected_build_types = ("Debug", "Debug", "Release", "Release")
    reference_toolchain = manifests[0].toolchain
    for index, (manifest, expected_build_type) in enumerate(
        zip(manifests, expected_build_types, strict=True), start=1
    ):
        if manifest.build_type != expected_build_type:
            fail(
                f"replay run #{index}: build_type is {manifest.build_type!r}, "
                f"expected {expected_build_type!r}"
            )
        if (manifest.toolchain.compiler, manifest.toolchain.os) != ("Clang", "Linux"):
            fail(
                f"replay run #{index}: expected Clang/Linux, got "
                f"{manifest.toolchain.compiler}/{manifest.toolchain.os}"
            )
        if manifest.toolchain != reference_toolchain:
            fail(
                f"replay run #{index}: toolchain metadata differs from run #1: "
                f"{manifest.toolchain!r} vs {reference_toolchain!r}"
            )

    for manifest in manifests[1:]:
        compare_semantic_identity(manifests[0], manifest)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--mode", choices=("cross-compiler", "replay"), default="cross-compiler"
    )
    parser.add_argument("--expected-head", required=True)
    parser.add_argument("manifests", nargs="+")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        if not is_valid_git_sha(args.expected_head):
            fail("--expected-head must be exactly 40 lowercase hexadecimal characters")

        paths = [Path(value) for value in args.manifests]
        expected_count = 3 if args.mode == "cross-compiler" else 4
        if len(paths) != expected_count:
            fail(
                f"{args.mode} mode requires exactly {expected_count} manifests, got {len(paths)}"
            )
        for path in paths:
            if not path.is_file():
                fail(f"manifest not found: {path}")

        manifests = [
            validate_manifest(path, position, args.expected_head)
            for position, path in enumerate(paths, start=1)
        ]

        if args.mode == "cross-compiler":
            by_role = compare_cross_compiler(manifests)
            reference = by_role[ROLE_ORDER[0]]
            print("M5 cross-compiler semantic manifest comparison PASS")
            print(f"  schema: {reference.schema_version}")
            print(f"  HEAD: {reference.head_sha}")
            print(f"  fixture_set_id: {reference.fixture_set_id}")
            print("  toolchains:")
            for role in ROLE_ORDER:
                toolchain = by_role[role].toolchain
                print(
                    f"    {role}: compiler={toolchain.compiler} "
                    f"version={toolchain.compiler_version} "
                    f"architecture={toolchain.architecture}"
                )
        else:
            compare_replay(manifests)
            reference = manifests[0]
            print("M5 replay semantic manifest comparison PASS")
            print(f"  schema: {reference.schema_version}")
            print(f"  HEAD: {reference.head_sha}")
            print(f"  fixture_set_id: {reference.fixture_set_id}")
            print(
                f"  toolchain: {reference.toolchain.compiler} "
                f"{reference.toolchain.compiler_version} on "
                f"{reference.toolchain.os} {reference.toolchain.architecture}"
            )
            for index, manifest in enumerate(manifests, start=1):
                print(f"  run #{index}: build_type={manifest.build_type} evidence=valid")

        print("  workloads:")
        for workload in reference.workloads:
            print(
                f"    {workload.workload_id}: fixture_id={workload.fixture_id} "
                f"fixture_hash={workload.fixture_hash} "
                f"semantic_digest={workload.semantic_digest}"
            )
        return 0
    except ManifestValidationError as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
