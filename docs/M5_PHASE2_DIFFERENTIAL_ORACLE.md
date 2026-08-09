# M5 Phase 2 Differential Oracle

## Status

- M5 implementation: **IN PROGRESS**
- Phase 1: **COMPLETE / MERGED** (PR #11, merge `5e8629a7ff825f8ea941304d9b09be1670643e8a`,
  post-merge main CI `31264500905` — PASS 16/16)
- Phase 2: **CORRECTED / PENDING INDEPENDENT RE-REVIEW**
- Phase 3: **NOT STARTED**
- OD-M5-003: **SPIKE-RESOLVABLE**
- M6: **NOT STARTED**

Phase 2 is test-only infrastructure. It does not change `include/**`, `src/**`, CI workflows,
Conan, or the Recorder repository. Production Core and ProtoAdapter are unchanged.

## Scope

Phase 2 establishes the independent differential oracle foundation approved by ADR-0007:

```text
canonical replay operation
  |
  +--------------------------------------+
  |                                      |
  v                                      v
production pipeline              reference pipeline
M1/M2/M3/M4                     R1/R2/R3/R4
  |                                      |
  +---------------> D <------------------+
  |
OperationObservation
  |
semantic comparison (first divergence, layer attribution)
```

| Component | Implementation |
|---|---|
| R1 | `tests/m5/reference/reference_decimal.{hpp,cpp}` — promoted from the embryonic `reference_parse`/`reference_fixed` property model. Independently decomposes the lexeme into integer/fraction substrings, represents the source-scale magnitude, proves exact discarded suffixes, then performs checked target-scale rescaling; also owns error offsets and canonical fixed formatting. Must not call M1 parser/formatter internals. |
| R2 | `tests/order_book/reference_order_book.hpp` — REUSED in place, unchanged (M2 vector-based reference; existing tests preserved). |
| R3 | `tests/projection_state/reference_projection.hpp` — REUSED in place, unchanged (own decision tables; Spot `U <= L < u` bootstrap contains-`L` rule retained; live successor coverage; USD-M `U <= L <= u` then `pu` continuity). |
| R4 | `tests/m5/reference/reference_adapter.{hpp,cpp}` — new. Own wire-enum mapping tables, independently variable wire/expected/projection dimensions, quality mapping/ranking, snapshot eligibility matrix, gap mapping, semantic snapshot construction, text-identifier rules, per-side depth-limit rules, and inbound field precedence. Consumes R1/R3 values as value types only. |
| D | `tests/m5/oracle/replay_driver.{hpp,cpp}` — neutral orchestration: loads an already-parsed `ReplayFixture`, dispatches one normalized operation to each side, stamps event identity, compares, stops at the first divergence. Contains no classification, decimal, book, or eligibility logic. |
| Observation model | `tests/m5/oracle/operation_observation.hpp` — canonical `OperationObservation`: event index, event kind, ordered per-token decimal parse evidence, operation result, post-operation `SemanticCheckpoint`, optional snapshot semantic observation. Deterministic integer/unit values and stable enum names only. |
| Comparison | `tests/m5/oracle/divergence.{hpp,cpp}` — fixed first-divergence order: (1) successful/error decimal evidence, (2) operation-result kind, (3) result value/error fields, (4) `SemanticCheckpoint`, (5) snapshot semantic observation. Stops at the first mismatch with structured attribution. |

### Layer attribution

| Layer | Attribution scope |
|---|---|
| R1 | decimal parse/rescale result divergences |
| R2 | book-content divergences (checkpoint bid/ask level vectors) |
| R3 | sequence/lifecycle divergences (status, last update ID, gap evidence, synchronized visibility, install/apply/reset/range results, NumericSpec identity) |
| R4 | M4 boundary divergences (adapter errors, observed quality, snapshot semantics) |
| D | composition divergences (event kind mapping, missing observations, malformed-operation intent, snapshot-not-produced vs produced) |

In the composed projection oracle the reference book content is carried by R3's storage, but a
level-content divergence is attributed to the M2 book-content layer (R2) that owns those
semantics. Deliberate fault-injection tests prove first-divergence reporting for operation-result
(R1/R3/R4), checkpoint (R2/R3), snapshot (R4), and composition (D) mismatches without modifying
production code.

## Operation-result domain

For every canonical event class the operation result is one of:

- `DecimalErrorOutcome` — Core-only parse failure (canonical M1 error category).
- `InstallOutcome` — disposition, status after, last update ID after.
- `ApplyOutcome` — disposition, status after, last update ID after, gap evidence
  (last accepted final, incoming range, previous final, reason, policy). The full result is
  compared, not only the disposition: `IgnoredStale` and `IgnoredDuplicate` produce identical
  state but different observable results.
