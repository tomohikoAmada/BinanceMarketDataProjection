# M5 Phase-6 Pre-Implementation Decisions

## Status

- Status: **APPROVED / MERGED**
- Repository baseline: `a8cf3dd908a609690f0475bcfa397faa8c5b65a9`
- Decision review: **APPROVED**
- P0: `0`
- P1 Methodology: `0` unresolved
- P2: `0`
- Implementation authorization: **YES / CONSUMED FOR THIS IMPLEMENTATION**

After this exact decision record is independently reviewed and merged without material decision
changes:

- Phase-6 implementation authorization becomes **YES**.

The merge condition has been satisfied. This decision record is APPROVED / MERGED; the
Phase-6 implementation authorization was consumed by the unified Phase-6 implementation PR.
The normative decisions OD-M5-P6-001 through OD-M5-P6-030 below are unchanged.

This document is normative pre-implementation authority for the M5 Phase 6 representative
benchmark measurement contracts. It records accepted design authority, the independent decision
lock, implementation constraints, and the future Phase-7/8/10 boundaries. It is NOT implementation
evidence: no benchmark code exists or is claimed here.

**This document does NOT implement Phase 6.** Phase 6 remains NOT STARTED in this PR.

## Decision Review

An independent read-only GPT-5.6 Sol benchmark-methodology review of exact main
`a8cf3dd908a609690f0475bcfa397faa8c5b65a9` concluded:

- Verdict: **APPROVED**
- P0: `0`
- P1 Methodology: `0` unresolved
- P2: `0`
- Phase-6 Implementation Authorization: **CONDITIONAL**

The only condition is governance recording:

1. Add a docs-only Phase-6 decision record.
2. Record OD-M5-P6-001 through OD-M5-P6-030 without materially changing them.
3. Independently review and merge that docs-only PR.
4. Only then may Phase-6 implementation begin.

There are **NO remaining benchmark-methodology blockers**.

## Normative Decision Table

All decisions below are:

- Status: **CLOSED — RECORD BEFORE IMPLEMENTATION**

