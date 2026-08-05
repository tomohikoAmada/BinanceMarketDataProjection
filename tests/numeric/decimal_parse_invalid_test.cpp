#include "test_helpers.hpp"

#include <binance_market_data/projection/v1/numeric/decimal_error.hpp>
#include <binance_market_data/projection/v1/numeric/decimal_parse.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <string_view>

namespace bmd = binance_market_data::projection::v1;

namespace {

struct InvalidCase final {
    std::string_view text;
    bmd::DecimalErrorCode code;
    std::size_t offset;
};

} // namespace

TEST(DecimalParseInvalidTest, RejectsEveryExplicitContractsSyntaxExample) {
    constexpr InvalidCase cases[]{{"", bmd::DecimalErrorCode::Empty, bmd::kNoErrorOffset},
                                  {" ", bmd::DecimalErrorCode::InvalidSyntax, 0},
                                  {" 1", bmd::DecimalErrorCode::InvalidSyntax, 0},
                                  {"1 ", bmd::DecimalErrorCode::InvalidSyntax, 1},
                                  {"+1", bmd::DecimalErrorCode::SignNotAllowed, 0},
                                  {"-1", bmd::DecimalErrorCode::SignNotAllowed, 0},
                                  {"-0", bmd::DecimalErrorCode::SignNotAllowed, 0},
                                  {"-0.0", bmd::DecimalErrorCode::SignNotAllowed, 0},
                                  {".5", bmd::DecimalErrorCode::InvalidSyntax, 0},
                                  {"1.", bmd::DecimalErrorCode::MissingFractionDigits, 1},
                                  {"01", bmd::DecimalErrorCode::LeadingZero, 1},
                                  {"00.1", bmd::DecimalErrorCode::LeadingZero, 1},
                                  {"0001.20", bmd::DecimalErrorCode::LeadingZero, 1},
                                  {"1e3", bmd::DecimalErrorCode::InvalidSyntax, 1},
                                  {"1E3", bmd::DecimalErrorCode::InvalidSyntax, 1},
                                  {"NaN", bmd::DecimalErrorCode::InvalidSyntax, 0},
                                  {"Infinity", bmd::DecimalErrorCode::InvalidSyntax, 0},
                                  {"-Infinity", bmd::DecimalErrorCode::SignNotAllowed, 0},
                                  {"1..0", bmd::DecimalErrorCode::InvalidSyntax, 2},
                                  {"1,2", bmd::DecimalErrorCode::InvalidSyntax, 1}};

    for (const auto& test_case : cases) {
        SCOPED_TRACE(test_case.text);
        bmd_test::expect_error(bmd::parse_quantity(test_case.text, bmd_test::scale(8)),
                               test_case.code, test_case.offset);
    }
}

TEST(DecimalParseInvalidTest, RejectsControlAndNonAsciiBytes) {
    const std::string ascii_control(1, '\x01');
    const std::string embedded_null{"1\0.0", 4};
    const std::string non_ascii_digit{"\xD9\xA1", 2};
    const std::string newline{"1\n", 2};
    const std::string tab{"\t1", 2};

    for (const auto& text : {ascii_control, embedded_null, non_ascii_digit, newline, tab}) {
        SCOPED_TRACE(testing::PrintToString(text));
        const auto result = bmd::parse_quantity(text, bmd_test::scale(8));
        ASSERT_TRUE(std::holds_alternative<bmd::DecimalError>(result));
        EXPECT_EQ(std::get<bmd::DecimalError>(result).code, bmd::DecimalErrorCode::InvalidSyntax);
    }
}

TEST(DecimalParseInvalidTest, RejectsAdditionalSeparatorForms) {
    for (const std::string_view text : {"1.2.3", "1,0", "1\t.0", "0x1"}) {
        SCOPED_TRACE(text);
        const auto result = bmd::parse_quantity(text, bmd_test::scale(8));
        ASSERT_TRUE(std::holds_alternative<bmd::DecimalError>(result));
        EXPECT_EQ(std::get<bmd::DecimalError>(result).code, bmd::DecimalErrorCode::InvalidSyntax);
    }
}

TEST(DecimalParseInvalidTest, PriceRejectsAllExactZeroSpellings) {
    for (const std::string_view text : {"0", "0.0", "0.00000000"}) {
        SCOPED_TRACE(text);
        bmd_test::expect_error(bmd::parse_price(text, bmd_test::scale(8)),
                               bmd::DecimalErrorCode::ZeroNotAllowed, bmd::kNoErrorOffset);
    }
}

TEST(DecimalParseInvalidTest, PositiveQuantityRejectsAllExactZeroSpellings) {
    for (const std::string_view text : {"0", "0.0", "0.00000000"}) {
        SCOPED_TRACE(text);
        bmd_test::expect_error(bmd::parse_positive_quantity(text, bmd_test::scale(8)),
                               bmd::DecimalErrorCode::ZeroNotAllowed, bmd::kNoErrorOffset);
    }
}

TEST(DecimalParseInvalidTest, ReportsEveryStableErrorCodeAsText) {
    EXPECT_EQ(bmd::to_string(bmd::DecimalErrorCode::Empty), "EMPTY");
    EXPECT_EQ(bmd::to_string(bmd::DecimalErrorCode::InvalidSyntax), "INVALID_SYNTAX");
    EXPECT_EQ(bmd::to_string(bmd::DecimalErrorCode::SignNotAllowed), "SIGN_NOT_ALLOWED");
    EXPECT_EQ(bmd::to_string(bmd::DecimalErrorCode::LeadingZero), "LEADING_ZERO");
    EXPECT_EQ(bmd::to_string(bmd::DecimalErrorCode::MissingFractionDigits),
              "MISSING_FRACTION_DIGITS");
    EXPECT_EQ(bmd::to_string(bmd::DecimalErrorCode::ZeroNotAllowed), "ZERO_NOT_ALLOWED");
    EXPECT_EQ(bmd::to_string(bmd::DecimalErrorCode::InexactScale), "INEXACT_SCALE");
    EXPECT_EQ(bmd::to_string(bmd::DecimalErrorCode::Overflow), "OVERFLOW");
    EXPECT_EQ(bmd::to_string(static_cast<bmd::DecimalErrorCode>(255)), "UNKNOWN");
}

TEST(DecimalParseInvalidTest, AppliesDocumentedErrorPrecedence) {
    bmd_test::expect_error(bmd::parse_quantity("01.234", bmd_test::scale(2)),
                           bmd::DecimalErrorCode::LeadingZero, 1);
    bmd_test::expect_error(bmd::parse_quantity("9223372036854775808.1", bmd_test::scale(0)),
                           bmd::DecimalErrorCode::InexactScale, 20);
    bmd_test::expect_error(bmd::parse_price("0.1", bmd_test::scale(0)),
                           bmd::DecimalErrorCode::InexactScale, 2);
    bmd_test::expect_error(bmd::parse_quantity("9223372036854775808x", bmd_test::scale(0)),
                           bmd::DecimalErrorCode::InvalidSyntax, 19);
}
