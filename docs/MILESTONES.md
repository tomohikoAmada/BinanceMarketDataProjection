# Milestones

## M0 Repository Foundation — COMPLETE

### Goal

Create a private, repeatably buildable, testable, installable C++20 library baseline that future
Gateway and History/Replay consumers can reuse.

### Inputs

None — this is the bootstrap milestone.

### Outputs

- CMake/Ninja presets and target-local warnings, sanitizers, clang-tidy, and coverage options.
- Repository-local Conan 2 workflow locked to GoogleTest 1.17.0 and Google Benchmark 1.9.5.
- Minimal version API, unit and header self-containment tests, and benchmark smoke target.
- Install/export package and an isolated staged-install consumer test.
- GitHub Actions across Ubuntu GCC, Ubuntu Clang, and macOS AppleClang, plus ASan/UBSan and benchmark
  jobs.
- README, architecture boundaries, agent rules, milestones, open questions, and four ADRs.

### Acceptance gates

Debug and Release builds/tests, ASan, UBSan, format, tidy, benchmark smoke, package installation, and
the independent consumer must pass. Required GitHub Actions jobs must be green, the Draft PR must
remain unmerged, the worktree must be clean, and M0 must contain no projection business behavior.
TSan must be exercised when the platform runtime supports it or its real limitation documented.

### Non-goals

No decimal parsing, fixed-point domain types, order book, snapshot/diff application, sequence policy,
gap/resync logic, market-state calculations, protobuf adapter, gRPC, network transport, JSON parser,
Python binding, persistence, Gateway/History runtime, strategy, risk, or trading behavior.

### Definition of done

All M0 deliverables are committed to `feat/m0-repository-foundation`, local mandatory checks pass,
required CI is green, a Draft PR targets the bootstrap-only `main`, and this status is changed to
`COMPLETE` in a separate final commit followed by another green CI run.

Local clang-tidy may be skipped when the host toolchain lacks it; CI clang-tidy is the mandatory
gate. The version header is generated from CMake; no hand-maintained duplicate version string exists.

## M1 Numeric and Domain Primitives — COMPLETE

### Goal

Establish the exact, deterministic numeric boundary used by future projection behavior without
implementing order-book or market-state logic.

### Outputs

- Explicit decimal scales from 0 through 18 and distinct signed 64-bit price and quantity units.
- Strict Contracts-compatible parsing for prices, quantities, and positive quantities.
- Exact rescaling and deterministic formatting with retained source fractional digit counts.
- Stable error codes and prechecked arithmetic that never relies on signed overflow or rounding.
- Boundary, roundtrip, public-header, and 20,000-case deterministic property validation.
- Clang-only libFuzzer harness, seed corpus, blocking CI smoke job, and staged-install consumer use.
- M1 numeric semantics documentation and accepted fixed-point representation decision.

### Acceptance status

Implementation, required local checks, and external code review are complete. External code review
concluded with no blocking correctness findings. M1 is approved for merge and is marked complete.

### Non-goals

No order book, price-level map, snapshots, diff application, sequencing, gap/resync policy,
market-state calculations, Protobuf adapter, networking, host runtime, Python binding, symbol
filters, signed decimals, persistence, threading, logging, strategy, or trading behavior.

## M2 Order Book Core — COMPLETE

### Goal

Implement a deterministic, single-writer, market-by-price order book with absolute-quantity
semantics, zero-quantity deletion, bid/ask ordering, best-level queries, top-N output, batch
updates, and atomic full-book replacement.

### Outputs

- `BookSide`, `BookLevel`, `LevelUpdate`, `LevelChange`, and `OrderBook` public API.
- PIMPL-isolated `std::map` storage with bid descending / ask ascending ordering.
- Absolute quantity replacement, zero-quantity deletion, missing-delete no-op.
- Batch update with ordered application and same-price last-write-wins.
- Atomic `replace_all` with last-write-wins per side and strong exception guarantee.
- Best bid/ask, quantity-at-price, top-N, and full-level-copy queries.
- Crossed and locked book acceptance without rejection or auto-resolution.
- Independent vector-based reference model for property and fuzz validation.
- Model-based libFuzzer harness and deterministic property tests.
- M2 semantics documentation and updated ADR-0003.