| ID | Question | Decision | Rationale | Implementation constraint | Status |
|---|---|---|---|---|---|
| OD-M5-P6-001 | Phase-6 scope | M1-M4 benchmark baselines, production replay throughput, event-latency methodology, metadata/provenance and validators only. | Matches the accepted Phase-6 implementation sequence. | No Phase-7 allocation instrumentation. No Phase-8 alternative container work. No Phase-10 reporting/workflow integration. | CLOSED — RECORD BEFORE IMPLEMENTATION |
| OD-M5-P6-002 | Can standalone rescale be benchmarked? | NO. There is no standalone production rescale API. Scale-conversion benchmarks must truthfully identify the actual parse or format public API path. | Prevents false standalone-rescale claims. | Do not create benchmark-only arithmetic and call it production rescale. | CLOSED — RECORD BEFORE IMPLEMENTATION |
| OD-M5-P6-003 | M1 normative workload cases | Required:<br>M1/ParsePrice/MatchedScale: `parse_price("1.23456789", scale 8)` → success, units = 123456789<br>M1/ParsePositiveQuantity/MatchedScale: `parse_positive_quantity("1.23456789", scale 8)` → success<br>M1/ParseQuantity/ZeroSuccess: `parse_quantity("0", scale 8)` → success, units = 0<br>M1/ParsePositiveQuantity/ZeroRejected: `parse_positive_quantity("0", scale 8)` → ZeroNotAllowed<br>M1/ParsePrice/ExactUpscale: `parse_price("1.2345", scale 8)` → success, units = 123450000<br>M1/ParsePrice/ExactDownscale: `parse_price("1.234567890000000000", scale 8)` → success, units = 123456789<br>M1/ParsePrice/InexactDownscaleRejected: `parse_price("1.234567890123456789", scale 8)` → InexactScale<br>M1/ParsePrice/OverflowRejected: `parse_price("92233720368.54775808", scale 8)` → Overflow<br>M1/ParsePrice/SyntaxRejected: `parse_price("1e3", scale 8)` → InvalidSyntax<br>M1/FormatPriceFixed: units = 123456789, scale = 8, result = "1.23456789"<br>M1/FormatQuantityFixed: units = 0, scale = 8, result = "0.00000000" | Prevents false standalone-rescale claims and distinguishes success/error paths. | The exact input/result table is normative. Positive quantity zero is an ERROR workload, not success. | CLOSED — RECORD BEFORE IMPLEMENTATION |
| OD-M5-P6-004 | M2 stateful iteration semantics | Every timed execution must have the same semantic precondition and expected disposition as every other timed execution. | Timed executions are only comparable when their semantic preconditions and dispositions are identical. | Insert: target absent before every timed op, expected Inserted. Replace: target exists with different quantity, expected Updated. Delete: target exists, quantity zero, expected Removed. Missing-delete: target absent, expected Unchanged. Batch: predetermined semantic mix every execution. replace_all: preconstructed vectors, result exactly matches canonical input. Queries: returned values/vectors consumed. State restoration and generation outside timing. Explicitly prohibit: insert → update/unchanged drift, delete → missing-delete drift, unbounded book growth, discarded query result, timed setup contamination. | CLOSED — RECORD BEFORE IMPLEMENTATION |
| OD-M5-P6-005 | M2 depth sets | Routine levels per side: {8, 100, 1000}. Full: {0, 8, 100, 1000, 5000, 10000}. Use D=0 only when semantically valid. Existing-level operations exclude D=0. apply_updates batches: {1, 10, 100}. Primary scaling workload: replacement-heavy, every update changes quantity. Insert-heavy/delete-heavy variants may exist only with explicit operation_mix metadata. | Depth and batch scaling must match accepted M2 semantics, with D=0 restricted to where it is semantically valid. | Existing-level operations must not use D=0. Variants beyond the primary replacement-heavy workload require explicit operation_mix metadata. | CLOSED — RECORD BEFORE IMPLEMENTATION |
| OD-M5-P6-006 | M3 accepted-apply scaling matrix | Depth per side: {0, 8, 100, 1000, 5000, 10000}. Batch: {0, 1, 10, 100}. Policy: Spot, UsdMPerpetual. Formal matrix: 6 × 4 × 2 = 48 cells. | Matches the accepted M3 live-apply scaling semantics. | Each measured cell begins Synchronized. Spot: valid successor-covering advancing live range. USD-M: advancing range with previous_final == current. Each intended accepted operation must actually return Applied. For D>0 prefer existing-price quantity changes. For D=0, B>0 is an explicitly labelled empty-book insertion edge. | CLOSED — RECORD BEFORE IMPLEMENTATION |
| OD-M5-P6-007 | Is batch=0 valid and required? | MANDATORY. | DepthBatch::levels permits an empty span. A valid advancing zero-level batch still performs the accepted transaction: all_levels bids, all_levels asks, candidate construction, replace_all, apply_updates(empty), move commit / old-book destruction, sequence advance. | batch=0 is a formal matrix cell and must not be replaced by batch=1. | CLOSED — RECORD BEFORE IMPLEMENTATION |
| OD-M5-P6-008 | Required M3 classification coverage | Required: accepted, stale, duplicate, gap, reset, baseline install. Both Spot and USD-M must be represented where sequence semantics differ. State-changing one-shot cases use independently prepared/reset states. | Covers every M3 sequence disposition without contaminating one benchmark with another's state change. | No second call against a changed state may masquerade as the same benchmark. | CLOSED — RECORD BEFORE IMPLEMENTATION |
| OD-M5-P6-009 | M3 decomposition authority | Public proxy/component measurements are allowed for: both-side all_levels copies, candidate rebuild via OrderBook + replace_all, candidate apply_updates, OrderBook move assignment / destination destruction. But they are approximate component/proxy measurements. | Public API can approximate private transaction stages without production changes. | Names must include Component, Proxy, or equivalent. Forbidden claim: sum(component results) == exact BookProjection::apply decomposition. No production API changes solely to expose private transaction stages. | CLOSED — RECORD BEFORE IMPLEMENTATION |
| OD-M5-P6-010 | M4 timing boundaries | Separate: adapt_exchange_depth_snapshot, adapt_depth_update, AdaptedBookBaseline::install_into, AdaptedDepthBatch::apply_to, make_local_order_book_snapshot unlimited, make_local_order_book_snapshot limited, snapshot protobuf serialization. | Each M4 boundary has a distinct production cost that must not be conflated. | Adaptation timing excludes: wire construction, Core apply, snapshot output, serialization. Checked apply/install excludes: wire construction, adaptation. Snapshot construction excludes: serialization. | CLOSED — RECORD BEFORE IMPLEMENTATION |
| OD-M5-P6-011 | Protobuf message construction | SETUP ONLY. Not a required formal Phase-6 benchmark. | Message construction is adaptation setup, not a measured production path. | Preconstruct inbound wire messages outside adaptation timing. A separately labelled diagnostic is permitted but not part of formal M4 inventory. | CLOSED — RECORD BEFORE IMPLEMENTATION |
| OD-M5-P6-012 | Serialization buffer policy | Formal primary: fresh buffer. Optional diagnostic: reused buffer. | A fresh buffer is the honest serialization baseline. | Primary baseline must not silently inherit retained capacity. | CLOSED — RECORD BEFORE IMPLEMENTATION |
| OD-M5-P6-013 | M4 benchmark availability | FAIL CLOSED. benchmark-smoke must enable BMD_PROJECTION_BUILD_PROTO_ADAPTER=ON and bootstrap pinned Contracts. Required M4 benchmark inventory must be validated. | M4 benchmarks are formal Phase-6 inventory; their absence must fail smoke. | Missing expected M4 benchmark names fails smoke. A Core-only benchmark executable is insufficient after Phase 6. | CLOSED — RECORD BEFORE IMPLEMENTATION |
| OD-M5-P6-014 | Core production replay architecture | CoreNormalizedReplay. Timed path: preloaded normalized replay operations → production M1 parsing where applicable → production M3 BookProjection → minimal result/checksum consumption. | Measures the production Core projection path with validation outside the timer. | No canonical replay text parsing in timed interval. Do NOT use CoreProductionSide::observe() as throughput executor because its OperationObservation/checkpoint construction is validation overhead. | CLOSED — RECORD BEFORE IMPLEMENTATION |
| OD-M5-P6-015 | Adapter production replay architecture | AdapterWireReplay. Timed path: preconstructed valid protobuf wire messages → M4 adaptation → checked M3 operation → minimal result/checksum consumption. | Measures the production adapter + projection path with all validation overhead excluded. | Excluded from timer: wire construction, fixture parsing, file I/O, hashing, generation, reference model, ReplayDriver, OperationObservation, checkpoint comparison, diagnostic rendering. | CLOSED — RECORD BEFORE IMPLEMENTATION |
| OD-M5-P6-016 | Differential ReplayDriver throughput | OPTIONAL. If implemented it must be explicitly labelled validation infrastructure, for example: Validation/DifferentialReplayDriver. | Differential validation is not a production throughput path. | It is NOT part of formal production throughput baseline. | CLOSED — RECORD BEFORE IMPLEMENTATION |
| OD-M5-P6-017 | Event-level latency method | Dedicated production-only steady_clock sampler. Required sequence: preload/pre-touch workload; construct immutable input outside timing; one complete untimed warmup replay; discard warmup state; fresh production state; preallocate latency vector; steady_clock immediately before/after each production event; store elapsed sample; no oracle/logging/formatting/sample-vector allocation/parsing/hashing inside event; post-run final-state/checksum validation; quantile calculation after run. Required calibration: separate empty bracket population — steady_clock, steady_clock, sample store. Report calibration separately. | An event-level sampler must keep every non-production operation outside the per-event bracket. | NO overhead subtraction. No per-event CPU-clock measurement required. | CLOSED — RECORD BEFORE IMPLEMENTATION |
| OD-M5-P6-018 | Percentile estimator and sample rules | Estimator: nearest-rank-v1. For sorted samples x[0..n-1]: Q(p) = x[ceil(p*n)-1]. No interpolation. Reporting minimums: p50: sample_count >= 1000; p90: sample_count >= 1000; p99: sample_count >= 10000; p99.9: sample_count >= 100000 AND unique_event_count >= 100000. Always record: sample_count, unique_event_count, passes. Repeating a 2048-event workload five times may support a timing-occurrence p99 with sample_count=10240, unique_event_count=2048, passes=5, but must not be described as 10240 unique workload events. Repeated small fixtures can NEVER satisfy the p99.9 unique-event rule. If eligible medium corpus unavailable: omit p99.9. | Nearest-rank-v1 is the declared estimator; percentile validity is tied to sample and unique-event counts. | No interpolation. p99.9 is omitted, never fabricated, when the eligible corpus is unavailable. | CLOSED — RECORD BEFORE IMPLEMENTATION |
| OD-M5-P6-019 | Timer denominators | Microbench: Google Benchmark CPU time primary. Real/wall secondary context. Replay throughput: wall / UseRealTime primary; events/sec and ns/event derive from wall denominator; CPU reported separately. Per-event latency: steady_clock wall only. | Each measurement class has one primary denominator that must never be mixed with another. | Never ratio a CPU-time baseline against a wall-time candidate. | CLOSED — RECORD BEFORE IMPLEMENTATION |
| OD-M5-P6-020 | Warmup and repetitions | Reported/full: >= 5 repetitions. Smoke: 1 repetition. Warmup: one complete untimed workload-equivalent pass before reported measurement. Warmup state must not leak into measured state. | Sufficient repetitions for run-level stability without contaminating measured state. | Google Benchmark repetition mean/median/stddev/CV are run-level stability metrics only, never event-level percentiles. | CLOSED — RECORD BEFORE IMPLEMENTATION |
| OD-M5-P6-021 | Metadata schema | M5_BENCHMARK_WRAPPER_V1. Measurement contract: M5_PHASE6_MEASUREMENT_CONTRACT_V1. Synthetic workload schema: M5_BENCHMARK_WORKLOAD_SPEC_V1. Latency schema: M5_REPLAY_LATENCY_V1. Wrapper must separate: source/binary provenance, workload identity, environment identity, measurement methodology, result payload binding. Required source/binary: source.git_sha, source.dirty_at_configure, binary.sha256, measurement_contract_version, result_payload_sha256. Required build: compiler ID/version, C++ standard, build type, sanitizer state, LTO state, stdlib name/version/detection status, Conan lockfile SHA-256, relevant Conan refs/package IDs, Google Benchmark version. Required environment: OS name/version, architecture, CPU model, logical core count. M4: Contracts revision, Contracts Conan reference/recipe revision/package ID, Protobuf identity, or explicit not_applicable for Core-only result. Required workload as applicable: workload ID, workload-spec SHA, fixture ID/hash, generator version, seed, generated-workload hash, complete parameters, market, symbol, NumericSpec, sequence policy, depth, batch, operation_mix, locality/profile, query limit/hit ratio, serialization buffer mode. Required measurement as applicable: timer, primary denominator, warmup kind/count, iterations, repetitions, sample_count, unique_event_count, passes, quantile estimator, checksum/result-consumption methodology version. | Full provenance separation is required for any result to be interpreted or replayed. | Do not pre-add Phase-7 allocation metrics as mandatory fields. | CLOSED — RECORD BEFORE IMPLEMENTATION |
| OD-M5-P6-022 | Binary/source provenance | Exact benchmark executable SHA-256 is REQUIRED. Formal baseline requires: source.dirty_at_configure = false. Dirty runs are allowed only as evidence_class = exploratory and cannot become formal baseline evidence. The dirty flag must represent configure/build source state, not merely result-wrapper generation time. | A formal baseline must be reproducible from a clean, exact source state. | Dirty runs are labelled exploratory only. | CLOSED — RECORD BEFORE IMPLEMENTATION |
| OD-M5-P6-023 | Synthetic workload identity | Required versioned identity: generator schema/version + seed + complete parameters + canonical workload-spec SHA-256 + canonical generated-workload SHA-256. | Seed alone is insufficient to identify a workload. | Microbenchmarks without replay files require a canonical workload-spec representation and hash. | CLOSED — RECORD BEFORE IMPLEMENTATION |
| OD-M5-P6-024 | Phase-6 PR CI blocking policy | NO numeric performance threshold. Existing benchmark-smoke must fail closed on: targets fail to build; required benchmark inventory absent; required M4 names absent; smoke filter matches zero expected cases; required benchmark fails to execute; SkipWithError; error_occurred; invalid Google Benchmark JSON; invalid wrapper; payload SHA mismatch; missing required metadata; semantic pre/postcondition failure; non-finite/non-positive required timing; replay/latency checksum failure; timeout. | PR CI gates structural, semantic, and formatting validity, not numeric performance. | Smoke values are NOT formal performance evidence. Recommended job timeout: 15 minutes. | CLOSED — RECORD BEFORE IMPLEMENTATION |
| OD-M5-P6-025 | Cross-compiler benchmark integration | Do NOT add new benchmark compilation to every release-matrix job in Phase 6. Existing Core/ProtoAdapter GCC/Clang/AppleClang gates plus Ubuntu Clang benchmark-smoke are sufficient for Phase 6. | Cross-toolchain timing is not comparable; Phase-6 CI breadth already covers semantic gates. | Never directly ratio performance across different toolchain/runner environments. | CLOSED — RECORD BEFORE IMPLEMENTATION |
| OD-M5-P6-026 | Phase-10 integration boundary | The following are NOT Phase-6 work: benchmark artifact upload, scheduled/manual m5-performance workflow, long-term retention policy, performance historical-trend workflow, release-matrix benchmark expansion for performance integration. | These belong to the later Phase-10 reporting/workflow integration scope. | Phase 6 may generate and validate machine-readable files inside local/manual result directories or CI workspace. Do not upload them as Phase-6 performance artifacts. | CLOSED — RECORD BEFORE IMPLEMENTATION |
| OD-M5-P6-027 | Phase-8 comparison denominator | Phase 6 freezes: workload semantics, generator schema, operation mix, depth/batch parameters, timer/denominator, production std::map system-baseline measurement. If Phase-8 candidate models use a different execution interface/path, candidate speedup ratios must use Phase8Candidate / Phase8StdMapControl under the SAME Phase-8 candidate-model interface and environment. | A Phase-8 candidate must be compared against a same-interface control, not the Phase-6 production path. | Do NOT directly ratio Phase8Candidate / Phase6ProductionBookProjection unless execution paths are demonstrably identical. Do not create the Phase-8 candidate abstraction in Phase 6. | CLOSED — RECORD BEFORE IMPLEMENTATION |
| OD-M5-P6-028 | Formal current-production baseline eligibility | Formal current-production std::map baseline requires: clean source at configure/build; Release; no sanitizer timing; complete applicable M2 container-sensitive full depths; all 48 M3 accepted-live-apply cells; M3 accepted/stale/duplicate/gap/reset/baseline classifications; CoreNormalizedReplay Spot + USD-M; AdapterWireReplay Spot + USD-M; stable workload IDs/hashes; >= 5 repetitions; complete wrapper provenance; semantic preflight; explicit noise reporting. | A formal baseline needs complete inventory, provenance, and stability evidence. | M1 and pure M4 adaptation/serialization are required Phase-6 performance evidence but are not themselves labelled isolated std::map measurements. M4 checked paths and production replay may be labelled system-level production baseline. No Phase-8 improvement threshold is set here. | CLOSED — RECORD BEFORE IMPLEMENTATION |
| OD-M5-P6-029 | Required Phase-6 output files | Required: Google Benchmark JSON, metadata-wrapper JSON, M5_REPLAY_LATENCY_V1 JSON. Human-readable summary: required for formal/manual full evidence run. | Machine-readable artifacts are the authoritative output. | Machine-readable data is authoritative over narrative summary. benchmark-smoke must generate and validate the files in workspace. Phase 6 does NOT upload them. | CLOSED — RECORD BEFORE IMPLEMENTATION |
| OD-M5-P6-030 | PR sequencing | 1. This docs-only decision PR first. 2. Independent lightweight decision-record review. 3. Merge it. 4. Then one unified Phase-6 implementation PR. | Governance recording precedes any benchmark implementation. | No benchmark implementation before this record is merged. | CLOSED — RECORD BEFORE IMPLEMENTATION |

