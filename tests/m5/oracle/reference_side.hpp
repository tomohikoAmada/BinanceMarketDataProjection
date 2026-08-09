#pragma once

// Reference pipeline side: composes R1 (reference decimal), R3 (ReferenceProjection),
// and R4 (ReferenceAdapter) into the ReplaySide interface. In Core-only mode R4 is
// not exercised (SNAPSHOT_REQUEST yields SnapshotNotProduced, matching the
// production Core-only side); in AdapterEnabled mode every M4-boundary event is
// predicted by R4 independently.

#include "operation_observation.hpp"
#include "replay_driver.hpp"
#include "replay_side.hpp"

#include "reference_adapter.hpp"
#include "reference_projection.hpp"

#include "replay_types.hpp"

#include <memory>
#include <optional>
#include <vector>

namespace bmd_projection::m5::oracle {

class ReferenceSide final : public ReplaySide {
  public:
    ReferenceSide(const replay::ReplayFixture& fixture, ReplayMode mode);

    [[nodiscard]] std::optional<OperationObservation>
    observe(const replay::Operation& operation) override;

  private:
    [[nodiscard]] std::optional<OperationObservation>
    observe_install(std::uint64_t last_update_id, const std::vector<replay::LevelInput>& bids,
                    const std::vector<replay::LevelInput>& asks, bool rebaseline);
    [[nodiscard]] std::optional<OperationObservation>
    observe_depth_update(const replay::DepthUpdateOp& operation);
    [[nodiscard]] std::optional<OperationObservation>
    observe_snapshot_request(const replay::SnapshotRequestOp& operation);
    [[nodiscard]] std::optional<OperationObservation> observe_reset();
    [[nodiscard]] std::optional<OperationObservation>
    observe_metadata(const replay::AdapterMetadataOp& operation);
    [[nodiscard]] std::optional<OperationObservation>
    observe_malformed_range(const replay::MalformedRangeOp& operation);

    [[nodiscard]] SemanticCheckpoint checkpoint() const;
    [[nodiscard]] std::optional<OperationObservation>
    make_observation(OperationResultValue result) const;
    [[nodiscard]] std::optional<OperationObservation>
    make_snapshot_observation(const SnapshotOutcome& snapshot) const;

    bmd_projection_reference::ReferenceProjection projection_;
    reference::ReferenceAdapter adapter_;
    replay::NumericSpec numeric_spec_;
    replay::SequencePolicy policy_{};
    std::string symbol_;
    std::vector<replay::HostQualityFact> pending_metadata_;
    ReplayMode mode_;
};

[[nodiscard]] std::unique_ptr<ReplaySide> make_reference_side(const replay::ReplayFixture& fixture,
                                                              ReplayMode mode);

} // namespace bmd_projection::m5::oracle
