# M5 Phase-10 CI Reporting Integration

## Current implementation state

```text
PHASE10=IN PROGRESS
WP_A=IMPLEMENTED / MERGED / MAIN CI GREEN
WP_B=IMPLEMENTED / MERGED / MINIMAL RUNTIME-BUDGET / DEDICATED-VALIDATOR DECOUPLING CORRECTION UNDER REVIEW
P10_PRE_P1_001=CLOSED
P10_WPB_P1_001=CLOSED
P10_WPB_ACT_P1_001=OPEN
P10_WPB_ACT_P2_001=CLOSED
PR_BLOCKING_JOB_COUNT=19
BENCHMARK_SMOKE_ARTIFACT=m5-benchmark-smoke-ubuntu-clang-<short-actual-checkout-sha>
BENCHMARK_SMOKE_RETENTION=3 days
BENCHMARK_SMOKE_EVIDENCE_CLASS=EXPLORATORY / STRUCTURAL-SMOKE
NUMERIC_PERFORMANCE_GATE=NO
WEEKLY_M5_PERFORMANCE=IMPLEMENTED / EXACT-MAIN ACTIVATION NOT PROVEN
WEEKLY_M5_PERFORMANCE_PURPOSE=EXPLORATORY PERFORMANCE CANARY
WEEKLY_REPETITIONS_PER_FIXTURE=3
WEEKLY_SPOT_REPETITIONS=3
WEEKLY_USDM_REPETITIONS=3
WEEKLY_NUMERIC_GATE=NO
WEEKLY_CONTAINER_DECISION_AUTHORITY=NO
FORMAL_DECISION_EVIDENCE_MIN_REPETITIONS=>=5
M5_PERFORMANCE_WORKFLOW_CREATED=YES AT THIS TREE
WORKFLOW=.github/workflows/m5-performance.yml
SCHEDULE_CRON=17 3 * * 1
SCHEDULE_TIMEZONE=UTC
TIER=recorded medium v1
FIXTURES=M5-REC-SPOT-BTCUSDT-V1 + M5-REC-USDM-BTCUSDT-V1
RUNNER=ubuntu-24.04 / GitHub-hosted x86_64
COMPILER=Clang
TIMEOUT=45 minutes
ACTIVATION_PROOF=PENDING
PR_BLOCKING=NO
EVIDENCE_CLASS=EXPLORATORY / NONBLOCKING REPORTING
RETENTION=7 days
PRODUCTION_CONTAINER=std::map
PRODUCTION_MIGRATION=NO
PHASE10_COMPLETE=NO
READY_FOR_PHASE11=NO
PHASE11_STARTED=NO
```

WP-A extends the existing `benchmark-smoke` job only. It checks out the exact
`EVIDENCE_SHA`, verifies the actual checkout identity, runs the unchanged full
Phase-6 smoke path, validates both wrapper source identities and clean configure
provenance, and uploads exactly these four files:

```text
build/benchmark/phase6-smoke-results/benchmarks.json
build/benchmark/phase6-smoke-results/benchmarks-wrapper.json
build/benchmark/phase6-smoke-results/latency.json
build/benchmark/phase6-smoke-results/latency-wrapper.json
```

The artifact uses three-day retention, fails if no output files are found, and is
attempted with `always()` so useful partial output can be retained after a failed
smoke or validation step. The validation remains fail-closed, and upload failure
is fatal. The benchmark remains blocking structural/execution smoke evidence only;
there is no throughput, latency, baseline, or other numeric performance gate.

## WP-B technical implementation

WP-B implements:

- immutable medium asset acquisition;
- recorded `Replay_V1` benchmark-only consumer;
- `.github/workflows/m5-performance.yml`;
- weekly main-only medium reporting;
- offline synthetic verifier tests registered in CTest.

The workflow binds the exact two recorded fixtures and keeps acquisition, archive
verification, and safe extraction outside the benchmark executable. The benchmark
owns fixture loading/identity checks, prepared-input validation, preflight, explicit
warmup, timed production-Core replay, and checksum verification. The weekly
exploratory canary no longer repeats the dedicated Core/reference differential and
lifecycle validator; its implementation remains existing correctness infrastructure
and its other authorities are unchanged. It changes no production API, no production
container, and no ordinary PR/push CI job.

The implemented operation, with the narrow canary-contract correction under
review, is:

```text
.github/workflows/m5-performance.yml
weekly
recorded medium v1
Spot + USD-M
Core current-production std::map
standard GitHub-hosted Ubuntu x86_64
45 minutes
EXPLORATORY PERFORMANCE CANARY
3 repetitions per accepted medium fixture
FORMAL_DECISION_EVIDENCE_MIN_REPETITIONS=>=5
PR_BLOCKING=NO
EXPLORATORY / NONBLOCKING REPORTING
7-day retention
STATUS=IMPLEMENTED / EXACT-MAIN ACTIVATION NOT PROVEN
```

This is not final Phase-10 closure. After merge, ordinary main CI must pass and
a separately dispatched run on the exact new main SHA must produce and
independently pass the exact five-file artifact contract. Only then may a
focused docs-only closure record `PHASE10=COMPLETE`, both WP states COMPLETE,
`WEEKLY_M5_PERFORMANCE=ACTIVE`, the actual activation identifiers, and
`READY_FOR_PHASE11=YES`. No future run ID, SHA, artifact name, active state, or
Phase-11 authorization is invented here.
