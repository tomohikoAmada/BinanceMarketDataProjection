#include "reference_side.hpp"

#include "divergence.hpp"
#include "operation_observation.hpp"

#include "reference_adapter.hpp"
#include "reference_decimal.hpp"
#include "reference_projection.hpp"

#include "replay_types.hpp"

#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace bmd_projection::m5::oracle {
namespace {

namespace ref = bmd_projection::m5::reference;
namespace reference = bmd_projection_reference;
namespace replay = bmd_projection::m5::replay;

[[nodiscard]] CanonicalPolicy canonical(reference::Policy policy) noexcept {
    switch (policy) {
    case reference::Policy::Spot:
        return CanonicalPolicy::Spot;
    case reference::Policy::UsdM:
        return CanonicalPolicy::UsdMPerpetual;
    }
    return CanonicalPolicy::Spot;
}

[[nodiscard]] CanonicalStatus canonical(reference::Status status) noexcept {
    switch (status) {
    case reference::Status::AwaitingBaseline:
        return CanonicalStatus::AwaitingBaseline;
    case reference::Status::AwaitingBridge:
        return CanonicalStatus::AwaitingBridge;
    case reference::Status::Synchronized:
        return CanonicalStatus::Synchronized;
    case reference::Status::NeedsResync:
        return CanonicalStatus::NeedsResync;
    }
    return CanonicalStatus::AwaitingBaseline;
}

[[nodiscard]] CanonicalDisposition canonical(reference::InstallDisposition disposition) noexcept {
    switch (disposition) {
    case reference::InstallDisposition::Installed:
        return CanonicalDisposition::Installed;
    case reference::InstallDisposition::RejectedWrongState:
        return CanonicalDisposition::RejectedWrongState;
    }
    return CanonicalDisposition::RejectedWrongState;
}

[[nodiscard]] CanonicalDisposition canonical(reference::Disposition disposition) noexcept {
    switch (disposition) {
    case reference::Disposition::Applied:
        return CanonicalDisposition::Applied;
    case reference::Disposition::IgnoredStale:
        return CanonicalDisposition::IgnoredStale;
    case reference::Disposition::IgnoredDuplicate:
        return CanonicalDisposition::IgnoredDuplicate;
    case reference::Disposition::GapDetected:
        return CanonicalDisposition::GapDetected;
    case reference::Disposition::RejectedWrongState:
        return CanonicalDisposition::RejectedWrongState;
    }
    return CanonicalDisposition::RejectedWrongState;
}

[[nodiscard]] CanonicalGapReason canonical(reference::GapReason reason) noexcept {
    switch (reason) {
    case reference::GapReason::SpotBootstrapForwardGap:
        return CanonicalGapReason::SpotBootstrapForwardGap;
    case reference::GapReason::SpotLiveForwardGap:
        return CanonicalGapReason::SpotLiveForwardGap;
    case reference::GapReason::FuturesBootstrapRangeMiss:
        return CanonicalGapReason::FuturesBootstrapRangeMiss;
    case reference::GapReason::FuturesMissingPreviousFinal:
        return CanonicalGapReason::FuturesMissingPreviousFinal;
    case reference::GapReason::FuturesPreviousFinalMismatch:
        return CanonicalGapReason::FuturesPreviousFinalMismatch;
    }
    return CanonicalGapReason::SpotBootstrapForwardGap;
}

[[nodiscard]] CanonicalGapEvidence canonical(const reference::Gap& gap) noexcept {
    return {
        gap.last, gap.first, gap.final, gap.previous, canonical(gap.reason), canonical(gap.policy)};
}

[[nodiscard]] InstallOutcome canonical(const reference::InstallResult& result) noexcept {
    return {canonical(result.disposition), canonical(result.status), result.last};
}

[[nodiscard]] ApplyOutcome canonical(const reference::Result& result) noexcept {
    return {canonical(result.disposition), canonical(result.status), result.last,
            result.gap.has_value() ? std::optional<CanonicalGapEvidence>{canonical(*result.gap)}
                                   : std::nullopt};
}

[[nodiscard]] CanonicalDecimalError canonical(ref::ReferenceDecimalErrorCode code) noexcept {
    switch (code) {
    case ref::ReferenceDecimalErrorCode::Empty:
        return CanonicalDecimalError::Empty;
    case ref::ReferenceDecimalErrorCode::InvalidSyntax:
        return CanonicalDecimalError::InvalidSyntax;
    case ref::ReferenceDecimalErrorCode::SignNotAllowed:
        return CanonicalDecimalError::SignNotAllowed;
    case ref::ReferenceDecimalErrorCode::LeadingZero:
        return CanonicalDecimalError::LeadingZero;
    case ref::ReferenceDecimalErrorCode::MissingFractionDigits:
        return CanonicalDecimalError::MissingFractionDigits;
    case ref::ReferenceDecimalErrorCode::ZeroNotAllowed:
        return CanonicalDecimalError::ZeroNotAllowed;
    case ref::ReferenceDecimalErrorCode::InexactScale:
        return CanonicalDecimalError::InexactScale;
    case ref::ReferenceDecimalErrorCode::Overflow:
        return CanonicalDecimalError::Overflow;
    }
    return CanonicalDecimalError::InvalidSyntax;
}

[[nodiscard]] CanonicalAdapterCode canonical(ref::ReferenceAdapterErrorCode code) noexcept {
    switch (code) {
    case ref::ReferenceAdapterErrorCode::UnsupportedVenue:
        return CanonicalAdapterCode::UnsupportedVenue;
    case ref::ReferenceAdapterErrorCode::UnsupportedMarket:
        return CanonicalAdapterCode::UnsupportedMarket;
    case ref::ReferenceAdapterErrorCode::UnexpectedStream:
        return CanonicalAdapterCode::UnexpectedStream;
    case ref::ReferenceAdapterErrorCode::IdentityMismatch:
        return CanonicalAdapterCode::IdentityMismatch;
    case ref::ReferenceAdapterErrorCode::UnsupportedSchemaVersion:
        return CanonicalAdapterCode::UnsupportedSchemaVersion;
    case ref::ReferenceAdapterErrorCode::UnspecifiedEnum:
        return CanonicalAdapterCode::UnspecifiedEnum;
    case ref::ReferenceAdapterErrorCode::UnknownEnumValue:
        return CanonicalAdapterCode::UnknownEnumValue;
    case ref::ReferenceAdapterErrorCode::InvalidUpdateRange:
        return CanonicalAdapterCode::InvalidUpdateRange;
    case ref::ReferenceAdapterErrorCode::MissingRequiredField:
        return CanonicalAdapterCode::MissingRequiredField;
    case ref::ReferenceAdapterErrorCode::InvalidIdentifier:
        return CanonicalAdapterCode::InvalidIdentifier;
    case ref::ReferenceAdapterErrorCode::InvalidDecimal:
        return CanonicalAdapterCode::InvalidDecimal;
    case ref::ReferenceAdapterErrorCode::NegativeQuantity:
        return CanonicalAdapterCode::NegativeQuantity;
    case ref::ReferenceAdapterErrorCode::NonPositivePrice:
        return CanonicalAdapterCode::NonPositivePrice;
    case ref::ReferenceAdapterErrorCode::ScaleMismatch:
        return CanonicalAdapterCode::ScaleMismatch;
    case ref::ReferenceAdapterErrorCode::NumericOverflow:
        return CanonicalAdapterCode::NumericOverflow;
    case ref::ReferenceAdapterErrorCode::InvalidDepthLimit:
        return CanonicalAdapterCode::InvalidDepthLimit;
    case ref::ReferenceAdapterErrorCode::InvalidOrdering:
        return CanonicalAdapterCode::InvalidOrdering;
    case ref::ReferenceAdapterErrorCode::UnsupportedProjectionState:
        return CanonicalAdapterCode::UnsupportedProjectionState;
    case ref::ReferenceAdapterErrorCode::MissingLastUpdateId:
        return CanonicalAdapterCode::MissingLastUpdateId;
    case ref::ReferenceAdapterErrorCode::InvalidGapContext:
        return CanonicalAdapterCode::InvalidGapContext;
    case ref::ReferenceAdapterErrorCode::InvalidHostQualityCombination:
        return CanonicalAdapterCode::InvalidHostQualityCombination;
    case ref::ReferenceAdapterErrorCode::ProjectionNumericSpecMismatch:
        return CanonicalAdapterCode::ProjectionNumericSpecMismatch;
    case ref::ReferenceAdapterErrorCode::ProjectionPolicyMismatch:
        return CanonicalAdapterCode::ProjectionPolicyMismatch;
    }
    return CanonicalAdapterCode::InvalidDecimal;
}

// Linear enum-name mapping table over the closed 25-value adapter field domain.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
[[nodiscard]] CanonicalAdapterField canonical(ref::ReferenceAdapterField field) noexcept {
    switch (field) {
    case ref::ReferenceAdapterField::None:
        return CanonicalAdapterField::None;
    case ref::ReferenceAdapterField::Venue:
        return CanonicalAdapterField::Venue;
    case ref::ReferenceAdapterField::Market:
        return CanonicalAdapterField::Market;
    case ref::ReferenceAdapterField::Stream:
        return CanonicalAdapterField::Stream;
    case ref::ReferenceAdapterField::Symbol:
        return CanonicalAdapterField::Symbol;
    case ref::ReferenceAdapterField::SchemaVersion:
        return CanonicalAdapterField::SchemaVersion;
    case ref::ReferenceAdapterField::Producer:
        return CanonicalAdapterField::Producer;
    case ref::ReferenceAdapterField::ProducerVersion:
        return CanonicalAdapterField::ProducerVersion;
    case ref::ReferenceAdapterField::RequestId:
        return CanonicalAdapterField::RequestId;
    case ref::ReferenceAdapterField::ConnectionId:
        return CanonicalAdapterField::ConnectionId;
    case ref::ReferenceAdapterField::FirstUpdateId:
        return CanonicalAdapterField::FirstUpdateId;
    case ref::ReferenceAdapterField::FinalUpdateId:
        return CanonicalAdapterField::FinalUpdateId;
    case ref::ReferenceAdapterField::PreviousFinalUpdateId:
        return CanonicalAdapterField::PreviousFinalUpdateId;
    case ref::ReferenceAdapterField::BidPrice:
        return CanonicalAdapterField::BidPrice;
    case ref::ReferenceAdapterField::BidQuantity:
        return CanonicalAdapterField::BidQuantity;
    case ref::ReferenceAdapterField::AskPrice:
        return CanonicalAdapterField::AskPrice;
    case ref::ReferenceAdapterField::AskQuantity:
        return CanonicalAdapterField::AskQuantity;
    case ref::ReferenceAdapterField::QualityFlag:
        return CanonicalAdapterField::QualityFlag;
    case ref::ReferenceAdapterField::ProjectionPriceScale:
        return CanonicalAdapterField::ProjectionPriceScale;
    case ref::ReferenceAdapterField::ProjectionQuantityScale:
        return CanonicalAdapterField::ProjectionQuantityScale;
    case ref::ReferenceAdapterField::ProjectionPolicy:
        return CanonicalAdapterField::ProjectionPolicy;
    case ref::ReferenceAdapterField::DepthLimit:
        return CanonicalAdapterField::DepthLimit;
    case ref::ReferenceAdapterField::SnapshotSource:
        return CanonicalAdapterField::SnapshotSource;
    case ref::ReferenceAdapterField::LastUpdateId:
        return CanonicalAdapterField::LastUpdateId;
    case ref::ReferenceAdapterField::CurrentGap:
        return CanonicalAdapterField::CurrentGap;
    case ref::ReferenceAdapterField::GapRecoveryState:
        return CanonicalAdapterField::GapRecoveryState;
    case ref::ReferenceAdapterField::HostQualityFact:
        return CanonicalAdapterField::HostQualityFact;
    }
    return CanonicalAdapterField::None;
}

[[nodiscard]] CanonicalQualityFlag canonical(ref::ReferenceQualityFlag flag) noexcept {
    switch (flag) {
    case ref::ReferenceQualityFlag::Duplicate:
        return CanonicalQualityFlag::Duplicate;
    case ref::ReferenceQualityFlag::OutOfOrder:
        return CanonicalQualityFlag::OutOfOrder;
    case ref::ReferenceQualityFlag::SequenceGap:
        return CanonicalQualityFlag::SequenceGap;
    case ref::ReferenceQualityFlag::OrderBookResync:
        return CanonicalQualityFlag::OrderBookResync;
    case ref::ReferenceQualityFlag::SnapshotBridgePending:
        return CanonicalQualityFlag::SnapshotBridgePending;
    case ref::ReferenceQualityFlag::SnapshotTooOld:
        return CanonicalQualityFlag::SnapshotTooOld;
    case ref::ReferenceQualityFlag::BootstrapBufferOverflow:
        return CanonicalQualityFlag::BootstrapBufferOverflow;
    case ref::ReferenceQualityFlag::RecoveredTail:
        return CanonicalQualityFlag::RecoveredTail;
    case ref::ReferenceQualityFlag::MalformedPayload:
        return CanonicalQualityFlag::MalformedPayload;
    case ref::ReferenceQualityFlag::ExchangeTimeMissing:
        return CanonicalQualityFlag::ExchangeTimeMissing;
    case ref::ReferenceQualityFlag::ReceiveClockDiscontinuity:
        return CanonicalQualityFlag::ReceiveClockDiscontinuity;
    case ref::ReferenceQualityFlag::SlowConsumerGap:
        return CanonicalQualityFlag::SlowConsumerGap;
    case ref::ReferenceQualityFlag::ProducerRestart:
        return CanonicalQualityFlag::ProducerRestart;
    case ref::ReferenceQualityFlag::Overlap:
        return CanonicalQualityFlag::Overlap;
    case ref::ReferenceQualityFlag::IdentityConflict:
        return CanonicalQualityFlag::IdentityConflict;
    case ref::ReferenceQualityFlag::CrossedBook:
        return CanonicalQualityFlag::CrossedBook;
    }
    return CanonicalQualityFlag::Duplicate;
}

[[nodiscard]] CanonicalResyncState canonical(ref::ReferenceResyncState state) noexcept {
    switch (state) {
    case ref::ReferenceResyncState::ResyncRequired:
        return CanonicalResyncState::ResyncRequired;
    case ref::ReferenceResyncState::ResyncInProgress:
        return CanonicalResyncState::ResyncInProgress;
    case ref::ReferenceResyncState::ResyncFailed:
        return CanonicalResyncState::ResyncFailed;
    }
    return CanonicalResyncState::ResyncRequired;
}

[[nodiscard]] CanonicalReasonCode canonical(ref::ReferenceReasonCode reason) noexcept {
    switch (reason) {
    case ref::ReferenceReasonCode::SequenceGapDetected:
        return CanonicalReasonCode::SequenceGapDetected;
    }
    return CanonicalReasonCode::SequenceGapDetected;
}

[[nodiscard]] CanonicalSnapshotSource canonical(replay::SnapshotOrigin origin) noexcept {
    switch (origin) {
    case replay::SnapshotOrigin::GatewayLive:
        return CanonicalSnapshotSource::GatewayLive;
    case replay::SnapshotOrigin::RecorderReplay:
        return CanonicalSnapshotSource::RecorderReplay;
    case replay::SnapshotOrigin::HistoryReplay:
        return CanonicalSnapshotSource::HistoryReplay;
    }
    return CanonicalSnapshotSource::HistoryReplay;
}

struct ReferenceLevelObservation final {
    std::vector<CanonicalDecimalObservation> decimals;
    std::vector<reference::RawLevel> levels;
    std::optional<CanonicalDecimalError> first_error;
};

[[nodiscard]] CanonicalBookSide canonical(replay::Side side) noexcept {
    return side == replay::Side::Bid ? CanonicalBookSide::Bid : CanonicalBookSide::Ask;
}

[[nodiscard]] CanonicalDecimalObservation
reference_decimal_observation(const replay::LevelInput& level, std::size_t position,
                              CanonicalDecimalRole role, const ref::ReferenceDecimalResult& parsed,
                              std::uint32_t storage_scale) {
    if (const auto* failure = std::get_if<ref::ReferenceDecimalError>(&parsed.value)) {
        return {canonical(level.side), position, role,
                CanonicalDecimalFailure{canonical(failure->code), failure->offset}};
    }
    const auto& value = std::get<ref::ReferenceDecimalValue>(parsed.value);
    return {canonical(level.side), position, role,
            CanonicalDecimalValue{value.units, storage_scale, value.source_fraction_digits}};
}

void record_first_error(ReferenceLevelObservation& observation,
                        const ref::ReferenceDecimalResult& parsed) {
    if (observation.first_error.has_value()) {
        return;
    }
    if (const auto* failure = std::get_if<ref::ReferenceDecimalError>(&parsed.value)) {
        observation.first_error = canonical(failure->code);
    }
}

[[nodiscard]] ReferenceLevelObservation
observe_reference_levels(const std::vector<replay::LevelInput>& levels, replay::NumericSpec spec) {
    ReferenceLevelObservation observation;
    observation.decimals.reserve(levels.size() * 2);
    observation.levels.reserve(levels.size());
    for (std::size_t position = 0; position < levels.size(); ++position) {
        const auto& level = levels[position];
        const auto price = ref::parse_reference_decimal(level.price, spec.price_scale, false);
        const auto quantity =
            ref::parse_reference_decimal(level.quantity, spec.quantity_scale, true);
        observation.decimals.push_back(reference_decimal_observation(
            level, position, CanonicalDecimalRole::Price, price, spec.price_scale));
        observation.decimals.push_back(reference_decimal_observation(
            level, position, CanonicalDecimalRole::Quantity, quantity, spec.quantity_scale));
        record_first_error(observation, price);
        record_first_error(observation, quantity);
        if (const auto* price_value = std::get_if<ref::ReferenceDecimalValue>(&price.value)) {
            if (const auto* quantity_value =
                    std::get_if<ref::ReferenceDecimalValue>(&quantity.value)) {
                observation.levels.push_back(
                    {level.side == replay::Side::Bid, price_value->units, quantity_value->units});
            }
        }
    }
    return observation;
}

void append_decimals(std::vector<CanonicalDecimalObservation>& destination,
                     std::vector<CanonicalDecimalObservation> source) {
    destination.reserve(destination.size() + source.size());
    destination.insert(destination.end(), std::make_move_iterator(source.begin()),
                       std::make_move_iterator(source.end()));
}

[[nodiscard]] ref::ReferencePolicy reference_policy(replay::SequencePolicy policy) noexcept {
    return policy == replay::SequencePolicy::Spot ? ref::ReferencePolicy::Spot
                                                  : ref::ReferencePolicy::UsdMPerpetual;
}

[[nodiscard]] ref::ReferenceVenue reference_venue(ScenarioVenue venue) noexcept {
    switch (venue) {
    case ScenarioVenue::Binance:
        return ref::ReferenceVenue::Binance;
    case ScenarioVenue::Unspecified:
        return ref::ReferenceVenue::Unspecified;
    case ScenarioVenue::UnknownNumeric:
        return ref::ReferenceVenue::Unknown;
    }
    return ref::ReferenceVenue::Unspecified;
}

[[nodiscard]] ref::ReferenceMarket reference_market(ScenarioMarket market) noexcept {
    switch (market) {
    case ScenarioMarket::Spot:
        return ref::ReferenceMarket::Spot;
    case ScenarioMarket::UsdMPerpetual:
        return ref::ReferenceMarket::UsdMPerpetual;
    case ScenarioMarket::Unspecified:
        return ref::ReferenceMarket::Unspecified;
    case ScenarioMarket::UnknownNumeric:
        return ref::ReferenceMarket::Unknown;
    }
    return ref::ReferenceMarket::Unspecified;
}

[[nodiscard]] ref::ReferenceAdapterDimensions
reference_dimensions(const AdapterScenario& scenario) {
    return {reference_venue(scenario.wire_venue),
            reference_market(scenario.wire_market),
            scenario.wire_symbol,
            scenario.expected_symbol,
            reference_policy(scenario.expected_policy),
            {scenario.conversion_numeric_spec.price_scale,
             scenario.conversion_numeric_spec.quantity_scale},
            {scenario.projection_numeric_spec.price_scale,
             scenario.projection_numeric_spec.quantity_scale},
            reference_policy(scenario.projection_policy)};
}

[[nodiscard]] AdapterErrorOutcome canonical(const ref::ReferenceAdapterError& error) noexcept {
    std::optional<CanonicalDecimalError> detail;
    if (error.decimal_error.has_value()) {
        detail = canonical(*error.decimal_error);
    }
    return {canonical(error.code), canonical(error.field), detail};
}

[[nodiscard]] std::vector<CanonicalQualityFlag>
canonical_quality(const std::vector<ref::ReferenceQualityFlag>& flags) {
    std::vector<CanonicalQualityFlag> result;
    result.reserve(flags.size());
    for (const auto flag : flags) {
        result.push_back(canonical(flag));
    }
    return result;
}

} // namespace

