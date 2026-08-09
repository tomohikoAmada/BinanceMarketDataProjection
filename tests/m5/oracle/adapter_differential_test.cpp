#include "differential_test_common.hpp"

#include "adapter_production_side.hpp"
#include "adapter_scenario.hpp"
#include "divergence.hpp"
#include "operation_observation.hpp"
#include "reference_side.hpp"
#include "replay_driver.hpp"

#include "replay_types.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <functional>
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

namespace oracle = bmd_projection::m5::oracle;
namespace replay = bmd_projection::m5::replay;

using oracle::AdapterErrorOutcome;
using oracle::AdapterScenario;
using oracle::AdapterSuccessOutcome;
using oracle::ApplyOutcome;
using oracle::CanonicalAdapterCode;
using oracle::CanonicalAdapterField;
using oracle::CanonicalDecimalError;
using oracle::CanonicalDisposition;
using oracle::CanonicalGapEvidence;
using oracle::CanonicalGapReason;
using oracle::CanonicalPolicy;
using oracle::CanonicalQualityFlag;
using oracle::CanonicalResyncState;
using oracle::CanonicalSnapshotSource;
using oracle::CanonicalStatus;
using oracle::GapDescriptorObservation;
using oracle::InstallOutcome;
using oracle::MetadataOutcome;
using oracle::OperationObservation;
using oracle::RangeOutcome;
using oracle::ResetOutcome;
using oracle::SnapshotLevel;
using oracle::SnapshotOutcome;

[[nodiscard]] oracle::ReplayOutcome run_adapter(std::string_view fixture_name) {
    const auto fixture = oracle::test::load_fixture(fixture_name);
    oracle::ReplayDriver driver{
        fixture, oracle::make_adapter_production_side(fixture),
        oracle::make_reference_side(fixture, oracle::ReplayMode::AdapterEnabled)};
    return driver.run();
}

[[nodiscard]] oracle::ReplayOutcome run_adapter(const replay::ReplayFixture& fixture) {
    oracle::ReplayDriver driver{
        fixture, oracle::make_adapter_production_side(fixture),
        oracle::make_reference_side(fixture, oracle::ReplayMode::AdapterEnabled)};
    return driver.run();
}

[[nodiscard]] oracle::ReplayOutcome run_adapter(const replay::ReplayFixture& fixture,
                                                const AdapterScenario& scenario) {
    oracle::ReplayDriver driver{
        fixture, oracle::make_adapter_production_side(fixture, scenario),
        oracle::make_reference_side(fixture, oracle::ReplayMode::AdapterEnabled, scenario)};
    return driver.run();
}

[[nodiscard]] replay::SourceLocation source(std::size_t index, std::string_view line) {
    return {index, index + 2, std::string{line}};
}

// Price and quantity are adjacent string views by construction in the neutral replay
// level record; the role-named parameters keep this compact test factory readable.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
[[nodiscard]] replay::LevelInput level(replay::Side side, std::string_view price,
                                       std::string_view quantity = "1.0000") {
    return {side, std::string{price}, std::string{quantity}};
}

struct SnapshotProducer final {
    std::string producer{"adapter"};
    std::string version{"1.0"};
};

[[nodiscard]] replay::SnapshotRequestOp
snapshot_request(std::size_t index, std::optional<std::uint32_t> depth_limit = std::nullopt,
                 SnapshotProducer producer = {}) {
    return {source(index, "SNAPSHOT_REQUEST"),
            depth_limit,
            {},
            "snapshot-regression",
            std::move(producer.producer),
            std::move(producer.version),
            replay::SnapshotOrigin::GatewayLive,
            4000,
            std::nullopt,
            std::nullopt};
}

struct BaselineSides final {
    // The same-type parameters are inseparable semantic sides of this tiny test
    // value; their role names make construction sites unambiguous.
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    BaselineSides(std::vector<replay::LevelInput> bid_values,
                  std::vector<replay::LevelInput> ask_values)
        : bids{std::move(bid_values)}, asks{std::move(ask_values)} {}

    std::vector<replay::LevelInput> bids;
    std::vector<replay::LevelInput> asks;
};

[[nodiscard]] replay::ReplayFixture synchronized_fixture(BaselineSides sides,
                                                         replay::SnapshotRequestOp request) {
    auto fixture = oracle::test::load_fixture("adapter_tiny");
    fixture.replay.operations = {
        replay::InstallBaselineOp{source(0, "INSTALL_BASELINE"), 50, std::move(sides.bids),
                                  std::move(sides.asks)},
        replay::DepthUpdateOp{source(1, "DEPTH_UPDATE"), 50, 51, std::nullopt, {}},
        std::move(request)};
    return fixture;
}

