# M5 Phase 3 Deterministic Replay Validation

## Status

- Phase 1: **COMPLETE / MERGED** (PR #11, merge
  `5e8629a7ff825f8ea941304d9b09be1670643e8a`, main CI `31264500905` — PASS 16/16)
- Phase 2: **COMPLETE / MERGED** (PR #12, merge
  `75c619dd683ff2a3893f9535e206231e7bfecc41`, main CI `31315421548` — PASS 16/16)
- Phase 2 final review: **APPROVED**; P0: 0; P1: 0; M5-P2-IR-001 through
  M5-P2-IR-007 and M5-P2-RR-001 CLOSED
- Phase 3: **PARTIAL / BLOCKED ON RECORDED SOURCE EVIDENCE**
- Phase 4: **NOT STARTED**
- M6: **NOT STARTED**

Phase 3 remains test/tool-only. `include/**`, `src/**`, production Core, production ProtoAdapter,
Contracts, Recorder, Conan, and CI workflows are unchanged. No medium payload or source Raw archive
is committed.

## Base gate

Implementation started from exact clean base
`75c619dd683ff2a3893f9535e206231e7bfecc41`. Phase-2 post-merge main run `31315421548` was a
completed successful `push` for that SHA with 16/16 successful jobs, including quality, ASan,
UBSan, fuzz smoke, benchmark smoke, all three release jobs, and every M4 static/shared job.

## Implemented foundation

### Deterministic small tier

`tests/m5/phase3/small_workload.cpp` deterministically generates two canonical Replay_V1 fixtures.
Both generated logs immediately pass the Phase-1 byte/parser/manifest/identity loader before the
Phase-2 driver sees them.

| Workload | Events | Generator | Replay-log SHA-256 |
|---|---:|---|---|
| `m5-small-spot-v1` | 2,048 | `m5-small-generator-v1`, seed `548746690337` | `14f44a6794af5c87fb3b359e16c1901a284e6da7946f02d6be3dc8cacb249227` |
| `m5-small-usdm-v1` | 2,048 | `m5-small-generator-v1`, seed `548746690337` | `f3e60732c8e4452f86231548cd0c5918b914487063d3f4f22d6588f088141e03` |

The workloads cover Spot and USD-M baseline/bridge/sustained updates, stale and duplicate events,
Spot forward gaps, USD-M `pu` discontinuities, reset/rebaseline/recovery cycles, locked/crossed
books, inbound adapter quality metadata, snapshot requests, and gap-context snapshot production.
Core-only and adapter-enabled production/reference executions compare every operation result,
checkpoint, and snapshot observation. Repeated executions compare equal as typed values.

### Scale and diagnostics

`ReplayDriver` adds a test-only `RetainNone` policy. Both sides still execute and compare every
event in the accepted Phase-2 order; only successful historical observations are omitted from the
returned vector. The final typed observation remains available for repeatability/final-checkpoint
evidence. `RetainAll` remains the default, and tests prove both policies reach the same final typed
observation.

`render_divergence` produces stable failure-only text containing fixture/log identity, exact event
and source line, event kind, layer/category, both values, both checkpoints, NumericSpec, policy,
market/symbol, and provenance. It is not an OperationObservation stream, semantic digest, or Phase-4
manifest. A seeded mismatch at event 1,500 in the 2,048-event Spot workload proves exact first
divergence, R2 attribution, suppression of a later event-1,700 mismatch as primary, bounded
retention, and byte-identical repeated diagnostic text.

This work minimally supplies source-line context for the level-vector path naturally reached by
scaled diagnostics (the permitted M5-P2-IR-009 incidental correction). M5-P2-IR-009 otherwise
remains deferred/non-blocking; M5-P2-IR-008 is unchanged.

## Offline Recorder materializer

`tools/m5_recorded_corpus_materializer.py` is an independent standard-library offline reader for
the minimal Recorder Raw-v1 source format frozen at Recorder commit
`cf1e749c7a533e916dbfb685212e5549a38c70dd`. It does not import Recorder Python code. The actual
sealed-source path uses the `zstd` executable solely in this offline tool; this adds no production,
installed-package, Core, or ProtoAdapter dependency. Uncompressed Raw is accepted only behind a
hidden explicit deterministic-test-archive gate and cannot be relabelled as production evidence.

The materializer version is `m5-recorder-materializer-v1`. It hard-codes and fails closed against
the OD-M5-001 Recorder repository/commit, Wheel/config hashes, formal run identity, and UTC source
interval. An explicit immutable source inventory binds the Catalog SHA-256, ordered Raw chunk IDs,
artifact paths, Raw manifest paths/hashes, and storage encoding. Paths locate bytes but never enter
the replay or provenance semantic identity.

For every requested market the tool:

