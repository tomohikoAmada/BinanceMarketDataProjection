# BinanceMarketDataProjection

`BinanceMarketDataProjection` is a C++20 library for a deterministic, strategy-independent Binance
market-data projection core. The current state is **M1 Numeric and Domain Primitives — COMPLETE, M2
Order Book Core — COMPLETE, M3 Sequence and Projection State — COMPLETE**. M3 passed external
architecture and implementation review, was merged through PR #6, and is available on `main`.

This is an unofficial project and is not affiliated with, endorsed by, or sponsored by Binance.
This module does not connect to Binance, use API keys, place orders, or contain trading strategies.

> M1 introduced strict decimal parsing, strongly typed signed 64-bit units, exact rescaling, and
> deterministic formatting. M2 subsequently added the deterministic order book core. The M3
> sequence validation and projection lifecycle state. Main CI passed after the M3 merge. Market-state
> snapshots, protobuf adapters, networking, persistence, and trading remain unimplemented.

## Architecture boundary

The future core is an embedded, single-writer, deterministic C++ library shared by live Gateway and
History/Replay hosts. It will not own threads, networking, persistence, logging, or system time.
Wire-format adaptation remains outside the Core. See [ARCHITECTURE.md](ARCHITECTURE.md).

## Supported platforms

- macOS with AppleClang and Ninja (Apple Silicon is a primary target).
- Ubuntu Linux with GCC or Clang and Ninja.
- Ubuntu ARM64/RK3588 is an architectural target; dedicated runner validation is deferred.

MSVC is not an M0 CI target, although the public API does not intentionally rely on compiler
extensions.

## Build requirements

- CMake 3.24 or newer.
- Ninja.
- Python 3 with `venv`.
- A C++20 compiler.
- `clang-format` for the local quality gate.
- `clang-tidy` is mandatory in CI. Local hosts without clang-tidy may skip it explicitly; CI
  clang-tidy remains the authoritative gate.

Conan 2, GoogleTest 1.17.0, and Google Benchmark 1.9.5 are installed/resolved into repository-local
directories. No global installation is performed.

## Bootstrap

```bash
export CONAN_HOME="$PWD/.cache/conan2"
export PIP_CACHE_DIR="$PWD/.cache/pip"
bash scripts/bootstrap.sh
```

Bootstrap creates `.venv-tools`, detects a repository-local Conan profile, uses or creates
`conan.lock`, and resolves dependencies from ConanCenter. Conan-generated files stay under `build/`.

## Build and test

```bash
bash scripts/configure.sh debug
bash scripts/build.sh debug
bash scripts/test.sh debug

bash scripts/configure.sh release
bash scripts/build.sh release
bash scripts/test.sh release
```

The configured CMake targets are `bmd_projection_core`, `bmd_projection_tests`, and, when enabled,
`bmd_projection_m3_allocation_failure_tests`, `bmd_projection_benchmarks`,
`bmd_projection_decimal_parser_fuzz`, `bmd_projection_order_book_fuzz`, or
`bmd_projection_book_projection_fuzz`. Build-tree and installed consumers link the alias
`BinanceMarketDataProjection::Core`.

## Exact numeric API

Contracts-compatible public decimal text is parsed into `PriceUnits` or `QuantityUnits`, backed by
`std::int64_t`, using an explicit caller-supplied scale from 0 through 18. Conversion is exact or
rejected and never rounds. Successful parse results retain the source fractional digit count so a
formatter can reconstruct trailing zeroes. See
[M1 numeric semantics](docs/M1_NUMERIC_SEMANTICS.md) and
[ADR-0002](docs/adr/ADR-0002-fixed-point-internal-representation.md).

## Order book core

The order book is deterministic and single-writer, and implements L2 market-by-price semantics.
Bids are ordered descending and asks ascending. Quantities are absolute values; zero quantity
deletes a level. Single-level, batch, and `replace_all` updates are supported, along with best,
quantity-at-price, top-N, and full ordered-level queries. Crossed and locked books are accepted;
the core does not match orders. Each `OrderBook` is bound to one `NumericSpec`.

Sequence validation and gap detection are intentionally outside the M2 `OrderBook` and enter only
through the M3 `BookProjection` mutation surface. Snapshot contracts and networking remain outside
Core. See [M2 order book semantics](docs/M2_ORDER_BOOK_SEMANTICS.md) and
[ADR-0003](docs/adr/ADR-0003-single-writer-order-book.md).

## Sequence and projection state

