#include <binance_market_data/projection/v1/version.hpp>

namespace binance_market_data::projection::v1 {

std::string_view library_version() noexcept { return kProjectVersion; }

} // namespace binance_market_data::projection::v1
