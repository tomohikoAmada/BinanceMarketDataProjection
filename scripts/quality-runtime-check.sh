#!/usr/bin/env bash
# quality-runtime-check.sh — canonical container runtime identity (INFRA-TC-001).
#
# Canonical Quality is validated only on:
#   Linux:  Docker Engine
#   macOS:  Docker Desktop (backed by Docker Engine)
# Podman and docker-compatible Podman wrappers (podman-docker / libpod
# backends exposed through a `docker` command) are NOT accepted canonical
# runtimes.
#
# A `docker` executable on PATH is not sufficient evidence of the backend.
# `docker version --format '{{json .Server}}'` returns the SERVER identity:
# a real Docker Engine server reports a Components array containing the
# Engine component (including on Docker Desktop), while Podman/libpod servers
# report no such structure. Unknown/unparseable identities fail closed.
#
# usage: quality-runtime-check.sh
set -euo pipefail

if ! command -v docker >/dev/null 2>&1; then
    echo "QUALITY FAIL: canonical Quality requires Docker (docker command not found)" >&2
    echo "  macOS: install and start Docker Desktop" >&2
    echo "         (https://docs.docker.com/desktop/setup/install/mac-install/)" >&2
    echo "  Linux: install Docker Engine (podman is not a validated canonical acceptance runtime)" >&2
    exit 1
fi

server_json="$(docker version --format '{{json .Server}}' 2>/dev/null)" || {
    echo "QUALITY FAIL: docker daemon unreachable or no server identity returned" >&2
    echo "  Start Docker Desktop / the Docker daemon and retry." >&2
    exit 1
}

if [[ -z "$server_json" ]]; then
    echo "QUALITY FAIL: docker daemon returned no server identity" >&2
    exit 1
fi

if ! grep -q '"Components"' <<<"$server_json" \
    || ! grep -qE '"Name"[[:space:]]*:[[:space:]]*"Engine"' <<<"$server_json"; then
    echo "QUALITY FAIL: docker server is not a Docker Engine backend (Podman/libpod or unknown backend?)" >&2
    echo "QUALITY FAIL: canonical Quality is validated only on Docker Engine / Docker Desktop" >&2
    exit 1
fi

echo "QUALITY RUNTIME CHECK: PASS (Docker Engine backend detected)"
