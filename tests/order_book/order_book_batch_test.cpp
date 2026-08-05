#include "test_helpers.hpp"

#include <binance_market_data/projection/v1/order_book/order_book.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>

namespace bmd = binance_market_data::projection::v1;

class OrderBookBatchTest : public ::testing::Test {
  protected:
    bmd::NumericSpec spec_{bmd_test::scale(8), bmd_test::scale(8)};
    bmd::OrderBook book_{spec_};
};

TEST_F(OrderBookBatchTest, EmptyBatchIsNoOp) {
    const std::array<bmd::LevelUpdate, 0> updates{};
    book_.apply_updates(updates);
    EXPECT_TRUE(book_.empty());
}

TEST_F(OrderBookBatchTest, SingleUpdateWorks) {
    const std::array updates = {
        bmd::LevelUpdate{bmd::BookSide::Bid, bmd_test::price_units(100),
                         bmd_test::quantity_units(5)},
    };
    book_.apply_updates(updates);
    EXPECT_EQ(book_.level_count(bmd::BookSide::Bid), 1);
    EXPECT_EQ(book_.best_bid().value().price.value(), 100);
}

TEST_F(OrderBookBatchTest, MultiSideBatchUpdatesBothSides) {
    const std::array updates = {
        bmd::LevelUpdate{bmd::BookSide::Bid, bmd_test::price_units(100),
                         bmd_test::quantity_units(2)},
        bmd::LevelUpdate{bmd::BookSide::Ask, bmd_test::price_units(101),
                         bmd_test::quantity_units(3)},
        bmd::LevelUpdate{bmd::BookSide::Bid, bmd_test::price_units(99),
                         bmd_test::quantity_units(1)},
    };
    book_.apply_updates(updates);
    EXPECT_EQ(book_.level_count(bmd::BookSide::Bid), 2);
    EXPECT_EQ(book_.level_count(bmd::BookSide::Ask), 1);
}

TEST_F(OrderBookBatchTest, DuplicatePriceLastWins) {
    const std::array updates = {
        bmd::LevelUpdate{bmd::BookSide::Bid, bmd_test::price_units(100),
                         bmd_test::quantity_units(2)},
        bmd::LevelUpdate{bmd::BookSide::Bid, bmd_test::price_units(100),
                         bmd_test::quantity_units(8)},
    };
    book_.apply_updates(updates);
    EXPECT_EQ(book_.level_count(bmd::BookSide::Bid), 1);
    EXPECT_EQ(book_.quantity_at(bmd::BookSide::Bid, bmd_test::price_units(100))->value(), 8);
}

TEST_F(OrderBookBatchTest, InsertThenDelete) {
    const std::array updates = {
        bmd::LevelUpdate{bmd::BookSide::Bid, bmd_test::price_units(100),
                         bmd_test::quantity_units(5)},
        bmd::LevelUpdate{bmd::BookSide::Bid, bmd_test::price_units(100),
                         bmd_test::quantity_units(0)},
    };
    book_.apply_updates(updates);
    EXPECT_EQ(book_.level_count(bmd::BookSide::Bid), 0);
}

TEST_F(OrderBookBatchTest, DeleteThenInsert) {
    const std::array updates = {
        bmd::LevelUpdate{bmd::BookSide::Bid, bmd_test::price_units(100),
                         bmd_test::quantity_units(0)},
        bmd::LevelUpdate{bmd::BookSide::Bid, bmd_test::price_units(100),
                         bmd_test::quantity_units(5)},
    };
    book_.apply_updates(updates);
    EXPECT_EQ(book_.level_count(bmd::BookSide::Bid), 1);
    EXPECT_EQ(book_.quantity_at(bmd::BookSide::Bid, bmd_test::price_units(100))->value(), 5);
}

TEST_F(OrderBookBatchTest, BatchEquivalentToSequential) {
    auto book2 = bmd::OrderBook{spec_};
    const std::array updates = {
        bmd::LevelUpdate{bmd::BookSide::Bid, bmd_test::price_units(100),
                         bmd_test::quantity_units(2)},
        bmd::LevelUpdate{bmd::BookSide::Bid, bmd_test::price_units(102),
                         bmd_test::quantity_units(3)},
        bmd::LevelUpdate{bmd::BookSide::Ask, bmd_test::price_units(101),
                         bmd_test::quantity_units(4)},
    };

    book_.apply_updates(updates);
    for (const auto& u : updates) {
        static_cast<void>(book2.apply_level(u.side, u.price, u.quantity));
    }

    EXPECT_EQ(book_.all_levels(bmd::BookSide::Bid), book2.all_levels(bmd::BookSide::Bid));
    EXPECT_EQ(book_.all_levels(bmd::BookSide::Ask), book2.all_levels(bmd::BookSide::Ask));
}
