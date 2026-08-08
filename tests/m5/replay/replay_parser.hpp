#pragma once

#include "replay_types.hpp"

#include <filesystem>
#include <string_view>

namespace bmd_projection::m5::replay {

[[nodiscard]] Result<NormalizedReplay> parse_replay_log(std::string_view bytes);
[[nodiscard]] Result<NormalizedReplay> load_replay_log(const std::filesystem::path& path);

} // namespace bmd_projection::m5::replay
