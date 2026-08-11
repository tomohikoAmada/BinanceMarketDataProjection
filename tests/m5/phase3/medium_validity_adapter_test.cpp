#include "medium_validity.hpp"

#include "adapter_production_side.hpp"
#include "divergence.hpp"
#include "operation_observation.hpp"
#include "reference_side.hpp"
#include "replay_driver.hpp"

#include "canonical_text.hpp"
#include "replay_fixture.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdlib>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace {

namespace oracle = bmd_projection::m5::oracle;
namespace phase3 = bmd_projection::m5::phase3;
namespace replay = bmd_projection::m5::replay;

[[nodiscard]] replay::ReplayFixture build_fixture(std::string_view fixture_id,
                                                  std::string_view market, std::string_view policy,
                                                  const std::vector<std::string>& events) {
    std::ostringstream log;
    log << "REPLAY_V1 market=" << market << " symbol=BTCUSDT price_scale=2 "
        << "quantity_scale=3 policy=" << policy << " fixture_id=" << fixture_id << '\n';
    for (const auto& event : events) {
        log << event << '\n';
    }
    const auto replay_log = log.str();
    const auto hash_result = replay::sha256_hex(replay_log);
    if (!std::holds_alternative<std::string>(hash_result)) {
        std::abort();
    }
    std::ostringstream manifest;
    manifest << "MANIFEST_V1\n"
             << "fixture_id=" << fixture_id << '\n'
             << "schema_version=REPLAY_V1\n"
             << "log_sha256=" << std::get<std::string>(hash_result) << '\n'
             << "market=" << market << '\n'
             << "symbol=BTCUSDT\n"
             << "price_scale=2\n"
             << "quantity_scale=3\n"
             << "policy=" << policy << '\n'
             << "event_count=" << events.size() << '\n';
    const auto loaded = replay::load_fixture(replay::FixtureBytes{replay_log, manifest.str()});
    if (!std::holds_alternative<replay::ReplayFixture>(loaded)) {
        std::abort();
    }
    return std::get<replay::ReplayFixture>(loaded);
}

constexpr std::string_view kBaseline =
    "INSTALL_BASELINE 100 B:100.00,1.000|B:99.00,2.000 A:101.00,1.500|A:102.00,2.500";

[[nodiscard]] replay::ReplayFixture valid_spot_corpus() {
    return build_fixture("med-adapter-valid", "Spot", "Spot",
                         {std::string{kBaseline}, "DEPTH_UPDATE 99 101 pu=- B:100.00,1.125",
                          "DEPTH_UPDATE 102 102 pu=- A:101.00,1.000",
                          "DEPTH_UPDATE 103 103 pu=- B:99.50,2.000",
                          "DEPTH_UPDATE 104 104 pu=- A:102.00,2.250"});
}

[[nodiscard]] replay::ReplayFixture equal_but_invalid_corpus() {
    return build_fixture("med-adapter-invalid", "Spot", "Spot",
                         {std::string{kBaseline}, "DEPTH_UPDATE 102 102 pu=- B:100.00,1.125",
                          "DEPTH_UPDATE 103 103 pu=- A:101.00,1.000",
                          "DEPTH_UPDATE 104 104 pu=- B:99.50,2.000"});
}

[[nodiscard]] replay::ReplayFixture bridge_then_gap_corpus() {
    return build_fixture("med-adapter-gap", "Spot", "Spot",
                         {std::string{kBaseline}, "DEPTH_UPDATE 99 101 pu=- B:100.00,1.125",
                          "DEPTH_UPDATE 102 102 pu=- A:101.00,1.000",
                          "DEPTH_UPDATE 104 104 pu=- B:99.50,2.000",
                          "DEPTH_UPDATE 105 105 pu=- A:102.00,2.250"});
}

[[nodiscard]] oracle::ReplayOutcome run_adapter(const replay::ReplayFixture& fixture) {
    oracle::ReplayDriver driver{
        fixture, oracle::make_adapter_production_side(fixture),
        oracle::make_reference_side(fixture, oracle::ReplayMode::AdapterEnabled),
        oracle::ObservationRetention::RetainNone};
    return driver.run();
}

} // namespace

TEST(MediumValidityAdapterTest, ValidCorpusPassesAdapterLifecycleGate) {
    const auto fixture = valid_spot_corpus();
    const auto outcome = run_adapter(fixture);
    EXPECT_FALSE(outcome.first_divergence.has_value());
    const auto report = phase3::check_medium_validity(outcome, fixture, 3);
    EXPECT_TRUE(report.valid);
    EXPECT_EQ(report.applied_count, 4U);
    EXPECT_EQ(report.adapter_error_count, 0U);
    EXPECT_EQ(report.final_status, oracle::CanonicalStatus::Synchronized);
}

TEST(MediumValidityAdapterTest, EqualButInvalidLifecycleFailsAdapter) {
    const auto fixture = equal_but_invalid_corpus();
    const auto outcome = run_adapter(fixture);
    EXPECT_FALSE(outcome.first_divergence.has_value());
    const auto report = phase3::check_medium_validity(outcome, fixture, 2);
    EXPECT_FALSE(report.valid);
    EXPECT_EQ(report.reason, "bridge-not-applied");
    EXPECT_EQ(report.event_index, 1U);
}

TEST(MediumValidityAdapterTest, ValidBridgeThenGapFailsAdapter) {
    const auto fixture = bridge_then_gap_corpus();
    const auto outcome = run_adapter(fixture);
    EXPECT_FALSE(outcome.first_divergence.has_value());
    const auto report = phase3::check_medium_validity(outcome, fixture, 3);
    EXPECT_FALSE(report.valid);
    EXPECT_EQ(report.reason, "depth-update-not-applied");
    EXPECT_EQ(report.event_index, 3U);
}
