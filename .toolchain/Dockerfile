# Canonical Quality acceptance environment (INFRA-TC-001).
#
# Immutable identity, declared in .toolchain/quality.env (single source of
# truth). This Dockerfile contains NO independent base digest literal:
#
#   - the authoritative base reference (<image>@<digest>) is computed by
#     scripts/quality-base-ref.sh from the contract and MUST be passed as the
#     BMD_CANONICAL_BASE_IMAGE_REF build argument (no default, no fallback);
#   - Ubuntu archive state is pinned to the contract's UBUNTU_SNAPSHOT_ID on
#     the official Ubuntu Snapshot Service for BOTH the archive pockets and
#     the security pocket;
#   - the only pre-snapshot apt operation is the exact-pinned ca-certificates
#     TLS bootstrap from the base image's shipped default sources (the base
#     image ships no CA store and the snapshot service requires TLS);
#   - every other package is installed with an exact version pin resolved
#     against the snapshot.
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
COPY scripts/quality-apt-sources.sh /opt/toolchain/quality-apt-sources.sh

# ---- TLS bootstrap (BEFORE snapshot mode) --------------------------------
# The pinned base image ships no CA store and the Ubuntu Snapshot Service
# requires TLS, so this single exact-pinned step installs ca-certificates
# from the base image's own shipped default sources (plain HTTP, GPG-verified
# indexes). Snapshot mode is established immediately afterwards; no live
# archive reference exists in the final sources and no later apt operation
# touches the live archive.
RUN set -eu; \
    . /opt/toolchain/quality.env; \
    echo "TLS bootstrap: installing ${CA_CERTIFICATES_PACKAGE_PIN} from shipped base sources"; \
    apt-get update; \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        "$CA_CERTIFICATES_PACKAGE_PIN"; \
    rm -rf /var/lib/apt/lists/*

# ---- Snapshot mode --------------------------------------------------------
# Every apt operation below resolves against the pinned Ubuntu snapshot.
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
        "$GIT_PACKAGE_PIN"; \
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
