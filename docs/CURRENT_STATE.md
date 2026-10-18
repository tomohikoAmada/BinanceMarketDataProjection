# Current State

```text
REVIEWED_BASE_MAIN=59a7bca6a22e39a31323b221d18cfd4d5ac24b90
REVIEWED_BASE_TREE=d18e4e66c3eb5972c3a96e6557f28fbf487468fd
DOC_FREEZE_PR=44
FULL_CI_RUN=NO
CURRENT_TECHNICAL_CANDIDATE=4b01082fc31dfb3a24ec65542a7931eab8520f26
TECHNICAL_REVIEW=APPROVED_FOR_DOCUMENTATION_ALIGNMENT
TECHNICAL_REVIEW_FINDINGS=P0=0 P1=0 P2=0 P3=0
TECHNICAL_CI_RUN=33059838857 SUCCESS
CONTRACTS_MAIN=518880bdfa60948c3b65b6b3525d024526995166
CONTRACTS_MESSAGE_RREV=53713fb82c27cf6ea0395f6b8a853006
M0-M4=COMPLETE
M5_PHASE1_11=COMPLETE
M5=COMPLETE
PR42_CORRECTIVE_CLOSURE=COMPLETE
KEEP_STD_MAP

M6_CROSS_REPOSITORY_CONTRACT_DESIGN=COMPLETE
M6_DESIGN=COMPLETE_CONTRACT_ONLY
M6_IMPLEMENTATION=COMPLETE_THROUGH_REAL_GATEWAY_INTEGRATION
GATEWAY_G0_FOUNDATION=COMPLETE
GATEWAY_G1_EXACT_CANDIDATE_DEPENDENCY_INTEGRATION=MERGED
GATEWAY_RUNTIME_PIPELINE=IMPLEMENTED_THROUGH_G11
GATEWAY_G8=COMPLETE
GATEWAY_G9=COMPLETE
GATEWAY_G10=COMPLETE
GATEWAY_G11=COMPLETE
POST_G11_RUNTIME_PRODUCTIZATION=COMPLETE
RECOVERY_OBSERVABILITY=COMPLETE
PERFORMANCE_INSTRUMENTATION=COMPLETE
POST_G11_PERFORMANCE_BASELINE=COMPLETE
RECOVERY_OBSERVATION_CAMPAIGN=COMPLETE
GATEWAY_NEXT=NOT_FROZEN
M6_REAL_GATEWAY_INTEGRATION_ACCEPTANCE=COMPLETE
M6=COMPLETE
PROJECTION_PRODUCTION_CODE_CHANGED_BY_M6=NO
M6_P1_001=RESOLVED_IN_CONTRACTS_MAIN
M6_P1_003=RESOLVED_IN_CONTRACTS_MAIN
M6_CONTRACTS_PACKAGE_PUBLICATION=SEPARATE_FUTURE_RELEASE_CONCERN_NOT_STAGE_BLOCKER
PROJECTION_ISSUE_46=CLOSED_COMPLETED_2026-08-27
PROJECTION_ISSUE_47=CLOSED_COMPLETED_2026-08-27
```

CURRENT_STATE is an orientation summary only. It does not override accepted ADRs, accepted
milestone designs, implementation evidence, or current Git/GitHub state. When those sources
disagree, verify the higher-authority source and update this file.

## Projection P0 under the v0.3 plan

The owner authorized Projection P0 on 2026-10-12: fixed-asset acquisition and the
tests+benchmarks Python CTest repair. The candidate is implemented on
`codex/projection-p0-ci-ctest` and awaits independent review. Scope, diagnosis,
and validation are recorded in [the P0 result](PROJECTION_P0_RESULT.md).
P1/P2, Gateway G1/G2, production VPS access, and Recorder qualification work
are outside this change.

## Repository role

Projection owns deterministic market-state projection semantics, fixed-point interpretation, the
single-writer order book, Spot/USD-M sequence policy, projection lifecycle, the optional Protobuf
adapter boundary, and M5 validation infrastructure. It does not own Binance network acquisition,
Recorder persistence/archive, or Gateway runtime.

