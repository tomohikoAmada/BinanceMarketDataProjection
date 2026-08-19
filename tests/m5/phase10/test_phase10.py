from __future__ import annotations

import io
import json
import tarfile
import tempfile
import unittest
from dataclasses import replace
from pathlib import Path

from scripts import m5_phase10


class Phase10VerifierTests(unittest.TestCase):
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
