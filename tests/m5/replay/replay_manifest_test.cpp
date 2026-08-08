#include "replay_fixture.hpp"
#include "replay_manifest.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace replay = bmd_projection::m5::replay;

#ifndef BMD_M5_FIXTURE_ROOT
#error "BMD_M5_FIXTURE_ROOT must be defined by CMake"
#endif

TEST(M5ReplayManifestTest, ParsesManifestAndVerifiesCanonicalIdentity) {
    const auto fixture =
        replay::load_fixture(std::filesystem::path{BMD_M5_FIXTURE_ROOT} / "spot_tiny");
    ASSERT_TRUE(std::holds_alternative<replay::ReplayFixture>(fixture));
    const auto& loaded = std::get<replay::ReplayFixture>(fixture);
    EXPECT_EQ(loaded.identity.replay_log_sha256,
              "77d6c315db00fab348305cdcb9d708585b85fc82ee67c59bcb2597cc6f060488");
    EXPECT_EQ(loaded.identity.fixture_id, "spot-tiny-v1");
    EXPECT_EQ(loaded.manifest.event_count, loaded.replay.operations.size());
    EXPECT_EQ(loaded.replay.header.symbol, "BTCUSDT");
}

TEST(M5ReplayManifestTest, RejectsUnsupportedSchemaAndMalformedIdentityFields) {
    const auto unsupported = replay::parse_manifest("MANIFEST_V2\n");
    ASSERT_TRUE(std::holds_alternative<replay::ParseError>(unsupported));
    EXPECT_EQ(std::get<replay::ParseError>(unsupported).category,
              replay::ErrorCategory::UnsupportedSchema);

    const auto malformed = replay::parse_manifest(
        "MANIFEST_V1\nfixture_id=x\nschema_version=REPLAY_V1\nlog_sha256=not-a-hash\n"
        "market=Spot\nsymbol=BTCUSDT\nprice_scale=8\nquantity_scale=8\npolicy=Spot\nevent_count="
        "0\n");
    ASSERT_TRUE(std::holds_alternative<replay::ParseError>(malformed));
    EXPECT_EQ(std::get<replay::ParseError>(malformed).category,
              replay::ErrorCategory::ManifestParse);
}

TEST(M5ReplayManifestTest, FailsClosedOnEventCountAndShaMismatch) {
    const std::string manifest =
        "MANIFEST_V1\nfixture_id=x\nschema_version=REPLAY_V1\n"
        "log_sha256=0000000000000000000000000000000000000000000000000000000000000000\n"
        "market=Spot\nsymbol=BTCUSDT\nprice_scale=8\nquantity_scale=8\npolicy=Spot\nevent_count="
        "1\n";
    const auto parsed = replay::parse_manifest(manifest);
    ASSERT_TRUE(std::holds_alternative<replay::ReplayManifest>(parsed));
    EXPECT_EQ(std::get<replay::ReplayManifest>(parsed).event_count, 1U);
}

TEST(M5ReplayManifestTest, FixtureLoaderRejectsManifestShaAndEventCountMismatches) {
    const auto source = std::filesystem::path{BMD_M5_FIXTURE_ROOT} / "spot_tiny";
    const auto temporary = std::filesystem::temp_directory_path() / "bmd-m5-fixture-mismatch";
    std::filesystem::remove_all(temporary);
    std::filesystem::create_directories(temporary);
    std::filesystem::copy_file(source / "replay.log", temporary / "replay.log");

    {
        std::ofstream output(temporary / "manifest.txt", std::ios::binary);
        output << "MANIFEST_V1\nfixture_id=spot-tiny-v1\nschema_version=REPLAY_V1\n"
                  "log_sha256=0000000000000000000000000000000000000000000000000000000000000000\n"
                  "market=Spot\nsymbol=BTCUSDT\nprice_scale=8\nquantity_scale=8\npolicy="
                  "Spot\nevent_count=8\n";
    }
    const auto sha_failure = replay::load_fixture(temporary);
    ASSERT_TRUE(std::holds_alternative<replay::ParseError>(sha_failure));
    EXPECT_EQ(std::get<replay::ParseError>(sha_failure).category,
              replay::ErrorCategory::IdentityMismatch);

    {
        std::ofstream output(temporary / "manifest.txt", std::ios::binary);
        output << "MANIFEST_V1\nfixture_id=spot-tiny-v1\nschema_version=REPLAY_V1\n"
                  "log_sha256=77d6c315db00fab348305cdcb9d708585b85fc82ee67c59bcb2597cc6f060488\n"
                  "market=Spot\nsymbol=BTCUSDT\nprice_scale=8\nquantity_scale=8\npolicy="
                  "Spot\nevent_count=7\n";
    }
    const auto count_failure = replay::load_fixture(temporary);
    ASSERT_TRUE(std::holds_alternative<replay::ParseError>(count_failure));
    EXPECT_EQ(std::get<replay::ParseError>(count_failure).category,
              replay::ErrorCategory::EventCountMismatch);
    std::filesystem::remove_all(temporary);
}

TEST(M5ReplayManifestTest, RepeatedFixtureLoadingIsExactlyDeterministic) {
    const auto path = std::filesystem::path{BMD_M5_FIXTURE_ROOT} / "usdm_tiny";
    const auto first = replay::load_fixture(path);
    ASSERT_TRUE(std::holds_alternative<replay::ReplayFixture>(first));
    for (int count = 0; count < 5; ++count) {
        const auto next = replay::load_fixture(path);
        ASSERT_TRUE(std::holds_alternative<replay::ReplayFixture>(next));
        EXPECT_EQ(std::get<replay::ReplayFixture>(next), std::get<replay::ReplayFixture>(first));
    }
}
