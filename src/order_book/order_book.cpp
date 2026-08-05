#include <binance_market_data/projection/v1/order_book/order_book.hpp>

#include <functional>
#include <limits>
#include <map>

namespace binance_market_data::projection::v1 {

using BidMap = std::map<PriceUnits, QuantityUnits, std::greater<PriceUnits>>;
using AskMap = std::map<PriceUnits, QuantityUnits, std::less<PriceUnits>>;

class OrderBook::Impl final {
  public:
    explicit Impl(NumericSpec spec) noexcept : spec_(spec) {}

    [[nodiscard]] NumericSpec numeric_spec() const noexcept { return spec_; }

    [[nodiscard]] bool empty() const noexcept { return bids_.empty() && asks_.empty(); }

    [[nodiscard]] bool side_empty(BookSide side) const noexcept {
        if (side == BookSide::Bid) {
            return bids_.empty();
        }
        return asks_.empty();
    }

    [[nodiscard]] std::size_t level_count(BookSide side) const noexcept {
        if (side == BookSide::Bid) {
            return bids_.size();
        }
        return asks_.size();
    }

    [[nodiscard]] std::optional<BookLevel> best_bid() const noexcept {
        if (bids_.empty()) {
            return std::nullopt;
        }
        const auto it = bids_.begin();
        return BookLevel{it->first, it->second};
    }

    [[nodiscard]] std::optional<BookLevel> best_ask() const noexcept {
        if (asks_.empty()) {
            return std::nullopt;
        }
        const auto it = asks_.begin();
        return BookLevel{it->first, it->second};
    }

    [[nodiscard]] std::optional<QuantityUnits> quantity_at(BookSide side,
                                                           PriceUnits price) const noexcept {
        if (side == BookSide::Bid) {
            const auto it = bids_.find(price);
            if (it != bids_.end()) {
                return it->second;
            }
            return std::nullopt;
        }
        const auto it = asks_.find(price);
        if (it != asks_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    [[nodiscard]] LevelChange apply_level(BookSide side, PriceUnits price, QuantityUnits quantity) {
        if (quantity.value() == 0) {
            return apply_remove(side, price);
        }
        return apply_insert_or_update(side, price, quantity);
    }

    void apply_updates(std::span<const LevelUpdate> updates) {
        for (const auto& update : updates) {
            static_cast<void>(apply_level(update.side, update.price, update.quantity));
        }
    }

    void replace_all(std::span<const BookLevel> bids, std::span<const BookLevel> asks) {
        BidMap new_bids;
        AskMap new_asks;

        for (const auto& level : bids) {
            if (level.quantity.value() == 0) {
                new_bids.erase(level.price);
            } else {
                new_bids.insert_or_assign(level.price, level.quantity);
            }
        }

        for (const auto& level : asks) {
            if (level.quantity.value() == 0) {
                new_asks.erase(level.price);
            } else {
                new_asks.insert_or_assign(level.price, level.quantity);
            }
        }

        bids_ = std::move(new_bids);
        asks_ = std::move(new_asks);
    }

    void clear() noexcept {
        bids_.clear();
        asks_.clear();
    }

    void clear_side(BookSide side) noexcept {
        if (side == BookSide::Bid) {
            bids_.clear();
        } else {
            asks_.clear();
        }
    }

    [[nodiscard]] std::vector<BookLevel> top_levels(BookSide side, std::size_t limit) const {
        if (limit == 0) {
            return {};
        }
        return copy_levels(side, limit);
    }

    [[nodiscard]] std::vector<BookLevel> all_levels(BookSide side) const {
        return copy_levels(side, std::numeric_limits<std::size_t>::max());
    }

  private:
    [[nodiscard]] LevelChange apply_remove(BookSide side, PriceUnits price) noexcept {
        if (side == BookSide::Bid) {
            const auto it = bids_.find(price);
            if (it == bids_.end()) {
                return LevelChange::Unchanged;
            }
            bids_.erase(it);
            return LevelChange::Removed;
        }
        const auto it = asks_.find(price);
        if (it == asks_.end()) {
            return LevelChange::Unchanged;
        }
        asks_.erase(it);
        return LevelChange::Removed;
    }

    [[nodiscard]] LevelChange apply_insert_or_update(BookSide side, PriceUnits price,
                                                     QuantityUnits quantity) {
        if (side == BookSide::Bid) {
            const auto it = bids_.find(price);
            if (it == bids_.end()) {
                bids_.insert_or_assign(price, quantity);
                return LevelChange::Inserted;
            }
            if (it->second == quantity) {
                return LevelChange::Unchanged;
            }
            it->second = quantity;
            return LevelChange::Updated;
        }
        const auto it = asks_.find(price);
        if (it == asks_.end()) {
            asks_.insert_or_assign(price, quantity);
            return LevelChange::Inserted;
        }
        if (it->second == quantity) {
            return LevelChange::Unchanged;
        }
        it->second = quantity;
        return LevelChange::Updated;
    }

    template <typename Map>
    [[nodiscard]] std::vector<BookLevel> copy_from_map(const Map& map, std::size_t limit) const {
        std::vector<BookLevel> result;
        for (const auto& [price, quantity] : map) {
            if (result.size() >= limit) {
                break;
            }
            result.push_back(BookLevel{price, quantity});
        }
        return result;
    }

    [[nodiscard]] std::vector<BookLevel> copy_levels(BookSide side, std::size_t limit) const {
        if (side == BookSide::Bid) {
            return copy_from_map(bids_, limit);
        }
        return copy_from_map(asks_, limit);
    }

    NumericSpec spec_;
    BidMap bids_;
    AskMap asks_;
};

OrderBook::OrderBook(NumericSpec numeric_spec) : impl_(std::make_unique<Impl>(numeric_spec)) {}

OrderBook::~OrderBook() = default;

OrderBook::OrderBook(OrderBook&&) noexcept = default;
OrderBook& OrderBook::operator=(OrderBook&&) noexcept = default;

NumericSpec OrderBook::numeric_spec() const noexcept { return impl_->numeric_spec(); }

bool OrderBook::empty() const noexcept { return impl_->empty(); }

bool OrderBook::side_empty(BookSide side) const noexcept { return impl_->side_empty(side); }

std::size_t OrderBook::level_count(BookSide side) const noexcept {
    return impl_->level_count(side);
}

std::optional<BookLevel> OrderBook::best_bid() const noexcept { return impl_->best_bid(); }

std::optional<BookLevel> OrderBook::best_ask() const noexcept { return impl_->best_ask(); }

std::optional<QuantityUnits> OrderBook::quantity_at(BookSide side,
                                                    PriceUnits price) const noexcept {
    return impl_->quantity_at(side, price);
}

LevelChange OrderBook::apply_level(BookSide side, PriceUnits price, QuantityUnits quantity) {
    return impl_->apply_level(side, price, quantity);
}

void OrderBook::apply_updates(std::span<const LevelUpdate> updates) {
    impl_->apply_updates(updates);
}

void OrderBook::replace_all(std::span<const BookLevel> bids, std::span<const BookLevel> asks) {
    impl_->replace_all(bids, asks);
}

void OrderBook::clear() noexcept { impl_->clear(); }

void OrderBook::clear_side(BookSide side) noexcept { impl_->clear_side(side); }

std::vector<BookLevel> OrderBook::top_levels(BookSide side, std::size_t limit) const {
    return impl_->top_levels(side, limit);
}

std::vector<BookLevel> OrderBook::all_levels(BookSide side) const {
    return impl_->all_levels(side);
}

} // namespace binance_market_data::projection::v1
