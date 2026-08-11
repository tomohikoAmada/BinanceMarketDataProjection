# M5 Phase 3 Deterministic Replay Validation

## Status

- Phase 1: **COMPLETE / MERGED** (PR #11, merge
  `5e8629a7ff825f8ea941304d9b09be1670643e8a`, post-merge main CI `31264500905` — PASS 16/16)
- Phase 2: **COMPLETE / MERGED** (PR #12, merge
  `75c619dd683ff2a3893f9535e206231e7bfecc41`, main CI `31315421548` — PASS 16/16)
- Phase 2 final review: **APPROVED**; P0: 0; P1: 0; M5-P2-IR-001 through
  M5-P2-IR-007 and M5-P2-RR-001 CLOSED
- Phase 3: **COMPLETE / MERGED** (PR #13; final approved Head
  `a8e4ccfc31efd4e67bc10cf0ac9ad2a99faa8354`; exact-head CI `31491615547` — PASS 16/16; squash
  merge `473a907eba2001d18926c57d6c8d16b10c7505be`; rebased onto accepted
  M3 successor-coverage semantics; authoritative Spot and USD-M 100k
  corpora validated PASS under ADR-0008 authority)
- Phase 4: **NOT STARTED**
- M6: **NOT STARTED**

Phase 3 remains test/tool-only. `include/**`, `src/**`, production Core, production ProtoAdapter,
Contracts, Recorder, Conan, and CI workflows are unchanged. No medium payload or source Raw archive
is committed.

## PR #13 review record

- Initial Phase-3 implementation head: `1139101` foundation, review **PARTIAL / BLOCKED** with
  P0: 0, P1: 3, P2: 2.
- Rejected head: `ea2afc599f1f4257be06c35d07a1b0df2ba93167`; rejected exact-head CI
  `31356548799` — PASS 16/16 but semantic review rejected (CI PASS is necessary but not
  sufficient).
- Blocking findings and dispositions:

| Finding | Disposition |
|---|---|
| M5-P3-IR-001 — Spot materializer changed accepted Projection bootstrap semantics from `U <= L < u` to `U <= L+1 <= u` | **CORRECTED / SUPERSEDED BY ADR-0008** — the historical contains-`L` restoration itself was superseded when the official 2025-11-12 Spot correction was independently reviewed: ADR-0008 is ACCEPTED and successor coverage (`U <= L + 1 <= u`, overflow-guarded) is the authoritative Spot bootstrap rule. The materializer now implements successor coverage, and the Phase-3 exact-next regression is restored and extended. |
| M5-P3-IR-002 — medium validators proved only production/reference equality and could PASS a workload that immediately enters NeedsResync | **CORRECTED** — medium lifecycle validity gate enforced by both validators |
| M5-P3-IR-003 — USD-M materialization claimed a continuous 100k post-sync pu chain but committed evidence recorded final NeedsResync | **CORRECTED** — regenerated evidence shows bridge Applied / Synchronized, 100,001 Applied, final Synchronized; the previously recorded NEEDS_RESYNC checkpoint was stale and has been replaced by actual current program output |
| M5-P3-IR-004 — string-based snapshot-retry error classification | **CORRECTED** — structured `BridgeEligibilityError` retry category; post-bridge live failures are never retried as snapshot errors |
| M5-P3-IR-005 — stale PR/README/MILESTONES lifecycle status | **CORRECTED** — status documentation synchronized with the actual disposition |

## Final review and merge record

```text
PR #13:
APPROVED
P0: 0
P1: 0
New blocking regression: NONE

M5-P3-RR2-001:
CLOSED

M5-P3-RR2-002:
PARTIALLY CLOSED / ACCEPTED NON-BLOCKING P2

Final approved PR Head:
a8e4ccfc31efd4e67bc10cf0ac9ad2a99faa8354

Exact-head CI:
31491615547 — PASS 16/16

Squash merge:
473a907eba2001d18926c57d6c8d16b10c7505be

PR #13:
SQUASH MERGED / CLOSED
```

