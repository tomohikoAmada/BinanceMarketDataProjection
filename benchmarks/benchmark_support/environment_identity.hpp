#pragma once

// Runtime environment and binary identity collection (OD-M5-P6-021/022).
// The binary SHA-256 is computed from the exact executable that produced the
// result payload; environment identity is never part of a benchmark name.

#include <string>

namespace bmd_projection::m5::benchmark {

struct EnvironmentIdentity final {
    std::string os_name;
    std::string os_version;
    std::string architecture;
    std::string cpu_model;
    std::string logical_core_count;
};

[[nodiscard]] EnvironmentIdentity collect_environment_identity();

// Resolves the path of the currently running executable (platform specific)
// and returns its SHA-256, or an empty string when unavailable.
[[nodiscard]] std::string current_executable_path();
[[nodiscard]] std::string sha256_file_hex(const std::string& path);

} // namespace bmd_projection::m5::benchmark
