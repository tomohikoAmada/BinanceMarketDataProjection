# M5 Phase 1 Canonical Replay

## Status

- M5 implementation: **IN PROGRESS**
- Phase 1: **COMPLETE / MERGED** (PR #11, merge `5e8629a7ff825f8ea941304d9b09be1670643e8a`,
  post-merge main CI `31264500905` — PASS 16/16)
- Phase 2: **IMPLEMENTED / PENDING INDEPENDENT REVIEW** — see
  `docs/M5_PHASE2_DIFFERENTIAL_ORACLE.md`
- M5-P1-IR-1: **CLOSED** - Spot bootstrap bridge follows the accepted M3/ADR-0005 contains-`L`
  rule `U <= L < u`
- M5-P1-IR-2: **CLOSED**
- M5-PIR-001: **CLOSED**
- M5-PIR-002: **DEFERRED / NON-BLOCKING**
- M5-PIR-003: **DEFERRED / NON-BLOCKING** - before required branch protection
- M5-PIR-004: **CLOSED** - explicit offline acquisition boundary

Phase 1 is test-only infrastructure. It does not change `include/**`, `src/**`, Core, or the
ProtoAdapter target.

## Replay Log V1

The first line is a single-space-separated header:

```text
REPLAY_V1 market=<Spot|UsdMPerpetual> symbol=<identifier> price_scale=<uint32> quantity_scale=<uint32> policy=<Spot|UsdMPerpetual> fixture_id=<identifier>
```

Optional interpretation/provenance fields use `provenance_<name>=<identifier>` and are preserved.
Scales are restricted to 0..18.

Events are one per line:

```text
INSTALL_BASELINE <id> <levels> <levels>
DEPTH_UPDATE <first> <final> pu=<id|-> <levels>
REBASELINE <id> <levels> <levels>
RESET
SNAPSHOT_REQUEST <depth|-> <facts|-> <snapshot_id> <producer> <producer_version> <origin> <generated_utc_ns> <monotonic_ns|-> <gap|->
ADAPTER_METADATA <facts|->
MALFORMED_RANGE <first> <final>
```

An empty level list is `-`. Each non-empty level list preserves input order and uses
`B:<decimal>,<decimal>|A:<decimal>,<decimal>`. Baseline fields require their corresponding side;
depth updates require an explicit side on every level. Decimal tokens are restricted to ASCII
numeric spellings (`0-9`, `+`, `-`, `.`, `e`, `E`) and are preserved verbatim. Their M1/domain
validity is deliberately deferred to execution. `facts` is a comma-separated list of fixed,
case-sensitive HostQualityFact names; input order and duplicates are preserved.

`ADAPTER_METADATA` must immediately precede one `INSTALL_BASELINE` or `DEPTH_UPDATE`. It is
inbound wire quality only; Host quality is carried by `SNAPSHOT_REQUEST`, and Core-derived
quality is never replay input. Phase 2 extended the ordering rule from `DEPTH_UPDATE`-only to
also allow `INSTALL_BASELINE`, matching the approved M4 adapter dimension table for baseline
inbound quality; this is backwards compatible with all Phase-1 fixtures
(see `docs/M5_PHASE2_DIFFERENTIAL_ORACLE.md`).

All symbolic identifiers use the conservative ASCII set `[A-Za-z0-9._:/+-]`, with `-` reserved
for absence. Integers are unsigned ASCII decimal with no `+`, leading zero, hex, octal, scientific
notation, or overflow. Canonical files are UTF-8 without BOM, LF-only, exactly one final LF, with
no blank lines, tabs, leading/trailing whitespace, multiple spaces, or event comments.

## Manifest

The checked-in manifest is a canonical text file named `manifest.txt`:

```text
MANIFEST_V1
fixture_id=<identifier>
schema_version=REPLAY_V1
log_sha256=<64 lowercase hex characters>
market=<Spot|UsdMPerpetual>
symbol=<identifier>
price_scale=<uint32>
quantity_scale=<uint32>
policy=<Spot|UsdMPerpetual>
event_count=<uint64>
```

Optional `provenance_<name>=<identifier>` records are allowed after the required fields. Fixture
loading hashes the exact replay-log bytes, including its final LF, and fails closed on hash,
event-count, fixture-ID, market, symbol, NumericSpec, policy, or provenance mismatch.

## Recorded-Corpus Materializer Boundary

The materializer accepts an explicitly supplied immutable source archive/root and an expected
provenance identity. It does not discover live Binance data, connect to Recorder runtime, use
Python, WebSocket, REST, or network services, and it does not copy Recorder implementation.
Before conversion it verifies the Recorder commit, deployed Wheel SHA-256, config SHA-256, run
identity, raw chunk identities/hashes, market/symbol, and source interval. Mutable host paths are
not semantic identity.

The source-of-truth chain is:

```text
Recorder immutable live Raw -> verified source manifest/catalog -> offline materializer
-> canonical replay-log v1 -> provenance manifest
```

Spot uses the accepted M3/ADR-0005 `L` bridge contract. The REST depth snapshot lastUpdateId is
`L`, and the buffer holds all diff-depth events received from stream start through snapshot
acquisition. Pre-bridge classification is: `u < L` is stale and discarded; `u == L` is a
duplicate/non-advancing event and cannot form a bridge. The first advancing bridge must contain
`L` and advance beyond it: `U <= L < u`. An advancing candidate with `U > L` (including the
exact-next range beginning at `L + 1`) is a forward gap. `L == UINT64_MAX` cannot form an
advancing bridge. Bootstrap semantics are distinct from the post-synchronization live successor
rule: after bridging, each live event must cover `local_last_update_id + 1` per the accepted M3
live Spot policy and then advance to its `u`. A missing bridge, forward gap, source integrity
failure, or provenance failure rejects the fixture and requires resync/rebaseline.

USD-M retains the buffered `U/u/pu` stream through snapshot acquisition, discards events with
`u < L`, accepts the first event satisfying `U <= L <= u`, then requires `pu == local_last_update_id`
for every later event and advances to `u`. Missing/mismatched `pu`, gaps, source integrity failure,
or provenance failure rejects the fixture and requires resync/rebaseline.

No medium or large corpus is acquired in Phase 1.
