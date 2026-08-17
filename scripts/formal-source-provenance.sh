#!/usr/bin/env bash
# formal-source-provenance.sh — Phase-7 formal source provenance proof.
#
# Proves, from the mounted source root itself (OD-M5-P7-023, single formal
# source model):
#
#   - the root exists, is a directory, and carries a usable .git;
#   - HEAD resolves to a real commit SHA (40 or 64 hex digits);
#   - HEAD equals the host-bound expected source SHA;
#   - the source state is clean per the existing accepted repository
#     provenance discipline (git status --porcelain: tracked, index,
#     worktree, and untracked state — never weakened);
#   - optionally (--require-read-only), the root is read-only: a write probe
#     must fail.
#
# This helper performs NO repair and NO mutation: it never stashes, cleans,
# resets, checks out, or writes into the source root. Every failure happens
# BEFORE any configure/build work. GIT_OPTIONAL_LOCKS=0 keeps git itself from
# writing index stat-refresh locks under a read-only mount.
#
# Used by:
#   - scripts/benchmark-allocation-formal.sh (host mode, on the checkout that
#     is about to be mounted; internal mode, authoritatively on /src)
#
# usage: formal-source-provenance.sh <src-root> <expected-sha> [--require-read-only]
set -euo pipefail

fail() {
    echo "FORMAL FAIL: $1" >&2
    exit 1
}

if [[ $# -lt 2 || $# -gt 3 ]]; then
    echo "usage: $0 <src-root> <expected-sha> [--require-read-only]" >&2
    exit 2
fi

src="$1"
expected_sha="$2"
require_read_only="no"
if [[ $# -eq 3 ]]; then
    case "$3" in
        --require-read-only) require_read_only="yes" ;;
        *)
            echo "usage: $0 <src-root> <expected-sha> [--require-read-only]" >&2
            exit 2
            ;;
    esac
fi

[[ "$expected_sha" =~ ^[0-9a-fA-F]{40}([0-9a-fA-F]{24})?$ ]] \
    || fail "expected source SHA is not a valid commit SHA: $expected_sha"

[[ -d "$src" ]] || fail "source root missing: $src"
[[ -d "$src/.git" ]] || fail "source root has no .git directory: $src"

if ! git -C "$src" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    fail "source root is not a usable Git work tree: $src"
fi

head_sha="$(GIT_OPTIONAL_LOCKS=0 git -C "$src" rev-parse HEAD 2>/dev/null)" \
    || fail "cannot resolve HEAD in $src"
[[ "$head_sha" =~ ^[0-9a-fA-F]{40}([0-9a-fA-F]{24})?$ ]] \
    || fail "HEAD of $src is not a real commit SHA: $head_sha"

if [[ "$head_sha" != "$expected_sha" ]]; then
    fail "HEAD(/src) $head_sha != host-bound expected source SHA $expected_sha"
fi

status_out="$(GIT_OPTIONAL_LOCKS=0 git -C "$src" status --porcelain)" \
    || fail "git status failed in $src"
if [[ -n "$status_out" ]]; then
    echo "FORMAL FAIL: source tree is not clean (tracked/index/worktree/untracked state):" >&2
    printf '%s\n' "$status_out" | head -n 5 >&2
    fail "dirty /src is rejected BEFORE configure; commit or clean the checkout and retry"
fi

if [[ "$require_read_only" == "yes" ]]; then
    probe="$src/.bmd-p7-formal-ro-probe"
    if ( : > "$probe" ) 2>/dev/null; then
        rm -f "$probe"
        fail "/src must be mounted read-only; a write probe succeeded"
    fi
    rm -f "$probe"
    echo "formal-source-provenance: /src proven read-only"
fi

echo "formal-source-provenance: PASS ($src HEAD $head_sha, clean)"
