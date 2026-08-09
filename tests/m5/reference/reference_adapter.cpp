#include "reference_adapter.hpp"

#include "reference_decimal.hpp"
#include "reference_projection.hpp"

#include "replay_types.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace bmd_projection::m5::reference {
namespace {

namespace replay = bmd_projection::m5::replay;

[[nodiscard]] ReferenceAdapterError error(ReferenceAdapterErrorCode code,
                                          ReferenceAdapterField field) noexcept {
    return {code, field, std::nullopt};
}

[[nodiscard]] ReferenceAdapterError decimal_error(ReferenceAdapterErrorCode code,
                                                  ReferenceAdapterField field,
                                                  ReferenceDecimalErrorCode detail) noexcept {
    return {code, field, detail};
}

[[nodiscard]] bool is_symbol(std::string_view value) noexcept {
    if (value.size() < 2 || value.size() > 20) {
        return false;
    }
    return std::ranges::all_of(value, [](char character) {
        return (character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9');
    });
}

[[nodiscard]] constexpr bool is_ascii_whitespace(char value) noexcept {
    return value == ' ' || value == '\t' || value == '\n' || value == '\r' || value == '\f' ||
           value == '\v';
}

[[nodiscard]] bool is_non_empty_text(std::string_view value) noexcept {
    return !value.empty() && value.size() <= 256 && !is_ascii_whitespace(value.front()) &&
           !is_ascii_whitespace(value.back());
}

[[nodiscard]] ReferencePolicy policy_for_market(ReferenceMarket market) noexcept {
    return market == ReferenceMarket::Spot ? ReferencePolicy::Spot : ReferencePolicy::UsdMPerpetual;
}

[[nodiscard]] ReferencePolicy reference_policy(replay::SequencePolicy policy) noexcept {
    return policy == replay::SequencePolicy::Spot ? ReferencePolicy::Spot
                                                  : ReferencePolicy::UsdMPerpetual;
}

[[nodiscard]] ReferenceMarket reference_market(replay::SequencePolicy policy) noexcept {
    return policy == replay::SequencePolicy::Spot ? ReferenceMarket::Spot
                                                  : ReferenceMarket::UsdMPerpetual;
}

// R4 quality-ranking table for the combined output domain. This is the R4-owned
// canonical rank; it is written from the M4 design's documented semantic rank.
[[nodiscard]] std::size_t quality_rank(ReferenceQualityFlag flag) noexcept {
    switch (flag) {
    case ReferenceQualityFlag::Duplicate:
        return 0;
    case ReferenceQualityFlag::OutOfOrder:
        return 1;
    case ReferenceQualityFlag::SequenceGap:
        return 2;
    case ReferenceQualityFlag::OrderBookResync:
        return 3;
    case ReferenceQualityFlag::SnapshotBridgePending:
        return 4;
    case ReferenceQualityFlag::SnapshotTooOld:
        return 5;
    case ReferenceQualityFlag::BootstrapBufferOverflow:
        return 6;
    case ReferenceQualityFlag::RecoveredTail:
        return 7;
    case ReferenceQualityFlag::MalformedPayload:
        return 8;
    case ReferenceQualityFlag::ExchangeTimeMissing:
        return 9;
    case ReferenceQualityFlag::ReceiveClockDiscontinuity:
        return 10;
    case ReferenceQualityFlag::SlowConsumerGap:
        return 11;
    case ReferenceQualityFlag::ProducerRestart:
        return 12;
    case ReferenceQualityFlag::Overlap:
        return 13;
    case ReferenceQualityFlag::IdentityConflict:
        return 14;
    case ReferenceQualityFlag::CrossedBook:
        return 15;
    }
    return 16;
}

// Inbound wire quality mapping: the replay grammar expresses exactly the 13 mappable
// host facts; the three Core-derived wire values are recognized but cannot assert the
// state of the target projection and are therefore not representable in replay input.
[[nodiscard]] ReferenceQualityFlag map_inbound_fact(replay::HostQualityFact fact) noexcept {
    switch (fact) {
    case replay::HostQualityFact::Duplicate:
        return ReferenceQualityFlag::Duplicate;
    case replay::HostQualityFact::OutOfOrder:
        return ReferenceQualityFlag::OutOfOrder;
    case replay::HostQualityFact::OrderBookResync:
        return ReferenceQualityFlag::OrderBookResync;
    case replay::HostQualityFact::SnapshotTooOld:
        return ReferenceQualityFlag::SnapshotTooOld;
    case replay::HostQualityFact::BootstrapBufferOverflow:
        return ReferenceQualityFlag::BootstrapBufferOverflow;
    case replay::HostQualityFact::RecoveredTail:
        return ReferenceQualityFlag::RecoveredTail;
    case replay::HostQualityFact::MalformedPayload:
        return ReferenceQualityFlag::MalformedPayload;
    case replay::HostQualityFact::ExchangeTimeMissing:
        return ReferenceQualityFlag::ExchangeTimeMissing;
    case replay::HostQualityFact::ReceiveClockDiscontinuity:
        return ReferenceQualityFlag::ReceiveClockDiscontinuity;
    case replay::HostQualityFact::SlowConsumerGap:
        return ReferenceQualityFlag::SlowConsumerGap;
    case replay::HostQualityFact::ProducerRestart:
        return ReferenceQualityFlag::ProducerRestart;
    case replay::HostQualityFact::Overlap:
        return ReferenceQualityFlag::Overlap;
    case replay::HostQualityFact::IdentityConflict:
        return ReferenceQualityFlag::IdentityConflict;
    }
    return ReferenceQualityFlag::CrossedBook;
}

[[nodiscard]] std::vector<ReferenceQualityFlag>
deduplicate_and_rank(std::vector<ReferenceQualityFlag> flags) {
    std::sort(flags.begin(), flags.end(), [](ReferenceQualityFlag lhs, ReferenceQualityFlag rhs) {
        return quality_rank(lhs) < quality_rank(rhs);
    });
    flags.erase(std::unique(flags.begin(), flags.end()), flags.end());
    return flags;
}

// R4-owned price/quantity decimal error mapping, mirroring the documented M4 adapter
// error precedence for decimal fields.
// R4 parses level tokens through R1 at the fixture storage scales and maps the
// documented M4 decimal error precedence exactly as the adapter boundary does.
[[nodiscard]] std::optional<ReferenceAdapterError>
adapt_price(const std::string& text, ReferenceAdapterField field, std::uint32_t storage_scale) {
    const auto parsed = parse_reference_decimal(text, storage_scale, false);
    const auto* failure = std::get_if<ReferenceDecimalError>(&parsed.value);
    if (failure == nullptr) {
        return std::nullopt;
    }
    auto code = ReferenceAdapterErrorCode::InvalidDecimal;
    if (failure->code == ReferenceDecimalErrorCode::ZeroNotAllowed ||
        (!text.empty() && text.front() == '-')) {
        code = ReferenceAdapterErrorCode::NonPositivePrice;
    } else if (failure->code == ReferenceDecimalErrorCode::InexactScale) {
        code = ReferenceAdapterErrorCode::ScaleMismatch;
    } else if (failure->code == ReferenceDecimalErrorCode::Overflow) {
        code = ReferenceAdapterErrorCode::NumericOverflow;
    }
    return decimal_error(code, field, failure->code);
}

[[nodiscard]] std::optional<ReferenceAdapterError>
adapt_quantity(const std::string& text, ReferenceAdapterField field, std::uint32_t storage_scale) {
    const auto parsed = parse_reference_decimal(text, storage_scale, true);
    const auto* failure = std::get_if<ReferenceDecimalError>(&parsed.value);
    if (failure == nullptr) {
        return std::nullopt;
    }
    auto code = ReferenceAdapterErrorCode::InvalidDecimal;
    if (!text.empty() && text.front() == '-') {
        code = ReferenceAdapterErrorCode::NegativeQuantity;
    } else if (failure->code == ReferenceDecimalErrorCode::InexactScale) {
        code = ReferenceAdapterErrorCode::ScaleMismatch;
    } else if (failure->code == ReferenceDecimalErrorCode::Overflow) {
        code = ReferenceAdapterErrorCode::NumericOverflow;
    }
    return decimal_error(code, field, failure->code);
}

[[nodiscard]] std::optional<ReferenceAdapterError> adapt_level(const replay::LevelInput& level,
                                                               ReferenceNumericSpec numeric_spec) {
    const bool bids = level.side == replay::Side::Bid;
    const auto price_field =
        bids ? ReferenceAdapterField::BidPrice : ReferenceAdapterField::AskPrice;
    const auto quantity_field =
        bids ? ReferenceAdapterField::BidQuantity : ReferenceAdapterField::AskQuantity;
    if (const auto failure = adapt_price(level.price, price_field, numeric_spec.price_scale)) {
        return failure;
    }
    if (const auto failure =
            adapt_quantity(level.quantity, quantity_field, numeric_spec.quantity_scale)) {
        return failure;
    }
    return std::nullopt;
}

[[nodiscard]] bool host_fact_valid_for_status(replay::HostQualityFact fact,
                                              bmd_projection_reference::Status status) noexcept {
    using Status = bmd_projection_reference::Status;
    switch (fact) {
    case replay::HostQualityFact::OrderBookResync:
        return status == Status::AwaitingBridge || status == Status::NeedsResync;
    case replay::HostQualityFact::RecoveredTail:
        return status == Status::Synchronized;
    case replay::HostQualityFact::MalformedPayload:
        return status == Status::AwaitingBridge || status == Status::NeedsResync;
    default:
        return true;
    }
}

[[nodiscard]] std::optional<ReferenceResyncState>
map_recovery(replay::GapRecoveryState state) noexcept {
    switch (state) {
    case replay::GapRecoveryState::ResyncRequired:
        return ReferenceResyncState::ResyncRequired;
    case replay::GapRecoveryState::ResyncInProgress:
        return ReferenceResyncState::ResyncInProgress;
    case replay::GapRecoveryState::ResyncFailed:
        return ReferenceResyncState::ResyncFailed;
    case replay::GapRecoveryState::Synchronized:
    case replay::GapRecoveryState::Recovered:
        return std::nullopt;
    }
    return std::nullopt;
}

} // namespace

ReferenceAdapter::ReferenceAdapter(replay::SequencePolicy policy, std::string_view symbol,
                                   replay::NumericSpec numeric_spec)
    : ReferenceAdapter{
          ReferenceAdapterDimensions{ReferenceVenue::Binance,
                                     reference_market(policy),
                                     std::string{symbol},
                                     std::string{symbol},
                                     reference_policy(policy),
                                     {numeric_spec.price_scale, numeric_spec.quantity_scale},
                                     {numeric_spec.price_scale, numeric_spec.quantity_scale},
                                     reference_policy(policy)}} {}

ReferenceAdapter::ReferenceAdapter(ReferenceAdapterDimensions dimensions)
    : dimensions_{std::move(dimensions)} {}

std::optional<ReferenceAdapterError> ReferenceAdapter::validate_inbound_identity() const {
    if (dimensions_.wire_venue == ReferenceVenue::Unspecified) {
        return error(ReferenceAdapterErrorCode::UnspecifiedEnum, ReferenceAdapterField::Venue);
    }
    if (dimensions_.wire_market == ReferenceMarket::Unspecified) {
        return error(ReferenceAdapterErrorCode::UnspecifiedEnum, ReferenceAdapterField::Market);
    }
    if (!is_symbol(dimensions_.wire_symbol)) {
        return error(ReferenceAdapterErrorCode::InvalidIdentifier, ReferenceAdapterField::Symbol);
    }
    if (!is_symbol(dimensions_.expected_symbol) ||
        dimensions_.wire_symbol != dimensions_.expected_symbol) {
        return error(ReferenceAdapterErrorCode::IdentityMismatch, ReferenceAdapterField::Symbol);
    }
    if (policy_for_market(dimensions_.wire_market) != dimensions_.expected_policy) {
        return error(ReferenceAdapterErrorCode::IdentityMismatch, ReferenceAdapterField::Market);
    }
    return std::nullopt;
}

std::optional<ReferenceAdapterError> ReferenceAdapter::validate_snapshot_identity() const {
    if (!is_symbol(dimensions_.expected_symbol)) {
        return error(ReferenceAdapterErrorCode::InvalidIdentifier, ReferenceAdapterField::Symbol);
    }
    if (dimensions_.expected_policy != dimensions_.projection_policy) {
        return error(ReferenceAdapterErrorCode::ProjectionPolicyMismatch,
                     ReferenceAdapterField::ProjectionPolicy);
    }
    return std::nullopt;
}

std::optional<ReferenceAdapterError> ReferenceAdapter::validate_binding() const {
    if (dimensions_.conversion_numeric_spec.price_scale !=
        dimensions_.projection_numeric_spec.price_scale) {
        return error(ReferenceAdapterErrorCode::ProjectionNumericSpecMismatch,
                     ReferenceAdapterField::ProjectionPriceScale);
    }
    if (dimensions_.conversion_numeric_spec.quantity_scale !=
        dimensions_.projection_numeric_spec.quantity_scale) {
        return error(ReferenceAdapterErrorCode::ProjectionNumericSpecMismatch,
                     ReferenceAdapterField::ProjectionQuantityScale);
    }
    if (policy_for_market(dimensions_.wire_market) != dimensions_.projection_policy) {
        return error(ReferenceAdapterErrorCode::ProjectionPolicyMismatch,
                     ReferenceAdapterField::ProjectionPolicy);
    }
    return std::nullopt;
}

std::optional<ReferenceAdapterError>
ReferenceAdapter::validate_depth_limit(const std::optional<std::uint32_t>& depth_limit) {
    if (!depth_limit.has_value()) {
        return std::nullopt;
    }
    if (*depth_limit == 0 ||
        *depth_limit > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())) {
        return error(ReferenceAdapterErrorCode::InvalidDepthLimit,
                     ReferenceAdapterField::DepthLimit);
    }
    return std::nullopt;
}

