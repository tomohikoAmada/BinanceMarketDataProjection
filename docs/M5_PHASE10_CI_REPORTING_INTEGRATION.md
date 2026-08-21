# M5 Phase-10 CI Reporting Integration

## Current implementation state

```text
M5=IN PROGRESS
PHASE10=COMPLETE
WP_A=COMPLETE
WP_B=COMPLETE
P10_PRE_P1_001=CLOSED
P10_WPB_P1_001=CLOSED
P10_WPB_ACT_P1_001=CLOSED
P10_WPB_ACT_P2_001=CLOSED
PR_BLOCKING_JOB_COUNT=19
BENCHMARK_SMOKE_ARTIFACT=m5-benchmark-smoke-ubuntu-clang-<short-actual-checkout-sha>
BENCHMARK_SMOKE_RETENTION=3 days
BENCHMARK_SMOKE_EVIDENCE_CLASS=EXPLORATORY / STRUCTURAL-SMOKE
NUMERIC_PERFORMANCE_GATE=NO
WEEKLY_M5_PERFORMANCE=ACTIVE
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
ACTIVATION_PROOF=ACCEPTED
PR_BLOCKING=NO
EVIDENCE_CLASS=EXPLORATORY / NONBLOCKING REPORTING
RETENTION=7 days
CONTAINER_DECISION=KEEP_STD_MAP
PRODUCTION_CONTAINER=std::map
PRODUCTION_MIGRATION=NO
PHASE10_COMPLETE=YES
READY_FOR_PHASE11=YES
PHASE11_STARTED=NO
NEXT_M5_PHASE=PHASE11 / INDEPENDENT IMPLEMENTATION REVIEW
```

## Exact-main activation and independent acceptance

The normal exact-main post-merge CI run `32431673420` (`ci`, `push`) completed
successfully with 19/19 jobs passing. The dedicated activation run
`32453032145` ([run details](https://github.com/tomohikoAmada/BinanceMarketDataProjection/actions/runs/32453032145))
completed successfully via `workflow_dispatch` on exact main SHA
`e6d2cf099ae4d9c5dc7c77789c3b4b55336af672`, whose tree is
`98b085b577a3b3731587e89983acd785a758d122`. It was created at
`2026-08-21T06:05:48Z` and updated at `2026-08-21T06:28:28Z`; performance job
`96684882412` succeeded.

The unexpired activation artifact was ID `9436787118`, named
`m5-performance-ubuntu-clang-e6d2cf09`, with digest
`sha256:f2404a068310c3ba39737e7cc6d8fa0107b83ed71e4539840f000210d7cb1d2f`.
It was created at `2026-08-21T06:28:25Z` and expires at
`2026-08-28T06:28:25Z`. Its exact five-file inventory is:

```text
spot-benchmark.json
spot-wrapper.json
usdm-benchmark.json
usdm-wrapper.json
performance-summary.txt
```

The accepted file SHA-256 values are:

```text
SPOT_BENCHMARK_SHA256=83a99285bd5399e5cb8403d3b1584d9af8962d3014e8b00e9cff62e5f367dc44
SPOT_WRAPPER_SHA256=8c892cf21a2e926e2e5cd263fb02f589236027ba22b33a157839078f854a0c87
USDM_BENCHMARK_SHA256=3b3794655ca677c0b8b81181c3d41e8d23ae4b05d415f1b345527484cd08626b
USDM_WRAPPER_SHA256=70ef9ba252410be9b5809e8475922805b407cb9f6c4f4cb20585a309ef15dc75
PERFORMANCE_SUMMARY_SHA256=013cc69a0e9d8eed0951385f5227a945b87cfe907a0dbfef123f11570fa5bc9f
```

Independent acceptance is `ACCEPT_WITH_NONBLOCKING_NOTES` with P0=0, P1=0,
and P2=0. GitHub Actions emitted a Node.js 20 deprecation annotation with no
execution impact. The weekly canary deliberately does not duplicate the
dedicated Core/reference differential validator; this limits its correctness
authority but is consistent with the frozen exploratory performance-canary
contract.

The activation establishes the frozen exploratory/nonblocking performance-canary
contract. It does not elevate the canary into a numeric performance gate,
container-decision authority, or independent Core/reference correctness proof.
`NUMERIC_PERFORMANCE_GATE=NO`, `CONTAINER_DECISION=KEEP_STD_MAP`,
`PRODUCTION_CONTAINER=std::map`, and `PRODUCTION_MIGRATION=NO` remain in force.
`P10_WPB_ACT_P1_001=CLOSED`; this evidence authorizes the focused documentation
closure.

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

The implemented and accepted operation is:

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
STATUS=ACTIVE
```

Phase 10 is complete and the exact five-file activation artifact is independently
accepted. The weekly operation is active under the exploratory/nonblocking
contract above. Phase 11 is ready but has not started; no Phase-11 implementation
is included in this closure.