[[nodiscard]] const oracle::OperationObservation& observation(const oracle::ReplayOutcome& outcome,
                                                              std::size_t index) {
    return outcome.observations.at(index);
}

[[nodiscard]] std::vector<CanonicalQualityFlag>
flags(std::initializer_list<CanonicalQualityFlag> values) {
    return {values};
}

[[nodiscard]] AdapterErrorOutcome error(CanonicalAdapterCode code, CanonicalAdapterField field,
                                        std::optional<CanonicalDecimalError> detail) {
    return {code, field, detail};
}

TEST(AdapterDifferentialReplayTest, SpotTinyAdapterBoundaryAgrees) {
    const auto outcome = run_adapter("spot_tiny");
    ASSERT_FALSE(outcome.first_divergence.has_value());
    ASSERT_EQ(outcome.observations.size(), 8U);

    EXPECT_TRUE(std::holds_alternative<MetadataOutcome>(observation(outcome, 1).result.value));

    const auto& failed_update = observation(outcome, 2);
    EXPECT_EQ(failed_update.result.value,
              (oracle::OperationResultValue{error(CanonicalAdapterCode::InvalidDecimal,
                                                  CanonicalAdapterField::BidQuantity,
                                                  CanonicalDecimalError::SignNotAllowed)}));
    EXPECT_EQ(failed_update.checkpoint.status, CanonicalStatus::AwaitingBridge);

    const auto& gap_update = observation(outcome, 3);
    const auto& adapted = std::get<AdapterSuccessOutcome>(gap_update.result.value);
    const auto& apply = std::get<ApplyOutcome>(adapted.core_result);
    EXPECT_EQ(apply.disposition, CanonicalDisposition::GapDetected);
    ASSERT_TRUE(apply.gap.has_value());
    EXPECT_EQ(apply.gap->reason, CanonicalGapReason::SpotBootstrapForwardGap);
    EXPECT_TRUE(adapted.observed_quality.empty());

    const auto& snapshot = observation(outcome, 4);
    EXPECT_EQ(snapshot.result.value, (oracle::OperationResultValue{
                                         error(CanonicalAdapterCode::MissingRequiredField,
                                               CanonicalAdapterField::CurrentGap, std::nullopt)}));

    const auto& malformed = observation(outcome, 7);
    ASSERT_TRUE(std::holds_alternative<RangeOutcome>(malformed.result.value));
    EXPECT_TRUE(std::get<RangeOutcome>(malformed.result.value).invalid_as_intended);
}

TEST(AdapterDifferentialReplayTest, UsdmTinySnapshotSemanticsAgree) {
    const auto outcome = run_adapter("usdm_tiny");
    ASSERT_FALSE(outcome.first_divergence.has_value());
    ASSERT_EQ(outcome.observations.size(), 8U);

    const auto& bridge = observation(outcome, 2);
    const auto& adapted = std::get<AdapterSuccessOutcome>(bridge.result.value);
    EXPECT_EQ(adapted.observed_quality, flags({CanonicalQualityFlag::RecoveredTail}));
    const auto& apply = std::get<ApplyOutcome>(adapted.core_result);
    EXPECT_EQ(apply.disposition, CanonicalDisposition::Applied);

    const auto& snapshot = observation(outcome, 3);
    const auto& semantic = std::get<SnapshotOutcome>(snapshot.result.value);
    EXPECT_EQ(semantic.policy, CanonicalPolicy::UsdMPerpetual);
    EXPECT_EQ(semantic.symbol, "BTCUSDT");
    EXPECT_EQ(semantic.producer, "recorder");
    EXPECT_EQ(semantic.producer_version, "1.0");
    EXPECT_EQ(semantic.source, CanonicalSnapshotSource::RecorderReplay);
    EXPECT_EQ(semantic.generated_time_utc_ns, 2000U);
    EXPECT_EQ(semantic.generated_monotonic_ns, std::optional<std::uint64_t>{3000});
    EXPECT_EQ(semantic.last_update_id, std::optional<std::uint64_t>{501});
    EXPECT_TRUE(semantic.synchronized);
    EXPECT_TRUE(semantic.bids.empty());
    EXPECT_EQ(semantic.asks, (std::vector<SnapshotLevel>{{"101.00", "3.125"}}));
    EXPECT_EQ(semantic.quality_flags, flags({CanonicalQualityFlag::RecoveredTail}));
    EXPECT_EQ(semantic.depth_limit, std::optional<std::uint32_t>{1});
    EXPECT_FALSE(semantic.gap_descriptor.has_value());
    ASSERT_TRUE(snapshot.snapshot.has_value());
    EXPECT_EQ(*snapshot.snapshot, semantic);
}

