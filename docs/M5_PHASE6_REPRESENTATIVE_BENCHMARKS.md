# M5 Phase-6 Representative Benchmarks

## Status

- Status: **COMPLETE / MERGED** (PR #21)
- Decision authority: OD-M5-P6-001 through OD-M5-P6-030
  (`docs/M5_PHASE6_PREIMPLEMENTATION_DECISIONS.md`, APPROVED / MERGED)
- Phase-6 implementation authorization: **YES / CONSUMED FOR THIS IMPLEMENTATION**
- Production `src/**` and public `include/**`: **UNCHANGED**
- Phase 7 (allocation/memory): NOT STARTED
- Phase 8 (alternative container spike): NOT STARTED
- Phase 10 (reporting/workflow integration): NOT STARTED

## Completion and Acceptance

Phase 6 is **COMPLETE / MERGED** via PR #21:

- Final independently accepted implementation Head:
  `9776ba6b93990c44e550f289b69127ca721b0d00`
- Accepted exact-head CI: `31803322848` — 18/18 PASS
- Formal exact-head Phase-6 evidence: **ACCEPTED** (generated from the accepted
  implementation Head above with clean source at configure, Release, ProtoAdapter ON,
  sanitizers off, 5 benchmark repetitions, 5 latency passes, 169/169 required inventory,
  complete 48-cell M3 accepted matrix, four production replay workloads, event latency
  evidence, and source/binary/payload/workload/dependency provenance validated)
- Independent final review: **APPROVED** — P0 = 0, P1 = 0, P2 = 1
- P6-FINAL-001 (CheckedApply canonical workload identity): **CLOSED**
- P6-FINAL-002 (Contracts Conan package-ID provenance): **CLOSED**
- Squash merge commit: `227524e6d17cce77813c6f26cd65bb8d996f5677`
- Post-merge main CI: `31809917018` — 18/18 PASS

The accepted Head and the squash merge commit share the same repository tree
(`79aa0151d65d42a1c63e6649bf24bf9053105667`); the formal benchmark evidence was generated
and independently accepted for the exact implementation Head above, and the squash merge
preserves that same tree with post-merge main CI passing 18/18. No numeric performance gate
exists, and no cross-machine comparability is claimed; detailed characterization numbers are
not reproduced here.

This document describes the implemented Phase-6 benchmark infrastructure. It is not performance
evidence by itself; measured numbers live in the machine-readable result files described below.

## Scope

Representative Google Benchmark workloads for M1-M4, production Core/Adapter replay throughput,
a dedicated production-only event-latency path, versioned workload identity, a fail-closed
metadata/provenance wrapper, deterministic inventory/result validation, machine-readable output
generation, and validator tests. Google Benchmark 1.9.5 remains the benchmark framework.

Explicitly NOT implemented here: production optimization, alternative containers, allocation
instrumentation, numeric performance CI thresholds, performance artifact upload, scheduled/
manual performance workflows, historical trends, release-matrix benchmark expansion, Phase 7/8/10
work, and the Phase-8 candidate interface (Phase 8 comparisons must use
`Phase8Candidate / Phase8StdMapControl` through the same candidate interface/environment; the
Phase-6 production `std::map` system baseline freezes workloads, dimensions, timers, and
denominators).

## Benchmark families

### M1 (normative OD-M5-P6-003 table)

`M1/ParsePrice/MatchedScale`, `M1/ParsePositiveQuantity/MatchedScale`,
`M1/ParseQuantity/ZeroSuccess`, `M1/ParsePositiveQuantity/ZeroRejected`,
`M1/ParsePrice/ExactUpscale`, `M1/ParsePrice/ExactDownscale`,
`M1/ParsePrice/InexactDownscaleRejected`, `M1/ParsePrice/OverflowRejected`,
`M1/ParsePrice/SyntaxRejected`, `M1/FormatPriceFixed`, `M1/FormatQuantityFixed`.

Inputs are preconstructed outside timing; one explicit 16-operation warmup block is discarded
before measurement. Each measured iteration is a homogeneous block of 16 identical public
operations with result consumption (`DoNotOptimize` on accumulated evidence). Total logical
items are reported once after the loop as `iterations * 16`. There is no
standalone rescale benchmark; scale conversion is measured through the real parse/format public
APIs only.

### M2

- `M2/apply_level/{insert,update,delete,missing_delete}/{8,100,1000}`
- `M2/apply_updates/{1,10,100}/{8,100,1000}` (replacement-heavy operation mix)
- `M2/apply_updates/update_mix/{0,8,100,1000,5000,10000}` (primary scaling workload,
  batch=100; the D=0 cell is explicitly labelled the empty-book insertion edge)
- `M2/replace_all/{0,8,100,1000,5000,10000}` (post-state exactly canonical)
- `M2/best_bid/{8,100,1000}`, `M2/best_ask/{8,100,1000}`,
  `M2/quantity_at/hit/{8,100,1000}`, `M2/quantity_at/miss/{8,100,1000}` (separate hit and
  miss workloads; no mixed hit ratio), `M2/top_levels/{1,5,50}/{8,100,1000}` (query limit
  recorded in workload metadata), `M2/all_levels/{0,8,100,1000,5000,10000}`

Stateful-iteration invariance (OD-M5-P6-004/005): insert and delete consume one freshly prepared
book per measured execution from a bounded pool built entirely outside the measured region
(fixed iteration counts, scaled by depth); update alternates quantities so every execution
returns `Updated`; missing-delete is idempotent (`Unchanged`); apply_updates uses a 10,007-period
quantity index so consecutive applications at the same price always mutate; replace_all and all
queries are idempotent over fixed state. Disposition drift is detected in-loop and fails the
benchmark closed via `SkipWithError`.

### M3

- `M3/LiveApply/Accepted/{Spot,UsdMPerpetual}/D{0,8,100,1000,5000,10000}/B{0,1,10,100}` —
  the complete 48-cell accepted live-apply matrix, all registered and inventory-validated.
- `M3/Classification/{Stale,Duplicate,Gap,Reset,BaselineInstall}/{Spot,UsdMPerpetual}`
- `M3/Component/AllLevelsBothSides/{8,100,1000}` and
  `M3/Proxy/{CandidateRebuildFromVectors,CandidateApplyUpdates,OrderBookMoveCommit}/{8,100,1000}`
  (approximate public-API proxy measurements; never claimed as an exact
  `BookProjection::apply` decomposition)

Every accepted cell begins Synchronized and every intended accepted operation must return
`Applied` (enforced in-loop). D>0 cells use existing-price quantity updates (fixed depth, real
mutation); `B=0` is the mandatory advancing empty-level batch that traverses the real accepted
production transaction and returns `Applied`; D=0/B>0 is the labelled empty-book insertion edge
on a prepared pool. Stale/duplicate use stable non-mutating state; gap/reset/baseline-install are
one-shot against freshly prepared pools (never "first change + subsequent RejectedWrongState"
under one label). The locked PR-CI smoke subset is D{8,1000} x B{0,10} x {Spot,UsdMPerpetual}
(8 cells); all 48 cells are inventory-validated in every run.

`M3/Proxy/OrderBookMoveCommit` move-assigns into a destination holding the populated old book,
so the measurement includes destination destruction.
`M3/Proxy/CandidateApplyUpdates` consumes a bounded pool of fully constructed candidates, so
candidate rebuild is excluded and only the production `apply_updates` operation is timed.

### M4 (fail-closed availability, OD-M5-P6-013/022)

`M4/AdaptExchangeDepthSnapshot/Spot/{8,100,1000}`, `M4/AdaptDepthUpdate/Spot/{8,100,1000}`,
`M4/CheckedInstall/{8,100,1000}`, `M4/CheckedApply/{8,100,1000}`,
`M4/MakeLocalOrderBookSnapshot/{Unlimited,Limited}/{8,100,1000}`,
`M4/SerializeSnapshot/{FreshBuffer,ReusedBuffer}/{8,100,1000}` (FreshBuffer is the formal
primary; ReusedBuffer is the optional diagnostic).

Timing boundaries: adaptation excludes wire construction/install/snapshot/serialization;
CheckedInstall pre-adapts the owner outside timing and times `install_into` against freshly
prepared AwaitingBaseline projections; CheckedApply pre-adapts and times `apply_to` against
freshly prepared Synchronized projections; snapshot construction excludes `SerializeToString`.
Protobuf message construction is setup only. The benchmark-smoke job enables
`BMD_PROJECTION_BUILD_PROTO_ADAPTER=ON` and bootstraps the pinned Contracts package; a Core-only
successful benchmark run fails inventory validation.

### Production replay throughput (wall-time primary)

`CoreNormalizedReplay/{Spot,UsdMPerpetual}`: timed path is preloaded normalized replay
operations -> production M1 parsing (the production host path for normalized text input) ->
production M3 `BookProjection` -> FNV-1a evidence consumption.
`CoreProductionSide::observe()` is never the throughput executor. Excluded: canonical text
parsing, fixture I/O, hashing, generation, reference model, ReplayDriver, OperationObservation,
checkpoints, diagnostics. The small Spot/USD-M workloads have stable workload ID, generator
version (`m5-small-generator-v1`), seed (`548746690337`), canonical log SHA-256, 2,048 events,
and NumericSpec/sequence policy recorded in the workload spec. Differential verification
(production vs reference oracle) runs exactly once outside any measured region; per-iteration
checksum mismatches fail closed via `SkipWithError`. `SetItemsProcessed` counts dispatched
events once as `iterations * event_count`; `UseRealTime` makes wall time the primary denominator,
so events/second and ns/event are reciprocal views of the same result.

`AdapterWireReplay/{Spot,UsdMPerpetual}`: preconstructed valid wire -> production M4 adaptation
-> checked production M3 invocation -> minimal consumption. Wire construction, fixture parsing,
I/O, hashing, generation, reference model, and observation machinery are excluded. Snapshot
production is included without protobuf serialization (serialization is measured separately in
`M4/SerializeSnapshot`). Core and Adapter replay results remain distinct.

Differential replay throughput (`Validation/DifferentialReplayDriver`) is NOT implemented
(optional per OD-M5-P6-016); it is not part of any production baseline and no benchmark is
called "production events/sec" except the wall-denominated production replay paths.

### Event latency (dedicated executable)

`bmd_projection_replay_latency` follows OD-M5-P6-017/018/020: preload/pre-touch, parse every
decimal into immutable typed input before timing, one complete untimed typed warmup pass with
discarded state, fresh production state per pass,
preallocated sample storage, a `steady_clock` bracket around exactly one production event per
sample (typed evidence capture only; no hashing/formatting/allocation/oracle inside the
bracket), post-run final-state/checksum validation, statistics after measurement. Calibration
samples use the same storage pattern in a separate empty-bracket distribution, are reported
separately, and are NEVER subtracted from event samples. Estimator: nearest-rank-v1
(`Q(p) = x[ceil(p*n)-1]`, no interpolation). Reporting minimums: p50/p90 sample_count >= 1000;
p99 >= 10000; p99.9 >= 100000 AND unique_event_count >= 100000. Repeating the 2,048-event
workload five times yields sample_count=10240, unique_event_count=2048, passes=5 (p99 eligible
as a timing-occurrence percentile only; p99.9 prohibited and omitted). The p99.9 unique-event
rule can never be satisfied by repeated small fixtures; without an available 100k recorded
corpus p99.9 is OMITTED.

## Workload identity and metadata

- Workload schema: `M5_BENCHMARK_WORKLOAD_SPEC_V1` — every registered benchmark owns a
  canonical key-sorted workload-spec document and its SHA-256, registered at static
  initialization independent of the run filter (this is what lets inventory validation prove
  all required families are REGISTERED while smoke executes a subset).
- Metadata wrapper: `M5_BENCHMARK_WRAPPER_V1` with measurement contract
  `M5_PHASE6_MEASUREMENT_CONTRACT_V1`; latency schema `M5_REPLAY_LATENCY_V1`. The wrapper
  separates source/binary provenance (git SHA, configure-time dirty bit, exact executable
  SHA-256), build identity (compiler, C++ standard, build type, sanitizer state, LTO state,
  standard library name/version/detection status, Conan lockfile SHA-256, Conan
  references/package IDs, Google Benchmark version), environment identity (OS, architecture,
  CPU model, logical cores), M4 dependency identity (Contracts revision/ref/rrev/package ID,
  Protobuf identity, or explicit `not_applicable` for Core-only payloads), workload identities,
  measurement identity (timer, denominator, warmup kind/count, repetitions, sample/unique/pass
  counts, estimator, checksum methodology), and result-payload binding (payload SHA-256).
- Configure-time source provenance is captured as known-clean, known-dirty, or unavailable and
  embedded in the binary. Dirty source produces exploratory evidence; unavailable or malformed
  provenance is conservatively dirty for exploratory evidence and cannot produce a formal wrapper.
- Result consumption methodology: `M5_PHASE6_REPLAY_CHECKSUM_V1` (FNV-1a over typed per-event
  evidence plus final projection state).

## Machine-readable outputs

Google Benchmark JSON payload, `M5_BENCHMARK_WRAPPER_V1` JSON, `M5_REPLAY_LATENCY_V1` JSON.
A formal/manual full run additionally produces a human-readable summary derived from the
machine-readable results. Files live locally/in CI workspace only; Phase 6 uploads nothing.

## Validation

`scripts/benchmark_phase6.py` implements fail-closed validators (also covered by deterministic
Python tests in `tests/m5/benchmark/test_phase6_validators.py` and C++ tests in
`tests/m5/benchmark/benchmark_support_test.cpp`):

- benchmark inventory: the required family registration, the 48-cell M3 matrix, and required
  M4 names must all be present in the wrapper's workload registry;
- payload structure: non-empty benchmark array (zero-match filter failure), no
  `error_occurred`, no skip records (`SkipWithError`), finite/positive required timings;
- smoke expectations: the executed set must equal the locked smoke set exactly;
- wrapper: schema, required provenance/build/environment fields, unknown/dirty-formal rejection,
  concrete generated-workload identity presence/shape, canonical replay-log consistency, explicit
  warmup and formal repetition checks, payload existence + SHA binding, binary SHA rehash where
  the binary is available; deterministic C++ tests independently reconstruct representative
  synthetic identities from their generated state/update sequences;
- item-rate contract: total logical items and the declared CPU/wall denominator reproduce
  `items_per_second`; replay events/second and ns/event are reciprocal;
- latency: nearest-rank-v1 recomputation from stored raw samples, sample_count ==
  passes x event_count, unique_event_count == event_count, eligibility rules, p99.9
  unique-event rule, checksum validation, calibration non-subtraction, wrapper binding.

`scripts/benchmark-smoke.sh` runs the CI-intended smoke (Release, ProtoAdapter ON, 1
repetition, locked filter, validators) and `scripts/benchmark-full.sh` runs the formal/manual
full evidence suite (>= 5 repetitions, full matrix, latency small-tier evidence, summary).
OD-M5-P6-024 retains its normative recommendation of a 15-minute job timeout. The current
benchmark-smoke workflow intentionally uses 45 minutes because fail-closed M4 inventory requires
ProtoAdapter ON and a pinned Contracts bootstrap; on cold CI runners the dependency path alone can
consume roughly 10-15 minutes before benchmark execution. The 45-minute value is an implementation
deviation with cold-run margin, not a claim that accepted authority mandated 45 minutes.

## CI policy

The existing `benchmark-smoke` job (Ubuntu Clang) is extended: pinned Contracts bootstrap,
`BMD_PROJECTION_BUILD_PROTO_ADAPTER=ON`, Phase-6 smoke, current 45-minute timeout. Blocking failures:
target build failure, missing required inventory, missing required M4 names, zero-match filter,
missing executed benchmarks, `SkipWithError`/`error_occurred`, invalid Google Benchmark JSON,
invalid wrapper, payload hash mismatch, missing required metadata, semantic pre/postcondition
failure, invalid replay checksum, non-finite/non-positive required timings, timeout. NO numeric
performance threshold exists. Smoke values are structural execution evidence only.

## Formal baseline eligibility

A run may be labelled formal current-production baseline only when: source clean at
configure/build; Release; sanitizers off; the full applicable M2 set; all 48 M3 accepted cells;
M3 classification families; CoreNormalizedReplay Spot + USD-M; AdapterWireReplay Spot + USD-M;
stable workload IDs/hashes; >= 5 repetitions; complete wrapper provenance; semantic preflight;
explicit noise reporting. M1 and pure M4 adaptation/serialization are required Phase-6 evidence
but are not labelled isolated std::map measurements; M4 checked paths and production replay may
be labelled system-level production baseline.

## Performance claim language

Only "On environment X, exact source/binary Y, workload Z measured result R" is used. Phase 6
characterizes the current implementation; it does not optimize it and makes no cross-machine,
capacity, or improvement claims.