ReferenceBaselinePrediction ReferenceAdapter::predict_baseline_input(
    const replay::InstallBaselineOp& operation,
    const std::vector<replay::HostQualityFact>& inbound_facts) const {
    if (const auto identity_error = validate_inbound_identity()) {
        return *identity_error;
    }
    const auto level_result =
        predict_baseline_levels(operation.bids, operation.asks, inbound_facts);
    if (const auto* failure = std::get_if<ReferenceAdapterError>(&level_result)) {
        return *failure;
    }
    if (const auto binding_error = validate_binding()) {
        return *binding_error;
    }
    return ReferenceInputPrediction{map_observed_quality(inbound_facts)};
}

ReferenceDepthPrediction ReferenceAdapter::predict_depth_update_input(
    const replay::DepthUpdateOp& operation,
    const std::vector<replay::HostQualityFact>& inbound_facts) const {
    if (const auto identity_error = validate_inbound_identity()) {
        return *identity_error;
    }
    if (operation.first_update_id > operation.final_update_id) {
        return error(ReferenceAdapterErrorCode::InvalidUpdateRange,
                     ReferenceAdapterField::FinalUpdateId);
    }
    const auto level_result = predict_update_levels(operation.levels, inbound_facts);
    if (const auto* failure = std::get_if<ReferenceAdapterError>(&level_result)) {
        return *failure;
    }
    if (const auto binding_error = validate_binding()) {
        return *binding_error;
    }
    return ReferenceInputPrediction{map_observed_quality(inbound_facts)};
}

