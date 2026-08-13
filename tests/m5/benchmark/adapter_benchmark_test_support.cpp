#include "adapter_benchmark_test_support.hpp"

#include "adapter_replay_executor.hpp"
#include "adapter_wire_support.hpp"
#include "small_workload.hpp"

#include <binance_market_data/projection/v1/projection_state/book_projection.hpp>

#include <algorithm>
#include <variant>

namespace bmd_projection::m5::benchmark::test_support {
namespace {

namespace core = binance_market_data::projection::v1;
namespace phase3 = bmd_projection::m5::phase3;
namespace adapter = binance_market_data::projection_adapter::v1;

} // namespace

RepeatObservation observe_spot_replay_repeat() {
    const auto fixture = phase3::make_spot_small_workload();
    const auto entries = adapter_support::preconstruct_adapter_wire(fixture);
    const auto first_baseline = std::find_if(
        entries.begin(), entries.end(), [](const adapter_support::PreconstructedEntry& entry) {
            return entry.kind == adapter_support::PreconstructedKind::Baseline;
        });
    bool first_baseline_adapts = false;
    if (first_baseline != entries.end()) {
        const auto adapted = adapter::adapt_exchange_depth_snapshot(first_baseline->baseline_wire,
                                                                    first_baseline->conversion_spec,
                                                                    first_baseline->expected);
        first_baseline_adapts = std::get_if<adapter::AdapterError>(&adapted) == nullptr;
    }
    AdapterReplayExecutor executor{fixture, entries};

    core::BookProjection first_projection{executor.numeric_spec(), executor.policy()};
    const auto first_checksum = executor.run(first_projection);
    core::BookProjection second_projection{executor.numeric_spec(), executor.policy()};
    const auto second_checksum = executor.run(second_projection);

    return {entries.size() == fixture.replay.operations.size(),
            first_baseline_adapts,
            executor.event_count(),
            first_checksum,
            second_checksum,
            first_projection.status() == core::ProjectionStatus::Synchronized,
            second_projection.status() == core::ProjectionStatus::Synchronized};
}

MarketObservation observe_spot_usdm_replays() {
    const auto spot_fixture = phase3::make_spot_small_workload();
    AdapterReplayExecutor spot{spot_fixture,
                               adapter_support::preconstruct_adapter_wire(spot_fixture)};
    core::BookProjection spot_projection{spot.numeric_spec(), spot.policy()};
    const auto spot_checksum = spot.run(spot_projection);

    const auto usdm_fixture = phase3::make_usdm_small_workload();
    AdapterReplayExecutor usdm{usdm_fixture,
                               adapter_support::preconstruct_adapter_wire(usdm_fixture)};
    core::BookProjection usdm_projection{usdm.numeric_spec(), usdm.policy()};
    const auto usdm_checksum = usdm.run(usdm_projection);

    return {spot_checksum, usdm_checksum,
            usdm_projection.status() == core::ProjectionStatus::Synchronized};
}

} // namespace bmd_projection::m5::benchmark::test_support
