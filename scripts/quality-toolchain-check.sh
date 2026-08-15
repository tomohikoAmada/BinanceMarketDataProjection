#!/usr/bin/env bash
# quality-toolchain-check.sh — deterministic canonical Quality identity check (INFRA-TC-001).
#
# Fail-closed validation of the canonical Quality toolchain against the
# repository-owned contract in .toolchain/quality.env.
#
# Asserted (fail closed):
#   - clang, clang++, clang-tidy, clang-format: exact LLVM_MAJOR.MINOR.PATCH
#   - conan: exact version pinned in requirements-tools.txt (unless --skip-conan)
#   - cmake: at least CMakePresets.json "cmakeMinimumRequired"
#   - python3: at least PYTHON_MINIMUM_VERSION from the contract
# Recorded (printed, not asserted):
#   - ninja
#
# Every failure prints expected, actual, and the resolved executable path.
# A resolved tool that is not inside the canonical installation directories is
# rejected (PATH must not select another installation).
#
# Usage: quality-toolchain-check.sh [--contract-only | --skip-conan]
#   --contract-only  validate only that the contract file is well formed
#                    (used by scripts/quality.sh before any container work)
#   --skip-conan     run without the Conan assertion (used before bootstrap)
#
# Test hooks (deterministic tests, no network):
#   BMD_QUALITY_CONTRACT_FILE   contract path override (default: repo .toolchain/quality.env)
#   BMD_QUALITY_TOOLCHAIN_DIR   fake-tool root; tools are then expected under
#                               <dir>/usr/bin and canonical-path policy applies
#                               within <dir> (mirrors /usr/bin in the container)
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

mode="full"
case "${1:-}" in
    "") ;;
    --contract-only) mode="contract-only" ;;
    --skip-conan) mode="skip-conan" ;;
    *)
        echo "usage: $0 [--contract-only | --skip-conan]" >&2
        exit 2
        ;;
esac

contract_file="${BMD_QUALITY_CONTRACT_FILE:-$repo_root/.toolchain/quality.env}"
toolchain_dir="${BMD_QUALITY_TOOLCHAIN_DIR:-}"

fail() {
    echo "QUALITY TOOLCHAIN CHECK: FAIL"
    echo "  $1"
    exit 1
}

check_failed=0
note_failure() {
    check_failed=1
    echo "  FAIL: $1"
}

# ---------------------------------------------------------------------------
# Contract file parsing
# ---------------------------------------------------------------------------
[[ -r "$contract_file" ]] || fail "contract file missing or unreadable: $contract_file"

while IFS= read -r line; do
    case "$line" in
        "" | \#*) continue ;;
    esac
    [[ "$line" =~ ^[A-Z0-9_]+=.*$ ]] \
        || fail "contract file malformed: unexpected line '${line}' in ${contract_file}"
done < "$contract_file"

for required_key in LLVM_MAJOR LLVM_MINOR LLVM_PATCH PYTHON_MINIMUM_VERSION; do
    grep -q "^${required_key}=..*$" "$contract_file" \
        || fail "contract file malformed: missing or empty key '${required_key}' in ${contract_file}"
done

contract_key() {
    # NOTE: must never fail() — output is captured by the caller
    local key="$1"
    sed -n "s/^${key}=//p" "$contract_file" | head -n1
}

llvm_major="$(contract_key LLVM_MAJOR)"
llvm_minor="$(contract_key LLVM_MINOR)"
llvm_patch="$(contract_key LLVM_PATCH)"
for v in "$llvm_major" "$llvm_minor" "$llvm_patch"; do
    [[ "$v" =~ ^[0-9]+$ ]] || fail "contract file malformed: non-numeric LLVM version component '${v}'"
