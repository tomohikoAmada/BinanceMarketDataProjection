#include "reference_projection.hpp"
#include "test_helpers.hpp"

#include <binance_market_data/projection/v1/projection_state/book_projection.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

// NOLINTBEGIN(bugprone-unchecked-optional-access)
namespace bmd = binance_market_data::projection::v1;
namespace helper = bmd_projection_test;
namespace reference = bmd_projection_reference;

namespace {

class Generator final {
  public:
    explicit Generator(std::uint64_t seed) : state_{seed} {}

    [[nodiscard]] std::uint64_t next() noexcept {
        state_ = state_ * 6364136223846793005ULL + 1442695040888963407ULL;
        return state_;
    }

    [[nodiscard]] std::size_t bounded(std::size_t bound) noexcept {
        return bound == 0 ? 0 : static_cast<std::size_t>(next() % bound);
    }

  private:
    std::uint64_t state_;
};

[[nodiscard]] bmd::ProjectionStatus production_status(reference::Status status) {
    switch (status) {
    case reference::Status::AwaitingBaseline:
        return bmd::ProjectionStatus::AwaitingBaseline;
    case reference::Status::AwaitingBridge:
        return bmd::ProjectionStatus::AwaitingBridge;
    case reference::Status::Synchronized:
        return bmd::ProjectionStatus::Synchronized;
    case reference::Status::NeedsResync:
        return bmd::ProjectionStatus::NeedsResync;
    }
    return bmd::ProjectionStatus::AwaitingBaseline;
}

[[nodiscard]] bmd::SequencePolicyKind production_policy(reference::Policy policy) noexcept {
    return policy == reference::Policy::Spot ? bmd::SequencePolicyKind::Spot
                                             : bmd::SequencePolicyKind::UsdMPerpetual;
}

[[nodiscard]] bmd::ApplyDisposition production_disposition(reference::Disposition disposition) {
    switch (disposition) {
    case reference::Disposition::Applied:
        return bmd::ApplyDisposition::Applied;
    case reference::Disposition::IgnoredStale:
        return bmd::ApplyDisposition::IgnoredStale;
    case reference::Disposition::IgnoredDuplicate:
        return bmd::ApplyDisposition::IgnoredDuplicate;
    case reference::Disposition::GapDetected:
        return bmd::ApplyDisposition::GapDetected;
    case reference::Disposition::RejectedWrongState:
        return bmd::ApplyDisposition::RejectedWrongState;
    }
    return bmd::ApplyDisposition::RejectedWrongState;
}

[[nodiscard]] bmd::GapReason production_gap_reason(reference::GapReason reason) {
    switch (reason) {
    case reference::GapReason::SpotBootstrapForwardGap:
        return bmd::GapReason::SpotBootstrapForwardGap;
    case reference::GapReason::SpotLiveForwardGap:
        return bmd::GapReason::SpotLiveForwardGap;
    case reference::GapReason::FuturesBootstrapRangeMiss:
        return bmd::GapReason::FuturesBootstrapRangeMiss;
    case reference::GapReason::FuturesMissingPreviousFinal:
        return bmd::GapReason::FuturesMissingPreviousFinal;
    case reference::GapReason::FuturesPreviousFinalMismatch:
        return bmd::GapReason::FuturesPreviousFinalMismatch;
    }
    return bmd::GapReason::SpotLiveForwardGap;
}

[[nodiscard]] std::vector<reference::RawLevel> generate_levels(Generator& generator) {
    std::vector<reference::RawLevel> levels;
    const auto count = generator.bounded(5);
    levels.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        levels.push_back({generator.bounded(2) == 0,
                          static_cast<std::int64_t>(generator.bounded(32) + 1),
                          static_cast<std::int64_t>(generator.bounded(8))});
    }
    return levels;
}

[[nodiscard]] std::vector<bmd::LevelUpdate>
to_updates(const std::vector<reference::RawLevel>& raw) {
    std::vector<bmd::LevelUpdate> updates;
    updates.reserve(raw.size());
    for (const auto& level : raw) {
        updates.push_back({level.bid ? bmd::BookSide::Bid : bmd::BookSide::Ask,
                           helper::price(level.price), helper::quantity(level.quantity)});
    }
    return updates;
}

[[nodiscard]] std::vector<bmd::BookLevel>
to_baseline_side(const std::vector<reference::RawLevel>& raw, bool bid) {
    std::vector<bmd::BookLevel> levels;
    for (const auto& level : raw) {
        if (level.bid == bid) {
            levels.push_back({helper::price(level.price), helper::quantity(level.quantity)});
        }
    }
    return levels;
}

[[nodiscard]] std::vector<reference::RawLevel> raw_book(const bmd::OrderBook& book,
                                                        bmd::BookSide side) {
    std::vector<reference::RawLevel> raw;
    for (const auto& level : book.all_levels(side)) {
        raw.push_back({side == bmd::BookSide::Bid, level.price.value(), level.quantity.value()});
    }
    return raw;
}

