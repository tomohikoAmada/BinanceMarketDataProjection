# BinanceMarketDataProjection

`BinanceMarketDataProjection` is a C++20 library for a deterministic, strategy-independent Binance
market-data projection core. M0-M4 are COMPLETE, M5 is COMPLETE, the M6 cross-repository contract
design is COMPLETE, and M6 implementation/Gateway runtime are NOT STARTED.

## For AI agents and independent reviewers

Start with [`docs/CURRENT_STATE.md`](docs/CURRENT_STATE.md), then read [`AGENTS.md`](AGENTS.md),
this README, [`ARCHITECTURE.md`](ARCHITECTURE.md), [`docs/MILESTONES.md`](docs/MILESTONES.md),
[`docs/M6_GATEWAY_INTEGRATION_SEQUENCE.md`](docs/M6_GATEWAY_INTEGRATION_SEQUENCE.md), relevant
accepted ADRs and phase-specific records, and finally the code/tests/build configuration. The
orientation file is a summary only; accepted ADRs/designs and current GitHub/code state remain
authoritative.

This is an unofficial project and is not affiliated with, endorsed by, or sponsored by Binance.
This module does not connect to Binance, use API keys, place orders, or contain trading strategies.

M1 introduced strict decimal parsing, strongly typed signed 64-bit units, exact rescaling, and
deterministic formatting. M2 added the deterministic order book core, M3 added sequence validation
and projection lifecycle state, and M4 added the optional Contracts message adapter and
local-order-book snapshot production boundary. M5 adds validation and benchmark infrastructure
only; runtime Gateway/History integration, networking, persistence, market-state derivation, and
trading remain unimplemented.

## Architecture boundary

The core is an embedded, single-writer, deterministic C++ library shared by live Gateway and
History/Replay hosts. It does not own threads, networking, persistence, logging, or system time.
Wire-format adaptation remains outside Core. See [ARCHITECTURE.md](ARCHITECTURE.md).

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
- Docker (Docker Engine on Linux, Docker Desktop on macOS) for the canonical Quality gate
  (`scripts/quality.sh`). The canonical clang/clang-tidy/clang-format 18.1.3 toolchain runs inside
  a repository-pinned container; see [docs/QUALITY_TOOLCHAIN.md](docs/QUALITY_TOOLCHAIN.md).
- A local `clang-format` for broad `verify.sh` validation. Local hosts without clang-tidy may skip
  it explicitly; canonical containerized clang-tidy remains authoritative.

Conan 2, GoogleTest 1.17.0, and Google Benchmark 1.9.5 are installed/resolved into repository-
local directories. No global installation is performed.

## Bootstrap

```bash
export CONAN_HOME="$PWD/.cache/conan2"
export PIP_CACHE_DIR="$PWD/.cache/pip"
bash scripts/bootstrap.sh
```

Bootstrap creates `.venv-tools`, detects a repository-local Conan profile, uses or creates
`conan.lock`, and resolves dependencies from ConanCenter. Conan-generated files stay under
`build/`.

## Build and test

```bash
bash scripts/configure.sh debug
bash scripts/build.sh debug
bash scripts/test.sh debug

bash scripts/configure.sh release
bash scripts/build.sh release
bash scripts/test.sh release
```

Configured targets include `bmd_projection_core`, `bmd_projection_tests`,
`bmd_projection_benchmarks`, the M3 allocation-failure tests, and the decimal/order-book/
projection fuzz targets. The optional M4 build also provides
`bmd_projection_proto_adapter` and `bmd_projection_proto_adapter_fuzz`. Build-tree and installed
consumers link the alias `BinanceMarketDataProjection::Core`.

## Exact numeric API

Contracts-compatible public decimal text is parsed into `PriceUnits` or `QuantityUnits`, backed by
`std::int64_t`, using an explicit caller-supplied scale from 0 through 18. Conversion is exact or
rejected and never rounds. Successful parse results retain source fractional digit count so a
formatter can reconstruct trailing zeroes. See [M1 numeric semantics](docs/M1_NUMERIC_SEMANTICS.md)
and [ADR-0002](docs/adr/ADR-0002-fixed-point-internal-representation.md).

## Order book core

The order book is deterministic and single-writer, and implements L2 market-by-price semantics.
Bids are ordered descending and asks ascending. Quantities are absolute values; zero quantity
deletes a level. Single-level, batch, and `replace_all` updates are supported, along with best,
quantity-at-price, top-N, and full ordered-level queries. Crossed and locked books are accepted;
the core does not match orders. Each `OrderBook` is bound to one `NumericSpec`.

Sequence validation and gap detection enter through the M3 `BookProjection` mutation surface.
Snapshot contracts and networking remain outside Core. See [M2 semantics](docs/M2_ORDER_BOOK_SEMANTICS.md),
[M3 design](docs/M3_SEQUENCE_AND_PROJECTION_STATE_DESIGN.md), and
[ADR-0003](docs/adr/ADR-0003-single-writer-order-book.md).

## Sequence and projection state

The M3 API provides strongly typed `UpdateId` and valid-by-construction `UpdateRange` values plus a
`BookProjection` that selects the approved Spot interval policy or USD-M previous-final policy. Its
lifecycle is `AwaitingBaseline`, `AwaitingBridge`, `Synchronized`, or `NeedsResync`. Baselines and
accepted batches commit with the strong exception guarantee. Core owns no snapshot download,
network buffer, wire type, clock, or recovery runtime. See [ADR-0005](docs/adr/ADR-0005-market-specific-sequence-policy.md)
and [ADR-0008](docs/adr/ADR-0008-spot-bootstrap-successor-coverage.md).

