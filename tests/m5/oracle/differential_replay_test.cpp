#include "differential_test_common.hpp"

#include "core_production_side.hpp"
#include "divergence.hpp"
#include "operation_observation.hpp"
#include "reference_side.hpp"
#include "replay_driver.hpp"

#include "replay_types.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace {

// Test assertions guard optional access; the dereferences below are covered by
// ASSERT/EXPECT has_value checks (repository-established pattern).
// NOLINTBEGIN(bugprone-unchecked-optional-access)

namespace oracle = bmd_projection::m5::oracle;
namespace replay = bmd_projection::m5::replay;

using oracle::AdapterErrorOutcome;
using oracle::ApplyOutcome;
using oracle::CanonicalDisposition;
using oracle::CanonicalGapEvidence;
using oracle::CanonicalGapReason;
using oracle::CanonicalLevel;
using oracle::CanonicalPolicy;
using oracle::CanonicalStatus;
using oracle::DecimalErrorOutcome;
using oracle::InstallOutcome;
using oracle::MetadataOutcome;
using oracle::OperationObservation;
using oracle::RangeOutcome;
using oracle::ResetOutcome;
using oracle::SnapshotNotProducedOutcome;

[[nodiscard]] oracle::ReplayOutcome run_core_only(std::string_view fixture_name) {
    const auto fixture = oracle::test::load_fixture(fixture_name);
    oracle::ReplayDriver driver{fixture, oracle::make_core_production_side(fixture),
                                oracle::make_reference_side(fixture, oracle::ReplayMode::CoreOnly)};
    return driver.run();
}

[[nodiscard]] const oracle::OperationObservation& observation(const oracle::ReplayOutcome& outcome,
                                                              std::size_t index) {
    return outcome.observations.at(index);
}

[[nodiscard]] CanonicalGapEvidence spot_bootstrap_gap(std::uint64_t last, std::uint64_t first,
                                                      std::uint64_t final) {
    return {last,
            first,
            final,
            std::nullopt,
            CanonicalGapReason::SpotBootstrapForwardGap,
            CanonicalPolicy::Spot};
}

