# M5 Pre-Implementation Decisions

## Status

- Decision date: 2026-08-08
- Projection base: `8cae70908ab420b0469e7bd35b853cd475561d90`
- Post-merge CI: `31248929954` — PASS (same-tree equivalence verified)
- Implementation status: **IN PROGRESS**

## OD-M5-001: Representative Transcript Corpus Acquisition Plan

### Decision

**CLOSED.** M5 recorded corpus v1 is sourced from the Recorder M21.4 validated live Raw capture.

### Recorder source identity

| Field | Value |
|---|---|
| Repository | `tomohikoAmada/BinanceMarketDataRecorder` |
| Production commit | `cf1e749c7a533e916dbfb685212e5549a38c70dd` |
| Deployed Wheel SHA-256 | `926615b09ef46130f49a87fe8ab20acb7cfa6313daa67af5b718931bd95ff329` |
| Production config SHA-256 | `a399e647faaac58b5db24e835f1c29e799c70ad0c94ec77b597cac2647cfb734` |

### Formal 24-hour source window

| Field | Value |
|---|---|
| Run identity | `preflight/m21-4-24h-20260805T150930Z/` |
| T0 | `2026-08-05T15:09:30.200566Z` |
| Target | `2026-08-06T15:09:30.200566Z` |
| Duration | 86400 seconds |
| Validation | PASS |
| Markets | BTCUSDT Spot, BTCUSDT USD-M perpetual |

### Exact source streams

For Spot BTCUSDT: REST depth snapshot + diff depth 100ms.
For USD-M BTCUSDT perpetual: REST depth snapshot + diff depth 100ms including `u`/`U`/`pu`.

Retained where available: exchange timestamp, receive timestamp, raw ordering,
generation/connection identity needed for provenance, gap/resync markers, raw manifest identity,
chunk hash/integrity information.

Excluded: aggTrade, funding, open interest, mark price, liquidation, 5-minute statistics
(unless needed solely as provenance/context).

### Corpus selection rules

**Small tier.** Small blocking CI fixtures remain checked into the Projection repository.
Derived deterministically from the pinned Recorder source or generated synthetically.
1,000–10,000 events total per fixture. No network dependency for blocking PR CI.

Must cover at least: Spot baseline + synchronized updates, USD-M baseline + synchronized updates,
Spot bridge/rebaseline path, USD-M pu-aware path.

**Medium recorded tier.** Two mandatory recorded baseline fixtures:

| Fixture | Market | Derivation |
|---|---|---|
| M5-REC-SPOT-BTCUSDT-V1 | BTCUSDT Spot | First valid REST baseline at/after T0, then ordered diff-depth stream |
| M5-REC-USDM-BTCUSDT-V1 | BTCUSDT USD-M Perpetual | First valid REST baseline at/after T0, then ordered diff-depth stream |

Target: first 100,000 valid replay operations after synchronization per fixture.
Materializer must fail closed if baseline cannot be proven, sequence continuity cannot be
established, source chunks fail integrity verification, market/symbol identity differs,
or required source metadata is absent.

**Rotation diagnostic fixture.** The formal 24h window contains a planned rotation around
`2026-08-06T06:43:11Z` through approximately `2026-08-06T06:43:23Z`. A recorded diagnostic
slice spanning 10 minutes before through 10 minutes after is designed for connection/generation
transition, rebaseline/bridge behavior, and deterministic replay across lifecycle transitions.
If raw depth data does not provide the required semantic sequence, the fixture eligibility is
recorded as NOT ELIGIBLE.

**Large tier.** Full formal 24-hour capture for both markets: M5 large/manual corpus v1.
Manual/performance characterization only. Not a PR-blocking dependency. Not committed into Git.
Not downloaded automatically by normal CI.

### Corpus provenance manifest

Every derived recorded fixture must record: corpus schema version, Projection replay schema
version, Recorder repository and production commit, deployed Wheel SHA-256, production config
SHA-256, formal source run identity, source UTC start/end, market, symbol, source stream types,
source raw chunk IDs, source raw chunk SHA-256 or immutable manifest identities, event count,
conversion/materializer version, canonical replay-log SHA-256, NumericSpec, sequence policy,
conversion timestamp (metadata only).

No semantic identity may depend on mutable file paths.

### Source-of-truth hierarchy

```text
Recorder immutable live Raw
  -> verified source manifests/catalog
    -> M5 corpus materializer
      -> canonical replay-log v1
        -> fixture provenance manifest
          -> SHA-256 fixture identity
```

Projection must never depend directly on Recorder Python internals at runtime.
Recorder is a corpus SOURCE, not a Projection dependency.

### Binance Vision policy

Binance public historical archives may be used for ancillary validation, trade/bar context,
or independent cross-checks. They are NOT the primary OD-M5-001 L2 corpus source.
Historical data does not contain equivalent live diff-depth + receive-clock semantics.

### Corpus storage policy

