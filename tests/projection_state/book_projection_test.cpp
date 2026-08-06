#include "test_helpers.hpp"

#include <binance_market_data/projection/v1/projection_state/book_projection.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

// NOLINTBEGIN(bugprone-unchecked-optional-access)
namespace bmd = binance_market_data::projection::v1;
namespace helper = bmd_projection_test;

namespace {

[[nodiscard]] bmd::BookProjection make_spot_at_bridge(std::uint64_t baseline_id = 500) {
    bmd::BookProjection projection{helper::spec(), bmd::SequencePolicyKind::Spot};
    EXPECT_EQ(helper::install(projection, baseline_id).disposition,
              bmd::InstallDisposition::Installed);
    return projection;
}

[[nodiscard]] bmd::BookProjection make_spot_synchronized(std::uint64_t baseline_id = 500,
                                                         std::uint64_t final_id = 501) {
    auto projection = make_spot_at_bridge(baseline_id);
    EXPECT_EQ(helper::apply(projection, baseline_id, final_id).disposition,
              bmd::ApplyDisposition::Applied);
    return projection;
}

[[nodiscard]] bmd::BookProjection make_usdm_at_bridge(std::uint64_t baseline_id = 500) {
    bmd::BookProjection projection{helper::spec(), bmd::SequencePolicyKind::UsdMPerpetual};
    EXPECT_EQ(helper::install(projection, baseline_id).disposition,
              bmd::InstallDisposition::Installed);
    return projection;
}

[[nodiscard]] bmd::BookProjection make_usdm_synchronized(std::uint64_t baseline_id = 500,
                                                         std::uint64_t final_id = 501) {
    auto projection = make_usdm_at_bridge(baseline_id);
    EXPECT_EQ(helper::apply(projection, baseline_id, final_id, 123).disposition,
              bmd::ApplyDisposition::Applied);
    return projection;
}

void expect_ignored(const bmd::ApplyResult& result, bmd::ApplyDisposition disposition,
                    bmd::ProjectionStatus status, std::uint64_t id) {
    EXPECT_EQ(result.disposition, disposition);
    EXPECT_EQ(result.status_after, status);
    EXPECT_EQ(result.last_update_id_after, bmd::UpdateId{id});
    EXPECT_FALSE(result.gap.has_value());
}

// GoogleTest assertion macros expand into control flow that inflates this helper's measured score.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void expect_gap(const bmd::BookProjection& projection, const bmd::ApplyResult& result,
                bmd::GapReason reason, bmd::SequencePolicyKind policy, std::uint64_t current,
                std::uint64_t first, std::uint64_t final,
                std::optional<std::uint64_t> previous = std::nullopt) {
    EXPECT_EQ(result.disposition, bmd::ApplyDisposition::GapDetected);
    EXPECT_EQ(result.status_after, bmd::ProjectionStatus::NeedsResync);
    EXPECT_EQ(result.last_update_id_after, bmd::UpdateId{current});
    ASSERT_TRUE(result.gap.has_value());
    EXPECT_EQ(result.gap->last_accepted_final, bmd::UpdateId{current});
    EXPECT_EQ(result.gap->incoming_range, helper::range(first, final));
    EXPECT_EQ(result.gap->reason, reason);
    EXPECT_EQ(result.gap->policy, policy);
    if (previous.has_value()) {
        EXPECT_EQ(result.gap->incoming_previous_final, bmd::UpdateId{previous.value()});
    } else {
        EXPECT_FALSE(result.gap->incoming_previous_final.has_value());
    }
    EXPECT_EQ(projection.last_gap(), result.gap);
    EXPECT_FALSE(projection.synchronized_book().has_value());
}

} // namespace

TEST(BookProjectionConstructionTest, StartsAwaitingBaselineWithImmutableContext) {
    const auto numeric_spec = helper::spec();
    bmd::BookProjection projection{numeric_spec, bmd::SequencePolicyKind::Spot};

    EXPECT_EQ(projection.numeric_spec(), numeric_spec);
    EXPECT_EQ(projection.policy(), bmd::SequencePolicyKind::Spot);
    EXPECT_EQ(projection.status(), bmd::ProjectionStatus::AwaitingBaseline);
    EXPECT_FALSE(projection.last_update_id().has_value());
    EXPECT_FALSE(projection.last_gap().has_value());
    EXPECT_FALSE(projection.synchronized_book().has_value());
    EXPECT_TRUE(projection.diagnostic_book().empty());
}

