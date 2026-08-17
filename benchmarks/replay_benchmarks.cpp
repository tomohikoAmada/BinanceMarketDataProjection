// Production replay throughput benchmarks (OD-M5-P6-014/015/016/019/020/028).
// Wall time is the primary denominator (UseRealTime);
// CPU time is reported separately by Google Benchmark. Differential
// verification runs exactly once outside any measured region and is never the
// production throughput executor.

#include "benchmark_support/core_replay_executor.hpp"
#include "benchmark_support/replay_workload_specs.hpp"
#include "benchmark_support/workload_spec.hpp"
#include "core_production_side.hpp"
#include "reference_side.hpp"
#include "replay_driver.hpp"
#include "small_workload.hpp"

#if defined(BMD_PROJECTION_PHASE6_ADAPTER_ENABLED)
#include "adapter_production_side.hpp"
#include "benchmark_support/adapter_replay_executor.hpp"
#include "benchmark_support/adapter_wire_support.hpp"
#endif

#include <binance_market_data/projection/v1/projection_state/book_projection.hpp>

#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace core = binance_market_data::projection::v1;
namespace bm = bmd_projection::m5::benchmark;
namespace oracle = bmd_projection::m5::oracle;
namespace phase3 = bmd_projection::m5::phase3;
namespace replay = bmd_projection::m5::replay;

// ---------------------------------------------------------------------------
// Static workload-spec registration. The canonical specs come from the shared
// replay identity source (replay_workload_specs) so Phase-6 timing and
// Phase-7 replay allocation measurement carry bit-identical workload
// identities (OD-M5-P7-013).
// ---------------------------------------------------------------------------
namespace {

const auto kReplaySpecRegistration = [] {
    bm::register_replay_workload_specs();
    return 0;
}();

} // namespace

// ---------------------------------------------------------------------------
// Differential semantic preflight: production vs reference oracle, run once
// outside timing. A divergent workload never reaches measurement.
// ---------------------------------------------------------------------------
void verify_fixture_differentially(const replay::ReplayFixture& fixture, oracle::ReplayMode mode) {
    std::unique_ptr<oracle::ReplaySide> production;
    std::unique_ptr<oracle::ReplaySide> reference;
    switch (mode) {
    case oracle::ReplayMode::CoreOnly:
        production = oracle::make_core_production_side(fixture);
        reference = oracle::make_reference_side(fixture, oracle::ReplayMode::CoreOnly);
        break;
    case oracle::ReplayMode::AdapterEnabled:
#if defined(BMD_PROJECTION_PHASE6_ADAPTER_ENABLED)
        production = oracle::make_adapter_production_side(fixture);
        reference = oracle::make_reference_side(fixture, oracle::ReplayMode::AdapterEnabled);
#else
        std::abort();
#endif
        break;
    }
    oracle::ReplayDriver driver{fixture, std::move(production), std::move(reference),
                                oracle::ObservationRetention::RetainNone};
    const auto outcome = driver.run();
    if (outcome.first_divergence.has_value() ||
        outcome.processed_events != fixture.replay.operations.size()) {
        std::abort();
    }
}

// ---------------------------------------------------------------------------
// CoreNormalizedReplay: the timed production Core path. Do not use
// CoreProductionSide::observe() as the throughput executor.
// ---------------------------------------------------------------------------
static void run_core_replay(benchmark::State& state,
                            const std::function<replay::ReplayFixture()>& make_fixture,
                            const char* name) {
    const auto fixture = make_fixture();
    verify_fixture_differentially(fixture, oracle::ReplayMode::CoreOnly);
    bm::CoreReplayExecutor executor{fixture};
    {
        core::BookProjection projection{executor.numeric_spec(), executor.policy()};
        executor.set_expected_checksum(executor.run(projection));
    }
    {
        core::BookProjection warmup_projection{executor.numeric_spec(), executor.policy()};
        if (executor.run(warmup_projection) != executor.expected_checksum()) {
            state.SkipWithError(std::string{name} + " explicit warmup checksum mismatch");
            return;
        }
    }
    const auto expected = executor.expected_checksum();
    std::uint64_t accumulator = 0;
    for ([[maybe_unused]] auto _ : state) {
        core::BookProjection projection{executor.numeric_spec(), executor.policy()};
        const auto checksum = executor.run(projection);
        if (checksum != expected) {
            state.SkipWithError(std::string{name} + " replay checksum mismatch");
            break;
        }
        accumulator += checksum;
        benchmark::DoNotOptimize(accumulator);
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                            static_cast<std::int64_t>(executor.event_count()));
}

