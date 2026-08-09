#include "divergence.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace bmd_projection::m5::oracle {
namespace {

[[nodiscard]] std::optional<Divergence>
compare_levels(const std::vector<CanonicalLevel>& expected, const char* side,
               const std::vector<CanonicalLevel>& actual, std::size_t event_index,
               replay::EventKind kind, Layer layer, const SemanticCheckpoint& production,
               const SemanticCheckpoint& reference) {
    if (expected.size() != actual.size()) {
        return Divergence{event_index,
                          kind,
                          layer,
                          DivergenceCategory::Checkpoint,
                          std::string(side) + " level count differs",
                          to_canonical_text(production),
                          to_canonical_text(reference),
                          "",
                          ""};
    }
    for (std::size_t index = 0; index < expected.size(); ++index) {
        if (expected[index].price != actual[index].price) {
            return Divergence{event_index,
                              kind,
                              layer,
                              DivergenceCategory::Checkpoint,
                              std::string(side) + "[" + std::to_string(index) + "].price differs",
                              to_canonical_text(production),
                              to_canonical_text(reference),
                              "",
                              ""};
        }
        if (expected[index].quantity != actual[index].quantity) {
            return Divergence{event_index,
                              kind,
                              layer,
                              DivergenceCategory::Checkpoint,
                              std::string(side) + "[" + std::to_string(index) +
                                  "].quantity differs",
                              to_canonical_text(production),
                              to_canonical_text(reference),
                              "",
                              ""};
        }
    }
    return std::nullopt;
}

// Ordered per-type field comparison for the operation result. Each overload is a
// small linear field walk; the exact field order is part of the fixed
// first-divergence discipline.
namespace detail {

template <typename T>
[[nodiscard]] std::optional<Divergence>
compare_result_impl(const T&, const T&, const OperationObservation&, const OperationObservation&,
                    std::string_view, const replay::SourceLocation&) {
    return std::nullopt;
}

} // namespace detail

template <>
[[nodiscard]] std::optional<Divergence> detail::compare_result_impl<DecimalErrorOutcome>(
    const DecimalErrorOutcome& expected, const DecimalErrorOutcome& actual,
    const OperationObservation& production, const OperationObservation& reference,
    std::string_view fixture_identity, const replay::SourceLocation& source) {
    if (expected.category != actual.category) {
        return Divergence{production.event_index,
                          production.event_kind,
                          Layer::R1,
                          DivergenceCategory::OperationResult,
                          "operation result field 'category' differs",
                          to_canonical_text(production.result),
                          to_canonical_text(reference.result),
                          std::string(fixture_identity),
                          source.canonical_line};
    }
    return std::nullopt;
}

template <>
[[nodiscard]] std::optional<Divergence> detail::compare_result_impl<InstallOutcome>(
    const InstallOutcome& expected, const InstallOutcome& actual,
    const OperationObservation& production, const OperationObservation& reference,
    std::string_view fixture_identity, const replay::SourceLocation& source) {
    const auto mismatch = [&](const char* field) {
        return std::optional<Divergence>{
            Divergence{production.event_index, production.event_kind, Layer::R3,
                       DivergenceCategory::OperationResult,
                       std::string("operation result field '") + field + "' differs",
                       to_canonical_text(production.result), to_canonical_text(reference.result),
                       std::string(fixture_identity), source.canonical_line}};
    };
    if (expected.disposition != actual.disposition) {
        return mismatch("disposition");
    }
    if (expected.status_after != actual.status_after) {
        return mismatch("status_after");
    }
    if (expected.last_update_id_after != actual.last_update_id_after) {
        return mismatch("last_update_id_after");
    }
    return std::nullopt;
}

template <>
[[nodiscard]] std::optional<Divergence> detail::compare_result_impl<ApplyOutcome>(
    const ApplyOutcome& expected, const ApplyOutcome& actual,
    const OperationObservation& production, const OperationObservation& reference,
    std::string_view fixture_identity, const replay::SourceLocation& source) {
    const auto mismatch = [&](const char* field) {
        return std::optional<Divergence>{
            Divergence{production.event_index, production.event_kind, Layer::R3,
                       DivergenceCategory::OperationResult,
                       std::string("operation result field '") + field + "' differs",
                       to_canonical_text(production.result), to_canonical_text(reference.result),
                       std::string(fixture_identity), source.canonical_line}};
    };
    if (expected.disposition != actual.disposition) {
        return mismatch("disposition");
    }
    if (expected.status_after != actual.status_after) {
        return mismatch("status_after");
    }
    if (expected.last_update_id_after != actual.last_update_id_after) {
        return mismatch("last_update_id_after");
    }
    if (expected.gap.has_value() != actual.gap.has_value()) {
        return mismatch("gap");
    }
    if (expected.gap.has_value()) {
        const auto& expected_gap = *expected.gap;
        const auto& actual_gap = actual.gap.value();
        if (expected_gap.last_accepted_final != actual_gap.last_accepted_final) {
            return mismatch("gap.last_accepted_final");
        }
        if (expected_gap.first_update_id != actual_gap.first_update_id) {
            return mismatch("gap.first_update_id");
        }
        if (expected_gap.final_update_id != actual_gap.final_update_id) {
            return mismatch("gap.final_update_id");
        }
        if (expected_gap.previous_final != actual_gap.previous_final) {
            return mismatch("gap.previous_final");
        }
        if (expected_gap.reason != actual_gap.reason) {
            return mismatch("gap.reason");
        }
        if (expected_gap.policy != actual_gap.policy) {
            return mismatch("gap.policy");
        }
    }
    return std::nullopt;
}

template <>
[[nodiscard]] std::optional<Divergence> detail::compare_result_impl<AdapterErrorOutcome>(
    const AdapterErrorOutcome& expected, const AdapterErrorOutcome& actual,
    const OperationObservation& production, const OperationObservation& reference,
    std::string_view fixture_identity, const replay::SourceLocation& source) {
    const auto mismatch = [&](const char* field) {
        return std::optional<Divergence>{
            Divergence{production.event_index, production.event_kind, Layer::R4,
                       DivergenceCategory::OperationResult,
                       std::string("operation result field '") + field + "' differs",
                       to_canonical_text(production.result), to_canonical_text(reference.result),
                       std::string(fixture_identity), source.canonical_line}};
    };
    if (expected.code != actual.code) {
        return mismatch("code");
    }
    if (expected.field != actual.field) {
        return mismatch("field");
    }
    if (expected.decimal_error != actual.decimal_error) {
        return mismatch("decimal_error");
    }
    return std::nullopt;
}

template <>
[[nodiscard]] std::optional<Divergence> detail::compare_result_impl<AdapterSuccessOutcome>(
    const AdapterSuccessOutcome& expected, const AdapterSuccessOutcome& actual,
    const OperationObservation& production, const OperationObservation& reference,
    std::string_view fixture_identity, const replay::SourceLocation& source) {
    const auto mismatch = [&](const char* field, Layer layer) {
        return std::optional<Divergence>{
            Divergence{production.event_index, production.event_kind, layer,
                       DivergenceCategory::OperationResult,
                       std::string("operation result field '") + field + "' differs",
                       to_canonical_text(production.result), to_canonical_text(reference.result),
                       std::string(fixture_identity), source.canonical_line}};
    };
    if (expected.core_result.index() != actual.core_result.index()) {
        return mismatch("core_result kind", Layer::R3);
    }
    if (const auto nested = std::visit(
            [&](const auto& expected_core, const auto& actual_core) -> std::optional<Divergence> {
                using ExpectedCore = std::decay_t<decltype(expected_core)>;
                using ActualCore = std::decay_t<decltype(actual_core)>;
                if constexpr (std::is_same_v<ExpectedCore, ActualCore>) {
                    return detail::compare_result_impl<ExpectedCore>(expected_core, actual_core,
                                                                     production, reference,
                                                                     fixture_identity, source);
                }
                return std::nullopt;
            },
            expected.core_result, actual.core_result)) {
        return nested;
    }
    if (expected.observed_quality.size() != actual.observed_quality.size()) {
        return mismatch("observed_quality count", Layer::R4);
    }
    for (std::size_t index = 0; index < expected.observed_quality.size(); ++index) {
        if (expected.observed_quality[index] != actual.observed_quality[index]) {
            return mismatch("observed_quality", Layer::R4);
        }
    }
    return std::nullopt;
}

template <>
[[nodiscard]] std::optional<Divergence> detail::compare_result_impl<SnapshotOutcome>(
    const SnapshotOutcome& expected, const SnapshotOutcome& actual,
    const OperationObservation& production, const OperationObservation& reference,
    std::string_view fixture_identity, const replay::SourceLocation& source) {
    const auto mismatch = [&](const char* field) {
        return std::optional<Divergence>{
            Divergence{production.event_index, production.event_kind, Layer::R4,
                       DivergenceCategory::OperationResult,
                       std::string("operation result field '") + field + "' differs",
                       to_canonical_text(production.result), to_canonical_text(reference.result),
                       std::string(fixture_identity), source.canonical_line}};
    };
    if (expected.policy != actual.policy) {
        return mismatch("policy");
    }
    if (expected.symbol != actual.symbol) {
        return mismatch("symbol");
    }
    if (expected.producer != actual.producer) {
        return mismatch("producer");
    }
    if (expected.producer_version != actual.producer_version) {
        return mismatch("producer_version");
    }
    if (expected.source != actual.source) {
        return mismatch("source");
    }
    if (expected.generated_time_utc_ns != actual.generated_time_utc_ns) {
        return mismatch("generated_time_utc_ns");
    }
    if (expected.generated_monotonic_ns != actual.generated_monotonic_ns) {
        return mismatch("generated_monotonic_ns");
    }
    if (expected.last_update_id != actual.last_update_id) {
        return mismatch("last_update_id");
    }
    if (expected.synchronized != actual.synchronized) {
        return mismatch("synchronized");
    }
    if (expected.bids != actual.bids) {
        return mismatch("bids");
    }
    if (expected.asks != actual.asks) {
        return mismatch("asks");
    }
    if (expected.quality_flags != actual.quality_flags) {
        return mismatch("quality_flags");
    }
    if (expected.depth_limit != actual.depth_limit) {
        return mismatch("depth_limit");
    }
    if (expected.gap_descriptor.has_value() != actual.gap_descriptor.has_value()) {
        return mismatch("gap_descriptor");
    }
    if (expected.gap_descriptor.has_value() &&
        *expected.gap_descriptor != actual.gap_descriptor.value()) {
        return mismatch("gap_descriptor");
    }
    return std::nullopt;
}

template <>
[[nodiscard]] std::optional<Divergence> detail::compare_result_impl<RangeOutcome>(
    const RangeOutcome& expected, const RangeOutcome& actual,
    const OperationObservation& production, const OperationObservation& reference,
    std::string_view fixture_identity, const replay::SourceLocation& source) {
    if (expected.invalid_as_intended != actual.invalid_as_intended) {
        return Divergence{production.event_index,
                          production.event_kind,
                          Layer::R3,
                          DivergenceCategory::OperationResult,
                          "operation result field 'invalid_as_intended' differs",
                          to_canonical_text(production.result),
                          to_canonical_text(reference.result),
                          std::string(fixture_identity),
                          source.canonical_line};
    }
    return std::nullopt;
}

[[nodiscard]] Layer layer_for_result_kind(const OperationResultValue& production,
                                          const OperationResultValue& reference) noexcept {
    const auto holds_adapter = [](const OperationResultValue& value) {
        return std::holds_alternative<AdapterErrorOutcome>(value) ||
               std::holds_alternative<AdapterSuccessOutcome>(value) ||
               std::holds_alternative<SnapshotOutcome>(value);
    };
    const auto holds_decimal = [](const OperationResultValue& value) {
        return std::holds_alternative<DecimalErrorOutcome>(value);
    };
    const auto holds_composition = [](const OperationResultValue& value) {
        return std::holds_alternative<MetadataOutcome>(value) ||
               std::holds_alternative<SnapshotNotProducedOutcome>(value);
    };
    if (holds_adapter(production) || holds_adapter(reference)) {
        return Layer::R4;
    }
    if (holds_decimal(production) || holds_decimal(reference)) {
        return Layer::R1;
    }
    if (holds_composition(production) || holds_composition(reference)) {
        return Layer::D;
    }
    return Layer::R3;
}

std::optional<Divergence> compare_checkpoint(const OperationObservation& production,
                                             const OperationObservation& reference,
                                             std::string_view fixture_identity,
                                             const replay::SourceLocation& source) {
    const auto divergence = [&](Layer layer, std::string detail) {
        return std::optional<Divergence>{Divergence{
            production.event_index, production.event_kind, layer, DivergenceCategory::Checkpoint,
            std::move(detail), to_canonical_text(production.checkpoint),
            to_canonical_text(reference.checkpoint), std::string(fixture_identity),
            source.canonical_line}};
    };
    const auto& expected = production.checkpoint;
    const auto& actual = reference.checkpoint;
    if (expected.status != actual.status) {
        return divergence(Layer::R3, "checkpoint field 'status' differs");
    }
    if (expected.last_update_id != actual.last_update_id) {
        return divergence(Layer::R3, "checkpoint field 'last_update_id' differs");
    }
    if (expected.last_gap != actual.last_gap) {
        return divergence(Layer::R3, "checkpoint field 'last_gap' differs");
    }
    if (expected.synchronized_visible != actual.synchronized_visible) {
        return divergence(Layer::R3, "checkpoint field 'synchronized_visible' differs");
    }
    if (auto level_divergence =
            compare_levels(expected.bids, "bids", actual.bids, production.event_index,
                           production.event_kind, Layer::R2, expected, actual)) {
        return level_divergence;
    }
    if (auto level_divergence =
            compare_levels(expected.asks, "asks", actual.asks, production.event_index,
                           production.event_kind, Layer::R2, expected, actual)) {
        return level_divergence;
    }
    if (expected.price_scale != actual.price_scale) {
        return divergence(Layer::R3, "checkpoint field 'price_scale' differs");
    }
    if (expected.quantity_scale != actual.quantity_scale) {
        return divergence(Layer::R3, "checkpoint field 'quantity_scale' differs");
    }
    return std::nullopt;
}

std::optional<Divergence> compare_snapshot(const OperationObservation& production,
                                           const OperationObservation& reference,
                                           std::string_view fixture_identity,
                                           const replay::SourceLocation& source) {
    const auto divergence = [&](Layer layer, std::string detail) {
        const auto production_text = production.snapshot.has_value()
                                         ? to_canonical_text(*production.snapshot)
                                         : std::string("-");
        const auto reference_text = reference.snapshot.has_value()
                                        ? to_canonical_text(*reference.snapshot)
                                        : std::string("-");
        return std::optional<Divergence>{
            Divergence{production.event_index, production.event_kind, layer,
                       DivergenceCategory::SnapshotObservation, std::move(detail), production_text,
                       reference_text, std::string(fixture_identity), source.canonical_line}};
    };
    if (production.snapshot.has_value() != reference.snapshot.has_value()) {
        return divergence(Layer::R4, "snapshot observation presence differs");
    }
    if (!production.snapshot.has_value()) {
        return std::nullopt;
    }
    if (*production.snapshot != reference.snapshot.value()) {
        return divergence(Layer::R4, "snapshot semantic observation differs");
    }
    return std::nullopt;
}

} // namespace

