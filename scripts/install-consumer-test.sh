#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
work_parent="$repo_root/build"
mkdir -p "$work_parent"
work_dir="$(mktemp -d "$work_parent/install-consumer.XXXXXX")"

cleanup() {
    case "$work_dir" in
        "$repo_root"/build/install-consumer.*) rm -rf "$work_dir" ;;
        *) echo "refusing to remove unexpected path: $work_dir" >&2 ;;
    esac
}
trap cleanup EXIT

stage_prefix="$work_dir/stage"
consumer_build="$work_dir/consumer-build"
missing_adapter_build="$work_dir/missing-adapter-build"

"$repo_root/scripts/configure.sh" release
"$repo_root/scripts/build.sh" release
cmake --install "$repo_root/build/release/cmake" --prefix "$stage_prefix"

cmake \
    -S "$repo_root/tests/downstream" \
    -B "$consumer_build" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$stage_prefix"
cmake --build "$consumer_build"
"$consumer_build/consumer"

if [[ -e "$stage_prefix/include/binance_market_data/projection_adapter" ]]; then
    echo "Core-only installation unexpectedly contains the ProtoAdapter public header" >&2
    exit 1
fi
if cmake \
    -S "$repo_root/tests/downstream_adapter_unavailable" \
    -B "$missing_adapter_build" \
    -G Ninja \
    -DCMAKE_PREFIX_PATH="$stage_prefix"; then
    echo "Core-only installation unexpectedly satisfied the ProtoAdapter component" >&2
    exit 1
fi
