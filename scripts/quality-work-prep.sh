#!/usr/bin/env bash
# quality-work-prep.sh — canonical source-scratch preparation (INFRA-TC-001).
#
# Every canonical Quality run executes from a scratch root that is rebuilt to
# exactly reflect the current /src worktree, so that no compilation, link, or
# configuration output from a previous source state can ever be reused:
#
#   - the scratch root is cleaned of EVERY entry except the explicitly
#     permitted persistent caches (.cache, .venv-tools);
#   - /work/build NEVER persists: no CMake/Ninja output survives a run, so a
#     stale object file cannot produce a false PASS regardless of mtimes;
#   - previously deleted source files cannot survive a run;
#   - source identity is content copy, never mtime based.
#
# Permitted persistent caches hold only content-addressed downloads and the
# tools venv (Conan and pip reconcile exact pins on every run); they never
# determine whether current source is compiled.
#
# usage: quality-work-prep.sh <src> <work>
#   BMD_QUALITY_WORK_KEEP  optional space-separated names to preserve
#                          (defaults to ".cache .venv-tools")
set -euo pipefail

if [[ $# -ne 2 ]]; then
    echo "usage: $0 <src> <work>" >&2
    exit 2
fi

src="$1"
work="$2"
keep="${BMD_QUALITY_WORK_KEEP:-.cache .venv-tools}"

[[ -d "$src" ]] || {
    echo "quality-work-prep: source root missing: $src" >&2
    exit 1
}

mkdir -p "$work"

rm_keep_entries() {
    local path
    find "$work" -mindepth 1 -maxdepth 1 -print | while IFS= read -r path; do
        local name
        name="$(basename "$path")"
        case " $keep " in
            *" $name "*) ;;
            *) rm -rf "$path" ;;
        esac
    done
}
rm_keep_entries

tar \
    --exclude='./.git' --exclude='./build' \
    --exclude='./.cache' --exclude='./.venv-tools' \
    -C "$src" -cf - . | tar -C "$work" -xf -

# The scratch build tree must never survive into the next run.
rm -rf "$work/build"

echo "quality-work-prep: scratch root $work prepared from $src (kept: ${keep})"
