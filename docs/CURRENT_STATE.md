# Current State

```text
REPOSITORY=BinanceMarketDataProjection
M0-M4=COMPLETE
M5_PHASE1_11=COMPLETE
M5=COMPLETE
M6=COMPLETE_THROUGH_REAL_GATEWAY_INTEGRATION
GATEWAY_G0_G11=COMPLETE
POST_G11_RUNTIME_PRODUCTIZATION=COMPLETE
RECOVERY_OBSERVABILITY=COMPLETE
PERFORMANCE_INSTRUMENTATION=COMPLETE
POST_G11_PERFORMANCE_BASELINE=COMPLETE
PROJECTION_P0=COMPLETE
PROJECTION_P1=COMPLETE
PROJECTION_M_P2_A=COMPLETE
PROJECTION_M_P2_B=COMPLETE
PROJECTION_M_P2_C=COMPLETE
KEEP_STD_MAP

M6_CONTRACTS_PACKAGE_PUBLICATION=SEPARATE_FUTURE_RELEASE_CONCERN_NOT_STAGE_BLOCKER
PROJECTION_ISSUE_46=CLOSED_COMPLETED_2026-08-27
PROJECTION_ISSUE_47=CLOSED_COMPLETED_2026-08-27
```

CURRENT_STATE is an orientation summary only. It does not override accepted ADRs, accepted
milestone designs, implementation evidence, or current Git/GitHub state. When those sources
disagree, verify the higher-authority source and update this file.

## M-P2.A in-place accepted apply (2026-11-11)

Detailed record: [Projection M-P2.A-C Result](PROJECTION_M_P2_RESULT.md).

P1 measured full-book copy-on-apply as the dominant accepted-update hotspot. M-P2.A removed it:

- `BookProjection::apply` skips book work for an empty batch and otherwise calls
  `OrderBook::apply_updates` directly on the live book.
- `OrderBook::copy_from_map` reserves the expected output size; same-price insert/update uses one
  map lookup.
- The two synthetic global-`operator new` allocation-failure test targets and their source files
  were deleted.
- Accepted apply now keeps the basic exception guarantee. Baseline installation remains
  transactional. [ADR-0010](adr/ADR-0010-in-place-accepted-apply.md) supersedes only ADR-0005's
  copy-on-apply transaction decision; sequence policy is unchanged.
- Regression coverage: `M3AcceptedApplyMutatesInPlace` (one new level -> one allocation, no old-book
  deallocation) and `M3EmptyAcceptedBatchDoesNoBookWork` (zero allocations).
- Measured after the change: D5000 existing-price B10/B100 accepted apply = 0 allocations and
  0 bytes; D0/B10 empty-book insertion = 10 allocations / 480 requested bytes (one node per new
  price level). The former D5000 measurement was 10,029 allocations / 1,004,312 requested bytes.

P1's measurements remain valid as pre-optimization evidence:
[four-product result](PROJECTION_P1_RESULT.md) and
[twenty-key result](PROJECTION_P1_TWENTY_RESULT.md). Target-VPS/Recorder budgets remain a Gateway
G3 concern.

## M-P2.B adapter fast path (2026-11-11)

- `adapt_depth_update` now appends bids and asks into one pre-reserved owning vector; the
  `AdaptDepthUpdate/Spot/10` allocation count is 1 allocation / 240 bytes (formerly 2 / 480).
- Adapted owners move their level vectors and metadata instead of copying them.
- Snapshot output uses standard Protobuf `set_*`, `mutable_*`, and `Add()` APIs; the manual
  `set_allocated_*` + `unique_ptr` + `bad_alloc` rollback assembly is removed.
- `adapt_trusted_depth_update`, `adapt_trusted_exchange_depth_snapshot`,
  `install_into_trusted`, and `apply_to_trusted` provide an in-process path that skips repeated
  fixed-metadata UTF-8/identity checks and target `NumericSpec`/policy checks. Level decimals,
  range, and quality enums remain validated. A local exploratory loop measured the same 10-level
  update at ~256 ns/op checked vs ~232 ns/op trusted.
- Local Release `AdaptDepthUpdate/Spot/10` mean: 0.2743 -> 0.2572 us; allocation count
  2 / 480 bytes -> 1 / 240 bytes. Snapshot timing remains dominated by per-level decimal
  formatting/string allocation; its allocation count is reduced but the timing change is small.

## M-P2.C CI deduplication (2026-11-11)

- The M4 static matrix no longer repeats Ubuntu GCC and Ubuntu Clang Release builds already
  produced by the Release build matrix. Its remaining matrix is Ubuntu GCC/Clang Debug and
  macOS AppleClang Debug/Release.
- Deleted the 1,248-line `scripts/test-quality-toolchain.sh` fake-environment suite and its
  `quality-toolchain-tests` CI job.
- `scripts/verify.sh` and contributor documentation no longer invoke the removed suite. The
  containerized `quality` job and `scripts/quality.sh` remain the single canonical Quality
  acceptance path.

## Repository role

Projection owns deterministic market-state projection semantics, fixed-point interpretation, the
single-writer order book, Spot/USD-M sequence policy, projection lifecycle, the optional Protobuf
adapter boundary, and M5 validation infrastructure. It does not own Binance network acquisition,
Recorder persistence/archive, or Gateway runtime.