M5-P3-RR2-002 remains an accepted non-blocking P2 (narrow `NOLINTNEXTLINE` instances); it does not
block Phase 3 completion and is not fixed by this phase.

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

## Medium lifecycle validity gate

### Contract

A mandatory materializer-generated medium corpus must be judged valid only when BOTH hold:

1. **differential equality** — production and reference agree on every event in the accepted
   Phase-2 first-divergence order; and
2. **medium lifecycle validity** — the emitted corpus itself satisfies the intended lifecycle.

Differential equality on its own is NOT medium validity: a run where both sides agree on
`GapDetected`/`NeedsResync`/`RejectedWrongState` is a FAILED corpus, not a PASS.

The emitted Replay_V1 shape for the mandatory medium corpus is:

```text
1 INSTALL_BASELINE
1 selected bootstrap bridge
N selected post-bridge advancing live updates    (N = target_live_updates)
```

The lifecycle gate therefore requires (for `target_live_updates = 100000`):

| Invariant | Required value |
|---|---|
| Baseline install result | `Installed`, status after `AwaitingBridge` |
| First emitted DEPTH_UPDATE (bridge) | `Applied`, status after `Synchronized` |
| Every emitted DEPTH_UPDATE | `Applied`, status after `Synchronized` |
| `Applied` count | 100,001 (1 bridge + 100,000 live) |
| `GapDetected` count | 0 |
| `RejectedWrongState` count | 0 |
| Adapter errors | 0 |
| Final status | `Synchronized` |
| Final accepted update ID | last selected source diff `final_update_id` |

### Implementation

- `ReplayDriver` accumulates a neutral compact `ExecutionSummary` (typed dispositions/statuses and
  deterministic counters only) AFTER production/reference equality for each event is established.
  It contains no Spot/USD-M classification, decimal parsing, book mutation, or adapter mapping;
  `RetainNone` still omits full historical observations.
- `tests/m5/phase3/medium_validity.{hpp,cpp}` implements the deterministic lifecycle gate
  `check_medium_validity` over the already-compared typed observations and the fixture's own
  operation sequence, plus a strict minimal reader for the materializer's canonical
  `corpus_provenance.json` intent (`selected_live_updates_after_synchronization`).
- `tests/m5/phase3/corpus_validation_common.hpp` shares the validator body between the Core-only
  and adapter validator executables; the Core validator still never links ProtoAdapter.
- Both validators emit per-run lifecycle evidence
  (`baseline_result`, `status_after_baseline`, `bridge_result`, `status_after_bridge`,
  `applied_count`, `ignored_stale_count`, `ignored_duplicate_count`, `gap_detected_count`,
  `rejected_wrong_state_count`, `adapter_error_count`, `final_status`,
  `final_accepted_update_id`, `last_selected_diff_final_update_id`) and fail with a stable typed
  reason such as `bridge-not-applied`, `depth-update-not-applied`, `final-status-NeedsResync`,
  or `unexpected-applied-count`.
- Differential failure still stops at the existing earliest-divergence gate; the lifecycle gate
  never hides or replaces a differential failure.
- The materializer corpus provenance now additionally records `bootstrap_bridge` (bridge diff
  identity plus its `U`/`u`) and `final_selected_update_id`, so the emitted intent is checkable
  against validator evidence.

### Negative tests

`medium_validity_test.cpp` (Core) and `medium_validity_adapter_test.cpp` (adapter) prove:

- equal-but-invalid lifecycle (baseline installs, first depth GapDetected, remaining
  RejectedWrongState; production/reference agree) FAILS with stable reason `bridge-not-applied`
  in both modes;
- valid bridge followed by a live gap FAILS;
- valid bridge followed by RejectedWrongState FAILS;
- final status NeedsResync FAILS;
- wrong expected applied count FAILS;
- final accepted ID not equal to last selected diff final ID FAILS;
- differential mismatch still fails at the earliest-divergence event;
- `RetainNone` still accumulates an identical summary;
- stale/duplicate typed results are counted but never silently accepted in the emitted chain
  (the materializer filters them during selection).
