#include "reference_decimal.hpp"

#include <binance_market_data/projection/v1/numeric/decimal_format.hpp>
#include <binance_market_data/projection/v1/numeric/decimal_parse.hpp>
#include <binance_market_data/projection/v1/numeric/decimal_scale.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace {

namespace ref = bmd_projection::m5::reference;
namespace core = binance_market_data::projection::v1;

struct InvalidCase final {
    std::string_view text;
    ref::ReferenceDecimalErrorCode code;
    std::size_t offset;
};

constexpr std::array kInvalidCases{
    InvalidCase{"", ref::ReferenceDecimalErrorCode::Empty, ref::kReferenceNoErrorOffset},
    InvalidCase{" ", ref::ReferenceDecimalErrorCode::InvalidSyntax, 0},
    InvalidCase{" 1", ref::ReferenceDecimalErrorCode::InvalidSyntax, 0},
    InvalidCase{"1 ", ref::ReferenceDecimalErrorCode::InvalidSyntax, 1},
    InvalidCase{"+1", ref::ReferenceDecimalErrorCode::SignNotAllowed, 0},
    InvalidCase{"-1", ref::ReferenceDecimalErrorCode::SignNotAllowed, 0},
    InvalidCase{"-0", ref::ReferenceDecimalErrorCode::SignNotAllowed, 0},
    InvalidCase{"-0.0", ref::ReferenceDecimalErrorCode::SignNotAllowed, 0},
    InvalidCase{".5", ref::ReferenceDecimalErrorCode::InvalidSyntax, 0},
    InvalidCase{"1.", ref::ReferenceDecimalErrorCode::MissingFractionDigits, 1},
    InvalidCase{"0.", ref::ReferenceDecimalErrorCode::MissingFractionDigits, 1},
    InvalidCase{"01", ref::ReferenceDecimalErrorCode::LeadingZero, 1},
    InvalidCase{"00.1", ref::ReferenceDecimalErrorCode::LeadingZero, 1},
    InvalidCase{"0001.20", ref::ReferenceDecimalErrorCode::LeadingZero, 1},
    InvalidCase{"1e3", ref::ReferenceDecimalErrorCode::InvalidSyntax, 1},
    InvalidCase{"1E3", ref::ReferenceDecimalErrorCode::InvalidSyntax, 1},
    InvalidCase{"NaN", ref::ReferenceDecimalErrorCode::InvalidSyntax, 0},
    InvalidCase{"Infinity", ref::ReferenceDecimalErrorCode::InvalidSyntax, 0},
    InvalidCase{"-Infinity", ref::ReferenceDecimalErrorCode::SignNotAllowed, 0},
    InvalidCase{"1..0", ref::ReferenceDecimalErrorCode::InvalidSyntax, 2},
    InvalidCase{"1.2.3", ref::ReferenceDecimalErrorCode::InvalidSyntax, 3},
    InvalidCase{"1,2", ref::ReferenceDecimalErrorCode::InvalidSyntax, 1},
    InvalidCase{"1+2", ref::ReferenceDecimalErrorCode::InvalidSyntax, 1},
    InvalidCase{"1-2", ref::ReferenceDecimalErrorCode::InvalidSyntax, 1},
    InvalidCase{"0x1", ref::ReferenceDecimalErrorCode::InvalidSyntax, 1},
};

[[nodiscard]] ref::ReferenceDecimalErrorCode to_reference(core::DecimalErrorCode code) noexcept {
    switch (code) {
    case core::DecimalErrorCode::Empty:
        return ref::ReferenceDecimalErrorCode::Empty;
    case core::DecimalErrorCode::InvalidSyntax:
        return ref::ReferenceDecimalErrorCode::InvalidSyntax;
    case core::DecimalErrorCode::SignNotAllowed:
        return ref::ReferenceDecimalErrorCode::SignNotAllowed;
    case core::DecimalErrorCode::LeadingZero:
        return ref::ReferenceDecimalErrorCode::LeadingZero;
    case core::DecimalErrorCode::MissingFractionDigits:
        return ref::ReferenceDecimalErrorCode::MissingFractionDigits;
    case core::DecimalErrorCode::ZeroNotAllowed:
        return ref::ReferenceDecimalErrorCode::ZeroNotAllowed;
    case core::DecimalErrorCode::InexactScale:
        return ref::ReferenceDecimalErrorCode::InexactScale;
    case core::DecimalErrorCode::Overflow:
        return ref::ReferenceDecimalErrorCode::Overflow;
    }
    return ref::ReferenceDecimalErrorCode::InvalidSyntax;
}

