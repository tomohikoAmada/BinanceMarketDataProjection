#!/usr/bin/env bash
# benchmark-allocation-formal.sh — the Phase-7 canonical formal Release
# runner (OD-M5-P7-023). This is the SOLE repository-owned public host
# command that may produce formal Phase-7 allocation/memory evidence.
#
#   bash scripts/benchmark-allocation-formal.sh
#
# scripts/benchmark-allocation.sh remains EXPLORATORY ONLY. There is no
# alternative formal entrypoint name.
#
# Host branch (no arguments):
#   - rejects ambient internal/container/source/work selection variables and
#     rejects every caller argument (fail closed, INFRA-TC-001 philosophy);
#   - validates the authoritative Quality contract (.toolchain/quality.env);
#   - derives the authoritative pinned base reference from that SAME contract
#     (scripts/quality-base-ref.sh — no duplicate base digest);
#   - validates the container runtime with the SAME accepted Docker Engine
#     backend rule (scripts/quality-runtime-check.sh; Podman/libpod rejected);
#   - builds/uses the SAME repository-owned canonical Quality toolchain image
#     identity as scripts/quality.sh (same tag derivation, same Dockerfile,
#     same contract — no duplicate image digest or snapshot identity);
#   - derives the expected source SHA from the repository checkout itself and
#     passes it ONLY through the fixed private host→internal protocol;
#   - mounts the checkout read-only at /src (with .git retained) and a fresh
#     ephemeral host work area at /work;
#   - enters ONLY the fixed internal entrypoint;
#   - never emits or imitates the canonical Quality acceptance gate output.
#   The canonical Quality gate semantics (scripts/quality.sh) are unchanged.
#
# Internal branch (entered only through the explicit positional protocol
# created by the host branch): proves the canonical image boundary with the
# accepted helper, re-proves source provenance and the read-only mount from
# /src BEFORE configure, configures Release with sanitizers off and explicit
# LTO off DIRECTLY from /src (single formal source model — no source copy),
# builds in /work, captures exact binary SHA-256 identities, executes the
# existing PR-B measurement executables with evidence_class=formal three
# times each (separate process invocations), validates everything with the
# existing independent validator WITHOUT --allow-exploratory, and asserts
# cross-invocation normalized-metric equality.
#
# Formal source model (OD-M5-P7-023, M5-P7-MR-009 final):
#   /src = read-only mounted Git checkout, .git retained, CMake source root,
#          the bytes compiled from.
#   /work = fresh ephemeral build/output root only. Source is NEVER copied,
#          re-materialized, archived, or re-checked-out into /work.
#
# Generated formal evidence stays under /work and is preserved by the host
# branch to build/benchmark/phase7-formal-results/ (deterministic repository
# convention; the host path never participates in container trust decisions).
# Final WP6 evidence acceptance is a separate later phase.
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
    echo "FORMAL FAIL: $1" >&2
    exit 1
}

# --------------------------------------------------------------------------
# Ambient rejection (host AND internal): the container execution mode is
# never ambient-selectable; source/work roots and identities are never
# ambient-substitutable. The quality test hooks and stale orchestration
# variables are rejected for the same reason scripts/quality.sh rejects
# them: this runner consumes the SAME accepted helpers, and a caller must
# never be able to point them at another contract, another tool tree, or
# another reference time.
# --------------------------------------------------------------------------
reject_ambient() {
    local hook
    for hook in \
        BMD_QUALITY_CONTRACT_FILE \
        BMD_QUALITY_REFERENCE_TIME \
        BMD_QUALITY_TOOLCHAIN_DIR \
        BMD_QUALITY_DPKG_INFO_DIR \
        BMD_QUALITY_WORK_KEEP \
        BMD_CANONICAL_QUALITY_CONTAINER \
        BMD_CANONICAL_QUALITY_SRC \
        BMD_CANONICAL_QUALITY_WORK \
        BMD_P7_FORMAL_INTERNAL \
        BMD_P7_FORMAL_SOURCE_SHA \
        BMD_P7_FORMAL_SRC \
        BMD_P7_FORMAL_WORK; do
        if [[ -n "${!hook:-}" ]]; then
            fail "forbidden variable ${hook} is set in the environment; the formal Phase-7 runner must not be influenced by ambient internal/source/work selection"
        fi
    done
}

