#pragma once

// Production adapter-enabled pipeline side: synthesizes Contracts wire messages
// from normalized replay operations and drives the real M4 ProtoAdapter through its
// public API. Inbound producer and request/connection identifiers are fixed driver
// constants outside differential scope; snapshot producer and producer-version
// semantics come from replay operations. This side is compiled only in the
// adapter-enabled test target.

#include "adapter_scenario.hpp"
#include "operation_observation.hpp"
#include "replay_side.hpp"

#include "replay_types.hpp"

#include <binance_market_data/projection/v1/projection_state/book_projection.hpp>
#include <binance_market_data/projection/v1/snapshots.pb.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <variant>

namespace bmd_projection::m5::oracle {

// Test-only observation-boundary result. A produced wire snapshot with an enum
// value outside the accepted M4 output domain must remain distinguishable from
// a valid semantic SnapshotOutcome.
struct SnapshotExtractionError final {
    CanonicalAdapterCode code{};
    CanonicalAdapterField field{};

    friend bool operator==(const SnapshotExtractionError&,
                           const SnapshotExtractionError&) = default;
};

using SnapshotExtractionResult = std::variant<SnapshotOutcome, SnapshotExtractionError>;

[[nodiscard]] SnapshotExtractionResult extract_snapshot_observation(
    const binance_market_data::projection::v1::LocalOrderBookSnapshot& wire,
    binance_market_data::projection::v1::SequencePolicyKind policy);

class AdapterProductionSide final : public ReplaySide {
  public:
    explicit AdapterProductionSide(const replay::ReplayFixture& fixture);
    AdapterProductionSide(const replay::ReplayFixture& fixture, AdapterScenario scenario);

    [[nodiscard]] std::optional<OperationObservation>
    observe(const replay::Operation& operation) override;

  private:
    [[nodiscard]] std::optional<OperationObservation>
    observe_install(const replay::InstallBaselineOp& operation);
    [[nodiscard]] std::optional<OperationObservation>
    observe_depth_update(const replay::DepthUpdateOp& operation);
    [[nodiscard]] std::optional<OperationObservation>
    observe_snapshot_request(const replay::SnapshotRequestOp& operation);

    [[nodiscard]] SemanticCheckpoint checkpoint() const;
    [[nodiscard]] std::optional<OperationObservation>
    make_observation(OperationResultValue result,
                     std::vector<CanonicalDecimalObservation> decimals = {}) const;
    [[nodiscard]] std::optional<OperationObservation>
    make_snapshot_observation(const SnapshotOutcome& snapshot) const;

    binance_market_data::projection::v1::BookProjection projection_;
    AdapterScenario scenario_;
    std::vector<replay::HostQualityFact> pending_metadata_;
};

[[nodiscard]] std::unique_ptr<ReplaySide>
make_adapter_production_side(const replay::ReplayFixture& fixture);
[[nodiscard]] std::unique_ptr<ReplaySide>
make_adapter_production_side(const replay::ReplayFixture& fixture, const AdapterScenario& scenario);

} // namespace bmd_projection::m5::oracle
