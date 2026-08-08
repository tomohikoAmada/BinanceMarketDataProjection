#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
contracts_revision="67ee1bf69fad980d114cfa278c3a6ffe310a4d7a"
contracts_rrev="7fd3efe3d289462fb16c78ffeced1682"
contracts_dir="$repo_root/build/m4-prerequisite/contracts"
export CONAN_HOME="$repo_root/.cache/conan2"
export PIP_CACHE_DIR="$repo_root/.cache/pip"

conan="$repo_root/.venv-tools/bin/conan"
if [[ ! -x "$conan" ]]; then
    echo "Conan is not bootstrapped; run scripts/bootstrap.sh first" >&2
    exit 1
fi

"$repo_root/.venv-tools/bin/python" -m pip install \
    --disable-pip-version-check \
    --requirement "$repo_root/requirements-m4.txt"

mkdir -p "$repo_root/build/m4-prerequisite"
if [[ ! -d "$contracts_dir/.git" ]]; then
    git clone --no-checkout \
        https://github.com/tomohikoAmada/BinanceMarketDataContracts.git \
        "$contracts_dir"
fi
git -C "$contracts_dir" fetch origin "$contracts_revision"
git -C "$contracts_dir" checkout --detach "$contracts_revision"

actual_revision="$(git -C "$contracts_dir" rev-parse HEAD)"
if [[ "$actual_revision" != "$contracts_revision" ]]; then
    echo "Contracts source revision mismatch: $actual_revision" >&2
    exit 1
fi
if [[ -n "$(git -C "$contracts_dir" status --short)" ]]; then
    echo "Contracts prerequisite checkout is dirty" >&2
    exit 1
fi

export_output="$($conan export "$contracts_dir" 2>&1)"
echo "$export_output"
if [[ "$export_output" != *"binance-market-data-contracts-cpp/0.1.0#$contracts_rrev"* ]]; then
    echo "Contracts Conan recipe revision mismatch" >&2
    exit 1
fi

package_json="$repo_root/build/m4-prerequisite/contracts-package.json"
"$conan" list \
    "binance-market-data-contracts-cpp/0.1.0#$contracts_rrev:*" \
    --format=json > "$package_json"
if ! "$repo_root/.venv-tools/bin/python" "$repo_root/scripts/verify-contracts-package.py" \
    "$conan" "$package_json"; then
    "$conan" create "$contracts_dir" \
        --lockfile="$contracts_dir/conan.lock" \
        --build=missing \
        -s build_type=Release \
        -s compiler.cppstd=20
    "$conan" list \
        "binance-market-data-contracts-cpp/0.1.0#$contracts_rrev:*" \
        --format=json > "$package_json"
    "$repo_root/.venv-tools/bin/python" "$repo_root/scripts/verify-contracts-package.py" \
        "$conan" "$package_json"
fi

echo "Pinned Contracts package ready: binance-market-data-contracts-cpp/0.1.0#$contracts_rrev"
