#!/usr/bin/env bash
# test-benchmark-allocation-formal.sh — deterministic trust-boundary tests
# for the Phase-7 formal Release runner (OD-M5-P7-023).
#
# Categories (all offline; fake tool shims and synthetic inputs are generated
# from the repository's own scripts, never from network state):
#   entrypoint adversarial: the PRODUCTION public entrypoint
#     (scripts/benchmark-allocation-formal.sh) rejects unknown/trailing
#     arguments, caller source/work/image/command selectors, ambient
#     internal/source/work selection variables, and direct internal-mode
#     host invocation — BEFORE any container-runtime invocation (proved with
#     a fake docker shim recording invocations);
#   provenance: the production scripts/formal-source-provenance.sh proves
#     clean HEAD equality and rejects dirty (tracked/index/worktree/
#     untracked), unexpected-HEAD, non-repository, and read-only violations;
#   build contract: the production scripts/formal-build-contract-check.sh
#     rejects non-Release, sanitizer-enabled, unrecorded/non-OFF LTO,
#     non-/src CMake source roots, and missing-requirement build caches;
#   validator-boundary: the existing independent validator rejects
#     exploratory output in a formal context (no --allow-exploratory),
#     incorrect binary SHA binding, dirty formal sources, and missing
#     required inventory — and accepts a valid clean formal pair;
#   structural: single formal source model (/src), fixed build root, no
#     source copy into /work, accepted INFRA-TC helper reuse, no canonical
#     Quality verdict emission, quality.sh semantics untouched.
#
# No test weakens the production trust boundary: every case exercises the
# production scripts themselves with fake inputs.
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
formal_script="$repo_root/scripts/benchmark-allocation-formal.sh"
provenance_script="$repo_root/scripts/formal-source-provenance.sh"
build_contract_script="$repo_root/scripts/formal-build-contract-check.sh"
work_root="$repo_root/build/formal-runner-tests"

pass_count=0
fail_count=0

bash_bin="$(command -v bash)"

make_fake_docker() {
    # A fake `docker` command: `docker version --format` prints the given
    # server JSON (real Docker Engine shape), everything else records the
    # invocation in $BMD_FAKE_DOCKER_MARKER. Mirrors the accepted
    # test-quality-toolchain.sh harness.
    local dir="$1"
    mkdir -p "$dir"
    printf '%s\n' \
        '{"Platform":{"Name":"Docker Engine - Community"},"Version":"27.5.1","ApiVersion":"1.47","MinAPIVersion":"1.24","Components":[{"Name":"Engine","Version":"27.5.1","Details":{"ApiVersion":"1.47","Os":"linux","Arch":"amd64"}},{"Name":"containerd","Version":"2.0.2","Details":{}},{"Name":"runc","Version":"1.2.5","Details":{}},{"Name":"docker-init","Version":"0.19.0","Details":{}}],"Os":"linux","Arch":"amd64"}' \
        > "$dir/version-server.json"
    cat > "$dir/docker" <<EOF
#!/usr/bin/env bash
if [[ "\${1:-}" == "version" ]]; then
    cat "$dir/version-server.json"
    exit 0
fi
echo "FAKE DOCKER INVOKED: \$*" >> "\${BMD_FAKE_DOCKER_MARKER}"
exit 0
EOF
    chmod +x "$dir/docker"
}

make_podman_docker() {
    # A Podman/libpod-shaped server identity exposed through a docker
    # command (no Components array, no Engine component).
    local dir="$1"
    mkdir -p "$dir"
    printf '%s\n' \
        '{"Client":{"Version":"5.3.2","ApiVersion":"5.3.2","GoVersion":"go1.23.0"},"Server":{"Version":"5.3.2","ApiVersion":"5.3.2","GoVersion":"go1.23.0","GitCommit":"9c7f8d1","Built":1735000000,"OsArch":"linux/amd64","Os":"linux","Arch":"amd64","BuildTime":"2025-01-01T00:00:00Z"}}' \
        > "$dir/version-server.json"
    cat > "$dir/docker" <<EOF
#!/usr/bin/env bash
if [[ "\${1:-}" == "version" ]]; then
    cat "$dir/version-server.json"
    exit 0
fi
echo "FAKE DOCKER INVOKED: \$*" >> "\${BMD_FAKE_DOCKER_MARKER}"
exit 0
EOF
    chmod +x "$dir/docker"
}

