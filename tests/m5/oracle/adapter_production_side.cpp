#include "adapter_production_side.hpp"

#include "canonical_convert.hpp"
#include "divergence.hpp"
#include "operation_observation.hpp"
#include "production_decimal_observation.hpp"

#include "replay_types.hpp"

#include <binance_market_data/common/v1/enums.pb.h>
#include <binance_market_data/common/v1/metadata.pb.h>
#include <binance_market_data/market/v1/market_events.pb.h>
#include <binance_market_data/projection/v1/numeric/decimal_parse.hpp>
#include <binance_market_data/projection/v1/order_book/book_level.hpp>
#include <binance_market_data/projection/v1/order_book/level_update.hpp>
#include <binance_market_data/projection/v1/snapshots.pb.h>
#include <binance_market_data/projection_adapter/v1/proto_adapter.hpp>

#include <cstdint>
#include <exception>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace bmd_projection::m5::oracle {
namespace {

namespace adapter = binance_market_data::projection_adapter::v1;
namespace common_wire = binance_market_data::common::v1;
namespace core = binance_market_data::projection::v1;
namespace market_wire = binance_market_data::market::v1;
namespace replay = bmd_projection::m5::replay;

// Bring the Core->canonical conversions into this translation unit's overload set.
using bmd_projection::m5::oracle::to_canonical;

// Fixed non-semantic wire identity synthesized by the replay driver. These fields
// are outside the differential scope (M5 design: adapter dimension scoping).
constexpr std::string_view kReplayProducer{"replay-driver"};
constexpr std::string_view kReplayProducerVersion{"1"};

[[nodiscard]] common_wire::Venue wire_venue(ScenarioVenue venue) noexcept {
    return venue == ScenarioVenue::Binance ? common_wire::VENUE_BINANCE
                                           : common_wire::VENUE_UNSPECIFIED;
}

[[nodiscard]] common_wire::Market wire_market(ScenarioMarket market) noexcept {
    switch (market) {
    case ScenarioMarket::Spot:
        return common_wire::MARKET_SPOT;
    case ScenarioMarket::UsdMPerpetual:
        return common_wire::MARKET_USD_M_PERPETUAL;
    case ScenarioMarket::Unspecified:
        return common_wire::MARKET_UNSPECIFIED;
    }
    return common_wire::MARKET_UNSPECIFIED;
}

[[nodiscard]] core::SequencePolicyKind core_policy(replay::SequencePolicy policy) noexcept {
    return policy == replay::SequencePolicy::Spot ? core::SequencePolicyKind::Spot
                                                  : core::SequencePolicyKind::UsdMPerpetual;
}

[[nodiscard]] core::DecimalScale required_scale(std::uint32_t value) {
    const auto scale = core::DecimalScale::create(value);
    if (!scale.has_value()) {
        std::terminate();
    }
    return scale.value();
}

[[nodiscard]] core::NumericSpec core_numeric_spec(replay::NumericSpec spec) {
    return {required_scale(spec.price_scale), required_scale(spec.quantity_scale)};
}

void append_decimals(std::vector<CanonicalDecimalObservation>& destination,
                     std::vector<CanonicalDecimalObservation> source) {
    destination.reserve(destination.size() + source.size());
    destination.insert(destination.end(), std::make_move_iterator(source.begin()),
                       std::make_move_iterator(source.end()));
}

// Explicit replay-to-M4 enum mapping for wire synthesis. The replay grammar and the
// M4 HostQualityFact/GapRecoveryState domains share stable semantic names; the
// mapping is written explicitly rather than cast.
[[nodiscard]] adapter::HostQualityFact host_fact(replay::HostQualityFact fact) noexcept {
    switch (fact) {
    case replay::HostQualityFact::Duplicate:
        return adapter::HostQualityFact::Duplicate;
    case replay::HostQualityFact::OutOfOrder:
        return adapter::HostQualityFact::OutOfOrder;
    case replay::HostQualityFact::OrderBookResync:
        return adapter::HostQualityFact::OrderBookResync;
    case replay::HostQualityFact::SnapshotTooOld:
        return adapter::HostQualityFact::SnapshotTooOld;
    case replay::HostQualityFact::BootstrapBufferOverflow:
        return adapter::HostQualityFact::BootstrapBufferOverflow;
    case replay::HostQualityFact::RecoveredTail:
        return adapter::HostQualityFact::RecoveredTail;
    case replay::HostQualityFact::MalformedPayload:
        return adapter::HostQualityFact::MalformedPayload;
    case replay::HostQualityFact::ExchangeTimeMissing:
        return adapter::HostQualityFact::ExchangeTimeMissing;
    case replay::HostQualityFact::ReceiveClockDiscontinuity:
        return adapter::HostQualityFact::ReceiveClockDiscontinuity;
    case replay::HostQualityFact::SlowConsumerGap:
        return adapter::HostQualityFact::SlowConsumerGap;
    case replay::HostQualityFact::ProducerRestart:
        return adapter::HostQualityFact::ProducerRestart;
    case replay::HostQualityFact::Overlap:
        return adapter::HostQualityFact::Overlap;
    case replay::HostQualityFact::IdentityConflict:
        return adapter::HostQualityFact::IdentityConflict;
    }
    return adapter::HostQualityFact::Duplicate;
}

[[nodiscard]] adapter::GapRecoveryState recovery_state(replay::GapRecoveryState state) noexcept {
    switch (state) {
    case replay::GapRecoveryState::Synchronized:
        return adapter::GapRecoveryState::Synchronized;
    case replay::GapRecoveryState::ResyncRequired:
        return adapter::GapRecoveryState::ResyncRequired;
    case replay::GapRecoveryState::ResyncInProgress:
        return adapter::GapRecoveryState::ResyncInProgress;
    case replay::GapRecoveryState::Recovered:
        return adapter::GapRecoveryState::Recovered;
    case replay::GapRecoveryState::ResyncFailed:
        return adapter::GapRecoveryState::ResyncFailed;
    }
    return adapter::GapRecoveryState::Synchronized;
}

[[nodiscard]] adapter::SnapshotOrigin snapshot_origin(replay::SnapshotOrigin origin) noexcept {
    switch (origin) {
    case replay::SnapshotOrigin::GatewayLive:
        return adapter::SnapshotOrigin::GatewayLive;
    case replay::SnapshotOrigin::RecorderReplay:
        return adapter::SnapshotOrigin::RecorderReplay;
    case replay::SnapshotOrigin::HistoryReplay:
        return adapter::SnapshotOrigin::HistoryReplay;
    }
    return adapter::SnapshotOrigin::GatewayLive;
}

[[nodiscard]] common_wire::QualityFlag wire_quality(replay::HostQualityFact fact) noexcept {
    switch (fact) {
    case replay::HostQualityFact::Duplicate:
        return common_wire::QUALITY_FLAG_DUPLICATE;
    case replay::HostQualityFact::OutOfOrder:
        return common_wire::QUALITY_FLAG_OUT_OF_ORDER;
    case replay::HostQualityFact::OrderBookResync:
        return common_wire::QUALITY_FLAG_ORDERBOOK_RESYNC;
    case replay::HostQualityFact::SnapshotTooOld:
        return common_wire::QUALITY_FLAG_SNAPSHOT_TOO_OLD;
    case replay::HostQualityFact::BootstrapBufferOverflow:
        return common_wire::QUALITY_FLAG_BOOTSTRAP_BUFFER_OVERFLOW;
    case replay::HostQualityFact::RecoveredTail:
        return common_wire::QUALITY_FLAG_RECOVERED_TAIL;
    case replay::HostQualityFact::MalformedPayload:
        return common_wire::QUALITY_FLAG_MALFORMED_PAYLOAD;
    case replay::HostQualityFact::ExchangeTimeMissing:
        return common_wire::QUALITY_FLAG_EXCHANGE_TIME_MISSING;
    case replay::HostQualityFact::ReceiveClockDiscontinuity:
        return common_wire::QUALITY_FLAG_RECEIVE_CLOCK_DISCONTINUITY;
    case replay::HostQualityFact::SlowConsumerGap:
        return common_wire::QUALITY_FLAG_SLOW_CONSUMER_GAP;
    case replay::HostQualityFact::ProducerRestart:
        return common_wire::QUALITY_FLAG_PRODUCER_RESTART;
    case replay::HostQualityFact::Overlap:
        return common_wire::QUALITY_FLAG_OVERLAP;
    case replay::HostQualityFact::IdentityConflict:
        return common_wire::QUALITY_FLAG_IDENTITY_CONFLICT;
    }
    return common_wire::QUALITY_FLAG_UNSPECIFIED;
}

[[nodiscard]] CanonicalQualityFlag to_canonical(adapter::HostQualityFact fact) noexcept {
    switch (fact) {
    case adapter::HostQualityFact::Duplicate:
        return CanonicalQualityFlag::Duplicate;
    case adapter::HostQualityFact::OutOfOrder:
        return CanonicalQualityFlag::OutOfOrder;
    case adapter::HostQualityFact::OrderBookResync:
        return CanonicalQualityFlag::OrderBookResync;
    case adapter::HostQualityFact::SnapshotTooOld:
        return CanonicalQualityFlag::SnapshotTooOld;
    case adapter::HostQualityFact::BootstrapBufferOverflow:
        return CanonicalQualityFlag::BootstrapBufferOverflow;
    case adapter::HostQualityFact::RecoveredTail:
        return CanonicalQualityFlag::RecoveredTail;
    case adapter::HostQualityFact::MalformedPayload:
        return CanonicalQualityFlag::MalformedPayload;
    case adapter::HostQualityFact::ExchangeTimeMissing:
        return CanonicalQualityFlag::ExchangeTimeMissing;
    case adapter::HostQualityFact::ReceiveClockDiscontinuity:
        return CanonicalQualityFlag::ReceiveClockDiscontinuity;
    case adapter::HostQualityFact::SlowConsumerGap:
        return CanonicalQualityFlag::SlowConsumerGap;
    case adapter::HostQualityFact::ProducerRestart:
        return CanonicalQualityFlag::ProducerRestart;
    case adapter::HostQualityFact::Overlap:
        return CanonicalQualityFlag::Overlap;
    case adapter::HostQualityFact::IdentityConflict:
        return CanonicalQualityFlag::IdentityConflict;
    }
    return CanonicalQualityFlag::CrossedBook;
}

[[nodiscard]] CanonicalAdapterCode to_canonical(adapter::AdapterErrorCode code) noexcept {
    switch (code) {
    case adapter::AdapterErrorCode::UnsupportedVenue:
        return CanonicalAdapterCode::UnsupportedVenue;
    case adapter::AdapterErrorCode::UnsupportedMarket:
        return CanonicalAdapterCode::UnsupportedMarket;
    case adapter::AdapterErrorCode::UnexpectedStream:
        return CanonicalAdapterCode::UnexpectedStream;
    case adapter::AdapterErrorCode::IdentityMismatch:
        return CanonicalAdapterCode::IdentityMismatch;
    case adapter::AdapterErrorCode::UnsupportedSchemaVersion:
        return CanonicalAdapterCode::UnsupportedSchemaVersion;
    case adapter::AdapterErrorCode::UnspecifiedEnum:
        return CanonicalAdapterCode::UnspecifiedEnum;
    case adapter::AdapterErrorCode::UnknownEnumValue:
        return CanonicalAdapterCode::UnknownEnumValue;
    case adapter::AdapterErrorCode::InvalidUpdateRange:
        return CanonicalAdapterCode::InvalidUpdateRange;
    case adapter::AdapterErrorCode::MissingRequiredField:
        return CanonicalAdapterCode::MissingRequiredField;
    case adapter::AdapterErrorCode::InvalidIdentifier:
        return CanonicalAdapterCode::InvalidIdentifier;
    case adapter::AdapterErrorCode::InvalidDecimal:
        return CanonicalAdapterCode::InvalidDecimal;
    case adapter::AdapterErrorCode::NegativeQuantity:
        return CanonicalAdapterCode::NegativeQuantity;
    case adapter::AdapterErrorCode::NonPositivePrice:
        return CanonicalAdapterCode::NonPositivePrice;
    case adapter::AdapterErrorCode::ScaleMismatch:
        return CanonicalAdapterCode::ScaleMismatch;
    case adapter::AdapterErrorCode::NumericOverflow:
        return CanonicalAdapterCode::NumericOverflow;
    case adapter::AdapterErrorCode::InvalidDepthLimit:
        return CanonicalAdapterCode::InvalidDepthLimit;
    case adapter::AdapterErrorCode::InvalidOrdering:
        return CanonicalAdapterCode::InvalidOrdering;
    case adapter::AdapterErrorCode::UnsupportedProjectionState:
        return CanonicalAdapterCode::UnsupportedProjectionState;
    case adapter::AdapterErrorCode::MissingLastUpdateId:
        return CanonicalAdapterCode::MissingLastUpdateId;
    case adapter::AdapterErrorCode::InvalidGapContext:
        return CanonicalAdapterCode::InvalidGapContext;
    case adapter::AdapterErrorCode::InvalidHostQualityCombination:
        return CanonicalAdapterCode::InvalidHostQualityCombination;
    case adapter::AdapterErrorCode::ContractsVersionMismatch:
        return CanonicalAdapterCode::ContractsVersionMismatch;
    case adapter::AdapterErrorCode::ProjectionNumericSpecMismatch:
        return CanonicalAdapterCode::ProjectionNumericSpecMismatch;
    case adapter::AdapterErrorCode::ProjectionPolicyMismatch:
        return CanonicalAdapterCode::ProjectionPolicyMismatch;
    }
    return CanonicalAdapterCode::InvalidDecimal;
}

// Linear enum-name mapping table over the closed 25-value adapter field domain.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
[[nodiscard]] CanonicalAdapterField to_canonical(adapter::AdapterField field) noexcept {
    switch (field) {
    case adapter::AdapterField::None:
        return CanonicalAdapterField::None;
    case adapter::AdapterField::Venue:
        return CanonicalAdapterField::Venue;
    case adapter::AdapterField::Market:
        return CanonicalAdapterField::Market;
    case adapter::AdapterField::Stream:
        return CanonicalAdapterField::Stream;
    case adapter::AdapterField::Symbol:
        return CanonicalAdapterField::Symbol;
    case adapter::AdapterField::SchemaVersion:
        return CanonicalAdapterField::SchemaVersion;
    case adapter::AdapterField::Producer:
        return CanonicalAdapterField::Producer;
    case adapter::AdapterField::ProducerVersion:
        return CanonicalAdapterField::ProducerVersion;
    case adapter::AdapterField::RequestId:
        return CanonicalAdapterField::RequestId;
    case adapter::AdapterField::ConnectionId:
        return CanonicalAdapterField::ConnectionId;
    case adapter::AdapterField::FirstUpdateId:
        return CanonicalAdapterField::FirstUpdateId;
    case adapter::AdapterField::FinalUpdateId:
        return CanonicalAdapterField::FinalUpdateId;
    case adapter::AdapterField::PreviousFinalUpdateId:
        return CanonicalAdapterField::PreviousFinalUpdateId;
    case adapter::AdapterField::BidPrice:
        return CanonicalAdapterField::BidPrice;
    case adapter::AdapterField::BidQuantity:
        return CanonicalAdapterField::BidQuantity;
    case adapter::AdapterField::AskPrice:
        return CanonicalAdapterField::AskPrice;
    case adapter::AdapterField::AskQuantity:
        return CanonicalAdapterField::AskQuantity;
    case adapter::AdapterField::QualityFlag:
        return CanonicalAdapterField::QualityFlag;
    case adapter::AdapterField::ProjectionPriceScale:
        return CanonicalAdapterField::ProjectionPriceScale;
    case adapter::AdapterField::ProjectionQuantityScale:
        return CanonicalAdapterField::ProjectionQuantityScale;
    case adapter::AdapterField::ProjectionPolicy:
        return CanonicalAdapterField::ProjectionPolicy;
    case adapter::AdapterField::DepthLimit:
        return CanonicalAdapterField::DepthLimit;
    case adapter::AdapterField::SnapshotSource:
        return CanonicalAdapterField::SnapshotSource;
    case adapter::AdapterField::LastUpdateId:
        return CanonicalAdapterField::LastUpdateId;
    case adapter::AdapterField::CurrentGap:
        return CanonicalAdapterField::CurrentGap;
    case adapter::AdapterField::GapRecoveryState:
        return CanonicalAdapterField::GapRecoveryState;
    case adapter::AdapterField::HostQualityFact:
        return CanonicalAdapterField::HostQualityFact;
    }
    return CanonicalAdapterField::None;
}

[[nodiscard]] CanonicalSnapshotSource to_canonical(common_wire::SnapshotSource source) noexcept {
    switch (source) {
    case common_wire::SNAPSHOT_SOURCE_GATEWAY_LIVE:
        return CanonicalSnapshotSource::GatewayLive;
    case common_wire::SNAPSHOT_SOURCE_RECORDER_REPLAY:
        return CanonicalSnapshotSource::RecorderReplay;
    default:
        return CanonicalSnapshotSource::HistoryReplay;
    }
}

[[nodiscard]] CanonicalResyncState to_canonical(common_wire::ResyncState state) noexcept {
    switch (state) {
    case common_wire::RESYNC_STATE_RESYNC_REQUIRED:
        return CanonicalResyncState::ResyncRequired;
    case common_wire::RESYNC_STATE_RESYNC_IN_PROGRESS:
        return CanonicalResyncState::ResyncInProgress;
    case common_wire::RESYNC_STATE_RESYNC_FAILED:
        return CanonicalResyncState::ResyncFailed;
    default:
        return CanonicalResyncState::ResyncRequired;
    }
}

[[nodiscard]] CanonicalReasonCode to_canonical(common_wire::ReasonCode reason) noexcept {
    static_cast<void>(reason);
    return CanonicalReasonCode::SequenceGapDetected;
}

[[nodiscard]] AdapterErrorOutcome to_canonical(const adapter::AdapterError& error) noexcept {
    std::optional<CanonicalDecimalError> detail;
    if (error.decimal_error.has_value()) {
        detail = to_canonical(error.decimal_error->code);
    }
    return {to_canonical(error.code), to_canonical(error.field), detail};
}

// The raw_enum_value field is not representable in the replay grammar and is always
// absent in replay-derived adapter errors; it remains M4 unit/property scope.
[[nodiscard]] std::vector<CanonicalQualityFlag>
to_canonical_observed(const adapter::AdaptedMetadata& metadata) {
    std::vector<CanonicalQualityFlag> result;
    result.reserve(metadata.observed_quality.size());
    for (const auto fact : metadata.observed_quality) {
        result.push_back(to_canonical(fact));
    }
    return result;
}

[[nodiscard]] std::optional<CanonicalQualityFlag>
snapshot_flag(const core::LocalOrderBookSnapshot& snapshot, int index) noexcept {
    switch (snapshot.quality_flags(index)) {
    case common_wire::QUALITY_FLAG_DUPLICATE:
        return CanonicalQualityFlag::Duplicate;
    case common_wire::QUALITY_FLAG_OUT_OF_ORDER:
        return CanonicalQualityFlag::OutOfOrder;
    case common_wire::QUALITY_FLAG_SEQUENCE_GAP:
        return CanonicalQualityFlag::SequenceGap;
    case common_wire::QUALITY_FLAG_ORDERBOOK_RESYNC:
        return CanonicalQualityFlag::OrderBookResync;
    case common_wire::QUALITY_FLAG_SNAPSHOT_BRIDGE_PENDING:
        return CanonicalQualityFlag::SnapshotBridgePending;
    case common_wire::QUALITY_FLAG_SNAPSHOT_TOO_OLD:
        return CanonicalQualityFlag::SnapshotTooOld;
    case common_wire::QUALITY_FLAG_BOOTSTRAP_BUFFER_OVERFLOW:
        return CanonicalQualityFlag::BootstrapBufferOverflow;
    case common_wire::QUALITY_FLAG_RECOVERED_TAIL:
        return CanonicalQualityFlag::RecoveredTail;
    case common_wire::QUALITY_FLAG_MALFORMED_PAYLOAD:
        return CanonicalQualityFlag::MalformedPayload;
    case common_wire::QUALITY_FLAG_EXCHANGE_TIME_MISSING:
        return CanonicalQualityFlag::ExchangeTimeMissing;
    case common_wire::QUALITY_FLAG_RECEIVE_CLOCK_DISCONTINUITY:
        return CanonicalQualityFlag::ReceiveClockDiscontinuity;
    case common_wire::QUALITY_FLAG_SLOW_CONSUMER_GAP:
        return CanonicalQualityFlag::SlowConsumerGap;
    case common_wire::QUALITY_FLAG_PRODUCER_RESTART:
        return CanonicalQualityFlag::ProducerRestart;
    case common_wire::QUALITY_FLAG_OVERLAP:
        return CanonicalQualityFlag::Overlap;
    case common_wire::QUALITY_FLAG_IDENTITY_CONFLICT:
        return CanonicalQualityFlag::IdentityConflict;
    case common_wire::QUALITY_FLAG_CROSSED_BOOK:
        return CanonicalQualityFlag::CrossedBook;
    default:
        return std::nullopt;
    }
}

// Semantic extraction of the produced snapshot. Never compares Protobuf bytes.
[[nodiscard]] std::optional<SnapshotOutcome>
extract_snapshot(const core::LocalOrderBookSnapshot& wire, core::SequencePolicyKind policy) {
    SnapshotOutcome snapshot;
    snapshot.policy = to_canonical(policy);
    snapshot.symbol = wire.symbol();
    snapshot.producer = wire.producer();
    snapshot.producer_version = wire.producer_version();
    snapshot.source = to_canonical(wire.source());
    snapshot.generated_time_utc_ns = wire.generated_time_utc_ns();
    if (wire.has_generated_monotonic_ns()) {
        snapshot.generated_monotonic_ns = wire.generated_monotonic_ns();
    }
    snapshot.last_update_id = wire.last_update_id();
    snapshot.synchronized = wire.synchronized();
    for (const auto& level : wire.bids()) {
        snapshot.bids.push_back({level.price(), level.quantity()});
    }
    for (const auto& level : wire.asks()) {
        snapshot.asks.push_back({level.price(), level.quantity()});
    }
    for (int index = 0; index < wire.quality_flags_size(); ++index) {
        const auto flag = snapshot_flag(wire, index);
        if (!flag.has_value()) {
            return std::nullopt;
        }
        snapshot.quality_flags.push_back(*flag);
    }
    if (wire.has_depth_limit()) {
        snapshot.depth_limit = wire.depth_limit();
    }
    if (wire.has_last_gap()) {
        const auto& gap = wire.last_gap();
        snapshot.gap_descriptor = GapDescriptorObservation{
            gap.detected_at_utc_ns(), gap.previous_sequence(), gap.next_sequence(),
            to_canonical(gap.reason_code()), to_canonical(gap.recovery_state())};
    }
    return snapshot;
}

} // namespace