## Current accepted evidence

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
PHASE8_TARGET_WIRING=SEPARATED_FROM_PHASE6_FORMAL_EXECUTABLE
PHASE9_DECISION=KEEP_STD_MAP
PHASE10=COMPLETE
PRODUCTION_CORE_CHANGED_BY_PR42=NO
```

The corrected Phase-6 inventory is truthful for the post-PR42 formal executable. The original
PR #21 acceptance remains recorded as historical `169/169` in
[`docs/M5_PHASE6_REPRESENTATIVE_BENCHMARKS.md`](M5_PHASE6_REPRESENTATIVE_BENCHMARKS.md); it was
not erased or rewritten.

Phase-8 target wiring is separated from the Phase-6 formal executable; Phase-8 model semantics
remain unchanged.

PR #42 corrected benchmark evidence provenance only. It did **not** change Production Core
semantics. The corrected evidence record explains the inventory and identity changes and links to
the detailed Phase-6, Phase-7, Phase-8, Phase-9, Phase-10, and Phase-11 records.

## M5 final state

M5 Phase 1 through Phase 11 are complete. At a high level, M5 provides:

- deterministic replay;
- independent differential/reference validation;
- cross-compiler semantic manifests;
- differential fuzzing;
- representative benchmark evidence;
- allocation/memory evidence;
- Phase-8 container experiments with the `KEEP_STD_MAP` decision;
- recorded replay and a weekly exploratory performance canary; and
- a final independent repository audit.

M5 remains validation-only: it introduced no Production Core semantic change, no production
container migration, and no Gateway runtime. Detailed historical evidence remains in the
phase-specific documents:

- [Phase 1](M5_PHASE1_CANONICAL_REPLAY.md) through
  [Phase 5](M5_PHASE5_DIFFERENTIAL_FUZZING.md);
- [Phase 6](M5_PHASE6_REPRESENTATIVE_BENCHMARKS.md), including the corrective acceptance;
- [Phase 7](M5_PHASE7_PREIMPLEMENTATION_DECISIONS.md);
- [Phase 8](M5_PHASE8_CONTAINER_BENCHMARK_EVIDENCE.md);
- [Phase 9](M5_PHASE9_CONTAINER_DECISION.md);
- [Phase 10](M5_PHASE10_CI_REPORTING_INTEGRATION.md); and
- [Phase 11](M5_PHASE11_FINAL_IMPLEMENTATION_REVIEW.md).

The accepted semantic correction for Spot successor coverage is [ADR-0008](adr/ADR-0008-spot-bootstrap-successor-coverage.md);
the historical superseded decision remains in [ADR-0005](adr/ADR-0005-market-specific-sequence-policy.md).

## M6 and Gateway integration closure

```text
M6_CROSS_REPOSITORY_CONTRACT_DESIGN=COMPLETE
M6_DESIGN_STARTED=YES_CONTRACT_ONLY
M6_IMPLEMENTATION_STARTED=YES_IN_GATEWAY_G8
GATEWAY_G0_FOUNDATION=COMPLETE
GATEWAY_G1_EXACT_CANDIDATE_DEPENDENCY_INTEGRATION=MERGED_AT_04e7cc37622a5feb0e416bf75ba000f13aa7f62a
GATEWAY_RUNTIME_PIPELINE_STARTED=YES_THROUGH_G11
M6_PLANNING_AUTHORIZED=YES
GATEWAY_G8=COMPLETE
GATEWAY_G9=COMPLETE
GATEWAY_G10=COMPLETE
GATEWAY_G11=COMPLETE
GATEWAY_NEXT=NOT_FROZEN
M6_REAL_GATEWAY_INTEGRATION_ACCEPTANCE=COMPLETE
PROJECTION_PRODUCTION_CODE_CHANGED_BY_M6=NO
```

M6 planning and the approved sequence are recorded in
[`M6_GATEWAY_INTEGRATION_SEQUENCE.md`](M6_GATEWAY_INTEGRATION_SEQUENCE.md) and the completed
[`M6 cross-repository integration contract`](M6_CROSS_REPOSITORY_INTEGRATION_CONTRACT.md).
The contract design is complete. Gateway G0 through G8 are complete, and Gateway G8 is the
accepted real-Gateway Projection M6 order-book integration boundary. Gateway later progressed
through G9, G10, and G11, then completed post-G11 runtime productization, recovery observability,
performance instrumentation, and the accepted post-G11 performance baseline. Gateway implementation
and runtime pipeline are in `BinanceMarketDataGateway`; live Gateway repository authority must be
verified in that repository. No Projection production code change was required.
The current Projection package integration consumes the message-only
`binance-market-data-contracts-cpp/0.1.0` artifact from Contracts main
`518880bdfa60948c3b65b6b3525d024526995166`, with message RREV
`53713fb82c27cf6ea0395f6b8a853006`. The separate Contracts gRPC artifact is not a Projection
dependency. Formal package publication/release is a separate future release concern, not a blocker
for closing this repository-level Projection stage. Issues #46 and #47 were closed as completed
on 2026-08-27. Gateway runtime belongs in `BinanceMarketDataGateway`, not this repository. G9 and
G10 were not prerequisites for the historical M6 acceptance; both, plus G11, are now complete. No
further numbered Gateway milestone is frozen.

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
- phase-specific M*/Phase documents: detailed historical design, acceptance, and evidence.

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

The orientation summary is deliberately not a substitute for those authorities.