TEST(AdapterDifferentialReplayTest, RecoveryTinyDepthLimitErrorAgrees) {
    const auto outcome = run_adapter("recovery_tiny");
    ASSERT_FALSE(outcome.first_divergence.has_value());
    ASSERT_EQ(outcome.observations.size(), 7U);

    const auto& snapshot = observation(outcome, 6);
    EXPECT_EQ(snapshot.result.value, (oracle::OperationResultValue{
                                         error(CanonicalAdapterCode::InvalidDepthLimit,
                                               CanonicalAdapterField::DepthLimit, std::nullopt)}));
}

TEST(AdapterDifferentialReplayTest, AdapterTinyCoversBaselineQualityAndGapSnapshot) {
    const auto outcome = run_adapter("adapter_tiny");
    ASSERT_FALSE(outcome.first_divergence.has_value());
    ASSERT_EQ(outcome.observations.size(), 11U);

    // Baseline inbound quality: ADAPTER_METADATA before INSTALL_BASELINE is mapped
    // into observed_quality, deduplicated and rank-ordered.
    const auto& install = observation(outcome, 1);
    const auto& adapted = std::get<AdapterSuccessOutcome>(install.result.value);
    EXPECT_EQ(adapted.observed_quality,
              flags({CanonicalQualityFlag::Duplicate, CanonicalQualityFlag::Overlap}));
    const auto& install_result = std::get<InstallOutcome>(adapted.core_result);
    EXPECT_EQ(install_result.disposition, CanonicalDisposition::Installed);
    EXPECT_EQ(install_result.status_after, CanonicalStatus::AwaitingBridge);
    EXPECT_EQ(install_result.last_update_id_after, std::optional<std::uint64_t>{50});

    // Depth-limited snapshot with a host quality fact.
    const auto& limited = observation(outcome, 3);
    const auto& limited_semantic = std::get<SnapshotOutcome>(limited.result.value);
    EXPECT_EQ(limited_semantic.quality_flags, flags({CanonicalQualityFlag::Duplicate}));
    EXPECT_EQ(limited_semantic.bids, (std::vector<SnapshotLevel>{{"100.0000", "1.5000"}}));
    EXPECT_TRUE(limited_semantic.asks.empty());
    EXPECT_EQ(limited_semantic.depth_limit, std::optional<std::uint32_t>{2});
    EXPECT_TRUE(limited_semantic.synchronized);

    // Crossed book derives the Core quality flag in output only.
    const auto& crossed = observation(outcome, 6);
    const auto& crossed_semantic = std::get<SnapshotOutcome>(crossed.result.value);
    EXPECT_EQ(crossed_semantic.quality_flags, flags({CanonicalQualityFlag::CrossedBook}));
    EXPECT_EQ(crossed_semantic.bids,
              (std::vector<SnapshotLevel>{{"100.0000", "1.5000"}, {"99.0000", "3.0000"}}));
    EXPECT_EQ(crossed_semantic.asks, (std::vector<SnapshotLevel>{{"99.0000", "1.0000"}}));
    EXPECT_FALSE(crossed_semantic.depth_limit.has_value());

    // Forward gap then gap-context snapshot with gap descriptor and derived flags.
    const auto& gap = observation(outcome, 7);
    const auto& gap_apply = std::get<AdapterSuccessOutcome>(gap.result.value);
    const auto& gap_result = std::get<ApplyOutcome>(gap_apply.core_result);
    EXPECT_EQ(gap_result.disposition, CanonicalDisposition::GapDetected);
    EXPECT_EQ(gap_result.status_after, CanonicalStatus::NeedsResync);

    const auto& recovery = observation(outcome, 8);
    const auto& recovery_semantic = std::get<SnapshotOutcome>(recovery.result.value);
    EXPECT_FALSE(recovery_semantic.synchronized);
    EXPECT_EQ(recovery_semantic.last_update_id, std::optional<std::uint64_t>{55});
    EXPECT_EQ(recovery_semantic.quality_flags,
              flags({CanonicalQualityFlag::SequenceGap, CanonicalQualityFlag::OrderBookResync,
                     CanonicalQualityFlag::CrossedBook}));
    ASSERT_TRUE(recovery_semantic.gap_descriptor.has_value());
    EXPECT_EQ(
        *recovery_semantic.gap_descriptor,
        (GapDescriptorObservation{5500, 55, 60, oracle::CanonicalReasonCode::SequenceGapDetected,
                                  CanonicalResyncState::ResyncRequired}));
}

