# M5 Phase-7 Pre-Implementation Decisions

## Status

- Status: **PRE-IMPLEMENTATION DECISIONS APPROVED / MERGED**
- Decision-review baseline (immutable): `eed3de99efaba8eaa96083a5348d538ed44f6bfe`
  (live `main` at review time; the squash merge of PR #24,
  `docs: record INFRA-TC-001 acceptance and merge`)
- Final independently approved Head: `92288927165f2d7486491371a11a7a586c645565`
- PR: **#25** (`Lock M5 Phase 7 allocation/memory measurement contract`),
  **MERGED / CLOSED** (merged 2026-08-16T09:36:27Z)
- Squash merge: `c2ad198677130d05ad054ea48ade3a1d8021c153`
- Merged tree == approved Head tree: `0a9a12ead44effdf4f5b471785fa543d419eae29`
  (equal)
- Final independent methodology review: **APPROVED** (P0: **0**; P1: **0**;
  P2: **0**)
- Findings: M5-P7-MR-001 .. M5-P7-MR-010 **CLOSED**; M5-P7-RR-001 **CLOSED**
- Phase 6: **COMPLETE / MERGED** (PR #21, squash merge
  `227524e6d17cce77813c6f26cd65bb8d996f5677`)
- INFRA-TC-001: **COMPLETE / ACCEPTED / MERGED** (PR #23, squash merge
  `24fb72232e928290add45ed8634cd0bf9a8d3442`)
- M5 Phase 7 methodology: **APPROVED / MERGED**
- M5 Phase 7 implementation: **AUTHORIZED / NOT STARTED**
- M5 Phase 8 (container spike): **NOT STARTED**
- Implementation authorization: **YES** — became YES only after the final
  independent APPROVAL and the squash merge of PR #25 with the merged tree equal
  to the approved Head tree. AUTHORIZED != STARTED, != IMPLEMENTED, != COMPLETE:
  Phase-7 implementation remains NOT STARTED.

**Independent methodology review record (chronological):**

- Fresh independent methodology review of the initial record: **CHANGES REQUESTED**
  - P0: **1**; P1: **8**; P2: **1**
  - Findings: M5-P7-MR-001 .. M5-P7-MR-010
  - Corrections (initial round): **IMPLEMENTED FOR FOCUSED INDEPENDENT RE-REVIEW**
- First focused independent methodology re-review of the corrected record:
  **CHANGES REQUESTED**
  - P0: **0**; P1: **2**; P2: **0**
  - Dispositions at that time: M5-P7-MR-001 .. M5-P7-MR-008 CLOSED;
    **M5-P7-MR-009 OPEN**; M5-P7-MR-010 CLOSED
  - New finding at that time: **M5-P7-RR-001 — P1** (standard-conforming
    unknown-provenance adversarial delete construction)
  - Corrections (second round, MR-009 and RR-001): **IMPLEMENTED FOR FOCUSED
    INDEPENDENT RE-REVIEW**
- Final independent methodology re-review of the exact approved Head
  `92288927165f2d7486491371a11a7a586c645565`: **APPROVED**
  - P0: **0**; P1: **0**; P2: **0**
  - M5-P7-MR-001 .. M5-P7-MR-010: **CLOSED**
  - M5-P7-RR-001: **CLOSED**
- Closure status: **CLOSED** — all findings closed; methodology APPROVED and
  squash merged via PR #25 (see Status above).

**Historical review wording:** earlier review rounds recorded below (CHANGES
REQUESTED, PENDING, authorization NO) are preserved as history and describe
what was true before final independent approval; they are not current status.

This document is the normative pre-implementation measurement contract for M5
Phase 7 (allocation and memory characterization). It is NOT implementation
evidence: no allocation instrumentation code exists or is claimed here.

**This document does NOT implement Phase 7.** The merged PR #25 was
methodology/documentation only. Phase-7 implementation remains NOT STARTED.

## Authorization wording

- Before independent acceptance and merge, implementation authorization was
  **NO**.
- After final independent APPROVAL (P0: 0; P1: 0; P2: 0), all blocking
  findings CLOSED, and the squash merge of PR #25 with the merged tree equal
  to the approved Head tree, implementation authorization is **YES**.
- **AUTHORIZED != STARTED; AUTHORIZED != IMPLEMENTED; AUTHORIZED != COMPLETE.**
  Phase-7 implementation remains **NOT STARTED**; Phase 8 remains
  **NOT STARTED**.

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

Phase 7 must prevent Phase 8 from incorrectly blaming the container for costs
that are actually dominated by the BookProjection transaction architecture.

Phase 7 is **characterization, not optimization**. It changes no production code,
no production container, no production semantics, and no production public API.
`std::map` remains the production correctness baseline throughout Phase 7.

## Source-of-truth precedence

