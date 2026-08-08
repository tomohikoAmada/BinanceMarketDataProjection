# Changelog

All notable changes will be recorded here.

## [Unreleased]

### Added

- M5 architecture approval recorded: independent M5 architecture review **APPROVED**
  (focused re-review of corrected head `9fff05ca8333d89d28d89c794d65255b56578715`,
  reviewed CI `31245814229` — PASS, 16/16). Findings M5-AR-001 through M5-AR-010:
  **CLOSED**. P0: **0**. P1 Design: **0**. ADR-0007: **ACCEPTED**. M5 design:
  **APPROVED / PENDING MERGE**. M5 implementation remains **NOT STARTED / NOT
  AUTHORIZED**, blocked by OD-M5-001 and OD-M5-002 (separate pre-implementation
  decision task required). M6 remains NOT STARTED.

- M5 differential validation and performance design: layered differential oracle
  (ADR-0007 ACCEPTED), canonical versioned replay
  event-log grammar with ADAPTER_METADATA/HOST_QUALITY quality-domain separation and
  adapter dimension scoping, canonical text format rules (UTF-8/LF/whitespace/token
  semantics), OperationObservation differential model (observable operation results
  plus semantic checkpoints), cross-job semantic manifest transport architecture
  with artifact fan-in and fail-closed behavior, dataset size tiers, determinism and
  cross-compiler semantic manifests, replay/differential fuzz with structured byte
  decoding, Google Benchmark methodology, allocation/memory instrumentation, and a
  benchmark-only container spike with batch-aware sorted-vector candidate distinction.
  Initial independent architecture review returned CHANGES REQUESTED (P0: 0,
  P1 design: 1, P1 implementation: 2, P2: 7). Corrected findings: M5-AR-001,
  M5-AR-002, M5-AR-003 (mandatory), M5-AR-004 through M5-AR-010 (P2).
  Design only; no implementation.

- M5 pre-implementation decisions closed: OD-M5-001 (representative transcript corpus
  acquisition plan — Recorder M21.4 validated 24h BTCUSDT Spot + USD-M perpetual live Raw
  capture as M5 corpus source v1) and OD-M5-002 (free standard GitHub-hosted runners only;
  docs-only CI skip via `paths-ignore`; superseded-run cancellation with
  `concurrency.cancel-in-progress`; scheduled medium: <= weekly, 45 min timeout, non-blocking;
  container spike: manual only, 60 min timeout; artifact ceiling: 200 MiB/run, 7-day retention;
  no paid larger runners; no self-hosted public-repo runner). OD-M5-003 remains
  SPIKE-RESOLVABLE. M5 implementation authorized on merge of the decision/CI-policy PR. M6
  remains NOT STARTED. See `docs/M5_PREIMPLEMENTATION_DECISIONS.md`.

### Changed

- M4 recorded as COMPLETE on `main`: merged through PR #8 at
  `ac780d9eb7b49ff20a6b3b4bee6a993b51b70af4`, post-merge main CI `31242162782` — 16/16 PASS.
  The three deferred M4 P2 findings remain open and non-blocking.

- M4 optional `BinanceMarketDataProjection::ProtoAdapter` component with strict owning conversion
  for Contracts `ExchangeDepthSnapshot`/`DepthUpdate`, checked `NumericSpec` and sequence-policy
  binding, deterministic four-state `LocalOrderBookSnapshot` output, explicit gap/recovery/context
  mapping, and separated Host/Core/inbound quality domains.
- Pinned C-M4-001 Conan bootstrap and lock identity, configure-time schema/package/runtime metadata
  checks and negative gates, component-aware installs, isolated Core/adapter consumers, static/shared
  generated-symbol ownership checks, property/fixture/lifetime/allocation matrices, and an M4
  libFuzzer corpus. M4 is approved pending merge.
- M3 `UpdateId`, valid-by-construction `UpdateRange`, and `BookProjection` public API with explicit
  Spot and USD-M sequence policies, a four-state lifecycle, deterministic gap evidence,
  synchronization-aware const visibility, and strongly transactional baseline/incremental apply.
- M3 unit/state-transition coverage, independent primitive/vector property model, deterministic
  Spot and USD-M replay tests, exhaustive allocation-failure sweeps, installed-consumer coverage,
  and a model-based libFuzzer harness with seed corpus.
- M2 deterministic order book core: `BookSide`, `BookLevel`, `LevelUpdate`, `LevelChange`, `OrderBook`
  with PIMPL storage, absolute-quantity semantics, batch updates, atomic replace-all, best bid/ask,
  top-N queries, and model-based fuzzing.

## [0.1.0-alpha.0]

### Added

- M1 exact numeric primitives: `DecimalScale`, `PriceUnits`, `QuantityUnits`, and `NumericSpec`.
- Strict Contracts-compatible price, quantity, and positive-quantity parsing with stable errors,
  exact rescaling, source fractional digit metadata, and checked signed 64-bit arithmetic.
- Deterministic exact formatting, fixed-format conveniences, boundary/roundtrip/property tests,
  public-header self-containment checks, installed-consumer coverage, and a libFuzzer harness.
- M0 C++20 repository foundation, version API, build/test/benchmark infrastructure, package export,
  CI, architecture documentation, and ADRs.