done
llvm_version="$llvm_major.$llvm_minor.$llvm_patch"
python_minimum="$(contract_key PYTHON_MINIMUM_VERSION)"
[[ "$python_minimum" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] \
    || fail "contract file malformed: non-numeric PYTHON_MINIMUM_VERSION '${python_minimum}'"

if [[ "$mode" == "contract-only" ]]; then
    echo "QUALITY TOOLCHAIN CHECK: PASS (contract well formed: $contract_file)"
    exit 0
fi

# ---------------------------------------------------------------------------
# CMake floor from CMakePresets.json (cmakeMinimumRequired)
# ---------------------------------------------------------------------------
cmake_min_lines="$(grep -A3 '"cmakeMinimumRequired"' "$repo_root/CMakePresets.json")"
cmake_min_major="$(sed -n 's/.*"major": *\([0-9][0-9]*\).*/\1/p' <<<"$cmake_min_lines" | head -n1)"
cmake_min_minor="$(sed -n 's/.*"minor": *\([0-9][0-9]*\).*/\1/p' <<<"$cmake_min_lines" | head -n1)"
cmake_min_patch="$(sed -n 's/.*"patch": *\([0-9][0-9]*\).*/\1/p' <<<"$cmake_min_lines" | head -n1)"
if [[ -z "$cmake_min_major" || -z "$cmake_min_minor" || -z "$cmake_min_patch" ]]; then
    fail "cannot read cmakeMinimumRequired from $repo_root/CMakePresets.json"
fi

# ---------------------------------------------------------------------------
# Tool discovery and version parsing
# ---------------------------------------------------------------------------
resolve_tool() {
    # PATH-based resolution only; the canonical-path policy rejects
    # installations outside the canonical directories.
    local name="$1"
    if command -v "$name" >/dev/null 2>&1; then
        command -v "$name"
    fi
}

version_of() {
    # first "X.Y.Z" triplet on the first --version line
    local first_line="$1"
    local parsed
    parsed="$(printf '%s\n' "$first_line" | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -n1)"
    printf '%s' "$parsed"
}

resolve_real() {
    # real path of an existing executable (BSD/GNU portable; no -m)
    local p="$1"
    if [[ -e "$p" ]]; then
        realpath "$p"
    else
        printf '%s' "$p"
    fi
}

is_canonical_path() {
    local real_path="$1"
    local root
    if [[ -n "$toolchain_dir" ]]; then
        root="$toolchain_dir"
    else
        root="/"
    fi
    root="${root%/}"
    case "$real_path" in
        "$root/usr/bin/"*) return 0 ;;
        "$root/usr/lib/llvm-"*/bin/*) return 0 ;;
    esac
    return 1
}

check_llvm_tool() {
    local name="$1"
    local expected="$2"
    local resolved
    resolved="$(resolve_tool "$name")"
    if [[ -z "$resolved" || ! -x "$resolved" ]]; then
        note_failure "$name: missing (expected $expected)"
        return
    fi
    local real_path
    real_path="$(resolve_real "$resolved")"
    local first_line actual
    first_line="$("$resolved" --version 2>&1 | head -n1)"
    if [[ "$first_line" == *"Apple clang"* ]]; then
        note_failure "$name: AppleClang detected; canonical Quality requires upstream LLVM Clang (expected $expected, resolved $resolved)"
        return
    fi
    actual="$(version_of "$first_line")"
    if [[ -z "$actual" ]]; then
        note_failure "$name: cannot parse version from '${first_line}' (expected $expected, resolved $resolved)"
        return
    fi
    if [[ "$actual" != "$expected" ]]; then
        note_failure "$name: wrong version (expected $expected, actual $actual, resolved $resolved)"
        return
    fi
    if ! is_canonical_path "$real_path"; then
        note_failure "$name: resolved outside canonical installation (actual $actual, resolved $resolved, real $real_path)"
        return
    fi
    echo "  OK   $name: $actual ($real_path)"
}

ver_ge() {
    # version_ge A B : A >= B for dotted triples
    local -a a b
    IFS=. read -r -a a <<< "$1"
    IFS=. read -r -a b <<< "$2"
    for i in 0 1 2; do
        if (( a[i] > b[i] )); then return 0; fi
        if (( a[i] < b[i] )); then return 1; fi
    done
    return 0
}

check_floor_tool() {
    # name expected_min label — assert found and >= expected_min
    local name="$1" expected="$2" label="$3"
    local resolved actual first_line
    resolved="$(resolve_tool "$name")"
    if [[ -z "$resolved" || ! -x "$resolved" ]]; then
        note_failure "$name: missing (expected >= $expected)"
        return
    fi
    first_line="$("$resolved" --version 2>&1 | head -n1)"
    actual="$(version_of "$first_line")"
    if [[ -z "$actual" ]]; then
        note_failure "$name: cannot parse version from '${first_line}' (expected >= $expected, resolved $resolved)"
        return
    fi
    if ! ver_ge "$actual" "$expected"; then
        note_failure "$name: too old (expected >= $expected, actual $actual, resolved $resolved)"
        return
    fi
    echo "  OK   $name: $actual >= $expected (${label})"
}

record_tool() {
    local name="$1"
    local resolved actual first_line
    resolved="$(resolve_tool "$name")"
    if [[ -z "$resolved" || ! -x "$resolved" ]]; then
        echo "  NOTE $name: not found (recorded only)"
        return
    fi
    first_line="$("$resolved" --version 2>&1 | head -n1)"
    actual="$(version_of "$first_line")"
    echo "  NOTE $name: ${actual:-unparseable} ($(resolve_real "$resolved"))"
}

check_conan() {
    local expected
    expected="$(sed -n 's/^conan==//p' "$repo_root/requirements-tools.txt" | head -n1)"
    if [[ -z "$expected" ]]; then
        fail "cannot read Conan pin from $repo_root/requirements-tools.txt"
    fi
    local resolved actual first_line
    resolved="$(resolve_tool conan)"
    if [[ -z "$resolved" || ! -x "$resolved" ]]; then
        note_failure "conan: missing (expected $expected from requirements-tools.txt)"
        return
    fi
    first_line="$("$resolved" --version 2>&1 | head -n1)"
    actual="$(version_of "$first_line")"
    if [[ "$actual" != "$expected" ]]; then
        note_failure "conan: wrong version (expected $expected from requirements-tools.txt, actual $actual, resolved $resolved)"
        return
    fi
    echo "  OK   conan: $actual (requirements-tools.txt pin, $resolved)"
}

# ---------------------------------------------------------------------------
# Run the checks
# ---------------------------------------------------------------------------
echo "QUALITY TOOLCHAIN CHECK (contract: $contract_file)"
echo "  canonical identity: clang/clang++/clang-tidy/clang-format $llvm_version"

for tool in clang clang++ clang-tidy clang-format; do
    check_llvm_tool "$tool" "$llvm_version"
done
check_floor_tool cmake "$cmake_min_major.$cmake_min_minor.$cmake_min_patch" "CMakePresets.json cmakeMinimumRequired"
check_floor_tool python3 "$python_minimum" ".toolchain/quality.env PYTHON_MINIMUM_VERSION"
record_tool ninja
if [[ "$mode" != "skip-conan" ]]; then
    check_conan
fi

if [[ "$check_failed" -ne 0 ]]; then
    echo "QUALITY TOOLCHAIN CHECK: FAIL"
    exit 1
fi
echo "QUALITY TOOLCHAIN CHECK: PASS"
