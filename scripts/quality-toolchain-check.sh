#!/usr/bin/env bash
# quality-toolchain-check.sh — deterministic canonical Quality identity check (INFRA-TC-001).
#
# Fail-closed validation of the canonical Quality toolchain against the
# repository-owned contract in .toolchain/quality.env.
#
# Asserted (fail closed):
#   - clang, clang++, clang-tidy, clang-format: exact LLVM_MAJOR.MINOR.PATCH
#     AND Ubuntu package provenance (owning package name and installed
#     package version from dpkg metadata must match the contract pins) AND
#     installed payload integrity (the real executable's md5 must match the
#     owning package's recorded md5sum — a modified payload is detected even
#     when dpkg metadata and --version text are unchanged)
#   - conan: exact version pinned in requirements-tools.txt (unless --skip-conan)
#   - cmake: at least CMakePresets.json "cmakeMinimumRequired"
#   - python3: at least PYTHON_MINIMUM_VERSION from the contract
#   - UBUNTU_SNAPSHOT_ID: present, YYYYMMDDTHHMMSSZ-formatted, a real UTC
#     calendar moment (20260230T120000Z is rejected), and NOT in the future
#     relative to the reference UTC time (future snapshot IDs fail before
#     image construction)
#   - the contract is a valid KEY=VALUE file: duplicate assignment of any key
#     is malformed and FAILS CLOSED (there is no first-wins or last-wins
#     interpretation; the file is rejected before any value is consumed)
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
# Test hooks (deterministic tests only, no network; scripts/quality.sh REJECTS
# all of them in its environment, so they can never influence canonical
# acceptance):
#   BMD_QUALITY_CONTRACT_FILE   contract path override (default: repo .toolchain/quality.env)
#   BMD_QUALITY_TOOLCHAIN_DIR   fake-tool root; tools are then expected under
#                               <dir>/usr/bin and canonical-path policy applies
#                               within <dir> (mirrors /usr/bin in the container)
#   BMD_QUALITY_REFERENCE_TIME  fixed UTC reference for snapshot temporal
#                               validation (default: current UTC clock)
#   BMD_QUALITY_DPKG_INFO_DIR   dpkg md5sums directory override (default:
#                               /var/lib/dpkg/info) for payload checks
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

for required_key in LLVM_MAJOR LLVM_MINOR LLVM_PATCH PYTHON_MINIMUM_VERSION UBUNTU_SNAPSHOT_ID; do
    grep -q "^${required_key}=..*$" "$contract_file" \
        || fail "contract file malformed: missing or empty key '${required_key}' in ${contract_file}"
done

# Duplicate assignment of ANY key is malformed. The repository parser
# (first occurrence) and the shell sourcing used by the Dockerfile/container
# (last occurrence) must never be able to interpret one file differently:
# the file is rejected before any canonical value is consumed.
duplicate_keys="$(awk -F= '/^[A-Z0-9_]+=/ { key=$1; if (seen[key]++) print key }' "$contract_file")"
if [[ -n "$duplicate_keys" ]]; then
    fail "contract file malformed: duplicate key assignment(s): $(printf '%s ' $duplicate_keys)in ${contract_file}"
fi

contract_key() {
    # NOTE: must never fail() — output is captured by the caller.
    # Safe first-occurrence read: duplicate keys are rejected above, so the
    # file holds at most one assignment per key.
    local key="$1"
    sed -n "s/^${key}=//p" "$contract_file" | head -n1
}

