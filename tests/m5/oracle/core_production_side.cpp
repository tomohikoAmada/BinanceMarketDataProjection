#include "core_production_side.hpp"

#include "canonical_convert.hpp"
#include "divergence.hpp"
#include "operation_observation.hpp"

#include "replay_types.hpp"

#include <binance_market_data/projection/v1/numeric/decimal_parse.hpp>
#include <binance_market_data/projection/v1/numeric/decimal_scale.hpp>
#include <binance_market_data/projection/v1/order_book/book_level.hpp>
#include <binance_market_data/projection/v1/order_book/level_update.hpp>
#include <binance_market_data/projection/v1/projection_state/book_projection.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace bmd_projection::m5::oracle {
namespace {

namespace core = binance_market_data::projection::v1;
namespace replay = bmd_projection::m5::replay;

// Parses one level token pair in input order. Returns nullopt on success and the
// canonical decimal failure category on the first failing token.
[[nodiscard]] std::optional<CanonicalDecimalError> parse_level(const replay::LevelInput& level,
                                                               core::NumericSpec spec) {
    const auto price = core::parse_price(level.price, spec.price_scale);
    if (const auto* error = std::get_if<core::DecimalError>(&price)) {
        return to_canonical(error->code);
    }
    const auto quantity = core::parse_quantity(level.quantity, spec.quantity_scale);
    if (const auto* error = std::get_if<core::DecimalError>(&quantity)) {
        return to_canonical(error->code);
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<CanonicalDecimalError>
parse_levels(const std::vector<replay::LevelInput>& levels, core::NumericSpec spec) {
    for (const auto& level : levels) {
        if (const auto failure = parse_level(level, spec)) {
            return failure;
        }
    }
    return std::nullopt;
}

[[nodiscard]] core::BookLevel make_book_level(const replay::LevelInput& level,
                                              core::NumericSpec spec) {
    return {
        std::get<core::ParsedDecimal<core::PriceUnits>>(
            core::parse_price(level.price, spec.price_scale))
            .value,
        std::get<core::ParsedDecimal<core::QuantityUnits>>(
            core::parse_quantity(level.quantity, spec.quantity_scale))
            .value,
    };
}

} // namespace

CoreProductionSide::CoreProductionSide(const replay::ReplayFixture& fixture)
    : projection_{
          {core::DecimalScale::create(fixture.identity.numeric_spec.price_scale).value(),
           core::DecimalScale::create(fixture.identity.numeric_spec.quantity_scale).value()},
          fixture.identity.sequence_policy == replay::SequencePolicy::Spot
              ? core::SequencePolicyKind::Spot
              : core::SequencePolicyKind::UsdMPerpetual} {}

std::optional<OperationObservation>
CoreProductionSide::observe(const replay::Operation& operation) {
    if (const auto* op = std::get_if<replay::InstallBaselineOp>(&operation)) {
        return observe_install(op->last_update_id, op->bids, op->asks);
    }
    if (const auto* op = std::get_if<replay::DepthUpdateOp>(&operation)) {
        return observe_depth_update(*op);
    }
    if (const auto* op = std::get_if<replay::RebaselineOp>(&operation)) {
        return observe_install(op->last_update_id, op->bids, op->asks);
    }
    if (std::holds_alternative<replay::ResetOp>(operation)) {
        return observe_reset();
    }
    if (std::holds_alternative<replay::SnapshotRequestOp>(operation)) {
        return observe_snapshot_request();
    }
    if (std::holds_alternative<replay::AdapterMetadataOp>(operation)) {
        return observe_metadata();
    }
    return observe_malformed_range(std::get<replay::MalformedRangeOp>(operation));
}

std::optional<OperationObservation>
CoreProductionSide::observe_install(std::uint64_t last_update_id,
                                    const std::vector<replay::LevelInput>& bids_input,
                                    const std::vector<replay::LevelInput>& asks_input) {
    const auto spec = projection_.numeric_spec();
    if (const auto failure = parse_levels(bids_input, spec)) {
        return make_observation(DecimalErrorOutcome{*failure});
    }
    if (const auto failure = parse_levels(asks_input, spec)) {
        return make_observation(DecimalErrorOutcome{*failure});
    }
    std::vector<core::BookLevel> bids;
    bids.reserve(bids_input.size());
    for (const auto& level : bids_input) {
        bids.push_back(make_book_level(level, spec));
    }
    std::vector<core::BookLevel> asks;
    asks.reserve(asks_input.size());
    for (const auto& level : asks_input) {
        asks.push_back(make_book_level(level, spec));
    }
    const auto result = projection_.install_baseline({core::UpdateId{last_update_id}, bids, asks});
    return make_observation(to_canonical(result));
}

std::optional<OperationObservation>
CoreProductionSide::observe_depth_update(const replay::DepthUpdateOp& operation) {
    const auto spec = projection_.numeric_spec();
    if (const auto failure = parse_levels(operation.levels, spec)) {
        return make_observation(DecimalErrorOutcome{*failure});
    }
    if (operation.first_update_id > operation.final_update_id) {
        return make_observation(RangeOutcome{false});
    }
    std::vector<core::LevelUpdate> levels;
    levels.reserve(operation.levels.size());
    for (const auto& level : operation.levels) {
        const auto book_level = make_book_level(level, spec);
        levels.push_back(
            {level.side == replay::Side::Bid ? core::BookSide::Bid : core::BookSide::Ask,
             book_level.price, book_level.quantity});
    }
    const auto range = core::UpdateRange::try_create(core::UpdateId{operation.first_update_id},
                                                     core::UpdateId{operation.final_update_id});
    if (!range.has_value()) {
        return make_observation(RangeOutcome{false});
    }
    std::optional<core::UpdateId> previous;
    if (operation.previous_final.has_value()) {
        previous = core::UpdateId{*operation.previous_final};
    }
    const auto result = projection_.apply({*range, previous, levels});
    return make_observation(to_canonical(result));
}

std::optional<OperationObservation> CoreProductionSide::observe_snapshot_request() {
    return make_observation(SnapshotNotProducedOutcome{});
}

std::optional<OperationObservation> CoreProductionSide::observe_reset() {
    projection_.reset();
    return make_observation(ResetOutcome{});
}

std::optional<OperationObservation> CoreProductionSide::observe_metadata() {
    return make_observation(MetadataOutcome{});
}

std::optional<OperationObservation>
CoreProductionSide::observe_malformed_range(const replay::MalformedRangeOp& operation) {
    const auto range = core::UpdateRange::try_create(core::UpdateId{operation.first_update_id},
                                                     core::UpdateId{operation.final_update_id});
    return make_observation(RangeOutcome{!range.has_value()});
}

SemanticCheckpoint CoreProductionSide::checkpoint() const {
    SemanticCheckpoint result;
    result.status = to_canonical(projection_.status());
    if (projection_.last_update_id().has_value()) {
        result.last_update_id = projection_.last_update_id()->value();
    }
    if (projection_.last_gap().has_value()) {
        result.last_gap = to_canonical(*projection_.last_gap());
    }
    result.synchronized_visible = projection_.synchronized_book().has_value();
    const auto& book = projection_.diagnostic_book();
    for (const auto& level : book.all_levels(core::BookSide::Bid)) {
        result.bids.push_back({level.price.value(), level.quantity.value()});
    }
    for (const auto& level : book.all_levels(core::BookSide::Ask)) {
        result.asks.push_back({level.price.value(), level.quantity.value()});
    }
    result.price_scale = projection_.numeric_spec().price_scale.value();
    result.quantity_scale = projection_.numeric_spec().quantity_scale.value();
    return result;
}

std::optional<OperationObservation>
CoreProductionSide::make_observation(OperationResultValue result) const {
    return OperationObservation{0, replay::EventKind::InstallBaseline,
                                OperationResult{std::move(result)}, checkpoint(), std::nullopt};
}

std::unique_ptr<ReplaySide> make_core_production_side(const replay::ReplayFixture& fixture) {
    return std::make_unique<CoreProductionSide>(fixture);
}

} // namespace bmd_projection::m5::oracle
