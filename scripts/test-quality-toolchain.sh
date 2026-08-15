#!/usr/bin/env bash
# test-quality-toolchain.sh — deterministic tests for the canonical Quality
# toolchain contract (INFRA-TC-001).
#
# Proves fail-closed behavior of scripts/quality-toolchain-check.sh:
#   - exact tool versions -> PASS
#   - wrong clang-tidy version -> FAIL
#   - wrong clang-format version -> FAIL
#   - missing required tool -> FAIL
#   - malformed toolchain contract -> FAIL
#   - AppleClang resolution -> FAIL
#   - PATH selecting a non-canonical installation -> FAIL
#   - Conan mismatch against requirements-tools.txt -> FAIL
#   - CMake/Python below contract floors -> FAIL
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

tree() { printf '%s/case-%s' "$work_root" "$1"; }

rm -rf "$work_root"
mkdir -p "$work_root"
cd "$work_root"

echo "quality-toolchain tests (contract: clang $llvm_version, conan $conan_version)"

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

echo
if [[ "$fail_count" -eq 0 ]]; then
    echo "QUALITY TOOLCHAIN TESTS: PASS ($pass_count cases)"
else
    echo "QUALITY TOOLCHAIN TESTS: FAIL ($fail_count failing, $pass_count passing)"
    exit 1
fi
