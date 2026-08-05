#include "test_helpers.hpp"

#include <binance_market_data/projection/v1/numeric/decimal_format.hpp>
#include <binance_market_data/projection/v1/numeric/decimal_parse.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

namespace bmd = binance_market_data::projection::v1;

namespace {

struct ValidCase final {
    std::string_view text;
    std::uint32_t scale;
    std::int64_t units;
    std::size_t source_fraction_digits;
};

void expect_valid_price(const ValidCase& test_case) {
    const auto scale = bmd_test::scale(test_case.scale);
    const auto result = bmd::parse_price(test_case.text, scale);
    const auto* parsed = std::get_if<bmd::ParsedDecimal<bmd::PriceUnits>>(&result);
    if (parsed == nullptr) {
        ADD_FAILURE() << "price parse failed";
        return;
    }

    EXPECT_EQ(parsed->value.value(), test_case.units);
    EXPECT_EQ(parsed->source_fraction_digits, test_case.source_fraction_digits);
    const auto formatted = bmd::format_price(parsed->value, scale, parsed->source_fraction_digits);
    const auto* text = std::get_if<std::string>(&formatted);
    if (text == nullptr) {
        ADD_FAILURE() << "price format failed";
        return;
    }
    EXPECT_EQ(*text, test_case.text);
}

void expect_valid_quantity(const ValidCase& test_case) {
    const auto scale = bmd_test::scale(test_case.scale);
    const auto result = bmd::parse_quantity(test_case.text, scale);
    const auto* parsed = std::get_if<bmd::ParsedDecimal<bmd::QuantityUnits>>(&result);
    if (parsed == nullptr) {
        ADD_FAILURE() << "quantity parse failed";
        return;
    }

    EXPECT_EQ(parsed->value.value(), test_case.units);
    EXPECT_EQ(parsed->source_fraction_digits, test_case.source_fraction_digits);
    const auto formatted =
        bmd::format_quantity(parsed->value, scale, parsed->source_fraction_digits);
    const auto* text = std::get_if<std::string>(&formatted);
    if (text == nullptr) {
        ADD_FAILURE() << "quantity format failed";
        return;
    }
    EXPECT_EQ(*text, test_case.text);
}

} // namespace

static_assert(
    std::is_nothrow_invocable_v<decltype(&bmd::parse_price), std::string_view, bmd::DecimalScale>);
static_assert(std::is_nothrow_invocable_v<decltype(&bmd::parse_quantity), std::string_view,
                                          bmd::DecimalScale>);
static_assert(std::is_nothrow_invocable_v<decltype(&bmd::parse_positive_quantity), std::string_view,
                                          bmd::DecimalScale>);

TEST(DecimalParseValidTest, ParsesRequiredPriceBoundaryCasesAndReconstructsSource) {
    constexpr std::array cases{
        ValidCase{"1", 0, 1, 0},
        ValidCase{"1", 8, 100'000'000, 0},
        ValidCase{"1.0", 8, 100'000'000, 1},
        ValidCase{"1.2300", 2, 123, 4},
        ValidCase{"0.00000001", 8, 1, 8},
        ValidCase{"9223372036854775807", 0, std::numeric_limits<std::int64_t>::max(), 0},
        ValidCase{"9.223372036854775807", 18, std::numeric_limits<std::int64_t>::max(), 18}};

    for (const auto& test_case : cases) {
        SCOPED_TRACE(test_case.text);
        expect_valid_price(test_case);
    }
}

TEST(DecimalParseValidTest, ParsesRequiredQuantityBoundaryCasesAndReconstructsSource) {
    constexpr std::array cases{
        ValidCase{"0", 0, 0, 0}, ValidCase{"0", 8, 0, 0}, ValidCase{"0.0000", 2, 0, 4},
        ValidCase{"1.2300", 2, 123, 4},
        ValidCase{"9223372036854775807", 0, std::numeric_limits<std::int64_t>::max(), 0}};

    for (const auto& test_case : cases) {
        SCOPED_TRACE(test_case.text);
        expect_valid_quantity(test_case);
    }
}

TEST(DecimalParseValidTest, PositiveQuantityAcceptsSmallestUnitAtScaleEight) {
    const auto result = bmd::parse_positive_quantity("0.00000001", bmd_test::scale(8));
    ASSERT_TRUE(std::holds_alternative<bmd::ParsedDecimal<bmd::QuantityUnits>>(result));
    EXPECT_EQ(std::get<bmd::ParsedDecimal<bmd::QuantityUnits>>(result).value.value(), 1);
}

TEST(DecimalParseValidTest, ExtraSourceFractionZeroesAreExactlyDiscarded) {
    const auto result = bmd::parse_quantity("1.000000000000000000", bmd_test::scale(0));
    ASSERT_TRUE(std::holds_alternative<bmd::ParsedDecimal<bmd::QuantityUnits>>(result));
    const auto parsed = std::get<bmd::ParsedDecimal<bmd::QuantityUnits>>(result);
    EXPECT_EQ(parsed.value.value(), 1);
    EXPECT_EQ(parsed.source_fraction_digits, 18);
}
