# ADR-0003: Single Writer Order Book

- Status: ACCEPTED
- Date: 2026-08-05

## Context

Deterministic ordered event processing is easier to reason about when mutation has one owner.

## Decision

Each future Venue/Market/Symbol projection instance has one ordered writer. Core creates no threads;
the host owns concurrency, queues, affinity, and scheduling. Results leave the Core by value or as
immutable objects.

## Consequences

Core algorithms need no internal scheduler or competing-writer synchronization. Hosts must enforce
ordering.

## M2 implementation

M2 implemented a single-writer order book with the following properties:

- Core creates no threads, locks, or atomics.
- The host guarantees ordered calls per projection instance.
- Storage is a PIMPL-hidden `std::map` (M2 correctness baseline).
- Quantity is absolute (not delta); zero deletes; missing delete is a no-op.
- Crossed and locked books are retained without rejection.
- Results leave by value or as immutable objects.
- Container selection may be re-evaluated in M5.
- M2 has no sequence tracking or update IDs.