### Acceptance status

Implementation, unit tests, property tests, fuzzing, and external code review are complete.
M2 passed external code review with no blocking correctness, determinism, portability, or
validation findings. M2 is approved for merge and is marked complete.

### Non-goals

No sequence validation, update IDs, gap detection, resynchronization, snapshot contract, protobuf
adapter, networking, Gateway runtime, History runtime, Python binding, matching engine, strategy,
persistence, or trading behavior.

## M3 Sequence and Projection State — COMPLETE

Implementation status: **COMPLETE**

### Goal

Define market-specific sequencing, bootstrap validation, gap detection, reset, and the synchronized
projection lifecycle while deterministically applying normalized absolute-quantity depth batches to
the M2 order book.

### Implementation deliverables

- Strongly typed unsigned Binance update IDs and valid-by-construction inclusive update ranges.
- Explicit Spot and USD(S)-M perpetual sequence-policy kinds, matching the frozen Contracts market
  identifiers.
- Separate Spot interval continuity and USD-M previous-final (`pu`) continuity classifiers.
- Domain-only baseline and depth-batch views with no wire or runtime dependency.
- `AwaitingBaseline`, `AwaitingBridge`, `Synchronized`, and `NeedsResync` lifecycle states.
- Stable apply results, deterministic gap evidence, reset, and synchronization-aware const queries.
- Strong exception safety for baseline installation and incremental sequence/book commit.
- Complete transition/unit matrices, an independent property model, deterministic replay tests,
  allocation-failure tests, and a model-based M3 fuzz harness.
- M3 architecture design and ACCEPTED ADR-0005, with the independent external review result
  recorded in the design documents.

COIN-M was reviewed as future-compatibility evidence but is not in the current M3 public surface
because the frozen Contracts baseline does not identify a COIN-M market.

### Design status

Architecture review: **APPROVED**.

Implementation review: **APPROVED**. Blocking findings: **0**.

ADR-0005 is **ACCEPTED**.

The M3 public API, production source, unit/state/property/replay/allocation-failure validation,
model-based fuzz harness, seed corpus, and staged-install consumer are implemented on `main`.
External implementation code review approved head
`7606a60bbb2d2a192f6c0259942174fbd49847ba` with no blocking findings, and CI run `31083008166`
passed 8/8 jobs for that exact head.

PR #6 is **MERGED** with squash merge
`39b34dc3a2fd5784f6a53d5e39c80e56be42355c`. Main CI run `31088396997` passed 8/8 jobs.
M3 is complete on `main`; M4 and later milestones remain separate future work.

### Non-goals

M3 does not own snapshot download, WebSocket transport, network-event buffering, reconnect, Host
queues, Gateway session identity, timestamps, threads, locks, logging, telemetry, persistence, or
History storage. M4 owns Protobuf/wire adaptation and snapshot contract production; M6 owns Gateway
runtime integration. Derived market prices, matching, orders, strategy, risk, and trading remain out
of scope.

## M4 Snapshots and Protobuf Boundary — COMPLETE

Implementation status: **COMPLETE**

### Acceptance status

Design status: **APPROVED / MERGED**.

ADR-0006: **ACCEPTED**.

External architecture review: **APPROVED**. Round 1 findings: **3 CLOSED**. Architecture blockers:
**0**.

Independent Implementation Review: **APPROVED**.

Implementation blockers: **0**. C-M4-001: **CLOSED / SATISFIED**. OD-M4-001: **CLOSED**.
Contracts integration: **VERIFIED**. M4 is **COMPLETE** on `main`.

Reviewed Head: `390cdceb013bc05878db090bdedc40068c03c79c`.
Reviewed CI: `31193311386` — 16/16 PASS.

### Merge completion record

