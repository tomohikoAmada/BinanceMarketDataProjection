#include "divergence.hpp"
#include "operation_observation.hpp"

#include "replay_types.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>

namespace {

// Test assertions guard optional access; the dereferences below are covered by
// ASSERT/EXPECT has_value checks (repository-established pattern).
// NOLINTBEGIN(bugprone-unchecked-optional-access)

namespace oracle = bmd_projection::m5::oracle;
namespace replay = bmd_projection::m5::replay;

using oracle::AdapterErrorOutcome;
using oracle::ApplyOutcome;
using oracle::CanonicalAdapterCode;
using oracle::CanonicalAdapterField;
using oracle::CanonicalDecimalError;
using oracle::CanonicalDisposition;
using oracle::CanonicalGapEvidence;
using oracle::CanonicalGapReason;
using oracle::CanonicalLevel;
using oracle::CanonicalPolicy;
using oracle::CanonicalQualityFlag;
using oracle::CanonicalStatus;
using oracle::DecimalErrorOutcome;
using oracle::InstallOutcome;
using oracle::MetadataOutcome;
using oracle::OperationObservation;
using oracle::OperationResult;
using oracle::RangeOutcome;
using oracle::ResetOutcome;
using oracle::SemanticCheckpoint;
using oracle::SnapshotLevel;
using oracle::SnapshotNotProducedOutcome;
using oracle::SnapshotOutcome;

[[nodiscard]] SemanticCheckpoint checkpoint() {
    SemanticCheckpoint value;
    value.status = CanonicalStatus::AwaitingBridge;
    value.last_update_id = 100;
    value.synchronized_visible = false;
    value.price_scale = 8;
    value.quantity_scale = 8;
    return value;
}

[[nodiscard]] OperationObservation observation(OperationResult result) {
    return OperationObservation{0, replay::EventKind::InstallBaseline, result, checkpoint(),
                                std::nullopt};
}

[[nodiscard]] OperationObservation install_observation() {
    return observation(OperationResult{
        InstallOutcome{CanonicalDisposition::Installed, CanonicalStatus::AwaitingBridge, 100}});
}

[[nodiscard]] std::optional<oracle::Divergence> compare(const OperationObservation& production,
                                                        const OperationObservation& reference) {
    return oracle::compare_observations(production, reference, "fixture_id=test",
                                        replay::SourceLocation{});
}

TEST(OperationObservationTest, SemanticEqualityCoversResultCheckpointAndSnapshot) {
    EXPECT_EQ(install_observation(), install_observation());

    auto different_result = install_observation();
    different_result.result.value = ResetOutcome{};
    EXPECT_NE(install_observation(), different_result);

    auto different_checkpoint = install_observation();
    different_checkpoint.checkpoint.status = CanonicalStatus::Synchronized;
    EXPECT_NE(install_observation(), different_checkpoint);

    auto with_snapshot = install_observation();
    SnapshotOutcome snapshot;
    snapshot.policy = CanonicalPolicy::Spot;
    snapshot.symbol = "BTCUSDT";
    snapshot.synchronized = true;
    snapshot.last_update_id = 100;
    with_snapshot.result.value = snapshot;
    with_snapshot.snapshot = snapshot;
    EXPECT_NE(install_observation(), with_snapshot);
}

TEST(OperationObservationTest, AgreeingObservationsProduceNoDivergence) {
    EXPECT_FALSE(compare(install_observation(), install_observation()).has_value());
}

TEST(OperationObservationTest, ResultKindMismatchIsAttributedToR1ForDecimal) {
    auto production = install_observation();
    auto reference = install_observation();
    reference.result.value = DecimalErrorOutcome{CanonicalDecimalError::Overflow};
    const auto divergence = compare(production, reference);
    ASSERT_TRUE(divergence.has_value());
    EXPECT_EQ(divergence->category, oracle::DivergenceCategory::OperationResult);
    EXPECT_EQ(divergence->layer, oracle::Layer::R1);
    EXPECT_EQ(divergence->event_index, 0U);
}

TEST(OperationObservationTest, ResultKindMismatchIsAttributedToR4ForAdapter) {
    auto production = install_observation();
    auto reference = install_observation();
    reference.result.value =
        AdapterErrorOutcome{CanonicalAdapterCode::InvalidDecimal, CanonicalAdapterField::BidPrice,
                            CanonicalDecimalError::SignNotAllowed};
    const auto divergence = compare(production, reference);
    ASSERT_TRUE(divergence.has_value());
    EXPECT_EQ(divergence->category, oracle::DivergenceCategory::OperationResult);
    EXPECT_EQ(divergence->layer, oracle::Layer::R4);
}

TEST(OperationObservationTest, ApplyResultFieldMismatchIsAttributedToR3) {
    auto production = observation(OperationResult{ApplyOutcome{
        CanonicalDisposition::Applied, CanonicalStatus::Synchronized, 101, std::nullopt}});
    auto reference = production;
    std::get<ApplyOutcome>(reference.result.value).disposition =
        CanonicalDisposition::IgnoredDuplicate;
    const auto divergence = compare(production, reference);
    ASSERT_TRUE(divergence.has_value());
    EXPECT_EQ(divergence->category, oracle::DivergenceCategory::OperationResult);
    EXPECT_EQ(divergence->layer, oracle::Layer::R3);
    EXPECT_NE(divergence->detail.find("disposition"), std::string::npos);
}

TEST(OperationObservationTest, GapEvidenceFieldMismatchIsAttributedToR3) {
    const CanonicalGapEvidence gap{100,
                                   101,
                                   101,
                                   std::nullopt,
                                   CanonicalGapReason::SpotBootstrapForwardGap,
                                   CanonicalPolicy::Spot};
    auto production = observation(OperationResult{
        ApplyOutcome{CanonicalDisposition::GapDetected, CanonicalStatus::NeedsResync, 100, gap}});
    auto reference = production;
    std::get<ApplyOutcome>(reference.result.value).gap->reason =
        CanonicalGapReason::SpotLiveForwardGap;
    const auto divergence = compare(production, reference);
    ASSERT_TRUE(divergence.has_value());
    EXPECT_EQ(divergence->category, oracle::DivergenceCategory::OperationResult);
    EXPECT_EQ(divergence->layer, oracle::Layer::R3);
    EXPECT_NE(divergence->detail.find("gap.reason"), std::string::npos);
}

TEST(OperationObservationTest, AdapterErrorCodeMismatchIsAttributedToR4) {
    auto production = observation(OperationResult{
        AdapterErrorOutcome{CanonicalAdapterCode::InvalidDecimal, CanonicalAdapterField::BidPrice,
                            CanonicalDecimalError::Overflow}});
    auto reference = production;
    std::get<AdapterErrorOutcome>(reference.result.value).code =
        CanonicalAdapterCode::NumericOverflow;
    const auto divergence = compare(production, reference);
    ASSERT_TRUE(divergence.has_value());
    EXPECT_EQ(divergence->category, oracle::DivergenceCategory::OperationResult);
    EXPECT_EQ(divergence->layer, oracle::Layer::R4);
}

TEST(OperationObservationTest, CheckpointStatusMismatchIsAttributedToR3) {
    auto reference = install_observation();
    reference.checkpoint.status = CanonicalStatus::Synchronized;
    const auto divergence = compare(install_observation(), reference);
    ASSERT_TRUE(divergence.has_value());
    EXPECT_EQ(divergence->category, oracle::DivergenceCategory::Checkpoint);
    EXPECT_EQ(divergence->layer, oracle::Layer::R3);
}

TEST(OperationObservationTest, CheckpointLevelMismatchIsAttributedToR2) {
    auto reference = install_observation();
    reference.checkpoint.bids.push_back({1'000'000, 12'300});
    const auto divergence = compare(install_observation(), reference);
    ASSERT_TRUE(divergence.has_value());
    EXPECT_EQ(divergence->category, oracle::DivergenceCategory::Checkpoint);
    EXPECT_EQ(divergence->layer, oracle::Layer::R2);
    EXPECT_NE(divergence->detail.find("bids"), std::string::npos);
}

TEST(OperationObservationTest, SnapshotMismatchIsAttributedToR4) {
    SnapshotOutcome production_snapshot;
    production_snapshot.policy = CanonicalPolicy::Spot;
    production_snapshot.symbol = "BTCUSDT";
    production_snapshot.synchronized = true;
    production_snapshot.last_update_id = 100;
    production_snapshot.quality_flags = {CanonicalQualityFlag::Duplicate};
    auto production = observation(OperationResult{production_snapshot});
    production.snapshot = production_snapshot;

    auto reference = production;
    reference.snapshot->quality_flags = {};
    const auto divergence = compare(production, reference);
    ASSERT_TRUE(divergence.has_value());
    EXPECT_EQ(divergence->category, oracle::DivergenceCategory::SnapshotObservation);
    EXPECT_EQ(divergence->layer, oracle::Layer::R4);
}

TEST(OperationObservationTest, EventKindMismatchIsCompositionDivergence) {
    auto reference = install_observation();
    reference.event_kind = replay::EventKind::DepthUpdate;
    const auto divergence = compare(install_observation(), reference);
    ASSERT_TRUE(divergence.has_value());
    EXPECT_EQ(divergence->category, oracle::DivergenceCategory::Composition);
    EXPECT_EQ(divergence->layer, oracle::Layer::D);
}

TEST(OperationObservationTest, CanonicalTextIsDeterministicAndInformative) {
    auto production = install_observation();
    production.checkpoint.bids.push_back({1'000'000, 12'300});
    const auto text = oracle::to_canonical_text(production.checkpoint);
    EXPECT_NE(text.find("AWAITING_BRIDGE"), std::string::npos);
    EXPECT_NE(text.find("1000000,12300"), std::string::npos);

    auto result_text = oracle::to_canonical_text(production.result);
    EXPECT_NE(result_text.find("INSTALL_OUTCOME"), std::string::npos);
    EXPECT_NE(result_text.find("INSTALLED"), std::string::npos);
}

TEST(OperationObservationTest, RangeAndMetadataResultKindsAreDistinct) {
    EXPECT_NE(observation(OperationResult{RangeOutcome{true}}),
              observation(OperationResult{RangeOutcome{false}}));
    EXPECT_NE(observation(OperationResult{MetadataOutcome{}}),
              observation(OperationResult{ResetOutcome{}}));
    EXPECT_NE(observation(OperationResult{SnapshotNotProducedOutcome{}}),
              observation(OperationResult{MetadataOutcome{}}));
}

TEST(OperationObservationTest, SnapshotLevelsUseCanonicalFixedFormat) {
    SnapshotLevel level{"100.0000", "1.5000"};
    EXPECT_EQ(level, (SnapshotLevel{"100.0000", "1.5000"}));
    EXPECT_NE(level, (SnapshotLevel{"100.0", "1.5000"}));
}

} // namespace

// NOLINTEND(bugprone-unchecked-optional-access)
