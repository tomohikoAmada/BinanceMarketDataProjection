#pragma once

#include <binance_market_data/projection/v1/numeric/decimal_error.hpp>
#include <binance_market_data/projection/v1/numeric/decimal_scale.hpp>
#include <binance_market_data/projection/v1/numeric/price_units.hpp>
#include <binance_market_data/projection/v1/numeric/quantity_units.hpp>

#include <cstddef>
#include <string>
#include <variant>

namespace binance_market_data::projection::v1 {

using DecimalFormatResult = std::variant<std::string, DecimalError>;

[[nodiscard]] DecimalFormatResult format_price(PriceUnits value, DecimalScale storage_scale,
                                               std::size_t output_fraction_digits);

[[nodiscard]] DecimalFormatResult format_quantity(QuantityUnits value, DecimalScale storage_scale,
                                                  std::size_t output_fraction_digits);

[[nodiscard]] DecimalFormatResult format_price_fixed(PriceUnits value, DecimalScale storage_scale);

[[nodiscard]] DecimalFormatResult format_quantity_fixed(QuantityUnits value,
                                                        DecimalScale storage_scale);

} // namespace binance_market_data::projection::v1
