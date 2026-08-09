#include "differential_test_common.hpp"

#include "core_production_side.hpp"
#include "divergence.hpp"
#include "operation_observation.hpp"
#include "reference_side.hpp"
#include "replay_driver.hpp"

#include "replay_types.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <variant>

namespace {

// Test assertions guard optional access; the dereferences below are covered by
// ASSERT/EXPECT has_value checks (repository-established pattern).
// NOLINTBEGIN(bugprone-unchecked-optional-access)

namespace oracle = bmd_projection::m5::oracle;
namespace replay = bmd_projection::m5::replay;

using oracle::AdapterErrorOutcome;
using oracle::ApplyOutcome;
using oracle::CanonicalAdapterCode;
using oracle::CanonicalAdapterField;
using oracle::CanonicalDecimalError;
using oracle::CanonicalDisposition;
using oracle::CanonicalLevel;
using oracle::CanonicalQualityFlag;
using oracle::CanonicalStatus;
using oracle::DecimalErrorOutcome;
using oracle::InstallOutcome;
using oracle::OperationObservation;
using oracle::ResetOutcome;
using oracle::SnapshotOutcome;

// A stateful mutator: applies `mutation` only to the observation produced for the
// given 0-based event index. The driver stamps event identity after observe(), so
// the mutation targets the call count instead of the observation fields.
[[nodiscard]] std::function<void(OperationObservation&)>
mutator_for(std::size_t event_index, void (*mutation)(OperationObservation&)) {
    return
        [event_index, mutation, calls = std::size_t{0}](OperationObservation& observation) mutable {
            if (calls == event_index) {
                mutation(observation);
            }
            ++calls;
        };
}

[[nodiscard]] oracle::ReplayOutcome
run_with_mutated_reference(std::string_view fixture_name,
                           std::function<void(OperationObservation&)> mutation) {
    const auto fixture = oracle::test::load_fixture(fixture_name);
    auto reference = oracle::make_reference_side(fixture, oracle::ReplayMode::CoreOnly);
    auto mutating =
        std::make_unique<oracle::MutatingSide>(std::move(reference), std::move(mutation));
    oracle::ReplayDriver driver{fixture, oracle::make_core_production_side(fixture),
                                std::move(mutating)};
    return driver.run();
}

[[nodiscard]] oracle::ReplayOutcome run_with_failing_reference(std::string_view fixture_name,
                                                               std::size_t fail_at_event) {
    const auto fixture = oracle::test::load_fixture(fixture_name);
    auto reference = oracle::make_reference_side(fixture, oracle::ReplayMode::CoreOnly);
    auto failing = std::make_unique<oracle::FailingSide>(std::move(reference), fail_at_event);
    oracle::ReplayDriver driver{fixture, oracle::make_core_production_side(fixture),
                                std::move(failing)};
    return driver.run();
}

// GoogleTest assertion macros inflate the measured path count for this linear
// fail-fast diagnostic assertion sequence.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST(FaultInjectionTest, DecimalResultMismatchStopsAtEventAndAttributesR1) {
    const auto outcome = run_with_mutated_reference(
        "spot_tiny", mutator_for(0, [](OperationObservation& observation) {
            observation.result.value = DecimalErrorOutcome{CanonicalDecimalError::Overflow};
        }));
    ASSERT_TRUE(outcome.first_divergence.has_value());
    EXPECT_EQ(outcome.first_divergence->event_index, 0U);
    EXPECT_EQ(outcome.first_divergence->layer, oracle::Layer::R1);
    EXPECT_EQ(outcome.first_divergence->category, oracle::DivergenceCategory::OperationResult);
    EXPECT_EQ(outcome.processed_events, 1U);
    EXPECT_TRUE(outcome.observations.empty());
    EXPECT_NE(outcome.first_divergence->production_value.find("INSTALL_OUTCOME"),
              std::string::npos);
    EXPECT_NE(outcome.first_divergence->reference_value.find("DECIMAL_ERROR"), std::string::npos);
}

TEST(FaultInjectionTest, DecimalCategoryMismatchIsR1ResultFieldDivergence) {
    const auto outcome = run_with_mutated_reference(
        "spot_tiny", mutator_for(2, [](OperationObservation& observation) {
            std::get<DecimalErrorOutcome>(observation.result.value).category =
                CanonicalDecimalError::Overflow;
        }));
    ASSERT_TRUE(outcome.first_divergence.has_value());
    EXPECT_EQ(outcome.first_divergence->event_index, 2U);
    EXPECT_EQ(outcome.first_divergence->layer, oracle::Layer::R1);
    EXPECT_EQ(outcome.first_divergence->category, oracle::DivergenceCategory::OperationResult);
    EXPECT_EQ(outcome.processed_events, 3U);
}

TEST(FaultInjectionTest, CheckpointStatusMismatchIsR3CheckpointDivergence) {
    const auto outcome = run_with_mutated_reference(
        "spot_tiny", mutator_for(1, [](OperationObservation& observation) {
            observation.checkpoint.status = CanonicalStatus::Synchronized;
        }));
    ASSERT_TRUE(outcome.first_divergence.has_value());
    EXPECT_EQ(outcome.first_divergence->event_index, 1U);
    EXPECT_EQ(outcome.first_divergence->layer, oracle::Layer::R3);
    EXPECT_EQ(outcome.first_divergence->category, oracle::DivergenceCategory::Checkpoint);
    EXPECT_EQ(outcome.processed_events, 2U);
}