## Optional snapshots and Protobuf adapter

Core-only mode remains the default and has no Contracts or Protobuf dependency. To build M4:

```bash
bash scripts/bootstrap-contracts.sh
bash scripts/configure.sh debug -DBMD_PROJECTION_BUILD_PROTO_ADAPTER=ON
bash scripts/build.sh debug
bash scripts/test.sh debug
```

The public `BinanceMarketDataProjection::ProtoAdapter` target converts strict Contracts
`ExchangeDepthSnapshot` and `DepthUpdate` messages to lifetime-safe owning values with checked M3
invocation. It also builds deterministic `LocalOrderBookSnapshot` values from Core plus explicit
Host identity, time, recovery, depth, and quality context. The adapter does not own clocks,
networking, buffering, persistence, recovery orchestration, or gRPC. See [ADR-0006](docs/adr/ADR-0006-protobuf-adapter-boundary.md).

## Sanitizers and fuzz smoke

```bash
for preset in asan ubsan tsan coverage; do
  bash scripts/configure.sh "$preset"
  bash scripts/build.sh "$preset"
  bash scripts/test.sh "$preset"
done

bash scripts/fuzz-smoke.sh
```

ASan and TSan are mutually exclusive. The fuzz smoke covers the decimal parser, order book,
projection, Protobuf adapter, and M5 replay differential harnesses where the local toolchain
supports them.

## Benchmark smoke test

```bash
bash scripts/configure.sh benchmark -DBMD_PROJECTION_BUILD_PROTO_ADAPTER=ON
bash scripts/build.sh benchmark
bash scripts/benchmark-smoke.sh
```

The Phase-6 suite provides representative M1-M4 workloads, production Core/Adapter replay
throughput, and a production-only event-latency path. Smoke validation is structural and has no
numeric performance threshold. A formal/manual full run uses:

```bash
bash scripts/benchmark-full.sh
```

See [M5 Phase 6](docs/M5_PHASE6_REPRESENTATIVE_BENCHMARKS.md) for measurement contracts and
corrected formal evidence.

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

An adapter consumer requests `COMPONENTS ProtoAdapter` and links
`BinanceMarketDataProjection::ProtoAdapter`. The scripted consumers use only the staged
installation and validate the pinned transitive Contracts dependency where applicable.

## Milestone status

M0-M4 are COMPLETE. M5 Phase 1-11 and M5 are COMPLETE. M5’s high-level capabilities include:

- deterministic replay;
- independent differential/reference validation;
- cross-compiler semantic manifests;
- differential fuzzing;
- representative benchmark evidence;
- allocation/memory evidence;
- Phase-8 container experiments;
- the `KEEP_STD_MAP` decision;
- recorded replay and a weekly exploratory performance canary; and
- a final independent repository audit.

PR #42 corrected benchmark evidence provenance. It did **not** change Production Core semantics.
See the [M5 phase records](docs/M5_PHASE1_CANONICAL_REPLAY.md) and
[MILESTONES.md](docs/MILESTONES.md) for detailed historical provenance.

The M6 cross-repository integration contract design is COMPLETE. M6 implementation and Gateway
runtime are NOT STARTED, and Gateway runtime is outside this repository. The authoritative contract
is [M6 Cross-Repository Integration](docs/M6_CROSS_REPOSITORY_INTEGRATION_CONTRACT.md); its
sequence in [M6 Gateway Integration](docs/M6_GATEWAY_INTEGRATION_SEQUENCE.md) still requires the
Contracts prerequisite stages plus explicit owner authorization and is not an implementation
authorization.

## Canonical Quality acceptance

```bash
bash scripts/quality.sh
```

This is the single repository-owned entrypoint for authoritative CI-equivalent Quality semantics.
It uses the pinned contract in `.toolchain/quality.env` and the repository-pinned container. CI
invokes the same command; the workflow contains no second Quality definition. See
[docs/QUALITY_TOOLCHAIN.md](docs/QUALITY_TOOLCHAIN.md).

## Complete local verification

```bash
bash scripts/verify.sh
```

`verify.sh` is broad supplemental developer verification and includes formatting, builds, tests,
sanitizers, smoke checks, packaging consumers, and `git diff --check`. It is not the canonical
Quality acceptance entrypoint.

## Technical limitations

- Networking, persistence, Gateway runtime, History runtime, derived market state, strategy, and
  trading behavior remain unimplemented.
- Tick-size, step-size, signed-decimal, and symbol-metadata validation remain outside the current
  numeric/order-book scope.
- TSan support varies by host platform and toolchain.
- Dedicated Ubuntu ARM64/RK3588 CI is not part of the initial hosted matrix.
- The formal Contracts Package Revision remains `NOT_FORMALLY_ASSIGNED` until release.

## Directory structure and links

```text
include/       Public C++ API       src/           Core implementation
tests/         Tests and consumers  benchmarks/    Benchmark infrastructure
cmake/         Package helpers      scripts/       Repository workflows
docs/          Milestones, ADRs     fuzz/          Harnesses and seed corpus
.toolchain/    Quality contract     .github/       CI workflows
```

The project’s semantic authorities are the accepted ADRs and milestone/phase designs. The current
orientation is [`docs/CURRENT_STATE.md`](docs/CURRENT_STATE.md); architecture is
[`ARCHITECTURE.md`](ARCHITECTURE.md); contribution workflow is [`AGENTS.md`](AGENTS.md).

## License status

No open-source license has been selected yet. The repository remains private; license selection is
tracked as `O-P001` in [docs/OPEN_QUESTIONS.md](docs/OPEN_QUESTIONS.md).
