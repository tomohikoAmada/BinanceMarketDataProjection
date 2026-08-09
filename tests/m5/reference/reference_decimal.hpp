#pragma once

// R1 ReferenceDecimal: an independent decimal reference for M5 differential validation.
//
// This layer is written from the M1 numeric semantics document and the M1 unit-test
// contract, not by calling production M1 functions. It implements its own digit scan,
// checked powers of ten, exact rescale, error offsets, and canonical fixed formatting.

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <variant>

namespace bmd_projection::m5::reference {

inline constexpr std::size_t kReferenceNoErrorOffset = std::numeric_limits<std::size_t>::max();

// Stable reference error categories. These are mapped to the canonical M1 semantic
// names only at the observation boundary; no production enum is used here.
enum class ReferenceDecimalErrorCode : std::uint8_t {
    Empty,
    InvalidSyntax,
    SignNotAllowed,
    LeadingZero,
    MissingFractionDigits,
    ZeroNotAllowed,
    InexactScale,
    Overflow,
};

struct ReferenceDecimalError final {
    ReferenceDecimalErrorCode code;
    std::size_t offset;

    friend constexpr bool operator==(const ReferenceDecimalError&,
                                     const ReferenceDecimalError&) noexcept = default;
};

struct ReferenceDecimalValue final {
    std::int64_t units;
    std::uint32_t source_fraction_digits;

    friend constexpr bool operator==(const ReferenceDecimalValue&,
                                     const ReferenceDecimalValue&) noexcept = default;
};

struct ReferenceDecimalResult final {
    std::variant<ReferenceDecimalValue, ReferenceDecimalError> value;

    friend constexpr bool operator==(const ReferenceDecimalResult&,
                                     const ReferenceDecimalResult&) noexcept = default;
};

// Parses an unsigned decimal text at the target storage scale. allow_zero=false
// enforces the price/positive-quantity non-zero constraint. The observable contract
// matches the M1 documented grammar, error precedence, and offset rules.
[[nodiscard]] ReferenceDecimalResult parse_reference_decimal(std::string_view text,
                                                             std::uint32_t target_scale,
                                                             bool allow_zero) noexcept;

// Canonical fixed formatting: integer part without leading zeros (zero itself emits
// "0"), decimal point, and exactly `scale` fractional digits with trailing zeros.
struct ReferenceFixedInput final {
    std::int64_t units;
    std::uint32_t scale;

    friend constexpr bool operator==(const ReferenceFixedInput&,
                                     const ReferenceFixedInput&) noexcept = default;
};

[[nodiscard]] std::string reference_fixed(ReferenceFixedInput input);

} // namespace bmd_projection::m5::reference