ReferenceSide::ReferenceSide(const replay::ReplayFixture& fixture, ReplayMode mode)
    : ReferenceSide{fixture, mode, default_adapter_scenario(fixture)} {}

ReferenceSide::ReferenceSide(const replay::ReplayFixture& /*fixture*/, ReplayMode mode,
                             const AdapterScenario& scenario)
    : projection_{scenario.projection_policy == replay::SequencePolicy::Spot
                      ? reference::Policy::Spot
                      : reference::Policy::UsdM},
      adapter_{reference_dimensions(scenario)},
      conversion_numeric_spec_{scenario.conversion_numeric_spec},
      projection_numeric_spec_{scenario.projection_numeric_spec},
      projection_policy_{scenario.projection_policy}, expected_symbol_{scenario.expected_symbol},
      mode_{mode} {}

std::optional<OperationObservation> ReferenceSide::observe(const replay::Operation& operation) {
    if (const auto* op = std::get_if<replay::InstallBaselineOp>(&operation)) {
        return observe_install(op->last_update_id, op->bids, op->asks, false);
    }
    if (const auto* op = std::get_if<replay::DepthUpdateOp>(&operation)) {
        return observe_depth_update(*op);
    }
    if (const auto* op = std::get_if<replay::RebaselineOp>(&operation)) {
        return observe_install(op->last_update_id, op->bids, op->asks, true);
    }
    if (std::holds_alternative<replay::ResetOp>(operation)) {
        return observe_reset();
    }
    if (const auto* op = std::get_if<replay::SnapshotRequestOp>(&operation)) {
        return observe_snapshot_request(*op);
    }
    if (const auto* op = std::get_if<replay::AdapterMetadataOp>(&operation)) {
        return observe_metadata(*op);
    }
    return observe_malformed_range(std::get<replay::MalformedRangeOp>(operation));
}

