# Milestones

## M0 Repository Foundation — IN PROGRESS

### Scope

Create a private, repeatably buildable, testable, installable C++20 library baseline that future
Gateway and History/Replay consumers can reuse.

### Deliverables

- CMake/Ninja presets and target-local warnings, sanitizers, clang-tidy, and coverage options.
- Repository-local Conan 2 workflow locked to GoogleTest 1.17.0 and Google Benchmark 1.9.5.
- Minimal version API, unit and header self-containment tests, and benchmark smoke target.
- Install/export package and an isolated staged-install consumer test.
- GitHub Actions across Ubuntu GCC, Ubuntu Clang, and macOS AppleClang, plus ASan/UBSan and benchmark
  jobs.
- README, architecture boundaries, agent rules, milestones, open questions, and four ADRs.

### Acceptance criteria

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

Current acceptance note: required GitHub Actions checks pass, including clang-tidy. The repository's
complete local `scripts/verify.sh` gate remains pending because the current macOS host has no
`clang-tidy` executable and the M0 download policy forbids installing one from an additional source.

## Development map

1. **M0 Repository Foundation** — Build, test, installation, CI, and governance baseline.
2. **M1 Fixed-Point Numeric Core** — Validate and implement exact internal numeric representation.
3. **M2 Order Book Core** — Deterministic book storage and operations.
4. **M3 Sequence and State Machine** — Market-specific sequencing, gap, and resynchronization rules.
5. **M4 Market State Projection** — Deterministic derived market-state values.
6. **M5 Protobuf Contract Adapter** — Translate versioned wire contracts outside Core.
7. **M6 Determinism and Differential Validation** — Replay/differential/fuzz validation.
8. **M7 Container and Performance Decision** — Measure and choose containers with representative
   workloads.
9. **M8 Python Binding and History Integration** — Bind the stable Core for historical workflows.
10. **M9 Gateway Embedding Interface** — Define the production host integration surface.
11. **M10 Platform Hardening** — Harden supported architectures and toolchains.
12. **M11 Acceptance Candidate** — End-to-end acceptance candidate and release readiness.
