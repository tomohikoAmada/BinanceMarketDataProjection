#!/usr/bin/env bash
# test-quality-toolchain.sh — deterministic tests for the canonical Quality
# toolchain contract (INFRA-TC-001).
#
# Categories:
#   unit (offline): exact/missing/wrong tool identity, malformed contract,
#     duplicate-key fail-closed, real-UTC calendar validation, AppleClang
#     rejection, wrong-PATH rejection, conan/cmake/python floors, dpkg package
#     provenance, UBUNTU_SNAPSHOT_ID validation, base-reference plumbing,
#     snapshot apt-sources generation, Dockerfile base binding.
#   entrypoint adversarial (offline): the PRODUCTION canonical entrypoint
#     (scripts/quality.sh) must reject test-only BMD_QUALITY_* hooks, stale
#     internal BMD_CANONICAL_QUALITY_* orchestration variables (container
#     mode is not ambient-selectable; /src and /work are not
#     ambient-substitutable), calendar/temporal-invalid authoritative
#     contracts, and direct internal-mode invocation outside the canonical
#     image, BEFORE any image-build command (proved with a fake docker shim
#     recording invocations).
#   adversarial (offline, requires cmake + ninja): stale-object build reuse
#     against the production work-preparation mechanism, source deletion and
#     rollback between runs.
#   integration: the canonical container build itself (CI quality job) and
#     the live failure proofs documented in the PR report.
#
# No network access; fake tool shims are generated from the repository's own
# .toolchain/quality.env, CMakePresets.json, and requirements-tools.txt so the
# tests validate the real contract.
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
check_script="$repo_root/scripts/quality-toolchain-check.sh"
work_root="$repo_root/build/quality-toolchain-tests"

llvm_major="$(sed -n 's/^LLVM_MAJOR=//p' "$repo_root/.toolchain/quality.env")"
llvm_minor="$(sed -n 's/^LLVM_MINOR=//p' "$repo_root/.toolchain/quality.env")"
llvm_patch="$(sed -n 's/^LLVM_PATCH=//p' "$repo_root/.toolchain/quality.env")"
llvm_version="$llvm_major.$llvm_minor.$llvm_patch"
conan_version="$(sed -n 's/^conan==//p' "$repo_root/requirements-tools.txt" | head -n1)"
clang_pkg_version="$(sed -n 's/^CLANG_PACKAGE_PIN=//p' "$repo_root/.toolchain/quality.env")"
clang_pkg_version="${clang_pkg_version#clang-18=}"
tidy_pkg_version="$(sed -n 's/^CLANG_TIDY_PACKAGE_PIN=//p' "$repo_root/.toolchain/quality.env")"
tidy_pkg_version="${tidy_pkg_version#clang-tidy-18=}"
format_pkg_version="$(sed -n 's/^CLANG_FORMAT_PACKAGE_PIN=//p' "$repo_root/.toolchain/quality.env")"
format_pkg_version="${format_pkg_version#clang-format-18=}"

pass_count=0
fail_count=0

md5_of() {
    # portable md5 of stdin/file
    if command -v md5sum >/dev/null 2>&1; then
        md5sum
    elif command -v md5 >/dev/null 2>&1; then
        md5 -q
    else
        echo "no md5 tool found" >&2
        exit 1
    fi
}

sha256_of() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256
    else
        echo "no sha256 tool found" >&2
        exit 1
    fi
}

if command -v sha256sum >/dev/null 2>&1; then
    sha256_cmd="sha256sum"
else
    sha256_cmd="shasum -a 256"
fi

bash_bin="$(command -v bash)"

make_shim() {
    local dir="$1" name="$2" version_line="$3"
    mkdir -p "$dir"
    cat > "$dir/$name" <<EOF
#!/usr/bin/env bash
case "\${1:-}" in
    --version) printf '%s\\n' "$version_line" ;;
esac
EOF
    chmod +x "$dir/$name"
}

make_dpkg_shim() {
    # fake dpkg for offline provenance tests; paths resolve under the fake
    # tree, package names/versions mirror the real contract
    local dir="$1"
    local clang_v="${2:-$clang_pkg_version}"
    local tidy_v="${3:-$tidy_pkg_version}"
    local format_v="${4:-$format_pkg_version}"
    cat > "$dir/dpkg" <<EOF
#!/usr/bin/env bash
case "\${1:-}" in
    -S)
        case "\$2" in
            */clang) printf '%s\\n' "clang-$llvm_major: \$2" ;;
            */clang++) printf '%s\\n' "clang-$llvm_major: \$2" ;;
            */clang-tidy) printf '%s\\n' "clang-tidy-$llvm_major: \$2" ;;
            */clang-format) printf '%s\\n' "clang-format-$llvm_major: \$2" ;;
            *) echo "dpkg-query: no path found matching pattern *\$2*" >&2; exit 1 ;;
        esac
        ;;
    -s)
        case "\$2" in
            clang-$llvm_major) printf '%s\\n' "Version: $clang_v" ;;
            clang-tidy-$llvm_major) printf '%s\\n' "Version: $tidy_v" ;;
            clang-format-$llvm_major) printf '%s\\n' "Version: $format_v" ;;
            *) echo "dpkg-query: package '\$2' is not installed" >&2; exit 1 ;;
        esac
        ;;
esac
EOF
    chmod +x "$dir/dpkg"
}

make_md5sums() {
    # fake dpkg md5sums database entries for the fake tree's shims; rel paths
    # mirror the container layout (real path minus leading '/')
    local dir="$1"
    mkdir -p "$dir/info"
    for t in clang clang++ clang-tidy clang-format; do
        local pkg real md5
        case "$t" in
            clang | clang++) pkg="clang-$llvm_major" ;;
            clang-tidy) pkg="clang-tidy-$llvm_major" ;;
            clang-format) pkg="clang-format-$llvm_major" ;;
        esac
        real="$(cd "$dir" && pwd)/$t"
        md5="$(md5_of < "$dir/$t" | cut -d' ' -f1)"
        printf '%s  %s\n' "$md5" "${real#/}" >> "$dir/info/$pkg.md5sums"
    done
}

build_tree() {
    local tree="$1"
    local clang_line="${2:-Ubuntu clang version $llvm_version (1ubuntu1)}"
    local clangpp_line="${3:-Ubuntu clang version $llvm_version (1ubuntu1)}"
    local tidy_line="${4:-Ubuntu clang-tidy version $llvm_version (1ubuntu1)}"
    local format_line="${5:-Ubuntu clang-format version $llvm_version (1ubuntu1)}"
    local cmake_line="${6:-cmake version 3.28.3}"
    local python_line="${7:-Python 3.12.3}"
    local conan_line="${8:-Conan version $conan_version}"
    local ninja_line="${9:-1.11.1}"
    rm -rf "$tree"
    mkdir -p "$tree/usr/bin" "$tree/contract"
    cp "$repo_root/.toolchain/quality.env" "$tree/contract/quality.env"
    make_shim "$tree/usr/bin" clang "$clang_line"
    make_shim "$tree/usr/bin" clang++ "$clangpp_line"
    make_shim "$tree/usr/bin" clang-tidy "$tidy_line"
    make_shim "$tree/usr/bin" clang-format "$format_line"
    make_shim "$tree/usr/bin" cmake "$cmake_line"
    make_shim "$tree/usr/bin" python3 "$python_line"
    make_shim "$tree/usr/bin" conan "$conan_line"
    make_shim "$tree/usr/bin" ninja "$ninja_line"
    make_dpkg_shim "$tree/usr/bin"
    make_md5sums "$tree/usr/bin"
}

