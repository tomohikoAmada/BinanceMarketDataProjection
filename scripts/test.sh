#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ $# -ne 1 ]]; then
    echo "usage: $0 {debug|release|asan|ubsan|tsan|coverage}" >&2
    exit 2
fi

case "$1" in
    debug|release|asan|ubsan|tsan|coverage) ;;
    *)
        echo "preset '$1' has no test preset" >&2
        exit 2
        ;;
esac

cd "$repo_root"
ctest --preset "$1"
