#include <binance_market_data/projection/v1/numeric/decimal_parse.hpp>

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
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

struct ParseState final {
    std::size_t target_fraction_digits{0};
    std::int64_t units{0};
    std::size_t source_fraction_digits{0};
    std::size_t decimal_point_offset{kNoErrorOffset};
    std::size_t overflow_offset{kNoErrorOffset};
    std::size_t inexact_offset{kNoErrorOffset};
    bool in_fraction{false};
    bool overflow{false};
};

struct ScannedCharacter final {
    char value;
    std::size_t offset;
};

[[nodiscard]] constexpr bool is_ascii_digit(char value) noexcept {
    return value >= '0' && value <= '9';
}

[[nodiscard]] std::optional<DecimalError> consume_decimal_point(ParseState& state,
                                                                std::size_t offset) noexcept {
    if (state.in_fraction || offset == 0) {
        return DecimalError{DecimalErrorCode::InvalidSyntax, offset};
    }
    state.in_fraction = true;
    state.decimal_point_offset = offset;
    return std::nullopt;
}

[[nodiscard]] bool should_consume_digit(ParseState& state,
                                        const ScannedCharacter& character) noexcept {
    if (!state.in_fraction) {
        return true;
    }

    ++state.source_fraction_digits;
    if (state.source_fraction_digits <= state.target_fraction_digits) {
        return true;
    }
    if (character.value != '0' && state.inexact_offset == kNoErrorOffset) {
        state.inexact_offset = character.offset;
    }
    return false;
}

void append_digit(ParseState& state, const ScannedCharacter& character) noexcept {
    if (state.overflow) {
        return;
    }

    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    const auto digit = static_cast<std::int64_t>(character.value - '0');
    if (state.units > (maximum - digit) / 10) {
        state.overflow = true;
        state.overflow_offset = character.offset;
        return;
    }
    state.units = state.units * 10 + digit;
}

[[nodiscard]] std::optional<DecimalError>
consume_character(std::string_view text, ParseState& state, std::size_t offset) noexcept {
    const ScannedCharacter character{text[offset], offset};
    if (character.value == '.') {
        return consume_decimal_point(state, offset);
    }
    if (!is_ascii_digit(character.value)) {
        return DecimalError{DecimalErrorCode::InvalidSyntax, offset};
    }
    if (!state.in_fraction && offset > 0 && text.front() == '0') {
        return DecimalError{DecimalErrorCode::LeadingZero, offset};
    }
    if (should_consume_digit(state, character)) {
        append_digit(state, character);
    }
    return std::nullopt;
}

void pad_to_storage_scale(ParseState& state) noexcept {
    if (state.overflow || state.source_fraction_digits >= state.target_fraction_digits) {
        return;
    }

    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    const auto scale_difference = state.target_fraction_digits - state.source_fraction_digits;
    const auto multiplier = kPowersOfTen.at(scale_difference);
    if (state.units > maximum / multiplier) {
        state.overflow = true;
        return;
    }
    state.units *= multiplier;
}

[[nodiscard]] MagnitudeParseResult parse_magnitude(std::string_view text,
                                                   DecimalScale storage_scale) noexcept {
    if (text.empty()) {
        return DecimalError{DecimalErrorCode::Empty, kNoErrorOffset};
    }
    if (text.front() == '+' || text.front() == '-') {
        return DecimalError{DecimalErrorCode::SignNotAllowed, 0};
    }

    ParseState state{};
    state.target_fraction_digits = static_cast<std::size_t>(storage_scale.value());

    for (std::size_t offset = 0; offset < text.size(); ++offset) {
        const auto error = consume_character(text, state, offset);
        if (error.has_value()) {
            return error.value();
        }
    }

    if (state.in_fraction && state.source_fraction_digits == 0) {
        return DecimalError{DecimalErrorCode::MissingFractionDigits, state.decimal_point_offset};
    }
    if (state.inexact_offset != kNoErrorOffset) {
        return DecimalError{DecimalErrorCode::InexactScale, state.inexact_offset};
    }

    pad_to_storage_scale(state);
    if (state.overflow) {
        return DecimalError{DecimalErrorCode::Overflow, state.overflow_offset};
    }

    return ParsedMagnitude{state.units, state.source_fraction_digits};
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
    const auto value = QuantityUnits::create(magnitude.units);
    if (!value.has_value()) {
        return DecimalError{DecimalErrorCode::Overflow, kNoErrorOffset};
    }
    return ParsedDecimal<QuantityUnits>{value.value(), magnitude.source_fraction_digits};
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
