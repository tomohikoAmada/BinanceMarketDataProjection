#include "test_helpers.hpp"

#include <binance_market_data/projection/v1/order_book/order_book.hpp>

#include <gtest/gtest.h>

// NOLINTBEGIN(bugprone-unchecked-optional-access)
namespace bmd = binance_market_data::projection::v1;

class OrderBookCrossedTest : public ::testing::Test {
    // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes)
  protected:
    bmd::NumericSpec spec_{bmd_test::scale(8), bmd_test::scale(8)};
    bmd::OrderBook book_{spec_};
};

TEST_F(OrderBookCrossedTest, LockedBookIsAccepted) {
    static_cast<void>(book_.apply_level(bmd::BookSide::Bid, bmd_test::price_units(100),
                                        bmd_test::quantity_units(2)));
    static_cast<void>(book_.apply_level(bmd::BookSide::Ask, bmd_test::price_units(100),
                                        bmd_test::quantity_units(3)));

    EXPECT_EQ(book_.best_bid().value().price, bmd_test::price_units(100));
    EXPECT_EQ(book_.best_ask().value().price, bmd_test::price_units(100));
    EXPECT_EQ(book_.level_count(bmd::BookSide::Bid), 1);
    EXPECT_EQ(book_.level_count(bmd::BookSide::Ask), 1);
}

TEST_F(OrderBookCrossedTest, CrossedBookIsAccepted) {
    static_cast<void>(book_.apply_level(bmd::BookSide::Bid, bmd_test::price_units(101),
                                        bmd_test::quantity_units(2)));
    static_cast<void>(book_.apply_level(bmd::BookSide::Ask, bmd_test::price_units(100),
                                        bmd_test::quantity_units(3)));

    EXPECT_EQ(book_.best_bid().value().price, bmd_test::price_units(101));
    EXPECT_EQ(book_.best_ask().value().price, bmd_test::price_units(100));
    EXPECT_GT(book_.best_bid().value().price.value(), book_.best_ask().value().price.value());
    EXPECT_EQ(book_.level_count(bmd::BookSide::Bid), 1);
    EXPECT_EQ(book_.level_count(bmd::BookSide::Ask), 1);
}

TEST_F(OrderBookCrossedTest, CrossedAfterUpdateIsAccepted) {
    static_cast<void>(book_.apply_level(bmd::BookSide::Bid, bmd_test::price_units(100),
                                        bmd_test::quantity_units(2)));
    static_cast<void>(book_.apply_level(bmd::BookSide::Ask, bmd_test::price_units(101),
                                        bmd_test::quantity_units(3)));
    EXPECT_LT(book_.best_bid().value().price.value(), book_.best_ask().value().price.value());

    static_cast<void>(book_.apply_level(bmd::BookSide::Bid, bmd_test::price_units(102),
                                        bmd_test::quantity_units(2)));
    EXPECT_GT(book_.best_bid().value().price.value(), book_.best_ask().value().price.value());
}

// NOLINTEND(bugprone-unchecked-optional-access)
