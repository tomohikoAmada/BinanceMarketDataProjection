#pragma once

#include <binance_market_data/projection/v1/numeric/decimal_scale.hpp>

namespace binance_market_data::projection::v1 {

struct NumericSpec final {
    DecimalScale price_scale;
    DecimalScale quantity_scale;

    friend constexpr bool operator==(const NumericSpec&, const NumericSpec&) noexcept = default;
};

} // namespace binance_market_data::projection::v1
