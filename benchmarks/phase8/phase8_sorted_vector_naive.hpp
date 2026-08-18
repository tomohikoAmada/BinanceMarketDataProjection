#pragma once

// M5 Phase-8 candidate model: Phase8SortedVectorNaive (PR-A / WP1).
//
// Per-side sorted contiguous std::vector representation. Lookup uses
// std::lower_bound (logarithmic); every mutation performs a vector
// insertion/erase/update. apply_updates applies each LevelUpdate sequentially
// in input order with no batch dedup optimization.
//
// Ordering discipline: the bid side is kept sorted in DESCENDING price order
// (bids_.front() is the best bid), the ask side in ASCENDING price order
// (asks_.front() is the best ask), matching the std::map control iteration
// order.

#include <binance_market_data/projection/v1/numeric/numeric_spec.hpp>
#include <binance_market_data/projection/v1/numeric/price_units.hpp>
#include <binance_market_data/projection/v1/numeric/quantity_units.hpp>
#include <binance_market_data/projection/v1/order_book/book_level.hpp>
#include <binance_market_data/projection/v1/order_book/book_side.hpp>
#include <binance_market_data/projection/v1/order_book/level_update.hpp>
#include <binance_market_data/projection/v1/order_book/order_book.hpp>

#include "phase8_model.hpp"

#include <algorithm>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

