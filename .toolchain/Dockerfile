# Canonical Quality acceptance environment (INFRA-TC-001).
#
# Immutable identity, declared in .toolchain/quality.env (single source of
# truth):
#   - base image: ubuntu:24.04 amd64, pinned by digest
#   - exact Ubuntu noble apt package versions for the LLVM 18.1.3 toolchain
#     (clang / clang++ / clang-tidy / clang-format), CMake, Ninja, Python
#     venv, g++-13 (libstdc++ headers), git, and ca-certificates
#
# Build with: docker build --platform linux/amd64 -f .toolchain/Dockerfile .
# Normal operation builds through scripts/quality.sh; do not rebuild ad hoc.

FROM ubuntu@sha256:019e8eb29a85e74d64925745884f2ec79aa27e3feab36353d24656f4d6b89467

ARG TARGETARCH
RUN test "$TARGETARCH" = "amd64" \
    && echo "Canonical Quality architecture: amd64" \
    || { echo "Canonical Quality requires amd64; got: ${TARGETARCH:-<unset>}" >&2; exit 1; }

COPY .toolchain/quality.env /opt/toolchain/quality.env

RUN set -eu; \
    . /opt/toolchain/quality.env; \
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
    done
