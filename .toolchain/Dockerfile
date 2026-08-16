# Canonical Quality acceptance environment (INFRA-TC-001).
#
# Immutable identity, declared in .toolchain/quality.env (single source of
# truth). This Dockerfile contains NO independent base digest literal:
#
#   - the authoritative base reference (<image>@<digest>) is computed by
#     scripts/quality-base-ref.sh from the contract and MUST be passed as the
#     BMD_CANONICAL_BASE_IMAGE_REF build argument (no default, no fallback);
#   - Ubuntu archive state is pinned to the contract's UBUNTU_SNAPSHOT_ID
#     (historical) on the official Ubuntu Snapshot Service for BOTH the
#     archive pockets and the security pocket; direct snapshot URIs only;
#   - TLS bootstrap uses a repository-committed, SHA-256-pinned
#     ca-certificates artifact (.toolchain/bootstrap/ca-certificates.deb,
#     retrieved from the same historical snapshot): bytes are verified and
#     only TLS trust material is extracted. NO mutable live archive is ever
#     contacted anywhere in this build;
#   - every package is installed with an exact version pin resolved against
#     the snapshot, with archive signature verification (Signed-By) intact.
#
# Build with: docker build --platform linux/amd64 \
#     --build-arg BMD_CANONICAL_BASE_IMAGE_REF=$(bash scripts/quality-base-ref.sh) \
#     -f .toolchain/Dockerfile .
# Normal operation builds through scripts/quality.sh; do not rebuild ad hoc.

ARG BMD_CANONICAL_BASE_IMAGE_REF
FROM ${BMD_CANONICAL_BASE_IMAGE_REF}

ARG BMD_CANONICAL_BASE_IMAGE_REF
ARG TARGETARCH

LABEL bmd.canonical.base.ref="${BMD_CANONICAL_BASE_IMAGE_REF}"

RUN test "$TARGETARCH" = "amd64" \
    && echo "Canonical Quality architecture: amd64" \
    || { echo "Canonical Quality requires amd64; got: ${TARGETARCH:-<unset>}" >&2; exit 1; }

COPY .toolchain/quality.env /opt/toolchain/quality.env
COPY scripts/quality-base-ref.sh /opt/toolchain/quality-base-ref.sh
COPY .toolchain/bootstrap/ca-certificates.deb /opt/toolchain/bootstrap/ca-certificates.deb
COPY scripts/quality-apt-sources.sh /opt/toolchain/quality-apt-sources.sh

# ---- Base-binding assertion (INV-BASE-001) ---------------------------------
# The actual FROM identity is the BMD_CANONICAL_BASE_IMAGE_REF build argument;
# the contract baked into THIS build is /opt/toolchain/quality.env. The image
# must fail closed unless the FROM argument equals base_ref(baked contract) —
# a build whose context contract changed between host-side base-reference
# capture and image build (FROM D0 + baked D1) must never reach TLS bootstrap,
# apt, package resolution, or canonical acceptance. Uses the SAME production
# base-reference semantics as scripts/quality.sh (no second parser).
RUN bash /opt/toolchain/quality-base-ref.sh /opt/toolchain/quality.env "${BMD_CANONICAL_BASE_IMAGE_REF}"

# ---- Immutable TLS bootstrap (NO live archive) ----------------------------
# The pristine base image ships no CA store and the Ubuntu Snapshot Service
# requires TLS. The committed ca-certificates package payload is pinned by
# CA_CERTIFICATES_BOOTSTRAP_SHA256 in the contract; its bytes are verified
# before use and only TLS trust material is extracted (no package install,
# no apt operation, no network contact before snapshot mode).
RUN set -eu; \
    . /opt/toolchain/quality.env; \
    echo "TLS bootstrap artifact sha256: ${CA_CERTIFICATES_BOOTSTRAP_SHA256}"; \
    test "$(sha256sum /opt/toolchain/bootstrap/ca-certificates.deb | cut -d' ' -f1)" = "$CA_CERTIFICATES_BOOTSTRAP_SHA256" \
        || { echo "TLS bootstrap artifact checksum mismatch" >&2; exit 1; }; \
    rm -rf /opt/toolchain/ca-bootstrap; \
    dpkg-deb -x /opt/toolchain/bootstrap/ca-certificates.deb /opt/toolchain/ca-bootstrap; \
    find /opt/toolchain/ca-bootstrap/usr/share/ca-certificates -type f -name '*.crt' -exec cat {} + \
        > /opt/toolchain/ca-bootstrap/ca-certificates.crt; \
    grep -q 'BEGIN CERTIFICATE' /opt/toolchain/ca-bootstrap/ca-certificates.crt \
        || { echo "TLS bootstrap bundle empty or malformed" >&2; exit 1; }; \
    printf 'Acquire::https::CaInfo "/opt/toolchain/ca-bootstrap/ca-certificates.crt";\n' \
        > /etc/apt/apt.conf.d/50snapshot-ca

# ---- Snapshot mode --------------------------------------------------------
# Every apt operation below resolves against the pinned HISTORICAL snapshot.
RUN set -eu; \
    . /opt/toolchain/quality.env; \
    echo "Ubuntu snapshot: ${UBUNTU_SNAPSHOT_ID} (${UBUNTU_SUITE}, archive + security pockets)"; \
    /opt/toolchain/quality-apt-sources.sh "$UBUNTU_SNAPSHOT_ID" "$UBUNTU_SUITE" \
        > /etc/apt/sources.list.d/ubuntu.sources; \
    cat /etc/apt/sources.list.d/ubuntu.sources; \
    apt-get update; \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        "$CLANG_PACKAGE_PIN" \
        "$CLANG_TIDY_PACKAGE_PIN" \
        "$CLANG_FORMAT_PACKAGE_PIN" \
        "$GXX_PACKAGE_PIN" \
        "$CMAKE_PACKAGE_PIN" \
        "$NINJA_PACKAGE_PIN" \
        "$MAKE_PACKAGE_PIN" \
        "$PATCH_PACKAGE_PIN" \
        "$PYTHON_VENV_PACKAGE_PIN" \
        "$GIT_PACKAGE_PIN" \
        "$CA_CERTIFICATES_PACKAGE_PIN"; \
    update-alternatives --install /usr/bin/clang clang "/usr/bin/clang-${LLVM_MAJOR}" 100; \
    update-alternatives --install /usr/bin/clang++ clang++ "/usr/bin/clang++-${LLVM_MAJOR}" 100; \
    update-alternatives --install /usr/bin/clang-tidy clang-tidy "/usr/bin/clang-tidy-${LLVM_MAJOR}" 100; \
    update-alternatives --install /usr/bin/clang-format clang-format "/usr/bin/clang-format-${LLVM_MAJOR}" 100; \
    for tool in clang clang++ clang-tidy clang-format; do \
        "$tool" --version \
            | grep -qE "version ${LLVM_MAJOR}\.${LLVM_MINOR}\.${LLVM_PATCH}( |$)" \
            || { echo "image build: unexpected $tool version" >&2; exit 1; }; \
    done; \
    dpkg -s ca-certificates \
        | grep -qE "^Version: ${CA_CERTIFICATES_PACKAGE_PIN#*=}$" \
        || { echo "image build: ca-certificates version mismatch" >&2; exit 1; }