std::optional<OperationObservation>
ReferenceSide::observe_install(std::uint64_t last_update_id,
                               const std::vector<replay::LevelInput>& bids_input,
                               const std::vector<replay::LevelInput>& asks_input, bool rebaseline) {
    // REBASELINE is an M3 lifecycle operation; it does not cross the M4 boundary in
    // either mode, so R4 is not exercised for it.
    if (rebaseline) {
        auto bids = observe_reference_levels(bids_input, projection_numeric_spec_);
        auto asks = observe_reference_levels(asks_input, projection_numeric_spec_);
        std::vector<CanonicalDecimalObservation> decimals;
        append_decimals(decimals, std::move(bids.decimals));
        append_decimals(decimals, std::move(asks.decimals));
        if (bids.first_error.has_value()) {
            return make_observation(DecimalErrorOutcome{bids.first_error.value()},
                                    std::move(decimals));
        }
        if (asks.first_error.has_value()) {
            return make_observation(DecimalErrorOutcome{asks.first_error.value()},
                                    std::move(decimals));
        }
        std::vector<reference::RawLevel> levels;
        levels.reserve(bids.levels.size() + asks.levels.size());
        levels.insert(levels.end(), bids.levels.begin(), bids.levels.end());
        levels.insert(levels.end(), asks.levels.begin(), asks.levels.end());
        return make_observation(canonical(projection_.install(last_update_id, levels)),
                                std::move(decimals));
    }
    auto bids = observe_reference_levels(bids_input, conversion_numeric_spec_);
    auto asks = observe_reference_levels(asks_input, conversion_numeric_spec_);
    std::vector<CanonicalDecimalObservation> decimals;
    append_decimals(decimals, std::move(bids.decimals));
    append_decimals(decimals, std::move(asks.decimals));
    if (mode_ == ReplayMode::AdapterEnabled) {
        const replay::InstallBaselineOp operation{replay::SourceLocation{}, last_update_id,
                                                  bids_input, asks_input};
        const auto prediction = adapter_.predict_baseline_input(operation, pending_metadata_);
        pending_metadata_.clear();
        if (const auto* failure = std::get_if<ref::ReferenceAdapterError>(&prediction)) {
            return make_observation(canonical(*failure), std::move(decimals));
        }
        const auto& input = std::get<ref::ReferenceInputPrediction>(prediction);
        std::vector<reference::RawLevel> levels;
        levels.reserve(bids.levels.size() + asks.levels.size());
        levels.insert(levels.end(), bids.levels.begin(), bids.levels.end());
        levels.insert(levels.end(), asks.levels.begin(), asks.levels.end());
        const auto core_result = canonical(projection_.install(last_update_id, levels));
        return make_observation(
            AdapterSuccessOutcome{std::variant<InstallOutcome, ApplyOutcome>{core_result},
                                  canonical_quality(input.observed_quality)},
            std::move(decimals));
    }
    if (bids.first_error.has_value()) {
        return make_observation(DecimalErrorOutcome{bids.first_error.value()}, std::move(decimals));
    }
    if (asks.first_error.has_value()) {
        return make_observation(DecimalErrorOutcome{asks.first_error.value()}, std::move(decimals));
    }
    std::vector<reference::RawLevel> levels;
    levels.reserve(bids.levels.size() + asks.levels.size());
    levels.insert(levels.end(), bids.levels.begin(), bids.levels.end());
    levels.insert(levels.end(), asks.levels.begin(), asks.levels.end());
    return make_observation(canonical(projection_.install(last_update_id, levels)),
                            std::move(decimals));
}