std::string_view to_text(Layer layer) noexcept {
    switch (layer) {
    case Layer::R1:
        return "R1";
    case Layer::R2:
        return "R2";
    case Layer::R3:
        return "R3";
    case Layer::R4:
        return "R4";
    case Layer::D:
        return "D";
    }
    return "UNKNOWN";
}

std::string_view to_text(DivergenceCategory category) noexcept {
    switch (category) {
    case DivergenceCategory::OperationResult:
        return "OPERATION_RESULT";
    case DivergenceCategory::Checkpoint:
        return "CHECKPOINT";
    case DivergenceCategory::SnapshotObservation:
        return "SNAPSHOT_OBSERVATION";
    case DivergenceCategory::Composition:
        return "COMPOSITION";
    }
    return "UNKNOWN";
}

std::string_view to_text(CanonicalDisposition disposition) noexcept {
    switch (disposition) {
    case CanonicalDisposition::Installed:
        return "INSTALLED";
    case CanonicalDisposition::Applied:
        return "APPLIED";
    case CanonicalDisposition::IgnoredStale:
        return "IGNORED_STALE";
    case CanonicalDisposition::IgnoredDuplicate:
        return "IGNORED_DUPLICATE";
    case CanonicalDisposition::GapDetected:
        return "GAP_DETECTED";
    case CanonicalDisposition::RejectedWrongState:
        return "REJECTED_WRONG_STATE";
    }
    return "UNKNOWN";
}

