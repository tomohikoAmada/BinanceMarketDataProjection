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

#include <memory>
#include <optional>

namespace bmd_projection::m5::oracle {

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