static void BM_CoreNormalizedReplaySpot(benchmark::State& state) {
    run_core_replay(
        state, [] { return phase3::make_spot_small_workload(); }, "CoreNormalizedReplay/Spot");
}

static void BM_CoreNormalizedReplayUsdM(benchmark::State& state) {
    run_core_replay(
        state, [] { return phase3::make_usdm_small_workload(); },
        "CoreNormalizedReplay/UsdMPerpetual");
}

#if defined(BMD_PROJECTION_PHASE6_ADAPTER_ENABLED)
// ---------------------------------------------------------------------------
// AdapterWireReplay: preconstructed wire -> M4 adaptation -> checked M3
// invocation -> minimal consumption. Core and adapter replay results remain
// distinct.
// ---------------------------------------------------------------------------
static void run_adapter_replay(benchmark::State& state,
                               const std::function<replay::ReplayFixture()>& make_fixture,
                               const char* name) {
    const auto fixture = make_fixture();
    verify_fixture_differentially(fixture, oracle::ReplayMode::AdapterEnabled);
    bm::AdapterReplayExecutor executor{fixture,
                                       bm::adapter_support::preconstruct_adapter_wire(fixture)};
    {
        core::BookProjection projection{executor.numeric_spec(), executor.policy()};
        executor.set_expected_checksum(executor.run(projection));
    }
    {
        core::BookProjection warmup_projection{executor.numeric_spec(), executor.policy()};
        if (executor.run(warmup_projection) != executor.expected_checksum()) {
            state.SkipWithError(std::string{name} + " explicit warmup checksum mismatch");
            return;
        }
    }
    const auto expected = executor.expected_checksum();
    std::uint64_t accumulator = 0;
    for ([[maybe_unused]] auto _ : state) {
        core::BookProjection projection{executor.numeric_spec(), executor.policy()};
        const auto checksum = executor.run(projection);
        if (checksum != expected) {
            state.SkipWithError(std::string{name} + " replay checksum mismatch");
            break;
        }
        accumulator += checksum;
        benchmark::DoNotOptimize(accumulator);
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                            static_cast<std::int64_t>(executor.event_count()));
}

static void BM_AdapterWireReplaySpot(benchmark::State& state) {
    run_adapter_replay(
        state, [] { return phase3::make_spot_small_workload(); }, "AdapterWireReplay/Spot");
}

static void BM_AdapterWireReplayUsdM(benchmark::State& state) {
    run_adapter_replay(
        state, [] { return phase3::make_usdm_small_workload(); },
        "AdapterWireReplay/UsdMPerpetual");
}
#endif

} // namespace

BENCHMARK(BM_CoreNormalizedReplaySpot)
    ->Name("CoreNormalizedReplay/Spot")
    ->Unit(benchmark::kMicrosecond)
    ->MinTime(0.05)
    ->UseRealTime();
BENCHMARK(BM_CoreNormalizedReplayUsdM)
    ->Name("CoreNormalizedReplay/UsdMPerpetual")
    ->Unit(benchmark::kMicrosecond)
    ->MinTime(0.05)
    ->UseRealTime();
#if defined(BMD_PROJECTION_PHASE6_ADAPTER_ENABLED)
BENCHMARK(BM_AdapterWireReplaySpot)
    ->Name("AdapterWireReplay/Spot")
    ->Unit(benchmark::kMicrosecond)
    ->MinTime(0.05)
    ->UseRealTime();
BENCHMARK(BM_AdapterWireReplayUsdM)
    ->Name("AdapterWireReplay/UsdMPerpetual")
    ->Unit(benchmark::kMicrosecond)
    ->MinTime(0.05)
    ->UseRealTime();
#endif