TEST(BookProjectionConstructionTest, MoveConstructionAndAssignmentTransferExactState) {
    auto source = make_spot_synchronized();
    const std::array updates = {
        bmd::LevelUpdate{bmd::BookSide::Bid, helper::price(100), helper::quantity(4)},
    };
    ASSERT_EQ(helper::apply(source, 502, 502, std::nullopt, updates).disposition,
              bmd::ApplyDisposition::Applied);
    const auto expected = helper::checkpoint(source);

    bmd::BookProjection moved{std::move(source)};
    EXPECT_EQ(helper::checkpoint(moved), expected);

    bmd::BookProjection assigned{helper::spec(), bmd::SequencePolicyKind::UsdMPerpetual};
    assigned = std::move(moved);
    EXPECT_EQ(helper::checkpoint(assigned), expected);
    EXPECT_EQ(assigned.policy(), bmd::SequencePolicyKind::Spot);
}

TEST(BookProjectionLifecycleTest, ApplyBeforeBaselineIsRejectedWithoutMutation) {
    bmd::BookProjection projection{helper::spec(), bmd::SequencePolicyKind::Spot};
    const auto before = helper::checkpoint(projection);
    const auto result = helper::apply(projection, 1, 1);

    EXPECT_EQ(result.disposition, bmd::ApplyDisposition::RejectedWrongState);
    EXPECT_FALSE(result.gap.has_value());
    EXPECT_EQ(helper::checkpoint(projection), before);
}

TEST(BookProjectionBaselineTest, InstallAndPendingReplacementUseM2Semantics) {
    bmd::BookProjection projection{helper::spec(), bmd::SequencePolicyKind::Spot};
    const std::array initial_bids = {
        bmd::BookLevel{helper::price(100), helper::quantity(2)},
    };
    const auto installed = helper::install(projection, 10, initial_bids);
    EXPECT_EQ(installed,
              (bmd::InstallResult{bmd::InstallDisposition::Installed,
                                  bmd::ProjectionStatus::AwaitingBridge, bmd::UpdateId{10}}));
    EXPECT_FALSE(projection.synchronized_book().has_value());

    const std::array replacement_bids = {
        bmd::BookLevel{helper::price(101), helper::quantity(1)},
        bmd::BookLevel{helper::price(101), helper::quantity(7)},
        bmd::BookLevel{helper::price(99), helper::quantity(0)},
    };
    const std::array replacement_asks = {
        bmd::BookLevel{helper::price(100), helper::quantity(3)},
    };
    EXPECT_EQ(helper::install(projection, 20, replacement_bids, replacement_asks).disposition,
              bmd::InstallDisposition::Installed);
    EXPECT_EQ(projection.last_update_id(), bmd::UpdateId{20});
    EXPECT_EQ(projection.diagnostic_book().level_count(bmd::BookSide::Bid), 1U);
    EXPECT_EQ(projection.diagnostic_book().quantity_at(bmd::BookSide::Bid, helper::price(101)),
              helper::quantity(7));
    EXPECT_GT(projection.diagnostic_book().best_bid()->price,
              projection.diagnostic_book().best_ask()->price);
}

TEST(BookProjectionBaselineTest, EmptyLockedAndCrossedBaselinesArePreserved) {
    bmd::BookProjection empty{helper::spec(), bmd::SequencePolicyKind::Spot};
    EXPECT_EQ(helper::install(empty, 1).disposition, bmd::InstallDisposition::Installed);
    EXPECT_TRUE(empty.diagnostic_book().empty());

    const std::array locked_bids = {
        bmd::BookLevel{helper::price(100), helper::quantity(1)},
    };
    const std::array locked_asks = {
        bmd::BookLevel{helper::price(100), helper::quantity(2)},
    };
    EXPECT_EQ(helper::install(empty, 2, locked_bids, locked_asks).disposition,
              bmd::InstallDisposition::Installed);
    EXPECT_EQ(empty.diagnostic_book().best_bid()->price, empty.diagnostic_book().best_ask()->price);

    const std::array crossed_bids = {
        bmd::BookLevel{helper::price(102), helper::quantity(1)},
    };
    const std::array crossed_asks = {
        bmd::BookLevel{helper::price(101), helper::quantity(2)},
    };
    EXPECT_EQ(helper::install(empty, 3, crossed_bids, crossed_asks).disposition,
              bmd::InstallDisposition::Installed);
    EXPECT_GT(empty.diagnostic_book().best_bid()->price, empty.diagnostic_book().best_ask()->price);
}

