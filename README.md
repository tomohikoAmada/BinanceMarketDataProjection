# BinanceMarketDataProjection

`BinanceMarketDataProjection` is a C++20 library for a deterministic, strategy-independent Binance
market-data projection core. M1 through M4 are complete on `main`; M5 (Differential Validation and
Performance) implementation is in progress (Phases 1 through 6 complete/merged; later phases not
started).

## For AI agents and independent reviewers

Start with [`docs/CURRENT_STATE.md`](docs/CURRENT_STATE.md), then read [`AGENTS.md`](AGENTS.md),
this README, [`ARCHITECTURE.md`](ARCHITECTURE.md), the current milestone and M5 phase documents,
the relevant accepted ADRs, actual code/tests, and the merged PR #16 record plus its exact-head
CI. The orientation file is a summary only; accepted ADRs/designs and current GitHub/code state
remain authoritative.

This is an unofficial project and is not affiliated with, endorsed by, or sponsored by Binance.
This module does not connect to Binance, use API keys, place orders, or contain trading strategies.

> M1 introduced strict decimal parsing, strongly typed signed 64-bit units, exact rescaling, and
> deterministic formatting. M2 subsequently added the deterministic order book core, M3 added
> sequence validation and projection lifecycle state, and M4 added the optional Contracts message
> adapter and local-order-book snapshot production boundary. M5 adds validation and benchmark
> infrastructure only; runtime Gateway/History integration, networking, persistence,
> market-state derivation, and trading remain unimplemented.

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
- Docker or Podman for the canonical Quality gate (`scripts/quality.sh`); the canonical
  clang/clang-tidy/clang-format 18.1.3 toolchain runs inside a repository-pinned container
  (see [docs/QUALITY_TOOLCHAIN.md](docs/QUALITY_TOOLCHAIN.md)).
- A local `clang-format` for the broad `verify.sh` gate. Local hosts without clang-tidy may skip
  it explicitly; canonical containerized clang-tidy remains the authoritative gate.

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
`bmd_projection_book_projection_fuzz`. The optional M4 build also provides
`bmd_projection_proto_adapter` and `bmd_projection_proto_adapter_fuzz`. Build-tree and installed consumers link the alias
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

## Optional snapshots and Protobuf adapter

Core-only mode remains the default and has no Contracts or Protobuf dependency. To build M4, first
create the exact accepted Contracts package in the repository-local Conan cache, then enable the
component:

```bash
bash scripts/bootstrap-contracts.sh
bash scripts/configure.sh debug -DBMD_PROJECTION_BUILD_PROTO_ADAPTER=ON
bash scripts/build.sh debug
bash scripts/test.sh debug
```

The public target `BinanceMarketDataProjection::ProtoAdapter` converts strict Contracts
`ExchangeDepthSnapshot` and `DepthUpdate` messages to lifetime-safe owning values with checked M3
invocation. It also builds deterministic `LocalOrderBookSnapshot` values from Core plus explicit
Host identity, time, recovery, depth, and quality context. The adapter does not own clocks,
networking, buffering, persistence, recovery orchestration, or gRPC.

Contracts are pinned by source revision, Conan recipe revision, schema baseline/fingerprint, and
generator/runtime metadata. The formal Contracts Package Revision is still
`NOT_FORMALLY_ASSIGNED` until release. See the
[M4 design and implementation status](docs/M4_SNAPSHOTS_AND_PROTOBUF_BOUNDARY_DESIGN.md) and
[ADR-0006](docs/adr/ADR-0006-protobuf-adapter-boundary.md).

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

The decimal parser, order book, book projection, Protobuf adapter, and M5 replay differential
fuzz harnesses are built with upstream Clang and libFuzzer plus AddressSanitizer and
UndefinedBehaviorSanitizer. Unsupported local toolchains report an explicit skip; the Ubuntu
Clang CI job requires support and runs each harness for a fixed 10,000-input smoke test from
its checked-in seed corpus.

## Benchmark smoke test

```bash
bash scripts/configure.sh benchmark -DBMD_PROJECTION_BUILD_PROTO_ADAPTER=ON
bash scripts/build.sh benchmark
bash scripts/benchmark-smoke.sh
```

