import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


REPO_ROOT = Path(__file__).parents[1]
BOOTSTRAP_SCRIPT = REPO_ROOT / "scripts" / "bootstrap-contracts.sh"
CONFIGURE_SCRIPT = REPO_ROOT / "scripts" / "configure.sh"
CONTRACTS_REVISION = "9c34199f42467b92a4380c6e88617dde0e54ae13"


class ContractsProvenanceTest(unittest.TestCase):
    def test_bootstrap_exports_pinned_contracts_revision_to_conan(self) -> None:
        source = BOOTSTRAP_SCRIPT.read_text(encoding="utf-8")
        revision = f'contracts_revision="{CONTRACTS_REVISION}"'
        exported = 'export BMD_CONTRACTS_SOURCE_REVISION="$contracts_revision"'

        self.assertIn(revision, source)
        self.assertIn(exported, source)
        self.assertLess(source.index(exported), source.index('export_output="$($conan export'))

    def test_configure_exports_pinned_revision_to_conan_process(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            repo = Path(temporary_directory)
            scripts = repo / "scripts"
            conan_bin = repo / ".venv-tools" / "bin"
            fake_bin = repo / "fake-bin"
            scripts.mkdir(parents=True)
            conan_bin.mkdir(parents=True)
            fake_bin.mkdir(parents=True)

            shutil.copy2(BOOTSTRAP_SCRIPT, scripts / "bootstrap-contracts.sh")
            shutil.copy2(CONFIGURE_SCRIPT, scripts / "configure.sh")
            (repo / "conan.lock").write_text("{}\n", encoding="utf-8")

            capture = repo / "captured-revision.txt"
            fake_conan = conan_bin / "conan"
            fake_conan.write_text(
                "#!/usr/bin/env bash\n"
                "printf '%s\\n' \"${BMD_CONTRACTS_SOURCE_REVISION-<unset>}\" "
                "> \"$BMD_TEST_CAPTURE\"\n",
                encoding="utf-8",
            )
            fake_conan.chmod(0o755)

            fake_cmake = fake_bin / "cmake"
            fake_cmake.write_text("#!/usr/bin/env bash\nexit 0\n", encoding="utf-8")
            fake_cmake.chmod(0o755)

            environment = os.environ.copy()
            environment.pop("BMD_CONTRACTS_SOURCE_REVISION", None)
            environment["BMD_TEST_CAPTURE"] = str(capture)
            environment["PATH"] = f"{fake_bin}{os.pathsep}{environment['PATH']}"
            result = subprocess.run(
                [
                    "bash",
                    str(scripts / "configure.sh"),
                    "debug",
                    "-DBMD_PROJECTION_BUILD_PROTO_ADAPTER=ON",
                ],
                cwd=repo,
                env=environment,
                capture_output=True,
                text=True,
                check=False,
            )

            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(capture.read_text(encoding="utf-8").strip(), CONTRACTS_REVISION)


if __name__ == "__main__":
    unittest.main()
