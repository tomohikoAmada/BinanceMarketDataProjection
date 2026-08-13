#pragma once

#include "../oracle/operation_observation.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace bmd_projection::m5::semantic {

inline constexpr std::string_view kObservationSchemaV1 = "M5_SEMANTIC_OBSERVATION_V1";
inline constexpr std::string_view kObservationSchemaV2 = "M5_SEMANTIC_OBSERVATION_V2";

// Historical V1 is retained as an explicit serializer so its byte meaning stays
// testable. V2 is the current serializer and adds complete snapshot wire identity.
[[nodiscard]] std::string serialize_observation_v1(const oracle::OperationObservation& observation);

[[nodiscard]] std::string serialize_observation(const oracle::OperationObservation& observation);

[[nodiscard]] std::vector<std::string>
serialize_observation_stream_v1(const std::vector<oracle::OperationObservation>& observations);

[[nodiscard]] std::vector<std::string>
serialize_observation_stream(const std::vector<oracle::OperationObservation>& observations);

} // namespace bmd_projection::m5::semantic