make_git_repo() {
    # A minimal real git repository with one committed file (deterministic
    # git identity for provenance tests).
    local dir="$1"
    rm -rf "$dir"
    mkdir -p "$dir"
    git -C "$dir" init -q
    git -C "$dir" config user.name test
    git -C "$dir" config user.email test@example.invalid
    printf 'content\n' > "$dir/file.txt"
    git -C "$dir" add file.txt
    git -C "$dir" commit -qm "initial"
}

run_entrypoint_case() {
    # run_entrypoint_case <name> <required-diagnostic-regex> <env...> -- <args...>
    # PASS requires: nonzero exit, the diagnostic, and NO container-runtime
    # invocation recorded by the fake docker shim (the failure happened
    # before any image-build command).
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
        "${envs[@]+"${envs[@]}"}" "$bash_bin" "${args[@]}" 2>&1)" && rc=0 || rc=$?
    if [[ "$rc" -ne 0 && "$out" =~ $diag_re && ! -f "$fake_docker_marker" ]]; then
        printf '  [PASS] %s\n' "$name"
        pass_count=$((pass_count + 1))
    else
        printf '  [FAIL] %s (rc=%s, build reached: %s)\n' "$name" "$rc" \
            "$([[ -f "$fake_docker_marker" ]] && echo YES || echo NO)"
        printf '%s\n' "$out"
        fail_count=$((fail_count + 1))
    fi
}

run_cmd_case() {
    # run_cmd_case <name> <expected_exit_zero:yes|no> <cmd...>
    local name="$1" want_ok="$2"
    shift 2
    local out rc
    out="$("$@" 2>&1)" && rc=0 || rc=$?
    if [[ "$want_ok" == "yes" && "$rc" -eq 0 ]]; then
        printf '  [PASS] %s\n' "$name"
        pass_count=$((pass_count + 1))
    elif [[ "$want_ok" == "no" && "$rc" -ne 0 ]]; then
        printf '  [PASS] %s\n' "$name"
        pass_count=$((pass_count + 1))
    else
        printf '  [FAIL] %s (expected exit %s, got %s)\n' "$name" \
            "$([[ "$want_ok" == "yes" ]] && echo zero || echo nonzero)" "$rc"
        printf '%s\n' "$out"
        fail_count=$((fail_count + 1))
    fi
}

tree() { printf '%s/case-%s' "$work_root" "$1"; }

rm -rf "$work_root"
mkdir -p "$work_root"

echo "Phase-7 formal runner tests"

# ============================================================================
# Entrypoint adversarial (production public entrypoint, fake docker shim)
# ============================================================================
fake_docker_dir="$work_root/fake-docker-bin"
fake_docker_marker="$work_root/fake-docker-invoked.marker"
make_fake_docker "$fake_docker_dir"

# Unknown / trailing arguments are rejected (exit 2) before any formal work.
run_entrypoint_case "formal runner rejects unknown argument -> FAIL before container" \
    "usage" -- "$formal_script" garbage

run_entrypoint_case "formal runner rejects trailing argument -> FAIL before container" \
    "usage" -- "$formal_script" foo bar

# Caller source/work/image/command/mode selectors are rejected.
run_entrypoint_case "caller source-root selector rejected -> FAIL before container" \
    "usage" -- "$formal_script" --source /tmp/elsewhere

run_entrypoint_case "caller work-root selector rejected -> FAIL before container" \
    "usage" -- "$formal_script" --work /tmp/elsewhere

run_entrypoint_case "caller image selector rejected -> FAIL before container" \
    "usage" -- "$formal_script" --image hostile:latest

run_entrypoint_case "caller command selector rejected -> FAIL before container" \
    "usage" -- "$formal_script" --command "rm -rf /"

run_entrypoint_case "caller internal selector rejected -> FAIL before container" \
    "usage" -- "$formal_script" --internal

run_entrypoint_case "caller container-mode selector rejected -> FAIL before container" \
    "usage" -- "$formal_script" --container-mode

# Direct internal-mode host invocation is rejected (wrong layout; the
# internal mode is entered only through the fixed private host protocol).
run_entrypoint_case "direct internal-mode host invocation rejected -> FAIL before container" \
    "canonical image layout" -- "$formal_script" --formal-inside-container \
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"