AdapterProductionSide::AdapterProductionSide(const replay::ReplayFixture& fixture)
    : AdapterProductionSide{fixture, default_adapter_scenario(fixture)} {}

AdapterProductionSide::AdapterProductionSide(const replay::ReplayFixture& /*fixture*/,
                                             AdapterScenario scenario)
    : projection_{core_numeric_spec(scenario.projection_numeric_spec),
                  core_policy(scenario.projection_policy)},
      scenario_{std::move(scenario)} {}

std::optional<OperationObservation>
AdapterProductionSide::observe(const replay::Operation& operation) {
    if (const auto* op = std::get_if<replay::InstallBaselineOp>(&operation)) {
        return observe_install(*op);
    }
    if (const auto* op = std::get_if<replay::DepthUpdateOp>(&operation)) {
        return observe_depth_update(*op);
    }
    if (const auto* op = std::get_if<replay::RebaselineOp>(&operation)) {
        auto bids = observe_production_levels(op->bids, projection_.numeric_spec());
        auto asks = observe_production_levels(op->asks, projection_.numeric_spec());
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
        const auto result = projection_.install_baseline(
            {core::UpdateId{op->last_update_id}, bids.levels, asks.levels});
        return make_observation(to_canonical(result), std::move(decimals));
    }
    if (std::holds_alternative<replay::ResetOp>(operation)) {
        projection_.reset();
        return make_observation(ResetOutcome{});
    }
    if (const auto* op = std::get_if<replay::SnapshotRequestOp>(&operation)) {
        return observe_snapshot_request(*op);
    }
    if (const auto* op = std::get_if<replay::AdapterMetadataOp>(&operation)) {
        pending_metadata_ = op->observed_quality;
        return make_observation(MetadataOutcome{});
    }
    const auto& malformed = std::get<replay::MalformedRangeOp>(operation);
    const auto range = core::UpdateRange::try_create(core::UpdateId{malformed.first_update_id},
                                                     core::UpdateId{malformed.final_update_id});
    return make_observation(RangeOutcome{!range.has_value()});
}

