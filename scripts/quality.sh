#!/usr/bin/env bash
# quality.sh — canonical Quality entrypoint (INFRA-TC-001).
#
# ONE repository-owned command for authoritative Quality acceptance. CI and
# local developers must call this same entrypoint; it must not be redefined
# in the workflow.
#
# Host branch:
#   - validates the toolchain contract file (.toolchain/quality.env)
#   - requires Docker (or Podman); fails with instructions when absent
#   - builds the canonical image from the pinned base digest and exact
#     package pins in the contract (fail closed if apt cannot install the
#     exact versions)
#   - runs the container, mounting this working tree read-only
#
# Container branch (inside the canonical image):
#   1. copies the mounted source into the canonical scratch root /work
#      (fresh copy per run; the host working tree is never polluted)
#   2. invalidates /work caches when the contract fingerprint changed
#   3. verifies the image was baked from this exact contract
#   4. fails closed unless the running toolchain matches the contract
#   5. runs the exact Quality semantics: formatting, repository-local Conan
#      bootstrap, pinned Contracts bootstrap, Debug configure with
#      ProtoAdapter ON, clang-tidy ON, Werror ON, build, tests, and the
#      staged-install consumer
#
# Local reproduction: bash scripts/quality.sh
#   macOS: Docker Desktop (amd64 containers run through Rosetta 2).
#   Linux: Docker or Podman.
#
# See docs/QUALITY_TOOLCHAIN.md for the contract and upgrade procedure.
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ "${BMD_CANONICAL_QUALITY_CONTAINER:-0}" == "1" ]]; then
    # ------------------------------------------------------------------
    # Container branch
    # ------------------------------------------------------------------
    src="${BMD_CANONICAL_QUALITY_SRC:-/src}"
    work="${BMD_CANONICAL_QUALITY_WORK:-/work}"

    if [[ "$(pwd)" != "$work" ]]; then
        echo "Canonical Quality: preparing canonical scratch root"
        scripts/quality-work-prep.sh "$src" "$work"
        cd "$work"
        exec bash scripts/quality.sh
    fi

    cd "$work"
    set -a
    # shellcheck disable=SC1091
    . .toolchain/quality.env
    set +a

    contract_normalize() {
        grep -vE '^[[:space:]]*(#|$)' "$1"
    }
    fingerprint() {
        contract_normalize "$1" | sha256sum | cut -d' ' -f1
    }

    fingerprint_file="$work/.bmd-quality-contract-fingerprint"
    current_fp="$(fingerprint .toolchain/quality.env)"
    if [[ -f "$fingerprint_file" ]] \
        && [[ "$(cat "$fingerprint_file")" != "$current_fp" ]]; then
        echo "Canonical Quality: contract changed; invalidating cached build/cache/venv"
        rm -rf "$work/build" "$work/.cache" "$work/.venv-tools"
    fi
    printf '%s' "$current_fp" > "$fingerprint_file"

    if [[ -f /opt/toolchain/quality.env ]]; then
        if ! diff -q \
            <(contract_normalize /opt/toolchain/quality.env) \
            <(contract_normalize .toolchain/quality.env) >/dev/null; then
            echo "QUALITY FAIL: image was built from a different contract than the working tree" >&2
            echo "QUALITY FAIL: rebuild the image (delete the bmd-projection-quality image) and retry" >&2
            exit 1
        fi
    else
        echo "QUALITY FAIL: image has no baked contract (stale image); rebuild and retry" >&2
        exit 1
    fi

    echo "Canonical Quality toolchain identity:"
    echo "  clang/clang++/clang-tidy/clang-format $LLVM_MAJOR.$LLVM_MINOR.$LLVM_PATCH"
    echo "  $CANONICAL_QUALITY_OS $CANONICAL_QUALITY_ARCH"
    echo "  base $CANONICAL_QUALITY_BASE_IMAGE $CANONICAL_QUALITY_BASE_IMAGE_DIGEST"
    echo "  ubuntu snapshot $UBUNTU_SNAPSHOT_ID"
    echo "  C++ standard $CPP_STANDARD"

    scripts/quality-toolchain-check.sh --skip-conan

    echo "Canonical Quality: formatting check"
    find include src tests benchmarks fuzz -type f \( -name '*.cpp' -o -name '*.hpp' \) -print0 \
        | xargs -0 clang-format --dry-run --Werror

    export CC=clang
    export CXX=clang++

    echo "Canonical Quality: repository-local Conan bootstrap"
    scripts/bootstrap.sh

    export PATH="$work/.venv-tools/bin:$PATH"
    scripts/quality-toolchain-check.sh

    echo "Canonical Quality: pinned Contracts bootstrap"
    scripts/bootstrap-contracts.sh

    echo "Canonical Quality: configure (Debug, ProtoAdapter ON, clang-tidy ON, Werror ON)"
    scripts/configure.sh debug \
        -DBMD_PROJECTION_BUILD_PROTO_ADAPTER=ON \
        -DBMD_PROJECTION_ENABLE_CLANG_TIDY=ON \
        -DBMD_PROJECTION_ENABLE_WERROR=ON

    echo "Canonical Quality: build"
    scripts/build.sh debug

    echo "Canonical Quality: tests"
    scripts/test.sh debug

    echo "Canonical Quality: staged install consumer"
    scripts/install-consumer-test.sh

    echo "CANONICAL QUALITY: PASS (clang/clang++/clang-tidy/clang-format $LLVM_MAJOR.$LLVM_MINOR.$LLVM_PATCH, $CANONICAL_QUALITY_OS $CANONICAL_QUALITY_ARCH, snapshot $UBUNTU_SNAPSHOT_ID)"
    exit 0