- Pull request: #8 — Implement M4 snapshots and Protobuf boundary
- Final approved PR head: `efddcd2fbe6901a7506ae84aee5586be773a2835`
- Squash merge commit: `ac780d9eb7b49ff20a6b3b4bee6a993b51b70af4`
- Merge time: `2026-08-08T05:39:08Z`
- Main CI run: `31242162782`
- Main CI event: `push`
- Main CI result: `16/16 PASS`
- M4 implementation status: `COMPLETE`

The three deferred P2 findings below remain DEFERRED / NON-BLOCKING. M5 design assigns
M4-IIR-3 to M5 corpus work and defers M4-IIR-1/M4-IIR-2 to maintenance; none are closed here.

### Deferred P2 findings

| ID | Area | Finding | Status |
|---|---|---|---|
| M4-IIR-1 | `tests/cmake/check_m4_lock.cmake` | Lock-drift test proves required Contracts and Protobuf identities but does not assert the entire allowable dependency recipe set | DEFERRED / NON-BLOCKING |
| M4-IIR-2 | shared duplicate-symbol audit | Shared-mode audit checks consumer and Contracts shared library but does not separately `nm` the installed ProtoAdapter library | DEFERRED / NON-BLOCKING |
| M4-IIR-3 | M4 fuzz corpus / quality-input coverage | Initial corpus seeds are simple labels; fuzz bytes do not currently drive quality flags / HostQualityFact inputs | DEFERRED / NON-BLOCKING |

These P2 items do not block M4 merge. They may be addressed later as test hardening,
maintenance, M5 preparation, or future quality follow-up.

### Goal

Define an optional Protobuf adapter boundary that converts pinned Contracts wire messages into
owning M1/M2/M3 inputs and converts `BookProjection` plus explicit Host context into deterministic
`LocalOrderBookSnapshot` messages without introducing Protobuf into Core.

### Design deliverables

- Separate optional adapter target and one-way dependency direction into Core.
- Audited Contracts C++ artifact acquisition with separate schema fingerprint and package
  revision/version identities.
- Explicit inbound `ExchangeDepthSnapshot` and `DepthUpdate` conversion.
- Owning wrappers bound to `NumericSpec` and sequence policy that expose checked M3 invocation and
  keep span-based views private.
- Typed, deterministic validation and projection-binding errors.
- Explicit Host-owned identity, producer, time, depth-limit, gap, and quality context.
- State-specific `LocalOrderBookSnapshot` eligibility and visibility rules.
- Deterministic `GapInfo` to `GapDescriptor` mapping and historical-gap policy.
- Separate Host/Core quality domains plus owning inbound wire-quality sidecars.
- Compatibility, packaging/install, downstream-consumer, test, property, and fuzz plans.
- Accepted ADR-0006 for the separate Core/Protobuf adapter boundary.

### Design status

External architecture review Round 2: **APPROVED**. Round 1 findings are **3 CLOSED** and
architecture blocking findings are **0**.

The merged C-M4-001 prerequisite at `67ee1bf69fad980d114cfa278c3a6ffe310a4d7a`
provides `binance-market-data-contracts-cpp/0.1.0` and
`BinanceMarketDataContracts::Protobuf`. Projection pins Conan RREV
`7fd3efe3d289462fb16c78ffeced1682`, validates the approved schema/runtime metadata at configure
time, and implements the optional `BinanceMarketDataProjection::ProtoAdapter` component without
changing Core. The formal Contracts Package Revision remains `NOT_FORMALLY_ASSIGNED` until release.

### Non-goals

No Contracts change, gRPC runtime, Gateway lifecycle, network ownership, system-time read,
generated-code copy, or M5/M6 work is included. `MarketStateSnapshot` generation and derived market
calculations remain deferred unless separately designed and approved.

## M5 Differential Validation and Performance — APPROVED / MERGED / IN PROGRESS

Implementation status: **IN PROGRESS**

### Design status

The M5 architecture is designed in
`docs/M5_DIFFERENTIAL_VALIDATION_AND_PERFORMANCE_DESIGN.md` and ADR-0007 is **ACCEPTED**.
The design covers layered differential validation with operation-result observation
(OperationObservation model), canonical replay fixtures with explicit canonical text format
rules, determinism and cross-compiler semantic manifests with artifact fan-in transport,
replay/differential fuzzing, benchmark methodology, allocation/memory characterization, and the
benchmark-only container spike with explicit decision criteria.

