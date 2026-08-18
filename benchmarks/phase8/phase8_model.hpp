#pragma once

// M5 Phase-8 container-model substrate (PR-A / WP1).
//
// This header defines the benchmark/test-only Phase-8 model protocol that every
// candidate container model must satisfy. It intentionally mirrors the
// observable production core::OrderBook surface (src/order_book/order_book.cpp)
// restricted to the subset needed to run the accepted M2 semantics, but it is
// NOT production code: the models live under benchmarks/ and are never
// installed or exported.
//
// Design intent (OD-M5-P6-027): Phase-8 candidate comparisons must later use
// the same candidate-model interface/environment under one denominator
// (Phase8Candidate / Phase8StdMapControl). This header is that single
// interface. It carries no numeric comparison logic and no decision matrix --
// those belong to later Phase-8 work packages.

#include <binance_market_data/projection/v1/numeric/decimal_scale.hpp>
#include <binance_market_data/projection/v1/numeric/numeric_spec.hpp>
#include <binance_market_data/projection/v1/numeric/price_units.hpp>
#include <binance_market_data/projection/v1/numeric/quantity_units.hpp>
#include <binance_market_data/projection/v1/order_book/book_level.hpp>
#include <binance_market_data/projection/v1/order_book/book_side.hpp>
#include <binance_market_data/projection/v1/order_book/level_update.hpp>
#include <binance_market_data/projection/v1/order_book/order_book.hpp>

#include <concepts>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

namespace bmd_projection::m5::phase8 {

namespace core = binance_market_data::projection::v1;

// Stable internal model identifiers. These identify the implementation model
// only; they never redefine the accepted Phase-6 M5_BENCHMARK_WORKLOAD_SPEC_V1
// workload identities.
inline constexpr std::string_view kPhase8StdMapControlId = "phase8-std-map-control-v1";
inline constexpr std::string_view kPhase8SortedVectorNaiveId = "phase8-sorted-vector-naive-v1";
inline constexpr std::string_view kPhase8AbslBtreeMapId = "phase8-absl-btree-map-v1";
inline constexpr std::string_view kPhase8SortedVectorBatchLwwId =
    "phase8-sorted-vector-batch-lww-v1";

// Compile-time protocol gate for every Phase-8 candidate model.
//
// The observable surface mirrors core::OrderBook (per-level replacement,
// quantity==0 deletion, deterministic ordering, replace_all strong guarantee
// structure, top_levels/all_levels limits) with static polymorphism: no
// virtual dispatch exists on the measured candidate path.
template <typename Model>
concept Phase8ModelConcept =
    requires(Model& model, const Model& const_model, core::BookSide side, core::PriceUnits price,
             core::QuantityUnits quantity, std::span<const core::LevelUpdate> updates,
             std::span<const core::BookLevel> bids, std::span<const core::BookLevel> asks,
             std::size_t limit) {
        // NumericSpec ownership: models are move-only like core::OrderBook.
        requires std::is_constructible_v<Model, core::NumericSpec>;
        requires std::is_nothrow_move_constructible_v<Model>;
        requires std::is_nothrow_move_assignable_v<Model>;
        requires !std::is_copy_constructible_v<Model>;
        requires !std::is_copy_assignable_v<Model>;

        { const_model.numeric_spec() } -> std::same_as<core::NumericSpec>;
        { const_model.empty() } -> std::same_as<bool>;
        { const_model.side_empty(side) } -> std::same_as<bool>;
        { const_model.level_count(side) } -> std::same_as<std::size_t>;
        { const_model.best_bid() } -> std::same_as<std::optional<core::BookLevel>>;
        { const_model.best_ask() } -> std::same_as<std::optional<core::BookLevel>>;
        {
            const_model.quantity_at(side, price)
        } -> std::same_as<std::optional<core::QuantityUnits>>;
        { const_model.top_levels(side, limit) } -> std::same_as<std::vector<core::BookLevel>>;
        { const_model.all_levels(side) } -> std::same_as<std::vector<core::BookLevel>>;

        { model.apply_level(side, price, quantity) } -> std::same_as<core::LevelChange>;
        { model.apply_updates(updates) } -> std::same_as<void>;
        { model.replace_all(bids, asks) } -> std::same_as<void>;
        { model.clear() } -> std::same_as<void>;
        { model.clear_side(side) } -> std::same_as<void>;
    };

} // namespace bmd_projection::m5::phase8