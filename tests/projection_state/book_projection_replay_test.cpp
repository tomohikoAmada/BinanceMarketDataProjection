#include "test_helpers.hpp"

#include <binance_market_data/projection/v1/projection_state/book_projection.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <vector>

namespace bmd = binance_market_data::projection::v1;
namespace helper = bmd_projection_test;

namespace {

struct TranscriptOutcome final {
    std::vector<bmd::InstallResult> installs;
    std::vector<bmd::ApplyResult> applies;
    helper::ProjectionCheckpoint checkpoint;

    friend bool operator==(const TranscriptOutcome&, const TranscriptOutcome&) = default;
};

[[nodiscard]] TranscriptOutcome replay_spot(bmd::BookProjection& projection) {
    TranscriptOutcome outcome{{}, {}, helper::checkpoint(projection)};
    const std::array baseline = {
        bmd::BookLevel{helper::price(100), helper::quantity(5)},
    };
    const std::array change = {
        bmd::LevelUpdate{bmd::BookSide::Bid, helper::price(100), helper::quantity(4)},
    };
    outcome.installs.push_back(helper::install(projection, 500, baseline));
    outcome.applies.push_back(helper::apply(projection, 499, 501, std::nullopt, change));
    outcome.applies.push_back(helper::apply(projection, 500, 502));
    outcome.applies.push_back(helper::apply(projection, 503, 503));
    outcome.applies.push_back(helper::apply(projection, 503, 503, 999));
    outcome.applies.push_back(helper::apply(projection, 1, 502));
    outcome.applies.push_back(helper::apply(projection, 505, 505));
    outcome.installs.push_back(helper::install(projection, 600, baseline));
    outcome.applies.push_back(helper::apply(projection, 599, 601));
    outcome.checkpoint = helper::checkpoint(projection);
    return outcome;
}

[[nodiscard]] TranscriptOutcome replay_usdm(bmd::BookProjection& projection) {
    TranscriptOutcome outcome{{}, {}, helper::checkpoint(projection)};
    const std::array limited_baseline = {
        bmd::BookLevel{helper::price(100), helper::quantity(5)},
    };
    const std::array equality_changes = {
        bmd::LevelUpdate{bmd::BookSide::Bid, helper::price(90), helper::quantity(7)},
    };
    outcome.installs.push_back(helper::install(projection, 700, limited_baseline));
    outcome.applies.push_back(helper::apply(projection, 699, 700, 650, equality_changes));
    outcome.applies.push_back(helper::apply(projection, 705, 710, 700));
    outcome.applies.push_back(helper::apply(projection, 1, 710, 0));
    outcome.applies.push_back(helper::apply(projection, 1, 699));
    outcome.applies.push_back(helper::apply(projection, 711, 711, 700));
    outcome.installs.push_back(helper::install(projection, 800, limited_baseline));
    outcome.applies.push_back(helper::apply(projection, 799, 801, 12));
    outcome.checkpoint = helper::checkpoint(projection);
    return outcome;
}

template <typename Replay>
void expect_deterministic_replay(bmd::SequencePolicyKind policy, Replay replay) {
    bmd::BookProjection first{helper::spec(), policy};
    bmd::BookProjection second{helper::spec(), policy};
    const auto first_outcome = replay(first);
    const auto second_outcome = replay(second);
    EXPECT_EQ(first_outcome, second_outcome);

    first.reset();
    const auto after_reset = replay(first);
    bmd::BookProjection fresh{helper::spec(), policy};
    const auto fresh_outcome = replay(fresh);
    EXPECT_EQ(after_reset, fresh_outcome);
}

} // namespace

TEST(BookProjectionReplayTest, SpotTranscriptIsDeterministicAcrossFreshAndResetReplay) {
    expect_deterministic_replay(bmd::SequencePolicyKind::Spot, replay_spot);
}

TEST(BookProjectionReplayTest, UsdMTranscriptIsDeterministicAcrossFreshAndResetReplay) {
    expect_deterministic_replay(bmd::SequencePolicyKind::UsdMPerpetual, replay_usdm);
}