Independent architecture review: **APPROVED** (focused re-review of corrected head
`9fff05ca8333d89d28d89c794d65255b56578715`; reviewed CI `31245814229` — PASS).
Findings M5-AR-001 through M5-AR-010: **CLOSED**. P0: **0**. P1 Design: **0**.
No M5 production change or container migration is authorized by the design.

### Pre-implementation decisions

OD-M5-001 and OD-M5-002 are **CLOSED**. Implementation authorization decisions and CI policy
are recorded in `docs/M5_PREIMPLEMENTATION_DECISIONS.md`. OD-M5-003 remains SPIKE-RESOLVABLE.
M5 implementation is authorized. Phases 1, 2, and 3 are complete and merged; later
phases remain separate.

Phase 1 canonical replay infrastructure is **COMPLETE / MERGED** (PR #11, merge
`5e8629a7ff825f8ea941304d9b09be1670643e8a`, post-merge main CI `31264500905` — PASS 16/16). It
covers canonical replay grammar and byte/UTF-8 validation, manifest and exact-log SHA-256
identity, normalized operation loading, deterministic tiny fixtures, structured parser
diagnostics, and the explicit offline Recorder materializer/bootstrap contract.

Phase 2 (independent differential oracle: R1 ReferenceDecimal promoted, R4 ReferenceAdapter,
neutral ReplayDriver, OperationObservation, semantic checkpoints, first-divergence diagnostics
with layer attribution) is **COMPLETE / MERGED** (PR #12, merge
`75c619dd683ff2a3893f9535e206231e7bfecc41`, final review APPROVED, P0: 0, P1: 0, post-merge main
CI `31315421548` — PASS 16/16). M5-P2-IR-001 through M5-P2-IR-007 and M5-P2-RR-001 are CLOSED;
M5-P2-IR-008 and M5-P2-IR-009 remain DEFERRED / NON-BLOCKING.

Phase 3 is **COMPLETE / MERGED** via PR #13 (final approved Head
`a8e4ccfc31efd4e67bc10cf0ac9ad2a99faa8354`; exact-head CI `31491615547` — PASS 16/16; P0: 0;
P1: 0; squash merge `473a907eba2001d18926c57d6c8d16b10c7505be`), rebased onto the accepted M3
Spot successor-coverage semantics (ADR-0008). Deterministic 2,048-event
small-tier Spot and USD-M workloads, per-event scaled differential comparison, bounded observation
retention, stable first-divergence diagnostics, the independent offline Recorder Raw-v1
materializer, and validator-enforced medium lifecycle validity are implemented. The mandatory
Spot 100k corpus (`M5-REC-SPOT-BTCUSDT-V1`) is validated PASS (exact-next bridge
`U=98288147168 u=98288147175` against `L=98288147167` under ADR-0008; 100,001 Applied, final
Synchronized, zero gaps). The mandatory
USD-M 100k corpus (`M5-REC-USDM-BTCUSDT-V1`) is validated PASS (bridge Applied / Synchronized,
100,001 Applied, final Synchronized, zero gaps). The Phase-3 corrections (M5-P3-IR-001 through
M5-P3-IR-005) are recorded; M5-P3-IR-001's historical contains-`L` restoration was itself
superseded by ADR-0008 acceptance. Residual M5-P3-RR2-002 remains PARTIALLY CLOSED / ACCEPTED
NON-BLOCKING P2. See `docs/M5_PHASE3_DETERMINISTIC_REPLAY.md`.
Phase 4 is IMPLEMENTED / PENDING INDEPENDENT REVIEW in branch
`feat/m5-cross-compiler-semantic-manifests`: canonical OperationObservation serialization
(schema v1), semantic SHA-256 digests, portable manifest v1, manifest producer, fail-closed
Python comparator, three-cross-compiler artifact fan-in, `m5-replay` Debug/Release
determinism, and `m5-semantic-compare` blocking job. See
`docs/M5_PHASE4_CROSS_COMPILER_SEMANTIC_MANIFESTS.md`.
No production-code change.

