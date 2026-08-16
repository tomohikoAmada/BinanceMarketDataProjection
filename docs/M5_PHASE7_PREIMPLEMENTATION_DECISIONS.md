# M5 Phase-7 Pre-Implementation Decisions

## Status

- Status: **PRE-IMPLEMENTATION DECISIONS PROPOSED — PENDING INDEPENDENT METHODOLOGY REVIEW**
- Decision-review baseline (immutable): `eed3de99efaba8eaa96083a5348d538ed44f6bfe`
  (live `main` at the time of this record; the squash merge of PR #24,
  `docs: record INFRA-TC-001 acceptance and merge`)
- Phase 6: **COMPLETE / MERGED** (PR #21, squash merge
  `227524e6d17cce77813c6f26cd65bb8d996f5677`)
- INFRA-TC-001: **COMPLETE / ACCEPTED / MERGED** (PR #23, squash merge
  `24fb72232e928290add45ed8634cd0bf9a8d3442`)
- M5 Phase 7 (allocation/memory): **AUTHORIZED / NOT STARTED**
- M5 Phase 8 (container spike): **NOT STARTED**
- Implementation authorization: **NO** (until this exact decision record is
  independently reviewed and merged without material unreviewed decision changes)

This document is the normative pre-implementation measurement contract for M5 Phase 7
(allocation and memory characterization). It records accepted design authority, the
independent decision lock, implementation constraints, and the future Phase-8/10
boundaries. It is NOT implementation evidence: no allocation instrumentation code
exists or is claimed here.

**This document does NOT implement Phase 7.** Phase 7 remains NOT STARTED in this PR.

## Authorization wording

- While this docs-only PR is open: Phase-7 implementation authorization is **NO**.
- After this exact decision record is independently reviewed and merged without
  material unreviewed decision changes: Phase-7 implementation authorization becomes
  **YES**.
- Implementation authorization becomes YES only after all blocking methodology
  findings raised in review are CLOSED.

## Purpose

Phase 6 established representative performance workloads and timing measurement.
Phase 7 must establish truthful allocation and memory characterization of the
CURRENT production M1-M4 implementation BEFORE any alternative order-book
container is benchmarked in Phase 8.

The central architecture question is NOT "Is std::map expensive?". The central
question is:

```text
Where do the current production allocation and memory costs actually come from?
```

In particular, Phase 7 must distinguish:

```text
A. M2 std::map storage/allocation cost;
B. M3 BookProjection accepted-apply transactional architecture, which currently
   performs logical copy-on-apply of the complete book;
C. temporary vectors / candidate rebuild / commit destruction;
D. M4 adaptation / snapshot-output allocation;
E. end-to-end replay allocation behavior.
```

Phase 7 must prevent Phase 8 from incorrectly blaming the container for costs that
are actually dominated by the BookProjection transaction architecture.

Phase 7 is **characterization, not optimization**. It changes no production code,
no production container, no production semantics, and no production public API.
`std::map` remains the production correctness baseline throughout Phase 7.

## Source-of-truth precedence

1. Accepted repository ADRs / semantic authority (ADR-0003, ADR-0005 with its
   ADR-0008 Spot-bootstrap supersession, ADR-0006, ADR-0007, ADR-0008).
2. Accepted M5 design (`docs/M5_DIFFERENTIAL_VALIDATION_AND_PERFORMANCE_DESIGN.md`).
3. Accepted Phase-6 decision authority (`docs/M5_PHASE6_PREIMPLEMENTATION_DECISIONS.md`,
   `docs/M5_PHASE6_REPRESENTATIVE_BENCHMARKS.md`).
4. Exact production implementation and tests on immutable main
   `eed3de99efaba8eaa96083a5348d538ed44f6bfe`.
5. Phase-6 benchmark implementation and formal measurement contracts.
6. Status/orientation documents (`docs/CURRENT_STATE.md`, this record).
7. PR descriptions / old handoff reports.

## Established implementation facts (inspected on the immutable base)

These facts were verified against the exact checked-out code at
`eed3de99efaba8eaa96083a5348d538ed44f6bfe`. Phase-7 measurement must be designed
against these facts, not against documentation alone.

### M2 storage and mutation (src/order_book/order_book.cpp)

- `OrderBook` is a PIMPL: `std::unique_ptr<Impl>` allocated by
  `std::make_unique<Impl>(numeric_spec)` (`order_book.cpp:200`).
- Storage is `BidMap = std::map<PriceUnits, QuantityUnits, std::greater<>>` and
  `AskMap = std::map<PriceUnits, QuantityUnits, std::less<>>` (`order_book.cpp:10-11`).
- `apply_level`:
  - zero quantity → `apply_remove` (`order_book.cpp:133-148`): node erase; a present
    node is freed (one deallocation, zero allocations); a missing node is a no-op.
  - non-zero quantity → `apply_insert_or_update` (`order_book.cpp:150-174`): missing
    price → one map-node allocation (`Inserted`); present price with different
    quantity → in-place value write, zero allocations (`Updated`); equal quantity →
    zero allocations (`Unchanged`).
- `replace_all` (`order_book.cpp:84-106`): builds fresh temporary `BidMap`/`AskMap`
  (node allocations per surviving level), then move-assigns over the live maps
  (destroys the previous maps' nodes — deallocations). Strong exception guarantee.
- `all_levels` / `top_levels` (`order_book.cpp:121-130, 176-193`): return
  `std::vector<BookLevel>` by value; `copy_from_map` push_backs without reserve, so
  each query allocates (and may grow) a vector buffer. Every call allocates
  (M2_ORDER_BOOK_SEMANTICS.md documents this).
- `best_bid`/`best_ask`/`quantity_at` (`order_book.cpp:38-68`): pure map reads;
  zero allocations.

### M3 accepted-apply transaction (src/projection_state/book_projection.cpp)

`BookProjection::Impl::apply_transaction` (`book_projection.cpp:198-205`) is the
established production accepted-apply path:

```text
1. const auto bids  = book_.all_levels(BookSide::Bid);   // full-side vector copy
2. const auto asks  = book_.all_levels(BookSide::Ask);   // full-side vector copy
3. OrderBook candidate{numeric_spec_};                   // make_unique<Impl>
4. candidate.replace_all(bids, asks);                    // fresh maps + ~2D nodes
5. candidate.apply_updates(levels);                      // per-batch node traffic
6. book_ = std::move(candidate);                         // noexcept; frees old maps
```

- Steps 1-2: two full-side vector allocations (buffer growth during copy).
- Step 3: one allocation (candidate `Impl` via `make_unique`).
- Step 4: fresh maps; node allocations for every surviving level on both sides
  (approximately 2 × depth), then old-vector storage of `bids`/`asks` remains in
  scope but the maps are new.
- Step 5: node allocations for inserted prices, node frees for deleted prices,
  value writes for existing prices — exactly M2 `apply_updates` semantics.
- Step 6: `noexcept` move assignment destroys the old book's two maps (frees
  approximately 2 × depth nodes) and releases the old vectors from step 1-2
  (their deallocation is outside the moved-into book but inside the operation).
- `BookProjection` itself is a PIMPL (`book_projection.cpp:232-233`):
  construction performs one `make_unique<Impl>` allocation.
- Accepted apply is O(book depth) in allocation and deallocation traffic for
  EVERY accepted batch, including a batch with zero levels (mandatory matrix
  cell B=0, OD-M5-P6-007): steps 1-4 and 6 run regardless of the batch content.
- `record_gap` (`book_projection.cpp:207-214`) is `noexcept` and performs no
  allocation; stale/duplicate/wrong-state classification paths perform no
  allocation.
- `install_baseline` (`book_projection.cpp:127-136`) delegates to M2
  `replace_all`: fresh maps plus node traffic, then old-map destruction.
- `reset()` (`book_projection.cpp:164-169`) is `noexcept`; it performs
  deallocations only (clears both maps).

### M4 adapter (src/proto_adapter/proto_adapter.cpp)

- `adapt_exchange_depth_snapshot` / `adapt_depth_update` are stateless free
  functions returning owning `AdaptedBookBaseline` / `AdaptedDepthBatch`
  values: per-level vector allocations plus binding metadata and the owning
  quality sidecar.
- `install_into` / `apply_to` are binding-checked synchronous M3 invocations:
  allocation cost is the underlying M3 transaction plus any small temporary.
- `make_local_order_book_snapshot` builds an owning Contracts
  `LocalOrderBookSnapshot` Protobuf message: Protobuf runtime allocation plus
  decimal-formatting string allocations.
- `SerializeToString` allocates a serialization buffer (`std::string`).
- Protobuf wire-message construction is documented as setup only
  (OD-M5-P6-011) and never a measured production path.

### Allocation-failure executable isolation pattern (established, reusable)

- `tests/projection_state/book_projection_allocation_failure_test.cpp:58-112` and
  `tests/proto_adapter/proto_adapter_allocation_failure_test.cpp:59-100` define
  global `operator new` / `operator new[]` / nothrow forms / `operator delete` /
  `operator delete[]` / sized delete / nothrow delete overrides in dedicated
  single-threaded test executables
  (`bmd_projection_m3_allocation_failure_tests`,
  `bmd_projection_proto_adapter_allocation_failure_tests`;
  `tests/CMakeLists.txt:227-244, 372-389`).
