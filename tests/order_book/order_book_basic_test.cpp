#include "test_helpers.hpp"

#include <binance_market_data/projection/v1/numeric/numeric_spec.hpp>
#include <binance_market_data/projection/v1/order_book/book_level.hpp>
#include <binance_market_data/projection/v1/order_book/level_update.hpp>
#include <binance_market_data/projection/v1/order_book/order_book.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <type_traits>

// NOLINTBEGIN(bugprone-unchecked-optional-access)
namespace bmd = binance_market_data::projection::v1;

static_assert(std::is_trivially_copyable_v<bmd::BookLevel>);
static_assert(std::is_standard_layout_v<bmd::BookLevel>);
static_assert(std::is_trivially_copyable_v<bmd::LevelUpdate>);
static_assert(std::is_standard_layout_v<bmd::LevelUpdate>);

static_assert(!std::is_copy_constructible_v<bmd::OrderBook>);
static_assert(!std::is_copy_assignable_v<bmd::OrderBook>);
static_assert(std::is_move_constructible_v<bmd::OrderBook>);
static_assert(std::is_move_assignable_v<bmd::OrderBook>);

class OrderBookBasicTest : public ::testing::Test {
    // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes)
  protected:
    bmd::NumericSpec spec_{bmd_test::scale(8), bmd_test::scale(8)};
    bmd::OrderBook book_{spec_};
};

TEST_F(OrderBookBasicTest, ConstructionPreservesNumericSpec) {
    EXPECT_EQ(book_.numeric_spec().price_scale.value(), 8);
    EXPECT_EQ(book_.numeric_spec().quantity_scale.value(), 8);
}

TEST_F(OrderBookBasicTest, InitiallyEmpty) {
    EXPECT_TRUE(book_.empty());
    EXPECT_TRUE(book_.side_empty(bmd::BookSide::Bid));
    EXPECT_TRUE(book_.side_empty(bmd::BookSide::Ask));
    EXPECT_EQ(book_.level_count(bmd::BookSide::Bid), 0);
    EXPECT_EQ(book_.level_count(bmd::BookSide::Ask), 0);
    EXPECT_FALSE(book_.best_bid().has_value());
    EXPECT_FALSE(book_.best_ask().has_value());
}

TEST_F(OrderBookBasicTest, InsertBidProducesInsertedChange) {
    const auto change = book_.apply_level(bmd::BookSide::Bid, bmd_test::price_units(100),
                                          bmd_test::quantity_units(5));
    EXPECT_EQ(change, bmd::LevelChange::Inserted);
}

TEST_F(OrderBookBasicTest, InsertAskProducesInsertedChange) {
    const auto change = book_.apply_level(bmd::BookSide::Ask, bmd_test::price_units(101),
                                          bmd_test::quantity_units(7));
    EXPECT_EQ(change, bmd::LevelChange::Inserted);
}

TEST_F(OrderBookBasicTest, InsertBidIncreasesCountAndSetsBest) {
    static_cast<void>(book_.apply_level(bmd::BookSide::Bid, bmd_test::price_units(100),
                                        bmd_test::quantity_units(5)));
    EXPECT_EQ(book_.level_count(bmd::BookSide::Bid), 1);
    EXPECT_EQ(book_.best_bid().value().price, bmd_test::price_units(100));
    EXPECT_EQ(book_.best_bid().value().quantity, bmd_test::quantity_units(5));
    EXPECT_EQ(book_.quantity_at(bmd::BookSide::Bid, bmd_test::price_units(100))->value(), 5);
}

TEST_F(OrderBookBasicTest, InsertAskIncreasesCountAndSetsBest) {
    static_cast<void>(book_.apply_level(bmd::BookSide::Ask, bmd_test::price_units(101),
                                        bmd_test::quantity_units(7)));
    EXPECT_EQ(book_.level_count(bmd::BookSide::Ask), 1);
    EXPECT_EQ(book_.best_ask().value().price, bmd_test::price_units(101));
    EXPECT_EQ(book_.best_ask().value().quantity, bmd_test::quantity_units(7));
}

TEST_F(OrderBookBasicTest, UpdateExistingBidReturnsUpdated) {
    static_cast<void>(book_.apply_level(bmd::BookSide::Bid, bmd_test::price_units(100),
                                        bmd_test::quantity_units(5)));
    const auto change = book_.apply_level(bmd::BookSide::Bid, bmd_test::price_units(100),
                                          bmd_test::quantity_units(8));
    EXPECT_EQ(change, bmd::LevelChange::Updated);
    EXPECT_EQ(book_.level_count(bmd::BookSide::Bid), 1);
    EXPECT_EQ(book_.quantity_at(bmd::BookSide::Bid, bmd_test::price_units(100))->value(), 8);
}

