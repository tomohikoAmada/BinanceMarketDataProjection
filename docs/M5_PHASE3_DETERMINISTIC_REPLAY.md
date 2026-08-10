# M5 Phase 3 Deterministic Replay Validation

## Status

- Phase 1: **COMPLETE / MERGED** (PR #11, merge
  `5e8629a7ff825f8ea941304d9b09be1670643e8a`, main CI `31264500905` — PASS 16/16)
- Phase 2: **COMPLETE / MERGED** (PR #12, merge
  `75c619dd683ff2a3893f9535e206231e7bfecc41`, main CI `31315421548` — PASS 16/16)
- Phase 2 final review: **APPROVED**; P0: 0; P1: 0; M5-P2-IR-001 through
  M5-P2-IR-007 and M5-P2-RR-001 CLOSED
- Phase 3: **IMPLEMENTED / PENDING INDEPENDENT REVIEW** (PR #13)
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
7. selects the first **valid** baseline at/after T0 and proves the accepted Spot or USD-M bridge;
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

The exact formal archive was located and validated on the production host `opihome`
(aarch64 / RK3588, 192.168.0.118). The external archive is mounted:

```text
/dev/sda1 on /mnt/binance-archive-4tb (exfat, UUID 67EF-62E0)
```

Exact run evidence root:

```text
/var/tmp/bmdr-m21-4-deploy-postmerge-20260804T023200Z/preflight/m21-4-24h-20260805T150930Z/
```

Run identity verified against `run-start.json` / `run.json` / anchors:
`preflight/m21-4-24h-20260805T150930Z/`, T0 `2026-08-05T15:09:30.200566Z`,
end `2026-08-06T15:09:30.200566Z`, production code commit
`cf1e749c7a533e916dbfb685212e5549a38c70dd`, deployed Wheel SHA-256
`926615b09ef46130f49a87fe8ab20acb7cfa6313daa67af5b718931bd95ff329` (verified against the
deployment artifact), production config SHA-256
`a399e647faaac58b5db24e835f1c29e799c70ad0c94ec77b597cac2647cfb734` (verified against
`/etc/binance-market-data-recorder/recorder.toml`), result PASS.

Source inventory was built strictly from verified evidence and staged outside all production/archive
roots:

```text
build/m5-authoritative-source/
  catalog.sqlite              (trimmed Catalog copy; rows byte-matched to production Catalog)
  artifacts/*.bmdr.zst        (2887 immutable external-archive chunks, zstd-frame.v1)
  manifests/*.manifest.json   (byte-exact internal Raw manifests)
  m5_source_inventory.json    (M5_RECORDER_SOURCE_INVENTORY_V1)
```

The staged set is the minimum source set satisfying all three requirements: the full formal-window
Spot/USD-M `depth_snapshot` + `diff_depth` chunk set (2,887 chunks, 505.6 MB compressed) including
the planned-rotation window. Every artifact's stored SHA-256 and every manifest SHA-256 were
verified source-to-staging and staging-to-local with zero mismatches. The inventory's Catalog SHA-256
is `f7289fcc3383063c5e3b83e65201df29503cf7c9bba227dbb8298dcdb4805d8c`.

### First-run rejection and materializer correction

The first authoritative run was rejected fail-closed:

```text
materialization=FAIL reason=Spot bootstrap forward gap before valid bridge
```

Diagnosis against the actual source showed the recorded data follows the Recorder reconstructor's
documented bootstrap authority at commit `cf1e749c` (R-034 open conflict):

```text
Spot  accepts U <= snapshot.last_update_id + 1 <= u
USD-M accepts U <= snapshot.last_update_id <= u
```

The planned-rotation resync at `2026-08-06T06:43:11Z..06:43:23Z` recorded its REST baselines after
the rotated diff stream resumed, so the first baseline's bridge diff covers the successor
(`U = last_update_id + 1` for Spot) rather than the snapshot's own `last_update_id`. The USD-M
resync retried with four snapshots (`06:43:25.936Z`, `06:43:27.475Z`, `06:43:29.845Z`,
`06:43:34.164Z`); only the second and later snapshots can bridge the resumed diff chain
(the first is `SNAPSHOT_TOO_OLD` in Recorder terms).

This is a materializer/Recorder-authority mismatch (the "Case C" class), not corrupt or missing
source. The minimum correction in `tools/m5_recorded_corpus_materializer.py`:

- Spot bootstrap target is now `last_update_id + 1` (mirroring R-034);
- the baseline is the first recorded snapshot whose bridge can be proven, skipping
  `SNAPSHOT_TOO_OLD` snapshots in receive order and failing closed when none can synchronize.

USD-M U/u/pu bridge and live semantics were already correct and are unchanged. Fail-closed behavior
is preserved and covered by updated/added unit tests (17 tests, including both compiled validators).

### Mandatory medium fixtures

Both medium fixtures materialized PASS from the authoritative source (price/quantity scale 8/8,
100,000 target live updates after synchronization):

| Fixture | Target live updates | Total replay operations | Replay log SHA-256 |
|---|---:|---:|---|
| `M5-REC-SPOT-BTCUSDT-V1` | 100,000 | 100,002 | `9e9831231192938ac1bd21c90b157ec17e8e2d4e8034131eb21ba57c99b2cc9d` |
| `M5-REC-USDM-BTCUSDT-V1` | 100,000 | 100,002 | `d28ffe19e134e4d5d1c4d57a60762e8884dee676c858587224aebf8afed29afc` |

Total replay operations = 1 baseline + 1 synchronization bridge + 100,000 live updates; the extra
operations over 100,000 are the accepted baseline/bridge events and are not a count defect.

| Evidence | Spot | USD-M |
|---|---|---|
| Baseline snapshot receive | 2026-08-06T06:43:24.441592Z | 2026-08-06T06:43:27.475072Z |
| Baseline source chunk | `f49af519-2332-47cc-9b19-692f77574281` | `3e1802ac-350b-4fb1-af3d-df7120ecd7d3` |
| Baseline `lastUpdateId` | 98288147167 | 11224041769040 |
| First retained diff chunk | `20ed2f72-7cb8-4132-9275-4b4d748589c5` | `06f632bc-4b4c-4ebd-98ca-7fbe7fd172ad` |
| Last retained diff chunk | `52edda7b-bd56-4129-82aa-eb5a38c04bf0` | `0fd08708-bcb9-400c-a317-13e84ab7c01b` |
| Source chunks consumed | 169 | 172 |
| Actual consumed interval | 06:43:24.441Z .. 09:30:40.066Z | 06:43:27.475Z .. 09:53:28.883Z |
| Core differential | **PASS** | **PASS** |
| Adapter differential | **PASS** | **PASS** |
| Final checkpoint | `CHECKPOINT NEEDS_RESYNC last_update_id=98288147167` `last_gap={first=98288147168 final=98288147175 reason=SPOT_BOOTSTRAP_FORWARD_GAP}` | `CHECKPOINT NEEDS_RESYNC last_update_id=11224984048179` |

Repeated materialization with different metadata-only conversion timestamps produced byte-identical
`replay.log`, `manifest.txt`, and provenance (only the explicitly metadata-only
`conversion_timestamp` differs) for both markets — **Materializer Determinism: PASS**. Repeated
Core/Adapter replay over the repeated outputs shows no divergence, identical event counts, identical
replay SHA-256, and the same final checkpoints — **repeatability: PASS**.

All authoritative/staged corpus data remains outside Git history under the ignored `build/`
directory; no medium payload, Raw chunk, Catalog copy, or source manifest is committed.

### Rotation diagnostic

Requested interval: approximately `2026-08-06T06:43:11Z` through `2026-08-06T06:43:23Z`, with a
design source window from `2026-08-06T06:33:11Z` through `2026-08-06T06:53:23Z`. Actual source
coverage and depth/generation records were inspected from the authoritative archive:

- Each market shows exactly one diff-depth receive-time gap in the window: Spot
  `06:43:05.266550Z -> 06:43:24.358157Z` (19.1 s, chunk `8dbf4547` -> `20ed2f72`) and USD-M
  `06:43:04.693506Z -> 06:43:25.993641Z` (21.3 s, chunk `5b24ec2a` -> `06f632bc`), aligned with the
  planned rotation and matching a single connection/generation transition per market
  (Spot `a9109cc1` -> `a781779c`; USD-M `ed09a5b6` -> `5fba1a1c`).
- REST resync snapshots immediately follow the gap: Spot one snapshot at `06:43:24.441592Z`
  (`lastUpdateId=98288147167`); USD-M four resync-retry snapshots at `06:43:25.936749Z`,
  `06:43:27.475072Z`, `06:43:29.845209Z`, `06:43:34.164671Z`.
- Both bridges are proven by the materializer itself: Spot `U = lastUpdateId + 1`
  (R-034 rule), USD-M first valid snapshot `U <= lastUpdateId <= u` with continuous `pu` chain
  afterwards; the 100,000-live-update selection then proves post-resync diff continuity.
- The USD-M `book_ticker` backpressure recovery events (pre-window `13:44:08`/`14:12:18`/`14:35:26`
  and formal-window gen5 `13:46:28`/gen6 `14:48:33`) do not create any diff-depth gap and are not
  used to manufacture a gap or rebaseline.

Eligibility: **ELIGIBLE**. The rotation window shows the planned depth resync with authoritative
snapshot/diff records and materializer-proven bridges; no gap or rebaseline is manufactured.

## Strict phase boundary

Phase 4 semantic observation serialization/digests/manifests and cross-compiler artifact transport
are not implemented. Phase 5 fuzzing, Phase 6+ benchmarks/allocation/container work, Phase 10 CI
jobs/workflows, and M6 remain not started. M4-IIR-3 remains DEFERRED / NON-BLOCKING.
