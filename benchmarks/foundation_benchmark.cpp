#include <binance_market_data/projection/v1/version.hpp>

#include <benchmark/benchmark.h>

namespace projection = binance_market_data::projection::v1;

static void BM_LibraryVersionAccess(benchmark::State& state) {
    benchmark::DoNotOptimize(projection::library_version());
    for ([[maybe_unused]] auto iteration : state) {
        benchmark::DoNotOptimize(projection::library_version());
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_LibraryVersionAccess);
