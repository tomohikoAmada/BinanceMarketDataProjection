// Adapter-conditional Phase-6 benchmark support tests: AdapterWireReplay
// preconstruction and final-state validation (OD-M5-P6-015/024).
// Compiled only when the ProtoAdapter is enabled.

#include "adapter_benchmark_test_support.hpp"

#include <gtest/gtest.h>

namespace {

namespace test_support = bmd_projection::m5::benchmark::test_support;

TEST(Phase6AdapterReplay, PreconstructionCoversFullWorkloadAndChecksumIsStable) {
    const auto observation = test_support::observe_spot_replay_repeat();
    EXPECT_TRUE(observation.full_preconstruction);
    EXPECT_EQ(observation.event_count, 2'048U);
    EXPECT_EQ(observation.first_checksum, observation.second_checksum);
    EXPECT_TRUE(observation.first_synchronized);
    EXPECT_TRUE(observation.second_synchronized);
}

TEST(Phase6AdapterReplay, UsdMWorkloadProducesStableDistinctChecksum) {
    const auto observation = test_support::observe_spot_usdm_replays();
    EXPECT_NE(observation.spot_checksum, observation.usdm_checksum);
    EXPECT_TRUE(observation.usdm_synchronized);
}

} // namespace
