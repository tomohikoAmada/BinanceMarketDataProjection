#pragma once

#include <binance_market_data/projection/v1/order_book/book_level.hpp>
#include <binance_market_data/projection/v1/order_book/book_side.hpp>

namespace binance_market_data::projection::v1 {

struct LevelUpdate final {
    BookSide side;
    PriceUnits price;
    QuantityUnits quantity;

    friend constexpr bool operator==(const LevelUpdate&, const LevelUpdate&) noexcept = default;
};

} // namespace binance_market_data::projection::v1
