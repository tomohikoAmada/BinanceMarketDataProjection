#!/usr/bin/env python3
"""M5 cross-compiler semantic manifest comparator.

Compares semantic manifests produced by different toolchains and verifies:
- identical HEAD SHA
- identical fixture-set identity
- identical fixture hashes per workload
- identical semantic digests per workload
- valid SHA-256 digest format
- recognized schema version
- Release build type in cross-compiler mode
- exactly the expected number of manifests

Fails closed: any unexpected condition exits nonzero with a diagnostic.
"""

import json
import sys
from pathlib import Path


def die(msg: str) -> None:
    print(f"FAIL: {msg}", file=sys.stderr)
    sys.exit(1)


def is_valid_sha256_hex(value: str) -> bool:
    return len(value) == 64 and all(c in "0123456789abcdef" for c in value)


def load_manifest(path: Path) -> dict:
    try:
        with open(path, encoding="utf-8") as fh:
            return json.load(fh)
    except json.JSONDecodeError as exc:
        die(f"invalid JSON in {path.name}: {exc}")
    except OSError as exc:
        die(f"cannot read {path.name}: {exc}")


def validate_manifest_schema(manifest: dict, name: str) -> None:
    version = manifest.get("schema_version")
    if version != "M5_SEMANTIC_MANIFEST_V1":
        die(f"{name}: unrecognized schema_version: {version!r}")


def validate_common_fields(manifests: dict[str, dict], expected_head: str | None = None) -> None:
    """Validate fields that must be identical across all manifests."""
    names = sorted(manifests.keys())

    if len(names) != 3:
        die(f"expected exactly 3 manifests, got {len(names)}: {', '.join(names)}")

    first = manifests[names[0]]

    head_sha = first.get("head_sha")
    if not head_sha or len(head_sha) != 40:
        die(f"{names[0]}: invalid head_sha: {head_sha!r}")

    if expected_head is not None and head_sha != expected_head:
        die(f"head_sha {head_sha!r} does not match expected {expected_head!r}")

    for name in names[1:]:
        m = manifests[name]
        m_head = m.get("head_sha")
        if m_head != head_sha:
            die(f"HEAD SHA mismatch: {names[0]}={head_sha} vs {name}={m_head}")

    build_types = {name: manifests[name].get("build_type") for name in names}
    for name, bt in build_types.items():
        if bt != "Release":
            die(f"{name}: build_type is {bt!r}, expected Release")

    fixture_set = first.get("fixture_set_id")
    if not fixture_set or not is_valid_sha256_hex(fixture_set):
        die(f"{names[0]}: invalid fixture_set_id: {fixture_set!r}")

    for name in names[1:]:
        m_fs = manifests[name].get("fixture_set_id")
        if m_fs != fixture_set:
            die(f"fixture_set_id mismatch: {names[0]}={fixture_set} vs {name}={m_fs}")


