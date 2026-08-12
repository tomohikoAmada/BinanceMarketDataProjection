#include "canonical_observation.hpp"

#include <cstdint>
#include <optional>
#include <string>
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
    return "UNKNOWN_EVENT_KIND_" + std::to_string(static_cast<int>(kind));
}

[[nodiscard]] std::string book_side_name(CanonicalBookSide side) {
    switch (side) {
    case CanonicalBookSide::Bid:
        return "Bid";
    case CanonicalBookSide::Ask:
        return "Ask";
    }
    return "UNKNOWN_SIDE_" + std::to_string(static_cast<int>(side));
}

[[nodiscard]] std::string decimal_role_name(CanonicalDecimalRole role) {
    switch (role) {
    case CanonicalDecimalRole::Price:
        return "Price";
    case CanonicalDecimalRole::Quantity:
        return "Quantity";
    }
    return "UNKNOWN_ROLE_" + std::to_string(static_cast<int>(role));
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
    return "UNKNOWN_DECIMAL_ERROR_" + std::to_string(static_cast<int>(error));
}

[[nodiscard]] std::string disposition_name(CanonicalDisposition d) {
    switch (d) {
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
    return "UNKNOWN_DISPOSITION_" + std::to_string(static_cast<int>(d));
}

[[nodiscard]] std::string status_name(CanonicalStatus s) {
    switch (s) {
    case CanonicalStatus::AwaitingBaseline:
        return "AwaitingBaseline";
    case CanonicalStatus::AwaitingBridge:
        return "AwaitingBridge";
    case CanonicalStatus::Synchronized:
        return "Synchronized";
    case CanonicalStatus::NeedsResync:
        return "NeedsResync";
    }
    return "UNKNOWN_STATUS_" + std::to_string(static_cast<int>(s));
}

[[nodiscard]] std::string gap_reason_name(CanonicalGapReason r) {
    switch (r) {
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
    return "UNKNOWN_GAP_REASON_" + std::to_string(static_cast<int>(r));
}

[[nodiscard]] std::string policy_name(CanonicalPolicy p) {
    switch (p) {
    case CanonicalPolicy::Spot:
        return "Spot";
    case CanonicalPolicy::UsdMPerpetual:
        return "UsdMPerpetual";
    }
    return "UNKNOWN_POLICY_" + std::to_string(static_cast<int>(p));
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
    return "UNKNOWN_ADAPTER_CODE_" + std::to_string(static_cast<int>(code));
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
    return "UNKNOWN_ADAPTER_FIELD_" + std::to_string(static_cast<int>(field));
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
    return "UNKNOWN_QUALITY_FLAG_" + std::to_string(static_cast<int>(flag));
}

[[nodiscard]] std::string snapshot_source_name(CanonicalSnapshotSource src) {
    switch (src) {
    case CanonicalSnapshotSource::GatewayLive:
        return "GatewayLive";
    case CanonicalSnapshotSource::RecorderReplay:
        return "RecorderReplay";
    case CanonicalSnapshotSource::HistoryReplay:
        return "HistoryReplay";
    }
    return "UNKNOWN_SNAPSHOT_SOURCE_" + std::to_string(static_cast<int>(src));
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
    return "UNKNOWN_RESYNC_STATE_" + std::to_string(static_cast<int>(state));
}

[[nodiscard]] std::string reason_code_name(CanonicalReasonCode code) {
    switch (code) {
    case CanonicalReasonCode::SequenceGapDetected:
        return "SequenceGapDetected";
    }
    return "UNKNOWN_REASON_CODE_" + std::to_string(static_cast<int>(code));
}

// ---- helpers ----

void write_u64(std::string& out, std::uint64_t value) { out += std::to_string(value); }

void write_u32(std::string& out, std::uint32_t value) { out += std::to_string(value); }

void write_i64(std::string& out, std::int64_t value) { out += std::to_string(value); }

void write_bool(std::string& out, bool value) { out += value ? "true" : "false"; }

void write_size(std::string& out, std::size_t value) { out += std::to_string(value); }

void write_string(std::string& out, const std::string& value) {
    write_size(out, value.size());
    out += ':';
    out += value;
}

void write_optional_u64(std::string& out, const std::optional<std::uint64_t>& value) {
    if (value.has_value()) {
        out += "some ";
        write_u64(out, *value);
    } else {
        out += "none";
    }
}

void write_optional_u32(std::string& out, const std::optional<std::uint32_t>& value) {
    if (value.has_value()) {
        out += "some ";
        write_u32(out, *value);
    } else {
        out += "none";
    }
}

void write_optional_decimal_error(std::string& out,
                                  const std::optional<CanonicalDecimalError>& value) {
    if (value.has_value()) {
        out += "some ";
        out += decimal_error_name(*value);
    } else {
        out += "none";
    }
}

// ---- decimal observations ----

void serialize_decimal_result(std::string& out, const CanonicalDecimalResult& result);

void serialize_decimal_value(std::string& out, const CanonicalDecimalValue& value) {
    out += "VALUE ";
    write_i64(out, value.units);
    out += ' ';
    write_u32(out, value.storage_scale);
    out += ' ';
    write_size(out, value.source_fraction_digits);
}

void serialize_decimal_failure(std::string& out, const CanonicalDecimalError& category,
                               std::size_t offset) {
    out += "FAILURE ";
    out += decimal_error_name(category);
    out += ' ';
    write_size(out, offset);
}

void serialize_decimal_result(std::string& out, const CanonicalDecimalResult& result) {
    if (const auto* value = std::get_if<CanonicalDecimalValue>(&result)) {
        serialize_decimal_value(out, *value);
        return;
    }
    const auto& failure = std::get<bmd_projection::m5::oracle::CanonicalDecimalFailure>(result);
    serialize_decimal_failure(out, failure.category, failure.offset);
}

void serialize_decimal_observations(std::string& out,
                                    const std::vector<CanonicalDecimalObservation>& decimals) {
    write_size(out, decimals.size());
    for (const auto& obs : decimals) {
        out += '\n';
        out += "    DECIMAL ";
        out += book_side_name(obs.side);
        out += ' ';
        write_size(out, obs.level_position);
        out += ' ';
        out += decimal_role_name(obs.role);
        out += ' ';
        serialize_decimal_result(out, obs.result);
    }
}

// ---- operation result ----

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
    for (const auto& flag : flags) {
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

void serialize_result(std::string& out, const OperationResultValue& value);

void serialize_decimal_error_outcome(std::string& out,
                                     const bmd_projection::m5::oracle::DecimalErrorOutcome& de) {
    out += "DecimalErrorOutcome ";
    out += decimal_error_name(de.category);
}

void serialize_install_outcome(std::string& out,
                               const bmd_projection::m5::oracle::InstallOutcome& inst) {
    out += "InstallOutcome ";
    out += disposition_name(inst.disposition);
    out += ' ';
    out += status_name(inst.status_after);
    out += ' ';
    write_optional_u64(out, inst.last_update_id_after);
}

void serialize_apply_outcome(std::string& out,
                             const bmd_projection::m5::oracle::ApplyOutcome& apply) {
    out += "ApplyOutcome ";
    out += disposition_name(apply.disposition);
    out += ' ';
    out += status_name(apply.status_after);
    out += ' ';
    write_optional_u64(out, apply.last_update_id_after);
    out += ' ';
    if (apply.gap.has_value()) {
        out += "gap ";
        serialize_gap_evidence(out, *apply.gap);
    } else {
        out += "nogap";
    }
}

void serialize_adapter_error_outcome(std::string& out,
                                     const bmd_projection::m5::oracle::AdapterErrorOutcome& err) {
    out += "AdapterErrorOutcome ";
    out += adapter_code_name(err.code);
    out += ' ';
    out += adapter_field_name(err.field);
    out += ' ';
    write_optional_decimal_error(out, err.decimal_error);
}

void serialize_adapter_success_outcome(
    std::string& out, const bmd_projection::m5::oracle::AdapterSuccessOutcome& success) {
    out += "AdapterSuccessOutcome ";
    if (const auto* inst =
            std::get_if<bmd_projection::m5::oracle::InstallOutcome>(&success.core_result)) {
        serialize_install_outcome(out, *inst);
    } else {
        const auto& apply = std::get<bmd_projection::m5::oracle::ApplyOutcome>(success.core_result);
        serialize_apply_outcome(out, apply);
    }
    out += ' ';
    serialize_quality_flags(out, success.observed_quality);
}

void serialize_snapshot_outcome(std::string& out,
                                const bmd_projection::m5::oracle::SnapshotOutcome& snap) {
    out += "SnapshotOutcome ";
    out += policy_name(snap.policy);
    out += ' ';
    write_string(out, snap.symbol);
    out += ' ';
    write_string(out, snap.producer);
    out += ' ';
    write_string(out, snap.producer_version);
    out += ' ';
    out += snapshot_source_name(snap.source);
    out += ' ';
    write_u64(out, snap.generated_time_utc_ns);
    out += ' ';
    write_optional_u64(out, snap.generated_monotonic_ns);
    out += ' ';
    write_optional_u64(out, snap.last_update_id);
    out += ' ';
    write_bool(out, snap.synchronized);
    out += " \n      BIDS ";
    serialize_snapshot_levels(out, snap.bids);
    out += " \n      ASKS ";
    serialize_snapshot_levels(out, snap.asks);
    out += ' ';
    serialize_quality_flags(out, snap.quality_flags);
    out += ' ';
    write_optional_u32(out, snap.depth_limit);
    out += ' ';
    if (snap.gap_descriptor.has_value()) {
        out += "gapdesc ";
        serialize_gap_descriptor(out, *snap.gap_descriptor);
    } else {
        out += "nogapdesc";
    }
}

void serialize_snapshot_not_produced(std::string& out) { out += "SnapshotNotProducedOutcome"; }

void serialize_reset_outcome(std::string& out) { out += "ResetOutcome"; }

void serialize_range_outcome(std::string& out,
                             const bmd_projection::m5::oracle::RangeOutcome& range) {
    out += "RangeOutcome ";
    write_bool(out, range.invalid_as_intended);
}

void serialize_metadata_outcome(std::string& out) { out += "MetadataOutcome"; }

void serialize_result(std::string& out, const OperationResultValue& value) {
    if (const auto* de = std::get_if<bmd_projection::m5::oracle::DecimalErrorOutcome>(&value)) {
        serialize_decimal_error_outcome(out, *de);
    } else if (const auto* inst = std::get_if<bmd_projection::m5::oracle::InstallOutcome>(&value)) {
        serialize_install_outcome(out, *inst);
    } else if (const auto* apply = std::get_if<bmd_projection::m5::oracle::ApplyOutcome>(&value)) {
        serialize_apply_outcome(out, *apply);
    } else if (const auto* ae =
                   std::get_if<bmd_projection::m5::oracle::AdapterErrorOutcome>(&value)) {
        serialize_adapter_error_outcome(out, *ae);
    } else if (const auto* as =
                   std::get_if<bmd_projection::m5::oracle::AdapterSuccessOutcome>(&value)) {
        serialize_adapter_success_outcome(out, *as);
    } else if (const auto* snap =
                   std::get_if<bmd_projection::m5::oracle::SnapshotOutcome>(&value)) {
        serialize_snapshot_outcome(out, *snap);
    } else if (std::holds_alternative<bmd_projection::m5::oracle::SnapshotNotProducedOutcome>(
                   value)) {
        serialize_snapshot_not_produced(out);
    } else if (std::holds_alternative<bmd_projection::m5::oracle::ResetOutcome>(value)) {
        serialize_reset_outcome(out);
    } else if (const auto* range = std::get_if<bmd_projection::m5::oracle::RangeOutcome>(&value)) {
        serialize_range_outcome(out, *range);
    } else {
        serialize_metadata_outcome(out);
    }
}

// ---- checkpoint ----

void serialize_checkpoint(std::string& out,
                          const bmd_projection::m5::oracle::SemanticCheckpoint& checkpoint) {
    out += "CHECKPOINT ";
    out += status_name(checkpoint.status);
    out += ' ';
    write_optional_u64(out, checkpoint.last_update_id);
    out += ' ';
    if (checkpoint.last_gap.has_value()) {
        out += "gap ";
        serialize_gap_evidence(out, *checkpoint.last_gap);
    } else {
        out += "nogap";
    }
    out += ' ';
    write_bool(out, checkpoint.synchronized_visible);
    out += "\n    BIDS ";
    serialize_levels(out, checkpoint.bids);
    out += "\n    ASKS ";
    serialize_levels(out, checkpoint.asks);
    out += "\n    SCALES ";
    write_u32(out, checkpoint.price_scale);
    out += ' ';
    write_u32(out, checkpoint.quantity_scale);
}

// ---- snapshot (optional) ----

void serialize_snapshot_optional(
    std::string& out, const std::optional<bmd_projection::m5::oracle::SnapshotOutcome>& snapshot) {
    if (snapshot.has_value()) {
        out += "SNAPSHOT ";
        serialize_snapshot_outcome(out, *snapshot);
    } else {
        out += "SNAPSHOT NONE";
    }
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
    result += "  RESULT ";
    serialize_result(result, observation.result.value);
    result += '\n';
    result += "  ";
    serialize_checkpoint(result, observation.checkpoint);
    result += '\n';
    result += "  ";
    serialize_snapshot_optional(result, observation.snapshot);
    if (!observation.decimal_observations.empty()) {
        result += '\n';
        result += "  DECIMALS ";
        serialize_decimal_observations(result, observation.decimal_observations);
    }
    return result;
}

std::vector<std::string>
serialize_observation_stream(const std::vector<OperationObservation>& observations) {
    std::vector<std::string> result;
    result.reserve(observations.size());
    for (const auto& obs : observations) {
        result.push_back(serialize_observation(obs));
    }
    return result;
}

} // namespace bmd_projection::m5::semantic
