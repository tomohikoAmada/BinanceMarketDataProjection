#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
work_parent="$repo_root/build"
mkdir -p "$work_parent"
work_dir="$(mktemp -d "$work_parent/install-adapter-consumer.XXXXXX")"
export CONAN_HOME="$repo_root/.cache/conan2"

cleanup() {
    case "$work_dir" in
        "$repo_root"/build/install-adapter-consumer.*) rm -rf "$work_dir" ;;
        *) echo "refusing to remove unexpected path: $work_dir" >&2 ;;
    esac
}
trap cleanup EXIT

stage_prefix="$work_dir/stage"
consumer_deps="$work_dir/consumer-deps"
consumer_build="$work_dir/consumer-build"
shared="${BMD_PROJECTION_SHARED:-0}"
shared_cmake="OFF"
shared_conan="False"
if [[ "$shared" == "1" ]]; then
    shared_cmake="ON"
    shared_conan="True"
fi

"$repo_root/scripts/bootstrap-contracts.sh"
"$repo_root/scripts/configure.sh" release \
    -DBMD_PROJECTION_BUILD_PROTO_ADAPTER=ON \
    -DBUILD_SHARED_LIBS="$shared_cmake"
"$repo_root/scripts/build.sh" release
cmake --install "$repo_root/build/release/cmake" --prefix "$stage_prefix"

"$repo_root/.venv-tools/bin/conan" install "$repo_root/tests/downstream_adapter" \
    --output-folder="$consumer_deps" \
    --lockfile="$repo_root/conan.lock" \
    --build=missing \
    -s build_type=Release \
    -s compiler.cppstd=20 \
    -o "binance-market-data-contracts-cpp/*:shared=$shared_conan"

cmake \
    -S "$repo_root/tests/downstream_adapter" \
    -B "$consumer_build" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$consumer_deps/conan_toolchain.cmake" \
    -DCMAKE_PREFIX_PATH="$stage_prefix"
cmake --build "$consumer_build"
"$consumer_build/proto_adapter_consumer"

symbol='ExchangeDepthSnapshotD0Ev$'
definition_count="$(nm -g "$consumer_build/proto_adapter_consumer" 2>/dev/null \
    | awk '$2 ~ /^[TDBS]$/ {print $0}' \
    | grep -Ec "$symbol" || true)"
if [[ "$definition_count" == "0" && "$shared" == "1" ]]; then
    contracts_config_dir="$(sed -n \
        's/^set(BinanceMarketDataContracts_DIR "\([^"]*\)".*$/\1/p' \
        "$consumer_deps/conan_toolchain.cmake")"
    if [[ -z "$contracts_config_dir" || ! -d "$contracts_config_dir" ]]; then
        echo "could not resolve the Contracts package prefix from the consumer toolchain" >&2
        exit 1
    fi
    contracts_prefix="$(cd "$contracts_config_dir/../../.." && pwd)"
    definition_count="$(find "$contracts_prefix/lib" -type f \
        \( -name '*contracts*.dylib' -o -name '*contracts*.so' -o -name '*contracts*.so.*' \) \
        -exec nm -g {} \; 2>/dev/null \
        | awk '$2 ~ /^[TDBS]$/ {print $0}' \
        | grep -Ec "$symbol" || true)"
fi
if [[ "$definition_count" != "1" ]]; then
    echo "expected one Contracts-generated symbol owner, found $definition_count" >&2
    exit 1
fi
