#pragma once

#include "replay_types.hpp"

#include <filesystem>
#include <string_view>

namespace bmd_projection::m5::replay {

[[nodiscard]] Result<ReplayManifest> parse_manifest(std::string_view bytes);
[[nodiscard]] Result<ReplayManifest> load_manifest(const std::filesystem::path& path);

} // namespace bmd_projection::m5::replay
