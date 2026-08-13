// M4 adapter boundary benchmarks (OD-M5-P6-010/011/012/020/021). Compiled only
// when the ProtoAdapter is enabled; their absence in a Core-only build makes
// the Phase-6 inventory validation fail closed (OD-M5-P6-013/022).

#include "benchmark_support/adapter_wire_support.hpp"
#include "benchmark_support/benchmark_registration.hpp"
#include "benchmark_support/book_state.hpp"
#include "benchmark_support/workload_spec.hpp"

#include <binance_market_data/projection/v1/snapshots.pb.h>
#include <binance_market_data/projection_adapter/v1/proto_adapter.hpp>

#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

namespace adapter = binance_market_data::projection_adapter::v1;
namespace core = binance_market_data::projection::v1;
namespace bm = bmd_projection::m5::benchmark;
namespace wire_support = bmd_projection::m5::benchmark::adapter_support;

constexpr std::size_t kM4DepthSet[] = {8, 100, 1'000};

// ---------------------------------------------------------------------------
// Static workload-spec registration.
// ---------------------------------------------------------------------------
namespace {

struct M4Specs {
    static void register_all() {
        const std::string_view families[] = {"AdaptExchangeDepthSnapshot/Spot",
                                             "AdaptDepthUpdate/Spot",
                                             "CheckedInstall",
                                             "CheckedApply",
                                             "MakeLocalOrderBookSnapshot/Unlimited",
                                             "MakeLocalOrderBookSnapshot/Limited",
                                             "SerializeSnapshot/FreshBuffer",
                                             "SerializeSnapshot/ReusedBuffer"};
        for (const auto family : families) {
            for (const auto depth : kM4DepthSet) {
                const auto name = "M4/" + std::string{family} + "/" + std::to_string(depth);
                auto& builder = bm::register_workload(name);
                builder.set("benchmark_name", name);
                builder.set("operation", family);
                builder.set("depth_per_side", depth);
                builder.set("market", "Spot");
                builder.set("generator_schema", "M5_PHASE6_M4_CELLS_V1");
                builder.set("primary_timer", "cpu");
                builder.set("primary_denominator", "cpu_time");
                if (std::string{family} == "SerializeSnapshot/ReusedBuffer") {
                    builder.set("serialization_buffer_mode", "reused");
                    builder.set("diagnostic", true);
                } else if (std::string{family} == "SerializeSnapshot/FreshBuffer") {
                    builder.set("serialization_buffer_mode", "fresh");
                }
                if (std::string{family} == "AdaptDepthUpdate/Spot" ||
                    std::string{family} == "CheckedApply") {
                    builder.set("update_level_count", wire_support::kM4UpdateLevelCount);
                }
            }
        }
    }
};

const auto kM4SpecRegistration = [] {
    M4Specs::register_all();
    return 0;
}();

} // namespace

[[nodiscard]] std::size_t m4_depth(const benchmark::State& state) {
    return static_cast<std::size_t>(state.range(0));
}

// AdaptExchangeDepthSnapshot: measures only adapt_exchange_depth_snapshot.
// Wire construction, install, snapshot generation, and serialization are all
// excluded.
static void BM_M4AdaptExchangeDepthSnapshot(benchmark::State& state) {
    const auto depth = m4_depth(state);
    const auto identity = wire_support::benchmark_wire_identity();
    const auto wire = wire_support::make_snapshot_wire(depth);
    std::uint64_t accumulator = 0;
    for ([[maybe_unused]] auto _ : state) {
        const auto result =
            adapter::adapt_exchange_depth_snapshot(wire, identity.numeric_spec, identity.expected);
        if (std::holds_alternative<adapter::AdapterError>(result)) {
            state.SkipWithError("M4/AdaptExchangeDepthSnapshot produced an adapter error");
            break;
        }
        const auto& owner = std::get<adapter::AdaptedBookBaseline>(result);
        accumulator += owner.metadata().observed_quality.size();
        benchmark::DoNotOptimize(accumulator);
        state.SetItemsProcessed(1);
    }
}

// AdaptDepthUpdate: measures only adapt_depth_update. Wire construction, Core
// apply, snapshot output, and serialization are excluded.
static void BM_M4AdaptDepthUpdate(benchmark::State& state) {
    const auto identity = wire_support::benchmark_wire_identity();
    const auto wire = wire_support::make_update_wire(1'000'001, 1'000'001, std::nullopt);
    std::uint64_t accumulator = 0;
    for ([[maybe_unused]] auto _ : state) {
        const auto result =
            adapter::adapt_depth_update(wire, identity.numeric_spec, identity.expected);
        if (std::holds_alternative<adapter::AdapterError>(result)) {
            state.SkipWithError("M4/AdaptDepthUpdate produced an adapter error");
            break;
        }
        const auto& owner = std::get<adapter::AdaptedDepthBatch>(result);
        accumulator += owner.metadata().observed_quality.size();
        benchmark::DoNotOptimize(accumulator);
        state.SetItemsProcessed(1);
    }
}

// CheckedInstall: the owner is adapted once outside timing; only install_into
// is timed, against a freshly prepared AwaitingBaseline projection per
// execution.
static void BM_M4CheckedInstall(benchmark::State& state) {
    const auto depth = m4_depth(state);
    const auto identity = wire_support::benchmark_wire_identity();
    const auto wire = wire_support::make_snapshot_wire(depth);
    const auto adapted =
        adapter::adapt_exchange_depth_snapshot(wire, identity.numeric_spec, identity.expected);
    if (!std::holds_alternative<adapter::AdaptedBookBaseline>(adapted)) {
        state.SkipWithError("M4/CheckedInstall pre-adaptation failed");
        return;
    }
    const auto& owner = std::get<adapter::AdaptedBookBaseline>(adapted);
    const auto pool_size = bm::pool_iteration_count(depth);
    bm::StatePool<core::BookProjection> pool;
    pool.fill(pool_size, [&identity] {
        return core::BookProjection{identity.numeric_spec, core::SequencePolicyKind::Spot};
    });
    std::uint64_t accumulator = 0;
    std::size_t pool_index = 0;
    for ([[maybe_unused]] auto _ : state) {
        if (pool_index >= pool_size) {
            state.SkipWithError("M4/CheckedInstall prepared-state pool exhausted");
            break;
        }
        auto target = std::move(pool.at(pool_index));
        const auto installed = owner.install_into(target);
        if (std::holds_alternative<adapter::AdapterError>(installed)) {
            state.SkipWithError("M4/CheckedInstall produced an adapter error");
            break;
        }
        const auto& result = std::get<core::InstallResult>(installed);
        if (result.disposition != core::InstallDisposition::Installed) {
            state.SkipWithError("M4/CheckedInstall disposition drift");
            break;
        }
        accumulator += static_cast<std::uint64_t>(static_cast<std::uint8_t>(result.status_after));
        ++pool_index;
        benchmark::DoNotOptimize(accumulator);
        state.SetItemsProcessed(1);
    }
}

// CheckedApply: the owner is adapted once outside timing; only apply_to is
// timed, against a freshly prepared Synchronized projection per execution.
static void BM_M4CheckedApply(benchmark::State& state) {
    const auto depth = m4_depth(state);
    const auto identity = wire_support::benchmark_wire_identity();
    const auto wire = wire_support::make_update_wire(1'000'002, 1'000'002, std::nullopt);
    const auto adapted =
        adapter::adapt_depth_update(wire, identity.numeric_spec, identity.expected);
    if (!std::holds_alternative<adapter::AdaptedDepthBatch>(adapted)) {
        state.SkipWithError("M4/CheckedApply pre-adaptation failed");
        return;
    }
    const auto& owner = std::get<adapter::AdaptedDepthBatch>(adapted);
    const auto pool_size = bm::pool_iteration_count(depth);
    bm::StatePool<core::BookProjection> pool;
    pool.fill(pool_size, [depth] {
        return bm::build_synchronized_projection(core::SequencePolicyKind::Spot, depth);
    });
    std::uint64_t accumulator = 0;
    std::size_t pool_index = 0;
    for ([[maybe_unused]] auto _ : state) {
        if (pool_index >= pool_size) {
            state.SkipWithError("M4/CheckedApply prepared-state pool exhausted");
            break;
        }
        auto target = std::move(pool.at(pool_index));
        const auto applied = owner.apply_to(target);
        if (std::holds_alternative<adapter::AdapterError>(applied)) {
            state.SkipWithError("M4/CheckedApply produced an adapter error");
            break;
        }
        const auto& result = std::get<core::ApplyResult>(applied);
        if (result.disposition != core::ApplyDisposition::Applied) {
            state.SkipWithError("M4/CheckedApply disposition drift");
            break;
        }
        accumulator += static_cast<std::uint64_t>(static_cast<std::uint8_t>(result.status_after));
        ++pool_index;
        benchmark::DoNotOptimize(accumulator);
        state.SetItemsProcessed(1);
    }
}

// Prepared projection + snapshot context/options; snapshot construction only
// (SerializeToString excluded).
[[nodiscard]] core::BookProjection make_prepared_projection(std::size_t depth) {
    return bm::build_synchronized_projection(core::SequencePolicyKind::Spot, depth);
}

[[nodiscard]] adapter::SnapshotContext make_snapshot_context() {
    return {{"BTCUSDT", core::SequencePolicyKind::Spot},
            "phase6-benchmark",
            "1",
            adapter::SnapshotOrigin::GatewayLive,
            1'234'567,
            std::uint64_t{654'321},
            std::nullopt};
}

static void BM_M4MakeLocalOrderBookSnapshotUnlimited(benchmark::State& state) {
    const auto depth = m4_depth(state);
    const auto projection = make_prepared_projection(depth);
    const auto context = make_snapshot_context();
    const adapter::SnapshotOptions options;
    std::uint64_t accumulator = 0;
    for ([[maybe_unused]] auto _ : state) {
        const auto produced = adapter::make_local_order_book_snapshot(projection, context, options);
        if (std::holds_alternative<adapter::AdapterError>(produced)) {
            state.SkipWithError("M4/MakeLocalOrderBookSnapshot/Unlimited adapter error");
            break;
        }
        const auto& snapshot = std::get<core::LocalOrderBookSnapshot>(produced);
        accumulator += static_cast<std::uint64_t>(snapshot.bids_size() + snapshot.asks_size());
        benchmark::DoNotOptimize(accumulator);
        state.SetItemsProcessed(1);
    }
}

static void BM_M4MakeLocalOrderBookSnapshotLimited(benchmark::State& state) {
    const auto depth = m4_depth(state);
    const auto projection = make_prepared_projection(depth);
    const auto context = make_snapshot_context();
    const auto limit = adapter::DepthLimit::create(20);
    if (std::holds_alternative<adapter::AdapterError>(limit)) {
        state.SkipWithError("M4/MakeLocalOrderBookSnapshot/Limited depth limit invalid");
        return;
    }
    adapter::SnapshotOptions options;
    options.depth_limit = std::get<adapter::DepthLimit>(limit);
    std::uint64_t accumulator = 0;
    for ([[maybe_unused]] auto _ : state) {
        const auto produced = adapter::make_local_order_book_snapshot(projection, context, options);
        if (std::holds_alternative<adapter::AdapterError>(produced)) {
            state.SkipWithError("M4/MakeLocalOrderBookSnapshot/Limited adapter error");
            break;
        }
        const auto& snapshot = std::get<core::LocalOrderBookSnapshot>(produced);
        accumulator += static_cast<std::uint64_t>(snapshot.bids_size() + snapshot.asks_size());
        benchmark::DoNotOptimize(accumulator);
        state.SetItemsProcessed(1);
    }
}

// SerializeSnapshot: preconstructed snapshot, serialization only. The fresh
// buffer path never retains capacity between primary iterations.
static void BM_M4SerializeSnapshotFreshBuffer(benchmark::State& state) {
    const auto depth = m4_depth(state);
    const auto projection = make_prepared_projection(depth);
    const auto produced = adapter::make_local_order_book_snapshot(
        projection, make_snapshot_context(), adapter::SnapshotOptions{});
    if (std::holds_alternative<adapter::AdapterError>(produced)) {
        state.SkipWithError("M4/SerializeSnapshot/FreshBuffer preconstruction failed");
        return;
    }
    const auto& snapshot = std::get<core::LocalOrderBookSnapshot>(produced);
    std::uint64_t accumulator = 0;
    for ([[maybe_unused]] auto _ : state) {
        std::string buffer;
        if (!snapshot.SerializeToString(&buffer)) {
            state.SkipWithError("M4/SerializeSnapshot/FreshBuffer serialization failed");
            break;
        }
        accumulator += buffer.size();
        if (!buffer.empty()) {
            accumulator += static_cast<unsigned char>(buffer.front());
        }
        benchmark::DoNotOptimize(accumulator);
        state.SetItemsProcessed(1);
    }
}

// Optional diagnostic: reused buffer retains capacity across iterations.
static void BM_M4SerializeSnapshotReusedBuffer(benchmark::State& state) {
    const auto depth = m4_depth(state);
    const auto projection = make_prepared_projection(depth);
    const auto produced = adapter::make_local_order_book_snapshot(
        projection, make_snapshot_context(), adapter::SnapshotOptions{});
    if (std::holds_alternative<adapter::AdapterError>(produced)) {
        state.SkipWithError("M4/SerializeSnapshot/ReusedBuffer preconstruction failed");
        return;
    }
    const auto& snapshot = std::get<core::LocalOrderBookSnapshot>(produced);
    std::string buffer;
    std::uint64_t accumulator = 0;
    for ([[maybe_unused]] auto _ : state) {
        buffer.clear();
        if (!snapshot.SerializeToString(&buffer)) {
            state.SkipWithError("M4/SerializeSnapshot/ReusedBuffer serialization failed");
            break;
        }
        accumulator += buffer.size();
        benchmark::DoNotOptimize(accumulator);
        state.SetItemsProcessed(1);
    }
}

} // namespace

namespace {

const auto kM4Registration = [] {
    for (const auto depth : kM4DepthSet) {
        BMD_PHASE6_REGISTER(BM_M4AdaptExchangeDepthSnapshot, BM_M4AdaptExchangeDepthSnapshot)
            ->Name("M4/AdaptExchangeDepthSnapshot/Spot/" + std::to_string(depth))
            ->ArgName("depth")
            ->Arg(static_cast<std::int64_t>(depth))
            ->Unit(benchmark::kMicrosecond)
            ->MinTime(0.05);
        BMD_PHASE6_REGISTER(BM_M4AdaptDepthUpdate, BM_M4AdaptDepthUpdate)
            ->Name("M4/AdaptDepthUpdate/Spot/" + std::to_string(depth))
            ->ArgName("depth")
            ->Arg(static_cast<std::int64_t>(depth))
            ->Unit(benchmark::kMicrosecond)
            ->MinTime(0.05);
        const auto pool_size = bm::pool_iteration_count(depth);
        BMD_PHASE6_REGISTER(BM_M4CheckedInstall, BM_M4CheckedInstall)
            ->Name("M4/CheckedInstall/" + std::to_string(depth))
            ->ArgName("depth")
            ->Arg(static_cast<std::int64_t>(depth))
            ->Iterations(static_cast<std::int64_t>(pool_size))
            ->Unit(benchmark::kMicrosecond);
        BMD_PHASE6_REGISTER(BM_M4CheckedApply, BM_M4CheckedApply)
            ->Name("M4/CheckedApply/" + std::to_string(depth))
            ->ArgName("depth")
            ->Arg(static_cast<std::int64_t>(depth))
            ->Iterations(static_cast<std::int64_t>(pool_size))
            ->Unit(benchmark::kMicrosecond);
        BMD_PHASE6_REGISTER(BM_M4MakeLocalOrderBookSnapshotUnlimited,
                            BM_M4MakeLocalOrderBookSnapshotUnlimited)
            ->Name("M4/MakeLocalOrderBookSnapshot/Unlimited/" + std::to_string(depth))
            ->ArgName("depth")
            ->Arg(static_cast<std::int64_t>(depth))
            ->Unit(benchmark::kMicrosecond)
            ->MinTime(0.05);
        BMD_PHASE6_REGISTER(BM_M4MakeLocalOrderBookSnapshotLimited,
                            BM_M4MakeLocalOrderBookSnapshotLimited)
            ->Name("M4/MakeLocalOrderBookSnapshot/Limited/" + std::to_string(depth))
            ->ArgName("depth")
            ->Arg(static_cast<std::int64_t>(depth))
            ->Unit(benchmark::kMicrosecond)
            ->MinTime(0.05);
        BMD_PHASE6_REGISTER(BM_M4SerializeSnapshotFreshBuffer, BM_M4SerializeSnapshotFreshBuffer)
            ->Name("M4/SerializeSnapshot/FreshBuffer/" + std::to_string(depth))
            ->ArgName("depth")
            ->Arg(static_cast<std::int64_t>(depth))
            ->Unit(benchmark::kMicrosecond)
            ->MinTime(0.05);
        BMD_PHASE6_REGISTER(BM_M4SerializeSnapshotReusedBuffer, BM_M4SerializeSnapshotReusedBuffer)
            ->Name("M4/SerializeSnapshot/ReusedBuffer/" + std::to_string(depth))
            ->ArgName("depth")
            ->Arg(static_cast<std::int64_t>(depth))
            ->Unit(benchmark::kMicrosecond)
            ->MinTime(0.05);
    }
    return 0;
}();

} // namespace