ReferenceBaselinePrediction ReferenceAdapter::predict_baseline_levels(
    const std::vector<replay::LevelInput>& bids, const std::vector<replay::LevelInput>& asks,
    const std::vector<replay::HostQualityFact>& inbound_facts) const {
    const auto adapt_baseline_side = [&](const std::vector<replay::LevelInput>& levels,
                                         bool bids_side) -> std::optional<ReferenceAdapterError> {
        std::optional<std::int64_t> previous_price;
        for (const auto& level : levels) {
            const auto price_field =
                bids_side ? ReferenceAdapterField::BidPrice : ReferenceAdapterField::AskPrice;
            if (const auto failure = adapt_level(level, dimensions_.conversion_numeric_spec)) {
                return failure;
            }
            const auto price = parse_reference_decimal(
                level.price, dimensions_.conversion_numeric_spec.price_scale, false);
            const auto units = std::get<ReferenceDecimalValue>(price.value).units;
            if (previous_price.has_value() &&
                (bids_side ? units >= *previous_price : units <= *previous_price)) {
                return error(ReferenceAdapterErrorCode::InvalidOrdering, price_field);
            }
            previous_price = units;
        }
        return std::nullopt;
    };
    if (const auto failure = adapt_baseline_side(bids, true)) {
        return *failure;
    }
    if (const auto failure = adapt_baseline_side(asks, false)) {
        return *failure;
    }
    return ReferenceInputPrediction{map_observed_quality(inbound_facts)};
}