- `AdapterSuccessOutcome` — underlying M3 result plus adapted observed quality (canonical rank).
- `AdapterErrorOutcome` — code, field, optional decimal error category. Stable semantic fields
  only; `raw_enum_value` is not representable in the replay grammar and remains M4 unit/property
  scope; exception text, compiler strings, addresses, and ABI are never compared.
- `SnapshotOutcome` — semantic snapshot fields (identity, producer/source, timestamps, last
  update ID, ordered levels in canonical fixed format, quality flags in canonical rank,
  depth-limit effect, gap descriptor, synchronized flag). Protobuf byte equality is never
  compared.
- `ResetOutcome`, `RangeOutcome` (MALFORMED_RANGE must fail domain construction and never reach
  apply), `MetadataOutcome` (ADAPTER_METADATA carries context only; its observable effect appears
  in the following event's result), `SnapshotNotProducedOutcome` (Core-only mode does not link
  the M4 boundary).

## Modes

- **Core-only** (`ReplayMode::CoreOnly`): R1 + R3 reference side against production M1/M3.
  Never links ProtoAdapter/Contracts. `SNAPSHOT_REQUEST` yields `SnapshotNotProducedOutcome` on
  both sides because snapshot semantics belong to the M4 boundary.
- **Adapter-enabled** (`ReplayMode::AdapterEnabled`): production M4 ProtoAdapter driven through
  synthesized Contracts wire messages plus the R4 independent semantic prediction. Non-semantic
  inbound request/connection identifiers and inbound producer fields are fixed driver constants;
  `SNAPSHOT_REQUEST` producer and producer-version validation/output remain differential scope.

`REBASELINE` is an M3 lifecycle operation: it is driven as a Core-level `install_baseline` in
both modes and does not cross the M4 boundary (the approved M4 dimension table binds
`adapt_exchange_depth_snapshot` to `INSTALL_BASELINE` only).

## Successful decimal evidence and R1 independence

Every level token now contributes two ordered observations before any M3 or M4 result is compared:
side, normalized input position, price/quantity role, and either `(units, storage scale, source
fraction digits)` or `(error category, error offset)`. Production evidence calls the public M1
parser; reference evidence calls only R1. Consequently, stale/duplicate/wrong-state operations
cannot hide a successful-parse disagreement behind an identical business disposition and
checkpoint. A seeded duplicate-operation fault proves parse evidence is attributed to R1 before
the ignored-duplicate result is considered.

R1 no longer follows production's digit-at-a-time target accumulator shape. Its phases are:

1. validate and decompose the complete lexeme without numeric or target-scale state;
2. represent the complete source magnitude as integer/fraction substrings and accumulate only the
   exact retained magnitude with an independently phrased quotient/remainder overflow check;
3. prove any discarded fractional suffix is all zero, then apply checked decimal padding.

Manually derived success/error tables cover grammar edges, exact up/downscale, inexact suffixes,
`int64_t` boundaries, padding overflow, zero-domain behavior, preserved source scale, and a
1,000-digit exact trailing-zero suffix. Generated M1 differential cases remain supplemental.

## Adapter dimension composition and authority exclusions

`AdapterScenario` is the test-only neutral input used by the adapter production/reference side
factory overloads. It independently carries wire venue/market/symbol, expected symbol/policy,
conversion numeric spec, target-projection numeric spec, and target-projection policy. Production
maps these values to Contracts/Core types; R4 maps them to its own `ReferenceVenue`,
`ReferenceMarket`, `ReferencePolicy`, and `ReferenceNumericSpec` types. Regression rows cover
unspecified venue/market, known cross-market and symbol identity mismatches, price/quantity target
spec mismatches, and projection-policy mismatch.

Raw unknown Protobuf numeric enum values remain deliberately excluded: the Phase-1 canonical
replay grammar has closed `Spot`/`UsdMPerpetual` and Binance venue semantics, and the M5 design
excludes raw unknown enum injection. Under the pinned Contracts enum domain there is no second
known venue and no market beyond Spot/USD-M to compose `UnsupportedVenue` or `UnsupportedMarket`
without violating that authority. Those two branches and `UnknownEnumValue` therefore remain M4
unit/property scope; the differential scenario covers the composable `UnspecifiedEnum` and
known-value `IdentityMismatch` branches.

