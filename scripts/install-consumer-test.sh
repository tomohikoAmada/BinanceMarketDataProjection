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
