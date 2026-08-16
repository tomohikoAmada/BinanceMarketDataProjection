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
# Assertion mode (second argument): .toolchain/Dockerfile copies this exact
# script into the image and invokes it against the baked contract to prove
# that the FROM build argument equals base_ref(baked contract) BEFORE any TLS
# bootstrap, snapshot setup, apt, or package work. A mismatch fails the build.
#
# usage: quality-base-ref.sh [contract-file] [expected-base-ref]
#   contract-file        default: <repo>/.toolchain/quality.env
#   expected-base-ref    if given, print "verified" and exit 0 only when the
#                        derived reference equals it; otherwise exit 1
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
contract_file="${1:-$repo_root/.toolchain/quality.env}"
expected_ref="${2:-}"

if [[ ! -r "$contract_file" ]]; then
    echo "quality-base-ref: contract file missing or unreadable: $contract_file" >&2
    exit 1
fi

# Duplicate assignment of any key is malformed (no first-wins/last-wins
# interpretation); reject before consuming any value so this consumer can
# never observe a different contract interpretation than the validator.
duplicate_keys="$(awk -F= '/^[A-Z0-9_]+=/ { key=$1; if (seen[key]++) print key }' "$contract_file")"
if [[ -n "$duplicate_keys" ]]; then
    echo "quality-base-ref: contract has duplicate key assignment(s): $(printf '%s ' $duplicate_keys)in $contract_file" >&2
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

ref="$image@$digest"

if [[ -n "$expected_ref" ]]; then
    if [[ "$ref" != "$expected_ref" ]]; then
        echo "quality-base-ref: ASSERTION FAILED: provided base reference '${expected_ref}'" >&2
        echo "quality-base-ref: != base reference derived from '${contract_file}' ('${ref}')" >&2
        echo "quality-base-ref: the build context contract changed between base-reference capture and image build" >&2
        exit 1
    fi
    echo "quality-base-ref: base reference verified: ${ref}"
    exit 0
fi

printf '%s\n' "$ref"
