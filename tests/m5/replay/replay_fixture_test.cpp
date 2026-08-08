#include "replay_fixture.hpp"

#include <gtest/gtest.h>

#include <filesystem>

namespace replay = bmd_projection::m5::replay;

#ifndef BMD_M5_FIXTURE_ROOT
#error "BMD_M5_FIXTURE_ROOT must be defined by CMake"
#endif

TEST(M5ReplayFixtureTest, LoadsSpotUsdMAndRecoveryTinyFixtures) {
    for (const auto name : {"spot_tiny", "usdm_tiny", "recovery_tiny"}) {
        const auto loaded = replay::load_fixture(std::filesystem::path{BMD_M5_FIXTURE_ROOT} / name);
        ASSERT_TRUE(std::holds_alternative<replay::ReplayFixture>(loaded)) << name;
        const auto& fixture = std::get<replay::ReplayFixture>(loaded);
        EXPECT_FALSE(fixture.replay.operations.empty());
        EXPECT_EQ(fixture.manifest.identity.fixture_id, fixture.replay.header.fixture_id);
    }
}

TEST(M5ReplayFixtureTest, EncodesExplicitBootstrapBridgeContracts) {
    const auto spot = replay::spot_materializer_contract();
    EXPECT_EQ(spot.snapshot_identity, "REST depth snapshot lastUpdateId=L");
    EXPECT_EQ(spot.first_bridge_rule, "first eligible event satisfies U <= L+1 <= u");
    EXPECT_NE(spot.buffer_window.find("snapshot acquisition"), std::string::npos);
    EXPECT_NE(spot.failure_rule.find("rejects"), std::string::npos);

    const auto usdm = replay::usdm_materializer_contract();
    EXPECT_EQ(usdm.first_bridge_rule, "first eligible event satisfies U <= L <= u");
    EXPECT_NE(usdm.post_bridge_rule.find("pu == local_last_update_id"), std::string::npos);
    EXPECT_NE(usdm.discard_rule.find("u < L"), std::string::npos);
}

TEST(M5ReplayFixtureTest, RequiresExplicitImmutableRecorderProvenance) {
    const replay::ImmutableSourceArchive valid{
        "archive-root-identity",
        "cf1e749c7a533e916dbfb685212e5549a38c70dd",
        "926615b09ef46130f49a87fe8ab20acb7cfa6313daa67af5b718931bd95ff329",
        "a399e647faaac58b5db24e835f1c29e799c70ad0c94ec77b597cac2647cfb734",
        "preflight/m21-4-24h-20260805T150930Z/",
        {{"spot-diff-0001", "0000000000000000000000000000000000000000000000000000000000000000"}},
        "Spot",
        "BTCUSDT",
        "2026-08-05T15:09:30.200566Z/2026-08-06T15:09:30.200566Z"};
    EXPECT_TRUE(std::holds_alternative<std::monostate>(replay::validate_source_archive(valid)));

    auto invalid = valid;
    invalid.recorder_config_sha256 = "host-path-or-unhashed-config";
    const auto result = replay::validate_source_archive(invalid);
    ASSERT_TRUE(std::holds_alternative<replay::ParseError>(result));
    EXPECT_EQ(std::get<replay::ParseError>(result).category,
              replay::ErrorCategory::InvalidMetadata);
}
