#!/usr/bin/env bash
# formal-build-contract-check.sh — Phase-7 formal build contract proof.
#
# After the internal runner configures the formal build, this helper proves
# from the CMake build tree itself that the configuration satisfies the
# accepted formal build contract (OD-M5-P7-017, OD-M5-P7-023):
#
#   - the CMake source root is exactly /src (CMAKE_HOME_DIRECTORY), so the
#     single formal source model is structurally enforced in the build tree;
#   - CMAKE_BUILD_TYPE is exactly Release;
#   - every sanitizer option is explicitly OFF (ASan/UBSan/TSan/coverage);
#   - LTO state is explicitly recorded as OFF (CMAKE_INTERPROCEDURAL_OPTIMIZATION);
#   - benchmarks ON, ProtoAdapter ON (the full M4 inventory must exist),
#     tests OFF.
#
# Fail closed on any missing or unexpected cache entry: the formal runner
# never relies on an implicit default for these identities.
#
# usage: formal-build-contract-check.sh <build-dir>
set -euo pipefail

fail() {
    echo "FORMAL FAIL: $1" >&2
    exit 1
}

if [[ $# -ne 1 ]]; then
    echo "usage: $0 <build-dir>" >&2
    exit 2
fi

cache="$1/CMakeCache.txt"
[[ -r "$cache" ]] || fail "formal build cache missing or unreadable: $cache"

require_line() {
    local pattern="$1" description="$2"
    if ! grep -qx "$pattern" "$cache"; then
        fail "formal build contract violation: $description (expected cache entry '$pattern')"
    fi
}

# The explicit -D LTO setting lands in the cache with the UNINITIALIZED type
# (command-line cache entries); the value is what establishes the explicit
# recorded state. Require the entry to exist with exactly OFF.
require_lto_off() {
    if ! grep -qE '^CMAKE_INTERPROCEDURAL_OPTIMIZATION:(UNINITIALIZED|BOOL)=OFF$' "$cache"; then
        fail "formal build contract violation: LTO state must be explicitly recorded (OFF)"
    fi
}

require_line 'CMAKE_HOME_DIRECTORY:INTERNAL=/src' \
    "CMake source root must be exactly /src"
require_line 'CMAKE_BUILD_TYPE:STRING=Release' \
    "build type must be exactly Release"
require_line 'BMD_PROJECTION_ENABLE_ASAN:BOOL=OFF' \
    "ASan must be explicitly OFF"
require_line 'BMD_PROJECTION_ENABLE_UBSAN:BOOL=OFF' \
    "UBSan must be explicitly OFF"
require_line 'BMD_PROJECTION_ENABLE_TSAN:BOOL=OFF' \
    "TSan must be explicitly OFF"
require_line 'BMD_PROJECTION_ENABLE_COVERAGE:BOOL=OFF' \
    "coverage must be explicitly OFF"
require_lto_off
require_line 'BMD_PROJECTION_BUILD_BENCHMARKS:BOOL=ON' \
    "benchmarks must be ON (measurement executables must exist)"
# The accepted repository benchmark build records the adapter option as a
# STRING cache entry; the formal build mirrors that exact shape.
require_line 'BMD_PROJECTION_BUILD_PROTO_ADAPTER:STRING=ON' \
    "ProtoAdapter must be ON (the M4 inventory is required formal evidence)"
require_line 'BMD_PROJECTION_BUILD_TESTS:BOOL=OFF' \
    "tests must be OFF for the formal measurement build"

echo "formal-build-contract-check: PASS (Release, sanitizers off, explicit LTO off, "
echo "  CMake source root /src, benchmarks ON, ProtoAdapter ON)"
