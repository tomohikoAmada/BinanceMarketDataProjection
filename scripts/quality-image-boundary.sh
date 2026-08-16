#!/usr/bin/env bash
# quality-image-boundary.sh — canonical image boundary proof (INFRA-TC-001).
#
# The internal canonical container mode must prove it is running against the
# canonical image context BEFORE any source copy, recursive execution, or
# acceptance work. This script is that proof: it returns 0 only if
#
#   1. the image-baked contract exists (built into the image at
#      /opt/toolchain/quality.env by .toolchain/Dockerfile), and
#   2. the mounted source tree carries a .toolchain/quality.env, and
#   3. the baked contract equals the mounted source contract (comments and
#      blank lines normalized).
#
# scripts/quality.sh (internal mode) invokes this with the fixed canonical
# layout: baked /opt/toolchain/quality.env against mounted /src, and again
# against the /work scratch copy after entering it.
#
# Threat-model scope: this proves the image/runtime boundary as built by the
# canonical host path. It does not claim protection against a malicious
# privileged host that can rewrite arbitrary root filesystem or
# container-engine state.
#
# usage: quality-image-boundary.sh <baked-contract-file> <mounted-source-root>
set -euo pipefail

baked="${1:-}"
src_root="${2:-}"

if [[ -z "$baked" || -z "$src_root" ]]; then
    echo "usage: $0 <baked-contract-file> <mounted-source-root>" >&2
    exit 2
fi

[[ -f "$baked" ]] || {
    echo "QUALITY FAIL: internal mode is running outside the canonical image (no baked contract: $baked)" >&2
    exit 1
}

[[ -r "$src_root/.toolchain/quality.env" ]] || {
    echo "QUALITY FAIL: mounted source $src_root has no .toolchain/quality.env" >&2
    exit 1
}

if ! diff -q \
    <(grep -vE '^[[:space:]]*(#|$)' "$baked") \
    <(grep -vE '^[[:space:]]*(#|$)' "$src_root/.toolchain/quality.env") >/dev/null; then
    echo "QUALITY FAIL: image-baked contract differs from the mounted source contract" >&2
    echo "QUALITY FAIL: rebuild the canonical image from the current contract (delete the bmd-projection-quality image) and retry" >&2
    exit 1
fi

exit 0
