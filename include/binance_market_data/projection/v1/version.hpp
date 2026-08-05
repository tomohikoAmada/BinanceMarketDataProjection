#pragma once

#include <string_view>

namespace binance_market_data::projection::v1 {

inline constexpr std::string_view kProjectName{"BinanceMarketDataProjection"};

inline constexpr std::string_view kProjectVersion{"0.1.0-alpha.0"};

[[nodiscard]] std::string_view library_version() noexcept;

} // namespace binance_market_data::projection::v1
