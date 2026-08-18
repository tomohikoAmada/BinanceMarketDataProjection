#include <gtest/gtest.h>

#include "phase8_absl_btree_map.hpp"
#include "phase8_sorted_vector_batch_lww.hpp"
#include "phase8_sorted_vector_naive.hpp"
#include "phase8_std_map_control.hpp"
#include "phase8_test_common.hpp"

#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>
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
using bmd_projection::m5::phase8::test::Updates;

using Phase8ModelTypes = ::testing::Types<bmd_projection::m5::phase8::Phase8StdMapControl<>,
                                          bmd_projection::m5::phase8::Phase8SortedVectorNaive<>,
                                          bmd_projection::m5::phase8::Phase8AbslBtreeMap<>,
                                          bmd_projection::m5::phase8::Phase8SortedVectorBatchLww<>>;

} // namespace

template <typename Model> class Phase8ConformanceTest : public ::testing::Test {
  protected:
    using model_type = Model;

    static_assert(bmd_projection::m5::phase8::Phase8ModelConcept<Model>,
                  "Phase-8 model must satisfy the shared model protocol");
    static_assert(std::is_nothrow_move_constructible_v<Model>,
                  "Phase-8 model requires noexcept move construction");
    static_assert(std::is_nothrow_move_assignable_v<Model>,
                  "Phase-8 model requires noexcept move assignment");

    [[nodiscard]] Model make_model() const { return Model{TestSpec()}; }
};

