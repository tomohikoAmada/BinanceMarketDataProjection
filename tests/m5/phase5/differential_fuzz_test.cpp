#include "replay_fuzz_decoder.hpp"
#include "replay_fuzz_fixture.hpp"

#include "adapter_scenario.hpp"
#include "core_production_side.hpp"
#include "divergence.hpp"
#include "operation_observation.hpp"
#include "reference_side.hpp"
#include "replay_driver.hpp"
#include "replay_side.hpp"

#ifdef BMD_PROJECTION_PHASE5_TEST_ADAPTER_ENABLED
#include "adapter_production_side.hpp"
#endif

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {

// Test assertions guard optional access; the dereferences below are covered by
// ASSERT/EXPECT has_value checks (repository-established pattern).
// NOLINTBEGIN(bugprone-unchecked-optional-access)

namespace decoder = bmd_projection::m5::replay::fuzz_decoder;
namespace oracle = bmd_projection::m5::oracle;
namespace replay = bmd_projection::m5::replay;

#ifdef BMD_PROJECTION_PHASE5_TEST_ADAPTER_ENABLED
[[nodiscard]] oracle::ScenarioVenue map_venue(decoder::FuzzVenue venue) noexcept {
    switch (venue) {
    case decoder::FuzzVenue::Binance:
        return oracle::ScenarioVenue::Binance;
    case decoder::FuzzVenue::Unspecified:
        return oracle::ScenarioVenue::Unspecified;
    case decoder::FuzzVenue::UnknownNumeric:
        return oracle::ScenarioVenue::UnknownNumeric;
    }
    return oracle::ScenarioVenue::Binance;
}

[[nodiscard]] oracle::ScenarioMarket map_market(decoder::FuzzMarket market) noexcept {
    switch (market) {
    case decoder::FuzzMarket::Spot:
        return oracle::ScenarioMarket::Spot;
    case decoder::FuzzMarket::UsdMPerpetual:
        return oracle::ScenarioMarket::UsdMPerpetual;
    case decoder::FuzzMarket::Unspecified:
        return oracle::ScenarioMarket::Unspecified;
    case decoder::FuzzMarket::UnknownNumeric:
        return oracle::ScenarioMarket::UnknownNumeric;
    }
    return oracle::ScenarioMarket::Spot;
}

[[nodiscard]] oracle::AdapterScenario scenario(const decoder::FuzzCase& fuzz_case) {
    return {
        map_venue(fuzz_case.scenario_venue),
        map_market(fuzz_case.scenario_market),
        fuzz_case.adapter_wire_symbol.empty() ? fuzz_case.symbol : fuzz_case.adapter_wire_symbol,
        fuzz_case.adapter_expected_symbol.empty() ? fuzz_case.symbol
                                                  : fuzz_case.adapter_expected_symbol,
        fuzz_case.adapter_expected_policy,
        fuzz_case.adapter_conversion_numeric_spec.price_scale == 0 &&
                fuzz_case.adapter_conversion_numeric_spec.quantity_scale == 0
            ? fuzz_case.numeric_spec
            : fuzz_case.adapter_conversion_numeric_spec,
        fuzz_case.adapter_projection_numeric_spec.price_scale == 0 &&
                fuzz_case.adapter_projection_numeric_spec.quantity_scale == 0
            ? fuzz_case.numeric_spec
            : fuzz_case.adapter_projection_numeric_spec,
        fuzz_case.adapter_projection_policy,
    };
}
#endif

[[nodiscard]] oracle::ReplayOutcome
run_case(const decoder::FuzzCase& fuzz_case,
         oracle::ObservationRetention retention = oracle::ObservationRetention::RetainNone) {
    auto fixture = decoder::build_structured_fixture(fuzz_case);

    auto mode = fuzz_case.mode == decoder::DecodedMode::AdapterEnabled
                    ? oracle::ReplayMode::AdapterEnabled
                    : oracle::ReplayMode::CoreOnly;

    std::unique_ptr<oracle::ReplaySide> production;
    std::unique_ptr<oracle::ReplaySide> reference;
    if (mode == oracle::ReplayMode::AdapterEnabled) {
#ifdef BMD_PROJECTION_PHASE5_TEST_ADAPTER_ENABLED
        const auto adapter_scenario = scenario(fuzz_case);
        production = oracle::make_adapter_production_side(fixture, adapter_scenario);
        reference = oracle::make_reference_side(fixture, mode, adapter_scenario);
#else
        return {};
#endif
    } else {
        production = oracle::make_core_production_side(fixture);
        reference = oracle::make_reference_side(fixture, mode);
    }

    oracle::ReplayDriver driver{fixture, std::move(production), std::move(reference), retention};

    return driver.run();
}

void run_and_expect_no_divergence(const decoder::FuzzCase& fuzz_case, bool expect_pass = true) {
    auto outcome = run_case(fuzz_case);

    if (expect_pass) {
        EXPECT_FALSE(outcome.first_divergence.has_value())
            << "Unexpected divergence at event "
            << (outcome.first_divergence.has_value()
                    ? std::to_string(outcome.first_divergence->event_index)
                    : "?")
            << (outcome.first_divergence.has_value()
                    ? ": " + oracle::render_divergence(*outcome.first_divergence)
                    : "");
    }
}

[[nodiscard]] decoder::FuzzCase load_seed(std::string_view filename) {
    const std::string path = std::string(BMD_M5_REPLAY_CORPUS_ROOT) + "/" + std::string(filename);
    std::ifstream file(path, std::ios::binary);
    const std::vector<std::uint8_t> data{std::istreambuf_iterator<char>(file),
                                         std::istreambuf_iterator<char>()};
    if (data.empty()) {
        return {};
    }
    const auto decoded = decoder::decode(data.data(), data.size());
    return decoded.value_or(decoder::FuzzCase{});
}

// Builds a minimal FuzzCase programmatically.
decoder::FuzzCase make_case(decoder::DecodedMode mode, replay::Market market,
                            const std::vector<replay::Operation>& ops) {
    decoder::FuzzCase c;
    c.mode = mode;
    c.market = market;
    c.numeric_spec = replay::NumericSpec{8, 8};
    c.sequence_policy = market == replay::Market::Spot ? replay::SequencePolicy::Spot
                                                       : replay::SequencePolicy::UsdMPerpetual;
    c.symbol = "BTCUSDT";
    c.operations = ops;
    if (mode == decoder::DecodedMode::AdapterEnabled && market == replay::Market::UsdMPerpetual) {
        c.scenario_market = decoder::FuzzMarket::UsdMPerpetual;
        c.adapter_expected_policy = replay::SequencePolicy::UsdMPerpetual;
        c.adapter_projection_policy = replay::SequencePolicy::UsdMPerpetual;
    }
    return c;
}

replay::InstallBaselineOp spot_install() {
    return replay::InstallBaselineOp{
        replay::SourceLocation{0, 0, "test"},
        50,
        {{replay::Side::Bid, "50000", "1.0"}, {replay::Side::Bid, "49900", "2.0"}},
        {{replay::Side::Ask, "50100", "1.5"}, {replay::Side::Ask, "50200", "3.0"}}};
}

replay::DepthUpdateOp spot_bridge() {
    return replay::DepthUpdateOp{replay::SourceLocation{1, 0, "test"},
                                 51,
                                 51,
                                 std::nullopt,
                                 {{replay::Side::Bid, "50010", "1.0"}}};
}

replay::DepthUpdateOp usdm_bridge() {
    return replay::DepthUpdateOp{replay::SourceLocation{1, 0, "test"},
                                 50,
                                 50,
                                 std::optional{50U},
                                 {{replay::Side::Bid, "50010", "1.0"}}};
}

replay::RebaselineOp rebaseline() {
    return replay::RebaselineOp{replay::SourceLocation{2, 0, "test"},
                                100,
                                {{replay::Side::Bid, "51000", "1.0"}},
                                {{replay::Side::Ask, "51100", "1.5"}}};
}

replay::ResetOp reset_op() { return replay::ResetOp{{2, 0, "test"}}; }

TEST(DifferentialFuzzHarnessTest, SpotCoreNoDivergence) {
    auto c = make_case(decoder::DecodedMode::CoreOnly, replay::Market::Spot,
                       {spot_install(), spot_bridge()});
    run_and_expect_no_divergence(c);
}

TEST(DifferentialFuzzHarnessTest, UsdmCoreNoDivergence) {
    replay::InstallBaselineOp install{replay::SourceLocation{0, 0, "test"},
                                      50,
                                      {{replay::Side::Bid, "50000", "1.0"}},
                                      {{replay::Side::Ask, "50100", "1.5"}}};
    auto c = make_case(decoder::DecodedMode::CoreOnly, replay::Market::UsdMPerpetual,
                       {install, usdm_bridge()});
    run_and_expect_no_divergence(c);
}

TEST(DifferentialFuzzHarnessTest, ResetRebaselineNoDivergence) {
    auto c = make_case(decoder::DecodedMode::CoreOnly, replay::Market::Spot,
                       {spot_install(), reset_op(), rebaseline()});
    run_and_expect_no_divergence(c);
}

TEST(DifferentialFuzzHarnessTest, SnapshotRequestNoDivergence) {
    replay::SnapshotRequestOp snap;
    snap.source = replay::SourceLocation{3, 0, "test"};
    snap.depth_limit = 10;
    snap.snapshot_id = "test-snap";
    snap.producer = "test";
    snap.producer_version = "1";
    snap.source_origin = replay::SnapshotOrigin::GatewayLive;
    snap.generated_time_utc_ns = 1000000;

    auto c = make_case(decoder::DecodedMode::CoreOnly, replay::Market::Spot,
                       {spot_install(), spot_bridge(), snap});
    run_and_expect_no_divergence(c);
}

TEST(DifferentialFuzzHarnessTest, MalformedRangeNoDivergence) {
    replay::InstallBaselineOp install{replay::SourceLocation{0, 0, "test"},
                                      100,
                                      {{replay::Side::Bid, "50000", "1.0"}},
                                      {{replay::Side::Ask, "50100", "1.5"}}};
    replay::MalformedRangeOp malformed{{1, 0, "test"}, 2, 1};

    auto c = make_case(decoder::DecodedMode::CoreOnly, replay::Market::Spot, {install, malformed});
    run_and_expect_no_divergence(c);
}

TEST(DifferentialFuzzHarnessTest, DecimalErrorNoDivergence) {
    replay::InstallBaselineOp install{replay::SourceLocation{0, 0, "test"},
                                      100,
                                      {{replay::Side::Bid, "", "1.0"}}, // empty price
                                      {{replay::Side::Ask, "50100", "1.5"}}};
    auto c = make_case(decoder::DecodedMode::CoreOnly, replay::Market::Spot, {install});
    run_and_expect_no_divergence(c);
}

TEST(DifferentialFuzzHarnessTest, GapNoDivergence) {
    replay::InstallBaselineOp install{replay::SourceLocation{0, 0, "test"},
                                      50,
                                      {{replay::Side::Bid, "50000", "1.0"}},
                                      {{replay::Side::Ask, "50100", "1.5"}}};
    replay::DepthUpdateOp gap_op{replay::SourceLocation{1, 0, "test"},
                                 100,
                                 101,
                                 std::nullopt,
                                 {{replay::Side::Bid, "60000", "1.0"}}};
    auto c = make_case(decoder::DecodedMode::CoreOnly, replay::Market::Spot, {install, gap_op});
    run_and_expect_no_divergence(c);
}

TEST(DifferentialFuzzHarnessTest, LargeFuzzCaseNoDivergence) {
    std::vector<replay::Operation> ops;
    ops.emplace_back(spot_install());
    ops.emplace_back(spot_bridge());
    for (int i = 0; i < 4; ++i) {
        replay::DepthUpdateOp update{
            replay::SourceLocation{static_cast<std::size_t>(2 + i), 0, "test"},
            static_cast<std::uint64_t>(52 + i),
            static_cast<std::uint64_t>(52 + i),
            std::nullopt,
            {{replay::Side::Bid, std::to_string(50010 + i * 10), "1.0"}}};
        ops.emplace_back(update);
    }
    auto c = make_case(decoder::DecodedMode::CoreOnly, replay::Market::Spot, ops);
    run_and_expect_no_divergence(c);
}

TEST(DifferentialFuzzHarnessTest, RecoverySeedEndsSynchronizedAfterPostRebaselineBridge) {
    const auto fuzz_case = load_seed("recovery.bin");
    ASSERT_FALSE(fuzz_case.operations.empty());
    const auto outcome = run_case(fuzz_case, oracle::ObservationRetention::RetainAll);
    ASSERT_FALSE(outcome.first_divergence.has_value());
    ASSERT_FALSE(outcome.observations.empty());
    EXPECT_EQ(outcome.observations.back().checkpoint.status, oracle::CanonicalStatus::Synchronized);
}

#ifdef BMD_PROJECTION_PHASE5_TEST_ADAPTER_ENABLED
TEST(DifferentialFuzzHarnessTest, SpotAdapterDepthLimitSeedTruncatesBothSnapshotSides) {
    const auto fuzz_case = load_seed("depth_limit_snapshot.bin");
    ASSERT_EQ(fuzz_case.mode, decoder::DecodedMode::AdapterEnabled);
    const auto outcome = run_case(fuzz_case, oracle::ObservationRetention::RetainAll);
    ASSERT_FALSE(outcome.first_divergence.has_value());
    ASSERT_EQ(outcome.observations.size(), 3U);
    const auto& observed = outcome.observations.back();
    const auto& snapshot = std::get<oracle::SnapshotOutcome>(observed.result.value);
    EXPECT_EQ(snapshot.depth_limit, 1U);
    EXPECT_EQ(snapshot.bids.size(), 1U);
    EXPECT_EQ(snapshot.asks.size(), 1U);
    EXPECT_GE(observed.checkpoint.bids.size(), 3U);
    EXPECT_GE(observed.checkpoint.asks.size(), 3U);
}

TEST(DifferentialFuzzHarnessTest, UsdmAdapterPathReplaysWithoutDivergence) {
    replay::InstallBaselineOp install{replay::SourceLocation{0, 0, "adapter-usdm"},
                                      50,
                                      {{replay::Side::Bid, "50000", "1.0"}},
                                      {{replay::Side::Ask, "50100", "1.5"}}};
    replay::DepthUpdateOp bridge{
        replay::SourceLocation{1, 0, "adapter-usdm"}, 50, 51, std::optional<std::uint64_t>{50}, {}};
    const auto fuzz_case = make_case(decoder::DecodedMode::AdapterEnabled,
                                     replay::Market::UsdMPerpetual, {install, bridge});
    const auto outcome = run_case(fuzz_case, oracle::ObservationRetention::RetainAll);
    ASSERT_FALSE(outcome.first_divergence.has_value());
    ASSERT_EQ(outcome.observations.size(), 2U);
    EXPECT_EQ(outcome.observations.back().checkpoint.status, oracle::CanonicalStatus::Synchronized);
}

TEST(DifferentialFuzzHarnessTest, AdapterErrorDoesNotMutateProjectionState) {
    auto fuzz_case =
        make_case(decoder::DecodedMode::AdapterEnabled, replay::Market::Spot, {spot_install()});
    fuzz_case.scenario_venue = decoder::FuzzVenue::Unspecified;
    const auto outcome = run_case(fuzz_case, oracle::ObservationRetention::RetainAll);
    ASSERT_FALSE(outcome.first_divergence.has_value());
    ASSERT_EQ(outcome.observations.size(), 1U);
    const auto& observed = outcome.observations.front();
    const auto& error = std::get<oracle::AdapterErrorOutcome>(observed.result.value);
    EXPECT_EQ(error.code, oracle::CanonicalAdapterCode::UnspecifiedEnum);
    EXPECT_EQ(error.field, oracle::CanonicalAdapterField::Venue);
    EXPECT_EQ(observed.checkpoint.status, oracle::CanonicalStatus::AwaitingBaseline);
    EXPECT_FALSE(observed.checkpoint.last_update_id.has_value());
    EXPECT_TRUE(observed.checkpoint.bids.empty());
    EXPECT_TRUE(observed.checkpoint.asks.empty());
}

TEST(DifferentialFuzzHarnessTest, QualitySeedSeparatesInboundHostAndDerivedFacts) {
    const auto fuzz_case = load_seed("quality_combinations.bin");
    ASSERT_EQ(fuzz_case.mode, decoder::DecodedMode::AdapterEnabled);
    const auto outcome = run_case(fuzz_case, oracle::ObservationRetention::RetainAll);
    ASSERT_FALSE(outcome.first_divergence.has_value());
    ASSERT_EQ(outcome.observations.size(), 4U);

    const auto& install =
        std::get<oracle::AdapterSuccessOutcome>(outcome.observations.at(1).result.value);
    EXPECT_EQ(install.observed_quality, (std::vector<oracle::CanonicalQualityFlag>{
                                            oracle::CanonicalQualityFlag::OutOfOrder,
                                            oracle::CanonicalQualityFlag::ProducerRestart}));

    const auto& snapshot =
        std::get<oracle::SnapshotOutcome>(outcome.observations.back().result.value);
    EXPECT_EQ(snapshot.quality_flags, (std::vector<oracle::CanonicalQualityFlag>{
                                          oracle::CanonicalQualityFlag::Duplicate,
                                          oracle::CanonicalQualityFlag::SnapshotTooOld,
                                          oracle::CanonicalQualityFlag::CrossedBook}));
    EXPECT_EQ(std::ranges::count(snapshot.quality_flags, oracle::CanonicalQualityFlag::OutOfOrder),
              0);
    EXPECT_EQ(
        std::ranges::count(snapshot.quality_flags, oracle::CanonicalQualityFlag::ProducerRestart),
        0);
}
#endif

// Regression: exact libFuzzer input recovered from CI run 31596081744.
// Previously decoded a normal DepthUpdateOp with first > final, which
// diverged (RANGE_OUTCOME vs APPLY_OUTCOME). After the decoder domain fix
// it must decode to conforming operations and replay without divergence.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST(DifferentialFuzzHarnessTest, CiReproducerNoDivergence) {
    // clang-format off
    const std::vector<std::uint8_t> data{
        0x20, 0x08, 0x07, 0x42, 0x54, 0x43, 0x55, 0x53, 0x44, 0x54, 0x03, 0x00, 0x00,
        0x32, 0x01, 0x00, 0x00, 0x04, 0x06, 0x00, 0x00, 0x00, 0x00, 0x32, 0x01, 0x00,
        0x01, 0x01, 0x00, 0x04, 0x06, 0x00, 0xff, 0x00, 0x00, 0x31, 0x01, 0x05, 0x01,
        0x00, 0x33, 0x00, 0x33, 0xf9, 0x00, 0x01, 0x00, 0x00, 0x04, 0x06, 0x00, 0x00,
        0x00, 0x00, 0x31, 0x00, 0x05, 0x01, 0x00, 0x33, 0x00, 0x33, 0x00, 0x01, 0x00,
        0x00, 0x50, 0x40, 0x00, 0x08, 0x1c, 0x31, 0x31, 0x01, 0x00};
    // clang-format on
    const auto fuzz_case = decoder::decode(data.data(), data.size());
    ASSERT_TRUE(fuzz_case.has_value());
    ASSERT_EQ(fuzz_case->operations.size(), 3U);

    for (const auto& op : fuzz_case->operations) {
        if (const auto* update = std::get_if<replay::DepthUpdateOp>(&op)) {
            EXPECT_LE(update->first_update_id, update->final_update_id)
                << "DepthUpdateOp must satisfy first <= final";
        }
        if (const auto* malformed = std::get_if<replay::MalformedRangeOp>(&op)) {
            EXPECT_GT(malformed->first_update_id, malformed->final_update_id)
                << "MalformedRangeOp must satisfy first > final";
        }
    }
    run_and_expect_no_divergence(*fuzz_case);
}