TEST(CoreOnlyDifferentialReplayTest, SpotTinyAgreesAndCoversFailureGapAndRangeEvents) {
    const auto outcome = run_core_only("spot_tiny");
    ASSERT_FALSE(outcome.first_divergence.has_value());
    ASSERT_EQ(outcome.observations.size(), 8U);

    const auto& install = observation(outcome, 0);
    EXPECT_EQ(install.result.value,
              (oracle::OperationResultValue{InstallOutcome{CanonicalDisposition::Installed,
                                                           CanonicalStatus::AwaitingBridge, 100}}));
    EXPECT_EQ(install.checkpoint.status, CanonicalStatus::AwaitingBridge);
    EXPECT_EQ(install.checkpoint.last_update_id, std::optional<std::uint64_t>{100});
    EXPECT_EQ(install.checkpoint.bids, (std::vector<CanonicalLevel>{{10'000'000'000, 123'000'000},
                                                                    {9'900'000'000, 200'000'000}}));
    EXPECT_EQ(install.checkpoint.asks,
              (std::vector<CanonicalLevel>{{10'100'000'000, 100'000'000}}));
    EXPECT_FALSE(install.checkpoint.synchronized_visible);

    EXPECT_TRUE(std::holds_alternative<MetadataOutcome>(observation(outcome, 1).result.value));

    const auto& failed_update = observation(outcome, 2);
    ASSERT_TRUE(std::holds_alternative<DecimalErrorOutcome>(failed_update.result.value));
    EXPECT_EQ(std::get<DecimalErrorOutcome>(failed_update.result.value).category,
              oracle::CanonicalDecimalError::SignNotAllowed);
    EXPECT_EQ(failed_update.checkpoint.status, CanonicalStatus::AwaitingBridge);

    const auto& gap_update = observation(outcome, 3);
    const auto& apply = std::get<ApplyOutcome>(gap_update.result.value);
    EXPECT_EQ(apply.disposition, CanonicalDisposition::GapDetected);
    EXPECT_EQ(apply.status_after, CanonicalStatus::NeedsResync);
    EXPECT_EQ(apply.last_update_id_after, std::optional<std::uint64_t>{100});
    ASSERT_TRUE(apply.gap.has_value());
    EXPECT_EQ(*apply.gap, spot_bootstrap_gap(100, 101, 101));
    EXPECT_EQ(gap_update.checkpoint.status, CanonicalStatus::NeedsResync);
    EXPECT_TRUE(gap_update.checkpoint.last_gap.has_value());

    EXPECT_TRUE(
        std::holds_alternative<SnapshotNotProducedOutcome>(observation(outcome, 4).result.value));

    const auto& reset = observation(outcome, 5);
    EXPECT_TRUE(std::holds_alternative<ResetOutcome>(reset.result.value));
    EXPECT_EQ(reset.checkpoint.status, CanonicalStatus::AwaitingBaseline);
    EXPECT_FALSE(reset.checkpoint.last_update_id.has_value());
    EXPECT_TRUE(reset.checkpoint.bids.empty());
    EXPECT_TRUE(reset.checkpoint.asks.empty());

    const auto& rebaseline = observation(outcome, 6);
    EXPECT_EQ(rebaseline.result.value,
              (oracle::OperationResultValue{InstallOutcome{CanonicalDisposition::Installed,
                                                           CanonicalStatus::AwaitingBridge, 200}}));

    const auto& malformed = observation(outcome, 7);
    ASSERT_TRUE(std::holds_alternative<RangeOutcome>(malformed.result.value));
    EXPECT_TRUE(std::get<RangeOutcome>(malformed.result.value).invalid_as_intended);
    // MALFORMED_RANGE never reached apply: state is unchanged from the rebaseline.
    EXPECT_EQ(malformed.checkpoint, rebaseline.checkpoint);
}

TEST(CoreOnlyDifferentialReplayTest, UsdmTinyAgreesWithPuAwareStream) {
    const auto outcome = run_core_only("usdm_tiny");
    ASSERT_FALSE(outcome.first_divergence.has_value());
    ASSERT_EQ(outcome.observations.size(), 8U);

    const auto& bridge = observation(outcome, 2);
    const auto& apply = std::get<ApplyOutcome>(bridge.result.value);
    EXPECT_EQ(apply.disposition, CanonicalDisposition::Applied);
    EXPECT_EQ(apply.status_after, CanonicalStatus::Synchronized);
    EXPECT_EQ(apply.last_update_id_after, std::optional<std::uint64_t>{501});
    EXPECT_TRUE(bridge.checkpoint.bids.empty());
    EXPECT_EQ(bridge.checkpoint.asks, (std::vector<CanonicalLevel>{{10'100, 3'125}}));

    EXPECT_TRUE(
        std::holds_alternative<SnapshotNotProducedOutcome>(observation(outcome, 3).result.value));

    const auto& live = observation(outcome, 4);
    const auto& live_apply = std::get<ApplyOutcome>(live.result.value);
    EXPECT_EQ(live_apply.disposition, CanonicalDisposition::Applied);
    EXPECT_EQ(live.checkpoint.bids, (std::vector<CanonicalLevel>{{9'999, 1'000}}));
    EXPECT_EQ(live.checkpoint.last_update_id, std::optional<std::uint64_t>{503});

    const auto& malformed = observation(outcome, 7);
    ASSERT_TRUE(std::holds_alternative<RangeOutcome>(malformed.result.value));
    EXPECT_TRUE(std::get<RangeOutcome>(malformed.result.value).invalid_as_intended);
}

TEST(CoreOnlyDifferentialReplayTest, RecoveryTinyNegativeZeroBaselineQuantityRejected) {
    const auto outcome = run_core_only("recovery_tiny");
    ASSERT_FALSE(outcome.first_divergence.has_value());
    ASSERT_EQ(outcome.observations.size(), 7U);

    // The baseline quantity "-0" violates the M1 grammar (SignNotAllowed), so the
    // install is rejected and the projection stays AwaitingBaseline. This exercises
    // the parse-failure observation on a baseline event.
    const auto& install = observation(outcome, 0);
    ASSERT_TRUE(std::holds_alternative<DecimalErrorOutcome>(install.result.value));
    EXPECT_EQ(std::get<DecimalErrorOutcome>(install.result.value).category,
              oracle::CanonicalDecimalError::SignNotAllowed);
    EXPECT_EQ(install.checkpoint.status, CanonicalStatus::AwaitingBaseline);
    EXPECT_TRUE(install.checkpoint.bids.empty());
    EXPECT_TRUE(install.checkpoint.asks.empty());

    const auto& rejected = observation(outcome, 1);
    const auto& apply = std::get<ApplyOutcome>(rejected.result.value);
    EXPECT_EQ(apply.disposition, CanonicalDisposition::RejectedWrongState);
    EXPECT_EQ(apply.status_after, CanonicalStatus::AwaitingBaseline);

    EXPECT_TRUE(std::holds_alternative<ResetOutcome>(observation(outcome, 2).result.value));

    const auto& rebaseline = observation(outcome, 3);
    EXPECT_EQ(rebaseline.result.value,
              (oracle::OperationResultValue{InstallOutcome{CanonicalDisposition::Installed,
                                                           CanonicalStatus::AwaitingBridge, 20}}));

    const auto& update = observation(outcome, 5);
    const auto& update_apply = std::get<ApplyOutcome>(update.result.value);
    EXPECT_EQ(update_apply.disposition, CanonicalDisposition::Applied);
    EXPECT_EQ(update.checkpoint.bids, (std::vector<CanonicalLevel>{{10'000, 10'000}}));

    EXPECT_TRUE(
        std::holds_alternative<SnapshotNotProducedOutcome>(observation(outcome, 6).result.value));
}

TEST(CoreOnlyDifferentialReplayTest, AdapterTinyIgnoresMetadataInCoreOnlyMode) {
    const auto outcome = run_core_only("adapter_tiny");
    ASSERT_FALSE(outcome.first_divergence.has_value());
    ASSERT_EQ(outcome.observations.size(), 11U);

    EXPECT_TRUE(std::holds_alternative<MetadataOutcome>(observation(outcome, 0).result.value));
    EXPECT_TRUE(
        std::holds_alternative<SnapshotNotProducedOutcome>(observation(outcome, 3).result.value));
    EXPECT_TRUE(
        std::holds_alternative<SnapshotNotProducedOutcome>(observation(outcome, 6).result.value));
    EXPECT_TRUE(
        std::holds_alternative<SnapshotNotProducedOutcome>(observation(outcome, 8).result.value));

    const auto& gap = observation(outcome, 7);
    const auto& apply = std::get<ApplyOutcome>(gap.result.value);
    EXPECT_EQ(apply.disposition, CanonicalDisposition::GapDetected);
    ASSERT_TRUE(apply.gap.has_value());
    EXPECT_EQ(apply.gap->reason, CanonicalGapReason::SpotLiveForwardGap);
    EXPECT_EQ(apply.gap->last_accepted_final, 55U);
    EXPECT_EQ(apply.gap->first_update_id, 60U);

    const auto& rebaseline = observation(outcome, 10);
    EXPECT_EQ(rebaseline.result.value,
              (oracle::OperationResultValue{InstallOutcome{CanonicalDisposition::Installed,
                                                           CanonicalStatus::AwaitingBridge, 70}}));
}

TEST(CoreOnlyDifferentialReplayTest, RepeatedReplayIsDeterministic) {
    for (const std::string_view fixture_name :
         {"spot_tiny", "usdm_tiny", "recovery_tiny", "adapter_tiny"}) {
        SCOPED_TRACE(fixture_name);
        const auto first = run_core_only(fixture_name);
        const auto second = run_core_only(fixture_name);
        EXPECT_EQ(first, second);
        EXPECT_EQ(first.observations.size(), second.observations.size());
    }
}

} // namespace

// NOLINTEND(bugprone-unchecked-optional-access)
