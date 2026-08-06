#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
require_fuzzers="${BMD_PROJECTION_REQUIRE_FUZZERS:-0}"
fuzz_runs="${BMD_PROJECTION_FUZZ_RUNS:-10000}"
decimal_fuzz_runs="${BMD_PROJECTION_DECIMAL_FUZZ_RUNS:-$fuzz_runs}"
order_book_fuzz_runs="${BMD_PROJECTION_ORDER_BOOK_FUZZ_RUNS:-$fuzz_runs}"

skip_or_fail() {
    local reason="$1"
    if [[ "$require_fuzzers" == "1" ]]; then
        echo "fuzz smoke required but unavailable: $reason" >&2
        exit 1
    fi
    echo "fuzz smoke: SKIPPED ($reason)"
    exit 0
}

cxx_compiler="${CXX:-}"
if [[ -z "$cxx_compiler" ]]; then
    cxx_compiler="$(command -v clang++ || true)"
fi
[[ -n "$cxx_compiler" ]] || skip_or_fail "clang++ not found"

compiler_version="$($cxx_compiler --version 2>&1 || true)"
[[ "$compiler_version" == *"clang"* ]] || skip_or_fail "C++ compiler is not Clang"
[[ "$compiler_version" != *"Apple clang"* ]] || skip_or_fail "AppleClang is not a libFuzzer CI target"

c_compiler="${CC:-}"
if [[ -z "$c_compiler" ]]; then
    c_compiler="$(command -v clang || true)"
fi
[[ -n "$c_compiler" ]] || skip_or_fail "clang not found"

if [[ ! -x "$repo_root/.venv-tools/bin/conan" ]]; then
    echo "Conan is not bootstrapped; run scripts/bootstrap.sh first" >&2
    exit 1
fi

cd "$repo_root"
CC="$c_compiler" CXX="$cxx_compiler" scripts/configure.sh fuzz \
    -DBMD_PROJECTION_ENABLE_WERROR=ON

cmake --build --preset fuzz --target bmd_projection_decimal_parser_fuzz
cmake --build --preset fuzz --target bmd_projection_order_book_fuzz

echo "Running decimal parser fuzz: $decimal_fuzz_runs inputs"
"$repo_root/build/fuzz/cmake/fuzz/bmd_projection_decimal_parser_fuzz" \
    "$repo_root/fuzz/corpus/decimal_parser" \
    -runs="$decimal_fuzz_runs" \
    -timeout=5

echo "Running order book fuzz: $order_book_fuzz_runs inputs"
"$repo_root/build/fuzz/cmake/fuzz/bmd_projection_order_book_fuzz" \
    "$repo_root/fuzz/corpus/order_book" \
    -runs="$order_book_fuzz_runs" \
    -timeout=5

echo "fuzz smoke: PASS"
