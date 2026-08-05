#pragma once

#include <binance_market_data/projection/v1/numeric/decimal_error.hpp>
#include <binance_market_data/projection/v1/numeric/decimal_scale.hpp>
#include <binance_market_data/projection/v1/numeric/price_units.hpp>
#include <binance_market_data/projection/v1/numeric/quantity_units.hpp>

#include <cstddef>
#include <string_view>
#include <variant>

namespace binance_market_data::projection::v1 {

template <typename T> struct ParsedDecimal final {
    T value;
    std::size_t source_fraction_digits;

    friend constexpr bool operator==(const ParsedDecimal&, const ParsedDecimal&) noexcept = default;
};

using PriceParseResult = std::variant<ParsedDecimal<PriceUnits>, DecimalError>;
using QuantityParseResult = std::variant<ParsedDecimal<QuantityUnits>, DecimalError>;

[[nodiscard]] PriceParseResult parse_price(std::string_view text,
                                           DecimalScale storage_scale) noexcept;

[[nodiscard]] QuantityParseResult parse_quantity(std::string_view text,
                                                 DecimalScale storage_scale) noexcept;

[[nodiscard]] QuantityParseResult parse_positive_quantity(std::string_view text,
                                                          DecimalScale storage_scale) noexcept;

} // namespace binance_market_data::projection::v1