std::optional<OperationObservation>
AdapterProductionSide::observe_install(const replay::InstallBaselineOp& operation) {
    const auto conversion_spec = core_numeric_spec(scenario_.conversion_numeric_spec);
    auto bids = observe_production_levels(operation.bids, conversion_spec);
    auto asks = observe_production_levels(operation.asks, conversion_spec);
    std::vector<CanonicalDecimalObservation> decimals;
    append_decimals(decimals, std::move(bids.decimals));
    append_decimals(decimals, std::move(asks.decimals));
    market_wire::ExchangeDepthSnapshot wire;
    wire.set_venue(wire_venue(scenario_.wire_venue));
    wire.set_market(wire_market(scenario_.wire_market));
    wire.set_symbol(scenario_.wire_symbol);
    wire.set_schema_version("exchange-depth-snapshot.v1");
    wire.set_producer(std::string(kReplayProducer));
    wire.set_producer_version(std::string(kReplayProducerVersion));
    wire.set_request_id("replay-baseline-" + std::to_string(operation.source.event_index));
    for (const auto fact : pending_metadata_) {
        wire.add_quality_flags(wire_quality(fact));
    }
    pending_metadata_.clear();
    wire.set_last_update_id(operation.last_update_id);
    for (const auto& level : operation.bids) {
        auto* wire_level = wire.add_bids();
        wire_level->set_price(level.price);
        wire_level->set_quantity(level.quantity);
    }
    for (const auto& level : operation.asks) {
        auto* wire_level = wire.add_asks();
        wire_level->set_price(level.price);
        wire_level->set_quantity(level.quantity);
    }

    const adapter::ExpectedIdentity expected{scenario_.expected_symbol,
                                             core_policy(scenario_.expected_policy)};
    auto adapted = adapter::adapt_exchange_depth_snapshot(wire, conversion_spec, expected);
    if (const auto* failure = std::get_if<adapter::AdapterError>(&adapted)) {
        return make_observation(to_canonical(*failure), std::move(decimals));
    }
    auto owner = std::move(std::get<adapter::AdaptedBookBaseline>(adapted));
    const auto observed = to_canonical_observed(owner.metadata());
    const auto installed = owner.install_into(projection_);
    if (const auto* failure = std::get_if<adapter::AdapterError>(&installed)) {
        return make_observation(to_canonical(*failure), std::move(decimals));
    }
    return make_observation(
        AdapterSuccessOutcome{std::variant<InstallOutcome, ApplyOutcome>{
                                  to_canonical(std::get<core::InstallResult>(installed))},
                              observed},
        std::move(decimals));
}