TEST(FaultInjectionTest, BookLevelMismatchIsR2CheckpointDivergence) {
    const auto outcome = run_with_mutated_reference(
        "spot_tiny", mutator_for(0, [](OperationObservation& observation) {
            observation.checkpoint.bids.push_back({1, 1});
        }));
    ASSERT_TRUE(outcome.first_divergence.has_value());
    EXPECT_EQ(outcome.first_divergence->event_index, 0U);
    EXPECT_EQ(outcome.first_divergence->layer, oracle::Layer::R2);
    EXPECT_EQ(outcome.first_divergence->category, oracle::DivergenceCategory::Checkpoint);
}

TEST(FaultInjectionTest, ApplyResultMismatchIsR3OperationResultDivergence) {
    const auto outcome = run_with_mutated_reference(
        "usdm_tiny", mutator_for(2, [](OperationObservation& observation) {
            std::get<ApplyOutcome>(observation.result.value).disposition =
                CanonicalDisposition::IgnoredDuplicate;
        }));
    ASSERT_TRUE(outcome.first_divergence.has_value());
    EXPECT_EQ(outcome.first_divergence->event_index, 2U);
    EXPECT_EQ(outcome.first_divergence->layer, oracle::Layer::R3);
    EXPECT_EQ(outcome.first_divergence->category, oracle::DivergenceCategory::OperationResult);
    EXPECT_NE(outcome.first_divergence->detail.find("disposition"), std::string::npos);
}

TEST(FaultInjectionTest, ResetResultKindMismatchIsR3OperationResultDivergence) {
    const auto outcome = run_with_mutated_reference(
        "spot_tiny", mutator_for(5, [](OperationObservation& observation) {
            observation.result.value =
                InstallOutcome{CanonicalDisposition::Installed, CanonicalStatus::AwaitingBridge, 0};
        }));
    ASSERT_TRUE(outcome.first_divergence.has_value());
    EXPECT_EQ(outcome.first_divergence->event_index, 5U);
    EXPECT_EQ(outcome.first_divergence->layer, oracle::Layer::R3);
    EXPECT_EQ(outcome.first_divergence->category, oracle::DivergenceCategory::OperationResult);
}

TEST(FaultInjectionTest, MissingObservationIsCompositionDivergence) {
    const auto outcome = run_with_failing_reference("spot_tiny", 1);
    ASSERT_TRUE(outcome.first_divergence.has_value());
    EXPECT_EQ(outcome.first_divergence->event_index, 1U);
    EXPECT_EQ(outcome.first_divergence->layer, oracle::Layer::D);
    EXPECT_EQ(outcome.first_divergence->category, oracle::DivergenceCategory::Composition);
    EXPECT_EQ(outcome.processed_events, 2U);
}

TEST(FaultInjectionTest, DivergenceStopsAtFirstMismatch) {
    // Two mutations: one at event 0 (result) and one at event 2 (checkpoint). The
    // driver must stop at event 0 and never observe the later mismatch.
    const auto outcome = run_with_mutated_reference(
        "spot_tiny", mutator_for(0, [](OperationObservation& observation) {
            observation.result.value = DecimalErrorOutcome{CanonicalDecimalError::Overflow};
            observation.checkpoint.status = CanonicalStatus::Synchronized;
        }));
    ASSERT_TRUE(outcome.first_divergence.has_value());
    EXPECT_EQ(outcome.first_divergence->event_index, 0U);
    EXPECT_EQ(outcome.first_divergence->category, oracle::DivergenceCategory::OperationResult);
    EXPECT_EQ(outcome.observations.size(), 0U);
}

TEST(FaultInjectionTest, DeterministicSeededFaultProducesStableDivergence) {
    const auto first = run_with_mutated_reference(
        "usdm_tiny", mutator_for(4, [](OperationObservation& observation) {
            observation.checkpoint.asks.push_back({1, 1});
        }));
    const auto second = run_with_mutated_reference(
        "usdm_tiny", mutator_for(4, [](OperationObservation& observation) {
            observation.checkpoint.asks.push_back({1, 1});
        }));
    ASSERT_TRUE(first.first_divergence.has_value());
    EXPECT_EQ(first, second);
    EXPECT_EQ(first.first_divergence->event_index, 4U);
    EXPECT_EQ(first.first_divergence->layer, oracle::Layer::R2);
}

TEST(FaultInjectionTest, SnapshotResultKindMismatchIsR4OperationResultDivergence) {
    const auto outcome = run_with_mutated_reference(
        "spot_tiny", mutator_for(4, [](OperationObservation& observation) {
            observation.result.value = SnapshotOutcome{};
        }));
    ASSERT_TRUE(outcome.first_divergence.has_value());
    EXPECT_EQ(outcome.first_divergence->event_index, 4U);
    EXPECT_EQ(outcome.first_divergence->layer, oracle::Layer::R4);
    EXPECT_EQ(outcome.first_divergence->category, oracle::DivergenceCategory::OperationResult);
}

} // namespace

// NOLINTEND(bugprone-unchecked-optional-access)
