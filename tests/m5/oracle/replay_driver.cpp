#include "replay_driver.hpp"

#include "divergence.hpp"

#include "replay_types.hpp"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace bmd_projection::m5::oracle {
namespace {

[[nodiscard]] replay::EventKind event_kind_of(const replay::Operation& operation) noexcept {
    if (std::holds_alternative<replay::InstallBaselineOp>(operation)) {
        return replay::EventKind::InstallBaseline;
    }
    if (std::holds_alternative<replay::DepthUpdateOp>(operation)) {
        return replay::EventKind::DepthUpdate;
    }
    if (std::holds_alternative<replay::RebaselineOp>(operation)) {
        return replay::EventKind::Rebaseline;
    }
    if (std::holds_alternative<replay::ResetOp>(operation)) {
        return replay::EventKind::Reset;
    }
    if (std::holds_alternative<replay::SnapshotRequestOp>(operation)) {
        return replay::EventKind::SnapshotRequest;
    }
    if (std::holds_alternative<replay::AdapterMetadataOp>(operation)) {
        return replay::EventKind::AdapterMetadata;
    }
    return replay::EventKind::MalformedRange;
}

[[nodiscard]] replay::SourceLocation source_of(const replay::Operation& operation) {
    return std::visit([](const auto& op) { return replay::SourceLocation{op.source}; }, operation);
}

[[nodiscard]] std::string fixture_identity_text(const replay::FixtureIdentity& identity) {
    return "fixture_id=" + identity.fixture_id + " log_sha256=" + identity.replay_log_sha256;
}

void accumulate_install(ExecutionSummary& summary, std::size_t event_index,
                        const InstallOutcome& install) noexcept {
    ++summary.install_events;
    if (!summary.first_install.has_value()) {
        summary.first_install =
            CompactInstallResult{event_index, install.disposition, install.status_after};
    }
    if (install.disposition == CanonicalDisposition::Installed) {
        ++summary.installed_count;
    } else if (install.disposition == CanonicalDisposition::RejectedWrongState) {
        ++summary.rejected_wrong_state_count;
    }
}

void accumulate_apply(ExecutionSummary& summary, std::size_t event_index,
                      const ApplyOutcome& apply) noexcept {
    ++summary.depth_events;
    if (!summary.first_depth_update.has_value()) {
        summary.first_depth_update =
            CompactDepthResult{event_index, apply.disposition, apply.status_after};
    }
    summary.depth_results.push_back(
        CompactDepthResult{event_index, apply.disposition, apply.status_after});
    switch (apply.disposition) {
    case CanonicalDisposition::Applied:
        ++summary.applied_count;
        break;
    case CanonicalDisposition::IgnoredStale:
        ++summary.ignored_stale_count;
        break;
    case CanonicalDisposition::IgnoredDuplicate:
        ++summary.ignored_duplicate_count;
        break;
    case CanonicalDisposition::GapDetected:
        ++summary.gap_detected_count;
        break;
    case CanonicalDisposition::RejectedWrongState:
        ++summary.rejected_wrong_state_count;
        break;
    case CanonicalDisposition::Installed:
        break;
    }
}

// Aggregation observes typed results only. AdapterSuccessOutcome is unwrapped to
// its underlying typed Core result; the quality vector is not aggregated.
void accumulate_summary(ExecutionSummary& summary, std::size_t event_index,
                        const OperationResult& result) noexcept {
    if (const auto* install = std::get_if<InstallOutcome>(&result.value)) {
        accumulate_install(summary, event_index, *install);
        return;
    }
    if (const auto* apply = std::get_if<ApplyOutcome>(&result.value)) {
        accumulate_apply(summary, event_index, *apply);
        return;
    }
    if (const auto* success = std::get_if<AdapterSuccessOutcome>(&result.value)) {
        if (const auto* inner_install = std::get_if<InstallOutcome>(&success->core_result)) {
            accumulate_install(summary, event_index, *inner_install);
        } else {
            accumulate_apply(summary, event_index, std::get<ApplyOutcome>(success->core_result));
        }
        return;
    }
    if (std::holds_alternative<AdapterErrorOutcome>(result.value)) {
        ++summary.adapter_error_count;
        if (!summary.first_adapter_error_index.has_value()) {
            summary.first_adapter_error_index = event_index;
        }
        return;
    }
    if (std::holds_alternative<DecimalErrorOutcome>(result.value)) {
        ++summary.decimal_error_count;
        return;
    }
    ++summary.other_events_count;
    if (!summary.first_other_event_index.has_value()) {
        summary.first_other_event_index = event_index;
    }
}

} // namespace

ReplayDriver::ReplayDriver(const replay::ReplayFixture& fixture,
                           std::unique_ptr<ReplaySide> production,
                           std::unique_ptr<ReplaySide> reference, ObservationRetention retention)
    : fixture_{&fixture}, production_{std::move(production)}, reference_{std::move(reference)},
      retention_{retention} {}

ReplayOutcome ReplayDriver::run() {
    ReplayOutcome outcome;
    const auto identity = fixture_identity_text(fixture_->identity);
    const auto& operations = fixture_->replay.operations;
    for (std::size_t index = 0; index < operations.size(); ++index) {
        const auto& operation = operations[index];
        auto production = production_->observe(operation);
        auto reference = reference_->observe(operation);
        const auto kind = event_kind_of(operation);
        const auto source = source_of(operation);
        outcome.processed_events = index + 1;
        if (!production.has_value() || !reference.has_value()) {
            Divergence divergence{
                index,
                kind,
                Layer::D,
                DivergenceCategory::Composition,
                "pipeline side failed to produce an observation",
                production.has_value() ? to_canonical_text(production->result) : "-",
                reference.has_value() ? to_canonical_text(reference->result) : "-",
                identity,
                source.canonical_line};
            enrich_divergence(divergence, *fixture_, source, production ? &*production : nullptr,
                              reference ? &*reference : nullptr);
            outcome.first_divergence = std::move(divergence);
            return outcome;
        }
        production->event_index = index;
        production->event_kind = kind;
        reference->event_index = index;
        reference->event_kind = kind;
        if (const auto divergence =
                compare_observations(*production, *reference, identity, source)) {
            auto enriched = *divergence;
            enrich_divergence(enriched, *fixture_, source, &*production, &*reference);
            outcome.first_divergence = std::move(enriched);
            return outcome;
        }
        outcome.final_observation = *production;
        accumulate_summary(outcome.summary, index, outcome.final_observation->result);
        if (retention_ == ObservationRetention::RetainAll) {
            outcome.observations.push_back(std::move(*production));
        }
    }
    return outcome;
}

MutatingSide::MutatingSide(std::unique_ptr<ReplaySide> wrapped,
                           std::function<void(OperationObservation&)> mutator) noexcept
    : wrapped_{std::move(wrapped)}, mutator_{std::move(mutator)} {}

std::optional<OperationObservation> MutatingSide::observe(const replay::Operation& operation) {
    auto observation = wrapped_->observe(operation);
    if (observation.has_value() && mutator_ != nullptr) {
        mutator_(*observation);
    }
    return observation;
}

FailingSide::FailingSide(std::unique_ptr<ReplaySide> wrapped, std::size_t fail_at_event)
    : wrapped_{std::move(wrapped)}, fail_at_event_{fail_at_event} {}

std::optional<OperationObservation> FailingSide::observe(const replay::Operation& operation) {
    if (observed_events_ == fail_at_event_) {
        ++observed_events_;
        return std::nullopt;
    }
    ++observed_events_;
    return wrapped_->observe(operation);
}

} // namespace bmd_projection::m5::oracle