run_entrypoint_case "internal protocol with extra arguments rejected -> FAIL before container" \
    "usage" -- "$formal_script" --formal-inside-container \
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" extra

# Ambient internal/source/work selection variables are rejected.
run_entrypoint_case "ambient BMD_P7_FORMAL_INTERNAL rejected -> FAIL before container" \
    "BMD_P7_FORMAL_INTERNAL" \
    BMD_P7_FORMAL_INTERNAL=1 -- "$formal_script"

run_entrypoint_case "ambient BMD_P7_FORMAL_SOURCE_SHA rejected -> FAIL before container" \
    "BMD_P7_FORMAL_SOURCE_SHA" \
    BMD_P7_FORMAL_SOURCE_SHA="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" \
    -- "$formal_script"

run_entrypoint_case "ambient BMD_P7_FORMAL_SRC rejected -> FAIL before container" \
    "BMD_P7_FORMAL_SRC" \
    BMD_P7_FORMAL_SRC="$work_root/alternate-src" -- "$formal_script"

run_entrypoint_case "ambient BMD_P7_FORMAL_WORK rejected -> FAIL before container" \
    "BMD_P7_FORMAL_WORK" \
    BMD_P7_FORMAL_WORK="$work_root/alternate-work" -- "$formal_script"

# Quality-contract hook and orchestration variables are rejected for the
# same reason scripts/quality.sh rejects them (the runner consumes the SAME
# accepted helpers and must never be pointed at another contract).
run_entrypoint_case "ambient BMD_QUALITY_CONTRACT_FILE rejected -> FAIL before container" \
    "BMD_QUALITY_CONTRACT_FILE" \
    BMD_QUALITY_CONTRACT_FILE="$work_root/alternate-contract" -- "$formal_script"

run_entrypoint_case "ambient BMD_CANONICAL_QUALITY_CONTAINER rejected -> FAIL before container" \
    "BMD_CANONICAL_QUALITY_CONTAINER" \
    BMD_CANONICAL_QUALITY_CONTAINER=1 -- "$formal_script"

run_entrypoint_case "ambient BMD_CANONICAL_QUALITY_SRC rejected -> FAIL before container" \
    "BMD_CANONICAL_QUALITY_SRC" \
    BMD_CANONICAL_QUALITY_SRC="$work_root/alternate-src" -- "$formal_script"

run_entrypoint_case "ambient BMD_CANONICAL_QUALITY_WORK rejected -> FAIL before container" \
    "BMD_CANONICAL_QUALITY_WORK" \
    BMD_CANONICAL_QUALITY_WORK="$work_root/alternate-work" -- "$formal_script"

# A valid host-mode invocation reaches the runtime check; a Podman backend
# must be rejected with the SAME accepted Docker Engine rule (and must never
# reach an image build).
podman_dir="$work_root/fake-podman-bin"
podman_marker="$work_root/fake-podman-invoked.marker"
make_podman_docker "$podman_dir"
rm -f "$podman_marker"
podman_out="$(PATH="$podman_dir:$PATH" env BMD_FAKE_DOCKER_MARKER="$podman_marker" \
    "$bash_bin" "$formal_script" 2>&1)" && podman_rc=0 || podman_rc=$?
if [[ "$podman_rc" -ne 0 && "$podman_out" == *"not a Docker Engine backend"* \
    && ! -f "$podman_marker" ]]; then
    printf '  [PASS] %s\n' \
        "Podman/libpod backend rejected by the accepted runtime rule -> FAIL before image build"
    pass_count=$((pass_count + 1))
else
    printf '  [FAIL] %s (rc=%s, build reached: %s)\n' \
        "Podman/libpod backend rejected by the accepted runtime rule" "$podman_rc" \
        "$([[ -f "$podman_marker" ]] && echo YES || echo NO)"
    printf '%s\n' "$podman_out"
    fail_count=$((fail_count + 1))
fi

# The accepted runtime rule itself: a real Docker Engine backend is accepted
# and the Podman-shaped backend is rejected (the exact SAME production helper
# the formal runner consumes; deterministic regardless of checkout state).
runtime_check="$repo_root/scripts/quality-runtime-check.sh"
run_cmd_case "runtime rule: real Docker Engine backend accepted -> PASS" yes \
    "$bash_bin" -c "PATH='$fake_docker_dir:$PATH' '$bash_bin' '$runtime_check'"

