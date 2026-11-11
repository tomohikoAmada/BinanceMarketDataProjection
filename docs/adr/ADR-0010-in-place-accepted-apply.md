# ADR-0010: In-Place Accepted Apply

- Status: ACCEPTED
- Date: 2026-11-11
- Supersedes: ADR-0005 exception-safety transaction decision and the corresponding M3 design
  section

## Context

ADR-0005 selected logical copy-on-apply for every accepted `BookProjection::apply` batch. That
decision copied both complete sides with `all_levels`, rebuilt a candidate `OrderBook`, applied the
batch to it, and committed by move assignment. It provided the strong exception guarantee, but it
also copied and destroyed the full book for every accepted batch, including an empty batch.

Projection P1 measured the result: at depth 5000 a single accepted apply performed 10,029
replaceable-global-new allocations and about 1 MB of requested bytes, independent of batch size.
Full-book copying was the dominant accepted-update cost. The synthetic global-`operator new`
allocation-failure tests existed to validate the rollback transaction, not a Host requirement.

## Decision

Accepted apply mutates the live `OrderBook` in place:

```cpp
if (!levels.empty()) {
    book_.apply_updates(levels);
}
```

- An accepted batch with no levels does no book work.
- A non-empty batch applies levels in input order directly to the live book.
- Sequence state advances only after `apply_updates` returns.
- Baseline installation remains transactional and keeps the strong exception guarantee through
  `OrderBook::replace_all`.
- Accepted apply keeps the basic exception guarantee. If a new-price-node allocation throws, the
  live book may be partially updated and the accepted ID/status are not advanced. Allocation
  failure is not converted into a synthetic resync event.

The two dedicated synthetic allocation-failure test targets and their global `operator new`
overrides are removed. Deterministic semantic tests, property/fuzz models, and the M5 allocation
instrumentation remain the evidence for normal behavior.

## Consequences

- Accepted-apply cost is O(batch size × log(book depth)); only a newly inserted price level
  allocates a map node. Existing-price updates and deletions mutate or free nodes in place.
- The projection no longer claims strong exception safety for accepted apply. Hosts that treat
  allocation failure as recoverable must resynchronize instead of assuming an unchanged book.
- `OrderBook::top_levels` / `all_levels` reserve the expected output size in one allocation before
  copying, and same-price insert/update uses one map lookup.
- ADR-0005 remains accepted history. Its Spot/USD-M sequence rules are unchanged; only its
  copy-on-apply transaction decision is superseded here.

## Evidence

- M-P2.A implementation: `src/projection_state/book_projection.cpp`,
  `src/order_book/order_book.cpp`, `tests/CMakeLists.txt`.
- M5 Phase-7/P1 evidence: pre-optimization D5000 accepted apply measured 10,029 allocations and
  1,004,312 requested bytes per apply.
- Post-change Phase-7 measurement: D5000 existing-price B10/B100 = 0 allocations / 0 bytes;
  D0/B10 empty-book insertion = 10 allocations / 480 bytes (one node per new price level).
- Regression coverage: `AllocationInstrumentationAdversarial.M3AcceptedApplyMutatesInPlace` and
  `AllocationInstrumentationAdversarial.M3EmptyAcceptedBatchDoesNoBookWork`.
