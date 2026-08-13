#include "canonical_observation.hpp"
#include "semantic_digest.hpp"
#include "semantic_manifest.hpp"

#include "../oracle/operation_observation.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace {

namespace oracle = bmd_projection::m5::oracle;
namespace replay = bmd_projection::m5::replay;
namespace semantic = bmd_projection::m5::semantic;

[[nodiscard]] bool contains_every_escape(std::string_view json) {
    static constexpr std::array<std::string_view, 11> kExpected{
        "\\u0000", "\\u0001", "\\b", "\\f", "\\u001F",  "\\\"",
        "\\\\",    "\\n",     "\\r", "\\t", "\xC3\xA9",
    };
    return std::ranges::all_of(kExpected, [json](std::string_view value) {
        return json.find(value) != std::string_view::npos;
    });
}

[[nodiscard]] bool has_only_permitted_json_bytes(std::string_view json) {
    return std::ranges::all_of(json, [](char character) {
        const auto byte = static_cast<unsigned char>(character);
        return byte >= 0x20U || byte == static_cast<unsigned char>('\n');
    });
}

TEST(SemanticManifestTest, RenderValidJson) {
    semantic::SemanticManifest manifest;
    manifest.schema_version = "M5_SEMANTIC_MANIFEST_V2";
    manifest.observation_schema_version = "M5_SEMANTIC_OBSERVATION_V2";
    manifest.head_sha = "a1db0f8374bec84d10b0005552983dd44b4e2026";
    manifest.toolchain.compiler = "GCC";
    manifest.toolchain.compiler_version = "14.2.0";
    manifest.toolchain.os = "Linux";
    manifest.toolchain.architecture = "x86_64";
    manifest.build_type = "Release";
    manifest.fixture_set_id = "abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234";

    semantic::ManifestWorkloadEntry w;
    w.workload_id = "m5-small-core-spot-v1";
    w.fixture_id = "m5-small-spot-v1";
    w.fixture_hash = "1111111111111111111111111111111111111111111111111111111111111111";
    w.semantic_digest = "2222222222222222222222222222222222222222222222222222222222222222";
    manifest.workloads.push_back(w);

    const auto json = semantic::render_manifest_json(manifest);
    EXPECT_NE(json.find("\"schema_version\": \"M5_SEMANTIC_MANIFEST_V2\""), std::string::npos);
    EXPECT_NE(json.find("\"observation_schema_version\": \"M5_SEMANTIC_OBSERVATION_V2\""),
              std::string::npos);
    EXPECT_NE(json.find("\"head_sha\""), std::string::npos);
    EXPECT_NE(json.find("\"toolchain\""), std::string::npos);
    EXPECT_NE(json.find("\"fixture_set_id\""), std::string::npos);
    EXPECT_NE(json.find("\"workloads\""), std::string::npos);
    EXPECT_NE(json.find("\"m5-small-core-spot-v1\""), std::string::npos);
    EXPECT_NE(json.find("\"GCC\""), std::string::npos);
    EXPECT_NE(json.find("\"Release\""), std::string::npos);
    EXPECT_NE(json.find("\"1111111111111111111111111111111111111111111111111111111111111111\""),
              std::string::npos);
}

TEST(SemanticManifestTest, RenderMultipleWorkloads) {
    semantic::SemanticManifest manifest;
    manifest.schema_version = "M5_SEMANTIC_MANIFEST_V2";
    manifest.observation_schema_version = "M5_SEMANTIC_OBSERVATION_V2";
    manifest.head_sha = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    manifest.toolchain.compiler = "Clang";
    manifest.toolchain.compiler_version = "19.1.0";
    manifest.toolchain.os = "Darwin";
    manifest.toolchain.architecture = "arm64";
    manifest.build_type = "Debug";
    manifest.fixture_set_id = "f" + std::string(63, '0');

    for (int i = 0; i < 4; ++i) {
        semantic::ManifestWorkloadEntry w;
        w.workload_id = "wl-" + std::to_string(i);
        w.fixture_id = "fx-" + std::to_string(i);
        w.fixture_hash = "aa" + std::string(62, static_cast<char>('0' + i));
        w.semantic_digest = "bb" + std::string(62, static_cast<char>('0' + i));
        manifest.workloads.push_back(w);
    }

    const auto json = semantic::render_manifest_json(manifest);
    EXPECT_NE(json.find("wl-0"), std::string::npos);
    EXPECT_NE(json.find("wl-3"), std::string::npos);
    EXPECT_EQ(json.find("wl-4"), std::string::npos);
    EXPECT_NE(json.find("\"workloads\""), std::string::npos);
}

