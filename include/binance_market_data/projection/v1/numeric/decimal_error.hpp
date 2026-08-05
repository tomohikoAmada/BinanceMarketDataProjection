#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

namespace binance_market_data::projection::v1 {

enum class DecimalErrorCode : std::uint8_t {
    Empty,
    InvalidSyntax,
    SignNotAllowed,
    LeadingZero,
    MissingFractionDigits,
    ZeroNotAllowed,
    InexactScale,
    Overflow,
};

inline constexpr std::size_t kNoErrorOffset = std::numeric_limits<std::size_t>::max();

struct DecimalError final {
    DecimalErrorCode code;
    std::size_t offset;

    friend constexpr bool operator==(const DecimalError&, const DecimalError&) noexcept = default;
};

[[nodiscard]] constexpr std::string_view to_string(DecimalErrorCode code) noexcept {
    switch (code) {
    case DecimalErrorCode::Empty:
        return "EMPTY";
    case DecimalErrorCode::InvalidSyntax:
        return "INVALID_SYNTAX";
    case DecimalErrorCode::SignNotAllowed:
        return "SIGN_NOT_ALLOWED";
    case DecimalErrorCode::LeadingZero:
        return "LEADING_ZERO";
    case DecimalErrorCode::MissingFractionDigits:
        return "MISSING_FRACTION_DIGITS";
    case DecimalErrorCode::ZeroNotAllowed:
        return "ZERO_NOT_ALLOWED";
    case DecimalErrorCode::InexactScale:
        return "INEXACT_SCALE";
    case DecimalErrorCode::Overflow:
        return "OVERFLOW";
    }
    return "UNKNOWN";
}

} // namespace binance_market_data::projection::v1