- a DIRECT Spot successor-conformance table locks, independently of production/reference
  equality, every ADR-0008 classification against `C=500`: stale `[400,499]`, duplicates
  `[499,500]`/`[500,500]`, bridges `[499,501]`/`[500,501]`/`[501,501]`/`[501,502]`/`[499,502]`
  (all Applied/Synchronized), and true gaps `[502,502]`/`[502,503]` (GapDetected/NeedsResync).
  The exact-next `[501,501]` row fails the test if BOTH production and reference models are
  reverted to contains-`L`;
- a live exact-next chain after a `[501,501]` bridge (`[502,502]`, `[503,503]`) remains
  Applied/Synchronized with zero gaps;
- the strict minimal JSON reader accepts `18446744073709551615` (UINT64_MAX) and rejects
  `18446744073709551616`, `36893488147419103231`, and very large decimal integers
  (checked-before-multiply uint64 overflow guard).

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
10. invokes each explicitly supplied validator and fails on any rejection.

### Accepted bootstrap authority (ADR-0008)

Baseline selection follows the accepted M3/ADR-0008 contract (SOT-001/SOT-002/SOT-003; the
Phase-1 `classify_spot_bootstrap` decision table):

- **Spot**: bootstrap target is the snapshot `lastUpdateId` (`L`). `u < L` is stale and
  discarded; `u == L` is a non-advancing duplicate and cannot form a bridge; the first
  advancing bridge must cover the overflow-safe successor of `L`: `U <= L + 1 <= u`.
  Exact-next input beginning at `L + 1` is a valid bridge. An advancing candidate with
  `U > L + 1` is `SpotBootstrapForwardGap`. `L == UINT64_MAX` can never form an advancing
  bridge and no successor arithmetic is performed (`L + 1` is never evaluated). Source update
  IDs are bounded to the uint64 domain at the materializer boundary.
- **USD-M**: `u < L` is discarded; the first relevant bridge satisfies `U <= L <= u` and carries
  `pu`; afterwards every advancing event requires `pu == previous accepted u`. USD-M semantics
  are unchanged by ADR-0008.
- A snapshot that cannot establish any bridge (including one received outside the formal source
  interval) is skipped in receive order and the next recorded snapshot is tried; the first
  eligibility failure is reported if no snapshot synchronizes. A live continuity failure after a
  proven bridge is NEVER retried against another snapshot: it propagates immediately
  (regression test `test_post_bridge_live_failure_is_not_retried_as_snapshot_error`).

Historical note: the earlier Phase-3 iteration that "restored" the contains-`L` rule
(`U <= L < u`, exact-next rejected) mirrored the then-current review correction of ADR-0005 and
Recorder R-034. That restoration was itself superseded: the official 2025-11-12 Spot
instruction correction and the official example predicate `U <= last_update_id + 1 <= u` were
independently reviewed and recorded in ADR-0008 (ACCEPTED), which supersedes only the
Spot-bootstrap contains-`L` portion of ADR-0005. USD-M semantics are unchanged. Recorder
reconstructor behavior explains how the recorded bytes look; it does not override the accepted
Projection M3 sequence policy, and the materializer now reproduces the projection's accepted
successor-coverage semantics.

### Deterministic archive tests

Twenty-six standard-library tests build independent minimal Raw-v1 archives. They cover valid Spot
and USD-M materialization across a chunk boundary, stale/duplicate input, repeatability,
metadata-only conversion time, compressed zstd source where available, CRC32C known vector,
outer-hash corruption, CRC corruption with matching outer hashes, missing chunks, incomplete
inventory, missing snapshot, wrong symbol, wrong market, missing authority metadata, Spot
successor-coverage bridge acceptance (`U=99,u=101`, `U=100,u=101`, exact-next `U=101,u=101`,
and exact-next-wide `U=101,u=103` against `L=100`), Spot true bootstrap gap rejection
(`U=102,u=102` and `U=102,u=103` against `L=100`), the authoritative pinned Spot exact-next
case (`L=98288147167`, bridge `U=98288147168 u=98288147175` accepted), Spot stale/duplicate
non-bridging, Spot `L == UINT64_MAX` non-bridging, source IDs outside uint64, Spot live exact-next
validity after a bridge, Spot and USD-M snapshot skipping (too old / outside formal interval),
post-bridge live failure propagation, USD-M missing/incorrect `pu`, source ordering inversion,
truncation, existing-output refusal, validator rejection without publishing partial output, and
the production/test-source gate. Each valid fixture is passed to the compiled Phase-1 loader and
Core differential validator.

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