std::optional<OperationObservation>
ReferenceSide::observe_depth_update(const replay::DepthUpdateOp& operation) {
    auto parsed = observe_reference_levels(operation.levels, conversion_numeric_spec_);
    if (mode_ == ReplayMode::AdapterEnabled) {
        const auto prediction = adapter_.predict_depth_update_input(operation, pending_metadata_);
        pending_metadata_.clear();
        if (const auto* failure = std::get_if<ref::ReferenceAdapterError>(&prediction)) {
            return make_observation(canonical(*failure), std::move(parsed.decimals));
        }
        const auto& input = std::get<ref::ReferenceInputPrediction>(prediction);
        const auto core_result =
            canonical(projection_.apply(operation.first_update_id, operation.final_update_id,
                                        operation.previous_final, parsed.levels));
        return make_observation(
            AdapterSuccessOutcome{std::variant<InstallOutcome, ApplyOutcome>{core_result},
                                  canonical_quality(input.observed_quality)},
            std::move(parsed.decimals));
    }
    if (parsed.first_error.has_value()) {
        return make_observation(DecimalErrorOutcome{parsed.first_error.value()},
                                std::move(parsed.decimals));
    }
    return make_observation(
        canonical(projection_.apply(operation.first_update_id, operation.final_update_id,
                                    operation.previous_final, parsed.levels)),
        std::move(parsed.decimals));
}

