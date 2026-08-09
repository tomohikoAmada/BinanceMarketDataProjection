#include "small_workload.hpp"

#include "adapter_production_side.hpp"
#include "reference_side.hpp"
#include "replay_driver.hpp"

#include <gtest/gtest.h>

namespace {

namespace oracle = bmd_projection::m5::oracle;
namespace phase3 = bmd_projection::m5::phase3;

[[nodiscard]] oracle::ReplayOutcome
run_adapter(const bmd_projection::m5::replay::ReplayFixture& fixture) {
    oracle::ReplayDriver driver{
        fixture, oracle::make_adapter_production_side(fixture),
        oracle::make_reference_side(fixture, oracle::ReplayMode::AdapterEnabled),
        oracle::ObservationRetention::RetainNone};
    return driver.run();
}

TEST(Phase3SmallAdapterReplayTest, SpotAndUsdMCompareEveryAdapterEventRepeatably) {
    for (const auto& fixture :
         {phase3::make_spot_small_workload(), phase3::make_usdm_small_workload()}) {
        SCOPED_TRACE(fixture.identity.fixture_id);
        const auto first = run_adapter(fixture);
        const auto second = run_adapter(fixture);
        EXPECT_EQ(first, second);
        EXPECT_FALSE(first.first_divergence.has_value());
        EXPECT_EQ(first.processed_events, phase3::kSmallWorkloadEventCount);
        EXPECT_TRUE(first.observations.empty());
        EXPECT_TRUE(first.final_observation.has_value());
    }
}

} // namespace