run_cmd_case "runtime rule: Podman/libpod backend rejected -> FAIL" no \
    "$bash_bin" -c "PATH='$podman_dir:$PATH' '$bash_bin' '$runtime_check'"

# ============================================================================
# Production provenance helper (real git repositories)
# ============================================================================
provenance_sha() {
    git -C "$1" rev-parse HEAD
}

make_git_repo "$(tree prov-clean)"
clean_sha="$(provenance_sha "$(tree prov-clean)")"

run_cmd_case "provenance: clean repo with expected HEAD -> PASS" yes \
    "$bash_bin" "$provenance_script" "$(tree prov-clean)" "$clean_sha"

run_cmd_case "provenance: clean repo with --require-read-only on writable root -> FAIL" no \
    "$bash_bin" "$provenance_script" "$(tree prov-clean)" "$clean_sha" --require-read-only

# Dirty: untracked file -> rejected BEFORE configure
make_git_repo "$(tree prov-untracked)"
printf 'untracked\n' > "$(tree prov-untracked)/untracked.txt"
run_cmd_case "provenance: dirty /src (untracked file) -> FAIL" no \
    "$bash_bin" "$provenance_script" "$(tree prov-untracked)" \
    "$(provenance_sha "$(tree prov-untracked)")"

# Dirty: modified tracked file -> rejected
make_git_repo "$(tree prov-modified)"
printf 'changed\n' > "$(tree prov-modified)/file.txt"
run_cmd_case "provenance: dirty /src (modified tracked file) -> FAIL" no \
    "$bash_bin" "$provenance_script" "$(tree prov-modified)" \
    "$(provenance_sha "$(tree prov-modified)")"

# Dirty: staged change -> rejected
make_git_repo "$(tree prov-staged)"
printf 'staged\n' > "$(tree prov-staged)/file.txt"
git -C "$(tree prov-staged)" add file.txt
run_cmd_case "provenance: dirty /src (staged change) -> FAIL" no \
    "$bash_bin" "$provenance_script" "$(tree prov-staged)" \
    "$(provenance_sha "$(tree prov-staged)")"

# Unexpected HEAD -> rejected BEFORE configure
make_git_repo "$(tree prov-head)"
run_cmd_case "provenance: unexpected HEAD(/src) -> FAIL" no \
    "$bash_bin" "$provenance_script" "$(tree prov-head)" \
    "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"

# Not a repository -> rejected
mkdir -p "$(tree prov-norepo)"
printf 'not a repo\n' > "$(tree prov-norepo)/file.txt"
run_cmd_case "provenance: non-repository root -> FAIL" no \
    "$bash_bin" "$provenance_script" "$(tree prov-norepo)" \
    "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"

# Missing .git -> rejected
make_git_repo "$(tree prov-nogit)"
rm -rf "$(tree prov-nogit)/.git"
run_cmd_case "provenance: missing .git -> FAIL" no \
    "$bash_bin" "$provenance_script" "$(tree prov-nogit)" \
    "$clean_sha"

# Malformed expected SHA -> rejected
run_cmd_case "provenance: malformed expected SHA -> FAIL" no \
    "$bash_bin" "$provenance_script" "$(tree prov-clean)" "not-a-sha"

# Read-only positive proof (deterministic for non-root test runs; root
# bypasses directory write permissions, so the case is reported as SKIPPED
# there rather than faking a proof).
make_git_repo "$(tree prov-ro)"
chmod a-w "$(tree prov-ro)"
ro_out=""
ro_rc=0
if [[ "$(id -u)" == "0" ]]; then
    printf '  [SKIP] %s (running as root)\n' \
        "provenance: read-only root accepted with --require-read-only"
else
    ro_out="$("$bash_bin" "$provenance_script" "$(tree prov-ro)" \
        "$(provenance_sha "$(tree prov-ro)")" --require-read-only 2>&1)" && ro_rc=0 || ro_rc=$?
    if [[ "$ro_rc" -eq 0 && "$ro_out" == *"read-only"* ]]; then
        printf '  [PASS] %s\n' "provenance: read-only root accepted with --require-read-only"
        pass_count=$((pass_count + 1))
    else
        printf '  [FAIL] %s (rc=%s)\n' \
            "provenance: read-only root accepted with --require-read-only" "$ro_rc"
        printf '%s\n' "$ro_out"
        fail_count=$((fail_count + 1))
    fi