The M3 API provides strongly typed `UpdateId` and valid-by-construction `UpdateRange`
values plus a `BookProjection` that selects either the approved Spot interval policy or USD-M
previous-final policy. Its lifecycle is `AwaitingBaseline`, `AwaitingBridge`, `Synchronized`, or
`NeedsResync`. Baselines and accepted batches commit with the strong exception guarantee.

A detected gap preserves the last accepted book and update ID but quarantines normal access:
`synchronized_book()` is available only while synchronized, while `diagnostic_book()` is the
explicit const view for pending or stale evidence. Core owns no snapshot download, network buffer,
wire type, clock, or recovery runtime. See the
[M3 design](docs/M3_SEQUENCE_AND_PROJECTION_STATE_DESIGN.md) and
[ADR-0005](docs/adr/ADR-0005-market-specific-sequence-policy.md). The M3 implementation was merged
through PR #6 and is available on `main`.

## Sanitizers and coverage

```bash
for preset in asan ubsan tsan coverage; do
  bash scripts/configure.sh "$preset"
  bash scripts/build.sh "$preset"
  bash scripts/test.sh "$preset"
done
```

ASan and TSan are mutually exclusive. Sanitizer and coverage options are target-local and are not
exported to installed consumers. TSan availability depends on the compiler/runtime/platform; CI
requires ASan and UBSan and retains TSan as an explicit independently testable configuration.

## Fuzz smoke

```bash
bash scripts/fuzz-smoke.sh
```

The decimal parser, order book, and book projection fuzz harnesses are built with upstream Clang and
libFuzzer plus AddressSanitizer and UndefinedBehaviorSanitizer. Unsupported local toolchains report
an explicit skip; the Ubuntu Clang CI job requires support and runs each harness for a fixed
10,000-input smoke test from its checked-in seed corpus.

## Benchmark smoke test

```bash
bash scripts/configure.sh benchmark
bash scripts/build.sh benchmark
build/benchmark/cmake/benchmarks/bmd_projection_benchmarks \
  --benchmark_format=json \
  --benchmark_out=build/benchmark/foundation-benchmark.json
```

The benchmark remains an infrastructure smoke test; it does not represent order-book or projection
performance.

## Install and downstream consumer

```bash
cmake --install build/release/cmake --prefix build/stage
bash scripts/install-consumer-test.sh
```

An installed consumer uses:

```cmake
find_package(BinanceMarketDataProjection CONFIG REQUIRED)
target_link_libraries(consumer PRIVATE BinanceMarketDataProjection::Core)
```

The scripted consumer is a separate CMake project. It only reads the staged installation and never
uses `add_subdirectory` or source-tree include paths.

## Complete local verification

```bash
bash scripts/verify.sh
```

This runs formatting, warnings-as-errors, Debug/Release tests, required sanitizers, the
benchmark smoke test, the staged-install consumer, and `git diff --check`. When the host
has clang-tidy the gate enables it automatically; otherwise it skips with a clear message.
CI clang-tidy is mandatory regardless of local skip.

## Directory structure

```text
include/       Public C++ API
src/           Core implementation
tests/         GoogleTest and independent downstream consumer
benchmarks/    Google Benchmark infrastructure smoke test
cmake/         Target-local quality and package-export helpers
scripts/       Repository-local development workflows
docs/          Milestones, open questions, and ADRs
fuzz/          libFuzzer harnesses and checked-in seed corpus
.github/       CI workflows
```

## Milestone status

M1, M2, and M3 are COMPLETE. All three milestones were externally reviewed and merged. The
Contracts reference baseline is
`01d76a41929f36d89573159f5f458f9f1e378ada`.

## Known limitations

- The repository includes numeric primitives, a deterministic L2 market-by-price order book, and
  the completed M3 sequence/projection implementation on `main`.
- Snapshot contracts, protobuf adapters, networking, persistence, Gateway runtime, History runtime,
  strategy, and trading behavior remain unimplemented.
- Tick-size, step-size, signed-decimal, and symbol-metadata validation remain outside the implemented
  M1/M2 scope.
- TSan support varies by host platform and toolchain.
- Dedicated Ubuntu ARM64/RK3588 CI is not part of the initial hosted matrix.
- M4 Snapshots and Protobuf Boundary and all later milestones remain unimplemented.

## License status

No open-source license has been selected yet. The repository remains private; license selection is
tracked as `O-P001` in [docs/OPEN_QUESTIONS.md](docs/OPEN_QUESTIONS.md).