TEST(SemanticManifestTest, JsonStringEscaping) {
    semantic::SemanticManifest manifest;
    manifest.schema_version = "M5_SEMANTIC_MANIFEST_V2";
    manifest.observation_schema_version = "M5_SEMANTIC_OBSERVATION_V2";
    manifest.head_sha = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    manifest.toolchain.compiler = "GCC";
    manifest.toolchain.compiler_version = "14.2.0";
    manifest.toolchain.os = "Linux";
    manifest.toolchain.architecture = "x86_64";
    manifest.build_type = "Release";
    manifest.fixture_set_id = "abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234abcd1234";

    semantic::ManifestWorkloadEntry w;
    w.workload_id = "test\"quote";
    w.fixture_id = "fixture";
    w.fixture_hash = "1111111111111111111111111111111111111111111111111111111111111111";
    w.semantic_digest = "2222222222222222222222222222222222222222222222222222222222222222";
    manifest.workloads.push_back(w);

    const auto json = semantic::render_manifest_json(manifest);
    EXPECT_NE(json.find("test\\\"quote"), std::string::npos);
}

TEST(SemanticManifestTest, JsonStringEscapesEntireControlRangeAndPreservesUtf8) {
    semantic::SemanticManifest manifest;
    manifest.schema_version = "M5_SEMANTIC_MANIFEST_V2";
    manifest.observation_schema_version = "M5_SEMANTIC_OBSERVATION_V2";
    manifest.head_sha = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    manifest.toolchain.compiler = "GNU";
    manifest.toolchain.compiler_version = "14.2.0";
    manifest.toolchain.os = "Linux";
    manifest.toolchain.architecture = "x86_64";
    manifest.build_type = "Release";
    manifest.fixture_set_id = std::string(64, 'a');

    semantic::ManifestWorkloadEntry workload;
    workload.workload_id = std::string{"A\0\x01\b\f\x1F\"\\\n\r\t\xC3\xA9", 13};
    workload.fixture_id = "fixture";
    workload.fixture_hash = std::string(64, '1');
    workload.semantic_digest = std::string(64, '2');
    manifest.workloads.push_back(workload);

    const auto json = semantic::render_manifest_json(manifest);
    EXPECT_TRUE(contains_every_escape(json));
    EXPECT_TRUE(has_only_permitted_json_bytes(json));
}

TEST(SemanticManifestTest, FixtureSetIdDeterministic) {
    std::vector<semantic::ManifestWorkloadEntry> entries;
    for (int i = 0; i < 4; ++i) {
        semantic::ManifestWorkloadEntry w;
        w.workload_id = "wl-" + std::to_string(i);
        w.fixture_id = "fx-" + std::to_string(i);
        w.fixture_hash = "aa" + std::string(62, static_cast<char>('0' + i));
        w.semantic_digest = "bb" + std::string(62, static_cast<char>('0' + i));
        entries.push_back(w);
    }

    const auto id1 = semantic::compute_fixture_set_id(entries);
    const auto id2 = semantic::compute_fixture_set_id(entries);
    EXPECT_EQ(id1, id2);
    EXPECT_EQ(id1.size(), 64);
}

TEST(SemanticManifestTest, FixtureSetIdRejectsUnorderedInput) {
    std::vector<semantic::ManifestWorkloadEntry> entries1;
    std::vector<semantic::ManifestWorkloadEntry> entries2;
    for (int i = 0; i < 4; ++i) {
        semantic::ManifestWorkloadEntry w;
        w.workload_id = "wl-" + std::to_string(i);
        w.fixture_id = "fx-" + std::to_string(i);
        w.fixture_hash = "ff" + std::string(62, '0');
        w.semantic_digest = "bb" + std::string(62, '0');
        entries1.push_back(w);
    }
    entries2 = {entries1[1], entries1[0], entries1[2], entries1[3]};

    const auto id1 = semantic::compute_fixture_set_id(entries1);
    const auto id2 = semantic::compute_fixture_set_id(entries2);
    EXPECT_NE(id1, id2);
}

TEST(SemanticManifestTest, ManifestSchemaVersionFrozen) {
    EXPECT_EQ(std::string(semantic::kManifestSchemaV1), "M5_SEMANTIC_MANIFEST_V1");
    EXPECT_EQ(std::string(semantic::kManifestSchemaV2), "M5_SEMANTIC_MANIFEST_V2");
}

TEST(SemanticManifestTest, EvidenceShaRequiresFortyLowercaseHexCharacters) {
    EXPECT_TRUE(semantic::is_valid_evidence_sha("a1db0f8374bec84d10b0005552983dd44b4e2026"));
    EXPECT_TRUE(semantic::is_valid_evidence_sha("0000000000000000000000000000000000000000"));
    EXPECT_FALSE(semantic::is_valid_evidence_sha(std::string(40, 'z')));
    EXPECT_FALSE(semantic::is_valid_evidence_sha("A1db0f8374bec84d10b0005552983dd44b4e2026"));
    EXPECT_FALSE(semantic::is_valid_evidence_sha(std::string(39, 'a')));
    EXPECT_FALSE(semantic::is_valid_evidence_sha(std::string(41, 'a')));
    EXPECT_FALSE(semantic::is_valid_evidence_sha("0x1db0f8374bec84d10b0005552983dd44b4e2026"));
    EXPECT_FALSE(semantic::is_valid_evidence_sha(" a1db0f8374bec84d10b0005552983dd44b4e202"));
    EXPECT_FALSE(semantic::is_valid_evidence_sha("a1db0f8374bec84d10b0005552983dd44b4e202\n"));
    EXPECT_FALSE(semantic::is_valid_evidence_sha("LOCAL"));
}

} // namespace
