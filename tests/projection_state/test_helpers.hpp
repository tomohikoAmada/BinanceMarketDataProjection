#pragma once

#include <binance_market_data/projection/v1/projection_state/book_projection.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace bmd_projection_test {

namespace bmd = binance_market_data::projection::v1;

// Test factories use only literal values whose validity is asserted by their focused tests.
// NOLINTBEGIN(bugprone-unchecked-optional-access)

[[nodiscard]] inline bmd::DecimalScale scale(std::uint32_t value = 8) {
    return bmd::DecimalScale::create(value).value();
}

[[nodiscard]] inline bmd::NumericSpec spec() { return {scale(), scale()}; }

[[nodiscard]] inline bmd::PriceUnits price(std::int64_t value) {
    return bmd::PriceUnits::create(value).value();
}

[[nodiscard]] inline bmd::QuantityUnits quantity(std::int64_t value) {
    return bmd::QuantityUnits::create(value).value();
}

[[nodiscard]] inline bmd::UpdateRange range(std::uint64_t first, std::uint64_t final) {
    return bmd::UpdateRange::try_create(bmd::UpdateId{first}, bmd::UpdateId{final}).value();
}

[[nodiscard]] inline bmd::InstallResult install(bmd::BookProjection& projection,
                                                std::uint64_t last_update_id,
                                                std::span<const bmd::BookLevel> bids = {},
                                                std::span<const bmd::BookLevel> asks = {}) {
    return projection.install_baseline({bmd::UpdateId{last_update_id}, bids, asks});
}

[[nodiscard]] inline bmd::ApplyResult
apply(bmd::BookProjection& projection, std::uint64_t first, std::uint64_t final,
      std::optional<std::uint64_t> previous_final = std::nullopt,
      std::span<const bmd::LevelUpdate> levels = {}) {
    std::optional<bmd::UpdateId> previous;
    if (previous_final.has_value()) {
        previous = bmd::UpdateId{previous_final.value()};
    }
    return projection.apply({range(first, final), previous, levels});
}

struct ProjectionCheckpoint final {
    bmd::ProjectionStatus status;
    std::optional<bmd::UpdateId> last_update_id;
    std::optional<bmd::GapInfo> last_gap;
    bool synchronized_visible;
    std::vector<bmd::BookLevel> bids;
    std::vector<bmd::BookLevel> asks;

    friend bool operator==(const ProjectionCheckpoint&, const ProjectionCheckpoint&) = default;
};

[[nodiscard]] inline ProjectionCheckpoint checkpoint(const bmd::BookProjection& projection) {
    return {
        projection.status(),
        projection.last_update_id(),
        projection.last_gap(),
        projection.synchronized_book().has_value(),
        projection.diagnostic_book().all_levels(bmd::BookSide::Bid),
        projection.diagnostic_book().all_levels(bmd::BookSide::Ask),
    };
}

// NOLINTEND(bugprone-unchecked-optional-access)

} // namespace bmd_projection_test
