#include <binance_market_data/projection/v1/numeric/decimal_parse.hpp>

#include <array>
#include <cstdint>
#include <limits>
#include <variant>

namespace binance_market_data::projection::v1 {
namespace {

constexpr std::array<std::int64_t, 19> kPowersOfTen{
    1,
    10,
    100,
    1'000,
    10'000,
    100'000,
    1'000'000,
    10'000'000,
    100'000'000,
    1'000'000'000,
    10'000'000'000,
    100'000'000'000,
    1'000'000'000'000,
    10'000'000'000'000,
    100'000'000'000'000,
    1'000'000'000'000'000,
    10'000'000'000'000'000,
    100'000'000'000'000'000,
    1'000'000'000'000'000'000,
};

struct ParsedMagnitude final {
    std::int64_t units;
    std::size_t source_fraction_digits;
};

using MagnitudeParseResult = std::variant<ParsedMagnitude, DecimalError>;

[[nodiscard]] constexpr bool is_ascii_digit(char value) noexcept {
    return value >= '0' && value <= '9';
}

[[nodiscard]] MagnitudeParseResult parse_magnitude(std::string_view text,
                                                   DecimalScale storage_scale) noexcept {
    if (text.empty()) {
        return DecimalError{DecimalErrorCode::Empty, kNoErrorOffset};
    }
    if (text.front() == '+' || text.front() == '-') {
        return DecimalError{DecimalErrorCode::SignNotAllowed, 0};
    }

    const auto target_fraction_digits = static_cast<std::size_t>(storage_scale.value());
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    std::int64_t units = 0;
    std::size_t source_fraction_digits = 0;
    std::size_t decimal_point_offset = kNoErrorOffset;
    std::size_t overflow_offset = kNoErrorOffset;
    std::size_t inexact_offset = kNoErrorOffset;
    bool in_fraction = false;
    bool overflow = false;

    for (std::size_t offset = 0; offset < text.size(); ++offset) {
        const char character = text[offset];
        if (character == '.') {
            if (in_fraction || offset == 0) {
                return DecimalError{DecimalErrorCode::InvalidSyntax, offset};
            }
            in_fraction = true;
            decimal_point_offset = offset;
            continue;
        }
        if (!is_ascii_digit(character)) {
            return DecimalError{DecimalErrorCode::InvalidSyntax, offset};
        }
        if (!in_fraction && offset > 0 && text.front() == '0') {
            return DecimalError{DecimalErrorCode::LeadingZero, offset};
        }

        bool consume_digit = !in_fraction;
        if (in_fraction) {
            ++source_fraction_digits;
            consume_digit = source_fraction_digits <= target_fraction_digits;
            if (!consume_digit && character != '0' && inexact_offset == kNoErrorOffset) {
                inexact_offset = offset;
            }
        }

        if (consume_digit && !overflow) {
            const auto digit = static_cast<std::int64_t>(character - '0');
            if (units > (maximum - digit) / 10) {
                overflow = true;
                overflow_offset = offset;
            } else {
                units = units * 10 + digit;
            }
        }
    }

    if (in_fraction && source_fraction_digits == 0) {
        return DecimalError{DecimalErrorCode::MissingFractionDigits, decimal_point_offset};
    }
    if (inexact_offset != kNoErrorOffset) {
        return DecimalError{DecimalErrorCode::InexactScale, inexact_offset};
    }

    if (!overflow && source_fraction_digits < target_fraction_digits) {
        const auto scale_difference = target_fraction_digits - source_fraction_digits;
        const auto multiplier = kPowersOfTen[scale_difference];
        if (units > maximum / multiplier) {
            overflow = true;
        } else {
            units *= multiplier;
        }
    }
    if (overflow) {
        return DecimalError{DecimalErrorCode::Overflow, overflow_offset};
    }

    return ParsedMagnitude{units, source_fraction_digits};
}

} // namespace

PriceParseResult parse_price(std::string_view text, DecimalScale storage_scale) noexcept {
    const auto magnitude_result = parse_magnitude(text, storage_scale);
    if (const auto* error = std::get_if<DecimalError>(&magnitude_result)) {
        return *error;
    }
    const auto magnitude = std::get<ParsedMagnitude>(magnitude_result);
    const auto value = PriceUnits::create(magnitude.units);
    if (!value.has_value()) {
        return DecimalError{DecimalErrorCode::ZeroNotAllowed, kNoErrorOffset};
    }
    return ParsedDecimal<PriceUnits>{*value, magnitude.source_fraction_digits};
}

QuantityParseResult parse_quantity(std::string_view text, DecimalScale storage_scale) noexcept {
    const auto magnitude_result = parse_magnitude(text, storage_scale);
    if (const auto* error = std::get_if<DecimalError>(&magnitude_result)) {
        return *error;
    }
    const auto magnitude = std::get<ParsedMagnitude>(magnitude_result);
    return ParsedDecimal<QuantityUnits>{*QuantityUnits::create(magnitude.units),
                                        magnitude.source_fraction_digits};
}

QuantityParseResult parse_positive_quantity(std::string_view text,
                                            DecimalScale storage_scale) noexcept {
    const auto result = parse_quantity(text, storage_scale);
    if (const auto* error = std::get_if<DecimalError>(&result)) {
        return *error;
    }
    const auto parsed = std::get<ParsedDecimal<QuantityUnits>>(result);
    if (parsed.value.value() == 0) {
        return DecimalError{DecimalErrorCode::ZeroNotAllowed, kNoErrorOffset};
    }
    return parsed;
}

} // namespace binance_market_data::projection::v1