fi
chmod u+w "$(tree prov-ro)"

# ============================================================================
# Production build-contract helper (synthetic CMakeCache.txt trees)
# ============================================================================
make_cache() {
    local dir="$1" mutate="$2"
    rm -rf "$dir"
    mkdir -p "$dir"
    cat > "$dir/CMakeCache.txt" <<EOF
CMAKE_HOME_DIRECTORY:INTERNAL=/src
CMAKE_BUILD_TYPE:STRING=Release
CMAKE_INTERPROCEDURAL_OPTIMIZATION:UNINITIALIZED=OFF
BMD_PROJECTION_ENABLE_ASAN:BOOL=OFF
BMD_PROJECTION_ENABLE_UBSAN:BOOL=OFF
BMD_PROJECTION_ENABLE_TSAN:BOOL=OFF
BMD_PROJECTION_ENABLE_COVERAGE:BOOL=OFF
BMD_PROJECTION_BUILD_BENCHMARKS:BOOL=ON
BMD_PROJECTION_BUILD_PROTO_ADAPTER:STRING=ON
BMD_PROJECTION_BUILD_TESTS:BOOL=OFF
EOF
    case "$mutate" in
        none) ;;
        debug)
            sed -i '' 's/^CMAKE_BUILD_TYPE:STRING=Release$/CMAKE_BUILD_TYPE:STRING=Debug/' \
                "$dir/CMakeCache.txt"
            ;;
        asan)
            sed -i '' 's/^BMD_PROJECTION_ENABLE_ASAN:BOOL=OFF$/BMD_PROJECTION_ENABLE_ASAN:BOOL=ON/' \
                "$dir/CMakeCache.txt"
            ;;
        ubsan)
            sed -i '' 's/^BMD_PROJECTION_ENABLE_UBSAN:BOOL=OFF$/BMD_PROJECTION_ENABLE_UBSAN:BOOL=ON/' \
                "$dir/CMakeCache.txt"
            ;;
        tsan)
            sed -i '' 's/^BMD_PROJECTION_ENABLE_TSAN:BOOL=OFF$/BMD_PROJECTION_ENABLE_TSAN:BOOL=ON/' \
                "$dir/CMakeCache.txt"
            ;;
        coverage)
            sed -i '' 's/^BMD_PROJECTION_ENABLE_COVERAGE:BOOL=OFF$/BMD_PROJECTION_ENABLE_COVERAGE:BOOL=ON/' \
                "$dir/CMakeCache.txt"
            ;;
        lto-on)
            sed -i '' 's/^CMAKE_INTERPROCEDURAL_OPTIMIZATION:UNINITIALIZED=OFF$/CMAKE_INTERPROCEDURAL_OPTIMIZATION:UNINITIALIZED=ON/' \
                "$dir/CMakeCache.txt"
            ;;
        lto-missing)
            grep -v '^CMAKE_INTERPROCEDURAL_OPTIMIZATION' "$dir/CMakeCache.txt" \
                > "$dir/CMakeCache.txt.tmp" && mv "$dir/CMakeCache.txt.tmp" "$dir/CMakeCache.txt"
            ;;
        asan-missing)
            grep -v '^BMD_PROJECTION_ENABLE_ASAN' "$dir/CMakeCache.txt" \
                > "$dir/CMakeCache.txt.tmp" && mv "$dir/CMakeCache.txt.tmp" "$dir/CMakeCache.txt"
            ;;
        home)
            sed -i '' 's|^CMAKE_HOME_DIRECTORY:INTERNAL=/src$|CMAKE_HOME_DIRECTORY:INTERNAL=/elsewhere|' \
                "$dir/CMakeCache.txt"
            ;;
        no-benchmarks)
            sed -i '' 's/^BMD_PROJECTION_BUILD_BENCHMARKS:BOOL=ON$/BMD_PROJECTION_BUILD_BENCHMARKS:BOOL=OFF/' \
                "$dir/CMakeCache.txt"
            ;;
        no-adapter)
            sed -i '' 's/^BMD_PROJECTION_BUILD_PROTO_ADAPTER:STRING=ON$/BMD_PROJECTION_BUILD_PROTO_ADAPTER:STRING=OFF/' \
                "$dir/CMakeCache.txt"
            ;;
        tests-on)
            sed -i '' 's/^BMD_PROJECTION_BUILD_TESTS:BOOL=OFF$/BMD_PROJECTION_BUILD_TESTS:BOOL=ON/' \
                "$dir/CMakeCache.txt"
            ;;
        *) echo "unknown mutate: $mutate" >&2; exit 2 ;;
    esac
}

