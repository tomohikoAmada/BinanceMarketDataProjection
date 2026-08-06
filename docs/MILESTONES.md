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

## M3 Sequence and Projection State — DESIGN APPROVED; IMPLEMENTATION IN REVIEW

Implementation status: **IN EXTERNAL CODE REVIEW**

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

Architecture and sequence policy design passed external review round 2 with no remaining blocking
findings.

ADR-0005 is ACCEPTED.

The M3 public API, production source, unit/state/property/replay/allocation-failure validation,
model-based fuzz harness, seed corpus, and staged-install consumer are implemented on
`feat/m3-sequence-projection-state`. Local validation passed subject to the repository's explicit
AppleClang clang-tidy/libFuzzer skips. Pull-request CI and external implementation code review are
still required; M3 is not complete or approved for merge.

### Non-goals

M3 does not own snapshot download, WebSocket transport, network-event buffering, reconnect, Host
queues, Gateway session identity, timestamps, threads, locks, logging, telemetry, persistence, or
History storage. M4 owns Protobuf/wire adaptation and snapshot contract production; M6 owns Gateway
runtime integration. Derived market prices, matching, orders, strategy, risk, and trading remain out
of scope.

## Development map

1. **M0 Repository Foundation** — Build, test, installation, CI, and governance baseline.
2. **M1 Numeric and Domain Primitives — COMPLETE** — Fixed-point
   types, quantity/price representation, decimal parsing, exact formatting, property tests, and fuzz
   validation.
3. **M2 Order Book Core — COMPLETE** — Deterministic book storage, level management, depth operations.
4. **M3 Sequence and Projection State — DESIGN APPROVED; IMPLEMENTATION IN REVIEW** —
   Market-specific sequencing, bootstrap validation, gap detection, reset logic, and synchronized
   projection lifecycle.
5. **M4 Snapshots and Protobuf Boundary** — Wire-format adapter outside Core; snapshot production.
6. **M5 Differential Validation and Performance** — Replay/differential/fuzz validation; benchmark
   with representative workloads.
7. **M6 Gateway Integration** — Production host embedding surface; live ingestion.
8. **M7 Platform Hardening and Acceptance** — Harden architectures, toolchains; end-to-end acceptance.

### Cross-cutting concerns

- **Determinism, property tests, and fuzz** are required from M1 onward, not deferred wholesale.
- **Python Binding** is an optional History Track; not a required milestone deliverable.
- **Container choice** is a performance spike within M5; not a standalone milestone.
- Detailed API design belongs in per-milestone documents written at the start of each phase.

## Core architecture principles

- C++20.
- Deterministic — all inputs explicit; no system clock, random, or ambient state.
- Single Writer — host preserves ordering per projection instance.
- Core/Wire Separation — protobuf messages do not enter Core.
- Live/Replay Same Core — identical ordered inputs produce identical outputs.
- Strategy Independent — no trading, risk, or strategy logic.
