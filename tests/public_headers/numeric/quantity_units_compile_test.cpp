#include <binance_market_data/projection/v1/numeric/quantity_units.hpp>

static_assert(binance_market_data::projection::v1::QuantityUnits::create(0).has_value());
