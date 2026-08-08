# M5 Phase 1 Canonical Replay

## Status

- M5 implementation: **IN PROGRESS**
- Phase 1: **IMPLEMENTED / PENDING INDEPENDENT REVIEW**
- Phase 2: **NOT STARTED**
- M5-PIR-001: **INCORPORATED INTO PHASE 1** - bootstrap/bridge materializer contract
- M5-PIR-002: **DEFERRED / NON-BLOCKING**
- M5-PIR-003: **DEFERRED / NON-BLOCKING** - before required branch protection
- M5-PIR-004: **INCORPORATED INTO PHASE 1** - explicit offline acquisition boundary

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

`ADAPTER_METADATA` must immediately precede one `DEPTH_UPDATE`. It is inbound wire quality only;
Host quality is carried by `SNAPSHOT_REQUEST`, and Core-derived quality is never replay input.

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

Spot uses the approved `L` bridge contract: retain the buffered diff stream through snapshot
acquisition, discard buffered events with `u < L+1`, accept the first event satisfying
`U <= L+1 <= u`, then require each later event to cover `local_last_update_id+1` and advance to
its `u`. A missing bridge, forward gap, source integrity failure, or provenance failure rejects
the fixture and requires resync/rebaseline.

USD-M retains the buffered `U/u/pu` stream through snapshot acquisition, discards events with
`u < L`, accepts the first event satisfying `U <= L <= u`, then requires `pu == local_last_update_id`
for every later event and advances to `u`. Missing/mismatched `pu`, gaps, source integrity failure,
or provenance failure rejects the fixture and requires resync/rebaseline.

No medium or large corpus is acquired in Phase 1.
