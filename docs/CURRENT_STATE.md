# Current State

```text
CURRENT_MAIN=d324dca4375cd5d3c188d3f187962a90c6821621
CURRENT_TREE=4946037405279d244658db844a2c429f82060b8d
POST_MERGE_CI=32541072950 SUCCESS
OPEN_PRS=0

M0-M4=COMPLETE
M5_PHASE1_11=COMPLETE
M5=COMPLETE
PR42_CORRECTIVE_CLOSURE=COMPLETE
KEEP_STD_MAP

M6_DESIGN=NOT STARTED
M6_IMPLEMENTATION=NOT STARTED
GATEWAY_IMPLEMENTATION=NOT STARTED
```

CURRENT_STATE is an orientation summary only. It does not override accepted ADRs, accepted
milestone designs, implementation evidence, or current Git/GitHub state. When those sources
disagree, verify the higher-authority source and update this file.

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

## M6 and Gateway start control

```text
M6_DESIGN_STARTED=NO
M6_IMPLEMENTATION_STARTED=NO
GATEWAY_IMPLEMENTATION_STARTED=NO
M6_PLANNING_AUTHORIZED=YES
OWNER_EXPLICIT_START_REQUIRED=YES
```

M6 planning and the approved sequence are recorded in
[`M6_GATEWAY_INTEGRATION_SEQUENCE.md`](M6_GATEWAY_INTEGRATION_SEQUENCE.md). The next scoped
activity is the read-only, architecture-first cross-repository integration contract design.
Gateway runtime belongs in `BinanceMarketDataGateway`, not this repository. This documentation
closure does not start M6 design, M6 implementation, or Gateway implementation.

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
