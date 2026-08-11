#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace bmd_projection::m5::semantic {

inline constexpr std::string_view kManifestSchemaV1 = "M5_SEMANTIC_MANIFEST_V1";

struct ManifestWorkloadEntry final {
    std::string workload_id;
    std::string fixture_id;
    std::string fixture_hash;
    std::string semantic_digest;
};

struct ManifestToolchain final {
    std::string compiler;
    std::string compiler_version;
    std::string os;
    std::string architecture;
};

struct SemanticManifest final {
    std::string schema_version;
    std::string head_sha;
    ManifestToolchain toolchain;
    std::string build_type;
    std::string fixture_set_id;
    std::vector<ManifestWorkloadEntry> workloads;
};

[[nodiscard]] std::string render_manifest_json(const SemanticManifest& manifest);

[[nodiscard]] std::string
compute_fixture_set_id(const std::vector<ManifestWorkloadEntry>& workloads);

} // namespace bmd_projection::m5::semantic
