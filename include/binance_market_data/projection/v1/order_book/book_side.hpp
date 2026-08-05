#pragma once

#include <cstdint>
#include <string_view>

namespace binance_market_data::projection::v1 {

enum class BookSide : std::uint8_t {
    Bid,
    Ask,
};

[[nodiscard]] constexpr std::string_view to_string(BookSide side) noexcept {
    switch (side) {
    case BookSide::Bid:
        return "BID";
    case BookSide::Ask:
        return "ASK";
    }
    return "UNKNOWN";
}

} // namespace binance_market_data::projection::v1
