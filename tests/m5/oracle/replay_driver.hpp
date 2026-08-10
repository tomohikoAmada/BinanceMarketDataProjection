#pragma once

// ReplayDriver: neutral differential orchestration.
//
// The driver loads an already-parsed ReplayFixture, dispatches each normalized
// operation to the production side and the reference side, stamps event identity,
// compares the resulting observations in the fixed first-divergence order, and stops
// at the first mismatch. It contains no Spot/USD-M classification logic, no decimal
// business rules, no book state, and no snapshot eligibility or quality mapping.
// It must not become a fifth business oracle.

#include "divergence.hpp"
#include "operation_observation.hpp"
#include "replay_side.hpp"

#include "replay_types.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace bmd_projection::m5::oracle {

enum class ReplayMode : std::uint8_t {
    CoreOnly,
    AdapterEnabled,
};

enum class ObservationRetention : std::uint8_t {
    RetainAll,
    RetainNone,
};

// Compact typed-result records used by ExecutionSummary. The driver fills these
// AFTER production/reference equality for an event has been established; they
// observe typed dispositions and statuses only and carry no decimal evidence,
// book state, or adapter-mapped values.
struct CompactInstallResult final {
    std::size_t event_index{};
    CanonicalDisposition disposition{};
    CanonicalStatus status_after{};

    friend bool operator==(const CompactInstallResult&, const CompactInstallResult&) = default;
};

struct CompactDepthResult final {
    std::size_t event_index{};
    CanonicalDisposition disposition{};
    CanonicalStatus status_after{};

    friend bool operator==(const CompactDepthResult&, const CompactDepthResult&) = default;
};

// Neutral compact execution summary accumulated by ReplayDriver::run(). It is
// derived only from already-compared typed OperationObservation values; it
// contains no Spot/USD-M classification, decimal parsing, book mutation, or
// adapter mapping. RetainNone retains full observations omitted but this
// summary is always accumulated.
struct ExecutionSummary final {
    std::size_t processed_events{};
    std::size_t install_events{};
    std::size_t depth_events{};
    std::optional<CompactInstallResult> first_install;
    std::optional<CompactDepthResult> first_depth_update;
    std::vector<CompactDepthResult> depth_results;
    std::size_t installed_count{};
    std::size_t applied_count{};
    std::size_t ignored_stale_count{};
    std::size_t ignored_duplicate_count{};
    std::size_t gap_detected_count{};
    std::size_t rejected_wrong_state_count{};
    std::size_t adapter_error_count{};
    std::size_t decimal_error_count{};
    std::size_t other_events_count{};
    std::optional<std::size_t> first_other_event_index;
    std::optional<std::size_t> first_adapter_error_index;

    friend bool operator==(const ExecutionSummary&, const ExecutionSummary&) = default;
};

struct ReplayOutcome final {
    std::vector<OperationObservation> observations;
    std::optional<OperationObservation> final_observation;
    std::optional<Divergence> first_divergence;
    ExecutionSummary summary;
    std::size_t processed_events{};

    friend bool operator==(const ReplayOutcome&, const ReplayOutcome&) = default;
};

class ReplayDriver final {
  public:
    ReplayDriver(const replay::ReplayFixture& fixture, std::unique_ptr<ReplaySide> production,
                 std::unique_ptr<ReplaySide> reference,
                 ObservationRetention retention = ObservationRetention::RetainAll);

    [[nodiscard]] ReplayOutcome run();

  private:
    const replay::ReplayFixture* fixture_;
    std::unique_ptr<ReplaySide> production_;
    std::unique_ptr<ReplaySide> reference_;
    ObservationRetention retention_;
};

// Test-only fault hooks: side wrappers that alter observations before they reach the
// comparator. Used to prove first-divergence reporting and layer attribution without
// modifying production code. MutatingSide applies a mutator to every observation;
// FailingSide refuses to produce an observation at one event index (composition
// divergence).
class MutatingSide final : public ReplaySide {
  public:
    explicit MutatingSide(std::unique_ptr<ReplaySide> wrapped,
                          std::function<void(OperationObservation&)> mutator) noexcept;

    [[nodiscard]] std::optional<OperationObservation>
    observe(const replay::Operation& operation) override;

  private:
    std::unique_ptr<ReplaySide> wrapped_;
    std::function<void(OperationObservation&)> mutator_;
};

class FailingSide final : public ReplaySide {
  public:
    FailingSide(std::unique_ptr<ReplaySide> wrapped, std::size_t fail_at_event);

    [[nodiscard]] std::optional<OperationObservation>
    observe(const replay::Operation& operation) override;

  private:
    std::unique_ptr<ReplaySide> wrapped_;
    std::size_t fail_at_event_;
    std::size_t observed_events_{0};
};

} // namespace bmd_projection::m5::oracle