std::optional<OperationObservation>
AdapterProductionSide::observe_depth_update(const replay::DepthUpdateOp& operation) {
    const auto conversion_spec = core_numeric_spec(scenario_.conversion_numeric_spec);
    auto parsed = observe_production_levels(operation.levels, conversion_spec);
    market_wire::DepthUpdate wire;
    auto* metadata = wire.mutable_metadata();
    metadata->set_venue(wire_venue(scenario_.wire_venue));
    metadata->set_market(wire_market(scenario_.wire_market));
    metadata->set_symbol(scenario_.wire_symbol);
    metadata->set_producer(std::string(kReplayProducer));
    metadata->set_producer_version(std::string(kReplayProducerVersion));
    metadata->set_connection_id("replay-connection-" +
                                std::to_string(operation.source.event_index));
    metadata->set_stream(common_wire::STREAM_DIFF_DEPTH);
    metadata->set_schema_version("depth-update.v1");
    for (const auto fact : pending_metadata_) {
        metadata->add_quality_flags(wire_quality(fact));
    }
    pending_metadata_.clear();
    wire.set_first_update_id(operation.first_update_id);
    wire.set_final_update_id(operation.final_update_id);
    if (operation.previous_final.has_value()) {
        wire.set_previous_final_update_id(*operation.previous_final);
    }
    for (const auto& level : operation.levels) {
        auto* wire_level = level.side == replay::Side::Bid ? wire.add_bids() : wire.add_asks();
        wire_level->set_price(level.price);
        wire_level->set_quantity(level.quantity);
    }

    const adapter::ExpectedIdentity expected{scenario_.expected_symbol,
                                             core_policy(scenario_.expected_policy)};
    auto adapted = adapter::adapt_depth_update(wire, conversion_spec, expected);
    if (const auto* failure = std::get_if<adapter::AdapterError>(&adapted)) {
        return make_observation(to_canonical(*failure), std::move(parsed.decimals));
    }
    auto owner = std::move(std::get<adapter::AdaptedDepthBatch>(adapted));
    const auto observed = to_canonical_observed(owner.metadata());
    const auto applied = owner.apply_to(projection_);
    if (const auto* failure = std::get_if<adapter::AdapterError>(&applied)) {
        return make_observation(to_canonical(*failure), std::move(parsed.decimals));
    }
    return make_observation(
        AdapterSuccessOutcome{std::variant<InstallOutcome, ApplyOutcome>{
                                  to_canonical(std::get<core::ApplyResult>(applied))},
                              observed},
        std::move(parsed.decimals));
}

