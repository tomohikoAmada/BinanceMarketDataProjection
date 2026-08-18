#include <gtest/gtest.h>

#include "phase8_absl_btree_map.hpp"
#include "phase8_sorted_vector_batch_lww.hpp"
#include "phase8_sorted_vector_naive.hpp"
#include "phase8_std_map_control.hpp"
#include "phase8_test_common.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace {

namespace core = binance_market_data::projection::v1;
using bmd_projection::m5::phase8::test::ExpectModelMatchesCore;
using bmd_projection::m5::phase8::test::ExpectModelMatchesReference;
using bmd_projection::m5::phase8::test::L;
using bmd_projection::m5::phase8::test::Levels;
using bmd_projection::m5::phase8::test::P;
using bmd_projection::m5::phase8::test::Q;
using bmd_projection::m5::phase8::test::TestSpec;
using bmd_projection::m5::phase8::test::U;

// Deterministic fixed-seed generator. No random_device, no clock, no ambient
// nondeterminism: identical seeds yield identical operation streams on every
// platform.
struct DeterministicLcg {
    std::uint64_t state;
    explicit DeterministicLcg(std::uint64_t seed) noexcept : state{seed} {}

    [[nodiscard]] std::uint64_t next() noexcept {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return state;
    }

    [[nodiscard]] core::BookSide side() noexcept {
        return (next() & 1U) == 0U ? core::BookSide::Bid : core::BookSide::Ask;
    }

    // Small price range forces collisions (duplicates/deletes), which is what
    // exercises the batch last-write-wins and update semantics.
    [[nodiscard]] core::PriceUnits price() noexcept {
        return P(1 + static_cast<std::int64_t>(next() % 60));
    }

    // Quantity 0 is common on purpose so delete paths are exercised.
    [[nodiscard]] core::QuantityUnits quantity() noexcept {
        return Q(static_cast<std::int64_t>(next() % 40));
    }

    [[nodiscard]] std::size_t batch_size() noexcept {
        return 1 + static_cast<std::size_t>(next() % 6);
    }

    [[nodiscard]] core::LevelUpdate unit() noexcept {
        return core::LevelUpdate{side(), price(), quantity()};
    }
};

// Independently derive the LevelChange a production apply_level must produce
// from the current observable state, without consulting any candidate or the
// reference implementation.
[[nodiscard]] core::LevelChange
ExpectedLevelChange(const std::optional<core::QuantityUnits>& before,
                    core::QuantityUnits quantity) noexcept {
    if (quantity.value() == 0) {
        return before.has_value() ? core::LevelChange::Removed : core::LevelChange::Unchanged;
    }
    if (!before.has_value()) {
        return core::LevelChange::Inserted;
    }
    return before->value() == quantity.value() ? core::LevelChange::Unchanged
                                               : core::LevelChange::Updated;
}

using Phase8ModelTypes = ::testing::Types<bmd_projection::m5::phase8::Phase8StdMapControl<>,
                                          bmd_projection::m5::phase8::Phase8SortedVectorNaive<>,
                                          bmd_projection::m5::phase8::Phase8AbslBtreeMap<>,
                                          bmd_projection::m5::phase8::Phase8SortedVectorBatchLww<>>;

} // namespace

template <typename Model> class Phase8PropertyTest : public ::testing::Test {
  protected:
    using model_type = Model;

    static_assert(bmd_projection::m5::phase8::Phase8ModelConcept<Model>,
                  "Phase-8 model must satisfy the shared model protocol");

    [[nodiscard]] Model make_model() const { return Model{TestSpec()}; }
};

TYPED_TEST_SUITE(Phase8PropertyTest, Phase8ModelTypes);

