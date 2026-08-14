#include "benchmark_support/benchmark_registration.hpp"
#include "benchmark_support/m2_cells.hpp"
#include "benchmark_support/m3_cells.hpp"
#include "benchmark_support/workload_spec.hpp"

#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace {

namespace core = binance_market_data::projection::v1;
namespace bm = bmd_projection::m5::benchmark;

constexpr std::size_t kDepthSet[] = {0, 8, 100, 1'000, 5'000, 10'000};
constexpr std::size_t kBatchSet[] = {0, 1, 10, 100};
constexpr std::size_t kComponentDepthSet[] = {8, 100, 1'000};

[[nodiscard]] std::string_view policy_label(core::SequencePolicyKind policy) noexcept {
    return policy == core::SequencePolicyKind::Spot ? "Spot" : "UsdMPerpetual";
}

[[nodiscard]] std::string_view kind_label(bm::M3ClassificationKind kind) noexcept {
    switch (kind) {
    case bm::M3ClassificationKind::Stale:
        return "Stale";
    case bm::M3ClassificationKind::Duplicate:
        return "Duplicate";
    case bm::M3ClassificationKind::Gap:
        return "Gap";
    case bm::M3ClassificationKind::Reset:
        return "Reset";
    case bm::M3ClassificationKind::BaselineInstall:
        return "BaselineInstall";
    }
    return "Unknown";
}

// ---------------------------------------------------------------------------
// Static workload-spec registration (complete 48-cell matrix plus
// classifications and component/proxy cells, independent of the run filter).
// ---------------------------------------------------------------------------
namespace {

const auto kM3SpecRegistration = [] {
    for (const auto policy :
         {core::SequencePolicyKind::Spot, core::SequencePolicyKind::UsdMPerpetual}) {
        for (const auto depth : kDepthSet) {
            for (const auto batch : kBatchSet) {
                const auto name = "M3/LiveApply/Accepted/" + std::string{policy_label(policy)} +
                                  "/D" + std::to_string(depth) + "/B" + std::to_string(batch);
                auto& builder = bm::register_workload(name);
                builder.set("benchmark_name", name);
                builder.set("operation", "BookProjection::apply");
                builder.set("policy", policy_label(policy));
                builder.set("depth_per_side", depth);
                builder.set("batch", batch);
                builder.set("expected_disposition", "Applied");
                builder.set("precondition", "Synchronized");
                if (depth == 0 && batch > 0) {
                    builder.set("edge", "empty_book_insertion");
                }
                if (batch == 0) {
                    builder.set("edge", "advancing_empty_level_batch");
                }
                builder.set("generator_schema", "M5_PHASE6_M3_CELLS_V1");
                builder.set("generated_workload_sha256",
                            bm::m3_accepted_generated_sha256({policy, depth, batch}));
                builder.set("primary_timer", "cpu");
                builder.set("primary_denominator", "cpu_time");
            }
        }
        for (const auto kind :
             {bm::M3ClassificationKind::Stale, bm::M3ClassificationKind::Duplicate,
              bm::M3ClassificationKind::Gap, bm::M3ClassificationKind::Reset,
              bm::M3ClassificationKind::BaselineInstall}) {
            const auto name = "M3/Classification/" + std::string{kind_label(kind)} + "/" +
                              std::string{policy_label(policy)};
            auto& builder = bm::register_workload(name);
            builder.set("benchmark_name", name);
            builder.set("operation", "classification");
            builder.set("policy", policy_label(policy));
            builder.set("classification", kind_label(kind));
            builder.set("depth_per_side", bm::kM3ClassificationDepth);
            builder.set("generator_schema", "M5_PHASE6_M3_CELLS_V1");
            builder.set(
                "generated_workload_sha256",
                bm::m3_classification_generated_sha256({kind, policy, bm::kM3ClassificationDepth}));
            builder.set("primary_timer", "cpu");
            builder.set("primary_denominator", "cpu_time");
        }
    }
    // Component/proxy cells are policy-neutral book operations; Spot is the
    // recorded projection policy.
    for (const auto depth : kComponentDepthSet) {
        const auto register_proxy = [depth](std::string_view family, std::string_view operation) {
            const auto name = "M3/" + std::string{family} + "/" + std::to_string(depth);
            auto& builder = bm::register_workload(name);
            builder.set("benchmark_name", name);
            builder.set("operation", operation);
            builder.set("depth_per_side", depth);
            builder.set("policy", "Spot");
            builder.set("proxy_component_measurement", "true");
            builder.set("generator_schema", "M5_PHASE6_M3_CELLS_V1");
            builder.set("generated_workload_sha256",
                        bm::m3_proxy_generated_sha256(operation, depth));
            builder.set("primary_timer", "cpu");
            builder.set("primary_denominator", "cpu_time");
        };
        register_proxy("Component/AllLevelsBothSides", "all_levels_both_sides");
        register_proxy("Proxy/CandidateRebuildFromVectors", "replace_all");
        register_proxy("Proxy/CandidateApplyUpdates", "apply_updates");
        register_proxy("Proxy/OrderBookMoveCommit", "move_assignment_with_destruction");
    }
    return 0;
}();

} // namespace

// ---------------------------------------------------------------------------
// Accepted live-apply cells. Every measured execution begins Synchronized and
// must return Applied.
// ---------------------------------------------------------------------------
static void BM_M3AcceptedLiveApply(benchmark::State& state) {
    const auto policy = static_cast<core::SequencePolicyKind>(state.range(0));
    const auto depth = static_cast<std::size_t>(state.range(1));
    const auto batch = static_cast<std::size_t>(state.range(2));
    bm::M3AcceptedCell warmup_cell{bm::M3AcceptedCell::Config{policy, depth, batch}};
    warmup_cell.prepare();
    const auto warmup_result = warmup_cell.execute_step(0);
    if (warmup_result.disposition != core::ApplyDisposition::Applied) {
        state.SkipWithError("M3 accepted explicit warmup disposition drift");
        return;
    }
    bm::M3AcceptedCell cell{bm::M3AcceptedCell::Config{policy, depth, batch}};
    cell.prepare();
    std::uint64_t accumulator = 0;
    std::size_t pool_index = 0;
    for ([[maybe_unused]] auto _ : state) {
        if (cell.uses_pool() && pool_index >= cell.pool_size()) {
            state.SkipWithError("M3 accepted cell prepared-state pool exhausted");
            break;
        }
        const auto result = cell.execute_step(pool_index);
        if (result.disposition != core::ApplyDisposition::Applied ||
            result.status_after != core::ProjectionStatus::Synchronized) {
            state.SkipWithError("M3 accepted cell did not return Applied/Synchronized");
            break;
        }
        accumulator += static_cast<std::uint64_t>(static_cast<std::uint8_t>(result.disposition));
        accumulator += cell.current_update_id().value();
        ++pool_index;
        benchmark::DoNotOptimize(accumulator);
    }
    state.SetItemsProcessed(state.iterations());
}

// ---------------------------------------------------------------------------
// Classification cells: stale/duplicate are non-mutating; gap/reset/baseline
// install are one-shot against freshly prepared state pools.
// ---------------------------------------------------------------------------
static void BM_M3Classification(benchmark::State& state) {
    const auto kind = static_cast<bm::M3ClassificationKind>(state.range(0));
    const auto policy = static_cast<core::SequencePolicyKind>(state.range(1));
    bm::M3ClassificationCell warmup_cell{
        bm::M3ClassificationCell::Config{kind, policy, bm::kM3ClassificationDepth}};
    warmup_cell.prepare();
    static_cast<void>(warmup_cell.execute_step(0));
    bm::M3ClassificationCell cell{
        bm::M3ClassificationCell::Config{kind, policy, bm::kM3ClassificationDepth}};
    cell.prepare();
    std::uint64_t accumulator = 0;
    std::size_t pool_index = 0;
    for ([[maybe_unused]] auto _ : state) {
        if (cell.uses_pool() && pool_index >= cell.pool_size()) {
            state.SkipWithError("M3 classification cell prepared-state pool exhausted");
            break;
        }
        const auto result = cell.execute_step(pool_index);
        bool valid = false;
        switch (kind) {
        case bm::M3ClassificationKind::Stale:
            valid = result.apply_disposition == core::ApplyDisposition::IgnoredStale;
            break;
        case bm::M3ClassificationKind::Duplicate:
            valid = result.apply_disposition == core::ApplyDisposition::IgnoredDuplicate;
            break;
        case bm::M3ClassificationKind::Gap:
            valid = result.apply_disposition == core::ApplyDisposition::GapDetected;
            break;
        case bm::M3ClassificationKind::Reset:
            valid = result.status_after == core::ProjectionStatus::AwaitingBaseline;
            break;
        case bm::M3ClassificationKind::BaselineInstall:
            valid = result.install_disposition == core::InstallDisposition::Installed &&
                    result.status_after == core::ProjectionStatus::AwaitingBridge;
            break;
        }
        if (!valid) {
            state.SkipWithError("M3 classification cell disposition drift");
            break;
        }
        accumulator += static_cast<std::uint64_t>(static_cast<std::uint8_t>(result.status_after));
        ++pool_index;
        benchmark::DoNotOptimize(accumulator);
    }
    state.SetItemsProcessed(state.iterations());
}

// ---------------------------------------------------------------------------
// Component/proxy cells (OD-M5-P6-009).
// ---------------------------------------------------------------------------
static void BM_M3ComponentAllLevelsBothSides(benchmark::State& state) {
    const auto depth = static_cast<std::size_t>(state.range(0));
    bm::M3ProxyCells warmup_cells{depth};
    warmup_cells.prepare();
    benchmark::DoNotOptimize(warmup_cells.all_levels_both_sides());
    bm::M3ProxyCells cells{depth};
    cells.prepare();
    std::uint64_t accumulator = 0;
    for ([[maybe_unused]] auto _ : state) {
        const auto levels = cells.all_levels_both_sides();
        accumulator += levels.size();
        benchmark::DoNotOptimize(accumulator);
    }
    state.SetItemsProcessed(state.iterations());
}

static void BM_M3ProxyCandidateRebuildFromVectors(benchmark::State& state) {
    const auto depth = static_cast<std::size_t>(state.range(0));
    bm::M3ProxyCells cells{depth};
    cells.prepare();
    {
        auto warmup_candidate = cells.candidate_rebuild_from_vectors();
        benchmark::DoNotOptimize(warmup_candidate);
    }
    std::uint64_t accumulator = 0;
    for ([[maybe_unused]] auto _ : state) {
        auto candidate = cells.candidate_rebuild_from_vectors();
        accumulator += candidate.level_count(core::BookSide::Bid);
        benchmark::DoNotOptimize(accumulator);
    }
    state.SetItemsProcessed(state.iterations());
}

static void BM_M3ProxyCandidateApplyUpdates(benchmark::State& state) {
    const auto depth = static_cast<std::size_t>(state.range(0));
    bm::M3ProxyCells cells{depth};
    cells.prepare();
    const auto pool_size = bm::pool_iteration_count(depth);
    auto warmup_candidate = cells.candidate_rebuild_from_vectors();
    cells.candidate_apply_updates(warmup_candidate, 0);
    benchmark::DoNotOptimize(warmup_candidate);
    bm::StatePool<core::OrderBook> candidate_pool;
    candidate_pool.fill(pool_size, [&cells] { return cells.candidate_rebuild_from_vectors(); });
    std::uint64_t accumulator = 0;
    std::size_t pool_index = 0;
    for ([[maybe_unused]] auto _ : state) {
        if (pool_index >= pool_size) {
            state.SkipWithError("candidate-apply prepared-state pool exhausted");
            break;
        }
        auto& candidate = candidate_pool.at(pool_index);
        cells.candidate_apply_updates(candidate, pool_index);
        accumulator += candidate.level_count(core::BookSide::Bid);
        ++pool_index;
        benchmark::DoNotOptimize(accumulator);
    }
    state.SetItemsProcessed(state.iterations());
}

static void BM_M3ProxyOrderBookMoveCommit(benchmark::State& state) {
    const auto depth = static_cast<std::size_t>(state.range(0));
    const auto pool_size = bm::pool_iteration_count(depth);
    {
        auto warmup_destination = bm::build_order_book(depth);
        auto warmup_source = bm::build_order_book(depth);
        bm::M3ProxyCells::move_commit(warmup_destination, std::move(warmup_source));
        benchmark::DoNotOptimize(warmup_destination);
    }
    // Prepared pools: destinations hold the populated old book (move-assignment
    // includes destination destruction), sources hold the candidate book.
    bm::StatePool<core::OrderBook> destination_pool;
    bm::StatePool<core::OrderBook> source_pool;
    {
        std::vector<core::BookLevel> old_bids;
        std::vector<core::BookLevel> old_asks;
        old_bids.reserve(depth);
        old_asks.reserve(depth);
        for (std::size_t index = 0; index < depth; ++index) {
            old_bids.push_back({bm::price_units(30'000 - static_cast<std::int64_t>(index)),
                                bm::quantity_units(1)});
            old_asks.push_back({bm::price_units(30'001 + static_cast<std::int64_t>(index)),
                                bm::quantity_units(1)});
        }
        destination_pool.fill(pool_size, [&] {
            core::OrderBook book{bm::benchmark_numeric_spec()};
            book.replace_all(old_bids, old_asks);
            return book;
        });
        source_pool.fill(pool_size, [depth] { return bm::build_order_book(depth); });
    }
    std::uint64_t accumulator = 0;
    std::size_t pool_index = 0;
    for ([[maybe_unused]] auto _ : state) {
        if (pool_index >= pool_size) {
            state.SkipWithError("move-commit prepared-state pool exhausted");
            break;
        }
        bm::M3ProxyCells::move_commit(destination_pool.at(pool_index),
                                      std::move(source_pool.at(pool_index)));
        accumulator += destination_pool.at(pool_index).level_count(core::BookSide::Bid);
        ++pool_index;
        benchmark::DoNotOptimize(accumulator);
    }
    state.SetItemsProcessed(state.iterations());
}

} // namespace

// ---------------------------------------------------------------------------
// Registration: the complete 48-cell accepted matrix plus classifications and
// component/proxy cells. Pool-based cells use fixed iterations.
// ---------------------------------------------------------------------------
namespace {

const auto kM3Registration = [] {
    for (const auto policy :
         {core::SequencePolicyKind::Spot, core::SequencePolicyKind::UsdMPerpetual}) {
        for (const auto depth : kDepthSet) {
            for (const auto batch : kBatchSet) {
                const auto name = "M3/LiveApply/Accepted/" + std::string{policy_label(policy)} +
                                  "/D" + std::to_string(depth) + "/B" + std::to_string(batch);
                const auto pool_cell = depth == 0 && batch > 0;
                const auto pool_size = bm::pool_iteration_count(depth);
                if (pool_cell) {
                    BMD_PHASE6_REGISTER(BM_M3AcceptedLiveApply, BM_M3AcceptedLiveApply)
                        ->Name(name)
                        ->ArgNames({"policy", "depth", "batch"})
                        ->Args({static_cast<std::int64_t>(policy), static_cast<std::int64_t>(depth),
                                static_cast<std::int64_t>(batch)})
                        ->Unit(benchmark::kMicrosecond)
                        ->Iterations(static_cast<std::int64_t>(pool_size));
                } else {
                    BMD_PHASE6_REGISTER(BM_M3AcceptedLiveApply, BM_M3AcceptedLiveApply)
                        ->Name(name)
                        ->ArgNames({"policy", "depth", "batch"})
                        ->Args({static_cast<std::int64_t>(policy), static_cast<std::int64_t>(depth),
                                static_cast<std::int64_t>(batch)})
                        ->Unit(benchmark::kMicrosecond)
                        ->MinTime(0.05);
                }
            }
        }
        for (const auto kind :
             {bm::M3ClassificationKind::Stale, bm::M3ClassificationKind::Duplicate,
              bm::M3ClassificationKind::Gap, bm::M3ClassificationKind::Reset,
              bm::M3ClassificationKind::BaselineInstall}) {
            const auto name = "M3/Classification/" + std::string{kind_label(kind)} + "/" +
                              std::string{policy_label(policy)};
            const auto pool_cell = kind != bm::M3ClassificationKind::Stale &&
                                   kind != bm::M3ClassificationKind::Duplicate;
            if (pool_cell) {
                BMD_PHASE6_REGISTER(BM_M3Classification, BM_M3Classification)
                    ->Name(name)
                    ->ArgNames({"kind", "policy"})
                    ->Args({static_cast<std::int64_t>(kind), static_cast<std::int64_t>(policy)})
                    ->Unit(benchmark::kMicrosecond)
                    ->Iterations(static_cast<std::int64_t>(
                        bm::pool_iteration_count(bm::kM3ClassificationDepth)));
            } else {
                BMD_PHASE6_REGISTER(BM_M3Classification, BM_M3Classification)
                    ->Name(name)
                    ->ArgNames({"kind", "policy"})
                    ->Args({static_cast<std::int64_t>(kind), static_cast<std::int64_t>(policy)})
                    ->Unit(benchmark::kMicrosecond)
                    ->MinTime(0.05);
            }
        }
    }
    for (const auto depth : kComponentDepthSet) {
        BMD_PHASE6_REGISTER(BM_M3ComponentAllLevelsBothSides, BM_M3ComponentAllLevelsBothSides)
            ->Name("M3/Component/AllLevelsBothSides/" + std::to_string(depth))
            ->ArgName("depth")
            ->Arg(static_cast<std::int64_t>(depth))
            ->Unit(benchmark::kMicrosecond)
            ->MinTime(0.05);
        BMD_PHASE6_REGISTER(BM_M3ProxyCandidateRebuildFromVectors,
                            BM_M3ProxyCandidateRebuildFromVectors)
            ->Name("M3/Proxy/CandidateRebuildFromVectors/" + std::to_string(depth))
            ->ArgName("depth")
            ->Arg(static_cast<std::int64_t>(depth))
            ->Unit(benchmark::kMicrosecond)
            ->MinTime(0.05);
        BMD_PHASE6_REGISTER(BM_M3ProxyCandidateApplyUpdates, BM_M3ProxyCandidateApplyUpdates)
            ->Name("M3/Proxy/CandidateApplyUpdates/" + std::to_string(depth))
            ->ArgName("depth")
            ->Arg(static_cast<std::int64_t>(depth))
            ->Unit(benchmark::kMicrosecond)
            ->Iterations(static_cast<std::int64_t>(bm::pool_iteration_count(depth)));
        BMD_PHASE6_REGISTER(BM_M3ProxyOrderBookMoveCommit, BM_M3ProxyOrderBookMoveCommit)
            ->Name("M3/Proxy/OrderBookMoveCommit/" + std::to_string(depth))
            ->ArgName("depth")
            ->Arg(static_cast<std::int64_t>(depth))
            ->Iterations(static_cast<std::int64_t>(bm::pool_iteration_count(depth)))
            ->Unit(benchmark::kMicrosecond);
    }
    return 0;
}();

} // namespace