- The control block is `thread_local` (`enabled`, `count`, `fail_at`), the
  fail-after-N counter increments only inside an explicit `Scope`, zero-size
  requests are normalized to 1 byte, and the underlying allocator is
  `std::malloc` / `std::free`.
- `tests/proto_adapter/proto_adapter_allocation_failure_test.cpp:272-284`
  (`measure_allocations`) already demonstrates deterministic allocation-count
  measurement of a production invocation through this pattern.
- Phase 7 generalizes this established pattern: the same dedicated-executable
  isolation, the same global-operator surface, but counting/bytes/live tracking
  instead of fail-after-N.

### Phase-6 measurement infrastructure (reused, not redefined)

- Workload identity: `M5_BENCHMARK_WORKLOAD_SPEC_V1` (canonical key-sorted
  workload-spec text + SHA-256, static registry;
  `benchmarks/benchmark_support/workload_spec.*`).
- Metadata wrapper: `M5_BENCHMARK_WRAPPER_V1` (provenance/build/environment/
  workload/measurement/result-payload sections;
  `benchmarks/benchmark_support/wrapper.*`). It contains NO allocation fields.
- Latency schema: `M5_REPLAY_LATENCY_V1`.
- M3 cells: `M5_PHASE6_M3_CELLS_V1` generator schema with deterministic
  generated-workload SHA-256s (`benchmarks/benchmark_support/m3_cells.*`).
- M2 cells: `benchmarks/benchmark_support/m2_cells.*`; prepared-state pools:
  `benchmarks/benchmark_support/book_state.hpp`.
- Replay workloads: small Spot/USD-M fixtures (generator
  `m5-small-generator-v1`, seed `548746690337`, 2,048 events, canonical
  replay-log SHA-256) shared with Phase 3/6.
- Configure-time build identity generation:
  `cmake/M5BenchmarkSupport.cmake` (`bmd_projection_generate_benchmark_build_identity`).
- Fail-closed Python validators: `scripts/benchmark_phase6.py`; deterministic
  validator tests: `tests/m5/benchmark/benchmark_support_test.cpp`,
  `tests/m5/benchmark/test_phase6_validators.py`.

## Normative Decision Table

All decisions below are:

- Status: **PROPOSED — PENDING INDEPENDENT METHODOLOGY REVIEW**

| ID | Decision | Status |
|---|---|---|
| OD-M5-P7-001 | Measurement purpose / attribution model | PROPOSED |
| OD-M5-P7-002 | Instrumentation isolation | PROPOSED |
| OD-M5-P7-003 | Complete allocation operator surface | PROPOSED |
| OD-M5-P7-004 | Mandatory allocation metrics | PROPOSED |
| OD-M5-P7-005 | Live bytes / peak live bytes semantics | PROPOSED |
| OD-M5-P7-006 | Persistent footprint semantics | PROPOSED |
| OD-M5-P7-007 | Baseline subtraction | PROPOSED |
| OD-M5-P7-008 | M2 workload coverage | PROPOSED |
| OD-M5-P7-009 | M3 accepted-apply matrix | PROPOSED |
| OD-M5-P7-010 | M3 attribution / decomposition | PROPOSED |
| OD-M5-P7-011 | Baseline install allocation | PROPOSED |
| OD-M5-P7-012 | M4 allocation coverage | PROPOSED |
| OD-M5-P7-013 | Replay allocation workload | PROPOSED |
| OD-M5-P7-014 | Warmup and one-time runtime effects | PROPOSED |
| OD-M5-P7-015 | Repetition / determinism | PROPOSED |
| OD-M5-P7-016 | Machine-readable evidence schema | PROPOSED |
| OD-M5-P7-017 | Formal evidence eligibility | PROPOSED |
| OD-M5-P7-018 | Toolchain/environment interpretation | PROPOSED |
| OD-M5-P7-019 | Instrumentation overflow / failure behavior | PROPOSED |
| OD-M5-P7-020 | Adversarial validation | PROPOSED |
| OD-M5-P7-021 | Phase-8 handoff | PROPOSED |
| OD-M5-P7-022 | Phase boundary / CI ownership | PROPOSED |

---

## OD-M5-P7-001 — Measurement purpose / attribution model

**Question:** What exactly is Phase 7 trying to attribute, and what is it NOT?

**Decision:**

Phase 7 is characterization, not optimization. It measures the current
production implementation at the immutable base
`eed3de99efaba8eaa96083a5348d538ed44f6bfe` and attributes allocation/memory
cost to exactly these buckets:

```text
A. PERSISTENT ORDER-BOOK STORAGE
   The requested heap bytes held by a populated M2 OrderBook (std::map nodes),
   measured by the footprint experiment (OD-M5-P7-006).

B. PER-OPERATION ALLOCATION TRAFFIC (M2)
   Allocation count / bytes / deallocations for one logical M2 mutation or
   query operation (OD-M5-P7-008).

C. TRANSIENT TRANSACTIONAL ALLOCATION (M3)
   Allocation count / bytes / live bytes / deallocations for one accepted
   BookProjection::apply transaction and one baseline install
   (OD-M5-P7-009, OD-M5-P7-011), including candidate reconstruction and
   old-state destruction.

D. OUTPUT / ADAPTATION ALLOCATION (M4)
   Allocation count / bytes for adaptation, checked install/apply, snapshot
   construction, and serialization (OD-M5-P7-012).

E. END-TO-END REPLAY ALLOCATION
   Aggregate and per-event allocation count / bytes for production
   CoreNormalizedReplay and AdapterWireReplay (OD-M5-P7-013).

F. BENCHMARK / INSTRUMENTATION OVERHEAD
   Harness-owned input preparation, pool construction, and the
   instrumentation's own bookkeeping. Excluded from every production bucket
   by bracket discipline (OD-M5-P7-007) and reported only as separately
   labelled calibration evidence; never subtracted from production metrics.
```

Every result names its bucket. A result that cannot be attributed to one
bucket is exploratory or invalid, never formal.

**Rationale:** The Phase-8 container decision must be able to see how much of
the observed cost is persistent container storage versus transaction
architecture versus output machinery. Attribution is the entire purpose of
Phase 7.

**Implementation constraint:** No production API, no production source hook,
no Core change. No optimization of any measured path. Report language is
limited to "On environment X, exact source/binary Y, workload Z, measured
result R".

---

## OD-M5-P7-002 — Instrumentation isolation

**Question:** Where does instrumentation live?

**Decision:**

- Instrumentation lives in dedicated single-threaded measurement executables
  only, following the established allocation-failure-executable isolation
  pattern (`tests/CMakeLists.txt:227-244, 372-389`). Each Phase-7 measurement
  executable owns its global `operator new`/`operator delete` overrides in its
  own translation unit; the overrides are never exported to other targets.
- The shared instrumentation logic is a benchmark/test-only header
  (proposed: `benchmarks/benchmark_support/allocation_instrumentation.hpp`)
  included only by Phase-7 measurement/test executables. It is never installed,
  never linked into `BinanceMarketDataProjection::Core` or `::ProtoAdapter`,
  and never visible to production consumers.
- No production public API is added merely for measurement. No benchmark-only
  hook enters Core. PIMPL is not weakened.
- Planned measurement executables (design only; not created by this PR):
  - `bmd_projection_allocation_m2_m3` — M2 and M3 allocation cells;
  - `bmd_projection_allocation_m4` — M4 allocation cells (built only with
    `BMD_PROJECTION_BUILD_PROTO_ADAPTER=ON`, mirroring the Phase-6 M4
    fail-closed availability rule);
  - `bmd_projection_allocation_replay` — replay allocation workloads;
  - `bmd_projection_allocation_footprint` — persistent footprint experiment;
  - `bmd_projection_allocation_instrumentation_tests` — adversarial
    instrumentation validation (OD-M5-P7-020).

**Why process-global instrumentation is safe enough here:** process-global
operator overrides observe every allocation in the process, including the
harness's. They are acceptable ONLY because:

1. each measurement executable is single-threaded (the only threads are the
   main thread; no TSan-style background machinery is present in these
   executables), so the control block needs no synchronization and
   bracket-scoped counters cannot be polluted by concurrent work;
2. brackets are entered/left only on the main thread and only in the
   measurement loop;
3. all harness allocations (input preparation, pools, output buffers) are
   performed OUTSIDE measured brackets (OD-M5-P7-007), so global visibility
   does not contaminate bracket-scoped deltas;
4. the instrumentation itself performs no allocation inside a measured
   bracket (preallocated counters/table, OD-M5-P7-019);
5. a recursion guard routes instrumentation-internal allocations around the
   counting path (OD-M5-P7-019).

Instrumentation is therefore NOT safe in multi-threaded executables, in
production, or in the Google Benchmark timing binary; all of those are out of
scope for Phase 7.

**Rationale:** This is the exact isolation model already reviewed and used for
M3/M4 allocation-failure sweeps, extended from counting to counting/bytes/live
tracking.

---

## OD-M5-P7-003 — Complete allocation operator surface

**Question:** Which C++ allocation/deallocation forms must an instrumentation
executable handle?

**Decision:**

Every Phase-7 instrumentation executable MUST override, in its own executable
(never in a library):

