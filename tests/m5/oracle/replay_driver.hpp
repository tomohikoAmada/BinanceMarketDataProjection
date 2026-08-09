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

struct ReplayOutcome final {
    std::vector<OperationObservation> observations;
    std::optional<Divergence> first_divergence;
    std::size_t processed_events{};

    friend bool operator==(const ReplayOutcome&, const ReplayOutcome&) = default;
};

class ReplayDriver final {
  public:
    ReplayDriver(const replay::ReplayFixture& fixture, std::unique_ptr<ReplaySide> production,
                 std::unique_ptr<ReplaySide> reference);

    [[nodiscard]] ReplayOutcome run();

  private:
    const replay::ReplayFixture* fixture_;
    std::unique_ptr<ReplaySide> production_;
    std::unique_ptr<ReplaySide> reference_;
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
