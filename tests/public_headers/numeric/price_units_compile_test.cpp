#include <binance_market_data/projection/v1/numeric/price_units.hpp>

static_assert(binance_market_data::projection::v1::PriceUnits::create(1).has_value());
