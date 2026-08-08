#pragma once

#include "replay_types.hpp"

#include <filesystem>

namespace bmd_projection::m5::replay {

[[nodiscard]] Result<ReplayFixture> load_fixture(const std::filesystem::path& directory);

} // namespace bmd_projection::m5::replay