std::string_view to_text(CanonicalStatus status) noexcept {
    switch (status) {
    case CanonicalStatus::AwaitingBaseline:
        return "AWAITING_BASELINE";
    case CanonicalStatus::AwaitingBridge:
        return "AWAITING_BRIDGE";
    case CanonicalStatus::Synchronized:
        return "SYNCHRONIZED";
    case CanonicalStatus::NeedsResync:
        return "NEEDS_RESYNC";
    }
    return "UNKNOWN";
}

std::string_view to_text(CanonicalGapReason reason) noexcept {
    switch (reason) {
    case CanonicalGapReason::SpotBootstrapForwardGap:
        return "SPOT_BOOTSTRAP_FORWARD_GAP";
    case CanonicalGapReason::SpotLiveForwardGap:
        return "SPOT_LIVE_FORWARD_GAP";
    case CanonicalGapReason::FuturesBootstrapRangeMiss:
        return "FUTURES_BOOTSTRAP_RANGE_MISS";
    case CanonicalGapReason::FuturesMissingPreviousFinal:
        return "FUTURES_MISSING_PREVIOUS_FINAL";
    case CanonicalGapReason::FuturesPreviousFinalMismatch:
        return "FUTURES_PREVIOUS_FINAL_MISMATCH";
    }
    return "UNKNOWN";
}

