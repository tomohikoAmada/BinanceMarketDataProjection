#pragma once

#include "canonical_observation.hpp"

#include <string>
#include <vector>

namespace bmd_projection::m5::semantic {

[[nodiscard]] std::string
compute_semantic_digest(const std::vector<std::string>& canonical_observations);

[[nodiscard]] std::string compute_semantic_digest_from_observations(
    const std::vector<oracle::OperationObservation>& observations);

} // namespace bmd_projection::m5::semantic