Inbound depth fields are validated in the M4 public order: all bids in repeated-field order, then
all asks in repeated-field order, with price before quantity within each level. Snapshot output
selects `min(depth_limit, side.size())` independently for bids and asks. Producer and producer
version use the M4 non-empty-text contract: length 1..256 with no leading/trailing ASCII
whitespace; empty maps to `MissingRequiredField`, while an invalid non-empty value maps to
`InvalidIdentifier`. Differential boundary tests cover lengths 1, 256, and 257 for both fields.

`MALFORMED_RANGE` is also fail closed at composition layer D. Agreement between production and
reference is insufficient when a fixture labels an equal or forward range as malformed; the
comparator emits a composition divergence in both Core-only and adapter-enabled modes. Reversed
ranges continue to prove that domain construction fails before apply and leaves state unchanged.

## ADAPTER_METADATA scope correction

The approved M4 dimension table binds `adapt_exchange_depth_snapshot` to
`INSTALL_BASELINE + ADAPTER_METADATA` and `adapt_depth_update` to
`DEPTH_UPDATE + optional ADAPTER_METADATA`. Phase 1's parser ordering rule allowed
`ADAPTER_METADATA` only immediately before `DEPTH_UPDATE`, which prevented baseline inbound-quality
differential coverage.

The smallest backwards-compatible test-support correction is applied: the parser now accepts
`ADAPTER_METADATA` immediately before `INSTALL_BASELINE` or `DEPTH_UPDATE`
(`tests/m5/replay/replay_parser.cpp`). All Phase-1 fixtures remain valid; no grammar token or
other rule changed. This is documented here and in `docs/M5_PHASE1_CANONICAL_REPLAY.md`. The
fixture `adapter_tiny` exercises baseline inbound quality end-to-end.

## Quality-domain separation

- Inbound wire quality: `ADAPTER_METADATA` facts map into `AdaptedMetadata::observed_quality`
  (production) / R4 predicted observed quality (reference); they never flow into snapshot output
  automatically.
- Host-observed quality: `SNAPSHOT_REQUEST` facts are validated against projection state and
  appear in output quality flags.
- Core-derived quality: `CROSSED_BOOK`, `SEQUENCE_GAP`, `SNAPSHOT_BRIDGE_PENDING` are derived
  from book/projection state and emitted only in output. R4 owns its own ranking and derivation
  tables.

The three domains are kept separate; tests assert that inbound quality does not leak into output
and that derived flags appear only in output.

## Determinism

Repeated replay within one process yields identical `OperationObservation` sequences,
checkpoints, snapshot observations, and (for seeded faults) identical first-divergence reports.
Semantic typed observation equality is Phase-2 scope; canonical observation serialization,
SHA-256 digests, and cross-compiler transport remain Phase 4 (OD-M5-003 stays SPIKE-RESOLVABLE).

## Fault injection

`MutatingSide` and `FailingSide` (test-only driver wrappers) seed deliberate mismatches in one
pipeline side. Tests prove the driver stops at the first divergence and reports the correct
event, layer, and category for: operation-result kind/field (R1, R3, R4), checkpoint (R2, R3),
snapshot semantics (R4), and composition (D). No production code is modified.

## Tiny fixture replay

`spot_tiny`, `usdm_tiny`, `recovery_tiny`, and the new `adapter_tiny` run through both modes
with deterministic production/reference agreement. `adapter_tiny` covers baseline inbound
quality, depth-limited and unlimited snapshots, host quality facts, crossed-book derivation,
forward gap, gap-context snapshot with descriptor, reset, and rebaseline. No small/medium/large
corpus is created (Phase 3).

## Phase-1 P2 backlog

No Phase-1 P2 finding is closed by this PR except where Phase 2 naturally reached the boundary:
the ADAPTER_METADATA scoping item is resolved by the documented scope correction above. All other
inherited P2 findings (manifest taxonomy, repository SHA known-vector coverage, C0/DEL
hardening, temporary-test-directory hygiene, materializer-contract testability, Spot tiny
fixture clarification, and the unidentified ninth finding) remain open and non-blocking.
M4-IIR-3 remains open until M5 corpus/fuzz evidence justifies closure.

## Build layout

```text
bmd_projection_m5_replay_support        (Phase 1, unchanged)
bmd_projection_m5_reference_support     R1 + R4 + reference side; no production linkage
bmd_projection_m5_oracle_support        observation model, comparison, driver, Core-only side
bmd_projection_m5_differential_tests    Core-only differential, R1/R4, observation, faults
bmd_projection_m5_adapter_differential_tests   adapter-enabled differential (optional target)
```

No M5 support library is installed or exported; `BUILD_TESTING=OFF` behavior is unchanged.
The Core-only target does not link ProtoAdapter; the adapter differential target links the
existing optional ProtoAdapter component and Contracts.