def validate_workloads(manifests: dict[str, dict]) -> None:
    """Validate workload entries are identical across all manifests."""
    names = sorted(manifests.keys())
    first_name = names[0]
    first_workloads = manifests[first_name].get("workloads", [])
    if not isinstance(first_workloads, list):
        die(f"{first_name}: workloads is not a list")

    first_ids = [w.get("workload_id") for w in first_workloads]

    seen = set()
    for wid in first_ids:
        if wid in seen:
            die(f"{first_name}: duplicate workload_id: {wid}")
        seen.add(wid)

    for name in names[1:]:
        m_workloads = manifests[name].get("workloads", [])
        if not isinstance(m_workloads, list):
            die(f"{name}: workloads is not a list")

        m_ids = [w.get("workload_id") for w in m_workloads]
        if m_ids != first_ids:
            missing = set(first_ids) - set(m_ids)
            extra = set(m_ids) - set(first_ids)
            parts = []
            if missing:
                parts.append(f"missing: {', '.join(sorted(missing))}")
            if extra:
                parts.append(f"extra: {', '.join(sorted(extra))}")
            die(f"workload set mismatch: {name} vs {first_name}: {'; '.join(parts)}")

    first_by_id = {w["workload_id"]: w for w in first_workloads}

    for wid in first_ids:
        first_w = first_by_id[wid]

        fixture_id = first_w.get("fixture_id")
        if not fixture_id:
            die(f"{first_name}/{wid}: missing fixture_id")

        fixture_hash = first_w.get("fixture_hash")
        if not fixture_hash or not is_valid_sha256_hex(fixture_hash):
            die(f"{first_name}/{wid}: invalid fixture_hash: {fixture_hash!r}")

        digest = first_w.get("semantic_digest")
        if not digest or not is_valid_sha256_hex(digest):
            die(f"{first_name}/{wid}: invalid semantic_digest: {digest!r}")

        for name in names[1:]:
            m_by_id = {w["workload_id"]: w for w in manifests[name].get("workloads", [])}
            m_w = m_by_id.get(wid)
            if m_w is None:
                die(f"{name}: missing workload {wid}")
            m_fixture_id = m_w.get("fixture_id")
            m_fixture_hash = m_w.get("fixture_hash")
            m_digest = m_w.get("semantic_digest")

            if m_fixture_id != fixture_id:
                die(
                    f"fixture_id mismatch for {wid}: "
                    f"{first_name}={fixture_id} vs {name}={m_fixture_id}"
                )

            if m_fixture_hash != fixture_hash:
                die(
                    f"fixture_hash mismatch for {wid}: "
                    f"{first_name}={fixture_hash} vs {name}={m_fixture_hash}"
                )

            if m_digest != digest:
                die(
                    f"semantic digest mismatch for {wid}:\n"
                    f"  fixture_id={fixture_id}\n"
                    f"  fixture_hash={fixture_hash}\n"
                    f"  {first_name} digest: {digest}\n"
                    f"  {name} digest: {m_digest}"
                )


def compare_manifests(manifest_paths: list[Path], expected_head: str | None = None) -> bool:
    """Compare the given manifests. Returns True on success, dies on failure."""
    manifests: dict[str, dict] = {}
    expected_toolchains = {
        "ubuntu-gcc": "m5-semantic-manifest-ubuntu-gcc",
        "ubuntu-clang": "m5-semantic-manifest-ubuntu-clang",
        "macos-appleclang": "m5-semantic-manifest-macos-appleclang",
    }

    for path in manifest_paths:
        tc_key = None
        path_str = str(path)
        for tc_name in expected_toolchains:
            if tc_name in path_str:
                tc_key = tc_name
                break
        if tc_key is None:
            die(f"cannot identify toolchain for manifest: {path}")
        manifests[tc_key] = load_manifest(path)

    missing = expected_toolchains.keys() - manifests.keys()
    if missing:
        die(f"missing manifests for toolchains: {sorted(missing)}")

    validate_manifest_schema(manifests[list(manifests.keys())[0]], list(manifests.keys())[0])

    validate_common_fields(manifests, expected_head)

    validate_workloads(manifests)

    return True


def main() -> None:
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} [--expected-head SHA] MANIFEST...", file=sys.stderr)
        sys.exit(1)

    args = sys.argv[1:]
    expected_head = None
    if args[0] == "--expected-head":
        if len(args) < 2:
            die("--expected-head requires a value")
        expected_head = args[1]
        args = args[2:]

    manifest_paths = [Path(p) for p in args]
    for p in manifest_paths:
        if not p.is_file():
            die(f"manifest not found: {p}")

    if compare_manifests(manifest_paths, expected_head):
        print("M5 semantic manifest comparison PASS")
        names = sorted(p.name for p in manifest_paths)
        manifests = {n: load_manifest(p) for n, p in zip(names, manifest_paths)}
        first_name = names[0]
        first = manifests[first_name]
        print(f"  schema: {first.get('schema_version')}")
        print(f"  HEAD: {first.get('head_sha')}")
        print(f"  fixture_set_id: {first.get('fixture_set_id')}")
        print(f"  workloads: {len(first.get('workloads', []))}")
        for w in first.get("workloads", []):
            print(f"    {w.get('workload_id')}: digest={w.get('semantic_digest')}")
        print("  toolchains:")
        for name in sorted(manifests.keys()):
            tc = manifests[name].get("toolchain", {})
            print(f"    {name}: {tc.get('compiler')} {tc.get('compiler_version')} on {tc.get('os')} {tc.get('architecture')}")
        sys.exit(0)


if __name__ == "__main__":
    main()