std::optional<OperationObservation>
AdapterProductionSide::observe_snapshot_request(const replay::SnapshotRequestOp& operation) {
    adapter::SnapshotOptions options;
    if (operation.depth_limit.has_value()) {
        const auto limit = adapter::DepthLimit::create(*operation.depth_limit);
        if (const auto* failure = std::get_if<adapter::AdapterError>(&limit)) {
            return make_observation(to_canonical(*failure));
        }
        options.depth_limit = std::get<adapter::DepthLimit>(limit);
    }
    options.host_quality_facts.reserve(operation.host_quality_facts.size());
    for (const auto fact : operation.host_quality_facts) {
        options.host_quality_facts.push_back(host_fact(fact));
    }
    adapter::SnapshotContext context{
        {scenario_.expected_symbol, core_policy(scenario_.expected_policy)},
        operation.producer,
        operation.producer_version,
        snapshot_origin(operation.source_origin),
        operation.generated_time_utc_ns,
        operation.generated_monotonic_ns,
        std::nullopt};
    if (operation.current_gap.has_value()) {
        context.current_gap = adapter::CurrentGapContext{
            operation.current_gap->first, recovery_state(operation.current_gap->second)};
    }
    const auto produced = adapter::make_local_order_book_snapshot(projection_, context, options);
    if (const auto* failure = std::get_if<adapter::AdapterError>(&produced)) {
        return make_observation(to_canonical(*failure));
    }
    const auto snapshot =
        extract_snapshot(std::get<core::LocalOrderBookSnapshot>(produced), projection_.policy());
    if (!snapshot.has_value()) {
        return make_observation(AdapterErrorOutcome{CanonicalAdapterCode::UnsupportedVenue,
                                                    CanonicalAdapterField::QualityFlag,
                                                    std::nullopt});
    }
    return make_snapshot_observation(*snapshot);
}