// Deterministic differential property stream: every generated operation is
// applied to the candidate, the independent ReferenceOrderBook, and the
// actual production core::OrderBook. Full observable state (best bid/ask, per
// side level counts, quantity_at for touched/present/absent prices, all_levels
// both sides) is compared at every step. For apply_level the independently
// derived LevelChange is checked against both the candidate and core.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TYPED_TEST(Phase8PropertyTest, DifferentialAgainstReferenceAndCore) {
    TypeParam model = this->make_model();
    core::OrderBook book{TestSpec()};
    bmd_test::ReferenceOrderBook ref;

    DeterministicLcg rng{0xC0FFEE39C0FFEEUL};
    constexpr int kSteps = 800;
    for (int step = 0; step < kSteps; ++step) {
        const std::uint64_t kind = rng.next() % 6;
        if (kind == 0 || kind == 3) {
            const core::LevelUpdate unit = rng.unit();
            const auto before = model.quantity_at(unit.side, unit.price);
            const auto expected = ExpectedLevelChange(before, unit.quantity);

            const auto candidate_change = model.apply_level(unit.side, unit.price, unit.quantity);
            const auto core_change = book.apply_level(unit.side, unit.price, unit.quantity);
            ref.apply_level(unit.side, unit.price, unit.quantity);

            EXPECT_EQ(candidate_change, expected) << "candidate LevelChange step " << step;
            EXPECT_EQ(core_change, expected) << "core LevelChange step " << step;
            ExpectModelMatchesReference(model, ref);
            ExpectModelMatchesCore(model, book);
        } else if (kind == 1 || kind == 4) {
            std::vector<core::LevelUpdate> batch;
            batch.reserve(6);
            const std::size_t n = rng.batch_size();
            for (std::size_t i = 0; i < n; ++i) {
                batch.push_back(rng.unit());
            }
            model.apply_updates(batch);
            book.apply_updates(batch);
            for (const auto& update : batch) {
                ref.apply_level(update.side, update.price, update.quantity);
            }
            ExpectModelMatchesReference(model, ref);
            ExpectModelMatchesCore(model, book);
        } else if (kind == 2) {
            std::vector<core::BookLevel> bids;
            std::vector<core::BookLevel> asks;
            for (int i = 0; i < 3; ++i) {
                bids.push_back(L(rng.price().value(), rng.quantity().value()));
                asks.push_back(L(rng.price().value(), rng.quantity().value()));
            }
            model.replace_all(bids, asks);
            book.replace_all(bids, asks);
            // ReferenceOrderBook has no replace_all; replace_all replaces the
            // entire book, so the reference must be cleared before the input
            // levels are applied sequentially in input order.
            ref.clear();
            for (const auto& level : bids) {
                ref.apply_level(core::BookSide::Bid, level.price, level.quantity);
            }
            for (const auto& level : asks) {
                ref.apply_level(core::BookSide::Ask, level.price, level.quantity);
            }
            ExpectModelMatchesReference(model, ref);
            ExpectModelMatchesCore(model, book);
        } else {
            if ((rng.next() & 1U) == 0U) {
                const auto side = rng.side();
                model.clear_side(side);
                book.clear_side(side);
                ref.clear_side(side);
            } else {
                model.clear();
                book.clear();
                ref.clear();
            }
            ExpectModelMatchesReference(model, ref);
            ExpectModelMatchesCore(model, book);
        }
    }
}

// Focused hand-written cross-check against the actual production core::OrderBook
// covering locked/crossed books, same price on both sides, duplicate batches,
// replace_all duplicate/zero ordering, and clear/clear_side.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TYPED_TEST(Phase8PropertyTest, FocusedCoreOrderBookCrossCheck) {
    TypeParam model = this->make_model();
    core::OrderBook book{TestSpec()};

    const auto probe = [&](const char* context) {
        SCOPED_TRACE(context);
        ExpectModelMatchesCore(model, book);
    };

    // Locked and crossed books.
    EXPECT_EQ(model.apply_level(core::BookSide::Bid, P(100), Q(5)), core::LevelChange::Inserted);
    EXPECT_EQ(book.apply_level(core::BookSide::Bid, P(100), Q(5)), core::LevelChange::Inserted);
    EXPECT_EQ(model.apply_level(core::BookSide::Ask, P(100), Q(5)), core::LevelChange::Inserted);
    EXPECT_EQ(book.apply_level(core::BookSide::Ask, P(100), Q(5)), core::LevelChange::Inserted);
    probe("locked book");
    EXPECT_EQ(model.apply_level(core::BookSide::Bid, P(200), Q(1)), core::LevelChange::Inserted);
    EXPECT_EQ(book.apply_level(core::BookSide::Bid, P(200), Q(1)), core::LevelChange::Inserted);
    EXPECT_EQ(model.apply_level(core::BookSide::Ask, P(90), Q(1)), core::LevelChange::Inserted);
    EXPECT_EQ(book.apply_level(core::BookSide::Ask, P(90), Q(1)), core::LevelChange::Inserted);
    probe("crossed book");

    // Same price on opposite sides stays independent.
    model.apply_updates(bmd_projection::m5::phase8::test::Updates(
        {U(core::BookSide::Bid, 50, 7), U(core::BookSide::Ask, 50, 9)}));
    book.apply_updates(bmd_projection::m5::phase8::test::Updates(
        {U(core::BookSide::Bid, 50, 7), U(core::BookSide::Ask, 50, 9)}));
    probe("same price both sides");

    // Duplicate batches with delete/reinsert patterns.
    model.apply_updates(bmd_projection::m5::phase8::test::Updates(
        {U(core::BookSide::Bid, 50, 0), U(core::BookSide::Bid, 50, 20)}));
    book.apply_updates(bmd_projection::m5::phase8::test::Updates(
        {U(core::BookSide::Bid, 50, 0), U(core::BookSide::Bid, 50, 20)}));
    probe("delete then reinsert");

    // replace_all duplicate/zero reinsert ordering.
    model.replace_all(
        bmd_projection::m5::phase8::test::Levels(
            {L(1, 1), L(2, 2), L(2, 0), L(3, 5), L(1, 9), L(4, 0), L(4, 7)}),
        bmd_projection::m5::phase8::test::Levels({L(10, 1), L(11, 2), L(10, 0), L(12, 3)}));
    book.replace_all(
        bmd_projection::m5::phase8::test::Levels(
            {L(1, 1), L(2, 2), L(2, 0), L(3, 5), L(1, 9), L(4, 0), L(4, 7)}),
        bmd_projection::m5::phase8::test::Levels({L(10, 1), L(11, 2), L(10, 0), L(12, 3)}));
    probe("replace_all duplicates and zeros");

    // clear_side / clear.
    model.clear_side(core::BookSide::Bid);
    book.clear_side(core::BookSide::Bid);
    probe("clear_side bid");
    model.clear();
    book.clear();
    probe("clear all");
}