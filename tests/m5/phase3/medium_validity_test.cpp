#include "medium_validity.hpp"

#include "core_production_side.hpp"
#include "divergence.hpp"
#include "operation_observation.hpp"
#include "reference_side.hpp"
#include "replay_driver.hpp"

#include "canonical_text.hpp"
#include "replay_fixture.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
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

struct FixtureSpec final {
    std::string_view market;
    std::string_view policy;
    std::string_view fixture_id;
    std::vector<std::string> events;
};

[[nodiscard]] replay::ReplayFixture build_fixture(const FixtureSpec& spec) {
    std::ostringstream log;
    log << "REPLAY_V1 market=" << spec.market << " symbol=BTCUSDT price_scale=2 "
        << "quantity_scale=3 policy=" << spec.policy << " fixture_id=" << spec.fixture_id << '\n';
    for (const auto& event : spec.events) {
        log << event << '\n';
    }
    const auto replay_log = log.str();
    const auto hash_result = replay::sha256_hex(replay_log);
    if (!std::holds_alternative<std::string>(hash_result)) {
        std::abort();
    }
    std::ostringstream manifest;
    manifest << "MANIFEST_V1\n"
             << "fixture_id=" << spec.fixture_id << '\n'
             << "schema_version=REPLAY_V1\n"
             << "log_sha256=" << std::get<std::string>(hash_result) << '\n'
             << "market=" << spec.market << '\n'
             << "symbol=BTCUSDT\n"
             << "price_scale=2\n"
             << "quantity_scale=3\n"
             << "policy=" << spec.policy << '\n'
             << "event_count=" << spec.events.size() << '\n';
    const auto loaded = replay::load_fixture(replay::FixtureBytes{replay_log, manifest.str()});
    if (!std::holds_alternative<replay::ReplayFixture>(loaded)) {
        std::abort();
    }
    return std::get<replay::ReplayFixture>(loaded);
}

constexpr std::string_view kBaseline =
    "INSTALL_BASELINE 100 B:100.00,1.000|B:99.00,2.000 A:101.00,1.500|A:102.00,2.500";

constexpr std::string_view kBaseline500 =
    "INSTALL_BASELINE 500 B:100.00,1.000|B:99.00,2.000 A:101.00,1.500|A:102.00,2.500";

[[nodiscard]] replay::ReplayFixture valid_spot_corpus() {
    return build_fixture(
        {"Spot",
         "Spot",
         "med-valid-spot",
         {std::string{kBaseline}, "DEPTH_UPDATE 99 101 pu=- B:100.00,1.125",
          "DEPTH_UPDATE 102 102 pu=- A:101.00,1.000", "DEPTH_UPDATE 103 103 pu=- B:99.50,2.000",
          "DEPTH_UPDATE 104 104 pu=- A:102.00,2.250"}});
}

[[nodiscard]] replay::ReplayFixture valid_usdm_corpus() {
    return build_fixture(
        {"UsdMPerpetual",
         "UsdMPerpetual",
         "med-valid-usdm",
         {std::string{kBaseline}, "DEPTH_UPDATE 99 100 pu=99 B:100.00,1.125",
          "DEPTH_UPDATE 101 101 pu=100 A:101.00,1.000", "DEPTH_UPDATE 102 102 pu=101 B:99.50,2.000",
          "DEPTH_UPDATE 103 103 pu=102 A:102.00,2.250"}});
}

// Baseline installs, first depth is a bootstrap forward gap, remaining
// operations are rejected in NeedsResync. Production and reference agree.
[[nodiscard]] replay::ReplayFixture equal_but_invalid_corpus() {
    return build_fixture(
        {"Spot",
         "Spot",
         "med-equal-but-invalid",
         {std::string{kBaseline}, "DEPTH_UPDATE 102 102 pu=- B:100.00,1.125",
          "DEPTH_UPDATE 103 103 pu=- A:101.00,1.000", "DEPTH_UPDATE 104 104 pu=- B:99.50,2.000"}});
}