1. Authoritative C++ standard / working draft for replaceable allocation
   function signatures and language semantics (e.g.,
   [[new.delete.single]](https://eel.is/c++draft/new.delete.single),
   [[new.delete.array]](https://eel.is/c++draft/new.delete.array),
   [basic.stc.dynamic]).
2. Accepted repository ADRs / semantic authority (ADR-0003, ADR-0005 with its
   ADR-0008 Spot-bootstrap supersession, ADR-0006, ADR-0007, ADR-0008).
3. Accepted M5 design (`docs/M5_DIFFERENTIAL_VALIDATION_AND_PERFORMANCE_DESIGN.md`).
4. Accepted Phase-6 decision authority (`docs/M5_PHASE6_PREIMPLEMENTATION_DECISIONS.md`,
   `docs/M5_PHASE6_REPRESENTATIVE_BENCHMARKS.md`).
5. Exact production implementation and tests on immutable main
   `eed3de99efaba8eaa96083a5348d538ed44f6bfe`.
6. Independent methodology review findings M5-P7-MR-001 .. M5-P7-MR-010
   (recorded in this document).
7. Approved / merged Phase-7 decision authority
   (`docs/M5_PHASE7_PREIMPLEMENTATION_DECISIONS.md`, this document;
   independently approved at Head
   `92288927165f2d7486491371a11a7a586c645565` and squash merged via
   PR #25 at `c2ad198677130d05ad054ea48ade3a1d8021c153`; lower authority
   than the authoritative C++ standard and independent methodology review
   findings where they conflict).
8. Status/orientation documents; PR descriptions / old handoff reports.

When fixing a C++ language fact, the authoritative standard source is binding;
reviewer wording and model memory are not substitutes.

## Established implementation facts (inspected on the immutable base)

These facts were verified against the exact checked-out code at
`eed3de99efaba8eaa96083a5348d538ed44f6bfe`. Phase-7 measurement must be designed
against these facts, not against documentation alone.

### M2 storage and mutation (src/order_book/order_book.cpp)

- `OrderBook` is a PIMPL: `std::unique_ptr<Impl>` allocated by
  `std::make_unique<Impl>(numeric_spec)` (`order_book.cpp:200`).
- Storage is `BidMap = std::map<PriceUnits, QuantityUnits, std::greater<>>` and
  `AskMap = std::map<PriceUnits, QuantityUnits, std::less<>>` (`order_book.cpp:10-11`),
  both default-constructed with the default `std::allocator`, which obtains
  storage through the replaceable global `::operator new` family
  ([allocator.members]: `allocator::allocate` calls `::operator new`).
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
  (M2_ORDER_BOOK_SEMANTICS.md documents this). The returned vector is an owning
  output: it is alive when the operation returns.
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
  (approximately 2 × depth).
- Step 5: node allocations for inserted prices, node frees for deleted prices,
  value writes for existing prices — exactly M2 `apply_updates` semantics.
- Step 6: `noexcept` move assignment destroys the old book's two maps (frees
  approximately 2 × depth nodes). The old book's maps/nodes were allocated
  BEFORE this operation bracket (during population), so their destruction
  inside the bracket is a pre-bracket-allocation / in-bracket-delete case that
  the two-lifetime tracking model must resolve exactly (OD-M5-P7-003).
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
  quality sidecar. The returned owner is alive when the operation returns.
- `install_into` / `apply_to` are binding-checked synchronous M3 invocations:
  allocation cost is the underlying M3 transaction plus any small temporary.
- `make_local_order_book_snapshot` builds an owning Contracts
  `LocalOrderBookSnapshot` Protobuf message: Protobuf runtime allocation plus
  decimal-formatting string allocations. The returned message is alive when
  the operation returns.
- `SerializeToString` allocates a serialization buffer (`std::string`), which
  is alive when the operation returns.
- Protobuf wire-message construction is documented as setup only
  (OD-M5-P6-011) and never a measured production path.
- The Protobuf/Contracts runtime may, in principle, obtain memory through
  channels other than the C++ replaceable global-new family (direct `malloc`,
  arena allocation, other allocator channels). Phase 7 makes NO completeness
  claim about those channels (OD-M5-P7-005, OD-M5-P7-007 in this record).

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
  requests are normalized to 1 byte at the malloc level, and the underlying
  allocator is `std::malloc` / `std::free`.
- `tests/proto_adapter/proto_adapter_allocation_failure_test.cpp:272-284`
  (`measure_allocations`) already demonstrates deterministic allocation-count
  measurement of a production invocation through this pattern.
- Phase 7 generalizes this established pattern with the corrected two-lifetime
  semantics defined in this record.

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

## Independent Review Correction Record

Chronological review history. Fresh independent methodology review of the
initial Phase-7 decision record: **CHANGES REQUESTED** (P0: 1, P1: 8, P2: 1).
Corrections were **IMPLEMENTED FOR FOCUSED INDEPENDENT RE-REVIEW**; at that
time independent closure was **PENDING** the next fresh reviewer. The final
independent review of the approved Head later returned **APPROVED** (see
below).

| Finding | Severity | Correction | Affected ODs |
|---|---|---|---|
| M5-P7-MR-001 | P1 | Sized aligned delete signatures corrected to the standard forms `operator delete(void*, std::size_t, std::align_val_t) noexcept` / `operator delete[](void*, std::size_t, std::align_val_t) noexcept` (size BEFORE align_val_t, per [new.delete.single]/[new.delete.array]); adversarial suite exercises the true exact scalar and array sized-aligned overloads by direct call | 003, 019, 020 |
| M5-P7-MR-002 | P0 | Aligned backing allocation uses checked `std::size_t` arithmetic (`effective_payload + (alignment−1) + header`, checked-add at every step) BEFORE any malloc or pointer arithmetic; on unrepresentable size: throwing new → `bad_alloc`, nothrow new → `nullptr`, no provenance insertion, no counter increment, no pointer, no wrap; near-`SIZE_MAX` adversarial cases required; allocation sizing arithmetic and counter-overflow protection are separate domains | 003, 019, 020 |
| M5-P7-MR-003 | P1 | Two-lifetime model: TRACKING/PROVENANCE ACTIVE (from first intercepted allocation, permanently) vs MEASUREMENT COUNTERS ACTIVE (bracket only); every intercepted successful allocation records pointer→exact raw requested size regardless of bracket state; every intercepted deallocation resolves provenance regardless of bracket state; only the active bracket increments traffic counters; pointer reuse observes no stale entries; static-storage provenance table, no instrumentation heap allocation | 002, 003, 004, 005, 007, 019, 020 |
| M5-P7-MR-004 | P1 | Universal return-to-baseline and unsigned B−A negative-as-error rules removed; every bracket records A/P/B and reports `persistent_live_delta` as {sign: negative|zero|positive, magnitude: uint64}; positive delta is not a leak; negative delta is not an error; return-to-baseline remains valid only for cells/lifecycles whose normative net persistent change is zero; owning outputs get an explicit B/D lifecycle | 004, 005, 007, 012, 016, 019 |
| M5-P7-MR-005 | P1 | Normalized peak metrics: `peak_above_entry = P − A` and `transient_excess_over_persistent = P − max(A, B)` are the primary per-operation transient metrics; absolute process tracked live bytes remain contextual auditing evidence; formal per-operation comparison uses normalized metrics; adversarial proof that pre-existing pool footprints do not change normalized quantities | 005, 007, 015, 016, 021 |
| M5-P7-MR-006 | P1 | Raw requested byte semantics: `total_allocated_bytes` = exact sum of the raw `std::size_t` arguments RECEIVED by intercepted replaceable allocation functions for successful in-bracket allocations; a received size of 0 contributes 0 bytes (count += 1); backing malloc(1) normalization is a separate `instrument_backing_request_bytes` diagnostic, never conflated with the production metric; array-new observed argument is recorded faithfully, never guessed via `n*sizeof(T)` | 004, 016, 020 |
| M5-P7-MR-007 | P1 | Measurement boundary made explicit: `allocation_boundary = cxx_replaceable_global_new`; all metrics apply ONLY to traffic observed through that boundary; no "complete heap traffic" claim; M4/Protobuf tracked balancing + no unknown delete + lifecycle checks prove only internal consistency of the observed subset, never absence of direct malloc/arena channels; adversarial malloc/free bypass proof required | 005, 012, 016, 017, 021 |
| M5-P7-MR-008 | P1 | Replay derived per-event values are exact rationals {numerator, denominator}, not integer division; aggregate totals and event_count remain exact; `3/2` is represented as `3/2`, never `1`, and validators must not reject non-divisible aggregates | 004, 013, 016, 021 |
| M5-P7-MR-009 | P1 | Formal canonical Release execution mechanism locked (new OD-M5-P7-023): one repository-owned host entrypoint (`bash scripts/benchmark-allocation-formal.sh`, conceptual), INFRA-TC-001-style fail-closed host/internal trust boundary, read-only mounted `/src` retaining `.git`, fresh `/work`, compiled-source content binding, fixed internal entrypoint, no "CANONICAL QUALITY: PASS" emission, quality.sh semantics unchanged | 017, 018, 023 (new) |
| M5-P7-MR-010 | P2 | Modeled memory corrected: node structural model is NON-ADDITIVE (the allocation request already includes node object structure); allocator backing model is ESTIMATED, environment/toolchain/allocator/size-class-specific, non-formal (no universal 16-byte header assumption); RSS remains NOT MEASURED; schema guards against measured+node totals | 006, 016, 021 |

**Latest focused independent methodology re-review** (of the corrected
record, second round): **CHANGES REQUESTED** (P0: 0, P1: 2, P2: 0).

Independent dispositions at that time:

- M5-P7-MR-001 CLOSED
- M5-P7-MR-002 CLOSED
- M5-P7-MR-003 CLOSED
- M5-P7-MR-004 CLOSED
- M5-P7-MR-005 CLOSED
- M5-P7-MR-006 CLOSED
- M5-P7-MR-007 CLOSED
- M5-P7-MR-008 CLOSED
- M5-P7-MR-009 **OPEN** — the underdefined optional "copy repository source
  into `/work` + source-content manifest/hash binding" formal source path
  must be eliminated; exactly ONE formal source model is required
- M5-P7-MR-010 CLOSED
- M5-P7-RR-001 **NEW (P1)** — adversarial case 25 must exercise the real
  unknown-provenance branch of the replacement delete instrumentation with a
  standard-conforming C++ allocation/deallocation pairing; the invalid
  "direct `malloc` → `operator delete`" construction must not appear

Corrections for M5-P7-MR-009 and M5-P7-RR-001 in that revision were
**IMPLEMENTED FOR FOCUSED INDEPENDENT RE-REVIEW**; at that time independent
closure of either finding remained **PENDING** the next fresh reviewer.

**Final independent methodology re-review** (of the exact approved Head
`92288927165f2d7486491371a11a7a586c645565`): **APPROVED** (P0: 0, P1: 0,
P2: 0).

- M5-P7-MR-001 .. M5-P7-MR-010: **CLOSED**
- M5-P7-RR-001: **CLOSED**

The approved Head was squash merged via PR #25 at
`c2ad198677130d05ad054ea48ade3a1d8021c153`; the merged tree equals the
approved Head tree (`0a9a12ead44effdf4f5b471785fa543d419eae29`).

| Finding | Severity | Correction | Affected ODs |
|---|---|---|---|
| M5-P7-MR-009 (final round) | P1 | Formal source model locked to EXACTLY ONE option: `/src` is the sole source root (read-only mounted checkout, `.git` retained, CMake source root, the source compiled from) and `/work` is the sole fresh ephemeral build/output root; the optional "materialize a source copy into `/work` + content-manifest/hash binding" path is REJECTED and removed; NO source-content manifest is required; exact host entrypoint locked as `bash scripts/benchmark-allocation-formal.sh` with no alternative-name escape hatch; formal eligibility re-proved from the SAME `/src`; negative tests reformulated for the single-source model (dirty `/src`, unexpected HEAD, alternative source/work root impossible/rejected, CMake source root other than `/src` impossible, contract/image mismatch, non-Release, sanitizers, binary-SHA mismatch) | 017, 023 |
| M5-P7-RR-001 | P1 | Adversarial case 25 corrected to a standard-conforming unknown-provenance construction: legal `::operator new(K)` (fixed hand-known K > 0) records a normal provenance entry; the TEST-ONLY provenance-removal seam (`forget_provenance_for_test(p)`, isolated in `bmd_projection_allocation_instrumentation_tests`; conceptual spelling) removes the entry without freeing `p`; the legally matched `::operator delete(p)` then executes the replacement delete's real unknown-provenance branch; exact OD-M5-P7-019 fail-closed behavior asserted; storage freed exactly once; no UB; `malloc → operator delete` never appears as a valid construction; case 26 (direct `malloc`/`free` bypass) remains the separate measurement-scope proof | 019, 020 |

## Normative Decision Table

All decisions below are:

- Status: **APPROVED / MERGED** — final independent methodology review
  APPROVED (P0: 0, P1: 0, P2: 0); all findings CLOSED; squash merged via
  PR #25.

| ID | Decision | Status |
|---|---|---|
| OD-M5-P7-001 | Measurement purpose / attribution model | APPROVED (accepted direction, preserved) |
| OD-M5-P7-002 | Instrumentation isolation and two-lifetime model | APPROVED (corrected) |
| OD-M5-P7-003 | Complete allocation operator surface and backing arithmetic | APPROVED (corrected) |
| OD-M5-P7-004 | Mandatory allocation metrics (raw requested semantics) | APPROVED (corrected) |
| OD-M5-P7-005 | Live bytes / peak live bytes semantics (A/P/B model) | APPROVED (corrected) |
| OD-M5-P7-006 | Persistent footprint semantics (non-additive models) | APPROVED (corrected) |
| OD-M5-P7-007 | Baseline subtraction / snapshot discipline | APPROVED (corrected) |
| OD-M5-P7-008 | M2 workload coverage | APPROVED (accepted, preserved) |
| OD-M5-P7-009 | M3 accepted-apply matrix | APPROVED (accepted, preserved) |
| OD-M5-P7-010 | M3 attribution / decomposition | APPROVED (accepted, preserved) |
| OD-M5-P7-011 | Baseline install allocation | APPROVED (accepted, preserved) |
| OD-M5-P7-012 | M4 allocation coverage (scoped boundary) | APPROVED (corrected scope wording) |
| OD-M5-P7-013 | Replay allocation workload (exact rationals) | APPROVED (corrected) |
| OD-M5-P7-014 | Warmup and one-time runtime effects | APPROVED (accepted, preserved) |
| OD-M5-P7-015 | Repetition / determinism (normalized metrics) | APPROVED (corrected) |
| OD-M5-P7-016 | Machine-readable evidence schema | APPROVED (corrected fields) |
| OD-M5-P7-017 | Formal evidence eligibility | APPROVED (corrected identity/runner) |
| OD-M5-P7-018 | Toolchain/environment interpretation | APPROVED (accepted, preserved) |
| OD-M5-P7-019 | Instrumentation overflow / failure behavior | APPROVED (corrected) |
| OD-M5-P7-020 | Adversarial validation | APPROVED (rewritten, 30 cases) |
| OD-M5-P7-021 | Phase-8 handoff | APPROVED (corrected metrics) |
| OD-M5-P7-022 | Phase boundary / CI ownership | APPROVED (accepted, preserved) |
| OD-M5-P7-023 | Formal canonical Release execution mechanism | APPROVED (corrected: exact host command, single formal source model; M5-P7-MR-009 final round) |

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
   The observed requested heap bytes held by a populated M2 OrderBook
   (std::map nodes), measured by the footprint experiment (OD-M5-P7-006).

B. PER-OPERATION ALLOCATION TRAFFIC (M2)
   Allocation count / raw requested bytes / deallocations for one logical M2
   mutation or query operation (OD-M5-P7-008).

C. TRANSIENT TRANSACTIONAL ALLOCATION (M3)
   Allocation count / raw requested bytes / normalized live metrics for one
   accepted BookProjection::apply transaction and one baseline install
   (OD-M5-P7-009, OD-M5-P7-011), including candidate reconstruction and
   old-state destruction.

D. OUTPUT / ADAPTATION ALLOCATION (M4)
   Allocation count / raw requested bytes for adaptation, checked
   install/apply, snapshot construction, and serialization
   (OD-M5-P7-012), with owning-output lifecycles made explicit.

E. END-TO-END REPLAY ALLOCATION
   Aggregate and per-event (exact rational) allocation count / raw requested
   bytes for production CoreNormalizedReplay and AdapterWireReplay
   (OD-M5-P7-013).

F. BENCHMARK / INSTRUMENTATION OVERHEAD
   Harness-owned input preparation, pool construction, and the
   instrumentation's own bookkeeping. Excluded from every production bucket
   by bracket discipline (OD-M5-P7-007) and reported only as separately
   labelled calibration evidence; never subtracted from production metrics.
```

Every result names its bucket AND its measurement boundary
(`cxx_replaceable_global_new`, OD-M5-P7-004). A result that cannot be
attributed to one bucket is exploratory or invalid, never formal.

**Rationale:** The Phase-8 container decision must be able to see how much of
the observed cost is persistent container storage versus transaction
architecture versus output machinery. Attribution is the entire purpose of
Phase 7.

**Implementation constraint:** No production API, no production source hook,
no Core change. No optimization of any measured path. Report language is
limited to "On environment X, exact source/binary Y, workload Z, measured
result R through boundary B".

---

## OD-M5-P7-002 — Instrumentation isolation and two-lifetime model

**Question:** Where does instrumentation live, and how are tracking and
measurement separated?

**Decision:**

### Isolation

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
- Planned measurement executables (design only; not created by the merged
  methodology PR, and not yet implemented):
  - `bmd_projection_allocation_m2_m3` — M2 and M3 allocation cells;
  - `bmd_projection_allocation_m4` — M4 allocation cells (built only with
    `BMD_PROJECTION_BUILD_PROTO_ADAPTER=ON`, mirroring the Phase-6 M4
    fail-closed availability rule);
  - `bmd_projection_allocation_replay` — replay allocation workloads;
  - `bmd_projection_allocation_footprint` — persistent footprint experiment;
  - `bmd_projection_allocation_instrumentation_tests` — adversarial
    instrumentation validation (OD-M5-P7-020).

### Two-lifetime model (M5-P7-MR-003) — MANDATORY

Two independent controls exist. They are NOT the same flag:

```text
A. TRACKING / PROVENANCE ACTIVE
B. MEASUREMENT COUNTERS ACTIVE
```

Required semantics after instrumentation initialization:

- Every intercepted successful allocation records
  `pointer → exact raw requested size` in the provenance table regardless of
  whether a measurement bracket is active.
- Every intercepted deallocation resolves/removes pointer provenance
  regardless of whether a measurement bracket is active.
- ONLY when measurement counters are active: increment bracket allocation
  traffic; increment bracket deallocation traffic; update bracket-specific
  metrics.

Therefore:

```text
allocation outside bracket → tracked for future size provenance
                             → NOT counted as bracket allocation traffic

delete outside bracket     → removes provenance
                             → NOT counted as bracket deallocation traffic
```

Required lifecycle:

```text
instrumentation bootstrap (provenance table static storage; tracking begins
                           at the FIRST intercepted allocation, so allocations
                           before main are covered and later resolvable)
↓
provenance tracking active (permanent until process teardown)
↓
warmup / prepared-state pool creation (tracked, not counted)
↓
measurement bracket opens (A snapshot; counters zeroed)
↓
production operation
↓
measurement bracket closes (P/B snapshots; counters frozen)
↓
later pool/result destruction (provenance resolves; not counted)
↓
provenance remains correct
```

- Tracking MUST begin before any prepared state that may later be mutated or
  destroyed inside a measured bracket (M2 book nodes and M3 old-book maps are
  populated before their operation brackets; their in-bracket destruction must
  resolve exactly — M5-P7-MR-003 test cases 5 and 6).
- Pointer reuse must not observe stale entries: a deallocation removes its
  entry; a later allocation at the same address inserts a fresh entry. An
  allocation that finds an existing entry for its address is a sticky
  instrumentation ERROR (OD-M5-P7-019).
- The provenance table is static fixed-capacity storage (no heap allocation,
  so table setup cannot recurse or be counted); the instrumentation performs
  NO heap allocation at any time (thread_local POD counters; direct
  `std::malloc`/`std::free` calls for backing storage).

**Why process-global instrumentation is safe enough here:** process-global
operator overrides observe every allocation through that boundary in the
process, including the harness's. They are acceptable ONLY because:

1. each measurement executable is single-threaded, so the control block needs
   no synchronization and bracket counters cannot be polluted by concurrent
   work;
2. brackets are entered/left only on the main thread and only in the
   measurement loop;
3. the two-lifetime model separates what the instrument records (all
   observed traffic) from what a bracket attributes (only in-bracket
   traffic), so global visibility does not contaminate bracket-scoped
   attribution (OD-M5-P7-007);
4. the instrumentation itself performs no allocation inside a measured
   bracket (static table, POD counters);
5. the measurement boundary is explicitly scoped
   (`cxx_replaceable_global_new`): traffic through other channels is never
   claimed to be observed (OD-M5-P7-004).

Instrumentation is therefore NOT safe in multi-threaded executables, in
production, or in the Google Benchmark timing binary; all of those are out of
scope for Phase 7.

**Rationale:** This is the exact isolation model already reviewed and used for
M3/M4 allocation-failure sweeps, extended with the corrected two-lifetime
tracking model so pre-bracket allocations can be legitimately deallocated
inside a bracket with exact provenance.

---

## OD-M5-P7-003 — Complete allocation operator surface and backing arithmetic

**Question:** Which C++ allocation/deallocation forms must an instrumentation
executable handle, and how is backing storage arithmetic made safe?

**Decision:**

### Complete replaceable surface

Every Phase-7 instrumentation executable MUST override the complete C++20
replaceable global allocation/deallocation surface in its own executable
(never in a library), verified against [[new.delete.single]] and
[[new.delete.array]]:

```text
// throwing single-object
void* operator new(std::size_t size);
void* operator new(std::size_t size, std::align_val_t alignment);

// nothrow single-object
void* operator new(std::size_t size, const std::nothrow_t&) noexcept;
void* operator new(std::size_t size, std::align_val_t alignment,
                   const std::nothrow_t&) noexcept;

// throwing array
void* operator new[](std::size_t size);
void* operator new[](std::size_t size, std::align_val_t alignment);

// nothrow array
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept;
void* operator new[](std::size_t size, std::align_val_t alignment,
                     const std::nothrow_t&) noexcept;

// unsized single-object delete
void operator delete(void* ptr) noexcept;
void operator delete(void* ptr, std::align_val_t alignment) noexcept;

// sized single-object delete
void operator delete(void* ptr, std::size_t size) noexcept;
void operator delete(void* ptr, std::size_t size,
                     std::align_val_t alignment) noexcept;

// nothrow single-object delete
void operator delete(void* ptr, const std::nothrow_t&) noexcept;
void operator delete(void* ptr, std::align_val_t alignment,
                     const std::nothrow_t&) noexcept;

// unsized array delete
void operator delete[](void* ptr) noexcept;
void operator delete[](void* ptr, std::align_val_t alignment) noexcept;

// sized array delete
void operator delete[](void* ptr, std::size_t size) noexcept;
void operator delete[](void* ptr, std::size_t size,
                       std::align_val_t alignment) noexcept;

// nothrow array delete
void operator delete[](void* ptr, const std::nothrow_t&) noexcept;
void operator delete[](void* ptr, std::align_val_t alignment,
                       const std::nothrow_t&) noexcept;
```

**NOTE (M5-P7-MR-001):** the sized aligned forms take `(void*, std::size_t,
std::align_val_t)` — size BEFORE alignment — in that exact order, for both
single-object and array forms. The reversed order
`(void*, std::align_val_t, std::size_t)` is NOT a standard replaceable
declaration and MUST NOT appear.

Rules:

- **Size recovery is never trusted to sized delete alone.** The exact raw
  requested size is recorded at allocation time in the provenance table;
  sized delete receives a size argument that is used ONLY as a consistency
  check (mismatch → sticky instrumentation error, OD-M5-P7-019). Unsized and
  sized deletes both recover sizes from the table.
- **Sized-delete substitution:** the standard permits a sized delete call to
  be changed to the corresponding unsized call
  ([new.delete.single] paragraph 13). The instrumentation must therefore
  handle BOTH unsized and sized deallocation of every observed allocation;
  no metric may depend on sized delete actually being emitted.
- **Aligned allocation backing (M5-P7-MR-002):** aligned overloads allocate a
  backing block computed with checked arithmetic:

  ```text
  effective_payload = max(raw_requested_size, 1)
  header_size      = sizeof(void*)
  backing_size     = checked_add(checked_add(effective_payload,
                                             (alignment - 1)),
                                 header_size)
  ```

  Every addition is checked `std::size_t` arithmetic (explicit check or
  compiler checked-add builtin) performed BEFORE any malloc call or pointer
  arithmetic. `alignment - 1` is always representable (`std::align_val_t`
  holds a non-zero power of two). If `backing_size` cannot be represented:
  - throwing new → `throw std::bad_alloc{}`;
  - nothrow new → `return nullptr`;
  - the request is NOT inserted into provenance, does NOT increment
    successful allocation counters, does NOT create a pointer, and does NOT
    wrap.
  The aligned payload is placed inside the block (pointer arithmetic within
  the proven-allocated range, all offsets computed with checked arithmetic),
  and the original block pointer is stored in the header slot immediately
  preceding the aligned payload so the aligned delete path can recover and
  free it exactly.
- **Raw metric semantics are separate from backing allocation semantics**
  (M5-P7-MR-006): the metric records the caller's raw requested size (0 stays
  0); the backing block size (`max(raw,1)` normalization, alignment padding,
  header) is allocator-internal and is reported only as the diagnostic
  `instrument_backing_request_bytes`, never as production requested bytes.
- **Underlying allocator:** `std::malloc` / `std::free` (the established
  pattern). The malloc-level zero-size normalization (`malloc(1)`) is backing
  behavior only (OD-M5-P7-004).
- **No undefined behavior, no broken alignment:** every returned pointer
  satisfies the requested alignment; the aligned path is covered by the
  adversarial suite (OD-M5-P7-020, cases 7-12).
- **No recursive instrumentation allocation:** the instrumentation performs no
  heap allocation (static provenance table, thread_local POD counters);
  a recursion guard remains as defense-in-depth and routes any
  instrumentation-internal allocation around recording.
- **Fail-closed strategy:** if any allocation form is discovered to escape
  the overridden surface (an unknown-pointer delete, a table overflow, a
  sized-delete mismatch, a stale-entry collision, or a form not overridden
  being emitted), the affected metric class is marked INVALID/INELIGIBLE for
  the run (OD-M5-P7-005, OD-M5-P7-019). Never silently estimate. Unobserved
  channels (direct malloc/arena) are a documented boundary fact, not an error
  (OD-M5-P7-004).
- The surface must be verified for each measured target/toolchain combination
  by the adversarial validation suite (OD-M5-P7-020), including at least one
  over-aligned type per target and direct calls of every sized/unsized/
  nothrow/aligned overload pair.

**Rationale:** The corrected signatures match the authoritative standard;
fail-closed backing arithmetic prevents undersized storage and UB at the
extremes; separating raw semantics from backing semantics keeps the
production metrics truthful.

---

## OD-M5-P7-004 — Mandatory allocation metrics (raw requested semantics)

**Question:** What do `allocation_count` and `total_allocated_bytes` mean
exactly, and what is the measurement boundary?

**Decision (exact semantics):**

### Measurement boundary (M5-P7-MR-007)

Every Phase-7 allocation/live metric carries the explicit machine-readable
identity:

```text
allocation_boundary: cxx_replaceable_global_new
```

The formal boundary is: **C++ replaceable global-new observed requested
traffic**. `allocation_count`, `total_allocated_bytes`, deallocation
traffic, and tracked live metrics apply ONLY to traffic observed through
that boundary. Phase 7 makes NO claim of complete OS/process heap, complete
malloc traffic, RSS, or total physical memory. For the current production
M2 `std::map`/`std::vector` storage backed by the default `std::allocator`,
the standard allocator obtains storage through the replaceable `::operator
new` family ([allocator.members]), which is the documented standard/library
authority supporting coverage of that traffic; for M4/Protobuf paths, only
observed-subset consistency is claimable (OD-M5-P7-005).

### Exact metric semantics (M5-P7-MR-006)

- `allocation_count`: the number of successful allocation requests observed
  inside the measurement bracket through any overridden allocation operator.
  - A request is counted exactly once, at the moment it succeeds.
  - An allocation that fails (underlying malloc returns null → `bad_alloc`
    propagates, or nothrow form returns null) is NOT counted; the enclosing
    cell is marked ERROR (OD-M5-P7-019).
  - A request received with raw size 0 that succeeds counts as exactly 1
    allocation.
- `total_allocated_bytes`: the exact sum of the raw `std::size_t` size
  arguments RECEIVED by the intercepted replaceable allocation functions for
  successful allocations inside the measurement bracket.
  - A received raw size of 0 contributes **0 bytes** (count += 1, bytes += 0).
    The raw argument is the production requested-byte evidence; the fact
    that the backing path uses `malloc(1)` to satisfy the request is
    allocator-internal and is NOT part of this metric.
  - Alignment padding, backing headers, allocator chunk overhead, and the
    malloc-level zero-size normalization are excluded from
    `total_allocated_bytes`; if useful, a separate instrumentation diagnostic
    named `instrument_backing_request_bytes` records backing-level requests,
    and MUST NOT be conflated with the Phase-7 mandatory production metric.
- **Arrays:** do NOT assume `new T[n]` causes `operator new[]` to receive
  exactly `n * sizeof(T)`; array allocation overhead/cookies may affect the
  allocation-function argument. The instrumentation records the ACTUAL
  argument observed by the replacement function, faithfully, never a
  non-portable guessed size.
- `deallocation_count`: the number of successful deallocation calls observed
  inside the bracket through any overridden delete operator.
- `deallocated_bytes`: the sum of the provenance-recorded raw requested
  sizes of pointers freed inside the bracket. Available only when every freed
  pointer resolves in the provenance table (guaranteed by always-on tracking
  for observed allocations); an unknown-pointer delete makes the
  deallocated-bytes and live metric classes INELIGIBLE for the bracket while
  `allocation_count`/`total_allocated_bytes` remain valid (OD-M5-P7-019).
- **Denominator:** every cell reports its operation denominator exactly: one
  logical production operation (one `apply_level`, one `apply_updates` call,
  one `replace_all`, one query, one `BookProjection::apply`, one
  `install_baseline`, one adaptation call, one checked install/apply, one
  snapshot construction, one serialization, or one replay event). Replay
  aggregates are reported with `event_count`; per-event derived values are
  exact rationals (see below), NOT integer division.
- **Setup/warmup exclusion:** allocations performed outside the bracket
  (input preparation, pools, one full untimed warmup pass) are tracked but
  never counted (OD-M5-P7-002, OD-M5-P7-007, OD-M5-P7-014).
- **Reporting:** count and bytes are reported as exact values plus the number
  of confirming repetitions (OD-M5-P7-015). No statistical distribution is
  claimed for count/bytes.
- Mandatory metric set for every Phase-7 cell: `allocation_count`,
  `total_allocated_bytes`, and — for eligible cells — `deallocation_count`,
  `deallocated_bytes`, and the A/P/B live model of OD-M5-P7-005.

**Rationale:** Exact raw-argument semantics (zero means zero) and an explicit
scoped boundary prevent both over- and under-claiming, which is exactly what
Phase 8 must not inherit.

---

## OD-M5-P7-005 — Live bytes / peak live bytes semantics (A/P/B model)

**Question:** How are live bytes and peak live bytes measured, and when are
they truthful?

**Decision (M5-P7-MR-004, MR-005, MR-007):**

### A/P/B model

- `live_bytes` = sum of provenance-recorded raw requested sizes of currently
  outstanding observed allocations (boundary `cxx_replaceable_global_new`).
  Updated exactly at every successful observed allocation (+raw requested
  size, zero included) and every successful observed deallocation
  (−recorded raw size), regardless of bracket state (always-on tracking,
  OD-M5-P7-002). Checked `std::uint64_t` arithmetic; wrap → sticky INVALID
  (OD-M5-P7-019).
- Every measurement bracket records, at minimum:

  ```text
  A = live_bytes_before          (snapshot at bracket open)
  P = peak_live_bytes_absolute   (maximum live_bytes observed during the
                                  bracket, event-driven at every allocation
                                  and deallocation transition; no sampling,
                                  no interpolation)
  B = live_bytes_after           (snapshot at bracket close)
  ```

- **Persistent live change** is represented exactly as:

  ```text
  persistent_live_delta:
    sign:      negative | zero | positive
    magnitude: uint64
  ```

  computed as: `B > A` → {positive, B−A}; `B == A` → {zero, 0};
  `B < A` → {negative, A−B}. The entire difference is NEVER forced into a
  signed `int64_t`; magnitude is `uint64_t`.
- **Normalized transient metrics** (the primary per-operation transient
  evidence):

  ```text
  peak_above_entry = P − A
      (valid because P >= A: A is the live value at bracket open, and P is
       the maximum over the closed bracket that includes the open point)

  transient_excess_over_persistent = P − max(A, B)
      (valid because P >= A and P >= B; separates temporary working-set
       overhead from legitimate persistent output/state change)
  ```

  Formal per-operation comparison MUST use these normalized metrics, not raw
  process absolute peak alone. Absolute `live_bytes_before`,
  `peak_live_bytes_absolute`, `live_bytes_after` remain recorded for auditing.

### Legitimate persistent change (M5-P7-MR-004)

A positive delta is NOT automatically a leak. A negative delta is NOT
automatically an instrumentation error. Legitimate production cases:

```text
insert:            delta > 0
delete:            delta < 0
reset:             delta < 0
missing delete:    delta = 0
replace_all:       delta may be < 0 / = 0 / > 0
baseline install:  may change persistent state
owning adaptation result:  output may still be alive at B
snapshot result:           output may still be alive at B
```

The removed invalid rules were: universal `live_bytes_after ==
live_bytes_before` and "unsigned B − A with negative delta == instrumentation
error". Return-to-baseline is a valid check ONLY for a cell/lifecycle whose
normative expected net persistent change is zero (e.g., a pure query whose
owning output is later destroyed, or the post-destroy lifecycle check of an
owning-output cell).

### Owning-output lifecycle (B/D)

For operations returning owning values, two lifecycle points are specified:

```text
B:  operation has returned; the returned owning result is still alive.
D:  the returned owning result has subsequently been destroyed OUTSIDE the
    operation bracket while provenance tracking remains active.
```

Report `A`, `P`, `B`, and, when applicable, `post_destroy_live_bytes = D`
with `post_destroy_lifecycle_status` (destroyed | retained-with-reason). The
operation allocation/deallocation counters cover A→B only. Destruction B→D
must NOT be silently included in the operation's traffic; D is a separate
lifecycle validation (and is exactly the case where a return-to-baseline
check is legitimate: the owning-output lifecycle's normative net persistent
change after destruction is zero).

Applies at least to: `AdaptExchangeDepthSnapshot`, `AdaptDepthUpdate`,
`MakeLocalOrderBookSnapshot`, `SerializeSnapshot` fresh output, and the M2
owning-return queries `all_levels`/`top_levels`.

### Eligibility (fail-closed, scoped, per metric class)

`live_bytes_*`, `peak_live_bytes_absolute`, `peak_above_entry`,
`transient_excess_over_persistent`, `persistent_live_delta`,
`deallocated_bytes` are ELIGIBLE for a bracket only when ALL of:

1. no provenance-table overflow during the bracket (OD-M5-P7-019);
2. no unknown-pointer deallocation inside the bracket;
3. no live-bytes checked-arithmetic wrap during the bracket;
4. no stale-entry collision or sized-delete mismatch during the bracket.

Otherwise the metric class is reported `ineligible` with a machine-readable
reason code; `allocation_count`/`total_allocated_bytes` may remain eligible.

**Scope rule (M5-P7-MR-007):** eligibility proves internal consistency of the
OBSERVED subset only. It does NOT prove absence of direct `malloc`/`free`,
arena allocation, or other allocator channels in third-party code. For M4 /
Protobuf / Contracts runtime cells: tracked allocations balancing + no
unknown delete + lifecycle checks prove only that the traffic OBSERVED
through the replaceable global-new boundary is tracked consistently
end-to-end. There is no claim of complete process heap traffic, and no
eligibility condition depends on such a claim. If a future Phase needs
complete heap traffic, that requires a separate instrumentation boundary and
decision.

**Platforms:** live metrics are eligible on the three supported CI toolchains
(Ubuntu GCC, Ubuntu Clang, macOS AppleClang) subject to the rules above.
Values are environment-specific (OD-M5-P7-018). MSVC is not a CI target and
makes no eligibility claim.

**Unavailability:** when a metric is INELIGIBLE/UNAVAILABLE it is reported as
`ineligible` with a machine-readable reason code. It is NEVER substituted
with an estimate. There is no "estimated peak live bytes" metric.

**Rationale:** Peak-live evidence fabricated from incomplete deallocation
data would poison the Phase-8 decision; truthful absence is preferred over
plausible numbers; normalized quantities are the only honest basis for
per-operation comparison under differing prepared-state footprints.

---

## OD-M5-P7-006 — Persistent footprint semantics (non-additive models)

**Question:** How is the memory-footprint experiment for order-book depths
100 / 1,000 / 5,000 / 10,000 levels per side defined and reported?

**Decision:**

- **Executable:** `bmd_projection_allocation_footprint`, single-threaded,
  with the full corrected operator surface (OD-M5-P7-003) and two-lifetime
  tracking (OD-M5-P7-002).
- **Cells:** depths per side D ∈ {100, 1000, 5000, 10000}. For each D:
  1. after warmup, record snapshot `S_empty` = live_bytes with an empty
     `OrderBook` constructed with the fixed `NumericSpec` (this bracket also
     yields the fixed object/PIMPL footprint delta);
  2. populate bids only to D levels (deterministic price/quantity generator,
     same generator schema as Phase-6 M2 cells), snapshot `S_bids`; delta
     `S_bids − S_empty` = observed requested heap bytes for one side at D;
  3. populate asks to D levels, snapshot `S_both`; delta
     `S_both − S_bids` = observed requested heap bytes for the second side
     at D;
  4. destroy the book and record the post-destroy snapshot D
     (lifecycle validation: for the footprint experiment, the normative net
     persistent change after destruction is zero, so D must equal the
     pre-experiment baseline; failure → record INELIGIBLE with reason).
- **Reported per cell:**
  - `measured_requested_heap_bytes_total` = `S_both − S_empty`;
  - `measured_requested_heap_bytes_per_side_bids`,
    `measured_requested_heap_bytes_per_side_asks` (measured independently,
    never assumed equal);
  - `measured_bytes_per_level_per_side` = per-side delta / D, reported as an
    exact rational pair (numerator, denominator D) plus optional display-only
    decimal rendering — no rounding into a lossy float;
  - `fixed_object_footprint_bytes` = delta from "no book" to "empty book"
    (construction of `unique_ptr<Impl>` + empty maps), measured once per
    executable and reported once;
  - `empty_book_baseline_bytes` = `S_empty` relative to no-book baseline;
  - `allocator_instrumentation_baseline`: the instrumentation's own
    bookkeeping overhead, measured by a separate empty-bracket calibration
    record, reported separately and NEVER subtracted from the deltas.
- **All metrics carry `allocation_boundary = cxx_replaceable_global_new`**
  (OD-M5-P7-004): these are observed requested heap bytes through the
  replaceable global-new boundary, not RSS and not complete heap.
- **Four explicitly separated quantities (never mixed) (M5-P7-MR-010):**

  ```text
  A. MEASURED        observed requested allocation bytes (the deltas above —
                     what Phase 7 asserts as measured evidence);
  B. NODE STRUCTURAL MODEL — NON-ADDITIVE, explanatory/cross-check only.
     For std::map the allocation request already includes the implementation
     node object structure (links/key/value/padding). The node model is
     therefore NEVER added to measured requested bytes; adding it again would
     double count. Schema field:
     node_structural_model.non_additive = true.
  C. ALLOCATOR BACKING-OVERHEAD MODEL — ESTIMATED, environment/toolchain/
     allocator/size-class-specific, NON-FORMAL. It may estimate allocator
     overhead outside the raw request size (chunk headers, size classes).
     A universal fixed header size (e.g., "16-byte glibc header") is NOT
     assumed unless proven for the exact relevant allocation
     size/class/environment. Schema field:
     allocator_backing_model.evidence_class = estimated.
  D. OS RSS / PHYSICAL MEMORY — NOT MEASURED. Phase 7 performs no
     `/proc`-style RSS reads and makes no RSS claim.
  ```

- Schema/reporting must make it structurally impossible to present
  `measured request + node structural model` as a "total".
- **Population method:** populate via `replace_all` from prepared
  harness-owned vectors (built outside the bracket), so no intermediate
  mutation traffic pollutes the persistent delta.
- **Container note:** the experiment measures the production `std::map`
  implementation. The modeled quantities exist so Phase 8 can compare
  candidate containers against the same experiment; Phase 7 changes nothing.

**Rationale:** Bytes-per-level must come from a real measured delta; modeled
quantities must be identified as non-additive explanations or estimates, so
Phase 8 can neither double-count nor ignore allocator overhead.

---

## OD-M5-P7-007 — Baseline subtraction / snapshot discipline

**Question:** What exactly does "baseline subtraction" mean?

**Decision:**

- Every reported delta is specified as:

  ```text
  snapshot A          := live_bytes recorded at a defined stable point
  operation/object lifetime := the bracket between A and B
  snapshot B          := live_bytes recorded at the closing stable point
  delta formula       := exact comparison of A and B producing
                         persistent_live_delta {sign, magnitude} (uint64)
  ```

  There is NO unsigned `B − A` subtraction of this delta and NO rule that a
  negative delta is an instrumentation error (M5-P7-MR-004). Where a
  post-destroy lifecycle snapshot applies, `D` is recorded and reported
  separately (OD-M5-P7-005).
- Snapshots are reads of the tracked live-bytes counter; taking a snapshot
  performs no allocation and cannot corrupt the delta.
- Stable points: A is recorded immediately before the measured production
  operation (all harness input/pool/output-buffer allocation already done,
  pools referenced but not constructed inside the bracket); B is recorded
  immediately after the operation returns and its result has been consumed
  by the harness in a way that performs no allocation (result consumption
  discipline mirroring Phase-6: scalar/evidence fold only; if a harness
  must allocate to consume a result, the consumption is moved outside the
  bracket and B is taken before it). Owning returned values remain alive at
  B by definition; their destruction defines D (OD-M5-P7-005).
- The instrumentation must not allocate inside the bracket: counters are
  preallocated PODs; the provenance table is static storage
  (OD-M5-P7-002/019).
- **Tracking vs counting separation (M5-P7-MR-003):** allocations performed
  outside the bracket are TRACKED (provenance recorded) but never counted;
  deallocations inside the bracket of those pre-bracket allocations are
  counted as in-bracket deallocation traffic and resolve exactly against
  provenance (this is how M3 old-book destruction and M2 delete/reset cells
  report truthful deallocation evidence without unknown-pointer errors).
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
- **Pool footprint independence (M5-P7-MR-005):** prepared-state pools may
  legitimately increase absolute A/P values; the normalized per-operation
  quantities (`persistent_live_delta`, `peak_above_entry`,
  `transient_excess_over_persistent`) must be identical for the same
  operation under identical semantics/allocation behavior, regardless of the
  surrounding pool's absolute footprint. This is asserted adversarially
  (OD-M5-P7-020, case 20).

**Rationale:** Without named A/B/D, an exact delta representation, and the
tracking/counting separation, "subtract baseline" is meaningless; this rule
makes every delta auditable and prevents pool footprint from corrupting
per-operation evidence.

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
M2/all_levels/{0,8,100,1000,5000,10000}   (owning-output cell: B/D lifecycle)
M2/top_levels/{1,5,50}/{8,100,1000}       (owning-output cell: B/D lifecycle)
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
- `all_levels`/`top_levels` are owning-output cells: they report A/P/B and
  the post-destroy lifecycle snapshot D (OD-M5-P7-005).
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
  no `MinTime`; the normalized metrics are expected to be deterministic per
  cell (OD-M5-P7-015). The full 48 cells are therefore affordable and are
  the mandatory inventory (mirroring OD-M5-P6-028's completeness requirement).
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
  - `Reset`: deallocation-only cell (book nodes freed; no allocation; the
    freed nodes were allocated before the bracket and resolve exactly
    through always-on provenance).
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
     and (2) at equal depths, by the normalized `transient_excess_over_
     persistent` of the apply cells, and by the labelled proxy cells.
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
  the normalized metrics repeat exactly (OD-M5-P7-015); any drift fails the
  cell closed.
- **Old-state destruction:** where a previous baseline/preserved book exists
  (`AwaitingBridge` or `NeedsResync` re-install), its node destruction is
  inside the bracket (it is part of the production operation); those nodes
  were allocated before the bracket and resolve exactly through always-on
  provenance (OD-M5-P7-002/007). The cell's semantic precondition records
  which starting state was prepared.

**Rationale:** Baseline installation is the M3 transaction's sibling; its
boundary must be as precise as the apply boundary or Phase 8 cannot compare
recovery costs.

---

## OD-M5-P7-012 — M4 allocation coverage (scoped boundary)

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
  inside the bracket). Owning-output lifecycle B/D applies (OD-M5-P7-005).
- `AdaptDepthUpdate`: bracket = the adaptation call only; wire
  preconstructed outside; owning-output lifecycle B/D applies.
- `CheckedInstall`: the owner is pre-adapted outside the bracket; bracket =
  `install_into(projection)` (binding checks + production `install_baseline`).
- `CheckedApply`: owner pre-adapted outside; bracket =
  `apply_to(projection)` (binding checks + full accepted apply transaction).
- `MakeLocalOrderBookSnapshot/{Unlimited,Limited}`: bracket = the snapshot
  builder call; `SnapshotContext`/`SnapshotOptions` preconstructed outside;
  serialization excluded; the returned owning message defines the B/D
  lifecycle.
- `SerializeSnapshot/{FreshBuffer,ReusedBuffer}`: bracket = the
  serialization call; `FreshBuffer` is the formal primary, `ReusedBuffer`
  the optional diagnostic (OD-M5-P6-012); the returned owning buffer
  defines the B/D lifecycle.
- **Wire construction/setup allocations must never be attributed to
  adaptation:** a separately labelled diagnostic of Protobuf message
  construction is permitted but is not part of the formal M4 inventory
  (OD-M5-P6-011 preserved).
- **Availability:** M4 allocation executables are built only with
  `BMD_PROJECTION_BUILD_PROTO_ADAPTER=ON` and pinned Contracts bootstrapped;
  absence of the required M4 inventory fails Phase-7 validation closed
  (mirroring OD-M5-P6-013).
- **Measurement scope (M5-P7-MR-007):** all M4 metrics are scoped to
  `allocation_boundary = cxx_replaceable_global_new`. M4 live/peak
  eligibility proves internal consistency of the OBSERVED subset only
  (tracked allocations balancing, no unknown delete, lifecycle checks); it
  must never be labelled complete heap traffic, and it must not claim
  absence of direct malloc/arena channels in the Protobuf/Contracts runtime.
- **Protobuf runtime one-time initialization** is absorbed by the untimed
  warmup (OD-M5-P7-014); no measured bracket may contain first-use runtime
  initialization.

**Rationale:** Phase-6 already fixed these timing boundaries; Phase 7 must
reuse them verbatim so timing and allocation evidence describe the same
production paths, with the measurement boundary stated truthfully.

---

## OD-M5-P7-013 — Replay allocation workload (exact rationals)

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
- **Denominator and derived values (M5-P7-MR-008):** the pass reports exact
  aggregates `aggregate_allocation_count`, `aggregate_allocated_bytes`
  (plus eligible deallocation/live aggregates) and the exact `event_count`.
  Per-event derived quantities are represented losslessly as EXACT
  RATIONALS:

  ```text
  derived_per_event:
    numerator   = aggregate_total        (uint64)
    denominator = event_count            (uint64)
  ```

  Optional decimal rendering is display-only and derived from the exact
  rational. There is NO integer division and NO divisibility invariant: an
  aggregate of 3 over 2 events is represented as 3/2, never 1, and
  validators MUST NOT reject evidence merely because the aggregate is not
  divisible by the event count.
- **Setup/reset rules:** one full untimed warmup pass with discarded state;
  fresh production state per measured pass; post-pass final-state/checksum
  validation outside the bracket; pass failures fail the cell closed
  (mirroring the Phase-6 replay benchmark and latency-sampler discipline).
- **Live metrics:** replay passes report the A/P/B model and normalized
  quantities per pass (OD-M5-P7-005) with the same
  `cxx_replaceable_global_new` boundary scoping; owning intermediate
  outputs are governed by the same B/D lifecycle rules.

**Rationale:** Replay allocation is the only evidence that shows the
end-to-end production path's allocation profile per real event shape; it
must measure the production path only, and per-event derivation must be
lossless.

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
- The measurement counters are zeroed after warmup completes; no warmup
  allocation is ever counted. (Warmup allocations remain TRACKED for
  provenance so any of their later deallocations resolve exactly —
  OD-M5-P7-002.)
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
     buffers).
- No measured bracket may contain a first-use path; a cell that triggers a
  previously unseen one-time allocation path fails closed as
  instrumentation contamination (OD-M5-P7-015).

**Rationale:** One-time allocations would otherwise appear as fake
per-operation traffic and corrupt every delta.

---

## OD-M5-P7-015 — Repetition / determinism (normalized metrics)

**Question:** How are repetitions and determinism handled for allocation
measurements?

**Decision (M5-P7-MR-005, corrected):**

- **Normalized metric determinism:** under one fixed binary, one fixed
  environment, one fixed workload, and identical semantic preconditions,
  the following normalized operation metrics are expected to be
  deterministic and MUST match EXACTLY across identical repetitions within
  a run and across separate process invocations for formal evidence:

  ```text
  allocation_count
  total_allocated_bytes
  deallocation_count / deallocated_bytes        (where eligible)
  persistent_live_delta                          {sign, magnitude}
  peak_above_entry
  transient_excess_over_persistent
  footprint deltas                               (OD-M5-P7-006)
  ```

- **Repetition requirement:** each cell executes a fixed number of measured
  repetitions (minimum 3 per formal run; fixed iteration counts per
  execution, no `MinTime`), and the record asserts exact equality of the
  normalized metrics across them. The full run is executed 3 times
  (separate process invocations of the same binary) with cross-run
  equality asserted for formal evidence.
- **Absolute values are contextual:** `live_bytes_before`,
  `peak_live_bytes_absolute`, `live_bytes_after` are recorded for auditing
  but are NOT required to be equal across runs if the harness/pool
  lifecycle legitimately differs (different pre-existing pool footprints
  may legally shift absolute values; the normalized quantities must not
  change — asserted adversarially, OD-M5-P7-020 case 20). This is NOT
  instrumentation contamination and MUST NOT be reported as such.
- **Instrumentation contamination definition:** any of — nonzero result in
  a zero-allocation control cell; normalized-metric variation across equal
  preconditions; provenance-table overflow; unknown-pointer delete;
  stale-entry collision; sized-delete mismatch; nested bracket attempt
  (OD-M5-P7-019).
- **Variance handling:** observed variation in a NORMALIZED metric across
  equal preconditions is instrumentation contamination or stateful-drift:
  the run FAILS CLOSED; there is no averaging, no median, and no tolerance
  band for count/bytes/normalized-live metrics. If a measured path
  legitimately cannot satisfy deterministic formal semantics (no such path
  is known in the current production implementation), that metric/cell is
  marked INELIGIBLE with a recorded reason — never averaged away.
- **Phase-6 timing statistics are NOT mechanically reused:** allocation
  cells do not report mean/stddev/percentiles of counts; they report exact
  normalized values plus the number of confirming repetitions. Timing
  statistical methodology remains the property of Phase-6 timing artifacts
  only.

**Rationale:** Count determinism is the strongest available check that the
bracket discipline holds; statistical machinery would mask real
contamination, while absolute-baseline variation is a legitimate pool-lifecycle
effect that must not be mislabelled.

---

## OD-M5-P7-016 — Machine-readable evidence schema

**Question:** What machine-readable schema records Phase-7 evidence?

**Decision (corrected fields):**

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
  allocation_boundary               "cxx_replaceable_global_new"
  allocation_count
  total_allocated_bytes             (sum of raw requested size arguments;
                                     a received 0 contributes 0)
  deallocation_count
  deallocated_bytes                 (present only when eligible)
  live_bytes_before                 (A)
  peak_live_bytes_absolute          (P)
  live_bytes_after                  (B)
  persistent_live_delta             { sign: negative|zero|positive,
                                      magnitude: uint64 }
  peak_above_entry                  (P − A)
  transient_excess_over_persistent  (P − max(A, B))
  live_metric_eligibility           eligible | ineligible:<reason code>
  post_destroy_live_bytes           (owning-output cells; = D)
  post_destroy_lifecycle_status     destroyed | retained:<reason>
  baseline_definition               {snapshot_a, snapshot_b, delta_formula}
  calibration_record                (separate; never subtracted)
  repetitions                       exact-equality confirmation count
  result_payload_sha256
  ```

- **Replay records additionally carry:** `aggregate_allocation_count`,
  `aggregate_allocated_bytes`, `event_count`, and lossless derived
  per-event values:

  ```text
  derived_per_event: { numerator: uint64, denominator: uint64 }
  ```

  (decimal rendering is display-only; no integer division; no
  divisibility requirement).
- **`M5_PHASE7_FOOTPRINT_RECORD_V1` fields:** depth, generator identity,
  `measured_requested_heap_bytes_total`,
  `measured_requested_heap_bytes_per_side_bids/asks`,
  `measured_bytes_per_level_per_side` (exact numerator/denominator pair),
  `fixed_object_footprint_bytes`, `empty_book_baseline_bytes`,
  post-destroy lifecycle snapshot, model fields:

  ```text
  node_structural_model:   { non_additive: true, description, toolchain }
  allocator_backing_model: { evidence_class: "estimated",
                             description, toolchain/allocator/size-class
                             scope; no universal fixed header size without
                             proof for the exact relevant size/class/
                             environment }
  rss:                     "not_measured"
  ```

  plus eligibility flags and provenance as above. Validators must reject
  any presentation of `measured + node_structural_model` as a total.
- **Fail-closed validation:** a Phase-7 Python validator
  (proposed `scripts/benchmark_phase7.py`, mirroring
  `scripts/benchmark_phase6.py`) with deterministic Python tests and C++
  tests for the record construction. Validators reject: unknown schema,
  missing mandatory fields, ineligible-metric substitution, absent
  baseline definition, formal records with dirty source, payload SHA
  mismatch, missing workload identity, missing `allocation_boundary`,
  rational records that truncate (integer division), and any
  `heap_complete = true`-style completeness claim.
- Schema duplication is avoided by reusing the Phase-6 provenance block and
  workload identity; Phase-7 adds only the allocation/footprint payload.

**Rationale:** Phase 6 established the provenance machinery; Phase 7 adds a
truthful, boundary-scoped allocation payload without forking the existing
contracts.

---

## OD-M5-P7-017 — Formal evidence eligibility

**Question:** When may a Phase-7 run be called a formal baseline, and what
is it a baseline OF?

**Decision (corrected identity, M5-P7-MR-007/009):**

- **Formal identity:** a formal Phase-7 result is labelled exactly:

  ```text
  FORMAL CURRENT-PRODUCTION
  C++ REPLACEABLE-GLOBAL-NEW ALLOCATION/MEMORY CHARACTERIZATION
  IN THE PINNED PHASE-7 CANONICAL TOOLCHAIN ENVIRONMENT
  ```

  It is NOT described as complete OS/process heap, RSS, all malloc
  traffic, or total physical memory. Persistent footprint for the M2
  std::allocator-backed `std::map` may be interpreted under the documented
  standard/global-new coverage ([allocator.members]); M4 values remain
  scoped to observed global-new traffic unless a later separate decision
  establishes a broader allocator boundary.
- **All conditions required:**
  1. evidence produced by the Phase-7 canonical Release runner
     (OD-M5-P7-023): `/src` HEAD == recorded source SHA; `/src` worktree
     clean under the repository's defined source set; repository source
     compiled DIRECTLY from `/src` (single formal source model — no
     copied source tree, no source-content manifest); exact canonical
     image/toolchain contract;
  2. Release; sanitizers off; LTO state explicit and recorded;
  3. exact binary SHA-256 recorded and rehashable (mirroring OD-M5-P6-022);
  4. exact source SHA recorded;
  5. canonical workload identities present: every record carries the accepted
     Phase-6 workload ID + workload-spec SHA-256 (and fixture/generator
     identity for replay records);
  6. complete required inventory: all OD-M5-P7-008 M2 cells; all 48 M3
     accepted-apply cells plus the classification cells; all OD-M5-P7-012 M4
     families; both Spot and USD-M replay records; the four footprint depths;
  7. determinism confirmation: exact equality of the normalized metrics
     across the required repetitions and 3 separate process invocations
     (OD-M5-P7-015);
  8. no instrumentation error/overflow/table-overflow/nested-bracket/
     unknown-pointer-delete/stale-collision/sized-delete-mismatch in the run
     (OD-M5-P7-019);
  9. eligible metric set marked per record (live/peak per OD-M5-P7-005,
     scoped boundary rule included);
  10. provenance wrapper complete and payload SHA binding valid;
  11. every record carries `allocation_boundary = cxx_replaceable_global_new`.
- Anything else is **exploratory**. Exploratory runs are permitted for
  development and are labelled `evidence_class = exploratory`; an exploratory
  run can never later be re-labelled formal. A run that fails any formal
  condition downgrades to exploratory or invalid, never silently formal.

**Rationale:** This mirrors and extends the Phase-6 formal-baseline
eligibility (OD-M5-P6-022/028) with instrumentation-specific gates and a
truthful boundary identity.

---

## OD-M5-P7-018 — Toolchain/environment interpretation

**Question:** What environment identity does formal Phase-7 evidence
require, and what may be compared across environments?

**Decision:**

- **Canonical formal identity:** formal Phase-7 evidence is generated in the
  canonical pinned Quality environment identity of INFRA-TC-001 (the
  repository-owned `scripts/quality.sh` container contract:
  `ubuntu:24.04` pinned by digest, clang 18.1.3, pinned historical Ubuntu
  archive snapshot), executed through the Phase-7 canonical runner
  (OD-M5-P7-023), which reuses that identity without redefining Quality
  semantics. A formal run records the canonical toolchain contract identity
  in its build identity.
- **Other environments:** Ubuntu GCC, Ubuntu Clang (native), and macOS
  AppleClang runs are supported and recorded with full environment identity,
  but they are labelled `evidence_class = exploratory` (environment-
  specific evidence). They are citable supporting evidence, never the
  canonical formal baseline.
- **Cross-environment comparison rules:**
  - Byte values (allocation counts, raw byte totals, live/peak bytes,
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

**Question:** How do counters, the tracking table, and sizing arithmetic
fail closed?

**Decision (M5-P7-MR-002, MR-003, MR-004 corrected):**

- **Separate arithmetic domains:** allocation SIZING arithmetic
  (backing-block computation) and METRIC counter arithmetic are separate
  domains with separate protections. Counter overflow checks do NOT
  protect sizing, and sizing checks do NOT protect counters.
- **Sizing arithmetic (M5-P7-MR-002):** `backing_size =
  checked_add(checked_add(max(raw,1), (alignment−1)), sizeof(void*))`
  with checked `std::size_t` arithmetic BEFORE malloc or pointer
  arithmetic (OD-M5-P7-003). On unrepresentable size: throwing new →
  `std::bad_alloc`; nothrow new → `nullptr`; NO provenance insertion, NO
  counter increment, NO pointer creation, NO wrap.
- **Integer widths:** all metric counters (`allocation_count`,
  `total_allocated_bytes`, `deallocation_count`, `deallocated_bytes`,
  `live_bytes`, `peak_live_bytes_absolute`) are `std::uint64_t`;
  `persistent_live_delta.magnitude` is `std::uint64_t`.
- **Counter overflow handling:** every increment uses checked arithmetic
  (explicit check or compiler checked-add builtin). On overflow: a sticky
  `overflowed` flag is set, the affected counter freezes at
  `UINT64_MAX`, and the affected metric class of the ENTIRE run is marked
  INVALID. A wrapped counter can never silently produce a plausible PASS.
- **Allocation failure interaction:** if the underlying `std::malloc`
  returns null, `std::bad_alloc` propagates (or null is returned by the
  nothrow form); the failed request is not counted and not recorded. If
  this occurs inside a measured bracket, the cell is marked ERROR (no
  partial result is recorded). Failures outside brackets behave like any
  normal program failure.
- **Tracking failures (two-lifetime model):**
  - provenance-table overflow (static fixed-capacity table full): the new
    pointer is still returned (production must proceed), the entry is not
    recorded, and the live-metric class is sticky-INELIGIBLE for the run;
    count/bytes remain valid;
  - stale-entry collision (allocation address already has a live entry):
    sticky instrumentation ERROR (live class INVALID);
  - unknown-pointer delete (freed pointer absent from the table): the
    deletion is not counted; live metric class INELIGIBLE for the bracket
    (fail closed; unobserved-channel traffic is a documented possibility,
    never silently attributed);
  - sized-delete size mismatch with the recorded size: run-level
    instrumentation ERROR.
- **Live-bytes model failures:** live-bytes arithmetic wrap, or any
  eligibility-condition failure listed in OD-M5-P7-005, makes the affected
  metric class INELIGIBLE with a reason code. There is NO rule that a
  negative `persistent_live_delta` is an error (M5-P7-MR-004): sign
  negative is a legitimate measured outcome.
- **Recursion guard:** instrumentation bookkeeping performs no heap
  allocation (static table, thread_local POD counters); a recursion guard
  flag remains as defense-in-depth and routes any instrumentation-internal
  allocation around recording.
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
plausibly is worse than no instrument, and sizing overflow is a
memory-safety failure mode distinct from counter overflow.

---

## OD-M5-P7-020 — Adversarial validation

**Question:** What deterministic tests prove the measurement mechanism
itself?

**Decision (rewritten; mandatory adversarial suite in a dedicated
single-threaded test executable
`bmd_projection_allocation_instrumentation_tests`):**

1. **Plain new/delete count and raw bytes:** N plain `new`/`delete` pairs
   of size S inside a bracket → `allocation_count == N`,
   `total_allocated_bytes == N*S` (N and S as literals).
2. **Array form without cookie assumptions:** `new[]`/`delete[]` pairing
   counted exactly once per request; byte total asserted against the
   ACTUAL argument observed by the replacement function (recorded
   faithfully), never against a guessed `n*sizeof(T)`.
3. **Direct `::operator new(0)`:** success → count 1, raw bytes 0; delete
   resolves provenance.
4. **Direct `::operator new[](0)`:** success → count 1, raw bytes 0;
   delete[] resolves provenance.
5. **Nothrow success:** `new (std::nothrow)` success path counted exactly
   once with faithful raw bytes.
6. **Nothrow failure:** simulated nothrow failure (backing failure) returns
   nullptr; no successful count, no provenance insertion, no record.
7. **Over-aligned allocation address correctness:** an over-aligned type
   (e.g., `alignas(64)`) round-trips: payload address satisfies the
   alignment, count/bytes faithful, delete resolves the recorded size.
8. **Exact unsized aligned delete:** `operator delete(void*, align_val_t)`
   (and array form) resolve provenance exactly.
9. **Exact sized aligned scalar delete signature:** direct call
   `::operator delete(ptr, size, std::align_val_t{a})` resolves with a
   size-consistency check against the recorded raw size.
10. **Exact sized aligned array delete signature:** direct call
    `::operator delete[](ptr, size, std::align_val_t{a})` resolves with a
    size-consistency check against the recorded raw size.
11. **Aligned backing-size overflow (throwing):** a near-`SIZE_MAX`
    aligned request → `std::bad_alloc`; no successful record, no
    provenance insertion, no counter increment, no UB.
12. **Aligned backing-size overflow (nothrow):** the same near-`SIZE_MAX`
    request via the nothrow form → `nullptr`; no successful record, no
    provenance insertion, no counter increment, no UB.
13. **Pre-bracket allocation / in-bracket delete provenance:** allocate
    outside the bracket, delete inside the bracket → the delete resolves
    the exact pre-recorded size and counts as in-bracket deallocation
    traffic only.
14. **Outside-bracket alloc+delete:** provenance updated on both; bracket
    counters remain exactly zero.
15. **Pointer reuse:** alloc → free → alloc at the same address observes
    fresh provenance (no stale size, no collision flag).
16. **Persistent positive delta (insert profile):** a deterministic
    profile that retains a new allocation → `persistent_live_delta` =
    {positive, exact magnitude}; B > A accepted as legitimate.
17. **Persistent negative delta (delete/reset profile):** a deterministic
    profile that releases a pre-bracket allocation inside the bracket →
    `persistent_live_delta` = {negative, exact magnitude}; NOT an
    instrumentation error.
18. **Zero persistent delta:** a missing-delete-style no-op profile →
    `persistent_live_delta` = {zero, 0}.
19. **Owning output lifecycle:** an operation returning an owning value →
    B includes the alive result (positive delta allowed); post-destroy
    snapshot D equals the pre-bracket A (normative net change zero after
    destruction); B→D destruction is NOT in operation counters.
20. **Pool-footprint independence:** the same measured operation under two
    different pre-existing pool absolute footprints: absolute A/P/B may
    differ; `persistent_live_delta`, `peak_above_entry`, and
    `transient_excess_over_persistent` are IDENTICAL.
21. **Known transient peak profile:** a deterministic ascent/descent
    profile with a hand-computed maximum → `peak_live_bytes_absolute`,
    `peak_above_entry`, and `transient_excess_over_persistent` equal the
    exact expected values; two deallocation orders asserted.
22. **Nested measurement rejection:** opening a bracket inside a bracket
    fails closed (asserted defined abort/rejection).
23. **Counter overflow:** seed a counter near `UINT64_MAX` via the test
    hook, force the overflow, assert the INVALID flag and frozen value.
24. **Provenance table overflow:** force live concurrency past the static
    table capacity → live metric class INELIGIBLE while count/bytes
    remain valid; subsequent production traffic still returns valid
    pointers.
25. **Unknown-provenance delete (standard-conforming; M5-P7-RR-001):** a
    TEST-ONLY provenance-removal seam in
    `bmd_projection_allocation_instrumentation_tests` (conceptual name
    `forget_provenance_for_test(void*)`; the exact C++ spelling may vary,
    the following semantics are NORMATIVE) deterministically constructs the
    REAL unknown-provenance branch of the replacement delete
    instrumentation using a LEGALLY MATCHED C++ allocation/deallocation
    pairing:
    (a) measurement counters inactive, tracking/provenance active;
    (b) `void* p = ::operator new(K)` for a fixed hand-known K > 0;
    (c) verify `p` has a normal provenance entry;
    (d) `forget_provenance_for_test(p)` intentionally removes that
        provenance entry WITHOUT freeing `p`, updating instrumentation
        bookkeeping consistently so the synthetic test setup itself leaves
        no false stale live record;
    (e) open the measurement bracket;
    (f) `::operator delete(p)` — the replacement delete executes against a
        pointer legitimately returned by the corresponding replacement
        allocation function, but provenance is intentionally absent;
    (g) assert the exact fail-closed unknown-provenance behavior required
        by OD-M5-P7-019: the deletion is not counted, the
        live/deallocated-byte metric class is INELIGIBLE/INVALID for the
        bracket, while `allocation_count` / `total_allocated_bytes`
        semantics remain unaffected by this synthetic missing-provenance
        event;
    (h) underlying storage is actually freed exactly once.
    This test invokes NO undefined behavior: the pairing is legal C++
    (matching replaceable allocation/deallocation functions) and no
    cross-boundary `malloc` → `operator delete` construction appears
    anywhere.
26. **Direct malloc/free bypass proof:** a measured helper performs direct
    `malloc`/`free` inside an otherwise measured path → the
    `cxx_replaceable_global_new` metric does NOT see those bytes; the
    validator/report still describes the metric truthfully as scoped; any
    hypothetical `heap_complete = true` claim is rejected. This proves
    measurement SCOPE — traffic outside
    `allocation_boundary = cxx_replaceable_global_new`; it does NOT test
    unknown-pointer delete handling (that is case 25).
27. **Replay rational case:** aggregate 3 over 2 events is represented as
    the exact rational {3, 2}; validators must not reject and must not
    truncate to 1.
28. **Repeated normalized-metric determinism:** the same deterministic
    profile yields identical normalized metrics across 3 separate process
    invocations (or subprocess re-exec) of the same binary.
29. **M2 delete/reset old allocations:** book nodes allocated before the
    bracket and freed inside the bracket resolve provenance exactly — no
    unknown-pointer error.
30. **M3 old-book destruction:** the M3 apply transaction's old-book map
    destruction inside the bracket resolves provenance exactly — no
    unknown-pointer error.

**Independence rule:** tests must not simply re-execute a second copy of the
counter logic. Expected values are hand-computed literals in the test;
at least one test cross-checks a container-driven profile (e.g., a
deterministic `std::vector` growth sequence) against an independently
hand-derived expected total computed from the documented growth policy.

**Rationale:** Phase 8 will trust these numbers; the mechanism must be
proven at least as hard as the measurements it produces, including the
corrected aligned-delete signatures, sizing overflow, the two-lifetime
model, and the scoped boundary.

---

## OD-M5-P7-021 — Phase-8 handoff

**Question:** What exactly must Phase 7 hand to Phase 8?

**Decision:**

Phase 7 MUST NOT choose the new container, define a migration gate, or
recommend a candidate. It hands to Phase 8:

1. **Persistent storage evidence:** observed requested heap bytes for the
   production `std::map` book at 100/1,000/5,000/10,000 levels per side,
   per-side and total, with fixed-object footprint, empty-book baseline,
   bytes-per-level as exact rationals, the NON-ADDITIVE node structural
   model, and the ESTIMATED allocator backing model (OD-M5-P7-006).
2. **M2 mutation evidence:** allocation/deallocation profiles for
   insert/update/delete/missing-delete, batch updates, replace_all, and
   queries, with the corrected A/P/B model per cell (OD-M5-P7-008).
3. **M3 accepted-apply evidence:** the full 48-cell matrix plus
   classification cells, showing how accepted-apply allocation scales with
   depth at B=0 and with batch size, using normalized metrics
   (`persistent_live_delta`, `peak_above_entry`,
   `transient_excess_over_persistent`) (OD-M5-P7-009, OD-M5-P7-011).
4. **M3 transaction evidence:** the documented production transaction
   structure with source locations, plus the labelled Component/Proxy
   diagnostic cells (never an exact decomposition; OD-M5-P7-010).
5. **M4 evidence:** adaptation, checked install/apply, snapshot
   construction, serialization — scoped to
   `cxx_replaceable_global_new`, with owning-output B/D lifecycles, and
   with the explicit statement of what is NOT proven (complete heap
   traffic) (OD-M5-P7-012).
6. **Replay evidence:** aggregate and exact-rational per-event derived
   allocation for CoreNormalizedReplay and AdapterWireReplay, Spot and
   USD-M (OD-M5-P7-013).
7. **Environment discipline:** canonical formal environment identity,
   exploratory environment records, and the cross-environment comparison
   rules (OD-M5-P7-018, OD-M5-P7-023).
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
   `scripts/benchmark-allocation.sh` local pattern, the Phase-7 canonical
   runner `scripts/benchmark-allocation-formal.sh` per OD-M5-P7-023, and
   the Phase-7 Python validator);
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

## OD-M5-P7-023 — Formal canonical Release execution mechanism

**Question:** Which exact host command produces formal Phase-7 evidence, on
which source, built by which toolchain, without weakening the canonical
Quality trust boundary?

**Decision (M5-P7-MR-009; conceptual contract — no scripts are created or
modified in this docs-only PR):**

The future Phase-7 implementation MUST provide ONE repository-owned host
entrypoint, EXACTLY:

```text
bash scripts/benchmark-allocation-formal.sh
```

This is the SOLE public host command that may produce formal Phase-7
allocation/memory evidence. There is NO alternative-name escape hatch ("name
may differ if repository convention favors another name" is REJECTED); no
second alias is formal authority. The script is NOT created in this docs-only
PR, but its future semantics are fixed by this decision record. The host
entrypoint must NOT change the meaning of `bash scripts/quality.sh` and must
NOT add arbitrary-command execution to `scripts/quality.sh`.

### Host mode (trust boundary)

The host mode must:

1. reject ambient internal/container/source/work selection variables using
   the same fail-closed trust-boundary philosophy as INFRA-TC-001
   (no caller-selectable container mode, no caller-selectable roots);
2. accept no arbitrary shell command from the caller;
3. validate the authoritative Quality contract (`.toolchain/quality.env`
   and the canonical container contract);
4. validate Docker Engine runtime identity (same backend rule as
   INFRA-TC-001; Podman and podman-docker/libpod backends rejected);
5. derive the same authoritative pinned base reference from the exact baked
   contract;
6. build/use the SAME repository-owned canonical toolchain Dockerfile and
   `.toolchain/quality.env` identity (no duplicate independent image
   digest, no duplicate independent snapshot identity);
7. enter ONLY a fixed Phase-7 internal entrypoint;
8. mount canonical source read-only at the fixed path `/src`;
9. use a fresh/ephemeral fixed `/work` for build/output (build trees never
   persist across runs);
10. never emit or imitate `CANONICAL QUALITY: PASS` — Phase-7 evidence is
    not the canonical Quality gate.

### Internal mode (trust boundary)

The internal Phase-7 mode must be entered only through an explicit trusted
positional flag from its host mode. There is NO ambient container-mode
selector and NO caller-controlled source path, work path, or command;
internal roots are fixed (`/src`, `/work`). The internal mode re-proves the
image boundary (baked toolchain contract == mounted source toolchain
contract) before formal build work. It must NOT reuse `quality.sh`'s
trusted internal flag by calling `quality.sh` directly from an untrusted
host path.

### Source provenance and compiled-source binding

There is EXACTLY ONE formal source model. `/src` is the sole source root and
`/work` is the sole build/output root:

```text
SOURCE ROOT:  /src   (repository checkout, mounted read-only, .git retained,
                      CMake source root, the source repository files are
                      compiled from)
BUILD ROOT:   /work  (fresh/ephemeral build and output state only)
```

The future formal runner MUST configure/build DIRECTLY from the read-only
mounted Git checkout:

```text
cmake -S /src -B /work/<fixed-build-dir> ...
```

Exact lower-level CMake arguments may be defined during implementation where
already constrained by this contract, but the source-root semantics are NOT
optional. Formal Phase-7 execution MUST NOT:

- copy repository source into `/work`;
- rsync repository source into `/work`;
- tar/extract repository source into `/work`;
- configure CMake from a copied source tree;
- claim Git identity from `/src` while compiling repository source from
  another tree.

`/work` may contain: CMake build trees; generated build files; generated
configure headers; Conan-generated build/dependency state; binaries;
Phase-7 evidence payloads; temporary build artifacts. It MUST NOT become a
second repository source root.

**NO SOURCE-CONTENT MANIFEST IS REQUIRED.** The previous optional
"materialize a source copy into `/work` and bind it to `/src` via a
content-manifest/hash" path is UNDERDEFINED and is now REJECTED and removed
completely; the ambiguity is eliminated rather than solved with a second
manifest protocol. Because repository source is compiled DIRECTLY from
`/src`, the formal source binding is:

```text
Git provenance source == CMake source root == repository source bytes
consumed by the build, namely /src
```

No `/src → /work` repository-source copy exists, so a source-content-mismatch
negative test is NOT implemented as "compare two source copies"; it is
implemented as the single-source negative proofs in the negative-test
section below.

Git provenance is obtained from that SAME `/src`. The runner must not
manufacture a SHA from another checkout.

It is forbidden to combine: git identity from `/src` + unbound different
source bytes in `/work`.

### Formal eligibility additions

Formal eligibility (OD-M5-P7-017) therefore additionally requires, checked
BEFORE configure:

- `/src` HEAD == recorded source SHA (HEAD(`/src`) == recorded
  `source.git_sha`);
- `/src` worktree clean — tracked/index/worktree/untracked state inspected
  per the existing accepted repository provenance discipline; the existing
  Phase-6 clean-source contract is NOT weakened;
- CMake source root == `/src`; build/output root under `/work`;
- exact canonical image/toolchain contract;
- Release; sanitizers off; explicit LTO state; exact binary SHA.

### Trust boundary statement

The Phase-7 formal runner is a SEPARATE fixed-purpose evidence runner. It
reuses the accepted INFRA-TC environment identity/helpers where appropriate
but does not redefine canonical Quality semantics. It must not introduce:
arbitrary container execution; ambient internal selection; arbitrary source
roots; arbitrary work roots; mutable toolchain identity; duplicate
independent image digest; duplicate independent snapshot identity. If
sharing helper logic would otherwise require copying parsing semantics,
reuse existing production helper scripts rather than create a divergent
second parser.

### Threat-boundary clarity

The formal runner binds normal repository/formal evidence identity under the
accepted repository/INFRA-TC-001 threat model. Phase 7 is NOT broadened into
protection against a malicious privileged host capable of mutating
bind-mounted files during execution — that is outside the already accepted
INFRA-TC threat model. All existing fail-closed checks are retained
unchanged; nothing above weakens them.

### Required future negative tests (implementation contract)

The future implementation must have tests proving:

```text
dirty /src                       → formal rejected BEFORE configure
unexpected HEAD(/src)            → formal rejected BEFORE configure
caller attempts alternative source root → impossible / rejected
caller attempts alternative work root   → impossible / rejected
CMake source root other than /src → formal runner rejects / cannot
                                    construct a formal invocation
canonical contract/image mismatch → rejected
direct internal-mode host invocation → rejected
ambient internal/source/work variables → rejected
non-Release                     → formal rejected
sanitizer-enabled formal run    → rejected
incorrect binary SHA binding    → evidence rejected
arbitrary caller command        → impossible / rejected
```

A source-content-mismatch negative proof is required and is implemented in
the single-source model as the first four rows above (dirty `/src`,
unexpected HEAD, no alternative source root, no alternative work root) — NOT
as a comparison of two source copies. No redundant file-manifest protocol is
invented.

The corrected contract answers, BEFORE implementation: which exact host
command produces formal evidence; which source tree is compiled; where
HEAD/dirty facts are obtained; how source identity and the bytes compiled
are bound (single source root `/src`); which exact pinned environment is
used; and why the mechanism does not weaken `scripts/quality.sh`.

**Rationale:** A formal baseline whose provenance is weaker than the
canonical Quality trust boundary could not be cited in the Phase-8/9
decision; this mechanism borrows the accepted identity without diluting
it.

---

## Critical measurement model (summary)

The normative model established by OD-M5-P7-002..007:

```text
TRACKING LIFETIME vs MEASUREMENT BRACKET
  provenance tracking: ON from the first intercepted allocation,
  permanently; every successful observed allocation records
  pointer→exact raw requested size; every observed deallocation resolves
  provenance. Measurement counters: ON only inside a bracket; only
  in-bracket traffic is attributed. Pre-bracket allocations deallocated
  inside a bracket resolve exactly. Static provenance table; no
  instrumentation heap allocation (OD-M5-P7-002/003).

ALLOCATION COUNTING / RAW BYTES
  allocation_count: successful in-bracket allocation requests, one per
  request (raw size 0 still counts 1). total_allocated_bytes: exact sum
  of RAW size arguments received (0 contributes 0 bytes). Backing
  normalization (malloc(1), alignment padding, headers) is a separate
  diagnostic, never the production metric. Array-new arguments recorded
  faithfully, never guessed. Boundary: cxx_replaceable_global_new —
  no complete-heap claim (OD-M5-P7-004).

LIVE / PEAK (A/P/B)
  A = live_bytes_before; P = peak_live_bytes_absolute (event-driven);
  B = live_bytes_after. persistent_live_delta = {sign ∈
  {negative,zero,positive}, magnitude uint64} — positive is not a leak,
  negative is not an error. peak_above_entry = P − A;
  transient_excess_over_persistent = P − max(A, B); normalized metrics
  are the primary per-operation evidence; absolute values are auditing
  context. Owning outputs add the post-destroy snapshot D (lifecycle
  validation; normative net change after destruction is zero).
  Eligibility fails closed; scope is observed-subset only
  (OD-M5-P7-005).

FOOTPRINT
  empty → bids-only → both-sides snapshots at 100/1k/5k/10k per side;
  fixed-object footprint; bytes-per-level as an exact rational;
  post-destroy lifecycle snapshot. MEASURED vs NODE MODEL (non-additive)
  vs ALLOCATOR MODEL (estimated, size-class/toolchain-specific) vs RSS
  (not measured) strictly separated; measured+node totals are
  structurally rejected (OD-M5-P7-006).

BASELINE SUBTRACTION
  every delta = exact A/B comparison producing persistent_live_delta
  {sign, magnitude}; snapshots allocate nothing; harness/pool allocation
  outside brackets (tracked, not counted); calibration reported, never
  subtracted; warmup absorbs one-time effects (OD-M5-P7-007/014).

WORKLOAD ATTRIBUTION
  reuse Phase-6 workload identities verbatim; production buckets A–E of
  OD-M5-P7-001; Component/Proxy diagnostics labelled and never an exact
  decomposition; reference/oracle machinery never inside a production
  bracket; replay per-event values are exact rationals
  (OD-M5-P7-008/009/010/012/013).
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

## Corrected-contract self-review

The corrected contract was challenged against the mandated scenarios; each
resolves without ambiguous or false evidence:

```text
A. map node allocated before bracket, deleted inside bracket
   → two-lifetime model: node tracked at population, its in-bracket
   delete resolves exact provenance and counts as deallocation traffic
   only (OD-M5-P7-002/007; adversarial cases 13, 29).

B. M3 old book allocated before bracket, destroyed on commit
   → same mechanism; adversarial case 30.

C. insert leaves more live memory
   → persistent_live_delta = {positive, magnitude}; NOT a leak
   (OD-M5-P7-005).

D. reset leaves less live memory
   → persistent_live_delta = {negative, magnitude}; NOT an error
   (OD-M5-P7-005; adversarial case 17).

E. snapshot returns owning output alive at operation return
   → B includes the alive output; D recorded post-destruction; B→D
   excluded from operation counters (OD-M5-P7-005/012; adversarial
   case 19).

F. large prepared-state pool surrounds one operation
   → absolute A/P/B shift legally; normalized metrics identical;
   asserted adversarially (OD-M5-P7-005/007/015; adversarial case 20).

G. aligned request near SIZE_MAX
   → checked sizing arithmetic fails BEFORE malloc; throwing → bad_alloc,
   nothrow → nullptr; no record, no counter, no pointer, no UB
   (OD-M5-P7-003/019; adversarial cases 11-12).

H. operator new receives size 0
   → count 1, raw bytes 0; backing malloc(1) is a separate diagnostic
   (OD-M5-P7-004; adversarial cases 3-4).

I. array-new allocation function receives implementation overhead
   → the ACTUAL observed argument is recorded faithfully; no guessed
   size (OD-M5-P7-004; adversarial case 2).

J. Protobuf performs hypothetical direct malloc/free
   → boundary is cxx_replaceable_global_new; those bytes are not seen
   and completeness is never claimed; scoped metrics stay truthful
   (OD-M5-P7-004/005/012; adversarial case 26).

K. replay aggregate=3 / events=2
   → exact rational {3, 2}; no truncation; validators must accept
   (OD-M5-P7-013/016; adversarial case 27).

L. formal Release run requires git SHA but /work has no .git
   → git identity from read-only /src; /src IS the compiled source root
   (single formal source model; no copy exists) (OD-M5-P7-023).

M. caller tries to enter Phase-7 internal container mode directly
   → rejected: no ambient internal selection; only the host mode's
   trusted positional flag enters the fixed internal entrypoint
   (OD-M5-P7-023).

N. engineer copies /src into /work
   → forbidden for formal evidence: formal execution never compiles from a
   copied source tree; /work is build/output state only
   (OD-M5-P7-023).

O. caller supplies another source root or another work root
   → impossible/rejected: fixed `/src`, fixed `/work` (OD-M5-P7-023).

P. `/src` dirty, or HEAD(/src) differs from the recorded source SHA
   → formal rejected BEFORE configure (OD-M5-P7-023).

Q. unknown-provenance delete: `::operator new(64)` records provenance;
   test-only seam removes it; `::operator delete(p)` runs
   → legal C++ pairing; replacement delete executes its real
   unknown-provenance branch; exact fail-closed INELIGIBLE/INVALID
   behavior; freed exactly once; no UB (OD-M5-P7-019/020; case 25).

R. `p = malloc(64); free(p)`
   → global-new metric does not see the traffic; no claim that the
   unknown-delete path was tested (OD-M5-P7-004/020; case 26).
```

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

- C++ working draft: [new.delete.single](https://eel.is/c++draft/new.delete.single),
  [new.delete.array](https://eel.is/c++draft/new.delete.array),
  [basic.stc.dynamic](https://eel.is/c++draft/basic.stc.dynamic)
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
