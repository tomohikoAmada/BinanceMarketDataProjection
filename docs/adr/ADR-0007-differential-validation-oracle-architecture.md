# ADR-0007: Differential Validation Oracle Architecture

- Status: ACCEPTED
- Date: 2026-08-08
- Correction date: 2026-08-08 (M5-AR-001: OperationObservation semantic equality)
- Acceptance date: 2026-08-08
- Acceptance evidence: focused independent M5 architecture re-review APPROVED;
  reviewed head `9fff05ca8333d89d28d89c794d65255b56578715`; reviewed CI `31245814229` — PASS

## Context

M1 through M4 form a deterministic C++20 Core plus an optional Protobuf adapter. M5 must raise
correctness confidence through replay/differential/fuzz validation and characterize performance
without mixing the two. Existing independent reference models already exist per milestone
(`ReferenceOrderBook`, `ReferenceProjection`, and a primitive decimal reference in the M4 property
tests). M5 must decide how those models are composed into a replay oracle and how that oracle is
kept independent from production logic. This ADR records the durable architecture decision; the
detailed design lives in `docs/M5_DIFFERENTIAL_VALIDATION_AND_PERFORMANCE_DESIGN.md`.

## Decision

M5 will use a **layered reference oracle** composed from independently implemented milestone-layer
reference models (R1 decimal, R2 order book, R3 projection, R4 adapter semantics) driven by a
canonical, versioned, deterministic replay event log and a neutral `ReplayDriver` that invokes the
production pipeline and the reference pipeline through their public APIs.

- Reference layers must not call production business-logic helpers; they store primitive integers
  and `std::vector` only.
- The replay event log is a versioned canonical text format with a manifest (SHA-256, provenance);
  it is the normalized operation vocabulary shared with a future M6 Host.
- Semantic equality is the correctness oracle. Semantic equality means equality of the
  canonical per-event OperationObservation stream: the observable operation result/error
  plus the post-operation semantic checkpoint, for every event in the replay. State/checkpoint
  equality alone is insufficient — two implementations can return different observable results
  (e.g., IgnoredStale vs IgnoredDuplicate) while coincidentally reaching identical book state.
  Byte equality is used only to prove determinism within one binary under a pinned Protobuf
  runtime, never as a cross-toolchain correctness claim.
- Cross-toolchain correctness is proven by identical SHA-256 semantic digests over the
  canonical OperationObservation stream.
- A production order-book container change requires a separate, later decision (proposed as
  ADR-0008 at migration time) backed by the benchmark evidence and decision criteria defined in the
  M5 design.

## Consequences

### Positive

- Divergence is attributable to a specific milestone layer and event.
- Reviewed M2/M3 reference models are reused rather than discarded.
- Live and replay share one normalized operation vocabulary.
- Performance evidence and correctness evidence remain separable.

### Negative

- More test-only infrastructure to maintain (reference layers, fixtures, driver).
- Fixture and manifest maintenance burden must be managed with dataset tiers.
- The oracle must be audited for accidental sharing with production in every review.

## Alternatives

A monolithic reference model was rejected because it cannot attribute divergence to a layer, would
discard reviewed per-milestone models, and increases the risk of replicating production composition
logic.

## Acceptance

ACCEPTED by independent M5 architecture review on 2026-08-08. Acceptance scope is exactly the
independently reviewed durable architecture: the layered R1/R2/R3/R4 reference oracle; the neutral
ReplayDriver; the canonical replay representation; OperationObservation (observable operation
result/error plus post-operation semantic checkpoint); the semantic observation-stream digest;
cross-toolchain semantic comparison; and the separate future production-container migration
decision. Acceptance does not authorize a production container migration. M5 implementation
remains blocked by OD-M5-001 and OD-M5-002 and requires a separate implementation branch,
validation cycle, and external code review.

## Superseded by

None.