TEST(AdapterDifferentialReplayTest, ResetAndRebaselineLeaveNoHiddenState) {
    const auto outcome = run_adapter("adapter_tiny");
    ASSERT_FALSE(outcome.first_divergence.has_value());

    const auto& reset = observation(outcome, 9);
    EXPECT_TRUE(std::holds_alternative<ResetOutcome>(reset.result.value));
    EXPECT_EQ(reset.checkpoint.status, CanonicalStatus::AwaitingBaseline);
    EXPECT_FALSE(reset.checkpoint.last_update_id.has_value());
    EXPECT_FALSE(reset.checkpoint.last_gap.has_value());
    EXPECT_TRUE(reset.checkpoint.bids.empty());
    EXPECT_TRUE(reset.checkpoint.asks.empty());

    const auto& rebaseline = observation(outcome, 10);
    EXPECT_EQ(rebaseline.result.value,
              (oracle::OperationResultValue{InstallOutcome{CanonicalDisposition::Installed,
                                                           CanonicalStatus::AwaitingBridge, 70}}));
    EXPECT_EQ(rebaseline.checkpoint.last_update_id, std::optional<std::uint64_t>{70});
}

TEST(AdapterDifferentialReplayTest, RepeatedAdapterReplayIsDeterministic) {
    for (const std::string_view fixture_name :
         {"spot_tiny", "usdm_tiny", "recovery_tiny", "adapter_tiny"}) {
        SCOPED_TRACE(fixture_name);
        const auto first = run_adapter(fixture_name);
        const auto second = run_adapter(fixture_name);
        EXPECT_EQ(first, second);
    }
}

TEST(AdapterDifferentialReplayTest, SnapshotFaultIsAttributedToR4) {
    const auto fixture = oracle::test::load_fixture("usdm_tiny");
    auto reference = oracle::make_reference_side(fixture, oracle::ReplayMode::AdapterEnabled);
    std::size_t calls = 0;
    auto mutating = std::make_unique<oracle::MutatingSide>(
        std::move(reference), [&calls](OperationObservation& observation) {
            if (calls == 3 && observation.snapshot.has_value()) {
                observation.snapshot->quality_flags.clear();
            }
            ++calls;
        });
    oracle::ReplayDriver driver{fixture, oracle::make_adapter_production_side(fixture),
                                std::move(mutating)};
    const auto outcome = driver.run();
    ASSERT_TRUE(outcome.first_divergence.has_value());
    EXPECT_EQ(outcome.first_divergence->event_index, 3U);
    EXPECT_EQ(outcome.first_divergence->layer, oracle::Layer::R4);
    EXPECT_EQ(outcome.first_divergence->category, oracle::DivergenceCategory::SnapshotObservation);
}

TEST(AdapterDifferentialReplayTest, ObservedQualityFaultIsAttributedToR4) {
    const auto fixture = oracle::test::load_fixture("adapter_tiny");
    auto reference = oracle::make_reference_side(fixture, oracle::ReplayMode::AdapterEnabled);
    std::size_t calls = 0;
    auto mutating = std::make_unique<oracle::MutatingSide>(
        std::move(reference), [&calls](OperationObservation& observation) {
            if (calls == 1) {
                std::get<AdapterSuccessOutcome>(observation.result.value).observed_quality.clear();
            }
            ++calls;
        });
    oracle::ReplayDriver driver{fixture, oracle::make_adapter_production_side(fixture),
                                std::move(mutating)};
    const auto outcome = driver.run();
    ASSERT_TRUE(outcome.first_divergence.has_value());
    EXPECT_EQ(outcome.first_divergence->event_index, 1U);
    EXPECT_EQ(outcome.first_divergence->layer, oracle::Layer::R4);
    EXPECT_EQ(outcome.first_divergence->category, oracle::DivergenceCategory::OperationResult);
}