utc_calendar_valid() {
    # $1 = YYYYMMDDTHHMMSSZ; returns 0 iff a real UTC calendar moment
    # (leap-year aware; century years need %400). No Python or host extras;
    # pure POSIX-Bash arithmetic with explicit 10# base (leading zeros must
    # never be parsed as octal).
    local ts="$1" y m d h mi s dim
    [[ "$ts" =~ ^[0-9]{8}T[0-9]{6}Z$ ]] || return 1
    y="${ts:0:4}"; m="${ts:4:2}"; d="${ts:6:2}"
    h="${ts:9:2}"; mi="${ts:11:2}"; s="${ts:13:2}"
    (( 10#$y >= 1 && 10#$y <= 9999 )) || return 1
    (( 10#$m >= 1 && 10#$m <= 12 )) || return 1
    (( 10#$h <= 23 && 10#$mi <= 59 && 10#$s <= 59 )) || return 1
    case "$m" in
        01|03|05|07|08|10|12) dim=31 ;;
        04|06|09|11) dim=30 ;;
        02)
            if (( 10#$y % 4 != 0 )); then dim=28
            elif (( 10#$y % 100 != 0 )); then dim=29
            elif (( 10#$y % 400 != 0 )); then dim=28
            else dim=29
            fi
            ;;
    esac
    (( 10#$d >= 1 && 10#$d <= dim )) || return 1
    return 0
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
snapshot_id="$(contract_key UBUNTU_SNAPSHOT_ID)"
[[ "$snapshot_id" =~ ^[0-9]{8}T[0-9]{6}Z$ ]] \
    || fail "contract file malformed: UBUNTU_SNAPSHOT_ID '${snapshot_id}' not in YYYYMMDDTHHMMSSZ format"
utc_calendar_valid "$snapshot_id" \
    || fail "contract file malformed: UBUNTU_SNAPSHOT_ID '${snapshot_id}' is not a valid UTC calendar date/time"
reference_time="${BMD_QUALITY_REFERENCE_TIME:-$(date -u +%Y%m%dT%H%M%SZ)}"
[[ "$reference_time" =~ ^[0-9]{8}T[0-9]{6}Z$ ]] \
    || fail "reference time invalid: '${reference_time}' (expected YYYYMMDDTHHMMSSZ)"
utc_calendar_valid "$reference_time" \
    || fail "reference time invalid: '${reference_time}' is not a valid UTC calendar date/time"
if [[ "$snapshot_id" > "$reference_time" ]]; then
    fail "contract file: UBUNTU_SNAPSHOT_ID '${snapshot_id}' is in the future (reference UTC ${reference_time}); a historical snapshot is required"
fi

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

package_pin_version() {
    # version part of a "<pkg>=<version>" contract pin (e.g. 1:18.1.3-1ubuntu1)
    local key="$1"
    local value
    value="$(contract_key "$key")"
    printf '%s' "${value#*=}"
}

check_package_identity() {
    # Binds the resolved real executable to the expected Ubuntu package
    # installation: owning package name and installed package version must
    # match the contract exactly. Correct text output by an arbitrary
    # replacement binary is NOT sufficient by itself.
    local name="$1" real_path="$2" expected_pkg="$3" expected_version="$4"
    local dpkg_bin owned_line owned_pkg version_line actual_version
    dpkg_bin="$(resolve_tool dpkg)"
    if [[ -z "$dpkg_bin" || ! -x "$dpkg_bin" ]]; then
        note_failure "$name: dpkg not found; cannot verify package provenance"
        return
    fi
    owned_line="$("$dpkg_bin" -S "$real_path" 2>&1 | head -n1)" || true
    owned_pkg="$(sed -n 's/^\([^:]*\): .*/\1/p' <<<"$owned_line")"
    if [[ -z "$owned_pkg" ]]; then
        note_failure "$name: real executable not owned by any installed package (real $real_path)"
        return
    fi
    if [[ "$owned_pkg" != "$expected_pkg" ]]; then
        note_failure "$name: unexpected package owner (expected $expected_pkg, actual $owned_pkg, real $real_path)"
        return
    fi
    version_line="$("$dpkg_bin" -s "$owned_pkg" 2>&1 | grep -E '^Version:' | head -n1)" || true
    actual_version="$(sed -n 's/^Version: //p' <<<"$version_line")"
    if [[ -z "$actual_version" ]]; then
        note_failure "$name: cannot read installed version of $owned_pkg"
        return
    fi
    if [[ "$actual_version" != "$expected_version" ]]; then
        note_failure "$name: package $owned_pkg version mismatch (expected $expected_version, actual $actual_version)"
        return
    fi
    echo "  OK   $name: package $owned_pkg $actual_version ($real_path)"
}

md5_of() {
    # portable md5 of a file (GNU md5sum; BSD md5)
    if command -v md5sum >/dev/null 2>&1; then
        md5sum "$1"
    elif command -v md5 >/dev/null 2>&1; then
        md5 -q "$1"
    else
        echo "no md5 tool found" >&2
        exit 1
    fi
}

check_payload_integrity() {
    # Installed-package payload verification: the real executable's md5 must
    # match the md5 recorded by the owning package in its md5sums database.
    # A payload modified while the dpkg database stays unchanged (and while
    # --version output still matches) is detected here.
    local name="$1" real_path="$2" pkg="$3"
    local info_dir
    info_dir="${BMD_QUALITY_DPKG_INFO_DIR:-}"
    if [[ -z "$info_dir" && -n "$toolchain_dir" ]]; then
        info_dir="$toolchain_dir/usr/bin/info"
    fi
    info_dir="${info_dir:-/var/lib/dpkg/info}"
    local md5sums_file="$info_dir/${pkg}.md5sums"
    local rel_path="${real_path#/}"
    if [[ ! -r "$md5sums_file" ]]; then
        note_failure "$name: md5sums database missing for package $pkg ($md5sums_file)"
        return
    fi
    local expected actual
    expected="$(awk -v p="$rel_path" 'NF == 2 && $2 == p { print $1 }' "$md5sums_file" | head -n1)"
    if [[ -z "$expected" ]]; then
        note_failure "$name: no md5sums entry for $rel_path in package $pkg"
        return
    fi
    actual="$(md5_of "$real_path" | cut -d' ' -f1)"
    if [[ "$actual" != "$expected" ]]; then
        note_failure "$name: payload modification detected ($rel_path: expected md5 $expected, actual $actual)"
        return
    fi
    echo "  OK   $name: payload md5 verified ($rel_path)"
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
    local expected_pkg expected_version
    case "$name" in
        clang | clang++)
            expected_pkg="clang-${llvm_major}"
            expected_version="$(package_pin_version CLANG_PACKAGE_PIN)"
            ;;
        clang-tidy)
            expected_pkg="clang-tidy-${llvm_major}"
            expected_version="$(package_pin_version CLANG_TIDY_PACKAGE_PIN)"
            ;;
        clang-format)
            expected_pkg="clang-format-${llvm_major}"
            expected_version="$(package_pin_version CLANG_FORMAT_PACKAGE_PIN)"
            ;;
    esac
    check_package_identity "$name" "$real_path" "$expected_pkg" "$expected_version"
    check_payload_integrity "$name" "$real_path" "$expected_pkg"
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
echo "  canonical identity: clang/clang++/clang-tidy/clang-format $llvm_version (packages clang-$llvm_major, clang-tidy-$llvm_major, clang-format-$llvm_major, payload md5 verified)"
echo "  ubuntu snapshot: $snapshot_id (must not be later than reference UTC ${reference_time})"

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
