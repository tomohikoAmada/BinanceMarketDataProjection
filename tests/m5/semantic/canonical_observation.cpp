#include "canonical_observation.hpp"

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

namespace bmd_projection::m5::semantic {
namespace {

using bmd_projection::m5::oracle::CanonicalAdapterCode;
using bmd_projection::m5::oracle::CanonicalAdapterField;
using bmd_projection::m5::oracle::CanonicalBookSide;
using bmd_projection::m5::oracle::CanonicalDecimalError;
using bmd_projection::m5::oracle::CanonicalDecimalObservation;
using bmd_projection::m5::oracle::CanonicalDecimalResult;
using bmd_projection::m5::oracle::CanonicalDecimalRole;
using bmd_projection::m5::oracle::CanonicalDecimalValue;
using bmd_projection::m5::oracle::CanonicalDisposition;
using bmd_projection::m5::oracle::CanonicalGapReason;
using bmd_projection::m5::oracle::CanonicalPolicy;
using bmd_projection::m5::oracle::CanonicalQualityFlag;
using bmd_projection::m5::oracle::CanonicalReasonCode;
using bmd_projection::m5::oracle::CanonicalResyncState;
using bmd_projection::m5::oracle::CanonicalSnapshotSource;
using bmd_projection::m5::oracle::CanonicalStatus;
using bmd_projection::m5::oracle::OperationObservation;
using bmd_projection::m5::oracle::OperationResultValue;
using bmd_projection::m5::replay::EventKind;

template <class... Callables> struct Overloaded final : Callables... {
    using Callables::operator()...;
};

template <class... Callables> Overloaded(Callables...) -> Overloaded<Callables...>;

template <typename Enum>
[[noreturn]] void throw_invalid_enum(std::string_view type_name, Enum value) {
    using Underlying = std::underlying_type_t<Enum>;
    throw std::invalid_argument(
        "invalid " + std::string(type_name) +
        " value: " + std::to_string(static_cast<std::int64_t>(static_cast<Underlying>(value))));
}

[[nodiscard]] std::string event_kind_name(EventKind kind) {
    switch (kind) {
    case EventKind::InstallBaseline:
        return "INSTALL_BASELINE";
    case EventKind::DepthUpdate:
        return "DEPTH_UPDATE";
    case EventKind::Rebaseline:
        return "REBASELINE";
    case EventKind::Reset:
        return "RESET";
    case EventKind::SnapshotRequest:
        return "SNAPSHOT_REQUEST";
    case EventKind::AdapterMetadata:
        return "ADAPTER_METADATA";
    case EventKind::MalformedRange:
        return "MALFORMED_RANGE";
    }
    throw_invalid_enum("EventKind", kind);
}

[[nodiscard]] std::string book_side_name(CanonicalBookSide side) {
    switch (side) {
    case CanonicalBookSide::Bid:
        return "Bid";
    case CanonicalBookSide::Ask:
        return "Ask";
    }
    throw_invalid_enum("CanonicalBookSide", side);
}

[[nodiscard]] std::string decimal_role_name(CanonicalDecimalRole role) {
    switch (role) {
    case CanonicalDecimalRole::Price:
        return "Price";
    case CanonicalDecimalRole::Quantity:
        return "Quantity";
    }
    throw_invalid_enum("CanonicalDecimalRole", role);
}

[[nodiscard]] std::string decimal_error_name(CanonicalDecimalError error) {
    switch (error) {
    case CanonicalDecimalError::Empty:
        return "Empty";
    case CanonicalDecimalError::InvalidSyntax:
        return "InvalidSyntax";
    case CanonicalDecimalError::SignNotAllowed:
        return "SignNotAllowed";
    case CanonicalDecimalError::LeadingZero:
        return "LeadingZero";
    case CanonicalDecimalError::MissingFractionDigits:
        return "MissingFractionDigits";
    case CanonicalDecimalError::ZeroNotAllowed:
        return "ZeroNotAllowed";
    case CanonicalDecimalError::InexactScale:
        return "InexactScale";
    case CanonicalDecimalError::Overflow:
        return "Overflow";
    }
    throw_invalid_enum("CanonicalDecimalError", error);
}

[[nodiscard]] std::string disposition_name(CanonicalDisposition disposition) {
    switch (disposition) {
    case CanonicalDisposition::Installed:
        return "Installed";
    case CanonicalDisposition::Applied:
        return "Applied";
    case CanonicalDisposition::IgnoredStale:
        return "IgnoredStale";
    case CanonicalDisposition::IgnoredDuplicate:
        return "IgnoredDuplicate";
    case CanonicalDisposition::GapDetected:
        return "GapDetected";
    case CanonicalDisposition::RejectedWrongState:
        return "RejectedWrongState";
    }
    throw_invalid_enum("CanonicalDisposition", disposition);
}

[[nodiscard]] std::string status_name(CanonicalStatus status) {
    switch (status) {
    case CanonicalStatus::AwaitingBaseline:
        return "AwaitingBaseline";
    case CanonicalStatus::AwaitingBridge:
        return "AwaitingBridge";
    case CanonicalStatus::Synchronized:
        return "Synchronized";
    case CanonicalStatus::NeedsResync:
        return "NeedsResync";
    }
    throw_invalid_enum("CanonicalStatus", status);
}

[[nodiscard]] std::string gap_reason_name(CanonicalGapReason reason) {
    switch (reason) {
    case CanonicalGapReason::SpotBootstrapForwardGap:
        return "SpotBootstrapForwardGap";
    case CanonicalGapReason::SpotLiveForwardGap:
        return "SpotLiveForwardGap";
    case CanonicalGapReason::FuturesBootstrapRangeMiss:
        return "FuturesBootstrapRangeMiss";
    case CanonicalGapReason::FuturesMissingPreviousFinal:
        return "FuturesMissingPreviousFinal";
    case CanonicalGapReason::FuturesPreviousFinalMismatch:
        return "FuturesPreviousFinalMismatch";
    }
    throw_invalid_enum("CanonicalGapReason", reason);
}

[[nodiscard]] std::string policy_name(CanonicalPolicy policy) {
    switch (policy) {
    case CanonicalPolicy::Spot:
        return "Spot";
    case CanonicalPolicy::UsdMPerpetual:
        return "UsdMPerpetual";
    }
    throw_invalid_enum("CanonicalPolicy", policy);
}

[[nodiscard]] std::string adapter_code_name(CanonicalAdapterCode code) {
    switch (code) {
    case CanonicalAdapterCode::UnsupportedVenue:
        return "UnsupportedVenue";
    case CanonicalAdapterCode::UnsupportedMarket:
        return "UnsupportedMarket";
    case CanonicalAdapterCode::UnexpectedStream:
        return "UnexpectedStream";
    case CanonicalAdapterCode::IdentityMismatch:
        return "IdentityMismatch";
    case CanonicalAdapterCode::UnsupportedSchemaVersion:
        return "UnsupportedSchemaVersion";
    case CanonicalAdapterCode::UnspecifiedEnum:
        return "UnspecifiedEnum";
    case CanonicalAdapterCode::UnknownEnumValue:
        return "UnknownEnumValue";
    case CanonicalAdapterCode::InvalidUpdateRange:
        return "InvalidUpdateRange";
    case CanonicalAdapterCode::MissingRequiredField:
        return "MissingRequiredField";
    case CanonicalAdapterCode::InvalidIdentifier:
        return "InvalidIdentifier";
    case CanonicalAdapterCode::InvalidDecimal:
        return "InvalidDecimal";
    case CanonicalAdapterCode::NegativeQuantity:
        return "NegativeQuantity";
    case CanonicalAdapterCode::NonPositivePrice:
        return "NonPositivePrice";
    case CanonicalAdapterCode::ScaleMismatch:
        return "ScaleMismatch";
    case CanonicalAdapterCode::NumericOverflow:
        return "NumericOverflow";
    case CanonicalAdapterCode::InvalidDepthLimit:
        return "InvalidDepthLimit";
    case CanonicalAdapterCode::InvalidOrdering:
        return "InvalidOrdering";
    case CanonicalAdapterCode::UnsupportedProjectionState:
        return "UnsupportedProjectionState";
    case CanonicalAdapterCode::MissingLastUpdateId:
        return "MissingLastUpdateId";
    case CanonicalAdapterCode::InvalidGapContext:
        return "InvalidGapContext";
    case CanonicalAdapterCode::InvalidHostQualityCombination:
        return "InvalidHostQualityCombination";
    case CanonicalAdapterCode::ContractsVersionMismatch:
        return "ContractsVersionMismatch";
    case CanonicalAdapterCode::ProjectionNumericSpecMismatch:
        return "ProjectionNumericSpecMismatch";
    case CanonicalAdapterCode::ProjectionPolicyMismatch:
        return "ProjectionPolicyMismatch";
    }
    throw_invalid_enum("CanonicalAdapterCode", code);
}

[[nodiscard]] std::string adapter_field_name(CanonicalAdapterField field) {
    switch (field) {
    case CanonicalAdapterField::None:
        return "None";
    case CanonicalAdapterField::Venue:
        return "Venue";
    case CanonicalAdapterField::Market:
        return "Market";
    case CanonicalAdapterField::Stream:
        return "Stream";
    case CanonicalAdapterField::Symbol:
        return "Symbol";
    case CanonicalAdapterField::SchemaVersion:
        return "SchemaVersion";
    case CanonicalAdapterField::Producer:
        return "Producer";
    case CanonicalAdapterField::ProducerVersion:
        return "ProducerVersion";
    case CanonicalAdapterField::RequestId:
        return "RequestId";
    case CanonicalAdapterField::ConnectionId:
        return "ConnectionId";
    case CanonicalAdapterField::FirstUpdateId:
        return "FirstUpdateId";
    case CanonicalAdapterField::FinalUpdateId:
        return "FinalUpdateId";
    case CanonicalAdapterField::PreviousFinalUpdateId:
        return "PreviousFinalUpdateId";
    case CanonicalAdapterField::BidPrice:
        return "BidPrice";
    case CanonicalAdapterField::BidQuantity:
        return "BidQuantity";
    case CanonicalAdapterField::AskPrice:
        return "AskPrice";
    case CanonicalAdapterField::AskQuantity:
        return "AskQuantity";
    case CanonicalAdapterField::QualityFlag:
        return "QualityFlag";
    case CanonicalAdapterField::ProjectionPriceScale:
        return "ProjectionPriceScale";
    case CanonicalAdapterField::ProjectionQuantityScale:
        return "ProjectionQuantityScale";
    case CanonicalAdapterField::ProjectionPolicy:
        return "ProjectionPolicy";
    case CanonicalAdapterField::DepthLimit:
        return "DepthLimit";
    case CanonicalAdapterField::SnapshotSource:
        return "SnapshotSource";
    case CanonicalAdapterField::LastUpdateId:
        return "LastUpdateId";
    case CanonicalAdapterField::CurrentGap:
        return "CurrentGap";
    case CanonicalAdapterField::GapRecoveryState:
        return "GapRecoveryState";
    case CanonicalAdapterField::HostQualityFact:
        return "HostQualityFact";
    }
    throw_invalid_enum("CanonicalAdapterField", field);
}

[[nodiscard]] std::string quality_flag_name(CanonicalQualityFlag flag) {
    switch (flag) {
    case CanonicalQualityFlag::Duplicate:
        return "Duplicate";
    case CanonicalQualityFlag::OutOfOrder:
        return "OutOfOrder";
    case CanonicalQualityFlag::SequenceGap:
        return "SequenceGap";
    case CanonicalQualityFlag::OrderBookResync:
        return "OrderBookResync";
    case CanonicalQualityFlag::SnapshotBridgePending:
        return "SnapshotBridgePending";
    case CanonicalQualityFlag::SnapshotTooOld:
        return "SnapshotTooOld";
    case CanonicalQualityFlag::BootstrapBufferOverflow:
        return "BootstrapBufferOverflow";
    case CanonicalQualityFlag::RecoveredTail:
        return "RecoveredTail";
    case CanonicalQualityFlag::MalformedPayload:
        return "MalformedPayload";
    case CanonicalQualityFlag::ExchangeTimeMissing:
        return "ExchangeTimeMissing";
    case CanonicalQualityFlag::ReceiveClockDiscontinuity:
        return "ReceiveClockDiscontinuity";
    case CanonicalQualityFlag::SlowConsumerGap:
        return "SlowConsumerGap";
    case CanonicalQualityFlag::ProducerRestart:
        return "ProducerRestart";
    case CanonicalQualityFlag::Overlap:
        return "Overlap";
    case CanonicalQualityFlag::IdentityConflict:
        return "IdentityConflict";
    case CanonicalQualityFlag::CrossedBook:
        return "CrossedBook";
    }
    throw_invalid_enum("CanonicalQualityFlag", flag);
}

[[nodiscard]] std::string snapshot_source_name(CanonicalSnapshotSource source) {
    switch (source) {
    case CanonicalSnapshotSource::GatewayLive:
        return "GatewayLive";
    case CanonicalSnapshotSource::RecorderReplay:
        return "RecorderReplay";
    case CanonicalSnapshotSource::HistoryReplay:
        return "HistoryReplay";
    }
    throw_invalid_enum("CanonicalSnapshotSource", source);
}

[[nodiscard]] std::string resync_state_name(CanonicalResyncState state) {
    switch (state) {
    case CanonicalResyncState::ResyncRequired:
        return "ResyncRequired";
    case CanonicalResyncState::ResyncInProgress:
        return "ResyncInProgress";
    case CanonicalResyncState::ResyncFailed:
        return "ResyncFailed";
    }
    throw_invalid_enum("CanonicalResyncState", state);
}

[[nodiscard]] std::string reason_code_name(CanonicalReasonCode code) {
    switch (code) {
    case CanonicalReasonCode::SequenceGapDetected:
        return "SequenceGapDetected";
    }
    throw_invalid_enum("CanonicalReasonCode", code);
}

void write_u64(std::string& out, std::uint64_t value) { out += std::to_string(value); }

void write_u32(std::string& out, std::uint32_t value) { out += std::to_string(value); }

void write_i64(std::string& out, std::int64_t value) { out += std::to_string(value); }

void write_bool(std::string& out, bool value) { out += value ? "true" : "false"; }

void write_size(std::string& out, std::size_t value) { out += std::to_string(value); }

void write_string(std::string& out, const std::string& value) {
    static constexpr std::string_view kHex = "0123456789abcdef";
    write_size(out, value.size());
    out += ':';
    for (const char character : value) {
        const auto byte = static_cast<unsigned char>(character);
        out += kHex.at(static_cast<std::size_t>(byte >> 4U));
        out += kHex.at(static_cast<std::size_t>(byte & 0x0FU));
    }
}

void write_optional_u64(std::string& out, const std::optional<std::uint64_t>& value) {
    if (value.has_value()) {
        write_u64(out, *value);
    } else {
        out += '-';
    }
}

void write_optional_u32(std::string& out, const std::optional<std::uint32_t>& value) {
    if (value.has_value()) {
        write_u32(out, *value);
    } else {
        out += '-';
    }
}

void write_optional_decimal_error(std::string& out,
                                  const std::optional<CanonicalDecimalError>& value) {
    if (value.has_value()) {
        out += decimal_error_name(*value);
    } else {
        out += '-';
    }
}

void serialize_decimal_value(std::string& out, const CanonicalDecimalValue& value) {
    out += "VALUE ";
    write_i64(out, value.units);
    out += ' ';
    write_u32(out, value.storage_scale);
    out += ' ';
    write_size(out, value.source_fraction_digits);
}

void serialize_decimal_failure(std::string& out,
                               const bmd_projection::m5::oracle::CanonicalDecimalFailure& failure) {
    out += "FAILURE ";
    out += decimal_error_name(failure.category);
    out += ' ';
    write_size(out, failure.offset);
}

void serialize_decimal_result(std::string& out, const CanonicalDecimalResult& result) {
    std::visit(
        Overloaded{
            [&out](const CanonicalDecimalValue& value) { serialize_decimal_value(out, value); },
            [&out](const bmd_projection::m5::oracle::CanonicalDecimalFailure& failure) {
                serialize_decimal_failure(out, failure);
            }},
        result);
}

void serialize_gap_evidence(std::string& out,
                            const bmd_projection::m5::oracle::CanonicalGapEvidence& gap) {
    write_u64(out, gap.last_accepted_final);
    out += ' ';
    write_u64(out, gap.first_update_id);
    out += ' ';
    write_u64(out, gap.final_update_id);
    out += ' ';
    write_optional_u64(out, gap.previous_final);
    out += ' ';
    out += gap_reason_name(gap.reason);
    out += ' ';
    out += policy_name(gap.policy);
}

void serialize_levels(std::string& out,
                      const std::vector<bmd_projection::m5::oracle::CanonicalLevel>& levels) {
    write_size(out, levels.size());
    for (const auto& level : levels) {
        out += ' ';
        write_i64(out, level.price);
        out += ' ';
        write_i64(out, level.quantity);
    }
}

void serialize_quality_flags(std::string& out, const std::vector<CanonicalQualityFlag>& flags) {
    write_size(out, flags.size());
    for (const auto flag : flags) {
        out += ' ';
        out += quality_flag_name(flag);
    }
}

void serialize_snapshot_levels(
    std::string& out, const std::vector<bmd_projection::m5::oracle::SnapshotLevel>& levels) {
    write_size(out, levels.size());
    for (const auto& level : levels) {
        out += ' ';
        write_string(out, level.price);
        out += ' ';
        write_string(out, level.quantity);
    }
}

void serialize_gap_descriptor(std::string& out,
                              const bmd_projection::m5::oracle::GapDescriptorObservation& gap) {
    write_u64(out, gap.detected_at_utc_ns);
    out += ' ';
    write_u64(out, gap.previous_sequence);
    out += ' ';
    write_u64(out, gap.next_sequence);
    out += ' ';
    out += reason_code_name(gap.reason_code);
    out += ' ';
    out += resync_state_name(gap.recovery_state);
}

void serialize_decimal_error_outcome(std::string& out,
                                     const bmd_projection::m5::oracle::DecimalErrorOutcome& value) {
    out += "DecimalErrorOutcome ";
    out += decimal_error_name(value.category);
}

void serialize_install_outcome(std::string& out,
                               const bmd_projection::m5::oracle::InstallOutcome& value) {
    out += "InstallOutcome ";
    out += disposition_name(value.disposition);
    out += ' ';
    out += status_name(value.status_after);
    out += " LAST_UPDATE ";
    write_optional_u64(out, value.last_update_id_after);
}

void serialize_apply_outcome(std::string& out,
                             const bmd_projection::m5::oracle::ApplyOutcome& value) {
    out += "ApplyOutcome ";
    out += disposition_name(value.disposition);
    out += ' ';
    out += status_name(value.status_after);
    out += " LAST_UPDATE ";
    write_optional_u64(out, value.last_update_id_after);
    out += " GAP ";
    if (value.gap.has_value()) {
        serialize_gap_evidence(out, *value.gap);
    } else {
        out += '-';
    }
}

void serialize_adapter_error_outcome(std::string& out,
                                     const bmd_projection::m5::oracle::AdapterErrorOutcome& value) {
    out += "AdapterErrorOutcome ";
    out += adapter_code_name(value.code);
    out += ' ';
    out += adapter_field_name(value.field);
    out += " DECIMAL_ERROR ";
    write_optional_decimal_error(out, value.decimal_error);
}

void serialize_adapter_success_outcome(
    std::string& out, const bmd_projection::m5::oracle::AdapterSuccessOutcome& value) {
    out += "AdapterSuccessOutcome ";
    std::visit(Overloaded{[&out](const bmd_projection::m5::oracle::InstallOutcome& result) {
                              serialize_install_outcome(out, result);
                          },
                          [&out](const bmd_projection::m5::oracle::ApplyOutcome& result) {
                              serialize_apply_outcome(out, result);
                          }},
               value.core_result);
    out += " QUALITY ";
    serialize_quality_flags(out, value.observed_quality);
}

void serialize_snapshot_outcome(std::string& out,
                                const bmd_projection::m5::oracle::SnapshotOutcome& value) {
    out += "SnapshotOutcome ";
    out += policy_name(value.policy);
    out += " SYMBOL ";
    write_string(out, value.symbol);
    out += " PRODUCER ";
    write_string(out, value.producer);
    out += " PRODUCER_VERSION ";
    write_string(out, value.producer_version);
    out += " SOURCE ";
    out += snapshot_source_name(value.source);
    out += " GENERATED_UTC_NS ";
    write_u64(out, value.generated_time_utc_ns);
    out += " GENERATED_MONOTONIC_NS ";
    write_optional_u64(out, value.generated_monotonic_ns);
    out += " LAST_UPDATE ";
    write_optional_u64(out, value.last_update_id);
    out += " SYNCHRONIZED ";
    write_bool(out, value.synchronized);
    out += " BIDS ";
    serialize_snapshot_levels(out, value.bids);
    out += " ASKS ";
    serialize_snapshot_levels(out, value.asks);
    out += " QUALITY ";
    serialize_quality_flags(out, value.quality_flags);
    out += " DEPTH_LIMIT ";
    write_optional_u32(out, value.depth_limit);
    out += " GAP_DESCRIPTOR ";
    if (value.gap_descriptor.has_value()) {
        serialize_gap_descriptor(out, *value.gap_descriptor);
    } else {
        out += '-';
    }
}

void serialize_result(std::string& out, const OperationResultValue& value) {
    std::visit(Overloaded{[&out](const bmd_projection::m5::oracle::DecimalErrorOutcome& result) {
                              serialize_decimal_error_outcome(out, result);
                          },
                          [&out](const bmd_projection::m5::oracle::InstallOutcome& result) {
                              serialize_install_outcome(out, result);
                          },
                          [&out](const bmd_projection::m5::oracle::ApplyOutcome& result) {
                              serialize_apply_outcome(out, result);
                          },
                          [&out](const bmd_projection::m5::oracle::AdapterErrorOutcome& result) {
                              serialize_adapter_error_outcome(out, result);
                          },
                          [&out](const bmd_projection::m5::oracle::AdapterSuccessOutcome& result) {
                              serialize_adapter_success_outcome(out, result);
                          },
                          [&out](const bmd_projection::m5::oracle::SnapshotOutcome& result) {
                              serialize_snapshot_outcome(out, result);
                          },
                          [&out](const bmd_projection::m5::oracle::SnapshotNotProducedOutcome&) {
                              out += "SnapshotNotProducedOutcome";
                          },
                          [&out](const bmd_projection::m5::oracle::ResetOutcome&) {
                              out += "ResetOutcome";
                          },
                          [&out](const bmd_projection::m5::oracle::RangeOutcome& result) {
                              out += "RangeOutcome ";
                              write_bool(out, result.invalid_as_intended);
                          },
                          [&out](const bmd_projection::m5::oracle::MetadataOutcome&) {
                              out += "MetadataOutcome";
                          }},
               value);
}

void serialize_checkpoint(std::string& out,
                          const bmd_projection::m5::oracle::SemanticCheckpoint& checkpoint) {
    out += "CHECKPOINT ";
    out += status_name(checkpoint.status);
    out += " LAST_UPDATE ";
    write_optional_u64(out, checkpoint.last_update_id);
    out += " GAP ";
    if (checkpoint.last_gap.has_value()) {
        serialize_gap_evidence(out, *checkpoint.last_gap);
    } else {
        out += '-';
    }
    out += " VISIBLE ";
    write_bool(out, checkpoint.synchronized_visible);
    out += " BIDS ";
    serialize_levels(out, checkpoint.bids);
    out += " ASKS ";
    serialize_levels(out, checkpoint.asks);
    out += " SCALES ";
    write_u32(out, checkpoint.price_scale);
    out += ' ';
    write_u32(out, checkpoint.quantity_scale);
}

void serialize_snapshot_optional(
    std::string& out, const std::optional<bmd_projection::m5::oracle::SnapshotOutcome>& snapshot) {
    out += "SNAPSHOT ";
    if (snapshot.has_value()) {
        serialize_snapshot_outcome(out, *snapshot);
    } else {
        out += '-';
    }
}

void serialize_decimal_observation(std::string& out,
                                   const CanonicalDecimalObservation& observation) {
    out += "DECIMAL ";
    out += book_side_name(observation.side);
    out += ' ';
    write_size(out, observation.level_position);
    out += ' ';
    out += decimal_role_name(observation.role);
    out += ' ';
    serialize_decimal_result(out, observation.result);
}

} // namespace

std::string serialize_observation(const OperationObservation& observation) {
    std::string result;
    result.reserve(4096);
    result += "OBS ";
    write_size(result, observation.event_index);
    result += ' ';
    result += event_kind_name(observation.event_kind);
    result += '\n';
    result += "RESULT ";
    serialize_result(result, observation.result.value);
    result += '\n';
    serialize_checkpoint(result, observation.checkpoint);
    result += '\n';
    serialize_snapshot_optional(result, observation.snapshot);
    result += '\n';
    result += "DECIMALS ";
    write_size(result, observation.decimal_observations.size());
    result += '\n';
    for (const auto& decimal : observation.decimal_observations) {
        serialize_decimal_observation(result, decimal);
        result += '\n';
    }
    return result;
}

std::vector<std::string>
serialize_observation_stream(const std::vector<OperationObservation>& observations) {
    std::vector<std::string> result;
    result.reserve(observations.size());
    for (const auto& observation : observations) {
        result.push_back(serialize_observation(observation));
    }
    return result;
}

} // namespace bmd_projection::m5::semantic