TEST(AdapterDifferentialReplayTest, AdapterErrorFaultIsAttributedToR4) {
    const auto fixture = oracle::test::load_fixture("spot_tiny");
    auto reference = oracle::make_reference_side(fixture, oracle::ReplayMode::AdapterEnabled);
    std::size_t calls = 0;
    auto mutating = std::make_unique<oracle::MutatingSide>(
        std::move(reference), [&calls](OperationObservation& observation) {
            if (calls == 2) {
                std::get<AdapterErrorOutcome>(observation.result.value).code =
                    CanonicalAdapterCode::NumericOverflow;
            }
            ++calls;
        });
    oracle::ReplayDriver driver{fixture, oracle::make_adapter_production_side(fixture),
                                std::move(mutating)};
    const auto outcome = driver.run();
    ASSERT_TRUE(outcome.first_divergence.has_value());
    EXPECT_EQ(outcome.first_divergence->event_index, 2U);
    EXPECT_EQ(outcome.first_divergence->layer, oracle::Layer::R4);
    EXPECT_EQ(outcome.first_divergence->category, oracle::DivergenceCategory::OperationResult);
}

// GoogleTest assertions inflate the path count for this single linear hidden-
// divergence scenario.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST(AdapterDifferentialReplayTest, SuccessfulParseEvidencePrecedesIgnoredDuplicateResult) {
    auto fixture = oracle::test::load_fixture("adapter_tiny");
    fixture.replay.operations = {
        replay::InstallBaselineOp{source(0, "INSTALL_BASELINE"),
                                  50,
                                  {level(replay::Side::Bid, "100.0000")},
                                  {level(replay::Side::Ask, "101.0000")}},
        replay::DepthUpdateOp{source(1, "DEPTH_UPDATE bridge"), 50, 51, std::nullopt, {}},
        replay::DepthUpdateOp{source(2, "DEPTH_UPDATE duplicate"),
                              51,
                              51,
                              std::nullopt,
                              {level(replay::Side::Bid, "99.0000")}}};
    const auto clean = run_adapter(fixture);
    ASSERT_FALSE(clean.first_divergence.has_value());
    const auto& duplicate = std::get<AdapterSuccessOutcome>(observation(clean, 2).result.value);
    EXPECT_EQ(std::get<ApplyOutcome>(duplicate.core_result).disposition,
              CanonicalDisposition::IgnoredDuplicate);

    auto reference = oracle::make_reference_side(fixture, oracle::ReplayMode::AdapterEnabled);
    std::size_t calls = 0;
    auto mutating = std::make_unique<oracle::MutatingSide>(
        std::move(reference), [&calls](OperationObservation& observed) {
            if (calls == 2) {
                auto& value = std::get<oracle::CanonicalDecimalValue>(
                    observed.decimal_observations.at(0).result);
                ++value.units;
            }
            ++calls;
        });
    oracle::ReplayDriver driver{fixture, oracle::make_adapter_production_side(fixture),
                                std::move(mutating)};
    const auto outcome = driver.run();
    ASSERT_TRUE(outcome.first_divergence.has_value());
    EXPECT_EQ(outcome.first_divergence->event_index, 2U);
    EXPECT_EQ(outcome.first_divergence->layer, oracle::Layer::R1);
    EXPECT_EQ(outcome.first_divergence->category, oracle::DivergenceCategory::ParseEvidence);
    EXPECT_EQ(outcome.first_divergence->source_line, "DEPTH_UPDATE duplicate");
}

