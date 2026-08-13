// Deterministic tests for the Phase-6 benchmark support components
// (OD-M5-P6-060/068): workload-spec identity, M1 semantic cases, M2 state
// invariance, the complete 48-cell M3 accepted registration set, M3 batch=0
// Applied behavior, classification dispositions, latency nearest-rank-v1
// thresholds, and replay checksum determinism.

#include "book_state.hpp"
#include "canonical_text.hpp"
#include "core_replay_executor.hpp"
#include "latency_stats.hpp"
#include "m2_cells.hpp"
#include "m3_cells.hpp"
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
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {

namespace core = binance_market_data::projection::v1;
namespace bm = bmd_projection::m5::benchmark;
namespace phase3 = bmd_projection::m5::phase3;

[[nodiscard]] core::DecimalScale scale_8() { return *core::DecimalScale::create(8); }

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
        const auto value = core::PriceUnits::create(123'456'789);
        ASSERT_TRUE(value.has_value());
        const auto result = core::format_price_fixed(*value, scale_8());
        ASSERT_TRUE(std::holds_alternative<std::string>(result));
        EXPECT_EQ(std::get<std::string>(result), "1.23456789");
    }
    {
        const auto value = core::QuantityUnits::create(0);
        ASSERT_TRUE(value.has_value());
        const auto result = core::format_quantity_fixed(*value, scale_8());
        ASSERT_TRUE(std::holds_alternative<std::string>(result));
        EXPECT_EQ(std::get<std::string>(result), "0.00000000");
    }
}

// ---------------------------------------------------------------------------
// Workload-spec identity (OD-M5-P6-023/036).
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

// ---------------------------------------------------------------------------
// M2 state invariance (OD-M5-P6-004/010/012).
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
        bm::M2ApplyUpdatesCell cell{depth, 10, bm::M2ApplyUpdatesMix::ReplacementHeavy};
        cell.prepare();
        for (std::size_t index = 0; index < 128; ++index) {
            cell.execute_step();
        }
        EXPECT_EQ(cell.book().level_count(core::BookSide::Bid), depth);
        EXPECT_EQ(cell.book().level_count(core::BookSide::Ask), depth);
    }
}

TEST(Phase6M2ApplyUpdates, EmptyBookInsertionEdgeUsesPool) {
    bm::M2ApplyUpdatesCell cell{0, 100, bm::M2ApplyUpdatesMix::Insertion};
    cell.prepare();
    ASSERT_TRUE(cell.uses_pool());
    for (std::size_t index = 0; index < cell.pool_size(); ++index) {
        cell.execute_step(index);
    }
}

