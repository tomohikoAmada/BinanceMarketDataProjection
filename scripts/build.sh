#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ $# -ne 1 ]]; then
    echo "usage: $0 {debug|release|asan|ubsan|tsan|coverage|benchmark|fuzz}" >&2
    exit 2
fi

cd "$repo_root"
cmake --build --preset "$1"