make_cache "$(tree bc-ok)" none
run_cmd_case "build contract: Release/sanitizers off/LTO off//src -> PASS" yes \
    "$bash_bin" "$build_contract_script" "$(tree bc-ok)"

for mutation in debug asan ubsan tsan coverage lto-on lto-missing asan-missing home no-benchmarks no-adapter tests-on; do
    make_cache "$(tree "bc-$mutation")" "$mutation"
    run_cmd_case "build contract: $mutation -> FAIL" no \
        "$bash_bin" "$build_contract_script" "$(tree "bc-$mutation")"
done

run_cmd_case "build contract: missing cache file -> FAIL" no \
    "$bash_bin" "$build_contract_script" "$work_root/bc-missing"

# ============================================================================
# Independent validator boundary (formal context, binary SHA, inventory)
# ============================================================================
validator_py="$repo_root/scripts/benchmark_phase7.py"
validator_case="$(tree validator)"
mkdir -p "$validator_case"
python3 - "$validator_case" "$validator_py" <<'PY'
import hashlib
import json
import os
import sys

repo = os.path.dirname(os.path.dirname(os.path.abspath(sys.argv[2])))
sys.path.insert(0, repo)
sys.path.insert(0, os.path.join(repo, "tests", "m5", "benchmark"))
import scripts.benchmark_phase7 as phase7  # type: ignore
import test_phase7_validators as tv  # type: ignore

case_dir = sys.argv[1]


def write_pair(payload, wrapper, payload_path, wrapper_path):
    with open(payload_path, "w", encoding="utf-8") as stream:
        text = json.dumps(payload)
        stream.write(text)
        wrapper["result_payload"]["sha256"] = hashlib.sha256(
            text.encode("utf-8")).hexdigest()
    with open(wrapper_path, "w", encoding="utf-8") as stream:
        json.dump(wrapper, stream)


def make_pair(evidence_class="exploratory", dirty=False, sha="a" * 40):
    spec = ("benchmark_name=M2/best_bid/8\ndepth_per_side=8\n"
            "generated_workload_sha256=d" + "e" * 63 +
            "\ngenerator_schema=M5_PHASE6_M2_CELLS_V1\noperation=best_bid\n"
            "seed=not_applicable\n")
    workload = tv._workload_identity("M2/best_bid/8", spec)
    measurement = tv._eligible_measurement(1000, 1000, 1000, 0, 0, 0, 0)
    record = tv._record("M2/best_bid/8", workload, measurement)
    record["evidence_class"] = evidence_class
    tv._bind_result_sha(record)
    payload = {
        "schema": phase7.PAYLOAD_SCHEMA,
        "measurement_contract_version": phase7.MEASUREMENT_CONTRACT,
        "record_count": 1,
        "calibration_record_count": 1,
        "calibration_records": [dict(tv._calibration(), evidence_class=evidence_class)],
        "records": [record],
    }
    payload_path = os.path.join(case_dir, f"payload-{evidence_class}.json")
    wrapper_path = os.path.join(case_dir, f"wrapper-{evidence_class}.json")
    wrapper = tv._wrapper(payload_path, "")
    wrapper["workload_identities"] = [workload]
    wrapper["evidence_class"] = evidence_class
    wrapper["requested_evidence_class"] = evidence_class
    wrapper["evidence_class_downgrade_reason"] = None
    wrapper["source_provenance"] = {"git_sha": sha, "status": "known",
                                    "dirty_at_configure": dirty}
    write_pair(payload, wrapper, payload_path, wrapper_path)
    return payload_path, wrapper_path


def run_validation(payload_path, wrapper_path, allow=True, binary=None,
                   inventory=None):
    phase7.main_run_for_test(payload_path, wrapper_path,
                             require_inventory=inventory, binary=binary,
                             allow_exploratory=allow)


results = []

