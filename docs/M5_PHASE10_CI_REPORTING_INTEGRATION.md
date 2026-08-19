# M5 Phase-10 CI Reporting Integration

## Current implementation state

```text
PHASE10=IN PROGRESS
WP_A=BENCHMARK-SMOKE ARTIFACT REPORTING IMPLEMENTED
WP_B=NOT IMPLEMENTED
P10_PRE_P1_001=CLOSED
PR_BLOCKING_JOB_COUNT=19
BENCHMARK_SMOKE_ARTIFACT=m5-benchmark-smoke-ubuntu-clang-<short-actual-checkout-sha>
BENCHMARK_SMOKE_RETENTION=3 days
BENCHMARK_SMOKE_EVIDENCE_CLASS=EXPLORATORY / STRUCTURAL-SMOKE
NUMERIC_PERFORMANCE_GATE=NO
WEEKLY_M5_PERFORMANCE=NOT IMPLEMENTED / NOT RUNNING
M5_PERFORMANCE_WORKFLOW_CREATED=NO
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

## WP-B boundary

WP-B remains responsible for:

- immutable medium asset acquisition;
- recorded `Replay_V1` benchmark-only consumer;
- `.github/workflows/m5-performance.yml`;
- weekly main-only medium reporting;
- final Phase-10 governance;
- `WEEKLY_M5_PERFORMANCE=ACTIVE` only after actual workflow implementation.

WP-B is not implemented here. No medium corpus is downloaded, no weekly trigger is
created, no performance workflow is created, and no Phase-11 work is started.

The future weekly operation remains:

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
```