// Valid bridge and one live update, then a Spot live forward gap and rejected
// tail: final state NeedsResync.
[[nodiscard]] replay::ReplayFixture bridge_then_gap_corpus() {
    return build_fixture(
        {"Spot",
         "Spot",
         "med-bridge-then-gap",
         {std::string{kBaseline}, "DEPTH_UPDATE 99 101 pu=- B:100.00,1.125",
          "DEPTH_UPDATE 102 102 pu=- A:101.00,1.000", "DEPTH_UPDATE 104 104 pu=- B:99.50,2.000",
          "DEPTH_UPDATE 105 105 pu=- A:102.00,2.250"}});
}

// Valid bridge and live updates, then a stale final update whose u is behind
// the accepted ID.
[[nodiscard]] replay::ReplayFixture trailing_stale_corpus() {
    return build_fixture(
        {"Spot",
         "Spot",
         "med-trailing-stale",
         {std::string{kBaseline}, "DEPTH_UPDATE 99 101 pu=- B:100.00,1.125",
          "DEPTH_UPDATE 102 102 pu=- A:101.00,1.000", "DEPTH_UPDATE 103 103 pu=- B:99.50,2.000",
          "DEPTH_UPDATE 101 101 pu=- A:102.00,2.250"}});
}

[[nodiscard]] oracle::ReplayOutcome
run_core(const replay::ReplayFixture& fixture,
         oracle::ObservationRetention retention = oracle::ObservationRetention::RetainNone) {
    oracle::ReplayDriver driver{fixture, oracle::make_core_production_side(fixture),
                                oracle::make_reference_side(fixture, oracle::ReplayMode::CoreOnly),
                                retention};
    return driver.run();
}

class TempDirectory final {
  public:
    TempDirectory() : path_{std::filesystem::temp_directory_path() / "bmd-m5-med-tmp"} {
        std::filesystem::create_directories(path_);
    }

    TempDirectory(const TempDirectory&) = delete;
    TempDirectory& operator=(const TempDirectory&) = delete;
    TempDirectory(TempDirectory&&) = delete;
    TempDirectory& operator=(TempDirectory&&) = delete;

    ~TempDirectory() { std::filesystem::remove_all(path_); }

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

  private:
    std::filesystem::path path_;
};

void write_provenance(const std::filesystem::path& directory, std::string_view body) {
    std::ofstream stream{directory / "corpus_provenance.json"};
    stream << body;
}

void expect_spot_bridge_applied(const oracle::ReplayOutcome& outcome, std::string_view range) {
    EXPECT_EQ(outcome.summary.depth_results.front().status_after,
              oracle::CanonicalStatus::Synchronized)
        << range;
    EXPECT_EQ(outcome.summary.applied_count, 1U) << range;
}

void expect_spot_bootstrap_gap(const oracle::ReplayOutcome& outcome, std::string_view range) {
    EXPECT_EQ(outcome.summary.gap_detected_count, 1U) << range;
    EXPECT_EQ(outcome.summary.depth_results.front().status_after,
              oracle::CanonicalStatus::NeedsResync)
        << range;
}

} // namespace