TYPED_TEST_SUITE(Phase8ConformanceTest, Phase8ModelTypes);

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TYPED_TEST(Phase8ConformanceTest, EmptyAndIdentity) {
    TypeParam model = this->make_model();

    EXPECT_TRUE(model.empty()) << "fresh model must be empty";
    EXPECT_TRUE(model.side_empty(core::BookSide::Bid));
    EXPECT_TRUE(model.side_empty(core::BookSide::Ask));
    EXPECT_EQ(model.level_count(core::BookSide::Bid), 0U);
    EXPECT_EQ(model.level_count(core::BookSide::Ask), 0U);
    EXPECT_FALSE(model.best_bid().has_value());
    EXPECT_FALSE(model.best_ask().has_value());
    EXPECT_EQ(model.all_levels(core::BookSide::Bid), (std::vector<core::BookLevel>{}));
    EXPECT_EQ(model.all_levels(core::BookSide::Ask), (std::vector<core::BookLevel>{}));

    EXPECT_EQ(model.numeric_spec(), TestSpec()) << "NumericSpec identity must be retained";

    ASSERT_TRUE(TypeParam::model_id() == bmd_projection::m5::phase8::kPhase8StdMapControlId ||
                TypeParam::model_id() == bmd_projection::m5::phase8::kPhase8SortedVectorNaiveId ||
                TypeParam::model_id() == bmd_projection::m5::phase8::kPhase8AbslBtreeMapId ||
                TypeParam::model_id() ==
                    bmd_projection::m5::phase8::kPhase8SortedVectorBatchLwwId);
    EXPECT_FALSE(TypeParam::model_id().empty());
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TYPED_TEST(Phase8ConformanceTest, ApplyLevel) {
    TypeParam model = this->make_model();

    EXPECT_EQ(model.apply_level(core::BookSide::Bid, P(100), Q(10)), core::LevelChange::Inserted);
    EXPECT_EQ(model.quantity_at(core::BookSide::Bid, P(100)), Q(10));

    EXPECT_EQ(model.apply_level(core::BookSide::Ask, P(101), Q(20)), core::LevelChange::Inserted);
    EXPECT_EQ(model.quantity_at(core::BookSide::Ask, P(101)), Q(20));

    EXPECT_EQ(model.apply_level(core::BookSide::Bid, P(100), Q(15)), core::LevelChange::Updated);
    EXPECT_EQ(model.quantity_at(core::BookSide::Bid, P(100)), Q(15));

    EXPECT_EQ(model.apply_level(core::BookSide::Bid, P(100), Q(15)), core::LevelChange::Unchanged);

    EXPECT_EQ(model.apply_level(core::BookSide::Bid, P(100), Q(0)), core::LevelChange::Removed);
    EXPECT_FALSE(model.quantity_at(core::BookSide::Bid, P(100)).has_value());

    EXPECT_EQ(model.apply_level(core::BookSide::Bid, P(100), Q(0)), core::LevelChange::Unchanged);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TYPED_TEST(Phase8ConformanceTest, Ordering) {
    TypeParam model = this->make_model();

    // Bids must iterate descending.
    for (const auto price : {20, 5, 10, 15}) {
        static_cast<void>(model.apply_level(core::BookSide::Bid, P(price), Q(1)));
    }
    EXPECT_EQ(model.best_bid(), std::make_optional(L(20, 1)));
    EXPECT_EQ(model.all_levels(core::BookSide::Bid),
              Levels({L(20, 1), L(15, 1), L(10, 1), L(5, 1)}));

    // Asks must iterate ascending.
    for (const auto price : {5, 20, 10, 15}) {
        static_cast<void>(model.apply_level(core::BookSide::Ask, P(price), Q(1)));
    }
    EXPECT_EQ(model.best_ask(), std::make_optional(L(5, 1)));
    EXPECT_EQ(model.all_levels(core::BookSide::Ask),
              Levels({L(5, 1), L(10, 1), L(15, 1), L(20, 1)}));

    // Locked book accepted (equal bid and ask prices).
    TypeParam locked = this->make_model();
    EXPECT_NO_THROW(locked.apply_level(core::BookSide::Bid, P(100), Q(5)));
    EXPECT_NO_THROW(locked.apply_level(core::BookSide::Ask, P(100), Q(5)));

    // Crossed book accepted (bid above ask).
    TypeParam crossed = this->make_model();
    EXPECT_NO_THROW(crossed.apply_level(core::BookSide::Bid, P(100), Q(5)));
    EXPECT_NO_THROW(crossed.apply_level(core::BookSide::Ask, P(90), Q(5)));
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TYPED_TEST(Phase8ConformanceTest, Query) {
    TypeParam model = this->make_model();

    EXPECT_EQ(model.best_bid(), std::nullopt);
    EXPECT_EQ(model.best_ask(), std::nullopt);
    EXPECT_EQ(model.quantity_at(core::BookSide::Bid, P(10)), std::nullopt);
    EXPECT_EQ(model.quantity_at(core::BookSide::Ask, P(10)), std::nullopt);
    EXPECT_EQ(model.top_levels(core::BookSide::Bid, 0), (std::vector<core::BookLevel>{}));
    EXPECT_EQ(model.top_levels(core::BookSide::Ask, 0), (std::vector<core::BookLevel>{}));

    for (const auto price : {50, 10, 90, 30}) {
        static_cast<void>(model.apply_level(core::BookSide::Bid, P(price), Q(price)));
        static_cast<void>(model.apply_level(core::BookSide::Ask, P(price + 1), Q(price)));
    }

    EXPECT_EQ(model.best_bid(), std::make_optional(L(90, 90)));
    EXPECT_EQ(model.best_ask(), std::make_optional(L(11, 10)));

    EXPECT_EQ(model.quantity_at(core::BookSide::Bid, P(30)), Q(30));
    EXPECT_EQ(model.quantity_at(core::BookSide::Bid, P(999)), std::nullopt);
    EXPECT_EQ(model.quantity_at(core::BookSide::Ask, P(31)), Q(30));
    EXPECT_EQ(model.quantity_at(core::BookSide::Ask, P(999)), std::nullopt);

    EXPECT_EQ(model.top_levels(core::BookSide::Bid, 1), Levels({L(90, 90)}));
    EXPECT_EQ(model.top_levels(core::BookSide::Bid, 4),
              Levels({L(90, 90), L(50, 50), L(30, 30), L(10, 10)}));
    EXPECT_EQ(model.top_levels(core::BookSide::Bid, 100),
              Levels({L(90, 90), L(50, 50), L(30, 30), L(10, 10)}));

    EXPECT_EQ(model.all_levels(core::BookSide::Bid),
              Levels({L(90, 90), L(50, 50), L(30, 30), L(10, 10)}));
    EXPECT_EQ(model.all_levels(core::BookSide::Ask),
              Levels({L(11, 10), L(31, 30), L(51, 50), L(91, 90)}));
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TYPED_TEST(Phase8ConformanceTest, Batch) {
    TypeParam model = this->make_model();

    // Updates on different prices.
    model.apply_updates(Updates({U(core::BookSide::Bid, 10, 1), U(core::BookSide::Ask, 11, 2),
                                 U(core::BookSide::Bid, 20, 3), U(core::BookSide::Ask, 21, 4)}));
    EXPECT_EQ(model.quantity_at(core::BookSide::Bid, P(10)), Q(1));
    EXPECT_EQ(model.quantity_at(core::BookSide::Ask, P(11)), Q(2));
    EXPECT_EQ(model.quantity_at(core::BookSide::Bid, P(20)), Q(3));
    EXPECT_EQ(model.quantity_at(core::BookSide::Ask, P(21)), Q(4));

    // Duplicate same price: last write wins.
    model.apply_updates(Updates({U(core::BookSide::Bid, 10, 5), U(core::BookSide::Bid, 10, 6)}));
    EXPECT_EQ(model.quantity_at(core::BookSide::Bid, P(10)), Q(6));

    // update -> delete leaves the price absent.
    model.apply_updates(Updates({U(core::BookSide::Bid, 10, 7), U(core::BookSide::Bid, 10, 0)}));
    EXPECT_FALSE(model.quantity_at(core::BookSide::Bid, P(10)).has_value());

    // delete -> insert reinserts at the later quantity.
    model.apply_updates(Updates({U(core::BookSide::Bid, 20, 0), U(core::BookSide::Bid, 20, 8)}));
    EXPECT_EQ(model.quantity_at(core::BookSide::Bid, P(20)), Q(8));

    // Multiple duplicate prices in one batch.
    model.apply_updates(Updates({
        U(core::BookSide::Bid, 30, 1),
        U(core::BookSide::Bid, 30, 2),
        U(core::BookSide::Bid, 40, 3),
        U(core::BookSide::Ask, 31, 4),
        U(core::BookSide::Ask, 31, 5),
        U(core::BookSide::Ask, 41, 6),
    }));
    EXPECT_EQ(model.quantity_at(core::BookSide::Bid, P(30)), Q(2));
    EXPECT_EQ(model.quantity_at(core::BookSide::Bid, P(40)), Q(3));
    EXPECT_EQ(model.quantity_at(core::BookSide::Ask, P(31)), Q(5));
    EXPECT_EQ(model.quantity_at(core::BookSide::Ask, P(41)), Q(6));

    // Same numeric price on opposite sides stays independent.
    model.apply_updates(Updates({U(core::BookSide::Bid, 50, 7), U(core::BookSide::Ask, 50, 9)}));
    EXPECT_EQ(model.quantity_at(core::BookSide::Bid, P(50)), Q(7));
    EXPECT_EQ(model.quantity_at(core::BookSide::Ask, P(50)), Q(9));

    // Exact last-write-wins: price -> 10 -> 20 -> 0 leaves absent; 0 -> 20 leaves 20.
    model.apply_updates(Updates({U(core::BookSide::Bid, 60, 10), U(core::BookSide::Bid, 60, 20),
                                 U(core::BookSide::Bid, 60, 0)}));
    EXPECT_FALSE(model.quantity_at(core::BookSide::Bid, P(60)).has_value());
    model.apply_updates(Updates({U(core::BookSide::Bid, 70, 0), U(core::BookSide::Bid, 70, 20)}));
    EXPECT_EQ(model.quantity_at(core::BookSide::Bid, P(70)), Q(20));

    // Whole-batch equivalence with the independent reference oracle applied
    // sequentially production-style.
    bmd_test::ReferenceOrderBook ref;
    const auto batch = Updates({
        U(core::BookSide::Bid, 100, 5),
        U(core::BookSide::Bid, 110, 6),
        U(core::BookSide::Ask, 111, 7),
        U(core::BookSide::Bid, 100, 9),
        U(core::BookSide::Bid, 110, 0),
        U(core::BookSide::Bid, 120, 1),
        U(core::BookSide::Ask, 111, 0),
        U(core::BookSide::Ask, 130, 2),
    });
    TypeParam batch_model = this->make_model();
    batch_model.apply_updates(batch);
    for (const auto& update : batch) {
        ref.apply_level(update.side, update.price, update.quantity);
    }
    ExpectModelMatchesReference(batch_model, ref);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TYPED_TEST(Phase8ConformanceTest, ReplaceAll) {
    TypeParam model = this->make_model();

    // Replace on empty book.
    model.replace_all(Levels({L(10, 1), L(20, 2)}), Levels({L(11, 3)}));
    EXPECT_EQ(model.all_levels(core::BookSide::Bid), Levels({L(20, 2), L(10, 1)}));
    EXPECT_EQ(model.all_levels(core::BookSide::Ask), Levels({L(11, 3)}));

    // Duplicate price: last wins.
    model.replace_all(Levels({L(10, 1), L(10, 5)}), Levels({}));
    EXPECT_EQ(model.quantity_at(core::BookSide::Bid, P(10)), Q(5));

    // Zero deletes an earlier same-price entry within the same call.
    model.replace_all(Levels({L(30, 4), L(30, 0)}), Levels({}));
    EXPECT_FALSE(model.quantity_at(core::BookSide::Bid, P(30)).has_value());

    // Later nonzero after zero reinserts.
    model.replace_all(Levels({L(40, 0), L(40, 6)}), Levels({}));
    EXPECT_EQ(model.quantity_at(core::BookSide::Bid, P(40)), Q(6));

    // Bid/ask independence.
    model.replace_all(Levels({L(1, 10)}), Levels({L(2, 20)}));
    EXPECT_EQ(model.quantity_at(core::BookSide::Bid, P(1)), Q(10));
    EXPECT_EQ(model.quantity_at(core::BookSide::Ask, P(2)), Q(20));
    EXPECT_EQ(model.level_count(core::BookSide::Bid), 1U);
    EXPECT_EQ(model.level_count(core::BookSide::Ask), 1U);

    // Old state replaced exactly (including already present entries removed).
    TypeParam populated = this->make_model();
    populated.apply_level(core::BookSide::Bid, P(500), Q(1));
    populated.apply_level(core::BookSide::Ask, P(501), Q(2));
    populated.replace_all(Levels({L(10, 1), L(20, 2)}), Levels({L(11, 3), L(12, 4)}));
    EXPECT_EQ(populated.all_levels(core::BookSide::Bid), Levels({L(20, 2), L(10, 1)}));
    EXPECT_EQ(populated.all_levels(core::BookSide::Ask), Levels({L(11, 3), L(12, 4)}));
    EXPECT_FALSE(populated.quantity_at(core::BookSide::Bid, P(500)).has_value());
    EXPECT_FALSE(populated.quantity_at(core::BookSide::Ask, P(501)).has_value());

    // Whole-call equivalence with core::OrderBook (sequence processed in order).
    core::OrderBook book{TestSpec()};
    const auto bids = Levels({L(1, 1), L(2, 2), L(2, 0), L(3, 5), L(1, 9)});
    const auto asks = Levels({L(4, 1), L(5, 2), L(5, 0), L(6, 3)});
    TypeParam compare = this->make_model();
    compare.replace_all(bids, asks);
    book.replace_all(bids, asks);
    ExpectModelMatchesCore(compare, book);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TYPED_TEST(Phase8ConformanceTest, Clear) {
    TypeParam model = this->make_model();
    model.apply_level(core::BookSide::Bid, P(10), Q(1));
    model.apply_level(core::BookSide::Ask, P(11), Q(1));
    model.apply_level(core::BookSide::Bid, P(100), Q(2));
    model.apply_level(core::BookSide::Ask, P(101), Q(2));

    model.clear_side(core::BookSide::Bid);
    EXPECT_EQ(model.level_count(core::BookSide::Bid), 0U);
    EXPECT_EQ(model.level_count(core::BookSide::Ask), 2U);
    EXPECT_FALSE(model.best_bid().has_value());
    EXPECT_TRUE(model.best_ask().has_value());

    model.clear_side(core::BookSide::Ask);
    EXPECT_TRUE(model.empty());

    model.apply_level(core::BookSide::Bid, P(10), Q(1));
    model.apply_level(core::BookSide::Ask, P(11), Q(1));
    model.clear();
    EXPECT_TRUE(model.empty());
    EXPECT_TRUE(model.side_empty(core::BookSide::Bid));
    EXPECT_TRUE(model.side_empty(core::BookSide::Ask));
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TYPED_TEST(Phase8ConformanceTest, Move) {
    static_assert(std::is_nothrow_move_constructible_v<typename TestFixture::model_type>);
    static_assert(std::is_nothrow_move_assignable_v<typename TestFixture::model_type>);
    static_assert(!std::is_copy_constructible_v<typename TestFixture::model_type>);
    static_assert(!std::is_copy_assignable_v<typename TestFixture::model_type>);

    const auto expected_bids = Levels({L(90, 9), L(20, 2), L(10, 1)});
    const auto expected_asks = Levels({L(11, 1), L(21, 2)});

    // Move construction.
    TypeParam source = this->make_model();
    source.apply_level(core::BookSide::Bid, P(20), Q(2));
    source.apply_level(core::BookSide::Bid, P(10), Q(1));
    source.apply_level(core::BookSide::Bid, P(90), Q(9));
    source.apply_level(core::BookSide::Ask, P(21), Q(2));
    source.apply_level(core::BookSide::Ask, P(11), Q(1));
    TypeParam moved{std::move(source)};
    EXPECT_EQ(moved.numeric_spec(), TestSpec());
    EXPECT_EQ(moved.all_levels(core::BookSide::Bid), expected_bids);
    EXPECT_EQ(moved.all_levels(core::BookSide::Ask), expected_asks);

    // Move assignment.
    TypeParam destination = this->make_model();
    destination = std::move(moved);
    EXPECT_EQ(destination.numeric_spec(), TestSpec());
    EXPECT_EQ(destination.all_levels(core::BookSide::Bid), expected_bids);
    EXPECT_EQ(destination.all_levels(core::BookSide::Ask), expected_asks);
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TYPED_TEST(Phase8ConformanceTest, Determinism) {
    // The same fixed operation stream executed repeatedly must produce an
    // identical ordered state at every checkpoint.
    const auto stream = [](TypeParam& model) {
        model.apply_level(core::BookSide::Bid, P(30), Q(3));
        model.apply_level(core::BookSide::Ask, P(31), Q(4));
        model.apply_updates(Updates({
            U(core::BookSide::Bid, 30, 9),
            U(core::BookSide::Bid, 40, 1),
            U(core::BookSide::Bid, 30, 0),
            U(core::BookSide::Ask, 41, 2),
            U(core::BookSide::Bid, 50, 5),
        }));
        model.apply_level(core::BookSide::Bid, P(10), Q(1));
        model.replace_all(Levels({L(1, 1), L(2, 2)}), Levels({L(3, 3), L(1, 9)}));
        model.clear_side(core::BookSide::Ask);
        model.apply_level(core::BookSide::Ask, P(7), Q(7));
        model.apply_updates(Updates({U(core::BookSide::Bid, 1, 0), U(core::BookSide::Ask, 8, 8)}));
    };

    TypeParam first = this->make_model();
    TypeParam second = this->make_model();
    stream(first);
    stream(second);

    EXPECT_TRUE(first.empty() == second.empty());
    for (const auto side : {core::BookSide::Bid, core::BookSide::Ask}) {
        EXPECT_EQ(first.level_count(side), second.level_count(side));
        EXPECT_EQ(first.all_levels(side), second.all_levels(side));
        EXPECT_EQ(first.top_levels(side, 2), second.top_levels(side, 2));
    }
    EXPECT_EQ(first.best_bid(), second.best_bid());
    EXPECT_EQ(first.best_ask(), second.best_ask());
}