run_case() {
    # run_case <name> <expected_exit_zero:yes|no> <env...> -- <args...>
    local name="$1" want_ok="$2"
    shift 2
    local envs=() args=()
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --) shift; break ;;
            *) envs+=("$1"); shift ;;
        esac
    done
    args+=("$@")
    local out rc
    out="$(env "${envs[@]+"${envs[@]}"}" "$check_script" "${args[@]+"${args[@]}"}" 2>&1)" && rc=0 || rc=$?
    local verdict
    if [[ "$want_ok" == "yes" && "$rc" -eq 0 ]]; then
        verdict="PASS"
        pass_count=$((pass_count + 1))
    elif [[ "$want_ok" == "no" && "$rc" -ne 0 ]]; then
        if [[ "$out" == *"QUALITY TOOLCHAIN CHECK: FAIL"* ]]; then
            verdict="PASS"
            pass_count=$((pass_count + 1))
        else
            verdict="FAIL(no FAIL diagnostic)"
            fail_count=$((fail_count + 1))
        fi
    else
        verdict="FAIL(expected exit ${want_ok}, got $rc)"
        fail_count=$((fail_count + 1))
    fi
    if [[ "$verdict" != "PASS" ]]; then
        printf '%s\n' "$out"
    fi
    printf '  [%s] %s\n' "$verdict" "$name"
}

run_cmd_case() {
    # run_cmd_case <name> <expected_exit_zero:yes|no> <cmd...>
    local name="$1" want_ok="$2"
    shift 2
    local out rc
    out="$("$@" 2>&1)" && rc=0 || rc=$?
    local verdict
    if [[ "$want_ok" == "yes" && "$rc" -eq 0 ]]; then
        verdict="PASS"
        pass_count=$((pass_count + 1))
    elif [[ "$want_ok" == "no" && "$rc" -ne 0 ]]; then
        verdict="PASS"
        pass_count=$((pass_count + 1))
    else
        verdict="FAIL(expected exit ${want_ok}, got $rc)"
        fail_count=$((fail_count + 1))
    fi
    if [[ "$verdict" != "PASS" ]]; then
        printf '%s\n' "$out"
    fi
    printf '  [%s] %s\n' "$verdict" "$name"
}

tree() { printf '%s/case-%s' "$work_root" "$1"; }

contract_variant() {
    # contract_variant <case-dir> <sed-expr> — a copy of the real repository
    # contract with exactly one sed transformation applied. The real contract
    # is never mutated.
    local dir="$1" expr="$2"
    rm -rf "$dir"
    mkdir -p "$dir"
    sed "$expr" "$repo_root/.toolchain/quality.env" > "$dir/quality.env"
}

dup_contract() {
    # dup_contract <case-dir> <key> — the real contract plus one additional
    # assignment of <key> (same value) prepended; a duplicate key contract
    # must FAIL CLOSED in every consumer.
    local dir="$1" key="$2"
    rm -rf "$dir"
    mkdir -p "$dir"
    {
        grep "^${key}=" "$repo_root/.toolchain/quality.env"
        cat "$repo_root/.toolchain/quality.env"
    } > "$dir/quality.env"
}

dup_contract_value() {
    # dup_contract_value <case-dir> <key> <value> — the real contract plus one
    # prepended assignment of <key> with a DIFFERENT value; a conflicting
    # duplicate must FAIL CLOSED (no first-wins/last-wins interpretation).
    local dir="$1" key="$2" value="$3"
    rm -rf "$dir"
    mkdir -p "$dir"
    {
        printf '%s=%s\n' "$key" "$value"
        cat "$repo_root/.toolchain/quality.env"
    } > "$dir/quality.env"
}

run_entrypoint_case() {
    # run_entrypoint_case <name> <required-diagnostic-regex> <env...> -- <args...>
    # Runs the PRODUCTION canonical entrypoint (scripts/quality.sh) with a
    # fake docker shim on PATH that records any invocation in
    # $fake_docker_marker. PASS requires: nonzero exit, the required
    # diagnostic, and NO container-runtime invocation (proof that the failure
    # happened before any image-build command was reached).
    local name="$1" diag_re="$2"
    shift 2
    local envs=() args=()
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --) shift; break ;;
            *) envs+=("$1"); shift ;;
        esac
    done
    args+=("$@")
    local out rc
    rm -f "$fake_docker_marker"
    out="$(PATH="$fake_docker_dir:$PATH" env \
        BMD_FAKE_DOCKER_MARKER="$fake_docker_marker" \
        "${envs[@]+"${envs[@]}"}" bash "${args[@]}" 2>&1)" && rc=0 || rc=$?
    if [[ "$rc" -ne 0 && "$out" =~ $diag_re && ! -f "$fake_docker_marker" ]]; then
        verdict="PASS"
        pass_count=$((pass_count + 1))
    else
        verdict="FAIL(rc=$rc, diag ~${diag_re}, build reached: $([[ -f "$fake_docker_marker" ]] && echo YES || echo NO))"
        fail_count=$((fail_count + 1))
    fi
    if [[ "$verdict" != "PASS" ]]; then
        printf '%s\n' "$out"
    fi
    printf '  [%s] %s\n' "$verdict" "$name"
}

make_fake_docker() {
    # make_fake_docker <dir> <version-server-json-or-empty> <version-rc>
    # A fake `docker` command: prints the given server JSON for
    # `docker version --format '{{json .Server}}'`, exits with the given rc
    # for it, and records any other invocation (info/build/run) in the
    # BMD_FAKE_DOCKER_MARKER file. The JSON payload is stored in a sidecar
    # file (embedding it inside the shim's double-quoted string would mangle
    # its quotes).
    local dir="$1" vout="$2" vrc="$3"
    mkdir -p "$dir"
    printf '%s\n' "$vout" > "$dir/version-server.json"
    cat > "$dir/docker" <<EOF
#!/usr/bin/env bash
if [[ "\${1:-}" == "version" ]]; then
    cat "$dir/version-server.json"
    exit $vrc
fi
echo "FAKE DOCKER INVOKED: \$*" >> "\${BMD_FAKE_DOCKER_MARKER}"
exit 0
EOF
    chmod +x "$dir/docker"
}

rm -rf "$work_root"
mkdir -p "$work_root"
cd "$work_root"

echo "quality-toolchain tests (contract: clang $llvm_version, conan $conan_version, snapshot $(sed -n 's/^UBUNTU_SNAPSHOT_ID=//p' "$repo_root/.toolchain/quality.env"))"

# ============================================================================
# Unit: tool identity, contract, provenance, snapshot
# ============================================================================

# 1. exact match, full mode
build_tree "$(tree match)"
run_case "exact toolchain match -> PASS" yes \
    BMD_QUALITY_CONTRACT_FILE="$(tree match)/contract/quality.env" \
    BMD_QUALITY_TOOLCHAIN_DIR="$(tree match)" \
    PATH="$(tree match)/usr/bin:$PATH" \
    --

# 2. exact match with --skip-conan and no conan shim
build_tree "$(tree skipconan)"
rm "$(tree skipconan)/usr/bin/conan"
run_case "skip-conan without conan installed -> PASS" yes \
    BMD_QUALITY_CONTRACT_FILE="$(tree skipconan)/contract/quality.env" \
    BMD_QUALITY_TOOLCHAIN_DIR="$(tree skipconan)" \
    PATH="$(tree skipconan)/usr/bin:$PATH" \
    -- --skip-conan

# 3. wrong clang-tidy version -> FAIL
build_tree "$(tree tidy)" \
    "" "" "Ubuntu clang-tidy version $llvm_major.$llvm_minor.$((llvm_patch + 1)) (1ubuntu1)" \
    ""
run_case "wrong clang-tidy version -> FAIL" no \
    BMD_QUALITY_CONTRACT_FILE="$(tree tidy)/contract/quality.env" \
    BMD_QUALITY_TOOLCHAIN_DIR="$(tree tidy)" \
    PATH="$(tree tidy)/usr/bin:$PATH" \
    --

# 4. wrong clang-format version -> FAIL
build_tree "$(tree fmt)" \
    "" "" "" "Ubuntu clang-format version $llvm_major.$((llvm_minor + 1)).$llvm_patch (1ubuntu1)" \
    ""
run_case "wrong clang-format version -> FAIL" no \
    BMD_QUALITY_CONTRACT_FILE="$(tree fmt)/contract/quality.env" \
    BMD_QUALITY_TOOLCHAIN_DIR="$(tree fmt)" \
    PATH="$(tree fmt)/usr/bin:$PATH" \
    --

# 5. missing clang -> FAIL
build_tree "$(tree missing)"
rm "$(tree missing)/usr/bin/clang"
run_case "missing clang -> FAIL" no \
    BMD_QUALITY_CONTRACT_FILE="$(tree missing)/contract/quality.env" \
    BMD_QUALITY_TOOLCHAIN_DIR="$(tree missing)" \
    PATH="$(tree missing)/usr/bin:$PATH" \
    --