TEST(MediumValidityTest, ValidSpotCorpusPassesLifecycleGate) {
    const auto fixture = valid_spot_corpus();
    const auto outcome = run_core(fixture);
    EXPECT_FALSE(outcome.first_divergence.has_value());
    const auto report = phase3::check_medium_validity(outcome, fixture, 3);
    EXPECT_TRUE(report.valid);
    EXPECT_EQ(report.install_events, 1U);
    EXPECT_EQ(report.depth_events, 4U);
    EXPECT_EQ(report.installed_count, 1U);
    EXPECT_EQ(report.applied_count, 4U);
    EXPECT_EQ(report.ignored_stale_count, 0U);
    EXPECT_EQ(report.ignored_duplicate_count, 0U);
    EXPECT_EQ(report.gap_detected_count, 0U);
    EXPECT_EQ(report.rejected_wrong_state_count, 0U);
    EXPECT_EQ(report.final_status, oracle::CanonicalStatus::Synchronized);
    ASSERT_TRUE(report.final_accepted_update_id.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(report.final_accepted_update_id.value(), 104U);
    ASSERT_TRUE(report.last_selected_diff_final_update_id.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(report.last_selected_diff_final_update_id.value(), 104U);
    ASSERT_TRUE(report.first_install.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    const auto& first_install = report.first_install.value();
    EXPECT_EQ(first_install.disposition, oracle::CanonicalDisposition::Installed);
    EXPECT_EQ(first_install.status_after, oracle::CanonicalStatus::AwaitingBridge);
    ASSERT_TRUE(report.first_depth_update.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    const auto& first_depth_update = report.first_depth_update.value();
    EXPECT_EQ(first_depth_update.disposition, oracle::CanonicalDisposition::Applied);
    EXPECT_EQ(first_depth_update.status_after, oracle::CanonicalStatus::Synchronized);
}

TEST(MediumValidityTest, ValidUsdMCorpusPassesLifecycleGate) {
    const auto fixture = valid_usdm_corpus();
    const auto outcome = run_core(fixture);
    EXPECT_FALSE(outcome.first_divergence.has_value());
    const auto report = phase3::check_medium_validity(outcome, fixture, 3);
    EXPECT_TRUE(report.valid);
    EXPECT_EQ(report.applied_count, 4U);
    ASSERT_TRUE(report.final_accepted_update_id.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(report.final_accepted_update_id.value(), 103U);
    EXPECT_EQ(report.final_status, oracle::CanonicalStatus::Synchronized);
}

TEST(MediumValidityTest, EqualButInvalidLifecycleFailsCoreWithStableReason) {
    const auto fixture = equal_but_invalid_corpus();
    const auto outcome = run_core(fixture);
    EXPECT_FALSE(outcome.first_divergence.has_value());
    const auto report = phase3::check_medium_validity(outcome, fixture, 2);
    EXPECT_FALSE(report.valid);
    EXPECT_EQ(report.reason, "bridge-not-applied");
    EXPECT_EQ(report.event_index, 1U);
    EXPECT_EQ(report.gap_detected_count, 1U);
    EXPECT_EQ(report.rejected_wrong_state_count, 2U);
    EXPECT_EQ(report.final_status, oracle::CanonicalStatus::NeedsResync);
}

TEST(MediumValidityTest, ValidBridgeThenLiveGapFails) {
    const auto fixture = bridge_then_gap_corpus();
    const auto outcome = run_core(fixture);
    EXPECT_FALSE(outcome.first_divergence.has_value());
    const auto report = phase3::check_medium_validity(outcome, fixture, 3);
    EXPECT_FALSE(report.valid);
    EXPECT_EQ(report.reason, "depth-update-not-applied");
    EXPECT_EQ(report.event_index, 3U);
}

TEST(MediumValidityTest, ValidBridgeThenRejectedWrongStateFails) {
    const auto fixture = bridge_then_gap_corpus();
    const auto outcome = run_core(fixture);
    EXPECT_FALSE(outcome.first_divergence.has_value());
    const auto report = phase3::check_medium_validity(outcome, fixture, 3);
    EXPECT_FALSE(report.valid);
    EXPECT_EQ(report.rejected_wrong_state_count, 1U);
}

TEST(MediumValidityTest, FinalNeedsResyncFailsMandatoryValidity) {
    const auto fixture = bridge_then_gap_corpus();
    const auto outcome = run_core(fixture);
    const auto report = phase3::check_medium_validity(outcome, fixture, 3);
    EXPECT_FALSE(report.valid);
    EXPECT_EQ(report.final_status, oracle::CanonicalStatus::NeedsResync);
}

TEST(MediumValidityTest, WrongExpectedAppliedCountFails) {
    const auto fixture = valid_spot_corpus();
    const auto outcome = run_core(fixture);
    const auto report = phase3::check_medium_validity(outcome, fixture, 5);
    EXPECT_FALSE(report.valid);
    EXPECT_EQ(report.reason, "unexpected-depth-event-count");
}

TEST(MediumValidityTest, FinalAcceptedIdMismatchFails) {
    const auto fixture = trailing_stale_corpus();
    const auto outcome = run_core(fixture);
    EXPECT_FALSE(outcome.first_divergence.has_value());
    const auto report = phase3::check_medium_validity(outcome, fixture, 3);
    EXPECT_FALSE(report.valid);
    ASSERT_TRUE(report.final_accepted_update_id.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(report.final_accepted_update_id.value(), 103U);
    ASSERT_TRUE(report.last_selected_diff_final_update_id.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(report.last_selected_diff_final_update_id.value(), 101U);
}

TEST(MediumValidityTest, DifferentialDivergenceIsNotHiddenByLifecycleGate) {
    const auto fixture = valid_spot_corpus();
    auto reference = oracle::make_reference_side(fixture, oracle::ReplayMode::CoreOnly);
    auto calls = std::make_shared<std::size_t>(0U);
    auto mutating = std::make_unique<oracle::MutatingSide>(
        std::move(reference), [calls](oracle::OperationObservation& observation) {
            if (*calls == 3U) {
                observation.checkpoint.bids.push_back({1, 1});
            }
            ++*calls;
        });
    oracle::ReplayDriver driver{fixture, oracle::make_core_production_side(fixture),
                                std::move(mutating), oracle::ObservationRetention::RetainNone};
    const auto outcome = driver.run();
    ASSERT_TRUE(outcome.first_divergence.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(outcome.first_divergence.value().event_index, 3U);
    const auto report = phase3::check_medium_validity(outcome, fixture, 3);
    EXPECT_FALSE(report.valid);
    EXPECT_EQ(report.reason, "differential-divergence");
    EXPECT_EQ(report.event_index, 3U);
}

TEST(MediumValidityTest, RetainNoneStillAccumulatesIdenticalSummary) {
    const auto fixture = valid_spot_corpus();
    const auto retained = run_core(fixture, oracle::ObservationRetention::RetainAll);
    const auto bounded = run_core(fixture, oracle::ObservationRetention::RetainNone);
    EXPECT_EQ(retained.summary, bounded.summary);
    EXPECT_EQ(retained.observations.size(), 5U);
    EXPECT_TRUE(bounded.observations.empty());
    EXPECT_EQ(retained.final_observation, bounded.final_observation);
    EXPECT_EQ(bounded.summary.applied_count, 4U);
    EXPECT_EQ(bounded.summary.installed_count, 1U);
}

TEST(MediumValidityTest, SummaryCountsStaleAndDuplicateTypedResults) {
    const auto fixture = build_fixture(
        {"Spot",
         "Spot",
         "med-stale-duplicate",
         {std::string{kBaseline}, "DEPTH_UPDATE 90 99 pu=- B:100.00,1.125",
          "DEPTH_UPDATE 99 100 pu=- A:101.00,1.000", "DEPTH_UPDATE 99 101 pu=- B:100.00,1.125",
          "DEPTH_UPDATE 102 102 pu=- A:101.00,1.000", "DEPTH_UPDATE 103 103 pu=- B:99.50,2.000",
          "DEPTH_UPDATE 104 104 pu=- A:102.00,2.250"}});
    const auto outcome = run_core(fixture);
    EXPECT_FALSE(outcome.first_divergence.has_value());
    EXPECT_EQ(outcome.summary.ignored_stale_count, 1U);
    EXPECT_EQ(outcome.summary.ignored_duplicate_count, 1U);
    EXPECT_EQ(outcome.summary.applied_count, 4U);
    EXPECT_EQ(outcome.summary.depth_events, 6U);
}

TEST(MediumValidityTest, DirectSpotSuccessorConformanceTable) {
    // DIRECT protocol-conformance table (ADR-0008): expected outcomes are
    // encoded here explicitly, independently of production/reference equality.
    // This test fails if BOTH production and reference models are changed back
    // to contains-L, because exact-next bootstrap input would then be reported
    // as a gap instead of the Applied dispositions asserted below.
    struct ConformanceCase final {
        std::string_view range;
        oracle::CanonicalDisposition expected;
    };
    const std::vector<ConformanceCase> cases = {
        {"DEPTH_UPDATE 400 499 pu=- B:100.00,1.125", oracle::CanonicalDisposition::IgnoredStale},
        {"DEPTH_UPDATE 499 500 pu=- B:100.00,1.125",
         oracle::CanonicalDisposition::IgnoredDuplicate},
        {"DEPTH_UPDATE 500 500 pu=- B:100.00,1.125",
         oracle::CanonicalDisposition::IgnoredDuplicate},
        {"DEPTH_UPDATE 499 501 pu=- B:100.00,1.125", oracle::CanonicalDisposition::Applied},
        {"DEPTH_UPDATE 500 501 pu=- B:100.00,1.125", oracle::CanonicalDisposition::Applied},
        {"DEPTH_UPDATE 501 501 pu=- B:100.00,1.125", oracle::CanonicalDisposition::Applied},
        {"DEPTH_UPDATE 501 502 pu=- B:100.00,1.125", oracle::CanonicalDisposition::Applied},
        {"DEPTH_UPDATE 499 502 pu=- B:100.00,1.125", oracle::CanonicalDisposition::Applied},
        {"DEPTH_UPDATE 502 502 pu=- B:100.00,1.125", oracle::CanonicalDisposition::GapDetected},
        {"DEPTH_UPDATE 502 503 pu=- B:100.00,1.125", oracle::CanonicalDisposition::GapDetected},
    };
    for (const auto& test_case : cases) {
        const auto fixture =
            build_fixture({"Spot",
                           "Spot",
                           "med-conformance",
                           {std::string{kBaseline500}, std::string{test_case.range}}});
        const auto outcome = run_core(fixture);
        ASSERT_FALSE(outcome.first_divergence.has_value()) << test_case.range;
        ASSERT_EQ(outcome.summary.depth_results.size(), 1U) << test_case.range;
        EXPECT_EQ(outcome.summary.depth_results.front().disposition, test_case.expected)
            << test_case.range;
        if (test_case.expected == oracle::CanonicalDisposition::Applied) {
            expect_spot_bridge_applied(outcome, test_case.range);
        }
        if (test_case.expected == oracle::CanonicalDisposition::GapDetected) {
            expect_spot_bootstrap_gap(outcome, test_case.range);
        }
    }
}

TEST(MediumValidityTest, DirectSpotExactNextLiveSuccessorLocked) {
    // Against C=500 the exact-next live successor is [501,501]; a later live
    // event beginning at C+1 remains valid after synchronization.
    const auto fixture = build_fixture(
        {"Spot",
         "Spot",
         "med-live-exact-next",
         {std::string{kBaseline500}, "DEPTH_UPDATE 501 501 pu=- B:100.00,1.125",
          "DEPTH_UPDATE 502 502 pu=- A:101.00,1.000", "DEPTH_UPDATE 503 503 pu=- B:99.50,2.000"}});
    const auto outcome = run_core(fixture);
    EXPECT_FALSE(outcome.first_divergence.has_value());
    EXPECT_EQ(outcome.summary.applied_count, 3U);
    EXPECT_EQ(outcome.summary.gap_detected_count, 0U);
    ASSERT_TRUE(outcome.final_observation.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(outcome.final_observation->checkpoint.status, oracle::CanonicalStatus::Synchronized);
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(outcome.final_observation->checkpoint.last_update_id, 503U);
}

TEST(MediumValidityTest, JsonIntentReaderAcceptsCanonicalProvenance) {
    TempDirectory temporary;
    write_provenance(
        temporary.path(),
        R"({"bootstrap_bridge":{"final_update_id":104},"event_count":5,"final_selected_update_id":104,"selected_live_updates_after_synchronization":3,"source_raw_chunks":[]})");
    const auto target = phase3::read_target_live_updates(temporary.path());
    ASSERT_TRUE(target.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(target.value(), 3U);
}

TEST(MediumValidityTest, JsonIntentReaderFailsClosed) {
    TempDirectory temporary;
    const auto& path = temporary.path();
    const auto* const canonical =
        R"({"selected_live_updates_after_synchronization":3,"other":"x"})";
    EXPECT_FALSE(phase3::read_target_live_updates(path).has_value());

    write_provenance(path, canonical);
    EXPECT_TRUE(phase3::read_target_live_updates(path).has_value());

    write_provenance(path, R"({"selected_live_updates_after_synchronization":3,)");
    EXPECT_FALSE(phase3::read_target_live_updates(path).has_value());

    write_provenance(path, R"({"selected_live_updates_after_synchronization":3.5})");
    EXPECT_FALSE(phase3::read_target_live_updates(path).has_value());

    write_provenance(path, R"({"selected_live_updates_after_synchronization":-3})");
    EXPECT_FALSE(phase3::read_target_live_updates(path).has_value());

    write_provenance(path, R"({"selected_live_updates_after_synchronization":3} extra)");
    EXPECT_FALSE(phase3::read_target_live_updates(path).has_value());

    write_provenance(
        path,
        R"({"selected_live_updates_after_synchronization":3,"selected_live_updates_after_synchronization":4})");
    EXPECT_FALSE(phase3::read_target_live_updates(path).has_value());

    write_provenance(path, R"({"other_key":3})");
    EXPECT_FALSE(phase3::read_target_live_updates(path).has_value());

    write_provenance(path, R"("selected_live_updates_after_synchronization":3)");
    EXPECT_FALSE(phase3::read_target_live_updates(path).has_value());

    write_provenance(path, "[]");
    EXPECT_FALSE(phase3::read_target_live_updates(path).has_value());
}

TEST(MediumValidityTest, JsonUint64BoundaryAcceptsMaximumAndRejectsOverflow) {
    TempDirectory temporary;
    const auto& path = temporary.path();

    write_provenance(path,
                     R"({"selected_live_updates_after_synchronization":18446744073709551615})");
    const auto maximum = phase3::read_target_live_updates(path);
    ASSERT_TRUE(maximum.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    EXPECT_EQ(maximum.value(), std::numeric_limits<std::uint64_t>::max());

    write_provenance(path,
                     R"({"selected_live_updates_after_synchronization":18446744073709551616})");
    EXPECT_FALSE(phase3::read_target_live_updates(path).has_value());

    write_provenance(path,
                     R"({"selected_live_updates_after_synchronization":36893488147419103231})");
    EXPECT_FALSE(phase3::read_target_live_updates(path).has_value());

    write_provenance(
        path,
        R"({"selected_live_updates_after_synchronization":9999999999999999999999999999999999})");
    EXPECT_FALSE(phase3::read_target_live_updates(path).has_value());
}

TEST(MediumValidityTest, TargetOverflowBoundaryFailsClosed) {
    const auto fixture = valid_spot_corpus();
    const auto outcome = run_core(fixture);

    {
        const auto report = phase3::check_medium_validity(
            outcome, fixture, std::numeric_limits<std::uint64_t>::max());
        EXPECT_FALSE(report.valid);
        EXPECT_EQ(report.reason, "invalid-target");
    }

    {
        const auto report = phase3::check_medium_validity(
            outcome, fixture, std::numeric_limits<std::uint64_t>::max() - 1U);
        EXPECT_FALSE(report.valid);
        EXPECT_EQ(report.reason, "invalid-target");
    }

    {
        const auto report = phase3::check_medium_validity(
            outcome, fixture, std::numeric_limits<std::uint64_t>::max() - 2U);
        EXPECT_FALSE(report.valid);
        EXPECT_NE(report.reason, "invalid-target");
    }

    EXPECT_TRUE(phase3::check_medium_validity(outcome, fixture, 3U).valid);
}
