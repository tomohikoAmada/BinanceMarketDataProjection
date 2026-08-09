#include "small_workload.hpp"

#include "core_production_side.hpp"
#include "divergence.hpp"
#include "operation_observation.hpp"
#include "reference_side.hpp"
#include "replay_driver.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <string>

namespace {

namespace oracle = bmd_projection::m5::oracle;
namespace phase3 = bmd_projection::m5::phase3;
namespace replay = bmd_projection::m5::replay;

[[nodiscard]] oracle::ReplayOutcome
run_core(const replay::ReplayFixture& fixture,
         oracle::ObservationRetention retention = oracle::ObservationRetention::RetainAll) {
    oracle::ReplayDriver driver{fixture, oracle::make_core_production_side(fixture),
                                oracle::make_reference_side(fixture, oracle::ReplayMode::CoreOnly),
                                retention};
    return driver.run();
}

TEST(Phase3SmallReplayTest, SpotAndUsdMCompareEveryEventAndRepeatExactly) {
    for (const auto& fixture :
         {phase3::make_spot_small_workload(), phase3::make_usdm_small_workload()}) {
        SCOPED_TRACE(fixture.identity.fixture_id);
        ASSERT_EQ(fixture.replay.operations.size(), phase3::kSmallWorkloadEventCount);
        ASSERT_EQ(fixture.manifest.event_count, phase3::kSmallWorkloadEventCount);

        const auto first = run_core(fixture);
        const auto second = run_core(fixture);
        EXPECT_EQ(first, second);
        EXPECT_FALSE(first.first_divergence.has_value());
        EXPECT_EQ(first.processed_events, phase3::kSmallWorkloadEventCount);
        EXPECT_EQ(first.observations.size(), phase3::kSmallWorkloadEventCount);
        ASSERT_TRUE(first.final_observation.has_value());
        EXPECT_EQ(first.final_observation, first.observations.back());

        const auto bounded = run_core(fixture, oracle::ObservationRetention::RetainNone);
        EXPECT_FALSE(bounded.first_divergence.has_value());
        EXPECT_EQ(bounded.processed_events, phase3::kSmallWorkloadEventCount);
        EXPECT_TRUE(bounded.observations.empty());
        EXPECT_EQ(bounded.final_observation, first.final_observation);
    }
}

TEST(Phase3SmallReplayTest, GeneratedCanonicalReplayIdentityIsStable) {
    const auto spot_first = phase3::make_spot_small_workload();
    const auto spot_second = phase3::make_spot_small_workload();
    const auto usdm_first = phase3::make_usdm_small_workload();
    const auto usdm_second = phase3::make_usdm_small_workload();

    EXPECT_EQ(spot_first.identity, spot_second.identity);
    EXPECT_EQ(spot_first.canonical_log_sha256, spot_second.canonical_log_sha256);
    EXPECT_EQ(usdm_first.identity, usdm_second.identity);
    EXPECT_EQ(usdm_first.canonical_log_sha256, usdm_second.canonical_log_sha256);
    EXPECT_NE(spot_first.canonical_log_sha256, usdm_first.canonical_log_sha256);
    EXPECT_EQ(spot_first.canonical_log_sha256,
              "14f44a6794af5c87fb3b359e16c1901a284e6da7946f02d6be3dc8cacb249227");
    EXPECT_EQ(usdm_first.canonical_log_sha256,
              "f3e60732c8e4452f86231548cd0c5918b914487063d3f4f22d6588f088141e03");
}

TEST(Phase3SmallReplayTest, SeededLargeWorkloadFaultReportsExactFirstDivergence) {
    const auto fixture = phase3::make_spot_small_workload();
    constexpr std::size_t kFirstFault = 1'500;
    constexpr std::size_t kSecondaryFault = 1'700;

    const auto run_fault = [&]() {
        auto reference = oracle::make_reference_side(fixture, oracle::ReplayMode::CoreOnly);
        auto calls = std::make_shared<std::size_t>(0U);
        auto mutating = std::make_unique<oracle::MutatingSide>(
            std::move(reference), [calls](oracle::OperationObservation& observation) {
                if (*calls == kFirstFault || *calls == kSecondaryFault) {
                    observation.checkpoint.bids.push_back({1, 1});
                }
                ++*calls;
            });
        oracle::ReplayDriver driver{fixture, oracle::make_core_production_side(fixture),
                                    std::move(mutating), oracle::ObservationRetention::RetainNone};
        return driver.run();
    };

    const auto first = run_fault();
    const auto second = run_fault();
    ASSERT_TRUE(first.first_divergence.has_value());
    ASSERT_TRUE(second.first_divergence.has_value());
    EXPECT_EQ(first.first_divergence->event_index, kFirstFault);
    EXPECT_EQ(first.first_divergence->layer, oracle::Layer::R2);
    EXPECT_EQ(first.first_divergence->category, oracle::DivergenceCategory::Checkpoint);
    EXPECT_EQ(first.processed_events, kFirstFault + 1U);
    EXPECT_TRUE(first.observations.empty());
    EXPECT_EQ(oracle::render_divergence(*first.first_divergence),
              oracle::render_divergence(*second.first_divergence));

    const auto diagnostic = oracle::render_divergence(*first.first_divergence);
    EXPECT_NE(diagnostic.find("fixture_id=m5-small-spot-v1"), std::string::npos);
    EXPECT_NE(diagnostic.find("event_index=1500"), std::string::npos);
    EXPECT_NE(diagnostic.find("layer=R2"), std::string::npos);
    EXPECT_NE(diagnostic.find("category=CHECKPOINT"), std::string::npos);
    EXPECT_NE(diagnostic.find("production_checkpoint=CHECKPOINT"), std::string::npos);
    EXPECT_NE(diagnostic.find("reference_checkpoint=CHECKPOINT"), std::string::npos);
    EXPECT_NE(diagnostic.find("normalized_event="), std::string::npos);
    EXPECT_FALSE(first.first_divergence->source_line.empty());
    EXPECT_EQ(diagnostic.back(), '\n');
}

} // namespace