std::string_view to_text(CanonicalPolicy policy) noexcept {
    switch (policy) {
    case CanonicalPolicy::Spot:
        return "SPOT";
    case CanonicalPolicy::UsdMPerpetual:
        return "USD_M_PERPETUAL";
    }
    return "UNKNOWN";
}

std::string_view to_text(CanonicalDecimalError error) noexcept {
    switch (error) {
    case CanonicalDecimalError::Empty:
        return "EMPTY";
    case CanonicalDecimalError::InvalidSyntax:
        return "INVALID_SYNTAX";
    case CanonicalDecimalError::SignNotAllowed:
        return "SIGN_NOT_ALLOWED";
    case CanonicalDecimalError::LeadingZero:
        return "LEADING_ZERO";
    case CanonicalDecimalError::MissingFractionDigits:
        return "MISSING_FRACTION_DIGITS";
    case CanonicalDecimalError::ZeroNotAllowed:
        return "ZERO_NOT_ALLOWED";
    case CanonicalDecimalError::InexactScale:
        return "INEXACT_SCALE";
    case CanonicalDecimalError::Overflow:
        return "OVERFLOW";
    }
    return "UNKNOWN";
}

std::string_view to_text(CanonicalAdapterCode code) noexcept {
    switch (code) {
    case CanonicalAdapterCode::UnsupportedVenue:
        return "UNSUPPORTED_VENUE";
    case CanonicalAdapterCode::UnsupportedMarket:
        return "UNSUPPORTED_MARKET";
    case CanonicalAdapterCode::UnexpectedStream:
        return "UNEXPECTED_STREAM";
    case CanonicalAdapterCode::IdentityMismatch:
        return "IDENTITY_MISMATCH";
    case CanonicalAdapterCode::UnsupportedSchemaVersion:
        return "UNSUPPORTED_SCHEMA_VERSION";
    case CanonicalAdapterCode::UnspecifiedEnum:
        return "UNSPECIFIED_ENUM";
    case CanonicalAdapterCode::UnknownEnumValue:
        return "UNKNOWN_ENUM_VALUE";
    case CanonicalAdapterCode::InvalidUpdateRange:
        return "INVALID_UPDATE_RANGE";
    case CanonicalAdapterCode::MissingRequiredField:
        return "MISSING_REQUIRED_FIELD";
    case CanonicalAdapterCode::InvalidIdentifier:
        return "INVALID_IDENTIFIER";
    case CanonicalAdapterCode::InvalidDecimal:
        return "INVALID_DECIMAL";
    case CanonicalAdapterCode::NegativeQuantity:
        return "NEGATIVE_QUANTITY";
    case CanonicalAdapterCode::NonPositivePrice:
        return "NON_POSITIVE_PRICE";
    case CanonicalAdapterCode::ScaleMismatch:
        return "SCALE_MISMATCH";
    case CanonicalAdapterCode::NumericOverflow:
        return "NUMERIC_OVERFLOW";
    case CanonicalAdapterCode::InvalidDepthLimit:
        return "INVALID_DEPTH_LIMIT";
    case CanonicalAdapterCode::InvalidOrdering:
        return "INVALID_ORDERING";
    case CanonicalAdapterCode::UnsupportedProjectionState:
        return "UNSUPPORTED_PROJECTION_STATE";
    case CanonicalAdapterCode::MissingLastUpdateId:
        return "MISSING_LAST_UPDATE_ID";
    case CanonicalAdapterCode::InvalidGapContext:
        return "INVALID_GAP_CONTEXT";
    case CanonicalAdapterCode::InvalidHostQualityCombination:
        return "INVALID_HOST_QUALITY_COMBINATION";
    case CanonicalAdapterCode::ContractsVersionMismatch:
        return "CONTRACTS_VERSION_MISMATCH";
    case CanonicalAdapterCode::ProjectionNumericSpecMismatch:
        return "PROJECTION_NUMERIC_SPEC_MISMATCH";
    case CanonicalAdapterCode::ProjectionPolicyMismatch:
        return "PROJECTION_POLICY_MISMATCH";
    }
    return "UNKNOWN";
}