# 6. malformed contract (missing key) -> FAIL
build_tree "$(tree malformed1)"
grep -v '^LLVM_PATCH=' "$(tree malformed1)/contract/quality.env" \
    > "$(tree malformed1)/contract/quality.env.tmp"
mv "$(tree malformed1)/contract/quality.env.tmp" "$(tree malformed1)/contract/quality.env"
run_case "malformed contract (missing LLVM_PATCH) -> FAIL" no \
    BMD_QUALITY_CONTRACT_FILE="$(tree malformed1)/contract/quality.env" \
    BMD_QUALITY_TOOLCHAIN_DIR="$(tree malformed1)" \
    PATH="$(tree malformed1)/usr/bin:$PATH" \
    --

# 7. malformed contract (non-numeric version) -> FAIL
build_tree "$(tree malformed2)"
sed 's/^LLVM_PATCH=.*/LLVM_PATCH=abc/' \
    "$(tree malformed2)/contract/quality.env" \
    > "$(tree malformed2)/contract/quality.env.tmp"
mv "$(tree malformed2)/contract/quality.env.tmp" "$(tree malformed2)/contract/quality.env"
run_case "malformed contract (LLVM_PATCH=abc) -> FAIL" no \
    BMD_QUALITY_CONTRACT_FILE="$(tree malformed2)/contract/quality.env" \
    BMD_QUALITY_TOOLCHAIN_DIR="$(tree malformed2)" \
    PATH="$(tree malformed2)/usr/bin:$PATH" \
    --

# 8. AppleClang -> FAIL
build_tree "$(tree apple)" \
    "" "Apple clang version 15.0.0 (clang-1500.0.40.1)" \
    ""
run_case "AppleClang clang++ -> FAIL" no \
    BMD_QUALITY_CONTRACT_FILE="$(tree apple)/contract/quality.env" \
    BMD_QUALITY_TOOLCHAIN_DIR="$(tree apple)" \
    PATH="$(tree apple)/usr/bin:$PATH" \
    --

# 9. PATH selects a non-canonical installation -> FAIL
build_tree "$(tree wrongpath)"
mkdir -p "$(tree wrongpath)/usr/local/bin"
make_shim "$(tree wrongpath)/usr/local/bin" clang \
    "Ubuntu clang version $llvm_version (1ubuntu1)"
run_case "PATH selects /usr/local clang -> FAIL" no \
    BMD_QUALITY_CONTRACT_FILE="$(tree wrongpath)/contract/quality.env" \
    BMD_QUALITY_TOOLCHAIN_DIR="$(tree wrongpath)" \
    PATH="$(tree wrongpath)/usr/local/bin:$(tree wrongpath)/usr/bin:$PATH" \
    --

# 10. conan mismatch against requirements-tools.txt -> FAIL
build_tree "$(tree conan)" \
    "" "" "" "" "" "" "Conan version 2.30.0"
run_case "conan version mismatch -> FAIL" no \
    BMD_QUALITY_CONTRACT_FILE="$(tree conan)/contract/quality.env" \
    BMD_QUALITY_TOOLCHAIN_DIR="$(tree conan)" \
    PATH="$(tree conan)/usr/bin:$PATH" \
    --

# 11. cmake below CMakePresets.json minimum -> FAIL
cm_min_major="$(sed -n 's/.*"major": *\([0-9][0-9]*\).*/\1/p' \
    < <(grep -A3 '"cmakeMinimumRequired"' "$repo_root/CMakePresets.json") | head -n1)"
cm_min_minor="$(sed -n 's/.*"minor": *\([0-9][0-9]*\).*/\1/p' \
    < <(grep -A3 '"cmakeMinimumRequired"' "$repo_root/CMakePresets.json") | head -n1)"
low_cmake="cmake version $cm_min_major.$((cm_min_minor - 1)).0"
build_tree "$(tree cmake)" "" "" "" "" "$low_cmake"
run_case "cmake below CMakePresets minimum -> FAIL" no \
    BMD_QUALITY_CONTRACT_FILE="$(tree cmake)/contract/quality.env" \
    BMD_QUALITY_TOOLCHAIN_DIR="$(tree cmake)" \
    PATH="$(tree cmake)/usr/bin:$PATH" \
    --

# 12. python below contract floor -> FAIL
py_min="$(sed -n 's/^PYTHON_MINIMUM_VERSION=//p' "$repo_root/.toolchain/quality.env")"
IFS=. read -r py_ma py_mi py_pa <<< "$py_min"
low_python="Python $py_ma.$((py_mi - 1)).0"
build_tree "$(tree python)" "" "" "" "" "" "$low_python"
run_case "python below contract floor -> FAIL" no \
    BMD_QUALITY_CONTRACT_FILE="$(tree python)/contract/quality.env" \
    BMD_QUALITY_TOOLCHAIN_DIR="$(tree python)" \
    PATH="$(tree python)/usr/bin:$PATH" \
    --

# 13. contract-only mode: well-formed contract -> PASS
run_case "contract-only well formed -> PASS" yes \
    BMD_QUALITY_CONTRACT_FILE="$repo_root/.toolchain/quality.env" \
    -- --contract-only

# 14. contract-only mode: malformed contract -> FAIL
run_case "contract-only malformed -> FAIL" no \
    BMD_QUALITY_CONTRACT_FILE="$(tree malformed1)/contract/quality.env" \
    -- --contract-only

# 15. package provenance: wrong installed package version -> FAIL
build_tree "$(tree provwrong)"
make_dpkg_shim "$(tree provwrong)/usr/bin" "1:18.1.2-1ubuntu1"
run_case "dpkg package version mismatch -> FAIL" no \
    BMD_QUALITY_CONTRACT_FILE="$(tree provwrong)/contract/quality.env" \
    BMD_QUALITY_TOOLCHAIN_DIR="$(tree provwrong)" \
    PATH="$(tree provwrong)/usr/bin:$PATH" \
    --

# 16. package provenance: unowned real executable -> FAIL
build_tree "$(tree provunowned)"
cat > "$(tree provunowned)/usr/bin/dpkg" <<EOF
#!/usr/bin/env bash
case "\${1:-}" in
    -S) echo "dpkg-query: no path found matching pattern *\$2*" >&2; exit 1 ;;
    -s) echo "dpkg-query: package '\$2' is not installed" >&2; exit 1 ;;
esac
EOF
chmod +x "$(tree provunowned)/usr/bin/dpkg"
run_case "dpkg unowned real executable -> FAIL" no \
    BMD_QUALITY_CONTRACT_FILE="$(tree provunowned)/contract/quality.env" \
    BMD_QUALITY_TOOLCHAIN_DIR="$(tree provunowned)" \
    PATH="$(tree provunowned)/usr/bin:$PATH" \
    --

# 17. snapshot: malformed UBUNTU_SNAPSHOT_ID -> FAIL
build_tree "$(tree snapbad)"
sed 's/^UBUNTU_SNAPSHOT_ID=.*/UBUNTU_SNAPSHOT_ID=20260815/' \
    "$(tree snapbad)/contract/quality.env" \
    > "$(tree snapbad)/contract/quality.env.tmp"
mv "$(tree snapbad)/contract/quality.env.tmp" "$(tree snapbad)/contract/quality.env"
run_case "malformed UBUNTU_SNAPSHOT_ID -> FAIL" no \
    BMD_QUALITY_CONTRACT_FILE="$(tree snapbad)/contract/quality.env" \
    BMD_QUALITY_TOOLCHAIN_DIR="$(tree snapbad)" \
    PATH="$(tree snapbad)/usr/bin:$PATH" \
    --

# 17b. snapshot: missing UBUNTU_SNAPSHOT_ID -> FAIL
build_tree "$(tree snapmissing)"
grep -v '^UBUNTU_SNAPSHOT_ID=' "$(tree snapmissing)/contract/quality.env" \
    > "$(tree snapmissing)/contract/quality.env.tmp"