// Regression: exact libFuzzer input recovered from CI run 31608561257.
// Previously decoded a MalformedRangeOp with a non-reversed range
// (first == final == 0), which the comparator rejects as a composition
// divergence. After the decoder domain fix it must decode to conforming
// operations and replay without divergence.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST(DifferentialFuzzHarnessTest, CiMalformedValidRangeNoDivergence) {
    // clang-format off
    const std::vector<std::uint8_t> data{
        0x20, 0x08, 0x00, 0x04, 0x06, 0x00, 0x00, 0x00, 0x00, 0x31, 0x00, 0x05, 0x40, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x04, 0x04, 0x06, 0x00, 0x00, 0x00, 0x00, 0x31,
        0x01, 0x00};
    // clang-format on
    const auto fuzz_case = decoder::decode(data.data(), data.size());
    ASSERT_TRUE(fuzz_case.has_value());

    for (const auto& op : fuzz_case->operations) {
        if (const auto* update = std::get_if<replay::DepthUpdateOp>(&op)) {
            EXPECT_LE(update->first_update_id, update->final_update_id)
                << "DepthUpdateOp must satisfy first <= final";
        }
        if (const auto* malformed = std::get_if<replay::MalformedRangeOp>(&op)) {
            EXPECT_GT(malformed->first_update_id, malformed->final_update_id)
                << "MalformedRangeOp must satisfy first > final";
        }
    }
    run_and_expect_no_divergence(*fuzz_case);
}