// Linear enum-name mapping table; each case is a distinct stable name with no
// control flow. Complexity is inherent to the closed 25-value domain (repository
// precedent: reference_projection.hpp explicit decision tables).
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
std::string_view to_text(CanonicalAdapterField field) noexcept {
    switch (field) {
    case CanonicalAdapterField::None:
        return "NONE";
    case CanonicalAdapterField::Venue:
        return "VENUE";
    case CanonicalAdapterField::Market:
        return "MARKET";
    case CanonicalAdapterField::Stream:
        return "STREAM";
    case CanonicalAdapterField::Symbol:
        return "SYMBOL";
    case CanonicalAdapterField::SchemaVersion:
        return "SCHEMA_VERSION";
    case CanonicalAdapterField::Producer:
        return "PRODUCER";
    case CanonicalAdapterField::ProducerVersion:
        return "PRODUCER_VERSION";
    case CanonicalAdapterField::RequestId:
        return "REQUEST_ID";
    case CanonicalAdapterField::ConnectionId:
        return "CONNECTION_ID";
    case CanonicalAdapterField::FirstUpdateId:
        return "FIRST_UPDATE_ID";
    case CanonicalAdapterField::FinalUpdateId:
        return "FINAL_UPDATE_ID";
    case CanonicalAdapterField::PreviousFinalUpdateId:
        return "PREVIOUS_FINAL_UPDATE_ID";
    case CanonicalAdapterField::BidPrice:
        return "BID_PRICE";
    case CanonicalAdapterField::BidQuantity:
        return "BID_QUANTITY";
    case CanonicalAdapterField::AskPrice:
        return "ASK_PRICE";
    case CanonicalAdapterField::AskQuantity:
        return "ASK_QUANTITY";
    case CanonicalAdapterField::QualityFlag:
        return "QUALITY_FLAG";
    case CanonicalAdapterField::ProjectionPriceScale:
        return "PROJECTION_PRICE_SCALE";
    case CanonicalAdapterField::ProjectionQuantityScale:
        return "PROJECTION_QUANTITY_SCALE";
    case CanonicalAdapterField::ProjectionPolicy:
        return "PROJECTION_POLICY";
    case CanonicalAdapterField::DepthLimit:
        return "DEPTH_LIMIT";
    case CanonicalAdapterField::SnapshotSource:
        return "SNAPSHOT_SOURCE";
    case CanonicalAdapterField::LastUpdateId:
        return "LAST_UPDATE_ID";
    case CanonicalAdapterField::CurrentGap:
        return "CURRENT_GAP";
    case CanonicalAdapterField::GapRecoveryState:
        return "GAP_RECOVERY_STATE";
    case CanonicalAdapterField::HostQualityFact:
        return "HOST_QUALITY_FACT";
    }
    return "UNKNOWN";
}