1. verifies the immutable inventory and exact pinned authority;
2. opens the Catalog read-only/immutable and matches chunk counts, sizes, and hashes;
3. verifies Raw manifest identity/completeness plus stored and uncompressed SHA-256;
4. decompresses sealed zstd bytes without modifying them;
5. independently validates Raw-v1 framing, canonical CBOR, CRC32C, identity, frame bounds, and
   envelope schema;
6. validates snapshot provenance and exact diff-depth JSON/sequence fields;
7. selects the first baseline at/after T0 and proves the accepted Spot or USD-M bridge;
8. proves every advancing live continuation while ignoring only semantically stale/duplicate Raw
   records;
9. emits exact canonical Replay_V1 bytes, `manifest.txt`, and `corpus_provenance.json`;
10. invokes each explicitly supplied Phase-1/differential validator and fails on any rejection.

The corpus provenance includes all required authority, source stream/chunk identities and hashes,
event count, materializer/replay versions, replay SHA-256, NumericSpec/policy, baseline and first/
last retained diff identities, actual consumed receive interval, and an explicit metadata-only
conversion timestamp. Repeating with a different conversion timestamp leaves replay bytes,
manifest, and every other provenance field identical.

Example manual invocation after building both validation modes:

```bash
python3 tools/m5_recorded_corpus_materializer.py \
  --source-root /immutable/source/root \
  --source-inventory /immutable/source/root/m5_source_inventory.json \
  --output build/m5-corpus/M5-REC-SPOT-BTCUSDT-V1 \
  --market spot \
  --target-live-updates 100000 \
  --price-scale 8 \
  --quantity-scale 8 \
  --conversion-timestamp 2026-08-09T00:00:00Z \
  --validator build/release/cmake/tests/bmd_projection_m5_corpus_validate \
  --validator build/release/cmake/tests/bmd_projection_m5_adapter_corpus_validate
```

The output directory must not already exist. Medium output belongs under an explicit local/build
corpus directory and must not be committed.

### Deterministic archive tests

Fifteen standard-library tests build independent minimal Raw-v1 archives. They cover valid Spot
and USD-M materialization across a chunk boundary, stale/duplicate input, repeatability,
metadata-only conversion time, compressed zstd source where available, CRC32C known vector,
outer-hash corruption, CRC corruption with matching outer hashes, missing chunks, incomplete
inventory, missing snapshot, wrong symbol, wrong market, missing authority metadata, Spot missing
bridge/forward gap, USD-M missing/incorrect `pu`, source ordering inversion, truncation,
existing-output refusal, validator rejection without publishing partial output, and the
production/test-source gate. Each valid fixture is passed to the compiled Phase-1 loader and Core
differential validator.

## Authoritative source availability

The exact formal archive was sought only in appropriate source locations:

- the local `BinanceMarketDataRecorder` repository;
- Recorder's documented application-data root
  `/Users/amada/Library/Application Support/BinanceMarketDataRecorder`;
- currently mounted `/Volumes` archive roots.

No directory or metadata matching `preflight/m21-4-24h-20260805T150930Z/`, the deployed Wheel hash,
or the production config hash was found. The pinned Recorder commit object is present and its Raw,
manifest/Catalog, Spot/USD-M snapshot/diff, ordering, and replay-reader authority was inspected
read-only with `git show`; the Recorder checkout and all Recorder data remained unchanged.

Therefore:

```text
PHASE 3 RECORDED-CORPUS EVIDENCE BLOCKED — AUTHORITATIVE SOURCE ARCHIVE NOT AVAILABLE
```

### Mandatory medium fixtures

| Fixture | Required source updates | Status |
|---|---:|---|
| `M5-REC-SPOT-BTCUSDT-V1` | 100,000 live updates after synchronization | BLOCKED — no authoritative archive |
| `M5-REC-USDM-BTCUSDT-V1` | 100,000 live updates after synchronization | BLOCKED — no authoritative archive |

No replay SHA-256, final checkpoint, repeated materialization/replay result, source-chunk list, or
differential PASS is claimed for either medium fixture.

### Rotation diagnostic

Requested interval: approximately `2026-08-06T06:43:11Z` through
`2026-08-06T06:43:23Z`, with a design source window from 10 minutes before through 10 minutes after.
Actual source coverage and depth/generation records cannot be inspected because the authoritative
archive is unavailable. Eligibility is therefore **BLOCKED**, not `ELIGIBLE` or `NOT ELIGIBLE`.
No transition, gap, rebaseline, or bridge is manufactured.

## Strict phase boundary

Phase 4 semantic observation serialization/digests/manifests and cross-compiler artifact transport
are not implemented. Phase 5 fuzzing, Phase 6+ benchmarks/allocation/container work, Phase 10 CI
jobs/workflows, and M6 remain not started. M4-IIR-3 remains DEFERRED / NON-BLOCKING.
