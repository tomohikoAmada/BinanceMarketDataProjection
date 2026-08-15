#!/usr/bin/env bash
# quality-base-ref.sh — canonical Docker FROM reference (INFRA-TC-001).
#
# Prints the authoritative base reference "<image>@<digest>" derived from the
# repository-owned toolchain contract. scripts/quality.sh passes exactly this
# value as the BMD_CANONICAL_BASE_IMAGE_REF build argument consumed by
# .toolchain/Dockerfile FROM. The Dockerfile contains NO independent digest
# literal; this script is the single plumbing point between the contract and
# the actual image base.
#
# usage: quality-base-ref.sh [contract-file]
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
contract_file="${1:-$repo_root/.toolchain/quality.env}"

if [[ ! -r "$contract_file" ]]; then
    echo "quality-base-ref: contract file missing or unreadable: $contract_file" >&2
    exit 1
fi

image="$(sed -n 's/^CANONICAL_QUALITY_BASE_IMAGE=//p' "$contract_file" | head -n1)"
digest="$(sed -n 's/^CANONICAL_QUALITY_BASE_IMAGE_DIGEST=//p' "$contract_file" | head -n1)"

if [[ -z "$image" || -z "$digest" ]]; then
    echo "quality-base-ref: contract missing CANONICAL_QUALITY_BASE_IMAGE or CANONICAL_QUALITY_BASE_IMAGE_DIGEST" >&2
    exit 1
fi
if [[ ! "$digest" =~ ^sha256:[0-9a-f]{64}$ ]]; then
    echo "quality-base-ref: malformed base digest: $digest" >&2
    exit 1
fi

printf '%s@%s\n' "$image" "$digest"