### M3 Spot successor-coverage correction (2026-08-10)

The M3 Spot bootstrap successor-coverage correction (PR #14, ADR-0008) is **MERGED**.
Independent focused re-review of implementation head
`5195a5cf639989ef073d908dfbf5ec5be1e3cc40` was **APPROVED** (P0: 0, P1: 0, blocking findings:
0; findings M3-SC-RR-001 through M3-SC-RR-005: **CLOSED**); exact-head CI run `31446514958`
passed 16/16. ADR-0008 is **ACCEPTED**, superseding only the Spot-bootstrap contains-`L`
portion of ADR-0005; USD-M semantics are unchanged. PR #14 merged at main
`8bc71f2ae457cf3d15a9dcb4ea659a9c3f85a569` (squash merge, 2026-08-11).

The M5 Phase-3 continuation (PR #13) was rebased onto the corrected main and the mandatory
Spot corpus gate was re-evaluated: under the accepted successor-coverage rule the pinned
in-window Spot archive's exact-next candidate (`U = L + 1`) is a valid bootstrap bridge, and the
Spot 100k corpus is validated PASS. At that time PR #13 remained OPEN / DRAFT pending independent
review; it has since been independently approved and merged (see Phase 3 acceptance above). This
branch changed no M5 Phase-3 production materializer dependency: the correction is test/tool-only
on top of the accepted main semantics.

### Deferred M4 P2 findings

The three deferred M4 P2 findings remain open and non-blocking. M5 design dispositions:
M4-IIR-1 and M4-IIR-2 are deferred to maintenance; M4-IIR-3 is incorporated into M5 corpus work.
These are design decisions only and close nothing.

### Non-goals

M5 does not implement live networking, Gateway runtime, gRPC, queues, backpressure, reconnect,
strategy, risk, trading, persistence, Python binding, or production container migration.

## Development map

1. **M0 Repository Foundation** — Build, test, installation, CI, and governance baseline.
2. **M1 Numeric and Domain Primitives — COMPLETE** — Fixed-point
   types, quantity/price representation, decimal parsing, exact formatting, property tests, and fuzz
   validation.
3. **M2 Order Book Core — COMPLETE** — Deterministic book storage, level management, depth operations.
4. **M3 Sequence and Projection State — COMPLETE** —
   Market-specific sequencing, bootstrap validation, gap detection, reset logic, and synchronized
   projection lifecycle.
5. **M4 Snapshots and Protobuf Boundary — COMPLETE** — Optional
   wire-format adapter outside Core; snapshot production boundary. Merged through PR #8 at
   `ac780d9eb7b49ff20a6b3b4bee6a993b51b70af4`; main CI `31242162782` — 16/16 PASS.
6. **M5 Differential Validation and Performance — APPROVED / MERGED / IN PROGRESS** —
   Replay/differential/fuzz validation; benchmark with representative workloads.
7. **M6 Gateway Integration** — Production host embedding surface; live ingestion.
8. **M7 Platform Hardening and Acceptance** — Harden architectures, toolchains; end-to-end acceptance.

### Cross-cutting concerns

- **Determinism, property tests, and fuzz** are required from M1 onward, not deferred wholesale.
- **Python Binding** is an optional History Track; not a required milestone deliverable.
- **Container choice** is a performance spike within M5; not a standalone milestone. Migration
  requires evidence, an explicit decision record/ADR, full regression, and independent review.
- Detailed API design belongs in per-milestone documents written at the start of each phase.

## Core architecture principles

- C++20.
- Deterministic — all inputs explicit; no system clock, random, or ambient state.
- Single Writer — host preserves ordering per projection instance.
- Core/Wire Separation — protobuf messages do not enter Core.
- Live/Replay Same Core — identical ordered inputs produce identical outputs.
- Strategy Independent — no trading, risk, or strategy logic.
