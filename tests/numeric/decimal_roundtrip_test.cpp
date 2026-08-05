#include "test_helpers.hpp"

#include <binance_market_data/projection/v1/numeric/decimal_format.hpp>
#include <binance_market_data/projection/v1/numeric/decimal_parse.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <variant>

namespace bmd = binance_market_data::projection::v1;

namespace {

struct RoundtripCase final {
    std::string_view input;
    std::uint32_t storage_scale;
};

struct FixedQuantityCase final {
    std::int64_t units;
    std::uint32_t scale;
};

void expect_source_roundtrip(const RoundtripCase& test_case) {
    const auto scale = bmd_test::scale(test_case.storage_scale);
    const auto result = bmd::parse_quantity(test_case.input, scale);
    const auto* parsed = std::get_if<bmd::ParsedDecimal<bmd::QuantityUnits>>(&result);
    if (parsed == nullptr) {
        ADD_FAILURE() << "quantity parse failed";
        return;
    }

    const auto formatted =
        bmd::format_quantity(parsed->value, scale, parsed->source_fraction_digits);
    const auto* text = std::get_if<std::string>(&formatted);
    if (text == nullptr) {
        ADD_FAILURE() << "quantity format failed";
        return;
    }
    EXPECT_EQ(*text, test_case.input);
}

void expect_fixed_quantity_roundtrip(const FixedQuantityCase& test_case) {
    const auto scale = bmd_test::scale(test_case.scale);
    const auto value = bmd_test::quantity_units(test_case.units);
    const auto formatted = bmd::format_quantity_fixed(value, scale);
    const auto* text = std::get_if<std::string>(&formatted);
    if (text == nullptr) {
        ADD_FAILURE() << "quantity format failed";
        return;
    }

    const auto parsed = bmd::parse_quantity(*text, scale);
    const auto* parsed_value = std::get_if<bmd::ParsedDecimal<bmd::QuantityUnits>>(&parsed);
    if (parsed_value == nullptr) {
        ADD_FAILURE() << "quantity parse failed";
        return;
    }
    EXPECT_EQ(parsed_value->value, value);
}

void expect_fixed_price_roundtrip(std::int64_t units) {
    const auto scale = bmd_test::scale(18);
    const auto value = bmd_test::price_units(units);
    const auto formatted = bmd::format_price_fixed(value, scale);
    const auto* text = std::get_if<std::string>(&formatted);
    if (text == nullptr) {
        ADD_FAILURE() << "price format failed";
        return;
    }

    const auto parsed = bmd::parse_price(*text, scale);
    const auto* parsed_value = std::get_if<bmd::ParsedDecimal<bmd::PriceUnits>>(&parsed);
    if (parsed_value == nullptr) {
        ADD_FAILURE() << "price parse failed";
        return;
    }
    EXPECT_EQ(parsed_value->value, value);
}

} // namespace

TEST(DecimalRoundtripTest, ParsedSourcePrecisionReconstructsCanonicalBytes) {
    constexpr std::array cases{RoundtripCase{"0", 18},
                               RoundtripCase{"1", 18},
                               RoundtripCase{"10", 0},
                               RoundtripCase{"0.1", 18},
                               RoundtripCase{"1.0", 18},
                               RoundtripCase{"1.2300", 18},
                               RoundtripCase{"999999.000000", 6},
                               RoundtripCase{"0.000000000000000000", 18}};
    for (const auto& test_case : cases) {
        SCOPED_TRACE(test_case.input);
        expect_source_roundtrip(test_case);
    }
}

TEST(DecimalRoundtripTest, FixedFormatThenParsePreservesQuantityUnits) {
    for (const std::int64_t units : {std::int64_t{0}, std::int64_t{1}, std::int64_t{123},
                                     std::numeric_limits<std::int64_t>::max()}) {
        for (const std::uint32_t scale_value : {0U, 1U, 8U, 18U}) {
            SCOPED_TRACE(testing::Message() << units << '@' << scale_value);
            expect_fixed_quantity_roundtrip(FixedQuantityCase{units, scale_value});
        }
    }
}

TEST(DecimalRoundtripTest, FixedFormatThenParsePreservesPriceUnits) {
    for (const std::int64_t units :
         {std::int64_t{1}, std::int64_t{123}, std::numeric_limits<std::int64_t>::max()}) {
        expect_fixed_price_roundtrip(units);
    }
}