std::optional<OperationObservation>
ReferenceSide::observe_snapshot_request(const replay::SnapshotRequestOp& operation) {
    if (mode_ != ReplayMode::AdapterEnabled) {
        return make_observation(SnapshotNotProducedOutcome{});
    }
    const auto prediction = adapter_.predict_snapshot(projection_, operation);
    if (const auto* failure = std::get_if<ref::ReferenceAdapterError>(&prediction)) {
        return make_observation(canonical(*failure));
    }
    const auto predicted = std::get<ref::ReferenceSnapshotPrediction>(prediction);
    SnapshotOutcome snapshot;
    snapshot.policy = projection_policy_ == replay::SequencePolicy::Spot
                          ? CanonicalPolicy::Spot
                          : CanonicalPolicy::UsdMPerpetual;
    snapshot.symbol = expected_symbol_;
    snapshot.producer = operation.producer;
    snapshot.producer_version = operation.producer_version;
    snapshot.source = canonical(operation.source_origin);
    snapshot.generated_time_utc_ns = operation.generated_time_utc_ns;
    snapshot.generated_monotonic_ns = operation.generated_monotonic_ns;
    snapshot.last_update_id = predicted.last_update_id;
    snapshot.synchronized = predicted.synchronized;
    for (const auto& level : predicted.bids) {
        snapshot.bids.push_back({level.price, level.quantity});
    }
    for (const auto& level : predicted.asks) {
        snapshot.asks.push_back({level.price, level.quantity});
    }
    snapshot.quality_flags = canonical_quality(predicted.quality_flags);
    snapshot.depth_limit = predicted.depth_limit;
    if (predicted.gap_descriptor.has_value()) {
        const auto& gap = *predicted.gap_descriptor;
        snapshot.gap_descriptor = GapDescriptorObservation{
            gap.detected_at_utc_ns, gap.previous_sequence, gap.next_sequence,
            canonical(gap.reason_code), canonical(gap.recovery_state)};
    }
    return make_snapshot_observation(snapshot);
}