mv "$(tree snapmissing)/contract/quality.env.tmp" "$(tree snapmissing)/contract/quality.env"
run_case "missing UBUNTU_SNAPSHOT_ID -> FAIL" no \
    BMD_QUALITY_CONTRACT_FILE="$(tree snapmissing)/contract/quality.env" \
    BMD_QUALITY_TOOLCHAIN_DIR="$(tree snapmissing)" \
    PATH="$(tree snapmissing)/usr/bin:$PATH" \
    --

# 17c. snapshot temporal: historical snapshot accepted
run_case "historical snapshot accepted (real contract) -> PASS" yes \
    BMD_QUALITY_CONTRACT_FILE="$repo_root/.toolchain/quality.env" \
    -- --contract-only

# 17d. snapshot temporal: future snapshot rejected (deterministic reference)
build_tree "$(tree snapfuture)"
sed 's/^UBUNTU_SNAPSHOT_ID=.*/UBUNTU_SNAPSHOT_ID=20990101T000000Z/' \
    "$(tree snapfuture)/contract/quality.env" \
    > "$(tree snapfuture)/contract/quality.env.tmp"
mv "$(tree snapfuture)/contract/quality.env.tmp" "$(tree snapfuture)/contract/quality.env"
run_case "future snapshot rejected (fixed reference) -> FAIL" no \
    BMD_QUALITY_CONTRACT_FILE="$(tree snapfuture)/contract/quality.env" \
    BMD_QUALITY_REFERENCE_TIME="20260815T000000Z" \
    -- --contract-only

# 17e. snapshot temporal: snapshot equal to reference accepted
build_tree "$(tree snapequal)"
sed 's/^UBUNTU_SNAPSHOT_ID=.*/UBUNTU_SNAPSHOT_ID=20260814T120000Z/' \
    "$(tree snapequal)/contract/quality.env" \
    > "$(tree snapequal)/contract/quality.env.tmp"
mv "$(tree snapequal)/contract/quality.env.tmp" "$(tree snapequal)/contract/quality.env"
run_case "snapshot equal to reference accepted -> PASS" yes \
    BMD_QUALITY_CONTRACT_FILE="$(tree snapequal)/contract/quality.env" \
    BMD_QUALITY_REFERENCE_TIME="20260814T120000Z" \
    -- --contract-only

# 17f. snapshot temporal: future snapshot rejected against real clock
run_case "future snapshot rejected (real clock) -> FAIL" no \
    BMD_QUALITY_CONTRACT_FILE="$(tree snapfuture)/contract/quality.env" \
    -- --contract-only

# ============================================================================
# Unit: duplicate contract keys fail closed (no first-wins/last-wins)
# ============================================================================

# A. duplicate UBUNTU_SNAPSHOT_ID -> FAIL
dup_contract "$(tree dup-snapshot)" UBUNTU_SNAPSHOT_ID
run_case "duplicate UBUNTU_SNAPSHOT_ID -> FAIL" no \
    BMD_QUALITY_CONTRACT_FILE="$(tree dup-snapshot)/quality.env" \
    -- --contract-only

# B. duplicate LLVM_MAJOR -> FAIL
dup_contract "$(tree dup-llvm)" LLVM_MAJOR
run_case "duplicate LLVM_MAJOR -> FAIL" no \
    BMD_QUALITY_CONTRACT_FILE="$(tree dup-llvm)/quality.env" \
    -- --contract-only

# C. duplicate base-image digest -> FAIL
dup_contract "$(tree dup-digest)" CANONICAL_QUALITY_BASE_IMAGE_DIGEST
run_case "duplicate base-image digest -> FAIL" no \
    BMD_QUALITY_CONTRACT_FILE="$(tree dup-digest)/quality.env" \
    -- --contract-only

# D. duplicate bootstrap artifact hash -> FAIL
dup_contract "$(tree dup-artifact)" CA_CERTIFICATES_BOOTSTRAP_SHA256
run_case "duplicate bootstrap artifact hash -> FAIL" no \
    BMD_QUALITY_CONTRACT_FILE="$(tree dup-artifact)/quality.env" \
    -- --contract-only

# D2. duplicate arbitrary syntactically valid key (not acceptance-critical)
# -> FAIL: the duplicate rule applies to ALL non-comment KEY=VALUE lines
dup_contract "$(tree dup-suite)" UBUNTU_SUITE
run_case "duplicate arbitrary valid key (UBUNTU_SUITE) -> FAIL" no \
    BMD_QUALITY_CONTRACT_FILE="$(tree dup-suite)/quality.env" \
    -- --contract-only

# D3. duplicate key with a DIFFERENT value -> FAIL (conflicting assignment;
# neither first-wins nor last-wins is an accepted interpretation)
dup_contract_value "$(tree dup-diffval)" LLVM_MAJOR 19
run_case "conflicting duplicate LLVM_MAJOR (18 then 19) -> FAIL" no \
    BMD_QUALITY_CONTRACT_FILE="$(tree dup-diffval)/quality.env" \
    -- --contract-only

# ============================================================================
# Unit: real UTC calendar validation of UBUNTU_SNAPSHOT_ID
# ============================================================================

# E/F. calendar-invalid snapshot IDs -> FAIL (format alone must not pass)
snapshot_invalid_cases=(
    "20260230T120000Z:Feb 30 (2026, non-leap)"
    "20261301T120000Z:month 13"
    "20260001T120000Z:month 00"
    "20260832T120000Z:day 32 (August has 31)"
    "20260814T240000Z:hour 24"
    "20260814T126000Z:minute 60"
    "20260814T120060Z:second 60"
)
snapshot_cal_n=0
for entry in "${snapshot_invalid_cases[@]}"; do
    bad_id="${entry%%:*}"
    label="${entry#*:}"
    snapshot_cal_n=$((snapshot_cal_n + 1))
    contract_variant "$(tree "snapcal-$snapshot_cal_n")" "s/^UBUNTU_SNAPSHOT_ID=.*/UBUNTU_SNAPSHOT_ID=${bad_id}/"
    run_case "calendar-invalid snapshot ${label} -> FAIL" no \
        BMD_QUALITY_CONTRACT_FILE="$(tree "snapcal-$snapshot_cal_n")/quality.env" \
        -- --contract-only
done

# G. valid leap-day snapshot (2024-02-29) -> PASS
contract_variant "$(tree snap-leap-ok)" "s/^UBUNTU_SNAPSHOT_ID=.*/UBUNTU_SNAPSHOT_ID=20240229T120000Z/"
run_case "valid leap-day snapshot 20240229T120000Z -> PASS" yes \
    BMD_QUALITY_CONTRACT_FILE="$(tree snap-leap-ok)/quality.env" \
    -- --contract-only

# H. invalid non-leap-day snapshot (2025-02-29) -> FAIL
contract_variant "$(tree snap-leap-bad)" "s/^UBUNTU_SNAPSHOT_ID=.*/UBUNTU_SNAPSHOT_ID=20250229T120000Z/"
run_case "invalid non-leap-day snapshot 20250229T120000Z -> FAIL" no \
    BMD_QUALITY_CONTRACT_FILE="$(tree snap-leap-bad)/quality.env" \
    -- --contract-only

# injected reference time must itself be calendar valid
run_case "calendar-invalid BMD_QUALITY_REFERENCE_TIME -> FAIL" no \
    BMD_QUALITY_CONTRACT_FILE="$repo_root/.toolchain/quality.env" \
    BMD_QUALITY_REFERENCE_TIME="20260230T120000Z" \
    -- --contract-only

# ============================================================================
# Unit: every canonical consumer rejects duplicate-key contracts
# ============================================================================

run_cmd_case "base-ref rejects duplicate-key contract -> FAIL" no \
    bash "$repo_root/scripts/quality-base-ref.sh" "$(tree dup-digest)/quality.env"

run_cmd_case "cache-key rejects duplicate-key contract -> FAIL" no \
    bash "$repo_root/scripts/quality-cache-key.sh" "$(tree dup-snapshot)/quality.env" "$repo_root/requirements-tools.txt"

# ============================================================================
# Unit: base-reference plumbing (production scripts/quality-base-ref.sh)
# ============================================================================
contract_digest="$(sed -n 's/^CANONICAL_QUALITY_BASE_IMAGE_DIGEST=//p' "$repo_root/.toolchain/quality.env")"
contract_image="$(sed -n 's/^CANONICAL_QUALITY_BASE_IMAGE=//p' "$repo_root/.toolchain/quality.env")"

