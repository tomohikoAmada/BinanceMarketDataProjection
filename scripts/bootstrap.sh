#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
tools_venv="$repo_root/.venv-tools"
export CONAN_HOME="$repo_root/.cache/conan2"
export PIP_CACHE_DIR="$repo_root/.cache/pip"

python3 -m venv "$tools_venv"
"$tools_venv/bin/python" -m pip install --upgrade pip
"$tools_venv/bin/python" -m pip install "conan>=2,<3"

conan="$tools_venv/bin/conan"
mkdir -p "$CONAN_HOME" "$PIP_CACHE_DIR"
"$conan" profile detect --force

if [[ ! -f "$repo_root/conan.lock" ]]; then
    "$conan" lock create "$repo_root" \
        --lockfile-out="$repo_root/conan.lock" \
        -s build_type=Release
fi

"$conan" install "$repo_root" \
    --output-folder="$repo_root/build/_bootstrap" \
    --lockfile="$repo_root/conan.lock" \
    --build=missing \
    -s build_type=Release

echo "Repository-local Conan environment is ready: $CONAN_HOME"
