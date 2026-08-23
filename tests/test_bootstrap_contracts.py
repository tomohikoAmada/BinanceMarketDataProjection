from pathlib import Path


SCRIPT = Path(__file__).parents[1] / "scripts" / "bootstrap-contracts.sh"


def test_bootstrap_exports_pinned_contracts_revision_to_conan() -> None:
    source = SCRIPT.read_text(encoding="utf-8")
    revision = 'contracts_revision="9c34199f42467b92a4380c6e88617dde0e54ae13"'
    exported = 'export BMD_CONTRACTS_SOURCE_REVISION="$contracts_revision"'

    assert revision in source
    assert exported in source
    assert source.index(exported) < source.index('export_output="$($conan export')