# Exact argv grammar: zero arguments = public host mode; exactly two
# arguments with the fixed private protocol = internal mode (entered only by
# the trusted host path). Everything else — including caller selectors —
# exits 2 before any formal work.
mode="host"
expected_sha=""
if [[ $# -eq 0 ]]; then
    :
elif [[ $# -eq 2 && "${1:-}" == "--formal-inside-container" ]]; then
    mode="formal-inside-container"
    expected_sha="$2"
else
    echo "usage: bash scripts/benchmark-allocation-formal.sh" >&2
    echo "  no arguments: public formal host mode (the only public invocation)" >&2
    echo "  the internal container mode is entered exclusively through the" >&2
    echo "  fixed private protocol created by the host mode and rejects" >&2
    echo "  direct hostile invocation" >&2
    exit 2
fi

reject_ambient

# ==========================================================================
# Internal branch (inside the canonical image only)
# ==========================================================================
if [[ "$mode" == "formal-inside-container" ]]; then
    src="/src"
    work="/work"

    if [[ "$(pwd)" != "$src" ]]; then
        fail "internal mode must be entered from the canonical image layout (/src)"
    fi

    # 1. Independently re-prove the accepted image/toolchain boundary with
    #    the SAME helper scripts/quality.sh uses (no second parser): the
    #    image-baked contract must equal the mounted /src contract. A direct
    #    host invocation dies here (no baked contract outside the image).
    echo "Phase-7 formal: proving the canonical image boundary"
    bash "$src/scripts/quality-image-boundary.sh" /opt/toolchain/quality.env "$src"

    # 2. Source provenance and read-only proof, BEFORE configure
    #    (OD-M5-P7-017/023). HEAD(/src) == the host-bound expected SHA;
    #    clean under the accepted repository provenance discipline; /src
    #    read-only. No repair, no mutation.
    echo "Phase-7 formal: proving /src provenance (read-only mount, HEAD, clean)"
    bash "$src/scripts/formal-source-provenance.sh" "$src" "$expected_sha" --require-read-only

    # 3. The canonical toolchain identity (clang 18.1.3, dpkg provenance,
    #    cmake/python floors) must match the accepted contract.
    echo "Phase-7 formal: verifying the canonical toolchain identity"
    bash "$src/scripts/quality-toolchain-check.sh" --skip-conan

    # 4. /work must be a fresh ephemeral work area (the host creates it
    #    empty for every invocation; build/output state never persists).
    [[ -d "$work" ]] || fail "canonical work root /work missing"
    if find "$work" -mindepth 1 -print -quit | grep -q .; then
        fail "/work is not fresh; the formal runner requires a fresh ephemeral work root"
    fi
    cd "$work"

    echo "Phase-7 formal: bootstrapping repository-local Conan (fresh /work)"
    python3 -m venv "$work/.venv-tools"
    export PIP_CACHE_DIR="$work/.cache/pip"
    export CONAN_HOME="$work/.cache/conan2"
    mkdir -p "$PIP_CACHE_DIR" "$CONAN_HOME"
    "$work/.venv-tools/bin/python" -m pip install \
        --disable-pip-version-check \
        --requirement "$src/requirements-tools.txt"
    conan="$work/.venv-tools/bin/conan"
    "$conan" profile detect --force

    # 5. Pinned Contracts / Protobuf prerequisite (the exact dependency
    #    identity of the accepted scripts/bootstrap-contracts.sh path). The
    #    pins are READ from the accepted script (single source of truth for
    #    the Contracts identity) — never duplicated here — and every step
    #    verifies them fail-closed.
    echo "Phase-7 formal: bootstrapping the pinned Contracts prerequisite"
    "$work/.venv-tools/bin/python" -m pip install \
        --disable-pip-version-check \
        --requirement "$src/requirements-m4.txt"
    contracts_revision="$(sed -n 's/^contracts_revision="\([0-9a-f]\{40\}\)"/\1/p' \
        "$src/scripts/bootstrap-contracts.sh" | head -n1)"
    contracts_rrev="$(sed -n 's/^contracts_rrev="\([0-9a-f]\{32\}\)"/\1/p' \
        "$src/scripts/bootstrap-contracts.sh" | head -n1)"
    contracts_url="$(grep -o 'https://github\.com/[^ "\\]*' \
        "$src/scripts/bootstrap-contracts.sh" | head -n1)"
    [[ "$contracts_revision" =~ ^[0-9a-f]{40}$ ]] \
        || fail "could not read the pinned Contracts source revision from the accepted bootstrap script"
    [[ "$contracts_rrev" =~ ^[0-9a-f]{32}$ ]] \
        || fail "could not read the pinned Contracts recipe revision from the accepted bootstrap script"
    [[ -n "$contracts_url" ]] \
        || fail "could not read the pinned Contracts source location from the accepted bootstrap script"

    contracts_dir="$work/m4-prerequisite/contracts"
    mkdir -p "$work/m4-prerequisite"
    git clone --no-checkout "$contracts_url" "$contracts_dir"
    git -C "$contracts_dir" fetch origin "$contracts_revision"
    git -C "$contracts_dir" checkout --detach "$contracts_revision"
    actual_revision="$(git -C "$contracts_dir" rev-parse HEAD)"
    [[ "$actual_revision" == "$contracts_revision" ]] \
        || fail "Contracts source revision mismatch: $actual_revision"
    [[ -z "$(git -C "$contracts_dir" status --short)" ]] \
        || fail "Contracts prerequisite checkout is dirty"

    export_output="$($conan export "$contracts_dir" 2>&1)"
    echo "$export_output"
    [[ "$export_output" == *"binance-market-data-contracts-cpp/0.1.0#$contracts_rrev"* ]] \
        || fail "Contracts Conan recipe revision mismatch"
    package_json="$work/m4-prerequisite/contracts-package.json"
    "$conan" list \
        "binance-market-data-contracts-cpp/0.1.0#$contracts_rrev:*" \
        --format=json > "$package_json"
    if ! "$work/.venv-tools/bin/python" "$src/scripts/verify-contracts-package.py" \
        "$conan" "$package_json"; then
        "$conan" create "$contracts_dir" \
            --lockfile="$contracts_dir/conan.lock" \
            --build=missing \
            -s build_type=Release \
            -s compiler.cppstd=20
        "$conan" list \
            "binance-market-data-contracts-cpp/0.1.0#$contracts_rrev:*" \
            --format=json > "$package_json"
        "$work/.venv-tools/bin/python" "$src/scripts/verify-contracts-package.py" \
            "$conan" "$package_json"
    fi
    echo "Pinned Contracts package ready: binance-market-data-contracts-cpp/0.1.0#$contracts_rrev"

    # 6. Configure the formal Release build DIRECTLY from /src (single formal
    #    source model). Build/output state lives under /work only.
    echo "Phase-7 formal: conan install (Release, ProtoAdapter ON)"
    export CC=clang
    export CXX=clang++
    "$conan" install "$src" \
        --output-folder="$work/conan" \
        --lockfile="$src/conan.lock" \
        --build=missing \
        -s build_type=Release \
        -s compiler.cppstd=20 \
        -o "&:proto_adapter=True" \
        -o "&:shared=False"

    echo "Phase-7 formal: configuring Release build (cmake -S /src -B /work/formal-build)"
    cmake -S "$src" -B "$work/formal-build" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$work/conan/build/Release/generators/conan_toolchain.cmake" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DBMD_PROJECTION_ENABLE_WARNINGS=ON \
        -DBMD_PROJECTION_BUILD_TESTS=OFF \
        -DBMD_PROJECTION_BUILD_BENCHMARKS=ON \
        -DBMD_PROJECTION_BUILD_PROTO_ADAPTER=ON \
        -DBMD_PROJECTION_ENABLE_ASAN=OFF \
        -DBMD_PROJECTION_ENABLE_UBSAN=OFF \
        -DBMD_PROJECTION_ENABLE_TSAN=OFF \
        -DBMD_PROJECTION_ENABLE_COVERAGE=OFF \
        -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF

    # 7. Prove the configured build tree satisfies the formal build contract
    #    (Release, sanitizers off, explicit LTO off, CMake source root /src).
    bash "$src/scripts/formal-build-contract-check.sh" "$work/formal-build"

    echo "Phase-7 formal: building"
    cmake --build "$work/formal-build"

    exe_dir="$work/formal-build/benchmarks"
    m2m3_exe="$exe_dir/bmd_projection_allocation_m2_m3"
    footprint_exe="$exe_dir/bmd_projection_allocation_footprint"
    replay_exe="$exe_dir/bmd_projection_allocation_replay"
    m4_exe="$exe_dir/bmd_projection_allocation_m4"
    for exe in "$m2m3_exe" "$footprint_exe" "$replay_exe" "$m4_exe"; do
        [[ -x "$exe" ]] || fail "formal measurement executable missing after build: $exe"
    done

    evidence="$work/formal-evidence"
    mkdir -p "$evidence/binaries"
    mkdir -p "$evidence/m2m3" "$evidence/footprint" "$evidence/replay" "$evidence/m4"

    # Wrapper identity assertions shared by all four families. The existing
    # independent validator enforces schema/invariants/inventory; this check
    # enforces the RUNNER-level formal identity facts (no measurement
    # validation rules duplicated here). The canonical toolchain version is
    # read from the SAME accepted contract (no duplicated identity).
    llvm_version="$(sed -n 's/^LLVM_MAJOR=//p' /opt/toolchain/quality.env).$(sed -n 's/^LLVM_MINOR=//p' /opt/toolchain/quality.env).$(sed -n 's/^LLVM_PATCH=//p' /opt/toolchain/quality.env)"
    wrapper_identity_check() {
        local wrapper="$1" exe_kind="$2" expected_sha_in="$3" expected_llvm="$4"
        python3 - "$wrapper" "$exe_kind" "$expected_sha_in" "$expected_llvm" <<'PY'
import json
import sys

wrapper_path, kind, expected_sha, expected_llvm = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]
problems = []
wrapper = json.load(open(wrapper_path, encoding="utf-8"))

def require(condition, message):
    if not condition:
        problems.append(message)

require(wrapper.get("evidence_class") == "formal",
        "evidence_class must be formal (a downgrade to exploratory is a failed formal run)")
require(wrapper.get("requested_evidence_class") == "formal",
        "requested_evidence_class must be formal")
require(wrapper.get("evidence_class_downgrade_reason") is None,
        "formal evidence must carry no downgrade reason")
source = wrapper.get("source_provenance", {})
require(source.get("git_sha") == expected_sha,
        f"recorded source git_sha {source.get('git_sha')!r} != HEAD(/src) {expected_sha}")
require(source.get("status") == "known", "source provenance status must be known")
require(source.get("dirty_at_configure") is False, "source must be clean at configure")
build = wrapper.get("build_identity", {})
require(build.get("build_type") == "Release", "recorded build type must be Release")
require(build.get("sanitizer_state") == "off", "recorded sanitizer state must be off")
require(build.get("lto_state") == "off", "recorded LTO state must be off")
compiler = build.get("compiler", {})
require(compiler.get("id") == "Clang", "recorded compiler id must be Clang")
require(compiler.get("version") == expected_llvm,
        f"recorded compiler version {compiler.get('version')!r} != canonical "
        f"contract {expected_llvm}")
environment = wrapper.get("environment_identity", {})
require(environment.get("os_name") == "Linux",
        "recorded environment must be the canonical Linux container")
require(environment.get("architecture") == "x86_64",
        "recorded environment must be the canonical amd64 container")
if problems:
    for problem in problems:
        print(f"FORMAL FAIL: {problem}", file=sys.stderr)
    sys.exit(1)
print(f"formal wrapper identity PASS ({kind})")
PY
    }

    # Fixed formal repetitions: >= 3 per run (OD-M5-P7-015), locked here.
    formal_repetitions=3

    measure_and_validate() {
        local name="$1" exe="$2" kind="$3"
        local pre_sha post_sha inv

        # Exact binary identity captured BEFORE measurement (truthful
        # sequencing: the SHA names the exact executable that is about to
        # run, and a later change is detected).
        pre_sha="$(sha256sum "$exe" | cut -d' ' -f1)"
        printf '%s  %s\n' "$pre_sha" "$exe" > "$evidence/binaries/$name.sha256"

        for inv in 1 2 3; do
            echo "Phase-7 formal: $name invocation $inv/3"
            "$exe" \
                --m5_output="$evidence/$name/run$inv.json" \
                --m5_wrapper_out="$evidence/$name/run$inv-wrapper.json" \
                --m5_evidence_class=formal \
                --m5_repetitions="$formal_repetitions"
        done

        post_sha="$(sha256sum "$exe" | cut -d' ' -f1)"
        [[ "$post_sha" == "$pre_sha" ]] \
            || fail "$name binary changed between SHA capture and measured execution"

        for inv in 1 2 3; do
            echo "Phase-7 formal: validating $name invocation $inv/3 (formal, full inventory, exact binary)"
            python3 "$src/scripts/benchmark_phase7.py" validate-records \
                "$evidence/$name/run$inv.json" \
                "$evidence/$name/run$inv-wrapper.json" \
                --binary "$exe" \
                --require-inventory "$kind"
            wrapper_identity_check "$evidence/$name/run$inv-wrapper.json" "$kind" "$expected_sha" "$llvm_version"
        done

        echo "Phase-7 formal: cross-invocation determinism for $name"
        python3 "$src/scripts/benchmark_phase7.py" check-determinism \
            "$evidence/$name/run1.json" "$evidence/$name/run2.json"
        python3 "$src/scripts/benchmark_phase7.py" check-determinism \
            "$evidence/$name/run1.json" "$evidence/$name/run3.json"
    }

    measure_and_validate m2m3 "$m2m3_exe" m2_m3
    measure_and_validate footprint "$footprint_exe" footprint
    measure_and_validate replay "$replay_exe" replay
    measure_and_validate m4 "$m4_exe" m4

    echo
    echo "FORMAL PHASE-7 ALLOCATION RUN: PASS"
    echo "  source:  /src (read-only, .git retained, HEAD $expected_sha, clean)"
    echo "  build:   /work/formal-build (Release, sanitizers off, LTO off,"
    echo "           canonical Quality toolchain image, Contracts/Protobuf pinned)"
    echo "  evidence: $evidence (M2/M3 54+48 cells, footprint depths"
    echo "           100/1000/5000/10000, M4 24 cells, replay 4 identities;"
    echo "           formal class; exact binary SHA binding; 3 process"
    echo "           invocations per family with cross-run determinism)"
    echo
    echo "NOTE: this runner is NOT the canonical Quality acceptance gate"
    echo "(bash scripts/quality.sh) and emits no canonical Quality verdict."
    exit 0
fi

# ==========================================================================
# Public host branch
# ==========================================================================
cd "$repo_root"

echo "Phase-7 formal: validating the authoritative toolchain contract"
scripts/quality-toolchain-check.sh --contract-only

base_ref="$(bash scripts/quality-base-ref.sh)"
echo "Phase-7 formal: canonical base reference $base_ref"

echo "Phase-7 formal: verifying the Docker Engine backend"
if ! scripts/quality-runtime-check.sh; then
    fail "canonical runtime validation failed (Docker Engine / Docker Desktop required; Podman/libpod is not a validated formal execution runtime)"
fi

echo "Phase-7 formal: deriving the expected source SHA from this checkout"
expected_sha="$(git rev-parse HEAD)"
[[ "$expected_sha" =~ ^[0-9a-fA-F]{40}([0-9a-fA-F]{24})?$ ]] \
    || fail "checkout HEAD is not a real commit SHA"

echo "Phase-7 formal: proving this checkout is clean before mounting"
scripts/formal-source-provenance.sh "$repo_root" "$expected_sha"

sha256_of() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256
    else
        fail "no sha256 tool found"
    fi
}
# SAME canonical image identity as scripts/quality.sh (same contract-derived
# tag, same repository-owned Dockerfile, same pinned base reference).
image_tag="bmd-projection-quality:$(grep -vE '^[[:space:]]*(#|$)' .toolchain/quality.env | sha256_of | cut -c1-16)"

echo "Phase-7 formal: building/using the canonical Quality toolchain image"
docker build --platform linux/amd64 \
    --build-arg "BMD_CANONICAL_BASE_IMAGE_REF=$base_ref" \
    -t "$image_tag" -f .toolchain/Dockerfile .

mkdir -p "$repo_root/build"
work_dir="$(mktemp -d "$repo_root/build/phase7-formal-work.XXXXXX")"
success=0
cleanup() {
    if [[ -n "${work_dir:-}" && -d "$work_dir" ]]; then
        if [[ "$success" == "1" ]]; then
            rm -rf "$work_dir"
        else
            echo "FORMAL FAIL: host work area kept for diagnosis: $work_dir" >&2
        fi
    fi
}
trap cleanup EXIT

echo "Phase-7 formal: running the fixed internal entrypoint in the pinned container"
echo "Phase-7 formal: /src read-only checkout (with .git), fresh ephemeral /work"
docker run --rm --platform linux/amd64 \
    -v "$repo_root":/src:ro \
    -v "$work_dir":/work \
    -w /src \
    "$image_tag" \
    bash scripts/benchmark-allocation-formal.sh --formal-inside-container "$expected_sha"

echo "Phase-7 formal: preserving the completed evidence"
results_dir="$repo_root/build/benchmark/phase7-formal-results"
mkdir -p "$results_dir"
rm -rf "$results_dir/formal-evidence"
cp -R "$work_dir/formal-evidence" "$results_dir/formal-evidence"

success=1
echo
echo "Formal Phase-7 evidence preserved at: $results_dir/formal-evidence"
echo "Evidence is bound to /src HEAD $expected_sha through the fixed"
echo "internal protocol; this runner is separate from the canonical Quality"
echo "gate (bash scripts/quality.sh), whose semantics are unchanged."
exit 0
