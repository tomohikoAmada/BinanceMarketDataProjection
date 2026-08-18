#pragma once

// M5 Phase-8 candidate model: Phase8AbslBtreeMap (PR-A / WP1).
//
// Benchmark-only absl::btree_map model behind the shared Phase-8 model
// interface, with the same bid/ask ordering semantics as the std::map control
// (bids descending via std::greater<>, asks ascending via std::less<>).
//
// Abseil identity is pinned by the current dependency graph
// (abseil/20260107.1, RREV 883e95a7be2b767999f64669ac642e5d). Abseil is an
// explicit TEST/BENCHMARK-only requirement; it never becomes a Core/
// exported-package/public-header/installed-consumer requirement.
//
// The bid and ask members are distinct types (their ordered comparator types
// differ), so every side-selecting path is written explicitly per side.

#include <binance_market_data/projection/v1/numeric/numeric_spec.hpp>
#include <binance_market_data/projection/v1/numeric/price_units.hpp>
#include <binance_market_data/projection/v1/numeric/quantity_units.hpp>
#include <binance_market_data/projection/v1/order_book/book_level.hpp>
#include <binance_market_data/projection/v1/order_book/book_side.hpp>
#include <binance_market_data/projection/v1/order_book/level_update.hpp>
#include <binance_market_data/projection/v1/order_book/order_book.hpp>

#include "phase8_model.hpp"

#include <absl/container/btree_map.h>

#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

namespace bmd_projection::m5::phase8 {

template <template <typename> class Alloc = std::allocator> class Phase8AbslBtreeMap {
  public:
    explicit Phase8AbslBtreeMap(core::NumericSpec spec) noexcept : spec_(spec) {}

    Phase8AbslBtreeMap(const Phase8AbslBtreeMap&) = delete;
    Phase8AbslBtreeMap& operator=(const Phase8AbslBtreeMap&) = delete;
    Phase8AbslBtreeMap(Phase8AbslBtreeMap&&) noexcept = default;
    Phase8AbslBtreeMap& operator=(Phase8AbslBtreeMap&&) noexcept = default;
    ~Phase8AbslBtreeMap() = default;

    static constexpr std::string_view model_id() noexcept { return kPhase8AbslBtreeMapId; }

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
        const auto it = bids_.begin();
        return core::BookLevel{it->first, it->second};
    }

    [[nodiscard]] std::optional<core::BookLevel> best_ask() const noexcept {
        if (asks_.empty()) {
            return std::nullopt;
        }
        const auto it = asks_.begin();
        return core::BookLevel{it->first, it->second};
    }

    [[nodiscard]] std::optional<core::QuantityUnits>
    quantity_at(core::BookSide side, core::PriceUnits price) const noexcept {
        if (side == core::BookSide::Bid) {
            const auto it = bids_.find(price);
            if (it == bids_.end()) {
                return std::nullopt;
            }
            return it->second;
        }
        const auto it = asks_.find(price);
        if (it == asks_.end()) {
            return std::nullopt;
        }
        return it->second;
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
        Phase8AbslBtreeMap temp(spec_);
        for (const auto& level : bids) {
            if (level.quantity.value() == 0) {
                temp.bids_.erase(level.price);
            } else {
                temp.bids_.insert_or_assign(level.price, level.quantity);
            }
        }
        for (const auto& level : asks) {
            if (level.quantity.value() == 0) {
                temp.asks_.erase(level.price);
            } else {
                temp.asks_.insert_or_assign(level.price, level.quantity);
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
    using MapValue = std::pair<const core::PriceUnits, core::QuantityUnits>;
    using BidMap =
        absl::btree_map<core::PriceUnits, core::QuantityUnits, std::greater<>, Alloc<MapValue>>;
    using AskMap =
        absl::btree_map<core::PriceUnits, core::QuantityUnits, std::less<>, Alloc<MapValue>>;

    [[nodiscard]] core::LevelChange apply_remove(core::BookSide side,
                                                 core::PriceUnits price) noexcept {
        if (side == core::BookSide::Bid) {
            const auto it = bids_.find(price);
            if (it == bids_.end()) {
                return core::LevelChange::Unchanged;
            }
            bids_.erase(it);
            return core::LevelChange::Removed;
        }
        const auto it = asks_.find(price);
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
            const auto it = bids_.find(price);
            if (it == bids_.end()) {
                bids_.insert_or_assign(price, quantity);
                return core::LevelChange::Inserted;
            }
            if (it->second == quantity) {
                return core::LevelChange::Unchanged;
            }
            it->second = quantity;
            return core::LevelChange::Updated;
        }
        const auto it = asks_.find(price);
        if (it == asks_.end()) {
            asks_.insert_or_assign(price, quantity);
            return core::LevelChange::Inserted;
        }
        if (it->second == quantity) {
            return core::LevelChange::Unchanged;
        }
        it->second = quantity;
        return core::LevelChange::Updated;
    }

    template <typename LevelMap>
    [[nodiscard]] std::vector<core::BookLevel> copy_from_map(const LevelMap& map,
                                                             std::size_t limit) const {
        std::vector<core::BookLevel> result;
        for (const auto& [price, quantity] : map) {
            if (result.size() >= limit) {
                break;
            }
            result.push_back(core::BookLevel{price, quantity});
        }
        return result;
    }

    [[nodiscard]] std::vector<core::BookLevel> copy_levels(core::BookSide side,
                                                           std::size_t limit) const {
        if (side == core::BookSide::Bid) {
            return copy_from_map(bids_, limit);
        }
        return copy_from_map(asks_, limit);
    }

    core::NumericSpec spec_;
    BidMap bids_;
    AskMap asks_;
};

static_assert(std::is_nothrow_move_constructible_v<Phase8AbslBtreeMap<>>);
static_assert(std::is_nothrow_move_assignable_v<Phase8AbslBtreeMap<>>);
static_assert(Phase8ModelConcept<Phase8AbslBtreeMap<>>);

} // namespace bmd_projection::m5::phase8