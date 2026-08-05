# Changelog

All notable changes will be recorded here.

## [Unreleased]

### Added

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
