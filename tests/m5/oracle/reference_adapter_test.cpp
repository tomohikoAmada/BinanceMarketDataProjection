#include "reference_adapter.hpp"

#include "reference_decimal.hpp"
#include "reference_projection.hpp"

#include "replay_types.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace {

namespace ref = bmd_projection::m5::reference;
namespace reference = bmd_projection_reference;
namespace replay = bmd_projection::m5::replay;

[[nodiscard]] replay::LevelInput level(replay::Side side, std::string price, std::string quantity) {
    return {side, std::move(price), std::move(quantity)};
}

[[nodiscard]] replay::InstallBaselineOp baseline(std::uint64_t last_update_id,
                                                 std::vector<replay::LevelInput> bids,
                                                 std::vector<replay::LevelInput> asks) {
    return {replay::SourceLocation{}, last_update_id, std::move(bids), std::move(asks)};
}

[[nodiscard]] replay::DepthUpdateOp update(std::uint64_t first, std::uint64_t final,
                                           std::optional<std::uint64_t> previous_final,
                                           std::vector<replay::LevelInput> levels) {
    return {replay::SourceLocation{}, first, final, previous_final, std::move(levels)};
}

[[nodiscard]] replay::SnapshotRequestOp
snapshot(std::optional<std::uint32_t> depth_limit, std::vector<replay::HostQualityFact> facts,
         std::optional<std::pair<std::uint64_t, replay::GapRecoveryState>> current_gap) {
    return {replay::SourceLocation{},
            depth_limit,
            std::move(facts),
            "snapshot",
            "producer",
            "1.0",
            replay::SnapshotOrigin::GatewayLive,
            1000,
            std::nullopt,
            std::move(current_gap)};
}

[[nodiscard]] ref::ReferenceAdapter adapter() {
    return ref::ReferenceAdapter{replay::SequencePolicy::Spot, "BTCUSDT",
                                 replay::NumericSpec{4, 4}};
}

[[nodiscard]] std::vector<replay::HostQualityFact>
facts(std::initializer_list<replay::HostQualityFact> values) {
    return {values};
}

[[nodiscard]] ref::ReferenceInputPrediction
input_of(const ref::ReferenceBaselinePrediction& prediction) {
    return std::get<ref::ReferenceInputPrediction>(prediction);
}

[[nodiscard]] ref::ReferenceAdapterError
error_of(const ref::ReferenceBaselinePrediction& prediction) {
    return std::get<ref::ReferenceAdapterError>(prediction);
}

[[nodiscard]] ref::ReferenceSnapshotPrediction
snapshot_of(const ref::ReferenceSnapshotResult& result) {
    return std::get<ref::ReferenceSnapshotPrediction>(result);
}

TEST(ReferenceAdapterTest, MapsObservedQualityCanonically) {
    EXPECT_EQ(ref::ReferenceAdapter::map_observed_quality({}),
              (std::vector<ref::ReferenceQualityFlag>{}));
    EXPECT_EQ(ref::ReferenceAdapter::map_observed_quality(
                  facts({replay::HostQualityFact::Duplicate, replay::HostQualityFact::Overlap,
                         replay::HostQualityFact::Duplicate})),
              (std::vector<ref::ReferenceQualityFlag>{ref::ReferenceQualityFlag::Duplicate,
                                                      ref::ReferenceQualityFlag::Overlap}));
    EXPECT_EQ(ref::ReferenceAdapter::map_observed_quality(
                  facts({replay::HostQualityFact::Overlap, replay::HostQualityFact::Duplicate})),
              (std::vector<ref::ReferenceQualityFlag>{ref::ReferenceQualityFlag::Duplicate,
                                                      ref::ReferenceQualityFlag::Overlap}));
    EXPECT_EQ(
        ref::ReferenceAdapter::map_observed_quality(
            facts({replay::HostQualityFact::IdentityConflict, replay::HostQualityFact::Duplicate})),
        (std::vector<ref::ReferenceQualityFlag>{ref::ReferenceQualityFlag::Duplicate,
                                                ref::ReferenceQualityFlag::IdentityConflict}));
}

