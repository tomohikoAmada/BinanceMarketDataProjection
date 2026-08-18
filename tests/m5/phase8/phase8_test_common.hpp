#pragma once

// M5 Phase-8 conformance-test shared helpers (PR-A / WP1). Test-only; never
// part of the installed surface. Provides deterministic value construction and
// cross-oracle state comparison helpers shared by the typed Phase-8 suites.

#include <gtest/gtest.h>

#include "phase8_model.hpp"

#include <binance_market_data/projection/v1/order_book/order_book.hpp>

#include "reference_order_book.hpp"

#include <array>
#include <cstdint>
#include <initializer_list>
#include <vector>

namespace bmd_projection::m5::phase8::test {

// NOLINTBEGIN(bugprone-unchecked-optional-access)
inline core::PriceUnits P(std::int64_t value) { return core::PriceUnits::create(value).value(); }
inline core::QuantityUnits Q(std::int64_t value) {
    return core::QuantityUnits::create(value).value();
}
inline core::BookLevel L(std::int64_t price, std::int64_t quantity) {
    return core::BookLevel{P(price), Q(quantity)};
}
inline core::LevelUpdate U(core::BookSide side, std::int64_t price, std::int64_t quantity) {
    return core::LevelUpdate{side, P(price), Q(quantity)};
}
inline core::NumericSpec TestSpec() {
    return core::NumericSpec{core::DecimalScale::create(8).value(),
                             core::DecimalScale::create(4).value()};
}
// NOLINTEND(bugprone-unchecked-optional-access)

inline std::vector<core::BookLevel> Levels(std::initializer_list<core::BookLevel> values) {
    return {values};
}

inline std::vector<core::LevelUpdate> Updates(std::initializer_list<core::LevelUpdate> values) {
    return {values};
}

// Full observable state comparison between a Phase-8 model and the independent
// ReferenceOrderBook oracle. A candidate with ordering or quantity bugs must
// fail here; total-count-only comparison is never sufficient.
template <typename Model>
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ExpectModelMatchesReference(const Model& model, const bmd_test::ReferenceOrderBook& ref) {
    const bool reference_empty =
        ref.level_count(core::BookSide::Bid) == 0 && ref.level_count(core::BookSide::Ask) == 0;
    EXPECT_EQ(model.empty(), reference_empty) << "empty mismatch";
    for (const auto side : {core::BookSide::Bid, core::BookSide::Ask}) {
        EXPECT_EQ(model.level_count(side), ref.level_count(side)) << "level_count side mismatch";
        EXPECT_EQ(model.all_levels(side), ref.all_levels(side)) << "all_levels side mismatch";
        EXPECT_EQ(model.best_bid(), ref.best_bid());
        EXPECT_EQ(model.best_ask(), ref.best_ask());
    }
}

template <typename Model>
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ExpectModelMatchesCore(const Model& model, const core::OrderBook& book) {
    EXPECT_EQ(model.numeric_spec(), book.numeric_spec()) << "numeric_spec mismatch";
    EXPECT_EQ(model.empty(), book.empty()) << "empty mismatch";
    for (const auto side : {core::BookSide::Bid, core::BookSide::Ask}) {
        EXPECT_EQ(model.level_count(side), book.level_count(side)) << "level_count side mismatch";
        EXPECT_EQ(model.all_levels(side), book.all_levels(side)) << "all_levels side mismatch";
        EXPECT_EQ(model.best_bid(), book.best_bid());
        EXPECT_EQ(model.best_ask(), book.best_ask());
        EXPECT_EQ(model.top_levels(side, 3), book.top_levels(side, 3)) << "top_levels(3) mismatch";
    }
    constexpr std::array<std::int64_t, 9> kProbePrices{1, 5, 10, 20, 50, 100, 500, 1000, 12345};
    for (const auto price : kProbePrices) {
        EXPECT_EQ(model.quantity_at(core::BookSide::Bid, P(price)),
                  book.quantity_at(core::BookSide::Bid, P(price)))
            << "bid quantity_at probe mismatch at price " << price;
        EXPECT_EQ(model.quantity_at(core::BookSide::Ask, P(price)),
                  book.quantity_at(core::BookSide::Ask, P(price)))
            << "ask quantity_at probe mismatch at price " << price;
    }
}

} // namespace bmd_projection::m5::phase8::test