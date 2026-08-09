#pragma once

// Production Core-only pipeline side: drives M1 parsing and the M3 BookProjection
// through their public APIs and converts observable results into canonical
// observations. SNAPSHOT_REQUEST yields SnapshotNotProduced because the M4 adapter
// boundary is not linked in this mode.

#include "operation_observation.hpp"
#include "replay_side.hpp"

#include "replay_types.hpp"

#include <binance_market_data/projection/v1/projection_state/book_projection.hpp>

#include <memory>
#include <optional>
#include <vector>

namespace bmd_projection::m5::oracle {

class CoreProductionSide final : public ReplaySide {
  public:
    explicit CoreProductionSide(const replay::ReplayFixture& fixture);

    [[nodiscard]] std::optional<OperationObservation>
    observe(const replay::Operation& operation) override;

  private:
    [[nodiscard]] std::optional<OperationObservation>
    observe_install(std::uint64_t last_update_id, const std::vector<replay::LevelInput>& bids,
                    const std::vector<replay::LevelInput>& asks);
    [[nodiscard]] std::optional<OperationObservation>
    observe_depth_update(const replay::DepthUpdateOp& operation);
    [[nodiscard]] std::optional<OperationObservation> observe_snapshot_request();
    [[nodiscard]] std::optional<OperationObservation> observe_reset();
    [[nodiscard]] std::optional<OperationObservation> observe_metadata();
    [[nodiscard]] std::optional<OperationObservation>
    observe_malformed_range(const replay::MalformedRangeOp& operation);

    [[nodiscard]] SemanticCheckpoint checkpoint() const;
    [[nodiscard]] std::optional<OperationObservation>
    make_observation(OperationResultValue result,
                     std::vector<CanonicalDecimalObservation> decimals = {}) const;

    binance_market_data::projection::v1::BookProjection projection_;
};

[[nodiscard]] std::unique_ptr<ReplaySide>
make_core_production_side(const replay::ReplayFixture& fixture);

} // namespace bmd_projection::m5::oracle