## Accepted evidence

```text
M4_LIMITED_IDENTITY=depth_limit=20
M4_ADAPT_DEPTH_IDENTITY=M4/AdaptDepthUpdate/Spot/10

PHASE6_CORRECTED_FORMAL_EVIDENCE=PASS
PHASE6_INVENTORY=167/167
PHASE6_REPETITIONS=5
PHASE6_OLD_FALSE_IDENTITIES=ABSENT

PHASE7_CORRECTED_FORMAL_EVIDENCE=PASS
PHASE7_M2M3=124/124
PHASE7_FOOTPRINT=5/5
PHASE7_REPLAY=4/4
PHASE7_M4=22/22
PHASE7_DETERMINISM=PASS

PHASE8_MODEL_SEMANTICS=UNCHANGED
PHASE9_DECISION=KEEP_STD_MAP
PHASE10=COMPLETE
PRODUCTION_CORE_CHANGED_BY_PR42=NO
```

The corrected Phase-6 inventory is truthful for the post-PR42 formal executable. The original PR #21
acceptance remains historical `169/169` in
[M5 Phase 6](M5_PHASE6_REPRESENTATIVE_BENCHMARKS.md); it was not erased or rewritten. Phase-6/7
timing and allocation evidence predates M-P2.A and describes the former full-book transaction.

M5 remains validation-only: it introduced no Production Core semantic change, no production
container migration, and no Gateway runtime. Detailed history remains in the phase documents:

- [Phase 1](M5_PHASE1_CANONICAL_REPLAY.md) through
  [Phase 5](M5_PHASE5_DIFFERENTIAL_FUZZING.md);
- [Phase 6](M5_PHASE6_REPRESENTATIVE_BENCHMARKS.md), including the corrective acceptance;
- [Phase 7](M5_PHASE7_PREIMPLEMENTATION_DECISIONS.md);
- [Phase 8](M5_PHASE8_CONTAINER_BENCHMARK_EVIDENCE.md);
- [Phase 9](M5_PHASE9_CONTAINER_DECISION.md);
- [Phase 10](M5_PHASE10_CI_REPORTING_INTEGRATION.md); and
- [Phase 11](M5_PHASE11_FINAL_IMPLEMENTATION_REVIEW.md).

The accepted semantic correction for Spot successor coverage is
[ADR-0008](adr/ADR-0008-spot-bootstrap-successor-coverage.md); the historical superseded decision
remains in [ADR-0005](adr/ADR-0005-market-specific-sequence-policy.md).

## M6 and Gateway closure

M6 planning and the approved sequence are recorded in
[M6 Gateway Integration Sequence](M6_GATEWAY_INTEGRATION_SEQUENCE.md) and the completed
[M6 cross-repository integration contract](M6_CROSS_REPOSITORY_INTEGRATION_CONTRACT.md). Gateway G8
is the accepted real-Gateway Projection order-book integration boundary; Gateway later completed
G9-G11, post-G11 runtime productization, recovery observability, performance instrumentation, and
its accepted performance baseline. Gateway implementation and runtime authority live in
`BinanceMarketDataGateway`; no Projection production change was required.

The current Projection package integration consumes the message-only
`binance-market-data-contracts-cpp/0.1.0` artifact from Contracts main
`518880bdfa60948c3b65b6b3525d024526995166`, with message RREV
`53713fb82c27cf6ea0395f6b8a853006`. The separate Contracts gRPC artifact is not a Projection
dependency. Formal package publication/release remains a future release concern.

## Authority model

For semantic decisions, use this order:

1. verified external Binance protocol facts recorded in accepted repository authorities;
2. accepted ADRs and accepted milestone designs;
3. checked-out implementation, tests, Git history, build results, and CI; and
4. status/orientation summaries such as this file.

The repository keeps one documentation authority model:

- CURRENT_STATE: current orientation;
- ARCHITECTURE.md: stable module architecture;
- AGENTS.md: contribution and AI/reviewer workflow;
- docs/QUALITY_TOOLCHAIN.md: canonical Quality/toolchain authority;
- docs/MILESTONES.md: milestone ledger and definitions;
- accepted ADRs: semantic decisions; and
- phase-specific M*/Phase documents: historical design, acceptance, and evidence.

No additional status/index layer is needed.

## Validation workflow

For docs-only changes, lightweight documentation validation is sufficient unless executable
configuration changes. For C++/test/build-toolchain-sensitive changes, use focused validation,
`scripts/verify.sh` as applicable, then canonical `bash scripts/quality.sh`, followed by push and
automatic GitHub CI. `quality.sh` is the canonical Quality acceptance entrypoint; `verify.sh` is
broad supplemental verification.

## AI/reviewer reading order

1. `docs/CURRENT_STATE.md`
2. `AGENTS.md`
3. `README.md`
4. `ARCHITECTURE.md`
5. `docs/MILESTONES.md`
6. `docs/M6_GATEWAY_INTEGRATION_SEQUENCE.md`
7. relevant accepted ADRs and phase-specific documents
8. code/tests/build configuration
9. active PR and exact-head CI, if one exists