# 18. base ref matches the authoritative contract
run_cmd_case "base-ref matches authoritative contract -> PASS" yes \
    bash -c "
        out=\"\$(bash '$repo_root/scripts/quality-base-ref.sh')\"
        [[ \"\$out\" == '$contract_image@$contract_digest' ]] || { echo \"got: \$out\"; exit 1; }
    "

# 19. digest mutation changes the base ref (BASE-1 plumbing)
mutated="$work_root/mutated-contract"
mkdir -p "$mutated"
sed 's/^CANONICAL_QUALITY_BASE_IMAGE_DIGEST=.*/CANONICAL_QUALITY_BASE_IMAGE_DIGEST=sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/' \
    "$repo_root/.toolchain/quality.env" > "$mutated/quality.env"
run_cmd_case "base-ref follows mutated digest -> PASS" yes \
    bash -c "
        out=\"\$(bash '$repo_root/scripts/quality-base-ref.sh' '$mutated/quality.env')\"
        [[ \"\$out\" == '$contract_image@sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa' ]] || { echo \"got: \$out\"; exit 1; }
    "

# 20. missing digest fails closed
nodigest="$work_root/contract-no-digest"
mkdir -p "$nodigest"
grep -v '^CANONICAL_QUALITY_BASE_IMAGE_DIGEST=' \
    "$repo_root/.toolchain/quality.env" > "$nodigest/quality.env"
run_cmd_case "base-ref missing digest -> FAIL" no \
    bash "$repo_root/scripts/quality-base-ref.sh" "$nodigest/quality.env"

# ============================================================================
# Unit: snapshot apt-sources generation (production scripts/quality-apt-sources.sh)
# ============================================================================
snapshot_id="$(sed -n 's/^UBUNTU_SNAPSHOT_ID=//p' "$repo_root/.toolchain/quality.env")"

# 21. generated sources reference ONLY the snapshot, both pockets
run_cmd_case "apt-sources snapshot-only both pockets -> PASS" yes \
    bash -c "
        out=\"\$(bash '$repo_root/scripts/quality-apt-sources.sh' '$snapshot_id' noble)\"
        echo \"\$out\" | grep -q 'https://snapshot.ubuntu.com/ubuntu/${snapshot_id}/' || { echo 'missing snapshot URI'; exit 1; }
        echo \"\$out\" | grep -q 'Suites: noble noble-updates noble-backports' || { echo 'missing archive pockets'; exit 1; }
        echo \"\$out\" | grep -q 'Suites: noble-security' || { echo 'missing security pocket'; exit 1; }
        ! echo \"\$out\" | grep -qE 'archive\.ubuntu\.com|security\.ubuntu\.com' || { echo 'live archive reference leaked'; exit 1; }
    "

# 22. malformed snapshot id fails closed
run_cmd_case "apt-sources malformed snapshot id -> FAIL" no \
    bash "$repo_root/scripts/quality-apt-sources.sh" "not-a-snapshot" noble

# ============================================================================
# Unit: Dockerfile base binding (no independent digest, no fallback)
# ============================================================================
dockerfile="$repo_root/.toolchain/Dockerfile"

# 23. Dockerfile FROM derives only from the authoritative build arg
run_cmd_case "Dockerfile FROM uses only the build arg -> PASS" yes \
    bash -c "
        from_line=\"\$(grep -E '^FROM ' '$dockerfile' | tail -n1)\"
        [[ \"\$from_line\" == 'FROM \${BMD_CANONICAL_BASE_IMAGE_REF}' ]] || { echo \"FROM: \$from_line\"; exit 1; }
        grep -q '^ARG BMD_CANONICAL_BASE_IMAGE_REF\$' '$dockerfile' || { echo 'ARG missing'; exit 1; }
        ! grep -qE '^ARG BMD_CANONICAL_BASE_IMAGE_REF=' '$dockerfile' || { echo 'ARG has a default fallback'; exit 1; }
    "

# 24. no independent digest literal anywhere in the Dockerfile
run_cmd_case "Dockerfile has no independent base digest literal -> PASS" yes \
    bash -c "
        ! grep -qE 'sha256:[0-9a-f]{64}' '$dockerfile' || { echo 'digest literal found'; exit 1; }
    "

# 25. architecture guard present
run_cmd_case "Dockerfile rejects non-amd64 TARGETARCH -> PASS" yes \
    bash -c "
        grep -q 'TARGETARCH' '$dockerfile' || { echo 'no TARGETARCH guard'; exit 1; }
        grep -q 'amd64' '$dockerfile' || { echo 'no amd64 check'; exit 1; }
    "

# 26. quality.sh passes the authoritative base ref as build argument
run_cmd_case "quality.sh passes --build-arg BMD_CANONICAL_BASE_IMAGE_REF -> PASS" yes \
    bash -c "
        grep -q -- '--build-arg \"BMD_CANONICAL_BASE_IMAGE_REF=\$base_ref\"' '$repo_root/scripts/quality.sh' || { echo 'missing build-arg plumbing'; exit 1; }
        grep -q 'quality-base-ref.sh' '$repo_root/scripts/quality.sh' || { echo 'base ref not derived from contract'; exit 1; }
    "

# 27. no mutable live archive anywhere in the canonical build
run_cmd_case "no live-archive references in Dockerfile -> PASS" yes \
    bash -c "
        ! grep -qE 'archive\.ubuntu\.com|security\.ubuntu\.com' '$dockerfile' || { echo 'live archive reference in Dockerfile'; exit 1; }
        ! grep -qE 'archive\.ubuntu\.com|security\.ubuntu\.com' '$repo_root/scripts/quality-apt-sources.sh' || { echo 'live archive reference in sources script'; exit 1; }
        grep -q 'apt-get update' '$dockerfile' || { echo 'no apt-get update at all'; exit 1; }
    "

# 28. bootstrap artifact is committed and matches the contract hash
bootstrap_sha="$(sed -n 's/^CA_CERTIFICATES_BOOTSTRAP_SHA256=//p' "$repo_root/.toolchain/quality.env")"
run_cmd_case "committed bootstrap artifact matches contract sha256 -> PASS" yes \
    bash -c "
        f='$repo_root/.toolchain/bootstrap/ca-certificates.deb'
        [[ -f \"\$f\" ]] || { echo 'bootstrap artifact missing'; exit 1; }
        actual=\"\$(${sha256_cmd} \"\$f\" | cut -d' ' -f1)\"
        [[ \"\$actual\" == '$bootstrap_sha' ]] || { echo \"sha mismatch: \$actual\"; exit 1; }
        grep -q 'CA_CERTIFICATES_BOOTSTRAP_SHA256' '$dockerfile' || { echo 'Dockerfile does not verify artifact'; exit 1; }
    "

# ============================================================================
# Unit: cache namespace identity (production scripts/quality-cache-key.sh)
# ============================================================================
cache_key_script="$repo_root/scripts/quality-cache-key.sh"
contract_real="$repo_root/.toolchain/quality.env"
reqs_real="$repo_root/requirements-tools.txt"

# 29. same contract -> same key
run_cmd_case "cache key stable for identical contract -> PASS" yes \
    bash -c "
        k1=\"\$(bash '$cache_key_script' '$contract_real' '$reqs_real')\"
        k2=\"\$(bash '$cache_key_script' '$contract_real' '$reqs_real')\"
        [[ \"\$k1\" == \"\$k2\" && \"\$k1\" =~ ^[0-9a-f]{64}$ ]] || { echo \"unstable/invalid key: \$k1 / \$k2\"; exit 1; }
    "

# 30. LLVM patch change -> different key
run_cmd_case "cache key changes on LLVM patch change -> PASS" yes \
    bash -c "
        k1=\"\$(bash '$cache_key_script' '$contract_real' '$reqs_real')\"
        sed 's/^LLVM_PATCH=.*/LLVM_PATCH=4/' '$contract_real' > '$work_root/contract-patch4.env'
        k2=\"\$(bash '$cache_key_script' '$work_root/contract-patch4.env' '$reqs_real')\"
        [[ \"\$k1\" != \"\$k2\" ]] || { echo 'key unchanged on LLVM patch change'; exit 1; }
    "

# 31. snapshot change -> different key
run_cmd_case "cache key changes on snapshot change -> PASS" yes \
    bash -c "
        k1=\"\$(bash '$cache_key_script' '$contract_real' '$reqs_real')\"
        sed 's/^UBUNTU_SNAPSHOT_ID=.*/UBUNTU_SNAPSHOT_ID=20260813T120000Z/' '$contract_real' > '$work_root/contract-snap2.env'
        k2=\"\$(bash '$cache_key_script' '$work_root/contract-snap2.env' '$reqs_real')\"
        [[ \"\$k1\" != \"\$k2\" ]] || { echo 'key unchanged on snapshot change'; exit 1; }
    "

# 32. bootstrap artifact hash change -> different key
run_cmd_case "cache key changes on bootstrap artifact change -> PASS" yes \
    bash -c "
        k1=\"\$(bash '$cache_key_script' '$contract_real' '$reqs_real')\"
        sed 's/^CA_CERTIFICATES_BOOTSTRAP_SHA256=.*/CA_CERTIFICATES_BOOTSTRAP_SHA256=0000000000000000000000000000000000000000000000000000000000000000/' '$contract_real' > '$work_root/contract-art2.env'
        k2=\"\$(bash '$cache_key_script' '$work_root/contract-art2.env' '$reqs_real')\"
        [[ \"\$k1\" != \"\$k2\" ]] || { echo 'key unchanged on artifact change'; exit 1; }
    "

# 33. Conan tool pin change -> different key
run_cmd_case "cache key changes on requirements-tools change -> PASS" yes \
    bash -c "
        k1=\"\$(bash '$cache_key_script' '$contract_real' '$reqs_real')\"
        sed 's/^conan==.*/conan==2.31.1/' '$reqs_real' > '$work_root/requirements-tools.2.txt'
        k2=\"\$(bash '$cache_key_script' '$contract_real' '$work_root/requirements-tools.2.txt')\"
        [[ \"\$k1\" != \"\$k2\" ]] || { echo 'key unchanged on conan pin change'; exit 1; }
    "

# 34. quality.sh uses the cache key for persistent volume names only
run_cmd_case "quality.sh namespaces caches by cache key -> PASS" yes \
    bash -c "
        grep -q 'quality-cache-key.sh' '$repo_root/scripts/quality.sh' || { echo 'cache key not used'; exit 1; }
        grep -q 'bmd-projection-quality-cache-\${cache_key}' '$repo_root/scripts/quality.sh' || { echo 'cache volume not keyed'; exit 1; }
        grep -q 'bmd-projection-quality-venv-\${cache_key}' '$repo_root/scripts/quality.sh' || { echo 'venv volume not keyed'; exit 1; }
        ! grep -q 'bmd-projection-quality-work' '$repo_root/scripts/quality.sh' || { echo 'persistent /work volume still present'; exit 1; }
        ! grep -q 'bmd-quality-contract-fingerprint' '$repo_root/scripts/quality.sh' || { echo 'fingerprint lifecycle still present'; exit 1; }
    "

# ============================================================================
# Unit: installed payload integrity (md5 vs package md5sums database)
# ============================================================================

# 35. payload tamper -> FAIL (shim modified, dpkg metadata unchanged)
build_tree "$(tree payload)"
printf '\n' >> "$(tree payload)/usr/bin/clang-tidy"
run_case "payload modification detected -> FAIL" no \
    BMD_QUALITY_CONTRACT_FILE="$(tree payload)/contract/quality.env" \
    BMD_QUALITY_TOOLCHAIN_DIR="$(tree payload)" \
    PATH="$(tree payload)/usr/bin:$PATH" \
    -- --skip-conan

# 36. missing md5sums database -> FAIL
build_tree "$(tree payload2)"
rm "$(tree payload2)/usr/bin/info/clang-18.md5sums"
run_case "missing md5sums database -> FAIL" no \
    BMD_QUALITY_CONTRACT_FILE="$(tree payload2)/contract/quality.env" \
    BMD_QUALITY_TOOLCHAIN_DIR="$(tree payload2)" \
    PATH="$(tree payload2)/usr/bin:$PATH" \
    -- --skip-conan

# ============================================================================
# Adversarial: stale build reuse vs production work preparation
# ============================================================================
if ! command -v cmake >/dev/null 2>&1 || ! command -v ninja >/dev/null 2>&1; then
    echo "  [FAIL] adversarial stale-build tests require cmake and ninja on PATH"
    fail_count=$((fail_count + 1))
else
    adv_src="$work_root/adv-src"
    adv_work="$work_root/adv-work"
    mkdir -p "$adv_src"
    printf 'int main() { return 0; }\n' > "$adv_src/main.cpp"
    cat > "$adv_src/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.16)
project(stale_probe CXX)
add_executable(stale_probe main.cpp)
EOF
    rm -rf "$adv_work"

    cmake -S "$adv_src" -B "$adv_work/build" -G Ninja >/dev/null 2>&1
    cmake --build "$adv_work/build" >/dev/null 2>&1

    # 27. adversarial premise: the modified source is given an mtime OLDER
    #     than the cached object (the reviewer's exact scenario), so any
    #     naive mtime-based reuse is blind to the source change. Whether a
    #     particular ninja version then skips is version-dependent; the
    #     deterministic invariant is the mtime relationship itself.
    printf '#error STALE_SOURCE_MUST_BE_OBSERVED\nint main() { return 0; }\n' > "$adv_src/main.cpp"
    touch -t 202001010000 "$adv_src/main.cpp"
    run_cmd_case "adversarial premise: object mtime newer than modified source" yes \
        bash -c "
            find '$adv_work/build/CMakeFiles/stale_probe.dir/main.cpp.o' -newer '$adv_src/main.cpp' \
                | grep -q . || { echo 'object is not newer than source'; exit 1; }
        "

    # 28. production prep: stale build tree must NOT survive
    run_cmd_case "work-prep removes stale build tree -> PASS" yes \
        bash -c "
            bash '$repo_root/scripts/quality-work-prep.sh' '$adv_src' '$adv_work' >/dev/null 2>&1
            [[ ! -d '$adv_work/build' ]] || { echo 'build tree survived prep'; exit 1; }
        "

    # 29. production prep + rebuild observes current (broken) source -> the
    #     compile error must surface; a stale-object false PASS is impossible
    run_cmd_case "stale object cannot yield PASS after canonical prep -> compile error observed" yes \
        bash -c "
            bash '$repo_root/scripts/quality-work-prep.sh' '$adv_src' '$adv_work' >/dev/null 2>&1
            cmake -S '$adv_work' -B '$adv_work/build' -G Ninja >/dev/null 2>&1
            out=\"\$(cmake --build '$adv_work/build' 2>&1)\"
            echo \"\$out\" | grep -q 'STALE_SOURCE_MUST_BE_OBSERVED' || { echo 'compile error not observed'; exit 1; }
        "

    # 30. source deletion between runs is observed
    printf 'int helper() { return 1; }\n' > "$adv_src/helper.cpp"
    bash "$repo_root/scripts/quality-work-prep.sh" "$adv_src" "$adv_work" >/dev/null 2>&1
    [[ -f "$adv_work/helper.cpp" ]] || { echo "helper.cpp missing after prep"; fail_count=$((fail_count + 1)); }
    rm "$adv_src/helper.cpp"
    bash "$repo_root/scripts/quality-work-prep.sh" "$adv_src" "$adv_work" >/dev/null 2>&1
    if [[ ! -f "$adv_work/helper.cpp" ]]; then
        echo "  [PASS] deleted source does not survive prep"
        pass_count=$((pass_count + 1))
    else
        echo "  [FAIL] deleted source survived prep"
        fail_count=$((fail_count + 1))
    fi

    # 31. rollback to older source content/timestamps is observed (content copy)
    printf 'content-A\n' > "$adv_src/main.cpp"
    bash "$repo_root/scripts/quality-work-prep.sh" "$adv_src" "$adv_work" >/dev/null 2>&1
    printf 'content-B\n' > "$adv_src/main.cpp"
    touch -t 202001010000 "$adv_src/main.cpp"
    bash "$repo_root/scripts/quality-work-prep.sh" "$adv_src" "$adv_work" >/dev/null 2>&1
    if [[ "$(cat "$adv_work/main.cpp")" == "content-B" ]]; then
        echo "  [PASS] rollback content observed (mtime-independent)"
        pass_count=$((pass_count + 1))
    else
        echo "  [FAIL] rollback content not observed"
        fail_count=$((fail_count + 1))
    fi

    # 32. recovery: fresh valid source builds after canonical prep
    printf 'int main() { return 0; }\n' > "$adv_src/main.cpp"
    run_cmd_case "fresh source builds after canonical prep -> PASS" yes \
        bash -c "
            bash '$repo_root/scripts/quality-work-prep.sh' '$adv_src' '$adv_work' >/dev/null 2>&1
            cmake -S '$adv_work' -B '$adv_work/build' -G Ninja >/dev/null 2>&1
            cmake --build '$adv_work/build' >/dev/null 2>&1
            '$adv_work/build/stale_probe'
        "
fi

# ============================================================================
# Entrypoint adversarial: canonical quality.sh must fail closed on test-only
# hooks and calendar/temporal-invalid authoritative contracts, BEFORE any
# image-build command is reached.
# ============================================================================
fake_docker_dir="$work_root/fake-docker-bin"
fake_docker_marker="$work_root/fake-docker-invoked.marker"
mkdir -p "$fake_docker_dir"
cat > "$fake_docker_dir/docker" <<'EOF'
#!/usr/bin/env bash
echo "FAKE DOCKER INVOKED: $*" >> "${BMD_FAKE_DOCKER_MARKER}"
exit 0
EOF
chmod +x "$fake_docker_dir/docker"

# a temporary authoritative repository root whose contract differs from the
# real repository contract (never mutated); quality.sh computes repo_root
# from its own location, so this exercises the production entrypoint against
# a different authoritative contract.
temp_repo="$work_root/temp-repo"
rm -rf "$temp_repo"
mkdir -p "$temp_repo/scripts" "$temp_repo/.toolchain"
for script_name in quality.sh quality-toolchain-check.sh quality-base-ref.sh quality-cache-key.sh quality-runtime-check.sh quality-image-boundary.sh; do
    cp "$repo_root/scripts/$script_name" "$temp_repo/scripts/$script_name"
done
cp "$repo_root/requirements-tools.txt" "$temp_repo/requirements-tools.txt"
sed 's/^UBUNTU_SNAPSHOT_ID=.*/UBUNTU_SNAPSHOT_ID=20990101T000000Z/' \
    "$repo_root/.toolchain/quality.env" > "$temp_repo/.toolchain/quality.env"

# a second temporary authoritative root whose contract is duplicate-keyed:
# canonical quality.sh must reject it before any image-build command
temp_repo_dup="$work_root/temp-repo-dup"
rm -rf "$temp_repo_dup"
mkdir -p "$temp_repo_dup/scripts" "$temp_repo_dup/.toolchain"
for script_name in quality.sh quality-toolchain-check.sh quality-base-ref.sh quality-cache-key.sh quality-runtime-check.sh quality-image-boundary.sh; do
    cp "$repo_root/scripts/$script_name" "$temp_repo_dup/scripts/$script_name"
done
cp "$repo_root/requirements-tools.txt" "$temp_repo_dup/requirements-tools.txt"
{
    grep '^UBUNTU_SNAPSHOT_ID=' "$repo_root/.toolchain/quality.env"
    cat "$repo_root/.toolchain/quality.env"
} > "$temp_repo_dup/.toolchain/quality.env"

# a benign alternate contract (a plain copy of the real one) that must NOT be
# promoted to canonical authority
benign_contract="$work_root/benign-contract/quality.env"
mkdir -p "$(dirname "$benign_contract")"
cp "$repo_root/.toolchain/quality.env" "$benign_contract"

# I. canonical quality.sh with a test-only BMD_QUALITY_REFERENCE_TIME
run_entrypoint_case "canonical quality.sh rejects BMD_QUALITY_REFERENCE_TIME -> FAIL before image build" \
    "BMD_QUALITY_REFERENCE_TIME" \
    BMD_QUALITY_REFERENCE_TIME="20991231T000000Z" \
    -- "$repo_root/scripts/quality.sh"

# J. canonical quality.sh with BMD_QUALITY_CONTRACT_FILE -> alternate benign contract
run_entrypoint_case "canonical quality.sh rejects BMD_QUALITY_CONTRACT_FILE alternate contract -> FAIL before image build" \
    "BMD_QUALITY_CONTRACT_FILE" \
    BMD_QUALITY_CONTRACT_FILE="$benign_contract" \
    -- "$repo_root/scripts/quality.sh"

# K. canonical quality.sh with the remaining documented test-only hooks
run_entrypoint_case "canonical quality.sh rejects BMD_QUALITY_TOOLCHAIN_DIR -> FAIL before image build" \
    "BMD_QUALITY_TOOLCHAIN_DIR" \
    BMD_QUALITY_TOOLCHAIN_DIR="$work_root/fake-tool-root" \
    -- "$repo_root/scripts/quality.sh"

run_entrypoint_case "canonical quality.sh rejects BMD_QUALITY_DPKG_INFO_DIR -> FAIL before image build" \
    "BMD_QUALITY_DPKG_INFO_DIR" \
    BMD_QUALITY_DPKG_INFO_DIR="$work_root/fake-dpkg-info" \
    -- "$repo_root/scripts/quality.sh"

# L. a future authoritative snapshot cannot be made acceptable by forging the
# reference time (the test-only hook itself is rejected)
run_entrypoint_case "forged BMD_QUALITY_REFERENCE_TIME cannot accept a future authoritative snapshot -> FAIL before image build" \
    "BMD_QUALITY_REFERENCE_TIME" \
    BMD_QUALITY_REFERENCE_TIME="20991231T000000Z" \
    -- "$temp_repo/scripts/quality.sh"

# M. a future authoritative repository snapshot fails against the real UTC
# clock, with no hook set, before image construction
run_entrypoint_case "future authoritative repository snapshot rejected by canonical quality.sh -> FAIL before image build" \
    "in the future" \
    -- "$temp_repo/scripts/quality.sh"

# N. a duplicate-key authoritative repository contract is rejected before any
# image-build command
run_entrypoint_case "duplicate-key authoritative contract rejected by canonical quality.sh -> FAIL before image build" \
    "duplicate key" \
    -- "$temp_repo_dup/scripts/quality.sh"

# ============================================================================
# Entrypoint adversarial: internal orchestration variables (FINALREREVIEW2-001)
# ============================================================================
# The container execution mode is NOT ambient-selectable; the source/work
# roots are NOT ambient-substitutable. The guard fails closed on every
# BMD_CANONICAL_QUALITY_* variable, so the previous forged-source bypass is
# structurally impossible.

# A. BMD_CANONICAL_QUALITY_CONTAINER=1 on an ordinary host
run_entrypoint_case "BMD_CANONICAL_QUALITY_CONTAINER=1 cannot enter internal mode -> FAIL before image build" \
    "BMD_CANONICAL_QUALITY_CONTAINER" \
    BMD_CANONICAL_QUALITY_CONTAINER=1 \
    -- "$repo_root/scripts/quality.sh"

# B. BMD_CANONICAL_QUALITY_SRC cannot substitute the canonical source root
run_entrypoint_case "BMD_CANONICAL_QUALITY_SRC cannot substitute canonical source -> FAIL before image build" \
    "BMD_CANONICAL_QUALITY_SRC" \
    BMD_CANONICAL_QUALITY_SRC="$work_root/alternate-src" \
    -- "$repo_root/scripts/quality.sh"

# C. BMD_CANONICAL_QUALITY_WORK cannot substitute the canonical work root
run_entrypoint_case "BMD_CANONICAL_QUALITY_WORK cannot substitute canonical work root -> FAIL before image build" \
    "BMD_CANONICAL_QUALITY_WORK" \
    BMD_CANONICAL_QUALITY_WORK="$work_root/alternate-work" \
    -- "$repo_root/scripts/quality.sh"

# D. all three variables together reproduce the previously exploitable shape
run_entrypoint_case "all BMD_CANONICAL_QUALITY_* together cannot reproduce the bypass -> FAIL before image build" \
    "BMD_CANONICAL_QUALITY" \
    BMD_CANONICAL_QUALITY_CONTAINER=1 \
    BMD_CANONICAL_QUALITY_SRC="$work_root/alternate-src" \
    BMD_CANONICAL_QUALITY_WORK="$work_root/alternate-work" \
    -- "$repo_root/scripts/quality.sh"

# E/F. an alternate source tree containing a forged quality.sh must NEVER
# execute and must NEVER emit a forged PASS
forged_src="$work_root/forged-src"
forged_exec_marker="$work_root/forged-executed.marker"
rm -rf "$forged_src"
rm -f "$forged_exec_marker"
mkdir -p "$forged_src/scripts"
cat > "$forged_src/scripts/quality.sh" <<EOF
#!/usr/bin/env bash
echo "FORGED SCRIPT EXECUTED" >> "$forged_exec_marker"
echo "CANONICAL QUALITY: PASS (forged)"
exit 0
EOF
chmod +x "$forged_src/scripts/quality.sh"
rm -f "$fake_docker_marker"
forged_out="$(PATH="$fake_docker_dir:$PATH" env \
    BMD_FAKE_DOCKER_MARKER="$fake_docker_marker" \
    BMD_CANONICAL_QUALITY_CONTAINER=1 \
    BMD_CANONICAL_QUALITY_SRC="$forged_src" \
    BMD_CANONICAL_QUALITY_WORK="$work_root/forged-work" \
    bash "$repo_root/scripts/quality.sh" 2>&1)" && forged_rc=0 || forged_rc=$?
forged_verdict="PASS"
[[ "$forged_rc" -ne 0 ]] || forged_verdict="FAIL(rc=0)"
[[ "$forged_out" == *"BMD_CANONICAL_QUALITY_CONTAINER"* ]] || forged_verdict="FAIL(no guard diagnostic)"
[[ ! -f "$forged_exec_marker" ]] || forged_verdict="FAIL(forged script executed)"
[[ "$forged_out" != *"CANONICAL QUALITY: PASS (forged)"* ]] || forged_verdict="FAIL(forged PASS emitted)"
[[ ! -f "$fake_docker_marker" ]] || forged_verdict="FAIL(image-build command reached)"
if [[ "$forged_verdict" == "PASS" ]]; then
    pass_count=$((pass_count + 1))
else
    fail_count=$((fail_count + 1))
    printf '%s\n' "$forged_out"
fi
printf '  [%s] %s\n' "$forged_verdict" \
    "forged alternate source never executes / never emits PASS (E, F, G)"

# H/I. manual direct invocation of the internal-only mode outside the
# canonical image (missing baked /opt/toolchain/quality.env) fails closed
# BEFORE any source copy or recursive execution
run_entrypoint_case "internal mode invoked outside canonical image (missing baked contract) -> FAIL before source copy" \
    "outside the canonical image" \
    -- "$repo_root/scripts/quality.sh" --inside-canonical-container

# K. BMD_QUALITY_WORK_KEEP is also rejected at the canonical entrypoint
run_entrypoint_case "canonical quality.sh rejects BMD_QUALITY_WORK_KEEP -> FAIL before image build" \
    "BMD_QUALITY_WORK_KEEP" \
    BMD_QUALITY_WORK_KEEP=".cache" \
    -- "$repo_root/scripts/quality.sh"

# ============================================================================
# Unit: canonical image boundary proof (scripts/quality-image-boundary.sh)
# ============================================================================
boundary_script="$repo_root/scripts/quality-image-boundary.sh"
boundary_baked="$work_root/boundary/baked/quality.env"
boundary_src_ok="$work_root/boundary/src-ok"
boundary_src_none="$work_root/boundary/src-none"
boundary_src_bad="$work_root/boundary/src-bad"
rm -rf "$work_root/boundary"
mkdir -p "$(dirname "$boundary_baked")" "$boundary_src_ok/.toolchain" "$boundary_src_none" "$boundary_src_bad/.toolchain"
cp "$repo_root/.toolchain/quality.env" "$boundary_baked"
cp "$repo_root/.toolchain/quality.env" "$boundary_src_ok/.toolchain/quality.env"
sed 's/^UBUNTU_SNAPSHOT_ID=.*/UBUNTU_SNAPSHOT_ID=20260813T120000Z/' \
    "$repo_root/.toolchain/quality.env" > "$boundary_src_bad/.toolchain/quality.env"

run_cmd_case "image boundary: matching baked and source contracts -> PASS" yes \
    bash "$boundary_script" "$boundary_baked" "$boundary_src_ok"

# I. missing baked contract -> FAIL closed before any source copy
run_cmd_case "image boundary: missing baked contract -> FAIL" no \
    bash "$boundary_script" "$work_root/boundary/missing/quality.env" "$boundary_src_ok"

run_cmd_case "image boundary: missing source contract -> FAIL" no \
    bash "$boundary_script" "$boundary_baked" "$boundary_src_none"

# J. baked/source contract mismatch -> FAIL closed
run_cmd_case "image boundary: baked/source contract mismatch -> FAIL" no \
    bash "$boundary_script" "$boundary_baked" "$boundary_src_bad"

run_cmd_case "image boundary: missing arguments -> FAIL" no \
    bash "$boundary_script"

# ============================================================================
# Unit: canonical runtime identity (scripts/quality-runtime-check.sh)
# ============================================================================
runtime_script="$repo_root/scripts/quality-runtime-check.sh"
empty_bin="$work_root/empty-bin"
mkdir -p "$empty_bin"

# a real Docker Engine server identity (Components array incl. Engine)
real_engine_json='{"Platform":{"Name":"Docker Engine - Community"},"Version":"27.5.1","ApiVersion":"1.47","MinAPIVersion":"1.24","Components":[{"Name":"Engine","Version":"27.5.1","Details":{"ApiVersion":"1.47","Os":"linux","Arch":"amd64"}},{"Name":"containerd","Version":"2.0.2","Details":{}},{"Name":"runc","Version":"1.2.5","Details":{}},{"Name":"docker-init","Version":"0.19.0","Details":{}}],"Os":"linux","Arch":"amd64"}'
make_fake_docker "$work_root/runtime-real" "$real_engine_json" 0

# a Podman/libpod-shaped server identity exposed through `docker` (no
# Components array, no Engine component)
podman_server_json='{"Client":{"Version":"5.3.2","ApiVersion":"5.3.2","GoVersion":"go1.23.0"},"Server":{"Version":"5.3.2","ApiVersion":"5.3.2","GoVersion":"go1.23.0","GitCommit":"9c7f8d1","Built":1735000000,"OsArch":"linux/amd64","Os":"linux","Arch":"amd64","BuildTime":"2025-01-01T00:00:00Z"}}'
make_fake_docker "$work_root/runtime-podman" "$podman_server_json" 0

# an unparseable/unknown backend
make_fake_docker "$work_root/runtime-garbage" "docker version: garbage from an unknown backend" 0

# a daemon that is unreachable (docker version fails)
make_fake_docker "$work_root/runtime-dead" "" 1

run_cmd_case "runtime: real Docker Engine server identity accepted -> PASS" yes \
    bash -c "PATH='$work_root/runtime-real:$PATH' bash '$runtime_script'"

run_cmd_case "runtime: Podman/libpod backend exposed as docker rejected -> FAIL" no \
    bash -c "PATH='$work_root/runtime-podman:$PATH' bash '$runtime_script'"

run_cmd_case "runtime: unknown/unparseable backend rejected -> FAIL" no \
    bash -c "PATH='$work_root/runtime-garbage:$PATH' bash '$runtime_script'"

run_cmd_case "runtime: docker daemon unavailable rejected -> FAIL" no \
    bash -c "PATH='$work_root/runtime-dead:$PATH' bash '$runtime_script'"

run_cmd_case "runtime: no docker on PATH rejected -> FAIL" no \
    bash -c "PATH='$empty_bin' '$bash_bin' '$runtime_script'"

echo
if [[ "$fail_count" -eq 0 ]]; then
    echo "QUALITY TOOLCHAIN TESTS: PASS ($pass_count cases)"
else
    echo "QUALITY TOOLCHAIN TESTS: FAIL ($fail_count failing, $pass_count passing)"
    exit 1
fi