# 1. exploratory evidence in a formal context (no --allow-exploratory) -> FAIL
payload_path, wrapper_path = make_pair("exploratory")
try:
    run_validation(payload_path, wrapper_path, allow=False)
    results.append(("validator: exploratory evidence without --allow-exploratory -> FAIL", False))
except phase7.ValidationError as error:
    results.append(("validator: exploratory evidence without --allow-exploratory -> FAIL",
                    "exploratory" in str(error)))

# 2. same evidence with the explicit exploratory policy -> PASS (control)
try:
    run_validation(payload_path, wrapper_path, allow=True)
    results.append(("validator: exploratory evidence with --allow-exploratory -> PASS (control)",
                    True))
except phase7.ValidationError as error:
    results.append(("validator: exploratory evidence with --allow-exploratory -> PASS (control)",
                    False))

# 3. incorrect binary SHA binding -> evidence rejected. The wrapper binary
#    identity and the record provenance binary block are mutated together so
#    the existing wrapper<->record identity binding (PRB-003) stays intact;
#    only the --binary rehash outcome differs between cases.
binary_a = os.path.join(case_dir, "binary-a")
binary_b = os.path.join(case_dir, "binary-b")
binary_a_copy = os.path.join(case_dir, "binary-a-copy")
with open(binary_a, "wb") as stream:
    stream.write(b"formal-binary-content-A")
with open(binary_b, "wb") as stream:
    stream.write(b"different-binary-content")
with open(binary_a_copy, "wb") as stream:
    stream.write(b"formal-binary-content-A")


def bind_binary_identity(binary_path, content):
    payload_path, wrapper_path = make_pair("exploratory")
    with open(wrapper_path, encoding="utf-8") as stream:
        wrapper = json.load(stream)
    with open(payload_path, encoding="utf-8") as stream:
        payload = json.load(stream)
    identity = {"path": binary_path, "sha256": hashlib.sha256(content).hexdigest()}
    wrapper["binary_provenance"] = dict(identity)
    payload["records"][0]["provenance"]["binary"] = dict(identity)
    with open(payload_path, "w", encoding="utf-8") as stream:
        text = json.dumps(payload)
        stream.write(text)
        wrapper["result_payload"]["sha256"] = hashlib.sha256(
            text.encode("utf-8")).hexdigest()
    with open(wrapper_path, "w", encoding="utf-8") as stream:
        json.dump(wrapper, stream)
    return payload_path, wrapper_path


payload_path, wrapper_path = bind_binary_identity(
    binary_a, b"formal-binary-content-A")
try:
    run_validation(payload_path, wrapper_path, allow=True, binary=binary_a)
    results.append(("validator: exact binary SHA binding -> PASS", True))
except phase7.ValidationError:
    results.append(("validator: exact binary SHA binding -> PASS", False))
try:
    run_validation(payload_path, wrapper_path, allow=True, binary=binary_b)
    results.append(("validator: incorrect binary SHA binding -> FAIL", False))
except phase7.ValidationError as error:
    results.append(("validator: incorrect binary SHA binding -> FAIL",
                    "mismatch" in str(error)))
try:
    run_validation(payload_path, wrapper_path, allow=True, binary=binary_a_copy)
    results.append(("validator: identical-content binary at another path -> PASS", True))
except phase7.ValidationError:
    results.append(("validator: identical-content binary at another path -> PASS", False))

# 4. missing required inventory -> rejected
try:
    run_validation(payload_path, wrapper_path, allow=True, inventory="m2_m3")
    results.append(("validator: missing required inventory -> FAIL", False))
except phase7.ValidationError as error:
    results.append(("validator: missing required inventory -> FAIL",
                    "inventory" in str(error)))

# 5. dirty source cannot produce formal evidence -> rejected
payload_path, wrapper_path = make_pair("formal", dirty=True)
try:
    run_validation(payload_path, wrapper_path, allow=True)
    results.append(("validator: dirty source with formal class -> FAIL", False))
except phase7.ValidationError as error:
    results.append(("validator: dirty source with formal class -> FAIL",
                    "dirty" in str(error)))

# 6. clean formal pair validates without any exploratory allowance -> PASS
payload_path, wrapper_path = make_pair("formal")
try:
    run_validation(payload_path, wrapper_path, allow=False)
    results.append(("validator: clean formal pair (no --allow-exploratory) -> PASS", True))
