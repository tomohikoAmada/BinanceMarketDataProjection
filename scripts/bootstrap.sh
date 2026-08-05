#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
tools_venv="$repo_root/.venv-tools"
export CONAN_HOME="$repo_root/.cache/conan2"
export PIP_CACHE_DIR="$repo_root/.cache/pip"

python3 -m venv "$tools_venv"
"$tools_venv/bin/python" -m pip install \
    --disable-pip-version-check \
    --requirement "$repo_root/requirements-tools.txt"

conan="$tools_venv/bin/conan"
echo -n "Conan version: "
"$conan" --version

mkdir -p "$CONAN_HOME" "$PIP_CACHE_DIR"
"$conan" profile detect --force

if [[ ! -f "$repo_root/conan.lock" ]]; then
    "$conan" lock create "$repo_root" \
        --lockfile-out="$repo_root/conan.lock" \
        -s build_type=Release \
        -s compiler.cppstd=20
fi

"$conan" install "$repo_root" \
    --output-folder="$repo_root/build/_bootstrap" \
    --lockfile="$repo_root/conan.lock" \
    --build=missing \
    -s build_type=Release \
    -s compiler.cppstd=20

echo "Repository-local Conan environment is ready: $CONAN_HOME"