std::optional<OperationObservation> ReferenceSide::observe_reset() {
    projection_.reset();
    return make_observation(ResetOutcome{});
}

std::optional<OperationObservation>
ReferenceSide::observe_metadata(const replay::AdapterMetadataOp& operation) {
    if (mode_ == ReplayMode::AdapterEnabled) {
        pending_metadata_ = operation.observed_quality;
    }
    return make_observation(MetadataOutcome{});
}

std::optional<OperationObservation>
ReferenceSide::observe_malformed_range(const replay::MalformedRangeOp& operation) {
    return make_observation(RangeOutcome{operation.first_update_id > operation.final_update_id});
}

SemanticCheckpoint ReferenceSide::checkpoint() const {
    SemanticCheckpoint result;
    result.status = canonical(projection_.status());
    result.last_update_id = projection_.last();
    const auto last_gap = projection_.last_gap();
    if (last_gap.has_value()) {
        result.last_gap = canonical(*last_gap);
    }
    result.synchronized_visible = projection_.status() == reference::Status::Synchronized;
    for (const auto& level : projection_.bids()) {
        result.bids.push_back({level.price, level.quantity});
    }
    for (const auto& level : projection_.asks()) {
        result.asks.push_back({level.price, level.quantity});
    }
    result.price_scale = projection_numeric_spec_.price_scale;
    result.quantity_scale = projection_numeric_spec_.quantity_scale;
    return result;
}

std::optional<OperationObservation>
ReferenceSide::make_observation(OperationResultValue result,
                                std::vector<CanonicalDecimalObservation> decimals) const {
    return OperationObservation{0,
                                replay::EventKind::InstallBaseline,
                                std::move(decimals),
                                OperationResult{std::move(result)},
                                checkpoint(),
                                std::nullopt};
}

std::optional<OperationObservation>
ReferenceSide::make_snapshot_observation(const SnapshotOutcome& snapshot) const {
    return OperationObservation{
        0,       replay::EventKind::InstallBaseline, {}, OperationResult{snapshot}, checkpoint(),
        snapshot};
}

std::unique_ptr<ReplaySide> make_reference_side(const replay::ReplayFixture& fixture,
                                                ReplayMode mode) {
    return std::make_unique<ReferenceSide>(fixture, mode);
}

std::unique_ptr<ReplaySide> make_reference_side(const replay::ReplayFixture& fixture,
                                                ReplayMode mode, const AdapterScenario& scenario) {
    return std::make_unique<ReferenceSide>(fixture, mode, scenario);
}

} // namespace bmd_projection::m5::oracle
