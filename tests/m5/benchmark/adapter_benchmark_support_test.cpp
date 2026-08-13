// Adapter-conditional Phase-6 benchmark support tests: AdapterWireReplay
// preconstruction and final-state validation (OD-M5-P6-015/024/068).
// Compiled only when the ProtoAdapter is enabled.

#include "adapter_replay_executor.hpp"
#include "adapter_wire_support.hpp"
#include "small_workload.hpp"

#include <binance_market_data/projection/v1/projection_state/book_projection.hpp>

#include <gtest/gtest.h>

#include <cstdint>

namespace {

namespace core = binance_market_data::projection::v1;
namespace bm = bmd_projection::m5::benchmark;
namespace phase3 = bmd_projection::m5::phase3;

TEST(Phase6AdapterReplay, PreconstructionCoversFullWorkloadAndChecksumIsStable) {
    const auto fixture = phase3::make_spot_small_workload();
    const auto entries = bm::adapter_support::preconstruct_adapter_wire(fixture);
    ASSERT_EQ(entries.size(), fixture.replay.operations.size());
    bm::AdapterReplayExecutor executor{fixture, entries};
    EXPECT_EQ(executor.event_count(), 2'048U);
    std::uint64_t first = 0;
    for (int run = 0; run < 2; ++run) {
        core::BookProjection projection{executor.numeric_spec(), executor.policy()};
        const auto checksum = executor.run(projection);
        if (run == 0) {
            first = checksum;
        }
        EXPECT_EQ(checksum, first);
        EXPECT_EQ(projection.status(), core::ProjectionStatus::Synchronized);
    }
}

TEST(Phase6AdapterReplay, UsdMWorkloadProducesStableDistinctChecksum) {
    const auto spot_fixture = phase3::make_spot_small_workload();
    bm::AdapterReplayExecutor spot{spot_fixture,
                                   bm::adapter_support::preconstruct_adapter_wire(spot_fixture)};
    core::BookProjection spot_projection{spot.numeric_spec(), spot.policy()};
    const auto spot_checksum = spot.run(spot_projection);

    const auto usdm_fixture = phase3::make_usdm_small_workload();
    bm::AdapterReplayExecutor usdm{usdm_fixture,
                                   bm::adapter_support::preconstruct_adapter_wire(usdm_fixture)};
    core::BookProjection usdm_projection{usdm.numeric_spec(), usdm.policy()};
    const auto usdm_checksum = usdm.run(usdm_projection);
    EXPECT_NE(spot_checksum, usdm_checksum);
    EXPECT_EQ(usdm_projection.status(), core::ProjectionStatus::Synchronized);
}

} // namespace
