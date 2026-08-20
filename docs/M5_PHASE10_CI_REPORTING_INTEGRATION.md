# M5 Phase-10 CI Reporting Integration

## Current implementation state

```text
PHASE10=IN PROGRESS
WP_A=BENCHMARK-SMOKE ARTIFACT REPORTING IMPLEMENTED
WP_B=IMPLEMENTED AT THIS TREE / DEFAULT-BRANCH ACTIVATION PROOF PENDING
P10_PRE_P1_001=CLOSED
PR_BLOCKING_JOB_COUNT=19
BENCHMARK_SMOKE_ARTIFACT=m5-benchmark-smoke-ubuntu-clang-<short-actual-checkout-sha>
BENCHMARK_SMOKE_RETENTION=3 days
BENCHMARK_SMOKE_EVIDENCE_CLASS=EXPLORATORY / STRUCTURAL-SMOKE
NUMERIC_PERFORMANCE_GATE=NO
WEEKLY_M5_PERFORMANCE=IMPLEMENTED AT THIS TREE / EXACT-MAIN ACTIVATION PROOF PENDING
M5_PERFORMANCE_WORKFLOW_CREATED=YES AT THIS TREE
WORKFLOW=.github/workflows/m5-performance.yml
SCHEDULE_CRON=17 3 * * 1
SCHEDULE_TIMEZONE=UTC
TIER=recorded medium v1
FIXTURES=M5-REC-SPOT-BTCUSDT-V1 + M5-REC-USDM-BTCUSDT-V1
RUNNER=ubuntu-24.04 / GitHub-hosted x86_64
COMPILER=Clang
TIMEOUT=45 minutes
PR_BLOCKING=NO
EVIDENCE_CLASS=EXPLORATORY / NONBLOCKING REPORTING
RETENTION=7 days
PRODUCTION_CONTAINER=std::map
PRODUCTION_MIGRATION=NO
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

The workflow binds the exact two recorded fixtures, reuses `replay::load_fixture`,
`CoreReplayExecutor`, and the existing Core differential/lifecycle validator,
and keeps acquisition, archive verification, fixture parsing, differential
validation, and wrapper generation outside the timed Core path. It changes no
production API, no production container, and no ordinary PR/push CI job.

The implemented operation is:

```text
.github/workflows/m5-performance.yml
weekly
recorded medium v1
Spot + USD-M
Core current-production std::map
standard GitHub-hosted Ubuntu x86_64
45 minutes
PR_BLOCKING=NO
EXPLORATORY / NONBLOCKING REPORTING
7-day retention
STATUS=IMPLEMENTED AT THIS TREE / EXACT-MAIN ACTIVATION PROOF PENDING
```

This is not final Phase-10 closure. After merge, ordinary main CI must pass and
a separately dispatched run on the exact new main SHA must produce and
independently pass the exact five-file artifact contract. Only then may a
focused docs-only closure record `PHASE10=COMPLETE`, both WP states COMPLETE,
`WEEKLY_M5_PERFORMANCE=ACTIVE`, the actual activation identifiers, and
`READY_FOR_PHASE11=YES`. No future run ID, SHA, artifact name, active state, or
Phase-11 authorization is invented here.
