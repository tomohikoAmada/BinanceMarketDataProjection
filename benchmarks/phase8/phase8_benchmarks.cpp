#include "phase8_absl_btree_map.hpp"
#include "phase8_sorted_vector_batch_lww.hpp"
#include "phase8_sorted_vector_naive.hpp"
#include "phase8_std_map_control.hpp"
#include "phase8_workload.hpp"

#include "../benchmark_support/book_state.hpp"

#include <benchmark/benchmark.h>

#include <cstddef>
#include <span>
#include <string>

namespace bmd_projection::m5::phase8 {
namespace {

// One sink is shared by all benchmark registrations. It makes the result of
// every measured operation observable without adding logging or serialization
// to the timed region.
volatile std::size_t g_phase8_sink = 0;

template <typename Model> void populate(Model& model, const Phase8Workload& workload) {
    model.replace_all(std::span{workload.bids}, std::span{workload.asks});
}

template <typename Model>
void benchmark_workload(::benchmark::State& state, const Phase8Workload& workload) {
    const auto spec = bmd_projection::m5::benchmark::benchmark_numeric_spec();
    switch (workload.operation) {
    case Phase8Operation::apply_level: {
        for (auto _ : state) {
            static_cast<void>(_);
            state.PauseTiming();
            Model model{spec};
            populate(model, workload);
            state.ResumeTiming();
            const auto& update = workload.updates.front();
            const auto change = model.apply_level(update.side, update.price, update.quantity);
            g_phase8_sink ^= static_cast<std::size_t>(change);
            ::benchmark::DoNotOptimize(g_phase8_sink);
            ::benchmark::ClobberMemory();
        }
        state.SetItemsProcessed(state.iterations());
        break;
    }
    case Phase8Operation::apply_updates: {
        std::size_t update_step = 0;
        for (auto _ : state) {
            static_cast<void>(_);
            state.PauseTiming();
            Model model{spec};
            populate(model, workload);
            state.ResumeTiming();
            const auto& updates =
                workload.update_batches.at(update_step % workload.update_batches.size());
            ++update_step;
            model.apply_updates(std::span{updates});
            g_phase8_sink ^= model.level_count(core::BookSide::Bid);
            g_phase8_sink ^= model.level_count(core::BookSide::Ask);
            ::benchmark::DoNotOptimize(g_phase8_sink);
            ::benchmark::ClobberMemory();
        }
        state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()) *
                                static_cast<std::int64_t>(workload.batch));
        break;
    }
    case Phase8Operation::replace_all: {
        for (auto _ : state) {
            static_cast<void>(_);
            state.PauseTiming();
            Model model{spec};
            populate(model, workload);
            state.ResumeTiming();
            model.replace_all(std::span{workload.bids}, std::span{workload.asks});
            g_phase8_sink ^= model.level_count(core::BookSide::Bid);
            ::benchmark::DoNotOptimize(g_phase8_sink);
            ::benchmark::ClobberMemory();
        }
        state.SetItemsProcessed(state.iterations());
        break;
    }
    case Phase8Operation::top_levels: {
        Model model{spec};
        populate(model, workload);
        for (auto _ : state) {
            static_cast<void>(_);
            const auto levels = model.top_levels(core::BookSide::Bid, workload.query_limit);
            g_phase8_sink ^= levels.size();
            if (!levels.empty()) {
                g_phase8_sink ^= static_cast<std::size_t>(levels.front().price.value());
            }
            ::benchmark::DoNotOptimize(g_phase8_sink);
            ::benchmark::ClobberMemory();
        }
        state.SetItemsProcessed(state.iterations());
        break;
    }
    }
}

template <typename Model> void register_model() {
    for (const auto& workload : phase8_workloads()) {
        const std::string name = std::string{Model::model_id()} + "/" + workload.id;
        ::benchmark::RegisterBenchmark(name.c_str(), [&workload](::benchmark::State& state) {
            benchmark_workload<Model>(state, workload);
        });
    }
}

const bool kRegistered = [] {
    register_model<Phase8StdMapControl<>>();
    register_model<Phase8SortedVectorNaive<>>();
    register_model<Phase8AbslBtreeMap<>>();
    register_model<Phase8SortedVectorBatchLww<>>();
    return true;
}();

} // namespace
} // namespace bmd_projection::m5::phase8
