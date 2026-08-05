#pragma once

#include <binance_market_data/projection/v1/numeric/decimal_scale.hpp>
#include <binance_market_data/projection/v1/numeric/price_units.hpp>
#include <binance_market_data/projection/v1/numeric/quantity_units.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>

namespace bmd_test {

[[nodiscard]] inline binance_market_data::projection::v1::DecimalScale scale(std::uint32_t value) {
    const auto result = binance_market_data::projection::v1::DecimalScale::create(value);
    if (!result.has_value()) {
        ADD_FAILURE() << "invalid test scale: " << value;
        std::abort();
    }
    return result.value();
}

[[nodiscard]] inline binance_market_data::projection::v1::PriceUnits
price_units(std::int64_t value) {
    const auto result = binance_market_data::projection::v1::PriceUnits::create(value);
    if (!result.has_value()) {
        ADD_FAILURE() << "invalid test price units: " << value;
        std::abort();
    }
    return result.value();
}

[[nodiscard]] inline binance_market_data::projection::v1::QuantityUnits
quantity_units(std::int64_t value) {
    const auto result = binance_market_data::projection::v1::QuantityUnits::create(value);
    if (!result.has_value()) {
        ADD_FAILURE() << "invalid test quantity units: " << value;
        std::abort();
    }
    return result.value();
}

} // namespace bmd_test