ReferenceDepthPrediction ReferenceAdapter::predict_update_levels(
    const std::vector<replay::LevelInput>& levels,
    const std::vector<replay::HostQualityFact>& inbound_facts) const {
    for (const auto side : {replay::Side::Bid, replay::Side::Ask}) {
        for (const auto& level : levels) {
            if (level.side == side) {
                if (const auto failure = adapt_level(level, dimensions_.conversion_numeric_spec)) {
                    return *failure;
                }
            }
        }
    }
    return ReferenceInputPrediction{map_observed_quality(inbound_facts)};
}

std::vector<ReferenceQualityFlag>
ReferenceAdapter::map_observed_quality(const std::vector<replay::HostQualityFact>& facts) {
    std::vector<ReferenceQualityFlag> flags;
    flags.reserve(facts.size());
    for (const auto fact : facts) {
        flags.push_back(map_inbound_fact(fact));
    }
    return deduplicate_and_rank(std::move(flags));
}

ReferenceSnapshotResult
ReferenceAdapter::predict_snapshot(const bmd_projection_reference::ReferenceProjection& projection,
                                   const replay::SnapshotRequestOp& operation) const {
    if (const auto depth_error = validate_depth_limit(operation.depth_limit)) {
        return *depth_error;
    }
    const auto status = projection.status();
    if (status == bmd_projection_reference::Status::AwaitingBaseline) {
        return error(ReferenceAdapterErrorCode::MissingLastUpdateId,
                     ReferenceAdapterField::LastUpdateId);
    }
    if (!projection.last().has_value()) {
        return error(ReferenceAdapterErrorCode::MissingLastUpdateId,
                     ReferenceAdapterField::LastUpdateId);
    }
    if (const auto identity_error = validate_snapshot_identity()) {
        return *identity_error;
    }
    if (!is_non_empty_text(operation.producer)) {
        return error(operation.producer.empty() ? ReferenceAdapterErrorCode::MissingRequiredField
                                                : ReferenceAdapterErrorCode::InvalidIdentifier,
                     ReferenceAdapterField::Producer);
    }
    if (!is_non_empty_text(operation.producer_version)) {
        return error(operation.producer_version.empty()
                         ? ReferenceAdapterErrorCode::MissingRequiredField
                         : ReferenceAdapterErrorCode::InvalidIdentifier,
                     ReferenceAdapterField::ProducerVersion);
    }
    const bool needs_resync = status == bmd_projection_reference::Status::NeedsResync;
    if (needs_resync != operation.current_gap.has_value()) {
        return error(needs_resync ? ReferenceAdapterErrorCode::MissingRequiredField
                                  : ReferenceAdapterErrorCode::InvalidGapContext,
                     ReferenceAdapterField::CurrentGap);
    }

    ReferenceSnapshotPrediction prediction;
    prediction.synchronized = status == bmd_projection_reference::Status::Synchronized;
    prediction.last_update_id = projection.last();

    std::vector<ReferenceQualityFlag> flags;
    flags.reserve(operation.host_quality_facts.size() + 3);
    if (const auto quality_error = predict_host_quality(projection, operation, flags)) {
        return *quality_error;
    }
    prediction.quality_flags = deduplicate_and_rank(std::move(flags));

    const auto& bids = projection.bids();
    const auto& asks = projection.asks();
    const auto select_levels = [&](const std::vector<bmd_projection_reference::RawLevel>& levels)
        -> std::vector<ReferenceSnapshotLevel> {
        const auto limit = operation.depth_limit.has_value()
                               ? static_cast<std::size_t>(*operation.depth_limit)
                               : levels.size();
        std::vector<ReferenceSnapshotLevel> selected;
        selected.reserve(std::min(limit, levels.size()));
        std::size_t count = 0;
        for (const auto& level : levels) {
            if (count == limit) {
                break;
            }
            selected.push_back(
                {reference_fixed(ReferenceFixedInput{
                     level.price, dimensions_.projection_numeric_spec.price_scale}),
                 reference_fixed(ReferenceFixedInput{
                     level.quantity, dimensions_.projection_numeric_spec.quantity_scale})});
            ++count;
        }
        return selected;
    };
    prediction.bids = select_levels(bids);
    prediction.asks = select_levels(asks);
    prediction.depth_limit = operation.depth_limit;
    if (needs_resync) {
        prediction.gap_descriptor = predict_gap_descriptor(projection, operation);
        if (!prediction.gap_descriptor.has_value()) {
            return error(ReferenceAdapterErrorCode::InvalidGapContext,
                         ReferenceAdapterField::GapRecoveryState);
        }
    }
    return prediction;
}

