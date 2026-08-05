#include "test_helpers.hpp"

#include <binance_market_data/projection/v1/numeric/decimal_format.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <variant>

namespace bmd = binance_market_data::projection::v1;

namespace {

void expect_quantity_format(std::int64_t units, std::uint32_t storage_scale,
                            std::size_t output_scale, const std::string& expected) {
    const auto value = *bmd::QuantityUnits::create(units);
    const auto result = bmd::format_quantity(value, bmd_test::scale(storage_scale), output_scale);
    ASSERT_TRUE(std::holds_alternative<std::string>(result));
    EXPECT_EQ(std::get<std::string>(result), expected);
}

} // namespace

TEST(DecimalFormatTest, FormatsRequiredScaleExamples) {
    expect_quantity_format(123, 2, 2, "1.23");
    expect_quantity_format(123, 2, 4, "1.2300");
    expect_quantity_format(12'300, 4, 2, "1.23");
    expect_quantity_format(100'000'000, 8, 0, "1");
    expect_quantity_format(0, 8, 8, "0.00000000");
}

TEST(DecimalFormatTest, FormatsStorageScaleZeroWithAndWithoutAddedPrecision) {
    expect_quantity_format(123, 0, 0, "123");
    expect_quantity_format(123, 0, 3, "123.000");
}

TEST(DecimalFormatTest, FormatsValuesBelowOneWithLeadingZero) {
    expect_quantity_format(1, 8, 8, "0.00000001");
    expect_quantity_format(1, 18, 18, "0.000000000000000001");
}

TEST(DecimalFormatTest, RejectsInexactDownscaleWithoutRounding) {
    const auto result =
        bmd::format_quantity(*bmd::QuantityUnits::create(12'301), bmd_test::scale(4), 2);
    bmd_test::expect_error(result, bmd::DecimalErrorCode::InexactScale, bmd::kNoErrorOffset);
}

TEST(DecimalFormatTest, FormatsInt64MaximumAtStorageScaleZeroAndEighteen) {
    expect_quantity_format(std::numeric_limits<std::int64_t>::max(), 0, 0, "9223372036854775807");
    expect_quantity_format(std::numeric_limits<std::int64_t>::max(), 18, 18,
                           "9.223372036854775807");
}

TEST(DecimalFormatTest, PriceAndQuantityUseSameExactFormattingRules) {
    const auto price_result =
        bmd::format_price(*bmd::PriceUnits::create(123), bmd_test::scale(2), 4);
    const auto quantity_result =
        bmd::format_quantity(*bmd::QuantityUnits::create(123), bmd_test::scale(2), 4);
    ASSERT_TRUE(std::holds_alternative<std::string>(price_result));
    ASSERT_TRUE(std::holds_alternative<std::string>(quantity_result));
    EXPECT_EQ(std::get<std::string>(price_result), "1.2300");
    EXPECT_EQ(price_result, quantity_result);
}

TEST(DecimalFormatTest, FixedConvenienceFunctionsUseStorageScale) {
    const auto price = bmd::format_price_fixed(*bmd::PriceUnits::create(123), bmd_test::scale(2));
    const auto quantity =
        bmd::format_quantity_fixed(*bmd::QuantityUnits::create(0), bmd_test::scale(3));
    ASSERT_TRUE(std::holds_alternative<std::string>(price));
    ASSERT_TRUE(std::holds_alternative<std::string>(quantity));
    EXPECT_EQ(std::get<std::string>(price), "1.23");
    EXPECT_EQ(std::get<std::string>(quantity), "0.000");
}

TEST(DecimalFormatTest, RejectsImpossibleOutputLengthBeforeAllocation) {
    const auto result = bmd::format_quantity(*bmd::QuantityUnits::create(1), bmd_test::scale(0),
                                             std::numeric_limits<std::size_t>::max());
    bmd_test::expect_error(result, bmd::DecimalErrorCode::Overflow, bmd::kNoErrorOffset);
}