template <typename Production>
void expect_equivalent_impl(std::string_view text, std::uint32_t scale,
                            const ref::ReferenceDecimalResult& reference,
                            const Production& production) {
    if (const auto* reference_error = std::get_if<ref::ReferenceDecimalError>(&reference.value)) {
        ASSERT_TRUE(std::holds_alternative<core::DecimalError>(production));
        const auto& production_error = std::get<core::DecimalError>(production);
        EXPECT_EQ(to_reference(production_error.code), reference_error->code)
            << "text=" << text << " scale=" << scale;
        EXPECT_EQ(production_error.offset, reference_error->offset)
            << "text=" << text << " scale=" << scale;
        return;
    }
    const auto& reference_value = std::get<ref::ReferenceDecimalValue>(reference.value);
    std::int64_t units = 0;
    std::size_t source_fraction_digits = 0;
    std::visit(
        [&](const auto& parsed) {
            using Parsed = std::decay_t<decltype(parsed)>;
            if constexpr (!std::is_same_v<Parsed, core::DecimalError>) {
                units = parsed.value.value();
                source_fraction_digits = parsed.source_fraction_digits;
            }
        },
        production);
    EXPECT_EQ(units, reference_value.units);
    EXPECT_EQ(source_fraction_digits, reference_value.source_fraction_digits);
}

void expect_equivalent(std::string_view text, std::uint32_t scale, bool allow_zero) {
    const auto reference = ref::parse_reference_decimal(text, scale, allow_zero);
    const auto storage_scale = core::DecimalScale::create(scale).value();
    if (allow_zero) {
        expect_equivalent_impl(text, scale, reference, core::parse_quantity(text, storage_scale));
    } else {
        expect_equivalent_impl(text, scale, reference, core::parse_price(text, storage_scale));
    }
}

TEST(ReferenceDecimalTest, RejectsEveryStableSyntaxCategoryWithOffset) {
    for (const auto& test_case : kInvalidCases) {
        SCOPED_TRACE(test_case.text);
        const auto result = ref::parse_reference_decimal(test_case.text, 8, true);
        ASSERT_TRUE(std::holds_alternative<ref::ReferenceDecimalError>(result.value));
        const auto& error = std::get<ref::ReferenceDecimalError>(result.value);
        EXPECT_EQ(error.code, test_case.code);
        EXPECT_EQ(error.offset, test_case.offset);
    }
}

TEST(ReferenceDecimalTest, AppliesDocumentedErrorPrecedence) {
    const auto leading_zero = ref::parse_reference_decimal("01.234", 2, true);
    const auto& leading_error = std::get<ref::ReferenceDecimalError>(leading_zero.value);
    EXPECT_EQ(leading_error.code, ref::ReferenceDecimalErrorCode::LeadingZero);
    EXPECT_EQ(leading_error.offset, 1U);

    const auto inexact_before_overflow =
        ref::parse_reference_decimal("9223372036854775808.1", 0, true);
    const auto& inexact_error = std::get<ref::ReferenceDecimalError>(inexact_before_overflow.value);
    EXPECT_EQ(inexact_error.code, ref::ReferenceDecimalErrorCode::InexactScale);
    EXPECT_EQ(inexact_error.offset, 20U);

    const auto overflow_padding = ref::parse_reference_decimal("10", 18, true);
    const auto& padding_error = std::get<ref::ReferenceDecimalError>(overflow_padding.value);
    EXPECT_EQ(padding_error.code, ref::ReferenceDecimalErrorCode::Overflow);
    EXPECT_EQ(padding_error.offset, ref::kReferenceNoErrorOffset);
}

