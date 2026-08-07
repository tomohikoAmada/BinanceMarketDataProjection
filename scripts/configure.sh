#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export CONAN_HOME="$repo_root/.cache/conan2"
export PIP_CACHE_DIR="$repo_root/.cache/pip"

if [[ $# -lt 1 ]]; then
    echo "usage: $0 {debug|release|asan|ubsan|tsan|coverage|benchmark|fuzz} [cmake args...]" >&2
    exit 2
fi

preset="$1"
shift

proto_adapter="False"
shared="False"
for cmake_arg in "$@"; do
    if [[ "$cmake_arg" == "-DBMD_PROJECTION_BUILD_PROTO_ADAPTER=ON" ]]; then
        proto_adapter="True"
    elif [[ "$cmake_arg" == "-DBUILD_SHARED_LIBS=ON" ]]; then
        shared="True"
    fi
done

case "$preset" in
    debug|asan|ubsan|tsan|coverage|fuzz)
        build_type="Debug"
        ;;
    release|benchmark)
        build_type="Release"
        ;;
    *)
        echo "unknown configure preset: $preset" >&2
        exit 2
        ;;
esac

conan="$repo_root/.venv-tools/bin/conan"
if [[ ! -x "$conan" ]]; then
    echo "Conan is not bootstrapped; run scripts/bootstrap.sh first" >&2
    exit 1
fi
if [[ ! -f "$repo_root/conan.lock" ]]; then
    echo "Conan lockfile is missing; run scripts/bootstrap.sh first" >&2
    exit 1
fi

"$conan" install "$repo_root" \
    --output-folder="$repo_root/build/$preset" \
    --lockfile="$repo_root/conan.lock" \
    --build=missing \
    -s build_type="$build_type" \
    -s compiler.cppstd=20 \
    -o "&:proto_adapter=$proto_adapter" \
    -o "&:shared=$shared"

cmake --preset "$preset" \
    -U BinanceMarketDataContracts_DIR \
    -DBMD_PROJECTION_BUILD_PROTO_ADAPTER="$proto_adapter" \
    -DBUILD_SHARED_LIBS="$shared" \
    "$@"