The Phase-6 benchmark suite provides representative M1-M4 Google Benchmark workloads (including
the full 48-cell M3 accepted live-apply matrix), production `CoreNormalizedReplay` and
`AdapterWireReplay` throughput, and a dedicated production-only event-latency executable. The
smoke driver runs the locked 8-cell M3 subset with 1 repetition and validates the required
inventory, the metadata wrapper (`M5_BENCHMARK_WRAPPER_V1`), payload SHA binding, and the
`M5_REPLAY_LATENCY_V1` latency evidence fail-closed. Smoke values are structural execution
evidence only; no numeric performance threshold exists.

A formal/manual full evidence run (>= 5 repetitions, full matrix) uses:

```bash
bash scripts/benchmark-full.sh
```

See [M5 Phase 6](docs/M5_PHASE6_REPRESENTATIVE_BENCHMARKS.md) for the measurement contracts.

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

An adapter installation is consumed explicitly:

```cmake
find_package(BinanceMarketDataProjection CONFIG REQUIRED COMPONENTS ProtoAdapter)
target_link_libraries(consumer PRIVATE BinanceMarketDataProjection::ProtoAdapter)
```

`scripts/install-adapter-consumer-test.sh` validates the pinned transitive Contracts dependency,
fixture conversion, checked M3 call, output snapshot, and single generated-symbol ownership for
static or shared builds.

## Complete local verification

```bash
bash scripts/verify.sh
```

This runs formatting, warnings-as-errors, Core-only and M4 Debug/Release tests, required sanitizers,
the benchmark/fuzz smoke tests, both staged-install consumers, static/shared M4 packaging, the
deterministic toolchain contract tests, and `git diff --check`. When the host
has clang-tidy the gate enables it automatically; otherwise it skips with a clear message.
The output ends with `CANONICAL QUALITY: NOT RUN` (or `PASS` if
`BMD_PROJECTION_RUN_CANONICAL_QUALITY=1` was set): `verify.sh` is broad developer verification,
not canonical acceptance.

## Canonical Quality acceptance

```bash
bash scripts/quality.sh
```