TEST(BookProjectionBaselineTest, InstallWhileSynchronizedIsRejected) {
    auto projection = make_spot_synchronized();
    const auto before = helper::checkpoint(projection);
    const auto result = helper::install(projection, 900);
    EXPECT_EQ(result.disposition, bmd::InstallDisposition::RejectedWrongState);
    EXPECT_EQ(result.status_after, bmd::ProjectionStatus::Synchronized);
    EXPECT_EQ(helper::checkpoint(projection), before);
}

class SpotBootstrapTableTest
    : public ::testing::TestWithParam<
          std::tuple<std::uint64_t, std::uint64_t, bmd::ApplyDisposition, bmd::ProjectionStatus,
                     std::optional<bmd::GapReason>>> {};

TEST_P(SpotBootstrapTableTest, AppliesAcceptedIntervalsAndClassifiesOthers) {
    const auto [first, final, disposition, status, reason] = GetParam();
    auto projection = make_spot_at_bridge();
    const auto result = helper::apply(projection, first, final, 999);
    EXPECT_EQ(result.disposition, disposition);
    EXPECT_EQ(result.status_after, status);
    if (reason.has_value()) {
        expect_gap(projection, result, reason.value(), bmd::SequencePolicyKind::Spot, 500, first,
                   final, 999);
    } else {
        EXPECT_FALSE(result.gap.has_value());
    }
}

INSTANTIATE_TEST_SUITE_P(
    ApprovedIntervals, SpotBootstrapTableTest,
    ::testing::Values(
        std::make_tuple(499U, 501U, bmd::ApplyDisposition::Applied,
                        bmd::ProjectionStatus::Synchronized, std::optional<bmd::GapReason>{}),
        std::make_tuple(500U, 501U, bmd::ApplyDisposition::Applied,
                        bmd::ProjectionStatus::Synchronized, std::optional<bmd::GapReason>{}),
        std::make_tuple(501U, 501U, bmd::ApplyDisposition::GapDetected,
                        bmd::ProjectionStatus::NeedsResync,
                        std::optional{bmd::GapReason::SpotBootstrapForwardGap}),
        std::make_tuple(501U, 502U, bmd::ApplyDisposition::GapDetected,
                        bmd::ProjectionStatus::NeedsResync,
                        std::optional{bmd::GapReason::SpotBootstrapForwardGap}),
        std::make_tuple(400U, 499U, bmd::ApplyDisposition::IgnoredStale,
                        bmd::ProjectionStatus::AwaitingBridge, std::optional<bmd::GapReason>{}),
        std::make_tuple(499U, 500U, bmd::ApplyDisposition::IgnoredDuplicate,
                        bmd::ProjectionStatus::AwaitingBridge, std::optional<bmd::GapReason>{})));

TEST(BookProjectionSpotBootstrapTest, AcceptedBridgeAppliesLevelsAndIgnoresPreviousFinal) {
    auto projection = make_spot_at_bridge();
    const std::array levels = {
        bmd::LevelUpdate{bmd::BookSide::Bid, helper::price(100), helper::quantity(3)},
        bmd::LevelUpdate{bmd::BookSide::Ask, helper::price(101), helper::quantity(4)},
    };
    const auto result = helper::apply(projection, 499, 501, 42, levels);

    EXPECT_EQ(result.disposition, bmd::ApplyDisposition::Applied);
    EXPECT_EQ(result.last_update_id_after, bmd::UpdateId{501});
    ASSERT_TRUE(projection.synchronized_book().has_value());
    EXPECT_EQ(projection.synchronized_book()->get().best_bid()->quantity, helper::quantity(3));
}

TEST(BookProjectionSpotBootstrapTest, MaxBaselineCannotAdvance) {
    auto projection = make_spot_at_bridge(std::numeric_limits<std::uint64_t>::max());
    expect_ignored(helper::apply(projection, UINT64_MAX, UINT64_MAX),
                   bmd::ApplyDisposition::IgnoredDuplicate, bmd::ProjectionStatus::AwaitingBridge,
                   UINT64_MAX);
    expect_ignored(helper::apply(projection, UINT64_MAX - 1U, UINT64_MAX - 1U),
                   bmd::ApplyDisposition::IgnoredStale, bmd::ProjectionStatus::AwaitingBridge,
                   UINT64_MAX);
}

