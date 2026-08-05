#include "test_helpers.hpp"

#include <binance_market_data/projection/v1/numeric/decimal_parse.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <variant>

namespace bmd = binance_market_data::projection::v1;

TEST(DecimalOverflowTest, RejectsIntegerImmediatelyAboveMaximum) {
    bmd_test::expect_error(bmd::parse_quantity("9223372036854775808", bmd_test::scale(0)),
                           bmd::DecimalErrorCode::Overflow, 18);
}

TEST(DecimalOverflowTest, AcceptsMaximumWithDiscardableFractionalZero) {
    const auto result = bmd::parse_quantity("9223372036854775807.0", bmd_test::scale(0));
    ASSERT_TRUE(std::holds_alternative<bmd::ParsedDecimal<bmd::QuantityUnits>>(result));
    EXPECT_EQ(std::get<bmd::ParsedDecimal<bmd::QuantityUnits>>(result).value.value(),
              std::numeric_limits<std::int64_t>::max());
}

TEST(DecimalOverflowTest, InexactScalePrecedesOverflow) {
    bmd_test::expect_error(bmd::parse_quantity("9223372036854775807.1", bmd_test::scale(0)),
                           bmd::DecimalErrorCode::InexactScale, 20);
}

TEST(DecimalOverflowTest, RejectsOverflowDuringFinalScalePadding) {
    bmd_test::expect_error(bmd::parse_quantity("10", bmd_test::scale(18)),
                           bmd::DecimalErrorCode::Overflow, bmd::kNoErrorOffset);
}

TEST(DecimalOverflowTest, HandlesScaleEighteenBoundaryExactly) {
    const auto maximum = bmd::parse_quantity("9.223372036854775807", bmd_test::scale(18));
    ASSERT_TRUE(std::holds_alternative<bmd::ParsedDecimal<bmd::QuantityUnits>>(maximum));
    EXPECT_EQ(std::get<bmd::ParsedDecimal<bmd::QuantityUnits>>(maximum).value.value(),
              std::numeric_limits<std::int64_t>::max());
    bmd_test::expect_error(bmd::parse_quantity("9.223372036854775808", bmd_test::scale(18)),
                           bmd::DecimalErrorCode::Overflow, 19);
}

TEST(DecimalOverflowTest, RejectsNonzeroDigitsDiscardedAtScaleZero) {
    bmd_test::expect_error(bmd::parse_quantity("1.000000000000000001", bmd_test::scale(0)),
                           bmd::DecimalErrorCode::InexactScale, 19);
}