This is the single repository-owned entrypoint for authoritative CI-equivalent Quality semantics.
It builds the pinned canonical container (Ubuntu 24.04 amd64 with exact clang 18.1.3,
clang-tidy 18.1.3, clang-format 18.1.3 from the contract in `.toolchain/quality.env`), fails closed
when the toolchain does not match the contract, and runs formatting, the repository-local Conan
and pinned Contracts bootstraps, a Debug configure with ProtoAdapter ON / clang-tidy ON /
WarningsAsErrors ON, build, tests, and the staged-install consumer. CI invokes the same command;
the workflow contains no second definition of Quality semantics. Local clang-format/clang-tidy
runs are supplemental and never canonical acceptance. See
[docs/QUALITY_TOOLCHAIN.md](docs/QUALITY_TOOLCHAIN.md) for the contract, identity table, and the
intentional upgrade procedure.

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
.toolchain/    Canonical Quality toolchain contract and container
.github/       CI workflows
```

## Milestone status

M1, M2, M3, and M4 are COMPLETE. M4 was merged through PR #8 at
`ac780d9eb7b49ff20a6b3b4bee6a993b51b70af4` with post-merge main CI `31242162782` — 16/16 PASS.
M5 (Differential Validation and Performance) is **APPROVED / MERGED / IN PROGRESS**: Phase 1
canonical replay infrastructure is COMPLETE / MERGED (PR #11, merge
`5e8629a7ff825f8ea941304d9b09be1670643e8a`, post-merge main CI `31264500905` — 16/16 PASS),
Phase 2 (independent differential oracle foundation) is COMPLETE / MERGED (PR #12, merge
`75c619dd683ff2a3893f9535e206231e7bfecc41`, post-merge main CI `31315421548` — 16/16 PASS), and
Phase 3 is COMPLETE / MERGED (PR #13, final approved Head
`a8e4ccfc31efd4e67bc10cf0ac9ad2a99faa8354`, exact-head CI `31491615547` — 16/16 PASS, squash
merge `473a907eba2001d18926c57d6c8d16b10c7505be`): deterministic 2,048-event Spot
and USD-M small workloads, scalable differential replay, failure diagnostics, the offline Raw-v1
materializer, and validator-enforced medium lifecycle validity are implemented, rebased onto the
accepted M3 Spot successor-coverage semantics (ADR-0008). Both mandatory 100k corpora are
validated PASS from the pinned authoritative source: Spot `M5-REC-SPOT-BTCUSDT-V1` (exact-next
bridge `U=98288147168 u=98288147175` against `L=98288147167`; 100,001 Applied, final
Synchronized) and USD-M `M5-REC-USDM-BTCUSDT-V1` (bridge Applied / Synchronized, 100,001
Applied, final Synchronized). See
[M5 Phase 3](docs/M5_PHASE3_DETERMINISTIC_REPLAY.md) and
[M5 Phase 4](docs/M5_PHASE4_CROSS_COMPILER_SEMANTIC_MANIFESTS.md).
Phase 4 (cross-compiler semantic manifests) is COMPLETE / MERGED (PR #16, final approved Head
`b612f85d281315346c0ccf6f599d51af538e3cf4`, exact-head CI `31571166506` — 18/18 PASS, squash
merge `7b6d9ef3b222675138fdd34f3fed381216fe9d02`, post-merge main CI `31576511096` — 18/18
PASS):
canonical OperationObservation serialization (schema v1), semantic SHA-256 digests,
portable manifest v1, manifest producer, fail-closed shared Python evidence validation, complete
Debug/Release fixture/build identity checks, and metadata-validated three-cross-compiler artifact
fan-in CI.
Phase 5 (differential replay fuzzing) is COMPLETE / MERGED (PR #18, final approved Head
`e56f5dbd12b9e66946343467221e8e3ba9984531`, exact-head CI `31668465623` — 18/18 PASS, squash
merge `53268d5cd2090f4779ffdc14c070184f470cc899`, post-merge main CI `31671708958` — 18/18
PASS). See [M5 Phase 5](docs/M5_PHASE5_DIFFERENTIAL_FUZZING.md). Phase 6 is COMPLETE / MERGED
(PR #21, accepted implementation Head
`9776ba6b93990c44e550f289b69127ca721b0d00`, exact-head CI `31803322848` — 18/18 PASS, final
independent review APPROVED, formal exact-head evidence ACCEPTED, squash merge
`227524e6d17cce77813c6f26cd65bb8d996f5677`, post-merge main CI `31809917018` — 18/18 PASS; see
[M5 Phase 6](docs/M5_PHASE6_REPRESENTATIVE_BENCHMARKS.md)); later phases remain not started.
Its design covers layered differential validation with operation-result
observation, canonical replay fixtures with canonical text format rules, determinism and
cross-compiler semantic manifests with artifact fan-in transport, replay/differential fuzzing,
benchmark methodology, allocation/memory characterization, and a benchmark-only container spike.
Pre-implementation decisions OD-M5-001 and OD-M5-002 are CLOSED. OD-M5-003 remains SPIKE-RESOLVABLE.
The Contracts reference baseline is `01d76a41929f36d89573159f5f458f9f1e378ada`.

## Known limitations

- The repository includes numeric primitives, a deterministic L2 market-by-price order book, the
  completed M3 sequence/projection implementation, and the merged optional M4 Protobuf adapter on
  `main`.
- M5 is **APPROVED / MERGED / IN PROGRESS**; Phases 1, 2, 3, 4, 5, and 6 are COMPLETE / MERGED
  (Phase 3 Spot and USD-M 100k corpora validated PASS from the pinned authoritative source under
  ADR-0008 successor coverage; Phase 4 cross-compiler GNU/Clang/AppleClang semantic equality
  PASS; Phase 5 differential replay fuzzing merged via PR #18, squash
  `53268d5cd2090f4779ffdc14c070184f470cc899`, exact-head CI `31668465623` — 18/18 PASS,
  post-merge CI `31671708958` — 18/18 PASS; Phase 6 representative benchmarks merged via PR #21,
  accepted implementation Head `9776ba6b93990c44e550f289b69127ca721b0d00`, exact-head CI
  `31803322848` — 18/18 PASS, final independent review APPROVED, formal exact-head evidence
  ACCEPTED, squash merge `227524e6d17cce77813c6f26cd65bb8d996f5677`, post-merge CI
  `31809917018` — 18/18 PASS). See
  [M5 Phase 6](docs/M5_PHASE6_REPRESENTATIVE_BENCHMARKS.md).
  M6 is not started.
- Networking, persistence, Gateway runtime, History runtime, derived market state, strategy, and
  trading behavior remain unimplemented.
- Tick-size, step-size, signed-decimal, and symbol-metadata validation remain outside the implemented
  M1/M2 scope.
- TSan support varies by host platform and toolchain.
- Dedicated Ubuntu ARM64/RK3588 CI is not part of the initial hosted matrix.

## License status

No open-source license has been selected yet. The repository remains private; license selection is
tracked as `O-P001` in [docs/OPEN_QUESTIONS.md](docs/OPEN_QUESTIONS.md).
