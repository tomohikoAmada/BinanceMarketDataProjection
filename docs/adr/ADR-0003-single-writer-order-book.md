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
ordering. This ADR establishes architecture only; M0 does not implement an order book.