```text
operator new(std::size_t)
operator new[](std::size_t)
operator new(std::size_t, const std::nothrow_t&) noexcept
operator new[](std::size_t, const std::nothrow_t&) noexcept
operator delete(void*) noexcept
operator delete[](void*) noexcept
operator delete(void*, std::size_t) noexcept
operator delete[](void*, std::size_t) noexcept
operator delete(void*, const std::nothrow_t&) noexcept
operator delete[](void*, const std::nothrow_t&) noexcept
operator new(std::size_t, std::align_val_t)
operator new[](std::size_t, std::align_val_t)
operator new(std::size_t, std::align_val_t, const std::nothrow_t&) noexcept
operator new[](std::size_t, std::align_val_t, const std::nothrow_t&) noexcept
operator delete(void*, std::align_val_t) noexcept
operator delete[](void*, std::align_val_t) noexcept
operator delete(void*, std::align_val_t, std::size_t) noexcept
operator delete[](void*, std::align_val_t, std::size_t) noexcept
operator delete(void*, std::align_val_t, const std::nothrow_t&) noexcept
operator delete[](void*, std::align_val_t, const std::nothrow_t&) noexcept
```

Rules:

- **Size recovery is never trusted to sized delete alone.** The requested size
  is recorded at allocation time in the instrumentation's pointer→size table;
  sized delete receives a size parameter that is used ONLY as a consistency
  check (mismatch → sticky instrumentation error, OD-M5-P7-019). Unsized and
  sized deletes both recover sizes from the table.
- **Aligned allocation:** the aligned overloads allocate
  `requested_size + align + sizeof(void*)` bytes via `std::malloc`, align the
  payload, and store the raw pointer immediately before the payload. This is
  well-defined and satisfies any alignment. Only `requested_size` (the value
  passed by the caller) is counted; the alignment padding is allocator
  overhead and is excluded from byte metrics but documented as such.
- **Underlying allocator:** `std::malloc` / `std::free` (the established
  pattern). Zero-size requests are normalized to 1 byte before malloc.
- **No undefined behavior, no broken alignment:** every returned pointer
  satisfies the requested alignment; the aligned path is covered by adversarial
  tests (OD-M5-P7-020).
- **No recursive instrumentation allocation:** the recording path performs no
  allocation; the pointer→size table is preallocated (via the overridden
  operator with the recursion guard active) before any measured bracket.
- **Fail-closed strategy:** if any allocation form is discovered to escape
  the overridden surface (an unknown-pointer delete, a table overflow, or a
  form not overridden being emitted), the affected metric class is marked
  INVALID/INELIGIBLE for the run (OD-M5-P7-005, OD-M5-P7-019). Never silently
  estimate.
- The surface must be verified for each measured target/toolchain
  combination by the adversarial validation suite (OD-M5-P7-020), including
  at least one over-aligned type per target.

**Rationale:** C++17 aligned new can be emitted by containers and the Protobuf
runtime on the canonical toolchains (Clang 18.1.3 / GCC / AppleClang). Sized
delete is not guaranteed to be emitted for every deallocation, so it cannot be
the source of truth for live-byte accounting.

---

## OD-M5-P7-004 — Mandatory allocation metrics

**Question:** What do `allocation_count` and `total_allocated_bytes` mean
exactly?

**Decision (exact semantics):**

- `allocation_count`: the number of successful allocation requests observed
  inside the measured bracket through any overridden allocation operator.
  - A request is counted exactly once, at the moment it succeeds.
  - An allocation that fails (underlying malloc returns null → bad_alloc
    propagates, or nothrow form returns null) is NOT counted; the enclosing
    cell is marked ERROR (OD-M5-P7-019).
  - A zero-size request counts as exactly 1 allocation.
- `total_allocated_bytes`: the sum, over the same counted requests, of the
  REQUESTED sizes as passed to the allocation operator.
  - Zero-size requests contribute 1 byte each (normalization matching the
    established allocation-failure pattern).
  - Alignment padding, allocator chunk headers, and malloc internal overhead
    are NOT included. These are allocator overhead, reported separately as a
    modeled quantity only in footprint records (OD-M5-P7-006).
- `deallocation_count`: the number of successful deallocation calls observed
  inside the bracket through any overridden delete operator.
- `deallocated_bytes`: the sum of the recorded requested sizes of pointers
  freed inside the bracket. Available only when every freed pointer is present
  in the pointer→size table; an unknown-pointer delete makes the
  deallocated-bytes (and live/peak) metric class INVALID for the bracket
  while `allocation_count`/`total_allocated_bytes` remain valid
  (OD-M5-P7-019).
- **Denominator:** every cell reports its operation denominator exactly: one
  logical production operation (one `apply_level`, one `apply_updates` call,
  one `replace_all`, one query, one `BookProjection::apply`, one
  `install_baseline`, one adaptation call, one checked install/apply, one
  snapshot construction, one serialization, or one replay event). Aggregate
  replay totals are reported together with `event_count` so per-event values
  are derived as exact integer division of the aggregate (OD-M5-P7-013).
- **Setup/warmup exclusion:** allocations performed before the bracket opens
  (input preparation, pools, one full untimed warmup pass) are never counted
  (OD-M5-P7-007, OD-M5-P7-014).
- **Reporting:** count and bytes are reported as exact values plus the number
  of confirming repetitions (OD-M5-P7-015). No statistical distribution is
  claimed for count/bytes.
- Mandatory metric set for every Phase-7 cell:
  `allocation_count`, `total_allocated_bytes`, and — for eligible cells —
  `deallocation_count`, `deallocated_bytes`, `live_bytes_after_bracket`,
  `peak_live_bytes` with its eligibility flag (OD-M5-P7-005).

**Rationale:** These two metrics are the existing M5-design mandatory metrics
("allocation count and total allocated bytes are the mandatory metrics",
M5 design, Allocation metrics). Exactness here prevents Phase 8 from
misreading partial counts.

---

## OD-M5-P7-005 — Live bytes / peak live bytes semantics

**Question:** How are live bytes and peak live bytes measured, and when are
they truthful?

**Decision:**

- `live_bytes` = sum of requested sizes of all currently outstanding
  allocations tracked in the pointer→size table. Updated exactly at every
  successful allocation (+requested size) and every successful tracked
  deallocation (−recorded size) inside the bracket.
- `peak_live_bytes` = the maximum value of `live_bytes` over the bracket,
  computed event-driven at every allocation and deallocation transition
  inside the bracket. There is NO sampling and NO interpolation.
- **Size recovery:** unsized deallocation is tracked safely through the
  pointer→size table; sized delete is a consistency check only
  (OD-M5-P7-003). No dependence on sized delete for size recovery.