TEST(ReferenceDecimalTest, HandlesInt64BoundaryExactly) {
    const auto maximum = ref::parse_reference_decimal("9223372036854775807.0", 0, true);
    const auto& maximum_value = std::get<ref::ReferenceDecimalValue>(maximum.value);
    EXPECT_EQ(maximum_value.units, std::numeric_limits<std::int64_t>::max());

    const auto overflow = ref::parse_reference_decimal("9223372036854775808", 0, true);
    const auto& overflow_error = std::get<ref::ReferenceDecimalError>(overflow.value);
    EXPECT_EQ(overflow_error.code, ref::ReferenceDecimalErrorCode::Overflow);
    EXPECT_EQ(overflow_error.offset, 18U);

    const auto scale_eighteen_max = ref::parse_reference_decimal("9.223372036854775807", 18, true);
    const auto& eighteen_value = std::get<ref::ReferenceDecimalValue>(scale_eighteen_max.value);
    EXPECT_EQ(eighteen_value.units, std::numeric_limits<std::int64_t>::max());

    const auto scale_eighteen_overflow =
        ref::parse_reference_decimal("9.223372036854775808", 18, true);
    const auto& eighteen_error =
        std::get<ref::ReferenceDecimalError>(scale_eighteen_overflow.value);
    EXPECT_EQ(eighteen_error.code, ref::ReferenceDecimalErrorCode::Overflow);
    EXPECT_EQ(eighteen_error.offset, 19U);
}

TEST(ReferenceDecimalTest, EnforcesZeroDomainConstraint) {
    for (const std::string_view zero : {"0", "0.0", "0.00000000"}) {
        SCOPED_TRACE(zero);
        const auto rejected = ref::parse_reference_decimal(zero, 8, false);
        const auto& error = std::get<ref::ReferenceDecimalError>(rejected.value);
        EXPECT_EQ(error.code, ref::ReferenceDecimalErrorCode::ZeroNotAllowed);
        EXPECT_EQ(error.offset, ref::kReferenceNoErrorOffset);

        const auto allowed = ref::parse_reference_decimal(zero, 8, true);
        EXPECT_TRUE(std::holds_alternative<ref::ReferenceDecimalValue>(allowed.value));
    }
}

TEST(ReferenceDecimalTest, ExactRescaleAndTrailingZeros) {
    const auto value = ref::parse_reference_decimal("1.2300", 8, true);
    const auto& parsed = std::get<ref::ReferenceDecimalValue>(value.value);
    EXPECT_EQ(parsed.units, 123'000'000);
    EXPECT_EQ(parsed.source_fraction_digits, 4U);

    const auto expanded = ref::parse_reference_decimal("1.5", 2, true);
    const auto& expanded_value = std::get<ref::ReferenceDecimalValue>(expanded.value);
    EXPECT_EQ(expanded_value.units, 150);

    const auto downscaled = ref::parse_reference_decimal("1.0000", 2, true);
    const auto& downscaled_value = std::get<ref::ReferenceDecimalValue>(downscaled.value);
    EXPECT_EQ(downscaled_value.units, 100);

    const auto inexact = ref::parse_reference_decimal("1.5", 0, true);
    const auto& inexact_error = std::get<ref::ReferenceDecimalError>(inexact.value);
    EXPECT_EQ(inexact_error.code, ref::ReferenceDecimalErrorCode::InexactScale);
    EXPECT_EQ(inexact_error.offset, 2U);
}