TEST(Phase6M2ReplaceAll, PostStateIsCanonical) {
    bm::M2ReplaceAllCell cell{8};
    cell.prepare();
    cell.execute_step();
    EXPECT_EQ(cell.book().level_count(core::BookSide::Bid), 8U);
    EXPECT_EQ(cell.book().level_count(core::BookSide::Ask), 8U);
    EXPECT_EQ(cell.book().best_bid()->price.value(), 20'000);
}

// ---------------------------------------------------------------------------
// M3 cells (OD-M5-P6-006/007/008/017).
// ---------------------------------------------------------------------------
TEST(Phase6M3AcceptedCellNames, Full48CellMatrix) {
    const auto names = bm::expected_m3_accepted_cell_names();
    ASSERT_EQ(names.size(), 48U);
    std::vector<std::string> unique{names};
    std::sort(unique.begin(), unique.end());
    unique.erase(std::unique(unique.begin(), unique.end()), unique.end());
    EXPECT_EQ(unique.size(), 48U);
    for (const auto& name : names) {
        EXPECT_TRUE(name.rfind("M3/LiveApply/Accepted/", 0) == 0);
    }
    // Spot and USD-M each cover 6 depths x 4 batches.
    for (const auto policy : {"Spot", "UsdMPerpetual"}) {
        int count = 0;
        for (const auto& name : names) {
            if (name.find(std::string{policy} + "/") != std::string::npos) {
                ++count;
            }
        }
        EXPECT_EQ(count, 24);
    }
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
    for (const auto policy :
         {core::SequencePolicyKind::Spot, core::SequencePolicyKind::UsdMPerpetual}) {
        {
            bm::M3ClassificationCell cell{
                bm::M3ClassificationCell::Config{bm::M3ClassificationKind::Stale, policy, 8}};
            cell.prepare();
            for (std::size_t index = 0; index < 32; ++index) {
                const auto result = cell.execute_step();
                ASSERT_TRUE(result.apply_disposition.has_value());
                EXPECT_EQ(*result.apply_disposition, core::ApplyDisposition::IgnoredStale);
            }
        }
        {
            bm::M3ClassificationCell cell{
                bm::M3ClassificationCell::Config{bm::M3ClassificationKind::Duplicate, policy, 8}};
            cell.prepare();
            for (std::size_t index = 0; index < 32; ++index) {
                const auto result = cell.execute_step();
                ASSERT_TRUE(result.apply_disposition.has_value());
                EXPECT_EQ(*result.apply_disposition, core::ApplyDisposition::IgnoredDuplicate);
            }
        }
        {
            bm::M3ClassificationCell cell{
                bm::M3ClassificationCell::Config{bm::M3ClassificationKind::Gap, policy, 8}};
            cell.prepare();
            ASSERT_TRUE(cell.uses_pool());
            for (std::size_t index = 0; index < cell.pool_size(); ++index) {
                const auto result = cell.execute_step(index);
                ASSERT_TRUE(result.apply_disposition.has_value());
                EXPECT_EQ(*result.apply_disposition, core::ApplyDisposition::GapDetected);
            }
        }
        {
            bm::M3ClassificationCell cell{
                bm::M3ClassificationCell::Config{bm::M3ClassificationKind::Reset, policy, 8}};
            cell.prepare();
            for (std::size_t index = 0; index < cell.pool_size(); ++index) {
                const auto result = cell.execute_step(index);
                EXPECT_EQ(result.status_after, core::ProjectionStatus::AwaitingBaseline);
            }
        }
        {
            bm::M3ClassificationCell cell{bm::M3ClassificationCell::Config{
                bm::M3ClassificationKind::BaselineInstall, policy, 8}};
            cell.prepare();
            for (std::size_t index = 0; index < cell.pool_size(); ++index) {
                const auto result = cell.execute_step(index);
                ASSERT_TRUE(result.install_disposition.has_value());
                EXPECT_EQ(*result.install_disposition, core::InstallDisposition::Installed);
                EXPECT_EQ(result.status_after, core::ProjectionStatus::AwaitingBridge);
            }
        }
    }
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
// Latency statistics (OD-M5-P6-018/032).
// ---------------------------------------------------------------------------
TEST(Phase6LatencyStats, NearestRankV1) {
    bm::LatencyReport report =
        bm::make_latency_report({10, 20, 30, 40, 50, 60, 70, 80, 90, 100}, 10, 1);
    EXPECT_EQ(report.quantile(0.5), 50);
    EXPECT_EQ(report.quantile(0.9), 90);
    EXPECT_EQ(report.quantile(1.0), 100);
    EXPECT_FALSE(report.quantile(0.0).has_value());
    EXPECT_FALSE(report.quantile(1.1).has_value());
}

TEST(Phase6LatencyStats, EligibilityThresholds) {
    {
        bm::LatencyReport small =
            bm::make_latency_report(std::vector<std::uint64_t>(999, 1), 999, 1);
        EXPECT_FALSE(small.p50_eligible());
        EXPECT_FALSE(small.p99_eligible());
    }
    {
        bm::LatencyReport medium =
            bm::make_latency_report(std::vector<std::uint64_t>(1'000, 1), 1'000, 1);
        EXPECT_TRUE(medium.p50_eligible());
        EXPECT_TRUE(medium.p90_eligible());
        EXPECT_FALSE(medium.p99_eligible());
        EXPECT_FALSE(medium.p999_eligible());
    }
    {
        bm::LatencyReport p99 =
            bm::make_latency_report(std::vector<std::uint64_t>(10'000, 1), 10'000, 1);
        EXPECT_TRUE(p99.p99_eligible());
        EXPECT_FALSE(p99.p999_eligible());
    }
    {
        bm::LatencyReport repeated_small =
            bm::make_latency_report(std::vector<std::uint64_t>(10'240, 1), 2'048, 5);
        EXPECT_TRUE(repeated_small.p99_eligible());
        EXPECT_FALSE(repeated_small.p999_eligible());
        EXPECT_NE(repeated_small.p999_omission_reason().find("unique_event_count"),
                  std::string::npos);
    }
    {
        bm::LatencyReport large =
            bm::make_latency_report(std::vector<std::uint64_t>(100'000, 1), 100'000, 1);
        EXPECT_TRUE(large.p999_eligible());
    }
}

// ---------------------------------------------------------------------------
// Replay checksum determinism (OD-M5-P6-026/045).
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

} // namespace
