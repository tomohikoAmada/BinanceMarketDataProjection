#pragma once

// Production-side M1 evidence collection. This test-only helper calls only the
// production M1 public API and records every replay price/quantity token before
// later M3/M4 behavior can hide a successful parse divergence.

#include "operation_observation.hpp"

#include "replay_types.hpp"

#include <binance_market_data/projection/v1/numeric/numeric_spec.hpp>
#include <binance_market_data/projection/v1/order_book/book_level.hpp>

#include <optional>
#include <vector>

namespace bmd_projection::m5::oracle {

struct ProductionLevelObservation final {
    std::vector<CanonicalDecimalObservation> decimals;
    std::vector<binance_market_data::projection::v1::BookLevel> levels;
    std::optional<CanonicalDecimalError> first_error;
};

[[nodiscard]] ProductionLevelObservation
observe_production_levels(const std::vector<replay::LevelInput>& levels,
                          binance_market_data::projection::v1::NumericSpec numeric_spec);

} // namespace bmd_projection::m5::oracle
