#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

clang_format="${CLANG_FORMAT:-}"
if [[ -z "$clang_format" ]] && command -v clang-format >/dev/null 2>&1; then
    clang_format="$(command -v clang-format)"
elif [[ -z "$clang_format" ]] && xcrun --find clang-format >/dev/null 2>&1; then
    clang_format="$(xcrun --find clang-format)"
fi
if [[ -z "$clang_format" || ! -x "$clang_format" ]]; then
    echo "clang-format is required for the complete quality gate" >&2
    exit 1
fi

BMD_PROJECTION_REQUIRE_CLANG_TIDY="${BMD_PROJECTION_REQUIRE_CLANG_TIDY:-0}"

clang_tidy="${CLANG_TIDY:-}"
if [[ -z "$clang_tidy" ]] && command -v clang-tidy >/dev/null 2>&1; then
    clang_tidy="$(command -v clang-tidy)"
elif [[ -z "$clang_tidy" ]] && xcrun --find clang-tidy >/dev/null 2>&1; then
    clang_tidy="$(xcrun --find clang-tidy)"
fi

if [[ -n "$clang_tidy" && -x "$clang_tidy" ]]; then
    echo "clang-tidy: ENABLED ($clang_tidy)"
    tidy_flag_enable="-DBMD_PROJECTION_ENABLE_CLANG_TIDY=ON"
    tidy_flag_exe="-DBMD_PROJECTION_CLANG_TIDY_EXECUTABLE=$clang_tidy"
elif [[ "$BMD_PROJECTION_REQUIRE_CLANG_TIDY" == "1" ]]; then
    echo "clang-tidy is required but could not be found" >&2
    exit 1
else
    echo "clang-tidy: SKIPPED locally; mandatory CI clang-tidy remains authoritative"
    tidy_flag_enable=""
    tidy_flag_exe=""
fi

find include src tests benchmarks fuzz -type f \( -name '*.cpp' -o -name '*.hpp' \) -print0 \
    | xargs -0 "$clang_format" --dry-run --Werror

scripts/configure.sh debug \
    -DBMD_PROJECTION_ENABLE_WERROR=ON \
    ${tidy_flag_enable:+"$tidy_flag_enable"} \
    ${tidy_flag_exe:+"$tidy_flag_exe"}
scripts/build.sh debug
scripts/test.sh debug

for preset in release asan ubsan tsan coverage; do
    scripts/configure.sh "$preset" -DBMD_PROJECTION_ENABLE_WERROR=ON
    scripts/build.sh "$preset"
    scripts/test.sh "$preset"
done

scripts/bootstrap-contracts.sh
scripts/configure.sh benchmark \
    -DBMD_PROJECTION_BUILD_PROTO_ADAPTER=ON \
    -DBMD_PROJECTION_ENABLE_WERROR=ON
scripts/build.sh benchmark
BMD_PROJECTION_SMOKE_MODE="${BMD_PROJECTION_SMOKE_MODE:-quick}" \
    scripts/benchmark-smoke.sh

scripts/install-consumer-test.sh
for preset in debug release asan ubsan tsan; do
    scripts/configure.sh "$preset" \
        -DBMD_PROJECTION_BUILD_PROTO_ADAPTER=ON \
        -DBMD_PROJECTION_ENABLE_WERROR=ON
    scripts/build.sh "$preset"
    scripts/test.sh "$preset"
done
scripts/install-adapter-consumer-test.sh
BMD_PROJECTION_SHARED=1 scripts/install-adapter-consumer-test.sh
scripts/fuzz-smoke.sh
git diff --check