TEST(BookProjectionSpotLiveTest, AcceptsOverlapExactNextWideAndEmptyUpdates) {
    auto projection = make_spot_synchronized();
    EXPECT_EQ(helper::apply(projection, 400, 502).disposition, bmd::ApplyDisposition::Applied);
    EXPECT_EQ(helper::apply(projection, 503, 510).disposition, bmd::ApplyDisposition::Applied);
    EXPECT_EQ(helper::apply(projection, 500, 700, 0).disposition, bmd::ApplyDisposition::Applied);
    EXPECT_EQ(projection.last_update_id(), bmd::UpdateId{700});
}

TEST(BookProjectionSpotLiveTest, StaleDuplicateAndForwardGapDoNotMutateBook) {
    auto projection = make_spot_synchronized();
    const std::array baseline_update = {
        bmd::LevelUpdate{bmd::BookSide::Bid, helper::price(100), helper::quantity(3)},
    };
    ASSERT_EQ(helper::apply(projection, 502, 502, std::nullopt, baseline_update).disposition,
              bmd::ApplyDisposition::Applied);

    const auto stable = helper::checkpoint(projection);
    expect_ignored(helper::apply(projection, 1, 501), bmd::ApplyDisposition::IgnoredStale,
                   bmd::ProjectionStatus::Synchronized, 502);
    EXPECT_EQ(helper::checkpoint(projection), stable);
    expect_ignored(helper::apply(projection, 1, 502, 999), bmd::ApplyDisposition::IgnoredDuplicate,
                   bmd::ProjectionStatus::Synchronized, 502);
    EXPECT_EQ(helper::checkpoint(projection), stable);

    const auto gap = helper::apply(projection, 504, 504);
    expect_gap(projection, gap, bmd::GapReason::SpotLiveForwardGap, bmd::SequencePolicyKind::Spot,
               502, 504, 504);
    EXPECT_EQ(projection.diagnostic_book().all_levels(bmd::BookSide::Bid), stable.bids);
}

TEST(BookProjectionSpotLiveTest, MaxFinalIsAppliedWithoutOverflow) {
    auto projection = make_spot_synchronized(10, 11);
    EXPECT_EQ(helper::apply(projection, 12, UINT64_MAX).disposition,
              bmd::ApplyDisposition::Applied);
    EXPECT_EQ(projection.last_update_id(), bmd::UpdateId{UINT64_MAX});
    expect_ignored(helper::apply(projection, UINT64_MAX, UINT64_MAX),
                   bmd::ApplyDisposition::IgnoredDuplicate, bmd::ProjectionStatus::Synchronized,
                   UINT64_MAX);
}

TEST(BookProjectionUsdMBootstrapTest, AdvancingBridgeContainsBaselineAndDoesNotComparePuToL) {
    auto projection = make_usdm_at_bridge();
    const auto result = helper::apply(projection, 499, 501, 123);
    EXPECT_EQ(result.disposition, bmd::ApplyDisposition::Applied);
    EXPECT_EQ(result.last_update_id_after, bmd::UpdateId{501});
    EXPECT_EQ(result.status_after, bmd::ProjectionStatus::Synchronized);
}

TEST(BookProjectionUsdMBootstrapTest, EqualityBridgeAppliesLevelsWithoutAdvancingId) {
    const std::array baseline_bids = {
        bmd::BookLevel{helper::price(100), helper::quantity(1)},
    };
    bmd::BookProjection projection{helper::spec(), bmd::SequencePolicyKind::UsdMPerpetual};
    ASSERT_EQ(helper::install(projection, 500, baseline_bids).disposition,
              bmd::InstallDisposition::Installed);
    const std::array levels = {
        bmd::LevelUpdate{bmd::BookSide::Bid, helper::price(100), helper::quantity(7)},
        bmd::LevelUpdate{bmd::BookSide::Bid, helper::price(90), helper::quantity(2)},
    };
    const auto result = helper::apply(projection, 499, 500, 17, levels);

    EXPECT_EQ(result.disposition, bmd::ApplyDisposition::Applied);
    EXPECT_EQ(result.last_update_id_after, bmd::UpdateId{500});
    EXPECT_EQ(projection.status(), bmd::ProjectionStatus::Synchronized);
    EXPECT_EQ(projection.diagnostic_book().quantity_at(bmd::BookSide::Bid, helper::price(100)),
              helper::quantity(7));
    EXPECT_EQ(projection.diagnostic_book().quantity_at(bmd::BookSide::Bid, helper::price(90)),
              helper::quantity(2));
}