TEST(ReferenceAdapterTest, PredictsBaselineInputAndOrdering) {
    const auto ok = adapter().predict_baseline_input(
        baseline(50, {level(replay::Side::Bid, "100.0000", "1.0000")},
                 {level(replay::Side::Ask, "100.5000", "2.0000")}),
        facts({replay::HostQualityFact::Duplicate}));
    ASSERT_TRUE(std::holds_alternative<ref::ReferenceInputPrediction>(ok));
    EXPECT_EQ(input_of(ok).observed_quality,
              (std::vector<ref::ReferenceQualityFlag>{ref::ReferenceQualityFlag::Duplicate}));

    const auto ascending_bids =
        adapter().predict_baseline_input(baseline(50,
                                                  {level(replay::Side::Bid, "100.0000", "1.0000"),
                                                   level(replay::Side::Bid, "100.1000", "1.0000")},
                                                  {}),
                                         {});
    ASSERT_TRUE(std::holds_alternative<ref::ReferenceAdapterError>(ascending_bids));
    EXPECT_EQ(error_of(ascending_bids).code, ref::ReferenceAdapterErrorCode::InvalidOrdering);
    EXPECT_EQ(error_of(ascending_bids).field, ref::ReferenceAdapterField::BidPrice);

    const auto descending_asks =
        adapter().predict_baseline_input(baseline(50, {},
                                                  {level(replay::Side::Ask, "100.5000", "2.0000"),
                                                   level(replay::Side::Ask, "100.4000", "2.0000")}),
                                         {});
    ASSERT_TRUE(std::holds_alternative<ref::ReferenceAdapterError>(descending_asks));
    EXPECT_EQ(error_of(descending_asks).field, ref::ReferenceAdapterField::AskPrice);
}

TEST(ReferenceAdapterTest, PredictsDepthUpdateRangeAndDecimalFailures) {
    const auto reversed =
        adapter().predict_depth_update_input(update(11, 10, std::nullopt, {}), {});
    ASSERT_TRUE(std::holds_alternative<ref::ReferenceAdapterError>(reversed));
    EXPECT_EQ(error_of(reversed).code, ref::ReferenceAdapterErrorCode::InvalidUpdateRange);
    EXPECT_EQ(error_of(reversed).field, ref::ReferenceAdapterField::FinalUpdateId);

    const auto zero_price = adapter().predict_depth_update_input(
        update(10, 11, std::nullopt, {level(replay::Side::Bid, "0", "1.0000")}), {});
    ASSERT_TRUE(std::holds_alternative<ref::ReferenceAdapterError>(zero_price));
    EXPECT_EQ(error_of(zero_price).code, ref::ReferenceAdapterErrorCode::NonPositivePrice);
    EXPECT_EQ(error_of(zero_price).field, ref::ReferenceAdapterField::BidPrice);
    EXPECT_EQ(error_of(zero_price).decimal_error,
              std::optional<ref::ReferenceDecimalErrorCode>{
                  ref::ReferenceDecimalErrorCode::ZeroNotAllowed});

    const auto negative_price = adapter().predict_depth_update_input(
        update(10, 11, std::nullopt, {level(replay::Side::Bid, "-1", "1.0000")}), {});
    ASSERT_TRUE(std::holds_alternative<ref::ReferenceAdapterError>(negative_price));
    EXPECT_EQ(error_of(negative_price).code, ref::ReferenceAdapterErrorCode::NonPositivePrice);

    const auto negative_quantity = adapter().predict_depth_update_input(
        update(10, 11, std::nullopt, {level(replay::Side::Bid, "1.0000", "-1.0000")}), {});
    ASSERT_TRUE(std::holds_alternative<ref::ReferenceAdapterError>(negative_quantity));
    EXPECT_EQ(error_of(negative_quantity).code, ref::ReferenceAdapterErrorCode::NegativeQuantity);
    EXPECT_EQ(error_of(negative_quantity).field, ref::ReferenceAdapterField::BidQuantity);

    const auto sign_not_allowed = adapter().predict_depth_update_input(
        update(10, 11, std::nullopt, {level(replay::Side::Bid, "+1", "1.0000")}), {});
    ASSERT_TRUE(std::holds_alternative<ref::ReferenceAdapterError>(sign_not_allowed));
    EXPECT_EQ(error_of(sign_not_allowed).code, ref::ReferenceAdapterErrorCode::InvalidDecimal);
    EXPECT_EQ(error_of(sign_not_allowed).decimal_error,
              std::optional<ref::ReferenceDecimalErrorCode>{
                  ref::ReferenceDecimalErrorCode::SignNotAllowed});

    const auto overflow = adapter().predict_depth_update_input(
        update(10, 11, std::nullopt, {level(replay::Side::Bid, "9223372036854775808", "1.0000")}),
        {});
    ASSERT_TRUE(std::holds_alternative<ref::ReferenceAdapterError>(overflow));
    EXPECT_EQ(error_of(overflow).code, ref::ReferenceAdapterErrorCode::NumericOverflow);
    EXPECT_EQ(error_of(overflow).decimal_error, std::optional<ref::ReferenceDecimalErrorCode>{
                                                    ref::ReferenceDecimalErrorCode::Overflow});
}

