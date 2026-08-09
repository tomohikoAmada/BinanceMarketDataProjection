#include "core_production_side.hpp"

#include "canonical_convert.hpp"
#include "divergence.hpp"
#include "operation_observation.hpp"
#include "production_decimal_observation.hpp"

#include "replay_types.hpp"

#include <binance_market_data/projection/v1/numeric/decimal_scale.hpp>
#include <binance_market_data/projection/v1/order_book/level_update.hpp>
#include <binance_market_data/projection/v1/projection_state/book_projection.hpp>

#include <cstdint>
#include <exception>
#include <iterator>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace bmd_projection::m5::oracle {
namespace {

namespace core = binance_market_data::projection::v1;
namespace replay = bmd_projection::m5::replay;

[[nodiscard]] core::DecimalScale required_scale(std::uint32_t value) {
    const auto scale = core::DecimalScale::create(value);
    if (!scale.has_value()) {
        std::terminate();
    }
    return scale.value();
}

void append_decimals(std::vector<CanonicalDecimalObservation>& destination,
                     std::vector<CanonicalDecimalObservation> source) {
    destination.reserve(destination.size() + source.size());
    destination.insert(destination.end(), std::make_move_iterator(source.begin()),
                       std::make_move_iterator(source.end()));
}

} // namespace

CoreProductionSide::CoreProductionSide(const replay::ReplayFixture& fixture)
    : projection_{{required_scale(fixture.identity.numeric_spec.price_scale),
                   required_scale(fixture.identity.numeric_spec.quantity_scale)},
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
    auto bids = observe_production_levels(bids_input, spec);
    auto asks = observe_production_levels(asks_input, spec);
    std::vector<CanonicalDecimalObservation> decimals;
    append_decimals(decimals, std::move(bids.decimals));
    append_decimals(decimals, std::move(asks.decimals));
    if (bids.first_error.has_value()) {
        return make_observation(DecimalErrorOutcome{bids.first_error.value()}, std::move(decimals));
    }
    if (asks.first_error.has_value()) {
        return make_observation(DecimalErrorOutcome{asks.first_error.value()}, std::move(decimals));
    }
    const auto result =
        projection_.install_baseline({core::UpdateId{last_update_id}, bids.levels, asks.levels});
    return make_observation(to_canonical(result), std::move(decimals));
}

std::optional<OperationObservation>
CoreProductionSide::observe_depth_update(const replay::DepthUpdateOp& operation) {
    const auto spec = projection_.numeric_spec();
    auto parsed = observe_production_levels(operation.levels, spec);
    if (parsed.first_error.has_value()) {
        return make_observation(DecimalErrorOutcome{parsed.first_error.value()},
                                std::move(parsed.decimals));
    }
    if (operation.first_update_id > operation.final_update_id) {
        return make_observation(RangeOutcome{false}, std::move(parsed.decimals));
    }
    std::vector<core::LevelUpdate> levels;
    levels.reserve(parsed.levels.size());
    for (std::size_t index = 0; index < operation.levels.size(); ++index) {
        const auto& level = operation.levels.at(index);
        const auto& book_level = parsed.levels.at(index);
        levels.push_back(
            {level.side == replay::Side::Bid ? core::BookSide::Bid : core::BookSide::Ask,
             book_level.price, book_level.quantity});
    }
    const auto range = core::UpdateRange::try_create(core::UpdateId{operation.first_update_id},
                                                     core::UpdateId{operation.final_update_id});
    if (!range.has_value()) {
        return make_observation(RangeOutcome{false}, std::move(parsed.decimals));
    }
    std::optional<core::UpdateId> previous;
    if (operation.previous_final.has_value()) {
        previous = core::UpdateId{operation.previous_final.value()};
    }
    const auto result = projection_.apply({range.value(), previous, levels});
    return make_observation(to_canonical(result), std::move(parsed.decimals));
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
    const auto last_update_id = projection_.last_update_id();
    if (last_update_id.has_value()) {
        result.last_update_id = last_update_id->value();
    }
    const auto last_gap = projection_.last_gap();
    if (last_gap.has_value()) {
        result.last_gap = to_canonical(*last_gap);
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
CoreProductionSide::make_observation(OperationResultValue result,
                                     std::vector<CanonicalDecimalObservation> decimals) const {
    return OperationObservation{0,
                                replay::EventKind::InstallBaseline,
                                std::move(decimals),
                                OperationResult{std::move(result)},
                                checkpoint(),
                                std::nullopt};
}

std::unique_ptr<ReplaySide> make_core_production_side(const replay::ReplayFixture& fixture) {
    return std::make_unique<CoreProductionSide>(fixture);
}

} // namespace bmd_projection::m5::oracle