TEST(BookProjectionUsdMBootstrapTest, ValidatesRangeBeforePuAndStaleBeforeBoth) {
    auto range_miss = make_usdm_at_bridge();
    auto result = helper::apply(range_miss, 501, 501);
    expect_gap(range_miss, result, bmd::GapReason::FuturesBootstrapRangeMiss,
               bmd::SequencePolicyKind::UsdMPerpetual, 500, 501, 501);

    auto missing = make_usdm_at_bridge();
    result = helper::apply(missing, 499, 501);
    expect_gap(missing, result, bmd::GapReason::FuturesMissingPreviousFinal,
               bmd::SequencePolicyKind::UsdMPerpetual, 500, 499, 501);

    auto stale = make_usdm_at_bridge();
    expect_ignored(helper::apply(stale, 1, 499), bmd::ApplyDisposition::IgnoredStale,
                   bmd::ProjectionStatus::AwaitingBridge, 500);
}

TEST(BookProjectionUsdMLiveTest, AcceptsMatchingPuRegardlessOfInterval) {
    auto projection = make_usdm_synchronized();
    const std::array levels = {
        bmd::LevelUpdate{bmd::BookSide::Ask, helper::price(200), helper::quantity(5)},
    };
    const auto result = helper::apply(projection, 900, 901, 501, levels);
    EXPECT_EQ(result.disposition, bmd::ApplyDisposition::Applied);
    EXPECT_EQ(result.last_update_id_after, bmd::UpdateId{901});
    EXPECT_EQ(projection.diagnostic_book().best_ask()->quantity, helper::quantity(5));
    EXPECT_EQ(helper::apply(projection, 1, 902, 901).disposition, bmd::ApplyDisposition::Applied);
}

TEST(BookProjectionUsdMLiveTest, StaleAndDuplicatePrecedePuValidation) {
    auto projection = make_usdm_synchronized();
    expect_ignored(helper::apply(projection, 1, 500), bmd::ApplyDisposition::IgnoredStale,
                   bmd::ProjectionStatus::Synchronized, 501);
    expect_ignored(helper::apply(projection, 1, 501), bmd::ApplyDisposition::IgnoredDuplicate,
                   bmd::ProjectionStatus::Synchronized, 501);
}

TEST(BookProjectionUsdMLiveTest, MissingAndMismatchedPuAreDistinctGaps) {
    auto missing = make_usdm_synchronized();
    auto result = helper::apply(missing, 502, 502);
    expect_gap(missing, result, bmd::GapReason::FuturesMissingPreviousFinal,
               bmd::SequencePolicyKind::UsdMPerpetual, 501, 502, 502);

    auto mismatch = make_usdm_synchronized();
    result = helper::apply(mismatch, 502, 502, 500);
    expect_gap(mismatch, result, bmd::GapReason::FuturesPreviousFinalMismatch,
               bmd::SequencePolicyKind::UsdMPerpetual, 501, 502, 502, 500);
}

TEST(BookProjectionLifecycleTest, GapQuarantinesBookAndRejectsFurtherUpdates) {
    auto projection = make_spot_synchronized();
    const std::array levels = {
        bmd::LevelUpdate{bmd::BookSide::Bid, helper::price(100), helper::quantity(4)},
    };
    ASSERT_EQ(helper::apply(projection, 502, 502, std::nullopt, levels).disposition,
              bmd::ApplyDisposition::Applied);
    const auto reliable = helper::checkpoint(projection);
    ASSERT_EQ(helper::apply(projection, 504, 504).disposition, bmd::ApplyDisposition::GapDetected);
    const auto quarantined = helper::checkpoint(projection);
    EXPECT_FALSE(quarantined.synchronized_visible);
    EXPECT_EQ(quarantined.bids, reliable.bids);
    EXPECT_EQ(quarantined.last_update_id, reliable.last_update_id);

    const auto result = helper::apply(projection, 503, 503);
    EXPECT_EQ(result.disposition, bmd::ApplyDisposition::RejectedWrongState);
    EXPECT_FALSE(result.gap.has_value());
    EXPECT_EQ(helper::checkpoint(projection), quarantined);
}