TEST(ReferenceDecimalTest, FixedFormattingIsCanonical) {
    EXPECT_EQ(ref::reference_fixed(ref::ReferenceFixedInput{0, 2}), "0.00");
    EXPECT_EQ(ref::reference_fixed(ref::ReferenceFixedInput{1, 0}), "1");
    EXPECT_EQ(ref::reference_fixed(ref::ReferenceFixedInput{12'300, 8}), "0.00012300");
    EXPECT_EQ(ref::reference_fixed(ref::ReferenceFixedInput{1'000'000, 4}), "100.0000");
    EXPECT_EQ(ref::reference_fixed(ref::ReferenceFixedInput{123, 3}), "0.123");
    EXPECT_EQ(ref::reference_fixed(ref::ReferenceFixedInput{10'100, 2}), "101.00");
    EXPECT_EQ(ref::reference_fixed(ref::ReferenceFixedInput{3'125, 3}), "3.125");
}

TEST(ReferenceDecimalTest, DifferentialMatchesM1AcrossBoundaryTable) {
    for (const auto& test_case : kInvalidCases) {
        SCOPED_TRACE(test_case.text);
        expect_equivalent(test_case.text, 8, true);
        expect_equivalent(test_case.text, 8, false);
    }
    for (const std::string_view text :
         {"0", "1", "1.0", "1.2300", "0.5", "99.0", "100.0000", "9223372036854775807",
          "9223372036854775807.0", "9223372036854775808", "9.223372036854775807",
          "1.000000000000000001", "10", "12345.6789", "0.000000000000000001"}) {
        for (std::uint32_t scale = 0; scale <= 18; ++scale) {
            expect_equivalent(text, scale, true);
        }
    }
    for (const std::string_view text : {"0", "1", "0.5", "1.5", "12345.6789"}) {
        for (std::uint32_t scale = 0; scale <= 18; ++scale) {
            expect_equivalent(text, scale, false);
        }
    }
}

TEST(ReferenceDecimalTest, DifferentialMatchesM1ForGeneratedTokens) {
    std::uint64_t state = 0x4D35524546444543ULL;
    const auto next = [&state]() noexcept {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return state;
    };
    for (std::size_t iteration = 0; iteration < 5000; ++iteration) {
        const auto random = next();
        const std::uint32_t scale = static_cast<std::uint32_t>(random % 19U);
        const auto digits = 1 + static_cast<std::uint32_t>(random % 15U);
        const auto fraction = static_cast<std::uint32_t>((random >> 8U) % 19U);
        const auto leading = static_cast<std::uint32_t>((random >> 16U) % 3U);
        std::string token;
        token.append(leading, '0');
        token += static_cast<char>('1' + (random % 9U));
        for (std::uint32_t index = 1; index < digits; ++index) {
            token += static_cast<char>('0' + (random >> index) % 10U);
        }
        if (fraction > 0) {
            token += '.';
            for (std::uint32_t index = 0; index < fraction; ++index) {
                token += static_cast<char>('0' + (random >> (index + 4U)) % 10U);
            }
        }
        const auto selector = static_cast<unsigned int>((random >> 24U) % 8U);
        if (selector == 0) {
            token.insert(token.begin(), '+');
        } else if (selector == 1) {
            token.insert(token.begin(), '-');
        } else if (selector == 2) {
            token.push_back('x');
        } else if (selector == 3 && !token.empty()) {
            token.insert(0, "1.");
        } else if (selector == 4) {
            token += '.';
        }
        SCOPED_TRACE(token);
        expect_equivalent(token, scale, (random & 1U) == 0U);
    }
}

TEST(ReferenceDecimalTest, FixedFormattingDifferentialMatchesM1) {
    std::uint64_t state = 0x4D35464F524D4154ULL;
    const auto next = [&state]() noexcept {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return state;
    };
    for (std::size_t iteration = 0; iteration < 2000; ++iteration) {
        const auto random = next();
        const auto scale = static_cast<std::uint32_t>(random % 19U);
        const auto units = static_cast<std::int64_t>(random % 100'000'000'000ULL);
        const auto formatted = ref::reference_fixed(ref::ReferenceFixedInput{units, scale});
        const auto production = core::format_quantity_fixed(
            core::QuantityUnits::create(units).value(), core::DecimalScale::create(scale).value());
        ASSERT_TRUE(std::holds_alternative<std::string>(production));
        EXPECT_EQ(std::get<std::string>(production), formatted)
            << "units=" << units << " scale=" << scale;
    }
}

} // namespace

// NOLINTEND(bugprone-unchecked-optional-access)
