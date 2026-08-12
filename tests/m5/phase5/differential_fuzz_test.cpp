#include "replay_fuzz_decoder.hpp"
#include "replay_fuzz_fixture.hpp"

#include "core_production_side.hpp"
#include "divergence.hpp"
#include "reference_side.hpp"
#include "replay_driver.hpp"
#include "replay_side.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {
namespace decoder = bmd_projection::m5::replay::fuzz_decoder;
namespace oracle = bmd_projection::m5::oracle;
namespace replay = bmd_projection::m5::replay;

void run_and_expect_no_divergence(const decoder::FuzzCase& fuzz_case, bool expect_pass = true) {
    auto fixture = decoder::build_structured_fixture(fuzz_case, {});

    auto mode = fuzz_case.mode == decoder::DecodedMode::AdapterEnabled
                    ? oracle::ReplayMode::AdapterEnabled
                    : oracle::ReplayMode::CoreOnly;

    auto production = oracle::make_core_production_side(fixture);
    auto reference = oracle::make_reference_side(fixture, mode);

    oracle::ReplayDriver driver{fixture, std::move(production), std::move(reference),
                                oracle::ObservationRetention::RetainNone};

    auto outcome = driver.run();

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
    ops.push_back(spot_install());
    ops.push_back(spot_bridge());
    for (int i = 0; i < 4; ++i) {
        replay::DepthUpdateOp update{
            replay::SourceLocation{static_cast<std::size_t>(2 + i), 0, "test"},
            static_cast<std::uint64_t>(52 + i),
            static_cast<std::uint64_t>(52 + i),
            std::nullopt,
            {{replay::Side::Bid, std::to_string(50010 + i * 10), "1.0"}}};
        ops.push_back(update);
    }
    auto c = make_case(decoder::DecodedMode::CoreOnly, replay::Market::Spot, ops);
    run_and_expect_no_divergence(c);
}

} // namespace
