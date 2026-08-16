// Deterministic tests for the Phase-6 benchmark support components:
// workload-spec identity, M1 semantic cases, M2 state
// invariance, the complete 48-cell M3 accepted registration set, M3 batch=0
// Applied behavior, classification dispositions, latency nearest-rank-v1
// thresholds, and replay checksum determinism.

#include "book_state.hpp"
#include "canonical_text.hpp"
#include "core_replay_executor.hpp"
#include "latency_stats.hpp"
#include "m2_cells.hpp"
#include "m2_workload_specs.hpp"
#include "m3_cells.hpp"
#include "m3_workload_specs.hpp"
#include "replay_workload_specs.hpp"
#if defined(BMD_PROJECTION_PHASE6_ADAPTER_ENABLED)
#include "m4_workload_specs.hpp"
#endif
#include "replay_checksum.hpp"
#include "small_workload.hpp"
#include "workload_spec.hpp"

#include <binance_market_data/projection/v1/numeric/decimal_format.hpp>
#include <binance_market_data/projection/v1/numeric/decimal_parse.hpp>
#include <binance_market_data/projection/v1/projection_state/book_projection.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {

namespace core = binance_market_data::projection::v1;
namespace bm = bmd_projection::m5::benchmark;
namespace phase3 = bmd_projection::m5::phase3;

template <typename T> [[nodiscard]] T required_value(std::optional<T> value) {
    if (!value.has_value()) {
        std::abort();
    }
    return *value;
}

[[nodiscard]] core::DecimalScale scale_8() { return required_value(core::DecimalScale::create(8)); }

struct ApplyLevelPreparedState final {
    std::string workload_hash;
    std::size_t update_slot_count;
    std::vector<core::BookLevel> bids;
    std::vector<core::BookLevel> asks;

    bool operator==(const ApplyLevelPreparedState&) const = default;
};

[[nodiscard]] ApplyLevelPreparedState capture_apply_level_state(const bm::M2ApplyLevelCell& cell) {
    return {cell.generated_workload_sha256(), cell.prepared_update_slot_count(),
            cell.book().all_levels(core::BookSide::Bid),
            cell.book().all_levels(core::BookSide::Ask)};
}

[[nodiscard]] bool all_update_steps_are_updated(bm::M2ApplyLevelCell& cell, std::size_t count) {
    for (std::size_t index = 0; index < count; ++index) {
        if (cell.execute_step() != core::LevelChange::Updated) {
            return false;
        }
    }
    return true;
}

struct ApplyUpdatesPreparedState final {
    std::string workload_hash;
    std::size_t prepared_batch_count;
    std::size_t pool_size;
    bool pool_is_empty;

    bool operator==(const ApplyUpdatesPreparedState&) const = default;
};

[[nodiscard]] ApplyUpdatesPreparedState
capture_apply_updates_state(const bm::M2ApplyUpdatesCell& cell) {
    bool pool_is_empty = true;
    for (std::size_t index = 0; index < cell.pool_size(); ++index) {
        pool_is_empty = pool_is_empty &&
                        cell.pooled_book(index).level_count(core::BookSide::Bid) == 0 &&
                        cell.pooled_book(index).level_count(core::BookSide::Ask) == 0;
    }
    return {cell.generated_workload_sha256(), cell.prepared_batch_count(), cell.pool_size(),
            pool_is_empty};
}

