# BinanceMarketDataProjection

`BinanceMarketDataProjection` is a C++20 library foundation for a future deterministic,
strategy-independent Binance market-data projection core. The current state is **M0 Foundation / No
Projection Logic Yet**. The library exposes only stable project/version metadata.

This is an unofficial project and is not affiliated with, endorsed by, or sponsored by Binance.
This module does not connect to Binance, use API keys, place orders, or contain trading strategies.

> M0 does not implement decimal parsing, order-book reconstruction, sequence validation,
> market-state projection, protobuf adapters, networking, persistence, or trading.

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
- `clang-format` and `clang-tidy` for the complete local quality gate.

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

The configured CMake targets are `bmd_projection_core`, `bmd_projection_tests`, and (when enabled)
`bmd_projection_benchmarks`. Build-tree and installed consumers link the alias
`BinanceMarketDataProjection::Core`.

## Sanitizers and coverage

```bash
for preset in asan ubsan tsan coverage; do
  bash scripts/configure.sh "$preset"
  bash scripts/build.sh "$preset"
  bash scripts/test.sh "$preset"
done
```

ASan and TSan are mutually exclusive. Sanitizer and coverage options are target-local and are not
exported to installed consumers. TSan availability depends on the compiler/runtime/platform; M0 CI
requires ASan and UBSan and retains TSan as an explicit independently testable configuration.

## Benchmark smoke test

```bash
bash scripts/configure.sh benchmark
bash scripts/build.sh benchmark
build/benchmark/cmake/benchmarks/bmd_projection_benchmarks \
  --benchmark_format=json \
  --benchmark_out=build/benchmark/foundation-benchmark.json
```

M0 Benchmark 只是基础设施 Smoke Test，不代表 Order Book 或 Projection 性能。

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

This runs formatting, clang-tidy, warnings-as-errors, Debug/Release tests, required sanitizers, the
benchmark smoke test, the staged-install consumer, and `git diff --check`.

## Directory structure

```text
include/       Public C++ API
src/           Core implementation
tests/         GoogleTest and independent downstream consumer
benchmarks/    Google Benchmark infrastructure smoke test
cmake/         Target-local quality and package-export helpers
scripts/       Repository-local development workflows
docs/          Milestones, open questions, and ADRs
.github/       CI workflows
```

## Milestone status

M0 Repository Foundation is tracked in [docs/MILESTONES.md](docs/MILESTONES.md). No later milestone
is implemented by this branch. The Contracts reference baseline is
`01d76a41929f36d89573159f5f458f9f1e378ada`.

## Known limitations

- There is no projection/domain behavior in M0.
- TSan support varies by host platform and toolchain.
- Dedicated Ubuntu ARM64/RK3588 CI is not part of the initial hosted matrix.
- The API surface is intentionally limited to version metadata.

## License status

No open-source license has been selected yet. The repository remains private; license selection is
tracked as `O-P001` in [docs/OPEN_QUESTIONS.md](docs/OPEN_QUESTIONS.md).
