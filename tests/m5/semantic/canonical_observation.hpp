#pragma once

#include "../oracle/operation_observation.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace bmd_projection::m5::semantic {

inline constexpr std::string_view kObservationSchemaV1 = "M5_SEMANTIC_OBSERVATION_V1";

[[nodiscard]] std::string serialize_observation(const oracle::OperationObservation& observation);

[[nodiscard]] std::vector<std::string>
serialize_observation_stream(const std::vector<oracle::OperationObservation>& observations);

} // namespace bmd_projection::m5::semantic
