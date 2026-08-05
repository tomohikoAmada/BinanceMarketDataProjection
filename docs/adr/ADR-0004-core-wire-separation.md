# ADR-0004: Core/Wire Separation

- Status: ACCEPTED
- Date: 2026-08-05

## Context

Wire schemas evolve for transport compatibility, while deterministic domain logic needs a focused,
independently testable API.

## Decision

Protobuf types and runtime dependencies do not enter the Core API. A separate adapter target will
translate between wire contracts and domain types in M5. M0 creates neither the adapter nor an empty
placeholder target.

## Consequences

Core can be built, unit-tested, replay-tested, and fuzzed without Protobuf. Adapter versioning and
translation costs are explicit at the boundary.
