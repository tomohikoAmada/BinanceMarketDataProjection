#include "reference_decimal.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

namespace bmd_projection::m5::reference {
namespace {

constexpr std::array<std::int64_t, 19> kReferencePowersOfTen{
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

struct ScannedCharacter final {
    char value;
    std::size_t offset;
};

struct ScanState final {
    std::size_t target_fraction_digits{0};
    std::int64_t units{0};
    std::size_t source_fraction_digits{0};
    std::size_t decimal_point_offset{kReferenceNoErrorOffset};
    std::size_t overflow_offset{kReferenceNoErrorOffset};
    std::size_t inexact_offset{kReferenceNoErrorOffset};
    bool in_fraction{false};
    bool overflow{false};
};

[[nodiscard]] bool is_ascii_digit(char value) noexcept { return value >= '0' && value <= '9'; }

// A decimal point is rejected at the first position (no integer part) or when the
// text already has one.
[[nodiscard]] std::optional<ReferenceDecimalError> consume_decimal_point(ScanState& state,
                                                                         std::size_t offset) {
    if (state.in_fraction || offset == 0) {
        return ReferenceDecimalError{ReferenceDecimalErrorCode::InvalidSyntax, offset};
    }
    state.in_fraction = true;
    state.decimal_point_offset = offset;
    return std::nullopt;
}

// Fractional digits beyond the storage scale are discarded after zero-checking; the
// first non-zero discarded digit is remembered as the inexact-scale evidence.
[[nodiscard]] bool should_consume_digit(ScanState& state, ScannedCharacter character) noexcept {
    if (!state.in_fraction) {
        return true;
    }
    ++state.source_fraction_digits;
    if (state.source_fraction_digits <= state.target_fraction_digits) {
        return true;
    }
    if (character.value != '0' && state.inexact_offset == kReferenceNoErrorOffset) {
        state.inexact_offset = character.offset;
    }
    return false;
}

void append_digit(ScanState& state, ScannedCharacter character) noexcept {
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

[[nodiscard]] std::optional<ReferenceDecimalError>
consume_character(ScanState& state, std::string_view text, std::size_t offset) noexcept {
    const ScannedCharacter character{text[offset], offset};
    if (character.value == '.') {
        return consume_decimal_point(state, offset);
    }
    if (!is_ascii_digit(character.value)) {
        return ReferenceDecimalError{ReferenceDecimalErrorCode::InvalidSyntax, offset};
    }
    if (!state.in_fraction && offset > 0 && text.front() == '0') {
        return ReferenceDecimalError{ReferenceDecimalErrorCode::LeadingZero, offset};
    }
    if (should_consume_digit(state, character)) {
        append_digit(state, character);
    }
    return std::nullopt;
}

// Padding appends storage-scale zeros. Overflow here is final-zero-padding overflow,
// which carries no digit offset.
void pad_to_storage_scale(ScanState& state) noexcept {
    if (state.overflow || state.source_fraction_digits >= state.target_fraction_digits) {
        return;
    }
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    const auto scale_difference = state.target_fraction_digits - state.source_fraction_digits;
    const auto multiplier = kReferencePowersOfTen.at(scale_difference);
    if (state.units > maximum / multiplier) {
        state.overflow = true;
        return;
    }
    state.units *= multiplier;
}

// Single linear scan with deferred error reporting in the documented M1 precedence:
// syntax first, then exact-scale representability, then overflow, then the zero
// domain constraint.
[[nodiscard]] ReferenceDecimalResult scan(std::string_view text, std::uint32_t target_scale,
                                          bool allow_zero) noexcept {
    if (text.empty()) {
        return ReferenceDecimalResult{
            ReferenceDecimalError{ReferenceDecimalErrorCode::Empty, kReferenceNoErrorOffset}};
    }
    if (text.front() == '+' || text.front() == '-') {
        return ReferenceDecimalResult{
            ReferenceDecimalError{ReferenceDecimalErrorCode::SignNotAllowed, 0}};
    }

    ScanState state{};
    state.target_fraction_digits = static_cast<std::size_t>(target_scale);

    for (std::size_t offset = 0; offset < text.size(); ++offset) {
        const auto error = consume_character(state, text, offset);
        if (error.has_value()) {
            return ReferenceDecimalResult{*error};
        }
    }

    if (state.in_fraction && state.source_fraction_digits == 0) {
        return ReferenceDecimalResult{ReferenceDecimalError{
            ReferenceDecimalErrorCode::MissingFractionDigits, state.decimal_point_offset}};
    }
    if (state.inexact_offset != kReferenceNoErrorOffset) {
        return ReferenceDecimalResult{
            ReferenceDecimalError{ReferenceDecimalErrorCode::InexactScale, state.inexact_offset}};
    }

    pad_to_storage_scale(state);
    if (state.overflow) {
        return ReferenceDecimalResult{
            ReferenceDecimalError{ReferenceDecimalErrorCode::Overflow, state.overflow_offset}};
    }
    if (!allow_zero && state.units == 0) {
        return ReferenceDecimalResult{ReferenceDecimalError{
            ReferenceDecimalErrorCode::ZeroNotAllowed, kReferenceNoErrorOffset}};
    }

    return ReferenceDecimalResult{ReferenceDecimalValue{
        state.units, static_cast<std::uint32_t>(state.source_fraction_digits)}};
}

} // namespace

ReferenceDecimalResult parse_reference_decimal(std::string_view text, std::uint32_t target_scale,
                                               bool allow_zero) noexcept {
    return scan(text, target_scale, allow_zero);
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