std::string_view to_text(CanonicalQualityFlag flag) noexcept {
    switch (flag) {
    case CanonicalQualityFlag::Duplicate:
        return "DUPLICATE";
    case CanonicalQualityFlag::OutOfOrder:
        return "OUT_OF_ORDER";
    case CanonicalQualityFlag::SequenceGap:
        return "SEQUENCE_GAP";
    case CanonicalQualityFlag::OrderBookResync:
        return "ORDER_BOOK_RESYNC";
    case CanonicalQualityFlag::SnapshotBridgePending:
        return "SNAPSHOT_BRIDGE_PENDING";
    case CanonicalQualityFlag::SnapshotTooOld:
        return "SNAPSHOT_TOO_OLD";
    case CanonicalQualityFlag::BootstrapBufferOverflow:
        return "BOOTSTRAP_BUFFER_OVERFLOW";
    case CanonicalQualityFlag::RecoveredTail:
        return "RECOVERED_TAIL";
    case CanonicalQualityFlag::MalformedPayload:
        return "MALFORMED_PAYLOAD";
    case CanonicalQualityFlag::ExchangeTimeMissing:
        return "EXCHANGE_TIME_MISSING";
    case CanonicalQualityFlag::ReceiveClockDiscontinuity:
        return "RECEIVE_CLOCK_DISCONTINUITY";
    case CanonicalQualityFlag::SlowConsumerGap:
        return "SLOW_CONSUMER_GAP";
    case CanonicalQualityFlag::ProducerRestart:
        return "PRODUCER_RESTART";
    case CanonicalQualityFlag::Overlap:
        return "OVERLAP";
    case CanonicalQualityFlag::IdentityConflict:
        return "IDENTITY_CONFLICT";
    case CanonicalQualityFlag::CrossedBook:
        return "CROSSED_BOOK";
    }
    return "UNKNOWN";
}

std::string_view to_text(CanonicalSnapshotSource source) noexcept {
    switch (source) {
    case CanonicalSnapshotSource::GatewayLive:
        return "GATEWAY_LIVE";
    case CanonicalSnapshotSource::RecorderReplay:
        return "RECORDER_REPLAY";
    case CanonicalSnapshotSource::HistoryReplay:
        return "HISTORY_REPLAY";
    }
    return "UNKNOWN";
}

std::string_view to_text(CanonicalResyncState state) noexcept {
    switch (state) {
    case CanonicalResyncState::ResyncRequired:
        return "RESYNC_REQUIRED";
    case CanonicalResyncState::ResyncInProgress:
        return "RESYNC_IN_PROGRESS";
    case CanonicalResyncState::ResyncFailed:
        return "RESYNC_FAILED";
    }
    return "UNKNOWN";
}

std::string_view to_text(CanonicalReasonCode reason) noexcept {
    switch (reason) {
    case CanonicalReasonCode::SequenceGapDetected:
        return "SEQUENCE_GAP_DETECTED";
    }
    return "UNKNOWN";
}

std::string_view to_text(replay::EventKind kind) noexcept {
    switch (kind) {
    case replay::EventKind::InstallBaseline:
        return "INSTALL_BASELINE";
    case replay::EventKind::DepthUpdate:
        return "DEPTH_UPDATE";
    case replay::EventKind::Rebaseline:
        return "REBASELINE";
    case replay::EventKind::Reset:
        return "RESET";
    case replay::EventKind::SnapshotRequest:
        return "SNAPSHOT_REQUEST";
    case replay::EventKind::AdapterMetadata:
        return "ADAPTER_METADATA";
    case replay::EventKind::MalformedRange:
        return "MALFORMED_RANGE";
    }
    return "UNKNOWN";
}