## Small CI Smoke Matrix

The locked M3 PR-CI execution subset:

- Depth: {8, 1000}
- Batch: {0, 10}
- Policy: {Spot, UsdMPerpetual}
- Total: **8 accepted-live-apply cells**

The full 48-cell inventory must be REGISTERED and inventory-validated, but only the 8-cell subset
is executed in blocking PR smoke.

## General Measured-Region Rules

Normative cross-cutting rules for every Phase-6 measurement:

- Release/optimized timing
- no sanitizer timing
- fixed deterministic inputs
- no random_device
- no clock-derived seed
- no network
- no filesystem access in timed loops
- no fixture parsing/generation/hashing in timed loops
- input/setup/state restoration outside intended measured region
- result consumption mandatory
- semantic postconditions mandatory
- no per-ns-operation PauseTiming/ResumeTiming pattern

State restoration should use prepared bounded pools/batched restoration where necessary.

## Remaining Spike-Resolvable Questions

The existing decisions OD-M5-003, OD-M5-004, OD-M5-005, OD-M5-006, OD-M5-007 remain
SPIKE-RESOLVABLE where already documented. They do NOT block this Phase-6 measurement contract.
They are not renumbered or reinterpreted here. In particular:

- OD-M5-005: performance threshold tuning remains unresolved, but Phase-6 PR CI has NO numeric
  performance gate.
- OD-M5-007: artifact retention remains later reporting scope.

## Authorization Wording

- Before this docs PR merges: Phase-6 implementation authorization is **CONDITIONAL**.
- After this exact decision PR is independently reviewed and merged without material decision
  changes: Phase-6 implementation authorization is **YES**.

This transition is now documented truthfully: the decision record is merged and the
authorization is **YES / CONSUMED FOR THIS IMPLEMENTATION** (see
`docs/M5_PHASE6_REPRESENTATIVE_BENCHMARKS.md`).

Decision-review baseline: `a8cf3dd908a609690f0475bcfa397faa8c5b65a9`. Once merged, this docs commit
will advance main; the decision-review baseline above remains fixed to the reviewed tree.

## Non-Goals

- This document does NOT implement Phase 6.
- No benchmark code, targets, or measurement files are added by this PR.
- Phase 7 (allocation instrumentation): NOT STARTED.
- Phase 8 (alternative container work): NOT STARTED.
- Phase 10 (reporting/workflow integration): NOT STARTED.