TEST(AdapterDifferentialReplayTest, SnapshotDepthLimitIsIndependentForUnequalSides) {
    struct ExpectedSideCounts final {
        std::size_t bids{};
        std::size_t asks{};
    };
    struct Case final {
        Case(BaselineSides sides_value, std::optional<std::uint32_t> limit_value,
             ExpectedSideCounts expected_value)
            : sides{std::move(sides_value)}, limit{limit_value}, expected{expected_value} {}

        BaselineSides sides;
        std::optional<std::uint32_t> limit;
        ExpectedSideCounts expected;
    };
    const std::vector<Case> cases{
        {BaselineSides{
             {level(replay::Side::Bid, "100.0000")},
             {level(replay::Side::Ask, "101.0000"), level(replay::Side::Ask, "102.0000")}},
         std::nullopt,
         {1, 2}},
        {BaselineSides{{level(replay::Side::Bid, "100.0000"), level(replay::Side::Bid, "99.0000")},
                       {level(replay::Side::Ask, "101.0000")}},
         std::nullopt,
         {2, 1}},
        {BaselineSides{
             {level(replay::Side::Bid, "100.0000"), level(replay::Side::Bid, "99.0000")},
             {level(replay::Side::Ask, "101.0000"), level(replay::Side::Ask, "102.0000")}},
         std::nullopt,
         {2, 2}},
        {BaselineSides{
             {level(replay::Side::Bid, "100.0000"), level(replay::Side::Bid, "99.0000")},
             {level(replay::Side::Ask, "101.0000"), level(replay::Side::Ask, "102.0000")}},
         1,
         {1, 1}},
        {BaselineSides{{level(replay::Side::Bid, "100.0000"), level(replay::Side::Bid, "99.0000")},
                       {level(replay::Side::Ask, "101.0000")}},
         3,
         {2, 1}},
        {BaselineSides{{level(replay::Side::Bid, "100.0000"), level(replay::Side::Bid, "99.0000"),
                        level(replay::Side::Bid, "98.0000")},
                       {level(replay::Side::Ask, "101.0000")}},
         2,
         {2, 1}},
    };
    for (const auto& test_case : cases) {
        SCOPED_TRACE(test_case.expected.bids);
        auto fixture = synchronized_fixture(test_case.sides, snapshot_request(2, test_case.limit));
        const auto outcome = run_adapter(fixture);
        ASSERT_FALSE(outcome.first_divergence.has_value());
        const auto& snapshot = std::get<SnapshotOutcome>(observation(outcome, 2).result.value);
        EXPECT_EQ(snapshot.bids.size(), test_case.expected.bids);
        EXPECT_EQ(snapshot.asks.size(), test_case.expected.asks);
    }
}

TEST(AdapterDifferentialReplayTest, DepthValidationUsesBidThenAskWirePrecedence) {
    struct Case final {
        Case(std::vector<replay::LevelInput> levels_value, AdapterErrorOutcome expected_value)
            : levels{std::move(levels_value)}, expected{expected_value} {}

        std::vector<replay::LevelInput> levels;
        AdapterErrorOutcome expected;
    };
    const std::vector<Case> cases{
        {{level(replay::Side::Ask, "+1"), level(replay::Side::Bid, "+2")},
         error(CanonicalAdapterCode::InvalidDecimal, CanonicalAdapterField::BidPrice,
               CanonicalDecimalError::SignNotAllowed)},
        {{level(replay::Side::Bid, "100.0000"), level(replay::Side::Ask, "101.0000", "-1")},
         error(CanonicalAdapterCode::NegativeQuantity, CanonicalAdapterField::AskQuantity,
               CanonicalDecimalError::SignNotAllowed)},
        {{level(replay::Side::Bid, "+1"), level(replay::Side::Ask, "101.0000")},
         error(CanonicalAdapterCode::InvalidDecimal, CanonicalAdapterField::BidPrice,
               CanonicalDecimalError::SignNotAllowed)},
        {{level(replay::Side::Bid, "100.0000", "+1"), level(replay::Side::Bid, "+2")},
         error(CanonicalAdapterCode::InvalidDecimal, CanonicalAdapterField::BidQuantity,
               CanonicalDecimalError::SignNotAllowed)},
        {{level(replay::Side::Ask, "101.0000", "+1"), level(replay::Side::Ask, "+2")},
         error(CanonicalAdapterCode::InvalidDecimal, CanonicalAdapterField::AskQuantity,
               CanonicalDecimalError::SignNotAllowed)},
    };
    for (const auto& test_case : cases) {
        auto fixture = oracle::test::load_fixture("adapter_tiny");
        fixture.replay.operations = {replay::DepthUpdateOp{source(0, "DEPTH_UPDATE precedence"), 1,
                                                           1, std::nullopt, test_case.levels}};
        const auto outcome = run_adapter(fixture);
        ASSERT_FALSE(outcome.first_divergence.has_value());
        EXPECT_EQ(observation(outcome, 0).result.value,
                  oracle::OperationResultValue{test_case.expected});
    }
}