std::string install_outcome_text(const InstallOutcome& outcome) {
    std::string text = "INSTALL_OUTCOME";
    text += " disposition=" + std::string(to_text(outcome.disposition));
    text += " status_after=" + std::string(to_text(outcome.status_after));
    text += " last_update_id_after=" + (outcome.last_update_id_after.has_value()
                                            ? std::to_string(*outcome.last_update_id_after)
                                            : "-");
    return text;
}

std::string apply_outcome_text(const ApplyOutcome& outcome) {
    std::string text = "APPLY_OUTCOME";
    text += " disposition=" + std::string(to_text(outcome.disposition));
    text += " status_after=" + std::string(to_text(outcome.status_after));
    text += " last_update_id_after=" + (outcome.last_update_id_after.has_value()
                                            ? std::to_string(*outcome.last_update_id_after)
                                            : "-");
    text += " gap=";
    if (!outcome.gap.has_value()) {
        text += "-";
    } else {
        const auto& gap = *outcome.gap;
        text += "{last_accepted_final=" + std::to_string(gap.last_accepted_final) +
                " first=" + std::to_string(gap.first_update_id) +
                " final=" + std::to_string(gap.final_update_id) + " previous=" +
                (gap.previous_final.has_value() ? std::to_string(*gap.previous_final) : "-") +
                " reason=" + std::string(to_text(gap.reason)) +
                " policy=" + std::string(to_text(gap.policy)) + "}";
    }
    return text;
}

std::string to_canonical_text(const OperationResult& result) {
    const auto& value = result.value;
    if (const auto* outcome = std::get_if<DecimalErrorOutcome>(&value)) {
        return std::string("DECIMAL_ERROR category=") + std::string(to_text(outcome->category));
    }
    if (const auto* outcome = std::get_if<InstallOutcome>(&value)) {
        return install_outcome_text(*outcome);
    }
    if (const auto* outcome = std::get_if<ApplyOutcome>(&value)) {
        return apply_outcome_text(*outcome);
    }
    if (const auto* outcome = std::get_if<AdapterErrorOutcome>(&value)) {
        std::string text = "ADAPTER_ERROR";
        text += " code=" + std::string(to_text(outcome->code));
        text += " field=" + std::string(to_text(outcome->field));
        text += " decimal_error=" + (outcome->decimal_error.has_value()
                                         ? std::string(to_text(*outcome->decimal_error))
                                         : "-");
        return text;
    }
    if (const auto* outcome = std::get_if<AdapterSuccessOutcome>(&value)) {
        std::string text = "ADAPTER_SUCCESS";
        text += " core_result=";
        if (const auto* install = std::get_if<InstallOutcome>(&outcome->core_result)) {
            text += install_outcome_text(*install);
        } else {
            text += apply_outcome_text(std::get<ApplyOutcome>(outcome->core_result));
        }
        text += " observed_quality=[";
        for (std::size_t index = 0; index < outcome->observed_quality.size(); ++index) {
            if (index > 0) {
                text += ";";
            }
            text += to_text(outcome->observed_quality[index]);
        }
        text += "]";
        return text;
    }
    if (const auto* outcome = std::get_if<SnapshotOutcome>(&value)) {
        return to_canonical_text(*outcome);
    }
    if (std::holds_alternative<SnapshotNotProducedOutcome>(value)) {
        return "SNAPSHOT_NOT_PRODUCED";
    }
    if (std::holds_alternative<ResetOutcome>(value)) {
        return "RESET_OUTCOME success";
    }
    if (const auto* outcome = std::get_if<RangeOutcome>(&value)) {
        return std::string("RANGE_OUTCOME invalid_as_intended=") +
               (outcome->invalid_as_intended ? "true" : "false");
    }
    if (std::holds_alternative<MetadataOutcome>(value)) {
        return "METADATA_OUTCOME context-only";
    }
    return "UNKNOWN_RESULT";
}

std::string to_canonical_text(const SemanticCheckpoint& checkpoint) {
    std::string text = "CHECKPOINT";
    text += " status=" + std::string(to_text(checkpoint.status));
    text +=
        " last_update_id=" +
        (checkpoint.last_update_id.has_value() ? std::to_string(*checkpoint.last_update_id) : "-");
    text += " last_gap=";
    if (!checkpoint.last_gap.has_value()) {
        text += "-";
    } else {
        const auto& gap = *checkpoint.last_gap;
        text += "{last_accepted_final=" + std::to_string(gap.last_accepted_final) +
                " first=" + std::to_string(gap.first_update_id) +
                " final=" + std::to_string(gap.final_update_id) + " previous=" +
                (gap.previous_final.has_value() ? std::to_string(*gap.previous_final) : "-") +
                " reason=" + std::string(to_text(gap.reason)) +
                " policy=" + std::string(to_text(gap.policy)) + "}";
    }
    text +=
        " synchronized_visible=" + std::string(checkpoint.synchronized_visible ? "true" : "false");
    text += " bids=[";
    for (std::size_t index = 0; index < checkpoint.bids.size(); ++index) {
        if (index > 0) {
            text += ";";
        }
        text += "(" + std::to_string(checkpoint.bids[index].price) + "," +
                std::to_string(checkpoint.bids[index].quantity) + ")";
    }
    text += "] asks=[";
    for (std::size_t index = 0; index < checkpoint.asks.size(); ++index) {
        if (index > 0) {
            text += ";";
        }
        text += "(" + std::to_string(checkpoint.asks[index].price) + "," +
                std::to_string(checkpoint.asks[index].quantity) + ")";
    }
    text += "]";
    text += " spec=(" + std::to_string(checkpoint.price_scale) + "," +
            std::to_string(checkpoint.quantity_scale) + ")";
    return text;
}

