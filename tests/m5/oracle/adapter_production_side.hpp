#pragma once

// Production adapter-enabled pipeline side: synthesizes Contracts wire messages
// from normalized replay operations and drives the real M4 ProtoAdapter through its
// public API. Non-semantic wire identity fields (producer, producer version,
// request/connection identifiers) are fixed driver constants and are outside the
// differential scope. This side is compiled only in the adapter-enabled test target.

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
    make_observation(OperationResultValue result) const;
    [[nodiscard]] std::optional<OperationObservation>
    make_snapshot_observation(const SnapshotOutcome& snapshot) const;

    binance_market_data::projection::v1::BookProjection projection_;
    replay::NumericSpec numeric_spec_;
    replay::SequencePolicy policy_{};
    std::string symbol_;
    std::vector<replay::HostQualityFact> pending_metadata_;
};

[[nodiscard]] std::unique_ptr<ReplaySide>
make_adapter_production_side(const replay::ReplayFixture& fixture);

} // namespace bmd_projection::m5::oracle
