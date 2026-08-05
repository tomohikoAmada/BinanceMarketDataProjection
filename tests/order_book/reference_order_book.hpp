#pragma once

#include <binance_market_data/projection/v1/numeric/price_units.hpp>
#include <binance_market_data/projection/v1/numeric/quantity_units.hpp>
#include <binance_market_data/projection/v1/order_book/book_level.hpp>
#include <binance_market_data/projection/v1/order_book/book_side.hpp>

#include <algorithm>
#include <functional>
#include <optional>
#include <vector>

namespace bmd_test {

class ReferenceOrderBook final {
  public:
    ReferenceOrderBook() = default;

    void apply_level(const binance_market_data::projection::v1::BookSide side,
                     const binance_market_data::projection::v1::PriceUnits price,
                     const binance_market_data::projection::v1::QuantityUnits quantity) {
        auto& levels = side == binance_market_data::projection::v1::BookSide::Bid ? bids_ : asks_;
        const auto it = find_level(levels, price);
        if (quantity.value() == 0) {
            if (it != levels.end()) {
                levels.erase(it);
            }
            return;
        }
        if (it != levels.end()) {
            it->quantity = quantity;
        } else {
            levels.push_back({price, quantity});
        }
        sort_side(side);
    }

    void clear() noexcept {
        bids_.clear();
        asks_.clear();
    }

    void clear_side(binance_market_data::projection::v1::BookSide side) noexcept {
        if (side == binance_market_data::projection::v1::BookSide::Bid) {
            bids_.clear();
        } else {
            asks_.clear();
        }
    }

    [[nodiscard]] std::size_t
    level_count(binance_market_data::projection::v1::BookSide side) const noexcept {
        return side == binance_market_data::projection::v1::BookSide::Bid ? bids_.size()
                                                                          : asks_.size();
    }

    [[nodiscard]] std::optional<binance_market_data::projection::v1::BookLevel>
    best_bid() const noexcept {
        if (bids_.empty()) {
            return std::nullopt;
        }
        return bids_.front();
    }

    [[nodiscard]] std::optional<binance_market_data::projection::v1::BookLevel>
    best_ask() const noexcept {
        if (asks_.empty()) {
            return std::nullopt;
        }
        return asks_.front();
    }

    [[nodiscard]] std::optional<binance_market_data::projection::v1::QuantityUnits>
    quantity_at(binance_market_data::projection::v1::BookSide side,
                const binance_market_data::projection::v1::PriceUnits price) const {
        const auto& levels =
            side == binance_market_data::projection::v1::BookSide::Bid ? bids_ : asks_;
        const auto it = find_level(levels, price);
        if (it == levels.end()) {
            return std::nullopt;
        }
        return it->quantity;
    }

    [[nodiscard]] std::vector<binance_market_data::projection::v1::BookLevel>
    all_levels(binance_market_data::projection::v1::BookSide side) const {
        if (side == binance_market_data::projection::v1::BookSide::Bid) {
            return bids_;
        }
        return asks_;
    }

    [[nodiscard]] std::vector<binance_market_data::projection::v1::BookLevel>
    top_levels(binance_market_data::projection::v1::BookSide side, std::size_t limit) const {
        const auto& src =
            side == binance_market_data::projection::v1::BookSide::Bid ? bids_ : asks_;
        if (limit == 0 || src.empty())
            return {};
        return std::vector<BookLevel>(
            src.begin(),
            src.begin() + static_cast<LevelVector::difference_type>(std::min(limit, src.size())));
    }

  private:
    using BookLevel = binance_market_data::projection::v1::BookLevel;
    using BookSide = binance_market_data::projection::v1::BookSide;
    using PriceUnits = binance_market_data::projection::v1::PriceUnits;
    using LevelVector = std::vector<BookLevel>;

    [[nodiscard]] LevelVector::iterator find_level(LevelVector& levels, const PriceUnits price) {
        return std::find_if(levels.begin(), levels.end(),
                            [&](const BookLevel& level) { return level.price == price; });
    }

    [[nodiscard]] LevelVector::const_iterator find_level(const LevelVector& levels,
                                                         const PriceUnits price) const {
        return std::find_if(levels.begin(), levels.end(),
                            [&](const BookLevel& level) { return level.price == price; });
    }

    void sort_side(BookSide side) {
        auto& levels = side == BookSide::Bid ? bids_ : asks_;
        if (side == BookSide::Bid) {
            std::sort(levels.begin(), levels.end(),
                      [](const BookLevel& a, const BookLevel& b) { return b.price < a.price; });
        } else {
            std::sort(levels.begin(), levels.end(),
                      [](const BookLevel& a, const BookLevel& b) { return a.price < b.price; });
        }
    }

    LevelVector bids_;
    LevelVector asks_;
};

} // namespace bmd_test
