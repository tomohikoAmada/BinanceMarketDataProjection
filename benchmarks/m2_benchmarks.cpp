#include "benchmark_support/benchmark_registration.hpp"
#include "benchmark_support/m2_cells.hpp"
#include "benchmark_support/m2_workload_specs.hpp"
#include "benchmark_support/workload_spec.hpp"

#include <binance_market_data/projection/v1/order_book/book_side.hpp>

#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace {

namespace core = binance_market_data::projection::v1;
namespace bm = bmd_projection::m5::benchmark;

constexpr std::size_t kRoutineDepths[] = {8, 100, 1'000};
constexpr std::size_t kFullDepthSet[] = {0, 8, 100, 1'000, 5'000, 10'000};
constexpr std::size_t kBatchSet[] = {1, 10, 100};
constexpr std::size_t kTopNSet[] = {1, 5, 50};

[[nodiscard]] std::string depth_name(std::size_t depth) { return std::to_string(depth); }

[[nodiscard]] std::string_view level_change_name(core::LevelChange change) noexcept {
    switch (change) {
    case core::LevelChange::Inserted:
        return "Inserted";
    case core::LevelChange::Updated:
        return "Updated";
    case core::LevelChange::Removed:
        return "Removed";
    case core::LevelChange::Unchanged:
        return "Unchanged";
    }
    return "Unknown";
}

// ---------------------------------------------------------------------------
// Static workload-spec registration for all M2 cells. The canonical specs are
// produced by the shared M2 identity source (m2_workload_specs) so Phase-6
// timing and Phase-7 allocation measurement carry bit-identical workload
// identities (OD-M5-P7-008). Phase 6 includes the update_mix scaling family.
// ---------------------------------------------------------------------------
namespace {

const auto kM2SpecRegistration = [] {
    bm::register_m2_workload_specs(true);
    return 0;
}();

} // namespace

// ---------------------------------------------------------------------------
// apply_level family. Insert/delete consume one freshly prepared book per
// measured execution (bounded pool, fixed iterations); update alternates
// quantities on one book; missing_delete is idempotent.
// ---------------------------------------------------------------------------
template <bm::M2ApplyLevelKind Kind>
void run_apply_level(benchmark::State& state, const char* name) {
    const auto depth = static_cast<std::size_t>(state.range(0));
    bm::M2ApplyLevelCell warmup_cell{Kind, depth};
    warmup_cell.prepare();
    static_cast<void>(warmup_cell.execute_step(0));
    bm::M2ApplyLevelCell cell{Kind, depth};
    cell.prepare();
    const auto expected =
        Kind == bm::M2ApplyLevelKind::Insert
            ? core::LevelChange::Inserted
            : (Kind == bm::M2ApplyLevelKind::Update
                   ? core::LevelChange::Updated
                   : (Kind == bm::M2ApplyLevelKind::Delete ? core::LevelChange::Removed
                                                           : core::LevelChange::Unchanged));
    std::uint64_t accumulator = 0;
    std::size_t pool_index = 0;
    for ([[maybe_unused]] auto _ : state) {
        if (cell.uses_pool() && pool_index >= cell.pool_size()) {
            state.SkipWithError(std::string{name} + " prepared-state pool exhausted");
            break;
        }
        const auto change = cell.execute_step(pool_index);
        if (change != expected) {
            state.SkipWithError(std::string{name} + " disposition drift: expected " +
                                std::string{level_change_name(expected)} + " got " +
                                std::string{level_change_name(change)});
            break;
        }
        accumulator += static_cast<std::uint64_t>(static_cast<std::uint8_t>(change));
        ++pool_index;
        benchmark::DoNotOptimize(accumulator);
    }
    state.SetItemsProcessed(state.iterations());
}

static void BM_ApplyLevelInsert(benchmark::State& state) {
    run_apply_level<bm::M2ApplyLevelKind::Insert>(state, "M2/apply_level/insert");
}

static void BM_ApplyLevelUpdate(benchmark::State& state) {
    run_apply_level<bm::M2ApplyLevelKind::Update>(state, "M2/apply_level/update");
}

static void BM_ApplyLevelDelete(benchmark::State& state) {
    run_apply_level<bm::M2ApplyLevelKind::Delete>(state, "M2/apply_level/delete");
}

static void BM_ApplyLevelMissingDelete(benchmark::State& state) {
    run_apply_level<bm::M2ApplyLevelKind::MissingDelete>(state, "M2/apply_level/missing_delete");
}

// ---------------------------------------------------------------------------
// apply_updates family. Replacement-heavy batches with cycling quantities on a
// fixed book; the D=0 update_mix cell is the empty-book insertion edge on a
// prepared pool.
// ---------------------------------------------------------------------------
static void run_apply_updates(benchmark::State& state, const char* name,
                              bm::M2ApplyUpdatesMix mix) {
    const auto depth = static_cast<std::size_t>(state.range(0));
    const auto batch = static_cast<std::size_t>(state.range(1));
    bm::M2ApplyUpdatesCell warmup_cell{bm::M2ApplyUpdatesCell::Config{depth, batch, mix}};
    warmup_cell.prepare();
    warmup_cell.execute_step(0);
    bm::M2ApplyUpdatesCell cell{bm::M2ApplyUpdatesCell::Config{depth, batch, mix}};
    cell.prepare();
    std::uint64_t accumulator = 0;
    std::size_t pool_index = 0;
    for ([[maybe_unused]] auto _ : state) {
        if (cell.uses_pool() && pool_index >= cell.pool_size()) {
            state.SkipWithError(std::string{name} + " prepared-state pool exhausted");
            break;
        }
        cell.execute_step(pool_index);
        accumulator += cell.book().level_count(core::BookSide::Bid);
        ++pool_index;
        benchmark::DoNotOptimize(accumulator);
    }
    state.SetItemsProcessed(state.iterations());
}

static void BM_ApplyUpdatesBatch(benchmark::State& state) {
    run_apply_updates(state, "M2/apply_updates", bm::M2ApplyUpdatesMix::ReplacementHeavy);
}

static void BM_ApplyUpdatesUpdateMix(benchmark::State& state) {
    run_apply_updates(state, "M2/apply_updates/update_mix",
                      bm::M2ApplyUpdatesMix::ReplacementHeavy);
}

static void BM_ApplyUpdatesUpdateMixInsertEdge(benchmark::State& state) {
    // D=0 cell of the primary scaling workload: explicitly labelled
    // empty-book insertion edge.
    run_apply_updates(state, "M2/apply_updates/update_mix/0", bm::M2ApplyUpdatesMix::Insertion);
}

// ---------------------------------------------------------------------------
// replace_all: preconstructed vectors, post-state exactly canonical.
// ---------------------------------------------------------------------------
static void BM_ReplaceAll(benchmark::State& state) {
    const auto depth = static_cast<std::size_t>(state.range(0));
    bm::M2ReplaceAllCell warmup_cell{depth};
    warmup_cell.prepare();
    warmup_cell.execute_step();
    bm::M2ReplaceAllCell cell{depth};
    cell.prepare();
    std::uint64_t accumulator = 0;
    for ([[maybe_unused]] auto _ : state) {
        cell.execute_step();
        accumulator += cell.book().level_count(core::BookSide::Bid);
        accumulator += cell.book().level_count(core::BookSide::Ask);
        benchmark::DoNotOptimize(accumulator);
    }
    state.SetItemsProcessed(state.iterations());
}

// ---------------------------------------------------------------------------
// Query family (best, quantity_at, top_levels, all_levels). Fixed state;
// returned objects are consumed.
// ---------------------------------------------------------------------------
template <typename Step> void run_query(benchmark::State& state, Step step, std::int64_t items) {
    const auto depth = static_cast<std::size_t>(state.range(0));
    bm::M2QueryCell warmup_cell{
        bm::M2QueryCell::Config{depth, static_cast<std::size_t>(state.range(1))}};
    warmup_cell.prepare();
    std::uint64_t warmup_accumulator = 0;
    step(warmup_cell, warmup_accumulator);
    benchmark::DoNotOptimize(warmup_accumulator);
    bm::M2QueryCell cell{bm::M2QueryCell::Config{depth, static_cast<std::size_t>(state.range(1))}};
    cell.prepare();
    std::uint64_t accumulator = 0;
    for ([[maybe_unused]] auto _ : state) {
        step(cell, accumulator);
        benchmark::DoNotOptimize(accumulator);
    }
    state.SetItemsProcessed(state.iterations() * items);
}

static void BM_BestBid(benchmark::State& state) {
    run_query(
        state,
        [](const bm::M2QueryCell& cell, std::uint64_t& accumulator) {
            const auto result = cell.best(core::BookSide::Bid);
            accumulator +=
                result.has_value() ? static_cast<std::uint64_t>(result->price.value()) : 0xDEAD;
        },
        1);
}

static void BM_BestAsk(benchmark::State& state) {
    run_query(
        state,
        [](const bm::M2QueryCell& cell, std::uint64_t& accumulator) {
            const auto result = cell.best(core::BookSide::Ask);
            accumulator +=
                result.has_value() ? static_cast<std::uint64_t>(result->price.value()) : 0xDEAD;
        },
        1);
}

static void BM_QuantityAtHit(benchmark::State& state) {
    run_query(
        state,
        [](const bm::M2QueryCell& cell, std::uint64_t& accumulator) {
            const auto result = cell.quantity_at_hit();
            accumulator +=
                result.has_value() ? static_cast<std::uint64_t>(result->value()) : 0xDEAD;
        },
        1);
}

static void BM_QuantityAtMiss(benchmark::State& state) {
    run_query(
        state,
        [](const bm::M2QueryCell& cell, std::uint64_t& accumulator) {
            const auto result = cell.quantity_at_miss();
            accumulator +=
                result.has_value() ? static_cast<std::uint64_t>(result->value()) : 0xDEAD;
        },
        1);
}

static void BM_TopLevels(benchmark::State& state) {
    run_query(
        state,
        [](const bm::M2QueryCell& cell, std::uint64_t& accumulator) {
            const auto bids = cell.top_levels(core::BookSide::Bid);
            accumulator += bids.size();
            if (!bids.empty()) {
                accumulator += static_cast<std::uint64_t>(bids.front().price.value());
            }
        },
        1);
}

static void BM_AllLevels(benchmark::State& state) {
    run_query(
        state,
        [](const bm::M2QueryCell& cell, std::uint64_t& accumulator) {
            const auto bids = cell.all_levels(core::BookSide::Bid);
            const auto asks = cell.all_levels(core::BookSide::Ask);
            accumulator += bids.size() + asks.size();
        },
        1);
}

} // namespace

// ---------------------------------------------------------------------------
// Registration with fixed iterations for pool-based cells.
// ---------------------------------------------------------------------------
namespace {

const auto kM2Registration = [] {
    for (const auto depth : kRoutineDepths) {
        const auto pool_size = bm::pool_iteration_count(depth);
        BMD_PHASE6_REGISTER(BM_ApplyLevelInsert, BM_ApplyLevelInsert)
            ->Name("M2/apply_level/insert/" + depth_name(depth))
            ->ArgName("depth")
            ->Arg(static_cast<std::int64_t>(depth))
            ->Iterations(static_cast<std::int64_t>(pool_size))
            ->Unit(benchmark::kMicrosecond);
        BMD_PHASE6_REGISTER(BM_ApplyLevelDelete, BM_ApplyLevelDelete)
            ->Name("M2/apply_level/delete/" + depth_name(depth))
            ->ArgName("depth")
            ->Arg(static_cast<std::int64_t>(depth))
            ->Iterations(static_cast<std::int64_t>(pool_size))
            ->Unit(benchmark::kMicrosecond);
        BMD_PHASE6_REGISTER(BM_ApplyLevelUpdate, BM_ApplyLevelUpdate)
            ->Name("M2/apply_level/update/" + depth_name(depth))
            ->ArgName("depth")
            ->Arg(static_cast<std::int64_t>(depth))
            ->Unit(benchmark::kMicrosecond)
            ->MinTime(0.05);
        BMD_PHASE6_REGISTER(BM_ApplyLevelMissingDelete, BM_ApplyLevelMissingDelete)
            ->Name("M2/apply_level/missing_delete/" + depth_name(depth))
            ->ArgName("depth")
            ->Arg(static_cast<std::int64_t>(depth))
            ->Unit(benchmark::kMicrosecond)
            ->MinTime(0.05);
        for (const auto batch : kBatchSet) {
            BMD_PHASE6_REGISTER(BM_ApplyUpdatesBatch, BM_ApplyUpdatesBatch)
                ->Name("M2/apply_updates/" + std::to_string(batch) + "/" + depth_name(depth))
                ->ArgNames({"depth", "batch"})
                ->Args({static_cast<std::int64_t>(depth), static_cast<std::int64_t>(batch)})
                ->Unit(benchmark::kMicrosecond)
                ->MinTime(0.05);
        }
        BMD_PHASE6_REGISTER(BM_BestBid, BM_BestBid)
            ->Name("M2/best_bid/" + depth_name(depth))
            ->ArgNames({"depth", "limit"})
            ->Args({static_cast<std::int64_t>(depth), 0})
            ->Unit(benchmark::kMicrosecond)
            ->MinTime(0.05);
        BMD_PHASE6_REGISTER(BM_BestAsk, BM_BestAsk)
            ->Name("M2/best_ask/" + depth_name(depth))
            ->ArgNames({"depth", "limit"})
            ->Args({static_cast<std::int64_t>(depth), 0})
            ->Unit(benchmark::kMicrosecond)
            ->MinTime(0.05);
        BMD_PHASE6_REGISTER(BM_QuantityAtHit, BM_QuantityAtHit)
            ->Name("M2/quantity_at/hit/" + depth_name(depth))
            ->ArgNames({"depth", "limit"})
            ->Args({static_cast<std::int64_t>(depth), 0})
            ->Unit(benchmark::kMicrosecond)
            ->MinTime(0.05);
        BMD_PHASE6_REGISTER(BM_QuantityAtMiss, BM_QuantityAtMiss)
            ->Name("M2/quantity_at/miss/" + depth_name(depth))
            ->ArgNames({"depth", "limit"})
            ->Args({static_cast<std::int64_t>(depth), 0})
            ->Unit(benchmark::kMicrosecond)
            ->MinTime(0.05);
    }
    for (const auto limit : kTopNSet) {
        for (const auto depth : kRoutineDepths) {
            BMD_PHASE6_REGISTER(BM_TopLevels, BM_TopLevels)
                ->Name("M2/top_levels/" + std::to_string(limit) + "/" + depth_name(depth))
                ->ArgNames({"depth", "limit"})
                ->Args({static_cast<std::int64_t>(depth), static_cast<std::int64_t>(limit)})
                ->Unit(benchmark::kMicrosecond)
                ->MinTime(0.05);
        }
    }
    for (const auto depth : kFullDepthSet) {
        BMD_PHASE6_REGISTER(BM_ReplaceAll, BM_ReplaceAll)
            ->Name("M2/replace_all/" + depth_name(depth))
            ->ArgName("depth")
            ->Arg(static_cast<std::int64_t>(depth))
            ->Unit(benchmark::kMicrosecond)
            ->MinTime(0.05);
        BMD_PHASE6_REGISTER(BM_AllLevels, BM_AllLevels)
            ->Name("M2/all_levels/" + depth_name(depth))
            ->ArgNames({"depth", "limit"})
            ->Args({static_cast<std::int64_t>(depth), 0})
            ->Unit(benchmark::kMicrosecond)
            ->MinTime(0.05);
        if (depth == 0) {
            const auto pool_size = bm::pool_iteration_count(0);
            BMD_PHASE6_REGISTER(BM_ApplyUpdatesUpdateMixInsertEdge,
                                BM_ApplyUpdatesUpdateMixInsertEdge)
                ->Name("M2/apply_updates/update_mix/0")
                ->ArgNames({"depth", "batch"})
                ->Args({0, 100})
                ->Iterations(static_cast<std::int64_t>(pool_size))
                ->Unit(benchmark::kMicrosecond);
        } else {
            BMD_PHASE6_REGISTER(BM_ApplyUpdatesUpdateMix, BM_ApplyUpdatesUpdateMix)
                ->Name("M2/apply_updates/update_mix/" + depth_name(depth))
                ->ArgNames({"depth", "batch"})
                ->Args({static_cast<std::int64_t>(depth), 100})
                ->Unit(benchmark::kMicrosecond)
                ->MinTime(0.05);
        }
    }
    return 0;
}();

} // namespace