TEST(BookProjectionLifecycleTest, RebaselineAndBridgeRetainHistoricalGapUntilReset) {
    auto projection = make_spot_synchronized();
    ASSERT_EQ(helper::apply(projection, 503, 503).disposition, bmd::ApplyDisposition::GapDetected);
    const auto historical_gap = projection.last_gap();
    ASSERT_TRUE(historical_gap.has_value());

    const std::array new_bids = {
        bmd::BookLevel{helper::price(90), helper::quantity(9)},
    };
    EXPECT_EQ(helper::install(projection, 600, new_bids).disposition,
              bmd::InstallDisposition::Installed);
    EXPECT_EQ(projection.status(), bmd::ProjectionStatus::AwaitingBridge);
    EXPECT_EQ(projection.last_gap(), historical_gap);
    EXPECT_EQ(helper::apply(projection, 599, 601).disposition, bmd::ApplyDisposition::Applied);
    EXPECT_EQ(projection.last_gap(), historical_gap);

    projection.reset();
    EXPECT_EQ(projection.status(), bmd::ProjectionStatus::AwaitingBaseline);
    EXPECT_FALSE(projection.last_update_id().has_value());
    EXPECT_FALSE(projection.last_gap().has_value());
    EXPECT_TRUE(projection.diagnostic_book().empty());
    EXPECT_EQ(projection.policy(), bmd::SequencePolicyKind::Spot);
    EXPECT_EQ(projection.numeric_spec(), helper::spec());
}

// GoogleTest assertion macros inside the state loop inflate the measured score.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST(BookProjectionLifecycleTest, ResetWorksFromEveryState) {
    std::vector<bmd::BookProjection> projections;
    projections.emplace_back(helper::spec(), bmd::SequencePolicyKind::Spot);
    projections.push_back(make_spot_at_bridge());
    projections.push_back(make_spot_synchronized());
    auto needs_resync = make_spot_synchronized();
    ASSERT_EQ(helper::apply(needs_resync, 503, 503).disposition,
              bmd::ApplyDisposition::GapDetected);
    projections.push_back(std::move(needs_resync));

    for (auto& projection : projections) {
        projection.reset();
        EXPECT_EQ(projection.status(), bmd::ProjectionStatus::AwaitingBaseline);
        EXPECT_FALSE(projection.last_update_id().has_value());
        EXPECT_FALSE(projection.last_gap().has_value());
        EXPECT_TRUE(projection.diagnostic_book().empty());
    }
}

TEST(BookProjectionBookSemanticsTest, AcceptedBatchUsesAbsoluteQuantityAndOrderedLastWriteWins) {
    auto projection = make_spot_synchronized();
    const std::array insert = {
        bmd::LevelUpdate{bmd::BookSide::Bid, helper::price(100), helper::quantity(1)},
        bmd::LevelUpdate{bmd::BookSide::Bid, helper::price(100), helper::quantity(5)},
        bmd::LevelUpdate{bmd::BookSide::Ask, helper::price(100), helper::quantity(2)},
    };
    ASSERT_EQ(helper::apply(projection, 502, 502, std::nullopt, insert).disposition,
              bmd::ApplyDisposition::Applied);
    EXPECT_EQ(projection.diagnostic_book().quantity_at(bmd::BookSide::Bid, helper::price(100)),
              helper::quantity(5));
    EXPECT_EQ(projection.diagnostic_book().best_bid()->price,
              projection.diagnostic_book().best_ask()->price);

    const std::array remove = {
        bmd::LevelUpdate{bmd::BookSide::Bid, helper::price(999), helper::quantity(0)},
        bmd::LevelUpdate{bmd::BookSide::Bid, helper::price(100), helper::quantity(0)},
    };
    ASSERT_EQ(helper::apply(projection, 503, 503, std::nullopt, remove).disposition,
              bmd::ApplyDisposition::Applied);
    EXPECT_FALSE(projection.diagnostic_book().best_bid().has_value());
}

// NOLINTEND(bugprone-unchecked-optional-access)
