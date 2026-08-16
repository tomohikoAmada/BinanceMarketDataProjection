#!/usr/bin/env bash
# quality-apt-sources.sh — canonical Ubuntu snapshot apt sources (INFRA-TC-001).
#
# Prints the deb822 sources.list content that pins every apt operation to one
# immutable Ubuntu Snapshot Service identity, for both the archive pockets
# (base/updates/backports) and the security pocket. No live archive reference
# is ever emitted. Used by .toolchain/Dockerfile when establishing snapshot
# mode after the TLS bootstrap; also used by the deterministic offline tests.
#
# usage: quality-apt-sources.sh <SNAPSHOT_ID> <SUITE>
#   SNAPSHOT_ID  e.g. 20260814T120000Z (YYYYMMDDTHHMMSSZ)
#   SUITE        e.g. noble
set -euo pipefail

snapshot_id="${1:-}"
suite="${2:-}"

if [[ -z "$snapshot_id" ]]; then
    echo "quality-apt-sources: missing snapshot id" >&2
    exit 2
fi
if [[ -z "$suite" ]]; then
    echo "quality-apt-sources: missing suite" >&2
    exit 2
fi
if [[ ! "$snapshot_id" =~ ^[0-9]{8}T[0-9]{6}Z$ ]]; then
    echo "quality-apt-sources: malformed snapshot id: $snapshot_id" >&2
    exit 1
fi

cat <<EOF
Types: deb
URIs: https://snapshot.ubuntu.com/ubuntu/${snapshot_id}/
Suites: ${suite} ${suite}-updates ${suite}-backports
Components: main universe restricted multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg

## Ubuntu security updates. Aside from URIs and Suites,
## this should mirror your choices in the previous section.
Types: deb
URIs: https://snapshot.ubuntu.com/ubuntu/${snapshot_id}/
Suites: ${suite}-security
Components: main universe restricted multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
EOF
