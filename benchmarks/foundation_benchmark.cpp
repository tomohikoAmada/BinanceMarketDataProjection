#include <binance_market_data/projection/v1/version.hpp>

#include <benchmark/benchmark.h>

namespace projection = binance_market_data::projection::v1;

static void BM_LibraryVersionAccess(benchmark::State& state) {
    for ([[maybe_unused]] auto iteration : state) {
        benchmark::DoNotOptimize(projection::library_version());
    }
}

BENCHMARK(BM_LibraryVersionAccess);
