#pragma once

// Production Core (M1/M3) to canonical observation conversions. These mechanical
// mappings are the comparison-boundary translation of production enum values into
// canonical observation names; they contain no business logic.

#include "operation_observation.hpp"

#include <binance_market_data/projection/v1/numeric/decimal_error.hpp>
#include <binance_market_data/projection/v1/numeric/decimal_parse.hpp>
#include <binance_market_data/projection/v1/projection_state/book_projection.hpp>

#include <cstdint>
#include <optional>

namespace bmd_projection::m5::oracle {

namespace core = binance_market_data::projection::v1;

[[nodiscard]] inline CanonicalStatus to_canonical(core::ProjectionStatus status) noexcept {
    switch (status) {
    case core::ProjectionStatus::AwaitingBaseline:
        return CanonicalStatus::AwaitingBaseline;
    case core::ProjectionStatus::AwaitingBridge:
        return CanonicalStatus::AwaitingBridge;
    case core::ProjectionStatus::Synchronized:
        return CanonicalStatus::Synchronized;
    case core::ProjectionStatus::NeedsResync:
        return CanonicalStatus::NeedsResync;
    }
    return CanonicalStatus::AwaitingBaseline;
}

[[nodiscard]] inline CanonicalDisposition
to_canonical(core::InstallDisposition disposition) noexcept {
    switch (disposition) {
    case core::InstallDisposition::Installed:
        return CanonicalDisposition::Installed;
    case core::InstallDisposition::RejectedWrongState:
        return CanonicalDisposition::RejectedWrongState;
    }
    return CanonicalDisposition::RejectedWrongState;
}

[[nodiscard]] inline CanonicalDisposition
to_canonical(core::ApplyDisposition disposition) noexcept {
    switch (disposition) {
    case core::ApplyDisposition::Applied:
        return CanonicalDisposition::Applied;
    case core::ApplyDisposition::IgnoredStale:
        return CanonicalDisposition::IgnoredStale;
    case core::ApplyDisposition::IgnoredDuplicate:
        return CanonicalDisposition::IgnoredDuplicate;
    case core::ApplyDisposition::GapDetected:
        return CanonicalDisposition::GapDetected;
    case core::ApplyDisposition::RejectedWrongState:
        return CanonicalDisposition::RejectedWrongState;
    }
    return CanonicalDisposition::RejectedWrongState;
}

[[nodiscard]] inline CanonicalGapReason to_canonical(core::GapReason reason) noexcept {
    switch (reason) {
    case core::GapReason::SpotBootstrapForwardGap:
        return CanonicalGapReason::SpotBootstrapForwardGap;
    case core::GapReason::SpotLiveForwardGap:
        return CanonicalGapReason::SpotLiveForwardGap;
    case core::GapReason::FuturesBootstrapRangeMiss:
        return CanonicalGapReason::FuturesBootstrapRangeMiss;
    case core::GapReason::FuturesMissingPreviousFinal:
        return CanonicalGapReason::FuturesMissingPreviousFinal;
    case core::GapReason::FuturesPreviousFinalMismatch:
        return CanonicalGapReason::FuturesPreviousFinalMismatch;
    }
    return CanonicalGapReason::SpotBootstrapForwardGap;
}

[[nodiscard]] inline CanonicalPolicy to_canonical(core::SequencePolicyKind policy) noexcept {
    switch (policy) {
    case core::SequencePolicyKind::Spot:
        return CanonicalPolicy::Spot;
    case core::SequencePolicyKind::UsdMPerpetual:
        return CanonicalPolicy::UsdMPerpetual;
    }
    return CanonicalPolicy::Spot;
}

[[nodiscard]] inline CanonicalDecimalError to_canonical(core::DecimalErrorCode code) noexcept {
    switch (code) {
    case core::DecimalErrorCode::Empty:
        return CanonicalDecimalError::Empty;
    case core::DecimalErrorCode::InvalidSyntax:
        return CanonicalDecimalError::InvalidSyntax;
    case core::DecimalErrorCode::SignNotAllowed:
        return CanonicalDecimalError::SignNotAllowed;
    case core::DecimalErrorCode::LeadingZero:
        return CanonicalDecimalError::LeadingZero;
    case core::DecimalErrorCode::MissingFractionDigits:
        return CanonicalDecimalError::MissingFractionDigits;
    case core::DecimalErrorCode::ZeroNotAllowed:
        return CanonicalDecimalError::ZeroNotAllowed;
    case core::DecimalErrorCode::InexactScale:
        return CanonicalDecimalError::InexactScale;
    case core::DecimalErrorCode::Overflow:
        return CanonicalDecimalError::Overflow;
    }
    return CanonicalDecimalError::InvalidSyntax;
}

[[nodiscard]] inline CanonicalGapEvidence to_canonical(const core::GapInfo& gap) noexcept {
    return {gap.last_accepted_final.value(),
            gap.incoming_range.first().value(),
            gap.incoming_range.final().value(),
            gap.incoming_previous_final.has_value()
                ? std::optional<std::uint64_t>{gap.incoming_previous_final->value()}
                : std::nullopt,
            to_canonical(gap.reason),
            to_canonical(gap.policy)};
}

[[nodiscard]] inline InstallOutcome to_canonical(const core::InstallResult& result) noexcept {
    return {to_canonical(result.disposition), to_canonical(result.status_after),
            result.last_update_id_after.has_value()
                ? std::optional<std::uint64_t>{result.last_update_id_after->value()}
                : std::nullopt};
}

[[nodiscard]] inline ApplyOutcome to_canonical(const core::ApplyResult& result) noexcept {
    return {to_canonical(result.disposition), to_canonical(result.status_after),
            result.last_update_id_after.has_value()
                ? std::optional<std::uint64_t>{result.last_update_id_after->value()}
                : std::nullopt,
            result.gap.has_value() ? std::optional<CanonicalGapEvidence>{to_canonical(*result.gap)}
                                   : std::nullopt};
}

} // namespace bmd_projection::m5::oracle
