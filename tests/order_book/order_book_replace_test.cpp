#include "test_helpers.hpp"

#include <binance_market_data/projection/v1/order_book/order_book.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <vector>

namespace bmd = binance_market_data::projection::v1;

class OrderBookReplaceTest : public ::testing::Test {
  protected:
    bmd::NumericSpec spec_{bmd_test::scale(8), bmd_test::scale(8)};
    bmd::OrderBook book_{spec_};
};

TEST_F(OrderBookReplaceTest, ReplaceAllSetsNewState) {
    static_cast<void>(book_.apply_level(bmd::BookSide::Bid, bmd_test::price_units(100),
                                        bmd_test::quantity_units(5)));
    static_cast<void>(book_.apply_level(bmd::BookSide::Ask, bmd_test::price_units(200),
                                        bmd_test::quantity_units(3)));

    const std::array new_bids = {
        bmd::BookLevel{bmd_test::price_units(101), bmd_test::quantity_units(7)},
    };
    const std::array new_asks = {
        bmd::BookLevel{bmd_test::price_units(201), bmd_test::quantity_units(9)},
    };
    book_.replace_all(new_bids, new_asks);

    EXPECT_EQ(book_.level_count(bmd::BookSide::Bid), 1);
    EXPECT_EQ(book_.level_count(bmd::BookSide::Ask), 1);
    EXPECT_EQ(book_.best_bid().value().price, bmd_test::price_units(101));
    EXPECT_EQ(book_.best_ask().value().price, bmd_test::price_units(201));
}

TEST_F(OrderBookReplaceTest, ReplaceAllClearsWhenEmpty) {
    static_cast<void>(book_.apply_level(bmd::BookSide::Bid, bmd_test::price_units(100),
                                        bmd_test::quantity_units(5)));
    static_cast<void>(book_.apply_level(bmd::BookSide::Ask, bmd_test::price_units(200),
                                        bmd_test::quantity_units(3)));

    const std::vector<bmd::BookLevel> empty;
    book_.replace_all(empty, empty);

    EXPECT_TRUE(book_.empty());
}

TEST_F(OrderBookReplaceTest, ReplaceBidOnlyPreservesAsk) {
    static_cast<void>(book_.apply_level(bmd::BookSide::Ask, bmd_test::price_units(200),
                                        bmd_test::quantity_units(3)));

    const std::array new_bids = {
        bmd::BookLevel{bmd_test::price_units(100), bmd_test::quantity_units(5)},
    };
    const std::vector<bmd::BookLevel> empty_asks;
    book_.replace_all(new_bids, empty_asks);

    EXPECT_EQ(book_.level_count(bmd::BookSide::Bid), 1);
    EXPECT_EQ(book_.level_count(bmd::BookSide::Ask), 0);
}

TEST_F(OrderBookReplaceTest, UnorderedInputSortedOnOutput) {
    const std::array bids = {
        bmd::BookLevel{bmd_test::price_units(99), bmd_test::quantity_units(1)},
        bmd::BookLevel{bmd_test::price_units(102), bmd_test::quantity_units(3)},
        bmd::BookLevel{bmd_test::price_units(100), bmd_test::quantity_units(2)},
    };
    const std::vector<bmd::BookLevel> empty;
    book_.replace_all(bids, empty);

    const auto levels = book_.all_levels(bmd::BookSide::Bid);
    ASSERT_EQ(levels.size(), 3);
    EXPECT_EQ(levels[0].price.value(), 102);
    EXPECT_EQ(levels[1].price.value(), 100);
    EXPECT_EQ(levels[2].price.value(), 99);
}

TEST_F(OrderBookReplaceTest, DuplicatePriceLastWins) {
    const std::array bids = {
        bmd::BookLevel{bmd_test::price_units(100), bmd_test::quantity_units(2)},
        bmd::BookLevel{bmd_test::price_units(100), bmd_test::quantity_units(8)},
    };
    const std::vector<bmd::BookLevel> empty;
    book_.replace_all(bids, empty);

    EXPECT_EQ(book_.level_count(bmd::BookSide::Bid), 1);
    EXPECT_EQ(book_.quantity_at(bmd::BookSide::Bid, bmd_test::price_units(100))->value(), 8);
}

TEST_F(OrderBookReplaceTest, PositiveThenZeroEliminates) {
    const std::array bids = {
        bmd::BookLevel{bmd_test::price_units(100), bmd_test::quantity_units(5)},
        bmd::BookLevel{bmd_test::price_units(100), bmd_test::quantity_units(0)},
    };
    const std::vector<bmd::BookLevel> empty;
    book_.replace_all(bids, empty);

    EXPECT_EQ(book_.level_count(bmd::BookSide::Bid), 0);
}

TEST_F(OrderBookReplaceTest, ZeroThenPositiveExists) {
    const std::array bids = {
        bmd::BookLevel{bmd_test::price_units(100), bmd_test::quantity_units(0)},
        bmd::BookLevel{bmd_test::price_units(100), bmd_test::quantity_units(5)},
    };
    const std::vector<bmd::BookLevel> empty;
    book_.replace_all(bids, empty);

    EXPECT_EQ(book_.level_count(bmd::BookSide::Bid), 1);
    EXPECT_EQ(book_.quantity_at(bmd::BookSide::Bid, bmd_test::price_units(100))->value(), 5);
}

TEST_F(OrderBookReplaceTest, SamePriceBothSidesPreserved) {
    const std::array bids = {
        bmd::BookLevel{bmd_test::price_units(100), bmd_test::quantity_units(2)},
    };
    const std::array asks = {
        bmd::BookLevel{bmd_test::price_units(100), bmd_test::quantity_units(3)},
    };
    book_.replace_all(bids, asks);

    EXPECT_EQ(book_.best_bid().value().price, bmd_test::price_units(100));
    EXPECT_EQ(book_.best_ask().value().price, bmd_test::price_units(100));
    EXPECT_EQ(book_.best_bid().value().quantity.value(), 2);
    EXPECT_EQ(book_.best_ask().value().quantity.value(), 3);
}

TEST_F(OrderBookReplaceTest, NumericSpecUnchanged) {
    const std::array bids = {
        bmd::BookLevel{bmd_test::price_units(100), bmd_test::quantity_units(1)},
    };
    const std::vector<bmd::BookLevel> empty;
    book_.replace_all(bids, empty);
    EXPECT_EQ(book_.numeric_spec().price_scale.value(), 8);
    EXPECT_EQ(book_.numeric_spec().quantity_scale.value(), 8);
}
