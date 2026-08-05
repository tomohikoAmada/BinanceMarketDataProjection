#pragma once

#include <binance_market_data/projection/v1/numeric/price_units.hpp>
#include <binance_market_data/projection/v1/numeric/quantity_units.hpp>

namespace binance_market_data::projection::v1 {

struct BookLevel final {
    PriceUnits price;
    QuantityUnits quantity;

    friend constexpr bool operator==(const BookLevel&, const BookLevel&) noexcept = default;
};

} // namespace binance_market_data::projection::v1
