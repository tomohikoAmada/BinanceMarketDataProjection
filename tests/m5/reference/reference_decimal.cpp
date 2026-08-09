#include "reference_decimal.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <variant>

namespace bmd_projection::m5::reference {
namespace {

struct DecimalLexeme final {
    std::string_view integer_digits;
    std::string_view fractional_digits;
    std::size_t fractional_offset{kReferenceNoErrorOffset};
};

using LexicalResult = std::variant<DecimalLexeme, ReferenceDecimalError>;

[[nodiscard]] constexpr bool is_ascii_digit(char value) noexcept {
    return value >= '0' && value <= '9';
}

// Phase A: decompose syntax into integer and optional fractional substrings. No
// numeric accumulator or target-scale state participates in lexical validation.
[[nodiscard]] LexicalResult decompose(std::string_view text) noexcept {
    if (text.empty()) {
        return ReferenceDecimalError{ReferenceDecimalErrorCode::Empty, kReferenceNoErrorOffset};
    }
    if (text.front() == '+' || text.front() == '-') {
        return ReferenceDecimalError{ReferenceDecimalErrorCode::SignNotAllowed, 0};
    }

    const auto decimal_point = text.find('.');
    const auto integer_end = decimal_point == std::string_view::npos ? text.size() : decimal_point;
    if (integer_end == 0) {
        return ReferenceDecimalError{ReferenceDecimalErrorCode::InvalidSyntax, 0};
    }
    for (std::size_t offset = 0; offset < integer_end; ++offset) {
        if (!is_ascii_digit(text[offset])) {
            return ReferenceDecimalError{ReferenceDecimalErrorCode::InvalidSyntax, offset};
        }
        if (offset > 0 && text.front() == '0') {
            return ReferenceDecimalError{ReferenceDecimalErrorCode::LeadingZero, offset};
        }
    }

    if (decimal_point == std::string_view::npos) {
        return DecimalLexeme{text, {}, kReferenceNoErrorOffset};
    }
    if (decimal_point + 1 == text.size()) {
        return ReferenceDecimalError{ReferenceDecimalErrorCode::MissingFractionDigits,
                                     decimal_point};
    }
    const auto fractional_offset = decimal_point + 1;
    auto fractional_digits = text;
    fractional_digits.remove_prefix(fractional_offset);
    for (std::size_t index = 0; index < fractional_digits.size(); ++index) {
        if (!is_ascii_digit(fractional_digits[index])) {
            return ReferenceDecimalError{ReferenceDecimalErrorCode::InvalidSyntax,
                                         fractional_offset + index};
        }
    }
    auto integer_digits = text;
    integer_digits.remove_suffix(text.size() - integer_end);
    return DecimalLexeme{integer_digits, fractional_digits, fractional_offset};
}

struct CheckedMagnitude final {
    std::int64_t value{};
    std::optional<std::size_t> overflow_offset;
};

// Phase B: interpret the complete source magnitude (or the exact retained prefix
// after a downscale) independently of target padding. The overflow test is phrased
// as quotient/remainder comparison rather than production's subtraction formula.
[[nodiscard]] CheckedMagnitude accumulate_segment(std::string_view digits,
                                                  std::size_t source_offset,
                                                  std::int64_t initial) noexcept {
    constexpr auto maximum = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    auto magnitude = static_cast<std::uint64_t>(initial);
    for (std::size_t index = 0; index < digits.size(); ++index) {
        const auto digit = static_cast<std::uint64_t>(digits[index] - '0');
        if (magnitude > maximum / 10 || (magnitude == maximum / 10 && digit > maximum % 10)) {
            return {initial, source_offset + index};
        }
        magnitude = magnitude * 10 + digit;
    }
    return {static_cast<std::int64_t>(magnitude), std::nullopt};
}

[[nodiscard]] CheckedMagnitude source_magnitude(const DecimalLexeme& lexeme,
                                                std::size_t retained_fraction_digits) noexcept {
    const auto integer = accumulate_segment(lexeme.integer_digits, 0, 0);
    if (integer.overflow_offset.has_value()) {
        return integer;
    }
    auto retained = lexeme.fractional_digits;
    retained.remove_suffix(retained.size() - retained_fraction_digits);
    return accumulate_segment(retained, lexeme.fractional_offset, integer.value);
}

[[nodiscard]] std::optional<std::size_t>
first_nonzero_discarded(const DecimalLexeme& lexeme,
                        std::size_t retained_fraction_digits) noexcept {
    auto discarded = lexeme.fractional_digits;
    discarded.remove_prefix(retained_fraction_digits);
    for (std::size_t index = 0; index < discarded.size(); ++index) {
        if (discarded[index] != '0') {
            return lexeme.fractional_offset + retained_fraction_digits + index;
        }
    }
    return std::nullopt;
}

// Phase C: upscale with checked multiplication, or downscale only after proving
// exact divisibility from the discarded decimal suffix.
[[nodiscard]] ReferenceDecimalResult rescale(const DecimalLexeme& lexeme,
                                             std::uint32_t target_scale, bool allow_zero) noexcept {
    const auto source_scale = lexeme.fractional_digits.size();
    const auto target = static_cast<std::size_t>(target_scale);
    const auto retained_fraction_digits = std::min(source_scale, target);
    if (const auto inexact = first_nonzero_discarded(lexeme, retained_fraction_digits)) {
        return ReferenceDecimalResult{
            ReferenceDecimalError{ReferenceDecimalErrorCode::InexactScale, *inexact}};
    }

    auto magnitude = source_magnitude(lexeme, retained_fraction_digits);
    if (magnitude.overflow_offset.has_value()) {
        return ReferenceDecimalResult{
            ReferenceDecimalError{ReferenceDecimalErrorCode::Overflow, *magnitude.overflow_offset}};
    }

    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    for (std::size_t scale = source_scale; scale < target; ++scale) {
        if (magnitude.value > maximum / 10) {
            return ReferenceDecimalResult{ReferenceDecimalError{ReferenceDecimalErrorCode::Overflow,
                                                                kReferenceNoErrorOffset}};
        }
        magnitude.value *= 10;
    }
    if (!allow_zero && magnitude.value == 0) {
        return ReferenceDecimalResult{ReferenceDecimalError{
            ReferenceDecimalErrorCode::ZeroNotAllowed, kReferenceNoErrorOffset}};
    }
    return ReferenceDecimalResult{ReferenceDecimalValue{magnitude.value, source_scale}};
}

[[nodiscard]] ReferenceDecimalResult parse(std::string_view text, std::uint32_t target_scale,
                                           bool allow_zero) noexcept {
    const auto lexical = decompose(text);
    if (const auto* lexeme = std::get_if<DecimalLexeme>(&lexical)) {
        return rescale(*lexeme, target_scale, allow_zero);
    }
    return ReferenceDecimalResult{*std::get_if<ReferenceDecimalError>(&lexical)};
}

} // namespace

ReferenceDecimalResult parse_reference_decimal(std::string_view text, std::uint32_t target_scale,
                                               bool allow_zero) noexcept {
    return parse(text, target_scale, allow_zero);
}

std::string reference_fixed(ReferenceFixedInput input) {
    auto digits = std::to_string(input.units);
    if (input.scale == 0) {
        return digits;
    }
    if (digits.size() <= input.scale) {
        digits.insert(0, static_cast<std::size_t>(input.scale) + 1 - digits.size(), '0');
    }
    digits.insert(digits.size() - input.scale, 1, '.');
    return digits;
}

} // namespace bmd_projection::m5::reference
