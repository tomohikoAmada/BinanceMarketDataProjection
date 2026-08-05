#include <binance_market_data/projection/v1/numeric/decimal_error.hpp>

static_assert(binance_market_data::projection::v1::to_string(
                  binance_market_data::projection::v1::DecimalErrorCode::Overflow) == "OVERFLOW");