## Spot authoritative materialization and validation

The same pinned authoritative source was re-scanned under the accepted ADR-0008 successor-coverage
Spot bootstrap predicate. The in-window Spot `depth_snapshot` baseline is:

| Snapshot | Receive time | `L` | In formal interval | Bridge evidence |
|---|---|---|---|---|
| `f49af519-2332-47cc-9b19-692f77574281` | `2026-08-06T06:43:24.441592Z` | `98288147167` | yes | resumed diff `U=98288147168 u=98288147175` — exact-next (`U = L + 1`), valid bridge under ADR-0008 |
| `f49af519-2332-47cc-9b19-692f77574281` | `2026-08-07T06:33:35.555641Z` | `98326157481` | no (outside formal interval) | not usable |

The in-window snapshot's bridge diff begins exactly at `L + 1` (`U = 98288147168`), which is a
valid exact-next Spot bootstrap bridge under the accepted `U <= L + 1 <= u` contract. The
historical contains-`L` rejection of this case (`SpotBootstrapForwardGap`) is superseded by
ADR-0008; the materializer therefore now materializes PASS:

```text
materialization=PASS fixture_id=M5-REC-SPOT-BTCUSDT-V1
event_count=100002
replay_log_sha256=9e9831231192938ac1bd21c90b157ec17e8e2d4e8034131eb21ba57c99b2cc9d
```

`M5-REC-SPOT-BTCUSDT-V1` materialized PASS from the pinned authoritative source
(price/quantity scale 8/8, 100,000 target live updates):

| Evidence | Value |
|---|---|
| Baseline snapshot | `f49af519-2332-47cc-9b19-692f77574281`, receive `2026-08-06T06:43:24.441592Z` |
| Baseline `lastUpdateId` (`L`) | `98288147167` |
| Bridge diff | `U=98288147168 u=98288147175` — exact-next (`U = L + 1`), valid bridge under ADR-0008 successor coverage |
| Total replay operations | 100,002 (1 baseline + 1 bridge + 100,000 live) |
| Replay log SHA-256 | `9e9831231192938ac1bd21c90b157ec17e8e2d4e8034131eb21ba57c99b2cc9d` |
| Final selected update ID | `98291309925` |
| Core validator | differential PASS, **medium_validation PASS** |
| Adapter validator | differential PASS, **medium_validation PASS** |

Validator lifecycle evidence (identical for Core and Adapter runs):

```text
baseline_result=INSTALLED            status_after_baseline=AWAITING_BRIDGE
bridge_result=APPLIED                status_after_bridge=SYNCHRONIZED
installed_count=1                    applied_count=100001
ignored_stale_count=0                ignored_duplicate_count=0
gap_detected_count=0                 rejected_wrong_state_count=0
adapter_error_count=0                final_status=SYNCHRONIZED
final_accepted_update_id=98291309925
last_selected_diff_final_update_id=98291309925
```

The earlier claim that the pinned in-window Spot archive is INELIGIBLE (because the only
advancing candidate was exact-next `U = L + 1`) rested on the superseded contains-`L` predicate
and is no longer valid. No alternative semantics were invented, no snapshot was forced, and no
synthetic event was created: the exact-next `U = L + 1` sequence is the exchange's own normal
successor pattern and is valid Spot bootstrap evidence under ADR-0008.