// GoogleTest assertion macros expand into control flow that inflates the measured score.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void verify_state(const bmd::BookProjection& production,
                  const reference::ReferenceProjection& model) {
    EXPECT_EQ(production.status(), production_status(model.status()));
    if (model.last().has_value()) {
        EXPECT_EQ(production.last_update_id(), bmd::UpdateId{model.last().value()});
    } else {
        EXPECT_FALSE(production.last_update_id().has_value());
    }
    EXPECT_EQ(production.synchronized_book().has_value(), model.synchronized_visible());
    EXPECT_EQ(raw_book(production.diagnostic_book(), bmd::BookSide::Bid), model.bids());
    EXPECT_EQ(raw_book(production.diagnostic_book(), bmd::BookSide::Ask), model.asks());

    const auto production_gap = production.last_gap();
    const auto model_gap = model.last_gap();
    ASSERT_EQ(production_gap.has_value(), model_gap.has_value());
    if (model_gap.has_value()) {
        EXPECT_EQ(production_gap->last_accepted_final, bmd::UpdateId{model_gap->last});
        EXPECT_EQ(production_gap->incoming_range,
                  helper::range(model_gap->first, model_gap->final));
        if (model_gap->previous.has_value()) {
            EXPECT_EQ(production_gap->incoming_previous_final,
                      bmd::UpdateId{model_gap->previous.value()});
        } else {
            EXPECT_FALSE(production_gap->incoming_previous_final.has_value());
        }
        EXPECT_EQ(production_gap->reason, production_gap_reason(model_gap->reason));
        EXPECT_EQ(production_gap->policy, production_policy(model_gap->policy));
    }
}

// GoogleTest assertion macros expand into control flow that inflates the measured score.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void verify_apply_result(const bmd::ApplyResult& production, const reference::Result& model) {
    EXPECT_EQ(production.disposition, production_disposition(model.disposition));
    EXPECT_EQ(production.status_after, production_status(model.status));
    if (model.last.has_value()) {
        EXPECT_EQ(production.last_update_id_after, bmd::UpdateId{model.last.value()});
    } else {
        EXPECT_FALSE(production.last_update_id_after.has_value());
    }
    ASSERT_EQ(production.gap.has_value(), model.gap.has_value());
    if (model.gap.has_value()) {
        EXPECT_EQ(production.gap->last_accepted_final, bmd::UpdateId{model.gap->last});
        EXPECT_EQ(production.gap->reason, production_gap_reason(model.gap->reason));
        EXPECT_EQ(production.gap->incoming_range,
                  helper::range(model.gap->first, model.gap->final));
        if (model.gap->previous.has_value()) {
            EXPECT_EQ(production.gap->incoming_previous_final,
                      bmd::UpdateId{model.gap->previous.value()});
        } else {
            EXPECT_FALSE(production.gap->incoming_previous_final.has_value());
        }
        EXPECT_EQ(production.gap->policy, production_policy(model.gap->policy));
    }
}

[[nodiscard]] std::pair<std::uint64_t, std::uint64_t>
generate_range(Generator& generator, const reference::ReferenceProjection& model) {
    const auto current = model.last().value_or(static_cast<std::uint64_t>(generator.bounded(20)));
    switch (generator.bounded(6)) {
    case 0:
        return current == 0 ? std::pair<std::uint64_t, std::uint64_t>{0, 0}
                            : std::pair<std::uint64_t, std::uint64_t>{0, current - 1U};
    case 1:
        return {current, current};
    case 2:
        return {current, current + 1U};
    case 3:
        return {current + 1U, current + 1U};
    case 4:
        return {current + 2U, current + 3U};
    default:
        return {0U, current + static_cast<std::uint64_t>(generator.bounded(4) + 1)};
    }
}

// The explicit operation dispatcher is intentionally independent of production classification.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void run_property(std::uint64_t seed, reference::Policy policy) {
    bmd::BookProjection production{helper::spec(), production_policy(policy)};
    reference::ReferenceProjection model{policy};
    Generator generator{seed};

    constexpr std::size_t kOperations = 750;
    for (std::size_t operation = 0; operation < kOperations; ++operation) {
        std::ostringstream context;
        context << "seed=" << seed << " operation=" << operation
                << " policy=" << (policy == reference::Policy::Spot ? "Spot" : "UsdM")
                << " production_state=" << static_cast<int>(production.status())
                << " reference_state=" << static_cast<int>(model.status());
        SCOPED_TRACE(context.str());

        const auto op = generator.bounded(10);
        if (op <= 1) {
            const auto last = static_cast<std::uint64_t>(generator.bounded(100));
            const auto raw = generate_levels(generator);
            const auto bids = to_baseline_side(raw, true);
            const auto asks = to_baseline_side(raw, false);
            const auto prod_result = helper::install(production, last, bids, asks);
            const auto ref_result = model.install(last, raw);
            EXPECT_EQ(prod_result.disposition,
                      ref_result.disposition == reference::InstallDisposition::Installed
                          ? bmd::InstallDisposition::Installed
                          : bmd::InstallDisposition::RejectedWrongState);
            EXPECT_EQ(prod_result.status_after, production_status(ref_result.status));
        } else if (op == 2) {
            production.reset();
            model.reset();
        } else if (op == 3) {
            static_cast<void>(production.diagnostic_book().best_bid());
            static_cast<void>(production.synchronized_book());
        } else {
            const auto [first, final] = generate_range(generator, model);
            std::optional<std::uint64_t> previous;
            if (generator.bounded(3) != 0) {
                const auto current = model.last().value_or(0);
                previous = generator.bounded(2) == 0 ? current : current + 7U;
            }
            const auto raw = generate_levels(generator);
            const auto updates = to_updates(raw);
            const auto prod_result = helper::apply(production, first, final, previous, updates);
            const auto ref_result = model.apply(first, final, previous, raw);
            verify_apply_result(prod_result, ref_result);
        }
        verify_state(production, model);
    }
}

} // namespace

TEST(BookProjectionPropertyTest, MatchesIndependentReferenceModelForFixedSeeds) {
    constexpr std::uint64_t kSeeds[] = {1, 7, 42, 99991, 0xC0FFEE, 0x123456789ABCDEF0ULL};
    for (const auto seed : kSeeds) {
        run_property(seed, reference::Policy::Spot);
        run_property(seed ^ 0x9E3779B97F4A7C15ULL, reference::Policy::UsdM);
    }
}

// NOLINTEND(bugprone-unchecked-optional-access)
