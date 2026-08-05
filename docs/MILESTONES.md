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

## M1 Numeric and Domain Primitives — IN PROGRESS — IMPLEMENTED, REVIEW PENDING

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

Implementation and required local checks are complete. M1 deliberately remains **IN PROGRESS —
IMPLEMENTED, REVIEW PENDING** until the Draft PR receives external code review and the normal merge
process finishes. It must not be marked complete from the implementation branch.

### Non-goals

No order book, price-level map, snapshots, diff application, sequencing, gap/resync policy,
market-state calculations, Protobuf adapter, networking, host runtime, Python binding, symbol
filters, signed decimals, persistence, threading, logging, strategy, or trading behavior.

## Development map

1. **M0 Repository Foundation** — Build, test, installation, CI, and governance baseline.
2. **M1 Numeric and Domain Primitives — IN PROGRESS — IMPLEMENTED, REVIEW PENDING** — Fixed-point
   types, quantity/price representation, decimal parsing, exact formatting, property tests, and fuzz
   validation.
3. **M2 Order Book Core** — Deterministic book storage, level management, depth operations.
4. **M3 Sequence and Projection State** — Market-specific sequencing, gap detection, reset logic.
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
