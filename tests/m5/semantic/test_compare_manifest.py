#!/usr/bin/env python3
"""Adversarial tests for M5 semantic-manifest evidence validation."""

import copy
import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


COMPARATOR = (
    Path(__file__).resolve().parent.parent.parent.parent
    / "scripts"
    / "compare-m5-semantic-manifests.py"
)
HEAD_SHA = "a1db0f8374bec84d10b0005552983dd44b4e2026"
WORKLOADS = [
    {
        "workload_id": "m5-small-core-spot-v1",
        "fixture_id": "m5-small-spot-v1",
        "fixture_hash": "14f44a6794af5c87fb3b359e16c1901a284e6da7946f02d6be3dc8cacb249227",
        "semantic_digest": "a" * 64,
    },
    {
        "workload_id": "m5-small-core-usdm-v1",
        "fixture_id": "m5-small-usdm-v1",
        "fixture_hash": "f3e60732c8e4452f86231548cd0c5918b914487063d3f4f22d6588f088141e03",
        "semantic_digest": "b" * 64,
    },
    {
        "workload_id": "m5-small-adapter-spot-v1",
        "fixture_id": "m5-small-spot-v1",
        "fixture_hash": "14f44a6794af5c87fb3b359e16c1901a284e6da7946f02d6be3dc8cacb249227",
        "semantic_digest": "c" * 64,
    },
    {
        "workload_id": "m5-small-adapter-usdm-v1",
        "fixture_id": "m5-small-usdm-v1",
        "fixture_hash": "f3e60732c8e4452f86231548cd0c5918b914487063d3f4f22d6588f088141e03",
        "semantic_digest": "d" * 64,
    },
]


def fixture_set_id(workloads: list[dict]) -> str:
    canonical = "".join(
        f"{workload['workload_id']}\n{workload['fixture_id']}\n{workload['fixture_hash']}\n"
        for workload in workloads
    )
    return hashlib.sha256(canonical.encode("utf-8")).hexdigest()


def toolchain(compiler: str, version: str, os_name: str, architecture: str) -> dict:
    return {
        "compiler": compiler,
        "compiler_version": version,
        "os": os_name,
        "architecture": architecture,
    }


def make_manifest(**overrides) -> dict:
    workloads = copy.deepcopy(WORKLOADS)
    manifest = {
        "schema_version": "M5_SEMANTIC_MANIFEST_V2",
        "observation_schema_version": "M5_SEMANTIC_OBSERVATION_V2",
        "head_sha": HEAD_SHA,
        "toolchain": toolchain("GNU", "13.3.0", "Linux", "x86_64"),
        "build_type": "Release",
        "fixture_set_id": fixture_set_id(workloads),
        "workloads": workloads,
    }
    manifest.update(copy.deepcopy(overrides))
    if "workloads" in overrides and "fixture_set_id" not in overrides:
        candidate_workloads = manifest["workloads"]
        if (
            isinstance(candidate_workloads, list)
            and all(
                isinstance(workload, dict)
                and all(field in workload for field in ("workload_id", "fixture_id", "fixture_hash"))
                for workload in candidate_workloads
            )
        ):
            manifest["fixture_set_id"] = fixture_set_id(candidate_workloads)
    return manifest


def write_manifest(path: Path, **overrides) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        json.dump(make_manifest(**overrides), handle)


def run_comparator(
    mode: str, paths: list[Path], expected_head: str = HEAD_SHA
) -> tuple[int, str, str]:
    command = [
        sys.executable,
        str(COMPARATOR),
        "--mode",
        mode,
        "--expected-head",
        expected_head,
        *(str(path) for path in paths),
    ]
    result = subprocess.run(command, capture_output=True, text=True, check=False)
    return result.returncode, result.stdout, result.stderr


class CrossCompilerTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.gcc = self.root / "ubuntu-gcc" / "m5-semantic-manifest.json"
        self.clang = self.root / "ubuntu-clang" / "m5-semantic-manifest.json"
        self.apple = self.root / "macos-appleclang" / "m5-semantic-manifest.json"
        self.write_valid_manifests()

    def tearDown(self):
        self.temporary.cleanup()

    def write_valid_manifests(self):
        write_manifest(
            self.gcc, toolchain=toolchain("GNU", "13.3.0", "Linux", "x86_64")
        )
        write_manifest(
            self.clang, toolchain=toolchain("Clang", "18.1.3", "Linux", "x86_64")
        )
        write_manifest(
            self.apple,
            toolchain=toolchain("AppleClang", "21.0.0.21000101", "Darwin", "arm64"),
        )

    def run_cross(self, paths: list[Path] | None = None, expected_head: str = HEAD_SHA):
        return run_comparator(
            "cross-compiler", paths or [self.gcc, self.clang, self.apple], expected_head
        )

    def assert_cross_fails(self, paths: list[Path] | None = None, expected_head: str = HEAD_SHA):
        return_code, stdout, stderr = self.run_cross(paths, expected_head)
        self.assertNotEqual(return_code, 0, f"stdout:\n{stdout}\nstderr:\n{stderr}")

    def test_three_matching_manifests_pass_and_report_all_roles(self):
        return_code, stdout, stderr = self.run_cross()
        self.assertEqual(return_code, 0, f"stdout:\n{stdout}\nstderr:\n{stderr}")
        self.assertIn("GNU/Linux: compiler=GNU", stdout)
        self.assertIn("Clang/Linux: compiler=Clang", stdout)
        self.assertIn("AppleClang/Darwin: compiler=AppleClang", stdout)

    def test_unknown_fields_are_ignored_for_forward_compatibility(self):
        manifest = make_manifest(
            toolchain={
                **toolchain("GNU", "13.3.0", "Linux", "x86_64"),
                "future_toolchain_field": {"value": 1},
            },
            future_top_level_field=[1, 2, 3],
        )
        manifest["workloads"][0]["future_workload_field"] = True
        with self.gcc.open("w", encoding="utf-8") as handle:
            json.dump(manifest, handle)
        return_code, _, stderr = self.run_cross()
        self.assertEqual(return_code, 0, stderr)

    def test_missing_manifest_fails(self):
        self.assert_cross_fails([self.gcc, self.clang, self.root / "missing.json"])

    def test_invalid_json_fails(self):
        self.clang.write_text("not json", encoding="utf-8")
        self.assert_cross_fails()

    def test_top_level_must_be_object(self):
        self.clang.write_text("[]", encoding="utf-8")
        self.assert_cross_fails()

    def test_duplicate_json_field_fails(self):
        self.clang.write_text('{"schema_version":"x","schema_version":"y"}', encoding="utf-8")
        self.assert_cross_fails()

    def test_malformed_expected_head_fails_before_comparison(self):
        malformed_values = [
            "z" * 40,
            "a" * 39,
            "a" * 41,
            "0x" + "a" * 38,
            "A" + "a" * 39,
            " " + "a" * 39,
            "a" * 39 + "\n",
        ]
        for value in malformed_values:
            with self.subTest(value=repr(value)):
                self.assert_cross_fails(expected_head=value)

    def test_unknown_schema_only_in_manifest_two_fails(self):
        write_manifest(
            self.clang,
            schema_version="M5_SEMANTIC_MANIFEST_V99",
            toolchain=toolchain("Clang", "18.1.3", "Linux", "x86_64"),
        )
        self.assert_cross_fails()

    def test_unknown_schema_only_in_manifest_three_fails(self):
        write_manifest(
            self.apple,
            schema_version="M5_SEMANTIC_MANIFEST_V99",
            toolchain=toolchain("AppleClang", "21", "Darwin", "arm64"),
        )
        self.assert_cross_fails()

    def test_missing_observation_schema_fails(self):
        manifest = make_manifest()
        del manifest["observation_schema_version"]
        with self.clang.open("w", encoding="utf-8") as handle:
            json.dump(manifest, handle)
        self.assert_cross_fails()

    def test_current_manifest_historical_observation_pair_fails(self):
        write_manifest(
            self.clang,
            observation_schema_version="M5_SEMANTIC_OBSERVATION_V1",
            toolchain=toolchain("Clang", "18.1.3", "Linux", "x86_64"),
        )
        self.assert_cross_fails()

    def test_historical_manifest_current_observation_pair_fails(self):
        write_manifest(
            self.clang,
            schema_version="M5_SEMANTIC_MANIFEST_V1",
            observation_schema_version="M5_SEMANTIC_OBSERVATION_V2",
            toolchain=toolchain("Clang", "18.1.3", "Linux", "x86_64"),
        )
        self.assert_cross_fails()

    def test_unknown_observation_schema_fails(self):
        write_manifest(
            self.clang,
            observation_schema_version="M5_SEMANTIC_OBSERVATION_V99",
            toolchain=toolchain("Clang", "18.1.3", "Linux", "x86_64"),
        )
        self.assert_cross_fails()

    def test_non_hex_head_only_in_manifest_two_fails(self):
        write_manifest(
            self.clang,
            head_sha="z" * 40,
            toolchain=toolchain("Clang", "18.1.3", "Linux", "x86_64"),
        )
        self.assert_cross_fails()

    def test_non_hex_head_only_in_manifest_three_fails(self):
        write_manifest(
            self.apple,
            head_sha="z" * 40,
            toolchain=toolchain("AppleClang", "21", "Darwin", "arm64"),
        )
        self.assert_cross_fails()

    def test_valid_but_unexpected_head_fails(self):
        write_manifest(
            self.clang,
            head_sha="b" * 40,
            toolchain=toolchain("Clang", "18.1.3", "Linux", "x86_64"),
        )
        self.assert_cross_fails()

    def test_non_release_manifest_fails(self):
        write_manifest(
            self.gcc,
            build_type="Debug",
            toolchain=toolchain("GNU", "13.3.0", "Linux", "x86_64"),
        )
        self.assert_cross_fails()

    def test_ubuntu_clang_path_with_gnu_manifest_fails(self):
        write_manifest(
            self.clang, toolchain=toolchain("GNU", "13.3.0", "Linux", "x86_64")
        )
        self.assert_cross_fails()

    def test_duplicate_gnu_linux_role_fails(self):
        paths = [self.root / "one.json", self.root / "two.json", self.root / "three.json"]
        write_manifest(paths[0], toolchain=toolchain("GNU", "13", "Linux", "x86_64"))
        write_manifest(paths[1], toolchain=toolchain("GNU", "14", "Linux", "aarch64"))
        write_manifest(paths[2], toolchain=toolchain("AppleClang", "21", "Darwin", "arm64"))
        self.assert_cross_fails(paths)

    def test_duplicate_clang_linux_role_fails(self):
        paths = [self.root / "one.json", self.root / "two.json", self.root / "three.json"]
        write_manifest(paths[0], toolchain=toolchain("GNU", "13", "Linux", "x86_64"))
        write_manifest(paths[1], toolchain=toolchain("Clang", "18", "Linux", "x86_64"))
        write_manifest(paths[2], toolchain=toolchain("Clang", "19", "Linux", "aarch64"))
        self.assert_cross_fails(paths)

    def test_appleclang_linux_role_fails(self):
        write_manifest(
            self.apple, toolchain=toolchain("AppleClang", "21", "Linux", "arm64")
        )
        self.assert_cross_fails()

    def test_clang_darwin_cannot_substitute_for_appleclang(self):
        write_manifest(self.apple, toolchain=toolchain("Clang", "21", "Darwin", "arm64"))
        self.assert_cross_fails()

    def test_toolchain_must_be_object(self):
        write_manifest(self.clang, toolchain="Clang")
        self.assert_cross_fails()

    def test_every_toolchain_field_is_required_and_nonempty(self):
        for field in ("compiler", "compiler_version", "os", "architecture"):
            with self.subTest(field=field):
                invalid_toolchain = toolchain("Clang", "18.1.3", "Linux", "x86_64")
                invalid_toolchain.pop(field)
                write_manifest(self.clang, toolchain=invalid_toolchain)
                self.assert_cross_fails()
                self.write_valid_manifests()

    def test_all_manifests_omitting_core_usdm_fail(self):
        workloads = [copy.deepcopy(workload) for index, workload in enumerate(WORKLOADS) if index != 1]
        self._write_all_with_workloads(workloads)
        self.assert_cross_fails()

    def test_all_manifests_using_same_wrong_four_workloads_fail(self):
        workloads = copy.deepcopy(WORKLOADS)
        for index, workload in enumerate(workloads):
            workload["workload_id"] = f"wrong-{index}"
        self._write_all_with_workloads(workloads)
        self.assert_cross_fails()

    def test_all_manifests_with_extra_fifth_workload_fail(self):
        workloads = copy.deepcopy(WORKLOADS)
        workloads.append({**copy.deepcopy(WORKLOADS[0]), "workload_id": "extra-v1"})
        self._write_all_with_workloads(workloads)
        self.assert_cross_fails()

    def test_all_manifests_with_duplicate_workload_id_fail(self):
        workloads = copy.deepcopy(WORKLOADS)
        workloads[1]["workload_id"] = workloads[0]["workload_id"]
        self._write_all_with_workloads(workloads)
        self.assert_cross_fails()

    def test_wrong_workload_order_fails(self):
        workloads = copy.deepcopy(WORKLOADS)
        workloads[0], workloads[1] = workloads[1], workloads[0]
        self._write_all_with_workloads(workloads)
        self.assert_cross_fails()

    def test_workload_must_be_object_in_manifest_two(self):
        workloads = copy.deepcopy(WORKLOADS)
        workloads[1] = 7
        write_manifest(
            self.clang,
            workloads=workloads,
            toolchain=toolchain("Clang", "18.1.3", "Linux", "x86_64"),
        )
        self.assert_cross_fails()

    def test_workload_required_field_types_are_validated_in_manifest_three(self):
        fields_and_values = (
            ("workload_id", 1),
            ("fixture_id", None),
            ("fixture_hash", ["a"]),
            ("semantic_digest", 5),
        )
        for field, value in fields_and_values:
            with self.subTest(field=field):
                workloads = copy.deepcopy(WORKLOADS)
                workloads[2][field] = value
                write_manifest(
                    self.apple,
                    workloads=workloads,
                    fixture_set_id=fixture_set_id(WORKLOADS),
                    toolchain=toolchain("AppleClang", "21", "Darwin", "arm64"),
                )
                self.assert_cross_fails()
                self.write_valid_manifests()

    def test_fixture_set_id_must_match_workload_identity(self):
        write_manifest(
            self.clang,
            fixture_set_id="0" * 64,
            toolchain=toolchain("Clang", "18.1.3", "Linux", "x86_64"),
        )
        self.assert_cross_fails()

    def test_fixture_id_mismatch_fails(self):
        workloads = copy.deepcopy(WORKLOADS)
        workloads[0]["fixture_id"] = "wrong-fixture"
        write_manifest(
            self.apple,
            workloads=workloads,
            toolchain=toolchain("AppleClang", "21", "Darwin", "arm64"),
        )
        self.assert_cross_fails()

    def test_fixture_hash_mismatch_fails(self):
        workloads = copy.deepcopy(WORKLOADS)
        workloads[1]["fixture_hash"] = "0" * 64
        write_manifest(
            self.apple,
            workloads=workloads,
            toolchain=toolchain("AppleClang", "21", "Darwin", "arm64"),
        )
        self.assert_cross_fails()

    def test_semantic_digest_mismatch_fails(self):
        workloads = copy.deepcopy(WORKLOADS)
        workloads[2]["semantic_digest"] = "1" * 64
        write_manifest(
            self.apple,
            workloads=workloads,
            toolchain=toolchain("AppleClang", "21", "Darwin", "arm64"),
        )
        self.assert_cross_fails()

    def test_invalid_sha256_syntax_fails(self):
        workloads = copy.deepcopy(WORKLOADS)
        workloads[0]["semantic_digest"] = "z" * 64
        write_manifest(
            self.gcc,
            workloads=workloads,
            toolchain=toolchain("GNU", "13.3.0", "Linux", "x86_64"),
        )
        self.assert_cross_fails()

    def _write_all_with_workloads(self, workloads: list[dict]):
        write_manifest(
            self.gcc,
            workloads=workloads,
            toolchain=toolchain("GNU", "13.3.0", "Linux", "x86_64"),
        )
        write_manifest(
            self.clang,
            workloads=workloads,
            toolchain=toolchain("Clang", "18.1.3", "Linux", "x86_64"),
        )
        write_manifest(
            self.apple,
            workloads=workloads,
            toolchain=toolchain("AppleClang", "21", "Darwin", "arm64"),
        )


class ReplayEvidenceTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.paths = [
            self.root / "m5-debug-1.json",
            self.root / "m5-debug-2.json",
            self.root / "m5-release-1.json",
            self.root / "m5-release-2.json",
        ]
        self.write_valid_manifests()

    def tearDown(self):
        self.temporary.cleanup()

    def write_valid_manifests(self):
        for path, build_type in zip(
            self.paths, ("Debug", "Debug", "Release", "Release"), strict=True
        ):
            write_manifest(
                path,
                build_type=build_type,
                toolchain=toolchain("Clang", "18.1.3", "Linux", "x86_64"),
            )

    def run_replay(self):
        return run_comparator("replay", self.paths)

    def assert_replay_fails(self):
        return_code, stdout, stderr = self.run_replay()
        self.assertNotEqual(return_code, 0, f"stdout:\n{stdout}\nstderr:\n{stderr}")

    def test_valid_debug_release_evidence_passes(self):
        return_code, stdout, stderr = self.run_replay()
        self.assertEqual(return_code, 0, f"stdout:\n{stdout}\nstderr:\n{stderr}")
        self.assertIn("run #1: build_type=Debug evidence=valid", stdout)
        self.assertIn("run #4: build_type=Release evidence=valid", stdout)

    def test_fixture_set_id_mismatch_fails(self):
        write_manifest(
            self.paths[0],
            build_type="Debug",
            fixture_set_id="0" * 64,
            toolchain=toolchain("Clang", "18.1.3", "Linux", "x86_64"),
        )
        self.assert_replay_fails()

    def test_fixture_id_mismatch_fails(self):
        workloads = copy.deepcopy(WORKLOADS)
        workloads[0]["fixture_id"] = "wrong-fixture"
        self._write_run(1, "Debug", workloads=workloads)
        self.assert_replay_fails()

    def test_fixture_hash_mismatch_fails(self):
        workloads = copy.deepcopy(WORKLOADS)
        workloads[1]["fixture_hash"] = "0" * 64
        self._write_run(3, "Release", workloads=workloads)
        self.assert_replay_fails()

    def test_missing_mandatory_workload_fails(self):
        self._write_run(0, "Debug", workloads=copy.deepcopy(WORKLOADS[:3]))
        self.assert_replay_fails()

    def test_wrong_workload_fails(self):
        workloads = copy.deepcopy(WORKLOADS)
        workloads[2]["workload_id"] = "wrong-workload"
        self._write_run(0, "Debug", workloads=workloads)
        self.assert_replay_fails()

    def test_wrong_debug_release_build_type_fails(self):
        self._write_run(0, "Release")
        self.assert_replay_fails()

    def test_head_mismatch_fails(self):
        self._write_run(2, "Release", head_sha="b" * 40)
        self.assert_replay_fails()

    def test_schema_mismatch_fails(self):
        self._write_run(1, "Debug", schema_version="M5_SEMANTIC_MANIFEST_V99")
        self.assert_replay_fails()

    def test_digest_mismatch_fails(self):
        workloads = copy.deepcopy(WORKLOADS)
        workloads[3]["semantic_digest"] = "1" * 64
        self._write_run(3, "Release", workloads=workloads)
        self.assert_replay_fails()

    def test_toolchain_mismatch_fails(self):
        self._write_run(
            3,
            "Release",
            toolchain=toolchain("Clang", "19.0.0", "Linux", "x86_64"),
        )
        self.assert_replay_fails()

    def _write_run(self, index: int, build_type: str, **overrides):
        write_manifest(
            self.paths[index],
            build_type=build_type,
            toolchain=overrides.pop(
                "toolchain", toolchain("Clang", "18.1.3", "Linux", "x86_64")
            ),
            **overrides,
        )


if __name__ == "__main__":
    unittest.main()