// GoogleTest assertions inflate the path count for this linear boundary matrix.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST(AdapterDifferentialReplayTest, ProducerIdentifiersHonorExactTextBoundaries) {
    struct Case final {
        Case(SnapshotProducer producer_value,
             std::optional<AdapterErrorOutcome> expected_error_value)
            : producer{std::move(producer_value)}, expected_error{expected_error_value} {}

        SnapshotProducer producer;
        std::optional<AdapterErrorOutcome> expected_error;
    };
    const std::vector<Case> cases{
        {SnapshotProducer{"x", "v"}, std::nullopt},
        {SnapshotProducer{"producer name", "version name"}, std::nullopt},
        {SnapshotProducer{std::string(256, 'p'), std::string(256, 'v')}, std::nullopt},
        {SnapshotProducer{"", "v"}, error(CanonicalAdapterCode::MissingRequiredField,
                                          CanonicalAdapterField::Producer, std::nullopt)},
        {SnapshotProducer{std::string(257, 'p'), "v"},
         error(CanonicalAdapterCode::InvalidIdentifier, CanonicalAdapterField::Producer,
               std::nullopt)},
        {SnapshotProducer{" producer", "v"}, error(CanonicalAdapterCode::InvalidIdentifier,
                                                   CanonicalAdapterField::Producer, std::nullopt)},
        {SnapshotProducer{"producer ", "v"}, error(CanonicalAdapterCode::InvalidIdentifier,
                                                   CanonicalAdapterField::Producer, std::nullopt)},
        {SnapshotProducer{"p", ""}, error(CanonicalAdapterCode::MissingRequiredField,
                                          CanonicalAdapterField::ProducerVersion, std::nullopt)},
        {SnapshotProducer{"p", std::string(257, 'v')},
         error(CanonicalAdapterCode::InvalidIdentifier, CanonicalAdapterField::ProducerVersion,
               std::nullopt)},
        {SnapshotProducer{"p", " version"},
         error(CanonicalAdapterCode::InvalidIdentifier, CanonicalAdapterField::ProducerVersion,
               std::nullopt)},
        {SnapshotProducer{"p", "version "},
         error(CanonicalAdapterCode::InvalidIdentifier, CanonicalAdapterField::ProducerVersion,
               std::nullopt)},
    };
    for (const auto& test_case : cases) {
        auto fixture = synchronized_fixture(
            {{level(replay::Side::Bid, "100.0000")}, {level(replay::Side::Ask, "101.0000")}},
            snapshot_request(2, std::nullopt, test_case.producer));
        const auto outcome = run_adapter(fixture);
        ASSERT_FALSE(outcome.first_divergence.has_value());
        const auto& result = observation(outcome, 2).result.value;
        if (test_case.expected_error.has_value()) {
            EXPECT_EQ(result, oracle::OperationResultValue{test_case.expected_error.value()});
        } else {
            EXPECT_TRUE(std::holds_alternative<SnapshotOutcome>(result));
        }
    }
}

TEST(AdapterDifferentialReplayTest, AdapterDimensionsVaryIndependently) {
    struct Case final {
        Case(AdapterScenario scenario_value, AdapterErrorOutcome expected_value)
            : scenario{std::move(scenario_value)}, expected{expected_value} {}

        AdapterScenario scenario;
        AdapterErrorOutcome expected;
    };
    const auto fixture = oracle::test::load_fixture("adapter_tiny");
    const auto baseline = replay::InstallBaselineOp{source(0, "INSTALL_BASELINE scenario"),
                                                    50,
                                                    {level(replay::Side::Bid, "100.0000")},
                                                    {level(replay::Side::Ask, "101.0000")}};
    auto scenario = oracle::default_adapter_scenario(fixture);
    std::vector<Case> cases;
    scenario.wire_venue = oracle::ScenarioVenue::Unspecified;
    cases.emplace_back(scenario, error(CanonicalAdapterCode::UnspecifiedEnum,
                                       CanonicalAdapterField::Venue, std::nullopt));
    scenario = oracle::default_adapter_scenario(fixture);
    scenario.wire_market = oracle::ScenarioMarket::Unspecified;
    cases.emplace_back(scenario, error(CanonicalAdapterCode::UnspecifiedEnum,
                                       CanonicalAdapterField::Market, std::nullopt));
    scenario = oracle::default_adapter_scenario(fixture);
    scenario.wire_market = oracle::ScenarioMarket::UsdMPerpetual;
    cases.emplace_back(scenario, error(CanonicalAdapterCode::IdentityMismatch,
                                       CanonicalAdapterField::Market, std::nullopt));
    scenario = oracle::default_adapter_scenario(fixture);
    scenario.wire_symbol = "ETHUSDT";
    cases.emplace_back(scenario, error(CanonicalAdapterCode::IdentityMismatch,
                                       CanonicalAdapterField::Symbol, std::nullopt));
    scenario = oracle::default_adapter_scenario(fixture);
    ++scenario.projection_numeric_spec.price_scale;
    cases.emplace_back(scenario, error(CanonicalAdapterCode::ProjectionNumericSpecMismatch,
                                       CanonicalAdapterField::ProjectionPriceScale, std::nullopt));
    scenario = oracle::default_adapter_scenario(fixture);
    ++scenario.projection_numeric_spec.quantity_scale;
    cases.emplace_back(scenario,
                       error(CanonicalAdapterCode::ProjectionNumericSpecMismatch,
                             CanonicalAdapterField::ProjectionQuantityScale, std::nullopt));
    scenario = oracle::default_adapter_scenario(fixture);
    scenario.projection_policy = replay::SequencePolicy::UsdMPerpetual;
    cases.emplace_back(scenario, error(CanonicalAdapterCode::ProjectionPolicyMismatch,
                                       CanonicalAdapterField::ProjectionPolicy, std::nullopt));

    for (const auto& test_case : cases) {
        auto case_fixture = fixture;
        case_fixture.replay.operations = {baseline};
        const auto outcome = run_adapter(case_fixture, test_case.scenario);
        ASSERT_FALSE(outcome.first_divergence.has_value());
        EXPECT_EQ(observation(outcome, 0).result.value,
                  oracle::OperationResultValue{test_case.expected});
    }
}