Repeatability: a second materialization of the same immutable source with a different
metadata-only conversion timestamp produced byte-identical `replay.log`, `manifest.txt`, and
provenance (only the explicitly metadata-only `conversion_timestamp` differs) — **Materializer
Determinism: PASS**. Repeated Core/Adapter validation over the repeated output shows no
divergence, identical counts, identical replay SHA-256, and identical final checkpoints —
**repeatability: PASS**.

## USD-M authoritative materialization and validation

`M5-REC-USDM-BTCUSDT-V1` materialized PASS from the pinned authoritative source
(price/quantity scale 8/8, 100,000 target live updates):

| Evidence | Value |
|---|---|
| Baseline snapshot | `3e1802ac-350b-4fb1-af3d-df7120ecd7d3`, receive `2026-08-06T06:43:27.475072Z` |
| Baseline `lastUpdateId` (`L`) | `11224041769040` |
| Bridge diff | `06f632bc-4b4c-4ebd-98ca-7fbe7fd172ad`, `U=11224041767914 u=11224041776042 pu=11224041767810` (`U <= L <= u`) |
| First retained diff chunk | `06f632bc-4b4c-4ebd-98ca-7fbe7fd172ad` |
| Last retained diff chunk | `0fd08708-bcb9-400c-a317-13e84ab7c01b` |
| Source chunks consumed | 172 |
| Total replay operations | 100,002 (1 baseline + 1 bridge + 100,000 live) |
| Replay log SHA-256 | `d28ffe19e134e4d5d1c4d57a60762e8884dee676c858587224aebf8afed29afc` |
| Final selected update ID | `11224984048179` |
| Core validator | differential PASS, **medium_validation PASS** |
| Adapter validator | differential PASS, **medium_validation PASS** |

Validator lifecycle evidence (identical for Core and Adapter runs):

```text
baseline_result=INSTALLED            status_after_baseline=AWAITING_BRIDGE
bridge_result=APPLIED                status_after_bridge=SYNCHRONIZED
installed_count=1                    applied_count=100001
ignored_stale_count=0                ignored_duplicate_count=0
gap_detected_count=0                 rejected_wrong_state_count=0
adapter_error_count=0                final_status=SYNCHRONIZED
final_accepted_update_id=11224984048179
last_selected_diff_final_update_id=11224984048179
```

The earlier report row `CHECKPOINT NEEDS_RESYNC last_update_id=11224984048179` was stale and
inaccurate; actual current program output proves the regenerated corpus ends Synchronized with
100,001 Applied operations and zero gaps. Repeated materialization with a different metadata-only
conversion timestamp produced byte-identical `replay.log`, `manifest.txt`, and provenance (only the
explicitly metadata-only `conversion_timestamp` differs) — **Materializer Determinism: PASS**.
Repeated Core/Adapter validation over the repeated output shows no divergence, identical counts,
identical replay SHA-256, and identical final checkpoints — **repeatability: PASS**.

All authoritative/staged corpus data remains outside Git history under the ignored `build/`
directory; no medium payload, Raw chunk, Catalog copy, or source manifest is committed.

## Rotation diagnostic

Requested interval: approximately `2026-08-06T06:43:11Z` through `2026-08-06T06:43:23Z`, with a
design source window from `2026-08-06T06:33:11Z` through `2026-08-06T06:53:23Z`. Actual source
coverage and depth/generation records were inspected from the authoritative archive:

- Each market shows exactly one diff-depth receive-time gap in the window: Spot
  `06:43:05.266550Z -> 06:43:24.358157Z` (19.1 s, chunk `8dbf4547` -> `20ed2f72`) and USD-M
  `06:43:04.693506Z -> 06:43:25.993641Z` (21.3 s, chunk `5b24ec2a` -> `06f632bc`), aligned with the
  planned rotation and matching a single connection/generation transition per market
  (Spot `a9109cc1` -> `a781779c`; USD-M `ed09a5b6` -> `5fba1a1c`).