except phase7.ValidationError as error:
    results.append(("validator: clean formal pair (no --allow-exploratory) -> PASS", False))

failed = [name for name, ok in results if not ok]
for name, ok in results:
    print(("PASS: " if ok else "FAIL: ") + name)
sys.exit(1 if failed else 0)
PY
if [[ $? -eq 0 ]]; then
    pass_count=$((pass_count + 6))
    printf '  [PASS] validator-boundary python cases (6/6)\n'
else
    fail_count=$((fail_count + 1))
    printf '  [FAIL] validator-boundary python cases\n'
fi

# ============================================================================
# Structural: single formal source model, helper reuse, quality separation
# ============================================================================
run_cmd_case "structural: cmake -S /src is the sole formal source root -> PASS" yes \
    "$bash_bin" -c "grep -q 'cmake -S \"\$src\" -B \"\$work/formal-build\"' '$formal_script'"

run_cmd_case "structural: build root is the fixed /work/formal-build -> PASS" yes \
    "$bash_bin" -c "grep -q '\"\$work/formal-build\"' '$formal_script'"

run_cmd_case "structural: fixed private internal protocol present -> PASS" yes \
    "$bash_bin" -c "grep -q -- '--formal-inside-container' '$formal_script'"

run_cmd_case "structural: no source copy/materialize/archive into /work -> PASS" yes \
    "$bash_bin" -c "! grep -qE 'cp -a|rsync|tar -C|git archive' '$formal_script'"

run_cmd_case "structural: accepted image-boundary helper reused -> PASS" yes \
    "$bash_bin" -c "grep -q 'quality-image-boundary.sh' '$formal_script'"

run_cmd_case "structural: accepted base-reference helper reused -> PASS" yes \
    "$bash_bin" -c "grep -q 'quality-base-ref.sh' '$formal_script'"

run_cmd_case "structural: accepted runtime-check helper reused -> PASS" yes \
    "$bash_bin" -c "grep -q 'quality-runtime-check.sh' '$formal_script'"

run_cmd_case "structural: accepted toolchain-check helper reused -> PASS" yes \
    "$bash_bin" -c "grep -q 'quality-toolchain-check.sh' '$formal_script'"

run_cmd_case "structural: authoritative contract consumed (no duplicate identity) -> PASS" yes \
    "$bash_bin" -c "grep -q '.toolchain/quality.env' '$formal_script'"

run_cmd_case "structural: independent validator invoked, not duplicated -> PASS" yes \
    "$bash_bin" -c "grep -q 'benchmark_phase7.py' '$formal_script' \
        && ! grep -q 'M5_PHASE7_MEASUREMENT_CONTRACT_V1' '$formal_script'"

run_cmd_case "structural: no canonical Quality verdict emission -> PASS" yes \
    "$bash_bin" -c "! grep -q 'CANONICAL QUALITY' '$formal_script' \
        && ! grep -q 'CANONICAL QUALITY' '$provenance_script' \
        && ! grep -q 'CANONICAL QUALITY' '$build_contract_script'"

run_cmd_case "structural: quality.sh semantics untouched by the runner -> PASS" yes \
    "$bash_bin" -c "! grep -q 'benchmark-allocation-formal' '$repo_root/scripts/quality.sh' \
        && ! grep -q 'formal-inside-container' '$repo_root/scripts/quality.sh'"

run_cmd_case "structural: exploratory driver remains the only other alloc driver -> PASS" yes \
    "$bash_bin" -c "grep -q 'EXPLORATORY ONLY' '$repo_root/scripts/benchmark-allocation.sh'"

run_cmd_case "structural: provenance helper never repairs/mutates source -> PASS" yes \
    "$bash_bin" -c "! grep -qE 'git (stash|clean|reset|checkout|commit)' '$provenance_script'"

run_cmd_case "structural: scripts parse under bash -> PASS" yes \
    "$bash_bin" -c "'$bash_bin' -n '$formal_script' && '$bash_bin' -n '$provenance_script' \
        && '$bash_bin' -n '$build_contract_script'"

echo
if [[ "$fail_count" -eq 0 ]]; then
    echo "PHASE-7 FORMAL RUNNER TESTS: PASS ($pass_count cases)"
else
    echo "PHASE-7 FORMAL RUNNER TESTS: FAIL ($fail_count failing, $pass_count passing)"
    exit 1
fi
