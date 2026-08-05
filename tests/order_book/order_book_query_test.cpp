#include "test_helpers.hpp"

#include <binance_market_data/projection/v1/order_book/order_book.hpp>

#include <gtest/gtest.h>

#include <cstdint>

namespace bmd = binance_market_data::projection::v1;

class OrderBookQueryTest : public ::testing::Test {
  protected:
    bmd::NumericSpec spec_{bmd_test::scale(8), bmd_test::scale(8)};
    bmd::OrderBook book_{spec_};

    void SetUp() override {
        static_cast<void>(book_.apply_level(bmd::BookSide::Bid, bmd_test::price_units(102),
                                            bmd_test::quantity_units(3)));
        static_cast<void>(book_.apply_level(bmd::BookSide::Bid, bmd_test::price_units(101),
                                            bmd_test::quantity_units(5)));
        static_cast<void>(book_.apply_level(bmd::BookSide::Bid, bmd_test::price_units(100),
                                            bmd_test::quantity_units(2)));
        static_cast<void>(book_.apply_level(bmd::BookSide::Ask, bmd_test::price_units(200),
                                            bmd_test::quantity_units(4)));
        static_cast<void>(book_.apply_level(bmd::BookSide::Ask, bmd_test::price_units(201),
                                            bmd_test::quantity_units(6)));
        static_cast<void>(book_.apply_level(bmd::BookSide::Ask, bmd_test::price_units(202),
                                            bmd_test::quantity_units(1)));
    }
};

TEST_F(OrderBookQueryTest, TopN_LimitZero_ReturnsEmpty) {
    const auto result = book_.top_levels(bmd::BookSide::Bid, 0);
    EXPECT_TRUE(result.empty());
}

TEST_F(OrderBookQueryTest, TopN_LimitOne_ReturnsFirst) {
    const auto result = book_.top_levels(bmd::BookSide::Bid, 1);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].price.value(), 102);
}

TEST_F(OrderBookQueryTest, TopN_LimitLessThanCount) {
    const auto result = book_.top_levels(bmd::BookSide::Bid, 2);
    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].price.value(), 102);
    EXPECT_EQ(result[1].price.value(), 101);
}

TEST_F(OrderBookQueryTest, TopN_LimitEqualsCount) {
    const auto result = book_.top_levels(bmd::BookSide::Bid, 3);
    ASSERT_EQ(result.size(), 3);
    EXPECT_EQ(result[0].price.value(), 102);
    EXPECT_EQ(result[2].price.value(), 100);
}

TEST_F(OrderBookQueryTest, TopN_LimitExceedsCount) {
    const auto result = book_.top_levels(bmd::BookSide::Bid, 100);
    ASSERT_EQ(result.size(), 3);
}

TEST_F(OrderBookQueryTest, AllLevelsReturnsFullOrderedCopy) {
    const auto bids = book_.all_levels(bmd::BookSide::Bid);
    ASSERT_EQ(bids.size(), 3);
    EXPECT_GT(bids[0].price.value(), bids[1].price.value());
    EXPECT_GT(bids[1].price.value(), bids[2].price.value());

    const auto asks = book_.all_levels(bmd::BookSide::Ask);
    ASSERT_EQ(asks.size(), 3);
    EXPECT_LT(asks[0].price.value(), asks[1].price.value());
    EXPECT_LT(asks[1].price.value(), asks[2].price.value());
}

TEST_F(OrderBookQueryTest, TopN_AsksAscending) {
    const auto result = book_.top_levels(bmd::BookSide::Ask, 2);
    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].price.value(), 200);
    EXPECT_EQ(result[1].price.value(), 201);
}

TEST_F(OrderBookQueryTest, ClearSideDoesNotAffectOther) {
    book_.clear_side(bmd::BookSide::Bid);
    EXPECT_EQ(book_.level_count(bmd::BookSide::Bid), 0);
    EXPECT_EQ(book_.level_count(bmd::BookSide::Ask), 3);
}

TEST_F(OrderBookQueryTest, ClearEntireBook) {
    book_.clear();
    EXPECT_TRUE(book_.empty());
    EXPECT_EQ(book_.level_count(bmd::BookSide::Bid), 0);
    EXPECT_EQ(book_.level_count(bmd::BookSide::Ask), 0);
}

TEST_F(OrderBookQueryTest, ClearEmptyBookIsNoOp) {
    book_.clear();
    book_.clear();
    EXPECT_TRUE(book_.empty());
}