namespace bmd_projection::m5::phase8 {

template <template <typename> class Alloc = std::allocator> class Phase8SortedVectorNaive {
  public:
    explicit Phase8SortedVectorNaive(core::NumericSpec spec) noexcept : spec_(spec) {}

    Phase8SortedVectorNaive(const Phase8SortedVectorNaive&) = delete;
    Phase8SortedVectorNaive& operator=(const Phase8SortedVectorNaive&) = delete;
    Phase8SortedVectorNaive(Phase8SortedVectorNaive&&) noexcept = default;
    Phase8SortedVectorNaive& operator=(Phase8SortedVectorNaive&&) noexcept = default;
    ~Phase8SortedVectorNaive() = default;

    static constexpr std::string_view model_id() noexcept { return kPhase8SortedVectorNaiveId; }

    [[nodiscard]] core::NumericSpec numeric_spec() const noexcept { return spec_; }

    [[nodiscard]] bool empty() const noexcept { return bids_.empty() && asks_.empty(); }

    [[nodiscard]] bool side_empty(core::BookSide side) const noexcept {
        return side == core::BookSide::Bid ? bids_.empty() : asks_.empty();
    }

    [[nodiscard]] std::size_t level_count(core::BookSide side) const noexcept {
        return side == core::BookSide::Bid ? bids_.size() : asks_.size();
    }

    [[nodiscard]] std::optional<core::BookLevel> best_bid() const noexcept {
        if (bids_.empty()) {
            return std::nullopt;
        }
        return bids_.front();
    }

    [[nodiscard]] std::optional<core::BookLevel> best_ask() const noexcept {
        if (asks_.empty()) {
            return std::nullopt;
        }
        return asks_.front();
    }

    [[nodiscard]] std::optional<core::QuantityUnits>
    quantity_at(core::BookSide side, core::PriceUnits price) const noexcept {
        if (side == core::BookSide::Bid) {
            const auto it = find_level(bids_, side, price);
            if (it == bids_.end()) {
                return std::nullopt;
            }
            return it->quantity;
        }
        const auto it = find_level(asks_, side, price);
        if (it == asks_.end()) {
            return std::nullopt;
        }
        return it->quantity;
    }

    core::LevelChange apply_level(core::BookSide side, core::PriceUnits price,
                                  core::QuantityUnits quantity) {
        if (quantity.value() == 0) {
            return apply_remove(side, price);
        }
        return apply_insert_or_update(side, price, quantity);
    }

    void apply_updates(std::span<const core::LevelUpdate> updates) {
        for (const auto& update : updates) {
            static_cast<void>(apply_level(update.side, update.price, update.quantity));
        }
    }

    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    void replace_all(std::span<const core::BookLevel> bids, std::span<const core::BookLevel> asks) {
        // Strong exception guarantee: build a complete temporary model state and
        // commit through one proven noexcept move.
        Phase8SortedVectorNaive temp(spec_);
        for (const auto& level : bids) {
            if (level.quantity.value() == 0) {
                erase_price(temp.bids_, core::BookSide::Bid, level.price);
            } else {
                insert_or_assign_level(temp.bids_, core::BookSide::Bid, level.price,
                                       level.quantity);
            }
        }
        for (const auto& level : asks) {
            if (level.quantity.value() == 0) {
                erase_price(temp.asks_, core::BookSide::Ask, level.price);
            } else {
                insert_or_assign_level(temp.asks_, core::BookSide::Ask, level.price,
                                       level.quantity);
            }
        }
        *this = std::move(temp);
    }

    void clear() noexcept {
        bids_.clear();
        asks_.clear();
    }

    void clear_side(core::BookSide side) noexcept {
        if (side == core::BookSide::Bid) {
            bids_.clear();
        } else {
            asks_.clear();
        }
    }

    [[nodiscard]] std::vector<core::BookLevel> top_levels(core::BookSide side,
                                                          std::size_t limit) const {
        if (limit == 0) {
            return {};
        }
        return copy_levels(side, limit);
    }

    [[nodiscard]] std::vector<core::BookLevel> all_levels(core::BookSide side) const {
        return copy_levels(side, static_cast<std::size_t>(-1));
    }

  private:
    using LevelVector = std::vector<core::BookLevel, Alloc<core::BookLevel>>;

    // Logarithmic position of `price` in a sorted contiguous side. For bids
    // (descending) this is the first level whose price is not greater than the
    // target; for asks (ascending) the first level whose price is not less than
    // the target. The returned iterator is the insertion point and also
    // validates presence by comparing `price`.
    template <typename LevelIt>
    [[nodiscard]] static LevelIt lower_bound_level(LevelIt begin, LevelIt end, core::BookSide side,
                                                   core::PriceUnits price) {
        if (side == core::BookSide::Bid) {
            return std::lower_bound(begin, end, price,
                                    [](const core::BookLevel& level, core::PriceUnits target) {
                                        return level.price > target;
                                    });
        }
        return std::lower_bound(begin, end, price,
                                [](const core::BookLevel& level, core::PriceUnits target) {
                                    return level.price < target;
                                });
    }

    [[nodiscard]] static LevelVector::iterator find_level(LevelVector& levels, core::BookSide side,
                                                          core::PriceUnits price) noexcept {
        const auto it = lower_bound_level(levels.begin(), levels.end(), side, price);
        if (it != levels.end() && it->price == price) {
            return it;
        }
        return levels.end();
    }

    [[nodiscard]] static LevelVector::const_iterator
    find_level(const LevelVector& levels, core::BookSide side, core::PriceUnits price) noexcept {
        const auto it = lower_bound_level(levels.begin(), levels.end(), side, price);
        if (it != levels.end() && it->price == price) {
            return it;
        }
        return levels.end();
    }

    static void erase_price(LevelVector& levels, core::BookSide side,
                            core::PriceUnits price) noexcept {
        if (levels.empty()) {
            return;
        }
        const auto position = lower_bound_level(levels.begin(), levels.end(), side, price);
        if (position != levels.end() && position->price == price) {
            levels.erase(position);
        }
    }

    static void insert_or_assign_level(LevelVector& levels, core::BookSide side,
                                       core::PriceUnits price, core::QuantityUnits quantity) {
        const auto position = lower_bound_level(levels.begin(), levels.end(), side, price);
        if (position != levels.end() && position->price == price) {
            position->quantity = quantity;
            return;
        }
        levels.insert(position, core::BookLevel{price, quantity});
    }

    [[nodiscard]] core::LevelChange apply_remove(core::BookSide side, core::PriceUnits price) {
        if (side == core::BookSide::Bid) {
            const auto it = find_level(bids_, side, price);
            if (it == bids_.end()) {
                return core::LevelChange::Unchanged;
            }
            bids_.erase(it);
            return core::LevelChange::Removed;
        }
        const auto it = find_level(asks_, side, price);
        if (it == asks_.end()) {
            return core::LevelChange::Unchanged;
        }
        asks_.erase(it);
        return core::LevelChange::Removed;
    }

    [[nodiscard]] core::LevelChange apply_insert_or_update(core::BookSide side,
                                                           core::PriceUnits price,
                                                           core::QuantityUnits quantity) {
        if (side == core::BookSide::Bid) {
            const auto position = lower_bound_level(bids_.begin(), bids_.end(), side, price);
            if (position == bids_.end() || position->price != price) {
                bids_.insert(position, core::BookLevel{price, quantity});
                return core::LevelChange::Inserted;
            }
            if (position->quantity == quantity) {
                return core::LevelChange::Unchanged;
            }
            position->quantity = quantity;
            return core::LevelChange::Updated;
        }
        const auto position = lower_bound_level(asks_.begin(), asks_.end(), side, price);
        if (position == asks_.end() || position->price != price) {
            asks_.insert(position, core::BookLevel{price, quantity});
            return core::LevelChange::Inserted;
        }
        if (position->quantity == quantity) {
            return core::LevelChange::Unchanged;
        }
        position->quantity = quantity;
        return core::LevelChange::Updated;
    }

    [[nodiscard]] std::vector<core::BookLevel> copy_levels(core::BookSide side,
                                                           std::size_t limit) const {
        const auto& levels = side == core::BookSide::Bid ? bids_ : asks_;
        std::vector<core::BookLevel> result;
        result.reserve(std::min(limit, levels.size()));
        for (const auto& level : levels) {
            if (result.size() >= limit) {
                break;
            }
            result.push_back(level);
        }
        return result;
    }

    core::NumericSpec spec_;
    LevelVector bids_;
    LevelVector asks_;
};

static_assert(std::is_nothrow_move_constructible_v<Phase8SortedVectorNaive<>>);
static_assert(std::is_nothrow_move_assignable_v<Phase8SortedVectorNaive<>>);
static_assert(Phase8ModelConcept<Phase8SortedVectorNaive<>>);

} // namespace bmd_projection::m5::phase8