- REST resync snapshots immediately follow the gap: Spot one in-window snapshot at
  `06:43:24.441592Z` (`lastUpdateId=98288147167`); USD-M resync-retry snapshots at
  `06:43:25.936749Z` (ineligible), `06:43:27.475072Z` (selected baseline
  `lastUpdateId=11224041769040`), `06:43:29.845209Z`, `06:43:34.164671Z`.
- The USD-M bridge is proven by the materializer under the accepted USD-M rule
  (`U <= L <= u` with present `pu`) and the 100,000-live-update selection proves continuous `pu`
  continuity afterwards; the strengthened validators prove every selected operation was Applied
  with the projection Synchronized.
- The Spot rotation bridge is valid under the accepted ADR-0008 successor-coverage rule: the
  resumed diff stream begins exactly at `U = L + 1` (`U=98288147168 u=98288147175`), which is the
  exchange's normal successor pattern and a valid bootstrap bridge. The same source produced the
  Spot 100k corpus validated PASS (see Spot authoritative materialization above).
  Historical note: the earlier contains-`L` interpretation classified this as
  `SpotBootstrapForwardGap`; that interpretation was superseded by ADR-0008.

Eligibility:

```text
Rotation Spot:  ELIGIBLE   (ADR-0008 successor-coverage bridge; validator-proven continuous chain)
Rotation USD-M: ELIGIBLE   (materializer-proven bridge; validator-proven continuous pu chain)
```

The two market results are NOT combined into an unqualified global ELIGIBLE claim. The USD-M
`book_ticker` backpressure recovery events (pre-window `13:44:08`/`14:12:18`/`14:35:26` and
formal-window gen5 `13:46:28`/gen6 `14:48:33`) do not create any diff-depth gap and are not
used to manufacture a gap or rebaseline.

## Strict phase boundary

Phase 4 semantic observation serialization/digests/manifests and cross-compiler artifact transport
are not implemented. Phase 5 fuzzing, Phase 6+ benchmarks/allocation/container work, Phase 10 CI
jobs/workflows, and M6 remain not started. M4-IIR-3 remains DEFERRED / NON-BLOCKING.
M5-P2-IR-008 and M5-P2-IR-009 remain DEFERRED / NON-BLOCKING (except the permitted incidental
source-line context). M5-P3-IR-004 is resolved by the structured retry-error correction.

## Phase 3 disposition

```text
Phase 3:
COMPLETE / MERGED
(PR #13, final approved Head a8e4ccfc31efd4e67bc10cf0ac9ad2a99faa8354,
 exact-head CI 31491615547 — PASS 16/16, squash merge 473a907eba2001d18926c57d6c8d16b10c7505be;
 rebased onto accepted ADR-0008 successor coverage; Spot and USD-M 100k corpora validated)

Spot medium:   VALID (exact-next bridge Applied / Synchronized under ADR-0008)
Spot 100k:     ESTABLISHED (bridge U=98288147168 u=98288147175 against L=98288147167;
             100,000 post-synchronization live updates all Applied)
USD-M medium:  VALID (bridge Applied / Synchronized, 100,001 Applied, final Synchronized)
USD-M 100k:    ESTABLISHED (100,000 post-synchronization live updates all Applied)

M5-P3-IR-001:  CORRECTED / SUPERSEDED BY ADR-0008 (successor coverage accepted)
M5-P3-IR-002:  CORRECTED
M5-P3-IR-003:  CORRECTED
M5-P3-IR-004:  CORRECTED (structured retry error category)
M5-P3-IR-005:  CORRECTED (status documentation synchronized)
M5-P3-RR2-001: CLOSED
M5-P3-RR2-002: PARTIALLY CLOSED / ACCEPTED NON-BLOCKING P2
```

The Phase-3 code changes remain test/tool-only. ADR-0008 acceptance is recorded on `main` (PR
#14); Phase 3 rebased onto that accepted main and changed no production Core or ProtoAdapter
code. The mandatory Spot and USD-M medium corpora are both established from the pinned
authoritative source; Phase 3 was independently approved (P0: 0, P1: 0) and merged through
PR #13 at `473a907eba2001d18926c57d6c8d16b10c7505be`.
