# M5 Phase-7 Allocation and Memory Evidence

## Status

- Phase-7 methodology: **APPROVED / MERGED**
- Formal evidence: **ACCEPTED**
- Original decision record: OD-M5-P7-001 through OD-M5-P7-023
- Independent methodology review: APPROVED (P0: 0, P1: 0, P2: 0)
- Current production relevance: **HISTORICAL** for the M3 accepted-apply transaction

This file replaces a 2,429-line pre-implementation decision record. That original record remains
available in Git history at `bb4dfd6` and in the Phase-7 merge. It is not current production
semantics where it describes the M3 copy-on-apply transaction.

## Purpose

Phase 7 measured allocation and memory behavior through executable-local replacement of the global
`operator new` / `operator delete` family. Production code has no failpoints, counters, allocator
hooks, or benchmark-only APIs.

## Metrics

The instrumentation reports:

- allocation count and total requested bytes;
- deallocation count and total requested bytes where sized delete is available;
- persistent live-byte delta;
- peak live bytes above the measurement entry; and
- eligibility flags when provenance is incomplete or an unsupported allocation channel is observed.

A/P/B accounting separates allocations before the measured bracket, allocations inside it, and
pre-bracket allocations destroyed inside it. Owning outputs such as `all_levels` remain alive when
the operation returns and are destroyed outside the measurement bracket.

## Current M2/M3 behavior

The current order book matches the M5-era M2 semantics:

- `apply_level`: inserting a missing price allocates one map node; updating an existing price
  writes in place; deleting a present price frees one node; missing delete is a no-op.
- `apply_updates`: input-order last-write-wins, with allocation only for newly inserted prices.
- `replace_all`: builds fresh maps and commits transactionally; baseline installation keeps the
  strong exception guarantee.
- `all_levels` / `top_levels`: one reserved vector allocation sized to the requested output.

M5 measured the former M3 copy-on-apply accepted apply at 10,029 allocations and 1,004,312 requested
bytes per D5000 operation. M-P2.A removed that transaction under
[ADR-0010](adr/ADR-0010-in-place-accepted-apply.md). Current accepted apply mutates the live book in
place: an existing-price update or deletion allocates nothing, a new price level allocates one node,
and an empty accepted batch does no book work. Post-change measurement: D5000 existing-price
B10/B100 = 0 allocations / 0 bytes; D0/B10 = 10 allocations / 480 bytes.

M-P2.B separately reduced the inbound `AdaptDepthUpdate/Spot/10` adapter cell from 2 allocations /
480 bytes to 1 allocation / 240 bytes by appending bids and asks into one pre-reserved vector and
moving owned level vectors/metadata. M4 snapshot output now uses standard Protobuf setters rather
than manual `set_allocated_*` ownership assembly.

## Validation

The reusable instrumentation and adversarial tests remain in:

- `benchmarks/benchmark_support/allocation_instrumentation.*`;
- `benchmarks/benchmark_support/allocation_global_new.cpp`;
- `tests/m5/allocation/allocation_instrumentation_test.cpp`; and
- `tests/m5/allocation/instrumentation_test_*`.

Current M3 regressions include:

- `AllocationInstrumentationAdversarial.M3AcceptedApplyMutatesInPlace` — a batch with one new level
  performs one allocation and zero old-book deallocations; and
- `AllocationInstrumentationAdversarial.M3EmptyAcceptedBatchDoesNoBookWork` — an advancing empty
  batch performs zero allocations and zero deallocations.

The synthetic allocation-failure sweep executables and their full-book rollback checks were removed
with the copy-on-apply transaction. Deterministic semantics remain covered by the order-book,
projection-state, property, replay, and fuzz suites.

## Boundaries

- Instrumentation is confined to test/benchmark executables.
- Requested bytes are not RSS, allocator-arena footprint, or physical memory.
- Protobuf/Contracts may allocate through channels outside the replaceable global-new family;
  Phase 7 makes no completeness claim for those channels.
- Phase-7 timing and allocation evidence describes the code at its accepted evidence commit. Use
  current implementation and tests for present behavior.
