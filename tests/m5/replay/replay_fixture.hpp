#pragma once

#include "replay_types.hpp"

#include <filesystem>
#include <string_view>

namespace bmd_projection::m5::replay {

[[nodiscard]] Result<ReplayFixture> load_fixture(const std::filesystem::path& directory);

// Validates already-materialized canonical bytes through the same parser, manifest,
// identity, and event-count boundary as the directory loader. Phase-3 deterministic
// generators use this overload so generated workloads do not depend on temporary paths.
[[nodiscard]] Result<ReplayFixture> load_fixture(std::string_view replay_log_bytes,
                                                 std::string_view manifest_bytes);

} // namespace bmd_projection::m5::replay
