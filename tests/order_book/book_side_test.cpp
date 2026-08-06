#include "test_helpers.hpp"

#include <binance_market_data/projection/v1/order_book/book_side.hpp>

#include <gtest/gtest.h>

#include <string>

namespace bmd = binance_market_data::projection::v1;

static_assert(std::is_same_v<std::underlying_type_t<bmd::BookSide>, std::uint8_t>);

TEST(BookSideTest, ToStringReturnsExpectedValues) {
    EXPECT_EQ(to_string(bmd::BookSide::Bid), "BID");
    EXPECT_EQ(to_string(bmd::BookSide::Ask), "ASK");
}

TEST(BookSideTest, BidAndAskAreDistinct) {
    EXPECT_NE(static_cast<std::uint8_t>(bmd::BookSide::Bid),
              static_cast<std::uint8_t>(bmd::BookSide::Ask));
}
