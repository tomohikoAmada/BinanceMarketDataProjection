#pragma once

// Pipeline-side interface for the neutral ReplayDriver. Each side consumes one
// normalized canonical operation and returns the corresponding canonical
// OperationObservation for the pipeline it models (production Core/M4 or the
// layered reference oracle). The driver performs orchestration only.

#include "operation_observation.hpp"

#include "replay_types.hpp"

#include <optional>

namespace bmd_projection::m5::oracle {

class ReplaySide {
  public:
    ReplaySide() = default;
    virtual ~ReplaySide() = default;

    ReplaySide(const ReplaySide&) = delete;
    ReplaySide& operator=(const ReplaySide&) = delete;
    ReplaySide(ReplaySide&&) = delete;
    ReplaySide& operator=(ReplaySide&&) = delete;

    [[nodiscard]] virtual std::optional<OperationObservation>
    observe(const replay::Operation& operation) = 0;
};

} // namespace bmd_projection::m5::oracle