TEST(ReferenceAdapterTest, SnapshotEligibilityMatrix) {
    auto projection = reference::ReferenceProjection{reference::Policy::Spot};
    const auto fresh =
        adapter().predict_snapshot(projection, snapshot(std::nullopt, {}, std::nullopt));
    ASSERT_TRUE(std::holds_alternative<ref::ReferenceAdapterError>(fresh));
    EXPECT_EQ(std::get<ref::ReferenceAdapterError>(fresh).code,
              ref::ReferenceAdapterErrorCode::MissingLastUpdateId);
    EXPECT_EQ(std::get<ref::ReferenceAdapterError>(fresh).field,
              ref::ReferenceAdapterField::LastUpdateId);

    static_cast<void>(
        projection.install(50, {{true, 1'000'000, 10'000}, {false, 1'005'000, 20'000}}));
    const auto awaiting_bridge =
        adapter().predict_snapshot(projection, snapshot(std::nullopt, {}, std::nullopt));
    ASSERT_TRUE(std::holds_alternative<ref::ReferenceSnapshotPrediction>(awaiting_bridge));
    const auto& bridge_prediction = snapshot_of(awaiting_bridge);
    EXPECT_FALSE(bridge_prediction.synchronized);
    EXPECT_EQ(bridge_prediction.last_update_id, std::optional<std::uint64_t>{50});
    EXPECT_EQ(
        bridge_prediction.quality_flags,
        (std::vector<ref::ReferenceQualityFlag>{ref::ReferenceQualityFlag::SnapshotBridgePending}));

    static_cast<void>(projection.apply(49, 51, std::nullopt, {{true, 1'000'000, 15'000}}));
    const auto synchronized =
        adapter().predict_snapshot(projection, snapshot(std::nullopt, {}, std::nullopt));
    ASSERT_TRUE(std::holds_alternative<ref::ReferenceSnapshotPrediction>(synchronized));
    EXPECT_TRUE(snapshot_of(synchronized).synchronized);
    EXPECT_EQ(snapshot_of(synchronized).last_update_id, std::optional<std::uint64_t>{51});

    static_cast<void>(projection.apply(60, 61, std::nullopt, {{true, 980'000, 40'000}}));
    const auto needs_resync_missing_context =
        adapter().predict_snapshot(projection, snapshot(std::nullopt, {}, std::nullopt));
    ASSERT_TRUE(std::holds_alternative<ref::ReferenceAdapterError>(needs_resync_missing_context));
    EXPECT_EQ(std::get<ref::ReferenceAdapterError>(needs_resync_missing_context).code,
              ref::ReferenceAdapterErrorCode::MissingRequiredField);
    EXPECT_EQ(std::get<ref::ReferenceAdapterError>(needs_resync_missing_context).field,
              ref::ReferenceAdapterField::CurrentGap);

    const auto invalid_recovery = adapter().predict_snapshot(
        projection, snapshot(std::nullopt, {},
                             std::optional<std::pair<std::uint64_t, replay::GapRecoveryState>>{
                                 {5500, replay::GapRecoveryState::Synchronized}}));
    ASSERT_TRUE(std::holds_alternative<ref::ReferenceAdapterError>(invalid_recovery));
    EXPECT_EQ(std::get<ref::ReferenceAdapterError>(invalid_recovery).code,
              ref::ReferenceAdapterErrorCode::InvalidGapContext);
    EXPECT_EQ(std::get<ref::ReferenceAdapterError>(invalid_recovery).field,
              ref::ReferenceAdapterField::GapRecoveryState);

    const auto valid_recovery = adapter().predict_snapshot(
        projection, snapshot(std::nullopt, {replay::HostQualityFact::OrderBookResync},
                             std::optional<std::pair<std::uint64_t, replay::GapRecoveryState>>{
                                 {5500, replay::GapRecoveryState::ResyncRequired}}));
    ASSERT_TRUE(std::holds_alternative<ref::ReferenceSnapshotPrediction>(valid_recovery));
    const auto& recovered = snapshot_of(valid_recovery);
    EXPECT_FALSE(recovered.synchronized);
    EXPECT_EQ(recovered.gap_descriptor,
              (std::optional<ref::ReferenceGapDescriptor>{ref::ReferenceGapDescriptor{
                  5500, 51, 60, ref::ReferenceReasonCode::SequenceGapDetected,
                  ref::ReferenceResyncState::ResyncRequired}}));
    EXPECT_EQ(recovered.quality_flags,
              (std::vector<ref::ReferenceQualityFlag>{ref::ReferenceQualityFlag::SequenceGap,
                                                      ref::ReferenceQualityFlag::OrderBookResync}));
}

TEST(ReferenceAdapterTest, EnforcesDepthLimitAndHostFactStateRules) {
    auto projection = reference::ReferenceProjection{reference::Policy::Spot};
    static_cast<void>(
        projection.install(50, {{true, 1'000'000, 10'000}, {false, 1'005'000, 20'000}}));
    static_cast<void>(projection.apply(49, 51, std::nullopt, {{true, 1'000'000, 15'000}}));

    const auto zero_limit = adapter().predict_snapshot(projection, snapshot(0, {}, std::nullopt));
    ASSERT_TRUE(std::holds_alternative<ref::ReferenceAdapterError>(zero_limit));
    EXPECT_EQ(std::get<ref::ReferenceAdapterError>(zero_limit).code,
              ref::ReferenceAdapterErrorCode::InvalidDepthLimit);
    EXPECT_EQ(std::get<ref::ReferenceAdapterError>(zero_limit).field,
              ref::ReferenceAdapterField::DepthLimit);

    const auto invalid_host_fact = adapter().predict_snapshot(
        projection,
        snapshot(std::nullopt, {replay::HostQualityFact::OrderBookResync}, std::nullopt));
    ASSERT_TRUE(std::holds_alternative<ref::ReferenceAdapterError>(invalid_host_fact));
    EXPECT_EQ(std::get<ref::ReferenceAdapterError>(invalid_host_fact).code,
              ref::ReferenceAdapterErrorCode::InvalidHostQualityCombination);
    EXPECT_EQ(std::get<ref::ReferenceAdapterError>(invalid_host_fact).field,
              ref::ReferenceAdapterField::HostQualityFact);

    // RecoveredTail is valid only in Synchronized.
    const auto valid_tail = adapter().predict_snapshot(
        projection, snapshot(std::nullopt, {replay::HostQualityFact::RecoveredTail}, std::nullopt));
    ASSERT_TRUE(std::holds_alternative<ref::ReferenceSnapshotPrediction>(valid_tail));
    EXPECT_EQ(snapshot_of(valid_tail).quality_flags,
              (std::vector<ref::ReferenceQualityFlag>{ref::ReferenceQualityFlag::RecoveredTail}));

    // In AwaitingBridge the same fact is rejected.
    projection.reset();
    static_cast<void>(
        projection.install(60, {{true, 1'000'000, 10'000}, {false, 1'005'000, 20'000}}));
    const auto wrong_state_tail = adapter().predict_snapshot(
        projection, snapshot(std::nullopt, {replay::HostQualityFact::RecoveredTail}, std::nullopt));
    ASSERT_TRUE(std::holds_alternative<ref::ReferenceAdapterError>(wrong_state_tail));
    EXPECT_EQ(std::get<ref::ReferenceAdapterError>(wrong_state_tail).code,
              ref::ReferenceAdapterErrorCode::InvalidHostQualityCombination);
}

TEST(ReferenceAdapterTest, DepthLimitTruncatesAndFormatsCanonically) {
    auto projection = reference::ReferenceProjection{reference::Policy::Spot};
    static_cast<void>(projection.install(
        50, {{true, 1'000'000, 10'000}, {true, 990'000, 30'000}, {false, 1'005'000, 20'000}}));
    static_cast<void>(projection.apply(49, 51, std::nullopt, {{true, 1'000'000, 15'000}}));
    const auto limited = adapter().predict_snapshot(projection, snapshot(1, {}, std::nullopt));
    ASSERT_TRUE(std::holds_alternative<ref::ReferenceSnapshotPrediction>(limited));
    const auto& prediction = snapshot_of(limited);
    ASSERT_EQ(prediction.bids.size(), 1U);
    EXPECT_EQ(prediction.bids[0].price, "100.0000");
    EXPECT_EQ(prediction.bids[0].quantity, "1.5000");
    ASSERT_EQ(prediction.asks.size(), 1U);
    EXPECT_EQ(prediction.asks[0].price, "100.5000");
    EXPECT_EQ(prediction.asks[0].quantity, "2.0000");
    EXPECT_EQ(prediction.depth_limit, std::optional<std::uint32_t>{1});
}

TEST(ReferenceAdapterTest, CrossedBookDerivesCoreQualityFlag) {
    auto projection = reference::ReferenceProjection{reference::Policy::Spot};
    static_cast<void>(
        projection.install(50, {{true, 1'000'000, 10'000}, {false, 990'000, 20'000}}));
    static_cast<void>(projection.apply(49, 51, std::nullopt, {}));
    const auto crossed =
        adapter().predict_snapshot(projection, snapshot(std::nullopt, {}, std::nullopt));
    ASSERT_TRUE(std::holds_alternative<ref::ReferenceSnapshotPrediction>(crossed));
    EXPECT_EQ(snapshot_of(crossed).quality_flags,
              (std::vector<ref::ReferenceQualityFlag>{ref::ReferenceQualityFlag::CrossedBook}));
}

} // namespace
