#include "test_helpers.hpp"

#include <binance_market_data/projection/v1/order_book/book_level.hpp>
#include <binance_market_data/projection/v1/order_book/level_update.hpp>
#include <binance_market_data/projection/v1/order_book/order_book.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

// NOLINTBEGIN(bugprone-unchecked-optional-access)
namespace bmd = binance_market_data::projection::v1;

class OrderBookDeterminismTest : public ::testing::Test {
  protected:
    // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes)
    bmd::NumericSpec spec_{bmd_test::scale(8), bmd_test::scale(8)};

    using Transcript = std::vector<bmd::LevelUpdate>;

    // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes)
    Transcript build_transcript() {
        // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes)
        return {
            // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes)
            {bmd::BookSide::Bid, bmd_test::price_units(100), bmd_test::quantity_units(5)},
            // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes)
            {bmd::BookSide::Bid, bmd_test::price_units(102), bmd_test::quantity_units(3)},
            // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes)
            {bmd::BookSide::Ask, bmd_test::price_units(200), bmd_test::quantity_units(7)},
            // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes)
            {bmd::BookSide::Bid, bmd_test::price_units(99), bmd_test::quantity_units(1)},
            // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes)
            {bmd::BookSide::Ask, bmd_test::price_units(201), bmd_test::quantity_units(2)},
            // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes)
            {bmd::BookSide::Bid, bmd_test::price_units(102), bmd_test::quantity_units(0)},
            // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes)
            {bmd::BookSide::Bid, bmd_test::price_units(100), bmd_test::quantity_units(8)},
        };
    }
};

TEST_F(OrderBookDeterminismTest, IdenticalTranscriptsProduceIdenticalResults) {
    const auto transcript = build_transcript();

    bmd::OrderBook book1{spec_};
    bmd::OrderBook book2{spec_};

    for (const auto& update : transcript) {
        static_cast<void>(book1.apply_level(update.side, update.price, update.quantity));
        static_cast<void>(book2.apply_level(update.side, update.price, update.quantity));
    }

    EXPECT_EQ(book1.all_levels(bmd::BookSide::Bid), book2.all_levels(bmd::BookSide::Bid));
    EXPECT_EQ(book1.all_levels(bmd::BookSide::Ask), book2.all_levels(bmd::BookSide::Ask));
    EXPECT_EQ(book1.level_count(bmd::BookSide::Bid), book2.level_count(bmd::BookSide::Bid));
    EXPECT_EQ(book1.best_bid().value(), book2.best_bid().value());
    EXPECT_EQ(book1.best_ask().value(), book2.best_ask().value());
}

TEST_F(OrderBookDeterminismTest, CheckpointsAreIdentical) {
    const auto transcript = build_transcript();
    bmd::OrderBook book1{spec_};
    bmd::OrderBook book2{spec_};

    for (const auto& update : transcript) {
        static_cast<void>(book1.apply_level(update.side, update.price, update.quantity));
        static_cast<void>(book2.apply_level(update.side, update.price, update.quantity));

        EXPECT_EQ(book1.all_levels(bmd::BookSide::Bid), book2.all_levels(bmd::BookSide::Bid));
        EXPECT_EQ(book1.all_levels(bmd::BookSide::Ask), book2.all_levels(bmd::BookSide::Ask));
    }
}

// NOLINTEND(bugprone-unchecked-optional-access)
