# Changelog

All notable changes will be recorded here.

## [Unreleased]

### Added

- M4 optional `BinanceMarketDataProjection::ProtoAdapter` component with strict owning conversion
  for Contracts `ExchangeDepthSnapshot`/`DepthUpdate`, checked `NumericSpec` and sequence-policy
  binding, deterministic four-state `LocalOrderBookSnapshot` output, explicit gap/recovery/context
  mapping, and separated Host/Core/inbound quality domains.
- Pinned C-M4-001 Conan bootstrap and lock identity, configure-time schema/package/runtime metadata
  checks and negative gates, component-aware installs, isolated Core/adapter consumers, static/shared
  generated-symbol ownership checks, property/fixture/lifetime/allocation matrices, and an M4
  libFuzzer corpus. M4 remains an implementation candidate pending independent review.
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
