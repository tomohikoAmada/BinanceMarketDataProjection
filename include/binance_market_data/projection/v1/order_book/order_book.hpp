#pragma once

#include <binance_market_data/projection/v1/numeric/numeric_spec.hpp>
#include <binance_market_data/projection/v1/numeric/price_units.hpp>
#include <binance_market_data/projection/v1/numeric/quantity_units.hpp>
#include <binance_market_data/projection/v1/order_book/book_level.hpp>
#include <binance_market_data/projection/v1/order_book/book_side.hpp>
#include <binance_market_data/projection/v1/order_book/level_update.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace binance_market_data::projection::v1 {

enum class LevelChange : std::uint8_t {
    Inserted,
    Updated,
    Removed,
    Unchanged,
};

[[nodiscard]] constexpr std::string_view to_string(LevelChange change) noexcept {
    switch (change) {
    case LevelChange::Inserted:
        return "INSERTED";
    case LevelChange::Updated:
        return "UPDATED";
    case LevelChange::Removed:
        return "REMOVED";
    case LevelChange::Unchanged:
        return "UNCHANGED";
    }
    return "UNKNOWN";
}

class OrderBook final {
  public:
    explicit OrderBook(NumericSpec numeric_spec);

    ~OrderBook();

    OrderBook(OrderBook&&) noexcept;
    OrderBook& operator=(OrderBook&&) noexcept;

    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;

    [[nodiscard]] NumericSpec numeric_spec() const noexcept;

    [[nodiscard]] bool empty() const noexcept;

    [[nodiscard]] bool side_empty(BookSide side) const noexcept;

    [[nodiscard]] std::size_t level_count(BookSide side) const noexcept;

    [[nodiscard]] std::optional<BookLevel> best_bid() const noexcept;

    [[nodiscard]] std::optional<BookLevel> best_ask() const noexcept;

    [[nodiscard]] std::optional<QuantityUnits>
    quantity_at(BookSide side, PriceUnits price) const noexcept;

    [[nodiscard]] LevelChange apply_level(BookSide side, PriceUnits price, QuantityUnits quantity);

    void apply_updates(std::span<const LevelUpdate> updates);

    void replace_all(std::span<const BookLevel> bids, std::span<const BookLevel> asks);

    void clear() noexcept;

    void clear_side(BookSide side) noexcept;

    [[nodiscard]] std::vector<BookLevel> top_levels(BookSide side, std::size_t limit) const;

    [[nodiscard]] std::vector<BookLevel> all_levels(BookSide side) const;

  private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace binance_market_data::projection::v1