TEST(AdapterDifferentialReplayTest, RawUnknownVenueComposedDifferential) {
    auto fixture = oracle::test::load_fixture("adapter_tiny");
    fixture.replay.operations = {
        replay::InstallBaselineOp{source(0, "INSTALL_BASELINE raw unknown venue"),
                                  50,
                                  {level(replay::Side::Bid, "100.0000")},
                                  {level(replay::Side::Ask, "101.0000")}}};
    auto scenario = oracle::default_adapter_scenario(fixture);
    scenario.wire_venue = oracle::ScenarioVenue::UnknownNumeric;

    const auto outcome = run_adapter(fixture, scenario);

    ASSERT_FALSE(outcome.first_divergence.has_value());
    EXPECT_EQ(observation(outcome, 0).result.value,
              oracle::OperationResultValue{error(CanonicalAdapterCode::UnsupportedVenue,
                                                 CanonicalAdapterField::Venue, std::nullopt)});
}

TEST(AdapterDifferentialReplayTest, RawUnknownMarketComposedDifferential) {
    auto fixture = oracle::test::load_fixture("adapter_tiny");
    fixture.replay.operations = {
        replay::InstallBaselineOp{source(0, "INSTALL_BASELINE raw unknown market"),
                                  50,
                                  {level(replay::Side::Bid, "100.0000")},
                                  {level(replay::Side::Ask, "101.0000")}}};
    auto scenario = oracle::default_adapter_scenario(fixture);
    scenario.wire_market = oracle::ScenarioMarket::UnknownNumeric;

    const auto outcome = run_adapter(fixture, scenario);

    ASSERT_FALSE(outcome.first_divergence.has_value());
    EXPECT_EQ(observation(outcome, 0).result.value,
              oracle::OperationResultValue{error(CanonicalAdapterCode::UnknownEnumValue,
                                                 CanonicalAdapterField::Market, std::nullopt)});
}

// GoogleTest assertions inflate the path count for this linear two-row negative
// contract matrix.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST(AdapterDifferentialReplayTest, MalformedRangeIntentFailsClosed) {
    for (const auto& [first, final] : {std::pair<std::uint64_t, std::uint64_t>{1, 1}, {1, 2}}) {
        auto fixture = oracle::test::load_fixture("adapter_tiny");
        fixture.replay.operations = {
            replay::MalformedRangeOp{source(0, "MALFORMED_RANGE invalid intent"), first, final}};
        const auto outcome = run_adapter(fixture);
        ASSERT_TRUE(outcome.first_divergence.has_value());
        EXPECT_EQ(outcome.first_divergence->event_index, 0U);
        EXPECT_EQ(outcome.first_divergence->layer, oracle::Layer::D);
        EXPECT_EQ(outcome.first_divergence->category, oracle::DivergenceCategory::Composition);
    }
}

} // namespace

// NOLINTEND(bugprone-unchecked-optional-access)
