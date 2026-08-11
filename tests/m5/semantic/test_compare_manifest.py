#!/usr/bin/env python3
"""Tests for the M5 semantic manifest comparator."""

import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


COMPARATOR = Path(__file__).resolve().parent.parent.parent.parent / "scripts" / "compare-m5-semantic-manifests.py"

HEAD_SHA = "a1db0f8374bec84d10b0005552983dd44b4e2026"  # 40 chars

MANIFEST_TEMPLATE = {
    "schema_version": "M5_SEMANTIC_MANIFEST_V1",
    "head_sha": HEAD_SHA,
    "toolchain": {
        "compiler": "GCC",
        "compiler_version": "15.0.0",
        "os": "Linux",
        "architecture": "x86_64",
    },
    "build_type": "Release",
    "fixture_set_id": "abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234",
    "workloads": [
        {
            "workload_id": "m5-small-core-spot-v1",
            "fixture_id": "m5-small-spot-v1",
            "fixture_hash": "14f44a6794af5c87fb3b359e16c1901a284e6da7946f02d6be3dc8cacb249227",
            "semantic_digest": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        },
        {
            "workload_id": "m5-small-core-usdm-v1",
            "fixture_id": "m5-small-usdm-v1",
            "fixture_hash": "f3e60732c8e4452f86231548cd0c5918b914487063d3f4f22d6588f088141e03",
            "semantic_digest": "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
        },
        {
            "workload_id": "m5-small-adapter-spot-v1",
            "fixture_id": "m5-small-spot-v1",
            "fixture_hash": "14f44a6794af5c87fb3b359e16c1901a284e6da7946f02d6be3dc8cacb249227",
            "semantic_digest": "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc",
        },
        {
            "workload_id": "m5-small-adapter-usdm-v1",
            "fixture_id": "m5-small-usdm-v1",
            "fixture_hash": "f3e60732c8e4452f86231548cd0c5918b914487063d3f4f22d6588f088141e03",
            "semantic_digest": "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd",
        },
    ],
}


def make_manifest(**overrides) -> dict:
    import copy
    m = copy.deepcopy(MANIFEST_TEMPLATE)
    for key, value in overrides.items():
        m[key] = value
    return m


def write_manifest(path: Path, **overrides) -> None:
    m = make_manifest(**overrides)
    with open(path, "w", encoding="utf-8") as fh:
        json.dump(m, fh)


def make_toolchain(compiler: str, version: str, os_name: str, arch: str) -> dict:
    return {
        "compiler": compiler,
        "compiler_version": version,
        "os": os_name,
        "architecture": arch,
    }


def run_comparator(*paths: Path, expected_head: str | None = None) -> tuple[int, str, str]:
    cmd = [sys.executable, str(COMPARATOR)]
    if expected_head:
        cmd.extend(["--expected-head", expected_head])
    cmd.extend(str(p) for p in paths)
    result = subprocess.run(cmd, capture_output=True, text=True)
    return result.returncode, result.stdout, result.stderr


class ComparatorPositiveTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.tmp_path = Path(self.tmp.name)

    def tearDown(self):
        self.tmp.cleanup()

    def test_three_matching_manifests_pass(self):
        gcc = self.tmp_path / "m5-semantic-manifest-ubuntu-gcc"
        clang = self.tmp_path / "m5-semantic-manifest-ubuntu-clang"
        appleclang = self.tmp_path / "m5-semantic-manifest-macos-appleclang"

        write_manifest(gcc, toolchain=make_toolchain("GCC", "15.0.0", "Linux", "x86_64"))
        write_manifest(clang, toolchain=make_toolchain("Clang", "20.1.0", "Linux", "x86_64"))
        write_manifest(appleclang, toolchain=make_toolchain("AppleClang", "17.0.0", "Darwin", "arm64"))

        rc, stdout, stderr = run_comparator(gcc, clang, appleclang, expected_head=HEAD_SHA)
        self.assertEqual(rc, 0, f"expected success\nstdout: {stdout}\nstderr: {stderr}")
        self.assertIn("PASS", stdout)


class ComparatorNegativeTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.tmp_path = Path(self.tmp.name)

    def tearDown(self):
        self.tmp.cleanup()

    def _three_manifests(self) -> tuple[Path, Path, Path]:
        gcc = self.tmp_path / "m5-semantic-manifest-ubuntu-gcc"
        clang = self.tmp_path / "m5-semantic-manifest-ubuntu-clang"
        appleclang = self.tmp_path / "m5-semantic-manifest-macos-appleclang"
        write_manifest(gcc, toolchain=make_toolchain("GCC", "15.0.0", "Linux", "x86_64"))
        write_manifest(clang, toolchain=make_toolchain("Clang", "20.1.0", "Linux", "x86_64"))
        write_manifest(appleclang, toolchain=make_toolchain("AppleClang", "17.0.0", "Darwin", "arm64"))
        return gcc, clang, appleclang

    def test_missing_manifest_fails(self):
        rc, _, stderr = run_comparator(self.tmp_path / "nonexistent")
        self.assertNotEqual(rc, 0)

    def test_invalid_json_fails(self):
        path = self.tmp_path / "bad.json"
        path.write_text("not json", encoding="utf-8")
        rc, _, stderr = run_comparator(path)
        self.assertNotEqual(rc, 0)

    def test_unknown_schema_version_fails(self):
        gcc, clang, appleclang = self._three_manifests()
        write_manifest(gcc, schema_version="M5_SEMANTIC_MANIFEST_V99",
                       toolchain=make_toolchain("GCC", "15.0.0", "Linux", "x86_64"))
        rc, _, stderr = run_comparator(gcc, clang, appleclang)
        self.assertNotEqual(rc, 0, f"expected failure for unknown schema\nstderr: {stderr}")

    def test_head_mismatch_fails(self):
        gcc, clang, appleclang = self._three_manifests()
        write_manifest(gcc, head_sha="bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
                       toolchain=make_toolchain("GCC", "15.0.0", "Linux", "x86_64"))
        rc, _, stderr = run_comparator(gcc, clang, appleclang)
        self.assertNotEqual(rc, 0, f"expected failure for head mismatch\nstderr: {stderr}")

    def test_unexpected_head_fails(self):
        gcc, clang, appleclang = self._three_manifests()
        rc, _, stderr = run_comparator(gcc, clang, appleclang,
                                       expected_head="bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb")
        self.assertNotEqual(rc, 0, f"expected failure for unexpected head\nstderr: {stderr}")

    def test_non_release_fails(self):
        gcc, clang, appleclang = self._three_manifests()
        write_manifest(gcc, build_type="Debug",
                       toolchain=make_toolchain("GCC", "15.0.0", "Linux", "x86_64"))
        rc, _, stderr = run_comparator(gcc, clang, appleclang)
        self.assertNotEqual(rc, 0, f"expected failure for non-Release\nstderr: {stderr}")

    def test_fixture_set_mismatch_fails(self):
        gcc, clang, appleclang = self._three_manifests()
        write_manifest(clang, fixture_set_id="1111111111111111111111111111111111111111111111111111111111111111",
                       toolchain=make_toolchain("Clang", "20.1.0", "Linux", "x86_64"))
        rc, _, stderr = run_comparator(gcc, clang, appleclang)
        self.assertNotEqual(rc, 0, f"expected failure for fixture_set mismatch\nstderr: {stderr}")

    def test_missing_workload_fails(self):
        gcc, clang, appleclang = self._three_manifests()
        write_manifest(gcc, workloads=[
            {
                "workload_id": "m5-small-core-spot-v1",
                "fixture_id": "m5-small-spot-v1",
                "fixture_hash": "14f44a6794af5c87fb3b359e16c1901a284e6da7946f02d6be3dc8cacb249227",
                "semantic_digest": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
            },
            {
                "workload_id": "m5-small-adapter-spot-v1",
                "fixture_id": "m5-small-spot-v1",
                "fixture_hash": "14f44a6794af5c87fb3b359e16c1901a284e6da7946f02d6be3dc8cacb249227",
                "semantic_digest": "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc",
            },
        ])
        rc, _, stderr = run_comparator(gcc, clang, appleclang)
        self.assertNotEqual(rc, 0, f"expected failure for missing workload\nstderr: {stderr}")

    def test_extra_workload_fails(self):
        gcc, clang, appleclang = self._three_manifests()
        extra = make_manifest(workloads=[])
        extra["workloads"] = [
            {
                "workload_id": "m5-small-core-spot-v1",
                "fixture_id": "m5-small-spot-v1",
                "fixture_hash": "14f44a6794af5c87fb3b359e16c1901a284e6da7946f02d6be3dc8cacb249227",
                "semantic_digest": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
            },
            {
                "workload_id": "m5-small-core-usdm-v1",
                "fixture_id": "m5-small-usdm-v1",
                "fixture_hash": "f3e60732c8e4452f86231548cd0c5918b914487063d3f4f22d6588f088141e03",
                "semantic_digest": "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
            },
            {
                "workload_id": "m5-small-adapter-spot-v1",
                "fixture_id": "m5-small-spot-v1",
                "fixture_hash": "14f44a6794af5c87fb3b359e16c1901a284e6da7946f02d6be3dc8cacb249227",
                "semantic_digest": "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc",
            },
        ]
        write_manifest(appleclang, workloads=extra["workloads"],
                       toolchain=make_toolchain("AppleClang", "17.0.0", "Darwin", "arm64"))
        rc, _, stderr = run_comparator(gcc, clang, appleclang)
        self.assertNotEqual(rc, 0, f"expected failure for extra workload\nstderr: {stderr}")

    def test_duplicate_workload_id_fails(self):
        gcc, clang, appleclang = self._three_manifests()
        write_manifest(gcc, workloads=[
            {
                "workload_id": "m5-small-core-spot-v1",
                "fixture_id": "m5-small-spot-v1",
                "fixture_hash": "14f44a6794af5c87fb3b359e16c1901a284e6da7946f02d6be3dc8cacb249227",
                "semantic_digest": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
            },
            {
                "workload_id": "m5-small-core-spot-v1",
                "fixture_id": "m5-small-spot-v1",
                "fixture_hash": "14f44a6794af5c87fb3b359e16c1901a284e6da7946f02d6be3dc8cacb249227",
                "semantic_digest": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
            },
        ])
        rc, _, stderr = run_comparator(gcc, clang, appleclang)
        self.assertNotEqual(rc, 0, f"expected failure for duplicate workload ID\nstderr: {stderr}")

    def test_fixture_id_mismatch_fails(self):
        gcc, clang, appleclang = self._three_manifests()
        workloads = list(MANIFEST_TEMPLATE["workloads"])
        workloads[0] = {**workloads[0], "fixture_id": "wrong-id"}
        write_manifest(appleclang, workloads=workloads,
                       toolchain=make_toolchain("AppleClang", "17.0.0", "Darwin", "arm64"))
        rc, _, stderr = run_comparator(gcc, clang, appleclang)
        self.assertNotEqual(rc, 0, f"expected failure for fixture_id mismatch\nstderr: {stderr}")

    def test_fixture_hash_mismatch_fails(self):
        gcc, clang, appleclang = self._three_manifests()
        workloads = list(MANIFEST_TEMPLATE["workloads"])
        workloads[1] = {**workloads[1], "fixture_hash": "0" * 64}
        write_manifest(appleclang, workloads=workloads,
                       toolchain=make_toolchain("AppleClang", "17.0.0", "Darwin", "arm64"))
        rc, _, stderr = run_comparator(gcc, clang, appleclang)
        self.assertNotEqual(rc, 0, f"expected failure for fixture_hash mismatch\nstderr: {stderr}")

    def test_digest_mismatch_fails(self):
        gcc, clang, appleclang = self._three_manifests()
        workloads = list(MANIFEST_TEMPLATE["workloads"])
        workloads[2] = {**workloads[2], "semantic_digest": "1" * 64}
        write_manifest(appleclang, workloads=workloads,
                       toolchain=make_toolchain("AppleClang", "17.0.0", "Darwin", "arm64"))
        rc, stdout, stderr = run_comparator(gcc, clang, appleclang)
        self.assertNotEqual(rc, 0, f"expected failure for digest mismatch\nstderr: {stderr}")
        self.assertIn("digest mismatch", stderr.lower() or stdout.lower())

    def test_invalid_digest_syntax_fails(self):
        gcc, clang, appleclang = self._three_manifests()
        workloads = list(MANIFEST_TEMPLATE["workloads"])
        workloads[0] = {**workloads[0], "semantic_digest": "not-a-valid-sha-256-hex-string-but-still"}
        write_manifest(gcc, workloads=workloads,
                       toolchain=make_toolchain("GCC", "15.0.0", "Linux", "x86_64"))
        rc, _, stderr = run_comparator(gcc, clang, appleclang)
        self.assertNotEqual(rc, 0, f"expected failure for invalid digest\nstderr: {stderr}")


if __name__ == "__main__":
    unittest.main()
