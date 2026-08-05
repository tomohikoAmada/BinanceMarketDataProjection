#include "test_helpers.hpp"

#include <binance_market_data/projection/v1/numeric/decimal_format.hpp>
#include <binance_market_data/projection/v1/numeric/decimal_parse.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <variant>

namespace bmd = binance_market_data::projection::v1;

TEST(DecimalRoundtripTest, ParsedSourcePrecisionReconstructsCanonicalBytes) {
    struct RoundtripCase final {
        std::string_view input;
        std::uint32_t storage_scale;
    };
    constexpr RoundtripCase cases[]{{"0", 18},
                                    {"1", 18},
                                    {"10", 0},
                                    {"0.1", 18},
                                    {"1.0", 18},
                                    {"1.2300", 18},
                                    {"999999.000000", 6},
                                    {"0.000000000000000000", 18}};
    for (const auto& test_case : cases) {
        SCOPED_TRACE(test_case.input);
        const auto scale = bmd_test::scale(test_case.storage_scale);
        const auto result = bmd::parse_quantity(test_case.input, scale);
        ASSERT_TRUE(std::holds_alternative<bmd::ParsedDecimal<bmd::QuantityUnits>>(result));
        const auto parsed = std::get<bmd::ParsedDecimal<bmd::QuantityUnits>>(result);
        const auto formatted =
            bmd::format_quantity(parsed.value, scale, parsed.source_fraction_digits);
        ASSERT_TRUE(std::holds_alternative<std::string>(formatted));
        EXPECT_EQ(std::get<std::string>(formatted), test_case.input);
    }
}

TEST(DecimalRoundtripTest, FixedFormatThenParsePreservesQuantityUnits) {
    for (const std::int64_t units : {std::int64_t{0}, std::int64_t{1}, std::int64_t{123},
                                     std::numeric_limits<std::int64_t>::max()}) {
        for (const std::uint32_t scale_value : {0U, 1U, 8U, 18U}) {
            SCOPED_TRACE(testing::Message() << units << '@' << scale_value);
            const auto scale = bmd_test::scale(scale_value);
            const auto value = bmd_test::quantity_units(units);
            const auto formatted = bmd::format_quantity_fixed(value, scale);
            ASSERT_TRUE(std::holds_alternative<std::string>(formatted));
            const auto parsed = bmd::parse_quantity(std::get<std::string>(formatted), scale);
            ASSERT_TRUE(std::holds_alternative<bmd::ParsedDecimal<bmd::QuantityUnits>>(parsed));
            EXPECT_EQ(std::get<bmd::ParsedDecimal<bmd::QuantityUnits>>(parsed).value, value);
        }
    }
}

TEST(DecimalRoundtripTest, FixedFormatThenParsePreservesPriceUnits) {
    for (const std::int64_t units :
         {std::int64_t{1}, std::int64_t{123}, std::numeric_limits<std::int64_t>::max()}) {
        const auto scale = bmd_test::scale(18);
        const auto value = bmd_test::price_units(units);
        const auto formatted = bmd::format_price_fixed(value, scale);
        ASSERT_TRUE(std::holds_alternative<std::string>(formatted));
        const auto parsed = bmd::parse_price(std::get<std::string>(formatted), scale);
        ASSERT_TRUE(std::holds_alternative<bmd::ParsedDecimal<bmd::PriceUnits>>(parsed));
        EXPECT_EQ(std::get<bmd::ParsedDecimal<bmd::PriceUnits>>(parsed).value, value);
    }
}