[[nodiscard]] bool all_insertion_steps_have_expected_state(bm::M2ApplyUpdatesCell& cell,
                                                           std::size_t expected_levels) {
    for (std::size_t index = 0; index < cell.pool_size(); ++index) {
        cell.execute_step(index);
        if (cell.pooled_book(index).level_count(core::BookSide::Bid) != expected_levels ||
            cell.pooled_book(index).level_count(core::BookSide::Ask) != 0) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool all_names_have_accepted_prefix(const std::vector<std::string>& names) {
    return std::all_of(names.begin(), names.end(), [](const std::string& name) {
        return name.starts_with("M3/LiveApply/Accepted/");
    });
}

[[nodiscard]] std::size_t count_policy_names(const std::vector<std::string>& names,
                                             std::string_view policy) {
    return static_cast<std::size_t>(
        std::count_if(names.begin(), names.end(), [policy](const std::string& name) {
            return name.find(std::string{policy} + "/") != std::string::npos;
        }));
}

[[nodiscard]] bool classification_apply_matches(core::SequencePolicyKind policy,
                                                bm::M3ClassificationKind kind,
                                                core::ApplyDisposition expected) {
    bm::M3ClassificationCell cell{bm::M3ClassificationCell::Config{kind, policy, 8}};
    cell.prepare();
    const auto step_count = cell.uses_pool() ? cell.pool_size() : 32U;
    for (std::size_t index = 0; index < step_count; ++index) {
        if (cell.execute_step(index).apply_disposition != expected) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool classification_reset_matches(core::SequencePolicyKind policy) {
    bm::M3ClassificationCell cell{bm::M3ClassificationCell::Config{
        bm::M3ClassificationKind::Reset, policy, bm::kM3ClassificationDepth}};
    cell.prepare();
    for (std::size_t index = 0; index < cell.pool_size(); ++index) {
        if (cell.execute_step(index).status_after != core::ProjectionStatus::AwaitingBaseline) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool classification_baseline_install_matches(core::SequencePolicyKind policy) {
    bm::M3ClassificationCell cell{bm::M3ClassificationCell::Config{
        bm::M3ClassificationKind::BaselineInstall, policy, bm::kM3ClassificationDepth}};
    cell.prepare();
    for (std::size_t index = 0; index < cell.pool_size(); ++index) {
        const auto result = cell.execute_step(index);
        if (result.install_disposition != core::InstallDisposition::Installed ||
            result.status_after != core::ProjectionStatus::AwaitingBridge) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool classification_table_matches(core::SequencePolicyKind policy) {
    return classification_apply_matches(policy, bm::M3ClassificationKind::Stale,
                                        core::ApplyDisposition::IgnoredStale) &&
           classification_apply_matches(policy, bm::M3ClassificationKind::Duplicate,
                                        core::ApplyDisposition::IgnoredDuplicate) &&
           classification_apply_matches(policy, bm::M3ClassificationKind::Gap,
                                        core::ApplyDisposition::GapDetected) &&
           classification_reset_matches(policy) && classification_baseline_install_matches(policy);
}

// ---------------------------------------------------------------------------
// M1 normative semantic table (OD-M5-P6-003).
// ---------------------------------------------------------------------------
TEST(Phase6M1Cases, NormativeSemanticTable) {
    {
        const auto result = core::parse_price("1.23456789", scale_8());
        ASSERT_TRUE(std::holds_alternative<core::ParsedDecimal<core::PriceUnits>>(result));
        EXPECT_EQ(std::get<core::ParsedDecimal<core::PriceUnits>>(result).value.value(),
                  123'456'789);
    }
    {
        const auto result = core::parse_positive_quantity("1.23456789", scale_8());
        EXPECT_TRUE(std::holds_alternative<core::ParsedDecimal<core::QuantityUnits>>(result));
    }
    {
        const auto result = core::parse_quantity("0", scale_8());
        ASSERT_TRUE(std::holds_alternative<core::ParsedDecimal<core::QuantityUnits>>(result));
        EXPECT_EQ(std::get<core::ParsedDecimal<core::QuantityUnits>>(result).value.value(), 0);
    }
    {
        const auto result = core::parse_positive_quantity("0", scale_8());
        ASSERT_TRUE(std::holds_alternative<core::DecimalError>(result));
        EXPECT_EQ(std::get<core::DecimalError>(result).code,
                  core::DecimalErrorCode::ZeroNotAllowed);
    }
    {
        const auto result = core::parse_price("1.2345", scale_8());
        ASSERT_TRUE(std::holds_alternative<core::ParsedDecimal<core::PriceUnits>>(result));
        EXPECT_EQ(std::get<core::ParsedDecimal<core::PriceUnits>>(result).value.value(),
                  123'450'000);
    }
    {
        const auto result = core::parse_price("1.234567890000000000", scale_8());
        ASSERT_TRUE(std::holds_alternative<core::ParsedDecimal<core::PriceUnits>>(result));
        EXPECT_EQ(std::get<core::ParsedDecimal<core::PriceUnits>>(result).value.value(),
                  123'456'789);
    }
    {
        const auto result = core::parse_price("1.234567890123456789", scale_8());
        ASSERT_TRUE(std::holds_alternative<core::DecimalError>(result));
        EXPECT_EQ(std::get<core::DecimalError>(result).code, core::DecimalErrorCode::InexactScale);
    }
    {
        const auto result = core::parse_price("92233720368.54775808", scale_8());
        ASSERT_TRUE(std::holds_alternative<core::DecimalError>(result));
        EXPECT_EQ(std::get<core::DecimalError>(result).code, core::DecimalErrorCode::Overflow);
    }
    {
        const auto result = core::parse_price("1e3", scale_8());
        ASSERT_TRUE(std::holds_alternative<core::DecimalError>(result));
        EXPECT_EQ(std::get<core::DecimalError>(result).code, core::DecimalErrorCode::InvalidSyntax);
    }
    {
        const auto value = required_value(core::PriceUnits::create(123'456'789));
        const auto result = core::format_price_fixed(value, scale_8());
        ASSERT_TRUE(std::holds_alternative<std::string>(result));
        EXPECT_EQ(std::get<std::string>(result), "1.23456789");
    }
    {
        const auto value = required_value(core::QuantityUnits::create(0));
        const auto result = core::format_quantity_fixed(value, scale_8());
        ASSERT_TRUE(std::holds_alternative<std::string>(result));
        EXPECT_EQ(std::get<std::string>(result), "0.00000000");
    }
}

// ---------------------------------------------------------------------------
// Workload-spec identity (OD-M5-P6-023).
// ---------------------------------------------------------------------------
TEST(Phase6WorkloadSpec, DistinctParametersProduceDistinctHashes) {
    bm::clear_registered_workloads_for_testing();
    auto& first = bm::register_workload("Cell/A");
    first.set("generator_schema", "T");
    first.set("seed", "1");
    first.set("depth_per_side", "8");
    auto& second = bm::register_workload("Cell/B");
    second.set("generator_schema", "T");
    second.set("seed", "1");
    second.set("depth_per_side", "100");
    const auto& specs = bm::registered_workloads();
    ASSERT_EQ(specs.size(), 2U);
    const auto hash_of = [](const std::string& text) {
        const auto hash = bmd_projection::m5::replay::sha256_hex(text);
        return std::get<std::string>(hash);
    };
    EXPECT_NE(hash_of(specs[0].second), hash_of(specs[1].second));
}

TEST(Phase6WorkloadSpec, CanonicalTextIsKeySortedAndStable) {
    bm::clear_registered_workloads_for_testing();
    auto& builder = bm::register_workload("Cell/A");
    builder.set("zeta", "1");
    builder.set("alpha", "2");
    const auto text = builder.canonical_text();
    EXPECT_EQ(text, "alpha=2\nzeta=1\n");
    const auto first_hash = builder.canonical_sha256();
    const auto second_hash = builder.canonical_sha256();
    EXPECT_EQ(first_hash, second_hash);
    EXPECT_EQ(first_hash.size(), 64U);
}

TEST(Phase6WorkloadSpec, SeedAloneIsInsufficient) {
    bm::clear_registered_workloads_for_testing();
    auto& same_seed = bm::register_workload("A");
    same_seed.set("seed", "42");
    same_seed.set("depth_per_side", "8");
    auto& other_params = bm::register_workload("B");
    other_params.set("seed", "42");
    other_params.set("depth_per_side", "1000");
    EXPECT_NE(same_seed.canonical_sha256(), other_params.canonical_sha256());
}

TEST(Phase6WorkloadSpec, GeneratedHashIsStableAndParameterSensitive) {
    const auto first =
        bm::m2_apply_updates_generated_sha256({8, 10, bm::M2ApplyUpdatesMix::ReplacementHeavy});
    EXPECT_EQ(first.size(), 64U);
    EXPECT_EQ(first, bm::m2_apply_updates_generated_sha256(
                         {8, 10, bm::M2ApplyUpdatesMix::ReplacementHeavy}));
    EXPECT_NE(first, bm::m2_apply_updates_generated_sha256(
                         {100, 10, bm::M2ApplyUpdatesMix::ReplacementHeavy}));
    EXPECT_NE(first, bm::m2_apply_updates_generated_sha256(
                         {8, 100, bm::M2ApplyUpdatesMix::ReplacementHeavy}));
    EXPECT_NE(first,
              bm::m2_apply_updates_generated_sha256({8, 10, bm::M2ApplyUpdatesMix::Insertion}));
    EXPECT_NE(bm::m3_accepted_generated_sha256({core::SequencePolicyKind::Spot, 8, 10}),
              bm::m3_accepted_generated_sha256({core::SequencePolicyKind::UsdMPerpetual, 8, 10}));
    EXPECT_NE(bm::m3_proxy_generated_sha256("apply_updates", 8),
              bm::m3_proxy_generated_sha256("replace_all", 8));
}

// ---------------------------------------------------------------------------
// M2 state invariance (OD-M5-P6-004/005).
// ---------------------------------------------------------------------------
TEST(Phase6M2ApplyLevel, InsertPoolAlwaysInsertsFreshState) {
    bm::M2ApplyLevelCell cell{bm::M2ApplyLevelKind::Insert, 8};
    cell.prepare();
    ASSERT_TRUE(cell.uses_pool());
    for (std::size_t index = 0; index < cell.pool_size(); ++index) {
        EXPECT_EQ(cell.execute_step(index), core::LevelChange::Inserted);
    }
}

TEST(Phase6M2ApplyLevel, UpdateAlternatesAndAlwaysUpdates) {
    bm::M2ApplyLevelCell cell{bm::M2ApplyLevelKind::Update, 8};
    cell.prepare();
    EXPECT_FALSE(cell.uses_pool());
    for (std::size_t index = 0; index < 256; ++index) {
        EXPECT_EQ(cell.execute_step(), core::LevelChange::Updated);
    }
}

TEST(Phase6M2ApplyLevel, PrepareTwiceRebuildsUpdateState) {
    bm::M2ApplyLevelCell cell{bm::M2ApplyLevelKind::Update, 8};
    cell.prepare();
    const auto first_state = capture_apply_level_state(cell);

    cell.prepare();

    EXPECT_EQ(capture_apply_level_state(cell), first_state);
    EXPECT_TRUE(all_update_steps_are_updated(cell, 256));
}

TEST(Phase6Warmup, MutatedWarmupStateIsDiscardedBeforeFreshMeasurementState) {
    bm::M2ApplyLevelCell warmup{bm::M2ApplyLevelKind::Update, 8};
    warmup.prepare();
    const auto canonical_precondition = capture_apply_level_state(warmup);
    ASSERT_EQ(warmup.execute_step(), core::LevelChange::Updated);
    EXPECT_NE(capture_apply_level_state(warmup), canonical_precondition);

    bm::M2ApplyLevelCell measured{bm::M2ApplyLevelKind::Update, 8};
    measured.prepare();
    EXPECT_EQ(capture_apply_level_state(measured), canonical_precondition);
    EXPECT_EQ(measured.execute_step(), core::LevelChange::Updated);
}

TEST(Phase6M2ApplyLevel, DeletePoolAlwaysRemovesFreshState) {
    bm::M2ApplyLevelCell cell{bm::M2ApplyLevelKind::Delete, 8};
    cell.prepare();
    ASSERT_TRUE(cell.uses_pool());
    for (std::size_t index = 0; index < cell.pool_size(); ++index) {
        EXPECT_EQ(cell.execute_step(index), core::LevelChange::Removed);
    }
}

TEST(Phase6M2ApplyLevel, MissingDeleteAlwaysUnchanged) {
    bm::M2ApplyLevelCell cell{bm::M2ApplyLevelKind::MissingDelete, 8};
    cell.prepare();
    EXPECT_FALSE(cell.uses_pool());
    for (std::size_t index = 0; index < 256; ++index) {
        EXPECT_EQ(cell.execute_step(), core::LevelChange::Unchanged);
    }
}

TEST(Phase6M2ApplyUpdates, ReplacementHeavyKeepsDepthFixed) {
    for (const auto depth : {8U, 100U, 1000U}) {
        bm::M2ApplyUpdatesCell cell{
            bm::M2ApplyUpdatesCell::Config{depth, 10, bm::M2ApplyUpdatesMix::ReplacementHeavy}};
        cell.prepare();
        for (std::size_t index = 0; index < 128; ++index) {
            cell.execute_step();
        }
        EXPECT_EQ(cell.book().level_count(core::BookSide::Bid), depth);
        EXPECT_EQ(cell.book().level_count(core::BookSide::Ask), depth);
    }
}

TEST(Phase6M2ApplyUpdates, EmptyBookInsertionEdgeUsesPool) {
    bm::M2ApplyUpdatesCell cell{
        bm::M2ApplyUpdatesCell::Config{0, 100, bm::M2ApplyUpdatesMix::Insertion}};
    cell.prepare();
    ASSERT_TRUE(cell.uses_pool());
    for (std::size_t index = 0; index < cell.pool_size(); ++index) {
        cell.execute_step(index);
    }
}

TEST(Phase6M2ApplyUpdates, PrepareTwiceRebuildsInsertionPool) {
    constexpr std::size_t kBatch = 10;
    bm::M2ApplyUpdatesCell cell{
        bm::M2ApplyUpdatesCell::Config{0, kBatch, bm::M2ApplyUpdatesMix::Insertion}};
    cell.prepare();
    const auto first_state = capture_apply_updates_state(cell);

    cell.prepare();

    EXPECT_EQ(capture_apply_updates_state(cell), first_state);
    EXPECT_TRUE(first_state.pool_is_empty);
    EXPECT_EQ(first_state.prepared_batch_count, 1U);
    EXPECT_TRUE(all_insertion_steps_have_expected_state(cell, kBatch));
}

TEST(Phase6M2ReplaceAll, PostStateIsCanonical) {
    bm::M2ReplaceAllCell cell{8};
    cell.prepare();
    cell.execute_step();
    EXPECT_EQ(cell.book().level_count(core::BookSide::Bid), 8U);
    EXPECT_EQ(cell.book().level_count(core::BookSide::Ask), 8U);
    EXPECT_EQ(required_value(cell.book().best_bid()).price.value(), 20'000);
}

// ---------------------------------------------------------------------------
// M3 cells (OD-M5-P6-006/007/008/009).
// ---------------------------------------------------------------------------
TEST(Phase6M3AcceptedCellNames, Full48CellMatrix) {
    const auto names = bm::expected_m3_accepted_cell_names();
    ASSERT_EQ(names.size(), 48U);
    std::vector<std::string> unique{names};
    std::sort(unique.begin(), unique.end());
    unique.erase(std::unique(unique.begin(), unique.end()), unique.end());
    EXPECT_EQ(unique.size(), 48U);
    EXPECT_TRUE(all_names_have_accepted_prefix(names));
    // Spot and USD-M each cover 6 depths x 4 batches.
    EXPECT_EQ(count_policy_names(names, "Spot"), 24U);
    EXPECT_EQ(count_policy_names(names, "UsdMPerpetual"), 24U);
}

TEST(Phase6M3AcceptedCell, BatchZeroReturnsAppliedAndStaysSynchronized) {
    for (const auto policy :
         {core::SequencePolicyKind::Spot, core::SequencePolicyKind::UsdMPerpetual}) {
        bm::M3AcceptedCell cell{bm::M3AcceptedCell::Config{policy, 8, 0}};
        cell.prepare();
        EXPECT_FALSE(cell.uses_pool());
        for (std::size_t index = 0; index < 64; ++index) {
            const auto result = cell.execute_step();
            EXPECT_EQ(result.disposition, core::ApplyDisposition::Applied);
            EXPECT_EQ(result.status_after, core::ProjectionStatus::Synchronized);
        }
    }
}

TEST(Phase6M3AcceptedCell, DepthPositiveBatchesReturnAppliedWithFixedDepth) {
    for (const auto policy :
         {core::SequencePolicyKind::Spot, core::SequencePolicyKind::UsdMPerpetual}) {
        bm::M3AcceptedCell cell{bm::M3AcceptedCell::Config{policy, 8, 10}};
        cell.prepare();
        EXPECT_FALSE(cell.uses_pool());
        for (std::size_t index = 0; index < 64; ++index) {
            const auto result = cell.execute_step();
            EXPECT_EQ(result.disposition, core::ApplyDisposition::Applied);
        }
        EXPECT_EQ(cell.projection().diagnostic_book().level_count(core::BookSide::Bid), 8U);
    }
}

TEST(Phase6M3AcceptedCell, EmptyBookInsertionEdgeUsesPreparedPool) {
    bm::M3AcceptedCell cell{bm::M3AcceptedCell::Config{core::SequencePolicyKind::Spot, 0, 10}};
    cell.prepare();
    ASSERT_TRUE(cell.uses_pool());
    for (std::size_t index = 0; index < cell.pool_size(); ++index) {
        const auto result = cell.execute_step(index);
        EXPECT_EQ(result.disposition, core::ApplyDisposition::Applied);
    }
}

TEST(Phase6M3Classification, DispositionsMatchNormativeTable) {
    EXPECT_TRUE(classification_table_matches(core::SequencePolicyKind::Spot));
    EXPECT_TRUE(classification_table_matches(core::SequencePolicyKind::UsdMPerpetual));
}

TEST(Phase6M3Proxy, MoveCommitIncludesPopulatedDestination) {
    bm::M3ProxyCells cells{8};
    cells.prepare();
    auto destination = bm::build_order_book(8);
    auto source = bm::build_order_book(8);
    const auto old_bid_count = destination.level_count(core::BookSide::Bid);
    bm::M3ProxyCells::move_commit(destination, std::move(source));
    EXPECT_GT(old_bid_count, 0U);
    EXPECT_EQ(destination.level_count(core::BookSide::Bid), 8U);
}

// ---------------------------------------------------------------------------
// Latency statistics (OD-M5-P6-017/018).
// ---------------------------------------------------------------------------
TEST(Phase6LatencyStats, NearestRankV1) {
    bm::LatencyReport report = bm::make_latency_report({10, 20, 30, 40, 50, 60, 70, 80, 90, 100},
                                                       bm::LatencyBookkeeping{10, 1});
    EXPECT_EQ(report.quantile(0.5), 50);
    EXPECT_EQ(report.quantile(0.9), 90);
    EXPECT_EQ(report.quantile(1.0), 100);
    EXPECT_FALSE(report.quantile(0.0).has_value());
    EXPECT_FALSE(report.quantile(1.1).has_value());
}

TEST(Phase6LatencyStats, EligibilityThresholds) {
    {
        bm::LatencyReport small = bm::make_latency_report(std::vector<std::uint64_t>(999, 1),
                                                          bm::LatencyBookkeeping{999, 1});
        EXPECT_FALSE(small.p50_eligible());
        EXPECT_FALSE(small.p99_eligible());
    }
    {
        bm::LatencyReport medium = bm::make_latency_report(std::vector<std::uint64_t>(1'000, 1),
                                                           bm::LatencyBookkeeping{1'000, 1});
        EXPECT_TRUE(medium.p50_eligible());
        EXPECT_TRUE(medium.p90_eligible());
        EXPECT_FALSE(medium.p99_eligible());
        EXPECT_FALSE(medium.p999_eligible());
    }
    {
        bm::LatencyReport p99 = bm::make_latency_report(std::vector<std::uint64_t>(10'000, 1),
                                                        bm::LatencyBookkeeping{10'000, 1});
        EXPECT_TRUE(p99.p99_eligible());
        EXPECT_FALSE(p99.p999_eligible());
    }
    {
        bm::LatencyReport repeated_small = bm::make_latency_report(
            std::vector<std::uint64_t>(10'240, 1), bm::LatencyBookkeeping{2'048, 5});
        EXPECT_TRUE(repeated_small.p99_eligible());
        EXPECT_FALSE(repeated_small.p999_eligible());
        EXPECT_NE(repeated_small.p999_omission_reason().find("unique_event_count"),
                  std::string::npos);
    }
    {
        bm::LatencyReport large = bm::make_latency_report(std::vector<std::uint64_t>(100'000, 1),
                                                          bm::LatencyBookkeeping{100'000, 1});
        EXPECT_TRUE(large.p999_eligible());
    }
}

// ---------------------------------------------------------------------------
// Replay checksum determinism (OD-M5-P6-021/024).
// ---------------------------------------------------------------------------
TEST(Phase6ReplayChecksum, FnvIsDeterministicAndSensitive) {
    const auto first = bm::replay_checksum_append(bm::kReplayChecksumSeed, 42);
    const auto second = bm::replay_checksum_append(bm::kReplayChecksumSeed, 42);
    const auto other = bm::replay_checksum_append(bm::kReplayChecksumSeed, 43);
    EXPECT_EQ(first, second);
    EXPECT_NE(first, other);
}

TEST(Phase6CoreReplay, ChecksumsAreStableAndWorkloadIdentityStable) {
    const auto fixture = phase3::make_spot_small_workload();
    bm::CoreReplayExecutor executor{fixture};
    EXPECT_EQ(executor.event_count(), 2'048U);
    std::uint64_t first = 0;
    for (int run = 0; run < 3; ++run) {
        core::BookProjection projection{executor.numeric_spec(), executor.policy()};
        const auto checksum = executor.run(projection);
        if (run == 0) {
            first = checksum;
        }
        EXPECT_EQ(checksum, first);
        EXPECT_EQ(projection.status(), core::ProjectionStatus::Synchronized);
    }
    const auto usdm_fixture = phase3::make_usdm_small_workload();
    bm::CoreReplayExecutor usdm{usdm_fixture};
    core::BookProjection projection{usdm.numeric_spec(), usdm.policy()};
    EXPECT_NE(usdm.run(projection), first);
}

TEST(Phase6CoreReplay, LatencyPreparedPathMatchesParsingPath) {
    const auto fixture = phase3::make_spot_small_workload();
    bm::CoreReplayExecutor executor{fixture};
    ASSERT_TRUE(executor.prepared_inputs_valid());
    core::BookProjection parsed_projection{executor.numeric_spec(), executor.policy()};
    const auto parsed_checksum = executor.run(parsed_projection);

    core::BookProjection prepared_projection{executor.numeric_spec(), executor.policy()};
    std::uint64_t prepared_checksum = bm::kReplayChecksumSeed;
    for (std::size_t index = 0; index < executor.event_count(); ++index) {
        prepared_checksum = bm::fold_evidence(
            prepared_checksum, executor.execute_prepared_event(prepared_projection, index));
    }
    prepared_checksum =
        bm::CoreReplayExecutor::finalize_checksum(prepared_projection, prepared_checksum);
    EXPECT_EQ(prepared_checksum, parsed_checksum);
}

TEST(Phase6CoreReplay, LatencyPreparationRejectsInvalidDecimalInput) {
    auto fixture = phase3::make_spot_small_workload();
    const auto found = std::find_if(
        fixture.replay.operations.begin(), fixture.replay.operations.end(),
        [](const auto& operation) {
            return std::holds_alternative<bmd_projection::m5::replay::InstallBaselineOp>(operation);
        });
    ASSERT_NE(found, fixture.replay.operations.end());
    auto* install = std::get_if<bmd_projection::m5::replay::InstallBaselineOp>(&*found);
    ASSERT_NE(install, nullptr);
    ASSERT_FALSE(install->bids.empty());
    install->bids.front().price = "not-a-decimal";
    bm::CoreReplayExecutor executor{fixture};
    EXPECT_FALSE(executor.prepared_inputs_valid());
}

// ---------------------------------------------------------------------------
// Phase-7 shared workload identity (OD-M5-P7-008/009/010/012/013): Phase-6
// timing and Phase-7 allocation measurement share ONE identity source. The
// Phase-7 selection deliberately excludes the Phase-6 update_mix scaling
// family; the 48-cell M3 matrix is complete and B=0 remains a cell.
// ---------------------------------------------------------------------------
TEST(Phase7WorkloadIdentity, Phase7M2SelectionExcludesUpdateMixScalingFamily) {
    bm::clear_registered_workloads_for_testing();
    bm::register_m2_workload_specs(false);
    const auto& specs = bm::registered_workloads();
    std::vector<std::string> names;
    for (const auto& [name, text] : specs) {
        names.push_back(name);
        EXPECT_FALSE(name.starts_with("M2/apply_updates/update_mix/"));
        EXPECT_EQ(text.find("primary_scaling_workload=true"), std::string::npos);
        static_cast<void>(text);
    }
    EXPECT_EQ(names.size(), 54U);
    for (const auto& expected :
         {"M2/apply_level/insert/8", "M2/apply_updates/100/1000", "M2/replace_all/10000",
          "M2/all_levels/0", "M2/top_levels/50/8", "M2/best_bid/8"}) {
        EXPECT_NE(std::find(names.begin(), names.end(), expected), names.end());
    }
}

TEST(Phase7WorkloadIdentity, Phase6FullM2RegistrationKeepsUpdateMix) {
    bm::clear_registered_workloads_for_testing();
    bm::register_m2_workload_specs(true);
    const auto& specs = bm::registered_workloads();
    EXPECT_EQ(specs.size(), 60U);
    EXPECT_NE(std::find_if(specs.begin(), specs.end(),
                           [](const auto& entry) {
                               return entry.first == "M2/apply_updates/update_mix/10000";
                           }),
              specs.end());
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST(Phase7WorkloadIdentity, M3RegistrationCoversTheComplete48CellMatrix) {
    bm::clear_registered_workloads_for_testing();
    bm::register_m3_workload_specs();
    const auto& specs = bm::registered_workloads();
    const auto expected = bm::expected_m3_accepted_cell_names();
    std::size_t accepted = 0;
    std::size_t classifications = 0;
    std::size_t proxies = 0;
    for (const auto& [name, text] : specs) {
        if (name.starts_with("M3/LiveApply/Accepted/")) {
            ++accepted;
            EXPECT_NE(std::find(expected.begin(), expected.end(), name), expected.end());
        } else if (name.starts_with("M3/Classification/")) {
            ++classifications;
        } else if (name.starts_with("M3/Component/") || name.starts_with("M3/Proxy/")) {
            ++proxies;
            EXPECT_NE(text.find("proxy_component_measurement=true"), std::string::npos);
        }
    }
    EXPECT_EQ(accepted, 48U);
    EXPECT_EQ(classifications, 10U);
    EXPECT_EQ(proxies, 12U);
    EXPECT_EQ(specs.size(), 70U);
    // B=0 remains a first-class cell in the matrix.
    EXPECT_NE(std::find(expected.begin(), expected.end(),
                        std::string{"M3/LiveApply/Accepted/Spot/D10000/B0"}),
              expected.end());
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST(Phase7WorkloadIdentity, ReplayRegistrationBindsCanonicalLogIdentity) {
    bm::clear_registered_workloads_for_testing();
    bm::register_replay_workload_specs();
    const auto& specs = bm::registered_workloads();
#if defined(BMD_PROJECTION_PHASE6_ADAPTER_ENABLED)
    ASSERT_EQ(specs.size(), 4U);
#else
    ASSERT_EQ(specs.size(), 2U);
#endif
    for (const auto& [name, text] : specs) {
        EXPECT_NE(text.find("canonical_log_sha256="), std::string::npos);
        EXPECT_NE(text.find("event_count=2048"), std::string::npos);
        EXPECT_NE(text.find("generator_schema=M5_PHASE6_REPLAY_V1"), std::string::npos);
        EXPECT_NE(text.find("seed=548746690337"), std::string::npos);
    }
    EXPECT_EQ(specs[0].first, "CoreNormalizedReplay/Spot");
    EXPECT_EQ(specs[1].first, "CoreNormalizedReplay/UsdMPerpetual");
#if defined(BMD_PROJECTION_PHASE6_ADAPTER_ENABLED)
    EXPECT_EQ(specs[2].first, "AdapterWireReplay/Spot");
    EXPECT_EQ(specs[3].first, "AdapterWireReplay/UsdMPerpetual");
#endif
}

// ---------------------------------------------------------------------------
// Bit-for-bit golden identity regression: the shared registration must produce
// exactly the accepted Phase-6 canonical specs (name + spec SHA-256). The
// golden files are committed Phase-6 identity evidence; they are never
// "updated to make a test pass" (OD-M5-P7-008/009/010/012/013).
// ---------------------------------------------------------------------------
namespace {

[[nodiscard]] std::vector<std::pair<std::string, std::string>>
load_golden_identity_file(const std::string& path) {
    std::ifstream stream{path};
    if (!stream.is_open()) {
        return {};
    }
    std::vector<std::pair<std::string, std::string>> entries;
    std::string line;
    while (std::getline(stream, line)) {
        const auto separator = line.find(' ');
        if (separator == std::string::npos) {
            continue;
        }
        entries.emplace_back(line.substr(0, separator), line.substr(separator + 1));
    }
    return entries;
}

} // namespace

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST(Phase7WorkloadIdentity, GoldenIdentityRegressionBitForBit) {
#ifndef BMD_PROJECTION_TEST_SOURCE_DIR
    GTEST_SKIP() << "golden identity directory not configured";
#else
    const std::string source_dir = BMD_PROJECTION_TEST_SOURCE_DIR;
    bm::clear_registered_workloads_for_testing();
    bm::register_m2_workload_specs(true);
    bm::register_m3_workload_specs();
    bm::register_replay_workload_specs();
    std::vector<std::pair<std::string, std::string>> expected =
        load_golden_identity_file(source_dir + "/m5/benchmark/"
                                               "phase6_workload_identity_golden_core.txt");
    ASSERT_FALSE(expected.empty());
#if defined(BMD_PROJECTION_PHASE6_ADAPTER_ENABLED)
    bm::register_m4_workload_specs();
    const auto adapter_expected = load_golden_identity_file(
        source_dir + "/m5/benchmark/phase6_workload_identity_golden_adapter.txt");
    ASSERT_FALSE(adapter_expected.empty());
    expected.insert(expected.end(), adapter_expected.begin(), adapter_expected.end());
#endif

    const auto& specs = bm::registered_workloads();
    ASSERT_EQ(specs.size(), expected.size());
    for (std::size_t index = 0; index < specs.size(); ++index) {
        const auto& [name, canonical_text] = specs.at(index);
        const auto hash = bmd_projection::m5::replay::sha256_hex(canonical_text);
        ASSERT_TRUE(std::holds_alternative<std::string>(hash));
        EXPECT_EQ(name, expected.at(index).first);
        EXPECT_EQ(std::get<std::string>(hash), expected.at(index).second)
            << "workload identity drift: " << name
            << " canonical spec must match the accepted Phase-6 golden identity";
    }
#endif
}

} // namespace