std::string to_canonical_text(const SnapshotOutcome& snapshot) {
    std::string text = "SNAPSHOT_OUTCOME";
    text += " policy=" + std::string(to_text(snapshot.policy));
    text += " symbol=" + snapshot.symbol;
    text += " producer=" + snapshot.producer;
    text += " producer_version=" + snapshot.producer_version;
    text += " source=" + std::string(to_text(snapshot.source));
    text += " generated_time_utc_ns=" + std::to_string(snapshot.generated_time_utc_ns);
    text += " generated_monotonic_ns=" + (snapshot.generated_monotonic_ns.has_value()
                                              ? std::to_string(*snapshot.generated_monotonic_ns)
                                              : "-");
    text += " last_update_id=" +
            (snapshot.last_update_id.has_value() ? std::to_string(*snapshot.last_update_id) : "-");
    text += " synchronized=" + std::string(snapshot.synchronized ? "true" : "false");
    text += " bids=[";
    for (std::size_t index = 0; index < snapshot.bids.size(); ++index) {
        if (index > 0) {
            text += ";";
        }
        text += "(" + snapshot.bids[index].price + "," + snapshot.bids[index].quantity + ")";
    }
    text += "] asks=[";
    for (std::size_t index = 0; index < snapshot.asks.size(); ++index) {
        if (index > 0) {
            text += ";";
        }
        text += "(" + snapshot.asks[index].price + "," + snapshot.asks[index].quantity + ")";
    }
    text += "]";
    text += " quality_flags=[";
    for (std::size_t index = 0; index < snapshot.quality_flags.size(); ++index) {
        if (index > 0) {
            text += ";";
        }
        text += to_text(snapshot.quality_flags[index]);
    }
    text += "]";
    text += " depth_limit=";
    if (snapshot.depth_limit.has_value()) {
        text += std::to_string(*snapshot.depth_limit);
    } else {
        text += "-";
    }
    text += " gap_descriptor=";
    if (snapshot.gap_descriptor.has_value()) {
        const auto& gap = *snapshot.gap_descriptor;
        text += "{detected_at_utc_ns=" + std::to_string(gap.detected_at_utc_ns) +
                " previous_sequence=" + std::to_string(gap.previous_sequence) +
                " next_sequence=" + std::to_string(gap.next_sequence) +
                " reason_code=" + std::string(to_text(gap.reason_code)) +
                " recovery_state=" + std::string(to_text(gap.recovery_state)) + "}";
    } else {
        text += "-";
    }
    return text;
}

std::optional<Divergence> compare_observations(const OperationObservation& production,
                                               const OperationObservation& reference,
                                               std::string_view fixture_identity,
                                               const replay::SourceLocation& source) {
    if (production.event_kind != reference.event_kind) {
        return Divergence{production.event_index,
                          production.event_kind,
                          Layer::D,
                          DivergenceCategory::Composition,
                          "event kind differs",
                          to_canonical_text(production.result),
                          to_canonical_text(reference.result),
                          std::string(fixture_identity),
                          source.canonical_line};
    }

    if (production.result.value.index() != reference.result.value.index()) {
        return Divergence{production.event_index,
                          production.event_kind,
                          layer_for_result_kind(production.result.value, reference.result.value),
                          DivergenceCategory::OperationResult,
                          "operation result kind differs",
                          to_canonical_text(production.result),
                          to_canonical_text(reference.result),
                          std::string(fixture_identity),
                          source.canonical_line};
    }

    if (const auto nested = std::visit(
            [&](const auto& expected, const auto& actual) -> std::optional<Divergence> {
                using Expected = std::decay_t<decltype(expected)>;
                using Actual = std::decay_t<decltype(actual)>;
                if constexpr (std::is_same_v<Expected, Actual>) {
                    return detail::compare_result_impl<Expected>(
                        expected, actual, production, reference, fixture_identity, source);
                }
                return std::nullopt;
            },
            production.result.value, reference.result.value)) {
        return nested;
    }

    if (const auto nested = compare_checkpoint(production, reference, fixture_identity, source)) {
        return nested;
    }

    return compare_snapshot(production, reference, fixture_identity, source);
}

} // namespace bmd_projection::m5::oracle