std::optional<ReferenceAdapterError> ReferenceAdapter::predict_host_quality(
    const bmd_projection_reference::ReferenceProjection& projection,
    const replay::SnapshotRequestOp& operation, std::vector<ReferenceQualityFlag>& flags) {
    const auto status = projection.status();
    for (const auto fact : operation.host_quality_facts) {
        if (!host_fact_valid_for_status(fact, status)) {
            return error(ReferenceAdapterErrorCode::InvalidHostQualityCombination,
                         ReferenceAdapterField::HostQualityFact);
        }
        flags.push_back(map_inbound_fact(fact));
    }
    if (status == bmd_projection_reference::Status::AwaitingBridge) {
        flags.push_back(ReferenceQualityFlag::SnapshotBridgePending);
    }
    if (status == bmd_projection_reference::Status::NeedsResync) {
        flags.push_back(ReferenceQualityFlag::SequenceGap);
    }
    const auto& bids = projection.bids();
    const auto& asks = projection.asks();
    if (!bids.empty() && !asks.empty() && bids.front().price >= asks.front().price) {
        flags.push_back(ReferenceQualityFlag::CrossedBook);
    }
    return std::nullopt;
}

std::optional<ReferenceGapDescriptor> ReferenceAdapter::predict_gap_descriptor(
    const bmd_projection_reference::ReferenceProjection& projection,
    const replay::SnapshotRequestOp& operation) {
    const auto gap = projection.last_gap();
    if (!gap.has_value() || !operation.current_gap.has_value()) {
        return std::nullopt;
    }
    const auto recovery = map_recovery(operation.current_gap->second);
    if (!recovery.has_value()) {
        return std::nullopt;
    }
    return ReferenceGapDescriptor{operation.current_gap->first, gap->last, gap->first,
                                  ReferenceReasonCode::SequenceGapDetected, *recovery};
}

} // namespace bmd_projection::m5::reference
