#include "reference_order_book.hpp"
#include "test_helpers.hpp"

#include <binance_market_data/projection/v1/order_book/order_book.hpp>

#include <gtest/gtest.h>

namespace bmd = binance_market_data::projection::v1;

class OrderBookUpdateTest : public ::testing::Test {
  protected:
    bmd::NumericSpec spec_{bmd_test::scale(8), bmd_test::scale(8)};
    bmd::OrderBook book_{spec_};

    void apply_and_verify(bmd::BookSide side, std::int64_t price_raw, std::int64_t quantity_raw,
                          bmd::LevelChange expected_change) {
        const auto price = bmd_test::price_units(price_raw);
        const auto quantity = bmd_test::quantity_units(quantity_raw);
        const auto change = book_.apply_level(side, price, quantity);
        EXPECT_EQ(change, expected_change);
        if (quantity_raw > 0) {
            EXPECT_EQ(book_.quantity_at(side, price).value().value(), quantity_raw);
        } else {
            EXPECT_FALSE(book_.quantity_at(side, price).has_value());
        }
    }
};

TEST_F(OrderBookUpdateTest, BidOrderingDescending) {
    apply_and_verify(bmd::BookSide::Bid, 100, 2, bmd::LevelChange::Inserted);
    apply_and_verify(bmd::BookSide::Bid, 102, 3, bmd::LevelChange::Inserted);
    apply_and_verify(bmd::BookSide::Bid, 99, 1, bmd::LevelChange::Inserted);
    apply_and_verify(bmd::BookSide::Bid, 101, 4, bmd::LevelChange::Inserted);

    const auto levels = book_.all_levels(bmd::BookSide::Bid);
    ASSERT_EQ(levels.size(), 4);
    EXPECT_EQ(levels[0].price.value(), 102);
    EXPECT_EQ(levels[1].price.value(), 101);
    EXPECT_EQ(levels[2].price.value(), 100);
    EXPECT_EQ(levels[3].price.value(), 99);
}

TEST_F(OrderBookUpdateTest, AskOrderingAscending) {
    apply_and_verify(bmd::BookSide::Ask, 102, 3, bmd::LevelChange::Inserted);
    apply_and_verify(bmd::BookSide::Ask, 100, 1, bmd::LevelChange::Inserted);
    apply_and_verify(bmd::BookSide::Ask, 103, 4, bmd::LevelChange::Inserted);
    apply_and_verify(bmd::BookSide::Ask, 101, 2, bmd::LevelChange::Inserted);

    const auto levels = book_.all_levels(bmd::BookSide::Ask);
    ASSERT_EQ(levels.size(), 4);
    EXPECT_EQ(levels[0].price.value(), 100);
    EXPECT_EQ(levels[1].price.value(), 101);
    EXPECT_EQ(levels[2].price.value(), 102);
    EXPECT_EQ(levels[3].price.value(), 103);
}

TEST_F(OrderBookUpdateTest, DeleteBestBidUpdatesBest) {
    apply_and_verify(bmd::BookSide::Bid, 100, 5, bmd::LevelChange::Inserted);
    apply_and_verify(bmd::BookSide::Bid, 101, 3, bmd::LevelChange::Inserted);
    EXPECT_EQ(book_.best_bid().value().price.value(), 101);

    const auto change = book_.apply_level(bmd::BookSide::Bid, bmd_test::price_units(101),
                                          bmd_test::quantity_units(0));
    EXPECT_EQ(change, bmd::LevelChange::Removed);
    EXPECT_EQ(book_.best_bid().value().price.value(), 100);
}

TEST_F(OrderBookUpdateTest, DeleteBestAskUpdatesBest) {
    apply_and_verify(bmd::BookSide::Ask, 100, 5, bmd::LevelChange::Inserted);
    apply_and_verify(bmd::BookSide::Ask, 99, 3, bmd::LevelChange::Inserted);
    EXPECT_EQ(book_.best_ask().value().price.value(), 99);

    static_cast<void>(book_.apply_level(bmd::BookSide::Ask, bmd_test::price_units(99),
                                        bmd_test::quantity_units(0)));
    EXPECT_EQ(book_.best_ask().value().price.value(), 100);
}

TEST_F(OrderBookUpdateTest, UpdateNonBestDoesNotAffectBest) {
    apply_and_verify(bmd::BookSide::Bid, 100, 5, bmd::LevelChange::Inserted);
    apply_and_verify(bmd::BookSide::Bid, 101, 3, bmd::LevelChange::Inserted);
    EXPECT_EQ(book_.best_bid().value().price.value(), 101);

    apply_and_verify(bmd::BookSide::Bid, 100, 10, bmd::LevelChange::Updated);
    EXPECT_EQ(book_.best_bid().value().price.value(), 101);
}

TEST_F(OrderBookUpdateTest, InsertBetterPriceChangesBest) {
    apply_and_verify(bmd::BookSide::Bid, 100, 5, bmd::LevelChange::Inserted);
    EXPECT_EQ(book_.best_bid().value().price.value(), 100);
    apply_and_verify(bmd::BookSide::Bid, 102, 3, bmd::LevelChange::Inserted);
    EXPECT_EQ(book_.best_bid().value().price.value(), 102);
}

TEST_F(OrderBookUpdateTest, QuantityAtReturnsNulloptForMissingPrice) {
    EXPECT_FALSE(book_.quantity_at(bmd::BookSide::Bid, bmd_test::price_units(999)).has_value());
    apply_and_verify(bmd::BookSide::Bid, 100, 5, bmd::LevelChange::Inserted);
    EXPECT_EQ(book_.quantity_at(bmd::BookSide::Bid, bmd_test::price_units(100))->value(), 5);
    EXPECT_FALSE(book_.quantity_at(bmd::BookSide::Bid, bmd_test::price_units(200)).has_value());
}