fi

# ------------------------------------------------------------------
# Host branch
# ------------------------------------------------------------------
cd "$repo_root"

echo "Canonical Quality: validating toolchain contract"
scripts/quality-toolchain-check.sh --contract-only

base_ref="$(bash scripts/quality-base-ref.sh)"
snapshot_id="$(sed -n 's/^UBUNTU_SNAPSHOT_ID=//p' .toolchain/quality.env | head -n1)"
echo "Canonical Quality: base reference $base_ref"
echo "Canonical Quality: ubuntu snapshot $snapshot_id"

runtime=""
if command -v docker >/dev/null 2>&1; then
    runtime="docker"
elif command -v podman >/dev/null 2>&1; then
    runtime="podman"
else
    echo "QUALITY FAIL: canonical Quality requires Docker or Podman" >&2
    echo "  macOS: install and start Docker Desktop" >&2
    echo "         (https://docs.docker.com/desktop/setup/install/mac-install/)" >&2
    echo "  Linux: install docker or podman" >&2
    exit 1
fi
if ! "$runtime" info >/dev/null 2>&1; then
    echo "QUALITY FAIL: '$runtime' is installed but not usable (daemon not running?)" >&2
    echo "  Start Docker Desktop / the container runtime and retry." >&2
    exit 1
fi

if [[ "$(uname -m)" != "x86_64" ]]; then
    echo "Note: host architecture is $(uname -m); canonical Quality runs the pinned"
    echo "      amd64 container (Docker Desktop on Apple Silicon uses Rosetta 2)."
fi

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
image_tag="bmd-projection-quality:$(grep -vE '^[[:space:]]*(#|$)' .toolchain/quality.env | sha256_of | cut -c1-16)"

echo "Canonical Quality: building pinned toolchain image ($runtime)"
"$runtime" build --platform linux/amd64 \
    --build-arg "BMD_CANONICAL_BASE_IMAGE_REF=$base_ref" \
    -t "$image_tag" -f .toolchain/Dockerfile .

echo "Canonical Quality: running canonical acceptance in the pinned container"
"$runtime" run --rm --platform linux/amd64 \
    -v "$repo_root":/src:ro \
    -v bmd-projection-quality-work:/work \
    -e BMD_CANONICAL_QUALITY_CONTAINER=1 \
    -w /src \
    "$image_tag" \
    bash scripts/quality.sh