// Regression: exact libFuzzer input recovered from CI run 31612853523.
// Previously decoded a RebaselineOp whose ask-vector level carried
// Side::Bid, so production installed it positionally into asks while the
// reference installed it into bids by per-level side. After the decoder
// side-normalization fix, baseline/rebaseline vectors always carry the
// vector's own side and replay without divergence.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST(DifferentialFuzzHarnessTest, CiRebaselineSideNoDivergence) {
    // clang-format off
    const std::vector<std::uint8_t> data{
        0x20, 0x08, 0x07, 0x42, 0x54, 0x43, 0x55, 0x53, 0x44, 0x54, 0x04, 0x00, 0x00,
        0x32, 0x01, 0x00, 0x00, 0x04, 0x06, 0x00, 0x00, 0x00, 0x00, 0x31, 0x01, 0x00,
        0x01, 0x01, 0x00, 0x04, 0x06, 0x00, 0x01, 0x00, 0x00, 0x30, 0x01, 0x05, 0x01,
        0x00, 0x33, 0x00, 0x33, 0x00, 0x01, 0x00, 0x00, 0x04, 0x06, 0x00, 0x00, 0x01,
        0x00, 0x31, 0x02, 0x00, 0x04, 0x01, 0x05, 0x02, 0x00, 0x01, 0x06, 0x61, 0x62,
        0x63, 0x31, 0x32, 0x33, 0x04, 0x66, 0x75, 0x7a, 0x7a, 0x01, 0x31, 0x00, 0x02,
        0x0f, 0x42, 0x40, 0x01, 0x02, 0x07, 0xa1, 0x20, 0x00, 0x04, 0x00, 0x00, 0x08,
        0x6e, 0x6f, 0x2d, 0x6c, 0x69, 0x6d, 0x69, 0x74, 0x04, 0x66, 0x75, 0x7a, 0x7a,
        0x01, 0x31, 0x02, 0x02, 0x1e, 0x84, 0x80, 0x00, 0x01, 0x00, 0x32, 0x01};
    // clang-format on
    const auto fuzz_case = decoder::decode(data.data(), data.size());
    ASSERT_TRUE(fuzz_case.has_value());

    for (const auto& op : fuzz_case->operations) {
        if (const auto* install = std::get_if<replay::InstallBaselineOp>(&op)) {
            for (const auto& level : install->bids) {
                EXPECT_EQ(level.side, replay::Side::Bid);
            }
            for (const auto& level : install->asks) {
                EXPECT_EQ(level.side, replay::Side::Ask);
            }
        }
        if (const auto* rebaseline = std::get_if<replay::RebaselineOp>(&op)) {
            for (const auto& level : rebaseline->bids) {
                EXPECT_EQ(level.side, replay::Side::Bid);
            }
            for (const auto& level : rebaseline->asks) {
                EXPECT_EQ(level.side, replay::Side::Ask);
            }
        }
    }
    run_and_expect_no_divergence(*fuzz_case);
}

} // namespace

// NOLINTEND(bugprone-unchecked-optional-access)
