#pragma once

#include "replay_types.hpp"

#include <cstddef>
#include <string_view>

namespace bmd_projection::m5::phase3 {

inline constexpr std::string_view kSmallGeneratorVersion = "m5-small-generator-v1";
inline constexpr std::size_t kSmallWorkloadEventCount = 2'048;

[[nodiscard]] replay::ReplayFixture make_spot_small_workload();
[[nodiscard]] replay::ReplayFixture make_usdm_small_workload();

} // namespace bmd_projection::m5::phase3
