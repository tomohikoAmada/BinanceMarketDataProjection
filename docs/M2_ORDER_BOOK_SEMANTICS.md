# M2 Order Book Core Semantics

## Scope

M2 implements a deterministic, single-writer, market-by-price order book. It tracks the absolute
aggregate quantity at each price level for bids (descending) and asks (ascending). It does not
implement sequence validation, snapshot contracts, protobuf adaptation, networking, matching,
or any trading behavior.

## Contracts Baseline

Read-only semantic reference: `01d76a41929f36d89573159f5f458f9f1e378ada` (BinanceMarketDataContracts).

## Market-by-Price

The order book maintains `price -> absolute quantity` per side. Each price has at most one level
per side. M2 knows nothing about individual orders, order IDs, time priority, or matching.

## Absolute Quantity

Updates replace the quantity at a price level. Quantity is absolute, not delta. A level update
`{price: 100, quantity: 8}` means the total quantity at that price is now 8.

## Zero Deletes

A quantity of zero removes the price level from the book. Removing a non-existent level is a
deterministic no-op. The book never stores a level with quantity zero.

## Bid/Ask Ordering

Bids are stored in strictly descending price order. Asks are stored in strictly ascending price
order. No duplicate prices exist within a side.

## Duplicate Price

Within a single batch or replace-all operation, the last occurrence of a price wins. Previous
occurrences are treated as intermediate states.

## Batch Order

`apply_updates` applies updates in input order. Same-price updates within a batch: the last one
takes effect. The batch result is equivalent to sequential `apply_level` calls.

## Replace All

`replace_all` builds a temporary book state from input spans, then atomically replaces the
current state:

1. Bids and asks are processed independently.
2. Each side: inputs processed in order, last-write-wins for duplicate prices.
3. Zero quantities remove the level.
4. The replacement is transactional (strong exception guarantee): on allocation failure the
   current book is unchanged.
5. `replace_all` does not merge with existing state; it replaces it.

## Crossed and Locked Books

M2 accepts locked books (`best_bid == best_ask`) and crossed books (`best_bid > best_ask`).
Neither condition triggers rejection, auto-deletion, or matching. M2 preserves input state
faithfully.

## No Matching

M2 does not match orders, produce trades, or calculate derived prices (mid, spread, microprice).

## NumericSpec Binding

Each `OrderBook` is constructed with one `NumericSpec`. The caller is responsible for supplying
`PriceUnits` and `QuantityUnits` consistent with the book's `price_scale` and `quantity_scale`.
The book does not store scale per-level, alter scales implicitly, or mix levels with different
specs.

## Top-N

`top_levels` returns at most `limit` levels from the requested side. The internal book retains
all applied levels regardless of the query limit. Top-N only truncates output, never internal
state.

## Internal Depth

The book stores all applied levels. No depth limit is enforced internally.

## PIMPL

Storage (`std::map`) is hidden behind a compiler firewall (`unique_ptr<Impl>`). Public headers
do not expose `<map>`, `std::map`, or any container type.

## Container Baseline

`std::map` is the M2 correctness baseline. M5 will evaluate alternative containers with
representative benchmarks. No third-party container is added in M2.

## Query Allocation

`top_levels` and `all_levels` return `std::vector<BookLevel>` by value. Each call allocates.
Callers should not call these in hot paths when best bid/ask or quantity-at-price suffices.

## No Sequence

M2 does not store `first_update_id`, `final_update_id`, `last_update_id`, or any sequence state.
Sequence continuity and gap detection belong to M3.

## No Snapshot Metadata

M2 does not store snapshot IDs, timestamps, or exchange venue metadata. These belong to M4.

## No Protobuf

M2 depends only on C++20 standard library and project numeric types. No protobuf is linked or
included.

## No System Time

M2 reads no system clock, random device, or ambient state.
