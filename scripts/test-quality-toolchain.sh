#!/usr/bin/env bash
# test-quality-toolchain.sh — deterministic tests for the canonical Quality
# toolchain contract (INFRA-TC-001).
#
# Categories:
#   unit (offline): exact/missing/wrong tool identity, malformed contract,
#     AppleClang rejection, wrong-PATH rejection, conan/cmake/python floors,
#     dpkg package provenance, UBUNTU_SNAPSHOT_ID validation, base-reference
#     plumbing, snapshot apt-sources generation, Dockerfile base binding.
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

echo
if [[ "$fail_count" -eq 0 ]]; then
    echo "QUALITY TOOLCHAIN TESTS: PASS ($pass_count cases)"
else
    echo "QUALITY TOOLCHAIN TESTS: FAIL ($fail_count failing, $pass_count passing)"
    exit 1
fi