- Small fixtures: checked into Projection Git repository.
- Medium recorded fixtures: NOT committed into normal Git history. Preferred future
  distribution as immutable compressed corpus asset with versioned asset name, SHA-256,
  manifest, and source identities.
- Large corpus: controlled/local archive only by default. No automatic normal-PR download.

### Legal/distribution boundary

Captured Binance data is not silently treated as redistributable. O-P001 / repository
distribution policy is preserved. OD-M5-001 closure means the exact source, selection
rules, provenance, and acquisition/materialization plan are decided. It does NOT mean
large data is uploaded publicly.

## OD-M5-002: Scheduled/Free CI Execution Budget

### Decision

**CLOSED.** Free standard GitHub-hosted runners only.

### CI policy tiers

**Documentation-only change.** Changes to `docs/**`, `README*.md`, `CHANGELOG*.md`,
`ARCHITECTURE.md`, and other Markdown-only status/ADR changes skip full CI.
Implemented via `paths-ignore` in the workflow trigger.

**Mixed or technical change.** Full appropriate CI is required when any technical path
changes: `include/**`, `src/**`, `tests/**`, `benchmarks/**`, `fuzz/**`, `scripts/**`,
`cmake/**`, `CMakeLists.txt`, `CMakePresets.json`, `conanfile.py`, `conan.lock`,
`.github/workflows/**`.

### Superseded-run cancellation

Workflow-level concurrency with `cancel-in-progress: true`. Grouped by workflow name and:
- PR number when PR event, or
- branch/ref otherwise.

Independent branches/PRs do not cancel one another.

### Free runner policy

- Allowed: standard GitHub-hosted runners (Ubuntu standard, macOS standard where portability
  requires it).
- Forbidden: GitHub larger runners, paid runner SKUs, GPU runners.
- Self-hosted runner: NOT APPROVED for this public repository. Public fork/PR execution
  creates an unnecessary host-security boundary.

### Scheduled medium-tier budget

- Frequency: maximum once per week automatically.
- Primary platform: Ubuntu x86_64 standard GitHub-hosted runner.
- Maximum 1 primary timing job.
- Maximum job timeout: 45 minutes.
- Performance result: NON-BLOCKING / REPORTING.
- Timing noise alone does not block merges.

### Container spike budget

- Trigger: `workflow_dispatch` / manual only.
- Default platform: Ubuntu x86_64 standard runner.
- Maximum timeout: 60 minutes.
- Run candidates in the same job/environment where feasible.

### Artifact budget

- Maximum scheduled-run uploaded artifacts: 200 MiB.
- Retention ceiling: 7 days.
- OD-M5-007 may choose a shorter exact retention during the spike.
- Actions artifact storage is not the immutable source of record for the corpus.

### Blocking PR workload

PR-blocking M5 workloads remain: tiny/small deterministic correctness, small differential
replay, semantic manifest correctness, fuzz smoke, benchmark execution smoke.
No medium/large recorded corpus should be downloaded merely to approve an ordinary PR.

## OD-M5-003: Semantic Manifest Granularity

**SPIKE-RESOLVABLE.** Not closed in this decision. Per-event versus per-checkpoint
granularity is decided during the performance spike.

## M5 Implementation Authorization

M5 Implementation: **IN PROGRESS.**

Phase 1 is **COMPLETE / MERGED** (PR #11, merge `5e8629a7ff825f8ea941304d9b09be1670643e8a`,
post-merge main CI `31264500905` — PASS 16/16). Phase 2 (independent differential oracle:
R1 promoted, R4 new, neutral ReplayDriver, OperationObservation, layer attribution) is
**IMPLEMENTED / PENDING INDEPENDENT REVIEW** — see `docs/M5_PHASE2_DIFFERENTIAL_ORACLE.md`.
Phase 3 and later phases remain not started.

## Phase 1 Review Dispositions

| ID | Disposition |
|---|---|
| M5-P1-IR-1 | **CLOSED** - Spot bootstrap bridge follows the accepted M3/ADR-0005 contains-`L` rule |
| M5-P1-IR-2 | **CLOSED** |
| M5-PIR-001 | **CLOSED** - explicit Spot/USD-M bootstrap and bridge materializer contract |
| M5-PIR-002 | **DEFERRED / NON-BLOCKING** |
| M5-PIR-003 | **DEFERRED / NON-BLOCKING** - before required branch protection |
| M5-PIR-004 | **CLOSED** - explicit offline acquisition boundary |

## Non-Goals

- No M5 phase 3+ implementation in this PR.
- No production code changes.
- No Recorder modification.
- No corpus download/upload.
- No M6 work.

## Remaining Open Decisions

| ID | Status |
|---|---|
| OD-M5-003 | SPIKE-RESOLVABLE |
| OD-M5-004 | CAN BE RESOLVED DURING SPIKE |
| OD-M5-005 | CAN BE RESOLVED DURING SPIKE |
| OD-M5-006 | CAN BE RESOLVED DURING SPIKE |
| OD-M5-007 | CAN BE RESOLVED DURING SPIKE |