SemanticCheckpoint AdapterProductionSide::checkpoint() const {
    SemanticCheckpoint result;
    result.status = to_canonical(projection_.status());
    if (const auto last_update_id = projection_.last_update_id()) {
        result.last_update_id = last_update_id->value();
    }
    if (const auto last_gap = projection_.last_gap()) {
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
AdapterProductionSide::make_observation(OperationResultValue result,
                                        std::vector<CanonicalDecimalObservation> decimals) const {
    return OperationObservation{0,
                                replay::EventKind::InstallBaseline,
                                std::move(decimals),
                                OperationResult{std::move(result)},
                                checkpoint(),
                                std::nullopt};
}

std::optional<OperationObservation>
AdapterProductionSide::make_snapshot_observation(const SnapshotOutcome& snapshot) const {
    return OperationObservation{
        0,       replay::EventKind::InstallBaseline, {}, OperationResult{snapshot}, checkpoint(),
        snapshot};
}

std::unique_ptr<ReplaySide> make_adapter_production_side(const replay::ReplayFixture& fixture) {
    return std::make_unique<AdapterProductionSide>(fixture);
}

std::unique_ptr<ReplaySide> make_adapter_production_side(const replay::ReplayFixture& fixture,
                                                         const AdapterScenario& scenario) {
    return std::make_unique<AdapterProductionSide>(fixture, scenario);
}

} // namespace bmd_projection::m5::oracle