- **Aligned allocations:** tracked like ordinary allocations (the recorded
  size is the caller's requested size; padding excluded).
- **Eligibility (fail-closed, per metric class):** `peak_live_bytes` and
  `live_bytes_after_bracket` are ELIGIBLE for a run only when ALL of:
  1. every operator form in OD-M5-P7-003 is overridden in the executable;
  2. the pointer→size table never overflows during the measured bracket
     (table sized for the observed maximum concurrency; overflow → INELIGIBLE);
  3. no unknown-pointer deallocation occurs inside the bracket
     (→ INELIGIBLE);
  4. no allocation form is emitted that was not overridden (proven per target
     by the adversarial suite, OD-M5-P7-020);
  5. `live_bytes` returns exactly to the pre-bracket baseline value at
     bracket close (deallocation-return-to-baseline proof). A non-returning
     bracket → INELIGIBLE with reason, and the cell is marked ERROR for
     live metrics (count/bytes may remain valid).
- **M4/Protobuf special rule:** for cells that execute the Protobuf runtime
  (`make_local_order_book_snapshot`, `SerializeToString`, and any replay
  cell with adapter machinery), live/peak metrics are ELIGIBLE only after a
  deterministic adversarial test proves that the pinned Protobuf runtime
  path under measurement routes its heap traffic through the overridden
  operators (observed: every tracked allocation is freed through a tracked
  delete, no unknown frees, baseline return). If that proof is not achieved,
  M4 live/peak metrics are reported INELIGIBLE with the recorded reason while
  `allocation_count`/`total_allocated_bytes` remain eligible.
- **Platforms:** peak-live is eligible on the three supported CI toolchains
  (Ubuntu GCC, Ubuntu Clang, macOS AppleClang) subject to the rules above.
  Values are environment-specific (OD-M5-P7-018). MSVC is not a CI target
  and makes no eligibility claim.
- **Unavailability:** when a metric is INELIGIBLE/UNAVAILABLE it is reported
  as `ineligible` with a machine-readable reason code. It is NEVER
  substituted with an estimate. There is no "estimated peak live bytes"
  metric.

**Rationale:** Peak-live evidence fabricated from incomplete deallocation
data would poison the Phase-8 decision. Truthful absence is preferred over
plausible numbers.

---

## OD-M5-P7-006 — Persistent footprint semantics

**Question:** How is the memory-footprint experiment for order-book depths
100 / 1,000 / 5,000 / 10,000 levels per side defined and reported?

**Decision:**

- **Executable:** `bmd_projection_allocation_footprint`, single-threaded,
  with the full operator surface (OD-M5-P7-003).
- **Cells:** depths per side D ∈ {100, 1000, 5000, 10000}. For each D:
  1. after warmup, record snapshot `S_empty` = live_bytes with an empty
     `OrderBook` constructed with the fixed `NumericSpec` (this bracket also
     yields the fixed object/PIMPL footprint delta);
  2. populate bids only to D levels (deterministic price/quantity generator,
     same generator schema as Phase-6 M2 cells), snapshot `S_bids`; delta
     `S_bids − S_empty` = requested heap bytes for one side at D;
  3. populate asks to D levels, snapshot `S_both`; delta
     `S_both − S_bids` = requested heap bytes for the second side at D;
  4. destroy the book and assert live_bytes returns to the pre-experiment
     baseline (deallocation-return proof; failure → record INELIGIBLE).
- **Reported per cell:**
  - `measured_requested_heap_bytes_total` = `S_both − S_empty`;
  - `measured_requested_heap_bytes_per_side_bids`,
    `measured_requested_heap_bytes_per_side_asks` (measured independently,
    never assumed equal);
  - `measured_bytes_per_level_per_side` = per-side delta / D, reported as an
    exact rational pair (numerator, denominator D) plus decimal rendering
    with full precision — no rounding into a lossy float;
  - `fixed_object_footprint_bytes` = delta from "no book" to "empty book"
    (construction of `unique_ptr<Impl>` + empty maps), measured once per
    executable and reported once;
  - `empty_book_baseline_bytes` = `S_empty` relative to no-book baseline;
  - `allocator_instrumentation_baseline`: the instrumentation's own
    bookkeeping overhead, measured by a separate empty-bracket calibration
    record, reported separately and NEVER subtracted from the deltas.
- **Three explicitly separated quantities (never mixed):**
  1. **MEASURED** requested heap bytes (the deltas above — what Phase 7
     asserts);
  2. **MODELED** allocator/node overhead: a documented static model per
     toolchain, e.g., libstdc++/libc++ red-black tree node =
     pointers/key/value/padding (approximately 48 bytes on 64-bit with
     8-byte key and value) plus a stated malloc chunk-header estimate (e.g.,
     16 bytes glibc). Reported in a dedicated `modeled_overhead` section with
     the exact formula, constants, and toolchain identity. Modeled numbers
     are explanatory only and may not be quoted as measured values.
  3. **OS RSS / physical memory: NOT MEASURED.** Phase 7 performs no
     `/proc`-style RSS reads and makes no RSS claim. RSS is explicitly out of
     Phase-7 evidence scope.
- **Population method:** populate via `replace_all` from prepared
  harness-owned vectors (built outside the bracket), so no intermediate
  mutation traffic pollutes the persistent delta.
- **Container note:** the experiment measures the production `std::map`
  implementation. The modeled node estimate exists so Phase 8 can compare
  candidate containers against the same experiment; Phase 7 changes nothing.

**Rationale:** Bytes-per-level must come from a real measured delta, with the
allocator overhead identified as a model, so Phase 8 can neither
double-count nor ignore allocator overhead.

---

## OD-M5-P7-007 — Baseline subtraction

**Question:** What exactly does "baseline subtraction" mean?

**Decision:**

- Every reported delta is specified as:

  ```text
  snapshot A          := live_bytes recorded at a defined stable point
  operation/object lifetime := the bracket between A and B
  snapshot B          := live_bytes recorded at the closing stable point
  delta formula       := B − A (unsigned, exact; negative delta → instrument
                         error, record INVALID)
  ```

- Snapshots are reads of thread_local counters; taking a snapshot performs
  no allocation and cannot corrupt the delta.
- Stable points: A is recorded immediately before the measured production
  operation (all harness input/pool/output-buffer allocation already done,
  pools referenced but not constructed inside the bracket); B is recorded
  immediately after the operation returns and its result has been consumed
  by the harness in a way that performs no allocation (result consumption
  discipline mirroring Phase-6: scalar/evidence fold only; if a harness
  must allocate to consume a result, the consumption is moved outside the
  bracket and B is taken before it).
- The instrumentation must not allocate inside the bracket: counters are
  preallocated PODs; the pointer→size table is preallocated outside every
  bracket.
- **Warmup:** each executable performs one full untimed workload-equivalent
  warmup pass before any snapshot (OD-M5-P7-014) so one-time runtime/library
  initialization allocations are absorbed and never appear in any delta.
- **No unrelated subtraction:** the delta never subtracts RSS, never
  subtracts another process's memory, never subtracts the calibration
  record. The calibration record (empty-bracket distribution) is reported
  beside the results, never subtracted from them (mirroring the Phase-6
  latency calibration non-subtraction rule, OD-M5-P6-017).
- **Stateful-operation drift prevention:** for state-mutating cells, every
  measured execution starts from a freshly prepared identical state drawn
  from a bounded pool built entirely outside the bracket; pool construction
  and destruction occur outside every bracket and their allocations never
  enter a delta. Repeated executions therefore measure the same semantic
  operation; a stateful drift (e.g., insert becoming update) fails the cell
  closed exactly as in Phase 6 (OD-M5-P6-004).

**Rationale:** Without named A/B and a formula, "subtract baseline" is
meaningless; this rule makes every delta auditable.

---

## OD-M5-P7-008 — M2 workload coverage

**Question:** Which M2 operations are measured for allocation?

**Decision (inventory):**

```text
M2/apply_level/insert/{8,100,1000}        (disposition Inserted every execution)
M2/apply_level/update/{8,100,1000}        (disposition Updated every execution)
M2/apply_level/delete/{8,100,1000}        (disposition Removed every execution)
M2/apply_level/missing_delete/{8,100,1000}(disposition Unchanged every execution)
M2/apply_updates/{1,10,100}/{8,100,1000}  (replacement-heavy operation mix,
                                           same mix identity as Phase 6)
M2/replace_all/{0,8,100,1000,5000,10000}  (post-state exactly canonical)
M2/all_levels/{0,8,100,1000,5000,10000}
M2/top_levels/{1,5,50}/{8,100,1000}
M2/best_bid/{8,100,1000}                  (zero-allocation control cell)
M2/best_ask/{8,100,1000}                  (zero-allocation control cell)
M2/quantity_at/hit/{8,100,1000}           (zero-allocation control cell)
M2/quantity_at/miss/{8,100,1000}          (zero-allocation control cell)
```

Workload identity and semantics reuse the accepted Phase-6 M2 cells
(`M5_PHASE6_M2_*` generator schemas and hashes) wherever a Phase-6 cell
exists; allocation cells run with fixed iteration counts (no `MinTime`),
using the same prepared-state pool discipline as the Phase-6 pooled cells.

**Documented justifications for coverage choices:**

- `apply_level` families are limited to routine depths {8,100,1000} because
  the allocation count per operation is depth-independent for the
  production implementation (insert = 1 node allocation; update = 0;
  delete = 0 allocations + 1 node free; missing delete = 0; all verified by
  the zero/one-allocation control assertions in the cells). The depth
  dependence of persistent storage is covered by OD-M5-P7-006 and by
  `replace_all`/`all_levels` at full depths. No apply_level cell at
  5000/10000 is therefore required.
- `best_bid`/`best_ask`/`quantity_at` cells exist ONLY as zero-allocation
  controls: they prove the instrumentation reports exactly zero where the
  production code provably allocates nothing (order_book.cpp:38-68). This is
  the strongest available ground-truth calibration of the counting
  mechanism; a non-zero result fails the cell closed.
- `replace_all` at D=0 is the empty-replacement edge (valid per Phase-6
  precedent; OD-M5-P6-005 allows D=0 only where semantically valid).

Every omission above is justified; there is no unmeasured M2 mutation
family in the Phase-6 semantic inventory.

**Rationale:** A smaller allocation-specific inventory is methodologically
sufficient and cheaper than re-measuring every Phase-6 timing cell, but the
zero-allocation control cells are mandatory to keep the mechanism honest.

---

## OD-M5-P7-009 — M3 accepted-apply matrix

**Question:** What M3 allocation coverage is required?

**Decision:**

- **Full 48-cell matrix** for accepted live apply:
  `M3/LiveApply/Accepted/{Spot,UsdMPerpetual}/D{0,8,100,1000,5000,10000}/B{0,1,10,100}`,
  reusing the exact Phase-6 cell names, cell semantics, prepared-state
  rules (each cell begins `Synchronized`; D>0 uses existing-price quantity
  updates; B=0 is the mandatory advancing empty-level batch; D=0/B>0 is the
  labelled empty-book insertion edge), generator schema
  `M5_PHASE6_M3_CELLS_V1`, and generated-workload SHA-256 identities.
- Allocation cells run with fixed iteration counts (pooled-state cells) and
  no `MinTime`; count/bytes are expected to be deterministic per cell
  (OD-M5-P7-015). The full 48 cells are therefore affordable and are the
  mandatory inventory (mirroring OD-M5-P6-028's completeness requirement).
- **Why the full matrix:** B=0 is the mandatory evidence that the accepted
  transaction allocates on the order of the full book even with an empty
  batch (steps 1-4 and 6 of `apply_transaction` run regardless of batch
  content). The D axis across {0,8,100,1000,5000,10000} at B=0 directly
  measures whether transaction cost is O(book depth); the B axis separates
  batch-size-driven traffic from depth-driven traffic. A reduced subset
  cannot answer the container-versus-transaction question truthfully, so no
  subset is permitted for formal evidence.
- **Classification cells** (allocation inventory): reuse the Phase-6
  `M3/Classification/{Stale,Duplicate,Gap,Reset,BaselineInstall}/{Spot,UsdMPerpetual}`
  cells at the Phase-6 classification depth (100) with the same prepared
  states:
  - `Stale` and `Duplicate`: mandatory zero-allocation controls
    (classification completes before any mutation; `book_projection.cpp:147-152`).
  - `Gap`: mandatory zero-allocation cell (`record_gap` is `noexcept`,
    allocation-free; `book_projection.cpp:207-214`).
  - `Reset`: deallocation-only cell (book nodes freed; no allocation).
  - `BaselineInstall`: full measured cell (OD-M5-P7-011).
- No new semantic operation is invented for measurement convenience. Every
  measured cell is an existing production public-API operation with an
  existing Phase-6 workload identity.

**Rationale:** Deep-book scaling evidence is the core Phase-7 deliverable;
the transaction is O(book) by construction, and only the full matrix proves
how strongly that dominates batch-driven traffic.

---

## OD-M5-P7-010 — M3 attribution / decomposition

**Question:** How are production end-to-end measurements separated from
diagnostic proxy measurements?

**Decision:**

- **Production end-to-end:** the 48-cell matrix measures the complete
  production `BookProjection::apply` public API path exactly as production
  executes it (`book_projection.cpp:198-205`). These measurements are the
  only numbers that may be labelled "production M3 accepted apply"
  allocation evidence.
- **Diagnostic proxy/component:** the Phase-6 families
  `M3/Component/AllLevelsBothSides/{8,100,1000}`,
  `M3/Proxy/CandidateRebuildFromVectors/{8,100,1000}`,
  `M3/Proxy/CandidateApplyUpdates/{8,100,1000}`, and
  `M3/Proxy/OrderBookMoveCommit/{8,100,1000}` are measured for allocation
  with the same names, semantics, and workload identities as Phase 6
  (including the Phase-6 documented constraints: MoveCommit move-assigns
  into a destination holding the populated old book so destination
  destruction is included; CandidateApplyUpdates consumes a bounded pool of
  fully constructed candidates so rebuild is excluded).
- These proxy cells remain **approximate component/proxy evidence and are
  NEVER claimed as an exact decomposition** of `BookProjection::apply`
  (preserving OD-M5-P6-009 exactly). In particular:
  - `sum(proxy results) == BookProjection::apply` is a forbidden claim;
  - subtracting proxy results from the production apply total and calling
    the remainder an exact hidden component is a forbidden claim;
  - proxy names must continue to include `Component` or `Proxy`.
- **Established transaction facts** (recorded from source, not from
  proxies): the production accepted transaction performs full-bid-side and
  full-ask-side `all_levels` copies, a candidate `OrderBook` construction,
  a candidate `replace_all`, candidate `apply_updates`, and a `noexcept`
  move-assignment that destroys the previous book (`apply_transaction`).
  Phase-7 reports may quote this structure as implementation fact with the
  source location, and may present proxy cells as diagnostic illustration of
  individual stages, but the authoritative per-stage evidence is
  "diagnostic proxy", not "decomposition".
- **Container-versus-transaction protection:** the attribution question is
  answered by measured relations, not by subtraction:
  1. persistent `std::map` storage cost comes from OD-M5-P7-006;
  2. ordinary M2 mutation cost comes from OD-M5-P7-008;
  3. accepted M3 apply cost comes from the 48-cell matrix
     (OD-M5-P7-009);
  4. the transaction component is evidenced by comparing (3) against (1)
     and (2) at equal depths and by the labelled proxy cells.
  If the measured accepted-apply traffic is on the order of the full-book
  footprint per accepted batch even at B=0, the record reports that as a
  measured property of the transaction architecture — never as a container
  verdict (Phase 8 owns verdicts).

**Rationale:** Phase 6 deliberately labelled these proxies approximate; Phase
7 must not silently upgrade their status, or Phase 8 could "fix the
container" for a cost that is actually transaction architecture.

---

## OD-M5-P7-011 — Baseline install allocation

**Question:** What is the exact measurement boundary for M3 baseline
installation?

**Decision:**

- **Bracket:** entry of `BookProjection::install_baseline` through return of
  its `InstallResult`, with disposition `Installed` asserted each execution
  (fail closed otherwise).
- **Included:** M2 `replace_all` internals (temporary map construction, node
  allocations for surviving levels, old-map destruction on move-assign),
  scalar state writes.
- **Excluded (prepared outside the bracket):** input level vectors
  (harness-owned `std::vector<BookLevel>` spans), the `BookProjection`
  construction itself, pool construction, result printing.
- **Prepared-state discipline:** each measured execution uses a freshly
  prepared `AwaitingBaseline`/`AwaitingBridge`/`NeedsResync` projection from
  a bounded pool built entirely outside the bracket; a projection used by
  one measured execution is never reused for another measured execution
  without re-preparation. Installation into a `Synchronized` projection is
  not a measured cell (its `RejectedWrongState` path is a zero-allocation
  control covered by the classification cells).
- **Repeated-measurement drift:** repeated executions measure identical
  semantic preconditions (same baseline content, same starting status) so
  count/bytes repeat exactly (OD-M5-P7-015); any drift fails the cell
  closed.
- **Old-state destruction:** where a previous baseline/preserved book exists
  (`AwaitingBridge` or `NeedsResync` re-install), its node destruction is
  inside the bracket (it is part of the production operation) and is
  reflected in deallocation/live metrics; the cell's semantic precondition
  records which starting state was prepared.

**Rationale:** Baseline installation is the M3 transaction's sibling; its
boundary must be as precise as the apply boundary or Phase 8 cannot compare
recovery costs.

---

## OD-M5-P7-012 — M4 allocation coverage

**Question:** Which M4 operations are measured, with which boundaries?

**Decision (inventory, reusing Phase-6 family names and depth set
{8,100,1000}):**

```text
M4/AdaptExchangeDepthSnapshot/Spot/{8,100,1000}
M4/AdaptDepthUpdate/Spot/{8,100,1000}
M4/CheckedInstall/{8,100,1000}
M4/CheckedApply/{8,100,1000}
M4/MakeLocalOrderBookSnapshot/{Unlimited,Limited}/{8,100,1000}
M4/SerializeSnapshot/{FreshBuffer,ReusedBuffer}/{8,100,1000}
```

**Boundaries (mirroring OD-M5-P6-010/011/012):**

- `AdaptExchangeDepthSnapshot`: bracket = the adaptation call only. Inbound
  Protobuf wire message is preconstructed OUTSIDE the bracket (wire
  construction is setup, OD-M5-P6-011). Output is the owning
  `AdaptedBookBaseline` (its vectors/metadata/sidecar allocations are
  inside the bracket).
- `AdaptDepthUpdate`: bracket = the adaptation call only; wire
  preconstructed outside.
- `CheckedInstall`: the owner is pre-adapted outside the bracket; bracket =
  `install_into(projection)` (binding checks + production `install_baseline`).
- `CheckedApply`: owner pre-adapted outside; bracket =
  `apply_to(projection)` (binding checks + full accepted apply transaction).
- `MakeLocalOrderBookSnapshot/{Unlimited,Limited}`: bracket = the snapshot
  builder call; `SnapshotContext`/`SnapshotOptions` preconstructed outside;
  serialization excluded.
- `SerializeSnapshot/{FreshBuffer,ReusedBuffer}`: bracket = the
  serialization call; `FreshBuffer` is the formal primary, `ReusedBuffer`
  the optional diagnostic (OD-M5-P6-012).
- **Wire construction/setup allocations must never be attributed to
  adaptation:** a separately labelled diagnostic of Protobuf message
  construction is permitted but is not part of the formal M4 inventory
  (OD-M5-P6-011 preserved).
- **Availability:** M4 allocation executables are built only with
  `BMD_PROJECTION_BUILD_PROTO_ADAPTER=ON` and pinned Contracts bootstrapped;
  absence of the required M4 inventory fails Phase-7 validation closed
  (mirroring OD-M5-P6-013).
- **Live/peak eligibility:** subject to the M4/Protobuf special rule of
  OD-M5-P7-005.
- **Protobuf runtime one-time initialization** is absorbed by the untimed
  warmup (OD-M5-P7-014); no measured bracket may contain first-use runtime
  initialization.

**Rationale:** Phase-6 already fixed these timing boundaries; Phase 7 must
reuse them verbatim so timing and allocation evidence describe the same
production paths.

---

## OD-M5-P7-013 — Replay allocation workload

**Question:** What replay allocation workload is measured and how?

**Decision:**

- **Coverage: BOTH** `CoreNormalizedReplay/{Spot,UsdMPerpetual}` and
  `AdapterWireReplay/{Spot,UsdMPerpetual}`.
- **Exact workload identity:** the accepted small-tier workloads: generator
  `m5-small-generator-v1`, seed `548746690337`, 2,048 events each for Spot
  and USD-M, with the canonical replay-log SHA-256 and workload-spec
  identity recorded in every result (the same identities as Phase 6; no new
  fixture is invented).
- **Measured path (per-event):** the production pipeline only —
  preloaded normalized operation → production M1 parsing where applicable →
  production M3 `BookProjection` → minimal evidence consumption
  (FNV-1a fold, no allocation) for Core; preconstructed valid wire →
  production M4 adaptation → checked M3 invocation → minimal consumption
  for Adapter.
- **Excluded from every measured region:** fixture parsing/loading,
  wire preconstruction, hashing of fixtures, differential oracle
  verification (runs exactly once outside any measured region, mirroring
  Phase-6), `OperationObservation`/checkpoint machinery, reference models,
  diagnostic rendering. **The reference oracle/differential machinery must
  never be included in a metric labelled production replay allocation.**
- **Denominator:** one event. Aggregate totals over the 2,048-event pass
  are recorded together with `event_count`; per-event values are derived as
  exact integer division of aggregates. No per-event micro-brackets are
  used for aggregate reporting.
- **Setup/reset rules:** one full untimed warmup pass with discarded state;
  fresh production state per measured pass; post-pass final-state/checksum
  validation outside the bracket; pass failures fail the cell closed
  (mirroring the Phase-6 replay benchmark and latency-sampler discipline).
- **Reporting:** aggregate `allocation_count`, `total_allocated_bytes`
  (and eligible deallocation/live metrics) per full pass, plus
  derived per-event values labelled `derived_per_event` with the exact
  division recorded.

**Rationale:** Replay allocation is the only evidence that shows the
end-to-end production path's allocation profile per real event shape; it
must measure the production path only, or Phase 8 would misattribute
validation infrastructure cost.

---

## OD-M5-P7-014 — Warmup and one-time runtime effects

**Question:** What warmup is specified, and what one-time effects are
excluded?

**Decision:**

- Every measurement executable performs exactly one **full untimed
  workload-equivalent warmup pass** before any snapshot or bracket:
  same operations, same semantic preconditions, same cell sequence;
  all warmup state is discarded and fresh measured state is prepared
  afterwards (mirroring OD-M5-P6-020's warmup discipline).
- The allocation counters are zeroed after warmup completes; no warmup
  allocation is ever counted.
- Warmup must not mutate the measured state such that the measured
  operation changes semantic disposition: pool discipline guarantees each
  measured execution sees an identical fresh precondition
  (OD-M5-P7-007, OD-M5-P7-011).
- One-time effects absorbed by warmup and explicitly documented:
  1. allocator/runtime first-use paths (malloc arena initialization);
  2. lazy library initialization inside the STL/runtime;
  3. Protobuf runtime initialization (M4 executables perform one untimed
     adaptation + serialization warmup specifically for this);
  4. any one-time construction performed by the harness (pools, output
     buffers, table preallocation).
- No measured bracket may contain a first-use path; a cell that triggers a
  previously unseen one-time allocation path fails closed as
  instrumentation contamination (OD-M5-P7-015).

**Rationale:** One-time allocations would otherwise appear as fake
per-operation traffic and corrupt every delta.

---

## OD-M5-P7-015 — Repetition / determinism

**Question:** How are repetitions and determinism handled for allocation
measurements?

**Decision:**

- **Allocation determinism (count/bytes/deallocation/live):** under one
  fixed binary and one fixed workload, bracket-scoped counts and bytes are
  deterministic (no ambient dependence exists: no adaptive iteration, no
  clock, no unordered traversal on measured paths). Expected property:
  **exact equality** of `allocation_count`, `total_allocated_bytes`,
  `deallocation_count`, and (where eligible) `peak_live_bytes` across
  repetitions.
- **Repetition requirement:** each cell executes a fixed number of measured
  repetitions (minimum 3 per formal run; fixed iteration counts per
  execution, no `MinTime`), and the record asserts exact equality across
  them within the run. Additionally, the full run is executed 3 times
  (separate process invocations of the same binary) and cross-run equality
  is asserted for formal evidence.
- **Variance:** any observed difference across repetitions or runs is
  defined as instrumentation contamination or stateful-operation drift.
  The run FAILS CLOSED; there is no averaging, no median, and no
  tolerance band for count/bytes. (A legitimate difference can only come
  from a nondeterministic measured path, which the design excludes by
  construction; if one is ever found, that cell's metric becomes
  INELIGIBLE and the cause must be recorded, not averaged.)
- **Memory-footprint repetitions:** same exact-equality rule for the
  footprint deltas (fixed binary, deterministic population order).
- **Instrumentation contamination definition:** any of — nonzero result in
  a zero-allocation control cell; count/bytes variation across equal
  preconditions; live-bytes failing to return to baseline at bracket close;
  table overflow; unknown-pointer delete; nested bracket attempt
  (OD-M5-P7-019).
- **Phase-6 timing statistics are NOT mechanically reused:** allocation
  cells do not report mean/stddev/percentiles of counts; they report exact
  values plus the number of confirming repetitions. Timing statistical
  methodology remains the property of Phase-6 timing artifacts only.

**Rationale:** Count determinism is the strongest available check that the
bracket discipline holds; statistical machinery would mask real
contamination.

---

## OD-M5-P7-016 — Machine-readable evidence schema

**Question:** What machine-readable schema records Phase-7 evidence?

**Decision:**

- **New versioned schemas (Phase-7-owned):**
  - wrapper: `M5_ALLOCATION_WRAPPER_V1`;
  - measurement contract: `M5_PHASE7_MEASUREMENT_CONTRACT_V1`;
  - per-cell record: `M5_PHASE7_ALLOCATION_RECORD_V1`;
  - footprint record: `M5_PHASE7_FOOTPRINT_RECORD_V1`.
- **Existing schemas are NOT altered and NOT redefined:**
  `M5_BENCHMARK_WRAPPER_V1`, `M5_PHASE6_MEASUREMENT_CONTRACT_V1`,
  `M5_REPLAY_LATENCY_V1`, and `M5_BENCHMARK_WORKLOAD_SPEC_V1` remain
  unchanged. Phase-7 records **reference** the accepted Phase-6 workload
  identities (workload ID + workload-spec SHA-256 + generator/schema/seed +
  generated-workload SHA-256) verbatim.
- **Provenance reuse:** the wrapper reuses the Phase-6 provenance field
  shapes (source/binary/build/environment/M4-dependency identity)
  without duplicating their computation: the existing
  `bmd_projection_generate_benchmark_build_identity` generation is reused.
- **`M5_PHASE7_ALLOCATION_RECORD_V1` fields (required where applicable):**

  ```text
  schema
  measurement_contract_version
  evidence_class                    formal | exploratory
  source.git_sha
  source.dirty_at_configure         false | true | unavailable
  binary.sha256
  build: compiler id/version, cxx standard, build type, sanitizer state,
         LTO state, stdlib name/version/detection status,
         conan lock SHA-256, relevant Conan refs/package IDs
  environment: os name/version, architecture, cpu model, logical core count
  m4_dependency_identity            (or explicit not_applicable)
  workload_id
  workload_spec_sha256
  generator/fixture identity        (schema, seed, canonical log SHA-256,
                                     event_count — replay records)
  measurement_scope                 family/cell name (exact Phase-6 name)
  operation_denominator             one <operation kind>
  allocation_count
  total_allocated_bytes
  deallocation_count
  deallocated_bytes                 (present only when eligible)
  live_bytes_after_bracket          (present only when eligible)
  peak_live_bytes                   (present only when eligible)
  peak_live_eligibility             eligible | ineligible:<reason code>
  baseline_definition               {snapshot_a, snapshot_b, delta_formula}
  calibration_record                (separate; never subtracted)
  repetitions                       exact-equality confirmation count
  result_payload_sha256
  ```

- **`M5_PHASE7_FOOTPRINT_RECORD_V1` fields:** depth, market-generator
  identity, `measured_requested_heap_bytes_total`,
  `measured_requested_heap_bytes_per_side_bids/asks`,
  `measured_bytes_per_level_per_side` (exact numerator/denominator pair),
  `fixed_object_footprint_bytes`, `empty_book_baseline_bytes`,
  `modeled_overhead` (formula + constants + toolchain identity, clearly
  labelled MODELED), eligibility flags, provenance as above.
- **Fail-closed validation:** a Phase-7 Python validator
  (proposed `scripts/benchmark_phase7.py`, mirroring
  `scripts/benchmark_phase6.py`) with deterministic Python tests and C++
  tests for the record construction. Validators reject: unknown schema,
  missing mandatory fields, ineligible-metric substitution, absent
  baseline definition, formal records with dirty source, payload SHA
  mismatch, missing workload identity.
- Schema duplication is avoided by reusing the Phase-6 provenance block and
  workload identity; Phase-7 adds only the allocation/footprint payload.

**Rationale:** Phase 6 established the provenance machinery; Phase 7 adds a
truthful allocation payload without forking the existing contracts.

---

## OD-M5-P7-017 — Formal evidence eligibility

**Question:** When may a Phase-7 run be called
"FORMAL CURRENT-PRODUCTION ALLOCATION/MEMORY BASELINE"?

**Decision (all conditions required):**

1. source clean at configure AND build (`source.dirty_at_configure ==
   false`, with the recorded git SHA matching the checked-out tree);
2. Release build; sanitizers off; LTO state explicit and recorded;
3. exact binary SHA-256 recorded and rehashable (mirroring OD-M5-P6-022);
4. exact source SHA recorded;
5. canonical workload identities present: every record carries the accepted
   Phase-6 workload ID + workload-spec SHA-256 (and fixture/generator
   identity for replay records);
6. complete required inventory: all OD-M5-P7-008 M2 cells; all 48 M3
   accepted-apply cells plus the classification cells; all OD-M5-P7-012 M4
   families; both Spot and USD-M replay records; the four footprint depths;
7. determinism confirmation: exact equality across the required repetitions
   and 3 separate process invocations (OD-M5-P7-015);
8. no instrumentation error/overflow/table-overflow/nested-bracket/
   unknown-pointer-delete in the run (OD-M5-P7-019);
9. eligible metric set marked per record (live/peak per OD-M5-P7-005,
   including the M4/Protobuf special rule);
10. provenance wrapper complete and payload SHA binding valid.

Anything else is **exploratory**. Exploratory runs are permitted for
development and are labelled `evidence_class = exploratory`; an exploratory
run can never later be re-labelled formal. A run that fails any formal
condition downgrades to exploratory or invalid, never silently formal.

**Rationale:** This mirrors and extends the Phase-6 formal-baseline
eligibility (OD-M5-P6-022/028) with instrumentation-specific gates.

---

## OD-M5-P7-018 — Toolchain/environment interpretation

**Question:** What environment identity does formal Phase-7 evidence
require, and what may be compared across environments?

**Decision:**

- **Canonical formal identity:** formal Phase-7 evidence is generated in the
  canonical pinned Quality environment of INFRA-TC-001 (the
  repository-owned `scripts/quality.sh` container contract:
  `ubuntu:24.04` pinned by digest, clang 18.1.3, pinned historical Ubuntu
  archive snapshot). A formal run records the canonical toolchain contract
  identity in its build identity.
- **Other environments:** Ubuntu GCC, Ubuntu Clang (native), and macOS
  AppleClang runs are supported and recorded with full environment identity,
  but they are labelled `evidence_class = exploratory` (environment-
  specific evidence). They are citable supporting evidence, never the
  canonical formal baseline.
- **Cross-environment comparison rules:**
  - Byte values (allocation counts, byte totals, live/peak bytes,
    footprint bytes) are NEVER compared across environments: allocator
    (glibc malloc vs macOS malloc), STL implementation (libstdc++ vs
    libc++ node sizes), and ABI layouts legitimately change these numbers.
    No universal-byte claim is ever made.
  - Structural conclusions (which stage dominates a measured path, whether
    accepted-apply traffic scales with depth at B=0, whether mutation
    allocation is depth-independent) may be compared across environments
    only when each environment's own numbers support the same conclusion,
    and the cross-environment nature of the claim is stated.
  - Derived ratios (e.g., accepted-apply allocation vs persistent
    footprint at equal depth) are computed within one environment only and
    reported with that environment identity.
- Toolchain identity fields (compiler, stdlib name/version/detection
  status, sanitizer/LTO state) are mandatory in every record so
  cross-environment misuse is detectable by validators.

**Rationale:** The repository already pins a canonical environment
(INFRA-TC-001); anchoring formal byte evidence there prevents
cross-environment number laundering into Phase 8.

---

## OD-M5-P7-019 — Instrumentation overflow / failure behavior

**Question:** How do counters and the tracking table fail closed?

**Decision:**

- **Integer widths:** all counters (`allocation_count`,
  `total_allocated_bytes`, `deallocation_count`, `deallocated_bytes`,
  `live_bytes`, `peak_live_bytes`, `max_concurrent`) are `std::uint64_t`.
- **Overflow handling:** every increment uses checked arithmetic (explicit
  check or compiler checked-add builtin). On overflow: a sticky
  `overflowed` flag is set, the affected counter freezes at
  `UINT64_MAX`, and the affected metric class of the ENTIRE run is marked
  INVALID. A wrapped counter can never silently produce a plausible PASS.
- **Allocation failure interaction:** if the underlying `std::malloc`
  returns null, `std::bad_alloc` propagates (or null is returned by the
  nothrow form); the failed request is not counted. If this occurs inside a
  measured bracket, the cell is marked ERROR (no partial result is
  recorded). Failures outside brackets behave like any normal program
  failure.
- **Recursion guard:** instrumentation bookkeeping performs no allocation
  on the hot path (thread_local POD counters); the pointer→size table is
  preallocated before any bracket through the overridden operator with the
  recursion-guard flag active, so table setup cannot count itself.
- **Malformed state (sticky, run-level):**
  - unknown-pointer delete (freed pointer absent from the table):
    count/bytes remain valid; deallocated-bytes/live/peak metric class
    INVALID;
  - table overflow (concurrent live allocations exceed the preallocated
    table capacity): live/peak class INELIGIBLE (OD-M5-P7-005);
  - live-bytes negative or bracket-close mismatch with pre-bracket
    baseline: live/peak class INELIGIBLE, cell ERROR;
  - sized-delete size mismatch with the recorded size: run-level
    instrumentation ERROR.
- **Measurement nesting:** nested measurement brackets are REJECTED:
  attempting to open a bracket while one is active is a defined
  fail-closed abort with a stable diagnostic (assertion in the Scope
  constructor). No silent outer-inner accumulation.
- **Exception handling:** `MeasurementScope` is RAII. If the measured
  production call throws, the bracket closes, counters retain their
  recorded values, and the cell is marked ERROR (the operation did not
  complete). Normal measured cells must not throw; a throw is always an
  error condition in Phase-7 evidence.

**Rationale:** The counters are the instrument; an instrument that can lie
plausibly is worse than no instrument.

---

## OD-M5-P7-020 — Adversarial validation

**Question:** What deterministic tests prove the measurement mechanism
itself?

**Decision (mandatory adversarial suite, dedicated single-threaded test
executable `bmd_projection_allocation_instrumentation_tests`):

1. **Known allocation count:** N plain `new`/`delete` pairs of size S inside
   a bracket → `allocation_count == N`,
   `total_allocated_bytes == N*S` (N and S as literals).
2. **Known byte total with mixed sizes:** a literal sequence of mixed-size
   allocations → exact byte sum computed independently in the test.
3. **Array form:** `new[]`/`delete[]` pairing counted exactly once per
   request, byte total = array size.
4. **Nothrow form:** `new (std::nothrow)` success path counted; nothrow
   allocation failure (simulated) not counted and returns null.
5. **Aligned form:** over-aligned type (e.g., `alignas(64)` struct)
   round-trips: payload address satisfies the alignment, count/bytes
   correct, delete recovers the recorded size.
6. **Sized and unsized deletion:** sized delete value equals the recorded
   size; unsized delete recovers size from the table; both decrement
   live-bytes identically.
7. **Live-byte return to baseline:** a known ascent/descent allocation
   profile leaves `live_bytes` exactly at the pre-bracket baseline.
8. **Peak-live behavior:** a deterministic profile with a known maximum
   (e.g., allocate A=1000, then B=2000 while A lives, free B, free A) →
   `peak_live_bytes` equals the exact expected maximum, and peak is
   independent of deallocation order (two orderings asserted).
9. **Nested measurement rejection:** opening a bracket inside a bracket
   fails closed (asserted defined abort/rejection).
10. **Overflow simulation:** seed a counter near `UINT64_MAX` via the test
    hook, force the overflow, assert the INVALID flag and frozen value.
11. **Setup allocation exclusion:** allocations before bracket open do not
    change bracket counters.
12. **Instrumentation-disabled region exclusion:** a disabled scope between
    two brackets leaves both brackets' counters unaffected.
13. **Repeated-run determinism:** the same deterministic profile yields
    identical count/bytes across 3 separate process invocations (or
    subprocess re-exec) of the same binary.
14. **Unknown-pointer delete:** freeing a pointer allocated before
    instrumentation (via malloc, not new) marks the live-metric class
    INVALID while count/bytes remain valid.
15. **Table-overflow simulation:** forcing live concurrency past the table
    capacity marks live/peak INELIGIBLE while count/bytes remain valid.
16. **Zero-size allocation:** `new char[0]`-style zero-size request counts
    as 1 allocation contributing 1 byte.

**Independence rule:** tests must not simply re-execute a second copy of the
counter logic. Expected values are hand-computed literals in the test;
at least one test cross-checks a container-driven profile (e.g., a
deterministic `std::vector` growth sequence) against an independently
hand-derived expected total computed from the documented growth policy.

**Rationale:** Phase 8 will trust these numbers; the mechanism must be
proven at least as hard as the measurements it produces.

---

## OD-M5-P7-021 — Phase-8 handoff

**Question:** What exactly must Phase 7 hand to Phase 8?

**Decision:**

Phase 7 MUST NOT choose the new container, define a migration gate, or
recommend a candidate. It hands to Phase 8:

1. **Persistent storage evidence:** measured requested heap bytes for the
   production `std::map` book at 100/1,000/5,000/10,000 levels per side,
   per-side and total, with fixed-object footprint, empty-book baseline,
   bytes-per-level, and the MODELED node/allocator overhead (OD-M5-P7-006).
2. **M2 mutation evidence:** allocation/deallocation profiles for
   insert/update/delete/missing-delete, batch updates, replace_all, and
   queries (OD-M5-P7-008).
3. **M3 accepted-apply evidence:** the full 48-cell matrix plus
   classification cells, showing how accepted-apply allocation scales with
   depth at B=0 and with batch size (OD-M5-P7-009, OD-M5-P7-011).
4. **M3 transaction evidence:** the documented production transaction
   structure with source locations, plus the labelled Component/Proxy
   diagnostic cells (never an exact decomposition; OD-M5-P7-010).
5. **M4 evidence:** adaptation, checked install/apply, snapshot
   construction, serialization (OD-M5-P7-012).
6. **Replay evidence:** per-event derived allocation for
   CoreNormalizedReplay and AdapterWireReplay, Spot and USD-M
   (OD-M5-P7-013).
7. **Environment discipline:** canonical formal environment identity,
   exploratory environment records, and the cross-environment comparison
   rules (OD-M5-P7-018).
8. **Schemas and validators** so Phase 8 can consume Phase-7 records
   mechanically (OD-M5-P7-016).

The Phase-7 report must allow Phase 8 to answer, with measured evidence:

```text
1. How much persistent memory is attributable to std::map storage?
2. How many allocations/bytes come from ordinary M2 mutation?
3. How much accepted M3 apply traffic is driven by whole-book
   transaction copying/reconstruction?
4. Does deep-book M3 cost scale strongly with depth even when batch
   size is zero/small?
5. Would replacing only the M2 container plausibly attack the dominant
   cost, or is transaction architecture also a major factor that must be
   considered separately?
```

Question 5 is answered only as a measured-evidence statement about where
the dominant cost sits in the CURRENT implementation; it is not a container
choice. No arbitrary numeric migration gate is created here. Phase 8
remains responsible for candidate comparison
(`Phase8Candidate / Phase8StdMapControl` under one candidate interface,
per OD-M5-P6-027).

**Rationale:** Phase 7 exists to make Phase 8 interpretable; a handoff that
pre-answers the container question would corrupt the separation the M5
design mandates.

---

## OD-M5-P7-022 — Phase boundary / CI ownership

**Question:** What CI/reporting work does Phase 7 own?

**Decision:**

Phase 7 may define and later implement:

1. local/formal evidence generation scripts (proposed
   `scripts/benchmark-allocation.sh` pattern, plus the Phase-7 Python
   validator);
2. validation scripts needed by Phase-7 implementation;
3. normal test integration required for correctness: the adversarial
   instrumentation tests and validator tests integrate into the existing
   CTest test tree (`scripts/test.sh`), so they run in the existing test
   jobs without any workflow change.

Phase 7 must NOT define or implement:

- the scheduled/history/reporting workflow;
- benchmark artifact upload or retention;
- any new CI workflow file or job;
- changes to the existing `benchmark-smoke` workflow's Phase-6 contract
  (the Phase-6 locked smoke subset and its validators remain authoritative
  and unchanged);
- numeric allocation thresholds in any CI gate;
- Phase-10 scheduled/manual allocation integration.

Any future integration of Phase-7 evidence into CI belongs to Phase 10,
which owns the larger CI/reporting workflow integration. No new workflow
implementation is part of this docs-only PR.

**Rationale:** Phase 10 boundaries from the M5 design and Phase-6 decision
record are preserved; Phase 7 local evidence generation plus normal test
integration is the entire CI-related scope.

---

## Critical measurement model (summary)

The normative model established by OD-M5-P7-003/004/005/006/007:

```text
ALLOCATION COUNTING
  dedicated single-threaded executables; global operator overrides;
  bracket-scoped thread_local counters; count incremented only on
  successful allocation; zero-size counts as 1; fixed iterations; exact
  equality across repetitions (OD-M5-P7-003/004/015).

TOTAL BYTES
  sum of caller-requested sizes (0 → 1); alignment padding and malloc
  overhead excluded from measured bytes and reported only as modeled
  overhead (OD-M5-P7-004/006).

LIVE / PEAK
  pointer→size table; event-driven exact peak; sized delete is a
  consistency check only; eligibility fails closed on table overflow,
  unknown-pointer delete, or baseline non-return; M4/Protobuf paths need
  the tracked-deallocation proof; INELIGIBLE is reported, never
  estimated (OD-M5-P7-005).

FOOTPRINT
  empty → bids-only → both-sides snapshot deltas at 100/1k/5k/10k per
  side; fixed-object footprint; bytes-per-level as an exact rational;
  MEASURED vs MODELED vs RSS strictly separated; RSS not measured
  (OD-M5-P7-006).

BASELINE SUBTRACTION
  every delta is (snapshot_B − snapshot_A) over a named lifetime in one
  process; snapshots allocate nothing; harness/pool allocation outside
  brackets; calibration reported, never subtracted; warmup absorbs
  one-time effects (OD-M5-P7-007/014).

WORKLOAD ATTRIBUTION
  reuse Phase-6 workload identities verbatim; production buckets A–E of
  OD-M5-P7-001; Component/Proxy diagnostics labelled and never an exact
  decomposition; reference/oracle machinery never inside a production
  bracket (OD-M5-P7-008/009/010/012/013).
```

## M3 architecture attribution (established facts)

The accepted production `BookProjection::apply` architecture is, at the
immutable base:

```text
accepted BookProjection apply
        ↓
full current book extraction / copy      (all_levels, both sides)
        ↓
candidate reconstruction                 (OrderBook ctor + replace_all)
        ↓
candidate updates                        (apply_updates on the candidate)
        ↓
noexcept commit                          (move assignment)
        ↓
old state destruction                    (destroyed by the move assignment)
```

Established from current source (`src/projection_state/book_projection.cpp`,
`apply_transaction`, lines 198-205; `src/order_book/order_book.cpp`,
`replace_all` lines 84-106, `all_levels` lines 128-130, map typedefs lines
10-11, `noexcept` move static asserts lines 13-14). These are facts about
the current implementation, not claims derived from proxy measurements.

The diagnostic proxy cells (`AllLevelsBothSides`,
`CandidateRebuildFromVectors`, `CandidateApplyUpdates`,
`OrderBookMoveCommit`) remain labelled approximate component/proxy evidence
per OD-M5-P6-009 and are never presented as an exact decomposition
(OD-M5-P7-010).

**Why Phase 7 must precede Phase 8:** the current accepted-apply path
reallocates and destroys a full book's worth of storage on every accepted
batch by design (the strong-exception-guarantee transaction). If Phase 8
benchmarked candidate containers against production `BookProjection::apply`
without Phase-7 evidence, any container improvement could be swamped by, or
wrongly credited to, transaction cost. Phase 7 establishes where the
current costs actually are, so Phase 8 can compare candidates against
meaningful baselines and consider container choice and transaction
architecture as separate factors. A possible conclusion is
"container choice is only part of the cost" — but that conclusion is NOT
assumed in advance; it must be measured first.

## Non-goals

- This document does NOT implement Phase 7: no allocation counter code,
  no operator-new overrides, no benchmark executables, no memory-report
  generators, no Phase-7 validators, no CMake targets, no CI jobs, no
  container models, no abseil usage, no production changes.
- Phase 7 (allocation instrumentation): NOT STARTED.
- Phase 8 (alternative container spike): NOT STARTED.
- Phase 10 (reporting/workflow integration): NOT STARTED.
- No production API addition, no PIMPL weakening, no production container
  change, no production semantics change.
- No new production dependency; instrumentation is standard-library-only.

## References

- `docs/M5_DIFFERENTIAL_VALIDATION_AND_PERFORMANCE_DESIGN.md` (M5 design:
  allocation metrics, memory footprint, container spike, phases)
- `docs/M5_PHASE6_PREIMPLEMENTATION_DECISIONS.md` (OD-M5-P6-001..030)
- `docs/M5_PHASE6_REPRESENTATIVE_BENCHMARKS.md`
- `docs/M3_SEQUENCE_AND_PROJECTION_STATE_DESIGN.md` (logical copy-on-apply
  transaction, ADR-0005)
- `docs/M2_ORDER_BOOK_SEMANTICS.md`, `docs/M4_SNAPSHOTS_AND_PROTOBUF_BOUNDARY_DESIGN.md`
- `docs/adr/ADR-0003-single-writer-order-book.md`,
  `docs/adr/ADR-0005-market-specific-sequence-policy.md`,
  `docs/adr/ADR-0006-protobuf-adapter-boundary.md`,
  `docs/adr/ADR-0007-differential-validation-oracle-architecture.md`,
  `docs/adr/ADR-0008-spot-bootstrap-successor-coverage.md`
- `docs/QUALITY_TOOLCHAIN.md` (INFRA-TC-001 canonical environment)
- Production sources: `src/order_book/order_book.cpp`,
  `src/projection_state/book_projection.cpp`,
  `src/proto_adapter/proto_adapter.cpp`
- Isolation pattern: `tests/projection_state/book_projection_allocation_failure_test.cpp`,
  `tests/proto_adapter/proto_adapter_allocation_failure_test.cpp`,
  `tests/CMakeLists.txt`
- Phase-6 infrastructure: `benchmarks/CMakeLists.txt`,
  `benchmarks/benchmark_support/*`, `cmake/M5BenchmarkSupport.cmake`,
  `scripts/benchmark_phase6.py`
