#pragma once

#include "replay_types.hpp"

#include <string>
#include <string_view>

namespace bmd_projection::m5::replay {

[[nodiscard]] Result<std::monostate> validate_canonical_bytes(std::string_view bytes);
[[nodiscard]] Result<std::string> sha256_hex(std::string_view bytes);

[[nodiscard]] bool is_canonical_integer(std::string_view token);
[[nodiscard]] Result<std::uint64_t> parse_uint64(std::string_view token, std::size_t line,
                                                 std::size_t event, std::size_t field);
[[nodiscard]] Result<std::uint32_t> parse_uint32(std::string_view token, std::size_t line,
                                                 std::size_t event, std::size_t field);

} // namespace bmd_projection::m5::replay