TEST_F(OrderBookBasicTest, SameQuantityReturnsUnchanged) {
    static_cast<void>(book_.apply_level(bmd::BookSide::Bid, bmd_test::price_units(100),
                                        bmd_test::quantity_units(5)));
    const auto change = book_.apply_level(bmd::BookSide::Bid, bmd_test::price_units(100),
                                          bmd_test::quantity_units(5));
    EXPECT_EQ(change, bmd::LevelChange::Unchanged);
}

TEST_F(OrderBookBasicTest, RemoveExistingLevelReturnsRemoved) {
    static_cast<void>(book_.apply_level(bmd::BookSide::Bid, bmd_test::price_units(100),
                                        bmd_test::quantity_units(5)));
    const auto change = book_.apply_level(bmd::BookSide::Bid, bmd_test::price_units(100),
                                          bmd_test::quantity_units(0));
    EXPECT_EQ(change, bmd::LevelChange::Removed);
    EXPECT_EQ(book_.level_count(bmd::BookSide::Bid), 0);
    EXPECT_FALSE(book_.best_bid().has_value());
}

TEST_F(OrderBookBasicTest, RemoveNonExistentIsUnchanged) {
    const auto change = book_.apply_level(bmd::BookSide::Bid, bmd_test::price_units(100),
                                          bmd_test::quantity_units(0));
    EXPECT_EQ(change, bmd::LevelChange::Unchanged);
}

TEST_F(OrderBookBasicTest, OrderBookMoveConstructPreservesState) {
    static_cast<void>(book_.apply_level(bmd::BookSide::Bid, bmd_test::price_units(100),
                                        bmd_test::quantity_units(5)));
    bmd::OrderBook moved{std::move(book_)};
    EXPECT_EQ(moved.level_count(bmd::BookSide::Bid), 1);
    EXPECT_EQ(moved.best_bid().value().price, bmd_test::price_units(100));
}

TEST_F(OrderBookBasicTest, MoveAssignPreservesState) {
    bmd::OrderBook other{bmd::NumericSpec{bmd_test::scale(8), bmd_test::scale(8)}};
    static_cast<void>(other.apply_level(bmd::BookSide::Bid, bmd_test::price_units(200),
                                        bmd_test::quantity_units(10)));
    book_ = std::move(other);
    EXPECT_EQ(book_.best_bid().value().price, bmd_test::price_units(200));
}

TEST_F(OrderBookBasicTest, ZeroInsertDoesNotStore) {
    const auto change = book_.apply_level(bmd::BookSide::Bid, bmd_test::price_units(100),
                                          bmd_test::quantity_units(0));
    EXPECT_EQ(change, bmd::LevelChange::Unchanged);
    EXPECT_EQ(book_.level_count(bmd::BookSide::Bid), 0);
}

TEST_F(OrderBookBasicTest, BoundaryValues) {
    const auto max_price = bmd_test::price_units(std::numeric_limits<std::int64_t>::max());
    const auto min_price = bmd_test::price_units(1);
    const auto max_qty = bmd_test::quantity_units(std::numeric_limits<std::int64_t>::max());

    static_cast<void>(
        book_.apply_level(bmd::BookSide::Bid, max_price, bmd_test::quantity_units(1)));
    static_cast<void>(book_.apply_level(bmd::BookSide::Bid, min_price, max_qty));
    EXPECT_EQ(book_.level_count(bmd::BookSide::Bid), 2);
    EXPECT_EQ(book_.best_bid().value().price, max_price);
}

TEST_F(OrderBookBasicTest, ToStringReturnsExpectedLevelChanges) {
    EXPECT_EQ(to_string(bmd::LevelChange::Inserted), "INSERTED");
    EXPECT_EQ(to_string(bmd::LevelChange::Updated), "UPDATED");
    EXPECT_EQ(to_string(bmd::LevelChange::Removed), "REMOVED");
    EXPECT_EQ(to_string(bmd::LevelChange::Unchanged), "UNCHANGED");
}

TEST_F(OrderBookBasicTest, NumericSpec_0_And_18_Boundary) {
    bmd::NumericSpec wide_spec{bmd_test::scale(0), bmd_test::scale(18)};
    bmd::OrderBook wide_book{wide_spec};
    EXPECT_EQ(wide_book.numeric_spec().price_scale.value(), 0);
    EXPECT_EQ(wide_book.numeric_spec().quantity_scale.value(), 18);
    static_cast<void>(wide_book.apply_level(bmd::BookSide::Ask, bmd_test::price_units(500),
                                            bmd_test::quantity_units(1000)));
    EXPECT_EQ(wide_book.level_count(bmd::BookSide::Ask), 1);
}

// NOLINTEND(bugprone-unchecked-optional-access)
