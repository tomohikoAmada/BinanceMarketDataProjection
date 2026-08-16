#!/usr/bin/env bash
# quality-cache-key.sh — canonical cache namespace identity (INFRA-TC-001).
#
# Persistent dependency/tool caches (.cache, .venv-tools) are namespaced by a
# key derived from EVERY acceptance input that can make reuse of binary/tool
# caches unsafe:
#
#   CACHE_KEY = SHA-256( normalized .toolchain/quality.env
#                        + exact contents of requirements-tools.txt )
#
# The normalized contract covers the base image identity, the historical
# Ubuntu snapshot, every exact package pin (LLVM major/minor/patch, build
# drivers), and the bootstrap artifact hash; requirements-tools.txt covers
# the Conan tool version. A contract change therefore produces a different
# namespace, making old-contract binaries structurally invisible.
#
# scripts/quality.sh uses this key for the persistent volume names
# bmd-projection-quality-cache-${CACHE_KEY} and
# bmd-projection-quality-venv-${CACHE_KEY}; /work itself stays ephemeral per
# run, so Projection build outputs never persist (FINAL-002 remains closed).
#
# usage: quality-cache-key.sh [contract-file] [requirements-file]
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
contract_file="${1:-$repo_root/.toolchain/quality.env}"
requirements_file="${2:-$repo_root/requirements-tools.txt}"

if [[ ! -r "$contract_file" || ! -r "$requirements_file" ]]; then
    echo "quality-cache-key: contract or requirements file missing/unreadable" >&2
    exit 1
fi

# Duplicate assignment of any key is malformed (no first-wins/last-wins
# interpretation); reject before consuming so the cache namespace can never
# be derived from a contract the validator would reject.
duplicate_keys="$(awk -F= '/^[A-Z0-9_]+=/ { key=$1; if (seen[key]++) print key }' "$contract_file")"
if [[ -n "$duplicate_keys" ]]; then
    echo "quality-cache-key: contract has duplicate key assignment(s): $(printf '%s ' $duplicate_keys)in $contract_file" >&2
    exit 1
fi

sha256_of() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256
    else
        echo "quality-cache-key: no sha256 tool found" >&2
        exit 1
    fi
}

{
    grep -vE '^[[:space:]]*(#|$)' "$contract_file"
    printf '%s\n' "=== requirements-tools.txt ==="
    cat "$requirements_file"
} | sha256_of | cut -d' ' -f1
