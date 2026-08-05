#include <binance_market_data/projection/v1/numeric/decimal_scale.hpp>

static_assert(binance_market_data::projection::v1::DecimalScale::create(18).has_value());
