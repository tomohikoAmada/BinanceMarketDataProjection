#include <binance_market_data/projection/v1/version.hpp>

#include <gtest/gtest.h>

TEST(PublicHeaderTest, PublicConstantsAreUsable) {
    EXPECT_FALSE(binance_market_data::projection::v1::kProjectName.empty());
}
